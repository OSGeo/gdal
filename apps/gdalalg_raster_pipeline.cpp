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
#include "gdalalg_raster_read.h"
#include "gdalalg_raster_calc.h"
#include "gdalalg_raster_aspect.h"
#include "gdalalg_raster_clip.h"
#include "gdalalg_raster_color_map.h"
#include "gdalalg_raster_color_merge.h"
#include "gdalalg_raster_edit.h"
#include "gdalalg_raster_fill_nodata.h"
#include "gdalalg_raster_hillshade.h"
#include "gdalalg_raster_info.h"
#include "gdalalg_raster_mosaic.h"
#include "gdalalg_raster_nodata_to_alpha.h"
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
#include "gdalalg_raster_viewshed.h"

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
/*        GDALRasterPipelineStepAlgorithm::SetOutputVRTCompatible()     */
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
/*        GDALRasterPipelineAlgorithm::GDALRasterPipelineAlgorithm()    */
/************************************************************************/

GDALRasterPipelineAlgorithm::GDALRasterPipelineAlgorithm(
    bool openForMixedRasterVector)
    : GDALAbstractPipelineAlgorithm<GDALRasterPipelineStepAlgorithm>(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetAddDefaultArguments(false)
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
/*       GDALRasterPipelineAlgorithm::RegisterAlgorithms()              */
/************************************************************************/

/* static */
void GDALRasterPipelineAlgorithm::RegisterAlgorithms(
    GDALAlgorithmRegistry &registry, bool forMixedPipeline)
{
    GDALAlgorithmRegistry::AlgInfo algInfo;

    const auto addSuffixIfNeeded =
        [forMixedPipeline](const char *name) -> std::string
    {
        return forMixedPipeline ? std::string(name).append(RASTER_SUFFIX)
                                : std::string(name);
    };

    algInfo.m_name = addSuffixIfNeeded(GDALRasterReadAlgorithm::NAME);
    algInfo.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
    { return std::make_unique<GDALRasterReadAlgorithm>(); };
    registry.Register(algInfo);

    registry.Register<GDALRasterCalcAlgorithm>();

    algInfo.m_name = addSuffixIfNeeded(GDALRasterWriteAlgorithm::NAME);
    algInfo.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
    { return std::make_unique<GDALRasterWriteAlgorithm>(); };
    registry.Register(algInfo);

    algInfo.m_name = addSuffixIfNeeded(GDALRasterInfoAlgorithm::NAME);
    algInfo.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
    { return std::make_unique<GDALRasterInfoAlgorithm>(); };
    registry.Register(algInfo);

    registry.Register<GDALRasterAspectAlgorithm>();

    algInfo.m_name = addSuffixIfNeeded(GDALRasterClipAlgorithm::NAME);
    algInfo.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
    { return std::make_unique<GDALRasterClipAlgorithm>(); };
    registry.Register(algInfo);

    registry.Register<GDALRasterColorMapAlgorithm>();
    registry.Register<GDALRasterColorMergeAlgorithm>();

    algInfo.m_name = addSuffixIfNeeded(GDALRasterEditAlgorithm::NAME);
    algInfo.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
    { return std::make_unique<GDALRasterEditAlgorithm>(); };
    registry.Register(algInfo);

    registry.Register<GDALRasterNoDataToAlphaAlgorithm>();
    registry.Register<GDALRasterFillNodataAlgorithm>();
    registry.Register<GDALRasterHillshadeAlgorithm>();
    registry.Register<GDALRasterMosaicAlgorithm>();
    registry.Register<GDALRasterPansharpenAlgorithm>();
    registry.Register<GDALRasterProximityAlgorithm>();
    registry.Register<GDALRasterReclassifyAlgorithm>();

    algInfo.m_name = addSuffixIfNeeded(GDALRasterReprojectAlgorithm::NAME);
    algInfo.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
    { return std::make_unique<GDALRasterReprojectAlgorithm>(); };
    registry.Register(algInfo);

    registry.Register<GDALRasterResizeAlgorithm>();
    registry.Register<GDALRasterRGBToPaletteAlgorithm>();
    registry.Register<GDALRasterRoughnessAlgorithm>();
    registry.Register<GDALRasterScaleAlgorithm>();

    algInfo.m_name = addSuffixIfNeeded(GDALRasterSelectAlgorithm::NAME);
    algInfo.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
    { return std::make_unique<GDALRasterSelectAlgorithm>(); };
    registry.Register(algInfo);

    registry.Register<GDALRasterSetTypeAlgorithm>();
    registry.Register<GDALRasterSieveAlgorithm>();
    registry.Register<GDALRasterSlopeAlgorithm>();
    registry.Register<GDALRasterStackAlgorithm>();
    registry.Register<GDALRasterTileAlgorithm>();
    registry.Register<GDALRasterTPIAlgorithm>();
    registry.Register<GDALRasterTRIAlgorithm>();
    registry.Register<GDALRasterUnscaleAlgorithm>();
    registry.Register<GDALRasterViewshedAlgorithm>();
}

/************************************************************************/
/*       GDALRasterPipelineAlgorithm::ParseCommandLineArguments()       */
/************************************************************************/

bool GDALRasterPipelineAlgorithm::ParseCommandLineArguments(
    const std::vector<std::string> &args)
{
    if (args.size() == 1 && (args[0] == "-h" || args[0] == "--help" ||
                             args[0] == "help" || args[0] == "--json-usage"))
    {
        return GDALAlgorithm::ParseCommandLineArguments(args);
    }
    else if (args.size() == 1 && STARTS_WITH(args[0].c_str(), "--help-doc="))
    {
        m_helpDocCategory = args[0].substr(strlen("--help-doc="));
        return GDALAlgorithm::ParseCommandLineArguments({"--help-doc"});
    }

    for (const auto &arg : args)
    {
        if (arg.find("--pipeline") == 0)
            return GDALAlgorithm::ParseCommandLineArguments(args);

        // gdal raster pipeline [--quiet] "read in.tif ..."
        if (arg.find("read ") == 0)
            return GDALAlgorithm::ParseCommandLineArguments(args);
    }

    if (!m_steps.empty())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "ParseCommandLineArguments() can only be called once per "
                    "instance.");
        return false;
    }

    struct Step
    {
        std::unique_ptr<GDALRasterPipelineStepAlgorithm> alg{};
        std::vector<std::string> args{};
    };

    std::vector<Step> steps;
    steps.resize(1);

    for (const auto &arg : args)
    {
        if (arg == "--progress")
        {
            m_progressBarRequested = true;
            continue;
        }
        if (arg == "--quiet")
        {
            m_quiet = true;
            m_progressBarRequested = false;
            continue;
        }

        if (IsCalledFromCommandLine() && (arg == "-h" || arg == "--help"))
        {
            if (!steps.back().alg)
                steps.pop_back();
            if (steps.empty())
            {
                return GDALAlgorithm::ParseCommandLineArguments(args);
            }
            else
            {
                m_stepOnWhichHelpIsRequested = std::move(steps.back().alg);
                return true;
            }
        }

        auto &curStep = steps.back();

        if (arg == "!" || arg == "|")
        {
            if (curStep.alg)
            {
                steps.resize(steps.size() + 1);
            }
        }
#ifdef GDAL_PIPELINE_PROJ_NOSTALGIA
        else if (arg == "+step")
        {
            if (curStep.alg)
            {
                steps.resize(steps.size() + 1);
            }
        }
        else if (arg.find("+gdal=") == 0)
        {
            const std::string stepName = arg.substr(strlen("+gdal="));
            curStep.alg = GetStepAlg(stepName);
            if (!curStep.alg)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "unknown step name: %s", stepName.c_str());
                return false;
            }
        }
#endif
        else if (!curStep.alg)
        {
            std::string algName = arg;
#ifdef GDAL_PIPELINE_PROJ_NOSTALGIA
            if (!algName.empty() && algName[0] == '+')
                algName = algName.substr(1);
#endif
            curStep.alg = GetStepAlg(algName);
            if (!curStep.alg)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "unknown step name: %s", algName.c_str());
                return false;
            }
            curStep.alg->SetCallPath({std::move(algName)});
        }
        else
        {
            if (curStep.alg->HasSubAlgorithms())
            {
                auto subAlg = std::unique_ptr<GDALRasterPipelineStepAlgorithm>(
                    cpl::down_cast<GDALRasterPipelineStepAlgorithm *>(
                        curStep.alg->InstantiateSubAlgorithm(arg).release()));
                if (!subAlg)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "'%s' is a unknown sub-algorithm of '%s'",
                                arg.c_str(), curStep.alg->GetName().c_str());
                    return false;
                }
                curStep.alg = std::move(subAlg);
                continue;
            }

#ifdef GDAL_PIPELINE_PROJ_NOSTALGIA
            if (!arg.empty() && arg[0] == '+' &&
                arg.find(' ') == std::string::npos)
            {
                curStep.args.push_back("--" + arg.substr(1));
                continue;
            }
#endif
            curStep.args.push_back(arg);
        }
    }

    // As we initially added a step without alg to bootstrap things, make
    // sure to remove it if it hasn't been filled, or the user has terminated
    // the pipeline with a '!' separator.
    if (!steps.back().alg)
        steps.pop_back();

    // Automatically add a final write step if none in m_executionForStreamOutput
    // mode
    if (m_executionForStreamOutput && !steps.empty() &&
        steps.back().alg->GetName() != GDALRasterWriteAlgorithm::NAME)
    {
        steps.resize(steps.size() + 1);
        steps.back().alg = GetStepAlg(GDALRasterWriteAlgorithm::NAME);
        steps.back().args.push_back("--output-format");
        steps.back().args.push_back("stream");
        steps.back().args.push_back("streamed_dataset");
    }

    bool helpRequested = false;
    if (IsCalledFromCommandLine())
    {
        for (auto &step : steps)
            step.alg->SetCalledFromCommandLine();

        for (const std::string &v : args)
        {
            if (cpl::ends_with(v, "=?"))
                helpRequested = true;
        }
    }

    if (steps.size() < 2)
    {
        if (!steps.empty() && helpRequested)
        {
            steps.back().alg->ParseCommandLineArguments(steps.back().args);
            return false;
        }

        ReportError(CE_Failure, CPLE_AppDefined,
                    "At least 2 steps must be provided");
        return false;
    }

    if (!steps.back().alg->CanBeLastStep())
    {
        if (helpRequested)
        {
            steps.back().alg->ParseCommandLineArguments(steps.back().args);
            return false;
        }
    }

    std::vector<GDALRasterPipelineStepAlgorithm *> stepAlgs;
    for (const auto &step : steps)
        stepAlgs.push_back(step.alg.get());
    if (!CheckFirstAndLastStep(stepAlgs))
        return false;  // CheckFirstAndLastStep emits an error

    for (auto &step : steps)
    {
        step.alg->SetReferencePathForRelativePaths(
            GetReferencePathForRelativePaths());
    }

    // Propagate input parameters set at the pipeline level to the
    // "read" step
    {
        auto &step = steps.front();
        for (auto &arg : step.alg->GetArgs())
        {
            if (!arg->IsHidden())
            {
                auto pipelineArg = GetArg(arg->GetName());
                if (pipelineArg && pipelineArg->IsExplicitlySet() &&
                    pipelineArg->GetType() == arg->GetType())
                {
                    arg->SetSkipIfAlreadySet(true);
                    arg->SetFrom(*pipelineArg);
                }
            }
        }
    }

    // Same with "write" step
    {
        auto &step = steps.back();
        for (auto &arg : step.alg->GetArgs())
        {
            if (!arg->IsHidden())
            {
                auto pipelineArg = GetArg(arg->GetName());
                if (pipelineArg && pipelineArg->IsExplicitlySet() &&
                    pipelineArg->GetType() == arg->GetType())
                {
                    arg->SetSkipIfAlreadySet(true);
                    arg->SetFrom(*pipelineArg);
                }
            }
        }
    }

    // Parse each step, but without running the validation
    for (const auto &step : steps)
    {
        step.alg->m_skipValidationInParseCommandLine = true;
        if (!step.alg->ParseCommandLineArguments(step.args))
            return false;
    }

    // Evaluate "input" argument of "read" step, together with the "output"
    // argument of the "write" step, in case they point to the same dataset.
    auto inputArg = steps.front().alg->GetArg(GDAL_ARG_NAME_INPUT);
    if (inputArg && inputArg->IsExplicitlySet() &&
        inputArg->GetType() == GAAT_DATASET_LIST &&
        inputArg->Get<std::vector<GDALArgDatasetValue>>().size() == 1)
    {
        steps.front().alg->ProcessDatasetArg(inputArg, steps.back().alg.get());
    }

    for (const auto &step : steps)
    {
        if (!step.alg->ValidateArguments())
            return false;
    }

    for (auto &step : steps)
        m_steps.push_back(std::move(step.alg));

    return true;
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
        std::string ret;
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

    ret +=
        "\n<PIPELINE> is of the form: read|mosaic|stack [READ-OPTIONS] "
        "( ! <STEP-NAME> [STEP-OPTIONS] )* ! write|info|tile [WRITE-OPTIONS]\n";

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
        if (alg->CanBeFirstStep() && !alg->IsHidden() &&
            name != GDALRasterReadAlgorithm::NAME)
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
        if (!alg->CanBeFirstStep() && !alg->CanBeLastStep() && !alg->IsHidden())
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
        if (alg->CanBeLastStep() && !alg->IsHidden() &&
            name != GDALRasterWriteAlgorithm::NAME)
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
/*           GDALRasterPipelineNonNativelyStreamingAlgorithm()          */
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
/*                    IsNativelyStreamingCompatible()                   */
/************************************************************************/

bool GDALRasterPipelineNonNativelyStreamingAlgorithm::
    IsNativelyStreamingCompatible() const
{
    return false;
}

/************************************************************************/
/*                     MustCreateOnDiskTempDataset()                    */
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
/*                      CreateTemporaryDataset()                        */
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
/*                       CreateTemporaryCopy()                          */
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
