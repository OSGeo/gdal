# Find MSSQL Native Client
# ~~~~~~~~~~~~~~~~~~~~~~~~
#
# Copyright (c) 2021 Even Rouault <even.rouault@spatialys.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

#[=======================================================================[.rst:
FindMSSQL_NCLI
--------------

Find the MSSQL Native Client includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``MSSQL_NCLI::MSSQL_NCLI``, if
MSSQL_NCLI has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  MSSQL_NCLI_INCLUDE_DIRS   - where to find sqlncli.h, etc.
  MSSQL_NCLI_LIBRARIES      - List of libraries when using MSSQL_NCLI.
  MSSQL_NCLI_FOUND          - True if MSSQL_NCLI found.
  MSSQL_NCLI_VERSION        - Major Version (11, 10, ...). Can be a input variable too

#]=======================================================================]

if(NOT WIN32)
    return()
endif()

if(NOT DEFINED MSSQL_NCLI_VERSION)
    set(MSSQL_NCLI_VERSION_CANDIDATES 11 10)
    foreach(_vers IN LISTS MSSQL_NCLI_VERSION_CANDIDATES)
      set(_dir "C:/Program Files/Microsoft SQL Server/${_vers}0/SDK")
      if(EXISTS "${_dir}")
        set(MSSQL_NCLI_VERSION "${_vers}")
        break()
      endif()
    endforeach()
endif()

if(NOT DEFINED MSSQL_NCLI_ROOT)
    set(MSSQL_NCLI_ROOT "C:/Program Files/Microsoft SQL Server/${MSSQL_NCLI_VERSION}0/SDK")
endif()

find_path(MSSQL_NCLI_INCLUDE_DIR NAMES sqlncli.h
          PATHS "${MSSQL_NCLI_ROOT}/Include")
mark_as_advanced(MSSQL_NCLI_INCLUDE_DIR)

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    set(MSSQL_NCLI_DIR_ARCH x64)
else()
    set(MSSQL_NCLI_DIR_ARCH x86)
endif()

find_library(MSSQL_NCLI_LIBRARY NAMES "sqlncli${MSSQL_NCLI_VERSION}.lib"
             PATHS "${MSSQL_NCLI_ROOT}/Lib/${MSSQL_NCLI_DIR_ARCH}")
mark_as_advanced(MSSQL_NCLI_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MSSQL_NCLI
                                  FOUND_VAR MSSQL_NCLI_FOUND
                                  VERSION_VAR MSSQL_NCLI_VERSION
                                  REQUIRED_VARS MSSQL_NCLI_LIBRARY MSSQL_NCLI_INCLUDE_DIR MSSQL_NCLI_VERSION)

if(MSSQL_NCLI_FOUND)
    set(MSSQL_NCLI_LIBRARIES ${MSSQL_NCLI_LIBRARY})
    set(MSSQL_NCLI_INCLUDE_DIRS ${MSSQL_NCLI_INCLUDE_DIR})
    if(NOT TARGET MSSQL_NCLI::MSSQL_NCLI)
        add_library(MSSQL_NCLI::MSSQL_NCLI UNKNOWN IMPORTED)
        set_target_properties(MSSQL_NCLI::MSSQL_NCLI PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${MSSQL_NCLI_INCLUDE_DIRS}"
                              INTERFACE_COMPILE_DEFINITIONS "SQLNCLI_VERSION=${MSSQL_NCLI_VERSION};MSSQL_BCP_SUPPORTED=1"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${MSSQL_NCLI_LIBRARY}")
    endif()
endif()

