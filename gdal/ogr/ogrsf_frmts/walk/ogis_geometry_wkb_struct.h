/******************************************************************************
 * $Id: ogis_geometry_wkb_struct.h
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definition of GeometryWkb Structs
 * Author:   Xian Chen, chenxian at walkinfo.com.cn
 *
 ******************************************************************************
 * Copyright (c) 2013,  ZJU Walkinfo Technology Corp., Ltd.
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

/**************************************************************************/
/* Basic Type definitions                                                 */
/* unsigned char : 1 BYTE                                                 */
/* GUInt32 : 32 bit unsigned integer (4 bytes)                            */
/* double : double precision number (8 bytes)                             */
/* Building Blocks : Point, LinearRing                                    */
/**************************************************************************/

#ifndef _OGIS_GEOMETRY_WKB_STRUCT_H
#define _OGIS_GEOMETRY_WKB_STRUCT_H

#define CPL_LSBPTRPOINT(p) \
{                                                                 \
    CPL_LSBPTR32(&p.x);                                           \
    CPL_LSBPTR32(&p.y);                                           \
    CPL_LSBPTR32(&p.z);                                           \
}

#ifdef CPL_MSB
#define CPL_LSBPTRPOINTS(p,n) \
{                                                                 \
    GUInt32 count;                                                \
    for(count=0; count<n; count++)                                \
        CPL_LSBPTRPOINT(p[count]);                                \
}
#else
#define CPL_LSBPTRPOINTS(p,n)
#endif

struct Point3D {
    double x;
    double y;
    double z;
};

struct Point2D {
    double x;
    double y;
};

struct CurveSegment;
struct LineString;

typedef Point3D Point;       //3D by default
typedef Point Vector;        //Space Vector    {dx, dy, dz}
typedef Point WKBPoint;

/**************************************************************************/
/* Curves are continuous, connected and have a measurable length in terms */
/* of the coordinate system. The curve segments are therefore connected   */
/* to one another, with the end point of each segment being the start     */
/* point of the next in the segment list.                                 */
/* A curve is composed of one or more curve segments. Each curve segment  */
/* may be defined using a different interpolation method than the other   */
/* ones in the curve.                                                     */
/* A LineString is a curve with linear interpolation between points. Each */
/* consecutive pair of points defines a line segment.                     */
/* Extention£ºLineString is composed of CurveSegment, but self-crossing   */
/* is not allowed.                                                        */
/**************************************************************************/

enum wkLineType {
    wkLineTypePoint        =0,   // Point
    wkLineTypeStraight    =1,    // Strightline
    wkLineTypeBezier    =2,      // Bezier
    wkLineType3PArc        =3,   // 3-point Arc; Three points are defined
    wkLineTypeRArc        =4,    // Radius Arc; Three points are defined
    wkLineType5PEllipse    =5,   // 5-point Ellipse; from wkLineTypeRectArc
    wkLineType3PCircle    =6,    // 3-point Circle;
    wkLineTypeRCircle    =7,     // Radius Circle; 2 points
    wkLineTypeRectCircle=8,      // Rectangular Circle; 2 points
    wkLineTypeBCurve    =9,      // B Curve
    wkLineTypeStrainCurve =10,   // Strain Curve
};

struct CurveSegment {
    GUInt32 lineType;
    GUInt32 numPoints;
    Point *points;
};

struct LineString {
    GUInt32 numSegments;
    CurveSegment *segments;
};

typedef OGRwkbGeometryType wkbGeometryType;

/**************************************************************************/
/*    BYTE byteOrder;                                                     */
/*    This enum should be used to head base structure, only as operation  */
/*    system is no_windows. See struct WKBPoint                           */
/**************************************************************************/
typedef OGRwkbByteOrder wkbByteOrder;

/**************************************************************************/
/*    A Point is a 0-dimensional geometry and represents a single         */
/*    location in coordinate space.                                       */
/*    A point has an x-coordinate value and a y-coordinate value.         */
/*    The boundary of a point is the empty set.                           */
/**************************************************************************/
typedef Point WKBPoint;
typedef LineString WKBLineString;

/**************************************************************************/
/*    A Polygon is a planar surface, defined by 1 exterior boundary       */
/*    and 0 or more interior boundaries.                                  */
/*        Each interior boundary defines a hole in the polygon.           */
/*    The assertions for polygons (the rules that define valid polygons)  */
/*    are:                                                                */
/*    1. Polygons are topologically closed.                               */
/*    2. The boundary of a polygon consists of a set of LinearRings that  */
/*       make up its exterior and interior boundaries.                    */
/*    3. No two rings in the boundary cross, the rings in the boundary    */
/*       of a polygon may intersect at a point but only as a tangent.     */
/*    4. A Polygon may not have cut lines, spikes or punctures.           */
/*    5. The Interior of every Polygon is a connected point set.          */
/*    6. The Exterior of a Polygon with 1 or more holes is not connected. */
/*       Each hole defines a connected component of the Exterior.         */
/*    In the above assertions, Interior, Closure and Exterior have the    */
/*    standard topological definitions. The combination of 1 and 3 make   */
/*    a Polygon a Regular Closed point set.                               */
/*    Polygons are simple geometries.                                     */
/**************************************************************************/
struct WKBPolygon {
    GUInt32 numRings;
    LineString *rings;
};

/**************************************************************************/
/*    A MultiPoint is a 0 dimensional geometric collection. The elements  */
/*        of a MultiPoint are restricted to Points.                       */
/*    The points are not connected or ordered.                            */
/*    A MultiPoint is simple if no two Points in the MultiPoint are       */
/*          equal (have identical coordinate values).                     */
/*    The boundary of a MultiPoint is the empty set.                      */
/**************************************************************************/
struct WKBMultiPoint {
    GUInt32 num_wkbPoints;
    WKBPoint *WKBPoints;
};

struct WKBMultiLineString {
    GUInt32 num_wkbLineStrings;
    WKBLineString *WKBLineStrings;
};

/**************************************************************************/
/*    The assertions for MultiPolygons are :                              */
/*    1. The interiors of 2 Polygons that are elements of a MultiPolygon  */
/*       may not intersect.                                               */
/*    2. The Boundaries of any 2 Polygons that are elements of a          */
/*       MultiPolygon may not 'cross' and may touch at only a finite      */
/*       number of points. (Note that crossing is prevented by            */
/*       assertion 1 above).                                              */
/*    3. A MultiPolygon is defined as topologically closed.               */
/*    4. A MultiPolygon may not have cut lines, spikes or punctures,      */
/*       a MultiPolygon is a Regular, Closed point set:                   */
/*    5. The interior of a MultiPolygon with more than 1 Polygon is not   */ 
/*       connected, the number of connected components of the interior    */
/*       of a MultiPolygon is equal to the number of Polygons in the      */
/*       MultiPolygon.                                                    */
/*    The boundary of a MultiPolygon is a set of closed curves            */
/*       (LineStrings) corresponding to the boundaries of its element     */
/*       Polygons. Each curve in the boundary of the MultiPolygon is in   */
/*       the boundary of exactly 1 element Polygon, and every curve in    */
/*       the boundary of an element Polygon is in the boundary of the     */ 
/*       MultiPolygon.                                                    */
/**************************************************************************/
struct WKBMultiPolygon {
    GUInt32 num_wkbPolygons;
    WKBPolygon *WKBPolygons;
};

struct WKBSimpleGeometry {
    GUInt32 wkbType;
    union     {
        WKBPoint point;
        WKBLineString linestring;
        WKBPolygon polygon;
    };
};

struct WKBGeometryCollection {
    GUInt32 num_wkbSGeometries;
    WKBSimpleGeometry *WKBGeometries;
};

struct WKBGeometry {
    GUInt32 wkbType;
    union {
        WKBPoint point;
        WKBLineString linestring;
        WKBPolygon polygon;
        WKBMultiPoint mpoint;
        WKBMultiLineString mlinestring;
        WKBMultiPolygon mpolygon;
        WKBGeometryCollection mgeometries;
    };
public:
    WKBGeometry () { wkbType=wkbUnknown; }
};

#endif /* ndef OGIS_GEOMETRY_WKB_STRUCT_H */


