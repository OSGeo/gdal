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

set(LIB_NAME "frmts_gtiff")
project ("lib${LIB_NAME}")

include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts)
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr)
include_directories(${GDAL_ROOT_SOURCE_DIR}/gcore)

set(GDAL_ENABLE_FRMT_GTIFF ON CACHE INTERNAL "GDAL_ENABLE_FRMT_GTIFF")

option(GDAL_USE_LIBTIFF_INTERNAL "Set ON to use internal libtiff" OFF)
if(GDAL_USE_LIBTIFF_INTERNAL)
	include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/gtiff/libtiff)
	add_subdirectory(${GDAL_ROOT_SOURCE_DIR}/frmts/gtiff/libtiff ${GDAL_ROOT_BINARY_DIR}/frmts/gtiff/libtiff)
else(GDAL_USE_LIBTIFF_INTERNAL)
	find_package(TIFF 4.0 REQUIRED)
	if(TIFF_FOUND)
		include_directories(${TIFF_INCLUDE_DIR})
		add_definitions(-DHAVE_TIFF)
		add_definitions(-DFRMT_gtiff -DBIGTIFF_SUPPORT -DPIXARLOG_SUPPORT)
		set(GDAL_HAVE_TIFF ON CACHE INTERNAL "GDAL_HAVE_TIFF")
		set(GDAL_BIGTIFF_SUPPORT ON CACHE INTERNAL "GDAL_BIGTIFF_SUPPORT")
		set(GDAL_PIXARLOG_SUPPORT ON CACHE INTERNAL "GDAL_PIXARLOG_SUPPORT")
	endif(TIFF_FOUND)
endif(GDAL_USE_LIBTIFF_INTERNAL)
	
option(GDAL_USE_LIBJPEG_INTERNAL "Set ON to use internal libjpeg" OFF)
if(GDAL_USE_LIBJPEG_INTERNAL)	
	include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/jpeg/libjpeg)
	add_subdirectory(${GDAL_ROOT_SOURCE_DIR}/frmts/jpeg/libjpeg ${GDAL_ROOT_BINARY_DIR}/frmts/jpeg/libjpego)
else(GDAL_USE_LIBJPEG_INTERNAL)
	find_package(JPEG REQUIRED)
	if(JPEG_FOUND)
		include_directories(${JPEG_INCLUDE_DIR})
		add_definitions(-DHAVE_JPEG)
		add_definitions(-DJPEG_SUPPORT -DOJPEG_SUPPORT)
		set(GDAL_HAVE_JPEG ON CACHE INTERNAL "GDAL_HAVE_JPEG")
		set(GDAL_JPEG_SUPPORT ON CACHE INTERNAL "GDAL_JPEG_SUPPORT")
		set(GDAL_OJPEG_SUPPORT ON CACHE INTERNAL "GDAL_OJPEG_SUPPORT")
	endif(JPEG_FOUND) 
endif(GDAL_USE_LIBJPEG_INTERNAL)

include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/gtiff/libgeotiff)    
add_subdirectory(${GDAL_ROOT_SOURCE_DIR}/frmts/gtiff/libgeotiff ${GDAL_ROOT_BINARY_DIR}/frmts/gtiff/libgeotiff)	
	
set(LIB_HHEADERS
	gtiff.h
	gt_citation.h
	gt_overview.h
	gt_wkt_srs.h
	gt_wkt_srs_for_gdal.h
	tifvsi.h
	tif_float.h
	gt_jpeg_copy.h
)

set(LIB_CSOURCES
	geotiff.cpp
	gt_citation.cpp
	gt_overview.cpp
	gt_wkt_srs.cpp
	tifvsi.cpp
	tif_float.c
	gt_jpeg_copy.cpp
)

# source_group("Header Files\\port" FILES ${LIB_HHEADERS})
# source_group("Source Files\\port" FILES ${LIB_CSOURCES})

set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
add_library(${LIB_NAME} OBJECT ${LIB_HHEADERS} ${LIB_CSOURCES})	

set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB})

if(TIFF_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${TIFF_LIBRARIES})
endif(TIFF_FOUND)

if(JPEG_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${JPEG_LIBRARIES})
endif(JPEG_FOUND)

set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} PARENT_SCOPE)
