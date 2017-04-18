# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author: Dmitriy Baryshnikov (aka Bishop), polimax@mail.ru
# ******************************************************************************
# * Copyright (C) 2012 Bishop
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

cmake_minimum_required (VERSION 2.8.12)

set(LIB_NAME "ogrsf_generic")
project ("lib${LIB_NAME}")

include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr)  
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr/ogrsf_frmts)  

set(LIB_HHEADERS
        ogr_gensql.h
		ogreditablelayer.h
		ogremulatedtransaction.h
		ogrmutexeddatasource.h
		ogrmutexedlayer.h
		ogrwarpedlayer.h
    )

set(LIB_CSOURCES
        ogrsfdriverregistrar.cpp
        ogrlayer.cpp
        ogrdatasource.cpp
        ogrsfdriver.cpp
        ogrregisterall.cpp
        ogr_gensql.cpp
        ogr_attrind.cpp
        ogr_miattrind.cpp
        ogrwarpedlayer.cpp
        ogrunionlayer.cpp
        ogrlayerpool.cpp
        ogrlayerdecorator.cpp
		ogreditablelayer.cpp
		ogremulatedtransaction.cpp
        ogrmutexeddatasource.cpp
        ogrmutexedlayer.cpp
)

if(GDAL_ENABLE_OGR)
    set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
    add_library(${LIB_NAME} OBJECT ${LIB_HHEADERS} ${LIB_CSOURCES})

    install(FILES ${LIB_HHEADERS} DESTINATION include/gdal)
endif()
