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
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_ALG_PRIV_H_INCLUDED
#define GDAL_ALG_PRIV_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include <cstdint>

#include <set>

#include "gdal_alg.h"
#include "ogr_spatialref.h"

CPL_C_START

/** Source of the burn value */
typedef enum
{
    /*! Use value from padfBurnValue */ GBV_UserBurnValue = 0,
    /*! Use value from the Z coordinate */ GBV_Z = 1,
    /*! Use value form the M value */ GBV_M = 2
} GDALBurnValueSrc;

typedef enum
{
    GRMA_Replace = 0,
    GRMA_Add = 1,
} GDALRasterMergeAlg;

typedef struct
{
    unsigned char *pabyChunkBuf;
    int nXSize;
    int nYSize;
    int nBands;
    GDALDataType eType;
    int nPixelSpace;
    GSpacing nLineSpace;
    GSpacing nBandSpace;
    GDALDataType eBurnValueType;

    union
    {
        const std::int64_t *int64_values;
        const double *double_values;
    } burnValues;

    GDALBurnValueSrc eBurnValueSource;
    GDALRasterMergeAlg eMergeAlg;
    bool bFillSetVisitedPoints;
    std::set<uint64_t> *poSetVisitedPoints;
} GDALRasterizeInfo;

typedef enum
{
    GRO_Raster = 0,
    GRO_Vector = 1,
    GRO_Auto = 2,
} GDALRasterizeOptim;

/************************************************************************/
/*      Low level rasterizer API.                                       */
/************************************************************************/

typedef void (*llScanlineFunc)(void *, int, int, int, double);
typedef void (*llPointFunc)(void *, int, int, double);

void GDALdllImagePoint(int nRasterXSize, int nRasterYSize, int nPartCount,
                       const int *panPartSize, const double *padfX,
                       const double *padfY, const double *padfVariant,
                       llPointFunc pfnPointFunc, void *pCBData);

void GDALdllImageLine(int nRasterXSize, int nRasterYSize, int nPartCount,
                      const int *panPartSize, const double *padfX,
                      const double *padfY, const double *padfVariant,
                      llPointFunc pfnPointFunc, void *pCBData);

void GDALdllImageLineAllTouched(int nRasterXSize, int nRasterYSize,
                                int nPartCount, const int *panPartSize,
                                const double *padfX, const double *padfY,
                                const double *padfVariant,
                                llPointFunc pfnPointFunc, void *pCBData,
                                bool bAvoidBurningSamePoints,
                                bool bIntersectOnly);

void GDALdllImageFilledPolygon(int nRasterXSize, int nRasterYSize,
                               int nPartCount, const int *panPartSize,
                               const double *padfX, const double *padfY,
                               const double *padfVariant,
                               llScanlineFunc pfnScanlineFunc, void *pCBData,
                               bool bAvoidBurningSamePoints);

CPL_C_END

/************************************************************************/
/*                          Polygon Enumerator                          */
/************************************************************************/

#define GP_NODATA_MARKER -51502112

template <class DataType, class EqualityTest> class GDALRasterPolygonEnumeratorT

{
  private:
    void MergePolygon(int nSrcId, int nDstId);
    int NewPolygon(DataType nValue);

    CPL_DISALLOW_COPY_ASSIGN(GDALRasterPolygonEnumeratorT)

  public:  // these are intended to be readonly.
    GInt32 *panPolyIdMap = nullptr;
    DataType *panPolyValue = nullptr;

    int nNextPolygonId = 0;
    int nPolyAlloc = 0;

    int nConnectedness = 0;

  public:
    explicit GDALRasterPolygonEnumeratorT(int nConnectedness = 4);
    ~GDALRasterPolygonEnumeratorT();

    bool ProcessLine(DataType *panLastLineVal, DataType *panThisLineVal,
                     GInt32 *panLastLineId, GInt32 *panThisLineId, int nXSize);

    void CompleteMerges();

    void Clear();
};

struct IntEqualityTest
{
    bool operator()(std::int64_t a, std::int64_t b) const
    {
        return a == b;
    }
};

typedef GDALRasterPolygonEnumeratorT<std::int64_t, IntEqualityTest>
    GDALRasterPolygonEnumerator;

constexpr const char *GDAL_APPROX_TRANSFORMER_CLASS_NAME =
    "GDALApproxTransformer";
constexpr const char *GDAL_GEN_IMG_TRANSFORMER_CLASS_NAME =
    "GDALGenImgProjTransformer";
constexpr const char *GDAL_RPC_TRANSFORMER_CLASS_NAME = "GDALRPCTransformer";

bool GDALIsTransformer(void *hTransformerArg, const char *pszClassName);

typedef void *(*GDALTransformDeserializeFunc)(CPLXMLNode *psTree);

void CPL_DLL *GDALRegisterTransformDeserializer(
    const char *pszTransformName, GDALTransformerFunc pfnTransformerFunc,
    GDALTransformDeserializeFunc pfnDeserializeFunc);
void CPL_DLL GDALUnregisterTransformDeserializer(void *pData);

void GDALCleanupTransformDeserializerMutex();

/* Transformer cloning */

void *GDALCreateTPSTransformerInt(int nGCPCount, const GDAL_GCP *pasGCPList,
                                  int bReversed, char **papszOptions);

void CPL_DLL *GDALCloneTransformer(void *pTransformerArg);

void GDALRefreshGenImgProjTransformer(void *hTransformArg);
void GDALRefreshApproxTransformer(void *hTransformArg);

int GDALTransformLonLatToDestGenImgProjTransformer(void *hTransformArg,
                                                   double *pdfX, double *pdfY);
int GDALTransformLonLatToDestApproxTransformer(void *hTransformArg,
                                               double *pdfX, double *pdfY);

bool GDALTransformIsTranslationOnPixelBoundaries(
    GDALTransformerFunc pfnTransformer, void *pTransformerArg);

bool GDALTransformIsAffineNoRotation(GDALTransformerFunc pfnTransformer,
                                     void *pTransformerArg);

bool GDALTransformHasFastClone(void *pTransformerArg);

typedef struct _CPLQuadTree CPLQuadTree;

typedef struct
{
    GDALTransformerInfo sTI;

    bool bReversed;
    double dfOversampleFactor;

    // Map from target georef coordinates back to geolocation array
    // pixel line coordinates.  Built only if needed.
    int nBackMapWidth;
    int nBackMapHeight;
    double adfBackMapGeoTransform[6];  // Maps georef to pixel/line.

    bool bUseArray;
    void *pAccessors;

    // Geolocation bands.
    GDALDatasetH hDS_X;
    GDALRasterBandH hBand_X;
    GDALDatasetH hDS_Y;
    GDALRasterBandH hBand_Y;
    int bSwapXY;

    // Located geolocation data.
    int nGeoLocXSize;
    int nGeoLocYSize;
    double dfMinX;
    double dfYAtMinX;
    double dfMinY;
    double dfXAtMinY;
    double dfMaxX;
    double dfYAtMaxX;
    double dfMaxY;
    double dfXAtMaxY;

    int bHasNoData;
    double dfNoDataX;

    // Geolocation <-> base image mapping.
    double dfPIXEL_OFFSET;
    double dfPIXEL_STEP;
    double dfLINE_OFFSET;
    double dfLINE_STEP;

    bool bOriginIsTopLeftCorner;
    bool bGeographicSRSWithMinus180Plus180LongRange;
    CPLQuadTree *hQuadTree;

    char **papszGeolocationInfo;

} GDALGeoLocTransformInfo;

/************************************************************************/
/* ==================================================================== */
/*                       GDALReprojectionTransformer                    */
/* ==================================================================== */
/************************************************************************/

struct GDALReprojectionTransformInfo
{
    GDALTransformerInfo sTI;
    char **papszOptions = nullptr;
    double dfTime = 0.0;

    OGRCoordinateTransformation *poForwardTransform = nullptr;
    OGRCoordinateTransformation *poReverseTransform = nullptr;

    GDALReprojectionTransformInfo() : sTI()
    {
        memset(&sTI, 0, sizeof(sTI));
    }

    GDALReprojectionTransformInfo(const GDALReprojectionTransformInfo &) =
        delete;
    GDALReprojectionTransformInfo &
    operator=(const GDALReprojectionTransformInfo &) = delete;
};

/************************************************************************/
/* ==================================================================== */
/*                       GDALGenImgProjTransformer                      */
/* ==================================================================== */
/************************************************************************/

typedef struct
{

    GDALTransformerInfo sTI;

    double adfSrcGeoTransform[6];
    double adfSrcInvGeoTransform[6];

    void *pSrcTransformArg;
    GDALTransformerFunc pSrcTransformer;

    void *pReprojectArg;
    GDALTransformerFunc pReproject;

    double adfDstGeoTransform[6];
    double adfDstInvGeoTransform[6];

    void *pDstTransformArg;
    GDALTransformerFunc pDstTransformer;

    // Memorize the value of the CHECK_WITH_INVERT_PROJ at the time we
    // instantiated the object, to be able to decide if
    // GDALRefreshGenImgProjTransformer() must do something or not.
    bool bCheckWithInvertPROJ;

    // Set to TRUE when the transformation pipline is a custom one.
    bool bHasCustomTransformationPipeline;

} GDALGenImgProjTransformInfo;

/************************************************************************/
/*      Color table related                                             */
/************************************************************************/

// Definitions exists for T = GUInt32 and T = GUIntBig.
template <class T>
int GDALComputeMedianCutPCTInternal(
    GDALRasterBandH hRed, GDALRasterBandH hGreen, GDALRasterBandH hBlue,
    GByte *pabyRedBand, GByte *pabyGreenBand, GByte *pabyBlueBand,
    int (*pfnIncludePixel)(int, int, void *), int nColors, int nBits,
    T *panHistogram, GDALColorTableH hColorTable, GDALProgressFunc pfnProgress,
    void *pProgressArg);

int GDALDitherRGB2PCTInternal(GDALRasterBandH hRed, GDALRasterBandH hGreen,
                              GDALRasterBandH hBlue, GDALRasterBandH hTarget,
                              GDALColorTableH hColorTable, int nBits,
                              GInt16 *pasDynamicColorMap, int bDither,
                              GDALProgressFunc pfnProgress, void *pProgressArg);

#define PRIME_FOR_65536 98317

// See HashHistogram structure in gdalmediancut.cpp and ColorIndex structure in
// gdaldither.cpp 6 * sizeof(int) should be the size of the largest of both
// structures.
#define MEDIAN_CUT_AND_DITHER_BUFFER_SIZE_65536                                \
    (6 * sizeof(int) * PRIME_FOR_65536)

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
    bool operator()(float a, float b)
    {
        return GDALFloatEquals(a, b) == TRUE;
    }
};

bool GDALComputeAreaOfInterest(OGRSpatialReference *poSRS, double adfGT[6],
                               int nXSize, int nYSize,
                               double &dfWestLongitudeDeg,
                               double &dfSouthLatitudeDeg,
                               double &dfEastLongitudeDeg,
                               double &dfNorthLatitudeDeg);

bool GDALComputeAreaOfInterest(OGRSpatialReference *poSRS, double dfX1,
                               double dfY1, double dfX2, double dfY2,
                               double &dfWestLongitudeDeg,
                               double &dfSouthLatitudeDeg,
                               double &dfEastLongitudeDeg,
                               double &dfNorthLatitudeDeg);

CPLStringList GDALCreateGeolocationMetadata(GDALDatasetH hBaseDS,
                                            const char *pszGeolocationDataset,
                                            bool bIsSource);

void *GDALCreateGeoLocTransformerEx(GDALDatasetH hBaseDS,
                                    CSLConstList papszGeolocationInfo,
                                    int bReversed, const char *pszSourceDataset,
                                    CSLConstList papszTransformOptions);

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* ndef GDAL_ALG_PRIV_H_INCLUDED */
