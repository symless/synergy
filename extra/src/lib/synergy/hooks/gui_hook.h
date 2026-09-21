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
#include "synergy/gui/LockedSettings.h"
#include "synergy/gui/SettingsMigration.h"
#include "synergy/gui/SettingsScope.h"
#include "synergy/gui/UpdateChannel.h"
#include "synergy/gui/dev_mode.h"
#include "synergy/gui/license/LicenseHandler.h"

#include "synergy/gui/styles.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QColor>
#include <QCoreApplication>
#include <QDialog>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
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

  // After the scope is settled, so the administrator's values land in the
  // settings file the rest of the launch reads.
  synergy::gui::LockedSettings::instance().apply();
}

// The migration notice is shown at the end of onAppStart rather than here, so it cannot compete
// with a dialog raised later in startup; this is how it gets the window to parent itself to.
inline QMainWindow *s_mainWindow = nullptr;

inline void onMainWindow(QMainWindow *mainWindow, deskflow::gui::CoreProcess *coreProcess)
{
  s_mainWindow = mainWindow;

  // Upstream asks, on first launch, whether to check for updates, which Debian packaging policy
  // wants and Synergy does not need. Writing the value before the main window opens means the
  // question never gets asked; anyone who has already answered it, here or in an older release
  // the settings migration carried forward, keeps their answer.
#ifdef SYNERGY_VERSION_CHECK
  if (!Settings::value(Settings::Gui::AutoUpdateCheck).isValid()) {
    Settings::setValue(Settings::Gui::AutoUpdateCheck, true);
  }
#else
  // Overwritten rather than defaulted: a settings file migrated from an edition that had the
  // check would otherwise switch it back on in a build that ships without it.
  Settings::setValue(Settings::Gui::AutoUpdateCheck, false);
#endif

  LicenseHandler::instance().handleMainWindow(mainWindow, coreProcess);
  FeatureHandler::instance().handleMainWindow(mainWindow);
}

inline void onTitleApplied(QMainWindow *mainWindow)
{
  const bool showVersion = Settings::value(Settings::Gui::ShowVersionInTitle).toBool();
  mainWindow->setWindowTitle(synergy::gui::windowTitle(synergy::kDisplayName, showVersion));
}

inline bool onAppStart()
{
  FeatureHandler::instance().handleAppStart();
  if (!LicenseHandler::instance().handleAppStart()) {
    return false;
  }

  // Last, so every other dialog startup can raise has been dealt with by the time it appears.
  synergy::gui::migration::showNoticeIfPending(s_mainWindow);
  return true;
}

inline void onSettings(QDialog *parent)
{
  LicenseHandler::instance().handleSettings(parent);
  FeatureHandler::instance().handleSettings(parent);
  synergy::gui::LockedSettings::instance().applyToDialog(parent);

#ifndef SYNERGY_VERSION_CHECK
  if (auto *const autoUpdate = parent->findChild<QCheckBox *>(QStringLiteral("cbAutoUpdate"))) {
    autoUpdate->hide();
  }
#endif
}

inline void onServerConfig(QDialog *parent)
{
  synergy::gui::LockedSettings::instance().applyToDialog(parent);
}

inline void onAbout(QDialog *parent)
{
  // The dialog names the product with the logo wordmark alone, which carries the brand but not the
  // edition, so every flavor's About dialog would otherwise look identical. Reuses the .ui's own
  // translated title string rather than restating it, so there is only one copy to translate.
  parent->setWindowTitle(QCoreApplication::translate("AboutDialog", "About %1").arg(synergy::kDisplayName));

  FeatureHandler::instance().handleAbout(parent);
  LicenseHandler::instance().handleAbout(parent);

  // After the handlers, so the product name sits directly under the logo and above any license
  // section they inserted at the same anchor.
  auto *const mainLayout = qobject_cast<QBoxLayout *>(parent->layout());
  auto *const anchor = parent->findChild<QWidget *>(QStringLiteral("frameLogo"));
  if (mainLayout == nullptr || anchor == nullptr) {
    qWarning("about: no frameLogo anchor, skipping product name");
    return;
  }

  auto *const productName = new QLabel(QString::fromUtf8(synergy::kDisplayName), parent);
  QFont font = productName->font();
  font.setBold(true);
  productName->setFont(font);
  mainLayout->insertWidget(mainLayout->indexOf(anchor) + 1, productName);
}

inline bool onVersionCheck([[maybe_unused]] QString &versionUrl)
{
#ifndef SYNERGY_VERSION_CHECK
  return false;
#else
  LicenseHandler::instance().handleVersionCheck(versionUrl);
  synergy::gui::UpdateChannel::applyToVersionCheckUrl(versionUrl);
  return true;
#endif
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
