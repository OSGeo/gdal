/******************************************************************************
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

CPL_CVSID("$Id$")

/*   SqlGeometry/SqlGeography serialization format

Simple Point (SerializationProps & IsSinglePoint)
  [SRID][0x01][SerializationProps][Point][z][m]

Simple Line Segment (SerializationProps & IsSingleLineSegment)
  [SRID][0x01][SerializationProps][Point1][Point2][z1][z2][m1][m2]

Complex Geometries
  [SRID][VersionAttribute][SerializationProps][NumPoints][Point1]..[PointN][z1]..[zN][m1]..[mN]
  [NumFigures][Figure]..[Figure][NumShapes][Shape]..[Shape]

Complex Geometries (FigureAttribute == Curve)
  [SRID][VersionAttribute][SerializationProps][NumPoints][Point1]..[PointN][z1]..[zN][m1]..[mN]
  [NumFigures][Figure]..[Figure][NumShapes][Shape]..[Shape][NumSegments][SegmentType]..[SegmentType]

VersionAttribute (1 byte)
  0x01 = Katmai (MSSQL2008+)
  0x02 = Denali (MSSQL2012+)

SRID
  Spatial Reference Id (4 bytes)

SerializationProps (bitmask) 1 byte
  0x01 = HasZValues
  0x02 = HasMValues
  0x04 = IsValid
  0x08 = IsSinglePoint
  0x10 = IsSingleLineSegment
  0x20 = IsLargerThanAHemisphere

Point (2-4)x8 bytes, size depends on SerializationProps & HasZValues & HasMValues
  [x][y]                  - SqlGeometry
  [latitude][longitude]   - SqlGeography

Figure
  [FigureAttribute][PointOffset]

FigureAttribute - Katmai (1 byte)
  0x00 = Interior Ring
  0x01 = Stroke
  0x02 = Exterior Ring

FigureAttribute - Denali (1 byte)
  0x00 = None
  0x01 = Line
  0x02 = Arc
  0x03 = Curve

Shape
  [ParentFigureOffset][FigureOffset][ShapeType]

ShapeType (1 byte)
  0x00 = Unknown
  0x01 = Point
  0x02 = LineString
  0x03 = Polygon
  0x04 = MultiPoint
  0x05 = MultiLineString
  0x06 = MultiPolygon
  0x07 = GeometryCollection
  -- Denali
  0x08 = CircularString
  0x09 = CompoundCurve
  0x0A = CurvePolygon
  0x0B = FullGlobe

SegmentType (1 byte)
  0x00 = Line
  0x01 = Arc
  0x02 = FirstLine
  0x03 = FirstArc

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
#define SegmentType(iSegment) (nSegmentPos + (iSegment))

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
    nPointSize = 16;
    if (poGeom2->getCoordinateDimension() == 3)
    {
        chProps |= SP_HASZVALUES;
        nPointSize += 8;
    }

    if (poGeom2->IsMeasured())
    {
        chProps |= SP_HASMVALUES;
        nPointSize += 8;
    }

    iPoint = 0;
    nNumPoints = 0;
    iFigure = 0;
    nNumFigures = 0;
    iShape = 0;
    nNumShapes = 0;
    iSegment = 0;
    nNumSegments = 0;

    /* calculate points figures, shapes and segments*/
    chVersion = VA_KATMAI;
    TrackGeometry(poGeom2);
    ++nNumShapes;

    OGRwkbGeometryType geomType = wkbFlatten(poGeom2->getGeometryType());

    if (nNumPoints == 1 && geomType == wkbPoint)
    {
        /* writing a single point */
        chProps |= SP_ISSINGLEPOINT | SP_ISVALID;
        nPointPos = 6;
        nLen = nPointPos + nPointSize;
    }
    else if (nNumPoints == 2 && geomType == wkbLineString)
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
        nSegmentPos = nShapePos + 9 * nNumShapes + 4;
        if (nNumSegments > 0)
            nLen = nSegmentPos + nNumSegments;
        else
            nLen = nShapePos + 9 * nNumShapes;
    }
}

/************************************************************************/
/*                         WritePoint()                                 */
/************************************************************************/

void OGRMSSQLGeometryWriter::WritePoint(OGRPoint* poGeom)
{
    if ((chProps & SP_HASZVALUES) && (chProps & SP_HASMVALUES))
        WritePoint(poGeom->getX(), poGeom->getY(), poGeom->getZ(), poGeom->getM());
    else if (chProps & SP_HASZVALUES)
        WritePoint(poGeom->getX(), poGeom->getY(), poGeom->getZ());
    else if (chProps & SP_HASMVALUES)
        WritePoint(poGeom->getX(), poGeom->getY(), poGeom->getM());
    else
        WritePoint(poGeom->getX(), poGeom->getY());
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
    WriteZ(iPoint, z);
    WritePoint(x, y);
}

void OGRMSSQLGeometryWriter::WritePoint(double x, double y, double z, double m)
{
    WriteZ(iPoint, z);
    WriteM(iPoint, m);
    WritePoint(x, y);
}

/************************************************************************/
/*                         WriteSimpleCurve()                           */
/************************************************************************/

void OGRMSSQLGeometryWriter::WriteSimpleCurve(OGRSimpleCurve* poGeom, int iStartIndex, int nCount)
{
    if ((chProps & SP_HASZVALUES) && (chProps & SP_HASMVALUES))
    {
        for (int i = iStartIndex; i < iStartIndex + nCount; i++)
            WritePoint(poGeom->getX(i), poGeom->getY(i), poGeom->getZ(i), poGeom->getM(i));
    }
    else if (chProps & SP_HASZVALUES)
    {
        for (int i = iStartIndex; i < iStartIndex + nCount; i++)
            WritePoint(poGeom->getX(i), poGeom->getY(i), poGeom->getZ(i));
    }
    else if (chProps & SP_HASMVALUES)
    {
        for (int i = iStartIndex; i < iStartIndex + nCount; i++)
            WritePoint(poGeom->getX(i), poGeom->getY(i), poGeom->getM(i));
    }
    else
    {
        for (int i = iStartIndex; i < iStartIndex + nCount; i++)
            WritePoint(poGeom->getX(i), poGeom->getY(i));
    }
}

void OGRMSSQLGeometryWriter::WriteSimpleCurve(OGRSimpleCurve* poGeom, int iStartIndex)
{
    WriteSimpleCurve(poGeom, iStartIndex, poGeom->getNumPoints() - iStartIndex);
}


void OGRMSSQLGeometryWriter::WriteSimpleCurve(OGRSimpleCurve* poGeom)
{
    WriteSimpleCurve(poGeom, 0, poGeom->getNumPoints());
}

/************************************************************************/
/*                         WriteCompoundCurve()                         */
/************************************************************************/

void OGRMSSQLGeometryWriter::WriteCompoundCurve(OGRCompoundCurve* poGeom)
{
    OGRSimpleCurve* poSubGeom;
    WriteByte(FigureAttribute(iFigure), FA_CURVE);
    WriteInt32(PointOffset(iFigure), iPoint);
    for (int i = 0; i < poGeom->getNumCurves(); i++)
    {
        poSubGeom = poGeom->getCurve(i)->toSimpleCurve();
        switch (wkbFlatten(poSubGeom->getGeometryType()))
        {
        case wkbLineString:
            if (i == 0)
                WriteSimpleCurve(poSubGeom);
            else
                WriteSimpleCurve(poSubGeom, 1);
            for (int j = 1; j < poSubGeom->getNumPoints(); j++)
            {
                if (j == 1)
                    WriteByte(SegmentType(iSegment++), SMT_FIRSTLINE);
                else
                    WriteByte(SegmentType(iSegment++), SMT_LINE);
            }
            break;

        case wkbCircularString:
            if (i == 0)
                WriteSimpleCurve(poSubGeom);
            else
                WriteSimpleCurve(poSubGeom, 1);
            for (int j = 2; j < poSubGeom->getNumPoints(); j += 2)
            {
                if (j == 2)
                    WriteByte(SegmentType(iSegment++), SMT_FIRSTARC);
                else
                    WriteByte(SegmentType(iSegment++), SMT_ARC);
            }
            break;

        default:
            break;
        }
    }
}

/************************************************************************/
/*                         WriteCurve()                                 */
/************************************************************************/

void OGRMSSQLGeometryWriter::WriteCurve(OGRCurve* poGeom)
{
    switch (wkbFlatten(poGeom->getGeometryType()))
    {
    case wkbLineString:
    case wkbLinearRing:
        WriteByte(FigureAttribute(iFigure), FA_LINE);
        WriteInt32(PointOffset(iFigure), iPoint);
        WriteSimpleCurve(poGeom->toSimpleCurve());
        ++iFigure;
        break;

    case wkbCircularString:
        WriteByte(FigureAttribute(iFigure), FA_ARC);
        WriteInt32(PointOffset(iFigure), iPoint);
        WriteSimpleCurve(poGeom->toSimpleCurve());
        ++iFigure;
        break;

    case wkbCompoundCurve:
        WriteCompoundCurve(poGeom->toCompoundCurve());
        ++iFigure;
        break;

    default:
        break;
    }
}

/************************************************************************/
/*                         WritePolygon()                               */
/************************************************************************/

void OGRMSSQLGeometryWriter::WritePolygon(OGRPolygon* poGeom)
{
    int r;  
    OGRLinearRing *poRing = poGeom->getExteriorRing();

    if (poRing == nullptr)
        return;

    if (chVersion == VA_KATMAI)
        WriteByte(FigureAttribute(iFigure), FA_EXTERIORRING);
    else
        WriteByte(FigureAttribute(iFigure), FA_LINE);

    WriteInt32(PointOffset(iFigure), iPoint);
    WriteSimpleCurve(poRing);
    ++iFigure;
    for (r = 0; r < poGeom->getNumInteriorRings(); r++)
    {
        /* write interior rings */
        poRing = poGeom->getInteriorRing(r);
        if (chVersion == VA_KATMAI)
            WriteByte(FigureAttribute(iFigure), FA_INTERIORRING);
        else
            WriteByte(FigureAttribute(iFigure), FA_LINE);

        WriteInt32(PointOffset(iFigure), iPoint);
        WriteSimpleCurve(poRing);
        ++iFigure;
    }
}

/************************************************************************/
/*                         WriteCurvePolygon()                          */
/************************************************************************/

void OGRMSSQLGeometryWriter::WriteCurvePolygon(OGRCurvePolygon* poGeom)
{
    if (poGeom->getExteriorRingCurve() == nullptr)
        return;

    WriteCurve(poGeom->getExteriorRingCurve());
    for (int r = 0; r < poGeom->getNumInteriorRings(); r++)
    {
        /* write interior rings */
        WriteCurve(poGeom->getInteriorRingCurve(r));
    }
}

/************************************************************************/
/*                         WriteGeometryCollection()                    */
/************************************************************************/

void OGRMSSQLGeometryWriter::WriteGeometryCollection(OGRGeometryCollection* poGeom, int iParent)
{
    for (int i = 0; i < poGeom->getNumGeometries(); i++)
        WriteGeometry(poGeom->getGeometryRef(i), iParent);
}

/************************************************************************/
/*                         WriteGeometry()                              */
/************************************************************************/

void OGRMSSQLGeometryWriter::WriteGeometry(OGRGeometry* poGeom, int iParent)
{
    /* write shape */
    int iCurrentFigure = iFigure;
    int iCurrentShape = iShape;
    WriteInt32(ParentOffset(iShape), iParent);

    iParent = iShape;

    switch (wkbFlatten(poGeom->getGeometryType()))
    {
    case wkbPoint:
        WriteByte(ShapeType(iShape++), ST_POINT);
        if (!poGeom->IsEmpty())
        {
            if (chVersion == VA_KATMAI)
                WriteByte(FigureAttribute(iFigure), FA_STROKE);
            else
                WriteByte(FigureAttribute(iFigure), FA_LINE);
            WriteInt32(PointOffset(iFigure), iPoint);
            WritePoint(poGeom->toPoint());
            ++iFigure;
        }
        break;

    case wkbLineString:
        WriteByte(ShapeType(iShape++), ST_LINESTRING);
        if (!poGeom->IsEmpty())
        {
            if (chVersion == VA_KATMAI)
                WriteByte(FigureAttribute(iFigure), FA_STROKE);
            else
                WriteByte(FigureAttribute(iFigure), FA_LINE);
            WriteInt32(PointOffset(iFigure), iPoint);
            WriteSimpleCurve(poGeom->toSimpleCurve());
            ++iFigure;
        }
        break;

    case wkbCircularString:
        WriteByte(ShapeType(iShape++), ST_CIRCULARSTRING);
        if (!poGeom->IsEmpty())
        {
            if (chVersion == VA_KATMAI)
                WriteByte(FigureAttribute(iFigure), FA_STROKE);
            else
                WriteByte(FigureAttribute(iFigure), FA_ARC);
            WriteInt32(PointOffset(iFigure), iPoint);
            WriteSimpleCurve(poGeom->toSimpleCurve());
            ++iFigure;
        }
        break;

    case wkbCompoundCurve:
        WriteByte(ShapeType(iShape++), ST_COMPOUNDCURVE);
        if (!poGeom->IsEmpty())
        {
            WriteCompoundCurve(poGeom->toCompoundCurve());
            ++iFigure;
        }
        break;

    case wkbPolygon:
        WriteByte(ShapeType(iShape++), ST_POLYGON);
        WritePolygon(poGeom->toPolygon());
        break;

    case wkbCurvePolygon:
        WriteByte(ShapeType(iShape++), ST_CURVEPOLYGON);
        WriteCurvePolygon(poGeom->toCurvePolygon());
        break;

    case wkbMultiPoint:
        WriteByte(ShapeType(iShape++), ST_MULTIPOINT);
        WriteGeometryCollection(poGeom->toGeometryCollection(), iParent);
        break;

    case wkbMultiLineString:
        WriteByte(ShapeType(iShape++), ST_MULTILINESTRING);
        WriteGeometryCollection(poGeom->toGeometryCollection(), iParent);
        break;

    case wkbMultiPolygon:
        WriteByte(ShapeType(iShape++), ST_MULTIPOLYGON);
        WriteGeometryCollection(poGeom->toGeometryCollection(), iParent);
        break;

    case wkbGeometryCollection:
        WriteByte(ShapeType(iShape++), ST_GEOMETRYCOLLECTION);
        WriteGeometryCollection(poGeom->toGeometryCollection(), iParent);
        break;

    default:
        return;
    }
    // check if new figures have been added to shape
    WriteInt32(FigureOffset(iCurrentShape), iFigure == iCurrentFigure? 0xFFFFFFFF : iCurrentFigure);
}

/************************************************************************/
/*                         TrackGeometry()                              */
/************************************************************************/

void OGRMSSQLGeometryWriter::TrackGeometry(OGRGeometry* poGeom)
{
    switch (wkbFlatten(poGeom->getGeometryType()))
    {
    case wkbPoint:
        if (!poGeom->IsEmpty())
        {
            ++nNumFigures;
            ++nNumPoints;
        }
        break;

    case wkbLineString:
        if (!poGeom->IsEmpty())
        {
            ++nNumFigures;
            nNumPoints += poGeom->toLineString()->getNumPoints();
        }
        break;

    case wkbCircularString:
        chVersion = VA_DENALI;
        if (!poGeom->IsEmpty())
        {
            ++nNumFigures;
            nNumPoints += poGeom->toCircularString()->getNumPoints();
        }
        break;

    case wkbCompoundCurve:
        {
            int c;
            chVersion = VA_DENALI;
            if (!poGeom->IsEmpty())
            {
                OGRCompoundCurve* g = poGeom->toCompoundCurve();
                OGRCurve* poSubGeom;
                ++nNumFigures;
                for (int i = 0; i < g->getNumCurves(); i++)
                {
                    poSubGeom = g->getCurve(i);
                    switch (wkbFlatten(poSubGeom->getGeometryType()))
                    {
                    case wkbLineString:
                        c = poSubGeom->toLineString()->getNumPoints();
                        if (c > 1)
                        {
                            if (i == 0)
                                nNumPoints += c;
                            else
                                nNumPoints += c - 1;
                            nNumSegments += c - 1;
                        }
                        break;

                    case wkbCircularString:
                        c = poSubGeom->toCircularString()->getNumPoints();
                        if (c > 2)
                        {
                            if (i == 0)
                                nNumPoints += c;
                            else
                                nNumPoints += c - 1;
                            nNumSegments += (int)((c - 1) / 2);
                        }
                        break;

                    default:
                        break;
                    }
                }
            }
        }
        break;

    case wkbPolygon:
        {
            OGRPolygon* g = poGeom->toPolygon();
            for( auto&& poIter: *g )
                TrackGeometry(poIter);
        }
        break;

    case wkbCurvePolygon:
        {
            chVersion = VA_DENALI;
            OGRCurvePolygon* g = poGeom->toCurvePolygon();
            for (auto&& poIter : *g)
                TrackGeometry(poIter);
        }
        break;

    case wkbMultiPoint:
    case wkbMultiLineString:
    case wkbMultiPolygon:
    case wkbGeometryCollection:
        {
            OGRGeometryCollection* g = poGeom->toGeometryCollection();
            for( auto&& poMember: *g )
            {
                TrackGeometry(poMember);
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

    OGRwkbGeometryType geomType = wkbFlatten(poGeom2->getGeometryType());

    if (nNumPoints == 1 && geomType == wkbPoint)
    {
        /* writing a single point */
        OGRPoint* g = poGeom2->toPoint();
        WriteInt32(0, nSRSId);
        WriteByte(4, VA_KATMAI);
        WriteByte(5, chProps);
        WritePoint(g);
    }
    else if (nNumPoints == 2 && geomType == wkbLineString)
    {
        /* writing a single line */
        OGRLineString* g = poGeom2->toLineString();
        WriteInt32(0, nSRSId);
        WriteByte(4, VA_KATMAI);
        WriteByte(5, chProps);

        if ((chProps & SP_HASZVALUES) && (chProps & SP_HASMVALUES))
        {
            WritePoint(g->getX(0), g->getY(0), g->getZ(0), g->getM(0));
            WritePoint(g->getX(1), g->getY(1), g->getZ(1), g->getM(1));
        }
        else if (chProps & SP_HASZVALUES)
        {
            WritePoint(g->getX(0), g->getY(0), g->getZ(0));
            WritePoint(g->getX(1), g->getY(1), g->getZ(1));
        }
        else if (chProps & SP_HASMVALUES)
        {
            WritePoint(g->getX(0), g->getY(0), g->getM(0));
            WritePoint(g->getX(1), g->getY(1), g->getM(1));
        }
        else
        {
            WritePoint(g->getX(0), g->getY(0));
            WritePoint(g->getX(1), g->getY(1));
        }
    }
    else
    {
        /* complex geometry */
        if (poGeom2->IsValid())
            chProps |= SP_ISVALID;

        WriteInt32(0, nSRSId);
        WriteByte(4, chVersion);
        WriteByte(5, chProps);
        WriteInt32(nPointPos - 4 , nNumPoints);
        WriteInt32(nFigurePos - 4 , nNumFigures);
        WriteInt32(nShapePos - 4 , nNumShapes);
        if (nNumSegments > 0)
            WriteInt32(nSegmentPos - 4, nNumSegments);

        WriteGeometry(poGeom2, 0xFFFFFFFF);
    }
    return OGRERR_NONE;
}
