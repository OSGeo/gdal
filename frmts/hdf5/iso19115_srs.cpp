/******************************************************************************
 * $Id$
 *
 * Project:  BAG Driver
 * Purpose:  Implements code to parse ISO 19115 metadata to extract a
 *           spatial reference system.  Eventually intended to be made
 *           a method on OGRSpatialReference.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_spatialref.h"
#include "cpl_minixml.h"
#include "cpl_error.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                     OGR_SRS_ImportFromISO19115()                     */
/************************************************************************/

OGRErr OGR_SRS_ImportFromISO19115( OGRSpatialReference *poThis, 
                                   const char *pszISOXML )

{
/* -------------------------------------------------------------------- */
/*      Parse the XML into tree form.                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot = CPLParseXMLString( pszISOXML );

    if( psRoot == NULL )
        return OGRERR_FAILURE;

    CPLStripXMLNamespace( psRoot, NULL, TRUE );


/* -------------------------------------------------------------------- */
/*      For now we look for projection codes recognised in the BAG      */
/*      format (see ons_fsd.pdf: Metadata Dataset Character String      */
/*      Constants).                                                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRSI = CPLSearchXMLNode( psRoot, "=referenceSystemInfo" );
    if( psRSI == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find <referenceSystemInfo> in metadata." );
        CPLDestroyXMLNode( psRoot );
        return OGRERR_FAILURE;
    }

    poThis->Clear();

/* -------------------------------------------------------------------- */
/*      First, set the datum.                                           */
/* -------------------------------------------------------------------- */
    const char *pszDatum = 
        CPLGetXMLValue( psRSI, "MD_CRS.datum.RS_Identifier.code", "" );
    
    if( strlen(pszDatum) > 0 
        && poThis->SetWellKnownGeogCS( pszDatum ) != OGRERR_NONE )
    {
        CPLDestroyXMLNode( psRoot );
        return OGRERR_FAILURE;
    }
    
/* -------------------------------------------------------------------- */
/*      Then try to extract the projection.                             */
/* -------------------------------------------------------------------- */
    const char *pszProjection = 
        CPLGetXMLValue( psRSI, "MD_CRS.projection.RS_Identifier.code", "" );

    if( EQUAL(pszProjection,"UTM") )
    {
        int nZone = atoi(CPLGetXMLValue( psRSI, "MD_CRS.projectionParameters.MD_ProjectionParameters.zone", "0" ));

        poThis->SetUTM( ABS(nZone), nZone > 0 );
    }
    else 
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "projection = %s not recognised by ISO 19115 parser.",
                  pszProjection );
        CPLDestroyXMLNode( psRoot );
        return OGRERR_FAILURE;
    }
    
    CPLDestroyXMLNode( psRoot );

    return OGRERR_NONE;
}

