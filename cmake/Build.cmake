# Deskflow -- mouse and keyboard sharing utility
# Copyright (C) 2024 Symless Ltd.
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

macro(configure_build)

  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

  if(APPLE)
    message(STATUS "Configuring for Apple")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
  endif()

  warnings_as_errors()
  set_build_date()

endmacro()

macro(warnings_as_errors)
  if(WIN32)
    message(STATUS "Enabling warnings as errors (MSVC)")
    add_compile_options(/WX)
  elseif(UNIX)
    message(STATUS "Enabling warnings as errors (GNU/Clang)")
    add_compile_options(-Werror)
  endif()
endmacro()

macro(set_build_date)
  # Since CMake 3.8.0, `string(TIMESTAMP ...)` respects `SOURCE_DATE_EPOCH` env var if set,
  # which allows package maintainers to create reproducible builds.
  # We require CMake 3.8.0 in the root `CMakeLists.txt` for this reason.
  string(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)
  message(STATUS "Build date: ${BUILD_DATE}")
  add_definitions(-DBUILD_DATE="${BUILD_DATE}")
endmacro()
