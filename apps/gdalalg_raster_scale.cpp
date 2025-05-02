/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "scale" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_scale.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

#include <cmath>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*         GDALRasterScaleAlgorithm::GDALRasterScaleAlgorithm()        */
/************************************************************************/

GDALRasterScaleAlgorithm::GDALRasterScaleAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddOutputDataTypeArg(&m_type);
    AddBandArg(&m_band,
               _("Select band to restrict the scaling (1-based index)"));
    AddArg("src-min", 0, _("Minimum value of the source range"), &m_srcMin);
    AddArg("src-max", 0, _("Maximum value of the source range"), &m_srcMax);
    AddArg("dst-min", 0, _("Minimum value of the destination range"),
           &m_dstMin);
    AddArg("dst-max", 0, _("Maximum value of the destination range"),
           &m_dstMax);
    AddArg("exponent", 0,
           _("Exponent to apply non-linear scaling with a power function"),
           &m_exponent);
    AddArg("no-clip", 0, _("Do not clip input values to [srcmin, srcmax]"),
           &m_noClip);
}

/************************************************************************/
/*               GDALRasterScaleAlgorithm::RunStep()                    */
/************************************************************************/

bool GDALRasterScaleAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");
    if (!m_type.empty())
    {
        aosOptions.AddString("-ot");
        aosOptions.AddString(m_type.c_str());
    }
    aosOptions.AddString(m_band > 0 ? CPLSPrintf("-scale_%d", m_band)
                                    : "-scale");
    if (!std::isnan(m_srcMin))
    {
        if (std::isnan(m_srcMax))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "src-max must be specified when src-min is specified");
            return false;
        }
        aosOptions.AddString(CPLSPrintf("%.17g", m_srcMin));
        aosOptions.AddString(CPLSPrintf("%.17g", m_srcMax));
    }
    else if (!std::isnan(m_srcMax))
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "src-min must be specified when src-max is specified");
        return false;
    }

    if (!std::isnan(m_dstMin))
    {
        if (std::isnan(m_dstMax))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "dst-max must be specified when dst-min is specified");
            return false;
        }
        if (std::isnan(m_srcMin))
        {
            aosOptions.AddString("NaN");
            aosOptions.AddString("NaN");
        }
        aosOptions.AddString(CPLSPrintf("%.17g", m_dstMin));
        aosOptions.AddString(CPLSPrintf("%.17g", m_dstMax));
    }
    else if (!std::isnan(m_dstMax))
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "dst-min must be specified when dst-max is specified");
        return false;
    }

    if (!std::isnan(m_exponent))
    {
        aosOptions.AddString(m_band > 0 ? CPLSPrintf("-exponent_%d", m_band)
                                        : "-exponent");
        aosOptions.AddString(CPLSPrintf("%.17g", m_exponent));
    }
    else if (!m_noClip)
    {
        aosOptions.AddString(m_band > 0 ? CPLSPrintf("-exponent_%d", m_band)
                                        : "-exponent");
        aosOptions.AddString("1");
    }

    if (m_noClip)
    {
        aosOptions.AddString("--no-clip");
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);

    auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALTranslate("", GDALDataset::ToHandle(m_inputDataset.GetDatasetRef()),
                      psOptions, nullptr)));
    GDALTranslateOptionsFree(psOptions);
    const bool bRet = poOutDS != nullptr;
    if (poOutDS)
    {
        m_outputDataset.Set(std::move(poOutDS));
    }

    return bRet;
}

//! @endcond
