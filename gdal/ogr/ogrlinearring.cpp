/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRLinearRing geometry class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_geometry.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRLinearRing()                            */
/************************************************************************/

OGRLinearRing::OGRLinearRing()

{
}

/************************************************************************/
/*                  OGRLinearRing( const OGRLinearRing& )               */
/************************************************************************/

/**
 * \brief Copy constructor.
 * 
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 * 
 * @since GDAL 2.1
 */

OGRLinearRing::OGRLinearRing( const OGRLinearRing& other ) :
    OGRLineString( other )
{
}

/************************************************************************/
/*                          ~OGRLinearRing()                            */
/************************************************************************/

OGRLinearRing::~OGRLinearRing()

{
}

/************************************************************************/
/*                           OGRLinearRing()                            */
/************************************************************************/

OGRLinearRing::OGRLinearRing( OGRLinearRing * poSrcRing )

{
    if( poSrcRing == NULL )
    {
        CPLDebug( "OGR", "OGRLinearRing::OGRLinearRing(OGRLinearRing*poSrcRing) - passed in ring is NULL!" );
        return;
    }

    setNumPoints( poSrcRing->getNumPoints(), FALSE );

    memcpy( paoPoints, poSrcRing->paoPoints,
            sizeof(OGRRawPoint) * getNumPoints() );

    if( poSrcRing->padfZ )
    {
        Make3D();
        memcpy( padfZ, poSrcRing->padfZ, sizeof(double) * getNumPoints() );
    }
}

/************************************************************************/
/*                    operator=( const OGRLinearRing& )                 */
/************************************************************************/

/**
 * \brief Assignment operator.
 * 
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 * 
 * @since GDAL 2.1
 */

OGRLinearRing& OGRLinearRing::operator=( const OGRLinearRing& other )
{
    if( this != &other)
    {
        OGRLineString::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRLinearRing::getGeometryName() const 

{
    return "LINEARRING";
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Disable this method.                                            */
/************************************************************************/

int OGRLinearRing::WkbSize() const

{
    return 0;
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Disable method for this class.                                  */
/************************************************************************/

OGRErr OGRLinearRing::importFromWkb( CPL_UNUSED unsigned char *pabyData,
                                     CPL_UNUSED  int nSize,
                                     CPL_UNUSED OGRwkbVariant eWkbVariant ) 

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Disable method for this class.                                  */
/************************************************************************/

OGRErr OGRLinearRing::exportToWkb( CPL_UNUSED OGRwkbByteOrder eByteOrder, 
                                   CPL_UNUSED unsigned char * pabyData,
                                   CPL_UNUSED OGRwkbVariant eWkbVariant ) const

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                           _importFromWkb()                           */
/*                                                                      */
/*      Helper method for OGRPolygon.  NOT A NORMAL importFromWkb()     */
/*      method!                                                         */
/************************************************************************/

OGRErr OGRLinearRing::_importFromWkb( OGRwkbByteOrder eByteOrder, int b3D, 
                                      unsigned char * pabyData,
                                      int nBytesAvailable ) 

{
    if( nBytesAvailable < 4 && nBytesAvailable != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the vertex count.                                           */
/* -------------------------------------------------------------------- */
    int         nNewNumPoints;
    
    memcpy( &nNewNumPoints, pabyData, 4 );
    
    if( OGR_SWAP( eByteOrder ) )
        nNewNumPoints = CPL_SWAP32(nNewNumPoints);

    /* Check if the wkb stream buffer is big enough to store
     * fetched number of points.
     * 16 or 24 - size of point structure
     */
    int nPointSize = (b3D ? 24 : 16);
    if (nNewNumPoints < 0 || nNewNumPoints > INT_MAX / nPointSize)
        return OGRERR_CORRUPT_DATA;
    int nBufferMinSize = nPointSize * nNewNumPoints;
   
    if( nBytesAvailable != -1 && nBufferMinSize > nBytesAvailable - 4 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Length of input WKB is too small" );
        return OGRERR_NOT_ENOUGH_DATA;
    }

    /* (Re)Allocation of paoPoints buffer. */
    setNumPoints( nNewNumPoints, FALSE );

    if( b3D )
        Make3D();
    else
        Make2D();
    
/* -------------------------------------------------------------------- */
/*      Get the vertices                                                */
/* -------------------------------------------------------------------- */
    int i = 0;

    if( !b3D )
    {
        memcpy( paoPoints, pabyData + 4, 16 * nPointCount );
    }
    else
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            memcpy( &(paoPoints[i].x), pabyData + 4 + 24 * i, 8 );
            memcpy( &(paoPoints[i].y), pabyData + 4 + 24 * i + 8, 8 );
            memcpy( padfZ + i, pabyData + 4 + 24 * i + 16, 8 );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Byte swap if needed.                                            */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            CPL_SWAPDOUBLE( &(paoPoints[i].x) );
            CPL_SWAPDOUBLE( &(paoPoints[i].y) );

            if( b3D )
            {
                CPL_SWAPDOUBLE( padfZ + i );
            }
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            _exportToWkb()                            */
/*                                                                      */
/*      Helper method for OGRPolygon.  THIS IS NOT THE NORMAL           */
/*      exportToWkb() METHOD!                                           */
/************************************************************************/

OGRErr  OGRLinearRing::_exportToWkb( OGRwkbByteOrder eByteOrder, int b3D,
                                     unsigned char * pabyData ) const

{
    int   i, nWords;

/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    memcpy( pabyData, &nPointCount, 4 );

/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    if( b3D )
    {
        nWords = 3 * nPointCount;
        for( i = 0; i < nPointCount; i++ )
        {
            memcpy( pabyData+4+i*24, &(paoPoints[i].x), 8 );
            memcpy( pabyData+4+i*24+8, &(paoPoints[i].y), 8 );
            if( padfZ == NULL )
                memset( pabyData+4+i*24+16, 0, 8 );
            else
                memcpy( pabyData+4+i*24+16, padfZ + i, 8 );
        }
    }
    else
    {
        nWords = 2 * nPointCount; 
        memcpy( pabyData+4, paoPoints, 16 * nPointCount );
    }

/* -------------------------------------------------------------------- */
/*      Swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        int     nCount;

        nCount = CPL_SWAP32( nPointCount );
        memcpy( pabyData, &nCount, 4 );

        for( i = 0; i < nWords; i++ )
        {
            CPL_SWAPDOUBLE( pabyData + 4 + 8 * i );
        }
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                              _WkbSize()                              */
/*                                                                      */
/*      Helper method for OGRPolygon.  NOT THE NORMAL WkbSize() METHOD! */
/************************************************************************/

int OGRLinearRing::_WkbSize( int b3D ) const

{
    if( b3D )
        return 4 + 24 * nPointCount;
    else
        return 4 + 16 * nPointCount;
}

/************************************************************************/
/*                               clone()                                */
/*                                                                      */
/*      We override the OGRCurve clone() to ensure that we get the      */
/*      correct virtual table.                                          */
/************************************************************************/

OGRGeometry *OGRLinearRing::clone() const

{
    OGRLinearRing       *poNewLinearRing;

    poNewLinearRing = new OGRLinearRing();
    poNewLinearRing->assignSpatialReference( getSpatialReference() );

    poNewLinearRing->setPoints( nPointCount, paoPoints, padfZ );

    return poNewLinearRing;
}


/************************************************************************/
/*                            epsilonEqual()                            */
/************************************************************************/

static const double EPSILON = 1E-5;

static inline bool epsilonEqual(double a, double b, double eps) 
{
    return (::fabs(a - b) < eps);
}

/************************************************************************/
/*                            isClockwise()                             */
/************************************************************************/

/**
 * \brief Returns TRUE if the ring has clockwise winding (or less than 2 points)
 *
 * @return TRUE if clockwise otherwise FALSE.
 */

int OGRLinearRing::isClockwise() const

{
    int    i, v, next;
    double  dx0, dy0, dx1, dy1, crossproduct;
    int    bUseFallback = FALSE;

    if( nPointCount < 2 )
        return TRUE;

    /* Find the lowest rightmost vertex */
    v = 0;
    for ( i = 1; i < nPointCount - 1; i++ )
    {
        /* => v < end */
        if ( paoPoints[i].y< paoPoints[v].y ||
             ( paoPoints[i].y== paoPoints[v].y &&
               paoPoints[i].x > paoPoints[v].x ) )
        {
            v = i;
            bUseFallback = FALSE;
        }
        else if ( paoPoints[i].y == paoPoints[v].y &&
                  paoPoints[i].x == paoPoints[v].x )
        {
            /* Two vertex with same coordinates are the lowest rightmost */
            /* vertex! We cannot use that point as the pivot (#5342) */
            bUseFallback = TRUE;
        }
    }

    /* previous */
    next = v - 1;
    if ( next < 0 )
    {
        next = nPointCount - 1 - 1;
    }

    if( epsilonEqual(paoPoints[next].x, paoPoints[v].x, EPSILON) &&
        epsilonEqual(paoPoints[next].y, paoPoints[v].y, EPSILON) )
    {
        /* Don't try to be too clever by retrying with a next point */
        /* This can lead to false results as in the case of #3356 */
        bUseFallback = TRUE;
    }

    dx0 = paoPoints[next].x - paoPoints[v].x;
    dy0 = paoPoints[next].y - paoPoints[v].y;
    
    
    /* following */
    next = v + 1;
    if ( next >= nPointCount - 1 )
    {
        next = 0;
    }

    if( epsilonEqual(paoPoints[next].x, paoPoints[v].x, EPSILON) &&
        epsilonEqual(paoPoints[next].y, paoPoints[v].y, EPSILON) )
    {
        /* Don't try to be too clever by retrying with a next point */
        /* This can lead to false results as in the case of #3356 */
        bUseFallback = TRUE;
    }

    dx1 = paoPoints[next].x - paoPoints[v].x;
    dy1 = paoPoints[next].y - paoPoints[v].y;

    crossproduct = dx1 * dy0 - dx0 * dy1;

    if (!bUseFallback)
    {
        if ( crossproduct > 0 )      /* CCW */
            return FALSE;
        else if ( crossproduct < 0 )  /* CW */
            return TRUE;
    }
    
    /* ok, this is a degenerate case : the extent of the polygon is less than EPSILON */
    /* or 2 nearly identical points were found */
    /* Try with Green Formula as a fallback, but this is not a guarantee */
    /* as we'll probably be affected by numerical instabilities */
    
    double dfSum = paoPoints[0].x * (paoPoints[1].y - paoPoints[nPointCount-1].y);

    for (i=1; i<nPointCount-1; i++) {
        dfSum += paoPoints[i].x * (paoPoints[i+1].y - paoPoints[i-1].y);
    }

    dfSum += paoPoints[nPointCount-1].x * (paoPoints[0].y - paoPoints[nPointCount-2].y);

    return dfSum < 0;
}

/************************************************************************/ 
/*                             reverseWindingOrder()                    */ 
/************************************************************************/ 

void OGRLinearRing::reverseWindingOrder() 

{ 
    int pos = 0; 
    OGRPoint pointA, pointB; 

    for( int i = 0; i < nPointCount / 2; i++ ) 
    { 
        getPoint( i, &pointA ); 
        pos = nPointCount - i - 1;
        getPoint( pos, &pointB );
        setPoint( i, &pointB );
        setPoint( pos, &pointA );
    }
} 

/************************************************************************/
/*                             closeRing()                              */
/************************************************************************/

void OGRLinearRing::closeRings()

{
    if( nPointCount < 2 )
        return;

    if( getX(0) != getX(nPointCount-1) 
        || getY(0) != getY(nPointCount-1)
        || getZ(0) != getZ(nPointCount-1) )
    {
        OGRPoint oFirstPoint;
        getPoint( 0, &oFirstPoint );
        addPoint( &oFirstPoint );
    }
}

/************************************************************************/
/*                              isPointInRing()                         */
/************************************************************************/

OGRBoolean OGRLinearRing::isPointInRing(const OGRPoint* poPoint, int bTestEnvelope) const
{
    if ( NULL == poPoint )
    {
        CPLDebug( "OGR", "OGRLinearRing::isPointInRing(const  OGRPoint* poPoint) - passed point is NULL!" );
        return 0;
    }

    const int iNumPoints = getNumPoints();

    // Simple validation
    if ( iNumPoints < 4 )
        return 0;

    const double dfTestX = poPoint->getX();
    const double dfTestY = poPoint->getY();

    // Fast test if point is inside extent of the ring
    if (bTestEnvelope)
    {
        OGREnvelope extent;
        getEnvelope(&extent);
        if ( !( dfTestX >= extent.MinX && dfTestX <= extent.MaxX
            && dfTestY >= extent.MinY && dfTestY <= extent.MaxY ) )
        {
            return 0;
        }
    }

	// For every point p in ring,
    // test if ray starting from given point crosses segment (p - 1, p)
    int iNumCrossings = 0;

    double prev_diff_x = getX(0) - dfTestX;
    double prev_diff_y = getY(0) - dfTestY;

    for ( int iPoint = 1; iPoint < iNumPoints; iPoint++ ) 
    {
        const double x1 = getX(iPoint) - dfTestX;
        const double y1 = getY(iPoint) - dfTestY;

        const double x2 = prev_diff_x;
        const double y2 = prev_diff_y;

        if( ( ( y1 > 0 ) && ( y2 <= 0 ) ) || ( ( y2 > 0 ) && ( y1 <= 0 ) ) )
        {
            // Check if ray intersects with segment of the ring
            const double dfIntersection = ( x1 * y2 - x2 * y1 ) / (y2 - y1);
            if ( 0.0 < dfIntersection )
            {
                // Count intersections
                iNumCrossings++;
            }
        }

        prev_diff_x = x1;
        prev_diff_y = y1;
    }

    // If iNumCrossings number is even, given point is outside the ring,
    // when the crossings number is odd, the point is inside the ring.
    return ( ( iNumCrossings % 2 ) == 1 ? 1 : 0 );
}

/************************************************************************/
/*                       isPointOnRingBoundary()                        */
/************************************************************************/

OGRBoolean OGRLinearRing::isPointOnRingBoundary(const OGRPoint* poPoint, int bTestEnvelope) const
{
    if ( NULL == poPoint )
    {
        CPLDebug( "OGR", "OGRLinearRing::isPointOnRingBoundary(const  OGRPoint* poPoint) - passed point is NULL!" );
        return 0;
    }

    const int iNumPoints = getNumPoints();

    // Simple validation
    if ( iNumPoints < 4 )
        return 0;

    const double dfTestX = poPoint->getX();
    const double dfTestY = poPoint->getY();

    // Fast test if point is inside extent of the ring
    if( bTestEnvelope )
    {
        OGREnvelope extent;
        getEnvelope(&extent);
        if ( !( dfTestX >= extent.MinX && dfTestX <= extent.MaxX
            && dfTestY >= extent.MinY && dfTestY <= extent.MaxY ) )
        {
            return 0;
        }
    }

    double prev_diff_x = getX(0) - dfTestX;
    double prev_diff_y = getY(0) - dfTestY;

    for ( int iPoint = 1; iPoint < iNumPoints; iPoint++ ) 
    {
        const double x1 = getX(iPoint) - dfTestX;
        const double y1 = getY(iPoint) - dfTestY;

        const double x2 = prev_diff_x;
        const double y2 = prev_diff_y;

        /* If the point is on the segment, return immediatly */
        /* FIXME? If the test point is not exactly identical to one of */
        /* the vertices of the ring, but somewhere on a segment, there's */
        /* little chance that we get 0. So that should be tested against some epsilon */

        if ( x1 * y2 - x2 * y1 == 0 )
        {
            /* If iPoint and iPointPrev are the same, go on */
            if( !(x1 == x2 && y1 == y2) )
            {
                return 1;
            }
        }

        prev_diff_x = x1;
        prev_diff_y = y1;
    }

    return 0;
}

/************************************************************************/
/*                          CastToLineString()                          */
/************************************************************************/

/**
 * \brief Cast to line string.
 *
 * The passed in geometry is consumed and a new one returned .
 * 
 * @param poLR the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRLineString* OGRLinearRing::CastToLineString(OGRLinearRing* poLR)
{
    return TransferMembersAndDestroy(poLR, new OGRLineString());
}

/************************************************************************/
/*                     GetCasterToLineString()                          */
/************************************************************************/

OGRCurveCasterToLineString OGRLinearRing::GetCasterToLineString() const {
    return (OGRCurveCasterToLineString) OGRLinearRing::CastToLineString;
}

/************************************************************************/
/*                        GetCasterToLinearRing()                       */
/************************************************************************/

OGRCurveCasterToLinearRing OGRLinearRing::GetCasterToLinearRing() const {
    return (OGRCurveCasterToLinearRing) OGRGeometry::CastToIdentity;
}
