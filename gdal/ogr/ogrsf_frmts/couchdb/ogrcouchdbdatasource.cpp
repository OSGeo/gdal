/******************************************************************************
 * $Id$
 *
 * Project:  CouchDB Translator
 * Purpose:  Implements OGRCouchDBDataSource class
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

#include "ogr_couchdb.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRCouchDBDataSource()                        */
/************************************************************************/

OGRCouchDBDataSource::OGRCouchDBDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;

    bReadWrite = FALSE;

    bMustCleanPersistant = FALSE;
}

/************************************************************************/
/*                       ~OGRCouchDBDataSource()                        */
/************************************************************************/

OGRCouchDBDataSource::~OGRCouchDBDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (bMustCleanPersistant)
    {
        char** papszOptions = CSLAddString(NULL,
                          CPLSPrintf("CLOSE_PERSISTENT=CouchDB:%p", this));
        CPLHTTPFetch( osURL, papszOptions);
        CSLDestroy(papszOptions);
    }

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCouchDBDataSource::TestCapability( const char * pszCap )

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

OGRLayer *OGRCouchDBDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRCouchDBDataSource::GetLayerByName(const char * pszLayerName)
{
    OGRLayer* poLayer = OGRDataSource::GetLayerByName(pszLayerName);
    if (poLayer)
        return poLayer;

    return OpenDatabase(pszLayerName);
}

/************************************************************************/
/*                             OpenDatabase()                           */
/************************************************************************/

OGRLayer* OGRCouchDBDataSource::OpenDatabase(const char* pszLayerName)
{
    CPLString osTableName;
    if (pszLayerName)
    {
        osTableName = pszLayerName;
    }
    else
    {
        char* pszURL = CPLStrdup(osURL);
        char* pszLastSlash = strrchr(pszURL, '/');
        if (pszLastSlash)
        {
            osTableName = pszLastSlash + 1;
            *pszLastSlash = 0;
        }
        osURL = pszURL;
        CPLFree(pszURL);
        pszURL = NULL;

        if (pszLastSlash == NULL)
            return NULL;
    }

    CPLString osURI("/");
    osURI += osTableName;

    json_object* poAnswerObj = GET(osURI);
    if (poAnswerObj == NULL)
        return NULL;

    if ( !json_object_is_type(poAnswerObj, json_type_object) ||
            json_object_object_get(poAnswerObj, "db_name") == NULL )
    {
        IsError(poAnswerObj, "Database opening failed");

        json_object_put(poAnswerObj);
        return NULL;
    }

    OGRCouchDBTableLayer* poLayer = new OGRCouchDBTableLayer(this, osTableName);

    if ( json_object_object_get(poAnswerObj, "update_seq") != NULL )
    {
        int nUpdateSeq = json_object_get_int(json_object_object_get(poAnswerObj, "update_seq"));
        poLayer->SetUpdateSeq(nUpdateSeq);
    }

    json_object_put(poAnswerObj);

    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers ++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCouchDBDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    int bHTTP = FALSE;
    if (strncmp(pszFilename, "http://", 7) == 0 ||
        strncmp(pszFilename, "https://", 8) == 0)
        bHTTP = TRUE;
    else if (!EQUALN(pszFilename, "CouchDB:", 8))
        return FALSE;

    bReadWrite = bUpdateIn;

    pszName = CPLStrdup( pszFilename );

    if (bHTTP)
        osURL = pszFilename;
    else
        osURL = pszFilename + 8;
    if (osURL.size() > 0 && osURL[osURL.size() - 1] == '/')
        osURL.resize(osURL.size() - 1);

    const char* pszUserPwd = CPLGetConfigOption("COUCHDB_USERPWD", NULL);
    if (pszUserPwd)
        osUserPwd = pszUserPwd;

    /* If passed with http://useraccount.knownprovider.com/database, do not */
    /* try to issue /all_dbs, but directly open the database */
    const char* pszKnowProvider = strstr(osURL, ".iriscouch.com/");
    if (pszKnowProvider != NULL &&
        strchr(pszKnowProvider + strlen(".iriscouch.com/"), '/' ) == NULL)
    {
        return OpenDatabase() != NULL;
    }
    pszKnowProvider = strstr(osURL, ".cloudant.com/");
    if (pszKnowProvider != NULL &&
        strchr(pszKnowProvider + strlen(".cloudant.com/"), '/' ) == NULL)
    {
        return OpenDatabase() != NULL;
    }

    /* Get list of tables */
    json_object* poAnswerObj = GET("/_all_dbs");

    if (poAnswerObj == NULL)
        return FALSE;

    if ( !json_object_is_type(poAnswerObj, json_type_array) )
    {
        if ( json_object_is_type(poAnswerObj, json_type_object) )
        {
            json_object* poError = json_object_object_get(poAnswerObj, "error");
            json_object* poReason = json_object_object_get(poAnswerObj, "reason");

            const char* pszError = json_object_get_string(poError);
            const char* pszReason = json_object_get_string(poReason);

            if (pszError && pszReason && strcmp(pszError, "not_found") == 0 &&
                strcmp(pszReason, "missing") == 0)
            {
                json_object_put(poAnswerObj);
                poAnswerObj = NULL;

                return OpenDatabase() != NULL;
            }
        }
        if (poAnswerObj)
        {
            IsError(poAnswerObj, "Database listing failed");

            json_object_put(poAnswerObj);
            return FALSE;
        }
    }

    int nTables = json_object_array_length(poAnswerObj);
    for(int i=0;i<nTables;i++)
    {
        json_object* poAnswerObjDBName = json_object_array_get_idx(poAnswerObj, i);
        if ( json_object_is_type(poAnswerObjDBName, json_type_string) )
        {
            const char* pszDBName = json_object_get_string(poAnswerObjDBName);
            if ( strcmp(pszDBName, "_users") != 0 &&
                 strcmp(pszDBName, "_replicator") != 0 )
            {
                papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
                papoLayers[nLayers ++] = new OGRCouchDBTableLayer(this, pszDBName);
            }
        }
    }

    json_object_put(poAnswerObj);

    return TRUE;
}


/************************************************************************/
/*                           CreateLayer()                              */
/************************************************************************/

OGRLayer   *OGRCouchDBDataSource::CreateLayer( const char *pszName,
                                           OGRSpatialReference *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           char ** papszOptions )
{
    if (!bReadWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
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

/* -------------------------------------------------------------------- */
/*      Create "database"                                               */
/* -------------------------------------------------------------------- */
    CPLString osURI;
    osURI = "/";
    osURI += pszName;
    json_object* poAnswerObj = PUT(osURI, NULL);

    if (poAnswerObj == NULL)
        return FALSE;

    if (!IsOK(poAnswerObj, "Layer creation failed"))
    {
        json_object_put(poAnswerObj);
        return FALSE;
    }

    json_object_put(poAnswerObj);

/* -------------------------------------------------------------------- */
/*      Create "spatial index"                                          */
/* -------------------------------------------------------------------- */
    int nUpdateSeq = 0;
    if (eGType != wkbNone)
    {
        osURI = "/";
        osURI += pszName;
        osURI += "/_design/ogr_spatial";

        CPLString osContent("{ \"spatial\": { \"spatial\" : \"function(doc) { if (doc.geometry && doc.geometry.coordinates && doc.geometry.coordinates.length != 0) { emit(doc.geometry, null); } } \" } }");

        poAnswerObj = PUT(osURI, osContent);

        if (IsOK(poAnswerObj, "Spatial index creation failed"))
            nUpdateSeq ++;

        json_object_put(poAnswerObj);
    }


/* -------------------------------------------------------------------- */
/*      Create validation function                                      */
/* -------------------------------------------------------------------- */
    const char* pszUpdatePermissions = CSLFetchNameValueDef(papszOptions, "UPDATE_PERMISSIONS", "LOGGED_USER");
    CPLString osValidation;
    if (EQUAL(pszUpdatePermissions, "LOGGED_USER"))
    {
        osValidation = "{\"validate_doc_update\": \"function(new_doc, old_doc, userCtx) { if(!userCtx.name) { throw({forbidden: \\\"Please log in first.\\\"}); } }\" }";
    }
    else if (EQUAL(pszUpdatePermissions, "ALL"))
    {
        osValidation = "{\"validate_doc_update\": \"function(new_doc, old_doc, userCtx) {  }\" }";
    }
    else if (EQUAL(pszUpdatePermissions, "ADMIN"))
    {
        osValidation = "{\"validate_doc_update\": \"function(new_doc, old_doc, userCtx) {if (userCtx.roles.indexOf('_admin') === -1) { throw({forbidden: \\\"No changes allowed except by admin.\\\"}); } }\" }";
    }
    else if (strncmp(pszUpdatePermissions, "function(", 9) == 0)
    {
        osValidation = "{\"validate_doc_update\": \"";
        osValidation += pszUpdatePermissions;
        osValidation += "\"}";
    }

    if (osValidation.size())
    {
        osURI = "/";
        osURI += pszName;
        osURI += "/_design/ogr_validation";

        poAnswerObj = PUT(osURI, osValidation);

        if (IsOK(poAnswerObj, "Validation function creation failed"))
            nUpdateSeq ++;

        json_object_put(poAnswerObj);
    }

    int bGeoJSONDocument = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "GEOJSON", "TRUE"));

    OGRCouchDBTableLayer* poLayer = new OGRCouchDBTableLayer(this, pszName);
    poLayer->SetInfoAfterCreation(eGType, poSpatialRef, nUpdateSeq, bGeoJSONDocument);
    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers ++] = poLayer;
    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRCouchDBDataSource::DeleteLayer( const char *pszLayerName )

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

OGRErr OGRCouchDBDataSource::DeleteLayer(int iLayer)
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

    CPLString osLayerName = GetLayer(iLayer)->GetName();

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLDebug( "CouchDB", "DeleteLayer(%s)", osLayerName.c_str() );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */

    CPLString osURI;
    osURI = "/";
    osURI += osLayerName;
    json_object* poAnswerObj = DELETE(osURI);

    if (poAnswerObj == NULL)
        return OGRERR_FAILURE;

    if (!IsOK(poAnswerObj, "Layer deletion failed"))
    {
        json_object_put(poAnswerObj);
        return OGRERR_FAILURE;
    }

    json_object_put(poAnswerObj);

    return OGRERR_NONE;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRCouchDBDataSource::ExecuteSQL( const char *pszSQLCommand,
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

    return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                        poSpatialFilter,
                                        pszDialect );
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRCouchDBDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                             REQUEST()                                */
/************************************************************************/

json_object* OGRCouchDBDataSource::REQUEST(const char* pszVerb,
                                           const char* pszURI,
                                           const char* pszData)
{
    bMustCleanPersistant = TRUE;

    char** papszOptions = NULL;
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=CouchDB:%p", this));

    CPLString osCustomRequest("CUSTOMREQUEST=");
    osCustomRequest += pszVerb;
    papszOptions = CSLAddString(papszOptions, osCustomRequest);

    CPLString osPOSTFIELDS("POSTFIELDS=");
    if (pszData)
        osPOSTFIELDS += pszData;
    papszOptions = CSLAddString(papszOptions, osPOSTFIELDS);

    papszOptions = CSLAddString(papszOptions, "HEADERS=Content-Type: application/json");

    if (osUserPwd.size())
    {
        CPLString osUserPwdOption("USERPWD=");
        osUserPwdOption += osUserPwd;
        papszOptions = CSLAddString(papszOptions, osUserPwdOption);
    }

    CPLDebug("CouchDB", "%s %s", pszVerb, pszURI);
    CPLString osFullURL(osURL);
    osFullURL += pszURI;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLHTTPResult * psResult = CPLHTTPFetch( osFullURL, papszOptions);
    CPLPopErrorHandler();
    CSLDestroy(papszOptions);
    if (psResult == NULL)
        return NULL;

    const char* pszServer = CSLFetchNameValue(psResult->papszHeaders, "Server");
    if (pszServer == NULL || !EQUALN(pszServer, "CouchDB", 7))
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if (psResult->nDataLen == 0)
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    json_tokener* jstok = NULL;
    json_object* jsobj = NULL;

    jstok = json_tokener_new();
    jsobj = json_tokener_parse_ex(jstok, (const char*)psResult->pabyData, -1);
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
    return jsobj;
}

/************************************************************************/
/*                               GET()                                  */
/************************************************************************/

json_object* OGRCouchDBDataSource::GET(const char* pszURI)
{
    return REQUEST("GET", pszURI, NULL);
}

/************************************************************************/
/*                               PUT()                                  */
/************************************************************************/

json_object* OGRCouchDBDataSource::PUT(const char* pszURI, const char* pszData)
{
    return REQUEST("PUT", pszURI, pszData);
}

/************************************************************************/
/*                               POST()                                 */
/************************************************************************/

json_object* OGRCouchDBDataSource::POST(const char* pszURI, const char* pszData)
{
    return REQUEST("POST", pszURI, pszData);
}

/************************************************************************/
/*                             DELETE()                                 */
/************************************************************************/

json_object* OGRCouchDBDataSource::DELETE(const char* pszURI)
{
    return REQUEST("DELETE", pszURI, NULL);
}

/************************************************************************/
/*                            IsError()                                 */
/************************************************************************/

int OGRCouchDBDataSource::IsError(json_object* poAnswerObj,
                                  const char* pszErrorMsg)
{
    if ( poAnswerObj == NULL ||
        !json_object_is_type(poAnswerObj, json_type_object) )
    {
        return FALSE;
    }

    json_object* poError = json_object_object_get(poAnswerObj, "error");
    json_object* poReason = json_object_object_get(poAnswerObj, "reason");

    const char* pszError = json_object_get_string(poError);
    const char* pszReason = json_object_get_string(poReason);
    if (pszError != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s : %s, %s",
                 pszErrorMsg,
                 pszError ? pszError : "",
                 pszReason ? pszReason : "");

        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                              IsOK()                                  */
/************************************************************************/

int OGRCouchDBDataSource::IsOK(json_object* poAnswerObj,
                               const char* pszErrorMsg)
{
    if ( poAnswerObj == NULL ||
        !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 pszErrorMsg);

        return FALSE;
    }

    json_object* poOK = json_object_object_get(poAnswerObj, "ok");
    if ( !poOK )
    {
        IsError(poAnswerObj, pszErrorMsg);

        return FALSE;
    }

    const char* pszOK = json_object_get_string(poOK);
    if ( !pszOK || !CSLTestBoolean(pszOK) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", pszErrorMsg);

        return FALSE;
    }

    return TRUE;
}