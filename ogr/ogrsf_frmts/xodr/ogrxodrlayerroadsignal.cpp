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
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(&m_poSRS);

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
            point->assignSpatialReference(&m_poSRS);
            feature->SetGeometryDirectly(point.release());
        }
        else
        {
            std::unique_ptr<OGRTriangulatedSurface> tin =
                triangulateSurface(roadSignalMesh);
            tin->assignSpatialReference(&m_poSRS);
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