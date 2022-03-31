###############################################################################
# - Try to find Sqlite3
# Once done this will define
#
#  SQLITE3_FOUND - system has Sqlite3
#  SQLITE3_INCLUDE_DIRS - the Sqlite3 include directory
#  SQLITE3_LIBRARIES - Link these to use Sqlite3
#  SQLITE3_DEFINITIONS - Compiler switches required for using Sqlite3
#
#  Copyright (c) 2008 Andreas Schneider <mail@cynapses.org>
#  Copyright (c) 2016, NextGIS <info@nextgis.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
###############################################################################

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
if (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
include(UsePkgConfig)
pkgconfig(sqlite3 _SQLITE3_INCLUDEDIR _SQLITE3_LIBDIR _SQLITE3_LDFLAGS _SQLITE3_CFLAGS)
else (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(_SQLITE3 sqlite3)
endif (PKG_CONFIG_FOUND)
endif (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
find_path(SQLITE3_INCLUDE_DIR
NAMES
  sqlite3.h
PATHS
  ${_SQLITE3_INCLUDEDIR}
  /usr/include
  /usr/local/include
  /opt/local/include
  /sw/include
)

find_library(SQLITE3_LIBRARY
NAMES
  sqlite3
PATHS
  ${_SQLITE3_LIBDIR}
  /usr/lib
  /usr/local/lib
  /opt/local/lib
  /sw/lib
)


# TODO Check version

# Handle the QUIETLY and REQUIRED arguments and set GEOS_FOUND to TRUE
# if all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SQLITE3 DEFAULT_MSG SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR)

if(SQLITE3_FOUND)
  set(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
  set(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
endif()

# Hide internal variables
mark_as_advanced(SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR)
