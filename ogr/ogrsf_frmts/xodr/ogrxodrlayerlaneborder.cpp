/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of LaneBorder layer.
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

OGRXODRLayerLaneBorder::OGRXODRLayerLaneBorder(
    const RoadElements &xodrRoadElements, const std::string &proj4Defn)
    : OGRXODRLayer(xodrRoadElements, proj4Defn)
{
    m_poFeatureDefn =
        std::make_unique<OGRFeatureDefn>(FEATURE_CLASS_NAME.c_str());
    m_poFeatureDefn->Reference();
    SetDescription(FEATURE_CLASS_NAME.c_str());

    OGRwkbGeometryType wkbLineStringWithZ = OGR_GT_SetZ(wkbLineString);
    m_poFeatureDefn->SetGeomType(wkbLineStringWithZ);
    if (!m_oSRS.IsEmpty())
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(&m_oSRS);

    OGRFieldDefn oFieldID("ID", OFTInteger);
    m_poFeatureDefn->AddFieldDefn(&oFieldID);

    OGRFieldDefn oFieldRoadID("RoadID", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldRoadID);

    OGRFieldDefn oFieldType("Type", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldType);

    OGRFieldDefn oFieldPred("Predecessor", OFTInteger);
    m_poFeatureDefn->AddFieldDefn(&oFieldPred);

    OGRFieldDefn oFieldSuc("Successor", OFTInteger);
    m_poFeatureDefn->AddFieldDefn(&oFieldSuc);
}

int OGRXODRLayerLaneBorder::TestCapability(const char *pszCap)
{
    int result = FALSE;

    if (EQUAL(pszCap, OLCZGeometries))
        result = TRUE;

    return result;
}

OGRFeature *OGRXODRLayerLaneBorder::GetNextRawFeature()
{
    std::unique_ptr<OGRFeature> feature;

    if (m_laneIter != m_roadElements.lanes.end())
    {
        feature = std::make_unique<OGRFeature>(m_poFeatureDefn.get());

        odr::Lane lane = *m_laneIter;
        odr::Line3D laneOuterBorder = *m_laneLinesOuterIter;
        std::string laneRoadID = *m_laneRoadIDIter;

        // Populate geometry field
        auto lineString = std::make_unique<OGRLineString>();
        for (const auto &borderVertex : laneOuterBorder)
        {
            lineString->addPoint(borderVertex[0], borderVertex[1],
                                 borderVertex[2]);
        }
        if (!m_oSRS.IsEmpty())
            lineString->assignSpatialReference(&m_oSRS);
        feature->SetGeometryDirectly(lineString.release());

        // Populate other fields
        feature->SetField(m_poFeatureDefn->GetFieldIndex("RoadID"),
                          laneRoadID.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("ID"), lane.id);
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Type"),
                          lane.type.c_str());
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Predecessor"),
                          lane.predecessor);
        feature->SetField(m_poFeatureDefn->GetFieldIndex("Successor"),
                          lane.successor);
        feature->SetFID(m_nNextFID++);

        ++m_laneIter;
        ++m_laneLinesOuterIter;
        ++m_laneLinesInnerIter;  // For consistency, even though not used here
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
