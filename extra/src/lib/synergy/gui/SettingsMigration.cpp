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
#include "gui/TlsUtility.h"
#include "net/Fingerprint.h"
#include "net/FingerprintDatabase.h"
#include "synergy/gui/LegacySettingsKeys.h"
#include "synergy/gui/SettingsScope.h"
#include "synergy/gui/UpdateChannel.h"
#include "synergy/gui/styles.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QStringLiteral>
#include <QTextStream>

#include <optional>

namespace synergy::gui::migration {

namespace {

const auto kSchemaKey = QStringLiteral("migration/schemaVersion");
const auto kNotifiedKey = QStringLiteral("migration/notifiedFor");
const auto kBackupPathKey = QStringLiteral("migration/backupPath");
const auto kBackupSuffix = QStringLiteral(".legacy.bak");
const auto kSystemBackupSuffix = QStringLiteral(".system.legacy.bak");
const auto kInternalConfigGroup = QStringLiteral("internalConfig/");
const auto kScreensSizeKey = QStringLiteral("internalConfig/screens/size");
const auto kScreenNameKey = QStringLiteral("internalConfig/screens/%1/name");

// The flag moved over the years: the releases up to 1.14 kept it in the user scope, and 1.20 read
// it from the system scope and deleted it from the user's. Set in either, the system scope held
// the live settings.
const auto kLegacySystemScopeKey = QStringLiteral("loadFromSystemScope");

// The schemas a machine may have recorded before this one, each named for what its migration got
// wrong. Schema 1 read the wrong macOS preferences domain, so on macOS it carried nothing. Schema
// 2 dropped the server configuration group and never read the All users scope. Schema 3, like
// both before it, dropped the update channel.
constexpr int kSchemaWrongMacDomain = 1;
constexpr int kSchemaWithoutServerConfig = 2;
constexpr int kSchemaWithoutUpdateChannel = 3;

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

const auto kLegacyUpdateTrackKey = QStringLiteral("updateTrack");

const auto kLegacyCertificateFile = QStringLiteral("%1.pem").arg(kAppName);
constexpr int kMinimumKeyLength = 2048;
const auto kLegacyTrustedServersFile = QStringLiteral("SSL/Fingerprints/TrustedServers.txt");
const auto kLegacyTrustedClientsFile = QStringLiteral("SSL/Fingerprints/TrustedClients.txt");

bool s_migrationRanThisLaunch = false;
QString s_lastBackupPath;
QString s_legacyTlsDir;

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

// Beta goes over a stable channel already stored, since the settings dialog writes the channel on
// every save and a stored stable may never have been chosen.
void migrateUpdateChannel(const QString &legacyTrack)
{
  if (legacyTrack != UpdateChannel::Beta) {
    return;
  }

  UpdateChannel::setCurrent(UpdateChannel::Beta);
  qInfo("settings migration: legacy beta update channel carried to extra settings");
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
  s_lastBackupPath = backupLegacy(legacyUser, Settings::UserSettingFile + kBackupSuffix);
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
  const auto suffix = systemWritable ? kBackupSuffix : kSystemBackupSuffix;
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

  // Read before the scopes are migrated, since migrating one clears it.
  const auto legacyTrack = (systemWasActive ? legacySystem : legacyUser).value(kLegacyUpdateTrackKey).toString();

  bool any = false;
  if (looksLikeLegacy(legacyUser)) {
    any = migrateUserScope(legacyUser);
  }
  if (looksLikeLegacy(legacySystem)) {
    any = migrateSystemScope(legacySystem, systemWasActive) || any;
  }
  migrateUpdateChannel(legacyTrack);

  // 1.20 kept its TLS files in the ini config directory of the live scope, and passed the same
  // directory to the core, which is where its trusted fingerprints were read from.
  if (any) {
    const QSettings legacyIniUser(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    const auto &liveScope = systemWasActive ? legacySystem : legacyIniUser;
    s_legacyTlsDir = QFileInfo(liveScope.fileName()).absolutePath();
  }
  return any;
}

void migrateCertificate(const QString &legacyPath)
{
  if (!QFile::exists(legacyPath)) {
    qDebug("settings migration: no legacy tls certificate at %s", qPrintable(legacyPath));
    return;
  }

  const auto target = Settings::value(Settings::Security::Certificate).toString();
  if (target != Settings::defaultValue(Settings::Security::Certificate).toString()) {
    qInfo("settings migration: tls certificate path carried, legacy default certificate not copied");
    return;
  }

  if (QFile::exists(target)) {
    qInfo("settings migration: tls certificate already at %s, legacy certificate not copied", qPrintable(target));
    return;
  }

  if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(legacyPath, target)) {
    qWarning("settings migration: failed to copy legacy tls certificate to %s", qPrintable(target));
    return;
  }
  qInfo("settings migration: legacy tls certificate copied to %s", qPrintable(target));
}

// OpenSSL rejects a key under 2048 bits in the handshake with "ee key too small", so a carried
// certificate that small is an identity nothing can connect to. It is replaced, and generating
// over it keeps the old one beside the new as .pem.old.
void replaceSmallCertificate()
{
  const auto path = Settings::value(Settings::Security::Certificate).toString();
  if (!QFile::exists(path)) {
    return;
  }

  const auto keyLength = deskflow::gui::TlsUtility::getCertKeyLength(path);
  if (keyLength <= 0) {
    qWarning("settings migration: unable to read the tls certificate key size, left as it is");
    return;
  }
  if (keyLength >= kMinimumKeyLength) {
    return;
  }

  qInfo("settings migration: tls certificate key is %d bits, replacing it with a %d-bit one", keyLength, kMinimumKeyLength);
  if (!deskflow::gui::TlsUtility::generateCertificate(kMinimumKeyLength)) {
    qWarning("settings migration: failed to replace the %d-bit tls certificate at %s", keyLength, qPrintable(path));
  }
}

// 1.20 wrote one fingerprint per line as colon-separated hex with no type, which the current
// format only accepts for SHA-1, so the type is taken from the length.
Fingerprint fingerprintFromLegacyLine(const QString &line)
{
  auto hex = line;
  hex.remove(QLatin1Char(':'));

  Fingerprint fingerprint;
  fingerprint.data = QByteArray::fromHex(hex.toLatin1());
  if (fingerprint.data.size() * 2 != hex.size()) {
    return {};
  }

  if (fingerprint.data.size() == 32) {
    fingerprint.type = QCryptographicHash::Sha256;
  } else if (fingerprint.data.size() == 20) {
    fingerprint.type = QCryptographicHash::Sha1;
  }
  return fingerprint;
}

void migrateTrustedFingerprints(const QString &legacyPath, const QString &targetPath)
{
  QFile legacy(legacyPath);
  if (!legacy.exists()) {
    return;
  }
  if (!legacy.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning("settings migration: unable to read legacy trusted fingerprints at %s", qPrintable(legacyPath));
    return;
  }

  FingerprintDatabase db;
  db.read(targetPath);

  int added = 0;
  int unreadable = 0;
  QTextStream in(&legacy);
  while (!in.atEnd()) {
    const auto line = in.readLine().trimmed();
    if (line.isEmpty()) {
      continue;
    }
    const auto fingerprint = fingerprintFromLegacyLine(line);
    if (!fingerprint.isValid()) {
      unreadable++;
    } else if (!db.isTrusted(fingerprint)) {
      db.addTrusted(fingerprint);
      added++;
    }
  }

  if (unreadable > 0) {
    qWarning("settings migration: %d unreadable lines in legacy trusted fingerprints", unreadable);
  }
  if (added == 0) {
    return;
  }

  if (!QDir().mkpath(QFileInfo(targetPath).absolutePath()) || !db.write(targetPath)) {
    qWarning("settings migration: failed to write trusted fingerprints to %s", qPrintable(targetPath));
    return;
  }
  qInfo("settings migration: %d legacy trusted fingerprints carried to %s", added, qPrintable(targetPath));
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

// A machine with legacy settings in both scopes backed up each, and the path recorded is whichever
// came last. Only the scope that held the live settings has the channel the customer was using,
// and a system scope backup says it was live by carrying the flag.
QString liveScopeBackupPath()
{
  for (const auto &path :
       {Settings::SystemSettingFile + kBackupSuffix, Settings::UserSettingFile + kSystemBackupSuffix}) {
    if (QSettings(path, QSettings::IniFormat).value(kLegacySystemScopeKey, false).toBool()) {
      return path;
    }
  }
  return Settings::UserSettingFile + kBackupSuffix;
}

void recoverUpdateChannel()
{
  const auto backupPath = liveScopeBackupPath();
  if (!QFile::exists(backupPath)) {
    return;
  }

  migrateUpdateChannel(QSettings(backupPath, QSettings::IniFormat).value(kLegacyUpdateTrackKey).toString());
}

} // namespace

bool migrateIfNeeded()
{
  const auto stored = storedSchemaVersion();
  if (stored >= kCurrentSchemaVersion) {
    return false;
  }

  // A machine that never migrated reads the legacy store, and so does one whose migration read
  // the wrong domain, since that one found nothing to clear. A machine that migrated before may be
  // missing its server configuration or its update channel, and gets them back from that
  // migration's backup. The channel alone does not raise the notice again: getting it back leaves
  // nothing different for the customer to look for.
  bool ran = false;
  if (stored <= kSchemaWrongMacDomain) {
    ran = runLegacyMigration();
  }
  if (stored > 0 && stored <= kSchemaWithoutServerConfig) {
    ran = recoverServerConfig() || ran;
  }
  if (stored > 0 && stored <= kSchemaWithoutUpdateChannel) {
    recoverUpdateChannel();
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

void migrateTlsFiles()
{
  if (s_legacyTlsDir.isEmpty()) {
    return;
  }

  const QDir legacyDir(s_legacyTlsDir);
  migrateCertificate(legacyDir.filePath(kLegacyCertificateFile));
  replaceSmallCertificate();
  migrateTrustedFingerprints(legacyDir.filePath(kLegacyTrustedServersFile), Settings::tlsTrustedServersDb());
  migrateTrustedFingerprints(legacyDir.filePath(kLegacyTrustedClientsFile), Settings::tlsTrustedClientsDb());
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
