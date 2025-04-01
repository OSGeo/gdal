/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "tri" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_tri.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

#include <cmath>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*           GDALRasterTRIAlgorithm::GDALRasterTRIAlgorithm()           */
/************************************************************************/

GDALRasterTRIAlgorithm::GDALRasterTRIAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    SetOutputVRTCompatible(false);

    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("algorithm", 0, _("Algorithm to compute TRI"), &m_algorithm)
        .SetChoices("Riley", "Wilson")
        .SetDefault(m_algorithm);
    AddArg("no-edges", 0,
           _("Do not try to interpolate values at dataset edges or close to "
             "nodata values"),
           &m_noEdges);
}

/************************************************************************/
/*                  GDALRasterTRIAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALRasterTRIAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("stream");
    aosOptions.AddString("-b");
    aosOptions.AddString(CPLSPrintf("%d", m_band));
    aosOptions.AddString("-alg");
    aosOptions.AddString(m_algorithm.c_str());
    if (!m_noEdges)
        aosOptions.AddString("-compute_edges");

    GDALDEMProcessingOptions *psOptions =
        GDALDEMProcessingOptionsNew(aosOptions.List(), nullptr);

    auto poOutDS =
        std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALDEMProcessing(
            "", GDALDataset::ToHandle(m_inputDataset.GetDatasetRef()), "TRI",
            nullptr, psOptions, nullptr)));
    GDALDEMProcessingOptionsFree(psOptions);
    const bool bRet = poOutDS != nullptr;
    if (poOutDS)
    {
        m_outputDataset.Set(std::move(poOutDS));
    }

    return bRet;
}

//! @endcond
