/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_pipeline.h"
#include "gdalalg_vector_read.h"
#include "gdalalg_vector_clip.h"
#include "gdalalg_vector_filter.h"
#include "gdalalg_vector_reproject.h"
#include "gdalalg_vector_select.h"
#include "gdalalg_vector_sql.h"
#include "gdalalg_vector_write.h"

#include "cpl_conv.h"
#include "cpl_string.h"

#include <algorithm>
#include <cassert>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*  GDALVectorPipelineStepAlgorithm::GDALVectorPipelineStepAlgorithm()  */
/************************************************************************/

GDALVectorPipelineStepAlgorithm::GDALVectorPipelineStepAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, bool standaloneStep)
    : GDALAlgorithm(name, description, helpURL),
      m_standaloneStep(standaloneStep)
{
    if (m_standaloneStep)
    {
        AddInputArgs(false);
        AddProgressArg();
        AddOutputArgs(false, false);
    }
}

/************************************************************************/
/*             GDALVectorPipelineStepAlgorithm::AddInputArgs()          */
/************************************************************************/

void GDALVectorPipelineStepAlgorithm::AddInputArgs(bool hiddenForCLI)
{
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR})
        .SetHiddenForCLI(hiddenForCLI);
    AddOpenOptionsArg(&m_openOptions).SetHiddenForCLI(hiddenForCLI);
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_VECTOR,
                       /* positionalAndRequired = */ !hiddenForCLI)
        .SetHiddenForCLI(hiddenForCLI);
    if (GetName() != "sql")
    {
        AddArg("input-layer", 'l', _("Input layer name(s)"), &m_inputLayerNames)
            .AddAlias("layer")
            .SetHiddenForCLI(hiddenForCLI);
    }
}

/************************************************************************/
/*             GDALVectorPipelineStepAlgorithm::AddOutputArgs()         */
/************************************************************************/

void GDALVectorPipelineStepAlgorithm::AddOutputArgs(
    bool hiddenForCLI, bool shortNameOutputLayerAllowed)
{
    AddOutputFormatArg(&m_format, true)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE})
        .SetHiddenForCLI(hiddenForCLI);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR,
                        /* positionalAndRequired = */ !hiddenForCLI)
        .SetHiddenForCLI(hiddenForCLI);
    m_outputDataset.SetInputFlags(GADV_NAME | GADV_OBJECT);
    AddCreationOptionsArg(&m_creationOptions).SetHiddenForCLI(hiddenForCLI);
    AddLayerCreationOptionsArg(&m_layerCreationOptions)
        .SetHiddenForCLI(hiddenForCLI);
    AddOverwriteArg(&m_overwrite).SetHiddenForCLI(hiddenForCLI);
    AddUpdateArg(&m_update).SetHiddenForCLI(hiddenForCLI);
    AddArg("overwrite-layer", 0,
           _("Whether overwriting existing layer is allowed"),
           &m_overwriteLayer)
        .SetDefault(false)
        .SetHiddenForCLI(hiddenForCLI);
    AddArg("append", 0, _("Whether appending to existing layer is allowed"),
           &m_appendLayer)
        .SetDefault(false)
        .SetHiddenForCLI(hiddenForCLI);
    if (GetName() != "sql")
    {
        AddArg("output-layer", shortNameOutputLayerAllowed ? 'l' : 0,
               _("Output layer name"), &m_outputLayerName)
            .AddHiddenAlias("nln")  // For ogr2ogr nostalgic people
            .SetHiddenForCLI(hiddenForCLI);
    }
}

/************************************************************************/
/*            GDALVectorPipelineStepAlgorithm::RunImpl()                */
/************************************************************************/

bool GDALVectorPipelineStepAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                              void *pProgressData)
{
    if (m_standaloneStep)
    {
        GDALVectorReadAlgorithm readAlg;
        for (auto &arg : readAlg.GetArgs())
        {
            auto stepArg = GetArg(arg->GetName());
            if (stepArg && stepArg->IsExplicitlySet())
            {
                arg->SetSkipIfAlreadySet(true);
                arg->SetFrom(*stepArg);
            }
        }

        GDALVectorWriteAlgorithm writeAlg;
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
                if (m_format == "stream")
                {
                    ret = true;
                }
                else
                {
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
/*        GDALVectorPipelineAlgorithm::GDALVectorPipelineAlgorithm()    */
/************************************************************************/

GDALVectorPipelineAlgorithm::GDALVectorPipelineAlgorithm()
    : GDALAbstractPipelineAlgorithm<GDALVectorPipelineStepAlgorithm>(
          NAME, DESCRIPTION, HELP_URL,
          /*standaloneStep=*/false)
{
    AddInputArgs(/* hiddenForCLI = */ true);
    AddProgressArg();
    AddArg("pipeline", 0, _("Pipeline string"), &m_pipeline)
        .SetHiddenForCLI()
        .SetPositional();
    AddOutputArgs(/* hiddenForCLI = */ true,
                  /* shortNameOutputLayerAllowed=*/false);

    m_stepRegistry.Register<GDALVectorReadAlgorithm>();
    m_stepRegistry.Register<GDALVectorWriteAlgorithm>();
    m_stepRegistry.Register<GDALVectorClipAlgorithm>();
    m_stepRegistry.Register<GDALVectorReprojectAlgorithm>();
    m_stepRegistry.Register<GDALVectorFilterAlgorithm>();
    m_stepRegistry.Register<GDALVectorSelectAlgorithm>();
    m_stepRegistry.Register<GDALVectorSQLAlgorithm>();
}

/************************************************************************/
/*       GDALVectorPipelineAlgorithm::ParseCommandLineArguments()       */
/************************************************************************/

bool GDALVectorPipelineAlgorithm::ParseCommandLineArguments(
    const std::vector<std::string> &args)
{
    if (args.size() == 1 && (args[0] == "-h" || args[0] == "--help" ||
                             args[0] == "help" || args[0] == "--json-usage"))
        return GDALAlgorithm::ParseCommandLineArguments(args);

    for (const auto &arg : args)
    {
        if (arg.find("--pipeline") == 0)
            return GDALAlgorithm::ParseCommandLineArguments(args);

        // gdal vector pipeline [--progress] "read poly.gpkg ..."
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
        std::unique_ptr<GDALVectorPipelineStepAlgorithm> alg{};
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
            std::string value = arg;

// #define GDAL_PIPELINE_NATURAL_LANGUAGE
#ifdef GDAL_PIPELINE_NATURAL_LANGUAGE
            // gdal vector pipeline "read [from] poly.gpkg, reproject [from EPSG:4326] to EPSG:32632 and write to out.gpkg with overwriting"
            if (value == "and")
            {
                steps.resize(steps.size() + 1);
            }
            else if (value == "from" && curStep.alg->GetName() == "read")
            {
                // do nothing
            }
            else if (value == "from" && curStep.alg->GetName() == "reproject")
            {
                curStep.args.push_back("--src-crs");
            }
            else if (value == "to" && curStep.alg->GetName() == "reproject")
            {
                curStep.args.push_back("--dst-crs");
            }
            else if (value == "to" && curStep.alg->GetName() == "write")
            {
                // do nothing
            }
            else if (value == "with" && curStep.alg->GetName() == "write")
            {
                // do nothing
            }
            else if (value == "overwriting" &&
                     curStep.alg->GetName() == "write")
            {
                curStep.args.push_back("--overwrite");
            }
            else if (!value.empty() && value.back() == ',')
            {
                curStep.args.push_back(value.substr(0, value.size() - 1));
                steps.resize(steps.size() + 1);
            }
            else
#endif

            {
                curStep.args.push_back(value);
            }
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

    if (steps.front().alg->GetName() != GDALVectorReadAlgorithm::NAME)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "First step should be '%s'",
                    GDALVectorReadAlgorithm::NAME);
        return false;
    }
    for (size_t i = 1; i < steps.size() - 1; ++i)
    {
        if (steps[i].alg->GetName() == GDALVectorReadAlgorithm::NAME)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Only first step can be '%s'",
                        GDALVectorReadAlgorithm::NAME);
            return false;
        }
    }
    if (steps.back().alg->GetName() != GDALVectorWriteAlgorithm::NAME)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Last step should be '%s'",
                    GDALVectorWriteAlgorithm::NAME);
        return false;
    }
    for (size_t i = 0; i < steps.size() - 1; ++i)
    {
        if (steps[i].alg->GetName() == GDALVectorWriteAlgorithm::NAME)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Only last step can be '%s'",
                        GDALVectorWriteAlgorithm::NAME);
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
/*            GDALVectorPipelineAlgorithm::GetUsageForCLI()             */
/************************************************************************/

std::string GDALVectorPipelineAlgorithm::GetUsageForCLI(
    bool shortUsage, const UsageOptions &usageOptions) const
{
    std::string ret = GDALAlgorithm::GetUsageForCLI(shortUsage, usageOptions);
    if (shortUsage)
        return ret;

    ret += "\n<PIPELINE> is of the form: read [READ-OPTIONS] "
           "( ! <STEP-NAME> [STEP-OPTIONS] )* ! write [WRITE-OPTIONS]\n";
    ret += '\n';
    ret += "Example: 'gdal vector pipeline --progress ! read in.gpkg ! \\\n";
    ret += "               reproject --dst-crs=EPSG:32632 ! ";
    ret += "write out.gpkg --overwrite'\n";
    ret += '\n';
    ret += "Potential steps are:\n";

    UsageOptions stepUsageOptions;
    stepUsageOptions.isPipelineStep = true;

    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        assert(alg);
        auto [options, maxOptLen] = alg->GetArgNamesForCLI();
        stepUsageOptions.maxOptLen =
            std::max(stepUsageOptions.maxOptLen, maxOptLen);
    }

    {
        const auto name = GDALVectorReadAlgorithm::NAME;
        ret += '\n';
        auto alg = GetStepAlg(name);
        assert(alg);
        alg->SetCallPath({name});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }
    for (const std::string &name : m_stepRegistry.GetNames())
    {
        if (name != GDALVectorReadAlgorithm::NAME &&
            name != GDALVectorWriteAlgorithm::NAME)
        {
            ret += '\n';
            auto alg = GetStepAlg(name);
            assert(alg);
            alg->SetCallPath({name});
            ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
    }
    {
        const auto name = GDALVectorWriteAlgorithm::NAME;
        ret += '\n';
        auto alg = GetStepAlg(name);
        assert(alg);
        alg->SetCallPath({name});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }

    return ret;
}

/************************************************************************/
/*                  GDALVectorPipelineOutputLayer                       */
/************************************************************************/

/************************************************************************/
/*                  GDALVectorPipelineOutputLayer()                     */
/************************************************************************/

GDALVectorPipelineOutputLayer::GDALVectorPipelineOutputLayer(OGRLayer &srcLayer)
    : m_srcLayer(srcLayer)
{
}

/************************************************************************/
/*             GDALVectorPipelineOutputLayer::ResetReading()            */
/************************************************************************/

void GDALVectorPipelineOutputLayer::ResetReading()
{
    m_srcLayer.ResetReading();
    m_pendingFeatures.clear();
    m_idxInPendingFeatures = 0;
}

/************************************************************************/
/*           GDALVectorPipelineOutputLayer::GetNextRawFeature()         */
/************************************************************************/

OGRFeature *GDALVectorPipelineOutputLayer::GetNextRawFeature()
{
    if (m_idxInPendingFeatures < m_pendingFeatures.size())
    {
        OGRFeature *poFeature =
            m_pendingFeatures[m_idxInPendingFeatures].release();
        ++m_idxInPendingFeatures;
        return poFeature;
    }
    m_pendingFeatures.clear();
    m_idxInPendingFeatures = 0;
    while (true)
    {
        auto poSrcFeature =
            std::unique_ptr<OGRFeature>(m_srcLayer.GetNextFeature());
        if (!poSrcFeature)
            return nullptr;
        TranslateFeature(std::move(poSrcFeature), m_pendingFeatures);
        if (!m_pendingFeatures.empty())
            break;
    }
    OGRFeature *poFeature = m_pendingFeatures[0].release();
    m_idxInPendingFeatures = 1;
    return poFeature;
}

/************************************************************************/
/*                 GDALVectorPipelineOutputDataset                      */
/************************************************************************/

/************************************************************************/
/*                 GDALVectorPipelineOutputDataset()                    */
/************************************************************************/

GDALVectorPipelineOutputDataset::GDALVectorPipelineOutputDataset(
    GDALDataset &srcDS)
    : m_srcDS(srcDS)
{
    SetDescription(m_srcDS.GetDescription());
}

/************************************************************************/
/*            GDALVectorPipelineOutputDataset::AddLayer()               */
/************************************************************************/

void GDALVectorPipelineOutputDataset::AddLayer(
    OGRLayer &oSrcLayer,
    std::unique_ptr<OGRLayerWithTranslateFeature> poNewLayer)
{
    m_layersToDestroy.push_back(std::move(poNewLayer));
    OGRLayerWithTranslateFeature *poNewLayerRaw =
        m_layersToDestroy.back().get();
    m_layers.push_back(poNewLayerRaw);
    m_mapSrcLayerToNewLayer[&oSrcLayer] = poNewLayerRaw;
}

/************************************************************************/
/*          GDALVectorPipelineOutputDataset::GetLayerCount()            */
/************************************************************************/

int GDALVectorPipelineOutputDataset::GetLayerCount()
{
    return static_cast<int>(m_layers.size());
}

/************************************************************************/
/*             GDALVectorPipelineOutputDataset::GetLayer()              */
/************************************************************************/

OGRLayer *GDALVectorPipelineOutputDataset::GetLayer(int idx)
{
    return idx >= 0 && idx < GetLayerCount() ? m_layers[idx] : nullptr;
}

/************************************************************************/
/*           GDALVectorPipelineOutputDataset::TestCapability()          */
/************************************************************************/

int GDALVectorPipelineOutputDataset::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCRandomLayerRead))
    {
        return m_srcDS.TestCapability(pszCap);
    }
    return false;
}

/************************************************************************/
/*             GDALVectorPipelineOutputDataset::ResetReading()          */
/************************************************************************/

void GDALVectorPipelineOutputDataset::ResetReading()
{
    m_srcDS.ResetReading();
    m_pendingFeatures.clear();
    m_idxInPendingFeatures = 0;
}

/************************************************************************/
/*            GDALVectorPipelineOutputDataset::GetNextFeature()         */
/************************************************************************/

OGRFeature *GDALVectorPipelineOutputDataset::GetNextFeature(
    OGRLayer **ppoBelongingLayer, double *pdfProgressPct,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (m_idxInPendingFeatures < m_pendingFeatures.size())
    {
        OGRFeature *poFeature =
            m_pendingFeatures[m_idxInPendingFeatures].release();
        if (ppoBelongingLayer)
            *ppoBelongingLayer = m_belongingLayer;
        ++m_idxInPendingFeatures;
        return poFeature;
    }

    m_pendingFeatures.clear();
    m_idxInPendingFeatures = 0;

    while (true)
    {
        OGRLayer *poSrcBelongingLayer = nullptr;
        auto poSrcFeature = std::unique_ptr<OGRFeature>(m_srcDS.GetNextFeature(
            &poSrcBelongingLayer, pdfProgressPct, pfnProgress, pProgressData));
        if (!poSrcFeature)
            return nullptr;
        auto iterToDstLayer = m_mapSrcLayerToNewLayer.find(poSrcBelongingLayer);
        if (iterToDstLayer != m_mapSrcLayerToNewLayer.end())
        {
            m_belongingLayer = iterToDstLayer->second;
            m_belongingLayer->TranslateFeature(std::move(poSrcFeature),
                                               m_pendingFeatures);
            if (!m_pendingFeatures.empty())
                break;
        }
    }
    OGRFeature *poFeature = m_pendingFeatures[0].release();
    if (ppoBelongingLayer)
        *ppoBelongingLayer = m_belongingLayer;
    m_idxInPendingFeatures = 1;
    return poFeature;
}

//! @endcond
