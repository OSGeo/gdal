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

OGRXODRLayerLane::OGRXODRLayerLane(RoadElements xodrRoadElements,
                                   std::string proj4Defn,
                                   bool dissolveTriangulatedSurface)
    : OGRXODRLayer(xodrRoadElements, proj4Defn, dissolveTriangulatedSurface)
{
    this->featureDefn = new OGRFeatureDefn(FEATURE_CLASS_NAME.c_str());
    SetDescription(FEATURE_CLASS_NAME.c_str());
    featureDefn->Reference();
    featureDefn->GetGeomFieldDefn(0)->SetSpatialRef(&spatialRef);

    defineFeatureClass();
}

OGRFeature *OGRXODRLayerLane::GetNextFeature()
{
    std::unique_ptr<OGRFeature> feature;

    while (laneIter != roadElements.lanes.end() && (*laneIter).id == 0)
    {
        // Skip lane(s) with id 0
        laneIter++;
        laneMeshIter++;
        laneRoadIDIter++;
    }

    if (laneIter != roadElements.lanes.end())
    {
        feature = std::unique_ptr<OGRFeature>(new OGRFeature(featureDefn));

        odr::Lane lane = *laneIter;
        odr::Mesh3D laneMesh = *laneMeshIter;

        std::string laneRoadID = *laneRoadIDIter;

        OGRTriangulatedSurface tin = triangulateSurface(laneMesh);

        if (dissolveTIN)
        {
            OGRGeometry *dissolvedPolygon = tin.UnaryUnion();
            feature->SetGeometry(dissolvedPolygon);
        }
        else
        {
            //tin.MakeValid(); // TODO Works for TINs only with enabled SFCGAL support
            feature->SetGeometry(&tin);
        }

        feature->SetFID(nNextFID++);
        feature->SetField(featureDefn->GetFieldIndex("RoadID"),
                          laneRoadID.c_str());
        feature->SetField(featureDefn->GetFieldIndex("LaneID"), lane.id);
        feature->SetField(featureDefn->GetFieldIndex("Type"),
                          lane.type.c_str());
        feature->SetField(featureDefn->GetFieldIndex("Predecessor"),
                          lane.predecessor);
        feature->SetField(featureDefn->GetFieldIndex("Successor"),
                          lane.successor);

        laneIter++;
        laneMeshIter++;
        laneRoadIDIter++;
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

void OGRXODRLayerLane::defineFeatureClass()
{
    if (dissolveTIN)
    {
        OGRwkbGeometryType wkbPolygonWithZ = OGR_GT_SetZ(wkbPolygon);
        featureDefn->SetGeomType(wkbPolygonWithZ);
    }
    else
    {
        featureDefn->SetGeomType(wkbTINZ);
    }

    OGRFieldDefn oFieldLaneID("LaneID", OFTInteger);
    featureDefn->AddFieldDefn(&oFieldLaneID);

    OGRFieldDefn oFieldRoadID("RoadID", OFTString);
    featureDefn->AddFieldDefn(&oFieldRoadID);

    OGRFieldDefn oFieldType("Type", OFTString);
    featureDefn->AddFieldDefn(&oFieldType);

    OGRFieldDefn oFieldPred("Predecessor", OFTInteger);
    featureDefn->AddFieldDefn(&oFieldPred);

    OGRFieldDefn oFieldSuc("Successor", OFTInteger);
    featureDefn->AddFieldDefn(&oFieldSuc);
}