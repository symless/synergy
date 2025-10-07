# Deskflow -- mouse and keyboard sharing utility
# Copyright (C) 2012-2024 Symless Ltd.
# Copyright (C) 2009-2012 Nick Bolton
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

# Either get the version number from the environment or from the VERSION file.
# On Windows, we also set a special 4-digit MSI version number.
macro(set_version)

  include(${SYNERGY_EXTRA_ROOT}/cmake/Version.cmake)
  version_from_git_tags(VERSION VERSION_MAJOR VERSION_MINOR VERSION_PATCH VERSION_REVISION)
  set(DESKFLOW_VERSION "${VERSION}")

  set(version_file "${CMAKE_BINARY_DIR}/VERSION")
  file(WRITE ${version_file} ${VERSION})
  message(VERBOSE "Version file output: ${version_file}")

  message(STATUS "Version number (semver): " ${DESKFLOW_VERSION})

  # Arch does not support SemVer or DEB/RPM version format, so use the four-part
  # version format which funnily enough is what Microsoft requires for MSI.
  set(DESKFLOW_VERSION_FOUR_PART "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_REVISION}")
  message(STATUS "Version number (4-part): ${DESKFLOW_VERSION_FOUR_PART}")

  # Useful for copyright (e.g. in macOS bundle .plist.in and Windows version .rc
  # file)
  string(TIMESTAMP DESKFLOW_BUILD_YEAR "%Y")

  if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set_windows_version()
  elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set_linux_version()
  endif()

endmacro()

# MSI requires a 4-digit number and doesn't accept semver.
macro(set_windows_version)

  # Dot-separated version number for MSI and Windows version .rc file.
  set(DESKFLOW_VERSION_MS ${DESKFLOW_VERSION_FOUR_PART})

  # CSV version number for Windows version .rc file.
  set(DESKFLOW_VERSION_MS_CSV
      "${VERSION_MAJOR},${VERSION_MINOR},${VERSION_PATCH},${VERSION_REVISION}")
  message(VERBOSE "Version number for (Microsoft CSV): "
          ${DESKFLOW_VERSION_MS_CSV})

endmacro()

macro(set_linux_version)

  # Replace the first occurrence of '-' with '~' for Linux versioning; the '-'
  # char is reserved for use at at the end of the version string to indicate a
  # package revision. Debian has always used this convention, but support for
  # this was also introduced in RPM 4.10.0.
  string(REGEX REPLACE "-" "~" DESKFLOW_VERSION_LINUX "${DESKFLOW_VERSION}")
  message(STATUS "Version number (DEB/RPM): ${DESKFLOW_VERSION_LINUX}")

endmacro()
