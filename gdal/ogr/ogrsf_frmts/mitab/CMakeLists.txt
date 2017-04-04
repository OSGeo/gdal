# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author: Dmitriy Baryshnikov (aka Bishop), polimax@mail.ru
# ******************************************************************************
# * Copyright (C) 2012,2013 Bishop
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

set(LIB_NAME "ogrsf_mitab")
project ("lib${LIB_NAME}")

# include_directories(${GDAL_ROOT_SOURCE_DIR}/gcore)
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr)  
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr/ogrsf_frmts)  

add_definitions(-DOGR -DMITAB_USE_OFTDATETIME)

set(LIB_HHEADERS
    mitab.h
	mitab_geometry.h
	mitab_ogr_driver.h
	mitab_priv.h
	mitab_utils.h
)

set(LIB_CSOURCES
	mitab_rawbinblock.cpp
	mitab_mapheaderblock.cpp
	mitab_mapindexblock.cpp
	mitab_indfile.cpp		
	mitab_tabview.cpp
	mitab_bounds.cpp
	mitab_mapobjectblock.cpp
	mitab_mapcoordblock.cpp
	mitab_feature.cpp
	mitab_feature_mif.cpp
	mitab_mapfile.cpp
	mitab_idfile.cpp
	mitab_datfile.cpp
	mitab_tabfile.cpp
	mitab_miffile.cpp
	mitab_utils.cpp
	mitab_imapinfofile.cpp
	mitab_middatafile.cpp
	mitab_maptoolblock.cpp
	mitab_coordsys.cpp
	mitab_tooldef.cpp
	mitab_spatialref.cpp
	mitab_ogr_driver.cpp
	mitab_ogr_datasource.cpp
	mitab_geometry.cpp
	mitab_tabseamless.cpp
)

set(OGR_ENABLE_MITAB ON CACHE INTERNAL "OGR_ENABLE_MITAB")

set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
add_library(${LIB_NAME} OBJECT ${LIB_HHEADERS} ${LIB_CSOURCES})
