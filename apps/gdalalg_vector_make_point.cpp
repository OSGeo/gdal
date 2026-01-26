/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector make-point"
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_make_point.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALVectorMakePointAlgorithm()                    */
/************************************************************************/

GDALVectorMakePointAlgorithm::GDALVectorMakePointAlgorithm(bool standaloneStep)
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
/*                  GDALVectorMakePointAlgorithmLayer                   */
/************************************************************************/

class GDALVectorMakePointAlgorithmLayer final
    : public GDALVectorPipelineOutputLayer
{
  public:
    GDALVectorMakePointAlgorithmLayer(OGRLayer &oSrcLayer,
                                      const std::string &xField,
                                      const std::string &yField,
                                      const std::string &zField,
                                      const std::string &mField,
                                      OGRSpatialReference *srs)
        : GDALVectorPipelineOutputLayer(oSrcLayer), m_xField(xField),
          m_yField(yField), m_zField(zField), m_mField(mField),
          m_xFieldIndex(
              oSrcLayer.GetLayerDefn()->GetFieldIndex(xField.c_str())),
          m_yFieldIndex(
              oSrcLayer.GetLayerDefn()->GetFieldIndex(yField.c_str())),
          m_zFieldIndex(
              zField.empty()
                  ? -1
                  : oSrcLayer.GetLayerDefn()->GetFieldIndex(zField.c_str())),
          m_mFieldIndex(
              mField.empty()
                  ? -1
                  : oSrcLayer.GetLayerDefn()->GetFieldIndex(mField.c_str())),
          m_hasZ(!zField.empty()), m_hasM(!mField.empty()), m_srs(srs),
          m_defn(oSrcLayer.GetLayerDefn()->Clone())
    {
        m_defn->Reference();
        if (m_srs)
        {
            m_srs->Reference();
        }

        if (!CheckField("X", m_xField, m_xFieldIndex, m_xFieldIsString))
            return;
        if (!CheckField("Y", m_yField, m_yFieldIndex, m_yFieldIsString))
            return;
        if (m_hasZ &&
            !CheckField("Z", m_zField, m_zFieldIndex, m_zFieldIsString))
            return;
        if (m_hasM &&
            !CheckField("M", m_mField, m_mFieldIndex, m_mFieldIsString))
            return;

        OGRwkbGeometryType eGeomType = wkbPoint;
        if (m_hasZ)
            eGeomType = OGR_GT_SetZ(eGeomType);
        if (m_hasM)
            eGeomType = OGR_GT_SetM(eGeomType);

        auto poGeomFieldDefn =
            std::make_unique<OGRGeomFieldDefn>("geometry", eGeomType);
        if (m_srs)
        {
            poGeomFieldDefn->SetSpatialRef(m_srs);
        }

        while (m_defn->GetGeomFieldCount() > 0)
            m_defn->DeleteGeomFieldDefn(0);
        m_defn->AddGeomFieldDefn(std::move(poGeomFieldDefn));
    }

    ~GDALVectorMakePointAlgorithmLayer() override
    {
        m_defn->Release();
        if (m_srs)
        {
            m_srs->Release();
        }
    }

    double GetField(const OGRFeature &feature, int fieldIndex, bool isString)
    {
        if (isString)
        {
            const char *pszValue = feature.GetFieldAsString(fieldIndex);
            while (std::isspace(static_cast<unsigned char>(*pszValue)))
            {
                pszValue++;
            }
            char *end = nullptr;
            double dfValue = CPLStrtodM(pszValue, &end);
            while (std::isspace(static_cast<unsigned char>(*end)))
            {
                end++;
            }
            if (end == pszValue || *end != '\0')
            {
                const char *pszFieldName =
                    m_defn->GetFieldDefn(fieldIndex)->GetNameRef();
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid value in field %s: %s ", pszFieldName,
                         pszValue);
                FailTranslation();
            }
            return dfValue;
        }
        else
        {
            return feature.GetFieldAsDouble(fieldIndex);
        }
    }

    bool CheckField(const std::string &dim, const std::string &fieldName,
                    int index, bool &isStringVar)
    {
        if (index == -1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Specified %s field name '%s' does not exist", dim.c_str(),
                     fieldName.c_str());
            FailTranslation();
            return false;
        }

        const auto eType = m_defn->GetFieldDefn(index)->GetType();
        if (eType == OFTString)
        {
            isStringVar = true;
        }
        else if (eType != OFTInteger && eType != OFTReal)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid %s field type: %s",
                     dim.c_str(), OGR_GetFieldTypeName(eType));
            FailTranslation();
            return false;
        }

        return true;
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

    const std::string m_xField;
    const std::string m_yField;
    const std::string m_zField;
    const std::string m_mField;
    const int m_xFieldIndex;
    const int m_yFieldIndex;
    const int m_zFieldIndex;
    const int m_mFieldIndex;
    const bool m_hasZ;
    const bool m_hasM;
    bool m_xFieldIsString = false;
    bool m_yFieldIsString = false;
    bool m_zFieldIsString = false;
    bool m_mFieldIsString = false;
    OGRSpatialReference *m_srs;
    OGRFeatureDefn *m_defn;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorMakePointAlgorithmLayer)
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

void GDALVectorMakePointAlgorithmLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeature,
    std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures)
{
    const double x = GetField(*poSrcFeature, m_xFieldIndex, m_xFieldIsString);
    const double y = GetField(*poSrcFeature, m_yFieldIndex, m_yFieldIsString);
    const double z =
        m_hasZ ? GetField(*poSrcFeature, m_zFieldIndex, m_zFieldIsString) : 0;
    const double m =
        m_hasM ? GetField(*poSrcFeature, m_mFieldIndex, m_mFieldIsString) : 0;

    std::unique_ptr<OGRPoint> poGeom;

    if (m_hasZ && m_hasM)
    {
        poGeom = std::make_unique<OGRPoint>(x, y, z, m);
    }
    else if (m_hasZ)
    {
        poGeom = std::make_unique<OGRPoint>(x, y, z);
    }
    else if (m_hasM)
    {
        poGeom.reset(OGRPoint::createXYM(x, y, m));
    }
    else
    {
        poGeom = std::make_unique<OGRPoint>(x, y);
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
/*               GDALVectorMakePointAlgorithm::RunStep()                */
/************************************************************************/

bool GDALVectorMakePointAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    GDALDataset *poSrcDS = m_inputDataset[0].GetDatasetRef();
    if (poSrcDS->GetLayerCount() == 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "No input vector layer");
        return false;
    }
    OGRLayer *poSrcLayer = poSrcDS->GetLayer(0);

    std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> poCRS;
    if (!m_dstCrs.empty())
    {
        poCRS.reset(new OGRSpatialReference());
        poCRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        auto eErr = poCRS->SetFromUserInput(m_dstCrs.c_str());
        if (eErr != OGRERR_NONE)
        {
            return false;
        }
    }

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    outDS->AddLayer(
        *poSrcLayer,
        std::make_unique<GDALVectorMakePointAlgorithmLayer>(
            *poSrcLayer, m_xField, m_yField, m_zField, m_mField, poCRS.get()));

    m_outputDataset.Set(std::move(outDS));

    return true;
}

GDALVectorMakePointAlgorithmStandalone::
    ~GDALVectorMakePointAlgorithmStandalone() = default;

//! @endcond
