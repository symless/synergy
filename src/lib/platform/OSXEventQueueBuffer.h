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

#pragma once

#include "base/IEventQueueBuffer.h"

#include <condition_variable>
#include <dispatch/dispatch.h>
#include <mutex>
#include <queue>

class IEventQueue;

//! Event queue buffer for OS X
class OSXEventQueueBuffer : public IEventQueueBuffer
{
public:
  OSXEventQueueBuffer(IEventQueue *eventQueue);
  virtual ~OSXEventQueueBuffer();

  // IEventQueueBuffer overrides
  virtual void init() override;
  virtual void waitForEvent(double timeout) override;
  virtual Type getEvent(Event &event, UInt32 &dataID) override;
  virtual bool addEvent(UInt32 dataID) override;
  virtual bool isEmpty() const override;
  virtual EventQueueTimer *newTimer(double duration, bool oneShot) const override;
  virtual void deleteTimer(EventQueueTimer *timer) const override;

private:
  IEventQueue *m_eventQueue;

  mutable std::mutex m_mutex;
  std::condition_variable m_cond;
  std::queue<UInt32> m_dataQueue;
};
