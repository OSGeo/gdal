/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "hillshade" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_hillshade.h"
#include "gdalalg_raster_write.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

#include <cmath>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*       GDALRasterHillshadeAlgorithm::GDALRasterHillshadeAlgorithm()   */
/************************************************************************/

GDALRasterHillshadeAlgorithm::GDALRasterHillshadeAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    SetOutputVRTCompatible(false);

    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("zfactor", 'z',
           _("Vertical exaggeration used to pre-multiply the elevations"),
           &m_zfactor)
        .SetMinValueExcluded(0);
    AddArg("xscale", 0, _("Ratio of vertical units to horizontal X axis units"),
           &m_xscale)
        .SetMinValueExcluded(0);
    AddArg("yscale", 0, _("Ratio of vertical units to horizontal Y axis units"),
           &m_yscale)
        .SetMinValueExcluded(0);
    AddArg("azimuth", 0, _("Azimuth of the light, in degrees"), &m_azimuth)
        .SetDefault(m_azimuth);
    AddArg("altitude", 0, _("Altitude of the light, in degrees"), &m_altitude)
        .SetDefault(m_altitude)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(90);
    AddArg("gradient-alg", 0, _("Algorithm used to compute terrain gradient"),
           &m_gradientAlg)
        .SetChoices("Horn", "ZevenbergenThorne")
        .SetDefault(m_gradientAlg);
    AddArg("variant", 0, _("Variant of the hillshading algorithm"), &m_variant)
        .SetChoices("regular", "combined", "multidirectional", "Igor")
        .SetDefault(m_variant);
    AddArg("no-edges", 0,
           _("Do not try to interpolate values at dataset edges or close to "
             "nodata values"),
           &m_noEdges);
}

/************************************************************************/
/*          GDALRasterHillshadeAlgorithm::CanHandleNextStep()           */
/************************************************************************/

bool GDALRasterHillshadeAlgorithm::CanHandleNextStep(
    GDALPipelineStepAlgorithm *poNextStep) const
{
    return poNextStep->GetName() == GDALRasterWriteAlgorithm::NAME &&
           poNextStep->GetOutputFormat() != "stream";
}

/************************************************************************/
/*              GDALRasterHillshadeAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterHillshadeAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    std::string outputFilename;
    if (ctxt.m_poNextUsableStep)
    {
        CPLAssert(CanHandleNextStep(ctxt.m_poNextUsableStep));
        outputFilename = ctxt.m_poNextUsableStep->GetOutputDataset().GetName();
        const auto &format = ctxt.m_poNextUsableStep->GetOutputFormat();
        if (!format.empty())
        {
            aosOptions.AddString("-of");
            aosOptions.AddString(format.c_str());
        }

        for (const std::string &co :
             ctxt.m_poNextUsableStep->GetCreationOptions())
        {
            aosOptions.AddString("-co");
            aosOptions.AddString(co.c_str());
        }
    }
    else
    {
        aosOptions.AddString("-of");
        aosOptions.AddString("stream");
    }

    aosOptions.AddString("-b");
    aosOptions.AddString(CPLSPrintf("%d", m_band));
    aosOptions.AddString("-z");
    aosOptions.AddString(CPLSPrintf("%.17g", m_zfactor));
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
    if (m_variant == "multidirectional")
    {
        if (GetArg("azimuth")->IsExplicitlySet())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'azimuth' argument cannot be used with multidirectional "
                     "variant");
            return false;
        }
    }
    else
    {
        aosOptions.AddString("-az");
        aosOptions.AddString(CPLSPrintf("%.17g", m_azimuth));
    }
    if (m_variant == "Igor")
    {
        if (GetArg("altitude")->IsExplicitlySet())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'altitude' argument cannot be used with Igor variant");
            return false;
        }
    }
    else
    {
        aosOptions.AddString("-alt");
        aosOptions.AddString(CPLSPrintf("%.17g", m_altitude));
    }
    aosOptions.AddString("-alg");
    aosOptions.AddString(m_gradientAlg.c_str());

    if (m_variant == "combined")
        aosOptions.AddString("-combined");
    else if (m_variant == "multidirectional")
        aosOptions.AddString("-multidirectional");
    else if (m_variant == "Igor")
        aosOptions.AddString("-igor");

    if (!m_noEdges)
        aosOptions.AddString("-compute_edges");

    GDALDEMProcessingOptions *psOptions =
        GDALDEMProcessingOptionsNew(aosOptions.List(), nullptr);
    bool bOK = false;
    if (psOptions)
    {
        if (ctxt.m_poNextUsableStep)
        {
            GDALDEMProcessingOptionsSetProgress(psOptions, ctxt.m_pfnProgress,
                                                ctxt.m_pProgressData);
        }
        auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
            GDALDEMProcessing(outputFilename.c_str(),
                              GDALDataset::ToHandle(poSrcDS), "hillshade",
                              nullptr, psOptions, nullptr)));
        GDALDEMProcessingOptionsFree(psOptions);
        bOK = poOutDS != nullptr;
        if (poOutDS)
        {
            m_outputDataset.Set(std::move(poOutDS));
        }
    }

    return bOK;
}

GDALRasterHillshadeAlgorithmStandalone::
    ~GDALRasterHillshadeAlgorithmStandalone() = default;

//! @endcond
