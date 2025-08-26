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
#include "gdalalg_raster_tile.h"
#include "gdalalg_vector_grid.h"
#include "gdalalg_vector_info.h"
#include "gdalalg_vector_rasterize.h"

#include <algorithm>
#include <cassert>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

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
/*                     IsReadSpecificArgument()                         */
/************************************************************************/

static bool IsReadSpecificArgument(const char *pszArgName)
{
    return std::find_if(std::begin(apszReadParametersPrefixOmitted),
                        std::end(apszReadParametersPrefixOmitted),
                        [pszArgName](const char *pszStr) {
                            return strcmp(pszStr, pszArgName) == 0;
                        }) != std::end(apszReadParametersPrefixOmitted);
}

/************************************************************************/
/*                     IsWriteSpecificArgument()                        */
/************************************************************************/

static bool IsWriteSpecificArgument(const char *pszArgName)
{
    return std::find_if(std::begin(apszWriteParametersPrefixOmitted),
                        std::end(apszWriteParametersPrefixOmitted),
                        [pszArgName](const char *pszStr) {
                            return strcmp(pszStr, pszArgName) == 0;
                        }) != std::end(apszWriteParametersPrefixOmitted);
}

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
        auto &layerArg = AddArg(GDAL_ARG_NAME_INPUT_LAYER, 'l',
                                _("Input layer name(s)"), &m_inputLayerNames)
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
        AddArg(GDAL_ARG_NAME_OUTPUT_LAYER,
               shortNameOutputLayerAllowed ? 'l' : 0, _("Output layer name"),
               &m_outputLayerName)
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
            if (stepArg && stepArg->IsExplicitlySet() &&
                stepArg->GetType() == arg->GetType())
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
            if (stepArg && stepArg->IsExplicitlySet() &&
                stepArg->GetType() == arg->GetType())
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
        AddArg("pipeline", 0, _("Pipeline string or filename"), &m_pipeline)
            .SetHiddenForCLI()
            .SetPositional();

        AddOutputStringArg(&m_output).SetHiddenForCLI();
        AddStdoutArg(&m_stdout);

        AllowArbitraryLongNameArgs();

        GDALRasterPipelineAlgorithm::RegisterAlgorithms(m_stepRegistry, true);
        GDALVectorPipelineAlgorithm::RegisterAlgorithms(m_stepRegistry, true);
        m_stepRegistry.Register<GDALRasterContourAlgorithm>();
        m_stepRegistry.Register<GDALRasterFootprintAlgorithm>();
        m_stepRegistry.Register<GDALRasterPolygonizeAlgorithm>();
        m_stepRegistry.Register<GDALVectorGridAlgorithm>();
        m_stepRegistry.Register<GDALVectorRasterizeAlgorithm>();
    }

    int GetInputType() const override
    {
        return GDAL_OF_RASTER | GDAL_OF_VECTOR;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_RASTER | GDAL_OF_VECTOR;
    }

    std::vector<std::string> GetAutoComplete(std::vector<std::string> &args,
                                             bool lastWordIsComplete,
                                             bool /* showAllOptions*/) override;

  protected:
    bool
    ParseCommandLineArguments(const std::vector<std::string> &args) override;

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

  private:
    bool ParseCommandLineArguments(const std::vector<std::string> &args,
                                   bool forAutoComplete);
};

/************************************************************************/
/*           GDALPipelineAlgorithm::ParseCommandLineArguments()         */
/************************************************************************/

bool GDALPipelineAlgorithm::ParseCommandLineArguments(
    const std::vector<std::string> &argsIn)
{
    return ParseCommandLineArguments(argsIn, /*forAutoComplete=*/false);
}

bool GDALPipelineAlgorithm::ParseCommandLineArguments(
    const std::vector<std::string> &argsIn, bool forAutoComplete)
{
    std::vector<std::string> args = argsIn;

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
        steps.back().args.push_back(
            std::string("--").append(GDAL_ARG_NAME_OUTPUT_FORMAT));
        steps.back().args.push_back("stream");
        steps.back().args.push_back("streamed_dataset");
    }

    else if (runExistingPipeline)
    {
        // Add a final "write" step if there is no explicit allowed last step
        if (!steps.empty() && !steps.back().alg->CanBeLastStep())
        {
            steps.resize(steps.size() + 1);
            steps.back().alg =
                GetStepAlg(std::string(GDALRasterWriteAlgorithm::NAME)
                               .append(RASTER_SUFFIX));
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

    std::vector<GDALPipelineStepAlgorithm *> stepAlgs;
    for (const auto &step : steps)
        stepAlgs.push_back(step.alg.get());
    if (!CheckFirstAndLastStep(stepAlgs))
        return false;  // CheckFirstAndLastStep emits an error

    // Propagate input parameters set at the pipeline level to the
    // "read" step
    {
        auto &step = steps.front();
        for (auto &arg : step.alg->GetArgs())
        {
            if (!arg->IsHidden())
            {
                auto pipelineArg =
                    const_cast<const GDALPipelineAlgorithm *>(this)->GetArg(
                        arg->GetName());
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
                    const_cast<const GDALPipelineAlgorithm *>(this)->GetArg(
                        arg->GetName());
                if (pipelineArg && pipelineArg->IsExplicitlySet() &&
                    pipelineArg->GetType() == arg->GetType())
                {
                    arg->SetSkipIfAlreadySet(true);
                    arg->SetFrom(*pipelineArg);
                }
            }
        }
    };

    SetWriteArgFromPipeline();

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
                    alreadyCleanedArgs.insert(oKeyPair);

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

    // Parse each step, but without running the validation
    int nDatasetType = 0;
    bool firstStep = true;
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
                if (ret && firstStep && step.alg->m_inputDataset.size() == 1)
                {
                    auto poDS = step.alg->m_inputDataset[0].GetDatasetRef();
                    if (poDS && poDS->GetLayerCount() > 0)
                        ret = false;
                }
            }
        }
        if (!ret)
        {
            auto algVector = GetStepAlg(step.alg->GetName() + VECTOR_SUFFIX);
            if (algVector &&
                (nDatasetType == 0 || nDatasetType == GDAL_OF_VECTOR))
            {
                step.alg = std::move(algVector);
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
                    CPLError(sError.type, sError.no, "%s", sError.msg.c_str());
                }
                return false;
            }
        }
        if (ret && forAutoComplete)
            nDatasetType = step.alg->GetOutputType();
        firstStep = false;
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

    for (size_t i = 1; !forAutoComplete && i < steps.size(); ++i)
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
        if (!step.alg->ValidateArguments() && !forAutoComplete)
            return false;
    }

    for (auto &step : steps)
        m_steps.push_back(std::move(step.alg));

    return true;
}

/************************************************************************/
/*                GDALPipelineAlgorithm::GetAutoComplete()              */
/************************************************************************/

std::vector<std::string>
GDALPipelineAlgorithm::GetAutoComplete(std::vector<std::string> &args,
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
        return GDALAbstractPipelineAlgorithm::GetAutoComplete(
            args, lastWordIsComplete, showAllOptions);
    }
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

    ret +=
        "\n<PIPELINE> is of the form: read|calc|concat|mosaic|stack "
        "[READ-OPTIONS] "
        "( ! <STEP-NAME> [STEP-OPTIONS] )* ! write!info!tile [WRITE-OPTIONS]\n";

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
        if (!alg->CanBeFirstStep() && !alg->CanBeLastStep() && !alg->IsHidden())
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
          std::string(GDALVectorInfoAlgorithm::NAME) + VECTOR_SUFFIX,
          std::string(GDALRasterTileAlgorithm::NAME),
          std::string(GDALRasterWriteAlgorithm::NAME) + RASTER_SUFFIX,
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
