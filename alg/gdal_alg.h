/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Image Processing Algorithms
 * Purpose:  Prototypes, and definitions for various GDAL based algorithms.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 ****************************************************************************/

#ifndef GDAL_ALG_H_INCLUDED
#define GDAL_ALG_H_INCLUDED

/**
 * \file gdal_alg.h
 *
 * Public (C callable) GDAL algorithm entry points, and definitions.
 */

#include "gdal.h"
#include "cpl_minixml.h"
#include "ogr_api.h"

CPL_C_START

int CPL_DLL CPL_STDCALL GDALComputeMedianCutPCT( GDALRasterBandH hRed, 
                             GDALRasterBandH hGreen, 
                             GDALRasterBandH hBlue, 
                             int (*pfnIncludePixel)(int,int,void*),
                             int nColors, 
                             GDALColorTableH hColorTable,
                             GDALProgressFunc pfnProgress, 
                             void * pProgressArg );

int CPL_DLL CPL_STDCALL GDALDitherRGB2PCT( GDALRasterBandH hRed, 
                       GDALRasterBandH hGreen, 
                       GDALRasterBandH hBlue, 
                       GDALRasterBandH hTarget, 
                       GDALColorTableH hColorTable, 
                       GDALProgressFunc pfnProgress, 
                       void * pProgressArg );

int CPL_DLL CPL_STDCALL GDALChecksumImage( GDALRasterBandH hBand, 
                               int nXOff, int nYOff, int nXSize, int nYSize );
                               

/*
 * Warp Related.
 */

typedef int 
(*GDALTransformerFunc)( void *pTransformerArg, 
                        int bDstToSrc, int nPointCount, 
                        double *x, double *y, double *z, int *panSuccess );

typedef struct {
    char szSignature[4];
    char *pszClassName;
    GDALTransformerFunc pfnTransform;
    void (*pfnCleanup)( void * );
    CPLXMLNode *(*pfnSerialize)( void * );
} GDALTransformerInfo;

void CPL_DLL GDALDestroyTransformer( void *pTransformerArg );


/* High level transformer for going from image coordinates on one file
   to image coordiantes on another, potentially doing reprojection, 
   utilizing GCPs or using the geotransform. */

void CPL_DLL *
GDALCreateGenImgProjTransformer( GDALDatasetH hSrcDS, const char *pszSrcWKT,
                                 GDALDatasetH hDstDS, const char *pszDstWKT,
                                 int bGCPUseOK, double dfGCPErrorThreshold,
                                 int nOrder );
void CPL_DLL GDALSetGenImgProjTransformerDstGeoTransform( void *, 
                                                          const double * );
void CPL_DLL GDALDestroyGenImgProjTransformer( void * );
int CPL_DLL GDALGenImgProjTransform( 
    void *pTransformArg, int bDstToSrc, int nPointCount,
    double *x, double *y, double *z, int *panSuccess );

/* Geo to geo reprojection transformer. */
void CPL_DLL *
GDALCreateReprojectionTransformer( const char *pszSrcWKT, 
                                   const char *pszDstWKT );
void CPL_DLL GDALDestroyReprojectionTransformer( void * );
int CPL_DLL GDALReprojectionTransform( 
    void *pTransformArg, int bDstToSrc, int nPointCount,
    double *x, double *y, double *z, int *panSuccess );

/* GCP based transformer ... forward is to georef coordinates */
void CPL_DLL *
GDALCreateGCPTransformer( int nGCPCount, const GDAL_GCP *pasGCPList, 
                          int nReqOrder, int bReversed );
void CPL_DLL GDALDestroyGCPTransformer( void *pTransformArg );
int CPL_DLL GDALGCPTransform( 
    void *pTransformArg, int bDstToSrc, int nPointCount,
    double *x, double *y, double *z, int *panSuccess );

/* Thin Plate Spine transformer ... forward is to georef coordinates */

void CPL_DLL *
GDALCreateTPSTransformer( int nGCPCount, const GDAL_GCP *pasGCPList, 
                          int bReversed );
void CPL_DLL GDALDestroyTPSTransformer( void *pTransformArg );
int CPL_DLL GDALTPSTransform( 
    void *pTransformArg, int bDstToSrc, int nPointCount,
    double *x, double *y, double *z, int *panSuccess );

/* RPC based transformer ... src is pixel/line/elev, dst is long/lat/elev */

void CPL_DLL *
GDALCreateRPCTransformer( GDALRPCInfo *psRPC, int bReversed, 
                          double dfPixErrThreshold );
void CPL_DLL GDALDestroyRPCTransformer( void *pTransformArg );
int CPL_DLL GDALRPCTransform( 
    void *pTransformArg, int bDstToSrc, int nPointCount,
    double *x, double *y, double *z, int *panSuccess );

/* Geolocation transformer */

void CPL_DLL *
GDALCreateGeoLocTransformer( GDALDatasetH hBaseDS, 
                             char **papszGeolocationInfo,
                             int bReversed );
void CPL_DLL GDALDestroyGeoLocTransformer( void *pTransformArg );
int CPL_DLL GDALGeoLocTransform( 
    void *pTransformArg, int bDstToSrc, int nPointCount,
    double *x, double *y, double *z, int *panSuccess );

/* Approximate transformer */
void CPL_DLL *
GDALCreateApproxTransformer( GDALTransformerFunc pfnRawTransformer, 
                             void *pRawTransformerArg, double dfMaxError );
void CPL_DLL GDALApproxTransformerOwnsSubtransformer( void *pCBData, 
                                                      int bOwnFlag );
void CPL_DLL GDALDestroyApproxTransformer( void *pApproxArg );
int  CPL_DLL GDALApproxTransform(
    void *pTransformArg, int bDstToSrc, int nPointCount,
    double *x, double *y, double *z, int *panSuccess );

                      


int CPL_DLL CPL_STDCALL
GDALSimpleImageWarp( GDALDatasetH hSrcDS, 
                     GDALDatasetH hDstDS, 
                     int nBandCount, int *panBandList,
                     GDALTransformerFunc pfnTransform,
                     void *pTransformArg,
                     GDALProgressFunc pfnProgress, 
                     void *pProgressArg, 
                     char **papszWarpOptions );

CPLErr CPL_DLL CPL_STDCALL
GDALSuggestedWarpOutput( GDALDatasetH hSrcDS, 
                         GDALTransformerFunc pfnTransformer,
                         void *pTransformArg,
                         double *padfGeoTransformOut, 
                         int *pnPixels, int *pnLines );
CPLErr CPL_DLL CPL_STDCALL
GDALSuggestedWarpOutput2( GDALDatasetH hSrcDS, 
                          GDALTransformerFunc pfnTransformer,
                          void *pTransformArg,
                          double *padfGeoTransformOut, 
                          int *pnPixels, int *pnLines,
                          double *padfExtents, 
                          int nOptions );

CPLXMLNode CPL_DLL *
GDALSerializeTransformer( GDALTransformerFunc pfnFunc, void *pTransformArg );
CPLErr CPL_DLL GDALDeserializeTransformer( CPLXMLNode *psTree, 
                                           GDALTransformerFunc *ppfnFunc, 
                                           void **ppTransformArg );
                                      

/* -------------------------------------------------------------------- */
/*      Contour Line Generation                                         */
/* -------------------------------------------------------------------- */

typedef CPLErr (*GDALContourWriter)( double dfLevel, int nPoints,
                                     double *padfX, double *padfY, void * );

typedef void *GDALContourGeneratorH;

GDALContourGeneratorH CPL_DLL
GDAL_CG_Create( int nWidth, int nHeight, 
                int bNoDataSet, double dfNoDataValue,
                double dfContourInterval, double dfContourBase,
                GDALContourWriter pfnWriter, void *pCBData );
CPLErr CPL_DLL GDAL_CG_FeedLine( GDALContourGeneratorH hCG, 
                                 double *padfScanline );
void CPL_DLL GDAL_CG_Destroy( GDALContourGeneratorH hCG );

typedef struct 
{
    void   *hLayer;

    double adfGeoTransform[6];
    
    int    nElevField;
    int    nIDField;
    int    nNextID;
} OGRContourWriterInfo;

CPLErr CPL_DLL 
OGRContourWriter( double, int, double *, double *, void *pInfo );

CPLErr CPL_DLL
GDALContourGenerate( GDALRasterBandH hBand, 
                            double dfContourInterval, double dfContourBase,
                            int nFixedLevelCount, double *padfFixedLevels,
                            int bUseNoData, double dfNoDataValue, 
                            void *hLayer, int iIDField, int iElevField,
                            GDALProgressFunc pfnProgress, void *pProgressArg );

/* -------------------------------------------------------------------- */
/*      Low level rasterizer API.                                       */
/* -------------------------------------------------------------------- */
typedef void (*llScanlineFunc)( void *pCBData, int nY, int nXStart, int nXEnd);


void GDALdllImageFilledPolygon(int nRasterXSize, int nRasterYSize, 
                               int nPartCount, int *panPartSize, 
                               double *padfX, double *padfY,
                               llScanlineFunc pfnScanlineFunc, void *pCBData );

/* -------------------------------------------------------------------- */
/*      High level API - GvShapes burned into GDAL raster.              */
/* -------------------------------------------------------------------- */

CPLErr CPL_DLL 
GDALRasterizeGeometries( GDALDatasetH hDS, 
                         int nBandCount, int *panBandList, 
                         int nGeomCount, OGRGeometryH *pahGeometries,
                         GDALTransformerFunc pfnTransformer, 
                         void *pTransformArg, 
                         double *padfGeomBurnValue,
                         char **papszOptions,
                         GDALProgressFunc pfnProgress, 
                         void * pProgressArg );

CPL_C_END
                            
#endif /* ndef GDAL_ALG_H_INCLUDED */
