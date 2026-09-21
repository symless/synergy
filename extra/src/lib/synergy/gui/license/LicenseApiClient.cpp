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

namespace {

constexpr auto kPropRequestKind = "requestKind";

// Opts the app into slot-limited, activate-at-core-start semantics on the website; without it
// the request falls through to the legacy unmetered path and nothing is enforced.
constexpr int kActivationProtocolVersion = 1;

QString apiBaseUrl()
{
  const auto testBase = TestSettings::instance().apiUrlBase();
  return testBase.isEmpty() ? QString::fromUtf8(kUrlApi) : testBase;
}

QString activateUrl()
{
  return QStringLiteral("%1/product/activate").arg(apiBaseUrl());
}

QString usageUrl()
{
  return QStringLiteral("%1/product/usage").arg(apiBaseUrl());
}

} // namespace

LicenseApiClient::LicenseApiClient()
{
  // Without a timeout, a request that connects but never completes leaves the client busy
  // forever, and busy gates core start.
  m_manager.setTransferTimeout();

  connect(&m_manager, &QNetworkAccessManager::finished, this, &LicenseApiClient::handleResponse);
}

void LicenseApiClient::activate(const Data &data)
{
  auto requestData = baseRequestData(data);
  if (requestData.isEmpty()) {
    return;
  }
  requestData["isServer"] = data.isServer;
  requestData["activationProtocolVersion"] = kActivationProtocolVersion;

  m_isBusy = true;
  post(RequestKind::kActivate, QUrl(activateUrl()), QJsonDocument(requestData).toJson());
}

void LicenseApiClient::reportUsage(const Data &data)
{
  const auto requestData = baseRequestData(data);
  if (requestData.isEmpty()) {
    return;
  }
  post(RequestKind::kUsage, QUrl(usageUrl()), QJsonDocument(requestData).toJson());
}

void LicenseApiClient::post(RequestKind kind, const QUrl &url, const QByteArray &body)
{
  qDebug().noquote() << "license api request:" << url.toString();

  auto request = QNetworkRequest(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  auto *reply = m_manager.post(request, body);
  reply->setProperty(kPropRequestKind, static_cast<int>(kind));
}

void LicenseApiClient::handleResponse(QNetworkReply *reply)
{
  if (!reply) {
    qWarning("no license api reply");
    return;
  }

  const auto kind = static_cast<RequestKind>(reply->property(kPropRequestKind).toInt());
  if (kind == RequestKind::kUsage) {
    if (reply->error() != QNetworkReply::NoError) {
      qDebug().noquote() << "usage report failed:" << reply->errorString();
    } else {
      qDebug("usage report sent");
    }
    reply->deleteLater();
    return;
  }

  m_isBusy = false;
  handleActivationResponse(reply);
  reply->deleteLater();
}

void LicenseApiClient::handleActivationResponse(QNetworkReply *reply)
{
  const auto response = reply->readAll();

  // A transport failure is not a license verdict; the core starts and the next core start retries.
  if (reply->error() != QNetworkReply::NoError) {
    const auto kLimit = 200;
    const auto responseSliced = response.length() > kLimit ? response.left(kLimit) + "..." : response;
    qWarning().noquote() << "license api error:" << reply->error() << reply->errorString() << responseSliced;
    Q_EMIT activationUnreachable();
    return;
  }

  qDebug().noquote() << "license api response:" << response;
  if (response.isNull()) {
    qWarning("empty license api response");
    Q_EMIT activationUnreachable();
    return;
  }

  const auto json = QJsonDocument::fromJson(response).object();
  const auto status = json["status"].toString();
  const auto reference = json["reference"].toString();

  if (status == "success") {
    qInfo("license activation successful");
    Q_EMIT activationSucceeded();
    return;
  }

  const auto message = json["message"].toString();
  const auto reason = json["reason"].toString();
  qWarning().noquote() << "license activation failed, status:" << status << "reason:" << reason
                       << "message:" << message;
  Q_EMIT activationFailed(
      message.isEmpty() ? tr("License activation failed, unknown error.") : message, reason, reference
  );
}

QJsonObject LicenseApiClient::baseRequestData(const Data &data) const
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
  return requestData;
}

}; // namespace synergy::gui::license
