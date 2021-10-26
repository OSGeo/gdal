# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst:
# FindShapelib
# -----------
#
# Copyright (c) 2018, Hiroshi Miura <miurahr@linux.com>
#

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_Shapelib QUIET shapelib)
endif()

find_path(Shapelib_INCLUDE_DIR
          NAMES shapefil.h
          HINTS ${PC_Shapelib_INCLUDE_DIRS})

if(Shapelib_INCLUDE_DIR)
    if(PC_Shapelib_VERSION)
        set(Shapelib_VERSION ${PC_Shapelib_VERSION})
    else()
        file(READ ${Shapelib_INCLUDE_DIR}/shapefil.h _shapefil_h_contents)
        string(REGEX MATCH "Id: shapefil.h,v ([0-9.]+)" Shapelib_H_VERSION "${_shapefil_h_contents}")
        string(REGEX MATCH "([0-9].[0-9][0-9])" Shapelib_H_VERSION "${Shapelib_H_VERSION}")
        # shapefil.h 1.26 = release 1.2.10
        # shapefil.h 1.52 = release 1.3.0
        # shapefil.h 1.55 = release 1.4.0, 1.4.1
        if(Shapelib_H_VERSION VERSION_LESS 1.26)
            message(WARNING "Shapelib version detection failed")
        elseif(Shapelib_H_VERSION VERSION_LESS 1.52)
            set(Shapelib_VERSION 1.2.10)
        elseif(Shapelib_H_VERSION VERSION_LESS 1.55)
            set(Shapelib_VERSION 1.3.0)
        else()
            set(Shapelib_VERSION 1.4.0)
        endif()
    endif()
endif()

if(MSVC)
    set(Shapelib_LIBNAME shapelib)
else()
    set(Shapelib_LIBNAME shp)
endif()

find_library(Shapelib_LIBRARY NAMES ${Shapelib_LIBNAME})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Shapelib
                                  FOUND_VAR Shapelib_FOUND
                                  REQUIRED_VARS Shapelib_INCLUDE_DIR Shapelib_LIBRARY
                                  VERSION_VAR Shapelib_VERSION
)

mark_as_advanced(Shapelib_INCLUDE_DIR Shapelib_LIBRARY)

if(Shapelib_FOUND)
    set(Shapelib_INCLUDE_DIRS ${Shapelib_INCLUDE_DIR})
    set(Shapelib_LIBRARIES ${Shapelib_LIBRARY})
    if(NOT TARGET SHAPELIB::shp)
        add_library(SHAPELIB::shp UNKNOWN IMPORTED)
        set_target_properties(SHAPELIB::shp PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${Shapelib_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES C
                              IMPORTED_LOCATION ${Shapelib_LIBRARY})
    endif()
endif()
