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

#include <map>
#include <set>

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
        auto &arg = AddArg("band", 'b',
                           _("Band(s) (1-based index, 'mask', 'mask:<band>' or "
                             "color interpretation such as 'red')"),
                           &m_bands)
                        .SetPositional()
                        .SetRequired()
                        .SetMinCount(1);
        arg.SetAutoCompleteFunction(
            [this](const std::string &)
            {
                std::vector<std::string> ret;
                std::unique_ptr<GDALDataset> poSrcDSTmp;
                GDALDataset *poSrcDS = m_inputDataset.empty()
                                           ? nullptr
                                           : m_inputDataset[0].GetDatasetRef();
                if (!poSrcDS && !m_inputDataset.empty())
                {
                    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                    poSrcDSTmp.reset(GDALDataset::Open(
                        m_inputDataset[0].GetName().c_str(), GDAL_OF_RASTER));
                    poSrcDS = poSrcDSTmp.get();
                }
                if (poSrcDS)
                {
                    std::set<GDALColorInterp> oSetColorInterp;
                    for (int i = 1; i <= poSrcDS->GetRasterCount(); ++i)
                    {
                        ret.push_back(std::to_string(i));
                        oSetColorInterp.insert(poSrcDS->GetRasterBand(i)
                                                   ->GetColorInterpretation());
                    }
                    ret.push_back("mask");
                    for (const auto eColorInterp : oSetColorInterp)
                    {
                        ret.push_back(CPLString(GDALGetColorInterpretationName(
                                                    eColorInterp))
                                          .tolower());
                    }
                }
                return ret;
            });
        arg.AddValidationAction(
            [&arg]()
            {
                int nColorInterpretations = 0;
                const auto paeColorInterp =
                    GDALGetColorInterpretationList(&nColorInterpretations);
                std::set<std::string> oSetValidColorInterp;
                for (int i = 0; i < nColorInterpretations; ++i)
                    oSetValidColorInterp.insert(
                        CPLString(
                            GDALGetColorInterpretationName(paeColorInterp[i]))
                            .tolower());

                const auto &val = arg.Get<std::vector<std::string>>();
                for (const auto &v : val)
                {
                    if (!STARTS_WITH(v.c_str(), "mask") &&
                        !(CPLGetValueType(v.c_str()) == CPL_VALUE_INTEGER &&
                          atoi(v.c_str()) >= 1) &&
                        !cpl::contains(oSetValidColorInterp,
                                       CPLString(v).tolower()))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Invalid band specification.");
                        return false;
                    }
                }
                return true;
            });
    }

    AddArg("exclude", 0, _("Exclude specified bands"), &m_exclude);

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
/*                 GDALRasterSelectAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterSelectAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    const auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    std::map<GDALColorInterp, std::vector<int>> oMapColorInterpToBands;
    for (int i = 1; i <= poSrcDS->GetRasterCount(); ++i)
    {
        oMapColorInterpToBands[poSrcDS->GetRasterBand(i)
                                   ->GetColorInterpretation()]
            .push_back(i);
    }

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");
    if (m_exclude)
    {
        if (m_bands.size() >= static_cast<size_t>(poSrcDS->GetRasterCount()))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot exclude all input bands");
            return false;
        }

        std::set<int> excludedBandsFromColor;
        for (const std::string &v : m_bands)
        {
            const auto eColorInterp =
                GDALGetColorInterpretationByName(v.c_str());
            if (v == "undefined" || eColorInterp != GCI_Undefined)
            {
                const auto iter = oMapColorInterpToBands.find(eColorInterp);
                if (iter != oMapColorInterpToBands.end())
                {
                    for (const int iBand : iter->second)
                    {
                        excludedBandsFromColor.insert(iBand);
                    }
                }
                // We don't emit a warning if there are no bands matching
                // the color interpretation, because a potential use case
                // could be to run on a set of input files that might have or
                // might not have an alpha band, and remove it.
            }
        }

        for (int i = 1; i <= poSrcDS->GetRasterCount(); ++i)
        {
            const std::string iStr = std::to_string(i);
            if (std::find(m_bands.begin(), m_bands.end(), iStr) ==
                    m_bands.end() &&
                !cpl::contains(excludedBandsFromColor, i))
            {
                aosOptions.AddString("-b");
                aosOptions.AddString(iStr);
            }
        }
    }
    else
    {
        for (const std::string &v : m_bands)
        {
            const auto eColorInterp =
                GDALGetColorInterpretationByName(v.c_str());
            if (v == "undefined" || eColorInterp != GCI_Undefined)
            {
                const auto iter = oMapColorInterpToBands.find(eColorInterp);
                if (iter == oMapColorInterpToBands.end())
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "No band has color interpretation %s",
                                v.c_str());
                    return false;
                }
                for (const int iBand : iter->second)
                {
                    aosOptions.AddString("-b");
                    aosOptions.AddString(std::to_string(iBand));
                }
            }
            else
            {
                aosOptions.AddString("-b");
                aosOptions.AddString(CPLString(v).replaceAll(':', ',').c_str());
            }
        }
    }
    if (!m_mask.empty())
    {
        aosOptions.AddString("-mask");
        aosOptions.AddString(CPLString(m_mask).replaceAll(':', ',').c_str());
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

GDALRasterSelectAlgorithmStandalone::~GDALRasterSelectAlgorithmStandalone() =
    default;

//! @endcond
