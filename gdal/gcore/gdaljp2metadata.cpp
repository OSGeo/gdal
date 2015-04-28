/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  GDALJP2Metadata - Read GeoTIFF and/or GML georef info.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2015, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2015, European Union Satellite Centre
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

#include "gdaljp2metadata.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "ogr_spatialref.h"
#include "ogr_geometry.h"
#include "ogr_api.h"
#include "gt_wkt_srs_for_gdal.h"
#include "json.h"

CPL_CVSID("$Id$");

static const unsigned char msi_uuid2[16] =
{0xb1,0x4b,0xf8,0xbd,0x08,0x3d,0x4b,0x43,
 0xa5,0xae,0x8c,0xd7,0xd5,0xa6,0xce,0x03}; 

static const unsigned char msig_uuid[16] = 
{ 0x96,0xA9,0xF1,0xF1,0xDC,0x98,0x40,0x2D,
  0xA7,0xAE,0xD6,0x8E,0x34,0x45,0x18,0x09 };

static const unsigned char xmp_uuid[16] =
{ 0xBE,0x7A,0xCF,0xCB,0x97,0xA9,0x42,0xE8,
  0x9C,0x71,0x99,0x94,0x91,0xE3,0xAF,0xAC};

struct _GDALJP2GeoTIFFBox
{
    int    nGeoTIFFSize;
    GByte  *pabyGeoTIFFData;
};

#define MAX_JP2GEOTIFF_BOXES 2

/************************************************************************/
/*                          GDALJP2Metadata()                           */
/************************************************************************/

GDALJP2Metadata::GDALJP2Metadata()

{
    pszProjection = NULL;

    nGCPCount = 0;
    pasGCPList = NULL;

    papszGMLMetadata = NULL;
    papszMetadata = NULL;

    nGeoTIFFBoxesCount = 0;
    pasGeoTIFFBoxes = NULL;

    nMSIGSize = 0;
    pabyMSIGData = NULL;

    pszXMPMetadata = NULL;
    pszGDALMultiDomainMetadata = NULL;
    pszXMLIPR = NULL;

    bHaveGeoTransform = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    bPixelIsPoint = FALSE;
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

    for( int i=0; i < nGeoTIFFBoxesCount; i++ )
    {
        CPLFree( pasGeoTIFFBoxes[i].pabyGeoTIFFData );
    }
    CPLFree( pasGeoTIFFBoxes );
    CPLFree( pabyMSIGData );
    CSLDestroy( papszGMLMetadata );
    CSLDestroy( papszMetadata );
    CPLFree( pszXMPMetadata );
    CPLFree( pszGDALMultiDomainMetadata );
    CPLFree( pszXMLIPR );
}

/************************************************************************/
/*                            ReadAndParse()                            */
/*                                                                      */
/*      Read a JP2 file and try to collect georeferencing               */
/*      information from the various available forms.  Returns TRUE     */
/*      if anything useful is found.                                    */
/************************************************************************/

int GDALJP2Metadata::ReadAndParse( const char *pszFilename )

{
    VSILFILE *fpLL;
        
    fpLL = VSIFOpenL( pszFilename, "rb" );
        
    if( fpLL == NULL )
    {
        CPLDebug( "GDALJP2Metadata", "Could not even open %s.", 
                  pszFilename );

        return FALSE;
    }

    int bRet = ReadAndParse( fpLL );
    VSIFCloseL( fpLL );

/* -------------------------------------------------------------------- */
/*      If we still don't have a geotransform, look for a world         */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    if( !bHaveGeoTransform )
    {
        bHaveGeoTransform = 
            GDALReadWorldFile( pszFilename, NULL, adfGeoTransform )
            || GDALReadWorldFile( pszFilename, ".wld", adfGeoTransform );
        bRet |= bHaveGeoTransform;
    }

    return bRet;
}


int GDALJP2Metadata::ReadAndParse( VSILFILE *fpLL )

{
    ReadBoxes( fpLL );

/* -------------------------------------------------------------------- */
/*      Try JP2GeoTIFF, GML and finally MSIG to get something.          */
/* -------------------------------------------------------------------- */
    if( !ParseJP2GeoTIFF() && !ParseGMLCoverageDesc() )
        ParseMSIG();

/* -------------------------------------------------------------------- */
/*      Return success either either of projection or geotransform      */
/*      or gcps.                                                        */
/* -------------------------------------------------------------------- */
    return bHaveGeoTransform
        || nGCPCount > 0 
        || (pszProjection != NULL && strlen(pszProjection) > 0);
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

    if( !oChildBox.ReadFirstChild( poGMLData ) )
        return;

    while( strlen(oChildBox.GetType()) > 0 )
    {
        if( EQUAL(oChildBox.GetType(),"asoc") )
        {
            GDALJP2Box oSubChildBox( oChildBox.GetFILE() );

            char *pszLabel = NULL;
            char *pszXML = NULL;

            if( !oSubChildBox.ReadFirstChild( &oChildBox ) )
                break;
            
            while( strlen(oSubChildBox.GetType()) > 0 )
            {
                if( EQUAL(oSubChildBox.GetType(),"lbl ") )
                    pszLabel = (char *)oSubChildBox.ReadBoxData();
                else if( EQUAL(oSubChildBox.GetType(),"xml ") )
                {
                    pszXML = (char *) oSubChildBox.ReadBoxData();

                    // Some GML data contains \0 instead of \n !
                    // See http://trac.osgeo.org/gdal/ticket/5760
                    if( pszXML )
                    {
                        int nXMLLength = (int)oSubChildBox.GetDataLength();
                        int i;
                        for(i=nXMLLength-1; i >= 0; i--)
                        {
                            if( pszXML[i] == '\0' )
                                nXMLLength --;
                            else
                                break;
                        }
                        for(i=0;i<nXMLLength;i++)
                        {
                            if( pszXML[i] == '\0' )
                                break;
                        }
                        if( i < nXMLLength )
                        {
                            CPLPushErrorHandler(CPLQuietErrorHandler);
                            CPLXMLNode* psNode = CPLParseXMLString(pszXML);
                            CPLPopErrorHandler();
                            if( psNode == NULL )
                            {
                                CPLDebug("GMLJP2", "GMLJP2 data contains nul characters inside content. Replacing them by \\n");
                                for(int i=0;i<nXMLLength;i++)
                                {
                                    if( pszXML[i] == '\0' )
                                        pszXML[i] = '\n';
                                }
                            }
                            else
                                CPLDestroyXMLNode(psNode);
                        }
                    }
                }

                if( !oSubChildBox.ReadNextChild( &oChildBox ) )
                    break;
            }
            
            if( pszLabel != NULL && pszXML != NULL )
                papszGMLMetadata = CSLSetNameValue( papszGMLMetadata, 
                                                    pszLabel, pszXML );
            CPLFree( pszLabel );
            CPLFree( pszXML );
        }
        
        if( !oChildBox.ReadNextChild( poGMLData ) )
            break;
    }
}

/************************************************************************/
/*                             ReadBoxes()                              */
/************************************************************************/

int GDALJP2Metadata::ReadBoxes( VSILFILE *fpVSIL )

{
    GDALJP2Box oBox( fpVSIL );
    int iBox = 0;

    if (!oBox.ReadFirst())
        return FALSE;

    while( strlen(oBox.GetType()) > 0 )
    {
#ifdef DEBUG
        if (CSLTestBoolean(CPLGetConfigOption("DUMP_JP2_BOXES", "NO")))
            oBox.DumpReadable(stderr);
#endif

/* -------------------------------------------------------------------- */
/*      Collect geotiff box.                                            */
/* -------------------------------------------------------------------- */
        if( EQUAL(oBox.GetType(),"uuid") 
            && memcmp( oBox.GetUUID(), msi_uuid2, 16 ) == 0 )
        {
            /* Erdas JPEG2000 files can in some conditions contain 2 GeoTIFF */
            /* UUID boxes. One that is correct, another one that does not contain */
            /* correct georeferencing. So let's fetch at most 2 of them */
            /* for later analysis. */
            if( nGeoTIFFBoxesCount == MAX_JP2GEOTIFF_BOXES )
            {
                CPLDebug("GDALJP2", "Too many UUID GeoTIFF boxes. Ignoring this one");
            }
            else
            {
                int nGeoTIFFSize = (int) oBox.GetDataLength();
                GByte* pabyGeoTIFFData = oBox.ReadBoxData();
                if (pabyGeoTIFFData == NULL)
                {
                    CPLDebug("GDALJP2", "Cannot read data for UUID GeoTIFF box");
                }
                else
                {
                    pasGeoTIFFBoxes = (GDALJP2GeoTIFFBox*) CPLRealloc(
                        pasGeoTIFFBoxes, sizeof(GDALJP2GeoTIFFBox) * (nGeoTIFFBoxesCount + 1) );
                    pasGeoTIFFBoxes[nGeoTIFFBoxesCount].nGeoTIFFSize = nGeoTIFFSize;
                    pasGeoTIFFBoxes[nGeoTIFFBoxesCount].pabyGeoTIFFData = pabyGeoTIFFData;
                    nGeoTIFFBoxesCount ++;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Collect MSIG box.                                               */
/* -------------------------------------------------------------------- */
        if( EQUAL(oBox.GetType(),"uuid") 
            && memcmp( oBox.GetUUID(), msig_uuid, 16 ) == 0 )
        {
            if( nMSIGSize == 0 )
            {
                nMSIGSize = (int) oBox.GetDataLength();
                pabyMSIGData = oBox.ReadBoxData();

                if( nMSIGSize < 70
                    || pabyMSIGData == NULL
                    || memcmp( pabyMSIGData, "MSIG/", 5 ) != 0 )
                {
                    CPLFree( pabyMSIGData );
                    pabyMSIGData = NULL;
                    nMSIGSize = 0;
                }
            }
            else
            {
                CPLDebug("GDALJP2", "Too many UUID MSIG boxes. Ignoring this one");
            }
        }

/* -------------------------------------------------------------------- */
/*      Collect XMP box.                                                */
/* -------------------------------------------------------------------- */
        if( EQUAL(oBox.GetType(),"uuid")
            && memcmp( oBox.GetUUID(), xmp_uuid, 16 ) == 0 )
        {
            if( pszXMPMetadata == NULL )
            {
                pszXMPMetadata = (char*) oBox.ReadBoxData();
            }
            else
            {
                CPLDebug("GDALJP2", "Too many UUID XMP boxes. Ignoring this one");
            }
        }

/* -------------------------------------------------------------------- */
/*      Process asoc box looking for Labelled GML data.                 */
/* -------------------------------------------------------------------- */
        if( EQUAL(oBox.GetType(),"asoc") )
        {
            GDALJP2Box oSubBox( fpVSIL );

            if( oSubBox.ReadFirstChild( &oBox ) && 
                EQUAL(oSubBox.GetType(),"lbl ") )
            {
                char *pszLabel = (char *) oSubBox.ReadBoxData();
                if( pszLabel != NULL && EQUAL(pszLabel,"gml.data") )
                {
                    CollectGMLData( &oBox );
                }
                CPLFree( pszLabel );
            }
        }

/* -------------------------------------------------------------------- */
/*      Process simple xml boxes.                                       */
/* -------------------------------------------------------------------- */
        if( EQUAL(oBox.GetType(),"xml ") )
        {
            CPLString osBoxName;

            char *pszXML = (char *) oBox.ReadBoxData();
            if( strncmp(pszXML, "<GDALMultiDomainMetadata>",
                        strlen("<GDALMultiDomainMetadata>")) == 0 )
            {
                if( pszGDALMultiDomainMetadata == NULL )
                {
                    pszGDALMultiDomainMetadata = pszXML;
                    pszXML = NULL;
                }
                else
                {
                    CPLDebug("GDALJP2", "Too many GDAL metadata boxes. Ignoring this one");
                }
            }
            else
            {
                osBoxName.Printf( "BOX_%d", iBox++ );

                papszGMLMetadata = CSLSetNameValue( papszGMLMetadata, 
                                                    osBoxName, pszXML );
            }
            CPLFree( pszXML );
        }

/* -------------------------------------------------------------------- */
/*      Check for a resd box in jp2h.                                   */
/* -------------------------------------------------------------------- */
        if( EQUAL(oBox.GetType(),"jp2h") )
        {
            GDALJP2Box oSubBox( fpVSIL );

            for( oSubBox.ReadFirstChild( &oBox );
                 strlen(oSubBox.GetType()) > 0;
                 oSubBox.ReadNextChild( &oBox ) )
            {
                if( EQUAL(oSubBox.GetType(),"res ") )
                {
                    GDALJP2Box oResBox( fpVSIL );

                    oResBox.ReadFirstChild( &oSubBox );
                    
                    // we will use either the resd or resc box, which ever
                    // happens to be first.  Should we prefer resd?
                    unsigned char *pabyResData = NULL;
                    if( oResBox.GetDataLength() == 10 &&
                        (pabyResData = oResBox.ReadBoxData()) != NULL )
                    {
                        int nVertNum, nVertDen, nVertExp;
                        int nHorzNum, nHorzDen, nHorzExp;
                        
                        nVertNum = pabyResData[0] * 256 + pabyResData[1];
                        nVertDen = pabyResData[2] * 256 + pabyResData[3];
                        nHorzNum = pabyResData[4] * 256 + pabyResData[5];
                        nHorzDen = pabyResData[6] * 256 + pabyResData[7];
                        nVertExp = pabyResData[8];
                        nHorzExp = pabyResData[9];
                        
                        // compute in pixels/cm 
                        double dfVertRes = 
                            (nVertNum/(double)nVertDen) * pow(10.0,nVertExp)/100;
                        double dfHorzRes = 
                            (nHorzNum/(double)nHorzDen) * pow(10.0,nHorzExp)/100;
                        CPLString osFormatter;

                        papszMetadata = CSLSetNameValue( 
                            papszMetadata, 
                            "TIFFTAG_XRESOLUTION",
                            osFormatter.Printf("%g",dfHorzRes) );
                        
                        papszMetadata = CSLSetNameValue( 
                            papszMetadata, 
                            "TIFFTAG_YRESOLUTION",
                            osFormatter.Printf("%g",dfVertRes) );
                        papszMetadata = CSLSetNameValue( 
                            papszMetadata, 
                            "TIFFTAG_RESOLUTIONUNIT", 
                            "3 (pixels/cm)" );
                        
                        CPLFree( pabyResData );
                    }
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Collect IPR box.                                                */
/* -------------------------------------------------------------------- */
        if( EQUAL(oBox.GetType(),"jp2i") )
        {
            if( pszXMLIPR == NULL )
            {
                pszXMLIPR = (char*) oBox.ReadBoxData();
                CPLXMLNode* psNode = CPLParseXMLString(pszXMLIPR);
                if( psNode == NULL )
                {
                    CPLFree(pszXMLIPR);
                    pszXMLIPR = NULL;
                }
                else
                    CPLDestroyXMLNode(psNode);
            }
            else
            {
                CPLDebug("GDALJP2", "Too many IPR boxes. Ignoring this one");
            }
        }

        if (!oBox.ReadNext())
            break;
    }

    return TRUE;
}

/************************************************************************/
/*                          ParseJP2GeoTIFF()                           */
/************************************************************************/

int GDALJP2Metadata::ParseJP2GeoTIFF()

{
    if(! CSLTestBoolean(CPLGetConfigOption("GDAL_USE_GEOJP2", "TRUE")) )
        return FALSE;

    int abValidProjInfo[MAX_JP2GEOTIFF_BOXES] = { FALSE };
    char* apszProjection[MAX_JP2GEOTIFF_BOXES] = { NULL };
    double aadfGeoTransform[MAX_JP2GEOTIFF_BOXES][6];
    int anGCPCount[MAX_JP2GEOTIFF_BOXES] = { 0 };
    GDAL_GCP    *apasGCPList[MAX_JP2GEOTIFF_BOXES] = { NULL };
    int abPixelIsPoint[MAX_JP2GEOTIFF_BOXES] = { 0 };

    int i;
    int nMax = MIN(nGeoTIFFBoxesCount, MAX_JP2GEOTIFF_BOXES);
    for(i=0; i < nMax; i++)
    {
    /* -------------------------------------------------------------------- */
    /*      Convert raw data into projection and geotransform.              */
    /* -------------------------------------------------------------------- */
        aadfGeoTransform[i][0] = 0;
        aadfGeoTransform[i][1] = 1;
        aadfGeoTransform[i][2] = 0;
        aadfGeoTransform[i][3] = 0;
        aadfGeoTransform[i][4] = 0;
        aadfGeoTransform[i][5] = 1;
        if( GTIFWktFromMemBufEx( pasGeoTIFFBoxes[i].nGeoTIFFSize,
                               pasGeoTIFFBoxes[i].pabyGeoTIFFData,
                               &apszProjection[i], aadfGeoTransform[i],
                               &anGCPCount[i], &apasGCPList[i], &abPixelIsPoint[i] ) == CE_None )
        {
            if( apszProjection[i] != NULL && strlen(apszProjection[i]) != 0 ) 
                abValidProjInfo[i] = TRUE;
        }
    }

    /* Detect which box is the better one */
    int iBestIndex = -1;
    for(i=0; i < nMax; i++)
    {
        if( abValidProjInfo[i] && iBestIndex < 0 )
        {
            iBestIndex = i;
        }
        else if( abValidProjInfo[i] && apszProjection[i] != NULL )
        {
            /* Anything else than a LOCAL_CS will probably be better */
            if( EQUALN(apszProjection[iBestIndex], "LOCAL_CS", strlen("LOCAL_CS")) )
                iBestIndex = i;
        }
    }

    if( iBestIndex < 0 )
    {
        for(i=0; i < nMax; i++)
        {
            if( aadfGeoTransform[i][0] != 0 
                || aadfGeoTransform[i][1] != 1 
                || aadfGeoTransform[i][2] != 0
                || aadfGeoTransform[i][3] != 0 
                || aadfGeoTransform[i][4] != 0
                || aadfGeoTransform[i][5] != 1
                || anGCPCount[i] > 0)
            {
                iBestIndex = i;
            }
        }
    }

    if( iBestIndex >= 0 )
    {
        pszProjection = apszProjection[iBestIndex];
        memcpy(adfGeoTransform, aadfGeoTransform[iBestIndex], 6 * sizeof(double));
        nGCPCount = anGCPCount[iBestIndex];
        pasGCPList = apasGCPList[iBestIndex];
        bPixelIsPoint = abPixelIsPoint[iBestIndex];

        if( adfGeoTransform[0] != 0 
            || adfGeoTransform[1] != 1 
            || adfGeoTransform[2] != 0
            || adfGeoTransform[3] != 0 
            || adfGeoTransform[4] != 0
            || adfGeoTransform[5] != 1 )
            bHaveGeoTransform = TRUE;

        if( pszProjection )
            CPLDebug( "GDALJP2Metadata", 
                "Got projection from GeoJP2 (geotiff) box (%d): %s", 
                iBestIndex, pszProjection );
    }

    /* Cleanup unused boxes */
    for(i=0; i < nMax; i++)
    {
        if( i != iBestIndex )
        {
            CPLFree( apszProjection[i] );
            if( anGCPCount[i] > 0 )
            {
                GDALDeinitGCPs( anGCPCount[i], apasGCPList[i] );
                CPLFree( apasGCPList[i] );
            }
        }
    }

    return iBestIndex >= 0;
}

/************************************************************************/
/*                             ParseMSIG()                              */
/************************************************************************/

int GDALJP2Metadata::ParseMSIG()

{
    if( nMSIGSize < 70 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Try and extract worldfile parameters and adjust.                */
/* -------------------------------------------------------------------- */
    memcpy( adfGeoTransform + 0, pabyMSIGData + 22 + 8 * 4, 8 );
    memcpy( adfGeoTransform + 1, pabyMSIGData + 22 + 8 * 0, 8 );
    memcpy( adfGeoTransform + 2, pabyMSIGData + 22 + 8 * 2, 8 );
    memcpy( adfGeoTransform + 3, pabyMSIGData + 22 + 8 * 5, 8 );
    memcpy( adfGeoTransform + 4, pabyMSIGData + 22 + 8 * 1, 8 );
    memcpy( adfGeoTransform + 5, pabyMSIGData + 22 + 8 * 3, 8 );

    // data is in LSB (little endian) order in file.
    CPL_LSBPTR64( adfGeoTransform + 0 );
    CPL_LSBPTR64( adfGeoTransform + 1 );
    CPL_LSBPTR64( adfGeoTransform + 2 );
    CPL_LSBPTR64( adfGeoTransform + 3 );
    CPL_LSBPTR64( adfGeoTransform + 4 );
    CPL_LSBPTR64( adfGeoTransform + 5 );

    // correct for center of pixel vs. top left of pixel
    adfGeoTransform[0] -= 0.5 * adfGeoTransform[1];
    adfGeoTransform[0] -= 0.5 * adfGeoTransform[2];
    adfGeoTransform[3] -= 0.5 * adfGeoTransform[4];
    adfGeoTransform[3] -= 0.5 * adfGeoTransform[5];

    bHaveGeoTransform = TRUE;

    return TRUE;
}

/************************************************************************/
/*                         GetDictionaryItem()                          */
/************************************************************************/

static CPLXMLNode *
GetDictionaryItem( char **papszGMLMetadata, const char *pszURN )

{
    char *pszLabel;
    const char *pszFragmentId = NULL;
    int i;


    if( EQUALN(pszURN,"urn:jp2k:xml:", 13) )
        pszLabel = CPLStrdup( pszURN + 13 );
    else if( EQUALN(pszURN,"urn:ogc:tc:gmljp2:xml:", 22) )
        pszLabel = CPLStrdup( pszURN + 22 );
    else if( EQUALN(pszURN,"gmljp2://xml/",13) )
        pszLabel = CPLStrdup( pszURN + 13 );
    else
        pszLabel = CPLStrdup( pszURN );

/* -------------------------------------------------------------------- */
/*      Split out label and fragment id.                                */
/* -------------------------------------------------------------------- */
    for( i = 0; pszLabel[i] != '#'; i++ )
    {
        if( pszLabel[i] == '\0' )
        {
            CPLFree(pszLabel);
            return NULL;
        }
    }

    pszFragmentId = pszLabel + i + 1;
    pszLabel[i] = '\0';

/* -------------------------------------------------------------------- */
/*      Can we find an XML box with the desired label?                  */
/* -------------------------------------------------------------------- */
    const char *pszDictionary = 
        CSLFetchNameValue( papszGMLMetadata, pszLabel );

    if( pszDictionary == NULL )
    {
        CPLFree(pszLabel);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try and parse the dictionary.                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDictTree = CPLParseXMLString( pszDictionary );

    if( psDictTree == NULL )
    {
        CPLFree(pszLabel);
        return NULL;
    }

    CPLStripXMLNamespace( psDictTree, NULL, TRUE );

    CPLXMLNode *psDictRoot = CPLSearchXMLNode( psDictTree, "=Dictionary" );
    
    if( psDictRoot == NULL )
    {
        CPLDestroyXMLNode( psDictTree );
        CPLFree(pszLabel);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Search for matching id.                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psEntry, *psHit = NULL;
    for( psEntry = psDictRoot->psChild; 
         psEntry != NULL && psHit == NULL; 
         psEntry = psEntry->psNext )
    {
        const char *pszId;

        if( psEntry->eType != CXT_Element )
            continue;

        if( !EQUAL(psEntry->pszValue,"dictionaryEntry") )
            continue;
        
        if( psEntry->psChild == NULL )
            continue;

        pszId = CPLGetXMLValue( psEntry->psChild, "id", "" );

        if( EQUAL(pszId, pszFragmentId) )
            psHit = CPLCloneXMLTree( psEntry->psChild );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pszLabel );
    CPLDestroyXMLNode( psDictTree );

    return psHit;
}

        
/************************************************************************/
/*                            GMLSRSLookup()                            */
/*                                                                      */
/*      Lookup an SRS in a dictionary inside this file.  We will get    */
/*      something like:                                                 */
/*        urn:jp2k:xml:CRSDictionary.xml#crs1112                        */
/*                                                                      */
/*      We need to split the filename from the fragment id, and         */
/*      lookup the fragment in the file if we can find it our           */
/*      list of labelled xml boxes.                                     */
/************************************************************************/

int GDALJP2Metadata::GMLSRSLookup( const char *pszURN )

{
    CPLXMLNode *psDictEntry = GetDictionaryItem( papszGMLMetadata, pszURN );

    if( psDictEntry == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Reserialize this fragment.                                      */
/* -------------------------------------------------------------------- */
    char *pszDictEntryXML = CPLSerializeXMLTree( psDictEntry );
    CPLDestroyXMLNode( psDictEntry );

/* -------------------------------------------------------------------- */
/*      Try to convert into an OGRSpatialReference.                     */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    int bSuccess = FALSE;

    if( oSRS.importFromXML( pszDictEntryXML ) == OGRERR_NONE )
    {
        CPLFree( pszProjection );
        pszProjection = NULL;

        oSRS.exportToWkt( &pszProjection );
        bSuccess = TRUE;
    }

    CPLFree( pszDictEntryXML );

    return bSuccess;
}

/************************************************************************/
/*                        ParseGMLCoverageDesc()                        */
/************************************************************************/

int GDALJP2Metadata::ParseGMLCoverageDesc() 

{
    if(! CSLTestBoolean(CPLGetConfigOption("GDAL_USE_GMLJP2", "TRUE")) )
        return FALSE;

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
        adfGeoTransform[1] = CPLAtof(papszOffset1Tokens[0]);
        adfGeoTransform[2] = CPLAtof(papszOffset2Tokens[0]);
        adfGeoTransform[3] = poOriginGeometry->getY();
        adfGeoTransform[4] = CPLAtof(papszOffset1Tokens[1]);
        adfGeoTransform[5] = CPLAtof(papszOffset2Tokens[1]);

        // offset from center of pixel.
        adfGeoTransform[0] -= adfGeoTransform[1]*0.5;
        adfGeoTransform[0] -= adfGeoTransform[2]*0.5;
        adfGeoTransform[3] -= adfGeoTransform[4]*0.5;
        adfGeoTransform[3] -= adfGeoTransform[5]*0.5;

        bSuccess = TRUE;
        bHaveGeoTransform = TRUE;
    }

    CSLDestroy( papszOffset1Tokens );
    CSLDestroy( papszOffset2Tokens );

    if( poOriginGeometry != NULL )
        delete poOriginGeometry;

/* -------------------------------------------------------------------- */
/*      If we still don't have an srsName, check for it on the          */
/*      boundedBy Envelope.  Some products                              */
/*      (ie. EuropeRasterTile23.jpx) use this as the only srsName       */
/*      delivery vehicle.                                               */
/* -------------------------------------------------------------------- */
    if( pszSRSName == NULL )
    {
        pszSRSName = 
            CPLGetXMLValue( psXML,
                            "=FeatureCollection.boundedBy.Envelope.srsName",
                            NULL );
    }
/* -------------------------------------------------------------------- */
/*      Examples of DGIWG_Profile_of_JPEG2000_for_Georeference_Imagery.pdf */
/*      have srsName only on RectifiedGrid element.                     */
/* -------------------------------------------------------------------- */
    if( psRG != NULL && pszSRSName == NULL )
    {
        pszSRSName = CPLGetXMLValue( psRG,  "srsName", NULL );
    }

/* -------------------------------------------------------------------- */
/*      If we have gotten a geotransform, then try to interprete the    */
/*      srsName.                                                        */
/* -------------------------------------------------------------------- */
    int bNeedAxisFlip = FALSE;

    if( bSuccess && pszSRSName != NULL 
        && (pszProjection == NULL || strlen(pszProjection) == 0) )
    {
        OGRSpatialReference oSRS;

        if( EQUALN(pszSRSName,"epsg:",5) )
        {
            if( oSRS.SetFromUserInput( pszSRSName ) == OGRERR_NONE )
                oSRS.exportToWkt( &pszProjection );
        }
        else if( (EQUALN(pszSRSName,"urn:",4) 
                 && strstr(pszSRSName,":def:") != NULL
                 && oSRS.importFromURN(pszSRSName) == OGRERR_NONE) ||
                 /* GMLJP2 v2.0 uses CRS URL instead of URN */
                 /* See e.g. http://schemas.opengis.net/gmljp2/2.0/examples/minimalInstance.xml */
                 (EQUALN(pszSRSName,"http://www.opengis.net/def/crs/",
                         strlen("http://www.opengis.net/def/crs/")) 
                 && oSRS.importFromCRSURL(pszSRSName) == OGRERR_NONE) )
        {
            oSRS.exportToWkt( &pszProjection );

            // Per #2131
            if( oSRS.EPSGTreatsAsLatLong() || oSRS.EPSGTreatsAsNorthingEasting() )
            {
                CPLDebug( "GMLJP2", "Request axis flip for SRS=%s",
                          pszSRSName );
                bNeedAxisFlip = TRUE;
            }
        }
        else if( !GMLSRSLookup( pszSRSName ) )
        {
            CPLDebug( "GDALJP2Metadata", 
                      "Unable to evaluate SRSName=%s", 
                      pszSRSName );
        }
    }

    if( pszProjection )
        CPLDebug( "GDALJP2Metadata", 
                  "Got projection from GML box: %s", 
                 pszProjection );

    CPLDestroyXMLNode( psXML );
    psXML = NULL;

/* -------------------------------------------------------------------- */
/*      Do we need to flip the axes?                                    */
/* -------------------------------------------------------------------- */
    if( bNeedAxisFlip
        && CSLTestBoolean( CPLGetConfigOption( "GDAL_IGNORE_AXIS_ORIENTATION",
                                               "FALSE" ) ) )
    {
        bNeedAxisFlip = FALSE;
        CPLDebug( "GMLJP2", "Suppressed axis flipping based on GDAL_IGNORE_AXIS_ORIENTATION." );
    }

    if( bNeedAxisFlip )
    {
        double dfTemp;

        CPLDebug( "GMLJP2", 
                  "Flipping axis orientation in GMLJP2 coverage description." );

        dfTemp = adfGeoTransform[0];
        adfGeoTransform[0] = adfGeoTransform[3];
        adfGeoTransform[3] = dfTemp;

        int swapWith1Index = 4;
        int swapWith2Index = 5;

        /* Look if we have GDAL_JP2K_ALT_OFFSETVECTOR_ORDER=TRUE as a XML comment */
        int bHasAltOffsetVectorOrderComment =
            strstr(pszCoverage, "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER=TRUE") != NULL;

        if( bHasAltOffsetVectorOrderComment ||
            CSLTestBoolean( CPLGetConfigOption( "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER",
                                                "FALSE" ) ) )
        {
            swapWith1Index = 5;
            swapWith2Index = 4;
            CPLDebug( "GMLJP2", "Choosing alternate GML \"<offsetVector>\" order based on "
                "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER." );
        }

        dfTemp = adfGeoTransform[1];
        adfGeoTransform[1] = adfGeoTransform[swapWith1Index];
        adfGeoTransform[swapWith1Index] = dfTemp;

        dfTemp = adfGeoTransform[2];
        adfGeoTransform[2] = adfGeoTransform[swapWith2Index];
        adfGeoTransform[swapWith2Index] = dfTemp;

        /* Found in autotest/gdrivers/data/ll.jp2 */
        if( adfGeoTransform[1] == 0.0 && adfGeoTransform[2] < 0.0 &&
            adfGeoTransform[4] > 0.0 && adfGeoTransform[5] == 0.0 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "It is likely that the axis order of the GMLJP2 box is not "
                     "consistant with the EPSG order and that the resulting georeferencing "
                     "will be incorrect. Try setting GDAL_IGNORE_AXIS_ORIENTATION=TRUE if it is the case");
        }
    }

    return pszProjection != NULL && bSuccess;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

void GDALJP2Metadata::SetProjection( const char *pszWKT )

{
    CPLFree( pszProjection );
    pszProjection = CPLStrdup(pszWKT);
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

void GDALJP2Metadata::SetGCPs( int nCount, const GDAL_GCP *pasGCPsIn )

{
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    nGCPCount = nCount;
    pasGCPList = GDALDuplicateGCPs(nGCPCount, pasGCPsIn);
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

void GDALJP2Metadata::SetGeoTransform( double *padfGT )

{
    memcpy( adfGeoTransform, padfGT, sizeof(double) * 6 );
}

/************************************************************************/
/*                          CreateJP2GeoTIFF()                          */
/************************************************************************/

GDALJP2Box *GDALJP2Metadata::CreateJP2GeoTIFF()

{
/* -------------------------------------------------------------------- */
/*      Prepare the memory buffer containing the degenerate GeoTIFF     */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    int         nGTBufSize = 0;
    unsigned char *pabyGTBuf = NULL;

    if( GTIFMemBufFromWktEx( pszProjection, adfGeoTransform, 
                             nGCPCount, pasGCPList,
                             &nGTBufSize, &pabyGTBuf, bPixelIsPoint ) != CE_None )
        return NULL;

    if( nGTBufSize == 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Write to a box on the JP2 file.                                 */
/* -------------------------------------------------------------------- */
    GDALJP2Box *poBox;

    poBox = GDALJP2Box::CreateUUIDBox( msi_uuid2, nGTBufSize, pabyGTBuf );
    
    CPLFree( pabyGTBuf );

    return poBox;
}

/************************************************************************/
/*                     GetGMLJP2GeoreferencingInfo()                    */
/************************************************************************/

int GDALJP2Metadata::GetGMLJP2GeoreferencingInfo( int& nEPSGCode,
                                                  double adfOrigin[2],
                                                  double adfXVector[2],
                                                  double adfYVector[2],
                                                  const char*& pszComment,
                                                  CPLString& osDictBox )
{

/* -------------------------------------------------------------------- */
/*      Try do determine a PCS or GCS code we can use.                  */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    char *pszWKTCopy = (char *) pszProjection;
    nEPSGCode = 0;
    int  bNeedAxisFlip = FALSE;

    if( oSRS.importFromWkt( &pszWKTCopy ) != OGRERR_NONE )
        return FALSE;

    if( oSRS.IsProjected() )
    {
        const char *pszAuthName = oSRS.GetAuthorityName( "PROJCS" );

        if( pszAuthName != NULL && EQUAL(pszAuthName,"epsg") )
        {
            nEPSGCode = atoi(oSRS.GetAuthorityCode( "PROJCS" ));
        }
    }
    else if( oSRS.IsGeographic() )
    {
        const char *pszAuthName = oSRS.GetAuthorityName( "GEOGCS" );

        if( pszAuthName != NULL && EQUAL(pszAuthName,"epsg") )
        {
            nEPSGCode = atoi(oSRS.GetAuthorityCode( "GEOGCS" ));
        }
    }

    // Save error state as importFromEPSGA() will call CPLReset()
    int errNo = CPLGetLastErrorNo();
    CPLErr eErr = CPLGetLastErrorType();
    CPLString osLastErrorMsg = CPLGetLastErrorMsg();

    // Determinte if we need to flix axis. Reimport from EPSG and make
    // sure not to strip axis definitions to determine the axis order.
    if( nEPSGCode != 0 && oSRS.importFromEPSGA(nEPSGCode) == OGRERR_NONE )
    {
        if( oSRS.EPSGTreatsAsLatLong() || oSRS.EPSGTreatsAsNorthingEasting() )
        {
            bNeedAxisFlip = TRUE;
        }
    }

    // Restore error state
    CPLErrorSetState( eErr, errNo, osLastErrorMsg);

/* -------------------------------------------------------------------- */
/*      Prepare coverage origin and offset vectors.  Take axis          */
/*      order into account if needed.                                   */
/* -------------------------------------------------------------------- */
    adfOrigin[0] = adfGeoTransform[0] + adfGeoTransform[1] * 0.5
        + adfGeoTransform[4] * 0.5;
    adfOrigin[1] = adfGeoTransform[3] + adfGeoTransform[2] * 0.5
        + adfGeoTransform[5] * 0.5;
    adfXVector[0] = adfGeoTransform[1];
    adfXVector[1] = adfGeoTransform[2];
        
    adfYVector[0] = adfGeoTransform[4];
    adfYVector[1] = adfGeoTransform[5];
    
    if( bNeedAxisFlip
        && CSLTestBoolean( CPLGetConfigOption( "GDAL_IGNORE_AXIS_ORIENTATION",
                                               "FALSE" ) ) )
    {
        bNeedAxisFlip = FALSE;
        CPLDebug( "GMLJP2", "Suppressed axis flipping on write based on GDAL_IGNORE_AXIS_ORIENTATION." );
    }

    pszComment = "";
    if( bNeedAxisFlip )
    {
        double dfTemp;
        
        CPLDebug( "GMLJP2", "Flipping GML coverage axis order." );
        
        dfTemp = adfOrigin[0];
        adfOrigin[0] = adfOrigin[1];
        adfOrigin[1] = dfTemp;

        if( CSLTestBoolean( CPLGetConfigOption( "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER",
                                                "FALSE" ) ) )
        {
            CPLDebug( "GMLJP2", "Choosing alternate GML \"<offsetVector>\" order based on "
                "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER." );

            /* In this case the swapping is done in an "X" pattern */
            dfTemp = adfXVector[0];
            adfXVector[0] = adfYVector[1];
            adfYVector[1] = dfTemp;

            dfTemp = adfYVector[0];
            adfYVector[0] = adfXVector[1];
            adfXVector[1] = dfTemp;

            /* We add this as an XML comment so that we know we must do OffsetVector flipping on reading */
            pszComment = "              <!-- GDAL_JP2K_ALT_OFFSETVECTOR_ORDER=TRUE: First "
                         "value of offset is latitude/northing component of the "
                         "latitude/northing axis. -->\n";
        }
        else
        {
            dfTemp = adfXVector[0];
            adfXVector[0] = adfXVector[1];
            adfXVector[1] = dfTemp;

            dfTemp = adfYVector[0];
            adfYVector[0] = adfYVector[1];
            adfYVector[1] = dfTemp;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we need a user defined CRSDictionary entry, prepare it       */
/*      here.                                                           */
/* -------------------------------------------------------------------- */
    if( nEPSGCode == 0 )
    {
        char *pszGMLDef = NULL;

        if( oSRS.exportToXML( &pszGMLDef, NULL ) == OGRERR_NONE )
        {
            char* pszWKT = NULL;
            oSRS.exportToWkt(&pszWKT);
            char* pszXMLEscapedWKT = CPLEscapeString(pszWKT, -1, CPLES_XML);
            CPLFree(pszWKT);
            osDictBox.Printf(  
"<gml:Dictionary gml:id=\"CRSU1\" \n"
"        xmlns:gml=\"http://www.opengis.net/gml\"\n"
"        xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
"        xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
"        xsi:schemaLocation=\"http://www.opengis.net/gml http://schemas.opengis.net/gml/3.1.1/base/gml.xsd\">\n"
"  <gml:description>Dictionnary for cursom SRS %s</gml:description>\n"
"  <gml:name>Dictionnary for custom SRS</gml:name>\n"
"  <gml:dictionaryEntry>\n"
"%s\n"
"  </gml:dictionaryEntry>\n"
"</gml:Dictionary>\n",
                     pszXMLEscapedWKT, pszGMLDef );
            CPLFree(pszXMLEscapedWKT);
        }
        CPLFree( pszGMLDef );
    }

    return TRUE;
}

/************************************************************************/
/*                          CreateGMLJP2()                              */
/************************************************************************/

GDALJP2Box *GDALJP2Metadata::CreateGMLJP2( int nXSize, int nYSize )

{
/* -------------------------------------------------------------------- */
/*      This is a backdoor to let us embed a literal gmljp2 chunk       */
/*      supplied by the user as an external file.  This is mostly       */
/*      for preparing test files with exotic contents.                  */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "GMLJP2OVERRIDE", NULL ) != NULL )
    {
        VSILFILE *fp = VSIFOpenL( CPLGetConfigOption( "GMLJP2OVERRIDE",""), "r" );
        char *pszGML = NULL;

        if( fp == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to open GMLJP2OVERRIDE file." );
            return NULL;
        }
        
        VSIFSeekL( fp, 0, SEEK_END );
        int nLength = (int) VSIFTellL( fp );
        pszGML = (char *) CPLCalloc(1,nLength+1);
        VSIFSeekL( fp, 0, SEEK_SET );
        VSIFReadL( pszGML, 1, nLength, fp );
        VSIFCloseL( fp );

        GDALJP2Box *apoGMLBoxes[2];

        apoGMLBoxes[0] = GDALJP2Box::CreateLblBox( "gml.data" );
        apoGMLBoxes[1] = 
            GDALJP2Box::CreateLabelledXMLAssoc( "gml.root-instance", 
                                                pszGML );

        GDALJP2Box *poGMLData = GDALJP2Box::CreateAsocBox( 2, apoGMLBoxes);
        
        delete apoGMLBoxes[0];
        delete apoGMLBoxes[1];

        CPLFree( pszGML );
        
        return poGMLData;
    }

    int nEPSGCode;
    double adfOrigin[2];
    double adfXVector[2];
    double adfYVector[2];
    const char* pszComment = "";
    CPLString osDictBox;
    if( !GetGMLJP2GeoreferencingInfo( nEPSGCode, adfOrigin,
                                      adfXVector, adfYVector,
                                      pszComment, osDictBox ) )
    {
        return NULL;
    }

    char szSRSName[100];
    if( nEPSGCode != 0 )
        sprintf( szSRSName, "urn:ogc:def:crs:EPSG::%d", nEPSGCode );
    else
        strcpy( szSRSName, 
                "gmljp2://xml/CRSDictionary.gml#ogrcrs1" );

/* -------------------------------------------------------------------- */
/*      For now we hardcode for a minimal instance format.              */
/* -------------------------------------------------------------------- */
    CPLString osDoc;

    osDoc.Printf( 
"<gml:FeatureCollection\n"
"   xmlns:gml=\"http://www.opengis.net/gml\"\n"
"   xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
"   xsi:schemaLocation=\"http://www.opengis.net/gml http://schemas.opengis.net/gml/3.1.1/profiles/gmlJP2Profile/1.0.0/gmlJP2Profile.xsd\">\n"
"  <gml:boundedBy>\n"
"    <gml:Null>withheld</gml:Null>\n"
"  </gml:boundedBy>\n"
"  <gml:featureMember>\n"
"    <gml:FeatureCollection>\n"
"      <gml:featureMember>\n"
"        <gml:RectifiedGridCoverage dimension=\"2\" gml:id=\"RGC0001\">\n"
"          <gml:rectifiedGridDomain>\n"
"            <gml:RectifiedGrid dimension=\"2\">\n"
"              <gml:limits>\n"
"                <gml:GridEnvelope>\n"
"                  <gml:low>0 0</gml:low>\n"
"                  <gml:high>%d %d</gml:high>\n"
"                </gml:GridEnvelope>\n"
"              </gml:limits>\n"
"              <gml:axisName>x</gml:axisName>\n"
"              <gml:axisName>y</gml:axisName>\n"
"              <gml:origin>\n"
"                <gml:Point gml:id=\"P0001\" srsName=\"%s\">\n"
"                  <gml:pos>%.15g %.15g</gml:pos>\n"
"                </gml:Point>\n"
"              </gml:origin>\n"
"%s"
"              <gml:offsetVector srsName=\"%s\">%.15g %.15g</gml:offsetVector>\n"
"              <gml:offsetVector srsName=\"%s\">%.15g %.15g</gml:offsetVector>\n"
"            </gml:RectifiedGrid>\n"
"          </gml:rectifiedGridDomain>\n"
"          <gml:rangeSet>\n"
"            <gml:File>\n"
"              <gml:rangeParameters/>\n"
"              <gml:fileName>gmljp2://codestream/0</gml:fileName>\n"
"              <gml:fileStructure>Record Interleaved</gml:fileStructure>\n"
"            </gml:File>\n"
"          </gml:rangeSet>\n"
"        </gml:RectifiedGridCoverage>\n"
"      </gml:featureMember>\n"
"    </gml:FeatureCollection>\n"
"  </gml:featureMember>\n"
"</gml:FeatureCollection>\n",
             nXSize-1, nYSize-1, szSRSName, adfOrigin[0], adfOrigin[1],
             pszComment,
             szSRSName, adfXVector[0], adfXVector[1], 
             szSRSName, adfYVector[0], adfYVector[1] );

/* -------------------------------------------------------------------- */
/*      Setup the gml.data label.                                       */
/* -------------------------------------------------------------------- */
    GDALJP2Box *apoGMLBoxes[5];
    int nGMLBoxes = 0;

    apoGMLBoxes[nGMLBoxes++] = GDALJP2Box::CreateLblBox( "gml.data" );

/* -------------------------------------------------------------------- */
/*      Setup gml.root-instance.                                        */
/* -------------------------------------------------------------------- */
    apoGMLBoxes[nGMLBoxes++] = 
        GDALJP2Box::CreateLabelledXMLAssoc( "gml.root-instance", osDoc );

/* -------------------------------------------------------------------- */
/*      Add optional dictionary.                                        */
/* -------------------------------------------------------------------- */
    if( osDictBox.size() > 0 )
        apoGMLBoxes[nGMLBoxes++] = 
            GDALJP2Box::CreateLabelledXMLAssoc( "CRSDictionary.gml",
                                                osDictBox );
        
/* -------------------------------------------------------------------- */
/*      Bundle gml.data boxes into an association.                      */
/* -------------------------------------------------------------------- */
    GDALJP2Box *poGMLData = GDALJP2Box::CreateAsocBox( nGMLBoxes, apoGMLBoxes);

/* -------------------------------------------------------------------- */
/*      Cleanup working boxes.                                          */
/* -------------------------------------------------------------------- */
    while( nGMLBoxes > 0 )
        delete apoGMLBoxes[--nGMLBoxes];

    return poGMLData;
}

/************************************************************************/
/*                          CreateGMLJP2V2()                            */
/************************************************************************/

class GMLJP2V2GMLFileDesc
{
    public:
        CPLString osFile;
        CPLString osNamespace;
        CPLString osSchemaLocation;
        int       bInline;
        int       bParentCoverageCollection;

            GMLJP2V2GMLFileDesc(): bInline(TRUE), bParentCoverageCollection(TRUE) {}
};

class GMLJP2V2AnnotationDesc
{
    public:
        CPLString osFile;
};

class GMLJP2V2MetadataDesc
{
    public:
        CPLString osFile;
        CPLString osContent;
        int       bParentCoverageCollection;

            GMLJP2V2MetadataDesc(): bParentCoverageCollection(TRUE) {}
};

class GMLJP2V2BoxDesc
{
    public:
        CPLString osFile;
        CPLString osLabel;
};

static CPLXMLNode* GetXMLRoot(CPLXMLNode* psNode)
{
    for( ; psNode != NULL; psNode = psNode->psNext )
    {
        if( psNode->eType == CXT_Element && strcmp(psNode->pszValue, "?xml") != 0 )
            return psNode;
    }
    return NULL;
}

GDALJP2Box *GDALJP2Metadata::CreateGMLJP2V2( int nXSize, int nYSize,
                                             const char* pszDefFilename )

{
    CPLString osRootGMLId = "ID_GMLJP2_0";
    CPLString osGridCoverage;
    CPLString osGridCoverageFile;
    int bCRSURL = TRUE;
    std::vector<GMLJP2V2MetadataDesc> aoMetadata;
    std::vector<GMLJP2V2AnnotationDesc> aoAnnotations;
    std::vector<GMLJP2V2GMLFileDesc> aoGMLFiles;
    std::vector<GMLJP2V2BoxDesc> aoBoxes;

/* -------------------------------------------------------------------- */
/*      Parse definition file.                                          */
/* -------------------------------------------------------------------- */
    if( pszDefFilename && !EQUAL(pszDefFilename, "YES") && !EQUAL(pszDefFilename, "TRUE") )
    {
        GByte* pabyContent = NULL;
        if( pszDefFilename[0] != '{' )
        {
            if( !VSIIngestFile( NULL, pszDefFilename, &pabyContent, NULL, -1 ) )
                return NULL;
        }

/*
{
    "#doc" : "Unless otherwise specified, all elements are optional",

    "#root_instance_doc": "Describe content of the GMLJP2CoverageCollection",
    "root_instance": {
        "#gml_id_doc": "Specify GMLJP2CoverageCollection id here. Default is ID_GMLJP2_0",
        "gml_id": "some_gml_id",

        "#grid_coverage_file_doc": [
            "External XML file, whose root might be a GMLJP2GridCoverage, ",
            "GMLJP2RectifiedGridCoverage or a GMLJP2ReferenceableGridCoverage",
            "If not specified, GDAL will auto-generate a GMLJP2RectifiedGridCoverage" ],
        "grid_coverage_file": "gmljp2gridcoverage.xml",

        "#crs_url_doc": [
            "true for http://www.opengis.net/def/crs/EPSG/0/XXXX CRS URL.",
            "If false, use CRS URN. Default value is true" ],
        "crs_url": true,

        "#metadata_doc": [ "An array of metadata items. Can be either strings, with ",
                           "a filename or directly inline XML content, or either ",
                           "a more complete description." ],
        "metadata": [

            "dcmetadata.xml",

            {
                "#file_doc": "Can use relative or absolute paths. Required",
                "file": "dcmetadata.xml",

                "#content": "Exclusive of file. Inline XML metadata content",
                "content": "<gmljp2:metadata>Some simple textual metadata</gmljp2:metadata>",

                "#parent_node": ["Where to put the metadata.",
                                 "Under CoverageCollection (default) or GridCoverage" ],
                "parent_node": "CoverageCollection"
            }
        ],

        "#annotations_doc": [ "An array of filenames, either directly KML files",
                              "or other vector files recognized by GDAL that ",
                              "will be translated on-the-fly as KML" ],
        "annotations": [
            "my.kml"
        ],

        "#gml_filelist_doc" :[
            "An array of GML files. Can be either GML filenames, ",
            "or a more complete description" ],
        "gml_filelist": [

            "my.gml",

            {
                "#file_doc": "can use relative or absolute paths. Required",
                "file": "converted/test_0.gml",

                "#namespace_doc": ["The namespace in schemaLocation for which to substitute",
                                  "its original schemaLocation with the one provided below"],
                "namespace": "http://example.com",

                "#schema_location_doc": ["Value of the substitued schemaLocation. ",
                                         "Typically a schema box label (link)"],
                "schema_location": "gmljp2://xml/schema_0.xsd",

                "#inline_doc": "Whether to inline the content, or put it in a separate xml box. Default is true",
                "inline": true,

                "#parent_node": ["Where to put the FeatureCollection.",
                                 "Under CoverageCollection (default) or GridCoverage" ],
                "parent_node": "CoverageCollection"
            }
        ]
    },

    "#boxes_doc": "An array to describe the content of XML asoc boxes",
    "boxes": [
        {
            "#file_doc": "can use relative or absolute paths. Required",
            "file": "converted/test_0.xsd",

            "#label_doc": ["the label of the XML box. If not specified, will be the ",
                          "filename without the directory part." ],
            "label": "schema_0.xsd"
        }
    ]
}
*/

        json_tokener* jstok = NULL;
        json_object* poObj = NULL;

        jstok = json_tokener_new();
        poObj = json_tokener_parse_ex(jstok, pabyContent ? (const char*) pabyContent : pszDefFilename, -1);
        CPLFree(pabyContent);
        if( jstok->err != json_tokener_success)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "JSON parsing error: %s (at offset %d)",
                        json_tokener_error_desc(jstok->err), jstok->char_offset);
            json_tokener_free(jstok);
            return NULL;
        }
        json_tokener_free(jstok);

        json_object* poRootInstance = json_object_object_get(poObj, "root_instance");
        if( poRootInstance && json_object_get_type(poRootInstance) == json_type_object )
        {
            json_object* poGMLId = json_object_object_get(poRootInstance, "gml_id");
            if( poGMLId && json_object_get_type(poGMLId) == json_type_string )
                osRootGMLId = json_object_get_string(poGMLId);

            json_object* poGridCoverageFile = json_object_object_get(poRootInstance, "grid_coverage_file");
            if( poGridCoverageFile && json_object_get_type(poGridCoverageFile) == json_type_string )
                osGridCoverageFile = json_object_get_string(poGridCoverageFile);

            json_object* poCRSURL = json_object_object_get(poRootInstance, "crs_url");
            if( poCRSURL && json_object_get_type(poCRSURL) == json_type_boolean )
                bCRSURL = json_object_get_boolean(poCRSURL);


            json_object* poMetadatas = json_object_object_get(poRootInstance, "metadata"); 
            if( poMetadatas && json_object_get_type(poMetadatas) == json_type_array )
            {
                for(int i=0;i<json_object_array_length(poMetadatas);i++)
                {
                    json_object* poMetadata = json_object_array_get_idx(poMetadatas, i);
                    if( poMetadata && json_object_get_type(poMetadata) == json_type_string )
                    {
                        GMLJP2V2MetadataDesc oDesc;
                        const char* pszStr = json_object_get_string(poMetadata);
                        if( pszStr[0] == '<' )
                            oDesc.osContent = pszStr;
                        else
                            oDesc.osFile = pszStr;
                        aoMetadata.push_back(oDesc);
                    }
                    else if ( poMetadata && json_object_get_type(poMetadata) == json_type_object )
                    {
                        const char* pszFile = NULL;
                        json_object* poFile = json_object_object_get(poMetadata, "file");
                        if( poFile && json_object_get_type(poFile) == json_type_string )
                            pszFile = json_object_get_string(poFile);
                        
                        const char* pszContent = NULL;
                        json_object* poContent = json_object_object_get(poMetadata, "content");
                        if( poContent && json_object_get_type(poContent) == json_type_string )
                            pszContent = json_object_get_string(poContent);

                        if( pszFile != NULL || pszContent != NULL )
                        {
                            GMLJP2V2MetadataDesc oDesc;
                            if( pszFile )
                                oDesc.osFile = pszFile;
                            if( pszContent )
                                oDesc.osContent = pszContent;

                            json_object* poLocation = json_object_object_get(poMetadata, "parent_node");
                            if( poLocation && json_object_get_type(poLocation) == json_type_string )
                            {
                                const char* pszLocation = json_object_get_string(poLocation);
                                if( EQUAL(pszLocation, "CoverageCollection") )
                                    oDesc.bParentCoverageCollection  = TRUE;
                                else if( EQUAL(pszLocation, "GridCoverage") )
                                    oDesc.bParentCoverageCollection = FALSE;
                                else
                                    CPLError(CE_Warning, CPLE_NotSupported,
                                             "metadata.location should be CoverageCollection or GridCoverage");
                            }

                            aoMetadata.push_back(oDesc);
                        }
                    }
                }
            }

            json_object* poAnnotations = json_object_object_get(poRootInstance, "annotations"); 
            if( poAnnotations && json_object_get_type(poAnnotations) == json_type_array )
            {
                for(int i=0;i<json_object_array_length(poAnnotations);i++)
                {
                    json_object* poAnnotation = json_object_array_get_idx(poAnnotations, i);
                    if( poAnnotation && json_object_get_type(poAnnotation) == json_type_string )
                    {
                        GMLJP2V2AnnotationDesc oDesc;
                        oDesc.osFile = json_object_get_string(poAnnotation);
                        aoAnnotations.push_back(oDesc);
                    }
                }
            }

            json_object* poGMLFileList = json_object_object_get(poRootInstance, "gml_filelist"); 
            if( poGMLFileList && json_object_get_type(poGMLFileList) == json_type_array )
            {
                for(int i=0;i<json_object_array_length(poGMLFileList);i++)
                {
                    json_object* poGMLFile = json_object_array_get_idx(poGMLFileList, i);
                    if( poGMLFile && json_object_get_type(poGMLFile) == json_type_object )
                    {
                        json_object* poFile = json_object_object_get(poGMLFile, "file");
                        if( poFile && json_object_get_type(poFile) == json_type_string )
                        {
                            GMLJP2V2GMLFileDesc oDesc;
                            oDesc.osFile = json_object_get_string(poFile);

                            json_object* poNamespace = json_object_object_get(poGMLFile, "namespace");
                            if( poNamespace && json_object_get_type(poNamespace) == json_type_string )
                                oDesc.osNamespace = json_object_get_string(poNamespace);

                            json_object* poSchemaLocation = json_object_object_get(poGMLFile, "schema_location");
                            if( poSchemaLocation && json_object_get_type(poSchemaLocation) == json_type_string )
                                oDesc.osSchemaLocation = json_object_get_string(poSchemaLocation);

                            json_object* poInline = json_object_object_get(poGMLFile, "inline");
                            if( poInline && json_object_get_type(poInline) == json_type_boolean )
                                oDesc.bInline = json_object_get_boolean(poInline);


                            json_object* poLocation = json_object_object_get(poGMLFile, "parent_node");
                            if( poLocation && json_object_get_type(poLocation) == json_type_string )
                            {
                                const char* pszLocation = json_object_get_string(poLocation);
                                if( EQUAL(pszLocation, "CoverageCollection") )
                                    oDesc.bParentCoverageCollection  = TRUE;
                                else if( EQUAL(pszLocation, "GridCoverage") )
                                    oDesc.bParentCoverageCollection = FALSE;
                                else
                                    CPLError(CE_Warning, CPLE_NotSupported,
                                             "gml_filelist.location should be CoverageCollection or GridCoverage");
                            }

                            aoGMLFiles.push_back(oDesc);
                        }
                    }
                    else if( poGMLFile && json_object_get_type(poGMLFile) == json_type_string )
                    {
                        GMLJP2V2GMLFileDesc oDesc;
                        oDesc.osFile = json_object_get_string(poGMLFile);
                        aoGMLFiles.push_back(oDesc);
                    }
                }
            }

        }

        json_object* poBoxes = json_object_object_get(poObj, "boxes"); 
        if( poBoxes && json_object_get_type(poBoxes) == json_type_array )
        {
            for(int i=0;i<json_object_array_length(poBoxes);i++)
            {
                json_object* poBox = json_object_array_get_idx(poBoxes, i);
                if( poBox && json_object_get_type(poBox) == json_type_object )
                {
                    json_object* poFile = json_object_object_get(poBox, "file");
                    if( poFile && json_object_get_type(poFile) == json_type_string )
                    {
                        GMLJP2V2BoxDesc oDesc;
                        oDesc.osFile = json_object_get_string(poFile);

                        json_object* poLabel = json_object_object_get(poBox, "label");
                        if( poLabel && json_object_get_type(poLabel) == json_type_string )
                            oDesc.osLabel = json_object_get_string(poLabel);
                        else
                            oDesc.osLabel = CPLGetFilename(oDesc.osFile);

                        aoBoxes.push_back(oDesc);
                    }
                }
                else if( poBox && json_object_get_type(poBox) == json_type_string )
                {
                    GMLJP2V2BoxDesc oDesc;
                    oDesc.osFile = json_object_get_string(poBox);
                    oDesc.osLabel = CPLGetFilename(oDesc.osFile);
                    aoBoxes.push_back(oDesc);
                }
            }
        }

        json_object_put(poObj);

        // Check that if a GML file points to an internal schemaLocation,
        // the matching box really exists.
        for(int i=0;i<(int)aoGMLFiles.size();i++)
        {
            if( aoGMLFiles[i].osSchemaLocation.size() &&
                strncmp(aoGMLFiles[i].osSchemaLocation, "gmljp2://xml/",
                        strlen("gmljp2://xml/")) == 0 )
            {
                const char* pszLookedLabel =
                    aoGMLFiles[i].osSchemaLocation.c_str() + strlen("gmljp2://xml/");
                int bFound = FALSE;
                for(int j=0; !bFound && j<(int)aoBoxes.size();j++)
                    bFound = (strcmp(pszLookedLabel, aoBoxes[j].osLabel) == 0);
                if( !bFound )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "GML file %s has a schema_location=%s, but no box with label %s is defined",
                             aoGMLFiles[i].osFile.c_str(),
                             aoGMLFiles[i].osSchemaLocation.c_str(),
                             pszLookedLabel);
                }
            }
        }

        // Read custom grid coverage file
        if( osGridCoverageFile.size() > 0 )
        {
            CPLXMLNode* psTmp = CPLParseXMLFile(osGridCoverageFile);
            if( psTmp == NULL )
                return NULL;
            CPLXMLNode* psTmpRoot = GetXMLRoot(psTmp);
            if( psTmpRoot )
            {
                char* pszTmp = CPLSerializeXMLTree(psTmpRoot);
                osGridCoverage = pszTmp;
                CPLFree(pszTmp);
            }
            CPLDestroyXMLNode(psTmp);
        }
    }

    CPLString osDictBox;
    CPLString osDoc;

    if( osGridCoverage.size() == 0 )
    {
/* -------------------------------------------------------------------- */
/*      Prepare GMLJP2RectifiedGridCoverage                             */
/* -------------------------------------------------------------------- */
        int nEPSGCode = 0;
        double adfOrigin[2];
        double adfXVector[2];
        double adfYVector[2];
        const char* pszComment = "";   
        if( !GetGMLJP2GeoreferencingInfo( nEPSGCode, adfOrigin,
                                        adfXVector, adfYVector,
                                        pszComment, osDictBox ) )
        {
            return NULL;
        }

        char szSRSName[100] = {0};
        if( nEPSGCode != 0 )
        {
            if( bCRSURL )
                sprintf( szSRSName, "http://www.opengis.net/def/crs/EPSG/0/%d", nEPSGCode );
            else
                sprintf( szSRSName, "urn:ogc:def:crs:EPSG::%d", nEPSGCode );
        }
        else
            strcpy( szSRSName, 
                    "gmljp2://xml/CRSDictionary.gml#ogrcrs1" );

        osGridCoverage.Printf(
"   <gmljp2:GMLJP2RectifiedGridCoverage gml:id=\"RGC_1_%s\">\n"
"     <gml:domainSet>\n"
"      <gml:RectifiedGrid gml:id=\"RGC_1_GRID_%s\" dimension=\"2\" srsName=\"%s\">\n"
"       <gml:limits>\n"
"         <gml:GridEnvelope>\n"
"           <gml:low>0 0</gml:low>\n"
"           <gml:high>%d %d</gml:high>\n"
"         </gml:GridEnvelope>\n"
"       </gml:limits>\n"
"       <gml:axisName>x</gml:axisName>\n"
"       <gml:axisName>y</gml:axisName>\n"
"       <gml:origin>\n"
"         <gml:Point gml:id=\"P0001\" srsName=\"%s\">\n"
"           <gml:pos>%.15g %.15g</gml:pos>\n"
"         </gml:Point>\n"
"       </gml:origin>\n"
"%s"
"       <gml:offsetVector srsName=\"%s\">%.15g %.15g</gml:offsetVector>\n"
"       <gml:offsetVector srsName=\"%s\">%.15g %.15g</gml:offsetVector>\n"
"      </gml:RectifiedGrid>\n"
"     </gml:domainSet>\n"
"     <gml:rangeSet>\n"
"      <gml:File>\n"
"        <gml:rangeParameters/>\n"
"        <gml:fileName>gmljp2://codestream/0</gml:fileName>\n"
"        <gml:fileStructure>inapplicable</gml:fileStructure>\n"
"      </gml:File>\n"
"     </gml:rangeSet>\n"
"     <gmlcov:rangeType/>\n"
"   </gmljp2:GMLJP2RectifiedGridCoverage>\n",
            osRootGMLId.c_str(),
            osRootGMLId.c_str(),
            szSRSName,
            nXSize-1, nYSize-1, szSRSName, adfOrigin[0], adfOrigin[1],
            pszComment,
            szSRSName, adfXVector[0], adfXVector[1], 
            szSRSName, adfYVector[0], adfYVector[1] );
    }

/* -------------------------------------------------------------------- */
/*      Main node.                                                      */
/* -------------------------------------------------------------------- */
    osDoc.Printf( 
//"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gmljp2:GMLJP2CoverageCollection gml:id=\"%s\"\n"
"     xmlns:gml=\"http://www.opengis.net/gml/3.2\"\n"
"     xmlns:gmlcov=\"http://www.opengis.net/gmlcov/1.0\"\n"
"     xmlns:gmljp2=\"http://www.opengis.net/gmljp2/2.0\"\n"
"     xmlns:swe=\"http://www.opengis.net/swe/2.0\"\n"
"     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
"     xsi:schemaLocation=\"http://www.opengis.net/gmljp2/2.0 http://schemas.opengis.net/gmljp2/2.0/gmljp2.xsd\">\n"
"  <gml:gridDomain/>\n"
"  <gml:rangeSet>\n"
"   <gml:File>\n"
"     <gml:rangeParameters/>\n"
"     <gml:fileName>gmljp2://codestream</gml:fileName>\n"
"     <gml:fileStructure>inapplicable</gml:fileStructure>\n"
"   </gml:File>\n"
"  </gml:rangeSet>\n"
"  <gmlcov:rangeType/>\n"
"  <gmljp2:featureMember>\n"
"%s"
"  </gmljp2:featureMember>\n"
"</gmljp2:GMLJP2CoverageCollection>\n",
                 osRootGMLId.c_str(),
                 osGridCoverage.c_str() );

/* -------------------------------------------------------------------- */
/*      Process metadata, annotations and features collections.         */
/* -------------------------------------------------------------------- */
    std::vector<CPLString> aosTmpFiles;
    int bRootHasXLink = FALSE;
    if( aoMetadata.size() || aoAnnotations.size() || aoGMLFiles.size() )
    {
        CPLXMLNode* psRoot = CPLParseXMLString(osDoc);
        CPLAssert(psRoot);
        CPLXMLNode* psGMLJP2CoverageCollection = GetXMLRoot(psRoot);
        CPLAssert(psGMLJP2CoverageCollection);

        for( int i=0; i < (int)aoMetadata.size(); i++ )
        {
            CPLXMLNode* psMetadata;
            if( aoMetadata[i].osFile.size() )
                psMetadata = CPLParseXMLFile(aoMetadata[i].osFile);
            else
                psMetadata = CPLParseXMLString(aoMetadata[i].osContent);
            if( psMetadata == NULL )
                continue;
            CPLXMLNode* psMetadataRoot = GetXMLRoot(psMetadata);
            if( psMetadataRoot )
            {
                if( strcmp(psMetadataRoot->pszValue, "gmljp2:isoMetadata") != 0 &&
                    strcmp(psMetadataRoot->pszValue, "gmljp2:eopMetadata") != 0 &&
                    strcmp(psMetadataRoot->pszValue, "gmljp2:dcMetadata") != 0 &&
                    strcmp(psMetadataRoot->pszValue, "gmljp2:metadata") != 0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The metadata root node should be one of gmljp2:isoMetadata, "
                             "gmljp2:eopMetadata, gmljp2:dcMetadata or gmljp2:metadata");
                }
                else if( aoMetadata[i].bParentCoverageCollection )
                {
                    /*  Insert the gmlcov:metadata link as the next sibbling of */
                    /* GMLJP2CoverageCollection.rangeType */
                    CPLXMLNode* psRangeType =
                        CPLGetXMLNode(psGMLJP2CoverageCollection, "gmlcov:rangeType");
                    CPLAssert(psRangeType);
                    CPLXMLNode* psNodeAfterWhichToInsert = psRangeType;
                    CPLXMLNode* psNext = psNodeAfterWhichToInsert->psNext;
                    while( psNext != NULL && psNext->eType == CXT_Element &&
                           strcmp(psNext->pszValue, "gmlcov:metadata") == 0 )
                    {
                        psNodeAfterWhichToInsert = psNext;
                        psNext = psNext->psNext;
                    }
                    psNodeAfterWhichToInsert->psNext = NULL;
                    CPLXMLNode* psGMLCovMetadata = CPLCreateXMLNode(
                        psGMLJP2CoverageCollection, CXT_Element, "gmlcov:metadata" );
                    psGMLCovMetadata->psNext = psNext;
                    CPLXMLNode* psGMLJP2Metadata = CPLCreateXMLNode(
                        psGMLCovMetadata, CXT_Element, "gmljp2:Metadata" );
                    CPLAddXMLChild( psGMLJP2Metadata, CPLCloneXMLTree(psMetadataRoot) );
                }
                else
                {
                    /* Insert the gmlcov:metadata link as the last child of */
                    /* GMLJP2RectifiedGridCoverage typically */
                    CPLXMLNode* psFeatureMemberOfGridCoverage =
                        CPLGetXMLNode(psGMLJP2CoverageCollection, "gmljp2:featureMember");
                    CPLAssert(psFeatureMemberOfGridCoverage);
                    CPLXMLNode* psGridCoverage = psFeatureMemberOfGridCoverage->psChild;
                    CPLAssert(psGridCoverage);
                    CPLXMLNode* psGMLCovMetadata = CPLCreateXMLNode(
                        psGridCoverage, CXT_Element, "gmlcov:metadata" );
                    CPLXMLNode* psGMLJP2Metadata = CPLCreateXMLNode(
                        psGMLCovMetadata, CXT_Element, "gmljp2:Metadata" );
                    CPLAddXMLChild( psGMLJP2Metadata, CPLCloneXMLTree(psMetadataRoot) );
                }
            }
            CPLDestroyXMLNode(psMetadata);
        }

        // Examples of inline or reference feature collections can be found
        // in http://schemas.opengis.net/gmljp2/2.0/examples/gmljp2.xml

        for( int i=0; i < (int)aoGMLFiles.size(); i++ )
        {
            // Is the file already a GML file ?
            CPLXMLNode* psGMLFile = NULL;
            if( EQUAL(CPLGetExtension(aoGMLFiles[i].osFile), "gml") ||
                EQUAL(CPLGetExtension(aoGMLFiles[i].osFile), "xml") )
            {
                 psGMLFile = CPLParseXMLFile(aoGMLFiles[i].osFile);
            }
            GDALDriverH hDrv = NULL;
            if( psGMLFile == NULL )
            {
                hDrv = GDALIdentifyDriver(aoGMLFiles[i].osFile, NULL);
                if( hDrv == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s is no a GDAL recognized file",
                             aoGMLFiles[i].osFile.c_str());
                    continue;
                }
            }
            GDALDriverH hGMLDrv = GDALGetDriverByName("GML");
            if( psGMLFile == NULL && hDrv == hGMLDrv )
            {
                // Yes, parse it
                psGMLFile = CPLParseXMLFile(aoGMLFiles[i].osFile);
            }
            else if( psGMLFile == NULL )
            {
                if( hGMLDrv == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot translate %s to GML",
                             aoGMLFiles[i].osFile.c_str());
                    continue;
                }

                // On-the-fly translation to GML 3.2
                GDALDatasetH hSrcDS = GDALOpenEx(aoGMLFiles[i].osFile, 0, NULL, NULL, NULL);
                if( hSrcDS )
                {
                    CPLString osTmpFile = CPLSPrintf("/vsimem/gmljp2/%p/%d/%s.gml",
                                                     this,
                                                     i,
                                                     CPLGetBasename(aoGMLFiles[i].osFile));
                    char* apszOptions[2];
                    apszOptions[0] = (char*) "FORMAT=GML3.2";
                    apszOptions[1] = NULL;
                    GDALDatasetH hDS = GDALCreateCopy(hGMLDrv, osTmpFile, hSrcDS,
                                                      FALSE, apszOptions, NULL, NULL);
                    if( hDS )
                    {
                        GDALClose(hDS);
                        psGMLFile = CPLParseXMLFile(osTmpFile);
                        aoGMLFiles[i].osFile = osTmpFile;
                        VSIUnlink(osTmpFile);
                        aosTmpFiles.push_back(CPLResetExtension(osTmpFile, "xsd"));
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Conversion of %s to GML failed",
                                 aoGMLFiles[i].osFile.c_str());
                    }
                }
                GDALClose(hSrcDS);
            }
            if( psGMLFile == NULL )
                continue;

            CPLXMLNode* psGMLFileRoot = GetXMLRoot(psGMLFile);
            if( psGMLFileRoot ) 
            {
                CPLXMLNode *node_f;
                if( aoGMLFiles[i].bParentCoverageCollection )
                {
                    // Insert in gmljp2:featureMember.gmljp2:GMLJP2Features.gmljp2:feature
                    CPLXMLNode *node_fm = CPLCreateXMLNode(
                            psGMLJP2CoverageCollection, CXT_Element, "gmljp2:featureMember" );

                    CPLXMLNode *node_gf = CPLCreateXMLNode(
                            node_fm, CXT_Element, "gmljp2:GMLJP2Features" );


                    CPLSetXMLValue(node_gf, "#gml:id", CPLSPrintf("%s_GMLJP2Features_%d",
                                                                    osRootGMLId.c_str(),
                                                                    i));

                    node_f = CPLCreateXMLNode( node_gf, CXT_Element, "gmljp2:feature"  );
                }
                else
                {
                    CPLXMLNode* psFeatureMemberOfGridCoverage =
                        CPLGetXMLNode(psGMLJP2CoverageCollection, "gmljp2:featureMember");
                    CPLAssert(psFeatureMemberOfGridCoverage);
                    CPLXMLNode* psGridCoverage = psFeatureMemberOfGridCoverage->psChild;
                    CPLAssert(psGridCoverage);
                    node_f = CPLCreateXMLNode( psGridCoverage, CXT_Element, "gmljp2:feature"  );
                }

                CPLString osTmpFile;
                if( !aoGMLFiles[i].bInline )
                {
                    osTmpFile = CPLSPrintf("/vsimem/gmljp2/%p/%d/%s.gml",
                                           this,
                                           i,
                                           CPLGetBasename(aoGMLFiles[i].osFile));
                    aosTmpFiles.push_back(osTmpFile);

                    GMLJP2V2BoxDesc oDesc;
                    oDesc.osFile = osTmpFile;
                    oDesc.osLabel = CPLGetFilename(oDesc.osFile);
                    aoBoxes.push_back(oDesc);

                    if( !bRootHasXLink )
                    {
                        bRootHasXLink = TRUE;
                        CPLSetXMLValue(psGMLJP2CoverageCollection, "#xmlns:xlink",
                                       "http://www.w3.org/1999/xlink");
                    }

                    CPLSetXMLValue(node_f, "#xlink:href",
                                   CPLSPrintf("gmljp2://xml/%s", oDesc.osLabel.c_str()));
                }

                if( CPLGetXMLNode(psGMLFileRoot, "xmlns") == NULL &&
                    CPLGetXMLNode(psGMLFileRoot, "xmlns:gml") == NULL )
                {
                    CPLSetXMLValue(psGMLFileRoot, "#xmlns",
                                   "http://www.opengis.net/gml/3.2");
                }

                // modify the gml id making it unique for this document
                CPLXMLNode* psGMLFileGMLId =
                    CPLGetXMLNode(psGMLFileRoot, "gml:id");
                if( psGMLFileGMLId && psGMLFileGMLId->eType == CXT_Attribute )
                    CPLSetXMLValue( psGMLFileGMLId, "", 
                                    CPLSPrintf("%s_%d_%s",
                                                osRootGMLId.c_str(), i,
                                                psGMLFileGMLId->psChild->pszValue) );
                psGMLFileGMLId = NULL;
                //PrefixAllGMLIds(psGMLFileRoot, CPLSPrintf("%s_%d_", osRootGMLId.c_str(), i));

                // replace schema location
                CPLXMLNode* psSchemaLocation =
                    CPLGetXMLNode(psGMLFileRoot, "xsi:schemaLocation");
                if( psSchemaLocation && psSchemaLocation->eType == CXT_Attribute )
                {
                    char **papszTokens = CSLTokenizeString2(
                        psSchemaLocation->psChild->pszValue, " \t\n", 
                        CSLT_HONOURSTRINGS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
                    CPLString osSchemaLocation;

                    if( CSLCount(papszTokens) == 2 &&
                        aoGMLFiles[i].osNamespace.size() == 0 &&
                        aoGMLFiles[i].osSchemaLocation.size() )
                    {
                        osSchemaLocation += papszTokens[0];
                        osSchemaLocation += " ";
                        osSchemaLocation += aoGMLFiles[i].osSchemaLocation;
                    }

                    else if( CSLCount(papszTokens) == 2 &&
                                (aoGMLFiles[i].osNamespace.size() == 0 ||
                                strcmp(papszTokens[0], aoGMLFiles[i].osNamespace) == 0) &&
                                aoGMLFiles[i].osSchemaLocation.size() == 0 )
                    {
                        VSIStatBufL sStat;
                        CPLString osXSD;
                        if( CSLCount(papszTokens) == 2 &&
                            !CPLIsFilenameRelative(papszTokens[1]) &&
                            VSIStatL(papszTokens[1], &sStat) == 0 )
                        {
                            osXSD = papszTokens[1];
                        }
                        else if( CSLCount(papszTokens) == 2 &&
                                CPLIsFilenameRelative(papszTokens[1]) &&
                                VSIStatL(CPLFormFilename(CPLGetDirname(aoGMLFiles[i].osFile),
                                                        papszTokens[1], NULL),
                                        &sStat) == 0 )
                        {
                            osXSD = CPLFormFilename(CPLGetDirname(aoGMLFiles[i].osFile),
                                                        papszTokens[1], NULL);
                        }
                        if( osXSD.size() )
                        {
                            GMLJP2V2BoxDesc oDesc;
                            oDesc.osFile = osXSD;
                            oDesc.osLabel = CPLGetFilename(oDesc.osFile);
                            osSchemaLocation += papszTokens[0];
                            osSchemaLocation += " ";
                            osSchemaLocation += "gmljp2://xml/";
                            osSchemaLocation += oDesc.osLabel;
                            int j;
                            for( j=0; j<(int)aoBoxes.size(); j++)
                            {
                                if( aoBoxes[j].osLabel == oDesc.osLabel )
                                    break;
                            }
                            if( j == (int)aoBoxes.size() )
                                aoBoxes.push_back(oDesc);
                        }
                    }

                    else if( (CSLCount(papszTokens) % 2) == 0 )
                    {
                        for(char** papszIter = papszTokens; *papszIter; papszIter += 2 )
                        {
                            if( osSchemaLocation.size() )
                                osSchemaLocation += " ";
                            if( aoGMLFiles[i].osNamespace.size() &&
                                aoGMLFiles[i].osSchemaLocation.size() &&
                                strcmp(papszIter[0], aoGMLFiles[i].osNamespace) == 0 )
                            {
                                osSchemaLocation += papszIter[0];
                                osSchemaLocation += " ";
                                osSchemaLocation += aoGMLFiles[i].osSchemaLocation;
                            }
                            else
                            {
                                osSchemaLocation += papszIter[0];
                                osSchemaLocation += " ";
                                osSchemaLocation += papszIter[1];
                            }
                        }
                    }
                    CSLDestroy(papszTokens);
                    CPLSetXMLValue( psSchemaLocation, "", osSchemaLocation);
                }

                if( aoGMLFiles[i].bInline )
                    CPLAddXMLChild(node_f, CPLCloneXMLTree(psGMLFileRoot));
                else
                    CPLSerializeXMLTreeToFile( psGMLFile, osTmpFile );
            }
            CPLDestroyXMLNode(psGMLFile);
        }

        // Cf http://schemas.opengis.net/gmljp2/2.0/examples/gmljp2_annotation.xml
        for( int i=0; i < (int)aoAnnotations.size(); i++ )
        {
            // Is the file already a KML file ?
            CPLXMLNode* psKMLFile = NULL;
            if( EQUAL(CPLGetExtension(aoAnnotations[i].osFile), "kml") )
                 psKMLFile = CPLParseXMLFile(aoAnnotations[i].osFile);
            GDALDriverH hDrv = NULL;
            if( psKMLFile == NULL )
            {
                hDrv = GDALIdentifyDriver(aoAnnotations[i].osFile, NULL);
                if( hDrv == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s is no a GDAL recognized file",
                             aoAnnotations[i].osFile.c_str());
                    continue;
                }
            }
            GDALDriverH hKMLDrv = GDALGetDriverByName("KML");
            GDALDriverH hLIBKMLDrv = GDALGetDriverByName("LIBKML");
            if( psKMLFile == NULL && (hDrv == hKMLDrv || hDrv == hLIBKMLDrv) )
            {
                // Yes, parse it
                psKMLFile = CPLParseXMLFile(aoAnnotations[i].osFile);
            }
            else if( psKMLFile == NULL )
            {
                if( hKMLDrv == NULL && hLIBKMLDrv == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot translate %s to KML",
                             aoAnnotations[i].osFile.c_str());
                    continue;
                }

                // On-the-fly translation to KML
                GDALDatasetH hSrcDS = GDALOpenEx(aoAnnotations[i].osFile, 0, NULL, NULL, NULL);
                if( hSrcDS )
                {
                    CPLString osTmpFile = CPLSPrintf("/vsimem/gmljp2/%p/%d/%s.kml",
                                                     this,
                                                     i,
                                                     CPLGetBasename(aoAnnotations[i].osFile));
                    char* apszOptions[2];
                    apszOptions[0] = NULL;
                    apszOptions[1] = NULL;
                    GDALDatasetH hDS = GDALCreateCopy(hLIBKMLDrv ? hLIBKMLDrv : hKMLDrv,
                                                      osTmpFile, hSrcDS,
                                                      FALSE, apszOptions, NULL, NULL);
                    if( hDS )
                    {
                        GDALClose(hDS);
                        psKMLFile = CPLParseXMLFile(osTmpFile);
                        aoAnnotations[i].osFile = osTmpFile;
                        VSIUnlink(osTmpFile);
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Conversion of %s to KML failed",
                                  aoAnnotations[i].osFile.c_str());
                    }
                }
                GDALClose(hSrcDS);
            }
            if( psKMLFile == NULL )
                continue;

            CPLXMLNode* psKMLFileRoot = GetXMLRoot(psKMLFile);
            if( psKMLFileRoot ) 
            {
                CPLXMLNode* psFeatureMemberOfGridCoverage =
                    CPLGetXMLNode(psGMLJP2CoverageCollection, "gmljp2:featureMember");
                CPLAssert(psFeatureMemberOfGridCoverage);
                CPLXMLNode* psGridCoverage = psFeatureMemberOfGridCoverage->psChild;
                CPLAssert(psGridCoverage);
                CPLXMLNode *psAnnotation = CPLCreateXMLNode(
                            psGridCoverage, CXT_Element, "gmljp2:annotation" );

                /* Add a xsi:schemaLocation if not already present */
                if( psKMLFileRoot->eType == CXT_Element &&
                    strcmp(psKMLFileRoot->pszValue, "kml") == 0 &&
                    CPLGetXMLNode(psKMLFileRoot, "xsi:schemaLocation") == NULL &&
                    strcmp(CPLGetXMLValue(psKMLFileRoot, "xmlns", ""),
                           "http://www.opengis.net/kml/2.2") == 0  )
                {
                    CPLSetXMLValue(psKMLFileRoot, "#xsi:schemaLocation",
                                   "http://www.opengis.net/kml/2.2 http://schemas.opengis.net/kml/2.2.0/ogckml22.xsd");
                }

                CPLAddXMLChild(psAnnotation, CPLCloneXMLTree(psKMLFileRoot));
            }
            CPLDestroyXMLNode(psKMLFile);
        }

        char* pszRoot = CPLSerializeXMLTree(psRoot);
        CPLDestroyXMLNode(psRoot);
        psRoot = NULL;
        osDoc = pszRoot;
        CPLFree(pszRoot);
        pszRoot = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup the gml.data label.                                       */
/* -------------------------------------------------------------------- */
    std::vector<GDALJP2Box *> apoGMLBoxes;

    apoGMLBoxes.push_back(GDALJP2Box::CreateLblBox( "gml.data" ));

/* -------------------------------------------------------------------- */
/*      Setup gml.root-instance.                                        */
/* -------------------------------------------------------------------- */
    apoGMLBoxes.push_back(
        GDALJP2Box::CreateLabelledXMLAssoc( "gml.root-instance", osDoc ));

/* -------------------------------------------------------------------- */
/*      Add optional dictionary.                                        */
/* -------------------------------------------------------------------- */
    if( osDictBox.size() > 0 )
        apoGMLBoxes.push_back(
            GDALJP2Box::CreateLabelledXMLAssoc( "CRSDictionary.gml",
                                                osDictBox ) );

/* -------------------------------------------------------------------- */
/*      Additional user specified boxes.                                */
/* -------------------------------------------------------------------- */
    for( int i=0; i < (int)aoBoxes.size(); i++ )
    {
        GByte* pabyContent = NULL;
        if( VSIIngestFile( NULL, aoBoxes[i].osFile, &pabyContent, NULL, -1 ) )
        {
            CPLXMLNode* psNode = CPLParseXMLString((const char*)pabyContent);
            CPLFree(pabyContent);
            pabyContent = NULL;
            if( psNode )
            {
                CPLXMLNode* psRoot = GetXMLRoot(psNode);
                if( psRoot ) 
                {
                    pabyContent = (GByte*) CPLSerializeXMLTree(psRoot);
                    apoGMLBoxes.push_back(
                        GDALJP2Box::CreateLabelledXMLAssoc( aoBoxes[i].osLabel,
                                                            (const char*)pabyContent ) );
                }
                CPLDestroyXMLNode (psNode);
            }
        }
        CPLFree(pabyContent);
    }

/* -------------------------------------------------------------------- */
/*      Bundle gml.data boxes into an association.                      */
/* -------------------------------------------------------------------- */
    GDALJP2Box *poGMLData = GDALJP2Box::CreateAsocBox( (int)apoGMLBoxes.size(),
                                                       &apoGMLBoxes[0]);

/* -------------------------------------------------------------------- */
/*      Cleanup working boxes.                                          */
/* -------------------------------------------------------------------- */
    for( int i=0; i < (int)apoGMLBoxes.size(); i++ )
        delete apoGMLBoxes[i];

    for( int i=0; i < (int)aosTmpFiles.size(); i++ )
    {
        VSIUnlink(aosTmpFiles[i]);
    }

    return poGMLData;
}

/************************************************************************/
/*                CreateGDALMultiDomainMetadataXMLBox()                 */
/************************************************************************/

GDALJP2Box *GDALJP2Metadata::CreateGDALMultiDomainMetadataXMLBox(
                                       GDALDataset* poSrcDS,
                                       int bMainMDDomainOnly )
{
    GDALMultiDomainMetadata oLocalMDMD;
    char** papszSrcMD = CSLDuplicate(poSrcDS->GetMetadata());
    /* Remove useless metadata */
    papszSrcMD = CSLSetNameValue(papszSrcMD, GDALMD_AREA_OR_POINT, NULL);
    papszSrcMD = CSLSetNameValue(papszSrcMD, "TIFFTAG_RESOLUTIONUNIT", NULL);
    papszSrcMD = CSLSetNameValue(papszSrcMD, "TIFFTAG_XRESOLUTION", NULL);
    papszSrcMD = CSLSetNameValue(papszSrcMD, "TIFFTAG_YRESOLUTION", NULL);
    papszSrcMD = CSLSetNameValue(papszSrcMD, "Corder", NULL); /* from JP2KAK */
    if( poSrcDS->GetDriver() != NULL &&
        EQUAL(poSrcDS->GetDriver()->GetDescription(), "JP2ECW") )
    {
        papszSrcMD = CSLSetNameValue(papszSrcMD, "COMPRESSION_RATE_TARGET", NULL);
        papszSrcMD = CSLSetNameValue(papszSrcMD, "COLORSPACE", NULL);
        papszSrcMD = CSLSetNameValue(papszSrcMD, "VERSION", NULL);
    }

    int bHasMD = FALSE;
    if( papszSrcMD && *papszSrcMD )
    {
        bHasMD = TRUE;
        oLocalMDMD.SetMetadata(papszSrcMD);
    }
    CSLDestroy(papszSrcMD);

    if( !bMainMDDomainOnly )
    {
        char** papszMDList = poSrcDS->GetMetadataDomainList();
        for( char** papszMDListIter = papszMDList; 
            papszMDListIter && *papszMDListIter; ++papszMDListIter )
        {
            if( !EQUAL(*papszMDListIter, "") &&
                !EQUAL(*papszMDListIter, "IMAGE_STRUCTURE") &&
                !EQUAL(*papszMDListIter, "JPEG2000") &&
                !EQUALN(*papszMDListIter, "xml:BOX_", strlen("xml:BOX_")) &&
                !EQUAL(*papszMDListIter, "xml:gml.root-instance") &&
                !EQUAL(*papszMDListIter, "xml:XMP") &&
                !EQUAL(*papszMDListIter, "xml:IPR") )
            {
                papszSrcMD = poSrcDS->GetMetadata(*papszMDListIter);
                if( papszSrcMD && *papszSrcMD )
                {
                    bHasMD = TRUE;
                    oLocalMDMD.SetMetadata(papszSrcMD, *papszMDListIter);
                }
            }
        }
        CSLDestroy(papszMDList);
    }

    GDALJP2Box* poBox = NULL;
    if( bHasMD )
    {
        CPLXMLNode* psXMLNode = oLocalMDMD.Serialize();
        CPLXMLNode* psMasterXMLNode = CPLCreateXMLNode( NULL, CXT_Element,
                                                    "GDALMultiDomainMetadata" );
        psMasterXMLNode->psChild = psXMLNode;
        char* pszXML = CPLSerializeXMLTree(psMasterXMLNode);
        CPLDestroyXMLNode(psMasterXMLNode);

        poBox = new GDALJP2Box();
        poBox->SetType("xml ");
        poBox->SetWritableData(strlen(pszXML) + 1, (const GByte*)pszXML);
        CPLFree(pszXML);
    }
    return poBox;
}

/************************************************************************/
/*                         WriteXMLBoxes()                              */
/************************************************************************/

GDALJP2Box** GDALJP2Metadata::CreateXMLBoxes( GDALDataset* poSrcDS,
                                              int* pnBoxes )
{
    GDALJP2Box** papoBoxes = NULL;
    *pnBoxes = 0;
    char** papszMDList = poSrcDS->GetMetadataDomainList();
    for( char** papszMDListIter = papszMDList; 
        papszMDListIter && *papszMDListIter; ++papszMDListIter )
    {
        /* Write metadata that look like originating from JP2 XML boxes */
        /* as a standalone JP2 XML box */
        if( EQUALN(*papszMDListIter, "xml:BOX_", strlen("xml:BOX_")) )
        {
            char** papszSrcMD = poSrcDS->GetMetadata(*papszMDListIter);
            if( papszSrcMD && *papszSrcMD )
            {
                GDALJP2Box* poBox = new GDALJP2Box();
                poBox->SetType("xml ");
                poBox->SetWritableData(strlen(*papszSrcMD) + 1,
                                       (const GByte*)*papszSrcMD);
                papoBoxes = (GDALJP2Box**)CPLRealloc(papoBoxes,
                                        sizeof(GDALJP2Box*) * (*pnBoxes + 1));
                papoBoxes[(*pnBoxes) ++] = poBox;
            }
        }
    }
    CSLDestroy(papszMDList);
    return papoBoxes;
}

/************************************************************************/
/*                          CreateXMPBox()                              */
/************************************************************************/

GDALJP2Box *GDALJP2Metadata::CreateXMPBox ( GDALDataset* poSrcDS )
{
    char** papszSrcMD = poSrcDS->GetMetadata("xml:XMP");
    GDALJP2Box* poBox = NULL;
    if( papszSrcMD && * papszSrcMD )
    {
        poBox = GDALJP2Box::CreateUUIDBox(xmp_uuid,
                                          strlen(*papszSrcMD) + 1,
                                          (const GByte*)*papszSrcMD);
    }
    return poBox;
}

/************************************************************************/
/*                          CreateIPRBox()                              */
/************************************************************************/

GDALJP2Box *GDALJP2Metadata::CreateIPRBox ( GDALDataset* poSrcDS )
{
    char** papszSrcMD = poSrcDS->GetMetadata("xml:IPR");
    GDALJP2Box* poBox = NULL;
    if( papszSrcMD && * papszSrcMD )
    {
        poBox = new GDALJP2Box();
        poBox->SetType("jp2i");
        poBox->SetWritableData(strlen(*papszSrcMD) + 1,
                                        (const GByte*)*papszSrcMD);
    }
    return poBox;
}

/************************************************************************/
/*                           IsUUID_MSI()                              */
/************************************************************************/

int GDALJP2Metadata::IsUUID_MSI(const GByte *abyUUID)
{
    return memcmp(abyUUID, msi_uuid2, 16) == 0;
}

/************************************************************************/
/*                           IsUUID_XMP()                               */
/************************************************************************/

int GDALJP2Metadata::IsUUID_XMP(const GByte *abyUUID)
{
    return memcmp(abyUUID, xmp_uuid, 16) == 0;
}
