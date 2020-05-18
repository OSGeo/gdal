/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for AWS S3
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

#include "cpl_atomic_ops.h"
#include "cpl_port.h"
#include "cpl_http.h"
#include "cpl_md5.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_time.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_vsil_curl_class.h"

#include <algorithm>
#include <functional>
#include <set>
#include <map>
#include <memory>

#include "cpl_aws.h"

CPL_CVSID("$Id$")

#ifndef HAVE_CURL


void VSIInstallS3FileHandler( void )
{
    // Not supported.
}

#else

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

#define ENABLE_DEBUG 0

constexpr int knMAX_PART_NUMBER = 10000; // Limitation from S3

namespace cpl {

/************************************************************************/
/*                             VSIDIRS3                                 */
/************************************************************************/

struct VSIDIRS3: public VSIDIR
{
    int nRecurseDepth = 0;

    CPLString osNextMarker{};
    std::vector<std::unique_ptr<VSIDIREntry>> aoEntries{};
    int nPos = 0;

    CPLString osBucket{};
    CPLString osObjectKey{};
    VSICurlFilesystemHandler* poFS = nullptr;
    IVSIS3LikeFSHandler* poS3FS = nullptr;
    IVSIS3LikeHandleHelper* poS3HandleHelper = nullptr;
    int nMaxFiles = 0;
    bool bCacheEntries = true;

    explicit VSIDIRS3(IVSIS3LikeFSHandler *poFSIn): poFS(poFSIn), poS3FS(poFSIn) {}
    explicit VSIDIRS3(VSICurlFilesystemHandler *poFSIn): poFS(poFSIn) {}
    ~VSIDIRS3()
    {
        delete poS3HandleHelper;
    }

    VSIDIRS3(const VSIDIRS3&) = delete;
    VSIDIRS3& operator=(const VSIDIRS3&) = delete;

    const VSIDIREntry* NextDirEntry() override;

    bool IssueListDir();
    bool AnalyseS3FileList( const CPLString& osBaseURL,
                            const char* pszXML,
                            bool bIgnoreGlacierStorageClass,
                            bool& bIsTruncated );
    void clear();
};

/************************************************************************/
/*                                clear()                               */
/************************************************************************/

void VSIDIRS3::clear()
{
    osNextMarker.clear();
    nPos = 0;
    aoEntries.clear();
}

/************************************************************************/
/*                        AnalyseS3FileList()                           */
/************************************************************************/

bool VSIDIRS3::AnalyseS3FileList(
    const CPLString& osBaseURL,
    const char* pszXML,
    bool bIgnoreGlacierStorageClass,
    bool &bIsTruncated)
{
#if DEBUG_VERBOSE
    const char *pszDebugPrefix = poS3FS ? poS3FS->GetDebugKey() : "S3";
    CPLDebug(pszDebugPrefix, "%s", pszXML);
#endif

    CPLXMLNode* psTree = CPLParseXMLString(pszXML);
    if( psTree == nullptr )
        return false;
    CPLXMLNode* psListBucketResult = CPLGetXMLNode(psTree, "=ListBucketResult");
    CPLXMLNode* psListAllMyBucketsResultBuckets =
        (psListBucketResult != nullptr ) ? nullptr :
        CPLGetXMLNode(psTree, "=ListAllMyBucketsResult.Buckets");

    bool ret = true;

    bIsTruncated = false;
    if( psListBucketResult )
    {
        ret = false;
        CPLString osPrefix = CPLGetXMLValue(psListBucketResult, "Prefix", "");
        if( osPrefix.empty() )
        {
            // in the case of an empty bucket
            ret = true;
        }
        bIsTruncated = CPLTestBool(
            CPLGetXMLValue(psListBucketResult, "IsTruncated", "false"));

        // Count the number of occurrences of a path. Can be 1 or 2. 2 in the
        // case that both a filename and directory exist
        std::map<CPLString, int> aoNameCount;
        for( CPLXMLNode* psIter = psListBucketResult->psChild;
             psIter != nullptr; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( strcmp(psIter->pszValue, "Contents") == 0 )
            {
                ret = true;
                const char* pszKey = CPLGetXMLValue(psIter, "Key", nullptr);
                if( pszKey && strlen(pszKey) > osPrefix.size() )
                {
                    aoNameCount[pszKey + osPrefix.size()] ++;
                }
            }
            else if( strcmp(psIter->pszValue, "CommonPrefixes") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Prefix", nullptr);
                if( pszKey && strncmp(pszKey, osPrefix, osPrefix.size()) == 0 )
                {
                    CPLString osKey = pszKey;
                    if( !osKey.empty() && osKey.back() == '/' )
                        osKey.resize(osKey.size()-1);
                    if( osKey.size() > osPrefix.size() )
                    {
                        ret = true;
                        aoNameCount[osKey.c_str() + osPrefix.size()] ++;
                    }
                }
            }
        }

        for( CPLXMLNode* psIter = psListBucketResult->psChild;
             psIter != nullptr; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( strcmp(psIter->pszValue, "Contents") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Key", nullptr);
                if( bIsTruncated && nRecurseDepth < 0 && pszKey )
                {
                    osNextMarker = pszKey;
                }
                if( pszKey && strlen(pszKey) > osPrefix.size() )
                {
                    const char* pszStorageClass = CPLGetXMLValue(psIter,
                        "StorageClass", "");
                    if( bIgnoreGlacierStorageClass &&
                        EQUAL(pszStorageClass, "GLACIER") )
                    {
                        continue;
                    }

                    aoEntries.push_back(
                        std::unique_ptr<VSIDIREntry>(new VSIDIREntry()));
                    auto& entry = aoEntries.back();
                    entry->pszName = CPLStrdup(pszKey + osPrefix.size());
                    entry->nSize = static_cast<GUIntBig>(
                        CPLAtoGIntBig(CPLGetXMLValue(psIter, "Size", "0")));
                    entry->bSizeKnown = true;
                    entry->nMode = entry->pszName[0] != 0 &&
                        entry->pszName[strlen(entry->pszName) - 1] == '/' ? S_IFDIR : S_IFREG;
                    if( entry->nMode == S_IFDIR && aoNameCount[entry->pszName] < 2 )
                    {
                        entry->pszName[strlen(entry->pszName) - 1] = 0;
                    }
                    entry->bModeKnown = true;

                    CPLString ETag = CPLGetXMLValue(psIter, "ETag", "");
                    if( ETag.size() > 2 && ETag[0] == '"' &&
                        ETag.back() == '"')
                    {
                        ETag = ETag.substr(1, ETag.size()-2);
                        entry->papszExtra = CSLSetNameValue(
                            entry->papszExtra, "ETag", ETag.c_str());
                    }

                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMin = 0;
                    int nSec = 0;
                    if( sscanf( CPLGetXMLValue(psIter, "LastModified", ""),
                                "%04d-%02d-%02dT%02d:%02d:%02d",
                                &nYear, &nMonth, &nDay,
                                &nHour, &nMin, &nSec ) == 6 )
                    {
                        struct tm brokendowntime;
                        brokendowntime.tm_year = nYear - 1900;
                        brokendowntime.tm_mon = nMonth - 1;
                        brokendowntime.tm_mday = nDay;
                        brokendowntime.tm_hour = nHour;
                        brokendowntime.tm_min = nMin;
                        brokendowntime.tm_sec = nSec;
                        entry->nMTime =
                                CPLYMDHMSToUnixTime(&brokendowntime);
                        entry->bMTimeKnown = true;
                    }

                    if( nMaxFiles != 1 && bCacheEntries )
                    {
                        FileProp prop;
                        prop.eExists = EXIST_YES;
                        prop.bHasComputedFileSize = true;
                        prop.fileSize = entry->nSize;
                        prop.bIsDirectory = (entry->nMode == S_IFDIR);
                        prop.mTime = static_cast<time_t>(entry->nMTime);
                        prop.ETag = ETag;

                        CPLString osCachedFilename =
                            osBaseURL + CPLAWSURLEncode(osPrefix,false) +
                            CPLAWSURLEncode(entry->pszName,false);
#if DEBUG_VERBOSE
                        CPLDebug(pszDebugPrefix, "Cache %s", osCachedFilename.c_str());
#endif
                        poFS->SetCachedFileProp(osCachedFilename, prop);
                    }

                    if( nMaxFiles > 0 && aoEntries.size() >= static_cast<unsigned>(nMaxFiles) )
                        break;
                }
            }
            else if( strcmp(psIter->pszValue, "CommonPrefixes") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Prefix", nullptr);
                if( pszKey && strncmp(pszKey, osPrefix, osPrefix.size()) == 0 )
                {
                    CPLString osKey = pszKey;
                    if( !osKey.empty() && osKey.back() == '/' )
                        osKey.resize(osKey.size()-1);
                    if( osKey.size() > osPrefix.size() )
                    {
                        aoEntries.push_back(
                            std::unique_ptr<VSIDIREntry>(new VSIDIREntry()));
                        auto& entry = aoEntries.back();
                        entry->pszName = CPLStrdup(osKey.c_str() + osPrefix.size());
                        if( aoNameCount[entry->pszName] == 2 )
                        {
                            // Add a / suffix to disambiguish the situation
                            // Normally we don't suffix directories with /, but
                            // we have no alternative here
                            CPLString osTemp(entry->pszName);
                            osTemp += '/';
                            CPLFree(entry->pszName);
                            entry->pszName = CPLStrdup(osTemp);
                        }
                        entry->nMode = S_IFDIR;
                        entry->bModeKnown = true;

                        if( nMaxFiles != 1 && bCacheEntries )
                        {
                            FileProp prop;
                            prop.eExists = EXIST_YES;
                            prop.bIsDirectory = true;
                            prop.bHasComputedFileSize = true;
                            prop.fileSize = 0;
                            prop.mTime = 0;

                            CPLString osCachedFilename =
                                osBaseURL + CPLAWSURLEncode(osPrefix,false) +
                                CPLAWSURLEncode(entry->pszName,false);
#if DEBUG_VERBOSE
                            CPLDebug(pszDebugPrefix, "Cache %s", osCachedFilename.c_str());
#endif
                            poFS->SetCachedFileProp(osCachedFilename, prop);
                        }

                        if( nMaxFiles > 0 && aoEntries.size() >= static_cast<unsigned>(nMaxFiles) )
                            break;
                    }
                }
            }
        }

        if( nRecurseDepth == 0 )
        {
            osNextMarker = CPLGetXMLValue(psListBucketResult, "NextMarker", "");
        }
    }
    else if( psListAllMyBucketsResultBuckets != nullptr )
    {
        CPLXMLNode* psIter = psListAllMyBucketsResultBuckets->psChild;
        for( ; psIter != nullptr; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( strcmp(psIter->pszValue, "Bucket") == 0 )
            {
                const char* pszName = CPLGetXMLValue(psIter, "Name", nullptr);
                if( pszName )
                {
                    aoEntries.push_back(
                        std::unique_ptr<VSIDIREntry>(new VSIDIREntry()));
                    auto& entry = aoEntries.back();
                    entry->pszName = CPLStrdup(pszName);
                    entry->nMode = S_IFDIR;
                    entry->bModeKnown = true;

                    if( nMaxFiles != 1 && bCacheEntries )
                    {
                        FileProp prop;
                        prop.eExists = EXIST_YES;
                        prop.bIsDirectory = true;
                        prop.bHasComputedFileSize = true;
                        prop.fileSize = 0;
                        prop.mTime = 0;

                        CPLString osCachedFilename = osBaseURL + CPLAWSURLEncode(pszName, false);
#if DEBUG_VERBOSE
                        CPLDebug(pszDebugPrefix, "Cache %s", osCachedFilename.c_str());
#endif
                        poFS->SetCachedFileProp(osCachedFilename, prop);
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
    if( nMaxFiles > 0 && nMaxFiles <= 100 &&
        (osMaxKeys.empty() || nMaxFiles < atoi(osMaxKeys)) )
    {
        osMaxKeys.Printf("%d", nMaxFiles);
    }

    const CPLString l_osNextMarker(osNextMarker);
    clear();

    while( true )
    {
        poS3HandleHelper->ResetQueryParameters();
        const CPLString osBaseURL(poS3HandleHelper->GetURL());

        CURL* hCurlHandle = curl_easy_init();

        if( !osBucket.empty() )
        {
            if( nRecurseDepth == 0 )
                poS3HandleHelper->AddQueryParameter("delimiter", "/");
            if( !l_osNextMarker.empty() )
                poS3HandleHelper->AddQueryParameter("marker", l_osNextMarker);
            if( !osMaxKeys.empty() )
                poS3HandleHelper->AddQueryParameter("max-keys", osMaxKeys);
            if( !osObjectKey.empty() )
                poS3HandleHelper->AddQueryParameter("prefix", osObjectKey + "/");
        }

        struct curl_slist* headers =
            VSICurlSetOptions(hCurlHandle, poS3HandleHelper->GetURL(), nullptr);

        headers = VSICurlMergeHeaders(headers,
                               poS3HandleHelper->GetCurlHeaders("GET", headers));
        // Disable automatic redirection
        curl_easy_setopt(hCurlHandle, CURLOPT_FOLLOWLOCATION, 0 );

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, nullptr);

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, poFS, poS3HandleHelper);

        if( requestHelper.sWriteFuncData.pBuffer == nullptr)
        {
            curl_easy_cleanup(hCurlHandle);
            return false;
        }

        if( response_code != 200 )
        {
            bool bUpdateMap = true;
            if( requestHelper.sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(requestHelper.sWriteFuncData.pBuffer,
                                                    requestHelper.sWriteFuncHeaderData.pBuffer,
                                                    false, &bUpdateMap) )
            {
                if( bUpdateMap )
                {
                    poS3FS->UpdateMapFromHandle(poS3HandleHelper);
                }
            }
            else
            {
                CPLDebug(poS3FS->GetDebugKey(), "%s",
                         requestHelper.sWriteFuncData.pBuffer
                         ? requestHelper.sWriteFuncData.pBuffer : "(null)");
                curl_easy_cleanup(hCurlHandle);
                return false;
            }
        }
        else
        {
            const bool bIgnoreGlacier = CPLTestBool(
                CPLGetConfigOption("CPL_VSIL_CURL_IGNORE_GLACIER_STORAGE", "YES"));
            bool bIsTruncated;
            bool ret = AnalyseS3FileList( osBaseURL,
                                          requestHelper.sWriteFuncData.pBuffer,
                                          bIgnoreGlacier,
                                          bIsTruncated );

            curl_easy_cleanup(hCurlHandle);
            return ret;
        }

        curl_easy_cleanup(hCurlHandle);
    }
}

/************************************************************************/
/*                           NextDirEntry()                             */
/************************************************************************/

const VSIDIREntry* VSIDIRS3::NextDirEntry()
{
    while( true )
    {
        if( nPos < static_cast<int>(aoEntries.size()) )
        {
            auto& entry = aoEntries[nPos];
            nPos ++;
            return entry.get();
        }
        if( osNextMarker.empty() )
        {
            return nullptr;
        }
        if( !IssueListDir() )
        {
            return nullptr;
        }
    }
}

/************************************************************************/
/*                          AnalyseS3FileList()                         */
/************************************************************************/

bool VSICurlFilesystemHandler::AnalyseS3FileList(
    const CPLString& osBaseURL,
    const char* pszXML,
    CPLStringList& osFileList,
    int nMaxFiles,
    bool bIgnoreGlacierStorageClass,
    bool& bIsTruncated )
{
    VSIDIRS3 oDir(this);
    oDir.nMaxFiles = nMaxFiles;
    bool ret =
        oDir.AnalyseS3FileList(osBaseURL, pszXML,
                               bIgnoreGlacierStorageClass, bIsTruncated);
    for(const auto &entry: oDir.aoEntries )
    {
        osFileList.AddString(entry->pszName);
    }
    return ret;
}

/************************************************************************/
/*                         VSIS3FSHandler                               */
/************************************************************************/

class VSIS3FSHandler final : public IVSIS3LikeFSHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIS3FSHandler)

    std::set<CPLString> DeleteObjects(const char* pszBucket, const char* pszXML);

  protected:
    VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
    CPLString GetURLFromFilename( const CPLString& osFilename ) override;

    const char* GetDebugKey() const override { return "S3"; }

    IVSIS3LikeHandleHelper* CreateHandleHelper(
    const char* pszURI, bool bAllowNoObject) override;

    CPLString GetFSPrefix() override { return "/vsis3/"; }

    void ClearCache() override;

  public:
    VSIS3FSHandler() = default;
    ~VSIS3FSHandler() override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError ) override;

    void UpdateMapFromHandle( IVSIS3LikeHandleHelper * poS3HandleHelper )
        override;
    void UpdateHandleFromMap( IVSIS3LikeHandleHelper * poS3HandleHelper )
        override;

    const char* GetOptions() override;

    char* GetSignedURL( const char* pszFilename, CSLConstList papszOptions ) override;

    int* UnlinkBatch( CSLConstList papszFiles ) override;
    int RmdirRecursive( const char* pszDirname ) override;

    char** GetFileMetadata( const char * pszFilename, const char* pszDomain,
                            CSLConstList papszOptions ) override;

    bool   SetFileMetadata( const char * pszFilename,
                            CSLConstList papszMetadata,
                            const char* pszDomain,
                            CSLConstList papszOptions ) override;
};

/************************************************************************/
/*                            VSIS3Handle                               */
/************************************************************************/

class VSIS3Handle final : public IVSIS3LikeHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIS3Handle)

    VSIS3HandleHelper* m_poS3HandleHelper = nullptr;

  protected:
    struct curl_slist* GetCurlHeaders(
        const CPLString& osVerb,
        const struct curl_slist* psExistingHeaders ) override;
    bool CanRestartOnError( const char*, const char*, bool ) override;
    bool AllowAutomaticRedirection() override
        { return m_poS3HandleHelper->AllowAutomaticRedirection(); }

  public:
    VSIS3Handle( VSIS3FSHandler* poFS,
                 const char* pszFilename,
                 VSIS3HandleHelper* poS3HandleHelper );
    ~VSIS3Handle() override;
};


/************************************************************************/
/*                         VSIS3WriteHandle()                           */
/************************************************************************/

VSIS3WriteHandle::VSIS3WriteHandle( IVSIS3LikeFSHandler* poFS,
                                    const char* pszFilename,
                                    IVSIS3LikeHandleHelper* poS3HandleHelper,
                                    bool bUseChunked ) :
        m_poFS(poFS), m_osFilename(pszFilename),
        m_poS3HandleHelper(poS3HandleHelper),
        m_bUseChunked(bUseChunked),
        m_nMaxRetry(atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)))),
        // coverity[tainted_data]
        m_dfRetryDelay(CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY))))
{
    // AWS S3 does not support chunked PUT in a convenient way, since you must
    // know in advance the total size... See
    // http://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-streaming.html
    // So we must use the mulipart upload mechanism.
    // But this mechanism is not supported by GS. Luckily it does support
    // standard "Transfer-Encoding: chunked" PUT mechanism
    // So two different implementations.

    if( !m_bUseChunked )
    {
        const int nChunkSizeMB = atoi(
            CPLGetConfigOption("VSIS3_CHUNK_SIZE",
                    CPLGetConfigOption("VSIOSS_CHUNK_SIZE", "50")));
        if( nChunkSizeMB <= 0 || nChunkSizeMB > 1000 )
            m_nBufferSize = 0;
        else
            m_nBufferSize = nChunkSizeMB * 1024 * 1024;

        // For testing only !
        const char* pszChunkSizeBytes =
            CPLGetConfigOption("VSIS3_CHUNK_SIZE_BYTES",
                CPLGetConfigOption("VSIOSS_CHUNK_SIZE_BYTES", nullptr));
        if( pszChunkSizeBytes )
            m_nBufferSize = atoi(pszChunkSizeBytes);
        if( m_nBufferSize <= 0 || m_nBufferSize > 1000 * 1024 * 1024 )
            m_nBufferSize = 50 * 1024 * 1024;

        m_pabyBuffer = static_cast<GByte *>(VSIMalloc(m_nBufferSize));
        if( m_pabyBuffer == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot allocate working buffer for %s",
                     m_poFS->GetFSPrefix().c_str());
        }
    }
}

/************************************************************************/
/*                        ~VSIS3WriteHandle()                           */
/************************************************************************/

VSIS3WriteHandle::~VSIS3WriteHandle()
{
    Close();
    delete m_poS3HandleHelper;
    CPLFree(m_pabyBuffer);
    if( m_hCurlMulti )
    {
        if( m_hCurl )
        {
            curl_multi_remove_handle(m_hCurlMulti, m_hCurl);
            curl_easy_cleanup(m_hCurl);
        }
        curl_multi_cleanup(m_hCurlMulti);
    }
    CPLFree(m_sWriteFuncHeaderData.pBuffer);
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIS3WriteHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if( !((nWhence == SEEK_SET && nOffset == m_nCurOffset) ||
          (nWhence == SEEK_CUR && nOffset == 0) ||
          (nWhence == SEEK_END && nOffset == 0)) )
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

vsi_l_offset VSIS3WriteHandle::Tell()
{
    return m_nCurOffset;
}

/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

size_t VSIS3WriteHandle::Read( void * /* pBuffer */, size_t /* nSize */,
                               size_t /* nMemb */ )
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

CPLString IVSIS3LikeFSHandler::InitiateMultipartUpload(
            const std::string& osFilename,
            IVSIS3LikeHandleHelper *poS3HandleHelper,
            int nMaxRetry,
            double dfRetryDelay)
{
    CPLString osUploadID;
    bool bRetry;
    int nRetryCount = 0;
    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("uploads", "");
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              poS3HandleHelper->GetURL().c_str(),
                              nullptr));
        headers = VSICurlMergeHeaders(headers,
                        poS3HandleHelper->GetCurlHeaders("POST", headers));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

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
                            poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else if( requestHelper.sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(requestHelper.sWriteFuncData.pBuffer,
                                                    requestHelper.sWriteFuncHeaderData.pBuffer,
                                                    false) )
            {
                UpdateMapFromHandle(poS3HandleHelper);
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
            InvalidateCachedData(
                poS3HandleHelper->GetURL().c_str());
            InvalidateDirContent( CPLGetDirname(osFilename.c_str()) );

            CPLXMLNode* psNode =
                CPLParseXMLString( requestHelper.sWriteFuncData.pBuffer );
            if( psNode )
            {
                osUploadID =
                    CPLGetXMLValue(
                        psNode, "=InitiateMultipartUploadResult.UploadId", "");
                CPLDebug(GetDebugKey(),
                         "UploadId: %s", osUploadID.c_str());
                CPLDestroyXMLNode(psNode);
            }
            if( osUploadID.empty() )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "InitiateMultipartUpload of %s failed: cannot get UploadId",
                    osFilename.c_str());
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );
    return osUploadID;
}

/************************************************************************/
/*                           UploadPart()                               */
/************************************************************************/

bool VSIS3WriteHandle::UploadPart()
{
    ++m_nPartNumber;
    if( m_nPartNumber > knMAX_PART_NUMBER )
    {
        m_bError = true;
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "%d parts have been uploaded for %s failed. "
            "This is the maximum. "
            "Increase VSIS3_CHUNK_SIZE to a higher value (e.g. 500 for 500 MB)",
            knMAX_PART_NUMBER,
            m_osFilename.c_str());
        return false;
    }
    const CPLString osEtag =
        m_poFS->UploadPart(m_osFilename, m_nPartNumber, m_osUploadID,
                           m_pabyBuffer, m_nBufferOff,
                           m_poS3HandleHelper,
                           m_nMaxRetry, m_dfRetryDelay);
    m_nBufferOff = 0;
    if( !osEtag.empty() )
    {
        m_aosEtags.push_back(osEtag);
    }
    return !osEtag.empty();
}

namespace {
    struct PutData
    {
        const GByte* pabyData;
        size_t       nOff;
        size_t       nTotalSize;

        static size_t ReadCallBackBuffer( char *buffer, size_t size,
                                          size_t nitems, void *instream )
        {
            PutData* poThis = static_cast<PutData *>(instream);
            const size_t nSizeMax = size * nitems;
            const size_t nSizeToWrite =
                std::min(nSizeMax, poThis->nTotalSize - poThis->nOff);
            memcpy(buffer, poThis->pabyData + poThis->nOff, nSizeToWrite);
            poThis->nOff += nSizeToWrite;
            return nSizeToWrite;
        }
    };
}

CPLString IVSIS3LikeFSHandler::UploadPart(const CPLString& osFilename,
                                          int nPartNumber,
                                          const std::string& osUploadID,
                                          const void* pabyBuffer,
                                          size_t nBufferSize,
                                          IVSIS3LikeHandleHelper *poS3HandleHelper,
                                          int nMaxRetry,
                                          double dfRetryDelay)
{

    bool bRetry;
    int nRetryCount = 0;
    CPLString osEtag;

    do
    {
        bRetry = false;

        CURL* hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("partNumber",
                                            CPLSPrintf("%d", nPartNumber));
        poS3HandleHelper->AddQueryParameter("uploadId", osUploadID);
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                         PutData::ReadCallBackBuffer);
        PutData putData;
        putData.pabyData = static_cast<const GByte*>(pabyBuffer);
        putData.nOff = 0;
        putData.nTotalSize = nBufferSize;
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, &putData);
        curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, nBufferSize);

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                            poS3HandleHelper->GetURL().c_str(),
                            nullptr));
        headers = VSICurlMergeHeaders(headers,
                        poS3HandleHelper->GetCurlHeaders("PUT", headers,
                                                            pabyBuffer,
                                                            nBufferSize));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        if( response_code != 200 || requestHelper.sWriteFuncHeaderData.pBuffer == nullptr )
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
                            poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                        requestHelper.sWriteFuncData.pBuffer ?
                        requestHelper.sWriteFuncData.pBuffer : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "UploadPart(%d) of %s failed",
                            nPartNumber, osFilename.c_str());
            }
        }
        else
        {
            CPLString osHeader(requestHelper.sWriteFuncHeaderData.pBuffer);
            size_t nPos = osHeader.ifind("ETag: ");
            if( nPos != std::string::npos )
            {
                osEtag = osHeader.substr(nPos + strlen("ETag: "));
                const size_t nPosEOL = osEtag.find("\r");
                if( nPosEOL != std::string::npos )
                    osEtag.resize(nPosEOL);
                CPLDebug(GetDebugKey(), "Etag for part %d is %s",
                         nPartNumber, osEtag.c_str());
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "UploadPart(%d) of %s (uploadId = %s) failed",
                        nPartNumber, osFilename.c_str(), osUploadID.c_str());
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );

    return osEtag;
}

/************************************************************************/
/*                      ReadCallBackBufferChunked()                     */
/************************************************************************/

size_t VSIS3WriteHandle::ReadCallBackBufferChunked( char *buffer, size_t size,
                                             size_t nitems, void *instream )
{
    VSIS3WriteHandle* poThis = static_cast<VSIS3WriteHandle *>(instream);
    if( poThis->m_nChunkedBufferSize == 0 )
    {
        //CPLDebug("VSIS3WriteHandle", "Writing 0 byte (finish)");
        return 0;
    }
    const size_t nSizeMax = size * nitems;
    size_t nSizeToWrite = nSizeMax;
    size_t nChunkedBufferRemainingSize =
                poThis->m_nChunkedBufferSize - poThis->m_nChunkedBufferOff;
    if( nChunkedBufferRemainingSize < nSizeToWrite )
        nSizeToWrite = nChunkedBufferRemainingSize;
    memcpy(buffer,
           static_cast<const GByte*>(poThis->m_pBuffer) + poThis->m_nChunkedBufferOff,
           nSizeToWrite);
    poThis->m_nChunkedBufferOff += nSizeToWrite;
    //CPLDebug("VSIS3WriteHandle", "Writing %d bytes", nSizeToWrite);
    return nSizeToWrite;
}

/************************************************************************/
/*                          WriteChunked()                              */
/************************************************************************/

size_t
VSIS3WriteHandle::WriteChunked( const void *pBuffer, size_t nSize, size_t nMemb )
{
    const size_t nBytesToWrite = nSize * nMemb;

    if( m_hCurlMulti == nullptr )
    {
        m_hCurlMulti = curl_multi_init();
    }

    double dfRetryDelay = m_dfRetryDelay;
    int nRetryCount = 0;
    // We can only easily retry at the first chunk of a transfer
    bool bCanRetry = (m_hCurl == nullptr);
    bool bRetry;
    do
    {
        bRetry = false;
        struct curl_slist* headers = nullptr;
        if( m_hCurl == nullptr )
        {
            CURL* hCurlHandle = curl_easy_init();
            curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                            ReadCallBackBufferChunked);
            curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);

            VSICURLInitWriteFuncStruct(&m_sWriteFuncHeaderData, nullptr, nullptr, nullptr);
            curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &m_sWriteFuncHeaderData);
            curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                            VSICurlHandleWriteFunc);

            headers = static_cast<struct curl_slist*>(
                CPLHTTPSetOptions(hCurlHandle,
                                m_poS3HandleHelper->GetURL().c_str(),
                                nullptr));
            headers = VSICurlMergeHeaders(headers,
                            m_poS3HandleHelper->GetCurlHeaders("PUT", headers));
            curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

            m_osCurlErrBuf.resize(CURL_ERROR_SIZE+1);
            curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, &m_osCurlErrBuf[0] );

            curl_multi_add_handle(m_hCurlMulti, hCurlHandle);
            m_hCurl = hCurlHandle;
        }

        m_pBuffer = pBuffer;
        m_nChunkedBufferOff = 0;
        m_nChunkedBufferSize = nBytesToWrite;

        int repeats = 0;
        while( m_nChunkedBufferOff <  m_nChunkedBufferSize && !bRetry )
        {
            int still_running;

            memset(&m_osCurlErrBuf[0], 0, m_osCurlErrBuf.size());

            while (curl_multi_perform(m_hCurlMulti, &still_running) ==
                                            CURLM_CALL_MULTI_PERFORM &&
                m_nChunkedBufferOff <  m_nChunkedBufferSize)
            {
                // loop
            }
            if( !still_running || m_nChunkedBufferOff == m_nChunkedBufferSize )
                break;

            CURLMsg *msg;
            do {
                int msgq = 0;
                msg = curl_multi_info_read(m_hCurlMulti, &msgq);
                if(msg && (msg->msg == CURLMSG_DONE))
                {
                    CURL *e = msg->easy_handle;
                    if( e == m_hCurl )
                    {
                        long response_code;
                        curl_easy_getinfo(m_hCurl, CURLINFO_RESPONSE_CODE,
                                            &response_code);
                        if( response_code != 200 && response_code != 201 )
                        {
                            // Look if we should attempt a retry
                            const double dfNewRetryDelay =
                                bCanRetry ? CPLHTTPGetNewRetryDelay(
                                static_cast<int>(response_code), dfRetryDelay,
                                m_sWriteFuncHeaderData.pBuffer,
                                m_osCurlErrBuf.c_str()) : 0.0;

                            curl_multi_remove_handle(m_hCurlMulti, m_hCurl);
                            curl_easy_cleanup(m_hCurl);
                            m_hCurl = nullptr;

                            CPLFree(m_sWriteFuncHeaderData.pBuffer);
                            m_sWriteFuncHeaderData.pBuffer = nullptr;

                            if( dfNewRetryDelay > 0 &&
                                nRetryCount < m_nMaxRetry )
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                            "HTTP error code: %d - %s. "
                                            "Retrying again in %.1f secs",
                                            static_cast<int>(response_code),
                                            m_poS3HandleHelper->GetURL().c_str(),
                                            dfRetryDelay);
                                CPLSleep(dfRetryDelay);
                                dfRetryDelay = dfNewRetryDelay;
                                nRetryCount++;
                                bRetry = true;
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                        "Error %d: %s",
                                        static_cast<int>(response_code),
                                        m_osCurlErrBuf.c_str());

                                curl_slist_free_all(headers);
                                return 0;
                            }
                        }
                    }
                }
            } while(msg);

            CPLMultiPerformWait(m_hCurlMulti, repeats);
        }

        curl_slist_free_all(headers);

        m_pBuffer = nullptr;

        if( !bRetry )
        {
            long response_code;
            curl_easy_getinfo(m_hCurl, CURLINFO_RESPONSE_CODE, &response_code);
            if( response_code != 100 )
            {
                // Look if we should attempt a retry
                const double dfNewRetryDelay =
                    bCanRetry ? CPLHTTPGetNewRetryDelay(
                    static_cast<int>(response_code), dfRetryDelay,
                    m_sWriteFuncHeaderData.pBuffer, m_osCurlErrBuf.c_str()) : 0.0;
                curl_multi_remove_handle(m_hCurlMulti, m_hCurl);
                curl_easy_cleanup(m_hCurl);
                m_hCurl = nullptr;

                CPLFree(m_sWriteFuncHeaderData.pBuffer);
                m_sWriteFuncHeaderData.pBuffer = nullptr;

                if( dfNewRetryDelay > 0 &&
                    nRetryCount < m_nMaxRetry )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                                "HTTP error code: %d - %s. "
                                "Retrying again in %.1f secs",
                                static_cast<int>(response_code),
                                m_poS3HandleHelper->GetURL().c_str(),
                                dfRetryDelay);
                    CPLSleep(dfRetryDelay);
                    dfRetryDelay = dfNewRetryDelay;
                    nRetryCount++;
                    bRetry = true;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                                "Error %d: %s",
                                static_cast<int>(response_code),
                                m_osCurlErrBuf.c_str());
                    return 0;
                }
            }
        }
    }
    while( bRetry );

    return nMemb;
}

/************************************************************************/
/*                        FinishChunkedTransfer()                       */
/************************************************************************/

int VSIS3WriteHandle::FinishChunkedTransfer()
{
    if( m_hCurl == nullptr )
        return -1;

    m_pBuffer = nullptr;
    m_nChunkedBufferOff = 0;
    m_nChunkedBufferSize = 0;

    MultiPerform(m_hCurlMulti);

    long response_code;
    curl_easy_getinfo(m_hCurl, CURLINFO_RESPONSE_CODE, &response_code);
    if( response_code == 200 || response_code == 201 )
    {
        InvalidateParentDirectory();
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Error %d: %s",
                    static_cast<int>(response_code),
                    m_osCurlErrBuf.c_str());
        return -1;
    }
    return 0;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t
VSIS3WriteHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
{
    if( m_bError )
        return 0;

    size_t nBytesToWrite = nSize * nMemb;
    if( nBytesToWrite == 0 )
        return 0;

    if( m_bUseChunked )
    {
        return WriteChunked(pBuffer, nSize, nMemb);
    }

    const GByte* pabySrcBuffer = reinterpret_cast<const GByte*>(pBuffer);
    while( nBytesToWrite > 0 )
    {
        const int nToWriteInBuffer = static_cast<int>(
            std::min(static_cast<size_t>(m_nBufferSize - m_nBufferOff),
                     nBytesToWrite));
        memcpy(m_pabyBuffer + m_nBufferOff, pabySrcBuffer, nToWriteInBuffer);
        pabySrcBuffer += nToWriteInBuffer;
        m_nBufferOff += nToWriteInBuffer;
        m_nCurOffset += nToWriteInBuffer;
        nBytesToWrite -= nToWriteInBuffer;
        if( m_nBufferOff == m_nBufferSize )
        {
            if( m_nCurOffset == static_cast<vsi_l_offset>(m_nBufferSize) )
            {
                m_osUploadID = m_poFS->InitiateMultipartUpload(
                        m_osFilename, m_poS3HandleHelper,
                        m_nMaxRetry, m_dfRetryDelay);
                if( m_osUploadID.empty() )
                {
                    m_bError = true;
                    return 0;
                }
            }
            if( !UploadPart() )
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
/*                                Eof()                                 */
/************************************************************************/

int VSIS3WriteHandle::Eof()
{
    return FALSE;
}

/************************************************************************/
/*                    InvalidateParentDirectory()                       */
/************************************************************************/

void VSIS3WriteHandle::InvalidateParentDirectory()
{
    m_poFS->InvalidateCachedData(
        m_poS3HandleHelper->GetURL().c_str() );

    CPLString osFilenameWithoutSlash(m_osFilename);
    if( !osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/' )
        osFilenameWithoutSlash.resize( osFilenameWithoutSlash.size() - 1 );
    m_poFS->InvalidateDirContent( CPLGetDirname(osFilenameWithoutSlash) );
}

/************************************************************************/
/*                           DoSinglePartPUT()                          */
/************************************************************************/

bool VSIS3WriteHandle::DoSinglePartPUT()
{
    bool bSuccess = true;
    bool bRetry;
    double dfRetryDelay = m_dfRetryDelay;
    int nRetryCount = 0;

    do
    {
        bRetry = false;

        PutData putData;
        putData.pabyData = m_pabyBuffer;
        putData.nOff = 0;
        putData.nTotalSize = m_nBufferOff;

        CURL* hCurlHandle = curl_easy_init();
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, PutData::ReadCallBackBuffer);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, &putData);
        curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              m_poS3HandleHelper->GetURL().c_str(),
                              nullptr));
        headers = VSICurlMergeHeaders(headers,
                        m_poS3HandleHelper->GetCurlHeaders("PUT", headers,
                                                           m_pabyBuffer,
                                                           m_nBufferOff));
        headers = curl_slist_append(headers, "Expect: 100-continue");

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, m_poFS, m_poS3HandleHelper);

        if( response_code != 200 && response_code != 201 )
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                requestHelper.sWriteFuncHeaderData.pBuffer, requestHelper.szCurlErrBuf);
            if( dfNewRetryDelay > 0 &&
                nRetryCount < m_nMaxRetry )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                            "HTTP error code: %d - %s. "
                            "Retrying again in %.1f secs",
                            static_cast<int>(response_code),
                            m_poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else if( requestHelper.sWriteFuncData.pBuffer != nullptr &&
                m_poS3HandleHelper->CanRestartOnError(requestHelper.sWriteFuncData.pBuffer,
                                                      requestHelper.sWriteFuncHeaderData.pBuffer,
                                                      false) )
            {
                m_poFS->UpdateMapFromHandle(m_poS3HandleHelper);
                bRetry = true;
            }
            else
            {
                CPLDebug("S3", "%s",
                         requestHelper.sWriteFuncData.pBuffer
                         ? requestHelper.sWriteFuncData.pBuffer
                         : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "DoSinglePartPUT of %s failed",
                         m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            InvalidateParentDirectory();
        }

        if( requestHelper.sWriteFuncHeaderData.pBuffer != nullptr )
        {
            const char* pzETag = strstr(
                requestHelper.sWriteFuncHeaderData.pBuffer, "ETag: \"");
            if( pzETag )
            {
                pzETag += strlen("ETag: \"");
                const char* pszEndOfETag = strchr(pzETag, '"');
                if( pszEndOfETag )
                {
                    FileProp oFileProp;
                    oFileProp.eExists = EXIST_YES;
                    oFileProp.fileSize = m_nBufferOff;
                    oFileProp.bHasComputedFileSize = true;
                    oFileProp.ETag.assign(pzETag, pszEndOfETag - pzETag);
                    m_poFS->SetCachedFileProp(
                        m_poFS->GetURLFromFilename(m_osFilename), oFileProp);
                }
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );
    return bSuccess;
}

/************************************************************************/
/*                        CompleteMultipart()                           */
/************************************************************************/

bool IVSIS3LikeFSHandler::CompleteMultipart(const CPLString& osFilename,
                                            const CPLString& osUploadID,
                                            const std::vector<CPLString>& aosEtags,
                                            IVSIS3LikeHandleHelper *poS3HandleHelper,
                                            int nMaxRetry,
                                            double dfRetryDelay)
{
    bool bSuccess = true;

    CPLString osXML = "<CompleteMultipartUpload>\n";
    for( size_t i = 0; i < aosEtags.size(); i++ )
    {
        osXML += "<Part>\n";
        osXML += CPLSPrintf("<PartNumber>%d</PartNumber>",
                              static_cast<int>(i+1));
        osXML += "<ETag>" + aosEtags[i] + "</ETag>";
        osXML += "</Part>\n";
    }
    osXML += "</CompleteMultipartUpload>\n";

    int nRetryCount = 0;
    bool bRetry;
    do
    {
        bRetry = false;

        PutData putData;
        putData.pabyData = reinterpret_cast<const GByte*>(osXML.data());
        putData.nOff = 0;
        putData.nTotalSize = osXML.size();

        CURL* hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("uploadId", osUploadID);
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                         PutData::ReadCallBackBuffer);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, &putData);
        curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                        static_cast<int>(osXML.size()));
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                            poS3HandleHelper->GetURL().c_str(),
                            nullptr));
        headers = VSICurlMergeHeaders(headers,
                        poS3HandleHelper->GetCurlHeaders("POST", headers,
                                                            osXML.c_str(),
                                                            osXML.size()));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

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
                            poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else 
            {
                CPLDebug("S3", "%s",
                    requestHelper.sWriteFuncData.pBuffer ? requestHelper.sWriteFuncData.pBuffer : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                        "CompleteMultipart of %s (uploadId=%s) failed",
                        osFilename.c_str(), osUploadID.c_str());
                bSuccess = false;
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );

    return bSuccess;
}

/************************************************************************/
/*                          AbortMultipart()                            */
/************************************************************************/

bool IVSIS3LikeFSHandler::AbortMultipart(const CPLString& osFilename,
                                         const CPLString& osUploadID,
                                         IVSIS3LikeHandleHelper *poS3HandleHelper,
                                         int nMaxRetry,
                                         double dfRetryDelay)
{
    bool bSuccess = true;

    int nRetryCount = 0;
    bool bRetry;
    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("uploadId", osUploadID);
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                            poS3HandleHelper->GetURL().c_str(),
                            nullptr));
        headers = VSICurlMergeHeaders(headers,
                        poS3HandleHelper->GetCurlHeaders("DELETE", headers));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        if( response_code != 204 )
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
                            poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else 
            {
                CPLDebug("S3", "%s",
                        requestHelper.sWriteFuncData.pBuffer ? requestHelper.sWriteFuncData.pBuffer : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                        "AbortMultipart of %s (uploadId=%s) failed",
                        osFilename.c_str(), osUploadID.c_str());
                bSuccess = false;
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );

    return bSuccess;
}

/************************************************************************/
/*                                 Close()                              */
/************************************************************************/

int VSIS3WriteHandle::Close()
{
    int nRet = 0;
    if( !m_bClosed )
    {
        m_bClosed = true;
        if( m_bUseChunked && m_hCurlMulti != nullptr )
        {
            nRet = FinishChunkedTransfer();
        }
        else if( m_osUploadID.empty() )
        {
            if( !m_bError && !DoSinglePartPUT() )
                nRet = -1;
        }
        else
        {
            if( m_bError )
            {
                if( !m_poFS->AbortMultipart(m_osFilename, m_osUploadID,
                                            m_poS3HandleHelper,
                                            m_nMaxRetry, m_dfRetryDelay) )
                    nRet = -1;
            }
            else if( m_nBufferOff > 0 && !UploadPart() )
                nRet = -1;
            else if( m_poFS->CompleteMultipart(
                                     m_osFilename, m_osUploadID,
                                     m_aosEtags, m_poS3HandleHelper,
                                     m_nMaxRetry, m_dfRetryDelay) )
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
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIS3FSHandler::Open( const char *pszFilename,
                                        const char *pszAccess,
                                        bool bSetError)
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr )
    {
        /*if( strchr(pszAccess, '+') != nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "w+ not supported for /vsis3. Only w");
            return nullptr;
        }*/
        VSIS3HandleHelper* poS3HandleHelper =
            VSIS3HandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str(), false);
        if( poS3HandleHelper == nullptr )
            return nullptr;
        UpdateHandleFromMap(poS3HandleHelper);
        VSIS3WriteHandle* poHandle =
            new VSIS3WriteHandle(this, pszFilename, poS3HandleHelper, false);
        if( !poHandle->IsOK() )
        {
            delete poHandle;
            poHandle = nullptr;
        }
        return poHandle;
    }

    if( CPLString(pszFilename).back() != '/' )
    {
        // If there's directory content for the directory where this file belongs to,
        // use it to detect if the object does not exist
        CachedDirList cachedDirList;
        const CPLString osDirname(CPLGetDirname(pszFilename));
        if( STARTS_WITH_CI(osDirname, GetFSPrefix()) &&
            GetCachedDirList(osDirname, cachedDirList) && cachedDirList.bGotFileList )
        {
            const CPLString osFilenameOnly(CPLGetFilename(pszFilename));
            bool bFound = false;
            for( int i = 0; i < cachedDirList.oFileList.size(); i++ )
            {
                if( cachedDirList.oFileList[i] == osFilenameOnly )
                {
                    bFound = true;
                    break;
                }
            }
            if( !bFound )
            {
                return nullptr;
            }
        }
    }

    return
        VSICurlFilesystemHandler::Open(pszFilename, pszAccess, bSetError);
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
    VSICurlFilesystemHandler::ClearCache();

    VSIS3UpdateParams::ClearCache();

    VSIS3HandleHelper::ClearCache();
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char* VSIS3FSHandler::GetOptions()
{
    static CPLString osOptions(
        CPLString("<Options>") +
    "  <Option name='AWS_SECRET_ACCESS_KEY' type='string' "
        "description='Secret access key. To use with AWS_ACCESS_KEY_ID'/>"
    "  <Option name='AWS_ACCESS_KEY_ID' type='string' "
        "description='Access key id'/>"
    "  <Option name='AWS_SESSION_TOKEN' type='string' "
        "description='Session token'/>"
    "  <Option name='AWS_REQUEST_PAYER' type='string' "
        "description='Content of the x-amz-request-payer HTTP header. "
        "Typically \"requester\" for requester-pays buckets'/>"
    "  <Option name='AWS_VIRTUAL_HOSTING' type='boolean' "
        "description='Whether to use virtual hosting server name when the "
        "bucket name is compatible with it' default='YES'/>"
    "  <Option name='AWS_NO_SIGN_REQUEST' type='boolean' "
        "description='Whether to disable signing of requests' default='NO'/>"
    "  <Option name='AWS_DEFAULT_REGION' type='string' "
        "description='AWS S3 default region' default='us-east-1'/>"
    "  <Option name='CPL_AWS_AUTODETECT_EC2' type='boolean' "
        "description='Whether to check Hypervisor & DMI identifiers to "
        "determine if current host is an AWS EC2 instance' default='YES'/>"
    "  <Option name='AWS_DEFAULT_PROFILE' type='string' "
        "description='Name of the profile to use for IAM credentials "
        "retrieval on EC2 instances' default='default'/>"
    "  <Option name='AWS_CONFIG_FILE' type='string' "
        "description='Filename that contains AWS configuration' "
        "default='~/.aws/config'/>"
    "  <Option name='CPL_AWS_CREDENTIALS_FILE' type='string' "
        "description='Filename that contains AWS credentials' "
        "default='~/.aws/credentials'/>"
    "  <Option name='VSIS3_CHUNK_SIZE' type='int' "
        "description='Size in MB for chunks of files that are uploaded. The"
        "default value of 50 MB allows for files up to 500 GB each' "
        "default='50' min='5' max='1000'/>" +
        VSICurlFilesystemHandler::GetOptionsStatic() +
        "</Options>");
    return osOptions.c_str();
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

char* VSIS3FSHandler::GetSignedURL(const char* pszFilename, CSLConstList papszOptions )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    VSIS3HandleHelper* poS3HandleHelper =
        VSIS3HandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str(), false,
                                        papszOptions);
    if( poS3HandleHelper == nullptr )
    {
        return nullptr;
    }

    CPLString osRet(poS3HandleHelper->GetSignedURL(papszOptions));

    delete poS3HandleHelper;
    return CPLStrdup(osRet);
}


/************************************************************************/
/*                           UnlinkBatch()                              */
/************************************************************************/

int* VSIS3FSHandler::UnlinkBatch( CSLConstList papszFiles )
{
    // Implemented using https://docs.aws.amazon.com/AmazonS3/latest/API/API_DeleteObjects.html

    int* panRet = static_cast<int*>(
        CPLCalloc(sizeof(int), CSLCount(papszFiles)));
    CPLStringList aosList;
    CPLString osCurBucket;
    int iStartIndex = -1;
    // For debug / testing only
    const int nBatchSize = atoi(CPLGetConfigOption("CPL_VSIS3_UNLINK_BATCH_SIZE", "1000"));
    for( int i = 0; papszFiles && papszFiles[i]; i++ )
    {
        CPLAssert( STARTS_WITH_CI(papszFiles[i], GetFSPrefix()) );
        const char* pszFilenameWithoutPrefix = papszFiles[i] + GetFSPrefix().size();
        const char* pszSlash = strchr(pszFilenameWithoutPrefix, '/');
        if( !pszSlash )
            return panRet;
        CPLString osBucket;
        osBucket.assign(pszFilenameWithoutPrefix, pszSlash - pszFilenameWithoutPrefix);
        bool bBucketChanged = false;
        if( (osCurBucket.empty() || osCurBucket == osBucket) )
        {
            if( osCurBucket.empty() )
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
        while( bBucketChanged || aosList.size() == nBatchSize ||
               papszFiles[i+1] == nullptr )
        {
            // Compose XML post content
            CPLXMLNode* psXML = CPLCreateXMLNode(nullptr, CXT_Element, "?xml");
            CPLAddXMLAttributeAndValue(psXML, "version", "1.0");
            CPLAddXMLAttributeAndValue(psXML, "encoding", "UTF-8");
            CPLXMLNode* psDelete = CPLCreateXMLNode(nullptr, CXT_Element, "Delete");
            psXML->psNext = psDelete;
            CPLAddXMLAttributeAndValue(psDelete, "xmlns",
                                       "http://s3.amazonaws.com/doc/2006-03-01/");
            CPLXMLNode* psLastChild = psDelete->psChild;
            CPLAssert(psLastChild != nullptr);
            CPLAssert(psLastChild->psNext == nullptr);
            std::map<CPLString, int> mapKeyToIndex;
            for( int j = 0; aosList[j]; ++j )
            {
                CPLXMLNode* psObject =
                    CPLCreateXMLNode(nullptr, CXT_Element, "Object");
                mapKeyToIndex[aosList[j]] = iStartIndex + j;
                CPLCreateXMLElementAndValue(psObject, "Key", aosList[j]);
                psLastChild->psNext = psObject;
                psLastChild = psObject;
            }

            // Run request
            char* pszXML = CPLSerializeXMLTree(psXML);
            CPLDestroyXMLNode(psXML);
            auto oDeletedKeys = DeleteObjects(osCurBucket.c_str(), pszXML);
            CPLFree(pszXML);

            // Mark delete file
            for( const auto& osDeletedKey: oDeletedKeys )
            {
                auto mapKeyToIndexIter = mapKeyToIndex.find(osDeletedKey);
                if( mapKeyToIndexIter != mapKeyToIndex.end() )
                {
                    panRet[mapKeyToIndexIter->second] = true;
                }
            }

            osCurBucket.clear();
            aosList.Clear();
            if( bBucketChanged )
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

int VSIS3FSHandler::RmdirRecursive( const char* pszDirname )
{
    CPLString osDirnameWithoutEndSlash(pszDirname);
    if( !osDirnameWithoutEndSlash.empty() && osDirnameWithoutEndSlash.back() == '/' )
        osDirnameWithoutEndSlash.resize( osDirnameWithoutEndSlash.size() - 1 );

    CPLStringList aosOptions;
    aosOptions.SetNameValue("CACHE_ENTRIES", "FALSE");
    auto poDir = std::unique_ptr<VSIDIR>(OpenDir(osDirnameWithoutEndSlash, -1, aosOptions.List()));
    if( !poDir )
        return -1;
    CPLStringList aosList;
    // For debug / testing only
    const int nBatchSize = atoi(CPLGetConfigOption("CPL_VSIS3_UNLINK_BATCH_SIZE", "1000"));
    while( true )
    {
        auto entry = poDir->NextDirEntry();
        if( entry )
        {
            CPLString osFilename(osDirnameWithoutEndSlash + '/' + entry->pszName);
            if( entry->nMode == S_IFDIR )
                osFilename += '/';
            aosList.AddString(osFilename);
        }
        if( entry == nullptr || aosList.size() == nBatchSize )
        {
            if( entry == nullptr && !osDirnameWithoutEndSlash.empty() )
            {
                aosList.AddString( (osDirnameWithoutEndSlash + '/').c_str() );
            }
            int* ret = UnlinkBatch(aosList.List());
            if( ret == nullptr )
                return -1;
            CPLFree(ret);
            aosList.Clear();
        }
        if( entry == nullptr )
            break;
    }
    PartialClearCache(osDirnameWithoutEndSlash);
    return 0;
}

/************************************************************************/
/*                            DeleteObjects()                           */
/************************************************************************/

std::set<CPLString> VSIS3FSHandler::DeleteObjects(const char* pszBucket,
                                                  const char* pszXML)
{
    auto poS3HandleHelper = std::unique_ptr<VSIS3HandleHelper>(
        VSIS3HandleHelper::BuildFromURI(pszBucket,
                                        GetFSPrefix().c_str(), true));
    if( !poS3HandleHelper )
        return std::set<CPLString>();

    std::set<CPLString> oDeletedKeys;
    bool bRetry;
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry = atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;

    CPLString osContentMD5;
    struct CPLMD5Context context;
    CPLMD5Init(&context);
    CPLMD5Update(&context, reinterpret_cast<unsigned char const *>(pszXML),
                  static_cast<int>(strlen(pszXML)));
    unsigned char hash[16];
    CPLMD5Final(hash, &context);
    char* pszBase64 = CPLBase64Encode(16, hash);
    osContentMD5.Printf("Content-MD5: %s", pszBase64);
    CPLFree(pszBase64);

    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("delete", "");
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS, pszXML );

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              poS3HandleHelper->GetURL().c_str(),
                              nullptr));
        headers = curl_slist_append(headers, "Content-Type: application/xml");
        headers = curl_slist_append(headers, osContentMD5.c_str());
        headers = VSICurlMergeHeaders(headers,
                        poS3HandleHelper->GetCurlHeaders("POST", headers,
                                                         pszXML,
                                                         strlen(pszXML)));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper.get());

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
                            poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else if( requestHelper.sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(requestHelper.sWriteFuncData.pBuffer,
                                                      requestHelper.sWriteFuncHeaderData.pBuffer,
                                                      false) )
            {
                UpdateMapFromHandle(poS3HandleHelper.get());
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
            CPLXMLNode* psXML = CPLParseXMLString(requestHelper.sWriteFuncData.pBuffer);
            if( psXML )
            {
                CPLXMLNode* psDeleteResult =
                    CPLGetXMLNode(psXML, "=DeleteResult");
                if( psDeleteResult )
                {
                    for( CPLXMLNode* psIter = psDeleteResult->psChild;
                                        psIter; psIter = psIter->psNext )
                    {
                        if( psIter->eType == CXT_Element &&
                            strcmp(psIter->pszValue, "Deleted") == 0 )
                        {
                            CPLString osKey = CPLGetXMLValue(psIter, "Key", "");
                            oDeletedKeys.insert(osKey);

                            InvalidateCachedData(
                                (poS3HandleHelper->GetURL() + osKey).c_str() );

                            InvalidateDirContent( CPLGetDirname(
                                (GetFSPrefix() + pszBucket + "/" + osKey).c_str()) );
                        }
                    }
                }
                CPLDestroyXMLNode(psXML);
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );
    return oDeletedKeys;
}

/************************************************************************/
/*                          GetFileMetadata()                           */
/************************************************************************/

char** VSIS3FSHandler::GetFileMetadata( const char* pszFilename,
                                        const char* pszDomain,
                                        CSLConstList papszOptions )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( pszDomain == nullptr || !EQUAL(pszDomain, "TAGS") )
    {
        return VSICurlFilesystemHandler::GetFileMetadata(
                    pszFilename, pszDomain, papszOptions);
    }

    auto poS3HandleHelper = std::unique_ptr<VSIS3HandleHelper>(
        VSIS3HandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str(), false));
    if( !poS3HandleHelper )
        return nullptr;

    bool bRetry;
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry = atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;

    CPLStringList aosTags;
    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("tagging", "");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              poS3HandleHelper->GetURL().c_str(),
                              nullptr));
        headers = VSICurlMergeHeaders(headers,
                        poS3HandleHelper->GetCurlHeaders("GET", headers));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper.get());
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
                            poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else if( requestHelper.sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(requestHelper.sWriteFuncData.pBuffer,
                                                      requestHelper.sWriteFuncHeaderData.pBuffer,
                                                      false) )
            {
                UpdateMapFromHandle(poS3HandleHelper.get());
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
            CPLXMLNode* psXML = CPLParseXMLString(requestHelper.sWriteFuncData.pBuffer);
            if( psXML )
            {
                CPLXMLNode* psTagSet =
                    CPLGetXMLNode(psXML, "=Tagging.TagSet");
                if( psTagSet )
                {
                    for( CPLXMLNode* psIter = psTagSet->psChild;
                                        psIter; psIter = psIter->psNext )
                    {
                        if( psIter->eType == CXT_Element &&
                            strcmp(psIter->pszValue, "Tag") == 0 )
                        {
                            CPLString osKey = CPLGetXMLValue(psIter, "Key", "");
                            CPLString osValue = CPLGetXMLValue(psIter, "Value", "");
                            aosTags.SetNameValue(osKey, osValue);
                        }
                    }
                }
                CPLDestroyXMLNode(psXML);
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );
    return CSLDuplicate(aosTags.List());
}

/************************************************************************/
/*                          SetFileMetadata()                           */
/************************************************************************/

bool VSIS3FSHandler::SetFileMetadata( const char * pszFilename,
                                      CSLConstList papszMetadata,
                                      const char* pszDomain,
                                      CSLConstList /* papszOptions */ )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return false;

    if( pszDomain == nullptr ||
        !(EQUAL(pszDomain, "HEADERS") || EQUAL(pszDomain, "TAGS")) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only HEADERS and TAGS domain are supported");
        return false;
    }

    if( EQUAL(pszDomain, "HEADERS") )
    {
        return CopyObject(pszFilename, pszFilename, papszMetadata) == 0;
    }

    auto poS3HandleHelper = std::unique_ptr<VSIS3HandleHelper>(
        VSIS3HandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str(), false));
    if( !poS3HandleHelper )
        return false;

    bool bRetry;
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    const int nMaxRetry = atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)));
    int nRetryCount = 0;

    // Compose XML post content
    CPLString osXML;
    if( papszMetadata != nullptr && papszMetadata[0] != nullptr )
    {
        CPLXMLNode* psXML = CPLCreateXMLNode(nullptr, CXT_Element, "?xml");
        CPLAddXMLAttributeAndValue(psXML, "version", "1.0");
        CPLAddXMLAttributeAndValue(psXML, "encoding", "UTF-8");
        CPLXMLNode* psTagging = CPLCreateXMLNode(nullptr, CXT_Element, "Tagging");
        psXML->psNext = psTagging;
        CPLAddXMLAttributeAndValue(psTagging, "xmlns",
                                    "http://s3.amazonaws.com/doc/2006-03-01/");
        CPLXMLNode* psTagSet = CPLCreateXMLNode(psTagging, CXT_Element, "TagSet");
        for( int i = 0; papszMetadata && papszMetadata[i]; ++i )
        {
            char* pszKey = nullptr;
            const char* pszValue = CPLParseNameValue(papszMetadata[i], &pszKey);
            if( pszKey && pszValue )
            {
                CPLXMLNode* psTag =
                    CPLCreateXMLNode(psTagSet, CXT_Element, "Tag");
                CPLCreateXMLElementAndValue(psTag, "Key", pszKey);
                CPLCreateXMLElementAndValue(psTag, "Value", pszValue);
            }
            CPLFree(pszKey);
        }

        osXML = CPLSerializeXMLTree(psXML);
        CPLDestroyXMLNode(psXML);
    }

    CPLString osContentMD5;
    if( !osXML.empty() )
    {
        struct CPLMD5Context context;
        CPLMD5Init(&context);
        CPLMD5Update(&context, reinterpret_cast<unsigned char const *>(osXML.c_str()),
                    static_cast<int>(osXML.size()));
        unsigned char hash[16];
        CPLMD5Final(hash, &context);
        char* pszBase64 = CPLBase64Encode(16, hash);
        osContentMD5.Printf("Content-MD5: %s", pszBase64);
        CPLFree(pszBase64);
    }

    bool bRet = false;

    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        poS3HandleHelper->AddQueryParameter("tagging", "");
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, osXML.empty() ? "DELETE" : "PUT");
        if( !osXML.empty() )
        {
            curl_easy_setopt(hCurlHandle, CURLOPT_POSTFIELDS, osXML.c_str() );
        }

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              poS3HandleHelper->GetURL().c_str(),
                              nullptr));
        if( !osXML.empty() )
        {
            headers = curl_slist_append(headers, "Content-Type: application/xml");
            headers = curl_slist_append(headers, osContentMD5.c_str());
            headers = VSICurlMergeHeaders(headers,
                            poS3HandleHelper->GetCurlHeaders("PUT", headers,
                                                            osXML.c_str(),
                                                            osXML.size()));
        }
        else
        {
            headers = VSICurlMergeHeaders(headers,
                            poS3HandleHelper->GetCurlHeaders("DELETE", headers));
        }

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper.get());

        if( (!osXML.empty() && response_code != 200) ||
            (osXML.empty() && response_code != 204) )
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
                            poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else if( requestHelper.sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(requestHelper.sWriteFuncData.pBuffer,
                                                      requestHelper.sWriteFuncHeaderData.pBuffer,
                                                      false) )
            {
                UpdateMapFromHandle(poS3HandleHelper.get());
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
    }
    while( bRetry );
    return bRet;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::MkdirInternal( const char * pszDirname, bool bDoStatCheck )
{
    if( !STARTS_WITH_CI(pszDirname, GetFSPrefix()) )
        return -1;

    CPLString osDirname(pszDirname);
    if( !osDirname.empty() && osDirname.back() != '/' )
        osDirname += "/";

    if( bDoStatCheck )
    {
        VSIStatBufL sStat;
        if( VSIStatL(osDirname, &sStat) == 0 &&
            sStat.st_mode == S_IFDIR )
        {
            CPLDebug(GetDebugKey(), "Directory %s already exists", osDirname.c_str());
            errno = EEXIST;
            return -1;
        }
    }

    VSILFILE* fp = VSIFOpenL(osDirname, "wb");
    if( fp != nullptr )
    {
        CPLErrorReset();
        VSIFCloseL(fp);
        int ret = CPLGetLastErrorType() == CPLE_None ? 0 : -1;
        if( ret == 0 )
        {
            CPLString osDirnameWithoutEndSlash(osDirname);
            osDirnameWithoutEndSlash.resize( osDirnameWithoutEndSlash.size() - 1 );

            InvalidateDirContent( CPLGetDirname(osDirnameWithoutEndSlash) );

            FileProp cachedFileProp;
            GetCachedFileProp(GetURLFromFilename(osDirname), cachedFileProp);
            cachedFileProp.eExists = EXIST_YES;
            cachedFileProp.bIsDirectory = true;
            cachedFileProp.bHasComputedFileSize = true;
            SetCachedFileProp(GetURLFromFilename(osDirname), cachedFileProp);

            RegisterEmptyDir(osDirnameWithoutEndSlash);
            RegisterEmptyDir(osDirname);
        }
        return ret;
    }
    else
    {
        return -1;
    }
}

int IVSIS3LikeFSHandler::Mkdir( const char * pszDirname, long /* nMode */ )
{
    return MkdirInternal(pszDirname, true);
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::Rmdir( const char * pszDirname )
{
    if( !STARTS_WITH_CI(pszDirname, GetFSPrefix()) )
        return -1;

    CPLString osDirname(pszDirname);
    if( !osDirname.empty() && osDirname.back() != '/' )
        osDirname += "/";

    VSIStatBufL sStat;
    if( VSIStatL(osDirname, &sStat) != 0 )
    {
        CPLDebug(GetDebugKey(), "%s is not a object", pszDirname);
        errno = ENOENT;
        return -1;
    }
    else if( sStat.st_mode != S_IFDIR )
    {
        CPLDebug(GetDebugKey(), "%s is not a directory", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    char** papszFileList = ReadDirEx(osDirname, 100);
    bool bEmptyDir = papszFileList == nullptr ||
                     (EQUAL(papszFileList[0], ".") &&
                      papszFileList[1] == nullptr);
    CSLDestroy(papszFileList);
    if( !bEmptyDir )
    {
        CPLDebug(GetDebugKey(), "%s is not empty", pszDirname);
        errno = ENOTEMPTY;
        return -1;
    }


    CPLString osDirnameWithoutEndSlash(osDirname);
    osDirnameWithoutEndSlash.resize( osDirnameWithoutEndSlash.size() - 1 );
    if( osDirnameWithoutEndSlash.find('/', GetFSPrefix().size()) ==
                                                        std::string::npos )
    {
        CPLDebug(GetDebugKey(), "%s is a bucket", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    int ret = DeleteObject(osDirname);
    if( ret == 0 )
    {
        InvalidateDirContent(osDirnameWithoutEndSlash);
    }
    return ret;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                          int nFlags )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return -1;

    memset(pStatBuf, 0, sizeof(VSIStatBufL));
    if( !IsAllowedFilename( pszFilename ) )
        return -1;

    CPLString osFilename(pszFilename);
    if( osFilename.find('/', GetFSPrefix().size()) == std::string::npos )
        osFilename += "/";

    CPLString osFilenameWithoutSlash(osFilename);
    if( osFilenameWithoutSlash.back() == '/' )
        osFilenameWithoutSlash.resize(osFilenameWithoutSlash.size()-1);

    // If there's directory content for the directory where this file belongs to,
    // use it to detect if the object does not exist
    CachedDirList cachedDirList;
    const CPLString osDirname(CPLGetDirname(osFilenameWithoutSlash));
    if( STARTS_WITH_CI(osDirname, GetFSPrefix()) &&
        GetCachedDirList(osDirname, cachedDirList) && cachedDirList.bGotFileList )
    {
        const CPLString osFilenameOnly(CPLGetFilename(osFilenameWithoutSlash));
        bool bFound = false;
        for( int i = 0; i < cachedDirList.oFileList.size(); i++ )
        {
            if( cachedDirList.oFileList[i] == osFilenameOnly )
            {
                bFound = true;
                break;
            }
        }
        if( !bFound )
        {
            return -1;
        }
    }

    if( VSICurlFilesystemHandler::Stat(osFilename, pStatBuf, nFlags) == 0 )
    {
        return 0;
    }

    char** papszRet = ReadDirInternal( osFilename, 100, nullptr );
    int nRet = papszRet ? 0 : -1;
    if( nRet == 0 )
    {
        pStatBuf->st_mtime = 0;
        pStatBuf->st_size = 0;
        pStatBuf->st_mode = S_IFDIR;

        FileProp cachedFileProp;
        GetCachedFileProp(GetURLFromFilename(osFilename), cachedFileProp);
        cachedFileProp.eExists = EXIST_YES;
        cachedFileProp.bIsDirectory = true;
        cachedFileProp.bHasComputedFileSize = true;
        SetCachedFileProp(GetURLFromFilename(osFilename), cachedFileProp);
    }
    CSLDestroy(papszRet);
    return nRet;
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIS3FSHandler::CreateFileHandle(const char* pszFilename)
{
    VSIS3HandleHelper* poS3HandleHelper =
        VSIS3HandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str(), false);
    if( poS3HandleHelper )
    {
        UpdateHandleFromMap(poS3HandleHelper);
        return new VSIS3Handle(this, pszFilename, poS3HandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                          GetURLFromFilename()                         */
/************************************************************************/

CPLString VSIS3FSHandler::GetURLFromFilename( const CPLString& osFilename )
{
    CPLString osFilenameWithoutPrefix = osFilename.substr(GetFSPrefix().size());

    VSIS3HandleHelper* poS3HandleHelper =
        VSIS3HandleHelper::BuildFromURI(osFilenameWithoutPrefix,
                                        GetFSPrefix().c_str(), true);
    if( poS3HandleHelper == nullptr )
    {
        return "";
    }
    UpdateHandleFromMap(poS3HandleHelper);
    CPLString osBaseURL(poS3HandleHelper->GetURL());
    if( !osBaseURL.empty() && osBaseURL.back() == '/' )
        osBaseURL.resize(osBaseURL.size()-1);
    delete poS3HandleHelper;

    return osBaseURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper* VSIS3FSHandler::CreateHandleHelper(const char* pszURI,
                                                          bool bAllowNoObject)
{
    return VSIS3HandleHelper::BuildFromURI(
                                pszURI, GetFSPrefix().c_str(), bAllowNoObject);
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int IVSIS3LikeFSHandler::Unlink( const char *pszFilename )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return -1;

    CPLString osNameWithoutPrefix = pszFilename + GetFSPrefix().size();
    if( osNameWithoutPrefix.find('/') == std::string::npos )
    {
        CPLDebug(GetDebugKey(), "%s is not a file", pszFilename);
        errno = EISDIR;
        return -1;
    }

    VSIStatBufL sStat;
    if( VSIStatL(pszFilename, &sStat) != 0 )
    {
        CPLDebug(GetDebugKey(), "%s is not a object", pszFilename);
        errno = ENOENT;
        return -1;
    }
    else if( sStat.st_mode != S_IFREG )
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

int IVSIS3LikeFSHandler::Rename( const char *oldpath, const char *newpath )
{
    if( !STARTS_WITH_CI(oldpath, GetFSPrefix()) )
        return -1;
    if( !STARTS_WITH_CI(newpath, GetFSPrefix()) )
        return -1;
    VSIStatBufL sStat;
    if( VSIStatL(oldpath, &sStat) != 0 )
    {
        CPLDebug(GetDebugKey(), "%s is not a object", oldpath);
        errno = ENOENT;
        return -1;
    }

    // AWS doesn't like renaming to the same name, and errors out
    // But GCS does like it, and so we might end up killing ourselves !
    // POSIX says renaming on the same file is OK
    if( strcmp(oldpath, newpath) == 0 )
        return 0;

    if( sStat.st_mode == S_IFDIR )
    {
        CPLStringList aosList(VSIReadDir(oldpath));
        Mkdir(newpath, 0755);
        for( int i = 0; i < aosList.size(); i++ )
        {
            CPLString osSrc = CPLFormFilename(oldpath, aosList[i], nullptr);
            CPLString osTarget = CPLFormFilename(newpath, aosList[i], nullptr);
            if( Rename(osSrc, osTarget) != 0 )
            {
                return -1;
            }
        }
        Rmdir(oldpath);
        return 0;
    }
    else
    {
        if( VSIStatL(newpath, &sStat) == 0 && sStat.st_mode == S_IFDIR )
        {
            CPLDebug(GetDebugKey(), "%s already exists and is a directory", newpath);
            errno = ENOTEMPTY;
            return -1;
        }
        if( CopyObject(oldpath, newpath, nullptr) != 0 )
        {
            return -1;
        }
        return DeleteObject(oldpath);
    }
}

/************************************************************************/
/*                            CopyObject()                              */
/************************************************************************/

int IVSIS3LikeFSHandler::CopyObject( const char *oldpath, const char *newpath,
                                     CSLConstList papszMetadata )
{
    CPLString osTargetNameWithoutPrefix = newpath + GetFSPrefix().size();
    std::unique_ptr<IVSIS3LikeHandleHelper> poS3HandleHelper(
        CreateHandleHelper(osTargetNameWithoutPrefix, false));
    if( poS3HandleHelper == nullptr )
    {
        return -1;
    }

    std::string osSourceHeader(poS3HandleHelper->GetCopySourceHeader());
    if( osSourceHeader.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Object copy not supported by this file system");
        return -1;
    }
    osSourceHeader += ": /";
    if( STARTS_WITH(oldpath, "/vsis3/") )
        osSourceHeader += CPLAWSURLEncode(oldpath + GetFSPrefix().size(), false);
    else
        osSourceHeader += (oldpath + GetFSPrefix().size());

    UpdateHandleFromMap(poS3HandleHelper.get());

    int nRet = 0;

    bool bRetry;

    const int nMaxRetry = atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)));
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    int nRetryCount = 0;

    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              poS3HandleHelper->GetURL().c_str(),
                              nullptr));
        headers = curl_slist_append(headers, osSourceHeader.c_str());
        headers = curl_slist_append(headers, "Content-Length: 0"); // Required by GCS, but not by S3
        if( papszMetadata && papszMetadata[0] )
        {
            headers = curl_slist_append(headers, "x-amz-metadata-directive: REPLACE");
            for( int i = 0; papszMetadata[i]; i++ )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(papszMetadata[i], &pszKey);
                if( pszKey && pszValue )
                {
                    headers = curl_slist_append(headers,
                                                CPLSPrintf("%s: %s", pszKey, pszValue));
                }
                CPLFree(pszKey);
            }
        }
        headers = VSICurlMergeHeaders(headers,
                        poS3HandleHelper->GetCurlHeaders("PUT", headers));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper.get());

        if( response_code != 200)
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
                            poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else if( requestHelper.sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(requestHelper.sWriteFuncData.pBuffer,
                                                    requestHelper.sWriteFuncHeaderData.pBuffer,
                                                    false) )
            {
                UpdateMapFromHandle(poS3HandleHelper.get());
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

            CPLString osFilenameWithoutSlash(newpath);
            if( !osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/' )
                osFilenameWithoutSlash.resize( osFilenameWithoutSlash.size() - 1 );

            InvalidateDirContent( CPLGetDirname(osFilenameWithoutSlash) );
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );

    return nRet;
}

/************************************************************************/
/*                           DeleteObject()                             */
/************************************************************************/

int IVSIS3LikeFSHandler::DeleteObject( const char *pszFilename )
{
    CPLString osNameWithoutPrefix = pszFilename + GetFSPrefix().size();
    IVSIS3LikeHandleHelper* poS3HandleHelper =
        CreateHandleHelper(osNameWithoutPrefix, false);
    if( poS3HandleHelper == nullptr )
    {
        return -1;
    }
    UpdateHandleFromMap(poS3HandleHelper);

    int nRet = 0;

    bool bRetry;

    const int nMaxRetry = atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)));
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
    int nRetryCount = 0;
    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              poS3HandleHelper->GetURL().c_str(),
                              nullptr));
        headers = VSICurlMergeHeaders(headers,
                        poS3HandleHelper->GetCurlHeaders("DELETE", headers));

        CurlRequestHelper requestHelper;
        const long response_code =
            requestHelper.perform(hCurlHandle, headers, this, poS3HandleHelper);

        // S3 and GS respond with 204. Azure with 202
        if( response_code != 204 && response_code != 202)
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
                            poS3HandleHelper->GetURL().c_str(),
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                bRetry = true;
            }
            else if( requestHelper.sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(requestHelper.sWriteFuncData.pBuffer,
                                                    requestHelper.sWriteFuncHeaderData.pBuffer,
                                                    false) )
            {
                UpdateMapFromHandle(poS3HandleHelper);
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

            CPLString osFilenameWithoutSlash(pszFilename);
            if( !osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/' )
                osFilenameWithoutSlash.resize( osFilenameWithoutSlash.size() - 1 );

            InvalidateDirContent( CPLGetDirname(osFilenameWithoutSlash) );
        }

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );

    delete poS3HandleHelper;
    return nRet;
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** IVSIS3LikeFSHandler::GetFileList( const char *pszDirname,
                                    int nMaxFiles,
                                    bool* pbGotFileList )
{
    if( ENABLE_DEBUG )
        CPLDebug(GetDebugKey(), "GetFileList(%s)" , pszDirname);

    *pbGotFileList = false;

    char** papszOptions = CSLSetNameValue(nullptr,
                                "MAXFILES", CPLSPrintf("%d", nMaxFiles));
    auto dir = OpenDir(pszDirname, 0, papszOptions);
    CSLDestroy(papszOptions);
    if( !dir )
    {
        return nullptr;
    }
    CPLStringList aosFileList;
    while( true )
    {
        auto entry = dir->NextDirEntry();
        if( !entry )
        {
            break;
        }
        aosFileList.AddString(entry->pszName);

        if( nMaxFiles > 0 && aosFileList.size() >= nMaxFiles )
            break;
    }
    delete dir;
    *pbGotFileList = true;
    return aosFileList.StealList();
}

/************************************************************************/
/*                            OpenDir()                                 */
/************************************************************************/

VSIDIR* IVSIS3LikeFSHandler::OpenDir( const char *pszPath,
                                      int nRecurseDepth,
                                      const char* const *papszOptions)
{
    if( nRecurseDepth > 0)
    {
        return VSIFilesystemHandler::OpenDir(pszPath, nRecurseDepth, papszOptions);
    }

    if( !STARTS_WITH_CI(pszPath, GetFSPrefix()) )
        return nullptr;

    CPLString osDirnameWithoutPrefix = pszPath + GetFSPrefix().size();
    if( !osDirnameWithoutPrefix.empty() &&
                                osDirnameWithoutPrefix.back() == '/' )
    {
        osDirnameWithoutPrefix.resize(osDirnameWithoutPrefix.size()-1);
    }

    CPLString osBucket(osDirnameWithoutPrefix);
    CPLString osObjectKey;
    size_t nSlashPos = osDirnameWithoutPrefix.find('/');
    if( nSlashPos != std::string::npos )
    {
        osBucket = osDirnameWithoutPrefix.substr(0, nSlashPos);
        osObjectKey = osDirnameWithoutPrefix.substr(nSlashPos+1);
    }

    IVSIS3LikeHandleHelper* poS3HandleHelper =
        CreateHandleHelper(osBucket, true);
    if( poS3HandleHelper == nullptr )
    {
        return nullptr;
    }
    UpdateHandleFromMap(poS3HandleHelper);

    VSIDIRS3* dir = new VSIDIRS3(this);
    dir->nRecurseDepth = nRecurseDepth;
    dir->poFS = this;
    dir->poS3HandleHelper = poS3HandleHelper;
    dir->osBucket = osBucket;
    dir->osObjectKey = osObjectKey;
    dir->nMaxFiles = atoi(CSLFetchNameValueDef(papszOptions, "MAXFILES", "0"));
    dir->bCacheEntries = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "CACHE_ENTRIES", "TRUE"));
    if( !dir->IssueListDir() )
    {
        delete dir;
        return nullptr;
    }

    return dir;
}

/************************************************************************/
/*                       ComputeMD5OfLocalFile()                        */
/************************************************************************/

static CPLString ComputeMD5OfLocalFile(VSILFILE* fp)
{
    constexpr size_t nBufferSize = 10 * 4096;
    std::vector<GByte> abyBuffer(nBufferSize, 0);

    struct CPLMD5Context context;
    CPLMD5Init(&context);

    while( true )
    {
        size_t nRead = VSIFReadL(&abyBuffer[0], 1, nBufferSize, fp);
        CPLMD5Update(&context, &abyBuffer[0], static_cast<int>(nRead));
        if( nRead < nBufferSize )
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

bool IVSIS3LikeFSHandler::CopyFile(VSILFILE* fpIn,
                     vsi_l_offset nSourceSize,
                     const char* pszSource,
                     const char* pszTarget,
                     GDALProgressFunc pProgressFunc,
                     void *pProgressData)
{
    CPLString osMsg;
    osMsg.Printf("Copying of %s", pszSource);

    const CPLString osPrefix(GetFSPrefix());
    if( STARTS_WITH(pszSource, osPrefix) &&
        STARTS_WITH(pszTarget, osPrefix) )
    {
        bool bRet = CopyObject(pszSource, pszTarget, nullptr) == 0;
        if( pProgressFunc )
        {
            bRet = pProgressFunc(1.0, osMsg.c_str(), pProgressData) != 0;
        }
        return bRet;
    }

    if( fpIn == nullptr )
    {
        if( STARTS_WITH(pszSource, osPrefix) &&
            (EQUAL(osPrefix, "/vsis3/") ||
             EQUAL(osPrefix, "/vsioss/") ||
             EQUAL(osPrefix, "/vsigs/") ||
             EQUAL(osPrefix, "/vsiaz/") ||
             EQUAL(osPrefix, "/vsiswift/")) )
        {
            // Transform /vsis3/foo insto /vsis3_streaming/foo
            const size_t nPrefixLen = osPrefix.size();
            fpIn = VSIFOpenExL(
                (osPrefix.substr(0, nPrefixLen-1) +
                    "_streaming/"  +
                    (pszSource + nPrefixLen)).c_str(), "rb", TRUE);
        }
        else
            fpIn = VSIFOpenExL(pszSource, "rb", TRUE);
    }
    if( fpIn == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                 pszSource);
        return false;
    }

    VSILFILE* fpOut = VSIFOpenExL(pszTarget, "wb", TRUE);
    if( fpOut == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszTarget);
        VSIFCloseL(fpIn);
        return false;
    }

    bool ret = true;
    constexpr size_t nBufferSize = 10 * 4096;
    std::vector<GByte> abyBuffer(nBufferSize, 0);
    GUIntBig nOffset = 0;
    while( true )
    {
        size_t nRead = VSIFReadL(&abyBuffer[0], 1, nBufferSize, fpIn);
        size_t nWritten = VSIFWriteL(&abyBuffer[0], 1, nRead, fpOut);
        if( nWritten != nRead )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Copying of %s to %s failed",
                     pszSource, pszTarget);
            ret = false;
            break;
        }
        nOffset += nRead;
        if( pProgressFunc && !pProgressFunc(
                    double(nOffset) / nSourceSize, osMsg.c_str(),
                    pProgressData) )
        {
            ret = false;
            break;
        }
        if( nRead < nBufferSize )
        {
            break;
        }
    }

    VSIFCloseL(fpIn);
    if( VSIFCloseL(fpOut) != 0 )
    {
        ret = false;
    }
    return ret;
}

/************************************************************************/
/*                          CopyChunk()                                 */
/************************************************************************/

static bool CopyChunk(const char* pszSource,
                      const char* pszTarget,
                      vsi_l_offset nStartOffset,
                      size_t nChunkSize)
{
    VSILFILE* fpIn = VSIFOpenExL(pszSource, "rb", TRUE);
    if( fpIn == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszSource);
        return false;
    }

    VSILFILE* fpOut = VSIFOpenExL(pszTarget, "wb+", TRUE);
    if( fpOut == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszTarget);
        VSIFCloseL(fpIn);
        return false;
    }

    bool ret = true;
    if( VSIFSeekL(fpIn, nStartOffset, SEEK_SET) < 0 ||
        VSIFSeekL(fpOut, nStartOffset, SEEK_SET) < 0 )
    {
        ret = false;
    }
    else
    {
        void* pBuffer = VSI_MALLOC_VERBOSE(nChunkSize);
        if( pBuffer == nullptr )
        {
            ret = false;
        }
        else
        {
            if( VSIFReadL(pBuffer, 1, nChunkSize, fpIn) != nChunkSize ||
                VSIFWriteL(pBuffer, 1, nChunkSize, fpOut) != nChunkSize )
            {
                ret = false;
            }
        }
        VSIFree(pBuffer);
    }

    VSIFCloseL(fpIn);
    if( VSIFCloseL(fpOut) != 0 )
    {
        ret = false;
    }
    if( !ret )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                    "Copying of %s to %s failed",
                    pszSource, pszTarget);
    }
    return ret;
}

/************************************************************************/
/*                               Sync()                                 */
/************************************************************************/

bool IVSIS3LikeFSHandler::Sync( const char* pszSource, const char* pszTarget,
                            const char* const * papszOptions,
                            GDALProgressFunc pProgressFunc,
                            void *pProgressData,
                            char*** ppapszOutputs  )
{
    if( ppapszOutputs )
    {
        *ppapszOutputs = nullptr;
    }

    CPLString osSource(pszSource);
    CPLString osSourceWithoutSlash(pszSource);
    if( osSourceWithoutSlash.back() == '/' )
    {
        osSourceWithoutSlash.resize(osSourceWithoutSlash.size()-1);
    }

    const int nMaxRetry(atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY))));
    // coverity[tainted_data]
    const double dfRetryDelay(CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY))));

    const bool bRecursive = CPLFetchBool(papszOptions, "RECURSIVE", true);
    const bool bETagStrategy = EQUAL(CSLFetchNameValueDef(
        papszOptions, "SYNC_STRATEGY", "TIMESTAMP"), "ETAG");
    const bool bDownloadFromNetworkToLocal =
        (!STARTS_WITH(pszTarget, "/vsi") || STARTS_WITH(pszTarget, "/vsimem/")) &&
        STARTS_WITH(pszSource, GetFSPrefix());
    const bool bTargetIsThisFS = STARTS_WITH(pszTarget, GetFSPrefix());
    const bool bUploadFromLocalToNetwork =
        (!STARTS_WITH(pszSource, "/vsi") || STARTS_WITH(pszSource, "/vsimem/")) &&
        bTargetIsThisFS;

    // If the source is likely to be a directory, try to issue a ReadDir()
    // if we haven't stat'ed it yet
    std::unique_ptr<VSIDIR> poSourceDir;
    if( STARTS_WITH(pszSource, GetFSPrefix()) && osSource.back() == '/' )
    {
        poSourceDir.reset(VSIOpenDir(osSourceWithoutSlash,
                            bRecursive ? -1 : 0,
                            nullptr));
    }

    VSIStatBufL sSource;
    if( VSIStatL(osSourceWithoutSlash, &sSource) < 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "%s does not exist", pszSource);
        return false;
    }

    const auto CanSkipDownloadFromNetworkToLocal = [this, bETagStrategy](
        const char* l_pszSource,
        const char* l_pszTarget,
        GIntBig sourceTime,
        GIntBig targetTime,
        const std::function<CPLString(const char*)>& getETAGSourceFile)
    {
        if( bETagStrategy )
        {
            VSILFILE* fpOutAsIn = VSIFOpenExL(l_pszTarget, "rb", TRUE);
            if( fpOutAsIn )
            {
                CPLString md5 = ComputeMD5OfLocalFile(fpOutAsIn);
                VSIFCloseL(fpOutAsIn);
                if( getETAGSourceFile(l_pszSource) == md5 )
                {
                    CPLDebug(GetDebugKey(),
                                "%s has already same content as %s",
                            l_pszTarget, l_pszSource);
                    return true;
                }
            }
        }
        else
        {
            if( targetTime <= sourceTime )
            {
                // Our local copy is older than the source, so
                // presumably the source was uploaded from it. Nothing to do
                CPLDebug(GetDebugKey(), "%s is older than %s. "
                            "Do not replace %s assuming it was used to "
                            "upload %s",
                            l_pszTarget, l_pszSource,
                            l_pszTarget, l_pszSource);
                return true;
            }
        }
        return false;
    };

    const auto CanSkipUploadFromLocalToNetwork = [this, bETagStrategy](
        VSILFILE*& l_fpIn,
        const char* l_pszSource,
        const char* l_pszTarget,
        GIntBig sourceTime,
        GIntBig targetTime,
        const std::function<CPLString(const char*)>& getETAGTargetFile)
    {
        if( bETagStrategy )
        {
            l_fpIn = VSIFOpenExL(l_pszSource, "rb", TRUE);
            if( l_fpIn && getETAGTargetFile(l_pszTarget) == ComputeMD5OfLocalFile(l_fpIn) )
            {
                CPLDebug(GetDebugKey(), "%s has already same content as %s",
                         l_pszTarget, l_pszSource);
                VSIFCloseL(l_fpIn);
                l_fpIn = nullptr;
                return true;
            }
        }
        else
        {
            if( targetTime >= sourceTime )
            {
                // The remote copy is more recent than the source, so
                // presumably it was uploaded from the source. Nothing to do
                CPLDebug(GetDebugKey(), "%s is more recent than %s. "
                            "Do not replace %s assuming it was uploaded from "
                            "%s",
                            l_pszTarget, l_pszSource,
                            l_pszTarget, l_pszSource);
                return true;
            }
        }
        return false;
    };

    if( VSI_ISDIR(sSource.st_mode) )
    {
        CPLString osTargetDir(pszTarget);
        if( osSource.back() != '/' )
        {
            osTargetDir = CPLFormFilename(osTargetDir,
                                          CPLGetFilename(pszSource), nullptr);
        }

        if( !poSourceDir )
        {
            poSourceDir.reset(
                VSIOpenDir(osSourceWithoutSlash,
                    bRecursive ? -1 : 0,
                    nullptr));
            if( !poSourceDir )
                return false;
        }

        auto poTargetDir = std::unique_ptr<VSIDIR>(
            VSIOpenDir(osTargetDir, bRecursive ? -1 : 0, nullptr));
        std::set<CPLString> oSetTargetSubdirs;
        std::map<CPLString, VSIDIREntry> oMapExistingTargetFiles;
        // Enumerate existing target files and directories
        if( poTargetDir )
        {
            while( true )
            {
                const auto entry = VSIGetNextDirEntry(poTargetDir.get());
                if( !entry )
                    break;
                if( VSI_ISDIR(entry->nMode) )
                {
                    oSetTargetSubdirs.insert(entry->pszName);
                }
                else
                {
                    oMapExistingTargetFiles.insert(
                        std::pair<CPLString, VSIDIREntry>(entry->pszName, *entry));
                }
            }
            poTargetDir.reset();
        }
        else
        {
            VSIStatBufL sTarget;
            if( VSIStatL(osTargetDir, &sTarget) < 0 &&
                VSIMkdirRecursive(osTargetDir, 0755) < 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot create directory %s", osTargetDir.c_str());
                return false;
            }
        }

        // Enumerate source files and directories
        struct ChunkToCopy
        {
            CPLString    osFilename{};
            GIntBig      nMTime = 0;
            CPLString    osETag{};
            vsi_l_offset nTotalSize = 0;
            vsi_l_offset nStartOffset = 0;
            vsi_l_offset nSize = 0;
        };
        std::vector<ChunkToCopy> aoChunksToCopy;
        std::set<CPLString> aoSetDirsToCreate;
        const char* pszChunkSize = CSLFetchNameValue(papszOptions, "CHUNK_SIZE");
        const int nRequestedThreads = atoi(CSLFetchNameValueDef(papszOptions, "NUM_THREADS", "1"));
        const bool bUploadToS3 = bUploadFromLocalToNetwork && STARTS_WITH(pszTarget, "/vsis3/");
        const bool bSimulateThreading = CPLTestBool(CPLGetConfigOption("VSIS3_SIMULATE_THREADING", "NO"));
        const int nMinSizeChunk = bUploadToS3 && !bSimulateThreading ? 5242880 : 1; // 5242880 defines by S3 API
        const int nMinThreads = bSimulateThreading ? 0 : 1;
        const size_t nMaxChunkSize =
            pszChunkSize && nRequestedThreads > nMinThreads &&
            (bDownloadFromNetworkToLocal || bUploadToS3) ?
                static_cast<size_t>(std::min(1024 * 1024 * 1024,
                                    std::max(nMinSizeChunk,
                                             atoi(pszChunkSize)))): 0;
        while( true )
        {
            const auto entry = VSIGetNextDirEntry(poSourceDir.get());
            if( !entry )
                break;
            if( VSI_ISDIR(entry->nMode) )
            {
                if( oSetTargetSubdirs.find(entry->pszName) == oSetTargetSubdirs.end() )
                {
                    const CPLString osTargetSubdir(
                        CPLFormFilename(osTargetDir, entry->pszName, nullptr));
                    aoSetDirsToCreate.insert(osTargetSubdir);
                }
            }
            else
            {
                // Split file in possibly multiple chunks
                const vsi_l_offset nChunksLarge = nMaxChunkSize == 0 ? 1 :
                        (entry->nSize + nMaxChunkSize - 1) / nMaxChunkSize;
                if( nChunksLarge > 1000 ) // must also be below knMAX_PART_NUMBER for upload
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Too small CHUNK_SIZE w.r.t file size");
                    return false;
                }
                ChunkToCopy chunk;
                chunk.osFilename = entry->pszName;
                chunk.nMTime = entry->nMTime;
                chunk.nTotalSize = entry->nSize;
                chunk.osETag = CSLFetchNameValueDef(entry->papszExtra, "ETag", "");
                const size_t nChunks = static_cast<size_t>(nChunksLarge);
                for( size_t iChunk = 0; iChunk < nChunks; iChunk++ )
                {
                    chunk.nStartOffset = iChunk * nMaxChunkSize;
                    chunk.nSize = nChunks == 1 ? entry->nSize:
                        std::min(entry->nSize - chunk.nStartOffset,
                                 static_cast<vsi_l_offset>(nMaxChunkSize));
                    aoChunksToCopy.push_back(chunk);
                    chunk.osETag.clear();
                }
            }
        }
        poSourceDir.reset();

        // Create missing target directories, sorted in lexicographic order
        // so that upper-level directories are listed before subdirectories.
        for( const auto& osTargetSubdir: aoSetDirsToCreate )
        {
            const bool ok =
                (bTargetIsThisFS ? MkdirInternal(osTargetSubdir, false):
                                   VSIMkdir(osTargetSubdir, 0755)) == 0;
            if( !ok )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                        "Cannot create directory %s",
                            osTargetSubdir.c_str());
                return false;
            }
        }

        // Collect source files to copy
        const size_t nChunkCount = aoChunksToCopy.size();
        uint64_t nTotalSize = 0;
        std::vector<size_t> anIndexToCopy; // points to aoChunksToCopy

        struct MultiPartDef
        {
            CPLString osUploadID{};
            int nCountValidETags = 0;
            int nExpectedCount = 0;
            std::vector<CPLString> aosEtags{};
        };
        std::map<CPLString, MultiPartDef> oMapMultiPartDefs;

        // Cleanup pending uploads in case of early exit
        struct CleanupPendingUploads
        {
            IVSIS3LikeFSHandler* m_poFS;
            std::map<CPLString, MultiPartDef>& m_oMapMultiPartDefs;
            int m_nMaxRetry;
            double m_dfRetryDelay;

            CleanupPendingUploads(
                IVSIS3LikeFSHandler* poFSIn,
                std::map<CPLString, MultiPartDef>& oMapMultiPartDefsIn,
                int nMaxRetryIn,
                double dfRetryDelayIn):
                m_poFS(poFSIn),
                m_oMapMultiPartDefs(oMapMultiPartDefsIn),
                m_nMaxRetry(nMaxRetryIn),
                m_dfRetryDelay(dfRetryDelayIn)
            {}

            ~CleanupPendingUploads()
            {
                for( const auto& kv: m_oMapMultiPartDefs )
                {
                    auto poS3HandleHelper = std::unique_ptr<VSIS3HandleHelper>(
                        VSIS3HandleHelper::BuildFromURI(
                            kv.first.c_str() + m_poFS->GetFSPrefix().size(),
                            m_poFS->GetFSPrefix().c_str(), false));
                    if( poS3HandleHelper )
                    {
                        m_poFS->UpdateHandleFromMap(poS3HandleHelper.get());
                        m_poFS->AbortMultipart(kv.first, kv.second.osUploadID,
                                               poS3HandleHelper.get(),
                                               m_nMaxRetry, m_dfRetryDelay);
                    }
                }
            }

            CleanupPendingUploads(const CleanupPendingUploads&) = delete;
            CleanupPendingUploads& operator=(const CleanupPendingUploads&) = delete;
        };
        const CleanupPendingUploads cleanupPendingUploads(this,
                                                          oMapMultiPartDefs,
                                                          nMaxRetry,
                                                          dfRetryDelay);

        for( size_t iChunk = 0; iChunk < nChunkCount; ++iChunk )
        {
            const auto& chunk = aoChunksToCopy[iChunk];
            if( chunk.nStartOffset != 0 )
                continue;
            const CPLString osSubSource(
                CPLFormFilename(osSourceWithoutSlash, chunk.osFilename, nullptr) );
            const CPLString osSubTarget(
                CPLFormFilename(osTargetDir, chunk.osFilename, nullptr) );
            bool bSkip = false;
            const auto oIterExistingTarget = oMapExistingTargetFiles.find(chunk.osFilename);
            if( oIterExistingTarget != oMapExistingTargetFiles.end() &&
                oIterExistingTarget->second.nSize == chunk.nTotalSize )
            {
                if( bDownloadFromNetworkToLocal )
                {
                    if( CanSkipDownloadFromNetworkToLocal(
                        osSubSource,
                        osSubTarget,
                        chunk.nMTime,
                        oIterExistingTarget->second.nMTime,
                        [&chunk](const char*)
                        {
                            return chunk.osETag;
                        }) )
                    {
                        bSkip = true;
                    }
                }
                else if( bUploadFromLocalToNetwork )
                {
                    VSILFILE* fpIn = nullptr;
                    if( CanSkipUploadFromLocalToNetwork(
                        fpIn,
                        osSubSource,
                        osSubTarget,
                        chunk.nMTime,
                        oIterExistingTarget->second.nMTime,
                        [&oIterExistingTarget](const char*)
                        {
                            return CPLString(CSLFetchNameValueDef(
                                oIterExistingTarget->second.papszExtra, "ETag", ""));
                        }) )
                    {
                        bSkip = true;
                    }
                    if( fpIn )
                        VSIFCloseL(fpIn);
                }
            }

            if( !bSkip )
            {
                anIndexToCopy.push_back(iChunk);
                nTotalSize += chunk.nTotalSize;
                if( chunk.nSize < chunk.nTotalSize )
                {
                    if( bDownloadFromNetworkToLocal )
                    {
                        // Suppress target file as we're going to open in wb+ mode
                        // for parallelized writing
                        VSIUnlink(osSubTarget);
                    }
                    else
                    {
                        auto poS3HandleHelper = std::unique_ptr<VSIS3HandleHelper>(
                            VSIS3HandleHelper::BuildFromURI(osSubTarget.c_str() + GetFSPrefix().size(),
                                                            GetFSPrefix().c_str(), false));
                        if( poS3HandleHelper == nullptr )
                            return false;
                        UpdateHandleFromMap(poS3HandleHelper.get());
                        const auto osUploadID = 
                            InitiateMultipartUpload(osSubTarget,
                                                    poS3HandleHelper.get(),
                                                    nMaxRetry,
                                                    dfRetryDelay);
                        if( osUploadID.empty() )
                        {
                            return false;
                        }
                        MultiPartDef def;
                        def.osUploadID = osUploadID;
                        def.nExpectedCount = static_cast<int>(
                            (chunk.nTotalSize + chunk.nSize - 1) / chunk.nSize);
                        oMapMultiPartDefs[osSubTarget] = def;
                    }

                    // Include all remaining chunks of the same file
                    while( iChunk + 1 < nChunkCount &&
                        aoChunksToCopy[iChunk + 1].nStartOffset > 0 )
                    {
                        ++iChunk;
                        anIndexToCopy.push_back(iChunk);
                    }
                }
            }
        }

        const int nThreads = std::min(std::max(1, nRequestedThreads),
                                        static_cast<int>(anIndexToCopy.size()));
        if( nThreads > nMinThreads )
        {
            struct JobQueue
            {
                IVSIS3LikeFSHandler* poFS;
                const std::vector<ChunkToCopy>& aoChunksToCopy;
                const std::vector<size_t>& anIndexToCopy;
                std::map<CPLString, MultiPartDef>& oMapMultiPartDefs;
                volatile int iCurIdx = 0;
                volatile bool ret = true;
                volatile bool stop = false;
                CPLString osSourceDir{};
                CPLString osTargetDir{};
                std::mutex sMutex{};
                uint64_t nTotalCopied = 0;
                bool bUploadToS3 = false;
                size_t nMaxChunkSize = 0;
                int nMaxRetry = 0;
                double dfRetryDelay = 0.0;

                JobQueue(IVSIS3LikeFSHandler* poFSIn,
                         const std::vector<ChunkToCopy>& aoChunksToCopyIn,
                         const std::vector<size_t>& anIndexToCopyIn,
                         std::map<CPLString, MultiPartDef>& oMapMultiPartDefsIn,
                         const CPLString& osSourceDirIn,
                         const CPLString& osTargetDirIn,
                         bool bUploadToS3In,
                         size_t nMaxChunkSizeIn,
                         int nMaxRetryIn,
                         double dfRetryDelayIn):
                    poFS(poFSIn),
                    aoChunksToCopy(aoChunksToCopyIn),
                    anIndexToCopy(anIndexToCopyIn),
                    oMapMultiPartDefs(oMapMultiPartDefsIn),
                    osSourceDir(osSourceDirIn),
                    osTargetDir(osTargetDirIn),
                    bUploadToS3(bUploadToS3In),
                    nMaxChunkSize(nMaxChunkSizeIn),
                    nMaxRetry(nMaxRetryIn),
                    dfRetryDelay(dfRetryDelayIn)
                {}

                JobQueue(const JobQueue&) = delete;
                JobQueue& operator=(const JobQueue&) = delete;
            };
            const auto threadFunc = [](void* pDataIn)
            {
                struct ProgressData
                {
                    uint64_t nFileSize;
                    double dfLastPct;
                    JobQueue* queue;
                };

                JobQueue* queue = static_cast<JobQueue*>(pDataIn);
                while( !queue->stop )
                {
                    const int idx = CPLAtomicInc(&(queue->iCurIdx)) - 1;
                    if( static_cast<size_t>(idx) >= queue->anIndexToCopy.size() )
                    {
                        queue->stop = true;
                        break;
                    }
                    const auto& chunk = queue->aoChunksToCopy[queue->anIndexToCopy[idx]];
                    const CPLString osSubSource(
                        CPLFormFilename(queue->osSourceDir, chunk.osFilename, nullptr) );
                    const CPLString osSubTarget(
                        CPLFormFilename(queue->osTargetDir, chunk.osFilename, nullptr) );

                    const auto progressFunc = [](double pct, const char*, void* pProgressDataIn)
                    {
                        ProgressData* pProgress = static_cast<ProgressData*>(pProgressDataIn);
                        const auto nInc = static_cast<uint64_t>(
                            (pct - pProgress->dfLastPct) * pProgress->nFileSize + 0.5);
                        pProgress->queue->sMutex.lock();
                        pProgress->queue->nTotalCopied += nInc;
                        pProgress->queue->sMutex.unlock();
                        pProgress->dfLastPct = pct;
                        return TRUE;
                    };
                    ProgressData progressData;
                    progressData.nFileSize = chunk.nSize;
                    progressData.dfLastPct = 0;
                    progressData.queue = queue;
                    if( chunk.nSize < chunk.nTotalSize )
                    {
                        const size_t nSizeToRead = static_cast<size_t>(chunk.nSize);
                        bool bSuccess = false;
                        if( queue->bUploadToS3 )
                        {
                            const auto iter = queue->oMapMultiPartDefs.find(osSubTarget);
                            CPLAssert(iter != queue->oMapMultiPartDefs.end());

                            VSILFILE* fpIn = VSIFOpenL(osSubSource, "rb");
                            void* pBuffer = VSI_MALLOC_VERBOSE(nSizeToRead);
                            auto poS3HandleHelper = std::unique_ptr<VSIS3HandleHelper>(
                                VSIS3HandleHelper::BuildFromURI(
                                    osSubTarget.c_str() + queue->poFS->GetFSPrefix().size(),
                                    queue->poFS->GetFSPrefix().c_str(), false));
                            if( fpIn && pBuffer && poS3HandleHelper &&
                                VSIFSeekL(fpIn, chunk.nStartOffset, SEEK_SET) == 0 &&
                                VSIFReadL(pBuffer, 1, nSizeToRead, fpIn) == nSizeToRead )
                            {
                                queue->poFS->UpdateHandleFromMap(poS3HandleHelper.get());
                                const int nPartNumber = 1 + static_cast<int>(
                                    chunk.nStartOffset / queue->nMaxChunkSize);
                                const CPLString osEtag = queue->poFS->UploadPart(
                                    osSubTarget, nPartNumber,
                                    iter->second.osUploadID,
                                    pBuffer, nSizeToRead,
                                    poS3HandleHelper.get(),
                                    queue->nMaxRetry,
                                    queue->dfRetryDelay);
                                if( !osEtag.empty() )
                                {
                                    iter->second.nCountValidETags ++;
                                    iter->second.aosEtags.resize(
                                        std::max(nPartNumber,
                                                 static_cast<int>(iter->second.aosEtags.size())));
                                    iter->second.aosEtags[nPartNumber-1] = osEtag;
                                    bSuccess = true;
                                }
                            }
                            if( fpIn )
                                VSIFCloseL(fpIn);
                            VSIFree(pBuffer);
                        }
                        else
                        {
                            bSuccess = CopyChunk(osSubSource, osSubTarget,
                                                 chunk.nStartOffset,
                                                 nSizeToRead);
                        }
                        if( bSuccess )
                        {
                            progressFunc(1.0, "", &progressData);
                        }
                        else
                        {
                            queue->ret = false;
                            queue->stop = true;
                        }
                    }
                    else
                    {
                        CPLAssert( chunk.nStartOffset == 0 );
                        if( !queue->poFS->CopyFile(nullptr, chunk.nTotalSize,
                                    osSubSource, osSubTarget,
                                    progressFunc, &progressData) )
                        {
                            queue->ret = false;
                            queue->stop = true;
                        }
                    }
                }
            };

            JobQueue sJobQueue(this, aoChunksToCopy, anIndexToCopy,
                               oMapMultiPartDefs,
                               osSourceWithoutSlash, osTargetDir,
                               bUploadToS3, nMaxChunkSize,
                               nMaxRetry, dfRetryDelay);

            if( CPLTestBool(CPLGetConfigOption("VSIS3_SYNC_MULTITHREADING", "YES")) )
            {
                std::vector<CPLJoinableThread*> ahThreads;
                for( int i = 0; i < nThreads; i++ )
                {
                    auto hThread = CPLCreateJoinableThread(threadFunc, &sJobQueue);
                    if( !hThread )
                    {
                        sJobQueue.ret = false;
                        sJobQueue.stop = true;
                        break;
                    }
                    ahThreads.push_back(hThread);
                }
                if( pProgressFunc )
                {
                    while( !sJobQueue.stop )
                    {
                        CPLSleep(0.1);
                        sJobQueue.sMutex.lock();
                        const auto nTotalCopied = sJobQueue.nTotalCopied;
                        sJobQueue.sMutex.unlock();
                        // coverity[divide_by_zero]
                        if( !pProgressFunc(double(nTotalCopied) / nTotalSize,
                                        "", pProgressData) )
                        {
                            sJobQueue.ret = false;
                            sJobQueue.stop = true;
                        }
                    }
                    if( sJobQueue.ret )
                    {
                        pProgressFunc(1.0, "", pProgressData);
                    }
                }
                for( auto hThread: ahThreads )
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
            if( sJobQueue.ret )
            {
                std::set<CPLString> oSetKeysToRemove;
                for( const auto& kv: oMapMultiPartDefs )
                {
                    auto poS3HandleHelper = std::unique_ptr<VSIS3HandleHelper>(
                        VSIS3HandleHelper::BuildFromURI(
                            kv.first.c_str() + GetFSPrefix().size(),
                            GetFSPrefix().c_str(), false));
                    sJobQueue.ret = false;
                    if( poS3HandleHelper )
                    {
                        CPLAssert(kv.second.nCountValidETags == kv.second.nExpectedCount);
                        UpdateHandleFromMap(poS3HandleHelper.get());
                        if( CompleteMultipart(kv.first, kv.second.osUploadID,
                                              kv.second.aosEtags,
                                              poS3HandleHelper.get(),
                                              nMaxRetry, dfRetryDelay) )
                        {
                            sJobQueue.ret = true;
                            oSetKeysToRemove.insert(kv.first);

                            InvalidateCachedData(
                                poS3HandleHelper->GetURL().c_str() );
                            InvalidateDirContent( CPLGetDirname(kv.first) );
                        }
                    }
                }
                for( const auto& key: oSetKeysToRemove )
                {
                    oMapMultiPartDefs.erase(key);
                }
            }

            return sJobQueue.ret;
        }

        // Proceed to file copy
        bool ret = true;
        uint64_t nAccSize = 0;
        for( const size_t iChunk: anIndexToCopy )
        {
            const auto& chunk = aoChunksToCopy[iChunk];
            CPLAssert(chunk.nStartOffset == 0);
            const CPLString osSubSource(
                CPLFormFilename(osSourceWithoutSlash, chunk.osFilename, nullptr) );
            const CPLString osSubTarget(
                CPLFormFilename(osTargetDir, chunk.osFilename, nullptr) );
            // coverity[divide_by_zero]
            void* pScaledProgress = GDALCreateScaledProgress(
                double(nAccSize) / nTotalSize, double(nAccSize + chunk.nSize) / nTotalSize,
                pProgressFunc, pProgressData);
            ret = CopyFile(nullptr, chunk.nSize,
                           osSubSource, osSubTarget,
                           GDALScaledProgress, pScaledProgress);
            GDALDestroyScaledProgress(pScaledProgress);
            if( !ret )
            {
                break;
            }
            nAccSize += chunk.nSize;
        }

        return ret;
    }

    CPLString osMsg;
    osMsg.Printf("Copying of %s", osSourceWithoutSlash.c_str());

    VSIStatBufL sTarget;
    CPLString osTarget(pszTarget);
    bool bTargetIsFile = false;
    sTarget.st_size = 0;
    if( VSIStatL(osTarget, &sTarget) == 0 )
    {
        bTargetIsFile = true;
        if( VSI_ISDIR(sTarget.st_mode) )
        {
            osTarget = CPLFormFilename(osTarget, CPLGetFilename(pszSource), nullptr);
            bTargetIsFile = VSIStatL(osTarget, &sTarget) == 0 && 
                            !CPL_TO_BOOL(VSI_ISDIR(sTarget.st_mode));
        }
    }

    // Download from network to local file system ?
    if( bTargetIsFile && bDownloadFromNetworkToLocal && sSource.st_size == sTarget.st_size )
    {
        if( CanSkipDownloadFromNetworkToLocal(
            osSourceWithoutSlash,
            osTarget,
            sSource.st_mtime,
            sTarget.st_mtime,
            [this](const char* pszFilename)
            {
                FileProp cachedFileProp;
                if( GetCachedFileProp(GetURLFromFilename(pszFilename),
                                      cachedFileProp) )
                {
                    return cachedFileProp.ETag;
                }
                return CPLString();
            }) )
        {
            if( pProgressFunc )
            {
                pProgressFunc(1.0, osMsg.c_str(), pProgressData);
            }
            return true;
        }
    }

    VSILFILE* fpIn = nullptr;

    // Upload from local file system to network ?
    if( bUploadFromLocalToNetwork && sSource.st_size == sTarget.st_size )
    {
        if( CanSkipUploadFromLocalToNetwork(
            fpIn,
            osSourceWithoutSlash,
            osTarget,
            sSource.st_mtime,
            sTarget.st_mtime,
            [this](const char* pszFilename)
            {
                FileProp cachedFileProp;
                if( GetCachedFileProp(GetURLFromFilename(pszFilename),
                                      cachedFileProp) )
                {
                    return cachedFileProp.ETag;
                }
                return CPLString();
            }) )
        {
            if( pProgressFunc )
            {
                pProgressFunc(1.0, osMsg.c_str(), pProgressData);
            }
            return true;
        }
    }

    return CopyFile(fpIn, sSource.st_size,
                    osSourceWithoutSlash,
                    osTarget,
                    pProgressFunc,
                    pProgressData);
}

/************************************************************************/
/*                         UpdateMapFromHandle()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateMapFromHandle( IVSIS3LikeHandleHelper * poHandleHelper )
{
    VSIS3UpdateParams::UpdateMapFromHandle(poHandleHelper);
}

/************************************************************************/
/*                         UpdateHandleFromMap()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateHandleFromMap( IVSIS3LikeHandleHelper * poHandleHelper )
{
    VSIS3UpdateParams::UpdateHandleFromMap(poHandleHelper);
}

/************************************************************************/
/*                             VSIS3Handle()                            */
/************************************************************************/

VSIS3Handle::VSIS3Handle( VSIS3FSHandler* poFSIn,
                          const char* pszFilename,
                          VSIS3HandleHelper* poS3HandleHelper ) :
        IVSIS3LikeHandle(poFSIn, pszFilename, poS3HandleHelper->GetURL()),
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

struct curl_slist* VSIS3Handle::GetCurlHeaders( const CPLString& osVerb,
                                const struct curl_slist* psExistingHeaders )
{
    return m_poS3HandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIS3Handle::CanRestartOnError(const char* pszErrorMsg,
                                    const char* pszHeaders, bool bSetError)
{
    bool bUpdateMap = false;
    if( m_poS3HandleHelper->CanRestartOnError(pszErrorMsg, pszHeaders,
                                              bSetError, &bUpdateMap) )
    {
        if( bUpdateMap )
        {
            static_cast<VSIS3FSHandler *>(poFS)->
                UpdateMapFromHandle(m_poS3HandleHelper);
        }

        SetURL(m_poS3HandleHelper->GetURL());
        return true;
    }
    return false;
}

} /* end of namespace cpl */


#endif // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                      VSIInstallS3FileHandler()                       */
/************************************************************************/

/**
 * \brief Install /vsis3/ Amazon S3 file system handler (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsis3">/vsis3/ documentation</a>
 *
 * @since GDAL 2.1
 */
void VSIInstallS3FileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsis3/", new cpl::VSIS3FSHandler );
}

#endif /* HAVE_CURL */
