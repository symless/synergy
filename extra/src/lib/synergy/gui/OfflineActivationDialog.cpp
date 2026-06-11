/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2026 Synergy App Ltd
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

#include "OfflineActivationDialog.h"

#include "synergy/gui/constants.h"
#include "synergy/gui/license/LicenseHandler.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace synergy::gui;

OfflineActivationDialog::OfflineActivationDialog(QWidget *parent, LicenseHandler &licenseHandler)
    : QDialog(parent),
      m_licenseHandler(licenseHandler)
{
  setWindowTitle(tr("Offline activation"));
  setMinimumWidth(440);

  const auto layout = new QVBoxLayout(this);

  const auto infoLabel = new QLabel(
      tr(R"(On a device with internet access, enter the activation code at )"
         R"(<a href="%1">your account page</a> to get a response code.)")
          .arg(kUrlAccount),
      this
  );
  infoLabel->setWordWrap(true);
  infoLabel->setOpenExternalLinks(true);
  layout->addWidget(infoLabel);

  const auto form = new QFormLayout();

  const auto challengeEdit = new QLineEdit(m_licenseHandler.offlineActivationChallenge(), this);
  challengeEdit->setReadOnly(true);
  challengeEdit->setCursorPosition(0);
  form->addRow(tr("Activation code"), challengeEdit);

  m_pResponseEdit = new QLineEdit(this);
  form->addRow(tr("Response code"), m_pResponseEdit);

  layout->addLayout(form);

  const auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Activate"));
  connect(buttons, &QDialogButtonBox::accepted, this, &OfflineActivationDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &OfflineActivationDialog::reject);
  layout->addWidget(buttons);

  m_pResponseEdit->setFocus();
}

void OfflineActivationDialog::accept()
{
  const auto response = m_pResponseEdit->text();
  if (response.trimmed().isEmpty()) {
    QMessageBox::information(this, tr("Offline activation"), tr("Please enter the response code."));
    return;
  }

  if (!m_licenseHandler.applyOfflineActivationResponse(response)) {
    QMessageBox::warning(
        this, tr("Offline activation"),
        tr("<p>That response code is not valid for this computer.</p>"
           "<p>Please check that you typed the whole response code correctly, and that "
           "the activation code was generated on this computer. "
           R"(If the problem persists, please <a href="%1">contact us</a>.</p>)")
            .arg(kUrlContact)
    );
    return;
  }

  QMessageBox::information(
      this, tr("Offline activation"), tr("Thanks, your license is now activated on this computer.")
  );
  QDialog::accept();
}
