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

#include "SettingsScope.h"

#include "common/Constants.h"
#include "common/Settings.h"

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

namespace synergy::gui {

namespace {

const auto kPreferSystemKey = QStringLiteral("scope/preferSystem");

QString extraFile()
{
  return QStringLiteral("%1/%2.extra.conf").arg(Settings::UserDir, kAppName);
}

} // namespace

bool SettingsScope::preferSystem()
{
  QSettings extra(extraFile(), QSettings::IniFormat);
  return extra.value(kPreferSystemKey, false).toBool();
}

void SettingsScope::setPreferSystem(bool prefer)
{
  QSettings extra(extraFile(), QSettings::IniFormat);
  extra.setValue(kPreferSystemKey, prefer);
  extra.sync();
}

namespace {

bool isPathWritable(const QString &path)
{
  const QFileInfo info(path);
  if (info.exists()) {
    return info.isWritable();
  }
  QFileInfo dir(QFileInfo(path).absolutePath());
  if (dir.exists()) {
    return dir.isWritable();
  }
  QDir cursor = dir.dir();
  while (!cursor.exists() && cursor.cdUp()) {
  }
  return QFileInfo(cursor.absolutePath()).isWritable();
}

} // namespace

bool SettingsScope::isSystemWritable()
{
  // The settings file is not all this scope writes: TLS material lands beside it, and a packaged
  // install can leave that directory owned by root while the settings directory is writable. A
  // scope that can hold the settings but not the fingerprint database looks healthy until a client
  // connects, and the core is then stopped for a failure the user cannot act on.
  return isPathWritable(Settings::SystemSettingFile) &&
         isPathWritable(QStringLiteral("%1/tls").arg(Settings::SystemDir));
}

bool SettingsScope::switchTo(QDialog *parent, bool toSystem)
{
  if (toSystem && !isSystemWritable()) {
    QMessageBox::warning(
        parent, QObject::tr("Cannot switch to 'All users' scope"),
        QObject::tr(
            "<p>%1 can't write to <code>%2</code>.</p>"
            "<p>The 'All users' scope requires administrator privileges. "
            "Run %1 as administrator/root to change settings here.</p>"
        )
            .arg(QApplication::applicationName(), Settings::SystemSettingFile)
    );
    return false;
  }

  Settings::save();

  const auto destFile = toSystem ? Settings::SystemSettingFile : Settings::UserSettingFile;
  const auto sourceFile = toSystem ? Settings::UserSettingFile : Settings::SystemSettingFile;

  QSettings dest(destFile, QSettings::IniFormat);
  if (dest.allKeys().isEmpty()) {
    qDebug() << "destination settings empty, copying from:" << sourceFile;
    QSettings source(sourceFile, QSettings::IniFormat);
    for (const auto &key : source.allKeys()) {
      dest.setValue(key, source.value(key));
    }
    dest.sync();
  }

  setPreferSystem(toSystem);

  const auto choice = QMessageBox::information(
      parent, QObject::tr("Restart required"),
      QObject::tr(
          "<p>Settings scope changes take effect when %1 restarts.</p>"
          "<p>Quit %1 now? You'll need to launch it again from your applications menu.</p>"
      )
          .arg(QApplication::applicationName()),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
  );
  if (choice == QMessageBox::Yes) {
    QApplication::quit();
  }
  return true;
}

} // namespace synergy::gui
