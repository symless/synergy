/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2025 Symless Ltd.
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

#include "QSettingsProxyMock.h"
#include "gui/config/ISettings.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class SettingsMock : public deskflow::gui::ISettings
{
  using QSettingsProxy = deskflow::gui::proxy::QSettingsProxy;

public:
  SettingsMock()
  {
    ON_CALL(*this, getLockedSettings()).WillByDefault(ReturnRef(m_mockSettings));
    ON_CALL(*this, getUserSettings()).WillByDefault(ReturnRef(m_mockSettings));
    ON_CALL(*this, getSystemSettings()).WillByDefault(ReturnRef(m_mockSettings));
  }

  MOCK_METHOD(void, signalReady, (), (override));
  MOCK_METHOD(bool, contains, (const QString &name), (const, override));
  MOCK_METHOD(QVariant, get, (const QString &name, const QVariant &defaultValue), (const, override));
  MOCK_METHOD(void, set, (const QString &name, const QVariant &value), (override));
  MOCK_METHOD(bool, isWritable, (), (const, override));
  MOCK_METHOD(QSettingsProxy &, getActiveSettings, (), (override));
  MOCK_METHOD(QSettingsProxy &, getSystemSettings, (), (override));
  MOCK_METHOD(QSettingsProxy &, getUserSettings, (), (override));
  MOCK_METHOD(QSettingsProxy &, getLockedSettings, (), (override));
  MOCK_METHOD(void, sync, (), (override));
  MOCK_METHOD(QString, fileName, (), (const, override));

  testing::NiceMock<QSettingsProxyMock> m_mockSettings;
};
