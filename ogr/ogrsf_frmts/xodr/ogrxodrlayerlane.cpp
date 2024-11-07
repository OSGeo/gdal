/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of Lane layer.
 * Author:   Michael Scholz, German Aerospace Center (DLR)
 *           Gülsen Bardak, German Aerospace Center (DLR)
 *
 ******************************************************************************
 * Copyright 2024 German Aerospace Center (DLR), Institute of Transportation Systems
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogr_xodr.h"

OGRXODRLayerLane::OGRXODRLayerLane(const RoadElements &xodrRoadElements,
                                   const std::string &proj4Defn,
                                   const bool dissolveTriangulatedSurface)
    : OGRXODRLayer(xodrRoadElements, proj4Defn, dissolveTriangulatedSurface)
{
    m_poFeatureDefn =
        std::make_unique<OGRFeatureDefn>(FEATURE_CLASS_NAME.c_str());
    m_poFeatureDefn->Reference();
    SetDescription(FEATURE_CLASS_NAME.c_str());

    if (m_bDissolveTIN)
    {
        OGRwkbGeometryType wkbPolygonWithZ = OGR_GT_SetZ(wkbPolygon);
        m_poFeatureDefn->SetGeomType(wkbPolygonWithZ);
    }
    else
    {
        m_poFeatureDefn->SetGeomType(wkbTINZ);
    }
    if (!m_oSRS.IsEmpty())
    {
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(&m_oSRS);
    }

    OGRFieldDefn oFieldLaneID("LaneID", OFTInteger);
    m_poFeatureDefn->AddFieldDefn(&oFieldLaneID);

    OGRFieldDefn oFieldRoadID("RoadID", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldRoadID);

    OGRFieldDefn oFieldType("Type", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldType);

    OGRFieldDefn oFieldPred("Predecessor", OFTInteger);
    m_poFeatureDefn->AddFieldDefn(&oFieldPred);

    OGRFieldDefn oFieldSuc("Successor", OFTInteger);
    m_poFeatureDefn->AddFieldDefn(&oFieldSuc);
}

int OGRXODRLayerLane::TestCapability(const char *pszCap)
{
    int result = FALSE;

    if (EQUAL(pszCap, OLCZGeometries))
        result = TRUE;

    return result;
}

OGRFeature *OGRXODRLayerLane::GetNextRawFeature()
{
    std::unique_ptr<OGRFeature> feature;

    while (m_laneIter != m_roadElements.lanes.end() && (*m_laneIter).id == 0)
    {
        // Skip lane(s) with id 0 because these "center lanes" don't have any width
        ++m_laneIter;
        ++m_laneMeshIter;
        ++m_laneRoadIDIter;
    }

    if (m_laneIter != m_roadElements.lanes.end())
    {

        feature = std::make_unique<OGRFeature>(m_poFeatureDefn.get());

        odr::Lane lane = *m_laneIter;
        odr::Mesh3D laneMesh = *m_laneMeshIter;
        std::string laneRoadID = *m_laneRoadIDIter;

        // Populate geometry field
        std::unique_ptr<OGRTriangulatedSurface> tin =
            triangulateSurface(laneMesh);
        if (m_bDissolveTIN)
        {
            OGRGeometry *dissolvedPolygon = tin->UnaryUnion();
            if (dissolvedPolygon != nullptr)
            {
                if (!m_oSRS.IsEmpty())
                    dissolvedPolygon->assignSpatialReference(&m_oSRS);
                feature->SetGeometryDirectly(dissolvedPolygon);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Lane feature with FID %d has no geometry because its "
                         "triangulated surface could not be dissolved.",
                         m_nNextFID);
            }
        }
        else
        {
            if (!m_oSRS.IsEmpty())
                tin->assignSpatialReference(&m_oSRS);
            feature->SetGeometryDirectly(tin.release());
        }

        // Populate other fields
        feature->SetFID(m_nNextFID++);
        feature->SetField(m_poFeatureDefn->GetFieldIndex("RoadID"),
                          laneRoadID.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("LaneID"), lane.id);
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Type"),
                          lane.type.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Predecessor"),
                          lane.predecessor);
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Successor"),
                          lane.successor);

        ++m_laneIter;
        ++m_laneMeshIter;
        ++m_laneRoadIDIter;
    }

    if (feature)
    {
        return feature.release();
    }
    else
    {
        // End of features for the given layer reached.
        return nullptr;
    }
}
