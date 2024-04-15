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
/*                            IsNumber()                               */
/************************************************************************/

static bool IsNumber(const char *pszStr)
{
    if (*pszStr == '-' || *pszStr == '+')
        pszStr++;
    if (*pszStr == '.')
        pszStr++;
    return *pszStr >= '0' && *pszStr <= '9';
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

    bool bGotSourceFilename = false;
    bool bGotDestFilename = false;
    /* -------------------------------------------------------------------- */
    /*      Handle command line arguments.                                  */
    /* -------------------------------------------------------------------- */
    const int argc = CSLCount(papszArgv);
    for (int i = 0; i < argc && papszArgv != nullptr && papszArgv[i] != nullptr;
         i++)
    {
        if (i < argc - 1 &&
            (EQUAL(papszArgv[i], "-of") || EQUAL(papszArgv[i], "-f")))
        {
            ++i;
            psOptions->osFormat = papszArgv[i];
        }

        else if (EQUAL(papszArgv[i], "-q") || EQUAL(papszArgv[i], "-quiet"))
        {
            if (psOptionsForBinary)
            {
                psOptionsForBinary->bQuiet = true;
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "%s switch only supported from gdal_grid binary.",
                         papszArgv[i]);
            }
        }

        else if (EQUAL(papszArgv[i], "-ot") && papszArgv[i + 1])
        {
            for (int iType = 1; iType < GDT_TypeCount; iType++)
            {
                if (GDALGetDataTypeName(static_cast<GDALDataType>(iType)) !=
                        nullptr &&
                    EQUAL(GDALGetDataTypeName(static_cast<GDALDataType>(iType)),
                          papszArgv[i + 1]))
                {
                    psOptions->eOutputType = static_cast<GDALDataType>(iType);
                }
            }

            if (psOptions->eOutputType == GDT_Unknown)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unknown output pixel type: %s.", papszArgv[i + 1]);
                return nullptr;
            }
            i++;
        }

        else if (i + 2 < argc && EQUAL(papszArgv[i], "-txe"))
        {
            psOptions->dfXMin = CPLAtof(papszArgv[++i]);
            psOptions->dfXMax = CPLAtof(papszArgv[++i]);
            psOptions->bIsXExtentSet = true;
        }

        else if (i + 2 < argc && EQUAL(papszArgv[i], "-tye"))
        {
            psOptions->dfYMin = CPLAtof(papszArgv[++i]);
            psOptions->dfYMax = CPLAtof(papszArgv[++i]);
            psOptions->bIsYExtentSet = true;
        }

        else if (i + 2 < argc && EQUAL(papszArgv[i], "-outsize"))
        {
            CPLAssert(papszArgv[i + 1]);
            CPLAssert(papszArgv[i + 2]);
            psOptions->nXSize = atoi(papszArgv[i + 1]);
            psOptions->nYSize = atoi(papszArgv[i + 2]);
            i += 2;
        }

        else if (i + 2 < argc && EQUAL(papszArgv[i], "-tr"))
        {
            psOptions->dfXRes = CPLAtofM(papszArgv[++i]);
            psOptions->dfYRes = CPLAtofM(papszArgv[++i]);
            if (psOptions->dfXRes <= 0 || psOptions->dfYRes <= 0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Wrong value for -tr parameters.");
                return nullptr;
            }
        }

        else if (i + 1 < argc && EQUAL(papszArgv[i], "-co"))
        {
            psOptions->aosCreateOptions.AddString(papszArgv[++i]);
        }

        else if (i + 1 < argc && EQUAL(papszArgv[i], "-zfield"))
        {
            psOptions->osBurnAttribute = papszArgv[++i];
        }

        else if (i + 1 < argc && EQUAL(papszArgv[i], "-z_increase"))
        {
            psOptions->dfIncreaseBurnValue = CPLAtof(papszArgv[++i]);
        }

        else if (i + 1 < argc && EQUAL(papszArgv[i], "-z_multiply"))
        {
            psOptions->dfMultiplyBurnValue = CPLAtof(papszArgv[++i]);
        }

        else if (i + 1 < argc && EQUAL(papszArgv[i], "-where"))
        {
            psOptions->osWHERE = papszArgv[++i];
        }

        else if (i + 1 < argc && EQUAL(papszArgv[i], "-l"))
        {
            psOptions->aosLayers.AddString(papszArgv[++i]);
        }

        else if (i + 1 < argc && EQUAL(papszArgv[i], "-sql"))
        {
            psOptions->osSQL = papszArgv[++i];
        }

        else if (i + 4 < argc && EQUAL(papszArgv[i], "-spat"))
        {
            OGRLinearRing oRing;

            oRing.addPoint(CPLAtof(papszArgv[i + 1]),
                           CPLAtof(papszArgv[i + 2]));
            oRing.addPoint(CPLAtof(papszArgv[i + 1]),
                           CPLAtof(papszArgv[i + 4]));
            oRing.addPoint(CPLAtof(papszArgv[i + 3]),
                           CPLAtof(papszArgv[i + 4]));
            oRing.addPoint(CPLAtof(papszArgv[i + 3]),
                           CPLAtof(papszArgv[i + 2]));
            oRing.addPoint(CPLAtof(papszArgv[i + 1]),
                           CPLAtof(papszArgv[i + 2]));

            auto poPoly = std::make_unique<OGRPolygon>();
            poPoly->addRing(&oRing);
            psOptions->poSpatialFilter = std::move(poPoly);
            i += 4;
        }

        else if (EQUAL(papszArgv[i], "-clipsrc"))
        {
            if (i + 1 >= argc || papszArgv[i + 1] == nullptr)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "%s option requires 1 or 4 arguments", papszArgv[i]);
                return nullptr;
            }

            VSIStatBufL sStat;
            psOptions->bClipSrc = true;
            if (IsNumber(papszArgv[i + 1]) && papszArgv[i + 2] != nullptr &&
                papszArgv[i + 3] != nullptr && papszArgv[i + 4] != nullptr)
            {
                OGRLinearRing oRing;

                oRing.addPoint(CPLAtof(papszArgv[i + 1]),
                               CPLAtof(papszArgv[i + 2]));
                oRing.addPoint(CPLAtof(papszArgv[i + 1]),
                               CPLAtof(papszArgv[i + 4]));
                oRing.addPoint(CPLAtof(papszArgv[i + 3]),
                               CPLAtof(papszArgv[i + 4]));
                oRing.addPoint(CPLAtof(papszArgv[i + 3]),
                               CPLAtof(papszArgv[i + 2]));
                oRing.addPoint(CPLAtof(papszArgv[i + 1]),
                               CPLAtof(papszArgv[i + 2]));

                auto poPoly = std::make_unique<OGRPolygon>();
                poPoly->addRing(&oRing);
                psOptions->poClipSrc = std::move(poPoly);
                i += 4;
            }
            else if ((STARTS_WITH_CI(papszArgv[i + 1], "POLYGON") ||
                      STARTS_WITH_CI(papszArgv[i + 1], "MULTIPOLYGON")) &&
                     VSIStatL(papszArgv[i + 1], &sStat) != 0)
            {
                OGRGeometry *poClipSrc = nullptr;
                OGRGeometryFactory::createFromWkt(papszArgv[i + 1], nullptr,
                                                  &poClipSrc);
                if (!poClipSrc)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or "
                             "MULTIPOLYGON WKT");
                    return nullptr;
                }
                psOptions->poClipSrc.reset(poClipSrc);
                i++;
            }
            else if (EQUAL(papszArgv[i + 1], "spat_extent"))
            {
                i++;
            }
            else
            {
                psOptions->osClipSrcDS = papszArgv[i + 1];
                i++;
            }
        }
        else if (i + 1 < argc && EQUAL(papszArgv[i], "-clipsrcsql"))
        {
            psOptions->osClipSrcSQL = papszArgv[i + 1];
            i++;
        }
        else if (i + 1 < argc && EQUAL(papszArgv[i], "-clipsrclayer"))
        {
            psOptions->osClipSrcLayer = papszArgv[i + 1];
            i++;
        }
        else if (i + 1 < argc && EQUAL(papszArgv[i], "-clipsrcwhere"))
        {
            psOptions->osClipSrcWhere = papszArgv[i + 1];
            i++;
        }

        else if (i + 1 < argc && EQUAL(papszArgv[i], "-a_srs"))
        {
            OGRSpatialReference oOutputSRS;

            if (oOutputSRS.SetFromUserInput(papszArgv[i + 1]) != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to process SRS definition: %s",
                         papszArgv[i + 1]);
                return nullptr;
            }

            char *pszWKT = nullptr;
            oOutputSRS.exportToWkt(&pszWKT);
            if (pszWKT)
                psOptions->osOutputSRS = pszWKT;
            CPLFree(pszWKT);
            i++;
        }

        else if (i + 1 < argc && EQUAL(papszArgv[i], "-a"))
        {
            const char *pszAlgorithm = papszArgv[++i];
            void *pOptions = nullptr;
            if (GDALGridParseAlgorithmAndOptions(
                    pszAlgorithm, &psOptions->eAlgorithm, &pOptions) != CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to process algorithm name and parameters");
                return nullptr;
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
        }
        else if (i + 1 < argc && EQUAL(papszArgv[i], "-oo"))
        {
            i++;
            if (psOptionsForBinary)
            {
                psOptionsForBinary->aosOpenOptions.AddString(papszArgv[i]);
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "-oo switch only supported from gdal_grid binary.");
            }
        }
        else if (papszArgv[i][0] == '-')
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Unknown option name '%s'",
                     papszArgv[i]);
            return nullptr;
        }
        else if (!bGotSourceFilename)
        {
            bGotSourceFilename = true;
            if (psOptionsForBinary)
            {
                psOptionsForBinary->osSource = papszArgv[i];
            }
            else
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "{source_filename} only supported from gdal_grid binary.");
            }
        }
        else if (!bGotDestFilename)
        {
            bGotDestFilename = true;
            if (psOptionsForBinary)
            {
                psOptionsForBinary->bDestSpecified = true;
                psOptionsForBinary->osDest = papszArgv[i];
            }
            else
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "{dest_filename} only supported from gdal_grid binary.");
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            return nullptr;
        }
    }

    if (psOptions->bClipSrc && !psOptions->osClipSrcDS.empty())
    {
        psOptions->poClipSrc =
            LoadGeometry(psOptions->osClipSrcDS, psOptions->osClipSrcSQL,
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
