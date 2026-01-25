/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "limit" step of "vector pipeline"
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_limit.h"
#include "gdalalg_vector_pipeline.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include <algorithm>
#include <set>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*         GDALVectorLimitAlgorithm::GDALVectorLimitAlgorithm()         */
/************************************************************************/

GDALVectorLimitAlgorithm::GDALVectorLimitAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("limit", 0, _("Limit the number of features to read per layer"),
           &m_featureLimit)
        .SetPositional()
        .SetRequired();
    AddActiveLayerArg(&m_activeLayer);
}

namespace
{

/************************************************************************/
/*                      GDALVectorReadLimitedLayer                      */
/************************************************************************/

class GDALVectorReadLimitedLayer : public OGRLayer
{
  public:
    GDALVectorReadLimitedLayer(OGRLayer &layer, int featureLimit)
        : m_srcLayer(layer), m_featureLimit(featureLimit), m_featuresRead(0)
    {
    }

    ~GDALVectorReadLimitedLayer() override;

    OGRFeature *GetNextFeature() override
    {
        if (m_featuresRead < m_featureLimit)
        {
            m_featuresRead++;
            return m_srcLayer.GetNextFeature();
        }
        return nullptr;
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_srcLayer.GetLayerDefn();
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        return std::min(m_featureLimit, m_srcLayer.GetFeatureCount(bForce));
    }

    void ResetReading() override
    {
        m_featuresRead = 0;
        m_srcLayer.ResetReading();
    }

    int TestCapability(const char *pszCap) const override
    {
        return m_srcLayer.TestCapability(pszCap);
    }

  private:
    OGRLayer &m_srcLayer;
    GIntBig m_featureLimit;
    GIntBig m_featuresRead;
};

GDALVectorReadLimitedLayer::~GDALVectorReadLimitedLayer() = default;

}  // namespace

/************************************************************************/
/*                 GDALVectorLimitAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALVectorLimitAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    auto outDS = std::make_unique<GDALVectorOutputDataset>();

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_activeLayer.empty() ||
            m_activeLayer == poSrcLayer->GetDescription())
        {
            outDS->AddLayer(std::make_unique<GDALVectorReadLimitedLayer>(
                *poSrcLayer, m_featureLimit));
        }
        else
        {
            outDS->AddLayer(
                std::make_unique<GDALVectorPipelinePassthroughLayer>(
                    *poSrcLayer));
        }
    }

    m_outputDataset.Set(std::move(outDS));

    return true;
}

//! @endcond
