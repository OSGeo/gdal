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

set(LIB_NAME "geotiff")
project ("lib${LIB_NAME}")

if(GDAL_USE_LIBTIFF_INTERNAL)
	include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/gtiff/libtiff)
else(GDAL_USE_LIBTIFF_INTERNAL)	
	include_directories(${TIFF_INCLUDE_DIR})
endif(GDAL_USE_LIBTIFF_INTERNAL)

set(LIB_HHEADERS
    cpl_serv.h
    gdal_libgeotiff_symbol_rename.h
    geo_config.h
    geo_keyp.h
    geo_normalize.h
    geo_simpletags.h
    geo_tiffp.h
    geokeys.h
    geonames.h
    geotiff.h
    geotiffio.h
    geovalues.h
    xtiffio.h
)

set(LIB_CSOURCES
	xtiff.c
	geo_free.c
	geo_get.c
	geo_names.c
	geo_new.c
	geo_print.c
	geo_set.c
	geo_tiffp.c
	geo_write.c
	geo_normalize.c
	geotiff_proj4.c
	geo_extra.c
	geo_trans.c
	geo_simpletags.c
)

# source_group("Header Files\\port" FILES ${LIB_HHEADERS})
# source_group("Source Files\\port" FILES ${LIB_CSOURCES})

set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
add_library(${LIB_NAME} OBJECT ${LIB_CSOURCES})	#${LIB_HHEADERS} 
