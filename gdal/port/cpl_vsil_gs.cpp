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
    VSICurlFilesystemHandler::ClearCache();
    VSIGSHandleHelper::CleanMutex();
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIGSFSHandler::ClearCache()
{
    VSICurlFilesystemHandler::ClearCache();

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
        VSICurlFilesystemHandler::Open(pszFilename, pszAccess, bSetError, papszOptions);
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
        "default='~/.boto'/>" +
        VSICurlFilesystemHandler::GetOptionsStatic() +
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
        return VSICurlFilesystemHandler::GetFileMetadata(
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

    if( pszDomain == nullptr || !(EQUAL(pszDomain, "ACL")) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only ACL domain is supported");
        return false;
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
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS, pszXML );

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

/**
 * \brief Install /vsigs/ Google Cloud Storage file system handler
 * (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsigs">/vsigs/ documentation</a>
 *
 * @since GDAL 2.2
 */

void VSIInstallGSFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsigs/", new cpl::VSIGSFSHandler );
}

#endif /* HAVE_CURL */
