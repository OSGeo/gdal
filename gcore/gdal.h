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
 * Revision 1.1  1998/10/18 06:15:10  warmerda
 * Initial implementation.
 *
 */

#ifndef GDAL_H_INCLUDED
#define GDAL_H_INCLUDED

#include "gdal_base.h"

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

typedef enum {
    GA_ReadOnly = 0,
    GA_Update = 1
} GDALAccess;

typedef enum {
    GF_Read = 0,
    GF_Write = 1
} GDALRWFlag;



/* -------------------------------------------------------------------- */
/*      Provide a C, or C++ style declaration for major classes if      */
/*      they haven't already been declared by gdal_priv.h.              */
/* -------------------------------------------------------------------- */

#if !defined(GDAL_PRIV_H_INCLUDED)

#  if defined(__cplusplus)

class GDALMajorObject {};
class GDALRasterBand : public GDALMajorObject {};
class GDALDataset : public GDALMajorObject {};
class GDALGeoref;

#  else

typedef void *GDALMajorObject;
typedef void *GDALDataset;
typedef void *GDALRasterBand;
typedef void *GDALGeoref;

#  endif

#endif /* !defined(GDAL_PRIV_H_INCLUDED)

/* ==================================================================== */
/*      GDALMajorObject                                                 */
/* ==================================================================== */

GDAL_C_START

const char GDAL_DLL GDALGetDescription( void * );
void GDAL_DLL   GDALSetDescription( void *, const char * );

/* ==================================================================== */
/*      GDALDataset class ... normally this represents one file.        */
/* ==================================================================== */

GDALDataset GDAL_DLL *GDALOpen( const char *, GDALAccess );
void GDAL_DLL   GDALClose( GDALDataset * );

int GDAL_DLL	GDALGetRasterXSize( GDALDataset * );
int GDAL_DLL	GDALGetRasterYSize( GDALDataset * );
int GDAL_DLL	GDALGetRasterCount( GDALDataset * );
GDALGeoref GDAL_DLL *GDALGetRasterGeoref( GDALDataset * );
GDALRasterBand GDAL_DLL *GDALGetRasterBand( GDALDataset *, int );

/* ==================================================================== */
/*      GDALRasterBand ... one band/channel in a dataset.               */
/* ==================================================================== */

GDALDataType GDAL_DLL *GDALGetRasterDataType( GDALRasterBand * );
void GDAL_DLL	GDALGetBlockSize( GDALRasterBand *,
                                   int * pnXSize, int * pnYSize );

GDALErr GDAL_DLL GDALRasterIO( GDALRasterBand *psRBand, GDALRWFlag eRWFlag,
                               int nDSXOff, int nDSYOff,
                               int nDSXSize, int nDSYSize,
                               void * pBuffer, int nBXSize, nBYSize,
                               GDALDataType eBDataType,
                               int nPixelSpace, int nLineSpace );

GDAL_C_END

#endif /* ndef GDAL_H_INCLUDED */
