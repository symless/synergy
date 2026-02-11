/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2015-2016 Symless Ltd.
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

#include "ipc/IpcMessage.h"
#include "ipc/IpcServer.h"

#include <condition_variable>
#include <gmock/gmock.h>
#include <mutex>

using ::testing::_;
using ::testing::Invoke;

class IEventQueue;

class MockIpcServer : public IpcServer
{
public:
  MockIpcServer() = default;
  ~MockIpcServer() = default;

  MOCK_METHOD(void, listen, (), (override));
  MOCK_METHOD(void, send, (const IpcMessage &, IpcClientType), (override));
  MOCK_METHOD(bool, hasClients, (IpcClientType), (const, override));

  void delegateToFake()
  {
    ON_CALL(*this, send(_, _)).WillByDefault(Invoke(this, &MockIpcServer::mockSend));
  }

  void waitForSend()
  {
    std::unique_lock<std::recursive_mutex> lock(m_sendMutex);
    m_sendCond.wait_for(lock, std::chrono::seconds(5));
  }

private:
  void mockSend(const IpcMessage &, IpcClientType)
  {
    std::lock_guard<std::recursive_mutex> lock(m_sendMutex);
    m_sendCond.notify_all();
  }

  std::condition_variable_any m_sendCond;
  std::recursive_mutex m_sendMutex;
};
