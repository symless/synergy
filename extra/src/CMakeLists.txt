# SPDX-FileCopyrightText: (C) 2012 - 2026 Synergy App Ltd
# SPDX-License-Identifier: MIT

add_subdirectory(lib)

if(BUILD_TESTS)
  add_subdirectory(unittests)
endif()
