/******************************************************************************
 * File:   ogrdxf_polyline_smooth.cpp
 *
 * Project:  Interpolation support for smooth POLYLINE and LWPOLYLINE entities.
 * Purpose:  Implementation of classes for OGR .dxf driver.
 * Author:   TJ Snider, timsn@thtree.com
 *           Ray Gardener, Daylon Graphics Ltd.
 *
 ******************************************************************************
 * Copyright (c) 2010 Daylon Graphics Ltd.
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

#include "stdlib.h"
#include "math.h"
#include "ogrdxf_polyline_smooth.h"


/************************************************************************/
/*                Local helper functions                                */
/************************************************************************/

static double GetRadius(double bulge, double length)
{
    const double h = (bulge * length) / 2;
    return (h / 2) + (length * length / (8 * h));
}


static double GetLength
(
    const DXFSmoothPolylineVertex& start, 
    const DXFSmoothPolylineVertex& end
)
{
    return sqrt(pow(end.x - start.x, 2) + pow(end.y - start.y, 2));
}


static double GetAngle
(
    const DXFSmoothPolylineVertex& start, 
    const DXFSmoothPolylineVertex& end
)
{
    return atan2((start.y - end.y), (start.x - end.x)) * 180.0 / M_PI;
}


static double GetOGRangle(double angle)
{
    return angle > 0.0
            ? -(angle - 180.0)
            : -(angle + 180.0);
}


/************************************************************************/
/*                DXFSmoothPolyline::Tesselate()                        */
/************************************************************************/

OGRGeometry* DXFSmoothPolyline::Tesselate() const
{
    assert(!m_vertices.empty());


/* -------------------------------------------------------------------- */
/*      If polyline is one vertex, convert it to a point                */
/* -------------------------------------------------------------------- */

    if(m_vertices.size() == 1)
        return new OGRPoint(m_vertices[0].x, m_vertices[0].y, m_vertices[0].z);


/* -------------------------------------------------------------------- */
/*      Otherwise, presume a line string                                */
/* -------------------------------------------------------------------- */

    OGRLineString* poLS = new OGRLineString;

    m_blinestringstarted = false;

    std::vector<DXFSmoothPolylineVertex>::const_iterator iter = m_vertices.begin();
    std::vector<DXFSmoothPolylineVertex>::const_iterator eiter = m_vertices.end();

    eiter--;

    DXFSmoothPolylineVertex begin = *iter;

    double dfZ = 0.0;
    const bool bConstantZ = this->HasConstantZ(dfZ);

    while(iter != eiter)
    {
        iter++;
        DXFSmoothPolylineVertex end = *iter;

        const double len = GetLength(begin,end);

        if((len == 0) || (begin.bulge == 0))
        {
            this->EmitLine(begin, end, poLS, bConstantZ, dfZ);
        }
        else
        {
            const double radius = GetRadius(begin.bulge,len);
            this->EmitArc(begin, end, radius, len, begin.bulge, poLS, dfZ);
        }

        // Move to next vertex
        begin = end;
    }


/* -------------------------------------------------------------------- */
/*      If polyline is closed, convert linestring to a linear ring      */
/* -------------------------------------------------------------------- */

    if(m_bClosed)
    {
        OGRLinearRing *poLR = new OGRLinearRing();
        poLR->addSubLineString( poLS, 0 );
        delete poLS;

        // Wrap as polygon.
        OGRPolygon *poPoly = new OGRPolygon();
        poPoly->addRingDirectly( poLR );

        return poPoly;
    }
    return poLS;
}


/************************************************************************/
/*                DXFSmoothPolyline::EmitArc()                        */
/************************************************************************/

void DXFSmoothPolyline::EmitArc
(
    const DXFSmoothPolylineVertex& start, 
    const DXFSmoothPolylineVertex& end,
    double radius, double len, double bulge,
    OGRLineString* poLS,
    double dfZ
) const
{
    assert(poLS);

    double  ogrArcRotation = 0.0,
            ogrArcRadius = fabs(radius);


/* -------------------------------------------------------------------- */
/*      Set arc's direction and keep bulge positive                     */
/* -------------------------------------------------------------------- */

    const bool bClockwise = (bulge < 0);

    if(bClockwise)
        bulge *= -1;


/* -------------------------------------------------------------------- */
/*      Get arc's center point                                          */
/* -------------------------------------------------------------------- */

    const double saggita = fabs(bulge * (len / 2.0));
    const double apo = bClockwise 
                        ? -(ogrArcRadius - saggita)
                        : -(saggita - ogrArcRadius);

    DXFSmoothPolylineVertex v;
    v.x = start.x - end.x;
    v.y = start.y - end.y;

#ifdef DEBUG
    const bool bMathissue = (v.x == 0.0 || v.y == 0.0);
#endif

    DXFSmoothPolylineVertex midpoint;
    midpoint.x = end.x + 0.5 * v.x;
    midpoint.y = end.y + 0.5 * v.y;

    DXFSmoothPolylineVertex pperp;
    pperp.x = v.y;
    pperp.y = -v.x;
    pperp.normalize();

    DXFSmoothPolylineVertex ogrArcCenter;
    ogrArcCenter.x = midpoint.x + (pperp.x * apo);
    ogrArcCenter.y = midpoint.y + (pperp.y * apo);


/* -------------------------------------------------------------------- */
/*      Get the line's general vertical direction (-1 = down, +1 = up)  */
/* -------------------------------------------------------------------- */

    const double linedir = end.y > start.y ? 1.0 : -1.0;


/* -------------------------------------------------------------------- */
/*      Get arc's starting angle.                                       */
/* -------------------------------------------------------------------- */

    double a = GetAngle(ogrArcCenter, start);

    if(bClockwise && (linedir == 1.0))
        a += (linedir * 180.0);

    double ogrArcStartAngle = GetOGRangle(a);


/* -------------------------------------------------------------------- */
/*      Get arc's ending angle.                                         */
/* -------------------------------------------------------------------- */

    a = GetAngle(ogrArcCenter, end);

    if(bClockwise && (linedir == 1.0))
        a += (linedir * 180.0);

    double ogrArcEndAngle = GetOGRangle(a);

    if(!bClockwise && (ogrArcStartAngle < ogrArcEndAngle))
        ogrArcEndAngle = -180.0 + (linedir * a);


/* -------------------------------------------------------------------- */
/*      Flip arc's rotation if necessary.                               */
/* -------------------------------------------------------------------- */

    if(bClockwise && (linedir == 1.0))
        ogrArcRotation = linedir * 180.0;


/* -------------------------------------------------------------------- */
/*      Tesselate the arc segment and append to the linestring.         */
/* -------------------------------------------------------------------- */

    OGRLineString* poArcpoLS = 
        (OGRLineString*)OGRGeometryFactory::approximateArcAngles(
            ogrArcCenter.x, ogrArcCenter.y, dfZ,
            ogrArcRadius, ogrArcRadius, ogrArcRotation,
            ogrArcStartAngle, ogrArcEndAngle,
            0.0);

    poLS->addSubLineString(poArcpoLS);

    delete poArcpoLS;
}



/************************************************************************/
/*                DXFSmoothPolyline::EmitLine()                         */
/************************************************************************/

void DXFSmoothPolyline::EmitLine
(
    const DXFSmoothPolylineVertex& start, 
    const DXFSmoothPolylineVertex& end,
    OGRLineString* poLS,
    bool bConstantZ,
    double dfZ
) const
{
    assert(poLS);

    if(!m_blinestringstarted)
    {
        poLS->addPoint(start.x, start.y, 
            bConstantZ ? dfZ : start.z);
        m_blinestringstarted = true;
    }

    poLS->addPoint(end.x, end.y, 
        bConstantZ ? dfZ : end.z);
}


/************************************************************************/
/*                DXFSmoothPolyline::Close()                            */
/************************************************************************/

void DXFSmoothPolyline::Close()
{
    assert(!m_bClosed);

    if(m_vertices.size() >= 2)
    {
        const bool bVisuallyClosed =
            (m_vertices[m_vertices.size() - 1].shares_2D_pos(m_vertices[0]));

        if(!bVisuallyClosed)
        {
            m_vertices.push_back(m_vertices[0]);
        }
        m_bClosed = true;
    }
}


/************************************************************************/
/*                DXFSmoothPolyline::HasConstantZ()                     */
/************************************************************************/

bool DXFSmoothPolyline::HasConstantZ(double& dfZ) const
{
    // Treat the polyline as having constant Z if all Z members
    // are equal or if any bulge attribute exists. In the latter case,
    // set dfZ to zero. Leave dfZ unassigned if false is returned.

    assert(!m_vertices.empty());

    const double d = m_vertices[0].z;

    for(int i = 1; i < m_vertices.size(); i++)
    {
        if(m_vertices[i].bulge != 0.0)
        {
            dfZ = 0.0;
            return true;
        }
        if(m_vertices[i].z != d)
            return false;
    }
    dfZ = d;
    return true;
}

