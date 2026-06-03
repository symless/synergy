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

#include "platform/OSXEventQueueBuffer.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"

//
// EventQueueTimer
//

class EventQueueTimer
{
};

//
// OSXEventQueueBuffer
//

OSXEventQueueBuffer::OSXEventQueueBuffer(IEventQueue *events) : m_eventQueue(events)
{
  // Initialization is now managed using modern constructs
}

OSXEventQueueBuffer::~OSXEventQueueBuffer()
{
  // No explicit clean-up needed as GCD and STL handle resource management
}

void OSXEventQueueBuffer::init()
{
  // No initialization needed for GCD-based implementation
}

void OSXEventQueueBuffer::waitForEvent(double timeout)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  if (m_dataQueue.empty()) {
    LOG_DEBUG2("waiting for event, timeout: %f seconds", timeout);
    auto end = timeout < 0 ? std::chrono::steady_clock::time_point::max()
                           : std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout);
    m_cond.wait_until(lock, end, [this] { return !m_dataQueue.empty(); });
  } else {
    LOG_DEBUG2("found events in the queue");
  }
}

IEventQueueBuffer::Type OSXEventQueueBuffer::getEvent(Event &event, UInt32 &dataID)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  if (m_dataQueue.empty()) {
    LOG_DEBUG2("no events in queue");
    return kNone;
  }

  dataID = m_dataQueue.front();
  m_dataQueue.pop();
  lock.unlock(); // Unlock early to allow other threads to proceed

  LOG_DEBUG2("handled user event with dataID: %u", dataID);
  return kUser;
}

bool OSXEventQueueBuffer::addEvent(UInt32 dataID)
{
  // Use GCD to dispatch event addition on the main queue
  dispatch_async(dispatch_get_main_queue(), ^{
    std::lock_guard<std::mutex> lock(this->m_mutex);
    LOG_DEBUG2("adding user event with dataID: %u", dataID);
    this->m_dataQueue.push(dataID);
    this->m_cond.notify_one();
    LOG_DEBUG2("user event added to queue, dataID=%u", dataID);
  });

  // Always return true since dispatch_async does not fail under normal conditions
  return true;
}

bool OSXEventQueueBuffer::isEmpty() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  bool empty = m_dataQueue.empty();
  LOG_DEBUG2("queue is %s", empty ? "empty" : "not empty");
  return empty;
}

EventQueueTimer *OSXEventQueueBuffer::newTimer(double, bool) const
{
  return new EventQueueTimer;
}

void OSXEventQueueBuffer::deleteTimer(EventQueueTimer *timer) const
{
  delete timer;
}
