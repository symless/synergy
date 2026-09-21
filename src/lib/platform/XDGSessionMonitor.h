/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <gio/gio.h>

#include <array>
#include <atomic>
#include <functional>
#include <string>

namespace deskflow {

/**
 * @brief Tracks whether the desktop can serve a portal session.
 *
 * Portal sessions requested while the screen is locked or blank, or while the
 * system is suspending, cannot be validated by the compositor, which then falls
 * back to a permission prompt once the screen comes back. Callers check
 * isReady() before requesting a session and are called back once the desktop can
 * serve one again.
 *
 * Lock and suspend come from logind on the system bus; a blank screen comes from
 * the screensaver on the session bus, where the desktop raising its shield is
 * what the compositor itself keys off. Every source fails open, so a desktop
 * that cannot be asked reports ready and nothing waits.
 *
 * GDBus delivers the signals to the main context that is the thread default at
 * construction, so construct this from a callback already running on the
 * context that should receive them; a Qt worker thread pushes a private context
 * that nothing else iterates.
 *
 * A sandboxed build reaches no system bus and is meant not to: Flathub rejects
 * the logind permission, so do not add it back to the Flatpak manifests. Those
 * builds wait on the screensaver alone, which still covers both a locked and a
 * blank screen on the desktops that report it.
 */
class XDGSessionMonitor
{
public:
  using ReadyCallback = std::function<void()>;

  /**
   * @param onReady Called when the desktop can serve a session again, after
   *                having been locked, blank or asleep.
   */
  explicit XDGSessionMonitor(ReadyCallback onReady);
  ~XDGSessionMonitor();

  XDGSessionMonitor(const XDGSessionMonitor &) = delete;
  XDGSessionMonitor &operator=(const XDGSessionMonitor &) = delete;

  /**
   * @return true when the screen is on and unlocked and the system is not
   *         preparing for sleep, or when the state cannot be determined.
   */
  bool isReady() const;

  /**
   * @brief Reads the lock and blank state from the bus again.
   *
   * The compositor closing a session and the signal saying why are independent
   * deliveries, so a retry can fall due before the signal arrives. A caller
   * about to act on isReady() asks for the state again here. It does not run
   * the ready callback, because the caller is already doing what that callback
   * would ask for.
   */
  void refresh();

private:
  struct ScreenSaverWatch
  {
    XDGSessionMonitor *monitor = nullptr;
    const char *name = nullptr; // also the interface name
    const char *path = nullptr;
    guint watchId = 0;
    guint signalId = 0;
    bool owned = true;
    bool supported = true;
    bool active = false;
  };

  void findSession();
  bool readLockedHint() const;
  void setLocked(bool locked);
  void setSleeping(bool sleeping);
  void watchScreenSavers();
  void readScreenSaverActive(ScreenSaverWatch &watch);
  bool anyScreenSaverActive() const;
  void updateBlanked();
  void notifyIfReady();

  static void onPrepareForSleep(
      GDBusConnection *connection, const gchar *sender, const gchar *path, const gchar *interface, const gchar *signal,
      GVariant *parameters, gpointer data
  );
  static void onSessionPropertiesChanged(
      GDBusConnection *connection, const gchar *sender, const gchar *path, const gchar *interface, const gchar *signal,
      GVariant *parameters, gpointer data
  );
  static void onScreenSaverAppeared(GDBusConnection *connection, const gchar *name, const gchar *owner, gpointer data);
  static void onScreenSaverVanished(GDBusConnection *connection, const gchar *name, gpointer data);
  static void onScreenSaverActiveChanged(
      GDBusConnection *connection, const gchar *sender, const gchar *path, const gchar *interface, const gchar *signal,
      GVariant *parameters, gpointer data
  );

  GDBusConnection *m_bus = nullptr;
  GDBusConnection *m_sessionBus = nullptr;
  std::string m_sessionPath;
  guint m_sleepSubscription = 0;
  guint m_lockSubscription = 0;
  std::array<ScreenSaverWatch, 2> m_screenSavers{};
  std::atomic<bool> m_locked{false};
  std::atomic<bool> m_sleeping{false};
  std::atomic<bool> m_blanked{false};
  std::atomic<bool> m_wasReady{true};
  ReadyCallback m_onReady;
};

} // namespace deskflow
