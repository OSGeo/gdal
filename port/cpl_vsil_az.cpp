/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Microsoft Azure Blob Storage
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

#include "cpl_port.h"
#include "cpl_http.h"
#include "cpl_minixml.h"
#include "cpl_time.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_vsil_curl_class.h"

#include <errno.h>

#include <algorithm>
#include <set>
#include <map>
#include <memory>

#include "cpl_azure.h"

// #define DEBUG_VERBOSE 1

#ifndef HAVE_CURL

void VSIInstallAzureFileHandler(void)
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

const char GDAL_MARKER_FOR_DIR[] = ".gdal_marker_for_dir";

/************************************************************************/
/*                             VSIDIRAz                                 */
/************************************************************************/

struct VSIDIRAz : public VSIDIRWithMissingDirSynthesis
{
    int nRecurseDepth = 0;

    std::string osNextMarker{};
    int nPos = 0;

    std::string osBucket{};
    std::string osObjectKey{};
    IVSIS3LikeFSHandler *poFS = nullptr;
    std::unique_ptr<IVSIS3LikeHandleHelper> poHandleHelper{};
    int nMaxFiles = 0;
    bool bCacheEntries = true;
    bool m_bSynthetizeMissingDirectories = false;
    std::string m_osFilterPrefix{};

    explicit VSIDIRAz(IVSIS3LikeFSHandler *poFSIn) : poFS(poFSIn)
    {
    }

    VSIDIRAz(const VSIDIRAz &) = delete;
    VSIDIRAz &operator=(const VSIDIRAz &) = delete;

    const VSIDIREntry *NextDirEntry() override;

    bool IssueListDir();
    bool AnalyseAzureFileList(const std::string &osBaseURL, const char *pszXML);
    void clear();
};

/************************************************************************/
/*                                clear()                               */
/************************************************************************/

void VSIDIRAz::clear()
{
    osNextMarker.clear();
    nPos = 0;
    aoEntries.clear();
}

/************************************************************************/
/*                        AnalyseAzureFileList()                        */
/************************************************************************/

bool VSIDIRAz::AnalyseAzureFileList(const std::string &osBaseURL,
                                    const char *pszXML)
{
#if DEBUG_VERBOSE
    CPLDebug("AZURE", "%s", pszXML);
#endif

    CPLXMLNode *psTree = CPLParseXMLString(pszXML);
    if (psTree == nullptr)
        return false;
    CPLXMLNode *psEnumerationResults =
        CPLGetXMLNode(psTree, "=EnumerationResults");

    bool bOK = false;
    if (psEnumerationResults)
    {
        CPLString osPrefix = CPLGetXMLValue(psEnumerationResults, "Prefix", "");
        if (osPrefix.empty())
        {
            // in the case of an empty bucket
            bOK = true;
        }
        else if (osPrefix.endsWith(m_osFilterPrefix))
        {
            osPrefix.resize(osPrefix.size() - m_osFilterPrefix.size());
        }

        CPLXMLNode *psBlobs = CPLGetXMLNode(psEnumerationResults, "Blobs");
        if (psBlobs == nullptr)
        {
            psBlobs = CPLGetXMLNode(psEnumerationResults, "Containers");
            if (psBlobs != nullptr)
                bOK = true;
        }

        std::string GDAL_MARKER_FOR_DIR_WITH_LEADING_SLASH("/");
        GDAL_MARKER_FOR_DIR_WITH_LEADING_SLASH += GDAL_MARKER_FOR_DIR;

        // Count the number of occurrences of a path. Can be 1 or 2. 2 in the
        // case that both a filename and directory exist
        std::map<std::string, int> aoNameCount;
        for (CPLXMLNode *psIter = psBlobs ? psBlobs->psChild : nullptr;
             psIter != nullptr; psIter = psIter->psNext)
        {
            if (psIter->eType != CXT_Element)
                continue;
            if (strcmp(psIter->pszValue, "Blob") == 0)
            {
                const char *pszKey = CPLGetXMLValue(psIter, "Name", nullptr);
                if (pszKey && strstr(pszKey, GDAL_MARKER_FOR_DIR) != nullptr)
                {
                    bOK = true;
                    if (nRecurseDepth < 0)
                    {
                        if (strcmp(pszKey + osPrefix.size(),
                                   GDAL_MARKER_FOR_DIR) == 0)
                            continue;
                        char *pszName = CPLStrdup(pszKey + osPrefix.size());
                        char *pszMarker = strstr(
                            pszName,
                            GDAL_MARKER_FOR_DIR_WITH_LEADING_SLASH.c_str());
                        if (pszMarker)
                            *pszMarker = '\0';
                        aoNameCount[pszName]++;
                        CPLFree(pszName);
                    }
                }
                else if (pszKey && strlen(pszKey) > osPrefix.size())
                {
                    bOK = true;
                    aoNameCount[pszKey + osPrefix.size()]++;
                }
            }
            else if (strcmp(psIter->pszValue, "BlobPrefix") == 0 ||
                     strcmp(psIter->pszValue, "Container") == 0)
            {
                bOK = true;

                const char *pszKey = CPLGetXMLValue(psIter, "Name", nullptr);
                if (pszKey &&
                    strncmp(pszKey, osPrefix.c_str(), osPrefix.size()) == 0)
                {
                    std::string osKey = pszKey;
                    if (!osKey.empty() && osKey.back() == '/')
                        osKey.resize(osKey.size() - 1);
                    if (osKey.size() > osPrefix.size())
                    {
                        aoNameCount[osKey.c_str() + osPrefix.size()]++;
                    }
                }
            }
        }

        for (CPLXMLNode *psIter = psBlobs ? psBlobs->psChild : nullptr;
             psIter != nullptr; psIter = psIter->psNext)
        {
            if (psIter->eType != CXT_Element)
                continue;
            if (strcmp(psIter->pszValue, "Blob") == 0)
            {
                const char *pszKey = CPLGetXMLValue(psIter, "Name", nullptr);
                if (pszKey && strstr(pszKey, GDAL_MARKER_FOR_DIR) != nullptr)
                {
                    if (nRecurseDepth < 0)
                    {
                        if (strcmp(pszKey + osPrefix.size(),
                                   GDAL_MARKER_FOR_DIR) == 0)
                            continue;
                        aoEntries.push_back(
                            std::unique_ptr<VSIDIREntry>(new VSIDIREntry()));
                        auto &entry = aoEntries.back();
                        entry->pszName = CPLStrdup(pszKey + osPrefix.size());
                        char *pszMarker = strstr(
                            entry->pszName,
                            GDAL_MARKER_FOR_DIR_WITH_LEADING_SLASH.c_str());
                        if (pszMarker)
                            *pszMarker = '\0';
                        entry->nMode = S_IFDIR;
                        entry->bModeKnown = true;
                    }
                }
                else if (pszKey && strlen(pszKey) > osPrefix.size())
                {
                    const std::string osKeySuffix = pszKey + osPrefix.size();
                    if (m_bSynthetizeMissingDirectories)
                    {
                        const auto nLastSlashPos = osKeySuffix.rfind('/');
                        if (nLastSlashPos != std::string::npos &&
                            (m_aosSubpathsStack.empty() ||
                             osKeySuffix.compare(0, nLastSlashPos,
                                                 m_aosSubpathsStack.back()) !=
                                 0))
                        {
                            const bool bAddEntryForThisSubdir =
                                nLastSlashPos != osKeySuffix.size() - 1;
                            SynthetizeMissingDirectories(
                                osKeySuffix.substr(0, nLastSlashPos),
                                bAddEntryForThisSubdir);
                        }
                    }

                    aoEntries.push_back(
                        std::unique_ptr<VSIDIREntry>(new VSIDIREntry()));
                    auto &entry = aoEntries.back();
                    entry->pszName = CPLStrdup(osKeySuffix.c_str());
                    entry->nSize =
                        static_cast<GUIntBig>(CPLAtoGIntBig(CPLGetXMLValue(
                            psIter, "Properties.Content-Length", "0")));
                    entry->bSizeKnown = true;
                    entry->nMode = S_IFREG;
                    entry->bModeKnown = true;

                    std::string ETag = CPLGetXMLValue(psIter, "Etag", "");
                    if (!ETag.empty())
                    {
                        entry->papszExtra = CSLSetNameValue(
                            entry->papszExtra, "ETag", ETag.c_str());
                    }

                    int nYear, nMonth, nDay, nHour, nMinute, nSecond;
                    if (CPLParseRFC822DateTime(
                            CPLGetXMLValue(psIter, "Properties.Last-Modified",
                                           ""),
                            &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond,
                            nullptr, nullptr))
                    {
                        struct tm brokendowntime;
                        brokendowntime.tm_year = nYear - 1900;
                        brokendowntime.tm_mon = nMonth - 1;
                        brokendowntime.tm_mday = nDay;
                        brokendowntime.tm_hour = nHour;
                        brokendowntime.tm_min = nMinute;
                        brokendowntime.tm_sec = nSecond < 0 ? 0 : nSecond;
                        entry->nMTime = CPLYMDHMSToUnixTime(&brokendowntime);
                        entry->bMTimeKnown = true;
                    }

                    if (bCacheEntries)
                    {
                        FileProp prop;
                        prop.eExists = EXIST_YES;
                        prop.bHasComputedFileSize = true;
                        prop.fileSize = entry->nSize;
                        prop.bIsDirectory = false;
                        prop.mTime = static_cast<time_t>(entry->nMTime);
                        prop.ETag = std::move(ETag);
                        prop.nMode = entry->nMode;

                        std::string osCachedFilename =
                            osBaseURL + "/" + CPLAWSURLEncode(osPrefix, false) +
                            CPLAWSURLEncode(entry->pszName, false);
#if DEBUG_VERBOSE
                        CPLDebug("AZURE", "Cache %s", osCachedFilename.c_str());
#endif
                        poFS->SetCachedFileProp(osCachedFilename.c_str(), prop);
                    }
                }
            }
            else if (strcmp(psIter->pszValue, "BlobPrefix") == 0 ||
                     strcmp(psIter->pszValue, "Container") == 0)
            {
                const char *pszKey = CPLGetXMLValue(psIter, "Name", nullptr);
                if (pszKey &&
                    strncmp(pszKey, osPrefix.c_str(), osPrefix.size()) == 0)
                {
                    std::string osKey = pszKey;
                    if (!osKey.empty() && osKey.back() == '/')
                        osKey.resize(osKey.size() - 1);
                    if (osKey.size() > osPrefix.size())
                    {
                        aoEntries.push_back(
                            std::unique_ptr<VSIDIREntry>(new VSIDIREntry()));
                        auto &entry = aoEntries.back();
                        entry->pszName =
                            CPLStrdup(osKey.c_str() + osPrefix.size());
                        if (aoNameCount[entry->pszName] == 2)
                        {
                            // Add a / suffix to disambiguish the situation
                            // Normally we don't suffix directories with /, but
                            // we have no alternative here
                            std::string osTemp(entry->pszName);
                            osTemp += '/';
                            CPLFree(entry->pszName);
                            entry->pszName = CPLStrdup(osTemp.c_str());
                        }
                        entry->nMode = S_IFDIR;
                        entry->bModeKnown = true;

                        if (bCacheEntries)
                        {
                            FileProp prop;
                            prop.eExists = EXIST_YES;
                            prop.bIsDirectory = true;
                            prop.bHasComputedFileSize = true;
                            prop.fileSize = 0;
                            prop.mTime = 0;
                            prop.nMode = entry->nMode;

                            std::string osCachedFilename =
                                osBaseURL + "/" +
                                CPLAWSURLEncode(osPrefix, false) +
                                CPLAWSURLEncode(entry->pszName, false);
#if DEBUG_VERBOSE
                            CPLDebug("AZURE", "Cache %s",
                                     osCachedFilename.c_str());
#endif
                            poFS->SetCachedFileProp(osCachedFilename.c_str(),
                                                    prop);
                        }
                    }
                }
            }

            if (nMaxFiles > 0 &&
                aoEntries.size() > static_cast<unsigned>(nMaxFiles))
                break;
        }

        osNextMarker = CPLGetXMLValue(psEnumerationResults, "NextMarker", "");
    }
    CPLDestroyXMLNode(psTree);

    return bOK;
}

/************************************************************************/
/*                          IssueListDir()                              */
/************************************************************************/

bool VSIDIRAz::IssueListDir()
{
    WriteFuncStruct sWriteFuncData;
    const std::string l_osNextMarker(osNextMarker);
    clear();

    NetworkStatisticsFileSystem oContextFS("/vsiaz/");
    NetworkStatisticsAction oContextAction("ListBucket");

    CPLString osMaxKeys = CPLGetConfigOption("AZURE_MAX_RESULTS", "");
    const int AZURE_SERVER_LIMIT_SINGLE_REQUEST = 5000;
    if (nMaxFiles > 0 && nMaxFiles < AZURE_SERVER_LIMIT_SINGLE_REQUEST &&
        (osMaxKeys.empty() || nMaxFiles < atoi(osMaxKeys.c_str())))
    {
        osMaxKeys.Printf("%d", nMaxFiles);
    }

    poHandleHelper->ResetQueryParameters();
    std::string osBaseURL(poHandleHelper->GetURLNoKVP());
    if (osBaseURL.back() == '/')
        osBaseURL.pop_back();

    CURL *hCurlHandle = curl_easy_init();

    poHandleHelper->AddQueryParameter("comp", "list");
    if (!l_osNextMarker.empty())
        poHandleHelper->AddQueryParameter("marker", l_osNextMarker);
    if (!osMaxKeys.empty())
        poHandleHelper->AddQueryParameter("maxresults", osMaxKeys);

    if (!osBucket.empty())
    {
        poHandleHelper->AddQueryParameter("restype", "container");

        if (nRecurseDepth == 0)
            poHandleHelper->AddQueryParameter("delimiter", "/");
        if (!osObjectKey.empty())
            poHandleHelper->AddQueryParameter("prefix", osObjectKey + "/" +
                                                            m_osFilterPrefix);
        else if (!m_osFilterPrefix.empty())
            poHandleHelper->AddQueryParameter("prefix", m_osFilterPrefix);
    }

    std::string osFilename("/vsiaz/");
    if (!osBucket.empty())
    {
        osFilename += osBucket;
        if (!osObjectKey.empty())
            osFilename += osObjectKey;
    }
    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osFilename.c_str()));

    struct curl_slist *headers = VSICurlSetOptions(
        hCurlHandle, poHandleHelper->GetURL().c_str(), aosHTTPOptions.List());

    headers = VSICurlMergeHeaders(
        headers, poHandleHelper->GetCurlHeaders("GET", headers));
    unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    CurlRequestHelper requestHelper;
    const long response_code =
        requestHelper.perform(hCurlHandle, headers, poFS, poHandleHelper.get());

    NetworkStatisticsLogger::LogGET(sWriteFuncData.nSize);

    if (requestHelper.sWriteFuncData.pBuffer == nullptr)
    {
        curl_easy_cleanup(hCurlHandle);
        return false;
    }

    bool ret = false;
    if (response_code != 200)
    {
        CPLDebug("AZURE", "%s", requestHelper.sWriteFuncData.pBuffer);
    }
    else
    {
        ret = AnalyseAzureFileList(osBaseURL,
                                   requestHelper.sWriteFuncData.pBuffer);
    }
    curl_easy_cleanup(hCurlHandle);
    return ret;
}

/************************************************************************/
/*                           NextDirEntry()                             */
/************************************************************************/

const VSIDIREntry *VSIDIRAz::NextDirEntry()
{
    while (true)
    {
        if (nPos < static_cast<int>(aoEntries.size()))
        {
            auto &entry = aoEntries[nPos];
            nPos++;
            return entry.get();
        }
        if (osNextMarker.empty())
        {
            return nullptr;
        }
        if (!IssueListDir())
        {
            return nullptr;
        }
    }
}

/************************************************************************/
/*                       VSIAzureFSHandler                              */
/************************************************************************/

class VSIAzureFSHandler final : public IVSIS3LikeFSHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIAzureFSHandler)
    const std::string m_osPrefix;

    int CreateContainer(const std::string &osDirname);
    int DeleteContainer(const std::string &osDirname);

  protected:
    VSICurlHandle *CreateFileHandle(const char *pszFilename) override;
    std::string GetURLFromFilename(const std::string &osFilename) override;

    VSIAzureBlobHandleHelper *CreateAzHandleHelper(const char *pszURI,
                                                   bool bAllowNoObject);

    IVSIS3LikeHandleHelper *CreateHandleHelper(const char *pszURI,
                                               bool bAllowNoObject) override
    {
        return CreateAzHandleHelper(pszURI, bAllowNoObject);
    }

    char **GetFileList(const char *pszFilename, int nMaxFiles,
                       bool *pbGotFileList) override;

    void InvalidateRecursive(const std::string &osDirnameIn);

    int CopyFile(const char *pszSource, const char *pszTarget,
                 VSILFILE *fpSource, vsi_l_offset nSourceSize,
                 const char *const *papszOptions,
                 GDALProgressFunc pProgressFunc, void *pProgressData) override;

    int CopyObject(const char *oldpath, const char *newpath,
                   CSLConstList papszMetadata) override;
    int MkdirInternal(const char *pszDirname, long nMode,
                      bool bDoStatCheck) override;

    void ClearCache() override;

    bool IsAllowedHeaderForObjectCreation(const char *pszHeaderName) override
    {
        return STARTS_WITH(pszHeaderName, "x-ms-");
    }

    VSIVirtualHandleUniquePtr
    CreateWriteHandle(const char *pszFilename,
                      CSLConstList papszOptions) override;

  public:
    explicit VSIAzureFSHandler(const char *pszPrefix) : m_osPrefix(pszPrefix)
    {
    }

    ~VSIAzureFSHandler() override = default;

    std::string GetFSPrefix() const override
    {
        return m_osPrefix;
    }

    const char *GetDebugKey() const override
    {
        return "AZURE";
    }

    int Unlink(const char *pszFilename) override;
    int *UnlinkBatch(CSLConstList papszFiles) override;

    int *DeleteObjectBatch(CSLConstList papszFilesOrDirs) override
    {
        return UnlinkBatch(papszFilesOrDirs);
    }

    int Mkdir(const char *, long) override;
    int Rmdir(const char *) override;
    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;

    char **GetFileMetadata(const char *pszFilename, const char *pszDomain,
                           CSLConstList papszOptions) override;

    bool SetFileMetadata(const char *pszFilename, CSLConstList papszMetadata,
                         const char *pszDomain,
                         CSLConstList papszOptions) override;

    const char *GetOptions() override;

    char *GetSignedURL(const char *pszFilename,
                       CSLConstList papszOptions) override;

    char **GetFileList(const char *pszFilename, int nMaxFiles,
                       bool bCacheEntries, bool *pbGotFileList);

    VSIDIR *OpenDir(const char *pszPath, int nRecurseDepth,
                    const char *const *papszOptions) override;

    // Block list upload
    std::string PutBlock(const std::string &osFilename, int nPartNumber,
                         const void *pabyBuffer, size_t nBufferSize,
                         IVSIS3LikeHandleHelper *poS3HandleHelper,
                         int nMaxRetry, double dfRetryDelay,
                         CSLConstList papszOptions);
    bool PutBlockList(const std::string &osFilename,
                      const std::vector<std::string> &aosBlockIds,
                      IVSIS3LikeHandleHelper *poS3HandleHelper, int nMaxRetry,
                      double dfRetryDelay);

    // Multipart upload (mapping of S3 interface to PutBlock/PutBlockList)

    bool SupportsParallelMultipartUpload() const override
    {
        return true;
    }

    std::string
    InitiateMultipartUpload(const std::string & /* osFilename */,
                            IVSIS3LikeHandleHelper *, int /* nMaxRetry */,
                            double /* dfRetryDelay */,
                            CSLConstList /* papszOptions */) override
    {
        return "dummy";
    }

    std::string UploadPart(const std::string &osFilename, int nPartNumber,
                           const std::string & /* osUploadID */,
                           vsi_l_offset /* nPosition */, const void *pabyBuffer,
                           size_t nBufferSize,
                           IVSIS3LikeHandleHelper *poS3HandleHelper,
                           int nMaxRetry, double dfRetryDelay,
                           CSLConstList papszOptions) override
    {
        return PutBlock(osFilename, nPartNumber, pabyBuffer, nBufferSize,
                        poS3HandleHelper, nMaxRetry, dfRetryDelay,
                        papszOptions);
    }

    bool CompleteMultipart(const std::string &osFilename,
                           const std::string & /* osUploadID */,
                           const std::vector<std::string> &aosEtags,
                           vsi_l_offset /* nTotalSize */,
                           IVSIS3LikeHandleHelper *poS3HandleHelper,
                           int nMaxRetry, double dfRetryDelay) override
    {
        return PutBlockList(osFilename, aosEtags, poS3HandleHelper, nMaxRetry,
                            dfRetryDelay);
    }

    bool AbortMultipart(const std::string & /* osFilename */,
                        const std::string & /* osUploadID */,
                        IVSIS3LikeHandleHelper * /*poS3HandleHelper */,
                        int /* nMaxRetry */, double /* dfRetryDelay */) override
    {
        return true;
    }

    std::string
    GetStreamingFilename(const std::string &osFilename) const override;

    VSIFilesystemHandler *Duplicate(const char *pszPrefix) override
    {
        return new VSIAzureFSHandler(pszPrefix);
    }
};

/************************************************************************/
/*                          VSIAzureHandle                              */
/************************************************************************/

class VSIAzureHandle final : public VSICurlHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIAzureHandle)

    std::unique_ptr<VSIAzureBlobHandleHelper> m_poHandleHelper{};

  protected:
    virtual struct curl_slist *
    GetCurlHeaders(const std::string &osVerb,
                   const struct curl_slist *psExistingHeaders) override;
    virtual bool IsDirectoryFromExists(const char *pszVerb,
                                       int response_code) override;

  public:
    VSIAzureHandle(VSIAzureFSHandler *poFS, const char *pszFilename,
                   VSIAzureBlobHandleHelper *poHandleHelper);
};

/************************************************************************/
/*                          VSIAzureWriteHandle                         */
/************************************************************************/

class VSIAzureWriteHandle final : public VSIAppendWriteHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIAzureWriteHandle)

    std::unique_ptr<VSIAzureBlobHandleHelper> m_poHandleHelper{};
    CPLStringList m_aosOptions{};
    CPLStringList m_aosHTTPOptions{};

    bool Send(bool bIsLastBlock) override;
    bool SendInternal(bool bInitOnly, bool bIsLastBlock);

    void InvalidateParentDirectory();

  public:
    VSIAzureWriteHandle(VSIAzureFSHandler *poFS, const char *pszFilename,
                        VSIAzureBlobHandleHelper *poHandleHelper,
                        CSLConstList papszOptions);
    virtual ~VSIAzureWriteHandle();
};

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle *VSIAzureFSHandler::CreateFileHandle(const char *pszFilename)
{
    VSIAzureBlobHandleHelper *poHandleHelper =
        VSIAzureBlobHandleHelper::BuildFromURI(
            pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str());
    if (poHandleHelper == nullptr)
        return nullptr;
    return new VSIAzureHandle(this, pszFilename, poHandleHelper);
}

/************************************************************************/
/*                          CreateWriteHandle()                         */
/************************************************************************/

VSIVirtualHandleUniquePtr
VSIAzureFSHandler::CreateWriteHandle(const char *pszFilename,
                                     CSLConstList papszOptions)
{
    VSIAzureBlobHandleHelper *poHandleHelper =
        VSIAzureBlobHandleHelper::BuildFromURI(
            pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str());
    if (poHandleHelper == nullptr)
        return nullptr;
    auto poHandle = std::make_unique<VSIAzureWriteHandle>(
        this, pszFilename, poHandleHelper, papszOptions);
    if (!poHandle->IsOK())
    {
        return nullptr;
    }
    return VSIVirtualHandleUniquePtr(poHandle.release());
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIAzureFSHandler::Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
                            int nFlags)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return -1;

    if ((nFlags & VSI_STAT_CACHE_ONLY) != 0)
        return VSICurlFilesystemHandlerBase::Stat(pszFilename, pStatBuf,
                                                  nFlags);

    std::string osFilename(pszFilename);

    if ((osFilename.find('/', GetFSPrefix().size()) == std::string::npos ||
         osFilename.find('/', GetFSPrefix().size()) == osFilename.size() - 1) &&
        CPLGetConfigOption("AZURE_SAS", nullptr) != nullptr)
    {
        // On "/vsiaz/container", a HEAD or GET request fails to authenticate
        // when SAS is used, so use directory listing instead.
        char **papszRet = ReadDirInternal(osFilename.c_str(), 100, nullptr);
        int nRet = papszRet ? 0 : -1;
        if (nRet == 0)
        {
            pStatBuf->st_mtime = 0;
            pStatBuf->st_size = 0;
            pStatBuf->st_mode = S_IFDIR;

            FileProp cachedFileProp;
            GetCachedFileProp(GetURLFromFilename(osFilename.c_str()).c_str(),
                              cachedFileProp);
            cachedFileProp.eExists = EXIST_YES;
            cachedFileProp.bIsDirectory = true;
            cachedFileProp.bHasComputedFileSize = true;
            SetCachedFileProp(GetURLFromFilename(osFilename.c_str()).c_str(),
                              cachedFileProp);
        }
        CSLDestroy(papszRet);
        return nRet;
    }

    if (osFilename.find('/', GetFSPrefix().size()) == std::string::npos)
    {
        osFilename += "/";
    }

    if (osFilename.size() > GetFSPrefix().size())
    {
        // Special case for container
        std::string osFilenameWithoutEndSlash(osFilename);
        if (osFilenameWithoutEndSlash.back() == '/')
            osFilenameWithoutEndSlash.resize(osFilenameWithoutEndSlash.size() -
                                             1);
        if (osFilenameWithoutEndSlash.find('/', GetFSPrefix().size()) ==
            std::string::npos)
        {
            char **papszFileList = ReadDir(GetFSPrefix().c_str());
            const int nIdx = CSLFindString(
                papszFileList,
                osFilenameWithoutEndSlash.substr(GetFSPrefix().size()).c_str());
            CSLDestroy(papszFileList);
            if (nIdx >= 0)
            {
                pStatBuf->st_mtime = 0;
                pStatBuf->st_size = 0;
                pStatBuf->st_mode = S_IFDIR;
                return 0;
            }
        }
    }

    return VSICurlFilesystemHandlerBase::Stat(osFilename.c_str(), pStatBuf,
                                              nFlags);
}

/************************************************************************/
/*                          GetFileMetadata()                           */
/************************************************************************/

char **VSIAzureFSHandler::GetFileMetadata(const char *pszFilename,
                                          const char *pszDomain,
                                          CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;

    if (pszDomain == nullptr ||
        (!EQUAL(pszDomain, "TAGS") && !EQUAL(pszDomain, "METADATA")))
    {
        return VSICurlFilesystemHandlerBase::GetFileMetadata(
            pszFilename, pszDomain, papszOptions);
    }

    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), false));
    if (poHandleHelper == nullptr)
    {
        return nullptr;
    }

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("GetFileMetadata");

    bool bRetry;
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(
        VSIGetPathSpecificOption(pszFilename, "GDAL_HTTP_RETRY_DELAY",
                                 CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry =
        atoi(VSIGetPathSpecificOption(pszFilename, "GDAL_HTTP_MAX_RETRY",
                                      CPLSPrintf("%d", CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;
    bool bError = true;

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));

    CPLStringList aosMetadata;
    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        if (EQUAL(pszDomain, "METADATA"))
            poHandleHelper->AddQueryParameter("comp", "metadata");
        else
            poHandleHelper->AddQueryParameter("comp", "tags");

        struct curl_slist *headers =
            VSICurlSetOptions(hCurlHandle, poHandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List());

        headers = VSICurlMergeHeaders(
            headers, poHandleHelper->GetCurlHeaders("GET", headers));
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poHandleHelper.get());

        NetworkStatisticsLogger::LogGET(requestHelper.sWriteFuncData.nSize);

        if (response_code != 200 ||
            requestHelper.sWriteFuncHeaderData.pBuffer == nullptr)
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer,
                requestHelper.szCurlErrBuf);
            if (dfNewRetryDelay > 0 && nRetryCount < nMaxRetry)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "HTTP error code: %d - %s. "
                         "Retrying again in %.1f secs",
                         static_cast<int>(response_code),
                         poHandleHelper->GetURL().c_str(), dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "GetFileMetadata failed on %s: %s",
                         pszFilename,
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
            }
        }
        else
        {
            if (EQUAL(pszDomain, "METADATA"))
            {
                char **papszHeaders = CSLTokenizeString2(
                    requestHelper.sWriteFuncHeaderData.pBuffer, "\r\n", 0);
                for (int i = 0; papszHeaders[i]; ++i)
                {
                    char *pszKey = nullptr;
                    const char *pszValue =
                        CPLParseNameValue(papszHeaders[i], &pszKey);
                    // Content-Length is returned as 0
                    if (pszKey && pszValue && !EQUAL(pszKey, "Content-Length"))
                    {
                        aosMetadata.SetNameValue(pszKey, pszValue);
                    }
                    CPLFree(pszKey);
                }
                CSLDestroy(papszHeaders);
            }
            else
            {
                CPLXMLNode *psXML =
                    CPLParseXMLString(requestHelper.sWriteFuncData.pBuffer);
                if (psXML)
                {
                    CPLXMLNode *psTagSet = CPLGetXMLNode(psXML, "=Tags.TagSet");
                    if (psTagSet)
                    {
                        for (CPLXMLNode *psIter = psTagSet->psChild; psIter;
                             psIter = psIter->psNext)
                        {
                            if (psIter->eType == CXT_Element &&
                                strcmp(psIter->pszValue, "Tag") == 0)
                            {
                                const char *pszKey =
                                    CPLGetXMLValue(psIter, "Key", "");
                                const char *pszValue =
                                    CPLGetXMLValue(psIter, "Value", "");
                                aosMetadata.SetNameValue(pszKey, pszValue);
                            }
                        }
                    }
                    CPLDestroyXMLNode(psXML);
                }
            }
            bError = false;
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);
    return bError ? nullptr : CSLDuplicate(aosMetadata.List());
}

/************************************************************************/
/*                          SetFileMetadata()                           */
/************************************************************************/

bool VSIAzureFSHandler::SetFileMetadata(const char *pszFilename,
                                        CSLConstList papszMetadata,
                                        const char *pszDomain,
                                        CSLConstList /* papszOptions */)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return false;

    if (pszDomain == nullptr ||
        !(EQUAL(pszDomain, "PROPERTIES") || EQUAL(pszDomain, "METADATA") ||
          EQUAL(pszDomain, "TAGS")))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only PROPERTIES, METADATA and TAGS domain are supported");
        return false;
    }

    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), false));
    if (poHandleHelper == nullptr)
    {
        return false;
    }

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("SetFileMetadata");

    bool bRetry;
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(
        VSIGetPathSpecificOption(pszFilename, "GDAL_HTTP_RETRY_DELAY",
                                 CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry =
        atoi(VSIGetPathSpecificOption(pszFilename, "GDAL_HTTP_MAX_RETRY",
                                      CPLSPrintf("%d", CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;

    bool bRet = false;

    // Compose XML content for TAGS
    std::string osXML;
    if (EQUAL(pszDomain, "TAGS"))
    {
        CPLXMLNode *psXML = CPLCreateXMLNode(nullptr, CXT_Element, "?xml");
        CPLAddXMLAttributeAndValue(psXML, "version", "1.0");
        CPLAddXMLAttributeAndValue(psXML, "encoding", "UTF-8");
        CPLXMLNode *psTags = CPLCreateXMLNode(nullptr, CXT_Element, "Tags");
        psXML->psNext = psTags;
        CPLXMLNode *psTagSet = CPLCreateXMLNode(psTags, CXT_Element, "TagSet");
        for (int i = 0; papszMetadata && papszMetadata[i]; ++i)
        {
            char *pszKey = nullptr;
            const char *pszValue = CPLParseNameValue(papszMetadata[i], &pszKey);
            if (pszKey && pszValue)
            {
                CPLXMLNode *psTag =
                    CPLCreateXMLNode(psTagSet, CXT_Element, "Tag");
                CPLCreateXMLElementAndValue(psTag, "Key", pszKey);
                CPLCreateXMLElementAndValue(psTag, "Value", pszValue);
            }
            CPLFree(pszKey);
        }

        char *pszXML = CPLSerializeXMLTree(psXML);
        osXML = pszXML;
        CPLFree(pszXML);
        CPLDestroyXMLNode(psXML);
    }

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));

    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();

        if (EQUAL(pszDomain, "PROPERTIES"))
            poHandleHelper->AddQueryParameter("comp", "properties");
        else if (EQUAL(pszDomain, "METADATA"))
            poHandleHelper->AddQueryParameter("comp", "metadata");
        else
            poHandleHelper->AddQueryParameter("comp", "tags");

        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT");
        if (!osXML.empty())
        {
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS,
                                       osXML.c_str());
        }

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poHandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));

        CPLStringList aosList;
        if (EQUAL(pszDomain, "PROPERTIES") || EQUAL(pszDomain, "METADATA"))
        {
            for (CSLConstList papszIter = papszMetadata;
                 papszIter && *papszIter; ++papszIter)
            {
                char *pszKey = nullptr;
                const char *pszValue = CPLParseNameValue(*papszIter, &pszKey);
                if (pszKey && pszValue)
                {
                    const char *pszHeader =
                        CPLSPrintf("%s: %s", pszKey, pszValue);
                    aosList.AddString(pszHeader);
                    headers = curl_slist_append(headers, pszHeader);
                }
                CPLFree(pszKey);
            }
        }

        CPLString osContentLength;
        osContentLength.Printf("Content-Length: %d",
                               static_cast<int>(osXML.size()));
        headers = curl_slist_append(headers, osContentLength.c_str());
        if (!osXML.empty())
        {
            headers = curl_slist_append(
                headers, "Content-Type: application/xml; charset=UTF-8");
            headers = VSICurlMergeHeaders(
                headers, poHandleHelper->GetCurlHeaders(
                             "PUT", headers, osXML.c_str(), osXML.size()));
        }
        else
        {
            headers = VSICurlMergeHeaders(
                headers, poHandleHelper->GetCurlHeaders("PUT", headers));
        }
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        NetworkStatisticsLogger::LogPUT(osXML.size());

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poHandleHelper.get());

        if (response_code != 200 && response_code != 204)
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer,
                requestHelper.szCurlErrBuf);
            if (dfNewRetryDelay > 0 && nRetryCount < nMaxRetry)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "HTTP error code: %d - %s. "
                         "Retrying again in %.1f secs",
                         static_cast<int>(response_code),
                         poHandleHelper->GetURL().c_str(), dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "SetFileMetadata on %s failed: %s",
                         pszFilename,
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
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
/*                      GetStreamingFilename()                          */
/************************************************************************/

std::string
VSIAzureFSHandler::GetStreamingFilename(const std::string &osFilename) const
{
    if (STARTS_WITH(osFilename.c_str(), GetFSPrefix().c_str()))
        return "/vsiaz_streaming/" + osFilename.substr(GetFSPrefix().size());
    return osFilename;
}

/************************************************************************/
/*                        GetAzureBufferSize()                          */
/************************************************************************/

int GetAzureBufferSize()
{
    int nBufferSize;
    int nChunkSizeMB = atoi(CPLGetConfigOption("VSIAZ_CHUNK_SIZE", "4"));
    if (nChunkSizeMB <= 0 || nChunkSizeMB > 4)
        nBufferSize = 4 * 1024 * 1024;
    else
        nBufferSize = nChunkSizeMB * 1024 * 1024;

    // For testing only !
    const char *pszChunkSizeBytes =
        CPLGetConfigOption("VSIAZ_CHUNK_SIZE_BYTES", nullptr);
    if (pszChunkSizeBytes)
        nBufferSize = atoi(pszChunkSizeBytes);
    if (nBufferSize <= 0 || nBufferSize > 4 * 1024 * 1024)
        nBufferSize = 4 * 1024 * 1024;
    return nBufferSize;
}

/************************************************************************/
/*                       VSIAzureWriteHandle()                          */
/************************************************************************/

VSIAzureWriteHandle::VSIAzureWriteHandle(
    VSIAzureFSHandler *poFS, const char *pszFilename,
    VSIAzureBlobHandleHelper *poHandleHelper, CSLConstList papszOptions)
    : VSIAppendWriteHandle(poFS, poFS->GetFSPrefix().c_str(), pszFilename,
                           GetAzureBufferSize()),
      m_poHandleHelper(poHandleHelper), m_aosOptions(papszOptions),
      m_aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename))
{
}

/************************************************************************/
/*                      ~VSIAzureWriteHandle()                          */
/************************************************************************/

VSIAzureWriteHandle::~VSIAzureWriteHandle()
{
    Close();
}

/************************************************************************/
/*                    InvalidateParentDirectory()                       */
/************************************************************************/

void VSIAzureWriteHandle::InvalidateParentDirectory()
{
    m_poFS->InvalidateCachedData(m_poHandleHelper->GetURLNoKVP().c_str());

    std::string osFilenameWithoutSlash(m_osFilename);
    if (!osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/')
        osFilenameWithoutSlash.resize(osFilenameWithoutSlash.size() - 1);
    m_poFS->InvalidateDirContent(CPLGetDirname(osFilenameWithoutSlash.c_str()));
}

/************************************************************************/
/*                             Send()                                   */
/************************************************************************/

bool VSIAzureWriteHandle::Send(bool bIsLastBlock)
{
    if (!bIsLastBlock)
    {
        CPLAssert(m_nBufferOff == m_nBufferSize);
        if (m_nCurOffset == static_cast<vsi_l_offset>(m_nBufferSize))
        {
            // First full buffer ? Then create the blob empty
            if (!SendInternal(true, false))
                return false;
        }
    }
    return SendInternal(false, bIsLastBlock);
}

/************************************************************************/
/*                          SendInternal()                              */
/************************************************************************/

bool VSIAzureWriteHandle::SendInternal(bool bInitOnly, bool bIsLastBlock)
{
    NetworkStatisticsFileSystem oContextFS("/vsiaz/");
    NetworkStatisticsFile oContextFile(m_osFilename.c_str());
    NetworkStatisticsAction oContextAction("Write");

    bool bSuccess = true;
    const bool bSingleBlock =
        bIsLastBlock &&
        (m_nCurOffset <= static_cast<vsi_l_offset>(m_nBufferSize));

    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(
        VSIGetPathSpecificOption(m_osFilename.c_str(), "GDAL_HTTP_RETRY_DELAY",
                                 CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry = atoi(
        VSIGetPathSpecificOption(m_osFilename.c_str(), "GDAL_HTTP_MAX_RETRY",
                                 CPLSPrintf("%d", CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;
    bool bHasAlreadyHandled409 = false;
    bool bRetry;

    do
    {
        bRetry = false;

        m_nBufferOffReadCallback = 0;
        CURL *hCurlHandle = curl_easy_init();

        m_poHandleHelper->ResetQueryParameters();
        if (!bSingleBlock && !bInitOnly)
        {
            m_poHandleHelper->AddQueryParameter("comp", "appendblock");
        }

        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                                   ReadCallBackBuffer);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READDATA,
                                   static_cast<VSIAppendWriteHandle *>(this));

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, m_poHandleHelper->GetURL().c_str(),
                              m_aosHTTPOptions.List()));
        headers = VSICurlSetCreationHeadersFromOptions(
            headers, m_aosOptions.List(), m_osFilename.c_str());

        CPLString osContentLength;  // leave it in this scope
        if (bSingleBlock)
        {
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                                       m_nBufferOff);
            if (m_nBufferOff)
                headers = curl_slist_append(headers, "Expect: 100-continue");
            osContentLength.Printf("Content-Length: %d", m_nBufferOff);
            headers = curl_slist_append(headers, osContentLength.c_str());
            headers = curl_slist_append(headers, "x-ms-blob-type: BlockBlob");
        }
        else if (bInitOnly)
        {
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, 0);
            headers = curl_slist_append(headers, "Content-Length: 0");
            headers = curl_slist_append(headers, "x-ms-blob-type: AppendBlob");
        }
        else
        {
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                                       m_nBufferOff);
            osContentLength.Printf("Content-Length: %d", m_nBufferOff);
            headers = curl_slist_append(headers, osContentLength.c_str());
            vsi_l_offset nStartOffset = m_nCurOffset - m_nBufferOff;
            const char *pszAppendPos = CPLSPrintf(
                "x-ms-blob-condition-appendpos: " CPL_FRMT_GUIB, nStartOffset);
            headers = curl_slist_append(headers, pszAppendPos);
        }

        headers = VSICurlMergeHeaders(
            headers, m_poHandleHelper->GetCurlHeaders("PUT", headers));
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, m_poFS, m_poHandleHelper.get());

        NetworkStatisticsLogger::LogPUT(m_nBufferOff);

        if (!bHasAlreadyHandled409 && response_code == 409)
        {
            bHasAlreadyHandled409 = true;
            CPLDebug(cpl::down_cast<VSIAzureFSHandler *>(m_poFS)->GetDebugKey(),
                     "%s",
                     requestHelper.sWriteFuncData.pBuffer
                         ? requestHelper.sWriteFuncData.pBuffer
                         : "(null)");

            // The blob type is invalid for this operation
            // Delete the file, and retry
            if (cpl::down_cast<VSIAzureFSHandler *>(m_poFS)->DeleteObject(
                    m_osFilename.c_str()) == 0)
            {
                bRetry = true;
            }
        }
        else if (response_code != 201)
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer,
                requestHelper.szCurlErrBuf);
            if (dfNewRetryDelay > 0 && nRetryCount < nMaxRetry)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "HTTP error code: %d - %s. "
                         "Retrying again in %.1f secs",
                         static_cast<int>(response_code),
                         m_poHandleHelper->GetURL().c_str(), dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else
            {
                CPLDebug(
                    cpl::down_cast<VSIAzureFSHandler *>(m_poFS)->GetDebugKey(),
                    "%s",
                    requestHelper.sWriteFuncData.pBuffer
                        ? requestHelper.sWriteFuncData.pBuffer
                        : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "PUT of %s failed",
                         m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            InvalidateParentDirectory();
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return bSuccess;
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIAzureFSHandler::ClearCache()
{
    IVSIS3LikeFSHandler::ClearCache();

    VSIAzureBlobHandleHelper::ClearCache();
}

/************************************************************************/
/*                          GetURLFromFilename()                        */
/************************************************************************/

std::string VSIAzureFSHandler::GetURLFromFilename(const std::string &osFilename)
{
    std::string osFilenameWithoutPrefix =
        osFilename.substr(GetFSPrefix().size());
    VSIAzureBlobHandleHelper *poHandleHelper =
        VSIAzureBlobHandleHelper::BuildFromURI(osFilenameWithoutPrefix.c_str(),
                                               GetFSPrefix().c_str());
    if (poHandleHelper == nullptr)
        return std::string();
    std::string osURL(poHandleHelper->GetURLNoKVP());
    delete poHandleHelper;
    return osURL;
}

/************************************************************************/
/*                        CreateAzHandleHelper()                       */
/************************************************************************/

VSIAzureBlobHandleHelper *
VSIAzureFSHandler::CreateAzHandleHelper(const char *pszURI, bool)
{
    return VSIAzureBlobHandleHelper::BuildFromURI(pszURI,
                                                  GetFSPrefix().c_str());
}

/************************************************************************/
/*                         InvalidateRecursive()                        */
/************************************************************************/

void VSIAzureFSHandler::InvalidateRecursive(const std::string &osDirnameIn)
{
    // As Azure directories disappear as soon there is no remaining file
    // we may need to invalidate the whole hierarchy
    std::string osDirname(osDirnameIn);
    while (osDirname.size() > GetFSPrefix().size())
    {
        InvalidateDirContent(osDirname.c_str());
        InvalidateCachedData(GetURLFromFilename(osDirname.c_str()).c_str());
        osDirname = CPLGetDirname(osDirname.c_str());
    }
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSIAzureFSHandler::Unlink(const char *pszFilename)
{
    int ret = IVSIS3LikeFSHandler::Unlink(pszFilename);
    if (ret != 0)
        return ret;

    InvalidateRecursive(CPLGetDirname(pszFilename));
    return 0;
}

/************************************************************************/
/*                           UnlinkBatch()                              */
/************************************************************************/

int *VSIAzureFSHandler::UnlinkBatch(CSLConstList papszFiles)
{
    // Implemented using
    // https://learn.microsoft.com/en-us/rest/api/storageservices/blob-batch

    const char *pszFirstFilename =
        papszFiles && papszFiles[0] ? papszFiles[0] : nullptr;

    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        VSIAzureBlobHandleHelper::BuildFromURI(
            "", GetFSPrefix().c_str(),
            pszFirstFilename &&
                    STARTS_WITH(pszFirstFilename, GetFSPrefix().c_str())
                ? pszFirstFilename + GetFSPrefix().size()
                : nullptr));

    int *panRet =
        static_cast<int *>(CPLCalloc(sizeof(int), CSLCount(papszFiles)));

    if (!poHandleHelper || pszFirstFilename == nullptr)
        return panRet;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("UnlinkBatch");

    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(
        VSIGetPathSpecificOption(pszFirstFilename, "GDAL_HTTP_RETRY_DELAY",
                                 CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry =
        atoi(VSIGetPathSpecificOption(pszFirstFilename, "GDAL_HTTP_MAX_RETRY",
                                      CPLSPrintf("%d", CPL_HTTP_MAX_RETRY)));

    // For debug / testing only
    const int nBatchSize =
        std::max(1, std::min(256, atoi(CPLGetConfigOption(
                                      "CPL_VSIAZ_UNLINK_BATCH_SIZE", "256"))));
    std::string osPOSTContent;

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(pszFirstFilename));

    int nFilesInBatch = 0;
    int nFirstIDInBatch = 0;

    const auto DoPOST = [this, panRet, &nFilesInBatch, &dfRetryDelay, nMaxRetry,
                         &aosHTTPOptions, &poHandleHelper, &osPOSTContent,
                         &nFirstIDInBatch](int nLastID)
    {
        osPOSTContent += "--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589--\r\n";

#ifdef DEBUG_VERBOSE
        CPLDebug(GetDebugKey(), "%s", osPOSTContent.c_str());
#endif

        // Run request
        int nRetryCount = 0;
        bool bRetry;
        std::string osResponse;
        do
        {
            poHandleHelper->AddQueryParameter("comp", "batch");

            bRetry = false;
            CURL *hCurlHandle = curl_easy_init();

            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST,
                                       "POST");
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS,
                                       osPOSTContent.c_str());

            struct curl_slist *headers = static_cast<struct curl_slist *>(
                CPLHTTPSetOptions(hCurlHandle, poHandleHelper->GetURL().c_str(),
                                  aosHTTPOptions.List()));
            headers = curl_slist_append(
                headers, "Content-Type: multipart/mixed; "
                         "boundary=batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589");
            headers = curl_slist_append(
                headers, CPLSPrintf("Content-Length: %d",
                                    static_cast<int>(osPOSTContent.size())));
            headers = VSICurlMergeHeaders(
                headers, poHandleHelper->GetCurlHeaders("POST", headers));

            CurlRequestHelper requestHelper;
            const long response_code = requestHelper.perform(
                hCurlHandle, headers, this, poHandleHelper.get());

            NetworkStatisticsLogger::LogPOST(
                osPOSTContent.size(), requestHelper.sWriteFuncData.nSize);

            if (response_code != 202 ||
                requestHelper.sWriteFuncData.pBuffer == nullptr)
            {
                // Look if we should attempt a retry
                const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                    static_cast<int>(response_code), dfRetryDelay,
                    requestHelper.sWriteFuncHeaderData.pBuffer,
                    requestHelper.szCurlErrBuf);
                if (dfNewRetryDelay > 0 && nRetryCount < nMaxRetry)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "HTTP error code: %d - %s. "
                             "Retrying again in %.1f secs",
                             static_cast<int>(response_code),
                             poHandleHelper->GetURL().c_str(), dfRetryDelay);
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
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer);
#endif
                osResponse = requestHelper.sWriteFuncData.pBuffer;
            }

            curl_easy_cleanup(hCurlHandle);
        } while (bRetry);

        // Mark deleted files
        for (int j = nFirstIDInBatch; j <= nLastID; j++)
        {
            auto nPos = osResponse.find(CPLSPrintf("Content-ID: <%d>", j));
            if (nPos != std::string::npos)
            {
                nPos = osResponse.find("HTTP/1.1 ", nPos);
                if (nPos != std::string::npos)
                {
                    const char *pszHTTPCode =
                        osResponse.c_str() + nPos + strlen("HTTP/1.1 ");
                    panRet[j] = (atoi(pszHTTPCode) == 202) ? 1 : 0;
                }
            }
        }

        osPOSTContent.clear();
        nFilesInBatch = 0;
        nFirstIDInBatch = nLastID;
    };

    for (int i = 0; papszFiles && papszFiles[i]; i++)
    {
        CPLAssert(STARTS_WITH_CI(papszFiles[i], GetFSPrefix().c_str()));

        std::string osAuthorization;
        std::string osXMSDate;
        {
            auto poTmpHandleHelper = std::unique_ptr<VSIAzureBlobHandleHelper>(
                VSIAzureBlobHandleHelper::BuildFromURI(papszFiles[i] +
                                                           GetFSPrefix().size(),
                                                       GetFSPrefix().c_str()));
            // x-ms-version must not be included in the subrequests...
            poTmpHandleHelper->SetIncludeMSVersion(false);
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
                else if (STARTS_WITH_CI(iter->data, "x-ms-date: "))
                {
                    osXMSDate = iter->data;
                }
            }
            curl_slist_free_all(subrequest_headers);
            curl_easy_cleanup(hCurlHandle);
        }

        std::string osSubrequest;
        osSubrequest += "--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589\r\n";
        osSubrequest += "Content-Type: application/http\r\n";
        osSubrequest += CPLSPrintf("Content-ID: <%d>\r\n", i);
        osSubrequest += "Content-Transfer-Encoding: binary\r\n";
        osSubrequest += "\r\n";
        osSubrequest += "DELETE /";
        osSubrequest += (papszFiles[i] + GetFSPrefix().size());
        osSubrequest += " HTTP/1.1\r\n";
        osSubrequest += osXMSDate;
        osSubrequest += "\r\n";
        osSubrequest += osAuthorization;
        osSubrequest += "\r\n";
        osSubrequest += "Content-Length: 0\r\n";
        osSubrequest += "\r\n";
        osSubrequest += "\r\n";

        // The size of the body for a batch request can't exceed 4 MB.
        // Add some margin for the end boundary delimiter.
        if (i > nFirstIDInBatch &&
            osPOSTContent.size() + osSubrequest.size() > 4 * 1024 * 1024 - 100)
        {
            DoPOST(i - 1);
        }

        osPOSTContent += osSubrequest;
        nFilesInBatch++;

        if (nFilesInBatch == nBatchSize || papszFiles[i + 1] == nullptr)
        {
            DoPOST(i);
        }
    }
    return panRet;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIAzureFSHandler::MkdirInternal(const char *pszDirname, long /* nMode */,
                                     bool bDoStatCheck)
{
    if (!STARTS_WITH_CI(pszDirname, GetFSPrefix().c_str()))
        return -1;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("Mkdir");

    std::string osDirname(pszDirname);
    if (!osDirname.empty() && osDirname.back() != '/')
        osDirname += "/";

    if (bDoStatCheck)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osDirname.c_str(), &sStat) == 0 &&
            sStat.st_mode == S_IFDIR)
        {
            CPLDebug(GetDebugKey(), "Directory %s already exists",
                     osDirname.c_str());
            errno = EEXIST;
            return -1;
        }
    }

    std::string osDirnameWithoutEndSlash(osDirname);
    osDirnameWithoutEndSlash.resize(osDirnameWithoutEndSlash.size() - 1);
    if (osDirnameWithoutEndSlash.size() > GetFSPrefix().size() &&
        osDirnameWithoutEndSlash.find('/', GetFSPrefix().size()) ==
            std::string::npos)
    {
        return CreateContainer(osDirnameWithoutEndSlash);
    }

    InvalidateCachedData(GetURLFromFilename(osDirname.c_str()).c_str());
    InvalidateCachedData(
        GetURLFromFilename(osDirnameWithoutEndSlash.c_str()).c_str());
    InvalidateDirContent(CPLGetDirname(osDirnameWithoutEndSlash.c_str()));

    VSILFILE *fp = VSIFOpenL((osDirname + GDAL_MARKER_FOR_DIR).c_str(), "wb");
    if (fp != nullptr)
    {
        CPLErrorReset();
        VSIFCloseL(fp);
        return CPLGetLastErrorType() == CPLE_None ? 0 : -1;
    }
    else
    {
        return -1;
    }
}

int VSIAzureFSHandler::Mkdir(const char *pszDirname, long nMode)
{
    return MkdirInternal(pszDirname, nMode, true);
}

/************************************************************************/
/*                        CreateContainer()                             */
/************************************************************************/

int VSIAzureFSHandler::CreateContainer(const std::string &osDirname)
{
    std::string osDirnameWithoutPrefix = osDirname.substr(GetFSPrefix().size());
    auto poS3HandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(osDirnameWithoutPrefix.c_str(), false));
    if (poS3HandleHelper == nullptr)
    {
        return -1;
    }

    int nRet = 0;

    bool bRetry;

    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(
        VSIGetPathSpecificOption(osDirname.c_str(), "GDAL_HTTP_RETRY_DELAY",
                                 CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry =
        atoi(VSIGetPathSpecificOption(osDirname.c_str(), "GDAL_HTTP_MAX_RETRY",
                                      CPLSPrintf("%d", CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osDirname.c_str()));

    do
    {
        poS3HandleHelper->AddQueryParameter("restype", "container");

        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = curl_slist_append(headers, "Content-Length: 0");
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("PUT", headers));
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poS3HandleHelper.get());

        NetworkStatisticsLogger::LogPUT(0);

        if (response_code != 201)
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer,
                requestHelper.szCurlErrBuf);
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
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Creation of container %s failed", osDirname.c_str());
                nRet = -1;
            }
        }
        else
        {
            InvalidateCachedData(poS3HandleHelper->GetURLNoKVP().c_str());
            InvalidateDirContent(GetFSPrefix().c_str());
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return nRet;
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIAzureFSHandler::Rmdir(const char *pszDirname)
{
    if (!STARTS_WITH_CI(pszDirname, GetFSPrefix().c_str()))
        return -1;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("Rmdir");

    std::string osDirname(pszDirname);
    if (!osDirname.empty() && osDirname.back() != '/')
        osDirname += "/";

    VSIStatBufL sStat;
    if (VSIStatL(osDirname.c_str(), &sStat) != 0)
    {
        InvalidateCachedData(
            GetURLFromFilename(osDirname.substr(0, osDirname.size() - 1))
                .c_str());
        // The directory might have not been created by GDAL, and thus lacking
        // the GDAL marker file, so do not turn non-existence as an error
        return 0;
    }
    else if (sStat.st_mode != S_IFDIR)
    {
        CPLDebug(GetDebugKey(), "%s is not a directory", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    char **papszFileList = ReadDirEx(osDirname.c_str(), 1);
    bool bEmptyDir =
        (papszFileList != nullptr && EQUAL(papszFileList[0], ".") &&
         papszFileList[1] == nullptr);
    CSLDestroy(papszFileList);
    if (!bEmptyDir)
    {
        CPLDebug(GetDebugKey(), "%s is not empty", pszDirname);
        errno = ENOTEMPTY;
        return -1;
    }

    std::string osDirnameWithoutEndSlash(osDirname);
    osDirnameWithoutEndSlash.resize(osDirnameWithoutEndSlash.size() - 1);
    if (osDirnameWithoutEndSlash.size() > GetFSPrefix().size() &&
        osDirnameWithoutEndSlash.find('/', GetFSPrefix().size()) ==
            std::string::npos)
    {
        return DeleteContainer(osDirnameWithoutEndSlash);
    }

    InvalidateCachedData(GetURLFromFilename(osDirname.c_str()).c_str());
    InvalidateCachedData(
        GetURLFromFilename(osDirnameWithoutEndSlash.c_str()).c_str());
    InvalidateRecursive(CPLGetDirname(osDirnameWithoutEndSlash.c_str()));
    if (osDirnameWithoutEndSlash.find('/', GetFSPrefix().size()) ==
        std::string::npos)
    {
        CPLDebug(GetDebugKey(), "%s is a container", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    if (DeleteObject((osDirname + GDAL_MARKER_FOR_DIR).c_str()) == 0)
        return 0;
    // The directory might have not been created by GDAL, and thus lacking the
    // GDAL marker file, so check if is there, and if not, return success.
    if (VSIStatL(osDirname.c_str(), &sStat) != 0)
        return 0;
    return -1;
}

/************************************************************************/
/*                        DeleteContainer()                             */
/************************************************************************/

int VSIAzureFSHandler::DeleteContainer(const std::string &osDirname)
{
    std::string osDirnameWithoutPrefix = osDirname.substr(GetFSPrefix().size());
    auto poS3HandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(osDirnameWithoutPrefix.c_str(), false));
    if (poS3HandleHelper == nullptr)
    {
        return -1;
    }

    int nRet = 0;

    bool bRetry;

    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(
        VSIGetPathSpecificOption(osDirname.c_str(), "GDAL_HTTP_RETRY_DELAY",
                                 CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry =
        atoi(VSIGetPathSpecificOption(osDirname.c_str(), "GDAL_HTTP_MAX_RETRY",
                                      CPLSPrintf("%d", CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osDirname.c_str()));

    do
    {
        poS3HandleHelper->AddQueryParameter("restype", "container");

        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST,
                                   "DELETE");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = curl_slist_append(headers, "Content-Length: 0");
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("DELETE", headers));
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poS3HandleHelper.get());

        NetworkStatisticsLogger::LogPUT(0);

        if (response_code != 202)
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer,
                requestHelper.szCurlErrBuf);
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
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Deletion of container %s failed", osDirname.c_str());
                nRet = -1;
            }
        }
        else
        {
            InvalidateCachedData(poS3HandleHelper->GetURLNoKVP().c_str());
            InvalidateDirContent(GetFSPrefix().c_str());
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return nRet;
}

/************************************************************************/
/*                           CopyFile()                                 */
/************************************************************************/

int VSIAzureFSHandler::CopyFile(const char *pszSource, const char *pszTarget,
                                VSILFILE *fpSource, vsi_l_offset nSourceSize,
                                CSLConstList papszOptions,
                                GDALProgressFunc pProgressFunc,
                                void *pProgressData)
{
    const std::string osPrefix(GetFSPrefix());
    if ((STARTS_WITH(pszSource, "/vsis3/") ||
         STARTS_WITH(pszSource, "/vsigs/") ||
         STARTS_WITH(pszSource, "/vsiadls/") ||
         STARTS_WITH(pszSource, "/vsicurl/")) &&
        STARTS_WITH(pszTarget, osPrefix.c_str()))
    {
        std::string osMsg("Copying of");
        osMsg += pszSource;

        NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
        NetworkStatisticsAction oContextAction("CopyFile");

        bool bRet = CopyObject(pszSource, pszTarget, papszOptions) == 0;
        if (bRet && pProgressFunc)
        {
            bRet = pProgressFunc(1.0, osMsg.c_str(), pProgressData) != 0;
        }
        return bRet ? 0 : -1;
    }

    return IVSIS3LikeFSHandler::CopyFile(pszSource, pszTarget, fpSource,
                                         nSourceSize, papszOptions,
                                         pProgressFunc, pProgressData);
}

/************************************************************************/
/*                            CopyObject()                              */
/************************************************************************/

int VSIAzureFSHandler::CopyObject(const char *oldpath, const char *newpath,
                                  CSLConstList /* papszMetadata */)
{
    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("CopyObject");

    std::string osTargetNameWithoutPrefix = newpath + GetFSPrefix().size();
    auto poHandleHelper = std::unique_ptr<VSIAzureBlobHandleHelper>(
        CreateAzHandleHelper(osTargetNameWithoutPrefix.c_str(), false));
    if (poHandleHelper == nullptr)
    {
        return -1;
    }

    std::string osSourceHeader("x-ms-copy-source: ");
    bool bUseSourceSignedURL = true;
    if (STARTS_WITH(oldpath, GetFSPrefix().c_str()))
    {
        std::string osSourceNameWithoutPrefix = oldpath + GetFSPrefix().size();
        auto poHandleHelperSource = std::unique_ptr<VSIAzureBlobHandleHelper>(
            CreateAzHandleHelper(osSourceNameWithoutPrefix.c_str(), false));
        if (poHandleHelperSource == nullptr)
        {
            return -1;
        }
        // We can use a unsigned source URL only if
        // the source and target are in the same bucket
        if (poHandleHelper->GetStorageAccount() ==
                poHandleHelperSource->GetStorageAccount() &&
            poHandleHelper->GetBucket() == poHandleHelperSource->GetBucket())
        {
            bUseSourceSignedURL = false;
            osSourceHeader += poHandleHelperSource->GetURLNoKVP();
        }
    }

    if (bUseSourceSignedURL)
    {
        VSIStatBufL sStat;
        // This has the effect of making sure that the S3 region is correct
        // if copying from /vsis3/
        if (VSIStatExL(oldpath, &sStat, VSI_STAT_EXISTS_FLAG) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s does not exist", oldpath);
            return -1;
        }

        char *pszSignedURL = VSIGetSignedURL(oldpath, nullptr);
        if (!pszSignedURL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot get signed URL for %s", oldpath);
            return -1;
        }
        osSourceHeader += pszSignedURL;
        VSIFree(pszSignedURL);
    }

    int nRet = 0;

    bool bRetry;

    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(
        VSIGetPathSpecificOption(oldpath, "GDAL_HTTP_RETRY_DELAY",
                                 CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry = atoi(VSIGetPathSpecificOption(
        oldpath, "GDAL_HTTP_MAX_RETRY", CPLSPrintf("%d", CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(oldpath));

    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poHandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = curl_slist_append(headers, osSourceHeader.c_str());
        headers = VSICurlSetContentTypeFromExt(headers, newpath);
        headers = curl_slist_append(headers, "Content-Length: 0");
        headers = VSICurlMergeHeaders(
            headers, poHandleHelper->GetCurlHeaders("PUT", headers));
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poHandleHelper.get());

        NetworkStatisticsLogger::LogPUT(0);

        if (response_code != 202)
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer,
                requestHelper.szCurlErrBuf);
            if (dfNewRetryDelay > 0 && nRetryCount < nMaxRetry)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "HTTP error code: %d - %s. "
                         "Retrying again in %.1f secs",
                         static_cast<int>(response_code),
                         poHandleHelper->GetURL().c_str(), dfRetryDelay);
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
                CPLError(CE_Failure, CPLE_AppDefined, "Copy of %s to %s failed",
                         oldpath, newpath);
                nRet = -1;
            }
        }
        else
        {
            InvalidateCachedData(poHandleHelper->GetURLNoKVP().c_str());

            std::string osFilenameWithoutSlash(newpath);
            if (!osFilenameWithoutSlash.empty() &&
                osFilenameWithoutSlash.back() == '/')
                osFilenameWithoutSlash.resize(osFilenameWithoutSlash.size() -
                                              1);

            InvalidateDirContent(CPLGetDirname(osFilenameWithoutSlash.c_str()));
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return nRet;
}

/************************************************************************/
/*                             PutBlock()                               */
/************************************************************************/

std::string VSIAzureFSHandler::PutBlock(
    const std::string &osFilename, int nPartNumber, const void *pabyBuffer,
    size_t nBufferSize, IVSIS3LikeHandleHelper *poS3HandleHelper, int nMaxRetry,
    double dfRetryDelay, CSLConstList papszOptions)
{
    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(osFilename.c_str());
    NetworkStatisticsAction oContextAction("PutBlock");

    bool bRetry;
    int nRetryCount = 0;
    std::string osBlockId(CPLSPrintf("%012d", nPartNumber));

    const std::string osContentLength(
        CPLSPrintf("Content-Length: %d", static_cast<int>(nBufferSize)));

    bool bHasAlreadyHandled409 = false;

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osFilename.c_str()));

    do
    {
        bRetry = false;

        poS3HandleHelper->AddQueryParameter("comp", "block");
        poS3HandleHelper->AddQueryParameter("blockid", osBlockId);

        CURL *hCurlHandle = curl_easy_init();
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                                   PutData::ReadCallBackBuffer);
        PutData putData;
        putData.pabyData = static_cast<const GByte *>(pabyBuffer);
        putData.nOff = 0;
        putData.nTotalSize = nBufferSize;
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, &putData);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                                   nBufferSize);

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = VSICurlSetCreationHeadersFromOptions(headers, papszOptions,
                                                       osFilename.c_str());
        headers = curl_slist_append(headers, osContentLength.c_str());
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("PUT", headers,
                                                      pabyBuffer, nBufferSize));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        NetworkStatisticsLogger::LogPUT(nBufferSize);

        if (!bHasAlreadyHandled409 && response_code == 409)
        {
            bHasAlreadyHandled409 = true;
            CPLDebug(GetDebugKey(), "%s",
                     requestHelper.sWriteFuncData.pBuffer
                         ? requestHelper.sWriteFuncData.pBuffer
                         : "(null)");

            // The blob type is invalid for this operation
            // Delete the file, and retry
            if (DeleteObject(osFilename.c_str()) == 0)
            {
                bRetry = true;
            }
        }
        else if ((response_code != 200 && response_code != 201) ||
                 requestHelper.sWriteFuncHeaderData.pBuffer == nullptr)
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer,
                requestHelper.szCurlErrBuf);
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
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "PutBlock(%d) of %s failed", nPartNumber,
                         osFilename.c_str());
                osBlockId.clear();
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return osBlockId;
}

/************************************************************************/
/*                           PutBlockList()                             */
/************************************************************************/

bool VSIAzureFSHandler::PutBlockList(
    const std::string &osFilename, const std::vector<std::string> &aosBlockIds,
    IVSIS3LikeHandleHelper *poS3HandleHelper, int nMaxRetry,
    double dfRetryDelay)
{
    bool bSuccess = true;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(osFilename.c_str());
    NetworkStatisticsAction oContextAction("PutBlockList");

    std::string osXML =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<BlockList>\n";
    for (const auto &osBlockId : aosBlockIds)
    {
        osXML += "<Latest>" + osBlockId + "</Latest>\n";
    }
    osXML += "</BlockList>\n";

    const std::string osContentLength(
        CPLSPrintf("Content-Length: %d", static_cast<int>(osXML.size())));

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osFilename.c_str()));

    int nRetryCount = 0;
    bool bRetry;
    do
    {
        bRetry = false;

        poS3HandleHelper->AddQueryParameter("comp", "blocklist");

        PutData putData;
        putData.pabyData = reinterpret_cast<const GByte *>(osXML.data());
        putData.nOff = 0;
        putData.nTotalSize = osXML.size();

        CURL *hCurlHandle = curl_easy_init();
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                                   PutData::ReadCallBackBuffer);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, &putData);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                                   static_cast<int>(osXML.size()));
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = curl_slist_append(headers, osContentLength.c_str());
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders(
                         "PUT", headers, osXML.c_str(), osXML.size()));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        NetworkStatisticsLogger::LogPUT(osXML.size());

        if (response_code != 201)
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer,
                requestHelper.szCurlErrBuf);
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
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "PutBlockList of %s  failed", osFilename.c_str());
                bSuccess = false;
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return bSuccess;
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char **VSIAzureFSHandler::GetFileList(const char *pszDirname, int nMaxFiles,
                                      bool *pbGotFileList)
{
    return GetFileList(pszDirname, nMaxFiles, true, pbGotFileList);
}

char **VSIAzureFSHandler::GetFileList(const char *pszDirname, int nMaxFiles,
                                      bool bCacheEntries, bool *pbGotFileList)
{
    if (ENABLE_DEBUG)
        CPLDebug(GetDebugKey(), "GetFileList(%s)", pszDirname);

    *pbGotFileList = false;

    char **papszOptions =
        CSLSetNameValue(nullptr, "MAXFILES", CPLSPrintf("%d", nMaxFiles));
    papszOptions = CSLSetNameValue(papszOptions, "CACHE_ENTRIES",
                                   bCacheEntries ? "YES" : "NO");
    auto dir = OpenDir(pszDirname, 0, papszOptions);
    CSLDestroy(papszOptions);
    if (!dir)
    {
        return nullptr;
    }
    CPLStringList aosFileList;
    while (true)
    {
        auto entry = dir->NextDirEntry();
        if (!entry)
        {
            break;
        }
        aosFileList.AddString(entry->pszName);

        if (nMaxFiles > 0 && aosFileList.size() >= nMaxFiles)
            break;
    }
    delete dir;
    *pbGotFileList = true;
    return aosFileList.StealList();
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char *VSIAzureFSHandler::GetOptions()
{
    static std::string osOptions(
        std::string("<Options>") +
        "  <Option name='AZURE_STORAGE_CONNECTION_STRING' type='string' "
        "description='Connection string that contains account name and "
        "secret key'/>"
        "  <Option name='AZURE_STORAGE_ACCOUNT' type='string' "
        "description='Storage account. To use with AZURE_STORAGE_ACCESS_KEY'/>"
        "  <Option name='AZURE_STORAGE_ACCESS_KEY' type='string' "
        "description='Secret key'/>"
        "  <Option name='AZURE_NO_SIGN_REQUEST' type='boolean' "
        "description='Whether to disable signing of requests' default='NO'/>"
        "  <Option name='VSIAZ_CHUNK_SIZE' type='int' "
        "description='Size in MB for chunks of files that are uploaded' "
        "default='4' min='1' max='4'/>" +
        VSICurlFilesystemHandlerBase::GetOptionsStatic() + "</Options>");
    return osOptions.c_str();
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

char *VSIAzureFSHandler::GetSignedURL(const char *pszFilename,
                                      CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;

    VSIAzureBlobHandleHelper *poHandleHelper =
        VSIAzureBlobHandleHelper::BuildFromURI(
            pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str(), nullptr,
            papszOptions);
    if (poHandleHelper == nullptr)
    {
        return nullptr;
    }

    std::string osRet(poHandleHelper->GetSignedURL(papszOptions));

    delete poHandleHelper;
    return CPLStrdup(osRet.c_str());
}

/************************************************************************/
/*                            OpenDir()                                 */
/************************************************************************/

VSIDIR *VSIAzureFSHandler::OpenDir(const char *pszPath, int nRecurseDepth,
                                   const char *const *papszOptions)
{
    if (nRecurseDepth > 0)
    {
        return VSIFilesystemHandler::OpenDir(pszPath, nRecurseDepth,
                                             papszOptions);
    }

    if (!STARTS_WITH_CI(pszPath, GetFSPrefix().c_str()))
        return nullptr;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("OpenDir");

    std::string osDirnameWithoutPrefix = pszPath + GetFSPrefix().size();
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

    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(osBucket.c_str(), true));
    if (poHandleHelper == nullptr)
    {
        return nullptr;
    }

    VSIDIRAz *dir = new VSIDIRAz(this);
    dir->nRecurseDepth = nRecurseDepth;
    dir->poFS = this;
    dir->poHandleHelper = std::move(poHandleHelper);
    dir->osBucket = std::move(osBucket);
    dir->osObjectKey = std::move(osObjectKey);
    dir->nMaxFiles = atoi(CSLFetchNameValueDef(papszOptions, "MAXFILES", "0"));
    dir->bCacheEntries =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "CACHE_ENTRIES", "YES"));
    dir->m_osFilterPrefix = CSLFetchNameValueDef(papszOptions, "PREFIX", "");
    dir->m_bSynthetizeMissingDirectories = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "SYNTHETIZE_MISSING_DIRECTORIES", "NO"));
    if (!dir->IssueListDir())
    {
        delete dir;
        return nullptr;
    }

    return dir;
}

/************************************************************************/
/*                           VSIAzureHandle()                           */
/************************************************************************/

VSIAzureHandle::VSIAzureHandle(VSIAzureFSHandler *poFSIn,
                               const char *pszFilename,
                               VSIAzureBlobHandleHelper *poHandleHelper)
    : VSICurlHandle(poFSIn, pszFilename, poHandleHelper->GetURLNoKVP().c_str()),
      m_poHandleHelper(poHandleHelper)
{
    m_osQueryString = poHandleHelper->GetSASQueryString();
}

/************************************************************************/
/*                          GetCurlHeaders()                            */
/************************************************************************/

struct curl_slist *
VSIAzureHandle::GetCurlHeaders(const std::string &osVerb,
                               const struct curl_slist *psExistingHeaders)
{
    return m_poHandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                         IsDirectoryFromExists()                      */
/************************************************************************/

bool VSIAzureHandle::IsDirectoryFromExists(const char * /*pszVerb*/,
                                           int response_code)
{
    if (response_code != 404)
        return false;

    std::string osDirname(m_osFilename);
    if (osDirname.size() > poFS->GetFSPrefix().size() &&
        osDirname.back() == '/')
        osDirname.resize(osDirname.size() - 1);
    bool bIsDir;
    if (poFS->ExistsInCacheDirList(osDirname, &bIsDir))
        return bIsDir;

    bool bGotFileList = false;
    char **papszDirContent =
        reinterpret_cast<VSIAzureFSHandler *>(poFS)->GetFileList(
            osDirname.c_str(), 1, false, &bGotFileList);
    CSLDestroy(papszDirContent);
    return bGotFileList;
}

} /* end of namespace cpl */

#endif  // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                      VSIInstallAzureFileHandler()                    */
/************************************************************************/

/*!
 \brief Install /vsiaz/ Microsoft Azure Blob file system handler
 (requires libcurl)

 \verbatim embed:rst
 See :ref:`/vsiaz/ documentation <vsiaz>`
 \endverbatim

 @since GDAL 2.3
 */

void VSIInstallAzureFileHandler(void)
{
    VSIFileManager::InstallHandler("/vsiaz/",
                                   new cpl::VSIAzureFSHandler("/vsiaz/"));
}

#endif /* HAVE_CURL */
