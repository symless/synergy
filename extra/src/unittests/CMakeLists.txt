# Synergy -- mouse and keyboard sharing utility
# Copyright (C) 2024 - 2026 Synergy App Ltd
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

enable_testing()
find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Test)

# legacytests provisions googletest, but when that is satisfied by find_package
# the imported targets are scoped to its own directory, so resolve them here too.
if(NOT TARGET GTest::gtest_main)
  find_package(GTest REQUIRED)
endif()

create_test(
  NAME LicenseTests
  DEPENDS license
  LIBS GTest::gtest_main
  SOURCE license/LicenseTests.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

create_test(
  NAME ProductTests
  DEPENDS license
  LIBS GTest::gtest_main
  SOURCE license/ProductTests.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

create_test(
  NAME SerialKeyTypeTests
  DEPENDS license
  LIBS GTest::gtest_main
  SOURCE license/SerialKeyTypeTests.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

create_test(
  NAME OfflineActivationTests
  DEPENDS license
  LIBS GTest::gtest_main
  SOURCE license/OfflineActivationTests.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
