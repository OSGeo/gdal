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
/*                  GDALVectorPipelineStepRunContext                    */
/************************************************************************/

class GDALVectorPipelineStepAlgorithm;

class GDALVectorPipelineStepRunContext
{
  public:
    GDALVectorPipelineStepRunContext() = default;

    // Progress callback to use during execution of the step
    GDALProgressFunc m_pfnProgress = nullptr;
    void *m_pProgressData = nullptr;

    // If there is a step in the pipeline immediately following step to which
    // this instance of GDALRasterPipelineStepRunContext is passed, and that
    // this next step is usable by the current step (as determined by
    // CanHandleNextStep()), then this member will point to this next step.
    GDALVectorPipelineStepAlgorithm *m_poNextUsableStep = nullptr;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALVectorPipelineStepRunContext)
};

/************************************************************************/
/*                GDALVectorPipelineStepAlgorithm                       */
/************************************************************************/

class GDALVectorPipelineStepAlgorithm /* non final */ : public GDALAlgorithm
{
  protected:
    GDALVectorPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    bool standaloneStep);

    struct ConstructorOptions
    {
        bool standaloneStep = false;
        bool outputDatasetRequired = true;
        std::string updateMutualExclusionGroup{};
        std::string outputDatasetMutualExclusionGroup{};

        inline ConstructorOptions &SetStandaloneStep(bool b)
        {
            standaloneStep = b;
            return *this;
        }

        inline ConstructorOptions &SetOutputDatasetRequired(bool b)
        {
            outputDatasetRequired = b;
            return *this;
        }

        inline ConstructorOptions &
        SetUpdateMutualExclusionGroup(const std::string &s)
        {
            updateMutualExclusionGroup = s;
            return *this;
        }

        inline ConstructorOptions &
        SetOutputDatasetMutualExclusionGroup(const std::string &s)
        {
            outputDatasetMutualExclusionGroup = s;
            return *this;
        }
    };

    GDALVectorPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    const ConstructorOptions &options);

    using StepRunContext = GDALVectorPipelineStepRunContext;

    friend class GDALVectorPipelineAlgorithm;
    friend class GDALAbstractPipelineAlgorithm<GDALVectorPipelineStepAlgorithm>;
    friend class GDALVectorConcatAlgorithm;

    virtual bool IsNativelyStreamingCompatible() const
    {
        return true;
    }

    virtual bool CanHandleNextStep(GDALVectorPipelineStepAlgorithm *) const
    {
        return false;
    }

    virtual bool RunStep(GDALVectorPipelineStepRunContext &ctxt) = 0;

    void AddInputArgs(bool hiddenForCLI);
    void AddOutputArgs(bool hiddenForCLI, bool shortNameOutputLayerAllowed);

    bool m_standaloneStep = false;
    const ConstructorOptions m_constructorOptions;

    bool m_outputVRTCompatible = false;

    // Input arguments
    std::vector<GDALArgDatasetValue> m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    std::vector<std::string> m_inputLayerNames{};

    // Output arguments
    GDALArgDatasetValue m_outputDataset{};
    std::string m_format{};
    std::vector<std::string> m_creationOptions{};
    std::vector<std::string> m_layerCreationOptions{};
    bool m_overwrite = false;
    bool m_update = false;
    bool m_overwriteLayer = false;
    bool m_appendLayer = false;
    std::string m_outputLayerName{};

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
    GDALAlgorithm::ProcessGDALGOutputRet ProcessGDALGOutput() override;
    bool CheckSafeForStreamOutput() override;
};

/************************************************************************/
/*                     GDALVectorPipelineAlgorithm                      */
/************************************************************************/

// This is an easter egg to pay tribute to PROJ pipeline syntax
// We accept "gdal vector +gdal=pipeline +step +gdal=read +input=poly.gpkg +step +gdal=reproject +dst-crs=EPSG:32632 +step +gdal=write +output=out.gpkg +overwrite"
// as an alternative to (recommended):
// "gdal vector pipeline ! read poly.gpkg ! reproject--dst-crs=EPSG:32632 ! write out.gpkg --overwrite"
#define GDAL_PIPELINE_PROJ_NOSTALGIA

class GDALVectorPipelineAlgorithm final
    : public GDALAbstractPipelineAlgorithm<GDALVectorPipelineStepAlgorithm>
{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION = "Process a vector dataset.";
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

    bool
    ParseCommandLineArguments(const std::vector<std::string> &args) override;

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

  protected:
    GDALArgDatasetValue &GetOutputDataset() override
    {
        return m_outputDataset;
    }

  private:
    std::string m_helpDocCategory{};
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
    ~GDALVectorPipelineOutputLayer();

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(GDALVectorPipelineOutputLayer)

    OGRLayer &m_srcLayer;

  public:
    void ResetReading() override;
    OGRFeature *GetNextRawFeature();

  private:
    std::vector<std::unique_ptr<OGRFeature>> m_pendingFeatures{};
    size_t m_idxInPendingFeatures = 0;
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

    OGRFeatureDefn *GetLayerDefn() override;

    int TestCapability(const char *pszCap) override
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
    ~GDALVectorNonStreamingAlgorithmDataset();

    virtual bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer) = 0;

    bool AddProcessedLayer(OGRLayer &srcLayer);
    void AddPassThroughLayer(OGRLayer &oLayer);
    int GetLayerCount() final override;
    OGRLayer *GetLayer(int idx) final override;
    int TestCapability(const char *pszCap) override;

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
    ~GDALVectorPipelineOutputDataset();

    void AddLayer(OGRLayer &oSrcLayer,
                  std::unique_ptr<OGRLayerWithTranslateFeature> poNewLayer);

    int GetLayerCount() override;

    OGRLayer *GetLayer(int idx) override;

    int TestCapability(const char *pszCap) override;

    void ResetReading() override;

    OGRFeature *GetNextFeature(OGRLayer **ppoBelongingLayer,
                               double *pdfProgressPct,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData) override;
};

//! @endcond

#endif
