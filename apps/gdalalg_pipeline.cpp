/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_error.h"
#include "cpl_error_internal.h"
#include "gdalalg_abstract_pipeline.h"
#include "gdal_priv.h"

#include "gdalalg_raster_read.h"
#include "gdalalg_raster_mosaic.h"
#include "gdalalg_raster_stack.h"
#include "gdalalg_raster_write.h"

#include "gdalalg_vector_read.h"
#include "gdalalg_vector_concat.h"
#include "gdalalg_vector_sql.h"
#include "gdalalg_vector_write.h"

#include "gdalalg_raster_contour.h"
#include "gdalalg_raster_footprint.h"
#include "gdalalg_raster_polygonize.h"
#include "gdalalg_raster_info.h"
#include "gdalalg_vector_grid.h"
#include "gdalalg_vector_info.h"
#include "gdalalg_vector_rasterize.h"

#include <algorithm>
#include <cassert>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                     GDALPipelineStepAlgorithm()                      */
/************************************************************************/

GDALPipelineStepAlgorithm::GDALPipelineStepAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, const ConstructorOptions &options)
    : GDALAlgorithm(name, description, helpURL),
      m_standaloneStep(options.standaloneStep), m_constructorOptions(options)
{
}

/************************************************************************/
/*       GDALPipelineStepAlgorithm::AddRasterHiddenInputDatasetArg()    */
/************************************************************************/

void GDALPipelineStepAlgorithm::AddRasterHiddenInputDatasetArg()
{
    // Added so that "band" argument validation works, because
    // GDALAlgorithm must be able to retrieve the input dataset
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER,
                       /* positionalAndRequired = */ false)
        .SetHidden();
}

/************************************************************************/
/*             GDALPipelineStepAlgorithm::AddRasterInputArgs()          */
/************************************************************************/

void GDALPipelineStepAlgorithm::AddRasterInputArgs(
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
    auto &arg =
        AddInputDatasetArg(&m_inputDataset,
                           openForMixedRasterVector
                               ? (GDAL_OF_RASTER | GDAL_OF_VECTOR)
                               : GDAL_OF_RASTER,
                           /* positionalAndRequired = */ !hiddenForCLI,
                           m_constructorOptions.inputDatasetHelpMsg.c_str())
            .SetMinCount(1)
            .SetMaxCount(m_constructorOptions.inputDatasetMaxCount)
            .SetAutoOpenDataset(m_constructorOptions.autoOpenInputDatasets)
            .SetMetaVar(m_constructorOptions.inputDatasetMetaVar)
            .SetHiddenForCLI(hiddenForCLI);
    if (!m_constructorOptions.inputDatasetAlias.empty())
        arg.AddAlias(m_constructorOptions.inputDatasetAlias);
}

/************************************************************************/
/*             GDALPipelineStepAlgorithm::AddRasterOutputArgs()         */
/************************************************************************/

void GDALPipelineStepAlgorithm::AddRasterOutputArgs(bool hiddenForCLI)
{
    m_outputFormatArg =
        &(AddOutputFormatArg(&m_format, /* bStreamAllowed = */ true,
                             /* bGDALGAllowed = */ true)
              .AddMetadataItem(
                  GAAMDI_REQUIRED_CAPABILITIES,
                  {GDAL_DCAP_RASTER,
                   m_constructorOptions.outputFormatCreateCapability.c_str()})
              .SetHiddenForCLI(hiddenForCLI));
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER,
                        /* positionalAndRequired = */ !hiddenForCLI,
                        m_constructorOptions.outputDatasetHelpMsg.c_str())
        .SetHiddenForCLI(hiddenForCLI)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    AddCreationOptionsArg(&m_creationOptions).SetHiddenForCLI(hiddenForCLI);
    AddOverwriteArg(&m_overwrite).SetHiddenForCLI(hiddenForCLI);
}

/************************************************************************/
/*             GDALPipelineStepAlgorithm::AddVectorInputArgs()         */
/************************************************************************/

void GDALPipelineStepAlgorithm::AddVectorInputArgs(bool hiddenForCLI)
{
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR})
        .SetHiddenForCLI(hiddenForCLI);
    AddOpenOptionsArg(&m_openOptions).SetHiddenForCLI(hiddenForCLI);
    auto &datasetArg =
        AddInputDatasetArg(&m_inputDataset, GDAL_OF_VECTOR,
                           /* positionalAndRequired = */ !hiddenForCLI)
            .SetMinCount(1)
            .SetMaxCount(m_constructorOptions.inputDatasetMaxCount)
            .SetAutoOpenDataset(m_constructorOptions.autoOpenInputDatasets)
            .SetHiddenForCLI(hiddenForCLI);
    if (m_constructorOptions.addInputLayerNameArgument)
    {
        auto &layerArg = AddArg("input-layer", 'l', _("Input layer name(s)"),
                                &m_inputLayerNames)
                             .AddAlias("layer")
                             .SetHiddenForCLI(hiddenForCLI);
        SetAutoCompleteFunctionForLayerName(layerArg, datasetArg);
    }
}

/************************************************************************/
/*             GDALPipelineStepAlgorithm::AddVectorOutputArgs()         */
/************************************************************************/

void GDALPipelineStepAlgorithm::AddVectorOutputArgs(
    bool hiddenForCLI, bool shortNameOutputLayerAllowed)
{
    AddOutputFormatArg(&m_format, /* bStreamAllowed = */ true,
                       /* bGDALGAllowed = */ true)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE})
        .SetHiddenForCLI(hiddenForCLI);
    auto &outputDatasetArg =
        AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR,
                            /* positionalAndRequired = */ false)
            .SetHiddenForCLI(hiddenForCLI)
            .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    if (!hiddenForCLI)
        outputDatasetArg.SetPositional();
    if (!hiddenForCLI && m_constructorOptions.outputDatasetRequired)
        outputDatasetArg.SetRequired();
    if (!m_constructorOptions.outputDatasetMutualExclusionGroup.empty())
    {
        outputDatasetArg.SetMutualExclusionGroup(
            m_constructorOptions.outputDatasetMutualExclusionGroup);
    }
    AddCreationOptionsArg(&m_creationOptions).SetHiddenForCLI(hiddenForCLI);
    AddLayerCreationOptionsArg(&m_layerCreationOptions)
        .SetHiddenForCLI(hiddenForCLI);
    AddOverwriteArg(&m_overwrite).SetHiddenForCLI(hiddenForCLI);
    auto &updateArg = AddUpdateArg(&m_update).SetHiddenForCLI(hiddenForCLI);
    if (!m_constructorOptions.updateMutualExclusionGroup.empty())
    {
        updateArg.SetMutualExclusionGroup(
            m_constructorOptions.updateMutualExclusionGroup);
    }
    AddOverwriteLayerArg(&m_overwriteLayer).SetHiddenForCLI(hiddenForCLI);
    AddAppendLayerArg(&m_appendLayer).SetHiddenForCLI(hiddenForCLI);
    if (GetName() != GDALVectorSQLAlgorithm::NAME &&
        GetName() != GDALVectorConcatAlgorithm::NAME)
    {
        AddArg("output-layer", shortNameOutputLayerAllowed ? 'l' : 0,
               _("Output layer name"), &m_outputLayerName)
            .AddHiddenAlias("nln")  // For ogr2ogr nostalgic people
            .SetHiddenForCLI(hiddenForCLI);
    }
}

/************************************************************************/
/*                 GDALPipelineStepAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALPipelineStepAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                        void *pProgressData)
{
    if (m_standaloneStep)
    {
        std::unique_ptr<GDALPipelineStepAlgorithm> readAlg;
        if (GetInputType() == GDAL_OF_RASTER)
            readAlg = std::make_unique<GDALRasterReadAlgorithm>();
        else
            readAlg = std::make_unique<GDALVectorReadAlgorithm>();
        for (auto &arg : readAlg->GetArgs())
        {
            auto stepArg = GetArg(arg->GetName());
            if (stepArg && stepArg->IsExplicitlySet())
            {
                arg->SetSkipIfAlreadySet(true);
                arg->SetFrom(*stepArg);
            }
        }

        std::unique_ptr<GDALPipelineStepAlgorithm> writeAlg;
        if (GetOutputType() == GDAL_OF_RASTER)
        {
            if (GetName() == GDALRasterInfoAlgorithm::NAME)
                writeAlg = std::make_unique<GDALRasterInfoAlgorithm>();
            else
                writeAlg = std::make_unique<GDALRasterWriteAlgorithm>();
        }
        else
        {
            if (GetName() == GDALVectorInfoAlgorithm::NAME)
                writeAlg = std::make_unique<GDALVectorInfoAlgorithm>();
            else
                writeAlg = std::make_unique<GDALVectorWriteAlgorithm>();
        }
        for (auto &arg : writeAlg->GetArgs())
        {
            auto stepArg = GetArg(arg->GetName());
            if (stepArg && stepArg->IsExplicitlySet())
            {
                arg->SetSkipIfAlreadySet(true);
                arg->SetFrom(*stepArg);
            }
        }

        const bool bIsStreaming = m_format == "stream";

        // Already checked by GDALAlgorithm::Run()
        CPLAssert(!m_executionForStreamOutput || bIsStreaming);

        bool ret = false;
        if (!m_outputVRTCompatible &&
            (EQUAL(m_format.c_str(), "VRT") ||
             (m_format.empty() &&
              EQUAL(CPLGetExtensionSafe(m_outputDataset.GetName().c_str())
                        .c_str(),
                    "VRT"))))
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "VRT output is not supported. Consider using the "
                        "GDALG driver instead (files with .gdalg.json "
                        "extension)");
        }
        else if (readAlg->Run())
        {
            auto outputArg = GetArg(GDAL_ARG_NAME_OUTPUT);
            const bool bOutputSpecified =
                outputArg && outputArg->IsExplicitlySet();

            m_inputDataset.clear();
            m_inputDataset.resize(1);
            m_inputDataset[0].Set(readAlg->m_outputDataset.GetDatasetRef());
            if (bOutputSpecified)
                m_outputDataset.Set(nullptr);

            std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                pScaledData(nullptr, GDALDestroyScaledProgress);

            const bool bCanHandleNextStep =
                !bIsStreaming && CanHandleNextStep(writeAlg.get());

            if (pfnProgress &&
                (bCanHandleNextStep || !IsNativelyStreamingCompatible()))
            {
                pScaledData.reset(GDALCreateScaledProgress(
                    0.0, bIsStreaming || bCanHandleNextStep ? 1.0 : 0.5,
                    pfnProgress, pProgressData));
            }

            GDALPipelineStepRunContext stepCtxt;
            stepCtxt.m_pfnProgress = pScaledData ? GDALScaledProgress : nullptr;
            stepCtxt.m_pProgressData = pScaledData.get();
            if (bCanHandleNextStep)
                stepCtxt.m_poNextUsableStep = writeAlg.get();
            if (RunPreStepPipelineValidations() && RunStep(stepCtxt))
            {
                if (bIsStreaming || bCanHandleNextStep || !bOutputSpecified)
                {
                    ret = true;
                }
                else
                {
                    writeAlg->m_outputVRTCompatible = m_outputVRTCompatible;
                    writeAlg->m_inputDataset.clear();
                    writeAlg->m_inputDataset.resize(1);
                    writeAlg->m_inputDataset[0].Set(
                        m_outputDataset.GetDatasetRef());
                    if (pfnProgress)
                    {
                        pScaledData.reset(GDALCreateScaledProgress(
                            IsNativelyStreamingCompatible() ? 0.0 : 0.5, 1.0,
                            pfnProgress, pProgressData));
                    }
                    stepCtxt.m_pfnProgress =
                        pScaledData ? GDALScaledProgress : nullptr;
                    stepCtxt.m_pProgressData = pScaledData.get();
                    if (writeAlg->ValidateArguments() &&
                        writeAlg->RunStep(stepCtxt))
                    {
                        if (pfnProgress)
                            pfnProgress(1.0, "", pProgressData);

                        m_outputDataset.Set(
                            writeAlg->m_outputDataset.GetDatasetRef());
                        ret = true;
                    }
                }
            }
        }

        return ret;
    }
    else
    {
        GDALPipelineStepRunContext stepCtxt;
        stepCtxt.m_pfnProgress = pfnProgress;
        stepCtxt.m_pProgressData = pProgressData;
        return RunPreStepPipelineValidations() && RunStep(stepCtxt);
    }
}

/************************************************************************/
/*                          ProcessGDALGOutput()                        */
/************************************************************************/

GDALAlgorithm::ProcessGDALGOutputRet
GDALPipelineStepAlgorithm::ProcessGDALGOutput()
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
/*          GDALPipelineStepAlgorithm::CheckSafeForStreamOutput()       */
/************************************************************************/

bool GDALPipelineStepAlgorithm::CheckSafeForStreamOutput()
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
/*                       GDALPipelineAlgorithm                          */
/************************************************************************/

class GDALPipelineAlgorithm final
    : public GDALAbstractPipelineAlgorithm<GDALPipelineStepAlgorithm>

{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION =
        "Process a dataset applying several steps.";
    static constexpr const char *HELP_URL = "/programs/gdal_pipeline.html";

    GDALPipelineAlgorithm()
        : GDALAbstractPipelineAlgorithm(
              NAME, DESCRIPTION, HELP_URL,
              ConstructorOptions().SetStandaloneStep(false))
    {
        m_supportsStreamedOutput = true;

        AddProgressArg();
        AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER | GDAL_OF_VECTOR,
                           /* positionalAndRequired = */ false)
            .SetMinCount(1)
            .SetMaxCount(INT_MAX)
            .SetHiddenForCLI();
        AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER | GDAL_OF_VECTOR,
                            /* positionalAndRequired = */ false)
            .SetHiddenForCLI()
            .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
        AddOutputFormatArg(&m_format, /* bStreamAllowed = */ true,
                           /* bGDALGAllowed = */ true)
            .SetHiddenForCLI();
        AddArg("pipeline", 0, _("Pipeline string"), &m_pipeline)
            .SetHiddenForCLI()
            .SetPositional();

        AddOutputStringArg(&m_output).SetHiddenForCLI();
        AddArg(
            "stdout", 0,
            _("Directly output on stdout (format=text mode only). If enabled, "
              "output-string will be empty"),
            &m_stdout)
            .SetHidden();

        GDALRasterPipelineAlgorithm::RegisterAlgorithms(m_stepRegistry, true);
        GDALVectorPipelineAlgorithm::RegisterAlgorithms(m_stepRegistry, true);
        m_stepRegistry.Register<GDALRasterContourAlgorithm>();
        m_stepRegistry.Register<GDALRasterFootprintAlgorithm>();
        m_stepRegistry.Register<GDALRasterPolygonizeAlgorithm>();
        m_stepRegistry.Register<GDALVectorGridAlgorithm>();
        m_stepRegistry.Register<GDALVectorRasterizeAlgorithm>();
    }

    // Declared to satisfy GDALPipelineStepAlgorithm, but not called as this
    // class is not an actual step, hence return value is "random"
    int GetInputType() const override
    {
        CPLAssert(false);
        return 0;
    }

    // Declared to satisfy GDALPipelineStepAlgorithm, but not called as this
    // class is not an actual step, hence return value is "random"
    int GetOutputType() const override
    {
        CPLAssert(false);
        return 0;
    }

  protected:
    bool
    ParseCommandLineArguments(const std::vector<std::string> &args) override;

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;
};

/************************************************************************/
/*           GDALPipelineAlgorithm::ParseCommandLineArguments()         */
/************************************************************************/

bool GDALPipelineAlgorithm::ParseCommandLineArguments(
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

        // gdal pipeline [--quiet] "read poly.gpkg ..."
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
        std::unique_ptr<GDALPipelineStepAlgorithm> alg{};
        std::vector<std::string> args{};
        bool alreadyChangedType = false;
        bool isSubAlgorithm = false;
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
        else if (!curStep.alg)
        {
            std::string algName = arg;
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
            if (!curStep.alg)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "unknown step name: %s", algName.c_str());
                return false;
            }
            curStep.alg->SetCallPath({std::move(algName)});
            curStep.alg->SetReferencePathForRelativePaths(
                GetReferencePathForRelativePaths());
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
                curStep.alg = std::move(subAlg);
                continue;
            }

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
        steps.back().alg->GetName() !=
            std::string(GDALRasterWriteAlgorithm::NAME).append(RASTER_SUFFIX))
    {
        steps.resize(steps.size() + 1);
        steps.back().alg = GetStepAlg(
            std::string(GDALRasterWriteAlgorithm::NAME).append(RASTER_SUFFIX));
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

    std::vector<GDALPipelineStepAlgorithm *> stepAlgs;
    for (const auto &step : steps)
        stepAlgs.push_back(step.alg.get());
    if (!CheckFirstStep(stepAlgs))
        return false;  // CheckFirstStep emits an error

    if (steps.back().alg->GetName() != GDALRasterWriteAlgorithm::NAME &&
        steps.back().alg->GetName() != GDALRasterInfoAlgorithm::NAME)
    {
        if (helpRequested)
        {
            steps.back().alg->ParseCommandLineArguments(steps.back().args);
            return false;
        }
        ReportError(
            CE_Failure, CPLE_AppDefined, "Last step should be '%s' or '%s'",
            GDALRasterWriteAlgorithm::NAME, GDALRasterInfoAlgorithm::NAME);
        return false;
    }
    for (size_t i = 0; i < steps.size() - 1; ++i)
    {
        if (steps[i].alg->GetName() == GDALRasterWriteAlgorithm::NAME ||
            steps[i].alg->GetName() == GDALRasterInfoAlgorithm::NAME)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Only last step can be '%s' or '%s'",
                        GDALRasterWriteAlgorithm::NAME,
                        GDALRasterInfoAlgorithm::NAME);
            return false;
        }
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
                if (pipelineArg && pipelineArg->IsExplicitlySet())
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
                auto pipelineArg = GetArg(arg->GetName());
                if (pipelineArg && pipelineArg->IsExplicitlySet())
                {
                    arg->SetSkipIfAlreadySet(true);
                    arg->SetFrom(*pipelineArg);
                }
            }
        }
    };

    SetWriteArgFromPipeline();

    // Parse each step, but without running the validation
    for (auto &step : steps)
    {
        bool ret;
        CPLErrorAccumulator oAccumulator;
        {
            [[maybe_unused]] auto context =
                oAccumulator.InstallForCurrentScope();
            step.alg->m_skipValidationInParseCommandLine = true;
            ret = step.alg->ParseCommandLineArguments(step.args);
        }
        if (!ret)
        {
            auto alg = GetStepAlg(step.alg->GetName() + VECTOR_SUFFIX);
            if (alg)
            {
                step.alg = std::move(alg);
                step.alg->m_skipValidationInParseCommandLine = true;
                ret = step.alg->ParseCommandLineArguments(step.args);
                if (!ret)
                    return false;
                step.alg->SetCallPath({step.alg->GetName()});
                step.alg->SetReferencePathForRelativePaths(
                    GetReferencePathForRelativePaths());
                step.alreadyChangedType = true;
            }
            else
            {
                for (const auto &sError : oAccumulator.GetErrors())
                {
                    CPLError(sError.type, sError.no, "%s", sError.msg.c_str());
                }
                return false;
            }
        }
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

    int nLastStepOutputType = GDAL_OF_VECTOR;
    if (steps.front().alg->GetName() !=
            std::string(GDALRasterReadAlgorithm::NAME) &&
        steps.front().alg->GetOutputType() == GDAL_OF_RASTER)
    {
        nLastStepOutputType = GDAL_OF_RASTER;
    }
    else
    {
        auto poSrcDS = steps.front().alg->GetInputDatasets()[0].GetDatasetRef();
        if (poSrcDS)
        {
            if (poSrcDS->GetRasterCount() != 0)
                nLastStepOutputType = GDAL_OF_RASTER;
        }
    }

    for (size_t i = 1; i < steps.size(); ++i)
    {
        if (!steps[i].alreadyChangedType && !steps[i].isSubAlgorithm &&
            GetStepAlg(steps[i].alg->GetName()) == nullptr)
        {
            steps[i].alg = GetStepAlg(steps[i].alg->GetName() +
                                      (nLastStepOutputType == GDAL_OF_RASTER
                                           ? RASTER_SUFFIX
                                           : VECTOR_SUFFIX));
            CPLAssert(steps[i].alg);

            if (i == steps.size() - 1)
            {
                SetWriteArgFromPipeline();
            }

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
        else if (steps[i].alg->GetInputType() != nLastStepOutputType)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Step '%s' expects a %s input dataset, but previous step '%s' "
                "generates a %s output dataset",
                steps[i].alg->GetName().c_str(),
                steps[i].alg->GetInputType() == GDAL_OF_RASTER ? "raster"
                                                               : "vector",
                steps[i - 1].alg->GetName().c_str(),
                nLastStepOutputType == GDAL_OF_RASTER ? "raster" : "vector");
            return false;
        }
        nLastStepOutputType = steps[i].alg->GetOutputType();
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
/*               GDALPipelineAlgorithm::GetUsageForCLI()                */
/************************************************************************/

std::string
GDALPipelineAlgorithm::GetUsageForCLI(bool shortUsage,
                                      const UsageOptions &usageOptions) const
{
    UsageOptions stepUsageOptions;
    stepUsageOptions.isPipelineStep = true;

    if (!m_helpDocCategory.empty() && m_helpDocCategory != "main")
    {
        auto alg = GetStepAlg(m_helpDocCategory);
        std::string ret;
        if (alg)
        {
            alg->SetCallPath({CPLString(m_helpDocCategory)
                                  .replaceAll(RASTER_SUFFIX, "")
                                  .replaceAll(VECTOR_SUFFIX, "")});
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

    ret += "\n<PIPELINE> is of the form: read|calc|concat|mosaic|stack "
           "[READ-OPTIONS] "
           "( ! <STEP-NAME> [STEP-OPTIONS] )* ! write [WRITE-OPTIONS]\n";

    if (m_helpDocCategory == "main")
    {
        return ret;
    }

    ret += '\n';
    ret += "Example: 'gdal pipeline --progress ! read in.tif ! \\\n";
    ret += "               rasterize --size 256 256 ! buffer 20 ! ";
    ret += "write out.gpkg --overwrite'\n";
    ret += '\n';
    ret += "Potential steps are:\n";

    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        assert(alg);
        auto [options, maxOptLen] = alg->GetArgNamesForCLI();
        stepUsageOptions.maxOptLen =
            std::max(stepUsageOptions.maxOptLen, maxOptLen);
    }

    {
        ret += '\n';
        auto alg = std::make_unique<GDALRasterReadAlgorithm>();
        alg->SetCallPath({alg->GetName()});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }
    {
        ret += '\n';
        auto alg = std::make_unique<GDALVectorReadAlgorithm>();
        alg->SetCallPath({alg->GetName()});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }
    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        assert(alg);
        if (alg->CanBeFirstStep() && !alg->IsHidden() &&
            !STARTS_WITH(name.c_str(), GDALRasterReadAlgorithm::NAME))
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
        if (!alg->CanBeFirstStep() && !alg->IsHidden() &&
            !STARTS_WITH(name.c_str(), GDALRasterWriteAlgorithm::NAME))
        {
            ret += '\n';
            alg->SetCallPath({CPLString(alg->GetName())
                                  .replaceAll(RASTER_SUFFIX, "")
                                  .replaceAll(VECTOR_SUFFIX, "")});
            ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
    }
    for (const std::string &name :
         {std::string(GDALRasterInfoAlgorithm::NAME) + RASTER_SUFFIX,
          std::string(GDALRasterWriteAlgorithm::NAME) + RASTER_SUFFIX,
          std::string(GDALVectorInfoAlgorithm::NAME) + VECTOR_SUFFIX,
          std::string(GDALVectorWriteAlgorithm::NAME) + VECTOR_SUFFIX})
    {
        ret += '\n';
        auto alg = GetStepAlg(name);
        assert(alg);
        alg->SetCallPath({CPLString(alg->GetName())
                              .replaceAll(RASTER_SUFFIX, "")
                              .replaceAll(VECTOR_SUFFIX, "")});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }

    ret += GetUsageForCLIEnd();

    return ret;
}

GDAL_STATIC_REGISTER_ALG(GDALPipelineAlgorithm);

//! @endcond
