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
#include "common/Settings.h"
#include "synergy/gui/LegacySettingsKeys.h"
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

#include <optional>

namespace synergy::gui::migration {

namespace {

const auto kSchemaKey = QStringLiteral("migration/schemaVersion");
const auto kNotifiedKey = QStringLiteral("migration/notifiedFor");
const auto kBackupPathKey = QStringLiteral("migration/backupPath");
const auto kInternalConfigGroup = QStringLiteral("internalConfig/");
const auto kScreensSizeKey = QStringLiteral("internalConfig/screens/size");
const auto kScreenNameKey = QStringLiteral("internalConfig/screens/%1/name");

// The flag moved over the years: the releases up to 1.14 kept it in the user scope, and 1.20 read
// it from the system scope and deleted it from the user's. Set in either, the system scope held
// the live settings.
const auto kLegacySystemScopeKey = QStringLiteral("loadFromSystemScope");

// The schemas a machine may have recorded before this one, each named for what its migration got
// wrong. Schema 1 read the wrong macOS preferences domain, so on macOS it carried nothing. Schema
// 2 dropped the server configuration group and never read the All users scope.
constexpr int kSchemaWrongMacDomain = 1;
constexpr int kSchemaWithoutServerConfig = 2;

// Qt builds the macOS preferences domain from the organization, reversing it if it is dotted and
// prefixing "com." if it is not. Every release up to 1.21 wrote com.symless.Synergy, which comes
// from "symless.com" and not from the application name: passing that lands on com.synergy.Synergy,
// a domain no release has ever written, so the migration finds nothing and the customer's settings
// and serial key stay behind in the real one. The old domain is a historical fact and does not
// follow the current brand, so it is spelled out rather than derived. Linux keys its path off the
// application name alone and Windows off the registry path, so both are already right.
#ifdef Q_OS_MAC
const auto kLegacyOrganization = QStringLiteral("symless.com");
#else
const auto kLegacyOrganization = QString::fromUtf8(kAppName);
#endif
const auto kLegacySerialKey = QStringLiteral("serialKey");
const auto kExtraSerialKey = QStringLiteral("license/serialKey");

bool s_migrationRanThisLaunch = false;
QString s_lastBackupPath;

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
    if (auto mapped = mapLegacySetting(oldKey, legacy.value(oldKey)); mapped.has_value()) {
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

bool migrateUserScope(QSettings &legacyUser)
{
  s_lastBackupPath = backupLegacy(legacyUser, Settings::UserSettingFile + QStringLiteral(".legacy.bak"));
  qInfo().noquote() << "settings migration: user-scope legacy backed up to" << s_lastBackupPath;
  const bool carried = migrateOneScope(legacyUser, Settings::UserSettingFile) > 0;
  if (carried) {
    applyMasterCompatDefaults(Settings::UserSettingFile);
  }
  migrateSerialKey(legacyUser, "user-scope");
  maybeClearLegacy(legacyUser, Settings::UserSettingFile, "user-scope");
  return carried;
}

// The system scope is only any use from a launch that can write it. Where this one cannot, and
// the system scope held the live settings, they are carried into the user scope instead, over
// whatever the user scope held: an install that starts empty is worse than one in the other
// scope. A system scope that was not the live one is left where it is in that case, because the
// user scope carried the live settings and writing the other set over them would change what
// the customer sees.
bool migrateSystemScope(QSettings &legacySystem, bool systemWasActive)
{
  const bool systemWritable = SettingsScope::isSystemWritable();
  if (!systemWritable && !systemWasActive) {
    qWarning("settings migration: system-scope legacy found, system scope not writable, leaving it in place");
    return false;
  }

  const auto target = systemWritable ? Settings::SystemSettingFile : Settings::UserSettingFile;
  const auto suffix = systemWritable ? QStringLiteral(".legacy.bak") : QStringLiteral(".system.legacy.bak");
  s_lastBackupPath = backupLegacy(legacySystem, target + suffix);
  qInfo().noquote() << "settings migration: system-scope legacy backed up to" << s_lastBackupPath;

  const bool carried = migrateOneScope(legacySystem, target) > 0;
  if (carried) {
    applyMasterCompatDefaults(target);
  }
  migrateSerialKey(legacySystem, "system-scope");
  maybeClearLegacy(legacySystem, target, "system-scope");

  if (!systemWritable) {
    qWarning("settings migration: system scope not writable, system-scope legacy carried to the user scope");
  } else if (systemWasActive) {
    SettingsScope::setPreferSystem(true);
  }
  return carried;
}

bool runLegacyMigration()
{
  QSettings legacyUser(QSettings::NativeFormat, QSettings::UserScope, kLegacyOrganization, kAppName);

  // 1.20 kept the All users scope in an ini file, not the native store: it opened Qt's
  // system-scope ini for an organisation and an application both named after the app, which is
  // C:\ProgramData\Synergy\Synergy.ini on Windows, /Library/Preferences/Synergy/Synergy.ini on
  // macOS and /etc/xdg/Synergy/Synergy.ini on Linux. Opened the same way here so the path stays
  // whatever Qt says it is. The plain name is the organisation on every platform, since the
  // domain only ever shaped the macOS plist.
  const auto appName = QString::fromUtf8(kAppName);
  QSettings legacySystem(QSettings::IniFormat, QSettings::SystemScope, appName, appName);

  const bool systemWasActive = legacySystem.value(kLegacySystemScopeKey, false).toBool() ||
                               legacyUser.value(kLegacySystemScopeKey, false).toBool();

  bool any = false;
  if (looksLikeLegacy(legacyUser)) {
    any = migrateUserScope(legacyUser);
  }
  if (looksLikeLegacy(legacySystem)) {
    any = migrateSystemScope(legacySystem, systemWasActive) || any;
  }
  return any;
}

// The window adds the server's own screen to an empty layout by itself, so a layout holding only
// that one is still empty for this purpose.
bool hasClientScreens(const QSettings &settings)
{
  const auto computerName = settings.value(Settings::Core::ComputerName).toString();
  const auto count = settings.value(kScreensSizeKey).toInt();
  for (int i = 1; i <= count; ++i) {
    const auto name = settings.value(kScreenNameKey.arg(i)).toString();
    if (!name.isEmpty() && name != computerName) {
      return true;
    }
  }
  return false;
}

// A machine that migrated under schema 2 has its server configuration only in the backup that
// migration took, since the native store was cleared once the backup was written. The group is
// put back from there into whichever scope the machine now uses, unless the customer has rebuilt
// a layout by hand since: that one is newer than the backup and is theirs to keep.
bool recoverServerConfig()
{
  const auto backupPath = storedBackupPath();
  if (backupPath.isEmpty() || !QFile::exists(backupPath)) {
    return false;
  }

  const auto settingsPath = SettingsScope::preferSystem() ? Settings::SystemSettingFile : Settings::UserSettingFile;
  QSettings current(settingsPath, QSettings::IniFormat);
  if (hasClientScreens(current)) {
    qInfo("settings migration: screen layout rebuilt since the last migration, backup left alone");
    return false;
  }

  const QSettings backup(backupPath, QSettings::IniFormat);
  int recovered = 0;
  for (const auto &key : backup.allKeys()) {
    if (key.startsWith(kInternalConfigGroup)) {
      current.setValue(key, backup.value(key));
      recovered++;
    }
  }
  current.sync();
  if (recovered == 0) {
    return false;
  }

  s_lastBackupPath = backupPath;
  qInfo("settings migration: recovered %d server configuration keys from %s", recovered, qPrintable(backupPath));
  return true;
}

} // namespace

bool migrateIfNeeded()
{
  const auto stored = storedSchemaVersion();
  if (stored >= kCurrentSchemaVersion) {
    return false;
  }

  // A machine that never migrated reads the legacy store, and so does one whose migration read
  // the wrong domain, since that one found nothing to clear. Any machine that has migrated before
  // may be missing its server configuration, and gets it back from that migration's backup.
  bool ran = false;
  if (stored <= kSchemaWrongMacDomain) {
    ran = runLegacyMigration();
  }
  if (stored > 0 && stored <= kSchemaWithoutServerConfig) {
    ran = recoverServerConfig() || ran;
  }

  s_migrationRanThisLaunch = ran;
  writeSchemaVersion(kCurrentSchemaVersion);
  if (s_migrationRanThisLaunch) {
    writeBackupPath(s_lastBackupPath);
  } else {
    // Nothing was carried, so there is nothing to tell the customer about. Without this, a machine
    // that migrated cleanly under an earlier schema would raise the notice a second time when the
    // schema is bumped, pointing at a backup taken releases ago.
    writeNotifiedVersion(kCurrentSchemaVersion);
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
