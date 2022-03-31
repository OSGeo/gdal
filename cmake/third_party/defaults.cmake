################################################################################
# Project:  CMake4GDAL
# Purpose:  CMake build scripts
# Author:   Dmitry Baryshnikov, polimax@mail.ru
################################################################################
# Copyright (C) 2017, NextGIS <info@nextgis.com>
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
 
set(WITH_EXPAT ON CACHE BOOL "Build with expat")
set(WITH_CURL ON CACHE BOOL "Build with curl")
set(WITH_GEOS ON CACHE BOOL  "Build with geos")
set(WITH_PROJ4 ON CACHE BOOL  "Build with proj.4")
set(WITH_GeoTIFF ON CACHE BOOL "Build with geotiff")
set(WITH_ICONV ON CACHE BOOL "Build with iconv")
set(WITH_JSONC ON CACHE BOOL "Build with json-c")
set(WITH_LibXml2 ON CACHE BOOL "Build with xml2")
set(WITH_TIFF ON CACHE BOOL "Build with tiff")
set(WITH_ZLIB ON CACHE BOOL "Build with zlib")
set(WITH_JBIG ON CACHE BOOL "Build with jbig")
set(WITH_JPEG ON CACHE BOOL "Build with jpeg")
set(WITH_JPEG12 ON CACHE BOOL "Build with jpeg12")
set(WITH_LibLZMA ON CACHE BOOL "Build with lzma")
set(WITH_PYTHON ON CACHE BOOL "Build with python")
set(WITH_PYTHON3 OFF CACHE BOOL "Build with python3")
set(WITH_PNG ON CACHE BOOL "Build with png")
set(WITH_OpenSSL ON CACHE BOOL "Build with openssl")
set(WITH_SQLite3 ON CACHE BOOL "Build with sqlite3")
set(WITH_PostgreSQL ON CACHE BOOL "Build with postgres")
set(WITH_OpenCAD ON CACHE BOOL "Build with opencad")

set(GDAL_ENABLE_GNM ON CACHE BOOL "Build geometry network model support")
set(GDAL_BUILD_APPS ON CACHE BOOL "Build applications")
set(CMAKE_BUILD_TYPE Release)
if(APPLE)
    set(OSX_FRAMEWORK ON CACHE INTERNAL "Build Mac OS X framework")
elseif(WIN32 OR UNIX)
    set(BUILD_SHARED_LIBS ON CACHE INTERNAL "Build shared library")
endif()

if(WIN32)
    set(WITH_EXPAT_EXTERNAL ON CACHE BOOL "Build with expat external")
    set(WITH_CURL_EXTERNAL ON CACHE BOOL "Build with curl external")
    set(WITH_GEOS_EXTERNAL ON CACHE BOOL  "Build with geos external")
    set(WITH_PROJ_EXTERNAL ON CACHE BOOL  "Build with proj.4 external")
    set(WITH_GeoTIFF_EXTERNAL ON CACHE BOOL "Build with geotiff external")
    set(WITH_ICONV_EXTERNAL ON CACHE BOOL "Build with iconv external")
    set(WITH_JSONC_EXTERNAL ON CACHE BOOL "Build with json-c external")
    set(WITH_LibXml2_EXTERNAL ON CACHE BOOL "Build with xml2 external")
    set(WITH_TIFF_EXTERNAL ON CACHE BOOL "Build with tiff external")
    set(WITH_ZLIB_EXTERNAL ON CACHE BOOL "Build with zlib external")
    set(WITH_JBIG_EXTERNAL ON CACHE BOOL "Build with jbig")
    set(WITH_JPEG_EXTERNAL ON CACHE BOOL "Build with jpeg external")
    set(WITH_JPEG12_EXTERNAL ON CACHE BOOL "Build with jpeg12 external")
    set(WITH_LibLZMA_EXTERNAL ON CACHE BOOL "Build with lzma external")
	set(WITH_GTEST_EXTERNAL ON CACHE BOOL "Build with jpeg12 external")
    set(WITH_PNG_EXTERNAL ON CACHE BOOL "Build with png external")
    set(WITH_OpenSSL_EXTERNAL ON CACHE BOOL "Build with openssl external")
    set(WITH_SQLite3_EXTERNAL ON CACHE BOOL "Build with sqlite3 external")
    set(WITH_PostgreSQL_EXTERNAL ON CACHE BOOL "Build with postgres external")
    set(WITH_OpenCAD_EXTERNAL ON CACHE BOOL "Build with opencad external")
endif()
