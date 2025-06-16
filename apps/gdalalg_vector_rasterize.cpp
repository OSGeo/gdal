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

#include <cmath>

#include "gdalalg_vector_rasterize.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALVectorRasterizeAlgorithm::GDALVectorRasterizeAlgorithm()  */
/************************************************************************/

GDALVectorRasterizeAlgorithm::GDALVectorRasterizeAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_RASTER, GDAL_DCAP_CREATE})
        .AddMetadataItem(GAAMDI_VRT_COMPATIBLE, {"false"});
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_VECTOR);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    AddCreationOptionsArg(&m_datasetCreationOptions);
    AddBandArg(&m_bands, _("The band(s) to burn values into (1-based index)"));
    AddArg("invert", 0, _("Invert the rasterization"), &m_invert)
        .SetDefault(false);
    AddArg("all-touched", 0, _("Enables the ALL_TOUCHED rasterization option"),
           &m_allTouched);
    AddArg("burn", 0, _("Burn value"), &m_burnValues);
    AddArg("attribute-name", 'a', _("Attribute name"), &m_attributeName);
    AddArg("3d", 0,
           _("Indicates that a burn value should be extracted from the Z"
             " values of the feature"),
           &m_3d);
    auto &addArg =
        AddArg("add", 0, _("Add to existing raster"), &m_add).SetDefault(false);
    AddArg("layer-name", 'l', _("Layer name"), &m_layerName)
        .AddAlias("layer")
        .SetMutualExclusionGroup("layer-name-or-sql");
    AddArg("where", 0, _("SQL where clause"), &m_where);
    AddArg("sql", 0, _("SQL select statement"), &m_sql)
        .SetMutualExclusionGroup("layer-name-or-sql");
    AddArg("dialect", 0, _("SQL dialect"), &m_dialect);
    AddArg("nodata", 0, _("Assign a specified nodata value to output bands"),
           &m_nodata);
    AddArg("init", 0, _("Pre-initialize output bands with specified value"),
           &m_initValues);
    AddArg("crs", 0, _("Override the projection for the output file"), &m_srs)
        .AddHiddenAlias("srs")
        .SetIsCRSArg(/*noneAllowed=*/false);
    AddArg("transformer-option", 0,
           _("Set a transformer option suitable to pass to "
             "GDALCreateGenImgProjTransformer2"),
           &m_transformerOption)
        .SetMetaVar("<NAME>=<VALUE>");
    AddArg("extent", 0, _("Set the target georeferenced extent"),
           &m_targetExtent)
        .SetMinCount(4)
        .SetMaxCount(4)
        .SetRepeatedArgAllowed(false)
        .SetMetaVar("<xmin>,<ymin>,<xmax>,<ymax>");
    AddArg("resolution", 0, _("Set the target resolution"), &m_targetResolution)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetRepeatedArgAllowed(false)
        .SetMetaVar("<xres>,<yres>")
        .SetMutualExclusionGroup("size-or-resolution");
    AddArg("target-aligned-pixels", 0,
           _("(target aligned pixels) Align the coordinates of the extent of "
             "the output file to the values of the resolution"),
           &m_tap)
        .AddAlias("tap");
    AddArg("size", 0, _("Set the target size in pixels and lines"),
           &m_targetSize)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetRepeatedArgAllowed(false)
        .SetMetaVar("<xsize>,<ysize>")
        .SetMutualExclusionGroup("size-or-resolution");
    AddOutputDataTypeArg(&m_outputType);
    AddArg("optimization", 0,
           _("Force the algorithm used (results are identical)"),
           &m_optimization)
        .SetChoices("AUTO", "RASTER", "VECTOR")
        .SetDefault("AUTO");
    auto &updateArg = AddUpdateArg(&m_update);
    AddOverwriteArg(&m_overwrite);
    addArg.AddValidationAction(
        [&updateArg]()
        {
            updateArg.Set(true);
            return true;
        });
}

/************************************************************************/
/*                GDALVectorRasterizeAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALVectorRasterizeAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    CPLAssert(m_inputDataset.GetDatasetRef());

    CPLStringList aosOptions;

    if (m_bands.size())
    {
        for (int band : m_bands)
        {
            aosOptions.AddString("-b");
            aosOptions.AddString(CPLSPrintf("%d", band));
        }
    }

    if (m_invert)
    {
        aosOptions.AddString("-i");
    }

    if (m_allTouched)
    {
        aosOptions.AddString("-at");
    }

    if (m_burnValues.size())
    {
        for (double burnValue : m_burnValues)
        {
            aosOptions.AddString("-burn");
            aosOptions.AddString(CPLSPrintf("%.17g", burnValue));
        }
    }

    if (!m_attributeName.empty())
    {
        aosOptions.AddString("-a");
        aosOptions.AddString(m_attributeName.c_str());
    }

    if (m_3d)
    {
        aosOptions.AddString("-3d");
    }

    if (m_add)
    {
        aosOptions.AddString("-add");
        // Implies update
        m_update = true;
    }

    if (!m_layerName.empty())
    {
        aosOptions.AddString("-l");
        aosOptions.AddString(m_layerName.c_str());
    }

    if (!m_where.empty())
    {
        aosOptions.AddString("-where");
        aosOptions.AddString(m_where.c_str());
    }

    if (!m_sql.empty())
    {
        aosOptions.AddString("-sql");
        aosOptions.AddString(m_sql.c_str());
    }

    if (!m_dialect.empty())
    {
        aosOptions.AddString("-dialect");
        aosOptions.AddString(m_dialect.c_str());
    }

    if (!m_outputFormat.empty())
    {
        aosOptions.AddString("-of");
        aosOptions.AddString(m_outputFormat.c_str());
    }

    if (!std::isnan(m_nodata))
    {
        aosOptions.AddString("-a_nodata");
        aosOptions.AddString(CPLSPrintf("%.17g", m_nodata));
    }

    if (m_initValues.size())
    {
        for (double initValue : m_initValues)
        {
            aosOptions.AddString("-init");
            aosOptions.AddString(CPLSPrintf("%.17g", initValue));
        }
    }

    if (!m_srs.empty())
    {
        aosOptions.AddString("-a_srs");
        aosOptions.AddString(m_srs.c_str());
    }

    if (m_transformerOption.size())
    {
        for (const auto &to : m_transformerOption)
        {
            aosOptions.AddString("-to");
            aosOptions.AddString(to.c_str());
        }
    }

    for (const auto &co : m_datasetCreationOptions)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(co.c_str());
    }

    if (m_targetExtent.size())
    {
        aosOptions.AddString("-te");
        for (double targetExtent : m_targetExtent)
        {
            aosOptions.AddString(CPLSPrintf("%.17g", targetExtent));
        }
    }

    if (m_targetResolution.size())
    {
        aosOptions.AddString("-tr");
        for (double targetResolution : m_targetResolution)
        {
            aosOptions.AddString(CPLSPrintf("%.17g", targetResolution));
        }
    }

    if (m_tap)
    {
        aosOptions.AddString("-tap");
    }

    if (m_targetSize.size())
    {
        aosOptions.AddString("-ts");
        for (int targetSize : m_targetSize)
        {
            aosOptions.AddString(CPLSPrintf("%d", targetSize));
        }
    }

    if (!m_outputType.empty())
    {
        aosOptions.AddString("-ot");
        aosOptions.AddString(m_outputType.c_str());
    }

    if (!m_optimization.empty())
    {
        aosOptions.AddString("-optim");
        aosOptions.AddString(m_optimization.c_str());
    }

    bool bOK = false;
    std::unique_ptr<GDALRasterizeOptions, decltype(&GDALRasterizeOptionsFree)>
        psOptions{GDALRasterizeOptionsNew(aosOptions.List(), nullptr),
                  GDALRasterizeOptionsFree};
    if (psOptions)
    {
        GDALRasterizeOptionsSetProgress(psOptions.get(), pfnProgress,
                                        pProgressData);

        GDALDatasetH hDstDS =
            GDALDataset::ToHandle(m_outputDataset.GetDatasetRef());

        GDALDatasetH hSrcDS =
            GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
        auto poRetDS = GDALDataset::FromHandle(
            GDALRasterize(m_outputDataset.GetName().c_str(), hDstDS, hSrcDS,
                          psOptions.get(), nullptr));
        bOK = poRetDS != nullptr;

        if (!hDstDS)
        {
            m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
        }
    }

    return bOK;
}

//! @endcond
