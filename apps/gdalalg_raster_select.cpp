/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "select" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_select.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALRasterSelectAlgorithm::GDALRasterSelectAlgorithm()        */
/************************************************************************/

GDALRasterSelectAlgorithm::GDALRasterSelectAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    {
        auto &arg =
            AddArg("band", 'b',
                   _("Band(s) (1-based index, 'mask' or 'mask:<band>')"),
                   &m_bands)
                .SetPositional()
                .SetRequired()
                .SetMinCount(1);
        arg.AddValidationAction(
            [&arg]()
            {
                const auto &val = arg.Get<std::vector<std::string>>();
                for (const auto &v : val)
                {
                    if (!STARTS_WITH(v.c_str(), "mask") &&
                        !(CPLGetValueType(v.c_str()) == CPL_VALUE_INTEGER &&
                          atoi(v.c_str()) >= 1))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Invalid band specification.");
                        return false;
                    }
                }
                return true;
            });
    }
    {
        auto &arg = AddArg(
            "mask", 0,
            _("Mask band (1-based index, 'mask', 'mask:<band>' or 'none')"),
            &m_mask);
        arg.AddValidationAction(
            [&arg]()
            {
                const auto &v = arg.Get<std::string>();
                if (!STARTS_WITH(v.c_str(), "mask") &&
                    !EQUAL(v.c_str(), "none") &&
                    !(CPLGetValueType(v.c_str()) == CPL_VALUE_INTEGER &&
                      atoi(v.c_str()) >= 1))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid mask band specification.");
                    return false;
                }
                return true;
            });
    }
}

/************************************************************************/
/*              GDALRasterSelectAlgorithm::RunStep()                    */
/************************************************************************/

bool GDALRasterSelectAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");
    for (const std::string &v : m_bands)
    {
        aosOptions.AddString("-b");
        aosOptions.AddString(CPLString(v).replaceAll(':', ',').c_str());
    }
    if (!m_mask.empty())
    {
        aosOptions.AddString("-mask");
        aosOptions.AddString(CPLString(m_mask).replaceAll(':', ',').c_str());
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
