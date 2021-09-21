/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRLinearRing geometry class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_geometry.h"

#include <climits>
#include <cmath>
#include <cstring>

#include "cpl_error.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRLinearRing()                            */
/************************************************************************/

/** Constructor */
OGRLinearRing::OGRLinearRing() = default;

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

OGRLinearRing::OGRLinearRing( const OGRLinearRing& ) = default;

/************************************************************************/
/*                          ~OGRLinearRing()                            */
/************************************************************************/

OGRLinearRing::~OGRLinearRing() = default;

/************************************************************************/
/*                           OGRLinearRing()                            */
/************************************************************************/

/** Constructor
 * @param poSrcRing source ring.
 */
OGRLinearRing::OGRLinearRing( OGRLinearRing * poSrcRing )

{
    if( poSrcRing == nullptr )
    {
        CPLDebug( "OGR",
                  "OGRLinearRing::OGRLinearRing(OGRLinearRing*poSrcRing) - "
                  "passed in ring is NULL!" );
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

size_t OGRLinearRing::WkbSize() const

{
    return 0;
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Disable method for this class.                                  */
/************************************************************************/

OGRErr OGRLinearRing::importFromWkb( const unsigned char * /*pabyData*/,
                                     size_t /*nSize*/,
                                     OGRwkbVariant /*eWkbVariant*/,
                                     size_t& /* nBytesConsumedOut */ )

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
/*      method.                                                         */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRErr OGRLinearRing::_importFromWkb( OGRwkbByteOrder eByteOrder, int _flags,
                                      const unsigned char * pabyData,
                                      size_t nBytesAvailable,
                                      size_t& nBytesConsumedOut )

{
    nBytesConsumedOut = 0;
    if( nBytesAvailable < 4 && nBytesAvailable != static_cast<size_t>(-1) )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the vertex count.                                           */
/* -------------------------------------------------------------------- */
    int nNewNumPoints = 0;

    memcpy( &nNewNumPoints, pabyData, 4 );

    if( OGR_SWAP( eByteOrder ) )
        nNewNumPoints = CPL_SWAP32(nNewNumPoints);

    // Check if the wkb stream buffer is big enough to store
    // fetched number of points.
    // 16, 24, or 32 - size of point structure.
    size_t nPointSize = 0;
    if( (_flags & OGR_G_3D) && (_flags & OGR_G_MEASURED) )
        nPointSize = 32;
    else if( (_flags & OGR_G_3D) || (_flags & OGR_G_MEASURED) )
        nPointSize = 24;
    else
        nPointSize = 16;

    if( nNewNumPoints < 0 ||
        static_cast<size_t>(nNewNumPoints) > std::numeric_limits<size_t>::max() / nPointSize )
    {
        return OGRERR_CORRUPT_DATA;
    }
    const size_t nBufferMinSize = nPointSize * nNewNumPoints;
    if( nBytesAvailable != static_cast<size_t>(-1) && nBufferMinSize > nBytesAvailable - 4 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Length of input WKB is too small" );
        return OGRERR_NOT_ENOUGH_DATA;
    }

    // (Re)Allocation of paoPoints buffer.
    setNumPoints( nNewNumPoints, FALSE );

    if( _flags & OGR_G_3D )
        Make3D();
    else
        Make2D();

    if( _flags & OGR_G_MEASURED )
        AddM();
    else
        RemoveM();


    nBytesConsumedOut =  4 + nPointCount * nPointSize;

/* -------------------------------------------------------------------- */
/*      Get the vertices                                                */
/* -------------------------------------------------------------------- */
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
    {
        for( size_t i = 0; i < static_cast<size_t>(nPointCount); i++ )
        {
            memcpy( &(paoPoints[i].x), pabyData + 4 + 32 * i, 8 );
            memcpy( &(paoPoints[i].y), pabyData + 4 + 32 * i + 8, 8 );
            memcpy( padfZ + i, pabyData + 4 + 32 * i + 16, 8 );
            memcpy( padfM + i, pabyData + 4 + 32 * i + 24, 8 );
        }
    }
    else if( flags & OGR_G_MEASURED )
    {
        for( size_t i = 0; i < static_cast<size_t>(nPointCount); i++ )
        {
            memcpy( &(paoPoints[i].x), pabyData + 4 + 24 * i, 8 );
            memcpy( &(paoPoints[i].y), pabyData + 4 + 24 * i + 8, 8 );
            memcpy( padfM + i, pabyData + 4 + 24 * i + 16, 8 );
        }
    }
    else if( flags & OGR_G_3D )
    {
        for( size_t i = 0; i < static_cast<size_t>(nPointCount); i++ )
        {
            memcpy( &(paoPoints[i].x), pabyData + 4 + 24 * i, 8 );
            memcpy( &(paoPoints[i].y), pabyData + 4 + 24 * i + 8, 8 );
            memcpy( padfZ + i, pabyData + 4 + 24 * i + 16, 8 );
        }
    }
    else
    {
        memcpy( paoPoints, pabyData + 4, 16 * static_cast<size_t>(nPointCount) );
    }

/* -------------------------------------------------------------------- */
/*      Byte swap if needed.                                            */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        for( size_t i = 0; i < static_cast<size_t>(nPointCount); i++ )
        {
            CPL_SWAPDOUBLE( &(paoPoints[i].x) );
            CPL_SWAPDOUBLE( &(paoPoints[i].y) );

            if( flags & OGR_G_3D )
            {
                CPL_SWAPDOUBLE( padfZ + i );
            }
            if( flags & OGR_G_MEASURED )
            {
                CPL_SWAPDOUBLE( padfM + i );
            }
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            _exportToWkb()                            */
/*                                                                      */
/*      Helper method for OGRPolygon.  THIS IS NOT THE NORMAL           */
/*      exportToWkb() METHOD.                                           */
/************************************************************************/

OGRErr OGRLinearRing::_exportToWkb( OGRwkbByteOrder eByteOrder, int _flags,
                                    unsigned char * pabyData ) const

{

/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    memcpy( pabyData, &nPointCount, 4 );

/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    size_t nWords = 0;
    if( (_flags & OGR_G_3D) && (_flags & OGR_G_MEASURED) )
    {
        nWords = 4 * static_cast<size_t>(nPointCount);
        for( size_t i = 0; i < static_cast<size_t>(nPointCount); i++ )
        {
            memcpy( pabyData+4+i*32, &(paoPoints[i].x), 8 );
            memcpy( pabyData+4+i*32+8, &(paoPoints[i].y), 8 );
            if( padfZ == nullptr )
                memset( pabyData+4+i*32+16, 0, 8 );
            else
                memcpy( pabyData+4+i*32+16, padfZ + i, 8 );
            if( padfM == nullptr )
                memset( pabyData+4+i*32+24, 0, 8 );
            else
                memcpy( pabyData+4+i*32+24, padfM + i, 8 );
        }
    }
    else if( _flags & OGR_G_MEASURED )
    {
        nWords = 3 * static_cast<size_t>(nPointCount);
        for( size_t i = 0; i < static_cast<size_t>(nPointCount); i++ )
        {
            memcpy( pabyData+4+i*24, &(paoPoints[i].x), 8 );
            memcpy( pabyData+4+i*24+8, &(paoPoints[i].y), 8 );
            if( padfM == nullptr )
                memset( pabyData+4+i*24+16, 0, 8 );
            else
                memcpy( pabyData+4+i*24+16, padfM + i, 8 );
        }
    }
    else if( _flags & OGR_G_3D )
    {
        nWords = 3 * static_cast<size_t>(nPointCount);
        for( size_t i = 0; i < static_cast<size_t>(nPointCount); i++ )
        {
            memcpy( pabyData+4+i*24, &(paoPoints[i].x), 8 );
            memcpy( pabyData+4+i*24+8, &(paoPoints[i].y), 8 );
            if( padfZ == nullptr )
                memset( pabyData+4+i*24+16, 0, 8 );
            else
                memcpy( pabyData+4+i*24+16, padfZ + i, 8 );
        }
    }
    else
    {
        nWords = 2 * static_cast<size_t>(nPointCount);
        memcpy( pabyData+4, paoPoints, 16 * static_cast<size_t>(nPointCount) );
    }

/* -------------------------------------------------------------------- */
/*      Swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        const int nCount = CPL_SWAP32( nPointCount );
        memcpy( pabyData, &nCount, 4 );

        for( size_t i = 0; i < nWords; i++ )
        {
            CPL_SWAPDOUBLE( pabyData + 4 + 8 * i );
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                              _WkbSize()                              */
/*                                                                      */
/*      Helper method for OGRPolygon.  NOT THE NORMAL WkbSize() METHOD. */
/************************************************************************/

size_t OGRLinearRing::_WkbSize( int _flags ) const

{
    if( (_flags & OGR_G_3D) && (_flags & OGR_G_MEASURED) )
        return 4 + 32 * static_cast<size_t>(nPointCount);
    else if( (_flags & OGR_G_3D) || (_flags & OGR_G_MEASURED) )
        return 4 + 24 * static_cast<size_t>(nPointCount);
    else
        return 4 + 16 * static_cast<size_t>(nPointCount);
}
//! @endcond

/************************************************************************/
/*                               clone()                                */
/*                                                                      */
/*      We override the OGRCurve clone() to ensure that we get the      */
/*      correct virtual table.                                          */
/************************************************************************/

OGRLinearRing *OGRLinearRing::clone() const

{
    OGRLinearRing *poNewLinearRing = new OGRLinearRing();
    poNewLinearRing->assignSpatialReference( getSpatialReference() );

    poNewLinearRing->setPoints( nPointCount, paoPoints, padfZ, padfM );
    poNewLinearRing->flags = flags;

    return poNewLinearRing;
}

/************************************************************************/
/*                            epsilonEqual()                            */
/************************************************************************/

constexpr double EPSILON = 1.0E-5;

static inline bool epsilonEqual(double a, double b, double eps)
{
    return ::fabs(a - b) < eps;
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
    if( nPointCount < 2 )
        return TRUE;

    bool bUseFallback = false;

    // Find the lowest rightmost vertex.
    int v = 0;  // Used after for.
    for( int i = 1; i < nPointCount - 1; i++ )
    {
        // => v < end.
        if( paoPoints[i].y< paoPoints[v].y ||
            ( paoPoints[i].y== paoPoints[v].y &&
              paoPoints[i].x > paoPoints[v].x ) )
        {
            v = i;
            bUseFallback = false;
        }
        else if( paoPoints[i].y == paoPoints[v].y &&
                 paoPoints[i].x == paoPoints[v].x )
        {
            // Two vertex with same coordinates are the lowest rightmost
            // vertex.  Cannot use that point as the pivot (#5342).
            bUseFallback = true;
        }
    }

    // Previous.
    int next = v - 1;
    if( next < 0 )
    {
        next = nPointCount - 1 - 1;
    }

    if( epsilonEqual(paoPoints[next].x, paoPoints[v].x, EPSILON) &&
        epsilonEqual(paoPoints[next].y, paoPoints[v].y, EPSILON) )
    {
        // Don't try to be too clever by retrying with a next point.
        // This can lead to false results as in the case of #3356.
        bUseFallback = true;
    }

    const double dx0 = paoPoints[next].x - paoPoints[v].x;
    const double dy0 = paoPoints[next].y - paoPoints[v].y;

    // Following.
    next = v + 1;
    if( next >= nPointCount - 1 )
    {
        next = 0;
    }

    if( epsilonEqual(paoPoints[next].x, paoPoints[v].x, EPSILON) &&
        epsilonEqual(paoPoints[next].y, paoPoints[v].y, EPSILON) )
    {
        // Don't try to be too clever by retrying with a next point.
        // This can lead to false results as in the case of #3356.
        bUseFallback = true;
    }

    const double dx1 = paoPoints[next].x - paoPoints[v].x;
    const double dy1 = paoPoints[next].y - paoPoints[v].y;

    const double crossproduct = dx1 * dy0 - dx0 * dy1;

    if( !bUseFallback )
    {
        if( crossproduct > 0 )       // CCW
            return FALSE;
        else if( crossproduct < 0 )  // CW
            return TRUE;
    }

    // This is a degenerate case: the extent of the polygon is less than EPSILON
    // or 2 nearly identical points were found.
    // Try with Green Formula as a fallback, but this is not a guarantee
    // as we'll probably be affected by numerical instabilities.

    double dfSum =
        paoPoints[0].x * (paoPoints[1].y - paoPoints[nPointCount-1].y);

    for( int i = 1; i < nPointCount-1; i++ )
    {
        dfSum += paoPoints[i].x * (paoPoints[i+1].y - paoPoints[i-1].y);
    }

    dfSum +=
        paoPoints[nPointCount-1].x *
        (paoPoints[0].y - paoPoints[nPointCount-2].y);

    return dfSum < 0;
}

/************************************************************************/
/*                             reverseWindingOrder()                    */
/************************************************************************/

/** Reverse order of points.
 */
void OGRLinearRing::reverseWindingOrder()

{
    OGRPoint pointA;
    OGRPoint pointB;

    for( int i = 0; i < nPointCount / 2; i++ )
    {
        getPoint( i, &pointA );
        const int pos = nPointCount - i - 1;
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

/** Returns whether the point is inside the ring.
 * @param poPoint point
 * @param bTestEnvelope set to TRUE if the presence of the point inside the
 *                      ring envelope must be checked first.
 * @return TRUE or FALSE.
 */
OGRBoolean OGRLinearRing::isPointInRing(const OGRPoint* poPoint,
                                        int bTestEnvelope) const
{
    if( nullptr == poPoint )
    {
        CPLDebug( "OGR",
                  "OGRLinearRing::isPointInRing(const OGRPoint* poPoint) - "
                  "passed point is NULL!" );
        return FALSE;
    }
    if( poPoint->IsEmpty() )
    {
        return FALSE;
    }

    const int iNumPoints = getNumPoints();

    // Simple validation
    if( iNumPoints < 4 )
        return FALSE;

    const double dfTestX = poPoint->getX();
    const double dfTestY = poPoint->getY();

    // Fast test if point is inside extent of the ring.
    if( bTestEnvelope )
    {
        OGREnvelope extent;
        getEnvelope(&extent);
        if( !( dfTestX >= extent.MinX && dfTestX <= extent.MaxX
               && dfTestY >= extent.MinY && dfTestY <= extent.MaxY ) )
        {
            return FALSE;
        }
    }

    // For every point p in ring,
    // test if ray starting from given point crosses segment (p - 1, p)
    int iNumCrossings = 0;

    double prev_diff_x = getX(0) - dfTestX;
    double prev_diff_y = getY(0) - dfTestY;

    for( int iPoint = 1; iPoint < iNumPoints; iPoint++ )
    {
        const double x1 = getX(iPoint) - dfTestX;
        const double y1 = getY(iPoint) - dfTestY;

        const double x2 = prev_diff_x;
        const double y2 = prev_diff_y;

        if( ( ( y1 > 0 ) && ( y2 <= 0 ) ) || ( ( y2 > 0 ) && ( y1 <= 0 ) ) )
        {
            // Check if ray intersects with segment of the ring
            const double dfIntersection = ( x1 * y2 - x2 * y1 ) / (y2 - y1);
            if( 0.0 < dfIntersection )
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
    return iNumCrossings % 2;  // OGRBoolean
}

/************************************************************************/
/*                       isPointOnRingBoundary()                        */
/************************************************************************/

/** Returns whether the point is on the ring boundary.
 * @param poPoint point
 * @param bTestEnvelope set to TRUE if the presence of the point inside the
 *                      ring envelope must be checked first.
 * @return TRUE or FALSE.
 */
OGRBoolean OGRLinearRing::isPointOnRingBoundary( const OGRPoint* poPoint,
                                                 int bTestEnvelope ) const
{
    if( nullptr == poPoint )
    {
        CPLDebug( "OGR",
                  "OGRLinearRing::isPointOnRingBoundary(const OGRPoint* "
                  "poPoint) - passed point is NULL!" );
        return 0;
    }

    const int iNumPoints = getNumPoints();

    // Simple validation.
    if( iNumPoints < 4 )
        return 0;

    const double dfTestX = poPoint->getX();
    const double dfTestY = poPoint->getY();

    // Fast test if point is inside extent of the ring
    if( bTestEnvelope )
    {
        OGREnvelope extent;
        getEnvelope(&extent);
        if( !( dfTestX >= extent.MinX && dfTestX <= extent.MaxX
               && dfTestY >= extent.MinY && dfTestY <= extent.MaxY ) )
        {
            return 0;
        }
    }

    double prev_diff_x = dfTestX - getX(0);
    double prev_diff_y = dfTestY - getY(0);

    for( int iPoint = 1; iPoint < iNumPoints; iPoint++ )
    {
        const double dx1 = dfTestX - getX(iPoint);
        const double dy1 = dfTestY - getY(iPoint);

        const double dx2 = prev_diff_x;
        const double dy2 = prev_diff_y;

        // If the point is on the segment, return immediately.
        // FIXME? If the test point is not exactly identical to one of
        // the vertices of the ring, but somewhere on a segment, there's
        // little chance that we get 0. So that should be tested against some
        // epsilon.

        if( dx1 * dy2 - dx2 * dy1 == 0 )
        {
            // If iPoint and iPointPrev are the same, go on.
            if( !(dx1 == dx2 && dy1 == dy2) )
            {
                const double dx_segment = getX(iPoint) - getX(iPoint-1);
                const double dy_segment = getY(iPoint) - getY(iPoint-1);
                const double crossproduct =
                    dx2 * dx_segment + dy2 * dy_segment;
                if( crossproduct >= 0 )
                {
                    const double sq_length_seg = dx_segment * dx_segment +
                                                 dy_segment * dy_segment;
                    if( crossproduct <= sq_length_seg )
                    {
                        return 1;
                    }
                }
            }
        }

        prev_diff_x = dx1;
        prev_diff_y = dy1;
    }

    return 0;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRLinearRing::transform( OGRCoordinateTransformation *poCT )

{
    const bool bIsClosed = getNumPoints() > 2 && CPL_TO_BOOL(get_IsClosed());
    OGRErr eErr = OGRLineString::transform(poCT);
    if( bIsClosed && eErr == OGRERR_NONE && !get_IsClosed() )
    {
        CPLDebug("OGR", "Linearring is not closed after coordinate "
                  "transformation. Forcing last point to be identical to "
                  "first one");
        // Force last point to be identical to first point.
        // This is a safety belt in case the reprojection of the same coordinate
        // isn't perfectly stable. This can for example happen in very rare cases
        // when reprojecting a cutline with a RPC transform with a DEM that
        // is a VRT whose sources are resampled...
        OGRPoint oStartPoint;
        StartPoint( &oStartPoint );

        setPoint( getNumPoints()-1, &oStartPoint);
    }
    return eErr;
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

OGRLineString* OGRLinearRing::CastToLineString( OGRLinearRing* poLR )
{
    return TransferMembersAndDestroy(poLR, new OGRLineString());
}

//! @cond Doxygen_Suppress
/************************************************************************/
/*                     GetCasterToLineString()                          */
/************************************************************************/

OGRLineString* OGRLinearRing::CasterToLineString( OGRCurve* poCurve )
{
    return OGRLinearRing::CastToLineString(poCurve->toLinearRing());
}

OGRCurveCasterToLineString OGRLinearRing::GetCasterToLineString() const
{
    return OGRLinearRing::CasterToLineString;
}

/************************************************************************/
/*                        GetCasterToLinearRing()                       */
/************************************************************************/

static OGRLinearRing* CasterToLinearRing(OGRCurve* poCurve)
{
    return poCurve->toLinearRing();
}

OGRCurveCasterToLinearRing OGRLinearRing::GetCasterToLinearRing() const
{
    return ::CasterToLinearRing;
}
//! @endcond
