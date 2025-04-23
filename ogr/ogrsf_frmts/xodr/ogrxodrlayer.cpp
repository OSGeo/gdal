/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of OGRXODRLayer.
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

#include "cpl_error.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

OGRXODRLayer::OGRXODRLayer(const RoadElements &xodrRoadElements,
                           const std::string &proj4Defn)
    : OGRXODRLayer(xodrRoadElements, proj4Defn, false)
{
}

OGRXODRLayer::OGRXODRLayer(const RoadElements &xodrRoadElements,
                           const std::string &proj4Defn,
                           const bool dissolveTriangulatedSurface)
    : m_roadElements(xodrRoadElements),
      m_bDissolveTIN(dissolveTriangulatedSurface)
{
    if (!proj4Defn.empty())
    {
        m_oSRS.importFromProj4(proj4Defn.c_str());
    }
    resetRoadElementIterators();
}

void OGRXODRLayer::ResetReading()
{
    m_nNextFID = 0;
    resetRoadElementIterators();
}

void OGRXODRLayer::resetRoadElementIterators()
{
    m_roadIter = m_roadElements.roads.begin();
    m_referenceLineIter = m_roadElements.referenceLines.begin();

    m_laneIter = m_roadElements.lanes.begin();
    m_laneSectionIter = m_roadElements.laneSections.begin();
    m_laneRoadIDIter = m_roadElements.laneRoadIDs.begin();
    m_laneMeshIter = m_roadElements.laneMeshes.begin();

    m_laneLinesInnerIter = m_roadElements.laneLinesInner.begin();
    m_laneLinesOuterIter = m_roadElements.laneLinesOuter.begin();

    m_roadMarkIter = m_roadElements.roadMarks.begin();
    m_roadMarkMeshIter = m_roadElements.roadMarkMeshes.begin();

    m_roadObjectIter = m_roadElements.roadObjects.begin();
    m_roadObjectMeshesIter = m_roadElements.roadObjectMeshes.begin();

    m_roadSignalIter = m_roadElements.roadSignals.begin();
    m_roadSignalMeshesIter = m_roadElements.roadSignalMeshes.begin();
}

std::unique_ptr<OGRTriangulatedSurface>
OGRXODRLayer::triangulateSurface(const odr::Mesh3D &mesh)
{
    std::vector<odr::Vec3D> meshVertices = mesh.vertices;
    std::vector<uint32_t> meshIndices = mesh.indices;

    auto tin = std::make_unique<OGRTriangulatedSurface>();
    const size_t numIndices = meshIndices.size();
    // Build triangles from mesh vertices.
    // Each triple of mesh indices defines which vertices form a triangle.
    for (std::size_t idx = 0; idx < numIndices; idx += 3)
    {
        uint32_t vertexIdx = meshIndices[idx];
        odr::Vec3D vertexP = meshVertices[vertexIdx];
        OGRPoint p(vertexP[0], vertexP[1], vertexP[2]);

        vertexIdx = meshIndices[idx + 1];
        odr::Vec3D vertexQ = meshVertices[vertexIdx];
        OGRPoint q(vertexQ[0], vertexQ[1], vertexQ[2]);

        vertexIdx = meshIndices[idx + 2];
        odr::Vec3D vertexR = meshVertices[vertexIdx];
        OGRPoint r(vertexR[0], vertexR[1], vertexR[2]);

        OGRTriangle triangle(p, q, r);
        tin->addGeometry(&triangle);
    }

    return tin;
}
