/******************************************************************************
 * $Id$
 *
 * Name:     gdal.h
 * Project:  GDAL Core
 * Purpose:  GDAL Core C/Public declarations.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * Revision 1.25  2000/06/26 15:26:21  warmerda
 * added GDALGetDescription
 *
 * Revision 1.24  2000/06/05 17:24:05  warmerda
 * added real complex support
 *
 * Revision 1.23  2000/04/30 23:22:16  warmerda
 * added CreateCopy support
 *
 * Revision 1.22  2000/04/26 18:25:29  warmerda
 * added missing CPL_DLL attributes
 *
 * Revision 1.21  2000/04/21 21:54:37  warmerda
 * updated metadata API
 *
 * Revision 1.20  2000/03/31 13:41:25  warmerda
 * added gcps
 *
 * Revision 1.19  2000/03/24 00:09:05  warmerda
 * rewrote cache management
 *
 * Revision 1.18  2000/03/09 23:22:03  warmerda
 * added GetHistogram
 *
 * Revision 1.17  2000/03/08 19:59:16  warmerda
 * added GDALFlushRasterCache
 *
 * Revision 1.16  2000/03/06 21:50:37  warmerda
 * added min/max support
 *
 * Revision 1.15  2000/03/06 02:19:56  warmerda
 * added lots of new functions
 *
 * Revision 1.14  2000/01/31 14:24:36  warmerda
 * implemented dataset delete
 *
 * Revision 1.13  1999/11/11 21:59:06  warmerda
 * added GetDriver() for datasets
 *
 * Revision 1.12  1999/10/21 13:23:28  warmerda
 * Added C callable driver related functions.
 *
 * Revision 1.11  1999/10/01 14:44:02  warmerda
 * added documentation
 *
 * Revision 1.10  1999/07/23 19:35:22  warmerda
 * added GDALSwapWords(), GDALCopyWords()
 *
 * Revision 1.9  1999/05/23 02:46:26  warmerda
 * Added documentation short description.
 *
 * Revision 1.8  1999/04/21 04:16:13  warmerda
 * experimental docs
 *
 * Revision 1.7  1999/03/02 21:09:48  warmerda
 * add GDALDecToDMS()
 *
 * Revision 1.6  1999/01/11 15:36:17  warmerda
 * Added projections support, and a few other things.
 *
 * Revision 1.5  1998/12/31 18:53:33  warmerda
 * Add GDALGetDriverByName
 *
 * Revision 1.4  1998/12/06 22:16:27  warmerda
 * Added GDALCreate().
 *
 * Revision 1.3  1998/12/06 02:50:36  warmerda
 * Added three new functions.
 *
 * Revision 1.2  1998/12/03 18:34:05  warmerda
 * Update to use CPL
 *
 * Revision 1.1  1998/10/18 06:15:10  warmerda
 * Initial implementation.
 *
 */

#ifndef GDAL_H_INCLUDED
#define GDAL_H_INCLUDED

/**
 * \file gdal.h
 *
 * Public (C callable) GDAL entry points.
 */

#include "cpl_port.h"
#include "cpl_error.h"

/* -------------------------------------------------------------------- */
/*      Significant constants.                                          */
/* -------------------------------------------------------------------- */

CPL_C_START

/*! Pixel data types */
typedef enum {
    GDT_Unknown = 0,
    /*! Eight bit unsigned integer */ 		GDT_Byte = 1,
    /*! Sixteen bit unsigned integer */         GDT_UInt16 = 2,
    /*! Sixteen bit signed integer */           GDT_Int16 = 3,
    /*! Thirty two bit unsigned integer */      GDT_UInt32 = 4,
    /*! Thirty two bit signed integer */        GDT_Int32 = 5,
    /*! Thirty two bit floating point */        GDT_Float32 = 6,
    /*! Sixty four bit floating point */        GDT_Float64 = 7,
    /*! Complex Int16 */                        GDT_CInt16 = 8,
    /*! Complex Int32 */                        GDT_CInt32 = 9,
    /*! Complex Float32 */                      GDT_CFloat32 = 10,
    /*! Complex Float64 */                      GDT_CFloat64 = 11,
    GDT_TypeCount = 12		/* maximum type # + 1 */
} GDALDataType;

int CPL_DLL GDALGetDataTypeSize( GDALDataType );
int CPL_DLL GDALDataTypeIsComplex( GDALDataType );
const char CPL_DLL *GDALGetDataTypeName( GDALDataType );

/*! Flag indicating read/write, or read-only access to data. */
typedef enum {
    /*! Read only (no update) access */ GA_ReadOnly = 0,
    /*! Read/write access. */           GA_Update = 1
} GDALAccess;

typedef enum {
    GF_Read = 0,
    GF_Write = 1
} GDALRWFlag;

/*! Types of color interpretation for raster bands. */
typedef enum
{
    GCI_Undefined=0,
    GCI_GrayIndex=1,
    GCI_PaletteIndex=2,
    GCI_RedBand=3,
    GCI_GreenBand=4,
    GCI_BlueBand=5,
    GCI_AlphaBand=6,
    GCI_HueBand=7,
    GCI_SaturationBand=8,
    GCI_LightnessBand=9,
    GCI_CyanBand=10,
    GCI_MagentaBand=11,
    GCI_YellowBand=12,
    GCI_BlackBand=13
} GDALColorInterp;

/*! Translate a GDALColorInterp into a user displayable string. */
const char CPL_DLL *GDALGetColorInterpretationName( GDALColorInterp );

/*! Types of color interpretations for a GDALColorTable. */
typedef enum 
{
    GPI_Gray=0,
    GPI_RGB=1,
    GPI_CMYK=2,
    GPI_HLS=3
} GDALPaletteInterp;

/*! Translate a GDALPaletteInterp into a user displayable string. */
const char CPL_DLL *GDALGetPaletteInterpretationName( GDALPaletteInterp );

/* -------------------------------------------------------------------- */
/*      GDAL Specific error codes.                                      */
/*                                                                      */
/*      error codes 100 to 299 reserved for GDAL.                       */
/* -------------------------------------------------------------------- */
#define CPLE_WrongFormat	200

/* -------------------------------------------------------------------- */
/*      Define handle types related to various internal classes.        */
/* -------------------------------------------------------------------- */

typedef void *GDALMajorObjectH;
typedef void *GDALDatasetH;
typedef void *GDALRasterBandH;
typedef void *GDALDriverH;
typedef void *GDALProjDefH;
typedef void *GDALColorTableH;

/* -------------------------------------------------------------------- */
/*      Callback "progress" function.                                   */
/* -------------------------------------------------------------------- */
typedef int (*GDALProgressFunc)(double,const char *, void *);
int CPL_DLL GDALDummyProgress( double, const char *, void *);

/* ==================================================================== */
/*      Registration/driver related.                                    */
/* ==================================================================== */

void CPL_DLL GDALAllRegister( void );

GDALDatasetH CPL_DLL GDALCreate( GDALDriverH hDriver,
                                 const char *, int, int, int, GDALDataType,
                                 char ** );
GDALDatasetH CPL_DLL GDALCreateCopy( GDALDriverH, const char *, GDALDatasetH,
                                     int, char **, GDALProgressFunc, void * );

GDALDatasetH CPL_DLL GDALOpen( const char *, GDALAccess );

GDALDriverH CPL_DLL GDALGetDriverByName( const char * );
int CPL_DLL         GDALGetDriverCount();
GDALDriverH CPL_DLL GDALGetDriver( int );
int         CPL_DLL GDALRegisterDriver( GDALDriverH );
void        CPL_DLL GDALDeregisterDriver( GDALDriverH );
CPLErr	    CPL_DLL GDALDeleteDataset( GDALDriverH, const char * );

const char CPL_DLL *GDALGetDriverShortName( GDALDriverH );
const char CPL_DLL *GDALGetDriverLongName( GDALDriverH );
const char CPL_DLL *GDALGetDriverHelpTopic( GDALDriverH );

/* ==================================================================== */
/*      GDAL_GCP                                                        */
/* ==================================================================== */

/** Ground Control Point */
typedef struct
{
    /** Unique identifier, often numeric */
    char	*pszId; 

    /** Informational message or "" */
    char	*pszInfo;

    /** Pixel (x) location of GCP on raster */
    double 	dfGCPPixel;
    /** Line (y) location of GCP on raster */
    double	dfGCPLine;

    /** X position of GCP in georeferenced space */
    double	dfGCPX;

    /** Y position of GCP in georeferenced space */
    double	dfGCPY;

    /** Elevation of GCP, or zero if not known */
    double	dfGCPZ;
} GDAL_GCP;

void CPL_DLL GDALInitGCPs( int, GDAL_GCP * );
void CPL_DLL GDALDeinitGCPs( int, GDAL_GCP * );
GDAL_GCP CPL_DLL *GDALDuplicateGCPs( int, const GDAL_GCP * );

/* ==================================================================== */
/*      major objects (dataset, and, driver, drivermanager).            */
/* ==================================================================== */

char CPL_DLL  **GDALGetMetadata( GDALMajorObjectH, const char * );
CPLErr CPL_DLL  GDALSetMetadata( GDALMajorObjectH, char **,
                                 const char * );
char CPL_DLL  **GDALGetMetadataItem( GDALMajorObjectH, const char * );
CPLErr CPL_DLL  GDALSetMetadataItem( GDALMajorObjectH,
                                     const char *, const char *,
                                     const char * );
const char CPL_DLL *GDALGetDescription( GDALMajorObjectH );

/* ==================================================================== */
/*      GDALDataset class ... normally this represents one file.        */
/* ==================================================================== */

GDALDriverH CPL_DLL GDALGetDatasetDriver( GDALDatasetH );
void CPL_DLL   GDALClose( GDALDatasetH );
int CPL_DLL	GDALGetRasterXSize( GDALDatasetH );
int CPL_DLL	GDALGetRasterYSize( GDALDatasetH );
int CPL_DLL	GDALGetRasterCount( GDALDatasetH );
GDALRasterBandH CPL_DLL GDALGetRasterBand( GDALDatasetH, int );

const char CPL_DLL *GDALGetProjectionRef( GDALDatasetH );
CPLErr CPL_DLL  GDALSetProjection( GDALDatasetH, const char * );
CPLErr CPL_DLL  GDALGetGeoTransform( GDALDatasetH, double * );
CPLErr CPL_DLL  GDALSetGeoTransform( GDALDatasetH, double * );

int CPL_DLL     GDALGetGCPCount( GDALDatasetH );
const char CPL_DLL *GDALGetGCPProjection( GDALDatasetH );
const GDAL_GCP CPL_DLL *GDALGetGCPs( GDALDatasetH );

void CPL_DLL   *GDALGetInternalHandle( GDALDatasetH, const char * );
int CPL_DLL     GDALReferenceDataset( GDALDatasetH );
int CPL_DLL     GDALDereferenceDataset( GDALDatasetH );

/* ==================================================================== */
/*      GDALRasterBand ... one band/channel in a dataset.               */
/* ==================================================================== */

GDALDataType CPL_DLL GDALGetRasterDataType( GDALRasterBandH );
void CPL_DLL	GDALGetBlockSize( GDALRasterBandH,
                                  int * pnXSize, int * pnYSize );

CPLErr CPL_DLL GDALRasterIO( GDALRasterBandH hRBand, GDALRWFlag eRWFlag,
                              int nDSXOff, int nDSYOff,
                              int nDSXSize, int nDSYSize,
                              void * pBuffer, int nBXSize, int nBYSize,
                              GDALDataType eBDataType,
                              int nPixelSpace, int nLineSpace );
CPLErr CPL_DLL GDALReadBlock( GDALRasterBandH, int, int, void * );
CPLErr CPL_DLL GDALWriteBlock( GDALRasterBandH, int, int, void * );
int CPL_DLL GDALGetRasterBandXSize( GDALRasterBandH );
int CPL_DLL GDALGetRasterBandYSize( GDALRasterBandH );
char CPL_DLL  **GDALGetRasterMetadata( GDALRasterBandH );

GDALColorInterp CPL_DLL GDALGetRasterColorInterpretation( GDALRasterBandH );
GDALColorTableH CPL_DLL GDALGetRasterColorTable( GDALRasterBandH );
int CPL_DLL             GDALGetOverviewCount( GDALRasterBandH );
GDALRasterBandH CPL_DLL GDALGetOverview( GDALRasterBandH, int );
double CPL_DLL GDALGetRasterNoDataValue( GDALRasterBandH, int *pbSuccess );
double CPL_DLL GDALGetRasterMinimum( GDALRasterBandH, int *pbSuccess );
double CPL_DLL GDALGetRasterMaximum( GDALRasterBandH, int *pbSuccess );
void CPL_DLL GDALComputeRasterMinMax( GDALRasterBandH hBand, int bApproxOK,
                                      double adfMinMax[2] );
CPLErr CPL_DLL GDALFlushRasterCache( GDALRasterBandH hBand );
CPLErr CPL_DLL GDALGetRasterHistogram( GDALRasterBandH hBand,
                                       double dfMin, double dfMax,
                                       int nBuckets, int *panHistogram,
                                       int bIncludeOutOfRange, int bApproxOK,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData );
int CPL_DLL GDALGetRandomRasterSample( GDALRasterBandH, int, float * );

/* need to add functions related to block cache */

/* helper functions */
void CPL_DLL GDALSwapWords( void *pData, int nWordSize, int nWordCount,
                            int nWordSkip );
void CPL_DLL
    GDALCopyWords( void * pSrcData, GDALDataType eSrcType, int nSrcPixelOffset,
                   void * pDstData, GDALDataType eDstType, int nDstPixelOffset,
                   int nWordCount );


/* ==================================================================== */
/*      Color tables.                                                   */
/* ==================================================================== */
/** Color tuple */
typedef struct
{
    /** gray, red, cyan or hue */
    short      c1;      

    /* green, magenta, or lightness */    
    short      c2;      

    /* blue, yellow, or saturation */
    short      c3;      

    /* alpha or blackband */
    short      c4;      
} GDALColorEntry;

GDALColorTableH CPL_DLL GDALCreateColorTable( GDALPaletteInterp );
void CPL_DLL            GDALDestroyColorTable( GDALColorTableH );
GDALColorTableH CPL_DLL GDALCloneColorTable( GDALColorTableH );
GDALPaletteInterp CPL_DLL GDALGetPaletteInterpretation( GDALColorTableH );
int CPL_DLL             GDALGetColorEntryCount( GDALColorTableH );
const GDALColorEntry CPL_DLL *GDALGetColorEntry( GDALColorTableH, int );
int CPL_DLL GDALGetColorEntryAsRGB( GDALColorTableH, int, GDALColorEntry *);
void CPL_DLL GDALSetColorEntry( GDALColorTableH, int, const GDALColorEntry * );

/* ==================================================================== */
/*      Projections                                                     */
/* ==================================================================== */

GDALProjDefH CPL_DLL GDALCreateProjDef( const char * );
CPLErr 	CPL_DLL GDALReprojectToLongLat( GDALProjDefH, double *, double * );
CPLErr 	CPL_DLL GDALReprojectFromLongLat( GDALProjDefH, double *, double * );
void    CPL_DLL GDALDestroyProjDef( GDALProjDefH );
const char CPL_DLL *GDALDecToDMS( double, const char *, int );

/* ==================================================================== */
/*      GDAL Cache Management                                           */
/* ==================================================================== */

void CPL_DLL GDALSetCacheMax( int nBytes );
int CPL_DLL GDALGetCacheMax();
int CPL_DLL GDALGetCacheUsed();
int CPL_DLL GDALFlushCacheBlock();

CPL_C_END

#endif /* ndef GDAL_H_INCLUDED */
