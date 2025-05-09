/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_convert.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALVectorConvertAlgorithm::GDALVectorConvertAlgorithm()    */
/************************************************************************/

GDALVectorConvertAlgorithm::GDALVectorConvertAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_VECTOR);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    AddCreationOptionsArg(&m_creationOptions);
    AddLayerCreationOptionsArg(&m_layerCreationOptions);
    AddOverwriteArg(&m_overwrite);
    auto &updateArg = AddUpdateArg(&m_update);
    AddArg("overwrite-layer", 0,
           _("Whether overwriting existing layer is allowed"),
           &m_overwriteLayer)
        .SetDefault(false)
        .AddValidationAction(
            [&updateArg]()
            {
                updateArg.Set(true);
                return true;
            });
    AddAppendUpdateArg(&m_appendLayer,
                       _("Whether appending to existing layer is allowed"));
    AddArg("input-layer", 'l', _("Input layer name(s)"), &m_inputLayerNames)
        .AddAlias("layer");
    AddArg("output-layer", 0, _("Output layer name"), &m_outputLayerName)
        .AddHiddenAlias("nln");  // For ogr2ogr nostalgic people
}

/************************************************************************/
/*                  GDALVectorConvertAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALVectorConvertAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                         void *pProgressData)
{
    CPLAssert(m_inputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("--invoked-from-gdal-vector-convert");
    if (!m_overwrite)
    {
        aosOptions.AddString("--no-overwrite");
    }
    if (m_overwriteLayer)
    {
        aosOptions.AddString("-overwrite");
    }
    if (m_appendLayer)
    {
        aosOptions.AddString("-append");
    }
    if (!m_outputFormat.empty())
    {
        aosOptions.AddString("-of");
        aosOptions.AddString(m_outputFormat.c_str());
    }
    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-dsco");
        aosOptions.AddString(co.c_str());
    }
    for (const auto &co : m_layerCreationOptions)
    {
        aosOptions.AddString("-lco");
        aosOptions.AddString(co.c_str());
    }
    if (!m_outputLayerName.empty())
    {
        aosOptions.AddString("-nln");
        aosOptions.AddString(m_outputLayerName.c_str());
    }
    if (pfnProgress && pfnProgress != GDALDummyProgress)
    {
        aosOptions.AddString("-progress");
    }

    // Must be last, as positional
    for (const auto &name : m_inputLayerNames)
    {
        aosOptions.AddString(name.c_str());
    }

    std::unique_ptr<GDALVectorTranslateOptions,
                    decltype(&GDALVectorTranslateOptionsFree)>
        psOptions(GDALVectorTranslateOptionsNew(aosOptions.List(), nullptr),
                  GDALVectorTranslateOptionsFree);
    bool bOK = false;
    if (psOptions)
    {
        GDALVectorTranslateOptionsSetProgress(psOptions.get(), pfnProgress,
                                              pProgressData);
        GDALDatasetH hOutDS =
            GDALDataset::ToHandle(m_outputDataset.GetDatasetRef());
        GDALDatasetH hSrcDS =
            GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
        auto poRetDS = GDALDataset::FromHandle(
            GDALVectorTranslate(m_outputDataset.GetName().c_str(), hOutDS, 1,
                                &hSrcDS, psOptions.get(), nullptr));
        bOK = poRetDS != nullptr;
        if (!hOutDS)
        {
            m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
        }
    }

    return bOK;
}

//! @endcond
