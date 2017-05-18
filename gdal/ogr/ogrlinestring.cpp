/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSimpleCurve and OGRLineString geometry classes.
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
#include "ogr_geos.h"
#include "ogr_p.h"

#include <cstdlib>
#include <algorithm>

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRSimpleCurve()                           */
/************************************************************************/

/** Constructor */
OGRSimpleCurve::OGRSimpleCurve() :
    nPointCount(0),
    paoPoints(NULL),
    padfZ(NULL),
    padfM(NULL)
{}

/************************************************************************/
/*                OGRSimpleCurve( const OGRSimpleCurve& )               */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRSimpleCurve::OGRSimpleCurve( const OGRSimpleCurve& other ) :
    OGRCurve(other),
    nPointCount(0),
    paoPoints(NULL),
    padfZ(NULL),
    padfM(NULL)
{
    setPoints( other.nPointCount, other.paoPoints, other.padfZ, other.padfM );
}

/************************************************************************/
/*                          ~OGRSimpleCurve()                           */
/************************************************************************/

OGRSimpleCurve::~OGRSimpleCurve()

{
    CPLFree( paoPoints );
    CPLFree( padfZ );
    CPLFree( padfM );
}

/************************************************************************/
/*                       operator=( const OGRPoint& )                   */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRSimpleCurve& OGRSimpleCurve::operator=( const OGRSimpleCurve& other )
{
    if( this == &other)
        return *this;

    OGRCurve::operator=( other );

    setPoints(other.nPointCount, other.paoPoints, other.padfZ, other.padfM);

    return *this;
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRSimpleCurve::flattenTo2D()

{
    Make2D();
    setMeasured(FALSE);
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRSimpleCurve::clone() const

{
    OGRSimpleCurve *poCurve = dynamic_cast<OGRSimpleCurve *>(
            OGRGeometryFactory::createGeometry(getGeometryType()));
    if( poCurve == NULL )
        return NULL;

    poCurve->assignSpatialReference( getSpatialReference() );
    poCurve->setPoints( nPointCount, paoPoints, padfZ, padfM );
    if( poCurve->getNumPoints() != nPointCount )
    {
        delete poCurve;
        return NULL;
    }
    poCurve->flags = flags;

    return poCurve;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRSimpleCurve::empty()

{
    setNumPoints( 0 );
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRSimpleCurve::setCoordinateDimension( int nNewDimension )

{
    if( nNewDimension == 2 )
        Make2D();
    else if( nNewDimension == 3 )
        Make3D();
    setMeasured(FALSE);
}

void OGRSimpleCurve::set3D( OGRBoolean bIs3D )

{
    if( bIs3D )
        Make3D();
    else
        Make2D();
}

void OGRSimpleCurve::setMeasured( OGRBoolean bIsMeasured )

{
    if( bIsMeasured )
        AddM();
    else
        RemoveM();
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRSimpleCurve::WkbSize() const

{
    return 5 + 4 + 8 * nPointCount * CoordinateDimension();
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                               Make2D()                               */
/************************************************************************/

void OGRSimpleCurve::Make2D()

{
    if( padfZ != NULL )
    {
        CPLFree( padfZ );
        padfZ = NULL;
    }
    flags &= ~OGR_G_3D;
}

/************************************************************************/
/*                               Make3D()                               */
/************************************************************************/

void OGRSimpleCurve::Make3D()

{
    if( padfZ == NULL )
    {
        if( nPointCount == 0 )
            padfZ =
                static_cast<double *>(VSI_CALLOC_VERBOSE(sizeof(double), 1));
        else
            padfZ = static_cast<double *>(VSI_CALLOC_VERBOSE(
                sizeof(double), nPointCount));
        if( padfZ == NULL )
        {
            flags &= ~OGR_G_3D;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "OGRSimpleCurve::Make3D() failed");
            return;
        }
    }
    flags |= OGR_G_3D;
}

/************************************************************************/
/*                               RemoveM()                              */
/************************************************************************/

void OGRSimpleCurve::RemoveM()

{
    if( padfM != NULL )
    {
        CPLFree( padfM );
        padfM = NULL;
    }
    flags &= ~OGR_G_MEASURED;
}

/************************************************************************/
/*                               AddM()                                 */
/************************************************************************/

void OGRSimpleCurve::AddM()

{
    if( padfM == NULL )
    {
        if( nPointCount == 0 )
            padfM =
                static_cast<double *>(VSI_CALLOC_VERBOSE(sizeof(double), 1));
        else
            padfM = static_cast<double *>(
                VSI_CALLOC_VERBOSE(sizeof(double), nPointCount));
        if( padfM == NULL )
        {
            flags &= ~OGR_G_MEASURED;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "OGRSimpleCurve::AddM() failed");
            return;
        }
    }
    flags |= OGR_G_MEASURED;
}

//! @endcond

/************************************************************************/
/*                              getPoint()                              */
/************************************************************************/

/**
 * \brief Fetch a point in line string.
 *
 * This method relates to the SFCOM ILineString::get_Point() method.
 *
 * @param i the vertex to fetch, from 0 to getNumPoints()-1.
 * @param poPoint a point to initialize with the fetched point.
 */

void OGRSimpleCurve::getPoint( int i, OGRPoint * poPoint ) const

{
    CPLAssert( i >= 0 );
    CPLAssert( i < nPointCount );
    CPLAssert( poPoint != NULL );

    poPoint->setX( paoPoints[i].x );
    poPoint->setY( paoPoints[i].y );

    if( (flags & OGR_G_3D) && padfZ != NULL )
        poPoint->setZ( padfZ[i] );
    if( (flags & OGR_G_MEASURED) && padfM != NULL )
        poPoint->setM( padfM[i] );
}

/**
 * \fn int OGRSimpleCurve::getNumPoints() const;
 *
 * \brief Fetch vertex count.
 *
 * Returns the number of vertices in the line string.
 *
 * @return vertex count.
 */

/**
 * \fn double OGRSimpleCurve::getX( int iVertex ) const;
 *
 * \brief Get X at vertex.
 *
 * Returns the X value at the indicated vertex.   If iVertex is out of range a
 * crash may occur, no internal range checking is performed.
 *
 * @param iVertex the vertex to return, between 0 and getNumPoints()-1.
 *
 * @return X value.
 */

/**
 * \fn double OGRSimpleCurve::getY( int iVertex ) const;
 *
 * \brief Get Y at vertex.
 *
 * Returns the Y value at the indicated vertex.   If iVertex is out of range a
 * crash may occur, no internal range checking is performed.
 *
 * @param iVertex the vertex to return, between 0 and getNumPoints()-1.
 *
 * @return X value.
 */

/************************************************************************/
/*                                getZ()                                */
/************************************************************************/

/**
 * \brief Get Z at vertex.
 *
 * Returns the Z (elevation) value at the indicated vertex.  If no Z
 * value is available, 0.0 is returned.  If iVertex is out of range a
 * crash may occur, no internal range checking is performed.
 *
 * @param iVertex the vertex to return, between 0 and getNumPoints()-1.
 *
 * @return Z value.
 */

double OGRSimpleCurve::getZ( int iVertex ) const

{
    if( padfZ != NULL && iVertex >= 0 && iVertex < nPointCount
        && (flags & OGR_G_3D) )
        return( padfZ[iVertex] );
    else
        return 0.0;
}

/************************************************************************/
/*                                getM()                                */
/************************************************************************/

/**
 * \brief Get measure at vertex.
 *
 * Returns the M (measure) value at the indicated vertex.  If no M
 * value is available, 0.0 is returned.  If iVertex is out of range a
 * crash may occur, no internal range checking is performed.
 *
 * @param iVertex the vertex to return, between 0 and getNumPoints()-1.
 *
 * @return M value.
 */

double OGRSimpleCurve::getM( int iVertex ) const

{
    if( padfM != NULL && iVertex >= 0 && iVertex < nPointCount
        && (flags & OGR_G_MEASURED) )
        return( padfM[iVertex] );
    else
        return 0.0;
}

/************************************************************************/
/*                            setNumPoints()                            */
/************************************************************************/

/**
 * \brief Set number of points in geometry.
 *
 * This method primary exists to preset the number of points in a linestring
 * geometry before setPoint() is used to assign them to avoid reallocating
 * the array larger with each call to addPoint().
 *
 * This method has no SFCOM analog.
 *
 * @param nNewPointCount the new number of points for geometry.
 * @param bZeroizeNewContent whether to set to zero the new elements of arrays
 *                           that are extended.
 */

void OGRSimpleCurve::setNumPoints( int nNewPointCount, int bZeroizeNewContent )

{
    CPLAssert( nNewPointCount >= 0 );

    if( nNewPointCount == 0 )
    {
        CPLFree( paoPoints );
        paoPoints = NULL;

        CPLFree( padfZ );
        padfZ = NULL;

        CPLFree( padfM );
        padfM = NULL;

        nPointCount = 0;
        return;
    }

    if( nNewPointCount > nPointCount )
    {
        OGRRawPoint* paoNewPoints = static_cast<OGRRawPoint *>(
            VSI_REALLOC_VERBOSE(paoPoints,
                                sizeof(OGRRawPoint) * nNewPointCount));
        if( paoNewPoints == NULL )
        {
            return;
        }
        paoPoints = paoNewPoints;

        if( bZeroizeNewContent )
            memset( paoPoints + nPointCount,
                0, sizeof(OGRRawPoint) * (nNewPointCount - nPointCount) );

        if( flags & OGR_G_3D )
        {
            double* padfNewZ = static_cast<double *>(
                VSI_REALLOC_VERBOSE(padfZ, sizeof(double) * nNewPointCount));
            if( padfNewZ == NULL )
            {
                return;
            }
            padfZ = padfNewZ;
            if( bZeroizeNewContent )
                memset( padfZ + nPointCount, 0,
                    sizeof(double) * (nNewPointCount - nPointCount) );
        }

        if( flags & OGR_G_MEASURED )
        {
            double* padfNewM = static_cast<double *>(
                VSI_REALLOC_VERBOSE(padfM, sizeof(double) * nNewPointCount));
            if( padfNewM == NULL )
            {
                return;
            }
            padfM = padfNewM;
            if( bZeroizeNewContent )
                memset( padfM + nPointCount, 0,
                    sizeof(double) * (nNewPointCount - nPointCount) );
        }
    }

    nPointCount = nNewPointCount;
}

/************************************************************************/
/*                              setPoint()                              */
/************************************************************************/

/**
 * \brief Set the location of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accommodate the request.
 *
 * There is no SFCOM analog to this method.
 *
 * @param iPoint the index of the vertex to assign (zero based).
 * @param poPoint the value to assign to the vertex.
 */

void OGRSimpleCurve::setPoint( int iPoint, OGRPoint * poPoint )

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        setPoint( iPoint, poPoint->getX(), poPoint->getY(), poPoint->getM() );
    else if( flags & OGR_G_MEASURED )
        setPointM( iPoint, poPoint->getX(), poPoint->getY(), poPoint->getM() );
    else if( flags & OGR_G_3D )
        setPoint( iPoint, poPoint->getX(), poPoint->getY(), poPoint->getZ() );
    else
        setPoint( iPoint, poPoint->getX(), poPoint->getY() );
}

/************************************************************************/
/*                              setPoint()                              */
/************************************************************************/

/**
 * \brief Set the location of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accommodate the request.
 *
 * There is no SFCOM analog to this method.
 *
 * @param iPoint the index of the vertex to assign (zero based).
 * @param xIn input X coordinate to assign.
 * @param yIn input Y coordinate to assign.
 * @param zIn input Z coordinate to assign (defaults to zero).
 */

void OGRSimpleCurve::setPoint( int iPoint, double xIn, double yIn, double zIn )

{
    if( !(flags & OGR_G_3D) )
        Make3D();

    if( iPoint >= nPointCount )
    {
        setNumPoints( iPoint+1 );
        if( nPointCount < iPoint + 1 )
            return;
    }
#ifdef DEBUG
    if( paoPoints == NULL )
        return;
#endif

    paoPoints[iPoint].x = xIn;
    paoPoints[iPoint].y = yIn;

    if( padfZ != NULL )
    {
        padfZ[iPoint] = zIn;
    }
}

/**
 * \brief Set the location of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accommodate the request.
 *
 * There is no SFCOM analog to this method.
 *
 * @param iPoint the index of the vertex to assign (zero based).
 * @param xIn input X coordinate to assign.
 * @param yIn input Y coordinate to assign.
 * @param mIn input M coordinate to assign (defaults to zero).
 */

void OGRSimpleCurve::setPointM( int iPoint, double xIn, double yIn, double mIn )

{
    if( !(flags & OGR_G_MEASURED) )
        AddM();

    if( iPoint >= nPointCount )
    {
        setNumPoints( iPoint+1 );
        if( nPointCount < iPoint + 1 )
            return;
    }
#ifdef DEBUG
    if( paoPoints == NULL )
        return;
#endif

    paoPoints[iPoint].x = xIn;
    paoPoints[iPoint].y = yIn;

    if( padfM != NULL )
    {
        padfM[iPoint] = mIn;
    }
}

/**
 * \brief Set the location of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accommodate the request.
 *
 * There is no SFCOM analog to this method.
 *
 * @param iPoint the index of the vertex to assign (zero based).
 * @param xIn input X coordinate to assign.
 * @param yIn input Y coordinate to assign.
 * @param zIn input Z coordinate to assign (defaults to zero).
 * @param mIn input M coordinate to assign (defaults to zero).
 */

void OGRSimpleCurve::setPoint( int iPoint, double xIn, double yIn,
                               double zIn, double mIn )

{
    if( !(flags & OGR_G_3D) )
        Make3D();
    if( !(flags & OGR_G_MEASURED) )
        AddM();

    if( iPoint >= nPointCount )
    {
        setNumPoints( iPoint+1 );
        if( nPointCount < iPoint + 1 )
            return;
    }
#ifdef DEBUG
    if( paoPoints == NULL )
        return;
#endif

    paoPoints[iPoint].x = xIn;
    paoPoints[iPoint].y = yIn;

    if( padfZ != NULL )
    {
        padfZ[iPoint] = zIn;
    }
    if( padfM != NULL )
    {
        padfM[iPoint] = mIn;
    }
}

/**
 * \brief Set the location of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accommodate the request.
 *
 * There is no SFCOM analog to this method.
 *
 * @param iPoint the index of the vertex to assign (zero based).
 * @param xIn input X coordinate to assign.
 * @param yIn input Y coordinate to assign.
 */

void OGRSimpleCurve::setPoint( int iPoint, double xIn, double yIn )

{
    if( iPoint >= nPointCount )
    {
        setNumPoints( iPoint+1 );
        if( nPointCount < iPoint + 1 )
            return;
    }

    paoPoints[iPoint].x = xIn;
    paoPoints[iPoint].y = yIn;
}

/************************************************************************/
/*                                setZ()                                */
/************************************************************************/

/**
 * \brief Set the Z of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accommodate the request.
 *
 * There is no SFCOM analog to this method.
 *
 * @param iPoint the index of the vertex to assign (zero based).
 * @param zIn input Z coordinate to assign.
 */

void OGRSimpleCurve::setZ( int iPoint, double zIn )
{
    if( getCoordinateDimension() == 2 )
        Make3D();

    if( iPoint >= nPointCount )
    {
        setNumPoints( iPoint+1 );
        if( nPointCount < iPoint + 1 )
            return;
    }

    if( padfZ != NULL )
        padfZ[iPoint] = zIn;
}

/************************************************************************/
/*                                setM()                                */
/************************************************************************/

/**
 * \brief Set the M of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accommodate the request.
 *
 * There is no SFCOM analog to this method.
 *
 * @param iPoint the index of the vertex to assign (zero based).
 * @param mIn input M coordinate to assign.
 */

void OGRSimpleCurve::setM( int iPoint, double mIn )
{
    if( !(flags & OGR_G_MEASURED) )
        AddM();

    if( iPoint >= nPointCount )
    {
        setNumPoints( iPoint+1 );
        if( nPointCount < iPoint + 1 )
            return;
    }

    if( padfM != NULL )
        padfM[iPoint] = mIn;
}

/************************************************************************/
/*                              addPoint()                              */
/************************************************************************/

/**
 * \brief Add a point to a line string.
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * There is no SFCOM analog to this method.
 *
 * @param poPoint the point to assign to the new vertex.
 */

void OGRSimpleCurve::addPoint( const OGRPoint * poPoint )

{
    if( poPoint->getCoordinateDimension() < 3 )
        setPoint( nPointCount, poPoint->getX(), poPoint->getY() );
    else
        setPoint( nPointCount, poPoint->getX(), poPoint->getY(), poPoint->getZ() );
}

/************************************************************************/
/*                              addPoint()                              */
/************************************************************************/

/**
 * \brief Add a point to a line string.
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * There is no SFCOM analog to this method.
 *
 * @param x the X coordinate to assign to the new point.
 * @param y the Y coordinate to assign to the new point.
 * @param z the Z coordinate to assign to the new point (defaults to zero).
 * @param m the M coordinate to assign to the new point (defaults to zero).
 */

void OGRSimpleCurve::addPoint( double x, double y, double z, double m )

{
    setPoint( nPointCount, x, y, z, m );
}

/**
 * \brief Add a point to a line string.
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * There is no SFCOM analog to this method.
 *
 * @param x the X coordinate to assign to the new point.
 * @param y the Y coordinate to assign to the new point.
 * @param z the Z coordinate to assign to the new point (defaults to zero).
 */

void OGRSimpleCurve::addPoint( double x, double y, double z )

{
    setPoint( nPointCount, x, y, z );
}

/**
 * \brief Add a point to a line string.
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * There is no SFCOM analog to this method.
 *
 * @param x the X coordinate to assign to the new point.
 * @param y the Y coordinate to assign to the new point.
 */

void OGRSimpleCurve::addPoint( double x, double y )

{
    setPoint( nPointCount, x, y );
}

/**
 * \brief Add a point to a line string.
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * There is no SFCOM analog to this method.
 *
 * @param x the X coordinate to assign to the new point.
 * @param y the Y coordinate to assign to the new point.
 * @param m the M coordinate to assign to the new point.
 */

void OGRSimpleCurve::addPointM( double x, double y, double m )

{
    setPointM( nPointCount, x, y, m );
}

/************************************************************************/
/*                             setPointsM()                             */
/************************************************************************/

/**
 * \brief Assign all points in a line string.
 *
 * This method clears any existing points assigned to this line string,
 * and assigns a whole new set.  It is the most efficient way of assigning
 * the value of a line string.
 *
 * There is no SFCOM analog to this method.
 *
 * @param nPointsIn number of points being passed in paoPointsIn
 * @param paoPointsIn list of points being assigned.
 * @param padfMIn the M values that go with the points.
 */

void OGRSimpleCurve::setPointsM( int nPointsIn, OGRRawPoint * paoPointsIn,
                                 double * padfMIn )

{
    setNumPoints( nPointsIn, FALSE );
    if( nPointCount < nPointsIn
#ifdef DEBUG
        || paoPoints == NULL
#endif
        )
        return;

    if( nPointsIn )
        memcpy( paoPoints, paoPointsIn, sizeof(OGRRawPoint) * nPointsIn);

/* -------------------------------------------------------------------- */
/*      Check measures.                                                 */
/* -------------------------------------------------------------------- */
    if( padfMIn == NULL && (flags & OGR_G_MEASURED) )
    {
        RemoveM();
    }
    else if( padfMIn )
    {
        AddM();
        if( padfM && nPointsIn )
            memcpy( padfM, padfMIn, sizeof(double) * nPointsIn );
    }
}

/************************************************************************/
/*                             setPoints()                              */
/************************************************************************/

/**
 * \brief Assign all points in a line string.
 *
 * This method clears any existing points assigned to this line string,
 * and assigns a whole new set.  It is the most efficient way of assigning
 * the value of a line string.
 *
 * There is no SFCOM analog to this method.
 *
 * @param nPointsIn number of points being passed in paoPointsIn
 * @param paoPointsIn list of points being assigned.
 * @param padfZIn the Z values that go with the points.
 * @param padfMIn the M values that go with the points.
 */

void OGRSimpleCurve::setPoints( int nPointsIn, OGRRawPoint * paoPointsIn,
                                double * padfZIn, double * padfMIn )

{
    setNumPoints( nPointsIn, FALSE );
    if( nPointCount < nPointsIn
#ifdef DEBUG
        || paoPoints == NULL
#endif
        )
        return;

    if( nPointsIn )
        memcpy( paoPoints, paoPointsIn, sizeof(OGRRawPoint) * nPointsIn);

/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZIn == NULL && getCoordinateDimension() > 2 )
    {
        Make2D();
    }
    else if( padfZIn )
    {
        Make3D();
        if( padfZ && nPointsIn )
            memcpy( padfZ, padfZIn, sizeof(double) * nPointsIn );
    }

/* -------------------------------------------------------------------- */
/*      Check measures.                                                 */
/* -------------------------------------------------------------------- */
    if( padfMIn == NULL && (flags & OGR_G_MEASURED) )
    {
        RemoveM();
    }
    else if( padfMIn )
    {
        AddM();
        if( padfM && nPointsIn )
            memcpy( padfM, padfMIn, sizeof(double) * nPointsIn );
    }
}

/************************************************************************/
/*                             setPoints()                              */
/************************************************************************/

/**
 * \brief Assign all points in a line string.
 *
 * This method clears any existing points assigned to this line string,
 * and assigns a whole new set.  It is the most efficient way of assigning
 * the value of a line string.
 *
 * There is no SFCOM analog to this method.
 *
 * @param nPointsIn number of points being passed in paoPointsIn
 * @param paoPointsIn list of points being assigned.
 * @param padfZIn the Z values that go with the points (optional, may be NULL).
 */

void OGRSimpleCurve::setPoints( int nPointsIn, OGRRawPoint * paoPointsIn,
                               double * padfZIn )

{
    setNumPoints( nPointsIn, FALSE );
    if( nPointCount < nPointsIn
#ifdef DEBUG
        || paoPoints == NULL
#endif
        )
        return;

    if( nPointsIn )
        memcpy( paoPoints, paoPointsIn, sizeof(OGRRawPoint) * nPointsIn);

/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZIn == NULL && getCoordinateDimension() > 2 )
    {
        Make2D();
    }
    else if( padfZIn )
    {
        Make3D();
        if( padfZ && nPointsIn )
            memcpy( padfZ, padfZIn, sizeof(double) * nPointsIn );
    }
}

/************************************************************************/
/*                             setPoints()                              */
/************************************************************************/

/**
 * \brief Assign all points in a line string.
 *
 * This method clear any existing points assigned to this line string,
 * and assigns a whole new set.
 *
 * There is no SFCOM analog to this method.
 *
 * @param nPointsIn number of points being passed in padfX and padfY.
 * @param padfX list of X coordinates of points being assigned.
 * @param padfY list of Y coordinates of points being assigned.
 * @param padfZIn list of Z coordinates of points being assigned (defaults to
 * NULL for 2D objects).
 */

void OGRSimpleCurve::setPoints( int nPointsIn, double * padfX, double * padfY,
                                double * padfZIn )

{
/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZIn == NULL )
        Make2D();
    else
        Make3D();

/* -------------------------------------------------------------------- */
/*      Assign values.                                                  */
/* -------------------------------------------------------------------- */
    setNumPoints( nPointsIn, FALSE );
    if( nPointCount < nPointsIn )
        return;

    for( int i = 0; i < nPointsIn; i++ )
    {
        paoPoints[i].x = padfX[i];
        paoPoints[i].y = padfY[i];
    }

    if( padfZ == NULL || !padfZIn || !nPointsIn )
    {
        return;
    }

    memcpy( padfZ, padfZIn, sizeof(double) * nPointsIn );
}

/************************************************************************/
/*                             setPointsM()                             */
/************************************************************************/

/**
 * \brief Assign all points in a line string.
 *
 * This method clear any existing points assigned to this line string,
 * and assigns a whole new set.
 *
 * There is no SFCOM analog to this method.
 *
 * @param nPointsIn number of points being passed in padfX and padfY.
 * @param padfX list of X coordinates of points being assigned.
 * @param padfY list of Y coordinates of points being assigned.
 * @param padfMIn list of M coordinates of points being assigned.
 */

void OGRSimpleCurve::setPointsM( int nPointsIn, double * padfX, double * padfY,
                                 double * padfMIn )

{
/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfMIn == NULL )
        RemoveM();
    else
        AddM();

/* -------------------------------------------------------------------- */
/*      Assign values.                                                  */
/* -------------------------------------------------------------------- */
    setNumPoints( nPointsIn, FALSE );
    if( nPointCount < nPointsIn )
        return;

    for( int i = 0; i < nPointsIn; i++ )
    {
        paoPoints[i].x = padfX[i];
        paoPoints[i].y = padfY[i];
    }

    if( padfMIn == NULL || !padfM || !nPointsIn )
    {
        return;
    }

    memcpy( padfM, padfMIn, sizeof(double) * nPointsIn );
}

/************************************************************************/
/*                             setPoints()                              */
/************************************************************************/

/**
 * \brief Assign all points in a line string.
 *
 * This method clear any existing points assigned to this line string,
 * and assigns a whole new set.
 *
 * There is no SFCOM analog to this method.
 *
 * @param nPointsIn number of points being passed in padfX and padfY.
 * @param padfX list of X coordinates of points being assigned.
 * @param padfY list of Y coordinates of points being assigned.
 * @param padfZIn list of Z coordinates of points being assigned.
 * @param padfMIn list of M coordinates of points being assigned.
 */

void OGRSimpleCurve::setPoints( int nPointsIn, double * padfX, double * padfY,
                                double * padfZIn, double * padfMIn )

{
/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZIn == NULL )
        Make2D();
    else
        Make3D();

/* -------------------------------------------------------------------- */
/*      Check measures.                                                 */
/* -------------------------------------------------------------------- */
    if( padfMIn == NULL )
        RemoveM();
    else
        AddM();

/* -------------------------------------------------------------------- */
/*      Assign values.                                                  */
/* -------------------------------------------------------------------- */
    setNumPoints( nPointsIn, FALSE );
    if( nPointCount < nPointsIn )
        return;

    for( int i = 0; i < nPointsIn; i++ )
    {
        paoPoints[i].x = padfX[i];
        paoPoints[i].y = padfY[i];
    }

    if( padfZ != NULL && padfZIn && nPointsIn )
        memcpy( padfZ, padfZIn, sizeof(double) * nPointsIn );
    if( padfM != NULL && padfMIn && nPointsIn )
        memcpy( padfM, padfMIn, sizeof(double) * nPointsIn );
}

/************************************************************************/
/*                          getPoints()                                 */
/************************************************************************/

/**
 * \brief Returns all points of line string.
 *
 * This method copies all points into user list. This list must be at
 * least sizeof(OGRRawPoint) * OGRGeometry::getNumPoints() byte in size.
 * It also copies all Z coordinates.
 *
 * There is no SFCOM analog to this method.
 *
 * @param paoPointsOut a buffer into which the points is written.
 * @param padfZOut the Z values that go with the points (optional, may be NULL).
 */

void OGRSimpleCurve::getPoints( OGRRawPoint * paoPointsOut,
                                double * padfZOut ) const
{
    if( !paoPointsOut || nPointCount == 0 )
        return;

    memcpy( paoPointsOut, paoPoints, sizeof(OGRRawPoint) * nPointCount );

/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZOut )
    {
        if( padfZ )
            memcpy( padfZOut, padfZ, sizeof(double) * nPointCount );
        else
            memset( padfZOut, 0, sizeof(double) * nPointCount );
    }
}

/************************************************************************/
/*                          getPoints()                                 */
/************************************************************************/

/**
 * \brief Returns all points of line string.
 *
 * This method copies all points into user arrays. The user provides the
 * stride between 2 consecutive elements of the array.
 *
 * On some CPU architectures, care must be taken so that the arrays are properly
 * aligned.
 *
 * There is no SFCOM analog to this method.
 *
 * @param pabyX a buffer of at least (sizeof(double) * nXStride * nPointCount)
 * bytes, may be NULL.
 * @param nXStride the number of bytes between 2 elements of pabyX.
 * @param pabyY a buffer of at least (sizeof(double) * nYStride * nPointCount)
 * bytes, may be NULL.
 * @param nYStride the number of bytes between 2 elements of pabyY.
 * @param pabyZ a buffer of at last size (sizeof(double) * nZStride *
 * nPointCount) bytes, may be NULL.
 * @param nZStride the number of bytes between 2 elements of pabyZ.
 *
 * @since OGR 1.9.0
 */

void OGRSimpleCurve::getPoints( void* pabyX, int nXStride,
                                void* pabyY, int nYStride,
                                void* pabyZ, int nZStride ) const
{
    if( pabyX != NULL && nXStride == 0 )
        return;
    if( pabyY != NULL && nYStride == 0 )
        return;
    if( pabyZ != NULL && nZStride == 0 )
        return;
    if( nXStride == 2 * sizeof(double) &&
        nYStride == 2 * sizeof(double) &&
        static_cast<char *>(pabyY) ==
        static_cast<char *>(pabyX) + sizeof(double) &&
        (pabyZ == NULL || nZStride == sizeof(double)) )
    {
        getPoints(static_cast<OGRRawPoint *>(pabyX),
                  static_cast<double *>(pabyZ));
        return;
    }
    for( int i = 0; i < nPointCount; i++ )
    {
        if( pabyX )
            *(double*)(static_cast<char*>(pabyX) + i * nXStride) = paoPoints[i].x;
        if( pabyY )
            *(double*)(static_cast<char*>(pabyY) + i * nYStride) = paoPoints[i].y;
    }

    if( pabyZ )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            *(double*)(static_cast<char*>(pabyZ) + i * nZStride) =
                padfZ ? padfZ[i] : 0.0;
        }
    }
}

/**
 * \brief Returns all points of line string.
 *
 * This method copies all points into user arrays. The user provides the
 * stride between 2 consecutive elements of the array.
 *
 * On some CPU architectures, care must be taken so that the arrays are properly aligned.
 *
 * There is no SFCOM analog to this method.
 *
 * @param pabyX a buffer of at least (sizeof(double) * nXStride * nPointCount) bytes, may be NULL.
 * @param nXStride the number of bytes between 2 elements of pabyX.
 * @param pabyY a buffer of at least (sizeof(double) * nYStride * nPointCount) bytes, may be NULL.
 * @param nYStride the number of bytes between 2 elements of pabyY.
 * @param pabyZ a buffer of at last size (sizeof(double) * nZStride * nPointCount) bytes, may be NULL.
 * @param nZStride the number of bytes between 2 elements of pabyZ.
 * @param pabyM a buffer of at last size (sizeof(double) * nMStride * nPointCount) bytes, may be NULL.
 * @param nMStride the number of bytes between 2 elements of pabyM.
 *
 * @since OGR 2.1.0
 */

void OGRSimpleCurve::getPoints( void* pabyX, int nXStride,
                                void* pabyY, int nYStride,
                                void* pabyZ, int nZStride,
                                void* pabyM, int nMStride ) const
{
    if( pabyX != NULL && nXStride == 0 )
        return;
    if( pabyY != NULL && nYStride == 0 )
        return;
    if( pabyZ != NULL && nZStride == 0 )
        return;
    if( pabyM != NULL && nMStride == 0 )
        return;
    for( int i = 0; i < nPointCount; i++ )
    {
        if( pabyX ) *(double*)((char*)pabyX + i * nXStride) = paoPoints[i].x;
        if( pabyY ) *(double*)((char*)pabyY + i * nYStride) = paoPoints[i].y;
    }

    if( pabyZ )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            *(double*)((char*)pabyZ + i * nZStride) = (padfZ) ? padfZ[i] : 0.0;
        }
    }
    if( pabyM )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            *(double*)((char*)pabyM + i * nZStride) = (padfM) ? padfM[i] : 0.0;
        }
    }
}

/************************************************************************/
/*                           reversePoints()                            */
/************************************************************************/

/**
 * \brief Reverse point order.
 *
 * This method updates the points in this line string in place
 * reversing the point ordering (first for last, etc).
 */

void OGRSimpleCurve::reversePoints()

{
    for( int i = 0; i < nPointCount/2; i++ )
    {
        const OGRRawPoint sPointTemp = paoPoints[i];

        paoPoints[i] = paoPoints[nPointCount-i-1];
        paoPoints[nPointCount-i-1] = sPointTemp;

        if( padfZ )
        {
            const double dfZTemp = padfZ[i];

            padfZ[i] = padfZ[nPointCount-i-1];
            padfZ[nPointCount-i-1] = dfZTemp;
        }
    }
}

/************************************************************************/
/*                          addSubLineString()                          */
/************************************************************************/

/**
 * \brief Add a segment of another linestring to this one.
 *
 * Adds the request range of vertices to the end of this line string
 * in an efficient manner.  If the nStartVertex is larger than the
 * nEndVertex then the vertices will be reversed as they are copied.
 *
 * @param poOtherLine the other OGRLineString.
 * @param nStartVertex the first vertex to copy, defaults to 0 to start
 * with the first vertex in the other linestring.
 * @param nEndVertex the last vertex to copy, defaults to -1 indicating
 * the last vertex of the other line string.
 */

void OGRSimpleCurve::addSubLineString( const OGRLineString *poOtherLine,
                                      int nStartVertex, int nEndVertex )

{
    int nOtherLineNumPoints = poOtherLine->getNumPoints();
    if( nOtherLineNumPoints == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Do a bit of argument defaulting and validation.                 */
/* -------------------------------------------------------------------- */
    if( nEndVertex == -1 )
        nEndVertex = nOtherLineNumPoints - 1;

    if( nStartVertex < 0 || nEndVertex < 0
        || nStartVertex >= nOtherLineNumPoints
        || nEndVertex >= nOtherLineNumPoints )
    {
        CPLAssert( false );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Grow this linestring to hold the additional points.             */
/* -------------------------------------------------------------------- */
    int nOldPoints = nPointCount;
    int nPointsToAdd = std::abs(nEndVertex - nStartVertex) + 1;

    setNumPoints( nPointsToAdd + nOldPoints, FALSE );
    if( nPointCount < nPointsToAdd + nOldPoints
#ifdef DEBUG
        || paoPoints == NULL
#endif
        )
        return;

/* -------------------------------------------------------------------- */
/*      Copy the x/y points - forward copies use memcpy.                */
/* -------------------------------------------------------------------- */
    if( nEndVertex >= nStartVertex )
    {
        memcpy( paoPoints + nOldPoints,
                poOtherLine->paoPoints + nStartVertex,
                sizeof(OGRRawPoint) * nPointsToAdd );
        if( poOtherLine->padfZ != NULL )
        {
            Make3D();
            if( padfZ != NULL )
            {
                memcpy( padfZ + nOldPoints, poOtherLine->padfZ + nStartVertex,
                        sizeof(double) * nPointsToAdd );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy the x/y points - reverse copies done double by double.     */
/* -------------------------------------------------------------------- */
    else
    {
        for( int i = 0; i < nPointsToAdd; i++ )
        {
            paoPoints[i+nOldPoints].x =
                poOtherLine->paoPoints[nStartVertex-i].x;
            paoPoints[i+nOldPoints].y =
                poOtherLine->paoPoints[nStartVertex-i].y;
        }

        if( poOtherLine->padfZ != NULL )
        {
            Make3D();
            if( padfZ != NULL )
            {
                for( int i = 0; i < nPointsToAdd; i++ )
                {
                    padfZ[i+nOldPoints] = poOtherLine->padfZ[nStartVertex-i];
                }
            }
        }
    }
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRSimpleCurve::importFromWkb( const unsigned char *pabyData,
                                      int nSize,
                                      OGRwkbVariant eWkbVariant,
                                      int& nBytesConsumedOut )

{
    OGRwkbByteOrder     eByteOrder;
    int                 nDataOffset = 0;
    int                 nNewNumPoints = 0;

    nBytesConsumedOut = -1;
    OGRErr eErr = importPreambuleOfCollectionFromWkb( pabyData,
                                                      nSize,
                                                      nDataOffset,
                                                      eByteOrder,
                                                      16,
                                                      nNewNumPoints,
                                                      eWkbVariant );
    if( eErr != OGRERR_NONE )
        return eErr;

    // Check if the wkb stream buffer is big enough to store
    // fetched number of points.
    const int dim = CoordinateDimension();
    int nPointSize = dim*sizeof(double);
    if( nNewNumPoints < 0 || nNewNumPoints > INT_MAX / nPointSize )
        return OGRERR_CORRUPT_DATA;
    int nBufferMinSize = nPointSize * nNewNumPoints;

    if( nSize != -1 && nBufferMinSize > nSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Length of input WKB is too small" );
        return OGRERR_NOT_ENOUGH_DATA;
    }

    setNumPoints( nNewNumPoints, FALSE );
    if( nPointCount < nNewNumPoints )
        return OGRERR_FAILURE;

    nBytesConsumedOut = 9 + 8 * nPointCount *
                                    (2 + ((flags & OGR_G_3D) ? 1 : 0)+
                                         ((flags & OGR_G_MEASURED) ? 1 : 0));

/* -------------------------------------------------------------------- */
/*      Get the vertex.                                                 */
/* -------------------------------------------------------------------- */
    int i = 0;

    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            memcpy( paoPoints + i, pabyData + 9 + i*32, 16 );
            memcpy( padfZ + i, pabyData + 9 + 16 + i*32, 8 );
            memcpy( padfM + i, pabyData + 9 + 24 + i*32, 8 );
        }
    }
    else if( flags & OGR_G_MEASURED )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            memcpy( paoPoints + i, pabyData + 9 + i*24, 16 );
            memcpy( padfM + i, pabyData + 9 + 16 + i*24, 8 );
        }
    }
    else if( flags & OGR_G_3D )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            memcpy( paoPoints + i, pabyData + 9 + i*24, 16 );
            memcpy( padfZ + i, pabyData + 9 + 16 + i*24, 8 );
        }
    }
    else if( nPointCount )
    {
        memcpy( paoPoints, pabyData + 9, 16 * nPointCount );
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
        }

        if( flags & OGR_G_3D )
        {
            for( i = 0; i < nPointCount; i++ )
            {
                CPL_SWAPDOUBLE( padfZ + i );
            }
        }

        if( flags & OGR_G_MEASURED )
        {
            for( i = 0; i < nPointCount; i++ )
            {
                CPL_SWAPDOUBLE( padfM + i );
            }
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr OGRSimpleCurve::exportToWkb( OGRwkbByteOrder eByteOrder,
                                    unsigned char * pabyData,
                                    OGRwkbVariant eWkbVariant ) const

{
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = getGeometryType();

    if( eWkbVariant == wkbVariantPostGIS1 )
    {
        nGType = wkbFlatten(nGType);
        if( Is3D() )
            // Explicitly set wkb25DBit.
            nGType = (OGRwkbGeometryType)(nGType | wkb25DBitInternalUse);
        if( IsMeasured() )
            nGType = (OGRwkbGeometryType)(nGType | 0x40000000);
    }
    else if( eWkbVariant == wkbVariantIso )
        nGType = getIsoGeometryType();

    if( eByteOrder == wkbNDR )
        nGType = CPL_LSBWORD32( nGType );
    else
        nGType = CPL_MSBWORD32( nGType );

    memcpy( pabyData + 1, &nGType, 4 );

/* -------------------------------------------------------------------- */
/*      Copy in the data count.                                         */
/* -------------------------------------------------------------------- */
    memcpy( pabyData+5, &nPointCount, 4 );

/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            memcpy( pabyData + 9 + 32*i, paoPoints+i, 16 );
            memcpy( pabyData + 9 + 16 + 32*i, padfZ+i, 8 );
            memcpy( pabyData + 9 + 24 + 32*i, padfM+i, 8 );
        }
    }
    else if( flags & OGR_G_MEASURED )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            memcpy( pabyData + 9 + 24*i, paoPoints+i, 16 );
            memcpy( pabyData + 9 + 16 + 24*i, padfM+i, 8 );
        }
    }
    else if( flags & OGR_G_3D )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            memcpy( pabyData + 9 + 24*i, paoPoints+i, 16 );
            memcpy( pabyData + 9 + 16 + 24*i, padfZ+i, 8 );
        }
    }
    else if( nPointCount )
        memcpy( pabyData+9, paoPoints, 16 * nPointCount );

/* -------------------------------------------------------------------- */
/*      Swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        int nCount = CPL_SWAP32( nPointCount );
        memcpy( pabyData+5, &nCount, 4 );

        for( int i = CoordinateDimension() * nPointCount - 1; i >= 0; i-- )
        {
            CPL_SWAP64PTR( pabyData + 9 + 8 * i );
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

OGRErr OGRSimpleCurve::importFromWkt( char ** ppszInput )

{
    int bHasZ = FALSE;
    int bHasM = FALSE;
    bool bIsEmpty = false;
    const OGRErr eErr =
        importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    flags = 0;
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bHasZ ) flags |= OGR_G_3D;
    if( bHasM ) flags |= OGR_G_MEASURED;
    if( bIsEmpty )
    {
        // we should be at the end
        if( !((*ppszInput[0] == '\000') || (*ppszInput[0] == ',')) )
            return OGRERR_CORRUPT_DATA;
        return OGRERR_NONE;
    }

    const char *pszInput = *ppszInput;

/* -------------------------------------------------------------------- */
/*      Read the point list.                                            */
/* -------------------------------------------------------------------- */
    int flagsFromInput = flags;
    nPointCount = 0;

    int nMaxPoints = 0;
    pszInput = OGRWktReadPointsM( pszInput, &paoPoints, &padfZ, &padfM,
                                  &flagsFromInput,
                                  &nMaxPoints, &nPointCount );
    if( pszInput == NULL )
        return OGRERR_CORRUPT_DATA;

    if( (flagsFromInput & OGR_G_3D) && !(flags & OGR_G_3D) )
    {
        set3D(TRUE);
    }
    if( (flagsFromInput & OGR_G_MEASURED) && !(flags & OGR_G_MEASURED) )
    {
        setMeasured(TRUE);
    }

    *ppszInput = (char *) pszInput;

    return OGRERR_NONE;
}

//! @cond Doxygen_Suppress
/************************************************************************/
/*                        importFromWKTListOnly()                       */
/*                                                                      */
/*      Instantiate from "(x y, x y, ...)"                              */
/************************************************************************/

OGRErr OGRSimpleCurve::importFromWKTListOnly( char ** ppszInput,
                                              int bHasZ, int bHasM,
                                              OGRRawPoint*& paoPointsIn,
                                              int& nMaxPointsIn,
                                              double*& padfZIn )

{
    const char *pszInput = *ppszInput;

/* -------------------------------------------------------------------- */
/*      Read the point list.                                            */
/* -------------------------------------------------------------------- */
    int flagsFromInput = flags;
    int nPointCountRead = 0;
    double *padfMIn = NULL;
    if( flagsFromInput == 0 )  // Flags was not set, this is not called by us.
    {
        if( bHasM )
            flagsFromInput |= OGR_G_MEASURED;
        if( bHasZ )
            flagsFromInput |= OGR_G_3D;
    }

    pszInput = OGRWktReadPointsM( pszInput, &paoPointsIn, &padfZIn, &padfMIn,
                                  &flagsFromInput,
                                  &nMaxPointsIn, &nPointCountRead );

    if( pszInput == NULL )
    {
        CPLFree( padfMIn );
        return OGRERR_CORRUPT_DATA;
    }
    if( (flagsFromInput & OGR_G_3D) && !(flags & OGR_G_3D) )
    {
        flags |= OGR_G_3D;
        bHasZ = TRUE;
    }
    if( (flagsFromInput & OGR_G_MEASURED) && !(flags & OGR_G_MEASURED) )
    {
        flags |= OGR_G_MEASURED;
        bHasM = TRUE;
    }

    *ppszInput = (char *) pszInput;

    if( bHasM && bHasZ )
        setPoints( nPointCountRead, paoPointsIn, padfZIn, padfMIn );
    else if( bHasM && !bHasZ )
        setPointsM( nPointCountRead, paoPointsIn, padfMIn );
    else
        setPoints( nPointCountRead, paoPointsIn, padfZIn );

    CPLFree( padfMIn );

    return OGRERR_NONE;
}
//! @endcond

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivalent.  This could be made a lot more CPU efficient.       */
/************************************************************************/

OGRErr OGRSimpleCurve::exportToWkt( char ** ppszDstText,
                                   OGRwkbVariant eWkbVariant ) const

{
    const size_t nMaxString = static_cast<size_t>(nPointCount) * 40 * 4 + 26;

/* -------------------------------------------------------------------- */
/*      Handle special empty case.                                      */
/* -------------------------------------------------------------------- */
    if( IsEmpty() )
    {
        CPLString osEmpty;
        if( eWkbVariant == wkbVariantIso )
        {
            if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
                osEmpty.Printf("%s ZM EMPTY", getGeometryName());
            else if( flags & OGR_G_MEASURED )
                osEmpty.Printf("%s M EMPTY", getGeometryName());
            else if( flags & OGR_G_3D )
                osEmpty.Printf("%s Z EMPTY", getGeometryName());
            else
                osEmpty.Printf("%s EMPTY", getGeometryName());
        }
        else
            osEmpty.Printf("%s EMPTY", getGeometryName());
        *ppszDstText = CPLStrdup(osEmpty);
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      General case.                                                   */
/* -------------------------------------------------------------------- */
    *ppszDstText = static_cast<char *>(VSI_MALLOC_VERBOSE( nMaxString ));
    if( *ppszDstText == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

    if( eWkbVariant == wkbVariantIso )
    {
        if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
            snprintf( *ppszDstText, nMaxString, "%s ZM (", getGeometryName() );
        else if( flags & OGR_G_MEASURED )
            snprintf( *ppszDstText, nMaxString, "%s M (", getGeometryName() );
        else if( flags & OGR_G_3D )
            snprintf( *ppszDstText, nMaxString, "%s Z (", getGeometryName() );
        else
            snprintf( *ppszDstText, nMaxString, "%s (", getGeometryName() );
    }
    else
        snprintf( *ppszDstText, nMaxString, "%s (", getGeometryName() );

    OGRBoolean hasZ = Is3D();
    OGRBoolean hasM = IsMeasured();
    if( eWkbVariant != wkbVariantIso )
        hasM = FALSE;

    size_t nRetLen = 0;

    for( int i = 0; i < nPointCount; i++ )
    {
        if( nMaxString <= strlen(*ppszDstText+nRetLen) + 32 + nRetLen )
        {
            CPLDebug( "OGR",
                      "OGRSimpleCurve::exportToWkt() ... buffer overflow.\n"
                      "nMaxString=%d, strlen(*ppszDstText) = %d, i=%d\n"
                      "*ppszDstText = %s",
                      static_cast<int>(nMaxString),
                      static_cast<int>(strlen(*ppszDstText)), i, *ppszDstText );

            VSIFree( *ppszDstText );
            *ppszDstText = NULL;
            return OGRERR_NOT_ENOUGH_MEMORY;
        }

        if( i > 0 )
            strcat( *ppszDstText + nRetLen, "," );

        nRetLen += strlen(*ppszDstText + nRetLen);
        OGRMakeWktCoordinateM( *ppszDstText + nRetLen,
                               paoPoints[i].x,
                               paoPoints[i].y,
                               padfZ ? padfZ[i] : 0.0,
                               padfM ? padfM[i] : 0.0,
                               hasZ, hasM );

        nRetLen += strlen(*ppszDstText + nRetLen);
    }

    strcat( *ppszDstText+nRetLen, ")" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             get_Length()                             */
/*                                                                      */
/*      For now we return a simple euclidean 2D distance.               */
/************************************************************************/

double OGRSimpleCurve::get_Length() const

{
    double dfLength = 0.0;

    for( int i = 0; i < nPointCount-1; i++ )
    {

        const double dfDeltaX = paoPoints[i+1].x - paoPoints[i].x;
        const double dfDeltaY = paoPoints[i+1].y - paoPoints[i].y;
        dfLength += sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);
    }

    return dfLength;
}

/************************************************************************/
/*                             StartPoint()                             */
/************************************************************************/

void OGRSimpleCurve::StartPoint( OGRPoint * poPoint ) const

{
    getPoint( 0, poPoint );
}

/************************************************************************/
/*                              EndPoint()                              */
/************************************************************************/

void OGRSimpleCurve::EndPoint( OGRPoint * poPoint ) const

{
    getPoint( nPointCount-1, poPoint );
}

/************************************************************************/
/*                               Value()                                */
/*                                                                      */
/*      Get an interpolated point at some distance along the curve.     */
/************************************************************************/

void OGRSimpleCurve::Value( double dfDistance, OGRPoint * poPoint ) const

{
    if( dfDistance < 0 )
    {
        StartPoint( poPoint );
        return;
    }

    double dfLength = 0.0;

    for( int i = 0; i < nPointCount-1; i++ )
    {
        const double dfDeltaX = paoPoints[i+1].x - paoPoints[i].x;
        const double dfDeltaY = paoPoints[i+1].y - paoPoints[i].y;
        const double dfSegLength = sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);

        if( dfSegLength > 0 )
        {
            if( (dfLength <= dfDistance) && ((dfLength + dfSegLength) >=
                                             dfDistance) )
            {
                double dfRatio = (dfDistance - dfLength) / dfSegLength;

                poPoint->setX( paoPoints[i].x * (1 - dfRatio)
                               + paoPoints[i+1].x * dfRatio );
                poPoint->setY( paoPoints[i].y * (1 - dfRatio)
                               + paoPoints[i+1].y * dfRatio );

                if( getCoordinateDimension() == 3 )
                    poPoint->setZ( padfZ[i] * (1 - dfRatio)
                                   + padfZ[i+1] * dfRatio );

                return;
            }

            dfLength += dfSegLength;
        }
    }

    EndPoint( poPoint );
}

/************************************************************************/
/*                              Project()                               */
/*                                                                      */
/* Return distance of point projected on line from origin of this line. */
/************************************************************************/

/**
* \brief Project point on linestring.
*
* The input point projected on linestring. This is the shortest distance
* from point to the linestring. The distance from begin of linestring to
* the point projection returned.
*
* This method is built on the GEOS library (GEOS >= 3.2.0), check it for the
* definition of the geometry operation.
* If OGR is built without the GEOS library, this method will always return -1,
* issuing a CPLE_NotSupported error.
*
* @return a distance from the begin of the linestring to the projected point.
*
* @since OGR 1.11.0
*/

// GEOS >= 3.2.0 for project capability.
#if defined(HAVE_GEOS)
#if GEOS_VERSION_MAJOR > 3 || (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 2)
#define HAVE_GEOS_PROJECT
#endif
#endif

double OGRSimpleCurve::Project(const OGRPoint *
#ifdef HAVE_GEOS_PROJECT
                                poPoint
#endif
                               ) const

{
    double dfResult = -1;
#ifndef HAVE_GEOS_PROJECT
    CPLError(CE_Failure, CPLE_NotSupported,
        "GEOS support not enabled.");
    return dfResult;
#else
    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hPointGeosGeom = NULL;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    hPointGeosGeom = poPoint->exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL && hPointGeosGeom != NULL )
    {
        dfResult = GEOSProject_r(hGEOSCtxt, hThisGeosGeom, hPointGeosGeom);
    }
    GEOSGeom_destroy_r(hGEOSCtxt, hThisGeosGeom);
    GEOSGeom_destroy_r(hGEOSCtxt, hPointGeosGeom);
    freeGEOSContext(hGEOSCtxt);

    return dfResult;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                            getSubLine()                              */
/*                                                                      */
/*  Extracts a portion of this OGRLineString into a new OGRLineString.  */
/************************************************************************/

/**
* \brief Get the portion of linestring.
*
* The portion of the linestring extracted to new one. The input distances
* (maybe present as ratio of length of linestring) set begin and end of
* extracted portion.
*
* @param dfDistanceFrom The distance from the origin of linestring, where the subline should begins
* @param dfDistanceTo The distance from the origin of linestring, where the subline should ends
* @param bAsRatio The flag indicating that distances are the ratio of the linestring length.
*
* @return a newly allocated linestring now owned by the caller, or NULL on failure.
*
* @since OGR 1.11.0
*/

OGRLineString* OGRSimpleCurve::getSubLine(double dfDistanceFrom,
                                          double dfDistanceTo,
                                          int bAsRatio) const

{
    OGRLineString *poNewLineString = new OGRLineString();

    poNewLineString->assignSpatialReference(getSpatialReference());
    poNewLineString->setCoordinateDimension(getCoordinateDimension());

    const double dfLen = get_Length();
    if( bAsRatio == TRUE )
    {
        // Convert to real distance.
        dfDistanceFrom *= dfLen;
        dfDistanceTo *= dfLen;
    }

    if( dfDistanceFrom < 0 )
        dfDistanceFrom = 0;
    if( dfDistanceTo > dfLen )
        dfDistanceTo = dfLen;

    if( dfDistanceFrom > dfDistanceTo || dfDistanceFrom >= dfLen )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Input distances are invalid.");

        return NULL;
    }

    double dfLength = 0.0;

    // Get first point.

    int i = 0;  // Used after if blocks.
    if( dfDistanceFrom == 0 )
    {
        if( getCoordinateDimension() == 3 )
            poNewLineString->addPoint(paoPoints[0].x, paoPoints[0].y, padfZ[0]);
        else
            poNewLineString->addPoint(paoPoints[0].x, paoPoints[0].y);
    }
    else
    {
        for( i = 0; i < nPointCount - 1; i++ )
        {
            const double dfDeltaX = paoPoints[i + 1].x - paoPoints[i].x;
            const double dfDeltaY = paoPoints[i + 1].y - paoPoints[i].y;
            const double dfSegLength =
                sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);

            if( dfSegLength > 0 )
            {
                if( (dfLength <= dfDistanceFrom) && ((dfLength + dfSegLength) >=
                    dfDistanceFrom) )
                {
                    double dfRatio = (dfDistanceFrom - dfLength) / dfSegLength;

                    double dfX = paoPoints[i].x * (1 - dfRatio)
                        + paoPoints[i + 1].x * dfRatio;
                    double dfY = paoPoints[i].y * (1 - dfRatio)
                        + paoPoints[i + 1].y * dfRatio;

                    if( getCoordinateDimension() == 3 )
                    {
                        poNewLineString->
                            addPoint(dfX, dfY, padfZ[i] * (1 - dfRatio)
                        + padfZ[i+1] * dfRatio);
                    }
                    else
                    {
                        poNewLineString->addPoint(dfX, dfY);
                    }

                    // Check if dfDistanceTo is in same segment.
                    if( dfLength <= dfDistanceTo &&
                        (dfLength + dfSegLength) >= dfDistanceTo )
                    {
                        dfRatio = (dfDistanceTo - dfLength) / dfSegLength;

                        dfX = paoPoints[i].x * (1 - dfRatio)
                            + paoPoints[i + 1].x * dfRatio;
                        dfY = paoPoints[i].y * (1 - dfRatio)
                            + paoPoints[i + 1].y * dfRatio;

                        if( getCoordinateDimension() == 3 )
                        {
                            poNewLineString->
                                addPoint(dfX, dfY, padfZ[i] * (1 - dfRatio)
                                         + padfZ[i + 1] * dfRatio);
                        }
                        else
                        {
                            poNewLineString->addPoint(dfX, dfY);
                        }

                        if( poNewLineString->getNumPoints() < 2 )
                        {
                            delete poNewLineString;
                            poNewLineString = NULL;
                        }

                        return poNewLineString;
                    }
                    i++;
                    dfLength += dfSegLength;
                    break;
                }

                dfLength += dfSegLength;
            }
        }
    }

    // Add points.
    for( ; i < nPointCount - 1; i++ )
    {
        if( getCoordinateDimension() == 3 )
            poNewLineString->addPoint(paoPoints[i].x, paoPoints[i].y, padfZ[i]);
        else
            poNewLineString->addPoint(paoPoints[i].x, paoPoints[i].y);

        const double dfDeltaX = paoPoints[i + 1].x - paoPoints[i].x;
        const double dfDeltaY = paoPoints[i + 1].y - paoPoints[i].y;
        const double dfSegLength = sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);

        if( dfSegLength > 0 )
        {
            if( (dfLength <= dfDistanceTo) && ((dfLength + dfSegLength) >=
                dfDistanceTo) )
            {
                const double dfRatio = (dfDistanceTo - dfLength) / dfSegLength;

                const double dfX = paoPoints[i].x * (1 - dfRatio)
                    + paoPoints[i + 1].x * dfRatio;
                const double dfY = paoPoints[i].y * (1 - dfRatio)
                    + paoPoints[i + 1].y * dfRatio;

                if( getCoordinateDimension() == 3 )
                    poNewLineString->addPoint(dfX, dfY, padfZ[i] * (1 - dfRatio)
                    + padfZ[i + 1] * dfRatio);
                else
                    poNewLineString->addPoint(dfX, dfY);

                return poNewLineString;
            }

            dfLength += dfSegLength;
        }
    }

    if( getCoordinateDimension() == 3 )
        poNewLineString->
            addPoint(paoPoints[nPointCount - 1].x,
                     paoPoints[nPointCount - 1].y,
                     padfZ[nPointCount - 1]);
    else
        poNewLineString->
            addPoint(paoPoints[nPointCount - 1].x,
                     paoPoints[nPointCount - 1].y);

    if( poNewLineString->getNumPoints() < 2 )
    {
        delete poNewLineString;
        poNewLineString = NULL;
    }

    return poNewLineString;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRSimpleCurve::getEnvelope( OGREnvelope * psEnvelope ) const

{
    if( IsEmpty() )
    {
        psEnvelope->MinX = 0.0;
        psEnvelope->MaxX = 0.0;
        psEnvelope->MinY = 0.0;
        psEnvelope->MaxY = 0.0;
        return;
    }

    double dfMinX = paoPoints[0].x;
    double dfMaxX = paoPoints[0].x;
    double dfMinY = paoPoints[0].y;
    double dfMaxY = paoPoints[0].y;

    for( int iPoint = 1; iPoint < nPointCount; iPoint++ )
    {
        if( dfMaxX < paoPoints[iPoint].x )
            dfMaxX = paoPoints[iPoint].x;
        if( dfMaxY < paoPoints[iPoint].y )
            dfMaxY = paoPoints[iPoint].y;
        if( dfMinX > paoPoints[iPoint].x )
            dfMinX = paoPoints[iPoint].x;
        if( dfMinY > paoPoints[iPoint].y )
            dfMinY = paoPoints[iPoint].y;
    }

    psEnvelope->MinX = dfMinX;
    psEnvelope->MaxX = dfMaxX;
    psEnvelope->MinY = dfMinY;
    psEnvelope->MaxY = dfMaxY;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRSimpleCurve::getEnvelope( OGREnvelope3D * psEnvelope ) const

{
    getEnvelope((OGREnvelope*)psEnvelope);

    if( IsEmpty() || padfZ == NULL )
    {
        psEnvelope->MinZ = 0.0;
        psEnvelope->MaxZ = 0.0;
        return;
    }

    double dfMinZ = padfZ[0];
    double dfMaxZ = padfZ[0];

    for( int iPoint = 1; iPoint < nPointCount; iPoint++ )
    {
        if( dfMinZ > padfZ[iPoint] )
            dfMinZ = padfZ[iPoint];
        if( dfMaxZ < padfZ[iPoint] )
            dfMaxZ = padfZ[iPoint];
    }

    psEnvelope->MinZ = dfMinZ;
    psEnvelope->MaxZ = dfMaxZ;
}

/************************************************************************/
/*                               Equals()                                */
/************************************************************************/

OGRBoolean OGRSimpleCurve::Equals( OGRGeometry * poOther ) const

{
    if( poOther == this )
        return TRUE;

    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if( IsEmpty() && poOther->IsEmpty() )
        return TRUE;

    // TODO(schwehr): Test the SRS.

    OGRSimpleCurve *poOLine = (OGRSimpleCurve *) poOther;
    if( getNumPoints() != poOLine->getNumPoints() )
        return FALSE;

    for( int iPoint = 0; iPoint < getNumPoints(); iPoint++ )
    {
        if( getX(iPoint) != poOLine->getX(iPoint)
            || getY(iPoint) != poOLine->getY(iPoint)
            || getZ(iPoint) != poOLine->getZ(iPoint) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRSimpleCurve::transform( OGRCoordinateTransformation *poCT )

{
/* -------------------------------------------------------------------- */
/*   Make a copy of the points to operate on, so as to be able to       */
/*   keep only valid reprojected points if partial reprojection enabled */
/*   or keeping intact the original geometry if only full reprojection  */
/*   allowed.                                                           */
/* -------------------------------------------------------------------- */
    double *xyz = static_cast<double *>(
        VSI_MALLOC_VERBOSE(sizeof(double) * nPointCount * 3));
    int *pabSuccess = static_cast<int *>(
        VSI_CALLOC_VERBOSE(sizeof(int), nPointCount));
    if( xyz == NULL || pabSuccess == NULL )
    {
        VSIFree(xyz);
        VSIFree(pabSuccess);
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    for( int i = 0; i < nPointCount; i++ )
    {
        xyz[i] = paoPoints[i].x;
        xyz[i+nPointCount] = paoPoints[i].y;
        if( padfZ )
            xyz[i+nPointCount*2] = padfZ[i];
        else
            xyz[i+nPointCount*2] = 0.0;
    }

/* -------------------------------------------------------------------- */
/*      Transform and reapply.                                          */
/* -------------------------------------------------------------------- */
    poCT->TransformEx( nPointCount, xyz, xyz + nPointCount,
                       xyz+nPointCount*2, pabSuccess );

    const char* pszEnablePartialReprojection = NULL;

    int j = 0;  // Used after for.
    for( int i = 0; i < nPointCount; i++ )
    {
        if( pabSuccess[i] )
        {
            xyz[j] = xyz[i];
            xyz[j+nPointCount] = xyz[i+nPointCount];
            xyz[j+2*nPointCount] = xyz[i+2*nPointCount];
            j++;
        }
        else
        {
            if( pszEnablePartialReprojection == NULL )
                pszEnablePartialReprojection =
                    CPLGetConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", NULL);
            if( pszEnablePartialReprojection == NULL )
            {
                static bool bHasWarned = false;
                if( !bHasWarned )
                {
                    // Check that there is at least one valid reprojected point
                    // and issue an error giving an hint to use
                    // OGR_ENABLE_PARTIAL_REPROJECTION.
                    bool bHasOneValidPoint = j != 0;
                    for( ; i < nPointCount && !bHasOneValidPoint; i++ )
                    {
                        if( pabSuccess[i] )
                            bHasOneValidPoint = true;
                    }
                    if( bHasOneValidPoint )
                    {
                        bHasWarned = true;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Full reprojection failed, but partial is "
                                 "possible if you define "
                                 "OGR_ENABLE_PARTIAL_REPROJECTION "
                                 "configuration option to TRUE");
                    }
                }

                CPLFree( xyz );
                CPLFree( pabSuccess );
                return OGRERR_FAILURE;
            }
            else if( !CPLTestBool(pszEnablePartialReprojection) )
            {
                CPLFree( xyz );
                CPLFree( pabSuccess );
                return OGRERR_FAILURE;
            }
        }
    }

    if( j == 0 && nPointCount != 0 )
    {
        CPLFree( xyz );
        CPLFree( pabSuccess );
        return OGRERR_FAILURE;
    }

    setPoints( j, xyz, xyz+nPointCount,
            ( padfZ ) ? xyz+nPointCount*2 : NULL);
    CPLFree( xyz );
    CPLFree( pabSuccess );

    assignSpatialReference( poCT->GetTargetCS() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRSimpleCurve::IsEmpty() const
{
    return (nPointCount == 0);
}

/************************************************************************/
/*                     OGRSimpleCurve::segmentize()                      */
/************************************************************************/

void OGRSimpleCurve::segmentize( double dfMaxLength )
{
    if( dfMaxLength <= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfMaxLength must be strictly positive");
        return;
    }
    if( nPointCount < 2 )
        return;

    // So as to make sure that the same line followed in both directions
    // result in the same segmentized line.
    if( paoPoints[0].x < paoPoints[nPointCount - 1].x ||
        (paoPoints[0].x == paoPoints[nPointCount - 1].x &&
         paoPoints[0].y < paoPoints[nPointCount - 1].y) )
    {
        reversePoints();
        segmentize(dfMaxLength);
        reversePoints();
    }

    OGRRawPoint* paoNewPoints = NULL;
    double* padfNewZ = NULL;
    int nNewPointCount = 0;
    const double dfSquareMaxLength = dfMaxLength * dfMaxLength;
    const int nCoordinateDimension = getCoordinateDimension();

    for( int i = 0; i < nPointCount; i++ )
    {
        paoNewPoints = static_cast<OGRRawPoint *>(
            CPLRealloc(paoNewPoints,
                       sizeof(OGRRawPoint) * (nNewPointCount + 1)));
        paoNewPoints[nNewPointCount] = paoPoints[i];

        if( nCoordinateDimension == 3 )
        {
            padfNewZ = static_cast<double *>(
                CPLRealloc(padfNewZ, sizeof(double) * (nNewPointCount + 1)));
            padfNewZ[nNewPointCount] = padfZ[i];
        }

        nNewPointCount++;

        if( i == nPointCount - 1 )
            break;

        const double dfX = paoPoints[i+1].x - paoPoints[i].x;
        const double dfY = paoPoints[i+1].y - paoPoints[i].y;
        const double dfSquareDist = dfX * dfX + dfY * dfY;
        if( dfSquareDist > dfSquareMaxLength )
        {
            const int nIntermediatePoints =
                static_cast<int>(floor(sqrt(dfSquareDist / dfSquareMaxLength)));

            paoNewPoints = static_cast<OGRRawPoint *>(
                CPLRealloc(paoNewPoints,
                           sizeof(OGRRawPoint) * (nNewPointCount +
                                                  nIntermediatePoints)));
            if( nCoordinateDimension == 3 )
            {
                padfNewZ = static_cast<double *>(
                    CPLRealloc(padfNewZ,
                               sizeof(double) * (nNewPointCount +
                                                 nIntermediatePoints)));
            }

            for( int j = 1; j <= nIntermediatePoints; j++ )
            {
                paoNewPoints[nNewPointCount + j - 1].x =
                    paoPoints[i].x + j * dfX / (nIntermediatePoints + 1);
                paoNewPoints[nNewPointCount + j - 1].y =
                    paoPoints[i].y + j * dfY / (nIntermediatePoints + 1);
                if( nCoordinateDimension == 3 )
                {
                    // No interpolation.
                    padfNewZ[nNewPointCount + j - 1] = padfZ[i];
                }
            }

            nNewPointCount += nIntermediatePoints;
        }
    }

    CPLFree(paoPoints);
    paoPoints = paoNewPoints;
    nPointCount = nNewPointCount;

    if( nCoordinateDimension == 3 )
    {
        CPLFree(padfZ);
        padfZ = padfNewZ;
    }
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRSimpleCurve::swapXY()
{
    for( int i = 0; i < nPointCount; i++ )
    {
        std::swap(paoPoints[i].x, paoPoints[i].y);
    }
}

/************************************************************************/
/*                       OGRSimpleCurvePointIterator                    */
/************************************************************************/

class OGRSimpleCurvePointIterator: public OGRPointIterator
{
        const OGRSimpleCurve* poSC;
        int                   iCurPoint;

    public:
        explicit OGRSimpleCurvePointIterator(const OGRSimpleCurve* poSCIn) :
            poSC(poSCIn),
            iCurPoint(0) {}

        virtual OGRBoolean getNextPoint( OGRPoint* p ) override;
};

/************************************************************************/
/*                            getNextPoint()                            */
/************************************************************************/

OGRBoolean OGRSimpleCurvePointIterator::getNextPoint(OGRPoint* p)
{
    if( iCurPoint >= poSC->getNumPoints() )
        return FALSE;
    poSC->getPoint(iCurPoint, p);
    iCurPoint++;
    return TRUE;
}

/************************************************************************/
/*                         getPointIterator()                           */
/************************************************************************/

OGRPointIterator* OGRSimpleCurve::getPointIterator() const
{
    return new OGRSimpleCurvePointIterator(this);
}

/************************************************************************/
/*                           OGRLineString()                            */
/************************************************************************/

/**
 * \brief Create an empty line string.
 */

OGRLineString::OGRLineString() {}

/************************************************************************/
/*                  OGRLineString( const OGRLineString& )               */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRLineString::OGRLineString( const OGRLineString& other ) :
    OGRSimpleCurve( other )
{}

/************************************************************************/
/*                          ~OGRLineString()                            */
/************************************************************************/

OGRLineString::~OGRLineString() {}

/************************************************************************/
/*                    operator=( const OGRLineString& )                 */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRLineString& OGRLineString::operator=( const OGRLineString& other )
{
    if( this != &other)
    {
        OGRSimpleCurve::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRLineString::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbLineStringZM;
    else if( flags & OGR_G_MEASURED )
        return wkbLineStringM;
    else if( flags & OGR_G_3D )
        return wkbLineString25D;
    else
        return wkbLineString;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRLineString::getGeometryName() const

{
    return "LINESTRING";
}

/************************************************************************/
/*                          curveToLine()                               */
/************************************************************************/

OGRLineString* OGRLineString::CurveToLine(
    CPL_UNUSED double /* dfMaxAngleStepSizeDegrees */,
    CPL_UNUSED const char* const* /* papszOptions */ ) const
{
    // Downcast.
    OGRLineString * poLineString = dynamic_cast<OGRLineString *>(clone());
    if( poLineString == NULL )
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "dynamic_cast failed.  Expected OGRLineString.");
    }
    return poLineString;
}

/************************************************************************/
/*                          get_LinearArea()                          */
/************************************************************************/

/**
 * \brief Compute area of ring / closed linestring.
 *
 * The area is computed according to Green's Theorem:
 *
 * Area is "Sum(x(i)*(y(i+1) - y(i-1)))/2" for i = 0 to pointCount-1,
 * assuming the last point is a duplicate of the first.
 *
 * @return computed area.
 */

double OGRSimpleCurve::get_LinearArea() const

{
    if( nPointCount < 2 ||
        (WkbSize() != 0 && /* if not a linearring, check it is closed */
            (paoPoints[0].x != paoPoints[nPointCount-1].x ||
             paoPoints[0].y != paoPoints[nPointCount-1].y)) )
    {
        return 0;
    }

    double dfAreaSum =
        paoPoints[0].x * (paoPoints[1].y - paoPoints[nPointCount-1].y);

    for( int i = 1; i < nPointCount-1; i++ )
    {
        dfAreaSum += paoPoints[i].x * (paoPoints[i+1].y - paoPoints[i-1].y);
    }

    dfAreaSum +=
        paoPoints[nPointCount-1].x *
        (paoPoints[0].y - paoPoints[nPointCount-2].y);

    return 0.5 * fabs(dfAreaSum);
}

/************************************************************************/
/*                             getCurveGeometry()                       */
/************************************************************************/

OGRGeometry *
OGRLineString::getCurveGeometry( const char* const* papszOptions ) const
{
    return OGRGeometryFactory::curveFromLineString(this, papszOptions);
}

/************************************************************************/
/*                      TransferMembersAndDestroy()                     */
/************************************************************************/
//! @cond Doxygen_Suppress
OGRLineString* OGRLineString::TransferMembersAndDestroy(
    OGRLineString* poSrc,
    OGRLineString* poDst )
{
    poDst->set3D(poSrc->Is3D());
    poDst->setMeasured(poSrc->IsMeasured());
    poDst->assignSpatialReference(poSrc->getSpatialReference());
    poDst->nPointCount = poSrc->nPointCount;
    poDst->paoPoints = poSrc->paoPoints;
    poDst->padfZ = poSrc->padfZ;
    poSrc->nPointCount = 0;
    poSrc->paoPoints = NULL;
    poSrc->padfZ = NULL;
    delete poSrc;
    return poDst;
}
//! @endcond
/************************************************************************/
/*                         CastToLinearRing()                           */
/************************************************************************/

/**
 * \brief Cast to linear ring.
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure)
 *
 * @param poLS the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRLinearRing* OGRLineString::CastToLinearRing( OGRLineString* poLS )
{
    if( poLS->nPointCount < 2 || !poLS->get_IsClosed() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot convert non-closed linestring to linearring");
        delete poLS;
        return NULL;
    }
    // Downcast.
    OGRLinearRing * poRing = dynamic_cast<OGRLinearRing *>(
        TransferMembersAndDestroy(poLS, new OGRLinearRing()));
    if( poRing == NULL )
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "dynamic_cast failed.  Expected OGRLinearRing.");
    }
    return poRing;
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GetCasterToLineString()                          */
/************************************************************************/

static OGRLineString* CasterToLineString(OGRCurve* poCurve)
{
    OGRLineString* poLS = dynamic_cast<OGRLineString*>(poCurve);
    CPLAssert(poLS);
    return poLS;
}

OGRCurveCasterToLineString OGRLineString::GetCasterToLineString() const
{
    return ::CasterToLineString;
}

/************************************************************************/
/*                        GetCasterToLinearRing()                       */
/************************************************************************/

OGRLinearRing* OGRLineString::CasterToLinearRing(OGRCurve* poCurve)
{
    OGRLineString* poLS = dynamic_cast<OGRLineString*>(poCurve);
    CPLAssert(poLS);
    return OGRLineString::CastToLinearRing(poLS);
}

OGRCurveCasterToLinearRing OGRLineString::GetCasterToLinearRing() const
{
    return OGRLineString::CasterToLinearRing;
}

/************************************************************************/
/*                            get_Area()                                */
/************************************************************************/

double OGRLineString::get_Area() const
{
    return get_LinearArea();
}

/************************************************************************/
/*                       get_AreaOfCurveSegments()                      */
/************************************************************************/

double OGRLineString::get_AreaOfCurveSegments() const
{
    return 0;
}
//! @endcond
