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

const auto kSerialKey = QStringLiteral("serialKey");
const auto kActivated = QStringLiteral("activated");
const auto kGraceStart = QStringLiteral("graceStartEpochSecs");
const auto kOfflineActivationResponse = QStringLiteral("offlineActivationResponse");

} // namespace

void ExtraSettings::load()
{
  QSettings ini(settingsFile(), QSettings::IniFormat);
  m_serialKey = ini.value(kSerialKey).toString();
  m_activated = ini.value(kActivated).toBool();
  m_graceStartEpochSecs = ini.value(kGraceStart).toLongLong();
  m_offlineActivationResponse = ini.value(kOfflineActivationResponse).toString();
}

void ExtraSettings::sync()
{
  QSettings ini(settingsFile(), QSettings::IniFormat);
  if (!ini.isWritable()) {
    qCritical() << "unable to save synergy settings, file not writable:" << ini.fileName();
    return;
  }
  ini.setValue(kSerialKey, m_serialKey);
  ini.setValue(kActivated, m_activated);
  ini.setValue(kGraceStart, m_graceStartEpochSecs);
  ini.setValue(kOfflineActivationResponse, m_offlineActivationResponse);
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
