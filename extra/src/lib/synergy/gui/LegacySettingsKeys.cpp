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

#include "LegacySettingsKeys.h"

#include "common/Settings.h"

namespace synergy::gui {

std::optional<std::pair<QString, QVariant>> mapLegacySetting(const QString &oldKey, const QVariant &value)
{
  if (oldKey == "screenName") {
    return std::make_pair(Settings::Core::ComputerName, value);
  }
  if (oldKey == "port") {
    return std::make_pair(Settings::Core::Port, value);
  }
  if (oldKey == "interface") {
    return std::make_pair(Settings::Core::Interface, value);
  }
  if (oldKey == "logLevel2") {
    return std::make_pair(Settings::Log::Level, value);
  }
  if (oldKey == "logToFile") {
    return std::make_pair(Settings::Log::ToFile, value);
  }
  if (oldKey == "logFilename") {
    return std::make_pair(Settings::Log::File, value);
  }
  if (oldKey == "elevateModeEnum") {
    return std::make_pair(Settings::Daemon::Elevate, value);
  }
  if (oldKey == "cryptoEnabled") {
    return std::make_pair(Settings::Security::TlsEnabled, value);
  }
  if (oldKey == "autoHide") {
    return std::make_pair(Settings::Gui::Autohide, value);
  }
  if (oldKey == "lastVersion") {
    return std::make_pair(Settings::Core::LastVersion, value);
  }
  if (oldKey == "groupServerChecked") {
    if (value.toBool()) {
      return std::make_pair(Settings::Core::CoreMode, QVariant(Settings::Server));
    }
    return std::nullopt;
  }
  if (oldKey == "groupClientChecked") {
    if (value.toBool()) {
      return std::make_pair(Settings::Core::CoreMode, QVariant(Settings::Client));
    }
    return std::nullopt;
  }
  if (oldKey == "useExternalConfig") {
    return std::make_pair(Settings::Server::ExternalConfig, value);
  }
  if (oldKey == "configFile") {
    return std::make_pair(Settings::Server::ExternalConfigFile, value);
  }
  if (oldKey == "serverHostname") {
    return std::make_pair(Settings::Client::RemoteHost, value);
  }
  if (oldKey == "tlsCertPath") {
    return std::make_pair(Settings::Security::Certificate, value);
  }
  if (oldKey == "tlsKeyLength") {
    return std::make_pair(Settings::Security::KeySize, value);
  }
  if (oldKey == "preventSleep") {
    return std::make_pair(Settings::Core::PreventSleep, value);
  }
  if (oldKey == "languageSync") {
    return std::make_pair(Settings::Client::LanguageSync, value);
  }
  if (oldKey == "invertScrollDirection") {
    return std::make_pair(Settings::Client::InvertYScroll, value);
  }
  if (oldKey == "enableService") {
    const auto mode = value.toBool() ? Settings::Service : Settings::Desktop;
    return std::make_pair(Settings::Core::ProcessMode, QVariant(mode));
  }
  if (oldKey == "closeToTray") {
    return std::make_pair(Settings::Gui::CloseToTray, value);
  }
  if (oldKey == "showCloseReminder") {
    return std::make_pair(Settings::Gui::CloseReminder, value);
  }
  if (oldKey == "enableUpdateCheck") {
    return std::make_pair(Settings::Gui::AutoUpdateCheck, value);
  }
  return std::nullopt;
}

} // namespace synergy::gui
