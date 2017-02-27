/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Image Processing Algorithms
 * Purpose:  Prototypes, and definitions for various GDAL based algorithms.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GDAL_ALG_H_INCLUDED
#define GDAL_ALG_H_INCLUDED

/**
 * \file gdal_alg.h
 *
 * Public (C callable) GDAL algorithm entry points, and definitions.
 */

#ifndef DOXYGEN_SKIP
#include "gdal.h"
#include "cpl_minixml.h"
#include "ogr_api.h"
#endif

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

CPLErr CPL_DLL CPL_STDCALL
GDALComputeProximity( GDALRasterBandH hSrcBand,
                      GDALRasterBandH hProximityBand,
                      char **papszOptions,
                      GDALProgressFunc pfnProgress,
                      void * pProgressArg );

CPLErr CPL_DLL CPL_STDCALL
GDALFillNodata( GDALRasterBandH hTargetBand,
                GDALRasterBandH hMaskBand,
                double dfMaxSearchDist,
                int bDeprecatedOption,
                int nSmoothingIterations,
                char **papszOptions,
                GDALProgressFunc pfnProgress,
                void * pProgressArg );

CPLErr CPL_DLL CPL_STDCALL
GDALPolygonize( GDALRasterBandH hSrcBand,
                GDALRasterBandH hMaskBand,
                OGRLayerH hOutLayer, int iPixValField,
                char **papszOptions,
                GDALProgressFunc pfnProgress,
                void * pProgressArg );

CPLErr CPL_DLL CPL_STDCALL
GDALFPolygonize( GDALRasterBandH hSrcBand,
                GDALRasterBandH hMaskBand,
                OGRLayerH hOutLayer, int iPixValField,
                char **papszOptions,
                GDALProgressFunc pfnProgress,
                void * pProgressArg );

CPLErr CPL_DLL CPL_STDCALL
GDALSieveFilter( GDALRasterBandH hSrcBand, GDALRasterBandH hMaskBand,
                 GDALRasterBandH hDstBand,
                 int nSizeThreshold, int nConnectedness,
                 char **papszOptions,
                 GDALProgressFunc pfnProgress,
                 void * pProgressArg );

/*
 * Warp Related.
 */

typedef int
(*GDALTransformerFunc)( void *pTransformerArg,
                        int bDstToSrc, int nPointCount,
                        double *x, double *y, double *z, int *panSuccess );

/*! @cond Doxygen_Suppress */
#define GDAL_GTI2_SIGNATURE     "GTI2"

typedef struct {
    GByte abySignature[4];
    const char *pszClassName;
    GDALTransformerFunc pfnTransform;
    void (*pfnCleanup)( void * pTransformerArg );
    CPLXMLNode *(*pfnSerialize)( void * pTransformerArg );
    void* (*pfnCreateSimilar)( void* pTransformerArg, double dfSrcRatioX, double dfSrcRatioY );
} GDALTransformerInfo;
/*! @endcond */

/*! @cond Doxygen_Suppress */
void CPL_DLL GDALDestroyTransformer( void *pTransformerArg );
int  CPL_DLL GDALUseTransformer( void *pTransformerArg,
                                 int bDstToSrc, int nPointCount,
                                 double *x, double *y, double *z,
                                 int *panSuccess );
void* GDALCreateSimilarTransformer( void* psTransformerArg, double dfSrcRatioX, double dfSrcRatioY );
/*! @endcond */

/* High level transformer for going from image coordinates on one file
   to image coordinates on another, potentially doing reprojection,
   utilizing GCPs or using the geotransform. */

void CPL_DLL *
GDALCreateGenImgProjTransformer( GDALDatasetH hSrcDS, const char *pszSrcWKT,
                                 GDALDatasetH hDstDS, const char *pszDstWKT,
                                 int bGCPUseOK, double dfGCPErrorThreshold,
                                 int nOrder );
void CPL_DLL *
GDALCreateGenImgProjTransformer2( GDALDatasetH hSrcDS, GDALDatasetH hDstDS,
                                  char **papszOptions );
void CPL_DLL *
GDALCreateGenImgProjTransformer3( const char *pszSrcWKT,
                                  const double *padfSrcGeoTransform,
                                  const char *pszDstWKT,
                                  const double *padfDstGeoTransform );
void CPL_DLL GDALSetGenImgProjTransformerDstGeoTransform( void *,
                                                          const double * );
void CPL_DLL GDALDestroyGenImgProjTransformer( void * );
int CPL_DLL GDALGenImgProjTransform(
    void *pTransformArg, int bDstToSrc, int nPointCount,
    double *x, double *y, double *z, int *panSuccess );

void GDALSetTransformerDstGeoTransform( void *, const double * );

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

/* GCP based transformer with refinement of the GCPs ... forward is to georef coordinates */
void CPL_DLL *
GDALCreateGCPRefineTransformer( int nGCPCount, const GDAL_GCP *pasGCPList,
                                int nReqOrder, int bReversed, double tolerance, int minimumGcps);

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

/*! @cond Doxygen_Suppress */
char CPL_DLL ** RPCInfoToMD( GDALRPCInfo *psRPCInfo );
/*! @endcond */

/* RPC based transformer ... src is pixel/line/elev, dst is long/lat/elev */

void CPL_DLL *
GDALCreateRPCTransformer( GDALRPCInfo *psRPC, int bReversed,
                          double dfPixErrThreshold,
                          char **papszOptions );
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

/*! @cond Doxygen_Suppress */
CPLXMLNode CPL_DLL *
GDALSerializeTransformer( GDALTransformerFunc pfnFunc, void *pTransformArg );
CPLErr CPL_DLL GDALDeserializeTransformer( CPLXMLNode *psTree,
                                           GDALTransformerFunc *ppfnFunc,
                                           void **ppTransformArg );
/*! @endcond */

CPLErr CPL_DLL
GDALTransformGeolocations( GDALRasterBandH hXBand,
                           GDALRasterBandH hYBand,
                           GDALRasterBandH hZBand,
                           GDALTransformerFunc pfnTransformer,
                           void *pTransformArg,
                           GDALProgressFunc pfnProgress,
                           void *pProgressArg,
                           char **papszOptions );

/* -------------------------------------------------------------------- */
/*      Contour Line Generation                                         */
/* -------------------------------------------------------------------- */

/** Contour writer callback type */
typedef CPLErr (*GDALContourWriter)( double dfLevel, int nPoints,
                                     double *padfX, double *padfY, void * );

/** Contour generator opaque type */
typedef void *GDALContourGeneratorH;

GDALContourGeneratorH CPL_DLL
GDAL_CG_Create( int nWidth, int nHeight,
                int bNoDataSet, double dfNoDataValue,
                double dfContourInterval, double dfContourBase,
                GDALContourWriter pfnWriter, void *pCBData );
CPLErr CPL_DLL GDAL_CG_FeedLine( GDALContourGeneratorH hCG,
                                 double *padfScanline );
void CPL_DLL GDAL_CG_Destroy( GDALContourGeneratorH hCG );

/*! @cond Doxygen_Suppress */
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
/*! @endcond */

CPLErr CPL_DLL
GDALContourGenerate( GDALRasterBandH hBand,
                            double dfContourInterval, double dfContourBase,
                            int nFixedLevelCount, double *padfFixedLevels,
                            int bUseNoData, double dfNoDataValue,
                            void *hLayer, int iIDField, int iElevField,
                            GDALProgressFunc pfnProgress, void *pProgressArg );

/************************************************************************/
/*      Rasterizer API - geometries burned into GDAL raster.            */
/************************************************************************/

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
CPLErr CPL_DLL
GDALRasterizeLayers( GDALDatasetH hDS,
                     int nBandCount, int *panBandList,
                     int nLayerCount, OGRLayerH *pahLayers,
                     GDALTransformerFunc pfnTransformer,
                     void *pTransformArg,
                     double *padfLayerBurnValues,
                     char **papszOptions,
                     GDALProgressFunc pfnProgress,
                     void *pProgressArg );

CPLErr CPL_DLL
GDALRasterizeLayersBuf( void *pData, int nBufXSize, int nBufYSize,
                        GDALDataType eBufType, int nPixelSpace, int nLineSpace,
                        int nLayerCount, OGRLayerH *pahLayers,
                        const char *pszDstProjection,
                        double *padfDstGeoTransform,
                        GDALTransformerFunc pfnTransformer,
                        void *pTransformArg, double dfBurnValue,
                        char **papszOptions, GDALProgressFunc pfnProgress,
                        void *pProgressArg );

/************************************************************************/
/*  Gridding interface.                                                 */
/************************************************************************/

/** Gridding Algorithms */
typedef enum {
  /*! Inverse distance to a power */    GGA_InverseDistanceToAPower = 1,
  /*! Moving Average */                 GGA_MovingAverage = 2,
  /*! Nearest Neighbor */               GGA_NearestNeighbor = 3,
  /*! Minimum Value (Data Metric) */    GGA_MetricMinimum = 4,
  /*! Maximum Value (Data Metric) */    GGA_MetricMaximum = 5,
  /*! Data Range (Data Metric) */       GGA_MetricRange = 6,
  /*! Number of Points (Data Metric) */ GGA_MetricCount = 7,
  /*! Average Distance (Data Metric) */ GGA_MetricAverageDistance = 8,
  /*! Average Distance Between Data Points (Data Metric) */
                                        GGA_MetricAverageDistancePts = 9,
  /*! Linear interpolation (from Delaunay triangulation. Since GDAL 2.1 */
                                        GGA_Linear = 10,
  /*! Inverse distance to a power with nearest neighbor search for max points */
                                        GGA_InverseDistanceToAPowerNearestNeighbor = 11
} GDALGridAlgorithm;

/** Inverse distance to a power method control options */
typedef struct
{
    /*! Weighting power. */
    double  dfPower;
    /*! Smoothing parameter. */
    double  dfSmoothing;
    /*! Reserved for future use. */
    double  dfAnisotropyRatio;
    /*! Reserved for future use. */
    double  dfAnisotropyAngle;
    /*! The first radius (X axis if rotation angle is 0) of search ellipse. */
    double  dfRadius1;
    /*! The second radius (Y axis if rotation angle is 0) of search ellipse. */
    double  dfRadius2;
    /*! Angle of ellipse rotation in degrees.
     *
     * Ellipse rotated counter clockwise.
     */
    double  dfAngle;
    /*! Maximum number of data points to use.
     *
     * Do not search for more points than this number.
     * If less amount of points found the grid node considered empty and will
     * be filled with NODATA marker.
     */
    GUInt32 nMaxPoints;
    /*! Minimum number of data points to use.
     *
     * If less amount of points found the grid node considered empty and will
     * be filled with NODATA marker.
     */
    GUInt32 nMinPoints;
    /*! No data marker to fill empty points. */
    double  dfNoDataValue;
} GDALGridInverseDistanceToAPowerOptions;

/** Inverse distance to a power, with nearest neighbour search, control options */
typedef struct
{
    /*! Weighting power. */
    double  dfPower;
    /*! The radius of search circle. */
    double  dfRadius;
    /*! Smoothing parameter. */
    double  dfSmoothing;

    /*! Maximum number of data points to use.
     *
     * Do not search for more points than this number.
     * If less amount of points found the grid node considered empty and will
     * be filled with NODATA marker.
     */
    GUInt32 nMaxPoints;
    /*! Minimum number of data points to use.
     *
     * If less amount of points found the grid node considered empty and will
     * be filled with NODATA marker.
     */
    GUInt32 nMinPoints;
    /*! No data marker to fill empty points. */
    double  dfNoDataValue;
} GDALGridInverseDistanceToAPowerNearestNeighborOptions;

/** Moving average method control options */
typedef struct
{
    /*! The first radius (X axis if rotation angle is 0) of search ellipse. */
    double  dfRadius1;
    /*! The second radius (Y axis if rotation angle is 0) of search ellipse. */
    double  dfRadius2;
    /*! Angle of ellipse rotation in degrees.
     *
     * Ellipse rotated counter clockwise.
     */
    double  dfAngle;
    /*! Minimum number of data points to average.
     *
     * If less amount of points found the grid node considered empty and will
     * be filled with NODATA marker.
     */
    GUInt32 nMinPoints;
    /*! No data marker to fill empty points. */
    double  dfNoDataValue;
} GDALGridMovingAverageOptions;

/** Nearest neighbor method control options */
typedef struct
{
    /*! The first radius (X axis if rotation angle is 0) of search ellipse. */
    double  dfRadius1;
    /*! The second radius (Y axis if rotation angle is 0) of search ellipse. */
    double  dfRadius2;
    /*! Angle of ellipse rotation in degrees.
     *
     * Ellipse rotated counter clockwise.
     */
    double  dfAngle;
    /*! No data marker to fill empty points. */
    double  dfNoDataValue;
} GDALGridNearestNeighborOptions;

/** Data metrics method control options */
typedef struct
{
    /*! The first radius (X axis if rotation angle is 0) of search ellipse. */
    double  dfRadius1;
    /*! The second radius (Y axis if rotation angle is 0) of search ellipse. */
    double  dfRadius2;
    /*! Angle of ellipse rotation in degrees.
     *
     * Ellipse rotated counter clockwise.
     */
    double  dfAngle;
    /*! Minimum number of data points to average.
     *
     * If less amount of points found the grid node considered empty and will
     * be filled with NODATA marker.
     */
    GUInt32 nMinPoints;
    /*! No data marker to fill empty points. */
    double  dfNoDataValue;
} GDALGridDataMetricsOptions;

/** Linear method control options */
typedef struct
{
    /*! In case the point to be interpolated does not fit into a triangle of
     * the Delaunay triangulation, use that maximum distance to search a nearest
     * neighbour, or use nodata otherwise. If set to -1, the search distance is infinite.
     * If set to 0, nodata value will be always used.
     */
    double  dfRadius;
    /*! No data marker to fill empty points. */
    double  dfNoDataValue;
} GDALGridLinearOptions;

CPLErr CPL_DLL
GDALGridCreate( GDALGridAlgorithm, const void *, GUInt32,
                const double *, const double *, const double *,
                double, double, double, double,
                GUInt32, GUInt32, GDALDataType, void *,
                GDALProgressFunc, void *);

/** Grid context opaque type */
typedef struct GDALGridContext GDALGridContext;

GDALGridContext CPL_DLL*
GDALGridContextCreate( GDALGridAlgorithm eAlgorithm, const void *poOptions,
                       GUInt32 nPoints,
                       const double *padfX, const double *padfY, const double *padfZ,
                       int bCallerWillKeepPointArraysAlive );

void CPL_DLL GDALGridContextFree(GDALGridContext* psContext);

CPLErr CPL_DLL GDALGridContextProcess(GDALGridContext* psContext,
                              double dfXMin, double dfXMax, double dfYMin, double dfYMax,
                              GUInt32 nXSize, GUInt32 nYSize, GDALDataType eType, void *pData,
                              GDALProgressFunc pfnProgress, void *pProgressArg );

GDAL_GCP CPL_DLL *
GDALComputeMatchingPoints( GDALDatasetH hFirstImage,
                           GDALDatasetH hSecondImage,
                           char **papszOptions,
                           int *pnGCPCount );

/************************************************************************/
/*  Delaunay triangulation interface.                                   */
/************************************************************************/

/** Triangle fact */
typedef struct
{
    int anVertexIdx[3];   /**< index to the padfX/padfY arrays */
    int anNeighborIdx[3]; /**< index to GDALDelaunayTriangulation.pasFacets, or -1 */
                          /* anNeighborIdx[k] is the triangle to the opposite side */
                          /* of the opposite segment of anVertexIdx[k] */
} GDALTriFacet;

/** Triangle barycentric coefficients.
 *
 * Conversion from cartesian (x,y) to barycentric (l1,l2,l3) with :
 *  l1 = dfMul1X * (x - dfCxtX) + dfMul1Y * (y - dfCstY)
 *  l2 = dfMul2X * (x - dfCxtX) + dfMul2Y * (y - dfCstY)
 *  l3 = 1 - l1 - l2
 */
typedef struct
{
    double      dfMul1X; /**< dfMul1X */
    double      dfMul1Y; /**< dfMul1Y */
    double      dfMul2X; /**< dfMul2X */
    double      dfMul2Y; /**< dfMul2Y */
    double      dfCstX;  /**< dfCstX */
    double      dfCstY;  /**< dfCstY */
} GDALTriBarycentricCoefficients;

/** Triangulation structure */
typedef struct
{
    int                             nFacets;       /**< number of facets */
    GDALTriFacet                   *pasFacets;     /**< array of nFacets facets */
    GDALTriBarycentricCoefficients *pasFacetCoefficients; /**< arra of nFacets barycentric coefficients */
} GDALTriangulation;

int CPL_DLL GDALHasTriangulation(void);

GDALTriangulation CPL_DLL *GDALTriangulationCreateDelaunay(int nPoints,
                                                           const double* padfX,
                                                           const double* padfY);
int  CPL_DLL GDALTriangulationComputeBarycentricCoefficients(
                                                GDALTriangulation* psDT,
                                                const double* padfX,
                                                const double* padfY);
int  CPL_DLL GDALTriangulationComputeBarycentricCoordinates(
                                                const GDALTriangulation* psDT,
                                                int nFacetIdx,
                                                double dfX,
                                                double dfY,
                                                double* pdfL1,
                                                double* pdfL2,
                                                double* pdfL3);
int CPL_DLL GDALTriangulationFindFacetBruteForce( const GDALTriangulation* psDT,
                                                  double dfX,
                                                  double dfY,
                                                  int* panOutputFacetIdx );
int CPL_DLL GDALTriangulationFindFacetDirected( const GDALTriangulation* psDT,
                                                int nFacetIdx,
                                                double dfX,
                                                double dfY,
                                                int* panOutputFacetIdx );
void CPL_DLL GDALTriangulationFree(GDALTriangulation* psDT);

/*! @cond Doxygen_Suppress */
// GDAL internal use only
void GDALTriangulationTerminate(void);
/*! @endcond */

CPL_C_END

#endif /* ndef GDAL_ALG_H_INCLUDED */
