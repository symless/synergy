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

#include "LockedSettings.h"

#include "common/Constants.h"
#include "common/Settings.h"
#include "synergy/gui/LegacySettingsKeys.h"
#include "synergy/gui/TestSettings.h"

#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QSettings>
#include <QStringList>
#include <QWidget>

namespace synergy::gui {

namespace {

// Server options live under this group in the settings file rather than in
// Settings' key list, so they pass through unmapped.
const auto kInternalConfigGroup = QStringLiteral("internalConfig/");

const auto kClipboardSharing = QStringLiteral("internalConfig/clipboardSharing");
const auto kClipboardSharingSize = QStringLiteral("internalConfig/clipboardSharingSize");

QString resolveFileName()
{
  if (const auto testFile = TestSettings::instance().lockedSettingsFile(); !testFile.isEmpty()) {
    return testFile;
  }

  // 1.20 reached this file through Qt's system-scope ini path, which is where
  // the help centre article and deployments in the field still point.
  const QSettings systemScope(
      QSettings::IniFormat, QSettings::SystemScope, kAppName, QStringLiteral("%1.locked").arg(kAppName)
  );
  const auto documentedFile = systemScope.fileName();
  if (QFile::exists(documentedFile)) {
    return documentedFile;
  }

  // Also accept the file beside the system-scope settings file, where an
  // administrator setting one up today would reasonably put it.
  if (const auto besideSettings = QStringLiteral("%1/%2.locked.ini").arg(Settings::SystemDir, kAppName);
      QFile::exists(besideSettings)) {
    return besideSettings;
  }

  return documentedFile;
}

// A disabled widget stays disabled only until the dialog recalculates which of
// its controls are available, which it does on almost every change. Holding the
// widget disabled survives that without the dialog knowing about locking.
class DisabledHold : public QObject
{
public:
  explicit DisabledHold(QWidget *widget) : QObject(widget)
  {
    widget->installEventFilter(this);
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (event->type() == QEvent::EnabledChange) {
      if (auto *widget = qobject_cast<QWidget *>(watched); widget != nullptr && widget->isEnabled()) {
        widget->setEnabled(false);
      }
    }
    return QObject::eventFilter(watched, event);
  }
};

void holdDisabled(QWidget *widget)
{
  widget->setEnabled(false);
  new DisabledHold(widget);
}

} // namespace

LockedSettings &LockedSettings::instance()
{
  static LockedSettings inst;
  return inst;
}

LockedSettings::LockedSettings()
{
  load();
}

void LockedSettings::load()
{
  m_fileName = resolveFileName();
  m_values.clear();

  if (!QFile::exists(m_fileName)) {
    qDebug().noquote() << "no locked settings file:" << m_fileName;
    return;
  }

  const QSettings ini(m_fileName, QSettings::IniFormat);
  const auto keys = ini.allKeys();
  for (const auto &key : keys) {
    const auto value = ini.value(key);
    if (Settings::validKeys().contains(key) || key.startsWith(kInternalConfigGroup)) {
      m_values.insert(key, value);
    } else if (const auto mapped = mapLegacySetting(key, value); mapped.has_value()) {
      m_values.insert(mapped->first, mapped->second);
    } else {
      qWarning().noquote() << "locked settings file names an unknown setting:" << key;
    }
  }

  qInfo().noquote() << "locked settings loaded:" << m_fileName
                    << "locking:" << QStringList(m_values.keys()).join(QStringLiteral(", "));
}

void LockedSettings::reload()
{
  load();
}

void LockedSettings::apply() const
{
  for (auto it = m_values.cbegin(); it != m_values.cend(); ++it) {
    Settings::setValue(it.key(), it.value());
  }
}

bool LockedSettings::isLocked(const QString &key) const
{
  return m_values.contains(key);
}

void LockedSettings::applyToDialog(QWidget *dialog) const
{
  if (dialog == nullptr || m_values.isEmpty()) {
    return;
  }

  const auto lock = [this, dialog](const QString &key, const QStringList &controls) {
    if (!isLocked(key)) {
      return;
    }
    for (const auto &control : controls) {
      if (auto *widget = dialog->findChild<QWidget *>(control); widget != nullptr) {
        holdDisabled(widget);
      }
    }
  };

  // The TLS checkbox is the header of a checkable group box, which has no
  // child widget of its own to disable, so locking it locks the group.
  lock(Settings::Security::TlsEnabled, {QStringLiteral("groupSecurity")});
  lock(Settings::Security::KeySize, {QStringLiteral("lblTlsKeyLength"), QStringLiteral("comboTlsKeyLength")});
  lock(
      Settings::Security::Certificate,
      {QStringLiteral("lblTlsCert"), QStringLiteral("widgetTlsCert"), QStringLiteral("btnTlsRegenCert")}
  );
  lock(Settings::Security::CheckPeers, {QStringLiteral("cbRequireClientCert")});
  lock(kClipboardSharing, {QStringLiteral("cbEnableClipboard")});
  lock(kClipboardSharingSize, {QStringLiteral("sbClipboardSizeLimit")});
}

} // namespace synergy::gui
