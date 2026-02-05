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
#include "gdalalg_raster_write.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*     GDALVectorRasterizeAlgorithm::GDALVectorRasterizeAlgorithm()     */
/************************************************************************/

GDALVectorRasterizeAlgorithm::GDALVectorRasterizeAlgorithm(bool bStandaloneStep)
    : GDALPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(bStandaloneStep)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE))
{
    AddProgressArg();
    if (bStandaloneStep)
    {
        AddOutputFormatArg(&m_format)
            .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                             {GDAL_DCAP_RASTER, GDAL_DCAP_CREATE})
            .AddMetadataItem(GAAMDI_VRT_COMPATIBLE, {"false"});
        AddOpenOptionsArg(&m_openOptions);
        AddInputFormatsArg(&m_inputFormats)
            .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR});
        AddInputDatasetArg(&m_inputDataset, GDAL_OF_VECTOR)
            .SetMinCount(1)
            .SetMaxCount(1);
        AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER)
            .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
        AddCreationOptionsArg(&m_creationOptions);
        AddOverwriteArg(&m_overwrite);
    }
    else
    {
        AddVectorHiddenInputDatasetArg();
    }

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
    AddLayerNameArg(&m_layerName).SetMutualExclusionGroup("layer-name-or-sql");
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

    if (bStandaloneStep)
    {
        auto &addArg = AddArg("add", 0, _("Add to existing raster"), &m_add)
                           .SetDefault(false);
        auto &updateArg = AddUpdateArg(&m_update);
        addArg.AddValidationAction(
            [&updateArg]()
            {
                updateArg.Set(true);
                return true;
            });
    }
}

/************************************************************************/
/*               GDALVectorRasterizeAlgorithm::RunStep()                */
/************************************************************************/

bool GDALVectorRasterizeAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

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

    std::string outputFilename;
    if (m_standaloneStep)
    {
        outputFilename = m_outputDataset.GetName();
        if (!m_format.empty())
        {
            aosOptions.AddString("-of");
            aosOptions.AddString(m_format.c_str());
        }

        for (const std::string &co : m_creationOptions)
        {
            aosOptions.AddString("-co");
            aosOptions.AddString(co.c_str());
        }
    }
    else
    {
        outputFilename = CPLGenerateTempFilenameSafe("_rasterize.tif");

        aosOptions.AddString("-of");
        aosOptions.AddString("GTiff");

        aosOptions.AddString("-co");
        aosOptions.AddString("TILED=YES");
    }

    if (!std::isnan(m_nodata))
    {
        if (m_update)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Cannot specify --nodata when updating an existing raster.");
            return false;
        }
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
        if (m_update)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Cannot specify --crs when updating an existing raster.");
            return false;
        }
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

    if (m_targetExtent.size())
    {
        aosOptions.AddString("-te");
        for (double targetExtent : m_targetExtent)
        {
            aosOptions.AddString(CPLSPrintf("%.17g", targetExtent));
        }
    }

    if (m_tap)
    {
        aosOptions.AddString("-tap");
    }

    if (m_targetResolution.size())
    {
        if (m_update)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot specify --resolution when updating an existing "
                        "raster.");
            return false;
        }
        aosOptions.AddString("-tr");
        for (double targetResolution : m_targetResolution)
        {
            aosOptions.AddString(CPLSPrintf("%.17g", targetResolution));
        }
    }
    else if (m_targetSize.size())
    {
        if (m_update)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Cannot specify --size when updating an existing raster.");
            return false;
        }
        aosOptions.AddString("-ts");
        for (int targetSize : m_targetSize)
        {
            aosOptions.AddString(CPLSPrintf("%d", targetSize));
        }
    }
    else if (m_outputDataset.GetDatasetRef() == nullptr)
    {
        ReportError(
            CE_Failure, CPLE_AppDefined,
            "Must specify output resolution (--resolution) or size (--size) "
            "when writing rasterized features to a new dataset.");
        return false;
    }

    if (!m_outputType.empty())
    {
        if (m_update)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot specify --output-data-type when updating an "
                        "existing raster.");
            return false;
        }
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
        GDALRasterizeOptionsSetProgress(psOptions.get(), ctxt.m_pfnProgress,
                                        ctxt.m_pProgressData);

        GDALDatasetH hDstDS =
            GDALDataset::ToHandle(m_outputDataset.GetDatasetRef());

        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);

        auto poRetDS = GDALDataset::FromHandle(GDALRasterize(
            outputFilename.c_str(), hDstDS, hSrcDS, psOptions.get(), nullptr));
        bOK = poRetDS != nullptr;

        if (!hDstDS)
        {
            if (!m_standaloneStep && poRetDS)
            {
                VSIUnlink(outputFilename.c_str());
                poRetDS->MarkSuppressOnClose();
            }

            m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
        }
    }

    return bOK;
}

/************************************************************************/
/*               GDALVectorRasterizeAlgorithm::RunImpl()                */
/************************************************************************/

bool GDALVectorRasterizeAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

GDALVectorRasterizeAlgorithmStandalone::
    ~GDALVectorRasterizeAlgorithmStandalone() = default;

//! @endcond
