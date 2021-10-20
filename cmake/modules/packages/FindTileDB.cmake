##############################################################################
#
# CMake module to search for the TileDB library
#
# On success, the macro sets the following variables:
# TILEDB_FOUND       = if the library found
# TILEDB_LIBRARIES   = full path to the library
# TILEDB_INCLUDE_DIR = where to find the library headers also defined,
#                       but not for general use are
# TILEDB_LIBRARY     = where to find the hexer library.
# TILEDB_VERSION     = version of library which was found, e.g. "1.4.1"
#
# IMPORTED targets
# This module defines the following IMPORTED target: ``TileDB::TileDB``
#
# Copyright (c) 2019 TileDB, Inc
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
###############################################################################

IF(TILEDB_FOUND)
    # Already in cache, be silent
    SET(TILEDB_FIND_QUIETLY TRUE)
ENDIF()

IF(DEFINED ENV{TILEDB_HOME})
    set(TILEDB_HOME $ENV{TILEDB_HOME})
ENDIF()

FIND_PATH(TILEDB_INCLUDE_DIR
        tiledb
        PATHS
        ${TILEDB_HOME}/dist/include
        ${TILEDB_ROOT}/include)

FIND_LIBRARY(TILEDB_LIBRARY
        NAMES tiledb
        PATHS
        ${TILEDB_HOME}/dist/lib
        ${TILEDB_ROOT}/lib)

SET(TILEDB_VERSION_H "${TILEDB_INCLUDE_DIR}/tiledb/tiledb_version.h")
IF(TILEDB_INCLUDE_DIR AND EXISTS ${TILEDB_VERSION_H})
    SET(TILEDB_VERSION 0)

    FILE(READ ${TILEDB_VERSION_H} TILEDB_VERSION_H_CONTENTS)

    IF (DEFINED TILEDB_VERSION_H_CONTENTS)
        string(REGEX REPLACE ".*#define TILEDB_VERSION_MAJOR ([0-9]+).*" "\\1" TILEDB_VERSION_MAJOR "${TILEDB_VERSION_H_CONTENTS}")
        string(REGEX REPLACE ".*#define TILEDB_VERSION_MINOR ([0-9]+).*" "\\1" TILEDB_VERSION_MINOR "${TILEDB_VERSION_H_CONTENTS}")
        string(REGEX REPLACE ".*#define TILEDB_VERSION_PATCH ([0-9]+).*" "\\1" TILEDB_VERSION_PATCH "${TILEDB_VERSION_H_CONTENTS}")

        if(NOT "${TILEDB_VERSION_MAJOR}" MATCHES "^[0-9]+$")
            message(FATAL_ERROR "TileDB version parsing failed for \"TILEDB_API_VERSION_MAJOR\"")
        endif()
        if(NOT "${TILEDB_VERSION_MINOR}" MATCHES "^[0-9]+$")
            message(FATAL_ERROR "TileDB version parsing failed for \"TILEDB_VERSION_MINOR\"")
        endif()
        if(NOT "${TILEDB_VERSION_PATCH}" MATCHES "^[0-9]+$")
            message(FATAL_ERROR "TileDB version parsing failed for \"TILEDB_VERSION_PATCH\"")
        endif()

        SET(TILEDB_VERSION "${TILEDB_VERSION_MAJOR}.${TILEDB_VERSION_MINOR}.${TILEDB_VERSION_PATCH}"
                CACHE INTERNAL "The version string for TileDB library")

        IF (TILEDB_VERSION VERSION_LESS TILEDB_FIND_VERSION)
            MESSAGE(FATAL_ERROR "TileDB version check failed. Version ${TILEDB_VERSION} was found, at least version ${TILEDB_FIND_VERSION} is required")
        ENDIF()
    ELSE()
        MESSAGE(FATAL_ERROR "Failed to open ${TILEDB_VERSION_H} file")
    ENDIF()

ENDIF()

SET(TILEDB_LIBRARIES ${TILEDB_LIBRARY})

# Handle the QUIETLY and REQUIRED arguments and set TILEDB_FOUND to TRUE
# if all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TileDB DEFAULT_MSG TILEDB_LIBRARY TILEDB_INCLUDE_DIR)

if(TILEDB_FOUND)
  if(NOT TARGET TileDB::TileDB)
    add_library(TileDB::TileDB UNKNOWN IMPORTED)
    set_target_properties(TileDB::TileDB PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${TILEDB_INCLUDE_DIR})
    set_target_properties(TileDB::TileDB PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
        IMPORTED_LOCATION "${TILEDB_LIBRARY}")
  endif()
endif()
