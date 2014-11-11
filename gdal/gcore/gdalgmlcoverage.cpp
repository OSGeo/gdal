/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  Generic support for GML Coverage descriptions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gdal_priv.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "ogr_spatialref.h"
#include "ogr_geometry.h"
#include "ogr_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        ParseGMLCoverageDesc()                        */
/************************************************************************/

CPLErr GDALParseGMLCoverage( CPLXMLNode *psXML, 
                             int *pnXSize, int *pnYSize,
                             double *padfGeoTransform,
                             char **ppszProjection )

{
    CPLStripXMLNamespace( psXML, NULL, TRUE );

/* -------------------------------------------------------------------- */
/*      Isolate RectifiedGrid.  Eventually we will need to support      */
/*      other georeferencing objects.                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRG = CPLSearchXMLNode( psXML, "=RectifiedGrid" );
    CPLXMLNode *psOriginPoint = NULL;
    const char *pszOffset1=NULL, *pszOffset2=NULL;

    if( psRG != NULL )
    {
        psOriginPoint = CPLGetXMLNode( psRG, "origin.Point" );
        if( psOriginPoint == NULL )
            psOriginPoint = CPLGetXMLNode( psRG, "origin" );

        CPLXMLNode *psOffset1 = CPLGetXMLNode( psRG, "offsetVector" );
        if( psOffset1 != NULL )
        {
            pszOffset1 = CPLGetXMLValue( psOffset1, "", NULL );
            pszOffset2 = CPLGetXMLValue( psOffset1->psNext, "=offsetVector", 
                                         NULL );
        }
    }

/* -------------------------------------------------------------------- */
/*      If we are missing any of the origin or 2 offsets then give up.  */
/* -------------------------------------------------------------------- */
    if( psRG == NULL || psOriginPoint == NULL 
        || pszOffset1 == NULL || pszOffset2 == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find GML RectifiedGrid, origin or offset vectors");
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Search for the GridEnvelope and derive the raster size.         */
/* -------------------------------------------------------------------- */
    char **papszLow = CSLTokenizeString(
        CPLGetXMLValue( psRG, "limits.GridEnvelope.low", ""));
    char **papszHigh = CSLTokenizeString(
        CPLGetXMLValue( psRG, "limits.GridEnvelope.high",""));

    if( CSLCount(papszLow) < 2 || CSLCount(papszHigh) < 2 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to find or parse GridEnvelope.low/high." );
        return CE_Failure;
    }        

    if( pnXSize != NULL )
        *pnXSize = atoi(papszHigh[0]) - atoi(papszLow[0]) + 1;
    if( pnYSize != NULL )
        *pnYSize = atoi(papszHigh[1]) - atoi(papszLow[1]) + 1;

    CSLDestroy( papszLow );
    CSLDestroy( papszHigh );

/* -------------------------------------------------------------------- */
/*      Extract origin location.                                        */
/* -------------------------------------------------------------------- */
    OGRPoint *poOriginGeometry = NULL;
    const char *pszSRSName = NULL;

    if( psOriginPoint != NULL )
    {
        int bOldWrap = FALSE;

        // old coverages (ie. WCS) just have <pos> under <origin> so we
        // may need to temporarily force <origin> to <Point>
        if( psOriginPoint->eType == CXT_Element
            && EQUAL(psOriginPoint->pszValue,"origin") )
        {
            strcpy( psOriginPoint->pszValue, "Point");
            bOldWrap = TRUE;
        }
        poOriginGeometry = (OGRPoint *) 
            OGR_G_CreateFromGMLTree( psOriginPoint );

        if( poOriginGeometry != NULL 
            && wkbFlatten(poOriginGeometry->getGeometryType()) != wkbPoint )
        {
            delete poOriginGeometry;
            poOriginGeometry = NULL;
        }

        if( bOldWrap )
            strcpy( psOriginPoint->pszValue, "origin");

        // SRS?
        pszSRSName = CPLGetXMLValue( psOriginPoint, "srsName", NULL );
    }

/* -------------------------------------------------------------------- */
/*      Extract offset(s)                                               */
/* -------------------------------------------------------------------- */
    char **papszOffset1Tokens = NULL;
    char **papszOffset2Tokens = NULL;
    int bSuccess = FALSE;

    papszOffset1Tokens = 
        CSLTokenizeStringComplex( pszOffset1, " ,", FALSE, FALSE );
    papszOffset2Tokens = 
        CSLTokenizeStringComplex( pszOffset2, " ,", FALSE, FALSE );

    if( CSLCount(papszOffset1Tokens) >= 2
        && CSLCount(papszOffset2Tokens) >= 2
        && poOriginGeometry != NULL )
    {
        padfGeoTransform[0] = poOriginGeometry->getX();
        padfGeoTransform[1] = CPLAtof(papszOffset1Tokens[0]);
        padfGeoTransform[2] = CPLAtof(papszOffset1Tokens[1]);
        padfGeoTransform[3] = poOriginGeometry->getY();
        padfGeoTransform[4] = CPLAtof(papszOffset2Tokens[0]);
        padfGeoTransform[5] = CPLAtof(papszOffset2Tokens[1]);

        // offset from center of pixel.
        padfGeoTransform[0] -= padfGeoTransform[1]*0.5;
        padfGeoTransform[0] -= padfGeoTransform[2]*0.5;
        padfGeoTransform[3] -= padfGeoTransform[4]*0.5;
        padfGeoTransform[3] -= padfGeoTransform[5]*0.5;

        bSuccess = TRUE;
        //bHaveGeoTransform = TRUE;
    }

    CSLDestroy( papszOffset1Tokens );
    CSLDestroy( papszOffset2Tokens );

    if( poOriginGeometry != NULL )
        delete poOriginGeometry;

/* -------------------------------------------------------------------- */
/*      If we have gotten a geotransform, then try to interprete the    */
/*      srsName.                                                        */
/* -------------------------------------------------------------------- */
    if( bSuccess && pszSRSName != NULL 
        && (*ppszProjection == NULL || strlen(*ppszProjection) == 0) )
    {
        if( EQUALN(pszSRSName,"epsg:",5) )
        {
            OGRSpatialReference oSRS;
            if( oSRS.SetFromUserInput( pszSRSName ) == OGRERR_NONE )
                oSRS.exportToWkt( ppszProjection );
        }
        else if( EQUALN(pszSRSName,"urn:ogc:def:crs:",16) )
        {
            OGRSpatialReference oSRS;
            if( oSRS.importFromURN( pszSRSName ) == OGRERR_NONE )
                oSRS.exportToWkt( ppszProjection );
        }
        else
            *ppszProjection = CPLStrdup(pszSRSName);
    }

    if( *ppszProjection )
        CPLDebug( "GDALJP2Metadata", 
                  "Got projection from GML box: %s", 
                  *ppszProjection );

    return CE_None;
}

