PROJECT (coda)
CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
INCLUDE (CMakeMacro.cmake)

USE_PACKAGE (expat expat.h)
USE_PACKAGE (ev ev.h)
USE_PACKAGE (coda coda/coda.h)

AUX_SOURCE_DIRECTORY (blizzard SRC_BLIZZARD)
ADD_EXECUTABLE (blizzard ${SRC_BLIZZARD})
TARGET_LINK_LIBRARIES (blizzard ${LIB_coda} ${LIB_expat} ${LIB_ev} pthread)

IF (CMAKE_SYSTEM_NAME STREQUAL Linux)
  TARGET_LINK_LIBRARIES (blizzard dl)
ENDIF ()

INSTALL (FILES blizzard/plugin.hpp DESTINATION include/blizzard)
INSTALL (TARGETS blizzard DESTINATION bin)
INSTALL_TEMPLATE (blizzard/config.xml.in DESTINATION etc/blizzard)

