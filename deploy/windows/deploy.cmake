# SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-License-Identifier: MIT

# HACK This is set when the files is included so its the real path
# calling CMAKE_CURRENT_LIST_DIR after include would return the wrong scope var
set(MY_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_LIBDIR})
include(InstallRequiredSystemLibraries)

configure_file(${MY_DIR}/pre-cpack.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/pre-cpack.cmake @ONLY)
set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_CURRENT_BINARY_DIR}/pre-cpack.cmake)

configure_file(${MY_DIR}/cpack-options.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/cpack-options.cmake @ONLY)
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/cpack-options.cmake)

set(OS_STRING "win-${BUILD_ARCHITECTURE}")

list(APPEND CPACK_GENERATOR "7Z")

# If Wix4+ is installed make a package
find_program(WIX_APP wix)
if (NOT "${WIX_APP}" STREQUAL "")
  set(CPACK_WIX_VERSION 4)
  set(CPACK_WIX_ARCHITECTURE ${BUILD_ARCHITECTURE})
  list(APPEND CPACK_GENERATOR "WIX")
endif()

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_PROPER_NAME}")

# Menu Entry
set(CPACK_WIX_PROGRAM_MENU_FOLDER "${CMAKE_PROJECT_PROPER_NAME}")
set(CPACK_PACKAGE_EXECUTABLES "${CMAKE_PROJECT_NAME}" "${CMAKE_PROJECT_PROPER_NAME}")

# Default Install Path
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CMAKE_PROJECT_PROPER_NAME}")

# Wix Specific Values
set(CPACK_WIX_UPGRADE_GUID "027D1C8A-E7A5-4754-BB93-B2D45BFDBDC8")
set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/extra/deploy/windows/wix-banner.png")
set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/extra/deploy/windows/wix-dialog.png")

# Show the Synergy EULA (which references the canonical copy at synergyapp.io/eula)
# on the license page instead of the GPL; the GPL still ships as the LICENSE file.
set(CPACK_WIX_LICENSE_RTF "${CMAKE_SOURCE_DIR}/extra/deploy/windows/synergy-eula.rtf")

# Required Extra Extenstions
list(APPEND CPACK_WIX_EXTENSIONS "WixToolset.Util.wixext" "WixToolset.Firewall.wixext")

# Make sure to also put the xmlns for the ext into the wix block on generated files
list(APPEND CPACK_WIX_CUSTOM_XMLNS "util=http://wixtoolset.org/schemas/v4/wxs/util" "firewall=http://wixtoolset.org/schemas/v4/wxs/firewall")

# Bundle the MSVC C++ runtime (CRT) as a merge module inside the MSI so that end
# users don't have to download and install the Visual C++ redistributable
# themselves. The merge module ships with the Visual Studio build environment.
# Docs: https://learn.microsoft.com/en-us/cpp/windows/redistributing-components-by-using-merge-modules
set(REDIST_MERGE_MODULE_DIR "$ENV{VCINSTALLDIR}Redist/MSVC")
file(GLOB REDIST_MERGE_MODULE_PATHS
  "${REDIST_MERGE_MODULE_DIR}/*/MergeModules/Microsoft_VC*_CRT_${BUILD_ARCHITECTURE}.msm")
if (REDIST_MERGE_MODULE_PATHS)
  # Multiple toolset versions may be installed; pick the newest.
  list(SORT REDIST_MERGE_MODULE_PATHS)
  list(GET REDIST_MERGE_MODULE_PATHS -1 REDIST_MERGE_MODULE_PATH)
  message(STATUS "MSVC CRT merge module: ${REDIST_MERGE_MODULE_PATH}")
  # XML snippets injected into wix-patch.xml.in to merge the CRT into the MSI.
  set(WIX_REDIST_MERGE
    "<DirectoryRef Id=\"TARGETDIR\"><Merge Id=\"VC_Redist\" SourceFile=\"${REDIST_MERGE_MODULE_PATH}\" DiskId=\"1\" Language=\"0\"/></DirectoryRef>")
  set(WIX_REDIST_MERGE_REF "<MergeRef Id=\"VC_Redist\"/>")
elseif(SYNERGY_VERSION_RELEASE OR SYNERGY_VERSION_SNAPSHOT)
  # Shipping build: a missing CRT module means a broken installer for end users.
  message(WARNING
    "MSVC CRT merge module not found under ${REDIST_MERGE_MODULE_DIR}; "
    "the installer will NOT bundle the Visual C++ runtime")
else()
  # Dev builds routinely lack the redist merge module; not bundling it is expected.
  message(STATUS
    "MSVC CRT merge module not found; installer will not bundle the Visual C++ "
    "runtime (expected for dev builds)")
endif()

# The patch needs the full path of the CRT merge module (see above).
configure_file(
  ${MY_DIR}/wix-patch.xml.in
  ${CMAKE_CURRENT_BINARY_DIR}/wix-patch.xml @ONLY
)

# This patch sets up firewall rules, the service and the CRT merge module.
set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_BINARY_DIR}/wix-patch.xml")
