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

CPL_CVSID("$Id$")

#ifndef HAVE_CURL

void VSIInstallSwiftFileHandler( void )
{
    // Not supported
}

#else

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

#define ENABLE_DEBUG 0

namespace cpl {

/************************************************************************/
/*                       AnalyseSwiftFileList()                         */
/************************************************************************/

void VSICurlFilesystemHandler::AnalyseSwiftFileList(
    const CPLString& osBaseURL,
    const CPLString& osPrefix,
    const char* pszJson,
    CPLStringList& osFileList,
    int nMaxFilesThisQuery,
    int nMaxFiles,
    bool& bIsTruncated,
    CPLString& osNextMarker )
{
#if DEBUG_VERBOSE
    CPLDebug("SWIFT", "%s", pszJson);
#endif
    osNextMarker = "";
    bIsTruncated = false;

    CPLJSONDocument oDoc;
    if( !oDoc.LoadMemory(reinterpret_cast<const GByte*>(pszJson)) )
        return;

    std::vector< std::pair<CPLString, FileProp> > aoProps;
    // Count the number of occurrences of a path. Can be 1 or 2. 2 in the case
    // that both a filename and directory exist
    std::map<CPLString, int> aoNameCount;

    CPLJSONArray oArray = oDoc.GetRoot().ToArray();
    for( int i = 0; i < oArray.Size(); i++ )
    {
        CPLJSONObject oItem = oArray[i];
        std::string osName = oItem.GetString("name");
        GInt64 nSize = oItem.GetLong("bytes");
        std::string osLastModified = oItem.GetString("last_modified");
        CPLString osSubdir = oItem.GetString("subdir");
        bool bHasCount = oItem.GetLong("count", -1) >= 0;
        if( !osName.empty() )
        {
            osNextMarker = osName;
            if( osName.size() > osPrefix.size() &&
                osName.substr(0, osPrefix.size()) == osPrefix )
            {
                if( bHasCount )
                {
                    // Case when listing /vsiswift/
                    FileProp prop;
                    prop.eExists = EXIST_YES;
                    prop.bIsDirectory = true;
                    prop.bHasComputedFileSize = true;
                    prop.fileSize = 0;
                    prop.mTime = 0;

                    aoProps.push_back(
                        std::pair<CPLString, FileProp>
                            (osName, prop));
                    aoNameCount[ osName ] ++;
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
                    if( sscanf( osLastModified.c_str(),
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
                        prop.mTime =
                            static_cast<time_t>(
                                CPLYMDHMSToUnixTime(&brokendowntime));
                    }

                    aoProps.push_back(
                        std::pair<CPLString, FileProp>
                            (osName.substr(osPrefix.size()), prop));
                    aoNameCount[ osName.substr(osPrefix.size()) ] ++;
                }
            }
        }
        else if( !osSubdir.empty() )
        {
            osNextMarker = osSubdir;
            if( osSubdir.back() == '/' )
                osSubdir.resize( osSubdir.size() - 1 );
            if( osSubdir.find(osPrefix) == 0 )
            {

                FileProp prop;
                prop.eExists = EXIST_YES;
                prop.bIsDirectory = true;
                prop.bHasComputedFileSize = true;
                prop.fileSize = 0;
                prop.mTime = 0;

                aoProps.push_back(
                    std::pair<CPLString, FileProp>
                        (osSubdir.substr(osPrefix.size()), prop));
                aoNameCount[ osSubdir.substr(osPrefix.size()) ] ++;
            }
        }

        if( nMaxFiles > 0 && aoProps.size() > static_cast<unsigned>(nMaxFiles) )
            break;
    }

    bIsTruncated =
        aoProps.size() >= static_cast<unsigned>(nMaxFilesThisQuery);
    if( !bIsTruncated )
    {
        osNextMarker.clear();
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
        if( nMaxFiles != 1 )
        {
            CPLString osCachedFilename =
                    osBaseURL + "/" + CPLAWSURLEncode(osPrefix,false) +
                    CPLAWSURLEncode(aoProps[i].first,false) + osSuffix;
#if DEBUG_VERBOSE
            CPLDebug("SWIFT", "Cache %s", osCachedFilename.c_str());
#endif
            SetCachedFileProp(osCachedFilename, aoProps[i].second);
        }
        osFileList.AddString( (aoProps[i].first + osSuffix).c_str() );
    }
}

/************************************************************************/
/*                         VSISwiftFSHandler                            */
/************************************************************************/

class VSISwiftFSHandler final : public IVSIS3LikeFSHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSISwiftFSHandler)

protected:
        VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
        CPLString GetURLFromFilename( const CPLString& osFilename ) override;

        const char* GetDebugKey() const override { return "SWIFT"; }

        IVSIS3LikeHandleHelper* CreateHandleHelper(
            const char* pszURI, bool bAllowNoObject) override;

        CPLString GetFSPrefix() const override { return "/vsiswift/"; }

        char** GetFileList( const char *pszFilename,
                            int nMaxFiles,
                            bool* pbGotFileList ) override;

        void ClearCache() override;

public:
        VSISwiftFSHandler() = default;
        ~VSISwiftFSHandler() override;

        VSIVirtualHandle *Open( const char *pszFilename,
                                const char *pszAccess,
                                bool bSetError ) override;

        int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                int nFlags ) override;

        VSIDIR* OpenDir( const char *pszPath, int nRecurseDepth,
                                const char* const *papszOptions) override
        {
            return VSICurlFilesystemHandler::OpenDir(pszPath, nRecurseDepth,
                                                     papszOptions);
        }

        const char* GetOptions() override;
};

/************************************************************************/
/*                            VSISwiftHandle                              */
/************************************************************************/

class VSISwiftHandle final : public IVSIS3LikeHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSISwiftHandle)

    VSISwiftHandleHelper* m_poHandleHelper = nullptr;

  protected:
    struct curl_slist* GetCurlHeaders(
        const CPLString& osVerb,
        const struct curl_slist* psExistingHeaders ) override;
    virtual bool Authenticate() override;

  public:
    VSISwiftHandle( VSISwiftFSHandler* poFS,
                  const char* pszFilename,
                  VSISwiftHandleHelper* poHandleHelper );
    ~VSISwiftHandle() override;
};

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSISwiftFSHandler::Open( const char *pszFilename,
                                        const char *pszAccess,
                                        bool bSetError)
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr )
    {
        if( strchr(pszAccess, '+') != nullptr &&
            !CPLTestBool(CPLGetConfigOption("CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "NO")) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "w+ not supported for /vsiswift, unless "
                        "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE is set to YES");
            errno = EACCES;
            return nullptr;
        }

        VSISwiftHandleHelper* poHandleHelper =
            VSISwiftHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str());
        if( poHandleHelper == nullptr )
            return nullptr;
        UpdateHandleFromMap(poHandleHelper);
        VSIS3WriteHandle* poHandle =
            new VSIS3WriteHandle(this, pszFilename, poHandleHelper, true);
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
        VSICurlFilesystemHandler::Open(pszFilename, pszAccess, bSetError);
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
    VSICurlFilesystemHandler::ClearCache();

    VSISwiftHandleHelper::ClearCache();
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char* VSISwiftFSHandler::GetOptions()
{
    static CPLString osOptions(
        CPLString("<Options>") +
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
        "description='Key/password to use with authentication V1'/>" +
        VSICurlFilesystemHandler::GetOptionsStatic() +
        "</Options>");
    return osOptions.c_str();
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSISwiftFSHandler::CreateFileHandle(const char* pszFilename)
{
    VSISwiftHandleHelper* poHandleHelper =
        VSISwiftHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                           GetFSPrefix().c_str());
    if( poHandleHelper )
    {
        UpdateHandleFromMap(poHandleHelper);
        return new VSISwiftHandle(this, pszFilename, poHandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                         GetURLFromFilename()                         */
/************************************************************************/

CPLString VSISwiftFSHandler::GetURLFromFilename( const CPLString& osFilename )
{
    CPLString osFilenameWithoutPrefix = osFilename.substr(GetFSPrefix().size());

    VSISwiftHandleHelper* poHandleHelper =
        VSISwiftHandleHelper::BuildFromURI(osFilenameWithoutPrefix,
                                           GetFSPrefix().c_str());
    if( poHandleHelper == nullptr )
    {
        return "";
    }
    CPLString osBaseURL(poHandleHelper->GetURL());
    if( !osBaseURL.empty() && osBaseURL.back() == '/' )
        osBaseURL.resize(osBaseURL.size()-1);
    delete poHandleHelper;

    return osBaseURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper* VSISwiftFSHandler::CreateHandleHelper(const char* pszURI,
                                                              bool)
{
    return VSISwiftHandleHelper::BuildFromURI(
                                pszURI, GetFSPrefix().c_str());
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSISwiftFSHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                             int nFlags )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return -1;

    CPLString osFilename(pszFilename);
    if( osFilename.back() == '/' )
        osFilename.resize( osFilename.size() - 1 );

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    if( VSICurlFilesystemHandler::Stat(pszFilename, pStatBuf, nFlags) == 0 )
    {
        // if querying /vsiswift/container_name, the GET will succeed and
        // we would consider this as a file whereas it should be exposed as
        // a directory
        if( std::count(osFilename.begin(), osFilename.end(), '/') <= 2 )
        {

            IVSIS3LikeHandleHelper* poS3HandleHelper =
                CreateHandleHelper(pszFilename + GetFSPrefix().size(), true);
            CPLString osURL(poS3HandleHelper->GetURL());
            delete poS3HandleHelper;

            FileProp cachedFileProp;
            cachedFileProp.eExists = EXIST_YES;
            cachedFileProp.bHasComputedFileSize = false;
            cachedFileProp.fileSize = 0;
            cachedFileProp.bIsDirectory = true;
            cachedFileProp.mTime = 0;
            SetCachedFileProp(osURL, cachedFileProp);

            pStatBuf->st_size = 0;
            pStatBuf->st_mode = S_IFDIR;
        }
        return 0;
    }

    // In the case of a directory, a GET on it will not work, so we have to
    // query the upper directory contents
    if( std::count(osFilename.begin(), osFilename.end(), '/') < 2 )
        return -1;

    char** papszContents = VSIReadDir( CPLGetPath(osFilename) );
    int nRet = CSLFindStringCaseSensitive(papszContents,
                    CPLGetFilename(osFilename)) >= 0 ? 0 : -1;
    CSLDestroy(papszContents);
    if( nRet == 0 )
    {
        pStatBuf->st_mode = S_IFDIR;
    }
    return nRet;
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** VSISwiftFSHandler::GetFileList( const char *pszDirname,
                                    int nMaxFiles,
                                    bool* pbGotFileList )
{
    if( ENABLE_DEBUG )
        CPLDebug(GetDebugKey(), "GetFileList(%s)" , pszDirname);
    *pbGotFileList = false;
    CPLAssert( strlen(pszDirname) >= GetFSPrefix().size() );
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

    IVSIS3LikeHandleHelper* poS3HandleHelper =
        CreateHandleHelper(osBucket, true);
    if( poS3HandleHelper == nullptr )
    {
        return nullptr;
    }

    WriteFuncStruct sWriteFuncData;

    CPLStringList osFileList; // must be left in this scope !
    CPLString osNextMarker; // must be left in this scope !

    CPLString osMaxKeys = CPLGetConfigOption("SWIFT_MAX_KEYS", "10000");
    int nMaxFilesThisQuery = atoi(osMaxKeys);
    if( nMaxFiles > 0 && nMaxFiles <= 100 && nMaxFiles < nMaxFilesThisQuery )
    {
        nMaxFilesThisQuery = nMaxFiles+1;
    }
    const CPLString osPrefix(osObjectKey.empty() ? CPLString():
                                                        osObjectKey + "/");

    while( true )
    {
        bool bRetry;
        int nRetryCount = 0;
        const int nMaxRetry = atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                    CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)));
        // coverity[tainted_data]
        double dfRetryDelay = CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                    CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)));
        do
        {
            bRetry = false;
            poS3HandleHelper->ResetQueryParameters();
            CPLString osBaseURL(poS3HandleHelper->GetURL());

            CURLM* hCurlMultiHandle = GetCurlMultiHandleFor(osBaseURL);
            CURL* hCurlHandle = curl_easy_init();

            if( !osBucket.empty() )
            {
                poS3HandleHelper->AddQueryParameter("delimiter", "/");
                if( !osNextMarker.empty() )
                    poS3HandleHelper->AddQueryParameter("marker", osNextMarker);
                poS3HandleHelper->AddQueryParameter("limit",
                                            CPLSPrintf("%d", nMaxFilesThisQuery));
                if( !osPrefix.empty() )
                    poS3HandleHelper->AddQueryParameter("prefix", osPrefix);
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

            if( headers != nullptr )
                curl_slist_free_all(headers);

            if( sWriteFuncData.pBuffer == nullptr)
            {
                delete poS3HandleHelper;
                curl_easy_cleanup(hCurlHandle);
                CPLFree(sWriteFuncHeaderData.pBuffer);
                return nullptr;
            }

            long response_code = 0;
            curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
            if( response_code != 200 )
            {
                // Look if we should attempt a retry
                const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                    static_cast<int>(response_code), dfRetryDelay,
                    sWriteFuncHeaderData.pBuffer, szCurlErrBuf);
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
                    CPLFree(sWriteFuncData.pBuffer);
                    CPLFree(sWriteFuncHeaderData.pBuffer);
                }
                else
                {
                    CPLDebug(GetDebugKey(), "%s",
                                sWriteFuncData.pBuffer
                                ? sWriteFuncData.pBuffer : "(null)");
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
                AnalyseSwiftFileList( osBaseURL,
                                    osPrefix,
                                    sWriteFuncData.pBuffer,
                                    osFileList,
                                    nMaxFilesThisQuery,
                                    nMaxFiles,
                                    bIsTruncated,
                                    osNextMarker );

                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);

                if( osNextMarker.empty() )
                {
                    delete poS3HandleHelper;
                    curl_easy_cleanup(hCurlHandle);
                    return osFileList.StealList();
                }
            }

            curl_easy_cleanup(hCurlHandle);
        }
        while(bRetry);
    }
}

/************************************************************************/
/*                            VSISwiftHandle()                            */
/************************************************************************/

VSISwiftHandle::VSISwiftHandle( VSISwiftFSHandler* poFSIn,
                          const char* pszFilename,
                          VSISwiftHandleHelper* poHandleHelper ) :
        IVSIS3LikeHandle(poFSIn, pszFilename, poHandleHelper->GetURL()),
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

struct curl_slist* VSISwiftHandle::GetCurlHeaders( const CPLString& osVerb,
                                const struct curl_slist* psExistingHeaders )
{
    return m_poHandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                           Authenticate()                             */
/************************************************************************/

bool VSISwiftHandle::Authenticate()
{
    return m_poHandleHelper->Authenticate();
}



} /* end of namespace cpl */


#endif // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                     VSIInstallSwiftFileHandler()                     */
/************************************************************************/

/**
 * \brief Install /vsiswift/ OpenStack Swif Object Storage (Swift) file
 * system handler (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsiswift">/vsiswift/ documentation</a>
 *
 * @since GDAL 2.3
 */
void VSIInstallSwiftFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsiswift/", new cpl::VSISwiftFSHandler );
}

#endif /* HAVE_CURL */
