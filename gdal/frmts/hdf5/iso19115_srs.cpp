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
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

/* used by bagdataset.cpp */
OGRErr OGR_SRS_ImportFromISO19115( OGRSpatialReference *poThis, 
                                   const char *pszISOXML );

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

        /*
        ** We have encountered files (#5152) that identify the southern
        ** hemisphere with a false northing of 10000000 value.  The existing
        ** code checked for negative zones but it isn't clear if any actual
        ** files use that. 
        */
        int bNorth =  nZone > 0;
        if( bNorth )
        {
            const char *pszFalseNorthing = CPLGetXMLValue( psRSI, "MD_CRS.projectionParameters.MD_ProjectionParameters.falseNorthing", "" );
            if ( strlen(pszFalseNorthing) > 0 )
            {
                if( EQUAL(pszFalseNorthing, "10000000"))
                {
                    bNorth = FALSE;
                }
                else
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "falseNorthing value not recognized: %s", pszFalseNorthing);
                }
            }
        }
        poThis->SetUTM( ABS(nZone), bNorth );
    }
    else if( EQUAL(pszProjection,"Geodetic") )
    {
        const char *pszEllipsoid = 
            CPLGetXMLValue( psRSI, "MD_CRS.ellipsoid.RS_Identifier.code", "" );

        if( !EQUAL(pszDatum, "WGS84") ||
            !EQUAL(pszEllipsoid, "WGS84") )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "ISO 19115 parser does not support custom GCS." );
            CPLDestroyXMLNode( psRoot );
            return OGRERR_FAILURE;
        }
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

