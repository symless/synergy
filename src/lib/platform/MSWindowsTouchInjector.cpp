/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Symless Ltd.
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

#include "platform/MSWindowsTouchInjector.h"

#include "base/Log.h"

// Required for InjectTouchInput / InitializeTouchInjection
#include <WinUser.h>

// Thread-local state for SetWinEventHook callback
HWND MSWindowsTouchInjector::s_foregroundTarget = NULL;
bool MSWindowsTouchInjector::s_foregroundArrived = false;

MSWindowsTouchInjector::MSWindowsTouchInjector()
{
}

MSWindowsTouchInjector::~MSWindowsTouchInjector()
{
}

void MSWindowsTouchInjector::injectClickAt(SInt32 x, SInt32 y)
{
  LOG((CLOG_DEBUG "touch injector: click at %d,%d", x, y));

  HWND activated = activateWindowAt(x, y);
  if (activated == NULL) {
    LOG((CLOG_DEBUG "touch injector: no window at %d,%d", x, y));
    return;
  }

  // try touch injection first, fall back to mouse
  if (!injectTouch(x, y)) {
    LOG((CLOG_DEBUG "touch injector: touch failed, falling back to mouse click"));
    injectMouseClick(x, y);
  }
}

HWND MSWindowsTouchInjector::activateWindowAt(SInt32 x, SInt32 y)
{
  POINT pt = {x, y};
  HWND child = WindowFromPoint(pt);
  if (child == NULL) {
    LOG((CLOG_DEBUG "touch injector: WindowFromPoint returned NULL for %d,%d", x, y));
    return NULL;
  }

  // find the top-level (root) window
  HWND root = GetAncestor(child, GA_ROOT);
  if (root == NULL) {
    root = child;
  }

  LOG((CLOG_DEBUG "touch injector: activating window 0x%08x (root of 0x%08x) at %d,%d", root, child, x, y));

  // try to set foreground directly first
  DWORD currentThread = GetCurrentThreadId();
  DWORD targetThread = GetWindowThreadProcessId(root, NULL);

  AttachThreadInput(currentThread, targetThread, TRUE);
  BOOL setResult = SetForegroundWindow(root);
  AttachThreadInput(currentThread, targetThread, FALSE);

  if (setResult && GetForegroundWindow() == root) {
    LOG((CLOG_DEBUG "touch injector: SetForegroundWindow succeeded immediately"));
    return root;
  }

  // if direct activation failed, wait event-driven using SetWinEventHook
  LOG((CLOG_DEBUG "touch injector: waiting for foreground activation via hook"));
  if (waitForForeground(root, 300)) {
    LOG((CLOG_DEBUG "touch injector: foreground activation confirmed via hook"));
  } else {
    LOG((CLOG_DEBUG "touch injector: foreground wait timed out, proceeding anyway"));
  }

  return root;
}

bool MSWindowsTouchInjector::waitForForeground(HWND target, DWORD timeoutMs)
{
  // set up thread-local callback state
  s_foregroundTarget = target;
  s_foregroundArrived = false;

  // check if already foreground
  if (GetForegroundWindow() == target) {
    return true;
  }

  // install an in-process hook for EVENT_SYSTEM_FOREGROUND
  HWINEVENTHOOK hook = SetWinEventHook(
      EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, foregroundEventProc, 0, 0,
      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
  );

  if (hook == NULL) {
    LOG((CLOG_WARN "touch injector: SetWinEventHook failed: %d", GetLastError()));
    return false;
  }

  // pump messages while waiting — keeps desk thread responsive
  DWORD start = GetTickCount();
  while (!s_foregroundArrived) {
    DWORD elapsed = GetTickCount() - start;
    if (elapsed >= timeoutMs) {
      break;
    }

    DWORD remaining = timeoutMs - elapsed;
    DWORD result = MsgWaitForMultipleObjects(0, NULL, FALSE, remaining, QS_ALLEVENTS);
    if (result == WAIT_OBJECT_0) {
      // messages available — process them so the hook callback fires
      MSG msg;
      while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
    // WAIT_TIMEOUT or other — check loop condition
  }

  UnhookWinEvent(hook);

  return s_foregroundArrived;
}

void CALLBACK MSWindowsTouchInjector::foregroundEventProc(
    HWINEVENTHOOK /*hook*/, DWORD event, HWND hwnd, LONG idObject, LONG /*idChild*/, DWORD /*eventThread*/,
    DWORD /*eventTime*/
)
{
  if (event == EVENT_SYSTEM_FOREGROUND && idObject == OBJID_WINDOW) {
    if (hwnd == s_foregroundTarget || s_foregroundTarget == NULL) {
      s_foregroundArrived = true;
    }
  }
}

void MSWindowsTouchInjector::ensureTouchInitialized()
{
  if (m_touchInitialized) {
    return;
  }

  m_touchInitialized = true;

  // InitializeTouchInjection requires Windows 8+
  if (InitializeTouchInjection(1, TOUCH_FEEDBACK_DEFAULT)) {
    m_touchAvailable = true;
    LOG((CLOG_DEBUG "touch injector: InitializeTouchInjection succeeded"));
  } else {
    m_touchAvailable = false;
    LOG((CLOG_WARN "touch injector: InitializeTouchInjection failed: %d", GetLastError()));
  }
}

bool MSWindowsTouchInjector::injectTouch(SInt32 x, SInt32 y)
{
  ensureTouchInitialized();

  if (!m_touchAvailable) {
    return false;
  }

  POINTER_TOUCH_INFO contact = {};
  contact.pointerInfo.pointerType = PT_TOUCH;
  contact.pointerInfo.pointerId = 0;
  contact.pointerInfo.ptPixelLocation.x = x;
  contact.pointerInfo.ptPixelLocation.y = y;
  contact.touchFlags = TOUCH_FLAG_NONE;
  contact.touchMask = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
  contact.orientation = 90;
  contact.pressure = 512;

  // contact area — a small rectangle around the point
  contact.rcContact.left = x - 2;
  contact.rcContact.right = x + 2;
  contact.rcContact.top = y - 2;
  contact.rcContact.bottom = y + 2;

  // inject touch down
  contact.pointerInfo.pointerFlags = POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
  if (!InjectTouchInput(1, &contact)) {
    LOG((CLOG_WARN "touch injector: InjectTouchInput DOWN failed: %d", GetLastError()));
    return false;
  }

  LOG((CLOG_DEBUG "touch injector: touch DOWN at %d,%d", x, y));

  // Windows requires a short delay between touch down and up
  Sleep(20);

  // inject touch up
  contact.pointerInfo.pointerFlags = POINTER_FLAG_UP;
  if (!InjectTouchInput(1, &contact)) {
    LOG((CLOG_WARN "touch injector: InjectTouchInput UP failed: %d", GetLastError()));
    // down succeeded but up failed — not much we can do
    return true;
  }

  LOG((CLOG_DEBUG "touch injector: touch UP at %d,%d", x, y));
  return true;
}

void MSWindowsTouchInjector::injectMouseClick(SInt32 x, SInt32 y)
{
  // move cursor to position
  SInt32 w = GetSystemMetrics(SM_CXSCREEN);
  SInt32 h = GetSystemMetrics(SM_CYSCREEN);
  DWORD nx = static_cast<DWORD>((65535.0f * x) / (w - 1) + 0.5f);
  DWORD ny = static_cast<DWORD>((65535.0f * y) / (h - 1) + 0.5f);

  mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, nx, ny, 0, 0);
  mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);

  Sleep(20);

  mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

  LOG((CLOG_DEBUG "touch injector: mouse click at %d,%d", x, y));
}
