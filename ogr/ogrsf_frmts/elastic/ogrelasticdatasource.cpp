/******************************************************************************
 *
 * Project:  Elasticsearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_swq.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRElasticDataSource()                        */
/************************************************************************/

OGRElasticDataSource::OGRElasticDataSource() :
    m_pszName(nullptr),
    m_bOverwrite(false),
    m_nBulkUpload(0),
    m_pszWriteMap(nullptr),
    m_pszMapping(nullptr),
    m_nBatchSize(100),
    m_nFeatureCountToEstablishFeatureDefn(100),
    m_bJSonField(false),
    m_bFlattenNestedAttributes(true)
{
    const char* pszWriteMapIn = CPLGetConfigOption("ES_WRITEMAP", nullptr);
    if (pszWriteMapIn != nullptr) {
        m_pszWriteMap = CPLStrdup(pszWriteMapIn);
    }
}

/************************************************************************/
/*                       ~OGRElasticDataSource()                        */
/************************************************************************/

OGRElasticDataSource::~OGRElasticDataSource() {
    m_apoLayers.clear();
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
/*                             GetLayerCount()                          */
/************************************************************************/

int OGRElasticDataSource::GetLayerCount()
{
    if( m_bAllLayersListed )
    {
        return static_cast<int>(m_apoLayers.size());
    }
    m_bAllLayersListed = true;

    CPLHTTPResult* psResult = HTTPFetch((m_osURL + "/_cat/indices?h=i").c_str(), nullptr);
    if( psResult == nullptr || psResult->pszErrBuf != nullptr ||
        psResult->pabyData == nullptr )
    {
        CPLHTTPDestroyResult(psResult);
        return 0;
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

        pszCur = pszNextEOL + 1;
        pszNextEOL = strchr(pszCur, '\n');

        if( STARTS_WITH(pszIndexName, ".security") ||
            STARTS_WITH(pszIndexName, ".monitoring") ||
            STARTS_WITH(pszIndexName, ".geoip_databases") )
        {
            continue;
        }

        FetchMapping(pszIndexName);
    }

    CPLHTTPDestroyResult(psResult);
    return static_cast<int>(m_apoLayers.size());
}

/************************************************************************/
/*                            FetchMapping()                            */
/************************************************************************/

void OGRElasticDataSource::FetchMapping(const char* pszIndexName)
{
    if( m_oSetLayers.find(pszIndexName) != m_oSetLayers.end() )
        return;

    CPLString osURL(m_osURL + CPLString("/") + pszIndexName +
                    CPLString("/_mapping?pretty"));
    json_object* poRes = RunRequest(osURL, nullptr, std::vector<int>({403}));
    if( poRes )
    {
        json_object* poLayerObj = CPL_json_object_object_get(poRes, pszIndexName);
        json_object* poMappings = nullptr;
        if( poLayerObj && json_object_get_type(poLayerObj) == json_type_object )
            poMappings = CPL_json_object_object_get(poLayerObj, "mappings");
        if( poMappings && json_object_get_type(poMappings) == json_type_object )
        {
            std::vector<CPLString> aosMappings;
            if (m_nMajorVersion < 7)
            {
                json_object_iter it;
                it.key = nullptr;
                it.val = nullptr;
                it.entry = nullptr;
                json_object_object_foreachC( poMappings, it )
                {
                    aosMappings.push_back(it.key);
                }

                if( aosMappings.size() == 1 &&
                    (aosMappings[0] == "FeatureCollection" || aosMappings[0] == "default") )
                {
                    m_oSetLayers.insert(pszIndexName);
                    OGRElasticLayer* poLayer = new OGRElasticLayer(
                        pszIndexName, pszIndexName, aosMappings[0], this, papszOpenOptions);
                    poLayer->InitFeatureDefnFromMapping(
                        CPL_json_object_object_get(poMappings, aosMappings[0]),
                        "", std::vector<CPLString>());
                    m_apoLayers.push_back(std::unique_ptr<OGRElasticLayer>(poLayer));
                }
                else
                {
                    for(size_t i=0; i<aosMappings.size();i++)
                    {
                        CPLString osLayerName(pszIndexName + CPLString("_") + aosMappings[i]);
                        if( m_oSetLayers.find(osLayerName) == m_oSetLayers.end() )
                        {
                            m_oSetLayers.insert(osLayerName);
                            OGRElasticLayer* poLayer = new OGRElasticLayer(
                                osLayerName,
                                pszIndexName, aosMappings[i], this, papszOpenOptions);
                            poLayer->InitFeatureDefnFromMapping(
                                CPL_json_object_object_get(poMappings, aosMappings[i]),
                                "", std::vector<CPLString>());

                            m_apoLayers.push_back(std::unique_ptr<OGRElasticLayer>(poLayer));
                        }
                    }
                }
            }
            else
            {
                m_oSetLayers.insert(pszIndexName);
                OGRElasticLayer* poLayer = new OGRElasticLayer(
                   pszIndexName, pszIndexName, "", this, papszOpenOptions);
                poLayer->InitFeatureDefnFromMapping(poMappings, "", std::vector<CPLString>());
                m_apoLayers.push_back(std::unique_ptr<OGRElasticLayer>(poLayer));
            }
        }


        json_object_put(poRes);
    }
}

/************************************************************************/
/*                            GetLayerByName()                          */
/************************************************************************/

OGRLayer* OGRElasticDataSource::GetLayerByName(const char* pszName)
{
    if( !m_bAllLayersListed )
    {
        for( auto& poLayer: m_apoLayers )
        {
            if( EQUAL( poLayer->GetName(), pszName) )
            {
                return poLayer.get();
            }
        }
        size_t nSizeBefore = m_apoLayers.size();
        FetchMapping(pszName);
        const char* pszLastUnderscore = strrchr(pszName, '_');
        if( pszLastUnderscore && m_apoLayers.size() == nSizeBefore )
        {
            CPLString osIndexName(pszName);
            osIndexName.resize( pszLastUnderscore - pszName);
            FetchMapping(osIndexName);
        }
        for( auto& poLayer: m_apoLayers )
        {
            if( EQUAL( poLayer->GetIndexName(), pszName) )
            {
                return poLayer.get();
            }
        }
        return nullptr;
    }
    else
    {
        return GDALDataset::GetLayerByName(pszName);
    }
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRElasticDataSource::GetLayer(int iLayer) {
    const int nLayers = GetLayerCount();
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;
    else
        return m_apoLayers[iLayer].get();
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

    if( iLayer < 0 || iLayer >= GetLayerCount() )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osLayerName = m_apoLayers[iLayer]->GetName();
    CPLString osIndex = m_apoLayers[iLayer]->GetIndexName();
    CPLString osMapping = m_apoLayers[iLayer]->GetMappingName();

    bool bSeveralMappings = false;
    json_object* poIndexResponse = RunRequest(CPLSPrintf("%s/%s",
                                       GetURL(), osIndex.c_str()), nullptr);
    if( poIndexResponse )
    {
        json_object* poIndex = CPL_json_object_object_get(poIndexResponse,
                                                          osMapping);
        if( poIndex != nullptr )
        {
            json_object* poMappings = CPL_json_object_object_get(poIndex,
                                                                 "mappings");
            if( poMappings != nullptr )
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

    m_oSetLayers.erase(osLayerName);
    m_apoLayers.erase(m_apoLayers.begin() + iLayer);

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
        return nullptr;
    }

    CPLString osLaunderedName(pszLayerName);

    const char* pszIndexName = CSLFetchNameValue(papszOptions, "INDEX_NAME");
    if( pszIndexName != nullptr )
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

    const char* pszMappingName = m_nMajorVersion < 7
        ? CSLFetchNameValueDef(papszOptions, "MAPPING_NAME", "FeatureCollection")
        : nullptr;

    // Check if the index and mapping exists
    bool bIndexExists = false;
    bool bMappingExists = false;
    bool bSeveralMappings = false;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    json_object* poIndexResponse = RunRequest(CPLSPrintf("%s/%s",
                                       GetURL(), osLaunderedName.c_str()), nullptr);
    CPLPopErrorHandler();

    // Restore error state
    CPLErrorSetState( eLastErrorType, nLastErrorNo, osLastErrorMsg );

    if( poIndexResponse )
    {
        bIndexExists = true;
        json_object* poIndex = CPL_json_object_object_get(poIndexResponse,
                                                          osLaunderedName);
        if (m_nMajorVersion < 7)
        {
            if( poIndex != nullptr )
            {
                json_object* poMappings = CPL_json_object_object_get(poIndex,
                                                                     "mappings");
                if( poMappings != nullptr )
                {
                    bMappingExists = CPL_json_object_object_get(
                                        poMappings, pszMappingName) != nullptr;
                    bSeveralMappings = json_object_object_length(poMappings) > 1;
                }
            }
        }
        else
        {
            // Indexes in Elasticsearch 7+ can not have multiple types,
            // so essentially this will always be true.
            bMappingExists = true;
        }
        json_object_put(poIndexResponse);
    }

    if( bMappingExists )
    {
        if( CPLFetchBool(papszOptions, "OVERWRITE_INDEX", false)  )
        {
            Delete(CPLSPrintf("%s/%s", GetURL(), osLaunderedName.c_str()));
            bIndexExists = false;
        }
        else if( m_bOverwrite || CPLFetchBool(papszOptions, "OVERWRITE", false) )
        {
            // Deletion of one mapping in an index was supported in ES 1.X, but
            // considered unsafe and removed in later versions
            if (m_nMajorVersion >= 7)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "The index %s already exists. "
                         "You have to delete the whole index. You can do that "
                         "with OVERWRITE_INDEX=YES",
                         osLaunderedName.c_str());
                return nullptr;
            }
            else if( bSeveralMappings )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "%s/%s already exists, but other mappings also exist "
                        "in this index. "
                        "You have to delete the whole index. You can do that "
                        "with OVERWRITE_INDEX=YES",
                        osLaunderedName.c_str(), pszMappingName);
                return nullptr;
            }
            Delete(CPLSPrintf("%s/%s", GetURL(), osLaunderedName.c_str()));
            bIndexExists = false;
        }
        else
        {
            if (m_nMajorVersion < 7)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s/%s already exists",
                         osLaunderedName.c_str(), pszMappingName);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s already exists",
                         osLaunderedName.c_str());
            }
            return nullptr;
        }
    }

    // Create the index
    if( !bIndexExists )
    {
        CPLString osIndexURL(CPLSPrintf("%s/%s", GetURL(), osLaunderedName.c_str()));

        // If we have a user specified index definition, use it
        const char* pszDef = CSLFetchNameValue(papszOptions, "INDEX_DEFINITION");
        CPLString osDef;
        if (pszDef != nullptr)
        {
            osDef = pszDef;
            if( strchr(pszDef, '{') == nullptr )
            {
                VSILFILE* fp = VSIFOpenL(pszDef, "rb");
                if( fp )
                {
                    GByte* pabyRet = nullptr;
                    CPL_IGNORE_RET_VAL(VSIIngestFile( fp, pszDef, &pabyRet, nullptr, -1));
                    if( pabyRet )
                    {
                        osDef = reinterpret_cast<char*>(pabyRet);
                        VSIFree(pabyRet);
                    }
                    VSIFCloseL(fp);
                }
            }
        }
        if( !UploadFile(osIndexURL, osDef.c_str(), "PUT") )
            return nullptr;
    }

    // If we have a user specified mapping, then go ahead and update it now
    const char* pszLayerMapping = CSLFetchNameValueDef(papszOptions, "MAPPING", m_pszMapping);
    if (pszLayerMapping != nullptr) {
        CPLString osLayerMapping(pszLayerMapping);
        if( strchr(pszLayerMapping, '{') == nullptr )
        {
            VSILFILE* fp = VSIFOpenL(pszLayerMapping, "rb");
            if( fp )
            {
                GByte* pabyRet = nullptr;
                CPL_IGNORE_RET_VAL(VSIIngestFile( fp, pszLayerMapping, &pabyRet, nullptr, -1));
                if( pabyRet )
                {
                    osLayerMapping = reinterpret_cast<char*>(pabyRet);
                    VSIFree(pabyRet);
                }
                VSIFCloseL(fp);
            }
        }

        CPLString osMappingURL = CPLSPrintf("%s/%s/_mapping",
                            GetURL(), osLaunderedName.c_str());
        if (m_nMajorVersion < 7)
            osMappingURL += CPLSPrintf("/%s", pszMappingName);
        if( !UploadFile(osMappingURL, osLayerMapping.c_str()) )
        {
            return nullptr;
        }
    }

    OGRElasticLayer* poLayer = new OGRElasticLayer(osLaunderedName.c_str(),
                                                   osLaunderedName.c_str(),
                                                   pszMappingName,
                                                   this, papszOptions);
    poLayer->FinalizeFeatureDefn(false);

    if( eGType != wkbNone )
    {
        const char* pszGeometryName = CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "geometry");
        OGRGeomFieldDefn oFieldDefn(pszGeometryName, eGType);
        if( poSRS )
        {
            OGRSpatialReference* poSRSClone = poSRS->Clone();
            poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            oFieldDefn.SetSpatialRef(poSRSClone);
            poSRSClone->Release();
        }
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

    m_oSetLayers.insert(poLayer->GetName());
    m_apoLayers.push_back(std::unique_ptr<OGRElasticLayer>(poLayer));

    return poLayer;
}

/************************************************************************/
/*                               HTTPFetch()                            */
/************************************************************************/

CPLHTTPResult* OGRElasticDataSource::HTTPFetch(const char* pszURL,
                                               CSLConstList papszOptions)
{
    CPLStringList aosOptions(papszOptions);
    if( !m_osUserPwd.empty() )
        aosOptions.SetNameValue("USERPWD", m_osUserPwd.c_str());
    if( !m_oMapHeadersFromEnv.empty() )
    {
        const char* pszExistingHeaders = aosOptions.FetchNameValue("HEADERS");
        std::string osHeaders;
        if( pszExistingHeaders )
        {
            osHeaders += pszExistingHeaders;
            osHeaders += '\n';
        }
        for( const auto& kv: m_oMapHeadersFromEnv )
        {
            const char* pszValueFromEnv =
                CPLGetConfigOption(kv.second.c_str(), nullptr);
            if( pszValueFromEnv )
            {
                osHeaders += kv.first;
                osHeaders += ": ";
                osHeaders += pszValueFromEnv;
                osHeaders += '\n';
            }
        }
        aosOptions.SetNameValue("HEADERS", osHeaders.c_str());
    }
    return CPLHTTPFetch(pszURL, aosOptions);
}

/************************************************************************/
/*                               RunRequest()                           */
/************************************************************************/

json_object* OGRElasticDataSource::RunRequest(const char* pszURL,
                                              const char* pszPostContent,
                                              const std::vector<int>& anSilentedHTTPErrors)
{
    char** papszOptions = nullptr;

    if( pszPostContent && pszPostContent[0] )
    {
        papszOptions = CSLSetNameValue(papszOptions, "POSTFIELDS",
                                       pszPostContent);
        papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                "Content-Type: application/json; charset=UTF-8");
    }

    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLHTTPResult * psResult = HTTPFetch( pszURL, papszOptions );
    CPLPopErrorHandler();
    CSLDestroy(papszOptions);

    if( psResult->pszErrBuf != nullptr )
    {
        CPLString osErrorMsg(
                psResult->pabyData ? (const char*) psResult->pabyData :
                psResult->pszErrBuf);
        bool bSilence = false;
        for( auto nCode: anSilentedHTTPErrors )
        {
            if( strstr(psResult->pszErrBuf, CPLSPrintf("%d", nCode)) )
            {
                bSilence = true;
                break;
            }
        }
        if( bSilence )
        {
            CPLDebug("ES", "%s", osErrorMsg.c_str());
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMsg.c_str());
        }
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    if( STARTS_WITH((const char*) psResult->pabyData, "{\"error\":") )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                    (const char*) psResult->pabyData );
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    json_object* poObj = nullptr;
    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
    if( !OGRJSonParse(pszText, &poObj, true) )
    {
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    CPLHTTPDestroyResult(psResult);

    if( json_object_get_type(poObj) != json_type_object )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Return is not a JSON dictionary");
        json_object_put(poObj);
        poObj = nullptr;
    }

    return poObj;
}

/************************************************************************/
/*                           CheckVersion()                             */
/************************************************************************/

bool OGRElasticDataSource::CheckVersion()
{
    json_object* poMainInfo = RunRequest(m_osURL);
    if( poMainInfo == nullptr )
        return false;
    bool bVersionFound = false;
    json_object* poVersion = CPL_json_object_object_get(poMainInfo, "version");
    if( poVersion != nullptr )
    {
        json_object* poNumber = CPL_json_object_object_get(poVersion, "number");
        if( poNumber != nullptr &&
            json_object_get_type(poNumber) == json_type_string )
        {
            bVersionFound = true;
            const char* pszVersion = json_object_get_string(poNumber);
            CPLDebug("ES", "Server version: %s", pszVersion);
            m_nMajorVersion = atoi(pszVersion);
            const char* pszDot = strchr(pszVersion, '.');
            if( pszDot )
                m_nMinorVersion = atoi(pszDot+1);
        }
    }
    json_object_put(poMainInfo);
    if( !bVersionFound )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Server version not found");
        return false;
    }
    if( m_nMajorVersion < 1 || m_nMajorVersion > 7 )
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
    m_osUserPwd = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "USERPWD", "");
    m_nBatchSize = atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "BATCH_SIZE", "100"));
    m_nFeatureCountToEstablishFeatureDefn = atoi(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN", "100"));
    m_bJSonField =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "JSON_FIELD", false);
    m_bFlattenNestedAttributes = CPLFetchBool(
            poOpenInfo->papszOpenOptions, "FLATTEN_NESTED_ATTRIBUTES", true);
    m_osFID = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "FID", "ogc_fid");

    const char* pszHeadersFromEnv = CPLGetConfigOption("ES_FORWARD_HTTP_HEADERS_FROM_ENV",
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "FORWARD_HTTP_HEADERS_FROM_ENV"));
    if( pszHeadersFromEnv )
    {
        CPLStringList aosTokens(CSLTokenizeString2(pszHeadersFromEnv, ",", 0));
        for( int i = 0; i < aosTokens.size(); ++i )
        {
            char* pszKey = nullptr;
            const char* pszValue = CPLParseNameValue(aosTokens[i], &pszKey);
            if( pszKey && pszValue )
            {
                m_oMapHeadersFromEnv[pszKey] = pszValue;
            }
            CPLFree(pszKey);
        }
    }

    if( !CheckVersion() )
        return FALSE;

    const char* pszLayerName = CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                                 "LAYER");
    if( pszLayerName )
    {
        bool bFound = GetLayerByName(pszLayerName) != nullptr;
        m_bAllLayersListed = true;
        return bFound;
    }

    return TRUE;
}

/************************************************************************/
/*                             Delete()                                 */
/************************************************************************/

void OGRElasticDataSource::Delete(const CPLString &url) {
    char** papszOptions = nullptr;
    papszOptions = CSLAddNameValue(papszOptions, "CUSTOMREQUEST", "DELETE");
    CPLHTTPResult* psResult = HTTPFetch(url, papszOptions);
    CSLDestroy(papszOptions);
    if (psResult) {
        CPLHTTPDestroyResult(psResult);
    }
}

/************************************************************************/
/*                            UploadFile()                              */
/************************************************************************/

bool OGRElasticDataSource::UploadFile( const CPLString &url,
                                       const CPLString &data,
                                       const CPLString &osVerb )
{
    bool bRet = true;
    char** papszOptions = nullptr;
    if( !osVerb.empty() )
    {
        papszOptions = CSLAddNameValue(papszOptions, "CUSTOMREQUEST",
                                       osVerb.c_str());
    }
    if( data.empty() )
    {
        if( osVerb.empty() )
        {
            papszOptions = CSLAddNameValue(papszOptions, "CUSTOMREQUEST", "PUT");
        }
    }
    else
    {
        papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", data.c_str());
        papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                "Content-Type: application/json; charset=UTF-8");
    }

    CPLHTTPResult* psResult = HTTPFetch(url, papszOptions);
    CSLDestroy(papszOptions);
    if( psResult )
    {
        if( psResult->pszErrBuf != nullptr ||
            (psResult->pabyData && STARTS_WITH((const char*) psResult->pabyData, "{\"error\":")) ||
            (psResult->pabyData && strstr((const char*) psResult->pabyData, "\"errors\":true,") != nullptr) )
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

    const char* pszMetaFile = CPLGetConfigOption("ES_META", nullptr);
    m_bOverwrite = CPLTestBool(CPLGetConfigOption("ES_OVERWRITE", "0"));
    // coverity[tainted_data]
    m_nBulkUpload = (int) CPLAtof(CPLGetConfigOption("ES_BULK", "0"));

    // Read in the meta file from disk
    if (pszMetaFile != nullptr)
    {
        VSILFILE* fp = VSIFOpenL(pszMetaFile, "rb");
        if( fp )
        {
            GByte* pabyRet = nullptr;
            CPL_IGNORE_RET_VAL(VSIIngestFile( fp, pszMetaFile, &pabyRet, nullptr, -1));
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
    const int nLayerCount = GetLayerCount();
    for( int i=0; i < nLayerCount; ++i )
    {
        if( strcmp( m_apoLayers[i]->GetName(), pszName ) == 0 )
            return i;
    }
    for( int i=0; i < nLayerCount; ++i )
    {
        if( EQUAL( m_apoLayers[i]->GetName(), pszName ) )
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
    const int nLayerCount = GetLayerCount();
    for(int i=0; i<nLayerCount; i++ )
    {
        m_apoLayers[i]->SyncToDisk();
    }

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DELLAYER:") )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        for( int iLayer = 0; iLayer < nLayerCount; iLayer++ )
        {
            if( EQUAL(m_apoLayers[iLayer]->GetName(),
                      pszLayerName ))
            {
                DeleteLayer( iLayer );
                break;
            }
        }
        return nullptr;
    }

    if( pszDialect != nullptr && EQUAL(pszDialect, "ES") )
    {
        return new OGRElasticLayer("RESULT",
                                   nullptr,
                                   nullptr,
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
            return nullptr;
        }

        int iLayer = 0;
        if( psSelectInfo->table_count == 1 &&
            psSelectInfo->table_defs[0].data_source == nullptr &&
            (iLayer =
                GetLayerIndex( psSelectInfo->table_defs[0].table_name )) >= 0 &&
            psSelectInfo->join_count == 0 &&
            psSelectInfo->order_specs > 0 &&
            psSelectInfo->poOtherSelect == nullptr )
        {
            OGRElasticLayer* poSrcLayer = m_apoLayers[iLayer].get();
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
                psSelectInfo = nullptr;

                /* Just set poDupLayer in the papoLayers for the time of the */
                /* base ExecuteSQL(), so that the OGRGenSQLResultsLayer */
                /* references  that temporary layer */
                m_apoLayers[iLayer].release();
                m_apoLayers[iLayer].reset(poDupLayer);

                OGRLayer* poResLayer = GDALDataset::ExecuteSQL(
                    pszSQLWithoutOrderBy, poSpatialFilter, pszDialect );
                m_apoLayers[iLayer].release();
                m_apoLayers[iLayer].reset(poSrcLayer);

                CPLFree(pszSQLWithoutOrderBy);

                if (poResLayer != nullptr)
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
    if (poResultsSet == nullptr)
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
