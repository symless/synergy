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

  /// @brief Why an activation was requested, which decides whether its result starts the core.
  enum class ActivationIntent
  {
    kKeyEntry,
    kCoreStart
  };

  /// @brief Why a check was requested. A core start asks the takeover question on a deactivated
  /// verdict regardless of how far the process has got; the recurring poll only acts on a server
  /// that is actually running.
  enum class CheckIntent
  {
    kPoll,
    kCoreStart
  };

  explicit LicenseApiClient();

  void activate(Data data, ActivationIntent intent, bool takeover = false);
  void check(Data data, CheckIntent intent = CheckIntent::kPoll);

  // Validates a serial key without activating, for business key entry where the role is not yet
  // known. Hits the check endpoint but stays out of the grace-period machinery the poll owns.
  void validate(Data data);

  bool isBusy()
  {
    return m_isBusy;
  }

Q_SIGNALS:
  void activationFailed(ActivationIntent intent, const QString &message);
  void activationSucceeded(ActivationIntent intent);
  void activationUnreachable(ActivationIntent intent);
  void activationDeactivated(ActivationIntent intent, const QString &message);
  void checkFailed(const QString &message);
  void checkSucceeded();
  void checkDeactivated(CheckIntent intent, const QString &message);
  void checkNotActivated();
  void validateFailed(const QString &message);
  void validateSucceeded();
  void validateDeactivated(const QString &message);

private Q_SLOTS:
  void handleResponse(QNetworkReply *reply);

private:
  enum class RequestKind
  {
    kActivate,
    kCheck,
    kValidate
  };

  void post(
      RequestKind kind, const QUrl &url, const Data &data, ActivationIntent intent = ActivationIntent::kCoreStart,
      CheckIntent checkIntent = CheckIntent::kPoll, std::optional<bool> takeover = std::nullopt
  );
  QByteArray getRequestData(const Data &data, std::optional<bool> takeover) const;

  QNetworkAccessManager m_manager;
  bool m_isBusy = false;
};

} // namespace synergy::gui::license
