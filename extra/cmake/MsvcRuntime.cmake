# SPDX-FileCopyrightText: (C) 2012 - 2026 Synergy App Ltd
# SPDX-License-Identifier: MIT

# Synergy bundles the MSVC C++ runtime as a merge module inside the MSI
# (deploy/windows/deploy.cmake) instead of making the user install the
# redistributable. The runtime guard (ArchMiscWindows::guardRuntimeVersion) must
# therefore require the version we actually ship, not the build machine's
# installed redist that upstream's registry query returns: CI runners carry a
# newer redist than the bundled module, so the guard rejects our own runtime and
# the service fails to start.
#
# The bundled module ships the runtime of the compiler's own toolset, but its
# path carries only the toolset major (.../Redist/MSVC/v143/MergeModules/...), so
# the runtime version isn't in it. Take the minor from the compiler version
# instead: cl major 19 maps to CRT major 14 and the minor matches (e.g. 19.44 ->
# 14.44), per Microsoft's _MSC_VER / build-tools version table:
# https://learn.microsoft.com/en-us/cpp/overview/compiler-versions?view=msvc-170
# That floor is always <= the runtime we bundle, so it can't reject what we ship.
# Builds with no bundled module (dev) keep upstream's registry value.
#
# The glob mirrors deploy/windows/deploy.cmake's bundling condition; keep in sync.
if(MSVC)
  file(GLOB _crt_msms
    "$ENV{VCINSTALLDIR}Redist/MSVC/*/MergeModules/Microsoft_VC*_CRT_${BUILD_ARCHITECTURE}.msm")
  if(_crt_msms AND CMAKE_CXX_COMPILER_VERSION MATCHES "^[0-9]+\\.([0-9]+)")
    set(REQUIRED_MSVC_RUNTIME_MINOR "${CMAKE_MATCH_1}")
    message(STATUS "MSVC runtime (compiler toolset): ${REQUIRED_MSVC_RUNTIME_MAJOR}.${REQUIRED_MSVC_RUNTIME_MINOR}")
  endif()
endif()
