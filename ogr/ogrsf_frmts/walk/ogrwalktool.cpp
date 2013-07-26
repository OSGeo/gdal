/******************************************************************************
 * $Id: ogrwalktool.cpp
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

#define COORDDIMENSION      2        //OgcWkb only supports simple 2D geometries

/************************************************************************/
/*                         ByteOrder()                                  */
/************************************************************************/
static inline wkbByteOrder ByteOrder()
{
#ifdef CPL_LSB
    return wkbNDR;    //Little Endian
#else
    return wkbXDR;    //Big endian
#endif
}

/************************************************************************/
/*                       Binary2WkbMGeom()                              */
/************************************************************************/
OGRErr Binary2WkbMGeom(unsigned char *& p, WKBGeometry* geom, int nBytes)
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
                CPL_LSBPTRPOINTS(geom->linestring.segments[i].points[j], 
                    geom->linestring.segments[i].numPoints);
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
    GUInt32 i;

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

        for(i = 0; i < geom->mgeometries.num_wkbSGeometries; i++)
            Binary2WkbMGeom(p, (WKBGeometry*)(&geom->mgeometries.WKBGeometries[i]), nBytes-8);
        break;
    default:
        geom->wkbType=wkbUnknown;
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                 SZPointSize(): Size of WKBPoint                      */
/************************************************************************/
long SZPointSize(WKBPoint* pWalkWKBPoint)
{
    long retOgcWkbGeomSize = 0;
    
    retOgcWkbGeomSize += 5+8*COORDDIMENSION;
    return retOgcWkbGeomSize;
}

/************************************************************************/
/*             SZLineStringSize(): Size of WKBLinestring                */
/************************************************************************/
long SZLineStringSize(WKBLineString* pLineString)
{
    int nPointCount = 0;    //only linestring.
    long retOgcWkbGeomSize = 0;

    for (GUInt32 i = 0; i < pLineString->numSegments; ++i)
        nPointCount += pLineString->segments[i].numPoints;

    retOgcWkbGeomSize += 5+4+8*COORDDIMENSION*nPointCount;
    return retOgcWkbGeomSize;
}

/************************************************************************/
/*             SZLinearringSize(): Size of WKBLinearring                */
/************************************************************************/
long SZLinearringSize(WKBLineString* pLinearring)
{
    int nPointCount = 0;
    long retOgcWkbGeomSize = 0;

    for (GUInt32 i = 0; i < pLinearring->numSegments; ++i)
        nPointCount += pLinearring->segments[i].numPoints;

    retOgcWkbGeomSize += 4 + 8 * COORDDIMENSION*nPointCount;
    return retOgcWkbGeomSize;
}

/************************************************************************/
/*                SZPolygonSize(): Size of WKBPolygon                   */
/************************************************************************/
long SZPolygonSize(WKBPolygon* pWalkWKBPolygon)
{
    long retOgcWkbGeomSize = 9;

    for (GUInt32 i = 0; i < pWalkWKBPolygon->numRings; ++i)
    {
        LineString* lineString = &pWalkWKBPolygon->rings[i];
        retOgcWkbGeomSize += SZLinearringSize(lineString);
    }
    return retOgcWkbGeomSize;
}

/************************************************************************/
/*                           SZWKBGeomSize()                            */
/************************************************************************/
long SZWKBGeomSize(WKBGeometry* pWalkWKBGeometry)
{
    long retOgcWkbGeomSize = 0;

    if (pWalkWKBGeometry == NULL)
        return retOgcWkbGeomSize;

    switch (pWalkWKBGeometry->wkbType)
    {
    case wkbPoint:
        retOgcWkbGeomSize += SZPointSize(&pWalkWKBGeometry->point);
        break;
    case wkbLineString:
        retOgcWkbGeomSize += SZLineStringSize(&pWalkWKBGeometry->linestring);
        break;
    case wkbPolygon:
        retOgcWkbGeomSize += SZPolygonSize(&pWalkWKBGeometry->polygon);
        break;
    case wkbMultiPoint:
        {
            retOgcWkbGeomSize = 9;
            WKBMultiPoint mpoint = pWalkWKBGeometry->mpoint;
            for (GUInt32 i = 0; i < mpoint.num_wkbPoints; ++i)
            {
                WKBPoint* point = &pWalkWKBGeometry->mpoint.WKBPoints[i];
                retOgcWkbGeomSize += SZPointSize(point);
            }
        }
        break;
    case wkbMultiLineString:
        {
            retOgcWkbGeomSize = 9;
            WKBMultiLineString mlines = pWalkWKBGeometry->mlinestring;
            for (GUInt32 i = 0; i < mlines.num_wkbLineStrings; ++i)
            {
                retOgcWkbGeomSize += SZLineStringSize(&mlines.WKBLineStrings[i]);
            }
        }
        break;
    case wkbMultiPolygon:
        {
            retOgcWkbGeomSize = 9;
            WKBMultiPolygon mpoly = pWalkWKBGeometry->mpolygon;
            for (GUInt32 i = 0; i < mpoly.num_wkbPolygons; ++i)
            {
                retOgcWkbGeomSize += SZPolygonSize(&mpoly.WKBPolygons[i]);
            }
        }
        break;
    case wkbGeometryCollection:
        {
            retOgcWkbGeomSize = 9;
            WKBGeometryCollection mg = pWalkWKBGeometry->mgeometries;
            for (GUInt32 i = 0; i < mg.num_wkbSGeometries; ++i)
            {
                WKBSimpleGeometry* sg = &mg.WKBGeometries[i];
                switch (sg->wkbType)
                {
                case wkbPoint: retOgcWkbGeomSize += SZPointSize(&sg->point);
                               break;
                case wkbLineString: retOgcWkbGeomSize += SZLineStringSize(&sg->linestring);
                                    break;
                case wkbPolygon: retOgcWkbGeomSize += SZPolygonSize(&sg->polygon);
                                 break;
                default: break;
                }                
            }
        }
        break;
    default:
        break;
    }

    return retOgcWkbGeomSize;
}

/************************************************************************/
/*                           WalkPoint2Wkb()                            */
/************************************************************************/
OGRBoolean WalkPoint2Wkb(unsigned char* pOgcWkb, WKBPoint* pWalkWkbPoint)
{
    if (pOgcWkb == NULL || pWalkWkbPoint == NULL)
        return FALSE;

    pOgcWkb[0] = (unsigned char)ByteOrder();
    unsigned int nGType = wkbPoint;
    memcpy(pOgcWkb+1, &nGType, 4);
    memcpy(pOgcWkb+5, &pWalkWkbPoint->x, 8*COORDDIMENSION);
    return TRUE;
}

/************************************************************************/
/*                        WalkLineString2Wkb()                          */
/************************************************************************/
OGRBoolean WalkLineString2Wkb(unsigned char* pOgcWkb, LineString* pLineString)
{
    if (pOgcWkb == NULL || pLineString == NULL)
        return FALSE;

    pOgcWkb[0] = (unsigned char)ByteOrder();
    unsigned int nGType = wkbLineString;
    memcpy(pOgcWkb+1, &nGType, 4);
    //Copy in the data count.   
    int nPointCount = 0;
    
    for (GUInt32 i = 0; i < pLineString->numSegments; ++i)
    {
        nPointCount += pLineString->segments[i].numPoints;
        memcpy(pOgcWkb+5, &nPointCount, 4);    
        
        for (GUInt32 j = 0; j < pLineString->segments[i].numPoints; ++j)
        {
            Point* point = pLineString->segments[i].points;
            memcpy(pOgcWkb+5+4+8*COORDDIMENSION*j, point+j, 8*COORDDIMENSION);
        }
    }
    
    return TRUE;
}

/************************************************************************/
/*                        WalkLinearring2Wkb()                          */
/************************************************************************/
OGRBoolean WalkLinearring2Wkb(unsigned char* pOgcWkb, LineString* pLineString)
{
    if (pOgcWkb == NULL || pLineString == NULL)
        return FALSE;

    for(GUInt32 i = 0; i < pLineString->numSegments; i++)
    {
        memcpy(pOgcWkb, &pLineString->segments[i].numPoints, 4);
        
        for (GUInt32 j = 0; j < pLineString->segments[i].numPoints; ++j)
        {
            Point* point = pLineString->segments[i].points;
            memcpy(pOgcWkb+4+8*COORDDIMENSION*j, point+j, 8*COORDDIMENSION);
        }
    }    
    return TRUE;
}

/************************************************************************/
/*                          WalkPolygon2Wkb()                           */
/************************************************************************/
OGRBoolean WalkPolygon2Wkb(unsigned char* pOgcWkb, WKBPolygon* pWalkWkbPolgon)
{
    if (pOgcWkb == NULL || pWalkWkbPolgon == NULL)
        return FALSE;

    pOgcWkb[0] = (unsigned char)ByteOrder();
    unsigned int nGType = wkbPolygon;
    memcpy(pOgcWkb+1, &nGType, 4);
    //Copy in the data count.   
    GUInt32 nRingCount = pWalkWkbPolgon->numRings;
    memcpy(pOgcWkb+5, &nRingCount, 4);
    int nOffset = 9;
    
    for (GUInt32 i = 0; i < nRingCount; ++i)
    {
        LineString* lineString = &pWalkWkbPolgon->rings[i];
        WalkLinearring2Wkb(nOffset + pOgcWkb, lineString);
        nOffset += SZLinearringSize(lineString);            
    }    
    return TRUE;
}

/************************************************************************/
/*                            WalkGeom2Wkb()                            */
/************************************************************************/
OGRBoolean WalkGeom2Wkb(unsigned char* pOgcWkb, WKBGeometry* geom)
{
    if (pOgcWkb == NULL || geom == NULL)
        return FALSE;

    switch (geom->wkbType)
    {
    case wkbPoint:
        {
            WalkPoint2Wkb(pOgcWkb, &geom->point);
        }
        break;
    case wkbLineString:
        {
            WalkLineString2Wkb(pOgcWkb, &geom->linestring);
        }
        break;
    case wkbPolygon:
        {
            WalkPolygon2Wkb(pOgcWkb, &geom->polygon);
        }
        break;
    case wkbMultiPoint:
        {
            pOgcWkb[0] = (unsigned char)ByteOrder();
            unsigned int nGType = wkbMultiPoint;
            memcpy(pOgcWkb+1, &nGType, 4);
            GUInt32 nCols = geom->mpoint.num_wkbPoints;
            memcpy(pOgcWkb+5, &nCols, 4);
            int nOffset = 9;

            for (GUInt32 i = 0; i < nCols; ++i)
            {
                WKBPoint* point = &geom->mpoint.WKBPoints[i];    
                WalkPoint2Wkb(pOgcWkb+nOffset, point);
                nOffset += SZPointSize(point);                
            }            
        }
        break;
    case wkbMultiLineString:
        {
            pOgcWkb[0] = (unsigned char)ByteOrder();
            unsigned int nGType = wkbMultiLineString;
            memcpy(pOgcWkb+1, &nGType, 4);
            GUInt32 nCols = geom->mlinestring.num_wkbLineStrings;
            memcpy(pOgcWkb+5, &nCols, 4);
            int nOffset = 9;
            
            for (GUInt32 i = 0; i < nCols; ++i)
            {
                WKBLineString* linestring = &geom->mlinestring.WKBLineStrings[i];
                WalkLineString2Wkb(pOgcWkb+nOffset, linestring);
                nOffset += SZLineStringSize(linestring);
            }
        }
        break;
    case wkbMultiPolygon:
        {
            pOgcWkb[0] = (unsigned char)ByteOrder();
            unsigned int nGType = wkbMultiPolygon;
            int nOffset = 9;
            memcpy(pOgcWkb+1, &nGType, 4);
            GUInt32 nCols = geom->mpolygon.num_wkbPolygons;
            memcpy(pOgcWkb+5, &nCols, 4);
            
            for (GUInt32 i = 0; i < nCols; ++i)
            {
                WKBPolygon* polygon = &geom->mpolygon.WKBPolygons[i];
                WalkPolygon2Wkb(pOgcWkb+nOffset, polygon);
                nOffset += SZPolygonSize(polygon);
            }
        }
        break;
    case wkbGeometryCollection:
        {
            pOgcWkb[0] = (unsigned char)ByteOrder();
            unsigned int nGType = wkbGeometryCollection;
            int nOffset = 9;
            memcpy(pOgcWkb+1, &nGType, 4);
            GUInt32 nCols = geom->mgeometries.num_wkbSGeometries;
            memcpy(pOgcWkb+5, &nCols, 4);
            
            for (GUInt32 i = 0; i < nCols; ++i)
            {
                WKBSimpleGeometry* sg = &geom->mgeometries.WKBGeometries[i];
                switch (sg->wkbType)
                {
                    case wkbPoint: WalkPoint2Wkb(pOgcWkb+nOffset, &sg->point);
                                   nOffset += SZPointSize(&sg->point);
                                   break;
                    case wkbLineString: WalkLineString2Wkb(pOgcWkb+nOffset, &sg->linestring);
                                        nOffset += SZLineStringSize(&sg->linestring);
                                        break;
                    case wkbPolygon: WalkPolygon2Wkb(pOgcWkb+nOffset, &sg->polygon);
                                     nOffset += SZPolygonSize(&sg->polygon);
                                     break;
                    default: break;
                }                
            }
        }
        break;
    default:
        break;
    }

    return TRUE;
}

/************************************************************************/
/*                      DeleteCurveSegment()                            */
/************************************************************************/
void DeleteCurveSegment(CurveSegment &obj)
{
    if(obj.numPoints)
        delete [] obj.points;
}

/************************************************************************/
/*                      DeleteWKBMultiPoint()                           */
/************************************************************************/
void DeleteWKBMultiPoint(WKBMultiPoint &obj)
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
void DeleteWKBLineString(WKBLineString &obj)
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
void DeleteWKBMultiLineString(WKBMultiLineString &obj)
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
void DeleteWKBPolygon(WKBPolygon &obj)
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
void DeleteWKBMultiPolygon(WKBMultiPolygon &obj)
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
void DeleteWKBGeometryCollection(WKBGeometryCollection &obj)
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