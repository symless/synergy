# SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
# SPDX-License-Identifier: MIT

# Included from CMakeLists.txt straight after Deskflow's version lines. This keeps
# Deskflow's version as a variable of its own, then replaces the numbers project()
# is about to read with the core's own version from Version.cmake.
set(SYNERGY_DESKFLOW_VERSION "${DESKFLOW_VERSION_MAJOR}.${DESKFLOW_VERSION_MINOR}")

include(${CMAKE_CURRENT_LIST_DIR}/Version.cmake)
set(DESKFLOW_VERSION_MAJOR ${SYNERGY_VERSION_MAJOR})
set(DESKFLOW_VERSION_MINOR ${SYNERGY_VERSION_MINOR})
set(DESKFLOW_VERSION_PATCH ${SYNERGY_VERSION_PATCH})
set(DESKFLOW_VERSION_TWEAK 0)
