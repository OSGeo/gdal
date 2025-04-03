/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "slope" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_slope.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

#include <cmath>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*       GDALRasterSlopeAlgorithm::GDALRasterSlopeAlgorithm()   */
/************************************************************************/

GDALRasterSlopeAlgorithm::GDALRasterSlopeAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    SetOutputVRTCompatible(false);

    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("unit", 0, _("Unit in which to express slopes"), &m_unit)
        .SetChoices("degree", "percent")
        .SetDefault(m_unit);
    AddArg("xscale", 0, _("Ratio of vertical units to horizontal X axis units"),
           &m_xscale)
        .SetMinValueExcluded(0);
    AddArg("yscale", 0, _("Ratio of vertical units to horizontal Y axis units"),
           &m_yscale)
        .SetMinValueExcluded(0);
    AddArg("gradient-alg", 0, _("Algorithm used to compute terrain gradient"),
           &m_gradientAlg)
        .SetChoices("Horn", "ZevenbergenThorne")
        .SetDefault(m_gradientAlg);
    AddArg("no-edges", 0,
           _("Do not try to interpolate values at dataset edges or close to "
             "nodata values"),
           &m_noEdges);
}

/************************************************************************/
/*              GDALRasterSlopeAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterSlopeAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("stream");
    aosOptions.AddString("-b");
    aosOptions.AddString(CPLSPrintf("%d", m_band));
    if (!std::isnan(m_xscale))
    {
        aosOptions.AddString("-xscale");
        aosOptions.AddString(CPLSPrintf("%.17g", m_xscale));
    }
    if (!std::isnan(m_yscale))
    {
        aosOptions.AddString("-yscale");
        aosOptions.AddString(CPLSPrintf("%.17g", m_yscale));
    }
    if (m_unit == "percent")
        aosOptions.AddString("-p");
    aosOptions.AddString("-alg");
    aosOptions.AddString(m_gradientAlg.c_str());

    if (!m_noEdges)
        aosOptions.AddString("-compute_edges");

    GDALDEMProcessingOptions *psOptions =
        GDALDEMProcessingOptionsNew(aosOptions.List(), nullptr);

    auto poOutDS =
        std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALDEMProcessing(
            "", GDALDataset::ToHandle(m_inputDataset.GetDatasetRef()), "slope",
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
