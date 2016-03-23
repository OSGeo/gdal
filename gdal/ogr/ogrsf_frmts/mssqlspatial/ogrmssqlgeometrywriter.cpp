/******************************************************************************
 * $Id$
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Implements OGRMSSQLGeometryWriter class to write native SqlGeometries.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2016, Tamas Szekeres
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

#include "cpl_conv.h"
#include "ogr_mssqlspatial.h"

CPL_CVSID("$Id$");

/*   SqlGeometry serialization format

Simple Point (SerializationProps & IsSinglePoint)
  [SRID][0x01][SerializationProps][Point][z][m]

Simple Line Segment (SerializationProps & IsSingleLineSegment)
  [SRID][0x01][SerializationProps][Point1][Point2][z1][z2][m1][m2]

Complex Geometries
  [SRID][0x01][SerializationProps][NumPoints][Point1]..[PointN][z1]..[zN][m1]..[mN]
  [NumFigures][Figure]..[Figure][NumShapes][Shape]..[Shape]

SRID
  Spatial Reference Id (4 bytes)

SerializationProps (bitmask) 1 byte
  0x01 = HasZValues
  0x02 = HasMValues
  0x04 = IsValid
  0x08 = IsSinglePoint
  0x10 = IsSingleLineSegment
  0x20 = IsWholeGlobe

Point (2-4)x8 bytes, size depends on SerializationProps & HasZValues & HasMValues
  [x][y]                  - SqlGeometry
  [latitude][longitude]   - SqlGeography

Figure
  [FigureAttribute][PointOffset]

FigureAttribute (1 byte)
  0x00 = Interior Ring
  0x01 = Stroke
  0x02 = Exterior Ring

Shape
  [ParentOffset][FigureOffset][ShapeType]

ShapeType (1 byte)
  0x00 = Unknown
  0x01 = Point
  0x02 = LineString
  0x03 = Polygon
  0x04 = MultiPoint
  0x05 = MultiLineString
  0x06 = MultiPolygon
  0x07 = GeometryCollection

*/

/************************************************************************/
/*                         Geometry writer macros                       */
/************************************************************************/

#define WriteInt32(nPos, value) (*((unsigned int*)(pszData + (nPos))) = value)

#define WriteByte(nPos, value) (pszData[nPos] = value)

#define WriteDouble(nPos, value) (*((double*)(pszData + (nPos))) = value)

#define ParentOffset(iShape) (nShapePos + (iShape) * 9 )
#define FigureOffset(iShape) (nShapePos + (iShape) * 9 + 4)
#define ShapeType(iShape) (nShapePos + (iShape) * 9 + 8)

#define FigureAttribute(iFigure) (nFigurePos + (iFigure) * 5)
#define PointOffset(iFigure) (nFigurePos + (iFigure) * 5 + 1)

#define WriteX(iPoint, value) (WriteDouble(nPointPos + 16 * (iPoint), value))
#define WriteY(iPoint, value) (WriteDouble(nPointPos + 16 * (iPoint) + 8, value))
#define WriteZ(iPoint, value) (WriteDouble(nPointPos + 16 * nNumPoints + 8 * (iPoint), value))
#define WriteM(iPoint, value) (WriteDouble(nPointPos + 24 * nNumPoints + 8 * (iPoint), value))

/************************************************************************/
/*                   OGRMSSQLGeometryWriter()                           */
/************************************************************************/

OGRMSSQLGeometryWriter::OGRMSSQLGeometryWriter(OGRGeometry *poGeometry, int nGeomColumnType, int nSRS)
{
    nColType = nGeomColumnType;
    nSRSId = nSRS;
    poGeom2 = poGeometry;

    chProps = 0;

    /* calculate required buffer length and the attributes */
    if (poGeom2->getCoordinateDimension() == 3)
    {
        chProps |= SP_HASZVALUES;
        nPointSize = 24;
    }
    else
    {
        nPointSize = 16;
    }

    iPoint = 0;
    nNumPoints = 0;
    iFigure = 0;
    nNumFigures = 0;
    iShape = 0;
    nNumShapes = 0;

    /* calculate points figures and shapes*/
    TrackGeometry(poGeom2);
    ++nNumShapes;

    OGRwkbGeometryType geomType = poGeom2->getGeometryType();

    if (nNumPoints == 1 && (geomType == wkbPoint || geomType == wkbPoint25D))
    {
        /* writing a single point */
        chProps |= SP_ISSINGLEPOINT | SP_ISVALID;
        nPointPos = 6;
        nLen = nPointPos + nPointSize;
    }
    else if (nNumPoints == 2 && (geomType == wkbLineString || geomType == wkbLineString25D))
    {
        /* writing a single line */
        chProps |= SP_ISSINGLELINESEGMENT | SP_ISVALID;
        nPointPos = 6;
        nLen = nPointPos + nPointSize * 2;
    }
    else
    {
        /* complex geometry */
        nPointPos = 10;
        nFigurePos = nPointPos + nPointSize * nNumPoints + 4;
        nShapePos = nFigurePos  + 5 * nNumFigures + 4;
        nLen = nShapePos + 9 * nNumShapes;
    }
}

/************************************************************************/
/*                         WritePoint()                                 */
/************************************************************************/

void OGRMSSQLGeometryWriter::WritePoint(OGRPoint* poGeom)
{
    if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        WriteY(iPoint, poGeom->getX());
        WriteX(iPoint, poGeom->getY());
        if (chProps & SP_HASZVALUES)
            WriteZ(iPoint, poGeom->getZ());
    }
    else
    {
        WriteX(iPoint, poGeom->getX());
        WriteY(iPoint, poGeom->getY());
        if (chProps & SP_HASZVALUES)
            WriteZ(iPoint, poGeom->getZ());
    }
    ++iPoint;
}

void OGRMSSQLGeometryWriter::WritePoint(double x, double y)
{
    if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        WriteY(iPoint, x);
        WriteX(iPoint, y);
    }
    else
    {
        WriteX(iPoint, x);
        WriteY(iPoint, y);
    }
    ++iPoint;
}

void OGRMSSQLGeometryWriter::WritePoint(double x, double y, double z)
{
    if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        WriteY(iPoint, x);
        WriteX(iPoint, y);
    }
    else
    {
        WriteX(iPoint, x);
        WriteY(iPoint, y);
    }
    WriteZ(iPoint, z);
    ++iPoint;
}

/************************************************************************/
/*                         WriteLineString()                            */
/************************************************************************/

void OGRMSSQLGeometryWriter::WriteLineString(OGRLineString* poGeom)
{
    int i;
    /* write figure */
    WriteByte(FigureAttribute(iFigure), 0x01);
    WriteInt32(PointOffset(iFigure), iPoint);
    if (chProps & SP_HASZVALUES)
    {
        for (i = 0; i < poGeom->getNumPoints(); i++)
            WritePoint(poGeom->getX(i), poGeom->getY(i), poGeom->getZ(i));
    }
    else
    {
        for (i = 0; i < poGeom->getNumPoints(); i++)
            WritePoint(poGeom->getX(i), poGeom->getY(i));          
    }
    ++iFigure;
}

/************************************************************************/
/*                         WritePolygon()                               */
/************************************************************************/

void OGRMSSQLGeometryWriter::WritePolygon(OGRPolygon* poGeom)
{
    int i, r;
    OGRLinearRing *poRing = poGeom->getExteriorRing();
    WriteByte(FigureAttribute(iFigure), 0x02);
    WriteInt32(PointOffset(iFigure), iPoint);
    if (chProps & SP_HASZVALUES)
    {
        /* write exterior ring */
        for (i = 0; i < poRing->getNumPoints(); i++)
            WritePoint(poRing->getX(i), poRing->getY(i), poRing->getZ(i));

        ++iFigure;

        for (r = 0; r < poGeom->getNumInteriorRings(); r++)
        {
            /* write interior rings */
            poRing = poGeom->getInteriorRing(r);
            WriteByte(FigureAttribute(iFigure), 0x00);
            WriteInt32(PointOffset(iFigure), iPoint);
            for (i = 0; i < poRing->getNumPoints(); i++)
                WritePoint(poRing->getX(i), poRing->getY(i), poRing->getZ(i));
            ++iFigure;
        }
    }
    else
    {
        /* write exterior ring */
        for (i = 0; i < poRing->getNumPoints(); i++)
            WritePoint(poRing->getX(i), poRing->getY(i));

        ++iFigure;

        for (r = 0; r < poGeom->getNumInteriorRings(); r++)
        {
            /* write interior rings */
            poRing = poGeom->getInteriorRing(r);
            WriteByte(FigureAttribute(iFigure), 0x00);
            WriteInt32(PointOffset(iFigure), iPoint);
            for (i = 0; i < poRing->getNumPoints(); i++)
                WritePoint(poRing->getX(i), poRing->getY(i));
            ++iFigure;
        }
    }  
}

/************************************************************************/
/*                         WriteGeometryCollection()                    */
/************************************************************************/

void OGRMSSQLGeometryWriter::WriteGeometryCollection(OGRGeometryCollection* poGeom, int iParent)
{
    int i;
    for (i = 0; i < poGeom->getNumGeometries(); i++)
        WriteGeometry(poGeom->getGeometryRef(i), iParent);
}

/************************************************************************/
/*                         WriteGeometry()                              */
/************************************************************************/

void OGRMSSQLGeometryWriter::WriteGeometry(OGRGeometry* poGeom, int iParent)
{
    /* write shape */
    WriteInt32(ParentOffset(iShape), iParent);
    WriteInt32(FigureOffset(iShape), iFigure);

    iParent = iShape;
    
    switch (poGeom->getGeometryType())
    {
    case wkbPoint:
    case wkbPoint25D:
        WriteByte(ShapeType(iShape++), ST_POINT);
        WriteByte(FigureAttribute(iFigure), 0x01);
        WriteInt32(PointOffset(iFigure), iPoint);
        WritePoint((OGRPoint*)poGeom);
        ++iFigure;
        break;
        
    case wkbLineString:
    case wkbLineString25D:
        WriteByte(ShapeType(iShape++), ST_LINESTRING);
        WriteLineString((OGRLineString*)poGeom);
        break;

    case wkbPolygon:
    case wkbPolygon25D:
        WriteByte(ShapeType(iShape++), ST_POLYGON);
        WritePolygon((OGRPolygon*)poGeom);
        break;

    case wkbMultiPoint:
    case wkbMultiPoint25D:
        WriteByte(ShapeType(iShape++), ST_MULTIPOINT);
        WriteGeometryCollection((OGRGeometryCollection*)poGeom, iParent);
        break;

    case wkbMultiLineString:
    case wkbMultiLineString25D:
        WriteByte(ShapeType(iShape++), ST_MULTILINESTRING);
        WriteGeometryCollection((OGRGeometryCollection*)poGeom, iParent);
        break;

    case wkbMultiPolygon:
    case wkbMultiPolygon25D:
        WriteByte(ShapeType(iShape++), ST_MULTIPOLYGON);
        WriteGeometryCollection((OGRGeometryCollection*)poGeom, iParent);
        break;

    case wkbGeometryCollection:
    case wkbGeometryCollection25D:
        WriteByte(ShapeType(iShape++), ST_GEOMETRYCOLLECTION);
        WriteGeometryCollection((OGRGeometryCollection*)poGeom, iParent);
        break;

    default:
        break;
    }
}

/************************************************************************/
/*                         TrackGeometry()                              */
/************************************************************************/

void OGRMSSQLGeometryWriter::TrackGeometry(OGRGeometry* poGeom)
{
    int i;
    switch (poGeom->getGeometryType())
    {
    case wkbPoint:
    case wkbPoint25D:
        ++nNumFigures;
        ++nNumPoints;
        break;
        
    case wkbLineString:
    case wkbLineString25D:
        ++nNumFigures;
        nNumPoints += ((OGRLineString*)poGeom)->getNumPoints();
        break;

    case wkbPolygon:
    case wkbPolygon25D:
        {
            OGRPolygon* g = (OGRPolygon*)poGeom;
            TrackGeometry(g->getExteriorRing());
            for (i = 0; i < g->getNumInteriorRings(); i++)
                TrackGeometry(g->getInteriorRing(i));
        }
        break;

    case wkbMultiPoint:
    case wkbMultiPoint25D:
    case wkbMultiLineString:
    case wkbMultiLineString25D:
    case wkbMultiPolygon:
    case wkbMultiPolygon25D:
    case wkbGeometryCollection:
    case wkbGeometryCollection25D:
        {
            OGRGeometryCollection* g = (OGRGeometryCollection*)poGeom;
            for (i = 0; i < g->getNumGeometries(); i++)
            {
                TrackGeometry(g->getGeometryRef(i));
                ++nNumShapes;
            }
        }
        break;

    default:
        break;
    }
}

/************************************************************************/
/*                         WriteSqlGeometry()                           */
/************************************************************************/

OGRErr OGRMSSQLGeometryWriter::WriteSqlGeometry(unsigned char* pszBuffer, int nBufLen)
{
    pszData = pszBuffer;

    if (nBufLen < nLen)
        return OGRERR_FAILURE;
    
    OGRwkbGeometryType geomType = poGeom2->getGeometryType();

    if (nNumPoints == 1 && (geomType == wkbPoint || geomType == wkbPoint25D))
    {
        /* writing a single point */
        OGRPoint* g = (OGRPoint*)poGeom2;
        WriteInt32(0, nSRSId);
        WriteByte(4, 0x01);
        WriteByte(5, chProps);
        if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
        {
            WriteY(0, g->getX());
            WriteX(0, g->getY());
            if (chProps & SP_HASZVALUES)
                WriteZ(0, g->getZ());
        }
        else
        {
            WriteX(0, g->getX());
            WriteY(0, g->getY());
            if (chProps & SP_HASZVALUES)
                WriteZ(0, g->getZ());
        }
    }
    else if (nNumPoints == 2 && (geomType == wkbLineString || geomType == wkbLineString25D))
    {
        /* writing a single line */
        OGRLineString* g = (OGRLineString*)poGeom2;
        WriteInt32(0, nSRSId);
        WriteByte(4, 0x01);
        WriteByte(5, chProps);
        if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
        {
            WriteY(0, g->getX(0));
            WriteX(0, g->getY(0));
            WriteY(1, g->getX(1));
            WriteX(1, g->getY(1));
            if (chProps & SP_HASZVALUES)
            {
                WriteZ(0, g->getZ(0));
                WriteZ(1, g->getZ(1));
            }
        }
        else
        {
            WriteX(0, g->getX(0));
            WriteY(0, g->getY(0));
            WriteX(1, g->getX(1));
            WriteY(1, g->getY(1));
            if (chProps & SP_HASZVALUES)
            {
                WriteZ(0, g->getZ(0));
                WriteZ(1, g->getZ(1));
            }
        }
    }
    else
    {
        /* complex geometry */
        if (poGeom2->IsValid())
            chProps |= SP_ISVALID;

        WriteInt32(0, nSRSId);
        WriteByte(4, 0x01);
        WriteByte(5, chProps);
        WriteInt32(nPointPos - 4 , nNumPoints);
        WriteInt32(nFigurePos - 4 , nNumFigures);
        WriteInt32(nShapePos - 4 , nNumShapes);

        WriteGeometry(poGeom2, 0xFFFFFFFF);
    }
    return OGRERR_NONE;
}

