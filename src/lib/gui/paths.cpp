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

#include "paths.h"

#include "constants.h"
#include "messages.h"

namespace deskflow::gui::paths {

bool persistUserConfigDir()
{
  const auto dir = userConfigDir();
  const auto dirPath = dir.absolutePath();
  if (!QDir().mkpath(dirPath)) {
    deskflow::gui::messages::showPermissionError(
        nullptr, QString("create user config directory: <code>%1</code>").arg(dirPath)
    );
    return false;
  }
  return true;
}

bool persistSystemConfigDir()
{
  const auto dir = systemConfigDir();
  const auto dirPath = dir.absolutePath();
  if (!QDir().mkpath(dirPath)) {
    deskflow::gui::messages::showPermissionError(
        nullptr, QString("create system config directory: <code>%1</code>").arg(dirPath)
    );
    return false;
  }
  return true;
}

QDir userConfigDir()
{
  QSettings settings(
      QSettings::IniFormat, QSettings::UserScope, //
      QCoreApplication::organizationName(), QCoreApplication::applicationName()
  );

  return QDir(QFileInfo(settings.fileName()).absolutePath());
}

QDir systemConfigDir()
{
  QSettings settings(
      QSettings::IniFormat, QSettings::SystemScope, //
      QCoreApplication::organizationName(), QCoreApplication::applicationName()
  );

  return QDir(QFileInfo(settings.fileName()).absolutePath());
}

QString tlsFilePath(const QString customPath, const bool isSystemScope)
{
  if (!customPath.isEmpty()) {
    return customPath;
  }

  QDir dir;
  if (isSystemScope) {
    dir = systemConfigDir();
  } else {
    dir = userConfigDir();
  }

  return dir.absoluteFilePath(kCertificateFilename);
}

} // namespace deskflow::gui::paths
