# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindODBCCPP
--------------

Find the odbc-cpp-wrapper includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``ODBCCPP::ODBCCPP``, if
the odbc-cpp-wrapper has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  ODBCCPP_INCLUDE_DIRS - where to find the odbc-cpp-wrapper headers.
  ODBCCPP_LIBRARIES    - List of libraries when using the odbc-cpp-wrapper.
  ODBCCPP_FOUND        - True if the odbc-cpp-wrapper found.

#]=======================================================================]

if(NOT ODBCCPP_INCLUDE_DIR)
  find_path(ODBCCPP_INCLUDE_DIR odbc/Environment.h
    PATHS
     /usr/include
     /usr/local/include
     c:/msys/local/include
     "$ENV{LIB_DIR}/include"
     "$ENV{INCLUDE}"
     "$ENV{ODBCCPP_PATH}/include")
endif(NOT ODBCCPP_INCLUDE_DIR)

if(NOT ODBCCPP_LIBRARY)
  find_library(ODBCCPP_LIBRARY odbccpp
    PATHS
     /usr/lib
     /usr/local/lib
     c:/msys/local/lib
     "$ENV{LIB_DIR}/lib"
     "$ENV{LIB}"
     "$ENV{ODBCCPP_PATH}/lib")
endif(NOT ODBCCPP_LIBRARY)

if(ODBCCPP_INCLUDE_DIR AND ODBCCPP_LIBRARY)
   set(ODBCCPP_FOUND TRUE)
endif(ODBCCPP_INCLUDE_DIR AND ODBCCPP_LIBRARY)

mark_as_advanced(ODBCCPP_LIBRARY ODBCCPP_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ODBCCPP
                                  FOUND_VAR ODBCCPP_FOUND
                                  REQUIRED_VARS ODBCCPP_LIBRARY ODBCCPP_INCLUDE_DIR)

if(ODBCCPP_FOUND)
  set(ODBCCPP_INCLUDE_DIRS ${ODBCCPP_INCLUDE_DIR})
  set(ODBCCPP_LIBRARIES ${ODBCCPP_LIBRARY})
  if(NOT TARGET ODBCCPP::ODBCCPP)
      add_library(ODBCCPP::ODBCCPP UNKNOWN IMPORTED)
      set_target_properties(ODBCCPP::ODBCCPP PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${ODBCCPP_INCLUDE_DIRS}"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
                            IMPORTED_LOCATION "${ODBCCPP_LIBRARY}")
  endif()
endif()
