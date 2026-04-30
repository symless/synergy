/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/win32/DaemonApp.h"

#include "arch/XArch.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "base/log_outputters.h"
#include "common/constants.h"
#include "deskflow/App.h"
#include "deskflow/win32/DaemonIpcServer.h"

#include "arch/win32/ArchMiscWindows.h" // IWYU pragma: keep
#include "deskflow/Screen.h"
#include "platform/MSWindowsDebugOutputter.h"
#include "platform/MSWindowsEventQueueBuffer.h"
#include "platform/MSWindowsWatchdog.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <string>

#include <QCoreApplication>

using namespace std;
using namespace deskflow::core;

void showHelp(int argc, char **argv) // NOSONAR - CLI args
{
  const auto binName = argc > 0 ? std::filesystem::path(argv[0]).filename().string() : kDaemonBinName;
  std::cout << "Usage: " << binName << " [-f|--foreground] [--install] [--uninstall]" << std::endl;
}

DaemonApp::DaemonApp(IEventQueue &events) : m_events(events)
{
}

DaemonApp::~DaemonApp() = default;

void DaemonApp::saveLogLevel(const QString &logLevel) const
{
  LOG_DEBUG("log level changed: %s", logLevel.toUtf8().constData());
  CLOG->setFilter(logLevel.toUtf8().constData());

  try {
    // saves setting for next time the daemon starts.
    ARCH->setting("LogLevel", logLevel.toStdString());
  } catch (XArch &e) {
    LOG_ERR("failed to save log level setting: %s", e.what());
  }
}

void DaemonApp::setMode(const QString &mode)
{
  LOG_DEBUG("mode changed: %s", mode.toUtf8().constData());
  m_mode = mode;

  try {
    // saves setting for next time the daemon starts.
    ARCH->setting("Mode", mode.toStdString());
  } catch (XArch &e) {
    LOG_ERR("failed to save mode setting: %s", e.what());
  }
}

void DaemonApp::setArgs(const QString &args)
{
  LOG_DEBUG("args updated");
  m_args = args;

  try {
    // saves setting for next time the daemon starts.
    ARCH->setting("Args", args.toStdString());
  } catch (XArch &e) {
    LOG_ERR("failed to save args setting: %s", e.what());
  }
}

void DaemonApp::setElevate(bool elevate)
{
  LOG_DEBUG("elevate value changed: %s", elevate ? "yes" : "no");
  m_elevate = elevate;

  try {
    // saves setting for next time the daemon starts.
    ARCH->setting("Elevate", std::string(elevate ? "1" : "0"));
  } catch (XArch &e) {
    LOG_ERR("failed to save elevate setting: %s", e.what());
  }
}

void DaemonApp::applyWatchdogCommand() const
{
#if SYSAPI_WIN32
  if (m_mode != "server" && m_mode != "client") {
    LOG_ERR("cannot apply watchdog command: invalid or unset mode: %s", m_mode.toUtf8().constData());
    return;
  }

  const auto appDir = QCoreApplication::applicationDirPath();

#ifdef BUILD_UNIFIED
  const auto binName = QStringLiteral(CORE_BINARY_NAME ".exe");
#else
  QString binName;
  if (m_mode == "server") {
    binName = QStringLiteral(SERVER_BINARY_NAME ".exe");
  } else if (m_mode == "client") {
    binName = QStringLiteral(CLIENT_BINARY_NAME ".exe");
  }
#endif

  const auto binPath = QStringLiteral("%1/%2").arg(appDir, binName);
  if (!std::filesystem::exists(binPath.toStdString())) {
    LOG_ERR("cannot apply watchdog command: binary does not exist at path: %s", binPath.toUtf8().constData());
    return;
  }

#ifdef BUILD_UNIFIED
  const auto command = QStringLiteral("\"%1\" %2 %3").arg(binPath, m_mode, m_args).toStdString();
#else
  const auto command = QStringLiteral("\"%1\" %2").arg(binPath, m_args).toStdString();
#endif

  LOG_INFO("running command (%s): %s", m_elevate ? "elevated" : "non-elevated", command.c_str());
  m_pWatchdog->setProcessConfig(command, m_elevate);
#else
  LOG_ERR("applying watchdog command not implemented on this platform");
#endif
}

void DaemonApp::clearWatchdogCommand()
{
  LOG_DEBUG("clearing watchdog command");

  m_mode.clear();
  m_args.clear();
  m_elevate = false;
  try {
    ARCH->setting("Mode", std::string());
    ARCH->setting("Args", std::string());
    ARCH->setting("Elevate", std::string("0"));
  } catch (XArch &e) {
    LOG_ERR("failed to clear watchdog settings: %s", e.what());
  }

#if SYSAPI_WIN32
  m_pWatchdog->setProcessConfig("", false);
#else
  LOG_ERR("clearing watchdog command not implemented on this platform");
#endif
}

void DaemonApp::clearSettings() const
{
  LOG_INFO("clearing daemon settings");
  ARCH->clearSettings();
}

void DaemonApp::connectIpcServer(const ipc::DaemonIpcServer *ipcServer) const
{
  // Use direct connection as this object is on it's own thread,
  // and so is on a different event loop to the main Qt loop.
  QObject::connect(
      ipcServer, &ipc::DaemonIpcServer::logLevelChanged, this, &DaemonApp::saveLogLevel, //
      Qt::DirectConnection
  );
  QObject::connect(
      ipcServer, &ipc::DaemonIpcServer::modeChanged, this, &DaemonApp::setMode, //
      Qt::DirectConnection
  );
  QObject::connect(
      ipcServer, &ipc::DaemonIpcServer::argsChanged, this, &DaemonApp::setArgs, //
      Qt::DirectConnection
  );
  QObject::connect(
      ipcServer, &ipc::DaemonIpcServer::elevateModeChanged, this, &DaemonApp::setElevate, //
      Qt::DirectConnection
  );
  QObject::connect(
      ipcServer, &ipc::DaemonIpcServer::startProcessRequested, this, &DaemonApp::applyWatchdogCommand, //
      Qt::DirectConnection
  );
  QObject::connect(
      ipcServer, &ipc::DaemonIpcServer::stopProcessRequested, this, &DaemonApp::clearWatchdogCommand, //
      Qt::DirectConnection
  );
  QObject::connect(
      ipcServer, &ipc::DaemonIpcServer::clearSettingsRequested, this, &DaemonApp::clearSettings, //
      Qt::DirectConnection
  );
}

void DaemonApp::install() const
{
  LOG_NOTE("installing windows daemon");
  ARCH->installDaemon();
}

void DaemonApp::uninstall() const
{
  LOG_NOTE("uninstalling windows daemon");
  ARCH->uninstallDaemon();
}

void DaemonApp::run(QThread &daemonThread)
{
  LOG_NOTE("starting daemon");

  // Important: Move the daemon app to the daemon thread before creating any more Qt objects
  // owned by the daemon app, as they will be created on the daemon thread.
  moveToThread(&daemonThread);

  QObject::connect(&daemonThread, &QThread::started, [this, &daemonThread]() {
    LOG_DEBUG("daemon thread started");

    if (m_foreground) {
      LOG_DEBUG("running daemon in foreground");
      mainLoop();
    } else {
      LOG_DEBUG("running daemon in background (daemonizing)");
      ARCH->daemonize(kAppName, [this](int, const char **) { return daemonLoop(); });
    }

    daemonThread.quit();
    LOG_DEBUG("daemon thread finished");
  });

#if SYSAPI_WIN32
  m_pWatchdog = std::make_unique<MSWindowsWatchdog>(m_foreground, *m_pFileLogOutputter);

  m_mode = QString::fromStdString(ARCH->setting("Mode"));
  m_args = QString::fromStdString(ARCH->setting("Args"));
  m_elevate = ARCH->setting("Elevate") == "1";
  if (!m_mode.isEmpty()) {
    LOG_DEBUG("using last known mode: %s", m_mode.toUtf8().constData());
    applyWatchdogCommand();
  }

  // Older daemons accepted `command=` IPC and persisted it here. Clearing
  // stops a stashed payload from auto-running if the user reverts to one.
  try {
    if (!ARCH->setting("Command").empty()) {
      LOG_DEBUG("clearing legacy Command setting");
      ARCH->setting("Command", std::string());
    }
  } catch (XArch &e) {
    LOG_ERR("failed to clear legacy Command setting: %s", e.what());
  }
#endif

  LOG_DEBUG("starting daemon thread");
  daemonThread.start();
}

int DaemonApp::daemonLoop()
{
#if SYSAPI_WIN32
  // Runs the daemon through the Windows service controller, which controls the program lifecycle.
  return ArchMiscWindows::runDaemon([this]() { return mainLoop(); });
#elif SYSAPI_UNIX
  return mainLoop();
#endif
}

int DaemonApp::mainLoop()
{
#if SYSAPI_WIN32
  if (m_pWatchdog == nullptr) {
    LOG_ERR("watchdog not initialized");
    return kExitFailed;
  }
#endif

  DAEMON_RUNNING(true);

  try {
#if SYSAPI_WIN32
    // Install the platform event queue to handle service stop events.
    // This must be done on the same thread as the event loop, otherwise the service stop
    // request will not add the quit event to the event queue, and the service won't stop.
    m_events.adoptBuffer(new MSWindowsEventQueueBuffer(&m_events));

    LOG_DEBUG("starting watchdog threads");
    m_pWatchdog->startAsync();
#endif

    LOG_INFO("daemon is running");
    m_events.loop();
  } catch (std::exception &e) { // NOSONAR - Catching all exceptions
    LOG((CLOG_CRIT "daemon error: %s", e.what()));
  } catch (...) { // NOSONAR - Catching remaining exceptions
    LOG((CLOG_CRIT "daemon unknown error"));
  }

  LOG_INFO("daemon is stopping");

#if SYSAPI_WIN32
  try {
    LOG_DEBUG("stopping process watchdog");
    m_pWatchdog->stop();
  } catch (std::exception &e) { // NOSONAR - Catching all exceptions
    LOG((CLOG_CRIT "daemon stop watchdog error: %s", e.what()));
  } catch (...) { // NOSONAR - Catching remaining exceptions
    LOG((CLOG_CRIT "daemon stop watchdog unknown error"));
  }
#endif

  DAEMON_RUNNING(false);
  return kExitSuccess;
}

std::string DaemonApp::logFilename()
{
  string logFilename;
  logFilename = ARCH->setting("LogFilename");
  if (logFilename.empty()) {
    logFilename = ARCH->getLogDirectory();
    logFilename.append("/");
    logFilename.append(kDaemonLogFilename);
  }

  return logFilename;
}

void DaemonApp::setForeground()
{
  m_foreground = true;
  showConsole();
}

void DaemonApp::initLogging()
{
#if SYSAPI_WIN32
  if (!m_foreground) {
    // Only use MS debug outputter when the process is daemonized, since stdout won't be accessible
    // in that case, but is accessible when running in the foreground.
    CLOG->insert(new MSWindowsDebugOutputter()); // NOSONAR - Adopted by `Log`
  }
#endif

  m_pFileLogOutputter = new FileLogOutputter(logFilename().c_str()); // NOSONAR - Adopted by `Log`
  CLOG->insert(m_pFileLogOutputter);
}

void DaemonApp::showConsole()
{
#if SYSAPI_WIN32
  // The daemon bin is compiled using the Win32 subsystem which works best for Windows services,
  // so when running as a foreground process we need to allocate a console (or we won't see output).
  // It is important to do this inside the arg check loop so that we can attach console ahead
  // of log output generated by handling other args.
  AllocConsole();
  freopen("CONOUT$", "w", stdout);
  freopen("CONOUT$", "w", stderr);
#endif
}
