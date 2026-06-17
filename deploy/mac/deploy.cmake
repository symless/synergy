# SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-License-Identifier: MIT

# HACK This is set when the files is included so its the real path
# calling CMAKE_CURRENT_LIST_DIR after include would return the wrong scope var
set(MY_DIR ${CMAKE_CURRENT_LIST_DIR})
set(OSX_BUNDLE ${BUILD_OSX_BUNDLE})

set(OS_STRING "macos-${BUILD_ARCHITECTURE}")

if (OSX_BUNDLE)
  install(CODE "execute_process(COMMAND
    ${DEPLOYQT}
    \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_PROJECT_PROPER_NAME}.app\"
    -timestamp -codesign=-
  )")

  # macdeployqt above only ad-hoc signs the staged bundle, and later install()
  # steps (e.g. LICENSE into Resources) would invalidate any signature applied
  # at install time. So when a Developer ID is provided, defer signing to a CPack
  # pre-build script that runs once the staging tree is fully populated, right
  # before the DMG is built; otherwise the notary service rejects it as Invalid.
  if(APPLE_CODESIGN_ID)
    configure_file(
      ${CMAKE_SOURCE_DIR}/extra/deploy/mac/codesign.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/mac-codesign.cmake
      @ONLY
    )
    set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_CURRENT_BINARY_DIR}/mac-codesign.cmake)
  endif()

  set(CPACK_PACKAGE_ICON "${MY_DIR}/dmg-volume.icns")
  set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/extra/deploy/mac/dmg-background.tiff")
  set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${MY_DIR}/generate_ds_store.applescript")
  set(CPACK_DMG_VOLUME_NAME "${CMAKE_PROJECT_PROPER_NAME}")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/extra/deploy/mac/synergy-eula.txt")
  set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
  set(CPACK_GENERATOR "DragNDrop")
endif()
