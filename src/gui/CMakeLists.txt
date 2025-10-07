# Deskflow -- mouse and keyboard sharing utility
# Copyright (C) 2024 Symless Ltd.
#
# This package is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# found in the file LICENSE that should have accompanied this file.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

set(target ${GUI_BINARY_NAME})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(res_dir ${GUI_RES_DIR})
set(qrc_file ${GUI_QRC_FILE})

file(
  GLOB
  sources
  src/*.cpp
  src/*.h
  src/validators/*
  src/widgets/*)
file(GLOB ui_files src/*.ui)

if(WIN32)
  set(rc_files ${res_dir}/win/app.rc ${PROJECT_BINARY_DIR}/src/version.rc)
endif()

# regular exe headers
include_directories(./src)

# gui library autogen headers:
# qt doesn't seem to auto include the autogen headers for libraries.
include_directories(${PROJECT_BINARY_DIR}/src/lib/gui/gui_autogen/include)

# generated includes
include_directories(${PROJECT_BINARY_DIR}/config)

add_executable(${target} WIN32 ${sources} ${ui_files} ${rc_files} ${qrc_file})

target_link_libraries(
  ${target}
  ${DESKFLOW_GUI_HOOK_LIB}
  gui
  Qt6::Core
  Qt6::Widgets
  Qt6::Network)

if(WIN32)
  set_target_properties(${target} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:LIBCMT")
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

  find_program(MACDEPLOYQT_BIN macdeployqt6)
  message(STATUS "Found macdeployqt6: ${MACDEPLOYQT_BIN}")

  set(MACDEPLOYQT_CMD
      "${MACDEPLOYQT_BIN} ${DESKFLOW_BUNDLE_DIR} -always-overwrite")

  install(TARGETS ${target} DESTINATION ${DESKFLOW_BUNDLE_BINARY_DIR})
  install(CODE "MESSAGE (\"Running: ${MACDEPLOYQT_CMD}\")")
  install(CODE "execute_process(COMMAND ${MACDEPLOYQT_CMD})")

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

  install(TARGETS ${target} DESTINATION bin)

elseif(WIN32)

  find_package(Qt6 REQUIRED COMPONENTS Widgets)
  get_target_property(WINDEPLOYQT_EXE Qt6::windeployqt IMPORTED_LOCATION)
  add_custom_command(TARGET ${GUI_BINARY_NAME} POST_BUILD
    COMMAND ${WINDEPLOYQT_EXE}
      --force-openssl
      --no-compiler-runtime
      --no-system-d3d-compiler
      --no-opengl-sw
      --no-quick-import
      $<TARGET_FILE:${GUI_BINARY_NAME}>
    COMMENT "Deploying Qt runtime for ${GUI_BINARY_NAME}.exe"
  )

endif()
