/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2026 Synergy App Ltd
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

#include "TestSettings.h"

#include "common/Constants.h"
#include "common/Settings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>

namespace synergy::gui {

namespace {

QString resolveFileName()
{
  const auto name = QStringLiteral("%1.test.conf").arg(kAppName);
  const auto fallback = QStringLiteral("%1/%2").arg(Settings::UserDir, name);

  // applicationDirPath() warns if called before the app object exists
  if (QCoreApplication::instance() == nullptr) {
    return fallback;
  }

  QDir dir(QCoreApplication::applicationDirPath());
  do {
    if (dir.exists(name)) {
      return dir.filePath(name);
    }
  } while (dir.cdUp());
  return fallback;
}

} // namespace

TestSettings &TestSettings::instance()
{
  static TestSettings inst;
  return inst;
}

TestSettings::TestSettings()
{
  load();
}

void TestSettings::load()
{
  m_fileName = resolveFileName();
  m_enabled = false;
  m_licensing = false;
  m_serialKey.clear();
  m_apiUrlBase.clear();
  m_machineId.clear();
  m_startTimeEpochSecs = 0;
  m_verbose = false;
  m_skipRemoteCheck = false;
  m_allowExpiredLicenses = false;

  if (!QFile::exists(m_fileName)) {
    qDebug().noquote() << "test settings file not found, test mode off:" << m_fileName;
    return;
  }

  QSettings ini(m_fileName, QSettings::IniFormat);
  m_enabled = ini.value(QStringLiteral("test/enabled"), false).toBool();
  if (!m_enabled) {
    qDebug().noquote() << "test settings file present but test/enabled=false:" << m_fileName;
    return;
  }
  m_licensing = ini.value(QStringLiteral("test/licensing"), false).toBool();
  m_serialKey = ini.value(QStringLiteral("test/serialKey")).toString();
  m_apiUrlBase = ini.value(QStringLiteral("test/apiUrlBase")).toString();
  m_machineId = ini.value(QStringLiteral("test/machineId")).toString();
  m_startTimeEpochSecs = ini.value(QStringLiteral("test/startTime"), 0).toLongLong();

  m_verbose = ini.value(QStringLiteral("features/verbose"), false).toBool();
  m_skipRemoteCheck = ini.value(QStringLiteral("features/skipRemoteCheck"), false).toBool();
  m_allowExpiredLicenses = ini.value(QStringLiteral("features/allowExpiredLicenses"), false).toBool();

  qInfo().noquote() << "test mode enabled, loaded:" << m_fileName;
}

void TestSettings::reload()
{
  load();
}

} // namespace synergy::gui
