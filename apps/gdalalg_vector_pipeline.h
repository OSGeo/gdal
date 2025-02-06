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

//! @cond Doxygen_Suppress

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

    friend class GDALVectorPipelineAlgorithm;
    friend class GDALAbstractPipelineAlgorithm<GDALVectorPipelineStepAlgorithm>;

    virtual bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) = 0;

    void AddInputArgs(bool hiddenForCLI);
    void AddOutputArgs(bool hiddenForCLI, bool shortNameOutputLayerAllowed);

    bool m_standaloneStep = false;

    // Input arguments
    GDALArgDatasetValue m_inputDataset{};
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

    static std::vector<std::string> GetAliases()
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
};

/************************************************************************/
/*                 GDALVectorPipelineOutputDataset                      */
/************************************************************************/

/** Class used by vector pipeline steps to create an output on-the-fly
 * dataset where they can store on-the-fly layers.
 */
class GDALVectorPipelineOutputDataset final : public GDALDataset
{
    std::vector<std::unique_ptr<OGRLayer>> m_layersToDestroy{};
    std::vector<OGRLayer *> m_layers{};

  public:
    GDALVectorPipelineOutputDataset() = default;

    void AddLayer(std::unique_ptr<OGRLayer> poLayer)
    {
        m_layersToDestroy.push_back(std::move(poLayer));
        m_layers.push_back(m_layersToDestroy.back().get());
    }

    void AddLayer(OGRLayer *poLayer)
    {
        m_layers.push_back(poLayer);
    }

    int GetLayerCount() override
    {
        return static_cast<int>(m_layers.size());
    }

    OGRLayer *GetLayer(int idx) override
    {
        return idx >= 0 && idx < GetLayerCount() ? m_layers[idx] : nullptr;
    }
};

//! @endcond

#endif
