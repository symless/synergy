/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
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

#include "arch/Arch.h"
#include "base/EventQueue.h"
#include "base/Log.h"
#include "deskflow/ServerApp.h"

#if SYSAPI_WIN32
#include "arch/win32/ArchMiscWindows.h"
#endif

#if WINAPI_MSWINDOWS
#include "MSWindowsServerTaskBarReceiver.h"
#elif WINAPI_XWINDOWS
#include "XWindowsServerTaskBarReceiver.h"
#elif WINAPI_CARBON
#include "OSXServerTaskBarReceiver.h"
#else
#error Platform not supported.
#endif

int main(int argc, char **argv)
{
#if SYSAPI_WIN32
  // record window instance for tray icon, etc
  ArchMiscWindows::setInstanceWin32(GetModuleHandle(NULL));
#endif

  Arch arch;
  arch.init();

  Log log;
  EventQueue events;

  // HACK: the `--active-desktop` arg actually belongs in the `deskflow-core` binary,
  // but we are placing it here in the server binary temporarily until we are ready to
  // ship the `deskflow-core` binary. we are deliberately not integrating `--active-desktop`
  // into the existing `ServerApp` arg parsing code as that would be a waste of time.
#if SYSAPI_WIN32
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    // This is called by the daemon (running in session 0) when it needs to know the name of the
    // interactive desktop.
    // It is necessary to run a utility process because the daemon runs in session 0, which does not
    // have access to the active desktop, and so cannot query it's name.
    if (arg == "--active-desktop") {
      const auto name = ArchMiscWindows::getActiveDesktopName();
      if (name.empty()) {
        LOG((CLOG_CRIT "failed to get active desktop name"));
        return kExitFailed;
      }

      LOG((CLOG_PRINT "%s", name.c_str()));
      return kExitSuccess;
    }
  }
#endif

  ServerApp app(&events, createTaskBarReceiver);
  return app.run(argc, argv);
}
