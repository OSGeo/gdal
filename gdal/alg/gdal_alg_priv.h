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

#ifndef GDAL_ALG_PRIV_H_INCLUDED
#define GDAL_ALG_PRIV_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "gdal_alg.h"

CPL_C_START

/** Source of the burn value */
typedef enum {
    /*! Use value from padfBurnValue */    GBV_UserBurnValue = 0,
    /*! Use value from the Z coordinate */    GBV_Z = 1,
    /*! Use value form the M value */    GBV_M = 2
} GDALBurnValueSrc;

typedef enum {
    GRMA_Replace = 0,
    GRMA_Add = 1,
} GDALRasterMergeAlg;

typedef struct {
    unsigned char * pabyChunkBuf;
    int nXSize;
    int nYSize;
    int nBands;
    GDALDataType eType;
    double *padfBurnValue;
    GDALBurnValueSrc eBurnValueSource;
    GDALRasterMergeAlg eMergeAlg;
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

#define GP_NODATA_MARKER -51502112

template<class DataType, class EqualityTest> class GDALRasterPolygonEnumeratorT

{
private:
    void     MergePolygon( int nSrcId, int nDstId );
    int      NewPolygon( DataType nValue );

public:  // these are intended to be readonly.

    GInt32   *panPolyIdMap;
    DataType   *panPolyValue;

    int      nNextPolygonId;
    int      nPolyAlloc;

    int      nConnectedness;

public:
             GDALRasterPolygonEnumeratorT( int nConnectedness=4 );
            ~GDALRasterPolygonEnumeratorT();

    void     ProcessLine( DataType *panLastLineVal, DataType *panThisLineVal,
                          GInt32 *panLastLineId,  GInt32 *panThisLineId,
                          int nXSize );

    void     CompleteMerges();

    void     Clear();
};

struct IntEqualityTest
{
    bool operator()(GInt32 a, GInt32 b) { return a == b; }
};

typedef GDALRasterPolygonEnumeratorT<GInt32, IntEqualityTest> GDALRasterPolygonEnumerator;

typedef void* (*GDALTransformDeserializeFunc)( CPLXMLNode *psTree );

void* GDALRegisterTransformDeserializer(const char* pszTransformName,
                                       GDALTransformerFunc pfnTransformerFunc,
                                       GDALTransformDeserializeFunc pfnDeserializeFunc);
void GDALUnregisterTransformDeserializer(void* pData);

void GDALCleanupTransformDeserializerMutex();

/* Transformer cloning */

void* GDALCreateTPSTransformerInt( int nGCPCount, const GDAL_GCP *pasGCPList,
                                   int bReversed, char** papszOptions );

void CPL_DLL * GDALCloneTransformer( void *pTransformerArg );

/************************************************************************/
/*      Color table related                                             */
/************************************************************************/

/* definitions exists for T = GUInt32 and T = GUIntBig */
template<class T> int
GDALComputeMedianCutPCTInternal( GDALRasterBandH hRed,
                           GDALRasterBandH hGreen,
                           GDALRasterBandH hBlue,
                           GByte* pabyRedBand,
                           GByte* pabyGreenBand,
                           GByte* pabyBlueBand,
                           int (*pfnIncludePixel)(int,int,void*),
                           int nColors,
                           int nBits,
                           T* panHistogram,
                           GDALColorTableH hColorTable,
                           GDALProgressFunc pfnProgress,
                           void * pProgressArg );

int GDALDitherRGB2PCTInternal( GDALRasterBandH hRed,
                               GDALRasterBandH hGreen,
                               GDALRasterBandH hBlue,
                               GDALRasterBandH hTarget,
                               GDALColorTableH hColorTable,
                               int nBits,
                               GInt16* pasDynamicColorMap,
                               int bDither,
                               GDALProgressFunc pfnProgress,
                               void * pProgressArg );

#define PRIME_FOR_65536                                 98317

/* See HashHistogram structure in gdalmediancut.cpp and ColorIndex structure in gdaldither.cpp */
/* 6 * sizeof(int) should be the size of the largest of both structures */
#define MEDIAN_CUT_AND_DITHER_BUFFER_SIZE_65536         (6 * sizeof(int) * PRIME_FOR_65536)

/************************************************************************/
/*      Float comparison function.                                      */
/************************************************************************/

/**
 * Units in the Last Place. This specifies how big an error we are willing to
 * accept in terms of the value of the least significant digit of the floating
 * point number’s representation. MAX_ULPS can also be interpreted in terms of
 * how many representable floats we are willing to accept between A and B.
 */
#define MAX_ULPS 10

GBool GDALFloatEquals(float A, float B);

struct FloatEqualityTest
{
    bool operator()(float a, float b) { return GDALFloatEquals(a,b) == TRUE; }
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* ndef GDAL_ALG_PRIV_H_INCLUDED */
