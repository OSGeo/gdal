# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author: Dmitriy Baryshnikov (aka Bishop), polimax@mail.ru
# ******************************************************************************
# * Copyright (C) 2012 Bishop
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

set(LIB_NAME "frmts")
project ("lib${LIB_NAME}")

include_directories(${GDAL_ROOT_SOURCE_DIR}/gcore)

set(LIB_CSOURCES
	gdalallregister.cpp
)

add_definitions(-DFRMT_gtiff)
add_subdirectory(gtiff)
add_definitions(-DFRMT_hfa)
add_subdirectory(hfa)
add_definitions(-DFRMT_vrt)
add_subdirectory(vrt)
#maybe make depended by s57 ogr driver
add_definitions(-DFRMT_iso8211)
add_subdirectory(iso8211)

option(GDAL_ENABLE_FRMT_BMP "Set ON to build bmp driver" OFF)
if(GDAL_ENABLE_FRMT_BMP)
	add_definitions(-DFRMT_bmp)
	add_subdirectory(bmp)
endif(GDAL_ENABLE_FRMT_BMP)

option(GDAL_ENABLE_FRMT_DIMAP "Set ON to build dimap driver" OFF)
if(GDAL_ENABLE_FRMT_DIMAP)
	add_definitions(-DFRMT_dimap)
	add_subdirectory(dimap)
endif(GDAL_ENABLE_FRMT_DIMAP)

option(GDAL_ENABLE_FRMT_MEM "Set ON to build memory raster driver" OFF)
if(GDAL_ENABLE_FRMT_MEM)
	add_definitions(-DFRMT_mem)
	add_subdirectory(mem)
endif(GDAL_ENABLE_FRMT_MEM)

option(GDAL_ENABLE_FRMT_RAW "Set ON to build raw driver" OFF)
if(GDAL_ENABLE_FRMT_RAW)
	add_definitions(-DFRMT_raw)
	add_subdirectory(raw)
endif(GDAL_ENABLE_FRMT_RAW)

# need by apps
# option(GDAL_ENABLE_FRMT_VRT "Set ON to build virtual raster driver" OFF)
# if(GDAL_ENABLE_FRMT_VRT)
	# add_definitions(-DFRMT_vrt)
	# add_subdirectory(vrt)
# endif(GDAL_ENABLE_FRMT_VRT)

option(GDAL_ENABLE_FRMT_SAGA "Set ON to build saga raster driver" OFF)
if(GDAL_ENABLE_FRMT_SAGA)
	add_definitions(-DFRMT_saga)
	add_subdirectory(saga)
endif(GDAL_ENABLE_FRMT_SAGA)

option(GDAL_ENABLE_FRMT_JPEG "Set ON to build jpeg driver" OFF)
if(GDAL_ENABLE_FRMT_JPEG)
	add_definitions(-DFRMT_jpeg)
	add_subdirectory(jpeg)
endif(GDAL_ENABLE_FRMT_JPEG)

option(GDAL_ENABLE_FRMT_PNG "Set ON to build png driver" OFF)
if(GDAL_ENABLE_FRMT_PNG)
	add_definitions(-DFRMT_png)
	add_subdirectory(png)
endif(GDAL_ENABLE_FRMT_PNG)

option(GDAL_ENABLE_FRMT_GIF "Set ON to build gif driver" OFF)
if(GDAL_ENABLE_FRMT_GIF)
	add_definitions(-DFRMT_gif)
	add_subdirectory(gif)
endif(GDAL_ENABLE_FRMT_GIF)

option(GDAL_ENABLE_FRMT_TIL "Set ON to build til driver" OFF)
if(GDAL_ENABLE_FRMT_TIL)
	add_definitions(-DFRMT_til)
	add_subdirectory(til)
endif(GDAL_ENABLE_FRMT_TIL)

option(GDAL_ENABLE_FRMT_OZI "Set ON to build ozi driver" OFF)
if(GDAL_ENABLE_FRMT_OZI)
	add_definitions(-DFRMT_ozi)
	add_subdirectory(ozi)
endif(GDAL_ENABLE_FRMT_OZI)

option(GDAL_ENABLE_FRMT_OZIMAP "Set ON to build ozi map driver" OFF)
if(GDAL_ENABLE_FRMT_OZIMAP)
	add_definitions(-DFRMT_map)
	add_subdirectory(map)
endif(GDAL_ENABLE_FRMT_OZIMAP)

option(GDAL_ENABLE_FRMT_POSTGISRASTER "Set ON to build postgis raster driver" OFF)
if(GDAL_ENABLE_FRMT_POSTGISRASTER)
	add_definitions(-DFRMT_postgisraster)
	add_subdirectory(postgisraster)
endif(GDAL_ENABLE_FRMT_POSTGISRASTER)

include(CMakeDependentOption)

CMAKE_DEPENDENT_OPTION(GDAL_ENABLE_FRMT_WMS "Set ON to build wms driver" OFF "GDAL_USE_CURL" OFF)
#option(GDAL_ENABLE_FRMT_WMS "Set ON to build wms driver" OFF)
if(GDAL_ENABLE_FRMT_WMS)
	add_definitions(-DFRMT_wms)
	add_subdirectory(wms)
endif(GDAL_ENABLE_FRMT_WMS)

set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} PARENT_SCOPE)
add_library(${LIB_NAME} OBJECT ${LIB_CSOURCES})

