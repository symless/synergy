/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2015 - 2026 Synergy App Ltd
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
#include "synergy/gui/AppTime.h"
#include "synergy/gui/ExtraSettings.h"
#include "synergy/gui/license/LicenseApiClient.h"
#include "synergy/license/License.h"
#include "synergy/license/Product.h"

#include <QTimer>

class QMainWindow;
class QDialog;

namespace deskflow::gui {
class CoreProcess;
}

/**
 * @brief A convenience wrapper for `License` that provides Qt signals, etc.
 */
class LicenseHandler : public QObject
{
  Q_OBJECT

  using License = synergy::license::License;
  using SerialKey = synergy::license::SerialKey;

public:
  enum class SetSerialKeyResult
  {
    kSuccess,
    kFatal,
    kUnchanged,
    kInvalid,
    kExpired
  };

  explicit LicenseHandler();

  static LicenseHandler &instance()
  {
    static LicenseHandler instance;
    return instance;
  }

  void handleMainWindow(QMainWindow *mainWindow, deskflow::gui::CoreProcess *coreProcess);
  bool handleAppStart();
  void handleSettings(QDialog *parent) const;
  void handleAbout(QDialog *parent) const;
  void handleVersionCheck(QString &versionUrl);
  bool handleCoreStart();
  bool loadSettings();
  void saveSettings();
  const License &license() const;
  Product::Edition productEdition() const;
  QString productName() const;
  SetSerialKeyResult setLicense(const QString &hexString, bool allowExpired = false);
  void clampFeatures();
  void disable();

  /// @brief Challenge code for offline activation, formatted for display.
  QString offlineActivationChallenge() const;

  /// @brief Verifies an offline activation response code and persists it if valid.
  /// @return False if the response does not verify for this machine and serial key.
  bool applyOfflineActivationResponse(const QString &responseCode);

  bool isEnabled() const
  {
    return m_enabled;
  }

private:
  void updateWindowTitle() const;
  bool showSerialKeyDialog();
  bool showOfflineActivationDialog();
  bool isOfflineActivated() const;
  bool check();
  void runRemoteCheck();
  void handleLicenseVerified();
  void handleLicenseUnverified(const QString &message);
  void handleActivationDeactivated(synergy::gui::license::LicenseApiClient::ActivationIntent intent, const QString &message);
  void handleCheckDeactivated(synergy::gui::license::LicenseApiClient::CheckIntent intent, const QString &message);
  void askServerQuestion();
  bool isInGracePeriod() const;
  bool isGracePeriodExpired() const;
  void resetGracePeriod();
  Settings::CoreMode liveCoreMode() const;
  void disableLicenseAfterGrace(const QString &reason);
  synergy::gui::license::LicenseApiClient::Data buildApiData() const;

  bool m_enabled = true;
  synergy::gui::AppTime m_time;
  License m_license = License::invalid();
  synergy::gui::ExtraSettings m_settings;
  synergy::gui::license::LicenseApiClient m_apiClient;
  bool m_warnedAboutGrace = false;
  qint64 m_lastCoreStartMs = 0;
  QTimer m_remoteCheckTimer;
  QMainWindow *m_pMainWindow = nullptr;
  deskflow::gui::CoreProcess *m_pCoreProcess = nullptr;
};
