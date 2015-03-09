/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  GDALJP2Metadata - Read GeoTIFF and/or GML georef info.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
/*                         PrepareCoverageBox()                         */
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

/* -------------------------------------------------------------------- */
/*      Try do determine a PCS or GCS code we can use.                  */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    char *pszWKTCopy = (char *) pszProjection;
    int nEPSGCode = 0;
    char szSRSName[100];
    int  bNeedAxisFlip = FALSE;

    if( oSRS.importFromWkt( &pszWKTCopy ) != OGRERR_NONE )
        return NULL;

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

    // Determinte if we need to flix axis. Reimport from EPSG and make
    // sure not to strip axis definitions to determine the axis order.
    if( nEPSGCode != 0 && oSRS.importFromEPSGA(nEPSGCode) == OGRERR_NONE )
    {
        if( oSRS.EPSGTreatsAsLatLong() || oSRS.EPSGTreatsAsNorthingEasting() )
        {
            bNeedAxisFlip = TRUE;
        }
    }

    if( nEPSGCode != 0 )
        sprintf( szSRSName, "urn:ogc:def:crs:EPSG::%d", nEPSGCode );
    else
        strcpy( szSRSName, 
                "gmljp2://xml/CRSDictionary.gml#ogrcrs1" );

/* -------------------------------------------------------------------- */
/*      Prepare coverage origin and offset vectors.  Take axis          */
/*      order into account if needed.                                   */
/* -------------------------------------------------------------------- */
    double adfOrigin[2];
    double adfXVector[2];
    double adfYVector[2];
    
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

    const char* pszComment = "";
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
/*      If we need a user defined CRSDictionary entry, prepare it       */
/*      here.                                                           */
/* -------------------------------------------------------------------- */
    CPLString osDictBox;

    if( nEPSGCode == 0 )
    {
        char *pszGMLDef = NULL;

        if( oSRS.exportToXML( &pszGMLDef, NULL ) == OGRERR_NONE )
        {
            osDictBox.Printf(  
"<gml:Dictionary gml:id=\"CRSU1\" \n"
"        xmlns:gml=\"http://www.opengis.net/gml\"\n"
"        xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
"        xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
"  <gml:dictionaryEntry>\n"
"%s\n"
"  </gml:dictionaryEntry>\n"
"</gml:Dictionary>\n",
                     pszGMLDef );
        }
        CPLFree( pszGMLDef );
    }

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
    if( strlen(osDictBox) > 0 )
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


/************************************************************************/
/*                      GDALGetJPEG2000StructureInternal()              */
/************************************************************************/

static void AddField(CPLXMLNode* psParent, const char* pszFieldName,
                     int nFieldSize, const char* pszValue,
                     const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                    psParent, "Field", pszValue );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "string" );
    CPLAddXMLAttributeAndValue(psField, "size", CPLSPrintf("%d", nFieldSize )  );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static void AddHexField(CPLXMLNode* psParent, const char* pszFieldName,
                        int nFieldSize, const char* pszValue,
                        const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                    psParent, "Field", pszValue );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "hexint" );
    CPLAddXMLAttributeAndValue(psField, "size", CPLSPrintf("%d", nFieldSize )  );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static void AddField(CPLXMLNode* psParent, const char* pszFieldName, GByte nVal,
                     const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                psParent, "Field", CPLSPrintf("%d", nVal) );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "uint8" );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static void AddField(CPLXMLNode* psParent, const char* pszFieldName, GUInt16 nVal,
                     const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                psParent, "Field", CPLSPrintf("%d", nVal) );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "uint16" );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static void AddField(CPLXMLNode* psParent, const char* pszFieldName, GUInt32 nVal,
                     const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                psParent, "Field", CPLSPrintf("%u", nVal) );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "uint32" );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static const char* GetInterpretationOfBPC(GByte bpc)
{
    if( bpc == 255 )
        return NULL;
    if( (bpc & 0x80) )
        return CPLSPrintf("Signed %d bits", 1 + (bpc & 0x7F));
    else
        return CPLSPrintf("Unsigned %d bits", 1 + bpc);
}

static const char* GetStandardFieldString(GUInt16 nVal)
{
    switch(nVal)
    {
        case 1: return "Codestream contains no extensions";
        case 2: return "Contains multiple composition layers";
        case 3: return "Codestream is compressed using JPEG 2000 and requires at least a Profile 0 decoder";
        case 4: return "Codestream is compressed using JPEG 2000 and requires at least a Profile 1 decoder";
        case 5: return "Codestream is compressed using JPEG 2000 unrestricted";
        case 35: return "Contains IPR metadata";
        case 67: return "Contains GMLJP2 metadata";
        default: return NULL;
    }
}

static void DumpGeoTIFFBox(CPLXMLNode* psBox,
                           GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    GDALDriver* poVRTDriver = (GDALDriver*) GDALGetDriverByName("VRT");
    if( pabyBoxData && poVRTDriver)
    {
        CPLString osTmpFilename(CPLSPrintf("/vsimem/tmp_%p.tif", oBox.GetFILE()));
        VSIFCloseL(VSIFileFromMemBuffer(
            osTmpFilename, pabyBoxData, nBoxDataLength, TRUE) );
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALDataset* poDS = (GDALDataset*) GDALOpen(osTmpFilename, GA_ReadOnly);
        CPLPopErrorHandler();
        if( poDS )
        {
            CPLString osTmpVRTFilename(CPLSPrintf("/vsimem/tmp_%p.vrt", oBox.GetFILE()));
            GDALDataset* poVRTDS = poVRTDriver->CreateCopy(osTmpVRTFilename, poDS, FALSE, NULL, NULL, NULL);
            GDALClose(poVRTDS);
            GByte* pabyXML = VSIGetMemFileBuffer( osTmpVRTFilename, NULL, FALSE );
            CPLXMLNode* psXMLVRT = CPLParseXMLString((const char*)pabyXML);
            if( psXMLVRT )
            {
                CPLXMLNode* psXMLContentNode = 
                    CPLCreateXMLNode( psBox, CXT_Element, "DecodedGeoTIFF" );
                psXMLContentNode->psChild = psXMLVRT;
                CPLXMLNode* psPrev = NULL;
                for(CPLXMLNode* psIter = psXMLVRT->psChild; psIter; psIter = psIter->psNext)
                {
                    if( psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "VRTRasterBand") == 0 )
                    {
                        CPLXMLNode* psNext = psIter->psNext;
                        psIter->psNext = NULL;
                        CPLDestroyXMLNode(psIter);
                        if( psPrev )
                            psPrev->psNext = psNext;
                        else
                            break;
                        psIter = psPrev;
                    }
                    psPrev = psIter;
                }
            }

            VSIUnlink(osTmpVRTFilename);
            GDALClose(poDS);
        }
        VSIUnlink(osTmpFilename);
    }
}

static void DumpFTYPBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        if( nRemainingLength >= 4 )
        {
            char szBranding[5];
            memcpy(szBranding, pabyIter, 4);
            szBranding[4] = 0;
            AddField(psDecodedContent, "BR", 4, szBranding);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if( nRemainingLength >= 4 )
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, "MinV", nVal);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        int nCLIndex = 0;
        while( nRemainingLength >= 4 )
        {
            char szBranding[5];
            memcpy(szBranding, pabyIter, 4);
            szBranding[4] = 0;
            AddField(psDecodedContent,
                        CPLSPrintf("CL%d", nCLIndex),
                        4, szBranding);
            pabyIter += 4;
            nRemainingLength -= 4;
            nCLIndex ++;
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpIHDRBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        if( nRemainingLength >= 4 )
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, "HEIGHT", nVal);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if( nRemainingLength >= 4 )
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, "WIDTH", nVal);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            AddField(psDecodedContent, "NC", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "BPC", *pabyIter,
                        GetInterpretationOfBPC(*pabyIter));
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "C", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "UnkC", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "IPR", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpBPCCBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        int nBPCIndex = 0;
        while( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent,
                        CPLSPrintf("BPC%d", nBPCIndex),
                        *pabyIter,
                        GetInterpretationOfBPC(*pabyIter));
            nBPCIndex ++;
            pabyIter += 1;
            nRemainingLength -= 1;
        }
    }
    CPLFree(pabyBoxData);
}

static void DumpCOLRBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GByte nMeth;
        if( nRemainingLength >= 1 )
        {
            nMeth = *pabyIter;
            AddField(psDecodedContent, "METH", nMeth,
                        (nMeth == 0) ? "Enumerated Colourspace":
                        (nMeth == 0) ? "Restricted ICC profile": NULL);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "PREC", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "APPROX", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 4 )
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, "EnumCS", nVal,
                        (nVal == 16) ? "sRGB" :
                        (nVal == 17) ? "greyscale":
                        (nVal == 18) ? "sYCC" : NULL);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpPCLRBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GUInt16 NE;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            NE = nVal;
            AddField(psDecodedContent, "NE", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        GByte NPC;
        if( nRemainingLength >= 1 )
        {
            NPC = *pabyIter;
            AddField(psDecodedContent, "NPC", NPC);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        int b8BitOnly = TRUE;
        for(int i=0;i<NPC;i++)
        {
            if( nRemainingLength >= 1 )
            {
                b8BitOnly &= (*pabyIter == 7);
                AddField(psDecodedContent,
                            CPLSPrintf("B%d", i),
                            *pabyIter,
                            GetInterpretationOfBPC(*pabyIter));
                pabyIter += 1;
                nRemainingLength -= 1;
            }
        }
        if( b8BitOnly )
        {
            for(int j=0;j<NE;j++)
            {
                for(int i=0;i<NPC;i++)
                {
                    if( nRemainingLength >= 1 )
                    {
                        AddField(psDecodedContent,
                                CPLSPrintf("C_%d_%d", j, i),
                                *pabyIter);
                        pabyIter += 1;
                        nRemainingLength -= 1;
                    }
                }
            }
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpCMAPBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        int nIndex = 0;
        while( nRemainingLength >= 2 + 1 + 1 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            AddField(psDecodedContent,
                        CPLSPrintf("CMP%d", nIndex),
                        nVal);
            pabyIter += 2;
            nRemainingLength -= 2;

            AddField(psDecodedContent,
                        CPLSPrintf("MTYP%d", nIndex),
                        *pabyIter,
                        (*pabyIter == 0) ? "Direct use":
                        (*pabyIter == 1) ? "Palette mapping": NULL);
            pabyIter += 1;
            nRemainingLength -= 1;

            AddField(psDecodedContent,
                        CPLSPrintf("PCOL%d", nIndex),
                        *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;

            nIndex ++;
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpCDEFBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GUInt16 nChannels;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            nChannels = nVal;
            CPL_MSBPTR16(&nVal);
            AddField(psDecodedContent, "N", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        for( int i=0; i < nChannels; i++ )
        {
            if( nRemainingLength >= 2 )
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent,
                            CPLSPrintf("Cn%d", i),
                            nVal);
                pabyIter += 2;
                nRemainingLength -= 2;
            }
            if( nRemainingLength >= 2 )
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent,
                            CPLSPrintf("Typ%d", i),
                            nVal,
                            (nVal == 0) ? "Colour channel":
                            (nVal == 1) ? "Opacity channel":
                            (nVal == 2) ? "Premultiplied opacity":
                            (nVal == 65535) ? "Not specified" : NULL);
                pabyIter += 2;
                nRemainingLength -= 2;
            }
            if( nRemainingLength >= 2 )
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent,
                            CPLSPrintf("Assoc%d", i),
                            nVal,
                            (nVal == 0) ? "Associated to the whole image":
                            (nVal == 65535) ? "Not associated with a particular colour":
                            "Associated with a particular colour");
                pabyIter += 2;
                nRemainingLength -= 2;
            }
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpRESxBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    char chC = oBox.GetType()[3];
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GUInt16 nNumV = 0, nNumH = 0, nDenomV = 1, nDenomH = 1, nExpV = 0, nExpH = 0;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nNumV = nVal;
            AddField(psDecodedContent, CPLSPrintf("VR%cN", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nDenomV = nVal;
            AddField(psDecodedContent, CPLSPrintf("VR%cD", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nNumH = nVal;
            AddField(psDecodedContent, CPLSPrintf("HR%cN", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nDenomH = nVal;
            AddField(psDecodedContent, CPLSPrintf("HR%cD", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, CPLSPrintf("VR%cE", chC), *pabyIter);
            nExpV = *pabyIter;
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, CPLSPrintf("HR%cE", chC), *pabyIter);
            nExpH = *pabyIter;
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength == 0 )
        {
            CPLCreateXMLElementAndValue(psDecodedContent, "VRes",
                CPLSPrintf("%.03f", 1.0 * nNumV / nDenomV * pow(10.0, nExpV)));
            CPLCreateXMLElementAndValue(psDecodedContent, "HRes",
                CPLSPrintf("%.03f", 1.0 * nNumH / nDenomH * pow(10.0, nExpH)));
        }
        else if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpRREQBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GByte ML = 0;
        if( nRemainingLength >= 1 )
        {
            ML = *pabyIter;
            AddField(psDecodedContent, "ML", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= ML )
        {
            CPLString osHex("0x");
            for(int i=0;i<ML;i++)
            {
                osHex += CPLSPrintf("%02X", *pabyIter);
                pabyIter += 1;
                nRemainingLength -= 1;
            }
            AddHexField(psDecodedContent, "FUAM", (int)ML, osHex.c_str());
        }
        if( nRemainingLength >= ML )
        {
            CPLString osHex("0x");
            for(int i=0;i<ML;i++)
            {
                osHex += CPLSPrintf("%02X", *pabyIter);
                pabyIter += 1;
                nRemainingLength -= 1;
            }
            AddHexField(psDecodedContent, "DCM", (int)ML, osHex.c_str());
        }
        GUInt16 NSF = 0;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            NSF = nVal;
            AddField(psDecodedContent, "NSF", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        for(int iNSF=0;iNSF<NSF;iNSF++)
        {
            if( nRemainingLength >= 2 )
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent,
                            CPLSPrintf("SF%d", iNSF), nVal,
                            GetStandardFieldString(nVal));
                pabyIter += 2;
                nRemainingLength -= 2;
            }
            if( nRemainingLength >= ML )
            {
                CPLString osHex("0x");
                for(int i=0;i<ML;i++)
                {
                    osHex += CPLSPrintf("%02X", *pabyIter);
                    pabyIter += 1;
                    nRemainingLength -= 1;
                }
                AddHexField(psDecodedContent,
                            CPLSPrintf("SM%d", iNSF),
                            (int)ML, osHex.c_str());
            }
        }
        GUInt16 NVF = 0;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            NVF = nVal;
            AddField(psDecodedContent, "NVF", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        for(int iNVF=0;iNVF<NVF;iNVF++)
        {
            if( nRemainingLength >= 16 )
            {
                CPLString osHex("0x");
                for(int i=0;i<16;i++)
                {
                    osHex += CPLSPrintf("%02X", *pabyIter);
                    pabyIter += 1;
                    nRemainingLength -= 1;
                }
                AddHexField(psDecodedContent,
                            CPLSPrintf("VF%d", iNVF),
                            (int)ML, osHex.c_str());
            }
            if( nRemainingLength >= ML )
            {
                CPLString osHex("0x");
                for(int i=0;i<ML;i++)
                {
                    osHex += CPLSPrintf("%02X", *pabyIter);
                    pabyIter += 1;
                    nRemainingLength -= 1;
                }
                AddHexField(psDecodedContent,
                            CPLSPrintf("VM%d", iNVF),
                            (int)ML, osHex.c_str());
            }
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static
void GDALGetJPEG2000StructureInternal(CPLXMLNode* psParent,
                                      VSILFILE* fp,
                                      GDALJP2Box* poParentBox,
                                      char** papszOptions)
{
    static const char* szHex = "0123456789ABCDEF";

    GDALJP2Box oBox( fp );
    if( oBox.ReadFirstChild(poParentBox) )
    {
        while( strlen(oBox.GetType()) > 0 )
        {
            GIntBig nBoxDataLength = oBox.GetDataLength();
            const char* pszBoxType = oBox.GetType();

            CPLXMLNode* psBox = CPLCreateXMLNode( psParent, CXT_Element, "JP2Box" );
            CPLAddXMLAttributeAndValue(psBox, "name", pszBoxType );
            CPLAddXMLAttributeAndValue(psBox, "box_offset",
                                       CPLSPrintf(CPL_FRMT_GIB, oBox.GetBoxOffset() )  );
            CPLAddXMLAttributeAndValue(psBox, "box_length",
                                       CPLSPrintf(CPL_FRMT_GIB, oBox.GetBoxLength() ) );
            CPLAddXMLAttributeAndValue(psBox, "data_offset",
                                       CPLSPrintf(CPL_FRMT_GIB, oBox.GetDataOffset() ) );
            CPLAddXMLAttributeAndValue(psBox, "data_length",
                                       CPLSPrintf(CPL_FRMT_GIB, nBoxDataLength ) );

            if( oBox.IsSuperBox() )
            {
                GDALGetJPEG2000StructureInternal(psBox, fp, &oBox, papszOptions);
            }
            else
            {
                if( strcmp(pszBoxType, "uuid") == 0 )
                {
                    char* pszBinaryContent = (char*)VSIMalloc( 2 * 16 + 1 );
                    const GByte* pabyUUID = oBox.GetUUID();
                    for(int i=0;i<16;i++)
                    {
                        pszBinaryContent[2*i] = szHex[pabyUUID[i] >> 4];
                        pszBinaryContent[2*i+1] = szHex[pabyUUID[i] & 0xf];
                    }
                    pszBinaryContent[2*16] = '\0';
                    CPLXMLNode* psUUIDNode =
                                CPLCreateXMLNode( psBox, CXT_Element, "UUID" );
                    if( GDALJP2Metadata::IsUUID_MSI(pabyUUID) )
                        CPLAddXMLAttributeAndValue(psUUIDNode, "description", "GeoTIFF" );
                    else if( GDALJP2Metadata::IsUUID_XMP(pabyUUID) )
                        CPLAddXMLAttributeAndValue(psUUIDNode, "description", "XMP" );
                    CPLCreateXMLNode( psUUIDNode, CXT_Text, pszBinaryContent);
                    VSIFree(pszBinaryContent);
                }

                if( CSLFetchBoolean(papszOptions, "BINARY_CONTENT", FALSE) &&
                    strcmp(pszBoxType, "jp2c") != 0 &&
                    nBoxDataLength < 100 * 1024 )
                {
                    CPLXMLNode* psBinaryContent = CPLCreateXMLNode( psBox, CXT_Element, "BinaryContent" );
                    GByte* pabyBoxData = oBox.ReadBoxData();
                    int nBoxLength = (int)nBoxDataLength;
                    char* pszBinaryContent = (char*)VSIMalloc( 2 * nBoxLength + 1 );
                    if( pabyBoxData && pszBinaryContent )
                    {
                        for(int i=0;i<nBoxLength;i++)
                        {
                            pszBinaryContent[2*i] = szHex[pabyBoxData[i] >> 4];
                            pszBinaryContent[2*i+1] = szHex[pabyBoxData[i] & 0xf];
                        }
                        pszBinaryContent[2*nBoxLength] = '\0';
                        CPLCreateXMLNode( psBinaryContent, CXT_Text, pszBinaryContent );
                    }
                    CPLFree(pabyBoxData);
                    VSIFree(pszBinaryContent);
                }

                if( CSLFetchBoolean(papszOptions, "TEXT_CONTENT", FALSE) &&
                    strcmp(pszBoxType, "jp2c") != 0 &&
                    nBoxDataLength < 100 * 1024 )
                {
                    GByte* pabyBoxData = oBox.ReadBoxData();
                    if( pabyBoxData )
                    {
                        if( CPLIsUTF8((const char*)pabyBoxData, -1) &&
                            (int)strlen((const char*)pabyBoxData) + 2 >= nBoxDataLength  )
                        {
                            CPLXMLNode* psXMLContentBox = NULL;
                            if( ((const char*)pabyBoxData)[0] ==  '<' )
                            {
                                CPLPushErrorHandler(CPLQuietErrorHandler);
                                psXMLContentBox = CPLParseXMLString((const char*)pabyBoxData);
                                CPLPopErrorHandler();
                            }
                            if( psXMLContentBox )
                            {
                                CPLXMLNode* psXMLContentNode = 
                                    CPLCreateXMLNode( psBox, CXT_Element, "XMLContent" );
                                psXMLContentNode->psChild = psXMLContentBox;
                            }
                            else
                            {
                                CPLCreateXMLNode( 
                                    CPLCreateXMLNode( psBox, CXT_Element, "TextContent" ),
                                        CXT_Text, (const char*)pabyBoxData);
                            }
                        }
                    }
                    CPLFree(pabyBoxData);
                }

                if( strcmp(pszBoxType, "uuid") == 0 &&
                    GDALJP2Metadata::IsUUID_MSI(oBox.GetUUID()) )
                {
                    DumpGeoTIFFBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "ftyp") == 0 )
                {
                    DumpFTYPBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "ihdr") == 0 )
                {
                    DumpIHDRBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "bpcc") == 0 )
                {
                    DumpBPCCBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "colr") == 0 )
                {
                    DumpCOLRBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "pclr") == 0 )
                {
                    DumpPCLRBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "cmap") == 0 )
                {
                    DumpCMAPBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "cdef") == 0 )
                {
                    DumpCDEFBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "resc") == 0 ||
                         strcmp(pszBoxType, "resd") == 0)
                {
                    DumpRESxBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "rreq") == 0 )
                {
                    DumpRREQBox(psBox, oBox);
                }
            }

            if (!oBox.ReadNextChild(poParentBox))
                break;
        }
    }
}

/************************************************************************/
/*                        GDALGetJPEG2000Structure()                    */
/************************************************************************/

static const unsigned char jpc_header[] = {0xff,0x4f};
static const unsigned char jp2_box_jp[] = {0x6a,0x50,0x20,0x20}; /* 'jP  ' */

/** Dump the structure of a JPEG2000 file as a XML tree.
 *
 * @param pszFilename filename.
 * @param papszOptions NULL terminated list of options, or NULL.
 *                     Allowed options are BINARY_CONTENT=YES, TEXT_CONTENT=YES.
 * @return XML tree (to be freed with CPLDestroyXMLNode()) or NULL in case
 *         of error
 * @since GDAL 2.0
 */

CPLXMLNode* GDALGetJPEG2000Structure(const char* pszFilename,
                                     char** papszOptions)
{
    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if( fp == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s", pszFilename);
        return NULL;
    }
    GByte abyHeader[16];
    if( VSIFReadL(abyHeader, 16, 1, fp) != 1 ||
        (/*memcmp(abyHeader, jpc_header, sizeof(jpc_header)) != 0 &&*/
         memcmp(abyHeader + 4, jp2_box_jp, sizeof(jp2_box_jp)) != 0) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s is not a JPEG2000 file", pszFilename);
        VSIFCloseL(fp);
        return NULL;
    }

    CPLXMLNode* psParent = CPLCreateXMLNode( NULL, CXT_Element, "JP2File" );
    GDALGetJPEG2000StructureInternal(psParent, fp, NULL, papszOptions );

    VSIFCloseL(fp);
    return psParent;
}
