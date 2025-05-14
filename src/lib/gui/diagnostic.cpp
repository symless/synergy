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

void clearSettings(QWidget *parent, Settings &settings, bool enableRestart)
{
  QStringList problems;

  qInfo("clearing user settings");
  auto &userSettings = settings.getUserSettings();
  if (userSettings.isWritable()) {
    userSettings.clear();
    userSettings.sync();
  } else {
    problems << "Cannot clear user settings, not writable.";
  }

  qInfo("clearing system settings");
  auto &systemSettings = settings.getSystemSettings();
  if (systemSettings.isWritable()) {
    systemSettings.clear();
    systemSettings.sync();
  } else {
    // Normally on some OS (e.g. Unix-like or Windows running as non-admin),
    // the system settings are not writable. So this is not an error case.
    problems << "Cannot clear system settings, not writable.";
  }

  // Used to store Core config files when in user scope.
  auto userConfigDir = paths::userConfigDir();
  if (userConfigDir.exists()) {
    qInfo().noquote() << "removing user config dir:" << userConfigDir.absolutePath();
    if (!userConfigDir.removeRecursively()) {
      problems << "Failed to remove user config dir.";
    }
  }

  // Used to store Core config files when in system scope.
  // Gotcha: Sometimes Windows doesn't really delete files even though they are "permanently deleted",
  // this is because NTFS may retain a copy via journaling or delayed write-backs. This even
  // persists across reboots. So the only way to truly delete the file is to delete the directory,
  // but unfortunately this isn't always possible due to permissions, so this is a best effort.
  auto systemConfigDir = paths::systemConfigDir();
  if (systemConfigDir.exists()) {
    qInfo().noquote() << "removing system config dir:" << systemConfigDir.absolutePath();
    if (!systemConfigDir.removeRecursively()) {
      problems << "Failed to remove system config dir.";
    }
  }

  if (!problems.isEmpty()) {
    const auto result = QMessageBox::warning(
        parent, "Clear settings",
        QString(
            "%1 occurred while clearing settings:\n\n"
            "%2\n\n"
            "Would you like to restart the application anyway?"
        )
            .arg(problems.length() > 1 ? "Problems" : "A problem")
            .arg(problems.join("\n")),
        QMessageBox::Yes | QMessageBox::No
    );
    if (result == QMessageBox::No) {
      qDebug("user chose not to restart");
      return;
    }
  } else {
    QMessageBox::information(
        parent, "Clear settings",
        "Settings cleared successfully.\n\n"
        "The application will now restart."
    );
  }

  if (enableRestart) {
    qInfo("restarting");
    restart();
  } else {
    qDebug("skipping restart");
  }
}

} // namespace deskflow::gui::diagnostic
