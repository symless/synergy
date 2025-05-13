/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2012 Symless Ltd.
 * Copyright (C) 2008 Volker Lanz (vl@fidra.de)
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

#include "AppConfig.h"
#include "constants.h"

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QVariant>
#include <QtCore>
#include <QtNetwork>
#include <functional>

using namespace deskflow::gui;

// this should be incremented each time the wizard is changed,
// which will force it to re-run for existing installations.
const int kWizardVersion = 8;

static const char *const kLogLevelNames[] = {"INFO", "DEBUG", "DEBUG1", "DEBUG2"};

#if defined(Q_OS_WIN)
const char AppConfig::m_CoreServerName[] = SERVER_BINARY_NAME ".exe";
const char AppConfig::m_CoreClientName[] = CLIENT_BINARY_NAME ".exe";
const char AppConfig::m_LogDir[] = "log/";
const char AppConfig::m_ConfigFilename[] = DESKFLOW_APP_ID ".sgc";
#else
const char AppConfig::m_CoreServerName[] = SERVER_BINARY_NAME;
const char AppConfig::m_CoreClientName[] = CLIENT_BINARY_NAME;
const char AppConfig::m_LogDir[] = "/var/log/";
const char AppConfig::m_ConfigFilename[] = DESKFLOW_APP_ID ".conf";
#endif

// TODO: instead, use key value pair table, which would be less fragile.
const char *const AppConfig::m_SettingsName[] = {
    "screenName",
    "port",
    "interface",
    "logLevel2",
    "logToFile",
    "logFilename",
    "wizardLastRun",
    "startedBefore",
    "elevateMode",
    "elevateModeEnum",
    "",              // 10 = edition, obsolete (using serial key instead)
    "cryptoEnabled", // 11 = kTlsEnabled (retain legacy string value)
    "autoHide",
    "", // 13 = serialKey, obsolete
    "lastVersion",
    "", // 15 = lastExpiringWarningTime, obsolete
    "", // 16 = activationHasRun, obsolete
    "", // 17 = minimizeToTray, obsolete
    "", // 18 = ActivateEmail, obsolete
    kSystemScopeSetting,
    "groupServerChecked", // kServerGroupChecked
    "useExternalConfig",
    "configFile",
    "useInternalConfig",
    "groupClientChecked",
    "serverHostname",
    "tlsCertPath",
    "tlsKeyLength",
    "preventSleep",
    "languageSync",
    "invertScrollDirection",
    "",                             // 31 = guid, obsolete
    "",                             // 32 = licenseRegistryUrl, obsolete
    "",                             // 33 = licenseNextCheck, obsolete
    "initiateConnectionFromServer", // kInvertConnection
    "",                             // 35 = clientHostMode, obsolete
    "",                             // 36 = serverClientMode, obsolete
    "enableService",
    "closeToTray",
    "mainWindowSize",
    "mainWindowPosition",
    "showDevThanks",
    "showCloseReminder",
    "enableUpdateCheck",
    "enableDragAndDrop",
    "enableLibei",
};

AppConfig::AppConfig(deskflow::gui::ISettings &settings, std::shared_ptr<Deps> deps)
    : m_Settings(settings),
      m_pDeps(deps),
      m_ScreenName(deps->hostname()),
      m_TlsCertPath(deps->defaultTlsCertPath())
{
  qDebug("determining config scope");
  setIsSystemScope(m_Settings.get(settingName(Setting::kLoadSystemSettings)).toBool());

  recall();
}

void AppConfig::recall()
{
  using enum Setting;

  recallScreenName();
  recallElevateMode();

  m_WizardLastRun = get(kWizardLastRun, m_WizardLastRun).toInt();
  m_Port = get(kPort, m_Port).toInt();
  m_Interface = get(kInterface, m_Interface).toString();
  m_LogLevel = get(kLogLevel, m_LogLevel).toInt();
  m_LogToFile = get(kLogToFile, m_LogToFile).toBool();
  m_LogFilename = get(kLogFilename, m_LogFilename).toString();
  m_StartedBefore = get(kStartedBefore, m_StartedBefore).toBool();
  m_AutoHide = get(kAutoHide, m_AutoHide).toBool();
  m_LastVersion = get(kLastVersion, m_LastVersion).toString();
  m_ServerGroupChecked = get(kServerGroupChecked, m_ServerGroupChecked).toBool();
  m_UseExternalConfig = get(kUseExternalConfig, m_UseExternalConfig).toBool();
  m_ConfigFile = get(kConfigFile, m_ConfigFile).toString();
  m_UseInternalConfig = get(kUseInternalConfig, m_UseInternalConfig).toBool();
  m_ClientGroupChecked = get(kClientGroupChecked, m_ClientGroupChecked).toBool();
  m_ServerHostname = get(kServerHostname, m_ServerHostname).toString();
  m_PreventSleep = get(kPreventSleep, m_PreventSleep).toBool();
  m_LanguageSync = get(kLanguageSync, m_LanguageSync).toBool();
  m_InvertScrollDirection = get(kInvertScrollDirection, m_InvertScrollDirection).toBool();
  m_InvertConnection = get(kInvertConnection, m_InvertConnection).toBool();
  m_EnableService = get(kEnableService, m_EnableService).toBool();
  m_CloseToTray = get(kCloseToTray, m_CloseToTray).toBool();
  m_TlsEnabled = get(kTlsEnabled, m_TlsEnabled).toBool();
  m_TlsCertPath = get(kTlsCertPath, m_TlsCertPath).toString();
  m_TlsKeyLength = get(kTlsKeyLength, m_TlsKeyLength).toInt();
  m_MainWindowPosition = get<QPoint>(kMainWindowPosition, [](const QVariant &v) { return v.toPoint(); });
  m_MainWindowSize = get<QSize>(kMainWindowSize, [](const QVariant &v) { return v.toSize(); });
  m_ShowDevThanks = get(kShowDevThanks, m_ShowDevThanks).toBool();
  m_ShowCloseReminder = get(kShowCloseReminder, m_ShowCloseReminder).toBool();
  m_EnableUpdateCheck = get<bool>(kEnableUpdateCheck, [](const QVariant &v) { return v.toBool(); });
  m_EnableDragAndDrop = get(kEnableDragAndDrop, m_EnableDragAndDrop).toBool();
  m_EnableLibei = get(kEnableLibei, m_EnableLibei).toBool();

  auto &locked = m_Settings.getLockedSettings();
  if (locked.contains(settingName(kTlsEnabled))) {
    m_TlsEnabled = locked.value(settingName(kTlsEnabled)).toBool();
  }
  if (locked.contains(settingName(kTlsCertPath))) {
    m_TlsCertPath = locked.value(settingName(kTlsCertPath)).toString();
  }
  if (locked.contains(settingName(kTlsKeyLength))) {
    m_TlsKeyLength = locked.value(settingName(kTlsKeyLength)).toInt();
  }
}

void AppConfig::recallScreenName()
{
  using enum Setting;

  const auto &screenName = get(kScreenName, m_ScreenName).toString().trimmed();

  // for some reason, the screen name can be saved as an empty string
  // in the config file. this is probably a bug. if this happens, then default
  // back to the hostname.
  if (screenName.isEmpty()) {
    qWarning("screen name was empty in config, setting to hostname");
    m_ScreenName = m_pDeps->hostname();
  } else {
    m_ScreenName = screenName;
  }
}

void AppConfig::commit()
{
  using enum Setting;
  using enum deskflow::gui::ISettings::Scope;

  qDebug("committing app config");

  if (isWritable()) {
    set(kWizardLastRun, m_WizardLastRun);
    set(kClientGroupChecked, m_ClientGroupChecked);
    set(kServerGroupChecked, m_ServerGroupChecked);
    set(kEnableUpdateCheck, m_EnableUpdateCheck);
    set(kScreenName, m_ScreenName);
    set(kPort, m_Port);
    set(kInterface, m_Interface);
    set(kLogLevel, m_LogLevel);
    set(kLogToFile, m_LogToFile);
    set(kLogFilename, m_LogFilename);
    set(kStartedBefore, m_StartedBefore);
    set(kElevateMode, static_cast<int>(m_ElevateMode));
    set(kElevateModeLegacy, m_ElevateMode == ElevateMode::kAlways);
    set(kTlsEnabled, m_TlsEnabled);
    set(kTlsCertPath, m_TlsCertPath);
    set(kTlsKeyLength, m_TlsKeyLength);
    set(kAutoHide, m_AutoHide);
    set(kLastVersion, m_LastVersion);
    set(kUseExternalConfig, m_UseExternalConfig);
    set(kConfigFile, m_ConfigFile);
    set(kUseInternalConfig, m_UseInternalConfig);
    set(kServerHostname, m_ServerHostname);
    set(kPreventSleep, m_PreventSleep);
    set(kLanguageSync, m_LanguageSync);
    set(kInvertScrollDirection, m_InvertScrollDirection);
    set(kInvertConnection, m_InvertConnection);
    set(kEnableService, m_EnableService);
    set(kCloseToTray, m_CloseToTray);
    set(kMainWindowSize, m_MainWindowSize);
    set(kMainWindowPosition, m_MainWindowPosition);
    set(kShowDevThanks, m_ShowDevThanks);
    set(kShowCloseReminder, m_ShowCloseReminder);
    set(kEnableDragAndDrop, m_EnableDragAndDrop);
    set(kEnableLibei, m_EnableLibei);
  }

  if (m_TlsChanged) {
    m_TlsChanged = false;
    emit tlsChanged();
  }
}

void AppConfig::recallElevateMode()
{
  using enum Setting;

  if (!m_Settings.contains(settingName(kElevateMode))) {
    qDebug("elevate mode not set yet, skipping");
    return;
  }

  QVariant elevateMode = get(kElevateMode);
  if (!elevateMode.isValid()) {
    qDebug("elevate mode not valid, loading legacy setting");
    elevateMode = get(kElevateModeLegacy, QVariant(static_cast<int>(kDefaultElevateMode)));
  }

  m_ElevateMode = static_cast<ElevateMode>(elevateMode.toInt());
}

QString AppConfig::settingName(Setting name)
{
  auto index = static_cast<int>(name);
  return m_SettingsName[index];
}

template <typename T> void AppConfig::set(Setting name, const std::optional<T> &value)
{
  if (value.has_value()) {
    m_Settings.set(settingName(name), value.value());
  }
}

template <typename T> void AppConfig::set(Setting name, T value)
{
  m_Settings.set(settingName(name), value);
}

QVariant AppConfig::get(Setting name, const QVariant &defaultValue) const
{
  return m_Settings.get(settingName(name), defaultValue);
}

template <typename T> std::optional<T> AppConfig::get(Setting name, std::function<T(const QVariant &)> toType) const
{
  if (m_Settings.contains(settingName(name))) {
    return toType(m_Settings.get(settingName(name)));
  } else {
    return std::nullopt;
  }
}

bool AppConfig::isWritable() const
{
  return m_Settings.isWritable();
}

QString AppConfig::logDir() const
{
  // by default log to home dir
  return QDir::home().absolutePath() + "/";
}

void AppConfig::persistLogDir() const
{
  QDir dir = logDir();

  // persist the log directory
  if (!dir.exists()) {
    dir.mkpath(dir.path());
  }
}

///////////////////////////////////////////////////////////////////////////////
// Begin getters
///////////////////////////////////////////////////////////////////////////////

ISettings &AppConfig::settings() const
{
  return m_Settings;
}

const QString &AppConfig::screenName() const
{
  return m_ScreenName;
}

int AppConfig::port() const
{
  return m_Port;
}

const QString &AppConfig::networkInterface() const
{
  return m_Interface;
}

int AppConfig::logLevel() const
{
  return m_LogLevel;
}

bool AppConfig::logToFile() const
{
  return m_LogToFile;
}

const QString &AppConfig::logFilename() const
{
  return m_LogFilename;
}

QString AppConfig::logLevelText() const
{
  return kLogLevelNames[logLevel()];
}

ProcessMode AppConfig::processMode() const
{
  return m_EnableService ? ProcessMode::kService : ProcessMode::kDesktop;
}

bool AppConfig::wizardShouldRun() const
{
  return m_WizardLastRun < kWizardVersion;
}

bool AppConfig::startedBefore() const
{
  return m_StartedBefore;
}

QString AppConfig::lastVersion() const
{
  return m_LastVersion;
}

QString AppConfig::coreServerName() const
{
  return m_CoreServerName;
}

QString AppConfig::coreClientName() const
{
  return m_CoreClientName;
}

ElevateMode AppConfig::elevateMode() const
{
  return m_ElevateMode;
}

bool AppConfig::tlsEnabled() const
{
  return m_TlsEnabled;
}

bool AppConfig::autoHide() const
{
  return m_AutoHide;
}

bool AppConfig::invertScrollDirection() const
{
  return m_InvertScrollDirection;
}

bool AppConfig::languageSync() const
{
  return m_LanguageSync;
}

bool AppConfig::preventSleep() const
{
  return m_PreventSleep;
}

bool AppConfig::invertConnection() const
{
  return m_InvertConnection;
}

QString AppConfig::tlsCertPath() const
{
  return m_TlsCertPath;
}

int AppConfig::tlsKeyLength() const
{
  return m_TlsKeyLength;
}

bool AppConfig::enableService() const
{
  return m_EnableService;
}

bool AppConfig::closeToTray() const
{
  return m_CloseToTray;
}

bool AppConfig::serverGroupChecked() const
{
  return m_ServerGroupChecked;
}

bool AppConfig::useExternalConfig() const
{
  return m_UseExternalConfig;
}

const QString &AppConfig::configFile() const
{
  return m_ConfigFile;
}

bool AppConfig::useInternalConfig() const
{
  return m_UseInternalConfig;
}

bool AppConfig::clientGroupChecked() const
{
  return m_ClientGroupChecked;
}

const QString &AppConfig::serverHostname() const
{
  return m_ServerHostname;
}

std::optional<QSize> AppConfig::mainWindowSize() const
{
  return m_MainWindowSize;
}

std::optional<QPoint> AppConfig::mainWindowPosition() const
{
  return m_MainWindowPosition;
}

bool AppConfig::showDevThanks() const
{
  return m_ShowDevThanks;
}

bool AppConfig::showCloseReminder() const
{
  return m_ShowCloseReminder;
}

std::optional<bool> AppConfig::enableUpdateCheck() const
{
  return m_EnableUpdateCheck;
}

bool AppConfig::enableDragAndDrop() const
{
  return m_EnableDragAndDrop;
}

bool AppConfig::enableLibei() const
{
  return m_EnableLibei;
}

bool AppConfig::isSystemScope() const
{
  return m_IsSystemScope;
}

///////////////////////////////////////////////////////////////////////////////
// End getters
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Begin setters
///////////////////////////////////////////////////////////////////////////////

void AppConfig::setTlsEnabled(bool value)
{
  // we purposefully do not set the 'tls changed' flag when enabling/disabling
  // tls, since that would cause the certificate to regenerate, which could get
  // pretty annoying.

  m_TlsEnabled = value;
}

void AppConfig::setTlsCertPath(const QString &value)
{
  if (m_TlsCertPath != value) {
    // deliberately only set the changed flag if there was a change.
    // it's important not to set this flag to false here.
    m_TlsChanged = true;
  }
  m_TlsCertPath = value;
}

void AppConfig::setTlsKeyLength(int value)
{
  if (m_TlsKeyLength != value) {
    // deliberately only set the changed flag if there was a change.
    // it's important not to set this flag to false here.
    m_TlsChanged = true;
  }
  m_TlsKeyLength = value;
}

void AppConfig::setServerGroupChecked(bool newValue)
{
  m_ServerGroupChecked = newValue;
}

void AppConfig::setUseExternalConfig(bool newValue)
{
  m_UseExternalConfig = newValue;
}

void AppConfig::setConfigFile(const QString &newValue)
{
  m_ConfigFile = newValue;
}

void AppConfig::setUseInternalConfig(bool newValue)
{
  m_UseInternalConfig = newValue;
}

void AppConfig::setClientGroupChecked(bool newValue)
{
  m_ClientGroupChecked = newValue;
}

void AppConfig::setServerHostname(const QString &newValue)
{
  m_ServerHostname = newValue;
}
void AppConfig::setLastVersion(const QString &version)
{
  m_LastVersion = version;
}

void AppConfig::setScreenName(const QString &s)
{
  m_ScreenName = s;
  emit screenNameChanged();
}

void AppConfig::setPort(int i)
{
  m_Port = i;
}

void AppConfig::setNetworkInterface(const QString &s)
{
  m_Interface = s;
}

void AppConfig::setLogLevel(int i)
{
  m_LogLevel = i;
  Q_EMIT logLevelChanged();
}

void AppConfig::setLogToFile(bool b)
{
  m_LogToFile = b;
}

void AppConfig::setLogFilename(const QString &s)
{
  m_LogFilename = s;
}

void AppConfig::setWizardHasRun()
{
  m_WizardLastRun = kWizardVersion;
}

void AppConfig::setStartedBefore(bool b)
{
  m_StartedBefore = b;
}

void AppConfig::setElevateMode(ElevateMode em)
{
  m_ElevateMode = em;
}

void AppConfig::setAutoHide(bool b)
{
  m_AutoHide = b;
}

void AppConfig::setInvertScrollDirection(bool newValue)
{
  m_InvertScrollDirection = newValue;
}

void AppConfig::setLanguageSync(bool newValue)
{
  m_LanguageSync = newValue;
}

void AppConfig::setPreventSleep(bool newValue)
{
  m_PreventSleep = newValue;
}

void AppConfig::setEnableService(bool enabled)
{
  m_EnableService = enabled;
}

void AppConfig::setCloseToTray(bool minimize)
{
  m_CloseToTray = minimize;
}

void AppConfig::setInvertConnection(bool value)
{
  m_InvertConnection = value;
  emit invertConnectionChanged();
}

void AppConfig::setMainWindowSize(const QSize &size)
{
  m_MainWindowSize = size;
}

void AppConfig::setMainWindowPosition(const QPoint &position)
{
  m_MainWindowPosition = position;
}

void AppConfig::setShowDevThanks(bool value)
{
  m_ShowDevThanks = value;
}

void AppConfig::setShowCloseReminder(bool value)
{
  m_ShowCloseReminder = value;
}

void AppConfig::setEnableUpdateCheck(bool value)
{
  m_EnableUpdateCheck = value;
}

void AppConfig::setEnableDragAndDrop(bool value)
{
  m_EnableDragAndDrop = value;
}

void AppConfig::setEnableLibei(bool value)
{
  m_EnableLibei = value;
}

void AppConfig::setIsSystemScope(bool value)
{
  m_IsSystemScope = value;
}

///////////////////////////////////////////////////////////////////////////////
// End setters
///////////////////////////////////////////////////////////////////////////////
