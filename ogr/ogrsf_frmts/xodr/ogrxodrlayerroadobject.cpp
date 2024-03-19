/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of RoadObject layer.
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

OGRXODRLayerRoadObject::OGRXODRLayerRoadObject(RoadElements xodrRoadElements,
                                               std::string proj4Defn)
    : OGRXODRLayer(xodrRoadElements, proj4Defn)
{
    this->featureDefn = new OGRFeatureDefn(FEATURE_CLASS_NAME.c_str());
    SetDescription(FEATURE_CLASS_NAME.c_str());
    featureDefn->Reference();
    featureDefn->GetGeomFieldDefn(0)->SetSpatialRef(&spatialRef);

    defineFeatureClass();
}

OGRFeature *OGRXODRLayerRoadObject::GetNextFeature()
{
    std::unique_ptr<OGRFeature> feature;

    if (roadObjectIter != roadElements.roadObjects.end())
    {
        feature = std::unique_ptr<OGRFeature>(new OGRFeature(featureDefn));

        odr::RoadObject roadObject = *roadObjectIter;
        odr::Mesh3D roadObjectMesh = *roadObjectMeshesIter;

        OGRTriangulatedSurface tin = triangulateSurface(roadObjectMesh);
        //tin.MakeValid(); // TODO Works for TINs only with enabled SFCGAL support
        feature->SetGeometry(&tin);

        feature->SetField(featureDefn->GetFieldIndex("ObjectID"),
                          roadObject.id.c_str());
        feature->SetField(featureDefn->GetFieldIndex("RoadID"),
                          roadObject.road_id.c_str());
        feature->SetField(featureDefn->GetFieldIndex("Type"),
                          roadObject.type.c_str());
        feature->SetField(featureDefn->GetFieldIndex("Name"),
                          roadObject.name.c_str());
        feature->SetFID(nNextFID++);

        roadObjectIter++;
        roadObjectMeshesIter++;
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

void OGRXODRLayerRoadObject::defineFeatureClass()
{
    featureDefn->SetGeomType(wkbTINZ);

    OGRFieldDefn oFieldObjectID("ObjectID", OFTString);
    featureDefn->AddFieldDefn(&oFieldObjectID);

    OGRFieldDefn oFieldRoadID("RoadID", OFTString);
    featureDefn->AddFieldDefn(&oFieldRoadID);

    OGRFieldDefn oFieldType("Type", OFTString);
    featureDefn->AddFieldDefn(&oFieldType);

    OGRFieldDefn oFieldObjectName("Name", OFTString);
    featureDefn->AddFieldDefn(&oFieldObjectName);
}