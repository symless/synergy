/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2026 Symless Ltd.
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

#include "platform/MSWindowsDesks.h"

#include "base/IEventQueue.h"
#include "base/IJob.h"
#include "base/Log.h"
#include "base/TMethodEventJob.h"
#include "base/TMethodJob.h"
#include "deskflow/IScreenSaver.h"
#include "deskflow/XScreen.h"
#include "deskflow/win32/AppUtilWindows.h"
#include "mt/Lock.h"
#include "mt/Thread.h"
#include "platform/MSWindowsScreen.h"
#include "platform/dfwhook.h"

#include <malloc.h>
#include <windowsx.h>

#ifndef _NTDEF_
typedef LONG NTSTATUS;
#endif
#include <hidusage.h>
#include <hidpi.h>

// these are only defined when WINVER >= 0x0500
#if !defined(SPI_GETMOUSESPEED)
#define SPI_GETMOUSESPEED 112
#endif
#if !defined(SPI_SETMOUSESPEED)
#define SPI_SETMOUSESPEED 113
#endif
#if !defined(SPI_GETSCREENSAVERRUNNING)
#define SPI_GETSCREENSAVERRUNNING 114
#endif

// X button stuff
#if !defined(WM_XBUTTONDOWN)
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_XBUTTONDBLCLK 0x020D
#define WM_NCXBUTTONDOWN 0x00AB
#define WM_NCXBUTTONUP 0x00AC
#define WM_NCXBUTTONDBLCLK 0x00AD
#define MOUSEEVENTF_XDOWN 0x0080
#define MOUSEEVENTF_XUP 0x0100
#define XBUTTON1 0x0001
#define XBUTTON2 0x0002
#endif
#if !defined(VK_XBUTTON1)
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#endif

// <unused>; <unused>
#define DESKFLOW_MSG_SWITCH DESKFLOW_HOOK_LAST_MSG + 1
// <unused>; <unused>
#define DESKFLOW_MSG_ENTER DESKFLOW_HOOK_LAST_MSG + 2
// <unused>; <unused>
#define DESKFLOW_MSG_LEAVE DESKFLOW_HOOK_LAST_MSG + 3
// wParam = flags, HIBYTE(lParam) = virtual key, LOBYTE(lParam) = scan code
#define DESKFLOW_MSG_FAKE_KEY DESKFLOW_HOOK_LAST_MSG + 4
// flags, XBUTTON id
#define DESKFLOW_MSG_FAKE_BUTTON DESKFLOW_HOOK_LAST_MSG + 5
// x; y
#define DESKFLOW_MSG_FAKE_MOVE DESKFLOW_HOOK_LAST_MSG + 6
// xDelta; yDelta
#define DESKFLOW_MSG_FAKE_WHEEL DESKFLOW_HOOK_LAST_MSG + 7
// POINT*; <unused>
#define DESKFLOW_MSG_CURSOR_POS DESKFLOW_HOOK_LAST_MSG + 8
// IKeyState*; <unused>
#define DESKFLOW_MSG_SYNC_KEYS DESKFLOW_HOOK_LAST_MSG + 9
// install; <unused>
#define DESKFLOW_MSG_SCREENSAVER DESKFLOW_HOOK_LAST_MSG + 10
// dx; dy
#define DESKFLOW_MSG_FAKE_REL_MOVE DESKFLOW_HOOK_LAST_MSG + 11
// enable; <unused>
#define DESKFLOW_MSG_FAKE_INPUT DESKFLOW_HOOK_LAST_MSG + 12
// x; y
#define DESKFLOW_MSG_FAKE_TOUCH DESKFLOW_HOOK_LAST_MSG + 13

static void send_keyboard_input(WORD wVk, WORD wScan, DWORD dwFlags)
{
  INPUT inp;
  inp.type = INPUT_KEYBOARD;
  inp.ki.wVk = (dwFlags & KEYEVENTF_UNICODE) ? 0 : wVk; // 1..254 inclusive otherwise
  inp.ki.wScan = wScan;
  inp.ki.dwFlags = dwFlags & 0xF;
  inp.ki.time = 0;
  inp.ki.dwExtraInfo = 0;
  SendInput(1, &inp, sizeof(inp));
}

static void send_mouse_input(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData)
{
  INPUT inp;
  inp.type = INPUT_MOUSE;
  inp.mi.dwFlags = dwFlags;
  inp.mi.dx = dx;
  inp.mi.dy = dy;
  inp.mi.mouseData = dwData;
  inp.mi.time = 0;
  inp.mi.dwExtraInfo = 0;
  SendInput(1, &inp, sizeof(inp));
}

//
// MSWindowsDesks
//

MSWindowsDesks::MSWindowsDesks(
    bool isPrimary, bool noHooks, const IScreenSaver *screensaver, IEventQueue *events, IJob *updateKeys,
    bool stopOnDeskSwitch
)
    : m_isPrimary(isPrimary),
      m_noHooks(noHooks),
      m_isOnScreen(m_isPrimary),
      m_deskLeaveTime(0),
      m_x(0),
      m_y(0),
      m_w(0),
      m_h(0),
      m_xCenter(0),
      m_yCenter(0),
      m_multimon(false),
      m_timer(NULL),
      m_screensaver(screensaver),
      m_screensaverNotify(false),
      m_activeDesk(NULL),
      m_activeDeskName(),
      m_mutex(),
      m_deskReady(&m_mutex, false),
      m_updateKeys(updateKeys),
      m_events(events),
      m_stopOnDeskSwitch(stopOnDeskSwitch)
{

  m_cursor = createBlankCursor();
  m_deskClass = createDeskWindowClass(m_isPrimary);
  m_keyLayout = AppUtilWindows::instance().getCurrentKeyboardLayout();
  resetOptions();
}

MSWindowsDesks::~MSWindowsDesks()
{
  disable();
  destroyClass(m_deskClass);
  destroyCursor(m_cursor);
  delete m_updateKeys;
}

void MSWindowsDesks::enable()
{
  m_threadID = GetCurrentThreadId();

  // set the active desk and (re)install the hooks
  checkDesk();

  // install the desk timer.  this timer periodically checks
  // which desk is active and reinstalls the hooks as necessary.
  // we wouldn't need this if windows notified us of a desktop
  // change but as far as i can tell it doesn't.
  m_timer = m_events->newTimer(0.2, NULL);
  m_events->adoptHandler(
      Event::kTimer, m_timer, new TMethodEventJob<MSWindowsDesks>(this, &MSWindowsDesks::handleCheckDesk)
  );

  updateKeys();
}

void MSWindowsDesks::disable()
{
  // remove timer
  if (m_timer != NULL) {
    m_events->removeHandler(Event::kTimer, m_timer);
    m_events->deleteTimer(m_timer);
    m_timer = NULL;
  }

  // destroy desks
  removeDesks();

  m_isOnScreen = m_isPrimary;
}

void MSWindowsDesks::enter()
{
  sendMessage(DESKFLOW_MSG_ENTER, 0, 0);
}

void MSWindowsDesks::leave(HKL keyLayout)
{
  sendMessage(DESKFLOW_MSG_LEAVE, (WPARAM)keyLayout, 0);
}

void MSWindowsDesks::resetOptions()
{
  m_leaveForegroundOption = false;
}

void MSWindowsDesks::setOptions(const OptionsList &options)
{
  for (UInt32 i = 0, n = (UInt32)options.size(); i < n; i += 2) {
    if (options[i] == kOptionWin32KeepForeground) {
      m_leaveForegroundOption = (options[i + 1] != 0);
      LOG((CLOG_DEBUG1 "%s the foreground window", m_leaveForegroundOption ? "don\'t grab" : "grab"));
    }
  }
}

void MSWindowsDesks::updateKeys()
{
  sendMessage(DESKFLOW_MSG_SYNC_KEYS, 0, 0);
}

void MSWindowsDesks::setShape(
    SInt32 x, SInt32 y, SInt32 width, SInt32 height, SInt32 xCenter, SInt32 yCenter, bool isMultimon
)
{
  m_x = x;
  m_y = y;
  m_w = width;
  m_h = height;
  m_xCenter = xCenter;
  m_yCenter = yCenter;
  m_multimon = isMultimon;
}

void MSWindowsDesks::installScreensaverHooks(bool install)
{
  if (m_isPrimary && m_screensaverNotify != install) {
    m_screensaverNotify = install;
    sendMessage(DESKFLOW_MSG_SCREENSAVER, install, 0);
  }
}

void MSWindowsDesks::fakeInputBegin()
{
  sendMessage(DESKFLOW_MSG_FAKE_INPUT, 1, 0);
}

void MSWindowsDesks::fakeInputEnd()
{
  sendMessage(DESKFLOW_MSG_FAKE_INPUT, 0, 0);
}

void MSWindowsDesks::getCursorPos(SInt32 &x, SInt32 &y) const
{
  POINT pos;
  sendMessage(DESKFLOW_MSG_CURSOR_POS, reinterpret_cast<WPARAM>(&pos), 0);
  x = pos.x;
  y = pos.y;
}

void MSWindowsDesks::fakeKeyEvent(WORD virtualKey, WORD scanCode, DWORD flags, bool /*isAutoRepeat*/) const
{
  sendMessage(DESKFLOW_MSG_FAKE_KEY, flags, MAKELPARAM(scanCode, virtualKey));
}

void MSWindowsDesks::fakeMouseButton(ButtonID button, bool press)
{
  // the system will swap the meaning of left/right for us if
  // the user has configured a left-handed mouse but we don't
  // want it to swap since we want the handedness of the
  // server's mouse.  so pre-swap for a left-handed mouse.
  if (GetSystemMetrics(SM_SWAPBUTTON)) {
    switch (button) {
    case kButtonLeft:
      button = kButtonRight;
      break;

    case kButtonRight:
      button = kButtonLeft;
      break;
    }
  }

  // map button id to button flag and button data
  DWORD data = 0;
  DWORD flags;
  switch (button) {
  case kButtonLeft:
    flags = press ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    break;

  case kButtonMiddle:
    flags = press ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    break;

  case kButtonRight:
    flags = press ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    break;

  case kButtonExtra0 + 0:
    data = XBUTTON1;
    flags = press ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    break;

  case kButtonExtra0 + 1:
    data = XBUTTON2;
    flags = press ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    break;

  default:
    return;
  }

  // do it
  sendMessage(DESKFLOW_MSG_FAKE_BUTTON, flags, data);
}

void MSWindowsDesks::fakeTouchClick(SInt32 x, SInt32 y) const
{
  sendMessage(DESKFLOW_MSG_FAKE_TOUCH, static_cast<WPARAM>(x), static_cast<LPARAM>(y));
}

void MSWindowsDesks::fakeMouseMove(SInt32 x, SInt32 y) const
{
  sendMessage(DESKFLOW_MSG_FAKE_MOVE, static_cast<WPARAM>(x), static_cast<LPARAM>(y));
}

void MSWindowsDesks::fakeMouseRelativeMove(SInt32 dx, SInt32 dy) const
{
  sendMessage(DESKFLOW_MSG_FAKE_REL_MOVE, static_cast<WPARAM>(dx), static_cast<LPARAM>(dy));
}

void MSWindowsDesks::fakeMouseWheel(SInt32 xDelta, SInt32 yDelta) const
{
  sendMessage(DESKFLOW_MSG_FAKE_WHEEL, xDelta, yDelta);
}

void MSWindowsDesks::sendMessage(UINT msg, WPARAM wParam, LPARAM lParam) const
{
  if (m_activeDesk != NULL && m_activeDesk->m_window != NULL) {
    PostThreadMessage(m_activeDesk->m_threadID, msg, wParam, lParam);
    waitForDesk();
  }
}

HCURSOR
MSWindowsDesks::createBlankCursor() const
{
  // create a transparent cursor
  int cw = GetSystemMetrics(SM_CXCURSOR);
  int ch = GetSystemMetrics(SM_CYCURSOR);
  UInt8 *cursorAND = new UInt8[ch * ((cw + 31) >> 2)];
  UInt8 *cursorXOR = new UInt8[ch * ((cw + 31) >> 2)];
  memset(cursorAND, 0xff, ch * ((cw + 31) >> 2));
  memset(cursorXOR, 0x00, ch * ((cw + 31) >> 2));
  HCURSOR c = CreateCursor(MSWindowsScreen::getWindowInstance(), 0, 0, cw, ch, cursorAND, cursorXOR);
  delete[] cursorXOR;
  delete[] cursorAND;
  return c;
}

void MSWindowsDesks::destroyCursor(HCURSOR cursor) const
{
  if (cursor != NULL) {
    DestroyCursor(cursor);
  }
}

ATOM MSWindowsDesks::createDeskWindowClass(bool isPrimary) const
{
  WNDCLASSEX classInfo;
  classInfo.cbSize = sizeof(classInfo);
  classInfo.style = CS_DBLCLKS | CS_NOCLOSE;
  classInfo.lpfnWndProc = isPrimary ? &MSWindowsDesks::primaryDeskProc : &MSWindowsDesks::secondaryDeskProc;
  classInfo.cbClsExtra = 0;
  classInfo.cbWndExtra = 0;
  classInfo.hInstance = MSWindowsScreen::getWindowInstance();
  classInfo.hIcon = NULL;
  classInfo.hCursor = m_cursor;
  classInfo.hbrBackground = NULL;
  classInfo.lpszMenuName = NULL;
  classInfo.lpszClassName = DESKFLOW_APP_NAME "Desk";
  classInfo.hIconSm = NULL;
  return RegisterClassEx(&classInfo);
}

void MSWindowsDesks::destroyClass(ATOM windowClass) const
{
  if (windowClass != 0) {
    UnregisterClass(MAKEINTATOM(windowClass), MSWindowsScreen::getWindowInstance());
  }
}

HWND MSWindowsDesks::createWindow(ATOM windowClass, const char *name) const
{
  HWND window = CreateWindowEx(
      WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, MAKEINTATOM(windowClass), name, WS_POPUP, 0, 0, 1, 1, NULL, NULL,
      MSWindowsScreen::getWindowInstance(), NULL
  );
  if (window == NULL) {
    LOG((CLOG_ERR "failed to create window: %d", GetLastError()));
    throw XScreenOpenFailure();
  }
  return window;
}

void MSWindowsDesks::destroyWindow(HWND hwnd) const
{
  if (hwnd != NULL) {
    DestroyWindow(hwnd);
  }
}

LRESULT CALLBACK MSWindowsDesks::primaryDeskProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_SETCURSOR:
    SetCursor(NULL);
    return TRUE;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MSWindowsDesks::secondaryDeskProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_POINTERACTIVATE:
    return PA_NOACTIVATE;

  case WM_POINTERDOWN: {
    MSWindowsDesks *self = reinterpret_cast<MSWindowsDesks *>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (self) {
      UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
      DWORD pointerType = PT_POINTER;
      GetPointerType(pointerId, &pointerType);
      if (pointerType == PT_TOUCH || pointerType == PT_PEN) {
        SInt32 x = GET_X_LPARAM(lParam);
        SInt32 y = GET_Y_LPARAM(lParam);
        LOG((CLOG_DEBUG1 "secondary WM_POINTERDOWN touch at %d,%d", x, y));
        PostThreadMessage(self->m_threadID, DESKFLOW_MSG_TOUCH,
                          static_cast<WPARAM>(x), static_cast<LPARAM>(y));
        return 0;
      }
    }
    break;
  }

  case WM_MOUSEMOVE: {
    LPARAM extraInfo = GetMessageExtraInfo();
    if ((extraInfo & TOUCH_SIGNATURE_MASK) == TOUCH_SIGNATURE)
      break;

    MSWindowsDesks *self = reinterpret_cast<MSWindowsDesks *>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (self && IsWindowVisible(hwnd)) {
      if (GetTickCount64() - self->m_deskLeaveTime < 200) {
        break;
      }
      ReleaseCapture();
      SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_HIDEWINDOW);
      HCURSOR arrow = LoadCursor(NULL, IDC_ARROW);
      SetClassLongPtr(hwnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(arrow));
      SetCursor(arrow);
    }
    break;
  }
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void MSWindowsDesks::deskMouseMove(SInt32 x, SInt32 y) const
{
  // when using absolute positioning with mouse_event(),
  // the normalized device coordinates range over only
  // the primary screen.
  SInt32 w = GetSystemMetrics(SM_CXSCREEN);
  SInt32 h = GetSystemMetrics(SM_CYSCREEN);
  send_mouse_input(
      MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, (DWORD)((65535.0f * x) / (w - 1) + 0.5f),
      (DWORD)((65535.0f * y) / (h - 1) + 0.5f), 0
  );
}

void MSWindowsDesks::deskFakeTouchClick(SInt32 x, SInt32 y) const
{
  static bool s_touchReady = false;

  if (!s_touchReady) {
    if (!InitializeTouchInjection(1, TOUCH_FEEDBACK_DEFAULT)) {
      DWORD err = GetLastError();
      LOG((CLOG_WARN "touch: InitializeTouchInjection failed, err=%d", err));
      deskMouseMove(x, y);
      send_mouse_input(MOUSEEVENTF_LEFTDOWN, 0, 0, 0);
      send_mouse_input(MOUSEEVENTF_LEFTUP, 0, 0, 0);
      return;
    }
    LOG((CLOG_DEBUG1 "touch: InitializeTouchInjection succeeded"));
    s_touchReady = true;
  }

  POINTER_TOUCH_INFO contact;
  memset(&contact, 0, sizeof(POINTER_TOUCH_INFO));

  contact.pointerInfo.pointerType = PT_TOUCH;
  contact.pointerInfo.pointerId = 0;
  contact.pointerInfo.ptPixelLocation.x = x;
  contact.pointerInfo.ptPixelLocation.y = y;

  contact.touchFlags = TOUCH_FLAG_NONE;
  contact.touchMask = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
  contact.orientation = 90;
  contact.pressure = 32000;
  contact.rcContact.left = x - 2;
  contact.rcContact.right = x + 2;
  contact.rcContact.top = y - 2;
  contact.rcContact.bottom = y + 2;

  contact.pointerInfo.pointerFlags =
      POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;

  LOG((CLOG_DEBUG1 "touch: injecting DOWN at %d,%d", x, y));

  bool injected = false;
  if (InjectTouchInput(1, &contact)) {
    Sleep(30);
    contact.pointerInfo.pointerFlags = POINTER_FLAG_UP;
    InjectTouchInput(1, &contact);
    injected = true;
    LOG((CLOG_DEBUG1 "touch: injected touch at %d,%d", x, y));
  } else {
    DWORD err = GetLastError();
    LOG((CLOG_WARN "touch: InjectTouchInput failed at %d,%d, err=%d", x, y, err));
  }

  Sleep(50);

  deskMouseMove(x, y);
  send_mouse_input(MOUSEEVENTF_LEFTDOWN, 0, 0, 0);
  send_mouse_input(MOUSEEVENTF_LEFTUP, 0, 0, 0);
  LOG((CLOG_DEBUG1 "touch: sent mouse click at %d,%d (touch injected=%d)", x, y, injected));
}

void MSWindowsDesks::deskMouseRelativeMove(SInt32 dx, SInt32 dy) const
{
  // relative moves are subject to cursor acceleration which we don't
  // want.so we disable acceleration, do the relative move, then
  // restore acceleration.  there's a slight chance we'll end up in
  // the wrong place if the user moves the cursor using this system's
  // mouse while simultaneously moving the mouse on the server
  // system.  that defeats the purpose of deskflow so we'll assume
  // that won't happen.  even if it does, the next mouse move will
  // correct the position.

  // save mouse speed & acceleration
  int oldSpeed[4];
  bool accelChanged =
      SystemParametersInfo(SPI_GETMOUSE, 0, oldSpeed, 0) && SystemParametersInfo(SPI_GETMOUSESPEED, 0, oldSpeed + 3, 0);

  // use 1:1 motion
  if (accelChanged) {
    int newSpeed[4] = {0, 0, 0, 1};
    accelChanged = SystemParametersInfo(SPI_SETMOUSE, 0, newSpeed, 0) ||
                   SystemParametersInfo(SPI_SETMOUSESPEED, 0, newSpeed + 3, 0);
  }

  // move relative to mouse position
  send_mouse_input(MOUSEEVENTF_MOVE, dx, dy, 0);

  // restore mouse speed & acceleration
  if (accelChanged) {
    SystemParametersInfo(SPI_SETMOUSE, 0, oldSpeed, 0);
    SystemParametersInfo(SPI_SETMOUSESPEED, 0, oldSpeed + 3, 0);
  }
}

// the system shows the mouse cursor when an internal display count
// is >= 0.  this count is maintained per application but there's
// apparently a system wide count added to the application's count.
// this system count is 0 if there's a mouse attached to the system
// and -1 otherwise.  the mouse keys accessibility feature can modify
// this system count by making the system appear to have a mouse.
void setCursorVisibility(bool visible)
{
  LOG_DEBUG("%s cursor", visible ? "showing" : "hiding");

  const int max = 10;
  int attempts = 0;
  while (attempts++ < max) {
    const auto displayCounter = ShowCursor(visible ? TRUE : FALSE);
    LOG_DEBUG1("cursor display counter: %d", displayCounter);

    if (visible) {
      if (displayCounter < 0) {
        LOG_DEBUG1("cursor still hidden, retrying, attempt: %d", attempts);
      } else {
        LOG_DEBUG1("cursor is now visible, attempts: %d", attempts);
        return;
      }
    } else {
      if (displayCounter >= 0) {
        LOG_DEBUG1("cursor still visible, retrying, attempt: %d", attempts);
      } else {
        LOG_DEBUG1("cursor is now hidden, attempts: %d", attempts);
        return;
      }
    }
  }

  LOG_ERR("unable to set cursor visibility after %d attempts", attempts);
}

void MSWindowsDesks::deskEnter(Desk *desk)
{
  if (!m_isPrimary) {
    ReleaseCapture();

    LONG_PTR exStyle = GetWindowLongPtr(desk->m_window, GWL_EXSTYLE);
    // let hit-testing pass through to windows underneath
    SetWindowLongPtr(desk->m_window, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
  } else {
    registerTouchRawInput(desk->m_window, false);
  }

  SetWindowPos(desk->m_window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_HIDEWINDOW);

  setCursorVisibility(true);

  HCURSOR arrow = LoadCursor(NULL, IDC_ARROW);
  SetClassLongPtr(desk->m_window, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(arrow));
  SetCursor(arrow);

  // restore the foreground window
  // XXX -- this raises the window to the top of the Z-order.  we
  // want it to stay wherever it was to properly support X-mouse
  // (mouse over activation) but i've no idea how to do that.
  // the obvious workaround of using SetWindowPos() to move it back
  // after being raised doesn't work.
  DWORD thisThread = GetWindowThreadProcessId(desk->m_window, NULL);
  DWORD thatThread = GetWindowThreadProcessId(desk->m_foregroundWindow, NULL);
  AttachThreadInput(thatThread, thisThread, TRUE);
  SetForegroundWindow(desk->m_foregroundWindow);
  AttachThreadInput(thatThread, thisThread, FALSE);
  EnableWindow(desk->m_window, desk->m_lowLevel ? FALSE : TRUE);
  desk->m_foregroundWindow = NULL;
}

void MSWindowsDesks::deskLeave(Desk *desk, HKL keyLayout)
{
  setCursorVisibility(false);

  if (m_isPrimary) {
    // map a window to hide the cursor and to use whatever keyboard
    // layout we choose rather than the keyboard layout of the last
    // active window.
    int x, y, w, h;
    if (desk->m_lowLevel) {
      // LL hook keeps the cursor pinned at center, so 1x1 is enough.
      // primaryDeskProc handles WM_SETCURSOR to force a blank cursor
      // (ShowCursor(FALSE) is unreliable on touchscreen devices).
      x = m_xCenter;
      y = m_yCenter;
      w = 1;
      h = 1;
    } else {
      // with regular hooks the cursor will jitter as it's moved
      // by the user then back to the center by us.  to be sure
      // we never lose it, cover all the monitors with the window.
      x = m_x;
      y = m_y;
      w = m_w;
      h = m_h;
    }
    SetWindowPos(desk->m_window, HWND_TOP, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // WM_SETCURSOR won't fire until the cursor moves, so force the
    // blank cursor immediately (LL hook pins cursor at center, no movement).
    SetCursor(NULL);

    // switch to requested keyboard layout
    ActivateKeyboardLayout(keyLayout, 0);

    // if not using low-level hooks we have to also activate the
    // window to ensure we don't lose keyboard focus.
    // FIXME -- see if this can be avoided.  if so then always
    // disable the window (see handling of DESKFLOW_MSG_SWITCH).
    if (!desk->m_lowLevel) {
      SetActiveWindow(desk->m_window);
    }

    // if using low-level hooks then disable the foreground window
    // so it can't mess up any of our keyboard events.  the console
    // program, for example, will cause characters to be reported as
    // unshifted, regardless of the shift key state.  interestingly
    // we do see the shift key go down and up.
    //
    // note that we must enable the window to activate it and we
    // need to disable the window on deskEnter.
    else {
      desk->m_foregroundWindow = getForegroundWindow();
      if (desk->m_foregroundWindow != NULL) {
        EnableWindow(desk->m_window, TRUE);
        SetActiveWindow(desk->m_window);
        DWORD thisThread = GetWindowThreadProcessId(desk->m_window, NULL);
        DWORD thatThread = GetWindowThreadProcessId(desk->m_foregroundWindow, NULL);

        AttachThreadInput(thatThread, thisThread, TRUE);
        SetForegroundWindow(desk->m_window);
        AttachThreadInput(thatThread, thisThread, FALSE);
      }
    }

    registerTouchRawInput(desk->m_window, true);
  } else {
    desk->m_foregroundWindow = getForegroundWindow();
    EnableWindow(desk->m_window, TRUE);

    SetClassLongPtr(desk->m_window, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(m_cursor));

    LONG_PTR exStyle = GetWindowLongPtr(desk->m_window, GWL_EXSTYLE);
    SetWindowLongPtr(desk->m_window, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);

    SetWindowPos(desk->m_window, HWND_TOPMOST, m_x, m_y, m_w, m_h, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    SetCapture(desk->m_window);

    // brief delay for the hider window's blank cursor to take effect
    ARCH->sleep(0.03);
    m_deskLeaveTime = GetTickCount64();
    deskMouseMove(m_xCenter, m_yCenter);
  }
}

MSWindowsDesks::HidTouchDevice MSWindowsDesks::initHidTouchDevice(HANDLE hDevice)
{
  HidTouchDevice dev = {};
  dev.valid = false;

  UINT ppSize = 0;
  if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, NULL, &ppSize) != 0 ||
      ppSize == 0) {
    return dev;
  }

  dev.preparsedData.resize(ppSize);
  if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA,
                            dev.preparsedData.data(), &ppSize) == static_cast<UINT>(-1)) {
    return dev;
  }

  auto pp = reinterpret_cast<PHIDP_PREPARSED_DATA>(dev.preparsedData.data());

  HIDP_CAPS caps = {};
  if (HidP_GetCaps(pp, &caps) != HIDP_STATUS_SUCCESS) {
    return dev;
  }

  std::vector<HIDP_VALUE_CAPS> valCaps(caps.NumberInputValueCaps);
  USHORT numValCaps = caps.NumberInputValueCaps;
  if (numValCaps == 0 ||
      HidP_GetValueCaps(HidP_Input, valCaps.data(), &numValCaps, pp) != HIDP_STATUS_SUCCESS) {
    return dev;
  }

  // find a link collection that has both X and Y on the generic desktop page
  struct CollectionInfo {
    bool hasX = false;
    bool hasY = false;
    LONG maxX = 0;
    LONG maxY = 0;
  };
  std::unordered_map<USHORT, CollectionInfo> collections;

  for (USHORT i = 0; i < numValCaps; ++i) {
    const auto &vc = valCaps[i];
    if (vc.UsagePage != HID_USAGE_PAGE_GENERIC)
      continue;

    USAGE usage = vc.IsRange ? vc.Range.UsageMin : vc.NotRange.Usage;
    auto &ci = collections[vc.LinkCollection];
    if (usage == HID_USAGE_GENERIC_X) {
      ci.hasX = true;
      ci.maxX = vc.LogicalMax > 0 ? vc.LogicalMax : vc.PhysicalMax;
    } else if (usage == HID_USAGE_GENERIC_Y) {
      ci.hasY = true;
      ci.maxY = vc.LogicalMax > 0 ? vc.LogicalMax : vc.PhysicalMax;
    }
  }

  for (const auto &pair : collections) {
    if (pair.second.hasX && pair.second.hasY &&
        pair.second.maxX > 0 && pair.second.maxY > 0) {
      dev.linkCollection = pair.first;
      dev.logicalMaxX = pair.second.maxX;
      dev.logicalMaxY = pair.second.maxY;
      dev.valid = true;
      LOG((CLOG_DEBUG "HID touch device: linkCollection=%d logicalMax X=%ld Y=%ld",
           dev.linkCollection, dev.logicalMaxX, dev.logicalMaxY));
      break;
    }
  }

  return dev;
}

bool MSWindowsDesks::parseHidTouch(const RAWINPUT *raw, const HidTouchDevice &dev,
                                   SInt32 &outX, SInt32 &outY)
{
  if (!dev.valid)
    return false;

  auto pp = reinterpret_cast<PHIDP_PREPARSED_DATA>(
      const_cast<BYTE *>(dev.preparsedData.data()));
  auto report = const_cast<PCHAR>(
      reinterpret_cast<const char *>(raw->data.hid.bRawData));
  ULONG reportLen = raw->data.hid.dwSizeHid;

  // check Tip Switch (digitizer page 0x0D, usage 0x42)
  USAGE usages[16] = {};
  ULONG numUsages = 16;
  if (HidP_GetUsages(HidP_Input, 0x0D, dev.linkCollection,
                     usages, &numUsages, pp,
                     report, reportLen) != HIDP_STATUS_SUCCESS) {
    numUsages = 0;
  }

  bool tipDown = false;
  for (ULONG i = 0; i < numUsages; ++i) {
    if (usages[i] == 0x42) {
      tipDown = true;
      break;
    }
  }
  if (!tipDown)
    return false;

  ULONG rawX = 0, rawY = 0;
  if (HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC,
                         dev.linkCollection, HID_USAGE_GENERIC_X,
                         &rawX, pp, report, reportLen) != HIDP_STATUS_SUCCESS) {
    return false;
  }
  if (HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC,
                         dev.linkCollection, HID_USAGE_GENERIC_Y,
                         &rawY, pp, report, reportLen) != HIDP_STATUS_SUCCESS) {
    return false;
  }

  outX = m_x + static_cast<SInt32>(rawX * m_w / dev.logicalMaxX);
  outY = m_y + static_cast<SInt32>(rawY * m_h / dev.logicalMaxY);
  return true;
}

void MSWindowsDesks::registerTouchRawInput(HWND window, bool enable)
{
  RAWINPUTDEVICE rids[4] = {};
  DWORD flags = enable ? RIDEV_INPUTSINK : RIDEV_REMOVE;
  HWND target = enable ? window : NULL;

  rids[0] = {0x0D, 0x04, flags, target};
  rids[1] = {0x0D, 0x05, flags, target};
  rids[2] = {0x0D, 0x01, flags, target};
  rids[3] = {0x0D, 0x02, flags, target};

  if (!RegisterRawInputDevices(rids, 4, sizeof(RAWINPUTDEVICE))) {
    RegisterRawInputDevices(rids, 1, sizeof(RAWINPUTDEVICE));
  }
}

void MSWindowsDesks::deskThread(void *vdesk)
{
  MSG msg;

  // use given desktop for this thread
  Desk *desk = static_cast<Desk *>(vdesk);
  desk->m_threadID = GetCurrentThreadId();
  desk->m_window = NULL;
  desk->m_foregroundWindow = NULL;
  if (desk->m_desk != NULL && SetThreadDesktop(desk->m_desk) != 0) {
    // create a message queue
    PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

    // create a window.  we use this window to hide the cursor.
    try {
      desk->m_window = createWindow(m_deskClass, DESKFLOW_APP_NAME "Desk");
      SetWindowLongPtr(desk->m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
      LOG((CLOG_DEBUG "desk %s window is 0x%08x", desk->m_name.c_str(), desk->m_window));
    } catch (...) {
      // ignore
      LOG((CLOG_DEBUG "can't create desk window for %s", desk->m_name.c_str()));
    }
  }

  // tell main thread that we're ready
  {
    Lock lock(&m_mutex);
    m_deskReady = true;
    m_deskReady.broadcast();
  }

  while (GetMessage(&msg, NULL, 0, 0)) {
    switch (msg.message) {
    default:
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      continue;

    case WM_INPUT: {
      UINT size = 0;
      GetRawInputData(
          reinterpret_cast<HRAWINPUT>(msg.lParam), RID_INPUT,
          NULL, &size, sizeof(RAWINPUTHEADER));
      if (size > 0 && size <= 1024) {
        BYTE buffer[1024];
        if (GetRawInputData(
                reinterpret_cast<HRAWINPUT>(msg.lParam), RID_INPUT,
                buffer, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1)) {
          RAWINPUT *raw = reinterpret_cast<RAWINPUT *>(buffer);
          if (raw->header.dwType == RIM_TYPEHID &&
              raw->data.hid.dwCount > 0 && raw->data.hid.dwSizeHid > 0) {
            auto it = m_hidTouchDevices.find(raw->header.hDevice);
            if (it == m_hidTouchDevices.end()) {
              it = m_hidTouchDevices.emplace(
                  raw->header.hDevice, initHidTouchDevice(raw->header.hDevice)).first;
            }

            SInt32 tx, ty;
            if (it->second.valid && parseHidTouch(raw, it->second, tx, ty)) {
              LOG((CLOG_DEBUG1 "desk raw touch at %d,%d", tx, ty));
              PostThreadMessage(m_threadID, DESKFLOW_MSG_TOUCH,
                                static_cast<WPARAM>(tx), static_cast<LPARAM>(ty));
            }
          }
        }
      }
      continue;
    }

    case DESKFLOW_MSG_SWITCH:
      if (!m_noHooks) {
        MSWindowsHook::uninstall();
        if (m_screensaverNotify) {
          MSWindowsHook::uninstallScreenSaver();
          MSWindowsHook::installScreenSaver();
        }
        switch (MSWindowsHook::install()) {
        case kHOOK_FAILED:
          // we won't work on this desk
          desk->m_lowLevel = false;
          break;

        case kHOOK_OKAY:
          desk->m_lowLevel = false;
          break;

        case kHOOK_OKAY_LL:
          desk->m_lowLevel = true;
          break;
        }

        // a window on the primary screen with low-level hooks
        // should never activate.
        if (desk->m_window)
          EnableWindow(desk->m_window, desk->m_lowLevel ? FALSE : TRUE);
      }
      break;

    case DESKFLOW_MSG_ENTER:
      m_isOnScreen = true;
      deskEnter(desk);
      break;

    case DESKFLOW_MSG_LEAVE:
      m_isOnScreen = false;
      m_keyLayout = (HKL)msg.wParam;
      deskLeave(desk, m_keyLayout);
      break;

    case DESKFLOW_MSG_FAKE_KEY:
      // Note, this is intended to be HI/LOWORD and not HI/LOBYTE
      send_keyboard_input(HIWORD(msg.lParam), LOWORD(msg.lParam), (DWORD)msg.wParam);
      break;

    case DESKFLOW_MSG_FAKE_BUTTON:
      if (msg.wParam != 0) {
        send_mouse_input((DWORD)msg.wParam, 0, 0, (DWORD)msg.lParam);
      }
      break;

    case DESKFLOW_MSG_FAKE_MOVE:
      deskMouseMove(static_cast<SInt32>(msg.wParam), static_cast<SInt32>(msg.lParam));
      break;

    case DESKFLOW_MSG_FAKE_REL_MOVE:
      deskMouseRelativeMove(static_cast<SInt32>(msg.wParam), static_cast<SInt32>(msg.lParam));
      break;

    case DESKFLOW_MSG_FAKE_TOUCH:
      deskFakeTouchClick(static_cast<SInt32>(msg.wParam), static_cast<SInt32>(msg.lParam));
      break;

    case DESKFLOW_MSG_FAKE_WHEEL:
      // XXX -- add support for x-axis scrolling
      if (msg.lParam != 0) {
        send_mouse_input(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)msg.lParam);
      }
      break;

    case DESKFLOW_MSG_CURSOR_POS: {
      POINT *pos = reinterpret_cast<POINT *>(msg.wParam);
      if (!GetCursorPos(pos)) {
        pos->x = m_xCenter;
        pos->y = m_yCenter;
      }
      break;
    }

    case DESKFLOW_MSG_SYNC_KEYS:
      m_updateKeys->run();
      break;

    case DESKFLOW_MSG_SCREENSAVER:
      if (!m_noHooks) {
        if (msg.wParam != 0) {
          MSWindowsHook::installScreenSaver();
        } else {
          MSWindowsHook::uninstallScreenSaver();
        }
      }
      break;

    case DESKFLOW_MSG_FAKE_INPUT:
      send_keyboard_input(
          DESKFLOW_HOOK_FAKE_INPUT_VIRTUAL_KEY, DESKFLOW_HOOK_FAKE_INPUT_SCANCODE, msg.wParam ? 0 : KEYEVENTF_KEYUP
      );
      break;
    }

    // notify that message was processed
    Lock lock(&m_mutex);
    m_deskReady = true;
    m_deskReady.broadcast();
  }

  // clean up
  deskEnter(desk);
  if (desk->m_window != NULL) {
    DestroyWindow(desk->m_window);
  }
  if (desk->m_desk != NULL) {
    closeDesktop(desk->m_desk);
  }
}

MSWindowsDesks::Desk *MSWindowsDesks::addDesk(const String &name, HDESK hdesk)
{
  Desk *desk = new Desk;
  desk->m_name = name;
  desk->m_desk = hdesk;
  desk->m_targetID = GetCurrentThreadId();
  desk->m_thread = new Thread(new TMethodJob<MSWindowsDesks>(this, &MSWindowsDesks::deskThread, desk));
  waitForDesk();
  m_desks.insert(std::make_pair(name, desk));
  return desk;
}

void MSWindowsDesks::removeDesks()
{
  for (Desks::iterator index = m_desks.begin(); index != m_desks.end(); ++index) {
    Desk *desk = index->second;
    PostThreadMessage(desk->m_threadID, WM_QUIT, 0, 0);
    desk->m_thread->wait();
    delete desk->m_thread;
    delete desk;
  }
  m_desks.clear();
  m_activeDesk = NULL;
  m_activeDeskName = "";
}

void MSWindowsDesks::checkDesk()
{
  // get current desktop.  if we already know about it then return.
  Desk *desk;
  HDESK hdesk = openInputDesktop();
  String name = getDesktopName(hdesk);
  Desks::const_iterator index = m_desks.find(name);
  if (index == m_desks.end()) {
    desk = addDesk(name, hdesk);
    // hold on to hdesk until thread exits so the desk can't
    // be removed by the system
  } else {
    closeDesktop(hdesk);
    desk = index->second;
  }

  // if we are told to shut down on desk switch, and this is not the
  // first switch, then shut down.
  if (m_stopOnDeskSwitch && m_activeDesk != NULL && name != m_activeDeskName) {
    LOG((CLOG_DEBUG "shutting down because of desk switch to \"%s\"", name.c_str()));
    m_events->addEvent(Event(Event::kQuit));
    return;
  }

  // if active desktop changed then tell the old and new desk threads
  // about the change.  don't switch desktops when the screensaver is
  // active becaue we'd most likely switch to the screensaver desktop
  // which would have the side effect of forcing the screensaver to
  // stop.
  if (name != m_activeDeskName && !m_screensaver->isActive()) {
    // show cursor on previous desk
    bool wasOnScreen = m_isOnScreen;
    if (!wasOnScreen) {
      sendMessage(DESKFLOW_MSG_ENTER, 0, 0);
    }

    // always sync keys when switching desks to ensure keyboard modifier
    // states are correct.
    LOG_DEBUG("switched to desk \"%s\"", name.c_str());
    bool syncKeys = false;
    if (isDeskAccessible(desk)) {
      LOG_DEBUG("desktop is accessible - syncing keyboard state after desk switch");
      syncKeys = true;
    } else {
      LOG_DEBUG("desktop is inaccessible");
    }

    // switch desk
    m_activeDesk = desk;
    m_activeDeskName = name;
    sendMessage(DESKFLOW_MSG_SWITCH, 0, 0);

    // hide cursor on new desk
    if (!wasOnScreen) {
      sendMessage(DESKFLOW_MSG_LEAVE, (WPARAM)m_keyLayout, 0);
    }

    // update keys if necessary
    if (syncKeys) {
      updateKeys();
    }
  } else if (name != m_activeDeskName) {
    // screen saver might have started
    PostThreadMessage(m_threadID, DESKFLOW_MSG_SCREEN_SAVER, TRUE, 0);
  }
}

bool MSWindowsDesks::isDeskAccessible(const Desk *desk) const
{
  return (desk != NULL && desk->m_desk != NULL);
}

void MSWindowsDesks::waitForDesk() const
{
  MSWindowsDesks *self = const_cast<MSWindowsDesks *>(this);

  Lock lock(&m_mutex);
  while (!(bool)m_deskReady) {
    m_deskReady.wait();
  }
  self->m_deskReady = false;
}

void MSWindowsDesks::handleCheckDesk(const Event &, void *)
{
  checkDesk();

  // also check if screen saver is running if on a modern OS and
  // this is the primary screen.
  if (m_isPrimary) {
    BOOL running;
    SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &running, FALSE);
    PostThreadMessage(m_threadID, DESKFLOW_MSG_SCREEN_SAVER, running, 0);
  }
}

HDESK
MSWindowsDesks::openInputDesktop()
{
  return OpenInputDesktop(DF_ALLOWOTHERACCOUNTHOOK, TRUE, DESKTOP_CREATEWINDOW | DESKTOP_HOOKCONTROL | GENERIC_WRITE);
}

void MSWindowsDesks::closeDesktop(HDESK desk)
{
  if (desk != NULL) {
    CloseDesktop(desk);
  }
}

String MSWindowsDesks::getDesktopName(HDESK desk)
{
  if (desk == NULL) {
    return String();
  } else {
    DWORD size;
    GetUserObjectInformation(desk, UOI_NAME, NULL, 0, &size);
    TCHAR *name = (TCHAR *)alloca(size + sizeof(TCHAR));
    GetUserObjectInformation(desk, UOI_NAME, name, size, &size);
    String result(name);
    return result;
  }
}

HWND MSWindowsDesks::getForegroundWindow() const
{
  // Ideally we'd return NULL as much as possible, only returning
  // the actual foreground window when we know it's going to mess
  // up our keyboard input.  For now we'll just let the user
  // decide.
  if (m_leaveForegroundOption) {
    return NULL;
  }
  return GetForegroundWindow();
}
