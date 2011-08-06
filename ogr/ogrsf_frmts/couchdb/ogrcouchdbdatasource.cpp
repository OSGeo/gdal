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
#include "swq.h"

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
    CPLString osEscapedName;
    if (pszLayerName)
    {
        osTableName = pszLayerName;
        char* pszEscapedName = CPLEscapeString(pszLayerName, -1, CPLES_URL);
        osEscapedName = pszEscapedName;
        CPLFree(pszEscapedName);
    }
    else
    {
        char* pszURL = CPLStrdup(osURL);
        char* pszLastSlash = strrchr(pszURL, '/');
        if (pszLastSlash)
        {
            osEscapedName = pszLastSlash + 1;
            char* pszName = CPLUnescapeString(osEscapedName, NULL, CPLES_URL);
            osTableName = pszName;
            CPLFree(pszName);
            *pszLastSlash = 0;
        }
        osURL = pszURL;
        CPLFree(pszURL);
        pszURL = NULL;

        if (pszLastSlash == NULL)
            return NULL;
    }

    CPLString osURI("/");
    osURI += osEscapedName;

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
/*                               OpenView()                             */
/************************************************************************/

OGRLayer* OGRCouchDBDataSource::OpenView()
{
    OGRCouchDBRowsLayer* poLayer = new OGRCouchDBRowsLayer(this);
    if (!poLayer->BuildFeatureDefn())
    {
        delete poLayer;
        return NULL;
    }

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


    if ((strstr(osURL, "/_design/") && strstr(osURL, "/_view/")) ||
        strstr(osURL, "/_all_docs"))
    {
        return OpenView() != NULL;
    }

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

                CPLErrorReset();

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

    char* pszEscapedName = CPLEscapeString(pszName, -1, CPLES_URL);
    CPLString osEscapedName = pszEscapedName;
    CPLFree(pszEscapedName);

/* -------------------------------------------------------------------- */
/*      Create "database"                                               */
/* -------------------------------------------------------------------- */
    CPLString osURI;
    osURI = "/";
    osURI += osEscapedName;
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
        osURI += osEscapedName;
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
        osURI += osEscapedName;
        osURI += "/_design/ogr_validation";

        poAnswerObj = PUT(osURI, osValidation);

        if (IsOK(poAnswerObj, "Validation function creation failed"))
            nUpdateSeq ++;

        json_object_put(poAnswerObj);
    }

    int bGeoJSONDocument = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "GEOJSON", "TRUE"));
    int nCoordPrecision = atoi(CSLFetchNameValueDef(papszOptions, "COORDINATE_PRECISION", "-1"));

    OGRCouchDBTableLayer* poLayer = new OGRCouchDBTableLayer(this, pszName);
    if (nCoordPrecision != -1)
        poLayer->SetCoordinatePrecision(nCoordPrecision);
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

    char* pszEscapedName = CPLEscapeString(osLayerName, -1, CPLES_URL);
    CPLString osEscapedName = pszEscapedName;
    CPLFree(pszEscapedName);

    CPLString osURI;
    osURI = "/";
    osURI += osEscapedName;
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

/* -------------------------------------------------------------------- */
/*      Special case 'COMPACT ON ' command.                             */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"COMPACT ON ",11) )
    {
        const char *pszLayerName = pszSQLCommand + 11;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        CPLString osURI("/");
        osURI += pszLayerName;
        osURI += "/_compact";

        json_object* poAnswerObj = POST(osURI, NULL);
        IsError(poAnswerObj, "Database compaction failed");
        json_object_put(poAnswerObj);

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special case 'VIEW CLEANUP ON ' command.                        */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"VIEW CLEANUP ON ",16) )
    {
        const char *pszLayerName = pszSQLCommand + 16;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        CPLString osURI("/");
        osURI += pszLayerName;
        osURI += "/_view_cleanup";

        json_object* poAnswerObj = POST(osURI, NULL);
        IsError(poAnswerObj, "View cleanup failed");
        json_object_put(poAnswerObj);

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Deal with "DELETE FROM layer_name WHERE expression" statement   */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand, "DELETE FROM ", 12) )
    {
        const char* pszIter = pszSQLCommand + 12;
        while(*pszIter && *pszIter != ' ')
            pszIter ++;
        if (*pszIter == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid statement");
            return NULL;
        }

        CPLString osName = pszSQLCommand + 12;
        osName.resize(pszIter - (pszSQLCommand + 12));
        OGRCouchDBLayer* poLayer = (OGRCouchDBLayer*)GetLayerByName(osName);
        if (poLayer == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown layer : %s", osName.c_str());
            return NULL;
        }
        if (poLayer->GetLayerType() != COUCHDB_TABLE_LAYER)
            return NULL;
        OGRCouchDBTableLayer* poTableLayer = (OGRCouchDBTableLayer*)poLayer;

        while(*pszIter && *pszIter == ' ')
            pszIter ++;
        if (!EQUALN(pszIter, "WHERE ", 5))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "WHERE clause missing");
            return NULL;
        }
        pszIter += 5;

        const char* pszQuery = pszIter;

        /* Check with the generic SQL engine that this is a valid WHERE clause */
        OGRFeatureQuery oQuery;
        OGRErr eErr = oQuery.Compile( poLayer->GetLayerDefn(), pszQuery );
        if( eErr != OGRERR_NONE )
        {
            return NULL;
        }

        swq_expr_node * pNode = (swq_expr_node *) oQuery.GetSWGExpr();
        if (pNode->eNodeType == SNT_OPERATION &&
            pNode->nOperation == SWQ_EQ &&
            pNode->nSubExprCount == 2 &&
            pNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
            pNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
            pNode->papoSubExpr[0]->field_index == _ID_FIELD &&
            pNode->papoSubExpr[1]->field_type == SWQ_STRING)
        {
            poTableLayer->DeleteFeature(pNode->papoSubExpr[1]->string_value);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid WHERE clause. Expecting '_id' = 'a_value'");
            return NULL;
        }


        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try an optimized implementation when doing only stats           */
/* -------------------------------------------------------------------- */
    if (poSpatialFilter == NULL && EQUALN(pszSQLCommand, "SELECT", 6))
    {
        OGRLayer* poRet = ExecuteSQLStats(pszSQLCommand);
        if (poRet)
            return poRet;
    }

    return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                        poSpatialFilter,
                                        pszDialect );
}

/************************************************************************/
/*                         ExecuteSQLStats()                            */
/************************************************************************/

class PointerAutoFree
{
        void ** m_pp;
    public:
        PointerAutoFree(void** pp) { m_pp = pp; }
        ~PointerAutoFree() { CPLFree(*m_pp); *m_pp = NULL; }
};

class OGRCouchDBOneLineLayer : public OGRLayer
{
    public:
        OGRFeature* poFeature;
        OGRFeatureDefn* poFeatureDefn;
        int bEnd;

        OGRCouchDBOneLineLayer() { poFeature = NULL; poFeatureDefn = NULL; bEnd = FALSE; }
        ~OGRCouchDBOneLineLayer()
        {
            delete poFeature;
            if( poFeatureDefn != NULL )
                poFeatureDefn->Release();
        }

        virtual void        ResetReading() { bEnd = FALSE;}
        virtual OGRFeature *GetNextFeature()
        {
            if (bEnd) return NULL;
            bEnd = TRUE;
            return poFeature->Clone();
        }
        virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }
        virtual int         TestCapability( const char * ) { return FALSE; }
};

OGRLayer * OGRCouchDBDataSource::ExecuteSQLStats( const char *pszSQLCommand )
{
    swq_select sSelectInfo;
    if( sSelectInfo.preparse( pszSQLCommand ) != CPLE_None )
    {
        return NULL;
    }

    if (sSelectInfo.table_count != 1)
    {
        return NULL;
    }

    swq_table_def *psTableDef = &sSelectInfo.table_defs[0];
    if( psTableDef->data_source != NULL )
    {
        return NULL;
    }

    OGRCouchDBLayer* _poSrcLayer =
        (OGRCouchDBLayer* )GetLayerByName( psTableDef->table_name );
    if (_poSrcLayer == NULL)
    {
        return NULL;
    }
    if (_poSrcLayer->GetLayerType() != COUCHDB_TABLE_LAYER)
        return NULL;

    OGRCouchDBTableLayer* poSrcLayer = (OGRCouchDBTableLayer* ) _poSrcLayer;

    int nFieldCount = poSrcLayer->GetLayerDefn()->GetFieldCount();

    swq_field_list sFieldList;
    memset( &sFieldList, 0, sizeof(sFieldList) );
    sFieldList.table_count = sSelectInfo.table_count;
    sFieldList.table_defs = sSelectInfo.table_defs;

    sFieldList.count = 0;
    sFieldList.names = (char **) CPLMalloc( sizeof(char *) * nFieldCount );
    sFieldList.types = (swq_field_type *)
        CPLMalloc( sizeof(swq_field_type) * nFieldCount );
    sFieldList.table_ids = (int *)
        CPLMalloc( sizeof(int) * nFieldCount );
    sFieldList.ids = (int *)
        CPLMalloc( sizeof(int) * nFieldCount );

    PointerAutoFree oHolderNames((void**)&(sFieldList.names));
    PointerAutoFree oHolderTypes((void**)&(sFieldList.types));
    PointerAutoFree oHolderTableIds((void**)&(sFieldList.table_ids));
    PointerAutoFree oHolderIds((void**)&(sFieldList.ids));

    int iField;
    for( iField = 0;
         iField < poSrcLayer->GetLayerDefn()->GetFieldCount();
         iField++ )
    {
        OGRFieldDefn *poFDefn=poSrcLayer->GetLayerDefn()->GetFieldDefn(iField);
        int iOutField = sFieldList.count++;
        sFieldList.names[iOutField] = (char *) poFDefn->GetNameRef();
        if( poFDefn->GetType() == OFTInteger )
            sFieldList.types[iOutField] = SWQ_INTEGER;
        else if( poFDefn->GetType() == OFTReal )
            sFieldList.types[iOutField] = SWQ_FLOAT;
        else if( poFDefn->GetType() == OFTString )
            sFieldList.types[iOutField] = SWQ_STRING;
        else
            sFieldList.types[iOutField] = SWQ_OTHER;

        sFieldList.table_ids[iOutField] = 0;
        sFieldList.ids[iOutField] = iField;
    }

    CPLString osLastFieldName;
    for( iField = 0; iField < sSelectInfo.result_columns; iField++ )
    {
        swq_col_def *psColDef = sSelectInfo.column_defs + iField;
        if (psColDef->field_name == NULL)
            return NULL;

        if (strcmp(psColDef->field_name, "*") != 0)
        {
            if (osLastFieldName.size() == 0)
                osLastFieldName = psColDef->field_name;
            else if (strcmp(osLastFieldName, psColDef->field_name) != 0)
                return NULL;

            if (poSrcLayer->GetLayerDefn()->GetFieldIndex(psColDef->field_name) == -1)
                return NULL;
        }

        if (!(psColDef->col_func == SWQCF_AVG ||
              psColDef->col_func == SWQCF_MIN ||
              psColDef->col_func == SWQCF_MAX ||
              psColDef->col_func == SWQCF_COUNT ||
              psColDef->col_func == SWQCF_SUM))
            return NULL;

        if (psColDef->distinct_flag) /* TODO: could perhaps be relaxed */
            return NULL;
    }

    if (osLastFieldName.size() == 0)
        return NULL;

    /* Normalize field name */
    int nIndex = poSrcLayer->GetLayerDefn()->GetFieldIndex(osLastFieldName);
    osLastFieldName = poSrcLayer->GetLayerDefn()->GetFieldDefn(nIndex)->GetNameRef();

/* -------------------------------------------------------------------- */
/*      Finish the parse operation.                                     */
/* -------------------------------------------------------------------- */

    if( sSelectInfo.parse( &sFieldList, 0 ) != CE_None )
    {
        return NULL;
    }

    if (sSelectInfo.join_defs != NULL ||
        sSelectInfo.where_expr != NULL ||
        sSelectInfo.order_defs != NULL ||
        sSelectInfo.query_mode != SWQM_SUMMARY_RECORD)
    {
        return NULL;
    }

    for( iField = 0; iField < sSelectInfo.result_columns; iField++ )
    {
        swq_col_def *psColDef = sSelectInfo.column_defs + iField;
        if (psColDef->field_index == -1)
        {
            if (psColDef->col_func == SWQCF_COUNT)
                continue;

            return NULL;
        }
        if (psColDef->field_type != SWQ_INTEGER &&
            psColDef->field_type != SWQ_FLOAT)
        {
            return NULL;
        }
    }

    int bFoundFilter = poSrcLayer->HasFilterOnFieldOrCreateIfNecessary(osLastFieldName);
    if (!bFoundFilter)
        return NULL;

    CPLString osURI = "/";
    osURI += poSrcLayer->GetName();
    osURI += "/_design/ogr_filter_";
    osURI += osLastFieldName;
    osURI += "/_view/filter?reduce=true";

    json_object* poAnswerObj = GET(osURI);
    json_object* poRows = NULL;
    if (!(poAnswerObj != NULL &&
          json_object_is_type(poAnswerObj, json_type_object) &&
          (poRows = json_object_object_get(poAnswerObj, "rows")) != NULL &&
          json_object_is_type(poRows, json_type_array)))
    {
        json_object_put(poAnswerObj);
        return NULL;
    }

    int nLength = json_object_array_length(poRows);
    if (nLength != 1)
    {
        json_object_put(poAnswerObj);
        return NULL;
    }

    json_object* poRow = json_object_array_get_idx(poRows, 0);
    if (!(poRow && json_object_is_type(poRow, json_type_object)))
    {
        json_object_put(poAnswerObj);
        return NULL;
    }

    json_object* poValue = json_object_object_get(poRow, "value");
    if (!(poValue != NULL && json_object_is_type(poValue, json_type_object)))
    {
        json_object_put(poAnswerObj);
        return NULL;
    }

    json_object* poSum = json_object_object_get(poValue, "sum");
    json_object* poCount = json_object_object_get(poValue, "count");
    json_object* poMin = json_object_object_get(poValue, "min");
    json_object* poMax = json_object_object_get(poValue, "max");
    if (poSum != NULL && (json_object_is_type(poSum, json_type_int) ||
                            json_object_is_type(poSum, json_type_double)) &&
        poCount != NULL && (json_object_is_type(poCount, json_type_int) ||
                            json_object_is_type(poCount, json_type_double)) &&
        poMin != NULL && (json_object_is_type(poMin, json_type_int) ||
                            json_object_is_type(poMin, json_type_double)) &&
        poMax != NULL && (json_object_is_type(poMax, json_type_int) ||
                            json_object_is_type(poMax, json_type_double)) )
    {
        double dfSum = json_object_get_double(poSum);
        int nCount = json_object_get_int(poCount);
        double dfMin = json_object_get_double(poMin);
        double dfMax = json_object_get_double(poMax);
        json_object_put(poAnswerObj);

        //CPLDebug("CouchDB", "sum=%f, count=%d, min=%f, max=%f",
        //         dfSum, nCount, dfMin, dfMax);

        OGRFeatureDefn* poFeatureDefn = new OGRFeatureDefn(poSrcLayer->GetName());
        poFeatureDefn->Reference();

        for( iField = 0; iField < sSelectInfo.result_columns; iField++ )
        {
            swq_col_def *psColDef = sSelectInfo.column_defs + iField;
            OGRFieldDefn oFDefn( "", OFTInteger );

            if( psColDef->field_alias != NULL )
            {
                oFDefn.SetName(psColDef->field_alias);
            }
            else
            {
                const swq_operation *op = swq_op_registrar::GetOperator(
                    (swq_op) psColDef->col_func );
                oFDefn.SetName( CPLSPrintf( "%s_%s",
                                            op->osName.c_str(),
                                            psColDef->field_name ) );
            }

            if( psColDef->col_func == SWQCF_COUNT )
                oFDefn.SetType( OFTInteger );
            else if (psColDef->field_type == SWQ_INTEGER)
                oFDefn.SetType( OFTInteger );
            else if (psColDef->field_type == SWQ_FLOAT)
                oFDefn.SetType( OFTReal );

            poFeatureDefn->AddFieldDefn(&oFDefn);
        }

        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

        for( iField = 0; iField < sSelectInfo.result_columns; iField++ )
        {
            swq_col_def *psColDef = sSelectInfo.column_defs + iField;
            switch(psColDef->col_func)
            {
                case SWQCF_AVG:
                    if (nCount)
                        poFeature->SetField(iField, dfSum / nCount);
                    break;
                case SWQCF_MIN:
                    poFeature->SetField(iField, dfMin);
                    break;
                case SWQCF_MAX:
                    poFeature->SetField(iField, dfMax);
                    break;
                case SWQCF_COUNT:
                    poFeature->SetField(iField, nCount);
                    break;
                case SWQCF_SUM:
                    poFeature->SetField(iField, dfSum);
                    break;
                default:
                    break;
            }
        }

        poFeature->SetFID(0);

        OGRCouchDBOneLineLayer* poAnswerLayer = new OGRCouchDBOneLineLayer();
        poAnswerLayer->poFeatureDefn = poFeatureDefn;
        poAnswerLayer->poFeature = poFeature;
        return poAnswerLayer;
    }
    json_object_put(poAnswerObj);

    return NULL;
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
