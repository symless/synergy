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

#include "base/Stopwatch.h"

#include <gtest/gtest.h>
#include <thread>

TEST(StopwatchTests, defaultCtor_startsRunning)
{
  Stopwatch sw;
  EXPECT_FALSE(sw.isStopped());
}

TEST(StopwatchTests, triggeredCtor_startsStopped)
{
  Stopwatch sw(true);
  EXPECT_TRUE(sw.isStopped());
}

TEST(StopwatchTests, getTime_returnsElapsed)
{
  Stopwatch sw;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  double t = sw.getTime();
  EXPECT_GE(t, 0.04);
  EXPECT_LE(t, 0.5);
}

TEST(StopwatchTests, reset_returnsElapsedAndRestartsFromZero)
{
  Stopwatch sw;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  double elapsed = sw.reset();
  EXPECT_GE(elapsed, 0.04);

  double afterReset = sw.getTime();
  EXPECT_LT(afterReset, 0.05);
}

TEST(StopwatchTests, stop_freezesTime)
{
  Stopwatch sw;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  sw.stop();
  double t1 = sw.getTime();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  double t2 = sw.getTime();
  EXPECT_DOUBLE_EQ(t1, t2);
  EXPECT_TRUE(sw.isStopped());
}

TEST(StopwatchTests, startAfterStop_resumesFromElapsed)
{
  Stopwatch sw;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  sw.stop();
  double frozen = sw.getTime();

  sw.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  double resumed = sw.getTime();

  EXPECT_GT(resumed, frozen);
}

TEST(StopwatchTests, resetWhileStopped_returnsElapsedAndZeroes)
{
  Stopwatch sw;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  sw.stop();
  double elapsed = sw.reset();
  EXPECT_GE(elapsed, 0.04);

  double afterReset = sw.getTime();
  EXPECT_DOUBLE_EQ(afterReset, 0.0);
}

TEST(StopwatchTests, setTrigger_stopsAndAutoStartsOnGetTime)
{
  Stopwatch sw;
  sw.setTrigger();
  EXPECT_TRUE(sw.isStopped());

  double t = sw.getTime();
  EXPECT_FALSE(sw.isStopped());

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  double t2 = sw.getTime();
  EXPECT_GE(t2, 0.04);
}

TEST(StopwatchTests, operatorDouble_matchesGetTime)
{
  Stopwatch sw;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  sw.stop();
  double fromOp = static_cast<double>(sw);
  double fromGet = sw.getTime();
  EXPECT_DOUBLE_EQ(fromOp, fromGet);
}

TEST(StopwatchTests, constGetTime_doesNotTrigger)
{
  Stopwatch sw(true);
  const Stopwatch &ref = sw;
  double t = ref.getTime();
  EXPECT_TRUE(sw.isStopped());
}
