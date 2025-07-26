/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for AWS S3
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2018, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_atomic_ops.h"
#include "cpl_port.h"
#include "cpl_json.h"
#include "cpl_http.h"
#include "cpl_md5.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_time.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_vsil_curl_class.h"

#include <errno.h>

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <set>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "cpl_aws.h"

#ifndef HAVE_CURL

void VSIInstallS3FileHandler(void)
{
    // Not supported.
}

#else

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

#define ENABLE_DEBUG 0

#define unchecked_curl_easy_setopt(handle, opt, param)                         \
    CPL_IGNORE_RET_VAL(curl_easy_setopt(handle, opt, param))

// MebIByte
constexpr int MIB_CONSTANT = 1024 * 1024;

namespace cpl
{

/************************************************************************/
/*                             VSIDIRS3                                 */
/************************************************************************/

struct VSIDIRS3 : public VSIDIRS3Like
{
    VSIDIRS3(const std::string &osDirName, IVSIS3LikeFSHandler *poFSIn)
        : VSIDIRS3Like(osDirName, poFSIn)
    {
    }

    VSIDIRS3(const std::string &osDirName, VSICurlFilesystemHandlerBase *poFSIn)
        : VSIDIRS3Like(osDirName, poFSIn)
    {
    }

    bool IssueListDir() override;
    bool
    AnalyseS3FileList(const std::string &osBaseURL, const char *pszXML,
                      const std::set<std::string> &oSetIgnoredStorageClasses,
                      bool &bIsTruncated);
};

/************************************************************************/
/*                                clear()                               */
/************************************************************************/

void VSIDIRS3Like::clear()
{
    osNextMarker.clear();
    nPos = 0;
    aoEntries.clear();
}

/************************************************************************/
/*                      SynthetizeMissingDirectories()                  */
/************************************************************************/

void VSIDIRWithMissingDirSynthesis::SynthetizeMissingDirectories(
    const std::string &osCurSubdir, bool bAddEntryForThisSubdir)
{
    const auto nLastSlashPos = osCurSubdir.rfind('/');
    if (nLastSlashPos == std::string::npos)
    {
        m_aosSubpathsStack = {osCurSubdir};
    }
    else if (m_aosSubpathsStack.empty())
    {
        SynthetizeMissingDirectories(osCurSubdir.substr(0, nLastSlashPos),
                                     true);

        m_aosSubpathsStack.emplace_back(osCurSubdir);
    }
    else if (osCurSubdir.compare(0, nLastSlashPos, m_aosSubpathsStack.back()) ==
             0)
    {
        m_aosSubpathsStack.emplace_back(osCurSubdir);
    }
    else
    {
        size_t depth = 1;
        for (char c : osCurSubdir)
        {
            if (c == '/')
                depth++;
        }

        while (depth <= m_aosSubpathsStack.size())
            m_aosSubpathsStack.pop_back();

        if (!m_aosSubpathsStack.empty() &&
            osCurSubdir.compare(0, nLastSlashPos, m_aosSubpathsStack.back()) !=
                0)
        {
            SynthetizeMissingDirectories(osCurSubdir.substr(0, nLastSlashPos),
                                         true);
        }

        m_aosSubpathsStack.emplace_back(osCurSubdir);
    }

    if (bAddEntryForThisSubdir)
    {
        aoEntries.push_back(std::make_unique<VSIDIREntry>());
        // cppcheck-suppress constVariableReference
        auto &entry = aoEntries.back();
        entry->pszName = CPLStrdup(osCurSubdir.c_str());
        entry->nMode = S_IFDIR;
        entry->bModeKnown = true;
    }
}

/************************************************************************/
/*                        AnalyseS3FileList()                           */
/************************************************************************/

bool VSIDIRS3::AnalyseS3FileList(
    const std::string &osBaseURL, const char *pszXML,
    const std::set<std::string> &oSetIgnoredStorageClasses, bool &bIsTruncated)
{
#if DEBUG_VERBOSE
    const char *pszDebugPrefix = poS3FS ? poS3FS->GetDebugKey() : "S3";
    CPLDebug(pszDebugPrefix, "%s", pszXML);
#endif

    CPLXMLNode *psTree = CPLParseXMLString(pszXML);
    if (psTree == nullptr)
        return false;
    CPLXMLNode *psListBucketResult = CPLGetXMLNode(psTree, "=ListBucketResult");
    CPLXMLNode *psListAllMyBucketsResultBuckets =
        (psListBucketResult != nullptr)
            ? nullptr
            : CPLGetXMLNode(psTree, "=ListAllMyBucketsResult.Buckets");

    bool ret = true;

    bIsTruncated = false;
    if (psListBucketResult)
    {
        ret = false;
        CPLString osPrefix = CPLGetXMLValue(psListBucketResult, "Prefix", "");
        if (osPrefix.empty())
        {
            // in the case of an empty bucket
            ret = true;
        }
        if (osPrefix.endsWith(m_osFilterPrefix))
        {
            osPrefix.resize(osPrefix.size() - m_osFilterPrefix.size());
        }

        bIsTruncated = CPLTestBool(
            CPLGetXMLValue(psListBucketResult, "IsTruncated", "false"));

        // Count the number of occurrences of a path. Can be 1 or 2. 2 in the
        // case that both a filename and directory exist
        std::map<std::string, int> aoNameCount;
        for (CPLXMLNode *psIter = psListBucketResult->psChild;
             psIter != nullptr; psIter = psIter->psNext)
        {
            if (psIter->eType != CXT_Element)
                continue;
            if (strcmp(psIter->pszValue, "Contents") == 0)
            {
                ret = true;
                const char *pszKey = CPLGetXMLValue(psIter, "Key", nullptr);
                if (pszKey && strlen(pszKey) > osPrefix.size())
                {
                    aoNameCount[pszKey + osPrefix.size()]++;
                }
            }
            else if (strcmp(psIter->pszValue, "CommonPrefixes") == 0)
            {
                const char *pszKey = CPLGetXMLValue(psIter, "Prefix", nullptr);
                if (pszKey &&
                    strncmp(pszKey, osPrefix.c_str(), osPrefix.size()) == 0)
                {
                    std::string osKey = pszKey;
                    if (!osKey.empty() && osKey.back() == '/')
                        osKey.pop_back();
                    if (osKey.size() > osPrefix.size())
                    {
                        ret = true;
                        aoNameCount[osKey.c_str() + osPrefix.size()]++;
                    }
                }
            }
        }

        for (CPLXMLNode *psIter = psListBucketResult->psChild;
             psIter != nullptr; psIter = psIter->psNext)
        {
            if (psIter->eType != CXT_Element)
                continue;
            if (strcmp(psIter->pszValue, "Contents") == 0)
            {
                const char *pszKey = CPLGetXMLValue(psIter, "Key", nullptr);
                if (bIsTruncated && nRecurseDepth < 0 && pszKey)
                {
                    osNextMarker = pszKey;
                }
                if (pszKey && strlen(pszKey) > osPrefix.size())
                {
                    const char *pszStorageClass =
                        CPLGetXMLValue(psIter, "StorageClass", "");
                    if (oSetIgnoredStorageClasses.find(pszStorageClass) !=
                        oSetIgnoredStorageClasses.end())
                    {
                        continue;
                    }

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
                    entry->nSize = static_cast<GUIntBig>(
                        CPLAtoGIntBig(CPLGetXMLValue(psIter, "Size", "0")));
                    entry->bSizeKnown = true;
                    entry->nMode =
                        entry->pszName[0] != 0 &&
                                entry->pszName[strlen(entry->pszName) - 1] ==
                                    '/'
                            ? S_IFDIR
                            : S_IFREG;
                    if (entry->nMode == S_IFDIR &&
                        aoNameCount[entry->pszName] < 2)
                    {
                        entry->pszName[strlen(entry->pszName) - 1] = 0;
                    }
                    entry->bModeKnown = true;

                    std::string ETag = CPLGetXMLValue(psIter, "ETag", "");
                    if (ETag.size() > 2 && ETag[0] == '"' && ETag.back() == '"')
                    {
                        ETag = ETag.substr(1, ETag.size() - 2);
                        entry->papszExtra = CSLSetNameValue(
                            entry->papszExtra, "ETag", ETag.c_str());
                    }

                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMin = 0;
                    int nSec = 0;
                    if (sscanf(CPLGetXMLValue(psIter, "LastModified", ""),
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
                        entry->nMTime = CPLYMDHMSToUnixTime(&brokendowntime);
                        entry->bMTimeKnown = true;
                    }

                    if (nMaxFiles != 1 && bCacheEntries)
                    {
                        FileProp prop;
                        prop.nMode = entry->nMode;
                        prop.eExists = EXIST_YES;
                        prop.bHasComputedFileSize = true;
                        prop.fileSize = entry->nSize;
                        prop.bIsDirectory = (entry->nMode == S_IFDIR);
                        prop.mTime = static_cast<time_t>(entry->nMTime);
                        prop.ETag = std::move(ETag);

                        std::string osCachedFilename =
                            osBaseURL + CPLAWSURLEncode(osPrefix, false) +
                            CPLAWSURLEncode(entry->pszName, false);
#if DEBUG_VERBOSE
                        CPLDebug(pszDebugPrefix, "Cache %s",
                                 osCachedFilename.c_str());
#endif
                        poFS->SetCachedFileProp(osCachedFilename.c_str(), prop);
                    }

                    if (nMaxFiles > 0 &&
                        aoEntries.size() >= static_cast<unsigned>(nMaxFiles))
                        break;
                }
            }
            else if (strcmp(psIter->pszValue, "CommonPrefixes") == 0)
            {
                const char *pszKey = CPLGetXMLValue(psIter, "Prefix", nullptr);
                if (pszKey &&
                    strncmp(pszKey, osPrefix.c_str(), osPrefix.size()) == 0)
                {
                    std::string osKey = pszKey;
                    if (!osKey.empty() && osKey.back() == '/')
                        osKey.pop_back();
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

                        if (nMaxFiles != 1 && bCacheEntries)
                        {
                            FileProp prop;
                            prop.eExists = EXIST_YES;
                            prop.bIsDirectory = true;
                            prop.bHasComputedFileSize = true;
                            prop.fileSize = 0;
                            prop.mTime = 0;
                            prop.nMode = S_IFDIR;

                            std::string osCachedFilename =
                                osBaseURL + CPLAWSURLEncode(osPrefix, false) +
                                CPLAWSURLEncode(entry->pszName, false);
#if DEBUG_VERBOSE
                            CPLDebug(pszDebugPrefix, "Cache %s",
                                     osCachedFilename.c_str());
#endif
                            poFS->SetCachedFileProp(osCachedFilename.c_str(),
                                                    prop);
                        }

                        if (nMaxFiles > 0 &&
                            aoEntries.size() >=
                                static_cast<unsigned>(nMaxFiles))
                            break;
                    }
                }
            }
        }

        if (nRecurseDepth == 0)
        {
            osNextMarker = CPLGetXMLValue(psListBucketResult, "NextMarker", "");
        }
    }
    else if (psListAllMyBucketsResultBuckets != nullptr)
    {
        CPLXMLNode *psIter = psListAllMyBucketsResultBuckets->psChild;
        for (; psIter != nullptr; psIter = psIter->psNext)
        {
            if (psIter->eType != CXT_Element)
                continue;
            if (strcmp(psIter->pszValue, "Bucket") == 0)
            {
                const char *pszName = CPLGetXMLValue(psIter, "Name", nullptr);
                if (pszName)
                {
                    aoEntries.push_back(std::make_unique<VSIDIREntry>());
                    // cppcheck-suppress constVariableReference
                    auto &entry = aoEntries.back();
                    entry->pszName = CPLStrdup(pszName);
                    entry->nMode = S_IFDIR;
                    entry->bModeKnown = true;

                    if (nMaxFiles != 1 && bCacheEntries)
                    {
                        FileProp prop;
                        prop.eExists = EXIST_YES;
                        prop.bIsDirectory = true;
                        prop.bHasComputedFileSize = true;
                        prop.fileSize = 0;
                        prop.mTime = 0;
                        prop.nMode = S_IFDIR;

                        std::string osCachedFilename =
                            osBaseURL + CPLAWSURLEncode(pszName, false);
#if DEBUG_VERBOSE
                        CPLDebug(pszDebugPrefix, "Cache %s",
                                 osCachedFilename.c_str());
#endif
                        poFS->SetCachedFileProp(osCachedFilename.c_str(), prop);
                    }
                }
            }
        }
    }

    CPLDestroyXMLNode(psTree);
    return ret;
}

/************************************************************************/
/*                          IssueListDir()                              */
/************************************************************************/

bool VSIDIRS3::IssueListDir()
{
    CPLString osMaxKeys = CPLGetConfigOption("AWS_MAX_KEYS", "");
    if (nMaxFiles > 0 && nMaxFiles <= 100 &&
        (osMaxKeys.empty() || nMaxFiles < atoi(osMaxKeys)))
    {
        osMaxKeys.Printf("%d", nMaxFiles);
    }

    NetworkStatisticsFileSystem oContextFS(poS3FS->GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("ListBucket");

    const std::string l_osNextMarker(osNextMarker);
    clear();

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(m_osDirName.c_str()));

    while (true)
    {
        poHandleHelper->ResetQueryParameters();
        const std::string osBaseURL(poHandleHelper->GetURL());

        CURL *hCurlHandle = curl_easy_init();

        if (!osBucket.empty())
        {
            if (nRecurseDepth == 0)
                poHandleHelper->AddQueryParameter("delimiter", "/");
            if (!l_osNextMarker.empty())
                poHandleHelper->AddQueryParameter("marker", l_osNextMarker);
            if (!osMaxKeys.empty())
                poHandleHelper->AddQueryParameter("max-keys", osMaxKeys);
            if (!osObjectKey.empty())
                poHandleHelper->AddQueryParameter(
                    "prefix", osObjectKey + "/" + m_osFilterPrefix);
            else if (!m_osFilterPrefix.empty())
                poHandleHelper->AddQueryParameter("prefix", m_osFilterPrefix);
        }

        struct curl_slist *headers =
            VSICurlSetOptions(hCurlHandle, poHandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List());

        headers = VSICurlMergeHeaders(
            headers, poHandleHelper->GetCurlHeaders("GET", headers));
        // Disable automatic redirection
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_FOLLOWLOCATION, 0);

        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, nullptr);

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, poFS, poHandleHelper.get());

        NetworkStatisticsLogger::LogGET(requestHelper.sWriteFuncData.nSize);

        if (response_code != 200 ||
            requestHelper.sWriteFuncData.pBuffer == nullptr)
        {
            if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                poHandleHelper->CanRestartOnError(
                    requestHelper.sWriteFuncData.pBuffer,
                    requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                // nothing to do
            }
            else
            {
                CPLDebug(poS3FS->GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                curl_easy_cleanup(hCurlHandle);
                return false;
            }
        }
        else
        {
            bool bIsTruncated;
            bool ret = AnalyseS3FileList(
                osBaseURL, requestHelper.sWriteFuncData.pBuffer,
                VSICurlFilesystemHandlerBase::GetS3IgnoredStorageClasses(),
                bIsTruncated);

            curl_easy_cleanup(hCurlHandle);
            return ret;
        }

        curl_easy_cleanup(hCurlHandle);
    }
}

/************************************************************************/
/*                           NextDirEntry()                             */
/************************************************************************/

const VSIDIREntry *VSIDIRS3Like::NextDirEntry()
{
    constexpr int ARBITRARY_LIMIT = 10;
    for (int i = 0; i < ARBITRARY_LIMIT; ++i)
    {
        if (nPos < static_cast<int>(aoEntries.size()))
        {
            auto &entry = aoEntries[nPos];
            if (osBucket.empty())
            {
                if (m_subdir)
                {
                    if (auto subentry = m_subdir->NextDirEntry())
                    {
                        const std::string name = std::string(entry->pszName)
                                                     .append("/")
                                                     .append(subentry->pszName);
                        CPLFree(const_cast<VSIDIREntry *>(subentry)->pszName);
                        const_cast<VSIDIREntry *>(subentry)->pszName =
                            CPLStrdup(name.c_str());
                        return subentry;
                    }
                    m_subdir.reset();
                    nPos++;
                    continue;
                }
                else if (nRecurseDepth != 0)
                {
                    m_subdir.reset(VSIOpenDir(std::string(poFS->GetFSPrefix())
                                                  .append(entry->pszName)
                                                  .c_str(),
                                              nRecurseDepth - 1, nullptr));
                    if (m_subdir)
                        return entry.get();
                }
            }
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
    CPLError(CE_Failure, CPLE_AppDefined,
             "More than %d consecutive List Blob "
             "requests returning no blobs",
             ARBITRARY_LIMIT);
    return nullptr;
}

/************************************************************************/
/*                          AnalyseS3FileList()                         */
/************************************************************************/

bool VSICurlFilesystemHandlerBase::AnalyseS3FileList(
    const std::string &osBaseURL, const char *pszXML, CPLStringList &osFileList,
    int nMaxFiles, const std::set<std::string> &oSetIgnoredStorageClasses,
    bool &bIsTruncated)
{
    VSIDIRS3 oDir(std::string(), this);
    oDir.nMaxFiles = nMaxFiles;
    bool ret = oDir.AnalyseS3FileList(osBaseURL, pszXML,
                                      oSetIgnoredStorageClasses, bIsTruncated);
    for (const auto &entry : oDir.aoEntries)
    {
        osFileList.AddString(entry->pszName);
    }
    return ret;
}

/************************************************************************/
/*                         VSIS3FSHandler                               */
/************************************************************************/

class VSIS3FSHandler final : public IVSIS3LikeFSHandlerWithMultipartUpload
{
    CPL_DISALLOW_COPY_ASSIGN(VSIS3FSHandler)

    const std::string m_osPrefix;
    std::set<std::string> DeleteObjects(const char *pszBucket,
                                        const char *pszXML);

  protected:
    VSICurlHandle *CreateFileHandle(const char *pszFilename) override;
    std::string
    GetURLFromFilename(const std::string &osFilename) const override;

    const char *GetDebugKey() const override
    {
        return "S3";
    }

    IVSIS3LikeHandleHelper *CreateHandleHelper(const char *pszURI,
                                               bool bAllowNoObject) override;

    std::string GetFSPrefix() const override
    {
        return m_osPrefix;
    }

    void ClearCache() override;

    bool IsAllowedHeaderForObjectCreation(const char *pszHeaderName) override
    {
        return STARTS_WITH(pszHeaderName, "x-amz-");
    }

    VSIVirtualHandleUniquePtr
    CreateWriteHandle(const char *pszFilename,
                      CSLConstList papszOptions) override;

  public:
    explicit VSIS3FSHandler(const char *pszPrefix) : m_osPrefix(pszPrefix)
    {
    }

    ~VSIS3FSHandler() override;

    const char *GetOptions() override;

    char *GetSignedURL(const char *pszFilename,
                       CSLConstList papszOptions) override;

    int *UnlinkBatch(CSLConstList papszFiles) override;

    int *DeleteObjectBatch(CSLConstList papszFilesOrDirs) override
    {
        return UnlinkBatch(papszFilesOrDirs);
    }

    int RmdirRecursive(const char *pszDirname) override;

    char **GetFileMetadata(const char *pszFilename, const char *pszDomain,
                           CSLConstList papszOptions) override;

    bool SetFileMetadata(const char *pszFilename, CSLConstList papszMetadata,
                         const char *pszDomain,
                         CSLConstList papszOptions) override;

    std::string
    GetStreamingFilename(const std::string &osFilename) const override;

    VSIFilesystemHandler *Duplicate(const char *pszPrefix) override
    {
        return new VSIS3FSHandler(pszPrefix);
    }

    bool SupportsMultipartAbort() const override
    {
        return true;
    }
};

/************************************************************************/
/*                            VSIS3Handle                               */
/************************************************************************/

class VSIS3Handle final : public IVSIS3LikeHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIS3Handle)

    VSIS3HandleHelper *m_poS3HandleHelper = nullptr;

  protected:
    struct curl_slist *
    GetCurlHeaders(const std::string &osVerb,
                   const struct curl_slist *psExistingHeaders) override;
    bool CanRestartOnError(const char *, const char *, bool) override;

    bool AllowAutomaticRedirection() override
    {
        return m_poS3HandleHelper->AllowAutomaticRedirection();
    }

  public:
    VSIS3Handle(VSIS3FSHandler *poFS, const char *pszFilename,
                VSIS3HandleHelper *poS3HandleHelper);
    ~VSIS3Handle() override;
};

/************************************************************************/
/*                      VSIMultipartWriteHandle()                       */
/************************************************************************/

VSIMultipartWriteHandle::VSIMultipartWriteHandle(
    IVSIS3LikeFSHandlerWithMultipartUpload *poFS, const char *pszFilename,
    IVSIS3LikeHandleHelper *poS3HandleHelper, CSLConstList papszOptions)
    : m_poFS(poFS), m_osFilename(pszFilename),
      m_poS3HandleHelper(poS3HandleHelper), m_aosOptions(papszOptions),
      m_aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename)),
      m_oRetryParameters(m_aosHTTPOptions)
{
    // AWS S3, OSS and GCS can use the multipart upload mechanism, which has
    // the advantage of being retryable in case of errors.
    // Swift only supports the "Transfer-Encoding: chunked" PUT mechanism.
    // So two different implementations.

    const char *pszChunkSize = m_aosOptions.FetchNameValue("CHUNK_SIZE");
    if (pszChunkSize)
        m_nBufferSize = poFS->GetUploadChunkSizeInBytes(
            pszFilename, CPLSPrintf(CPL_FRMT_GIB, CPLAtoGIntBig(pszChunkSize) *
                                                      MIB_CONSTANT));
    else
        m_nBufferSize = poFS->GetUploadChunkSizeInBytes(pszFilename, nullptr);

    m_pabyBuffer = static_cast<GByte *>(VSIMalloc(m_nBufferSize));
    if (m_pabyBuffer == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot allocate working buffer for %s",
                 m_poFS->GetFSPrefix().c_str());
    }
}

/************************************************************************/
/*                      GetUploadChunkSizeInBytes()                     */
/************************************************************************/

size_t IVSIS3LikeFSHandlerWithMultipartUpload::GetUploadChunkSizeInBytes(
    const char *pszFilename, const char *pszSpecifiedValInBytes)
{
    size_t nChunkSize = 0;

    const char *pszChunkSizeBytes =
        pszSpecifiedValInBytes ? pszSpecifiedValInBytes :
                               // For testing only !
            VSIGetPathSpecificOption(pszFilename,
                                     std::string("VSI")
                                         .append(GetDebugKey())
                                         .append("_CHUNK_SIZE_BYTES")
                                         .c_str(),
                                     nullptr);
    if (pszChunkSizeBytes)
    {
        const auto nChunkSizeInt = CPLAtoGIntBig(pszChunkSizeBytes);
        if (nChunkSizeInt <= 0)
        {
            nChunkSize =
                static_cast<size_t>(GetDefaultPartSizeInMiB()) * MIB_CONSTANT;
        }
        else if (nChunkSizeInt >
                 static_cast<int64_t>(GetMaximumPartSizeInMiB()) * MIB_CONSTANT)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Specified chunk size too large. Clamping to %d MiB",
                     GetMaximumPartSizeInMiB());
            nChunkSize =
                static_cast<size_t>(GetMaximumPartSizeInMiB()) * MIB_CONSTANT;
        }
        else
            nChunkSize = static_cast<size_t>(nChunkSizeInt);
    }
    else
    {
        const int nChunkSizeMiB = atoi(VSIGetPathSpecificOption(
            pszFilename,
            std::string("VSI")
                .append(GetDebugKey())
                .append("_CHUNK_SIZE")
                .c_str(),
            CPLSPrintf("%d", GetDefaultPartSizeInMiB())));
        if (nChunkSizeMiB <= 0)
            nChunkSize = 0;
        else if (nChunkSizeMiB > GetMaximumPartSizeInMiB())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Specified chunk size too large. Clamping to %d MiB",
                     GetMaximumPartSizeInMiB());
            nChunkSize =
                static_cast<size_t>(GetMaximumPartSizeInMiB()) * MIB_CONSTANT;
        }
        else
            nChunkSize = static_cast<size_t>(nChunkSizeMiB) * MIB_CONSTANT;
    }

    return nChunkSize;
}

/************************************************************************/
/*                     ~VSIMultipartWriteHandle()                       */
/************************************************************************/

VSIMultipartWriteHandle::~VSIMultipartWriteHandle()
{
    VSIMultipartWriteHandle::Close();
    delete m_poS3HandleHelper;
    CPLFree(m_pabyBuffer);
    CPLFree(m_sWriteFuncHeaderData.pBuffer);
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIMultipartWriteHandle::Seek(vsi_l_offset nOffset, int nWhence)
{
    if (!((nWhence == SEEK_SET && nOffset == m_nCurOffset) ||
          (nWhence == SEEK_CUR && nOffset == 0) ||
          (nWhence == SEEK_END && nOffset == 0)))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Seek not supported on writable %s files",
                 m_poFS->GetFSPrefix().c_str());
        m_bError = true;
        return -1;
    }
    return 0;
}

/************************************************************************/
/*                               Tell()                                 */
/************************************************************************/

vsi_l_offset VSIMultipartWriteHandle::Tell()
{
    return m_nCurOffset;
}

/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

size_t VSIMultipartWriteHandle::Read(void * /* pBuffer */, size_t /* nSize */,
                                     size_t /* nMemb */)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Read not supported on writable %s files",
             m_poFS->GetFSPrefix().c_str());
    m_bError = true;
    return 0;
}

/************************************************************************/
/*                        InitiateMultipartUpload()                     */
/************************************************************************/

std::string IVSIS3LikeFSHandlerWithMultipartUpload::InitiateMultipartUpload(
    const std::string &osFilename, IVSIS3LikeHandleHelper *poS3HandleHelper,
    const CPLHTTPRetryParameters &oRetryParameters, CSLConstList papszOptions)
{
    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(osFilename.c_str());
    NetworkStatisticsAction oContextAction("InitiateMultipartUpload");

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osFilename.c_str()));

    std::string osUploadID;
    bool bRetry;
    CPLHTTPRetryContext oRetryContext(oRetryParameters);
    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("uploads", "");
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = VSICurlSetCreationHeadersFromOptions(headers, papszOptions,
                                                       osFilename.c_str());
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("POST", headers));
        headers = curl_slist_append(
            headers, "Content-Length: 0");  // Required by GCS in HTTP 1.1

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        NetworkStatisticsLogger::LogPOST(0, requestHelper.sWriteFuncData.nSize);

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
                         poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "InitiateMultipartUpload of %s failed",
                         osFilename.c_str());
            }
        }
        else
        {
            InvalidateCachedData(poS3HandleHelper->GetURL().c_str());
            InvalidateDirContent(CPLGetDirnameSafe(osFilename.c_str()));

            CPLXMLNode *psNode =
                CPLParseXMLString(requestHelper.sWriteFuncData.pBuffer);
            if (psNode)
            {
                osUploadID = CPLGetXMLValue(
                    psNode, "=InitiateMultipartUploadResult.UploadId", "");
                CPLDebug(GetDebugKey(), "UploadId: %s", osUploadID.c_str());
                CPLDestroyXMLNode(psNode);
            }
            if (osUploadID.empty())
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "InitiateMultipartUpload of %s failed: cannot get UploadId",
                    osFilename.c_str());
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);
    return osUploadID;
}

/************************************************************************/
/*                           UploadPart()                               */
/************************************************************************/

bool VSIMultipartWriteHandle::UploadPart()
{
    ++m_nPartNumber;
    if (m_nPartNumber > m_poFS->GetMaximumPartCount())
    {
        m_bError = true;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%d parts have been uploaded for %s failed. "
                 "This is the maximum. "
                 "Increase VSI%s_CHUNK_SIZE to a higher value (e.g. 500 for "
                 "500 MiB)",
                 m_poFS->GetMaximumPartCount(), m_osFilename.c_str(),
                 m_poFS->GetDebugKey());
        return false;
    }
    const std::string osEtag = m_poFS->UploadPart(
        m_osFilename, m_nPartNumber, m_osUploadID,
        static_cast<vsi_l_offset>(m_nBufferSize) * (m_nPartNumber - 1),
        m_pabyBuffer, m_nBufferOff, m_poS3HandleHelper, m_oRetryParameters,
        nullptr);
    m_nBufferOff = 0;
    if (!osEtag.empty())
    {
        m_aosEtags.push_back(osEtag);
    }
    return !osEtag.empty();
}

std::string IVSIS3LikeFSHandlerWithMultipartUpload::UploadPart(
    const std::string &osFilename, int nPartNumber,
    const std::string &osUploadID, vsi_l_offset /* nPosition */,
    const void *pabyBuffer, size_t nBufferSize,
    IVSIS3LikeHandleHelper *poS3HandleHelper,
    const CPLHTTPRetryParameters &oRetryParameters,
    CSLConstList /* papszOptions */)
{
    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(osFilename.c_str());
    NetworkStatisticsAction oContextAction("UploadPart");

    bool bRetry;
    CPLHTTPRetryContext oRetryContext(oRetryParameters);
    std::string osEtag;

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osFilename.c_str()));

    do
    {
        bRetry = false;

        CURL *hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("partNumber",
                                            CPLSPrintf("%d", nPartNumber));
        poS3HandleHelper->AddQueryParameter("uploadId", osUploadID);
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
                              aosHTTPOptions));
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("PUT", headers,
                                                      pabyBuffer, nBufferSize));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        NetworkStatisticsLogger::LogPUT(nBufferSize);

        if (response_code != 200 ||
            requestHelper.sWriteFuncHeaderData.pBuffer == nullptr)
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
                         poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "UploadPart(%d) of %s failed", nPartNumber,
                         osFilename.c_str());
            }
        }
        else
        {
            const CPLString osHeader(
                requestHelper.sWriteFuncHeaderData.pBuffer);
            const size_t nPos = osHeader.ifind("ETag: ");
            if (nPos != std::string::npos)
            {
                osEtag = osHeader.substr(nPos + strlen("ETag: "));
                const size_t nPosEOL = osEtag.find("\r");
                if (nPosEOL != std::string::npos)
                    osEtag.resize(nPosEOL);
                CPLDebug(GetDebugKey(), "Etag for part %d is %s", nPartNumber,
                         osEtag.c_str());
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "UploadPart(%d) of %s (uploadId = %s) failed",
                         nPartNumber, osFilename.c_str(), osUploadID.c_str());
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return osEtag;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIMultipartWriteHandle::Write(const void *pBuffer, size_t nSize,
                                      size_t nMemb)
{
    if (m_bError)
        return 0;

    size_t nBytesToWrite = nSize * nMemb;
    if (nBytesToWrite == 0)
        return 0;

    const GByte *pabySrcBuffer = reinterpret_cast<const GByte *>(pBuffer);
    while (nBytesToWrite > 0)
    {
        const size_t nToWriteInBuffer =
            std::min(m_nBufferSize - m_nBufferOff, nBytesToWrite);
        memcpy(m_pabyBuffer + m_nBufferOff, pabySrcBuffer, nToWriteInBuffer);
        pabySrcBuffer += nToWriteInBuffer;
        m_nBufferOff += nToWriteInBuffer;
        m_nCurOffset += nToWriteInBuffer;
        nBytesToWrite -= nToWriteInBuffer;
        if (m_nBufferOff == m_nBufferSize)
        {
            if (m_nCurOffset == m_nBufferSize)
            {
                m_osUploadID = m_poFS->InitiateMultipartUpload(
                    m_osFilename, m_poS3HandleHelper, m_oRetryParameters,
                    m_aosOptions.List());
                if (m_osUploadID.empty())
                {
                    m_bError = true;
                    return 0;
                }
            }
            if (!UploadPart())
            {
                m_bError = true;
                return 0;
            }
            m_nBufferOff = 0;
        }
    }
    return nMemb;
}

/************************************************************************/
/*                    InvalidateParentDirectory()                       */
/************************************************************************/

void VSIMultipartWriteHandle::InvalidateParentDirectory()
{
    m_poFS->InvalidateCachedData(m_poS3HandleHelper->GetURL().c_str());

    std::string osFilenameWithoutSlash(m_osFilename);
    if (!osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/')
        osFilenameWithoutSlash.pop_back();
    m_poFS->InvalidateDirContent(
        CPLGetDirnameSafe(osFilenameWithoutSlash.c_str()));
}

/************************************************************************/
/*                           DoSinglePartPUT()                          */
/************************************************************************/

bool VSIMultipartWriteHandle::DoSinglePartPUT()
{
    bool bSuccess = true;
    bool bRetry;
    CPLHTTPRetryContext oRetryContext(m_oRetryParameters);

    NetworkStatisticsFileSystem oContextFS(m_poFS->GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(m_osFilename.c_str());
    NetworkStatisticsAction oContextAction("Write");

    do
    {
        bRetry = false;

        PutData putData;
        putData.pabyData = m_pabyBuffer;
        putData.nOff = 0;
        putData.nTotalSize = m_nBufferOff;

        CURL *hCurlHandle = curl_easy_init();
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                                   PutData::ReadCallBackBuffer);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, &putData);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                                   m_nBufferOff);

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, m_poS3HandleHelper->GetURL().c_str(),
                              m_aosHTTPOptions.List()));
        headers = VSICurlSetCreationHeadersFromOptions(
            headers, m_aosOptions.List(), m_osFilename.c_str());
        headers = VSICurlMergeHeaders(
            headers, m_poS3HandleHelper->GetCurlHeaders(
                         "PUT", headers, m_pabyBuffer, m_nBufferOff));
        headers = curl_slist_append(headers, "Expect: 100-continue");

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, m_poFS, m_poS3HandleHelper);

        NetworkStatisticsLogger::LogPUT(m_nBufferOff);

        if (response_code != 200 && response_code != 201)
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
                         m_poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     m_poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug("S3", "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "DoSinglePartPUT of %s failed", m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            InvalidateParentDirectory();
        }

        if (requestHelper.sWriteFuncHeaderData.pBuffer != nullptr)
        {
            const char *pzETag =
                strstr(requestHelper.sWriteFuncHeaderData.pBuffer, "ETag: \"");
            if (pzETag)
            {
                pzETag += strlen("ETag: \"");
                const char *pszEndOfETag = strchr(pzETag, '"');
                if (pszEndOfETag)
                {
                    FileProp oFileProp;
                    oFileProp.eExists = EXIST_YES;
                    oFileProp.fileSize = m_nBufferOff;
                    oFileProp.bHasComputedFileSize = true;
                    oFileProp.ETag.assign(pzETag, pszEndOfETag - pzETag);
                    m_poFS->SetCachedFileProp(
                        m_poFS->GetURLFromFilename(m_osFilename.c_str())
                            .c_str(),
                        oFileProp);
                }
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);
    return bSuccess;
}

/************************************************************************/
/*                        CompleteMultipart()                           */
/************************************************************************/

bool IVSIS3LikeFSHandlerWithMultipartUpload::CompleteMultipart(
    const std::string &osFilename, const std::string &osUploadID,
    const std::vector<std::string> &aosEtags, vsi_l_offset /* nTotalSize */,
    IVSIS3LikeHandleHelper *poS3HandleHelper,
    const CPLHTTPRetryParameters &oRetryParameters)
{
    bool bSuccess = true;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(osFilename.c_str());
    NetworkStatisticsAction oContextAction("CompleteMultipart");

    std::string osXML = "<CompleteMultipartUpload>\n";
    for (size_t i = 0; i < aosEtags.size(); i++)
    {
        osXML += "<Part>\n";
        osXML +=
            CPLSPrintf("<PartNumber>%d</PartNumber>", static_cast<int>(i + 1));
        osXML += "<ETag>" + aosEtags[i] + "</ETag>";
        osXML += "</Part>\n";
    }
    osXML += "</CompleteMultipartUpload>\n";

#ifdef DEBUG_VERBOSE
    CPLDebug(GetDebugKey(), "%s", osXML.c_str());
#endif

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osFilename.c_str()));

    CPLHTTPRetryContext oRetryContext(oRetryParameters);
    bool bRetry;
    do
    {
        bRetry = false;

        PutData putData;
        putData.pabyData = reinterpret_cast<const GByte *>(osXML.data());
        putData.nOff = 0;
        putData.nTotalSize = osXML.size();

        CURL *hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("uploadId", osUploadID);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                                   PutData::ReadCallBackBuffer);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, &putData);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                                   static_cast<int>(osXML.size()));
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders(
                         "POST", headers, osXML.c_str(), osXML.size()));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        NetworkStatisticsLogger::LogPOST(
            osXML.size(), requestHelper.sWriteFuncHeaderData.nSize);

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
                         poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug("S3", "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "CompleteMultipart of %s (uploadId=%s) failed",
                         osFilename.c_str(), osUploadID.c_str());
                bSuccess = false;
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return bSuccess;
}

/************************************************************************/
/*                          AbortMultipart()                            */
/************************************************************************/

bool IVSIS3LikeFSHandlerWithMultipartUpload::AbortMultipart(
    const std::string &osFilename, const std::string &osUploadID,
    IVSIS3LikeHandleHelper *poS3HandleHelper,
    const CPLHTTPRetryParameters &oRetryParameters)
{
    bool bSuccess = true;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(osFilename.c_str());
    NetworkStatisticsAction oContextAction("AbortMultipart");

    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osFilename.c_str()));

    CPLHTTPRetryContext oRetryContext(oRetryParameters);
    bool bRetry;
    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("uploadId", osUploadID);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST,
                                   "DELETE");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("DELETE", headers));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        NetworkStatisticsLogger::LogDELETE();

        if (response_code != 204)
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
                         poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug("S3", "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "AbortMultipart of %s (uploadId=%s) failed",
                         osFilename.c_str(), osUploadID.c_str());
                bSuccess = false;
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return bSuccess;
}

/************************************************************************/
/*                       AbortPendingUploads()                          */
/************************************************************************/

bool IVSIS3LikeFSHandlerWithMultipartUpload::AbortPendingUploads(
    const char *pszFilename)
{
    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(pszFilename);
    NetworkStatisticsAction oContextAction("AbortPendingUploads");

    std::string osDirnameWithoutPrefix = pszFilename + GetFSPrefix().size();
    if (!osDirnameWithoutPrefix.empty() && osDirnameWithoutPrefix.back() == '/')
    {
        osDirnameWithoutPrefix.pop_back();
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
        return false;
    }

    // For debugging purposes
    const int nMaxUploads = std::min(
        1000, atoi(CPLGetConfigOption("CPL_VSIS3_LIST_UPLOADS_MAX", "1000")));

    std::string osKeyMarker;
    std::string osUploadIdMarker;
    std::vector<std::pair<std::string, std::string>> aosUploads;

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);

    // First pass: collect (key, uploadId)
    while (true)
    {
        CPLHTTPRetryContext oRetryContext(oRetryParameters);
        bool bRetry;
        std::string osXML;
        bool bSuccess = true;

        do
        {
            bRetry = false;
            CURL *hCurlHandle = curl_easy_init();
            poHandleHelper->AddQueryParameter("uploads", "");
            if (!osObjectKey.empty())
            {
                poHandleHelper->AddQueryParameter("prefix", osObjectKey);
            }
            if (!osKeyMarker.empty())
            {
                poHandleHelper->AddQueryParameter("key-marker", osKeyMarker);
            }
            if (!osUploadIdMarker.empty())
            {
                poHandleHelper->AddQueryParameter("upload-id-marker",
                                                  osUploadIdMarker);
            }
            poHandleHelper->AddQueryParameter("max-uploads",
                                              CPLSPrintf("%d", nMaxUploads));

            struct curl_slist *headers = static_cast<struct curl_slist *>(
                CPLHTTPSetOptions(hCurlHandle, poHandleHelper->GetURL().c_str(),
                                  aosHTTPOptions.List()));
            headers = VSICurlMergeHeaders(
                headers, poHandleHelper->GetCurlHeaders("GET", headers));

            CurlRequestHelper requestHelper;
            const long response_code = requestHelper.perform(
                hCurlHandle, headers, this, poHandleHelper.get());

            NetworkStatisticsLogger::LogGET(requestHelper.sWriteFuncData.nSize);

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
                else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                         poHandleHelper->CanRestartOnError(
                             requestHelper.sWriteFuncData.pBuffer,
                             requestHelper.sWriteFuncHeaderData.pBuffer, false))
                {
                    bRetry = true;
                }
                else
                {
                    CPLDebug(GetDebugKey(), "%s",
                             requestHelper.sWriteFuncData.pBuffer
                                 ? requestHelper.sWriteFuncData.pBuffer
                                 : "(null)");
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "ListMultipartUpload failed");
                    bSuccess = false;
                }
            }
            else
            {
                osXML = requestHelper.sWriteFuncData.pBuffer
                            ? requestHelper.sWriteFuncData.pBuffer
                            : "(null)";
            }

            curl_easy_cleanup(hCurlHandle);
        } while (bRetry);

        if (!bSuccess)
            return false;

#ifdef DEBUG_VERBOSE
        CPLDebug(GetDebugKey(), "%s", osXML.c_str());
#endif

        CPLXMLTreeCloser oTree(CPLParseXMLString(osXML.c_str()));
        if (!oTree)
            return false;

        const CPLXMLNode *psRoot =
            CPLGetXMLNode(oTree.get(), "=ListMultipartUploadsResult");
        if (!psRoot)
            return false;

        for (const CPLXMLNode *psIter = psRoot->psChild; psIter;
             psIter = psIter->psNext)
        {
            if (!(psIter->eType == CXT_Element &&
                  strcmp(psIter->pszValue, "Upload") == 0))
                continue;
            const char *pszKey = CPLGetXMLValue(psIter, "Key", nullptr);
            const char *pszUploadId =
                CPLGetXMLValue(psIter, "UploadId", nullptr);
            if (pszKey && pszUploadId)
            {
                aosUploads.emplace_back(
                    std::pair<std::string, std::string>(pszKey, pszUploadId));
            }
        }

        const bool bIsTruncated =
            CPLTestBool(CPLGetXMLValue(psRoot, "IsTruncated", "false"));
        if (!bIsTruncated)
            break;

        osKeyMarker = CPLGetXMLValue(psRoot, "NextKeyMarker", "");
        osUploadIdMarker = CPLGetXMLValue(psRoot, "NextUploadIdMarker", "");
    }

    // Second pass: actually abort those pending uploads
    bool bRet = true;
    for (const auto &pair : aosUploads)
    {
        const auto &osKey = pair.first;
        const auto &osUploadId = pair.second;
        CPLDebug(GetDebugKey(), "Abort %s/%s", osKey.c_str(),
                 osUploadId.c_str());

        auto poSubHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
            CreateHandleHelper((osBucket + '/' + osKey).c_str(), true));
        if (poSubHandleHelper == nullptr)
        {
            bRet = false;
            continue;
        }

        if (!AbortMultipart(GetFSPrefix() + osBucket + '/' + osKey, osUploadId,
                            poSubHandleHelper.get(), oRetryParameters))
        {
            bRet = false;
        }
    }

    return bRet;
}

/************************************************************************/
/*                                 Close()                              */
/************************************************************************/

int VSIMultipartWriteHandle::Close()
{
    int nRet = 0;
    if (!m_bClosed)
    {
        m_bClosed = true;
        if (m_osUploadID.empty())
        {
            if (!m_bError && !DoSinglePartPUT())
                nRet = -1;
        }
        else
        {
            if (m_bError)
            {
                if (!m_poFS->AbortMultipart(m_osFilename, m_osUploadID,
                                            m_poS3HandleHelper,
                                            m_oRetryParameters))
                    nRet = -1;
            }
            else if (m_nBufferOff > 0 && !UploadPart())
                nRet = -1;
            else if (m_poFS->CompleteMultipart(
                         m_osFilename, m_osUploadID, m_aosEtags, m_nCurOffset,
                         m_poS3HandleHelper, m_oRetryParameters))
            {
                InvalidateParentDirectory();
            }
            else
                nRet = -1;
        }
    }
    return nRet;
}

/************************************************************************/
/*                          CreateWriteHandle()                         */
/************************************************************************/

VSIVirtualHandleUniquePtr
VSIS3FSHandler::CreateWriteHandle(const char *pszFilename,
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
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *VSICurlFilesystemHandlerBaseWritable::Open(
    const char *pszFilename, const char *pszAccess, bool bSetError,
    CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;

    if (strchr(pszAccess, '+'))
    {
        if (!SupportsRandomWrite(pszFilename, true))
        {
            if (bSetError)
            {
                VSIError(
                    VSIE_FileError,
                    "%s not supported for %s, unless "
                    "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE is set to YES",
                    pszAccess, GetFSPrefix().c_str());
            }
            errno = EACCES;
            return nullptr;
        }

        const std::string osTmpFilename(
            CPLGenerateTempFilenameSafe(CPLGetFilename(pszFilename)));
        if (strchr(pszAccess, 'r'))
        {
            auto poExistingFile =
                VSIVirtualHandleUniquePtr(VSIFOpenL(pszFilename, "rb"));
            if (!poExistingFile)
            {
                return nullptr;
            }
            if (VSICopyFile(pszFilename, osTmpFilename.c_str(),
                            poExistingFile.get(), static_cast<vsi_l_offset>(-1),
                            nullptr, nullptr, nullptr) != 0)
            {
                VSIUnlink(osTmpFilename.c_str());
                return nullptr;
            }
        }

        auto fpTemp = VSIVirtualHandleUniquePtr(
            VSIFOpenL(osTmpFilename.c_str(), pszAccess));
        VSIUnlink(osTmpFilename.c_str());
        if (!fpTemp)
        {
            return nullptr;
        }

        auto poWriteHandle = CreateWriteHandle(pszFilename, papszOptions);
        if (!poWriteHandle)
        {
            return nullptr;
        }

        return VSICreateUploadOnCloseFile(std::move(poWriteHandle),
                                          std::move(fpTemp), osTmpFilename);
    }
    else if (strchr(pszAccess, 'w') || strchr(pszAccess, 'a'))
    {
        return CreateWriteHandle(pszFilename, papszOptions).release();
    }

    if (std::string(pszFilename).back() != '/')
    {
        // If there's directory content for the directory where this file
        // belongs to, use it to detect if the object does not exist
        CachedDirList cachedDirList;
        const std::string osDirname(CPLGetDirnameSafe(pszFilename));
        if (STARTS_WITH_CI(osDirname.c_str(), GetFSPrefix().c_str()) &&
            GetCachedDirList(osDirname.c_str(), cachedDirList) &&
            cachedDirList.bGotFileList)
        {
            const std::string osFilenameOnly(CPLGetFilename(pszFilename));
            bool bFound = false;
            for (int i = 0; i < cachedDirList.oFileList.size(); i++)
            {
                if (cachedDirList.oFileList[i] == osFilenameOnly)
                {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                return nullptr;
            }
        }
    }

    return VSICurlFilesystemHandlerBase::Open(pszFilename, pszAccess, bSetError,
                                              papszOptions);
}

/************************************************************************/
/*                        SupportsRandomWrite()                         */
/************************************************************************/

bool VSICurlFilesystemHandlerBaseWritable::SupportsRandomWrite(
    const char *pszPath, bool bAllowLocalTempFile)
{
    return bAllowLocalTempFile &&
           CPLTestBool(VSIGetPathSpecificOption(
               pszPath, "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "NO"));
}

/************************************************************************/
/*                         ~VSIS3FSHandler()                            */
/************************************************************************/

VSIS3FSHandler::~VSIS3FSHandler()
{
    VSIS3FSHandler::ClearCache();
    VSIS3HandleHelper::CleanMutex();
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIS3FSHandler::ClearCache()
{
    VSICurlFilesystemHandlerBase::ClearCache();

    VSIS3UpdateParams::ClearCache();

    VSIS3HandleHelper::ClearCache();
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char *VSIS3FSHandler::GetOptions()
{
    static std::string osOptions(
        std::string("<Options>")
            .append(
                "  <Option name='AWS_SECRET_ACCESS_KEY' type='string' "
                "description='Secret access key. To use with "
                "AWS_ACCESS_KEY_ID'/>"
                "  <Option name='AWS_ACCESS_KEY_ID' type='string' "
                "description='Access key id'/>"
                "  <Option name='AWS_SESSION_TOKEN' type='string' "
                "description='Session token'/>"
                "  <Option name='AWS_REQUEST_PAYER' type='string' "
                "description='Content of the x-amz-request-payer HTTP header. "
                "Typically \"requester\" for requester-pays buckets'/>"
                "  <Option name='AWS_S3_ENDPOINT' type='string' "
                "description='Endpoint for a S3-compatible API' "
                "default='https://s3.amazonaws.com'/>"
                "  <Option name='AWS_VIRTUAL_HOSTING' type='boolean' "
                "description='Whether to use virtual hosting server name when "
                "the "
                "bucket name is compatible with it' default='YES'/>"
                "  <Option name='AWS_NO_SIGN_REQUEST' type='boolean' "
                "description='Whether to disable signing of requests' "
                "default='NO'/>"
                "  <Option name='AWS_DEFAULT_REGION' type='string' "
                "description='AWS S3 default region' default='us-east-1'/>"
                "  <Option name='CPL_AWS_AUTODETECT_EC2' type='boolean' "
                "description='Whether to check Hypervisor and DMI identifiers "
                "to "
                "determine if current host is an AWS EC2 instance' "
                "default='YES'/>"
                "  <Option name='AWS_PROFILE' type='string' "
                "description='Name of the profile to use for IAM credentials "
                "retrieval on EC2 instances' default='default'/>"
                "  <Option name='AWS_DEFAULT_PROFILE' type='string' "
                "description='(deprecated) Name of the profile to use for "
                "IAM credentials "
                "retrieval on EC2 instances' default='default'/>"
                "  <Option name='AWS_CONFIG_FILE' type='string' "
                "description='Filename that contains AWS configuration' "
                "default='~/.aws/config'/>"
                "  <Option name='CPL_AWS_CREDENTIALS_FILE' type='string' "
                "description='Filename that contains AWS credentials' "
                "default='~/.aws/credentials'/>"
                "  <Option name='VSIS3_CHUNK_SIZE' type='int' "
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

char *VSIS3FSHandler::GetSignedURL(const char *pszFilename,
                                   CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;

    VSIS3HandleHelper *poS3HandleHelper = VSIS3HandleHelper::BuildFromURI(
        pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str(), false,
        papszOptions);
    if (poS3HandleHelper == nullptr)
    {
        return nullptr;
    }

    std::string osRet(poS3HandleHelper->GetSignedURL(papszOptions));

    delete poS3HandleHelper;
    return CPLStrdup(osRet.c_str());
}

/************************************************************************/
/*                           UnlinkBatch()                              */
/************************************************************************/

int *VSIS3FSHandler::UnlinkBatch(CSLConstList papszFiles)
{
    // Implemented using
    // https://docs.aws.amazon.com/AmazonS3/latest/API/API_DeleteObjects.html

    int *panRet =
        static_cast<int *>(CPLCalloc(sizeof(int), CSLCount(papszFiles)));
    CPLStringList aosList;
    std::string osCurBucket;
    int iStartIndex = -1;
    // For debug / testing only
    const int nBatchSize =
        atoi(CPLGetConfigOption("CPL_VSIS3_UNLINK_BATCH_SIZE", "1000"));
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
        bool bBucketChanged = false;
        if ((osCurBucket.empty() || osCurBucket == osBucket))
        {
            if (osCurBucket.empty())
            {
                iStartIndex = i;
                osCurBucket = osBucket;
            }
            aosList.AddString(pszSlash + 1);
        }
        else
        {
            bBucketChanged = true;
        }
        while (bBucketChanged || aosList.size() == nBatchSize ||
               papszFiles[i + 1] == nullptr)
        {
            // Compose XML post content
            CPLXMLNode *psXML = CPLCreateXMLNode(nullptr, CXT_Element, "?xml");
            CPLAddXMLAttributeAndValue(psXML, "version", "1.0");
            CPLAddXMLAttributeAndValue(psXML, "encoding", "UTF-8");
            CPLXMLNode *psDelete =
                CPLCreateXMLNode(nullptr, CXT_Element, "Delete");
            psXML->psNext = psDelete;
            CPLAddXMLAttributeAndValue(
                psDelete, "xmlns", "http://s3.amazonaws.com/doc/2006-03-01/");
            CPLXMLNode *psLastChild = psDelete->psChild;
            CPLAssert(psLastChild != nullptr);
            CPLAssert(psLastChild->psNext == nullptr);
            std::map<std::string, int> mapKeyToIndex;
            for (int j = 0; aosList[j]; ++j)
            {
                CPLXMLNode *psObject =
                    CPLCreateXMLNode(nullptr, CXT_Element, "Object");
                mapKeyToIndex[aosList[j]] = iStartIndex + j;
                CPLCreateXMLElementAndValue(psObject, "Key", aosList[j]);
                psLastChild->psNext = psObject;
                psLastChild = psObject;
            }

            // Run request
            char *pszXML = CPLSerializeXMLTree(psXML);
            CPLDestroyXMLNode(psXML);
            auto oDeletedKeys = DeleteObjects(osCurBucket.c_str(), pszXML);
            CPLFree(pszXML);

            // Mark delete file
            for (const auto &osDeletedKey : oDeletedKeys)
            {
                auto mapKeyToIndexIter = mapKeyToIndex.find(osDeletedKey);
                if (mapKeyToIndexIter != mapKeyToIndex.end())
                {
                    panRet[mapKeyToIndexIter->second] = true;
                }
            }

            osCurBucket.clear();
            aosList.Clear();
            if (bBucketChanged)
            {
                iStartIndex = i;
                osCurBucket = osBucket;
                aosList.AddString(pszSlash + 1);
                bBucketChanged = false;
            }
            else
            {
                break;
            }
        }
    }
    return panRet;
}

/************************************************************************/
/*                           RmdirRecursive()                           */
/************************************************************************/

int VSIS3FSHandler::RmdirRecursive(const char *pszDirname)
{
    // Some S3-like APIs do not support DeleteObjects
    if (CPLTestBool(VSIGetPathSpecificOption(
            pszDirname, "CPL_VSIS3_USE_BASE_RMDIR_RECURSIVE", "NO")))
        return VSIFilesystemHandler::RmdirRecursive(pszDirname);

    // For debug / testing only
    const int nBatchSize =
        atoi(CPLGetConfigOption("CPL_VSIS3_UNLINK_BATCH_SIZE", "1000"));

    return RmdirRecursiveInternal(pszDirname, nBatchSize);
}

int IVSIS3LikeFSHandler::RmdirRecursiveInternal(const char *pszDirname,
                                                int nBatchSize)
{
    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("RmdirRecursive");

    std::string osDirnameWithoutEndSlash(pszDirname);
    if (!osDirnameWithoutEndSlash.empty() &&
        osDirnameWithoutEndSlash.back() == '/')
        osDirnameWithoutEndSlash.pop_back();

    CPLStringList aosOptions;
    aosOptions.SetNameValue("CACHE_ENTRIES", "FALSE");
    auto poDir = std::unique_ptr<VSIDIR>(
        OpenDir(osDirnameWithoutEndSlash.c_str(), -1, aosOptions.List()));
    if (!poDir)
        return -1;
    CPLStringList aosList;

    while (true)
    {
        auto entry = poDir->NextDirEntry();
        if (entry)
        {
            std::string osFilename(osDirnameWithoutEndSlash + '/' +
                                   entry->pszName);
            if (entry->nMode == S_IFDIR)
                osFilename += '/';
            aosList.AddString(osFilename.c_str());
        }
        if (entry == nullptr || aosList.size() == nBatchSize)
        {
            if (entry == nullptr && !osDirnameWithoutEndSlash.empty())
            {
                aosList.AddString((osDirnameWithoutEndSlash + '/').c_str());
            }
            int *ret = DeleteObjectBatch(aosList.List());
            if (ret == nullptr)
                return -1;
            CPLFree(ret);
            aosList.Clear();
        }
        if (entry == nullptr)
            break;
    }
    PartialClearCache(osDirnameWithoutEndSlash.c_str());
    return 0;
}

/************************************************************************/
/*                            DeleteObjects()                           */
/************************************************************************/

std::set<std::string> VSIS3FSHandler::DeleteObjects(const char *pszBucket,
                                                    const char *pszXML)
{
    auto poS3HandleHelper =
        std::unique_ptr<VSIS3HandleHelper>(VSIS3HandleHelper::BuildFromURI(
            pszBucket, GetFSPrefix().c_str(), true));
    if (!poS3HandleHelper)
        return std::set<std::string>();

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("DeleteObjects");

    std::set<std::string> oDeletedKeys;
    bool bRetry;
    const std::string osFilename(GetFSPrefix() + pszBucket);
    const CPLStringList aosHTTPOptions(
        CPLHTTPGetOptionsFromEnv(osFilename.c_str()));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);
    CPLHTTPRetryContext oRetryContext(oRetryParameters);

    struct CPLMD5Context context;
    CPLMD5Init(&context);
    CPLMD5Update(&context, pszXML, strlen(pszXML));
    unsigned char hash[16];
    CPLMD5Final(hash, &context);
    char *pszBase64 = CPLBase64Encode(16, hash);
    std::string osContentMD5("Content-MD5: ");
    osContentMD5 += pszBase64;
    CPLFree(pszBase64);

    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("delete", "");
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS, pszXML);

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = curl_slist_append(headers, "Content-Type: application/xml");
        headers = curl_slist_append(headers, osContentMD5.c_str());
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("POST", headers, pszXML,
                                                      strlen(pszXML)));

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poS3HandleHelper.get());

        NetworkStatisticsLogger::LogPOST(strlen(pszXML),
                                         requestHelper.sWriteFuncData.nSize);

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
                         poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "DeleteObjects failed");
            }
        }
        else
        {
            CPLXMLNode *psXML =
                CPLParseXMLString(requestHelper.sWriteFuncData.pBuffer);
            if (psXML)
            {
                CPLXMLNode *psDeleteResult =
                    CPLGetXMLNode(psXML, "=DeleteResult");
                if (psDeleteResult)
                {
                    for (CPLXMLNode *psIter = psDeleteResult->psChild; psIter;
                         psIter = psIter->psNext)
                    {
                        if (psIter->eType == CXT_Element &&
                            strcmp(psIter->pszValue, "Deleted") == 0)
                        {
                            std::string osKey =
                                CPLGetXMLValue(psIter, "Key", "");
                            oDeletedKeys.insert(osKey);

                            InvalidateCachedData(
                                (poS3HandleHelper->GetURL() + osKey).c_str());

                            InvalidateDirContent(CPLGetDirnameSafe(
                                (GetFSPrefix() + pszBucket + "/" + osKey)
                                    .c_str()));
                        }
                    }
                }
                CPLDestroyXMLNode(psXML);
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);
    return oDeletedKeys;
}

/************************************************************************/
/*                          GetFileMetadata()                           */
/************************************************************************/

char **VSIS3FSHandler::GetFileMetadata(const char *pszFilename,
                                       const char *pszDomain,
                                       CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;

    if (pszDomain == nullptr || !EQUAL(pszDomain, "TAGS"))
    {
        return VSICurlFilesystemHandlerBase::GetFileMetadata(
            pszFilename, pszDomain, papszOptions);
    }

    auto poS3HandleHelper =
        std::unique_ptr<VSIS3HandleHelper>(VSIS3HandleHelper::BuildFromURI(
            pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str(), false));
    if (!poS3HandleHelper)
        return nullptr;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("GetFileMetadata");

    bool bRetry;

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);
    CPLHTTPRetryContext oRetryContext(oRetryParameters);

    CPLStringList aosTags;
    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("tagging", "");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("GET", headers));

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poS3HandleHelper.get());

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
                         poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "GetObjectTagging failed");
            }
        }
        else
        {
            CPLXMLNode *psXML =
                CPLParseXMLString(requestHelper.sWriteFuncData.pBuffer);
            if (psXML)
            {
                CPLXMLNode *psTagSet = CPLGetXMLNode(psXML, "=Tagging.TagSet");
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
                            aosTags.SetNameValue(pszKey, pszValue);
                        }
                    }
                }
                CPLDestroyXMLNode(psXML);
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);
    return CSLDuplicate(aosTags.List());
}

/************************************************************************/
/*                          SetFileMetadata()                           */
/************************************************************************/

bool VSIS3FSHandler::SetFileMetadata(const char *pszFilename,
                                     CSLConstList papszMetadata,
                                     const char *pszDomain,
                                     CSLConstList /* papszOptions */)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return false;

    if (pszDomain == nullptr ||
        !(EQUAL(pszDomain, "HEADERS") || EQUAL(pszDomain, "TAGS")))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only HEADERS and TAGS domain are supported");
        return false;
    }

    if (EQUAL(pszDomain, "HEADERS"))
    {
        return CopyObject(pszFilename, pszFilename, papszMetadata) == 0;
    }

    auto poS3HandleHelper =
        std::unique_ptr<VSIS3HandleHelper>(VSIS3HandleHelper::BuildFromURI(
            pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str(), false));
    if (!poS3HandleHelper)
        return false;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("SetFileMetadata");

    // Compose XML post content
    std::string osXML;
    if (papszMetadata != nullptr && papszMetadata[0] != nullptr)
    {
        CPLXMLNode *psXML = CPLCreateXMLNode(nullptr, CXT_Element, "?xml");
        CPLAddXMLAttributeAndValue(psXML, "version", "1.0");
        CPLAddXMLAttributeAndValue(psXML, "encoding", "UTF-8");
        CPLXMLNode *psTagging =
            CPLCreateXMLNode(nullptr, CXT_Element, "Tagging");
        psXML->psNext = psTagging;
        CPLAddXMLAttributeAndValue(psTagging, "xmlns",
                                   "http://s3.amazonaws.com/doc/2006-03-01/");
        CPLXMLNode *psTagSet =
            CPLCreateXMLNode(psTagging, CXT_Element, "TagSet");
        for (int i = 0; papszMetadata[i]; ++i)
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

    std::string osContentMD5;
    if (!osXML.empty())
    {
        struct CPLMD5Context context;
        CPLMD5Init(&context);
        CPLMD5Update(&context, osXML.data(), osXML.size());
        unsigned char hash[16];
        CPLMD5Final(hash, &context);
        char *pszBase64 = CPLBase64Encode(16, hash);
        osContentMD5 = "Content-MD5: ";
        osContentMD5 += pszBase64;
        CPLFree(pszBase64);
    }

    bool bRetry;

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);
    CPLHTTPRetryContext oRetryContext(oRetryParameters);

    bool bRet = false;

    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("tagging", "");
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST,
                                   osXML.empty() ? "DELETE" : "PUT");
        if (!osXML.empty())
        {
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS,
                                       osXML.c_str());
        }

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        if (!osXML.empty())
        {
            headers =
                curl_slist_append(headers, "Content-Type: application/xml");
            headers = curl_slist_append(headers, osContentMD5.c_str());
            headers = VSICurlMergeHeaders(
                headers, poS3HandleHelper->GetCurlHeaders(
                             "PUT", headers, osXML.c_str(), osXML.size()));
            NetworkStatisticsLogger::LogPUT(osXML.size());
        }
        else
        {
            headers = VSICurlMergeHeaders(
                headers, poS3HandleHelper->GetCurlHeaders("DELETE", headers));
            NetworkStatisticsLogger::LogDELETE();
        }

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poS3HandleHelper.get());

        if ((!osXML.empty() && response_code != 200) ||
            (osXML.empty() && response_code != 204))
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
                         poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "PutObjectTagging failed");
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
VSIS3FSHandler::GetStreamingFilename(const std::string &osFilename) const
{
    if (STARTS_WITH(osFilename.c_str(), GetFSPrefix().c_str()))
        return "/vsis3_streaming/" + osFilename.substr(GetFSPrefix().size());
    return osFilename;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::MkdirInternal(const char *pszDirname, long /*nMode*/,
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
            VSI_ISDIR(sStat.st_mode))
        {
            CPLDebug(GetDebugKey(), "Directory %s already exists",
                     osDirname.c_str());
            errno = EEXIST;
            return -1;
        }
    }

    int ret = 0;
    if (CPLTestBool(CPLGetConfigOption("CPL_VSIS3_CREATE_DIR_OBJECT", "YES")))
    {
        VSILFILE *fp = VSIFOpenL(osDirname.c_str(), "wb");
        if (fp != nullptr)
        {
            CPLErrorReset();
            VSIFCloseL(fp);
            ret = CPLGetLastErrorType() == CPLE_None ? 0 : -1;
        }
        else
        {
            ret = -1;
        }
    }

    if (ret == 0)
    {
        std::string osDirnameWithoutEndSlash(osDirname);
        osDirnameWithoutEndSlash.pop_back();

        InvalidateDirContent(
            CPLGetDirnameSafe(osDirnameWithoutEndSlash.c_str()));

        FileProp cachedFileProp;
        GetCachedFileProp(GetURLFromFilename(osDirname.c_str()).c_str(),
                          cachedFileProp);
        cachedFileProp.eExists = EXIST_YES;
        cachedFileProp.bIsDirectory = true;
        cachedFileProp.bHasComputedFileSize = true;
        SetCachedFileProp(GetURLFromFilename(osDirname.c_str()).c_str(),
                          cachedFileProp);

        RegisterEmptyDir(osDirnameWithoutEndSlash);
        RegisterEmptyDir(osDirname);
    }
    return ret;
}

int IVSIS3LikeFSHandler::Mkdir(const char *pszDirname, long nMode)
{
    return MkdirInternal(pszDirname, nMode, true);
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::Rmdir(const char *pszDirname)
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
        CPLDebug(GetDebugKey(), "%s is not a object", pszDirname);
        errno = ENOENT;
        return -1;
    }
    else if (!VSI_ISDIR(sStat.st_mode))
    {
        CPLDebug(GetDebugKey(), "%s is not a directory", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    char **papszFileList = ReadDirEx(osDirname.c_str(), 100);
    bool bEmptyDir =
        papszFileList == nullptr ||
        (EQUAL(papszFileList[0], ".") && papszFileList[1] == nullptr);
    CSLDestroy(papszFileList);
    if (!bEmptyDir)
    {
        CPLDebug(GetDebugKey(), "%s is not empty", pszDirname);
        errno = ENOTEMPTY;
        return -1;
    }

    std::string osDirnameWithoutEndSlash(osDirname);
    osDirnameWithoutEndSlash.pop_back();
    if (osDirnameWithoutEndSlash.find('/', GetFSPrefix().size()) ==
        std::string::npos)
    {
        CPLDebug(GetDebugKey(), "%s is a bucket", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    int ret = DeleteObject(osDirname.c_str());
    if (ret == 0)
    {
        InvalidateDirContent(osDirnameWithoutEndSlash.c_str());
    }
    return ret;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
                              int nFlags)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return -1;

    if ((nFlags & VSI_STAT_CACHE_ONLY) != 0)
        return VSICurlFilesystemHandlerBase::Stat(pszFilename, pStatBuf,
                                                  nFlags);

    memset(pStatBuf, 0, sizeof(VSIStatBufL));
    if (!IsAllowedFilename(pszFilename))
        return -1;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("Stat");

    std::string osFilename(pszFilename);
    if (osFilename.find('/', GetFSPrefix().size()) == std::string::npos)
        osFilename += "/";

    std::string osFilenameWithoutSlash(osFilename);
    if (osFilenameWithoutSlash.back() == '/')
        osFilenameWithoutSlash.pop_back();

    // If there's directory content for the directory where this file belongs
    // to, use it to detect if the object does not exist
    CachedDirList cachedDirList;
    const std::string osDirname(
        CPLGetDirnameSafe(osFilenameWithoutSlash.c_str()));
    if (STARTS_WITH_CI(osDirname.c_str(), GetFSPrefix().c_str()) &&
        GetCachedDirList(osDirname.c_str(), cachedDirList) &&
        cachedDirList.bGotFileList)
    {
        const std::string osFilenameOnly(
            CPLGetFilename(osFilenameWithoutSlash.c_str()));
        bool bFound = false;
        for (int i = 0; i < cachedDirList.oFileList.size(); i++)
        {
            if (cachedDirList.oFileList[i] == osFilenameOnly)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            return -1;
        }
    }

    if (VSICurlFilesystemHandlerBase::Stat(osFilename.c_str(), pStatBuf,
                                           nFlags) == 0)
    {
        return 0;
    }

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

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle *VSIS3FSHandler::CreateFileHandle(const char *pszFilename)
{
    VSIS3HandleHelper *poS3HandleHelper = VSIS3HandleHelper::BuildFromURI(
        pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str(), false);
    if (poS3HandleHelper)
    {
        return new VSIS3Handle(this, pszFilename, poS3HandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                          GetURLFromFilename()                         */
/************************************************************************/

std::string
VSIS3FSHandler::GetURLFromFilename(const std::string &osFilename) const
{
    const std::string osFilenameWithoutPrefix =
        osFilename.substr(GetFSPrefix().size());

    auto poS3HandleHelper =
        std::unique_ptr<VSIS3HandleHelper>(VSIS3HandleHelper::BuildFromURI(
            osFilenameWithoutPrefix.c_str(), GetFSPrefix().c_str(), true));
    if (!poS3HandleHelper)
    {
        return std::string();
    }
    std::string osBaseURL(poS3HandleHelper->GetURL());
    if (!osBaseURL.empty() && osBaseURL.back() == '/')
        osBaseURL.pop_back();
    return osBaseURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper *VSIS3FSHandler::CreateHandleHelper(const char *pszURI,
                                                           bool bAllowNoObject)
{
    return VSIS3HandleHelper::BuildFromURI(pszURI, GetFSPrefix().c_str(),
                                           bAllowNoObject);
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int IVSIS3LikeFSHandler::Unlink(const char *pszFilename)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return -1;

    std::string osNameWithoutPrefix = pszFilename + GetFSPrefix().size();
    if (osNameWithoutPrefix.find('/') == std::string::npos)
    {
        CPLDebug(GetDebugKey(), "%s is not a file", pszFilename);
        errno = EISDIR;
        return -1;
    }

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("Unlink");

    VSIStatBufL sStat;
    if (VSIStatL(pszFilename, &sStat) != 0)
    {
        CPLDebug(GetDebugKey(), "%s is not a object", pszFilename);
        errno = ENOENT;
        return -1;
    }
    else if (!VSI_ISREG(sStat.st_mode))
    {
        CPLDebug(GetDebugKey(), "%s is not a file", pszFilename);
        errno = EISDIR;
        return -1;
    }

    return DeleteObject(pszFilename);
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int IVSIS3LikeFSHandler::Rename(const char *oldpath, const char *newpath,
                                GDALProgressFunc pfnProgress,
                                void *pProgressData)
{
    if (!STARTS_WITH_CI(oldpath, GetFSPrefix().c_str()))
        return -1;
    if (!STARTS_WITH_CI(newpath, GetFSPrefix().c_str()))
        return -1;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("Rename");

    VSIStatBufL sStat;
    if (VSIStatL(oldpath, &sStat) != 0)
    {
        CPLDebug(GetDebugKey(), "%s is not a object", oldpath);
        errno = ENOENT;
        return -1;
    }

    // AWS doesn't like renaming to the same name, and errors out
    // But GCS does like it, and so we might end up killing ourselves !
    // POSIX says renaming on the same file is OK
    if (strcmp(oldpath, newpath) == 0)
        return 0;

    if (VSI_ISDIR(sStat.st_mode))
    {
        int ret = 0;
        const CPLStringList aosList(VSIReadDir(oldpath));
        Mkdir(newpath, 0755);
        const int nListSize = aosList.size();
        for (int i = 0; ret == 0 && i < nListSize; i++)
        {
            const std::string osSrc =
                CPLFormFilenameSafe(oldpath, aosList[i], nullptr);
            const std::string osTarget =
                CPLFormFilenameSafe(newpath, aosList[i], nullptr);
            void *pScaledProgress =
                GDALCreateScaledProgress(static_cast<double>(i) / nListSize,
                                         static_cast<double>(i + 1) / nListSize,
                                         pfnProgress, pProgressData);
            ret = Rename(osSrc.c_str(), osTarget.c_str(),
                         pScaledProgress ? GDALScaledProgress : nullptr,
                         pScaledProgress);
            GDALDestroyScaledProgress(pScaledProgress);
        }
        if (ret == 0)
            Rmdir(oldpath);
        return ret;
    }
    else
    {
        if (VSIStatL(newpath, &sStat) == 0 && VSI_ISDIR(sStat.st_mode))
        {
            CPLDebug(GetDebugKey(), "%s already exists and is a directory",
                     newpath);
            errno = ENOTEMPTY;
            return -1;
        }
        if (CopyObject(oldpath, newpath, nullptr) != 0)
        {
            return -1;
        }
        return DeleteObject(oldpath);
    }
}

/************************************************************************/
/*                            CopyObject()                              */
/************************************************************************/

int IVSIS3LikeFSHandler::CopyObject(const char *oldpath, const char *newpath,
                                    CSLConstList papszMetadata)
{
    std::string osTargetNameWithoutPrefix = newpath + GetFSPrefix().size();
    std::unique_ptr<IVSIS3LikeHandleHelper> poS3HandleHelper(
        CreateHandleHelper(osTargetNameWithoutPrefix.c_str(), false));
    if (poS3HandleHelper == nullptr)
    {
        return -1;
    }

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("CopyObject");

    std::string osSourceHeader(poS3HandleHelper->GetCopySourceHeader());
    if (osSourceHeader.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Object copy not supported by this file system");
        return -1;
    }
    osSourceHeader += ": /";
    if (STARTS_WITH(oldpath, "/vsis3/"))
        osSourceHeader +=
            CPLAWSURLEncode(oldpath + GetFSPrefix().size(), false);
    else
        osSourceHeader += (oldpath + GetFSPrefix().size());

    int nRet = 0;

    bool bRetry;

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(oldpath));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);
    CPLHTTPRetryContext oRetryContext(oRetryParameters);

    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = curl_slist_append(headers, osSourceHeader.c_str());
        headers = curl_slist_append(
            headers, "Content-Length: 0");  // Required by GCS, but not by S3
        if (papszMetadata && papszMetadata[0])
        {
            const char *pszReplaceDirective =
                poS3HandleHelper->GetMetadataDirectiveREPLACE();
            if (pszReplaceDirective[0])
                headers = curl_slist_append(headers, pszReplaceDirective);
            for (int i = 0; papszMetadata[i]; i++)
            {
                char *pszKey = nullptr;
                const char *pszValue =
                    CPLParseNameValue(papszMetadata[i], &pszKey);
                if (pszKey && pszValue)
                {
                    headers = curl_slist_append(
                        headers, CPLSPrintf("%s: %s", pszKey, pszValue));
                }
                CPLFree(pszKey);
            }
        }
        headers = VSICurlSetContentTypeFromExt(headers, newpath);
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("PUT", headers));

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, this, poS3HandleHelper.get());

        NetworkStatisticsLogger::LogPUT(0);

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
                         poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
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
            InvalidateCachedData(poS3HandleHelper->GetURL().c_str());

            std::string osFilenameWithoutSlash(newpath);
            if (!osFilenameWithoutSlash.empty() &&
                osFilenameWithoutSlash.back() == '/')
                osFilenameWithoutSlash.resize(osFilenameWithoutSlash.size() -
                                              1);

            InvalidateDirContent(
                CPLGetDirnameSafe(osFilenameWithoutSlash.c_str()));
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    return nRet;
}

/************************************************************************/
/*                           DeleteObject()                             */
/************************************************************************/

int IVSIS3LikeFSHandler::DeleteObject(const char *pszFilename)
{
    std::string osNameWithoutPrefix = pszFilename + GetFSPrefix().size();
    IVSIS3LikeHandleHelper *poS3HandleHelper =
        CreateHandleHelper(osNameWithoutPrefix.c_str(), false);
    if (poS3HandleHelper == nullptr)
    {
        return -1;
    }

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("DeleteObject");

    int nRet = 0;

    bool bRetry;

    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);
    CPLHTTPRetryContext oRetryContext(oRetryParameters);

    do
    {
        bRetry = false;
        CURL *hCurlHandle = curl_easy_init();
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST,
                                   "DELETE");

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, poS3HandleHelper->GetURL().c_str(),
                              aosHTTPOptions.List()));
        headers = VSICurlMergeHeaders(
            headers, poS3HandleHelper->GetCurlHeaders("DELETE", headers));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        NetworkStatisticsLogger::LogDELETE();

        // S3 and GS respond with 204. Azure with 202. ADLS with 200.
        if (response_code != 204 && response_code != 202 &&
            response_code != 200)
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
                         poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "Delete of %s failed",
                         pszFilename);
                nRet = -1;
            }
        }
        else
        {
            InvalidateCachedData(poS3HandleHelper->GetURL().c_str());

            std::string osFilenameWithoutSlash(pszFilename);
            if (!osFilenameWithoutSlash.empty() &&
                osFilenameWithoutSlash.back() == '/')
                osFilenameWithoutSlash.resize(osFilenameWithoutSlash.size() -
                                              1);

            InvalidateDirContent(
                CPLGetDirnameSafe(osFilenameWithoutSlash.c_str()));
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);

    delete poS3HandleHelper;
    return nRet;
}

/************************************************************************/
/*                        DeleteObjectBatch()                           */
/************************************************************************/

int *IVSIS3LikeFSHandler::DeleteObjectBatch(CSLConstList papszFilesOrDirs)
{
    int *panRet =
        static_cast<int *>(CPLMalloc(sizeof(int) * CSLCount(papszFilesOrDirs)));
    for (int i = 0; papszFilesOrDirs && papszFilesOrDirs[i]; ++i)
    {
        panRet[i] = DeleteObject(papszFilesOrDirs[i]) == 0;
    }
    return panRet;
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char **IVSIS3LikeFSHandler::GetFileList(const char *pszDirname, int nMaxFiles,
                                        bool *pbGotFileList)
{
    if (ENABLE_DEBUG)
        CPLDebug(GetDebugKey(), "GetFileList(%s)", pszDirname);

    *pbGotFileList = false;

    char **papszOptions =
        CSLSetNameValue(nullptr, "MAXFILES", CPLSPrintf("%d", nMaxFiles));
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
/*                            OpenDir()                                 */
/************************************************************************/

VSIDIR *IVSIS3LikeFSHandler::OpenDir(const char *pszPath, int nRecurseDepth,
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
        osDirnameWithoutPrefix.pop_back();
    }

    std::string osBucket(osDirnameWithoutPrefix);
    std::string osObjectKey;
    size_t nSlashPos = osDirnameWithoutPrefix.find('/');
    if (nSlashPos != std::string::npos)
    {
        osBucket = osDirnameWithoutPrefix.substr(0, nSlashPos);
        osObjectKey = osDirnameWithoutPrefix.substr(nSlashPos + 1);
    }

    auto poS3HandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(osBucket.c_str(), true));
    if (poS3HandleHelper == nullptr)
    {
        return nullptr;
    }

    VSIDIRS3 *dir = new VSIDIRS3(pszPath, this);
    dir->nRecurseDepth = nRecurseDepth;
    dir->poHandleHelper = std::move(poS3HandleHelper);
    dir->osBucket = std::move(osBucket);
    dir->osObjectKey = std::move(osObjectKey);
    dir->nMaxFiles = atoi(CSLFetchNameValueDef(papszOptions, "MAXFILES", "0"));
    dir->bCacheEntries = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "CACHE_ENTRIES", "TRUE"));
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
/*                       ComputeMD5OfLocalFile()                        */
/************************************************************************/

static std::string ComputeMD5OfLocalFile(VSILFILE *fp)
{
    constexpr size_t nBufferSize = 10 * 4096;
    std::vector<GByte> abyBuffer(nBufferSize, 0);

    struct CPLMD5Context context;
    CPLMD5Init(&context);

    while (true)
    {
        size_t nRead = VSIFReadL(&abyBuffer[0], 1, nBufferSize, fp);
        CPLMD5Update(&context, &abyBuffer[0], nRead);
        if (nRead < nBufferSize)
        {
            break;
        }
    }

    unsigned char hash[16];
    CPLMD5Final(hash, &context);

    constexpr char tohex[] = "0123456789abcdef";
    char hhash[33];
    for (int i = 0; i < 16; ++i)
    {
        hhash[i * 2] = tohex[(hash[i] >> 4) & 0xf];
        hhash[i * 2 + 1] = tohex[hash[i] & 0xf];
    }
    hhash[32] = '\0';

    VSIFSeekL(fp, 0, SEEK_SET);

    return hhash;
}

/************************************************************************/
/*                           CopyFile()                                 */
/************************************************************************/

int IVSIS3LikeFSHandler::CopyFile(const char *pszSource, const char *pszTarget,
                                  VSILFILE *fpSource, vsi_l_offset nSourceSize,
                                  CSLConstList papszOptions,
                                  GDALProgressFunc pProgressFunc,
                                  void *pProgressData)
{
    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("CopyFile");

    if (!pszSource)
    {
        return VSIFilesystemHandler::CopyFile(pszSource, pszTarget, fpSource,
                                              nSourceSize, papszOptions,
                                              pProgressFunc, pProgressData);
    }

    std::string osMsg("Copying of ");
    osMsg += pszSource;

    const std::string osPrefix(GetFSPrefix());
    if (STARTS_WITH(pszSource, osPrefix.c_str()) &&
        STARTS_WITH(pszTarget, osPrefix.c_str()))
    {
        bool bRet = CopyObject(pszSource, pszTarget, papszOptions) == 0;
        if (bRet && pProgressFunc)
        {
            bRet = pProgressFunc(1.0, osMsg.c_str(), pProgressData) != 0;
        }
        return bRet ? 0 : -1;
    }

    VSIVirtualHandleUniquePtr poFileHandleAutoClose;
    bool bUsingStreaming = false;
    if (!fpSource)
    {
        if (STARTS_WITH(pszSource, osPrefix.c_str()) &&
            CPLTestBool(CPLGetConfigOption(
                "VSIS3_COPYFILE_USE_STREAMING_SOURCE", "YES")))
        {
            // Try to get a streaming path from the source path
            auto poSourceFSHandler = dynamic_cast<IVSIS3LikeFSHandler *>(
                VSIFileManager::GetHandler(pszSource));
            if (poSourceFSHandler)
            {
                const std::string osStreamingPath =
                    poSourceFSHandler->GetStreamingFilename(pszSource);
                if (!osStreamingPath.empty())
                {
                    fpSource = VSIFOpenExL(osStreamingPath.c_str(), "rb", TRUE);
                    if (fpSource)
                        bUsingStreaming = true;
                }
            }
        }
        if (!fpSource)
        {
            fpSource = VSIFOpenExL(pszSource, "rb", TRUE);
        }
        if (!fpSource)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszSource);
            return false;
        }

        poFileHandleAutoClose.reset(fpSource);
    }

    int ret = VSIFilesystemHandler::CopyFile(pszSource, pszTarget, fpSource,
                                             nSourceSize, papszOptions,
                                             pProgressFunc, pProgressData);
    if (ret == -1 && bUsingStreaming)
    {
        // Retry without streaming. This may be useful for large files, when
        // there are connectivity issues, as retry attempts will be more
        // efficient when using range requests.
        CPLDebug(GetDebugKey(), "Retrying copy without streaming");
        fpSource = VSIFOpenExL(pszSource, "rb", TRUE);
        if (fpSource)
        {
            poFileHandleAutoClose.reset(fpSource);
            ret = VSIFilesystemHandler::CopyFile(pszSource, pszTarget, fpSource,
                                                 nSourceSize, papszOptions,
                                                 pProgressFunc, pProgressData);
        }
    }

    return ret;
}

/************************************************************************/
/*                    GetRequestedNumThreadsForCopy()                   */
/************************************************************************/

static int GetRequestedNumThreadsForCopy(CSLConstList papszOptions)
{
#if defined(CPL_MULTIPROC_STUB)
    (void)papszOptions;
    return 1;
#else
    // 10 threads used by default by the Python s3transfer library
    const char *pszValue =
        CSLFetchNameValueDef(papszOptions, "NUM_THREADS", "10");
    if (EQUAL(pszValue, "ALL_CPUS"))
        return CPLGetNumCPUs();
    return atoi(pszValue);
#endif
}

/************************************************************************/
/*                       CopyFileRestartable()                          */
/************************************************************************/

int IVSIS3LikeFSHandlerWithMultipartUpload::CopyFileRestartable(
    const char *pszSource, const char *pszTarget, const char *pszInputPayload,
    char **ppszOutputPayload, CSLConstList papszOptions,
    GDALProgressFunc pProgressFunc, void *pProgressData)
{
    const std::string osPrefix(GetFSPrefix());
    NetworkStatisticsFileSystem oContextFS(osPrefix.c_str());
    NetworkStatisticsAction oContextAction("CopyFileRestartable");

    *ppszOutputPayload = nullptr;

    if (!STARTS_WITH(pszTarget, osPrefix.c_str()))
        return -1;

    std::string osMsg("Copying of ");
    osMsg += pszSource;

    // Can we use server-side copy ?
    if (STARTS_WITH(pszSource, osPrefix.c_str()) &&
        STARTS_WITH(pszTarget, osPrefix.c_str()))
    {
        bool bRet = CopyObject(pszSource, pszTarget, papszOptions) == 0;
        if (bRet && pProgressFunc)
        {
            bRet = pProgressFunc(1.0, osMsg.c_str(), pProgressData) != 0;
        }
        return bRet ? 0 : -1;
    }

    // If multipart upload is not supported, fallback to regular CopyFile()
    if (!SupportsParallelMultipartUpload())
    {
        return CopyFile(pszSource, pszTarget, nullptr,
                        static_cast<vsi_l_offset>(-1), papszOptions,
                        pProgressFunc, pProgressData);
    }

    VSIVirtualHandleUniquePtr fpSource(VSIFOpenExL(pszSource, "rb", TRUE));
    if (!fpSource)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszSource);
        return -1;
    }

    const char *pszChunkSize = CSLFetchNameValue(papszOptions, "CHUNK_SIZE");
    size_t nChunkSize = GetUploadChunkSizeInBytes(pszTarget, pszChunkSize);

    VSIStatBufL sStatBuf;
    if (VSIStatL(pszSource, &sStatBuf) != 0)
        return -1;

    auto poS3HandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(pszTarget + osPrefix.size(), false));
    if (poS3HandleHelper == nullptr)
        return -1;

    int nChunkCount = 0;
    std::vector<std::string> aosEtags;
    std::string osUploadID;

    if (pszInputPayload)
    {
        // If there is an input payload, parse it, and do sanity checks
        // and initial setup

        CPLJSONDocument oDoc;
        if (!oDoc.LoadMemory(pszInputPayload))
            return -1;

        auto oRoot = oDoc.GetRoot();
        if (oRoot.GetString("source") != pszSource)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "'source' field in input payload does not match pszSource");
            return -1;
        }

        if (oRoot.GetString("target") != pszTarget)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "'target' field in input payload does not match pszTarget");
            return -1;
        }

        if (static_cast<uint64_t>(oRoot.GetLong("source_size")) !=
            static_cast<uint64_t>(sStatBuf.st_size))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'source_size' field in input payload does not match "
                     "source file size");
            return -1;
        }

        if (oRoot.GetLong("source_mtime") !=
            static_cast<GIntBig>(sStatBuf.st_mtime))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'source_mtime' field in input payload does not match "
                     "source file modification time");
            return -1;
        }

        osUploadID = oRoot.GetString("upload_id");
        if (osUploadID.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'upload_id' field in input payload missing or invalid");
            return -1;
        }

        const auto nChunkSizeLong = oRoot.GetLong("chunk_size");
        if (nChunkSizeLong <= 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'chunk_size' field in input payload missing or invalid");
            return -1;
        }
#if SIZEOF_VOIDP < 8
        if (static_cast<uint64_t>(nChunkSizeLong) >
            std::numeric_limits<size_t>::max())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'chunk_size' field in input payload is too large");
            return -1;
        }
#endif
        nChunkSize = static_cast<size_t>(nChunkSizeLong);

        auto oEtags = oRoot.GetArray("chunk_etags");
        if (!oEtags.IsValid())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'chunk_etags' field in input payload missing or invalid");
            return -1;
        }

        const auto nChunkCountLarge =
            (sStatBuf.st_size + nChunkSize - 1) / nChunkSize;
        if (nChunkCountLarge != static_cast<size_t>(oEtags.Size()))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "'chunk_etags' field in input payload has not expected size");
            return -1;
        }
        nChunkCount = oEtags.Size();
        for (int iChunk = 0; iChunk < nChunkCount; ++iChunk)
        {
            aosEtags.push_back(oEtags[iChunk].ToString());
        }
    }
    else
    {
        // Compute the number of chunks
        auto nChunkCountLarge =
            (sStatBuf.st_size + nChunkSize - 1) / nChunkSize;
        if (nChunkCountLarge > static_cast<size_t>(GetMaximumPartCount()))
        {
            // Re-adjust the chunk size if needed
            const int nWishedChunkCount = GetMaximumPartCount() / 10;
            const uint64_t nMinChunkSizeLarge =
                (sStatBuf.st_size + nWishedChunkCount - 1) / nWishedChunkCount;
            if (pszChunkSize)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Too small CHUNK_SIZE compared to file size. Should be at "
                    "least " CPL_FRMT_GUIB,
                    static_cast<GUIntBig>(nMinChunkSizeLarge));
                return -1;
            }
            if (nMinChunkSizeLarge >
                static_cast<size_t>(GetMaximumPartSizeInMiB()) * MIB_CONSTANT)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too large file");
                return -1;
            }
            nChunkSize = static_cast<size_t>(nMinChunkSizeLarge);
            nChunkCountLarge = (sStatBuf.st_size + nChunkSize - 1) / nChunkSize;
        }
        nChunkCount = static_cast<int>(nChunkCountLarge);
        aosEtags.resize(nChunkCount);
    }

    const CPLHTTPRetryParameters oRetryParameters(
        CPLStringList(CPLHTTPGetOptionsFromEnv(pszSource)));
    if (osUploadID.empty())
    {
        osUploadID = InitiateMultipartUpload(pszTarget, poS3HandleHelper.get(),
                                             oRetryParameters, nullptr);
        if (osUploadID.empty())
        {
            return -1;
        }
    }

    const int nRequestedThreads = GetRequestedNumThreadsForCopy(papszOptions);
    const int nNeededThreads = std::min(nRequestedThreads, nChunkCount);
    std::mutex oMutex;
    std::condition_variable oCV;
    bool bSuccess = true;
    bool bStop = false;
    bool bAbort = false;
    int iCurChunk = 0;

    const bool bRunInThread = nNeededThreads > 1;

    const auto threadFunc =
        [this, &fpSource, &aosEtags, &oMutex, &oCV, &iCurChunk, &bStop, &bAbort,
         &bSuccess, &osMsg, &osUploadID, &sStatBuf, &poS3HandleHelper,
         &osPrefix, bRunInThread, pszSource, pszTarget, nChunkCount, nChunkSize,
         &oRetryParameters, pProgressFunc, pProgressData]()
    {
        VSIVirtualHandleUniquePtr fpUniquePtr;
        VSIVirtualHandle *fp = nullptr;
        std::unique_ptr<IVSIS3LikeHandleHelper>
            poS3HandleHelperThisThreadUniquePtr;
        IVSIS3LikeHandleHelper *poS3HandleHelperThisThread = nullptr;

        std::vector<GByte> abyBuffer;
        try
        {
            abyBuffer.resize(nChunkSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate working buffer");
            std::lock_guard oLock(oMutex);
            bSuccess = false;
            bStop = true;
            return;
        }

        while (true)
        {
            int iChunk;
            {
                std::lock_guard oLock(oMutex);
                if (bStop)
                    break;
                if (iCurChunk == nChunkCount)
                    break;
                iChunk = iCurChunk;
                ++iCurChunk;
            }
            if (!fp)
            {
                if (iChunk == 0)
                {
                    fp = fpSource.get();
                    poS3HandleHelperThisThread = poS3HandleHelper.get();
                }
                else
                {
                    fpUniquePtr.reset(VSIFOpenExL(pszSource, "rb", TRUE));
                    if (!fpUniquePtr)
                    {
                        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                                 pszSource);

                        std::lock_guard oLock(oMutex);
                        bSuccess = false;
                        bStop = true;
                        break;
                    }
                    fp = fpUniquePtr.get();

                    poS3HandleHelperThisThreadUniquePtr.reset(
                        CreateHandleHelper(pszTarget + osPrefix.size(), false));
                    if (!poS3HandleHelperThisThreadUniquePtr)
                    {
                        std::lock_guard oLock(oMutex);
                        bSuccess = false;
                        bStop = true;
                        break;
                    }
                    poS3HandleHelperThisThread =
                        poS3HandleHelperThisThreadUniquePtr.get();
                }
            }

            if (aosEtags[iChunk].empty())
            {
                const auto nCurPos =
                    iChunk * static_cast<vsi_l_offset>(nChunkSize);
                CPL_IGNORE_RET_VAL(fp->Seek(nCurPos, SEEK_SET));
                const auto nRemaining = sStatBuf.st_size - nCurPos;
                const size_t nToRead =
                    nRemaining > static_cast<vsi_l_offset>(nChunkSize)
                        ? nChunkSize
                        : static_cast<int>(nRemaining);
                const size_t nRead = fp->Read(abyBuffer.data(), 1, nToRead);
                if (nRead != nToRead)
                {
                    CPLError(
                        CE_Failure, CPLE_FileIO,
                        "Did not get expected number of bytes from input file");
                    std::lock_guard oLock(oMutex);
                    bAbort = true;
                    bSuccess = false;
                    bStop = true;
                    break;
                }
                const auto osEtag = UploadPart(
                    pszTarget, 1 + iChunk, osUploadID, nCurPos,
                    abyBuffer.data(), nToRead, poS3HandleHelperThisThread,
                    oRetryParameters, nullptr);
                if (osEtag.empty())
                {
                    std::lock_guard oLock(oMutex);
                    bSuccess = false;
                    bStop = true;
                    break;
                }
                aosEtags[iChunk] = osEtag;
            }

            if (bRunInThread)
            {
                std::lock_guard oLock(oMutex);
                oCV.notify_one();
            }
            else
            {
                if (pProgressFunc &&
                    !pProgressFunc(double(iChunk) / nChunkCount, osMsg.c_str(),
                                   pProgressData))
                {
                    // Lock taken only to make static analyzer happy...
                    std::lock_guard oLock(oMutex);
                    bSuccess = false;
                    break;
                }
            }
        }
    };

    if (bRunInThread)
    {
        std::vector<std::thread> aThreads;
        for (int i = 0; i < nNeededThreads; i++)
        {
            aThreads.emplace_back(std::thread(threadFunc));
        }
        if (pProgressFunc)
        {
            std::unique_lock oLock(oMutex);
            while (!bStop)
            {
                oCV.wait(oLock);
                // coverity[ uninit_use_in_call]
                oLock.unlock();
                const bool bInterrupt =
                    !pProgressFunc(double(iCurChunk) / nChunkCount,
                                   osMsg.c_str(), pProgressData);
                oLock.lock();
                if (bInterrupt)
                {
                    bSuccess = false;
                    bStop = true;
                    break;
                }
            }
        }
        for (auto &thread : aThreads)
        {
            thread.join();
        }
    }
    else
    {
        threadFunc();
    }

    if (bAbort)
    {
        AbortMultipart(pszTarget, osUploadID, poS3HandleHelper.get(),
                       oRetryParameters);
        return -1;
    }
    else if (!bSuccess)
    {
        // Compose an output restart payload
        CPLJSONDocument oDoc;
        auto oRoot = oDoc.GetRoot();
        oRoot.Add("type", "CopyFileRestartablePayload");
        oRoot.Add("source", pszSource);
        oRoot.Add("target", pszTarget);
        oRoot.Add("source_size", static_cast<uint64_t>(sStatBuf.st_size));
        oRoot.Add("source_mtime", static_cast<GIntBig>(sStatBuf.st_mtime));
        oRoot.Add("chunk_size", static_cast<uint64_t>(nChunkSize));
        oRoot.Add("upload_id", osUploadID);
        CPLJSONArray oArray;
        for (int iChunk = 0; iChunk < nChunkCount; ++iChunk)
        {
            if (aosEtags[iChunk].empty())
                oArray.AddNull();
            else
                oArray.Add(aosEtags[iChunk]);
        }
        oRoot.Add("chunk_etags", oArray);
        *ppszOutputPayload = CPLStrdup(oDoc.SaveAsString().c_str());
        return 1;
    }

    if (!CompleteMultipart(pszTarget, osUploadID, aosEtags, sStatBuf.st_size,
                           poS3HandleHelper.get(), oRetryParameters))
    {
        AbortMultipart(pszTarget, osUploadID, poS3HandleHelper.get(),
                       oRetryParameters);
        return -1;
    }

    return 0;
}

/************************************************************************/
/*                          CopyChunk()                                 */
/************************************************************************/

static bool CopyChunk(const char *pszSource, const char *pszTarget,
                      vsi_l_offset nStartOffset, size_t nChunkSize)
{
    VSILFILE *fpIn = VSIFOpenExL(pszSource, "rb", TRUE);
    if (fpIn == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszSource);
        return false;
    }

    VSILFILE *fpOut = VSIFOpenExL(pszTarget, "wb+", TRUE);
    if (fpOut == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszTarget);
        VSIFCloseL(fpIn);
        return false;
    }

    bool ret = true;
    if (VSIFSeekL(fpIn, nStartOffset, SEEK_SET) < 0 ||
        VSIFSeekL(fpOut, nStartOffset, SEEK_SET) < 0)
    {
        ret = false;
    }
    else
    {
        void *pBuffer = VSI_MALLOC_VERBOSE(nChunkSize);
        if (pBuffer == nullptr)
        {
            ret = false;
        }
        else
        {
            if (VSIFReadL(pBuffer, 1, nChunkSize, fpIn) != nChunkSize ||
                VSIFWriteL(pBuffer, 1, nChunkSize, fpOut) != nChunkSize)
            {
                ret = false;
            }
        }
        VSIFree(pBuffer);
    }

    VSIFCloseL(fpIn);
    if (VSIFCloseL(fpOut) != 0)
    {
        ret = false;
    }
    if (!ret)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Copying of %s to %s failed",
                 pszSource, pszTarget);
    }
    return ret;
}

/************************************************************************/
/*                               Sync()                                 */
/************************************************************************/

bool IVSIS3LikeFSHandler::Sync(const char *pszSource, const char *pszTarget,
                               const char *const *papszOptions,
                               GDALProgressFunc pProgressFunc,
                               void *pProgressData, char ***ppapszOutputs)
{
    if (ppapszOutputs)
    {
        *ppapszOutputs = nullptr;
    }

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix().c_str());
    NetworkStatisticsAction oContextAction("Sync");

    std::string osSource(pszSource);
    std::string osSourceWithoutSlash(pszSource);
    if (osSourceWithoutSlash.back() == '/' ||
        osSourceWithoutSlash.back() == '\\')
    {
        osSourceWithoutSlash.pop_back();
    }

    const CPLHTTPRetryParameters oRetryParameters(
        CPLStringList(CPLHTTPGetOptionsFromEnv(pszSource)));

    const bool bRecursive = CPLFetchBool(papszOptions, "RECURSIVE", true);

    enum class SyncStrategy
    {
        TIMESTAMP,
        ETAG,
        OVERWRITE
    };
    SyncStrategy eSyncStrategy = SyncStrategy::TIMESTAMP;
    const char *pszSyncStrategy =
        CSLFetchNameValueDef(papszOptions, "SYNC_STRATEGY", "TIMESTAMP");
    if (EQUAL(pszSyncStrategy, "TIMESTAMP"))
        eSyncStrategy = SyncStrategy::TIMESTAMP;
    else if (EQUAL(pszSyncStrategy, "ETAG"))
        eSyncStrategy = SyncStrategy::ETAG;
    else if (EQUAL(pszSyncStrategy, "OVERWRITE"))
        eSyncStrategy = SyncStrategy::OVERWRITE;
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported value for SYNC_STRATEGY: %s", pszSyncStrategy);
    }

    const bool bDownloadFromNetworkToLocal =
        (!STARTS_WITH(pszTarget, "/vsi") ||
         STARTS_WITH(pszTarget, "/vsimem/")) &&
        STARTS_WITH(pszSource, GetFSPrefix().c_str());
    const bool bTargetIsThisFS = STARTS_WITH(pszTarget, GetFSPrefix().c_str());
    const bool bUploadFromLocalToNetwork =
        (!STARTS_WITH(pszSource, "/vsi") ||
         STARTS_WITH(pszSource, "/vsimem/")) &&
        bTargetIsThisFS;

    // If the source is likely to be a directory, try to issue a ReadDir()
    // if we haven't stat'ed it yet
    std::unique_ptr<VSIDIR> poSourceDir;
    if (STARTS_WITH(pszSource, GetFSPrefix().c_str()) &&
        (osSource.back() == '/' || osSource.back() == '\\'))
    {
        const char *const apszOptions[] = {"SYNTHETIZE_MISSING_DIRECTORIES=YES",
                                           nullptr};
        poSourceDir.reset(VSIOpenDir(osSourceWithoutSlash.c_str(),
                                     bRecursive ? -1 : 0, apszOptions));
    }

    VSIStatBufL sSource;
    if (VSIStatL(osSourceWithoutSlash.c_str(), &sSource) < 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "%s does not exist", pszSource);
        return false;
    }

    const auto CanSkipDownloadFromNetworkToLocal =
        [this, eSyncStrategy](
            const char *l_pszSource, const char *l_pszTarget,
            GIntBig sourceTime, GIntBig targetTime,
            const std::function<std::string(const char *)> &getETAGSourceFile)
    {
        switch (eSyncStrategy)
        {
            case SyncStrategy::ETAG:
            {
                VSILFILE *fpOutAsIn = VSIFOpenExL(l_pszTarget, "rb", TRUE);
                if (fpOutAsIn)
                {
                    std::string md5 = ComputeMD5OfLocalFile(fpOutAsIn);
                    VSIFCloseL(fpOutAsIn);
                    if (getETAGSourceFile(l_pszSource) == md5)
                    {
                        CPLDebug(GetDebugKey(),
                                 "%s has already same content as %s",
                                 l_pszTarget, l_pszSource);
                        return true;
                    }
                }
                return false;
            }

            case SyncStrategy::TIMESTAMP:
            {
                if (targetTime <= sourceTime)
                {
                    // Our local copy is older than the source, so
                    // presumably the source was uploaded from it. Nothing to do
                    CPLDebug(GetDebugKey(),
                             "%s is older than %s. "
                             "Do not replace %s assuming it was used to "
                             "upload %s",
                             l_pszTarget, l_pszSource, l_pszTarget,
                             l_pszSource);
                    return true;
                }
                return false;
            }

            case SyncStrategy::OVERWRITE:
            {
                break;
            }
        }
        return false;
    };

    const auto CanSkipUploadFromLocalToNetwork =
        [this, eSyncStrategy](
            VSILFILE *&l_fpIn, const char *l_pszSource, const char *l_pszTarget,
            GIntBig sourceTime, GIntBig targetTime,
            const std::function<std::string(const char *)> &getETAGTargetFile)
    {
        switch (eSyncStrategy)
        {
            case SyncStrategy::ETAG:
            {
                l_fpIn = VSIFOpenExL(l_pszSource, "rb", TRUE);
                if (l_fpIn && getETAGTargetFile(l_pszTarget) ==
                                  ComputeMD5OfLocalFile(l_fpIn))
                {
                    CPLDebug(GetDebugKey(), "%s has already same content as %s",
                             l_pszTarget, l_pszSource);
                    VSIFCloseL(l_fpIn);
                    l_fpIn = nullptr;
                    return true;
                }
                return false;
            }

            case SyncStrategy::TIMESTAMP:
            {
                if (targetTime >= sourceTime)
                {
                    // The remote copy is more recent than the source, so
                    // presumably it was uploaded from the source. Nothing to do
                    CPLDebug(GetDebugKey(),
                             "%s is more recent than %s. "
                             "Do not replace %s assuming it was uploaded from "
                             "%s",
                             l_pszTarget, l_pszSource, l_pszTarget,
                             l_pszSource);
                    return true;
                }
                return false;
            }

            case SyncStrategy::OVERWRITE:
            {
                break;
            }
        }
        return false;
    };

    struct ChunkToCopy
    {
        std::string osSrcFilename{};
        std::string osDstFilename{};
        GIntBig nMTime = 0;
        std::string osETag{};
        vsi_l_offset nTotalSize = 0;
        vsi_l_offset nStartOffset = 0;
        vsi_l_offset nSize = 0;
    };

    std::vector<ChunkToCopy> aoChunksToCopy;
    std::set<std::string> aoSetDirsToCreate;
    const char *pszChunkSize = CSLFetchNameValue(papszOptions, "CHUNK_SIZE");
    const int nRequestedThreads = GetRequestedNumThreadsForCopy(papszOptions);
    auto poTargetFSMultipartHandler =
        dynamic_cast<IVSIS3LikeFSHandlerWithMultipartUpload *>(
            VSIFileManager::GetHandler(pszTarget));
    const bool bSupportsParallelMultipartUpload =
        bUploadFromLocalToNetwork && poTargetFSMultipartHandler != nullptr &&
        poTargetFSMultipartHandler->SupportsParallelMultipartUpload();
    const bool bSimulateThreading =
        CPLTestBool(CPLGetConfigOption("VSIS3_SIMULATE_THREADING", "NO"));
    const int nMinSizeChunk =
        bSupportsParallelMultipartUpload && !bSimulateThreading
            ? 8 * MIB_CONSTANT
            : 1;  // 5242880 defined by S3 API as the minimum, but 8 MB used by
                  // default by the Python s3transfer library
    const int nMinThreads = bSimulateThreading ? 0 : 1;
    const size_t nMaxChunkSize =
        pszChunkSize && nRequestedThreads > nMinThreads &&
                (bDownloadFromNetworkToLocal ||
                 bSupportsParallelMultipartUpload)
            ? static_cast<size_t>(
                  std::min(1024 * MIB_CONSTANT,
                           std::max(nMinSizeChunk, atoi(pszChunkSize))))
            : 0;

    // Filter x-amz- options when outputting to /vsis3/
    CPLStringList aosObjectCreationOptions;
    if (poTargetFSMultipartHandler != nullptr && papszOptions != nullptr)
    {
        for (auto papszIter = papszOptions; *papszIter != nullptr; ++papszIter)
        {
            char *pszKey = nullptr;
            const char *pszValue = CPLParseNameValue(*papszIter, &pszKey);
            if (pszKey && pszValue &&
                poTargetFSMultipartHandler->IsAllowedHeaderForObjectCreation(
                    pszKey))
            {
                aosObjectCreationOptions.SetNameValue(pszKey, pszValue);
            }
            CPLFree(pszKey);
        }
    }

    uint64_t nTotalSize = 0;
    std::vector<size_t> anIndexToCopy;  // points to aoChunksToCopy

    struct MultiPartDef
    {
        std::string osUploadID{};
        int nCountValidETags = 0;
        int nExpectedCount = 0;
        // cppcheck-suppress unusedStructMember
        std::vector<std::string> aosEtags{};
        vsi_l_offset nTotalSize = 0;
    };

    std::map<std::string, MultiPartDef> oMapMultiPartDefs;

    // Cleanup pending uploads in case of early exit
    struct CleanupPendingUploads
    {
        IVSIS3LikeFSHandlerWithMultipartUpload *m_poFS;
        std::map<std::string, MultiPartDef> &m_oMapMultiPartDefs;
        const CPLHTTPRetryParameters &m_oRetryParameters;

        CleanupPendingUploads(
            IVSIS3LikeFSHandlerWithMultipartUpload *poFSIn,
            std::map<std::string, MultiPartDef> &oMapMultiPartDefsIn,
            const CPLHTTPRetryParameters &oRetryParametersIn)
            : m_poFS(poFSIn), m_oMapMultiPartDefs(oMapMultiPartDefsIn),
              m_oRetryParameters(oRetryParametersIn)
        {
        }

        ~CleanupPendingUploads()
        {
            if (m_poFS)
            {
                for (const auto &kv : m_oMapMultiPartDefs)
                {
                    auto poS3HandleHelper =
                        std::unique_ptr<IVSIS3LikeHandleHelper>(
                            m_poFS->CreateHandleHelper(
                                kv.first.c_str() + m_poFS->GetFSPrefix().size(),
                                false));
                    if (poS3HandleHelper)
                    {
                        m_poFS->AbortMultipart(kv.first, kv.second.osUploadID,
                                               poS3HandleHelper.get(),
                                               m_oRetryParameters);
                    }
                }
            }
        }

        CleanupPendingUploads(const CleanupPendingUploads &) = delete;
        CleanupPendingUploads &
        operator=(const CleanupPendingUploads &) = delete;
    };

    const CleanupPendingUploads cleanupPendingUploads(
        poTargetFSMultipartHandler, oMapMultiPartDefs, oRetryParameters);

    std::string osTargetDir;  // set in the VSI_ISDIR(sSource.st_mode) case
    std::string osTarget;     // set in the !(VSI_ISDIR(sSource.st_mode)) case

    const auto NormalizeDirSeparatorForDstFilename =
        [&osSource, &osTargetDir](const std::string &s) -> std::string
    {
        return CPLString(s).replaceAll(
            VSIGetDirectorySeparator(osSource.c_str()),
            VSIGetDirectorySeparator(osTargetDir.c_str()));
    };

    if (VSI_ISDIR(sSource.st_mode))
    {
        osTargetDir = pszTarget;
        if (osSource.back() != '/' && osSource.back() != '\\')
        {
            osTargetDir = CPLFormFilenameSafe(
                osTargetDir.c_str(), CPLGetFilename(pszSource), nullptr);
        }

        if (!poSourceDir)
        {
            const char *const apszOptions[] = {
                "SYNTHETIZE_MISSING_DIRECTORIES=YES", nullptr};
            poSourceDir.reset(VSIOpenDir(osSourceWithoutSlash.c_str(),
                                         bRecursive ? -1 : 0, apszOptions));
            if (!poSourceDir)
                return false;
        }

        auto poTargetDir = std::unique_ptr<VSIDIR>(
            VSIOpenDir(osTargetDir.c_str(), bRecursive ? -1 : 0, nullptr));
        std::set<std::string> oSetTargetSubdirs;
        std::map<std::string, VSIDIREntry> oMapExistingTargetFiles;
        // Enumerate existing target files and directories
        if (poTargetDir)
        {
            while (true)
            {
                const auto entry = VSIGetNextDirEntry(poTargetDir.get());
                if (!entry)
                    break;
                const auto osDstName =
                    NormalizeDirSeparatorForDstFilename(entry->pszName);
                if (VSI_ISDIR(entry->nMode))
                {
                    oSetTargetSubdirs.insert(osDstName);
                }
                else
                {
                    oMapExistingTargetFiles.insert(
                        std::pair<std::string, VSIDIREntry>(osDstName, *entry));
                }
            }
            poTargetDir.reset();
        }
        else
        {
            VSIStatBufL sTarget;
            if (VSIStatL(osTargetDir.c_str(), &sTarget) < 0 &&
                VSIMkdirRecursive(osTargetDir.c_str(), 0755) < 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s",
                         osTargetDir.c_str());
                return false;
            }
        }

        // Enumerate source files and directories
        while (true)
        {
            const auto entry = VSIGetNextDirEntry(poSourceDir.get());
            if (!entry)
                break;
            if (VSI_ISDIR(entry->nMode))
            {
                const auto osDstName =
                    NormalizeDirSeparatorForDstFilename(entry->pszName);
                if (oSetTargetSubdirs.find(osDstName) ==
                    oSetTargetSubdirs.end())
                {
                    const std::string osTargetSubdir(CPLFormFilenameSafe(
                        osTargetDir.c_str(), osDstName.c_str(), nullptr));
                    aoSetDirsToCreate.insert(osTargetSubdir);
                }
            }
            else
            {
                // Split file in possibly multiple chunks
                const vsi_l_offset nChunksLarge =
                    nMaxChunkSize == 0
                        ? 1
                        : (entry->nSize + nMaxChunkSize - 1) / nMaxChunkSize;
                if (nChunksLarge >
                    1000)  // must also be below knMAX_PART_NUMBER for upload
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Too small CHUNK_SIZE w.r.t file size");
                    return false;
                }
                ChunkToCopy chunk;
                chunk.osSrcFilename = entry->pszName;
                chunk.osDstFilename =
                    NormalizeDirSeparatorForDstFilename(entry->pszName);
                chunk.nMTime = entry->nMTime;
                chunk.nTotalSize = entry->nSize;
                chunk.osETag =
                    CSLFetchNameValueDef(entry->papszExtra, "ETag", "");
                const size_t nChunks = static_cast<size_t>(nChunksLarge);
                for (size_t iChunk = 0; iChunk < nChunks; iChunk++)
                {
                    chunk.nStartOffset = iChunk * nMaxChunkSize;
                    chunk.nSize =
                        nChunks == 1
                            ? entry->nSize
                            : std::min(
                                  entry->nSize - chunk.nStartOffset,
                                  static_cast<vsi_l_offset>(nMaxChunkSize));
                    aoChunksToCopy.push_back(chunk);
                    chunk.osETag.clear();
                }
            }
        }
        poSourceDir.reset();

        // Create missing target directories, sorted in lexicographic order
        // so that upper-level directories are listed before subdirectories.
        for (const auto &osTargetSubdir : aoSetDirsToCreate)
        {
            const bool ok =
                (bTargetIsThisFS
                     ? MkdirInternal(osTargetSubdir.c_str(), 0755, false)
                     : VSIMkdir(osTargetSubdir.c_str(), 0755)) == 0;
            if (!ok)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s",
                         osTargetSubdir.c_str());
                return false;
            }
        }

        // Collect source files to copy
        const size_t nChunkCount = aoChunksToCopy.size();
        for (size_t iChunk = 0; iChunk < nChunkCount; ++iChunk)
        {
            const auto &chunk = aoChunksToCopy[iChunk];
            if (chunk.nStartOffset != 0)
                continue;
            const std::string osSubSource(
                CPLFormFilenameSafe(osSourceWithoutSlash.c_str(),
                                    chunk.osSrcFilename.c_str(), nullptr));
            const std::string osSubTarget(CPLFormFilenameSafe(
                osTargetDir.c_str(), chunk.osDstFilename.c_str(), nullptr));
            bool bSkip = false;
            const auto oIterExistingTarget =
                oMapExistingTargetFiles.find(chunk.osDstFilename);
            if (oIterExistingTarget != oMapExistingTargetFiles.end() &&
                oIterExistingTarget->second.nSize == chunk.nTotalSize)
            {
                if (bDownloadFromNetworkToLocal)
                {
                    if (CanSkipDownloadFromNetworkToLocal(
                            osSubSource.c_str(), osSubTarget.c_str(),
                            chunk.nMTime, oIterExistingTarget->second.nMTime,
                            [&chunk](const char *) -> std::string
                            { return chunk.osETag; }))
                    {
                        bSkip = true;
                    }
                }
                else if (bUploadFromLocalToNetwork)
                {
                    VSILFILE *fpIn = nullptr;
                    if (CanSkipUploadFromLocalToNetwork(
                            fpIn, osSubSource.c_str(), osSubTarget.c_str(),
                            chunk.nMTime, oIterExistingTarget->second.nMTime,
                            [&oIterExistingTarget](const char *) -> std::string
                            {
                                return std::string(CSLFetchNameValueDef(
                                    oIterExistingTarget->second.papszExtra,
                                    "ETag", ""));
                            }))
                    {
                        bSkip = true;
                    }
                    if (fpIn)
                        VSIFCloseL(fpIn);
                }
                else
                {

                    if (eSyncStrategy == SyncStrategy::TIMESTAMP &&
                        chunk.nMTime < oIterExistingTarget->second.nMTime)
                    {
                        // The target is more recent than the source.
                        // Nothing to do
                        CPLDebug(GetDebugKey(),
                                 "%s is older than %s. "
                                 "Do not replace %s assuming it was used to "
                                 "upload %s",
                                 osSubSource.c_str(), osSubTarget.c_str(),
                                 osSubTarget.c_str(), osSubSource.c_str());
                        bSkip = true;
                    }
                }
            }

            if (!bSkip)
            {
                anIndexToCopy.push_back(iChunk);
                nTotalSize += chunk.nTotalSize;
                if (chunk.nSize < chunk.nTotalSize)
                {
                    if (bDownloadFromNetworkToLocal)
                    {
                        // Suppress target file as we're going to open in wb+
                        // mode for parallelized writing
                        VSIUnlink(osSubTarget.c_str());
                    }
                    else if (bSupportsParallelMultipartUpload)
                    {
                        auto poS3HandleHelper =
                            std::unique_ptr<IVSIS3LikeHandleHelper>(
                                CreateHandleHelper(osSubTarget.c_str() +
                                                       GetFSPrefix().size(),
                                                   false));
                        if (poS3HandleHelper == nullptr)
                            return false;

                        const auto osUploadID =
                            poTargetFSMultipartHandler->InitiateMultipartUpload(
                                osSubTarget, poS3HandleHelper.get(),
                                oRetryParameters,
                                aosObjectCreationOptions.List());
                        if (osUploadID.empty())
                        {
                            return false;
                        }
                        MultiPartDef def;
                        def.osUploadID = osUploadID;
                        def.nExpectedCount = static_cast<int>(
                            (chunk.nTotalSize + chunk.nSize - 1) / chunk.nSize);
                        def.nTotalSize = chunk.nTotalSize;
                        oMapMultiPartDefs[osSubTarget] = std::move(def);
                    }
                    else
                    {
                        CPLAssert(false);
                    }

                    // Include all remaining chunks of the same file
                    while (iChunk + 1 < nChunkCount &&
                           aoChunksToCopy[iChunk + 1].nStartOffset > 0)
                    {
                        ++iChunk;
                        anIndexToCopy.push_back(iChunk);
                    }
                }
            }
        }

        const int nThreads = std::min(std::max(1, nRequestedThreads),
                                      static_cast<int>(anIndexToCopy.size()));
        if (nThreads <= nMinThreads)
        {
            // Proceed to file copy
            bool ret = true;
            uint64_t nAccSize = 0;
            for (const size_t iChunk : anIndexToCopy)
            {
                const auto &chunk = aoChunksToCopy[iChunk];
                CPLAssert(chunk.nStartOffset == 0);
                const std::string osSubSource(
                    CPLFormFilenameSafe(osSourceWithoutSlash.c_str(),
                                        chunk.osSrcFilename.c_str(), nullptr));
                const std::string osSubTarget(CPLFormFilenameSafe(
                    osTargetDir.c_str(), chunk.osDstFilename.c_str(), nullptr));
                // coverity[divide_by_zero]
                void *pScaledProgress = GDALCreateScaledProgress(
                    double(nAccSize) / nTotalSize,
                    double(nAccSize + chunk.nSize) / nTotalSize, pProgressFunc,
                    pProgressData);
                ret =
                    CopyFile(osSubSource.c_str(), osSubTarget.c_str(), nullptr,
                             chunk.nSize, aosObjectCreationOptions.List(),
                             GDALScaledProgress, pScaledProgress) == 0;
                GDALDestroyScaledProgress(pScaledProgress);
                if (!ret)
                {
                    break;
                }
                nAccSize += chunk.nSize;
            }

            return ret;
        }
    }
    else
    {
        std::string osMsg("Copying of ");
        osMsg += osSourceWithoutSlash;

        VSIStatBufL sTarget;
        osTarget = pszTarget;
        bool bTargetIsFile = false;
        sTarget.st_size = 0;
        if (VSIStatL(osTarget.c_str(), &sTarget) == 0)
        {
            bTargetIsFile = true;
            if (VSI_ISDIR(sTarget.st_mode))
            {
                osTarget = CPLFormFilenameSafe(
                    osTarget.c_str(), CPLGetFilename(pszSource), nullptr);
                bTargetIsFile = VSIStatL(osTarget.c_str(), &sTarget) == 0 &&
                                !CPL_TO_BOOL(VSI_ISDIR(sTarget.st_mode));
            }
        }

        if (eSyncStrategy == SyncStrategy::TIMESTAMP && bTargetIsFile &&
            !bDownloadFromNetworkToLocal && !bUploadFromLocalToNetwork &&
            sSource.st_size == sTarget.st_size &&
            sSource.st_mtime < sTarget.st_mtime)
        {
            // The target is more recent than the source. Nothing to do
            CPLDebug(GetDebugKey(),
                     "%s is older than %s. "
                     "Do not replace %s assuming it was used to "
                     "upload %s",
                     osSource.c_str(), osTarget.c_str(), osTarget.c_str(),
                     osSource.c_str());
            if (pProgressFunc)
            {
                pProgressFunc(1.0, osMsg.c_str(), pProgressData);
            }
            return true;
        }

        // Download from network to local file system ?
        if (bTargetIsFile && bDownloadFromNetworkToLocal &&
            sSource.st_size == sTarget.st_size)
        {
            if (CanSkipDownloadFromNetworkToLocal(
                    osSourceWithoutSlash.c_str(), osTarget.c_str(),
                    sSource.st_mtime, sTarget.st_mtime,
                    [this](const char *pszFilename) -> std::string
                    {
                        FileProp cachedFileProp;
                        if (GetCachedFileProp(
                                GetURLFromFilename(pszFilename).c_str(),
                                cachedFileProp))
                        {
                            return cachedFileProp.ETag;
                        }
                        return std::string();
                    }))
            {
                if (pProgressFunc)
                {
                    pProgressFunc(1.0, osMsg.c_str(), pProgressData);
                }
                return true;
            }
        }

        VSILFILE *fpIn = nullptr;

        // Upload from local file system to network ?
        if (bUploadFromLocalToNetwork && sSource.st_size == sTarget.st_size)
        {
            if (CanSkipUploadFromLocalToNetwork(
                    fpIn, osSourceWithoutSlash.c_str(), osTarget.c_str(),
                    sSource.st_mtime, sTarget.st_mtime,
                    [this](const char *pszFilename) -> std::string
                    {
                        FileProp cachedFileProp;
                        if (GetCachedFileProp(
                                GetURLFromFilename(pszFilename).c_str(),
                                cachedFileProp))
                        {
                            return cachedFileProp.ETag;
                        }
                        return std::string();
                    }))
            {
                if (pProgressFunc)
                {
                    pProgressFunc(1.0, osMsg.c_str(), pProgressData);
                }
                return true;
            }
        }

        // Split file in possibly multiple chunks
        const vsi_l_offset nChunksLarge =
            nMaxChunkSize == 0
                ? 1
                : (sSource.st_size + nMaxChunkSize - 1) / nMaxChunkSize;
        if (nChunksLarge >
            1000)  // must also be below knMAX_PART_NUMBER for upload
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too small CHUNK_SIZE w.r.t file size");
            return false;
        }
        ChunkToCopy chunk;
        chunk.nMTime = sSource.st_mtime;
        chunk.nTotalSize = sSource.st_size;
        nTotalSize = chunk.nTotalSize;
        const size_t nChunks = static_cast<size_t>(nChunksLarge);
        for (size_t iChunk = 0; iChunk < nChunks; iChunk++)
        {
            chunk.nStartOffset = iChunk * nMaxChunkSize;
            chunk.nSize =
                nChunks == 1
                    ? sSource.st_size
                    : std::min(sSource.st_size - chunk.nStartOffset,
                               static_cast<vsi_l_offset>(nMaxChunkSize));
            aoChunksToCopy.push_back(chunk);
            anIndexToCopy.push_back(iChunk);

            if (nChunks > 1)
            {
                if (iChunk == 0)
                {
                    if (bDownloadFromNetworkToLocal)
                    {
                        // Suppress target file as we're going to open in wb+
                        // mode for parallelized writing
                        VSIUnlink(osTarget.c_str());
                    }
                    else if (bSupportsParallelMultipartUpload)
                    {
                        auto poS3HandleHelper =
                            std::unique_ptr<IVSIS3LikeHandleHelper>(
                                CreateHandleHelper(osTarget.c_str() +
                                                       GetFSPrefix().size(),
                                                   false));
                        if (poS3HandleHelper == nullptr)
                            return false;

                        const auto osUploadID =
                            poTargetFSMultipartHandler->InitiateMultipartUpload(
                                osTarget, poS3HandleHelper.get(),
                                oRetryParameters,
                                aosObjectCreationOptions.List());
                        if (osUploadID.empty())
                        {
                            return false;
                        }
                        MultiPartDef def;
                        def.osUploadID = osUploadID;
                        def.nExpectedCount = static_cast<int>(
                            (chunk.nTotalSize + chunk.nSize - 1) / chunk.nSize);
                        def.nTotalSize = chunk.nTotalSize;
                        oMapMultiPartDefs[osTarget] = std::move(def);
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
            }
        }

        const int nThreads = std::min(std::max(1, nRequestedThreads),
                                      static_cast<int>(anIndexToCopy.size()));
        if (nThreads <= nMinThreads)
        {
            bool bRet =
                CopyFile(osSourceWithoutSlash.c_str(), osTarget.c_str(), fpIn,
                         sSource.st_size, aosObjectCreationOptions.List(),
                         pProgressFunc, pProgressData) == 0;
            if (fpIn)
            {
                VSIFCloseL(fpIn);
            }
            return bRet;
        }
        if (fpIn)
        {
            VSIFCloseL(fpIn);
        }
    }

    const int nThreads = std::min(std::max(1, nRequestedThreads),
                                  static_cast<int>(anIndexToCopy.size()));

    struct JobQueue
    {
        IVSIS3LikeFSHandler *poFS;
        IVSIS3LikeFSHandlerWithMultipartUpload *poTargetFSMultipartHandler;
        const std::vector<ChunkToCopy> &aoChunksToCopy;
        const std::vector<size_t> &anIndexToCopy;
        std::map<std::string, MultiPartDef> &oMapMultiPartDefs;
        volatile int iCurIdx = 0;
        volatile bool ret = true;
        volatile bool stop = false;
        std::string osSourceDir{};
        std::string osTargetDir{};
        std::string osSource{};
        std::string osTarget{};
        std::mutex sMutex{};
        uint64_t nTotalCopied = 0;
        bool bSupportsParallelMultipartUpload = false;
        size_t nMaxChunkSize = 0;
        const CPLHTTPRetryParameters &oRetryParameters;
        const CPLStringList &aosObjectCreationOptions;

        JobQueue(IVSIS3LikeFSHandler *poFSIn,
                 IVSIS3LikeFSHandlerWithMultipartUpload
                     *poTargetFSMultipartHandlerIn,
                 const std::vector<ChunkToCopy> &aoChunksToCopyIn,
                 const std::vector<size_t> &anIndexToCopyIn,
                 std::map<std::string, MultiPartDef> &oMapMultiPartDefsIn,
                 const std::string &osSourceDirIn,
                 const std::string &osTargetDirIn,
                 const std::string &osSourceIn, const std::string &osTargetIn,
                 bool bSupportsParallelMultipartUploadIn,
                 size_t nMaxChunkSizeIn,
                 const CPLHTTPRetryParameters &oRetryParametersIn,
                 const CPLStringList &aosObjectCreationOptionsIn)
            : poFS(poFSIn),
              poTargetFSMultipartHandler(poTargetFSMultipartHandlerIn),
              aoChunksToCopy(aoChunksToCopyIn), anIndexToCopy(anIndexToCopyIn),
              oMapMultiPartDefs(oMapMultiPartDefsIn),
              osSourceDir(osSourceDirIn), osTargetDir(osTargetDirIn),
              osSource(osSourceIn), osTarget(osTargetIn),
              bSupportsParallelMultipartUpload(
                  bSupportsParallelMultipartUploadIn),
              nMaxChunkSize(nMaxChunkSizeIn),
              oRetryParameters(oRetryParametersIn),
              aosObjectCreationOptions(aosObjectCreationOptionsIn)
        {
        }

        JobQueue(const JobQueue &) = delete;
        JobQueue &operator=(const JobQueue &) = delete;
    };

    const auto threadFunc = [](void *pDataIn)
    {
        struct ProgressData
        {
            uint64_t nFileSize;
            double dfLastPct;
            JobQueue *queue;

            static int CPL_STDCALL progressFunc(double pct, const char *,
                                                void *pProgressDataIn)
            {
                ProgressData *pProgress =
                    static_cast<ProgressData *>(pProgressDataIn);
                const auto nInc = static_cast<uint64_t>(
                    (pct - pProgress->dfLastPct) * pProgress->nFileSize + 0.5);
                pProgress->queue->sMutex.lock();
                pProgress->queue->nTotalCopied += nInc;
                pProgress->queue->sMutex.unlock();
                pProgress->dfLastPct = pct;
                return TRUE;
            }
        };

        JobQueue *queue = static_cast<JobQueue *>(pDataIn);
        while (!queue->stop)
        {
            const int idx = CPLAtomicInc(&(queue->iCurIdx)) - 1;
            if (static_cast<size_t>(idx) >= queue->anIndexToCopy.size())
            {
                queue->stop = true;
                break;
            }
            const auto &chunk =
                queue->aoChunksToCopy[queue->anIndexToCopy[idx]];
            const std::string osSubSource(
                queue->osTargetDir.empty()
                    ? queue->osSource
                    : CPLFormFilenameSafe(queue->osSourceDir.c_str(),
                                          chunk.osSrcFilename.c_str(),
                                          nullptr));
            const std::string osSubTarget(
                queue->osTargetDir.empty()
                    ? queue->osTarget
                    : CPLFormFilenameSafe(queue->osTargetDir.c_str(),
                                          chunk.osDstFilename.c_str(),
                                          nullptr));

            ProgressData progressData;
            progressData.nFileSize = chunk.nSize;
            progressData.dfLastPct = 0;
            progressData.queue = queue;
            if (chunk.nSize < chunk.nTotalSize)
            {
                const size_t nSizeToRead = static_cast<size_t>(chunk.nSize);
                bool bSuccess = false;
                if (queue->bSupportsParallelMultipartUpload)
                {
                    const auto iter =
                        queue->oMapMultiPartDefs.find(osSubTarget);
                    CPLAssert(iter != queue->oMapMultiPartDefs.end());

                    VSILFILE *fpIn = VSIFOpenL(osSubSource.c_str(), "rb");
                    void *pBuffer = VSI_MALLOC_VERBOSE(nSizeToRead);
                    auto poS3HandleHelper =
                        std::unique_ptr<IVSIS3LikeHandleHelper>(
                            queue->poFS->CreateHandleHelper(
                                osSubTarget.c_str() +
                                    queue->poFS->GetFSPrefix().size(),
                                false));
                    if (fpIn && pBuffer && poS3HandleHelper &&
                        VSIFSeekL(fpIn, chunk.nStartOffset, SEEK_SET) == 0 &&
                        VSIFReadL(pBuffer, 1, nSizeToRead, fpIn) == nSizeToRead)
                    {
                        const int nPartNumber =
                            1 + (queue->nMaxChunkSize == 0
                                     ? 0 /* shouldn't happen */
                                     : static_cast<int>(chunk.nStartOffset /
                                                        queue->nMaxChunkSize));
                        const std::string osEtag =
                            queue->poTargetFSMultipartHandler->UploadPart(
                                osSubTarget, nPartNumber,
                                iter->second.osUploadID, chunk.nStartOffset,
                                pBuffer, nSizeToRead, poS3HandleHelper.get(),
                                queue->oRetryParameters,
                                queue->aosObjectCreationOptions.List());
                        if (!osEtag.empty())
                        {
                            std::lock_guard<std::mutex> lock(queue->sMutex);
                            iter->second.nCountValidETags++;
                            iter->second.aosEtags.resize(
                                std::max(nPartNumber,
                                         static_cast<int>(
                                             iter->second.aosEtags.size())));
                            iter->second.aosEtags[nPartNumber - 1] = osEtag;
                            bSuccess = true;
                        }
                    }
                    if (fpIn)
                        VSIFCloseL(fpIn);
                    VSIFree(pBuffer);
                }
                else
                {
                    bSuccess =
                        CopyChunk(osSubSource.c_str(), osSubTarget.c_str(),
                                  chunk.nStartOffset, nSizeToRead);
                }
                if (bSuccess)
                {
                    ProgressData::progressFunc(1.0, "", &progressData);
                }
                else
                {
                    queue->ret = false;
                    queue->stop = true;
                }
            }
            else
            {
                CPLAssert(chunk.nStartOffset == 0);
                if (queue->poFS->CopyFile(
                        osSubSource.c_str(), osSubTarget.c_str(), nullptr,
                        chunk.nTotalSize,
                        queue->aosObjectCreationOptions.List(),
                        ProgressData::progressFunc, &progressData) != 0)
                {
                    queue->ret = false;
                    queue->stop = true;
                }
            }
        }
    };

    JobQueue sJobQueue(this, poTargetFSMultipartHandler, aoChunksToCopy,
                       anIndexToCopy, oMapMultiPartDefs, osSourceWithoutSlash,
                       osTargetDir, osSourceWithoutSlash, osTarget,
                       bSupportsParallelMultipartUpload, nMaxChunkSize,
                       oRetryParameters, aosObjectCreationOptions);

    if (CPLTestBool(CPLGetConfigOption("VSIS3_SYNC_MULTITHREADING", "YES")))
    {
        std::vector<CPLJoinableThread *> ahThreads;
        for (int i = 0; i < nThreads; i++)
        {
            auto hThread = CPLCreateJoinableThread(threadFunc, &sJobQueue);
            if (!hThread)
            {
                sJobQueue.ret = false;
                sJobQueue.stop = true;
                break;
            }
            ahThreads.push_back(hThread);
        }
        if (pProgressFunc)
        {
            while (!sJobQueue.stop)
            {
                CPLSleep(0.1);
                sJobQueue.sMutex.lock();
                const auto nTotalCopied = sJobQueue.nTotalCopied;
                sJobQueue.sMutex.unlock();
                // coverity[divide_by_zero]
                if (!pProgressFunc(double(nTotalCopied) / nTotalSize, "",
                                   pProgressData))
                {
                    sJobQueue.ret = false;
                    sJobQueue.stop = true;
                }
            }
            if (sJobQueue.ret)
            {
                pProgressFunc(1.0, "", pProgressData);
            }
        }
        for (auto hThread : ahThreads)
        {
            CPLJoinThread(hThread);
        }
    }
    else
    {
        // Only for simulation case
        threadFunc(&sJobQueue);
    }

    // Finalize multipart uploads
    if (sJobQueue.ret && bSupportsParallelMultipartUpload)
    {
        std::set<std::string> oSetKeysToRemove;
        for (const auto &kv : oMapMultiPartDefs)
        {
            auto poS3HandleHelper =
                std::unique_ptr<IVSIS3LikeHandleHelper>(CreateHandleHelper(
                    kv.first.c_str() + GetFSPrefix().size(), false));
            sJobQueue.ret = false;
            if (poS3HandleHelper)
            {
                CPLAssert(kv.second.nCountValidETags ==
                          kv.second.nExpectedCount);
                if (poTargetFSMultipartHandler->CompleteMultipart(
                        kv.first, kv.second.osUploadID, kv.second.aosEtags,
                        kv.second.nTotalSize, poS3HandleHelper.get(),
                        oRetryParameters))
                {
                    sJobQueue.ret = true;
                    oSetKeysToRemove.insert(kv.first);

                    InvalidateCachedData(poS3HandleHelper->GetURL().c_str());
                    InvalidateDirContent(CPLGetDirnameSafe(kv.first.c_str()));
                }
            }
        }
        for (const auto &key : oSetKeysToRemove)
        {
            oMapMultiPartDefs.erase(key);
        }
    }

    return sJobQueue.ret;
}

/************************************************************************/
/*                    MultipartUploadGetCapabilities()                  */
/************************************************************************/

bool IVSIS3LikeFSHandlerWithMultipartUpload::MultipartUploadGetCapabilities(
    int *pbNonSequentialUploadSupported, int *pbParallelUploadSupported,
    int *pbAbortSupported, size_t *pnMinPartSize, size_t *pnMaxPartSize,
    int *pnMaxPartCount)
{
    if (pbNonSequentialUploadSupported)
        *pbNonSequentialUploadSupported =
            SupportsNonSequentialMultipartUpload();
    if (pbParallelUploadSupported)
        *pbParallelUploadSupported = SupportsParallelMultipartUpload();
    if (pbAbortSupported)
        *pbAbortSupported = SupportsMultipartAbort();
    if (pnMinPartSize)
        *pnMinPartSize = GetMinimumPartSizeInMiB();
    if (pnMaxPartSize)
        *pnMaxPartSize = GetMaximumPartSizeInMiB();
    if (pnMaxPartCount)
        *pnMaxPartCount = GetMaximumPartCount();
    return true;
}

/************************************************************************/
/*                         MultipartUploadStart()                       */
/************************************************************************/

char *IVSIS3LikeFSHandlerWithMultipartUpload::MultipartUploadStart(
    const char *pszFilename, CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;
    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), false));
    if (poHandleHelper == nullptr)
        return nullptr;
    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);

    const std::string osRet = InitiateMultipartUpload(
        pszFilename, poHandleHelper.get(), oRetryParameters, papszOptions);
    if (osRet.empty())
        return nullptr;
    return CPLStrdup(osRet.c_str());
}

/************************************************************************/
/*                       MultipartUploadAddPart()                       */
/************************************************************************/

char *IVSIS3LikeFSHandlerWithMultipartUpload::MultipartUploadAddPart(
    const char *pszFilename, const char *pszUploadId, int nPartNumber,
    vsi_l_offset nFileOffset, const void *pData, size_t nDataLength,
    CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;
    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), false));
    if (poHandleHelper == nullptr)
        return nullptr;
    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);

    const std::string osRet = UploadPart(
        pszFilename, nPartNumber, pszUploadId, nFileOffset, pData, nDataLength,
        poHandleHelper.get(), oRetryParameters, papszOptions);
    if (osRet.empty())
        return nullptr;
    return CPLStrdup(osRet.c_str());
}

/************************************************************************/
/*                         MultipartUploadEnd()                         */
/************************************************************************/

bool IVSIS3LikeFSHandlerWithMultipartUpload::MultipartUploadEnd(
    const char *pszFilename, const char *pszUploadId, size_t nPartIdsCount,
    const char *const *apszPartIds, vsi_l_offset nTotalSize, CSLConstList)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return false;
    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), false));
    if (poHandleHelper == nullptr)
        return false;
    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);

    std::vector<std::string> aosTags;
    for (size_t i = 0; i < nPartIdsCount; ++i)
        aosTags.emplace_back(apszPartIds[i]);
    return CompleteMultipart(pszFilename, pszUploadId, aosTags, nTotalSize,
                             poHandleHelper.get(), oRetryParameters);
}

/************************************************************************/
/*                         MultipartUploadAbort()                       */
/************************************************************************/

bool IVSIS3LikeFSHandlerWithMultipartUpload::MultipartUploadAbort(
    const char *pszFilename, const char *pszUploadId, CSLConstList)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return false;
    auto poHandleHelper = std::unique_ptr<IVSIS3LikeHandleHelper>(
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), false));
    if (poHandleHelper == nullptr)
        return false;
    const CPLStringList aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename));
    const CPLHTTPRetryParameters oRetryParameters(aosHTTPOptions);
    return AbortMultipart(pszFilename, pszUploadId, poHandleHelper.get(),
                          oRetryParameters);
}

/************************************************************************/
/*                             VSIS3Handle()                            */
/************************************************************************/

VSIS3Handle::VSIS3Handle(VSIS3FSHandler *poFSIn, const char *pszFilename,
                         VSIS3HandleHelper *poS3HandleHelper)
    : IVSIS3LikeHandle(poFSIn, pszFilename,
                       poS3HandleHelper->GetURLNoKVP().c_str()),
      m_poS3HandleHelper(poS3HandleHelper)
{
}

/************************************************************************/
/*                            ~VSIS3Handle()                            */
/************************************************************************/

VSIS3Handle::~VSIS3Handle()
{
    delete m_poS3HandleHelper;
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSIS3Handle::GetCurlHeaders(const std::string &osVerb,
                            const struct curl_slist *psExistingHeaders)
{
    return m_poS3HandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIS3Handle::CanRestartOnError(const char *pszErrorMsg,
                                    const char *pszHeaders, bool bSetError)
{
    if (m_poS3HandleHelper->CanRestartOnError(pszErrorMsg, pszHeaders,
                                              bSetError))
    {
        SetURL(m_poS3HandleHelper->GetURL().c_str());
        return true;
    }
    return false;
}

} /* end of namespace cpl */

#endif  // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                      VSIInstallS3FileHandler()                       */
/************************************************************************/

/*!
 \brief Install /vsis3/ Amazon S3 file system handler (requires libcurl)

 \verbatim embed:rst
 See :ref:`/vsis3/ documentation <vsis3>`
 \endverbatim

 @since GDAL 2.1
 */
void VSIInstallS3FileHandler(void)
{
    VSIFileManager::InstallHandler("/vsis3/",
                                   new cpl::VSIS3FSHandler("/vsis3/"));
}

#endif /* HAVE_CURL */
