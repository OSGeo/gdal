/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  GDALJP2Metadata - Read GeoTIFF and/or GML georef info.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.1  2005/05/03 21:10:59  fwarmerdam
 * New
 *
 */

#include "gdaljp2metadata.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "ogr_spatialref.h"
#include "ogr_geometry.h"
#include "ogr_api.h"

CPL_CVSID("$Id$");

static unsigned char msi_uuid2[16] =
{0xb1,0x4b,0xf8,0xbd,0x08,0x3d,0x4b,0x43,
 0xa5,0xae,0x8c,0xd7,0xd5,0xa6,0xce,0x03}; 

CPL_C_START
CPLErr CPL_DLL GTIFMemBufFromWkt( const char *pszWKT, 
                                  const double *padfGeoTransform,
                                  int nGCPCount, const GDAL_GCP *pasGCPList,
                                  int *pnSize, unsigned char **ppabyBuffer );
CPLErr CPL_DLL GTIFWktFromMemBuf( int nSize, unsigned char *pabyBuffer, 
                          char **ppszWKT, double *padfGeoTransform,
                          int *pnGCPCount, GDAL_GCP **ppasGCPList );
CPL_C_END

/************************************************************************/
/*                          GDALJP2Metadata()                           */
/************************************************************************/

GDALJP2Metadata::GDALJP2Metadata()

{
    bHaveGeoTransform = FALSE;
    pszProjection = NULL;

    nGCPCount = 0;
    pasGCPList = NULL;

    papszGMLMetadata = NULL;

    nGeoTIFFSize = 0;
    pabyGeoTIFFData = NULL;

    
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                          ~GDALJP2Metadata()                          */
/************************************************************************/

GDALJP2Metadata::~GDALJP2Metadata()

{
    CPLFree( pszProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CPLFree( pabyGeoTIFFData );
}

/************************************************************************/
/*                           CollectGMLData()                           */
/*                                                                      */
/*      Read all the asoc boxes after this node, and store the          */
/*      contain xml documents along with the name from the label.       */
/************************************************************************/

void GDALJP2Metadata::CollectGMLData( GDALJP2Box *poGMLData )

{
    GDALJP2Box oChildBox( poGMLData->GetFILE() );

    oChildBox.ReadFirstChild( poGMLData );

    while( strlen(oChildBox.GetType()) > 0 )
    {
        if( EQUAL(oChildBox.GetType(),"asoc") )
        {
            GDALJP2Box oSubChildBox( oChildBox.GetFILE() );

            char *pszLabel = NULL;
            char *pszXML = NULL;

            oSubChildBox.ReadFirstChild( &oChildBox );
            
            while( strlen(oSubChildBox.GetType()) > 0 )
            {
                if( EQUAL(oSubChildBox.GetType(),"lbl ") )
                    pszLabel = (char *)oSubChildBox.ReadBoxData();
                else if( EQUAL(oSubChildBox.GetType(),"xml ") )
                    pszXML = (char *) oSubChildBox.ReadBoxData();

                oSubChildBox.ReadNextChild( &oChildBox );
            }
            
            if( pszLabel != NULL && pszXML != NULL )
                papszGMLMetadata = CSLSetNameValue( papszGMLMetadata, 
                                                    pszLabel, pszXML );
            CPLFree( pszLabel );
            CPLFree( pszXML );
        }
        
        oChildBox.ReadNextChild( poGMLData );
    }
}

/************************************************************************/
/*                             ReadBoxes()                              */
/************************************************************************/

int GDALJP2Metadata::ReadBoxes( FILE *fpVSIL )

{
    GDALJP2Box oBox( fpVSIL );

    oBox.ReadFirst(); 

    while( strlen(oBox.GetType()) > 0 )
    {
        if( EQUAL(oBox.GetType(),"uuid") 
            && memcmp( oBox.GetUUID(), msi_uuid2, 16 ) == 0 )
        {
            nGeoTIFFSize = oBox.GetDataLength();
            pabyGeoTIFFData = oBox.ReadBoxData();
        }

        if( EQUAL(oBox.GetType(),"asoc") )
        {
            GDALJP2Box oSubBox( fpVSIL );

            oSubBox.ReadFirstChild( &oBox );
            if( EQUAL(oSubBox.GetType(),"lbl ") )
            {
                char *pszLabel = (char *) oSubBox.ReadBoxData();
                if( EQUAL(pszLabel,"gml.data") )
                {
                    CollectGMLData( &oBox );
                }
                CPLFree( pszLabel );
            }
        }

        oBox.ReadNext();
    }
    
    return TRUE;
}


/************************************************************************/
/*                          ParseJP2GeoTIFF()                           */
/************************************************************************/

int GDALJP2Metadata::ParseJP2GeoTIFF()

{
    if( nGeoTIFFSize < 1 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Convert raw data into projection and geotransform.              */
/* -------------------------------------------------------------------- */
    int bSuccess = TRUE;

    if( GTIFWktFromMemBuf( nGeoTIFFSize, pabyGeoTIFFData,
                           &pszProjection, adfGeoTransform,
                           &nGCPCount, &pasGCPList ) != CE_None )
    {
        bSuccess = FALSE;
    }

    if( pszProjection == NULL || strlen(pszProjection) == 0 )
        bSuccess = FALSE;

    if( bSuccess )
        CPLDebug( "GDALJP2Metadata", 
                  "Got projection from GeoJP2 (geotiff) box: %s", 
                 pszProjection );

    return bSuccess;;
}

/************************************************************************/
/*                        ParseGMLCoverageDesc()                        */
/************************************************************************/

int GDALJP2Metadata::ParseGMLCoverageDesc() 

{
/* -------------------------------------------------------------------- */
/*      Do we have an XML doc that is apparently a coverage             */
/*      description?                                                    */
/* -------------------------------------------------------------------- */
    const char *pszCoverage = CSLFetchNameValue( papszGMLMetadata, 
                                                 "gml.root-instance" );

    if( pszCoverage == NULL )
        return FALSE;

    CPLDebug( "GDALJP2Metadata", "Found GML Box:\n%s", pszCoverage );

/* -------------------------------------------------------------------- */
/*      Try parsing the XML.  Wipe any namespace prefixes.              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psXML = CPLParseXMLString( pszCoverage );

    if( psXML == NULL )
        return FALSE;

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
    if( psOriginPoint == NULL || pszOffset1 == NULL || pszOffset2 == NULL )
    {
        CPLDestroyXMLNode( psXML );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract origin location.                                        */
/* -------------------------------------------------------------------- */
    OGRPoint *poOriginGeometry = NULL;
    const char *pszSRSName = NULL;

    if( psOriginPoint != NULL )
    {
        poOriginGeometry = (OGRPoint *) 
            OGR_G_CreateFromGMLTree( psOriginPoint );

        if( poOriginGeometry != NULL 
            && wkbFlatten(poOriginGeometry->getGeometryType()) != wkbPoint )
        {
            delete poOriginGeometry;
            poOriginGeometry = NULL;
        }

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
        adfGeoTransform[0] = poOriginGeometry->getX();
        adfGeoTransform[1] = atof(papszOffset1Tokens[0]);
        adfGeoTransform[2] = atof(papszOffset1Tokens[1]);
        adfGeoTransform[3] = poOriginGeometry->getY();
        adfGeoTransform[4] = atof(papszOffset2Tokens[0]);
        adfGeoTransform[5] = atof(papszOffset2Tokens[1]);
        bSuccess = TRUE;
    }

    CSLDestroy( papszOffset1Tokens );
    CSLDestroy( papszOffset2Tokens );

    if( poOriginGeometry != NULL )
        delete poOriginGeometry;

/* -------------------------------------------------------------------- */
/*      If we have gotten a geotransform, then try to interprete the    */
/*      srsName.                                                        */
/* -------------------------------------------------------------------- */
    if( bSuccess && pszSRSName != NULL && pszProjection == NULL )
    {
        if( EQUALN(pszSRSName,"epsg:",5) )
        {
            OGRSpatialReference oSRS;
            if( oSRS.SetFromUserInput( pszSRSName ) == OGRERR_NONE )
                oSRS.exportToWkt( &pszProjection );
        }
        else if( EQUALN(pszSRSName,"urn:ogc:def:crs:EPSG::",22) )
        {
            OGRSpatialReference oSRS;
            if( oSRS.importFromEPSG( atoi(pszSRSName + 22) ) == OGRERR_NONE )
                oSRS.exportToWkt( &pszProjection );
        }
        else if( EQUALN(pszSRSName,"urn:ogc:def:crs:EPSG:",21) )
        {
            const char *pszCode = pszSRSName+21;
            while( *pszCode != ':' && *pszCode != '\0' )
                pszCode++;

            OGRSpatialReference oSRS;
            if( oSRS.importFromEPSG( atoi(pszCode+1) ) == OGRERR_NONE )
                oSRS.exportToWkt( &pszProjection );
        }
        else
            CPLDebug( "GDALJP2Metadata", "Unable to evaluate SRSName=%s", 
                      pszSRSName );
    }

    if( pszProjection )
        CPLDebug( "GDALJP2Metadata", 
                  "Got projection from GML box: %s", 
                 pszProjection );

    return pszProjection != NULL && bSuccess;
}

