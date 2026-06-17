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

#include "LicenseHandler.h"

#include "ActivationDialog.h"
#include "OfflineActivationDialog.h"
#include "common/Settings.h"
#include "common/VersionInfo.h"
#include "gui/core/CoreProcess.h"
#include "synergy/gui/TestSettings.h"
#include "synergy/gui/constants.h"
#include "synergy/gui/dev_mode.h"
#include "synergy/gui/license/license_utils.h"
#include "synergy/license/OfflineActivation.h"
#include "synergy/license/Product.h"

#include <QAction>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QHBoxLayout>
#include <QHostInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QObject>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>
#include <QtCore>
#include <chrono>

using namespace std::chrono;
using namespace synergy::gui::license;
using namespace synergy::gui;
using namespace deskflow::gui;

using ActivationIntent = LicenseApiClient::ActivationIntent;
using CheckIntent = LicenseApiClient::CheckIntent;

namespace {

std::string licenseMachineId()
{
  const auto testId = TestSettings::instance().machineId();
  return testId.isEmpty() ? QSysInfo::machineUniqueId().toStdString() : testId.toStdString();
}

QByteArray anonymousSignature(const QByteArray &value)
{
  return QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex();
}

} // namespace
using License = synergy::license::License;

LicenseHandler::LicenseHandler()
{
  m_enabled = synergy::gui::license::isActivationEnabled();

  connect(
      &m_apiClient, &LicenseApiClient::activationFailed, this,
      [this](ActivationIntent intent, const QString &message) {
        handleLicenseUnverified(message);

        if (intent == ActivationIntent::kKeyEntry) {
          qWarning("activation failed at key entry, showing serial key dialog");
          showSerialKeyDialog();
        }
      }
  );

  connect(&m_apiClient, &LicenseApiClient::activationSucceeded, this, [this](ActivationIntent intent) {
    qDebug("license activation succeeded, saving settings");
    const bool holdsServerActivation =
        m_license.productEdition() == Product::Edition::kBusiness && liveCoreMode() == Settings::Server;
    m_settings.setActivated(true);
    m_settings.setHoldsServerActivation(holdsServerActivation);
    resetGracePeriod();

    if (m_pCoreProcess == nullptr) {
      qCritical("core process not set");
      return;
    }

    // Key entry activations must never start the core; the customer chose nothing yet.
    if (intent == ActivationIntent::kCoreStart &&
        m_pCoreProcess->processState() == deskflow::core::ProcessState::Stopped) {
      qDebug("resuming core process after activation");
      m_pCoreProcess->start();
    }
  });

  connect(&m_apiClient, &LicenseApiClient::activationUnreachable, this, [this](ActivationIntent intent) {
    if (isGracePeriodExpired()) {
      qWarning("license activation server unreachable and grace period expired, not starting core");
      return;
    }

    qWarning("license activation server unreachable, continuing without activation");

    if (intent == ActivationIntent::kCoreStart && m_pCoreProcess != nullptr &&
        m_pCoreProcess->processState() == deskflow::core::ProcessState::Stopped) {
      m_pCoreProcess->start();
    }
  });

  connect(&m_apiClient, &LicenseApiClient::activationDeactivated, this, &LicenseHandler::handleActivationDeactivated);
  connect(&m_apiClient, &LicenseApiClient::checkSucceeded, this, &LicenseHandler::handleLicenseVerified);
  connect(&m_apiClient, &LicenseApiClient::checkFailed, this, &LicenseHandler::handleLicenseUnverified);
  connect(&m_apiClient, &LicenseApiClient::checkDeactivated, this, &LicenseHandler::handleCheckDeactivated);

  connect(&m_apiClient, &LicenseApiClient::checkNotActivated, this, [this] {
    // The server has no record of this machine, so a read-only check cannot heal it; claim now.
    qInfo("license check found no activation for this machine, activating");
    m_apiClient.activate(buildApiData(), ActivationIntent::kCoreStart);
  });

  connect(&m_apiClient, &LicenseApiClient::validateSucceeded, this, [] { qInfo("business license key validated"); });

  connect(&m_apiClient, &LicenseApiClient::validateFailed, this, [this](const QString &message) {
    qWarning().noquote() << "business license key validation failed:" << message;
    if (m_pMainWindow != nullptr) {
      QMessageBox::warning(m_pMainWindow, "License check failed", message);
    }
    showSerialKeyDialog();
  });

  connect(&m_apiClient, &LicenseApiClient::validateDeactivated, this, [](const QString &message) {
    // The role is unknown at key entry; the next core start resolves any server contention.
    qInfo().noquote() << "validation found another server, deferring to core start:" << message;
  });
}

void LicenseHandler::handleMainWindow(QMainWindow *mainWindow, deskflow::gui::CoreProcess *coreProcess)
{
  // Must still be set as these are used when not enabled.
  m_pMainWindow = mainWindow;
  m_pCoreProcess = coreProcess;

  if (!m_enabled) {
    qDebug("license handler disabled, skipping main window handler");
    return;
  }

  qDebug("main window create handled");

  if (!loadSettings()) {
    qCritical("failed to load license settings");
  }
}

bool LicenseHandler::handleAppStart()
{
  if (m_pMainWindow == nullptr) {
    qCritical("main window not set");
    return false;
  }

  if (!m_enabled) {
    qDebug("license handler disabled, skipping start handler");
    return true;
  }

  updateWindowTitle();

  const auto serialKeyAction = new QAction("Change serial key", m_pMainWindow);
  QObject::connect(serialKeyAction, &QAction::triggered, [this] { showSerialKeyDialog(); });

  const auto licenseMenu = new QMenu("License");
  licenseMenu->addAction(serialKeyAction);
  m_pMainWindow->menuBar()->addAction(licenseMenu->menuAction());

  const auto checkResult = check();
  if (!checkResult) {
    return false;
  }

  qDebug("license is valid, continuing with start");
  updateWindowTitle();
  clampFeatures();
  runRemoteCheck();

  // A long-running session must still honor a disabled (e.g. unpaid) license within a day, so
  // re-check on a cadence; runRemoteCheck self-skips personal and offline licenses.
  m_remoteCheckTimer.setInterval(kRemoteCheckInterval);
  connect(&m_remoteCheckTimer, &QTimer::timeout, this, &LicenseHandler::runRemoteCheck);
  m_remoteCheckTimer.start();
  return true;
}

void LicenseHandler::handleSettings(QDialog *parent) const
{
  Q_UNUSED(parent);

  // Settings UI injection (TLS toggle, scope radios, etc.) lives in extra/.
  // To be wired when the synergy widget-injection layer lands; until then,
  // license-tier clamping happens in clampFeatures() at app start and on
  // license change.
}

void LicenseHandler::handleAbout(QDialog *parent) const
{
  if (!m_enabled || parent == nullptr) {
    return;
  }

  if (!license().serialKey().isValid) {
    return;
  }

  auto *mainLayout = qobject_cast<QBoxLayout *>(parent->layout());
  auto *anchor = parent->findChild<QWidget *>(QStringLiteral("frameLogo"));
  const int index = (mainLayout && anchor) ? mainLayout->indexOf(anchor) : -1;
  if (index < 0) {
    qWarning("about: no frameLogo anchor, skipping license info");
    return;
  }

  auto *section = new QVBoxLayout();
  section->setContentsMargins(0, 0, 0, 8);

  auto *registrantLabel = new QLabel(parent);
  auto *keyField = new QLineEdit(parent);
  keyField->setReadOnly(true);
  auto *changeButton = new QPushButton(QObject::tr("Change"), parent);

  section->addWidget(registrantLabel);
  auto *keyRow = new QHBoxLayout();
  keyRow->addWidget(keyField, 1);
  keyRow->addWidget(changeButton);
  section->addLayout(keyRow);

  // The serial key can change while the About dialog is open (via the Change
  // button), so refresh the widgets from the current license each time rather
  // than building them once with whatever key was active at construction.
  const auto refresh = [registrantLabel, keyField] {
    const auto &k = LicenseHandler::instance().license().serialKey();
    const auto name = QString::fromStdString(k.name);
    auto company = QString::fromStdString(k.company);
    if (!company.isEmpty()) {
      const auto seats = k.seats == 1 ? QObject::tr("1 seat") : QObject::tr("%1 seats").arg(k.seats);
      company = QStringLiteral("%1 (%2)").arg(company, seats);
    }
    QString registrant = name;
    if (!company.isEmpty()) {
      registrant = name.isEmpty() ? company : QStringLiteral("%1, %2").arg(name, company);
    }
    registrantLabel->setText(QObject::tr("Registered to %1").arg(registrant));
    registrantLabel->setVisible(!registrant.isEmpty());
    keyField->setText(QString::fromStdString(k.toString()));
    keyField->setCursorPosition(0);
  };
  refresh();

  QObject::connect(changeButton, &QPushButton::clicked, changeButton, [refresh] {
    LicenseHandler::instance().showSerialKeyDialog();
    refresh();
  });

  mainLayout->insertLayout(index + 1, section);
}

void LicenseHandler::handleVersionCheck(QString &versionUrl)
{
  if (!m_enabled) {
    qDebug("license handler disabled, skipping version check handler");
    return;
  }

  const auto edition = license().productEdition();
  if (edition == Product::Edition::kBusiness) {
    versionUrl.append("/business");
  } else {
    versionUrl.append("/personal");
  }
}

bool LicenseHandler::handleCoreStart()
{
  // HACK: For some reason, the core start trigger gets called twice when clicking the 'start'
  // button. Absorb the duplicate by time rather than by network state, or an unanswered
  // license request blocks core starts entirely.
  const auto now = QDateTime::currentMSecsSinceEpoch();
  if (now - m_lastCoreStartMs < 500) {
    qDebug("duplicate core start trigger, skipping");
    return false;
  }
  m_lastCoreStartMs = now;

  if (!m_enabled) {
    qDebug("license handler disabled, skipping core start handler");
    return true;
  }

  if (m_pMainWindow == nullptr) {
    qCritical("main window not set");
    return false;
  }

  if (m_pCoreProcess == nullptr) {
    qCritical("core process not set");
    return false;
  }

  // The role is only reliably known at core start; start optimistically, a deactivated verdict stops the core.
  if (m_settings.activated() && !m_license.serialKey().isOffline) {
    // A personal license activates once and stays activated; refreshing here would be the
    // kind of post-activation phone-home that breaks offline networks.
    if (m_license.productEdition() != Product::Edition::kBusiness) {
      qDebug("license is activated, starting core for personal license");
      return true;
    }
    if (m_apiClient.isBusy()) {
      qInfo("license api busy, starting core without refreshing activation");
      return true;
    }

    // A routine restart must not re-claim the slot, or the takeover ledger would inflate and a
    // displaced server would silently reclaim instead of asking. So a machine that already holds
    // its slot (and a plain client) only checks; a machine taking on the server role claims it.
    const bool isServer = liveCoreMode() == Settings::Server;
    if (isServer != m_settings.holdsServerActivation()) {
      if (isServer) {
        qDebug("core server, claiming server activation slot on core start");
      } else {
        qDebug("core client, releasing server activation slot on core start");
      }
      m_apiClient.activate(buildApiData(), ActivationIntent::kCoreStart);
    } else {
      qDebug("license is activated, checking on core start");
      m_apiClient.check(buildApiData(), CheckIntent::kCoreStart);
    }
    return true;
  }

  if (m_license.serialKey().isOffline) {
    if (liveCoreMode() != Settings::Server) {
      qDebug("offline license in client mode, starting core without activation");
      return true;
    }

    if (isOfflineActivated()) {
      qDebug("offline activation verified, starting core");
      return true;
    }
    qInfo("offline serial key not activated as server, showing offline activation dialog");
    return showOfflineActivationDialog();
  }

  if (!m_license.isValid()) {
    qWarning("no valid license, skipping core start");
    return false;
  }

  if (m_apiClient.isBusy()) {
    qWarning("license api busy, cannot activate for core start, try again shortly");
    return false;
  }

  qInfo("activating license");
  m_apiClient.activate(buildApiData(), ActivationIntent::kCoreStart);

  return false;
}

bool LicenseHandler::loadSettings()
{
  using enum SetSerialKeyResult;

  m_settings.load();

  const auto serialKey = m_settings.serialKey();
  if (!serialKey.isEmpty()) {
    const auto result = setLicense(m_settings.serialKey(), true);
    if (result != kSuccess && result != kUnchanged) {
      qWarning("set serial key failed, showing activation dialog");
      return showSerialKeyDialog();
    }
  }

  return true;
}

void LicenseHandler::saveSettings()
{
  const auto hexString = m_license.serialKey().hexString;
  m_settings.setSerialKey(QString::fromStdString(hexString));
  m_settings.sync();
}

bool LicenseHandler::showSerialKeyDialog()
{
  if (!m_settings.isWritable()) {
    QMessageBox::warning(
        m_pMainWindow, "Write access required",
        tr("<p>The settings file is not writable:</p>"
           "<p><code>%1</code></p>"
           "<p>Please check the file permissions and try again.</p>")
            .arg(m_settings.fileName())
    );
    return false;
  }

  ActivationDialog dialog(m_pMainWindow, *this);
  const auto result = dialog.exec();
  if (result != QDialog::Accepted) {
    qWarning("license serial key dialog declined");
    return false;
  }

  if (dialog.serialKeyChanged()) {
    // Reset activation so new serial key can be activated.
    qDebug("serial key changed, updating settings");
    m_settings.setActivated(false);
    m_settings.setHoldsServerActivation(false);
    m_settings.setGraceStartEpochSecs(0);
    m_settings.setOfflineActivationResponse({});
    m_warnedAboutGrace = false;
    m_settings.sync();
  }

  saveSettings();
  updateWindowTitle();
  clampFeatures();

  // Key entry never touches the core process; everything a new key implies (activation, the
  // offline challenge, feature clamps) is applied at the next core start.
  // If the user accepted the dialog while not activated (e.g. recovering from a
  // remote disable), reach out so something visible happens regardless of whether the
  // serial key changed.
  if (!m_settings.activated() && m_license.isValid() && !m_license.serialKey().isOffline && !m_apiClient.isBusy()) {
    // The role is unknown at key entry, so business only validates the key here; its first
    // activation happens at core start once the role is known. Personal has no role, so it
    // activates immediately.
    if (m_license.productEdition() == Product::Edition::kBusiness) {
      qInfo("validating business license after dialog accept");
      m_apiClient.validate(buildApiData());
    } else {
      qInfo("retrying activation after dialog accept");
      m_apiClient.activate(buildApiData(), ActivationIntent::kKeyEntry);
    }
  }

  qDebug("license serial key dialog accepted");
  return true;
}

bool LicenseHandler::showOfflineActivationDialog()
{
  if (!m_settings.isWritable()) {
    QMessageBox::warning(
        m_pMainWindow, "Write access required",
        tr("<p>The settings file is not writable:</p>"
           "<p><code>%1</code></p>"
           "<p>Please check the file permissions and try again.</p>")
            .arg(m_settings.fileName())
    );
    return false;
  }

  if (offlineActivationChallenge().isEmpty()) {
    QMessageBox::critical(
        m_pMainWindow, tr("Offline activation"),
        tr("<p>A unique ID for this computer could not be determined, "
           "so an activation code cannot be generated.</p>"
           R"(<p>Please <a href="%1">contact us</a> for help.</p>)")
            .arg(kUrlContact)
    );
    return false;
  }

  OfflineActivationDialog dialog(m_pMainWindow, *this);
  return dialog.exec() == QDialog::Accepted;
}

QString LicenseHandler::offlineActivationChallenge() const
{
  const auto machineId = licenseMachineId();
  const auto challenge = synergy::license::buildOfflineChallenge(machineId, m_license.serialKey().hexString);
  return QString::fromStdString(synergy::license::formatOfflineCode(challenge, 4));
}

bool LicenseHandler::isOfflineActivated() const
{
  const auto response = m_settings.offlineActivationResponse();
  if (response.isEmpty()) {
    return false;
  }
  const auto machineId = licenseMachineId();
  return synergy::license::verifyOfflineResponse(machineId, m_license.serialKey().hexString, response.toStdString());
}

bool LicenseHandler::applyOfflineActivationResponse(const QString &responseCode)
{
  const auto response = responseCode.simplified();
  const auto machineId = licenseMachineId();
  if (!synergy::license::verifyOfflineResponse(machineId, m_license.serialKey().hexString, response.toStdString())) {
    qWarning("offline activation response not valid for this machine");
    return false;
  }

  qInfo("offline activation response verified");
  m_settings.setOfflineActivationResponse(response);
  m_settings.sync();
  return true;
}

void LicenseHandler::updateWindowTitle() const
{
  const auto productName = QString::fromStdString(m_license.productName());
  qDebug("updating main window title: %s", qPrintable(productName));
  const bool showVersion = Settings::value(Settings::Gui::ShowVersionInTitle).toBool();
  m_pMainWindow->setWindowTitle(synergy::gui::windowTitle(productName, showVersion));
}

const synergy::license::License &LicenseHandler::license() const
{
  return m_license;
}

Product::Edition LicenseHandler::productEdition() const
{
  return m_license.productEdition();
}

QString LicenseHandler::productName() const
{
  return QString::fromStdString(m_license.productName());
}

/// @param allowExpired If true, allow expired licenses to be set.
///     Useful for passing an expired license to the activation dialog.
LicenseHandler::SetSerialKeyResult LicenseHandler::setLicense(const QString &hexString, bool allowExpired)
{
  using enum LicenseHandler::SetSerialKeyResult;

  if (hexString.isEmpty()) {
    qCritical("serial key is empty");
    return kInvalid;
  }

  qDebug() << "changing serial key to:" << hexString;
  auto serialKey = parseSerialKey(hexString);

  if (!serialKey.isValid) {
    qWarning() << "invalid serial key, ignoring:" << hexString;
    return kInvalid;
  }

  auto license = License(serialKey);
  if (m_time.hasTestTime()) {
    license.setNowFunc([this]() { return m_time.now(); });
  }

  if (!allowExpired && license.isExpired()) {
    qDebug("license is expired, ignoring");
    return kExpired;
  }

  const auto oldSerialKey = m_license.serialKey();
  m_license = license;

  // This delayed check logic seems really complex. Is it really worth the maintenance and testing cost?
  // Condition must run *after* the license member is set, since it's async callback uses this member.
  if (!m_license.isExpired() && m_license.isTimeLimited()) {
    auto secondsLeft = m_license.secondsLeft();
    if (secondsLeft.count() < INT_MAX) {
      const auto validateAt = secondsLeft + seconds{1};
      const auto interval = duration_cast<milliseconds>(validateAt);
      QTimer::singleShot(interval, this, &LicenseHandler::check);
    } else {
      qDebug("license expiry too distant to schedule timer");
    }
  }

  if (serialKey == oldSerialKey) {
    qDebug("serial key did not change, ignoring");
    return kUnchanged;
  }

  return kSuccess;
}

bool LicenseHandler::check()
{
  if (!m_license.isValid()) {
    qDebug("license validation failed, license invalid");
    return showSerialKeyDialog();
  } else if (m_license.isExpired()) {
    qDebug("license validation failed, license expired");
    return showSerialKeyDialog();
  } else if (m_license.isExpiringSoon()) {
    if (isInGracePeriod()) {
      qDebug("license expiring soon but in remote grace period, suppressing renew nag");
      return true;
    }
    qDebug("license is expiring soon, showing serial key dialog");
    showSerialKeyDialog();
    // Return true even if dialog cancelled, since expiring soon licenses are still valid.
    return true;
  } else {
    qDebug("license validation succeeded");
    return true;
  }
}

void LicenseHandler::clampFeatures()
{
  if (Settings::value(Settings::Security::TlsEnabled).toBool() && !m_license.isTlsAvailable()) {
    qWarning("tls not available, disabling tls");
    Settings::setValue(Settings::Security::TlsEnabled, false);
  }

  const auto isSystemScope = (Settings::settingsFile() == Settings::SystemSettingFile);
  if (isSystemScope && !m_license.isSettingsScopeAvailable()) {
    qCritical("settings scope not available");
    return;
  }

  qDebug("committing default feature settings");
  Settings::save();
}

void LicenseHandler::disable()
{
  qDebug("disabling license handler");
  m_enabled = false;
}

bool LicenseHandler::isInGracePeriod() const
{
  return m_settings.graceStartEpochSecs() > 0;
}

bool LicenseHandler::isGracePeriodExpired() const
{
  if (!isInGracePeriod()) {
    return false;
  }
  const auto now = QDateTime::currentSecsSinceEpoch();
  const auto elapsed = seconds{now - m_settings.graceStartEpochSecs()};
  return elapsed >= duration_cast<seconds>(kLicenseGracePeriod);
}

void LicenseHandler::resetGracePeriod()
{
  m_settings.setGraceStartEpochSecs(0);
  m_settings.sync();
  m_warnedAboutGrace = false;
}

Settings::CoreMode LicenseHandler::liveCoreMode() const
{
  // The saved mode setting lags the UI at core start.
  if (m_pCoreProcess != nullptr) {
    return m_pCoreProcess->mode();
  }
  return static_cast<Settings::CoreMode>(Settings::value(Settings::Core::CoreMode).toInt());
}

LicenseApiClient::Data LicenseHandler::buildApiData() const
{
  const auto machineSignature = anonymousSignature(QByteArray::fromStdString(licenseMachineId()));
  const auto hostnameSignature = anonymousSignature(QHostInfo::localHostName().toUtf8());

  return {
      machineSignature,
      hostnameSignature,
      QString::fromStdString(m_license.serialKey().hexString),
      kVersion,
      QSysInfo::prettyProductName(),
      liveCoreMode() == Settings::Server
  };
}

void LicenseHandler::runRemoteCheck()
{
  if (!m_license.isValid() || m_license.serialKey().isOffline) {
    qDebug("license invalid or offline, skipping remote check");
    return;
  }

  // Personal licenses activate once and are never re-validated, so they keep working on
  // offline and restricted networks. Only business licenses are re-checked.
  if (m_license.productEdition() != Product::Edition::kBusiness) {
    if (isInGracePeriod()) {
      // Nothing else clears grace once personal checks stop, and a stale grace flag
      // suppresses the renew nag forever.
      qInfo("clearing stale grace period for personal license");
      resetGracePeriod();
    }
    qDebug("personal license, skipping remote check");
    return;
  }

  if (m_apiClient.isBusy()) {
    qDebug("license activator busy, skipping remote check");
    return;
  }

  qInfo("running remote license check");
  m_apiClient.check(buildApiData());
}

void LicenseHandler::handleLicenseVerified()
{
  qInfo("remote license check succeeded");

  const bool wasInGrace = isInGracePeriod();
  resetGracePeriod();

  if (wasInGrace && m_pMainWindow != nullptr) {
    QMessageBox::information(
        m_pMainWindow, "License restored", tr("Your license is valid again. Thanks for your patience.")
    );
  }
}

void LicenseHandler::handleLicenseUnverified(const QString &message)
{
  qWarning().noquote() << "license could not be verified:" << message;

  if (!isInGracePeriod()) {
    m_settings.setGraceStartEpochSecs(QDateTime::currentSecsSinceEpoch());
    m_settings.sync();
  }

  if (isGracePeriodExpired()) {
    disableLicenseAfterGrace(message);
    return;
  }

  if (!m_warnedAboutGrace && m_pMainWindow != nullptr) {
    m_warnedAboutGrace = true;
    const auto now = QDateTime::currentSecsSinceEpoch();
    const auto elapsed = seconds{now - m_settings.graceStartEpochSecs()};
    const auto daysRemaining = ceil<days>(kLicenseGracePeriod - elapsed).count();
    QMessageBox::warning(
        m_pMainWindow, "License check failed",
        tr("<p>We could not verify your license:</p>"
           "<p><i>%1</i></p>"
           "<p>%2 will keep working for %3 days. "
           R"(If the problem persists, please <a href="%4">contact us</a>.)"
           "</p>")
            .arg(message.toHtmlEscaped())
            .arg(productName())
            .arg(daysRemaining)
            .arg(kUrlContact)
    );
  }
}

void LicenseHandler::handleActivationDeactivated(ActivationIntent intent, const QString &message)
{
  qWarning().noquote() << "server activation held by another computer:" << message;

  m_settings.setHoldsServerActivation(false);
  resetGracePeriod();

  // The role is only reliably known at core start; an activation sent from anywhere else
  // (e.g. serial key entry) must not ask or stop anything, the next core start will.
  if (intent != ActivationIntent::kCoreStart) {
    qInfo("not a core start activation, leaving the server question for the next core start");
    return;
  }

  if (m_pCoreProcess != nullptr && m_pCoreProcess->isStarted()) {
    qDebug("stopping core process while the server question is unanswered");
    m_pCoreProcess->stop();
  }

  askServerQuestion();
}

void LicenseHandler::askServerQuestion()
{
  // The license allows one server per seat, but we never block the customer standing at this
  // machine; they are almost always the rightful user (switching desks, replacing a machine).
  // Asking first keeps use of one license fair and deliberate, and the other computer is
  // asked the same question rather than cut off. Switching the old server to client mode
  // releases the server slot, so the normal switching flow never sees this question.
  QString question;
  if (m_license.serialKey().seats > 1) {
    question = tr("<p>All of the server activations for your team's license are currently in use.</p>"
                  "<p>If you need to add more seats to your team's license, please "
                  R"(<a href="%1">contact us</a> today.</p>)"
                  "<p>Do you want to reassign a server activation to this computer?</p>"
                  "<p>This will deactivate the other computer and interrupt an existing setup.</p>")
                   .arg(kUrlContact);
  } else {
    question = tr("<p>Another computer is currently the server for your license.</p>"
                  "<p>If you need more than one server running at the same time, please "
                  R"(<a href="%1">contact us</a> today.</p>)"
                  "<p>Do you want to reassign the server activation to this computer?</p>"
                  "<p>This will deactivate the other computer and interrupt an existing setup.</p>")
                   .arg(kUrlContact);
  }
  const auto reply = QMessageBox::question(m_pMainWindow, "License limit reached", question);
  if (reply == QMessageBox::Yes) {
    qInfo("server question accepted, reactivating");

    // The customer just chose to be the server, so success may resume the stopped core.
    m_apiClient.activate(buildApiData(), ActivationIntent::kCoreStart, true);
    return;
  }

  // Decline changes nothing; writing the mode setting here would desync it from the main
  // window's mode controls and the core process, which only the main window owns.
  qInfo("server question declined, leaving core stopped");
}

void LicenseHandler::handleCheckDeactivated(CheckIntent intent, const QString &message)
{
  qWarning().noquote() << "server activation taken over by another computer:" << message;

  m_settings.setHoldsServerActivation(false);
  resetGracePeriod();

  // The recurring poll only acts on a server that is actually running; a core start is becoming
  // the server right now, so it asks regardless of how far the process has got.
  const bool runningAsServer =
      m_pCoreProcess != nullptr && m_pCoreProcess->isStarted() && liveCoreMode() == Settings::Server;
  if (intent == CheckIntent::kPoll && !runningAsServer) {
    qDebug("not running as server, ignoring deactivated check");
    return;
  }

  if (m_pCoreProcess != nullptr && m_pCoreProcess->isStarted()) {
    qInfo("stopping core, another computer took over as server");
    m_pCoreProcess->stop();
  }

  askServerQuestion();
}

void LicenseHandler::disableLicenseAfterGrace(const QString &reason)
{
  qWarning().noquote() << "license grace period expired, disabling:" << reason;

  if (m_pCoreProcess != nullptr && m_pCoreProcess->isStarted()) {
    qDebug("stopping core process due to disabled license");
    m_pCoreProcess->stop();
  }

  // Keep the serial key + in-memory license so the next activation attempt can succeed
  // automatically if the server re-enables the license (e.g. after the customer pays).
  // Keep the grace clock too, so a restart stays disabled instead of granting a fresh grace.
  m_settings.setActivated(false);
  m_settings.setHoldsServerActivation(false);
  m_settings.sync();
  m_warnedAboutGrace = false;

  if (m_pMainWindow != nullptr) {
    QMessageBox::warning(
        m_pMainWindow, "License disabled",
        tr("<p>Your license has been disabled and could not be verified within the grace period:</p>"
           "<p><i>%1</i></p>"
           R"(<p>Please <a href="%2">contact us</a> to restore access. )"
           "Once your license is reinstated, the app will resume automatically.</p>")
            .arg(reason.toHtmlEscaped())
            .arg(kUrlContact)
    );
  }
}
