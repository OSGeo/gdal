/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiPoint class.
 * Author:   Frank Warmerdam, warmerda@home.com
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

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiPoint::getGeometryType()

{
    return wkbMultiPoint;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiPoint::getGeometryName()

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

OGRGeometry *OGRMultiPoint::clone()

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

OGRErr OGRMultiPoint::exportToWkt( char ** ppszReturn )

{
    int         nMaxString = getNumGeometries() * 16 * 2 + 20;
    int         nRetLen = 0;

    *ppszReturn = (char *) VSIMalloc( nMaxString );
    if( *ppszReturn == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

    sprintf( *ppszReturn, "%s (", getGeometryName() );

    for( int i = 0; i < getNumGeometries(); i++ )
    {
        OGRPoint	*poPoint = (OGRPoint *) getGeometryRef( i );

        assert( nMaxString > (int) strlen(*ppszReturn+nRetLen) + 32 + nRetLen);
        
        if( i > 0 )
            strcat( *ppszReturn + nRetLen, "," );

        if( poPoint->getCoordinateDimension() == 3 )
            strcat( *ppszReturn + nRetLen,
                    OGRMakeWktCoordinate( poPoint->getX(), 
                                          poPoint->getY(),
                                          poPoint->getZ() ));
        else
            strcat( *ppszReturn + nRetLen,
                    OGRMakeWktCoordinate( poPoint->getX(),
                                          poPoint->getY(),
                                          0.0 ) );

        nRetLen += strlen(*ppszReturn + nRetLen);
    }

    strcat( *ppszReturn+nRetLen, ")" );

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
    OGRErr	eErr = OGRERR_NONE;

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
/*      Read the point list which should consist of exactly one point.  */
/* -------------------------------------------------------------------- */
    int                 nMaxPoint = 0;
    int			nPointCount = 0;
    OGRRawPoint		*paoPoints = NULL;
    double		*padfZ = NULL;

    pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoint,
                                 &nPointCount );
    if( pszInput == NULL )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Transform raw points into point objects.                        */
/* -------------------------------------------------------------------- */
    for( iGeom = 0; iGeom < nPointCount && eErr == OGRERR_NONE; iGeom++ )
    {
        OGRGeometry	*poGeom;
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


