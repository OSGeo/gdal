/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster overview add" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_overview.h"
#include "gdalalg_raster_overview_add.h"

#include "cpl_string.h"
#include "gdal_priv.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

bool GDALRasterOverviewAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "The Run() method should not be called directly on the \"gdal "
             "raster overview\" program.");
    return false;
}

GDALRasterOverviewAlgorithmStandalone::
    ~GDALRasterOverviewAlgorithmStandalone() = default;

/************************************************************************/
/*                   GDALRasterOverviewAlgorithmAdd()                   */
/************************************************************************/

GDALRasterOverviewAlgorithmAdd::GDALRasterOverviewAlgorithmAdd(
    bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(standaloneStep)
                                          .SetAddDefaultArguments(false))
{
    AddProgressArg();

    AddOpenOptionsArg(&m_openOptions);
    auto &datasetArg =
        AddInputDatasetArg(
            &m_inputDataset, GDAL_OF_RASTER | GDAL_OF_UPDATE,
            /* positionalAndRequired = */ standaloneStep,
            _("Dataset (to be updated in-place, unless --external)"))
            .AddAlias("dataset")
            .SetMaxCount(1);
    if (!standaloneStep)
    {
        datasetArg.SetPositional();
        datasetArg.SetHidden();
    }

    constexpr const char *OVERVIEW_SRC_LEVELS_MUTEX = "overview-src-levels";

    auto &overviewSrcArg =
        AddArg("overview-src", 0, _("Source overview dataset"),
               &m_overviewSources, GDAL_OF_RASTER)
            .SetMutualExclusionGroup(OVERVIEW_SRC_LEVELS_MUTEX);
    SetAutoCompleteFunctionForFilename(overviewSrcArg, GDAL_OF_RASTER);

    if (standaloneStep)
    {
        AddArg("external", 0, _("Add external overviews"), &m_readOnly)
            .AddHiddenAlias("ro")
            .AddHiddenAlias(GDAL_ARG_NAME_READ_ONLY);
    }

    AddArg("resampling", 'r', _("Resampling method"), &m_resampling)
        .SetChoices("nearest", "average", "cubic", "cubicspline", "lanczos",
                    "bilinear", "gauss", "average_magphase", "rms", "mode")
        .SetHiddenChoices("near", "none");

    AddArg("levels", 0, _("Levels / decimation factors"), &m_levels)
        .SetMinValueIncluded(2)
        .SetMutualExclusionGroup(OVERVIEW_SRC_LEVELS_MUTEX);
    AddArg("min-size", 0,
           _("Maximum width or height of the smallest overview level."),
           &m_minSize)
        .SetMinValueIncluded(1);

    if (standaloneStep)
    {
        auto &ovrCreationOptionArg =
            AddArg(GDAL_ARG_NAME_CREATION_OPTION, 0,
                   _("Overview creation option"), &m_creationOptions)
                .AddAlias("co")
                .SetMetaVar("<KEY>=<VALUE>")
                .SetPackedValuesAllowed(false);
        ovrCreationOptionArg.AddValidationAction(
            [this, &ovrCreationOptionArg]()
            { return ParseAndValidateKeyValue(ovrCreationOptionArg); });

        ovrCreationOptionArg.SetAutoCompleteFunction(
            [this](const std::string &currentValue)
            {
                std::vector<std::string> oRet;

                const std::string osDSName = m_inputDataset.size() == 1
                                                 ? m_inputDataset[0].GetName()
                                                 : std::string();
                const std::string osExt = CPLGetExtensionSafe(osDSName.c_str());
                if (!osExt.empty())
                {
                    std::set<std::string> oVisitedExtensions;
                    auto poDM = GetGDALDriverManager();
                    for (int i = 0; i < poDM->GetDriverCount(); ++i)
                    {
                        auto poDriver = poDM->GetDriver(i);
                        if (poDriver->GetMetadataItem(GDAL_DCAP_RASTER))
                        {
                            const char *pszExtensions =
                                poDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS);
                            if (pszExtensions)
                            {
                                const CPLStringList aosExts(
                                    CSLTokenizeString2(pszExtensions, " ", 0));
                                for (const char *pszExt : cpl::Iterate(aosExts))
                                {
                                    if (EQUAL(pszExt, osExt.c_str()) &&
                                        !cpl::contains(oVisitedExtensions,
                                                       pszExt))
                                    {
                                        oVisitedExtensions.insert(pszExt);
                                        if (AddOptionsSuggestions(
                                                poDriver->GetMetadataItem(
                                                    GDAL_DMD_OVERVIEW_CREATIONOPTIONLIST),
                                                GDAL_OF_RASTER, currentValue,
                                                oRet))
                                        {
                                            return oRet;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                return oRet;
            });
    }
}

/************************************************************************/
/*              GDALRasterOverviewAlgorithmAdd::RunStep()               */
/************************************************************************/

bool GDALRasterOverviewAlgorithmAdd::RunStep(GDALPipelineStepRunContext &ctxt)
{
    GDALProgressFunc pfnProgress = ctxt.m_pfnProgress;
    void *pProgressData = ctxt.m_pProgressData;
    auto poDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poDS);

    CPLStringList aosOptions(m_creationOptions);
    if (m_readOnly)
    {
        auto poDriver = poDS->GetDriver();
        if (poDriver)
        {
            const char *pszOptionList =
                poDriver->GetMetadataItem(GDAL_DMD_OVERVIEW_CREATIONOPTIONLIST);
            if (pszOptionList)
            {
                if (strstr(pszOptionList, "<Value>EXTERNAL</Value>") == nullptr)
                {
                    ReportError(CE_Failure, CPLE_NotSupported,
                                "Driver %s does not support external overviews",
                                poDriver->GetDescription());
                    return false;
                }
                else if (aosOptions.FetchNameValue("LOCATION") == nullptr)
                {
                    aosOptions.SetNameValue("LOCATION", "EXTERNAL");
                }
            }
        }
    }

    std::string resampling = m_resampling;
    if (resampling.empty() && poDS->GetRasterCount() > 0)
    {
        auto poBand = poDS->GetRasterBand(1);
        if (poBand->GetOverviewCount() > 0)
        {
            const char *pszResampling =
                poBand->GetOverview(0)->GetMetadataItem("RESAMPLING");
            if (pszResampling)
            {
                resampling = pszResampling;
                CPLDebug("GDAL",
                         "Reusing resampling method %s from existing "
                         "overview",
                         pszResampling);
            }
        }
    }
    if (resampling.empty())
        resampling = "nearest";

    if (!m_overviewSources.empty())
    {
        std::vector<GDALDataset *> apoDS;
        for (auto &val : m_overviewSources)
        {
            CPLAssert(val.GetDatasetRef());
            apoDS.push_back(val.GetDatasetRef());
        }
        return poDS->AddOverviews(apoDS, pfnProgress, pProgressData, nullptr) ==
               CE_None;
    }

    std::vector<int> levels = m_levels;

    // If no levels are specified, reuse the potentially existing ones.
    if (levels.empty() && poDS->GetRasterCount() > 0)
    {
        auto poBand = poDS->GetRasterBand(1);
        const int nExistingCount = poBand->GetOverviewCount();
        if (nExistingCount > 0)
        {
            for (int iOvr = 0; iOvr < nExistingCount; ++iOvr)
            {
                auto poOverview = poBand->GetOverview(iOvr);
                if (poOverview)
                {
                    const int nOvFactor = GDALComputeOvFactor(
                        poOverview->GetXSize(), poBand->GetXSize(),
                        poOverview->GetYSize(), poBand->GetYSize());
                    levels.push_back(nOvFactor);
                }
            }
        }
    }

    if (levels.empty())
    {
        const int nXSize = poDS->GetRasterXSize();
        const int nYSize = poDS->GetRasterYSize();
        int nOvrFactor = 1;
        while (DIV_ROUND_UP(nXSize, nOvrFactor) > m_minSize ||
               DIV_ROUND_UP(nYSize, nOvrFactor) > m_minSize)
        {
            nOvrFactor *= 2;
            levels.push_back(nOvrFactor);
        }
    }

    if (!m_standaloneStep && !levels.empty())
    {
        auto poVRTDriver = GetGDALDriverManager()->GetDriverByName("VRT");
        if (!poVRTDriver)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "VRT driver not available");
            return false;
        }
        auto poVRTDS = std::unique_ptr<GDALDataset>(poVRTDriver->CreateCopy(
            "", poDS, false, nullptr, nullptr, nullptr));
        bool bRet = poVRTDS != nullptr;
        if (bRet)
        {
            aosOptions.SetNameValue("VIRTUAL", "YES");
            bRet = GDALBuildOverviewsEx(
                       GDALDataset::ToHandle(poVRTDS.get()), resampling.c_str(),
                       static_cast<int>(levels.size()), levels.data(), 0,
                       nullptr, nullptr, nullptr, aosOptions.List()) == CE_None;
            if (bRet)
                m_outputDataset.Set(std::move(poVRTDS));
        }
        return bRet;
    }
    else
    {
        const auto ret =
            levels.empty() ||
            GDALBuildOverviewsEx(
                GDALDataset::ToHandle(poDS), resampling.c_str(),
                static_cast<int>(levels.size()), levels.data(), 0, nullptr,
                pfnProgress, pProgressData, aosOptions.List()) == CE_None;
        if (ret)
            m_outputDataset.Set(poDS);
        return ret;
    }
}

/************************************************************************/
/*              GDALRasterOverviewAlgorithmAdd::RunImpl()               */
/************************************************************************/

bool GDALRasterOverviewAlgorithmAdd::RunImpl(GDALProgressFunc pfnProgress,
                                             void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunStep(stepCtxt);
}

GDALRasterOverviewAlgorithmAddStandalone::
    ~GDALRasterOverviewAlgorithmAddStandalone() = default;

//! @endcond
