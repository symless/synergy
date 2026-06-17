/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2016 - 2026 Synergy App Ltd
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

#include "ActivationDialog.h"

#include "CancelActivationDialog.h"
#include "common/Settings.h"
#include "synergy/gui/TestSettings.h"
#include "synergy/gui/constants.h"
#include "synergy/gui/license/LicenseHandler.h"
#include "synergy/gui/license/license_notices.h"
#include "synergy/gui/styles.h"
#include "synergy/license/parse_serial_key.h"
#include "ui_ActivationDialog.h"

#include <QApplication>
#include <QFontDatabase>
#include <QMessageBox>
#include <QScreen>
#include <QStyle>
#include <QThread>
#include <QTimer>

using namespace deskflow::gui;
using namespace synergy::gui;
using namespace synergy::license;

const QString successTitle = "Serial key";
const QString problemTitle = "Serial key problem";

ActivationDialog::ActivationDialog(QWidget *parent, LicenseHandler &licenseHandler)
    : QDialog(parent),
      m_ui(new Ui::ActivationDialog),
      m_licenseHandler(licenseHandler)
{
  m_ui->setupUi(this);

  m_ui->m_pLabelNotice->setStyleSheet(kStyleNoticeLabel);
  m_ui->m_pTextEditSerialKey->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  refreshSerialKey();
}

ActivationDialog::~ActivationDialog()
{
  delete m_ui;
}

void ActivationDialog::refreshSerialKey()
{
  const QString testSerialKey = TestSettings::instance().serialKey();
  if (!testSerialKey.isEmpty()) {
    qDebug("using serial key from test settings");
    m_ui->m_pTextEditSerialKey->setText(testSerialKey);
  } else {
    qDebug("using serial key from config");
    const auto hexString = m_licenseHandler.license().serialKey().hexString;
    m_ui->m_pTextEditSerialKey->setText(QString::fromStdString(hexString));
  }

  m_ui->m_pTextEditSerialKey->setFocus();
  m_ui->m_pTextEditSerialKey->moveCursor(QTextCursor::End);

  const auto &license = m_licenseHandler.license();
  if (license.isTimeLimited() && (license.isExpired() || license.isExpiringSoon())) {
    m_ui->m_pLabelNotice->setText(licenseNotice(license, kColorWhite));
    m_ui->m_widgetNotice->show();
  } else {
    m_ui->m_widgetNotice->hide();
  }
}

void ActivationDialog::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);

  QTimer::singleShot(0, this, [this]() {
    const auto &license = m_licenseHandler.license();
    if (license.isTimeLimited()) {
      const auto notice = licenseNotice(license, kColorSecondary);
      if (license.isExpired()) {
        QMessageBox::warning(
            this, "License expired",
            tr("%1"
               "<p>The application will now stop working. Please renew your license today to continue using the "
               "application.</p>"
               "<p>Once you have received your new serial key, you can enter it on the next screen.</p>")
                .arg(notice)
        );
      } else if (license.isExpiringSoon()) {
        QMessageBox::warning(
            this, "License expiring soon",
            tr("%1"
               "<p>If your license expires, the application will stop working. Please renew your license today to "
               "avoid any interruptions.</p>"
               "<p>Once you have received your new serial key, you can enter it on the next screen.</p>")
                .arg(notice)
        );
      }
    }
  });
}

void ActivationDialog::reject()
{
  // don't show the cancel confirmation dialog if they've already registered,
  // since it's not relevant to customers who are changing their serial key.
  const auto &license = m_licenseHandler.license();
  if (license.isValid() && !license.isExpired()) {
    QDialog::reject();
    return;
  }

  // the accept button should be labeled "Exit" on the cancel dialog.
  CancelActivationDialog cancelActivationDialog(this);
  if (cancelActivationDialog.exec() == QDialog::Accepted) {
    QApplication::exit();
  }
}

void ActivationDialog::accept()
{
  using Result = LicenseHandler::SetSerialKeyResult;
  auto serialKey = m_ui->m_pTextEditSerialKey->toPlainText();

  if (serialKey.isEmpty()) {
    QMessageBox::information(this, "Activation", "Please enter a serial key.");
    return;
  }

  const auto result = m_licenseHandler.setLicense(serialKey);
  if (result == Result::kUnchanged) {
    qInfo() << "serial key did not change, nothing to do";
    QDialog::accept();
    return;
  }

  if (result != Result::kSuccess) {
    showResultDialog(result);
    return;
  }

  m_serialKeyChanged = true;
  showSuccessDialog();
  QDialog::accept();
}

void ActivationDialog::showResultDialog(LicenseHandler::SetSerialKeyResult result)
{
  switch (result) {
    using enum LicenseHandler::SetSerialKeyResult;

  case kInvalid:
    QMessageBox::warning(
        this, problemTitle,
        QString(
            "Invalid serial key. "
            R"(Please <a href="%1">contact us</a> for help.)"
        )
            .arg(kUrlContact)
    );
    break;

  case kExpired:
    QMessageBox::warning(
        this, problemTitle,
        QString(
            "Sorry, that serial key has expired. "
            R"(Please <a href="%1">renew</a> your license.)"
        )
            .arg(kUrlContact)
    );
    break;

  default:
    qCritical("unexpected change serial key result: %d", static_cast<int>(result));
    break;
  }
}

void ActivationDialog::showSuccessDialog()
{
  const auto &license = m_licenseHandler.license();

  QString title = successTitle;
  QString message = tr("<p>Thanks for entering your serial key for %1.</p>").arg(m_licenseHandler.productName());

  const auto tlsAvailable = m_licenseHandler.license().isTlsAvailable();
  if (tlsAvailable && Settings::value(Settings::Security::TlsEnabled).toBool()) {
    message += "<p>To ensure that TLS encryption works correctly, "
               "please use the same serial key on all of your computers.</p>";
  }

  if (license.isTimeLimited()) {
    auto daysLeft = license.daysLeft().count();
    if (license.isTrial()) {
      title = "Trial started";
      message += QString("Your trial will expire in %1 %2.").arg(daysLeft).arg((daysLeft == 1) ? "day" : "days");
    } else if (license.isSubscription()) {
      message += QString("Your license will expire in %1 %2.").arg(daysLeft).arg((daysLeft == 1) ? "day" : "days");
    }
  }

  QMessageBox::information(this, title, message);
}

void ActivationDialog::showErrorDialog(const QString &message)
{
  QString fullMessage = QString(
                            "<p>There was a problem with your serial key.</p>"
                            R"(<p>Please <a href="%1">contact us</a> )"
                            "and provide the following information:</p>"
                            "%2"
  )
                            .arg(kUrlContact)
                            .arg(message);
  QMessageBox::warning(this, problemTitle, fullMessage);
}
