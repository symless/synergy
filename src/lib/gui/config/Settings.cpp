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

  auto system = m_deps->makeSettingsProxy();
  system->loadSystem();

  auto user = m_deps->makeSettingsProxy();
  user->loadUser();

  if (system->fileExists()) {
    qDebug("loaded existing system settings");
    m_pSettingsProxy = system;
    m_scope = Scope::System;
    return;
  }

  if (user->fileExists()) {
    qDebug("loaded existing user settings");
  } else {
    qDebug("defaulting to user new settings");
  }

  m_pSettingsProxy = user;
  m_scope = Scope::User;
}

QSettingsProxy &Settings::getProxy()
{
  return *m_pSettingsProxy.get();
}

const QSettingsProxy &Settings::getProxy() const
{
  return *m_pSettingsProxy.get();
}

QString Settings::fileName() const
{
  return m_pSettingsProxy->fileName();
}

void Settings::clear()
{
  m_pSettingsProxy->clear();
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
  m_pSettingsProxy->sync();
}

bool Settings::isWritable() const
{
  return m_pSettingsProxy->isWritable();
}

void Settings::setScope(Settings::Scope scope)
{
  if (scope == m_scope) {
    return;
  }

  if (scope == Scope::User) {
    m_pSettingsProxy->loadUser();
  } else if (scope == Scope::System) {
    m_pSettingsProxy->loadSystem();
  } else {
    qFatal("invalid scope");
  }

  m_scope = scope;
}

Settings::Scope Settings::scope() const
{
  return m_scope;
}

bool Settings::contains(const QString &name) const
{
  return m_pSettingsProxy->contains(name);
}

QVariant Settings::get(const QString &name, const QVariant &defaultValue) const
{
  return m_pSettingsProxy->value(name, defaultValue);
}

void Settings::set(const QString &name, const QVariant &value)
{
  m_pSettingsProxy->setValue(name, value);
}

} // namespace deskflow::gui
