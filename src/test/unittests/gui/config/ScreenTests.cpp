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

#include "gui/config/Screen.h"

#include "shared/gui/TestQtCoreApp.h"
#include "shared/gui/mocks/QSettingsProxyMock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace deskflow::gui::proxy;
using namespace testing;

TEST(ScreenTests, loadSettings_whenHasSetting_readsArray)
{
  TestQtCoreApp app;
  NiceMock<QSettingsProxyMock> settings;
  Screen screen;
  ON_CALL(settings, value(_)).WillByDefault(Return("stub"));

  EXPECT_CALL(settings, beginReadArray(_)).Times(4);

  screen.loadSettings(settings);
}

TEST(ScreenTests, saveSettings_whenNameIsSet_writesArray)
{
  TestQtCoreApp app;
  NiceMock<QSettingsProxyMock> settings;
  Screen screen;
  screen.setName("stub");

  EXPECT_CALL(settings, beginWriteArray(_)).Times(4);

  screen.saveSettings(settings);
}
