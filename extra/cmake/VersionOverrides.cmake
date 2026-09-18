# SPDX-FileCopyrightText: (C) 2012 - 2026 Synergy App Ltd
# SPDX-License-Identifier: MIT

# Must be included after the REQUIRED_*_VERSION constants are set in CMakeLists.txt, so
# these take effect. Lets a build lower a floor without editing the upstream constants:
# the el8 CI legs pass -DQT_VERSION_OVERRIDE=5.13 -DOPENSSL_VERSION_OVERRIDE=1.1.1, and
# the Ubuntu 22.04 legs -DQT_VERSION_OVERRIDE=6.2.0 for that LTS's Qt 6.2.4.

if(QT_VERSION_OVERRIDE)
  set(REQUIRED_QT_VERSION ${QT_VERSION_OVERRIDE})
endif()

if(OPENSSL_VERSION_OVERRIDE)
  set(REQUIRED_OPENSSL_VERSION ${OPENSSL_VERSION_OVERRIDE})
endif()
