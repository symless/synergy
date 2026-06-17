# SPDX-FileCopyrightText: (C) 2012 - 2026 Synergy App Ltd
# SPDX-License-Identifier: MIT

# Synergy bundles the MSVC C++ runtime as a merge module inside the MSI
# (deploy/windows/deploy.cmake) instead of making the user install the
# redistributable. The runtime guard (ArchMiscWindows::guardRuntimeVersion) must
# therefore require the version we actually ship, not the build machine's
# installed redist that upstream's registry query returns. The two drift: CI
# runners get a newer redist than the toolset's merge module, so the guard ends
# up rejecting our own bundled runtime and the service fails to start.
#
# Override the upstream registry-derived minor with the version of the CRT merge
# module we bundle, taken from the redist directory name
# (.../Redist/MSVC/<version>/MergeModules/...). Builds without a merge module
# (dev) keep upstream's registry value as the fallback.
#
# This globs the same path deploy/windows/deploy.cmake bundles from; keep the two
# in sync until a future change discovers the module once and shares the path.
if(MSVC)
  file(GLOB _crt_msms
    "$ENV{VCINSTALLDIR}Redist/MSVC/*/MergeModules/Microsoft_VC*_CRT_${BUILD_ARCHITECTURE}.msm")
  list(SORT _crt_msms)
  if(_crt_msms)
    list(GET _crt_msms -1 _crt_msm)
    string(REGEX MATCH "Redist/MSVC/[0-9]+\\.([0-9]+)\\." _ "${_crt_msm}")
    set(REQUIRED_MSVC_RUNTIME_MINOR "${CMAKE_MATCH_1}")
    message(STATUS "MSVC runtime (bundled CRT merge module): ${REQUIRED_MSVC_RUNTIME_MAJOR}.${REQUIRED_MSVC_RUNTIME_MINOR}")
  endif()
endif()
