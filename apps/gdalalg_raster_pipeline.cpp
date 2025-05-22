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
#include "gdalalg_raster_aspect.h"
#include "gdalalg_raster_clip.h"
#include "gdalalg_raster_color_map.h"
#include "gdalalg_raster_edit.h"
#include "gdalalg_raster_hillshade.h"
#include "gdalalg_raster_reclassify.h"
#include "gdalalg_raster_reproject.h"
#include "gdalalg_raster_resize.h"
#include "gdalalg_raster_roughness.h"
#include "gdalalg_raster_scale.h"
#include "gdalalg_raster_select.h"
#include "gdalalg_raster_set_type.h"
#include "gdalalg_raster_slope.h"
#include "gdalalg_raster_write.h"
#include "gdalalg_raster_tpi.h"
#include "gdalalg_raster_tri.h"
#include "gdalalg_raster_unscale.h"

#include "cpl_conv.h"
#include "cpl_string.h"

#include <algorithm>

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
    : GDALAlgorithm(name, description, helpURL),
      m_standaloneStep(standaloneStep)
{
    if (m_standaloneStep)
    {
        m_supportsStreamedOutput = true;

        AddInputArgs(false, false);
        AddProgressArg();
        AddOutputArgs(false);
    }
}

/************************************************************************/
/*             GDALRasterPipelineStepAlgorithm::AddInputArgs()          */
/************************************************************************/

void GDALRasterPipelineStepAlgorithm::AddInputArgs(
    bool openForMixedRasterVector, bool hiddenForCLI)
{
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(
            GAAMDI_REQUIRED_CAPABILITIES,
            openForMixedRasterVector
                ? std::vector<std::string>{GDAL_DCAP_RASTER, GDAL_DCAP_VECTOR}
                : std::vector<std::string>{GDAL_DCAP_RASTER})
        .SetHiddenForCLI(hiddenForCLI);
    AddOpenOptionsArg(&m_openOptions).SetHiddenForCLI(hiddenForCLI);
    AddInputDatasetArg(&m_inputDataset,
                       openForMixedRasterVector
                           ? (GDAL_OF_RASTER | GDAL_OF_VECTOR)
                           : GDAL_OF_RASTER,
                       /* positionalAndRequired = */ !hiddenForCLI)
        .SetHiddenForCLI(hiddenForCLI);
}

/************************************************************************/
/*             GDALRasterPipelineStepAlgorithm::AddOutputArgs()         */
/************************************************************************/

void GDALRasterPipelineStepAlgorithm::AddOutputArgs(bool hiddenForCLI)
{
    m_outputFormatArg =
        &(AddOutputFormatArg(&m_format, /* bStreamAllowed = */ true,
                             /* bGDALGAllowed = */ true)
              .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                               {GDAL_DCAP_RASTER, GDAL_DCAP_CREATECOPY})
              .SetHiddenForCLI(hiddenForCLI));
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER,
                        /* positionalAndRequired = */ !hiddenForCLI)
        .SetHiddenForCLI(hiddenForCLI)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    AddCreationOptionsArg(&m_creationOptions).SetHiddenForCLI(hiddenForCLI);
    AddOverwriteArg(&m_overwrite).SetHiddenForCLI(hiddenForCLI);
}

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
/*            GDALRasterPipelineStepAlgorithm::RunImpl()                */
/************************************************************************/

bool GDALRasterPipelineStepAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                              void *pProgressData)
{
    if (m_standaloneStep)
    {
        GDALRasterReadAlgorithm readAlg;
        for (auto &arg : readAlg.GetArgs())
        {
            auto stepArg = GetArg(arg->GetName());
            if (stepArg && stepArg->IsExplicitlySet())
            {
                arg->SetSkipIfAlreadySet(true);
                arg->SetFrom(*stepArg);
            }
        }

        GDALRasterWriteAlgorithm writeAlg;
        for (auto &arg : writeAlg.GetArgs())
        {
            auto stepArg = GetArg(arg->GetName());
            if (stepArg && stepArg->IsExplicitlySet())
            {
                arg->SetSkipIfAlreadySet(true);
                arg->SetFrom(*stepArg);
            }
        }

        // Already checked by GDALAlgorithm::Run()
        CPLAssert(!m_executionForStreamOutput ||
                  EQUAL(m_format.c_str(), "stream"));

        bool ret = false;
        if (readAlg.Run())
        {
            m_inputDataset.Set(readAlg.m_outputDataset.GetDatasetRef());
            m_outputDataset.Set(nullptr);
            if (RunStep(nullptr, nullptr))
            {
                if (m_format == "stream")
                {
                    ret = true;
                }
                else if (!m_outputVRTCompatible &&
                         (EQUAL(m_format.c_str(), "VRT") ||
                          (m_format.empty() &&
                           EQUAL(CPLGetExtensionSafe(
                                     writeAlg.m_outputDataset.GetName().c_str())
                                     .c_str(),
                                 "VRT"))))
                {
                    ReportError(
                        CE_Failure, CPLE_NotSupported,
                        "VRT output is not supported. Consider using the "
                        "GDALG driver instead (files with .gdalg.json "
                        "extension)");
                    ret = false;
                }
                else
                {
                    writeAlg.m_outputVRTCompatible = m_outputVRTCompatible;
                    writeAlg.m_inputDataset.Set(
                        m_outputDataset.GetDatasetRef());
                    if (writeAlg.Run(pfnProgress, pProgressData))
                    {
                        m_outputDataset.Set(
                            writeAlg.m_outputDataset.GetDatasetRef());
                        ret = true;
                    }
                }
            }
        }

        return ret;
    }
    else
    {
        return RunStep(pfnProgress, pProgressData);
    }
}

/************************************************************************/
/*                          ProcessGDALGOutput()                        */
/************************************************************************/

GDALAlgorithm::ProcessGDALGOutputRet
GDALRasterPipelineStepAlgorithm::ProcessGDALGOutput()
{
    if (m_standaloneStep)
    {
        return GDALAlgorithm::ProcessGDALGOutput();
    }
    else
    {
        // GDALAbstractPipelineAlgorithm<StepAlgorithm>::RunStep() might
        // actually detect a GDALG output request and process it.
        return GDALAlgorithm::ProcessGDALGOutputRet::NOT_GDALG;
    }
}

/************************************************************************/
/*      GDALRasterPipelineStepAlgorithm::CheckSafeForStreamOutput()     */
/************************************************************************/

bool GDALRasterPipelineStepAlgorithm::CheckSafeForStreamOutput()
{
    if (m_standaloneStep)
    {
        return GDALAlgorithm::CheckSafeForStreamOutput();
    }
    else
    {
        // The check is actually done in
        // GDALAbstractPipelineAlgorithm<StepAlgorithm>::RunStep()
        // so return true for now.
        return true;
    }
}

/************************************************************************/
/*        GDALRasterPipelineAlgorithm::GDALRasterPipelineAlgorithm()    */
/************************************************************************/

GDALRasterPipelineAlgorithm::GDALRasterPipelineAlgorithm(
    bool openForMixedRasterVector)
    : GDALAbstractPipelineAlgorithm<GDALRasterPipelineStepAlgorithm>(
          NAME, DESCRIPTION, HELP_URL,
          /*standaloneStep=*/false)
{
    m_supportsStreamedOutput = true;

    AddInputArgs(openForMixedRasterVector, /* hiddenForCLI = */ true);
    AddProgressArg();
    AddArg("pipeline", 0, _("Pipeline string"), &m_pipeline)
        .SetHiddenForCLI()
        .SetPositional();
    AddOutputArgs(/* hiddenForCLI = */ true);

    m_stepRegistry.Register<GDALRasterReadAlgorithm>();
    m_stepRegistry.Register<GDALRasterWriteAlgorithm>();
    m_stepRegistry.Register<GDALRasterAspectAlgorithm>();
    m_stepRegistry.Register<GDALRasterClipAlgorithm>();
    m_stepRegistry.Register<GDALRasterColorMapAlgorithm>();
    m_stepRegistry.Register<GDALRasterEditAlgorithm>();
    m_stepRegistry.Register<GDALRasterHillshadeAlgorithm>();
    m_stepRegistry.Register<GDALRasterReclassifyAlgorithm>();
    m_stepRegistry.Register<GDALRasterReprojectAlgorithm>();
    m_stepRegistry.Register<GDALRasterResizeAlgorithm>();
    m_stepRegistry.Register<GDALRasterRoughnessAlgorithm>();
    m_stepRegistry.Register<GDALRasterScaleAlgorithm>();
    m_stepRegistry.Register<GDALRasterSelectAlgorithm>();
    m_stepRegistry.Register<GDALRasterSetTypeAlgorithm>();
    m_stepRegistry.Register<GDALRasterSlopeAlgorithm>();
    m_stepRegistry.Register<GDALRasterTPIAlgorithm>();
    m_stepRegistry.Register<GDALRasterTRIAlgorithm>();
    m_stepRegistry.Register<GDALRasterUnscaleAlgorithm>();
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

        // gdal raster pipeline [--progress] "read in.tif ..."
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
            curStep.alg->SetCallPath({algName});
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

    if (IsCalledFromCommandLine())
    {
        for (auto &step : steps)
            step.alg->SetCalledFromCommandLine();
    }

    if (steps.size() < 2)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "At least 2 steps must be provided");
        return false;
    }

    if (steps.front().alg->GetName() != GDALRasterReadAlgorithm::NAME)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "First step should be '%s'",
                    GDALRasterReadAlgorithm::NAME);
        return false;
    }
    for (size_t i = 1; i < steps.size() - 1; ++i)
    {
        if (steps[i].alg->GetName() == GDALRasterReadAlgorithm::NAME)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Only first step can be '%s'",
                        GDALRasterReadAlgorithm::NAME);
            return false;
        }
    }
    if (steps.back().alg->GetName() != GDALRasterWriteAlgorithm::NAME)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Last step should be '%s'",
                    GDALRasterWriteAlgorithm::NAME);
        return false;
    }
    for (size_t i = 0; i < steps.size() - 1; ++i)
    {
        if (steps[i].alg->GetName() == GDALRasterWriteAlgorithm::NAME)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Only last step can be '%s'",
                        GDALRasterWriteAlgorithm::NAME);
            return false;
        }
    }

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
            auto pipelineArg = GetArg(arg->GetName());
            if (pipelineArg && pipelineArg->IsExplicitlySet())
            {
                arg->SetSkipIfAlreadySet(true);
                arg->SetFrom(*pipelineArg);
            }
        }
    }

    // Same with "write" step
    {
        auto &step = steps.back();
        for (auto &arg : step.alg->GetArgs())
        {
            auto pipelineArg = GetArg(arg->GetName());
            if (pipelineArg && pipelineArg->IsExplicitlySet())
            {
                arg->SetSkipIfAlreadySet(true);
                arg->SetFrom(*pipelineArg);
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
        inputArg->GetType() == GAAT_DATASET)
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

    ret += "\n<PIPELINE> is of the form: read [READ-OPTIONS] "
           "( ! <STEP-NAME> [STEP-OPTIONS] )* ! write [WRITE-OPTIONS]\n";

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
        if (name != GDALRasterReadAlgorithm::NAME &&
            name != GDALRasterWriteAlgorithm::NAME)
        {
            ret += '\n';
            auto alg = GetStepAlg(name);
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

//! @endcond
