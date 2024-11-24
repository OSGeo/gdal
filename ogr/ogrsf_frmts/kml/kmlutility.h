/******************************************************************************
 *
 * Project:  KML Driver
 * Purpose:  KML driver utilities
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef OGR_KMLUTILITY_H_INCLUDED
#define OGR_KMLUTILITY_H_INCLUDED

#include <string>
#include <vector>
#include "ogr_geometry.h"

namespace OGRKML
{

enum Nodetype
{
    Unknown,
    Empty,
    Mixed,
    Point,
    LineString,
    Polygon,
    Rest,
    MultiGeometry,
    MultiPoint,
    MultiLineString,
    MultiPolygon
};

struct Attribute
{
    std::string sName;
    std::string sValue;
};

struct Coordinate
{
    double dfLongitude;
    double dfLatitude;
    double dfAltitude;
    bool bHasZ;

    Coordinate() : dfLongitude(0), dfLatitude(0), dfAltitude(0), bHasZ(false)
    {
    }
};

struct Feature
{
    Nodetype eType;
    std::string sName;
    std::string sDescription;
    OGRGeometry *poGeom;

    Feature() : eType(Unknown), poGeom(nullptr)
    {
    }

    ~Feature()
    {
        delete poGeom;
    }
};

}  // namespace OGRKML

using namespace OGRKML;

#endif /* OGR_KMLUTILITY_H_INCLUDED */
