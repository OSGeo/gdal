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
#include "gdalalgorithm.h"
#include "gdalalg_raster_pipeline.h"
#include "gdalalg_vector_pipeline.h"
#include "gdalalg_dispatcher.h"
#include "gdal_priv.h"

#include "gdalalg_raster_read.h"
#include "gdalalg_raster_write.h"
#include "gdalalg_vector_read.h"
#include "gdalalg_vector_write.h"

//! @cond Doxygen_Suppress

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
            writeAlg = std::make_unique<GDALRasterWriteAlgorithm>();
        else
            writeAlg = std::make_unique<GDALVectorWriteAlgorithm>();
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
            const bool bOutputSpecified =
                GetArg(GDAL_ARG_NAME_OUTPUT)->IsExplicitlySet();

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
    : public GDALDispatcherAlgorithm<GDALRasterPipelineAlgorithm,
                                     GDALVectorPipelineAlgorithm>
{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION =
        "Execute a pipeline (shortcut for 'gdal raster pipeline' or 'gdal "
        "vector pipeline').";
    static constexpr const char *HELP_URL = "/programs/gdal_pipeline.html";

    GDALPipelineAlgorithm()
        : GDALDispatcherAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        // only for the help message
        AddOutputFormatArg(&m_format).SetDefault("json").SetChoices("json",
                                                                    "text");
        AddInputDatasetArg(&m_dataset);

        m_longDescription =
            "For all options, run 'gdal raster pipeline --help' or "
            "'gdal vector pipeline --help'";
    }

  private:
    std::unique_ptr<GDALRasterPipelineAlgorithm> m_rasterPipeline{};
    std::unique_ptr<GDALVectorPipelineAlgorithm> m_vectorPipeline{};

    std::string m_format{};
    GDALArgDatasetValue m_dataset{};

    bool RunImpl(GDALProgressFunc, void *) override;
};

bool GDALPipelineAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "The Run() method should not be called directly on the \"gdal "
             "pipeline\" program.");
    return false;
}

GDAL_STATIC_REGISTER_ALG(GDALPipelineAlgorithm);

//! @endcond
