/******************************************************************************
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
 * gdal.h
 *
 * This is the primary public C/C++ include file for application code
 * that calls the GDAL library.
 * 
 * $Log$
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
    GDT_TypeCount = 8		/* maximum type # + 1 */
} GDALDataType;

int CPL_DLL GDALGetDataTypeSize( GDALDataType );
const char *CPL_DLL GDALGetDataTypeName( GDALDataType );

/*! Flag indicating read/write, or read-only access to data. */
typedef enum {
    /*! Read only (no update) access */ GA_ReadOnly = 0,
    /*! Read/write access. */           GA_Update = 1
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
#define CPLE_WrongFormat	200

/* -------------------------------------------------------------------- */
/*      Define handle types related to various internal classes.        */
/* -------------------------------------------------------------------- */

typedef void *GDALMajorObjectH;
typedef void *GDALDatasetH;
typedef void *GDALRasterBandH;
typedef void *GDALGeorefH;
typedef void *GDALDriverH;
typedef void *GDALProjDefH;

/* ==================================================================== */
/*      Registration/driver related.                                    */
/* ==================================================================== */

void GDALAllRegister( void );

GDALDatasetH CPL_DLL GDALCreate( GDALDriverH hDriver,
                                 const char *, int, int, int, GDALDataType,
                                 char ** );
GDALDatasetH CPL_DLL GDALOpen( const char *, GDALAccess );

GDALDriverH CPL_DLL GDALGetDriverByName( const char * );

/* ==================================================================== */
/*      GDALDataset class ... normally this represents one file.        */
/* ==================================================================== */

void CPL_DLL   GDALClose( GDALDatasetH );
int CPL_DLL	GDALGetRasterXSize( GDALDatasetH );
int CPL_DLL	GDALGetRasterYSize( GDALDatasetH );
int CPL_DLL	GDALGetRasterCount( GDALDatasetH );
GDALGeorefH CPL_DLL GDALGetRasterGeoref( GDALDatasetH );
GDALRasterBandH CPL_DLL GDALGetRasterBand( GDALDatasetH, int );
const char CPL_DLL *GDALGetProjectionRef( GDALDatasetH );
CPLErr CPL_DLL  GDALSetProjection( GDALDatasetH, const char * );
CPLErr CPL_DLL  GDALGetGeoTransform( GDALDatasetH, double * );
CPLErr CPL_DLL  GDALSetGeoTransform( GDALDatasetH, double * );
void CPL_DLL   *GDALGetInternalHandle( GDALDatasetH, const char * );

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

/* need to add functions related to block cache */

/* helper functions */
void CPL_DLL GDALSwapWords( void *pData, int nWordSize, int nWordCount,
                            int nWordSkip );
void CPL_DLL
    GDALCopyWords( void * pSrcData, GDALDataType eSrcType, int nSrcPixelOffset,
                   void * pDstData, GDALDataType eDstType, int nDstPixelOffset,
                   int nWordCount );


/* ==================================================================== */
/*      Projections                                                     */
/* ==================================================================== */

GDALProjDefH CPL_DLL GDALCreateProjDef( const char * );
CPLErr 	CPL_DLL GDALReprojectToLongLat( GDALProjDefH, double *, double * );
CPLErr 	CPL_DLL GDALReprojectFromLongLat( GDALProjDefH, double *, double * );
void    CPL_DLL GDALDestroyProjDef( GDALProjDefH );
const char CPL_DLL *GDALDecToDMS( double, const char *, int );

CPL_C_END

#endif /* ndef GDAL_H_INCLUDED */
