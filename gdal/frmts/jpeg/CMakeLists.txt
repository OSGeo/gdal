# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author: Dmitriy Baryshnikov (aka Bishop), polimax@mail.ru
# ******************************************************************************
# * Copyright (C) 2013 Bishop
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

set(LIB_NAME "frmts_jpeg")
project ("lib${LIB_NAME}")

include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts)
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr)
include_directories(${GDAL_ROOT_SOURCE_DIR}/gcore)

option(GDAL_USE_LIBJPEG_INTERNAL "Set ON to use internal libjpeg" OFF)
if(GDAL_USE_LIBJPEG_INTERNAL)	
	include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/jpeg/libjpeg)
	add_subdirectory(${GDAL_ROOT_SOURCE_DIR}/frmts/jpeg/libjpeg ${GDAL_ROOT_BINARY_DIR}/frmts/jpeg/libjpegm)
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

#check zip and internal zip
if(GDAL_USE_LIBZ)
	add_definitions(-DHAVE_LIBZ -DZIP_SUPPORT)
	if(GDAL_USE_LIBZ_INTERNAL)
		include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/zlib)
	else(GDAL_USE_LIBZ_INTERNAL)
		find_package(ZLIB REQUIRED)
		if(ZLIB_FOUND)
			include_directories(${ZLIB_INCLUDE_DIRS})
			add_definitions(-DHAVE_ZLIB_H -DHAVE_ZLIB)
			if(MSVC)
				add_definitions(-DZLIB_DLL)
			endif(MSVC)
		else(ZLIB_FOUND)	
			MESSAGE(FATAL_ERROR "Could not find ZLIB")
		endif(ZLIB_FOUND)		
	endif(GDAL_USE_LIBZ_INTERNAL)
endif(GDAL_USE_LIBZ)	

set(HHEADERS
	gdalexif.h
	vsidataio.h
)

set(LIB_CSOURCES
	vsidataio.cpp
	jpgdataset.cpp
)

if(GDAL_JPEG12_SUPPORTED)
	set(LIB_CSOURCES ${LIB_CSOURCES}
		jpgdataset_12.cpp
	)
endif(GDAL_JPEG12_SUPPORTED)

set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
add_library(${LIB_NAME} OBJECT ${LIB_CSOURCES})	

set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB})

if(JPEG_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${JPEG_LIBRARIES})
endif(JPEG_FOUND)

if(ZLIB_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${ZLIB_LIBRARIES})
endif(ZLIB_FOUND)

set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} PARENT_SCOPE)
