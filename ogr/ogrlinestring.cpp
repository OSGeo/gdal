/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRLineString geometry class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.5  1999/05/31 15:01:59  warmerda
 * OGRCurve now an abstract base class with essentially no implementation.
 * Everything moved down to OGRLineString where it belongs.  Also documented
 * classes.
 *
 * Revision 1.4  1999/05/23 05:34:40  warmerda
 * added support for clone(), multipolygons and geometry collections
 *
 * Revision 1.3  1999/05/20 14:35:44  warmerda
 * added support for well known text format
 *
 * Revision 1.2  1999/03/30 21:21:43  warmerda
 * added linearring/polygon support
 *
 * Revision 1.1  1999/03/29 21:21:10  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <assert.h>

/************************************************************************/
/*                           OGRLineString()                            */
/************************************************************************/

/**
 * Create an empty line string.
 */

OGRLineString::OGRLineString()

{
    nPointCount = 0;
    paoPoints = NULL;
}

/************************************************************************/
/*                           ~OGRLineString()                           */
/************************************************************************/

OGRLineString::~OGRLineString()

{
    if( paoPoints != NULL )
        OGRFree( paoPoints );
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRLineString::getGeometryType()

{
    return wkbLineString;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRLineString::getGeometryName()

{
    return "LINESTRING";
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRLineString::clone()

{
    OGRLineString	*poNewLineString;

    poNewLineString = new OGRLineString();

    poNewLineString->setPoints( nPointCount, paoPoints );

    return poNewLineString;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRLineString::getDimension()

{
    return 1;
}

/************************************************************************/
/*                       getCoordinateDimension()                       */
/************************************************************************/

int OGRLineString::getCoordinateDimension()

{
    return 2;
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRLineString::WkbSize()

{
    return 5 + 4 + 16 * nPointCount;
}

/************************************************************************/
/*                              getPoint()                              */
/************************************************************************/

/**
 * Fetch a point in line string.
 *
 * This method relates to the SFCOM ILineString::get_Point() method.
 *
 * @param i the vertex to fetch, from 0 to getNumPoints()-1.
 * @param poPoint a point to initialize with the fetched point.
 */

void	OGRLineString::getPoint( int i, OGRPoint * poPoint )

{
    assert( i >= 0 );
    assert( i < nPointCount );
    assert( poPoint != NULL );

    poPoint->setX( paoPoints[i].x );
    poPoint->setY( paoPoints[i].y );
}

/************************************************************************/
/*                            setNumPoints()                            */
/************************************************************************/

/**
 * Set number of points in geometry.
 *
 * This method primary exists to preset the number of points in a linestring
 * geometry before setPoint() is used to assign them to avoid reallocating
 * the array larger with each call to addPoint(). 
 *
 * This method has no SFCOM analog.
 *
 * @param nNewPointCount the new number of points for geometry.
 */

void OGRLineString::setNumPoints( int nNewPointCount )

{
    if( nNewPointCount == 0 )
    {
        if( paoPoints != NULL )
            OGRFree( paoPoints );
        paoPoints = NULL;
        nPointCount = nNewPointCount;
        return;
    }

    if( nNewPointCount > nPointCount )
    {
        paoPoints = (OGRRawPoint *)
            OGRRealloc(paoPoints, sizeof(OGRRawPoint) * nNewPointCount);

        assert( paoPoints != NULL );

        memset( paoPoints + nPointCount,
                0, sizeof(OGRRawPoint) * (nNewPointCount - nPointCount) );
    }

    nPointCount = nNewPointCount;
}

/************************************************************************/
/*                              setPoint()                              */
/************************************************************************/

/**
 * Set the location of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accomodate the request.
 *
 * There is no SFCOM analog to this method.
 * 
 * @param iPoint the index of the vertex to assign (zero based).
 * @param poPoint the value to assign to the vertex.
 */

void OGRLineString::setPoint( int iPoint, OGRPoint * poPoint )

{
    setPoint( iPoint, poPoint->getX(), poPoint->getY() );
}

/************************************************************************/
/*                              setPoint()                              */
/************************************************************************/

/**
 * Set the location of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accomodate the request.
 * 
 * There is no SFCOM analog to this method.
 *
 * @param iPoint the index of the vertex to assign (zero based).
 * @param xIn input X coordinate to assign.
 * @param yIn input Y coordinate to assign.
 */

void OGRLineString::setPoint( int iPoint, double xIn, double yIn )

{
    if( iPoint >= nPointCount )
        setNumPoints( iPoint+1 );

    paoPoints[iPoint].x = xIn;
    paoPoints[iPoint].y = yIn;
}

/************************************************************************/
/*                              addPoint()                              */
/************************************************************************/

/**
 * Add a point to a line string.
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * There is no SFCOM analog to this method.
 *
 * @param poPoint the point to assign to the new vertex.
 */

void OGRLineString::addPoint( OGRPoint * poPoint )

{
    setPoint( nPointCount, poPoint->getX(), poPoint->getY() );
}

/************************************************************************/
/*                              addPoint()                              */
/************************************************************************/

/**
 * Add a point to a line string.
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * There is no SFCOM analog to this method.
 *
 * @param x the X coordinate to assign to the new point.
 * @param y the Y coordinate to assign to the new point.
 */

void OGRLineString::addPoint( double x, double y )

{
    setPoint( nPointCount, x, y );
}

/************************************************************************/
/*                             setPoints()                              */
/************************************************************************/

/**
 * Assign all points in a line string.
 *
 * This method clear any existing points assigned to this line string,
 * and assigns a whole new set.  It is the most efficient way of assigning
 * the value of a line string.
 *
 * There is no SFCOM analog to this method.
 *
 * @param nPointsIn number of points being passed in paoPointsIn
 * @param paoPointsIn list of points being assigned.
 */

void OGRLineString::setPoints( int nPointsIn, OGRRawPoint * paoPointsIn )

{
    setNumPoints( nPointsIn );
    memcpy( paoPoints, paoPointsIn, sizeof(OGRRawPoint) * nPointsIn);
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRLineString::importFromWkb( unsigned char * pabyData,
                                int nBytesAvailable )

{
    OGRwkbByteOrder	eByteOrder;
    
    if( nBytesAvailable < 21 && nBytesAvailable != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.                                        */
/* -------------------------------------------------------------------- */
    eByteOrder = (OGRwkbByteOrder) *pabyData;
    assert( eByteOrder == wkbXDR || eByteOrder == wkbNDR );

/* -------------------------------------------------------------------- */
/*      Get the geometry feature type.  For now we assume that          */
/*      geometry type is between 0 and 255 so we only have to fetch     */
/*      one byte.                                                       */
/* -------------------------------------------------------------------- */
#ifdef DEBUG
    OGRwkbGeometryType eGeometryType;
    
    if( eByteOrder == wkbNDR )
        eGeometryType = (OGRwkbGeometryType) pabyData[1];
    else
        eGeometryType = (OGRwkbGeometryType) pabyData[4];

    assert( eGeometryType == wkbLineString );
#endif    

/* -------------------------------------------------------------------- */
/*      Get the vertex count.                                           */
/* -------------------------------------------------------------------- */
    int		nNewNumPoints;
    
    memcpy( &nNewNumPoints, pabyData + 5, 4 );
    
    if( OGR_SWAP( eByteOrder ) )
        nNewNumPoints = CPL_SWAP32(nNewNumPoints);

    setNumPoints( nNewNumPoints );
    
/* -------------------------------------------------------------------- */
/*      Get the vertex.                                                 */
/* -------------------------------------------------------------------- */
    memcpy( paoPoints, pabyData + 9, 16 * nPointCount );
    
    if( OGR_SWAP( eByteOrder ) )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            CPL_SWAPDOUBLE( &(paoPoints[i].x) );
            CPL_SWAPDOUBLE( &(paoPoints[i].y) );
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr	OGRLineString::exportToWkb( OGRwkbByteOrder eByteOrder,
                               unsigned char * pabyData )

{
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = (unsigned char) eByteOrder;

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    if( eByteOrder == wkbNDR )
    {
        pabyData[1] = wkbLineString;
        pabyData[2] = 0;
        pabyData[3] = 0;
        pabyData[4] = 0;
    }
    else
    {
        pabyData[1] = 0;
        pabyData[2] = 0;
        pabyData[3] = 0;
        pabyData[4] = wkbLineString;
    }
    
/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    memcpy( pabyData+5, &nPointCount, 4 );

/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    memcpy( pabyData+9, paoPoints, 16 * nPointCount );

/* -------------------------------------------------------------------- */
/*      Swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        int	nCount;

        nCount = CPL_SWAP32( nPointCount );
        memcpy( pabyData+5, &nCount, 4 );

        for( int i = 0; i < nPointCount; i++ )
        {
            CPL_SWAPDOUBLE( pabyData + 9 + 16 * i );
            CPL_SWAPDOUBLE( pabyData + 9 + 16 * i + 8 );
        }
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.  Currently this is     */
/*      `LINESTRING ( x y, x y, ...)',                                  */
/************************************************************************/

OGRErr OGRLineString::importFromWkt( char ** ppszInput )

{
    char	szToken[OGR_WKT_TOKEN_MAX];
    const char	*pszInput = *ppszInput;

    if( paoPoints != NULL )
    {
        nPointCount = 0;
        CPLFree( paoPoints );
    }

/* -------------------------------------------------------------------- */
/*      Read and verify the ``LINESTRING'' keyword token.               */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( !EQUAL(szToken,getGeometryName()) )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Read the point list which should consist of exactly one point.  */
/* -------------------------------------------------------------------- */
    int			nMaxPoint = 0;

    nPointCount = 0;

    pszInput = OGRWktReadPoints( pszInput, &paoPoints, &nMaxPoint,
                                 &nPointCount );
    if( pszInput == NULL )
        return OGRERR_CORRUPT_DATA;

    *ppszInput = (char *) pszInput;
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivelent.  This could be made alot more CPU efficient!        */
/************************************************************************/

OGRErr OGRLineString::exportToWkt( char ** ppszReturn )

{
    int		nMaxString = nPointCount * 16 * 2 + 20;

    *ppszReturn = (char *) VSIMalloc( nMaxString );
    if( *ppszReturn == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

    sprintf( *ppszReturn, "%s (", getGeometryName() );

    for( int i = 0; i < nPointCount; i++ )
    {
        assert( nMaxString > (int) strlen(*ppszReturn) + 32 );

        if( i > 0 )
            strcat( *ppszReturn, "," );

        strcat( *ppszReturn, OGRMakeWktCoordinate( paoPoints[i].x,
                                                   paoPoints[i].y ) );
    }

    strcat( *ppszReturn, ")" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             get_Length()                             */
/*                                                                      */
/*      For now we return a simple euclidian 2D distance.               */
/************************************************************************/

double OGRLineString::get_Length()

{
    double      dfLength = 0;
    int         i;

    for( i = 0; i < nPointCount-1; i++ )
    {
        double      dfDeltaX, dfDeltaY;

        dfDeltaX = paoPoints[i+1].x - paoPoints[i].x;
        dfDeltaY = paoPoints[i+1].y - paoPoints[i].y;
        dfLength += sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);
    }
    
    return dfLength;
}

/************************************************************************/
/*                             StartPoint()                             */
/************************************************************************/

void OGRLineString::StartPoint( OGRPoint * poPoint )

{
    getPoint( 0, poPoint );
}

/************************************************************************/
/*                              EndPoint()                              */
/************************************************************************/

void OGRLineString::EndPoint( OGRPoint * poPoint )

{
    getPoint( nPointCount-1, poPoint );
}

/************************************************************************/
/*                               Value()                                */
/*                                                                      */
/*      Get an interpolated point at some distance along the curve.     */
/************************************************************************/

void OGRLineString::Value( double dfDistance, OGRPoint * poPoint )

{
    double      dfLength = 0;
    int         i;

    if( dfDistance < 0 )
    {
        StartPoint( poPoint );
        return;
    }

    for( i = 0; i < nPointCount-1; i++ )
    {
        double      dfDeltaX, dfDeltaY, dfSegLength;

        dfDeltaX = paoPoints[i+1].x - paoPoints[i].x;
        dfDeltaY = paoPoints[i+1].y - paoPoints[i].y;
        dfSegLength = sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);

        if( dfLength <= dfDistance && dfLength + dfSegLength >= dfDistance )
        {
            double      dfRatio;

            dfRatio = (dfDistance - dfLength) / dfSegLength;

            poPoint->setX( paoPoints[i].x * (1 - dfRatio)
                           + paoPoints[i+1].x * dfRatio );
            poPoint->setY( paoPoints[i].y * (1 - dfRatio)
                           + paoPoints[i+1].y * dfRatio );
            return;
        }
    }
    
    EndPoint( poPoint );
}


