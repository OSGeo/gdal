/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The Point geometry class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

/* for std::numeric_limits */
#include <limits>

CPL_CVSID("$Id$");

/* CAUTION: we use nCoordDimension == -2 to mean POINT EMPTY (2D) and
                   nCoordDimension == -3 to mean POINT Z EMPTY
*/

/************************************************************************/
/*                              OGRPoint()                              */
/************************************************************************/

/**
 * \brief Create a (0,0) point.
 */

OGRPoint::OGRPoint()

{
    empty();
}

/************************************************************************/
/*                              OGRPoint()                              */
/*                                                                      */
/*      Initialize point to value.                                      */
/************************************************************************/

OGRPoint::OGRPoint( double xIn, double yIn, double zIn )

{
    x = xIn;
    y = yIn;
    z = zIn;
    nCoordDimension = 3;
}

/************************************************************************/
/*                              OGRPoint()                              */
/*                                                                      */
/*      Initialize point to value.                                      */
/************************************************************************/

OGRPoint::OGRPoint( double xIn, double yIn )

{
    x = xIn;
    y = yIn;
    z = 0.0;
    nCoordDimension = 2;
}

/************************************************************************/
/*                       OGRPoint( const OGRPoint& )                    */
/************************************************************************/

/**
 * \brief Copy constructor.
 * 
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 * 
 * @since GDAL 2.1
 */

OGRPoint::OGRPoint( const OGRPoint& other ) :
    OGRGeometry( other ),
    x( other.x ),
    y( other.y ),
    z( other.z )
{
}

/************************************************************************/
/*                             ~OGRPoint()                              */
/************************************************************************/

OGRPoint::~OGRPoint()

{
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

OGRPoint& OGRPoint::operator=( const OGRPoint& other )
{
    if( this != &other)
    {
        OGRGeometry::operator=( other );
        
        x = other.x;
        y = other.y;
        z = other.z;
    }
    return *this;
}

/************************************************************************/
/*                               clone()                                */
/*                                                                      */
/*      Make a new object that is a copy of this object.                */
/************************************************************************/

OGRGeometry *OGRPoint::clone() const

{
    OGRPoint    *poNewPoint = new OGRPoint( x, y, z );

    poNewPoint->assignSpatialReference( getSpatialReference() );
    poNewPoint->setCoordinateDimension( nCoordDimension );

    return poNewPoint;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/
void OGRPoint::empty()

{
    x = y = z = 0.0;
    nCoordDimension = -2;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRPoint::getDimension() const

{
    return 0;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRPoint::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbPoint25D;
    else
        return wkbPoint;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRPoint::getGeometryName() const

{
    return "POINT";
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRPoint::flattenTo2D()

{
    z = 0;
    if (nCoordDimension > 2)
        nCoordDimension = 2;
}

/************************************************************************/
/*                       getCoordinateDimension()                       */
/************************************************************************/

int OGRPoint::getCoordinateDimension() const

{
    return nCoordDimension < 0 ? -nCoordDimension : nCoordDimension;
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRPoint::setCoordinateDimension( int nNewDimension )

{
    nCoordDimension = (nCoordDimension < 0 ) ? -nNewDimension : nNewDimension;
    
    if( nNewDimension == 2 )
        z = 0;
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRPoint::WkbSize() const

{
    if( getCoordinateDimension() != 3 )
        return 21;
    else
        return 29;
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRPoint::importFromWkb( unsigned char * pabyData,
                                int nSize,
                                OGRwkbVariant eWkbVariant )

{
    OGRwkbByteOrder     eByteOrder;
    OGRBoolean          bIs3D = FALSE;

    OGRErr eErr = importPreambuleFromWkb( pabyData, nSize, eByteOrder, bIs3D, eWkbVariant );
    if( eErr != OGRERR_NONE )
        return eErr;

    if ( nSize < ((bIs3D) ? 29 : 21) && nSize != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the vertex.                                                 */
/* -------------------------------------------------------------------- */
    memcpy( &x, pabyData + 5,     8 );
    memcpy( &y, pabyData + 5 + 8, 8 );
    
    if( OGR_SWAP( eByteOrder ) )
    {
        CPL_SWAPDOUBLE( &x );
        CPL_SWAPDOUBLE( &y );
    }

    if( bIs3D )
    {
        memcpy( &z, pabyData + 5 + 16, 8 );
        if( OGR_SWAP( eByteOrder ) )
        {
            CPL_SWAPDOUBLE( &z );
        }
        nCoordDimension = 3;
    }
    else
    {
        z = 0;
        nCoordDimension = 2;
    }

    /* Detect NaN coordinates --> EMPTY */
    if( x != x && y != y )
        nCoordDimension = -nCoordDimension;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr  OGRPoint::exportToWkb( OGRwkbByteOrder eByteOrder,
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
    
    if ( eWkbVariant == wkbVariantIso )
        nGType = getIsoGeometryType();
    
    if( eByteOrder == wkbNDR )
        nGType = CPL_LSBWORD32( nGType );
    else
        nGType = CPL_MSBWORD32( nGType );

    memcpy( pabyData + 1, &nGType, 4 );
    
/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */

    if ( IsEmpty() && eWkbVariant == wkbVariantIso )
    {
        double dNan = std::numeric_limits<double>::quiet_NaN();
        memcpy( pabyData+5, &dNan, 8 );
        memcpy( pabyData+5+8, &dNan, 8 );
        if( getCoordinateDimension() == 3 )
            memcpy( pabyData+5+16, &dNan, 8 );
    }
    else
    {
        memcpy( pabyData+5, &x, 16 );
        if( getCoordinateDimension() == 3 )
        {
            memcpy( pabyData + 5 + 16, &z, 8 );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        CPL_SWAPDOUBLE( pabyData + 5 );
        CPL_SWAPDOUBLE( pabyData + 5 + 8 );

        if( getCoordinateDimension() == 3 )
            CPL_SWAPDOUBLE( pabyData + 5 + 16 );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate point from well known text format ``POINT           */
/*      (x,y)''.                                                        */
/************************************************************************/

OGRErr OGRPoint::importFromWkt( char ** ppszInput )

{
    int bHasZ = FALSE, bHasM = FALSE;
    bool bIsEmpty = false;
    OGRErr      eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bIsEmpty )
    {
        nCoordDimension = (bHasZ) ? -3 : -2;
        return OGRERR_NONE;
    }

    const char  *pszInput = *ppszInput;

/* -------------------------------------------------------------------- */
/*      Read the point list which should consist of exactly one point.  */
/* -------------------------------------------------------------------- */
    OGRRawPoint         *poPoints = NULL;
    double              *padfZ = NULL;
    int                 nMaxPoint = 0, nPoints = 0;

    pszInput = OGRWktReadPoints( pszInput, &poPoints, &padfZ,
                                 &nMaxPoint, &nPoints );
    if( pszInput == NULL || nPoints != 1 )
    {
        CPLFree( poPoints );
        CPLFree( padfZ );
        return OGRERR_CORRUPT_DATA;
    }

    x = poPoints[0].x;
    y = poPoints[0].y;

    CPLFree( poPoints );

    if( padfZ != NULL )
    {
        /* If there's a 3rd value, and it is not a POINT M, */
        /* then assume it is the Z */
        if ((!(bHasM && !bHasZ)))
        {
            z = padfZ[0];
            nCoordDimension = 3;
        }
        else
            nCoordDimension = 2;
        CPLFree( padfZ );
    }
    else if ( bHasZ )
    {
        /* In theory we should have a z coordinate for POINT Z */
        /* oh well, let be tolerant */
        nCoordDimension = 3;
    }
    else
        nCoordDimension = 2;

    *ppszInput = (char *) pszInput;
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivelent.                                                     */
/************************************************************************/

OGRErr OGRPoint::exportToWkt( char ** ppszDstText,
                              OGRwkbVariant eWkbVariant ) const

{
    char        szTextEquiv[140];
    char        szCoordinate[80];

    if ( IsEmpty() )
    {
        if( getCoordinateDimension() == 3 && eWkbVariant == wkbVariantIso )
            *ppszDstText = CPLStrdup("POINT Z EMPTY");
        else
            *ppszDstText = CPLStrdup("POINT EMPTY");
    }
    else
    {
        OGRMakeWktCoordinate(szCoordinate, x, y, z, getCoordinateDimension() );
        if( getCoordinateDimension() == 3 && eWkbVariant == wkbVariantIso )
            sprintf( szTextEquiv, "POINT Z (%s)", szCoordinate );
        else
            sprintf( szTextEquiv, "POINT (%s)", szCoordinate );
        *ppszDstText = CPLStrdup( szTextEquiv );
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRPoint::getEnvelope( OGREnvelope * psEnvelope ) const

{
    psEnvelope->MinX = psEnvelope->MaxX = getX();
    psEnvelope->MinY = psEnvelope->MaxY = getY();
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRPoint::getEnvelope( OGREnvelope3D * psEnvelope ) const

{
    psEnvelope->MinX = psEnvelope->MaxX = getX();
    psEnvelope->MinY = psEnvelope->MaxY = getY();
    psEnvelope->MinZ = psEnvelope->MaxZ = getZ();
}


/**
 * \fn double OGRPoint::getX() const;
 *
 * \brief Fetch X coordinate.
 *
 * Relates to the SFCOM IPoint::get_X() method.
 *
 * @return the X coordinate of this point. 
 */

/**
 * \fn double OGRPoint::getY() const;
 *
 * \brief Fetch Y coordinate.
 *
 * Relates to the SFCOM IPoint::get_Y() method.
 *
 * @return the Y coordinate of this point. 
 */

/**
 * \fn double OGRPoint::getZ() const;
 *
 * \brief Fetch Z coordinate.
 *
 * Relates to the SFCOM IPoint::get_Z() method.
 *
 * @return the Z coordinate of this point, or zero if it is a 2D point.
 */

/**
 * \fn void OGRPoint::setX( double xIn );
 *
 * \brief Assign point X coordinate.
 *
 * There is no corresponding SFCOM method.
 */ 

/**
 * \fn void OGRPoint::setY( double yIn );
 *
 * \brief Assign point Y coordinate.
 *
 * There is no corresponding SFCOM method.
 */ 

/**
 * \fn void OGRPoint::setZ( double zIn );
 *
 * \brief Assign point Z coordinate.
 * Calling this method will force the geometry
 * coordinate dimension to 3D (wkbPoint|wkbZ).
 *
 * There is no corresponding SFCOM method.  
 */ 

/************************************************************************/
/*                               Equal()                                */
/************************************************************************/

OGRBoolean OGRPoint::Equals( OGRGeometry * poOther ) const

{
    if( poOther== this )
        return TRUE;
    
    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    OGRPoint    *poOPoint = (OGRPoint *) poOther;
    if ( nCoordDimension != poOPoint->nCoordDimension )
        return FALSE;
    
    if ( IsEmpty() )
        return TRUE;

    // we should eventually test the SRS.
    if( poOPoint->getX() != getX()
        || poOPoint->getY() != getY()
        || poOPoint->getZ() != getZ() )
        return FALSE;
    else
        return TRUE;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRPoint::transform( OGRCoordinateTransformation *poCT )

{
#ifdef DISABLE_OGRGEOM_TRANSFORM
    return OGRERR_FAILURE;
#else
    if( poCT->Transform( 1, &x, &y, &z ) )
    {
        assignSpatialReference( poCT->GetTargetCS() );
        return OGRERR_NONE;
    }
    else
        return OGRERR_FAILURE;
#endif
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRPoint::IsEmpty(  ) const
{
    return nCoordDimension < 0;
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRPoint::swapXY()
{
    double dfTemp = x;
    x = y;
    y = dfTemp;
}

/************************************************************************/
/*                               Within()                               */
/************************************************************************/

OGRBoolean OGRPoint::Within( const OGRGeometry *poOtherGeom ) const

{
    if( !IsEmpty() && poOtherGeom != NULL &&
        wkbFlatten(poOtherGeom->getGeometryType()) == wkbCurvePolygon )
    {
        return ((OGRCurvePolygon*)poOtherGeom)->Contains(this);
    }
    else
        return OGRGeometry::Within(poOtherGeom);
}

/************************************************************************/
/*                              Intersects()                            */
/************************************************************************/

OGRBoolean OGRPoint::Intersects( const OGRGeometry *poOtherGeom ) const

{
    if( !IsEmpty() && poOtherGeom != NULL &&
        wkbFlatten(poOtherGeom->getGeometryType()) == wkbCurvePolygon )
    {
        return ((OGRCurvePolygon*)poOtherGeom)->Intersects(this);
    }
    else
        return OGRGeometry::Intersects(poOtherGeom);
}
