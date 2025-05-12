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

#include "gui/config/Settings.h"

#include "test/shared/gui/mocks/QSettingsProxyMock.h"

#include "gmock/gmock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

using namespace testing;
using namespace deskflow::gui;
using namespace deskflow::gui::proxy;

namespace {

struct DepsMock : public Settings::Deps
{
  DepsMock()
  {
    ON_CALL(*this, makeSettingsProxy()).WillByDefault(Return(m_pMockSettings));
  }

  MOCK_METHOD(std::shared_ptr<QSettingsProxy>, makeSettingsProxy, (), (override));

  std::shared_ptr<QSettingsProxyMock> m_pMockSettings = std::make_shared<NiceMock<QSettingsProxyMock>>();
};

} // namespace

TEST(SettingsTests, ctor_loadsBothScopes)
{
  auto deps = std::make_shared<NiceMock<DepsMock>>();

  EXPECT_CALL(*deps, makeSettingsProxy()).Times(3);
  EXPECT_CALL(*deps->m_pMockSettings, loadUser()).Times(1);
  EXPECT_CALL(*deps->m_pMockSettings, loadSystem()).Times(1);
  EXPECT_CALL(*deps->m_pMockSettings, loadLocked()).Times(1);

  Settings settings(deps);
}

TEST(SettingsTests, save_callsSync)
{
  auto deps = std::make_shared<NiceMock<DepsMock>>();

  Settings settings(deps);

  EXPECT_CALL(*deps->m_pMockSettings, sync()).Times(1);

  settings.sync();
}

TEST(SettingsTests, isWritable_returnsTrue)
{
  auto deps = std::make_shared<NiceMock<DepsMock>>();

  Settings settings(deps);

  EXPECT_CALL(*deps->m_pMockSettings, isWritable()).WillOnce(Return(true));

  EXPECT_TRUE(settings.isWritable());
}

TEST(SettingsTests, contains_returnsTrue)
{
  auto deps = std::make_shared<NiceMock<DepsMock>>();

  ON_CALL(*deps->m_pMockSettings, contains(_)).WillByDefault(Return(true));

  Settings settings(deps);

  EXPECT_TRUE(settings.contains("stub"));
}

TEST(SettingsTests, fileName_returnsValue)
{
  auto deps = std::make_shared<NiceMock<DepsMock>>();
  ON_CALL(*deps->m_pMockSettings, fileName()).WillByDefault(Return("test"));

  Settings settings(deps);

  EXPECT_EQ(settings.fileName(), "test");
}

TEST(SettingsTests, get_getsValue)
{
  auto deps = std::make_shared<NiceMock<DepsMock>>();
  ON_CALL(*deps->m_pMockSettings, value(_, _)).WillByDefault(Return("test"));

  Settings settings(deps);

  EXPECT_EQ(settings.get("stub"), "test");
}

TEST(SettingsTests, set_setsvalue)
{
  auto deps = std::make_shared<NiceMock<DepsMock>>();

  Settings settings(deps);

  EXPECT_CALL(*deps->m_pMockSettings, setValue(_, _)).Times(1);

  settings.set("stub", "test");
}
