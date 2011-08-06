/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Implements OGRGFTDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

    bMustCleanPersistant = FALSE;
}

/************************************************************************/
/*                         ~OGRGFTDataSource()                          */
/************************************************************************/

OGRGFTDataSource::~OGRGFTDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (bMustCleanPersistant)
    {
        char** papszOptions = CSLAddString(NULL, CPLSPrintf("CLOSE_PERSISTENT=GFT:%p", this));
        CPLHTTPFetch( GetAPIURL(), papszOptions);
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
    char* pszName = CPLStrdup(pszLayerName);
    char *pszLeftParenthesis = strchr(pszName, '(');
    if( pszLeftParenthesis != NULL )
    {
        *pszLeftParenthesis = '\0';
        pszGeomColumnName = CPLStrdup(pszLeftParenthesis+1);
        int len = strlen(pszGeomColumnName);
        if (len > 0 && pszGeomColumnName[len - 1] == ')')
            pszGeomColumnName[len - 1] = '\0';
    }
    
    CPLString osTableId(pszName);
    for(int i=0;i<nLayers;i++)
    {
        if( strcmp(papoLayers[i]->GetName(), pszName) == 0)
        {
            osTableId = ((OGRGFTTableLayer*)papoLayers[i])->GetTableId();
            break;
        }
    }

    poLayer = new OGRGFTTableLayer(this, pszLayerName, osTableId,
                                   pszGeomColumnName);
    CPLFree(pszName);
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
/*                             FetchAuth()                              */
/************************************************************************/

int OGRGFTDataSource::FetchAuth(const char* pszEmail, const char* pszPassword)
{

    char** papszOptions = NULL;

    const char* pszAuthURL = CPLGetConfigOption("GFT_AUTH_URL", NULL);
    if (pszAuthURL == NULL)
        pszAuthURL = "https://www.google.com/accounts/ClientLogin";

    papszOptions = CSLAddString(papszOptions, "HEADERS=Content-Type: application/x-www-form-urlencoded");
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("POSTFIELDS=Email=%s&Passwd=%s&service=fusiontables", pszEmail, pszPassword));
    CPLHTTPResult * psResult = CPLHTTPFetch( pszAuthURL, papszOptions);
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

    CPLDebug("GFT", "Auth key : %s", osAuth.c_str());

    CPLHTTPDestroyResult(psResult);

    return TRUE;
}

/************************************************************************/
/*                      OGRGFTGetOptionValue()                          */
/************************************************************************/

CPLString OGRGFTGetOptionValue(const char* pszFilename,
                               const char* pszOptionName)
{
    CPLString osOptionName(pszOptionName);
    osOptionName += "=";
    const char* pszOptionValue = strstr(pszFilename, osOptionName);
    if (!pszOptionValue)
        return "";

    CPLString osOptionValue(pszOptionValue + strlen(osOptionName));
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
    if (!EQUALN(pszFilename, "GFT:", 4))
        return FALSE;

    bReadWrite = bUpdateIn;

    pszName = CPLStrdup( pszFilename );

    const char* pszEmail = CPLGetConfigOption("GFT_EMAIL", NULL);
    CPLString osEmail = OGRGFTGetOptionValue(pszFilename, "email");
    if (osEmail.size() != 0)
        pszEmail = osEmail.c_str();

    const char* pszPassword = CPLGetConfigOption("GFT_PASSWORD", NULL);
    CPLString osPassword = OGRGFTGetOptionValue(pszFilename, "password");
    if (osPassword.size() != 0)
        pszPassword = osPassword.c_str();

    const char* pszAuth = CPLGetConfigOption("GFT_AUTH", NULL);
    osAuth = OGRGFTGetOptionValue(pszFilename, "auth");
    if (osAuth.size() == 0 && pszAuth != NULL)
        osAuth = pszAuth;

    CPLString osTables = OGRGFTGetOptionValue(pszFilename, "tables");

    bUseHTTPS = strstr(pszFilename, "protocol=https") != NULL;

    if (osAuth.size() == 0 && (pszEmail == NULL || pszPassword == NULL))
    {
        if (osTables.size() == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Unauthenticated access requires explicit tables= parameter");
            return FALSE;
        }
    }
    else if (osAuth.size() == 0 && !FetchAuth(pszEmail, pszPassword))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Authentication failed");
        return FALSE;
    }

    if (osTables.size() != 0)
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
        strncmp(pszLine, "table id,name", strlen("table id,name")) != 0)
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

    OGRGFTTableLayer* poLayer = new OGRGFTTableLayer(this, pszName);
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
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osAuth.size() == 0)
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
    bMustCleanPersistant = TRUE;
    papszOptions = CSLAddString(papszOptions,
        CPLSPrintf("HEADERS=Authorization: GoogleLogin auth=%s", osAuth.c_str()));
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
    char** papszOptions = CSLAddString(AddHTTPOptions(), osSQL);
    CPLDebug("GFT", "Run %s", pszUnescapedSQL);
    CPLHTTPResult * psResult = CPLHTTPFetch( GetAPIURL(), papszOptions);
    CSLDestroy(papszOptions);
    if (psResult && psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "HTML error page returned by server");
        CPLHTTPDestroyResult(psResult);
        psResult = NULL;
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
    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
        return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"DELLAYER:",9) )
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
