/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Google Cloud Storage
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2018, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
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

#ifndef HAVE_CURL

void VSIInstallGSFileHandler(void)
{
    // Not supported.
}

#else

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

#define ENABLE_DEBUG 0

#define unchecked_curl_easy_setopt(handle, opt, param)                         \
    CPL_IGNORE_RET_VAL(curl_easy_setopt(handle, opt, param))

namespace cpl
{

/************************************************************************/
/*                         VSIGSFSHandler                               */
/************************************************************************/

class VSIGSFSHandler final : public IVSIS3LikeFSHandlerWithMultipartUpload
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGSFSHandler)
    const std::string m_osPrefix;

  protected:
    VSICurlHandle *CreateFileHandle(const char *pszFilename) override;

    const char *GetDebugKey() const override
    {
        return "GS";
    }

    std::string GetFSPrefix() const override
    {
        return m_osPrefix;
    }

    std::string
    GetURLFromFilename(const std::string &osFilename) const override;

    IVSIS3LikeHandleHelper *CreateHandleHelper(const char *pszURI,
                                               bool bAllowNoObject) override;

    void ClearCache() override;

    bool IsAllowedHeaderForObjectCreation(const char *pszHeaderName) override
    {
        return STARTS_WITH(pszHeaderName, "x-goog-");
    }

    VSIVirtualHandleUniquePtr
    CreateWriteHandle(const char *pszFilename,
                      CSLConstList papszOptions) override;

  public:
    explicit VSIGSFSHandler(const char *pszPrefix) : m_osPrefix(pszPrefix)
    {
    }

    ~VSIGSFSHandler() override;

    const char *GetOptions() override;

    char *GetSignedURL(const char *pszFilename,
                       CSLConstList papszOptions) override;

    char **GetFileMetadata(const char *pszFilename, const char *pszDomain,
                           CSLConstList papszOptions) override;

    bool SetFileMetadata(const char *pszFilename, CSLConstList papszMetadata,
                         const char *pszDomain,
                         CSLConstList papszOptions) override;

    int *UnlinkBatch(CSLConstList papszFiles) override;
    int RmdirRecursive(const char *pszDirname) override;

    std::string
    GetStreamingFilename(const std::string &osFilename) const override;

    VSIFilesystemHandler *Duplicate(const char *pszPrefix) override
    {
        return new VSIGSFSHandler(pszPrefix);
    }

    bool SupportsMultipartAbort() const override
    {
        return true;
    }
};

/************************************************************************/
/*                            VSIGSHandle                               */
/************************************************************************/

class VSIGSHandle final : public IVSIS3LikeHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGSHandle)

    VSIGSHandleHelper *m_poHandleHelper = nullptr;

  protected:
    struct curl_slist *
    GetCurlHeaders(const std::string &osVerb,
                   const struct curl_slist *psExistingHeaders) override;

  public:
    VSIGSHandle(VSIGSFSHandler *poFS, const char *pszFilename,
                VSIGSHandleHelper *poHandleHelper);
    ~VSIGSHandle() override;
};

/************************************************************************/
/*                          ~VSIGSFSHandler()                           */
/************************************************************************/

VSIGSFSHandler::~VSIGSFSHandler()
{
    VSICurlFilesystemHandlerBase::ClearCache();
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

VSICurlHandle *VSIGSFSHandler::CreateFileHandle(const char *pszFilename)
{
    VSIGSHandleHelper *poHandleHelper = VSIGSHandleHelper::BuildFromURI(
        pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str());
    if (poHandleHelper == nullptr)
        return nullptr;
    return new VSIGSHandle(this, pszFilename, poHandleHelper);
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char *VSIGSFSHandler::GetOptions()
{
    static std::string osOptions(
        std::string("<Options>")
            .append(
                "  <Option name='GS_SECRET_ACCESS_KEY' type='string' "
                "description='Secret access key. To use with "
                "GS_ACCESS_KEY_ID'/>"
                "  <Option name='GS_ACCESS_KEY_ID' type='string' "
                "description='Access key id'/>"
                "  <Option name='GS_NO_SIGN_REQUEST' type='boolean' "
                "description='Whether to disable signing of requests' "
                "default='NO'/>"
                "  <Option name='GS_OAUTH2_REFRESH_TOKEN' type='string' "
                "description='OAuth2 refresh token. For OAuth2 client "
                "authentication. "
                "To use with GS_OAUTH2_CLIENT_ID and GS_OAUTH2_CLIENT_SECRET'/>"
                "  <Option name='GS_OAUTH2_CLIENT_ID' type='string' "
                "description='OAuth2 client id for OAuth2 client "
                "authentication'/>"
                "  <Option name='GS_OAUTH2_CLIENT_SECRET' type='string' "
                "description='OAuth2 client secret for OAuth2 client "
                "authentication'/>"
                "  <Option name='GS_OAUTH2_PRIVATE_KEY' type='string' "
                "description='Private key for OAuth2 service account "
                "authentication. "
                "To use with GS_OAUTH2_CLIENT_EMAIL'/>"
                "  <Option name='GS_OAUTH2_PRIVATE_KEY_FILE' type='string' "
                "description='Filename that contains private key for OAuth2 "
                "service "
                "account authentication. "
                "To use with GS_OAUTH2_CLIENT_EMAIL'/>"
                "  <Option name='GS_OAUTH2_CLIENT_EMAIL' type='string' "
                "description='Client email to use with OAuth2 service account "
                "authentication'/>"
                "  <Option name='GS_OAUTH2_SCOPE' type='string' "
                "description='OAuth2 authorization scope' "
                "default='https://www.googleapis.com/auth/"
                "devstorage.read_write'/>"
                "  <Option name='CPL_MACHINE_IS_GCE' type='boolean' "
                "description='Whether the current machine is a Google Compute "
                "Engine "
                "instance' default='NO'/>"
                "  <Option name='CPL_GCE_CHECK_LOCAL_FILES' type='boolean' "
                "description='Whether to check system logs to determine "
                "if current machine is a GCE instance' default='YES'/>"
                "description='Filename that contains AWS configuration' "
                "default='~/.aws/config'/>"
                "  <Option name='CPL_GS_CREDENTIALS_FILE' type='string' "
                "description='Filename that contains Google Storage "
                "credentials' "
                "default='~/.boto'/>"
                "  <Option name='VSIGS_CHUNK_SIZE' type='int' "
                "description='Size in MiB for chunks of files that are "
                "uploaded. The"
                "default value allows for files up to ")
            .append(CPLSPrintf("%d", GetDefaultPartSizeInMiB() *
                                         GetMaximumPartCount() / 1024))
            .append("GiB each' default='")
            .append(CPLSPrintf("%d", GetDefaultPartSizeInMiB()))
            .append("' min='")
            .append(CPLSPrintf("%d", GetMinimumPartSizeInMiB()))
            .append("' max='")
            .append(CPLSPrintf("%d", GetMaximumPartSizeInMiB()))
            .append("'/>")
            .append(VSICurlFilesystemHandlerBase::GetOptionsStatic())
            .append("</Options>"));
    return osOptions.c_str();
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

char *VSIGSFSHandler::GetSignedURL(const char *pszFilename,
                                   CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;

    VSIGSHandleHelper *poHandleHelper = VSIGSHandleHelper::BuildFromURI(
        pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str(), nullptr,
        papszOptions);
    if (poHandleHelper == nullptr)
    {
        return nullptr;
    }

    std::string osRet(poHandleHelper->GetSignedURL(papszOptions));

    delete poHandleHelper;
    return osRet.empty() ? nullptr : CPLStrdup(osRet.c_str());
}

/************************************************************************/
/*                          GetURLFromFilename()                         */
/************************************************************************/

std::string
VSIGSFSHandler::GetURLFromFilename(const std::string &osFilename) const
{
    const std::string osFilenameWithoutPrefix =
        osFilename.substr(GetFSPrefix().size());
    auto poHandleHelper =
        std::unique_ptr<VSIGSHandleHelper>(VSIGSHandleHelper::BuildFromURI(
            osFilenameWithoutPrefix.c_str(), GetFSPrefix().c_str()));
    if (poHandleHelper == nullptr)
        return std::string();
    return poHandleHelper->GetURL();
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper *VSIGSFSHandler::CreateHandleHelper(const char *pszURI,
                                                           bool)
{
    return VSIGSHandleHelper::BuildFromURI(pszURI, GetFSPrefix().c_str());
}

/************************************************************************/
/*                          CreateWriteHandle()                         */
/************************************************************************/

VSIVirtualHandleUniquePtr
VSIGSFSHandler::CreateWriteHandle(const char *pszFilename,
                                  CSLConstList papszOptions)
{
    auto poHandleHelper =
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), false);
    if (poHandleHelper == nullptr)
        return nullptr;
    auto poHandle = std::make_unique<VSIMultipartWriteHandle>(
        this, pszFilename, poHandleHelper, papszOptions);
    if (!poHandle->IsOK())
    {
        return nullptr;
    }
    return VSIVirtualHandleUniquePtr(poHandle.release());
}

/************************************************************************/
/*                          GetFileMetadata()                           */
/************************************************************************/

char **VSIGSFSHandler::GetFileMetadata(const char *pszFilename,
                                       const char *pszDomain,
                                       CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;

    if (pszDomain == nullptr || !EQUAL(pszDomain, "ACL"))
    {
        return VSICurlFilesystemHandlerBase::GetFileMetadata(
            pszFilename, pszDomain, papszOptions);
    }

    auto poHandleHelper =
        std::unique_ptr<IVSIS3LikeHandleHelper>(VSIGSHandleHelper::BuildFromURI(
            pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str()));
    if (!poHandleHelper)
        return nullptr;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("GetFileMetadata");

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);
    CPLHTTPRetryContext oRetryContext(oRetryParameters);

    bool bRetry;
    CPLStringList aosResult;
    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        poHandleHelper->AddQueryParameter("acl", "");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poHandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = VSICurlMergeHeaders(
            headers, poHandleHelper->GetCurlHeaders("GET", headers));

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poHandleHelper.get());

        NetworkStatisticsLogger::LogGET(requestHelper.sWriteFuncData.nSize);

        if (response_code != 200 ||
            requestHelper.sWriteFuncData.pBuffer == nullptr)
        {
            // Look if we should attempt a retry
            if (oRetryContext.CanRetry(
                    static_cast<int>(response_code),
                    requestHelper.sWriteFuncHeaderData.pBuffer,
                    requestHelper.szCurlErrBuf))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "HTTP error code: %d - %s. "
                         "Retrying again in %.1f secs",
                         static_cast<int>(response_code),
                         poHandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "GetFileMetadata failed");
            }
        }
        else
        {
            aosResult.SetNameValue("XML", requestHelper.sWriteFuncData.pBuffer);
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);
    return CSLDuplicate(aosResult.List());
}

/************************************************************************/
/*                          SetFileMetadata()                           */
/************************************************************************/

bool VSIGSFSHandler::SetFileMetadata(const char *pszFilename,
                                     CSLConstList papszMetadata,
                                     const char *pszDomain,
                                     CSLConstList /* papszOptions */)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return false;

    if (pszDomain == nullptr ||
        !(EQUAL(pszDomain, "HEADERS") || EQUAL(pszDomain, "ACL")))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only HEADERS and ACL domain are supported");
        return false;
    }

    if (EQUAL(pszDomain, "HEADERS"))
    {
        return CopyObject(pszFilename, pszFilename, papszMetadata) == 0;
    }

    const char *pszXML = CSLFetchNameValue(papszMetadata, "XML");
    if (pszXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "XML key is missing in metadata");
        return false;
    }

    auto poHandleHelper =
        std::unique_ptr<IVSIS3LikeHandleHelper>(VSIGSHandleHelper::BuildFromURI(
            pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str()));
    if (!poHandleHelper)
        return false;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("SetFileMetadata");

    bool bRetry;
    bool bRet = false;

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);
    CPLHTTPRetryContext oRetryContext(oRetryParameters);

    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        poHandleHelper->AddQueryParameter("acl", "");
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT");
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS, pszXML);

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poHandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = curl_slist_append(headers, "Content-Type: application/xml");
        headers = VSICurlMergeHeaders(
            headers, poHandleHelper->GetCurlHeaders("PUT", headers, pszXML,
                                                    strlen(pszXML)));
        NetworkStatisticsLogger::LogPUT(strlen(pszXML));

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poHandleHelper.get());

        if (response_code != 200)
        {
            // Look if we should attempt a retry
            if (oRetryContext.CanRetry(
                    static_cast<int>(response_code),
                    requestHelper.sWriteFuncHeaderData.pBuffer,
                    requestHelper.szCurlErrBuf))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "HTTP error code: %d - %s. "
                         "Retrying again in %.1f secs",
                         static_cast<int>(response_code),
                         poHandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "SetFileMetadata failed");
            }
        }
        else
        {
            bRet = true;
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);
    return bRet;
}

/************************************************************************/
/*                           UnlinkBatch()                              */
/************************************************************************/

int *VSIGSFSHandler::UnlinkBatch(CSLConstList papszFiles)
{
    // Implemented using
    // https://cloud.google.com/storage/docs/json_api/v1/how-tos/batch

    const char *pszFirstFilename =
        papszFiles && papszFiles[0] ? papszFiles[0] : nullptr;

    auto poHandleHelper =
        std::unique_ptr<VSIGSHandleHelper>(VSIGSHandleHelper::BuildFromURI(
            "batch/storage/v1", GetFSPrefix().c_str(),
            pszFirstFilename &&
                    STARTS_WITH(pszFirstFilename, GetFSPrefix().c_str())
                ? pszFirstFilename + GetFSPrefix().size()
                : nullptr));

    // The JSON API cannot be used with HMAC keys
    if (poHandleHelper && poHandleHelper->UsesHMACKey())
    {
        CPLDebug(GetDebugKey(), "UnlinkBatch() has an efficient implementation "
                                "only for OAuth2 authentication");
        return VSICurlFilesystemHandlerBase::UnlinkBatch(papszFiles);
    }

    int *panRet =
        static_cast<int *>(CPLCalloc(sizeof(int), CSLCount(papszFiles)));

    if (!poHandleHelper || pszFirstFilename == nullptr)
        return panRet;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("UnlinkBatch");

    // For debug / testing only
    const int nBatchSize =
        std::max(1, std::min(100, atoi(CPLGetConfigOption(
                                      "CPL_VSIGS_UNLINK_BATCH_SIZE", "100"))));
    std::string osPOSTContent;

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(pszFirstFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);
    CPLHTTPRetryContext oRetryContext(oRetryParameters);

    for (int i = 0; papszFiles && papszFiles[i]; i++)
    {
        CPLAssert(STARTS_WITH_CI(papszFiles[i], GetFSPrefix().c_str()));
        const char *pszFilenameWithoutPrefix =
            papszFiles[i] + GetFSPrefix().size();
        const char *pszSlash = strchr(pszFilenameWithoutPrefix, '/');
        if (!pszSlash)
            return panRet;
        std::string osBucket;
        osBucket.assign(pszFilenameWithoutPrefix,
                        pszSlash - pszFilenameWithoutPrefix);

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
            CURL *hCurlHandle = curl_easy_init();
            struct curl_slist *subrequest_headers =
                static_cast<struct curl_slist *>(CPLHTTPSetOptions(
                    hCurlHandle, poTmpHandleHelper->GetURL().c_str(),
                    aosHTTPOptions.List()));
            subrequest_headers = poTmpHandleHelper->GetCurlHeaders(
                "DELETE", subrequest_headers, nullptr, 0);
            for (struct curl_slist *iter = subrequest_headers; iter;
                 iter = iter->next)
            {
                if (STARTS_WITH_CI(iter->data, "Authorization: "))
                {
                    osAuthorization = iter->data;
                }
                else if (STARTS_WITH_CI(iter->data, "Date: "))
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
        osPOSTContent += CPLSPrintf("Content-ID: <%d>\r\n", i + 1);
        osPOSTContent += "\r\n\r\n";
        osPOSTContent += "DELETE /";
        osPOSTContent += osResource;
        osPOSTContent += " HTTP/1.1\r\n";
#ifdef ADD_AUTH_TO_NESTED_REQUEST
        if (!osAuthorization.empty())
        {
            osPOSTContent += osAuthorization;
            osPOSTContent += "\r\n";
        }
        if (!osDate.empty())
        {
            osPOSTContent += osDate;
            osPOSTContent += "\r\n";
        }
#endif
        osPOSTContent += "\r\n\r\n";

        if (((i + 1) % nBatchSize) == 0 || papszFiles[i + 1] == nullptr)
        {
            osPOSTContent += "--===============7330845974216740156==--\r\n";

#ifdef DEBUG_VERBOSE
            CPLDebug(GetDebugKey(), "%s", osPOSTContent.c_str());
#endif

            // Run request
            bool bRetry;
            std::string osResponse;
            do
            {
                bRetry = false;
                CURL *hCurlHandle = curl_easy_init();

                unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST,
                                           "POST");
                unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS,
                                           osPOSTContent.c_str());

                struct curl_slist *headers =
                    static_cast<struct curl_slist *>(CPLHTTPSetOptions(
                        hCurlHandle, poHandleHelper->GetURL().c_str(),
                        aosHTTPOptions.List()));
                headers = curl_slist_append(
                    headers,
                    "Content-Type: multipart/mixed; "
                    "boundary=\"===============7330845974216740156==\"");
                headers = VSICurlMergeHeaders(
                    headers, poHandleHelper->GetCurlHeaders(
                                 "POST", headers, osPOSTContent.c_str(),
                                 osPOSTContent.size()));

                CurlRequestHelper requestHelper;
                const long response_code = requestHelper.perform(
                    hCurlHandle, headers, this, poHandleHelper.get());

                NetworkStatisticsLogger::LogPOST(
                    osPOSTContent.size(), requestHelper.sWriteFuncData.nSize);

                if (response_code != 200 ||
                    requestHelper.sWriteFuncData.pBuffer == nullptr)
                {
                    // Look if we should attempt a retry
                    if (oRetryContext.CanRetry(
                            static_cast<int>(response_code),
                            requestHelper.sWriteFuncHeaderData.pBuffer,
                            requestHelper.szCurlErrBuf))
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "HTTP error code: %d - %s. "
                                 "Retrying again in %.1f secs",
                                 static_cast<int>(response_code),
                                 poHandleHelper->GetURL().c_str(),
                                 oRetryContext.GetCurrentDelay());
                        CPLSleep(oRetryContext.GetCurrentDelay());
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
                    CPLDebug(GetDebugKey(), "%s",
                             requestHelper.sWriteFuncData.pBuffer);
#endif
                    osResponse = requestHelper.sWriteFuncData.pBuffer;
                }

                curl_easy_cleanup(hCurlHandle);
            } while (bRetry);

            // Mark deleted files
            for (int j = i + 1 - nBatchSize; j <= i; j++)
            {
                auto nPos = osResponse.find(
                    CPLSPrintf("Content-ID: <response-%d>", j + 1));
                if (nPos != std::string::npos)
                {
                    nPos = osResponse.find("HTTP/1.1 ", nPos);
                    if (nPos != std::string::npos)
                    {
                        const char *pszHTTPCode =
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

int VSIGSFSHandler::RmdirRecursive(const char *pszDirname)
{
    // For debug / testing only
    const int nBatchSize = std::min(
        100, atoi(CPLGetConfigOption("CPL_VSIGS_UNLINK_BATCH_SIZE", "100")));

    return RmdirRecursiveInternal(pszDirname, nBatchSize);
}

/************************************************************************/
/*                      GetStreamingFilename()                          */
/************************************************************************/

std::string
VSIGSFSHandler::GetStreamingFilename(const std::string &osFilename) const
{
    if (STARTS_WITH(osFilename.c_str(), GetFSPrefix().c_str()))
        return "/vsigs_streaming/" + osFilename.substr(GetFSPrefix().size());
    return osFilename;
}

/************************************************************************/
/*                             VSIGSHandle()                            */
/************************************************************************/

VSIGSHandle::VSIGSHandle(VSIGSFSHandler *poFSIn, const char *pszFilename,
                         VSIGSHandleHelper *poHandleHelper)
    : IVSIS3LikeHandle(poFSIn, pszFilename, poHandleHelper->GetURL().c_str()),
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

struct curl_slist *
VSIGSHandle::GetCurlHeaders(const std::string &osVerb,
                            const struct curl_slist *psExistingHeaders)
{
    return m_poHandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

} /* end of namespace cpl */

#endif  // DOXYGEN_SKIP
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

void VSIInstallGSFileHandler(void)
{
    VSIFileManager::InstallHandler("/vsigs/",
                                   new cpl::VSIGSFSHandler("/vsigs/"));
}

#endif /* HAVE_CURL */
