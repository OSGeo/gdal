/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Base classes for some geometry-related vector algorithms
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
#include "ogr_geos.h"

//! @cond Doxygen_Suppress

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

    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

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
    const OGRFeatureDefn *GetLayerDefn() const override
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

    int TestCapability(const char *pszCap) const override
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

#ifdef HAVE_GEOS

/************************************************************************/
/*                    GDALGeosNonStreamingAlgorithmDataset              */
/************************************************************************/

/** A GDALGeosNonStreamingAlgorithmDataset manages the work of reading features
 *  from an input layer, converting OGR geometries into GEOS geometries,
 *  applying a GEOS function, and writing result to an output layer. It is
 *  appropriate only for GEOS algorithms that operate on all input geometries
 *  at a single time.
 */
class GDALGeosNonStreamingAlgorithmDataset
    : public GDALVectorNonStreamingAlgorithmDataset
{
  public:
    GDALGeosNonStreamingAlgorithmDataset();

    ~GDALGeosNonStreamingAlgorithmDataset() override;

    CPL_DISALLOW_COPY_ASSIGN(GDALGeosNonStreamingAlgorithmDataset)

    bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer, int geomFieldIndex,
                 GDALProgressFunc pfnProgress, void *pProgressData) override;

    virtual bool ProcessGeos() = 0;

    /// Whether the operation should fail if non-polygonal geometries are present
    virtual bool PolygonsOnly() const = 0;

    /// Whether empty result features should be excluded from the output
    virtual bool SkipEmpty() const = 0;

  protected:
    GEOSContextHandle_t m_poGeosContext{nullptr};
    std::vector<GEOSGeometry *> m_apoGeosInputs{};
    GEOSGeometry *m_poGeosResultAsCollection{nullptr};
    GEOSGeometry **m_papoGeosResults{nullptr};

  private:
    bool ConvertInputsToGeos(OGRLayer &srcLayer, OGRLayer &dstLayer,
                             int geomFieldIndex, bool sameDefn,
                             GDALProgressFunc pfnProgress, void *pProgressData);

    bool ConvertOutputsFromGeos(OGRLayer &dstLayer,
                                GDALProgressFunc pfnProgress,
                                void *pProgressData, double dfProgressStart,
                                double dfProgressRatio);

    void Cleanup();

    std::vector<std::unique_ptr<OGRFeature>> m_apoFeatures{};
    unsigned int m_nGeosResultSize{0};
};

#endif

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_INCLUDED */
