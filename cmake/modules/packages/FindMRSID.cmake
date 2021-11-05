# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file LICENSE.TXT.

#[=======================================================================[.rst:
configure
---------

#]=======================================================================]
# Find the MRSID library - Multi-resolution Seamless Image Database.
#
# Copyright (C) 2017,2018 Hiroshi Miura
# Copyright (c) 2015 NextGIS <info@nextgis.com>
#
# Sets
#   MRSID_FOUND.
#   MRSID_INCLUDE_DIRS
#   MRSID_LIBRARIES
#

find_path(MRSID_INCLUDE_DIR NAMES lt_base.h)

if( MRSID_INCLUDE_DIR )
  find_library( MRSID_LIBRARY NAMES lti_dsdk ltidsdk)
endif()

if(MRSID_INCLUDE_DIR AND MRSID_LIBRARY)
  set(MAJOR_VERSION 0)
  set(MINOR_VERSION 0)
  set(SRV_VERSION 0)
  set(BLD_VERSION 0)
  if(EXISTS "${MRSID_INCLUDE_DIR}/lti_version.h")
    file(READ "${MRSID_INCLUDE_DIR}/lti_version.h" VERSION_H_CONTENTS)
    string(REGEX MATCH "LTI_SDK_MAJOR[ \t]+([0-9]+)"
      MAJOR_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      MAJOR_VERSION ${MAJOR_VERSION})
    string(REGEX MATCH "LTI_SDK_MINOR[ \t]+([0-9]+)"
      MINOR_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      MINOR_VERSION ${MINOR_VERSION})
    string(REGEX MATCH "LTI_SDK_REV[ \t]+([0-9]+)"
      REV_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      REV_VERSION ${REV_VERSION})
    unset(VERSION_H_CONTENTS)
  endif()
  set(MRSID_VERSION_STRING "${MAJOR_VERSION}.${MINOR_VERSION}.${REV_VERSION}")
endif()
mark_as_advanced(MRSID_INCLUDE_DIR MRSID_LIBRARY MRSID_VERSION_STRING)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MRSID FOUND_VAR MRSID_FOUND
                                  REQUIRED_VARS MRSID_LIBRARY MRSID_INCLUDE_DIR
                                  VERSION_VAR MRSID_VERSION_STRING)

# Copy the results to the output variables.
if(MRSID_FOUND)
  set(MRSID_LIBRARIES ${MRSID_LIBRARY})
  set(MRSID_INCLUDE_DIRS ${MRSID_INCLUDE_DIR})
  if(NOT TARGET MRSID::MRSID)
    add_library(MRSID::MRSID UNKNOWN IMPORTED)
    set_target_properties(MRSID::MRSID PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${MRSID_INCLUDE_DIR}"
                          IMPORTED_LOCATION "${MRSID_LIBRARY}")
  endif()
endif()
