# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author:   Hiroshi Miura <miurahr@linux.com>
# ******************************************************************************
# * Copyright (C) 2012,2013 Bishop
# *           (C) 2017 Hiroshi Miura
# * 
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included
# * in all copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# * DEALINGS IN THE SOFTWARE.
# ******************************************************************************

cmake_minimum_required (VERSION 2.8.12)

set(LIB_NAME "ogrsf_gmlas")
project ("lib${LIB_NAME}")

include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr)    
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr/ogrsf_frmts)    

set(LIB_HHEADERS
        ogr_gmlas.h
        ogr_gmlas_consts.h
)

set(LIB_CSOURCES
        ogrgmlasconf.cpp
        ogrgmlasdatasource.cpp
        ogrgmlasfeatureclass.cpp
        ogrgmlasdriver.cpp
        ogrgmlaslayer.cpp
        ogrgmlasreader.cpp
        ogrgmlasschemaanalyzer.cpp
        ogrgmlaswriter.cpp
        ogrgmlasxlinkresolver.cpp
        ogrgmlasxpatchmatcher.cpp
        ogrgmlasxsdcache.cpp
)

find_package(XercesC)
if(XercesC_FOUND)
    include_directories(${XercesC_INCLUDE_DIRS})
    add_definitions(-DHAVE_XERCES)
    set(GDAL_HAVE_XERCES ON)
 	set(LIB_HHEADERS ${LIB_HHEADERS}
            xercesc_headers.h
	)
endif()

if (OGR_ENABLE_GMLAS)
    set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
    add_library(${LIB_NAME} OBJECT ${LIB_HHEADERS} ${LIB_CSOURCES})
    set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} PARENT_SCOPE)
endif ()

