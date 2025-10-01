/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "read" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_read.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALVectorReadAlgorithm::GDALVectorReadAlgorithm()          */
/************************************************************************/

GDALVectorReadAlgorithm::GDALVectorReadAlgorithm()
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep =*/false)
{
    AddVectorInputArgs(/* hiddenForCLI = */ false);
    AddArg("limit", 0, _("Limit the number of features to read per layer"),
           &m_featureLimit);
}

/************************************************************************/
/*                      GDALVectorReadLimitedLayer                      */
/************************************************************************/

class GDALVectorReadLimitedLayer : public OGRLayer
{
  public:
    GDALVectorReadLimitedLayer(OGRLayer &layer, int featureLimit)
        : m_layer(layer), m_featureLimit(featureLimit), m_featuresRead(0)
    {
    }

    ~GDALVectorReadLimitedLayer() override;

    OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_layer.GetLayerDefn();
    }

    OGRFeature *GetNextFeature() override
    {
        if (m_featuresRead < m_featureLimit)
        {
            m_featuresRead++;
            return m_layer.GetNextFeature();
        }
        return nullptr;
    }

    void ResetReading() override
    {
        m_layer.ResetReading();
        m_featuresRead = 0;
    }

    int TestCapability(const char *const pszCap) const override
    {
        return m_layer.TestCapability(pszCap);
    }

  private:
    OGRLayer &m_layer;
    int m_featureLimit;
    int m_featuresRead;
};

GDALVectorReadLimitedLayer::~GDALVectorReadLimitedLayer() = default;

/************************************************************************/
/*                 GDALVectorPipelineReadOutputDataset                  */
/************************************************************************/

/** Class used by vector pipeline steps to create an output on-the-fly
 * dataset where they can store on-the-fly layers.
 */
class GDALVectorPipelineReadOutputDataset final : public GDALDataset
{
    GDALDataset &m_srcDS;
    std::vector<OGRLayer *> m_layers{};
    std::vector<std::unique_ptr<OGRLayer>> m_ownedLayers{};
    int m_featureLimit{};

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorPipelineReadOutputDataset)

  public:
    GDALVectorPipelineReadOutputDataset(GDALDataset &oSrcDS, int featureLimit);

    void AddLayer(OGRLayer &oSrcLayer);

    int GetLayerCount() const override;

    OGRLayer *GetLayer(int idx) const override;

    int TestCapability(const char *pszCap) const override;

    void ResetReading() override;

    OGRFeature *GetNextFeature(OGRLayer **ppoBelongingLayer,
                               double *pdfProgressPct,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData) override;
};

/************************************************************************/
/*                 GDALVectorPipelineReadOutputDataset()                */
/************************************************************************/

GDALVectorPipelineReadOutputDataset::GDALVectorPipelineReadOutputDataset(
    GDALDataset &srcDS, int featureLimit)
    : m_srcDS(srcDS), m_featureLimit(featureLimit)
{
    SetDescription(m_srcDS.GetDescription());
}

/************************************************************************/
/*            GDALVectorPipelineReadOutputDataset::AddLayer()           */
/************************************************************************/

void GDALVectorPipelineReadOutputDataset::AddLayer(OGRLayer &oSrcLayer)
{
    OGRLayer *poSrcLayer = &oSrcLayer;

    CPLDebug("GDAL", "feature limit is %d", m_featureLimit);

    if (m_featureLimit)
    {
        m_ownedLayers.push_back(std::make_unique<GDALVectorReadLimitedLayer>(
            oSrcLayer, m_featureLimit));
        poSrcLayer = m_ownedLayers.back().get();
    }

    m_layers.push_back(poSrcLayer);
}

/************************************************************************/
/*          GDALVectorPipelineReadOutputDataset::GetLayerCount()        */
/************************************************************************/

int GDALVectorPipelineReadOutputDataset::GetLayerCount() const
{
    return static_cast<int>(m_layers.size());
}

/************************************************************************/
/*           GDALVectorPipelineReadOutputDataset::GetLayer()            */
/************************************************************************/

OGRLayer *GDALVectorPipelineReadOutputDataset::GetLayer(int idx) const
{
    return idx >= 0 && idx < GetLayerCount() ? m_layers[idx] : nullptr;
}

/************************************************************************/
/*         GDALVectorPipelineReadOutputDataset::TestCapability()        */
/************************************************************************/

int GDALVectorPipelineReadOutputDataset::TestCapability(
    const char *pszCap) const
{
    if (EQUAL(pszCap, ODsCRandomLayerRead))
        return m_srcDS.TestCapability(pszCap);
    return false;
}

/************************************************************************/
/*           GDALVectorPipelineReadOutputDataset::ResetReading()        */
/************************************************************************/

void GDALVectorPipelineReadOutputDataset::ResetReading()
{
    m_srcDS.ResetReading();
}

/************************************************************************/
/*          GDALVectorPipelineReadOutputDataset::GetNextFeature()       */
/************************************************************************/

OGRFeature *GDALVectorPipelineReadOutputDataset::GetNextFeature(
    OGRLayer **ppoBelongingLayer, double *pdfProgressPct,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    while (true)
    {
        OGRLayer *poBelongingLayer = nullptr;
        auto poFeature = std::unique_ptr<OGRFeature>(m_srcDS.GetNextFeature(
            &poBelongingLayer, pdfProgressPct, pfnProgress, pProgressData));
        if (ppoBelongingLayer)
            *ppoBelongingLayer = poBelongingLayer;
        if (!poFeature)
            break;
        if (std::find(m_layers.begin(), m_layers.end(), poBelongingLayer) !=
            m_layers.end())
            return poFeature.release();
    }
    return nullptr;
}

/************************************************************************/
/*                  GDALVectorReadAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALVectorReadAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_inputLayerNames.empty() && !m_featureLimit)
    {
        m_outputDataset.Set(poSrcDS);
    }
    else
    {
        auto poOutDS = std::make_unique<GDALVectorPipelineReadOutputDataset>(
            *poSrcDS, m_featureLimit);
        if (m_inputLayerNames.empty())
        {
            for (int i = 0; i < poSrcDS->GetLayerCount(); i++)
            {
                poOutDS->AddLayer(*poSrcDS->GetLayer(i));
            }
        }
        else
        {
            for (const auto &srcLayerName : m_inputLayerNames)
            {
                auto poSrcLayer = poSrcDS->GetLayerByName(srcLayerName.c_str());
                if (!poSrcLayer)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot find source layer '%s'",
                                srcLayerName.c_str());
                    return false;
                }
                poOutDS->AddLayer(*poSrcLayer);
            }
        }
        m_outputDataset.Set(std::move(poOutDS));
    }
    return true;
}

//! @endcond
