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
#include <QScopeGuard>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>
#include <QtCore>
#include <chrono>

using namespace std::chrono;
using namespace synergy::gui::license;
using namespace synergy::gui;
using namespace deskflow::gui;

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

  connect(&m_apiClient, &LicenseApiClient::activationSucceeded, this, &LicenseHandler::handleActivationSucceeded);
  connect(&m_apiClient, &LicenseApiClient::activationFailed, this, &LicenseHandler::handleActivationFailed);
  connect(&m_apiClient, &LicenseApiClient::activationUnreachable, this, &LicenseHandler::handleActivationUnreachable);
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

  reportUsage();
  m_usageTimer.setInterval(kUsageReportInterval);
  connect(&m_usageTimer, &QTimer::timeout, this, &LicenseHandler::reportUsage);
  m_usageTimer.start();
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

  // The dialog runs a nested event loop, and the duplicate trigger above can land inside it.
  if (m_inCoreStart) {
    qDebug("core start already in progress, skipping");
    return false;
  }
  m_inCoreStart = true;
  const auto guard = qScopeGuard([this] { m_inCoreStart = false; });

  // The main window auto-starts the core before the app start hook runs, so on a fresh install
  // this is the first thing that can ask for a key; without asking here the start is silently
  // refused and the key the customer then enters starts nothing.
  if (!m_license.isValid()) {
    qInfo("no valid license, showing serial key dialog before core start");
    if (!showSerialKeyDialog()) {
      return false;
    }
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

  // Activation is per role: once a machine has activated in a role it never phones home again
  // for that role, so a routine restart is silent and only a mode change reaches the server.
  const auto mode = liveActivatedMode();
  if (m_settings.activatedMode() == mode) {
    qDebug("already activated in this mode, starting core");
    return true;
  }

  if (m_apiClient.isBusy()) {
    qWarning("license api busy, cannot activate for core start, try again shortly");
    return false;
  }

  qInfo("activating license as %s", mode == ActivatedMode::kServer ? "server" : "client");
  m_pendingMode = mode;
  m_apiClient.activate(buildApiData());

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
    m_settings.setActivatedMode(ActivatedMode::kNone);
    m_settings.setOfflineActivationResponse({});
    m_settings.sync();
  }

  saveSettings();
  updateWindowTitle();
  clampFeatures();

  // Key entry only parses the key and never phones home; the role is unknown until the core
  // starts, and that is where activation (or the offline challenge) happens.
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
  // The build names the product; the license only refines that name for editions that have one.
  // Without this a keyless build would fall back to the licensed default instead of its own name.
  if (!m_enabled || m_pMainWindow == nullptr) {
    return;
  }

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
    qDebug("license is expiring soon, showing serial key dialog");
    showSerialKeyDialog();

    // A licence that is expiring is still valid, so declining the dialog is not a failure.
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

  // Warned rather than clamped: the settings already live in the system file by this point, and
  // moving the preference without carrying them across would strand the user's configuration.
  // qWarning, because qCritical raises a modal dialog, and this recurs on every launch.
  const auto isSystemScope = (Settings::settingsFile() == Settings::SystemSettingFile);
  if (isSystemScope && !m_license.isSettingsScopeAvailable()) {
    qWarning("settings scope not available for this license");
  }

  qDebug("committing default feature settings");
  Settings::save();
}

void LicenseHandler::disable()
{
  qDebug("disabling license handler");
  m_enabled = false;
}

Settings::CoreMode LicenseHandler::liveCoreMode() const
{
  // The saved mode setting lags the UI at core start.
  if (m_pCoreProcess != nullptr) {
    return m_pCoreProcess->mode();
  }
  return static_cast<Settings::CoreMode>(Settings::value(Settings::Core::CoreMode).toInt());
}

LicenseHandler::ActivatedMode LicenseHandler::liveActivatedMode() const
{
  return liveCoreMode() == Settings::Server ? ActivatedMode::kServer : ActivatedMode::kClient;
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

void LicenseHandler::handleActivationSucceeded()
{
  qDebug("license activation succeeded, saving settings");
  m_settings.setActivatedMode(m_pendingMode);
  m_pendingMode = ActivatedMode::kNone;
  m_settings.sync();
  resumeCore();
}

void LicenseHandler::handleActivationFailed(const QString &message, const QString &reason, const QString &reference)
{
  m_pendingMode = ActivatedMode::kNone;

  if (m_pMainWindow == nullptr) {
    return;
  }

  // The website words the cause, the app adds the way out. Both are fixed from the account page
  // and either can turn out to be the wrong key in hand, so the account link and the serial key
  // button are offered whatever the website says, or fails to say.
  const bool limitReached = reason == QLatin1String("limitReached");

  auto text = QStringLiteral("<p>%1</p>").arg(message.toHtmlEscaped());
  if (limitReached) {
    text +=
        tr(R"(<p>Free an activation on your <a href="%1">account page</a>, then start again.</p>)").arg(kUrlAccount);
  } else {
    text += tr(R"(<p>Check your license and serial key on your <a href="%1">account page</a>.</p>)").arg(kUrlAccount);
  }
  text += tr(R"(<p>If you need help, please <a href="%1">contact us</a>.</p>)").arg(kUrlContact);
  if (!reference.isEmpty()) {
    text += tr("<p>License reference: <code>%1</code></p>").arg(reference.toHtmlEscaped());
  }

  QMessageBox box(QMessageBox::Warning, tr("Activation failed"), text, QMessageBox::Close, m_pMainWindow);
  auto *changeSerialKey = box.addButton(tr("Change serial key"), QMessageBox::ActionRole);
  if (limitReached) {
    box.setDefaultButton(QMessageBox::Close);
  } else {
    box.setDefaultButton(changeSerialKey);
  }
  box.exec();

  if (box.clickedButton() == changeSerialKey) {
    showSerialKeyDialog();
  }
}

void LicenseHandler::handleActivationUnreachable()
{
  // Leaving the mode unset means the next core start tries again; the customer is never
  // blocked by our server being down.
  m_pendingMode = ActivatedMode::kNone;
  qWarning("license activation server unreachable, starting core without activation");
  resumeCore();
}

void LicenseHandler::resumeCore()
{
  if (m_pCoreProcess == nullptr) {
    qCritical("core process not set");
    return;
  }

  if (m_pCoreProcess->processState() == deskflow::core::ProcessState::Stopped) {
    qDebug("resuming core process after activation");
    m_pCoreProcess->start();
  }
}

void LicenseHandler::reportUsage()
{
  // Personal licenses keep an absolute no-phone-home promise after activation; the report is
  // only there so business seat usage is a defensible number at renewal.
  if (!m_license.isValid() || m_license.serialKey().isOffline ||
      m_license.productEdition() != Product::Edition::kBusiness) {
    return;
  }

  if (m_settings.activatedMode() == ActivatedMode::kNone) {
    qDebug("not yet activated, skipping usage report");
    return;
  }

  qDebug("sending usage report");
  m_apiClient.reportUsage(buildApiData());
}
