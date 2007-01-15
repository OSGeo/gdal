/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRLinearRing geometry class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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

    setNumPoints( poSrcRing->getNumPoints() );

    memcpy( paoPoints, poSrcRing->paoPoints,
            sizeof(OGRRawPoint) * getNumPoints() );

    if( poSrcRing->padfZ )
    {
        Make3D();
        memcpy( padfZ, poSrcRing->padfZ, sizeof(double) * getNumPoints() );
    }
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

OGRErr OGRLinearRing::importFromWkb( unsigned char *pabyData, int nSize ) 

{
    (void) pabyData;
    (void) nSize;

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Disable method for this class.                                  */
/************************************************************************/

OGRErr OGRLinearRing::exportToWkb( OGRwkbByteOrder eByteOrder, 
                                   unsigned char * pabyData ) const

{
    (void) eByteOrder;
    (void) pabyData;

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
    int nBufferMinSize = nPointSize * nNewNumPoints;
   
    if( nBufferMinSize > nBytesAvailable && nBytesAvailable > 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Length of input WKB is too small" );
        return OGRERR_NOT_ENOUGH_DATA;
    }

    /* (Re)Allocation of paoPoints buffer. */
    setNumPoints( nNewNumPoints );

    if( b3D )
        Make3D();
    else
        Make2D();
    
/* -------------------------------------------------------------------- */
/*      Get the vertices                                                */
/* -------------------------------------------------------------------- */
    int i = 0;
    int nBytesToCopy = 0;

    if( !b3D )
    {
        nBytesToCopy = 16 * nPointCount;
        
        if( nBytesToCopy > nBytesAvailable && nBytesAvailable > 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "WKB buffer with OGRLinearRing points is too small! \
                      \n\tWKB stream may be corrupted or it is EWKB stream which is not supported");
            return OGRERR_NOT_ENOUGH_DATA;
        }

        memcpy( paoPoints, pabyData + 4, nBytesToCopy );
    }
    else
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            nBytesToCopy = 24;

            if( nBytesToCopy > nBytesAvailable && nBytesAvailable > 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "WKB buffer with OGRLinearRing points is too small! \
                          \n\tWKB stream may be corrupted or it is EWKB stream which is not supported");
                return OGRERR_NOT_ENOUGH_DATA;
            }
            if ( nBytesAvailable > 0 )
                nBytesAvailable -= nBytesToCopy;

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
/*                            isClockwise()                             */
/************************************************************************/

/**
 * Returns TRUE if the ring has clockwise winding.
 *
 * @return TRUE if clockwise otherwise FALSE.
 */

int OGRLinearRing::isClockwise() const

{
    double dfSum = 0.0;

    for( int iVert = 0; iVert < nPointCount-1; iVert++ )
    {
        dfSum += paoPoints[iVert].x * paoPoints[iVert+1].y
            - paoPoints[iVert].y * paoPoints[iVert+1].x;
    }

    dfSum += paoPoints[nPointCount-1].x * paoPoints[0].y
        - paoPoints[nPointCount-1].y * paoPoints[0].x;

    return dfSum < 0.0;
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
        addPoint( getX(0), getY(0), getZ(0) );
    }
}

/************************************************************************/
/*                              get_Area()                              */
/************************************************************************/

/**
 * Compute area of ring.
 *
 * The area is computed according to Green's Theorem:  
 *
 * Area is "Sum(x(i)*y(i+1) - x(i+1)*y(i))/2" for i = 0 to pointCount-1, 
 * assuming the last point is a duplicate of the first. 
 *
 * @return computed area.
 */

double OGRLinearRing::get_Area() const

{
    double dfAreaSum = 0.0;
    int i;

    for( i = 0; i < nPointCount-1; i++ )
    {
        dfAreaSum += 0.5 * ( paoPoints[i].x * paoPoints[i+1].y 
                             - paoPoints[i+1].x * paoPoints[i].y );
    }

    dfAreaSum += 0.5 * ( paoPoints[nPointCount-1].x * paoPoints[0].y 
                         - paoPoints[0].x * paoPoints[nPointCount-1].y );

    return fabs(dfAreaSum);
}

/************************************************************************/
/*                              isPointInRing()                         */
/************************************************************************/

OGRBoolean OGRLinearRing::isPointInRing(const OGRPoint* poPoint) const
{
    if ( NULL == poPoint )
    {
        CPLDebug( "OGR", "OGRLinearRing::isPointInRing(const  OGRPoint* poPoint) - passed point is NULL!" );
        return 0;
    }

    CPLDebug( "OGR", "OGRLinearRing::isPointInRing(): passed point: (%.8f,%.8f)",
              poPoint->getX(), poPoint->getY() );

    const int iNumPoints = getNumPoints();

    // Simple validation
    if ( iNumPoints < 4 )
        return 0;

    const double dfTestX = poPoint->getX();
    const double dfTestY = poPoint->getY();

    // Fast test if point is inside extent of the ring
    OGREnvelope extent;
    getEnvelope(&extent);
    if ( !( dfTestX >= extent.MinX && dfTestX <= extent.MaxX
         && dfTestY >= extent.MinY && dfTestY <= extent.MaxY ) )
    {
        CPLDebug( "OGR", "OGRLinearRing::isPointInRing(): passed point is out of extent of ring" );
        return 0;
    }

	// For every point p in ring,
    // test if ray starting from given point crosses segment (p - 1, p)
    int iNumCrossings = 0;

    for ( int iPoint = 1; iPoint < iNumPoints; iPoint++ ) 
    {
        const int iPointPrev = iPoint - 1;

        const double x1 = getX(iPoint) - dfTestX;
        const double y1 = getY(iPoint) - dfTestY;

        const double x2 = getX(iPointPrev) - dfTestX;
        const double y2 = getY(iPointPrev) - dfTestY;

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
    }

    // If iNumCrossings number is even, given point is outside the ring,
    // when the crossings number is odd, the point is inside the ring.
    return ( ( iNumCrossings % 2 ) == 1 ? 1 : 0 );
}
