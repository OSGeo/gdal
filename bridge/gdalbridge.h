/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Bridge 
 * Purpose:  Declarations for GDAL Bridge support.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 * This file needs to be kept up to date with the contents of gdal.h by hand.
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
 * Revision 1.4  1999/09/17 03:18:37  warmerda
 * added name indirection for function pointer names for libtool
 *
 * Revision 1.3  1999/04/22 13:36:43  warmerda
 * Added copyright header.
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

GDAL_ENTRY int	(*pfnGDALGetDataTypeSize)( GDALDataType ) GDAL_NULL;
#define GDALGetDataTypeSize pfnGDALGetDataTypeSize

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

GDAL_ENTRY void (*pfnGDALAllRegister)( void ) GDAL_NULL;
#define GDALAllRegister pfnGDALAllRegister

GDAL_ENTRY GDALDatasetH (*pfnGDALCreate)( GDALDriverH hDriver, const char *,
                                          int, int, int, GDALDataType,
                                          char ** ) GDAL_NULL;
#define GDALCreate pfnGDALCreate


GDAL_ENTRY GDALDatasetH (*pfnGDALOpen)( const char *, GDALAccess ) GDAL_NULL;
#define GDALOpen pfnGDALOpen

GDAL_ENTRY GDALDriverH (*pfnGDALGetDriverByName)( const char * ) GDAL_NULL;
#define GDALGetDriverByName pfnGDALGetDriverByName

/* ==================================================================== */
/*      GDALDataset class ... normally this represents one file.        */
/* ==================================================================== */

GDAL_ENTRY void (*pfnGDALClose)( GDALDatasetH ) GDAL_NULL;
#define GDALClose pfnGDALClose

GDAL_ENTRY int (*pfnGDALGetRasterXSize)( GDALDatasetH ) GDAL_NULL;
#define GDALGetRasterXSize pfnGDALGetRasterXSize

GDAL_ENTRY int (*pfnGDALGetRasterYSize)( GDALDatasetH ) GDAL_NULL;
#define GDALGetRasterYSize pfnGDALGetRasterYSize

GDAL_ENTRY int (*pfnGDALGetRasterCount)( GDALDatasetH ) GDAL_NULL;
#define GDALGetRasterCount pfnGDALGetRasterCount

GDAL_ENTRY GDALRasterBandH
               (*pfnGDALGetRasterBand)( GDALDatasetH, int) GDAL_NULL;
#define GDALGetRasterBand pfnGDALGetRasterBand

GDAL_ENTRY const char *(*pfnGDALGetProjectionRef)( GDALDatasetH ) GDAL_NULL;
#define GDALGetProjectionRef pfnGDALGetProjectionRef

GDAL_ENTRY CPLErr (*pfnGDALSetProjection)( GDALDatasetH,
                                           const char * ) GDAL_NULL;
#define GDALSetProjection pfnGDALSetProjection

GDAL_ENTRY CPLErr (*pfnGDALGetGeoTransform)( GDALDatasetH, double* ) GDAL_NULL;
#define GDALGetGeoTransform pfnGDALGetGeoTransform

GDAL_ENTRY CPLErr (*pfnGDALSetGeoTransform)( GDALDatasetH, double* ) GDAL_NULL;
#define GDALSetGeoTransform pfnGDALSetGeoTransform

GDAL_ENTRY void *(*pfnGDALGetInternalHandle)( GDALDatasetH,
                                              const char * ) GDAL_NULL;
#define GDALGetInternalHandle pfnGDALGetInternalHandle

/* ==================================================================== */
/*      GDALRasterBand ... one band/channel in a dataset.               */
/* ==================================================================== */

GDAL_ENTRY GDALDataType (*pGDALGetRasterDataType)( GDALRasterBandH ) GDAL_NULL;
#define GDALGetRasterDataType pGDALGetRasterDataType

GDAL_ENTRY void (*pGDALGetBlockSize)( GDALRasterBandH,
                                      int * pnXSize, int * pnYSize ) GDAL_NULL;
#define GDALGetBlockSize pGDALGetBlockSize

GDAL_ENTRY CPLErr (*pGDALRasterIO)( GDALRasterBandH hRBand, GDALRWFlag eRWFlag,
                                    int nDSXOff, int nDSYOff,
                                    int nDSXSize, int nDSYSize,
                                    void * pBuffer, int nBXSize, int nBYSize,
                                    GDALDataType eBDataType,
                                   int nPixelSpace, int nLineSpace ) GDAL_NULL;
#define GDALRasterIO pGDALRasterIO

GDAL_ENTRY CPLErr (*pGDALReadBlock)( GDALRasterBandH,
                                     int, int, void * ) GDAL_NULL;
#define GDALReadBlock pGDALReadBlock

GDAL_ENTRY CPLErr (*pGDALWriteBlock)( GDALRasterBandH,
                                      int, int, void * ) GDAL_NULL;
#define GDALWriteBlock pGDALWriteBlock

/* need to add functions related to block cache */

/* ==================================================================== */
/*      Projections                                                     */
/* ==================================================================== */

GDAL_ENTRY GDALProjDefH (*pGDALCreateProjDef)( const char * ) GDAL_NULL;
#define GDALCreateProjDef pGDALCreateProjDef

GDAL_ENTRY CPLErr (*pGDALReprojectToLongLat)( GDALProjDefH,
                                              double *, double * ) GDAL_NULL;
#define GDALReprojectToLongLat pGDALReprojectToLongLat

GDAL_ENTRY CPLErr (*pGDALReprojectFromLongLat)( GDALProjDefH,
                                                double *, double * ) GDAL_NULL;
#define GDALReprojectFromLongLat pGDALReprojectFromLongLat

GDAL_ENTRY void (*pGDALDestroyProjDef)( GDALProjDefH ) GDAL_NULL;
#define GDALDestroyProjDef pGDALDestroyProjDef

GDAL_ENTRY const char *(*pGDALDecToDMS)( double, const char *, int ) GDAL_NULL;
#define GDALDecToDMS pGDALDecToDMS

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
