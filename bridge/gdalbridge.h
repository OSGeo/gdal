/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Bridge
 * Purpose:  Defines structures and functions for use of GDAL Bridge.  GDAL
 *           Bridge is a lightweight approach to using the GDAL C API via
 *           demand loading from gdal*.so at runtime.  This include file, and
 *           gdalbridge.cpp would normally be copied into other packages and
 *           used to avoid direct dependence on the rest of GDAL. 
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/04/21 23:01:31  warmerda
 * New
 *
 */

#ifndef GDALBRIDGE_H_INCLUDED
#define GDALBRIDGE_H_INCLUDED

/* -------------------------------------------------------------------- */
/*      Start C context.                                                */
/* -------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif
    
/* ==================================================================== */
/*      Standard types and defines normally supplied by cpl_port.h.     */
/* ==================================================================== */
#if UINT_MAX == 65535
typedef long            GInt32;
typedef unsigned long   GUInt32;
#else
typedef int             GInt32;
typedef unsigned int    GUInt32;
#endif

typedef short           GInt16;
typedef unsigned short  GUInt16;
typedef unsigned char   GByte;
typedef int             GBool;

#ifndef FALSE
#  define FALSE		0
#  define TRUE		1
#endif

#ifndef NULL
#  define NULL		0
#endif

#ifndef GDAL_ENTRY
#  define GDAL_ENTRY extern
#  define GDAL_NULL
#endif

/* -------------------------------------------------------------------- */
/*      Significant constants.                                          */
/* -------------------------------------------------------------------- */

typedef enum {
    GDT_Unknown = 0,
    GDT_Byte = 1,
    GDT_UInt16 = 2,
    GDT_Int16 = 3,
    GDT_UInt32 = 4,
    GDT_Int32 = 5,
    GDT_Float32 = 6,
    GDT_Float64 = 7
} GDALDataType;

GDAL_ENTRY int	(*GDALGetDataTypeSize)( GDALDataType ) GDAL_NULL;

typedef enum {
    GA_ReadOnly = 0,
    GA_Update = 1
} GDALAccess;

typedef enum {
    GF_Read = 0,
    GF_Write = 1
} GDALRWFlag;

/* -------------------------------------------------------------------- */
/*      GDAL Specific error codes.                                      */
/*                                                                      */
/*      error codes 100 to 299 reserved for GDAL.                       */
/* -------------------------------------------------------------------- */
typedef enum
{
    CE_None = 0,
    CE_Log = 1,
    CE_Warning = 2,
    CE_Failure = 3,
    CE_Fatal = 4
  
} CPLErr;

#define CPLE_AppDefined			1
#define CPLE_OutOfMemory		2
#define CPLE_FileIO			3
#define CPLE_OpenFailed			4
#define CPLE_IllegalArg			5
#define CPLE_NotSupported		6
#define CPLE_AssertionFailed		7
#define CPLE_NoWriteAccess		8

#define CPLE_WrongFormat	200

/* -------------------------------------------------------------------- */
/*      Define handle types related to various internal classes.        */
/* -------------------------------------------------------------------- */

typedef void *GDALMajorObjectH;
typedef void *GDALDatasetH;
typedef void *GDALRasterBandH;
typedef void *GDALDriverH;
typedef void *GDALProjDefH;

/* ==================================================================== */
/*      Registration/driver related.                                    */
/* ==================================================================== */

GDAL_ENTRY void (*GDALAllRegister)( void ) GDAL_NULL;

GDAL_ENTRY GDALDatasetH *(*GDALCreate)( GDALDriverH hDriver, const char *,
                                        int, int, int, GDALDataType,
                                        char ** ) GDAL_NULL;

GDAL_ENTRY GDALDatasetH *(*GDALOpen)( const char *, GDALAccess ) GDAL_NULL;

GDAL_ENTRY GDALDriverH *(*GDALGetDriverByName)( const char * ) GDAL_NULL;

/* ==================================================================== */
/*      GDALDataset class ... normally this represents one file.        */
/* ==================================================================== */

GDAL_ENTRY void (*GDALClose)( GDALDatasetH ) GDAL_NULL;

GDAL_ENTRY int (*GDALGetRasterXSize)( GDALDatasetH ) GDAL_NULL;
GDAL_ENTRY int (*GDALGetRasterYSize)( GDALDatasetH ) GDAL_NULL;
GDAL_ENTRY int (*GDALGetRasterCount)( GDALDatasetH ) GDAL_NULL;
GDAL_ENTRY GDALRasterBandH *(*GDALGetRasterBand)( GDALDatasetH, int) GDAL_NULL;
GDAL_ENTRY const char *(*GDALGetProjectionRef)( GDALDatasetH ) GDAL_NULL;
GDAL_ENTRY CPLErr (*GDALSetProjection)( GDALDatasetH, const char * ) GDAL_NULL;
GDAL_ENTRY CPLErr (*GDALGetGeoTransform)( GDALDatasetH, double * ) GDAL_NULL;
GDAL_ENTRY CPLErr (*GDALSetGeoTransform)( GDALDatasetH, double * ) GDAL_NULL;
GDAL_ENTRY void *(*GDALGetInternalHandle)( GDALDatasetH,
                                           const char * ) GDAL_NULL;

/* ==================================================================== */
/*      GDALRasterBand ... one band/channel in a dataset.               */
/* ==================================================================== */

GDAL_ENTRY GDALDataType (*GDALGetRasterDataType)( GDALRasterBandH ) GDAL_NULL;
GDAL_ENTRY void (*GDALGetBlockSize)( GDALRasterBandH,
                                     int * pnXSize, int * pnYSize ) GDAL_NULL;

GDAL_ENTRY CPLErr (*GDALRasterIO)( GDALRasterBandH hRBand, GDALRWFlag eRWFlag,
                                   int nDSXOff, int nDSYOff,
                                   int nDSXSize, int nDSYSize,
                                   void * pBuffer, int nBXSize, int nBYSize,
                                   GDALDataType eBDataType,
                                   int nPixelSpace, int nLineSpace ) GDAL_NULL;

GDAL_ENTRY CPLErr (*GDALReadBlock)( GDALRasterBandH,
                                    int, int, void * ) GDAL_NULL;
GDAL_ENTRY CPLErr (*GDALWriteBlock)( GDALRasterBandH,
                                     int, int, void * ) GDAL_NULL;

/* need to add functions related to block cache */

/* ==================================================================== */
/*      Projections                                                     */
/* ==================================================================== */

GDAL_ENTRY GDALProjDefH *(*GDALCreateProjDef)( const char * ) GDAL_NULL;
GDAL_ENTRY CPLErr (*GDALReprojectToLongLat)( GDALProjDefH,
                                             double *, double * ) GDAL_NULL;
GDAL_ENTRY CPLErr (*GDALReprojectFromLongLat)( GDALProjDefH,
                                               double *, double * ) GDAL_NULL;
GDAL_ENTRY void (*GDALDestroyProjDef)( GDALProjDefH ) GDAL_NULL;
GDAL_ENTRY const char *(*GDALDecToDMS)( double, const char *, int ) GDAL_NULL;

/* -------------------------------------------------------------------- */
/*      This is the real entry point.  It tries to load the shared      */
/*      libraries (given a hint of a directory it might be in).  It     */
/*      returns TRUE if it succeeds, or FALSE otherwise.                */
/* -------------------------------------------------------------------- */
int	GDALBridgeInitialize( const char * );
void	*GBGetSymbol( const char *, const char * );

/* -------------------------------------------------------------------- */
/*      Terminate C context.                                            */
/* -------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif

#endif /* ndef GDALBRIDGE_H_INCLUDED */
