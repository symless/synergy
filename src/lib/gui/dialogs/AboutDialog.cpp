/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "common/Constants.h"
#include "common/Settings.h"
#include "common/VersionInfo.h"
#include "gui/StyleUtils.h"

#ifdef SYNERGY_EXTRA_HEADER
#include "synergy/hooks/gui_hook.h"
#endif

#include <QClipboard>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent), ui{std::make_unique<Ui::AboutDialog>()}
{
  ui->setupUi(this);

  setWindowTitle(windowTitle().arg(kAppName));
  ui->lblIcon->hide();
  ui->lblName->setPixmap(QPixmap(QStringLiteral(":/image/logo-%1.png").arg(deskflow::gui::iconMode())));
  ui->lblName->setContentsMargins(0, 0, 0, 10);
  ui->linkContributors->hide();

  const int px = (fontMetrics().height() * 6);
  const QSize pixmapSize(px, px);
  ui->lblIcon->setFixedSize(pixmapSize);

  ui->lblIcon->setPixmap(QPixmap(QIcon::fromTheme(kRevFqdnName).pixmap(QSize().scaled(pixmapSize, Qt::KeepAspectRatio)))
  );

  ui->btnCopyVersion->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
  connect(ui->btnCopyVersion, &QPushButton::clicked, this, &AboutDialog::copyVersionText);

  ui->lblVersion->setText(kDisplayVersion);
  ui->lblDescription->setText(kAppDescription);
  ui->lblCopyright->setText(kCopyright);

  // Use non-breaking space in each awesome dev name so names are not split across lines.
  QStringList devsNbsp;
  for (const auto &dev : s_awesomeDevs) {
    QString withNbsp = dev;
    devsNbsp.append(withNbsp.replace(" ", QStringLiteral("&nbsp;")));
  }

  ui->lblImportantDevs->setTextFormat(Qt::RichText);
  ui->lblImportantDevs->setText(QStringLiteral("%1\n").arg(devsNbsp.join(", ")));

  ui->btnOk->setDefault(true);
  connect(ui->btnOk, &QPushButton::clicked, this, &AboutDialog::close);

#ifdef SYNERGY_EXTRA_HEADER
  synergy::hooks::onAbout(this);
#endif
}

void AboutDialog::copyVersionText() const
{
  // Divergence from upstream: kDisplayVersion already carries version + commit; don't append the git sha.
  QString infoString = QStringLiteral("%1: %2\nQt: %3\nSystem: %4")
                           .arg(kAppName, kDisplayVersion, qVersion(), QSysInfo::prettyProductName());
  if (Settings::isPortableMode()) {
    infoString.append(QStringLiteral("\nPortable Mode"));
  }
#ifdef Q_OS_LINUX
  infoString.append(QStringLiteral("\nSession: %1 (%2)")
                        .arg(qEnvironmentVariable("XDG_CURRENT_DESKTOP"), qEnvironmentVariable("XDG_SESSION_TYPE")));
#endif
  QGuiApplication::clipboard()->setText(infoString);
}

AboutDialog::~AboutDialog() = default;
