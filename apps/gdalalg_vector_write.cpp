/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "write" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_write.h"
#include "cpl_string.h"
#include "gdal_utils.h"
#include "gdal_priv.h"

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

/************************************************************************/
/*         GDALVectorWriteAlgorithm::GDALVectorWriteAlgorithm()         */
/************************************************************************/

GDALVectorWriteAlgorithm::GDALVectorWriteAlgorithm()
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(false)
                                          .SetNoCreateEmptyLayersArgument(true))
{
    AddVectorOutputArgs(/* hiddenForCLI = */ false,
                        /* shortNameOutputLayerAllowed=*/true);
}

/************************************************************************/
/*                 GDALVectorWriteAlgorithm::RunStep()                  */
/************************************************************************/

namespace
{
class OGRReadBufferedLayer
    : public OGRLayer,
      public OGRGetNextFeatureThroughRaw<OGRReadBufferedLayer>
{
  public:
    explicit OGRReadBufferedLayer(OGRLayer &srcLayer)
        : m_srcLayer(srcLayer), m_poFeature(nullptr)
    {
        m_poFeature.reset(m_srcLayer.GetNextFeature());
    }

    ~OGRReadBufferedLayer() override;

    const char *GetDescription() const override
    {
        return m_srcLayer.GetDescription();
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        if (m_poAttrQuery == nullptr && m_poFilterGeom == nullptr)
        {
            return m_srcLayer.GetFeatureCount(bForce);
        }

        return OGRLayer::GetFeatureCount(bForce);
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_srcLayer.GetLayerDefn();
    }

    OGRFeature *GetNextRawFeature()
    {
        auto ret = m_poFeature.release();
        m_poFeature.reset(m_srcLayer.GetNextFeature());
        return ret;
    }

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRReadBufferedLayer)

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        return m_srcLayer.GetExtent(iGeomField, psExtent, bForce);
    }

    OGRErr IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent,
                        bool bForce) override
    {
        return m_srcLayer.GetExtent3D(iGeomField, psExtent, bForce);
    }

    const OGRFeature *PeekNextFeature() const
    {
        return m_poFeature.get();
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCFastFeatureCount) ||
            EQUAL(pszCap, OLCFastGetExtent) ||
            EQUAL(pszCap, OLCFastGetExtent3D) ||
            EQUAL(pszCap, OLCZGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries) ||
            EQUAL(pszCap, OLCCurveGeometries))
        {
            return m_srcLayer.TestCapability(pszCap);
        }

        return false;
    }

    void ResetReading() override
    {
        m_srcLayer.ResetReading();
        m_poFeature.reset(m_srcLayer.GetNextFeature());
    }

  private:
    OGRLayer &m_srcLayer;
    std::unique_ptr<OGRFeature> m_poFeature;
};

OGRReadBufferedLayer::~OGRReadBufferedLayer() = default;

class GDALReadBufferedDataset final : public GDALDataset
{
  public:
    explicit GDALReadBufferedDataset(GDALDataset &srcDS) : m_srcDS(srcDS)
    {
        m_srcDS.Reference();

        for (int i = 0; i < srcDS.GetLayerCount(); i++)
        {
            auto poLayer =
                std::make_unique<OGRReadBufferedLayer>(*srcDS.GetLayer(i));
            if (poLayer->PeekNextFeature())
            {
                m_layers.push_back(std::move(poLayer));
            }
        }
    }

    ~GDALReadBufferedDataset() override;

    int GetLayerCount() const override
    {
        return static_cast<int>(m_layers.size());
    }

    const OGRLayer *GetLayer(int nLayer) const override
    {
        if (nLayer < 0 || nLayer >= static_cast<int>(m_layers.size()))
        {
            return nullptr;
        }
        return m_layers[nLayer].get();
    }

  private:
    GDALDataset &m_srcDS;
    std::vector<std::unique_ptr<OGRReadBufferedLayer>> m_layers{};
};

GDALReadBufferedDataset::~GDALReadBufferedDataset()
{
    m_srcDS.Release();
}

}  // namespace

bool GDALVectorWriteAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    std::unique_ptr<GDALDataset> poReadBufferedDataset;

    if (m_noCreateEmptyLayers)
    {
        if (poSrcDS->TestCapability(ODsCRandomLayerRead))
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Source dataset supports random-layer reading, but this "
                "is not compatible with --no-create-empty-layers. Attempting "
                "to read features by layer, but this may fail if the "
                "source dataset is large.");
        }

        poReadBufferedDataset =
            std::make_unique<GDALReadBufferedDataset>(*poSrcDS);

        if (m_format == "stream")
        {
            m_outputDataset.Set(std::move(poReadBufferedDataset));
            return true;
        }

        poSrcDS = poReadBufferedDataset.get();
    }

    if (m_format == "stream")
    {
        m_outputDataset.Set(poSrcDS);
        return true;
    }

    CPLStringList aosOptions;
    aosOptions.AddString("--invoked-from-gdal-algorithm");
    if (!m_overwrite)
    {
        aosOptions.AddString("--no-overwrite");
    }
    if (m_overwriteLayer)
    {
        aosOptions.AddString("-overwrite");
    }
    if (m_appendLayer)
    {
        aosOptions.AddString("-append");
    }
    if (m_upsert)
    {
        aosOptions.AddString("-upsert");
    }
    if (!m_format.empty())
    {
        aosOptions.AddString("-of");
        aosOptions.AddString(m_format.c_str());
    }
    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-dsco");
        aosOptions.AddString(co.c_str());
    }
    for (const auto &co : m_layerCreationOptions)
    {
        aosOptions.AddString("-lco");
        aosOptions.AddString(co.c_str());
    }
    if (!m_outputLayerName.empty())
    {
        aosOptions.AddString("-nln");
        aosOptions.AddString(m_outputLayerName.c_str());
    }
    if (pfnProgress && pfnProgress != GDALDummyProgress)
    {
        aosOptions.AddString("-progress");
    }
    if (m_skipErrors)
    {
        aosOptions.AddString("-skipfailures");
    }

    GDALDataset *poRetDS = nullptr;
    GDALDatasetH hOutDS =
        GDALDataset::ToHandle(m_outputDataset.GetDatasetRef());
    GDALVectorTranslateOptions *psOptions =
        GDALVectorTranslateOptionsNew(aosOptions.List(), nullptr);
    if (psOptions)
    {
        GDALVectorTranslateOptionsSetProgress(psOptions, pfnProgress,
                                              pProgressData);

        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
        poRetDS = GDALDataset::FromHandle(
            GDALVectorTranslate(m_outputDataset.GetName().c_str(), hOutDS, 1,
                                &hSrcDS, psOptions, nullptr));
        GDALVectorTranslateOptionsFree(psOptions);
    }

    if (!poRetDS)
    {
        return false;
    }

    if (!hOutDS)
    {
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
    }

    return true;
}

//! @endcond
