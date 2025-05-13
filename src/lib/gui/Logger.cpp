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

#include "Logger.h"

#include "string_utils.h"

#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QTime>

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

namespace deskflow::gui {

const auto kForceDebugMessages = QStringList{
    "No functional TLS backend was found", "No TLS backend is available",
    "QSslSocket::connectToHostEncrypted: TLS initialization failed", "Retrying to obtain clipboard.",
    "Unable to obtain clipboard."
};

Logger Logger::s_instance;

QString printLine(FILE *out, const QString &type, const QString &message, const QString &fileLine = "")
{
  auto datetime = QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss");
  auto logLine = QString("[%1] %2: %3").arg(datetime, type, message);

  QTextStream stream(&logLine);
  if (!fileLine.isEmpty()) {
    stream << "\n\t" + fileLine;
  }

  // We must return a non-terminated log line, but before returning,
  // stdout/stderr and Windows debug output all expect a terminated line.
  QString terminatedLogLine = logLine;
  QTextStream terminatedStream(&terminatedLogLine);
  terminatedStream << Qt::endl;

#if defined(Q_OS_WIN)
  // Debug output is viewable using either VS Code, Visual Studio, DebugView, or
  // DbgView++ (only one can be used at once). It's important to send output to
  // the debug output API, because it's difficult to view stdout and stderr from
  // a Windows GUI app.
  OutputDebugStringA(terminatedLogLine.toLocal8Bit().constData());
#else
  QTextStream outStream(out);
  outStream << terminatedLogLine;
#endif

  return logLine;
}

void Logger::loadEnvVars()
{
  const auto debugEnvVar = qEnvironmentVariable("SYNERGY_GUI_DEBUG");
  if (!debugEnvVar.isEmpty()) {
    m_debug = strToTrue(debugEnvVar);
  }

  const auto verboseEnvVar = qEnvironmentVariable("SYNERGY_GUI_VERBOSE");
  if (!verboseEnvVar.isEmpty()) {
    m_verbose = strToTrue(verboseEnvVar);
  }
}

void Logger::logVerbose(const QString &message) const
{
  if (m_verbose) {
    printLine(stdout, "VERBOSE", message);
  }
}

void Logger::handleMessage(const QtMsgType type, const QString &fileLine, const QString &message)
{
  auto mutatedType = type;
  if (kForceDebugMessages.contains(message)) {
    mutatedType = QtDebugMsg;
  }

  QString typeString;
  auto out = stdout;
  switch (mutatedType) {
  case QtDebugMsg:
    typeString = "DEBUG";
    if (!m_debug) {
      return;
    }
    break;
  case QtInfoMsg:
    typeString = "INFO";
    break;
  case QtWarningMsg:
    typeString = "WARNING";
    out = stderr;
    break;
  case QtCriticalMsg:
    typeString = "CRITICAL";
    out = stderr;
    break;
  case QtFatalMsg:
    typeString = "FATAL";
    out = stderr;
    break;
  }

  const auto logLine = printLine(out, typeString, message, fileLine);
  emit newLine(logLine);
}

} // namespace deskflow::gui
