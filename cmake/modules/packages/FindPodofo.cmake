# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindPODOFO
-----------

Copyright (c) 2017, Hiroshi Miura <miurahr@linux.com>

Find the PODOFO headers and libraries.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``PODOFO::PODOFO``, if
PODOFO has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``PODOFO_FOUND``
  True if PODOFO found.

``PODOFO_INCLUDE_DIRS``
  where to find podofo/POFDoc.h, etc.

``PODOFO_LIBRARIES``
  List of podofo libraries to link.

``PODOFO_VERSION_STRING``
  Version of podofo library defined in podofo/base/podofo_config.h

#]=======================================================================]

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_PODOFO QUIET podofo)
endif()

find_path(PODOFO_INCLUDE_DIR
          NAMES podofo.h
          HINTS ${PC_PODOFO_INCLUDE_DIRS}
          PATH_SUFFIXES podofo)

find_library(PODOFO_LIBRARY
             NAMES podofo libpodofo
             HINTS ${PC_PODOFO_LIBRARY_DIRS})

if(PODOFO_INCLUDE_DIR)
  set(version_hdr ${PODOFO_INCLUDE_DIR}/base/podofo_config.h)
  if(EXISTS ${version_hdr})
    file(STRINGS ${version_hdr} _contents REGEX "^[ \t]*#define PODOFO_VERSION_.*")
    if(_contents)
        string(REGEX REPLACE ".*#define PODOFO_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" PODOFO_VERSION_MAJOR "${_contents}")
        string(REGEX REPLACE ".*#define PODOFO_VERSION_MINOR[ \t]+([0-9]+).*" "\\1" PODOFO_VERSION_MINOR "${_contents}")
        string(REGEX REPLACE ".*#define PODOFO_VERSION_PATCH[ \t]+([0-9]+).*" "\\1" PODOFO_VERSION_PATCH "${_contents}")
    endif()
    set(PODOFO_VERSION_STRING  ${PODOFO_VERSION_MAJOR}.${PODOFO_VERSION_MINOR}.${PODOFO_VERSION_PATCH})
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Podofo
                                  FOUND_VAR PODOFO_FOUND
                                  REQUIRED_VARS PODOFO_LIBRARY PODOFO_INCLUDE_DIR
                                  VERSION_VAR PODOFO_VERSION_STRING)

if(PODOFO_FOUND)
  set(PODOFO_INCLUDE_DIRS ${PODOFO_INCLUDE_DIR})
  set(PODOFO_LIBRARIES ${PDOFO_LIBRARY})
  if(NOT TARGET PODOFO::Podofo)
    add_library(PODOFO::Podofo UNKNOWN IMPORTED)
    set_target_properties(PODOFO::Podofo PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES ${PODOFO_INCLUDE_DIR}
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION ${PODOFO_LIBRARY})
  endif()
endif()

include(FeatureSummary)
set_package_properties(PODOFO PROPERTIES
                       DESCRIPTION "a free, portable C++ library which includes classes to parse PDF files and modify their contents into memory."
                       URL "http://podofo.sourceforge.net/"
)