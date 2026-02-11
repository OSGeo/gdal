/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster update" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_update.h"

#include "cpl_conv.h"

#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdalalg_raster_reproject.h"  // for GDALRasterReprojectUtils
#include "gdalalg_raster_overview_refresh.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALRasterUpdateAlgorithm::GDALRasterUpdateAlgorithm()        */
/************************************************************************/

GDALRasterUpdateAlgorithm::GDALRasterUpdateAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(standaloneStep)
                                          .SetInputDatasetMaxCount(1)
                                          .SetAddDefaultArguments(false)
                                          .SetInputDatasetAlias("dataset"))
{
    AddProgressArg();

    if (standaloneStep)
    {
        AddRasterInputArgs(/* openForMixedRasterVector = */ false,
                           /* hiddenForCLI = */ false);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
    }

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);

    m_update = true;
    AddUpdateArg(&m_update).SetDefault(true).SetHidden();

    AddArg("geometry", 0, _("Clipping geometry (WKT or GeoJSON)"), &m_geometry)
        .SetMutualExclusionGroup("bbox-geometry-like");
    AddArg("geometry-crs", 0, _("CRS of clipping geometry"), &m_geometryCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("geometry_srs");

    GDALRasterReprojectUtils::AddResamplingArg(this, m_resampling);

    GDALRasterReprojectUtils::AddWarpOptTransformOptErrorThresholdArg(
        this, m_warpOptions, m_transformOptions, m_errorThreshold);

    AddArg("no-update-overviews", 0, _("Do not update existing overviews"),
           &m_noUpdateOverviews);
}

/************************************************************************/
/*                 GDALRasterUpdateAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterUpdateAlgorithm::RunStep(GDALPipelineStepRunContext &stepCtxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poDstDS = m_outputDataset.GetDatasetRef();
    CPLAssert(poDstDS);
    CPLAssert(poDstDS->GetAccess() == GA_Update);

    std::unique_ptr<OGRGeometry> poClipGeom;
    std::string errMsg;
    if (!m_geometry.empty())
    {
        std::tie(poClipGeom, errMsg) = GetClipGeometry();
        if (!poClipGeom)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "%s", errMsg.c_str());
            return false;
        }
    }

    auto poSrcDriver = poSrcDS->GetDriver();
    auto poDstDriver = poDstDS->GetDriver();
    if (poSrcDS == poDstDS ||
        (poSrcDriver && poDstDriver &&
         !EQUAL(poSrcDriver->GetDescription(), "MEM") &&
         !EQUAL(poDstDriver->GetDescription(), "MEM") &&
         strcmp(poSrcDS->GetDescription(), poDstDS->GetDescription()) == 0))
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Source and destination datasets must be different");
        return false;
    }

    CPLStringList aosOptions;
    if (!m_resampling.empty())
    {
        aosOptions.AddString("-r");
        aosOptions.AddString(m_resampling.c_str());
    }
    for (const std::string &opt : m_warpOptions)
    {
        aosOptions.AddString("-wo");
        aosOptions.AddString(opt.c_str());
    }
    for (const std::string &opt : m_transformOptions)
    {
        aosOptions.AddString("-to");
        aosOptions.AddString(opt.c_str());
    }
    if (std::isfinite(m_errorThreshold))
    {
        aosOptions.AddString("-et");
        aosOptions.AddString(CPLSPrintf("%.17g", m_errorThreshold));
    }

    if (poClipGeom)
    {
        aosOptions.AddString("-cutline");
        aosOptions.AddString(poClipGeom->exportToWkt());
    }

    bool bOvrCanBeUpdated = false;
    std::vector<double> overviewRefreshBBox;
    if (poDstDS->GetRasterBand(1)->GetOverviewCount() > 0 &&
        !m_noUpdateOverviews)
    {
        GDALGeoTransform gt;
        const auto poSrcCRS = poSrcDS->GetSpatialRef();
        const auto poDstCRS = poDstDS->GetSpatialRef();
        const bool bBothCRS = poSrcCRS && poDstCRS;
        const bool bBothNoCRS = !poSrcCRS && !poDstCRS;
        if ((bBothCRS || bBothNoCRS) && poSrcDS->GetGeoTransform(gt) == CE_None)
        {
            auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                bBothCRS ? OGRCreateCoordinateTransformation(poSrcCRS, poDstCRS)
                         : nullptr);
            if (bBothNoCRS || poCT)
            {
                const double dfTLX = gt.xorig;
                const double dfTLY = gt.yorig;

                double dfTRX = 0;
                double dfTRY = 0;
                gt.Apply(poSrcDS->GetRasterXSize(), 0, &dfTRX, &dfTRY);

                double dfBLX = 0;
                double dfBLY = 0;
                gt.Apply(0, poSrcDS->GetRasterYSize(), &dfBLX, &dfBLY);

                double dfBRX = 0;
                double dfBRY = 0;
                gt.Apply(poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                         &dfBRX, &dfBRY);

                const double dfXMin =
                    std::min(std::min(dfTLX, dfTRX), std::min(dfBLX, dfBRX));
                const double dfYMin =
                    std::min(std::min(dfTLY, dfTRY), std::min(dfBLY, dfBRY));
                const double dfXMax =
                    std::max(std::max(dfTLX, dfTRX), std::max(dfBLX, dfBRX));
                const double dfYMax =
                    std::max(std::max(dfTLY, dfTRY), std::max(dfBLY, dfBRY));
                double dfOutXMin = dfXMin;
                double dfOutYMin = dfYMin;
                double dfOutXMax = dfXMax;
                double dfOutYMax = dfYMax;
                if (!poCT || poCT->TransformBounds(
                                 dfXMin, dfYMin, dfXMax, dfYMax, &dfOutXMin,
                                 &dfOutYMin, &dfOutXMax, &dfOutYMax, 21))
                {
                    bOvrCanBeUpdated = true;
                    CPLDebug("update",
                             "Refresh overviews from (%f,%f) to (%f,%f)",
                             dfOutXMin, dfOutYMin, dfOutXMax, dfOutYMax);
                    overviewRefreshBBox = std::vector<double>{
                        dfOutXMin, dfOutYMin, dfOutXMax, dfOutYMax};
                }
            }
        }
        if (!bOvrCanBeUpdated)
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "Overviews can not be updated");
        }
    }

    bool bOK = false;
    GDALWarpAppOptions *psOptions =
        GDALWarpAppOptionsNew(aosOptions.List(), nullptr);
    if (psOptions)
    {
        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)> pScaledData(
            nullptr, GDALDestroyScaledProgress);
        auto pfnProgress = stepCtxt.m_pfnProgress;
        void *pProgressData = stepCtxt.m_pProgressData;
        if (pfnProgress)
        {
            pScaledData.reset(
                GDALCreateScaledProgress(0.0, bOvrCanBeUpdated ? 0.75 : 1.0,
                                         pfnProgress, pProgressData));
            GDALWarpAppOptionsSetProgress(psOptions, GDALScaledProgress,
                                          pScaledData.get());
        }

        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
        GDALDatasetH hDstDS = GDALDataset::ToHandle(poDstDS);
        auto poRetDS = GDALDataset::FromHandle(
            GDALWarp(nullptr, hDstDS, 1, &hSrcDS, psOptions, nullptr));
        GDALWarpAppOptionsFree(psOptions);

        bOK = poRetDS != nullptr;
        if (bOK && bOvrCanBeUpdated)
        {
            GDALRasterOverviewAlgorithmRefresh refresh;
            refresh.GetArg("dataset")->Set(poRetDS);
            if (!m_resampling.empty())
                refresh.GetArg("resampling")->Set(m_resampling);
            refresh.GetArg("bbox")->Set(overviewRefreshBBox);
            pScaledData.reset(GDALCreateScaledProgress(0.75, 1.0, pfnProgress,
                                                       pProgressData));
            bOK = refresh.Run(pScaledData ? GDALScaledProgress : nullptr,
                              pScaledData.get());
        }
        if (bOK && pfnProgress)
            pfnProgress(1.0, "", pProgressData);
    }

    return bOK;
}

/************************************************************************/
/*                 GDALRasterUpdateAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALRasterUpdateAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                        void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunStep(stepCtxt);
}

GDALRasterUpdateAlgorithmStandalone::~GDALRasterUpdateAlgorithmStandalone() =
    default;

//! @endcond
