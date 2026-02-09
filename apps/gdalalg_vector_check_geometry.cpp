/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector check-geometry" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_check_geometry.h"

#include "cpl_error.h"
#include "gdal_priv.h"
#include "gdalalg_vector_geom.h"
#include "ogr_geometry.h"
#include "ogr_geos.h"

#include <cinttypes>

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

GDALVectorCheckGeometryAlgorithm::GDALVectorCheckGeometryAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("include-field", 0,
           _("Fields from input layer to include in output (special values: "
             "ALL and NONE)"),
           &m_includeFields)
        .SetDefault("NONE");

    AddArg("include-valid", 0,
           _("Include valid inputs in output, with empty geometry"),
           &m_includeValid);

    AddArg("geometry-field", 0, _("Name of geometry field to check"),
           &m_geomField);
}

#ifdef HAVE_GEOS

class GDALInvalidLocationLayer final : public GDALVectorPipelineOutputLayer
{
  private:
    static constexpr const char *ERROR_DESCRIPTION_FIELD = "error";

  public:
    GDALInvalidLocationLayer(OGRLayer &layer,
                             const std::vector<int> &srcFieldIndices,
                             bool bSingleLayerOutput, int srcGeomField,
                             bool skipValid)
        : GDALVectorPipelineOutputLayer(layer),
          m_defn(OGRFeatureDefn::CreateFeatureDefn(
              bSingleLayerOutput ? "error_location"
                                 : std::string("error_location_")
                                       .append(layer.GetDescription())
                                       .c_str())),
          m_geosContext(OGRGeometry::createGEOSContext()),
          m_srcGeomField(srcGeomField), m_skipValid(skipValid)
    {
        m_defn->Reference();
        m_defn->SetGeomType(wkbMultiPoint);

        if (!srcFieldIndices.empty())
        {
            const OGRFeatureDefn &srcDefn = *layer.GetLayerDefn();
            m_srcFieldMap.resize(srcDefn.GetFieldCount(), -1);
            int iDstField = 0;
            for (int iSrcField : srcFieldIndices)
            {
                m_defn->AddFieldDefn(srcDefn.GetFieldDefn(iSrcField));
                m_srcFieldMap[iSrcField] = iDstField++;
            }
        }

        auto poDescriptionFieldDefn =
            std::make_unique<OGRFieldDefn>(ERROR_DESCRIPTION_FIELD, OFTString);
        m_defn->AddFieldDefn(std::move(poDescriptionFieldDefn));

        m_defn->GetGeomFieldDefn(0)->SetSpatialRef(
            m_srcLayer.GetLayerDefn()
                ->GetGeomFieldDefn(m_srcGeomField)
                ->GetSpatialRef());
    }

    ~GDALInvalidLocationLayer() override;

    int TestCapability(const char *) const override
    {
        return false;
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_defn;
    }

    std::unique_ptr<OGRFeature> CreateFeatureFromLastError() const
    {
        auto poErrorFeature = std::make_unique<OGRFeature>(m_defn);

        std::string msg = CPLGetLastErrorMsg();

        // Trim GEOS exception name
        const auto subMsgPos = msg.find(": ");
        if (subMsgPos != std::string::npos)
        {
            msg = msg.substr(subMsgPos + strlen(": "));
        }

        // Trim newline from end of GEOS exception message
        if (!msg.empty() && msg.back() == '\n')
        {
            msg.pop_back();
        }

        poErrorFeature->SetField(ERROR_DESCRIPTION_FIELD, msg.c_str());

        CPLErrorReset();

        return poErrorFeature;
    }

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutputFeatures) override
    {
        const OGRGeometry *poGeom =
            poSrcFeature->GetGeomFieldRef(m_srcGeomField);
        std::unique_ptr<OGRFeature> poErrorFeature;

        if (poGeom)
        {
            if (poGeom->getDimension() < 1)
            {
                CPLErrorOnce(CE_Warning, CPLE_AppDefined,
                             "Point geometry passed to 'gdal vector "
                             "check-geometry'. Point geometries are "
                             "always valid/simple.");
            }
            else
            {
                auto eType = wkbFlatten(poGeom->getGeometryType());
                GEOSGeometry *poGeosGeom = poGeom->exportToGEOS(m_geosContext);

                if (!poGeosGeom)
                {
                    // Try to find a useful message / coordinate from
                    // GEOS exception message.
                    poErrorFeature = CreateFeatureFromLastError();

                    if (eType == wkbPolygon)
                    {
                        const OGRLinearRing *poRing =
                            poGeom->toPolygon()->getExteriorRing();
                        if (poRing != nullptr && !poRing->IsEmpty())
                        {
                            auto poPoint = std::make_unique<OGRPoint>();
                            poRing->StartPoint(poPoint.get());
                            auto poMultiPoint =
                                std::make_unique<OGRMultiPoint>();
                            poMultiPoint->addGeometry(std::move(poPoint));
                            poErrorFeature->SetGeometry(
                                std::move(poMultiPoint));
                        }
                        else
                        {
                            // TODO get a point from somewhere else?
                        }
                    }
                }
                else
                {
                    char *pszReason = nullptr;
                    GEOSGeometry *location = nullptr;
                    char ret = 1;
                    bool warnAboutGeosVersion = false;
                    bool checkedSimple = false;

                    if (eType == wkbPolygon || eType == wkbMultiPolygon ||
                        eType == wkbCurvePolygon || eType == wkbMultiSurface ||
                        eType == wkbGeometryCollection)
                    {
                        ret = GEOSisValidDetail_r(m_geosContext, poGeosGeom, 0,
                                                  &pszReason, &location);
                    }

                    if (eType == wkbLineString || eType == wkbMultiLineString ||
                        eType == wkbCircularString ||
                        eType == wkbCompoundCurve ||
                        (ret == 1 && eType == wkbGeometryCollection))
                    {
                        checkedSimple = true;
#if GEOS_VERSION_MAJOR > 3 ||                                                  \
    (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 14)
                        ret = GEOSisSimpleDetail_r(m_geosContext, poGeosGeom, 1,
                                                   &location);
#else
                        ret = GEOSisSimple_r(m_geosContext, poGeosGeom);
                        warnAboutGeosVersion = true;
#endif
                    }

                    GEOSGeom_destroy_r(m_geosContext, poGeosGeom);
                    if (ret == 0)
                    {
                        if (warnAboutGeosVersion)
                        {
                            CPLErrorOnce(
                                CE_Warning, CPLE_AppDefined,
                                "Detected a non-simple linear geometry, but "
                                "cannot output self-intersection points "
                                "because GEOS library version is < 3.14.");
                        }

                        poErrorFeature = std::make_unique<OGRFeature>(m_defn);
                        if (pszReason == nullptr)
                        {
                            if (checkedSimple)
                            {
                                poErrorFeature->SetField(
                                    ERROR_DESCRIPTION_FIELD,
                                    "self-intersection");
                            }
                        }
                        else
                        {
                            poErrorFeature->SetField(ERROR_DESCRIPTION_FIELD,
                                                     pszReason);
                            GEOSFree_r(m_geosContext, pszReason);
                        }

                        if (location != nullptr)
                        {
                            std::unique_ptr<OGRGeometry> poErrorGeom(
                                OGRGeometryFactory::createFromGEOS(
                                    m_geosContext, location));
                            GEOSGeom_destroy_r(m_geosContext, location);

                            if (poErrorGeom->getGeometryType() == wkbPoint)
                            {
                                auto poMultiPoint =
                                    std::make_unique<OGRMultiPoint>();
                                poMultiPoint->addGeometry(
                                    std::move(poErrorGeom));
                                poErrorGeom = std::move(poMultiPoint);
                            }

                            poErrorGeom->assignSpatialReference(
                                m_srcLayer.GetLayerDefn()
                                    ->GetGeomFieldDefn(m_srcGeomField)
                                    ->GetSpatialRef());

                            poErrorFeature->SetGeometry(std::move(poErrorGeom));
                        }
                    }
                    else if (ret == 2)
                    {
                        poErrorFeature = CreateFeatureFromLastError();
                    }
                }
            }
        }

        if (!poErrorFeature && !m_skipValid)
        {
            poErrorFeature = std::make_unique<OGRFeature>(m_defn);
            // TODO Set geometry to POINT EMPTY ?
        }

        if (poErrorFeature)
        {
            if (!m_srcFieldMap.empty())
            {
                poErrorFeature->SetFieldsFrom(
                    poSrcFeature.get(), m_srcFieldMap.data(), false, false);
            }
            poErrorFeature->SetFID(poSrcFeature->GetFID());
            apoOutputFeatures.push_back(std::move(poErrorFeature));
        }
    }

    CPL_DISALLOW_COPY_ASSIGN(GDALInvalidLocationLayer)

  private:
    std::vector<int> m_srcFieldMap{};
    OGRFeatureDefn *const m_defn;
    const GEOSContextHandle_t m_geosContext;
    const int m_srcGeomField;
    const bool m_skipValid;
};

GDALInvalidLocationLayer::~GDALInvalidLocationLayer()
{
    m_defn->Release();
    finishGEOS_r(m_geosContext);
}

#endif

bool GDALVectorCheckGeometryAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
#ifdef HAVE_GEOS
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    const bool bSingleLayerOutput = m_inputLayerNames.empty()
                                        ? poSrcDS->GetLayerCount() == 1
                                        : m_inputLayerNames.size() == 1;

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);
    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_inputLayerNames.empty() ||
            std::find(m_inputLayerNames.begin(), m_inputLayerNames.end(),
                      poSrcLayer->GetDescription()) != m_inputLayerNames.end())
        {
            const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
            if (poSrcLayerDefn->GetGeomFieldCount() == 0)
            {
                if (m_inputLayerNames.empty())
                    continue;
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Specified layer '%s' has no geometry field",
                            poSrcLayer->GetDescription());
                return false;
            }

            const int geomFieldIndex =
                m_geomField.empty()
                    ? 0
                    : poSrcLayerDefn->GetGeomFieldIndex(m_geomField.c_str());

            if (geomFieldIndex == -1)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Specified geometry field '%s' does not exist in "
                            "layer '%s'",
                            m_geomField.c_str(), poSrcLayer->GetDescription());
                return false;
            }

            std::vector<int> includeFieldIndices;
            if (!GetFieldIndices(m_includeFields,
                                 OGRLayer::ToHandle(poSrcLayer),
                                 includeFieldIndices))
            {
                return false;
            }

            outDS->AddLayer(*poSrcLayer,
                            std::make_unique<GDALInvalidLocationLayer>(
                                *poSrcLayer, includeFieldIndices,
                                bSingleLayerOutput, geomFieldIndex,
                                !m_includeValid));
        }
    }

    m_outputDataset.Set(std::move(outDS));

    return true;
#else
    ReportError(CE_Failure, CPLE_AppDefined,
                "%s requires GDAL to be built against the GEOS library.", NAME);
    return false;
#endif
}

GDALVectorCheckGeometryAlgorithmStandalone::
    ~GDALVectorCheckGeometryAlgorithmStandalone() = default;

//! @endcond
