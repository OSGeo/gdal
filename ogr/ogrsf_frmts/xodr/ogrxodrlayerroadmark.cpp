/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of RoadMark layer.
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

OGRXODRLayerRoadMark::OGRXODRLayerRoadMark(
    const RoadElements &xodrRoadElements, const std::string &proj4Defn,
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
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(&m_oSRS);

    OGRFieldDefn oFieldRoadID("RoadID", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldRoadID);

    OGRFieldDefn oFieldLaneID("LaneID", OFTInteger);
    m_poFeatureDefn->AddFieldDefn(&oFieldLaneID);

    OGRFieldDefn oFieldType("Type", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldType);
}

int OGRXODRLayerRoadMark::TestCapability(const char *pszCap)
{
    int result = FALSE;

    if (EQUAL(pszCap, OLCZGeometries))
        result = TRUE;

    return result;
}

OGRFeature *OGRXODRLayerRoadMark::GetNextRawFeature()
{
    std::unique_ptr<OGRFeature> feature;

    if (m_roadMarkIter != m_roadElements.roadMarks.end())
    {
        feature = std::make_unique<OGRFeature>(m_poFeatureDefn.get());

        odr::RoadMark roadMark = *m_roadMarkIter;
        odr::Mesh3D roadMarkMesh = *m_roadMarkMeshIter;

        // Populate geometry field
        std::unique_ptr<OGRTriangulatedSurface> tin =
            triangulateSurface(roadMarkMesh);
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
                         "RoadMark feature with FID %d has no geometry because "
                         "its triangulated surface could not be dissolved.",
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
        feature->SetField(m_poFeatureDefn->GetFieldIndex("RoadID"),
                          roadMark.road_id.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("LaneID"),
                          roadMark.lane_id);
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Type"),
                          roadMark.type.c_str());
        feature->SetFID(m_nNextFID++);

        ++m_roadMarkIter;
        ++m_roadMarkMeshIter;
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
