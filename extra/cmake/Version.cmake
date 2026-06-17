# SPDX-FileCopyrightText: (C) 2012 - 2026 Synergy App Ltd
# SPDX-License-Identifier: MIT

# Synergy version: single source of truth, shared between the build (root
# CMakeLists.txt → Synergy.cmake) and CI (SaveVersion.cmake run via cmake -P).
# Bump these constants to release a new version.
set(SYNERGY_VERSION_MAJOR 1)
set(SYNERGY_VERSION_MINOR 21)
set(SYNERGY_VERSION_PATCH 0)
set(SYNERGY_VERSION_STAGE "beta")

# Composes the full version string by applying SYNERGY_VERSION_RELEASE /
# SYNERGY_VERSION_SNAPSHOT semantics on top of the SYNERGY_VERSION_MAJOR/MINOR/PATCH
# constants above, plus a git-derived revision count for snapshot/dev builds:
#
#   RELEASE       → "X.Y.Z[-STAGE]"             (STAGE = SYNERGY_VERSION_STAGE, e.g. beta1)
#   SNAPSHOT      → "X.Y.Z[-STAGE]-snapshot+rN"  (N = commits since last v* tag)
#   neither (dev) → "X.Y.Z[-STAGE]-dev+<short-sha>"
#
# Outputs (PARENT_SCOPE):
#   OUT_VERSION  the composed version string
#   OUT_TWEAK    rev count (used by `windows.rc.in` as the 4th MSI digit so
#                each snapshot/dev build is a distinguishable Windows installer)
#   OUT_BASE     plain base semver (X.Y.Z) for consumers that reject prerelease
#                suffixes (Arch makepkg pkgver, anything treating the version as
#                a strict identifier).
function(synergy_compute_version SOURCE_DIR OUT_VERSION OUT_TWEAK OUT_BASE)
  set(_rev_count 0)
  set(_git_sha "")
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe HEAD --tags --long --match "v[0-9]*"
      WORKING_DIRECTORY "${SOURCE_DIR}"
      OUTPUT_VARIABLE _git_describe
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_git_describe MATCHES "-([0-9]+)-g")
      set(_rev_count "${CMAKE_MATCH_1}")
    endif()
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
      WORKING_DIRECTORY "${SOURCE_DIR}"
      OUTPUT_VARIABLE _git_sha
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()

  set(_base "${SYNERGY_VERSION_MAJOR}.${SYNERGY_VERSION_MINOR}.${SYNERGY_VERSION_PATCH}")
  set(_stage "")
  if(SYNERGY_VERSION_STAGE)
    set(_stage "-${SYNERGY_VERSION_STAGE}")
  endif()
  if(SYNERGY_VERSION_RELEASE)
    set(_version "${_base}${_stage}")
    set(_tweak 0)
  elseif(SYNERGY_VERSION_SNAPSHOT)
    set(_version "${_base}${_stage}-snapshot+r${_rev_count}")
    set(_tweak ${_rev_count})
  else()
    if(_git_sha)
      set(_version "${_base}${_stage}-dev+${_git_sha}")
    else()
      set(_version "${_base}${_stage}-dev")
    endif()
    set(_tweak ${_rev_count})
  endif()

  set(${OUT_VERSION} "${_version}" PARENT_SCOPE)
  set(${OUT_TWEAK} "${_tweak}" PARENT_SCOPE)
  set(${OUT_BASE} "${_base}" PARENT_SCOPE)
endfunction()
