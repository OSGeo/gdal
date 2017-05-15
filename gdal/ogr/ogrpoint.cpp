/******************************************************************************
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

#include "cpl_port.h"
#include "ogr_geometry.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <limits>
#include <new>

#include "cpl_conv.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                              OGRPoint()                              */
/************************************************************************/

/**
 * \brief Create an empty point.
 */

OGRPoint::OGRPoint()

{
    empty();
    flags = 0;
}

/************************************************************************/
/*                              OGRPoint()                              */
/************************************************************************/

/**
 * \brief Create a point.
 * @param xIn x
 * @param yIn y
 * @param zIn z
 */

OGRPoint::OGRPoint( double xIn, double yIn, double zIn ) :
    x(xIn),
    y(yIn),
    z(zIn),
    m(0.0)
{
    flags = OGR_G_NOT_EMPTY_POINT | OGR_G_3D;
}

/************************************************************************/
/*                              OGRPoint()                              */
/************************************************************************/

/**
 * \brief Create a point.
 * @param xIn x
 * @param yIn y
 */

OGRPoint::OGRPoint( double xIn, double yIn ) :
    x(xIn),
    y(yIn),
    z(0.0),
    m(0.0)
{
    flags = OGR_G_NOT_EMPTY_POINT;
}

/************************************************************************/
/*                              OGRPoint()                              */
/************************************************************************/

/**
 * \brief Create a point.
 * @param xIn x
 * @param yIn y
 * @param zIn z
 * @param mIn m
 */

OGRPoint::OGRPoint( double xIn, double yIn, double zIn, double mIn ) :
    x(xIn),
    y(yIn),
    z(zIn),
    m(mIn)
{
    flags = OGR_G_NOT_EMPTY_POINT | OGR_G_3D | OGR_G_MEASURED;
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
    z( other.z ),
    m( other.m )
{
}

/************************************************************************/
/*                             ~OGRPoint()                              */
/************************************************************************/

OGRPoint::~OGRPoint() {}

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
        m = other.m;
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
    OGRPoint *poNewPoint = new (std::nothrow) OGRPoint( x, y, z, m );
    if( poNewPoint == NULL )
        return NULL;

    poNewPoint->assignSpatialReference( getSpatialReference() );
    poNewPoint->flags = flags;

    return poNewPoint;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/
void OGRPoint::empty()

{
    x = 0.0;
    y = 0.0;
    z = 0.0;
    m = 0.0;
    flags &= ~OGR_G_NOT_EMPTY_POINT;
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
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbPointZM;
    else if( flags & OGR_G_MEASURED )
        return wkbPointM;
    else if( flags & OGR_G_3D )
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
    z = 0.0;
    m = 0.0;
    flags &= ~OGR_G_3D;
    setMeasured(FALSE);
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRPoint::setCoordinateDimension( int nNewDimension )

{
    if( nNewDimension == 2 )
        flattenTo2D();
    else if( nNewDimension == 3 )
        flags |= OGR_G_3D;

    setMeasured(FALSE);
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRPoint::WkbSize() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return 37;
    else if( (flags & OGR_G_3D) || (flags & OGR_G_MEASURED) )
        return 29;
    else
        return 21;
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRPoint::importFromWkb( const unsigned char *pabyData,
                                int nSize,
                                OGRwkbVariant eWkbVariant,
                                int& nBytesConsumedOut )

{
    nBytesConsumedOut = -1;
    OGRwkbByteOrder eByteOrder = wkbNDR;

    flags = 0;
    OGRErr eErr =
        importPreambuleFromWkb( pabyData, nSize, eByteOrder, eWkbVariant );
    pabyData += 5;
    if( eErr != OGRERR_NONE )
        return eErr;

    if( nSize != -1 )
    {
        if( (nSize < 37) && ((flags & OGR_G_3D) && (flags & OGR_G_MEASURED)) )
            return OGRERR_NOT_ENOUGH_DATA;
        else if( (nSize < 29) && ((flags & OGR_G_3D) ||
                                  (flags & OGR_G_MEASURED)) )
            return OGRERR_NOT_ENOUGH_DATA;
        else if( nSize < 21 )
            return OGRERR_NOT_ENOUGH_DATA;
    }

    nBytesConsumedOut = 5 + 8 * (2 + ((flags & OGR_G_3D) ? 1 : 0)+
                                     ((flags & OGR_G_MEASURED) ? 1 : 0));

/* -------------------------------------------------------------------- */
/*      Get the vertex.                                                 */
/* -------------------------------------------------------------------- */
    memcpy( &x, pabyData, 8 );
    pabyData += 8;
    memcpy( &y, pabyData, 8 );
    pabyData += 8;

    if( OGR_SWAP( eByteOrder ) )
    {
        CPL_SWAPDOUBLE( &x );
        CPL_SWAPDOUBLE( &y );
    }

    if( flags & OGR_G_3D )
    {
        memcpy( &z, pabyData, 8 );
        pabyData += 8;
        if( OGR_SWAP( eByteOrder ) )
            CPL_SWAPDOUBLE( &z );
    }
    else
    {
        z = 0;
    }
    if( flags & OGR_G_MEASURED )
    {
        memcpy( &m, pabyData, 8 );
        /*pabyData += 8; */
        if( OGR_SWAP( eByteOrder ) )
        {
            CPL_SWAPDOUBLE( &m );
        }
    }
    else
    {
        m = 0;
    }

    // Detect coordinates are not NaN --> NOT EMPTY.
    if( !(CPLIsNan(x) && CPLIsNan(y)) )
        flags |= OGR_G_NOT_EMPTY_POINT;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr OGRPoint::exportToWkb( OGRwkbByteOrder eByteOrder,
                              unsigned char * pabyData,
                              OGRwkbVariant eWkbVariant ) const

{
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] =
        DB2_V72_UNFIX_BYTE_ORDER(static_cast<unsigned char>(eByteOrder));
    pabyData += 1;

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */

    GUInt32 nGType = getGeometryType();

    if( eWkbVariant == wkbVariantPostGIS1 )
    {
        nGType = wkbFlatten(nGType);
        if( Is3D() )
            // Explicitly set wkb25DBit.
            nGType =
                static_cast<OGRwkbGeometryType>(nGType | wkb25DBitInternalUse);
        if( IsMeasured() )
            nGType = static_cast<OGRwkbGeometryType>(nGType | 0x40000000);
    }
    else if( eWkbVariant == wkbVariantIso )
    {
        nGType = getIsoGeometryType();
    }

    if( eByteOrder == wkbNDR )
        nGType = CPL_LSBWORD32( nGType );
    else
        nGType = CPL_MSBWORD32( nGType );

    memcpy( pabyData, &nGType, 4 );
    pabyData += 4;

/* -------------------------------------------------------------------- */
/*      Copy in the raw data. Swap if needed.                           */
/* -------------------------------------------------------------------- */

    if( IsEmpty() && eWkbVariant == wkbVariantIso )
    {
        const double dNan = std::numeric_limits<double>::quiet_NaN();
        memcpy( pabyData, &dNan, 8 );
        if( OGR_SWAP( eByteOrder ) )
            CPL_SWAPDOUBLE( pabyData );
        pabyData += 8;
        memcpy( pabyData, &dNan, 8 );
        if( OGR_SWAP( eByteOrder ) )
            CPL_SWAPDOUBLE( pabyData );
        pabyData += 8;
        if( flags & OGR_G_3D ) {
            memcpy( pabyData, &dNan, 8 );
            if( OGR_SWAP( eByteOrder ) )
                CPL_SWAPDOUBLE( pabyData );
            pabyData += 8;
        }
        if( flags & OGR_G_MEASURED ) {
            memcpy( pabyData, &dNan, 8 );
            if( OGR_SWAP( eByteOrder ) )
                CPL_SWAPDOUBLE( pabyData );
        }
    }
    else
    {
        memcpy( pabyData, &x, 8 );
        if( OGR_SWAP( eByteOrder ) )
            CPL_SWAPDOUBLE( pabyData );
        pabyData += 8;
        memcpy( pabyData, &y, 8 );
        if( OGR_SWAP( eByteOrder ) )
            CPL_SWAPDOUBLE( pabyData );
        pabyData += 8;
        if( flags & OGR_G_3D ) {
            memcpy( pabyData, &z, 8 );
            if( OGR_SWAP( eByteOrder ) )
                CPL_SWAPDOUBLE( pabyData );
            pabyData += 8;
        }
        if( flags & OGR_G_MEASURED )
        {
            memcpy( pabyData, &m, 8 );
            if( OGR_SWAP( eByteOrder ) )
                CPL_SWAPDOUBLE( pabyData );
        }
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
    int bHasZ = FALSE;
    int bHasM = FALSE;
    bool bIsEmpty = false;
    OGRErr eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    flags = 0;
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bHasZ ) flags |= OGR_G_3D;
    if( bHasM ) flags |= OGR_G_MEASURED;
    if( bIsEmpty )
    {
        // Should be at the end.
        if( !((*ppszInput[0] == '\000') || (*ppszInput[0] == ',')) )
            return OGRERR_CORRUPT_DATA;
        return OGRERR_NONE;
    }
    else
    {
        flags |= OGR_G_NOT_EMPTY_POINT;
    }

    const char *pszInput = *ppszInput;

/* -------------------------------------------------------------------- */
/*      Read the point list which should consist of exactly one point.  */
/* -------------------------------------------------------------------- */
    OGRRawPoint *poPoints = NULL;
    double *padfZ = NULL;
    double *padfM = NULL;
    int nMaxPoint = 0;
    int nPoints = 0;
    int flagsFromInput = flags;

    pszInput = OGRWktReadPointsM( pszInput, &poPoints, &padfZ, &padfM,
                                  &flagsFromInput,
                                  &nMaxPoint, &nPoints );
    if( pszInput == NULL || nPoints != 1 )
    {
        CPLFree( poPoints );
        CPLFree( padfZ );
        CPLFree( padfM );
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

    x = poPoints[0].x;
    y = poPoints[0].y;

    CPLFree( poPoints );

    if( bHasZ )
    {
        if( padfZ != NULL )
            z = padfZ[0];
    }
    if( bHasM )
    {
        if( padfM != NULL )
            m = padfM[0];
    }

    CPLFree( padfZ );
    CPLFree( padfM );

    *ppszInput = const_cast<char *>(pszInput);

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivalent.                                                     */
/************************************************************************/

OGRErr OGRPoint::exportToWkt( char ** ppszDstText,
                              OGRwkbVariant eWkbVariant ) const

{
    if( IsEmpty() )
    {
        if( eWkbVariant == wkbVariantIso )
        {
            if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
                *ppszDstText = CPLStrdup("POINT ZM EMPTY");
            else if( flags & OGR_G_MEASURED )
                *ppszDstText = CPLStrdup("POINT M EMPTY");
            else if( flags & OGR_G_3D )
                *ppszDstText = CPLStrdup("POINT Z EMPTY");
            else
                *ppszDstText = CPLStrdup("POINT EMPTY");
        }
        else
        {
            *ppszDstText = CPLStrdup("POINT EMPTY");
        }
    }
    else
    {
        char szTextEquiv[180] = {};
        if( eWkbVariant == wkbVariantIso )
        {
            char szCoordinate[80] = {};
            OGRMakeWktCoordinateM(szCoordinate, x, y, z, m,
                                  Is3D(), IsMeasured());
            if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
                snprintf(szTextEquiv, sizeof(szTextEquiv),
                         "POINT ZM (%s)", szCoordinate);
            else if( flags & OGR_G_MEASURED )
                snprintf(szTextEquiv, sizeof(szTextEquiv),
                         "POINT M (%s)", szCoordinate);
            else if( flags & OGR_G_3D )
                snprintf(szTextEquiv, sizeof(szTextEquiv),
                         "POINT Z (%s)", szCoordinate);
            else
                snprintf(szTextEquiv, sizeof(szTextEquiv),
                         "POINT (%s)", szCoordinate);
        }
        else
        {
            char szCoordinate[80] = {};
            OGRMakeWktCoordinateM(szCoordinate, x, y, z, m, Is3D(), FALSE);
            snprintf( szTextEquiv, sizeof(szTextEquiv),
                      "POINT (%s)", szCoordinate );
        }
        *ppszDstText = CPLStrdup( szTextEquiv );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRPoint::getEnvelope( OGREnvelope * psEnvelope ) const

{
    psEnvelope->MinX = getX();
    psEnvelope->MaxX = getX();
    psEnvelope->MinY = getY();
    psEnvelope->MaxY = getY();
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRPoint::getEnvelope( OGREnvelope3D * psEnvelope ) const

{
    psEnvelope->MinX = getX();
    psEnvelope->MaxX = getX();
    psEnvelope->MinY = getY();
    psEnvelope->MaxY = getY();
    psEnvelope->MinZ = getZ();
    psEnvelope->MaxZ = getZ();
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

    OGRPoint *poOPoint = dynamic_cast<OGRPoint *>(poOther);
    if( poOPoint == NULL )
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "dynamic_cast failed.  Expected OGRPoint.");
        return FALSE;
    }
    if( flags != poOPoint->flags )
        return FALSE;

    if( IsEmpty() )
        return TRUE;

    // Should eventually test the SRS.
    if( poOPoint->getX() != getX()
        || poOPoint->getY() != getY()
        || poOPoint->getZ() != getZ() )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRPoint::transform( OGRCoordinateTransformation *poCT )

{
    if( poCT->Transform( 1, &x, &y, &z ) )
    {
        assignSpatialReference( poCT->GetTargetCS() );
        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRPoint::swapXY()
{
    std::swap(x, y);
}

/************************************************************************/
/*                               Within()                               */
/************************************************************************/

OGRBoolean OGRPoint::Within( const OGRGeometry *poOtherGeom ) const

{
    if( !IsEmpty() && poOtherGeom != NULL &&
        wkbFlatten(poOtherGeom->getGeometryType()) == wkbCurvePolygon )
    {
        const OGRCurvePolygon *poCurve =
            dynamic_cast<const OGRCurvePolygon *>(poOtherGeom);
        if( poCurve == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRCurvePolygon.");
            return FALSE;
        }
        return poCurve->Contains(this);
    }

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
        const OGRCurvePolygon *poCurve =
            dynamic_cast<const OGRCurvePolygon *>(poOtherGeom);
        if( poCurve == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRCurvePolygon.");
            return FALSE;
        }
        return poCurve->Intersects(this);
    }

    return OGRGeometry::Intersects(poOtherGeom);
}
