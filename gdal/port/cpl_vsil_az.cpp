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

#include <algorithm>
#include <set>
#include <map>
#include <memory>

#include "cpl_azure.h"

CPL_CVSID("$Id$")

#ifndef HAVE_CURL

void VSIInstallAzureFileHandler( void )
{
    // Not supported
}

#else

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

#define ENABLE_DEBUG 0

namespace cpl {

const char GDAL_MARKER_FOR_DIR[] = ".gdal_marker_for_dir";

/************************************************************************/
/*                         AnalyseAzureFileList()                       */
/************************************************************************/

void VSICurlFilesystemHandler::AnalyseAzureFileList(
    const CPLString& osBaseURL,
    bool bCacheResults,
    const char* pszXML,
    CPLStringList& osFileList,
    int nMaxFiles,
    bool& bIsTruncated,
    CPLString& osNextMarker )
{
#if DEBUG_VERBOSE
    CPLDebug("AZURE", "%s", pszXML);
#endif
    osNextMarker = "";
    bIsTruncated = false;
    CPLXMLNode* psTree = CPLParseXMLString(pszXML);
    if( psTree == nullptr )
        return;
    CPLXMLNode* psEnumerationResults = CPLGetXMLNode(psTree, "=EnumerationResults");

    std::vector< std::pair<CPLString, CachedFileProp> > aoProps;
    // Count the number of occurrences of a path. Can be 1 or 2. 2 in the case
    // that both a filename and directory exist
    std::map<CPLString, int> aoNameCount;

    if( psEnumerationResults )
    {
        bool bNonEmpty = false;
        CPLString osPrefix = CPLGetXMLValue(psEnumerationResults, "Prefix", "");
        CPLXMLNode* psBlobs = CPLGetXMLNode(psEnumerationResults, "Blobs");
        if( psBlobs == nullptr )
        {
            psBlobs = CPLGetXMLNode(psEnumerationResults, "Containers");
            if( psBlobs != nullptr )
                bNonEmpty = true;
        }
        CPLXMLNode* psIter = psBlobs ? psBlobs->psChild : nullptr;
        for( ; psIter != nullptr; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( strcmp(psIter->pszValue, "Blob") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Name", nullptr);
                if( pszKey && strstr(pszKey, GDAL_MARKER_FOR_DIR) != nullptr )
                {
                    bNonEmpty = true;
                }
                else if( pszKey && strlen(pszKey) > osPrefix.size() )
                {
                    bNonEmpty = true;

                    CachedFileProp prop;
                    prop.eExists = EXIST_YES;
                    prop.bHasComputedFileSize = true;
                    prop.fileSize = static_cast<GUIntBig>(
                        CPLAtoGIntBig(CPLGetXMLValue(psIter, "Properties.Content-Length", "0")));
                    prop.bIsDirectory = false;
                    prop.mTime = 0;

                    int nYear, nMonth, nDay, nHour, nMinute, nSecond;
                    if( CPLParseRFC822DateTime(
                        CPLGetXMLValue(psIter, "Properties.Last-Modified", ""),
                                                    &nYear,
                                                    &nMonth,
                                                    &nDay,
                                                    &nHour,
                                                    &nMinute,
                                                    &nSecond,
                                                    nullptr,
                                                    nullptr ) )
                    {
                        struct tm brokendowntime;
                        brokendowntime.tm_year = nYear - 1900;
                        brokendowntime.tm_mon = nMonth - 1;
                        brokendowntime.tm_mday = nDay;
                        brokendowntime.tm_hour = nHour;
                        brokendowntime.tm_min = nMinute;
                        brokendowntime.tm_sec = nSecond < 0 ? 0 : nSecond;
                        prop.mTime =
                            static_cast<time_t>(
                                CPLYMDHMSToUnixTime(&brokendowntime));
                    }

                    aoProps.push_back(
                        std::pair<CPLString, CachedFileProp>
                            (pszKey + osPrefix.size(), prop));
                    aoNameCount[pszKey + osPrefix.size()] ++;
                }
            }
            else if( strcmp(psIter->pszValue, "BlobPrefix") == 0 ||
                     strcmp(psIter->pszValue, "Container") == 0 )
            {
                bNonEmpty = true;

                const char* pszKey = CPLGetXMLValue(psIter, "Name", nullptr);
                if( pszKey && strncmp(pszKey, osPrefix, osPrefix.size()) == 0 )
                {
                    CPLString osKey = pszKey;
                    if( !osKey.empty() && osKey.back() == '/' )
                        osKey.resize(osKey.size()-1);
                    if( osKey.size() > osPrefix.size() )
                    {
                        CachedFileProp prop;
                        prop.eExists = EXIST_YES;
                        prop.bIsDirectory = true;
                        prop.bHasComputedFileSize = true;
                        prop.fileSize = 0;
                        prop.mTime = 0;

                        aoProps.push_back(
                            std::pair<CPLString, CachedFileProp>
                                (osKey.c_str() + osPrefix.size(), prop));
                        aoNameCount[osKey.c_str() + osPrefix.size()] ++;
                    }
                }
            }

            if( nMaxFiles > 0 && aoProps.size() > static_cast<unsigned>(nMaxFiles) )
                break;
        }

        if( !(nMaxFiles > 0 && aoProps.size() > static_cast<unsigned>(nMaxFiles)) )
        {
            osNextMarker = CPLGetXMLValue(psEnumerationResults, "NextMarker", "");
            bIsTruncated =
                CPLTestBool(CPLGetXMLValue(psEnumerationResults,
                                           "IsTruncated", "false"));
        }

        for( size_t i = 0; i < aoProps.size(); i++ )
        {
            CPLString osSuffix;
            if( aoNameCount[aoProps[i].first] == 2 &&
                    aoProps[i].second.bIsDirectory )
            {
                // Add a / suffix to disambiguish the situation
                // Normally we don't suffix directories with /, but we have
                // no alternative here
                osSuffix = "/";
            }
            if( bCacheResults )
            {
                CPLString osCachedFilename =
                        osBaseURL + "/" + osPrefix + aoProps[i].first + osSuffix;
#if DEBUG_VERBOSE
                CPLDebug("AZURE", "Cache %s", osCachedFilename.c_str());
#endif
                *GetCachedFileProp(osCachedFilename) = aoProps[i].second;
            }
            osFileList.AddString( (aoProps[i].first + osSuffix).c_str() );
        }

        if( osFileList.size() == 0 && (bNonEmpty || osPrefix.empty()) )
        {
            // To avoid an error to be reported
            osFileList.AddString(".");
        }
    }
    CPLDestroyXMLNode(psTree);
}

/************************************************************************/
/*                       VSIAzureFSHandler                              */
/************************************************************************/

class VSIAzureFSHandler final : public IVSIS3LikeFSHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIAzureFSHandler)

  protected:
    VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
    CPLString GetURLFromFilename( const CPLString& osFilename ) override;

    IVSIS3LikeHandleHelper* CreateHandleHelper(
        const char* pszURI, bool bAllowNoObject) override;

    char** GetFileList( const char *pszFilename,
                        int nMaxFiles,
                        bool* pbGotFileList ) override;

    void InvalidateRecursive( const CPLString& osDirnameIn );

  public:
    VSIAzureFSHandler() = default;
    ~VSIAzureFSHandler() override = default;

    CPLString GetFSPrefix() override { return "/vsiaz/"; }
    const char* GetDebugKey() const override { return "AZURE"; }

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError ) override;

    int Unlink( const char *pszFilename ) override;
    int Mkdir( const char *, long  ) override;
    int Rmdir( const char * ) override;
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;

    const char* GetOptions() override;

    char* GetSignedURL( const char* pszFilename, CSLConstList papszOptions ) override;

    char** GetFileList( const char *pszFilename,
                        int nMaxFiles,
                        bool bCacheResults,
                        bool* pbGotFileList );
};

/************************************************************************/
/*                          VSIAzureHandle                              */
/************************************************************************/

class VSIAzureHandle final : public VSICurlHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIAzureHandle)

    VSIAzureBlobHandleHelper* m_poHandleHelper = nullptr;

  protected:
        virtual struct curl_slist* GetCurlHeaders( const CPLString& osVerb,
                    const struct curl_slist* psExistingHeaders ) override;
        virtual bool IsDirectoryFromExists( const char* pszVerb,
                                            int response_code ) override;

    public:
        VSIAzureHandle( VSIAzureFSHandler* poFS, const char* pszFilename,
                     VSIAzureBlobHandleHelper* poHandleHelper);
        virtual ~VSIAzureHandle();
};

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIAzureFSHandler::CreateFileHandle(const char* pszFilename)
{
    VSIAzureBlobHandleHelper* poHandleHelper =
        VSIAzureBlobHandleHelper::BuildFromURI( pszFilename + GetFSPrefix().size(),
                                         GetFSPrefix() );
    if( poHandleHelper == nullptr )
        return nullptr;
    return new VSIAzureHandle(this, pszFilename, poHandleHelper);
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIAzureFSHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                          int nFlags )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return -1;

    CPLString osFilename(pszFilename);
    if( osFilename.find('/', GetFSPrefix().size()) == std::string::npos )
        osFilename += "/";
    return VSICurlFilesystemHandler::Stat(osFilename, pStatBuf, nFlags);
}


/************************************************************************/
/*                          VSIAzureWriteHandle                         */
/************************************************************************/

class VSIAzureWriteHandle final : public VSIAppendWriteHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIAzureWriteHandle)

    VSIAzureBlobHandleHelper  *m_poHandleHelper = nullptr;

    bool                Send(bool bIsLastBlock) override;
    bool                SendInternal(bool bInitOnly, bool bIsLastBlock);

    void                InvalidateParentDirectory();

    public:
        VSIAzureWriteHandle( VSIAzureFSHandler* poFS,
                          const char* pszFilename,
                          VSIAzureBlobHandleHelper* poHandleHelper );
        virtual ~VSIAzureWriteHandle();
};

/************************************************************************/
/*                        GetAzureBufferSize()                          */
/************************************************************************/

static int GetAzureBufferSize()
{
    int nBufferSize;
    int nChunkSizeMB = atoi(CPLGetConfigOption("VSIAZ_CHUNK_SIZE", "4"));
    if( nChunkSizeMB <= 0 || nChunkSizeMB > 4 )
        nBufferSize = 4 * 1024 * 1024;
    else
        nBufferSize = nChunkSizeMB * 1024 * 1024;

    // For testing only !
    const char* pszChunkSizeBytes =
        CPLGetConfigOption("VSIAZ_CHUNK_SIZE_BYTES", nullptr);
    if( pszChunkSizeBytes )
        nBufferSize = atoi(pszChunkSizeBytes);
    if( nBufferSize <= 0 || nBufferSize > 4 * 1024 * 1024 )
        nBufferSize = 4 * 1024 * 1024;
    return nBufferSize;
}

/************************************************************************/
/*                       VSIAzureWriteHandle()                          */
/************************************************************************/

VSIAzureWriteHandle::VSIAzureWriteHandle( VSIAzureFSHandler* poFS,
                                    const char* pszFilename,
                                    VSIAzureBlobHandleHelper* poHandleHelper) :
        VSIAppendWriteHandle(poFS, poFS->GetFSPrefix(), pszFilename, GetAzureBufferSize()),
        m_poHandleHelper(poHandleHelper)
{
}

/************************************************************************/
/*                      ~VSIAzureWriteHandle()                          */
/************************************************************************/

VSIAzureWriteHandle::~VSIAzureWriteHandle()
{
    Close();
    delete m_poHandleHelper;
}

/************************************************************************/
/*                    InvalidateParentDirectory()                       */
/************************************************************************/

void VSIAzureWriteHandle::InvalidateParentDirectory()
{
    m_poFS->InvalidateCachedData(
        m_poHandleHelper->GetURL().c_str() );

    CPLString osFilenameWithoutSlash(m_osFilename);
    if( !osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/' )
        osFilenameWithoutSlash.resize( osFilenameWithoutSlash.size() - 1 );
    m_poFS->InvalidateDirContent( CPLGetDirname(osFilenameWithoutSlash) );
}

/************************************************************************/
/*                             Send()                                   */
/************************************************************************/

bool VSIAzureWriteHandle::Send(bool bIsLastBlock)
{
    if( !bIsLastBlock )
    {
        CPLAssert( m_nBufferOff == m_nBufferSize );
        if( m_nCurOffset == static_cast<vsi_l_offset>(m_nBufferSize) )
        {
            // First full buffer ? Then create the blob empty
            if( !SendInternal( true, false ) )
                return false;
        }
    }
    return SendInternal( false, bIsLastBlock );
}

/************************************************************************/
/*                          SendInternal()                              */
/************************************************************************/

bool VSIAzureWriteHandle::SendInternal(bool bInitOnly, bool bIsLastBlock)
{
    bool bSuccess = true;
    const bool bSingleBlock = bIsLastBlock &&
                ( m_nCurOffset <= static_cast<vsi_l_offset>(m_nBufferSize) );

    for( int i = 0; i < 2; i++ )
    {
        m_nBufferOffReadCallback = 0;
        CURL* hCurlHandle = curl_easy_init();

        m_poHandleHelper->ResetQueryParameters();
        if( !bSingleBlock && !bInitOnly )
        {
            m_poHandleHelper->AddQueryParameter("comp", "appendblock");
        }

        curl_easy_setopt(hCurlHandle, CURLOPT_URL,
                            m_poHandleHelper->GetURL().c_str());
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA,
                         static_cast<VSIAppendWriteHandle*>(this));

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle, nullptr));

        CPLString osContentLength; // leave it in this scope
        if( bSingleBlock )
        {
            curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);
            if( m_nBufferOff )
                headers = curl_slist_append(headers, "Expect: 100-continue");
            osContentLength.Printf("Content-Length: %d", m_nBufferOff);
            headers = curl_slist_append(headers, osContentLength.c_str());
            headers = curl_slist_append(headers, "x-ms-blob-type: BlockBlob");
        }
        else if( bInitOnly )
        {
            curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, 0);
            headers = curl_slist_append(headers, "Content-Length: 0");
            headers = curl_slist_append(headers, "x-ms-blob-type: AppendBlob");
        }
        else
        {
            curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);
            osContentLength.Printf("Content-Length: %d", m_nBufferOff);
            headers = curl_slist_append(headers, osContentLength.c_str());
            headers = curl_slist_append(headers, "x-ms-blob-type: AppendBlob");
        }

        headers = VSICurlMergeHeaders(headers,
                        m_poHandleHelper->GetCurlHeaders("PUT", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                            VSICurlHandleWriteFunc);

        MultiPerform(m_poFS->GetCurlMultiHandleFor(m_poHandleHelper->GetURL()),
                     hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

        bool bRetry = false;
        if( i == 0 && response_code == 409 )
        {
            CPLDebug(cpl::down_cast<VSIAzureFSHandler*>(m_poFS)->GetDebugKey(),
                     "%s",
                        sWriteFuncData.pBuffer
                        ? sWriteFuncData.pBuffer
                        : "(null)");

            // The blob type is invalid for this operation
            // Delete the file, and retry
            if( cpl::down_cast<VSIAzureFSHandler*>(m_poFS)->
                    DeleteObject(m_osFilename) == 0 )
            {
                bRetry = true;
            }
        }
        else if( response_code != 201 )
        {
            CPLDebug(cpl::down_cast<VSIAzureFSHandler*>(m_poFS)->GetDebugKey(),
                     "%s",
                        sWriteFuncData.pBuffer
                        ? sWriteFuncData.pBuffer
                        : "(null)");
            CPLError(CE_Failure, CPLE_AppDefined,
                        "PUT of %s failed",
                        m_osFilename.c_str());
            bSuccess = false;
        }
        else
        {
            InvalidateParentDirectory();
        }

        CPLFree(sWriteFuncData.pBuffer);

        curl_easy_cleanup(hCurlHandle);

        if( !bRetry )
            break;
    }

    return bSuccess;
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIAzureFSHandler::Open( const char *pszFilename,
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
        VSIAzureBlobHandleHelper* poHandleHelper =
            VSIAzureBlobHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str());
        if( poHandleHelper == nullptr )
            return nullptr;
        return new VSIAzureWriteHandle(this, pszFilename, poHandleHelper);
    }

    return
        VSICurlFilesystemHandler::Open(pszFilename, pszAccess, bSetError);
}

/************************************************************************/
/*                          GetURLFromFilename()                        */
/************************************************************************/

CPLString VSIAzureFSHandler::GetURLFromFilename( const CPLString& osFilename )
{
    CPLString osFilenameWithoutPrefix = osFilename.substr(GetFSPrefix().size());
    VSIAzureBlobHandleHelper* poHandleHelper =
        VSIAzureBlobHandleHelper::BuildFromURI( osFilenameWithoutPrefix, GetFSPrefix() );
    if( poHandleHelper == nullptr )
        return CPLString();
    CPLString osURL( poHandleHelper->GetURL() );
    delete poHandleHelper;
    return osURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper* VSIAzureFSHandler::CreateHandleHelper(const char* pszURI,
                                                           bool)
{
    return VSIAzureBlobHandleHelper::BuildFromURI(pszURI, GetFSPrefix().c_str());
}

/************************************************************************/
/*                         InvalidateRecursive()                        */
/************************************************************************/

void VSIAzureFSHandler::InvalidateRecursive( const CPLString& osDirnameIn )
{
    // As Azure directories disappear as soon there is no remaining file
    // we may need to invalidate the whole hierarchy
    CPLString osDirname(osDirnameIn);
    while( osDirname.size() > GetFSPrefix().size() )
    {
        InvalidateDirContent(osDirname);
        InvalidateCachedData( GetURLFromFilename(osDirname) );
        osDirname = CPLGetDirname(osDirname);
    }
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSIAzureFSHandler::Unlink( const char *pszFilename )
{
    int ret = IVSIS3LikeFSHandler::Unlink(pszFilename);
    if( ret != 0 )
        return ret;

    InvalidateRecursive(CPLGetDirname(pszFilename));
    return 0;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIAzureFSHandler::Mkdir( const char * pszDirname, long /* nMode */ )
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

    CPLString osDirnameWithoutEndSlash(osDirname);
    osDirnameWithoutEndSlash.resize( osDirnameWithoutEndSlash.size() - 1 );
    InvalidateCachedData( GetURLFromFilename(osDirname) );
    InvalidateCachedData( GetURLFromFilename(osDirnameWithoutEndSlash) );
    InvalidateDirContent( CPLGetDirname(osDirnameWithoutEndSlash) );

    VSILFILE* fp = VSIFOpenL((osDirname + GDAL_MARKER_FOR_DIR).c_str(),
                             "wb");
    if( fp != nullptr )
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

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIAzureFSHandler::Rmdir( const char * pszDirname )
{
    if( !STARTS_WITH_CI(pszDirname, GetFSPrefix()) )
        return -1;

    CPLString osDirname(pszDirname);
    if( !osDirname.empty() && osDirname.back() != '/' )
        osDirname += "/";

    VSIStatBufL sStat;
    if( VSIStatL(osDirname, &sStat) != 0 )
    {
        InvalidateCachedData(
            GetURLFromFilename(osDirname.substr(0, osDirname.size() - 1)) );
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

    char** papszFileList = ReadDirEx(osDirname, 1);
    bool bEmptyDir = (papszFileList != nullptr && EQUAL(papszFileList[0], ".") &&
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
    InvalidateCachedData( GetURLFromFilename(osDirname) );
    InvalidateCachedData( GetURLFromFilename(osDirnameWithoutEndSlash) );
    InvalidateRecursive( CPLGetDirname(osDirnameWithoutEndSlash) );
    if( osDirnameWithoutEndSlash.find('/', GetFSPrefix().size()) ==
                                                        std::string::npos )
    {
        CPLDebug(GetDebugKey(), "%s is a container", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    return DeleteObject((osDirname + GDAL_MARKER_FOR_DIR).c_str());
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** VSIAzureFSHandler::GetFileList( const char *pszDirname,
                                    int nMaxFiles,
                                    bool* pbGotFileList )
{
    return GetFileList(pszDirname, nMaxFiles, true, pbGotFileList);
}


char** VSIAzureFSHandler::GetFileList( const char *pszDirname,
                                       int nMaxFiles,
                                       bool bCacheResults,
                                       bool* pbGotFileList )
{
    if( ENABLE_DEBUG )
        CPLDebug(GetDebugKey(), "GetFileList(%s)" , pszDirname);
    *pbGotFileList = false;
    CPLString osDirnameWithoutPrefix = pszDirname + GetFSPrefix().size();
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

    IVSIS3LikeHandleHelper* poHandleHelper =
        CreateHandleHelper(osBucket, true);
    if( poHandleHelper == nullptr )
    {
        return nullptr;
    }

    WriteFuncStruct sWriteFuncData;

    CPLStringList osFileList; // must be left in this scope !
    CPLString osNextMarker; // must be left in this scope !

    CPLString osMaxKeys = CPLGetConfigOption("AZURE_MAX_RESULTS", "");
    const int AZURE_SERVER_LIMIT_SINGLE_REQUEST = 5000;
    if( nMaxFiles > 0 && nMaxFiles < AZURE_SERVER_LIMIT_SINGLE_REQUEST &&
        (osMaxKeys.empty() || nMaxFiles < atoi(osMaxKeys)) )
    {
        osMaxKeys.Printf("%d", nMaxFiles);
    }

    while( true )
    {
        poHandleHelper->ResetQueryParameters();
        CPLString osBaseURL(poHandleHelper->GetURL());

        CURLM* hCurlMultiHandle = GetCurlMultiHandleFor(osBaseURL);
        CURL* hCurlHandle = curl_easy_init();

        poHandleHelper->AddQueryParameter("comp", "list");
        if( !osNextMarker.empty() )
            poHandleHelper->AddQueryParameter("marker", osNextMarker);
        if( !osMaxKeys.empty() )
             poHandleHelper->AddQueryParameter("maxresults", osMaxKeys);

        if( !osDirnameWithoutPrefix.empty() )
        {
            poHandleHelper->AddQueryParameter("restype", "container");

            poHandleHelper->AddQueryParameter("delimiter", "/");
            if( !osObjectKey.empty() )
                poHandleHelper->AddQueryParameter("prefix", osObjectKey + "/");
        }

        struct curl_slist* headers =
            VSICurlSetOptions(hCurlHandle, poHandleHelper->GetURL(), nullptr);

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, nullptr);

        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        headers = VSICurlMergeHeaders(headers,
                               poHandleHelper->GetCurlHeaders("GET", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        MultiPerform(hCurlMultiHandle, hCurlHandle);

        if( headers != nullptr )
            curl_slist_free_all(headers);

        if( sWriteFuncData.pBuffer == nullptr)
        {
            delete poHandleHelper;
            curl_easy_cleanup(hCurlHandle);
            return nullptr;
        }

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            CPLDebug(GetDebugKey(), "%s",
                        sWriteFuncData.pBuffer
                        ? sWriteFuncData.pBuffer : "(null)");
            CPLFree(sWriteFuncData.pBuffer);
            delete poHandleHelper;
            curl_easy_cleanup(hCurlHandle);
            return nullptr;
        }
        else
        {
            *pbGotFileList = true;
            bool bIsTruncated;
            AnalyseAzureFileList( osBaseURL,
                                  bCacheResults,
                                  sWriteFuncData.pBuffer,
                                  osFileList,
                                  nMaxFiles,
                                  bIsTruncated,
                                  osNextMarker );

            CPLFree(sWriteFuncData.pBuffer);

            if( osNextMarker.empty() )
            {
                delete poHandleHelper;
                curl_easy_cleanup(hCurlHandle);
                return osFileList.StealList();
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char* VSIAzureFSHandler::GetOptions()
{
    static CPLString osOptions(
        CPLString("<Options>") +
    "  <Option name='AZURE_STORAGE_CONNECTION_STRING' type='string' "
        "description='Connection string that contains account name and "
        "secret key'/>"
    "  <Option name='AZURE_STORAGE_ACCOUNT' type='string' "
        "description='Storage account. To use with AZURE_STORAGE_ACCESS_KEY'/>"
    "  <Option name='AZURE_STORAGE_ACCESS_KEY' type='string' "
        "description='Secret key'/>"
    "  <Option name='VSIAZ_CHUNK_SIZE' type='int' "
        "description='Size in MB for chunks of files that are uploaded' "
        "default='4' min='1' max='4'/>" +
        VSICurlFilesystemHandler::GetOptionsStatic() +
        "</Options>");
    return osOptions.c_str();
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

char* VSIAzureFSHandler::GetSignedURL(const char* pszFilename, CSLConstList papszOptions )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    VSIAzureBlobHandleHelper* poHandleHelper =
        VSIAzureBlobHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                               GetFSPrefix().c_str(),
                                               papszOptions);
    if( poHandleHelper == nullptr )
    {
        return nullptr;
    }

    CPLString osRet(poHandleHelper->GetSignedURL(papszOptions));

    delete poHandleHelper;
    return CPLStrdup(osRet);
}

/************************************************************************/
/*                           VSIAzureHandle()                           */
/************************************************************************/

VSIAzureHandle::VSIAzureHandle( VSIAzureFSHandler* poFSIn,
                          const char* pszFilename,
                          VSIAzureBlobHandleHelper* poHandleHelper ) :
        VSICurlHandle(poFSIn, pszFilename, poHandleHelper->GetURL()),
        m_poHandleHelper(poHandleHelper)
{
}

/************************************************************************/
/*                          ~VSIAzureHandle()                           */
/************************************************************************/

VSIAzureHandle::~VSIAzureHandle()
{
    delete m_poHandleHelper;
}


/************************************************************************/
/*                          GetCurlHeaders()                            */
/************************************************************************/

struct curl_slist* VSIAzureHandle::GetCurlHeaders( const CPLString& osVerb,
                                const struct curl_slist* psExistingHeaders )
{
    return m_poHandleHelper->GetCurlHeaders( osVerb, psExistingHeaders );
}

/************************************************************************/
/*                         IsDirectoryFromExists()                      */
/************************************************************************/

bool VSIAzureHandle::IsDirectoryFromExists( const char* /*pszVerb*/,
                                            int response_code )
{
    if( response_code != 404 )
        return false;

    CPLString osDirname(m_osFilename);
    if( osDirname.size() > poFS->GetFSPrefix().size() && osDirname.back() == '/' )
        osDirname.resize(osDirname.size() - 1);
    bool bIsDir;
    if( poFS->ExistsInCacheDirList(osDirname, &bIsDir) )
        return bIsDir;

    bool bGotFileList = false;
    char** papszDirContent = reinterpret_cast<VSIAzureFSHandler*>(poFS)
                        ->GetFileList( osDirname, 1, false, &bGotFileList );
    bIsDir = papszDirContent != nullptr && papszDirContent[0] != nullptr;
    CSLDestroy(papszDirContent);
    return bIsDir;
}

} /* end of namespace cpl */


#endif // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                      VSIInstallAzureFileHandler()                    */
/************************************************************************/

/**
 * \brief Install /vsiaz/ Microsoft Azure Blob file system handler
 * (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsiaz">/vsiaz/ documentation</a>
 *
 * @since GDAL 2.3
 */

void VSIInstallAzureFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsiaz/", new cpl::VSIAzureFSHandler );
}

#endif /* HAVE_CURL */
