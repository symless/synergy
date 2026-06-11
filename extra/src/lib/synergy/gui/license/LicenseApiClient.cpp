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

#include "LicenseApiClient.h"

#include "synergy/gui/TestSettings.h"
#include "synergy/gui/constants.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace synergy::gui::license {

QString activateUrl()
{
  const auto testUrl = TestSettings::instance().apiUrlActivate();
  return testUrl.isEmpty() ? kUrlApiLicenseActivate : testUrl;
}

QString checkUrl()
{
  const auto testUrl = TestSettings::instance().apiUrlCheck();
  return testUrl.isEmpty() ? kUrlApiLicenseCheck : testUrl;
}

LicenseApiClient::LicenseApiClient()
{
  // Without a timeout, a request that connects but never completes leaves the client busy
  // forever, and busy gates core start.
  m_manager.setTransferTimeout();

  connect(&m_manager, &QNetworkAccessManager::finished, this, &LicenseApiClient::handleResponse);
}

void LicenseApiClient::activate(Data data, bool takeover)
{
  post(RequestKind::kActivate, QUrl(activateUrl()), data, takeover);
}

void LicenseApiClient::check(Data data)
{
  post(RequestKind::kCheck, QUrl(checkUrl()), data);
}

void LicenseApiClient::post(RequestKind kind, const QUrl &url, const Data &data, std::optional<bool> takeover)
{
  m_isBusy = true;
  m_pendingKind = kind;

  qDebug().noquote() << "license api request:" << url.toString();

  auto request = QNetworkRequest(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  m_manager.post(request, getRequestData(data, takeover));
}

void LicenseApiClient::handleResponse(QNetworkReply *reply)
{
  m_isBusy = false;
  const auto kind = m_pendingKind;

  const auto emitFailed = [this, kind](const QString &message) {
    if (kind == RequestKind::kActivate) {
      Q_EMIT activationFailed(message);
    } else {
      Q_EMIT checkFailed(message);
    }
  };

  const auto emitSucceeded = [this, kind] {
    if (kind == RequestKind::kActivate) {
      Q_EMIT activationSucceeded();
    } else {
      Q_EMIT checkSucceeded();
    }
  };

  // A transport failure is not a license verdict; activation tolerates it and retries
  // later, while the check path owns the grace period.
  const auto emitUnreachable = [this, kind](const QString &message) {
    if (kind == RequestKind::kActivate) {
      Q_EMIT activationUnreachable();
    } else {
      Q_EMIT checkFailed(message);
    }
  };

  if (!reply) {
    qWarning("no license api reply");
    emitUnreachable("License request failed, empty network reply.");
    return;
  }

  const auto response = reply->readAll();

  if (reply->error() != QNetworkReply::NoError) {
    const auto kLimit = 200;
    const auto responseSliced = response.length() > kLimit ? response.sliced(0, kLimit) + "..." : response;
    qWarning().noquote() << "license api error:" << reply->error() << reply->errorString() << responseSliced;
    emitUnreachable("License request failed, there was a network error.");
    reply->deleteLater();
    return;
  }

  qDebug().noquote() << "license api response:" << response;
  const auto jsonDoc = QJsonDocument::fromJson(response);
  if (response.isNull()) {
    qWarning("empty license api response");
    emitUnreachable("License request failed, the server sent an empty response.");
    reply->deleteLater();
    return;
  }

  const auto json = jsonDoc.object();
  if (json["status"].toString() != "success") {
    const auto status = json["status"].toString();
    const auto message = json["message"].toString();

    // Not a failure: the license is valid, another computer just holds the server activation.
    if (status == "deactivated") {
      qWarning("license api found this machine deactivated");
      if (kind == RequestKind::kActivate) {
        Q_EMIT activationDeactivated(message);
      } else {
        Q_EMIT checkDeactivated(message);
      }
      reply->deleteLater();
      return;
    }

    if (!status.isEmpty()) {
      qWarning().noquote() << "license api status:" << status;
    } else {
      qWarning("license api status was empty");
    }

    if (!message.isEmpty()) {
      qWarning().noquote() << "license api message:" << message;
    } else {
      qWarning("license api message was empty");
    }

    // An explicit disable gets the grace window like any other failure, not an abrupt cutoff.
    if (!message.isEmpty()) {
      emitFailed(message);
    } else if (status == "disabled") {
      emitFailed("License has been disabled.");
    } else {
      emitFailed("License request failed, unknown error.");
    }

    reply->deleteLater();
    return;
  }

  qInfo().noquote() << "license api request successful";
  emitSucceeded();
  reply->deleteLater();
}

QByteArray LicenseApiClient::getRequestData(const Data &data, std::optional<bool> takeover) const
{
  if (data.machineSignature.isEmpty()) {
    qFatal("cannot create license request, no machine id");
  }

  if (data.hostnameSignature.isEmpty()) {
    qFatal("cannot create license request, no hostname");
  }

  if (data.serialKey.isEmpty()) {
    qFatal("cannot create license request, no serial key");
  }

  if (data.appVersion.isEmpty()) {
    qFatal("cannot create license request, no app version");
  }

  if (data.osName.isEmpty()) {
    qFatal("cannot create license request, no os name");
  }

  QJsonObject requestData;
  requestData["machineSignature"] = data.machineSignature;
  requestData["hostnameSignature"] = data.hostnameSignature;
  requestData["serialKey"] = data.serialKey;
  requestData["appVersion"] = data.appVersion;
  requestData["osName"] = data.osName;
  requestData["isServer"] = data.isServer;
  if (takeover.has_value()) {
    requestData["takeover"] = takeover.value();
  }

  return QJsonDocument(requestData).toJson();
}

}; // namespace synergy::gui::license
