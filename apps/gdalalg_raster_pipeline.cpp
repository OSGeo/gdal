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
#include "gdalalg_raster_edit.h"
#include "gdalalg_raster_reproject.h"
#include "gdalalg_raster_write.h"

#include "cpl_conv.h"
#include "cpl_json.h"
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
                       /* positionalAndRequired = */ !hiddenForCLI);
}

/************************************************************************/
/*             GDALRasterPipelineStepAlgorithm::AddOutputArgs()         */
/************************************************************************/

void GDALRasterPipelineStepAlgorithm::AddOutputArgs(bool hiddenForCLI)
{
    AddOutputFormatArg(&m_format)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_RASTER, GDAL_DCAP_CREATECOPY})
        .SetHiddenForCLI(hiddenForCLI);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER,
                        /* positionalAndRequired = */ !hiddenForCLI)
        .SetHiddenForCLI(hiddenForCLI);
    m_outputDataset.SetInputFlags(GADV_NAME | GADV_OBJECT);
    AddCreationOptionsArg(&m_creationOptions).SetHiddenForCLI(hiddenForCLI);
    AddOverwriteArg(&m_overwrite).SetHiddenForCLI(hiddenForCLI);
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

        bool ret = false;
        if (readAlg.Run())
        {
            m_inputDataset.Set(readAlg.m_outputDataset.GetDatasetRef());
            m_outputDataset.Set(nullptr);
            if (RunStep(nullptr, nullptr))
            {
                writeAlg.m_inputDataset.Set(m_outputDataset.GetDatasetRef());
                if (writeAlg.Run(pfnProgress, pProgressData))
                {
                    m_outputDataset.Set(
                        writeAlg.m_outputDataset.GetDatasetRef());
                    ret = true;
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
/*        GDALRasterPipelineAlgorithm::GDALRasterPipelineAlgorithm()    */
/************************************************************************/

GDALRasterPipelineAlgorithm::GDALRasterPipelineAlgorithm(
    bool openForMixedRasterVector)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /*standaloneStep=*/false)
{
    AddInputArgs(openForMixedRasterVector, /* hiddenForCLI = */ true);
    AddProgressArg();
    AddArg("pipeline", 0, _("Pipeline string"), &m_pipeline)
        .SetHiddenForCLI()
        .SetPositional();
    AddOutputArgs(/* hiddenForCLI = */ true);

    m_stepRegistry.Register<GDALRasterReadAlgorithm>();
    m_stepRegistry.Register<GDALRasterWriteAlgorithm>();
    m_stepRegistry.Register<GDALRasterEditAlgorithm>();
    m_stepRegistry.Register<GDALRasterReprojectAlgorithm>();
}

/************************************************************************/
/*              GDALRasterPipelineAlgorithm::GetStepAlg()               */
/************************************************************************/

std::unique_ptr<GDALRasterPipelineStepAlgorithm>
GDALRasterPipelineAlgorithm::GetStepAlg(const std::string &name) const
{
    auto alg = m_stepRegistry.Instantiate(name);
    return std::unique_ptr<GDALRasterPipelineStepAlgorithm>(
        cpl::down_cast<GDALRasterPipelineStepAlgorithm *>(alg.release()));
}

/************************************************************************/
/*       GDALRasterPipelineAlgorithm::ParseCommandLineArguments()       */
/************************************************************************/

bool GDALRasterPipelineAlgorithm::ParseCommandLineArguments(
    const std::vector<std::string> &args)
{
    if (args.size() == 1 && (args[0] == "-h" || args[0] == "--help" ||
                             args[0] == "help" || args[0] == "--json-usage"))
        return GDALAlgorithm::ParseCommandLineArguments(args);

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
        }
        else
        {
#ifdef GDAL_PIPELINE_PROJ_NOSTALGIA
            if (!arg.empty() && arg[0] == '+')
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

    if (!m_pipeline.empty())
    {
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
/*               GDALRasterPipelineAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterPipelineAlgorithm::RunStep(GDALProgressFunc pfnProgress,
                                          void *pProgressData)
{
    if (m_steps.empty())
    {
        // If invoked programmatically, not from the command line.

        if (m_pipeline.empty())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "'pipeline' argument not set");
            return false;
        }

        const CPLStringList aosTokens(CSLTokenizeString(m_pipeline.c_str()));
        if (!ParseCommandLineArguments(aosTokens))
            return false;
    }

    GDALDataset *poCurDS = nullptr;
    for (size_t i = 0; i < m_steps.size(); ++i)
    {
        auto &step = m_steps[i];
        if (i > 0)
        {
            if (step->m_inputDataset.GetDatasetRef())
            {
                // Shouldn't happen
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Step nr %d (%s) has already an input dataset",
                            static_cast<int>(i), step->GetName().c_str());
                return false;
            }
            step->m_inputDataset.Set(poCurDS);
        }
        if (i + 1 < m_steps.size() && step->m_outputDataset.GetDatasetRef())
        {
            // Shouldn't happen
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Step nr %d (%s) has already an output dataset",
                        static_cast<int>(i), step->GetName().c_str());
            return false;
        }
        if (!step->Run(i < m_steps.size() - 1 ? nullptr : pfnProgress,
                       i < m_steps.size() - 1 ? nullptr : pProgressData))
        {
            return false;
        }
        poCurDS = step->m_outputDataset.GetDatasetRef();
        if (!poCurDS)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Step nr %d (%s) failed to produce an output dataset",
                        static_cast<int>(i), step->GetName().c_str());
            return false;
        }
    }

    if (!m_outputDataset.GetDatasetRef())
    {
        m_outputDataset.Set(poCurDS);
    }

    return true;
}

/************************************************************************/
/*                     GDALAlgorithm::Finalize()                        */
/************************************************************************/

bool GDALRasterPipelineAlgorithm::Finalize()
{
    bool ret = GDALAlgorithm::Finalize();
    for (auto &step : m_steps)
    {
        ret = step->Finalize() && ret;
    }
    return ret;
}

/************************************************************************/
/*            GDALRasterPipelineAlgorithm::GetUsageForCLI()             */
/************************************************************************/

std::string GDALRasterPipelineAlgorithm::GetUsageForCLI(
    bool shortUsage, const UsageOptions &usageOptions) const
{
    std::string ret = GDALAlgorithm::GetUsageForCLI(shortUsage, usageOptions);
    if (shortUsage)
        return ret;

    ret += "\n<PIPELINE> is of the form: read [READ-OPTIONS] "
           "( ! <STEP-NAME> [STEP-OPTIONS] )* ! write [WRITE-OPTIONS]\n";
    ret += '\n';
    ret += "Example: 'gdal raster pipeline --progress ! read in.tif ! \\\n";
    ret += "               reproject --dst-crs=EPSG:32632 ! ";
    ret += "write out.tif --overwrite'\n";
    ret += '\n';
    ret += "Potential steps are:\n";

    UsageOptions stepUsageOptions;
    stepUsageOptions.isPipelineStep = true;

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

    return ret;
}

/************************************************************************/
/*             GDALRasterPipelineAlgorithm::GetUsageAsJSON()            */
/************************************************************************/

std::string GDALRasterPipelineAlgorithm::GetUsageAsJSON() const
{
    CPLJSONDocument oDoc;
    oDoc.LoadMemory(GDALAlgorithm::GetUsageAsJSON());

    CPLJSONArray jPipelineSteps;
    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        CPLJSONDocument oStepDoc;
        oStepDoc.LoadMemory(alg->GetUsageAsJSON());
        jPipelineSteps.Add(oStepDoc.GetRoot());
    }
    oDoc.GetRoot().Add("pipeline_algorithms", jPipelineSteps);

    return oDoc.SaveAsString();
}

//! @endcond
