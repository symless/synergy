# Deskflow -- mouse and keyboard sharing utility
# Copyright (C) 2024 - 2025 Symless Ltd.
#
# This package is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# found in the file LICENSE that should have accompanied this file.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.22)

project(deskflow C CXX)

set(SYNERGY_EXTRA_ROOT "${CMAKE_SOURCE_DIR}/ext/synergy-extra")
set(SYNERGY_EXTRA_FILE "${SYNERGY_EXTRA_ROOT}/cmake/Extra.cmake")
if (NOT EXISTS ${SYNERGY_EXTRA_FILE})
  execute_process(
    COMMAND git submodule update --init
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE GIT_SUBMODULE_RESULT
  )
  if (NOT EXISTS ${SYNERGY_EXTRA_FILE})
    message(FATAL_ERROR "Unable to init submodule. Run manually: git submodule update --init")
  endif()
endif()

# Include 'Synergy Extra' first to control the cached variables.
include(${SYNERGY_EXTRA_FILE})

include(cmake/Version.cmake)
include(cmake/Definitions.cmake)
include(cmake/Build.cmake)
include(cmake/Libraries.cmake)
include(cmake/Packaging.cmake)

set_version()
configure_definitions()
configure_build()
configure_libs()
configure_packaging()

add_subdirectory(${SYNERGY_EXTRA_ROOT})
add_subdirectory(src)
