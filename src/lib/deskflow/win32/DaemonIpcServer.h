/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QSet>

class QLocalServer;
class QLocalSocket;

namespace deskflow::core::ipc {

class DaemonIpcServer : public QObject
{
  Q_OBJECT

public:
  explicit DaemonIpcServer(QObject *parent, const QString &logFilename);
  ~DaemonIpcServer() override;

  void listen();

signals:
  void logLevelChanged(const QString &logLevel);
  void modeChanged(const QString &mode);
  void argsChanged(const QString &args);
  void elevateModeChanged(bool elevate);
  void startProcessRequested();
  void stopProcessRequested();
  void clearSettingsRequested();

private:
  void processMessage(QLocalSocket *clientSocket, const QString &message);
  void processLogLevel(QLocalSocket *&clientSocket, const QStringList &messageParts);
  void processMode(QLocalSocket *&clientSocket, const QStringList &messageParts);
  void processArgs(QLocalSocket *&clientSocket, const QString &message);
  void processElevate(QLocalSocket *&clientSocket, const QStringList &messageParts);

private slots:
  void handleNewConnection();
  void handleReadyRead();
  void handleDisconnected();
  void handleErrorOccurred();

private:
  const QString m_logFilename;
  QLocalServer *m_server;
  QSet<QLocalSocket *> m_clients;
};

} // namespace deskflow::core::ipc
