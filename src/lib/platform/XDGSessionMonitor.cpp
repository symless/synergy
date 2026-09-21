/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/XDGSessionMonitor.h"

#include "base/Log.h"

#include <algorithm>

namespace deskflow {

namespace {

const auto kLogindName = "org.freedesktop.login1";
const auto kLogindPath = "/org/freedesktop/login1";
const auto kManagerInterface = "org.freedesktop.login1.Manager";
const auto kSessionInterface = "org.freedesktop.login1.Session";
const auto kPropertiesInterface = "org.freedesktop.DBus.Properties";
const auto kAutoSessionPath = "/org/freedesktop/login1/session/auto";
const auto kGnomeScreenSaverName = "org.gnome.ScreenSaver";
const auto kGnomeScreenSaverPath = "/org/gnome/ScreenSaver";
const auto kFreedesktopScreenSaverName = "org.freedesktop.ScreenSaver";
const auto kFreedesktopScreenSaverPath = "/org/freedesktop/ScreenSaver";

// A desktop that has stopped answering must not hold a session back, so give up on it
const int kCallTimeoutMs = 1000;

GVariant *getProperty(GDBusConnection *bus, const std::string &path, const char *name)
{
  g_autoptr(GError) error = nullptr;
  g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
      bus, kLogindName, path.c_str(), kPropertiesInterface, "Get", g_variant_new("(ss)", kSessionInterface, name),
      G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error
  );
  if (!reply) {
    LOG_DEBUG("logind session property %s unavailable: %s", name, error->message);
    return nullptr;
  }

  GVariant *value = nullptr;
  g_variant_get(reply, "(v)", &value);
  return value;
}

} // namespace

XDGSessionMonitor::XDGSessionMonitor(ReadyCallback onReady) : m_onReady{std::move(onReady)}
{
  // Before the returns below, which a desktop without logind takes while still blanking its screen
  watchScreenSavers();

  g_autoptr(GError) error = nullptr;
  m_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
  if (!m_bus) {
    LOG_DEBUG("no system bus, portal sessions will not wait for the desktop to unlock: %s", error->message);
    return;
  }

  m_sleepSubscription = g_dbus_connection_signal_subscribe(
      m_bus, kLogindName, kManagerInterface, "PrepareForSleep", kLogindPath, nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
      onPrepareForSleep, this, nullptr
  );

  findSession();
  if (m_sessionPath.empty()) {
    LOG_DEBUG("no logind session for this desktop, portal sessions will not wait for the screen to unlock");
    return;
  }

  m_lockSubscription = g_dbus_connection_signal_subscribe(
      m_bus, kLogindName, kPropertiesInterface, "PropertiesChanged", m_sessionPath.c_str(), kSessionInterface,
      G_DBUS_SIGNAL_FLAGS_NONE, onSessionPropertiesChanged, this, nullptr
  );
  m_locked = readLockedHint();
  m_wasReady = isReady();
  LOG_DEBUG("watching logind session %s for lock state, locked=%d", m_sessionPath.c_str(), m_locked.load());
}

XDGSessionMonitor::~XDGSessionMonitor()
{
  if (m_sessionBus) {
    for (auto &watch : m_screenSavers) {
      if (watch.watchId) {
        g_bus_unwatch_name(watch.watchId);
      }
      if (watch.signalId) {
        g_dbus_connection_signal_unsubscribe(m_sessionBus, watch.signalId);
      }
    }
    g_object_unref(m_sessionBus);
  }

  if (m_bus) {
    if (m_sleepSubscription) {
      g_dbus_connection_signal_unsubscribe(m_bus, m_sleepSubscription);
    }
    if (m_lockSubscription) {
      g_dbus_connection_signal_unsubscribe(m_bus, m_lockSubscription);
    }
    g_object_unref(m_bus);
  }
}

bool XDGSessionMonitor::isReady() const
{
  return !m_locked && !m_sleeping && !m_blanked;
}

void XDGSessionMonitor::refresh()
{
  for (auto &watch : m_screenSavers) {
    readScreenSaverActive(watch);
  }
  m_blanked = anyScreenSaverActive();

  if (!m_sessionPath.empty()) {
    m_locked = readLockedHint();
  }

  // Without this a state found here rather than through a signal leaves the next edge looking
  // like no change at all, and the callback that arms the retry never runs
  m_wasReady = isReady();
}

// Desktops launch apps in systemd scopes outside the login session cgroup, so
// GetSessionByPID fails and XDG_SESSION_ID is unset. The "auto" alias resolves to the
// caller's session, or the user's display session, but signals are emitted on the real
// path, so resolve that once here.
void XDGSessionMonitor::findSession()
{
  g_autoptr(GVariant) id = getProperty(m_bus, kAutoSessionPath, "Id");
  if (!id) {
    return;
  }

  g_autoptr(GError) error = nullptr;
  g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
      m_bus, kLogindName, kLogindPath, kManagerInterface, "GetSession",
      g_variant_new("(s)", g_variant_get_string(id, nullptr)), G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1,
      nullptr, &error
  );
  if (!reply) {
    LOG_DEBUG("cannot resolve the logind session path: %s", error->message);
    return;
  }

  const gchar *path = nullptr;
  g_variant_get(reply, "(&o)", &path);
  m_sessionPath = path;
}

bool XDGSessionMonitor::readLockedHint() const
{
  g_autoptr(GVariant) locked = getProperty(m_bus, m_sessionPath, "LockedHint");
  return locked && g_variant_get_boolean(locked);
}

void XDGSessionMonitor::setLocked(bool locked)
{
  if (m_locked.exchange(locked) != locked) {
    LOG_DEBUG("desktop %s", locked ? "locked" : "unlocked");
    notifyIfReady();
  }
}

void XDGSessionMonitor::setSleeping(bool sleeping)
{
  if (m_sleeping.exchange(sleeping) != sleeping) {
    LOG_DEBUG("system %s", sleeping ? "preparing for sleep" : "resumed");
    notifyIfReady();
  }
}

void XDGSessionMonitor::watchScreenSavers()
{
  g_autoptr(GError) error = nullptr;
  m_sessionBus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
  if (!m_sessionBus) {
    LOG_DEBUG("no session bus, portal sessions will not wait for the screen to come back: %s", error->message);
    return;
  }

  m_screenSavers = {
      ScreenSaverWatch{.monitor = this, .name = kGnomeScreenSaverName, .path = kGnomeScreenSaverPath},
      ScreenSaverWatch{.monitor = this, .name = kFreedesktopScreenSaverName, .path = kFreedesktopScreenSaverPath},
  };

  for (auto &watch : m_screenSavers) {
    watch.watchId = g_bus_watch_name_on_connection(
        m_sessionBus, watch.name, G_BUS_NAME_WATCHER_FLAGS_NONE, onScreenSaverAppeared, onScreenSaverVanished, &watch,
        nullptr
    );
    // A lock screen can own its name on more than one object path, so take the signal from any
    watch.signalId = g_dbus_connection_signal_subscribe(
        m_sessionBus, watch.name, watch.name, "ActiveChanged", nullptr, nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
        onScreenSaverActiveChanged, &watch, nullptr
    );
    readScreenSaverActive(watch);
  }

  m_blanked = anyScreenSaverActive();
  m_wasReady = isReady();
  LOG_DEBUG("watching the screensaver for a blank screen, blanked=%d", m_blanked.load());
}

void XDGSessionMonitor::readScreenSaverActive(ScreenSaverWatch &watch)
{
  if (!m_sessionBus || !watch.owned || !watch.supported) {
    return;
  }

  g_autoptr(GError) error = nullptr;
  g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
      m_sessionBus, watch.name, watch.path, watch.name, "GetActive", nullptr, G_VARIANT_TYPE("(b)"),
      G_DBUS_CALL_FLAGS_NO_AUTO_START, kCallTimeoutMs, nullptr, &error
  );
  if (!reply) {
    // GNOME hands this name to a service that only implements idle inhibition, and answering
    // here at all is how it says so; asking it again every retry would be pointless
    if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED)) {
      LOG_DEBUG("%s does not report a blank screen", watch.name);
      watch.supported = false;
    }
    watch.active = false;
    return;
  }

  gboolean active = FALSE;
  g_variant_get(reply, "(b)", &active);
  watch.active = active;
}

bool XDGSessionMonitor::anyScreenSaverActive() const
{
  return std::ranges::any_of(m_screenSavers, [](const ScreenSaverWatch &watch) { return watch.active; });
}

void XDGSessionMonitor::updateBlanked()
{
  const bool blanked = anyScreenSaverActive();
  if (m_blanked.exchange(blanked) != blanked) {
    LOG_DEBUG("screen %s", blanked ? "blanked" : "unblanked");
    notifyIfReady();
  }
}

void XDGSessionMonitor::notifyIfReady()
{
  const bool ready = isReady();
  if (m_wasReady.exchange(ready) || !ready) {
    return;
  }
  if (m_onReady) {
    m_onReady();
  }
}

void XDGSessionMonitor::onPrepareForSleep(
    GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *, GVariant *parameters, gpointer data
)
{
  gboolean sleeping = FALSE;
  g_variant_get(parameters, "(b)", &sleeping);

  auto self = static_cast<XDGSessionMonitor *>(data);
  if (!sleeping && !self->m_sessionPath.empty()) {
    // the lock screen usually engages while asleep, before any change signal reaches us
    self->m_locked = self->readLockedHint();
  }
  self->setSleeping(sleeping);
}

void XDGSessionMonitor::onSessionPropertiesChanged(
    GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *, GVariant *parameters, gpointer data
)
{
  g_autoptr(GVariant) changed = g_variant_get_child_value(parameters, 1);
  g_autoptr(GVariant) locked = g_variant_lookup_value(changed, "LockedHint", G_VARIANT_TYPE_BOOLEAN);
  if (locked) {
    static_cast<XDGSessionMonitor *>(data)->setLocked(g_variant_get_boolean(locked));
  }
}

void XDGSessionMonitor::onScreenSaverAppeared(GDBusConnection *, const gchar *, const gchar *, gpointer data)
{
  auto watch = static_cast<ScreenSaverWatch *>(data);
  watch->owned = true;
  watch->monitor->readScreenSaverActive(*watch);
  watch->monitor->updateBlanked();
}

void XDGSessionMonitor::onScreenSaverVanished(GDBusConnection *, const gchar *, gpointer data)
{
  // A screensaver that is not running cannot be holding the screen blank, and this is what keeps
  // one that died while blank from deferring every session that follows it
  auto watch = static_cast<ScreenSaverWatch *>(data);
  watch->owned = false;
  watch->active = false;
  watch->monitor->updateBlanked();
}

void XDGSessionMonitor::onScreenSaverActiveChanged(
    GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *, GVariant *parameters, gpointer data
)
{
  gboolean active = FALSE;
  g_variant_get(parameters, "(b)", &active);

  auto watch = static_cast<ScreenSaverWatch *>(data);
  watch->active = active;
  watch->monitor->updateBlanked();
}

} // namespace deskflow
