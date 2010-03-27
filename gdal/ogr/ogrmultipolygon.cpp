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
 ****************************************************************************/

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
        /* Test for old-style MULTIPOLYGON(EMPTY) */
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
        if( EQUAL(szToken, "EMPTY") )
        {
            eErr = addGeometryDirectly( poPolygon );
            if( eErr != OGRERR_NONE )
                return eErr;

            pszInput = OGRWktReadToken( pszInput, szToken );
            if ( !EQUAL(szToken, ",") )
                break;

            continue;
        }
        else if( szToken[0] != '(' )
        {
            eErr = OGRERR_CORRUPT_DATA;
            delete poPolygon;
            break;
        }

/* -------------------------------------------------------------------- */
/*      Loop over each ring in this polygon.                            */
/* -------------------------------------------------------------------- */
        do
        {
            int     nPoints = 0;

            const char* pszNext = OGRWktReadToken( pszInput, szToken );
            if (EQUAL(szToken,"EMPTY"))
            {
                poPolygon->addRingDirectly( new OGRLinearRing() );

                pszInput = OGRWktReadToken( pszNext, szToken );
                if ( !EQUAL(szToken, ",") )
                    break;

                continue;
            }

/* -------------------------------------------------------------------- */
/*      Read points for one line from input.                            */
/* -------------------------------------------------------------------- */
            pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoints,
                                         &nPoints );

            if( pszInput == NULL || nPoints == 0 )
            {
                eErr = OGRERR_CORRUPT_DATA;
                break;
            }
        
/* -------------------------------------------------------------------- */
/*      Create the new line, and add to collection.                     */
/* -------------------------------------------------------------------- */
            OGRLinearRing       *poLine;

            poLine = new OGRLinearRing();
            /* Ignore Z array when we have a MULTIPOLYGON M */
            if (bHasM && !bHasZ)
                poLine->setPoints( nPoints, paoPoints, NULL );
            else
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
        else
            delete poPolygon;

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
    char        **papszPolygons;
    int         iPoly, nCumulativeLength = 0, nValidPolys=0;
    OGRErr      eErr;
    int         bMustWriteComma = FALSE;

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each ring.     */
/* -------------------------------------------------------------------- */
    papszPolygons = (char **) CPLCalloc(sizeof(char *),getNumGeometries());

    for( iPoly = 0; iPoly < getNumGeometries(); iPoly++ )
    {
        eErr = getGeometryRef(iPoly)->exportToWkt( &(papszPolygons[iPoly]) );
        if( eErr != OGRERR_NONE )
            goto error;

        if( !EQUALN(papszPolygons[iPoly],"POLYGON (", 9) )
        {
            CPLDebug( "OGR", "OGRMultiPolygon::exportToWkt() - skipping %s.",
                      papszPolygons[iPoly] );
            CPLFree( papszPolygons[iPoly] );
            papszPolygons[iPoly] = NULL;
            continue;
        }
        
        nCumulativeLength += strlen(papszPolygons[iPoly] + 8);
        nValidPolys++;
    }
    
/* -------------------------------------------------------------------- */
/*      Return MULTIPOLYGON EMPTY if we get no valid polygons.          */
/* -------------------------------------------------------------------- */
    if( nValidPolys == 0 )
    {
        CPLFree( papszPolygons );
        *ppszDstText = CPLStrdup("MULTIPOLYGON EMPTY");
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Allocate exactly the right amount of space for the              */
/*      aggregated string.                                              */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSIMalloc(nCumulativeLength+getNumGeometries()+20);

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszDstText, "MULTIPOLYGON (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( iPoly = 0; iPoly < getNumGeometries(); iPoly++ )
    {                                                           
        if( papszPolygons[iPoly] == NULL )
            continue;

        if( bMustWriteComma )
            (*ppszDstText)[nCumulativeLength++] = ',';
        bMustWriteComma = TRUE;
        
        int nPolyLength = strlen(papszPolygons[iPoly] + 8);
        memcpy( *ppszDstText + nCumulativeLength, papszPolygons[iPoly] + 8, nPolyLength );
        nCumulativeLength += nPolyLength;
        VSIFree( papszPolygons[iPoly] );
    }

    (*ppszDstText)[nCumulativeLength++] = ')';
    (*ppszDstText)[nCumulativeLength] = '\0';

    CPLFree( papszPolygons );

    return OGRERR_NONE;

error:
    for( iPoly = 0; iPoly < getNumGeometries(); iPoly++ )
        CPLFree( papszPolygons[iPoly] );
    CPLFree( papszPolygons );
    return eErr;
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

