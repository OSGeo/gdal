/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector rasterize" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_rasterize.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALVectorRasterizeAlgorithm::GDALVectorRasterizeAlgorithm()    */
/************************************************************************/

GDALVectorRasterizeAlgorithm::GDALVectorRasterizeAlgorithm()
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
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR);
    m_outputDataset.SetInputFlags(GADV_NAME | GADV_OBJECT);
    AddCreationOptionsArg(&m_datasetCreationOptions);
    AddLayerCreationOptionsArg(&m_layerCreationOptions);
    AddArg("band", 'b', _("The band(s) to burn values into."), &m_bands);
    AddArg("invert", 0, _("Invert the rasterization"), &m_invert)
        .SetDefault(false);
    AddArg("all-touched", 0, _("Enables the ALL_TOUCHED rasterization option"),
           &m_allTouched);
    AddArg("burn-value", 0, _("Burn value"), &m_burnValues);
    AddArg("attribute-name", 'a', _("Attribute name"), &m_attributeName);
    AddArg("3d", 0,
           _("Indicates that a burn value should be extracted from the " Z
             " values of the feature."),
           &m_3d);
    AddArg("add", 0, _("Add to existing raster"), &m_add).SetDefault(false);
    AddArg("layer-name", 'l', _("Layer name"), &m_layerName)
        .AddAlias("layer")
        .SetMutualExclusionGroup("layer-name-or-sql");
    AddArg("where", 0, _("SQL where clause"), &m_where);
    AddArg("sql", 0, _("SQL where clause"), &m_sql)
        .SetMutualExclusionGroup("layer-name-or-sql");
    AddArg("dialect", 0, _("SQL dialect"), &m_dialect);
    AddArg("nodata", 0, _("Assign a specified nodata value to output bands"),
           &m_nodata);
    AddArg("init", 0, _("Pre-initialize output bands with specified value"),
           &m_init);
    AddArg("srs", 0, _("Override the projection for the output file"), &m_srs);
    AddArg("transformer-option", 0,
           _("Set a transformer option suitable to pass to "
             "GDALCreateGenImgProjTransformer2"),
           &m_transformerOption);
}

/************************************************************************/
/*                  GDALVectorRasterizeAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALVectorRasterizeAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    CPLAssert(m_inputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("--invoked-from-gdal-vector-rasterize");
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

    GDALVectorTranslateOptions *psOptions =
        GDALVectorTranslateOptionsNew(aosOptions.List(), nullptr);

    GDALVectorTranslateOptionsSetProgress(psOptions, pfnProgress,
                                          pProgressData);

    GDALDatasetH hOutDS =
        GDALDataset::ToHandle(m_outputDataset.GetDatasetRef());
    GDALDatasetH hSrcDS = GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
    auto poRetDS = GDALDataset::FromHandle(
        GDALVectorTranslate(m_outputDataset.GetName().c_str(), hOutDS, 1,
                            &hSrcDS, psOptions, nullptr));
    GDALVectorTranslateOptionsFree(psOptions);
    if (!poRetDS)
        return false;

    if (!hOutDS)
    {
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
    }

    return true;
}

//! @endcond
