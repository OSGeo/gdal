/******************************************************************************
 *
 * Project:  AeronavFAA Translator
 * Purpose:  Implements OGRAeronavFAADataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_aeronavfaa.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                      OGRAeronavFAADataSource()                       */
/************************************************************************/

OGRAeronavFAADataSource::OGRAeronavFAADataSource() :
    pszName(NULL),
    papoLayers(NULL),
    nLayers(0)
{}

/************************************************************************/
/*                     ~OGRAeronavFAADataSource()                       */
/************************************************************************/

OGRAeronavFAADataSource::~OGRAeronavFAADataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAeronavFAADataSource::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRAeronavFAADataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRAeronavFAADataSource::Open( const char * pszFilename )

{
    pszName = CPLStrdup( pszFilename );

// --------------------------------------------------------------------
//      Does this appear to be a .dat file?
// --------------------------------------------------------------------

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    char szBuffer[10000];
    const int nbRead = static_cast<int>(
        VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fp));
    szBuffer[nbRead] = '\0';

    const bool bIsDOF =
        szBuffer[128] == 13 && szBuffer[128+1] == 10 &&
        szBuffer[130+128] == 13 && szBuffer[130+129] == 10 &&
        szBuffer[2*130+128] == 13 && szBuffer[2*130+129] == 10 &&
        STARTS_WITH(
            szBuffer + 3 * 130,
            "-----------------------------------------------------------------"
            "-------------------------------------------------------- ");

    const bool bIsNAVAID =
        szBuffer[132] == 13 && szBuffer[132+1] == 10 &&
        STARTS_WITH(szBuffer + 20 - 1, "CREATION DATE") &&
        szBuffer[134 + 132] == 13 && szBuffer[134 + 132+1] == 10;

    const bool bIsIAP =
        strstr(szBuffer,
               "INSTRUMENT APPROACH PROCEDURE NAVAID & FIX DATA") != NULL &&
        szBuffer[85] == 13 && szBuffer[85+1] == 10;

    bool bIsROUTE = STARTS_WITH(
        szBuffer,
        "           UNITED STATES GOVERNMENT FLIGHT INFORMATION PUBLICATION"
        "             149343") &&
        szBuffer[85] == 13 && szBuffer[85+1] == 10;

    // TODO(schwehr): Fold into bool bIsROUTE so it can be const.
    if( bIsIAP )
        bIsROUTE = false;

    if( bIsDOF )
    {
        VSIFSeekL( fp, 0, SEEK_SET );
        nLayers = 1;
        papoLayers = static_cast<OGRLayer **>(CPLMalloc(sizeof(OGRLayer*)));
        papoLayers[0] = new OGRAeronavFAADOFLayer(fp, CPLGetBasename(pszFilename));
        return TRUE;
    }
    else if( bIsNAVAID )
    {
        VSIFSeekL( fp, 0, SEEK_SET );
        nLayers = 1;
        papoLayers = static_cast<OGRLayer **>(CPLMalloc(sizeof(OGRLayer*)));
        papoLayers[0] = new OGRAeronavFAANAVAIDLayer(fp, CPLGetBasename(pszFilename));
        return TRUE;
    }
    else if (bIsIAP)
    {
        VSIFSeekL( fp, 0, SEEK_SET );
        nLayers = 1;
        papoLayers = static_cast<OGRLayer **>(CPLMalloc(sizeof(OGRLayer*)));
        papoLayers[0] = new OGRAeronavFAAIAPLayer(fp, CPLGetBasename(pszFilename));
        return TRUE;
    }
    else if (bIsROUTE)
    {
        const bool bIsDPOrSTARS =
            strstr(szBuffer, "DPs - DEPARTURE PROCEDURES") != NULL ||
            strstr(szBuffer, "STARS - STANDARD TERMINAL ARRIVALS") != NULL;
        VSIFSeekL( fp, 0, SEEK_SET );
        nLayers = 1;
        papoLayers = static_cast<OGRLayer **>(CPLMalloc(sizeof(OGRLayer*)));
        papoLayers[0] = new OGRAeronavFAARouteLayer(fp, CPLGetBasename(pszFilename), bIsDPOrSTARS);
        return TRUE;
    }
    else
    {
        VSIFCloseL(fp);
        return FALSE;
    }
}
