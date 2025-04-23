/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of RoadSignal layer.
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

OGRXODRLayerRoadSignal::OGRXODRLayerRoadSignal(
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
        OGRwkbGeometryType wkbPointWithZ = OGR_GT_SetZ(wkbPoint);
        m_poFeatureDefn->SetGeomType(wkbPointWithZ);
    }
    else
    {
        m_poFeatureDefn->SetGeomType(wkbTINZ);
    }
    if (!m_oSRS.IsEmpty())
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(&m_oSRS);

    OGRFieldDefn oFieldSignalID("SignalID", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldSignalID);

    OGRFieldDefn oFieldRoadID("RoadID", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldRoadID);

    OGRFieldDefn oFieldType("Type", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldType);

    OGRFieldDefn oFieldSubType("SubType", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldSubType);

    OGRFieldDefn oFieldHOffset("HOffset", OFTReal);
    m_poFeatureDefn->AddFieldDefn(&oFieldHOffset);

    OGRFieldDefn oFieldPitch("Pitch", OFTReal);
    m_poFeatureDefn->AddFieldDefn(&oFieldPitch);

    OGRFieldDefn oFieldRoll("Roll", OFTReal);
    m_poFeatureDefn->AddFieldDefn(&oFieldRoll);

    OGRFieldDefn oFieldOrientation("Orientation", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldOrientation);

    OGRFieldDefn oFieldName("Name", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldName);

    OGRFieldDefn oFieldObjectDynamic("Dynamic", OFTInteger);
    oFieldObjectDynamic.SetSubType(OFSTBoolean);
    m_poFeatureDefn->AddFieldDefn(&oFieldObjectDynamic);
}

int OGRXODRLayerRoadSignal::TestCapability(const char *pszCap)
{
    int result = FALSE;

    if (EQUAL(pszCap, OLCZGeometries))
        result = TRUE;

    return result;
}

OGRFeature *OGRXODRLayerRoadSignal::GetNextRawFeature()
{
    std::unique_ptr<OGRFeature> feature;

    if (m_roadSignalIter != m_roadElements.roadSignals.end())
    {
        feature = std::make_unique<OGRFeature>(m_poFeatureDefn.get());

        odr::RoadSignal roadSignal = *m_roadSignalIter;
        odr::Mesh3D roadSignalMesh = *m_roadSignalMeshesIter;

        // Populate geometry field
        if (m_bDissolveTIN)
        {
            // Use simplified centroid, directly provided by libOpenDRIVE
            std::string roadId = roadSignal.road_id;
            odr::Road road = m_roadElements.roads.at(roadId);

            double s = roadSignal.s0;
            double t = roadSignal.t0;
            double h = roadSignal.zOffset;
            odr::Vec3D xyz = road.get_xyz(s, t, h);

            auto point = std::make_unique<OGRPoint>(xyz[0], xyz[1], xyz[2]);
            if (!m_oSRS.IsEmpty())
                point->assignSpatialReference(&m_oSRS);
            feature->SetGeometryDirectly(point.release());
        }
        else
        {
            std::unique_ptr<OGRTriangulatedSurface> tin =
                triangulateSurface(roadSignalMesh);
            if (!m_oSRS.IsEmpty())
                tin->assignSpatialReference(&m_oSRS);
            feature->SetGeometryDirectly(tin.release());
        }

        // Populate other fields
        feature->SetField(m_poFeatureDefn->GetFieldIndex("SignalID"),
                          roadSignal.id.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("RoadID"),
                          roadSignal.road_id.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Type"),
                          roadSignal.type.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("SubType"),
                          roadSignal.subtype.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("HOffset"),
                          roadSignal.hOffset);
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Pitch"),
                          roadSignal.pitch);
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Roll"),
                          roadSignal.roll);
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Orientation"),
                          roadSignal.orientation.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Name"),
                          roadSignal.name.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Dynamic"),
                          roadSignal.is_dynamic);
        feature->SetFID(m_nNextFID++);

        ++m_roadSignalIter;
        ++m_roadSignalMeshesIter;
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
