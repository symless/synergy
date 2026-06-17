# SPDX-FileCopyrightText: (C) 2012 - 2026 Synergy App Ltd
# SPDX-License-Identifier: MIT

# Synergy distribution filename: synergy_<version>_<os>_<arch>.<ext>, matching the
# release Package Templates that the downloads page / release flow match artifacts
# against. Upstream's deploy/ builds a different scheme (hyphen-joined, win/macos,
# os-release-derived distro tokens) it will never change to ours, so we override
# CPACK_PACKAGE_FILE_NAME here instead of patching deploy/*. Included once from
# deploy/CMakeLists.txt after the per-platform OS detection, so it can reuse the
# DISTRO_* vars deploy/linux/deploy.cmake already parsed from /etc/os-release.
#
# The flatpak bundle and the Arch .pkg.tar.zst are named in
# .github/workflows/ci.yml (built outside CPack; Actions can't include cmake) —
# keep those in step with this scheme.

set(_os "")
if(WIN32)
  set(_os "windows_${BUILD_ARCHITECTURE}")
elseif(APPLE)
  # mac uses the short arch token (mac_x64 / mac_arm64), not x86_64.
  set(_arch "${BUILD_ARCHITECTURE}")
  if(_arch STREQUAL "x86_64")
    set(_arch "x64")
  endif()
  set(_os "mac_${_arch}")
elseif(UNIX)
  set(_distro "${DISTRO_NAME}")
  set(_ver "")
  if("${_distro}" STREQUAL "arch")
    set(_distro "arch-linux")           # os-release ID is "arch"
  elseif("${_distro}" STREQUAL "opensuse")
    set(_ver "-${DISTRO_CODENAME}")     # rolling: codename (tumbleweed), not the ISO date
  elseif(NOT "${DISTRO_VERSION_ID}" STREQUAL "")
    string(REPLACE "." "-" _ver "${DISTRO_VERSION_ID}")   # 24.04 -> 24-04
    set(_ver "-${_ver}")
  elseif(NOT "${DISTRO_CODENAME}" STREQUAL "")
    set(_ver "-${DISTRO_CODENAME}")
  endif()
  if("${_distro}" STREQUAL "")
    set(_distro "linux")
  endif()
  set(_os "${_distro}${_ver}_${BUILD_ARCHITECTURE}")
endif()

if(_os)
  set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${PACKAGE_VERSION_LABEL}_${_os}")
endif()
