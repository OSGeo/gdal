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

#include "cpl_port.h"
#include "cpl_http.h"
#include "cpl_md5.h"
#include "cpl_minixml.h"
#include "cpl_time.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_vsil_curl_class.h"

#include <algorithm>
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

                    if( nMaxFiles != 1 )
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

                        if( nMaxFiles != 1 )
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

                    if( nMaxFiles != 1 )
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
    WriteFuncStruct sWriteFuncData;
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
        CPLString osBaseURL(poS3HandleHelper->GetURL());

        CURLM* hCurlMultiHandle = poFS->GetCurlMultiHandleFor(osBaseURL);
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
        // Disable automatic redirection
        curl_easy_setopt(hCurlHandle, CURLOPT_FOLLOWLOCATION, 0 );

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, nullptr);

        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        headers = VSICurlMergeHeaders(headers,
                               poS3HandleHelper->GetCurlHeaders("GET", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        MultiPerform(hCurlMultiHandle, hCurlHandle);

        VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

        curl_slist_free_all(headers);

        if( sWriteFuncData.pBuffer == nullptr)
        {
            curl_easy_cleanup(hCurlHandle);
            CPLFree(sWriteFuncHeaderData.pBuffer);
            return false;
        }

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            bool bUpdateMap = true;
            if( sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer,
                                                    sWriteFuncHeaderData.pBuffer,
                                                    false, &bUpdateMap) )
            {
                if( bUpdateMap )
                {
                    poS3FS->UpdateMapFromHandle(poS3HandleHelper);
                }
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
            }
            else
            {
                CPLDebug(poS3FS->GetDebugKey(), "%s",
                         sWriteFuncData.pBuffer
                         ? sWriteFuncData.pBuffer : "(null)");
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
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
                                          sWriteFuncData.pBuffer,
                                          bIgnoreGlacier,
                                          bIsTruncated );

            CPLFree(sWriteFuncData.pBuffer);
            CPLFree(sWriteFuncHeaderData.pBuffer);

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

    std::map< CPLString, VSIS3UpdateParams > oMapBucketsToS3Params{};

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

bool VSIS3WriteHandle::InitiateMultipartUpload()
{
    bool bSuccess = true;
    bool bRetry;
    double dfRetryDelay = m_dfRetryDelay;
    int nRetryCount = 0;
    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        m_poS3HandleHelper->AddQueryParameter("uploads", "");
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                              m_poS3HandleHelper->GetURL().c_str(),
                              nullptr));
        headers = VSICurlMergeHeaders(headers,
                        m_poS3HandleHelper->GetCurlHeaders("POST", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        m_poS3HandleHelper->ResetQueryParameters();

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);


        MultiPerform(m_poFS->GetCurlMultiHandleFor(m_poS3HandleHelper->GetURL()),
                     hCurlHandle);

        VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 || sWriteFuncData.pBuffer == nullptr )
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                sWriteFuncHeaderData.pBuffer);
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
            else if( sWriteFuncData.pBuffer != nullptr &&
                m_poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer,
                                                      sWriteFuncHeaderData.pBuffer,
                                                      false) )
            {
                m_poFS->UpdateMapFromHandle(m_poS3HandleHelper);
                bRetry = true;
            }
            else
            {
                CPLDebug(m_poFS->GetDebugKey(), "%s",
                         sWriteFuncData.pBuffer
                         ? sWriteFuncData.pBuffer
                         : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "InitiateMultipartUpload of %s failed",
                         m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            m_poFS->InvalidateCachedData(
                m_poS3HandleHelper->GetURL().c_str());
            m_poFS->InvalidateDirContent( CPLGetDirname(m_osFilename) );

            CPLXMLNode* psNode =
                CPLParseXMLString( sWriteFuncData.pBuffer );
            if( psNode )
            {
                m_osUploadID =
                    CPLGetXMLValue(
                        psNode, "=InitiateMultipartUploadResult.UploadId", "");
                CPLDebug(m_poFS->GetDebugKey(),
                         "UploadId: %s", m_osUploadID.c_str());
                CPLDestroyXMLNode(psNode);
            }
            if( m_osUploadID.empty() )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "InitiateMultipartUpload of %s failed: cannot get UploadId",
                    m_osFilename.c_str());
                bSuccess = false;
            }
        }

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );
    return bSuccess;
}

/************************************************************************/
/*                         ReadCallBackBuffer()                         */
/************************************************************************/

size_t VSIS3WriteHandle::ReadCallBackBuffer( char *buffer, size_t size,
                                             size_t nitems, void *instream )
{
    VSIS3WriteHandle* poThis = static_cast<VSIS3WriteHandle *>(instream);
    const int nSizeMax = static_cast<int>(size * nitems);
    const int nSizeToWrite =
        std::min(nSizeMax,
                 poThis->m_nBufferOff - poThis->m_nBufferOffReadCallback);
    memcpy(buffer, poThis->m_pabyBuffer + poThis->m_nBufferOffReadCallback,
           nSizeToWrite);
    poThis->m_nBufferOffReadCallback += nSizeToWrite;
    return nSizeToWrite;
}

/************************************************************************/
/*                           UploadPart()                               */
/************************************************************************/

bool VSIS3WriteHandle::UploadPart()
{
    ++m_nPartNumber;
    if( m_nPartNumber > 10000 )
    {
        m_bError = true;
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "10000 parts have been uploaded for %s failed. "
            "This is the maximum. "
            "Increase VSIS3_CHUNK_SIZE to a higher value (e.g. 500 for 500 MB)",
            m_osFilename.c_str());
        return false;
    }

    bool bRetry;
    double dfRetryDelay = m_dfRetryDelay;
    int nRetryCount = 0;
    bool bSuccess = true;
    do
    {
        bRetry = false;

        m_nBufferOffReadCallback = 0;
        CURL* hCurlHandle = curl_easy_init();
        m_poS3HandleHelper->AddQueryParameter("partNumber",
                                            CPLSPrintf("%d", m_nPartNumber));
        m_poS3HandleHelper->AddQueryParameter("uploadId", m_osUploadID);
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
        curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                            m_poS3HandleHelper->GetURL().c_str(),
                            nullptr));
        headers = VSICurlMergeHeaders(headers,
                        m_poS3HandleHelper->GetCurlHeaders("PUT", headers,
                                                            m_pabyBuffer,
                                                            m_nBufferOff));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        m_poS3HandleHelper->ResetQueryParameters();

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                        VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                        VSICurlHandleWriteFunc);


        MultiPerform(m_poFS->GetCurlMultiHandleFor(m_poS3HandleHelper->GetURL()),
                        hCurlHandle);

        VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 || sWriteFuncHeaderData.pBuffer == nullptr )
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                sWriteFuncHeaderData.pBuffer);
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
                CPLDebug(m_poFS->GetDebugKey(), "%s",
                        sWriteFuncData.pBuffer ? sWriteFuncData.pBuffer : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "UploadPart(%d) of %s failed",
                            m_nPartNumber, m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            CPLString osHeader(sWriteFuncHeaderData.pBuffer);
            size_t nPos = osHeader.ifind("ETag: ");
            if( nPos != std::string::npos )
            {
                CPLString osEtag(osHeader.substr(nPos + strlen("ETag: ")));
                const size_t nPosEOL = osEtag.find("\r");
                if( nPosEOL != std::string::npos )
                    osEtag.resize(nPosEOL);
                CPLDebug(m_poFS->GetDebugKey(), "Etag for part %d is %s",
                        m_nPartNumber, osEtag.c_str());
                m_aosEtags.push_back(osEtag);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "UploadPart(%d) of %s (uploadId = %s) failed",
                        m_nPartNumber, m_osFilename.c_str(), m_osUploadID.c_str());
                bSuccess = false;
            }
        }

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );

    return bSuccess;
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
    size_t nChunckedBufferRemainingSize =
                poThis->m_nChunkedBufferSize - poThis->m_nChunkedBufferOff;
    if( nChunckedBufferRemainingSize < nSizeToWrite )
        nSizeToWrite = nChunckedBufferRemainingSize;
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
                                m_sWriteFuncHeaderData.pBuffer) : 0.0;

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
                    m_sWriteFuncHeaderData.pBuffer) : 0.0;
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
                if( !InitiateMultipartUpload() )
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
        m_nBufferOffReadCallback = 0;
        CURL* hCurlHandle = curl_easy_init();
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
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
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);


        MultiPerform(m_poFS->GetCurlMultiHandleFor(m_poS3HandleHelper->GetURL()),
                     hCurlHandle);

        VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 && response_code != 201 )
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                sWriteFuncHeaderData.pBuffer);
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
            else if( sWriteFuncData.pBuffer != nullptr &&
                m_poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer,
                                                      sWriteFuncHeaderData.pBuffer,
                                                      false) )
            {
                m_poFS->UpdateMapFromHandle(m_poS3HandleHelper);
                bRetry = true;
            }
            else
            {
                CPLDebug("S3", "%s",
                         sWriteFuncData.pBuffer
                         ? sWriteFuncData.pBuffer
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

        if( sWriteFuncHeaderData.pBuffer != nullptr )
        {
            const char* pzETag = strstr(
                sWriteFuncHeaderData.pBuffer, "ETag: \"");
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

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );
    return bSuccess;
}

/************************************************************************/
/*                            ReadCallBackXML()                         */
/************************************************************************/

size_t VSIS3WriteHandle::ReadCallBackXML( char *buffer, size_t size,
                                          size_t nitems, void *instream )
{
    VSIS3WriteHandle* poThis = static_cast<VSIS3WriteHandle *>(instream);
    const int nSizeMax = static_cast<int>(size * nitems);
    const int nSizeToWrite =
        std::min(nSizeMax,
                 static_cast<int>(poThis->m_osXML.size()) -
                 poThis->m_nOffsetInXML);
    memcpy(buffer, poThis->m_osXML.c_str() + poThis->m_nOffsetInXML,
           nSizeToWrite);
    poThis->m_nOffsetInXML += nSizeToWrite;
    return nSizeToWrite;
}

/************************************************************************/
/*                        CompleteMultipart()                           */
/************************************************************************/

bool VSIS3WriteHandle::CompleteMultipart()
{
    bool bSuccess = true;

    m_osXML = "<CompleteMultipartUpload>\n";
    for( size_t i = 0; i < m_aosEtags.size(); i++ )
    {
        m_osXML += "<Part>\n";
        m_osXML += CPLSPrintf("<PartNumber>%d</PartNumber>",
                              static_cast<int>(i+1));
        m_osXML += "<ETag>" + m_aosEtags[i] + "</ETag>";
        m_osXML += "</Part>\n";
    }
    m_osXML += "</CompleteMultipartUpload>\n";

    m_nOffsetInXML = 0;

    double dfRetryDelay = m_dfRetryDelay;
    int nRetryCount = 0;
    bool bRetry;
    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        m_poS3HandleHelper->AddQueryParameter("uploadId", m_osUploadID);
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackXML);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
        curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                        static_cast<int>(m_osXML.size()));
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                            m_poS3HandleHelper->GetURL().c_str(),
                            nullptr));
        headers = VSICurlMergeHeaders(headers,
                        m_poS3HandleHelper->GetCurlHeaders("POST", headers,
                                                            m_osXML.c_str(),
                                                            m_osXML.size()));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        m_poS3HandleHelper->ResetQueryParameters();

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                        VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);

        MultiPerform(m_poFS->GetCurlMultiHandleFor(m_poS3HandleHelper->GetURL()),
                    hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                sWriteFuncHeaderData.pBuffer);
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
                CPLDebug("S3", "%s",
                    sWriteFuncData.pBuffer ? sWriteFuncData.pBuffer : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                        "CompleteMultipart of %s (uploadId=%s) failed",
                        m_osFilename.c_str(), m_osUploadID.c_str());
                bSuccess = false;
            }
        }
        else
        {
            InvalidateParentDirectory();
        }

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bRetry );

    return bSuccess;
}

/************************************************************************/
/*                          AbortMultipart()                            */
/************************************************************************/

bool VSIS3WriteHandle::AbortMultipart()
{
    bool bSuccess = true;

    double dfRetryDelay = m_dfRetryDelay;
    int nRetryCount = 0;
    bool bRetry;
    do
    {
        bRetry = false;
        CURL* hCurlHandle = curl_easy_init();
        m_poS3HandleHelper->AddQueryParameter("uploadId", m_osUploadID);
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle,
                            m_poS3HandleHelper->GetURL().c_str(),
                            nullptr));
        headers = VSICurlMergeHeaders(headers,
                        m_poS3HandleHelper->GetCurlHeaders("DELETE", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        m_poS3HandleHelper->ResetQueryParameters();

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                        VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);

        MultiPerform(m_poFS->GetCurlMultiHandleFor(m_poS3HandleHelper->GetURL()),
                    hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 204 )
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                sWriteFuncHeaderData.pBuffer);
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
                CPLDebug("S3", "%s",
                        sWriteFuncData.pBuffer ? sWriteFuncData.pBuffer : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                        "AbortMultipart of %s (uploadId=%s) failed",
                        m_osFilename.c_str(), m_osUploadID.c_str());
                bSuccess = false;
            }
        }

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

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
                if( !AbortMultipart() )
                    nRet = -1;
            }
            else if( m_nBufferOff > 0 && !UploadPart() )
                nRet = -1;
            else if( !CompleteMultipart() )
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

    oMapBucketsToS3Params.clear();

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
        "default='50' min='1' max='1000'/>" +
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
/*                               Mkdir()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::Mkdir( const char * pszDirname, long /* nMode */ )
{
    if( !STARTS_WITH_CI(pszDirname, GetFSPrefix()) )
        return -1;

    CPLString osDirname(pszDirname);
    if( !osDirname.empty() && osDirname.back() != '/' )
        osDirname += "/";

    VSIStatBufL sStat;
    if( VSIStatL(osDirname, &sStat) == 0 &&
        sStat.st_mode == S_IFDIR )
    {
        CPLDebug(GetDebugKey(), "Directory %s already exists", osDirname.c_str());
        errno = EEXIST;
        return -1;
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
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);

        MultiPerform(GetCurlMultiHandleFor(poS3HandleHelper->GetURL()),
                     hCurlHandle);

        VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        // S3 and GS respond with 204. Azure with 202
        if( response_code != 204 && response_code != 202)
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                sWriteFuncHeaderData.pBuffer);
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
            else if( sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer,
                                                    sWriteFuncHeaderData.pBuffer,
                                                    false) )
            {
                UpdateMapFromHandle(poS3HandleHelper);
                bRetry = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         sWriteFuncData.pBuffer
                         ? sWriteFuncData.pBuffer
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

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

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

    // If the source is likely to be a directory, try to issue a ReadDir()
    // if we haven't stat'ed it yet
    if( STARTS_WITH(pszSource, GetFSPrefix()) && osSource.back() == '/' )
    {
        FileProp cachedFileProp;
        if( !GetCachedFileProp(GetURLFromFilename(osSourceWithoutSlash),
                                cachedFileProp) )
        {
            CSLDestroy( VSIReadDir(osSourceWithoutSlash) );
        }
    }

    VSIStatBufL sSource;
    if( VSIStatL(osSourceWithoutSlash, &sSource) < 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "%s does not exist", pszSource);
        return false;
    }

    if( VSI_ISDIR(sSource.st_mode) )
    {
        CPLString osTargetDir(pszTarget);
        if( osSource.back() != '/' )
        {
            osTargetDir = CPLFormFilename(osTargetDir,
                                          CPLGetFilename(pszSource), nullptr);
        }

        // Force caching of directory content to avoid less individual
        // requests
        char** papszFileList = VSIReadDir(osTargetDir);
        const bool bTargetDirKnownExist = papszFileList != nullptr;
        CSLDestroy(papszFileList);

        VSIStatBufL sTarget;
        bool ret = true;
        if( !bTargetDirKnownExist && VSIStatL(osTargetDir, &sTarget) < 0 )
        {
            if( VSIMkdirRecursive(osTargetDir, 0755) < 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot create directory %s", osTargetDir.c_str());
                return false;
            }
        }

        if( !CPLFetchBool(papszOptions, "STOP_ON_DIR", false) )
        {
            CPLStringList aosChildOptions(CSLDuplicate(papszOptions));
            if( !CPLFetchBool(papszOptions, "RECURSIVE", true) )
            {
                aosChildOptions.SetNameValue("RECURSIVE", nullptr);
                aosChildOptions.AddString("STOP_ON_DIR=TRUE");
            }

            char** papszSrcFiles = VSIReadDir(osSourceWithoutSlash);
            int nFileCount = 0;
            for( auto iter = papszSrcFiles ; iter && *iter; ++iter )
            {
                if( strcmp(*iter, ".") != 0 && strcmp(*iter, "..") != 0 )
                {
                    nFileCount ++;
                }
            }
            int iFile = 0;
            for( auto iter = papszSrcFiles ; iter && *iter; ++iter, ++iFile )
            {
                if( strcmp(*iter, ".") == 0 || strcmp(*iter, "..") == 0 )
                {
                    continue;
                }
                CPLString osSubSource(
                    CPLFormFilename(osSourceWithoutSlash, *iter, nullptr) );
                CPLString osSubTarget(
                    CPLFormFilename(osTargetDir, *iter, nullptr) );
                void* pScaledProgress = GDALCreateScaledProgress(
                    double(iFile) / nFileCount, double(iFile + 1) / nFileCount,
                    pProgressFunc, pProgressData);
                ret = Sync( (osSubSource + '/').c_str(), osSubTarget,
                            aosChildOptions.List(),
                            GDALScaledProgress, pScaledProgress,
                            nullptr );
                GDALDestroyScaledProgress(pScaledProgress);
                if( !ret )
                {
                    break;
                }
            }
            CSLDestroy(papszSrcFiles);
        }
        return ret;
    }

    CPLString osMsg;
    osMsg.Printf("Copying of %s", osSourceWithoutSlash.c_str());

    VSIStatBufL sTarget;
    CPLString osTarget(pszTarget);
    bool bTargetIsFile = false;
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

    const bool bETagStrategy = EQUAL(CSLFetchNameValueDef(
        papszOptions, "SYNC_STRATEGY", "TIMESTAMP"), "ETAG");

    // Download from network to local file system ?
    if( (!STARTS_WITH(pszTarget, "/vsi") || STARTS_WITH(pszTarget, "/vsimem/")) &&
        STARTS_WITH(pszSource, GetFSPrefix()) )
    {
        FileProp cachedFileProp;
        if( bTargetIsFile && sSource.st_size == sTarget.st_size &&
            GetCachedFileProp(GetURLFromFilename(osSourceWithoutSlash),
                              cachedFileProp) )
        {
            if( bETagStrategy )
            {
                VSILFILE* fpOutAsIn = VSIFOpenExL(osTarget, "rb", TRUE);
                if( fpOutAsIn )
                {
                    CPLString md5 = ComputeMD5OfLocalFile(fpOutAsIn);
                    VSIFCloseL(fpOutAsIn);
                    if( cachedFileProp.ETag == md5 )
                    {
                        CPLDebug(GetDebugKey(),
                                 "%s has already same content as %s",
                                osTarget.c_str(), osSourceWithoutSlash.c_str());
                        if( pProgressFunc )
                        {
                            pProgressFunc(1.0, osMsg.c_str(), pProgressData);
                        }
                        return true;
                    }
                }
            }
            else
            {
                if( sTarget.st_mtime <= sSource.st_mtime )
                {
                    // Our local copy is older than the source, so
                    // presumably the source was uploaded from it. Nothing to do
                    CPLDebug(GetDebugKey(), "%s is older than %s. "
                             "Do not replace %s assuming it was used to "
                             "upload %s",
                             osTarget.c_str(), osSourceWithoutSlash.c_str(),
                             osTarget.c_str(), osSourceWithoutSlash.c_str());
                    if( pProgressFunc )
                    {
                        pProgressFunc(1.0, osMsg.c_str(), pProgressData);
                    }
                    return true;
                }
            }
        }
    }

    VSILFILE* fpIn = nullptr;

    // Upload from local file system to network ?
    if( (!STARTS_WITH(pszSource, "/vsi") || STARTS_WITH(pszSource, "/vsimem/")) &&
        STARTS_WITH(pszTarget, GetFSPrefix()) )
    {
        FileProp cachedFileProp;
        if( sSource.st_size == sTarget.st_size &&
            GetCachedFileProp(GetURLFromFilename(osTarget), cachedFileProp) )
        {
            if( bETagStrategy )
            {
                fpIn = VSIFOpenExL(osSourceWithoutSlash, "rb", TRUE);
                if( fpIn && cachedFileProp.ETag == ComputeMD5OfLocalFile(fpIn) )
                {
                    CPLDebug(GetDebugKey(), "%s has already same content as %s",
                            osTarget.c_str(), osSourceWithoutSlash.c_str());
                    VSIFCloseL(fpIn);
                    if( pProgressFunc )
                    {
                        pProgressFunc(1.0, osMsg.c_str(), pProgressData);
                    }
                    return true;
                }
            }
            else
            {
                if( sTarget.st_mtime >= sSource.st_mtime )
                {
                    // The remote copy is more recent than the source, so
                    // presumably it was uploaded from the source. Nothing to do
                    CPLDebug(GetDebugKey(), "%s is more recent than %s. "
                             "Do not replace %s assuming it was uploaded from "
                             "%s",
                             osTarget.c_str(), osSourceWithoutSlash.c_str(),
                             osTarget.c_str(), osSourceWithoutSlash.c_str());
                    if( pProgressFunc )
                    {
                        pProgressFunc(1.0, osMsg.c_str(), pProgressData);
                    }
                    return true;
                }
            }
        }
    }

    if( fpIn == nullptr )
    {
        fpIn = VSIFOpenExL(osSourceWithoutSlash, "rb", TRUE);
    }
    if( fpIn == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                 osSourceWithoutSlash.c_str());
        return false;
    }

    VSILFILE* fpOut = VSIFOpenExL(osTarget.c_str(), "wb", TRUE);
    if( fpOut == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", osTarget.c_str());
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
                     osSourceWithoutSlash.c_str(), osTarget.c_str());
            ret = false;
            break;
        }
        nOffset += nRead;
        if( pProgressFunc && !pProgressFunc(
                double(nOffset) / sSource.st_size, osMsg.c_str(),
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
/*                         UpdateMapFromHandle()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateMapFromHandle( IVSIS3LikeHandleHelper * poHandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    VSIS3HandleHelper * poS3HandleHelper =
        dynamic_cast<VSIS3HandleHelper *>(poHandleHelper);
    CPLAssert( poS3HandleHelper );
    if( !poS3HandleHelper )
        return;
    oMapBucketsToS3Params[ poS3HandleHelper->GetBucket() ] =
        VSIS3UpdateParams ( poS3HandleHelper );
}

/************************************************************************/
/*                         UpdateHandleFromMap()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateHandleFromMap( IVSIS3LikeHandleHelper * poHandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    VSIS3HandleHelper * poS3HandleHelper =
        dynamic_cast<VSIS3HandleHelper *>(poHandleHelper);
    CPLAssert( poS3HandleHelper );
    if( !poS3HandleHelper )
        return;
    std::map< CPLString, VSIS3UpdateParams>::iterator oIter =
        oMapBucketsToS3Params.find(poS3HandleHelper->GetBucket());
    if( oIter != oMapBucketsToS3Params.end() )
    {
        oIter->second.UpdateHandlerHelper(poS3HandleHelper);
    }
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
