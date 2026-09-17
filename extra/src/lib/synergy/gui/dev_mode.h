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

#pragma once

#include "common/VersionInfo.h"
#include "synergy/build_config.h"

#include <QString>
#include <QStringList>
#include <QStringLiteral>

namespace synergy::gui {

// Extracts the release stage (e.g. "beta") from the composed version string.
// kVersion is "X.Y.Z[-STAGE][-dev|-snapshot][+BUILD]", so the stage is the
// first token after the base semver that isn't a build-type marker, with any
// "+BUILD" metadata (git sha, snapshot number) dropped before the check.
inline QString versionStage()
{
  const QStringList parts = QString::fromLatin1(kVersion).split('-');
  if (parts.size() < 2) {
    return {};
  }
  const QString candidate = parts.at(1).section('+', 0, 0);
  if (candidate == QStringLiteral("dev") || candidate == QStringLiteral("snapshot")) {
    return {};
  }
  return candidate;
}

// Composes the main-window title. The stage / dev-build marker is always part
// of the name so a pre-release binary can't masquerade as stable; showVersion
// (the upstream "Include version in the window title" setting) gates only the
// trailing version-number suffix.
inline QString windowTitle(const QString &productName, bool showVersion)
{
  const QString stage = versionStage();
  const QString prettyStage = stage.isEmpty() ? QString() : stage.left(1).toUpper() + stage.mid(1);

  QString name = productName;
  if constexpr (synergy::kIsDevBuild) {
    name = prettyStage.isEmpty() ? QStringLiteral("%1 (Development Build)").arg(productName)
                                 : QStringLiteral("%1 (%2 Development Build)").arg(productName, prettyStage);
  } else if (!prettyStage.isEmpty()) {
    name = QStringLiteral("%1 %2").arg(productName, prettyStage);
  }

  if (showVersion) {
    return QStringLiteral("%1 - %2").arg(name, kDisplayVersion);
  }
  return name;
}

} // namespace synergy::gui
