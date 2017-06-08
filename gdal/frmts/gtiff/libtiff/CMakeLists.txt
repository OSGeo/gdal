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

set(LIB_NAME "tiff")
project ("lib${LIB_NAME}")

if(GDAL_USE_LIBZ_INTERNAL)
	include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/zlib)
else(GDAL_USE_LIBZ_INTERNAL)	
	include_directories(${ZLIB_INCLUDE_DIRS})
endif(GDAL_USE_LIBZ_INTERNAL)

 set(LIB_HHEADERS
	gdal_libtiff_symbol_rename.h
	t4.h
	tif_config.h
	tif_dir.h
	tif_fax3.h
	tif_predict.h
	tiff.h
	tiffconf.h
	tiffio.h
	tiffiop.h
    tiffvers.h
    uvcode.h
 )

set(LIB_CSOURCES
	tif_aux.c
	tif_close.c
	tif_codec.c
	tif_color.c
	tif_compress.c
	tif_dir.c
	tif_dirinfo.c
	tif_dirread.c
	tif_dirwrite.c
	tif_dumpmode.c
	tif_error.c
	tif_extension.c
	tif_fax3.c
	tif_fax3sm.c
	tif_getimage.c
	tif_jpeg.c
	tif_jpeg_12.c
	tif_flush.c
	tif_luv.c
	tif_lzw.c
	tif_next.c
	tif_ojpeg.c
	tif_open.c
	tif_packbits.c
	tif_pixarlog.c
	tif_predict.c
	tif_print.c
	tif_read.c
	tif_swab.c
	tif_strip.c
	tif_thunder.c
	tif_tile.c
	tif_vsi.c
	tif_version.c
	tif_warning.c
	tif_write.c
	tif_zip.c
	tif_lzma.c
)

# source_group("Header Files\\port" FILES ${LIB_HHEADERS})
# source_group("Source Files\\port" FILES ${LIB_CSOURCES})

set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
add_library(${LIB_NAME} OBJECT ${LIB_CSOURCES} ${LIB_HHEADERS} )
