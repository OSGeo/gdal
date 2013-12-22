/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The Point geometry class.
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

CPL_CVSID("$Id$");

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
/*                             ~OGRPoint()                              */
/************************************************************************/

OGRPoint::~OGRPoint()

{
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
    nCoordDimension = 0;
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
    if( nCoordDimension == 3 )
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
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRPoint::setCoordinateDimension( int nNewDimension )

{
    nCoordDimension = nNewDimension;
    
    if( nCoordDimension == 2 )
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
    if( nCoordDimension != 3 )
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
                                int nSize )

{
    OGRwkbByteOrder     eByteOrder;
    
    if( nSize < 21 && nSize != -1 )
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
    OGRBoolean bIs3D;
    OGRwkbGeometryType eGeometryType;
    OGRErr err = OGRReadWKBGeometryType( pabyData, &eGeometryType, &bIs3D );

    if( err != OGRERR_NONE || eGeometryType != wkbPoint )
        return OGRERR_CORRUPT_DATA;

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
        if ( nSize < 29 && nSize != -1 )
            return OGRERR_NOT_ENOUGH_DATA;

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
        if( nCoordDimension == 3 )
            memcpy( pabyData+5+16, &dNan, 8 );
    }
    else
    {
        memcpy( pabyData+5, &x, 16 );
        if( nCoordDimension == 3 )
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

        if( nCoordDimension == 3 )
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
    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;

/* -------------------------------------------------------------------- */
/*      Read and verify the ``POINT'' keyword token.                    */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( !EQUAL(szToken,"POINT") )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Check for EMPTY ... but treat like a point at 0,0.              */
/* -------------------------------------------------------------------- */
    const char *pszPreScan;
    int bHasZ = FALSE, bHasM = FALSE;

    pszPreScan = OGRWktReadToken( pszInput, szToken );
    if( EQUAL(szToken,"EMPTY") )
    {
        *ppszInput = (char *) pszPreScan;
        empty();
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
        /* Test for old-style POINT(EMPTY) */
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

OGRErr OGRPoint::exportToWkt( char ** ppszDstText ) const

{
    char        szTextEquiv[140];
    char        szCoordinate[80];

    if ( IsEmpty() )
        *ppszDstText = CPLStrdup( "POINT EMPTY" );
    else
    {
        OGRMakeWktCoordinate(szCoordinate, x, y, z, nCoordDimension );
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
    OGRPoint    *poOPoint = (OGRPoint *) poOther;
    
    if( poOPoint== this )
        return TRUE;
    
    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if ( IsEmpty() && poOther->IsEmpty() )
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
    return nCoordDimension == 0;
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
