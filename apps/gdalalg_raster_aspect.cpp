/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "aspect" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_aspect.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

#include <cmath>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*         GDALRasterAspectAlgorithm::GDALRasterAspectAlgorithm()       */
/************************************************************************/

GDALRasterAspectAlgorithm::GDALRasterAspectAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    SetOutputVRTCompatible(false);

    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("convention", 0, _("Convention for output angles"), &m_convention)
        .SetChoices("azimuth", "trigonometric-angle")
        .SetDefault(m_convention);
    AddArg("gradient-alg", 0, _("Algorithm used to compute terrain gradient"),
           &m_gradientAlg)
        .SetChoices("Horn", "ZevenbergenThorne")
        .SetDefault(m_gradientAlg);
    AddArg("zero-for-flat", 0, _("Whether to output zero for flat areas"),
           &m_zeroForFlat);
    AddArg("no-edges", 0,
           _("Do not try to interpolate values at dataset edges or close to "
             "nodata values"),
           &m_noEdges);
}

/************************************************************************/
/*                GDALRasterAspectAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALRasterAspectAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("stream");
    aosOptions.AddString("-b");
    aosOptions.AddString(CPLSPrintf("%d", m_band));
    if (m_convention == "trigonometric-angle")
        aosOptions.AddString("-trigonometric");
    aosOptions.AddString("-alg");
    aosOptions.AddString(m_gradientAlg.c_str());
    if (m_zeroForFlat)
        aosOptions.AddString("-zero_for_flat");
    if (!m_noEdges)
        aosOptions.AddString("-compute_edges");

    GDALDEMProcessingOptions *psOptions =
        GDALDEMProcessingOptionsNew(aosOptions.List(), nullptr);

    auto poOutDS =
        std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALDEMProcessing(
            "", GDALDataset::ToHandle(m_inputDataset.GetDatasetRef()), "aspect",
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
