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

#pragma once

#include "base/EventTypes.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

/// Handles touch click injection for the touch-to-switch feature.
///
/// Responsible for:
/// - Activating the window at a given screen coordinate
/// - Injecting a touch (or mouse) click at that coordinate
///
/// All methods must be called from the desk thread.
class MSWindowsTouchInjector
{
public:
  MSWindowsTouchInjector();
  ~MSWindowsTouchInjector();

  /// Activate the window at (x, y) and inject a click there.
  /// Uses InjectTouchInput if available, falls back to mouse_event.
  /// Must be called from the desk thread.
  void injectClickAt(SInt32 x, SInt32 y);

private:
  /// Try to bring the window at (x, y) to the foreground.
  /// Uses SetWinEventHook to wait for activation event-driven.
  /// Returns the HWND that was activated, or NULL.
  HWND activateWindowAt(SInt32 x, SInt32 y);

  /// Wait for a window to become foreground using SetWinEventHook.
  /// Returns true if the target window became foreground within the timeout.
  bool waitForForeground(HWND target, DWORD timeoutMs);

  /// Inject a touch down+up at (x, y) using InjectTouchInput.
  /// Returns true if successful.
  bool injectTouch(SInt32 x, SInt32 y);

  /// Inject a click at (x, y) using mouse_event as fallback.
  void injectMouseClick(SInt32 x, SInt32 y);

  /// Initialize the touch injection API (lazy, called once).
  void ensureTouchInitialized();

  bool m_touchInitialized = false;
  bool m_touchAvailable = false;

  /// SetWinEventHook callback context (thread-local by design).
  static void CALLBACK foregroundEventProc(
      HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD eventThread, DWORD eventTime
  );
  static HWND s_foregroundTarget;
  static bool s_foregroundArrived;
};
