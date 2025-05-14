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

#include "gui/config/AppConfig.h"

#include "shared/gui/mocks/SettingsMock.h"

#include "gmock/gmock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;
using namespace deskflow::gui::proxy;

namespace {

struct DepsMock : public AppConfig::Deps
{
  DepsMock()
  {
    ON_CALL(*this, hostname()).WillByDefault(Return("stub"));
  }

  static std::shared_ptr<NiceMock<DepsMock>> makeNice()
  {
    return std::make_shared<NiceMock<DepsMock>>();
  }

  MOCK_METHOD(QString, hostname, (), (const, override));
};

} // namespace

class AppConfigTests : public Test
{
};

TEST_F(AppConfigTests, ctor_byDefault_screenNameIsHostname)
{
  NiceMock<SettingsMock> settings;
  auto deps = DepsMock::makeNice();
  ON_CALL(*deps, hostname()).WillByDefault(Return("test hostname"));

  AppConfig appConfig(settings, deps);

  ASSERT_EQ(appConfig.screenName().toStdString(), "test hostname");
}

TEST_F(AppConfigTests, ctor_byDefault_getsFromScope)
{
  NiceMock<SettingsMock> settings;
  auto deps = DepsMock::makeNice();

  ON_CALL(settings, contains(_)).WillByDefault(Return(true));
  ON_CALL(settings, get(_, _)).WillByDefault(Return(QVariant("test screen")));
  EXPECT_CALL(settings, get(_, _)).Times(AnyNumber());

  AppConfig appConfig(settings, deps);

  ASSERT_EQ(appConfig.screenName().toStdString(), "test screen");
}

TEST_F(AppConfigTests, commit_byDefault_setsToScope)
{
  NiceMock<SettingsMock> settings;
  auto deps = DepsMock::makeNice();
  AppConfig appConfig(settings, deps);

  ON_CALL(settings, isWritable()).WillByDefault(Return(true));
  EXPECT_CALL(settings, set(_, _)).Times(AnyNumber());

  appConfig.commit();
}
