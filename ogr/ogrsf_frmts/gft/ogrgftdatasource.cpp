/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Implements OGRGFTDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_gft.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_http.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRGFTDataSource()                          */
/************************************************************************/

OGRGFTDataSource::OGRGFTDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
}

/************************************************************************/
/*                         ~OGRGFTDataSource()                          */
/************************************************************************/

OGRGFTDataSource::~OGRGFTDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGFTDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGFTDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                             FetchAuth()                              */
/************************************************************************/

int OGRGFTDataSource::FetchAuth(const char* pszEmail, const char* pszPassword)
{

    char** papszOptions = NULL;

    papszOptions = CSLAddString(papszOptions, "HEADERS=Content-Type: application/x-www-form-urlencoded");
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("POSTFIELDS=Email=%s&Passwd=%s&service=fusiontables", pszEmail, pszPassword));
    CPLHTTPResult * psResult = CPLHTTPFetch( "https://www.google.com/accounts/ClientLogin", papszOptions);
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
        return FALSE;

    const char* pszAuth = NULL;
    if (psResult->pabyData == NULL ||
        psResult->pszErrBuf != NULL ||
        (pszAuth = strstr((const char*)psResult->pabyData, "Auth=")) == NULL)
    {
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }
    osAuth = pszAuth + 5;
    pszAuth = NULL;

    while(osAuth.size() &&
          (osAuth[osAuth.size()-1] == 13 || osAuth[osAuth.size()-1] == 10))
        osAuth.resize(osAuth.size()-1);

    CPLDebug("GFT", "Auth=%s", osAuth.c_str());

    CPLHTTPDestroyResult(psResult);

    return TRUE;
}

/************************************************************************/
/*                         GotoNextLine()                               */
/************************************************************************/

static char* OGRGFTDataSourceGotoNextLine(char* pszData)
{
    char* pszNextLine = strchr(pszData, '\n');
    if (pszNextLine)
        return pszNextLine + 1;
    return NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGFTDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (!EQUALN(pszFilename, "GFT:", 4))
        return FALSE;

    pszName = CPLStrdup( pszFilename );

    const char* pszEmail = CPLGetConfigOption("GFT_EMAIL", NULL);
    const char* pszPassword = CPLGetConfigOption("GFT_PASSWORD", NULL);

    if (pszEmail == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GFT needs a Google account email to be specified with GFT_EMAIL configuration option");
        return FALSE;
    }

    if (pszPassword == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GFT needs a Google account password to be specified with GFT_PASSWORD configuration option");
        return FALSE;
    }

    if (!FetchAuth(pszEmail, pszPassword))
        return FALSE;

    /* Get list of tables */
    char** papszOptions = NULL;
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("HEADERS=Authorization: GoogleLogin auth=%s", osAuth.c_str()));
    papszOptions = CSLAddString(papszOptions, "POSTFIELDS=sql=SHOW TABLES");
    CPLHTTPResult * psResult = CPLHTTPFetch( "https://www.google.com/fusiontables/api/query", papszOptions);
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
        return FALSE;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        psResult->pszErrBuf != NULL ||
        strncmp(pszLine, "table id,name", strlen("table id,name")) != 0)
    {
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }

    pszLine = OGRGFTDataSourceGotoNextLine(pszLine);
    while(pszLine != NULL && *pszLine != 0)
    {
        char* pszNextLine = OGRGFTDataSourceGotoNextLine(pszLine);
        if (pszNextLine)
            pszNextLine[-1] = 0;

        char** papszTokens = CSLTokenizeString2(pszLine, ",", 0);
        if (CSLCount(papszTokens) == 2)
        {
            papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers ++] = new OGRGFTLayer(this, papszTokens[1], papszTokens[0]);
        }
        CSLDestroy(papszTokens);

        pszLine = pszNextLine;
    }

    CPLHTTPDestroyResult(psResult);

    return TRUE;
}
