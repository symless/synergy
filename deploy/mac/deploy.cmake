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

  # macdeployqt only ad-hoc signs the staged bundle above. When a Developer ID
  # is provided, re-sign every nested binary with hardened runtime and a secure
  # timestamp so the app embedded in the DMG passes notarization; an ad-hoc
  # signature is rejected by the notary service as Invalid.
  if(APPLE_CODESIGN_ID)
    install(CODE "
      execute_process(
        COMMAND /usr/bin/codesign --force --deep --options runtime --timestamp
                --sign \"${APPLE_CODESIGN_ID}\"
                \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_PROJECT_PROPER_NAME}.app\"
        RESULT_VARIABLE result
      )
      if(NOT result EQUAL 0)
        message(FATAL_ERROR \"codesign of the app bundle failed (\${result})\")
      endif()
    ")
  endif()

  set(CPACK_PACKAGE_ICON "${MY_DIR}/dmg-volume.icns")
  set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/extra/deploy/mac/dmg-background.tiff")
  set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${MY_DIR}/generate_ds_store.applescript")
  set(CPACK_DMG_VOLUME_NAME "${CMAKE_PROJECT_PROPER_NAME}")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/extra/deploy/mac/synergy-eula.txt")
  set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
  set(CPACK_GENERATOR "DragNDrop")
endif()
