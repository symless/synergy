/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2024 - 2026 Synergy App Ltd
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ExtraSettings.h"

#include "common/Constants.h"
#include "common/Settings.h"

#include <QDebug>
#include <QFileInfo>
#include <QSettings>

namespace synergy::gui {

namespace {

// Persisted synergy-side state lives in its own sibling file rather than
// upstream's Synergy.conf, because upstream's Settings::cleanSettings()
// strips any key not in its allow-list at startup. Keeping our state out
// of that file means no upstream patch + no key-allow-list maintenance.
QString settingsFile()
{
  return QStringLiteral("%1/%2.extra.conf").arg(Settings::UserDir, kAppName);
}

const auto kLicenseGroup = QStringLiteral("license");

const auto kSerialKey = QStringLiteral("serialKey");
const auto kActivatedMode = QStringLiteral("activatedMode");
const auto kOfflineActivationResponse = QStringLiteral("offlineActivationResponse");

const auto kModeServer = QStringLiteral("server");
const auto kModeClient = QStringLiteral("client");

// Pre-slot-limit activation state. Dropped rather than migrated: the next core start activates
// under the new protocol, which is idempotent for a machine the website already knows.
const auto kLegacyActivated = QStringLiteral("activated");
const auto kLegacyHoldsServerActivation = QStringLiteral("holdsServerActivation");
const auto kLegacyGraceStart = QStringLiteral("graceStartEpochSecs");
const auto kLegacyActivationState = QStringLiteral("activationState");

ExtraSettings::ActivatedMode parseActivatedMode(const QString &value)
{
  using enum ExtraSettings::ActivatedMode;
  if (value == kModeServer) {
    return kServer;
  }
  if (value == kModeClient) {
    return kClient;
  }
  return kNone;
}

QString activatedModeString(ExtraSettings::ActivatedMode mode)
{
  using enum ExtraSettings::ActivatedMode;
  switch (mode) {
  case kServer:
    return kModeServer;
  case kClient:
    return kModeClient;
  default:
    return {};
  }
}

} // namespace

void ExtraSettings::load()
{
  QSettings ini(settingsFile(), QSettings::IniFormat);

  // License state moved from the top level into the license group; fall back to the old flat key
  // so existing installs keep their serial key until the next sync rewrites it under the group.
  const auto value = [&ini](const QString &key) {
    const auto grouped = QStringLiteral("%1/%2").arg(kLicenseGroup, key);
    return ini.contains(grouped) ? ini.value(grouped) : ini.value(key);
  };

  m_serialKey = value(kSerialKey).toString();
  m_activatedMode = parseActivatedMode(value(kActivatedMode).toString());
  m_offlineActivationResponse = value(kOfflineActivationResponse).toString();
}

void ExtraSettings::sync()
{
  QSettings ini(settingsFile(), QSettings::IniFormat);
  if (!ini.isWritable()) {
    qCritical() << "unable to save synergy settings, file not writable:" << ini.fileName();
    return;
  }

  // Drop the old top-level keys (license state now lives under the license group) and any
  // legacy key, wherever it was written.
  for (const auto &key : {kSerialKey, kOfflineActivationResponse}) {
    ini.remove(key);
  }
  for (const auto &key : {kLegacyActivated, kLegacyHoldsServerActivation, kLegacyGraceStart, kLegacyActivationState}) {
    ini.remove(key);
    ini.remove(QStringLiteral("%1/%2").arg(kLicenseGroup, key));
  }

  ini.beginGroup(kLicenseGroup);
  ini.setValue(kSerialKey, m_serialKey);
  ini.setValue(kActivatedMode, activatedModeString(m_activatedMode));
  ini.setValue(kOfflineActivationResponse, m_offlineActivationResponse);
  ini.endGroup();
  ini.sync();
}

QString ExtraSettings::fileName() const
{
  return settingsFile();
}

bool ExtraSettings::isWritable() const
{
  QFileInfo info(settingsFile());
  if (info.exists()) {
    return info.isWritable();
  }
  // If the file doesn't exist yet, writability depends on the parent dir.
  return QFileInfo(info.absolutePath()).isWritable();
}

} // namespace synergy::gui
