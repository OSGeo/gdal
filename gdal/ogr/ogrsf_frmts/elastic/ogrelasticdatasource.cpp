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

#include "ogr_elastic.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_http.h"
#include "ogrgeojsonreader.h"
#include "swq.h"

CPL_CVSID("$Id$")

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
    m_bFlattenNestedAttributes(true),
    m_nMajorVersion(0)
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

int OGRElasticDataSource::TestCapability(const char * pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer) ||
        EQUAL(pszCap, ODsCDeleteLayer) ||
        EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer) )
    {
        return GetAccess() == GA_Update;
    }

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

    bool bSeveralMappings = false;
    json_object* poIndexResponse = RunRequest(CPLSPrintf("%s/%s",
                                       GetURL(), osIndex.c_str()), NULL);
    if( poIndexResponse )
    {
        json_object* poIndex = CPL_json_object_object_get(poIndexResponse,
                                                          osMapping);
        if( poIndex != NULL )
        {
            json_object* poMappings = CPL_json_object_object_get(poIndex,
                                                                 "mappings");
            if( poMappings != NULL )
            {
                bSeveralMappings = json_object_object_length(poMappings) > 1;
            }
        }
        json_object_put(poIndexResponse);
    }
    // Deletion of one mapping in an index was supported in ES 1.X, but
    // considered unsafe and removed in later versions
    if( bSeveralMappings )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "%s/%s already exists, but other mappings also exist in "
                "this index. "
                "You have to delete the whole index.",
                osIndex.c_str(), osMapping.c_str());
        return OGRERR_FAILURE;
    }

    CPLDebug( "ES", "DeleteLayer(%s)", osLayerName.c_str() );

    delete m_papoLayers[iLayer];
    memmove(m_papoLayers + iLayer, m_papoLayers + iLayer + 1,
            (m_nLayers - 1 - iLayer) * sizeof(OGRLayer*));
    m_nLayers --;

    Delete(CPLSPrintf("%s/%s",  GetURL(), osIndex.c_str()));

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

    const char* pszMappingName = CSLFetchNameValueDef(papszOptions,
                                        "MAPPING_NAME", "FeatureCollection");

    // Check if the index and mapping exists
    bool bIndexExists = false;
    bool bMappingExists = false;
    bool bSeveralMappings = false;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    json_object* poIndexResponse = RunRequest(CPLSPrintf("%s/%s",
                                       GetURL(), osLaunderedName.c_str()), NULL);
    CPLPopErrorHandler();

    // Restore error state
    CPLErrorSetState( eLastErrorType, nLastErrorNo, osLastErrorMsg );

    if( poIndexResponse )
    {
        bIndexExists = true;
        json_object* poIndex = CPL_json_object_object_get(poIndexResponse,
                                                          osLaunderedName);
        if( poIndex != NULL )
        {
            json_object* poMappings = CPL_json_object_object_get(poIndex,
                                                                 "mappings");
            if( poMappings != NULL )
            {
                bMappingExists = CPL_json_object_object_get(
                                    poMappings, pszMappingName) != NULL;
                bSeveralMappings = json_object_object_length(poMappings) > 1;
            }
        }
        json_object_put(poIndexResponse);
    }

    if( bMappingExists )
    {
        if( CPLFetchBool(papszOptions, "OVERWRITE_INDEX", false)  )
        {
            Delete(CPLSPrintf("%s/%s", GetURL(), osLaunderedName.c_str()));
        }
        else if( m_bOverwrite || CPLFetchBool(papszOptions, "OVERWRITE", false) )
        {
            // Deletion of one mapping in an index was supported in ES 1.X, but
            // considered unsafe and removed in later versions
            if( bSeveralMappings )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "%s/%s already exists, but other mappings also exist "
                        "in this index. "
                        "You have to delete the whole index. You can do that "
                        "with OVERWRITE_INDEX=YES",
                        osLaunderedName.c_str(), pszMappingName);
                return NULL;
            }
            Delete(CPLSPrintf("%s/%s", GetURL(), osLaunderedName.c_str()));
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s/%s already exists",
                    osLaunderedName.c_str(), pszMappingName);
            return NULL;
        }
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
                            GetURL(), osLaunderedName.c_str(), pszMappingName),
                        pszLayerMapping) )
        {
            return NULL;
        }
    }

    OGRElasticLayer* poLayer = new OGRElasticLayer(osLaunderedName.c_str(),
                                                   osLaunderedName.c_str(),
                                                   pszMappingName,
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

    json_object* poObj = NULL;
    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
    if( !OGRJSonParse(pszText, &poObj, true) )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

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
/*                           CheckVersion()                             */
/************************************************************************/

bool OGRElasticDataSource::CheckVersion()
{
    json_object* poMainInfo = RunRequest(m_osURL);
    if( poMainInfo == NULL )
        return false;
    bool bVersionFound = false;
    json_object* poVersion = CPL_json_object_object_get(poMainInfo, "version");
    if( poVersion != NULL )
    {
        json_object* poNumber = CPL_json_object_object_get(poVersion, "number");
        if( poNumber != NULL &&
            json_object_get_type(poNumber) == json_type_string )
        {
            bVersionFound = true;
            const char* pszVersion = json_object_get_string(poNumber);
            CPLDebug("ES", "Server version: %s", pszVersion);
            m_nMajorVersion = atoi(pszVersion);
        }
    }
    json_object_put(poMainInfo);
    if( !bVersionFound )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Server version not found");
        return false;
    }
    if( m_nMajorVersion != 1 && m_nMajorVersion != 2 && m_nMajorVersion != 5 )
    {
        CPLDebug("ES", "Server version untested with current driver");
    }
    return true;
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

    if( !CheckVersion() )
        return FALSE;

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

        json_object* poRes = RunRequest((m_osURL + CPLString("/") + pszIndexName + CPLString("/_mapping?pretty")).c_str());
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
    if( data.empty() )
        papszOptions = CSLAddNameValue(papszOptions, "CUSTOMREQUEST", "PUT");
    else
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

    return CheckVersion();
}

/************************************************************************/
/*                           GetLayerIndex()                            */
/************************************************************************/

int OGRElasticDataSource::GetLayerIndex( const char* pszName )
{
    for( int i=0; i < m_nLayers; ++i )
    {
        if( strcmp( m_papoLayers[i]->GetName(), pszName ) == 0 )
            return i;
    }
    for( int i=0; i < m_nLayers; ++i )
    {
        if( EQUAL( m_papoLayers[i]->GetName(), pszName ) )
            return i;
    }
    return -1;
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


/* -------------------------------------------------------------------- */
/*      Deal with "SELECT xxxx ORDER BY" statement                      */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "SELECT"))
    {
        swq_select* psSelectInfo = new swq_select();
        if( psSelectInfo->preparse( pszSQLCommand, TRUE ) != CE_None )
        {
            delete psSelectInfo;
            return NULL;
        }

        int iLayer = 0;
        if( psSelectInfo->table_count == 1 &&
            psSelectInfo->table_defs[0].data_source == NULL &&
            (iLayer =
                GetLayerIndex( psSelectInfo->table_defs[0].table_name )) >= 0 &&
            psSelectInfo->join_count == 0 &&
            psSelectInfo->order_specs > 0 &&
            psSelectInfo->poOtherSelect == NULL )
        {
            OGRElasticLayer* poSrcLayer = m_papoLayers[iLayer];
            std::vector<OGRESSortDesc> aoSortColumns;
            int i = 0;  // Used after for.
            for( ; i < psSelectInfo->order_specs; i++ )
            {
                int nFieldIndex = poSrcLayer->GetLayerDefn()->GetFieldIndex(
                                        psSelectInfo->order_defs[i].field_name);
                if (nFieldIndex < 0)
                    break;

                /* Make sure to have the right case */
                const char* pszFieldName = poSrcLayer->GetLayerDefn()->
                    GetFieldDefn(nFieldIndex)->GetNameRef();

                OGRESSortDesc oSortDesc(pszFieldName,
                    CPL_TO_BOOL(psSelectInfo->order_defs[i].ascending_flag));
                aoSortColumns.push_back(oSortDesc);
            }

            if( i == psSelectInfo->order_specs )
            {
                OGRElasticLayer* poDupLayer = poSrcLayer->Clone();

                poDupLayer->SetOrderBy(aoSortColumns);
                int nBackup = psSelectInfo->order_specs;
                psSelectInfo->order_specs = 0;
                char* pszSQLWithoutOrderBy = psSelectInfo->Unparse();
                CPLDebug("ES", "SQL without ORDER BY: %s", pszSQLWithoutOrderBy);
                psSelectInfo->order_specs = nBackup;
                delete psSelectInfo;
                psSelectInfo = NULL;

                /* Just set poDupLayer in the papoLayers for the time of the */
                /* base ExecuteSQL(), so that the OGRGenSQLResultsLayer */
                /* references  that temporary layer */
                m_papoLayers[iLayer] = poDupLayer;

                OGRLayer* poResLayer = GDALDataset::ExecuteSQL(
                    pszSQLWithoutOrderBy, poSpatialFilter, pszDialect );
                m_papoLayers[iLayer] = poSrcLayer;

                CPLFree(pszSQLWithoutOrderBy);

                if (poResLayer != NULL)
                    m_oMapResultSet[poResLayer] = poDupLayer;
                else
                    delete poDupLayer;
                return poResLayer;
            }
        }
        delete psSelectInfo;
    }

    return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRElasticDataSource::ReleaseResultSet( OGRLayer * poResultsSet )
{
    if (poResultsSet == NULL)
        return;

    std::map<OGRLayer*, OGRLayer*>::iterator oIter =
                                        m_oMapResultSet.find(poResultsSet);
    if (oIter != m_oMapResultSet.end())
    {
        /* Destroy first the result layer, because it still references */
        /* the poDupLayer (oIter->second) */
        delete poResultsSet;

        delete oIter->second;
        m_oMapResultSet.erase(oIter);
    }
    else
    {
        delete poResultsSet;
    }
}
