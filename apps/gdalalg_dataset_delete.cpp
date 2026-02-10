/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset delete" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_dataset_delete.h"

#include "gdal.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                     GDALDatasetDeleteAlgorithm()                     */
/************************************************************************/

GDALDatasetDeleteAlgorithm::GDALDatasetDeleteAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    {
        auto &arg =
            AddArg("filename", 0, _("File or directory name"), &m_filename)
                .SetPositional()
                .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, 0);
    }

    {
        auto &arg =
            AddArg("format", 'f', _("Dataset format"), &m_format)
                .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_OPEN})
                .SetCategory(GAAC_ADVANCED);
        arg.AddValidationAction([this, &arg]()
                                { return ValidateFormat(arg, false, false); });
        arg.SetAutoCompleteFunction(
            [&arg](const std::string &)
            {
                return GDALAlgorithm::FormatAutoCompleteFunction(arg, false,
                                                                 false);
            });
    }
}

/************************************************************************/
/*                GDALDatasetDeleteAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALDatasetDeleteAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    GDALDriverH hDriver = nullptr;
    if (!m_format.empty())
        hDriver = GDALGetDriverByName(m_format.c_str());

    bool ret = true;
    for (const auto &datasetName : m_filename)
    {
        ret = ret && GDALDeleteDataset(hDriver, datasetName.c_str()) == CE_None;
    }

    return ret;
}

//! @endcond
