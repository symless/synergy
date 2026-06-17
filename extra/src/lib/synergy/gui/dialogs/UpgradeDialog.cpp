/*
 * Synergy -- mouse and keyboard sharing utility
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

#include "UpgradeDialog.h"

#include "synergy/gui/constants.h"

#include <QtCore>
#include <QtGui>

UpgradeDialog::UpgradeDialog(QWidget *parent) : QMessageBox(parent)
{
  m_cancel = addButton("Cancel", QMessageBox::RejectRole);
  m_upgrade = addButton("Upgrade", QMessageBox::AcceptRole);
}

void UpgradeDialog::showDialog(const QString &title, const QString &body, const QString &link)
{
  setWindowTitle(title);
  setText(body);
  exec();

  if (clickedButton() == m_upgrade) {
    const auto url = QUrl(link);
    if (QDesktopServices::openUrl(url)) {
      qDebug("opened url: %s", qUtf8Printable(url.toString()));
    } else {
      qCritical("failed to open url: %s", qUtf8Printable(url.toString()));
    }
  } else {
    qDebug("upgrade was declined");
  }
}
