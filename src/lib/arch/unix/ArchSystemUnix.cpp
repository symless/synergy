/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
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

#include "arch/unix/ArchSystemUnix.h"

#include "base/Log.h"

#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <sys/utsname.h>

#ifndef __APPLE__
#include <QtDBus>
#endif

const auto kConfigFilePath = ".config/Synergy/synergy-core.ini";

//
// ArchSystemUnix
//

ArchSystemUnix::ArchSystemUnix()
{
  // do nothing
}

ArchSystemUnix::~ArchSystemUnix()
{
  // do nothing
}

std::string ArchSystemUnix::getOSName() const
{
#if defined(HAVE_SYS_UTSNAME_H)
  struct utsname info;
  if (uname(&info) == 0) {
    std::string msg;
    msg += info.sysname;
    msg += " ";
    msg += info.release;
    return msg;
  }
#endif
  return "Unix";
}

std::string ArchSystemUnix::getPlatformName() const
{
#if defined(HAVE_SYS_UTSNAME_H)
  struct utsname info;
  if (uname(&info) == 0) {
    return std::string(info.machine);
  }
#endif
  return "unknown";
}

std::filesystem::path configFilePath()
{
  return std::filesystem::path(getenv("HOME")) / kConfigFilePath;
}

std::map<std::string, std::string> readConfig()
{
  const auto filePath = configFilePath();
  LOG_DEBUG("reading config file: %s", filePath.c_str());
  if (!std::filesystem::exists(filePath)) {
    return {};
  }

  std::map<std::string, std::string> kv;
  std::ifstream in(filePath);

  std::string line;
  while (std::getline(in, line)) {
    auto pos = line.find('=');
    if (pos == std::string::npos)
      continue;
    kv[line.substr(0, pos)] = line.substr(pos + 1);
  }

  return kv;
}

std::string ArchSystemUnix::setting(const std::string &key) const
{
  auto kv = readConfig();
  auto it = kv.find(key);
  if (it != kv.end()) {
    return it->second;
  }
  return "";
}

void ArchSystemUnix::setting(const std::string &key, const std::string &value) const
{
  auto kv = readConfig();
  kv[key] = value;

  const auto filePath = configFilePath();
  LOG_DEBUG("writing config file: %s", filePath.c_str());
  std::filesystem::create_directories(filePath.parent_path());
  std::ofstream out(filePath, std::ios::trunc);
  for (const auto &[k, v] : kv) {
    out << k << "=" << v << "\n";
  }
}

void ArchSystemUnix::clearSettings() const
{
  // Not implemented
}

std::string ArchSystemUnix::getLibsUsed(void) const
{
  return "not implemented.\nuse lsof on shell";
}

#ifndef __APPLE__
bool ArchSystemUnix::DBusInhibitScreenCall(InhibitScreenServices serviceID, bool state, std::string &error)
{
  error = "";
  static const std::array<QString, 2> services = {"org.freedesktop.ScreenSaver", "org.gnome.SessionManager"};
  static const std::array<QString, 2> paths = {"/org/freedesktop/ScreenSaver", "/org/gnome/SessionManager"};
  static std::array<uint, 2> cookies;

  auto serviceNum = static_cast<uint8_t>(serviceID);

  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) {
    error = "bus failed to connect";
    return false;
  }

  QDBusInterface screenSaverInterface(services[serviceNum], paths[serviceNum], services[serviceNum], bus);

  if (!screenSaverInterface.isValid()) {
    error = "screen saver interface failed to initialize";
    return false;
  }

  QDBusReply<uint> reply;
  if (state) {
    if (cookies[serviceNum]) {
      error = "coockies are not empty";
      return false;
    }

    reply = screenSaverInterface.call(
        "Inhibit", DESKFLOW_APP_NAME, "Sleep is manually prevented by the " DESKFLOW_APP_NAME " preferences"
    );
    if (reply.isValid())
      cookies[serviceNum] = reply.value();
  } else {
    if (!cookies[serviceNum]) {
      error = "coockies are empty";
      return false;
    }
    reply = screenSaverInterface.call("UnInhibit", cookies[serviceNum]);
    cookies[serviceNum] = 0;
  }

  if (!reply.isValid()) {
    QDBusError qerror = reply.error();
    error = qerror.name().toStdString() + " : " + qerror.message().toStdString();
    return false;
  }

  return true;
}
#endif
