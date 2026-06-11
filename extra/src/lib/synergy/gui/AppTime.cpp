/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2024 - 2026 Synergy App Ltd
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

#include "AppTime.h"

#include "synergy/gui/TestSettings.h"

#include <QtCore>

namespace synergy::gui {

AppTime::AppTime()
{
  m_realStartTime = std::chrono::system_clock::now();
  if (const auto testTime = TestSettings::instance().startTimeEpochSecs(); testTime != 0) {
    qDebug("setting test time to: %lld", static_cast<long long>(testTime));
    m_testStartTime = std::chrono::seconds{testTime};
  }
}

bool AppTime::hasTestTime() const
{
  return m_testStartTime.has_value();
}

AppTime::TimePoint AppTime::now()
{
  if (m_testStartTime.has_value()) {
    const auto runtime = std::chrono::system_clock::now() - m_realStartTime;
    return TimePoint{m_testStartTime.value()} + runtime;
  }
  return std::chrono::system_clock::now();
}

} // namespace synergy::gui
