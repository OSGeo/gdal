/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_mdim_convert.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*            GDALMdimConvertAlgorithm::GDALMdimConvertAlgorithm()      */
/************************************************************************/

GDALMdimConvertAlgorithm::GDALMdimConvertAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_CREATE_MULTIDIMENSIONAL});
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_ALG_DCAP_RASTER_OR_MULTIDIM_RASTER});
    AddInputDatasetArg(&m_inputDataset,
                       GDAL_OF_RASTER | GDAL_OF_MULTIDIM_RASTER);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_MULTIDIM_RASTER);
    AddCreationOptionsArg(&m_creationOptions);
    AddOverwriteArg(&m_overwrite);

    {
        auto &arg = AddArg("array", 0,
                           _("Select a single array instead of converting the "
                             "whole dataset."),
                           &m_arrays)
                        .SetMetaVar("<ARRAY-SPEC>")
                        .SetPackedValuesAllowed(false);

        arg.SetAutoCompleteFunction(
            [this](const std::string &)
            {
                std::vector<std::string> ret;

                if (auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                        m_inputDataset.GetName().c_str(),
                        GDAL_OF_MULTIDIM_RASTER, nullptr, nullptr, nullptr)))
                {
                    if (auto poRG = poDS->GetRootGroup())
                    {
                        ret = poRG->GetMDArrayFullNamesRecursive();
                    }
                }

                return ret;
            });
    }

    {
        auto &arg = AddArg("array-option", 0,
                           _("Option passed to GDALGroup::GetMDArrayNames() to "
                             "filter arrays."),
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
                        m_inputDataset.GetName().c_str(),
                        GDAL_OF_MULTIDIM_RASTER, nullptr, nullptr, nullptr)))
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

    AddArg("group", 0,
           _("Select a single group instead of converting the whole dataset."),
           &m_groups)
        .SetMetaVar("<GROUP-SPEC>");

    AddArg("subset", 0, _("Select a subset of the data."), &m_subsets)
        .SetMetaVar("<SUBSET-SPEC>");

    AddArg("scale-axes", 0,
           _("Applies a integral scale factor to one or several dimensions"),
           &m_scaleAxes)
        .SetMetaVar("<SCALEAXES-SPEC>");

    AddArg("strict", 0, _("Turn warnings into failures."), &m_strict);
}

/************************************************************************/
/*                   GDALMdimConvertAlgorithm::RunImpl()                */
/************************************************************************/

bool GDALMdimConvertAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                       void *pProgressData)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    if (!m_outputFormat.empty())
    {
        aosOptions.AddString("-of");
        aosOptions.AddString(m_outputFormat.c_str());
    }
    if (m_overwrite)
    {
        aosOptions.AddString("--overwrite");
    }
    else
    {
        aosOptions.AddString("--no-overwrite");
    }
    if (m_strict)
    {
        aosOptions.AddString("-strict");
    }
    for (const auto &array : m_arrays)
    {
        aosOptions.AddString("-array");
        aosOptions.AddString(array.c_str());
    }
    for (const auto &opt : m_arrayOptions)
    {
        aosOptions.AddString("-arrayoption");
        aosOptions.AddString(opt.c_str());
    }
    for (const auto &group : m_groups)
    {
        aosOptions.AddString("-group");
        aosOptions.AddString(group.c_str());
    }
    for (const auto &subset : m_subsets)
    {
        aosOptions.AddString("-subset");
        aosOptions.AddString(subset.c_str());
    }

    std::string scaleAxes;
    for (const auto &scaleAxis : m_scaleAxes)
    {
        if (!scaleAxes.empty())
            scaleAxes += ',';
        scaleAxes += scaleAxis;
    }
    if (!scaleAxes.empty())
    {
        aosOptions.AddString("-scaleaxes");
        aosOptions.AddString(scaleAxes.c_str());
    }

    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(co.c_str());
    }

    GDALMultiDimTranslateOptions *psOptions =
        GDALMultiDimTranslateOptionsNew(aosOptions.List(), nullptr);
    GDALMultiDimTranslateOptionsSetProgress(psOptions, pfnProgress,
                                            pProgressData);

    auto hSrcDS = GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
    auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALMultiDimTranslate(m_outputDataset.GetName().c_str(), nullptr, 1,
                              &hSrcDS, psOptions, nullptr)));
    GDALMultiDimTranslateOptionsFree(psOptions);
    if (!poOutDS)
        return false;

    m_outputDataset.Set(std::move(poOutDS));

    return true;
}

//! @endcond
