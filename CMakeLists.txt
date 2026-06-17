# SPDX-FileCopyrightText: (C) 2024 - 2026 Deskflow Developers
# SPDX-FileCopyrightText: (C) 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: (C) 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.24)

# Link items by fill path
cmake_policy(SET CMP0003 NEW)

# Fix define escaping
cmake_policy(SET CMP0005 NEW)

# De-duplicate libraries on linker calls
if(POLICY CMP0156)
  cmake_policy(SET CMP0156 NEW)
endif()

# Set CXX Requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project version. SYNERGY_VERSION_* constants live in extra/cmake/Version.cmake
# (single source of truth, also consumed by SaveVersion.cmake in CI); bridged to
# the upstream-named DESKFLOW_VERSION_* here so project() can keep its existing
# shape. Synergy.cmake adds the dev/snapshot/release suffix later.
include(${CMAKE_CURRENT_SOURCE_DIR}/extra/cmake/Version.cmake)
set(DESKFLOW_VERSION_MAJOR ${SYNERGY_VERSION_MAJOR})
set(DESKFLOW_VERSION_MINOR ${SYNERGY_VERSION_MINOR})
set(DESKFLOW_VERSION_PATCH ${SYNERGY_VERSION_PATCH})
set(DESKFLOW_VERSION_TWEAK 0)

# Git short SHA, consumed by VersionInfo.h.in (kVersionGitSha).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
  find_package(Git)
  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE GIT_SHA_SHORT
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()
endif()

#generate vcpkg file if needed
if(WIN32)
    option (VCPKG_QT "Use Qt from VCPKG" OFF)
    if(VCPKG_QT)
        set(QT_LIBS ", \"qttranslations\", \"qtsvg\"")
    endif()
    configure_file(cmake/vcpkg.json.in ${CMAKE_SOURCE_DIR}/vcpkg.json @ONLY)
endif()

#Define our project
project(
  deskflow
  VERSION "${DESKFLOW_VERSION_MAJOR}.${DESKFLOW_VERSION_MINOR}.${DESKFLOW_VERSION_PATCH}.${DESKFLOW_VERSION_TWEAK}"
  DESCRIPTION "Keyboard and mouse sharing utility"
  HOMEPAGE_URL "https://deskflow.org"
  LANGUAGES C CXX
)

# Define Additional "PROJECT" vars for packaging and metadata
set(CMAKE_PROJECT_PROPER_NAME "Deskflow")
set(CMAKE_PROJECT_VENDOR "${CMAKE_PROJECT_PROPER_NAME} Devs")
set(CMAKE_PROJECT_COPYRIGHT "(C) 2024-2026 ${CMAKE_PROJECT_VENDOR}")
set(CMAKE_PROJECT_CONTACT "${CMAKE_PROJECT_PROPER_NAME} <maintainers@deskflow.org>")
set(CMAKE_PROJECT_REV_FQDN "org.deskflow.deskflow")
set(CMAKE_PROJECT_DOMAIN "deskflow.org")

# Captured before any downstream brand override of CMAKE_PROJECT_NAME, for use
# by code that reads upstream-named on-disk artifacts (Qt .qm translation files).
set(UPSTREAM_PROJECT_NAME ${CMAKE_PROJECT_NAME})

# Identifier-safe form of the project version; downstream brands overwrite this.
set(CMAKE_PROJECT_VERSION_BASE "${CMAKE_PROJECT_VERSION}")

# Pull in Synergy branding overrides.
include(${CMAKE_SOURCE_DIR}/extra/cmake/Synergy.cmake)

#Unset the vars used in the project call
unset(DESKFLOW_VERSION_MAJOR)
unset(DESKFLOW_VERSION_MINOR)
unset(DESKFLOW_VERSION_PATCH)
unset(DESKFLOW_VERSION_TWEAK)

message(STATUS "Building ${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}")

# Set lib versions
set(REQUIRED_OPENSSL_VERSION 3.0)
set(REQUIRED_LIBEI_VERSION 1.0)
set(REQUIRED_LIBPORTAL_VERSION 0.8.0)
set(REQUIRED_QT_VERSION 6.4.0)

if (WIN32)
  # VSCMD_ARG_TGT_ARCH is set on CI
  if ("$ENV{VSCMD_ARG_TGT_ARCH}" STREQUAL "")
    # NOT on CI
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "[Aa][Rr][Mm]64")
      set(BUILD_ARCHITECTURE arm64)
    else()
      set(BUILD_ARCHITECTURE x64)
    endif()
  else()
    set (BUILD_ARCHITECTURE $ENV{VSCMD_ARG_TGT_ARCH})
  endif()
else()
  set (BUILD_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
endif()

if (MSVC)
  # On Windows, require that the same MSVC runtime is used as on the host.
  # Mitigates things like access violations caused by accidental ABI-compatibility breakage.
  set(REQUIRED_MSVC_RUNTIME_MAJOR 14)
  cmake_host_system_information(
    RESULT REQUIRED_MSVC_RUNTIME_MINOR
    QUERY WINDOWS_REGISTRY
    "HKLM/SOFTWARE/Microsoft/VisualStudio/${REQUIRED_MSVC_RUNTIME_MAJOR}.0/VC/Runtimes/${BUILD_ARCHITECTURE}"
    VALUE "Minor")
  if (REQUIRED_MSVC_RUNTIME_MINOR)
    message(STATUS "MSVC runtime: ${REQUIRED_MSVC_RUNTIME_MAJOR}.${REQUIRED_MSVC_RUNTIME_MINOR}")
  else()
    message(FATAL_ERROR "MSVC runtime registry entry not found")
  endif()
endif()

# Synergy overlay: anchor the runtime requirement to the bundled CRT merge module.
include(${CMAKE_SOURCE_DIR}/extra/cmake/MsvcRuntime.cmake)

# Control debug item visibility
# When not set logging is forced to DEBUG and show code locations
# Also exposes a test menu
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Disabling debug build")
  add_definitions(-DNDEBUG)
endif()

# Set Output Folders
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

# disables the use of signals,slots and emit
# Instead use Q_SIGNAL, Q_SLOT and Q_EMIT
# prevents issues when used with glib for libportal
add_definitions(-DQT_NO_KEYWORDS)

#Options for Linux platform support
if(UNIX AND NOT APPLE)
  option(BUILD_X11_SUPPORT "Build with x11 support" ON)
elseif (APPLE)
  option(BUILD_OSX_BUNDLE "Build mac os bundle" ON)
endif()

include(cmake/Libraries.cmake)
configure_libs()

if(BUILD_OSX_BUNDLE AND APPLE_CODESIGN_DEV)
  include(cmake/MacCodesign.cmake)
endif()

# setup install paths
include(GNUInstallDirs)
if (WIN32)
  set(CMAKE_INSTALL_BINDIR .)
  set(CMAKE_INSTALL_LIBDIR .)
  set(CMAKE_INSTALL_DOCDIR docs)
  set(CMAKE_INSTALL_LICENSE_DIR .)
  set(CMAKE_INSTALL_I18N_DIR translations)
elseif(BUILD_OSX_BUNDLE)
  set(CMAKE_INSTALL_BINDIR ${CMAKE_PROJECT_PROPER_NAME}.app/Contents/MacOS)
  set(CMAKE_INSTALL_LICENSE_DIR ${CMAKE_PROJECT_PROPER_NAME}.app/Contents/Resources)
  set(CMAKE_INSTALL_DOCDIR ${CMAKE_PROJECT_PROPER_NAME}.app/Contents/Resources)
else()
  set(CMAKE_INSTALL_LICENSE_DIR ${CMAKE_INSTALL_DATADIR}/licenses/${CMAKE_PROJECT_NAME})
  set(CMAKE_INSTALL_I18N_DIR ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/translations)
endif()

add_subdirectory(docs)

option(BUILD_GUI "Build GUI" ON)

# build translations before source, I18N unit tests fail if they are missing
if(BUILD_GUI)
  add_subdirectory(translations)
endif()

add_subdirectory(src)
add_subdirectory(extra)

option(BUILD_INSTALLER "Build installer" ON)
if(BUILD_INSTALLER)
    add_subdirectory(deploy)
endif()

install(
  FILES ${PROJECT_SOURCE_DIR}/LICENSE
  DESTINATION ${CMAKE_INSTALL_LICENSE_DIR}
)

install(
  FILES ${PROJECT_SOURCE_DIR}/LICENSES/LicenseRef-OpenSSL-Exception.txt
  DESTINATION ${CMAKE_INSTALL_LICENSE_DIR}
  RENAME LICENSE_EXCEPTION
)
