/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2021 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXPowerManager.h"
#include "base/Log.h"
#include "common/Constants.h"

#include <string>

OSXPowerManager::~OSXPowerManager()
{
  enableSleep();
}

void OSXPowerManager::disableSleep()
{
  if (!m_sleepPreventionAssertionID) {
    const std::string reason = std::string(kAppName) + " application";
    CFStringRef reasonForActivity = CFStringCreateWithCString(nullptr, reason.c_str(), kCFStringEncodingUTF8);
    IOReturn result = IOPMAssertionCreateWithName(
        kIOPMAssertPreventUserIdleDisplaySleep, kIOPMAssertionLevelOn, reasonForActivity, &m_sleepPreventionAssertionID
    );
    CFRelease(reasonForActivity);
    if (result != kIOReturnSuccess) {
      m_sleepPreventionAssertionID = 0;
      LOG_ERR("failed to disable system idle sleep");
    }
  }
}

void OSXPowerManager::enableSleep()
{
  if (m_sleepPreventionAssertionID) {
    IOPMAssertionRelease(m_sleepPreventionAssertionID);
  }
}
