################################################################################
# Project:  CMake4GDAL
# Purpose:  CMake build scripts
# Author:   Dmitry Baryshnikov, polimax@mail.ru
################################################################################
# Copyright (C) 2015-2020, NextGIS <info@nextgis.com>
# Copyright (C) 2012-2020 Dmitry Baryshnikov
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
################################################################################

cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)

project(gdal_thrid_party)


# Some init settings
set(CMAKE_COLOR_MAKEFILE ON)
# Set path to additional CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../cmake/third_party ${CMAKE_MODULE_PATH})

if(NOT SKIP_DEFAULTS)
    include(defaults)
endif()


# Setup package meta-data
include(CMakeDependentOption)
include(third_party_util)
include(FindAnyProject)


# find_anyproject(ICONV DEFAULT ON) # If not set anything - default OFF, set ON to check ON by DEFAULT
# if(ICONV_FOUND)
    # set(HAVE_ICONV TRUE)
    # add_definitions(-DHAVE_ICONV)
    # if(${ICONV_SECOND_ARGUMENT_IS_CONST})
        # set(ICONV_CONST "const")
    # endif()

    # if(${ICONV_SECOND_ARGUMENT_CPP_IS_CONST})
        # set(ICONV_CPP_CONST "const")
    # endif()
# else()
    # message(WARNING "No iconv support")
# endif()

# find_anyproject(ZLIB DEFAULT ON)
# if(ZLIB_FOUND)
    # add_definitions(-DHAVE_LIBZ -DZIP_SUPPORT)
# else()
    # message(WARNING "No zlib support")
# endif()

# find_anyproject(EXPAT DEFAULT ON)
# if(EXPAT_FOUND)
    # add_definitions(-DHAVE_EXPAT)
    # if(NOT BUILD_SHARED_LIBS)
        # add_definitions(-DXML_STATIC)
    # endif()
# endif()

# find_anyproject(JSONC REQUIRED
    # NAMES json-c jsonc json_c
# )
# if(JSONC_FOUND)
    # include_directories(${JSONC_INCLUDE_DIRS}/json-c)
# endif()

# find_anyproject(CURL DEFAULT ON CMAKE_ARGS
    # -DBUILD_CURL_EXE=OFF
    # -DHTTP_ONLY=ON
    # -DUSE_MANUAL=OFF)
# if(CURL_FOUND)
    # add_definitions(-DHAVE_CURL)
# else()
    # message(WARNING "No curl support")
# endif()

# find_anyproject(LibXml2 DEFAULT ON)
# if(LIBXML2_FOUND)
    # add_definitions(-DHAVE_LIBXML2)
# endif()

# # This is here as geos used in core/ogr and apps
# find_anyproject(GEOS DEFAULT ON)
# if(GEOS_FOUND)
    # add_definitions(-DHAVE_GEOS=1)
# else()
    # message(WARNING "No geos support")
# endif()

# find_anyproject(LibLZMA)
# if(LIBLZMA_FOUND)
    # add_definitions(-DHAVE_LZMA_H=1)
# endif()


find_anyproject(SQLite3 CMAKE_ARGS -DENABLE_COLUMN_METADATA=ON)
if(SQLITE3_FOUND)
    add_definitions(-DHAVE_SQLITE=1)
    option(ENABLE_SQLITE_GPKG "Build SQLite & GeoPackage (raster + vector) formats" ON)
    if(ENABLE_SQLITE_GPKG)
        add_definitions(-DSQLITE_ENABLED)
    endif()
endif()

find_anyproject(PROJ REQUIRED)
if(PROJ_FOUND)
    add_definitions(-DHAVE_PROJ_H=1)
    # if(NOT BUILD_STATIC_LIBS)
    #     add_definitions(-DPROJ_RENAME_SYMBOLS)
    # endif()    
endif()



SET(LIB_NAME ${PROJECT_NAME})
ADD_EXECUTABLE(${LIB_NAME} main.cpp)


target_link_extlibraries(${LIB_NAME})

