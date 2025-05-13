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

#pragma once

#include <QSettings>

namespace deskflow::gui::proxy {

/**
 * @brief Wrapper for faking settings in tests.
 */
class QSettingsProxy
{
public:
  virtual ~QSettingsProxy() = default;

  //
  // Custom methods
  //

  QSettings &get() const;
  virtual bool fileExists() const;
  virtual void loadUser();
  virtual void loadSystem();
  virtual void loadLocked();
  virtual void copyFrom(const QSettingsProxy &other);
  virtual QString allKeysCSV() const;
  virtual bool isEmpty() const;

  //
  // QSettings methods
  //

  virtual void sync();
  virtual void clear();
  virtual QString fileName() const;
  virtual int beginReadArray(const QString &prefix);
  virtual void beginWriteArray(const QString &prefix);
  virtual void setArrayIndex(int i);
  virtual QVariant value(const QString &key) const;
  virtual QVariant value(const QString &key, const QVariant &defaultValue) const;
  virtual void endArray();
  virtual void setValue(const QString &key, const QVariant &value);
  virtual void beginGroup(const QString &prefix);
  virtual void endGroup();
  virtual void remove(const QString &key);
  virtual bool isWritable() const;
  virtual bool contains(const QString &key) const;
  virtual bool isIniFormat() const;

private:
  std::unique_ptr<QSettings> m_pSettings;
};

} // namespace deskflow::gui::proxy
