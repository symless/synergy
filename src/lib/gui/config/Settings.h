/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2020 Symless Ltd.
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

#include "ISettings.h"

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <memory>

namespace deskflow::gui {

/// @brief Encapsulates Qt config for both user and global scopes.
class Settings : public QObject, public ISettings
{
  using QSettingsProxy = deskflow::gui::proxy::QSettingsProxy;

  Q_OBJECT

public:
  struct Deps
  {
    virtual ~Deps() = default;
    virtual std::shared_ptr<QSettingsProxy> makeSettingsProxy();
  };

  explicit Settings(std::shared_ptr<Deps> deps = std::make_shared<Deps>());
  ~Settings() override = default;

  void save(bool emitSaving = true) override;
  void clear();
  void signalReady() override;
  bool contains(const QString &name) const override;
  bool isWritable() const override;
  void set(const QString &name, const QVariant &value) override;
  QVariant get(const QString &name, const QVariant &defaultValue = QVariant()) const override;
  void setScope(Scope scope = Scope::User) override;
  Scope scope() const override;
  QString fileName() const override;
  QSettingsProxy &getActiveSettings() override;
  QSettingsProxy &getSystemSettings() override;
  QSettingsProxy &getUserSettings() override;

signals:
  void ready();
  void saving();

private:
  std::shared_ptr<Deps> m_deps;
  Scope m_scope = Scope::User;
  std::shared_ptr<QSettingsProxy> m_pActiveSettings;
  std::shared_ptr<QSettingsProxy> m_pSystemSettings;
  std::shared_ptr<QSettingsProxy> m_pUserSettings;
};

} // namespace deskflow::gui
