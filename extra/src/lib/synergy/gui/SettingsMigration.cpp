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

#include "SettingsMigration.h"

#include "common/Constants.h"
#include "common/LogLevel.h"
#include "common/Settings.h"
#include "synergy/gui/SettingsScope.h"
#include "synergy/gui/styles.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QStringLiteral>

#include <algorithm>
#include <optional>
#include <utility>

namespace synergy::gui::migration {

namespace {

const auto kSchemaKey = QStringLiteral("migration/schemaVersion");
const auto kNotifiedKey = QStringLiteral("migration/notifiedFor");
const auto kBackupPathKey = QStringLiteral("migration/backupPath");
const auto kLegacySystemScopeKey = QStringLiteral("systemScope");
const auto kLegacySerialKey = QStringLiteral("serialKey");
const auto kExtraSerialKey = QStringLiteral("license/serialKey");

QString extraFile()
{
  return QStringLiteral("%1/%2.extra.conf").arg(Settings::UserDir, kAppName);
}

int storedSchemaVersion()
{
  QSettings ini(extraFile(), QSettings::IniFormat);
  return ini.value(kSchemaKey, 0).toInt();
}

void writeSchemaVersion(int version)
{
  QSettings ini(extraFile(), QSettings::IniFormat);
  ini.setValue(kSchemaKey, version);
  ini.sync();
}

int notifiedSchemaVersion()
{
  QSettings ini(extraFile(), QSettings::IniFormat);
  return ini.value(kNotifiedKey, 0).toInt();
}

void writeNotifiedVersion(int version)
{
  QSettings ini(extraFile(), QSettings::IniFormat);
  ini.setValue(kNotifiedKey, version);
  ini.sync();
}

// Persisted so the notice survives a launch the customer did not acknowledge it on, and so a
// fresh install, which also records a schema version, is not mistaken for one that migrated.
QString storedBackupPath()
{
  QSettings ini(extraFile(), QSettings::IniFormat);
  return ini.value(kBackupPathKey).toString();
}

void writeBackupPath(const QString &path)
{
  QSettings ini(extraFile(), QSettings::IniFormat);
  ini.setValue(kBackupPathKey, path);
  ini.sync();
}

std::optional<std::pair<QString, QVariant>> mapKey(const QString &oldKey, const QVariant &value)
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

// File-existence isn't a usable signal: on Linux the legacy NativeFormat
// path coincides with the new IniFormat path, so the file is always
// "present". Sentinel keys discriminate.
bool looksLikeLegacy(const QSettings &legacy)
{
  return legacy.contains(QStringLiteral("startedBefore")) || legacy.contains(QStringLiteral("groupServerChecked")) ||
         legacy.contains(QStringLiteral("groupClientChecked")) || legacy.contains(kLegacySystemScopeKey);
}

// Dumps in-memory contents instead of file-copying so the backup format is
// uniform regardless of the legacy backend (file, plist, or registry).
QString backupLegacy(const QSettings &legacy, const QString &destPath)
{
  if (QFile::exists(destPath)) {
    QFile::remove(destPath);
  }
  QSettings backup(destPath, QSettings::IniFormat);
  for (const auto &key : legacy.allKeys()) {
    backup.setValue(key, legacy.value(key));
  }
  backup.sync();
  return destPath;
}

// Master had hardcoded behavior where beta exposes settings; align beta's
// defaults with master's runtime behavior so migrated users see no change.
void applyMasterCompatDefaults(const QString &newPath)
{
  QSettings newSettings(newPath, QSettings::IniFormat);
  newSettings.setValue(Settings::Gui::AutoStartCore, true);
  newSettings.sync();
}

int migrateOneScope(QSettings &legacy, const QString &newPath)
{
  QSettings newSettings(newPath, QSettings::IniFormat);
  int migrated = 0;
  int dropped = 0;
  for (const auto &oldKey : legacy.allKeys()) {
    if (auto mapped = mapKey(oldKey, legacy.value(oldKey)); mapped.has_value()) {
      newSettings.setValue(mapped->first, mapped->second);
      migrated++;
    } else {
      dropped++;
    }
  }
  newSettings.sync();
  qInfo("settings migration: scope %s, migrated %d keys, dropped %d obsolete", qPrintable(newPath), migrated, dropped);
  return migrated;
}

// The serial key does not belong in Synergy.conf, where cleanSettings() would strip it; it goes
// to the extra file the license code reads. A key already there wins, so a key entered into a
// newer build is never overwritten by a stale one. Activation state is deliberately left behind:
// the next core start re-activates against the current endpoint.
void migrateSerialKey(const QSettings &legacy, const char *scopeLabel)
{
  const auto serialKey = legacy.value(kLegacySerialKey).toString();
  if (serialKey.isEmpty()) {
    return;
  }

  QSettings extra(extraFile(), QSettings::IniFormat);
  if (!extra.value(kExtraSerialKey).toString().isEmpty()) {
    qDebug("settings migration: %s legacy serial key ignored, extra settings already hold one", scopeLabel);
    return;
  }

  extra.setValue(kExtraSerialKey, serialKey);
  extra.sync();
  qInfo("settings migration: %s legacy serial key carried to extra settings", scopeLabel);
}

bool s_migrationRanThisLaunch = false;
QString s_lastBackupPath;

// On Linux, NativeFormat resolves to the same file beta's Settings uses,
// so clear() on the legacy QSettings would wipe the keys we just wrote.
// On macOS/Windows the storage backends are distinct (plist / registry).
bool sharesPathWith(const QSettings &legacy, const QString &newPath)
{
  const auto legacyCanonical = QFileInfo(legacy.fileName()).canonicalFilePath();
  const auto newCanonical = QFileInfo(newPath).canonicalFilePath();
  if (legacyCanonical.isEmpty() || newCanonical.isEmpty()) {
    return QFileInfo(legacy.fileName()).absoluteFilePath() == QFileInfo(newPath).absoluteFilePath();
  }
  return legacyCanonical == newCanonical;
}

void maybeClearLegacy(QSettings &legacy, const QString &newPath, const char *scopeLabel)
{
  if (sharesPathWith(legacy, newPath)) {
    qDebug("settings migration: %s legacy shares storage with new file, skipping clear", scopeLabel);
    return;
  }
  if (!legacy.isWritable()) {
    qWarning("settings migration: %s legacy not writable, leaving legacy keys in place", scopeLabel);
    return;
  }
  legacy.clear();
  legacy.sync();
  qInfo("settings migration: cleared %s legacy storage at %s", scopeLabel, qPrintable(legacy.fileName()));
}

bool runLegacyMigration()
{
  bool any = false;

  QSettings legacyUser(QSettings::NativeFormat, QSettings::UserScope, kAppName, kAppName);
  const bool legacyHadSystemScope = legacyUser.value(kLegacySystemScopeKey, false).toBool();
  if (looksLikeLegacy(legacyUser)) {
    s_lastBackupPath = backupLegacy(legacyUser, Settings::UserSettingFile + QStringLiteral(".legacy.bak"));
    qInfo().noquote() << "settings migration: user-scope legacy backed up to" << s_lastBackupPath;
    if (migrateOneScope(legacyUser, Settings::UserSettingFile) > 0) {
      any = true;
      applyMasterCompatDefaults(Settings::UserSettingFile);
    }
    migrateSerialKey(legacyUser, "user-scope");
    maybeClearLegacy(legacyUser, Settings::UserSettingFile, "user-scope");
  }

  QSettings legacySystem(QSettings::NativeFormat, QSettings::SystemScope, kAppName, kAppName);
  if (looksLikeLegacy(legacySystem)) {
    const auto backupPath = Settings::SystemSettingFile + QStringLiteral(".legacy.bak");
    s_lastBackupPath = backupLegacy(legacySystem, backupPath);
    qInfo().noquote() << "settings migration: system-scope legacy backed up to" << s_lastBackupPath;
    if (migrateOneScope(legacySystem, Settings::SystemSettingFile) > 0) {
      any = true;
      applyMasterCompatDefaults(Settings::SystemSettingFile);
    }
    migrateSerialKey(legacySystem, "system-scope");
    maybeClearLegacy(legacySystem, Settings::SystemSettingFile, "system-scope");
  }

  if (legacyHadSystemScope) {
    SettingsScope::setPreferSystem(true);
  }

  return any;
}

} // namespace

bool migrateIfNeeded()
{
  if (storedSchemaVersion() >= kCurrentSchemaVersion) {
    return false;
  }

  s_migrationRanThisLaunch = runLegacyMigration();
  writeSchemaVersion(kCurrentSchemaVersion);
  if (s_migrationRanThisLaunch) {
    writeBackupPath(s_lastBackupPath);
  }
  return s_migrationRanThisLaunch;
}

void showNoticeIfPending(QWidget *parent)
{
  if (notifiedSchemaVersion() >= kCurrentSchemaVersion) {
    return;
  }

  const auto backupPath = storedBackupPath();
  if (backupPath.isEmpty()) {
    writeNotifiedVersion(kCurrentSchemaVersion);
    return;
  }

  auto *mainWindow = qobject_cast<QMainWindow *>(parent);
  auto *statusBar = mainWindow != nullptr ? mainWindow->statusBar() : nullptr;
  if (statusBar == nullptr) {
    qWarning("settings migration: no status bar, notice not shown");
    return;
  }

  // A status bar pill rather than a dialog. Startup already raises the serial key dialog,
  // activation, and the first-server-start message once the core is up, and Qt leaves two dialogs
  // at once fighting over input: the one in front can be the one that ignores the mouse. Nothing
  // here blocks use of the product, so it waits to be asked.
  auto *pill = new QPushButton(QObject::tr("Settings migrated"), statusBar);
  pill->setFlat(true);
  pill->setStyleSheet(kStyleNoticeLabel);
  pill->setToolTip(QObject::tr("Your settings were migrated to a new format"));
  statusBar->addPermanentWidget(pill);

  QObject::connect(pill, &QPushButton::clicked, pill, [pill, mainWindow, backupPath] {
    QMessageBox::information(
        mainWindow, QObject::tr("Settings updated"),
        QObject::tr(
            "<p>We've migrated your settings to a new format used by this version of Synergy.</p>"
            "<p>Your previous settings have been backed up to:</p>"
            "<p><code>%1</code></p>"
            "<p>If anything looks different, please contact us.</p>"
        )
            .arg(backupPath)
    );
    writeNotifiedVersion(kCurrentSchemaVersion);
    pill->deleteLater();
  });
}

} // namespace synergy::gui::migration
