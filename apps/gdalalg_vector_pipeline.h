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

#ifndef GDALALG_VECTOR_PIPELINE_INCLUDED
#define GDALALG_VECTOR_PIPELINE_INCLUDED

#include "gdalalgorithm.h"
#include "gdalalg_abstract_pipeline.h"

#include "ogrsf_frmts.h"
#include "ogrlayerwithtranslatefeature.h"

#include <map>
#include <vector>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                GDALVectorPipelineStepAlgorithm                       */
/************************************************************************/

class GDALRasterAlgorithmStepRegistry;

class GDALVectorPipelineStepAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    ~GDALVectorPipelineStepAlgorithm() override;

  protected:
    GDALVectorPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    bool standaloneStep);

    GDALVectorPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    const ConstructorOptions &options);

    friend class GDALVectorPipelineAlgorithm;
    friend class GDALVectorConcatAlgorithm;

    int GetInputType() const override
    {
        return GDAL_OF_VECTOR;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_VECTOR;
    }
};

/************************************************************************/
/*                      GDALVectorAlgorithmStepRegistry                 */
/************************************************************************/

class GDALVectorAlgorithmStepRegistry : public virtual GDALAlgorithmRegistry
{
  public:
    GDALVectorAlgorithmStepRegistry() = default;
    ~GDALVectorAlgorithmStepRegistry() override;

    /** Register the algorithm of type MyAlgorithm.
     */
    template <class MyAlgorithm>
    bool Register(const std::string &name = std::string())
    {
        static_assert(
            std::is_base_of_v<GDALVectorPipelineStepAlgorithm, MyAlgorithm>,
            "Algorithm is not a GDALVectorPipelineStepAlgorithm");

        AlgInfo info;
        info.m_name = name.empty() ? MyAlgorithm::NAME : name;
        info.m_aliases = MyAlgorithm::GetAliasesStatic();
        info.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
        { return std::make_unique<MyAlgorithm>(); };
        return GDALAlgorithmRegistry::Register(info);
    }
};

/************************************************************************/
/*                     GDALVectorPipelineAlgorithm                      */
/************************************************************************/

class GDALVectorPipelineAlgorithm final : public GDALAbstractPipelineAlgorithm
{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION =
        "Process a vector dataset applying several steps.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_pipeline.html";

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

    GDALVectorPipelineAlgorithm();

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

    static void RegisterAlgorithms(GDALVectorAlgorithmStepRegistry &registry,
                                   bool forMixedPipeline);

    int GetInputType() const override
    {
        return GDAL_OF_VECTOR;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_VECTOR;
    }

  protected:
    GDALVectorAlgorithmStepRegistry m_stepRegistry{};

    GDALAlgorithmRegistry &GetStepRegistry() override
    {
        return m_stepRegistry;
    }

    const GDALAlgorithmRegistry &GetStepRegistry() const override
    {
        return m_stepRegistry;
    }

  private:
    std::unique_ptr<GDALAbstractPipelineAlgorithm>
    CreateNestedPipeline() const override
    {
        auto pipeline = std::make_unique<GDALVectorPipelineAlgorithm>();
        pipeline->m_bInnerPipeline = true;
        return pipeline;
    }
};

/************************************************************************/
/*                  GDALVectorOutputDataset                             */
/************************************************************************/

class GDALVectorOutputDataset final : public GDALDataset
{

  public:
    int GetLayerCount() const override
    {
        return static_cast<int>(m_layers.size());
    }

    const OGRLayer *GetLayer(int idx) const override
    {
        return m_layers[idx].get();
    }

    int TestCapability(const char *) const override;

    void AddLayer(std::unique_ptr<OGRLayer> layer)
    {
        m_layers.emplace_back(std::move(layer));
    }

  private:
    std::vector<std::unique_ptr<OGRLayer>> m_layers{};
};

/************************************************************************/
/*                  GDALVectorPipelineOutputLayer                       */
/************************************************************************/

/** Class that implements GetNextFeature() by forwarding to
 * OGRLayerWithTranslateFeature::TranslateFeature() implementation, which
 * might return several features.
 */
class GDALVectorPipelineOutputLayer /* non final */
    : public OGRLayerWithTranslateFeature,
      public OGRGetNextFeatureThroughRaw<GDALVectorPipelineOutputLayer>
{
  protected:
    explicit GDALVectorPipelineOutputLayer(OGRLayer &oSrcLayer);
    ~GDALVectorPipelineOutputLayer() override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(GDALVectorPipelineOutputLayer)

    OGRLayer &m_srcLayer;

    void FailTranslation()
    {
        m_translateError = true;
    }

  public:
    void ResetReading() override;
    OGRFeature *GetNextRawFeature();

  private:
    std::vector<std::unique_ptr<OGRFeature>> m_pendingFeatures{};
    size_t m_idxInPendingFeatures = 0;
    bool m_translateError = false;
};

/************************************************************************/
/*                  GDALVectorPipelinePassthroughLayer                  */
/************************************************************************/

/** Class that forwards GetNextFeature() calls to the source layer and
 * can be added to GDALVectorPipelineOutputDataset::AddLayer()
 */
class GDALVectorPipelinePassthroughLayer /* non final */
    : public GDALVectorPipelineOutputLayer
{
  public:
    explicit GDALVectorPipelinePassthroughLayer(OGRLayer &oSrcLayer)
        : GDALVectorPipelineOutputLayer(oSrcLayer)
    {
    }

    const OGRFeatureDefn *GetLayerDefn() const override;

    int TestCapability(const char *pszCap) const override
    {
        return m_srcLayer.TestCapability(pszCap);
    }

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        apoOutFeatures.push_back(std::move(poSrcFeature));
    }
};

/************************************************************************/
/*                 GDALVectorNonStreamingAlgorithmDataset               */
/************************************************************************/

class MEMDataset;

/**
 * Dataset used to read all input features into memory and perform some
 * processing.
 */
class GDALVectorNonStreamingAlgorithmDataset /* non final */
    : public GDALDataset
{
  public:
    GDALVectorNonStreamingAlgorithmDataset();
    ~GDALVectorNonStreamingAlgorithmDataset() override;

    virtual bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer) = 0;

    bool AddProcessedLayer(OGRLayer &srcLayer);
    bool AddProcessedLayer(OGRLayer &srcLayer, OGRFeatureDefn &dstDefn);
    void AddPassThroughLayer(OGRLayer &oLayer);
    int GetLayerCount() const final override;
    OGRLayer *GetLayer(int idx) const final override;
    int TestCapability(const char *pszCap) const override;

  private:
    std::vector<std::unique_ptr<OGRLayer>> m_passthrough_layers{};
    std::vector<OGRLayer *> m_layers{};
    std::unique_ptr<MEMDataset> m_ds{};
};

/************************************************************************/
/*                 GDALVectorPipelineOutputDataset                      */
/************************************************************************/

/** Class used by vector pipeline steps to create an output on-the-fly
 * dataset where they can store on-the-fly layers.
 */
class GDALVectorPipelineOutputDataset final : public GDALDataset
{
    GDALDataset &m_srcDS;
    std::map<OGRLayer *, OGRLayerWithTranslateFeature *>
        m_mapSrcLayerToNewLayer{};
    std::vector<std::unique_ptr<OGRLayerWithTranslateFeature>>
        m_layersToDestroy{};
    std::vector<OGRLayerWithTranslateFeature *> m_layers{};

    OGRLayerWithTranslateFeature *m_belongingLayer = nullptr;
    std::vector<std::unique_ptr<OGRFeature>> m_pendingFeatures{};
    size_t m_idxInPendingFeatures = 0;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorPipelineOutputDataset)

  public:
    explicit GDALVectorPipelineOutputDataset(GDALDataset &oSrcDS);

    void AddLayer(OGRLayer &oSrcLayer,
                  std::unique_ptr<OGRLayerWithTranslateFeature> poNewLayer);

    int GetLayerCount() const override;

    OGRLayer *GetLayer(int idx) const override;

    int TestCapability(const char *pszCap) const override;

    void ResetReading() override;

    OGRFeature *GetNextFeature(OGRLayer **ppoBelongingLayer,
                               double *pdfProgressPct,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData) override;
};

//! @endcond

#endif
