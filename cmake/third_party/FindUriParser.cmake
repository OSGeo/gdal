# - Find UriParser
# Find the UriParser includes and client library
# This module defines
#  URIPARSER_INCLUDE_DIRS, where to find qhull_a.h
#  URIPARSER_LIBRARIES, libraries
#  URIPARSER_VERSION, library version
#  URIPARSER_FOUND, if false, do not try to use UriParser
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


find_path(URIPARSER_INCLUDE_DIRS UriBase.h
  PATHS
  /usr/include
  /usr/local/include
  PATH_SUFFIXES
  uriparser
)

find_library(URIPARSER_LIBRARIES NAMES liburiparser uriparser
  PATHS
  /usr/lib
  /usr/local/lib
  /usr/lib64
  /usr/local/lib64
)

if(URIPARSER_INCLUDE_DIRS)
    set(URI_VER_MAJOR 0)
    set(URI_VER_MINOR 0)
    set(URI_VER_RELEASE 0)

    set(VERSION_FILE ${URIPARSER_INCLUDE_DIRS}/uriparser/UriBase.h)

    if(EXISTS ${VERSION_FILE})
        file(READ ${VERSION_FILE} _VERSION_H_CONTENTS)
        string(REGEX MATCH "URI_VER_MAJOR[ \t]+([0-9]+)"
          URI_VER_MAJOR ${_VERSION_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          URI_VER_MAJOR ${URI_VER_MAJOR})
        string(REGEX MATCH "URI_VER_MINOR[ \t]+([0-9]+)"
          URI_VER_MINOR ${_VERSION_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          URI_VER_MINOR ${URI_VER_MINOR})
        string(REGEX MATCH "URI_VER_RELEASE[ \t]+([0-9]+)"
          URI_VER_RELEASE ${_VERSION_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          URI_VER_RELEASE ${URI_VER_RELEASE})

        unset(_VERSION_H_CONTENTS)
    endif()

    set(URIPARSER_VERSION_STRING "${URI_VER_MAJOR}.${URI_VER_MINOR}.${URI_VER_RELEASE}")   
endif ()

# Handle the QUIETLY and REQUIRED arguments and set SPATIALINDEX_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(URIPARSER
                                  REQUIRED_VARS URIPARSER_LIBRARIES URIPARSER_INCLUDE_DIRS
                                  VERSION_VAR URIPARSER_VERSION_STRING)


# Hide internal variables
mark_as_advanced(URIPARSER_INCLUDE_DIRS URIPARSER_LIBRARIES)
