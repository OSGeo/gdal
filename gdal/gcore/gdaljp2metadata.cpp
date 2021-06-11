
/******************************************************************************
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

#include "cpl_port.h"
#include "gdaljp2metadata.h"
#include "gdaljp2metadatagenerator.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "gdaljp2metadatagenerator.h"
#include "gt_wkt_srs_for_gdal.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogrgeojsonreader.h"

/*! @cond Doxygen_Suppress */

CPL_CVSID("$Id$")

static const unsigned char msi_uuid2[16] = {
    0xb1,0x4b,0xf8,0xbd,0x08,0x3d,0x4b,0x43,
    0xa5,0xae,0x8c,0xd7,0xd5,0xa6,0xce,0x03 };

static const unsigned char msig_uuid[16] = {
    0x96,0xA9,0xF1,0xF1,0xDC,0x98,0x40,0x2D,
    0xA7,0xAE,0xD6,0x8E,0x34,0x45,0x18,0x09 };

static const unsigned char xmp_uuid[16] = {
    0xBE,0x7A,0xCF,0xCB,0x97,0xA9,0x42,0xE8,
    0x9C,0x71,0x99,0x94,0x91,0xE3,0xAF,0xAC };

struct _GDALJP2GeoTIFFBox
{
    int    nGeoTIFFSize;
    GByte  *pabyGeoTIFFData;
};

constexpr int MAX_JP2GEOTIFF_BOXES = 2;

/************************************************************************/
/*                          GDALJP2Metadata()                           */
/************************************************************************/

GDALJP2Metadata::GDALJP2Metadata() :
    nGeoTIFFBoxesCount(0),
    pasGeoTIFFBoxes(nullptr),
    nMSIGSize(0),
    pabyMSIGData(nullptr),
    papszGMLMetadata(nullptr),
    bHaveGeoTransform(false),
    adfGeoTransform{0.0, 1.0, 0.0, 0.0, 0.0, 1.0},
    bPixelIsPoint(false),
    nGCPCount(0),
    pasGCPList(nullptr),
    papszRPCMD(nullptr),
    papszMetadata(nullptr),
    pszXMPMetadata(nullptr),
    pszGDALMultiDomainMetadata(nullptr),
    pszXMLIPR(nullptr)
{
}

/************************************************************************/
/*                          ~GDALJP2Metadata()                          */
/************************************************************************/

GDALJP2Metadata::~GDALJP2Metadata()

{
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
    CSLDestroy(papszRPCMD);

    for( int i = 0; i < nGeoTIFFBoxesCount; ++i )
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

int GDALJP2Metadata::ReadAndParse( const char *pszFilename, int nGEOJP2Index,
                                   int nGMLJP2Index, int nMSIGIndex,
                                   int nWorldFileIndex, int *pnIndexUsed  )

{
    VSILFILE *fpLL = VSIFOpenL( pszFilename, "rb" );
    if( fpLL == nullptr )
    {
        CPLDebug( "GDALJP2Metadata", "Could not even open %s.",
                  pszFilename );

        return FALSE;
    }

    int nIndexUsed = -1;
    bool bRet = CPL_TO_BOOL(ReadAndParse( fpLL, nGEOJP2Index, nGMLJP2Index,
                                          nMSIGIndex, &nIndexUsed ));
    CPL_IGNORE_RET_VAL(VSIFCloseL( fpLL ));

/* -------------------------------------------------------------------- */
/*      If we still don't have a geotransform, look for a world         */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    if( nWorldFileIndex >= 0 &&
        ((bHaveGeoTransform && nWorldFileIndex < nIndexUsed) ||
         !bHaveGeoTransform) )
    {
        bHaveGeoTransform = CPL_TO_BOOL(
            GDALReadWorldFile( pszFilename, nullptr, adfGeoTransform )
            || GDALReadWorldFile( pszFilename, ".wld", adfGeoTransform ) );
        bRet |= bHaveGeoTransform;
    }

    if( pnIndexUsed )
        *pnIndexUsed = nIndexUsed;

    return bRet;
}

int GDALJP2Metadata::ReadAndParse( VSILFILE *fpLL, int nGEOJP2Index,
                                   int nGMLJP2Index, int nMSIGIndex,
                                   int *pnIndexUsed )

{
    ReadBoxes( fpLL );

/* -------------------------------------------------------------------- */
/*      Try JP2GeoTIFF, GML and finally MSIG in specified order.        */
/* -------------------------------------------------------------------- */
    std::set<int> aoSetPriorities;
    if( nGEOJP2Index >= 0 ) aoSetPriorities.insert(nGEOJP2Index);
    if( nGMLJP2Index >= 0 ) aoSetPriorities.insert(nGMLJP2Index);
    if( nMSIGIndex >= 0 ) aoSetPriorities.insert(nMSIGIndex);
    std::set<int>::iterator oIter = aoSetPriorities.begin();
    for( ; oIter != aoSetPriorities.end(); ++oIter )
    {
        int nIndex = *oIter;
        if( (nIndex == nGEOJP2Index && ParseJP2GeoTIFF()) ||
            (nIndex == nGMLJP2Index && ParseGMLCoverageDesc()) ||
            (nIndex == nMSIGIndex && ParseMSIG() ) )
        {
            if( pnIndexUsed )
                *pnIndexUsed = nIndex;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Return success either either of projection or geotransform      */
/*      or gcps.                                                        */
/* -------------------------------------------------------------------- */
    return bHaveGeoTransform
        || nGCPCount > 0
        || !m_oSRS.IsEmpty()
        || papszRPCMD != nullptr;
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

            if( !oSubChildBox.ReadFirstChild( &oChildBox ) )
                break;

            char *pszLabel = nullptr;
            char *pszXML = nullptr;

            while( strlen(oSubChildBox.GetType()) > 0 )
            {
                if( EQUAL(oSubChildBox.GetType(),"lbl ") )
                    pszLabel = reinterpret_cast<char *>(oSubChildBox.ReadBoxData());
                else if( EQUAL(oSubChildBox.GetType(),"xml ") )
                {
                    pszXML =
                        reinterpret_cast<char *>( oSubChildBox.ReadBoxData() );
                    GIntBig nXMLLength = oSubChildBox.GetDataLength();

                    // Some GML data contains \0 instead of \n.
                    // See http://trac.osgeo.org/gdal/ticket/5760
                    // TODO(schwehr): Explain the numbers in the next line.
                    if( pszXML != nullptr && nXMLLength < 100 * 1024 * 1024 )
                    {
                        // coverity[tainted_data].
                        for( GIntBig i = nXMLLength - 1; i >= 0; --i )
                        {
                            if( pszXML[i] == '\0' )
                                --nXMLLength;
                            else
                                break;
                        }
                        // coverity[tainted_data]
                        GIntBig i = 0;  // Used after for.
                        for( ; i < nXMLLength; ++i )
                        {
                            if( pszXML[i] == '\0' )
                                break;
                        }
                        if( i < nXMLLength )
                        {
                            CPLPushErrorHandler(CPLQuietErrorHandler);
                            CPLXMLTreeCloser psNode(CPLParseXMLString(pszXML));
                            CPLPopErrorHandler();
                            if( psNode == nullptr )
                            {
                                CPLDebug(
                                    "GMLJP2",
                                    "GMLJP2 data contains nul characters "
                                    "inside content. Replacing them by \\n");
                                // coverity[tainted_data]
                                for( GIntBig j = 0; j < nXMLLength; ++j )
                                {
                                    if( pszXML[j] == '\0' )
                                        pszXML[j] = '\n';
                                }
                            }
                        }
                    }
                }

                if( !oSubChildBox.ReadNextChild( &oChildBox ) )
                    break;
            }

            if( pszLabel != nullptr && pszXML != nullptr )
            {
                papszGMLMetadata = CSLSetNameValue( papszGMLMetadata,
                                                    pszLabel, pszXML );

                if( strcmp(pszLabel, "gml.root-instance") == 0 &&
                    pszGDALMultiDomainMetadata == nullptr &&
                    strstr(pszXML, "GDALMultiDomainMetadata") != nullptr )
                {
                    CPLXMLTreeCloser psTree(CPLParseXMLString(pszXML));
                    if( psTree != nullptr )
                    {
                        CPLXMLNode* psGDALMDMD =
                            CPLSearchXMLNode(psTree.get(), "GDALMultiDomainMetadata");
                        if( psGDALMDMD )
                            pszGDALMultiDomainMetadata =
                                CPLSerializeXMLTree(psGDALMDMD);
                    }
                }
            }

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

    if (!oBox.ReadFirst())
        return FALSE;

    int iBox = 0;
    while( strlen(oBox.GetType()) > 0 )
    {
#ifdef DEBUG
        if (CPLTestBool(CPLGetConfigOption("DUMP_JP2_BOXES", "NO")))
            oBox.DumpReadable(stderr);
#endif

/* -------------------------------------------------------------------- */
/*      Collect geotiff box.                                            */
/* -------------------------------------------------------------------- */
        if( EQUAL(oBox.GetType(),"uuid")
            && memcmp( oBox.GetUUID(), msi_uuid2, 16 ) == 0 )
        {
            // Erdas JPEG2000 files sometimes contain 2 GeoTIFF UUID boxes. One
            // that is correct, another one that does not contain correct
            // georeferencing. Fetch at most 2 of them for later analysis.
            if( nGeoTIFFBoxesCount == MAX_JP2GEOTIFF_BOXES )
            {
                CPLDebug( "GDALJP2",
                          "Too many UUID GeoTIFF boxes. Ignoring this one" );
            }
            else
            {
                const int nGeoTIFFSize =
                    static_cast<int>( oBox.GetDataLength() );
                GByte* pabyGeoTIFFData = oBox.ReadBoxData();
                if( pabyGeoTIFFData == nullptr )
                {
                    CPLDebug( "GDALJP2",
                              "Cannot read data for UUID GeoTIFF box" );
                }
                else
                {
                    pasGeoTIFFBoxes = static_cast<GDALJP2GeoTIFFBox *>(
                        CPLRealloc(
                            pasGeoTIFFBoxes,
                            sizeof(GDALJP2GeoTIFFBox) *
                                (nGeoTIFFBoxesCount + 1) ) );
                    pasGeoTIFFBoxes[nGeoTIFFBoxesCount].nGeoTIFFSize =
                        nGeoTIFFSize;
                    pasGeoTIFFBoxes[nGeoTIFFBoxesCount].pabyGeoTIFFData =
                        pabyGeoTIFFData;
                    ++nGeoTIFFBoxesCount;
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
                nMSIGSize = static_cast<int>( oBox.GetDataLength() );
                pabyMSIGData = oBox.ReadBoxData();

                if( nMSIGSize < 70
                    || pabyMSIGData == nullptr
                    || memcmp( pabyMSIGData, "MSIG/", 5 ) != 0 )
                {
                    CPLFree( pabyMSIGData );
                    pabyMSIGData = nullptr;
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
            if( pszXMPMetadata == nullptr )
            {
                pszXMPMetadata = reinterpret_cast<char *>(oBox.ReadBoxData());
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
                char *pszLabel = reinterpret_cast<char *>(oSubBox.ReadBoxData());
                if( pszLabel != nullptr && EQUAL(pszLabel,"gml.data") )
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

            char *pszXML = reinterpret_cast<char *>(oBox.ReadBoxData());
            if( pszXML != nullptr &&
                STARTS_WITH(pszXML, "<GDALMultiDomainMetadata>") )
            {
                if( pszGDALMultiDomainMetadata == nullptr )
                {
                    pszGDALMultiDomainMetadata = pszXML;
                    pszXML = nullptr;
                }
                else
                {
                    CPLDebug(
                        "GDALJP2",
                        "Too many GDAL metadata boxes. Ignoring this one");
                }
            }
            else if( pszXML != nullptr )
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

                    // We will use either the resd or resc box, which ever
                    // happens to be first.  Should we prefer resd?
                    unsigned char *pabyResData = nullptr;
                    if( oResBox.GetDataLength() == 10 &&
                        (pabyResData = oResBox.ReadBoxData()) != nullptr )
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
                        const double dfVertRes =
                            (nVertNum / static_cast<double>(nVertDen)) *
                            pow(10.0, nVertExp) / 100;
                        const double dfHorzRes =
                            (nHorzNum / static_cast<double>(nHorzDen)) *
                            pow(10.0,nHorzExp)/100;
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
            if( pszXMLIPR == nullptr )
            {
                pszXMLIPR = reinterpret_cast<char*>(oBox.ReadBoxData());
                CPLXMLTreeCloser psNode(CPLParseXMLString(pszXMLIPR));
                if( psNode == nullptr )
                {
                    CPLFree(pszXMLIPR);
                    pszXMLIPR = nullptr;
                }
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
    if(! CPLTestBool(CPLGetConfigOption("GDAL_USE_GEOJP2", "TRUE")) )
        return FALSE;

    bool abValidProjInfo[MAX_JP2GEOTIFF_BOXES] = { false };
    OGRSpatialReferenceH ahSRS[MAX_JP2GEOTIFF_BOXES] = { nullptr };
    double aadfGeoTransform[MAX_JP2GEOTIFF_BOXES][6];
    int anGCPCount[MAX_JP2GEOTIFF_BOXES] = { 0 };
    GDAL_GCP    *apasGCPList[MAX_JP2GEOTIFF_BOXES] = { nullptr };
    int abPixelIsPoint[MAX_JP2GEOTIFF_BOXES] = { 0 };
    char** apapszRPCMD[MAX_JP2GEOTIFF_BOXES] = { nullptr };

    const int nMax = std::min(nGeoTIFFBoxesCount, MAX_JP2GEOTIFF_BOXES);
    for( int i = 0; i < nMax; ++i )
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
                               &ahSRS[i], aadfGeoTransform[i],
                               &anGCPCount[i], &apasGCPList[i],
                               &abPixelIsPoint[i], &apapszRPCMD[i] ) == CE_None )
        {
            if( ahSRS[i] != nullptr )
                abValidProjInfo[i] = true;
        }
    }

    // Detect which box is the better one.
    int iBestIndex = -1;
    for( int i = 0; i < nMax; ++i )
    {
        if( abValidProjInfo[i] && iBestIndex < 0 )
        {
            iBestIndex = i;
        }
        else if( abValidProjInfo[i] && ahSRS[i] != nullptr )
        {
            // Anything else than a LOCAL_CS will probably be better.
            if( OSRIsLocal(ahSRS[iBestIndex]) )
                iBestIndex = i;
        }
    }

    if( iBestIndex < 0 )
    {
        for( int i = 0; i < nMax; ++i )
        {
            if( aadfGeoTransform[i][0] != 0
                || aadfGeoTransform[i][1] != 1
                || aadfGeoTransform[i][2] != 0
                || aadfGeoTransform[i][3] != 0
                || aadfGeoTransform[i][4] != 0
                || aadfGeoTransform[i][5] != 1
                || anGCPCount[i] > 0
                || apapszRPCMD[i] != nullptr )
            {
                iBestIndex = i;
            }
        }
    }

    if( iBestIndex >= 0 )
    {
        m_oSRS.Clear();
        if( ahSRS[iBestIndex] )
            m_oSRS = *(OGRSpatialReference::FromHandle(ahSRS[iBestIndex]));
        m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        memcpy(adfGeoTransform, aadfGeoTransform[iBestIndex], 6 * sizeof(double));
        nGCPCount = anGCPCount[iBestIndex];
        pasGCPList = apasGCPList[iBestIndex];
        bPixelIsPoint = CPL_TO_BOOL(abPixelIsPoint[iBestIndex]);
        papszRPCMD = apapszRPCMD[iBestIndex];

        if( adfGeoTransform[0] != 0
            || adfGeoTransform[1] != 1
            || adfGeoTransform[2] != 0
            || adfGeoTransform[3] != 0
            || adfGeoTransform[4] != 0
            || adfGeoTransform[5] != 1 )
            bHaveGeoTransform = true;

        if( ahSRS[iBestIndex] )
        {
            char* pszWKT = nullptr;
            m_oSRS.exportToWkt(&pszWKT);
            CPLDebug( "GDALJP2Metadata",
                "Got projection from GeoJP2 (geotiff) box (%d): %s",
                iBestIndex, pszWKT ? pszWKT : "(null)" );
            CPLFree(pszWKT);
        }
    }

    // Cleanup unused boxes.
    for( int i = 0; i < nMax; ++i )
    {
        if( i != iBestIndex )
        {
            if( anGCPCount[i] > 0 )
            {
                GDALDeinitGCPs( anGCPCount[i], apasGCPList[i] );
                CPLFree( apasGCPList[i] );
            }
            CSLDestroy( apapszRPCMD[i] );
        }
        OSRDestroySpatialReference( ahSRS[i] );
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

    bHaveGeoTransform = true;

    return TRUE;
}

/************************************************************************/
/*                         GetDictionaryItem()                          */
/************************************************************************/

static CPLXMLNode *
GetDictionaryItem( char **papszGMLMetadata, const char *pszURN )

{
    char *pszLabel = nullptr;

    if( STARTS_WITH_CI(pszURN, "urn:jp2k:xml:") )
        pszLabel = CPLStrdup( pszURN + 13 );
    else if( STARTS_WITH_CI(pszURN, "urn:ogc:tc:gmljp2:xml:") )
        pszLabel = CPLStrdup( pszURN + 22 );
    else if( STARTS_WITH_CI(pszURN, "gmljp2://xml/") )
        pszLabel = CPLStrdup( pszURN + 13 );
    else
        pszLabel = CPLStrdup( pszURN );

/* -------------------------------------------------------------------- */
/*      Split out label and fragment id.                                */
/* -------------------------------------------------------------------- */
    const char *pszFragmentId = nullptr;

    {
        int i = 0;  // Used after for.
        for( ; pszLabel[i] != '#'; ++i )
        {
            if( pszLabel[i] == '\0' )
            {
                CPLFree(pszLabel);
                return nullptr;
            }
        }

        pszFragmentId = pszLabel + i + 1;
        pszLabel[i] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Can we find an XML box with the desired label?                  */
/* -------------------------------------------------------------------- */
    const char *pszDictionary =
        CSLFetchNameValue( papszGMLMetadata, pszLabel );

    if( pszDictionary == nullptr )
    {
        CPLFree(pszLabel);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try and parse the dictionary.                                   */
/* -------------------------------------------------------------------- */
    CPLXMLTreeCloser psDictTree(CPLParseXMLString( pszDictionary ));

    if( psDictTree == nullptr )
    {
        CPLFree(pszLabel);
        return nullptr;
    }

    CPLStripXMLNamespace( psDictTree.get(), nullptr, TRUE );

    CPLXMLNode *psDictRoot = CPLSearchXMLNode( psDictTree.get(), "=Dictionary" );

    if( psDictRoot == nullptr )
    {
        CPLFree(pszLabel);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Search for matching id.                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psEntry, *psHit = nullptr;
    for( psEntry = psDictRoot->psChild;
         psEntry != nullptr && psHit == nullptr;
         psEntry = psEntry->psNext )
    {
        const char *pszId;

        if( psEntry->eType != CXT_Element )
            continue;

        if( !EQUAL(psEntry->pszValue,"dictionaryEntry") )
            continue;

        if( psEntry->psChild == nullptr )
            continue;

        pszId = CPLGetXMLValue( psEntry->psChild, "id", "" );

        if( EQUAL(pszId, pszFragmentId) )
            psHit = CPLCloneXMLTree( psEntry->psChild );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pszLabel );

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
    CPLXMLTreeCloser psDictEntry(GetDictionaryItem( papszGMLMetadata, pszURN ));

    if( psDictEntry == nullptr )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Reserialize this fragment.                                      */
/* -------------------------------------------------------------------- */
    char *pszDictEntryXML = CPLSerializeXMLTree( psDictEntry.get() );
    psDictEntry.reset();

/* -------------------------------------------------------------------- */
/*      Try to convert into an OGRSpatialReference.                     */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    bool bSuccess = false;

    if( oSRS.importFromXML( pszDictEntryXML ) == OGRERR_NONE )
    {
        m_oSRS = oSRS;
        m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        bSuccess = true;
    }

    CPLFree( pszDictEntryXML );

    return bSuccess;
}

/************************************************************************/
/*                        ParseGMLCoverageDesc()                        */
/************************************************************************/

int GDALJP2Metadata::ParseGMLCoverageDesc()

{
    if(! CPLTestBool(CPLGetConfigOption("GDAL_USE_GMLJP2", "TRUE")) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do we have an XML doc that is apparently a coverage             */
/*      description?                                                    */
/* -------------------------------------------------------------------- */
    const char *pszCoverage = CSLFetchNameValue( papszGMLMetadata,
                                                 "gml.root-instance" );

    if( pszCoverage == nullptr )
        return FALSE;

    CPLDebug( "GDALJP2Metadata", "Found GML Box:\n%s", pszCoverage );

/* -------------------------------------------------------------------- */
/*      Try parsing the XML.  Wipe any namespace prefixes.              */
/* -------------------------------------------------------------------- */
    CPLXMLTreeCloser psXML(CPLParseXMLString( pszCoverage ));

    if( psXML == nullptr )
        return FALSE;

    CPLStripXMLNamespace( psXML.get(), nullptr, TRUE );

/* -------------------------------------------------------------------- */
/*      Isolate RectifiedGrid.  Eventually we will need to support      */
/*      other georeferencing objects.                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRG = CPLSearchXMLNode( psXML.get(), "=RectifiedGrid" );
    CPLXMLNode *psOriginPoint = nullptr;
    const char *pszOffset1 = nullptr;
    const char *pszOffset2 = nullptr;

    if( psRG != nullptr )
    {
        psOriginPoint = CPLGetXMLNode( psRG, "origin.Point" );

        CPLXMLNode *psOffset1 = CPLGetXMLNode( psRG, "offsetVector" );
        if( psOffset1 != nullptr )
        {
            pszOffset1 = CPLGetXMLValue( psOffset1, "", nullptr );
            pszOffset2 = CPLGetXMLValue( psOffset1->psNext, "=offsetVector",
                                         nullptr );
        }
    }

/* -------------------------------------------------------------------- */
/*      If we are missing any of the origin or 2 offsets then give up.  */
/* -------------------------------------------------------------------- */
    if( psOriginPoint == nullptr || pszOffset1 == nullptr || pszOffset2 == nullptr )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract origin location.                                        */
/* -------------------------------------------------------------------- */
    OGRPoint *poOriginGeometry = nullptr;

    OGRGeometry* poGeom = reinterpret_cast<OGRGeometry*>(
        OGR_G_CreateFromGMLTree( psOriginPoint ));

    if( poGeom != nullptr
        && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        poOriginGeometry = poGeom->toPoint();
    }
    else
    {
        delete poGeom;
    }

    // SRS?
    const char* pszSRSName = CPLGetXMLValue( psOriginPoint, "srsName", nullptr );

/* -------------------------------------------------------------------- */
/*      Extract offset(s)                                               */
/* -------------------------------------------------------------------- */
    bool bSuccess = false;

    char** papszOffset1Tokens =
        CSLTokenizeStringComplex( pszOffset1, " ,", FALSE, FALSE );
    char** papszOffset2Tokens =
        CSLTokenizeStringComplex( pszOffset2, " ,", FALSE, FALSE );

    if( CSLCount(papszOffset1Tokens) >= 2
        && CSLCount(papszOffset2Tokens) >= 2
        && poOriginGeometry != nullptr )
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

        bSuccess = true;
        bHaveGeoTransform = true;
    }

    CSLDestroy( papszOffset1Tokens );
    CSLDestroy( papszOffset2Tokens );

    if( poOriginGeometry != nullptr )
        delete poOriginGeometry;

/* -------------------------------------------------------------------- */
/*      If we still don't have an srsName, check for it on the          */
/*      boundedBy Envelope.  Some products                              */
/*      (i.e. EuropeRasterTile23.jpx) use this as the only srsName      */
/*      delivery vehicle.                                               */
/* -------------------------------------------------------------------- */
    if( pszSRSName == nullptr )
    {
        pszSRSName =
            CPLGetXMLValue( psXML.get(),
                            "=FeatureCollection.boundedBy.Envelope.srsName",
                            nullptr );
    }
/* -------------------------------------------------------------------- */
/*      Examples of DGIWG_Profile_of_JPEG2000_for_Georeference_Imagery.pdf */
/*      have srsName only on RectifiedGrid element.                     */
/* -------------------------------------------------------------------- */
    if( psRG != nullptr && pszSRSName == nullptr )
    {
        pszSRSName = CPLGetXMLValue( psRG,  "srsName", nullptr );
    }

/* -------------------------------------------------------------------- */
/*      If we have gotten a geotransform, then try to interpret the     */
/*      srsName.                                                        */
/* -------------------------------------------------------------------- */
    bool bNeedAxisFlip = false;

    OGRSpatialReference oSRS;
    if( bSuccess && pszSRSName != nullptr
        && m_oSRS.IsEmpty() )
    {
        if( STARTS_WITH_CI(pszSRSName, "epsg:") )
        {
            if( oSRS.SetFromUserInput( pszSRSName ) == OGRERR_NONE )
                m_oSRS = oSRS;
        }
        else if( (STARTS_WITH_CI(pszSRSName, "urn:")
                 && strstr(pszSRSName,":def:") != nullptr
                 && oSRS.importFromURN(pszSRSName) == OGRERR_NONE) ||
                 /* GMLJP2 v2.0 uses CRS URL instead of URN */
                 /* See e.g. http://schemas.opengis.net/gmljp2/2.0/examples/minimalInstance.xml */
                 (STARTS_WITH_CI(pszSRSName, "http://www.opengis.net/def/crs/")                  && oSRS.importFromCRSURL(pszSRSName) == OGRERR_NONE) )
        {
            m_oSRS = oSRS;

            // Per #2131
            if( oSRS.EPSGTreatsAsLatLong() || oSRS.EPSGTreatsAsNorthingEasting() )
            {
                CPLDebug( "GMLJP2", "Request axis flip for SRS=%s",
                          pszSRSName );
                bNeedAxisFlip = true;
            }
        }
        else if( !GMLSRSLookup( pszSRSName ) )
        {
            CPLDebug( "GDALJP2Metadata",
                      "Unable to evaluate SRSName=%s",
                      pszSRSName );
        }
    }

    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( !m_oSRS.IsEmpty() )
    {
        char* pszWKT = nullptr;
        m_oSRS.exportToWkt(&pszWKT);
        CPLDebug( "GDALJP2Metadata",
                  "Got projection from GML box: %s",
                 pszWKT ? pszWKT : "" );
        CPLFree(pszWKT);
    }

/* -------------------------------------------------------------------- */
/*      Do we need to flip the axes?                                    */
/* -------------------------------------------------------------------- */
    if( bNeedAxisFlip
        && CPLTestBool( CPLGetConfigOption( "GDAL_IGNORE_AXIS_ORIENTATION",
                                               "FALSE" ) ) )
    {
        bNeedAxisFlip = false;
        CPLDebug( "GMLJP2", "Suppressed axis flipping based on GDAL_IGNORE_AXIS_ORIENTATION." );
    }

    /* Some Pleiades files have explicit <gml:axisName>Easting</gml:axisName> */
    /* <gml:axisName>Northing</gml:axisName> to override default EPSG order */
    if( bNeedAxisFlip && psRG != nullptr )
    {
        int nAxisCount = 0;
        bool bFirstAxisIsEastOrLong = false;
        bool bSecondAxisIsNorthOrLat = false;
        for(CPLXMLNode* psIter = psRG->psChild; psIter != nullptr; psIter = psIter->psNext )
        {
            if( psIter->eType == CXT_Element && strcmp(psIter->pszValue, "axisName") == 0 &&
                psIter->psChild != nullptr && psIter->psChild->eType == CXT_Text )
            {
                if( nAxisCount == 0 &&
                    (STARTS_WITH_CI(psIter->psChild->pszValue, "EAST") ||
                     STARTS_WITH_CI(psIter->psChild->pszValue, "LONG") ) )
                {
                    bFirstAxisIsEastOrLong = true;
                }
                else if( nAxisCount == 1 &&
                         (STARTS_WITH_CI(psIter->psChild->pszValue, "NORTH") ||
                          STARTS_WITH_CI(psIter->psChild->pszValue, "LAT")) )
                {
                    bSecondAxisIsNorthOrLat = true;
                }
                ++nAxisCount;
            }
        }
        if( bFirstAxisIsEastOrLong && bSecondAxisIsNorthOrLat )
        {
            CPLDebug(
                "GMLJP2",
                "Disable axis flip because of explicit axisName disabling it" );
            bNeedAxisFlip = false;
        }
    }

    psXML.reset();
    psRG = nullptr;

    if( bNeedAxisFlip )
    {
        CPLDebug( "GMLJP2",
                  "Flipping axis orientation in GMLJP2 coverage description." );

        std::swap(adfGeoTransform[0], adfGeoTransform[3]);

        int swapWith1Index = 4;
        int swapWith2Index = 5;

        /* Look if we have GDAL_JP2K_ALT_OFFSETVECTOR_ORDER=TRUE as a XML comment */
        int bHasAltOffsetVectorOrderComment =
            strstr(pszCoverage, "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER=TRUE") != nullptr;

        if( bHasAltOffsetVectorOrderComment ||
            CPLTestBool( CPLGetConfigOption( "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER",
                                                "FALSE" ) ) )
        {
            swapWith1Index = 5;
            swapWith2Index = 4;
            CPLDebug( "GMLJP2", "Choosing alternate GML \"<offsetVector>\" order based on "
                "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER." );
        }

        std::swap(adfGeoTransform[1], adfGeoTransform[swapWith1Index]);
        std::swap(adfGeoTransform[2], adfGeoTransform[swapWith2Index]);

        /* Found in autotest/gdrivers/data/ll.jp2 */
        if( adfGeoTransform[1] == 0.0 && adfGeoTransform[2] < 0.0 &&
            adfGeoTransform[4] > 0.0 && adfGeoTransform[5] == 0.0 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "It is likely that the axis order of the GMLJP2 box is not "
                     "consistent with the EPSG order and that the resulting georeferencing "
                     "will be incorrect. Try setting GDAL_IGNORE_AXIS_ORIENTATION=TRUE if it is the case");
        }
    }

    return !m_oSRS.IsEmpty() && bSuccess;
}

/************************************************************************/
/*                         SetSpatialRef()                              */
/************************************************************************/

void GDALJP2Metadata::SetSpatialRef( const OGRSpatialReference* poSRS )

{
    m_oSRS.Clear();
    if( poSRS )
        m_oSRS = *poSRS;
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
/*                             SetRPCMD()                               */
/************************************************************************/

void GDALJP2Metadata::SetRPCMD( char** papszRPCMDIn )

{
    CSLDestroy( papszRPCMD );
    papszRPCMD = CSLDuplicate(papszRPCMDIn);
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
    unsigned char *pabyGTBuf = nullptr;

    if( GTIFMemBufFromSRS( OGRSpatialReference::ToHandle(&m_oSRS),
                             adfGeoTransform,
                             nGCPCount, pasGCPList,
                             &nGTBufSize, &pabyGTBuf, bPixelIsPoint,
                             papszRPCMD ) != CE_None )
        return nullptr;

    if( nGTBufSize == 0 )
        return nullptr;

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
                                                  CPLString& osDictBox,
                                                  int& bNeedAxisFlip )
{

/* -------------------------------------------------------------------- */
/*      Try do determine a PCS or GCS code we can use.                  */
/* -------------------------------------------------------------------- */
    nEPSGCode = 0;
    bNeedAxisFlip = FALSE;
    OGRSpatialReference oSRS(m_oSRS);

    if( oSRS.IsProjected() )
    {
        const char *pszAuthName = oSRS.GetAuthorityName( "PROJCS" );

        if( pszAuthName != nullptr && EQUAL(pszAuthName,"epsg") )
        {
            nEPSGCode = atoi(oSRS.GetAuthorityCode( "PROJCS" ));
        }
    }
    else if( oSRS.IsGeographic() )
    {
        const char *pszAuthName = oSRS.GetAuthorityName( "GEOGCS" );

        if( pszAuthName != nullptr && EQUAL(pszAuthName,"epsg") )
        {
            nEPSGCode = atoi(oSRS.GetAuthorityCode( "GEOGCS" ));
        }
    }

    // Save error state as importFromEPSGA() will call CPLReset()
    CPLErrorNum errNo = CPLGetLastErrorNo();
    CPLErr eErr = CPLGetLastErrorType();
    CPLString osLastErrorMsg = CPLGetLastErrorMsg();

    // Determine if we need to flip axis. Reimport from EPSG and make
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
        && CPLTestBool( CPLGetConfigOption( "GDAL_IGNORE_AXIS_ORIENTATION",
                                               "FALSE" ) ) )
    {
        bNeedAxisFlip = FALSE;
        CPLDebug( "GMLJP2", "Suppressed axis flipping on write based on GDAL_IGNORE_AXIS_ORIENTATION." );
    }

    pszComment = "";
    if( bNeedAxisFlip )
    {
        CPLDebug( "GMLJP2", "Flipping GML coverage axis order." );

        std::swap(adfOrigin[0], adfOrigin[1]);

        if( CPLTestBool( CPLGetConfigOption( "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER",
                                                "FALSE" ) ) )
        {
            CPLDebug( "GMLJP2", "Choosing alternate GML \"<offsetVector>\" order based on "
                "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER." );

            /* In this case the swapping is done in an "X" pattern */
            std::swap(adfXVector[0], adfYVector[1]);
            std::swap(adfYVector[0], adfXVector[1]);

            /* We add this as an XML comment so that we know we must do OffsetVector flipping on reading */
            pszComment = "              <!-- GDAL_JP2K_ALT_OFFSETVECTOR_ORDER=TRUE: First "
                         "value of offset is latitude/northing component of the "
                         "latitude/northing axis. -->\n";
        }
        else
        {
            std::swap(adfXVector[0], adfXVector[1]);
            std::swap(adfYVector[0], adfYVector[1]);
        }
    }

/* -------------------------------------------------------------------- */
/*      If we need a user defined CRSDictionary entry, prepare it       */
/*      here.                                                           */
/* -------------------------------------------------------------------- */
    if( nEPSGCode == 0 )
    {
        char *pszGMLDef = nullptr;

        if( oSRS.exportToXML( &pszGMLDef, nullptr ) == OGRERR_NONE )
        {
            char* pszWKT = nullptr;
            oSRS.exportToWkt(&pszWKT);
            char* pszXMLEscapedWKT = CPLEscapeString(pszWKT, -1, CPLES_XML);
            CPLFree(pszWKT);
            osDictBox.Printf(
"<gml:Dictionary gml:id=\"CRSU1\" \n"
"        xmlns:gml=\"http://www.opengis.net/gml\"\n"
"        xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
"        xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
"        xsi:schemaLocation=\"http://www.opengis.net/gml "
"http://schemas.opengis.net/gml/3.1.1/base/gml.xsd\">\n"
"  <gml:description>Dictionary for custom SRS %s</gml:description>\n"
"  <gml:name>Dictionary for custom SRS</gml:name>\n"
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
    if( CPLGetConfigOption( "GMLJP2OVERRIDE", nullptr ) != nullptr )
    {
        VSILFILE *fp = VSIFOpenL( CPLGetConfigOption( "GMLJP2OVERRIDE",""), "r" );
        char *pszGML = nullptr;

        if( fp == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to open GMLJP2OVERRIDE file." );
            return nullptr;
        }

        CPL_IGNORE_RET_VAL(VSIFSeekL( fp, 0, SEEK_END ));
        const int nLength = static_cast<int>( VSIFTellL( fp ) );
        pszGML = static_cast<char *>(CPLCalloc(1,nLength+1));
        CPL_IGNORE_RET_VAL(VSIFSeekL( fp, 0, SEEK_SET ));
        CPL_IGNORE_RET_VAL(VSIFReadL( pszGML, 1, nLength, fp ));
        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

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
    int bNeedAxisFlip = FALSE;
    if( !GetGMLJP2GeoreferencingInfo( nEPSGCode, adfOrigin,
                                      adfXVector, adfYVector,
                                      pszComment, osDictBox, bNeedAxisFlip ) )
    {
        return nullptr;
    }

    char szSRSName[100];
    if( nEPSGCode != 0 )
        snprintf( szSRSName, sizeof(szSRSName), "urn:ogc:def:crs:EPSG::%d", nEPSGCode );
    else
        snprintf( szSRSName, sizeof(szSRSName), "%s",
                "gmljp2://xml/CRSDictionary.gml#ogrcrs1" );

    // Compute bounding box
    double dfX1 = adfGeoTransform[0];
    double dfX2 = adfGeoTransform[0] + nXSize * adfGeoTransform[1];
    double dfX3 = adfGeoTransform[0] +                               nYSize * adfGeoTransform[2];
    double dfX4 = adfGeoTransform[0] + nXSize * adfGeoTransform[1] + nYSize * adfGeoTransform[2];
    double dfY1 = adfGeoTransform[3];
    double dfY2 = adfGeoTransform[3] + nXSize * adfGeoTransform[4];
    double dfY3 = adfGeoTransform[3] +                               nYSize * adfGeoTransform[5];
    double dfY4 = adfGeoTransform[3] + nXSize * adfGeoTransform[4] + nYSize * adfGeoTransform[5];
    double dfLCX = std::min(std::min(dfX1, dfX2), std::min(dfX3, dfX4));
    double dfLCY = std::min(std::min(dfY1, dfY2), std::min(dfY3, dfY4));
    double dfUCX = std::max(std::max(dfX1, dfX2), std::max(dfX3, dfX4));
    double dfUCY = std::max(std::max(dfY1, dfY2), std::max(dfY3, dfY4));
    if( bNeedAxisFlip )
    {
        std::swap(dfLCX, dfLCY);
        std::swap(dfUCX, dfUCY);
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
"    <gml:Envelope srsName=\"%s\">\n"
"      <gml:lowerCorner>%.15g %.15g</gml:lowerCorner>\n"
"      <gml:upperCorner>%.15g %.15g</gml:upperCorner>\n"
"    </gml:Envelope>\n"
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
             szSRSName, dfLCX, dfLCY, dfUCX, dfUCY,
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
    if( !osDictBox.empty() )
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
/*                      GDALGMLJP2GetXMLRoot()                          */
/************************************************************************/

static CPLXMLNode* GDALGMLJP2GetXMLRoot(CPLXMLNode* psNode)
{
    for( ; psNode != nullptr; psNode = psNode->psNext )
    {
        if( psNode->eType == CXT_Element && psNode->pszValue[0] != '?' )
            return psNode;
    }
    return nullptr;
}

/************************************************************************/
/*            GDALGMLJP2PatchFeatureCollectionSubstitutionGroup()       */
/************************************************************************/

static void GDALGMLJP2PatchFeatureCollectionSubstitutionGroup(CPLXMLNode* psRoot)
{
    /* GML 3.2 SF profile recommends the feature collection type to derive */
    /* from gml:AbstractGML to prevent it to be included in another feature */
    /* collection, but this is what we want to do. So patch that... */

    /* <xs:element name="FeatureCollection" type="ogr:FeatureCollectionType" substitutionGroup="gml:AbstractGML"/> */
    /* --> */
    /* <xs:element name="FeatureCollection" type="ogr:FeatureCollectionType" substitutionGroup="gml:AbstractFeature"/> */
    if( psRoot->eType == CXT_Element &&
        (strcmp(psRoot->pszValue, "schema") == 0 || strcmp(psRoot->pszValue, "xs:schema") == 0) )
    {
        for(CPLXMLNode* psIter = psRoot->psChild; psIter != nullptr; psIter = psIter->psNext)
        {
            if( psIter->eType == CXT_Element &&
                (strcmp(psIter->pszValue, "element") == 0 || strcmp(psIter->pszValue, "xs:element") == 0) &&
                strcmp(CPLGetXMLValue(psIter, "name", ""), "FeatureCollection") == 0 &&
                strcmp(CPLGetXMLValue(psIter, "substitutionGroup", ""), "gml:AbstractGML") == 0 )
            {
                CPLDebug("GMLJP2", R"(Patching substitutionGroup="gml:AbstractGML" to "gml:AbstractFeature")");
                CPLSetXMLValue( psIter, "#substitutionGroup", "gml:AbstractFeature" );
                break;
            }
        }
    }
}

/************************************************************************/
/*                          CreateGMLJP2V2()                            */
/************************************************************************/

class GMLJP2V2GMLFileDesc
{
    public:
        CPLString osFile{};
        CPLString osRemoteResource{};
        CPLString osNamespace{};
        CPLString osNamespacePrefix{};
        CPLString osSchemaLocation{};
        int       bInline = true;
        int       bParentCoverageCollection = true;
};

class GMLJP2V2AnnotationDesc
{
    public:
        CPLString osFile{};
};

class GMLJP2V2MetadataDesc
{
    public:
        CPLString osFile{};
        CPLString osContent{};
        CPLString osTemplateFile{};
        CPLString osSourceFile{};
        int       bGDALMetadata = false;
        int       bParentCoverageCollection = true;
};

class GMLJP2V2StyleDesc
{
    public:
        CPLString osFile{};
        int       bParentCoverageCollection = true;
};

class GMLJP2V2ExtensionDesc
{
    public:
        CPLString osFile{};
        int       bParentCoverageCollection = true;
};

class GMLJP2V2BoxDesc
{
    public:
        CPLString osFile{};
        CPLString osLabel{};
};

GDALJP2Box *GDALJP2Metadata::CreateGMLJP2V2( int nXSize, int nYSize,
                                             const char* pszDefFilename,
                                             GDALDataset* poSrcDS )

{
    CPLString osRootGMLId = "ID_GMLJP2_0";
    CPLString osGridCoverage;
    CPLString osGridCoverageFile;
    CPLString osCoverageRangeTypeXML;
    bool bCRSURL = true;
    std::vector<GMLJP2V2MetadataDesc> aoMetadata;
    std::vector<GMLJP2V2AnnotationDesc> aoAnnotations;
    std::vector<GMLJP2V2GMLFileDesc> aoGMLFiles;
    std::vector<GMLJP2V2StyleDesc> aoStyles;
    std::vector<GMLJP2V2ExtensionDesc> aoExtensions;
    std::vector<GMLJP2V2BoxDesc> aoBoxes;

/* -------------------------------------------------------------------- */
/*      Parse definition file.                                          */
/* -------------------------------------------------------------------- */
    if( pszDefFilename && !EQUAL(pszDefFilename, "YES") && !EQUAL(pszDefFilename, "TRUE") )
    {
        GByte* pabyContent = nullptr;
        if( pszDefFilename[0] != '{' )
        {
            if( !VSIIngestFile( nullptr, pszDefFilename, &pabyContent, nullptr, -1 ) )
                return nullptr;
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

        "#grid_coverage_range_type_field_predefined_name_doc": [
            "One of Color, Elevation_meter or Panchromatic ",
            "to fill gmlcov:rangeType/swe:DataRecord/swe:field",
            "Only used if grid_coverage_file is not defined.",
            "Exclusive with grid_coverage_range_type_file" ],
        "grid_coverage_range_type_field_predefined_name": "Color",

        "#grid_coverage_range_type_file_doc": [
            "File that is XML content to put under gml:RectifiedGrid/gmlcov:rangeType",
            "Only used if grid_coverage_file is not defined.",
            "Exclusive with grid_coverage_range_type_field_predefined_name" ],
        "grid_coverage_range_type_file": "grid_coverage_range_type.xml",

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
                "#file_doc": "Can use relative or absolute paths. Exclusive of content, gdal_metadata and generated_metadata.",
                "file": "dcmetadata.xml",

                "#gdal_metadata_doc": "Whether to serialize GDAL metadata as GDALMultiDomainMetadata",
                "gdal_metadata": false,

                "#dynamic_metadata_doc":
                    [ "The metadata file will be generated from a template and a source file.",
                      "The template is a valid GMLJP2 metadata XML tree with placeholders like",
                      "{{{XPATH(some_xpath_expression)}}}",
                      "that are evaluated from the source XML file. Typical use case",
                      "is to generate a gmljp2:eopMetadata from the XML metadata",
                      "provided by the image provider in their own particular format." ],
                "dynamic_metadata" :
                {
                    "template": "my_template.xml",
                    "source": "my_source.xml"
                },

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
                "#file_doc": "Can use relative or absolute paths. Exclusive of remote_resource",
                "file": "converted/test_0.gml",

                "#remote_resource_doc": "URL of a feature collection that must be referenced through a xlink:href",
                "remote_resource": "http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/expected_gml_gml32.gml",

                "#namespace_doc": ["The namespace in schemaLocation for which to substitute",
                                  "its original schemaLocation with the one provided below.",
                                  "Ignored for a remote_resource"],
                "namespace": "http://example.com",

                "#schema_location_doc": ["Value of the substituted schemaLocation. ",
                                         "Typically a schema box label (link)",
                                         "Ignored for a remote_resource"],
                "schema_location": "gmljp2://xml/schema_0.xsd",

                "#inline_doc": [
                    "Whether to inline the content, or put it in a separate xml box. Default is true",
                    "Ignored for a remote_resource." ],
                "inline": true,

                "#parent_node": ["Where to put the FeatureCollection.",
                                 "Under CoverageCollection (default) or GridCoverage" ],
                "parent_node": "CoverageCollection"
            }
        ],

        "#styles_doc": [ "An array of styles. For example SLD files" ],
        "styles" : [
            {
                "#file_doc": "Can use relative or absolute paths.",
                "file": "my.sld",

                "#parent_node": ["Where to put the FeatureCollection.",
                                 "Under CoverageCollection (default) or GridCoverage" ],
                "parent_node": "CoverageCollection"
            }
        ],

        "#extensions_doc": [ "An array of extensions." ],
        "extensions" : [
            {
                "#file_doc": "Can use relative or absolute paths.",
                "file": "my.xml",

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

        json_tokener* jstok = json_tokener_new();
        json_object* poObj = json_tokener_parse_ex(jstok, pabyContent ?
            reinterpret_cast<const char*>(pabyContent) : pszDefFilename, -1);
        CPLFree(pabyContent);
        if( jstok->err != json_tokener_success)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "JSON parsing error: %s (at offset %d)",
                        json_tokener_error_desc(jstok->err), jstok->char_offset);
            json_tokener_free(jstok);
            return nullptr;
        }
        json_tokener_free(jstok);

        json_object* poRootInstance = CPL_json_object_object_get(poObj, "root_instance");
        if( poRootInstance && json_object_get_type(poRootInstance) == json_type_object )
        {
            json_object* poGMLId = CPL_json_object_object_get(poRootInstance, "gml_id");
            if( poGMLId && json_object_get_type(poGMLId) == json_type_string )
                osRootGMLId = json_object_get_string(poGMLId);

            json_object* poGridCoverageFile = CPL_json_object_object_get(poRootInstance, "grid_coverage_file");
            if( poGridCoverageFile && json_object_get_type(poGridCoverageFile) == json_type_string )
                osGridCoverageFile = json_object_get_string(poGridCoverageFile);

            json_object* poGCRTFPN =
                CPL_json_object_object_get(poRootInstance, "grid_coverage_range_type_field_predefined_name");
            if( poGCRTFPN && json_object_get_type(poGCRTFPN) == json_type_string )
            {
                CPLString osPredefinedName( json_object_get_string(poGCRTFPN) );
                if( EQUAL(osPredefinedName, "Color") )
                {
                    osCoverageRangeTypeXML =
    "<swe:DataRecord>"
        "<swe:field name=\"Color\">"
            "<swe:Quantity definition=\"http://www.opengis.net/def/ogc-eo/opt/SpectralMode/Color\">"
                "<swe:description>Color image</swe:description>"
                "<swe:uom code=\"unity\"/>"
            "</swe:Quantity>"
        "</swe:field>"
    "</swe:DataRecord>";
                }
                else if( EQUAL(osPredefinedName, "Elevation_meter") )
                {
                    osCoverageRangeTypeXML =
    "<swe:DataRecord>"
        "<swe:field name=\"Elevation\">"
            "<swe:Quantity definition=\"http://inspire.ec.europa.eu/enumeration/ElevationPropertyTypeValue/height\" "
                          "referenceFrame=\"http://www.opengis.net/def/crs/EPSG/0/5714\">"
                "<swe:description>Elevation above sea level</swe:description>"
                "<swe:uom code=\"m\"/>"
            "</swe:Quantity>"
        "</swe:field>"
    "</swe:DataRecord>";
                }
                else if( EQUAL(osPredefinedName, "Panchromatic") )
                {
                    osCoverageRangeTypeXML =
    "<swe:DataRecord>"
        "<swe:field name=\"Panchromatic\">"
            "<swe:Quantity definition=\"http://www.opengis.net/def/ogc-eo/opt/SpectralMode/Panchromatic\">"
                "<swe:description>Panchromatic Channel</swe:description>"
                "<swe:uom code=\"unity\"/>"
            "</swe:Quantity>"
        "</swe:field>"
    "</swe:DataRecord>";
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unrecognized value for grid_coverage_range_type_field_predefined_name");
                }
            }
            else
            {
                json_object* poGCRTFile =
                    CPL_json_object_object_get(poRootInstance, "grid_coverage_range_type_file");
                if( poGCRTFile && json_object_get_type(poGCRTFile) == json_type_string )
                {
                    CPLXMLTreeCloser psTmp(CPLParseXMLFile(json_object_get_string(poGCRTFile)));
                    if( psTmp != nullptr )
                    {
                        CPLXMLNode* psTmpRoot = GDALGMLJP2GetXMLRoot(psTmp.get());
                        if( psTmpRoot )
                        {
                            char* pszTmp = CPLSerializeXMLTree(psTmpRoot);
                            osCoverageRangeTypeXML = pszTmp;
                            CPLFree(pszTmp);
                        }
                    }
                }
            }

            json_object* poCRSURL = CPL_json_object_object_get(poRootInstance, "crs_url");
            if( poCRSURL && json_object_get_type(poCRSURL) == json_type_boolean )
                bCRSURL = CPL_TO_BOOL(json_object_get_boolean(poCRSURL));

            json_object* poMetadatas = CPL_json_object_object_get(poRootInstance, "metadata");
            if( poMetadatas && json_object_get_type(poMetadatas) == json_type_array )
            {
                auto nLength = json_object_array_length(poMetadatas);
                for( decltype(nLength) i = 0; i < nLength; ++i )
                {
                    json_object* poMetadata =
                        json_object_array_get_idx(poMetadatas, i);
                    if( poMetadata &&
                        json_object_get_type(poMetadata) == json_type_string )
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
                        const char* pszFile = nullptr;
                        json_object* poFile = CPL_json_object_object_get(poMetadata, "file");
                        if( poFile && json_object_get_type(poFile) == json_type_string )
                            pszFile = json_object_get_string(poFile);

                        const char* pszContent = nullptr;
                        json_object* poContent = CPL_json_object_object_get(poMetadata, "content");
                        if( poContent && json_object_get_type(poContent) == json_type_string )
                            pszContent = json_object_get_string(poContent);

                        const char* pszTemplate = nullptr;
                        const char* pszSource = nullptr;
                        json_object* poDynamicMetadata = CPL_json_object_object_get(poMetadata, "dynamic_metadata");
                        if( poDynamicMetadata && json_object_get_type(poDynamicMetadata) == json_type_object )
                        {
#ifdef HAVE_LIBXML2
                            if( CPLTestBool(CPLGetConfigOption("GDAL_DEBUG_PROCESS_DYNAMIC_METADATA", "YES")) )
                            {
                                json_object* poTemplate = CPL_json_object_object_get(poDynamicMetadata, "template");
                                if( poTemplate && json_object_get_type(poTemplate) == json_type_string )
                                    pszTemplate = json_object_get_string(poTemplate);

                                json_object* poSource = CPL_json_object_object_get(poDynamicMetadata, "source");
                                if( poSource && json_object_get_type(poSource) == json_type_string )
                                    pszSource = json_object_get_string(poSource);
                            }
                            else
#endif
                            {
                            CPLError(CE_Warning, CPLE_NotSupported,
                                     "dynamic_metadata not supported since libxml2 is not available");
                            }
                        }

                        bool bGDALMetadata = false;
                        json_object* poGDALMetadata = CPL_json_object_object_get(poMetadata, "gdal_metadata");
                        if( poGDALMetadata && json_object_get_type(poGDALMetadata) == json_type_boolean )
                            bGDALMetadata = CPL_TO_BOOL(
                                json_object_get_boolean(poGDALMetadata));

                        if( pszFile != nullptr || pszContent != nullptr ||
                            (pszTemplate != nullptr && pszSource != nullptr) ||
                            bGDALMetadata )
                        {
                            GMLJP2V2MetadataDesc oDesc;
                            if( pszFile )
                                oDesc.osFile = pszFile;
                            if( pszContent )
                                oDesc.osContent = pszContent;
                            if( pszTemplate )
                                oDesc.osTemplateFile = pszTemplate;
                            if( pszSource )
                                oDesc.osSourceFile = pszSource;
                            oDesc.bGDALMetadata = bGDALMetadata;

                            json_object* poLocation = CPL_json_object_object_get(poMetadata, "parent_node");
                            if( poLocation && json_object_get_type(poLocation) == json_type_string )
                            {
                                const char* pszLocation = json_object_get_string(poLocation);
                                if( EQUAL(pszLocation, "CoverageCollection") )
                                    oDesc.bParentCoverageCollection  = TRUE;
                                else if( EQUAL(pszLocation, "GridCoverage") )
                                    oDesc.bParentCoverageCollection = FALSE;
                                else
                                    CPLError(CE_Warning, CPLE_NotSupported,
                                             "metadata[].parent_node should be CoverageCollection or GridCoverage");
                            }

                            aoMetadata.push_back(oDesc);
                        }
                    }
                }
            }

            json_object* poAnnotations = CPL_json_object_object_get(poRootInstance, "annotations");
            if( poAnnotations && json_object_get_type(poAnnotations) == json_type_array )
            {
                auto nLength = json_object_array_length(poAnnotations);
                for( decltype(nLength) i = 0; i < nLength; ++i )
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

            json_object* poGMLFileList =
                CPL_json_object_object_get(poRootInstance, "gml_filelist");
            if( poGMLFileList &&
                json_object_get_type(poGMLFileList) == json_type_array )
            {
                auto nLength = json_object_array_length(poGMLFileList);
                for( decltype(nLength) i = 0; i < nLength; ++i )
                {
                    json_object* poGMLFile =
                        json_object_array_get_idx(poGMLFileList, i);
                    if( poGMLFile &&
                        json_object_get_type(poGMLFile) == json_type_object )
                    {
                        const char* pszFile = nullptr;
                        json_object* poFile = CPL_json_object_object_get(poGMLFile, "file");
                        if( poFile && json_object_get_type(poFile) == json_type_string )
                            pszFile = json_object_get_string(poFile);

                        const char* pszRemoteResource = nullptr;
                        json_object* poRemoteResource = CPL_json_object_object_get(poGMLFile, "remote_resource");
                        if( poRemoteResource && json_object_get_type(poRemoteResource) == json_type_string )
                            pszRemoteResource = json_object_get_string(poRemoteResource);

                        if( pszFile || pszRemoteResource )
                        {
                            GMLJP2V2GMLFileDesc oDesc;
                            if( pszFile )
                                oDesc.osFile = pszFile;
                            else if( pszRemoteResource )
                                oDesc.osRemoteResource = pszRemoteResource;

                            json_object* poNamespacePrefix = CPL_json_object_object_get(poGMLFile, "namespace_prefix");
                            if( poNamespacePrefix && json_object_get_type(poNamespacePrefix) == json_type_string )
                                oDesc.osNamespacePrefix = json_object_get_string(poNamespacePrefix);

                            json_object* poNamespace = CPL_json_object_object_get(poGMLFile, "namespace");
                            if( poNamespace && json_object_get_type(poNamespace) == json_type_string )
                                oDesc.osNamespace = json_object_get_string(poNamespace);

                            json_object* poSchemaLocation = CPL_json_object_object_get(poGMLFile, "schema_location");
                            if( poSchemaLocation && json_object_get_type(poSchemaLocation) == json_type_string )
                                oDesc.osSchemaLocation = json_object_get_string(poSchemaLocation);

                            json_object* poInline = CPL_json_object_object_get(poGMLFile, "inline");
                            if( poInline && json_object_get_type(poInline) == json_type_boolean )
                                oDesc.bInline = json_object_get_boolean(poInline);

                            json_object* poLocation = CPL_json_object_object_get(poGMLFile, "parent_node");
                            if( poLocation && json_object_get_type(poLocation) == json_type_string )
                            {
                                const char* pszLocation = json_object_get_string(poLocation);
                                if( EQUAL(pszLocation, "CoverageCollection") )
                                    oDesc.bParentCoverageCollection  = TRUE;
                                else if( EQUAL(pszLocation, "GridCoverage") )
                                    oDesc.bParentCoverageCollection = FALSE;
                                else
                                    CPLError(CE_Warning, CPLE_NotSupported,
                                             "gml_filelist[].parent_node should be CoverageCollection or GridCoverage");
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

            json_object* poStyles = CPL_json_object_object_get(poRootInstance, "styles");
            if( poStyles && json_object_get_type(poStyles) == json_type_array )
            {
                auto nLength = json_object_array_length(poStyles);
                for( decltype(nLength) i = 0; i < nLength; ++i )
                {
                    json_object* poStyle = json_object_array_get_idx(poStyles, i);
                    if( poStyle && json_object_get_type(poStyle) == json_type_object )
                    {
                        const char* pszFile = nullptr;
                        json_object* poFile = CPL_json_object_object_get(poStyle, "file");
                        if( poFile && json_object_get_type(poFile) == json_type_string )
                            pszFile = json_object_get_string(poFile);

                        if( pszFile )
                        {
                            GMLJP2V2StyleDesc oDesc;
                            oDesc.osFile = pszFile;

                            json_object* poLocation = CPL_json_object_object_get(poStyle, "parent_node");
                            if( poLocation && json_object_get_type(poLocation) == json_type_string )
                            {
                                const char* pszLocation = json_object_get_string(poLocation);
                                if( EQUAL(pszLocation, "CoverageCollection") )
                                    oDesc.bParentCoverageCollection  = TRUE;
                                else if( EQUAL(pszLocation, "GridCoverage") )
                                    oDesc.bParentCoverageCollection = FALSE;
                                else
                                    CPLError(CE_Warning, CPLE_NotSupported,
                                             "styles[].parent_node should be CoverageCollection or GridCoverage");
                            }

                            aoStyles.push_back(oDesc);
                        }
                    }
                    else if( poStyle && json_object_get_type(poStyle) == json_type_string )
                    {
                        GMLJP2V2StyleDesc oDesc;
                        oDesc.osFile = json_object_get_string(poStyle);
                        aoStyles.push_back(oDesc);
                    }
                }
            }

            json_object* poExtensions = CPL_json_object_object_get(poRootInstance, "extensions");
            if( poExtensions && json_object_get_type(poExtensions) == json_type_array )
            {
                auto nLength = json_object_array_length(poExtensions);
                for( decltype(nLength) i = 0; i < nLength; ++i )
                {
                    json_object* poExtension = json_object_array_get_idx(poExtensions, i);
                    if( poExtension && json_object_get_type(poExtension) == json_type_object )
                    {
                        const char* pszFile = nullptr;
                        json_object* poFile = CPL_json_object_object_get(poExtension, "file");
                        if( poFile && json_object_get_type(poFile) == json_type_string )
                            pszFile = json_object_get_string(poFile);

                        if( pszFile )
                        {
                            GMLJP2V2ExtensionDesc oDesc;
                            oDesc.osFile = pszFile;

                            json_object* poLocation = CPL_json_object_object_get(poExtension, "parent_node");
                            if( poLocation && json_object_get_type(poLocation) == json_type_string )
                            {
                                const char* pszLocation = json_object_get_string(poLocation);
                                if( EQUAL(pszLocation, "CoverageCollection") )
                                    oDesc.bParentCoverageCollection  = TRUE;
                                else if( EQUAL(pszLocation, "GridCoverage") )
                                    oDesc.bParentCoverageCollection = FALSE;
                                else
                                    CPLError(CE_Warning, CPLE_NotSupported,
                                             "extensions[].parent_node should be CoverageCollection or GridCoverage");
                            }

                            aoExtensions.push_back(oDesc);
                        }
                    }
                    else if( poExtension && json_object_get_type(poExtension) == json_type_string )
                    {
                        GMLJP2V2ExtensionDesc oDesc;
                        oDesc.osFile = json_object_get_string(poExtension);
                        aoExtensions.push_back(oDesc);
                    }
                }
            }
        }

        json_object* poBoxes = CPL_json_object_object_get(poObj, "boxes");
        if( poBoxes && json_object_get_type(poBoxes) == json_type_array )
        {
            auto nLength = json_object_array_length(poBoxes);
            for( decltype(nLength) i = 0; i < nLength; ++i )
            {
                json_object* poBox = json_object_array_get_idx(poBoxes, i);
                if( poBox && json_object_get_type(poBox) == json_type_object )
                {
                    json_object* poFile = CPL_json_object_object_get(poBox, "file");
                    if( poFile &&
                        json_object_get_type(poFile) == json_type_string )
                    {
                        GMLJP2V2BoxDesc oDesc;
                        oDesc.osFile = json_object_get_string(poFile);

                        json_object* poLabel =
                            CPL_json_object_object_get(poBox, "label");
                        if( poLabel &&
                            json_object_get_type(poLabel) == json_type_string )
                            oDesc.osLabel = json_object_get_string(poLabel);
                        else
                            oDesc.osLabel = CPLGetFilename(oDesc.osFile);

                        aoBoxes.push_back(oDesc);
                    }
                }
                else if( poBox &&
                         json_object_get_type(poBox) == json_type_string )
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
        for( const auto& oGMLFile: aoGMLFiles )
        {
            if( !oGMLFile.osSchemaLocation.empty() &&
                STARTS_WITH(oGMLFile.osSchemaLocation, "gmljp2://xml/") )
            {
                const char* pszLookedLabel =
                    oGMLFile.osSchemaLocation.c_str() +
                    strlen("gmljp2://xml/");
                bool bFound = false;
                for( int j = 0;
                     !bFound && j < static_cast<int>(aoBoxes.size());
                     ++j )
                    bFound = (strcmp(pszLookedLabel, aoBoxes[j].osLabel) == 0);
                if( !bFound )
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "GML file %s has a schema_location=%s, "
                        "but no box with label %s is defined",
                        oGMLFile.osFile.c_str(),
                        oGMLFile.osSchemaLocation.c_str(),
                        pszLookedLabel);
                }
            }
        }

        // Read custom grid coverage file.
        if( !osGridCoverageFile.empty() )
        {
            CPLXMLTreeCloser psTmp(CPLParseXMLFile(osGridCoverageFile));
            if( psTmp == nullptr )
                return nullptr;
            CPLXMLNode* psTmpRoot = GDALGMLJP2GetXMLRoot(psTmp.get());
            if( psTmpRoot )
            {
                char* pszTmp = CPLSerializeXMLTree(psTmpRoot);
                osGridCoverage = pszTmp;
                CPLFree(pszTmp);
            }
        }
    }

    CPLString osDictBox;
    CPLString osDoc;

    if( osGridCoverage.empty() )
    {
/* -------------------------------------------------------------------- */
/*      Prepare GMLJP2RectifiedGridCoverage                             */
/* -------------------------------------------------------------------- */
        int nEPSGCode = 0;
        double adfOrigin[2];
        double adfXVector[2];
        double adfYVector[2];
        const char* pszComment = "";
        int bNeedAxisFlip = FALSE;
        if( !GetGMLJP2GeoreferencingInfo( nEPSGCode, adfOrigin,
                                        adfXVector, adfYVector,
                                        pszComment, osDictBox, bNeedAxisFlip ) )
        {
            return nullptr;
        }

        char szSRSName[100] = {0};
        if( nEPSGCode != 0 )
        {
            if( bCRSURL )
                snprintf( szSRSName, sizeof(szSRSName),
                          "http://www.opengis.net/def/crs/EPSG/0/%d", nEPSGCode );
            else
                snprintf( szSRSName, sizeof(szSRSName),
                          "urn:ogc:def:crs:EPSG::%d", nEPSGCode );
        }
        else
            snprintf( szSRSName, sizeof(szSRSName), "%s",
                    "gmljp2://xml/CRSDictionary.gml#ogrcrs1" );


        // Compute bounding box
        double dfX1 = adfGeoTransform[0];
        double dfX2 = adfGeoTransform[0] + nXSize * adfGeoTransform[1];
        double dfX3 = adfGeoTransform[0] +                               nYSize * adfGeoTransform[2];
        double dfX4 = adfGeoTransform[0] + nXSize * adfGeoTransform[1] + nYSize * adfGeoTransform[2];
        double dfY1 = adfGeoTransform[3];
        double dfY2 = adfGeoTransform[3] + nXSize * adfGeoTransform[4];
        double dfY3 = adfGeoTransform[3] +                               nYSize * adfGeoTransform[5];
        double dfY4 = adfGeoTransform[3] + nXSize * adfGeoTransform[4] + nYSize * adfGeoTransform[5];
        double dfLCX = std::min(std::min(dfX1, dfX2), std::min(dfX3, dfX4));
        double dfLCY = std::min(std::min(dfY1, dfY2), std::min(dfY3, dfY4));
        double dfUCX = std::max(std::max(dfX1, dfX2), std::max(dfX3, dfX4));
        double dfUCY = std::max(std::max(dfY1, dfY2), std::max(dfY3, dfY4));
        if( bNeedAxisFlip )
        {
            std::swap(dfLCX, dfLCY);
            std::swap(dfUCX, dfUCY);
        }

        osGridCoverage.Printf(
"   <gmljp2:GMLJP2RectifiedGridCoverage gml:id=\"RGC_1_%s\">\n"
"     <gml:boundedBy>\n"
"       <gml:Envelope srsDimension=\"2\" srsName=\"%s\">\n"
"         <gml:lowerCorner>%.15g %.15g</gml:lowerCorner>\n"
"         <gml:upperCorner>%.15g %.15g</gml:upperCorner>\n"
"       </gml:Envelope>\n"
"     </gml:boundedBy>\n"
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
"     <gmlcov:rangeType>%s</gmlcov:rangeType>\n"
"   </gmljp2:GMLJP2RectifiedGridCoverage>\n",
            osRootGMLId.c_str(),
            szSRSName,
            dfLCX, dfLCY,
            dfUCX, dfUCY,
            osRootGMLId.c_str(),
            szSRSName,
            nXSize-1, nYSize-1, szSRSName, adfOrigin[0], adfOrigin[1],
            pszComment,
            szSRSName, adfXVector[0], adfXVector[1],
            szSRSName, adfYVector[0], adfYVector[1],
            osCoverageRangeTypeXML.c_str() );
    }

/* -------------------------------------------------------------------- */
/*      Main node.                                                      */
/* -------------------------------------------------------------------- */

    // Per http://docs.opengeospatial.org/is/08-085r5/08-085r5.html#requirement_11
    osDoc.Printf(
//"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gmljp2:GMLJP2CoverageCollection gml:id=\"%s\"\n"
"     xmlns:gml=\"http://www.opengis.net/gml/3.2\"\n"
"     xmlns:gmlcov=\"http://www.opengis.net/gmlcov/1.0\"\n"
"     xmlns:gmljp2=\"http://www.opengis.net/gmljp2/2.0\"\n"
"     xmlns:swe=\"http://www.opengis.net/swe/2.0\"\n"
"     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
"     xsi:schemaLocation=\"http://www.opengis.net/gmljp2/2.0 http://schemas.opengis.net/gmljp2/2.0/gmljp2.xsd\">\n"
"  <gml:domainSet nilReason=\"inapplicable\"/>\n"
"  <gml:rangeSet>\n"
"    <gml:DataBlock>\n"
"       <gml:rangeParameters nilReason=\"inapplicable\"/>\n"
"       <gml:doubleOrNilReasonTupleList>inapplicable</gml:doubleOrNilReasonTupleList>\n"
"     </gml:DataBlock>\n"
"  </gml:rangeSet>\n"
"  <gmlcov:rangeType>\n"
"    <swe:DataRecord>\n"
"      <swe:field name=\"Collection\"> </swe:field>\n"
"    </swe:DataRecord>\n"
"  </gmlcov:rangeType>\n"
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
    if( !aoMetadata.empty() || !aoAnnotations.empty() || !aoGMLFiles.empty() ||
        !aoStyles.empty() || !aoExtensions.empty() )
    {
        CPLXMLTreeCloser psRoot(CPLParseXMLString(osDoc));
        CPLAssert(psRoot);
        CPLXMLNode* psGMLJP2CoverageCollection = GDALGMLJP2GetXMLRoot(psRoot.get());
        CPLAssert(psGMLJP2CoverageCollection);

        for( const auto& oMetadata: aoMetadata )
        {
            CPLXMLTreeCloser psMetadata(nullptr);
            if( !oMetadata.osFile.empty() )
                psMetadata = CPLXMLTreeCloser(CPLParseXMLFile(oMetadata.osFile));
            else if( !oMetadata.osContent.empty() )
                psMetadata = CPLXMLTreeCloser(CPLParseXMLString(oMetadata.osContent));
            else if( oMetadata.bGDALMetadata )
            {
                psMetadata = CPLXMLTreeCloser(CreateGDALMultiDomainMetadataXML(poSrcDS, TRUE));
                if( psMetadata )
                {
                    CPLSetXMLValue(psMetadata.get(), "#xmlns", "http://gdal.org");
                    CPLXMLNode* psNewMetadata =
                        CPLCreateXMLNode(nullptr, CXT_Element, "gmljp2:metadata");
                    CPLAddXMLChild(psNewMetadata, psMetadata.release());
                    psMetadata = CPLXMLTreeCloser(psNewMetadata);
                }
            }
            else
                psMetadata = CPLXMLTreeCloser(
                    GDALGMLJP2GenerateMetadata(oMetadata.osTemplateFile,
                                               oMetadata.osSourceFile));
            if( psMetadata == nullptr )
                continue;
            CPLXMLNode* psMetadataRoot = GDALGMLJP2GetXMLRoot(psMetadata.get());
            if( psMetadataRoot )
            {
                if( strcmp(psMetadataRoot->pszValue,
                           "eop:EarthObservation") == 0 )
                {
                    CPLXMLNode* psNewMetadata = CPLCreateXMLNode(nullptr, CXT_Element, "gmljp2:eopMetadata");
                    CPLAddXMLChild(psNewMetadata, CPLCloneXMLTree(psMetadataRoot));
                    psMetadataRoot = psNewMetadata;
                    psMetadata = CPLXMLTreeCloser(psNewMetadata);
                }
                if( strcmp(psMetadataRoot->pszValue, "gmljp2:isoMetadata") != 0 &&
                    strcmp(psMetadataRoot->pszValue, "gmljp2:eopMetadata") != 0 &&
                    strcmp(psMetadataRoot->pszValue, "gmljp2:dcMetadata") != 0 &&
                    strcmp(psMetadataRoot->pszValue, "gmljp2:metadata") != 0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The metadata root node should be one of gmljp2:isoMetadata, "
                             "gmljp2:eopMetadata, gmljp2:dcMetadata or gmljp2:metadata");
                }
                else if( oMetadata.bParentCoverageCollection )
                {
                    /* Insert the gmlcov:metadata link as the next sibling of */
                    /* GMLJP2CoverageCollection.rangeType */
                    CPLXMLNode* psRangeType =
                        CPLGetXMLNode(psGMLJP2CoverageCollection, "gmlcov:rangeType");
                    CPLAssert(psRangeType);
                    CPLXMLNode* psNodeAfterWhichToInsert = psRangeType;
                    CPLXMLNode* psNext = psNodeAfterWhichToInsert->psNext;
                    while( psNext != nullptr && psNext->eType == CXT_Element &&
                           strcmp(psNext->pszValue, "gmlcov:metadata") == 0 )
                    {
                        psNodeAfterWhichToInsert = psNext;
                        psNext = psNext->psNext;
                    }
                    psNodeAfterWhichToInsert->psNext = nullptr;
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
        }

        bool bRootHasXLink = false;

        // Examples of inline or reference feature collections can be found
        // in http://schemas.opengis.net/gmljp2/2.0/examples/gmljp2.xml
        for( int i = 0; i < static_cast<int>(aoGMLFiles.size()); ++i )
        {
            // Is the file already a GML file?
            CPLXMLTreeCloser psGMLFile(nullptr);
            if( !aoGMLFiles[i].osFile.empty() )
            {
                if( EQUAL(CPLGetExtension(aoGMLFiles[i].osFile), "gml") ||
                    EQUAL(CPLGetExtension(aoGMLFiles[i].osFile), "xml") )
                {
                    psGMLFile = CPLXMLTreeCloser(CPLParseXMLFile(aoGMLFiles[i].osFile));
                }
                GDALDriverH hDrv = nullptr;
                if( psGMLFile == nullptr )
                {
                    hDrv = GDALIdentifyDriver(aoGMLFiles[i].osFile, nullptr);
                    if( hDrv == nullptr )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s is no a GDAL recognized file",
                                 aoGMLFiles[i].osFile.c_str());
                        continue;
                    }
                }
                GDALDriverH hGMLDrv = GDALGetDriverByName("GML");
                if( psGMLFile == nullptr && hDrv == hGMLDrv )
                {
                    // Yes, parse it
                    psGMLFile = CPLXMLTreeCloser(CPLParseXMLFile(aoGMLFiles[i].osFile));
                }
                else if( psGMLFile == nullptr )
                {
                    if( hGMLDrv == nullptr )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "Cannot translate %s to GML",
                                aoGMLFiles[i].osFile.c_str());
                        continue;
                    }

                    // On-the-fly translation to GML 3.2
                    GDALDatasetH hSrcDS = GDALOpenEx(aoGMLFiles[i].osFile, 0, nullptr, nullptr, nullptr);
                    if( hSrcDS )
                    {
                        CPLString osTmpFile = CPLSPrintf("/vsimem/gmljp2/%p/%d/%s.gml",
                                                        this,
                                                        i,
                                                        CPLGetBasename(aoGMLFiles[i].osFile));
                        char ** papszOptions = nullptr;
                        papszOptions = CSLSetNameValue(papszOptions,
                                                       "FORMAT", "GML3.2");
                        papszOptions = CSLSetNameValue(papszOptions,
                                "SRSNAME_FORMAT",
                                (bCRSURL) ? "OGC_URL" : "OGC_URN");
                        if( aoGMLFiles.size() > 1 ||
                            !aoGMLFiles[i].osNamespace.empty() ||
                            !aoGMLFiles[i].osNamespacePrefix.empty() )
                        {
                            papszOptions = CSLSetNameValue(papszOptions,
                                "PREFIX",
                                    aoGMLFiles[i].osNamespacePrefix.empty() ?
                                        CPLSPrintf("ogr%d", i) :
                                        aoGMLFiles[i].osNamespacePrefix.c_str());
                            papszOptions = CSLSetNameValue(papszOptions,
                                "TARGET_NAMESPACE",
                                aoGMLFiles[i].osNamespace.empty() ?
                                    CPLSPrintf("http://ogr.maptools.org/%d", i) :
                                    aoGMLFiles[i].osNamespace.c_str());
                        }
                        GDALDatasetH hDS = GDALCreateCopy(
                                    hGMLDrv, osTmpFile, hSrcDS,
                                    FALSE,
                                    papszOptions, nullptr, nullptr);
                        CSLDestroy(papszOptions);
                        if( hDS )
                        {
                            GDALClose(hDS);
                            psGMLFile = CPLXMLTreeCloser(CPLParseXMLFile(osTmpFile));
                            aoGMLFiles[i].osFile = osTmpFile;
                            VSIUnlink(osTmpFile);
                            aosTmpFiles.emplace_back(CPLResetExtension(osTmpFile, "xsd"));
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
                if( psGMLFile == nullptr )
                    continue;
            }

            CPLXMLNode* psGMLFileRoot = psGMLFile ? GDALGMLJP2GetXMLRoot(psGMLFile.get()) : nullptr;
            if( psGMLFileRoot || !aoGMLFiles[i].osRemoteResource.empty() )
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

                if( !aoGMLFiles[i].bInline || !aoGMLFiles[i].osRemoteResource.empty() )
                {
                    if( !bRootHasXLink )
                    {
                        bRootHasXLink = true;
                        CPLSetXMLValue(psGMLJP2CoverageCollection, "#xmlns:xlink",
                                       "http://www.w3.org/1999/xlink");
                    }
                }

                if( !aoGMLFiles[i].osRemoteResource.empty() )
                {
                    CPLSetXMLValue(node_f, "#xlink:href",
                                   aoGMLFiles[i].osRemoteResource.c_str());
                    continue;
                }

                CPLString osTmpFile;
                if( !aoGMLFiles[i].bInline || !aoGMLFiles[i].osRemoteResource.empty() )
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

                    CPLSetXMLValue(node_f, "#xlink:href",
                            CPLSPrintf("gmljp2://xml/%s", oDesc.osLabel.c_str()));
                }

                if( CPLGetXMLNode(psGMLFileRoot, "xmlns") == nullptr &&
                    CPLGetXMLNode(psGMLFileRoot, "xmlns:gml") == nullptr )
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
                psGMLFileGMLId = nullptr;
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
                        aoGMLFiles[i].osNamespace.empty() &&
                        !aoGMLFiles[i].osSchemaLocation.empty() )
                    {
                        osSchemaLocation += papszTokens[0];
                        osSchemaLocation += " ";
                        osSchemaLocation += aoGMLFiles[i].osSchemaLocation;
                    }

                    else if( CSLCount(papszTokens) == 2 &&
                                (aoGMLFiles[i].osNamespace.empty() ||
                                strcmp(papszTokens[0], aoGMLFiles[i].osNamespace) == 0) &&
                                aoGMLFiles[i].osSchemaLocation.empty() )
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
                                                        papszTokens[1], nullptr),
                                        &sStat) == 0 )
                        {
                            osXSD = CPLFormFilename(CPLGetDirname(aoGMLFiles[i].osFile),
                                                        papszTokens[1], nullptr);
                        }
                        if( !osXSD.empty() )
                        {
                            GMLJP2V2BoxDesc oDesc;
                            oDesc.osFile = osXSD;
                            oDesc.osLabel = CPLGetFilename(oDesc.osFile);
                            osSchemaLocation += papszTokens[0];
                            osSchemaLocation += " ";
                            osSchemaLocation += "gmljp2://xml/";
                            osSchemaLocation += oDesc.osLabel;
                            int j = 0;  // Used after for.
                            for( ; j < static_cast<int>(aoBoxes.size()); ++j )
                            {
                                if( aoBoxes[j].osLabel == oDesc.osLabel )
                                    break;
                            }
                            if( j == static_cast<int>(aoBoxes.size()) )
                                aoBoxes.push_back(oDesc);
                        }
                    }

                    else if( (CSLCount(papszTokens) % 2) == 0 )
                    {
                        for( char** papszIter = papszTokens;
                             *papszIter;
                             papszIter += 2 )
                        {
                            if( !osSchemaLocation.empty() )
                                osSchemaLocation += " ";
                            if( !aoGMLFiles[i].osNamespace.empty() &&
                                !aoGMLFiles[i].osSchemaLocation.empty() &&
                                strcmp(papszIter[0],
                                       aoGMLFiles[i].osNamespace) == 0 )
                            {
                                osSchemaLocation += papszIter[0];
                                osSchemaLocation += " ";
                                osSchemaLocation +=
                                    aoGMLFiles[i].osSchemaLocation;
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
                    CPLSerializeXMLTreeToFile( psGMLFile.get(), osTmpFile );
            }
        }

        // c.f.
        // http://schemas.opengis.net/gmljp2/2.0/examples/gmljp2_annotation.xml
        for( int i = 0; i < static_cast<int>(aoAnnotations.size()); ++i )
        {
            // Is the file already a KML file?
            CPLXMLTreeCloser psKMLFile(nullptr);
            if( EQUAL(CPLGetExtension(aoAnnotations[i].osFile), "kml") )
                 psKMLFile = CPLXMLTreeCloser(CPLParseXMLFile(aoAnnotations[i].osFile));
            GDALDriverH hDrv = nullptr;
            if( psKMLFile == nullptr )
            {
                hDrv = GDALIdentifyDriver(aoAnnotations[i].osFile, nullptr);
                if( hDrv == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s is no a GDAL recognized file",
                             aoAnnotations[i].osFile.c_str());
                    continue;
                }
            }
            GDALDriverH hKMLDrv = GDALGetDriverByName("KML");
            GDALDriverH hLIBKMLDrv = GDALGetDriverByName("LIBKML");
            if( psKMLFile == nullptr && (hDrv == hKMLDrv || hDrv == hLIBKMLDrv) )
            {
                // Yes, parse it
                psKMLFile = CPLXMLTreeCloser(CPLParseXMLFile(aoAnnotations[i].osFile));
            }
            else if( psKMLFile == nullptr )
            {
                if( hKMLDrv == nullptr && hLIBKMLDrv == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot translate %s to KML",
                             aoAnnotations[i].osFile.c_str());
                    continue;
                }

                // On-the-fly translation to KML
                GDALDatasetH hSrcDS = GDALOpenEx(aoAnnotations[i].osFile, 0, nullptr, nullptr, nullptr);
                if( hSrcDS )
                {
                    CPLString osTmpFile = CPLSPrintf("/vsimem/gmljp2/%p/%d/%s.kml",
                                                     this,
                                                     i,
                                                     CPLGetBasename(aoAnnotations[i].osFile));
                    char** papszOptions = nullptr;
                    if( aoAnnotations.size() > 1 )
                    {
                        papszOptions = CSLSetNameValue(
                            papszOptions, "DOCUMENT_ID",
                            CPLSPrintf("root_doc_%d", i));
                    }
                    GDALDatasetH hDS = GDALCreateCopy(hLIBKMLDrv ? hLIBKMLDrv : hKMLDrv,
                                                      osTmpFile, hSrcDS,
                                                      FALSE, papszOptions, nullptr, nullptr);
                    CSLDestroy(papszOptions);
                    if( hDS )
                    {
                        GDALClose(hDS);
                        psKMLFile = CPLXMLTreeCloser(CPLParseXMLFile(osTmpFile));
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
            if( psKMLFile == nullptr )
                continue;

            CPLXMLNode* psKMLFileRoot = GDALGMLJP2GetXMLRoot(psKMLFile.get());
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
                    CPLGetXMLNode(psKMLFileRoot, "xsi:schemaLocation") == nullptr &&
                    strcmp(CPLGetXMLValue(psKMLFileRoot, "xmlns", ""),
                           "http://www.opengis.net/kml/2.2") == 0  )
                {
                    CPLSetXMLValue(psKMLFileRoot, "#xsi:schemaLocation",
                                   "http://www.opengis.net/kml/2.2 http://schemas.opengis.net/kml/2.2.0/ogckml22.xsd");
                }

                CPLAddXMLChild(psAnnotation, CPLCloneXMLTree(psKMLFileRoot));
            }
        }

        // Add styles.
        for( const auto& oStyle: aoStyles )
        {
            CPLXMLTreeCloser psStyle(CPLParseXMLFile(oStyle.osFile));
            if( psStyle == nullptr )
                continue;

            CPLXMLNode* psStyleRoot = GDALGMLJP2GetXMLRoot(psStyle.get());
            if( psStyleRoot )
            {
                CPLXMLNode *psGMLJP2Style = nullptr;
                if( oStyle.bParentCoverageCollection )
                {
                    psGMLJP2Style =
                        CPLCreateXMLNode( psGMLJP2CoverageCollection,
                                          CXT_Element, "gmljp2:style" );
                }
                else
                {
                    CPLXMLNode* psFeatureMemberOfGridCoverage =
                        CPLGetXMLNode(psGMLJP2CoverageCollection,
                                      "gmljp2:featureMember");
                    CPLAssert(psFeatureMemberOfGridCoverage);
                    CPLXMLNode* psGridCoverage =
                        psFeatureMemberOfGridCoverage->psChild;
                    CPLAssert(psGridCoverage);
                    psGMLJP2Style =
                        CPLCreateXMLNode( psGridCoverage,
                                          CXT_Element, "gmljp2:style" );
                }

                // Add dummy namespace for validation purposes if needed
                if( strchr(psStyleRoot->pszValue, ':') == nullptr &&
                    CPLGetXMLValue(psStyleRoot, "xmlns", nullptr) == nullptr )
                {
                    CPLSetXMLValue(psStyleRoot, "#xmlns",
                                   "http://undefined_namespace");
                }

                CPLAddXMLChild( psGMLJP2Style, CPLCloneXMLTree(psStyleRoot) );
            }
        }

        // Add extensions.
        for( const auto& oExt: aoExtensions )
        {
            CPLXMLTreeCloser psExtension(CPLParseXMLFile(oExt.osFile));
            if( psExtension == nullptr )
                continue;

            CPLXMLNode* psExtensionRoot = GDALGMLJP2GetXMLRoot(psExtension.get());
            if( psExtensionRoot )
            {
                CPLXMLNode *psGMLJP2Extension;
                if( oExt.bParentCoverageCollection )
                {
                    psGMLJP2Extension =
                        CPLCreateXMLNode( psGMLJP2CoverageCollection,
                                          CXT_Element, "gmljp2:extension" );
                }
                else
                {
                    CPLXMLNode* psFeatureMemberOfGridCoverage =
                        CPLGetXMLNode(psGMLJP2CoverageCollection,
                                      "gmljp2:featureMember");
                    CPLAssert(psFeatureMemberOfGridCoverage);
                    CPLXMLNode* psGridCoverage =
                        psFeatureMemberOfGridCoverage->psChild;
                    CPLAssert(psGridCoverage);
                    psGMLJP2Extension =
                        CPLCreateXMLNode( psGridCoverage,
                                          CXT_Element, "gmljp2:extension" );
                }

                // Add dummy namespace for validation purposes if needed
                if( strchr(psExtensionRoot->pszValue, ':') == nullptr &&
                    CPLGetXMLValue(psExtensionRoot, "xmlns", nullptr) == nullptr )
                {
                    CPLSetXMLValue(psExtensionRoot, "#xmlns",
                                   "http://undefined_namespace");
                }

                CPLAddXMLChild( psGMLJP2Extension,
                                CPLCloneXMLTree(psExtensionRoot) );
            }
        }

        char* pszRoot = CPLSerializeXMLTree(psRoot.get());
        psRoot.reset();
        osDoc = pszRoot;
        CPLFree(pszRoot);
        pszRoot = nullptr;
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
    if( !osDictBox.empty() )
        apoGMLBoxes.push_back(
            GDALJP2Box::CreateLabelledXMLAssoc( "CRSDictionary.gml",
                                                osDictBox ) );

/* -------------------------------------------------------------------- */
/*      Additional user specified boxes.                                */
/* -------------------------------------------------------------------- */
    for( auto& oBox: aoBoxes )
    {
        GByte* pabyContent = nullptr;
        if( VSIIngestFile( nullptr, oBox.osFile, &pabyContent, nullptr, -1 ) )
        {
            CPLXMLTreeCloser psNode(CPLParseXMLString(
                                reinterpret_cast<const char*>(pabyContent)));
            CPLFree(pabyContent);
            pabyContent = nullptr;
            if( psNode.get() )
            {
                CPLXMLNode* psRoot = GDALGMLJP2GetXMLRoot(psNode.get());
                if( psRoot )
                {
                    GDALGMLJP2PatchFeatureCollectionSubstitutionGroup(psRoot);
                    pabyContent = reinterpret_cast<GByte*>(CPLSerializeXMLTree(psRoot));
                    apoGMLBoxes.push_back(
                        GDALJP2Box::CreateLabelledXMLAssoc( oBox.osLabel,
                                reinterpret_cast<const char*>(pabyContent)) );
                }
            }
        }
        CPLFree(pabyContent);
    }

/* -------------------------------------------------------------------- */
/*      Bundle gml.data boxes into an association.                      */
/* -------------------------------------------------------------------- */
    GDALJP2Box *poGMLData =
        GDALJP2Box::CreateAsocBox( static_cast<int>(apoGMLBoxes.size()),
                                   &apoGMLBoxes[0] );

/* -------------------------------------------------------------------- */
/*      Cleanup working boxes.                                          */
/* -------------------------------------------------------------------- */
    for( auto& poGMLBox: apoGMLBoxes )
        delete poGMLBox;

    for( const auto& osTmpFile: aosTmpFiles )
    {
        VSIUnlink(osTmpFile);
    }

    return poGMLData;
}

/************************************************************************/
/*                 CreateGDALMultiDomainMetadataXML()                   */
/************************************************************************/

CPLXMLNode* GDALJP2Metadata::CreateGDALMultiDomainMetadataXML(
    GDALDataset* poSrcDS, int bMainMDDomainOnly )
{
    GDALMultiDomainMetadata oLocalMDMD;
    char** papszSrcMD = CSLDuplicate(poSrcDS->GetMetadata());
    /* Remove useless metadata */
    papszSrcMD = CSLSetNameValue(papszSrcMD, GDALMD_AREA_OR_POINT, nullptr);
    papszSrcMD = CSLSetNameValue(papszSrcMD, "TIFFTAG_RESOLUTIONUNIT", nullptr);
    papszSrcMD = CSLSetNameValue(papszSrcMD, "TIFFTAG_XREpsMasterXMLNodeSOLUTION", nullptr);
    papszSrcMD = CSLSetNameValue(papszSrcMD, "TIFFTAG_YRESOLUTION", nullptr);
    papszSrcMD = CSLSetNameValue(papszSrcMD, "Corder", nullptr); /* from JP2KAK */
    if( poSrcDS->GetDriver() != nullptr &&
        EQUAL(poSrcDS->GetDriver()->GetDescription(), "JP2ECW") )
    {
        papszSrcMD = CSLSetNameValue(papszSrcMD, "COMPRESSION_RATE_TARGET", nullptr);
        papszSrcMD = CSLSetNameValue(papszSrcMD, "COLORSPACE", nullptr);
        papszSrcMD = CSLSetNameValue(papszSrcMD, "VERSION", nullptr);
    }

    bool bHasMD = false;
    if( papszSrcMD && *papszSrcMD )
    {
        bHasMD = true;
        oLocalMDMD.SetMetadata(papszSrcMD);
    }
    CSLDestroy(papszSrcMD);

    if( !bMainMDDomainOnly )
    {
        char** papszMDList = poSrcDS->GetMetadataDomainList();
        for( char** papszMDListIter = papszMDList;
             papszMDListIter && *papszMDListIter;
             ++papszMDListIter )
        {
            if( !EQUAL(*papszMDListIter, "") &&
                !EQUAL(*papszMDListIter, "IMAGE_STRUCTURE") &&
                !EQUAL(*papszMDListIter, "DERIVED_SUBDATASETS") &&
                !EQUAL(*papszMDListIter, "JPEG2000") &&
                !STARTS_WITH_CI(*papszMDListIter, "xml:BOX_") &&
                !EQUAL(*papszMDListIter, "xml:gml.root-instance") &&
                !EQUAL(*papszMDListIter, "xml:XMP") &&
                !EQUAL(*papszMDListIter, "xml:IPR") )
            {
                papszSrcMD = poSrcDS->GetMetadata(*papszMDListIter);
                if( papszSrcMD && *papszSrcMD )
                {
                    bHasMD = true;
                    oLocalMDMD.SetMetadata(papszSrcMD, *papszMDListIter);
                }
            }
        }
        CSLDestroy(papszMDList);
    }

    CPLXMLNode* psMasterXMLNode = nullptr;
    if( bHasMD )
    {
        CPLXMLNode* psXMLNode = oLocalMDMD.Serialize();
        psMasterXMLNode = CPLCreateXMLNode( nullptr, CXT_Element,
                                                    "GDALMultiDomainMetadata" );
        psMasterXMLNode->psChild = psXMLNode;
    }
    return psMasterXMLNode;
}

/************************************************************************/
/*                CreateGDALMultiDomainMetadataXMLBox()                 */
/************************************************************************/

GDALJP2Box *GDALJP2Metadata::CreateGDALMultiDomainMetadataXMLBox(
                                       GDALDataset* poSrcDS,
                                       int bMainMDDomainOnly )
{
    CPLXMLTreeCloser psMasterXMLNode(CreateGDALMultiDomainMetadataXML(
                                       poSrcDS, bMainMDDomainOnly ));
    if( psMasterXMLNode == nullptr )
        return nullptr;
    char* pszXML = CPLSerializeXMLTree(psMasterXMLNode.get());
    psMasterXMLNode.reset();

    GDALJP2Box* poBox = new GDALJP2Box();
    poBox->SetType("xml ");
    poBox->SetWritableData(static_cast<int>(strlen(pszXML) + 1),
                           reinterpret_cast<const GByte*>(pszXML));
    CPLFree(pszXML);

    return poBox;
}

/************************************************************************/
/*                         WriteXMLBoxes()                              */
/************************************************************************/

GDALJP2Box** GDALJP2Metadata::CreateXMLBoxes( GDALDataset* poSrcDS,
                                              int* pnBoxes )
{
    GDALJP2Box** papoBoxes = nullptr;
    *pnBoxes = 0;
    char** papszMDList = poSrcDS->GetMetadataDomainList();
    for( char** papszMDListIter = papszMDList;
         papszMDListIter && *papszMDListIter;
         ++papszMDListIter )
    {
        /* Write metadata that look like originating from JP2 XML boxes */
        /* as a standalone JP2 XML box */
        if( STARTS_WITH_CI(*papszMDListIter, "xml:BOX_") )
        {
            char** papszSrcMD = poSrcDS->GetMetadata(*papszMDListIter);
            if( papszSrcMD && *papszSrcMD )
            {
                GDALJP2Box* poBox = new GDALJP2Box();
                poBox->SetType("xml ");
                poBox->SetWritableData(static_cast<int>(strlen(*papszSrcMD) + 1),
                                       reinterpret_cast<const GByte*>(*papszSrcMD));
                papoBoxes = static_cast<GDALJP2Box**>(CPLRealloc(papoBoxes,
                                        sizeof(GDALJP2Box*) * (*pnBoxes + 1)));
                papoBoxes[(*pnBoxes)++] = poBox;
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
    GDALJP2Box* poBox = nullptr;
    if( papszSrcMD && * papszSrcMD )
    {
        poBox = GDALJP2Box::CreateUUIDBox(xmp_uuid,
                                          static_cast<int>(strlen(*papszSrcMD) + 1),
                                          reinterpret_cast<const GByte*>(*papszSrcMD));
    }
    return poBox;
}

/************************************************************************/
/*                          CreateIPRBox()                              */
/************************************************************************/

GDALJP2Box *GDALJP2Metadata::CreateIPRBox ( GDALDataset* poSrcDS )
{
    char** papszSrcMD = poSrcDS->GetMetadata("xml:IPR");
    GDALJP2Box* poBox = nullptr;
    if( papszSrcMD && * papszSrcMD )
    {
        poBox = new GDALJP2Box();
        poBox->SetType("jp2i");
        poBox->SetWritableData(static_cast<int>(strlen(*papszSrcMD) + 1),
                               reinterpret_cast<const GByte*>(*papszSrcMD));
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

/*! @endcond */
