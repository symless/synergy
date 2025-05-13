/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2020 Symless Ltd.
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

#include "Settings.h"

#include "Logger.h"
#include "constants.h"
#include "proxy/QSettingsProxy.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

namespace deskflow::gui {

using namespace proxy;

//
// Settings::Deps
//

std::shared_ptr<QSettingsProxy> Settings::Deps::makeSettingsProxy()
{
  return std::make_shared<QSettingsProxy>();
}

//
// Settings
//

Settings::Settings(std::shared_ptr<Deps> deps) : m_deps(deps)
{
  qDebug("loading settings");

  m_pSystemSettings = m_deps->makeSettingsProxy();
  m_pSystemSettings->loadSystem();
  logVerbose(tr("system settings keys: %1").arg(m_pSystemSettings->allKeysCSV()));

  m_pUserSettings = m_deps->makeSettingsProxy();
  m_pUserSettings->loadUser();
  logVerbose(tr("user settings keys: %1").arg(m_pUserSettings->allKeysCSV()));

  if (m_pSystemSettings->value(kSystemScopeSetting).toBool()) {
    qDebug("loaded existing system settings");
    m_pActiveSettings = m_pSystemSettings;
  } else {
    // Remove system scope setting from user settings, which will exist on configs from older
    // versions before we moved the setting to system scope. If we were to leave it in, then
    // this would cause a bug in the GUI, since the settings dialog checks this value in all scopes.
    m_pUserSettings->remove(kSystemScopeSetting);

    qDebug("loaded existing user settings");
    m_pActiveSettings = m_pUserSettings;
  }

  m_pLockedSettings = m_deps->makeSettingsProxy();
  m_pLockedSettings->loadLocked();
  logVerbose(tr("locked settings keys: %1").arg(m_pLockedSettings->allKeysCSV()));
  if (m_pLockedSettings->fileExists()) {
    qDebug("loaded locked settings");
    m_pActiveSettings->copyFrom(*m_pLockedSettings);
  }
}

QSettingsProxy &Settings::getActiveSettings()
{
  return *m_pActiveSettings.get();
}

QSettingsProxy &Settings::getSystemSettings()
{
  return *m_pSystemSettings.get();
}

QSettingsProxy &Settings::getUserSettings()
{
  return *m_pUserSettings.get();
}

QSettingsProxy &Settings::getLockedSettings()
{
  return *m_pLockedSettings.get();
}

QString Settings::fileName() const
{
  return m_pActiveSettings->fileName();
}

void Settings::clear()
{
  m_pActiveSettings->clear();
}

void Settings::signalReady()
{
  emit ready();
}

void Settings::sync()
{
  qDebug() << "emitting before sync signal";
  emit beforeSync();

  qDebug().noquote() << "settings sync, filename:" << m_pActiveSettings->fileName();
  if (m_pActiveSettings->isWritable()) {
    qDebug() << "setting save will be skipped, not writable";
  }

  m_pActiveSettings->sync();
}

bool Settings::isWritable() const
{
  return m_pActiveSettings->isWritable();
}

bool Settings::contains(const QString &name) const
{
  return m_pActiveSettings->contains(name);
}

QVariant Settings::get(const QString &name, const QVariant &defaultValue) const
{
  return m_pActiveSettings->value(name, defaultValue);
}

void Settings::set(const QString &name, const QVariant &value)
{
  m_pActiveSettings->setValue(name, value);
}

bool Settings::isUnavailable() const
{
  return !m_pUserSettings->isWritable() && !m_pSystemSettings->fileExists();
}

} // namespace deskflow::gui
