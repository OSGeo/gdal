/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCurve geometry class. 
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
 * Revision 1.3  1999/05/17 14:39:46  warmerda
 * Added various new methods for ICurve compatibility.
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
/*                              OGRCurve()                              */
/************************************************************************/

OGRCurve::OGRCurve()

{
    nPointCount = 0;
    paoPoints = NULL;
}

/************************************************************************/
/*                             ~OGRCurve()                              */
/************************************************************************/

OGRCurve::~OGRCurve()

{
    if( paoPoints != NULL )
        OGRFree( paoPoints );
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRCurve::getDimension()

{
    return 1;
}

/************************************************************************/
/*                       getCoordinateDimension()                       */
/************************************************************************/

int OGRCurve::getCoordinateDimension()

{
    return 2;
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRCurve::WkbSize()

{
    return 5 + 4 + 16 * nPointCount;
}

/************************************************************************/
/*                              getPoint()                              */
/************************************************************************/

void	OGRCurve::getPoint( int i, OGRPoint * poPoint )

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

void OGRCurve::setNumPoints( int nNewPointCount )

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

        memset( paoPoints + sizeof(OGRRawPoint) * nPointCount,
                0, sizeof(OGRRawPoint) * (nNewPointCount - nPointCount) );
    }

    nPointCount = nNewPointCount;
}

/************************************************************************/
/*                              setPoint()                              */
/************************************************************************/

void OGRCurve::setPoint( int iPoint, OGRPoint * poPoint )

{
    setPoint( iPoint, poPoint->getX(), poPoint->getY() );
}

/************************************************************************/
/*                              setPoint()                              */
/************************************************************************/

void OGRCurve::setPoint( int iPoint, double xIn, double yIn )

{
    if( iPoint >= nPointCount )
        setNumPoints( iPoint+1 );

    paoPoints[iPoint].x = xIn;
    paoPoints[iPoint].y = yIn;
}

/************************************************************************/
/*                              addPoint()                              */
/************************************************************************/

void OGRCurve::addPoint( OGRPoint * poPoint )

{
    setPoint( nPointCount, poPoint->getX(), poPoint->getY() );
}

/************************************************************************/
/*                              addPoint()                              */
/************************************************************************/

void OGRCurve::addPoint( double x, double y )

{
    setPoint( nPointCount, x, y );
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRCurve::importFromWkb( unsigned char * pabyData,
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

OGRErr	OGRCurve::exportToWkb( OGRwkbByteOrder eByteOrder,
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
/*                         dumpPointsReadable()                         */
/*                                                                      */
/*      Helper function for the derived classes dumpReadable()          */
/*      methods.                                                        */
/************************************************************************/

void OGRCurve::dumpPointsReadable( FILE * fp, const char * pszPrefix )

{
    fprintf( fp, "%s  nPointCount = %d\n", pszPrefix, nPointCount );
    for( int i = 0; i < nPointCount; i++ )
        fprintf( fp, "%s  Point[%d] = (%g,%g)\n",
                 pszPrefix, i, paoPoints[i].x, paoPoints[i].y );
}

/************************************************************************/
/*                             get_Length()                             */
/*                                                                      */
/*      For now we return a simple euclidian 2D distance.               */
/************************************************************************/

double OGRCurve::get_Length()

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

void OGRCurve::StartPoint( OGRPoint * poPoint )

{
    getPoint( 0, poPoint );
}

/************************************************************************/
/*                              EndPoint()                              */
/************************************************************************/

void OGRCurve::EndPoint( OGRPoint * poPoint )

{
    getPoint( nPointCount-1, poPoint );
}

/************************************************************************/
/*                            get_IsClosed()                            */
/************************************************************************/

int OGRCurve::get_IsClosed()

{
    return( nPointCount > 1 
            && paoPoints[0].x == paoPoints[nPointCount-1].x
            && paoPoints[0].y == paoPoints[nPointCount-1].y );
}

/************************************************************************/
/*                               Value()                                */
/*                                                                      */
/*      Get an interpolated point at some distance along the curve.     */
/************************************************************************/

void OGRCurve::Value( double dfDistance, OGRPoint * poPoint )

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


