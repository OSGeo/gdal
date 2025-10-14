/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector create-point"
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_create_point.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALVectorCreatePointAlgorithm()                       */
/************************************************************************/

GDALVectorCreatePointAlgorithm::GDALVectorCreatePointAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("x", 0, _("Field from which X coordinate should be read"), &m_xField)
        .SetRequired();
    AddArg("y", 0, _("Field from which Y coordinate should be read"), &m_yField)
        .SetRequired();
    AddArg("z", 0, _("Optional field from which Z coordinate should be read"),
           &m_zField);
    AddArg("m", 0, _("Optional field from which M coordinate should be read"),
           &m_mField);
    AddArg("dst-crs", 0, _("Destination CRS"), &m_dstCrs).SetIsCRSArg();
}

namespace
{

/************************************************************************/
/*                  GDALVectorCreatePointAlgorithmLayer                 */
/************************************************************************/

class GDALVectorCreatePointAlgorithmLayer final
    : public GDALVectorPipelineOutputLayer
{
  public:
    GDALVectorCreatePointAlgorithmLayer(OGRLayer &oSrcLayer,
                                        const std::string &xField,
                                        const std::string &yField,
                                        const std::string &zField,
                                        const std::string &mField,
                                        OGRSpatialReference *srs)
        : GDALVectorPipelineOutputLayer(oSrcLayer), m_xField(xField),
          m_yField(yField), m_zField(zField), m_mField(mField), m_srs(srs),
          m_defn(oSrcLayer.GetLayerDefn()->Clone())
    {
        m_defn->Reference();
        if (m_srs)
        {
            m_srs->Reference();
        }

        const bool hasZ = !m_zField.empty();
        const bool hasM = !m_mField.empty();

        OGRwkbGeometryType eGeomType = wkbPoint;

        if (hasZ && hasM)
        {
            eGeomType = wkbPointZM;
        }
        else if (hasZ)
        {
            eGeomType = wkbPoint25D;
        }
        else if (hasM)
        {
            eGeomType = wkbPointM;
        }

        auto poGeomFieldDefn =
            std::make_unique<OGRGeomFieldDefn>("geometry", eGeomType);
        if (m_srs)
        {
            poGeomFieldDefn->SetSpatialRef(m_srs);
        }

        m_defn->AddGeomFieldDefn(std::move(poGeomFieldDefn));
    }

    ~GDALVectorCreatePointAlgorithmLayer() override
    {
        m_defn->Release();
        if (m_srs)
        {
            m_srs->Release();
        }
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_defn;
    }

    int TestCapability(const char *pszCap) const override
    {
        return m_srcLayer.TestCapability(pszCap);
    }

  private:
    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override;

    std::string m_xField;
    std::string m_yField;
    std::string m_zField;
    std::string m_mField;
    OGRSpatialReference *m_srs;
    OGRFeatureDefn *m_defn;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorCreatePointAlgorithmLayer)
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

void GDALVectorCreatePointAlgorithmLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeature,
    std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures)
{
    double x = poSrcFeature->GetFieldAsDouble(m_xField.c_str());
    double y = poSrcFeature->GetFieldAsDouble(m_yField.c_str());
    double z = 0;
    double m = 0;

    if (!m_zField.empty())
    {
        z = poSrcFeature->GetFieldAsDouble(m_zField.c_str());
    }
    if (!m_mField.empty())
    {
        m = poSrcFeature->GetFieldAsDouble(m_mField.c_str());
    }

    std::unique_ptr<OGRPoint> poGeom;

    if (m_mField.empty() && m_zField.empty())
    {
        poGeom = std::make_unique<OGRPoint>(x, y);
    }
    else if (m_mField.empty())
    {
        poGeom = std::make_unique<OGRPoint>(x, y, z);
    }
    else if (m_zField.empty())
    {
        poGeom.reset(OGRPoint::createXYM(x, y, m));
    }
    else
    {
        poGeom = std::make_unique<OGRPoint>(x, y, z, m);
    }

    if (m_srs)
    {
        poGeom->assignSpatialReference(m_srs);
    }

    auto poDstFeature = std::make_unique<OGRFeature>(m_defn);
    poDstFeature->SetFID(poSrcFeature->GetFID());
    poDstFeature->SetFrom(poSrcFeature.get());
    poDstFeature->SetGeometry(std::move(poGeom));

    apoOutFeatures.push_back(std::move(poDstFeature));
}

}  // namespace

/************************************************************************/
/*               GDALVectorCreatePointAlgorithm::RunStep()              */
/************************************************************************/

bool GDALVectorCreatePointAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    GDALDataset *poSrcDS = m_inputDataset[0].GetDatasetRef();
    OGRLayer *poSrcLayer = poSrcDS->GetLayer(0);

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    OGRSpatialReference *poCRS = new OGRSpatialReference();
    poCRS->Reference();

    if (!m_dstCrs.empty())
    {
        auto eErr = poCRS->SetFromUserInput(m_dstCrs.c_str());
        if (eErr != OGRERR_NONE)
        {
            poCRS->Release();
            return false;
        }
    }

    outDS->AddLayer(
        *poSrcLayer,
        std::make_unique<GDALVectorCreatePointAlgorithmLayer>(
            *poSrcLayer, m_xField, m_yField, m_zField, m_mField, poCRS));

    m_outputDataset.Set(std::move(outDS));
    poCRS->Release();

    return true;
}

GDALVectorCreatePointAlgorithmStandalone::
    ~GDALVectorCreatePointAlgorithmStandalone() = default;

//! @endcond
