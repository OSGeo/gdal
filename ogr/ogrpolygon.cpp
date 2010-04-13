/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRPolygon geometry class.
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
#include "ogr_geos.h"
#include "ogr_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             OGRPolygon()                             */
/************************************************************************/

/**
 * \brief Create an empty polygon.
 */

OGRPolygon::OGRPolygon()

{
    nRingCount = 0;
    papoRings = NULL;
}

/************************************************************************/
/*                            ~OGRPolygon()                             */
/************************************************************************/

OGRPolygon::~OGRPolygon()

{
    empty();
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRPolygon::clone() const

{
    OGRPolygon  *poNewPolygon;

    poNewPolygon = new OGRPolygon;
    poNewPolygon->assignSpatialReference( getSpatialReference() );

    for( int i = 0; i < nRingCount; i++ )
    {
        poNewPolygon->addRing( papoRings[i] );
    }

    return poNewPolygon;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRPolygon::empty()

{
    if( papoRings != NULL )
    {
        for( int i = 0; i < nRingCount; i++ )
        {
            delete papoRings[i];
        }
        OGRFree( papoRings );
    }

    papoRings = NULL;
    nRingCount = 0;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRPolygon::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbPolygon25D;
    else
        return wkbPolygon;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRPolygon::getDimension() const

{
    return 2;
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRPolygon::flattenTo2D()

{
    for( int iRing = 0; iRing < nRingCount; iRing++ )
        papoRings[iRing]->flattenTo2D();

    nCoordDimension = 2;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRPolygon::getGeometryName() const

{
    return "POLYGON";
}

/************************************************************************/
/*                          getExteriorRing()                           */
/************************************************************************/

/**
 * \brief Fetch reference to external polygon ring.
 *
 * Note that the returned ring pointer is to an internal data object of
 * the OGRPolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use
 * the OGRGeometry::clone() method to make a separate copy within the
 * application.
 *
 * Relates to the SFCOM IPolygon::get_ExteriorRing() method.
 *
 * @return pointer to external ring.  May be NULL if the OGRPolygon is empty.
 */

OGRLinearRing *OGRPolygon::getExteriorRing()

{
    if( nRingCount > 0 )
        return papoRings[0];
    else
        return NULL;
}

const OGRLinearRing *OGRPolygon::getExteriorRing() const

{
    if( nRingCount > 0 )
        return papoRings[0];
    else
        return NULL;
}

/************************************************************************/
/*                        getNumInteriorRings()                         */
/************************************************************************/

/**
 * \brief Fetch the number of internal rings.
 *
 * Relates to the SFCOM IPolygon::get_NumInteriorRings() method.
 *
 * @return count of internal rings, zero or more.
 */


int OGRPolygon::getNumInteriorRings() const

{
    if( nRingCount > 0 )
        return nRingCount-1;
    else
        return 0;
}

/************************************************************************/
/*                          getInteriorRing()                           */
/************************************************************************/

/**
 * \brief Fetch reference to indicated internal ring.
 *
 * Note that the returned ring pointer is to an internal data object of
 * the OGRPolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use
 * the OGRGeometry::clone() method to make a separate copy within the
 * application.
 *
 * Relates to the SFCOM IPolygon::get_InternalRing() method.
 *
 * @param iRing internal ring index from 0 to getNumInternalRings() - 1.
 *
 * @return pointer to external ring.  May be NULL if the OGRPolygon is empty.
 */

OGRLinearRing *OGRPolygon::getInteriorRing( int iRing )

{
    if( iRing < 0 || iRing >= nRingCount-1 )
        return NULL;
    else
        return papoRings[iRing+1];
}

const OGRLinearRing *OGRPolygon::getInteriorRing( int iRing ) const

{
    if( iRing < 0 || iRing >= nRingCount-1 )
        return NULL;
    else
        return papoRings[iRing+1];
}

/************************************************************************/
/*                              addRing()                               */
/************************************************************************/

/**
 * \brief Add a ring to a polygon.
 *
 * If the polygon has no external ring (it is empty) this will be used as
 * the external ring, otherwise it is used as an internal ring.  The passed
 * OGRLinearRing remains the responsibility of the caller (an internal copy
 * is made).
 *
 * This method has no SFCOM analog.
 *
 * @param poNewRing ring to be added to the polygon.
 */

void OGRPolygon::addRing( OGRLinearRing * poNewRing )

{
    papoRings = (OGRLinearRing **) OGRRealloc( papoRings,
                                               sizeof(void*) * (nRingCount+1));

    papoRings[nRingCount] = new OGRLinearRing( poNewRing );

    nRingCount++;

    if( poNewRing->getCoordinateDimension() == 3 )
        nCoordDimension = 3;
}

/************************************************************************/
/*                          addRingDirectly()                           */
/************************************************************************/

/**
 * \brief Add a ring to a polygon.
 *
 * If the polygon has no external ring (it is empty) this will be used as
 * the external ring, otherwise it is used as an internal ring.  Ownership
 * of the passed ring is assumed by the OGRPolygon, but otherwise this
 * method operates the same as OGRPolygon::AddRing().
 *
 * This method has no SFCOM analog.
 *
 * @param poNewRing ring to be added to the polygon.
 */

void OGRPolygon::addRingDirectly( OGRLinearRing * poNewRing )

{
    papoRings = (OGRLinearRing **) OGRRealloc( papoRings,
                                               sizeof(void*) * (nRingCount+1));

    papoRings[nRingCount] = poNewRing;

    nRingCount++;


    if( poNewRing->getCoordinateDimension() == 3 )
        nCoordDimension = 3;
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRPolygon::WkbSize() const

{
    int         nSize = 9;
    int         b3D = getCoordinateDimension() == 3;

    for( int i = 0; i < nRingCount; i++ )
    {
        nSize += papoRings[i]->_WkbSize( b3D );
    }

    return nSize;
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRPolygon::importFromWkb( unsigned char * pabyData,
                                  int nSize )

{
    OGRwkbByteOrder     eByteOrder;
    int                 nDataOffset, b3D;
    
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
#ifdef DEBUG
    OGRwkbGeometryType eGeometryType;
    
    if( eByteOrder == wkbNDR )
        eGeometryType = (OGRwkbGeometryType) pabyData[1];
    else
        eGeometryType = (OGRwkbGeometryType) pabyData[4];

    if( eGeometryType != wkbPolygon )
        return OGRERR_CORRUPT_DATA;
#endif    

    if( eByteOrder == wkbNDR )
        b3D = pabyData[4] & 0x80 || pabyData[2] & 0x80;
    else
        b3D = pabyData[1] & 0x80 || pabyData[3] & 0x80;

    if( b3D )
        nCoordDimension = 3;
    else
        nCoordDimension = 2;

/* -------------------------------------------------------------------- */
/*      Do we already have some rings?                                  */
/* -------------------------------------------------------------------- */
    if( nRingCount != 0 )
    {
        for( int iRing = 0; iRing < nRingCount; iRing++ )
            delete papoRings[iRing];

        OGRFree( papoRings );
        papoRings = NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Get the ring count.                                             */
/* -------------------------------------------------------------------- */
    memcpy( &nRingCount, pabyData + 5, 4 );
    
    if( OGR_SWAP( eByteOrder ) )
        nRingCount = CPL_SWAP32(nRingCount);

    if (nRingCount < 0 || nRingCount > INT_MAX / 4)
    {
        nRingCount = 0;
        return OGRERR_CORRUPT_DATA;
    }

    /* Each ring has a minimum of 4 bytes (point count) */
    if (nSize != -1 && nSize - 9 < nRingCount * 4)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Length of input WKB is too small" );
        nRingCount = 0;
        return OGRERR_NOT_ENOUGH_DATA;
    }

    papoRings = (OGRLinearRing **) VSIMalloc2(sizeof(void*), nRingCount);
    if (nRingCount != 0 && papoRings == NULL)
    {
        nRingCount = 0;
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    nDataOffset = 9;
    if( nSize != -1 )
        nSize -= nDataOffset;

/* -------------------------------------------------------------------- */
/*      Get the rings.                                                  */
/* -------------------------------------------------------------------- */
    for( int iRing = 0; iRing < nRingCount; iRing++ )
    {
        OGRErr  eErr;
        
        papoRings[iRing] = new OGRLinearRing();
        eErr = papoRings[iRing]->_importFromWkb( eByteOrder, b3D,
                                                 pabyData + nDataOffset,
                                                 nSize );
        if( eErr != OGRERR_NONE )
        {
            delete papoRings[iRing];
            nRingCount = iRing;
            return eErr;
        }

        if( nSize != -1 )
            nSize -= papoRings[iRing]->_WkbSize( b3D );

        nDataOffset += papoRings[iRing]->_WkbSize( b3D );
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr  OGRPolygon::exportToWkb( OGRwkbByteOrder eByteOrder,
                                 unsigned char * pabyData ) const

{
    int         nOffset;
    int         b3D = getCoordinateDimension() == 3;
    
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
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        int     nCount;

        nCount = CPL_SWAP32( nRingCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
    {
        memcpy( pabyData+5, &nRingCount, 4 );
    }
    
    nOffset = 9;
    
/* ==================================================================== */
/*      Serialize each of the rings.                                    */
/* ==================================================================== */
    for( int iRing = 0; iRing < nRingCount; iRing++ )
    {
        papoRings[iRing]->_exportToWkb( eByteOrder, b3D,
                                        pabyData + nOffset );

        nOffset += papoRings[iRing]->_WkbSize(b3D);
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.  Currently this is     */
/*      `POLYGON ((x y, x y, ...),(x y, ...),...)'.                     */
/************************************************************************/

OGRErr OGRPolygon::importFromWkt( char ** ppszInput )

{
    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;
    int         iRing;

/* -------------------------------------------------------------------- */
/*      Clear existing rings.                                           */
/* -------------------------------------------------------------------- */
    if( nRingCount > 0 )
    {
        for( iRing = 0; iRing < nRingCount; iRing++ )
            delete papoRings[iRing];
        
        nRingCount = 0;
        CPLFree( papoRings );
    }

/* -------------------------------------------------------------------- */
/*      Read and verify the ``POLYGON'' keyword token.                  */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( !EQUAL(szToken,"POLYGON") )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Check for EMPTY ...                                             */
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
        /* Test for old-style POLYGON(EMPTY) */
        pszPreScan = OGRWktReadToken( pszPreScan, szToken );
        if( EQUAL(szToken,"EMPTY") )
        {
            pszPreScan = OGRWktReadToken( pszPreScan, szToken );

            if( EQUAL(szToken,",") )
            {
                /* This is OK according to SFSQL SPEC. */
            }
            else if( !EQUAL(szToken,")") )
                return OGRERR_CORRUPT_DATA;
            else
            {
                *ppszInput = (char *) pszPreScan;
                empty();
                return OGRERR_NONE;
            }
        }
    }

    /* Skip first '(' */
    pszInput = OGRWktReadToken( pszInput, szToken );

/* ==================================================================== */
/*      Read each ring in turn.  Note that we try to reuse the same     */
/*      point list buffer from ring to ring to cut down on              */
/*      allocate/deallocate overhead.                                   */
/* ==================================================================== */
    OGRRawPoint *paoPoints = NULL;
    int         nMaxPoints = 0, nMaxRings = 0;
    double      *padfZ = NULL;

    nCoordDimension = 2;
    
    do
    {
        int     nPoints = 0;

        const char* pszNext = OGRWktReadToken( pszInput, szToken );
        if (EQUAL(szToken,"EMPTY"))
        {
/* -------------------------------------------------------------------- */
/*      Do we need to grow the ring array?                              */
/* -------------------------------------------------------------------- */
            if( nRingCount == nMaxRings )
            {
                nMaxRings = nMaxRings * 2 + 1;
                papoRings = (OGRLinearRing **)
                    CPLRealloc(papoRings, nMaxRings * sizeof(OGRLinearRing*));
            }
            papoRings[nRingCount] = new OGRLinearRing();
            nRingCount++;

            pszInput = OGRWktReadToken( pszNext, szToken );
            if ( !EQUAL(szToken, ",") )
                break;

            continue;
        }

/* -------------------------------------------------------------------- */
/*      Read points for one ring from input.                            */
/* -------------------------------------------------------------------- */
        pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoints,
                                     &nPoints );

        if( pszInput == NULL || nPoints == 0 )
        {
            CPLFree( paoPoints );
            return OGRERR_CORRUPT_DATA;
        }
        
/* -------------------------------------------------------------------- */
/*      Do we need to grow the ring array?                              */
/* -------------------------------------------------------------------- */
        if( nRingCount == nMaxRings )
        {
            nMaxRings = nMaxRings * 2 + 1;
            papoRings = (OGRLinearRing **)
                CPLRealloc(papoRings, nMaxRings * sizeof(OGRLinearRing*));
        }

/* -------------------------------------------------------------------- */
/*      Create the new ring, and assign to ring list.                   */
/* -------------------------------------------------------------------- */
        papoRings[nRingCount] = new OGRLinearRing();
        /* Ignore Z array when we have a POLYGON M */
        if (bHasM && !bHasZ)
            papoRings[nRingCount]->setPoints( nPoints, paoPoints, NULL );
        else
            papoRings[nRingCount]->setPoints( nPoints, paoPoints, padfZ );

        nRingCount++;

        if( padfZ && !(bHasM && !bHasZ) )
            nCoordDimension = 3;

/* -------------------------------------------------------------------- */
/*      Read the delimeter following the ring.                          */
/* -------------------------------------------------------------------- */
        
        pszInput = OGRWktReadToken( pszInput, szToken );
    } while( szToken[0] == ',' );

/* -------------------------------------------------------------------- */
/*      freak if we don't get a closing bracket.                        */
/* -------------------------------------------------------------------- */
    CPLFree( paoPoints );
    CPLFree( padfZ );

    if( szToken[0] != ')' )
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

OGRErr OGRPolygon::exportToWkt( char ** ppszDstText ) const

{
    char        **papszRings;
    int         iRing, nCumulativeLength = 0, nNonEmptyRings = 0;
    OGRErr      eErr;
    int         bMustWriteComma = FALSE;

/* -------------------------------------------------------------------- */
/*      If we have no valid exterior ring, return POLYGON EMPTY.        */
/* -------------------------------------------------------------------- */
    if (getExteriorRing() == NULL ||
        getExteriorRing()->IsEmpty())
    {
        *ppszDstText = CPLStrdup("POLYGON EMPTY");
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each ring.     */
/* -------------------------------------------------------------------- */
    papszRings = (char **) CPLCalloc(sizeof(char *),nRingCount);

    for( iRing = 0; iRing < nRingCount; iRing++ )
    {
        papoRings[iRing]->setCoordinateDimension( getCoordinateDimension() );
        if( papoRings[iRing]->getNumPoints() == 0 )
        {
            papszRings[iRing] = NULL;
            continue;
        }

        eErr = papoRings[iRing]->exportToWkt( &(papszRings[iRing]) );
        if( eErr != OGRERR_NONE )
            goto error;

        CPLAssert( EQUALN(papszRings[iRing],"LINEARRING (", 12) );
        nCumulativeLength += strlen(papszRings[iRing] + 11);

        nNonEmptyRings++;
    }

/* -------------------------------------------------------------------- */
/*      Allocate exactly the right amount of space for the              */
/*      aggregated string.                                              */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSIMalloc(nCumulativeLength + nNonEmptyRings + 11);

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszDstText, "POLYGON (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( iRing = 0; iRing < nRingCount; iRing++ )
    {                                                           
        if( papszRings[iRing] == NULL )
        {
            CPLDebug( "OGR", "OGRPolygon::exportToWkt() - skipping empty ring.");
            continue;
        }

        if( bMustWriteComma )
            (*ppszDstText)[nCumulativeLength++] = ',';
        bMustWriteComma = TRUE;
        
        int nRingLen = strlen(papszRings[iRing] + 11);
        memcpy( *ppszDstText + nCumulativeLength, papszRings[iRing] + 11, nRingLen );
        nCumulativeLength += nRingLen;
        VSIFree( papszRings[iRing] );
    }

    (*ppszDstText)[nCumulativeLength++] = ')';
    (*ppszDstText)[nCumulativeLength] = '\0';

    CPLFree( papszRings );

    return OGRERR_NONE;

error:
    for( iRing = 0; iRing < nRingCount; iRing++ )
        CPLFree(papszRings[iRing]);
    CPLFree(papszRings);
    return eErr;
}

/************************************************************************/
/*                           PointOnSurface()                           */
/************************************************************************/

int OGRPolygon::PointOnSurface( OGRPoint *poPoint ) const

{
    if( poPoint == NULL )
        return OGRERR_FAILURE;
 
#ifndef HAVE_GEOS
    return OGRERR_FAILURE;
#else
    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
     
    hThisGeosGeom = exportToGEOS();
 
    if( hThisGeosGeom != NULL )
    {
     	hOtherGeosGeom = GEOSPointOnSurface( hThisGeosGeom );
        GEOSGeom_destroy( hThisGeosGeom );

        if( hOtherGeosGeom == NULL )
            return OGRERR_FAILURE;

        OGRGeometry *poInsidePointGeom = (OGRGeometry *) 
            OGRGeometryFactory::createFromGEOS( hOtherGeosGeom );
 
        GEOSGeom_destroy( hOtherGeosGeom );

        if (poInsidePointGeom == NULL)
            return OGRERR_FAILURE;
        if (wkbFlatten(poInsidePointGeom->getGeometryType()) != wkbPoint)
        {
            delete poInsidePointGeom;
            return OGRERR_FAILURE;
        }

        OGRPoint *poInsidePoint = (OGRPoint *) poInsidePointGeom;
 	poPoint->setX( poInsidePoint->getX() );
 	poPoint->setY( poInsidePoint->getY() );
 
        delete poInsidePointGeom;
 
     	return OGRERR_NONE;
    }
    else
    {
     	return OGRERR_FAILURE;
    }
#endif /* HAVE_GEOS */
}


/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/
 
void OGRPolygon::getEnvelope( OGREnvelope * psEnvelope ) const

{
    OGREnvelope         oRingEnv;
    
    if( nRingCount == 0 )
        return;

    papoRings[0]->getEnvelope( psEnvelope );

    for( int iRing = 1; iRing < nRingCount; iRing++ )
    {
        papoRings[iRing]->getEnvelope( &oRingEnv );

        if( psEnvelope->MinX > oRingEnv.MinX )
            psEnvelope->MinX = oRingEnv.MinX;
        if( psEnvelope->MinY > oRingEnv.MinY )
            psEnvelope->MinY = oRingEnv.MinY;
        if( psEnvelope->MaxX < oRingEnv.MaxX )
            psEnvelope->MaxX = oRingEnv.MaxX;
        if( psEnvelope->MaxY < oRingEnv.MaxY )
            psEnvelope->MaxY = oRingEnv.MaxY;
    }
}

/************************************************************************/
/*                               Equal()                                */
/************************************************************************/

OGRBoolean OGRPolygon::Equals( OGRGeometry * poOther ) const

{
    OGRPolygon *poOPoly = (OGRPolygon *) poOther;

    if( poOPoly == this )
        return TRUE;
    
    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if( getNumInteriorRings() != poOPoly->getNumInteriorRings() )
        return FALSE;

    if( getExteriorRing() == NULL && poOPoly->getExteriorRing() == NULL )
        /* ok */;
    else if( getExteriorRing() == NULL || poOPoly->getExteriorRing() == NULL )
        return FALSE;
    else if( !getExteriorRing()->Equals( poOPoly->getExteriorRing() ) )
        return FALSE;
    
    // we should eventually test the SRS.

    for( int iRing = 0; iRing < getNumInteriorRings(); iRing++ )
    {
        if( !getInteriorRing(iRing)->Equals(poOPoly->getInteriorRing(iRing)) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRPolygon::transform( OGRCoordinateTransformation *poCT )

{
#ifdef DISABLE_OGRGEOM_TRANSFORM
    return OGRERR_FAILURE;
#else
    for( int iRing = 0; iRing < nRingCount; iRing++ )
    {
        OGRErr  eErr;

        eErr = papoRings[iRing]->transform( poCT );
        if( eErr != OGRERR_NONE )
        {
            if( iRing != 0 )
            {
                CPLDebug("OGR", 
                         "OGRPolygon::transform() failed for a ring other\n"
                         "than the first, meaning some rings are transformed\n"
                         "and some are not!\n" );

                return OGRERR_FAILURE;
            }

            return eErr;
        }
    }

    assignSpatialReference( poCT->GetTargetCS() );

    return OGRERR_NONE;
#endif
}

/************************************************************************/
/*                           IsPointOnSurface()                           */
/************************************************************************/

OGRBoolean OGRPolygon::IsPointOnSurface( const OGRPoint * pt) const
{
    if ( NULL == pt)
        return 0;

    for( int iRing = 0; iRing < nRingCount; iRing++ )
    {
        if ( papoRings[iRing]->isPointInRing(pt) )
        {
            return 1;
        }
    }

    return 0;
}

/************************************************************************/
/*                             closeRings()                             */
/************************************************************************/

void OGRPolygon::closeRings()

{
    for( int iRing = 0; iRing < nRingCount; iRing++ )
        papoRings[iRing]->closeRings();
}

/************************************************************************/
/*                              get_Area()                              */
/************************************************************************/

/**
 * \brief Compute area of polygon.
 *
 * The area is computed as the area of the outer ring less the area of all
 * internal rings. 
 *
 * @return computed area.
 */

double OGRPolygon::get_Area() const

{
    double dfArea = 0.0;

    if( getExteriorRing() != NULL )
    {
        int iRing;

        dfArea = getExteriorRing()->get_Area();

        for( iRing = 0; iRing < getNumInteriorRings(); iRing++ )
            dfArea -= getInteriorRing( iRing )->get_Area();
    }

    return dfArea;
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRPolygon::setCoordinateDimension( int nNewDimension )

{
    for( int iRing = 0; iRing < nRingCount; iRing++ )
        papoRings[iRing]->setCoordinateDimension( nNewDimension );

    OGRGeometry::setCoordinateDimension( nNewDimension );
}


/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRPolygon::IsEmpty(  ) const
{
    for( int iRing = 0; iRing < nRingCount; iRing++ )
        if (papoRings[iRing]->IsEmpty() == FALSE)
            return FALSE;
    return TRUE;
}

/************************************************************************/
/*                       OGRPolygon::segmentize()                       */
/************************************************************************/

void OGRPolygon::segmentize( double dfMaxLength )
{
    for( int iRing = 0; iRing < nRingCount; iRing++ )
        papoRings[iRing]->segmentize(dfMaxLength);
}
