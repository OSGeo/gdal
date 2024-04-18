/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for OpenStack Swift
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017-2018, Even Rouault <even.rouault at spatialys.com>
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

#include "cpl_json.h"
#include "cpl_port.h"
#include "cpl_http.h"
#include "cpl_time.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_vsil_curl_class.h"

#include <errno.h>

#include <algorithm>
#include <set>
#include <map>
#include <memory>

#include "cpl_swift.h"

#ifndef HAVE_CURL

void VSIInstallSwiftFileHandler(void)
{
    // Not supported
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
/*                       AnalyseSwiftFileList()                         */
/************************************************************************/

void VSICurlFilesystemHandlerBase::AnalyseSwiftFileList(
    const std::string &osBaseURL, const std::string &osPrefix,
    const char *pszJson, CPLStringList &osFileList, int nMaxFilesThisQuery,
    int nMaxFiles, bool &bIsTruncated, std::string &osNextMarker)
{
#if DEBUG_VERBOSE
    CPLDebug("SWIFT", "%s", pszJson);
#endif
    osNextMarker = "";
    bIsTruncated = false;

    CPLJSONDocument oDoc;
    if (!oDoc.LoadMemory(reinterpret_cast<const GByte *>(pszJson)))
        return;

    std::vector<std::pair<std::string, FileProp>> aoProps;
    // Count the number of occurrences of a path. Can be 1 or 2. 2 in the case
    // that both a filename and directory exist
    std::map<std::string, int> aoNameCount;

    CPLJSONArray oArray = oDoc.GetRoot().ToArray();
    for (int i = 0; i < oArray.Size(); i++)
    {
        CPLJSONObject oItem = oArray[i];
        std::string osName = oItem.GetString("name");
        GInt64 nSize = oItem.GetLong("bytes");
        std::string osLastModified = oItem.GetString("last_modified");
        std::string osSubdir = oItem.GetString("subdir");
        bool bHasCount = oItem.GetLong("count", -1) >= 0;
        if (!osName.empty())
        {
            osNextMarker = osName;
            if (osName.size() > osPrefix.size() &&
                osName.substr(0, osPrefix.size()) == osPrefix)
            {
                if (bHasCount)
                {
                    // Case when listing /vsiswift/
                    FileProp prop;
                    prop.eExists = EXIST_YES;
                    prop.bIsDirectory = true;
                    prop.bHasComputedFileSize = true;
                    prop.fileSize = 0;
                    prop.mTime = 0;

                    aoProps.push_back(
                        std::pair<std::string, FileProp>(osName, prop));
                    aoNameCount[osName]++;
                }
                else
                {
                    FileProp prop;
                    prop.eExists = EXIST_YES;
                    prop.bHasComputedFileSize = true;
                    prop.fileSize = static_cast<GUIntBig>(nSize);
                    prop.bIsDirectory = false;
                    prop.mTime = 0;
                    int nYear, nMonth, nDay, nHour, nMin, nSec;
                    if (sscanf(osLastModified.c_str(),
                               "%04d-%02d-%02dT%02d:%02d:%02d", &nYear, &nMonth,
                               &nDay, &nHour, &nMin, &nSec) == 6)
                    {
                        struct tm brokendowntime;
                        brokendowntime.tm_year = nYear - 1900;
                        brokendowntime.tm_mon = nMonth - 1;
                        brokendowntime.tm_mday = nDay;
                        brokendowntime.tm_hour = nHour;
                        brokendowntime.tm_min = nMin;
                        brokendowntime.tm_sec = nSec;
                        prop.mTime = static_cast<time_t>(
                            CPLYMDHMSToUnixTime(&brokendowntime));
                    }

                    aoProps.push_back(std::pair<std::string, FileProp>(
                        osName.substr(osPrefix.size()), prop));
                    aoNameCount[osName.substr(osPrefix.size())]++;
                }
            }
        }
        else if (!osSubdir.empty())
        {
            osNextMarker = osSubdir;
            if (osSubdir.back() == '/')
                osSubdir.resize(osSubdir.size() - 1);
            if (STARTS_WITH(osSubdir.c_str(), osPrefix.c_str()))
            {

                FileProp prop;
                prop.eExists = EXIST_YES;
                prop.bIsDirectory = true;
                prop.bHasComputedFileSize = true;
                prop.fileSize = 0;
                prop.mTime = 0;

                aoProps.push_back(std::pair<std::string, FileProp>(
                    osSubdir.substr(osPrefix.size()), prop));
                aoNameCount[osSubdir.substr(osPrefix.size())]++;
            }
        }

        if (nMaxFiles > 0 && aoProps.size() > static_cast<unsigned>(nMaxFiles))
            break;
    }

    bIsTruncated = aoProps.size() >= static_cast<unsigned>(nMaxFilesThisQuery);
    if (!bIsTruncated)
    {
        osNextMarker.clear();
    }

    for (size_t i = 0; i < aoProps.size(); i++)
    {
        std::string osSuffix;
        if (aoNameCount[aoProps[i].first] == 2 &&
            aoProps[i].second.bIsDirectory)
        {
            // Add a / suffix to disambiguish the situation
            // Normally we don't suffix directories with /, but we have
            // no alternative here
            osSuffix = "/";
        }
        if (nMaxFiles != 1)
        {
            std::string osCachedFilename =
                osBaseURL + "/" + CPLAWSURLEncode(osPrefix, false) +
                CPLAWSURLEncode(aoProps[i].first, false) + osSuffix;
#if DEBUG_VERBOSE
            CPLDebug("SWIFT", "Cache %s", osCachedFilename.c_str());
#endif
            SetCachedFileProp(osCachedFilename.c_str(), aoProps[i].second);
        }
        osFileList.AddString((aoProps[i].first + osSuffix).c_str());
    }
}

/************************************************************************/
/*                         VSISwiftFSHandler                            */
/************************************************************************/

class VSISwiftFSHandler final : public IVSIS3LikeFSHandler
{
    const std::string m_osPrefix;
    CPL_DISALLOW_COPY_ASSIGN(VSISwiftFSHandler)

  protected:
    VSICurlHandle *CreateFileHandle(const char *pszFilename) override;
    std::string GetURLFromFilename(const std::string &osFilename) override;

    const char *GetDebugKey() const override
    {
        return "SWIFT";
    }

    IVSIS3LikeHandleHelper *CreateHandleHelper(const char *pszURI,
                                               bool bAllowNoObject) override;

    std::string GetFSPrefix() const override
    {
        return m_osPrefix;
    }

    char **GetFileList(const char *pszFilename, int nMaxFiles,
                       bool *pbGotFileList) override;

    void ClearCache() override;

    VSIVirtualHandleUniquePtr
    CreateWriteHandle(const char *pszFilename,
                      CSLConstList papszOptions) override;

  public:
    explicit VSISwiftFSHandler(const char *pszPrefix) : m_osPrefix(pszPrefix)
    {
    }

    ~VSISwiftFSHandler() override;

    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;

    VSIDIR *OpenDir(const char *pszPath, int nRecurseDepth,
                    const char *const *papszOptions) override
    {
        return VSICurlFilesystemHandlerBase::OpenDir(pszPath, nRecurseDepth,
                                                     papszOptions);
    }

    const char *GetOptions() override;

    std::string
    GetStreamingFilename(const std::string &osFilename) const override
    {
        return osFilename;
    }

    VSIFilesystemHandler *Duplicate(const char *pszPrefix) override
    {
        return new VSISwiftFSHandler(pszPrefix);
    }
};

/************************************************************************/
/*                            VSISwiftHandle                              */
/************************************************************************/

class VSISwiftHandle final : public IVSIS3LikeHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSISwiftHandle)

    VSISwiftHandleHelper *m_poHandleHelper = nullptr;

  protected:
    struct curl_slist *
    GetCurlHeaders(const std::string &osVerb,
                   const struct curl_slist *psExistingHeaders) override;
    virtual bool Authenticate(const char *pszFilename) override;

  public:
    VSISwiftHandle(VSISwiftFSHandler *poFS, const char *pszFilename,
                   VSISwiftHandleHelper *poHandleHelper);
    ~VSISwiftHandle() override;
};

/************************************************************************/
/*                          CreateWriteHandle()                         */
/************************************************************************/

VSIVirtualHandleUniquePtr
VSISwiftFSHandler::CreateWriteHandle(const char *pszFilename,
                                     CSLConstList papszOptions)
{
    auto poHandleHelper =
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), false);
    if (poHandleHelper == nullptr)
        return nullptr;
    auto poHandle = std::make_unique<VSIS3WriteHandle>(
        this, pszFilename, poHandleHelper, true, papszOptions);
    if (!poHandle->IsOK())
    {
        return nullptr;
    }
    return VSIVirtualHandleUniquePtr(poHandle.release());
}

/************************************************************************/
/*                       ~VSISwiftFSHandler()                           */
/************************************************************************/

VSISwiftFSHandler::~VSISwiftFSHandler()
{
    VSISwiftFSHandler::ClearCache();
    VSISwiftHandleHelper::CleanMutex();
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSISwiftFSHandler::ClearCache()
{
    VSICurlFilesystemHandlerBase::ClearCache();

    VSISwiftHandleHelper::ClearCache();
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char *VSISwiftFSHandler::GetOptions()
{
    static std::string osOptions(
        std::string("<Options>") +
        "  <Option name='SWIFT_STORAGE_URL' type='string' "
        "description='Storage URL. To use with SWIFT_AUTH_TOKEN'/>"
        "  <Option name='SWIFT_AUTH_TOKEN' type='string' "
        "description='Authorization token'/>"
        "  <Option name='SWIFT_AUTH_V1_URL' type='string' "
        "description='Authentication V1 URL. To use with SWIFT_USER and "
        "SWIFT_KEY'/>"
        "  <Option name='SWIFT_USER' type='string' "
        "description='User name to use with authentication V1'/>"
        "  <Option name='SWIFT_KEY' type='string' "
        "description='Key/password to use with authentication V1'/>"
        "  <Option name='OS_IDENTITY_API_VERSION' type='string' "
        "description='OpenStack identity API version'/>"
        "  <Option name='OS_AUTH_TYPE' type='string' "
        "description='Authentication URL'/>"
        "  <Option name='OS_USERNAME' type='string' "
        "description='User name'/>"
        "  <Option name='OS_PASSWORD' type='string' "
        "description='Password'/>"
        "  <Option name='OS_USER_DOMAIN_NAME' type='string' "
        "description='User domain name'/>"
        "  <Option name='OS_PROJECT_NAME' type='string' "
        "description='Project name'/>"
        "  <Option name='OS_PROJECT_DOMAIN_NAME' type='string' "
        "description='Project domain name'/>"
        "  <Option name='OS_REGION_NAME' type='string' "
        "description='Region name'/>" +
        VSICurlFilesystemHandlerBase::GetOptionsStatic() + "</Options>");
    return osOptions.c_str();
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle *VSISwiftFSHandler::CreateFileHandle(const char *pszFilename)
{
    VSISwiftHandleHelper *poHandleHelper = VSISwiftHandleHelper::BuildFromURI(
        pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str());
    if (poHandleHelper)
    {
        return new VSISwiftHandle(this, pszFilename, poHandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                         GetURLFromFilename()                         */
/************************************************************************/

std::string VSISwiftFSHandler::GetURLFromFilename(const std::string &osFilename)
{
    std::string osFilenameWithoutPrefix =
        osFilename.substr(GetFSPrefix().size());

    VSISwiftHandleHelper *poHandleHelper = VSISwiftHandleHelper::BuildFromURI(
        osFilenameWithoutPrefix.c_str(), GetFSPrefix().c_str());
    if (poHandleHelper == nullptr)
    {
        return "";
    }
    std::string osBaseURL(poHandleHelper->GetURL());
    if (!osBaseURL.empty() && osBaseURL.back() == '/')
        osBaseURL.resize(osBaseURL.size() - 1);
    delete poHandleHelper;

    return osBaseURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper *
VSISwiftFSHandler::CreateHandleHelper(const char *pszURI, bool)
{
    return VSISwiftHandleHelper::BuildFromURI(pszURI, GetFSPrefix().c_str());
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSISwiftFSHandler::Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
                            int nFlags)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return -1;

    if ((nFlags & VSI_STAT_CACHE_ONLY) != 0)
        return VSICurlFilesystemHandlerBase::Stat(pszFilename, pStatBuf,
                                                  nFlags);

    std::string osFilename(pszFilename);
    if (osFilename.back() == '/')
        osFilename.resize(osFilename.size() - 1);

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    if (VSICurlFilesystemHandlerBase::Stat(pszFilename, pStatBuf, nFlags) == 0)
    {
        // if querying /vsiswift/container_name, the GET will succeed and
        // we would consider this as a file whereas it should be exposed as
        // a directory
        if (std::count(osFilename.begin(), osFilename.end(), '/') <= 2)
        {

            auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
                CreateHandleHelper(pszFilename + GetFSPrefix().size(), true));
            if (poHandleHelper)
            {
                FileProp cachedFileProp;
                cachedFileProp.eExists = EXIST_YES;
                cachedFileProp.bHasComputedFileSize = false;
                cachedFileProp.fileSize = 0;
                cachedFileProp.bIsDirectory = true;
                cachedFileProp.mTime = 0;
                cachedFileProp.nMode = S_IFDIR;
                SetCachedFileProp(poHandleHelper->GetURL().c_str(),
                                  cachedFileProp);
            }

            pStatBuf->st_size = 0;
            pStatBuf->st_mode = S_IFDIR;
        }
        return 0;
    }

    // In the case of a directory, a GET on it will not work, so we have to
    // query the upper directory contents
    if (std::count(osFilename.begin(), osFilename.end(), '/') < 2)
        return -1;

    char **papszContents = VSIReadDir(CPLGetPath(osFilename.c_str()));
    int nRet = CSLFindStringCaseSensitive(
                   papszContents, CPLGetFilename(osFilename.c_str())) >= 0
                   ? 0
                   : -1;
    CSLDestroy(papszContents);

    FileProp cachedFileProp;
    if (nRet == 0)
    {
        pStatBuf->st_mode = S_IFDIR;

        cachedFileProp.eExists = EXIST_YES;
        cachedFileProp.bHasComputedFileSize = false;
        cachedFileProp.fileSize = 0;
        cachedFileProp.bIsDirectory = true;
        cachedFileProp.mTime = 0;
        cachedFileProp.nMode = S_IFDIR;
    }
    else
    {
        cachedFileProp.eExists = EXIST_NO;
    }

    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), true));
    if (poHandleHelper)
    {
        SetCachedFileProp(poHandleHelper->GetURL().c_str(), cachedFileProp);
    }

    return nRet;
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char **VSISwiftFSHandler::GetFileList(const char *pszDirname, int nMaxFiles,
                                      bool *pbGotFileList)
{
    if (ENABLE_DEBUG)
        CPLDebug(GetDebugKey(), "GetFileList(%s)", pszDirname);
    *pbGotFileList = false;
    CPLAssert(strlen(pszDirname) >= GetFSPrefix().size());
    std::string osDirnameWithoutPrefix = pszDirname + GetFSPrefix().size();
    if (!osDirnameWithoutPrefix.empty() && osDirnameWithoutPrefix.back() == '/')
    {
        osDirnameWithoutPrefix.resize(osDirnameWithoutPrefix.size() - 1);
    }

    std::string osBucket(osDirnameWithoutPrefix);
    std::string osObjectKey;
    size_t nSlashPos = osDirnameWithoutPrefix.find('/');
    if (nSlashPos != std::string::npos)
    {
        osBucket = osDirnameWithoutPrefix.substr(0, nSlashPos);
        osObjectKey = osDirnameWithoutPrefix.substr(nSlashPos + 1);
    }

    IVSIS3LikeHandleHelper *poS3HandleHelper =
        CreateHandleHelper(osBucket.c_str(), true);
    if (poS3HandleHelper == nullptr)
    {
        return nullptr;
    }

    WriteFuncStruct sWriteFuncData;

    CPLStringList osFileList;  // must be left in this scope !
    std::string osNextMarker;  // must be left in this scope !

    std::string osMaxKeys = CPLGetConfigOption("SWIFT_MAX_KEYS", "10000");
    int nMaxFilesThisQuery = atoi(osMaxKeys.c_str());
    if (nMaxFiles > 0 && nMaxFiles <= 100 && nMaxFiles < nMaxFilesThisQuery)
    {
        nMaxFilesThisQuery = nMaxFiles + 1;
    }
    const std::string osPrefix(osObjectKey.empty() ? std::string()
                                                   : osObjectKey + "/");

    while (true)
    {
        bool bRetry;
        int nRetryCount = 0;
        const int nMaxRetry = atoi(CPLGetConfigOption(
            "GDAL_HTTP_MAX_RETRY", CPLSPrintf("%d", CPL_HTTP_MAX_RETRY)));
        // coverity[tainted_data]
        double dfRetryDelay = CPLAtof(CPLGetConfigOption(
            "GDAL_HTTP_RETRY_DELAY", CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
        do
        {
            bRetry = false;
            poS3HandleHelper->ResetQueryParameters();
            std::string osBaseURL(poS3HandleHelper->GetURL());

            CURLM *hCurlMultiHandle = GetCurlMultiHandleFor(osBaseURL);
            CURL *hCurlHandle = curl_easy_init();

            if (!osBucket.empty())
            {
                poS3HandleHelper->AddQueryParameter("delimiter", "/");
                if (!osNextMarker.empty())
                    poS3HandleHelper->AddQueryParameter("marker", osNextMarker);
                poS3HandleHelper->AddQueryParameter(
                    "limit", CPLSPrintf("%d", nMaxFilesThisQuery));
                if (!osPrefix.empty())
                    poS3HandleHelper->AddQueryParameter("prefix", osPrefix);
            }

            struct curl_slist *headers = VSICurlSetOptions(
                hCurlHandle, poS3HandleHelper->GetURL().c_str(), nullptr);
            // Disable automatic redirection
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_FOLLOWLOCATION, 0);

            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, nullptr);

            VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr,
                                       nullptr);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA,
                                       &sWriteFuncData);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                                       VSICurlHandleWriteFunc);

            WriteFuncStruct sWriteFuncHeaderData;
            VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr,
                                       nullptr);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA,
                                       &sWriteFuncHeaderData);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                                       VSICurlHandleWriteFunc);

            char szCurlErrBuf[CURL_ERROR_SIZE + 1] = {};
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER,
                                       szCurlErrBuf);

            headers = VSICurlMergeHeaders(
                headers, poS3HandleHelper->GetCurlHeaders("GET", headers));
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER,
                                       headers);

            VSICURLMultiPerform(hCurlMultiHandle, hCurlHandle);

            VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

            if (headers != nullptr)
                curl_slist_free_all(headers);

            if (sWriteFuncData.pBuffer == nullptr)
            {
                delete poS3HandleHelper;
                curl_easy_cleanup(hCurlHandle);
                CPLFree(sWriteFuncHeaderData.pBuffer);
                return nullptr;
            }

            long response_code = 0;
            curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
            if (response_code != 200)
            {
                // Look if we should attempt a retry
                const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                    static_cast<int>(response_code), dfRetryDelay,
                    sWriteFuncHeaderData.pBuffer, szCurlErrBuf);
                if (dfNewRetryDelay > 0 && nRetryCount < nMaxRetry)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "HTTP error code: %d - %s. "
                             "Retrying again in %.1f secs",
                             static_cast<int>(response_code),
                             poS3HandleHelper->GetURL().c_str(), dfRetryDelay);
                    CPLSleep(dfRetryDelay);
                    dfRetryDelay = dfNewRetryDelay;
                    nRetryCount++;
                    bRetry = true;
                    CPLFree(sWriteFuncData.pBuffer);
                    CPLFree(sWriteFuncHeaderData.pBuffer);
                }
                else
                {
                    CPLDebug(GetDebugKey(), "%s", sWriteFuncData.pBuffer);
                    CPLFree(sWriteFuncData.pBuffer);
                    CPLFree(sWriteFuncHeaderData.pBuffer);
                    delete poS3HandleHelper;
                    curl_easy_cleanup(hCurlHandle);
                    return nullptr;
                }
            }
            else
            {
                *pbGotFileList = true;
                bool bIsTruncated;
                AnalyseSwiftFileList(
                    osBaseURL, osPrefix, sWriteFuncData.pBuffer, osFileList,
                    nMaxFilesThisQuery, nMaxFiles, bIsTruncated, osNextMarker);

                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);

                if (osNextMarker.empty())
                {
                    delete poS3HandleHelper;
                    curl_easy_cleanup(hCurlHandle);
                    return osFileList.StealList();
                }
            }

            curl_easy_cleanup(hCurlHandle);
        } while (bRetry);
    }
}

/************************************************************************/
/*                            VSISwiftHandle()                            */
/************************************************************************/

VSISwiftHandle::VSISwiftHandle(VSISwiftFSHandler *poFSIn,
                               const char *pszFilename,
                               VSISwiftHandleHelper *poHandleHelper)
    : IVSIS3LikeHandle(poFSIn, pszFilename, poHandleHelper->GetURL().c_str()),
      m_poHandleHelper(poHandleHelper)
{
}

/************************************************************************/
/*                            ~VSISwiftHandle()                           */
/************************************************************************/

VSISwiftHandle::~VSISwiftHandle()
{
    delete m_poHandleHelper;
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSISwiftHandle::GetCurlHeaders(const std::string &osVerb,
                               const struct curl_slist *psExistingHeaders)
{
    return m_poHandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                           Authenticate()                             */
/************************************************************************/

bool VSISwiftHandle::Authenticate(const char *pszFilename)
{
    return m_poHandleHelper->Authenticate(pszFilename);
}

} /* end of namespace cpl */

#endif  // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                     VSIInstallSwiftFileHandler()                     */
/************************************************************************/

/*!
 \brief Install /vsiswift/ OpenStack Swif Object Storage (Swift) file
 system handler (requires libcurl)

 \verbatim embed:rst
 See :ref:`/vsiswift/ documentation <vsiswift>`
 \endverbatim

 @since GDAL 2.3
 */
void VSIInstallSwiftFileHandler(void)
{
    VSIFileManager::InstallHandler("/vsiswift/",
                                   new cpl::VSISwiftFSHandler("/vsiswift/"));
}

#endif /* HAVE_CURL */
