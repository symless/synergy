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
    qCritical("user settings are not writable");
  }

  qInfo("clearing system settings");
  auto &systemSettings = settings.getSystemSettings();
  if (systemSettings.isWritable()) {
    systemSettings.clear();
    systemSettings.sync();
  } else {
    // Normally on some OS (e.g. Unix-like or Windows running as non-admin),
    // the system settings are not writable. So this is not an error case.
    qWarning("system settings are not writable");
  }

  // On Windows, the registry is used for user settings, so there is no dir to remove,
  // but removing it on Unix-like systems is still possible.
  QFileInfo userFileInfo(userSettings.fileName());
  QDir userDir(userFileInfo.absoluteDir());
  if (userDir.exists()) {
    qInfo().noquote() << "removing user config dir:" << userDir.absolutePath();
    if (!userDir.removeRecursively()) {
      qCritical("failed to remove user config dir");
    }
  }

  // Sometimes Windows doesn't really delete files even though they are "permanently deleted",
  // this is because NTFS may retain a copy via journaling or delayed write-backs. This even
  // persists across reboots. So the only way to truly delete the file is to delete the directory.
  QFileInfo fileInfo(systemSettings.fileName());
  QDir systemDir(fileInfo.absoluteDir());
  if (systemDir.exists()) {
    qInfo().noquote() << "removing system config dir:" << systemDir.absolutePath();
    if (!systemDir.removeRecursively()) {
      // Should be a warning, since on Unix-like, we won't normally have access to remove dirs
      // from /etc/xdg or wherever, as it's a system dir.
      qWarning("failed to remove system config dir");
    }
  }

  auto configDir = paths::configDir();
  if (configDir.exists()) {
    qInfo().noquote() << "removing config dir:" << configDir.absolutePath();
    if (!configDir.removeRecursively()) {
      qCritical("failed to remove config dir");
    }
  }

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
