/******************************************************************************
 * $Id$
 *
 * Project:  CartoDB Translator
 * Purpose:  Implements OGRCARTODBDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_cartodb.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRCARTODBDataSource()                        */
/************************************************************************/

OGRCARTODBDataSource::OGRCARTODBDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
    pszAccount = NULL;

    bReadWrite = FALSE;
    bUseHTTPS = FALSE;

    bMustCleanPersistant = FALSE;
}

/************************************************************************/
/*                       ~OGRCARTODBDataSource()                        */
/************************************************************************/

OGRCARTODBDataSource::~OGRCARTODBDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (bMustCleanPersistant)
    {
        char** papszOptions = CSLAddString(NULL, CPLSPrintf("CLOSE_PERSISTENT=CARTODB:%p", this));
        CPLHTTPFetch( GetAPIURL(), papszOptions);
        CSLDestroy(papszOptions);
    }

    CPLFree( pszName );
    CPLFree( pszAccount );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCARTODBDataSource::TestCapability( const char * pszCap )

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

OGRLayer *OGRCARTODBDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRCARTODBDataSource::GetLayerByName(const char * pszLayerName)
{
    OGRLayer* poLayer = OGRDataSource::GetLayerByName(pszLayerName);
    return poLayer;
}

/************************************************************************/
/*                     OGRCARTODBGetOptionValue()                       */
/************************************************************************/

CPLString OGRCARTODBGetOptionValue(const char* pszFilename,
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

int OGRCARTODBDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (!EQUALN(pszFilename, "CARTODB:", strlen("CARTODB:")))
        return FALSE;

    bReadWrite = bUpdateIn;

    pszName = CPLStrdup( pszFilename );
    pszAccount = CPLStrdup(pszFilename + strlen("CARTODB:"));
    char* pchSpace = strchr(pszAccount, ' ');
    if( pchSpace )
        *pchSpace = '\0';

    osAPIKey = CPLGetConfigOption("CARTODB_API_KEY", "");

    CPLString osTables = OGRCARTODBGetOptionValue(pszFilename, "tables");

    bUseHTTPS = CSLTestBoolean(CPLGetConfigOption("CARTODB_HTTPS", "YES"));

    if (osTables.size() != 0)
    {
        char** papszTables = CSLTokenizeString2(osTables, ",", 0);
        for(int i=0;papszTables && papszTables[i];i++)
        {
            papoLayers = (OGRLayer**) CPLRealloc(
                papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers ++] = new OGRCARTODBTableLayer(this, papszTables[i]);
        }
        CSLDestroy(papszTables);
        return TRUE;
    }
    
    if( osAPIKey.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "When not specifying tables option, CARTODB_API_KEY must be defined");
        return FALSE;
    }

    json_object* poObj = RunSQL("SELECT CDB_UserTables()");
    if( poObj == NULL )
        return FALSE;
    json_object* poRows = json_object_object_get(poObj, "rows");
    if( poRows == NULL || json_object_get_type(poRows) != json_type_array)
    {
        json_object_put(poObj);
        return FALSE;
    }
    for(int i=0; i < json_object_array_length(poRows); i++)
    {
        json_object* poTableNameObj = json_object_array_get_idx(poRows, i);
        if( poTableNameObj != NULL &&
            json_object_get_type(poTableNameObj) == json_type_object )
        {
            json_object* poVal = json_object_object_get(poTableNameObj, "cdb_usertables");
            if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_string )
            {
                papoLayers = (OGRLayer**) CPLRealloc(
                    papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
                papoLayers[nLayers ++] = new OGRCARTODBTableLayer(
                            this, json_object_get_string(poVal));
            }
        }
    }
    json_object_put(poObj);

    return TRUE;
}

/************************************************************************/
/*                            GetAPIURL()                               */
/************************************************************************/

const char* OGRCARTODBDataSource::GetAPIURL() const
{
    const char* pszAPIURL = CPLGetConfigOption("CARTODB_API_URL", NULL);
    if (pszAPIURL)
        return pszAPIURL;
    else if (bUseHTTPS)
        return CPLSPrintf("https://%s.cartodb.com/api/v2/sql", pszAccount);
    else
        return CPLSPrintf("http://%s.cartodb.com/api/v2/sql", pszAccount);
}

/************************************************************************/
/*                           CreateLayer()                              */
/************************************************************************/

OGRLayer   *OGRCARTODBDataSource::CreateLayer( const char *pszName,
                                           OGRSpatialReference *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           char ** papszOptions )
{
    if (!bReadWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return NULL;
    }

    return NULL;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRCARTODBDataSource::DeleteLayer( const char *pszLayerName )

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

OGRErr OGRCARTODBDataSource::DeleteLayer(int iLayer)
{
    if (!bReadWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          AddHTTPOptions()                            */
/************************************************************************/

char** OGRCARTODBDataSource::AddHTTPOptions(char** papszOptions)
{
    bMustCleanPersistant = TRUE;

    return CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=CARTODB:%p", this));
}

/************************************************************************/
/*                               RunSQL()                               */
/************************************************************************/

json_object* OGRCARTODBDataSource::RunSQL(const char* pszUnescapedSQL)
{
    CPLString osSQL("POSTFIELDS=q=");
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
/*      Provide the API Key                                             */
/* -------------------------------------------------------------------- */
    if( osAPIKey.size() )
    {
        osSQL += "&api_key=";
        osSQL += osAPIKey;
    }

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
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "CARTODB", "RunSQL HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "HTML error page returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult && psResult->pszErrBuf != NULL) 
    {
        CPLDebug( "CARTODB", "RunSQL Error Message:%s", psResult->pszErrBuf );
    }
    else if (psResult && psResult->nStatus != 0) 
    {
        CPLDebug( "CARTODB", "RunSQL Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    
    //CPLDebug( "CARTODB", "RunSQL Response:%s", psResult->pabyData );
    
    json_tokener* jstok = NULL;
    json_object* poObj = NULL;

    jstok = json_tokener_new();
    poObj = json_tokener_parse_ex(jstok, (const char*) psResult->pabyData, -1);
    if( jstok->err != json_tokener_success)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "JSON parsing error: %s (at offset %d)",
                    json_tokener_errors[jstok->err], jstok->char_offset);
        json_tokener_free(jstok);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    json_tokener_free(jstok);

    CPLHTTPDestroyResult(psResult);

    if( poObj != NULL )
    {
        if( json_object_get_type(poObj) == json_type_object )
        {
            json_object* poError = json_object_object_get(poObj, "error");
            if( poError != NULL && json_object_get_type(poError) == json_type_array &&
                json_object_array_length(poError) > 0 )
            {
                poError = json_object_array_get_idx(poError, 0);
                if( poError != NULL && json_object_get_type(poError) == json_type_string )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, 
                            "Error returned by server : %s", json_object_get_string(poError));
                    json_object_put(poObj);
                    return NULL;
                }
            }
        }
        else
        {
            json_object_put(poObj);
            return NULL;
        }
    }

    return poObj;
}

/************************************************************************/
/*                        OGRCARTODBGetSingleRow()                      */
/************************************************************************/

json_object* OGRCARTODBGetSingleRow(json_object* poObj)
{
    if( poObj == NULL )
    {
        return NULL;
    }

    json_object* poRows = json_object_object_get(poObj, "rows");
    if( poRows == NULL ||
        json_object_get_type(poRows) != json_type_array ||
        json_object_array_length(poRows) != 1 )
    {
        json_object_put(poObj);
        return NULL;
    }

    json_object* poRowObj = json_object_array_get_idx(poRows, 0);
    if( poRowObj == NULL || json_object_get_type(poRowObj) != json_type_object )
    {
        json_object_put(poObj);
        return NULL;
    }

    return poRowObj;
}
