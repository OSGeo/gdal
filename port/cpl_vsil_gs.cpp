/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Google Cloud Storage
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2018, Even Rouault <even.rouault at spatialys.com>
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

#include "cpl_port.h"
#include "cpl_http.h"
#include "cpl_minixml.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_vsil_curl_class.h"

#include <errno.h>

#include <algorithm>
#include <set>
#include <map>
#include <memory>

#include "cpl_google_cloud.h"

CPL_CVSID("$Id$")

#ifndef HAVE_CURL

void VSIInstallGSFileHandler( void )
{
    // Not supported.
}

#else

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

#define ENABLE_DEBUG 0

#define unchecked_curl_easy_setopt(handle,opt,param) CPL_IGNORE_RET_VAL(curl_easy_setopt(handle,opt,param))

namespace cpl {

/************************************************************************/
/*                         VSIGSFSHandler                               */
/************************************************************************/

class VSIGSFSHandler final : public IVSIS3LikeFSHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGSFSHandler)

  protected:
    VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
    const char* GetDebugKey() const override { return "GS"; }

    CPLString GetFSPrefix() const override { return "/vsigs/"; }
    CPLString GetURLFromFilename( const CPLString& osFilename ) override;

    IVSIS3LikeHandleHelper* CreateHandleHelper(
        const char* pszURI, bool bAllowNoObject) override;

    void ClearCache() override;

    bool IsAllowedHeaderForObjectCreation( const char* pszHeaderName ) override { return STARTS_WITH(pszHeaderName, "x-goog-"); }

  public:
    VSIGSFSHandler() = default;
    ~VSIGSFSHandler() override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError,
                            CSLConstList papszOptions ) override;

    const char* GetOptions() override;

    char* GetSignedURL( const char* pszFilename, CSLConstList papszOptions ) override;

    // Multipart upload
    bool SupportsParallelMultipartUpload() const override { return true; }

    char** GetFileMetadata( const char * pszFilename, const char* pszDomain,
                            CSLConstList papszOptions ) override;

    bool   SetFileMetadata( const char * pszFilename,
                            CSLConstList papszMetadata,
                            const char* pszDomain,
                            CSLConstList papszOptions ) override;

    int* UnlinkBatch( CSLConstList papszFiles ) override;
    int RmdirRecursive( const char* pszDirname ) override;

    std::string GetStreamingFilename(const std::string& osFilename) const override;
};

/************************************************************************/
/*                            VSIGSHandle                               */
/************************************************************************/

class VSIGSHandle final : public IVSIS3LikeHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGSHandle)

    VSIGSHandleHelper* m_poHandleHelper = nullptr;

  protected:
    struct curl_slist* GetCurlHeaders( const CPLString& osVerb,
        const struct curl_slist* psExistingHeaders ) override;

  public:
    VSIGSHandle( VSIGSFSHandler* poFS, const char* pszFilename,
                 VSIGSHandleHelper* poHandleHelper);
    ~VSIGSHandle() override;
};

/************************************************************************/
/*                          ~VSIGSFSHandler()                           */
/************************************************************************/

VSIGSFSHandler::~VSIGSFSHandler()
{
    VSICurlFilesystemHandlerBase::ClearCache();
    VSIGSHandleHelper::CleanMutex();
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIGSFSHandler::ClearCache()
{
    VSICurlFilesystemHandlerBase::ClearCache();

    VSIGSHandleHelper::ClearCache();
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIGSFSHandler::CreateFileHandle(const char* pszFilename)
{
    VSIGSHandleHelper* poHandleHelper =
        VSIGSHandleHelper::BuildFromURI( pszFilename + GetFSPrefix().size(),
                                         GetFSPrefix() );
    if( poHandleHelper == nullptr )
        return nullptr;
    return new VSIGSHandle(this, pszFilename, poHandleHelper);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIGSFSHandler::Open( const char *pszFilename,
                                        const char *pszAccess,
                                        bool bSetError,
                                        CSLConstList papszOptions )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr )
    {
        if( strchr(pszAccess, '+') != nullptr &&
            !CPLTestBool(CPLGetConfigOption("CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "NO")) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "w+ not supported for /vsigs, unless "
                        "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE is set to YES");
            errno = EACCES;
            return nullptr;
        }

        VSIGSHandleHelper* poHandleHelper =
            VSIGSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str());
        if( poHandleHelper == nullptr )
            return nullptr;
        VSIS3WriteHandle* poHandle =
            new VSIS3WriteHandle(this, pszFilename, poHandleHelper, false, papszOptions);
        if( !poHandle->IsOK() )
        {
            delete poHandle;
            return nullptr;
        }
        if( strchr(pszAccess, '+') != nullptr)
        {
            return VSICreateUploadOnCloseFile(poHandle);
        }
        return poHandle;
    }

    return
        VSICurlFilesystemHandlerBase::Open(pszFilename, pszAccess, bSetError, papszOptions);
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char* VSIGSFSHandler::GetOptions()
{
    static CPLString osOptions(
        CPLString("<Options>") +
    "  <Option name='GS_SECRET_ACCESS_KEY' type='string' "
        "description='Secret access key. To use with GS_ACCESS_KEY_ID'/>"
    "  <Option name='GS_ACCESS_KEY_ID' type='string' "
        "description='Access key id'/>"
    "  <Option name='GS_NO_SIGN_REQUEST' type='boolean' "
        "description='Whether to disable signing of requests' default='NO'/>"
    "  <Option name='GS_OAUTH2_REFRESH_TOKEN' type='string' "
        "description='OAuth2 refresh token. For OAuth2 client authentication. "
        "To use with GS_OAUTH2_CLIENT_ID and GS_OAUTH2_CLIENT_SECRET'/>"
    "  <Option name='GS_OAUTH2_CLIENT_ID' type='string' "
        "description='OAuth2 client id for OAuth2 client authentication'/>"
    "  <Option name='GS_OAUTH2_CLIENT_SECRET' type='string' "
        "description='OAuth2 client secret for OAuth2 client authentication'/>"
    "  <Option name='GS_OAUTH2_PRIVATE_KEY' type='string' "
        "description='Private key for OAuth2 service account authentication. "
        "To use with GS_OAUTH2_CLIENT_EMAIL'/>"
    "  <Option name='GS_OAUTH2_PRIVATE_KEY_FILE' type='string' "
        "description='Filename that contains private key for OAuth2 service "
        "account authentication. "
        "To use with GS_OAUTH2_CLIENT_EMAIL'/>"
    "  <Option name='GS_OAUTH2_CLIENT_EMAIL' type='string' "
        "description='Client email to use with OAuth2 service account "
        "authentication'/>"
    "  <Option name='GS_OAUTH2_SCOPE' type='string' "
        "description='OAuth2 authorization scope' "
        "default='https://www.googleapis.com/auth/devstorage.read_write'/>"
    "  <Option name='CPL_MACHINE_IS_GCE' type='boolean' "
        "description='Whether the current machine is a Google Compute Engine "
        "instance' default='NO'/>"
    "  <Option name='CPL_GCE_CHECK_LOCAL_FILES' type='boolean' "
        "description='Whether to check system logs to determine "
        "if current machine is a GCE instance' default='YES'/>"
        "description='Filename that contains AWS configuration' "
        "default='~/.aws/config'/>"
    "  <Option name='CPL_GS_CREDENTIALS_FILE' type='string' "
        "description='Filename that contains Google Storage credentials' "
        "default='~/.boto'/>"
    + VSICurlFilesystemHandlerBase::GetOptionsStatic() +
        "</Options>");
    return osOptions.c_str();
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

char* VSIGSFSHandler::GetSignedURL(const char* pszFilename, CSLConstList papszOptions )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    VSIGSHandleHelper* poHandleHelper =
        VSIGSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str(),
                                        papszOptions);
    if( poHandleHelper == nullptr )
    {
        return nullptr;
    }

    CPLString osRet(poHandleHelper->GetSignedURL(papszOptions));

    delete poHandleHelper;
    return osRet.empty() ? nullptr : CPLStrdup(osRet);
}

/************************************************************************/
/*                          GetURLFromFilename()                         */
/************************************************************************/

CPLString VSIGSFSHandler::GetURLFromFilename( const CPLString& osFilename )
{
    CPLString osFilenameWithoutPrefix = osFilename.substr(GetFSPrefix().size());
    VSIGSHandleHelper* poHandleHelper =
        VSIGSHandleHelper::BuildFromURI( osFilenameWithoutPrefix, GetFSPrefix() );
    if( poHandleHelper == nullptr )
        return CPLString();
    CPLString osURL( poHandleHelper->GetURL() );
    delete poHandleHelper;
    return osURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper* VSIGSFSHandler::CreateHandleHelper(const char* pszURI,
                                                           bool)
{
    return VSIGSHandleHelper::BuildFromURI(pszURI, GetFSPrefix().c_str());
}

/************************************************************************/
/*                          GetFileMetadata()                           */
/************************************************************************/

char** VSIGSFSHandler::GetFileMetadata( const char* pszFilename,
                                        const char* pszDomain,
                                        CSLConstList papszOptions )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( pszDomain == nullptr || !EQUAL(pszDomain, "ACL") )
    {
        return VSICurlFilesystemHandlerBase::GetFileMetadata(
                    pszFilename, pszDomain, papszOptions);
    }

    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        VSIGSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str()));
    if( !poHandleHelper )
        return nullptr;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix());
    NetworkStatisticsAction oContextAction("GetFileMetadata");

    bool bRetry;
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry = atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;

    CPLStringList aosResult;
    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        poHandleHelper->AddQueryParameter("acl", "");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              poHandleHelper->GetURL().c_str(),
                              nullptr));
        headers = VSICurlMergeHeaders(headers,
                        poHandleHelper->GetCurlHeaders("GET", headers));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poHandleHelper.get());

        NetworkStatisticsLogger::LogGET(requestHelper.sWriteFuncData.nSize);

        if( response_code != 200 || requestHelper.sWriteFuncData.pBuffer == nullptr )
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer, requestHelper.szCurlErrBuf);
            if( dfNewRetryDelay > 0 &&
                nRetryCount < nMaxRetry )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                            "HTTP error code: %d - %s. "
                            "Retrying again in %.1f secs",
                            static_cast<int>(response_code),
                            poHandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                         ? requestHelper.sWriteFuncData.pBuffer
                         : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "GetFileMetadata failed");
            }
        }
        else
        {
            aosResult.SetNameValue("XML", requestHelper.sWriteFuncData.pBuffer);
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );
    return CSLDuplicate(aosResult.List());
}

/************************************************************************/
/*                          SetFileMetadata()                           */
/************************************************************************/

bool VSIGSFSHandler::SetFileMetadata( const char * pszFilename,
                                      CSLConstList papszMetadata,
                                      const char* pszDomain,
                                      CSLConstList /* papszOptions */ )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return false;

    if( pszDomain == nullptr ||
        !(EQUAL(pszDomain, "HEADERS") || EQUAL(pszDomain, "ACL")) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only HEADERS and ACL domain are supported");
        return false;
    }

    if( EQUAL(pszDomain, "HEADERS") )
    {
        return CopyObject(pszFilename, pszFilename, papszMetadata) == 0;
    }

    const char* pszXML = CSLFetchNameValue(papszMetadata, "XML");
    if( pszXML == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "XML key is missing in metadata");
        return false;
    }

    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        VSIGSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str()));
    if( !poHandleHelper )
        return false;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix());
    NetworkStatisticsAction oContextAction("SetFileMetadata");

    bool bRetry;
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry = atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;

    bool bRet = false;

    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        poHandleHelper->AddQueryParameter("acl", "");
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT");
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS, pszXML );

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              poHandleHelper->GetURL().c_str(),
                              nullptr));
        headers = curl_slist_append(headers, "Content-Type: application/xml");
        headers = VSICurlMergeHeaders(headers,
                        poHandleHelper->GetCurlHeaders("PUT", headers,
                                                        pszXML,
                                                        strlen(pszXML)));
        NetworkStatisticsLogger::LogPUT(strlen(pszXML));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poHandleHelper.get());

        if( response_code != 200 )
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer, requestHelper.szCurlErrBuf);
            if( dfNewRetryDelay > 0 &&
                nRetryCount < nMaxRetry )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                            "HTTP error code: %d - %s. "
                            "Retrying again in %.1f secs",
                            static_cast<int>(response_code),
                            poHandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                         ? requestHelper.sWriteFuncData.pBuffer
                         : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "SetFileMetadata failed");
            }
        }
        else
        {
            bRet = true;
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );
    return bRet;
}

/************************************************************************/
/*                           UnlinkBatch()                              */
/************************************************************************/

int* VSIGSFSHandler::UnlinkBatch( CSLConstList papszFiles )
{
    // Implemented using https://cloud.google.com/storage/docs/json_api/v1/how-tos/batch

    auto poHandleHelper = std::unique_ptr<VSIGSHandleHelper>(
        VSIGSHandleHelper::BuildFromURI("batch/storage/v1",
                                        GetFSPrefix().c_str()));

    // The JSON API cannot be used with HMAC keys
    if( poHandleHelper && poHandleHelper->UsesHMACKey() )
    {
        CPLDebug(GetDebugKey(),
                 "UnlinkBatch() has an efficient implementation "
                 "only for OAuth2 authentication");
        return VSICurlFilesystemHandlerBase::UnlinkBatch(papszFiles);
    }

    int* panRet = static_cast<int*>(
        CPLCalloc(sizeof(int), CSLCount(papszFiles)));

    if( !poHandleHelper )
        return panRet;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix());
    NetworkStatisticsAction oContextAction("UnlinkBatch");

    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry = atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)));

    // For debug / testing only
    const int nBatchSize = std::max(1, std::min(100,
        atoi(CPLGetConfigOption("CPL_VSIGS_UNLINK_BATCH_SIZE", "100"))));
    CPLString osPOSTContent;
    for( int i = 0; papszFiles && papszFiles[i]; i++ )
    {
        CPLAssert( STARTS_WITH_CI(papszFiles[i], GetFSPrefix()) );
        const char* pszFilenameWithoutPrefix = papszFiles[i] + GetFSPrefix().size();
        const char* pszSlash = strchr(pszFilenameWithoutPrefix, '/');
        if( !pszSlash )
            return panRet;
        CPLString osBucket;
        osBucket.assign(pszFilenameWithoutPrefix, pszSlash - pszFilenameWithoutPrefix);

        std::string osResource = "storage/v1/b/";
        osResource += osBucket;
        osResource += "/o/";
        osResource += CPLAWSURLEncode(pszSlash + 1, true);

#ifdef ADD_AUTH_TO_NESTED_REQUEST
        std::string osAuthorization;
        std::string osDate;
        {
            auto poTmpHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
                VSIGSHandleHelper::BuildFromURI(osResource.c_str(),
                                                GetFSPrefix().c_str()));
            CURL* hCurlHandle = curl_easy_init();
            struct curl_slist* subrequest_headers = static_cast<struct curl_slist*>(
                        CPLHTTPSetOptions(hCurlHandle,
                                          poTmpHandleHelper->GetURL().c_str(),
                                          nullptr));
            subrequest_headers = poTmpHandleHelper->GetCurlHeaders(
                "DELETE", subrequest_headers, nullptr, 0);
            for( struct curl_slist *iter = subrequest_headers; iter; iter = iter->next )
            {
                if( STARTS_WITH_CI(iter->data, "Authorization: ") )
                {
                   osAuthorization = iter->data;
                }
                else if( STARTS_WITH_CI(iter->data, "Date: ") )
                {
                   osDate = iter->data;
                }
            }
            curl_slist_free_all(subrequest_headers);
            curl_easy_cleanup(hCurlHandle);
        }
#endif

        osPOSTContent += "--===============7330845974216740156==\r\n";
        osPOSTContent += "Content-Type: application/http\r\n";
        osPOSTContent += CPLSPrintf("Content-ID: <%d>\r\n", i+1);
        osPOSTContent += "\r\n\r\n";
        osPOSTContent += "DELETE /";
        osPOSTContent += osResource;
        osPOSTContent += " HTTP/1.1\r\n";
#ifdef ADD_AUTH_TO_NESTED_REQUEST
        if( !osAuthorization.empty() )
        {
            osPOSTContent += osAuthorization;
            osPOSTContent += "\r\n";
        }
        if( !osDate.empty() )
        {
            osPOSTContent += osDate;
            osPOSTContent += "\r\n";
        }
#endif
        osPOSTContent += "\r\n\r\n";

        if( ((i+1) % nBatchSize) == 0 || papszFiles[i+1] == nullptr )
        {
            osPOSTContent += "--===============7330845974216740156==--\r\n";

#ifdef DEBUG_VERBOSE
            CPLDebug(GetDebugKey(), "%s", osPOSTContent.c_str());
#endif

            // Run request
            int nRetryCount = 0;
            bool bRetry;
            std::string osResponse;
            do
            {
                bRetry = false;
                CURL* hCurlHandle = curl_easy_init();

                unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");
                unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS, osPOSTContent.c_str() );

                struct curl_slist* headers = static_cast<struct curl_slist*>(
                    CPLHTTPSetOptions(hCurlHandle,
                                      poHandleHelper->GetURL().c_str(),
                                      nullptr));
                headers = curl_slist_append(headers,
                    "Content-Type: multipart/mixed; boundary=\"===============7330845974216740156==\"");
                headers = VSICurlMergeHeaders(headers,
                                poHandleHelper->GetCurlHeaders("POST", headers,
                                                               osPOSTContent.c_str(),
                                                               osPOSTContent.size()));

                CurlRequestHelper requestHelper;
                const long response_code =
                    requestHelper.perform(hCurlHandle, headers, this, poHandleHelper.get());

                NetworkStatisticsLogger::LogPOST(osPOSTContent.size(),
                                                 requestHelper.sWriteFuncData.nSize);

                if( response_code != 200 || requestHelper.sWriteFuncData.pBuffer == nullptr )
                {
                    // Look if we should attempt a retry
                    const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                        static_cast<int>(response_code), dfRetryDelay,
                        requestHelper.sWriteFuncHeaderData.pBuffer, requestHelper.szCurlErrBuf);
                    if( dfNewRetryDelay > 0 &&
                        nRetryCount < nMaxRetry )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                    "HTTP error code: %d - %s. "
                                    "Retrying again in %.1f secs",
                                    static_cast<int>(response_code),
                                    poHandleHelper->GetURL().c_str(),
                                    dfRetryDelay);
                        CPLSleep(dfRetryDelay);
                        dfRetryDelay = dfNewRetryDelay;
                        nRetryCount++;
                        bRetry = true;
                    }
                    else
                    {
                        CPLDebug(GetDebugKey(), "%s",
                                 requestHelper.sWriteFuncData.pBuffer
                                 ? requestHelper.sWriteFuncData.pBuffer
                                 : "(null)");
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "DeleteObjects failed");
                    }
                }
                else
                {
#ifdef DEBUG_VERBOSE
                    CPLDebug(GetDebugKey(), "%s", requestHelper.sWriteFuncData.pBuffer);
#endif
                    osResponse = requestHelper.sWriteFuncData.pBuffer;
                }

                curl_easy_cleanup(hCurlHandle);
            }
            while( bRetry );

            // Mark deleted files
            for( int j = i+1 - nBatchSize; j <= i; j++)
            {
                auto nPos = osResponse.find(CPLSPrintf("Content-ID: <response-%d>", j+1));
                if( nPos != std::string::npos )
                {
                    nPos = osResponse.find("HTTP/1.1 ", nPos);
                    if( nPos != std::string::npos )
                    {
                        const char* pszHTTPCode =
                            osResponse.c_str() + nPos + strlen("HTTP/1.1 ");
                        panRet[j] = (atoi(pszHTTPCode) == 204) ? 1 : 0;
                    }
                }
            }

            osPOSTContent.clear();
        }
    }
    return panRet;
}

/************************************************************************/
/*                           RmdirRecursive()                           */
/************************************************************************/

int VSIGSFSHandler::RmdirRecursive( const char* pszDirname )
{
    // For debug / testing only
    const int nBatchSize = std::min(100,
        atoi(CPLGetConfigOption("CPL_VSIGS_UNLINK_BATCH_SIZE", "100")));

    return RmdirRecursiveInternal(pszDirname, nBatchSize);
}

/************************************************************************/
/*                      GetStreamingFilename()                          */
/************************************************************************/

std::string VSIGSFSHandler::GetStreamingFilename(const std::string& osFilename) const
{
    if( STARTS_WITH(osFilename.c_str(), GetFSPrefix().c_str()) )
        return "/vsigs_streaming/" + osFilename.substr(GetFSPrefix().size());
    return osFilename;
}

/************************************************************************/
/*                             VSIGSHandle()                            */
/************************************************************************/

VSIGSHandle::VSIGSHandle( VSIGSFSHandler* poFSIn,
                          const char* pszFilename,
                          VSIGSHandleHelper* poHandleHelper ) :
        IVSIS3LikeHandle(poFSIn, pszFilename, poHandleHelper->GetURL()),
        m_poHandleHelper(poHandleHelper)
{
}

/************************************************************************/
/*                            ~VSIGSHandle()                            */
/************************************************************************/

VSIGSHandle::~VSIGSHandle()
{
    delete m_poHandleHelper;
}


/************************************************************************/
/*                          GetCurlHeaders()                            */
/************************************************************************/

struct curl_slist* VSIGSHandle::GetCurlHeaders( const CPLString& osVerb,
                                const struct curl_slist* psExistingHeaders )
{
    return m_poHandleHelper->GetCurlHeaders( osVerb, psExistingHeaders );
}

} /* end of namespace cpl */


#endif // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                      VSIInstallGSFileHandler()                       */
/************************************************************************/

/*!
 \brief Install /vsigs/ Google Cloud Storage file system handler
 (requires libcurl)

 \verbatim embed:rst
 See :ref:`/vsigs/ documentation <vsigs>`
 \endverbatim

 @since GDAL 2.2
 */

void VSIInstallGSFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsigs/", new cpl::VSIGSFSHandler );
}

#endif /* HAVE_CURL */
