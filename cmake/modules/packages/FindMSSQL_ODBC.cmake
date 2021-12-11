# Find MSSQL ODBC driver
# ~~~~~~~~~~~~~~~~~~~~~~
#
# Copyright (c) 2021 Even Rouault <even.rouault@spatialys.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

#[=======================================================================[.rst:
FindMSSQL_ODBC
--------------

Find the MSSQL ODBC driver includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``MSSQL_ODBC::MSSQL_ODBC``, if
MSSQL_ODBC has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  MSSQL_ODBC_INCLUDE_DIRS   - where to find msodbcsql.h, etc.
  MSSQL_ODBC_LIBRARIES      - List of libraries when using MSSQL_ODBC.
  MSSQL_ODBC_FOUND          - True if MSSQL_ODBC found.
  MSSQL_ODBC_VERSION		- Major Version (17, 13, ...). Can be a input variable too

#]=======================================================================]

if(WIN32)
    if(NOT DEFINED MSSQL_ODBC_VERSION)
        set(MSSQL_ODBC_VERSION_CANDIDATES 17 13)
        foreach(_vers IN LISTS MSSQL_ODBC_VERSION_CANDIDATES)
          set(_dir "C:/Program Files/Microsoft SQL Server/Client SDK/ODBC/${_vers}0/SDK")
          if(EXISTS "${_dir}")
            set(MSSQL_ODBC_VERSION "${_vers}")
            break()
          endif()
        endforeach()
    endif()

    if(NOT DEFINED MSSQL_ODBC_ROOT)
        set(MSSQL_ODBC_ROOT "C:/Program Files/Microsoft SQL Server/Client SDK/ODBC/${MSSQL_ODBC_VERSION}0/SDK")
    endif()

    find_path(MSSQL_ODBC_INCLUDE_DIR NAMES msodbcsql.h
              PATHS "${MSSQL_ODBC_ROOT}/Include")
    mark_as_advanced(MSSQL_ODBC_INCLUDE_DIR)

    if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "AMD64")
        set(MSSQL_ODBC_DIR_ARCH x64)
    else()
        set(MSSQL_ODBC_DIR_ARCH x86)
    endif()

    find_library(MSSQL_ODBC_LIBRARY NAMES "msodbcsql${MSSQL_ODBC_VERSION}.lib"
                 PATHS "${MSSQL_ODBC_ROOT}/Lib/${MSSQL_ODBC_DIR_ARCH}")
    mark_as_advanced(MSSQL_ODBC_LIBRARY)
else()
    if(NOT DEFINED MSSQL_ODBC_VERSION)
        set(MSSQL_ODBC_VERSION_CANDIDATES 17 13)
        foreach(_vers IN LISTS MSSQL_ODBC_VERSION_CANDIDATES)
          set(_dir "/opt/microsoft/msodbcsql${_vers}")
          if(EXISTS "${_dir}")
            set(MSSQL_ODBC_VERSION "${_vers}")
            break()
          endif()
        endforeach()
    endif()

    if(NOT DEFINED MSSQL_ODBC_ROOT)
        set(MSSQL_ODBC_ROOT "/opt/microsoft/msodbcsql${MSSQL_ODBC_VERSION}")
    endif()

    find_path(MSSQL_ODBC_INCLUDE_DIR NAMES msodbcsql.h
              PATHS "${MSSQL_ODBC_ROOT}/include")
    mark_as_advanced(MSSQL_ODBC_INCLUDE_DIR)

    file(GLOB _libs "${MSSQL_ODBC_ROOT}/lib*/libmsodbc*")
    list(LENGTH _libs _libs_length)
    if(_libs_length EQUAL "1")
        set(MSSQL_ODBC_LIBRARY ${_libs})
    endif()
    mark_as_advanced(MSSQL_ODBC_LIBRARY)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MSSQL_ODBC
                                  FOUND_VAR MSSQL_ODBC_FOUND
                                  VERSION_VAR MSSQL_ODBC_VERSION
                                  REQUIRED_VARS MSSQL_ODBC_LIBRARY MSSQL_ODBC_INCLUDE_DIR MSSQL_ODBC_VERSION)

if(MSSQL_ODBC_FOUND)
    set(MSSQL_ODBC_LIBRARIES ${MSSQL_ODBC_LIBRARY})
    set(MSSQL_ODBC_INCLUDE_DIRS ${MSSQL_ODBC_INCLUDE_DIR})
    if(NOT TARGET MSSQL_ODBC::MSSQL_ODBC)
        add_library(MSSQL_ODBC::MSSQL_ODBC UNKNOWN IMPORTED)
        set_target_properties(MSSQL_ODBC::MSSQL_ODBC PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${MSSQL_ODBC_INCLUDE_DIRS}"
                              INTERFACE_COMPILE_DEFINITIONS "MSODBCSQL_VERSION=${MSSQL_ODBC_VERSION};MSSQL_BCP_SUPPORTED=1"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${MSSQL_ODBC_LIBRARY}")
    endif()
endif()
