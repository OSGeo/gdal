/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GTIFF_H_INCLUDED
#define GTIFF_H_INCLUDED

#include "cpl_port.h"
#include "cpl_string.h"

#include "gdal.h"
#include "tiffio.h"

CPL_C_START
int    GTiffOneTimeInit();
void CPL_DLL LibgeotiffOneTimeInit();
CPL_C_END

void    GTIFFSetInExternalOvr( bool b );
void    GTIFFGetOverviewBlockSize( int* pnBlockXSize, int* pnBlockYSize );
void    GTIFFSetJpegQuality( GDALDatasetH hGTIFFDS, int nJpegQuality );
void    GTIFFSetJpegTablesMode( GDALDatasetH hGTIFFDS, int nJpegTablesMode );
int     GTIFFGetCompressionMethod( const char* pszValue,
                                   const char* pszVariableName );

void GTiffDatasetWriteRPCTag( TIFF *hTIFF, char **papszRPCMD );
char** GTiffDatasetReadRPCTag( TIFF *hTIFF );

void GTiffWriteJPEGTables( TIFF* hTIFF,
                           const char* pszPhotometric,
                           const char* pszJPEGQuality,
                           const char* pszJPEGTablesMode );
CPLString GTiffFormatGDALNoDataTagValue( double dfNoData );

const int knGTIFFJpegTablesModeDefault = 1; /* JPEGTABLESMODE_QUANT */

// Note: Was EXTRASAMPLE_ASSOCALPHA in GDAL < 1.10.
constexpr uint16 DEFAULT_ALPHA_TYPE = EXTRASAMPLE_UNASSALPHA;

uint16 GTiffGetAlphaValue(const char* pszValue, uint16 nDefault);

#define TIFFTAG_GDAL_METADATA  42112
#define TIFFTAG_GDAL_NODATA    42113
#define TIFFTAG_RPCCOEFFICIENT 50844

/* GeoTIFF DGIWG */
/* https://www.awaresystems.be/imaging/tiff/tifftags/tiff_rsid.html */
#define TIFFTAG_TIFF_RSID      50908
/* https://www.awaresystems.be/imaging/tiff/tifftags/geo_metadata.html */
#define TIFFTAG_GEO_METADATA   50909

#if defined(TIFFLIB_VERSION) && TIFFLIB_VERSION >= 20081217 && defined(BIGTIFF_SUPPORT)
#  define HAVE_UNSETFIELD
#endif

#if defined(TIFFLIB_VERSION) && TIFFLIB_VERSION > 20041016
/* We need at least TIFF 3.7.0 for TIFFGetSizeProc and TIFFClientdata */
#  define HAVE_TIFFGETSIZEPROC
#endif

#if !defined(PREDICTOR_NONE)
#define PREDICTOR_NONE 1
#endif

#if !defined(COMPRESSION_LZMA)
#define     COMPRESSION_LZMA        34925   /* LZMA2 */
#endif

#if !defined(TIFFTAG_LZMAPRESET)
#define TIFFTAG_LZMAPRESET      65562   /* LZMA2 preset (compression level) */
#endif

#if !defined(COMPRESSION_ZSTD)
#define     COMPRESSION_ZSTD        50000   /* ZSTD */
#endif

#if !defined(TIFFTAG_ZSTD_LEVEL)
#define TIFFTAG_ZSTD_LEVEL      65564    /* ZSTD compression level */
#endif
 
#if !defined(COMPRESSION_LERC)
#define     COMPRESSION_LERC        34887   /* LERC */
#endif

#if !defined(COMPRESSION_WEBP)
#define     COMPRESSION_WEBP        50001   /* WebP */
#endif

#if !defined(TIFFTAG_WEBP_LEVEL)
#define     TIFFTAG_WEBP_LEVEL        65568   /* WebP compression level */
#endif

#if !defined(TIFFTAG_WEBP_LOSSLESS)
#define     TIFFTAG_WEBP_LOSSLESS     65569 /* WebP lossless/lossy */
#endif

#endif // GTIFF_H_INCLUDED
