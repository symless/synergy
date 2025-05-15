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

namespace deskflow::gui::paths {

QDir userConfigDir(const bool persist)
{
  QSettings settings(
      QSettings::IniFormat, QSettings::UserScope, //
      QCoreApplication::organizationName(), QCoreApplication::applicationName()
  );

  const auto dir = QDir(QFileInfo(settings.fileName()).absolutePath());
  if (persist) {
    const auto dirPath = dir.absolutePath();
    if (!QDir().mkpath(dirPath)) {
      qFatal("failed to persist user config dir: %s", qUtf8Printable(dirPath));
    }
  }
  return dir;
}

QDir systemConfigDir(const bool persist)
{
  QSettings settings(
      QSettings::IniFormat, QSettings::SystemScope, //
      QCoreApplication::organizationName(), QCoreApplication::applicationName()
  );

  const auto dir = QDir(QFileInfo(settings.fileName()).absolutePath());
  if (persist) {
    const auto dirPath = dir.absolutePath();
    if (!QDir().mkpath(dirPath)) {
      qFatal("failed to persist system config dir: %s", qUtf8Printable(dirPath));
    }
  }
  return dir;
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
