/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiPoint class.
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
/*                           OGRMultiPoint()                            */
/************************************************************************/

OGRMultiPoint::OGRMultiPoint()
{
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
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiPoint::getGeometryName() const

{
    return "MULTIPOINT";
}

/************************************************************************/
/*                        addGeometryDirectly()                         */
/*                                                                      */
/*      Add a new geometry to a collection.  Subclasses should          */
/*      override this to verify the type of the new geometry, and       */
/*      then call this method to actually add it.                       */
/************************************************************************/

OGRErr OGRMultiPoint::addGeometryDirectly( OGRGeometry * poNewGeom )

{
    if( poNewGeom->getGeometryType() != wkbPoint 
        && poNewGeom->getGeometryType() != wkbPoint25D )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    return OGRGeometryCollection::addGeometryDirectly( poNewGeom );
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRMultiPoint::clone() const

{
    OGRMultiPoint       *poNewGC;

    poNewGC = new OGRMultiPoint;
    poNewGC->assignSpatialReference( getSpatialReference() );

    for( int i = 0; i < getNumGeometries(); i++ )
    {
        poNewGC->addGeometry( getGeometryRef(i) );
    }

    return poNewGC;
}

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivelent.  This could be made alot more CPU efficient!        */
/************************************************************************/

OGRErr OGRMultiPoint::exportToWkt( char ** ppszDstText ) const

{
    int         nMaxString = getNumGeometries() * 20 + 128;
    int         nRetLen = 0;

    if( getNumGeometries() == 0 )
    {
        *ppszDstText = CPLStrdup("MULTIPOINT EMPTY");
        return OGRERR_NONE;
    }

    *ppszDstText = (char *) VSIMalloc( nMaxString );
    if( *ppszDstText == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

    sprintf( *ppszDstText, "%s (", getGeometryName() );

    for( int i = 0; i < getNumGeometries(); i++ )
    {
        OGRPoint        *poPoint = (OGRPoint *) getGeometryRef( i );

        if( i > 0 )
            strcat( *ppszDstText + nRetLen, "," );

        nRetLen += strlen(*ppszDstText + nRetLen);

        if( nMaxString < nRetLen + 100 )
        {
            nMaxString = nMaxString * 2;
            *ppszDstText = (char *) CPLRealloc(*ppszDstText,nMaxString);
        }
        
        OGRMakeWktCoordinate( *ppszDstText + nRetLen,
                              poPoint->getX(), 
                              poPoint->getY(),
                              poPoint->getZ(),
                              poPoint->getCoordinateDimension() );
    }

    strcat( *ppszDstText+nRetLen, ")" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

OGRErr OGRMultiPoint::importFromWkt( char ** ppszInput )

{

    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;
    int         iGeom;
    OGRErr      eErr = OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Clear existing Geoms.                                           */
/* -------------------------------------------------------------------- */
    empty();

/* -------------------------------------------------------------------- */
/*      Read and verify the type keyword, and ensure it matches the     */
/*      actual type of this container.                                  */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( !EQUAL(szToken,getGeometryName()) )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Skip past first bracket for checking purposes, but don't        */
/*      alter pszInput.                                                 */
/* -------------------------------------------------------------------- */
    const char *pszPreScan = pszInput;

    // skip white space. 
    while( *pszPreScan == ' ' || *pszPreScan == '\t' )
        pszPreScan++;

    // Handle the proper EMPTY syntax.
    if( EQUALN(pszPreScan,"EMPTY",5) )
    {
        *ppszInput = (char *) pszPreScan+5;
        return OGRERR_NONE;
    }

    // Skip outer bracket.
    if( *pszPreScan != '(' )
        return OGRERR_CORRUPT_DATA;

    pszPreScan++;

/* -------------------------------------------------------------------- */
/*      If the next token is EMPTY, then verify that we have proper     */
/*      EMPTY format will a trailing closing bracket.                   */
/* -------------------------------------------------------------------- */
    OGRWktReadToken( pszPreScan, szToken );
    if( EQUAL(szToken,"EMPTY") )
    {
        pszInput = OGRWktReadToken( pszPreScan, szToken );
        pszInput = OGRWktReadToken( pszInput, szToken );
        
        *ppszInput = (char *) pszInput;

        if( !EQUAL(szToken,")") )
            return OGRERR_CORRUPT_DATA;
        else
            return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Check for inner bracket indicating the improper bracketed       */
/*      format which we still want to support.                          */
/* -------------------------------------------------------------------- */
    // skip white space.
    while( *pszPreScan == ' ' || *pszPreScan == '\t' )
        pszPreScan++;

    // Do we have an inner bracket? 
    if( *pszPreScan == '(' )
        return importFromWkt_Bracketed( ppszInput );
    
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
        return OGRERR_CORRUPT_DATA;

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

OGRErr OGRMultiPoint::importFromWkt_Bracketed( char ** ppszInput )

{

    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;
    OGRErr      eErr = OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Skip MULTIPOINT keyword.                                        */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

/* -------------------------------------------------------------------- */
/*      Read points till we get to the closing bracket.                 */
/* -------------------------------------------------------------------- */
    int                 nMaxPoint = 0;
    int                 nPointCount = 0;
    OGRRawPoint         *paoPoints = NULL;
    double              *padfZ = NULL;

    while( (pszInput = OGRWktReadToken( pszInput, szToken ))
           && (EQUAL(szToken,"(") || EQUAL(szToken,",")) )
    {
        OGRGeometry     *poGeom;

        pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoint,
                                     &nPointCount );

        if( pszInput == NULL || nPointCount != 1 )
            return OGRERR_CORRUPT_DATA;

        if( padfZ )
            poGeom = new OGRPoint( paoPoints[0].x, 
                                   paoPoints[0].y, 
                                   padfZ[0] );
        else
            poGeom =  new OGRPoint( paoPoints[0].x, 
                                    paoPoints[0].y );

        eErr = addGeometryDirectly( poGeom );
        if( eErr != OGRERR_NONE )
            return eErr;
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


