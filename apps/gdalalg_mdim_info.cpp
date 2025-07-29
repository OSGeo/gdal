/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_mdim_info.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*              GDALMdimInfoAlgorithm::GDALMdimInfoAlgorithm()          */
/************************************************************************/

GDALMdimInfoAlgorithm::GDALMdimInfoAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddOutputFormatArg(&m_format).SetHidden().SetDefault("json").SetChoices(
        "json", "text");
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_MULTIDIM_RASTER});
    AddInputDatasetArg(&m_dataset, GDAL_OF_MULTIDIM_RASTER).AddAlias("dataset");
    AddOutputStringArg(&m_output);
    AddArg(
        "detailed", 0,
        _("Most verbose output. Report attribute data types and array values."),
        &m_detailed);
    {
        auto &arg = AddArg("array", 0,
                           _("Name of the array, used to restrict the output "
                             "to the specified array."),
                           &m_array);

        arg.SetAutoCompleteFunction(
            [this](const std::string &)
            {
                std::vector<std::string> ret;

                if (auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                        m_dataset.GetName().c_str(), GDAL_OF_MULTIDIM_RASTER,
                        nullptr, nullptr, nullptr)))
                {
                    if (auto poRG = poDS->GetRootGroup())
                    {
                        ret = poRG->GetMDArrayFullNamesRecursive();
                    }
                }

                return ret;
            });
    }

    AddArg("limit", 0,
           _("Number of values in each dimension that is used to limit the "
             "display of array values."),
           &m_limit);
    {
        auto &arg = AddArg("array-option", 0,
                           _("Option passed to GDALGroup::GetMDArrayNames() to "
                             "filter reported arrays."),
                           &m_arrayOptions)
                        .SetMetaVar("<KEY>=<VALUE>")
                        .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });

        arg.SetAutoCompleteFunction(
            [this](const std::string &currentValue)
            {
                std::vector<std::string> ret;

                if (auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                        m_dataset.GetName().c_str(), GDAL_OF_MULTIDIM_RASTER,
                        nullptr, nullptr, nullptr)))
                {
                    if (auto poDriver = poDS->GetDriver())
                    {
                        if (const char *pszXML = poDriver->GetMetadataItem(
                                GDAL_DMD_MULTIDIM_ARRAY_OPENOPTIONLIST))
                        {
                            AddOptionsSuggestions(pszXML, 0, currentValue, ret);
                        }
                    }
                }

                return ret;
            });
    }
    AddArg("stats", 0, _("Read and display image statistics."), &m_stats);

    AddArg("stdout", 0,
           _("Directly output on stdout. If enabled, "
             "output-string will be empty"),
           &m_stdout)
        .SetHiddenForCLI();
}

/************************************************************************/
/*                   GDALMdimInfoAlgorithm::RunImpl()                   */
/************************************************************************/

bool GDALMdimInfoAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    CPLAssert(m_dataset.GetDatasetRef());

    CPLStringList aosOptions;

    if (m_stdout)
        aosOptions.AddString("-stdout");
    if (m_detailed)
        aosOptions.AddString("-detailed");
    if (m_stats)
        aosOptions.AddString("-stats");
    if (m_limit > 0)
    {
        aosOptions.AddString("-limit");
        aosOptions.AddString(CPLSPrintf("%d", m_limit));
    }
    if (!m_array.empty())
    {
        aosOptions.AddString("-array");
        aosOptions.AddString(m_array.c_str());
    }
    for (const std::string &opt : m_arrayOptions)
    {
        aosOptions.AddString("-arrayoption");
        aosOptions.AddString(opt.c_str());
    }

    GDALDatasetH hDS = GDALDataset::ToHandle(m_dataset.GetDatasetRef());
    GDALMultiDimInfoOptions *psOptions =
        GDALMultiDimInfoOptionsNew(aosOptions.List(), nullptr);
    char *ret = GDALMultiDimInfo(hDS, psOptions);
    GDALMultiDimInfoOptionsFree(psOptions);
    const bool bOK = ret != nullptr;
    if (ret && !m_stdout)
    {
        m_output = ret;
    }
    CPLFree(ret);

    return bOK;
}

//! @endcond
