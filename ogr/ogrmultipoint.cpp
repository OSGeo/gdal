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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.21  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.20  2005/07/20 01:43:51  fwarmerdam
 * upgraded OGR geometry dimension handling
 *
 * Revision 1.19  2005/07/12 17:34:00  fwarmerdam
 * updated to produce proper empty syntax and consume either
 *
 * Revision 1.18  2004/02/21 15:36:14  warmerda
 * const correctness updates for geometry: bug 289
 *
 * Revision 1.17  2004/01/16 21:57:16  warmerda
 * fixed up EMPTY support
 *
 * Revision 1.16  2004/01/16 21:20:00  warmerda
 * Added EMPTY support
 *
 * Revision 1.15  2003/09/11 22:47:54  aamici
 * add class constructors and destructors where needed in order to
 * let the mingw/cygwin binutils produce sensible partially linked objet files
 * with 'ld -r'.
 *
 * Revision 1.14  2003/05/28 19:16:43  warmerda
 * fixed up argument names and stuff for docs
 *
 * Revision 1.13  2003/04/28 15:28:53  warmerda
 * Ryan Proulx fixed WKT MULTIPOINT format
 *
 * Revision 1.12  2002/10/25 15:20:50  warmerda
 * fixed MULTIPOINT WKT format
 *
 * Revision 1.11  2002/09/11 13:47:17  warmerda
 * preliminary set of fixes for 3D WKB enum
 *
 * Revision 1.10  2002/08/06 21:16:41  warmerda
 * fixed possible overrun error in exportToWkt()
 *
 * Revision 1.9  2001/12/19 22:44:14  warmerda
 * fixed bug in conversion to WKT
 *
 * Revision 1.8  2001/11/01 17:01:28  warmerda
 * pass output buffer into OGRMakeWktCoordinate
 *
 * Revision 1.7  2001/07/19 18:25:07  warmerda
 * expanded tabs
 *
 * Revision 1.6  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.5  2001/05/29 02:24:33  warmerda
 * fixed bracket count on import
 *
 * Revision 1.4  2001/05/24 18:05:18  warmerda
 * substantial fixes to WKT support for MULTIPOINT/LINE/POLYGON
 *
 * Revision 1.3  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.2  1999/06/25 20:44:43  warmerda
 * implemented assignSpatialReference, carry properly
 *
 * Revision 1.1  1999/05/31 20:42:05  warmerda
 * New
 *
 */

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


