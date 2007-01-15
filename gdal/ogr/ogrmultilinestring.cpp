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
/*      The next character should be a ( indicating the start of the    */
/*      list of linestrings.                                            */
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
        OGRLineString   *poLine;

        poLine = new OGRLineString();
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
    int         iLine, nCumulativeLength = 0;
    OGRErr      eErr;

    if( getNumGeometries() == 0 )
    {
        *ppszDstText = CPLStrdup("MULTILINESTRING EMPTY");
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each ring.     */
/* -------------------------------------------------------------------- */
    papszLines = (char **) CPLCalloc(sizeof(char *),getNumGeometries());

    for( iLine = 0; iLine < getNumGeometries(); iLine++ )
    {
        eErr = getGeometryRef(iLine)->exportToWkt( &(papszLines[iLine]) );
        if( eErr != OGRERR_NONE )
            return eErr;

        CPLAssert( EQUALN(papszLines[iLine],"LINESTRING (", 12) );
        nCumulativeLength += strlen(papszLines[iLine] + 11);
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

    for( iLine = 0; iLine < getNumGeometries(); iLine++ )
    {                                                           
        if( iLine > 0 )
            strcat( pszAppendPoint, "," );
        
        strcat( pszAppendPoint, papszLines[iLine] + 11 );
        pszAppendPoint += strlen(pszAppendPoint);

        VSIFree( papszLines[iLine] );
    }

    strcat( pszAppendPoint, ")" );

    CPLFree( papszLines );

    return OGRERR_NONE;
}

