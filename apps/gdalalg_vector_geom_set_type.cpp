/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom set-type"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom_set_type.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*   GDALVectorGeomSetTypeAlgorithm::GDALVectorGeomSetTypeAlgorithm()   */
/************************************************************************/

GDALVectorGeomSetTypeAlgorithm::GDALVectorGeomSetTypeAlgorithm(
    bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
    AddArg("layer-only", 0, _("Only modify the layer geometry type"),
           &m_opts.m_layerOnly)
        .SetMutualExclusionGroup("only");
    AddArg("feature-only", 0, _("Only modify the geometry type of features"),
           &m_opts.m_featureGeomOnly)
        .SetMutualExclusionGroup("only");

    AddArg("geometry-type", 0, _("Geometry type"), &m_opts.m_type)
        .SetAutoCompleteFunction(
            [](const std::string &currentValue)
            {
                std::vector<std::string> oRet;
                for (const char *type :
                     {"GEOMETRY", "POINT", "LINESTRING", "POLYGON",
                      "MULTIPOINT", "MULTILINESTRING", "MULTIPOLYGON",
                      "GEOMETRYCOLLECTION", "CURVE", "CIRCULARSTRING",
                      "COMPOUNDCURVE", "SURFACE", "CURVEPOLYGON", "MULTICURVE",
                      "MULTISURFACE", "POLYHEDRALSURFACE", "TIN"})
                {
                    if (currentValue.empty() ||
                        STARTS_WITH(type, currentValue.c_str()))
                    {
                        oRet.push_back(type);
                        oRet.push_back(std::string(type).append("Z"));
                        oRet.push_back(std::string(type).append("M"));
                        oRet.push_back(std::string(type).append("ZM"));
                    }
                }
                return oRet;
            });

    AddArg("multi", 0, _("Force geometries to MULTI geometry types"),
           &m_opts.m_multi)
        .SetMutualExclusionGroup("multi-single");
    AddArg("single", 0, _("Force geometries to non-MULTI geometry types"),
           &m_opts.m_single)
        .SetMutualExclusionGroup("multi-single");

    AddArg("linear", 0, _("Convert curve geometries to linear types"),
           &m_opts.m_linear)
        .SetMutualExclusionGroup("linear-curve");
    AddArg("curve", 0, _("Convert linear geometries to curve types"),
           &m_opts.m_curve)
        .SetMutualExclusionGroup("linear-curve");

    AddArg("dim", 0, _("Force geometries to the specified dimension"),
           &m_opts.m_dim)
        .SetChoices("XY", "XYZ", "XYM", "XYZM");

    AddArg("skip", 0,
           _("Skip feature when change of feature geometry type failed"),
           &m_opts.m_skip);
}

namespace
{

/************************************************************************/
/*                GDALVectorGeomSetTypeAlgorithmLayer                   */
/************************************************************************/

class GDALVectorGeomSetTypeAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<
          GDALVectorGeomSetTypeAlgorithm>
{
  public:
    GDALVectorGeomSetTypeAlgorithmLayer(
        OGRLayer &oSrcLayer,
        const GDALVectorGeomSetTypeAlgorithm::Options &opts);

    ~GDALVectorGeomSetTypeAlgorithmLayer() override
    {
        m_poFeatureDefn->Release();
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        if (!m_opts.m_skip && !m_poAttrQuery && !m_poFilterGeom)
            return m_srcLayer.GetFeatureCount(bForce);
        return OGRLayer::GetFeatureCount(bForce);
    }

    int TestCapability(const char *pszCap) override
    {
        if (EQUAL(pszCap, OLCRandomRead) || EQUAL(pszCap, OLCCurveGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries) ||
            EQUAL(pszCap, OLCZGeometries) ||
            (EQUAL(pszCap, OLCFastFeatureCount) && !m_opts.m_skip &&
             !m_poAttrQuery && !m_poFilterGeom) ||
            EQUAL(pszCap, OLCFastGetExtent) || EQUAL(pszCap, OLCStringsAsUTF8))
        {
            return m_srcLayer.TestCapability(pszCap);
        }
        return false;
    }

  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override;

  private:
    OGRFeatureDefn *m_poFeatureDefn = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorGeomSetTypeAlgorithmLayer)

    OGRwkbGeometryType ConvertType(OGRwkbGeometryType eType) const;
};

/************************************************************************/
/*                  GDALVectorGeomSetTypeAlgorithmLayer()               */
/************************************************************************/

GDALVectorGeomSetTypeAlgorithmLayer::GDALVectorGeomSetTypeAlgorithmLayer(
    OGRLayer &oSrcLayer, const GDALVectorGeomSetTypeAlgorithm::Options &opts)
    : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorGeomSetTypeAlgorithm>(
          oSrcLayer, opts),
      m_poFeatureDefn(oSrcLayer.GetLayerDefn()->Clone())
{
    m_poFeatureDefn->Reference();

    if (!m_opts.m_featureGeomOnly)
    {
        for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
        {
            if (IsSelectedGeomField(i))
            {
                auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
                poGeomFieldDefn->SetType(
                    ConvertType(poGeomFieldDefn->GetType()));
            }
        }
    }
}

/************************************************************************/
/*                           ConvertType()                              */
/************************************************************************/

OGRwkbGeometryType
GDALVectorGeomSetTypeAlgorithmLayer::ConvertType(OGRwkbGeometryType eType) const
{
    if (!m_opts.m_type.empty())
        return m_opts.m_eType;

    OGRwkbGeometryType eRetType = eType;

    if (m_opts.m_multi)
    {
        if (eRetType == wkbTriangle || eRetType == wkbTIN ||
            eRetType == wkbPolyhedralSurface)
        {
            eRetType = wkbMultiPolygon;
        }
        else if (!OGR_GT_IsSubClassOf(eRetType, wkbGeometryCollection))
        {
            eRetType = OGR_GT_GetCollection(eRetType);
        }
    }
    else if (m_opts.m_single)
    {
        eRetType = OGR_GT_GetSingle(eRetType);
    }

    if (m_opts.m_linear)
    {
        eRetType = OGR_GT_GetLinear(eRetType);
    }
    else if (m_opts.m_curve)
    {
        eRetType = OGR_GT_GetCurve(eRetType);
    }

    if (EQUAL(m_opts.m_dim.c_str(), "XY"))
    {
        eRetType = OGR_GT_Flatten(eRetType);
    }
    else if (EQUAL(m_opts.m_dim.c_str(), "XYZ"))
    {
        eRetType = OGR_GT_SetZ(OGR_GT_Flatten(eRetType));
    }
    else if (EQUAL(m_opts.m_dim.c_str(), "XYM"))
    {
        eRetType = OGR_GT_SetM(OGR_GT_Flatten(eRetType));
    }
    else if (EQUAL(m_opts.m_dim.c_str(), "XYZM"))
    {
        eRetType = OGR_GT_SetZ(OGR_GT_SetM(OGR_GT_Flatten(eRetType)));
    }

    return eRetType;
}

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature>
GDALVectorGeomSetTypeAlgorithmLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeature) const
{
    poSrcFeature->SetFDefnUnsafe(m_poFeatureDefn);
    for (int i = 0; i < poSrcFeature->GetGeomFieldCount(); ++i)
    {
        auto poGeom = poSrcFeature->GetGeomFieldRef(i);
        if (poGeom)
        {
            const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
            if (!m_opts.m_layerOnly && IsSelectedGeomField(i))
            {
                poGeom = poSrcFeature->StealGeometry(i);
                const auto eTargetType = ConvertType(poGeom->getGeometryType());
                auto poNewGeom = std::unique_ptr<OGRGeometry>(
                    OGRGeometryFactory::forceTo(poGeom, eTargetType));
                if (m_opts.m_skip &&
                    (!poNewGeom ||
                     (wkbFlatten(eTargetType) != wkbUnknown &&
                      poNewGeom->getGeometryType() != eTargetType)))
                {
                    return nullptr;
                }
                poNewGeom->assignSpatialReference(
                    poGeomFieldDefn->GetSpatialRef());
                poSrcFeature->SetGeomFieldDirectly(i, poNewGeom.release());
            }
            else
            {
                poGeom->assignSpatialReference(
                    poGeomFieldDefn->GetSpatialRef());
            }
        }
    }
    return poSrcFeature;
}

}  // namespace

/************************************************************************/
/*           GDALVectorGeomSetTypeAlgorithm::CreateAlgLayer()           */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorGeomSetTypeAlgorithm::CreateAlgLayer(OGRLayer &srcLayer)
{
    return std::make_unique<GDALVectorGeomSetTypeAlgorithmLayer>(srcLayer,
                                                                 m_opts);
}

/************************************************************************/
/*            GDALVectorGeomSetTypeAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALVectorGeomSetTypeAlgorithm::RunStep(GDALProgressFunc, void *)
{
    if (!m_opts.m_type.empty())
    {
        if (m_opts.m_multi || m_opts.m_single || m_opts.m_linear ||
            m_opts.m_curve || !m_opts.m_dim.empty())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "--geometry-type cannot be used with any of "
                        "--multi/single/linear/multi/dim");
            return false;
        }

        m_opts.m_eType = OGRFromOGCGeomType(m_opts.m_type.c_str());
        if (wkbFlatten(m_opts.m_eType) == wkbUnknown &&
            !STARTS_WITH_CI(m_opts.m_type.c_str(), "GEOMETRY"))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Invalid geometry type '%s'", m_opts.m_type.c_str());
            return false;
        }
    }

    return GDALVectorGeomAbstractAlgorithm::RunStep(nullptr, nullptr);
}

//! @endcond
