# - Find libkml
# Find the libkml includes and client library
# This module defines
#  KML_INCLUDE_DIRS, where to find qhull_a.h
#  KML_LIBRARIES, libraries
#  KML_VERSION, library version
#  KML_FOUND, if false, do not try to use UriParser
#
# Copyright (c) 2018, NextGIS, <info@nextgis.com>
# Copyright (c) 2018, Dmitry Baryshnikov, <dmitry.baryshnikov@nextgis.com>
#
# This script is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This script is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this script.  If not, see <http://www.gnu.org/licenses/>.


find_path(KML_INCLUDE_DIRS version.h
  PATH_SUFFIXES
  kml/base
)

if(KML_INCLUDE_DIRS)
    get_filename_component(KML_INCLUDE_DIRS "${KML_INCLUDE_DIRS}" PATH)
    get_filename_component(KML_INCLUDE_DIRS "${KML_INCLUDE_DIRS}" PATH)
endif()

set(LIB_NAMES
    kml
    kmlbase
    kmlconvenience
    kmldom
    kmlengine
    kmlregionator
    kmlxsd
    minizip
)

foreach(LIB_NAME ${LIB_NAMES})

    unset(${LIB_NAME}_LIBRARY)
    find_library(${LIB_NAME}_LIBRARY
        NAMES lib${LIB_NAME} ${LIB_NAME}
    )

    if(${LIB_NAME}_LIBRARY)
        set(KML_LIBRARIES ${KML_LIBRARIES} ${${LIB_NAME}_LIBRARY})
    endif()
endforeach()

if(KML_INCLUDE_DIRS)
    set(LIBKML_MAJOR_VERSION 0)
    set(LIBKML_MINOR_VERSION 0)
    set(LIBKML_MICRO_VERSION 0)

    set(VERSION_FILE ${KML_INCLUDE_DIRS}/kml/base/version.h)

    if(EXISTS ${VERSION_FILE})
        file(READ ${VERSION_FILE} _VERSION_H_CONTENTS)

        string(REGEX MATCH "LIBKML_MAJOR_VERSION[ \t]+([0-9]+)"
          LIBKML_MAJOR_VERSION ${_VERSION_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          LIBKML_MAJOR_VERSION ${LIBKML_MAJOR_VERSION})
        string(REGEX MATCH "LIBKML_MINOR_VERSION[ \t]+([0-9]+)"
          LIBKML_MINOR_VERSION ${_VERSION_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          LIBKML_MINOR_VERSION ${LIBKML_MINOR_VERSION})
        string(REGEX MATCH "LIBKML_MICRO_VERSION[ \t]+([0-9]+)"
          LIBKML_MICRO_VERSION ${_VERSION_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          LIBKML_MICRO_VERSION ${LIBKML_MICRO_VERSION})

        unset(_VERSION_H_CONTENTS)
    endif()

    set(KML_VERSION_STRING "${LIBKML_MAJOR_VERSION}.${LIBKML_MINOR_VERSION}.${LIBKML_MICRO_VERSION}")
endif ()

# Handle the QUIETLY and REQUIRED arguments and set SPATIALINDEX_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KML
                                  REQUIRED_VARS KML_LIBRARIES KML_INCLUDE_DIRS
                                  VERSION_VAR KML_VERSION_STRING)


# Hide internal variables
mark_as_advanced(KML_INCLUDE_DIRS KML_LIBRARIES)
