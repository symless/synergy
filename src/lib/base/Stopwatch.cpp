/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
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

Stopwatch::Stopwatch(bool triggered) : m_mark(Clock::now()), m_elapsed(0.0), m_triggered(triggered), m_stopped(triggered)
{
}

double Stopwatch::reset()
{
  if (m_stopped) {
    const double dt = m_elapsed.count();
    m_elapsed = Duration(0.0);
    return dt;
  } else {
    const auto now = Clock::now();
    const double dt = Duration(now - m_mark).count();
    m_mark = now;
    return dt;
  }
}

void Stopwatch::stop()
{
  if (m_stopped) {
    return;
  }

  m_elapsed = Duration(Clock::now() - m_mark);
  m_stopped = true;
}

void Stopwatch::start()
{
  m_triggered = false;
  if (!m_stopped) {
    return;
  }

  // resume: set mark so elapsed time is preserved
  m_mark = Clock::now() - std::chrono::duration_cast<Clock::duration>(m_elapsed);
  m_elapsed = Duration(0.0);
  m_stopped = false;
}

void Stopwatch::setTrigger()
{
  stop();
  m_triggered = true;
}

double Stopwatch::getTime()
{
  if (m_triggered) {
    const double dt = m_elapsed.count();
    start();
    return dt;
  } else if (m_stopped) {
    return m_elapsed.count();
  } else {
    return Duration(Clock::now() - m_mark).count();
  }
}

Stopwatch::operator double()
{
  return getTime();
}

bool Stopwatch::isStopped() const
{
  return m_stopped;
}

double Stopwatch::getTime() const
{
  if (m_stopped) {
    return m_elapsed.count();
  } else {
    return Duration(Clock::now() - m_mark).count();
  }
}

Stopwatch::operator double() const
{
  return getTime();
}
