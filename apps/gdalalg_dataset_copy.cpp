/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset copy" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_dataset_copy.h"

#include "gdal.h"
#include "gdal_priv.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                GDALDatasetCopyRenameCommonAlgorithm()                */
/************************************************************************/

GDALDatasetCopyRenameCommonAlgorithm::GDALDatasetCopyRenameCommonAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL)
    : GDALAlgorithm(name, description, helpURL)
{
    {
        auto &arg = AddArg("source", 0, _("Source dataset name"), &m_source)
                        .SetPositional()
                        .SetMinCharCount(0)
                        .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, 0);
    }

    {
        auto &arg = AddArg("destination", 0, _("Destination dataset name"),
                           &m_destination)
                        .SetPositional()
                        .SetMinCharCount(0)
                        .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, 0);
    }

    AddOverwriteArg(&m_overwrite);

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
/*           GDALDatasetCopyRenameCommonAlgorithm::RunImpl()            */
/************************************************************************/

bool GDALDatasetCopyRenameCommonAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    const char *pszType = "";
    GDALDriver *poDriver = nullptr;
    if (GDALDoesFileOrDatasetExist(m_destination.c_str(), &pszType, &poDriver))
    {
        if (!m_overwrite)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "%s '%s' already exists. Specify the --overwrite "
                        "option to overwrite it.",
                        pszType, m_destination.c_str());
            return false;
        }
        else if (EQUAL(pszType, "File"))
        {
            VSIUnlink(m_destination.c_str());
        }
        else if (EQUAL(pszType, "Directory"))
        {
            // We don't want the user to accidentally erase a non-GDAL dataset
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Directory '%s' already exists, but is not "
                        "recognized as a valid GDAL dataset. "
                        "Please manually delete it before retrying",
                        m_destination.c_str());
            return false;
        }
        else if (poDriver)
        {
            CPLStringList aosDrivers;
            aosDrivers.AddString(poDriver->GetDescription());
            GDALDriver::QuietDelete(m_destination.c_str(), aosDrivers.List());
        }
    }

    GDALDriverH hDriver = nullptr;
    if (!m_format.empty())
        hDriver = GDALGetDriverByName(m_format.c_str());
    if (GetName() == GDALDatasetCopyAlgorithm::NAME)
    {
        return GDALCopyDatasetFiles(hDriver, m_destination.c_str(),
                                    m_source.c_str()) == CE_None;
    }
    else
    {
        return GDALRenameDataset(hDriver, m_destination.c_str(),
                                 m_source.c_str()) == CE_None;
    }
}

/************************************************************************/
/*         GDALDatasetCopyAlgorithm::GDALDatasetCopyAlgorithm()         */
/************************************************************************/

GDALDatasetCopyAlgorithm::GDALDatasetCopyAlgorithm()
    : GDALDatasetCopyRenameCommonAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
}

GDALDatasetCopyAlgorithm::~GDALDatasetCopyAlgorithm() = default;

//! @endcond
