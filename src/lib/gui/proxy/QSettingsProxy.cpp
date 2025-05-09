/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Symless Ltd.
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

#include "QSettingsProxy.h"

#include "common/constants.h"
#include "gui/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <memory>

namespace deskflow::gui::proxy {

const auto kLegacyOrgDomain = "http-symless-com";

//
// Free functions
//

void migrateLegacyUserSettings(QSettings &newSettings)
{
  QString newPath = newSettings.fileName();
  QFile newFile(newPath);
  if (newFile.exists()) {
    qInfo("user settings already exist, skipping migration");
    return;
  }

  QSettings oldSettings(kLegacyOrgDomain, kAppName);
  QString oldPath = oldSettings.fileName();

  if (oldPath.isEmpty()) {
    qInfo("no legacy settings to migrate, filename empty");
    return;
  }

  if (!QFile(oldPath).exists()) {
    qInfo("no legacy settings to migrate, file does not exist");
    return;
  }

  QFileInfo oldFileInfo(oldPath);
  QFileInfo newFileInfo(newPath);

  qDebug(
      "migrating legacy settings: '%s' -> '%s'", //
      qPrintable(oldFileInfo.fileName()), qPrintable(newFileInfo.fileName())
  );

  QStringList keys = oldSettings.allKeys();
  for (const QString &key : keys) {
    QVariant oldValue = oldSettings.value(key);
    newSettings.setValue(key, oldValue);
    logVerbose(QString("migrating setting '%1' = '%2'").arg(key, oldValue.toString()));
  }

  newSettings.sync();
}

//
// QSettingsProxy
//

QSettings &QSettingsProxy::get() const
{
  return *m_pSettings;
}

bool QSettingsProxy::fileExists() const
{
  return QFile::exists(m_pSettings->fileName());
}

void QSettingsProxy::loadUser()
{
  m_pSettings = std::make_unique<QSettings>();

#if defined(Q_OS_MAC)
  // on mac, we used to save settings to "com.http-symless-com.Deskflow.plist"
  // because `setOrganizationName` was historically called using a url instead
  // of an actual domain (e.g. deskflow.org).
  migrateLegacyUserSettings(*m_pSettings);
#endif // Q_OS_MAC

  qDebug() << "user settings filename:" << m_pSettings->fileName();
}

void QSettingsProxy::loadSystem()
{
  m_pSettings.reset();
  m_pSettings = std::make_unique<QSettings>(
      QSettings::Format::IniFormat, QSettings::Scope::SystemScope, //
      QCoreApplication::organizationName(), QCoreApplication::applicationName()
  );

  qDebug() << "system settings filename:" << m_pSettings->fileName();
}

void QSettingsProxy::loadLocked()
{
  // Appending ".locked" to the app name should result in "[app-name].locked.ini"
  const auto appName = QCoreApplication::applicationName() + ".locked";

  m_pSettings.reset();
  m_pSettings = std::make_unique<QSettings>(
      QSettings::Format::IniFormat, QSettings::Scope::SystemScope, //
      QCoreApplication::organizationName(), appName
  );

  qDebug() << "locked settings filename:" << m_pSettings->fileName();
}

void QSettingsProxy::clear()
{
  m_pSettings->clear();
}

void QSettingsProxy::sync()
{
  m_pSettings->sync();
}

QString QSettingsProxy::fileName() const
{
  return m_pSettings->fileName();
}

int QSettingsProxy::beginReadArray(const QString &prefix)
{
  return m_pSettings->beginReadArray(prefix);
}

void QSettingsProxy::setArrayIndex(int i)
{
  m_pSettings->setArrayIndex(i);
}

QVariant QSettingsProxy::value(const QString &key) const
{
  return m_pSettings->value(key);
}

QVariant QSettingsProxy::value(const QString &key, const QVariant &defaultValue) const
{
  return m_pSettings->value(key, defaultValue);
}

void QSettingsProxy::endArray()
{
  m_pSettings->endArray();
}

void QSettingsProxy::beginWriteArray(const QString &prefix)
{
  m_pSettings->beginWriteArray(prefix);
}

void QSettingsProxy::setValue(const QString &key, const QVariant &value)
{
  m_pSettings->setValue(key, value);
}

void QSettingsProxy::beginGroup(const QString &prefix)
{
  m_pSettings->beginGroup(prefix);
}

void QSettingsProxy::remove(const QString &key)
{
  m_pSettings->remove(key);
}

void QSettingsProxy::endGroup()
{
  m_pSettings->endGroup();
}

bool QSettingsProxy::isWritable() const
{
  return m_pSettings->isWritable();
}

bool QSettingsProxy::contains(const QString &key) const
{
  return m_pSettings->contains(key);
}

void QSettingsProxy::copyFrom(const QSettingsProxy &other, bool overwrite)
{
  QStringList keys = other.get().allKeys();
  for (const QString &key : keys) {
    if (m_pSettings->contains(key) && !overwrite) {
      qDebug("skipping existing key '%s'", qPrintable(key));
      continue;
    }

    QVariant value = other.get().value(key);
    m_pSettings->setValue(key, value);
    logVerbose(QString("copying setting '%1' = '%2'").arg(key, value.toString()));
  }
}

} // namespace deskflow::gui::proxy
