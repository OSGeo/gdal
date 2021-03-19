/******************************************************************************
 *
 * Project:  AmigoCloud Translator
 * Purpose:  Implements OGRAmigoCloudDataSource class
 * Author:   Victor Chernetsky, <victor at amigocloud dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Victor Chernetsky, <victor at amigocloud dot com>
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

#include "ogr_amigocloud.h"
#include "ogr_pgdump.h"
#include "ogrgeojsonreader.h"
#include <sstream>

CPL_CVSID("$Id$")

CPLString OGRAMIGOCLOUDGetOptionValue(const char* pszFilename, const char* pszOptionName);

/************************************************************************/
/*                        OGRAmigoCloudDataSource()                        */
/************************************************************************/

OGRAmigoCloudDataSource::OGRAmigoCloudDataSource() :
    pszName(nullptr),
    pszProjectId(nullptr),
    papoLayers(nullptr),
    nLayers(0),
    bReadWrite(false),
    bUseHTTPS(true),
    bMustCleanPersistent(false),
    bHasOGRMetadataFunction(-1)
{}

/************************************************************************/
/*                       ~OGRAmigoCloudDataSource()                        */
/************************************************************************/

OGRAmigoCloudDataSource::~OGRAmigoCloudDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if( bMustCleanPersistent )
    {
        char** papszOptions = nullptr;
        papszOptions = CSLSetNameValue(papszOptions, "CLOSE_PERSISTENT", CPLSPrintf("AMIGOCLOUD:%p", this));
        papszOptions = CSLAddString(papszOptions, GetUserAgentOption().c_str());
        CPLHTTPDestroyResult( CPLHTTPFetch( GetAPIURL(), papszOptions) );
        CSLDestroy(papszOptions);
    }

    CPLFree( pszName );
    CPLFree(pszProjectId);
}

std::string  OGRAmigoCloudDataSource::GetUserAgentOption()
{
    std::stringstream userAgent;
    userAgent << "USERAGENT=gdal/AmigoCloud build:" << GDALVersionInfo("RELEASE_NAME");
    return userAgent.str();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAmigoCloudDataSource::TestCapability( const char * pszCap )

{
    if( bReadWrite && EQUAL(pszCap, ODsCCreateLayer) && nLayers == 0 )
        return TRUE;
    else if( bReadWrite && EQUAL(pszCap, ODsCDeleteLayer) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return bReadWrite;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRAmigoCloudDataSource::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRAmigoCloudDataSource::GetLayerByName(const char * pszLayerName)
{
    if (nLayers > 1) {
        return OGRDataSource::GetLayerByName(pszLayerName);
    } else if (nLayers == 1) {
        return papoLayers[0];
    }
    return nullptr;
}

/************************************************************************/
/*                     OGRAMIGOCLOUDGetOptionValue()                       */
/************************************************************************/

CPLString OGRAMIGOCLOUDGetOptionValue(const char* pszFilename,
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

bool OGRAmigoCloudDataSource::ListDatasets()
{
    std::stringstream url;
    url << std::string(GetAPIURL()) << "/users/0/projects/" << std::string(GetProjectId()) << "/datasets/?summary";
    json_object* result = RunGET(url.str().c_str());
    if( result == nullptr ) {
        CPLError(CE_Failure, CPLE_AppDefined, "AmigoCloud:get failed.");
        return false;
    }

    {
        auto type = json_object_get_type(result);
        if(type == json_type_object)
        {
            json_object *poResults = CPL_json_object_object_get(result, "results");
            if(poResults != nullptr &&
                json_object_get_type(poResults) == json_type_array)
            {
                CPLprintf("List of available datasets for project id: %s\n", GetProjectId());
                CPLprintf("| id \t | name\n");
                CPLprintf("|--------|-------------------\n");
                const auto nSize = json_object_array_length(poResults);
                for(auto i = decltype(nSize){0}; i < nSize; ++i) {
                    json_object *ds = json_object_array_get_idx(poResults, i);
                    if(ds!=nullptr) {
                        const char *name = nullptr;
                        int64_t dataset_id = 0;
                        json_object *poName = CPL_json_object_object_get(ds, "name");
                        if (poName != nullptr) {
                            name = json_object_get_string(poName);
                        }
                        json_object *poId = CPL_json_object_object_get(ds, "id");
                        if (poId != nullptr) {
                            dataset_id = json_object_get_int64(poId);
                        }
                        if (name != nullptr) {
                            std::stringstream str;
                            str << "| " << dataset_id << "\t | " << name;
                            CPLprintf("%s\n", str.str().c_str());
                        }
                    }
                }
            }
        }
        json_object_put(result);
    }
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRAmigoCloudDataSource::Open( const char * pszFilename,
                                   char** papszOpenOptionsIn,
                                   int bUpdateIn )

{

    bReadWrite = CPL_TO_BOOL(bUpdateIn);

    pszName = CPLStrdup( pszFilename );
    pszProjectId = CPLStrdup(pszFilename + strlen("AMIGOCLOUD:"));
    char* pchSpace = strchr(pszProjectId, ' ');
    if( pchSpace )
        *pchSpace = '\0';
    if( pszProjectId[0] == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing project id");
        return FALSE;
    }

    osAPIKey = CSLFetchNameValueDef(papszOpenOptionsIn, "AMIGOCLOUD_API_KEY",
                                    CPLGetConfigOption("AMIGOCLOUD_API_KEY", ""));

    if (osAPIKey.empty())
    {
        osAPIKey = OGRAMIGOCLOUDGetOptionValue(pszFilename, "AMIGOCLOUD_API_KEY");
    }
    if (osAPIKey.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AMIGOCLOUD_API_KEY is not defined.\n");
        return FALSE;
    }

    OGRLayer* poSchemaLayer = ExecuteSQLInternal("SELECT current_schema()");
    if( poSchemaLayer )
    {
        OGRFeature* poFeat = poSchemaLayer->GetNextFeature();
        if( poFeat )
        {
            if( poFeat->GetFieldCount() == 1 )
            {
                osCurrentSchema = poFeat->GetFieldAsString(0);
            }
            delete poFeat;
        }
        ReleaseResultSet(poSchemaLayer);
    }
    if( osCurrentSchema.empty() )
        return FALSE;

    CPLString osDatasets = OGRAMIGOCLOUDGetOptionValue(pszFilename, "datasets");
    if (!osDatasets.empty())
    {
        char** papszTables = CSLTokenizeString2(osDatasets, ",", 0);
        for(int i=0;papszTables && papszTables[i];i++)
        {

            papoLayers = (OGRAmigoCloudTableLayer**) CPLRealloc(
                papoLayers, (nLayers + 1) * sizeof(OGRAmigoCloudTableLayer*));

            papoLayers[nLayers ++] = new OGRAmigoCloudTableLayer(this, papszTables[i]);
        }
        CSLDestroy(papszTables);

        // If OVERWRITE: YES, truncate the layer.
        if( nLayers==1 &&
            CPLFetchBool(papszOpenOptionsIn, "OVERWRITE", false) )
        {
           TruncateDataset(papoLayers[0]->GetTableName());
        }
        return TRUE;
    } else {
        // If 'datasets' word is in the filename, but no dataset id specified,
        // print the list of available datasets
        if(std::string(pszFilename).find("datasets") != std::string::npos)
            ListDatasets();
    }

    return TRUE;
}

/************************************************************************/
/*                            GetAPIURL()                               */
/************************************************************************/

const char* OGRAmigoCloudDataSource::GetAPIURL() const
{
    const char* pszAPIURL = CPLGetConfigOption("AMIGOCLOUD_API_URL", nullptr);
    if (pszAPIURL)
        return pszAPIURL;

    else if( bUseHTTPS )
        return CPLSPrintf("https://app.amigocloud.com/api/v1");
    else
        return CPLSPrintf("http://app.amigocloud.com/api/v1");
}

/************************************************************************/
/*                             FetchSRSId()                             */
/************************************************************************/

int OGRAmigoCloudDataSource::FetchSRSId( OGRSpatialReference * poSRS )

{
    if( poSRS == nullptr )
        return 0;

    OGRSpatialReference oSRS(*poSRS);
    // cppcheck-suppress uselessAssignmentPtrArg
    poSRS = nullptr;

    const char* pszAuthorityName = oSRS.GetAuthorityName(nullptr);

    if( pszAuthorityName == nullptr || strlen(pszAuthorityName) == 0 )
    {
/* -------------------------------------------------------------------- */
/*      Try to identify an EPSG code                                    */
/* -------------------------------------------------------------------- */
        oSRS.AutoIdentifyEPSG();

        pszAuthorityName = oSRS.GetAuthorityName(nullptr);
        if (pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG"))
        {
            const char* pszAuthorityCode = oSRS.GetAuthorityCode(nullptr);
            if ( pszAuthorityCode != nullptr && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                oSRS.importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = oSRS.GetAuthorityName(nullptr);
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      Check whether the EPSG authority code is already mapped to a    */
/*      SRS ID.                                                         */
/* -------------------------------------------------------------------- */
    if( pszAuthorityName != nullptr && EQUAL( pszAuthorityName, "EPSG" ) )
    {
        /* For the root authority name 'EPSG', the authority code
         * should always be integral
         */
        const int nAuthorityCode = atoi( oSRS.GetAuthorityCode(nullptr) );

        return nAuthorityCode;
    }

    return 0;
}

/************************************************************************/
/*                          ICreateLayer()                              */
/************************************************************************/

OGRLayer   *OGRAmigoCloudDataSource::ICreateLayer( const char *pszNameIn,
                                           OGRSpatialReference *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           char ** papszOptions )
{
    if( !bReadWrite )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return nullptr;
    }

    CPLString osName(pszNameIn);
    OGRAmigoCloudTableLayer* poLayer = new OGRAmigoCloudTableLayer(this, osName);
    const bool bGeomNullable =
        CPLFetchBool(papszOptions, "GEOMETRY_NULLABLE", true);
    OGRSpatialReference* poSRSClone = poSpatialRef;
    if( poSRSClone )
    {
        poSRSClone = poSRSClone->Clone();
        poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    poLayer->SetDeferredCreation(eGType, poSRSClone, bGeomNullable);
    if( poSRSClone )
        poSRSClone->Release();
    papoLayers = (OGRAmigoCloudTableLayer**) CPLRealloc(
                    papoLayers, (nLayers + 1) * sizeof(OGRAmigoCloudTableLayer*));
    papoLayers[nLayers ++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRAmigoCloudDataSource::DeleteLayer(int iLayer)
{
    if( !bReadWrite )
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

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osDatasetId = papoLayers[iLayer]->GetDatasetId();

    CPLDebug( "AMIGOCLOUD", "DeleteLayer(%s)", osDatasetId.c_str() );

    int bDeferredCreation = papoLayers[iLayer]->GetDeferredCreation();
    papoLayers[iLayer]->CancelDeferredCreation();
    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

    if (osDatasetId.empty())
        return OGRERR_NONE;

    if( !bDeferredCreation )
    {
        std::stringstream url;
        url << std::string(GetAPIURL()) << "/users/0/projects/" + std::string(GetProjectId()) + "/datasets/"+ osDatasetId.c_str();
        if( !RunDELETE(url.str().c_str()) ) {
            return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          AddHTTPOptions()                            */
/************************************************************************/

char** OGRAmigoCloudDataSource::AddHTTPOptions()
{
    bMustCleanPersistent = true;

    return CSLAddString(nullptr, CPLSPrintf("PERSISTENT=AMIGOCLOUD:%p", this));
}

/************************************************************************/
/*                               RunPOST()                               */
/************************************************************************/

json_object* OGRAmigoCloudDataSource::RunPOST(const char*pszURL, const char *pszPostData, const char *pszHeaders)
{
    CPLString osURL(pszURL);

    /* -------------------------------------------------------------------- */
    /*      Provide the API Key                                             */
    /* -------------------------------------------------------------------- */
    if( !osAPIKey.empty() )
    {
        if(osURL.find("?") == std::string::npos)
            osURL += "?token=";
        else
            osURL += "&token=";
        osURL += osAPIKey;
    }

    char** papszOptions=nullptr;
    CPLString osPOSTFIELDS("POSTFIELDS=");
    if (pszPostData)
        osPOSTFIELDS += pszPostData;
    papszOptions = CSLAddString(papszOptions, osPOSTFIELDS);
    papszOptions = CSLAddString(papszOptions, pszHeaders);
    papszOptions = CSLAddString(papszOptions, GetUserAgentOption().c_str());

    CPLHTTPResult * psResult = CPLHTTPFetch( osURL.c_str(), papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == nullptr )
        return nullptr;

    if (psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunPOST HTML Response: %s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server: %s", psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }
    if (psResult->pszErrBuf != nullptr && psResult->pabyData != nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "POST Response: %s", psResult->pabyData );
    }
    else if (psResult->nStatus != 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunPOST Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == nullptr )
    {
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

    if( poObj != nullptr )
    {
        if( json_object_get_type(poObj) == json_type_object )
        {
            json_object* poError = CPL_json_object_object_get(poObj, "error");
            if( poError != nullptr && json_object_get_type(poError) == json_type_array &&
                json_object_array_length(poError) > 0 )
            {
                poError = json_object_array_get_idx(poError, 0);
                if( poError != nullptr && json_object_get_type(poError) == json_type_string )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error returned by server : %s", json_object_get_string(poError));
                    json_object_put(poObj);
                    return nullptr;
                }
            }
            json_object* poJob = CPL_json_object_object_get(poObj, "job");
            if (poJob != nullptr) {
                const char *job = json_object_get_string(poJob);
                if (job != nullptr) {
                    waitForJobToFinish(job);
                }
            }
        }
        else
        {
            json_object_put(poObj);
            return nullptr;
        }
    }

    return poObj;
}

bool OGRAmigoCloudDataSource::waitForJobToFinish(const char* jobId)
{
    std::stringstream url;
    url << std::string(GetAPIURL()) << "/me/jobs/" << std::string(jobId);
    int count = 0;
    while (count<5) {
        count++;
        json_object *result = RunGET(url.str().c_str());
        if (result == nullptr) {
            CPLError(CE_Failure, CPLE_AppDefined, "waitForJobToFinish failed.");
            return false;
        }

        {
            int type = json_object_get_type(result);
            if (type == json_type_object) {
                json_object *poStatus = CPL_json_object_object_get(result, "status");
                const char *status = json_object_get_string(poStatus);
                if (status != nullptr) {
                    if (std::string(status) == "SUCCESS") {
                        return true;
                    } else if (std::string(status) == "FAILURE") {
                        CPLError(CE_Failure, CPLE_AppDefined, "Job failed : %s", json_object_get_string(result));
                        return false;
                    }
                }
            }
        }
        CPLSleep(1.0); // Sleep 1 sec.
    }
    return false;
}

bool OGRAmigoCloudDataSource::TruncateDataset(const CPLString &tableName)
{
    std::stringstream changeset;
    changeset << "[{\"type\":\"DML\",\"entity\":\"" << tableName << "\",";
    changeset << "\"parent\":null,\"action\":\"TRUNCATE\",\"data\":null}]";
    SubmitChangeset(changeset.str());
    return true;
}

void OGRAmigoCloudDataSource::SubmitChangeset(const CPLString &json)
{
    std::stringstream url;
    url << std::string(GetAPIURL()) << "/users/0/projects/" + std::string(GetProjectId()) + "/submit_changeset";
    std::stringstream changeset;
    changeset << "{\"changeset\":\"" << OGRAMIGOCLOUDJsonEncode(json) << "\"}";
    json_object* poObj = RunPOST(url.str().c_str(), changeset.str().c_str());
    if( poObj != nullptr )
        json_object_put(poObj);
}

/************************************************************************/
/*                               RunDELETE()                               */
/************************************************************************/

bool OGRAmigoCloudDataSource::RunDELETE(const char*pszURL)
{
    CPLString osURL(pszURL);

    /* -------------------------------------------------------------------- */
    /*      Provide the API Key                                             */
    /* -------------------------------------------------------------------- */
    if( !osAPIKey.empty() )
    {
        if(osURL.find("?") == std::string::npos)
            osURL += "?token=";
        else
            osURL += "&token=";
        osURL += osAPIKey;
    }

    char** papszOptions=nullptr;
    CPLString osPOSTFIELDS("CUSTOMREQUEST=DELETE");
    papszOptions = CSLAddString(papszOptions, osPOSTFIELDS);
    papszOptions = CSLAddString(papszOptions, GetUserAgentOption().c_str());

    CPLHTTPResult * psResult = CPLHTTPFetch( osURL.c_str(), papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == nullptr )
        return false;

    if (psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunDELETE HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server:%s", psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return false;
    }
    if (psResult->pszErrBuf != nullptr && psResult->pabyData != nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "DELETE Response: %s", psResult->pabyData );
    }
    else if ( psResult->nStatus != 0)
    {
        CPLDebug( "AMIGOCLOUD", "DELETE Error Status:%d", psResult->nStatus );
    }
    CPLHTTPDestroyResult(psResult);

    return true;
}

/************************************************************************/
/*                               RunGET()                               */
/************************************************************************/

json_object* OGRAmigoCloudDataSource::RunGET(const char*pszURL)
{
    CPLString osURL(pszURL);

    /* -------------------------------------------------------------------- */
    /*      Provide the API Key                                             */
    /* -------------------------------------------------------------------- */
    if( !osAPIKey.empty() )
    {
        if(osURL.find("?") == std::string::npos)
            osURL += "?token=";
        else
            osURL += "&token=";
        osURL += osAPIKey;
    }
    char** papszOptions=nullptr;
    papszOptions = CSLAddString(papszOptions, GetUserAgentOption().c_str());

    CPLHTTPResult * psResult = CPLHTTPFetch( osURL.c_str(), papszOptions);
    CSLDestroy( papszOptions );
    if( psResult == nullptr ) {
        return nullptr;
    }

    if (psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server:%s", psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }
    if (psResult->pszErrBuf != nullptr && psResult->pabyData != nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "GET Response: %s", psResult->pabyData );
    }
    else if (psResult->nStatus != 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunGET Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == nullptr )
    {
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    CPLDebug( "AMIGOCLOUD", "RunGET Response:%s", psResult->pabyData );

    json_object* poObj = nullptr;
    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
    if( !OGRJSonParse(pszText, &poObj, true) )
    {
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    CPLHTTPDestroyResult(psResult);

    if( poObj != nullptr )
    {
        if( json_object_get_type(poObj) == json_type_object )
        {
            json_object* poError = CPL_json_object_object_get(poObj, "error");
            if( poError != nullptr && json_object_get_type(poError) == json_type_array &&
                json_object_array_length(poError) > 0 )
            {
                poError = json_object_array_get_idx(poError, 0);
                if( poError != nullptr && json_object_get_type(poError) == json_type_string )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error returned by server : %s", json_object_get_string(poError));
                    json_object_put(poObj);
                    return nullptr;
                }
            }
        }
        else
        {
            json_object_put(poObj);
            return nullptr;
        }
    }

    return poObj;
}

/************************************************************************/
/*                               RunSQL()                               */
/************************************************************************/

json_object* OGRAmigoCloudDataSource::RunSQL(const char* pszUnescapedSQL)
{
    CPLString osSQL;
    std::string pszAPIURL = GetAPIURL();
    osSQL = pszAPIURL + "/users/0/projects/" + CPLString(pszProjectId) + "/sql";
    std::string sql = pszUnescapedSQL;
    if (sql.find("DELETE") != std::string::npos || sql.find("delete") != std::string::npos ||
        sql.find("INSERT") != std::string::npos || sql.find("insert") != std::string::npos ||
        sql.find("UPDATE") != std::string::npos || sql.find("update") != std::string::npos) {
        std::stringstream query;
        query << "{\"query\": \"" << OGRAMIGOCLOUDJsonEncode(pszUnescapedSQL) << "\"}";
        return RunPOST(osSQL.c_str(), query.str().c_str());
    } else {
        osSQL += "?query=";
        char * pszEscaped = CPLEscapeString( pszUnescapedSQL, -1, CPLES_URL );
        osSQL += pszEscaped;
        CPLFree(pszEscaped);
        return RunGET(osSQL.c_str());
    }
}

/************************************************************************/
/*                        OGRAMIGOCLOUDGetSingleRow()                      */
/************************************************************************/

json_object* OGRAMIGOCLOUDGetSingleRow(json_object* poObj)
{
    if( poObj == nullptr )
    {
        return nullptr;
    }

    json_object* poRows = CPL_json_object_object_get(poObj, "data");
    if( poRows == nullptr ||
        json_object_get_type(poRows) != json_type_array ||
        json_object_array_length(poRows) != 1 )
    {
        return nullptr;
    }

    json_object* poRowObj = json_object_array_get_idx(poRows, 0);
    if( poRowObj == nullptr || json_object_get_type(poRowObj) != json_type_object )
    {
        return nullptr;
    }

    return poRowObj;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRAmigoCloudDataSource::ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect )

{
    return ExecuteSQLInternal(pszSQLCommand, poSpatialFilter, pszDialect, true);
}

OGRLayer * OGRAmigoCloudDataSource::ExecuteSQLInternal(
    const char *pszSQLCommand,
    OGRGeometry *poSpatialFilter,
    const char *,
    bool bRunDeferredActions )

{
    if( bRunDeferredActions )
    {
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            papoLayers[iLayer]->RunDeferredCreationIfNecessary();
            papoLayers[iLayer]->FlushDeferredInsert();
        }
    }

    /* Skip leading spaces */
    while(*pszSQLCommand == ' ')
        pszSQLCommand ++;

    if( !EQUALN(pszSQLCommand, "SELECT", strlen("SELECT")) &&
        !EQUALN(pszSQLCommand, "EXPLAIN", strlen("EXPLAIN")) &&
        !EQUALN(pszSQLCommand, "WITH", strlen("WITH")) )
    {
        RunSQL(pszSQLCommand);
        return nullptr;
    }

    OGRAmigoCloudResultLayer* poLayer = new OGRAmigoCloudResultLayer( this, pszSQLCommand );

    if( poSpatialFilter != nullptr )
        poLayer->SetSpatialFilter( poSpatialFilter );

    if( !poLayer->IsOK() )
    {
        delete poLayer;
        return nullptr;
    }

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRAmigoCloudDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
