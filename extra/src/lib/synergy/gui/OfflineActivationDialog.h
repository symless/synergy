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

#pragma once

#include <QDialog>

class LicenseHandler;
class QLineEdit;

/**
 * @brief Walks the customer through offline challenge-response activation.
 *
 * Shows the machine-bound challenge code the customer exchanges for a response
 * code on their account page, and accepts that response code. Accepted only once
 * the response verifies, after which it is persisted by the license handler.
 */
class OfflineActivationDialog : public QDialog
{
  Q_OBJECT

public:
  OfflineActivationDialog(QWidget *parent, LicenseHandler &licenseHandler);

public Q_SLOTS:
  void accept() override;

private:
  LicenseHandler &m_licenseHandler;
  QLineEdit *m_pResponseEdit = nullptr;
};
