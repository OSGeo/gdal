/******************************************************************************
 *
 * Project:  HTF Translator
 * Purpose:  Implements OGRHTFDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_htf.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRHTFDataSource()                          */
/************************************************************************/

OGRHTFDataSource::OGRHTFDataSource() :
    pszName(NULL),
    papoLayers(NULL),
    nLayers(0),
    poMetadataLayer(NULL)
{}

/************************************************************************/
/*                         ~OGRHTFDataSource()                          */
/************************************************************************/

OGRHTFDataSource::~OGRHTFDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
    delete poMetadataLayer;

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRHTFDataSource::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRHTFDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer* OGRHTFDataSource::GetLayerByName( const char* pszLayerName )
{
    if (nLayers == 0)
        return NULL;
    if (EQUAL(pszLayerName, "polygon"))
        return papoLayers[0];
    if (EQUAL(pszLayerName, "sounding"))
        return papoLayers[1];
    if (EQUAL(pszLayerName, "metadata"))
        return poMetadataLayer;
    return NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRHTFDataSource::Open( const char * pszFilename )

{
    pszName = CPLStrdup( pszFilename );

// --------------------------------------------------------------------
//      Does this appear to be a .htf file?
// --------------------------------------------------------------------

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    bool bEndOfHTFHeader = false;
    bool bIsSouth = false;
    bool bGeodeticDatumIsWGS84 = false;
    bool bIsUTM = false;
    int nZone = 0;
    int nLines = 0;
    bool bHasSWEasting = false;
    bool bHasSWNorthing = false;
    bool bHasNEEasting = false;
    bool bHasNENorthing = false;
    double dfSWEasting = 0;
    double dfSWNorthing = 0;
    double dfNEEasting = 0;
    double dfNENorthing = 0;
    std::vector<CPLString> aosMD;
    int nTotalSoundings = 0;

    const char* pszLine = NULL;
    while( (pszLine = CPLReadLine2L(fp, 1024, NULL)) != NULL)
    {
        nLines ++;
        if (nLines == 1000)
        {
            break;
        }
        if (*pszLine == ';' || *pszLine == '\0')
            continue;

        if (strcmp(pszLine, "END OF HTF HEADER") == 0)
        {
            bEndOfHTFHeader = true;
            break;
        }

        aosMD.push_back(pszLine);

        if (STARTS_WITH(pszLine, "GEODETIC DATUM: "))
        {
            if (strcmp(pszLine + 16, "WG84") == 0 ||
                strcmp(pszLine + 16, "WGS84") == 0)
                bGeodeticDatumIsWGS84 = true;
            else
            {
                VSIFCloseL(fp);
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported datum : %s", pszLine + 16);
                return FALSE;
            }
        }
        else if (STARTS_WITH(pszLine, "NE LATITUDE: -"))
            bIsSouth = true;
        else if (STARTS_WITH(pszLine, "GRID REFERENCE SYSTEM: "))
        {
            if (STARTS_WITH(pszLine + 23, "UTM"))
                bIsUTM = true;
            else
            {
                VSIFCloseL(fp);
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported grid : %s", pszLine + 23);
                return false;
            }
        }
        else if (STARTS_WITH(pszLine, "GRID ZONE: "))
        {
            nZone = atoi(pszLine + 11);
        }
        else if (STARTS_WITH(pszLine, "SW GRID COORDINATE - EASTING: "))
        {
            bHasSWEasting = true;
            dfSWEasting = CPLAtof(pszLine + 30);
        }
        else if (STARTS_WITH(pszLine, "SW GRID COORDINATE - NORTHING: "))
        {
            bHasSWNorthing = true;
            dfSWNorthing = CPLAtof(pszLine + 31);
        }
        else if (STARTS_WITH(pszLine, "NE GRID COORDINATE - EASTING: "))
        {
            bHasNEEasting = true;
            dfNEEasting = CPLAtof(pszLine + 30);
        }
        else if (STARTS_WITH(pszLine, "NE GRID COORDINATE - NORTHING: "))
        {
            bHasNENorthing = true;
            dfNENorthing = CPLAtof(pszLine + 31);
        }
        else if (STARTS_WITH(pszLine, "TOTAL SOUNDINGS: "))
        {
            nTotalSoundings = atoi(pszLine + 17);
        }
    }

    VSIFCloseL(fp);

    if (!bEndOfHTFHeader)
        return FALSE;
    if (!bGeodeticDatumIsWGS84)
        return FALSE;
    if (!bIsUTM)
        return FALSE;
    if (nZone == 0)
        return FALSE;

    nLayers = 2;
    papoLayers = static_cast<OGRHTFLayer **>(
        CPLMalloc(sizeof(OGRHTFLayer*) * 2) );
    papoLayers[0] = new OGRHTFPolygonLayer(pszFilename, nZone, !bIsSouth);
    papoLayers[1] = new OGRHTFSoundingLayer(
        pszFilename, nZone, !bIsSouth, nTotalSoundings);

    if (bHasSWEasting && bHasSWNorthing && bHasNEEasting && bHasNENorthing)
    {
        papoLayers[0]->SetExtent(
            dfSWEasting, dfSWNorthing, dfNEEasting, dfNENorthing);
        papoLayers[1]->SetExtent(
            dfSWEasting, dfSWNorthing, dfNEEasting, dfNENorthing);
    }

    poMetadataLayer = new OGRHTFMetadataLayer(aosMD);

    return TRUE;
}
