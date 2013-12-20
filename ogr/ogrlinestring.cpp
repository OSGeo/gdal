/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRLineString geometry class.
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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <assert.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRLineString()                            */
/************************************************************************/

/**
 * \brief Create an empty line string.
 */

OGRLineString::OGRLineString()

{
    nPointCount = 0;
    paoPoints = NULL;
    padfZ = NULL;
}

/************************************************************************/
/*                           ~OGRLineString()                           */
/************************************************************************/

OGRLineString::~OGRLineString()

{
    if( paoPoints != NULL )
        OGRFree( paoPoints );
    if( padfZ != NULL )
        OGRFree( padfZ );
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRLineString::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbLineString25D;
    else
        return wkbLineString;
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRLineString::flattenTo2D()

{
    Make2D();
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRLineString::getGeometryName() const

{
    return "LINESTRING";
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRLineString::clone() const

{
    OGRLineString       *poNewLineString;

    poNewLineString = new OGRLineString();

    poNewLineString->assignSpatialReference( getSpatialReference() );
    poNewLineString->setPoints( nPointCount, paoPoints, padfZ );
    poNewLineString->setCoordinateDimension( getCoordinateDimension() );
    
    return poNewLineString;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRLineString::empty()

{
    setNumPoints( 0 );
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRLineString::getDimension() const

{
    return 1;
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRLineString::setCoordinateDimension( int nNewDimension )

{
    nCoordDimension = nNewDimension;
    if( nNewDimension == 2 )
        Make2D();
    else if( nNewDimension == 3 )
        Make3D();
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRLineString::WkbSize() const

{
    return 5 + 4 + 8 * nPointCount * getCoordinateDimension();
}

/************************************************************************/
/*                               Make2D()                               */
/************************************************************************/

void OGRLineString::Make2D()

{
    if( padfZ != NULL )
    {
        OGRFree( padfZ );
        padfZ = NULL;
    }
    nCoordDimension = 2;
}

/************************************************************************/
/*                               Make3D()                               */
/************************************************************************/

void OGRLineString::Make3D()

{
    if( padfZ == NULL )
    {
        if( nPointCount == 0 )
            padfZ = (double *) OGRCalloc(sizeof(double),1);
        else
            padfZ = (double *) OGRCalloc(sizeof(double),nPointCount);
    }
    nCoordDimension = 3;
}

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

void    OGRLineString::getPoint( int i, OGRPoint * poPoint ) const

{
    assert( i >= 0 );
    assert( i < nPointCount );
    assert( poPoint != NULL );

    poPoint->setX( paoPoints[i].x );
    poPoint->setY( paoPoints[i].y );

    if( getCoordinateDimension() == 3 && padfZ != NULL )
        poPoint->setZ( padfZ[i] );
}

/**
 * \fn int OGRLineString::getNumPoints() const;
 *
 * \brief Fetch vertex count.
 *
 * Returns the number of vertices in the line string.  
 *
 * @return vertex count.
 */

/**
 * \fn double OGRLineString::getX( int iVertex ) const;
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
 * \fn double OGRLineString::getY( int iVertex ) const;
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

double OGRLineString::getZ( int iVertex ) const

{
    if( padfZ != NULL && iVertex >= 0 && iVertex < nPointCount 
        && nCoordDimension >= 3 )
        return( padfZ[iVertex] );
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
 */

void OGRLineString::setNumPoints( int nNewPointCount, int bZeroizeNewContent )

{
    if( nNewPointCount == 0 )
    {
        OGRFree( paoPoints );
        paoPoints = NULL;
        
        OGRFree( padfZ );
        padfZ = NULL;
        
        nPointCount = 0;
        return;
    }

    if( nNewPointCount > nPointCount )
    {
        OGRRawPoint* paoNewPoints = (OGRRawPoint *)
            VSIRealloc(paoPoints, sizeof(OGRRawPoint) * nNewPointCount);
        if (paoNewPoints == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Could not allocate array for %d points", nNewPointCount);
            return;
        }
        paoPoints = paoNewPoints;
        
        if( bZeroizeNewContent )
            memset( paoPoints + nPointCount,
                0, sizeof(OGRRawPoint) * (nNewPointCount - nPointCount) );
        
        if( getCoordinateDimension() == 3 )
        {
            double* padfNewZ = (double *)
                VSIRealloc( padfZ, sizeof(double)*nNewPointCount );
            if (padfNewZ == NULL)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Could not allocate array for %d points", nNewPointCount);
                return;
            }
            padfZ = padfNewZ;
            if( bZeroizeNewContent )
                memset( padfZ + nPointCount, 0,
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
 * accomodate the request.
 *
 * There is no SFCOM analog to this method.
 * 
 * @param iPoint the index of the vertex to assign (zero based).
 * @param poPoint the value to assign to the vertex.
 */

void OGRLineString::setPoint( int iPoint, OGRPoint * poPoint )

{
    if (poPoint->getCoordinateDimension() < 3)
        setPoint( iPoint, poPoint->getX(), poPoint->getY() );
    else
        setPoint( iPoint, poPoint->getX(), poPoint->getY(), poPoint->getZ() );
}

/************************************************************************/
/*                              setPoint()                              */
/************************************************************************/

/**
 * \brief Set the location of a vertex in line string.
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
 * @param zIn input Z coordinate to assign (defaults to zero).
 */

void OGRLineString::setPoint( int iPoint, double xIn, double yIn, double zIn )

{
    if( getCoordinateDimension() == 2 )
        Make3D();

    if( iPoint >= nPointCount )
    {
        setNumPoints( iPoint+1 );
        if (nPointCount < iPoint + 1)
            return;
    }

    paoPoints[iPoint].x = xIn;
    paoPoints[iPoint].y = yIn;

    if( zIn != 0.0 )
    {
        Make3D();
        padfZ[iPoint] = zIn;
    }
    else if( getCoordinateDimension() == 3 )
    {
        padfZ[iPoint] = 0.0;
    }
}

void OGRLineString::setPoint( int iPoint, double xIn, double yIn )

{
    if( iPoint >= nPointCount )
    {
        setNumPoints( iPoint+1 );
        if (nPointCount < iPoint + 1)
            return;
    }

    paoPoints[iPoint].x = xIn;
    paoPoints[iPoint].y = yIn;
}

/************************************************************************/
/*                                setZ()                                */
/************************************************************************/

void OGRLineString::setZ( int iPoint, double zIn )
{
    if( getCoordinateDimension() == 2 )
        Make3D();

    if( iPoint >= nPointCount )
    {
        setNumPoints( iPoint+1 );
        if (nPointCount < iPoint + 1)
            return;
    }

    if( padfZ )
        padfZ[iPoint] = zIn;
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

void OGRLineString::addPoint( OGRPoint * poPoint )

{
    if ( poPoint->getCoordinateDimension() < 3 )
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
 */

void OGRLineString::addPoint( double x, double y, double z )

{
    setPoint( nPointCount, x, y, z );
}

void OGRLineString::addPoint( double x, double y )

{
    setPoint( nPointCount, x, y );
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
 * @param padfZ the Z values that go with the points (optional, may be NULL).
 */

void OGRLineString::setPoints( int nPointsIn, OGRRawPoint * paoPointsIn,
                               double * padfZ )

{
    setNumPoints( nPointsIn, FALSE );
    if (nPointCount < nPointsIn)
        return;

    memcpy( paoPoints, paoPointsIn, sizeof(OGRRawPoint) * nPointsIn);

/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZ == NULL && getCoordinateDimension() > 2 )
    {
        Make2D();
    }
    else if( padfZ )
    {
        Make3D();
        memcpy( this->padfZ, padfZ, sizeof(double) * nPointsIn );
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
 * @param padfZ list of Z coordinates of points being assigned (defaults to
 * NULL for 2D objects).
 */

void OGRLineString::setPoints( int nPointsIn, double * padfX, double * padfY,
                               double * padfZ )

{
    int         i;

/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZ == NULL )
        Make2D();
    else
        Make3D();
    
/* -------------------------------------------------------------------- */
/*      Assign values.                                                  */
/* -------------------------------------------------------------------- */
    setNumPoints( nPointsIn, FALSE );
    if (nPointCount < nPointsIn)
        return;

    for( i = 0; i < nPointsIn; i++ )
    {
        paoPoints[i].x = padfX[i];
        paoPoints[i].y = padfY[i];
    }

    if( this->padfZ != NULL )
        memcpy( this->padfZ, padfZ, sizeof(double) * nPointsIn );
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
 * @param padfZ the Z values that go with the points (optional, may be NULL).
 */

void OGRLineString::getPoints( OGRRawPoint * paoPointsOut, double * padfZ ) const
{
    if ( ! paoPointsOut )
        return;
        
    memcpy( paoPointsOut, paoPoints, sizeof(OGRRawPoint) * nPointCount );

/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZ )
    {
        if ( this->padfZ )
            memcpy( padfZ, this->padfZ, sizeof(double) * nPointCount );
        else
            memset( padfZ, 0, sizeof(double) * nPointCount );
    }
}


/************************************************************************/
/*                          getPoints()                                 */
/************************************************************************/

/**
 * \brief Returns all points of line string.
 *
 * This method copies all points into user arrays. The user provides the
 * stride between 2 consecutives elements of the array.
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
 *
 * @since OGR 1.9.0
 */

void OGRLineString::getPoints( void* pabyX, int nXStride,
                               void* pabyY, int nYStride,
                               void* pabyZ, int nZStride)  const
{
    int i;
    if (pabyX != NULL && nXStride == 0)
        return;
    if (pabyY != NULL && nYStride == 0)
        return;
    if (pabyZ != NULL && nZStride == 0)
        return;
    if (nXStride == 2 * sizeof(double) &&
        nYStride == 2 * sizeof(double) &&
        (char*)pabyY == (char*)pabyX + sizeof(double) &&
        (pabyZ == NULL || nZStride == sizeof(double)))
    {
        getPoints((OGRRawPoint *)pabyX, (double*)pabyZ);
        return;
    }
    for(i=0;i<nPointCount;i++)
    {
        if (pabyX) *(double*)((char*)pabyX + i * nXStride) = paoPoints[i].x;
        if (pabyY) *(double*)((char*)pabyY + i * nYStride) = paoPoints[i].y;
    }

    if (pabyZ)
    {
        for(i=0;i<nPointCount;i++)
        {
            *(double*)((char*)pabyZ + i * nZStride) = (padfZ) ? padfZ[i] : 0.0;
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

void OGRLineString::reversePoints()

{
    int i;

    for( i = 0; i < nPointCount/2; i++ )
    {
        OGRRawPoint sPointTemp = paoPoints[i];

        paoPoints[i] = paoPoints[nPointCount-i-1];
        paoPoints[nPointCount-i-1] = sPointTemp;

        if( padfZ )
        {
            double dfZTemp = padfZ[i];

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

void OGRLineString::addSubLineString( const OGRLineString *poOtherLine, 
                                      int nStartVertex, int nEndVertex )

{
    int nOtherLineNumPoints = poOtherLine->getNumPoints();
    if (nOtherLineNumPoints == 0)
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
        CPLAssert( FALSE );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Grow this linestring to hold the additional points.             */
/* -------------------------------------------------------------------- */
    int nOldPoints = nPointCount;
    int nPointsToAdd = ABS(nEndVertex-nStartVertex) + 1;

    setNumPoints( nPointsToAdd + nOldPoints );
    if (nPointCount < nPointsToAdd + nOldPoints)
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
            memcpy( padfZ + nOldPoints, poOtherLine->padfZ + nStartVertex,
                    sizeof(double) * nPointsToAdd );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Copy the x/y points - reverse copies done double by double.     */
/* -------------------------------------------------------------------- */
    else
    {
        int i;

        for( i = 0; i < nPointsToAdd; i++ )
        {
            paoPoints[i+nOldPoints].x = 
                poOtherLine->paoPoints[nStartVertex-i].x;
            paoPoints[i+nOldPoints].y = 
                poOtherLine->paoPoints[nStartVertex-i].y;
        }

        if( poOtherLine->padfZ != NULL )
        {
            Make3D();

            for( i = 0; i < nPointsToAdd; i++ )
            {
                padfZ[i+nOldPoints] = poOtherLine->padfZ[nStartVertex-i];
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

OGRErr OGRLineString::importFromWkb( unsigned char * pabyData,
                                     int nSize )

{
    OGRwkbByteOrder     eByteOrder;
    
    if( nSize < 9 && nSize != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.                                        */
/* -------------------------------------------------------------------- */
    eByteOrder = DB2_V72_FIX_BYTE_ORDER((OGRwkbByteOrder) *pabyData);
    if (!( eByteOrder == wkbXDR || eByteOrder == wkbNDR ))
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Get the geometry feature type.  For now we assume that          */
/*      geometry type is between 0 and 255 so we only have to fetch     */
/*      one byte.                                                       */
/* -------------------------------------------------------------------- */
    OGRwkbGeometryType eGeometryType;
    int bIs3D = FALSE;

    if( eByteOrder == wkbNDR )
    {
        eGeometryType = (OGRwkbGeometryType) pabyData[1];
        bIs3D = pabyData[4] & 0x80 || pabyData[2] & 0x80;
    }
    else
    {
        eGeometryType = (OGRwkbGeometryType) pabyData[4];
        bIs3D = pabyData[1] & 0x80 || pabyData[3] & 0x80;
    }

    if( eGeometryType != wkbLineString )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Get the vertex count.                                           */
/* -------------------------------------------------------------------- */
    int         nNewNumPoints;
    
    memcpy( &nNewNumPoints, pabyData + 5, 4 );
    
    if( OGR_SWAP( eByteOrder ) )
        nNewNumPoints = CPL_SWAP32(nNewNumPoints);
    
    /* Check if the wkb stream buffer is big enough to store
     * fetched number of points.
     * 16 or 24 - size of point structure
     */
    int nPointSize = (bIs3D ? 24 : 16);
    if (nNewNumPoints < 0 || nNewNumPoints > INT_MAX / nPointSize)
        return OGRERR_CORRUPT_DATA;
    int nBufferMinSize = nPointSize * nNewNumPoints;

    if( nSize != -1 && nBufferMinSize > nSize-9 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Length of input WKB is too small" );
        return OGRERR_NOT_ENOUGH_DATA;
    }

    setNumPoints( nNewNumPoints );
    if (nPointCount < nNewNumPoints)
        return OGRERR_FAILURE;
    
    if( bIs3D )
        Make3D();
    else
        Make2D();
    
/* -------------------------------------------------------------------- */
/*      Get the vertex.                                                 */
/* -------------------------------------------------------------------- */
    int i = 0;
    
    if( bIs3D )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            memcpy( paoPoints + i, pabyData + 9 + i*24, 16 );
            memcpy( padfZ + i, pabyData + 9 + 16 + i*24, 8 );
        }
    }
    else
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

        if( bIs3D )
        {
            for( i = 0; i < nPointCount; i++ )
            {
                CPL_SWAPDOUBLE( padfZ + i );
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

OGRErr  OGRLineString::exportToWkb( OGRwkbByteOrder eByteOrder,
                               unsigned char * pabyData ) const

{
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = getGeometryType();
    
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
    int         i;
    
    if( getCoordinateDimension() == 3 )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            memcpy( pabyData + 9 + 24*i, paoPoints+i, 16 );
            memcpy( pabyData + 9 + 16 + 24*i, padfZ+i, 8 );
        }
    }
    else
        memcpy( pabyData+9, paoPoints, 16 * nPointCount );

/* -------------------------------------------------------------------- */
/*      Swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        int     nCount;

        nCount = CPL_SWAP32( nPointCount );
        memcpy( pabyData+5, &nCount, 4 );

        for( i = getCoordinateDimension() * nPointCount - 1; i >= 0; i-- )
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

OGRErr OGRLineString::importFromWkt( char ** ppszInput )

{
    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;

    empty();

/* -------------------------------------------------------------------- */
/*      Read and verify the ``LINESTRING'' keyword token.               */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( !EQUAL(szToken,getGeometryName()) )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Check for EMPTY                                                 */
/* -------------------------------------------------------------------- */
    const char *pszPreScan;
    int bHasZ = FALSE, bHasM = FALSE;

    pszPreScan = OGRWktReadToken( pszInput, szToken );
    if( EQUAL(szToken,"EMPTY") )
    {
        *ppszInput = (char *) pszPreScan;
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Check for Z, M or ZM. Will ignore the Measure                   */
/* -------------------------------------------------------------------- */
    else if( EQUAL(szToken,"Z") )
    {
        bHasZ = TRUE;
    }
    else if( EQUAL(szToken,"M") )
    {
        bHasM = TRUE;
    }
    else if( EQUAL(szToken,"ZM") )
    {
        bHasZ = TRUE;
        bHasM = TRUE;
    }

    if (bHasZ || bHasM)
    {
        pszInput = pszPreScan;
        pszPreScan = OGRWktReadToken( pszInput, szToken );
        if( EQUAL(szToken,"EMPTY") )
        {
            *ppszInput = (char *) pszPreScan;
            empty();
            /* FIXME?: In theory we should store the dimension and M presence */
            /* if we want to allow round-trip with ExportToWKT v1.2 */
            return OGRERR_NONE;
        }
    }

    if( !EQUAL(szToken,"(") )
        return OGRERR_CORRUPT_DATA;

    if ( !bHasZ && !bHasM )
    {
        /* Test for old-style LINESTRING(EMPTY) */
        pszPreScan = OGRWktReadToken( pszPreScan, szToken );
        if( EQUAL(szToken,"EMPTY") )
        {
            pszInput = OGRWktReadToken( pszPreScan, szToken );

            if( !EQUAL(szToken,")") )
                return OGRERR_CORRUPT_DATA;
            else
            {
                *ppszInput = (char *) pszInput;
                empty();
                return OGRERR_NONE;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Read the point list.                                            */
/* -------------------------------------------------------------------- */
    int      nMaxPoint = 0;

    nPointCount = 0;

    pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoint,
                                 &nPointCount );
    if( pszInput == NULL )
        return OGRERR_CORRUPT_DATA;

    *ppszInput = (char *) pszInput;

    if( padfZ == NULL )
        nCoordDimension = 2;
    else
    {
        /* Ignore Z array when we have a LINESTRING M */
        if (bHasM && !bHasZ)
        {
            nCoordDimension = 2;
            CPLFree(padfZ);
            padfZ = NULL;
        }
        else
            nCoordDimension = 3;
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivelent.  This could be made alot more CPU efficient!        */
/************************************************************************/

OGRErr OGRLineString::exportToWkt( char ** ppszDstText ) const

{
    int         nMaxString = nPointCount * 40 * 3 + 20;
    int         nRetLen = 0;

/* -------------------------------------------------------------------- */
/*      Handle special empty case.                                      */
/* -------------------------------------------------------------------- */
    if( nPointCount == 0 )
    {
        CPLString osEmpty;
        osEmpty.Printf("%s EMPTY",getGeometryName());
        *ppszDstText = CPLStrdup(osEmpty);
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      General case.                                                   */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSIMalloc( nMaxString );
    if( *ppszDstText == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

    sprintf( *ppszDstText, "%s (", getGeometryName() );

    for( int i = 0; i < nPointCount; i++ )
    {
        if( nMaxString <= (int) strlen(*ppszDstText+nRetLen) + 32 + nRetLen )
        {
            CPLDebug( "OGR", 
                      "OGRLineString::exportToWkt() ... buffer overflow.\n"
                      "nMaxString=%d, strlen(*ppszDstText) = %d, i=%d\n"
                      "*ppszDstText = %s", 
                      nMaxString, (int) strlen(*ppszDstText), i, *ppszDstText );

            VSIFree( *ppszDstText );
            *ppszDstText = NULL;
            return OGRERR_NOT_ENOUGH_MEMORY;
        }
        
        if( i > 0 )
            strcat( *ppszDstText + nRetLen, "," );

        nRetLen += strlen(*ppszDstText + nRetLen);
        if( getCoordinateDimension() == 3 )
            OGRMakeWktCoordinate( *ppszDstText + nRetLen,
                                  paoPoints[i].x,
                                  paoPoints[i].y,
                                  padfZ[i],
                                  nCoordDimension );
        else
            OGRMakeWktCoordinate( *ppszDstText + nRetLen,
                                  paoPoints[i].x,
                                  paoPoints[i].y,
                                  0.0,
                                  nCoordDimension );

        nRetLen += strlen(*ppszDstText + nRetLen);
    }

    strcat( *ppszDstText+nRetLen, ")" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             get_Length()                             */
/*                                                                      */
/*      For now we return a simple euclidian 2D distance.               */
/************************************************************************/

double OGRLineString::get_Length() const

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

void OGRLineString::StartPoint( OGRPoint * poPoint ) const

{
    getPoint( 0, poPoint );
}

/************************************************************************/
/*                              EndPoint()                              */
/************************************************************************/

void OGRLineString::EndPoint( OGRPoint * poPoint ) const

{
    getPoint( nPointCount-1, poPoint );
}

/************************************************************************/
/*                               Value()                                */
/*                                                                      */
/*      Get an interpolated point at some distance along the curve.     */
/************************************************************************/

void OGRLineString::Value( double dfDistance, OGRPoint * poPoint ) const

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

        if (dfSegLength > 0)
        {
            if( (dfLength <= dfDistance) && ((dfLength + dfSegLength) >= 
                                             dfDistance) )
            {
                double      dfRatio;

                dfRatio = (dfDistance - dfLength) / dfSegLength;

                poPoint->setX( paoPoints[i].x * (1 - dfRatio)
                               + paoPoints[i+1].x * dfRatio );
                poPoint->setY( paoPoints[i].y * (1 - dfRatio)
                               + paoPoints[i+1].y * dfRatio );

                if( getCoordinateDimension() == 3 )
                    poPoint->setZ( padfZ[i] * (1 - dfRatio)
                                   + padfZ[i] * dfRatio );
                
                return;
            }

            dfLength += dfSegLength;
        }
    }
    
    EndPoint( poPoint );
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRLineString::getEnvelope( OGREnvelope * psEnvelope ) const

{
    double      dfMinX, dfMinY, dfMaxX, dfMaxY;

    if( nPointCount == 0 )
    {
        psEnvelope->MinX = 0;
        psEnvelope->MaxX = 0;
        psEnvelope->MinY = 0;
        psEnvelope->MaxY = 0;
        return;
    }
    
    dfMinX = dfMaxX = paoPoints[0].x;
    dfMinY = dfMaxY = paoPoints[0].y;

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

void OGRLineString::getEnvelope( OGREnvelope3D * psEnvelope ) const

{
    getEnvelope((OGREnvelope*)psEnvelope);

    double      dfMinZ, dfMaxZ;

    if( nPointCount == 0 || padfZ == NULL )
    {
        psEnvelope->MinZ = 0;
        psEnvelope->MaxZ = 0;
        return;
    }

    dfMinZ = dfMaxZ = padfZ[0];

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

OGRBoolean OGRLineString::Equals( OGRGeometry * poOther ) const

{
    OGRLineString       *poOLine = (OGRLineString *) poOther;
    
    if( poOLine == this )
        return TRUE;
    
    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    // we should eventually test the SRS.

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

OGRErr OGRLineString::transform( OGRCoordinateTransformation *poCT )

{
#ifdef DISABLE_OGRGEOM_TRANSFORM
    return OGRERR_FAILURE;
#else
    double      *xyz;
    int         *pabSuccess;
    int         i, j;

/* -------------------------------------------------------------------- */
/*   Make a copy of the points to operate on, so as to be able to       */
/*   keep only valid reprojected points if partial reprojection enabled */
/*   or keeping intact the original geometry if only full reprojection  */
/*   allowed.                                                           */
/* -------------------------------------------------------------------- */
    xyz = (double *) VSIMalloc(sizeof(double) * nPointCount * 3);
    pabSuccess = (int *) VSICalloc(sizeof(int), nPointCount);
    if( xyz == NULL || pabSuccess == NULL )
    {
        VSIFree(xyz);
        VSIFree(pabSuccess);
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    for( i = 0; i < nPointCount; i++ )
    {
        xyz[i  ] = paoPoints[i].x;
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

    for( i = 0, j = 0; i < nPointCount; i++ )
    {
        if (pabSuccess[i])
        {
            xyz[j] = xyz[i];
            xyz[j+nPointCount] = xyz[i+nPointCount];
            xyz[j+2*nPointCount] = xyz[i+2*nPointCount];
            j ++;
        }
        else
        {
            if (pszEnablePartialReprojection == NULL)
                pszEnablePartialReprojection =
                    CPLGetConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", NULL);
            if (pszEnablePartialReprojection == NULL)
            {
                static int bHasWarned = FALSE;
                if (!bHasWarned)
                {
                    /* Check that there is at least one valid reprojected point */
                    /* and issue an error giving an hint to use OGR_ENABLE_PARTIAL_REPROJECTION */
                    int bHasOneValidPoint = (j != 0);
                    for( ; i < nPointCount && !bHasOneValidPoint; i++ )
                    {
                        if (pabSuccess[i])
                            bHasOneValidPoint = TRUE;
                    }
                    if (bHasOneValidPoint)
                    {
                        bHasWarned = TRUE;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "Full reprojection failed, but partial is possible if you define "
                                "OGR_ENABLE_PARTIAL_REPROJECTION configuration option to TRUE");
                    }
                }

                CPLFree( xyz );
                CPLFree( pabSuccess );
                return OGRERR_FAILURE;
            }
            else if (!CSLTestBoolean(pszEnablePartialReprojection))
            {
                CPLFree( xyz );
                CPLFree( pabSuccess );
                return OGRERR_FAILURE;
            }
        }
    }

    if (j == 0 && nPointCount != 0)
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
#endif
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRLineString::IsEmpty(  ) const
{
    return (nPointCount == 0);
}

/************************************************************************/
/*                     OGRLineString::segmentize()                      */
/************************************************************************/

void OGRLineString::segmentize( double dfMaxLength )
{
    if (dfMaxLength <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfMaxLength must be strictly positive");
        return;
    }

    int i;
    OGRRawPoint* paoNewPoints = NULL;
    double* padfNewZ = NULL;
    int nNewPointCount = 0;
    double dfSquareMaxLength = dfMaxLength * dfMaxLength;
    const int nCoordinateDimension = getCoordinateDimension();

    for( i = 0; i < nPointCount; i++ )
    {
        paoNewPoints = (OGRRawPoint *)
            OGRRealloc(paoNewPoints, sizeof(OGRRawPoint) * (nNewPointCount + 1));
        paoNewPoints[nNewPointCount] = paoPoints[i];

        if( nCoordinateDimension == 3 )
        {
            padfNewZ = (double *)
                OGRRealloc(padfNewZ, sizeof(double) * (nNewPointCount + 1));
            padfNewZ[nNewPointCount] = padfZ[i];
        }

        nNewPointCount++;

        if (i == nPointCount - 1)
            break;

        double dfX = paoPoints[i+1].x - paoPoints[i].x;
        double dfY = paoPoints[i+1].y - paoPoints[i].y;
        double dfSquareDist = dfX * dfX + dfY * dfY;
        if (dfSquareDist > dfSquareMaxLength)
        {
            int nIntermediatePoints = (int)floor(sqrt(dfSquareDist / dfSquareMaxLength));
            int j;

            paoNewPoints = (OGRRawPoint *)
                OGRRealloc(paoNewPoints, sizeof(OGRRawPoint) * (nNewPointCount + nIntermediatePoints));
            if( nCoordinateDimension == 3 )
            {
                padfNewZ = (double *)
                    OGRRealloc(padfNewZ, sizeof(double) * (nNewPointCount + nIntermediatePoints));
            }

            for(j=1;j<=nIntermediatePoints;j++)
            {
                paoNewPoints[nNewPointCount + j - 1].x = paoPoints[i].x + j * dfX / (nIntermediatePoints + 1);
                paoNewPoints[nNewPointCount + j - 1].y = paoPoints[i].y + j * dfY / (nIntermediatePoints + 1);
                if( nCoordinateDimension == 3 )
                {
                    /* No interpolation */
                    padfNewZ[nNewPointCount + j - 1] = padfZ[i];
                }
            }

            nNewPointCount += nIntermediatePoints;
        }
    }

    OGRFree(paoPoints);
    paoPoints = paoNewPoints;
    nPointCount = nNewPointCount;

    if( nCoordinateDimension == 3 )
    {
        OGRFree(padfZ);
        padfZ = padfNewZ;
    }
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRLineString::swapXY()
{
    int i;
    for( i = 0; i < nPointCount; i++ )
    {
        double dfTemp = paoPoints[i].x;
        paoPoints[i].x = paoPoints[i].y;
        paoPoints[i].y = dfTemp;
    }
}
