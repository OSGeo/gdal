/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_pipeline.h"
#include "gdalalg_materialize.h"
#include "gdalalg_raster_read.h"
#include "gdalalg_raster_calc.h"
#include "gdalalg_raster_aspect.h"
#include "gdalalg_raster_blend.h"
#include "gdalalg_raster_clip.h"
#include "gdalalg_raster_color_map.h"
#include "gdalalg_raster_compare.h"
#include "gdalalg_raster_create.h"
#include "gdalalg_raster_edit.h"
#include "gdalalg_raster_fill_nodata.h"
#include "gdalalg_raster_hillshade.h"
#include "gdalalg_raster_info.h"
#include "gdalalg_raster_mosaic.h"
#include "gdalalg_raster_neighbors.h"
#include "gdalalg_raster_nodata_to_alpha.h"
#include "gdalalg_raster_overview.h"
#include "gdalalg_raster_pansharpen.h"
#include "gdalalg_raster_proximity.h"
#include "gdalalg_raster_reclassify.h"
#include "gdalalg_raster_reproject.h"
#include "gdalalg_raster_resize.h"
#include "gdalalg_raster_rgb_to_palette.h"
#include "gdalalg_raster_roughness.h"
#include "gdalalg_raster_scale.h"
#include "gdalalg_raster_select.h"
#include "gdalalg_raster_set_type.h"
#include "gdalalg_raster_sieve.h"
#include "gdalalg_raster_slope.h"
#include "gdalalg_raster_stack.h"
#include "gdalalg_raster_tile.h"
#include "gdalalg_raster_write.h"
#include "gdalalg_raster_tpi.h"
#include "gdalalg_raster_tri.h"
#include "gdalalg_raster_unscale.h"
#include "gdalalg_raster_update.h"
#include "gdalalg_raster_viewshed.h"
#include "gdalalg_tee.h"

#include "cpl_conv.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

#include <algorithm>
#include <array>
#include <cassert>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

GDALRasterAlgorithmStepRegistry::~GDALRasterAlgorithmStepRegistry() = default;

/************************************************************************/
/*  GDALRasterPipelineStepAlgorithm::GDALRasterPipelineStepAlgorithm()  */
/************************************************************************/

GDALRasterPipelineStepAlgorithm::GDALRasterPipelineStepAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(
          name, description, helpURL,
          ConstructorOptions().SetStandaloneStep(standaloneStep))
{
}

/************************************************************************/
/*  GDALRasterPipelineStepAlgorithm::GDALRasterPipelineStepAlgorithm()  */
/************************************************************************/

GDALRasterPipelineStepAlgorithm::GDALRasterPipelineStepAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, const ConstructorOptions &options)
    : GDALPipelineStepAlgorithm(name, description, helpURL, options)
{
    if (m_standaloneStep)
    {
        m_supportsStreamedOutput = true;

        if (m_constructorOptions.addDefaultArguments)
        {
            AddRasterInputArgs(false, false);
            AddProgressArg();
            AddRasterOutputArgs(false);
        }
    }
    else if (m_constructorOptions.addDefaultArguments)
    {
        AddRasterHiddenInputDatasetArg();
    }
}

GDALRasterPipelineStepAlgorithm::~GDALRasterPipelineStepAlgorithm() = default;

/************************************************************************/
/*      GDALRasterPipelineStepAlgorithm::SetOutputVRTCompatible()       */
/************************************************************************/

void GDALRasterPipelineStepAlgorithm::SetOutputVRTCompatible(bool b)
{
    m_outputVRTCompatible = b;
    if (m_outputFormatArg)
    {
        m_outputFormatArg->AddMetadataItem(GAAMDI_VRT_COMPATIBLE,
                                           {b ? "true" : "false"});
    }
}

/************************************************************************/
/*      GDALRasterPipelineAlgorithm::GDALRasterPipelineAlgorithm()      */
/************************************************************************/

GDALRasterPipelineAlgorithm::GDALRasterPipelineAlgorithm(
    bool openForMixedRasterVector)
    : GDALAbstractPipelineAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                    ConstructorOptions()
                                        .SetAddDefaultArguments(false)
                                        .SetInputDatasetRequired(false)
                                        .SetInputDatasetPositional(false)
                                        .SetInputDatasetMaxCount(INT_MAX))
{
    m_supportsStreamedOutput = true;

    AddRasterInputArgs(openForMixedRasterVector, /* hiddenForCLI = */ true);
    AddProgressArg();
    AddArg("pipeline", 0, _("Pipeline string"), &m_pipeline)
        .SetHiddenForCLI()
        .SetPositional();
    AddRasterOutputArgs(/* hiddenForCLI = */ true);

    AddOutputStringArg(&m_output).SetHiddenForCLI();
    AddStdoutArg(&m_stdout);

    RegisterAlgorithms(m_stepRegistry, false);
}

/************************************************************************/
/*          GDALRasterPipelineAlgorithm::RegisterAlgorithms()           */
/************************************************************************/

/* static */
void GDALRasterPipelineAlgorithm::RegisterAlgorithms(
    GDALRasterAlgorithmStepRegistry &registry, bool forMixedPipeline)
{
    GDALAlgorithmRegistry::AlgInfo algInfo;

    const auto addSuffixIfNeeded =
        [forMixedPipeline](const char *name) -> std::string
    {
        return forMixedPipeline ? std::string(name).append(RASTER_SUFFIX)
                                : std::string(name);
    };

    registry.Register<GDALRasterReadAlgorithm>(
        addSuffixIfNeeded(GDALRasterReadAlgorithm::NAME));

    registry.Register<GDALRasterCalcAlgorithm>();
    registry.Register<GDALRasterCreateAlgorithm>();

    registry.Register<GDALRasterNeighborsAlgorithm>();

    registry.Register<GDALRasterWriteAlgorithm>(
        addSuffixIfNeeded(GDALRasterWriteAlgorithm::NAME));

    registry.Register<GDALRasterInfoAlgorithm>(
        addSuffixIfNeeded(GDALRasterInfoAlgorithm::NAME));

    registry.Register<GDALRasterAspectAlgorithm>();
    registry.Register<GDALRasterBlendAlgorithm>();

    registry.Register<GDALRasterClipAlgorithm>(
        addSuffixIfNeeded(GDALRasterClipAlgorithm::NAME));

    registry.Register<GDALRasterColorMapAlgorithm>();
    registry.Register<GDALRasterCompareAlgorithm>();

    registry.Register<GDALRasterEditAlgorithm>(
        addSuffixIfNeeded(GDALRasterEditAlgorithm::NAME));

    registry.Register<GDALRasterNoDataToAlphaAlgorithm>();
    registry.Register<GDALRasterFillNodataAlgorithm>();
    registry.Register<GDALRasterHillshadeAlgorithm>();

    registry.Register<GDALMaterializeRasterAlgorithm>(
        addSuffixIfNeeded(GDALMaterializeRasterAlgorithm::NAME));

    registry.Register<GDALRasterMosaicAlgorithm>();
    registry.Register<GDALRasterOverviewAlgorithm>();
    registry.Register<GDALRasterPansharpenAlgorithm>();
    registry.Register<GDALRasterProximityAlgorithm>();
    registry.Register<GDALRasterReclassifyAlgorithm>();

    registry.Register<GDALRasterReprojectAlgorithm>(
        addSuffixIfNeeded(GDALRasterReprojectAlgorithm::NAME));

    registry.Register<GDALRasterResizeAlgorithm>();
    registry.Register<GDALRasterRGBToPaletteAlgorithm>();
    registry.Register<GDALRasterRoughnessAlgorithm>();
    registry.Register<GDALRasterScaleAlgorithm>();

    registry.Register<GDALRasterSelectAlgorithm>(
        addSuffixIfNeeded(GDALRasterSelectAlgorithm::NAME));

    registry.Register<GDALRasterSetTypeAlgorithm>();
    registry.Register<GDALRasterSieveAlgorithm>();
    registry.Register<GDALRasterSlopeAlgorithm>();
    registry.Register<GDALRasterStackAlgorithm>();
    registry.Register<GDALRasterTileAlgorithm>();
    registry.Register<GDALRasterTPIAlgorithm>();
    registry.Register<GDALRasterTRIAlgorithm>();
    registry.Register<GDALRasterUnscaleAlgorithm>();
    registry.Register<GDALRasterUpdateAlgorithm>(
        addSuffixIfNeeded(GDALRasterUpdateAlgorithm::NAME));
    registry.Register<GDALRasterViewshedAlgorithm>();
    registry.Register<GDALTeeRasterAlgorithm>(
        addSuffixIfNeeded(GDALTeeRasterAlgorithm::NAME));
}

/************************************************************************/
/*            GDALRasterPipelineAlgorithm::GetUsageForCLI()             */
/************************************************************************/

std::string GDALRasterPipelineAlgorithm::GetUsageForCLI(
    bool shortUsage, const UsageOptions &usageOptions) const
{
    UsageOptions stepUsageOptions;
    stepUsageOptions.isPipelineStep = true;

    if (!m_helpDocCategory.empty() && m_helpDocCategory != "main")
    {
        auto alg = GetStepAlg(m_helpDocCategory);
        if (alg)
        {
            alg->SetCallPath({m_helpDocCategory});
            alg->GetArg("help-doc")->Set(true);
            return alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
        else
        {
            fprintf(stderr, "ERROR: unknown pipeline step '%s'\n",
                    m_helpDocCategory.c_str());
            return CPLSPrintf("ERROR: unknown pipeline step '%s'\n",
                              m_helpDocCategory.c_str());
        }
    }

    UsageOptions usageOptionsMain(usageOptions);
    usageOptionsMain.isPipelineMain = true;
    std::string ret =
        GDALAlgorithm::GetUsageForCLI(shortUsage, usageOptionsMain);
    if (shortUsage)
        return ret;

    ret += "\n<PIPELINE> is of the form: read|mosaic|stack [READ-OPTIONS] "
           "( ! <STEP-NAME> [STEP-OPTIONS] )* ! info|compare|tile|write "
           "[WRITE-OPTIONS]\n";

    if (m_helpDocCategory == "main")
    {
        return ret;
    }

    ret += '\n';
    ret += "Example: 'gdal raster pipeline --progress ! read in.tif ! \\\n";
    ret += "               reproject --dst-crs=EPSG:32632 ! ";
    ret += "write out.tif --overwrite'\n";
    ret += '\n';
    ret += "Potential steps are:\n";

    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        auto [options, maxOptLen] = alg->GetArgNamesForCLI();
        stepUsageOptions.maxOptLen =
            std::max(stepUsageOptions.maxOptLen, maxOptLen);
    }

    {
        const auto name = GDALRasterReadAlgorithm::NAME;
        ret += '\n';
        auto alg = GetStepAlg(name);
        alg->SetCallPath({name});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }
    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        assert(alg);
        if (alg->CanBeFirstStep() && !alg->CanBeMiddleStep() &&
            !alg->IsHidden() && name != GDALRasterReadAlgorithm::NAME)
        {
            ret += '\n';
            alg->SetCallPath({name});
            ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
    }
    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        assert(alg);
        if (alg->CanBeMiddleStep() && !alg->IsHidden())
        {
            ret += '\n';
            alg->SetCallPath({name});
            ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
    }
    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        assert(alg);
        if (alg->CanBeLastStep() && !alg->CanBeMiddleStep() &&
            !alg->IsHidden() && name != GDALRasterWriteAlgorithm::NAME)
        {
            ret += '\n';
            alg->SetCallPath({name});
            ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
    }
    {
        const auto name = GDALRasterWriteAlgorithm::NAME;
        ret += '\n';
        auto alg = GetStepAlg(name);
        alg->SetCallPath({name});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }
    ret += GetUsageForCLIEnd();

    return ret;
}

/************************************************************************/
/*          GDALRasterPipelineNonNativelyStreamingAlgorithm()           */
/************************************************************************/

GDALRasterPipelineNonNativelyStreamingAlgorithm::
    GDALRasterPipelineNonNativelyStreamingAlgorithm(
        const std::string &name, const std::string &description,
        const std::string &helpURL, bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(name, description, helpURL,
                                      standaloneStep)
{
}

/************************************************************************/
/*                   IsNativelyStreamingCompatible()                    */
/************************************************************************/

bool GDALRasterPipelineNonNativelyStreamingAlgorithm::
    IsNativelyStreamingCompatible() const
{
    return false;
}

/************************************************************************/
/*                    MustCreateOnDiskTempDataset()                     */
/************************************************************************/

static bool MustCreateOnDiskTempDataset(int nWidth, int nHeight, int nBands,
                                        GDALDataType eDT)
{
    // Config option mostly for autotest purposes
    if (CPLTestBool(CPLGetConfigOption(
            "GDAL_RASTER_PIPELINE_USE_GTIFF_FOR_TEMP_DATASET", "NO")))
        return true;

    // Allow up to 10% of RAM usage for temporary dataset
    const auto nRAM = CPLGetUsablePhysicalRAM() / 10;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    const bool bOnDisk =
        nBands > 0 && nDTSize > 0 && nRAM > 0 &&
        static_cast<int64_t>(nWidth) * nHeight > nRAM / (nBands * nDTSize);
    return bOnDisk;
}

/************************************************************************/
/*                       CreateTemporaryDataset()                       */
/************************************************************************/

std::unique_ptr<GDALDataset>
GDALRasterPipelineNonNativelyStreamingAlgorithm::CreateTemporaryDataset(
    int nWidth, int nHeight, int nBands, GDALDataType eDT,
    bool bTiledIfPossible, GDALDataset *poSrcDSForMetadata, bool bCopyMetadata)
{
    const bool bOnDisk =
        MustCreateOnDiskTempDataset(nWidth, nHeight, nBands, eDT);
    const char *pszDriverName = bOnDisk ? "GTIFF" : "MEM";
    GDALDriver *poDriver =
        GetGDALDriverManager()->GetDriverByName(pszDriverName);
    CPLStringList aosOptions;
    std::string osTmpFilename;
    if (bOnDisk)
    {
        osTmpFilename =
            CPLGenerateTempFilenameSafe(
                poSrcDSForMetadata
                    ? CPLGetBasenameSafe(poSrcDSForMetadata->GetDescription())
                          .c_str()
                    : "") +
            ".tif";
        if (bTiledIfPossible)
            aosOptions.SetNameValue("TILED", "YES");
        const char *pszCOList =
            poDriver->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST);
        aosOptions.SetNameValue("COMPRESS",
                                pszCOList && strstr(pszCOList, "ZSTD") ? "ZSTD"
                                                                       : "LZW");
        aosOptions.SetNameValue("SPARSE_OK", "YES");
    }
    std::unique_ptr<GDALDataset> poOutDS(
        poDriver ? poDriver->Create(osTmpFilename.c_str(), nWidth, nHeight,
                                    nBands, eDT, aosOptions.List())
                 : nullptr);
    if (poOutDS && bOnDisk)
    {
        // In file systems that allow it (all but Windows...), we want to
        // delete the temporary file as soon as soon as possible after
        // having open it, so that if someone kills the process there are
        // no temp files left over. If that unlink() doesn't succeed
        // (on Windows), then the file will eventually be deleted when
        // poTmpDS is cleaned due to MarkSuppressOnClose().
        VSIUnlink(osTmpFilename.c_str());
        poOutDS->MarkSuppressOnClose();
    }

    if (poOutDS && poSrcDSForMetadata)
    {
        poOutDS->SetSpatialRef(poSrcDSForMetadata->GetSpatialRef());
        GDALGeoTransform gt;
        if (poSrcDSForMetadata->GetGeoTransform(gt) == CE_None)
            poOutDS->SetGeoTransform(gt);
        if (const int nGCPCount = poSrcDSForMetadata->GetGCPCount())
        {
            const auto apsGCPs = poSrcDSForMetadata->GetGCPs();
            if (apsGCPs)
            {
                poOutDS->SetGCPs(nGCPCount, apsGCPs,
                                 poSrcDSForMetadata->GetGCPSpatialRef());
            }
        }
        if (bCopyMetadata)
        {
            poOutDS->SetMetadata(poSrcDSForMetadata->GetMetadata());
        }
    }

    return poOutDS;
}

/************************************************************************/
/*                        CreateTemporaryCopy()                         */
/************************************************************************/

std::unique_ptr<GDALDataset>
GDALRasterPipelineNonNativelyStreamingAlgorithm::CreateTemporaryCopy(
    GDALAlgorithm *poAlg, GDALDataset *poSrcDS, int nSingleBand,
    bool bTiledIfPossible, GDALProgressFunc pfnProgress, void *pProgressData)
{
    const int nBands = nSingleBand > 0 ? 1 : poSrcDS->GetRasterCount();
    const auto eDT =
        nBands ? poSrcDS->GetRasterBand(1)->GetRasterDataType() : GDT_Unknown;
    const bool bOnDisk = MustCreateOnDiskTempDataset(
        poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(), nBands, eDT);
    const char *pszDriverName = bOnDisk ? "GTIFF" : "MEM";

    CPLStringList options;
    if (nSingleBand > 0)
    {
        options.AddString("-b");
        options.AddString(CPLSPrintf("%d", nSingleBand));
    }

    options.AddString("-of");
    options.AddString(pszDriverName);

    std::string osTmpFilename;
    if (bOnDisk)
    {
        osTmpFilename =
            CPLGenerateTempFilenameSafe(
                CPLGetBasenameSafe(poSrcDS->GetDescription()).c_str()) +
            ".tif";
        if (bTiledIfPossible)
        {
            options.AddString("-co");
            options.AddString("TILED=YES");
        }

        GDALDriver *poDriver =
            GetGDALDriverManager()->GetDriverByName(pszDriverName);
        const char *pszCOList =
            poDriver ? poDriver->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST)
                     : nullptr;
        options.AddString("-co");
        options.AddString(pszCOList && strstr(pszCOList, "ZSTD")
                              ? "COMPRESS=ZSTD"
                              : "COMPRESS=LZW");
    }

    GDALTranslateOptions *translateOptions =
        GDALTranslateOptionsNew(options.List(), nullptr);

    if (pfnProgress)
        GDALTranslateOptionsSetProgress(translateOptions, pfnProgress,
                                        pProgressData);

    std::unique_ptr<GDALDataset> poOutDS(GDALDataset::FromHandle(
        GDALTranslate(osTmpFilename.c_str(), GDALDataset::ToHandle(poSrcDS),
                      translateOptions, nullptr)));
    GDALTranslateOptionsFree(translateOptions);

    if (!poOutDS)
    {
        poAlg->ReportError(CE_Failure, CPLE_AppDefined,
                           "Failed to create temporary dataset");
    }
    else if (bOnDisk)
    {
        // In file systems that allow it (all but Windows...), we want to
        // delete the temporary file as soon as soon as possible after
        // having open it, so that if someone kills the process there are
        // no temp files left over. If that unlink() doesn't succeed
        // (on Windows), then the file will eventually be deleted when
        // poTmpDS is cleaned due to MarkSuppressOnClose().
        VSIUnlink(osTmpFilename.c_str());
        poOutDS->MarkSuppressOnClose();
    }
    return poOutDS;
}

//! @endcond
