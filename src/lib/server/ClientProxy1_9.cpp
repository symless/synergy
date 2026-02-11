/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
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

#include "server/ClientProxy1_9.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/IPrimaryScreen.h"
#include "deskflow/ProtocolUtil.h"
#include "deskflow/protocol_types.h"

#include <cstring>

ClientProxy1_9::ClientProxy1_9(
    const String &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_8(name, adoptedStream, server, events),
      m_events(events)
{
}

bool ClientProxy1_9::parseMessage(const UInt8 *code)
{
  if (memcmp(code, kMsgCGrabScreen, 4) == 0) {
    return recvGrabScreen();
  }
  return ClientProxy1_8::parseMessage(code);
}

bool ClientProxy1_9::recvGrabScreen()
{
  SInt16 x, y;
  if (!ProtocolUtil::readf(getStream(), kMsgCGrabScreen + 4, &x, &y)) {
    return false;
  }
  LOG((CLOG_DEBUG "received client \"%s\" grab screen request at %d,%d", getName().c_str(), x, y));

  m_events->addEvent(Event(
      m_events->forClientProxy().grabScreen(),
      getEventTarget(),
      IPrimaryScreen::MotionInfo::alloc(x, y)
  ));

  return true;
}
