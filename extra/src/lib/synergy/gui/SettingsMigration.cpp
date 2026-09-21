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

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QStringLiteral>

#include <optional>

namespace synergy::gui::migration {

namespace {

const auto kSchemaKey = QStringLiteral("migration/schemaVersion");
const auto kNotifiedKey = QStringLiteral("migration/notifiedFor");
const auto kLegacySystemScopeKey = QStringLiteral("systemScope");

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
  return s_migrationRanThisLaunch;
}

void showNoticeIfPending(QWidget *parent)
{
  if (notifiedSchemaVersion() >= kCurrentSchemaVersion) {
    return;
  }
  if (!s_migrationRanThisLaunch) {
    writeNotifiedVersion(kCurrentSchemaVersion);
    return;
  }

  QMessageBox::information(
      parent, QObject::tr("Settings updated"),
      QObject::tr("<p>We've migrated your settings to a new format used by this version of Synergy.</p>"
                  "<p>Your previous settings have been backed up to:</p>"
                  "<p><code>%1</code></p>"
                  "<p>If anything looks different, please contact us.</p>")
          .arg(s_lastBackupPath)
  );
  writeNotifiedVersion(kCurrentSchemaVersion);
}

} // namespace synergy::gui::migration
