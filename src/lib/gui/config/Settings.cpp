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

  m_pUserSettings = m_deps->makeSettingsProxy();
  m_pUserSettings->loadUser();

  if (m_pSystemSettings->fileExists()) {
    qDebug("loaded existing system settings");
    m_pActiveSettings = m_pSystemSettings;
    m_scope = Scope::System;
  } else {
    if (m_pUserSettings->fileExists()) {
      qDebug("loaded existing user settings");
    } else {
      qDebug("defaulting to user new settings");
    }
    m_pActiveSettings = m_pUserSettings;
    m_scope = Scope::User;
  }

  m_pLockedSettings = m_deps->makeSettingsProxy();
  m_pLockedSettings->loadLocked();
  if (m_pLockedSettings->fileExists()) {
    qDebug("loaded locked settings");
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

void Settings::save(bool emitSaving)
{
  if (emitSaving) {
    qDebug("emitting config saving signal");
    emit saving();
  }

  qDebug("writing config to filesystem");
  m_pActiveSettings->sync();
}

bool Settings::isWritable() const
{
  return m_pActiveSettings->isWritable();
}

void Settings::setScope(Settings::Scope scope)
{
  if (scope == m_scope) {
    return;
  }

  m_scope = scope;

  if (scope == Scope::User) {
    m_pActiveSettings = m_pUserSettings;
  } else if (scope == Scope::System) {
    m_pActiveSettings = m_pSystemSettings;
  } else {
    qFatal("invalid scope");
  }
}

Settings::Scope Settings::scope() const
{
  return m_scope;
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

} // namespace deskflow::gui
