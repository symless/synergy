# SPDX-FileCopyrightText: (C) 2012 - 2026 Synergy App Ltd
# SPDX-License-Identifier: MIT

# Must be included after deskflow's project() call and the CMAKE_PROJECT_*
# defaults are set, so these overrides take effect.
set(CMAKE_PROJECT_PROPER_NAME "Synergy")
set(CMAKE_PROJECT_VENDOR "Synergy App Ltd")
set(CMAKE_PROJECT_COPYRIGHT "(C) 2012-2026 ${CMAKE_PROJECT_VENDOR}")
set(CMAKE_PROJECT_CONTACT "${CMAKE_PROJECT_PROPER_NAME} <support@synergyapp.io>")
set(CMAKE_PROJECT_REV_FQDN "com.symless.synergy")
set(CMAKE_PROJECT_DOMAIN "synergyapp.io")
set(CMAKE_PROJECT_HOMEPAGE_URL "https://synergyapp.io")

# Display brand. "Synergy 1" is the default user-facing name (window title,
# About dialog). When building as the Core, flip to "Synergy Core" so the
# same codebase ships under a different product label.
# Distinct from CMAKE_PROJECT_PROPER_NAME, which stays "Synergy" to keep file paths
# (~/.config/Synergy/, Synergy.conf) and Windows globals space-free.
# A fork shipping this code as its own product names itself with
# SYNERGY_PRODUCT_NAME, which wins over the flavor default.
option(SYNERGY_CORE_FLAVOR "Build as Synergy Core" OFF)
set(SYNERGY_PRODUCT_NAME "" CACHE STRING "User-facing product name")
if(SYNERGY_PRODUCT_NAME)
  set(SYNERGY_DISPLAY_NAME "${SYNERGY_PRODUCT_NAME}")
elseif(SYNERGY_CORE_FLAVOR)
  set(SYNERGY_DISPLAY_NAME "Synergy Core")
else()
  set(SYNERGY_DISPLAY_NAME "Synergy 1")
endif()
add_compile_definitions(SYNERGY_DISPLAY_NAME="${SYNERGY_DISPLAY_NAME}")

# Single source of truth for the minimum macOS version. Synergy is long-term
# stable (unlike upstream, which tracks recent macOS), so we target the oldest
# macOS the linked Qt 6.x supports, the same value for every architecture.
# Both the build (here) and the packaged app's advertised minimum
# (apps/gui-electron/dist/package.config.js, which reads this line) derive from
# this one value, so they can't drift. CI must NOT pass -DCMAKE_OSX_DEPLOYMENT_TARGET.
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12")
endif()

# Core flavor seeds headless-build defaults (no GUI, no tests, no installer).
# No `FORCE` on the cache writes: the seed only fills empty slots, so a user
# passing -DBUILD_GUI=ON alongside the flavor flag still wins.
if(SYNERGY_CORE_FLAVOR)
  set(BUILD_GUI OFF CACHE BOOL "Build GUI")
  set(BUILD_TESTS OFF CACHE BOOL "Build tests")
  set(BUILD_INSTALLER OFF CACHE BOOL "Build installer")
  # On macOS the core normally nests into the GUI .app bundle
  # ($<TARGET_BUNDLE_CONTENT_DIR:Synergy>), but with BUILD_GUI=OFF that target
  # doesn't exist and cmake generation fails. Headless builds ship the core
  # binary directly, so disable the bundle too.
  set(BUILD_OSX_BUNDLE OFF CACHE BOOL "Build mac os bundle")
endif()

# Don't run unit tests as part of the build. Devs can opt back in with
# -DSKIP_BUILD_TESTS=OFF if they want post-build ctest invocation.
set(SKIP_BUILD_TESTS ON CACHE BOOL "Skip build time test")

# Resource paths consumed by extra/src/lib/synergy/gui/CMakeLists.txt.
set(GUI_RES_DIR "${CMAKE_SOURCE_DIR}/extra/src/apps/res")
set(GUI_QRC_FILE "${GUI_RES_DIR}/synergy.qrc")

# Override deskflow's project name. This cascades into binary names
# (${CMAKE_PROJECT_NAME}-core, etc.), install paths, package names,
# translation file naming, and CPack metadata. Source files in src/apps/*/
# are patched to use literal filenames since they previously assumed
# target name == source basename.
set(CMAKE_PROJECT_NAME synergy)

# Prefix of the distribution filename only. A fork that ships this same code
# under its own product name sets it so its downloads are told apart from the
# standard ones; CMAKE_PROJECT_NAME cannot carry that, because it also names
# the binaries, the install paths and the user's config directory.
set(SYNERGY_PACKAGE_PREFIX "${CMAKE_PROJECT_NAME}" CACHE STRING "Distribution filename prefix")

# Synergy version. Base semver lives in ./VERSION (read by the root CMakeLists.txt);
# composition rules — dev/snapshot/release suffix, rev count — are shared with
# extra/cmake/SaveVersion.cmake via synergy_compute_version() so the CI-side
# version (used in package filenames, S3 paths, etc.) matches what the binaries
# report. Default mode is dev; flip with -DSYNERGY_VERSION_RELEASE=ON or
# -DSYNERGY_VERSION_SNAPSHOT=ON for CI/release builds.
option(SYNERGY_VERSION_RELEASE "Release version" OFF)
option(SYNERGY_VERSION_SNAPSHOT "Snapshot version" OFF)

include(${CMAKE_CURRENT_LIST_DIR}/Version.cmake)
synergy_compute_version("${CMAKE_SOURCE_DIR}"
  CMAKE_PROJECT_VERSION
  CMAKE_PROJECT_VERSION_TWEAK
  CMAKE_PROJECT_VERSION_BASE
)
set(CMAKE_PROJECT_VERSION_MAJOR ${SYNERGY_VERSION_MAJOR})
set(CMAKE_PROJECT_VERSION_MINOR ${SYNERGY_VERSION_MINOR})
set(CMAKE_PROJECT_VERSION_PATCH ${SYNERGY_VERSION_PATCH})

# Human-facing version. The composed version already carries its build metadata
# (dev: +<sha>, snapshot: +rN), so it is the display string as-is. Snapshot is the
# exception: +rN is a rev count, not the commit, so append the short sha for
# traceability. Dev already embeds the sha (don't double it); release stays clean.
# Consumed by VersionInfo.h.in (kDisplayVersion).
if(SYNERGY_VERSION_SNAPSHOT AND GIT_SHA_SHORT)
  set(CMAKE_PROJECT_VERSION_DISPLAY "${CMAKE_PROJECT_VERSION} (${GIT_SHA_SHORT})")
else()
  set(CMAKE_PROJECT_VERSION_DISPLAY "${CMAKE_PROJECT_VERSION}")
endif()

if(NOT SYNERGY_VERSION_RELEASE AND NOT SYNERGY_VERSION_SNAPSHOT)
  add_compile_definitions(SYNERGY_VERSION_DEV)
endif()

# Build mode picks the default only: distributable builds compile activation in,
# dev builds opt in at runtime via Synergy.test.conf (licensing=true) so local
# iteration isn't gated on a serial key. A keyless flavor passes
# -DSYNERGY_ENABLE_ACTIVATION=OFF, and option() leaves an already-cached value
# alone, so build mode cannot undo it.
if(SYNERGY_VERSION_RELEASE OR SYNERGY_VERSION_SNAPSHOT)
  option(SYNERGY_ENABLE_ACTIVATION "Compile in serial key activation" ON)
else()
  option(SYNERGY_ENABLE_ACTIVATION "Compile in serial key activation" OFF)
endif()
if(SYNERGY_ENABLE_ACTIVATION)
  add_compile_definitions(SYNERGY_ENABLE_ACTIVATION)
endif()

# A flavor that ships without telemetry compiles the update check out rather than
# defaulting its setting off, because a settings file migrated from another
# edition can carry the setting forward as enabled.
option(SYNERGY_VERSION_CHECK "Compile in the GUI update check" ON)
if(SYNERGY_VERSION_CHECK)
  add_compile_definitions(SYNERGY_VERSION_CHECK)
endif()
