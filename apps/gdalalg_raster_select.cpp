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

static std::optional<std::vector<int>> ParseBandRange(const std::string &v,
                                                      int nBands)
{
    CPLStringList bandSel = cpl::tokenize_string(v, ":", CSLT_ALLOWEMPTYTOKENS);
    if (bandSel.Count() < 2 || bandSel.Count() > 3)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Invalid value for --band: %s",
                 v.c_str());
        return std::nullopt;
    }
    int nFirst = 1;
    const auto osvFirst = cpl::trim(bandSel[0]);
    if (!osvFirst.empty())
    {
        const auto maybeStart = cpl::strict_parse<int>(osvFirst);
        if (maybeStart.has_value())
        {
            nFirst = maybeStart.value();
            if (nFirst < 0)
            {
                nFirst += nBands + 1;
            }
            if (nFirst > nBands || nFirst <= 0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Invalid band: %s",
                         bandSel[0]);
                return std::nullopt;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Failed to parse start value of --band range: %s",
                     bandSel[0]);
            return std::nullopt;
        }
    }
    int nLast = nBands;
    const auto osvLast = cpl::trim(bandSel[1]);
    if (!osvLast.empty())
    {
        const auto maybeLast = cpl::strict_parse<int>(osvLast);
        if (maybeLast.has_value())
        {
            nLast = maybeLast.value();
            if (nLast < 0)
            {
                nLast += nBands + 1;
            }
            if (nLast > nBands || nLast <= 0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Invalid band: %s",
                         bandSel[1]);
                return std::nullopt;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Failed to parse stop value of --band range: %s",
                     bandSel[1]);
            return std::nullopt;
        }
    }
    int nStep = nFirst < nLast ? 1 : -1;
    if (bandSel.Count() == 3)
    {
        const auto maybeStep = cpl::strict_parse<int>(bandSel[2]);
        if (maybeStep.has_value())
        {
            nStep = maybeStep.value();
        }
        else
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Failed to parse step value of --band range: %s",
                     bandSel[2]);
            return std::nullopt;
        }
    }

    if (nFirst < nLast && nStep <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Step value must be positive");
        return std::nullopt;
    }
    if (nFirst > nLast && nStep >= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Step value must be negative");
        return std::nullopt;
    }

    std::vector<int> ret;

    for (int iBand = nFirst; nStep > 0 ? iBand <= nLast : iBand >= nLast;
         iBand += nStep)
    {
        if (iBand < 1 || iBand > nBands)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid band: %d", iBand);
            return std::nullopt;
        }

        ret.push_back(iBand);
    }

    return ret;
}

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
                        v.find(":") == std::string::npos &&
                        CPLGetValueType(v.c_str()) != CPL_VALUE_INTEGER &&
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
            else if (v.find(':') != std::string::npos)
            {
                const auto &aiBands =
                    ParseBandRange(v, poSrcDS->GetRasterCount());
                if (!aiBands.has_value())
                {
                    return false;
                }
                for (int iBand : aiBands.value())
                {
                    aosOptions.AddString("-b");
                    aosOptions.AddString(std::to_string(iBand));
                }
            }
            else if (cpl::equals_ci(v, "mask"))
            {
                aosOptions.AddString("-b");
                aosOptions.AddString(v);
            }
            else
            {
                const auto maybeBand = cpl::strict_parse<int>(v);
                if (!maybeBand)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg, "Invalid band: %s",
                             v.c_str());
                    return false;
                }
                const int nBands = poSrcDS->GetRasterCount();
                int iBand = maybeBand.value();
                if (iBand < 0)
                {
                    iBand += nBands + 1;
                }

                if (iBand > nBands || iBand < 1)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg, "Invalid band: %s",
                             v.c_str());
                    return false;
                }

                aosOptions.AddString("-b");
                aosOptions.AddString(std::to_string(iBand));
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
