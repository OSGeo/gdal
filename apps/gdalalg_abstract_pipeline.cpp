/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster/vector pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024-2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_error_internal.h"
#include "cpl_json.h"

#include "gdalalg_abstract_pipeline.h"
#include "gdalalg_raster_read.h"
#include "gdalalg_raster_write.h"
#include "gdalalg_vector_read.h"
#include "gdalalg_tee.h"

#include <algorithm>
#include <cassert>

//! @cond Doxygen_Suppress

/* clang-format off */
constexpr const char *const apszReadParametersPrefixOmitted[] = {
    GDAL_ARG_NAME_INPUT,
    GDAL_ARG_NAME_INPUT_FORMAT,
    GDAL_ARG_NAME_OPEN_OPTION,
    GDAL_ARG_NAME_INPUT_LAYER};

constexpr const char *const apszWriteParametersPrefixOmitted[] = {
    GDAL_ARG_NAME_OUTPUT,
    GDAL_ARG_NAME_OUTPUT_FORMAT,
    GDAL_ARG_NAME_CREATION_OPTION,
    GDAL_ARG_NAME_OUTPUT_LAYER,
    GDAL_ARG_NAME_LAYER_CREATION_OPTION,
    GDAL_ARG_NAME_UPDATE,
    GDAL_ARG_NAME_OVERWRITE,
    GDAL_ARG_NAME_APPEND,
    GDAL_ARG_NAME_OVERWRITE_LAYER};

/* clang-format on */

/************************************************************************/
/*                       IsReadSpecificArgument()                       */
/************************************************************************/

/* static */
bool GDALAbstractPipelineAlgorithm::IsReadSpecificArgument(
    const char *pszArgName)
{
    return std::find_if(std::begin(apszReadParametersPrefixOmitted),
                        std::end(apszReadParametersPrefixOmitted),
                        [pszArgName](const char *pszStr)
                        { return strcmp(pszStr, pszArgName) == 0; }) !=
           std::end(apszReadParametersPrefixOmitted);
}

/************************************************************************/
/*                      IsWriteSpecificArgument()                       */
/************************************************************************/

/* static */
bool GDALAbstractPipelineAlgorithm::IsWriteSpecificArgument(
    const char *pszArgName)
{
    return std::find_if(std::begin(apszWriteParametersPrefixOmitted),
                        std::end(apszWriteParametersPrefixOmitted),
                        [pszArgName](const char *pszStr)
                        { return strcmp(pszStr, pszArgName) == 0; }) !=
           std::end(apszWriteParametersPrefixOmitted);
}

/************************************************************************/
/*        GDALAbstractPipelineAlgorithm::CheckFirstAndLastStep()        */
/************************************************************************/

bool GDALAbstractPipelineAlgorithm::CheckFirstAndLastStep(
    const std::vector<GDALPipelineStepAlgorithm *> &steps,
    bool forAutoComplete) const
{
    if (m_bExpectReadStep && !steps.front()->CanBeFirstStep())
    {
        std::set<CPLString> setFirstStepNames;
        for (const auto &stepName : GetStepRegistry().GetNames())
        {
            auto alg = GetStepAlg(stepName);
            if (alg && alg->CanBeFirstStep() && stepName != "read")
            {
                setFirstStepNames.insert(CPLString(stepName)
                                             .replaceAll(RASTER_SUFFIX, "")
                                             .replaceAll(VECTOR_SUFFIX, ""));
            }
        }
        std::vector<std::string> firstStepNames{"read"};
        for (const std::string &s : setFirstStepNames)
            firstStepNames.push_back(s);

        std::string msg = "First step should be ";
        for (size_t i = 0; i < firstStepNames.size(); ++i)
        {
            if (i == firstStepNames.size() - 1)
                msg += " or ";
            else if (i > 0)
                msg += ", ";
            msg += '\'';
            msg += firstStepNames[i];
            msg += '\'';
        }

        ReportError(CE_Failure, CPLE_AppDefined, "%s", msg.c_str());
        return false;
    }

    if (!m_bExpectReadStep)
    {
        if (steps.front()->CanBeFirstStep())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "No read-like step like '%s' is allowed",
                        steps.front()->GetName().c_str());
            return false;
        }
    }

    if (forAutoComplete)
        return true;

    if (m_eLastStepAsWrite == StepConstraint::CAN_NOT_BE)
    {
        if (steps.back()->CanBeLastStep() && !steps.back()->CanBeMiddleStep())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "No write-like step like '%s' is allowed",
                        steps.back()->GetName().c_str());
            return false;
        }
    }

    for (size_t i = 1; i < steps.size() - 1; ++i)
    {
        if (!steps[i]->CanBeMiddleStep())
        {
            if (steps[i]->CanBeFirstStep() && m_bExpectReadStep)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Only first step can be '%s'",
                            steps[i]->GetName().c_str());
            }
            else if (steps[i]->CanBeLastStep() &&
                     m_eLastStepAsWrite != StepConstraint::CAN_NOT_BE)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Only last step can be '%s'",
                            steps[i]->GetName().c_str());
            }
            else
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "'%s' is not allowed as an intermediate step",
                            steps[i]->GetName().c_str());
                return false;
            }
        }
    }

    if (steps.size() >= 2 && steps.back()->CanBeFirstStep() &&
        !steps.back()->CanBeLastStep())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "'%s' is only allowed as a first step",
                    steps.back()->GetName().c_str());
        return false;
    }

    if (m_eLastStepAsWrite == StepConstraint::MUST_BE &&
        !steps.back()->CanBeLastStep())
    {
        std::set<CPLString> setLastStepNames;
        for (const auto &stepName : GetStepRegistry().GetNames())
        {
            auto alg = GetStepAlg(stepName);
            if (alg && alg->CanBeLastStep() && stepName != "write")
            {
                setLastStepNames.insert(CPLString(stepName)
                                            .replaceAll(RASTER_SUFFIX, "")
                                            .replaceAll(VECTOR_SUFFIX, ""));
            }
        }
        std::vector<std::string> lastStepNames{"write"};
        for (const std::string &s : setLastStepNames)
            lastStepNames.push_back(s);

        std::string msg = "Last step should be ";
        for (size_t i = 0; i < lastStepNames.size(); ++i)
        {
            if (i == lastStepNames.size() - 1)
                msg += " or ";
            else if (i > 0)
                msg += ", ";
            msg += '\'';
            msg += lastStepNames[i];
            msg += '\'';
        }

        ReportError(CE_Failure, CPLE_AppDefined, "%s", msg.c_str());
        return false;
    }

    return true;
}

/************************************************************************/
/*             GDALAbstractPipelineAlgorithm::GetStepAlg()              */
/************************************************************************/

std::unique_ptr<GDALPipelineStepAlgorithm>
GDALAbstractPipelineAlgorithm::GetStepAlg(const std::string &name) const
{
    auto alg = GetStepRegistry().Instantiate(name);
    return std::unique_ptr<GDALPipelineStepAlgorithm>(
        cpl::down_cast<GDALPipelineStepAlgorithm *>(alg.release()));
}

/************************************************************************/
/*      GDALAbstractPipelineAlgorithm::ParseCommandLineArguments()      */
/************************************************************************/

bool GDALAbstractPipelineAlgorithm::ParseCommandLineArguments(
    const std::vector<std::string> &argsIn)
{
    return ParseCommandLineArguments(argsIn, /*forAutoComplete=*/false);
}

bool GDALAbstractPipelineAlgorithm::ParseCommandLineArguments(
    const std::vector<std::string> &argsIn, bool forAutoComplete)
{
    std::vector<std::string> args = argsIn;

    if (IsCalledFromCommandLine())
    {
        m_eLastStepAsWrite = StepConstraint::MUST_BE;
    }

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

    bool foundStepMarker = false;

    for (size_t i = 0; i < args.size(); ++i)
    {
        const auto &arg = args[i];
        if (arg == "--pipeline")
        {
            if (i + 1 < args.size() &&
                CPLString(args[i + 1]).ifind(".json") != std::string::npos)
                break;
            return GDALAlgorithm::ParseCommandLineArguments(args);
        }

        else if (cpl::starts_with(arg, "--pipeline="))
        {
            if (CPLString(arg).ifind(".json") != std::string::npos)
                break;
            return GDALAlgorithm::ParseCommandLineArguments(args);
        }

        // gdal pipeline [--quiet] "read poly.gpkg ..."
        if (arg.find("read ") == 0)
            return GDALAlgorithm::ParseCommandLineArguments(args);

        if (arg == "!")
            foundStepMarker = true;
    }

    bool runExistingPipeline = false;
    if (!foundStepMarker && !m_executionForStreamOutput)
    {
        std::string osCommandLine;
        for (const auto &arg : args)
        {
            if (((!arg.empty() && arg[0] != '-') ||
                 cpl::starts_with(arg, "--pipeline=")) &&
                CPLString(arg).ifind(".json") != std::string::npos)
            {
                bool ret;
                if (m_pipeline == arg)
                    ret = true;
                else
                {
                    const std::string filename =
                        cpl::starts_with(arg, "--pipeline=")
                            ? arg.substr(strlen("--pipeline="))
                            : arg;
                    if (forAutoComplete)
                    {
                        SetParseForAutoCompletion();
                    }
                    ret = GDALAlgorithm::ParseCommandLineArguments(args) ||
                          forAutoComplete;
                    if (ret)
                    {
                        ret = m_pipeline == filename;
                    }
                }
                if (ret)
                {
                    CPLJSONDocument oDoc;
                    ret = oDoc.Load(m_pipeline);
                    if (ret)
                    {
                        osCommandLine =
                            oDoc.GetRoot().GetString("command_line");
                        if (osCommandLine.empty())
                        {
                            ReportError(CE_Failure, CPLE_AppDefined,
                                        "command_line missing in %s",
                                        m_pipeline.c_str());
                            return false;
                        }

                        for (const char *prefix :
                             {"gdal pipeline ", "gdal raster pipeline ",
                              "gdal vector pipeline "})
                        {
                            if (cpl::starts_with(osCommandLine, prefix))
                                osCommandLine =
                                    osCommandLine.substr(strlen(prefix));
                        }

                        if (oDoc.GetRoot().GetBool(
                                "relative_paths_relative_to_this_file", true))
                        {
                            SetReferencePathForRelativePaths(
                                CPLGetPathSafe(m_pipeline.c_str()).c_str());
                        }

                        runExistingPipeline = true;
                    }
                }
                if (ret)
                    break;
                else
                    return false;
            }
        }
        if (runExistingPipeline)
        {
            const CPLStringList aosArgs(
                CSLTokenizeString(osCommandLine.c_str()));

            args = aosArgs;
        }
    }

    if (!m_steps.empty())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "ParseCommandLineArguments() can only be called once per "
                    "instance.");
        return false;
    }

    const bool bIsGenericPipeline =
        (GetInputType() == (GDAL_OF_RASTER | GDAL_OF_VECTOR));

    struct Step
    {
        std::unique_ptr<GDALPipelineStepAlgorithm> alg{};
        std::vector<std::string> args{};
        bool alreadyChangedType = false;
        bool isSubAlgorithm = false;
    };

    const auto SetCurStepAlg =
        [this, bIsGenericPipeline](Step &curStep, const std::string &algName,
                                   bool firstStep)
    {
        if (bIsGenericPipeline)
        {
            if (algName == "read")
            {
                curStep.alg = std::make_unique<GDALRasterReadAlgorithm>(true);
            }
            else
            {
                curStep.alg = GetStepAlg(algName);
                if (!curStep.alg)
                    curStep.alg = GetStepAlg(algName + RASTER_SUFFIX);
            }
        }
        else
        {
            curStep.alg = GetStepAlg(algName);
        }
        if (!curStep.alg)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "unknown step name: %s",
                        algName.c_str());
            return false;
        }
        // We don't want to accept '_PIPE_' dataset placeholder for the first
        // step of a pipeline.
        curStep.alg->m_inputDatasetCanBeOmitted =
            !firstStep || !m_bExpectReadStep;
        curStep.alg->SetCallPath({algName});
        curStep.alg->SetReferencePathForRelativePaths(
            GetReferencePathForRelativePaths());
        return true;
    };

    std::vector<Step> steps;
    steps.resize(1);

    int nNestLevel = 0;
    std::vector<std::string> nestedPipelineArgs;

    for (const auto &argIn : args)
    {
        std::string arg(argIn);

        // If outputting to stdout, automatically turn off progress bar
        if (arg == "/vsistdout/")
        {
            auto quietArg = GetArg(GDAL_ARG_NAME_QUIET);
            if (quietArg && quietArg->GetType() == GAAT_BOOLEAN)
                quietArg->Set(true);
        }

        auto &curStep = steps.back();

        if (nNestLevel > 0)
        {
            if (arg == CLOSE_NESTED_PIPELINE)
            {
                if ((--nNestLevel) == 0)
                {
                    arg = BuildNestedPipeline(
                        curStep.alg.get(), nestedPipelineArgs, forAutoComplete);
                    if (arg.empty())
                    {
                        return false;
                    }
                }
                else
                {
                    nestedPipelineArgs.push_back(std::move(arg));
                    continue;
                }
            }
            else
            {
                if (arg == OPEN_NESTED_PIPELINE)
                {
                    if (++nNestLevel == MAX_NESTING_LEVEL)
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Too many nested pipelines");
                        return false;
                    }
                }
                nestedPipelineArgs.push_back(std::move(arg));
                continue;
            }
        }

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

        if (arg == "!" || arg == "|")
        {
            if (curStep.alg)
            {
                steps.resize(steps.size() + 1);
            }
        }
        else if (arg == OPEN_NESTED_PIPELINE)
        {
            if (!curStep.alg)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Open bracket must be placed where an input "
                            "dataset is expected");
                return false;
            }
            ++nNestLevel;
        }
        else if (arg == CLOSE_NESTED_PIPELINE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Closing bracket found without matching open bracket");
            return false;
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
            const std::string algName = arg.substr(strlen("+gdal="));
            if (!SetCurStepAlg(curStep, algName, steps.size() == 1))
                return false;
        }
#endif
        else if (!curStep.alg)
        {
            std::string algName = std::move(arg);
#ifdef GDAL_PIPELINE_PROJ_NOSTALGIA
            if (!algName.empty() && algName[0] == '+')
                algName = algName.substr(1);
#endif
            if (!SetCurStepAlg(curStep, algName, steps.size() == 1))
                return false;
        }
        else
        {
            if (curStep.alg->HasSubAlgorithms())
            {
                auto subAlg = std::unique_ptr<GDALPipelineStepAlgorithm>(
                    cpl::down_cast<GDALPipelineStepAlgorithm *>(
                        curStep.alg->InstantiateSubAlgorithm(arg).release()));
                if (!subAlg)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "'%s' is a unknown sub-algorithm of '%s'",
                                arg.c_str(), curStep.alg->GetName().c_str());
                    return false;
                }
                curStep.isSubAlgorithm = true;
                subAlg->m_inputDatasetCanBeOmitted =
                    steps.size() > 1 || !m_bExpectReadStep;
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
            curStep.args.push_back(std::move(arg));
        }
    }

    if (nNestLevel > 0)
    {
        if (forAutoComplete)
        {
            BuildNestedPipeline(steps.back().alg.get(), nestedPipelineArgs,
                                forAutoComplete);
            return true;
        }
        else
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Open bracket has no matching closing bracket");
            return false;
        }
    }

    // As we initially added a step without alg to bootstrap things, make
    // sure to remove it if it hasn't been filled, or the user has terminated
    // the pipeline with a '!' separator.
    if (!steps.back().alg)
        steps.pop_back();

    if (runExistingPipeline)
    {
        // Add a final "write" step if there is no explicit allowed last step
        if (!steps.empty() && !steps.back().alg->CanBeLastStep())
        {
            steps.resize(steps.size() + 1);
            steps.back().alg = GetStepAlg(
                std::string(GDALRasterWriteAlgorithm::NAME)
                    .append(bIsGenericPipeline ? RASTER_SUFFIX : ""));
            steps.back().alg->m_inputDatasetCanBeOmitted = true;
        }

        // Remove "--output-format=stream" and "streamed_dataset" if found
        if (steps.back().alg->GetName() == GDALRasterWriteAlgorithm::NAME)
        {
            for (auto oIter = steps.back().args.begin();
                 oIter != steps.back().args.end();)
            {
                if (*oIter == std::string("--")
                                  .append(GDAL_ARG_NAME_OUTPUT_FORMAT)
                                  .append("=stream") ||
                    *oIter == std::string("--")
                                  .append(GDAL_ARG_NAME_OUTPUT)
                                  .append("=streamed_dataset") ||
                    *oIter == "streamed_dataset")
                {
                    oIter = steps.back().args.erase(oIter);
                }
                else
                {
                    ++oIter;
                }
            }
        }
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

    if (m_eLastStepAsWrite == StepConstraint::MUST_BE)
    {
        if (!m_bExpectReadStep)
        {
            if (steps.empty())
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "At least one step must be provided in %s pipeline.",
                    m_bInnerPipeline ? "an inner" : "a");
                return false;
            }
        }
        else if (steps.size() < 2)
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
    }
    else
    {
        if (steps.empty())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "At least one step must be provided in %s pipeline.",
                        m_bInnerPipeline ? "an inner" : "a");
            return false;
        }

        if (m_eLastStepAsWrite == StepConstraint::CAN_NOT_BE &&
            steps.back().alg->CanBeLastStep() &&
            !steps.back().alg->CanBeMiddleStep())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Last step in %s pipeline must not be a "
                        "write-like step.",
                        m_bInnerPipeline ? "an inner" : "a");
            return false;
        }
    }

    std::vector<GDALPipelineStepAlgorithm *> stepAlgs;
    for (const auto &step : steps)
        stepAlgs.push_back(step.alg.get());
    if (!CheckFirstAndLastStep(stepAlgs, forAutoComplete))
        return false;  // CheckFirstAndLastStep emits an error

    for (auto &step : steps)
    {
        step.alg->SetReferencePathForRelativePaths(
            GetReferencePathForRelativePaths());
    }

    // Propagate input parameters set at the pipeline level to the
    // "read" step
    if (m_bExpectReadStep)
    {
        auto &step = steps.front();
        for (auto &arg : step.alg->GetArgs())
        {
            if (!arg->IsHidden())
            {
                auto pipelineArg =
                    const_cast<const GDALAbstractPipelineAlgorithm *>(this)
                        ->GetArg(arg->GetName());
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
    const auto SetWriteArgFromPipeline = [this, &steps]()
    {
        auto &step = steps.back();
        for (auto &arg : step.alg->GetArgs())
        {
            if (!arg->IsHidden())
            {
                auto pipelineArg =
                    const_cast<const GDALAbstractPipelineAlgorithm *>(this)
                        ->GetArg(arg->GetName());
                if (pipelineArg && pipelineArg->IsExplicitlySet() &&
                    pipelineArg->GetType() == arg->GetType())
                {
                    arg->SetSkipIfAlreadySet(true);
                    arg->SetFrom(*pipelineArg);
                }
            }
        }
    };

    if (m_eLastStepAsWrite != StepConstraint::CAN_NOT_BE &&
        steps.back().alg->CanBeLastStep())
    {
        SetWriteArgFromPipeline();
    }

    if (runExistingPipeline)
    {
        std::set<std::pair<Step *, std::string>> alreadyCleanedArgs;

        for (const auto &arg : GetArgs())
        {
            if (arg->IsUserProvided() ||
                ((arg->GetName() == GDAL_ARG_NAME_INPUT ||
                  arg->GetName() == GDAL_ARG_NAME_INPUT_LAYER ||
                  arg->GetName() == GDAL_ARG_NAME_OUTPUT ||
                  arg->GetName() == GDAL_ARG_NAME_OUTPUT_FORMAT) &&
                 arg->IsExplicitlySet()))
            {
                CPLStringList tokens(
                    CSLTokenizeString2(arg->GetName().c_str(), ".", 0));
                std::string stepName;
                std::string stepArgName;
                if (tokens.size() == 1 && IsReadSpecificArgument(tokens[0]))
                {
                    stepName = steps.front().alg->GetName();
                    stepArgName = tokens[0];
                }
                else if (tokens.size() == 1 &&
                         IsWriteSpecificArgument(tokens[0]))
                {
                    stepName = steps.back().alg->GetName();
                    stepArgName = tokens[0];
                }
                else if (tokens.size() == 2)
                {
                    stepName = tokens[0];
                    stepArgName = tokens[1];
                }
                else
                {
                    if (tokens.size() == 1)
                    {
                        const Step *matchingStep = nullptr;
                        for (auto &step : steps)
                        {
                            if (step.alg->GetArg(tokens[0]))
                            {
                                if (!matchingStep)
                                    matchingStep = &step;
                                else
                                {
                                    ReportError(
                                        CE_Failure, CPLE_AppDefined,
                                        "Ambiguous argument name '%s', because "
                                        "it is valid for several steps in the "
                                        "pipeline. It should be specified with "
                                        "the form "
                                        "<algorithm-name>.<argument-name>.",
                                        tokens[0]);
                                    return false;
                                }
                            }
                        }
                        if (!matchingStep)
                        {
                            ReportError(CE_Failure, CPLE_AppDefined,
                                        "No step in the pipeline has an "
                                        "argument named '%s'",
                                        tokens[0]);
                            return false;
                        }
                        stepName = matchingStep->alg->GetName();
                        stepArgName = tokens[0];
                    }
                    else
                    {
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "Invalid argument name '%s'. It should of the "
                            "form <algorithm-name>.<argument-name>.",
                            arg->GetName().c_str());
                        return false;
                    }
                }
                const auto nPosBracket = stepName.find('[');
                int iRequestedStepIdx = -1;
                if (nPosBracket != std::string::npos && stepName.back() == ']')
                {
                    iRequestedStepIdx =
                        atoi(stepName.c_str() + nPosBracket + 1);
                    stepName.resize(nPosBracket);
                }
                int iMatchingStepIdx = 0;
                Step *matchingStep = nullptr;
                for (auto &step : steps)
                {
                    if (step.alg->GetName() == stepName)
                    {
                        if (iRequestedStepIdx >= 0)
                        {
                            if (iRequestedStepIdx == iMatchingStepIdx)
                            {
                                matchingStep = &step;
                                break;
                            }
                            ++iMatchingStepIdx;
                        }
                        else if (matchingStep == nullptr)
                        {
                            matchingStep = &step;
                        }
                        else
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Argument '%s' is ambiguous as there are "
                                "several '%s' steps in the pipeline. Qualify "
                                "it as '%s[<zero-based-index>]' to remove "
                                "ambiguity.",
                                arg->GetName().c_str(), stepName.c_str(),
                                stepName.c_str());
                            return false;
                        }
                    }
                }
                if (!matchingStep)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Argument '%s' refers to a non-existing '%s' "
                                "step in the pipeline.",
                                arg->GetName().c_str(), tokens[0]);
                    return false;
                }

                auto &step = *matchingStep;
                std::string stepArgNameDashDash =
                    std::string("--").append(stepArgName);

                auto oKeyPair = std::make_pair(matchingStep, stepArgName);
                if (!cpl::contains(alreadyCleanedArgs, oKeyPair))
                {
                    alreadyCleanedArgs.insert(std::move(oKeyPair));

                    std::vector<GDALAlgorithmArg *> positionalArgs;
                    for (auto &stepArg : step.alg->GetArgs())
                    {
                        if (stepArg->IsPositional())
                            positionalArgs.push_back(stepArg.get());
                    }

                    // Remove step arguments that match the user override
                    const std::string stepArgNameDashDashEqual =
                        stepArgNameDashDash + '=';
                    size_t idxPositional = 0;
                    for (auto oIter = step.args.begin();
                         oIter != step.args.end();)
                    {
                        const auto &iterArgName = *oIter;
                        if (iterArgName == stepArgNameDashDash)
                        {
                            oIter = step.args.erase(oIter);
                            auto stepArg = step.alg->GetArg(stepArgName);
                            if (stepArg && stepArg->GetType() != GAAT_BOOLEAN)
                            {
                                if (oIter != step.args.end())
                                    oIter = step.args.erase(oIter);
                            }
                        }
                        else if (cpl::starts_with(iterArgName,
                                                  stepArgNameDashDashEqual))
                        {
                            oIter = step.args.erase(oIter);
                        }
                        else if (!iterArgName.empty() && iterArgName[0] == '-')
                        {
                            const auto equalPos = iterArgName.find('=');
                            auto stepArg = step.alg->GetArg(
                                equalPos == std::string::npos
                                    ? iterArgName
                                    : iterArgName.substr(0, equalPos));
                            ++oIter;
                            if (stepArg && equalPos == std::string::npos &&
                                stepArg->GetType() != GAAT_BOOLEAN)
                            {
                                if (oIter != step.args.end())
                                    ++oIter;
                            }
                        }
                        else if (idxPositional < positionalArgs.size())
                        {
                            if (positionalArgs[idxPositional]->GetName() ==
                                stepArgName)
                            {
                                oIter = step.args.erase(oIter);
                            }
                            else
                            {
                                ++oIter;
                            }
                            ++idxPositional;
                        }
                        else
                        {
                            ++oIter;
                        }
                    }
                }

                if (arg->IsUserProvided())
                {
                    // Add user override
                    step.args.push_back(std::move(stepArgNameDashDash));
                    auto stepArg = step.alg->GetArg(stepArgName);
                    if (stepArg && stepArg->GetType() != GAAT_BOOLEAN)
                    {
                        step.args.push_back(arg->Get<std::string>());
                    }
                }
            }
        }
    }

    int nInitialDatasetType = 0;
    if (bIsGenericPipeline)
    {
        if (!m_bExpectReadStep)
        {
            CPLAssert(m_inputDataset.size() == 1 &&
                      m_inputDataset[0].GetDatasetRef());
            if (m_inputDataset[0].GetDatasetRef()->GetRasterCount() > 0)
            {
                nInitialDatasetType = GDAL_OF_RASTER;
            }
            else if (m_inputDataset[0].GetDatasetRef()->GetLayerCount() > 0)
            {
                nInitialDatasetType = GDAL_OF_VECTOR;
            }
        }

        // Parse each step, but without running the validation
        int nDatasetType = nInitialDatasetType;
        bool firstStep = nDatasetType == 0;

        for (auto &step : steps)
        {
            bool ret = false;
            CPLErrorAccumulator oAccumulator;
            bool hasTriedRaster = false;
            if (nDatasetType == 0 || nDatasetType == GDAL_OF_RASTER)
            {
                hasTriedRaster = true;
                [[maybe_unused]] auto context =
                    oAccumulator.InstallForCurrentScope();
                step.alg->m_skipValidationInParseCommandLine = true;
                ret = step.alg->ParseCommandLineArguments(step.args);
                if (ret && nDatasetType == 0 && forAutoComplete)
                {
                    ret = step.alg->ValidateArguments();
                    if (ret && firstStep &&
                        step.alg->m_inputDataset.size() == 1)
                    {
                        auto poDS = step.alg->m_inputDataset[0].GetDatasetRef();
                        if (poDS && poDS->GetLayerCount() > 0)
                            ret = false;
                    }
                    else if (!ret && firstStep)
                        ret = true;
                }
            }
            else if (!m_bExpectReadStep &&
                     nDatasetType == step.alg->GetInputType())
            {
                step.alg->m_skipValidationInParseCommandLine = true;
                ret = step.alg->ParseCommandLineArguments(step.args);
                if (!ret)
                    return false;
            }

            if (!ret)
            {
                auto algVector =
                    GetStepAlg(step.alg->GetName() + VECTOR_SUFFIX);
                if (algVector &&
                    (nDatasetType == 0 || nDatasetType == GDAL_OF_VECTOR))
                {
                    step.alg = std::move(algVector);
                    step.alg->m_inputDatasetCanBeOmitted =
                        !firstStep || !m_bExpectReadStep;
                    step.alg->m_skipValidationInParseCommandLine = true;
                    ret = step.alg->ParseCommandLineArguments(step.args);
                    if (ret)
                    {
                        step.alg->SetCallPath({step.alg->GetName()});
                        step.alg->SetReferencePathForRelativePaths(
                            GetReferencePathForRelativePaths());
                        step.alreadyChangedType = true;
                    }
                    else if (!forAutoComplete)
                        return false;
                }
                if (!ret && hasTriedRaster && !forAutoComplete)
                {
                    for (const auto &sError : oAccumulator.GetErrors())
                    {
                        CPLError(sError.type, sError.no, "%s",
                                 sError.msg.c_str());
                    }
                    return false;
                }
            }
            if (ret && forAutoComplete)
                nDatasetType = step.alg->GetOutputType();
            firstStep = false;
        }
    }
    else
    {
        for (auto &step : steps)
        {
            step.alg->m_skipValidationInParseCommandLine = true;
            if (!step.alg->ParseCommandLineArguments(step.args) &&
                !forAutoComplete)
                return false;
        }
    }

    // Evaluate "input" argument of "read" step, together with the "output"
    // argument of the "write" step, in case they point to the same dataset.
    auto inputArg = steps.front().alg->GetArg(GDAL_ARG_NAME_INPUT);
    if (inputArg && inputArg->IsExplicitlySet() &&
        inputArg->GetType() == GAAT_DATASET_LIST &&
        inputArg->Get<std::vector<GDALArgDatasetValue>>().size() == 1)
    {
        int nCountChangeFieldTypeStepsToBeRemoved = 0;
        std::string osTmpJSONFilename;

        // Check if there are steps like change-field-type just after the read
        // step. If so, we can convert them into a OGR_SCHEMA open option for
        // drivers that support it.
        auto &inputVals = inputArg->Get<std::vector<GDALArgDatasetValue>>();
        if (!inputVals[0].GetDatasetRef() && steps.size() >= 2 &&
            steps[0].alg->GetName() == GDALVectorReadAlgorithm::NAME &&
            !steps.back().alg->IsGDALGOutput())
        {
            auto openOptionArgs =
                steps.front().alg->GetArg(GDAL_ARG_NAME_OPEN_OPTION);
            if (openOptionArgs && !openOptionArgs->IsExplicitlySet() &&
                openOptionArgs->GetType() == GAAT_STRING_LIST)
            {
                const auto &openOptionVals =
                    openOptionArgs->Get<std::vector<std::string>>();
                if (CPLStringList(openOptionVals)
                        .FetchNameValue("OGR_SCHEMA") == nullptr)
                {
                    CPLJSONArray oLayers;
                    for (size_t iStep = 1; iStep < steps.size(); ++iStep)
                    {
                        auto oObj =
                            steps[iStep].alg->Get_OGR_SCHEMA_OpenOption_Layer();
                        if (!oObj.IsValid())
                            break;
                        oLayers.Add(oObj);
                        ++nCountChangeFieldTypeStepsToBeRemoved;
                    }

                    if (nCountChangeFieldTypeStepsToBeRemoved > 0)
                    {
                        CPLJSONDocument oDoc;
                        oDoc.GetRoot().Set("layers", oLayers);
                        osTmpJSONFilename =
                            VSIMemGenerateHiddenFilename(nullptr);
                        // CPLDebug("GDAL", "OGR_SCHEMA: %s", oDoc.SaveAsString().c_str());
                        oDoc.Save(osTmpJSONFilename);

                        openOptionArgs->Set(std::vector<std::string>{
                            std::string("@OGR_SCHEMA=")
                                .append(osTmpJSONFilename)});
                    }
                }
            }
        }

        const bool bOK = steps.front().alg->ProcessDatasetArg(
                             inputArg, steps.back().alg.get()) ||
                         forAutoComplete;

        if (!osTmpJSONFilename.empty())
            VSIUnlink(osTmpJSONFilename.c_str());

        if (!bOK)
        {
            return false;
        }

        // Now check if the driver of the input dataset actually supports
        // the OGR_SCHEMA open option. If so, we can remove the steps from
        // the pipeline
        if (nCountChangeFieldTypeStepsToBeRemoved)
        {
            if (auto poDS = inputVals[0].GetDatasetRef())
            {
                if (auto poDriver = poDS->GetDriver())
                {
                    const char *pszOpenOptionList =
                        poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST);
                    if (pszOpenOptionList &&
                        strstr(pszOpenOptionList, "OGR_SCHEMA"))
                    {
                        CPLDebug("GDAL",
                                 "Merging %d step(s) as OGR_SCHEMA open option",
                                 nCountChangeFieldTypeStepsToBeRemoved);
                        steps.erase(steps.begin() + 1,
                                    steps.begin() + 1 +
                                        nCountChangeFieldTypeStepsToBeRemoved);
                    }
                }
            }
        }
    }

    if (bIsGenericPipeline)
    {
        int nLastStepOutputType = nInitialDatasetType;
        if (m_bExpectReadStep)
        {
            nLastStepOutputType = GDAL_OF_VECTOR;
            if (steps.front().alg->GetName() !=
                    std::string(GDALRasterReadAlgorithm::NAME) &&
                steps.front().alg->GetOutputType() == GDAL_OF_RASTER)
            {
                nLastStepOutputType = GDAL_OF_RASTER;
            }
            else
            {
                auto &inputDatasets = steps.front().alg->GetInputDatasets();
                if (!inputDatasets.empty())
                {
                    auto poSrcDS = inputDatasets[0].GetDatasetRef();
                    if (poSrcDS)
                    {
                        if (poSrcDS->GetRasterCount() != 0)
                            nLastStepOutputType = GDAL_OF_RASTER;
                    }
                }
            }
        }

        for (size_t i = (m_bExpectReadStep ? 1 : 0);
             !forAutoComplete && i < steps.size(); ++i)
        {
            if (!steps[i].alreadyChangedType && !steps[i].isSubAlgorithm &&
                GetStepAlg(steps[i].alg->GetName()) == nullptr)
            {
                auto newAlg = GetStepAlg(steps[i].alg->GetName() +
                                         (nLastStepOutputType == GDAL_OF_RASTER
                                              ? RASTER_SUFFIX
                                              : VECTOR_SUFFIX));
                CPLAssert(newAlg);

                if (steps[i].alg->GetName() ==
                    GDALTeeStepAlgorithmAbstract::NAME)
                {
                    const auto poSrcTeeAlg =
                        dynamic_cast<const GDALTeeStepAlgorithmAbstract *>(
                            steps[i].alg.get());
                    auto poDstTeeAlg =
                        dynamic_cast<GDALTeeStepAlgorithmAbstract *>(
                            newAlg.get());
                    CPLAssert(poSrcTeeAlg);
                    CPLAssert(poDstTeeAlg);
                    poDstTeeAlg->CopyFilenameBindingsFrom(poSrcTeeAlg);
                }

                steps[i].alg = std::move(newAlg);

                if (i == steps.size() - 1 &&
                    m_eLastStepAsWrite != StepConstraint::CAN_NOT_BE)
                {
                    SetWriteArgFromPipeline();
                }

                steps[i].alg->m_inputDatasetCanBeOmitted =
                    i > 0 || !m_bExpectReadStep;
                steps[i].alg->m_skipValidationInParseCommandLine = true;
                if (!steps[i].alg->ParseCommandLineArguments(steps[i].args))
                    return false;
                steps[i].alg->SetCallPath({steps[i].alg->GetName()});
                steps[i].alg->SetReferencePathForRelativePaths(
                    GetReferencePathForRelativePaths());
                if (IsCalledFromCommandLine())
                    steps[i].alg->SetCalledFromCommandLine();
                steps[i].alreadyChangedType = true;
            }
            else if (i > 0 &&
                     steps[i].alg->GetInputType() != nLastStepOutputType)
            {
                bool emitError = true;

                // Check if a dataset argument, which has as value the
                // placeholder value, has the same dataset type as the output
                // of the last step
                for (const auto &arg : steps[i].alg->GetArgs())
                {
                    if (!arg->IsOutput() &&
                        (arg->GetType() == GAAT_DATASET ||
                         arg->GetType() == GAAT_DATASET_LIST))
                    {
                        if (arg->GetType() == GAAT_DATASET)
                        {
                            if (arg->Get<GDALArgDatasetValue>().GetName() ==
                                GDAL_DATASET_PIPELINE_PLACEHOLDER_VALUE)
                            {
                                if ((arg->GetDatasetType() &
                                     nLastStepOutputType) != 0)
                                {
                                    emitError = false;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            CPLAssert(arg->GetType() == GAAT_DATASET_LIST);
                            auto &val =
                                arg->Get<std::vector<GDALArgDatasetValue>>();
                            if (val.size() == 1 &&
                                val[0].GetName() ==
                                    GDAL_DATASET_PIPELINE_PLACEHOLDER_VALUE)
                            {
                                if ((arg->GetDatasetType() &
                                     nLastStepOutputType) != 0)
                                {
                                    emitError = false;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (emitError)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Step '%s' expects a %s input dataset, but "
                                "previous step '%s' "
                                "generates a %s output dataset",
                                steps[i].alg->GetName().c_str(),
                                steps[i].alg->GetInputType() == GDAL_OF_RASTER
                                    ? "raster"
                                : steps[i].alg->GetInputType() == GDAL_OF_VECTOR
                                    ? "vector"
                                    : "unknown",
                                steps[i - 1].alg->GetName().c_str(),
                                nLastStepOutputType == GDAL_OF_RASTER ? "raster"
                                : nLastStepOutputType == GDAL_OF_VECTOR
                                    ? "vector"
                                    : "unknown");
                    return false;
                }
            }
            nLastStepOutputType = steps[i].alg->GetOutputType();
        }
    }

    for (const auto &step : steps)
    {
        if (!step.alg->ValidateArguments() && !forAutoComplete)
            return false;
    }

    for (auto &step : steps)
        m_steps.push_back(std::move(step.alg));

    return true;
}

/************************************************************************/
/*         GDALAbstractPipelineAlgorithm::BuildNestedPipeline()         */
/************************************************************************/

std::string GDALAbstractPipelineAlgorithm::BuildNestedPipeline(
    GDALPipelineStepAlgorithm *curAlg,
    std::vector<std::string> &nestedPipelineArgs, bool forAutoComplete)
{
    std::string datasetNameOut;
    CPLAssert(curAlg);

    auto nestedPipeline = CreateNestedPipeline();
    if (curAlg->GetName() == GDALTeeStepAlgorithmAbstract::NAME)
        nestedPipeline->m_bExpectReadStep = false;
    else
        nestedPipeline->m_eLastStepAsWrite = StepConstraint::CAN_NOT_BE;
    nestedPipeline->m_executionForStreamOutput = m_executionForStreamOutput;
    nestedPipeline->SetReferencePathForRelativePaths(
        GetReferencePathForRelativePaths());

    std::string argsStr = OPEN_NESTED_PIPELINE;
    for (const std::string &str : nestedPipelineArgs)
    {
        argsStr += ' ';
        argsStr += GDALAlgorithmArg::GetEscapedString(str);
    }
    argsStr += ' ';
    argsStr += CLOSE_NESTED_PIPELINE;

    if (curAlg->GetName() != GDALTeeStepAlgorithmAbstract::NAME)
    {
        if (!nestedPipeline->ParseCommandLineArguments(nestedPipelineArgs,
                                                       forAutoComplete) ||
            (!forAutoComplete && !nestedPipeline->Run()))
        {
            return datasetNameOut;
        }
        auto poDS = nestedPipeline->GetOutputDataset().GetDatasetRef();
        if (!poDS)
        {
            // That shouldn't happen normally for well-behaved algorithms, but
            // it doesn't hurt checking.
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Nested pipeline does not generate an output dataset");
            return datasetNameOut;
        }
        datasetNameOut =
            CPLSPrintf("$$nested_pipeline_%p$$", nestedPipeline.get());
        curAlg->m_oMapDatasetNameToDataset[datasetNameOut] = poDS;

        poDS->SetDescription(argsStr.c_str());
    }

    m_apoNestedPipelines.emplace_back(std::move(nestedPipeline));

    if (curAlg->GetName() == GDALTeeStepAlgorithmAbstract::NAME)
    {
        auto teeAlg = dynamic_cast<GDALTeeStepAlgorithmAbstract *>(curAlg);
        if (teeAlg)
        {
            datasetNameOut = std::move(argsStr);
            if (!teeAlg->BindFilename(datasetNameOut,
                                      m_apoNestedPipelines.back().get(),
                                      nestedPipelineArgs))
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Another identical nested pipeline exists");
                datasetNameOut.clear();
            }
        }
    }

    nestedPipelineArgs.clear();

    return datasetNameOut;
}

/************************************************************************/
/*           GDALAbstractPipelineAlgorithm::GetAutoComplete()           */
/************************************************************************/

std::vector<std::string>
GDALAbstractPipelineAlgorithm::GetAutoComplete(std::vector<std::string> &args,
                                               bool lastWordIsComplete,
                                               bool showAllOptions)
{
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        ParseCommandLineArguments(args, /*forAutoComplete=*/true);
    }
    VSIStatBufL sStat;
    if (!m_pipeline.empty() && VSIStatL(m_pipeline.c_str(), &sStat) == 0 &&
        !m_steps.empty() && !args.empty())
    {
        std::map<std::string, std::vector<GDALAlgorithm *>> mapSteps;
        for (const auto &step : m_steps)
        {
            mapSteps[step->GetName()].push_back(step.get());
        }

        std::vector<std::string> ret;
        const auto &lastArg = args.back();
        if (!lastArg.empty() && lastArg[0] == '-' &&
            lastArg.find('=') == std::string::npos && !lastWordIsComplete)
        {
            for (const auto &step : m_steps)
            {
                const int iterCount =
                    static_cast<int>(mapSteps[step->GetName()].size());
                for (int i = 0; i < iterCount; ++i)
                {
                    for (const auto &arg : step->GetArgs())
                    {
                        if (!arg->IsHiddenForCLI() &&
                            arg->GetCategory() != GAAC_COMMON)
                        {
                            std::string s = std::string("--");
                            if (!((step->GetName() == "read" &&
                                   IsReadSpecificArgument(
                                       arg->GetName().c_str())) ||
                                  (step->GetName() == "write" &&
                                   IsWriteSpecificArgument(
                                       arg->GetName().c_str()))))
                            {
                                s += step->GetName();
                                if (iterCount > 1)
                                {
                                    s += '[';
                                    s += std::to_string(i);
                                    s += ']';
                                }
                                s += '.';
                            }
                            s += arg->GetName();
                            if (arg->GetType() == GAAT_BOOLEAN)
                                ret.push_back(std::move(s));
                            else
                                ret.push_back(s + "=");
                        }
                    }
                }
            }
        }
        else if (cpl::starts_with(lastArg, "--") &&
                 lastArg.find('=') != std::string::npos && !lastWordIsComplete)
        {
            const auto nDotPos = lastArg.find('.');
            std::string stepName;
            std::string argName;
            int idx = 0;
            if (nDotPos != std::string::npos)
            {
                stepName = lastArg.substr(strlen("--"), nDotPos - strlen("--"));
                const auto nBracketPos = stepName.find('[');
                if (nBracketPos != std::string::npos)
                {
                    idx = atoi(stepName.c_str() + nBracketPos + 1);
                    stepName.resize(nBracketPos);
                }
                argName = "--" + lastArg.substr(nDotPos + 1);
            }
            else
            {
                argName = lastArg;
                for (const char *prefix : apszReadParametersPrefixOmitted)
                {
                    if (cpl::starts_with(lastArg.substr(strlen("--")),
                                         std::string(prefix) + "="))
                    {
                        stepName = "read";
                        break;
                    }
                }

                for (const char *prefix : apszWriteParametersPrefixOmitted)
                {
                    if (cpl::starts_with(lastArg.substr(strlen("--")),
                                         std::string(prefix) + "="))
                    {
                        stepName = "write";
                        break;
                    }
                }
            }

            auto iter = mapSteps.find(stepName);
            if (iter != mapSteps.end() && idx >= 0 &&
                static_cast<size_t>(idx) < iter->second.size())
            {
                auto &step = iter->second[idx];
                std::vector<std::string> subArgs;
                for (const auto &arg : step->GetArgs())
                {
                    std::string strArg;
                    if (arg->IsExplicitlySet() &&
                        arg->Serialize(strArg, /* absolutePath=*/false))
                    {
                        subArgs.push_back(std::move(strArg));
                    }
                }
                subArgs.push_back(std::move(argName));
                ret = step->GetAutoComplete(subArgs, lastWordIsComplete,
                                            showAllOptions);
            }
        }
        return ret;
    }
    else
    {
        std::vector<std::string> ret;
        std::set<std::string> setSuggestions;
        if (args.size() <= 1)
        {
            for (const std::string &name : GetStepRegistry().GetNames())
            {
                auto alg = GetStepRegistry().Instantiate(name);
                auto stepAlg =
                    dynamic_cast<GDALPipelineStepAlgorithm *>(alg.get());
                if (stepAlg && stepAlg->CanBeFirstStep())
                {
                    std::string suggestionName =
                        CPLString(name)
                            .replaceAll(RASTER_SUFFIX, "")
                            .replaceAll(VECTOR_SUFFIX, "");
                    if (!cpl::contains(setSuggestions, suggestionName))
                    {
                        if (!args.empty() && suggestionName == args[0])
                            return {};
                        if (args.empty() ||
                            cpl::starts_with(suggestionName, args[0]))
                        {
                            setSuggestions.insert(suggestionName);
                            ret.push_back(std::move(suggestionName));
                        }
                    }
                }
            }
        }
        else
        {
            int nDatasetType = GetInputType();
            constexpr int MIXED_TYPE = GDAL_OF_RASTER | GDAL_OF_VECTOR;
            const bool isMixedTypePipeline = nDatasetType == MIXED_TYPE;
            std::string lastStep = args[0];
            std::vector<std::string> lastArgs;
            bool firstStep = true;
            bool foundSlowStep = false;
            for (size_t i = 1; i < args.size(); ++i)
            {
                if (firstStep && isMixedTypePipeline &&
                    nDatasetType == MIXED_TYPE && !args[i].empty() &&
                    args[i][0] != '-')
                {
                    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                    auto poDS = std::unique_ptr<GDALDataset>(
                        GDALDataset::Open(args[i].c_str()));
                    if (poDS && poDS->GetLayerCount() > 0 &&
                        poDS->GetRasterCount() == 0)
                    {
                        nDatasetType = GDAL_OF_VECTOR;
                    }
                    else if (poDS && poDS->GetLayerCount() == 0 &&
                             (poDS->GetRasterCount() > 0 ||
                              poDS->GetMetadata("SUBDATASETS") != nullptr))
                    {
                        nDatasetType = GDAL_OF_RASTER;
                    }
                }
                lastArgs.push_back(args[i]);
                if (i + 1 < args.size() && args[i] == "!")
                {
                    firstStep = false;
                    ++i;
                    lastArgs.clear();
                    lastStep = args[i];
                    auto curAlg = GetStepAlg(lastStep);
                    if (isMixedTypePipeline && !curAlg)
                    {
                        if (nDatasetType == GDAL_OF_RASTER)
                            curAlg = GetStepAlg(lastStep + RASTER_SUFFIX);
                        else if (nDatasetType == GDAL_OF_VECTOR)
                            curAlg = GetStepAlg(lastStep + VECTOR_SUFFIX);
                    }
                    if (curAlg)
                    {
                        foundSlowStep =
                            foundSlowStep ||
                            !curAlg->IsNativelyStreamingCompatible();
                        nDatasetType = curAlg->GetOutputType();
                    }
                }
            }

            if (args.back() == "!" ||
                (args[args.size() - 2] == "!" && !GetStepAlg(args.back()) &&
                 !GetStepAlg(args.back() + RASTER_SUFFIX) &&
                 !GetStepAlg(args.back() + VECTOR_SUFFIX)))
            {
                for (const std::string &name : GetStepRegistry().GetNames())
                {
                    auto alg = GetStepRegistry().Instantiate(name);
                    auto stepAlg =
                        dynamic_cast<GDALPipelineStepAlgorithm *>(alg.get());
                    if (stepAlg && isMixedTypePipeline &&
                        nDatasetType != MIXED_TYPE &&
                        stepAlg->GetInputType() != nDatasetType)
                    {
                        continue;
                    }
                    if (stepAlg && !stepAlg->CanBeFirstStep())
                    {
                        std::string suggestionName =
                            CPLString(name)
                                .replaceAll(RASTER_SUFFIX, "")
                                .replaceAll(VECTOR_SUFFIX, "");
                        if (!cpl::contains(setSuggestions, suggestionName))
                        {
                            setSuggestions.insert(suggestionName);
                            ret.push_back(std::move(suggestionName));
                        }
                    }
                }
            }
            else
            {
                if (!foundSlowStep)
                {
                    // Try to run the pipeline so that the last step gets its
                    // input dataset.
                    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                    GDALPipelineStepRunContext ctxt;
                    RunStep(ctxt);
                    if (!m_steps.empty() &&
                        m_steps.back()->GetName() == lastStep)
                    {
                        return m_steps.back()->GetAutoComplete(
                            lastArgs, lastWordIsComplete,
                            /* showAllOptions = */ false);
                    }
                }

                auto curAlg = GetStepAlg(lastStep);
                if (isMixedTypePipeline && !curAlg)
                {
                    if (nDatasetType == GDAL_OF_RASTER)
                        curAlg = GetStepAlg(lastStep + RASTER_SUFFIX);
                    else if (nDatasetType == GDAL_OF_VECTOR)
                        curAlg = GetStepAlg(lastStep + VECTOR_SUFFIX);
                    else
                    {
                        for (const char *suffix :
                             {RASTER_SUFFIX, VECTOR_SUFFIX})
                        {
                            curAlg = GetStepAlg(lastStep + suffix);
                            if (curAlg)
                            {
                                for (const auto &v : curAlg->GetAutoComplete(
                                         lastArgs, lastWordIsComplete,
                                         /* showAllOptions = */ false))
                                {
                                    if (!cpl::contains(setSuggestions, v))
                                    {
                                        setSuggestions.insert(v);
                                        ret.push_back(std::move(v));
                                    }
                                }
                            }
                        }
                        curAlg.reset();
                    }
                }
                if (curAlg)
                {
                    ret = curAlg->GetAutoComplete(lastArgs, lastWordIsComplete,
                                                  /* showAllOptions = */ false);
                }
            }
        }
        return ret;
    }
}

/************************************************************************/
/*            GDALAbstractPipelineAlgorithm::SaveGDALGFile()            */
/************************************************************************/

bool GDALAbstractPipelineAlgorithm::SaveGDALGFile(
    const std::string &outFilename, std::string &outString) const
{
    std::string osCommandLine;

    for (const auto &path : GDALAlgorithm::m_callPath)
    {
        if (!osCommandLine.empty())
            osCommandLine += ' ';
        osCommandLine += path;
    }

    // Do not include the last step
    for (size_t i = 0; i + 1 < m_steps.size(); ++i)
    {
        const auto &step = m_steps[i];
        if (!step->IsNativelyStreamingCompatible())
        {
            GDALAlgorithm::ReportError(
                CE_Warning, CPLE_AppDefined,
                "Step %s is not natively streaming compatible, and "
                "may cause significant processing time at opening",
                step->GDALAlgorithm::GetName().c_str());
        }

        if (i > 0)
            osCommandLine += " !";
        for (const auto &path : step->GDALAlgorithm::m_callPath)
        {
            if (!osCommandLine.empty())
                osCommandLine += ' ';
            osCommandLine += path;
        }

        for (const auto &arg : step->GetArgs())
        {
            if (arg->IsExplicitlySet())
            {
                osCommandLine += ' ';
                std::string strArg;
                if (!arg->Serialize(strArg, /* absolutePath=*/false))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot serialize argument %s",
                             arg->GetName().c_str());
                    return false;
                }
                osCommandLine += strArg;
            }
        }
    }

    return GDALAlgorithm::SaveGDALG(outFilename, outString, osCommandLine);
}

/************************************************************************/
/*               GDALAbstractPipelineAlgorithm::RunStep()               */
/************************************************************************/

bool GDALAbstractPipelineAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    if (m_stepOnWhichHelpIsRequested)
    {
        printf(
            "%s",
            m_stepOnWhichHelpIsRequested->GetUsageForCLI(false).c_str()); /*ok*/
        return true;
    }

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

    // Handle output to GDALG file
    if (!m_steps.empty() && m_steps.back()->GetName() == "write")
    {
        if (m_steps.back()->IsGDALGOutput())
        {
            const auto outputArg = m_steps.back()->GetArg(GDAL_ARG_NAME_OUTPUT);
            const auto &filename =
                outputArg->Get<GDALArgDatasetValue>().GetName();
            const char *pszType = "";
            if (GDALDoesFileOrDatasetExist(filename.c_str(), &pszType))
            {
                const auto overwriteArg =
                    m_steps.back()->GetArg(GDAL_ARG_NAME_OVERWRITE);
                if (overwriteArg && overwriteArg->GetType() == GAAT_BOOLEAN)
                {
                    if (!overwriteArg->Get<bool>())
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s '%s' already exists. Specify the "
                                 "--overwrite option to overwrite it.",
                                 pszType, filename.c_str());
                        return false;
                    }
                }
            }

            std::string outStringUnused;
            return SaveGDALGFile(filename, outStringUnused);
        }

        const auto outputFormatArg =
            m_steps.back()->GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT);
        const auto outputArg = m_steps.back()->GetArg(GDAL_ARG_NAME_OUTPUT);
        if (outputArg && outputArg->GetType() == GAAT_DATASET &&
            outputArg->IsExplicitlySet())
        {
            const auto &outputFile = outputArg->Get<GDALArgDatasetValue>();
            bool isVRTOutput;
            if (outputFormatArg && outputFormatArg->GetType() == GAAT_STRING &&
                outputFormatArg->IsExplicitlySet())
            {
                const auto &val = outputFormatArg->Get<std::string>();
                isVRTOutput = EQUAL(val.c_str(), "vrt");
            }
            else
            {
                isVRTOutput = EQUAL(
                    CPLGetExtensionSafe(outputFile.GetName().c_str()).c_str(),
                    "vrt");
            }
            if (isVRTOutput && !outputFile.GetName().empty() &&
                m_steps.size() > 3)
            {
                ReportError(
                    CE_Failure, CPLE_NotSupported,
                    "VRT output is not supported when there are more than 3 "
                    "steps. Consider using the GDALG driver (files with "
                    ".gdalg.json extension)");
                return false;
            }
            if (isVRTOutput)
            {
                for (const auto &step : m_steps)
                {
                    if (!step->m_outputVRTCompatible)
                    {
                        step->ReportError(
                            CE_Failure, CPLE_NotSupported,
                            "VRT output is not supported. Consider using the "
                            "GDALG driver instead (files with .gdalg.json "
                            "extension)");
                        return false;
                    }
                }
            }
        }
    }

    if (m_executionForStreamOutput &&
        !CPLTestBool(
            CPLGetConfigOption("GDAL_ALGORITHM_ALLOW_WRITES_IN_STREAM", "NO")))
    {
        // For security reasons, to avoid that reading a .gdalg.json file writes
        // a file on the file system.
        for (const auto &step : m_steps)
        {
            if (step->GetName() == "write")
            {
                if (!EQUAL(step->m_format.c_str(), "stream"))
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "in streamed execution, --format "
                                "stream should be used");
                    return false;
                }
            }
            else if (step->GeneratesFilesFromUserInput())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Step '%s' not allowed in stream execution, unless "
                            "the GDAL_ALGORITHM_ALLOW_WRITES_IN_STREAM "
                            "configuration option is set.",
                            step->GetName().c_str());
                return false;
            }
        }
    }

    // Because of multiprocessing in gdal raster tile, make sure that all
    // steps before it are serialized in a .gdal.json file
    if (m_steps.size() >= 2 && m_steps.back()->SupportsInputMultiThreading() &&
        m_steps.back()
                ->GetArg(GDAL_ARG_NAME_NUM_THREADS_INT_HIDDEN)
                ->Get<int>() > 1 &&
        !(m_steps.size() == 2 && m_steps[0]->GetName() == "read"))
    {
        bool ret = false;
        auto poSrcDS = m_inputDataset.size() == 1
                           ? m_inputDataset[0].GetDatasetRef()
                           : nullptr;
        if (poSrcDS)
        {
            auto poSrcDriver = poSrcDS->GetDriver();
            if (!poSrcDriver || EQUAL(poSrcDriver->GetDescription(), "MEM"))
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "Cannot execute this pipeline in parallel mode due to "
                    "input dataset being a non-materialized dataset. "
                    "Materialize it first, or add '-j 1' to the last step "
                    "'tile'");
                return false;
            }
        }
        std::string outString;
        if (SaveGDALGFile(std::string(), outString))
        {
            const char *const apszAllowedDrivers[] = {"GDALG", nullptr};
            auto poCurDS = GDALDataset::Open(
                outString.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                apszAllowedDrivers);
            if (poCurDS)
            {
                auto &tileAlg = m_steps.back();
                tileAlg->m_inputDataset.clear();
                tileAlg->m_inputDataset.resize(1);
                tileAlg->m_inputDataset[0].Set(poCurDS);
                tileAlg->m_inputDataset[0].SetDatasetOpenedByAlgorithm();
                poCurDS->Release();
                ret = tileAlg->RunStep(ctxt);
                tileAlg->m_inputDataset[0].Close();
            }
        }
        return ret;
    }

    int countPipelinesWithProgress = 0;
    for (size_t i = (m_bExpectReadStep ? 0 : 1); i < m_steps.size(); ++i)
    {
        const bool bCanHandleNextStep =
            i < m_steps.size() - 1 &&
            !m_steps[i]->CanHandleNextStep(m_steps[i + 1].get());
        if (bCanHandleNextStep &&
            !m_steps[i + 1]->IsNativelyStreamingCompatible())
            ++countPipelinesWithProgress;
        else if (!m_steps[i]->IsNativelyStreamingCompatible())
            ++countPipelinesWithProgress;
        if (bCanHandleNextStep)
            ++i;
    }
    if (countPipelinesWithProgress == 0)
        countPipelinesWithProgress = 1;

    bool ret = true;
    GDALDataset *poCurDS = nullptr;
    int iCurStepWithProgress = 0;

    if (!m_bExpectReadStep)
    {
        CPLAssert(m_inputDataset.size() == 1);
        poCurDS = m_inputDataset[0].GetDatasetRef();
        CPLAssert(poCurDS);
    }

    GDALProgressFunc pfnProgress = ctxt.m_pfnProgress;
    void *pProgressData = ctxt.m_pProgressData;
    if (IsCalledFromCommandLine() && HasOutputString())
    {
        pfnProgress = nullptr;
        pProgressData = nullptr;
    }

    for (size_t i = 0; i < m_steps.size(); ++i)
    {
        auto &step = m_steps[i];
        if (i > 0 || poCurDS)
        {
            bool prevStepOutputSetToThisStep = false;
            for (auto &arg : step->GetArgs())
            {
                if (!arg->IsOutput() && (arg->GetType() == GAAT_DATASET ||
                                         arg->GetType() == GAAT_DATASET_LIST))
                {
                    if (arg->GetType() == GAAT_DATASET)
                    {
                        if ((arg->GetName() == GDAL_ARG_NAME_INPUT &&
                             !arg->IsExplicitlySet()) ||
                            arg->Get<GDALArgDatasetValue>().GetName() ==
                                GDAL_DATASET_PIPELINE_PLACEHOLDER_VALUE)
                        {
                            auto &val = arg->Get<GDALArgDatasetValue>();
                            if (val.GetDatasetRef())
                            {
                                // Shouldn't happen
                                ReportError(CE_Failure, CPLE_AppDefined,
                                            "Step nr %d (%s) has already an "
                                            "input dataset for argument %s",
                                            static_cast<int>(i),
                                            step->GetName().c_str(),
                                            arg->GetName().c_str());
                                return false;
                            }
                            prevStepOutputSetToThisStep = true;
                            val.Set(poCurDS);
                            arg->NotifyValueSet();
                        }
                    }
                    else
                    {
                        CPLAssert(arg->GetType() == GAAT_DATASET_LIST);
                        auto &val =
                            arg->Get<std::vector<GDALArgDatasetValue>>();
                        if ((arg->GetName() == GDAL_ARG_NAME_INPUT &&
                             !arg->IsExplicitlySet()) ||
                            (val.size() == 1 &&
                             val[0].GetName() ==
                                 GDAL_DATASET_PIPELINE_PLACEHOLDER_VALUE))
                        {
                            if (val.size() == 1 && val[0].GetDatasetRef())
                            {
                                // Shouldn't happen
                                ReportError(CE_Failure, CPLE_AppDefined,
                                            "Step nr %d (%s) has already an "
                                            "input dataset for argument %s",
                                            static_cast<int>(i),
                                            step->GetName().c_str(),
                                            arg->GetName().c_str());
                                return false;
                            }
                            prevStepOutputSetToThisStep = true;
                            val.clear();
                            val.resize(1);
                            val[0].Set(poCurDS);
                            arg->NotifyValueSet();
                        }
                    }
                }
            }
            if (!prevStepOutputSetToThisStep)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Step nr %d (%s) does not use input dataset from "
                            "previous step",
                            static_cast<int>(i), step->GetName().c_str());
                return false;
            }
        }

        if (i + 1 < m_steps.size() && step->m_outputDataset.GetDatasetRef() &&
            !step->OutputDatasetAllowedBeforeRunningStep())
        {
            // Shouldn't happen
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Step nr %d (%s) has already an output dataset",
                        static_cast<int>(i), step->GetName().c_str());
            return false;
        }

        const bool bCanHandleNextStep =
            i < m_steps.size() - 1 &&
            step->CanHandleNextStep(m_steps[i + 1].get());

        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)> pScaledData(
            nullptr, GDALDestroyScaledProgress);
        GDALPipelineStepRunContext stepCtxt;
        if ((bCanHandleNextStep &&
             m_steps[i + 1]->IsNativelyStreamingCompatible()) ||
            !step->IsNativelyStreamingCompatible())
        {
            pScaledData.reset(GDALCreateScaledProgress(
                iCurStepWithProgress /
                    static_cast<double>(countPipelinesWithProgress),
                (iCurStepWithProgress + 1) /
                    static_cast<double>(countPipelinesWithProgress),
                pfnProgress, pProgressData));
            ++iCurStepWithProgress;
            stepCtxt.m_pfnProgress = pScaledData ? GDALScaledProgress : nullptr;
            stepCtxt.m_pProgressData = pScaledData.get();
        }
        if (bCanHandleNextStep)
        {
            stepCtxt.m_poNextUsableStep = m_steps[i + 1].get();
        }
        if (i + 1 == m_steps.size() && m_stdout &&
            step->GetArg(GDAL_ARG_NAME_STDOUT) != nullptr)
        {
            step->m_stdout = true;
        }
        step->m_inputDatasetCanBeOmitted = false;
        if (!step->ValidateArguments() || !step->RunStep(stepCtxt))
        {
            ret = false;
            break;
        }
        poCurDS = step->m_outputDataset.GetDatasetRef();
        if (!poCurDS && !(i + 1 == m_steps.size() &&
                          (!step->m_output.empty() ||
                           step->GetArg(GDAL_ARG_NAME_STDOUT) != nullptr ||
                           step->GetName() == "compare")))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Step nr %d (%s) failed to produce an output dataset",
                        static_cast<int>(i), step->GetName().c_str());
            return false;
        }

        m_output += step->GetOutputString();

        if (bCanHandleNextStep)
        {
            ++i;
        }
    }

    if (pfnProgress && m_output.empty())
        pfnProgress(1.0, "", pProgressData);

    if (!m_output.empty())
    {
        auto outputStringArg = GetArg(GDAL_ARG_NAME_OUTPUT_STRING);
        if (outputStringArg && outputStringArg->GetType() == GAAT_STRING)
            outputStringArg->Set(m_output);
    }

    if (ret && poCurDS && !m_outputDataset.GetDatasetRef())
    {
        m_outputDataset.Set(poCurDS);
    }

    return ret;
}

/************************************************************************/
/*           GDALAbstractPipelineAlgorithm::HasOutputString()           */
/************************************************************************/

bool GDALAbstractPipelineAlgorithm::HasOutputString() const
{
    for (const auto &step : m_steps)
    {
        if (step->HasOutputString())
            return true;
    }
    return false;
}

/************************************************************************/
/*              GDALAbstractPipelineAlgorithm::Finalize()               */
/************************************************************************/

bool GDALAbstractPipelineAlgorithm::Finalize()
{
    bool ret = GDALPipelineStepAlgorithm::Finalize();
    for (auto &step : m_steps)
    {
        ret = step->Finalize() && ret;
    }
    return ret;
}

/************************************************************************/
/*           GDALAbstractPipelineAlgorithm::GetUsageAsJSON()            */
/************************************************************************/

std::string GDALAbstractPipelineAlgorithm::GetUsageAsJSON() const
{
    CPLJSONDocument oDoc;
    CPL_IGNORE_RET_VAL(oDoc.LoadMemory(GDALAlgorithm::GetUsageAsJSON()));

    CPLJSONArray jPipelineSteps;
    for (const std::string &name : GetStepRegistry().GetNames())
    {
        auto alg = GetStepAlg(name);
        if (!alg->IsHidden())
        {
            CPLJSONDocument oStepDoc;
            CPL_IGNORE_RET_VAL(oStepDoc.LoadMemory(alg->GetUsageAsJSON()));
            jPipelineSteps.Add(oStepDoc.GetRoot());
        }
    }
    oDoc.GetRoot().Add("pipeline_algorithms", jPipelineSteps);

    return oDoc.SaveAsString();
}

//! @endcond
