/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Footprint OGR shapes into a GDAL raster.
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
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

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <limits>
#include <vector>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_mem.h"
#include "ogrsf_frmts.h"
#include "ogr_spatialref.h"

constexpr const char *DEFAULT_LAYER_NAME = "footprint";

/************************************************************************/
/*                          GDALFootprintOptions                        */
/************************************************************************/

struct GDALFootprintOptions
{
    /*! output format. Use the short format name. */
    std::string osFormat{};

    /*! the progress function to use */
    GDALProgressFunc pfnProgress = GDALDummyProgress;

    /*! pointer to the progress data variable */
    void *pProgressData = nullptr;

    bool bCreateOutput = false;

    std::string osDestLayerName{};

    /*! Layer creation options */
    CPLStringList aosLCO{};

    /*! Dataset creation options */
    CPLStringList aosDSCO{};

    /*! Overview index: 0 = first overview level */
    int nOvrIndex = -1;

    /** Whether output geometry should be in georeferenced coordinates, if
     * possible (if explicitly requested, bOutCSGeorefRequested is also set)
     * false = in pixel coordinates
     */
    bool bOutCSGeoref = true;

    /** Whether -t_cs georef has been explicitly set */
    bool bOutCSGeorefRequested = false;

    OGRSpatialReference oOutputSRS{};

    bool bSplitPolys = false;

    double dfDensifyDistance = 0;

    double dfSimplifyTolerance = 0;

    bool bConvexHull = false;

    double dfMinRingArea = 0;

    int nMaxPoints = 100;

    /*! Source bands to take into account */
    std::vector<int> anBands{};

    /*! Whether to combine bands unioning (true) or intersecting (false) */
    bool bCombineBandsUnion = true;

    /*! Field name where to write the path of the raster. Empty if not desired */
    std::string osLocationFieldName = "location";

    /*! Whether to force writing absolute paths in location field. */
    bool bAbsolutePath = false;

    std::string osSrcNoData;
};

/************************************************************************/
/*                       GDALFootprintMaskBand                          */
/************************************************************************/

class GDALFootprintMaskBand final : public GDALRasterBand
{
    GDALRasterBand *m_poSrcBand = nullptr;

  public:
    explicit GDALFootprintMaskBand(GDALRasterBand *poSrcBand)
        : m_poSrcBand(poSrcBand)
    {
        nRasterXSize = m_poSrcBand->GetXSize();
        nRasterYSize = m_poSrcBand->GetYSize();
        eDataType = GDT_Byte;
        m_poSrcBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) override
    {
        int nWindowXSize;
        int nWindowYSize;
        m_poSrcBand->GetActualBlockSize(nBlockXOff, nBlockYOff, &nWindowXSize,
                                        &nWindowYSize);
        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);
        return IRasterIO(GF_Read, nBlockXOff * nBlockXSize,
                         nBlockYOff * nBlockYSize, nWindowXSize, nWindowYSize,
                         pData, nWindowXSize, nWindowYSize, GDT_Byte, 1,
                         nBlockXSize, &sExtraArg);
    }

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override
    {
        if (eRWFlag == GF_Read && nXSize == nBufXSize && nYSize == nBufYSize &&
            eBufType == GDT_Byte && nPixelSpace == 1)
        {
            // Request when band seen as the mask band for GDALPolygonize()

            if (m_poSrcBand->RasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize, eBufType,
                                      nPixelSpace, nLineSpace,
                                      psExtraArg) != CE_None)
            {
                return CE_Failure;
            }
            GByte *pabyData = static_cast<GByte *>(pData);
            for (int iY = 0; iY < nYSize; ++iY)
            {
                for (int iX = 0; iX < nXSize; ++iX)
                {
                    if (pabyData[iX])
                        pabyData[iX] = 1;
                }
                pabyData += nLineSpace;
            }

            return CE_None;
        }

        if (eRWFlag == GF_Read && nXSize == nBufXSize && nYSize == nBufYSize &&
            eBufType == GDT_Int64 &&
            nPixelSpace == static_cast<int>(sizeof(int64_t)) &&
            (nLineSpace % nPixelSpace) == 0)
        {
            // Request when band seen as the value band for GDALPolygonize()

            if (m_poSrcBand->RasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize, eBufType,
                                      nPixelSpace, nLineSpace,
                                      psExtraArg) != CE_None)
            {
                return CE_Failure;
            }
            int64_t *panData = static_cast<int64_t *>(pData);
            for (int iY = 0; iY < nYSize; ++iY)
            {
                for (int iX = 0; iX < nXSize; ++iX)
                {
                    if (panData[iX])
                        panData[iX] = 1;
                }
                panData += (nLineSpace / nPixelSpace);
            }

            return CE_None;
        }

        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg);
    }
};

/************************************************************************/
/*                   GDALFootprintCombinedMaskBand                      */
/************************************************************************/

class GDALFootprintCombinedMaskBand final : public GDALRasterBand
{
    std::vector<GDALRasterBand *> m_apoSrcBands{};

    /** Whether to combine bands unioning (true) or intersecting (false) */
    bool m_bUnion = false;

  public:
    explicit GDALFootprintCombinedMaskBand(
        const std::vector<GDALRasterBand *> &apoSrcBands, bool bUnion)
        : m_apoSrcBands(apoSrcBands), m_bUnion(bUnion)
    {
        nRasterXSize = m_apoSrcBands[0]->GetXSize();
        nRasterYSize = m_apoSrcBands[0]->GetYSize();
        eDataType = GDT_Byte;
        m_apoSrcBands[0]->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) override
    {
        int nWindowXSize;
        int nWindowYSize;
        m_apoSrcBands[0]->GetActualBlockSize(nBlockXOff, nBlockYOff,
                                             &nWindowXSize, &nWindowYSize);
        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);
        return IRasterIO(GF_Read, nBlockXOff * nBlockXSize,
                         nBlockYOff * nBlockYSize, nWindowXSize, nWindowYSize,
                         pData, nWindowXSize, nWindowYSize, GDT_Byte, 1,
                         nBlockXSize, &sExtraArg);
    }

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override
    {
        if (eRWFlag == GF_Read && nXSize == nBufXSize && nYSize == nBufYSize &&
            eBufType == GDT_Byte && nPixelSpace == 1)
        {
            // Request when band seen as the mask band for GDALPolygonize()
            {
                GByte *pabyData = static_cast<GByte *>(pData);
                for (int iY = 0; iY < nYSize; ++iY)
                {
                    memset(pabyData, m_bUnion ? 0 : 1, nXSize);
                    pabyData += nLineSpace;
                }
            }

            std::vector<GByte> abyTmp(static_cast<size_t>(nXSize) * nYSize);
            for (auto poBand : m_apoSrcBands)
            {
                if (poBand->RasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                                     abyTmp.data(), nBufXSize, nBufYSize,
                                     GDT_Byte, 1, nXSize,
                                     psExtraArg) != CE_None)
                {
                    return CE_Failure;
                }
                GByte *pabyData = static_cast<GByte *>(pData);
                size_t iTmp = 0;
                for (int iY = 0; iY < nYSize; ++iY)
                {
                    if (m_bUnion)
                    {
                        for (int iX = 0; iX < nXSize; ++iX, ++iTmp)
                        {
                            if (abyTmp[iTmp])
                                pabyData[iX] = 1;
                        }
                    }
                    else
                    {
                        for (int iX = 0; iX < nXSize; ++iX, ++iTmp)
                        {
                            if (abyTmp[iTmp] == 0)
                                pabyData[iX] = 0;
                        }
                    }
                    pabyData += nLineSpace;
                }
            }

            return CE_None;
        }

        if (eRWFlag == GF_Read && nXSize == nBufXSize && nYSize == nBufYSize &&
            eBufType == GDT_Int64 &&
            nPixelSpace == static_cast<int>(sizeof(int64_t)) &&
            (nLineSpace % nPixelSpace) == 0)
        {
            // Request when band seen as the value band for GDALPolygonize()
            {
                int64_t *panData = static_cast<int64_t *>(pData);
                for (int iY = 0; iY < nYSize; ++iY)
                {
                    if (m_bUnion)
                    {
                        memset(panData, 0, nXSize * sizeof(int64_t));
                    }
                    else
                    {
                        int64_t nOne = 1;
                        GDALCopyWords(&nOne, GDT_Int64, 0, panData, GDT_Int64,
                                      sizeof(int64_t), nXSize);
                    }
                    panData += (nLineSpace / nPixelSpace);
                }
            }

            std::vector<GByte> abyTmp(static_cast<size_t>(nXSize) * nYSize);
            for (auto poBand : m_apoSrcBands)
            {
                if (poBand->RasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                                     abyTmp.data(), nBufXSize, nBufYSize,
                                     GDT_Byte, 1, nXSize,
                                     psExtraArg) != CE_None)
                {
                    return CE_Failure;
                }
                size_t iTmp = 0;
                int64_t *panData = static_cast<int64_t *>(pData);
                for (int iY = 0; iY < nYSize; ++iY)
                {
                    if (m_bUnion)
                    {
                        for (int iX = 0; iX < nXSize; ++iX, ++iTmp)
                        {
                            if (abyTmp[iTmp])
                                panData[iX] = 1;
                        }
                    }
                    else
                    {
                        for (int iX = 0; iX < nXSize; ++iX, ++iTmp)
                        {
                            if (abyTmp[iTmp] == 0)
                                panData[iX] = 0;
                        }
                    }
                    panData += (nLineSpace / nPixelSpace);
                }
            }
            return CE_None;
        }

        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg);
    }
};

/************************************************************************/
/*                    GetOutputLayerAndUpdateDstDS()                    */
/************************************************************************/

static OGRLayer *
GetOutputLayerAndUpdateDstDS(const char *pszDest, GDALDatasetH &hDstDS,
                             GDALDataset *poSrcDS,
                             const GDALFootprintOptions *psOptions)
{

    if (pszDest == nullptr)
        pszDest = GDALGetDescription(hDstDS);

    /* -------------------------------------------------------------------- */
    /*      Create output dataset if needed                                 */
    /* -------------------------------------------------------------------- */
    const bool bCreateOutput = psOptions->bCreateOutput || hDstDS == nullptr;

    GDALDriverH hDriver = nullptr;
    if (bCreateOutput)
    {
        std::string osFormat(psOptions->osFormat);
        if (osFormat.empty())
        {
            const auto aoDrivers = GetOutputDriversFor(pszDest, GDAL_OF_VECTOR);
            if (aoDrivers.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot guess driver for %s", pszDest);
                return nullptr;
            }
            else
            {
                if (aoDrivers.size() > 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Several drivers matching %s extension. Using %s",
                             CPLGetExtension(pszDest), aoDrivers[0].c_str());
                }
                osFormat = aoDrivers[0];
            }
        }

        /* ----------------------------------------------------------------- */
        /*      Find the output driver. */
        /* ----------------------------------------------------------------- */
        hDriver = GDALGetDriverByName(osFormat.c_str());
        char **papszDriverMD =
            hDriver ? GDALGetMetadata(hDriver, nullptr) : nullptr;
        if (hDriver == nullptr ||
            !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_VECTOR,
                                              "FALSE")) ||
            !CPLTestBool(
                CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE, "FALSE")))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Output driver `%s' not recognised or does not support "
                     "direct output file creation.",
                     osFormat.c_str());
            return nullptr;
        }

        hDstDS = GDALCreate(hDriver, pszDest, 0, 0, 0, GDT_Unknown,
                            psOptions->aosDSCO.List());
        if (!hDstDS)
        {
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Open or create target layer.                                    */
    /* -------------------------------------------------------------------- */
    auto poDstDS = GDALDataset::FromHandle(hDstDS);
    OGRLayer *poLayer = nullptr;
    if (!psOptions->osDestLayerName.empty())
    {
        poLayer = poDstDS->GetLayerByName(psOptions->osDestLayerName.c_str());
        if (!poLayer)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find layer %s",
                     psOptions->osDestLayerName.c_str());
            return nullptr;
        }
    }
    else if (poDstDS->GetLayerCount() == 1 && poDstDS->GetDriver() &&
             EQUAL(poDstDS->GetDriver()->GetDescription(), "ESRI Shapefile"))
    {
        poLayer = poDstDS->GetLayer(0);
    }
    else
    {
        poLayer = poDstDS->GetLayerByName(DEFAULT_LAYER_NAME);
    }
    if (!poLayer)
    {
        std::string osDestLayerName = psOptions->osDestLayerName;
        if (osDestLayerName.empty())
        {
            if (poDstDS->GetDriver() &&
                EQUAL(poDstDS->GetDriver()->GetDescription(), "ESRI Shapefile"))
            {
                osDestLayerName = CPLGetBasename(pszDest);
            }
            else
            {
                osDestLayerName = DEFAULT_LAYER_NAME;
            }
        }

        std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> poSRS;
        if (psOptions->bOutCSGeoref)
        {
            if (!psOptions->oOutputSRS.IsEmpty())
            {
                poSRS.reset(psOptions->oOutputSRS.Clone());
            }
            else if (auto poSrcSRS = poSrcDS->GetSpatialRef())
            {
                poSRS.reset(poSrcSRS->Clone());
            }
        }

        poLayer = poDstDS->CreateLayer(
            osDestLayerName.c_str(), poSRS.get(),
            psOptions->bSplitPolys ? wkbPolygon : wkbMultiPolygon,
            const_cast<char **>(psOptions->aosLCO.List()));

        if (!psOptions->osLocationFieldName.empty())
        {
            OGRFieldDefn oFieldDefn(psOptions->osLocationFieldName.c_str(),
                                    OFTString);
            if (poLayer->CreateField(&oFieldDefn) != OGRERR_NONE)
                return nullptr;
        }
    }

    return poLayer;
}

/************************************************************************/
/*                 GeoTransformCoordinateTransformation                 */
/************************************************************************/

class GeoTransformCoordinateTransformation final
    : public OGRCoordinateTransformation
{
    const std::array<double, 6> m_gt;

  public:
    explicit GeoTransformCoordinateTransformation(
        const std::array<double, 6> &gt)
        : m_gt(gt)
    {
    }

    const OGRSpatialReference *GetSourceCS() const override
    {
        return nullptr;
    }

    const OGRSpatialReference *GetTargetCS() const override
    {
        return nullptr;
    }

    OGRCoordinateTransformation *Clone() const override
    {
        return new GeoTransformCoordinateTransformation(m_gt);
    }

    OGRCoordinateTransformation *GetInverse() const override
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoTransformCoordinateTransformation::GetInverse() not "
                 "implemented");
        return nullptr;
    }

    int Transform(size_t nCount, double *x, double *y, double * /* z */,
                  double * /* t */, int *pabSuccess) override
    {
        for (size_t i = 0; i < nCount; ++i)
        {
            const double X = m_gt[0] + x[i] * m_gt[1] + y[i] * m_gt[2];
            const double Y = m_gt[3] + x[i] * m_gt[4] + y[i] * m_gt[5];
            x[i] = X;
            y[i] = Y;
            if (pabSuccess)
                pabSuccess[i] = TRUE;
        }
        return TRUE;
    }
};

/************************************************************************/
/*                             CountPoints()                            */
/************************************************************************/

static size_t CountPoints(const OGRGeometry *poGeom)
{
    if (poGeom->getGeometryType() == wkbMultiPolygon)
    {
        size_t n = 0;
        for (auto *poPoly : poGeom->toMultiPolygon())
        {
            n += CountPoints(poPoly);
        }
        return n;
    }
    else if (poGeom->getGeometryType() == wkbPolygon)
    {
        size_t n = 0;
        for (auto *poRing : poGeom->toPolygon())
        {
            n += poRing->getNumPoints() - 1;
        }
        return n;
    }
    return 0;
}

/************************************************************************/
/*                   GetMinDistanceBetweenTwoPoints()                   */
/************************************************************************/

static double GetMinDistanceBetweenTwoPoints(const OGRGeometry *poGeom)
{
    if (poGeom->getGeometryType() == wkbMultiPolygon)
    {
        double v = std::numeric_limits<double>::max();
        for (auto *poPoly : poGeom->toMultiPolygon())
        {
            v = std::min(v, GetMinDistanceBetweenTwoPoints(poPoly));
        }
        return v;
    }
    else if (poGeom->getGeometryType() == wkbPolygon)
    {
        double v = std::numeric_limits<double>::max();
        for (auto *poRing : poGeom->toPolygon())
        {
            v = std::min(v, GetMinDistanceBetweenTwoPoints(poRing));
        }
        return v;
    }
    else if (poGeom->getGeometryType() == wkbLineString)
    {
        double v = std::numeric_limits<double>::max();
        const auto poLS = poGeom->toLineString();
        const int nNumPoints = poLS->getNumPoints();
        for (int i = 0; i < nNumPoints - 1; ++i)
        {
            const double x1 = poLS->getX(i);
            const double y1 = poLS->getY(i);
            const double x2 = poLS->getX(i + 1);
            const double y2 = poLS->getY(i + 1);
            const double d = (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
            if (d > 0)
                v = std::min(v, d);
        }
        return sqrt(v);
    }
    return 0;
}

/************************************************************************/
/*                       GDALFootprintProcess()                         */
/************************************************************************/

static bool GDALFootprintProcess(GDALDataset *poSrcDS, OGRLayer *poDstLayer,
                                 const GDALFootprintOptions *psOptions)
{
    std::unique_ptr<OGRCoordinateTransformation> poCT_SRS;
    const OGRSpatialReference *poDstSRS = poDstLayer->GetSpatialRef();
    if (!psOptions->oOutputSRS.IsEmpty())
        poDstSRS = &(psOptions->oOutputSRS);
    if (poDstSRS)
    {
        auto poSrcSRS = poSrcDS->GetSpatialRef();
        if (!poSrcSRS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Output layer has CRS, but input is not georeferenced");
            return false;
        }
        poCT_SRS.reset(OGRCreateCoordinateTransformation(poSrcSRS, poDstSRS));
        if (!poCT_SRS)
            return false;
    }

    std::vector<int> anBands = psOptions->anBands;
    const int nBandCount = poSrcDS->GetRasterCount();
    if (anBands.empty())
    {
        for (int i = 1; i <= nBandCount; ++i)
            anBands.push_back(i);
    }

    std::vector<GDALRasterBand *> apoSrcMaskBands;
    const CPLStringList aosSrcNoData(
        CSLTokenizeString2(psOptions->osSrcNoData.c_str(), " ", 0));
    std::vector<double> adfSrcNoData;
    if (!psOptions->osSrcNoData.empty())
    {
        if (aosSrcNoData.size() != 1 &&
            static_cast<size_t>(aosSrcNoData.size()) != anBands.size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Number of values in -srcnodata should be 1 or the number "
                     "of bands");
            return false;
        }
        for (int i = 0; i < aosSrcNoData.size(); ++i)
        {
            adfSrcNoData.emplace_back(CPLAtof(aosSrcNoData[i]));
        }
    }
    bool bGlobalMask = true;
    std::vector<std::unique_ptr<GDALRasterBand>> apoTmpNoDataMaskBands;
    for (size_t i = 0; i < anBands.size(); ++i)
    {
        const int nBand = anBands[i];
        if (nBand <= 0 || nBand > nBandCount)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid band number: %d",
                     nBand);
            return false;
        }
        auto poBand = poSrcDS->GetRasterBand(nBand);
        if (!adfSrcNoData.empty())
        {
            bGlobalMask = false;
            apoTmpNoDataMaskBands.emplace_back(
                std::make_unique<GDALNoDataMaskBand>(
                    poBand, adfSrcNoData.size() == 1 ? adfSrcNoData[0]
                                                     : adfSrcNoData[i]));
            apoSrcMaskBands.push_back(apoTmpNoDataMaskBands.back().get());
        }
        else
        {
            GDALRasterBand *poMaskBand;
            const int nMaskFlags = poBand->GetMaskFlags();
            if (poBand->GetColorInterpretation() == GCI_AlphaBand)
            {
                poMaskBand = poBand;
            }
            else
            {
                if ((nMaskFlags & GMF_PER_DATASET) == 0)
                {
                    bGlobalMask = false;
                }
                poMaskBand = poBand->GetMaskBand();
            }
            if (psOptions->nOvrIndex >= 0)
            {
                if (nMaskFlags == GMF_NODATA)
                {
                    // If the mask band is based on nodata, we don't need
                    // to check the overviews of the mask band, but we
                    // can take the mask band of the overviews
                    auto poOvrBand = poBand->GetOverview(psOptions->nOvrIndex);
                    if (!poOvrBand)
                    {
                        if (poBand->GetOverviewCount() == 0)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Overview index %d invalid for this dataset. "
                                "Bands of this dataset have no "
                                "precomputed overviews",
                                psOptions->nOvrIndex);
                        }
                        else
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Overview index %d invalid for this dataset. "
                                "Value should be in [0,%d] range",
                                psOptions->nOvrIndex,
                                poBand->GetOverviewCount() - 1);
                        }
                        return false;
                    }
                    if (poOvrBand->GetMaskFlags() != GMF_NODATA)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "poOvrBand->GetMaskFlags() != GMF_NODATA");
                        return false;
                    }
                    poMaskBand = poOvrBand->GetMaskBand();
                }
                else
                {
                    poMaskBand = poMaskBand->GetOverview(psOptions->nOvrIndex);
                    if (!poMaskBand)
                    {
                        if (poBand->GetMaskBand()->GetOverviewCount() == 0)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Overview index %d invalid for this dataset. "
                                "Mask bands of this dataset have no "
                                "precomputed overviews",
                                psOptions->nOvrIndex);
                        }
                        else
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Overview index %d invalid for this dataset. "
                                "Value should be in [0,%d] range",
                                psOptions->nOvrIndex,
                                poBand->GetMaskBand()->GetOverviewCount() - 1);
                        }
                        return false;
                    }
                }
            }
            apoSrcMaskBands.push_back(poMaskBand);
        }
    }

    std::unique_ptr<OGRCoordinateTransformation> poCT_GT;
    std::array<double, 6> adfGeoTransform{{0.0, 1.0, 0.0, 0.0, 0.0, 1.0}};
    if (psOptions->bOutCSGeoref &&
        poSrcDS->GetGeoTransform(adfGeoTransform.data()) == CE_None)
    {
        auto poMaskBand = apoSrcMaskBands[0];
        adfGeoTransform[1] *=
            double(poSrcDS->GetRasterXSize()) / poMaskBand->GetXSize();
        adfGeoTransform[2] *=
            double(poSrcDS->GetRasterYSize()) / poMaskBand->GetYSize();
        adfGeoTransform[4] *=
            double(poSrcDS->GetRasterXSize()) / poMaskBand->GetXSize();
        adfGeoTransform[5] *=
            double(poSrcDS->GetRasterYSize()) / poMaskBand->GetYSize();
        poCT_GT = std::make_unique<GeoTransformCoordinateTransformation>(
            adfGeoTransform);
    }
    else if (psOptions->bOutCSGeorefRequested)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Georeferenced coordinates requested, but "
                 "input dataset has no geotransform.");
        return false;
    }
    else if (psOptions->nOvrIndex >= 0)
    {
        // Transform from overview pixel coordinates to full resolution
        // pixel coordinates
        auto poMaskBand = apoSrcMaskBands[0];
        adfGeoTransform[1] =
            double(poSrcDS->GetRasterXSize()) / poMaskBand->GetXSize();
        adfGeoTransform[2] = 0;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] =
            double(poSrcDS->GetRasterYSize()) / poMaskBand->GetYSize();
        poCT_GT = std::make_unique<GeoTransformCoordinateTransformation>(
            adfGeoTransform);
    }

    std::unique_ptr<GDALRasterBand> poMaskForRasterize;
    if (bGlobalMask || anBands.size() == 1)
    {
        poMaskForRasterize =
            std::make_unique<GDALFootprintMaskBand>(apoSrcMaskBands[0]);
    }
    else
    {
        poMaskForRasterize = std::make_unique<GDALFootprintCombinedMaskBand>(
            apoSrcMaskBands, psOptions->bCombineBandsUnion);
    }

    auto hBand = GDALRasterBand::ToHandle(poMaskForRasterize.get());
    auto poMemLayer = std::make_unique<OGRMemLayer>("", nullptr, wkbUnknown);
    const CPLErr eErr =
        GDALPolygonize(hBand, hBand, OGRLayer::ToHandle(poMemLayer.get()),
                       /* iPixValField = */ -1,
                       /* papszOptions = */ nullptr, psOptions->pfnProgress,
                       psOptions->pProgressData);
    if (eErr != CE_None)
    {
        return false;
    }

    if (!psOptions->bSplitPolys)
    {
        auto poMP = std::make_unique<OGRMultiPolygon>();
        for (auto &&poFeature : poMemLayer.get())
        {
            auto poGeom =
                std::unique_ptr<OGRGeometry>(poFeature->StealGeometry());
            CPLAssert(poGeom);
            if (poGeom->getGeometryType() == wkbPolygon)
            {
                poMP->addGeometryDirectly(poGeom.release());
            }
        }
        poMemLayer = std::make_unique<OGRMemLayer>("", nullptr, wkbUnknown);
        auto poFeature =
            std::make_unique<OGRFeature>(poMemLayer->GetLayerDefn());
        poFeature->SetGeometryDirectly(poMP.release());
        CPL_IGNORE_RET_VAL(poMemLayer->CreateFeature(poFeature.get()));
    }

    for (auto &&poFeature : poMemLayer.get())
    {
        auto poGeom = std::unique_ptr<OGRGeometry>(poFeature->StealGeometry());
        CPLAssert(poGeom);
        if (poGeom->IsEmpty())
            continue;

        auto poDstFeature =
            std::make_unique<OGRFeature>(poDstLayer->GetLayerDefn());
        poDstFeature->SetFrom(poFeature.get());

        if (poCT_GT)
        {
            if (poGeom->transform(poCT_GT.get()) != OGRERR_NONE)
                return false;
        }

        if (psOptions->dfDensifyDistance > 0)
        {
            OGREnvelope sEnvelope;
            poGeom->getEnvelope(&sEnvelope);
            // Some sanity check to avoid insane memory allocations
            if (sEnvelope.MaxX - sEnvelope.MinX >
                    1e6 * psOptions->dfDensifyDistance ||
                sEnvelope.MaxY - sEnvelope.MinY >
                    1e6 * psOptions->dfDensifyDistance)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Densification distance too small compared to "
                         "geometry extent");
                return false;
            }
            poGeom->segmentize(psOptions->dfDensifyDistance);
        }

        if (poCT_SRS)
        {
            if (poGeom->transform(poCT_SRS.get()) != OGRERR_NONE)
                return false;
        }

        if (psOptions->dfMinRingArea != 0)
        {
            if (poGeom->getGeometryType() == wkbMultiPolygon)
            {
                auto poMP = std::make_unique<OGRMultiPolygon>();
                for (auto *poPoly : poGeom->toMultiPolygon())
                {
                    auto poNewPoly = std::make_unique<OGRPolygon>();
                    for (auto *poRing : poPoly)
                    {
                        if (poRing->get_Area() >= psOptions->dfMinRingArea)
                        {
                            poNewPoly->addRing(poRing);
                        }
                    }
                    if (!poNewPoly->IsEmpty())
                        poMP->addGeometryDirectly(poNewPoly.release());
                }
                poGeom = std::move(poMP);
            }
            else if (poGeom->getGeometryType() == wkbPolygon)
            {
                auto poNewPoly = std::make_unique<OGRPolygon>();
                for (auto *poRing : poGeom->toPolygon())
                {
                    if (poRing->get_Area() >= psOptions->dfMinRingArea)
                    {
                        poNewPoly->addRing(poRing);
                    }
                }
                poGeom = std::move(poNewPoly);
            }
            if (poGeom->IsEmpty())
                continue;
        }

        if (psOptions->bConvexHull)
        {
            poGeom.reset(poGeom->ConvexHull());
            if (!poGeom || poGeom->IsEmpty())
                continue;
        }

        if (psOptions->dfSimplifyTolerance != 0)
        {
            poGeom.reset(poGeom->Simplify(psOptions->dfSimplifyTolerance));
            if (!poGeom || poGeom->IsEmpty())
                continue;
        }

        if (psOptions->nMaxPoints > 0 &&
            CountPoints(poGeom.get()) >
                static_cast<size_t>(psOptions->nMaxPoints))
        {
            OGREnvelope sEnvelope;
            poGeom->getEnvelope(&sEnvelope);
            double tolMin = GetMinDistanceBetweenTwoPoints(poGeom.get());
            double tolMax = std::max(sEnvelope.MaxY - sEnvelope.MinY,
                                     sEnvelope.MaxX - sEnvelope.MinX);
            for (int i = 0; i < 20; ++i)
            {
                const double tol = (tolMin + tolMax) / 2;
                std::unique_ptr<OGRGeometry> poSimplifiedGeom(
                    poGeom->Simplify(tol));
                if (!poSimplifiedGeom || poSimplifiedGeom->IsEmpty())
                {
                    tolMax = tol;
                    continue;
                }
                const auto nPoints = CountPoints(poSimplifiedGeom.get());
                if (nPoints == static_cast<size_t>(psOptions->nMaxPoints))
                {
                    tolMax = tol;
                    break;
                }
                else if (nPoints < static_cast<size_t>(psOptions->nMaxPoints))
                {
                    tolMax = tol;
                }
                else
                {
                    tolMin = tol;
                }
            }
            poGeom.reset(poGeom->Simplify(tolMax));
            if (!poGeom || poGeom->IsEmpty())
                continue;
        }

        if (!psOptions->bSplitPolys && poGeom->getGeometryType() == wkbPolygon)
            poGeom.reset(
                OGRGeometryFactory::forceToMultiPolygon(poGeom.release()));

        poDstFeature->SetGeometryDirectly(poGeom.release());

        if (!psOptions->osLocationFieldName.empty())
        {

            std::string osFilename = poSrcDS->GetDescription();
            // Make sure it is a file before building absolute path name.
            VSIStatBufL sStatBuf;
            if (psOptions->bAbsolutePath &&
                CPLIsFilenameRelative(osFilename.c_str()) &&
                VSIStatL(osFilename.c_str(), &sStatBuf) == 0)
            {
                char *pszCurDir = CPLGetCurrentDir();
                if (pszCurDir)
                {
                    osFilename = CPLProjectRelativeFilename(pszCurDir,
                                                            osFilename.c_str());
                    CPLFree(pszCurDir);
                }
            }
            poDstFeature->SetField(psOptions->osLocationFieldName.c_str(),
                                   osFilename.c_str());
        }

        if (poDstLayer->CreateFeature(poDstFeature.get()) != OGRERR_NONE)
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                             GDALFootprint()                          */
/************************************************************************/

/* clang-format off */
/**
 * Computes the footprint of a raster.
 *
 * This is the equivalent of the
 * <a href="/programs/gdal_footprint.html">gdal_footprint</a> utility.
 *
 * GDALFootprintOptions* must be allocated and freed with
 * GDALFootprintOptionsNew() and GDALFootprintOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * @param pszDest the vector destination dataset path or NULL.
 * @param hDstDS the vector destination dataset or NULL.
 * @param hSrcDataset the raster source dataset handle.
 * @param psOptionsIn the options struct returned by GDALFootprintOptionsNew()
 * or NULL.
 * @param pbUsageError pointer to a integer output variable to store if any
 * usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose(), or hDstDS is not NULL) or NULL in case of error.
 *
 * @since GDAL 3.8
 */
/* clang-format on */

GDALDatasetH GDALFootprint(const char *pszDest, GDALDatasetH hDstDS,
                           GDALDatasetH hSrcDataset,
                           const GDALFootprintOptions *psOptionsIn,
                           int *pbUsageError)
{
    if (pszDest == nullptr && hDstDS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "pszDest == NULL && hDstDS == NULL");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (hSrcDataset == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "hSrcDataset== NULL");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (hDstDS != nullptr && psOptionsIn && psOptionsIn->bCreateOutput)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "hDstDS != NULL but options that imply creating a new dataset "
                 "have been set.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    GDALFootprintOptions *psOptionsToFree = nullptr;
    const GDALFootprintOptions *psOptions = psOptionsIn;
    if (psOptions == nullptr)
    {
        psOptionsToFree = GDALFootprintOptionsNew(nullptr, nullptr);
        psOptions = psOptionsToFree;
    }

    const bool bCloseOutDSOnError = hDstDS == nullptr;

    auto poSrcDS = GDALDataset::FromHandle(hSrcDataset);
    if (poSrcDS->GetRasterCount() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Input dataset has no raster band.%s",
                 poSrcDS->GetMetadata("SUBDATASETS")
                     ? " You need to specify one subdataset."
                     : "");
        GDALFootprintOptionsFree(psOptionsToFree);
        if (bCloseOutDSOnError)
            GDALClose(hDstDS);
        return nullptr;
    }

    auto poLayer =
        GetOutputLayerAndUpdateDstDS(pszDest, hDstDS, poSrcDS, psOptions);
    if (!poLayer)
    {
        GDALFootprintOptionsFree(psOptionsToFree);
        if (hDstDS && bCloseOutDSOnError)
            GDALClose(hDstDS);
        return nullptr;
    }

    if (!GDALFootprintProcess(poSrcDS, poLayer, psOptions))
    {
        GDALFootprintOptionsFree(psOptionsToFree);
        if (bCloseOutDSOnError)
            GDALClose(hDstDS);
        return nullptr;
    }

    GDALFootprintOptionsFree(psOptionsToFree);

    return hDstDS;
}

/************************************************************************/
/*                           GDALFootprintOptionsNew()                  */
/************************************************************************/

/**
 * Allocates a GDALFootprintOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdal_rasterize.html">gdal_rasterize</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdal_translate_bin.cpp use case) must be allocated with
 * GDALFootprintOptionsForBinaryNew() prior to this function. Will be filled
 * with potentially present filename, open options,...
 * @return pointer to the allocated GDALFootprintOptions struct. Must be freed
 * with GDALFootprintOptionsFree().
 *
 * @since GDAL 3.8
 */

GDALFootprintOptions *
GDALFootprintOptionsNew(char **papszArgv,
                        GDALFootprintOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALFootprintOptions>();

    bool bGotSourceFilename = false;
    bool bGotDestFilename = false;
    /* -------------------------------------------------------------------- */
    /*      Handle command line arguments.                                  */
    /* -------------------------------------------------------------------- */
    const int argc = CSLCount(papszArgv);
    for (int i = 0; papszArgv != nullptr && i < argc; i++)
    {
        if (i < argc - 1 &&
            (EQUAL(papszArgv[i], "-of") || EQUAL(papszArgv[i], "-f")))
        {
            ++i;
            psOptions->osFormat = papszArgv[i];
            psOptions->bCreateOutput = true;
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
                         "%s switch only supported from gdal_footprint binary.",
                         papszArgv[i]);
                return nullptr;
            }
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-oo"))
        {
            i++;
            if (psOptionsForBinary)
            {
                psOptionsForBinary->aosOpenOptions.AddString(papszArgv[i]);
            }
            else
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "-oo switch only supported from gdal_footprint binary.");
            }
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-t_cs"))
        {
            i++;
            const std::string osVal(papszArgv[i]);
            if (osVal == "georef")
            {
                psOptions->bOutCSGeoref = true;
                psOptions->bOutCSGeorefRequested = true;
            }
            else if (osVal == "pixel")
            {
                psOptions->bOutCSGeoref = false;
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Invalid value for -t_cs");
                return nullptr;
            }
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-t_srs"))
        {
            i++;
            const std::string osVal(papszArgv[i]);
            if (psOptions->oOutputSRS.SetFromUserInput(osVal.c_str()) !=
                OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to process SRS definition: %s", osVal.c_str());
                return nullptr;
            }
            psOptions->oOutputSRS.SetAxisMappingStrategy(
                OAMS_TRADITIONAL_GIS_ORDER);
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-b"))
        {
            i++;
            psOptions->anBands.push_back(atoi(papszArgv[i]));
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-combine_bands"))
        {
            i++;
            if (EQUAL(papszArgv[i], "union"))
                psOptions->bCombineBandsUnion = true;
            else if (EQUAL(papszArgv[i], "intersection"))
                psOptions->bCombineBandsUnion = false;
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Invalid value for -combine_bands");
                return nullptr;
            }
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-srcnodata"))
        {
            i++;
            psOptions->osSrcNoData = papszArgv[i];
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-lco"))
        {
            i++;
            psOptions->aosLCO.AddString(papszArgv[i]);
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-dsco"))
        {
            i++;
            psOptions->aosDSCO.AddString(papszArgv[i]);
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-lyr_name"))
        {
            i++;
            psOptions->osDestLayerName = papszArgv[i];
        }

        else if (EQUAL(papszArgv[i], "-split_polys"))
        {
            psOptions->bSplitPolys = true;
        }

        else if (EQUAL(papszArgv[i], "-convex_hull"))
        {
            psOptions->bConvexHull = true;
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-densify"))
        {
            i++;
            psOptions->dfDensifyDistance = CPLAtof(papszArgv[i]);
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-simplify"))
        {
            i++;
            psOptions->dfSimplifyTolerance = CPLAtof(papszArgv[i]);
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-max_points"))
        {
            i++;
            if (EQUAL(papszArgv[i], "unlimited"))
            {
                psOptions->nMaxPoints = 0;
            }
            else
            {
                psOptions->nMaxPoints = atoi(papszArgv[i]);
                if (psOptions->nMaxPoints > 0 && psOptions->nMaxPoints < 3)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Invalid value for -max_points");
                    return nullptr;
                }
            }
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-min_ring_area"))
        {
            i++;
            psOptions->dfMinRingArea = CPLAtof(papszArgv[i]);
        }

        else if (EQUAL(papszArgv[i], "-overwrite"))
        {
            if (psOptionsForBinary)
            {
                psOptionsForBinary->bOverwrite = true;
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "-overwrite switch only supported from gdal_footprint "
                         "binary.");
                return nullptr;
            }
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-location_field_name"))
        {
            i++;
            psOptions->osLocationFieldName = papszArgv[i];
        }

        else if (EQUAL(papszArgv[i], "-no_location"))
        {
            psOptions->osLocationFieldName.clear();
        }

        else if (EQUAL(papszArgv[i], "-write_absolute_path"))
        {
            psOptions->bAbsolutePath = true;
        }

        else if (i < argc - 1 && EQUAL(papszArgv[i], "-ovr"))
        {
            i++;
            psOptions->nOvrIndex = atoi(papszArgv[i]);
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
                CPLError(CE_Failure, CPLE_NotSupported,
                         "{source_filename} only supported from gdal_footprint "
                         "binary.");
                return nullptr;
            }
        }
        else if (!bGotDestFilename)
        {
            bGotDestFilename = true;
            CPLAssert(psOptionsForBinary);
            psOptionsForBinary->bDestSpecified = true;
            psOptionsForBinary->osDest = papszArgv[i];
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            return nullptr;
        }
    }

    if (!psOptions->bOutCSGeoref && !psOptions->oOutputSRS.IsEmpty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "-t_cs pixel and -t_srs are mutually exclusive.");
        return nullptr;
    }

    if (!psOptions->osSrcNoData.empty() && psOptions->nOvrIndex >= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "-srcnodata and -ovr are mutually exclusive.");
        return nullptr;
    }

    if (psOptionsForBinary)
    {
        psOptionsForBinary->bCreateOutput = psOptions->bCreateOutput;
        psOptionsForBinary->osFormat = psOptions->osFormat;
        psOptionsForBinary->osDestLayerName = psOptions->osDestLayerName;
    }

    return psOptions.release();
}

/************************************************************************/
/*                       GDALFootprintOptionsFree()                     */
/************************************************************************/

/**
 * Frees the GDALFootprintOptions struct.
 *
 * @param psOptions the options struct for GDALFootprint().
 *
 * @since GDAL 3.8
 */

void GDALFootprintOptionsFree(GDALFootprintOptions *psOptions)
{
    delete psOptions;
}

/************************************************************************/
/*                  GDALFootprintOptionsSetProgress()                   */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALFootprint().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 3.8
 */

void GDALFootprintOptionsSetProgress(GDALFootprintOptions *psOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
}
