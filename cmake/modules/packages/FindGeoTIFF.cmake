###############################################################################
#
# CMake module to search for GeoTIFF library
#
# On success, the macro sets the following variables:
# GEOTIFF_FOUND       = if the library found
# GEOTIFF_LIBRARIES   = full path to the library
# GEOTIFF_INCLUDE_DIR = where to find the library headers
# also defined, but not for general use are
# GEOTIFF_LIBRARY
#
# Copyright (c) 2009 Mateusz Loskot <mateusz@loskot.net>
# Copyright (c) 2016 NextGIS <info@nextgis.com>
# Copyright (C) 2017,2018 Hiroshi Miura
#
# Origin from
# https://svn.osgeo.org/metacrs/geotiff/trunk/libgeotiff/cmake/FindGeoTIFF.cmake
# Module source: http://github.com/mloskot/workshop/tree/master/cmake/
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
###############################################################################

find_path(GEOTIFF_INCLUDE_DIR geotiff.h PATH_SUFFIXES geotiff libgeotiff)
find_library(GEOTIFF_LIBRARY geotiff geotiff_i)

if(GEOTIFF_INCLUDE_DIR)
    set(GEOTIFF_MAJOR_VERSION 0)
    set(GEOTIFF_MINOR_VERSION 0)
    set(GEOTIFF_PATCH_VERSION 0)
    set(GEOTIFF_REVISION_VERSION 0)

    if(EXISTS "${GEOTIFF_INCLUDE_DIR}/geotiff.h")
        file(READ "${GEOTIFF_INCLUDE_DIR}/geotiff.h" GEOTIFF_H_CONTENTS)
    string(REGEX MATCH "LIBGEOTIFF_VERSION[ \t]+([0-9]+)"
      LIBGEOTIFF_VERSION ${GEOTIFF_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      LIBGEOTIFF_VERSION ${LIBGEOTIFF_VERSION})

    string(SUBSTRING ${LIBGEOTIFF_VERSION} 0 1 GEOTIFF_MAJOR_VERSION)
    string(SUBSTRING ${LIBGEOTIFF_VERSION} 1 1 GEOTIFF_MINOR_VERSION)
    string(SUBSTRING ${LIBGEOTIFF_VERSION} 2 1 GEOTIFF_PATCH_VERSION)
    string(SUBSTRING ${LIBGEOTIFF_VERSION} 3 1 GEOTIFF_REVISION_VERSION)

    unset(GEOTIFF_H_CONTENTS)
    endif()

    set(GEOTIFF_VERSION_STRING "${GEOTIFF_MAJOR_VERSION}.${GEOTIFF_MINOR_VERSION}.${GEOTIFF_PATCH_VERSION}.${GEOTIFF_REVISION_VERSION}")

endif()
mark_as_advanced(GEOTIFF_LIBRARY GEOTIFF_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GeoTIFF
                                  FOUND_VAR GEOTIFF_FOUND
                                  REQUIRED_VARS GEOTIFF_LIBRARY GEOTIFF_INCLUDE_DIR
                                  VERSION_VAR GEOTIFF_VERSION_STRING)

if(GEOTIFF_FOUND)
    set(GEOTIFF_LIBRARIES ${GEOTIFF_LIBRARY})
    set(GEOTIFF_INCLUDE_DIRS ${GEOTIFF_INCLUDE_DIR})
    if(NOT TARGET GEOTIFF::GEOTIFF)
        add_library(GEOTIFF::GEOTIFF UNKNOWN IMPORTED)
        set_target_properties(GEOTIFF::GEOTIFF PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${GEOTIFF_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES C
                              IMPORTED_LOCATION ${GEOTIFF_LIBRARY})
    endif()
endif()

