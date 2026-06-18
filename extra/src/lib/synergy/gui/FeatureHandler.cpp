/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2025 - 2026 Synergy App Ltd
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

#include "FeatureHandler.h"

#include "common/Settings.h"
#include "license/LicenseHandler.h"
#include "synergy/gui/SettingsMigration.h"
#include "synergy/gui/SettingsScope.h"
#include "synergy/gui/TestSettings.h"
#include "synergy/gui/UpdateChannel.h"
#include "synergy/gui/constants.h"
#include "synergy/gui/styles.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

using namespace synergy::gui;

void FeatureHandler::handleMainWindow(QMainWindow *mainWindow)
{
  m_pMainWindow = mainWindow;
  styleUpdateNotice(mainWindow);
}

void FeatureHandler::styleUpdateNotice(QMainWindow *mainWindow) const
{
  // Reuse the license-expiry notice pill (kStyleNoticeLabel) so the upstream
  // "update available" status-bar button reads identically to it.
  auto *button = mainWindow->findChild<QPushButton *>(QStringLiteral("btnUpdate"));
  if (button == nullptr) {
    qWarning("update notice: no btnUpdate in status bar, skipping styling");
    return;
  }
  button->setStyleSheet(kStyleNoticeLabel);
}

void FeatureHandler::handleAppStart()
{
  if (TestSettings::instance().isEnabled()) {
    addTestMenu();
  }
}

void FeatureHandler::addTestMenu()
{
  if (m_pMainWindow == nullptr) {
    qWarning("cannot add test menu, main window not set");
    return;
  }

  auto testMenu = new QMenu(QObject::tr("Test"), m_pMainWindow);
  m_pMainWindow->menuBar()->addMenu(testMenu);

  auto fatalAction = new QAction(QObject::tr("Trigger fatal error"), m_pMainWindow);
  QObject::connect(fatalAction, &QAction::triggered, [] { qFatal("test fatal error"); });
  testMenu->addAction(fatalAction);

  auto criticalAction = new QAction(QObject::tr("Trigger critical error"), m_pMainWindow);
  QObject::connect(criticalAction, &QAction::triggered, [] { qCritical("test critical error"); });
  testMenu->addAction(criticalAction);
}

void FeatureHandler::handleSettings(QDialog *parent) const
{
  if (parent == nullptr) {
    return;
  }

  addUpdateChannelOption(parent);

  const auto &licenseHandler = LicenseHandler::instance();
  if (licenseHandler.isEnabled() && !licenseHandler.license().isSettingsScopeAvailable()) {
    return;
  }
  addScopeTab(parent);
}

// The helpers below reach into upstream's AboutDialog.ui by object name and grid
// coordinate. Those references are upstream's, not ours, so they look like magic
// strings/numbers: the logo, version and license links share "gridLayout", where
// row 1 is the logo, row 3 the version and row 6 a trailing spacer.

void FeatureHandler::handleAbout(QDialog *parent) const
{
  if (parent == nullptr) {
    return;
  }
  addTagline(parent);
  setAttribution(parent);
  tightenVersionRow(parent);
  addBuildDate(parent);
  addLicenseLinks(parent);
  addTrademark(parent);
}

void FeatureHandler::addTagline(QDialog *parent) const
{
  auto *grid = parent->findChild<QGridLayout *>(QStringLiteral("gridLayout"));
  if (grid == nullptr) {
    return;
  }
  // Row 2 is empty, so the tagline sits under the logo; the margin is its gap to the version.
  auto *tagline = new QLabel(QObject::tr("Unite every computer on your desk"), parent);
  tagline->setContentsMargins(0, 0, 0, 16);
  grid->addWidget(tagline, 2, 1);

  // Collapse the trailing spacer to tighten the gap to the section below.
  if (auto *item = grid->itemAtPosition(6, 1); item != nullptr && item->spacerItem() != nullptr) {
    item->spacerItem()->changeSize(0, 0);
    grid->invalidate();
  }
}

void FeatureHandler::setAttribution(QDialog *parent) const
{
  auto *description = parent->findChild<QLabel *>(QStringLiteral("lblDescription"));
  if (description == nullptr) {
    return;
  }
  // Dim via alpha on the theme foreground (not a fixed grey) so it adapts to light/dark.
  description->setWordWrap(true);
  auto font = description->font();
  font.setPointSizeF(font.pointSizeF() * 0.85);
  description->setFont(font);
  auto palette = description->palette();
  auto color = palette.color(QPalette::WindowText);
  color.setAlphaF(0.6);
  palette.setColor(QPalette::WindowText, color);
  description->setPalette(palette);
  description->setText(
      QObject::tr(
          "Synergy was originally created by Chris Schoeneman. Thanks to our contributors "
          "and the open source projects Synergy builds on, including Deskflow, Qt, OpenSSL, "
          "and many others."
      )
  );
}

void FeatureHandler::tightenVersionRow(QDialog *parent) const
{
  // The .ui pins lblVersion to a fixed point size, so it renders out of step with its
  // "Version:" sibling on systems whose default UI font differs; match the sibling instead.
  auto *prefix = parent->findChild<QLabel *>(QStringLiteral("label"));
  auto *version = parent->findChild<QLabel *>(QStringLiteral("lblVersion"));
  if (prefix != nullptr && version != nullptr) {
    version->setFont(prefix->font());
  }

  // The copy button is taller than the version text and padded the centred row; cap it.
  if (auto *copyButton = parent->findChild<QPushButton *>(QStringLiteral("btnCopyVersion"))) {
    copyButton->setMaximumHeight(copyButton->fontMetrics().height() + 4);
  }
}

void FeatureHandler::addBuildDate(QDialog *parent) const
{
  auto *version = parent->findChild<QLabel *>(QStringLiteral("lblVersion"));
  if (version == nullptr) {
    return;
  }

  // Compile date of this file; close enough to the build date since release
  // builds always compile from scratch in CI.
  const auto date = QLocale::c().toDate(QStringLiteral(__DATE__).simplified(), QStringLiteral("MMM d yyyy"));
  if (!date.isValid()) {
    return;
  }
  version->setText(QStringLiteral("%1, built %2").arg(version->text(), date.toString(Qt::ISODate)));
}

void FeatureHandler::addLicenseLinks(QDialog *parent) const
{
  // Upstream shows only the GPL link; add the EULA alongside it.
  if (auto *linkGpl = parent->findChild<QLabel *>(QStringLiteral("linkGpl"))) {
    const auto eula = QString(kLink).arg(kUrlEula, kColorSecondary, QObject::tr("End User License Agreement"));
    const auto gpl = QString(kLink).arg(kUrlGpl, kColorSecondary, QObject::tr("License: GNU GPL Version 2"));
    linkGpl->setText(QStringLiteral("%1&nbsp;&nbsp;|&nbsp;&nbsp;%2").arg(eula, gpl));
  }
}

void FeatureHandler::addTrademark(QDialog *parent) const
{
  if (auto *copyright = parent->findChild<QLabel *>(QStringLiteral("lblCopyright"))) {
    copyright->setText(
        copyright->text() + QStringLiteral("\n") + QObject::tr("The Synergy logo is a trademark of Synergy App Ltd")
    );
  }
}

static QString pathLabel(const QString &path)
{
  if (QFileInfo::exists(path)) {
    return QStringLiteral("<a href=\"file://%1\">%1</a>").arg(path);
  }
  return QStringLiteral("<code>%1</code> %2").arg(path, QObject::tr("(not yet created)"));
}

void FeatureHandler::addScopeTab(QDialog *parent) const
{
  auto *tabWidget = parent->findChild<QTabWidget *>();
  if (tabWidget == nullptr) {
    qWarning("scope tab: no QTabWidget in settings dialog, skipping");
    return;
  }

  auto *scopeTab = new QWidget(tabWidget);
  auto *layout = new QVBoxLayout(scopeTab);

  auto *intro = new QLabel(
      QObject::tr("Choose where %1 stores its settings. Changes take effect when %1 restarts.")
          .arg(QApplication::applicationName()),
      scopeTab
  );
  intro->setWordWrap(true);
  layout->addWidget(intro);
  layout->addSpacing(8);

  const auto mutedColor = scopeTab->palette().color(QPalette::Disabled, QPalette::WindowText).name();
  const auto helpStyle = QStringLiteral("color: %1;").arg(mutedColor);
  const auto pathStyle = QStringLiteral("color: %1; font-size: 10pt;").arg(mutedColor);

  const auto buildOption = [&](const QString &title, const QString &help, const QString &path) {
    auto *radio = new QRadioButton(title, scopeTab);
    auto *helpLabel = new QLabel(help, scopeTab);
    helpLabel->setWordWrap(true);
    helpLabel->setIndent(22);
    helpLabel->setStyleSheet(helpStyle);
    auto *pathWidget = new QLabel(pathLabel(path), scopeTab);
    pathWidget->setIndent(22);
    pathWidget->setOpenExternalLinks(true);
    pathWidget->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pathWidget->setWordWrap(true);
    pathWidget->setStyleSheet(pathStyle);

    layout->addWidget(radio);
    layout->addWidget(helpLabel);
    layout->addWidget(pathWidget);
    layout->addSpacing(14);
    return radio;
  };

  auto *userRadio = buildOption(
      QObject::tr("Current user"), QObject::tr("Settings apply only to your user account on this computer."),
      Settings::UserSettingFile
  );
  auto *systemRadio = buildOption(
      QObject::tr("All users"),
      QObject::tr("Settings apply to every user on this computer. Requires administrator privileges."),
      Settings::SystemSettingFile
  );

  const bool isSystem = (Settings::settingsFile() == Settings::SystemSettingFile);
  userRadio->setChecked(!isSystem);
  systemRadio->setChecked(isSystem);

  layout->addStretch();

  QObject::connect(systemRadio, &QRadioButton::toggled, parent, [parent, userRadio, systemRadio](bool toSystem) {
    if (SettingsScope::switchTo(parent, toSystem)) {
      return;
    }
    QSignalBlocker blockUser(userRadio);
    QSignalBlocker blockSystem(systemRadio);
    userRadio->setChecked(!toSystem);
    systemRadio->setChecked(toSystem ? false : true);
  });

  tabWidget->addTab(scopeTab, QObject::tr("Scope"));
}

void FeatureHandler::addUpdateChannelOption(QDialog *parent) const
{
  auto *autoUpdate = parent->findChild<QCheckBox *>(QStringLiteral("cbAutoUpdate"));
  if (autoUpdate == nullptr) {
    qWarning("update channel: no cbAutoUpdate in settings dialog, skipping");
    return;
  }

  auto *page = autoUpdate->parentWidget();
  auto *pageLayout = page ? qobject_cast<QBoxLayout *>(page->layout()) : nullptr;
  const int checkboxIndex = pageLayout ? pageLayout->indexOf(autoUpdate) : -1;
  if (checkboxIndex < 0) {
    qWarning("update channel: unexpected update-checkbox layout, skipping");
    return;
  }

  auto *row = new QHBoxLayout();
  auto *label = new QLabel(QObject::tr("Update channel"), page);
  auto *combo = new QComboBox(page);
  combo->addItem(QObject::tr("Stable"), UpdateChannel::Stable);
  combo->addItem(QObject::tr("Beta"), UpdateChannel::Beta);
  combo->setCurrentIndex(combo->findData(UpdateChannel::current()));
  label->setBuddy(combo);
  row->setContentsMargins(22, 0, 0, 0);
  row->addWidget(label);
  row->addWidget(combo);
  row->addStretch();
  pageLayout->insertLayout(checkboxIndex + 1, row);

  // The channel only affects the on-startup check, so mirror the checkbox's
  // enabled state; the checkbox itself is disabled when settings aren't writable.
  const auto syncEnabled = [label, combo](bool on) {
    label->setEnabled(on);
    combo->setEnabled(on);
  };
  syncEnabled(autoUpdate->isChecked() && autoUpdate->isEnabled());
  QObject::connect(autoUpdate, &QCheckBox::toggled, combo, syncEnabled);

  // The injected combo isn't part of the dialog's isModified() check, so changing
  // it would never enable Save. Enable it directly on change, and persist on the
  // dialog's accept() (not on change) so Cancel still discards.
  auto *buttonBox = parent->findChild<QDialogButtonBox *>();
  if (auto *saveButton = buttonBox ? buttonBox->button(QDialogButtonBox::Save) : nullptr) {
    QObject::connect(combo, &QComboBox::currentIndexChanged, saveButton, [saveButton]() {
      saveButton->setEnabled(true);
    });
  }

  QObject::connect(parent, &QDialog::accepted, combo, [combo]() {
    UpdateChannel::setCurrent(combo->currentData().toString());
  });
}
