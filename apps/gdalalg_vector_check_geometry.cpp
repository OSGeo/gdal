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
    AddArg("include-valid", 0,
           _("Include valid inputs in output, with empty geometry"),
           &m_includeValid);

    AddArg("geometry-field", 0, _("Name or geometry field to check"),
           &m_geomField);
}

#ifdef HAVE_GEOS

class GDALInvalidLocationLayer : public GDALVectorPipelineOutputLayer
{
  private:
    static constexpr const char *ERROR_DESCRIPTION_FIELD = "error";

  public:
    GDALInvalidLocationLayer(OGRLayer &layer, int srcGeomField, bool skipValid)
        : GDALVectorPipelineOutputLayer(layer),
          m_defn(OGRFeatureDefn::CreateFeatureDefn("error_location")),
          m_geosContext(OGRGeometry::createGEOSContext()),
          m_srcGeomField(srcGeomField), m_skipValid(skipValid)
    {
        m_defn->Reference();

        auto poDescriptionFieldDefn =
            std::make_unique<OGRFieldDefn>(ERROR_DESCRIPTION_FIELD, OFTString);
        m_defn->AddFieldDefn(std::move(poDescriptionFieldDefn));

        m_defn->GetGeomFieldDefn(0)->SetSpatialRef(
            m_srcLayer.GetLayerDefn()
                ->GetGeomFieldDefn(m_srcGeomField)
                ->GetSpatialRef());
    }

    ~GDALInvalidLocationLayer() override;

    int TestCapability(const char *) override
    {
        return false;
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_defn;
    }

    std::unique_ptr<OGRFeature> CreateFeatureFromLastError()
    {
        auto poErrorFeature = std::make_unique<OGRFeature>(m_defn);

        const char *msg = CPLGetLastErrorMsg();

        // Trim GEOS exception name
        const char *subMsg = strstr(msg, ": ");
        if (subMsg != nullptr)
        {
            msg = subMsg + 2;
        }

        // Trim newline from end of GEOS exception message
        if (msg[strlen(msg) - 1] == '\n')
        {
            const_cast<char *>(msg)[strlen(msg) - 1] = '\0';
        }

        poErrorFeature->SetField(ERROR_DESCRIPTION_FIELD, msg);

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
            auto eType = wkbFlatten(poGeom->getGeometryType());

            if (eType != wkbPolygon && eType != wkbMultiPolygon)
            {
                CPLErrorOnce(CE_Warning, CPLE_AppDefined,
                             "Non-polygonal geometry passed to 'gdal vector "
                             "check-geometry'. Non-polygonal geometries are "
                             "always valid.");
            }
            else
            {
                GEOSGeometry *poGeosGeom = poGeom->exportToGEOS(m_geosContext);

                if (!poGeosGeom)
                {
                    poErrorFeature = CreateFeatureFromLastError();

                    if (eType == wkbPolygon)
                    {
                        const OGRLinearRing *poRing =
                            cpl::down_cast<const OGRPolygon *>(poGeom)
                                ->getExteriorRing();
                        if (poRing != nullptr && !poRing->IsEmpty())
                        {
                            auto poPoint = std::make_unique<OGRPoint>();
                            poRing->StartPoint(poPoint.get());
                            poErrorFeature->SetGeometry(std::move(poPoint));
                        }
                        else
                        {
                            // TODO get a point from somewhere else?
                        }
                    }
                }
                else
                {
                    char *pszReason;
                    GEOSGeometry *location;
                    auto ret = GEOSisValidDetail_r(m_geosContext, poGeosGeom, 0,
                                                   &pszReason, &location);
                    GEOSGeom_destroy_r(m_geosContext, poGeosGeom);
                    if (ret == 0)
                    {
                        poErrorFeature = std::make_unique<OGRFeature>(m_defn);
                        poErrorFeature->SetField(ERROR_DESCRIPTION_FIELD,
                                                 pszReason);
                        GEOSFree_r(m_geosContext, pszReason);

                        std::unique_ptr<OGRGeometry> poErrorGeom(
                            OGRGeometryFactory::createFromGEOS(m_geosContext,
                                                               location));
                        GEOSGeom_destroy_r(m_geosContext, location);

                        poErrorGeom->assignSpatialReference(
                            m_srcLayer.GetLayerDefn()
                                ->GetGeomFieldDefn(m_srcGeomField)
                                ->GetSpatialRef());

                        poErrorFeature->SetGeometry(std::move(poErrorGeom));
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
            apoOutputFeatures.push_back(std::move(poErrorFeature));
        }
    }

    CPL_DISALLOW_COPY_ASSIGN(GDALInvalidLocationLayer)

  private:
    OGRFeatureDefn *m_defn;
    GEOSContextHandle_t m_geosContext;
    int m_srcGeomField;
    bool m_skipValid;
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

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);
    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_inputLayerNames.empty() ||
            std::find(m_inputLayerNames.begin(), m_inputLayerNames.end(),
                      poSrcLayer->GetDescription()) != m_inputLayerNames.end())
        {
            int geomFieldIndex = 0;

            if (!m_geomField.empty())
            {
                geomFieldIndex = -1;
                const OGRFeatureDefn *defn = poSrcLayer->GetLayerDefn();
                for (int i = 0; i < defn->GetGeomFieldCount(); i++)
                {
                    if (EQUAL(defn->GetGeomFieldDefn(i)->GetNameRef(),
                              m_geomField.c_str()))
                    {
                        geomFieldIndex = i;
                        break;
                    }
                }
            }

            if (geomFieldIndex == -1)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Specified geometry field '%s' does not exist in "
                            "layer '%s'",
                            m_geomField.c_str(), poSrcLayer->GetDescription());
                return false;
            }

            outDS->AddLayer(*poSrcLayer,
                            std::make_unique<GDALInvalidLocationLayer>(
                                *poSrcLayer, geomFieldIndex, !m_includeValid));
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
