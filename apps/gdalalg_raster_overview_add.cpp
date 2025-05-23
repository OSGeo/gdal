/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster overview add" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_overview_add.h"

#include "cpl_string.h"
#include "gdal_priv.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALRasterOverviewAlgorithmAdd()                  */
/************************************************************************/

GDALRasterOverviewAlgorithmAdd::GDALRasterOverviewAlgorithmAdd()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOpenOptionsArg(&m_openOptions);
    AddArg("dataset", 0,
           _("Dataset (to be updated in-place, unless --external)"), &m_dataset,
           GDAL_OF_RASTER | GDAL_OF_UPDATE)
        .SetPositional()
        .SetRequired();
    AddArg("external", 0, _("Add external overviews"), &m_readOnly)
        .AddHiddenAlias("ro")
        .AddHiddenAlias(GDAL_ARG_NAME_READ_ONLY);

    AddArg("resampling", 'r', _("Resampling method"), &m_resampling)
        .SetChoices("nearest", "average", "cubic", "cubicspline", "lanczos",
                    "bilinear", "gauss", "average_magphase", "rms", "mode")
        .SetHiddenChoices("near", "none");

    AddArg("levels", 0, _("Levels / decimation factors"), &m_levels)
        .SetMinValueIncluded(2);
    AddArg("min-size", 0,
           _("Maximum width or height of the smallest overview level."),
           &m_minSize)
        .SetMinValueIncluded(1);
}

/************************************************************************/
/*                GDALRasterOverviewAlgorithmAdd::RunImpl()             */
/************************************************************************/

bool GDALRasterOverviewAlgorithmAdd::RunImpl(GDALProgressFunc pfnProgress,
                                             void *pProgressData)
{
    auto poDS = m_dataset.GetDatasetRef();
    CPLAssert(poDS);

    std::string resampling = m_resampling;
    if (resampling.empty() && poDS->GetRasterCount() > 0)
    {
        auto poBand = poDS->GetRasterBand(1);
        if (poBand->GetOverviewCount() > 0)
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
    }
    if (resampling.empty())
        resampling = "nearest";

    std::vector<int> levels = m_levels;

    // If no levels are specified, reuse the potentially existing ones.
    if (levels.empty() && poDS->GetRasterCount() > 0)
    {
        auto poBand = poDS->GetRasterBand(1);
        const int nExistingCount = poBand->GetOverviewCount();
        if (nExistingCount > 0)
        {
            for (int iOvr = 0; iOvr < nExistingCount; ++iOvr)
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
    }

    if (levels.empty())
    {
        const int nXSize = poDS->GetRasterXSize();
        const int nYSize = poDS->GetRasterYSize();
        int nOvrFactor = 1;
        while (DIV_ROUND_UP(nXSize, nOvrFactor) > m_minSize ||
               DIV_ROUND_UP(nYSize, nOvrFactor) > m_minSize)
        {
            nOvrFactor *= 2;
            levels.push_back(nOvrFactor);
        }
    }

    return levels.empty() ||
           GDALBuildOverviews(GDALDataset::ToHandle(poDS), resampling.c_str(),
                              static_cast<int>(levels.size()), levels.data(), 0,
                              nullptr, pfnProgress, pProgressData) == CE_None;
}

//! @endcond
