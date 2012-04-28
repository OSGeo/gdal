/******************************************************************************
 * $Id$
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Implements OGRMSSQLGeometryParser class to parse native SqlGeometries.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
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

/************************************************************************/
/*                         Geometry parser macros                       */
/************************************************************************/

#define SP_NONE 0
#define SP_HASZVALUES 1
#define SP_HASMVALUES 2
#define SP_ISVALID 4
#define SP_ISSINGLEPOINT 8
#define SP_ISSINGLELINESEGMENT 0x10
#define SP_ISWHOLEGLOBE 0x20

#define ST_UNKNOWN 0
#define ST_POINT 1
#define ST_LINESTRING 2
#define ST_POLYGON 3
#define ST_MULTIPOINT 4
#define ST_MULTILINESTRING 5
#define ST_MULTIPOLYGON 6
#define ST_GEOMETRYCOLLECTION 7

#define ReadInt32(nPos) (*((unsigned int*)(pszData + (nPos))))

#define ReadByte(nPos) (pszData[nPos])

#define ReadDouble(nPos) (*((double*)(pszData + (nPos))))

#define ParentOffset(iShape) (ReadInt32(nShapePos + (iShape) * 9 ))
#define FigureOffset(iShape) (ReadInt32(nShapePos + (iShape) * 9 + 4))
#define ShapeType(iShape) (ReadByte(nShapePos + (iShape) * 9 + 8))

#define NextFigureOffset(iShape) (iShape + 1 < nNumShapes? FigureOffset((iShape) +1) : nNumFigures)

#define FigureAttribute(iFigure) (ReadByte(nFigurePos + (iFigure) * 5))
#define PointOffset(iFigure) (ReadInt32(nFigurePos + (iFigure) * 5 + 1))
#define NextPointOffset(iFigure) (iFigure + 1 < nNumFigures? PointOffset((iFigure) +1) : nNumPoints)

#define ReadX(iPoint) (ReadDouble(nPointPos + nPointSize * (iPoint)))
#define ReadY(iPoint) (ReadDouble(nPointPos + nPointSize * (iPoint) + 8))
#define ReadZ(iPoint) (ReadDouble(nPointPos + nPointSize * (iPoint) + 16))

/************************************************************************/
/*                   OGRMSSQLGeometryParser()                           */
/************************************************************************/

OGRMSSQLGeometryParser::OGRMSSQLGeometryParser(int nGeomColumnType)
{
    nColType = nGeomColumnType;
}

/************************************************************************/
/*                         ReadPoint()                                  */
/************************************************************************/

OGRPoint* OGRMSSQLGeometryParser::ReadPoint(int iShape)
{
    int iFigure = FigureOffset(iShape);
    if ( iFigure < nNumFigures )
    {
        int iPoint = PointOffset(iFigure);
        if ( iPoint < nNumPoints )
        {
            if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
            {
                if ( chProps & SP_HASZVALUES )
                    return new OGRPoint( ReadY(iPoint), ReadX(iPoint), ReadZ(iPoint) );
                else
                    return new OGRPoint( ReadY(iPoint), ReadX(iPoint) );
            }
            else
            {
                if ( chProps & SP_HASZVALUES )
                    return new OGRPoint( ReadX(iPoint), ReadY(iPoint), ReadZ(iPoint) );
                else
                    return new OGRPoint( ReadX(iPoint), ReadY(iPoint) );
            }
        }
    }
    return NULL;
}

/************************************************************************/
/*                         ReadMultiPoint()                             */
/************************************************************************/

OGRMultiPoint* OGRMSSQLGeometryParser::ReadMultiPoint(int iShape)
{
    int iFigure, iPoint, iNextPoint;
    iFigure = FigureOffset(iShape);

    OGRMultiPoint* poMultiPoint = new OGRMultiPoint();
    iNextPoint = NextPointOffset(iFigure);
    for (iPoint = PointOffset(iFigure); iPoint < iNextPoint; iPoint++)
    {
        OGRPoint* poPoint;
        
        if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
        {
            if ( chProps & SP_HASZVALUES )
                poPoint = new OGRPoint( ReadY(iPoint), ReadX(iPoint), ReadZ(iPoint) );
            else
                poPoint = new OGRPoint( ReadY(iPoint), ReadX(iPoint) );
        }
        else
        {
            if ( chProps & SP_HASZVALUES )
                poPoint = new OGRPoint( ReadX(iPoint), ReadY(iPoint), ReadZ(iPoint) );
            else
                poPoint = new OGRPoint( ReadX(iPoint), ReadY(iPoint) );
        }

        if ( poPoint )
            poMultiPoint->addGeometryDirectly( poPoint );
    }

    return poMultiPoint;
}

/************************************************************************/
/*                         ReadLineString()                             */
/************************************************************************/

OGRLineString* OGRMSSQLGeometryParser::ReadLineString(int iShape)
{
    int iFigure, iPoint, iNextPoint, i;
    iFigure = FigureOffset(iShape);

    OGRLineString* poLineString = new OGRLineString();
    iPoint = PointOffset(iFigure);
    iNextPoint = NextPointOffset(iFigure);
    poLineString->setNumPoints(iNextPoint - iPoint);
    i = 0;
    while (iPoint < iNextPoint)
    {
        if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
        {
            if ( chProps & SP_HASZVALUES )
                poLineString->setPoint(i, ReadY(iPoint), ReadX(iPoint), ReadZ(iPoint) );
            else
                poLineString->setPoint(i, ReadY(iPoint), ReadX(iPoint) );
        }
        else
        {
            if ( chProps & SP_HASZVALUES )
                poLineString->setPoint(i, ReadX(iPoint), ReadY(iPoint), ReadZ(iPoint) );
            else
                poLineString->setPoint(i, ReadX(iPoint), ReadY(iPoint) );
        }
        
        ++iPoint;
        ++i;
    }

    return poLineString;
}

/************************************************************************/
/*                         ReadMultiLineString()                        */
/************************************************************************/

OGRMultiLineString* OGRMSSQLGeometryParser::ReadMultiLineString(int iShape)
{
    int i;
    OGRMultiLineString* poMultiLineString = new OGRMultiLineString();
    OGRGeometry* poGeom;

    for (i = iShape + 1; i < nNumShapes; i++)
    {
        poGeom = NULL;
        if (ParentOffset(i) == (unsigned int)iShape)
        {
            if  ( ShapeType(i) == ST_LINESTRING )
                poGeom = ReadLineString(i);
        }
        if ( poGeom )
            poMultiLineString->addGeometryDirectly( poGeom );
    }

    return poMultiLineString;
}

/************************************************************************/
/*                         ReadPolygon()                                */
/************************************************************************/

OGRPolygon* OGRMSSQLGeometryParser::ReadPolygon(int iShape)
{
    int iFigure, iPoint, iNextPoint, i;
    int iNextFigure = NextFigureOffset(iShape);
    
    OGRPolygon* poPoly = new OGRPolygon();
    for (iFigure = FigureOffset(iShape); iFigure < iNextFigure; iFigure++)
    {
        OGRLinearRing* poRing = new OGRLinearRing();
        iPoint = PointOffset(iFigure);
        iNextPoint = NextPointOffset(iFigure);
        poRing->setNumPoints(iNextPoint - iPoint);
        i = 0;
        while (iPoint < iNextPoint)
        {
            if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
            {  
                if ( chProps & SP_HASZVALUES )
                    poRing->setPoint(i, ReadY(iPoint), ReadX(iPoint), ReadZ(iPoint) );
                else
                    poRing->setPoint(i, ReadY(iPoint), ReadX(iPoint) );
            }
            else
            {
                if ( chProps & SP_HASZVALUES )
                    poRing->setPoint(i, ReadX(iPoint), ReadY(iPoint), ReadZ(iPoint) );
                else
                    poRing->setPoint(i, ReadX(iPoint), ReadY(iPoint) );
            }

            ++iPoint;
            ++i;
        }
        poPoly->addRingDirectly( poRing );
    }
    return poPoly;
}

/************************************************************************/
/*                         ReadMultiPolygon()                           */
/************************************************************************/

OGRMultiPolygon* OGRMSSQLGeometryParser::ReadMultiPolygon(int iShape)
{
    int i;
    OGRMultiPolygon* poMultiPolygon = new OGRMultiPolygon();
    OGRGeometry* poGeom;

    for (i = iShape + 1; i < nNumShapes; i++)
    {
        poGeom = NULL;
        if (ParentOffset(i) == (unsigned int)iShape)
        {
            if ( ShapeType(i) == ST_POLYGON )
                poGeom = ReadPolygon(i);
        }
        if ( poGeom )
            poMultiPolygon->addGeometryDirectly( poGeom );
    }

    return poMultiPolygon;
}

/************************************************************************/
/*                         ReadGeometryCollection()                     */
/************************************************************************/

OGRGeometryCollection* OGRMSSQLGeometryParser::ReadGeometryCollection(int iShape)
{
    int i;
    OGRGeometryCollection* poGeomColl = new OGRGeometryCollection();
    OGRGeometry* poGeom;

    for (i = iShape + 1; i < nNumShapes; i++)
    {
        poGeom = NULL;
        if (ParentOffset(i) == (unsigned int)iShape)
        {
            switch (ShapeType(i))
            {
            case ST_POINT:
                poGeom = ReadPoint(i);
                break;
            case ST_LINESTRING:
                poGeom = ReadLineString(i);
                break;
            case ST_POLYGON:
                poGeom = ReadPolygon(i);
                break;
            case ST_MULTIPOINT:
                poGeom = ReadMultiPoint(i);
                break;
            case ST_MULTILINESTRING:
                poGeom = ReadMultiLineString(i);
                break;
            case ST_MULTIPOLYGON:
                poGeom = ReadMultiPolygon(i);
                break;
            case ST_GEOMETRYCOLLECTION:
                poGeom = ReadGeometryCollection(i);
                break;
            }
        }
        if ( poGeom )
            poGeomColl->addGeometryDirectly( poGeom );
    }

    return poGeomColl;
}

/************************************************************************/
/*                         ParseSqlGeometry()                           */
/************************************************************************/


OGRErr OGRMSSQLGeometryParser::ParseSqlGeometry(unsigned char* pszInput, 
                                int nLen, OGRGeometry **poGeom)
{
    if (nLen < 10)
        return OGRERR_NOT_ENOUGH_DATA;

    pszData = pszInput;
    
    /* store the SRS id for further use */
    nSRSId = ReadInt32(0);
    
    if ( ReadByte(4) != 1 )
    {
        return OGRERR_CORRUPT_DATA;
    }

    chProps = ReadByte(5);

    if ( chProps & SP_HASMVALUES )
        nPointSize = 32;
    else if ( chProps & SP_HASZVALUES )
        nPointSize = 24;
    else
        nPointSize = 16;

    if ( chProps & SP_ISSINGLEPOINT )
    {
        // single point geometry
        nPointPos = 6;

        if (nLen < 6 + nPointSize)
        {
            return OGRERR_NOT_ENOUGH_DATA;
        }

        if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
        {
            if (chProps & SP_HASZVALUES)
                *poGeom = new OGRPoint(ReadY(0), ReadX(0), ReadZ(0));
            else
                *poGeom = new OGRPoint(ReadY(0), ReadX(0));
        }
        else
        {
            if (chProps & SP_HASZVALUES)
                *poGeom = new OGRPoint(ReadX(0), ReadY(0), ReadZ(0));
            else
                *poGeom = new OGRPoint(ReadX(0), ReadY(0));
        }
    }
    else if ( chProps & SP_ISSINGLELINESEGMENT )
    {
        // single line segment with 2 points
        nPointPos = 6;

        if (nLen < 6 + 2 * nPointSize)
        {
            return OGRERR_NOT_ENOUGH_DATA;
        }

        OGRLineString* line = new OGRLineString();
        line->setNumPoints(2);
        
        if (nColType == MSSQLCOLTYPE_GEOGRAPHY)
        {
            if ( chProps & SP_HASZVALUES )
            {
                line->setPoint(0, ReadY(0), ReadX(0), ReadZ(0));
                line->setPoint(1, ReadY(1), ReadX(1), ReadZ(1));
            }
            else
            {
                line->setPoint(0, ReadY(0), ReadX(0));
                line->setPoint(1, ReadY(1), ReadX(1));
            }
        }
        else
        {
            if ( chProps & SP_HASZVALUES )
            {
                line->setPoint(0, ReadX(0), ReadY(0), ReadZ(0));
                line->setPoint(1, ReadX(1), ReadY(1), ReadZ(1));
            }
            else
            {
                line->setPoint(0, ReadX(0), ReadY(0));
                line->setPoint(1, ReadX(1), ReadY(1));
            }
        }
        
        *poGeom = line;
    }
    else
    {
        // complex geometries
        nNumPoints = ReadInt32(6);

        if ( nNumPoints <= 0 )
        {
            return OGRERR_NONE;
        }

        // position of the point array
        nPointPos = 10;

        // position of the figures
        nFigurePos = nPointPos + nPointSize * nNumPoints + 4;
        
        if (nLen < nFigurePos)
        {
            return OGRERR_NOT_ENOUGH_DATA;
        }

        nNumFigures = ReadInt32(nFigurePos - 4);

        if ( nNumFigures <= 0 )
        {
            return OGRERR_NONE;
        }
        
        // position of the shapes
        nShapePos = nFigurePos + 5 * nNumFigures + 4;

        if (nLen < nShapePos)
        {
            return OGRERR_NOT_ENOUGH_DATA;
        }

        nNumShapes = ReadInt32(nShapePos - 4);

        if (nLen < nShapePos + 9 * nNumShapes)
        {
            return OGRERR_NOT_ENOUGH_DATA;
        }

        if ( nNumShapes <= 0 )
        {
            return OGRERR_NONE;
        }

        // pick up the root shape
        if ( ParentOffset(0) != 0xFFFFFFFF)
        {
            return OGRERR_CORRUPT_DATA;
        }

        // determine the shape type
        switch (ShapeType(0))
        {
        case ST_POINT:
            *poGeom = ReadPoint(0);
            break;
        case ST_LINESTRING:
            *poGeom = ReadLineString(0);
            break;
        case ST_POLYGON:
            *poGeom = ReadPolygon(0);
            break;
        case ST_MULTIPOINT:
            *poGeom = ReadMultiPoint(0);
            break;
        case ST_MULTILINESTRING:
            *poGeom = ReadMultiLineString(0);
            break;
        case ST_MULTIPOLYGON:
            *poGeom = ReadMultiPolygon(0);
            break;
        case ST_GEOMETRYCOLLECTION:
            *poGeom = ReadGeometryCollection(0);
            break;
        default:
            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }
    }

    return OGRERR_NONE;
}

