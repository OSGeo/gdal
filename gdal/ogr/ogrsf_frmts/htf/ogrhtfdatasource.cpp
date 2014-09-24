/******************************************************************************
 * $Id$
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

#include "ogr_htf.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRHTFDataSource()                          */
/************************************************************************/

OGRHTFDataSource::OGRHTFDataSource()

{
    papoLayers = NULL;
    nLayers = 0;
    poMetadataLayer = NULL;

    pszName = NULL;
}

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

int OGRHTFDataSource::TestCapability( CPL_UNUSED const char * pszCap )
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
    else
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

int OGRHTFDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = CPLStrdup( pszFilename );

// -------------------------------------------------------------------- 
//      Does this appear to be a .htf file?
// --------------------------------------------------------------------

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    char szBuffer[11];
    int nbRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fp);
    szBuffer[nbRead] = '\0';

    int bIsHTF = strcmp(szBuffer, "HTF HEADER") == 0;
    if (!bIsHTF)
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    VSIFSeekL(fp, 0, SEEK_SET);

    const char* pszLine;
    int bEndOfHTFHeader = FALSE;
    int bIsSouth = FALSE;
    int bGeodeticDatumIsWGS84 = FALSE;
    int bIsUTM = FALSE;
    int nZone = 0;
    int nLines = 0;
    int bHasSWEasting = FALSE, bHasSWNorthing = FALSE, bHasNEEasting = FALSE, bHasNENorthing = FALSE;
    double dfSWEasting = 0, dfSWNorthing = 0, dfNEEasting = 0, dfNENorthing = 0;
    std::vector<CPLString> aosMD;
    int nTotalSoundings = 0;
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
            bEndOfHTFHeader = TRUE;
            break;
        }

        aosMD.push_back(pszLine);

        if (strncmp(pszLine, "GEODETIC DATUM: ", 16) == 0)
        {
            if (strcmp(pszLine + 16, "WG84") == 0 ||
                strcmp(pszLine + 16, "WGS84") == 0)
                bGeodeticDatumIsWGS84 = TRUE;
            else
            {
                VSIFCloseL(fp);
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported datum : %s", pszLine + 16);
                return FALSE;
            }
        }
        else if (strncmp(pszLine, "NE LATITUDE: -", 14) == 0)
            bIsSouth = TRUE;
        else if (strncmp(pszLine, "GRID REFERENCE SYSTEM: ", 23) == 0)
        {
            if (strncmp(pszLine + 23, "UTM", 3) == 0)
                bIsUTM = TRUE;
            else
            {
                VSIFCloseL(fp);
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported grid : %s", pszLine + 23);
                return FALSE;
            }
        }
        else if (strncmp(pszLine, "GRID ZONE: ", 11) == 0)
        {
            nZone = atoi(pszLine + 11);
        }
        else if (strncmp(pszLine, "SW GRID COORDINATE - EASTING: ", 30) == 0)
        {
            bHasSWEasting = TRUE;
            dfSWEasting = atof(pszLine + 30);
        }
        else if (strncmp(pszLine, "SW GRID COORDINATE - NORTHING: ", 31) == 0)
        {
            bHasSWNorthing = TRUE;
            dfSWNorthing = atof(pszLine + 31);
        }
        else if (strncmp(pszLine, "NE GRID COORDINATE - EASTING: ", 30) == 0)
        {
            bHasNEEasting = TRUE;
            dfNEEasting = atof(pszLine + 30);
        }
        else if (strncmp(pszLine, "NE GRID COORDINATE - NORTHING: ", 31) == 0)
        {
            bHasNENorthing = TRUE;
            dfNENorthing = atof(pszLine + 31);
        }
        else if (strncmp(pszLine, "TOTAL SOUNDINGS: ", 17) == 0)
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
    papoLayers = (OGRHTFLayer**) CPLMalloc(sizeof(OGRHTFLayer*) * 2);
    papoLayers[0] = new OGRHTFPolygonLayer(pszFilename, nZone, !bIsSouth);
    papoLayers[1] = new OGRHTFSoundingLayer(pszFilename, nZone, !bIsSouth, nTotalSoundings);

    if (bHasSWEasting && bHasSWNorthing && bHasNEEasting && bHasNENorthing)
    {
        papoLayers[0]->SetExtent(dfSWEasting, dfSWNorthing, dfNEEasting, dfNENorthing);
        papoLayers[1]->SetExtent(dfSWEasting, dfSWNorthing, dfNEEasting, dfNENorthing);
    }

    poMetadataLayer = new OGRHTFMetadataLayer(aosMD);

    return TRUE;
}
