/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of OGRXODRDataSource.
 * Author:   Michael Scholz, German Aerospace Center (DLR)
 *           Gülsen Bardak, German Aerospace Center (DLR)
 *
 ******************************************************************************
 * Copyright 2024 German Aerospace Center (DLR), Institute of Transportation Systems
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_xodr.h"
using namespace odr;
using namespace pugi;
using namespace std;

bool OGRXODRDataSource::Open(const char *pszFilename, CSLConstList openOptions)
{
    odr::OpenDriveMap xodr(pszFilename, false);
    pugi::xml_parse_result parse_result = xodr.xml_parse_result;
    if (!parse_result ||
        parse_result.status != pugi::xml_parse_status::status_ok)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OpenDRIVE dataset %s could not be parsed: %s.", pszFilename,
                 parse_result.description());
        return FALSE;
    }
    bool parsingFailed = xodr.xml_doc.child("OpenDRIVE").empty();
    if (parsingFailed)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The provided file does not contain any OpenDRIVE data. Is it "
                 "empty?");
        return false;
    }

    std::vector<odr::Road> roads = xodr.get_roads();
    if (roads.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OpenDRIVE dataset does not contain any roads.");
        return false;
    }

    const char *openOptionValue = CSLFetchNameValue(openOptions, "EPSILON");
    if (openOptionValue != nullptr)
    {
        double dfEpsilon = CPLAtof(openOptionValue);
        if (dfEpsilon > 0.0)
        {
            m_dfEpsilon = dfEpsilon;
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Invalid value for EPSILON specified. Falling back to "
                     "default of 1.0.");
        }
    }
    openOptionValue = CSLFetchNameValueDef(openOptions, "DISSOLVE_TIN", "NO");
    bool bDissolveTIN = CPLTestBool(openOptionValue);

    RoadElements roadElements = createRoadElements(roads);
    std::string &proj4Defn = xodr.proj4;

    auto refLine =
        std::make_unique<OGRXODRLayerReferenceLine>(roadElements, proj4Defn);
    auto laneBorder =
        std::make_unique<OGRXODRLayerLaneBorder>(roadElements, proj4Defn);
    auto roadMark = std::make_unique<OGRXODRLayerRoadMark>(
        roadElements, proj4Defn, bDissolveTIN);
    auto roadObject =
        std::make_unique<OGRXODRLayerRoadObject>(roadElements, proj4Defn);
    auto lane = std::make_unique<OGRXODRLayerLane>(roadElements, proj4Defn,
                                                   bDissolveTIN);
    auto roadSignal = std::make_unique<OGRXODRLayerRoadSignal>(
        roadElements, proj4Defn, bDissolveTIN);

    m_apoLayers.push_back(std::move(refLine));
    m_apoLayers.push_back(std::move(laneBorder));
    m_apoLayers.push_back(std::move(roadMark));
    m_apoLayers.push_back(std::move(roadObject));
    m_apoLayers.push_back(std::move(lane));
    m_apoLayers.push_back(std::move(roadSignal));

    return true;
}

OGRLayer *OGRXODRDataSource::GetLayer(int iLayer)
{
    if (iLayer < 0 || static_cast<size_t>(iLayer) >= m_apoLayers.size())
        return nullptr;

    return m_apoLayers[iLayer].get();
}

int OGRXODRDataSource::TestCapability(const char *pszCap)
{
    int result = FALSE;

    if (EQUAL(pszCap, ODsCZGeometries))
        result = TRUE;

    return result;
}

RoadElements
OGRXODRDataSource::createRoadElements(const std::vector<odr::Road> &roads)
{
    RoadElements elements;

    for (const odr::Road &road : roads)
    {
        elements.roads.insert({road.id, road});

        odr::Line3D referenceLine =
            road.ref_line.get_line(0.0, road.length, m_dfEpsilon);
        elements.referenceLines.push_back(referenceLine);

        for (const odr::LaneSection &laneSection : road.get_lanesections())
        {
            elements.laneSections.push_back(laneSection);

            for (const odr::Lane &lane : laneSection.get_lanes())
            {
                elements.laneRoadIDs.push_back(road.id);

                elements.lanes.push_back(lane);

                odr::Mesh3D laneMesh = road.get_lane_mesh(lane, m_dfEpsilon);
                elements.laneMeshes.push_back(laneMesh);

                odr::Line3D laneLineOuter =
                    road.get_lane_border_line(lane, m_dfEpsilon, true);
                elements.laneLinesOuter.push_back(laneLineOuter);

                odr::Line3D laneLineInner =
                    road.get_lane_border_line(lane, m_dfEpsilon, false);
                elements.laneLinesInner.push_back(laneLineInner);

                const double sectionStart = laneSection.s0;
                const double sectionEnd = road.get_lanesection_end(laneSection);
                for (const odr::RoadMark &roadMark :
                     lane.get_roadmarks(sectionStart, sectionEnd))
                {
                    elements.roadMarks.push_back(roadMark);

                    odr::Mesh3D roadMarkMesh =
                        road.get_roadmark_mesh(lane, roadMark, m_dfEpsilon);
                    elements.roadMarkMeshes.push_back(roadMarkMesh);
                }
            }
        }

        for (const odr::RoadObject &roadObject : road.get_road_objects())
        {
            elements.roadObjects.push_back(roadObject);

            odr::Mesh3D roadObjectMesh =
                road.get_road_object_mesh(roadObject, m_dfEpsilon);
            elements.roadObjectMeshes.push_back(roadObjectMesh);
        }

        for (const odr::RoadSignal &roadSignal : road.get_road_signals())
        {
            elements.roadSignals.push_back(roadSignal);

            odr::Mesh3D roadSignalMesh = road.get_road_signal_mesh(roadSignal);
            elements.roadSignalMeshes.push_back(roadSignalMesh);
        }
    }
    return elements;
}
