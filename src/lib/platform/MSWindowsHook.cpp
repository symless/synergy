/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2011 Chris Schoeneman
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

#include "platform/MSWindowsHook.h"
#include "base/Log.h"
#include "deskflow/XScreen.h"
#include "deskflow/protocol_types.h"

#include <cstring>

static const char *g_name = "dfwhook";

static DWORD g_processID = 0;
static DWORD g_threadID = 0;
static HHOOK g_getMessage = NULL;
static HHOOK g_keyboardLL = NULL;
static HHOOK g_mouseLL = NULL;
static bool g_screenSaver = false;
static EHookMode g_mode = kHOOK_DISABLE;
static UInt32 g_zoneSides = 0;
static SInt32 g_zoneSize = 0;
static SInt32 g_xScreen = 0;
static SInt32 g_yScreen = 0;
static SInt32 g_wScreen = 0;
static SInt32 g_hScreen = 0;
static WPARAM g_deadVirtKey = 0;
static WPARAM g_deadRelease = 0;
static LPARAM g_deadLParam = 0;
static BYTE g_deadKeyState[256] = {0};
static BYTE g_keyState[256] = {0};
static DWORD g_hookThread = 0;
static bool g_fakeServerInput = false;
static BOOL g_isPrimary = TRUE;
static UInt32 g_anchoredKeysMask[8] = {};

struct AnchoredCombo
{
  UInt8 modifiers;
  UInt8 vk;
};
static const int kMaxAnchoredCombos = 32;
static AnchoredCombo g_anchoredCombos[kMaxAnchoredCombos] = {};
static int g_anchoredComboCount = 0;

static const UInt8 kModShift = 0x01;
static const UInt8 kModCtrl = 0x02;
static const UInt8 kModAlt = 0x04;
static const UInt8 kModWin = 0x08;

struct PendingMod
{
  DWORD vk;
  LPARAM lParam;
  UInt8 modBit;
};
static PendingMod g_pendingMods[4] = {};
static int g_pendingModCount = 0;
static UInt8 g_pendingModBits = 0;
static UInt8 g_comboFiredModBits = 0;
static PendingMod g_firedMods[4] = {};
static int g_firedModCount = 0;
static DWORD g_comboTriggerVk = 0;
static WORD g_comboTriggerSc = 0;
static DWORD g_pendingTriggerVk = 0;
static LPARAM g_pendingTriggerLParam = 0;
static HWND g_anchorForeground = NULL;
static HWND g_anchorDeskWindow = NULL;

static UInt8 modBitForVk(DWORD vk)
{
  switch (vk) {
  case VK_SHIFT:
  case VK_LSHIFT:
  case VK_RSHIFT:
    return kModShift;
  case VK_CONTROL:
  case VK_LCONTROL:
  case VK_RCONTROL:
    return kModCtrl;
  case VK_MENU:
  case VK_LMENU:
  case VK_RMENU:
    return kModAlt;
  case VK_LWIN:
  case VK_RWIN:
    return kModWin;
  default:
    return 0;
  }
}

MSWindowsHook::MSWindowsHook()
{
}

MSWindowsHook::~MSWindowsHook()
{
  cleanup();

  if (g_processID == GetCurrentProcessId()) {
    uninstall();
    uninstallScreenSaver();
    g_processID = 0;
  }
}

void MSWindowsHook::loadLibrary()
{
  if (g_processID == 0) {
    g_processID = GetCurrentProcessId();
  }

  // initialize library
  if (init(GetCurrentThreadId()) == 0) {
    LOG((CLOG_ERR "failed to init %s.dll, another program may be using it", g_name));
    LOG((CLOG_INFO "restarting your computer may solve this error"));
    throw XScreenOpenFailure();
  }
}

int MSWindowsHook::init(DWORD threadID)
{
  // try to open process that last called init() to see if it's
  // still running or if it died without cleaning up.
  if (g_processID != 0 && g_processID != GetCurrentProcessId()) {
    HANDLE process = OpenProcess(STANDARD_RIGHTS_REQUIRED, FALSE, g_processID);
    if (process != NULL) {
      // old process (probably) still exists so refuse to
      // reinitialize this DLL (and thus steal it from the
      // old process).
      int result = CloseHandle(process);
      if (result == false) {
        return 0;
      }
    }

    // clean up after old process.  the system should've already
    // removed the hooks so we just need to reset our state.
    g_processID = GetCurrentProcessId();
    g_threadID = 0;
    g_getMessage = NULL;
    g_keyboardLL = NULL;
    g_mouseLL = NULL;
    g_screenSaver = false;
  }

  // save thread id.  we'll post messages to this thread's
  // message queue.
  g_threadID = threadID;

  // set defaults
  g_mode = kHOOK_DISABLE;
  g_zoneSides = 0;
  g_zoneSize = 0;
  g_xScreen = 0;
  g_yScreen = 0;
  g_wScreen = 0;
  g_hScreen = 0;

  return 1;
}

int MSWindowsHook::cleanup()
{
  if (g_processID == GetCurrentProcessId()) {
    g_threadID = 0;
  }

  return 1;
}

void MSWindowsHook::setSides(UInt32 sides)
{
  g_zoneSides = sides;
}

void MSWindowsHook::setZone(SInt32 x, SInt32 y, SInt32 w, SInt32 h, SInt32 jumpZoneSize)
{
  g_zoneSize = jumpZoneSize;
  g_xScreen = x;
  g_yScreen = y;
  g_wScreen = w;
  g_hScreen = h;
}

static void releaseComboLocally(int triggerVk, WORD triggerScanCode);
static void injectComboLocally(int triggerVk, WORD triggerScanCode);

void MSWindowsHook::setMode(EHookMode mode)
{
  if (mode == g_mode) {
    // no change
    return;
  }
  if (g_mode == kHOOK_RELAY_EVENTS) {
    if (g_comboFiredModBits != 0 && g_comboTriggerVk != 0) {
      releaseComboLocally(g_comboTriggerVk, g_comboTriggerSc);
    }
    g_pendingModCount = 0;
    g_pendingModBits = 0;
    g_pendingTriggerVk = 0;
    g_pendingTriggerLParam = 0;
    g_comboFiredModBits = 0;
    g_firedModCount = 0;
    g_comboTriggerVk = 0;
    memset(g_keyState, 0, sizeof(g_keyState));
  }
  g_mode = mode;
}

void MSWindowsHook::setAnchoredKeys(const UInt32 mask[8])
{
  memcpy(g_anchoredKeysMask, mask, sizeof(g_anchoredKeysMask));
}

void MSWindowsHook::setAnchoredCombos(const UInt8 *data, int count)
{
  g_anchoredComboCount = (count > kMaxAnchoredCombos) ? kMaxAnchoredCombos : count;
  for (int i = 0; i < g_anchoredComboCount; ++i) {
    g_anchoredCombos[i].modifiers = data[i * 2];
    g_anchoredCombos[i].vk = data[i * 2 + 1];
  }
}

void MSWindowsHook::setAnchoredKeysFKeys(UInt32 fKeyBitmask)
{
  UInt32 mask[8] = {};
  for (int i = 0; i < 24; ++i) {
    if (fKeyBitmask & (1u << i)) {
      int vk = VK_F1 + i;
      mask[vk / 32] |= (1u << (vk % 32));
    }
  }
  setAnchoredKeys(mask);
}

void MSWindowsHook::setAnchorTargets(HWND foregroundWindow, HWND deskWindow)
{
  g_anchorForeground = foregroundWindow;
  g_anchorDeskWindow = deskWindow;
}

static void keyboardGetState(BYTE keys[256], DWORD vkCode, bool kf_up)
{
  // we have to use GetAsyncKeyState() rather than GetKeyState() because
  // we don't pass through most keys so the event synchronous state
  // doesn't get updated.  we do that because certain modifier keys have
  // side effects, like alt and the windows key.
  if (vkCode < 0 || vkCode >= 256) {
    return;
  }

  // Keep track of key state on our own in case GetAsyncKeyState() fails
  g_keyState[vkCode] = kf_up ? 0 : 0x80;
  g_keyState[VK_SHIFT] = g_keyState[VK_LSHIFT] | g_keyState[VK_RSHIFT];

  SHORT key;
  // Test whether GetAsyncKeyState() is being honest with us
  key = GetAsyncKeyState(vkCode);

  if (key & 0x80) {
    // The only time we know for sure that GetAsyncKeyState() is working
    // is when it tells us that the current key is down.
    // In this case, update g_keyState to reflect what GetAsyncKeyState()
    // is telling us, just in case we have gotten out of sync

    for (int i = 0; i < 256; ++i) {
      key = GetAsyncKeyState(i);
      g_keyState[i] = (BYTE)((key < 0) ? 0x80u : 0);
    }
  }

  // copy g_keyState to keys
  for (int i = 0; i < 256; ++i) {
    keys[i] = g_keyState[i];
  }

  key = GetKeyState(VK_CAPITAL);
  keys[VK_CAPITAL] = (BYTE)(((key < 0) ? 0x80 : 0) | (key & 1));
}

static WPARAM makeKeyMsg(UINT virtKey, WCHAR wc, bool noAltGr)
{
  return MAKEWPARAM((WORD)wc, MAKEWORD(virtKey & 0xff, noAltGr ? 1 : 0));
}

static void flushPendingMods()
{
  for (int i = 0; i < g_pendingModCount; i++) {
    WPARAM charAndVirtKey = makeKeyMsg((UINT)g_pendingMods[i].vk, 0, false);
    PostThreadMessage(g_threadID, DESKFLOW_MSG_KEY, charAndVirtKey, g_pendingMods[i].lParam);
  }
  g_pendingModCount = 0;
  g_pendingModBits = 0;
}

static void flushPendingTrigger()
{
  if (g_pendingTriggerVk != 0) {
    WPARAM charAndVirtKey = makeKeyMsg((UINT)g_pendingTriggerVk, 0, false);
    PostThreadMessage(g_threadID, DESKFLOW_MSG_KEY, charAndVirtKey, g_pendingTriggerLParam);
    g_pendingTriggerVk = 0;
    g_pendingTriggerLParam = 0;
  }
}

static bool tryFireCombo()
{
  if (g_pendingTriggerVk == 0 || g_pendingModBits == 0)
    return false;
  for (int i = 0; i < g_anchoredComboCount; ++i) {
    if (g_anchoredCombos[i].vk == g_pendingTriggerVk &&
        g_anchoredCombos[i].modifiers == g_pendingModBits) {
      WORD sc = (WORD)((g_pendingTriggerLParam >> 16) & 0x1FF);
      injectComboLocally(g_pendingTriggerVk, sc);
      g_comboFiredModBits = g_pendingModBits;
      g_comboTriggerVk = g_pendingTriggerVk;
      g_comboTriggerSc = sc;
      PostThreadMessage(g_threadID, DESKFLOW_MSG_DEBUG,
          0xAC000000u | g_pendingTriggerVk | (g_comboFiredModBits << 8),
          g_pendingTriggerLParam);
      g_pendingModCount = 0;
      g_pendingModBits = 0;
      g_pendingTriggerVk = 0;
      g_pendingTriggerLParam = 0;
      return true;
    }
  }
  return false;
}

static void sendInputToTarget(INPUT *inputs, int count)
{
  HWND target = g_anchorForeground;
  HWND deskWnd = g_anchorDeskWindow;

  INPUT wrapped[12] = {};
  int idx = 0;

  wrapped[idx].type = INPUT_KEYBOARD;
  wrapped[idx].ki.wVk = DESKFLOW_HOOK_FAKE_INPUT_VIRTUAL_KEY;
  wrapped[idx].ki.wScan = DESKFLOW_HOOK_FAKE_INPUT_SCANCODE;
  wrapped[idx].ki.dwFlags = 0;
  idx++;

  for (int i = 0; i < count && idx < 11; i++)
    wrapped[idx++] = inputs[i];

  wrapped[idx].type = INPUT_KEYBOARD;
  wrapped[idx].ki.wVk = DESKFLOW_HOOK_FAKE_INPUT_VIRTUAL_KEY;
  wrapped[idx].ki.wScan = DESKFLOW_HOOK_FAKE_INPUT_SCANCODE;
  wrapped[idx].ki.dwFlags = KEYEVENTF_KEYUP;
  idx++;

  if (target != NULL && deskWnd != NULL) {
    DWORD deskThread = GetWindowThreadProcessId(deskWnd, NULL);
    DWORD targetThread = GetWindowThreadProcessId(target, NULL);
    AttachThreadInput(targetThread, deskThread, TRUE);
    SetForegroundWindow(target);
    AttachThreadInput(targetThread, deskThread, FALSE);

    SendInput(idx, wrapped, sizeof(INPUT));

    AttachThreadInput(targetThread, deskThread, TRUE);
    SetForegroundWindow(deskWnd);
    AttachThreadInput(targetThread, deskThread, FALSE);
  } else {
    SendInput(idx, wrapped, sizeof(INPUT));
  }
}

static void injectComboLocally(int triggerVk, WORD triggerScanCode)
{
  INPUT inputs[8] = {};
  int idx = 0;

  for (int i = 0; i < g_pendingModCount; i++) {
    inputs[idx].type = INPUT_KEYBOARD;
    inputs[idx].ki.wVk = (WORD)g_pendingMods[i].vk;
    inputs[idx].ki.wScan = (WORD)MapVirtualKey(g_pendingMods[i].vk, MAPVK_VK_TO_VSC);
    inputs[idx].ki.dwFlags = 0;
    idx++;
  }

  inputs[idx].type = INPUT_KEYBOARD;
  inputs[idx].ki.wVk = (WORD)triggerVk;
  inputs[idx].ki.wScan = triggerScanCode;
  inputs[idx].ki.dwFlags = 0;
  idx++;

  g_firedModCount = g_pendingModCount;
  for (int i = 0; i < g_pendingModCount; i++)
    g_firedMods[i] = g_pendingMods[i];

  sendInputToTarget(inputs, idx);
}

static void releaseComboLocally(int triggerVk, WORD triggerScanCode)
{
  INPUT inputs[8] = {};
  int idx = 0;

  inputs[idx].type = INPUT_KEYBOARD;
  inputs[idx].ki.wVk = (WORD)triggerVk;
  inputs[idx].ki.wScan = triggerScanCode;
  inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
  idx++;

  for (int i = g_firedModCount - 1; i >= 0; i--) {
    inputs[idx].type = INPUT_KEYBOARD;
    inputs[idx].ki.wVk = (WORD)g_firedMods[i].vk;
    inputs[idx].ki.wScan = (WORD)MapVirtualKey(g_firedMods[i].vk, MAPVK_VK_TO_VSC);
    inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
    idx++;
  }

  g_firedModCount = 0;
  sendInputToTarget(inputs, idx);
}

static void setDeadKey(WCHAR wc[], int size, UINT flags)
{
  if (g_deadVirtKey != 0) {
    auto virtualKey = static_cast<UINT>(g_deadVirtKey);
    auto scanCode = static_cast<UINT>((g_deadLParam & 0x10ff0000u) >> 16);
    if (ToUnicode(virtualKey, scanCode, g_deadKeyState, wc, size, flags) >= 2) {
      // If ToUnicode returned >=2, it means that we accidentally removed
      // a double dead key instead of restoring it. Thus, we call
      // ToUnicode again with the same parameters to restore the
      // internal dead key state.
      ToUnicode(virtualKey, scanCode, g_deadKeyState, wc, size, flags);

      // We need to keep track of this because g_deadVirtKey will be
      // cleared later on; this would cause the dead key release to
      // incorrectly restore the dead key state.
      g_deadRelease = g_deadVirtKey;
    }
  }
}

static bool keyboardHookHandler(WPARAM wParam, LPARAM lParam)
{
  DWORD vkCode = static_cast<DWORD>(wParam);
  bool kf_up = (lParam & (KF_UP << 16)) != 0;

  // check for special events indicating if we should start or stop
  // passing events through and not report them to the server.  this
  // is used to allow the server to synthesize events locally but
  // not pick them up as user events.
  if (wParam == DESKFLOW_HOOK_FAKE_INPUT_VIRTUAL_KEY && ((lParam >> 16) & 0xffu) == DESKFLOW_HOOK_FAKE_INPUT_SCANCODE) {
    // update flag
    g_fakeServerInput = ((lParam & 0x80000000u) == 0);
    PostThreadMessage(g_threadID, DESKFLOW_MSG_DEBUG, 0xff000000u | wParam, lParam);

    // discard event
    return true;
  }

  // if we're expecting fake input then just pass the event through
  // and do not forward to the server
  if (g_fakeServerInput) {
    PostThreadMessage(g_threadID, DESKFLOW_MSG_DEBUG, 0xfe000000u | wParam, lParam);
    return false;
  }

  if (g_mode == kHOOK_RELAY_EVENTS) {
    UInt32 vk = static_cast<UInt32>(vkCode);

    if (vk < 256 && (g_anchoredKeysMask[vk / 32] & (1u << (vk % 32))) != 0) {
      bool isComboTrigger = false;
      for (int i = 0; i < g_anchoredComboCount; ++i) {
        if (g_anchoredCombos[i].vk == vk) {
          isComboTrigger = true;
          break;
        }
      }
      if (!isComboTrigger) {
        if ((lParam & 0x80000000u) == 0 && (lParam & 0x40000000u) == 0)
          PostThreadMessage(g_threadID, DESKFLOW_MSG_DEBUG, 0xAC000000u | vk, lParam);
        return false;
      }
    }

    if (g_anchoredComboCount > 0 && vk < 256) {
      bool isKeyDown = (lParam & 0x80000000u) == 0;
      UInt8 modBit = modBitForVk(vk);

      // Modifier UP after combo fired: pass through locally, don't relay.
      // No isInjected guard -- VK_CANCEL wrapping keeps our own events out.
      if (!isKeyDown && modBit != 0 && (g_comboFiredModBits & modBit)) {
        g_comboFiredModBits &= ~modBit;
        if (g_comboFiredModBits == 0 && g_comboTriggerVk != 0) {
          releaseComboLocally(g_comboTriggerVk, g_comboTriggerSc);
          g_comboTriggerVk = 0;
        }
        g_keyState[vk] = 0;
        if (vk == VK_LSHIFT || vk == VK_RSHIFT)
          g_keyState[VK_SHIFT] = g_keyState[VK_LSHIFT] | g_keyState[VK_RSHIFT];
        return false;
      }

      // While combo is active, repeat combo trigger stays local.
      if (isKeyDown && modBit == 0 && g_comboFiredModBits != 0) {
        for (int i = 0; i < g_anchoredComboCount; ++i) {
          if (g_anchoredCombos[i].vk == vk && g_anchoredCombos[i].modifiers == g_comboFiredModBits) {
            WORD sc = (WORD)((lParam >> 16) & 0x1FF);
            INPUT inputs[1] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = (WORD)vk;
            inputs[0].ki.wScan = sc;
            inputs[0].ki.dwFlags = 0;
            sendInputToTarget(inputs, 1);
            PostThreadMessage(g_threadID, DESKFLOW_MSG_DEBUG, 0xAC000000u | vk | (g_comboFiredModBits << 8), lParam);
            return true;
          }
        }
      }

      // While combo is active, trigger UP releases the combo.
      if (!isKeyDown && modBit == 0 && g_comboFiredModBits != 0) {
        for (int i = 0; i < g_anchoredComboCount; ++i) {
          if (g_anchoredCombos[i].vk == vk) {
            WORD sc = (WORD)((lParam >> 16) & 0x1FF);
            releaseComboLocally(vk, sc);
            g_comboTriggerVk = 0;
            return true;
          }
        }
      }

      // While combo is active, non-combo keys also stay local
      if (g_comboFiredModBits != 0)
        return false;

      // Modifier DOWN: hold it if used in any anchored combo
      if (isKeyDown && modBit != 0) {
        if (g_pendingModBits & modBit)
          return true; // already pending (repeat), eat it

        bool usedInCombo = false;
        for (int i = 0; i < g_anchoredComboCount; ++i) {
          if (g_anchoredCombos[i].modifiers & modBit) {
            usedInCombo = true;
            break;
          }
        }
        if (usedInCombo && g_pendingModCount < 4) {
          g_pendingMods[g_pendingModCount].vk = vk;
          g_pendingMods[g_pendingModCount].lParam = lParam;
          g_pendingMods[g_pendingModCount].modBit = modBit;
          g_pendingModCount++;
          g_pendingModBits |= modBit;
          g_keyState[vk] = 0x80;
          if (vk == VK_LSHIFT || vk == VK_RSHIFT)
            g_keyState[VK_SHIFT] = 0x80;
          if (tryFireCombo())
            return true;
          return true; // eat it, don't relay yet
        }
      }

      // Modifier UP while pending: no combo arrived, flush to client
      if (!isKeyDown && modBit != 0 && (g_pendingModBits & modBit)) {
        flushPendingTrigger();
        flushPendingMods();
        // fall through to relay this UP normally
      }

      if (isKeyDown && modBit == 0) {
        if (g_pendingModBits != 0 || g_pendingTriggerVk != 0) {
          bool isThisComboTrigger = false;
          for (int i = 0; i < g_anchoredComboCount; ++i) {
            if (g_anchoredCombos[i].vk == vk) {
              isThisComboTrigger = true;
              break;
            }
          }

          if (isThisComboTrigger) {
            g_pendingTriggerVk = vk;
            g_pendingTriggerLParam = lParam;
            if (tryFireCombo())
              return true;
            return true;
          }

          flushPendingTrigger();
          flushPendingMods();
        }
      }

      if (!isKeyDown && modBit == 0 && g_pendingTriggerVk != 0) {
        flushPendingTrigger();
        flushPendingMods();
      }

      if (g_pendingModBits == 0 && g_comboFiredModBits == 0 &&
          vk < 256 && (g_anchoredKeysMask[vk / 32] & (1u << (vk % 32))) != 0) {
        return false;
      }
    }
  }

  // VK_RSHIFT may be sent with an extended scan code but right shift
  // is not an extended key so we reset that bit.
  if (wParam == VK_RSHIFT) {
    lParam &= ~0x01000000u;
  }

  // tell server about event
  PostThreadMessage(g_threadID, DESKFLOW_MSG_DEBUG, wParam, lParam);

  // ignore dead key release
  if ((g_deadVirtKey == wParam || g_deadRelease == wParam) && (lParam & 0x80000000u) != 0) {
    g_deadRelease = 0;
    PostThreadMessage(g_threadID, DESKFLOW_MSG_DEBUG, wParam | 0x04000000, lParam);
    return false;
  }

  // we need the keyboard state for ToAscii()
  BYTE keys[256];
  keyboardGetState(keys, vkCode, kf_up);

  // ToAscii() maps ctrl+letter to the corresponding control code
  // and ctrl+backspace to delete.  we don't want those translations
  // so clear the control modifier state.  however, if we want to
  // simulate AltGr (which is ctrl+alt) then we must not clear it.
  UINT control = keys[VK_CONTROL] | keys[VK_LCONTROL] | keys[VK_RCONTROL];
  UINT menu = keys[VK_MENU] | keys[VK_LMENU] | keys[VK_RMENU];
  if ((control & 0x80) == 0 || (menu & 0x80) == 0) {
    keys[VK_LCONTROL] = 0;
    keys[VK_RCONTROL] = 0;
    keys[VK_CONTROL] = 0;
  } else {
    keys[VK_LCONTROL] = 0x80;
    keys[VK_RCONTROL] = 0x80;
    keys[VK_CONTROL] = 0x80;
    keys[VK_LMENU] = 0x80;
    keys[VK_RMENU] = 0x80;
    keys[VK_MENU] = 0x80;
  }

  // ToAscii() needs to know if a menu is active for some reason.
  // we don't know and there doesn't appear to be any way to find
  // out.  so we'll just assume a menu is active if the menu key
  // is down.
  // FIXME -- figure out some way to check if a menu is active
  UINT flags = 0;
  if ((menu & 0x80) != 0)
    flags |= 1;

  // if we're on the server screen then just pass numpad keys with alt
  // key down as-is.  we won't pick up the resulting character but the
  // local app will.  if on a client screen then grab keys as usual;
  // if the client is a windows system it'll synthesize the expected
  // character.  if not then it'll probably just do nothing.
  if (g_mode != kHOOK_RELAY_EVENTS) {
    // we don't use virtual keys because we don't know what the
    // state of the numlock key is.  we'll hard code the scan codes
    // instead.  hopefully this works across all keyboards.
    UINT sc = (lParam & 0x01ff0000u) >> 16;
    if (menu && (sc >= 0x47u && sc <= 0x52u && sc != 0x4au && sc != 0x4eu)) {
      return false;
    }
  }

  // map the key event to a character.  we have to put the dead
  // key back first and this has the side effect of removing it.
  WCHAR wc[] = {0, 0};
  setDeadKey(wc, 2, flags);

  UINT scanCode = ((lParam & 0x10ff0000u) >> 16);
  int n = ToUnicode((UINT)wParam, scanCode, keys, wc, 2, flags);

  // if mapping failed and ctrl and alt are pressed then try again
  // with both not pressed.  this handles the case where ctrl and
  // alt are being used as individual modifiers rather than AltGr.
  // we note that's the case in the message sent back to deskflow
  // because there's no simple way to deduce it after the fact.
  // we have to put the dead key back first, if there was one.
  bool noAltGr = false;
  if (n == 0 && (control & 0x80) != 0 && (menu & 0x80) != 0) {
    noAltGr = true;
    PostThreadMessage(g_threadID, DESKFLOW_MSG_DEBUG, wParam | 0x05000000, lParam);
    setDeadKey(wc, 2, flags);

    BYTE keys2[256];
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
      keys2[i] = keys[i];
    }
    keys2[VK_LCONTROL] = 0;
    keys2[VK_RCONTROL] = 0;
    keys2[VK_CONTROL] = 0;
    keys2[VK_LMENU] = 0;
    keys2[VK_RMENU] = 0;
    keys2[VK_MENU] = 0;
    n = ToUnicode((UINT)wParam, scanCode, keys2, wc, 2, flags);
  }

  PostThreadMessage(
      g_threadID, DESKFLOW_MSG_DEBUG, (wc[0] & 0xffff) | ((wParam & 0xff) << 16) | ((n & 0xf) << 24) | 0x60000000,
      lParam
  );
  WPARAM charAndVirtKey = 0;
  bool clearDeadKey = false;
  switch (n) {
  default:
    // key is a dead key

    if (lParam & 0x80000000u)
      // This handles the obscure situation where a key has been
      // pressed which is both a dead key and a normal character
      // depending on which modifiers have been pressed. We
      // break here to prevent it from being considered a dead
      // key.
      break;

    g_deadVirtKey = wParam;
    g_deadLParam = lParam;
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
      g_deadKeyState[i] = keys[i];
    }
    break;

  case 0:
    // key doesn't map to a character.  this can happen if
    // non-character keys are pressed after a dead key.
    charAndVirtKey = makeKeyMsg((UINT)wParam, 0, noAltGr);
    break;

  case 1:
    // key maps to a character composed with dead key
    charAndVirtKey = makeKeyMsg((UINT)wParam, wc[0], noAltGr);
    clearDeadKey = true;
    break;

  case 2: {
    // previous dead key not composed.  send a fake key press
    // and release for the dead key to our window.
    WPARAM deadCharAndVirtKey = makeKeyMsg((UINT)g_deadVirtKey, wc[0], noAltGr);
    PostThreadMessage(g_threadID, DESKFLOW_MSG_KEY, deadCharAndVirtKey, g_deadLParam & 0x7fffffffu);
    PostThreadMessage(g_threadID, DESKFLOW_MSG_KEY, deadCharAndVirtKey, g_deadLParam | 0x80000000u);

    // use uncomposed character
    charAndVirtKey = makeKeyMsg((UINT)wParam, wc[1], noAltGr);
    clearDeadKey = true;
    break;
  }
  }

  // put back the dead key, if any, for the application to use
  if (g_deadVirtKey != 0) {
    ToUnicode((UINT)g_deadVirtKey, (g_deadLParam & 0x10ff0000u) >> 16, g_deadKeyState, wc, 2, flags);
  }

  // clear out old dead key state
  if (clearDeadKey) {
    g_deadVirtKey = 0;
    g_deadLParam = 0;
  }

  // forward message to our window.  do this whether or not we're
  // forwarding events to clients because this'll keep our thread's
  // key state table up to date.  that's important for querying
  // the scroll lock toggle state.
  // XXX -- with hot keys for actions we may only need to do this when
  // forwarding.
  if (charAndVirtKey != 0) {
    PostThreadMessage(g_threadID, DESKFLOW_MSG_DEBUG, charAndVirtKey | 0x07000000, lParam);
    PostThreadMessage(g_threadID, DESKFLOW_MSG_KEY, charAndVirtKey, lParam);
  }

  if (g_mode == kHOOK_RELAY_EVENTS) {
    // let certain keys pass through
    switch (wParam) {
    case VK_CAPITAL:
    case VK_NUMLOCK:
    case VK_SCROLL:
      // pass event on.  we want to let these through to
      // the window proc because otherwise the keyboard
      // lights may not stay synchronized.
      break;

    case VK_HANGUL:
      // pass these modifiers if using a low level hook, discard
      // them if not.
      if (g_hookThread == 0) {
        return true;
      }
      break;

    default:
      // discard
      return true;
    }
  }

  return false;
}

#if !NO_GRAB_KEYBOARD
static LRESULT CALLBACK keyboardLLHook(int code, WPARAM wParam, LPARAM lParam)
{
  if (code >= 0) {
    // decode the message
    KBDLLHOOKSTRUCT *info = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);

    bool const injected = info->flags & LLKHF_INJECTED;
    if (!g_isPrimary && injected) {
      return CallNextHookEx(g_keyboardLL, code, wParam, lParam);
    }

    WPARAM wParam = info->vkCode;
    LPARAM lParam = 1;                // repeat code
    lParam |= (info->scanCode << 16); // scan code
    if (info->flags & LLKHF_EXTENDED) {
      lParam |= (1lu << 24); // extended key
    }
    if (info->flags & LLKHF_ALTDOWN) {
      lParam |= (1lu << 29); // context code
    }
    if (info->flags & LLKHF_UP) {
      lParam |= (1lu << 31); // transition
    }
    if (info->flags & LLKHF_INJECTED) {
      lParam |= (1lu << 30); // injected event marker
    }

    // handle the message
    if (keyboardHookHandler(wParam, lParam)) {
      return 1;
    }
  }

  return CallNextHookEx(g_keyboardLL, code, wParam, lParam);
}
#endif

//
// low-level mouse hook -- this allows us to capture and handle mouse
// events very early.  the earlier the better.
//

static bool mouseHookHandler(WPARAM wParam, SInt32 x, SInt32 y, SInt32 data)
{
  switch (wParam) {
  case WM_LBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_XBUTTONDOWN:
  case WM_LBUTTONDBLCLK:
  case WM_MBUTTONDBLCLK:
  case WM_RBUTTONDBLCLK:
  case WM_XBUTTONDBLCLK:
  case WM_LBUTTONUP:
  case WM_MBUTTONUP:
  case WM_RBUTTONUP:
  case WM_XBUTTONUP:
  case WM_NCLBUTTONDOWN:
  case WM_NCMBUTTONDOWN:
  case WM_NCRBUTTONDOWN:
  case WM_NCXBUTTONDOWN:
  case WM_NCLBUTTONDBLCLK:
  case WM_NCMBUTTONDBLCLK:
  case WM_NCRBUTTONDBLCLK:
  case WM_NCXBUTTONDBLCLK:
  case WM_NCLBUTTONUP:
  case WM_NCMBUTTONUP:
  case WM_NCRBUTTONUP:
  case WM_NCXBUTTONUP:
    // always relay the event.  eat it if relaying.
    PostThreadMessage(g_threadID, DESKFLOW_MSG_MOUSE_BUTTON, wParam, data);
    return (g_mode == kHOOK_RELAY_EVENTS);

  case WM_MOUSEWHEEL:
    if (g_mode == kHOOK_RELAY_EVENTS) {
      // relay event
      PostThreadMessage(g_threadID, DESKFLOW_MSG_MOUSE_WHEEL, 0, data);
    }
    return (g_mode == kHOOK_RELAY_EVENTS);

  case WM_MOUSEHWHEEL:
    if (g_mode == kHOOK_RELAY_EVENTS) {
      // relay event
      PostThreadMessage(g_threadID, DESKFLOW_MSG_MOUSE_WHEEL, data, 0);
    }
    return (g_mode == kHOOK_RELAY_EVENTS);

  case WM_NCMOUSEMOVE:
  case WM_MOUSEMOVE:
    if (g_mode == kHOOK_RELAY_EVENTS) {
      // relay and eat event
      PostThreadMessage(g_threadID, DESKFLOW_MSG_MOUSE_MOVE, x, y);
      return true;
    } else if (g_mode == kHOOK_WATCH_JUMP_ZONE) {
      // low level hooks can report bogus mouse positions that are
      // outside of the screen.  jeez.  naturally we end up getting
      // fake motion in the other direction to get the position back
      // on the screen, which plays havoc with switch on double tap.
      // Server deals with that.  we'll clamp positions onto the
      // screen.  also, if we discard events for positions outside
      // of the screen then the mouse appears to get a bit jerky
      // near the edge.  we can either accept that or pass the bogus
      // events.  we'll try passing the events.
      bool bogus = false;
      if (x < g_xScreen) {
        x = g_xScreen;
        bogus = true;
      } else if (x >= g_xScreen + g_wScreen) {
        x = g_xScreen + g_wScreen - 1;
        bogus = true;
      }
      if (y < g_yScreen) {
        y = g_yScreen;
        bogus = true;
      } else if (y >= g_yScreen + g_hScreen) {
        y = g_yScreen + g_hScreen - 1;
        bogus = true;
      }

      // check for mouse inside jump zone
      bool inside = false;
      if (!inside && (g_zoneSides & kLeftMask) != 0) {
        inside = (x < g_xScreen + g_zoneSize);
      }
      if (!inside && (g_zoneSides & kRightMask) != 0) {
        inside = (x >= g_xScreen + g_wScreen - g_zoneSize);
      }
      if (!inside && (g_zoneSides & kTopMask) != 0) {
        inside = (y < g_yScreen + g_zoneSize);
      }
      if (!inside && (g_zoneSides & kBottomMask) != 0) {
        inside = (y >= g_yScreen + g_hScreen - g_zoneSize);
      }

      // relay the event
      PostThreadMessage(g_threadID, DESKFLOW_MSG_MOUSE_MOVE, x, y);

      // if inside and not bogus then eat the event
      return inside && !bogus;
    }
  }

  // pass the event
  return false;
}

static LRESULT CALLBACK mouseLLHook(int code, WPARAM wParam, LPARAM lParam)
{
  if (code >= 0) {
    // decode the message
    MSLLHOOKSTRUCT *info = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);

    bool const injected = info->flags & LLMHF_INJECTED;
    if (!g_isPrimary && injected) {
      return CallNextHookEx(g_mouseLL, code, wParam, lParam);
    }

    SInt32 x = static_cast<SInt32>(info->pt.x);
    SInt32 y = static_cast<SInt32>(info->pt.y);
    SInt32 w = static_cast<SInt16>(HIWORD(info->mouseData));

    // handle the message
    if (mouseHookHandler(wParam, x, y, w)) {
      return 1;
    }
  }

  return CallNextHookEx(g_mouseLL, code, wParam, lParam);
}

EHookResult MSWindowsHook::install()
{
  assert(g_getMessage == NULL || g_screenSaver);

  // must be initialized
  if (g_threadID == 0) {
    return kHOOK_FAILED;
  }

  // discard old dead keys
  g_deadVirtKey = 0;
  g_deadLParam = 0;

  // reset fake input flag
  g_fakeServerInput = false;

  // install low-level hooks.  we require that they both get installed.
  g_mouseLL = SetWindowsHookEx(WH_MOUSE_LL, &mouseLLHook, NULL, 0);
#if !NO_GRAB_KEYBOARD
  g_keyboardLL = SetWindowsHookEx(WH_KEYBOARD_LL, &keyboardLLHook, NULL, 0);
  if (g_mouseLL == NULL || g_keyboardLL == NULL) {
    if (g_keyboardLL != NULL) {
      UnhookWindowsHookEx(g_keyboardLL);
      g_keyboardLL = NULL;
    }
    if (g_mouseLL != NULL) {
      UnhookWindowsHookEx(g_mouseLL);
      g_mouseLL = NULL;
    }
  }
#endif

  // check that we got all the hooks we wanted
  if (
      (g_mouseLL == NULL) ||
#if !NO_GRAB_KEYBOARD
      (g_keyboardLL == NULL)
#endif
  ) {
    uninstall();
    return kHOOK_FAILED;
  }

  if (g_keyboardLL != NULL || g_mouseLL != NULL) {
    g_hookThread = GetCurrentThreadId();
    return kHOOK_OKAY_LL;
  }

  return kHOOK_OKAY;
}

int MSWindowsHook::uninstall()
{
  // discard old dead keys
  g_deadVirtKey = 0;
  g_deadLParam = 0;

  // uninstall hooks
  if (g_keyboardLL != NULL) {
    UnhookWindowsHookEx(g_keyboardLL);
    g_keyboardLL = NULL;
  }
  if (g_mouseLL != NULL) {
    UnhookWindowsHookEx(g_mouseLL);
    g_mouseLL = NULL;
  }
  if (g_getMessage != NULL && !g_screenSaver) {
    UnhookWindowsHookEx(g_getMessage);
    g_getMessage = NULL;
  }

  return 1;
}

static LRESULT CALLBACK getMessageHook(int code, WPARAM wParam, LPARAM lParam)
{
  if (code >= 0) {
    if (g_screenSaver) {
      MSG *msg = reinterpret_cast<MSG *>(lParam);
      if (msg->message == WM_SYSCOMMAND && msg->wParam == SC_SCREENSAVE) {
        // broadcast screen saver started message
        PostThreadMessage(g_threadID, DESKFLOW_MSG_SCREEN_SAVER, TRUE, 0);
      }
    }
  }

  return CallNextHookEx(g_getMessage, code, wParam, lParam);
}

int MSWindowsHook::installScreenSaver()
{
  // must be initialized
  if (g_threadID == 0) {
    return 0;
  }

  // generate screen saver messages
  g_screenSaver = true;

  // install hook unless it's already installed
  if (g_getMessage == NULL) {
    g_getMessage = SetWindowsHookEx(WH_GETMESSAGE, &getMessageHook, NULL, 0);
  }

  return (g_getMessage != NULL) ? 1 : 0;
}

int MSWindowsHook::uninstallScreenSaver()
{
  // uninstall hook unless the mouse wheel hook is installed
  if (g_getMessage != NULL) {
    UnhookWindowsHookEx(g_getMessage);
    g_getMessage = NULL;
  }

  // screen saver hook is no longer installed
  g_screenSaver = false;

  return 1;
}
