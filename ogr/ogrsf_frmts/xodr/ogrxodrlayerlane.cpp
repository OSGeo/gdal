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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(&m_poSRS);

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
                dissolvedPolygon->assignSpatialReference(&m_poSRS);
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
            tin->assignSpatialReference(&m_poSRS);
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