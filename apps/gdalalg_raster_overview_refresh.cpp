/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster overview refresh" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_overview_refresh.h"

#include "cpl_string.h"
#include "gdal_priv.h"
#include "vrtdataset.h"
#include "vrt_priv.h"

#include <algorithm>
#include <limits>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                 GDALRasterOverviewAlgorithmRefresh()                 */
/************************************************************************/

GDALRasterOverviewAlgorithmRefresh::GDALRasterOverviewAlgorithmRefresh()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOpenOptionsArg(&m_openOptions);
    AddArg("dataset", 0,
           _("Dataset (to be updated in-place, unless --external)"), &m_dataset,
           GDAL_OF_RASTER | GDAL_OF_UPDATE)
        .SetPositional()
        .SetRequired();
    AddArg("external", 0, _("Refresh external overviews"), &m_readOnly)
        .AddHiddenAlias("ro")
        .AddHiddenAlias(GDAL_ARG_NAME_READ_ONLY);

    AddArg("resampling", 'r', _("Resampling method"), &m_resampling)
        .SetChoices("nearest", "average", "cubic", "cubicspline", "lanczos",
                    "bilinear", "gauss", "average_magphase", "rms", "mode")
        .SetHiddenChoices("near", "none");

    AddArg("levels", 0, _("Levels / decimation factors"), &m_levels)
        .SetMinValueIncluded(2);

    AddBBOXArg(&m_refreshBbox, _("Bounding box to refresh"))
        .SetMutualExclusionGroup("refresh");
    AddArg("like", 0, _("Use extent of dataset(s)"), &m_like)
        .SetMutualExclusionGroup("refresh");
    AddArg("use-source-timestamp", 0,
           _("Use timestamp of VRT or GTI sources as refresh criterion"),
           &m_refreshFromSourceTimestamp)
        .SetMutualExclusionGroup("refresh");
}

/************************************************************************/
/*                           PartialRefresh()                           */
/************************************************************************/

static bool PartialRefresh(GDALDataset *poDS,
                           const std::vector<int> &anOvrIndices,
                           const char *pszResampling, int nXOff, int nYOff,
                           int nXSize, int nYSize, GDALProgressFunc pfnProgress,
                           void *pProgressArg)
{
    int nOvCount = 0;
    const int nBandCount = poDS->GetRasterCount();
    for (int i = 0; i < nBandCount; ++i)
    {
        auto poSrcBand = poDS->GetRasterBand(i + 1);
        if (i == 0)
            nOvCount = poSrcBand->GetOverviewCount();
        else if (nOvCount != poSrcBand->GetOverviewCount())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Not same number of overviews on all bands");
            return false;
        }
    }

    std::vector<GDALRasterBand *> apoSrcBands;
    std::vector<std::vector<GDALRasterBand *>> aapoOverviewBands;
    for (int i = 0; i < nBandCount; ++i)
    {
        auto poSrcBand = poDS->GetRasterBand(i + 1);
        apoSrcBands.push_back(poSrcBand);
        std::vector<GDALRasterBand *> apoOverviewBands;
        for (int nOvrIdx : anOvrIndices)
        {
            apoOverviewBands.push_back(poSrcBand->GetOverview(nOvrIdx));
        }
        aapoOverviewBands.push_back(std::move(apoOverviewBands));
    }

    CPLStringList aosOptions;
    aosOptions.SetNameValue("XOFF", CPLSPrintf("%d", nXOff));
    aosOptions.SetNameValue("YOFF", CPLSPrintf("%d", nYOff));
    aosOptions.SetNameValue("XSIZE", CPLSPrintf("%d", nXSize));
    aosOptions.SetNameValue("YSIZE", CPLSPrintf("%d", nYSize));
    return GDALRegenerateOverviewsMultiBand(
               apoSrcBands, aapoOverviewBands, pszResampling, pfnProgress,
               pProgressArg, aosOptions.List()) == CE_None;
}

/************************************************************************/
/*                 PartialRefreshFromSourceTimestamp()                  */
/************************************************************************/

static bool
PartialRefreshFromSourceTimestamp(GDALDataset *poDS, const char *pszResampling,
                                  const std::vector<int> &anOvrIndices,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressArg)
{
    VSIStatBufL sStatOvr;
    std::string osOvr(std::string(poDS->GetDescription()) + ".ovr");
    if (VSIStatL(osOvr.c_str(), &sStatOvr) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", osOvr.c_str());
        return false;
    }
    if (sStatOvr.st_mtime == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot get modification time of %s", osOvr.c_str());
        return false;
    }

    std::vector<GTISourceDesc> regions;

    // init slightly above zero to please Coverity Scan
    double dfTotalPixels = std::numeric_limits<double>::min();

    if (dynamic_cast<VRTDataset *>(poDS))
    {
        auto poVRTBand =
            dynamic_cast<VRTSourcedRasterBand *>(poDS->GetRasterBand(1));
        if (!poVRTBand)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Band is not a VRTSourcedRasterBand");
            return false;
        }

        for (auto &poSource : poVRTBand->m_papoSources)
        {
            auto poSimpleSource =
                dynamic_cast<VRTSimpleSource *>(poSource.get());
            if (poSimpleSource)
            {
                VSIStatBufL sStatSource;
                if (VSIStatL(poSimpleSource->GetSourceDatasetName().c_str(),
                             &sStatSource) == 0)
                {
                    if (sStatSource.st_mtime > sStatOvr.st_mtime)
                    {
                        double dfXOff, dfYOff, dfXSize, dfYSize;
                        poSimpleSource->GetDstWindow(dfXOff, dfYOff, dfXSize,
                                                     dfYSize);
                        constexpr double EPS = 1e-8;
                        int nXOff = static_cast<int>(dfXOff + EPS);
                        int nYOff = static_cast<int>(dfYOff + EPS);
                        int nXSize = static_cast<int>(dfXSize + 0.5);
                        int nYSize = static_cast<int>(dfYSize + 0.5);
                        if (!(nXOff > poDS->GetRasterXSize() ||
                              nYOff > poDS->GetRasterYSize() || nXSize <= 0 ||
                              nYSize <= 0))
                        {
                            if (nXOff < 0)
                            {
                                nXSize += nXOff;
                                nXOff = 0;
                            }
                            if (nXOff > poDS->GetRasterXSize() - nXSize)
                            {
                                nXSize = poDS->GetRasterXSize() - nXOff;
                            }
                            if (nYOff < 0)
                            {
                                nYSize += nYOff;
                                nYOff = 0;
                            }
                            if (nYOff > poDS->GetRasterYSize() - nYSize)
                            {
                                nYSize = poDS->GetRasterYSize() - nYOff;
                            }

                            dfTotalPixels +=
                                static_cast<double>(nXSize) * nYSize;
                            GTISourceDesc region;
                            region.osFilename =
                                poSimpleSource->GetSourceDatasetName();
                            region.nDstXOff = nXOff;
                            region.nDstYOff = nYOff;
                            region.nDstXSize = nXSize;
                            region.nDstYSize = nYSize;
                            regions.push_back(std::move(region));
                        }
                    }
                }
            }
        }
    }
#ifdef GTI_DRIVER_DISABLED_OR_PLUGIN
    else if (poDS->GetDriver() &&
             EQUAL(poDS->GetDriver()->GetDescription(), "GTI"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "--use-source-timestamp only works on a GTI "
                 "dataset if the GTI driver is not built as a plugin, "
                 "but in core library");
        return false;
    }
#else
    else if (auto poGTIDS = GDALDatasetCastToGTIDataset(poDS))
    {
        regions = GTIGetSourcesMoreRecentThan(poGTIDS, sStatOvr.st_mtime);
        for (const auto &region : regions)
        {
            dfTotalPixels +=
                static_cast<double>(region.nDstXSize) * region.nDstYSize;
        }
    }
#endif
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "--use-source-timestamp only works on a VRT or GTI "
                 "dataset");
        return false;
    }

    bool bRet = true;
    if (!regions.empty())
    {
        double dfCurPixels = 0;
        for (const auto &region : regions)
        {
            if (bRet)
            {
                CPLDebug("GDAL", "Refresh from source %s",
                         region.osFilename.c_str());
                double dfNextCurPixels =
                    dfCurPixels +
                    static_cast<double>(region.nDstXSize) * region.nDstYSize;
                void *pScaledProgress = GDALCreateScaledProgress(
                    dfCurPixels / dfTotalPixels,
                    dfNextCurPixels / dfTotalPixels, pfnProgress, pProgressArg);
                bRet = PartialRefresh(
                    poDS, anOvrIndices, pszResampling, region.nDstXOff,
                    region.nDstYOff, region.nDstXSize, region.nDstYSize,
                    pScaledProgress ? GDALScaledProgress : nullptr,
                    pScaledProgress);
                GDALDestroyScaledProgress(pScaledProgress);
                dfCurPixels = dfNextCurPixels;
            }
        }
    }
    else
    {
        CPLDebug("GDAL", "No source is more recent than the overviews");
    }

    return bRet;
}

/************************************************************************/
/*                   PartialRefreshFromSourceExtent()                   */
/************************************************************************/

static bool PartialRefreshFromSourceExtent(
    GDALDataset *poDS, const std::vector<std::string> &sources,
    const char *pszResampling, const std::vector<int> &anOvrIndices,
    GDALProgressFunc pfnProgress, void *pProgressArg)
{
    GDALGeoTransform gt;
    if (poDS->GetGeoTransform(gt) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset has no geotransform");
        return false;
    }
    GDALGeoTransform invGT;
    if (!gt.GetInverse(invGT))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
        return false;
    }

    struct Region
    {
        std::string osFileName{};
        int nXOff = 0;
        int nYOff = 0;
        int nXSize = 0;
        int nYSize = 0;
    };

    std::vector<Region> regions;

    // init slightly above zero to please Coverity Scan
    double dfTotalPixels = std::numeric_limits<double>::min();
    for (const std::string &filename : sources)
    {
        auto poSrcDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            filename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
        if (!poSrcDS)
            return false;

        GDALGeoTransform srcGT;
        if (poSrcDS->GetGeoTransform(srcGT) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Source dataset has no geotransform");
            return false;
        }

        const double dfULX = srcGT[0];
        const double dfULY = srcGT[3];
        const double dfLRX = srcGT[0] + poSrcDS->GetRasterXSize() * srcGT[1] +
                             poSrcDS->GetRasterYSize() * srcGT[2];
        const double dfLRY = srcGT[3] + poSrcDS->GetRasterXSize() * srcGT[4] +
                             poSrcDS->GetRasterYSize() * srcGT[5];
        const double dfX1 = invGT[0] + invGT[1] * dfULX + invGT[2] * dfULY;
        const double dfY1 = invGT[3] + invGT[4] * dfULX + invGT[5] * dfULY;
        const double dfX2 = invGT[0] + invGT[1] * dfLRX + invGT[2] * dfLRY;
        const double dfY2 = invGT[3] + invGT[4] * dfLRX + invGT[5] * dfLRY;
        constexpr double EPS = 1e-8;
        const int nXOff =
            static_cast<int>(std::max(0.0, std::min(dfX1, dfX2)) + EPS);
        const int nYOff =
            static_cast<int>(std::max(0.0, std::min(dfY1, dfY2)) + EPS);
        const int nXSize =
            static_cast<int>(
                std::ceil(std::min(static_cast<double>(poDS->GetRasterXSize()),
                                   std::max(dfX1, dfX2)) -
                          EPS)) -
            nXOff;
        const int nYSize =
            static_cast<int>(
                std::ceil(std::min(static_cast<double>(poDS->GetRasterYSize()),
                                   std::max(dfY1, dfY2)) -
                          EPS)) -
            nYOff;

        dfTotalPixels += static_cast<double>(nXSize) * nYSize;
        Region region;
        region.osFileName = filename;
        region.nXOff = nXOff;
        region.nYOff = nYOff;
        region.nXSize = nXSize;
        region.nYSize = nYSize;
        regions.push_back(std::move(region));
    }

    bool bRet = true;
    double dfCurPixels = 0;
    for (const auto &region : regions)
    {
        if (bRet)
        {
            CPLDebug("GDAL", "Refresh from source %s",
                     region.osFileName.c_str());
            double dfNextCurPixels =
                dfCurPixels +
                static_cast<double>(region.nXSize) * region.nYSize;
            // coverity[divide_by_zero]
            void *pScaledProgress = GDALCreateScaledProgress(
                dfCurPixels / dfTotalPixels, dfNextCurPixels / dfTotalPixels,
                pfnProgress, pProgressArg);
            bRet =
                PartialRefresh(poDS, anOvrIndices, pszResampling, region.nXOff,
                               region.nYOff, region.nXSize, region.nYSize,
                               pScaledProgress ? GDALScaledProgress : nullptr,
                               pScaledProgress);
            GDALDestroyScaledProgress(pScaledProgress);
            dfCurPixels = dfNextCurPixels;
        }
    }

    return bRet;
}

/************************************************************************/
/*                       PartialRefreshFromBBOX()                       */
/************************************************************************/

static bool PartialRefreshFromBBOX(GDALDataset *poDS,
                                   const std::vector<double> &bbox,
                                   const char *pszResampling,
                                   const std::vector<int> &anOvrIndices,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressArg)
{
    const double dfULX = bbox[0];
    const double dfLRY = bbox[1];
    const double dfLRX = bbox[2];
    const double dfULY = bbox[3];

    GDALGeoTransform gt;
    if (poDS->GetGeoTransform(gt) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset has no geotransform");
        return false;
    }
    GDALGeoTransform invGT;
    if (!gt.GetInverse(invGT))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
        return false;
    }
    const double dfX1 = invGT[0] + invGT[1] * dfULX + invGT[2] * dfULY;
    const double dfY1 = invGT[3] + invGT[4] * dfULX + invGT[5] * dfULY;
    const double dfX2 = invGT[0] + invGT[1] * dfLRX + invGT[2] * dfLRY;
    const double dfY2 = invGT[3] + invGT[4] * dfLRX + invGT[5] * dfLRY;
    constexpr double EPS = 1e-8;
    const int nXOff =
        static_cast<int>(std::max(0.0, std::min(dfX1, dfX2)) + EPS);
    const int nYOff =
        static_cast<int>(std::max(0.0, std::min(dfY1, dfY2)) + EPS);
    const int nXSize = static_cast<int>(std::ceil(
                           std::min(static_cast<double>(poDS->GetRasterXSize()),
                                    std::max(dfX1, dfX2)) -
                           EPS)) -
                       nXOff;
    const int nYSize = static_cast<int>(std::ceil(
                           std::min(static_cast<double>(poDS->GetRasterYSize()),
                                    std::max(dfY1, dfY2)) -
                           EPS)) -
                       nYOff;
    return PartialRefresh(poDS, anOvrIndices, pszResampling, nXOff, nYOff,
                          nXSize, nYSize, pfnProgress, pProgressArg);
}

/************************************************************************/
/*            GDALRasterOverviewAlgorithmRefresh::RunImpl()             */
/************************************************************************/

bool GDALRasterOverviewAlgorithmRefresh::RunImpl(GDALProgressFunc pfnProgress,
                                                 void *pProgressData)
{
    auto poDS = m_dataset.GetDatasetRef();
    CPLAssert(poDS);
    if (poDS->GetRasterCount() == 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Dataset has no raster band");
        return false;
    }

    auto poBand = poDS->GetRasterBand(1);
    const int nOvCount = poBand->GetOverviewCount();

    std::vector<int> levels = m_levels;

    // If no levels are specified, reuse the potentially existing ones.
    if (levels.empty())
    {
        for (int iOvr = 0; iOvr < nOvCount; ++iOvr)
        {
            auto poOverview = poBand->GetOverview(iOvr);
            if (poOverview)
            {
                const int nOvFactor = GDALComputeOvFactor(
                    poOverview->GetXSize(), poBand->GetXSize(),
                    poOverview->GetYSize(), poBand->GetYSize());
                levels.push_back(nOvFactor);
            }
        }
    }
    if (levels.empty())
    {
        ReportError(CE_Failure, CPLE_AppDefined, "No overviews to refresh");
        return false;
    }

    std::vector<int> anOvrIndices;
    for (int nLevel : levels)
    {
        int nIdx = -1;
        for (int iOvr = 0; iOvr < nOvCount; iOvr++)
        {
            auto poOverview = poBand->GetOverview(iOvr);
            if (poOverview)
            {
                const int nOvFactor = GDALComputeOvFactor(
                    poOverview->GetXSize(), poBand->GetXSize(),
                    poOverview->GetYSize(), poBand->GetYSize());
                if (nOvFactor == nLevel ||
                    nOvFactor == GDALOvLevelAdjust2(nLevel, poBand->GetXSize(),
                                                    poBand->GetYSize()))
                {
                    nIdx = iOvr;
                    break;
                }
            }
        }
        if (nIdx < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find overview level with subsampling factor of %d",
                     nLevel);
            return false;
        }
        CPLDebug("GDAL", "Refreshing overview idx %d", nIdx);
        anOvrIndices.push_back(nIdx);
    }

    std::string resampling = m_resampling;
    if (resampling.empty())
    {
        const char *pszResampling =
            poBand->GetOverview(0)->GetMetadataItem("RESAMPLING");
        if (pszResampling)
        {
            resampling = pszResampling;
            CPLDebug("GDAL",
                     "Reusing resampling method %s from existing "
                     "overview",
                     pszResampling);
        }
    }
    if (resampling.empty())
        resampling = "nearest";

    if (m_refreshFromSourceTimestamp)
    {
        return PartialRefreshFromSourceTimestamp(
            poDS, resampling.c_str(), anOvrIndices, pfnProgress, pProgressData);
    }
    else if (!m_refreshBbox.empty())
    {
        return PartialRefreshFromBBOX(poDS, m_refreshBbox, resampling.c_str(),
                                      anOvrIndices, pfnProgress, pProgressData);
    }
    else if (!m_like.empty())
    {
        return PartialRefreshFromSourceExtent(poDS, m_like, resampling.c_str(),
                                              anOvrIndices, pfnProgress,
                                              pProgressData);
    }
    else
    {
        return GDALBuildOverviews(
                   GDALDataset::ToHandle(poDS), resampling.c_str(),
                   static_cast<int>(levels.size()), levels.data(), 0, nullptr,
                   pfnProgress, pProgressData) == CE_None;
    }
}

//! @endcond
