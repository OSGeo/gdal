/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implement system hook functions for libtiff on top of CPL/VSI,
 *           including > 2GB support.  Based on tif_unix.c from libtiff
 *           distribution.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam, warmerdam@pobox.com
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef TIFVSI_H_INCLUDED
#define TIFVSI_H_INCLUDED

#include "cpl_port.h"
#include "cpl_vsi.h"
#include "tiffio.h"

TIFF CPL_DLL * VSI_TIFFOpen( const char* name, const char* mode, VSILFILE* fp );
TIFF* VSI_TIFFOpenChild( TIFF* parent ); // the returned handle must be closed before the parent. They share the same underlying VSILFILE
TIFF* VSI_TIFFReOpen( TIFF* tif );
VSILFILE* VSI_TIFFGetVSILFile( thandle_t th );
int VSI_TIFFFlushBufferedWrite( thandle_t th );
toff_t VSI_TIFFSeek(TIFF* tif, toff_t off, int whence );
int VSI_TIFFWrite( TIFF* tif, const void* buffer, size_t buffersize );
int VSI_TIFFHasCachedRanges( thandle_t th );
void VSI_TIFFSetCachedRanges( thandle_t th, int nRanges,
                              void ** ppData, // memory pointed by ppData[i] must be kept alive by caller
                              const vsi_l_offset* panOffsets,
                              const size_t* panSizes );
void* VSI_TIFFGetCachedRange( thandle_t th, vsi_l_offset nOffset, size_t nSize );

#endif // TIFVSI_H_INCLUDED
