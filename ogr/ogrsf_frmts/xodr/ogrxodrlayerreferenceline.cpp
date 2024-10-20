/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of ReferenceLine layer.
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

OGRXODRLayerReferenceLine::OGRXODRLayerReferenceLine(
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

    OGRFieldDefn oFieldID("ID", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldID);

    OGRFieldDefn oFieldLen("Length", OFTReal);
    m_poFeatureDefn->AddFieldDefn(&oFieldLen);

    OGRFieldDefn oFieldJunction("Junction", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oFieldJunction);
}

int OGRXODRLayerReferenceLine::TestCapability(const char *pszCap)
{
    int result = FALSE;

    if (EQUAL(pszCap, OLCZGeometries))
        result = TRUE;

    return result;
}

OGRFeature *OGRXODRLayerReferenceLine::GetNextRawFeature()
{
    std::unique_ptr<OGRFeature> feature;

    if (m_roadIter != m_roadElements.roads.end())
    {
        feature = std::make_unique<OGRFeature>(m_poFeatureDefn.get());

        odr::Road road = (*m_roadIter).second;
        odr::Line3D refLine = *m_referenceLineIter;

        // Populate geometry field
        auto lineString = std::make_unique<OGRLineString>();
        for (const auto &lineVertex : refLine)
        {
            lineString->addPoint(lineVertex[0], lineVertex[1], lineVertex[2]);
        }
        if (!m_oSRS.IsEmpty())
            lineString->assignSpatialReference(&m_oSRS);
        feature->SetGeometryDirectly(lineString.release());

        // Populate other fields
        feature->SetField("ID", road.id.c_str());
        feature->SetField("Length", road.length);
        feature->SetField("Junction", road.junction.c_str());
        feature->SetFID(m_nNextFID++);

        ++m_roadIter;
        ++m_referenceLineIter;
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
