/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "resize" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_resize.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALRasterResizeAlgorithm::GDALRasterResizeAlgorithm()        */
/************************************************************************/

GDALRasterResizeAlgorithm::GDALRasterResizeAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("resolution", 0, _("Target resolution (in destination CRS units)"),
           &m_resolution)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetMinValueExcluded(0)
        .SetRepeatedArgAllowed(false)
        .SetDisplayHintAboutRepetition(false)
        .SetMetaVar("<xres>,<yres>")
        .SetMutualExclusionGroup("resolution-size");

    AddArg("size", 0,
           _("Target size in pixels (or percentage if using '%' suffix)"),
           &m_size)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetRequired()
        .SetMinValueIncluded(0)
        .SetRepeatedArgAllowed(false)
        .SetDisplayHintAboutRepetition(false)
        .SetMetaVar("<width[%]>,<height[%]>")
        .SetMutualExclusionGroup("resolution-size")
        .AddValidationAction(
            [this]()
            {
                for (const auto &s : m_size)
                {
                    char *endptr = nullptr;
                    const double val = CPLStrtod(s.c_str(), &endptr);
                    bool ok = false;
                    if (endptr == s.c_str() + s.size())
                    {
                        if (val >= 0 && val <= INT_MAX &&
                            static_cast<int>(val) == val)
                        {
                            ok = true;
                        }
                    }
                    else if (endptr && ((endptr[0] == ' ' && endptr[1] == '%' &&
                                         endptr + 2 == s.c_str() + s.size()) ||
                                        (endptr[0] == '%' &&
                                         endptr + 1 == s.c_str() + s.size())))
                    {
                        if (val >= 0)
                        {
                            ok = true;
                        }
                    }
                    if (!ok)
                    {
                        ReportError(CE_Failure, CPLE_IllegalArg,
                                    "Invalid size value: %s'", s.c_str());
                        return false;
                    }
                }
                return true;
            });
    AddArg("resampling", 'r', _("Resampling method"), &m_resampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average", "mode")
        .SetDefault("nearest")
        .SetHiddenChoices("near");
}

/************************************************************************/
/*                 GDALRasterResizeAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterResizeAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    const auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");
    if (!m_size.empty())
    {
        aosOptions.AddString("-outsize");
        aosOptions.AddString(m_size[0]);
        aosOptions.AddString(m_size[1]);
    }
    if (!m_resolution.empty())
    {
        aosOptions.AddString("-tr");
        aosOptions.AddString(CPLSPrintf("%.17g", m_resolution[0]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_resolution[1]));
    }
    if (!m_resampling.empty())
    {
        aosOptions.AddString("-r");
        aosOptions.AddString(m_resampling.c_str());
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);

    auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALTranslate("", GDALDataset::ToHandle(poSrcDS), psOptions, nullptr)));
    GDALTranslateOptionsFree(psOptions);
    const bool bRet = poOutDS != nullptr;
    if (poOutDS)
    {
        m_outputDataset.Set(std::move(poOutDS));
    }

    return bRet;
}

GDALRasterResizeAlgorithmStandalone::~GDALRasterResizeAlgorithmStandalone() =
    default;

//! @endcond
