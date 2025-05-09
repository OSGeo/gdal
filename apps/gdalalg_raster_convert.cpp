/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_convert.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALRasterConvertAlgorithm::GDALRasterConvertAlgorithm()    */
/************************************************************************/

GDALRasterConvertAlgorithm::GDALRasterConvertAlgorithm(
    bool openForMixedRasterVector)
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_RASTER, GDAL_DCAP_CREATECOPY});
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_inputDataset, openForMixedRasterVector
                                            ? GDAL_OF_RASTER | GDAL_OF_VECTOR
                                            : GDAL_OF_RASTER);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER);
    AddCreationOptionsArg(&m_creationOptions);
    const char *exclusionGroup = "overwrite-append";
    AddOverwriteArg(&m_overwrite).SetMutualExclusionGroup(exclusionGroup);
    AddArg(GDAL_ARG_NAME_APPEND, 0,
           _("Append as a subdataset to existing output"), &m_append)
        .SetDefault(false)
        .SetMutualExclusionGroup(exclusionGroup);
}

/************************************************************************/
/*                  GDALRasterConvertAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALRasterConvertAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
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
    if (!m_overwrite)
    {
        aosOptions.AddString("--no-overwrite");
    }
    if (m_append)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString("APPEND_SUBDATASET=YES");
    }
    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(co.c_str());
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);
    GDALTranslateOptionsSetProgress(psOptions, pfnProgress, pProgressData);

    auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALTranslate(m_outputDataset.GetName().c_str(),
                      GDALDataset::ToHandle(m_inputDataset.GetDatasetRef()),
                      psOptions, nullptr)));
    GDALTranslateOptionsFree(psOptions);
    if (!poOutDS)
        return false;

    m_outputDataset.Set(std::move(poOutDS));

    return true;
}

//! @endcond
