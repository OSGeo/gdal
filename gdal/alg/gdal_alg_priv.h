/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Image Processing Algorithms
 * Purpose:  Prototypes and definitions for various GDAL based algorithms:
 *           private declarations.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2008, Andrey Kiselev <dron@ak4719.spb.edu>
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

#ifndef GDAL_ALG_PRIV_H_INCLUDED
#define GDAL_ALG_PRIV_H_INCLUDED

#include "gdal_alg.h"

CPL_C_START

/** Source of the burn value */
typedef enum {
    /*! Use value from padfBurnValue */    GBV_UserBurnValue = 0,
    /*! Use value from the Z coordinate */    GBV_Z = 1,
    /*! Use value form the M value */    GBV_M = 2
} GDALBurnValueSrc;

typedef struct {
    unsigned char * pabyChunkBuf;
    int nXSize;
    int nYSize;
    int nBands;
    GDALDataType eType;
    double *padfBurnValue;
    GDALBurnValueSrc eBurnValueSource;
} GDALRasterizeInfo;

/************************************************************************/
/*      Low level rasterizer API.                                       */
/************************************************************************/

typedef void (*llScanlineFunc)( void *, int, int, int, double );
typedef void (*llPointFunc)( void *, int, int, double );

void GDALdllImagePoint( int nRasterXSize, int nRasterYSize,
                        int nPartCount, int *panPartSize,
                        double *padfX, double *padfY, double *padfVariant,
                        llPointFunc pfnPointFunc, void *pCBData );

void GDALdllImageLine( int nRasterXSize, int nRasterYSize, 
                       int nPartCount, int *panPartSize,
                       double *padfX, double *padfY, double *padfVariant,
                       llPointFunc pfnPointFunc, void *pCBData );

void GDALdllImageLineAllTouched(int nRasterXSize, int nRasterYSize, 
                                int nPartCount, int *panPartSize,
                                double *padfX, double *padfY,
                                double *padfVariant,
                                llPointFunc pfnPointFunc, void *pCBData );

void GDALdllImageFilledPolygon(int nRasterXSize, int nRasterYSize, 
                               int nPartCount, int *panPartSize,
                               double *padfX, double *padfY,
                               double *padfVariant,
                               llScanlineFunc pfnScanlineFunc, void *pCBData );

CPL_C_END

/************************************************************************/
/*                          Polygon Enumerator                          */
/************************************************************************/

class GDALRasterPolygonEnumerator

{
private:
    void     MergePolygon( int nSrcId, int nDstId );
    int      NewPolygon( GInt32 nValue );

public:  // these are intended to be readonly.

    GInt32   *panPolyIdMap;
    GInt32   *panPolyValue;

    int      nNextPolygonId;
    int      nPolyAlloc;

    int      nConnectedness;

public:
             GDALRasterPolygonEnumerator( int nConnectedness=4 );
            ~GDALRasterPolygonEnumerator();

    void     ProcessLine( GInt32 *panLastLineVal, GInt32 *panThisLineVal,
                          GInt32 *panLastLineId,  GInt32 *panThisLineId, 
                          int nXSize );

    void     CompleteMerges();

    void     Clear();
};

#endif /* ndef GDAL_ALG_PRIV_H_INCLUDED */
