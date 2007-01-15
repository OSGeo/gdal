/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiPolygon class.
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
 * Revision 1.16  2006/07/07 15:09:12  fwarmerdam
 * improve handling of empty linearrings and polygons in aggregates
 *
 * Revision 1.15  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.14  2005/07/12 17:34:00  fwarmerdam
 * updated to produce proper empty syntax and consume either
 *
 * Revision 1.13  2004/09/17 15:05:36  fwarmerdam
 * added get_Area() support
 *
 * Revision 1.12  2004/02/21 15:36:14  warmerda
 * const correctness updates for geometry: bug 289
 *
 * Revision 1.11  2004/01/16 21:57:17  warmerda
 * fixed up EMPTY support
 *
 * Revision 1.10  2004/01/16 21:20:00  warmerda
 * Added EMPTY support
 *
 * Revision 1.9  2003/09/11 22:47:54  aamici
 * add class constructors and destructors where needed in order to
 * let the mingw/cygwin binutils produce sensible partially linked objet files
 * with 'ld -r'.
 *
 * Revision 1.8  2003/05/28 19:16:43  warmerda
 * fixed up argument names and stuff for docs
 *
 * Revision 1.7  2002/09/11 13:47:17  warmerda
 * preliminary set of fixes for 3D WKB enum
 *
 * Revision 1.6  2001/07/19 18:25:07  warmerda
 * expanded tabs
 *
 * Revision 1.5  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
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
 * Revision 1.1  1999/05/23 05:34:36  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRMultiPolygon()                           */
/************************************************************************/

OGRMultiPolygon::OGRMultiPolygon()
{
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiPolygon::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbMultiPolygon25D;
    else
        return wkbMultiPolygon;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiPolygon::getGeometryName() const

{
    return "MULTIPOLYGON";
}

/************************************************************************/
/*                        addGeometryDirectly()                         */
/************************************************************************/

OGRErr OGRMultiPolygon::addGeometryDirectly( OGRGeometry * poNewGeom )

{
    if( poNewGeom->getGeometryType() != wkbPolygon 
        && poNewGeom->getGeometryType() != wkbPolygon25D )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    return OGRGeometryCollection::addGeometryDirectly( poNewGeom );
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRMultiPolygon::clone() const

{
    OGRMultiPolygon     *poNewGC;

    poNewGC = new OGRMultiPolygon;
    poNewGC->assignSpatialReference( getSpatialReference() );

    for( int i = 0; i < getNumGeometries(); i++ )
    {
        poNewGC->addGeometry( getGeometryRef(i) );
    }

    return poNewGC;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.  Currently this is     */
/*      `MULTIPOLYGON ((x y, x y, ...),(x y, ...),...)'.                */
/************************************************************************/

OGRErr OGRMultiPolygon::importFromWkt( char ** ppszInput )

{
    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;
    OGRErr      eErr = OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Clear existing rings.                                           */
/* -------------------------------------------------------------------- */
    empty();

/* -------------------------------------------------------------------- */
/*      Read and verify the MULTIPOLYGON keyword token.                 */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( !EQUAL(szToken,getGeometryName()) )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      The next character should be a ( indicating the start of the    */
/*      list of polygons.                                               */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( EQUAL(szToken,"EMPTY") )
    {
        *ppszInput = (char *) pszInput;
        return OGRERR_NONE;
    }

    if( szToken[0] != '(' )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      If the next token is EMPTY, then verify that we have proper     */
/*      EMPTY format will a trailing closing bracket.                   */
/* -------------------------------------------------------------------- */
    OGRWktReadToken( pszInput, szToken );
    if( EQUAL(szToken,"EMPTY") )
    {
        pszInput = OGRWktReadToken( pszInput, szToken );
        pszInput = OGRWktReadToken( pszInput, szToken );
        
        *ppszInput = (char *) pszInput;

        if( !EQUAL(szToken,")") )
            return OGRERR_CORRUPT_DATA;
        else
            return OGRERR_NONE;
    }

/* ==================================================================== */
/*      Read each polygon in turn.  Note that we try to reuse the same  */
/*      point list buffer from ring to ring to cut down on              */
/*      allocate/deallocate overhead.                                   */
/* ==================================================================== */
    OGRRawPoint *paoPoints = NULL;
    int         nMaxPoints = 0;
    double      *padfZ = NULL;
    
    do
    {
        OGRPolygon      *poPolygon = new OGRPolygon();

/* -------------------------------------------------------------------- */
/*      The next character should be a ( indicating the start of the    */
/*      list of polygons.                                               */
/* -------------------------------------------------------------------- */
        pszInput = OGRWktReadToken( pszInput, szToken );
        if( szToken[0] != '(' )
        {
            eErr = OGRERR_CORRUPT_DATA;
            break;
        }

/* -------------------------------------------------------------------- */
/*      Loop over each ring in this polygon.                            */
/* -------------------------------------------------------------------- */
        do
        {
            int     nPoints = 0;

/* -------------------------------------------------------------------- */
/*      Read points for one line from input.                            */
/* -------------------------------------------------------------------- */
            pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoints,
                                         &nPoints );

            if( pszInput == NULL )
            {
                eErr = OGRERR_CORRUPT_DATA;
                break;
            }
        
/* -------------------------------------------------------------------- */
/*      Create the new line, and add to collection.                     */
/* -------------------------------------------------------------------- */
            OGRLinearRing       *poLine;

            poLine = new OGRLinearRing();
            poLine->setPoints( nPoints, paoPoints, padfZ );

            poPolygon->addRingDirectly( poLine ); 

/* -------------------------------------------------------------------- */
/*      Read the delimeter following the ring.                          */
/* -------------------------------------------------------------------- */
        
            pszInput = OGRWktReadToken( pszInput, szToken );
        } while( szToken[0] == ',' && eErr == OGRERR_NONE );

/* -------------------------------------------------------------------- */
/*      Verify that we have a closing bracket.                          */
/* -------------------------------------------------------------------- */
        if( eErr == OGRERR_NONE )
        {
            if( szToken[0] != ')' )
                eErr = OGRERR_CORRUPT_DATA;
            else
                pszInput = OGRWktReadToken( pszInput, szToken );
        }
        
/* -------------------------------------------------------------------- */
/*      Add the polygon to the MULTIPOLYGON.                            */
/* -------------------------------------------------------------------- */
        if( eErr == OGRERR_NONE )
            eErr = addGeometryDirectly( poPolygon );

    } while( szToken[0] == ',' && eErr == OGRERR_NONE );

/* -------------------------------------------------------------------- */
/*      freak if we don't get a closing bracket.                        */
/* -------------------------------------------------------------------- */
    CPLFree( paoPoints );
    CPLFree( padfZ );
   
    if( eErr != OGRERR_NONE )
        return eErr;

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

OGRErr OGRMultiPolygon::exportToWkt( char ** ppszDstText ) const

{
    char        **papszLines;
    int         iLine, nCumulativeLength = 0, nValidPolys=0;
    OGRErr      eErr;

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each ring.     */
/* -------------------------------------------------------------------- */
    papszLines = (char **) CPLCalloc(sizeof(char *),getNumGeometries());

    for( iLine = 0; iLine < getNumGeometries(); iLine++ )
    {
        eErr = getGeometryRef(iLine)->exportToWkt( &(papszLines[iLine]) );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( !EQUALN(papszLines[iLine],"POLYGON (", 9) )
        {
            CPLDebug( "OGR", "OGRMultiPolygon::exportToWkt() - skipping %s.",
                      papszLines[iLine] );
            CPLFree( papszLines[iLine] );
            papszLines[iLine] = NULL;
            continue;
        }
        
        nCumulativeLength += strlen(papszLines[iLine] + 8);
        nValidPolys++;
    }
    
/* -------------------------------------------------------------------- */
/*      Return MULTIPOLYGON EMPTY if we get no valid polygons.          */
/* -------------------------------------------------------------------- */
    if( nValidPolys == 0 )
    {
        *ppszDstText = CPLStrdup("MULTIPOLYGON EMPTY");
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Allocate exactly the right amount of space for the              */
/*      aggregated string.                                              */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSIMalloc(nCumulativeLength+getNumGeometries()+20);

    if( *ppszDstText == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszDstText, "MULTIPOLYGON (" );

    for( iLine = 0; iLine < getNumGeometries(); iLine++ )
    {                                                           
        if( papszLines[iLine] == NULL )
            continue;

        if( iLine > 0 )
            strcat( *ppszDstText, "," );
        
        strcat( *ppszDstText, papszLines[iLine] + 8 );
        VSIFree( papszLines[iLine] );
    }

    strcat( *ppszDstText, ")" );

    CPLFree( papszLines );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              get_Area()                              */
/************************************************************************/

/**
 * Compute area of multipolygon.
 *
 * The area is computed as the sum of the areas of all polygon members
 * in this collection.
 *
 * @return computed area.
 */

double OGRMultiPolygon::get_Area() const

{
    double dfArea = 0.0;
    int iPoly;

    for( iPoly = 0; iPoly < getNumGeometries(); iPoly++ )
    {
        OGRPolygon *poPoly = (OGRPolygon *) getGeometryRef( iPoly );

        dfArea += poPoly->get_Area();
    }

    return dfArea;
}

