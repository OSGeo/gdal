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

set(LIB_NAME "frmts_libpng")
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

set(HHEADERS
	png.h
	pngconf.h
)

set(LIB_CSOURCES
	png.c
	pngerror.c
	pnggccrd.c
	pngget.c
	pngmem.c
	pngpread.c
	pngread.c
	pngrio.c
	pngrtran.c
	pngrutil.c
	pngset.c
	pngtrans.c
	pngvcrd.c
	pngwio.c
	pngwrite.c
	pngwtran.c
	pngwutil.c
)

set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
add_library(${LIB_NAME} OBJECT ${LIB_CSOURCES})	

