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

#include <cmath>
#include <tuple>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALRasterUpdateAlgorithm::GDALRasterUpdateAlgorithm()        */
/************************************************************************/

GDALRasterUpdateAlgorithm::GDALRasterUpdateAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();

    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER);

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);

    AddUpdateArg(&m_update).SetDefault(true).SetHidden();

    AddArg("geometry", 0, _("Clipping geometry (WKT or GeoJSON)"), &m_geometry)
        .SetMutualExclusionGroup("bbox-geometry-like");
    AddArg("geometry-crs", 0, _("CRS of clipping geometry"), &m_geometryCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("geometry_srs");

    GDALRasterReprojectUtils::AddResamplingArg(this, m_resampling);

    GDALRasterReprojectUtils::AddWarpOptTransformOptErrorThresholdArg(
        this, m_warpOptions, m_transformOptions, m_errorThreshold);
}

/************************************************************************/
/*                GDALRasterUpdateAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALRasterUpdateAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                        void *pProgressData)
{
    auto poSrcDS = m_inputDataset.GetDatasetRef();
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

    bool bOK = false;
    GDALWarpAppOptions *psOptions =
        GDALWarpAppOptionsNew(aosOptions.List(), nullptr);
    if (psOptions)
    {
        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)> pScaledData(
            nullptr, GDALDestroyScaledProgress);
        if (pfnProgress)
        {
            GDALWarpAppOptionsSetProgress(psOptions, pfnProgress, pProgressData);
        }

        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
        GDALDatasetH hDstDS = GDALDataset::ToHandle(poDstDS);
        auto poRetDS = GDALDataset::FromHandle(
            GDALWarp(nullptr, hDstDS, 1, &hSrcDS, psOptions, nullptr));
        GDALWarpAppOptionsFree(psOptions);

        bOK = poRetDS != nullptr;
        if (bOK && pfnProgress)
            pfnProgress(1.0, "", pProgressData);
    }

    return bOK;
}

//! @endcond
