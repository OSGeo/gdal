/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  KML driver utilities
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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
#ifndef OGR_KMLUTILITY_H_INCLUDED
#define OGR_KMLUTILITY_H_INCLUDED

#include <string>
#include <vector>
#include "ogr_geometry.h"

namespace OGRKML
{

enum Nodetype
{
    Unknown, Empty, Mixed, Point, LineString, Polygon, Rest, MultiGeometry,
    MultiPoint, MultiLineString, MultiPolygon
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
    bool   bHasZ;

    Coordinate() :
        dfLongitude(0),
        dfLatitude(0),
        dfAltitude(0),
        bHasZ(false)
    {}
};

struct Feature
{
    Nodetype eType;
    std::string sName;
    std::string sDescription;
    OGRGeometry* poGeom;

    Feature()
        : eType(Unknown), poGeom(nullptr)
    {}

    ~Feature()
    {
        delete poGeom;
    }
};

}

using namespace OGRKML;

#endif /* OGR_KMLUTILITY_H_INCLUDED */

