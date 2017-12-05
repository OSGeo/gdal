/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Walk Binary Data to Walk Geometry and OGC WKB
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

#include "ogrwalk.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                   OGRWalkArcCenterFromEdgePoints()                   */
/*                                                                      */
/*      Compute the center of an arc/circle from three edge points.     */
/************************************************************************/

static bool
OGRWalkArcCenterFromEdgePoints( double x_c0, double y_c0,
                                double x_c1, double y_c1,
                                double x_c2, double y_c2,
                                double *x_center, double *y_center )

{
/* -------------------------------------------------------------------- */
/*      Compute the inverse of the slopes connecting the first and      */
/*      second points.  Also compute the center point of the two        */
/*      points ... the point our crossing line will go through.          */
/* -------------------------------------------------------------------- */
    const double m1 = (y_c1 - y_c0) != 0.0
        ? ((x_c0 - x_c1) / (y_c1 - y_c0))
        : 1e+10;

    const double x1 = (x_c0 + x_c1) * 0.5;
    const double y1 = (y_c0 + y_c1) * 0.5;

/* -------------------------------------------------------------------- */
/*      Compute the same for the second point compared to the third     */
/*      point.                                                          */
/* -------------------------------------------------------------------- */
    const double m2 = (y_c2 - y_c1) != 0.0
        ? ((x_c1 - x_c2) / (y_c2 - y_c1))
        : 1e+10;

    const double x2 = (x_c1 + x_c2) * 0.5;
    const double y2 = (y_c1 + y_c2) * 0.5;

/* -------------------------------------------------------------------- */
/*      Turn these into the Ax+By+C = 0 form of the lines.              */
/* -------------------------------------------------------------------- */
    const double a1 = m1;
    const double a2 = m2;

    const double b1 = -1.0;
    const double b2 = -1.0;

    const double c1 = (y1 - m1*x1);
    const double c2 = (y2 - m2*x2);

/* -------------------------------------------------------------------- */
/*      Compute the intersection of the two lines through the center    */
/*      of the circle, using Kramers rule.                              */
/* -------------------------------------------------------------------- */
    if( a1*b2 - a2*b1 == 0.0 )
        return false;

    const double det_inv = 1 / (a1*b2 - a2*b1);

    *x_center = (b1*c2 - b2*c1) * det_inv;
    *y_center = (a2*c1 - a1*c2) * det_inv;

    return true;
}

/************************************************************************/
/*                       OGRWalkArcToLineString()                       */
/************************************************************************/
static bool
OGRWalkArcToLineString( double dfStartX, double dfStartY,
                        double dfAlongX, double dfAlongY,
                        double dfEndX, double dfEndY,
                        double dfCenterX, double dfCenterY,
                        double dfCenterZ, double dfRadius,
                        int nNumPoints, OGRLineString *poLS )
{
    double dfDeltaX = dfStartX - dfCenterX;
    double dfDeltaY = dfStartY - dfCenterY;
    const double dfStartAngle = -1 * atan2(dfDeltaY,dfDeltaX) * 180.0 / M_PI;

    dfDeltaX = dfAlongX - dfCenterX;
    dfDeltaY = dfAlongY - dfCenterY;
    double dfAlongAngle = -1 * atan2(dfDeltaY,dfDeltaX) * 180.0 / M_PI;
    // Try positive (clockwise?) winding.
    while( dfAlongAngle < dfStartAngle )
        dfAlongAngle += 360.0;

    dfDeltaX = dfEndX - dfCenterX;
    dfDeltaY = dfEndY - dfCenterY;
    double dfEndAngle = -1 * atan2(dfDeltaY,dfDeltaX) * 180.0 / M_PI;
    while( dfEndAngle < dfAlongAngle )
        dfEndAngle += 360.0;

    if( nNumPoints == 3 )        //Arc
    {
        if( dfEndAngle - dfStartAngle > 360.0 )
        {
            while( dfAlongAngle > dfStartAngle )
                dfAlongAngle -= 360.0;

            while( dfEndAngle > dfAlongAngle )
                dfEndAngle -= 360.0;
        }
    }
    else if( nNumPoints == 5 )  //Circle
    {
        // If anticlockwise, then start angle - end angle = 360.0;
        // Otherwise end angle - start angle = 360.0;
        if( dfEndAngle - dfStartAngle > 360.0 )
            dfEndAngle = dfStartAngle - 360.0;
        else
            dfEndAngle = dfStartAngle + 360.0;
    }
    else
    {
        return false;
    }

    OGRLineString* poArcpoLS =
        (OGRLineString*)OGRGeometryFactory::approximateArcAngles(
            dfCenterX, dfCenterY, dfCenterZ,
            dfRadius, dfRadius, 0.0,
            dfStartAngle, dfEndAngle, 0.0 );

    if( poArcpoLS == NULL )
        return false;

    poLS->addSubLineString(poArcpoLS);
    delete poArcpoLS;

    return true;
}

/************************************************************************/
/*                       Binary2WkbMGeom()                              */
/************************************************************************/
static OGRErr Binary2WkbMGeom(unsigned char *& p, WKBGeometry* geom, int nBytes)
{
    GUInt32 i,j,k;

    if( nBytes < 28 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WalkGeom binary size (%d) too small",
                 nBytes);
        return OGRERR_FAILURE;
    }

    memcpy(&geom->wkbType, p, 4);
    CPL_LSBPTR32( &geom->wkbType );
    p += 4;

    switch(geom->wkbType)
    {
    case wkbPoint:
        memcpy(&geom->point, p, sizeof(WKBPoint));
        CPL_LSBPTRPOINT(geom->point);
        p += sizeof(WKBPoint);
        break;
    case wkbLineString:
        memcpy(&geom->linestring.numSegments, p, 4);
        CPL_LSBPTR32(&geom->linestring.numSegments);
        p += 4;

        geom->linestring.segments = new CurveSegment[geom->linestring.numSegments];

        for(i = 0; i < geom->linestring.numSegments; i++)
        {
            memcpy(&geom->linestring.segments[i].lineType, p, 4);
            CPL_LSBPTR32(&geom->linestring.segments[i].lineType);
            p += 4;
            memcpy(&geom->linestring.segments[i].numPoints, p, 4);
            CPL_LSBPTR32(&geom->linestring.segments[i].numPoints);
            p += 4;
            geom->linestring.segments[i].points =
                new Point[geom->linestring.segments[i].numPoints];
            memcpy(geom->linestring.segments[i].points, p,
                sizeof(Point) * geom->linestring.segments[i].numPoints);
            CPL_LSBPTRPOINTS(geom->linestring.segments[i].points,
                geom->linestring.segments[i].numPoints);
            p += sizeof(Point) * geom->linestring.segments[i].numPoints;
        }
        break;
    case wkbPolygon:
        memcpy(&geom->polygon.numRings, p, 4);
        CPL_LSBPTR32(&geom->polygon.numRings);
        p += 4;
        geom->polygon.rings = new LineString[geom->polygon.numRings];

        for(i = 0; i < geom->polygon.numRings; i++)
        {
            memcpy(&geom->polygon.rings[i].numSegments, p, 4);
            CPL_LSBPTR32(&geom->polygon.rings[i].numSegments);
            p += 4;
            geom->polygon.rings[i].segments =
                new CurveSegment[geom->polygon.rings[i].numSegments];

            for(j = 0; j < geom->polygon.rings[i].numSegments; j++)
            {
                memcpy(&geom->polygon.rings[i].segments[j].lineType, p, 4);
                CPL_LSBPTR32(&geom->polygon.rings[i].segments[j].lineType);
                p += 4;
                memcpy(&geom->polygon.rings[i].segments[j].numPoints, p, 4);
                CPL_LSBPTR32(&geom->polygon.rings[i].segments[j].numPoints);
                p += 4;
                geom->polygon.rings[i].segments[j].points =
                    new Point[geom->polygon.rings[i].segments[j].numPoints];
                memcpy(geom->polygon.rings[i].segments[j].points, p,
                    sizeof(Point) * geom->polygon.rings[i].segments[j].numPoints);
                CPL_LSBPTRPOINTS(geom->polygon.rings[i].segments[j].points,
                    geom->polygon.rings[i].segments[j].numPoints);
                p += sizeof(Point) * geom->polygon.rings[i].segments[j].numPoints;
            }
        }
        break;
    case wkbMultiPoint:
        memcpy(&geom->mpoint.num_wkbPoints, p, 4);
        CPL_LSBPTR32(&geom->mpoint.num_wkbPoints);
        p += 4;
        geom->mpoint.WKBPoints = new WKBPoint[geom->mpoint.num_wkbPoints];
        memcpy(geom->mpoint.WKBPoints, p, sizeof(WKBPoint) * geom->mpoint.num_wkbPoints);
        CPL_LSBPTRPOINTS(geom->mpoint.WKBPoints, geom->mpoint.num_wkbPoints);
        p += sizeof(WKBPoint) * geom->mpoint.num_wkbPoints;
        break;
    case wkbMultiLineString:
        memcpy(&geom->mlinestring.num_wkbLineStrings, p, 4);
        CPL_LSBPTR32(&geom->mlinestring.num_wkbLineStrings);
        p += 4;
        geom->mlinestring.WKBLineStrings =
            new WKBLineString[geom->mlinestring.num_wkbLineStrings];

        for(i = 0; i < geom->mlinestring.num_wkbLineStrings; i++)
        {
            memcpy(&geom->mlinestring.WKBLineStrings[i].numSegments, p, 4);
            CPL_LSBPTR32(&geom->mlinestring.WKBLineStrings[i].numSegments);
            p += 4;
            geom->mlinestring.WKBLineStrings[i].segments =
                new CurveSegment[geom->mlinestring.WKBLineStrings[i].numSegments];

            for(j = 0; j < geom->mlinestring.WKBLineStrings[i].numSegments; j++)
            {
                memcpy(&geom->mlinestring.WKBLineStrings[i].segments[j].lineType, p, 4);
                CPL_LSBPTR32(&geom->mlinestring.WKBLineStrings[i].segments[j].lineType);
                p += 4;
                memcpy(&geom->mlinestring.WKBLineStrings[i].segments[j].numPoints, p, 4);
                CPL_LSBPTR32(&geom->mlinestring.WKBLineStrings[i].segments[j].numPoints);
                p += 4;
                geom->mlinestring.WKBLineStrings[i].segments[j].points =
                    new Point[geom->mlinestring.WKBLineStrings[i].segments[j].numPoints];
                memcpy(geom->mlinestring.WKBLineStrings[i].segments[j].points, p,
                    sizeof(Point) * geom->mlinestring.WKBLineStrings[i].segments[j].numPoints);
                CPL_LSBPTRPOINTS(geom->mlinestring.WKBLineStrings[i].segments[j].points,
                    geom->mlinestring.WKBLineStrings[i].segments[j].numPoints);
                p += sizeof(Point) * geom->mlinestring.WKBLineStrings[i].segments[j].numPoints;
            }
        }
        break;
    case wkbMultiPolygon:
        memcpy(&geom->mpolygon.num_wkbPolygons, p, 4);
        CPL_LSBPTR32(&geom->mpolygon.num_wkbPolygons);
        p += 4;
        geom->mpolygon.WKBPolygons = new WKBPolygon[geom->mpolygon.num_wkbPolygons];

        for(i = 0; i < geom->mpolygon.num_wkbPolygons; i++)
        {
            memcpy(&geom->mpolygon.WKBPolygons[i].numRings, p, 4);
            CPL_LSBPTR32(&geom->mpolygon.WKBPolygons[i].numRings);
            p += 4;
            geom->mpolygon.WKBPolygons[i].rings =
                new LineString[geom->mpolygon.WKBPolygons[i].numRings];

            for(j = 0; j < geom->mpolygon.WKBPolygons[i].numRings; j++)
            {
                memcpy(&geom->mpolygon.WKBPolygons[i].rings[j].numSegments, p, 4);
                CPL_LSBPTR32(&geom->mpolygon.WKBPolygons[i].rings[j].numSegments);
                p += 4;
                geom->mpolygon.WKBPolygons[i].rings[j].segments =
                    new CurveSegment[geom->mpolygon.WKBPolygons[i].rings[j].numSegments];

                for(k = 0; k < geom->mpolygon.WKBPolygons[i].rings[j].numSegments; k++)
                {
                    memcpy(&geom->mpolygon.WKBPolygons[i].rings[j].segments[k].lineType, p, 4);
                    CPL_LSBPTR32(&geom->mpolygon.WKBPolygons[i].rings[j].segments[k].lineType);
                    p += 4;
                    memcpy(&geom->mpolygon.WKBPolygons[i].rings[j].segments[k].numPoints, p, 4);
                    CPL_LSBPTR32(&geom->mpolygon.WKBPolygons[i].rings[j].segments[k].numPoints);
                    p += 4;
                    geom->mpolygon.WKBPolygons[i].rings[j].segments[k].points =
                        new Point[geom->mpolygon.WKBPolygons[i].rings[j].segments[k].numPoints];
                    memcpy(geom->mpolygon.WKBPolygons[i].rings[j].segments[k].points,
                        p, sizeof(Point) *
                        geom->mpolygon.WKBPolygons[i].rings[j].segments[k].numPoints);
                    CPL_LSBPTRPOINTS(geom->mpolygon.WKBPolygons[i].rings[j].segments[k].points,
                        geom->mpolygon.WKBPolygons[i].rings[j].segments[k].numPoints);
                    p += sizeof(Point) *
                        geom->mpolygon.WKBPolygons[i].rings[j].segments[k].numPoints;
                }
            }
        }
        break;
    default:
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                       Binary2WkbGeom()                               */
/************************************************************************/
OGRErr Binary2WkbGeom(unsigned char *p, WKBGeometry* geom, int nBytes)
{
    if( nBytes < 28 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WalkGeom binary size (%d) too small",
                 nBytes);
        return OGRERR_FAILURE;
    }

    memcpy(&geom->wkbType, p, 4);
    CPL_LSBPTR32( &geom->wkbType );

    switch(geom->wkbType)
    {
    case wkbPoint:
    case wkbLineString:
    case wkbPolygon:
    case wkbMultiPoint:
    case wkbMultiLineString:
    case wkbMultiPolygon:
        return Binary2WkbMGeom(p, geom, nBytes);
    case wkbGeometryCollection:
        p += 4;
        memcpy(&geom->mgeometries.num_wkbSGeometries, p, 4);
        CPL_LSBPTR32( &geom->mgeometries.num_wkbSGeometries );
        p += 4;
        geom->mgeometries.WKBGeometries =
            new WKBSimpleGeometry[geom->mgeometries.num_wkbSGeometries];

        for( GUInt32 i = 0; i < geom->mgeometries.num_wkbSGeometries; i++ )
            Binary2WkbMGeom(p, (WKBGeometry*)(&geom->mgeometries.WKBGeometries[i]), nBytes-8);
        break;
    default:
        geom->wkbType=wkbUnknown;
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                       TranslateWalkPoint()                           */
/************************************************************************/
static bool TranslateWalkPoint(OGRPoint *poPoint, WKBPoint* pWalkWkbPoint)
{
    if ( poPoint == NULL || pWalkWkbPoint == NULL )
        return false;

    poPoint->setX(pWalkWkbPoint->x);
    poPoint->setY(pWalkWkbPoint->y);
    poPoint->setZ(pWalkWkbPoint->z);
    return true;
}

/************************************************************************/
/*                    TranslateCurveSegment()                           */
/************************************************************************/
static bool TranslateCurveSegment(OGRLineString *poLS, CurveSegment* pSegment)
{
    if ( poLS == NULL || pSegment == NULL )
        return false;

    switch(pSegment->lineType)
    {
    case OGRWALK::wkLineType3PArc:
    case OGRWALK::wkLineType3PCircle:
        {
            double dfCenterX;
            double dfCenterY;
            double dfCenterZ;
            double dfRadius;

            if ( !OGRWalkArcCenterFromEdgePoints( pSegment->points[0].x, pSegment->points[0].y,
                                           pSegment->points[1].x, pSegment->points[1].y,
                                           pSegment->points[2].x, pSegment->points[2].y,
                                           &dfCenterX, &dfCenterY ) )
                return false;

            //Use Z value of the first point
            dfCenterZ = pSegment->points[0].z;
            dfRadius = sqrt( (dfCenterX - pSegment->points[0].x) * (dfCenterX - pSegment->points[0].x)
                           + (dfCenterY - pSegment->points[0].y) * (dfCenterY - pSegment->points[0].y) );

            if ( !OGRWalkArcToLineString( pSegment->points[0].x, pSegment->points[0].y,
                        pSegment->points[1].x, pSegment->points[1].y,
                        pSegment->points[2].x, pSegment->points[2].y,
                        dfCenterX, dfCenterY, dfCenterZ, dfRadius,
                        pSegment->numPoints, poLS ) )
                return false;
        }
        break;
    case OGRWALK::wkLineTypeStraight:
    default:
        {
            for (GUInt32 i = 0; i < pSegment->numPoints; ++i)
            {
                Point point = pSegment->points[i];
                poLS->addPoint(point.x, point.y, point.z);
            }
        }
        break;
    }

    return true;
}

/************************************************************************/
/*                    TranslateWalkLineString()                         */
/************************************************************************/
static bool TranslateWalkLineString( OGRLineString *poLS,
                                     LineString* pLineString )
{
    if( poLS == NULL || pLineString == NULL )
        return false;

    for( GUInt32 i = 0; i < pLineString->numSegments; ++i )
    {
        if ( !TranslateCurveSegment(poLS, &pLineString->segments[i]) )
            return false;
    }

    return true;
}

/************************************************************************/
/*                    TranslateWalkLinearring()                         */
/************************************************************************/
static bool TranslateWalkLinearring( OGRLinearRing *poRing,
                                     LineString* pLineString )
{
    if( poRing == NULL || pLineString == NULL )
        return false;

    for( GUInt32 i = 0; i < pLineString->numSegments; i++ )
        TranslateCurveSegment(poRing, &pLineString->segments[i]);

    return true;
}

/************************************************************************/
/*                    TranslateWalkPolygon()                            */
/************************************************************************/
static bool TranslateWalkPolygon( OGRPolygon *poPolygon,
                                  WKBPolygon* pWalkWkbPolgon )
{
    if ( poPolygon == NULL || pWalkWkbPolgon == NULL )
        return false;

    for( GUInt32 i = 0; i < pWalkWkbPolgon->numRings; ++i )
    {
        OGRLinearRing* poRing = new OGRLinearRing();
        LineString* lineString = &pWalkWkbPolgon->rings[i];
        TranslateWalkLinearring(poRing, lineString);
        poPolygon->addRingDirectly(poRing);
    }

    return true;
}

/************************************************************************/
/*                        TranslateWalkGeom()                           */
/************************************************************************/
OGRErr TranslateWalkGeom(OGRGeometry **ppoGeom, WKBGeometry* geom)
{
    if ( ppoGeom == NULL || geom == NULL )
        return OGRERR_NOT_ENOUGH_DATA;

    OGRGeometry* poGeom =
        OGRGeometryFactory::createGeometry(wkbFlatten(geom->wkbType));

    if ( poGeom == NULL )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    switch (geom->wkbType)
    {
    case wkbPoint:
        {
            if( !TranslateWalkPoint((OGRPoint *)poGeom, &geom->point) )
            {
                delete poGeom;
                return OGRERR_CORRUPT_DATA;
            }
        }
        break;
    case wkbLineString:
        {
            if (!TranslateWalkLineString((OGRLineString *)poGeom, &geom->linestring))
            {
                delete poGeom;
                return OGRERR_CORRUPT_DATA;
            }
        }
        break;
    case wkbPolygon:
        {
            if (!TranslateWalkPolygon((OGRPolygon *)poGeom, &geom->polygon))
            {
                delete poGeom;
                return OGRERR_CORRUPT_DATA;
            }
        }
        break;
    case wkbMultiPoint:
        {
            for (GUInt32 i = 0; i < geom->mpoint.num_wkbPoints; ++i)
            {
                OGRPoint* poPoint = new OGRPoint();
                if( !TranslateWalkPoint(poPoint, &geom->mpoint.WKBPoints[i]) )
                {
                    delete poPoint;
                    delete poGeom;
                    return OGRERR_CORRUPT_DATA;
                }
                ((OGRMultiPoint *)poGeom)->addGeometryDirectly(poPoint);
            }
        }
        break;
    case wkbMultiLineString:
        {
            for (GUInt32 i = 0; i < geom->mlinestring.num_wkbLineStrings; ++i)
            {
                OGRLineString* poLS = new OGRLineString();
                if (!TranslateWalkLineString(poLS, &geom->mlinestring.WKBLineStrings[i]))
                {
                    delete poLS;
                    delete poGeom;
                    return OGRERR_CORRUPT_DATA;
                }
                ((OGRMultiLineString *)poGeom)->addGeometryDirectly(poLS);
            }
        }
        break;
    case wkbMultiPolygon:
        {
            for (GUInt32 i = 0; i < geom->mpolygon.num_wkbPolygons; ++i)
            {
                OGRPolygon* poPolygon = new OGRPolygon();
                if (!TranslateWalkPolygon(poPolygon, &geom->mpolygon.WKBPolygons[i]))
                {
                    delete poPolygon;
                    delete poGeom;
                    return OGRERR_CORRUPT_DATA;
                }
                ((OGRMultiPolygon *)poGeom)->addGeometryDirectly(poPolygon);
            }
        }
        break;
    case wkbGeometryCollection:
        {
            for (GUInt32 i = 0; i < geom->mgeometries.num_wkbSGeometries; ++i)
            {
                WKBSimpleGeometry* sg = &geom->mgeometries.WKBGeometries[i];
                switch (sg->wkbType)
                {
                    case wkbPoint:
                        {
                            OGRPoint* poPoint = new OGRPoint();
                            if( !TranslateWalkPoint(poPoint, &sg->point) )
                            {
                                delete poPoint;
                                delete poGeom;
                                return OGRERR_CORRUPT_DATA;
                            }
                            ((OGRGeometryCollection *)poGeom)->addGeometryDirectly(poPoint);
                        }
                        break;
                    case wkbLineString:
                        {
                            OGRLineString* poLS = new OGRLineString();
                            if (!TranslateWalkLineString(poLS, &sg->linestring))
                            {
                                delete poLS;
                                delete poGeom;
                                return OGRERR_CORRUPT_DATA;
                            }
                            ((OGRGeometryCollection *)poGeom)->addGeometryDirectly(poLS);
                        }
                        break;
                    case wkbPolygon:
                        {
                            OGRPolygon* poPolygon = new OGRPolygon();
                            if (!TranslateWalkPolygon(poPolygon, &sg->polygon))
                            {
                                delete poPolygon;
                                delete poGeom;
                                return OGRERR_CORRUPT_DATA;
                            }
                            ((OGRGeometryCollection *)poGeom)->addGeometryDirectly(poPolygon);
                        }
                        break;
                    default:
                    {
                        delete poGeom;
                        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                    }
                }
            }
        }
        break;
    default:
        delete poGeom;
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

    *ppoGeom = poGeom;

    return OGRERR_NONE;
}

/************************************************************************/
/*                      DeleteCurveSegment()                            */
/************************************************************************/
static void DeleteCurveSegment(CurveSegment &obj)
{
    if(obj.numPoints)
        delete [] obj.points;
}

/************************************************************************/
/*                      DeleteWKBMultiPoint()                           */
/************************************************************************/
static void DeleteWKBMultiPoint(WKBMultiPoint &obj)
{
    if (obj.num_wkbPoints)
    {
        delete[] obj.WKBPoints;
        obj.num_wkbPoints = 0;
    }
}

/************************************************************************/
/*                      DeleteWKBLineString()                           */
/************************************************************************/
static void DeleteWKBLineString(WKBLineString &obj)
{
    if(obj.numSegments)
    {
        for (GUInt32 i = 0; i < obj.numSegments; i++)
            DeleteCurveSegment(obj.segments[i]);
        delete [] obj.segments;
        obj.numSegments = 0;
    }
}

/************************************************************************/
/*                     DeleteWKBMultiLineString()                       */
/************************************************************************/
static void DeleteWKBMultiLineString(WKBMultiLineString &obj)
{
    if (obj.num_wkbLineStrings)
    {
        for (GUInt32 i = 0; i < obj.num_wkbLineStrings; i++)
            DeleteWKBLineString(obj.WKBLineStrings[i]);

        delete [] obj.WKBLineStrings;
        obj.num_wkbLineStrings = 0;
    }
}

/************************************************************************/
/*                        DeleteWKBPolygon()                            */
/************************************************************************/
static void DeleteWKBPolygon(WKBPolygon &obj)
{
    if (obj.numRings)
    {
        for (GUInt32 i = 0; i < obj.numRings; i++)
            DeleteWKBLineString(obj.rings[i]);

        delete [] obj.rings;
        obj.numRings = 0;
    }
}

/************************************************************************/
/*                      DeleteWKBMultiPolygon()                         */
/************************************************************************/
static void DeleteWKBMultiPolygon(WKBMultiPolygon &obj)
{
    if (obj.num_wkbPolygons)
    {
        for (GUInt32 i = 0; i < obj.num_wkbPolygons; i++)
            DeleteWKBPolygon(obj.WKBPolygons[i]);

        delete [] obj.WKBPolygons;
        obj.num_wkbPolygons = 0;
    }
}

/************************************************************************/
/*                    DeleteWKBGeometryCollection()                     */
/************************************************************************/
static void DeleteWKBGeometryCollection(WKBGeometryCollection &obj)
{
    if (obj.num_wkbSGeometries)
    {
        for (GUInt32 i = 0; i < obj.num_wkbSGeometries; i++)
            DeleteWKBGeometry(*(WKBGeometry*)&obj.WKBGeometries[i]);

        delete [] obj.WKBGeometries;
        obj.num_wkbSGeometries = 0;
    }
}

/************************************************************************/
/*                       DeleteWKBGeometry()                            */
/************************************************************************/
void DeleteWKBGeometry(WKBGeometry &obj)
{
    switch (obj.wkbType)
    {
    case wkbPoint:
        break;

    case wkbLineString:
        DeleteWKBLineString(obj.linestring);
        break;

    case wkbPolygon:
        DeleteWKBPolygon(obj.polygon);
        break;

    case wkbMultiPoint:
        DeleteWKBMultiPoint(obj.mpoint);
        break;

    case wkbMultiLineString:
        DeleteWKBMultiLineString(obj.mlinestring);
        break;

    case wkbMultiPolygon:
        DeleteWKBMultiPolygon(obj.mpolygon);
        break;

    case wkbGeometryCollection:
        DeleteWKBGeometryCollection(obj.mgeometries);
        break;
    }
    obj.wkbType = wkbUnknown;
}
