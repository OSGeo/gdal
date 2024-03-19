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

OGRXODRLayerRoadSignal::OGRXODRLayerRoadSignal(RoadElements xodrRoadElements,
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

OGRFeature *OGRXODRLayerRoadSignal::GetNextFeature()
{
    std::unique_ptr<OGRFeature> feature;

    if (roadSignalIter != roadElements.roadSignals.end())
    {
        feature = std::unique_ptr<OGRFeature>(new OGRFeature(featureDefn));

        odr::RoadSignal roadSignal = *roadSignalIter;
        odr::Mesh3D roadSignalMesh = *roadSignalMeshesIter;

        if (dissolveTIN)
        {
            std::string roadId = roadSignal.road_id;
            odr::Road road = roadElements.roads.at(roadId);

            double s = roadSignal.s0;
            double t = roadSignal.t0;
            double z = roadSignal.zOffset;
            odr::Vec3D xyz = road.get_xyz(s, t, z);

            OGRPoint point(xyz[0], xyz[1], xyz[2]);
            OGRGeometry *geometry = point.MakeValid();
            feature->SetGeometry(geometry);
        }
        else
        {
            OGRTriangulatedSurface tin = triangulateSurface(roadSignalMesh);
            //tin.MakeValid(); // TODO Works for TINs only with enabled SFCGAL support
            feature->SetGeometry(&tin);
        }

        feature->SetField(featureDefn->GetFieldIndex("SignalID"),
                          roadSignal.id.c_str());
        feature->SetField(featureDefn->GetFieldIndex("RoadID"),
                          roadSignal.road_id.c_str());
        feature->SetField(featureDefn->GetFieldIndex("Type"),
                          roadSignal.type.c_str());
        feature->SetField(featureDefn->GetFieldIndex("SubType"),
                          roadSignal.subtype.c_str());
        feature->SetField(featureDefn->GetFieldIndex("HOffset"),
                          roadSignal.hOffset);
        feature->SetField(featureDefn->GetFieldIndex("Pitch"),
                          roadSignal.pitch);
        feature->SetField(featureDefn->GetFieldIndex("Roll"), roadSignal.roll);
        feature->SetField(featureDefn->GetFieldIndex("Orientation"),
                          roadSignal.orientation.c_str());
        feature->SetField(featureDefn->GetFieldIndex("Name"),
                          roadSignal.name.c_str());
        feature->SetField(featureDefn->GetFieldIndex("Dynamic"),
                          roadSignal.is_dynamic);
        feature->SetFID(nNextFID++);

        roadSignalIter++;
        roadSignalMeshesIter++;
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

void OGRXODRLayerRoadSignal::defineFeatureClass()
{
    if (dissolveTIN)
    {
        OGRwkbGeometryType wkbPointWithZ = OGR_GT_SetZ(wkbPoint);
        featureDefn->SetGeomType(wkbPointWithZ);
    }
    else
    {
        featureDefn->SetGeomType(wkbTINZ);
    }

    OGRFieldDefn oFieldSignalID("SignalID", OFTString);
    featureDefn->AddFieldDefn(&oFieldSignalID);

    OGRFieldDefn oFieldRoadID("RoadID", OFTString);
    featureDefn->AddFieldDefn(&oFieldRoadID);

    OGRFieldDefn oFieldType("Type", OFTString);
    featureDefn->AddFieldDefn(&oFieldType);

    OGRFieldDefn oFieldSubType("SubType", OFTString);
    featureDefn->AddFieldDefn(&oFieldSubType);

    OGRFieldDefn oFieldHOffset("HOffset", OFTReal);
    featureDefn->AddFieldDefn(&oFieldHOffset);

    OGRFieldDefn oFieldPitch("Pitch", OFTReal);
    featureDefn->AddFieldDefn(&oFieldPitch);

    OGRFieldDefn oFieldRoll("Roll", OFTReal);
    featureDefn->AddFieldDefn(&oFieldRoll);

    OGRFieldDefn oFieldOrientation("Orientation", OFTString);
    featureDefn->AddFieldDefn(&oFieldOrientation);

    OGRFieldDefn oFieldName("Name", OFTString);
    featureDefn->AddFieldDefn(&oFieldName);

    OGRFieldDefn oFieldObjectDynamic("Dynamic", OFTInteger);
    oFieldObjectDynamic.SetSubType(OFSTBoolean);
    featureDefn->AddFieldDefn(&oFieldObjectDynamic);
}