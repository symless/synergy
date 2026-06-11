/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2022-2026 Synergy Ltd.
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

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

#include <optional>

class QNetworkReply;

namespace synergy::gui::license {

class LicenseApiClient : public QObject
{
  Q_OBJECT

public:
  struct Data
  {
    QString machineSignature;
    QString hostnameSignature;
    QString serialKey;
    QString appVersion;
    QString osName;
    bool isServer;
  };

  explicit LicenseApiClient();

  void activate(Data data, bool takeover = false);
  void check(Data data);

  bool isBusy()
  {
    return m_isBusy;
  }

Q_SIGNALS:
  void activationFailed(const QString &message);
  void activationSucceeded();
  void activationUnreachable();
  void activationDeactivated(const QString &message);
  void checkFailed(const QString &message);
  void checkSucceeded();

private Q_SLOTS:
  void handleResponse(QNetworkReply *reply);

private:
  enum class RequestKind
  {
    kActivate,
    kCheck
  };

  void post(RequestKind kind, const QUrl &url, const Data &data, std::optional<bool> takeover = std::nullopt);
  QByteArray getRequestData(const Data &data, std::optional<bool> takeover) const;

  QNetworkAccessManager m_manager;
  bool m_isBusy = false;
  RequestKind m_pendingKind = RequestKind::kActivate;
};

} // namespace synergy::gui::license
