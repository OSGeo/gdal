/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "geom" step of "vector pipeline", or "gdal vector geom" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_INCLUDED
#define GDALALG_VECTOR_GEOM_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorGeomAlgorithm                        */
/************************************************************************/

class GDALVectorGeomAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "geom";
    static constexpr const char *DESCRIPTION =
        "Geometry operations on a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_geom.html";

    explicit GDALVectorGeomAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    /** Register the sub-algorithm of type MyAlgorithm.
     */
    template <class MyAlgorithm> bool RegisterSubAlgorithm(bool standalone)
    {
        GDALAlgorithmRegistry::AlgInfo info;
        info.m_name = MyAlgorithm::NAME;
        info.m_aliases = MyAlgorithm::GetAliasesStatic();
        info.m_creationFunc = [standalone]() -> std::unique_ptr<GDALAlgorithm>
        { return std::make_unique<MyAlgorithm>(standalone); };
        return GDALAlgorithm::RegisterSubAlgorithm(info);
    }
};

/************************************************************************/
/*                    GDALVectorGeomAlgorithmStandalone                 */
/************************************************************************/

class GDALVectorGeomAlgorithmStandalone final : public GDALVectorGeomAlgorithm
{
  public:
    GDALVectorGeomAlgorithmStandalone()
        : GDALVectorGeomAlgorithm(/* standaloneStep = */ true)
    {
    }
};

/************************************************************************/
/*                    GDALVectorGeomAbstractAlgorithm                   */
/************************************************************************/

class GDALVectorGeomAbstractAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  protected:
    struct OptionsBase
    {
        std::string m_activeLayer{};
        std::string m_geomField{};
    };

    virtual std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) = 0;

    GDALVectorGeomAbstractAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    bool standaloneStep, OptionsBase &opts);

    bool RunStep(GDALProgressFunc, void *) override;

  private:
    std::string &m_activeLayer;
};

/************************************************************************/
/*                  GDALVectorGeomOneToOneAlgorithmLayer                */
/************************************************************************/

template <class T>
class GDALVectorGeomOneToOneAlgorithmLayer /* non final */
    : public GDALVectorPipelineOutputLayer
{
  public:
    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_srcLayer.GetLayerDefn();
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        if (!m_poAttrQuery && !m_poFilterGeom)
            return m_srcLayer.GetFeatureCount(bForce);
        return OGRLayer::GetFeatureCount(bForce);
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        return m_srcLayer.GetExtent(iGeomField, psExtent, bForce);
    }

    OGRFeature *GetFeature(GIntBig nFID) override
    {
        auto poSrcFeature =
            std::unique_ptr<OGRFeature>(m_srcLayer.GetFeature(nFID));
        if (!poSrcFeature)
            return nullptr;
        return TranslateFeature(std::move(poSrcFeature)).release();
    }

    int TestCapability(const char *pszCap) override
    {
        if (EQUAL(pszCap, OLCRandomRead) || EQUAL(pszCap, OLCCurveGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries) ||
            EQUAL(pszCap, OLCZGeometries) || EQUAL(pszCap, OLCFastGetExtent) ||
            (EQUAL(pszCap, OLCFastFeatureCount) && !m_poAttrQuery &&
             !m_poFilterGeom) ||
            EQUAL(pszCap, OLCStringsAsUTF8))
        {
            return m_srcLayer.TestCapability(pszCap);
        }
        return false;
    }

  protected:
    const typename T::Options m_opts;

    GDALVectorGeomOneToOneAlgorithmLayer(OGRLayer &oSrcLayer,
                                         const typename T::Options &opts)
        : GDALVectorPipelineOutputLayer(oSrcLayer), m_opts(opts)
    {
        SetDescription(oSrcLayer.GetDescription());
        SetMetadata(oSrcLayer.GetMetadata());
        if (!m_opts.m_geomField.empty())
        {
            const int nIdx = oSrcLayer.GetLayerDefn()->GetGeomFieldIndex(
                m_opts.m_geomField.c_str());
            if (nIdx >= 0)
                m_iGeomIdx = nIdx;
            else
                m_iGeomIdx = INT_MAX;
        }
    }

    bool IsSelectedGeomField(int idx) const
    {
        return m_iGeomIdx < 0 || idx == m_iGeomIdx;
    }

    virtual std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const = 0;

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        auto poDstFeature = TranslateFeature(std::move(poSrcFeature));
        if (poDstFeature)
            apoOutFeatures.push_back(std::move(poDstFeature));
    }

  private:
    int m_iGeomIdx = -1;
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_INCLUDED */
