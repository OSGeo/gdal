/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "roughness" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_roughness.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

#include <cmath>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*       GDALRasterRoughnessAlgorithm::GDALRasterRoughnessAlgorithm()   */
/************************************************************************/

GDALRasterRoughnessAlgorithm::GDALRasterRoughnessAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    SetOutputVRTCompatible(false);

    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("no-edges", 0,
           _("Do not try to interpolate values at dataset edges or close to "
             "nodata values"),
           &m_noEdges);
}

/************************************************************************/
/*                GDALRasterRoughnessAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterRoughnessAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("stream");
    aosOptions.AddString("-b");
    aosOptions.AddString(CPLSPrintf("%d", m_band));
    if (!m_noEdges)
        aosOptions.AddString("-compute_edges");

    GDALDEMProcessingOptions *psOptions =
        GDALDEMProcessingOptionsNew(aosOptions.List(), nullptr);

    auto poOutDS =
        std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALDEMProcessing(
            "", GDALDataset::ToHandle(m_inputDataset.GetDatasetRef()),
            "roughness", nullptr, psOptions, nullptr)));
    GDALDEMProcessingOptionsFree(psOptions);
    const bool bRet = poOutDS != nullptr;
    if (poOutDS)
    {
        m_outputDataset.Set(std::move(poOutDS));
    }

    return bRet;
}

//! @endcond
