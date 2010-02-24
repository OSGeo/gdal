/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiLineString class.
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
/*                        OGRMultiLineString()                          */
/************************************************************************/

OGRMultiLineString::OGRMultiLineString()
{
}

/************************************************************************/
/*                        ~OGRMultiLineString()                         */
/************************************************************************/

OGRMultiLineString::~OGRMultiLineString()
{
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiLineString::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbMultiLineString25D;
    else
        return wkbMultiLineString;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiLineString::getGeometryName() const

{
    return "MULTILINESTRING";
}

/************************************************************************/
/*                        addGeometryDirectly()                         */
/************************************************************************/

OGRErr OGRMultiLineString::addGeometryDirectly( OGRGeometry * poNewGeom )

{
    if( poNewGeom->getGeometryType() != wkbLineString 
        && poNewGeom->getGeometryType() != wkbLineString25D ) 
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    return OGRGeometryCollection::addGeometryDirectly( poNewGeom );
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRMultiLineString::clone() const

{
    OGRMultiLineString  *poNewGC;

    poNewGC = new OGRMultiLineString;
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
/*      `MULTILINESTRING ((x y, x y, ...),(x y, ...),...)'.             */
/************************************************************************/

OGRErr OGRMultiLineString::importFromWkt( char ** ppszInput )

{
    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;
    OGRErr      eErr;

/* -------------------------------------------------------------------- */
/*      Clear existing rings.                                           */
/* -------------------------------------------------------------------- */
    empty();

/* -------------------------------------------------------------------- */
/*      Read and verify the ``MULTILINESTRING'' keyword token.          */
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
        /* Test for old-style MULTILINESTRING(EMPTY) */
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
/*      Read each line in turn.  Note that we try to reuse the same     */
/*      point list buffer from ring to ring to cut down on              */
/*      allocate/deallocate overhead.                                   */
/* ==================================================================== */
    OGRRawPoint *paoPoints = NULL;
    int         nMaxPoints = 0;
    double      *padfZ = NULL;
    
    do
    {
        int     nPoints = 0;

        const char* pszNext = OGRWktReadToken( pszInput, szToken );
        if (EQUAL(szToken,"EMPTY"))
        {
            eErr = addGeometryDirectly( new OGRLineString() );
            if( eErr != OGRERR_NONE )
                return eErr;

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
        OGRLineString   *poLine;

        poLine = new OGRLineString();
        /* Ignore Z array when we have a MULTILINESTRING M */
        if (bHasM && !bHasZ)
            poLine->setPoints( nPoints, paoPoints, NULL );
        else
            poLine->setPoints( nPoints, paoPoints, padfZ );

        eErr = addGeometryDirectly( poLine ); 

/* -------------------------------------------------------------------- */
/*      Read the delimeter following the ring.                          */
/* -------------------------------------------------------------------- */
        
        pszInput = OGRWktReadToken( pszInput, szToken );
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

OGRErr OGRMultiLineString::exportToWkt( char ** ppszDstText ) const

{
    char        **papszLines;
    int         iLine, nCumulativeLength = 0, nValidLineStrings=0;
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

        if( !EQUALN(papszLines[iLine],"LINESTRING (", 12) )
        {
            CPLDebug( "OGR", "OGRMultiLineString::exportToWkt() - skipping %s.",
                      papszLines[iLine] );
            CPLFree( papszLines[iLine] );
            papszLines[iLine] = NULL;
            continue;
        }

        nCumulativeLength += strlen(papszLines[iLine] + 11);
        nValidLineStrings++;
    }
    
/* -------------------------------------------------------------------- */
/*      Return MULTILINESTRING EMPTY if we get no valid line string.    */
/* -------------------------------------------------------------------- */
    if( nValidLineStrings == 0 )
    {
        CPLFree( papszLines );
        *ppszDstText = CPLStrdup("MULTILINESTRING EMPTY");
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
    char *pszAppendPoint = *ppszDstText;

    strcpy( pszAppendPoint, "MULTILINESTRING (" );

    int bMustWriteComma = FALSE;
    for( iLine = 0; iLine < getNumGeometries(); iLine++ )
    {                                                           
        if( papszLines[iLine] == NULL )
            continue;

        if( bMustWriteComma )
            strcat( pszAppendPoint, "," );
        bMustWriteComma = TRUE;
        
        strcat( pszAppendPoint, papszLines[iLine] + 11 );
        pszAppendPoint += strlen(pszAppendPoint);

        VSIFree( papszLines[iLine] );
    }

    strcat( pszAppendPoint, ")" );

    CPLFree( papszLines );

    return OGRERR_NONE;
}

