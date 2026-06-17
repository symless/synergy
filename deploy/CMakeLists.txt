# SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-License-Identifier: MIT

# Copy License with txt ext for picky package creation tools
file(COPY_FILE
  ${CMAKE_SOURCE_DIR}/LICENSE
  ${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt
  ONLY_IF_DIFFERENT
)

# Generic Package Items
set(CPACK_STRIP_FILES TRUE)
set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_CONTACT ${CMAKE_PROJECT_CONTACT})
set(CPACK_PACKAGE_DESCRIPTION ${CMAKE_PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR ${CMAKE_PROJECT_VENDOR})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt)

set(CPACK_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})

#Prevent this override from being written in the package
if(NOT PACKAGE_VERSION_LABEL)
    set (PACKAGE_VERSION_LABEL "${CPACK_PACKAGE_VERSION}")
endif()

if(WIN32)
  include(windows/deploy.cmake)
elseif(UNIX AND NOT APPLE)
  include(linux/deploy.cmake)
elseif(APPLE)
  include(mac/deploy.cmake)
else()
  message(STATUS "UNKNOWN System: ${CMAKE_SYSTEM_NAME}")
endif()

set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${PACKAGE_VERSION_LABEL}-${OS_STRING}")

# Synergy overlay: rewrite CPACK_PACKAGE_FILE_NAME to the release-templates scheme
# (synergy_<version>_<os>_<arch>); kept in extra/ since upstream won't adopt it.
include(${CMAKE_SOURCE_DIR}/extra/deploy/PackageFileName.cmake)

message(STATUS "Package Basename: ${CPACK_PACKAGE_FILE_NAME}")

include(CPack)
