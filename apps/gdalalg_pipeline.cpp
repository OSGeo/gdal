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
#include "gdalalg_abstract_pipeline.h"
#include "gdal_priv.h"

#include "gdalalg_raster_read.h"
#include "gdalalg_raster_mosaic.h"
#include "gdalalg_raster_stack.h"
#include "gdalalg_raster_write.h"
#include "gdalalg_raster_zonal_stats.h"

#include "gdalalg_vector_read.h"
#include "gdalalg_vector_write.h"

#include "gdalalg_raster_as_features.h"
#include "gdalalg_raster_compare.h"
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
        .SetMinCount(1)
        .SetMaxCount(m_constructorOptions.inputDatasetMaxCount)
        .SetAutoOpenDataset(m_constructorOptions.autoOpenInputDatasets)
        .SetMetaVar(m_constructorOptions.inputDatasetMetaVar)
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
        AddInputDatasetArg(
            &m_inputDataset,
            openForMixedRasterVector ? (GDAL_OF_RASTER | GDAL_OF_VECTOR)
                                     : GDAL_OF_RASTER,
            m_constructorOptions.inputDatasetRequired && !hiddenForCLI,
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
    constexpr const char *MUTUAL_EXCLUSION_GROUP_OVERWRITE_APPEND =
        "overwrite-append";
    AddOverwriteArg(&m_overwrite)
        .SetHiddenForCLI(hiddenForCLI)
        .SetMutualExclusionGroup(MUTUAL_EXCLUSION_GROUP_OVERWRITE_APPEND);
    AddArg(GDAL_ARG_NAME_APPEND, 0,
           _("Append as a subdataset to existing output"), &m_appendRaster)
        .SetDefault(false)
        .SetHiddenForCLI(hiddenForCLI)
        .SetMutualExclusionGroup(MUTUAL_EXCLUSION_GROUP_OVERWRITE_APPEND);
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
    AddOutputOpenOptionsArg(&m_outputOpenOptions).SetHiddenForCLI(hiddenForCLI);
    auto &outputDatasetArg =
        AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR,
                            /* positionalAndRequired = */ false)
            .SetHiddenForCLI(hiddenForCLI)
            .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    if (!hiddenForCLI)
        outputDatasetArg.SetPositional();
    if (!hiddenForCLI && m_constructorOptions.outputDatasetRequired)
        outputDatasetArg.SetRequired();

    AddCreationOptionsArg(&m_creationOptions).SetHiddenForCLI(hiddenForCLI);
    AddLayerCreationOptionsArg(&m_layerCreationOptions)
        .SetHiddenForCLI(hiddenForCLI);
    AddOverwriteArg(&m_overwrite).SetHiddenForCLI(hiddenForCLI);
    GDALInConstructionAlgorithmArg *updateArg = nullptr;
    if (m_constructorOptions.addUpdateArgument)
    {
        updateArg = &AddUpdateArg(&m_update).SetHiddenForCLI(hiddenForCLI);
    }
    if (m_constructorOptions.addOverwriteLayerArgument)
    {
        AddOverwriteLayerArg(&m_overwriteLayer).SetHiddenForCLI(hiddenForCLI);
    }
    constexpr const char *MUTUAL_EXCLUSION_GROUP_APPEND_UPSERT =
        "append-upsert";
    if (m_constructorOptions.addAppendLayerArgument)
    {
        AddAppendLayerArg(&m_appendLayer)
            .SetHiddenForCLI(hiddenForCLI)
            .SetMutualExclusionGroup(MUTUAL_EXCLUSION_GROUP_APPEND_UPSERT);
    }
    if (m_constructorOptions.addUpsertArgument)
    {
        AddArg("upsert", 0, _("Upsert features (implies 'append')"), &m_upsert)
            .SetHiddenForCLI(hiddenForCLI)
            .SetMutualExclusionGroup(MUTUAL_EXCLUSION_GROUP_APPEND_UPSERT)
            .AddAction(
                [updateArg, this]()
                {
                    if (m_upsert && updateArg)
                        updateArg->Set(true);
                })
            .SetCategory(GAAC_ADVANCED);
    }
    if (m_constructorOptions.addOutputLayerNameArgument)
    {
        AddArg(GDAL_ARG_NAME_OUTPUT_LAYER,
               shortNameOutputLayerAllowed ? 'l' : 0, _("Output layer name"),
               &m_outputLayerName)
            .AddHiddenAlias("nln")  // For ogr2ogr nostalgic people
            .SetHiddenForCLI(hiddenForCLI);
    }
    if (m_constructorOptions.addSkipErrorsArgument)
    {
        AddArg("skip-errors", 0, _("Skip errors when writing features"),
               &m_skipErrors)
            .AddHiddenAlias("skip-failures");  // For ogr2ogr nostalgic people
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
            else if (GetName() == GDALRasterCompareAlgorithm::NAME)
                writeAlg = std::make_unique<GDALRasterCompareAlgorithm>();
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

            GDALPipelineStepRunContext stepCtxt;
            if (pfnProgress && GetName() == GDALRasterCompareAlgorithm::NAME)
            {
                stepCtxt.m_pfnProgress = pfnProgress;
                stepCtxt.m_pProgressData = pProgressData;
            }
            else if (pfnProgress &&
                     (bCanHandleNextStep || !IsNativelyStreamingCompatible()))
            {
                pScaledData.reset(GDALCreateScaledProgress(
                    0.0, bIsStreaming || bCanHandleNextStep ? 1.0 : 0.5,
                    pfnProgress, pProgressData));
                stepCtxt.m_pfnProgress =
                    pScaledData ? GDALScaledProgress : nullptr;
                stepCtxt.m_pProgressData = pScaledData.get();
            }

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
/*                          SetInputDataset()                           */
/************************************************************************/

void GDALPipelineStepAlgorithm::SetInputDataset(GDALDataset *poDS)
{
    auto arg = GetArg(GDAL_ARG_NAME_INPUT);
    if (arg)
    {
        auto &val = arg->Get<std::vector<GDALArgDatasetValue>>();
        val.resize(1);
        val[0].Set(poDS);
        arg->NotifyValueSet();
        arg->SetSkipIfAlreadySet();
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
/*                 GDALPipelineStepAlgorithm::Finalize()                */
/************************************************************************/

bool GDALPipelineStepAlgorithm::Finalize()
{
    bool ret = GDALAlgorithm::Finalize();
    for (auto &argValue : m_inputDataset)
        ret = argValue.Close() && ret;
    ret = m_outputDataset.Close() && ret;
    return ret;
}

/************************************************************************/
/*                      GDALAlgorithmStepRegistry                       */
/************************************************************************/

class GDALAlgorithmStepRegistry final : public GDALRasterAlgorithmStepRegistry,
                                        public GDALVectorAlgorithmStepRegistry
{
  public:
    GDALAlgorithmStepRegistry() = default;
    ~GDALAlgorithmStepRegistry() override;

    /** Register the algorithm of type MyAlgorithm.
     */
    template <class MyAlgorithm>
    bool Register(const std::string &name = std::string())
    {
        static_assert(std::is_base_of_v<GDALPipelineStepAlgorithm, MyAlgorithm>,
                      "Algorithm is not a GDALPipelineStepAlgorithm");

        AlgInfo info;
        info.m_name = name.empty() ? MyAlgorithm::NAME : name;
        info.m_aliases = MyAlgorithm::GetAliasesStatic();
        info.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
        { return std::make_unique<MyAlgorithm>(); };
        return GDALAlgorithmRegistry::Register(info);
    }
};

GDALAlgorithmStepRegistry::~GDALAlgorithmStepRegistry() = default;

/************************************************************************/
/*                       GDALPipelineAlgorithm                          */
/************************************************************************/

class GDALPipelineAlgorithm final : public GDALAbstractPipelineAlgorithm

{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION =
        "Process a dataset applying several steps.";
    static constexpr const char *HELP_URL = "/programs/gdal_pipeline.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {
#ifdef GDAL_PIPELINE_PROJ_NOSTALGIA
            GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR,
            "+pipeline",
            "+gdal=pipeline",
#endif
        };
    }

    GDALPipelineAlgorithm();

    int GetInputType() const override
    {
        return GDAL_OF_RASTER | GDAL_OF_VECTOR;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_RASTER | GDAL_OF_VECTOR;
    }

  protected:
    GDALAlgorithmStepRegistry m_stepRegistry{};

    GDALAlgorithmRegistry &GetStepRegistry() override
    {
        return m_stepRegistry;
    }

    const GDALAlgorithmRegistry &GetStepRegistry() const override
    {
        return m_stepRegistry;
    }

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

  private:
    std::unique_ptr<GDALAbstractPipelineAlgorithm>
    CreateNestedPipeline() const override
    {
        auto pipeline = std::make_unique<GDALPipelineAlgorithm>();
        pipeline->m_bInnerPipeline = true;
        return pipeline;
    }
};

/************************************************************************/
/*                       GDALPipelineAlgorithm                          */
/************************************************************************/

GDALPipelineAlgorithm::GDALPipelineAlgorithm()
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
    m_stepRegistry.Register<GDALRasterAsFeaturesAlgorithm>();
    m_stepRegistry.Register<GDALRasterContourAlgorithm>();
    m_stepRegistry.Register<GDALRasterFootprintAlgorithm>();
    m_stepRegistry.Register<GDALRasterPolygonizeAlgorithm>();
    m_stepRegistry.Register<GDALRasterZonalStatsAlgorithm>();
    m_stepRegistry.Register<GDALVectorGridAlgorithm>();
    m_stepRegistry.Register<GDALVectorRasterizeAlgorithm>();
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
        "\n<PIPELINE> is of the form: read|calc|concat|create|mosaic|stack "
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
        if (alg->CanBeFirstStep() && !alg->CanBeMiddleStep() &&
            !alg->IsHidden() &&
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
        if (alg->CanBeMiddleStep() && !alg->IsHidden())
        {
            ret += '\n';
            alg->SetCallPath({CPLString(alg->GetName())
                                  .replaceAll(RASTER_SUFFIX, "")
                                  .replaceAll(VECTOR_SUFFIX, "")});
            ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
    }
    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        assert(alg);
        if (alg->CanBeLastStep() && !alg->CanBeMiddleStep() &&
            !alg->IsHidden() &&
            !STARTS_WITH(name.c_str(), GDALRasterWriteAlgorithm::NAME))
        {
            ret += '\n';
            alg->SetCallPath({name});
            ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
    }
    {
        ret += '\n';
        auto alg = std::make_unique<GDALRasterWriteAlgorithm>();
        alg->SetCallPath({alg->GetName()});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }
    {
        ret += '\n';
        auto alg = std::make_unique<GDALVectorWriteAlgorithm>();
        alg->SetCallPath({alg->GetName()});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }

    ret += GetUsageForCLIEnd();

    return ret;
}

GDAL_STATIC_REGISTER_ALG(GDALPipelineAlgorithm);

//! @endcond
