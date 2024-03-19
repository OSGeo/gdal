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

OGRXODRLayerLaneBorder::OGRXODRLayerLaneBorder(RoadElements xodrRoadElements,
                                               std::string proj4Defn)
    : OGRXODRLayer(xodrRoadElements, proj4Defn)
{
    this->featureDefn = new OGRFeatureDefn(FEATURE_CLASS_NAME.c_str());
    SetDescription(FEATURE_CLASS_NAME.c_str());
    featureDefn->Reference();
    featureDefn->GetGeomFieldDefn(0)->SetSpatialRef(&spatialRef);

    defineFeatureClass();
}

OGRFeature *OGRXODRLayerLaneBorder::GetNextFeature()
{
    std::unique_ptr<OGRFeature> feature;

    if (laneIter != roadElements.lanes.end())
    {
        feature = std::unique_ptr<OGRFeature>(new OGRFeature(featureDefn));

        odr::Lane lane = *laneIter;
        odr::Line3D laneOuter = *laneLinesOuterIter;
        std::string laneRoadID = *laneRoadIDIter;

        OGRLineString lineString;
        for (auto vertexIter = laneOuter.begin(); vertexIter != laneOuter.end();
             ++vertexIter)
        {
            odr::Vec3D laneVertex = *vertexIter;
            lineString.addPoint(laneVertex[0], laneVertex[1], laneVertex[2]);
        }
        OGRGeometry *geometry = lineString.MakeValid();

        feature->SetGeometry(geometry);
        feature->SetField(featureDefn->GetFieldIndex("RoadID"),
                          laneRoadID.c_str());
        feature->SetField(featureDefn->GetFieldIndex("ID"), lane.id);
        feature->SetField(featureDefn->GetFieldIndex("Type"),
                          lane.type.c_str());
        feature->SetField(featureDefn->GetFieldIndex("Predecessor"),
                          lane.predecessor);
        feature->SetField(featureDefn->GetFieldIndex("Successor"),
                          lane.successor);
        feature->SetFID(nNextFID++);

        laneIter++;
        laneLinesOuterIter++;
        laneLinesInnerIter++;  // For consistency, even though not used here
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

void OGRXODRLayerLaneBorder::defineFeatureClass()
{
    OGRwkbGeometryType wkbLineStringWithZ = OGR_GT_SetZ(wkbLineString);
    featureDefn->SetGeomType(wkbLineStringWithZ);

    OGRFieldDefn oFieldID("ID", OFTInteger);
    featureDefn->AddFieldDefn(&oFieldID);

    OGRFieldDefn oFieldRoadID("RoadID", OFTString);
    featureDefn->AddFieldDefn(&oFieldRoadID);

    OGRFieldDefn oFieldType("Type", OFTString);
    featureDefn->AddFieldDefn(&oFieldType);

    OGRFieldDefn oFieldPred("Predecessor", OFTInteger);
    featureDefn->AddFieldDefn(&oFieldPred);

    OGRFieldDefn oFieldSuc("Successor", OFTInteger);
    featureDefn->AddFieldDefn(&oFieldSuc);
}