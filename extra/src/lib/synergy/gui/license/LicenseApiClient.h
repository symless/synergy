/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2022 - 2026 Synergy App Ltd
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

class QJsonObject;
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

  /// @brief Activates this machine in the given role. Emits one of the activation signals.
  void activate(const Data &data);

  /// @brief Fire-and-forget usage report. Never emits; failures are logged at debug and ignored.
  void reportUsage(const Data &data);

  /// @brief True while an activation is in flight. Usage reports never count as busy.
  bool isBusy() const
  {
    return m_isBusy;
  }

Q_SIGNALS:
  void activationSucceeded();
  /// @param reason Machine-readable cause from the website, e.g. "limitReached" or
  /// "licenseNotFound"; empty when the website did not say.
  void activationFailed(const QString &message, const QString &reason, const QString &reference);
  void activationUnreachable();

private Q_SLOTS:
  void handleResponse(QNetworkReply *reply);

private:
  enum class RequestKind
  {
    kActivate,
    kUsage
  };

  void post(RequestKind kind, const QUrl &url, const QByteArray &body);
  void handleActivationResponse(QNetworkReply *reply);
  QJsonObject baseRequestData(const Data &data) const;

  QNetworkAccessManager m_manager;
  bool m_isBusy = false;
};

} // namespace synergy::gui::license
