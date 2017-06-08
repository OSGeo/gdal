# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author:   Hiroshi Miura <miurahr@linux.com>
# ******************************************************************************
# * Copyright (C) 2017 Hiroshi Miura
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

cmake_minimum_required (VERSION 2.8.10)

set(LIB_NAME "ogrsf_nas")
project ("lib${LIB_NAME}")

include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr)    
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr/ogrsf_frmts)


set(LIB_HHEADERS
        ogr_nas.h
        nasreaderp.h
)

set(LIB_CSOURCES
        ogrnasdatasource.cpp
        ogrnasdriver.cpp
        ogrnaslayer.cpp
        ogrnasrelationlayer.cpp
        nashandler.cpp
        nasreader.cpp
)

find_package(EXPAT)
if(EXPAT_FOUND)
    include_directories(${EXPAT_INCLUDE_DIRS})
    add_definitions(-DHAVE_EXPAT)
    set(GDAL_HAVE_EXPAT ON CACHE INTERNAL "GDAL_HAVE_EXPAT")
endif(EXPAT_FOUND)

if(OGR_ENABLE_NAS)
    set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
    add_library(${LIB_NAME} OBJECT ${LIB_HHEADERS} ${LIB_CSOURCES})

    find_package(XercesC REQUIRED)
    if(XercesC_FOUND)
        include_directories(${XercesC_INCLUDE_DIRS})
        add_definitions(-DHAVE_XERCES)
        set(GDAL_HAVE_XERCES ON CACHE INTERNAL "GDAL_HAVE_XERCES")
        set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${XercesC_LIBRARIES})
    endif()

    set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB})

    if(EXPAT_FOUND)
        set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${EXPAT_LIBRARIES})
    endif()

    if(XERCES_FOUND)
        set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${XercesC_LIBRARIES})
    endif()

    set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} PARENT_SCOPE)
endif()
