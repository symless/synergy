/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2024 - 2026 Synergy App Ltd
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

#include "common/Settings.h"
#include "synergy/build_config.h"
#include "synergy/gui/FeatureHandler.h"
#include "synergy/gui/SettingsMigration.h"
#include "synergy/gui/SettingsScope.h"
#include "synergy/gui/UpdateChannel.h"
#include "synergy/gui/dev_mode.h"
#include "synergy/gui/license/LicenseHandler.h"

#include "synergy/gui/styles.h"

#include <QColor>
#include <QDialog>
#include <QGuiApplication>
#include <QIcon>
#include <QMainWindow>
#include <QPalette>
#include <QSize>

namespace deskflow::gui {
class CoreProcess;
}

namespace synergy::hooks {

// Runs before any Settings::value() call, so that legacy-format keys can be
// migrated to the new format before upstream's cleanSettings() wipes them.
inline void onPreInit()
{
  synergy::gui::migration::migrateIfNeeded();

  // setSettingsFile() instantiates Settings; that's expected here.
  if (synergy::gui::SettingsScope::preferSystem()) {
    if (synergy::gui::SettingsScope::isSystemWritable()) {
      Settings::setSettingsFile(Settings::SystemSettingFile);
    } else {
      qWarning("scope: system-scope no longer writable, falling back to user scope");
      synergy::gui::SettingsScope::setPreferSystem(false);
    }
  }
}

inline void onMainWindow(QMainWindow *mainWindow, deskflow::gui::CoreProcess *coreProcess)
{
  // Qt's default link color is unreadable on the dark theme; setting the palette link role
  // once colors every anchor, so dialog copy never needs inline link styles.
  auto palette = QGuiApplication::palette();
  palette.setColor(QPalette::Link, QColor(kColorSecondary));
  QGuiApplication::setPalette(palette);

  LicenseHandler::instance().handleMainWindow(mainWindow, coreProcess);
  FeatureHandler::instance().handleMainWindow(mainWindow);
  synergy::gui::migration::showNoticeIfPending(mainWindow);
}

inline void onTitleApplied(QMainWindow *mainWindow)
{
  const bool showVersion = Settings::value(Settings::Gui::ShowVersionInTitle).toBool();
  mainWindow->setWindowTitle(synergy::gui::windowTitle(synergy::kDisplayName, showVersion));
}

inline bool onAppStart()
{
  FeatureHandler::instance().handleAppStart();
  return LicenseHandler::instance().handleAppStart();
}

inline void onSettings(QDialog *parent)
{
  LicenseHandler::instance().handleSettings(parent);
  FeatureHandler::instance().handleSettings(parent);
}

inline void onAbout(QDialog *parent)
{
  FeatureHandler::instance().handleAbout(parent);
  LicenseHandler::instance().handleAbout(parent);
}

inline void onVersionCheck(QString &versionUrl)
{
  LicenseHandler::instance().handleVersionCheck(versionUrl);
  synergy::gui::UpdateChannel::applyToVersionCheckUrl(versionUrl);
}

inline bool onCoreStart()
{
  return LicenseHandler::instance().handleCoreStart();
}

inline void onTestStart()
{
  LicenseHandler::instance().disable();
}

/**
 * @brief Build a crisp system-tray icon from a Qt-resource SVG.
 *
 * Synergy shows its colored brand logo in the tray, rendered straight from a
 * resource SVG rather than a themed monochrome icon that GNOME would size
 * itself. A fresh SVG-backed QIcon reports no available sizes, so Qt's
 * StatusNotifierItem backend only hands GNOME 22px and 64px renderings; GNOME
 * upscales the nearest one to its panel slot and the icon looks blurry.
 * Pre-rendering the common tray sizes gives GNOME a near-exact match, so it
 * barely scales the bitmap and the icon stays sharp.
 *
 * @param resourcePath Qt resource path of the SVG to render.
 * @return A multi-size icon suitable for QSystemTrayIcon::setIcon.
 */
inline QIcon trayIcon(const QString &resourcePath)
{
  const QIcon source(resourcePath);
  QIcon icon;
  for (const int size : {16, 22, 24, 32, 48, 64})
    icon.addPixmap(source.pixmap(QSize(size, size)));
  return icon;
}

} // namespace synergy::hooks
