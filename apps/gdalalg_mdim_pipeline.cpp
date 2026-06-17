/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_mdim_pipeline.h"
#include "gdalalg_mdim_compare.h"
#include "gdalalg_mdim_read.h"
#include "gdalalg_mdim_write.h"
#include "gdalalg_mdim_info.h"
#include "gdalalg_mdim_mosaic.h"
#include "gdalalg_mdim_reproject.h"

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

GDALMdimAlgorithmStepRegistry::~GDALMdimAlgorithmStepRegistry() = default;

/************************************************************************/
/*    GDALMdimPipelineStepAlgorithm::GDALMdimPipelineStepAlgorithm()    */
/************************************************************************/

GDALMdimPipelineStepAlgorithm::GDALMdimPipelineStepAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, const ConstructorOptions &options)
    : GDALPipelineStepAlgorithm(name, description, helpURL, options)
{
    if (m_standaloneStep)
    {
        m_supportsStreamedOutput = true;

        if (m_constructorOptions.addDefaultArguments)
        {
            AddMdimInputArgs(false, false, /* acceptRaster = */ false);
            AddProgressArg();
            AddMdimOutputArgs(false);
        }
    }
    else if (m_constructorOptions.addDefaultArguments)
    {
        AddMdimHiddenInputDatasetArg();
    }
}

GDALMdimPipelineStepAlgorithm::~GDALMdimPipelineStepAlgorithm() = default;

/************************************************************************/
/*        GDALMdimPipelineAlgorithm::GDALMdimPipelineAlgorithm()        */
/************************************************************************/

GDALMdimPipelineAlgorithm::GDALMdimPipelineAlgorithm(
    bool openForMixedMdimVector)
    : GDALAbstractPipelineAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                    ConstructorOptions()
                                        .SetAddDefaultArguments(false)
                                        .SetInputDatasetRequired(false)
                                        .SetInputDatasetPositional(false)
                                        .SetInputDatasetMaxCount(INT_MAX))
{
    m_supportsStreamedOutput = true;

    AddMdimInputArgs(openForMixedMdimVector, /* hiddenForCLI = */ true,
                     /* acceptRaster = */ false);
    AddProgressArg();
    AddArg("pipeline", 0, _("Pipeline string"), &m_pipeline)
        .SetHiddenForCLI()
        .SetPositional();
    AddMdimOutputArgs(/* hiddenForCLI = */ true);

    AddOutputStringArg(&m_output).SetHiddenForCLI();
    AddStdoutArg(&m_stdout);

    RegisterAlgorithms(m_stepRegistry, false);
}

/************************************************************************/
/*           GDALMdimPipelineAlgorithm::RegisterAlgorithms()            */
/************************************************************************/

/* static */
void GDALMdimPipelineAlgorithm::RegisterAlgorithms(
    GDALMdimAlgorithmStepRegistry &registry, bool forMixedPipeline)
{
    GDALAlgorithmRegistry::AlgInfo algInfo;

    const auto addSuffixIfNeeded =
        [forMixedPipeline](const char *name) -> std::string
    {
        return forMixedPipeline ? std::string(name).append(MULTIDIM_SUFFIX)
                                : std::string(name);
    };

    registry.Register<GDALMdimReadAlgorithm>(
        addSuffixIfNeeded(GDALMdimReadAlgorithm::NAME));

    registry.Register<GDALMdimMosaicAlgorithm>(
        addSuffixIfNeeded(GDALMdimMosaicAlgorithm::NAME));

    registry.Register<GDALMdimCompareAlgorithm>(
        addSuffixIfNeeded(GDALMdimCompareAlgorithm::NAME));

    registry.Register<GDALMdimWriteAlgorithm>(
        addSuffixIfNeeded(GDALMdimWriteAlgorithm::NAME));

    registry.Register<GDALMdimInfoAlgorithm>(
        addSuffixIfNeeded(GDALMdimInfoAlgorithm::NAME));

    registry.Register<GDALMdimReprojectAlgorithm>(
        addSuffixIfNeeded(GDALMdimReprojectAlgorithm::NAME));
}

/************************************************************************/
/*             GDALMdimPipelineAlgorithm::GetUsageForCLI()              */
/************************************************************************/

std::string GDALMdimPipelineAlgorithm::GetUsageForCLI(
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

    ret += "\n<PIPELINE> is of the form: read|mosaic [READ-OPTIONS] "
           "( ! <STEP-NAME> [STEP-OPTIONS] )* ! info|write "
           "[WRITE-OPTIONS]\n";

    if (m_helpDocCategory == "main")
    {
        return ret;
    }

    ret += '\n';
    ret += "Example: 'gdal mdim pipeline --progress ! read in.nc ! \\\n";
    ret += "               reproject --output-crs=EPSG:32632 ! ";
    ret += "write out.nc --overwrite'\n";
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
        const auto name = GDALMdimReadAlgorithm::NAME;
        ret += '\n';
        auto alg = GetStepAlg(name);
        alg->SetCallPath({name});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }
    {
        const auto name = GDALMdimMosaicAlgorithm::NAME;
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
            !alg->IsHidden() && name != GDALMdimReadAlgorithm::NAME &&
            name != GDALMdimMosaicAlgorithm::NAME)
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
            !alg->IsHidden() && name != GDALMdimWriteAlgorithm::NAME)
        {
            ret += '\n';
            alg->SetCallPath({name});
            ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
    }
    {
        const auto name = GDALMdimWriteAlgorithm::NAME;
        ret += '\n';
        auto alg = GetStepAlg(name);
        alg->SetCallPath({name});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }
    ret += GetUsageForCLIEnd();

    return ret;
}

//! @endcond
