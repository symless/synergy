/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Symless Ltd.
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

#include "diagnostic.h"

#include "config/Settings.h"
#include "paths.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QProcess>

namespace deskflow::gui::diagnostic {

void restart()
{
  QString program = QCoreApplication::applicationFilePath();
  QStringList arguments = QCoreApplication::arguments();

  // prevent infinite reset loop when env var set.
  arguments << "--no-reset";

  qInfo("launching new process: %s", qPrintable(program));
  QProcess::startDetached(program, arguments);

  qDebug("exiting current process");
  QApplication::exit();
}

void clearSettings(Settings &settings, bool enableRestart)
{
  qInfo("clearing user settings");
  auto &userSettings = settings.getUserSettings();
  if (userSettings.isWritable()) {
    userSettings.clear();
    userSettings.sync();
  } else {
    qCritical("cannot clear user settings, not writable");
  }

  qInfo("clearing system settings");
  auto &systemSettings = settings.getSystemSettings();
  if (systemSettings.isWritable()) {
    systemSettings.clear();
    systemSettings.sync();
  } else {
    // Normally on some OS (e.g. Unix-like or Windows running as non-admin),
    // the system settings are not writable. So this is not an error case.
    qWarning("cannot clear system settings, not writable");
  }

  // Tell the usee we're leaving the user config dir behind, so they can delete it manually.
  // On Windows, the registry is used for user settings, so there is no dir to remove,
  // but removing it on Unix-like systems it's also not always possible to remove the dirs;
  // on macOS, all the .plist files are in the same directory, so we can't remove that dir.
  // On Linux it's probably possible to remove the config dir, but we should be consistent
  // across all platforms. So we just leave the user config dir behind.
  QFileInfo userFileInfo(userSettings.fileName());
  QDir userDir(userFileInfo.absoluteDir());
  if (userDir.exists()) {
    qInfo().noquote() << "user config dir:" << userDir.absolutePath();
  }

  // Tell the usee we're leaving the system config dir behind, so they can delete it manually.
  // Sometimes Windows doesn't really delete files even though they are "permanently deleted",
  // this is because NTFS may retain a copy via journaling or delayed write-backs. This even
  // persists across reboots. So the only way to truly delete the file is to delete the directory,
  // but we shouldn't try to do that from the app, since there are too many edge cases.
  QFileInfo fileInfo(systemSettings.fileName());
  QDir systemDir(fileInfo.absoluteDir());
  if (systemDir.exists()) {
    qInfo().noquote() << "system config dir:" << systemDir.absolutePath();
  }

  // Gotcha: Not necessarily the same as the dir that contains the user settings file.
  auto configDir = paths::configDir();
  if (configDir.exists()) {
    qInfo().noquote() << "removing config dir:" << configDir.absolutePath();
    if (!configDir.removeRecursively()) {
      qCritical("failed to remove config dir");
    }
  }

  // Gotcha: Legacy path for TLS, etc.
  auto profileDir = paths::coreProfileDir();
  if (profileDir.exists()) {
    qInfo("removing profile dir: %s", qPrintable(profileDir.absolutePath()));
    if (!profileDir.removeRecursively()) {
      qCritical("failed to remove profile dir");
    }
  }

  // It's important to block the UI thread to show a message so that the user has a chance to read the log output,
  // in case any errors or warnings happened. Ideally, we should be logging to a file instead, but until then,
  // this is probably the best approach.
  QMessageBox::information(
      nullptr, "Settings cleared",
      "<p>Your settings have been cleared.</p>"
      "<p>The application will now restart.</p>"
  );

  if (enableRestart) {
    qInfo("restarting");
    restart();
  } else {
    qDebug("skipping restart");
  }
}

} // namespace deskflow::gui::diagnostic
