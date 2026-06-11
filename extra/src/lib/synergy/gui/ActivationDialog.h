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

#pragma once

#include "synergy/gui/license/LicenseHandler.h"

#include <QDialog>

namespace Ui {
class ActivationDialog;
}

class ActivationDialog : public QDialog
{
  Q_OBJECT

public:
  ActivationDialog(QWidget *parent, LicenseHandler &licenseHandler);
  ~ActivationDialog() override;

  class ActivationMessageError : public std::runtime_error
  {
  public:
    ActivationMessageError() : std::runtime_error("could not show activation message")
    {
    }
  };

  bool serialKeyChanged() const
  {
    return m_serialKeyChanged;
  }

public Q_SLOTS:
  void reject() override;
  void accept() override;

protected:
  void refreshSerialKey();

private:
  void showResultDialog(LicenseHandler::SetSerialKeyResult result);
  void showSuccessDialog();
  void showErrorDialog(const QString &message);
  void showEvent(QShowEvent *) override;

  Ui::ActivationDialog *m_ui;
  LicenseHandler &m_licenseHandler;
  bool m_serialKeyChanged = false;
};
