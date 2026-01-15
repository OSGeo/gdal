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
#include "gdalalg_materialize.h"
#include "gdalalg_vector_read.h"
#include "gdalalg_vector_buffer.h"
#include "gdalalg_vector_check_coverage.h"
#include "gdalalg_vector_check_geometry.h"
#include "gdalalg_vector_clean_coverage.h"
#include "gdalalg_vector_clip.h"
#include "gdalalg_vector_concat.h"
#include "gdalalg_vector_edit.h"
#include "gdalalg_vector_explode_collections.h"
#include "gdalalg_vector_filter.h"
#include "gdalalg_vector_info.h"
#include "gdalalg_vector_limit.h"
#include "gdalalg_vector_make_point.h"
#include "gdalalg_vector_make_valid.h"
#include "gdalalg_vector_partition.h"
#include "gdalalg_vector_reproject.h"
#include "gdalalg_vector_segmentize.h"
#include "gdalalg_vector_select.h"
#include "gdalalg_vector_set_field_type.h"
#include "gdalalg_vector_set_geom_type.h"
#include "gdalalg_vector_simplify.h"
#include "gdalalg_vector_simplify_coverage.h"
#include "gdalalg_vector_sort.h"
#include "gdalalg_vector_sql.h"
#include "gdalalg_vector_swap_xy.h"
#include "gdalalg_vector_update.h"
#include "gdalalg_vector_write.h"
#include "gdalalg_tee.h"

#include "../frmts/mem/memdataset.h"

#include "cpl_conv.h"
#include "cpl_string.h"

#include <algorithm>
#include <cassert>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

GDALVectorAlgorithmStepRegistry::~GDALVectorAlgorithmStepRegistry() = default;

/************************************************************************/
/*  GDALVectorPipelineStepAlgorithm::GDALVectorPipelineStepAlgorithm()  */
/************************************************************************/

GDALVectorPipelineStepAlgorithm::GDALVectorPipelineStepAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(
          name, description, helpURL,
          ConstructorOptions().SetStandaloneStep(standaloneStep))
{
}

/************************************************************************/
/*  GDALVectorPipelineStepAlgorithm::GDALVectorPipelineStepAlgorithm()  */
/************************************************************************/

GDALVectorPipelineStepAlgorithm::GDALVectorPipelineStepAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, const ConstructorOptions &options)
    : GDALPipelineStepAlgorithm(name, description, helpURL, options)
{
    if (m_standaloneStep)
    {
        m_supportsStreamedOutput = true;

        if (m_constructorOptions.addDefaultArguments)
        {
            AddVectorInputArgs(false);
            AddProgressArg();
            AddVectorOutputArgs(false, false);
        }
    }
}

GDALVectorPipelineStepAlgorithm::~GDALVectorPipelineStepAlgorithm() = default;

/************************************************************************/
/*        GDALVectorPipelineAlgorithm::GDALVectorPipelineAlgorithm()    */
/************************************************************************/

GDALVectorPipelineAlgorithm::GDALVectorPipelineAlgorithm()
    : GDALAbstractPipelineAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions().SetInputDatasetMaxCount(INT_MAX))
{
    m_supportsStreamedOutput = true;

    AddVectorInputArgs(/* hiddenForCLI = */ true);
    AddProgressArg();
    AddArg("pipeline", 0, _("Pipeline string"), &m_pipeline)
        .SetHiddenForCLI()
        .SetPositional();
    AddVectorOutputArgs(/* hiddenForCLI = */ true,
                        /* shortNameOutputLayerAllowed=*/false);

    AddOutputStringArg(&m_output).SetHiddenForCLI();
    AddStdoutArg(&m_stdout);

    RegisterAlgorithms(m_stepRegistry, false);
}

/************************************************************************/
/*       GDALVectorPipelineAlgorithm::RegisterAlgorithms()              */
/************************************************************************/

/* static */
void GDALVectorPipelineAlgorithm::RegisterAlgorithms(
    GDALVectorAlgorithmStepRegistry &registry, bool forMixedPipeline)
{
    GDALAlgorithmRegistry::AlgInfo algInfo;

    const auto addSuffixIfNeeded =
        [forMixedPipeline](const char *name) -> std::string
    {
        return forMixedPipeline ? std::string(name).append(VECTOR_SUFFIX)
                                : std::string(name);
    };

    registry.Register<GDALVectorReadAlgorithm>(
        addSuffixIfNeeded(GDALVectorReadAlgorithm::NAME));

    registry.Register<GDALVectorWriteAlgorithm>(
        addSuffixIfNeeded(GDALVectorWriteAlgorithm::NAME));

    registry.Register<GDALVectorInfoAlgorithm>(
        addSuffixIfNeeded(GDALVectorInfoAlgorithm::NAME));

    registry.Register<GDALVectorBufferAlgorithm>();
    registry.Register<GDALVectorCheckCoverageAlgorithm>();
    registry.Register<GDALVectorCheckGeometryAlgorithm>();
    registry.Register<GDALVectorConcatAlgorithm>();
    registry.Register<GDALVectorCleanCoverageAlgorithm>();

    registry.Register<GDALVectorClipAlgorithm>(
        addSuffixIfNeeded(GDALVectorClipAlgorithm::NAME));

    registry.Register<GDALVectorEditAlgorithm>(
        addSuffixIfNeeded(GDALVectorEditAlgorithm::NAME));

    registry.Register<GDALVectorExplodeCollectionsAlgorithm>();

    registry.Register<GDALMaterializeVectorAlgorithm>(
        addSuffixIfNeeded(GDALMaterializeVectorAlgorithm::NAME));

    registry.Register<GDALVectorReprojectAlgorithm>(
        addSuffixIfNeeded(GDALVectorReprojectAlgorithm::NAME));

    registry.Register<GDALVectorFilterAlgorithm>();
    registry.Register<GDALVectorLimitAlgorithm>();
    registry.Register<GDALVectorMakePointAlgorithm>();
    registry.Register<GDALVectorMakeValidAlgorithm>();
    registry.Register<GDALVectorPartitionAlgorithm>();
    registry.Register<GDALVectorSegmentizeAlgorithm>();

    registry.Register<GDALVectorSelectAlgorithm>(
        addSuffixIfNeeded(GDALVectorSelectAlgorithm::NAME));

    registry.Register<GDALVectorSetFieldTypeAlgorithm>();
    registry.Register<GDALVectorSetGeomTypeAlgorithm>();
    registry.Register<GDALVectorSimplifyAlgorithm>();
    registry.Register<GDALVectorSimplifyCoverageAlgorithm>();
    registry.Register<GDALVectorSortAlgorithm>();
    registry.Register<GDALVectorSQLAlgorithm>();
    registry.Register<GDALVectorUpdateAlgorithm>(
        addSuffixIfNeeded(GDALVectorUpdateAlgorithm::NAME));
    registry.Register<GDALVectorSwapXYAlgorithm>();

    registry.Register<GDALTeeVectorAlgorithm>(
        addSuffixIfNeeded(GDALTeeVectorAlgorithm::NAME));
}

/************************************************************************/
/*            GDALVectorPipelineAlgorithm::GetUsageForCLI()             */
/************************************************************************/

std::string GDALVectorPipelineAlgorithm::GetUsageForCLI(
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

    ret += "\n<PIPELINE> is of the form: read|concat [READ-OPTIONS] "
           "( ! <STEP-NAME> [STEP-OPTIONS] )* ! write|info [WRITE-OPTIONS]\n";

    if (m_helpDocCategory == "main")
    {
        return ret;
    }

    ret += '\n';
    ret += "Example: 'gdal vector pipeline --progress ! read in.gpkg ! \\\n";
    ret += "               reproject --dst-crs=EPSG:32632 ! ";
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
        const auto name = GDALVectorReadAlgorithm::NAME;
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
            !alg->IsHidden() && name != GDALVectorReadAlgorithm::NAME)
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
            !alg->IsHidden() && name != GDALVectorWriteAlgorithm::NAME)
        {
            ret += '\n';
            alg->SetCallPath({name});
            ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
        }
    }
    {
        const auto name = GDALVectorWriteAlgorithm::NAME;
        ret += '\n';
        auto alg = GetStepAlg(name);
        alg->SetCallPath({name});
        ret += alg->GetUsageForCLI(shortUsage, stepUsageOptions);
    }

    ret += GetUsageForCLIEnd();

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
/*                 ~GDALVectorPipelineOutputLayer()                     */
/************************************************************************/

GDALVectorPipelineOutputLayer::~GDALVectorPipelineOutputLayer() = default;

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
        if (m_translateError)
        {
            return nullptr;
        }
        if (!m_pendingFeatures.empty())
            break;
    }
    OGRFeature *poFeature = m_pendingFeatures[0].release();
    m_idxInPendingFeatures = 1;
    return poFeature;
}

/************************************************************************/
/*                         GDALVectorOutputDataset                      */
/************************************************************************/

int GDALVectorOutputDataset::TestCapability(const char *) const
{
    return 0;
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
    SetMetadata(m_srcDS.GetMetadata());
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

int GDALVectorPipelineOutputDataset::GetLayerCount() const
{
    return static_cast<int>(m_layers.size());
}

/************************************************************************/
/*             GDALVectorPipelineOutputDataset::GetLayer()              */
/************************************************************************/

OGRLayer *GDALVectorPipelineOutputDataset::GetLayer(int idx) const
{
    return idx >= 0 && idx < GetLayerCount() ? m_layers[idx] : nullptr;
}

/************************************************************************/
/*           GDALVectorPipelineOutputDataset::TestCapability()          */
/************************************************************************/

int GDALVectorPipelineOutputDataset::TestCapability(const char *pszCap) const
{
    if (EQUAL(pszCap, ODsCRandomLayerRead) ||
        EQUAL(pszCap, ODsCMeasuredGeometries) || EQUAL(pszCap, ODsCZGeometries))
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

/************************************************************************/
/*            GDALVectorPipelinePassthroughLayer::GetLayerDefn()        */
/************************************************************************/

const OGRFeatureDefn *GDALVectorPipelinePassthroughLayer::GetLayerDefn() const
{
    return m_srcLayer.GetLayerDefn();
}

/************************************************************************/
/*                 GDALVectorNonStreamingAlgorithmDataset()             */
/************************************************************************/

GDALVectorNonStreamingAlgorithmDataset::GDALVectorNonStreamingAlgorithmDataset()
{
}

/************************************************************************/
/*                ~GDALVectorNonStreamingAlgorithmDataset()             */
/************************************************************************/

GDALVectorNonStreamingAlgorithmDataset::
    ~GDALVectorNonStreamingAlgorithmDataset() = default;

/************************************************************************/
/*    GDALVectorNonStreamingAlgorithmDataset::AddProcessedLayer()       */
/************************************************************************/

bool GDALVectorNonStreamingAlgorithmDataset::AddProcessedLayer(
    std::unique_ptr<GDALVectorNonStreamingAlgorithmLayer> layer,
    GDALProgressFunc progressFn, void *progressData)
{
    if (!layer->Process(progressFn, progressData))
    {
        return false;
    }

    m_owned_layers.emplace_back(std::move(layer));
    m_layers.push_back(m_owned_layers.back().get());

    return true;
}

/************************************************************************/
/*    GDALVectorNonStreamingAlgorithmDataset::AddPassThroughLayer()     */
/************************************************************************/

void GDALVectorNonStreamingAlgorithmDataset::AddPassThroughLayer(
    OGRLayer &oLayer)
{
    m_owned_layers.push_back(
        std::make_unique<GDALVectorPipelinePassthroughLayer>(oLayer));
    m_layers.push_back(m_owned_layers.back().get());
}

/************************************************************************/
/*       GDALVectorNonStreamingAlgorithmDataset::GetLayerCount()        */
/************************************************************************/

int GDALVectorNonStreamingAlgorithmDataset::GetLayerCount() const
{
    return static_cast<int>(m_layers.size());
}

/************************************************************************/
/*       GDALVectorNonStreamingAlgorithmDataset::GetLayer()             */
/************************************************************************/

OGRLayer *GDALVectorNonStreamingAlgorithmDataset::GetLayer(int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(m_layers.size()))
    {
        return nullptr;
    }
    return m_layers[idx];
}

/************************************************************************/
/*    GDALVectorNonStreamingAlgorithmDataset::TestCapability()          */
/************************************************************************/

int GDALVectorNonStreamingAlgorithmDataset::TestCapability(const char *) const
{
    return false;
}

/************************************************************************/
/*               GDALVectorAlgorithmLayerProgressHelper()               */
/************************************************************************/

GDALVectorAlgorithmLayerProgressHelper::GDALVectorAlgorithmLayerProgressHelper(
    GDALProgressFunc pfnProgress, void *pProgressData)
    : m_pfnProgress(pfnProgress), m_pProgressData(pProgressData)
{
}

/************************************************************************/
/*               GDALVectorAlgorithmLayerProgressHelper()               */
/************************************************************************/

GDALVectorAlgorithmLayerProgressHelper::GDALVectorAlgorithmLayerProgressHelper(
    const GDALPipelineStepRunContext &ctxt)
    : GDALVectorAlgorithmLayerProgressHelper(ctxt.m_pfnProgress,
                                             ctxt.m_pProgressData)
{
}

/************************************************************************/
/*     GDALVectorAlgorithmLayerProgressHelper::AddProcessedLayer()      */
/************************************************************************/

void GDALVectorAlgorithmLayerProgressHelper::AddProcessedLayer(
    OGRLayer &srcLayer)
{
    m_apoSrcLayers.emplace_back(&srcLayer, true);
    if (m_pfnProgress && m_nTotalFeatures >= 0 &&
        srcLayer.TestCapability(OLCFastFeatureCount))
    {
        const auto nLayerFeatures = srcLayer.GetFeatureCount(false);
        if (nLayerFeatures < 0)
            m_nTotalFeatures = -1;
        else
            m_nTotalFeatures += nLayerFeatures;
        m_anFeatures.push_back(nLayerFeatures);
    }
    else
    {
        m_anFeatures.push_back(-1);
        m_nTotalFeatures = -1;
    }
}

/************************************************************************/
/*     GDALVectorAlgorithmLayerProgressHelper::AddPassThroughLayer()    */
/************************************************************************/

void GDALVectorAlgorithmLayerProgressHelper::AddPassThroughLayer(
    OGRLayer &srcLayer)
{
    m_apoSrcLayers.emplace_back(&srcLayer, false);
}

/************************************************************************/
/*                  GDALVectorNonStreamingAlgorithmLayer()              */
/************************************************************************/

GDALVectorNonStreamingAlgorithmLayer::GDALVectorNonStreamingAlgorithmLayer(
    OGRLayer &srcLayer, int geomFieldIndex)
    : m_srcLayer(srcLayer), m_geomFieldIndex(geomFieldIndex)
{
}

/************************************************************************/
/*                 ~GDALVectorNonStreamingAlgorithmLayer()              */
/************************************************************************/

GDALVectorNonStreamingAlgorithmLayer::~GDALVectorNonStreamingAlgorithmLayer() =
    default;

/************************************************************************/
/*        GDALVectorNonStreamingAlgorithmLayer::GetNextRawFeature()     */
/************************************************************************/

OGRFeature *GDALVectorNonStreamingAlgorithmLayer::GetNextRawFeature()
{
    return GetNextProcessedFeature().release();
}

/************************************************************************/
/*     GDALVectorAlgorithmLayerProgressHelper::iterator::operator*()    */
/************************************************************************/

GDALVectorAlgorithmLayerProgressHelper::iterator::value_type
GDALVectorAlgorithmLayerProgressHelper::iterator::operator*() const
{
    const double dfProgressStart =
        m_helper.m_anFeatures.empty() ? 0
        : m_helper.m_nTotalFeatures > 0
            ? static_cast<double>(m_nFeatureIdx) /
                  static_cast<double>(m_helper.m_nTotalFeatures)
            : static_cast<double>(m_nProcessedLayerIdx) /
                  std::max(1.0,
                           static_cast<double>(m_helper.m_anFeatures.size()));
    const double dfProgressEnd =
        m_helper.m_anFeatures.empty() ? 0
        : m_helper.m_nTotalFeatures > 0
            ? static_cast<double>(m_nFeatureIdx +
                                  m_helper.m_anFeatures[m_nProcessedLayerIdx]) /
                  static_cast<double>(m_helper.m_nTotalFeatures)
            : static_cast<double>(m_nProcessedLayerIdx + 1) /
                  std::max(1.0,
                           static_cast<double>(m_helper.m_anFeatures.size()));

    progress_data_unique_ptr pScaledProgressData(nullptr,
                                                 GDALDestroyScaledProgress);
    if (m_helper.m_pfnProgress && m_helper.m_apoSrcLayers[m_nLayerIdx].second)
    {
        pScaledProgressData.reset(GDALCreateScaledProgress(
            dfProgressStart, dfProgressEnd, m_helper.m_pfnProgress,
            m_helper.m_pProgressData));
    }

    return value_type(m_helper.m_apoSrcLayers[m_nLayerIdx].first,
                      m_helper.m_apoSrcLayers[m_nLayerIdx].second,
                      m_helper.m_pfnProgress ? GDALScaledProgress : nullptr,
                      std::move(pScaledProgressData));
}

//! @endcond
