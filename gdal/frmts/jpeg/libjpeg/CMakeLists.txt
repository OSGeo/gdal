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

set(LIB_NAME "jpeg")
project ("lib${LIB_NAME}")

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

add_definitions(-DDEFAULT_MAX_MEM=500000000L)

option(GDAL_JPEG12_SUPPORTED "Set ON to use libjpeg12 support" OFF)

set(LIB_HHEADERS
	jchuff.h
	jconfig.h
	jdct.h
	jdhuff.h
	jerror.h
	jinclude.h
	jmemsys.h
	jpegint.h
	jpeglib.h
	jversion.h
)

set(LIB_CSOURCES
	jcapimin.c
	jcapistd.c
	jccoefct.c
	jccolor.c
	jcdctmgr.c
	jchuff.c
    jcinit.c
	jcmainct.c
	jcmarker.c
	jcmaster.c
	jcomapi.c
	jcparam.c
	jcphuff.c 
	jcprepct.c
	jcsample.c
	jctrans.c
	jdapimin.c
	jdapistd.c
	jdatadst.c
	jdatasrc.c
	jdcoefct.c
	jdcolor.c
	jddctmgr.c
	jdhuff.c
	jdinput.c
	jdmainct.c
	jdmarker.c
	jdmaster.c
	jdmerge.c
	jdphuff.c
	jdpostct.c
	jdsample.c
	jdtrans.c
	jerror.c
	jfdctflt.c
	jfdctfst.c
	jfdctint.c
	jidctflt.c
	jidctfst.c
	jidctint.c
	jidctred.c
	jquant1.c
	jquant2.c
	jutils.c
	jmemmgr.c
	jmemansi.c 
)

if(GDAL_JPEG12_SUPPORTED)
	add_definitions(-DJPEG_DUAL_MODE_8_12)
	include_directories(${GDAL_ROOT_BINARY_DIR}/frmts/jpeg/libjpeg)    
	file(COPY ${GDAL_ROOT_SOURCE_DIR}/frmts/jpeg/libjpeg12/jmorecfg.h.12 DESTINATION ${GDAL_ROOT_BINARY_DIR}/frmts/jpeg/libjpeg)
	file(RENAME ${GDAL_ROOT_BINARY_DIR}/frmts/jpeg/libjpeg/jmorecfg.h.12 ${GDAL_ROOT_BINARY_DIR}/frmts/jpeg/libjpeg/jmorecfg.h)
	set(LIB_HHEADERS ${LIB_HHEADERS}
		${GDAL_ROOT_BINARY_DIR}/frmts/jpeg/libjpeg/jmorecfg.h
	)
else(GDAL_JPEG12_SUPPORTED)
	set(LIB_HHEADERS ${LIB_HHEADERS}
		jmorecfg.h
	)
endif(GDAL_JPEG12_SUPPORTED)

# check if we already have jpeglib
list(FIND GDAL_TARGET_OBJECTS "$<TARGET_OBJECTS:${LIB_NAME}>" JPEGLIB_INDEX)
if(${JPEGLIB_INDEX} EQUAL "-1")
	set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
	add_library(${LIB_NAME} OBJECT ${LIB_CSOURCES}	${LIB_HHEADERS}) 
endif(${JPEGLIB_INDEX} EQUAL "-1")