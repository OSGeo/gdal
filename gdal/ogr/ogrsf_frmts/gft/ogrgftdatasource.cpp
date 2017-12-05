/******************************************************************************
 *
 * Project:  Google Fusion Table Translator
 * Purpose:  Implements OGRGFTDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$")

#define GDAL_API_KEY "AIzaSyA_2h1_wXMOLHNSVeo-jf1ACME-M1XMgP0"
#define FUSION_TABLE_SCOPE "https://www.googleapis.com/Fauth/fusiontables"

/************************************************************************/
/*                          OGRGFTDataSource()                          */
/************************************************************************/

OGRGFTDataSource::OGRGFTDataSource() :
    pszName(NULL),
    papoLayers(NULL),
    nLayers(0),
    bReadWrite(FALSE),
    bUseHTTPS(FALSE),
    bMustCleanPersistent(FALSE)
{}

/************************************************************************/
/*                         ~OGRGFTDataSource()                          */
/************************************************************************/

OGRGFTDataSource::~OGRGFTDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (bMustCleanPersistent)
    {
        char** papszOptions = NULL;
        papszOptions = CSLSetNameValue(papszOptions, "CLOSE_PERSISTENT", CPLSPrintf("GFT:%p", this));
        CPLHTTPDestroyResult( CPLHTTPFetch( GetAPIURL(), papszOptions) );
        CSLDestroy(papszOptions);
    }

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
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRGFTDataSource::GetLayerByName(const char * pszLayerName)
{
    OGRLayer* poLayer = OGRDataSource::GetLayerByName(pszLayerName);
    if (poLayer)
        return poLayer;

    char* pszGeomColumnName = NULL;
    char* l_pszName = CPLStrdup(pszLayerName);
    char *pszLeftParenthesis = strchr(l_pszName, '(');
    if( pszLeftParenthesis != NULL )
    {
        *pszLeftParenthesis = '\0';
        pszGeomColumnName = CPLStrdup(pszLeftParenthesis+1);
        int len = static_cast<int>(strlen(pszGeomColumnName));
        if (len > 0 && pszGeomColumnName[len - 1] == ')')
            pszGeomColumnName[len - 1] = '\0';
    }

    CPLString osTableId(l_pszName);
    for(int i=0;i<nLayers;i++)
    {
        if( strcmp(papoLayers[i]->GetName(), l_pszName) == 0)
        {
            osTableId = ((OGRGFTTableLayer*)papoLayers[i])->GetTableId();
            break;
        }
    }

    poLayer = new OGRGFTTableLayer(this, pszLayerName, osTableId,
                                   pszGeomColumnName);
    CPLFree(l_pszName);
    CPLFree(pszGeomColumnName);
    if (poLayer->GetLayerDefn()->GetFieldCount() == 0)
    {
        delete poLayer;
        return NULL;
    }
    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers ++] = poLayer;
    return poLayer;
}

/************************************************************************/
/*                      OGRGFTGetOptionValue()                          */
/************************************************************************/

static CPLString OGRGFTGetOptionValue(const char* pszFilename,
                               const char* pszOptionName)
{
    CPLString osOptionName(pszOptionName);
    osOptionName += "=";
    const char* pszOptionValue = strstr(pszFilename, osOptionName);
    if (!pszOptionValue)
        return "";

    CPLString osOptionValue(pszOptionValue + osOptionName.size());
    const char* pszSpace = strchr(osOptionValue.c_str(), ' ');
    if (pszSpace)
        osOptionValue.resize(pszSpace - osOptionValue.c_str());
    return osOptionValue;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGFTDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    bReadWrite = bUpdateIn;

    pszName = CPLStrdup( pszFilename );

    osAuth = OGRGFTGetOptionValue(pszFilename, "auth");
    if (osAuth.empty())
        osAuth = CPLGetConfigOption("GFT_AUTH", "");

    osRefreshToken = OGRGFTGetOptionValue(pszFilename, "refresh");
    if (osRefreshToken.empty())
        osRefreshToken = CPLGetConfigOption("GFT_REFRESH_TOKEN", "");

    osAPIKey = CPLGetConfigOption("GFT_APIKEY", GDAL_API_KEY);

    CPLString osTables = OGRGFTGetOptionValue(pszFilename, "tables");

    bUseHTTPS = TRUE;

    osAccessToken = OGRGFTGetOptionValue(pszFilename, "access");
    if (osAccessToken.empty())
        osAccessToken = CPLGetConfigOption("GFT_ACCESS_TOKEN","");
    if (osAccessToken.empty() && !osRefreshToken.empty())
    {
        osAccessToken.Seize(GOA2GetAccessToken(osRefreshToken,
                                               FUSION_TABLE_SCOPE));
        if (osAccessToken.empty())
            return FALSE;
    }
    /* coverity[copy_paste_error] */
    if (osAccessToken.empty() && !osAuth.empty())
    {
        osRefreshToken.Seize(GOA2GetRefreshToken(osAuth, FUSION_TABLE_SCOPE));
        if (osRefreshToken.empty())
            return FALSE;
    }

    if (osAccessToken.empty())
    {
        if (osTables.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Unauthenticated access requires explicit tables= parameter");
            return FALSE;
        }
    }

    if (!osTables.empty())
    {
        char** papszTables = CSLTokenizeString2(osTables, ",", 0);
        for(int i=0;papszTables && papszTables[i];i++)
        {
            papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers ++] = new OGRGFTTableLayer(this, papszTables[i], papszTables[i]);
        }
        CSLDestroy(papszTables);
        return TRUE;
    }

    /* Get list of tables */
    CPLHTTPResult * psResult = RunSQL("SHOW TABLES");

    if (psResult == NULL)
        return FALSE;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        psResult->pszErrBuf != NULL ||
        !STARTS_WITH(pszLine, "table id,name"))
    {
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }

    pszLine = OGRGFTGotoNextLine(pszLine);
    while(pszLine != NULL && *pszLine != 0)
    {
        char* pszNextLine = OGRGFTGotoNextLine(pszLine);
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
            papoLayers[nLayers ++] = new OGRGFTTableLayer(this, osLayerName, osTableId);
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
    const char* pszAPIURL = CPLGetConfigOption("GFT_API_URL", NULL);
    if (pszAPIURL)
        return pszAPIURL;
    else if (bUseHTTPS)
        return "https://www.googleapis.com/fusiontables/v1/query";
    else
        return "http://www.googleapis.com/fusiontables/v1/query";
}

/************************************************************************/
/*                          ICreateLayer()                              */
/************************************************************************/

OGRLayer   *OGRGFTDataSource::ICreateLayer( const char *pszNameIn,
                                            CPL_UNUSED OGRSpatialReference *poSpatialRef,
                                            OGRwkbGeometryType eGType,
                                            char ** papszOptions )
{
    if (!bReadWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return NULL;
    }

    if (osAccessToken.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in unauthenticated mode");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */

    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszNameIn,papoLayers[iLayer]->GetName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                DeleteLayer( pszNameIn );
                break;
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszNameIn );
                return NULL;
            }
        }
    }

    OGRGFTTableLayer* poLayer = new OGRGFTTableLayer(this, pszNameIn);
    poLayer->SetGeometryType(eGType);
    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers ++] = poLayer;
    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRGFTDataSource::DeleteLayer( const char *pszLayerName )

{
/* -------------------------------------------------------------------- */
/*      Try to find layer.                                              */
/* -------------------------------------------------------------------- */
    int iLayer = 0;  // Used after for.
    for( ; iLayer < nLayers; iLayer++ )
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
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osAccessToken.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in unauthenticated mode");
        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    CPLString osTableId = ((OGRGFTTableLayer*)papoLayers[iLayer])->GetTableId();
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

    CPLString osSQL("DROP TABLE ");
    osSQL += osTableId;

    CPLHTTPResult* psResult = RunSQL( osSQL );

    if (psResult == NULL || psResult->nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Table deletion failed (1)");
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
    bMustCleanPersistent = TRUE;

    if( !osAccessToken.empty() )
      papszOptions = CSLAddString(papszOptions,
        CPLSPrintf("HEADERS=Authorization: Bearer %s",
                   osAccessToken.c_str()));

    return CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=GFT:%p", this));
}

/************************************************************************/
/*                               RunSQL()                               */
/************************************************************************/

CPLHTTPResult * OGRGFTDataSource::RunSQL(const char* pszUnescapedSQL)
{
    CPLString osSQL("POSTFIELDS=sql=");
    /* Do post escaping */
    for(int i=0;pszUnescapedSQL[i] != 0;i++)
    {
        const int ch = ((unsigned char*)pszUnescapedSQL)[i];
        if (ch != '&' && ch >= 32 && ch < 128)
            osSQL += (char)ch;
        else
            osSQL += CPLSPrintf("%%%02X", ch);
    }

/* -------------------------------------------------------------------- */
/*      Provide the API Key - used to rate limit access (see            */
/*      GFT_APIKEY config)                                              */
/* -------------------------------------------------------------------- */
    osSQL += "&key=";
    osSQL += osAPIKey;

/* -------------------------------------------------------------------- */
/*      Force old style CSV output from calls - maybe we want to        */
/*      migrate to JSON output at some point?                           */
/* -------------------------------------------------------------------- */
    osSQL += "&alt=csv";

/* -------------------------------------------------------------------- */
/*      Collection the header options and execute request.              */
/* -------------------------------------------------------------------- */
    char** papszOptions = CSLAddString(AddHTTPOptions(), osSQL);
    CPLHTTPResult * psResult = CPLHTTPFetch( GetAPIURL(), papszOptions);
    CSLDestroy(papszOptions);

/* -------------------------------------------------------------------- */
/*      Check for some error conditions and report.  HTML Messages      */
/*      are transformed info failure.                                   */
/* -------------------------------------------------------------------- */
    if (psResult && psResult->pszContentType &&
        STARTS_WITH(psResult->pszContentType, "text/html"))
    {
        CPLDebug( "GFT", "RunSQL HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server");
        CPLHTTPDestroyResult(psResult);
        psResult = NULL;
    }
    if (psResult && psResult->pszErrBuf != NULL)
    {
        CPLDebug( "GFT", "RunSQL Error Message:%s", psResult->pszErrBuf );
    }
    else if (psResult && psResult->nStatus != 0)
    {
        CPLDebug( "GFT", "RunSQL Error Status:%d", psResult->nStatus );
    }

    return psResult;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRGFTDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
/* -------------------------------------------------------------------- */
/*      Use generic implementation for recognized dialects              */
/* -------------------------------------------------------------------- */
    if( IsGenericSQLDialect(pszDialect) )
        return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DELLAYER:") )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        DeleteLayer( pszLayerName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create layer.                                                   */
/* -------------------------------------------------------------------- */
    OGRGFTResultLayer *poLayer = NULL;

    CPLString osSQL = pszSQLCommand;
    poLayer = new OGRGFTResultLayer( this, osSQL );
    if (!poLayer->RunSQL())
    {
        delete poLayer;
        return NULL;
    }

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRGFTDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                      OGRGFTGotoNextLine()                            */
/************************************************************************/

char* OGRGFTGotoNextLine(char* pszData)
{
    char* pszNextLine = strchr(pszData, '\n');
    if (pszNextLine)
        return pszNextLine + 1;
    return NULL;
}
