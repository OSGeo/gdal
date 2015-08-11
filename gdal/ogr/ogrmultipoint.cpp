/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiPoint class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
/*                           OGRMultiPoint()                            */
/************************************************************************/

/**
 * \brief Create an empty multi point collection.
 */

OGRMultiPoint::OGRMultiPoint()
{
}

/************************************************************************/
/*                OGRMultiPoint( const OGRMultiPoint& )                 */
/************************************************************************/

/**
 * \brief Copy constructor.
 * 
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 * 
 * @since GDAL 2.1
 */

OGRMultiPoint::OGRMultiPoint( const OGRMultiPoint& other ) :
    OGRGeometryCollection(other)
{
}

/************************************************************************/
/*                          ~OGRMultiPoint()                            */
/************************************************************************/

OGRMultiPoint::~OGRMultiPoint()
{
}

/************************************************************************/
/*                  operator=( const OGRMultiPoint&)                    */
/************************************************************************/

/**
 * \brief Assignment operator.
 * 
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 * 
 * @since GDAL 2.1
 */

OGRMultiPoint& OGRMultiPoint::operator=( const OGRMultiPoint& other )
{
    if( this != &other)
    {
        OGRGeometryCollection::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiPoint::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbMultiPoint25D;
    else
        return wkbMultiPoint;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRMultiPoint::getDimension() const

{
    return 0;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiPoint::getGeometryName() const

{
    return "MULTIPOINT";
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

OGRBoolean OGRMultiPoint::isCompatibleSubType( OGRwkbGeometryType eGeomType ) const
{
    return wkbFlatten(eGeomType) == wkbPoint;
}

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivelent.  This could be made alot more CPU efficient!        */
/************************************************************************/

OGRErr OGRMultiPoint::exportToWkt( char ** ppszDstText,
                                   OGRwkbVariant eWkbVariant ) const

{
    int         nMaxString = getNumGeometries() * 22 + 128;
    int         nRetLen = 0;

/* -------------------------------------------------------------------- */
/*      Return MULTIPOINT EMPTY if we get no valid points.              */
/* -------------------------------------------------------------------- */
    if( IsEmpty() )
    {
        if( getCoordinateDimension() == 3 && eWkbVariant == wkbVariantIso )
            *ppszDstText = CPLStrdup("MULTIPOINT Z EMPTY");
        else
            *ppszDstText = CPLStrdup("MULTIPOINT EMPTY");
        return OGRERR_NONE;
    }

    *ppszDstText = (char *) VSIMalloc( nMaxString );
    if( *ppszDstText == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

    if( getCoordinateDimension() == 3 && eWkbVariant == wkbVariantIso )
        sprintf( *ppszDstText, "%s Z (", getGeometryName() );
    else
        sprintf( *ppszDstText, "%s (", getGeometryName() );

    int bMustWriteComma = FALSE;
    for( int i = 0; i < getNumGeometries(); i++ )
    {
        OGRPoint        *poPoint = (OGRPoint *) getGeometryRef( i );

        if (poPoint->IsEmpty())
        {
            CPLDebug( "OGR", "OGRMultiPoint::exportToWkt() - skipping POINT EMPTY.");
            continue;
        }

        if( bMustWriteComma )
            strcat( *ppszDstText + nRetLen, "," );
        bMustWriteComma = TRUE;

        nRetLen += strlen(*ppszDstText + nRetLen);

        if( nMaxString < nRetLen + 100 )
        {
            nMaxString = nMaxString * 2;
            *ppszDstText = (char *) CPLRealloc(*ppszDstText,nMaxString);
        }

        if( eWkbVariant == wkbVariantIso )
        {
            strcat( *ppszDstText + nRetLen, "(" );
            nRetLen ++;
        }

        OGRMakeWktCoordinate( *ppszDstText + nRetLen,
                              poPoint->getX(), 
                              poPoint->getY(),
                              poPoint->getZ(),
                              poPoint->getCoordinateDimension() );

        if( eWkbVariant == wkbVariantIso )
        {
            strcat( *ppszDstText + nRetLen, ")" );
            nRetLen ++;
        }
    }

    strcat( *ppszDstText+nRetLen, ")" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

OGRErr OGRMultiPoint::importFromWkt( char ** ppszInput )

{
    const char *pszInputBefore = *ppszInput;
    int bHasZ = FALSE, bHasM = FALSE;
    OGRErr      eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM);
    if( eErr >= 0 )
        return eErr;

    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;
    int         iGeom;
    eErr = OGRERR_NONE;

    const char* pszPreScan = OGRWktReadToken( pszInput, szToken );
    OGRWktReadToken( pszPreScan, szToken );

    // Do we have an inner bracket? 
    if (EQUAL(szToken,"(") || EQUAL(szToken, "EMPTY") )
    {
        *ppszInput = (char*) pszInputBefore;
        return importFromWkt_Bracketed( ppszInput, bHasM, bHasZ );
    }

    if (bHasZ || bHasM)
    {
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Read the point list which should consist of exactly one point.  */
/* -------------------------------------------------------------------- */
    int                 nMaxPoint = 0;
    int                 nPointCount = 0;
    OGRRawPoint         *paoPoints = NULL;
    double              *padfZ = NULL;

    pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoint,
                                 &nPointCount );
    if( pszInput == NULL )
    {
        OGRFree( paoPoints );
        OGRFree( padfZ );
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Transform raw points into point objects.                        */
/* -------------------------------------------------------------------- */
    for( iGeom = 0; iGeom < nPointCount && eErr == OGRERR_NONE; iGeom++ )
    {
        OGRGeometry     *poGeom;
        if( padfZ )
            poGeom = new OGRPoint( paoPoints[iGeom].x, 
                                   paoPoints[iGeom].y, 
                                   padfZ[iGeom] );
        else
            poGeom =  new OGRPoint( paoPoints[iGeom].x, 
                                    paoPoints[iGeom].y );

        eErr = addGeometryDirectly( poGeom );
    }

    OGRFree( paoPoints );
    if( padfZ )
        OGRFree( padfZ );

    if( eErr != OGRERR_NONE )
        return eErr;

    *ppszInput = (char *) pszInput;
    
    return OGRERR_NONE;
}


/************************************************************************/
/*                      importFromWkt_Bracketed()                       */
/*                                                                      */
/*      This operates similar to importFromWkt(), but reads a format    */
/*      with brackets around each point.  This is the form defined      */
/*      in the BNF of the SFSQL spec.  It is called from                */
/*      importFromWkt().                                                */
/************************************************************************/

OGRErr OGRMultiPoint::importFromWkt_Bracketed( char ** ppszInput, int bHasM, int bHasZ )

{

    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;
    OGRErr      eErr = OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Skip MULTIPOINT keyword.                                        */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if (bHasZ || bHasM)
    {
        /* Skip Z, M or ZM */
        pszInput = OGRWktReadToken( pszInput, szToken );
    }

/* -------------------------------------------------------------------- */
/*      Read points till we get to the closing bracket.                 */
/* -------------------------------------------------------------------- */
    int                 nMaxPoint = 0;
    int                 nPointCount = 0;
    OGRRawPoint         *paoPoints = NULL;
    double              *padfZ = NULL;

    while( (pszInput = OGRWktReadToken( pszInput, szToken )) != NULL
           && (EQUAL(szToken,"(") || EQUAL(szToken,",")) )
    {
        OGRGeometry     *poGeom;

        const char* pszNext = OGRWktReadToken( pszInput, szToken );
        if (EQUAL(szToken,"EMPTY"))
        {
            poGeom = new OGRPoint(0,0);
            poGeom->empty();
            eErr = addGeometryDirectly( poGeom );
            if( eErr != OGRERR_NONE )
            {
                delete poGeom;
                return eErr;
            }

            pszInput = pszNext;

            continue;
        }

        pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoint,
                                     &nPointCount );

        if( pszInput == NULL || nPointCount != 1 )
        {
            OGRFree( paoPoints );
            OGRFree( padfZ );
            return OGRERR_CORRUPT_DATA;
        }

        /* Ignore Z array when we have a MULTIPOINT M */
        if( padfZ && !(bHasM && !bHasZ))
            poGeom = new OGRPoint( paoPoints[0].x, 
                                   paoPoints[0].y, 
                                   padfZ[0] );
        else
            poGeom =  new OGRPoint( paoPoints[0].x, 
                                    paoPoints[0].y );

        eErr = addGeometryDirectly( poGeom );
        if( eErr != OGRERR_NONE )
        {
            delete poGeom;
            return eErr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    OGRFree( paoPoints );
    if( padfZ )
        OGRFree( padfZ );

    if( !EQUAL(szToken,")") )
        return OGRERR_CORRUPT_DATA;

    *ppszInput = (char *) pszInput;
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRMultiPoint::hasCurveGeometry(CPL_UNUSED int bLookForNonLinear) const
{
    return FALSE;
}
