/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "filter" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_filter.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include <set>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALVectorFilterAlgorithm::GDALVectorFilterAlgorithm()        */
/************************************************************************/

GDALVectorFilterAlgorithm::GDALVectorFilterAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddActiveLayerArg(&m_activeLayer);
    AddBBOXArg(&m_bbox);
    AddArg("where", 0,
           _("Attribute query in a restricted form of the queries used in the "
             "SQL WHERE statement"),
           &m_where)
        .SetReadFromFileAtSyntaxAllowed()
        .SetMetaVar("<WHERE>|@<filename>")
        .SetRemoveSQLCommentsEnabled();
    AddArg("update-extent", 0,
           _("Update layer extent to take into account the filter"),
           &m_updateExtent);
}

/************************************************************************/
/*              GDALVectorFilterAlgorithmLayerChangeExtent              */
/************************************************************************/

namespace
{
class GDALVectorFilterAlgorithmLayerChangeExtent final
    : public GDALVectorPipelinePassthroughLayer
{
  public:
    GDALVectorFilterAlgorithmLayerChangeExtent(
        OGRLayer &oSrcLayer, const OGREnvelope3D &sLayerEnvelope)
        : GDALVectorPipelinePassthroughLayer(oSrcLayer),
          m_sLayerEnvelope(sLayerEnvelope)
    {
    }

    OGRErr IGetExtent(int /*iGeomField*/, OGREnvelope *psExtent,
                      bool /* bForce */) override
    {
        if (m_sLayerEnvelope.IsInit())
        {
            *psExtent = m_sLayerEnvelope;
            return OGRERR_NONE;
        }
        else
        {
            return OGRERR_FAILURE;
        }
    }

    OGRErr IGetExtent3D(int /*iGeomField*/, OGREnvelope3D *psExtent,
                        bool /* bForce */) override
    {
        if (m_sLayerEnvelope.IsInit())
        {
            *psExtent = m_sLayerEnvelope;
            return OGRERR_NONE;
        }
        else
        {
            return OGRERR_FAILURE;
        }
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCFastGetExtent))
            return true;
        return m_srcLayer.TestCapability(pszCap);
    }

  private:
    const OGREnvelope3D m_sLayerEnvelope;
};

}  // namespace

/************************************************************************/
/*                 GDALVectorFilterAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALVectorFilterAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    const int nLayerCount = poSrcDS->GetLayerCount();

    bool ret = true;
    if (m_bbox.size() == 4)
    {
        const double xmin = m_bbox[0];
        const double ymin = m_bbox[1];
        const double xmax = m_bbox[2];
        const double ymax = m_bbox[3];
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (poSrcLayer && (m_activeLayer.empty() ||
                               m_activeLayer == poSrcLayer->GetDescription()))
                poSrcLayer->SetSpatialFilterRect(xmin, ymin, xmax, ymax);
        }
    }

    if (ret && !m_where.empty())
    {
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (ret && (m_activeLayer.empty() ||
                        m_activeLayer == poSrcLayer->GetDescription()))
            {
                ret = poSrcLayer->SetAttributeFilter(m_where.c_str()) ==
                      OGRERR_NONE;
            }
        }
    }

    if (ret && m_updateExtent)
    {
        auto outDS =
            std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

        int64_t nTotalFeatures = 0;
        if (ctxt.m_pfnProgress)
        {
            for (int i = 0; ret && i < nLayerCount; ++i)
            {
                auto poSrcLayer = poSrcDS->GetLayer(i);
                ret = (poSrcLayer != nullptr);
                if (ret)
                {
                    if (m_activeLayer.empty() ||
                        m_activeLayer == poSrcLayer->GetDescription())
                    {
                        if (poSrcLayer->TestCapability(OLCFastFeatureCount))
                        {
                            const auto nFC = poSrcLayer->GetFeatureCount(false);
                            if (nFC < 0)
                            {
                                nTotalFeatures = 0;
                                break;
                            }
                            nTotalFeatures += nFC;
                        }
                    }
                }
            }
        }

        int64_t nFeatureCounter = 0;
        for (int i = 0; ret && i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = (poSrcLayer != nullptr);
            if (ret)
            {
                if (m_activeLayer.empty() ||
                    m_activeLayer == poSrcLayer->GetDescription())
                {
                    OGREnvelope3D sLayerEnvelope, sFeatureEnvelope;
                    for (auto &&poFeature : poSrcLayer)
                    {
                        const auto poGeom = poFeature->GetGeometryRef();
                        if (poGeom && !poGeom->IsEmpty())
                        {
                            poGeom->getEnvelope(&sFeatureEnvelope);
                            sLayerEnvelope.Merge(sFeatureEnvelope);
                        }

                        ++nFeatureCounter;
                        if (nTotalFeatures > 0 && ctxt.m_pfnProgress &&
                            !ctxt.m_pfnProgress(
                                static_cast<double>(nFeatureCounter) /
                                    static_cast<double>(nTotalFeatures),
                                "", ctxt.m_pProgressData))
                        {
                            ReportError(CE_Failure, CPLE_UserInterrupt,
                                        "Interrupted by user");
                            return false;
                        }
                    }
                    outDS->AddLayer(
                        *poSrcLayer,
                        std::make_unique<
                            GDALVectorFilterAlgorithmLayerChangeExtent>(
                            *poSrcLayer, sLayerEnvelope));
                }
                else
                {
                    outDS->AddLayer(
                        *poSrcLayer,
                        std::make_unique<GDALVectorPipelinePassthroughLayer>(
                            *poSrcLayer));
                }
            }
        }

        if (ret)
            m_outputDataset.Set(std::move(outDS));
    }
    else if (ret)
    {
        m_outputDataset.Set(poSrcDS);
    }

    return ret;
}

GDALVectorFilterAlgorithmStandalone::~GDALVectorFilterAlgorithmStandalone() =
    default;

//! @endcond
