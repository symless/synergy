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

#include "common/LogLevel.h"
#include "common/Settings.h"

#include <algorithm>

namespace synergy::gui {

std::optional<std::pair<QString, QVariant>> mapLegacySetting(const QString &oldKey, const QVariant &value)
{
  // The server configuration, the screen layout included, has kept this group and these key
  // names through every release, so the whole group is current as it stands.
  if (oldKey.startsWith(QStringLiteral("internalConfig/"))) {
    return std::make_pair(oldKey, value);
  }
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
    // The old value is an index into {INFO, DEBUG, DEBUG1, DEBUG2}; the current one is the name of
    // a level on a scale that starts at FATAL, so the number means something else entirely and
    // passing it through leaves a value the app rejects and replaces with its default. There is
    // nothing between DEBUG and the most detailed level any more, so both old debug levels above
    // the first land there: somebody who asked for more detail should not quietly get less.
    static const QList<LogLevel::Level> levels = {
        LogLevel::Level::Info, LogLevel::Level::Debug, LogLevel::Level::Verbose, LogLevel::Level::Verbose
    };
    const auto index = std::clamp(value.toInt(), 0, static_cast<int>(levels.size()) - 1);
    return std::make_pair(Settings::Log::Level, LogLevel::toOption(static_cast<int>(levels.at(index))));
  }
  if (oldKey == "logToFile") {
    return std::make_pair(Settings::Log::ToFile, value);
  }
  if (oldKey == "logFilename") {
    return std::make_pair(Settings::Log::File, value);
  }
  if (oldKey == "elevateModeEnum") {
    // The old value was an enum, 0 automatic, 1 always, 2 never, and the current setting is a
    // boolean read with toBool(), so passing the number through inverts the one choice that
    // matters: never elevate arrives as true. Automatic meant elevate when required, which is
    // what the boolean's true means and what it defaults to.
    constexpr int kElevateNever = 2;
    return std::make_pair(Settings::Daemon::Elevate, QVariant(value.toInt() != kElevateNever));
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
#ifdef Q_OS_WIN
    const auto mode = value.toBool() ? Settings::Service : Settings::Desktop;
    return std::make_pair(Settings::Core::ProcessMode, QVariant(mode));
#else
    // The daemon is only built on Windows, so carrying this anywhere else puts the core into a
    // mode with nothing to talk to: it fails to start, and the only explanation offered is a
    // dialog about UAC and the Windows services program. The platform default is correct here.
    return std::nullopt;
#endif
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
