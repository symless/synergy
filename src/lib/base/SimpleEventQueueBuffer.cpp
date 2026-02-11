/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
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

#include "base/SimpleEventQueueBuffer.h"
#include "base/Stopwatch.h"

#include <chrono>

class EventQueueTimer
{
};

//
// SimpleEventQueueBuffer
//

SimpleEventQueueBuffer::SimpleEventQueueBuffer() : m_queueReady(false)
{
}

SimpleEventQueueBuffer::~SimpleEventQueueBuffer()
{
}

void SimpleEventQueueBuffer::waitForEvent(double timeout)
{
  std::unique_lock<std::recursive_mutex> lock(m_queueMutex);
  Stopwatch timer(true);
  while (!m_queueReady) {
    double timeLeft = timeout;
    if (timeLeft >= 0.0) {
      timeLeft -= timer.getTime();
      if (timeLeft < 0.0) {
        return;
      }
    }
    if (timeLeft < 0.0) {
      m_queueReadyCond.wait(lock);
    } else {
      m_queueReadyCond.wait_for(lock, std::chrono::duration<double>(timeLeft));
    }
  }
}

IEventQueueBuffer::Type SimpleEventQueueBuffer::getEvent(Event &, UInt32 &dataID)
{
  std::lock_guard<std::recursive_mutex> lock(m_queueMutex);
  if (!m_queueReady) {
    return kNone;
  }
  dataID = m_queue.back();
  m_queue.pop_back();
  m_queueReady = !m_queue.empty();
  return kUser;
}

bool SimpleEventQueueBuffer::addEvent(UInt32 dataID)
{
  std::lock_guard<std::recursive_mutex> lock(m_queueMutex);
  m_queue.push_front(dataID);
  if (!m_queueReady) {
    m_queueReady = true;
    m_queueReadyCond.notify_all();
  }
  return true;
}

bool SimpleEventQueueBuffer::isEmpty() const
{
  std::lock_guard<std::recursive_mutex> lock(m_queueMutex);
  return !m_queueReady;
}

EventQueueTimer *SimpleEventQueueBuffer::newTimer(double, bool) const
{
  return new EventQueueTimer;
}

void SimpleEventQueueBuffer::deleteTimer(EventQueueTimer *timer) const
{
  delete timer;
}
