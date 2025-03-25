/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster overview delete" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_overview_delete.h"

#include "gdal_priv.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                 GDALRasterOverviewAlgorithmDelete()                  */
/************************************************************************/

GDALRasterOverviewAlgorithmDelete::GDALRasterOverviewAlgorithmDelete()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOpenOptionsArg(&m_openOptions);
    AddArg("dataset", 0,
           _("Dataset (to be updated in-place, unless --read-only)"),
           &m_dataset, GDAL_OF_RASTER | GDAL_OF_UPDATE)
        .SetPositional()
        .SetRequired();
    AddArg("external", 0, _("Delete external overviews"), &m_readOnly)
        .AddHiddenAlias("ro")
        .AddHiddenAlias(GDAL_ARG_NAME_READ_ONLY);
}

/************************************************************************/
/*               GDALRasterOverviewAlgorithmDelete::RunImpl()           */
/************************************************************************/

bool GDALRasterOverviewAlgorithmDelete::RunImpl(GDALProgressFunc pfnProgress,
                                                void *pProgressData)
{
    auto poDS = m_dataset.GetDatasetRef();
    CPLAssert(poDS);

    return GDALBuildOverviews(GDALDataset::ToHandle(poDS), "NONE", 0, nullptr,
                              0, nullptr, pfnProgress,
                              pProgressData) == CE_None;
}

//! @endcond
