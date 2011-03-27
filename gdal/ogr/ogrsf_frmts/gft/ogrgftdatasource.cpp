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

    bReadWrite = FALSE;
    bUseHTTPS = FALSE;
}

/************************************************************************/
/*                         ~OGRGFTDataSource()                          */
/************************************************************************/

OGRGFTDataSource::~OGRGFTDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    char** papszOptions = CSLAddString(NULL, CPLSPrintf("CLOSE_PERSISTENT=GFT:%p", this));
    CPLHTTPFetch( GetAPIURL(), papszOptions);
    CSLDestroy(papszOptions);

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGFTDataSource::TestCapability( const char * pszCap )

{
    if( bReadWrite && EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else if( bReadWrite && EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;
    else
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

    bReadWrite = bUpdateIn;

    pszName = CPLStrdup( pszFilename );

    const char* pszEmail = CPLGetConfigOption("GFT_EMAIL", NULL);
    const char* pszPassword = CPLGetConfigOption("GFT_PASSWORD", NULL);
    const char* pszTables = strstr(pszFilename, "tables=");

    bUseHTTPS = strstr(pszFilename, "protocol=https") != NULL;

    if (pszEmail == NULL || pszPassword == NULL)
    {
        if (pszTables == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Unauthenticated access requires explicit tables= parameter");
            return FALSE;
        }
    }
    else if (!FetchAuth(pszEmail, pszPassword))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Authentication failed");
        return FALSE;
    }

    if (pszTables)
    {
        CPLString osTables(pszTables + 7);
        const char* pszSpace = strchr(pszTables + 7, ' ');
        if (pszSpace)
            osTables.resize(pszSpace - (pszTables + 7));

        char** papszTables = CSLTokenizeString2(osTables, ",", 0);
        for(int i=0;papszTables && papszTables[i];i++)
        {
            papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers ++] = new OGRGFTLayer(this, papszTables[i], papszTables[i]);
        }
        CSLDestroy(papszTables);
        return TRUE;
    }

    /* Get list of tables */
    char** papszOptions = CSLAddString(AddHTTPOptions(), "POSTFIELDS=sql=SHOW TABLES");
    CPLHTTPResult * psResult = CPLHTTPFetch( GetAPIURL(), papszOptions);
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
            CPLString osTableId(papszTokens[0]);
            CPLString osLayerName(papszTokens[1]);
            for(int i=0;i<nLayers;i++)
            {
                if (strcmp(papoLayers[i]->GetName(), osLayerName) == 0)
                {
                    osLayerName += " (";
                    osLayerName += osTableId;
                    osLayerName += ")";
                    break;
                }
            }
            papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers ++] = new OGRGFTLayer(this, osLayerName, osTableId);
        }
        CSLDestroy(papszTokens);

        pszLine = pszNextLine;
    }

    CPLHTTPDestroyResult(psResult);

    return TRUE;
}

/************************************************************************/
/*                            GetAPIURL()                               */
/************************************************************************/

const char*  OGRGFTDataSource::GetAPIURL() const
{
    if (bUseHTTPS)
        return "https://www.google.com/fusiontables/api/query";
    else
        return "http://www.google.com/fusiontables/api/query";
}

/************************************************************************/
/*                           CreateLayer()                              */
/************************************************************************/

OGRLayer   *OGRGFTDataSource::CreateLayer( const char *pszName,
                                           OGRSpatialReference *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           char ** papszOptions )
{
    if (!bReadWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return NULL;
    }

    if (osAuth.size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in unauthenticated mode");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszName,papoLayers[iLayer]->GetName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                DeleteLayer( pszName );
                break;
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszName );
                return NULL;
            }
        }
    }

    OGRLayer* poLayer = new OGRGFTLayer(this, pszName);
    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers ++] = poLayer;
    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRGFTDataSource::DeleteLayer( const char *pszLayerName )

{
    int iLayer;

/* -------------------------------------------------------------------- */
/*      Try to find layer.                                              */
/* -------------------------------------------------------------------- */
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetName()) )
            break;
    }

    if( iLayer == nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to delete layer '%s', but this layer is not known to OGR.",
                  pszLayerName );
        return;
    }

    DeleteLayer(iLayer);
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRGFTDataSource::DeleteLayer(int iLayer)
{
    if (!bReadWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osAuth.size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in unauthenticated mode");
        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    CPLString osTableId = ((OGRGFTLayer*)papoLayers[iLayer])->GetTableId();
    CPLString osLayerName = GetLayer(iLayer)->GetName();

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLDebug( "GFT", "DeleteLayer(%s)", osLayerName.c_str() );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */

    CPLString osPOST("POSTFIELDS=sql=DROP TABLE ");
    osPOST += osTableId;

    char** papszOptions = CSLAddString(AddHTTPOptions(), osPOST);
    CPLHTTPResult* psResult = CPLHTTPFetch( GetAPIURL(), papszOptions);
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Table deletion failed");
        return OGRERR_FAILURE;
    }

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        !EQUALN(pszLine, "OK", 2) ||
        psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Table deletion failed");
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLHTTPDestroyResult(psResult);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          AddHTTPOptions()                            */
/************************************************************************/

char** OGRGFTDataSource::AddHTTPOptions(char** papszOptions)
{
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("HEADERS=Authorization: GoogleLogin auth=%s", osAuth.c_str()));
    return CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=GFT:%p", this));
}
