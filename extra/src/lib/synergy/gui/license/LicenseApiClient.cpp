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

#include "LicenseApiClient.h"

#include "synergy/gui/TestSettings.h"
#include "synergy/gui/constants.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace synergy::gui::license {

constexpr auto kPropRequestKind = "requestKind";
constexpr auto kPropActivationIntent = "activationIntent";
constexpr auto kPropCheckIntent = "checkIntent";

QString apiBaseUrl()
{
  const auto testBase = TestSettings::instance().apiUrlBase();
  return testBase.isEmpty() ? QString::fromUtf8(kUrlApi) : testBase;
}

QString activateUrl()
{
  return QStringLiteral("%1/product/activate").arg(apiBaseUrl());
}

QString checkUrl()
{
  return QStringLiteral("%1/product/check").arg(apiBaseUrl());
}

LicenseApiClient::LicenseApiClient()
{
  // Without a timeout, a request that connects but never completes leaves the client busy
  // forever, and busy gates core start.
  m_manager.setTransferTimeout();

  connect(&m_manager, &QNetworkAccessManager::finished, this, &LicenseApiClient::handleResponse);
}

void LicenseApiClient::activate(Data data, ActivationIntent intent, bool takeover)
{
  post(RequestKind::kActivate, QUrl(activateUrl()), data, intent, CheckIntent::kPoll, takeover);
}

void LicenseApiClient::check(Data data, CheckIntent intent)
{
  post(RequestKind::kCheck, QUrl(checkUrl()), data, ActivationIntent::kCoreStart, intent);
}

void LicenseApiClient::validate(Data data)
{
  post(RequestKind::kValidate, QUrl(checkUrl()), data);
}

void LicenseApiClient::post(
    RequestKind kind, const QUrl &url, const Data &data, ActivationIntent intent, CheckIntent checkIntent,
    std::optional<bool> takeover
)
{
  m_isBusy = true;

  qDebug().noquote() << "license api request:" << url.toString();

  auto request = QNetworkRequest(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  auto *reply = m_manager.post(request, getRequestData(data, takeover));
  reply->setProperty(kPropRequestKind, static_cast<int>(kind));
  reply->setProperty(kPropActivationIntent, static_cast<int>(intent));
  reply->setProperty(kPropCheckIntent, static_cast<int>(checkIntent));
}

void LicenseApiClient::handleResponse(QNetworkReply *reply)
{
  m_isBusy = false;

  if (!reply) {
    qWarning("no license api reply");
    return;
  }

  const auto kind = static_cast<RequestKind>(reply->property(kPropRequestKind).toInt());
  const auto intent = static_cast<ActivationIntent>(reply->property(kPropActivationIntent).toInt());
  const auto checkIntent = static_cast<CheckIntent>(reply->property(kPropCheckIntent).toInt());

  const auto emitFailed = [this, kind, intent](const QString &message) {
    if (kind == RequestKind::kActivate) {
      Q_EMIT activationFailed(intent, message);
    } else if (kind == RequestKind::kValidate) {
      Q_EMIT validateFailed(message);
    } else {
      Q_EMIT checkFailed(message);
    }
  };

  const auto emitSucceeded = [this, kind, intent] {
    if (kind == RequestKind::kActivate) {
      Q_EMIT activationSucceeded(intent);
    } else if (kind == RequestKind::kValidate) {
      Q_EMIT validateSucceeded();
    } else {
      Q_EMIT checkSucceeded();
    }
  };

  // A transport failure is not a license verdict; activation tolerates it and retries
  // later, while the check path owns the grace period. Key entry can't validate offline,
  // so it proceeds and lets the runtime check catch a bad key later.
  const auto emitUnreachable = [this, kind, intent](const QString &message) {
    if (kind == RequestKind::kActivate) {
      Q_EMIT activationUnreachable(intent);
    } else if (kind == RequestKind::kValidate) {
      Q_EMIT validateSucceeded();
    } else {
      Q_EMIT checkFailed(message);
    }
  };

  const auto response = reply->readAll();

  if (reply->error() != QNetworkReply::NoError) {
    const auto kLimit = 200;
    const auto responseSliced = response.length() > kLimit ? response.left(kLimit) + "..." : response;
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
        Q_EMIT activationDeactivated(intent, message);
      } else if (kind == RequestKind::kValidate) {
        Q_EMIT validateDeactivated(message);
      } else {
        Q_EMIT checkDeactivated(checkIntent, message);
      }
      reply->deleteLater();
      return;
    }

    // The license is valid but this machine has no activation row. At key entry that is
    // expected (the row appears once the role is known); at runtime the app must (re)claim it.
    if (status == "notActivated") {
      if (kind == RequestKind::kValidate) {
        Q_EMIT validateSucceeded();
        reply->deleteLater();
        return;
      }
      if (kind == RequestKind::kCheck) {
        Q_EMIT checkNotActivated();
        reply->deleteLater();
        return;
      }
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
    qCritical("cannot create license request, no machine id");
    return {};
  }

  if (data.hostnameSignature.isEmpty()) {
    qCritical("cannot create license request, no hostname");
    return {};
  }

  if (data.serialKey.isEmpty()) {
    qCritical("cannot create license request, no serial key");
    return {};
  }

  if (data.appVersion.isEmpty()) {
    qCritical("cannot create license request, no app version");
    return {};
  }

  if (data.osName.isEmpty()) {
    qCritical("cannot create license request, no os name");
    return {};
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
