/* ****************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL scattered data gridding (interpolation) tool
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 * ****************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_port.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"
#include "commonutils.h"
#include "gdalargumentparser.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdalgrid.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                          GDALGridOptions                             */
/************************************************************************/

/** Options for use with GDALGrid(). GDALGridOptions* must be allocated
 * and freed with GDALGridOptionsNew() and GDALGridOptionsFree() respectively.
 */
struct GDALGridOptions
{
    /*! output format. Use the short format name. */
    std::string osFormat{};

    /*! allow or suppress progress monitor and other non-error output */
    bool bQuiet = true;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress = GDALDummyProgress;

    /*! pointer to the progress data variable */
    void *pProgressData = nullptr;

    CPLStringList aosLayers{};
    std::string osBurnAttribute{};
    double dfIncreaseBurnValue = 0.0;
    double dfMultiplyBurnValue = 1.0;
    std::string osWHERE{};
    std::string osSQL{};
    GDALDataType eOutputType = GDT_Float64;
    CPLStringList aosCreateOptions{};
    int nXSize = 0;
    int nYSize = 0;
    double dfXRes = 0;
    double dfYRes = 0;
    double dfXMin = 0;
    double dfXMax = 0;
    double dfYMin = 0;
    double dfYMax = 0;
    bool bIsXExtentSet = false;
    bool bIsYExtentSet = false;
    GDALGridAlgorithm eAlgorithm = GGA_InverseDistanceToAPower;
    std::unique_ptr<void, VSIFreeReleaser> pOptions{};
    std::string osOutputSRS{};
    std::unique_ptr<OGRGeometry> poSpatialFilter{};
    bool bClipSrc = false;
    std::unique_ptr<OGRGeometry> poClipSrc{};
    std::string osClipSrcDS{};
    std::string osClipSrcSQL{};
    std::string osClipSrcLayer{};
    std::string osClipSrcWhere{};
    bool bNoDataSet = false;
    double dfNoDataValue = 0;

    GDALGridOptions()
    {
        void *l_pOptions = nullptr;
        GDALGridParseAlgorithmAndOptions(szAlgNameInvDist, &eAlgorithm,
                                         &l_pOptions);
        pOptions.reset(l_pOptions);
    }
};

/************************************************************************/
/*                          GetAlgorithmName()                          */
/*                                                                      */
/*      Grids algorithm code into mnemonic name.                        */
/************************************************************************/

static void PrintAlgorithmAndOptions(GDALGridAlgorithm eAlgorithm,
                                     void *pOptions)
{
    switch (eAlgorithm)
    {
        case GGA_InverseDistanceToAPower:
        {
            printf("Algorithm name: \"%s\".\n", szAlgNameInvDist);
            GDALGridInverseDistanceToAPowerOptions *pOptions2 =
                static_cast<GDALGridInverseDistanceToAPowerOptions *>(pOptions);
            CPLprintf("Options are "
                      "\"power=%f:smoothing=%f:radius1=%f:radius2=%f:angle=%f"
                      ":max_points=%u:min_points=%u:nodata=%f\"\n",
                      pOptions2->dfPower, pOptions2->dfSmoothing,
                      pOptions2->dfRadius1, pOptions2->dfRadius2,
                      pOptions2->dfAngle, pOptions2->nMaxPoints,
                      pOptions2->nMinPoints, pOptions2->dfNoDataValue);
            break;
        }
        case GGA_InverseDistanceToAPowerNearestNeighbor:
        {
            printf("Algorithm name: \"%s\".\n",
                   szAlgNameInvDistNearestNeighbor);
            GDALGridInverseDistanceToAPowerNearestNeighborOptions *pOptions2 =
                static_cast<
                    GDALGridInverseDistanceToAPowerNearestNeighborOptions *>(
                    pOptions);
            CPLString osStr;
            osStr.Printf("power=%f:smoothing=%f:radius=%f"
                         ":max_points=%u:min_points=%u:nodata=%f",
                         pOptions2->dfPower, pOptions2->dfSmoothing,
                         pOptions2->dfRadius, pOptions2->nMaxPoints,
                         pOptions2->nMinPoints, pOptions2->dfNoDataValue);
            if (pOptions2->nMinPointsPerQuadrant > 0)
                osStr += CPLSPrintf(":min_points_per_quadrant=%u",
                                    pOptions2->nMinPointsPerQuadrant);
            if (pOptions2->nMaxPointsPerQuadrant > 0)
                osStr += CPLSPrintf(":max_points_per_quadrant=%u",
                                    pOptions2->nMaxPointsPerQuadrant);
            printf("Options are: \"%s\n", osStr.c_str()); /* ok */
            break;
        }
        case GGA_MovingAverage:
        {
            printf("Algorithm name: \"%s\".\n", szAlgNameAverage);
            GDALGridMovingAverageOptions *pOptions2 =
                static_cast<GDALGridMovingAverageOptions *>(pOptions);
            CPLString osStr;
            osStr.Printf("radius1=%f:radius2=%f:angle=%f:min_points=%u"
                         ":nodata=%f",
                         pOptions2->dfRadius1, pOptions2->dfRadius2,
                         pOptions2->dfAngle, pOptions2->nMinPoints,
                         pOptions2->dfNoDataValue);
            if (pOptions2->nMinPointsPerQuadrant > 0)
                osStr += CPLSPrintf(":min_points_per_quadrant=%u",
                                    pOptions2->nMinPointsPerQuadrant);
            if (pOptions2->nMaxPointsPerQuadrant > 0)
                osStr += CPLSPrintf(":max_points_per_quadrant=%u",
                                    pOptions2->nMaxPointsPerQuadrant);
            if (pOptions2->nMaxPoints > 0)
                osStr += CPLSPrintf(":max_points=%u", pOptions2->nMaxPoints);
            printf("Options are: \"%s\n", osStr.c_str()); /* ok */
            break;
        }
        case GGA_NearestNeighbor:
        {
            printf("Algorithm name: \"%s\".\n", szAlgNameNearest);
            GDALGridNearestNeighborOptions *pOptions2 =
                static_cast<GDALGridNearestNeighborOptions *>(pOptions);
            CPLprintf("Options are "
                      "\"radius1=%f:radius2=%f:angle=%f:nodata=%f\"\n",
                      pOptions2->dfRadius1, pOptions2->dfRadius2,
                      pOptions2->dfAngle, pOptions2->dfNoDataValue);
            break;
        }
        case GGA_MetricMinimum:
        case GGA_MetricMaximum:
        case GGA_MetricRange:
        case GGA_MetricCount:
        case GGA_MetricAverageDistance:
        case GGA_MetricAverageDistancePts:
        {
            const char *pszAlgName = "";
            switch (eAlgorithm)
            {
                case GGA_MetricMinimum:
                    pszAlgName = szAlgNameMinimum;
                    break;
                case GGA_MetricMaximum:
                    pszAlgName = szAlgNameMaximum;
                    break;
                case GGA_MetricRange:
                    pszAlgName = szAlgNameRange;
                    break;
                case GGA_MetricCount:
                    pszAlgName = szAlgNameCount;
                    break;
                case GGA_MetricAverageDistance:
                    pszAlgName = szAlgNameAverageDistance;
                    break;
                case GGA_MetricAverageDistancePts:
                    pszAlgName = szAlgNameAverageDistancePts;
                    break;
                default:
                    CPLAssert(false);
                    break;
            }
            printf("Algorithm name: \"%s\".\n", pszAlgName);
            GDALGridDataMetricsOptions *pOptions2 =
                static_cast<GDALGridDataMetricsOptions *>(pOptions);
            CPLString osStr;
            osStr.Printf("radius1=%f:radius2=%f:angle=%f:min_points=%u"
                         ":nodata=%f",
                         pOptions2->dfRadius1, pOptions2->dfRadius2,
                         pOptions2->dfAngle, pOptions2->nMinPoints,
                         pOptions2->dfNoDataValue);
            if (pOptions2->nMinPointsPerQuadrant > 0)
                osStr += CPLSPrintf(":min_points_per_quadrant=%u",
                                    pOptions2->nMinPointsPerQuadrant);
            if (pOptions2->nMaxPointsPerQuadrant > 0)
                osStr += CPLSPrintf(":max_points_per_quadrant=%u",
                                    pOptions2->nMaxPointsPerQuadrant);
            printf("Options are: \"%s\n", osStr.c_str()); /* ok */
            break;
        }
        case GGA_Linear:
        {
            printf("Algorithm name: \"%s\".\n", szAlgNameLinear);
            GDALGridLinearOptions *pOptions2 =
                static_cast<GDALGridLinearOptions *>(pOptions);
            CPLprintf("Options are "
                      "\"radius=%f:nodata=%f\"\n",
                      pOptions2->dfRadius, pOptions2->dfNoDataValue);
            break;
        }
        default:
        {
            printf("Algorithm is unknown.\n");
            break;
        }
    }
}

/************************************************************************/
/*  Extract point coordinates from the geometry reference and set the   */
/*  Z value as requested. Test whether we are in the clipped region     */
/*  before processing.                                                  */
/************************************************************************/

class GDALGridGeometryVisitor final : public OGRDefaultConstGeometryVisitor
{
  public:
    const OGRGeometry *poClipSrc = nullptr;
    int iBurnField = 0;
    double dfBurnValue = 0;
    double dfIncreaseBurnValue = 0;
    double dfMultiplyBurnValue = 1;
    std::vector<double> adfX{};
    std::vector<double> adfY{};
    std::vector<double> adfZ{};

    using OGRDefaultConstGeometryVisitor::visit;

    void visit(const OGRPoint *p) override
    {
        if (poClipSrc && !p->Within(poClipSrc))
            return;

        if (iBurnField < 0 && std::isnan(p->getZ()))
            return;

        adfX.push_back(p->getX());
        adfY.push_back(p->getY());
        if (iBurnField < 0)
            adfZ.push_back((p->getZ() + dfIncreaseBurnValue) *
                           dfMultiplyBurnValue);
        else
            adfZ.push_back((dfBurnValue + dfIncreaseBurnValue) *
                           dfMultiplyBurnValue);
    }
};

/************************************************************************/
/*                            ProcessLayer()                            */
/*                                                                      */
/*      Process all the features in a layer selection, collecting       */
/*      geometries and burn values.                                     */
/************************************************************************/

static CPLErr ProcessLayer(OGRLayer *poSrcLayer, GDALDataset *poDstDS,
                           const OGRGeometry *poClipSrc, int nXSize, int nYSize,
                           int nBand, bool &bIsXExtentSet, bool &bIsYExtentSet,
                           double &dfXMin, double &dfXMax, double &dfYMin,
                           double &dfYMax, const std::string &osBurnAttribute,
                           const double dfIncreaseBurnValue,
                           const double dfMultiplyBurnValue, GDALDataType eType,
                           GDALGridAlgorithm eAlgorithm, void *pOptions,
                           bool bQuiet, GDALProgressFunc pfnProgress,
                           void *pProgressData)

{
    /* -------------------------------------------------------------------- */
    /*      Get field index, and check.                                     */
    /* -------------------------------------------------------------------- */
    int iBurnField = -1;

    if (!osBurnAttribute.empty())
    {
        iBurnField =
            poSrcLayer->GetLayerDefn()->GetFieldIndex(osBurnAttribute.c_str());
        if (iBurnField == -1)
        {
            printf("Failed to find field %s on layer %s, skipping.\n",
                   osBurnAttribute.c_str(), poSrcLayer->GetName());
            return CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Collect the geometries from this layer, and build list of       */
    /*      values to be interpolated.                                      */
    /* -------------------------------------------------------------------- */
    GDALGridGeometryVisitor oVisitor;
    oVisitor.poClipSrc = poClipSrc;
    oVisitor.iBurnField = iBurnField;
    oVisitor.dfIncreaseBurnValue = dfIncreaseBurnValue;
    oVisitor.dfMultiplyBurnValue = dfMultiplyBurnValue;

    for (auto &&poFeat : poSrcLayer)
    {
        const OGRGeometry *poGeom = poFeat->GetGeometryRef();
        if (poGeom)
        {
            if (iBurnField >= 0)
            {
                if (!poFeat->IsFieldSetAndNotNull(iBurnField))
                {
                    continue;
                }
                oVisitor.dfBurnValue = poFeat->GetFieldAsDouble(iBurnField);
            }

            poGeom->accept(&oVisitor);
        }
    }

    if (oVisitor.adfX.empty())
    {
        printf("No point geometry found on layer %s, skipping.\n",
               poSrcLayer->GetName());
        return CE_None;
    }

    /* -------------------------------------------------------------------- */
    /*      Compute grid geometry.                                          */
    /* -------------------------------------------------------------------- */
    if (!bIsXExtentSet || !bIsYExtentSet)
    {
        OGREnvelope sEnvelope;
        if (poSrcLayer->GetExtent(&sEnvelope, TRUE) == OGRERR_FAILURE)
        {
            return CE_Failure;
        }

        if (!bIsXExtentSet)
        {
            dfXMin = sEnvelope.MinX;
            dfXMax = sEnvelope.MaxX;
            bIsXExtentSet = true;
        }

        if (!bIsYExtentSet)
        {
            dfYMin = sEnvelope.MinY;
            dfYMax = sEnvelope.MaxY;
            bIsYExtentSet = true;
        }
    }

    // Produce north-up images
    if (dfYMin < dfYMax)
        std::swap(dfYMin, dfYMax);

    /* -------------------------------------------------------------------- */
    /*      Perform gridding.                                               */
    /* -------------------------------------------------------------------- */

    const double dfDeltaX = (dfXMax - dfXMin) / nXSize;
    const double dfDeltaY = (dfYMax - dfYMin) / nYSize;

    if (!bQuiet)
    {
        printf("Grid data type is \"%s\"\n", GDALGetDataTypeName(eType));
        printf("Grid size = (%d %d).\n", nXSize, nYSize);
        CPLprintf("Corner coordinates = (%f %f)-(%f %f).\n", dfXMin, dfYMin,
                  dfXMax, dfYMax);
        CPLprintf("Grid cell size = (%f %f).\n", dfDeltaX, dfDeltaY);
        printf("Source point count = %lu.\n",
               static_cast<unsigned long>(oVisitor.adfX.size()));
        PrintAlgorithmAndOptions(eAlgorithm, pOptions);
        printf("\n");
    }

    GDALRasterBand *poBand = poDstDS->GetRasterBand(nBand);

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eType);

    // Try to grow the work buffer up to 16 MB if it is smaller
    poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    if (nXSize == 0 || nYSize == 0 || nBlockXSize == 0 || nBlockYSize == 0)
        return CE_Failure;

    const int nDesiredBufferSize = 16 * 1024 * 1024;
    if (nBlockXSize < nXSize && nBlockYSize < nYSize &&
        nBlockXSize < nDesiredBufferSize / (nBlockYSize * nDataTypeSize))
    {
        const int nNewBlockXSize =
            nDesiredBufferSize / (nBlockYSize * nDataTypeSize);
        nBlockXSize = (nNewBlockXSize / nBlockXSize) * nBlockXSize;
        if (nBlockXSize > nXSize)
            nBlockXSize = nXSize;
    }
    else if (nBlockXSize == nXSize && nBlockYSize < nYSize &&
             nBlockYSize < nDesiredBufferSize / (nXSize * nDataTypeSize))
    {
        const int nNewBlockYSize =
            nDesiredBufferSize / (nXSize * nDataTypeSize);
        nBlockYSize = (nNewBlockYSize / nBlockYSize) * nBlockYSize;
        if (nBlockYSize > nYSize)
            nBlockYSize = nYSize;
    }
    CPLDebug("GDAL_GRID", "Work buffer: %d * %d", nBlockXSize, nBlockYSize);

    std::unique_ptr<void, VSIFreeReleaser> pData(
        VSIMalloc3(nBlockXSize, nBlockYSize, nDataTypeSize));
    if (!pData)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate work buffer");
        return CE_Failure;
    }

    GIntBig nBlock = 0;
    const double dfBlockCount =
        static_cast<double>(DIV_ROUND_UP(nXSize, nBlockXSize)) *
        DIV_ROUND_UP(nYSize, nBlockYSize);

    struct GDALGridContextReleaser
    {
        void operator()(GDALGridContext *psContext)
        {
            GDALGridContextFree(psContext);
        }
    };

    std::unique_ptr<GDALGridContext, GDALGridContextReleaser> psContext(
        GDALGridContextCreate(eAlgorithm, pOptions,
                              static_cast<int>(oVisitor.adfX.size()),
                              &(oVisitor.adfX[0]), &(oVisitor.adfY[0]),
                              &(oVisitor.adfZ[0]), TRUE));
    if (!psContext)
    {
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    for (int nYOffset = 0; nYOffset < nYSize && eErr == CE_None;
         nYOffset += nBlockYSize)
    {
        for (int nXOffset = 0; nXOffset < nXSize && eErr == CE_None;
             nXOffset += nBlockXSize)
        {
            std::unique_ptr<void, GDALScaledProgressReleaser> pScaledProgress(
                GDALCreateScaledProgress(
                    static_cast<double>(nBlock) / dfBlockCount,
                    static_cast<double>(nBlock + 1) / dfBlockCount, pfnProgress,
                    pProgressData));
            nBlock++;

            int nXRequest = nBlockXSize;
            if (nXOffset > nXSize - nXRequest)
                nXRequest = nXSize - nXOffset;

            int nYRequest = nBlockYSize;
            if (nYOffset > nYSize - nYRequest)
                nYRequest = nYSize - nYOffset;

            eErr = GDALGridContextProcess(
                psContext.get(), dfXMin + dfDeltaX * nXOffset,
                dfXMin + dfDeltaX * (nXOffset + nXRequest),
                dfYMin + dfDeltaY * nYOffset,
                dfYMin + dfDeltaY * (nYOffset + nYRequest), nXRequest,
                nYRequest, eType, pData.get(), GDALScaledProgress,
                pScaledProgress.get());

            if (eErr == CE_None)
                eErr = poBand->RasterIO(GF_Write, nXOffset, nYOffset, nXRequest,
                                        nYRequest, pData.get(), nXRequest,
                                        nYRequest, eType, 0, 0, nullptr);
        }
    }
    if (eErr == CE_None && pfnProgress)
        pfnProgress(1.0, "", pProgressData);

    return eErr;
}

/************************************************************************/
/*                            LoadGeometry()                            */
/*                                                                      */
/*  Read geometries from the given dataset using specified filters and  */
/*  returns a collection of read geometries.                            */
/************************************************************************/

static std::unique_ptr<OGRGeometry> LoadGeometry(const std::string &osDS,
                                                 const std::string &osSQL,
                                                 const std::string &osLyr,
                                                 const std::string &osWhere)
{
    auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
        osDS.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    if (!poDS)
        return nullptr;

    OGRLayer *poLyr = nullptr;
    if (!osSQL.empty())
        poLyr = poDS->ExecuteSQL(osSQL.c_str(), nullptr, nullptr);
    else if (!osLyr.empty())
        poLyr = poDS->GetLayerByName(osLyr.c_str());
    else
        poLyr = poDS->GetLayer(0);

    if (poLyr == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to identify source layer from datasource.");
        return nullptr;
    }

    if (!osWhere.empty())
        poLyr->SetAttributeFilter(osWhere.c_str());

    std::unique_ptr<OGRGeometryCollection> poGeom;
    for (auto &poFeat : poLyr)
    {
        const OGRGeometry *poSrcGeom = poFeat->GetGeometryRef();
        if (poSrcGeom)
        {
            const OGRwkbGeometryType eType =
                wkbFlatten(poSrcGeom->getGeometryType());

            if (!poGeom)
                poGeom = std::make_unique<OGRMultiPolygon>();

            if (eType == wkbPolygon)
            {
                poGeom->addGeometry(poSrcGeom);
            }
            else if (eType == wkbMultiPolygon)
            {
                const int nGeomCount =
                    poSrcGeom->toMultiPolygon()->getNumGeometries();

                for (int iGeom = 0; iGeom < nGeomCount; iGeom++)
                {
                    poGeom->addGeometry(
                        poSrcGeom->toMultiPolygon()->getGeometryRef(iGeom));
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Geometry not of polygon type.");
                if (!osSQL.empty())
                    poDS->ReleaseResultSet(poLyr);
                return nullptr;
            }
        }
    }

    if (!osSQL.empty())
        poDS->ReleaseResultSet(poLyr);

    return poGeom;
}

/************************************************************************/
/*                               GDALGrid()                             */
/************************************************************************/

/* clang-format off */
/**
 * Create raster from the scattered data.
 *
 * This is the equivalent of the
 * <a href="/programs/gdal_grid.html">gdal_grid</a> utility.
 *
 * GDALGridOptions* must be allocated and freed with GDALGridOptionsNew()
 * and GDALGridOptionsFree() respectively.
 *
 * @param pszDest the destination dataset path.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALGridOptionsNew() or
 * NULL.
 * @param pbUsageError pointer to a integer output variable to store if any
 * usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose()) or NULL in case of error.
 *
 * @since GDAL 2.1
 */
/* clang-format on */

GDALDatasetH GDALGrid(const char *pszDest, GDALDatasetH hSrcDataset,
                      const GDALGridOptions *psOptionsIn, int *pbUsageError)

{
    if (hSrcDataset == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No source dataset specified.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (pszDest == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    std::unique_ptr<GDALGridOptions> psOptionsToFree;
    const GDALGridOptions *psOptions = psOptionsIn;
    if (psOptions == nullptr)
    {
        psOptionsToFree = std::make_unique<GDALGridOptions>();
        psOptions = psOptionsToFree.get();
    }

    GDALDataset *poSrcDS = GDALDataset::FromHandle(hSrcDataset);

    if (psOptions->osSQL.empty() && psOptions->aosLayers.empty() &&
        poSrcDS->GetLayerCount() != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Neither -sql nor -l are specified, but the source dataset "
                 "has not one single layer.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if ((psOptions->nXSize != 0 || psOptions->nYSize != 0) &&
        (psOptions->dfXRes != 0 || psOptions->dfYRes != 0))
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "-outsize and -tr options cannot be used at the same time.");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Find the output driver.                                         */
    /* -------------------------------------------------------------------- */
    std::string osFormat;
    if (psOptions->osFormat.empty())
    {
        osFormat = GetOutputDriverForRaster(pszDest);
        if (osFormat.empty())
        {
            return nullptr;
        }
    }
    else
    {
        osFormat = psOptions->osFormat;
    }

    GDALDriverH hDriver = GDALGetDriverByName(osFormat.c_str());
    if (hDriver == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Output driver `%s' not recognised.", osFormat.c_str());
        fprintf(stderr, "The following format drivers are configured and "
                        "support output:\n");
        for (int iDr = 0; iDr < GDALGetDriverCount(); iDr++)
        {
            hDriver = GDALGetDriver(iDr);

            if (GDALGetMetadataItem(hDriver, GDAL_DCAP_RASTER, nullptr) !=
                    nullptr &&
                (GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) !=
                     nullptr ||
                 GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY, nullptr) !=
                     nullptr))
            {
                fprintf(stderr, "  %s: %s\n", GDALGetDriverShortName(hDriver),
                        GDALGetDriverLongName(hDriver));
            }
        }
        printf("\n");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create target raster file.                                      */
    /* -------------------------------------------------------------------- */
    int nLayerCount = psOptions->aosLayers.size();
    if (nLayerCount == 0 && psOptions->osSQL.empty())
        nLayerCount = 1; /* due to above check */

    int nBands = nLayerCount;

    if (!psOptions->osSQL.empty())
        nBands++;

    int nXSize;
    int nYSize;
    if (psOptions->dfXRes != 0 && psOptions->dfYRes != 0)
    {
        if ((psOptions->dfXMax == psOptions->dfXMin) ||
            (psOptions->dfYMax == psOptions->dfYMin))
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Invalid txe or tye parameters detected. Please check "
                     "your -txe or -tye argument.");

            if (pbUsageError)
                *pbUsageError = TRUE;
            return nullptr;
        }

        double dfXSize = (std::fabs(psOptions->dfXMax - psOptions->dfXMin) +
                          (psOptions->dfXRes / 2.0)) /
                         psOptions->dfXRes;
        double dfYSize = (std::fabs(psOptions->dfYMax - psOptions->dfYMin) +
                          (psOptions->dfYRes / 2.0)) /
                         psOptions->dfYRes;

        if (dfXSize >= 1 && dfXSize <= INT_MAX && dfYSize >= 1 &&
            dfYSize <= INT_MAX)
        {
            nXSize = static_cast<int>(dfXSize);
            nYSize = static_cast<int>(dfYSize);
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_IllegalArg,
                "Invalid output size detected. Please check your -tr argument");

            if (pbUsageError)
                *pbUsageError = TRUE;
            return nullptr;
        }
    }
    else
    {
        // FIXME
        nXSize = psOptions->nXSize;
        if (nXSize == 0)
            nXSize = 256;
        nYSize = psOptions->nYSize;
        if (nYSize == 0)
            nYSize = 256;
    }

    std::unique_ptr<GDALDataset> poDstDS(GDALDataset::FromHandle(GDALCreate(
        hDriver, pszDest, nXSize, nYSize, nBands, psOptions->eOutputType,
        psOptions->aosCreateOptions.List())));
    if (!poDstDS)
    {
        return nullptr;
    }

    if (psOptions->bNoDataSet)
    {
        for (int i = 1; i <= nBands; i++)
        {
            poDstDS->GetRasterBand(i)->SetNoDataValue(psOptions->dfNoDataValue);
        }
    }

    double dfXMin = psOptions->dfXMin;
    double dfYMin = psOptions->dfYMin;
    double dfXMax = psOptions->dfXMax;
    double dfYMax = psOptions->dfYMax;
    bool bIsXExtentSet = psOptions->bIsXExtentSet;
    bool bIsYExtentSet = psOptions->bIsYExtentSet;
    CPLErr eErr = CE_None;

    /* -------------------------------------------------------------------- */
    /*      Process SQL request.                                            */
    /* -------------------------------------------------------------------- */

    if (!psOptions->osSQL.empty())
    {
        OGRLayer *poLayer =
            poSrcDS->ExecuteSQL(psOptions->osSQL.c_str(),
                                psOptions->poSpatialFilter.get(), nullptr);
        if (poLayer == nullptr)
        {
            return nullptr;
        }

        // Custom layer will be rasterized in the first band.
        eErr = ProcessLayer(
            poLayer, poDstDS.get(), psOptions->poSpatialFilter.get(), nXSize,
            nYSize, 1, bIsXExtentSet, bIsYExtentSet, dfXMin, dfXMax, dfYMin,
            dfYMax, psOptions->osBurnAttribute, psOptions->dfIncreaseBurnValue,
            psOptions->dfMultiplyBurnValue, psOptions->eOutputType,
            psOptions->eAlgorithm, psOptions->pOptions.get(), psOptions->bQuiet,
            psOptions->pfnProgress, psOptions->pProgressData);

        poSrcDS->ReleaseResultSet(poLayer);
    }

    /* -------------------------------------------------------------------- */
    /*      Process each layer.                                             */
    /* -------------------------------------------------------------------- */
    std::string osOutputSRS(psOptions->osOutputSRS);
    for (int i = 0; i < nLayerCount; i++)
    {
        auto poLayer = psOptions->aosLayers.empty()
                           ? poSrcDS->GetLayer(0)
                           : poSrcDS->GetLayerByName(psOptions->aosLayers[i]);
        if (!poLayer)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to find layer \"%s\".",
                     !psOptions->aosLayers.empty() && psOptions->aosLayers[i]
                         ? psOptions->aosLayers[i]
                         : "null");
            eErr = CE_Failure;
            break;
        }

        if (!psOptions->osWHERE.empty())
        {
            if (poLayer->SetAttributeFilter(psOptions->osWHERE.c_str()) !=
                OGRERR_NONE)
            {
                eErr = CE_Failure;
                break;
            }
        }

        if (psOptions->poSpatialFilter)
            poLayer->SetSpatialFilter(psOptions->poSpatialFilter.get());

        // Fetch the first meaningful SRS definition
        if (osOutputSRS.empty())
        {
            auto poSRS = poLayer->GetSpatialRef();
            if (poSRS)
                osOutputSRS = poSRS->exportToWkt();
        }

        eErr = ProcessLayer(
            poLayer, poDstDS.get(), psOptions->poSpatialFilter.get(), nXSize,
            nYSize, i + 1 + nBands - nLayerCount, bIsXExtentSet, bIsYExtentSet,
            dfXMin, dfXMax, dfYMin, dfYMax, psOptions->osBurnAttribute,
            psOptions->dfIncreaseBurnValue, psOptions->dfMultiplyBurnValue,
            psOptions->eOutputType, psOptions->eAlgorithm,
            psOptions->pOptions.get(), psOptions->bQuiet,
            psOptions->pfnProgress, psOptions->pProgressData);
        if (eErr != CE_None)
            break;
    }

    /* -------------------------------------------------------------------- */
    /*      Apply geotransformation matrix.                                 */
    /* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = {dfXMin, (dfXMax - dfXMin) / nXSize,
                                 0.0,    dfYMin,
                                 0.0,    (dfYMax - dfYMin) / nYSize};
    poDstDS->SetGeoTransform(adfGeoTransform);

    /* -------------------------------------------------------------------- */
    /*      Apply SRS definition if set.                                    */
    /* -------------------------------------------------------------------- */
    if (!osOutputSRS.empty())
    {
        poDstDS->SetProjection(osOutputSRS.c_str());
    }

    /* -------------------------------------------------------------------- */
    /*      End                                                             */
    /* -------------------------------------------------------------------- */

    if (eErr != CE_None)
    {
        return nullptr;
    }

    return GDALDataset::ToHandle(poDstDS.release());
}

/************************************************************************/
/*                       GDALGridOptionsGetParser()                     */
/************************************************************************/

/*! @cond Doxygen_Suppress */

static std::unique_ptr<GDALArgumentParser>
GDALGridOptionsGetParser(GDALGridOptions *psOptions,
                         GDALGridOptionsForBinary *psOptionsForBinary,
                         int nCountClipSrc)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdal_grid", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(
        _("Creates a regular grid (raster) from the scattered data read from a "
          "vector datasource."));

    argParser->add_epilog(_(
        "Available algorithms and parameters with their defaults:\n"
        "    Inverse distance to a power (default)\n"
        "        "
        "invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_"
        "points=0:min_points=0:nodata=0.0\n"
        "    Inverse distance to a power with nearest neighbor search\n"
        "        "
        "invdistnn:power=2.0:radius=1.0:max_points=12:min_points=0:nodata=0\n"
        "    Moving average\n"
        "        "
        "average:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0\n"
        "    Nearest neighbor\n"
        "        nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0\n"
        "    Various data metrics\n"
        "        <metric "
        "name>:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0\n"
        "        possible metrics are:\n"
        "            minimum\n"
        "            maximum\n"
        "            range\n"
        "            count\n"
        "            average_distance\n"
        "            average_distance_pts\n"
        "    Linear\n"
        "        linear:radius=-1.0:nodata=0.0\n"
        "\n"
        "For more details, consult https://gdal.org/programs/gdal_grid.html"));

    argParser->add_quiet_argument(
        psOptionsForBinary ? &psOptionsForBinary->bQuiet : nullptr);

    argParser->add_output_format_argument(psOptions->osFormat);

    argParser->add_output_type_argument(psOptions->eOutputType);

    argParser->add_argument("-txe")
        .metavar("<xmin> <xmax>")
        .nargs(2)
        .scan<'g', double>()
        .help(_("Set georeferenced X extents of output file to be created."));

    argParser->add_argument("-tye")
        .metavar("<ymin> <ymax>")
        .nargs(2)
        .scan<'g', double>()
        .help(_("Set georeferenced Y extents of output file to be created."));

    argParser->add_argument("-outsize")
        .metavar("<xsize> <ysize>")
        .nargs(2)
        .scan<'i', int>()
        .help(_("Set the size of the output file."));

    argParser->add_argument("-tr")
        .metavar("<xres> <yes>")
        .nargs(2)
        .scan<'g', double>()
        .help(_("Set target resolution."));

    argParser->add_creation_options_argument(psOptions->aosCreateOptions);

    argParser->add_argument("-zfield")
        .metavar("<field_name>")
        .store_into(psOptions->osBurnAttribute)
        .help(_("Field name from which to get Z values."));

    argParser->add_argument("-z_increase")
        .metavar("<increase_value>")
        .store_into(psOptions->dfIncreaseBurnValue)
        .help(_("Addition to the attribute field on the features to be used to "
                "get a Z value from."));

    argParser->add_argument("-z_multiply")
        .metavar("<multiply_value>")
        .store_into(psOptions->dfMultiplyBurnValue)
        .help(_("Multiplication ratio for the Z field.."));

    argParser->add_argument("-where")
        .metavar("<expression>")
        .store_into(psOptions->osWHERE)
        .help(_("Query expression to be applied to select features to process "
                "from the input layer(s)."));

    argParser->add_argument("-l")
        .metavar("<layer_name>")
        .append()
        .action([psOptions](const std::string &s)
                { psOptions->aosLayers.AddString(s.c_str()); })
        .help(_("Layer(s) from the datasource that will be used for input "
                "features."));

    argParser->add_argument("-sql")
        .metavar("<select_statement>")
        .store_into(psOptions->osSQL)
        .help(_("SQL statement to be evaluated to produce a layer of features "
                "to be processed."));

    argParser->add_argument("-spat")
        .metavar("<xmin> <ymin> <xmax> <ymax>")
        .nargs(4)
        .scan<'g', double>()
        .help(_("The area of interest. Only features within the rectangle will "
                "be reported."));

    argParser->add_argument("-clipsrc")
        .nargs(nCountClipSrc)
        .metavar("[<xmin> <ymin> <xmax> <ymax>]|<WKT>|<datasource>|spat_extent")
        .help(_("Clip geometries (in source SRS)."));

    argParser->add_argument("-clipsrcsql")
        .metavar("<sql_statement>")
        .store_into(psOptions->osClipSrcSQL)
        .help(_("Select desired geometries from the source clip datasource "
                "using an SQL query."));

    argParser->add_argument("-clipsrclayer")
        .metavar("<layername>")
        .store_into(psOptions->osClipSrcLayer)
        .help(_("Select the named layer from the source clip datasource."));

    argParser->add_argument("-clipsrcwhere")
        .metavar("<expression>")
        .store_into(psOptions->osClipSrcWhere)
        .help(_("Restrict desired geometries from the source clip layer based "
                "on an attribute query."));

    argParser->add_argument("-a_srs")
        .metavar("<srs_def>")
        .action(
            [psOptions](const std::string &osOutputSRSDef)
            {
                OGRSpatialReference oOutputSRS;

                if (oOutputSRS.SetFromUserInput(osOutputSRSDef.c_str()) !=
                    OGRERR_NONE)
                {
                    throw std::invalid_argument(
                        std::string("Failed to process SRS definition: ")
                            .append(osOutputSRSDef));
                }

                char *pszWKT = nullptr;
                oOutputSRS.exportToWkt(&pszWKT);
                if (pszWKT)
                    psOptions->osOutputSRS = pszWKT;
                CPLFree(pszWKT);
            })
        .help(_("Assign an output SRS, but without reprojecting."));

    argParser->add_argument("-a")
        .metavar("<algorithm>[[:<parameter1>=<value1>]...]")
        .action(
            [psOptions](const std::string &s)
            {
                const char *pszAlgorithm = s.c_str();
                void *pOptions = nullptr;
                if (GDALGridParseAlgorithmAndOptions(pszAlgorithm,
                                                     &psOptions->eAlgorithm,
                                                     &pOptions) != CE_None)
                {
                    throw std::invalid_argument(
                        "Failed to process algorithm name and parameters");
                }
                psOptions->pOptions.reset(pOptions);

                const CPLStringList aosParams(
                    CSLTokenizeString2(pszAlgorithm, ":", FALSE));
                const char *pszNoDataValue = aosParams.FetchNameValue("nodata");
                if (pszNoDataValue != nullptr)
                {
                    psOptions->bNoDataSet = true;
                    psOptions->dfNoDataValue = CPLAtofM(pszNoDataValue);
                }
            })
        .help(_("Set the interpolation algorithm or data metric name and "
                "(optionally) its parameters."));

    if (psOptionsForBinary)
    {
        argParser->add_open_options_argument(
            &(psOptionsForBinary->aosOpenOptions));
    }

    if (psOptionsForBinary)
    {
        argParser->add_argument("src_dataset_name")
            .metavar("<src_dataset_name>")
            .store_into(psOptionsForBinary->osSource)
            .help(_("Input dataset."));

        argParser->add_argument("dst_dataset_name")
            .metavar("<dst_dataset_name>")
            .store_into(psOptionsForBinary->osDest)
            .help(_("Output dataset."));
    }

    return argParser;
}

/*! @endcond */

/************************************************************************/
/*                         GDALGridGetParserUsage()                     */
/************************************************************************/

std::string GDALGridGetParserUsage()
{
    try
    {
        GDALGridOptions sOptions;
        GDALGridOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALGridOptionsGetParser(&sOptions, &sOptionsForBinary, 1);
        return argParser->usage();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return std::string();
    }
}

/************************************************************************/
/*                   CHECK_HAS_ENOUGH_ADDITIONAL_ARGS()                 */
/************************************************************************/

#ifndef CheckHasEnoughAdditionalArgs_defined
#define CheckHasEnoughAdditionalArgs_defined

static bool CheckHasEnoughAdditionalArgs(CSLConstList papszArgv, int i,
                                         int nExtraArg, int nArgc)
{
    if (i + nExtraArg >= nArgc)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "%s option requires %d argument%s", papszArgv[i], nExtraArg,
                 nExtraArg == 1 ? "" : "s");
        return false;
    }
    return true;
}
#endif

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg)                            \
    if (!CheckHasEnoughAdditionalArgs(papszArgv, i, nExtraArg, nArgc))         \
    {                                                                          \
        return nullptr;                                                        \
    }

/************************************************************************/
/*                             GDALGridOptionsNew()                     */
/************************************************************************/

/**
 * Allocates a GDALGridOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdal_translate.html">gdal_translate</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALGridOptionsForBinaryNew() prior to this
 * function. Will be filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALGridOptions struct. Must be freed with
 * GDALGridOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALGridOptions *
GDALGridOptionsNew(char **papszArgv,
                   GDALGridOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALGridOptions>();

    /* -------------------------------------------------------------------- */
    /*      Pre-processing for custom syntax that ArgumentParser does not   */
    /*      support.                                                        */
    /* -------------------------------------------------------------------- */

    CPLStringList aosArgv;
    const int nArgc = CSLCount(papszArgv);
    int nCountClipSrc = 0;
    for (int i = 0;
         i < nArgc && papszArgv != nullptr && papszArgv[i] != nullptr; i++)
    {
        if (EQUAL(papszArgv[i], "-clipsrc"))
        {
            if (nCountClipSrc)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Duplicate argument %s",
                         papszArgv[i]);
                return nullptr;
            }
            // argparse doesn't handle well variable number of values
            // just before the positional arguments, so we have to detect
            // it manually and set the correct number.
            nCountClipSrc = 1;
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            if (CPLGetValueType(papszArgv[i + 1]) != CPL_VALUE_STRING &&
                i + 4 < nArgc)
            {
                nCountClipSrc = 4;
            }

            for (int j = 0; j < 1 + nCountClipSrc; ++j)
            {
                aosArgv.AddString(papszArgv[i]);
                ++i;
            }
            --i;
        }

        else
        {
            aosArgv.AddString(papszArgv[i]);
        }
    }

    try
    {
        auto argParser = GDALGridOptionsGetParser(
            psOptions.get(), psOptionsForBinary, nCountClipSrc);

        argParser->parse_args_without_binary_name(aosArgv.List());

        if (auto oTXE = argParser->present<std::vector<double>>("-txe"))
        {
            psOptions->dfXMin = (*oTXE)[0];
            psOptions->dfXMax = (*oTXE)[1];
            psOptions->bIsXExtentSet = true;
        }

        if (auto oTYE = argParser->present<std::vector<double>>("-tye"))
        {
            psOptions->dfYMin = (*oTYE)[0];
            psOptions->dfYMax = (*oTYE)[1];
            psOptions->bIsYExtentSet = true;
        }

        if (auto oOutsize = argParser->present<std::vector<int>>("-outsize"))
        {
            psOptions->nXSize = (*oOutsize)[0];
            psOptions->nYSize = (*oOutsize)[1];
        }

        if (auto adfTargetRes = argParser->present<std::vector<double>>("-tr"))
        {
            psOptions->dfXRes = (*adfTargetRes)[0];
            psOptions->dfYRes = (*adfTargetRes)[1];
            if (psOptions->dfXRes <= 0 || psOptions->dfYRes <= 0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Wrong value for -tr parameters.");
                return nullptr;
            }
        }

        if (auto oSpat = argParser->present<std::vector<double>>("-spat"))
        {
            OGRLinearRing oRing;
            const double dfMinX = (*oSpat)[0];
            const double dfMinY = (*oSpat)[1];
            const double dfMaxX = (*oSpat)[2];
            const double dfMaxY = (*oSpat)[3];

            oRing.addPoint(dfMinX, dfMinY);
            oRing.addPoint(dfMinX, dfMaxY);
            oRing.addPoint(dfMaxX, dfMaxY);
            oRing.addPoint(dfMaxX, dfMinY);
            oRing.addPoint(dfMinX, dfMinY);

            auto poPolygon = std::make_unique<OGRPolygon>();
            poPolygon->addRing(&oRing);
            psOptions->poSpatialFilter = std::move(poPolygon);
        }

        if (auto oClipSrc =
                argParser->present<std::vector<std::string>>("-clipsrc"))
        {
            const std::string &osVal = (*oClipSrc)[0];

            psOptions->poClipSrc.reset();
            psOptions->osClipSrcDS.clear();

            VSIStatBufL sStat;
            psOptions->bClipSrc = true;
            if (oClipSrc->size() == 4)
            {
                const double dfMinX = CPLAtofM((*oClipSrc)[0].c_str());
                const double dfMinY = CPLAtofM((*oClipSrc)[1].c_str());
                const double dfMaxX = CPLAtofM((*oClipSrc)[2].c_str());
                const double dfMaxY = CPLAtofM((*oClipSrc)[3].c_str());

                OGRLinearRing oRing;

                oRing.addPoint(dfMinX, dfMinY);
                oRing.addPoint(dfMinX, dfMaxY);
                oRing.addPoint(dfMaxX, dfMaxY);
                oRing.addPoint(dfMaxX, dfMinY);
                oRing.addPoint(dfMinX, dfMinY);

                auto poPoly = std::make_unique<OGRPolygon>();
                poPoly->addRing(&oRing);
                psOptions->poClipSrc = std::move(poPoly);
            }
            else if ((STARTS_WITH_CI(osVal.c_str(), "POLYGON") ||
                      STARTS_WITH_CI(osVal.c_str(), "MULTIPOLYGON")) &&
                     VSIStatL(osVal.c_str(), &sStat) != 0)
            {
                OGRGeometry *poGeom = nullptr;
                OGRGeometryFactory::createFromWkt(osVal.c_str(), nullptr,
                                                  &poGeom);
                psOptions->poClipSrc.reset(poGeom);
                if (psOptions->poClipSrc == nullptr)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or "
                             "MULTIPOLYGON WKT");
                    return nullptr;
                }
            }
            else if (EQUAL(osVal.c_str(), "spat_extent"))
            {
                // Nothing to do
            }
            else
            {
                psOptions->osClipSrcDS = osVal;
            }
        }

        if (psOptions->bClipSrc && !psOptions->osClipSrcDS.empty())
        {
            psOptions->poClipSrc = LoadGeometry(
                psOptions->osClipSrcDS, psOptions->osClipSrcSQL,
                psOptions->osClipSrcLayer, psOptions->osClipSrcWhere);
            if (!psOptions->poClipSrc)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot load source clip geometry.");
                return nullptr;
            }
        }
        else if (psOptions->bClipSrc && !psOptions->poClipSrc &&
                 !psOptions->poSpatialFilter)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "-clipsrc must be used with -spat option or \n"
                     "a bounding box, WKT string or datasource must be "
                     "specified.");
            return nullptr;
        }

        if (psOptions->poSpatialFilter)
        {
            if (psOptions->poClipSrc)
            {
                auto poTemp = std::unique_ptr<OGRGeometry>(
                    psOptions->poSpatialFilter->Intersection(
                        psOptions->poClipSrc.get()));
                if (poTemp)
                {
                    psOptions->poSpatialFilter = std::move(poTemp);
                }

                psOptions->poClipSrc.reset();
            }
        }
        else
        {
            if (psOptions->poClipSrc)
            {
                psOptions->poSpatialFilter = std::move(psOptions->poClipSrc);
            }
        }

        return psOptions.release();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", err.what());
        return nullptr;
    }
}

/************************************************************************/
/*                          GDALGridOptionsFree()                       */
/************************************************************************/

/**
 * Frees the GDALGridOptions struct.
 *
 * @param psOptions the options struct for GDALGrid().
 *
 * @since GDAL 2.1
 */

void GDALGridOptionsFree(GDALGridOptions *psOptions)
{
    delete psOptions;
}

/************************************************************************/
/*                     GDALGridOptionsSetProgress()                     */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALGrid().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALGridOptionsSetProgress(GDALGridOptions *psOptions,
                                GDALProgressFunc pfnProgress,
                                void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
    if (pfnProgress == GDALTermProgress)
        psOptions->bQuiet = false;
}

#undef CHECK_HAS_ENOUGH_ADDITIONAL_ARGS
