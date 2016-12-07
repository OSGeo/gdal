/******************************************************************************
 *
 * Project:  ElasticSearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

// What was this supposed to do?
// #pragma warning( disable : 4251 )

#include "ogr_elastic.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_http.h"
#include "ogrgeojsonreader.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRElasticDataSource()                        */
/************************************************************************/

OGRElasticDataSource::OGRElasticDataSource() :
    m_pszName(NULL),
    m_papoLayers(NULL),
    m_nLayers(0),
    m_bOverwrite(false),
    m_nBulkUpload(0),
    m_pszWriteMap(NULL),
    m_pszMapping(NULL),
    m_nBatchSize(100),
    m_nFeatureCountToEstablishFeatureDefn(100),
    m_bJSonField(false),
    m_bFlattenNestedAttributes(true)
{
    const char* pszWriteMapIn = CPLGetConfigOption("ES_WRITEMAP", NULL);
    if (pszWriteMapIn != NULL) {
        m_pszWriteMap = CPLStrdup(pszWriteMapIn);
    }
}

/************************************************************************/
/*                       ~OGRElasticDataSource()                        */
/************************************************************************/

OGRElasticDataSource::~OGRElasticDataSource() {
    for (int i = 0; i < m_nLayers; i++)
        delete m_papoLayers[i];
    CPLFree(m_papoLayers);
    CPLFree(m_pszName);
    CPLFree(m_pszMapping);
    CPLFree(m_pszWriteMap);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRElasticDataSource::TestCapability(const char * pszCap) {
    if (EQUAL(pszCap, ODsCCreateLayer) ||
        EQUAL(pszCap, ODsCDeleteLayer) ||
        EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRElasticDataSource::GetLayer(int iLayer) {
    if (iLayer < 0 || iLayer >= m_nLayers)
        return NULL;
    else
        return m_papoLayers[iLayer];
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRElasticDataSource::DeleteLayer( int iLayer )

{
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= m_nLayers )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osLayerName = m_papoLayers[iLayer]->GetName();
    CPLString osIndex = m_papoLayers[iLayer]->GetIndexName();
    CPLString osMapping = m_papoLayers[iLayer]->GetMappingName();

    CPLDebug( "ES", "DeleteLayer(%s)", osLayerName.c_str() );

    delete m_papoLayers[iLayer];
    memmove(m_papoLayers + iLayer, m_papoLayers + iLayer + 1,
            (m_nLayers - 1 - iLayer) * sizeof(OGRLayer*));
    m_nLayers --;

    Delete(CPLSPrintf("%s/%s/_mapping/%s",
                      GetURL(), osIndex.c_str(), osMapping.c_str()));

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer * OGRElasticDataSource::ICreateLayer(const char * pszLayerName,
                                              OGRSpatialReference *poSRS,
                                              OGRwkbGeometryType eGType,
                                              char ** papszOptions)
{
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return NULL;
    }

    CPLString osLaunderedName(pszLayerName);

    const char* pszIndexName = CSLFetchNameValue(papszOptions, "INDEX_NAME");
    if( pszIndexName != NULL )
        osLaunderedName = pszIndexName;

    for(size_t i=0;i<osLaunderedName.size();i++)
    {
        if( osLaunderedName[i] >= 'A' && osLaunderedName[i] <= 'Z' )
            osLaunderedName[i] += 'a' - 'A';
        else if( osLaunderedName[i] == '/' || osLaunderedName[i] == '?' )
            osLaunderedName[i] = '_';
    }
    if( strcmp(osLaunderedName.c_str(), pszLayerName) != 0 )
        CPLDebug("ES", "Laundered layer name to %s", osLaunderedName.c_str());

    // Backup error state
    CPLErr eLastErrorType = CPLGetLastErrorType();
    CPLErrorNum nLastErrorNo = CPLGetLastErrorNo();
    CPLString osLastErrorMsg = CPLGetLastErrorMsg();

    // Check if the index exists
    bool bIndexExists = false;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLHTTPResult* psResult = CPLHTTPFetch(CPLSPrintf("%s/%s",
                                           GetURL(), osLaunderedName.c_str()), NULL);
    CPLPopErrorHandler();
    if( psResult )
      {
        bIndexExists = psResult->pszErrBuf == NULL;
        CPLHTTPDestroyResult(psResult);
    }

    const char* m_pszMappingName = CSLFetchNameValueDef(papszOptions,
                                        "MAPPING_NAME", "FeatureCollection");

    bool bMappingExists = false;
    if( bIndexExists )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        psResult = CPLHTTPFetch(CPLSPrintf("%s/%s/_mapping/%s",
                        GetURL(), osLaunderedName.c_str(), m_pszMappingName), NULL);
        CPLPopErrorHandler();
        bMappingExists = psResult != NULL && psResult->pabyData != NULL &&
                         !STARTS_WITH_CI((const char*)psResult->pabyData, "{}");
        CPLHTTPDestroyResult(psResult);
    }

    // Restore error state
    CPLErrorSetState( eLastErrorType, nLastErrorNo, osLastErrorMsg );

    if( m_bOverwrite || CPLFetchBool(papszOptions, "OVERWRITE", false) )
    {
        if( bMappingExists )
        {
            Delete(CPLSPrintf("%s/%s/_mapping/%s",
                              GetURL(), osLaunderedName.c_str(), m_pszMappingName));
        }
    }
    else if( bMappingExists )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s/%s already exists",
                 osLaunderedName.c_str(), m_pszMappingName);
        return NULL;
    }

    // Create the index
    if( !bIndexExists )
    {
        if( !UploadFile(CPLSPrintf("%s/%s", GetURL(), osLaunderedName.c_str()), "") )
            return NULL;
    }

    // If we have a user specified mapping, then go ahead and update it now
    const char* pszLayerMapping = CSLFetchNameValueDef(papszOptions, "MAPPING", m_pszMapping);
    if (pszLayerMapping != NULL) {
        CPLString osLayerMapping;
        if( strchr(pszLayerMapping, '{') == NULL )
        {
            VSILFILE* fp = VSIFOpenL(pszLayerMapping, "rb");
            if( fp )
            {
                GByte* pabyRet = NULL;
                CPL_IGNORE_RET_VAL(VSIIngestFile( fp, pszLayerMapping, &pabyRet, NULL, -1));
                if( pabyRet )
                {
                    osLayerMapping = (char*)pabyRet;
                    pszLayerMapping = osLayerMapping.c_str();
                    VSIFree(pabyRet);
                }
                VSIFCloseL(fp);
            }
        }

        if( !UploadFile(CPLSPrintf("%s/%s/%s/_mapping",
                            GetURL(), osLaunderedName.c_str(), m_pszMappingName),
                        pszLayerMapping) )
        {
            return NULL;
        }
    }

    OGRElasticLayer* poLayer = new OGRElasticLayer(osLaunderedName.c_str(),
                                                   osLaunderedName.c_str(),
                                                   m_pszMappingName,
                                                   this, papszOptions);
    m_nLayers++;
    m_papoLayers = (OGRElasticLayer **) CPLRealloc(m_papoLayers, m_nLayers * sizeof (OGRElasticLayer*));
    m_papoLayers[m_nLayers - 1] = poLayer;

    poLayer->FinalizeFeatureDefn(false);

    if( eGType != wkbNone )
    {
        const char* pszGeometryName = CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "geometry");
        OGRGeomFieldDefn oFieldDefn(pszGeometryName, eGType);
        oFieldDefn.SetSpatialRef(poSRS);
        poLayer->CreateGeomField(&oFieldDefn, FALSE);
    }
    if( pszLayerMapping )
        poLayer->SetManualMapping();

    poLayer->SetIgnoreSourceID(
        CPLFetchBool(papszOptions, "IGNORE_SOURCE_ID", false));
    poLayer->SetDotAsNestedField(
        CPLFetchBool(papszOptions, "DOT_AS_NESTED_FIELD", true));
    poLayer->SetFID(CSLFetchNameValueDef(papszOptions, "FID", "ogc_fid"));
    poLayer->SetNextFID(0);

    return poLayer;
}

/************************************************************************/
/*                               RunRequest()                           */
/************************************************************************/

json_object* OGRElasticDataSource::RunRequest(const char* pszURL, const char* pszPostContent)
{
    char** papszOptions = NULL;

    if( pszPostContent && pszPostContent[0] )
    {
        papszOptions = CSLSetNameValue(papszOptions, "POSTFIELDS",
                                       pszPostContent);
    }

    CPLHTTPResult * psResult = CPLHTTPFetch( pszURL, papszOptions );
    CSLDestroy(papszOptions);

    if( psResult->pszErrBuf != NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                psResult->pabyData ? (const char*) psResult->pabyData :
                psResult->pszErrBuf);

        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if( psResult->pabyData == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if( STARTS_WITH((const char*) psResult->pabyData, "{\"error\":") )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                    (const char*) psResult->pabyData );
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    json_tokener* jstok = NULL;
    json_object* poObj = NULL;

    jstok = json_tokener_new();
    poObj = json_tokener_parse_ex(jstok, (const char*) psResult->pabyData, -1);
    if( jstok->err != json_tokener_success)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "JSON parsing error: %s (at offset %d)",
                    json_tokener_error_desc(jstok->err), jstok->char_offset);
        json_tokener_free(jstok);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    json_tokener_free(jstok);

    CPLHTTPDestroyResult(psResult);

    if( json_object_get_type(poObj) != json_type_object )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Return is not a JSON dictionary");
        json_object_put(poObj);
        poObj = NULL;
    }

    return poObj;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRElasticDataSource::Open(GDALOpenInfo* poOpenInfo)
{
    eAccess = poOpenInfo->eAccess;
    m_pszName = CPLStrdup(poOpenInfo->pszFilename);
    m_osURL = (STARTS_WITH_CI(m_pszName, "ES:")) ? m_pszName + 3 : m_pszName;
    if( m_osURL.empty() )
    {
        const char* pszHost =
            CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "HOST", "localhost");
        m_osURL = pszHost;
        m_osURL += ":";
        const char* pszPort = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "PORT", "9200");
        m_osURL += pszPort;
    }
    m_nBatchSize = atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "BATCH_SIZE", "100"));
    m_nFeatureCountToEstablishFeatureDefn = atoi(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN", "100"));
    m_bJSonField =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "JSON_FIELD", false);
    m_bFlattenNestedAttributes = CPLFetchBool(
            poOpenInfo->papszOpenOptions, "FLATTEN_NESTED_ATTRIBUTES", true);
    m_osFID = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "FID", "ogc_fid");

    CPLHTTPResult* psResult = CPLHTTPFetch((m_osURL + "/_cat/indices?h=i").c_str(), NULL);
    if( psResult == NULL || psResult->pszErrBuf != NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }

    // If no indices, fallback to querying _stats
    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);

        json_object* poRes = RunRequest((m_osURL + "/_stats").c_str());
        if( poRes == NULL )
            return FALSE;
        json_object_put(poRes);
        return TRUE;
    }

    char* pszCur = (char*)psResult->pabyData;
    char* pszNextEOL = strchr(pszCur, '\n');
    while( pszNextEOL && pszNextEOL > pszCur )
    {
        *pszNextEOL = '\0';

        char* pszBeforeEOL = pszNextEOL - 1;
        while( *pszBeforeEOL == ' ' )
        {
            *pszBeforeEOL = '\0';
            pszBeforeEOL  --;
        }

        const char* pszIndexName = pszCur;

        json_object* poRes = RunRequest((m_osURL + CPLString("/") + pszIndexName + CPLString("?pretty")).c_str());
        if( poRes )
        {
            json_object* poLayerObj = CPL_json_object_object_get(poRes, pszIndexName);
            json_object* poMappings = NULL;
            if( poLayerObj && json_object_get_type(poLayerObj) == json_type_object )
                poMappings = CPL_json_object_object_get(poLayerObj, "mappings");
            if( poMappings && json_object_get_type(poMappings) == json_type_object )
            {
                json_object_iter it;
                it.key = NULL;
                it.val = NULL;
                it.entry = NULL;
                std::vector<CPLString> aosMappings;
                json_object_object_foreachC( poMappings, it )
                {
                    aosMappings.push_back(it.key);
                }
                if( aosMappings.size() == 1 &&
                    (aosMappings[0] == "FeatureCollection" || aosMappings[0] == "default") )
                {
                    OGRElasticLayer* poLayer = new OGRElasticLayer(
                        pszCur, pszCur, aosMappings[0], this, poOpenInfo->papszOpenOptions);
                    poLayer->InitFeatureDefnFromMapping(CPL_json_object_object_get(poMappings, aosMappings[0]),
                                                        "", std::vector<CPLString>());

                    m_nLayers++;
                    m_papoLayers = (OGRElasticLayer **) CPLRealloc(m_papoLayers, m_nLayers * sizeof (OGRElasticLayer*));
                    m_papoLayers[m_nLayers - 1] = poLayer;
                }
                else
                {
                    for(size_t i=0; i<aosMappings.size();i++)
                    {
                        OGRElasticLayer* poLayer = new OGRElasticLayer(
                            (pszCur + CPLString("_") + aosMappings[i]).c_str(), pszCur, aosMappings[i], this, poOpenInfo->papszOpenOptions);
                        poLayer->InitFeatureDefnFromMapping(CPL_json_object_object_get(poMappings, aosMappings[i]),
                                                            "", std::vector<CPLString>());

                        m_nLayers++;
                        m_papoLayers = (OGRElasticLayer **) CPLRealloc(m_papoLayers, m_nLayers * sizeof (OGRElasticLayer*));
                        m_papoLayers[m_nLayers - 1] = poLayer;
                    }
                }
            }

            json_object_put(poRes);
        }

        pszCur = pszNextEOL + 1;
        pszNextEOL = strchr(pszCur, '\n');
    }

    CPLHTTPDestroyResult(psResult);
    return TRUE;
}

/************************************************************************/
/*                             Delete()                                 */
/************************************************************************/

void OGRElasticDataSource::Delete(const CPLString &url) {
    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "CUSTOMREQUEST", "DELETE");
    CPLHTTPResult* psResult = CPLHTTPFetch(url, papszOptions);
    CSLDestroy(papszOptions);
    if (psResult) {
        CPLHTTPDestroyResult(psResult);
    }
}

/************************************************************************/
/*                            UploadFile()                              */
/************************************************************************/

bool OGRElasticDataSource::UploadFile( const CPLString &url,
                                       const CPLString &data )
{
    bool bRet = true;
    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", data.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
            "Content-Type: application/x-javascript; charset=UTF-8");

    CPLHTTPResult* psResult = CPLHTTPFetch(url, papszOptions);
    CSLDestroy(papszOptions);
    if( psResult )
    {
        if( psResult->pszErrBuf != NULL ||
            (psResult->pabyData && STARTS_WITH((const char*) psResult->pabyData, "{\"error\":")) ||
            (psResult->pabyData && strstr((const char*) psResult->pabyData, "\"errors\":true,") != NULL) )
        {
            bRet = false;
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     psResult->pabyData ? (const char*) psResult->pabyData :
                     psResult->pszErrBuf);
        }
        CPLHTTPDestroyResult(psResult);
    }
    return bRet;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRElasticDataSource::Create(const char *pszFilename,
                                 CPL_UNUSED char **papszOptions)
{
    eAccess = GA_Update;
    m_pszName = CPLStrdup(pszFilename);
    m_osURL = (STARTS_WITH_CI(pszFilename, "ES:")) ? pszFilename + 3 : pszFilename;
    if( m_osURL.empty() )
        m_osURL = "localhost:9200";

    const char* pszMetaFile = CPLGetConfigOption("ES_META", NULL);
    m_bOverwrite = CPLTestBool(CPLGetConfigOption("ES_OVERWRITE", "0"));
    m_nBulkUpload = (int) CPLAtof(CPLGetConfigOption("ES_BULK", "0"));

    // Read in the meta file from disk
    if (pszMetaFile != NULL)
    {
        VSILFILE* fp = VSIFOpenL(pszMetaFile, "rb");
        if( fp )
        {
            GByte* pabyRet = NULL;
            CPL_IGNORE_RET_VAL(VSIIngestFile( fp, pszMetaFile, &pabyRet, NULL, -1));
            if( pabyRet )
            {
                m_pszMapping = (char*)pabyRet;
            }
            VSIFCloseL(fp);
        }
    }

    // Do a status check to ensure that the server is valid
    CPLHTTPResult* psResult = CPLHTTPFetch(CPLSPrintf("%s/_stats", m_osURL.c_str()), NULL);
    int bOK = (psResult != NULL && psResult->pszErrBuf == NULL);
    if (!bOK)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                "Could not connect to server");
    }

    CPLHTTPDestroyResult(psResult);

    return bOK;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer* OGRElasticDataSource::ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect )
{
    for(int i=0; i<m_nLayers; i++ )
    {
        m_papoLayers[i]->SyncToDisk();
    }

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DELLAYER:") )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
        {
            if( EQUAL(m_papoLayers[iLayer]->GetName(),
                      pszLayerName ))
            {
                DeleteLayer( iLayer );
                break;
            }
        }
        return NULL;
    }

    if( pszDialect != NULL && EQUAL(pszDialect, "ES") )
    {
        return new OGRElasticLayer("RESULT",
                                   NULL,
                                   NULL,
                                   this, papszOpenOptions,
                                   pszSQLCommand);
    }
    else
    {
        return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
    }
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRElasticDataSource::ReleaseResultSet( OGRLayer * poLayer )
{
    delete poLayer;
}
