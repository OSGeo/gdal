/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  KML driver utilities
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
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

enum Nodetype
{
    Unknown, Empty, Mixed, Point, LineString, Polygon, Rest
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

    Coordinate()
        : dfLongitude(0), dfLatitude(0), dfAltitude(0)
    {}
};

struct Feature
{
    Nodetype eType;
    std::string sName;
    std::string sDescription;
    std::vector<Coordinate*>* pvpsCoordinates;
    std::vector< std::vector<Coordinate*>* >* pvpsCoordinatesExtra;

    Feature()
        : eType(Unknown), pvpsCoordinates(NULL), pvpsCoordinatesExtra(NULL)
    {}
};

struct Extent
{
    double dfX1;
    double dfX2;
    double dfY1;
    double dfY2;

    Extent()
        : dfX1(0), dfX2(0), dfY1(0), dfY2(0)
    {}
};

#endif /* OGR_KMLUTILITY_H_INCLUDED */

