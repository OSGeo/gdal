/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for HTTP/FTP files
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2010-2015, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_vsi_virtual.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "cpl_hash_set.h"
#include "cpl_time.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_aws.h"
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

#ifndef HAVE_CURL

void VSIInstallCurlFileHandler(void)
{
    /* not supported */
}

void VSIInstallS3FileHandler(void)
{
    /* not supported */
}

/************************************************************************/
/*                      VSICurlInstallReadCbk()                         */
/************************************************************************/

int VSICurlInstallReadCbk (CPL_UNUSED VSILFILE* fp,
                           CPL_UNUSED VSICurlReadCbkFunc pfnReadCbk,
                           CPL_UNUSED void* pfnUserData,
                           CPL_UNUSED int bStopOnInterrruptUntilUninstall)
{
    return FALSE;
}


/************************************************************************/
/*                    VSICurlUninstallReadCbk()                         */
/************************************************************************/

int VSICurlUninstallReadCbk(CPL_UNUSED VSILFILE* fp)
{
    return FALSE;
}

#else

#include <curl/curl.h>

void CPLHTTPSetOptions(CURL *http_handle, char** papszOptions);
void VSICurlSetOptions(CURL* hCurlHandle, const char* pszURL);

#include <map>

#define ENABLE_DEBUG 1

static const int N_MAX_REGIONS = 1000;
static const int DOWNLOAD_CHUNK_SIZE = 16384;

namespace {

typedef enum
{
    EXIST_UNKNOWN = -1,
    EXIST_NO,
    EXIST_YES,
} ExistStatus;

typedef struct
{
    ExistStatus     eExists;
    bool            bHasComputedFileSize;
    vsi_l_offset    fileSize;
    bool            bIsDirectory;
    time_t          mTime;
} CachedFileProp;

typedef struct
{
    bool            bGotFileList;
    char**          papszFileList; /* only file name without path */
} CachedDirList;

typedef struct
{
    unsigned long   pszURLHash;
    vsi_l_offset    nFileOffsetStart;
    size_t          nSize;
    char           *pData;
} CachedRegion;

typedef struct
{
    char*           pBuffer;
    size_t          nSize;
    bool            bIsHTTP;
    bool            bIsInHeader;
    bool            bMultiRange;
    vsi_l_offset    nStartOffset;
    vsi_l_offset    nEndOffset;
    int             nHTTPCode;
    vsi_l_offset    nContentLength;
    bool            bFoundContentRange;
    bool            bError;
    bool            bDownloadHeaderOnly;

    VSILFILE           *fp; 
    VSICurlReadCbkFunc  pfnReadCbk;
    void               *pReadCbkUserData;
    bool                bInterrupted;
} WriteFuncStruct;

} /* end of anoymous namespace */

static const char* VSICurlGetCacheFileName()
{
    return "gdal_vsicurl_cache.bin";
}

/************************************************************************/
/*          VSICurlFindStringSensitiveExceptEscapeSequences()           */
/************************************************************************/

static int VSICurlFindStringSensitiveExceptEscapeSequences( char ** papszList,
                                                            const char * pszTarget )

{
    if( papszList == NULL )
        return -1;

    for( int i = 0; papszList[i] != NULL; i++ )
    {
        const char* pszIter1 = papszList[i];
        const char* pszIter2 = pszTarget;
        char ch1, ch2;
        /* The comparison is case-sensitive, escape for escaped */
        /* sequences where letters of the hexadecimal sequence */
        /* can be uppercase or lowercase depending on the quoting algorithm */
        while(true)
        {
            ch1 = *pszIter1;
            ch2 = *pszIter2;
            if (ch1 == '\0' || ch2 == '\0')
                break;
            if (ch1 == '%' && ch2 == '%' &&
                pszIter1[1] != '\0' && pszIter1[2] != '\0' &&
                pszIter2[1] != '\0' && pszIter2[2] != '\0')
            {
                if (!EQUALN(pszIter1+1, pszIter2+1, 2))
                    break;
                pszIter1 += 2;
                pszIter2 += 2;
            }
            if (ch1 != ch2)
                break;
            pszIter1 ++;
            pszIter2 ++;
        }
        if (ch1 == ch2 && ch1 == '\0')
            return i;
    }

    return -1;
}

/************************************************************************/
/*                      VSICurlIsFileInList()                           */
/************************************************************************/

static int VSICurlIsFileInList( char ** papszList, const char * pszTarget )
{
    int nRet = VSICurlFindStringSensitiveExceptEscapeSequences(papszList, pszTarget);
    if (nRet >= 0)
        return nRet;

    /* If we didn't find anything, try to URL-escape the target filename */
    char* pszEscaped = CPLEscapeString(pszTarget, -1, CPLES_URL);
    if (strcmp(pszTarget, pszEscaped) != 0)
    {
        nRet = VSICurlFindStringSensitiveExceptEscapeSequences(papszList, pszEscaped);
    }
    CPLFree(pszEscaped);
    return nRet;
}

/************************************************************************/
/*                     VSICurlFilesystemHandler                         */
/************************************************************************/

typedef struct
{
    CPLString       osURL;
    CURL           *hCurlHandle;
} CachedConnection;

class VSICurlHandle;

class VSICurlFilesystemHandler : public VSIFilesystemHandler 
{
    CachedRegion  **papsRegions;
    int             nRegions;

    std::map<CPLString, CachedFileProp*>   cacheFileSize;
    std::map<CPLString, CachedDirList*>        cacheDirList;

    int             bUseCacheDisk;

    /* Per-thread Curl connection cache */
    std::map<GIntBig, CachedConnection*> mapConnections;


    char**              ParseHTMLFileList(const char* pszFilename,
                                          int nMaxFiles,
                                          char* pszData,
                                          bool* pbGotFileList);

protected:
    CPLMutex       *hMutex;

    virtual CPLString GetFSPrefix() { return "/vsicurl/"; }
    virtual VSICurlHandle* CreateFileHandle(const char* pszURL);
    virtual char** GetFileList(const char *pszFilename,
                               int nMaxFiles,
                               bool* pbGotFileList);
    virtual CPLString GetURLFromDirname( const CPLString& osDirname );

    void AnalyseS3FileList( const CPLString& osBaseURL,
                            const char* pszXML,
                            CPLStringList& osFileList,
                            int nMaxFiles,
                            bool& bIsTruncated,
                            CPLString& osNextMarker );

public:
    VSICurlFilesystemHandler();
    ~VSICurlFilesystemHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );
    virtual int      Unlink( const char *pszFilename );
    virtual int      Rename( const char *oldpath, const char *newpath );
    virtual int      Mkdir( const char *pszDirname, long nMode );
    virtual int      Rmdir( const char *pszDirname );
    virtual char   **ReadDirEx( const char *pszDirname, int nMaxFiles );
            char   **ReadDirInternal( const char *pszDirname, int nMaxFiles, bool* pbGotFileList );
            void     InvalidateDirContent( const char *pszDirname );


    const CachedRegion* GetRegion(const char*     pszURL,
                                  vsi_l_offset    nFileOffsetStart);

    void                AddRegion(const char*     pszURL,
                                  vsi_l_offset    nFileOffsetStart,
                                  size_t          nSize,
                                  const char     *pData);

    CachedFileProp*     GetCachedFileProp(const char*     pszURL);
    void                InvalidateCachedFileProp(const char*     pszURL);

    void                AddRegionToCacheDisk(CachedRegion* psRegion);
    const CachedRegion* GetRegionFromCacheDisk(const char*     pszURL,
                                               vsi_l_offset nFileOffsetStart);

    CURL               *GetCurlHandleFor(CPLString osURL);
};

/************************************************************************/
/*                           VSICurlHandle                              */
/************************************************************************/

class VSICurlHandle : public VSIVirtualHandle
{

  protected:
    VSICurlFilesystemHandler* poFS;

    vsi_l_offset    fileSize;
    bool            bHasComputedFileSize;
    ExistStatus     eExists;
    bool            bIsDirectory;

  private:
    char*           pszURL;

    vsi_l_offset    curOffset;
    time_t          mTime;

    vsi_l_offset    lastDownloadedOffset;
    int             nBlocksToDownload;
    bool            bEOF;

    bool            DownloadRegion(vsi_l_offset startOffset, int nBlocks);

    VSICurlReadCbkFunc  pfnReadCbk;
    void               *pReadCbkUserData;
    bool                bStopOnInterrruptUntilUninstall;
    bool                bInterrupted;

  protected:
    virtual struct curl_slist* GetCurlHeaders(const CPLString& ) { return NULL; }
    virtual bool CanRestartOnError(const char*) { return false; }
    virtual bool UseLimitRangeGetInsteadOfHead() { return false; }
  virtual void ProcessGetFileSizeResult(const char* /* pszContent */ ) {}
    void SetURL(const char* pszURL);

  public:

    VSICurlHandle(VSICurlFilesystemHandler* poFS, const char* pszURL);
    ~VSICurlHandle();

    virtual int          Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t       Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual int          ReadMultiRange( int nRanges, void ** ppData,
                                         const vsi_l_offset* panOffsets, const size_t* panSizes );
    virtual size_t       Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int          Eof();
    virtual int          Flush();
    virtual int          Close();

    bool                 IsKnownFileSize() const { return bHasComputedFileSize; }
    vsi_l_offset         GetFileSize();
    bool                 Exists();
    bool                 IsDirectory() const { return bIsDirectory; }
    time_t               GetMTime() const { return mTime; }

    int                  InstallReadCbk(VSICurlReadCbkFunc pfnReadCbk,
                                        void* pfnUserData,
                                        int bStopOnInterrruptUntilUninstall);
    int                  UninstallReadCbk();
};

/************************************************************************/
/*                           VSICurlHandle()                            */
/************************************************************************/

VSICurlHandle::VSICurlHandle(VSICurlFilesystemHandler* poFSIn, const char* pszURLIn) :
    poFS(poFSIn),
    curOffset(0),
    lastDownloadedOffset(VSI_L_OFFSET_MAX),
    nBlocksToDownload(1),
    bEOF(false),
    pfnReadCbk(NULL),
    pReadCbkUserData(NULL),
    bStopOnInterrruptUntilUninstall(false),
    bInterrupted(false)
{
    pszURL = CPLStrdup(pszURLIn);
    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    eExists = cachedFileProp->eExists;
    fileSize = cachedFileProp->fileSize;
    bHasComputedFileSize = cachedFileProp->bHasComputedFileSize;
    bIsDirectory = cachedFileProp->bIsDirectory;
    mTime = cachedFileProp->mTime;
}

/************************************************************************/
/*                          ~VSICurlHandle()                            */
/************************************************************************/

VSICurlHandle::~VSICurlHandle()
{
    CPLFree(pszURL);
}

/************************************************************************/
/*                            SetURL()                                  */
/************************************************************************/

void VSICurlHandle::SetURL(const char* pszURLIn)
{
    CPLFree(pszURL);
    pszURL = CPLStrdup(pszURLIn);
}

/************************************************************************/
/*                          InstallReadCbk()                            */
/************************************************************************/

int   VSICurlHandle::InstallReadCbk(VSICurlReadCbkFunc pfnReadCbkIn,
                                    void* pfnUserDataIn,
                                    int bStopOnInterrruptUntilUninstallIn)
{
    if (pfnReadCbk != NULL)
        return FALSE;

    pfnReadCbk = pfnReadCbkIn;
    pReadCbkUserData = pfnUserDataIn;
    bStopOnInterrruptUntilUninstall = CPL_TO_BOOL(bStopOnInterrruptUntilUninstallIn);
    bInterrupted = false;
    return TRUE;
}

/************************************************************************/
/*                         UninstallReadCbk()                           */
/************************************************************************/

int VSICurlHandle::UninstallReadCbk()
{
    if (pfnReadCbk == NULL)
        return FALSE;

    pfnReadCbk = NULL;
    pReadCbkUserData = NULL;
    bStopOnInterrruptUntilUninstall = false;
    bInterrupted = false;
    return TRUE;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICurlHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if (nWhence == SEEK_SET)
    {
        curOffset = nOffset;
    }
    else if (nWhence == SEEK_CUR)
    {
        curOffset = curOffset + nOffset;
    }
    else
    {
        curOffset = GetFileSize() + nOffset;
    }
    bEOF = false;
    return 0;
}

/************************************************************************/
/*                       VSICurlSetOptions()                            */
/************************************************************************/

void VSICurlSetOptions(CURL* hCurlHandle, const char* pszURL)
{
    curl_easy_setopt(hCurlHandle, CURLOPT_URL, pszURL);

    CPLHTTPSetOptions(hCurlHandle, NULL);

/* 7.16 */
#if LIBCURL_VERSION_NUM >= 0x071000
    long option = CURLFTPMETHOD_SINGLECWD;
    curl_easy_setopt(hCurlHandle, CURLOPT_FTP_FILEMETHOD, option);
#endif

/* 7.12.3 */
#if LIBCURL_VERSION_NUM > 0x070C03
    /* ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif/ doesn't like EPSV command */
    curl_easy_setopt(hCurlHandle, CURLOPT_FTP_USE_EPSV, 0);
#endif

    curl_easy_setopt(hCurlHandle, CURLOPT_NOBODY, 0);
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPGET, 1); 
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADER, 0);

/* 7.16.4 */
#if LIBCURL_VERSION_NUM <= 0x071004
    curl_easy_setopt(hCurlHandle, CURLOPT_FTPLISTONLY, 0);
#elif LIBCURL_VERSION_NUM > 0x071004
    curl_easy_setopt(hCurlHandle, CURLOPT_DIRLISTONLY, 0);
#endif

    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, NULL);
}

/************************************************************************/
/*                    VSICURLInitWriteFuncStruct()                      */
/************************************************************************/

static void VSICURLInitWriteFuncStruct(WriteFuncStruct   *psStruct,
                                       VSILFILE          *fp,
                                       VSICurlReadCbkFunc pfnReadCbk,
                                       void              *pReadCbkUserData)
{
    psStruct->pBuffer = NULL;
    psStruct->nSize = 0;
    psStruct->bIsHTTP = false;
    psStruct->bIsInHeader = true;
    psStruct->bMultiRange = false;
    psStruct->nStartOffset = 0;
    psStruct->nEndOffset = 0;
    psStruct->nHTTPCode = 0;
    psStruct->nContentLength = 0;
    psStruct->bFoundContentRange = false;
    psStruct->bError = false;
    psStruct->bDownloadHeaderOnly = false;

    psStruct->fp = fp;
    psStruct->pfnReadCbk = pfnReadCbk;
    psStruct->pReadCbkUserData = pReadCbkUserData;
    psStruct->bInterrupted = false;
}

/************************************************************************/
/*                       VSICurlHandleWriteFunc()                       */
/************************************************************************/

static size_t VSICurlHandleWriteFunc(void *buffer, size_t count, size_t nmemb, void *req)
{
    WriteFuncStruct* psStruct = (WriteFuncStruct*) req;
    const size_t nSize = count * nmemb;

    char* pNewBuffer = (char*) VSIRealloc(psStruct->pBuffer,
                                          psStruct->nSize + nSize + 1);
    if (pNewBuffer)
    {
        psStruct->pBuffer = pNewBuffer;
        memcpy(psStruct->pBuffer + psStruct->nSize, buffer, nSize);
        psStruct->pBuffer[psStruct->nSize + nSize] = '\0';
        if (psStruct->bIsHTTP && psStruct->bIsInHeader)
        {
            char* pszLine = psStruct->pBuffer + psStruct->nSize;
            if (STARTS_WITH_CI(pszLine, "HTTP/1.0 ") ||
                STARTS_WITH_CI(pszLine, "HTTP/1.1 "))
                psStruct->nHTTPCode = atoi(pszLine + 9);
            else if (STARTS_WITH_CI(pszLine, "Content-Length: "))
                psStruct->nContentLength = CPLScanUIntBig(pszLine + 16,
                                                          static_cast<int>(strlen(pszLine + 16)));
            else if (STARTS_WITH_CI(pszLine, "Content-Range: "))
                psStruct->bFoundContentRange = true;

            /*if (nSize > 2 && pszLine[nSize - 2] == '\r' &&
                pszLine[nSize - 1] == '\n')
            {
                pszLine[nSize - 2] = 0;
                CPLDebug("VSICURL", "%s", pszLine);
                pszLine[nSize - 2] = '\r';
            }*/

            if (pszLine[0] == '\r' || pszLine[0] == '\n')
            {
                if (psStruct->bDownloadHeaderOnly)
                {
                    /* If moved permanently/temporarily, go on. Otherwise stop now*/
                    if (!(psStruct->nHTTPCode == 301 || psStruct->nHTTPCode == 302))
                        return 0;
                }
                else
                {
                    psStruct->bIsInHeader = false;

                    /* Detect servers that don't support range downloading */
                    if (psStruct->nHTTPCode == 200 &&
                        !psStruct->bMultiRange &&
                        !psStruct->bFoundContentRange &&
                        (psStruct->nStartOffset != 0 || psStruct->nContentLength > 10 *
                            (psStruct->nEndOffset - psStruct->nStartOffset + 1)))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "Range downloading not supported by this server !");
                        psStruct->bError = true;
                        return 0;
                    }
                }
            }
        }
        else
        {
            if (psStruct->pfnReadCbk)
            {
                if ( ! psStruct->pfnReadCbk(psStruct->fp, buffer, nSize,
                                            psStruct->pReadCbkUserData) )
                {
                    psStruct->bInterrupted = true;
                    return 0;
                }
            }
        }
        psStruct->nSize += nSize;
        return nmemb;
    }
    else
    {
        return 0;
    }
}


/************************************************************************/
/*                           GetFileSize()                              */
/************************************************************************/

vsi_l_offset VSICurlHandle::GetFileSize()
{
    WriteFuncStruct sWriteFuncData;
    WriteFuncStruct sWriteFuncHeaderData;

    if (bHasComputedFileSize)
        return fileSize;

    bHasComputedFileSize = true;

    /* Consider that only the files whose extension ends up with one that is */
    /* listed in CPL_VSIL_CURL_ALLOWED_EXTENSIONS exist on the server */
    /* This can speeds up dramatically open experience, in case the server */
    /* cannot return a file list */
    /* {noext} can be used as a special token to mean file with no extension */
    /* For example : */
    /* gdalinfo --config CPL_VSIL_CURL_ALLOWED_EXTENSIONS ".tif" /vsicurl/http://igskmncngs506.cr.usgs.gov/gmted/Global_tiles_GMTED/075darcsec/bln/W030/30N030W_20101117_gmted_bln075.tif */
    const char* pszAllowedExtensions =
        CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", NULL);
    if (pszAllowedExtensions)
    {
        char** papszExtensions = CSLTokenizeString2( pszAllowedExtensions, ", ", 0 );
        const size_t nURLLen = strlen(pszURL);
        bool bFound = false;
        for(int i=0;papszExtensions[i] != NULL;i++)
        {
            const size_t nExtensionLen = strlen(papszExtensions[i]);
            if( EQUAL(papszExtensions[i], "{noext}") )
            {
                if( nURLLen > 4 && strchr(pszURL + nURLLen - 4, '.') == NULL )
                {
                    bFound = true;
                    break;
                }
            }
            else if (nURLLen > nExtensionLen &&
                EQUAL(pszURL + nURLLen - nExtensionLen, papszExtensions[i]))
            {
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            eExists = EXIST_NO;
            fileSize = 0;

            CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
            cachedFileProp->bHasComputedFileSize = true;
            cachedFileProp->fileSize = fileSize;
            cachedFileProp->eExists = eExists;

            CSLDestroy(papszExtensions);

            return 0;
        }

        CSLDestroy(papszExtensions);
    }

#if LIBCURL_VERSION_NUM < 0x070B00
    /* Curl 7.10.X doesn't manage to unset the CURLOPT_RANGE that would have been */
    /* previously set, so we have to reinit the connection handle */
    poFS->GetCurlHandleFor("");
#endif
    CURL* hCurlHandle = poFS->GetCurlHandleFor(pszURL);

    VSICurlSetOptions(hCurlHandle, pszURL);

    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, NULL, NULL, NULL);

    /* We need that otherwise OSGEO4W's libcurl issue a dummy range request */
    /* when doing a HEAD when recycling connections */
    curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

    /* HACK for mbtiles driver: proper fix would be to auto-detect servers that don't accept HEAD */
    /* http://a.tiles.mapbox.com/v3/ doesn't accept HEAD, so let's start a GET */
    /* and interrupt is as soon as the header is found */
    CPLString osVerb;
    if( UseLimitRangeGetInsteadOfHead() )
    {
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, VSICurlHandleWriteFunc);

        sWriteFuncHeaderData.bIsHTTP = STARTS_WITH(pszURL, "http");
        osVerb = "GET";

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, "0-4095");
    }
    else if (strstr(pszURL, ".tiles.mapbox.com/") != NULL
	|| !CSLTestBoolean(CPLGetConfigOption("CPL_VSIL_CURL_USE_HEAD", "YES")))
    {
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, VSICurlHandleWriteFunc);

        sWriteFuncHeaderData.bIsHTTP = STARTS_WITH(pszURL, "http");
        sWriteFuncHeaderData.bDownloadHeaderOnly = true;
        osVerb = "GET";
    }
    else
    {
        curl_easy_setopt(hCurlHandle, CURLOPT_NOBODY, 1);
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPGET, 0);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADER, 1);
        osVerb = "HEAD";
    }

    /* Bug with older curl versions (<=7.16.4) and FTP. See http://curl.haxx.se/mail/lib-2007-08/0312.html */
    VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    char szCurlErrBuf[CURL_ERROR_SIZE+1];
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    struct curl_slist* headers = GetCurlHeaders(osVerb);
    if( headers != NULL )
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    curl_easy_perform(hCurlHandle);

    if( headers != NULL )
        curl_slist_free_all(headers);

    eExists = EXIST_UNKNOWN;

    if (STARTS_WITH(pszURL, "ftp"))
    {
        if (sWriteFuncData.pBuffer != NULL &&
            STARTS_WITH(sWriteFuncData.pBuffer, "Content-Length: "))
        {
            const char* pszBuffer = sWriteFuncData.pBuffer + strlen("Content-Length: ");
            eExists = EXIST_YES;
            fileSize = CPLScanUIntBig(pszBuffer, static_cast<int>(sWriteFuncData.nSize - strlen("Content-Length: ")));
            if (ENABLE_DEBUG)
                CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB,
                        pszURL, fileSize);
        }
    }

    double dfSize = 0;
    if (eExists != EXIST_YES)
    {
        CURLcode code = curl_easy_getinfo(hCurlHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dfSize );
        if (code == 0)
        {
            eExists = EXIST_YES;
            if (dfSize < 0)
                fileSize = 0;
            else
                fileSize = (GUIntBig)dfSize;
        }
        else
        {
            eExists = EXIST_NO;
            fileSize = 0;
            CPLError(CE_Failure, CPLE_AppDefined, "VSICurlHandle::GetFileSize failed");
        }

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( UseLimitRangeGetInsteadOfHead() && response_code == 206 )
        {
            eExists = EXIST_NO;
            fileSize = 0;
            if( sWriteFuncHeaderData.pBuffer != NULL )
            {
                const char* pszContentRange = strstr((const char*)sWriteFuncHeaderData.pBuffer, "Content-Range: bytes ");
                if( pszContentRange )
                    pszContentRange = strchr(pszContentRange, '/');
                if( pszContentRange )
                {
                    eExists = EXIST_YES;
                    fileSize = (GUIntBig)CPLAtoGIntBig(pszContentRange+1);
                }
            }
        }
        else if( response_code != 200 )
        {
            if( UseLimitRangeGetInsteadOfHead() && sWriteFuncData.pBuffer != NULL &&
                CanRestartOnError((const char*)sWriteFuncData.pBuffer) )
            {
                bHasComputedFileSize = false;
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
                return GetFileSize();
            }

            eExists = EXIST_NO;
            fileSize = 0;
        }
        else if( sWriteFuncData.pBuffer != NULL )
        {
            ProcessGetFileSizeResult( (const char*)sWriteFuncData.pBuffer );
        }

        /* Try to guess if this is a directory. Generally if this is a directory, */
        /* curl will retry with an URL with slash added */
        char *pszEffectiveURL = NULL;
        curl_easy_getinfo(hCurlHandle, CURLINFO_EFFECTIVE_URL, &pszEffectiveURL);
        if (pszEffectiveURL != NULL && strncmp(pszURL, pszEffectiveURL, strlen(pszURL)) == 0 &&
            pszEffectiveURL[strlen(pszURL)] == '/')
        {
            eExists = EXIST_YES;
            fileSize = 0;
            bIsDirectory = true;
        }

        if (ENABLE_DEBUG)
            CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB "  response_code=%d",
                    pszURL, fileSize, (int)response_code);
    }

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    cachedFileProp->bHasComputedFileSize = true;
    cachedFileProp->fileSize = fileSize;
    cachedFileProp->eExists = eExists;
    cachedFileProp->bIsDirectory = bIsDirectory;

    return fileSize;
}

/************************************************************************/
/*                                 Exists()                             */
/************************************************************************/

bool VSICurlHandle::Exists()
{
    if (eExists == EXIST_UNKNOWN)
        GetFileSize();
    return eExists == EXIST_YES;
}

/************************************************************************/
/*                                  Tell()                              */
/************************************************************************/

vsi_l_offset VSICurlHandle::Tell()
{
    return curOffset;
}

/************************************************************************/
/*                          DownloadRegion()                            */
/************************************************************************/

bool VSICurlHandle::DownloadRegion(vsi_l_offset startOffset, int nBlocks)
{
    WriteFuncStruct sWriteFuncData;
    WriteFuncStruct sWriteFuncHeaderData;

    if (bInterrupted && bStopOnInterrruptUntilUninstall)
        return false;

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    if (cachedFileProp->eExists == EXIST_NO)
        return false;

    CURL* hCurlHandle = poFS->GetCurlHandleFor(pszURL);
    VSICurlSetOptions(hCurlHandle, pszURL);

    VSICURLInitWriteFuncStruct(&sWriteFuncData, (VSILFILE*)this, pfnReadCbk, pReadCbkUserData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, VSICurlHandleWriteFunc);
    sWriteFuncHeaderData.bIsHTTP = STARTS_WITH(pszURL, "http");
    sWriteFuncHeaderData.nStartOffset = startOffset;
    sWriteFuncHeaderData.nEndOffset = startOffset + nBlocks * DOWNLOAD_CHUNK_SIZE - 1;
    /* Some servers don't like we try to read after end-of-file (#5786) */
    if( cachedFileProp->bHasComputedFileSize && 
        sWriteFuncHeaderData.nEndOffset >= cachedFileProp->fileSize )
    {
        sWriteFuncHeaderData.nEndOffset = cachedFileProp->fileSize - 1;
    }

    char rangeStr[512];
    snprintf(rangeStr, sizeof(rangeStr),
             CPL_FRMT_GUIB "-" CPL_FRMT_GUIB, startOffset,
            sWriteFuncHeaderData.nEndOffset);

    if (ENABLE_DEBUG)
        CPLDebug("VSICURL", "Downloading %s (%s)...", rangeStr, pszURL);

    curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, rangeStr);

    char szCurlErrBuf[CURL_ERROR_SIZE+1];
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    struct curl_slist* headers = GetCurlHeaders("GET");
    if( headers != NULL )
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    curl_easy_perform(hCurlHandle);

    if( headers != NULL )
        curl_slist_free_all(headers);

    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, NULL);

    if (sWriteFuncData.bInterrupted)
    {
        bInterrupted = true;

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        return false;
    }

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

    char *content_type = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_CONTENT_TYPE, &content_type);

    if (ENABLE_DEBUG)
        CPLDebug("VSICURL", "Got response_code=%ld", response_code);

    if ((response_code != 200 && response_code != 206 &&
         response_code != 225 && response_code != 226 && response_code != 426) || sWriteFuncHeaderData.bError)
    {
        if( sWriteFuncData.pBuffer != NULL &&
            CanRestartOnError((const char*)sWriteFuncData.pBuffer) )
        {
            CPLFree(sWriteFuncData.pBuffer);
            CPLFree(sWriteFuncHeaderData.pBuffer);
            return DownloadRegion(startOffset, nBlocks);
        }

        if (response_code >= 400 && szCurlErrBuf[0] != '\0')
        {
            if (strcmp(szCurlErrBuf, "Couldn't use REST") == 0)
                CPLError(CE_Failure, CPLE_AppDefined, "%d: %s, %s",
                         (int)response_code, szCurlErrBuf,
                         "Range downloading not supported by this server !");
            else
                CPLError(CE_Failure, CPLE_AppDefined, "%d: %s", (int)response_code, szCurlErrBuf);
        }
        if (!bHasComputedFileSize && startOffset == 0)
        {
            cachedFileProp->bHasComputedFileSize = bHasComputedFileSize = true;
            cachedFileProp->fileSize = fileSize = 0;
            cachedFileProp->eExists = eExists = EXIST_NO;
        }
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        return false;
    }

    if (!bHasComputedFileSize && sWriteFuncHeaderData.pBuffer)
    {
        /* Try to retrieve the filesize from the HTTP headers */
        /* if in the form : "Content-Range: bytes x-y/filesize" */
        char* pszContentRange = strstr(sWriteFuncHeaderData.pBuffer, "Content-Range: bytes ");
        if (pszContentRange)
        {
            char* pszEOL = strchr(pszContentRange, '\n');
            if (pszEOL)
            {
                *pszEOL = 0;
                pszEOL = strchr(pszContentRange, '\r');
                if (pszEOL)
                    *pszEOL = 0;
                char* pszSlash = strchr(pszContentRange, '/');
                if (pszSlash)
                {
                    pszSlash ++;
                    fileSize = CPLScanUIntBig(pszSlash, static_cast<int>(strlen(pszSlash)));
                }
            }
        }
        else if (STARTS_WITH(pszURL, "ftp"))
        {
            /* Parse 213 answer for FTP protocol */
            char* pszSize = strstr(sWriteFuncHeaderData.pBuffer, "213 ");
            if (pszSize)
            {
                pszSize += 4;
                char* pszEOL = strchr(pszSize, '\n');
                if (pszEOL)
                {
                    *pszEOL = 0;
                    pszEOL = strchr(pszSize, '\r');
                    if (pszEOL)
                        *pszEOL = 0;

                    fileSize = CPLScanUIntBig(pszSize, static_cast<int>(strlen(pszSize)));
                }
            }
        }

        if (fileSize != 0)
        {
            eExists = EXIST_YES;

            if (ENABLE_DEBUG)
                CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB "  response_code=%d",
                        pszURL, fileSize, (int)response_code);

            bHasComputedFileSize = cachedFileProp->bHasComputedFileSize = true;
            cachedFileProp->fileSize = fileSize;
            cachedFileProp->eExists = eExists;
        }
    }

    lastDownloadedOffset = startOffset + nBlocks * DOWNLOAD_CHUNK_SIZE;

    char* pBuffer = sWriteFuncData.pBuffer;
    size_t nSize = sWriteFuncData.nSize;

    if (nSize > static_cast<size_t>(nBlocks) * DOWNLOAD_CHUNK_SIZE)
    {
        if (ENABLE_DEBUG)
            CPLDebug("VSICURL", "Got more data than expected : %u instead of %d",
                     static_cast<unsigned int>(nSize), nBlocks * DOWNLOAD_CHUNK_SIZE);
    }

    while(nSize > 0)
    {
        //if (ENABLE_DEBUG)
        //    CPLDebug("VSICURL", "Add region %d - %d", startOffset, MIN(DOWNLOAD_CHUNK_SIZE, nSize));
        size_t nChunkSize = MIN((size_t)DOWNLOAD_CHUNK_SIZE, nSize);
        poFS->AddRegion(pszURL, startOffset, nChunkSize, pBuffer);
        startOffset += nChunkSize;
        pBuffer += nChunkSize;
        nSize -= nChunkSize;
    }

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);

    return true;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSICurlHandle::Read( void * const pBufferIn, size_t const  nSize, size_t const  nMemb )
{
    void* pBuffer = pBufferIn;
    size_t nBufferRequestSize = nSize * nMemb;
    if (nBufferRequestSize == 0)
        return 0;

    //CPLDebug("VSICURL", "offset=%d, size=%d", (int)curOffset, (int)nBufferRequestSize);

    vsi_l_offset iterOffset = curOffset;
    while (nBufferRequestSize)
    {
        const CachedRegion* psRegion = poFS->GetRegion(pszURL, iterOffset);
        if (psRegion == NULL)
        {
            vsi_l_offset nOffsetToDownload =
                (iterOffset / DOWNLOAD_CHUNK_SIZE) * DOWNLOAD_CHUNK_SIZE;

            if (nOffsetToDownload == lastDownloadedOffset)
            {
                /* In case of consecutive reads (of small size), we use a */
                /* heuristic that we will read the file sequentially, so */
                /* we double the requested size to decrease the number of */
                /* client/server roundtrips. */
                if (nBlocksToDownload < 100)
                    nBlocksToDownload *= 2;
            }
            else
            {
                /* Random reads. Cancel the above heuristics */
                nBlocksToDownload = 1;
            }

            /* Ensure that we will request at least the number of blocks */
            /* to satisfy the remaining buffer size to read */
            vsi_l_offset nEndOffsetToDownload =
                ((iterOffset + nBufferRequestSize) / DOWNLOAD_CHUNK_SIZE) * DOWNLOAD_CHUNK_SIZE;
            int nMinBlocksToDownload = 1 + (int)
                ((nEndOffsetToDownload - nOffsetToDownload) / DOWNLOAD_CHUNK_SIZE);
            if (nBlocksToDownload < nMinBlocksToDownload)
                nBlocksToDownload = nMinBlocksToDownload;

            int i;
            /* Avoid reading already cached data */
            for(i=1;i<nBlocksToDownload;i++)
            {
                if (poFS->GetRegion(pszURL, nOffsetToDownload + i * DOWNLOAD_CHUNK_SIZE) != NULL)
                {
                    nBlocksToDownload = i;
                    break;
                }
            }

            if( nBlocksToDownload > N_MAX_REGIONS )
                nBlocksToDownload = N_MAX_REGIONS;

            if (DownloadRegion(nOffsetToDownload, nBlocksToDownload) == false)
            {
                if (!bInterrupted)
                    bEOF = true;
                return 0;
            }
            psRegion = poFS->GetRegion(pszURL, iterOffset);
        }
        if (psRegion == NULL || psRegion->pData == NULL)
        {
            bEOF = true;
            return 0;
        }
        int nToCopy = (int) MIN(nBufferRequestSize, psRegion->nSize - (iterOffset - psRegion->nFileOffsetStart));
        memcpy(pBuffer, psRegion->pData + iterOffset - psRegion->nFileOffsetStart,
                nToCopy);
        pBuffer = (char*) pBuffer + nToCopy;
        iterOffset += nToCopy;
        nBufferRequestSize -= nToCopy;
        if (psRegion->nSize != (size_t)DOWNLOAD_CHUNK_SIZE && nBufferRequestSize != 0)
        {
            break;
        }
    }

    size_t ret = (size_t) ((iterOffset - curOffset) / nSize);
    if (ret != nMemb)
        bEOF = true;

    curOffset = iterOffset;

    return ret;
}


/************************************************************************/
/*                           ReadMultiRange()                           */
/************************************************************************/

int VSICurlHandle::ReadMultiRange( int const nRanges, void ** const ppData,
                                   const vsi_l_offset* const panOffsets,
                                   const size_t* const panSizes )
{
    WriteFuncStruct sWriteFuncData;
    WriteFuncStruct sWriteFuncHeaderData;

    if (bInterrupted && bStopOnInterrruptUntilUninstall)
        return FALSE;

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    if (cachedFileProp->eExists == EXIST_NO)
        return -1;

    CPLString osRanges, osFirstRange, osLastRange;
    int i;
    int nMergedRanges = 0;
    vsi_l_offset nTotalReqSize = 0;
    for(i=0;i<nRanges;i++)
    {
        CPLString osCurRange;
        if (i != 0)
            osRanges.append(",");
        osCurRange = CPLSPrintf(CPL_FRMT_GUIB "-", panOffsets[i]);
        while (i + 1 < nRanges && panOffsets[i] + panSizes[i] == panOffsets[i+1])
        {
            nTotalReqSize += panSizes[i];
            i ++;
        }
        nTotalReqSize += panSizes[i];
        osCurRange.append(CPLSPrintf(CPL_FRMT_GUIB, panOffsets[i] + panSizes[i]-1));
        nMergedRanges ++;

        osRanges += osCurRange;

        if (nMergedRanges == 1)
            osFirstRange = osCurRange;
        osLastRange = osCurRange;
    }

    const char* pszMaxRanges = CPLGetConfigOption("CPL_VSIL_CURL_MAX_RANGES", "250");
    int nMaxRanges = atoi(pszMaxRanges);
    if (nMaxRanges <= 0)
        nMaxRanges = 250;
    if (nMergedRanges > nMaxRanges)
    {
        int nHalf = nRanges / 2;
        int nRet = ReadMultiRange(nHalf, ppData, panOffsets, panSizes);
        if (nRet != 0)
            return nRet;
        return ReadMultiRange(nRanges - nHalf, ppData + nHalf, panOffsets + nHalf, panSizes + nHalf);
    }

    CURL* hCurlHandle = poFS->GetCurlHandleFor(pszURL);
    VSICurlSetOptions(hCurlHandle, pszURL);

    VSICURLInitWriteFuncStruct(&sWriteFuncData, (VSILFILE*)this, pfnReadCbk, pReadCbkUserData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, VSICurlHandleWriteFunc);
    sWriteFuncHeaderData.bIsHTTP = STARTS_WITH(pszURL, "http");
    sWriteFuncHeaderData.bMultiRange = nMergedRanges > 1;
    if (nMergedRanges == 1)
    {
        sWriteFuncHeaderData.nStartOffset = panOffsets[0];
        sWriteFuncHeaderData.nEndOffset = panOffsets[0] + nTotalReqSize-1;
    }

    if (ENABLE_DEBUG)
    {
        if (nMergedRanges == 1)
            CPLDebug("VSICURL", "Downloading %s (%s)...", osRanges.c_str(), pszURL);
        else
            CPLDebug("VSICURL", "Downloading %s, ..., %s (" CPL_FRMT_GUIB " bytes, %s)...",
                     osFirstRange.c_str(), osLastRange.c_str(), (GUIntBig)nTotalReqSize, pszURL);
    }

    curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, osRanges.c_str());

    char szCurlErrBuf[CURL_ERROR_SIZE+1];
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    struct curl_slist* headers = GetCurlHeaders("GET");
    if( headers != NULL )
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    curl_easy_perform(hCurlHandle);

    if( headers != NULL )
        curl_slist_free_all(headers);

    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, NULL);

    if (sWriteFuncData.bInterrupted)
    {
        bInterrupted = true;

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        return -1;
    }

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

    char *content_type = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_CONTENT_TYPE, &content_type);

    if ((response_code != 200 && response_code != 206 &&
         response_code != 225 && response_code != 226 && response_code != 426) || sWriteFuncHeaderData.bError)
    {
        if (response_code >= 400 && szCurlErrBuf[0] != '\0')
        {
            if (strcmp(szCurlErrBuf, "Couldn't use REST") == 0)
                CPLError(CE_Failure, CPLE_AppDefined, "%d: %s, %s",
                         (int)response_code, szCurlErrBuf,
                         "Range downloading not supported by this server !");
            else
                CPLError(CE_Failure, CPLE_AppDefined, "%d: %s", (int)response_code, szCurlErrBuf);
        }
        /*
        if (!bHasComputedFileSize && startOffset == 0)
        {
            cachedFileProp->bHasComputedFileSize = bHasComputedFileSize = true;
            cachedFileProp->fileSize = fileSize = 0;
            cachedFileProp->eExists = eExists = EXIST_NO;
        }
        */
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        return -1;
    }

    char* pBuffer = sWriteFuncData.pBuffer;
    size_t nSize = sWriteFuncData.nSize;

    int nRet = -1;
    char* pszBoundary;
    CPLString osBoundary;
    char *pszNext;
    int iRange = 0;
    int iPart = 0;
    char* pszEOL;

/* -------------------------------------------------------------------- */
/*      No multipart if a single range has been requested               */
/* -------------------------------------------------------------------- */

    if (nMergedRanges == 1)
    {
        size_t nAccSize = 0;
        if ((vsi_l_offset)nSize < nTotalReqSize)
            goto end;

        for(i=0;i<nRanges;i++)
        {
            memcpy(ppData[i], pBuffer + nAccSize, panSizes[i]);
            nAccSize += panSizes[i];
        }

        nRet = 0;
        goto end;
    }

/* -------------------------------------------------------------------- */
/*      Extract boundary name                                           */
/* -------------------------------------------------------------------- */

    pszBoundary = strstr(sWriteFuncHeaderData.pBuffer,
                         "Content-Type: multipart/byteranges; boundary=");
    if( pszBoundary == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Could not find '%s'",
                  "Content-Type: multipart/byteranges; boundary=" );
        goto end;
    }

    pszBoundary += strlen( "Content-Type: multipart/byteranges; boundary=" );

    pszEOL = strchr(pszBoundary, '\r');
    if (pszEOL)
        *pszEOL = 0;
    pszEOL = strchr(pszBoundary, '\n');
    if (pszEOL)
        *pszEOL = 0;

    /* Remove optional double-quote character around boundary name */
    if (pszBoundary[0] == '"')
    {
        pszBoundary ++;
        char* pszLastDoubleQuote = strrchr(pszBoundary, '"');
        if (pszLastDoubleQuote)
            *pszLastDoubleQuote = 0;
    }

    osBoundary = "--";
    osBoundary += pszBoundary;

/* -------------------------------------------------------------------- */
/*      Find the start of the first chunk.                              */
/* -------------------------------------------------------------------- */
    pszNext = strstr(pBuffer,osBoundary.c_str());
    if( pszNext == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No parts found." );
        goto end;
    }

    pszNext += strlen(osBoundary);
    while( *pszNext != '\n' && *pszNext != '\r' && *pszNext != '\0' )
        pszNext++;
    if( *pszNext == '\r' )
        pszNext++;
    if( *pszNext == '\n' )
        pszNext++;

/* -------------------------------------------------------------------- */
/*      Loop over parts...                                              */
/* -------------------------------------------------------------------- */
    while( iPart < nRanges )
    {
/* -------------------------------------------------------------------- */
/*      Collect headers.                                                */
/* -------------------------------------------------------------------- */
        bool bExpectedRange = false;

        while( *pszNext != '\n' && *pszNext != '\r' && *pszNext != '\0' )
        {
            pszEOL = strstr(pszNext,"\n");

            if( pszEOL == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Error while parsing multipart content (at line %d)", __LINE__);
                goto end;
            }

            *pszEOL = '\0';
            bool bRestoreAntislashR = false;
            if (pszEOL - pszNext > 1 && pszEOL[-1] == '\r')
            {
                bRestoreAntislashR = true;
                pszEOL[-1] = '\0';
            }

            if (STARTS_WITH_CI(pszNext, "Content-Range: bytes "))
            {
                bExpectedRange = true; /* FIXME */
            }

            if (bRestoreAntislashR)
                pszEOL[-1] = '\r';
            *pszEOL = '\n';

            pszNext = pszEOL + 1;
        }

        if (!bExpectedRange)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Error while parsing multipart content (at line %d)", __LINE__);
            goto end;
        }

        if( *pszNext == '\r' )
            pszNext++;
        if( *pszNext == '\n' )
            pszNext++;

/* -------------------------------------------------------------------- */
/*      Work out the data block size.                                   */
/* -------------------------------------------------------------------- */
        size_t nBytesAvail = nSize - (pszNext - pBuffer);

        while(true)
        {
            if (nBytesAvail < panSizes[iRange])
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Error while parsing multipart content (at line %d)", __LINE__);
                goto end;
            }

            memcpy(ppData[iRange], pszNext, panSizes[iRange]);
            pszNext += panSizes[iRange];
            nBytesAvail -= panSizes[iRange];
            if( iRange + 1 < nRanges &&
                panOffsets[iRange] + panSizes[iRange] == panOffsets[iRange + 1] )
            {
                iRange++;
            }
            else
                break;
        }

        iPart ++;
        iRange ++;

        while( nBytesAvail > 0
               && (*pszNext != '-'
                   || strncmp(pszNext,osBoundary,strlen(osBoundary)) != 0) )
        {
            pszNext++;
            nBytesAvail--;
        }

        if( nBytesAvail == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Error while parsing multipart content (at line %d)", __LINE__);
            goto end;
        }

        pszNext += strlen(osBoundary);
        if( STARTS_WITH(pszNext, "--") )
        {
            /* End of multipart */
            break;
        }

        if( *pszNext == '\r' )
            pszNext++;
        if( *pszNext == '\n' )
            pszNext++;
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Error while parsing multipart content (at line %d)", __LINE__);
            goto end;
        }
    }

    if (iPart == nMergedRanges)
        nRet = 0;
    else
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Got only %d parts, where %d were expected", iPart, nMergedRanges);

end:
    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);

    return nRet;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSICurlHandle::Write( CPL_UNUSED const void *pBuffer,
                             CPL_UNUSED size_t nSize,
                             CPL_UNUSED size_t nMemb )
{
    return 0;
}

/************************************************************************/
/*                                 Eof()                                */
/************************************************************************/


int       VSICurlHandle::Eof()
{
    return bEOF;
}

/************************************************************************/
/*                                 Flush()                              */
/************************************************************************/

int       VSICurlHandle::Flush()
{
    return 0;
}

/************************************************************************/
/*                                  Close()                             */
/************************************************************************/

int       VSICurlHandle::Close()
{
    return 0;
}




/************************************************************************/
/*                   VSICurlFilesystemHandler()                         */
/************************************************************************/

VSICurlFilesystemHandler::VSICurlFilesystemHandler()
{
    hMutex = NULL;
    papsRegions = NULL;
    nRegions = 0;
    bUseCacheDisk = CSLTestBoolean(CPLGetConfigOption("CPL_VSIL_CURL_USE_CACHE", "NO"));
}

/************************************************************************/
/*                  ~VSICurlFilesystemHandler()                         */
/************************************************************************/

VSICurlFilesystemHandler::~VSICurlFilesystemHandler()
{
    int i;
    for(i=0;i<nRegions;i++)
    {
        CPLFree(papsRegions[i]->pData);
        CPLFree(papsRegions[i]);
    }
    CPLFree(papsRegions);

    std::map<CPLString, CachedFileProp*>::const_iterator iterCacheFileSize;

    for( iterCacheFileSize = cacheFileSize.begin(); iterCacheFileSize != cacheFileSize.end(); iterCacheFileSize++ )
    {
        CPLFree(iterCacheFileSize->second);
    }

    std::map<CPLString, CachedDirList*>::const_iterator iterCacheDirList;

    for( iterCacheDirList = cacheDirList.begin(); iterCacheDirList != cacheDirList.end(); iterCacheDirList++ )
    {
        CSLDestroy(iterCacheDirList->second->papszFileList);
        CPLFree(iterCacheDirList->second);
    }

    std::map<GIntBig, CachedConnection*>::const_iterator iterConnections;
    for( iterConnections = mapConnections.begin(); iterConnections != mapConnections.end(); iterConnections++ )
    {
        curl_easy_cleanup(iterConnections->second->hCurlHandle);
        delete iterConnections->second;
    }

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
}

/************************************************************************/
/*                      GetCurlHandleFor()                              */
/************************************************************************/

CURL* VSICurlFilesystemHandler::GetCurlHandleFor(CPLString osURL)
{
    CPLMutexHolder oHolder( &hMutex );

    std::map<GIntBig, CachedConnection*>::const_iterator iterConnections;

    iterConnections = mapConnections.find(CPLGetPID());
    if (iterConnections == mapConnections.end())
    {
        CURL* hCurlHandle = curl_easy_init();
        CachedConnection* psCachedConnection = new CachedConnection;
        psCachedConnection->osURL = osURL;
        psCachedConnection->hCurlHandle = hCurlHandle;
        mapConnections[CPLGetPID()] = psCachedConnection;
        return hCurlHandle;
    }
    else
    {
        CachedConnection* psCachedConnection = iterConnections->second;
        if (osURL == psCachedConnection->osURL)
            return psCachedConnection->hCurlHandle;

        const char* pszURL = osURL.c_str();
        const char* pszEndOfServ = strchr(pszURL, '.');
        if (pszEndOfServ != NULL)
            pszEndOfServ = strchr(pszEndOfServ, '/');
        if (pszEndOfServ == NULL)
            pszURL = pszURL + strlen(pszURL);
        bool bReinitConnection = strncmp(psCachedConnection->osURL,
                                        pszURL, pszEndOfServ-pszURL) != 0;

        if (bReinitConnection)
        {
            if (psCachedConnection->hCurlHandle)
                curl_easy_cleanup(psCachedConnection->hCurlHandle);
            psCachedConnection->hCurlHandle = curl_easy_init();
        }
        psCachedConnection->osURL = osURL;

        return psCachedConnection->hCurlHandle;
    }
}


/************************************************************************/
/*                   GetRegionFromCacheDisk()                           */
/************************************************************************/

const CachedRegion* 
VSICurlFilesystemHandler::GetRegionFromCacheDisk(const char* pszURL,
                                                 vsi_l_offset nFileOffsetStart)
{
    nFileOffsetStart = (nFileOffsetStart / DOWNLOAD_CHUNK_SIZE) * DOWNLOAD_CHUNK_SIZE;
    VSILFILE* fp = VSIFOpenL(VSICurlGetCacheFileName(), "rb");
    if (fp)
    {
        unsigned long   pszURLHash = CPLHashSetHashStr(pszURL);
        unsigned long   pszURLHashCached;
        vsi_l_offset    nFileOffsetStartCached;
        size_t          nSizeCached;
        while(true)
        {
            if (VSIFReadL(&pszURLHashCached, sizeof(unsigned long), 1, fp) == 0)
                break;
            if( VSIFReadL(&nFileOffsetStartCached, sizeof(vsi_l_offset), 1, fp) == 0)
                break;
            if( VSIFReadL(&nSizeCached, sizeof(size_t), 1, fp) == 0)
                break;
            if (pszURLHash == pszURLHashCached &&
                nFileOffsetStart == nFileOffsetStartCached)
            {
                if (ENABLE_DEBUG)
                    CPLDebug("VSICURL", "Got data at offset " CPL_FRMT_GUIB " from disk" , nFileOffsetStart);
                if (nSizeCached)
                {
                    char* pBuffer = (char*) CPLMalloc(nSizeCached);
                    if( VSIFReadL(pBuffer, 1, nSizeCached, fp) != nSizeCached )
                    {
                        CPLFree(pBuffer);
                        break;
                    }
                    AddRegion(pszURL, nFileOffsetStart, nSizeCached, pBuffer);
                    CPLFree(pBuffer);
                }
                else
                {
                    AddRegion(pszURL, nFileOffsetStart, 0, NULL);
                }
                VSIFCloseL(fp);
                return GetRegion(pszURL, nFileOffsetStart);
            }
            else
            {
                if( VSIFSeekL(fp, nSizeCached, SEEK_CUR) != 0 )
                    break;
            }
        }
        VSIFCloseL(fp);
    }
    return NULL;
}


/************************************************************************/
/*                  AddRegionToCacheDisk()                                */
/************************************************************************/

void VSICurlFilesystemHandler::AddRegionToCacheDisk(CachedRegion* psRegion)
{
    VSILFILE* fp = VSIFOpenL(VSICurlGetCacheFileName(), "r+b");
    if (fp)
    {
        unsigned long   pszURLHashCached;
        vsi_l_offset    nFileOffsetStartCached;
        size_t          nSizeCached;
        while(true)
        {
            if (VSIFReadL(&pszURLHashCached, 1, sizeof(unsigned long), fp) == 0)
                break;
            if( VSIFReadL(&nFileOffsetStartCached, sizeof(vsi_l_offset), 1, fp) == 0 )
                break;
            if( VSIFReadL(&nSizeCached, sizeof(size_t), 1, fp) == 0 )
                break;
            if (psRegion->pszURLHash == pszURLHashCached &&
                psRegion->nFileOffsetStart == nFileOffsetStartCached)
            {
                CPLAssert(psRegion->nSize == nSizeCached);
                VSIFCloseL(fp);
                return;
            }
            else
            {
                if( VSIFSeekL(fp, nSizeCached, SEEK_CUR) != 0 )
                    break;
            }
        }
    }
    else
    {
        fp = VSIFOpenL(VSICurlGetCacheFileName(), "wb");
    }
    if (fp)
    {
        if (ENABLE_DEBUG)
             CPLDebug("VSICURL", "Write data at offset " CPL_FRMT_GUIB " to disk" , psRegion->nFileOffsetStart);
        CPL_IGNORE_RET_VAL(VSIFWriteL(&psRegion->pszURLHash, 1, sizeof(unsigned long), fp));
        CPL_IGNORE_RET_VAL(VSIFWriteL(&psRegion->nFileOffsetStart, 1, sizeof(vsi_l_offset), fp));
        CPL_IGNORE_RET_VAL(VSIFWriteL(&psRegion->nSize, 1, sizeof(size_t), fp));
        if (psRegion->nSize)
            CPL_IGNORE_RET_VAL(VSIFWriteL(psRegion->pData, 1, psRegion->nSize, fp));

        VSIFCloseL(fp);
    }
    return;
}


/************************************************************************/
/*                          GetRegion()                                 */
/************************************************************************/

const CachedRegion* VSICurlFilesystemHandler::GetRegion(const char* pszURL,
                                                        vsi_l_offset nFileOffsetStart)
{
    CPLMutexHolder oHolder( &hMutex );

    unsigned long   pszURLHash = CPLHashSetHashStr(pszURL);

    nFileOffsetStart = (nFileOffsetStart / DOWNLOAD_CHUNK_SIZE) * DOWNLOAD_CHUNK_SIZE;
    int i;
    for(i=0;i<nRegions;i++)
    {
        CachedRegion* psRegion = papsRegions[i];
        if (psRegion->pszURLHash == pszURLHash &&
            nFileOffsetStart == psRegion->nFileOffsetStart)
        {
            memmove(papsRegions + 1, papsRegions, i * sizeof(CachedRegion*));
            papsRegions[0] = psRegion;
            return psRegion;
        }
    }
    if (bUseCacheDisk)
        return GetRegionFromCacheDisk(pszURL, nFileOffsetStart);
    return NULL;
}

/************************************************************************/
/*                          AddRegion()                                 */
/************************************************************************/

void  VSICurlFilesystemHandler::AddRegion(const char* pszURL,
                                          vsi_l_offset    nFileOffsetStart,
                                          size_t          nSize,
                                          const char     *pData)
{
    CPLMutexHolder oHolder( &hMutex );

    unsigned long   pszURLHash = CPLHashSetHashStr(pszURL);

    CachedRegion* psRegion;
    if (nRegions == N_MAX_REGIONS)
    {
        psRegion = papsRegions[N_MAX_REGIONS-1];
        memmove(papsRegions + 1, papsRegions, (N_MAX_REGIONS-1) * sizeof(CachedRegion*));
        papsRegions[0] = psRegion;
        CPLFree(psRegion->pData);
    }
    else
    {
        papsRegions = (CachedRegion**) CPLRealloc(papsRegions, (nRegions + 1) * sizeof(CachedRegion*));
        if (nRegions)
            memmove(papsRegions + 1, papsRegions, nRegions * sizeof(CachedRegion*));
        nRegions ++;
        papsRegions[0] = psRegion = (CachedRegion*) CPLMalloc(sizeof(CachedRegion));
    }

    psRegion->pszURLHash = pszURLHash;
    psRegion->nFileOffsetStart = nFileOffsetStart;
    psRegion->nSize = nSize;
    psRegion->pData = (nSize) ? (char*) CPLMalloc(nSize) : NULL;
    if (nSize)
        memcpy(psRegion->pData, pData, nSize);

    if (bUseCacheDisk)
        AddRegionToCacheDisk(psRegion);
}

/************************************************************************/
/*                         GetCachedFileProp()                          */
/************************************************************************/

CachedFileProp*  VSICurlFilesystemHandler::GetCachedFileProp(const char* pszURL)
{
    CPLMutexHolder oHolder( &hMutex );

    CachedFileProp* cachedFileProp = cacheFileSize[pszURL];
    if (cachedFileProp == NULL)
    {
        cachedFileProp = (CachedFileProp*) CPLMalloc(sizeof(CachedFileProp));
        cachedFileProp->eExists = EXIST_UNKNOWN;
        cachedFileProp->bHasComputedFileSize = false;
        cachedFileProp->fileSize = 0;
        cachedFileProp->bIsDirectory = false;
        cacheFileSize[pszURL] = cachedFileProp;
    }

    return cachedFileProp;
}

/************************************************************************/
/*                    InvalidateCachedFileProp()                        */
/************************************************************************/

void VSICurlFilesystemHandler::InvalidateCachedFileProp(const char* pszURL)
{
    CPLMutexHolder oHolder( &hMutex );

    std::map<CPLString, CachedFileProp*>::iterator oIter = cacheFileSize.find(pszURL);
    if( oIter != cacheFileSize.end() )
    {
        CPLFree(oIter->second);
        cacheFileSize.erase(oIter);
    }
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSICurlFilesystemHandler::CreateFileHandle(const char* pszURL)
{
    return new VSICurlHandle(this, pszURL);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSICurlFilesystemHandler::Open( const char *pszFilename,
                                                  const char *pszAccess)
{
    if (strchr(pszAccess, 'w') != NULL ||
        strchr(pszAccess, '+') != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only read-only mode is supported for /vsicurl");
        return NULL;
    }

    const char* pszOptionVal =
        CPLGetConfigOption( "GDAL_DISABLE_READDIR_ON_OPEN", "NO" );
    const bool bSkipReadDir = EQUAL(pszOptionVal, "EMPTY_DIR") ||
                              CSLTestBoolean(pszOptionVal);

    CPLString osFilename(pszFilename);
    bool bGotFileList = true;
    if (strchr(CPLGetFilename(osFilename), '.') != NULL &&
        !STARTS_WITH(CPLGetExtension(osFilename), "zip") && !bSkipReadDir)
    {
        char** papszFileList = ReadDirInternal(CPLGetDirname(osFilename), 0, &bGotFileList);
        const bool bFound = (VSICurlIsFileInList(papszFileList, CPLGetFilename(osFilename)) != -1);
        CSLDestroy(papszFileList);
        if (bGotFileList && !bFound)
        {
            return NULL;
        }
    }

    VSICurlHandle* poHandle = CreateFileHandle( osFilename + strlen(GetFSPrefix()));
    if( poHandle == NULL )
        return NULL;
    if (!bGotFileList)
    {
        /* If we didn't get a filelist, check that the file really exists */
        if (!poHandle->Exists())
        {
            delete poHandle;
            return NULL;
        }
    }

    if( CSLTestBoolean( CPLGetConfigOption( "VSI_CACHE", "FALSE" ) ) )
        return VSICreateCachedFile( poHandle );
    else
        return poHandle;
}

/************************************************************************/
/*                        VSICurlParserFindEOL()                        */
/*                                                                      */
/*      Small helper function for VSICurlPaseHTMLFileList() to find     */
/*      the end of a line in the directory listing.  Either a <br>      */
/*      or newline.                                                     */
/************************************************************************/

static char *VSICurlParserFindEOL( char *pszData )

{
    while( *pszData != '\0' && *pszData != '\n' && !STARTS_WITH_CI(pszData, "<br>") )
        pszData++;

    if( *pszData == '\0' )
        return NULL;
    else 
        return pszData;
}


/************************************************************************/
/*                   VSICurlParseHTMLDateTimeFileSize()                 */
/************************************************************************/

static const char* const apszMonths[] = { "January", "February", "March",
                                          "April", "May", "June", "July",
                                          "August", "September", "October",
                                          "November", "December" };

static bool VSICurlParseHTMLDateTimeFileSize(const char* pszStr,
                                            struct tm& brokendowntime,
                                            GUIntBig& nFileSize,
                                            GIntBig& mTime)
{
    for(int iMonth=0;iMonth<12;iMonth++)
    {
        char szMonth[32];
        szMonth[0] = '-';
        memcpy(szMonth + 1, apszMonths[iMonth], 3);
        szMonth[4] = '-';
        szMonth[5] = '\0';
        const char* pszMonthFound = strstr(pszStr, szMonth);
        if (pszMonthFound)
        {
            /* Format of Apache, like in http://download.osgeo.org/gdal/data/gtiff/ */
            /* "17-May-2010 12:26" */
            if (pszMonthFound - pszStr > 2 && strlen(pszMonthFound) > 15 &&
                pszMonthFound[-2 + 11] == ' ' && pszMonthFound[-2 + 14] == ':')
            {
                pszMonthFound -= 2;
                int nDay = atoi(pszMonthFound);
                int nYear = atoi(pszMonthFound + 7);
                int nHour = atoi(pszMonthFound + 12);
                int nMin = atoi(pszMonthFound + 15);
                if (nDay >= 1 && nDay <= 31 && nYear >= 1900 &&
                    nHour >= 0 && nHour <= 24 && nMin >= 0 && nMin < 60)
                {
                    brokendowntime.tm_year = nYear - 1900;
                    brokendowntime.tm_mon = iMonth;
                    brokendowntime.tm_mday = nDay;
                    brokendowntime.tm_hour = nHour;
                    brokendowntime.tm_min = nMin;
                    mTime = CPLYMDHMSToUnixTime(&brokendowntime);

                    return true;
                }
            }
            return false;
        }

        /* Microsoft IIS */
        szMonth[0] = ' ';
        strcpy(szMonth + 1, apszMonths[iMonth]);
        strcat(szMonth, " ");
        pszMonthFound = strstr(pszStr, szMonth);
        if (pszMonthFound)
        {
            int nLenMonth = static_cast<int>(strlen(apszMonths[iMonth]));
            if (pszMonthFound - pszStr > 2 &&
                pszMonthFound[-1] != ',' &&
                pszMonthFound[-2] != ' ' &&
                (int)strlen(pszMonthFound-2) > 2 + 1 + nLenMonth + 1 + 4 + 1 + 5 + 1 + 4)
            {
                /* Format of http://ortho.linz.govt.nz/tifs/1994_95/ */
                /* "        Friday, 21 April 2006 12:05 p.m.     48062343 m35a_fy_94_95.tif" */
                pszMonthFound -= 2;
                    int nDay = atoi(pszMonthFound);
                int nCurOffset = 2 + 1 + nLenMonth + 1;
                int nYear = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 4 + 1;
                int nHour = atoi(pszMonthFound + nCurOffset);
                if (nHour < 10)
                    nCurOffset += 1 + 1;
                else
                    nCurOffset += 2 + 1;
                int nMin = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 2 + 1;
                if (STARTS_WITH(pszMonthFound + nCurOffset, "p.m."))
                    nHour += 12;
                else if (!STARTS_WITH(pszMonthFound + nCurOffset, "a.m."))
                    nHour = -1;
                nCurOffset += 4;

                const char* pszFilesize = pszMonthFound + nCurOffset;
                while(*pszFilesize == ' ')
                    pszFilesize ++;
                if (*pszFilesize >= '1' && *pszFilesize <= '9')
                    nFileSize = CPLScanUIntBig(pszFilesize, static_cast<int>(strlen(pszFilesize)));

                if (nDay >= 1 && nDay <= 31 && nYear >= 1900 &&
                    nHour >= 0 && nHour <= 24 && nMin >= 0 && nMin < 60)
                {
                    brokendowntime.tm_year = nYear - 1900;
                    brokendowntime.tm_mon = iMonth;
                    brokendowntime.tm_mday = nDay;
                    brokendowntime.tm_hour = nHour;
                    brokendowntime.tm_min = nMin;
                    mTime = CPLYMDHMSToUnixTime(&brokendowntime);

                    return true;
                }
                nFileSize = 0;
            }
            else if (pszMonthFound - pszStr > 1 &&
                        pszMonthFound[-1] == ',' &&
                        (int)strlen(pszMonthFound) > 1 + nLenMonth + 1 + 2 + 1 + 1 + 4 + 1 + 5 + 1 + 2)
            {
                /* Format of http://publicfiles.dep.state.fl.us/dear/BWR_GIS/2007NWFLULC/ */
                /* "        Sunday, June 20, 2010  6:46 PM    233170905 NWF2007LULCForSDE.zip" */
                pszMonthFound += 1;
                int nCurOffset = nLenMonth + 1;
                int nDay = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 2 + 1 + 1;
                int nYear = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 4 + 1;
                int nHour = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 2 + 1;
                int nMin = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 2 + 1;
                if (STARTS_WITH(pszMonthFound + nCurOffset, "PM"))
                    nHour += 12;
                else if (!STARTS_WITH(pszMonthFound + nCurOffset, "AM"))
                    nHour = -1;
                nCurOffset += 2;

                const char* pszFilesize = pszMonthFound + nCurOffset;
                while(*pszFilesize == ' ')
                    pszFilesize ++;
                if (*pszFilesize >= '1' && *pszFilesize <= '9')
                    nFileSize = CPLScanUIntBig(pszFilesize, static_cast<int>(strlen(pszFilesize)));

                if (nDay >= 1 && nDay <= 31 && nYear >= 1900 &&
                    nHour >= 0 && nHour <= 24 && nMin >= 0 && nMin < 60)
                {
                    brokendowntime.tm_year = nYear - 1900;
                    brokendowntime.tm_mon = iMonth;
                    brokendowntime.tm_mday = nDay;
                    brokendowntime.tm_hour = nHour;
                    brokendowntime.tm_min = nMin;
                    mTime = CPLYMDHMSToUnixTime(&brokendowntime);

                    return true;
                }
                nFileSize = 0;
            }
            return false;
        }
    }

    return false;
}

/************************************************************************/
/*                          ParseHTMLFileList()                         */
/*                                                                      */
/*      Parse a file list document and return all the components.       */
/************************************************************************/

char** VSICurlFilesystemHandler::ParseHTMLFileList(const char* pszFilename,
                                                   int nMaxFiles,
                                                   char* pszData,
                                                   bool* pbGotFileList)
{
    *pbGotFileList = false;

    const char* pszDir;
    if (STARTS_WITH_CI(pszFilename, "/vsicurl/http://"))
        pszDir = strchr(pszFilename + strlen("/vsicurl/http://"), '/');
    else if (STARTS_WITH_CI(pszFilename, "/vsicurl/https://"))
        pszDir = strchr(pszFilename + strlen("/vsicurl/https://"), '/');
    else
        pszDir = strchr(pszFilename + strlen("/vsicurl/ftp://"), '/');
    if (pszDir == NULL)
        pszDir = "";

    /* Apache */
    CPLString osExpectedString = "<title>Index of ";
    osExpectedString += pszDir;
    osExpectedString += "</title>";
    /* shttpd */
    CPLString osExpectedString2 = "<title>Index of ";
    osExpectedString2 += pszDir;
    osExpectedString2 += "/</title>";
    /* FTP */
    CPLString osExpectedString3 = "FTP Listing of ";
    osExpectedString3 += pszDir;
    osExpectedString3 += "/";
    /* Apache 1.3.33 */
    CPLString osExpectedString4 = "<TITLE>Index of ";
    osExpectedString4 += pszDir;
    osExpectedString4 += "</TITLE>";

    /* The listing of http://dds.cr.usgs.gov/srtm/SRTM_image_sample/picture%20examples/ */
    /* has "<title>Index of /srtm/SRTM_image_sample/picture examples</title>" so we must */
    /* try unescaped %20 also */
    /* Similar with http://datalib.usask.ca/gis/Data/Central_America_goodbutdoweown%3f/ */
    CPLString osExpectedString_unescaped;
    if (strchr(pszDir, '%'))
    {
        char* pszUnescapedDir = CPLUnescapeString(pszDir, NULL, CPLES_URL);
        osExpectedString_unescaped = "<title>Index of ";
        osExpectedString_unescaped += pszUnescapedDir;
        osExpectedString_unescaped += "</title>";
        CPLFree(pszUnescapedDir);
    }

    char* c;
    int nCount = 0;
    int nCountTable = 0;
    CPLStringList oFileList;
    char* pszLine = pszData;
    bool bIsHTMLDirList = false;

    while( (c = VSICurlParserFindEOL( pszLine )) != NULL )
    {
        *c = 0;

        /* To avoid false positive on pages such as http://www.ngs.noaa.gov/PC_PROD/USGG2009BETA */
        /* This is a heuristics, but normal HTML listing of files have not more than one table */
        if (strstr(pszLine, "<table"))
        {
            nCountTable ++;
            if (nCountTable == 2)
            {
                *pbGotFileList = false;
                return NULL;
            }
        }

        if (!bIsHTMLDirList &&
            (strstr(pszLine, osExpectedString.c_str()) ||
             strstr(pszLine, osExpectedString2.c_str()) ||
             strstr(pszLine, osExpectedString3.c_str()) ||
             strstr(pszLine, osExpectedString4.c_str()) ||
             (osExpectedString_unescaped.size() != 0 && strstr(pszLine, osExpectedString_unescaped.c_str()))))
        {
            bIsHTMLDirList = true;
            *pbGotFileList = true;
        }
        /* Subversion HTTP listing */
        /* or Microsoft-IIS/6.0 listing (e.g. http://ortho.linz.govt.nz/tifs/2005_06/) */
        else if (!bIsHTMLDirList && strstr(pszLine, "<title>"))
        {
            /* Detect something like : <html><head><title>gdal - Revision 20739: /trunk/autotest/gcore/data</title></head> */
            /* The annoying thing is that what is after ': ' is a subpart of what is after http://server/ */
            char* pszSubDir = strstr(pszLine, ": ");
            if (pszSubDir == NULL)
                /* or <title>ortho.linz.govt.nz - /tifs/2005_06/</title> */
                pszSubDir = strstr(pszLine, "- ");
            if (pszSubDir)
            {
                pszSubDir += 2;
                char* pszTmp = strstr(pszSubDir, "</title>");
                if (pszTmp)
                {
                    if (pszTmp[-1] == '/')
                        pszTmp[-1] = 0;
                    else
                        *pszTmp = 0;
                    if (strstr(pszDir, pszSubDir))
                    {
                        bIsHTMLDirList = true;
                        *pbGotFileList = true;
                    }
                }
            }
        }
        else if (bIsHTMLDirList &&
                 (strstr(pszLine, "<a href=\"") != NULL || strstr(pszLine, "<A HREF=\"") != NULL) &&
                 strstr(pszLine, "<a href=\"http://") == NULL && /* exclude absolute links, like to subversion home */
                 strstr(pszLine, "Parent Directory") == NULL /* exclude parent directory */)
        {
            char *beginFilename = strstr(pszLine, "<a href=\"");
            if (beginFilename == NULL)
                beginFilename = strstr(pszLine, "<A HREF=\"");
            beginFilename += strlen("<a href=\"");
            char *endQuote = strchr(beginFilename, '"');
            if (endQuote && !STARTS_WITH(beginFilename, "?C=") && !STARTS_WITH(beginFilename, "?N="))
            {
                struct tm brokendowntime;
                memset(&brokendowntime, 0, sizeof(brokendowntime));
                GUIntBig nFileSize = 0;
                GIntBig mTime = 0;

                VSICurlParseHTMLDateTimeFileSize(pszLine,
                                                 brokendowntime,
                                                 nFileSize,
                                                 mTime);

                *endQuote = '\0';

                /* Remove trailing slash, that are returned for directories by */
                /* Apache */
                bool bIsDirectory = false;
                if (endQuote[-1] == '/')
                {
                    bIsDirectory = true;
                    endQuote[-1] = 0;
                }

                /* shttpd links include slashes from the root directory. Skip them */
                while(strchr(beginFilename, '/'))
                    beginFilename = strchr(beginFilename, '/') + 1;

                if (strcmp(beginFilename, ".") != 0 &&
                    strcmp(beginFilename, "..") != 0)
                {
                    CPLString osCachedFilename =
                        CPLSPrintf("%s/%s", pszFilename + strlen("/vsicurl/"), beginFilename);
                    CachedFileProp* cachedFileProp = GetCachedFileProp(osCachedFilename);
                    cachedFileProp->eExists = EXIST_YES;
                    cachedFileProp->bIsDirectory = bIsDirectory;
                    cachedFileProp->mTime = static_cast<time_t>(mTime);
                    cachedFileProp->bHasComputedFileSize = nFileSize > 0;
                    cachedFileProp->fileSize = nFileSize;

                    oFileList.AddString( beginFilename );
                    if (ENABLE_DEBUG)
                        CPLDebug("VSICURL", "File[%d] = %s, is_dir = %d, size = " CPL_FRMT_GUIB ", time = %04d/%02d/%02d %02d:%02d:%02d",
                                nCount, beginFilename, bIsDirectory, nFileSize,
                                brokendowntime.tm_year + 1900, brokendowntime.tm_mon + 1, brokendowntime.tm_mday,
                                brokendowntime.tm_hour, brokendowntime.tm_min, brokendowntime.tm_sec);
                    nCount ++;

                    if( nMaxFiles > 0 && oFileList.Count() > nMaxFiles )
                        break;
                }
            }
        }
        pszLine = c + 1;
    }

    return oFileList.StealList();
}

/************************************************************************/
/*                          AnalyseS3FileList()                         */
/************************************************************************/

#include "cpl_minixml.h"
void VSICurlFilesystemHandler::AnalyseS3FileList( const CPLString& osBaseURL,
                                        const char* pszXML,
                                        CPLStringList& osFileList,
                                        int nMaxFiles,
                                        bool& bIsTruncated,
                                        CPLString& osNextMarker )
{
    //CPLDebug("S3", "%s", pszXML);
    osNextMarker = "";
    bIsTruncated = false;
    CPLXMLNode* psTree = CPLParseXMLString(pszXML);
    if( psTree == NULL )
        return;
    CPLXMLNode* psListBucketResult = CPLGetXMLNode(psTree, "=ListBucketResult");
    if( psListBucketResult )
    {
        CPLString osPrefix = CPLGetXMLValue(psListBucketResult, "Prefix", "");
        CPLXMLNode* psIter = psListBucketResult->psChild;
        for( ; psIter != NULL; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( strcmp(psIter->pszValue, "Contents") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Key", NULL);
                if( pszKey && strlen(pszKey) > osPrefix.size() )
                {
                    CPLString osCachedFilename = osBaseURL + pszKey;
                    //CPLDebug("S3", "Cache %s", osCachedFilename.c_str());

                    CachedFileProp* cachedFileProp = GetCachedFileProp(osCachedFilename);
                    cachedFileProp->eExists = EXIST_YES;
                    cachedFileProp->bHasComputedFileSize = true;
                    cachedFileProp->fileSize = (GUIntBig)CPLAtoGIntBig(CPLGetXMLValue(psIter, "Size", "0"));
                    cachedFileProp->bIsDirectory = false;
                    cachedFileProp->mTime = 0;

                    int nYear, nMonth, nDay, nHour, nMin, nSec;
                    if( sscanf( CPLGetXMLValue(psIter, "LastModified", ""),
                                "%04d-%02d-%02dT%02d:%02d:%02d",
                                &nYear, &nMonth, &nDay, &nHour, &nMin, &nSec ) == 6 )
                    {
                        struct tm brokendowntime;
                        brokendowntime.tm_year = nYear - 1900;
                        brokendowntime.tm_mon = nMonth - 1;
                        brokendowntime.tm_mday = nDay;
                        brokendowntime.tm_hour = nHour;
                        brokendowntime.tm_min = nMin;
                        brokendowntime.tm_sec = nSec;
                        cachedFileProp->mTime = static_cast<time_t>(CPLYMDHMSToUnixTime(&brokendowntime));
                    }

                    osFileList.AddString(pszKey + osPrefix.size());
                }
            }
            else if( strcmp(psIter->pszValue, "CommonPrefixes") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Prefix", NULL);
                if( pszKey && strncmp(pszKey, osPrefix, osPrefix.size()) == 0  )
                {
                    CPLString osKey = pszKey;
                    if( osKey.size() && osKey[osKey.size()-1] == '/' )
                        osKey.resize(osKey.size()-1);
                    if( osKey.size() > osPrefix.size() )
                    {
                        CPLString osCachedFilename = osBaseURL + osKey;
                        //CPLDebug("S3", "Cache %s", osCachedFilename.c_str());

                        CachedFileProp* cachedFileProp = GetCachedFileProp(osCachedFilename);
                        cachedFileProp->eExists = EXIST_YES;
                        cachedFileProp->bIsDirectory = true;
                        cachedFileProp->mTime = 0;

                        osFileList.AddString(osKey.c_str() + osPrefix.size());
                    }
                }
            }

            if( nMaxFiles > 0 && osFileList.Count() > nMaxFiles )
                break;
        }

        if( !(nMaxFiles > 0 && osFileList.Count() > nMaxFiles) )
        {
            osNextMarker = CPLGetXMLValue(psListBucketResult, "NextMarker", "");
            bIsTruncated = CPL_TO_BOOL(CSLTestBoolean(CPLGetXMLValue(psListBucketResult, "IsTruncated", "false")));
        }
    }
    CPLDestroyXMLNode(psTree);
}

/************************************************************************/
/*                         VSICurlGetToken()                            */
/************************************************************************/

static char* VSICurlGetToken(char* pszCurPtr, char** ppszNextToken)
{
    if (pszCurPtr == NULL)
        return NULL;

    while((*pszCurPtr) == ' ')
        pszCurPtr ++;
    if (*pszCurPtr == '\0')
        return NULL;

    char* pszToken = pszCurPtr;
    while((*pszCurPtr) != ' ' && (*pszCurPtr) != '\0')
        pszCurPtr ++;
    if (*pszCurPtr == '\0')
        *ppszNextToken = NULL;
    else
    {
        *pszCurPtr = '\0';
        pszCurPtr ++;
        while((*pszCurPtr) == ' ')
            pszCurPtr ++;
        *ppszNextToken = pszCurPtr;
    }

    return pszToken;
}

/************************************************************************/
/*                    VSICurlParseFullFTPLine()                         */
/************************************************************************/

/* Parse lines like the following ones :
-rw-r--r--    1 10003    100           430 Jul 04  2008 COPYING
lrwxrwxrwx    1 ftp      ftp            28 Jun 14 14:13 MPlayer -> mirrors/mplayerhq.hu/MPlayer
-rw-r--r--    1 ftp      ftp      725614592 May 13 20:13 Fedora-15-x86_64-Live-KDE.iso
drwxr-xr-x  280 1003  1003  6656 Aug 26 04:17 gnu
*/

static bool VSICurlParseFullFTPLine(char* pszLine,
                                   char*& pszFilename,
                                   bool& bSizeValid,
                                   GUIntBig& nSize,
                                   bool& bIsDirectory,
                                   GIntBig& nUnixTime)
{
    char* pszNextToken = pszLine;
    char* pszPermissions = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszPermissions == NULL || strlen(pszPermissions) != 10)
        return false;
    bIsDirectory = (pszPermissions[0] == 'd');

    for(int i = 0; i < 3; i++)
    {
        if (VSICurlGetToken(pszNextToken, &pszNextToken) == NULL)
            return false;
    }

    char* pszSize = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszSize == NULL)
        return false;

    if (pszPermissions[0] == '-')
    {
        /* Regular file */
        bSizeValid = true;
        nSize = CPLScanUIntBig(pszSize, static_cast<int>(strlen(pszSize)));
    }

    struct tm brokendowntime;
    memset(&brokendowntime, 0, sizeof(brokendowntime));
    bool bBrokenDownTimeValid = true;

    char* pszMonth = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszMonth == NULL || strlen(pszMonth) != 3)
        return false;

    int i;
    for(i = 0; i < 12; i++)
    {
        if (EQUALN(pszMonth, apszMonths[i], 3))
            break;
    }
    if (i < 12)
        brokendowntime.tm_mon = i;
    else
        bBrokenDownTimeValid = false;

    char* pszDay = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszDay == NULL || (strlen(pszDay) != 1 && strlen(pszDay) != 2))
        return false;
    int nDay = atoi(pszDay);
    if (nDay >= 1 && nDay <= 31)
        brokendowntime.tm_mday = nDay;
    else
        bBrokenDownTimeValid = false;

    char* pszHourOrYear = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszHourOrYear == NULL || (strlen(pszHourOrYear) != 4 && strlen(pszHourOrYear) != 5))
        return false;
    if (strlen(pszHourOrYear) == 4)
    {
        brokendowntime.tm_year = atoi(pszHourOrYear) - 1900;
    }
    else
    {
        time_t sTime;
        time(&sTime);
        struct tm currentBrokendowntime;
        CPLUnixTimeToYMDHMS((GIntBig)sTime, &currentBrokendowntime);
        brokendowntime.tm_year = currentBrokendowntime.tm_year;
        brokendowntime.tm_hour = atoi(pszHourOrYear);
        brokendowntime.tm_min = atoi(pszHourOrYear + 3);
    }

    if (bBrokenDownTimeValid)
        nUnixTime = CPLYMDHMSToUnixTime(&brokendowntime);
    else
        nUnixTime = 0;

    if (pszNextToken == NULL)
        return false;

    pszFilename = pszNextToken;

    char* pszCurPtr = pszFilename;
    while( *pszCurPtr != '\0')
    {
        /* In case of a link, stop before the pointed part of the link */
        if (pszPermissions[0] == 'l' && STARTS_WITH(pszCurPtr, " -> "))
        {
            break;
        }
        pszCurPtr ++;
    }
    *pszCurPtr = '\0';

    return true;
}

/************************************************************************/
/*                          GetURLFromDirname()                         */
/************************************************************************/

CPLString VSICurlFilesystemHandler::GetURLFromDirname( const CPLString& osDirname )
{
    return osDirname.substr(GetFSPrefix().size());
}

/************************************************************************/
/*                          GetFileList()                               */
/************************************************************************/

char** VSICurlFilesystemHandler::GetFileList(const char *pszDirname,
                                             int nMaxFiles,
                                             bool* pbGotFileList)
{
    if (ENABLE_DEBUG)
        CPLDebug("VSICURL", "GetFileList(%s)" , pszDirname);

    *pbGotFileList = false;

    /* HACK (optimization in fact) for MBTiles driver */
    if (strstr(pszDirname, ".tiles.mapbox.com") != NULL)
        return NULL;

    if (STARTS_WITH(pszDirname, "/vsicurl/ftp"))
    {
        WriteFuncStruct sWriteFuncData;
        sWriteFuncData.pBuffer = NULL;

        CPLString osDirname(pszDirname + strlen("/vsicurl/"));
        osDirname += '/';

        char** papszFileList = NULL;

        for(int iTry=0;iTry<2;iTry++)
        {
            CURL* hCurlHandle = GetCurlHandleFor(osDirname);
            VSICurlSetOptions(hCurlHandle, osDirname.c_str());

            /* On the first pass, we want to try fetching all the possible */
            /* information (filename, file/directory, size). If that */
            /* does not work, then try again with CURLOPT_DIRLISTONLY set */
            if (iTry == 1)
            {
        /* 7.16.4 */
        #if LIBCURL_VERSION_NUM <= 0x071004
                curl_easy_setopt(hCurlHandle, CURLOPT_FTPLISTONLY, 1);
        #elif LIBCURL_VERSION_NUM > 0x071004
                curl_easy_setopt(hCurlHandle, CURLOPT_DIRLISTONLY, 1);
        #endif
            }

            VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
            curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
            curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

            char szCurlErrBuf[CURL_ERROR_SIZE+1];
            szCurlErrBuf[0] = '\0';
            curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

            curl_easy_perform(hCurlHandle);

            if (sWriteFuncData.pBuffer == NULL)
                return NULL;

            char* pszLine = sWriteFuncData.pBuffer;
            char* c;
            int nCount = 0;

            if (STARTS_WITH_CI(pszLine, "<!DOCTYPE HTML") ||
                STARTS_WITH_CI(pszLine, "<HTML>"))
            {
                papszFileList = ParseHTMLFileList(pszDirname,
                                                  nMaxFiles,
                                                  sWriteFuncData.pBuffer,
                                                  pbGotFileList);
                break;
            }
            else if (iTry == 0)
            {
                CPLStringList oFileList;
                *pbGotFileList = true;

                while( (c = strchr(pszLine, '\n')) != NULL)
                {
                    *c = 0;
                    if (c - pszLine > 0 && c[-1] == '\r')
                        c[-1] = 0;

                    char* pszFilename = NULL;
                    bool bSizeValid = false;
                    GUIntBig nFileSize = 0;
                    bool bIsDirectory = false;
                    GIntBig mUnixTime = 0;
                    if (!VSICurlParseFullFTPLine(pszLine, pszFilename,
                                                 bSizeValid, nFileSize,
                                                 bIsDirectory, mUnixTime))
                        break;

                    if (strcmp(pszFilename, ".") != 0 &&
                        strcmp(pszFilename, "..") != 0)
                    {
                        CPLString osCachedFilename =
                            CPLSPrintf("%s/%s", pszDirname + strlen("/vsicurl/"), pszFilename);
                        CachedFileProp* cachedFileProp = GetCachedFileProp(osCachedFilename);
                        cachedFileProp->eExists = EXIST_YES;
                        cachedFileProp->bHasComputedFileSize = bSizeValid;
                        cachedFileProp->fileSize = nFileSize;
                        cachedFileProp->bIsDirectory = bIsDirectory;
                        cachedFileProp->mTime = static_cast<time_t>(mUnixTime);

                        oFileList.AddString(pszFilename);
                        if (ENABLE_DEBUG)
                        {
                            struct tm brokendowntime;
                            CPLUnixTimeToYMDHMS(mUnixTime, &brokendowntime);
                            CPLDebug("VSICURL", "File[%d] = %s, is_dir = %d, size = " CPL_FRMT_GUIB ", time = %04d/%02d/%02d %02d:%02d:%02d",
                                    nCount, pszFilename, bIsDirectory, nFileSize,
                                    brokendowntime.tm_year + 1900, brokendowntime.tm_mon + 1, brokendowntime.tm_mday,
                                    brokendowntime.tm_hour, brokendowntime.tm_min, brokendowntime.tm_sec);
                        }

                        nCount ++;

                        if( nMaxFiles > 0 && oFileList.Count() > nMaxFiles )
                            break;
                    }

                    pszLine = c + 1;
                }

                if (c == NULL)
                {
                    papszFileList = oFileList.StealList();
                    break;
                }
            }
            else
            {
                CPLStringList oFileList;
                *pbGotFileList = true;

                while( (c = strchr(pszLine, '\n')) != NULL)
                {
                    *c = 0;
                    if (c - pszLine > 0 && c[-1] == '\r')
                        c[-1] = 0;

                    if (strcmp(pszLine, ".") != 0 &&
                        strcmp(pszLine, "..") != 0)
                    {
                        oFileList.AddString(pszLine);
                        if (ENABLE_DEBUG)
                            CPLDebug("VSICURL", "File[%d] = %s", nCount, pszLine);
                        nCount ++;
                    }

                    pszLine = c + 1;
                }

                papszFileList = oFileList.StealList();
            }

            CPLFree(sWriteFuncData.pBuffer);
            sWriteFuncData.pBuffer = NULL;
        }

        CPLFree(sWriteFuncData.pBuffer);

        return papszFileList;
    }

    /* Try to recognize HTML pages that list the content of a directory */
    /* Currently this supports what Apache and shttpd can return */
    else if (STARTS_WITH(pszDirname, "/vsicurl/http://") ||
             STARTS_WITH(pszDirname, "/vsicurl/https://"))
    {
        WriteFuncStruct sWriteFuncData;

        CPLString osDirname(pszDirname + strlen("/vsicurl/"));
        osDirname += '/';

    #if LIBCURL_VERSION_NUM < 0x070B00
        /* Curl 7.10.X doesn't manage to unset the CURLOPT_RANGE that would have been */
        /* previously set, so we have to reinit the connection handle */
        GetCurlHandleFor("");
    #endif

        CURL* hCurlHandle = GetCurlHandleFor(osDirname);
        VSICurlSetOptions(hCurlHandle, osDirname.c_str());

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1];
        szCurlErrBuf[0] = '\0';
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        curl_easy_perform(hCurlHandle);

        if (sWriteFuncData.pBuffer == NULL)
            return NULL;

        char** papszFileList = NULL;
        if( STARTS_WITH_CI((const char*)sWriteFuncData.pBuffer, "<?xml") &&
            strstr((const char*)sWriteFuncData.pBuffer, "<ListBucketResult") != NULL )
        {
            CPLString osNextMarker;
            CPLStringList osFileList;
            CPLString osBaseURL(pszDirname);
            osBaseURL += "/";
            bool bIsTruncated = true;
            AnalyseS3FileList( osBaseURL,
                               (const char*)sWriteFuncData.pBuffer,
                               osFileList,
                               nMaxFiles,
                               bIsTruncated,
                               osNextMarker );
            // If the list is truncated, then don't report it
            if( !bIsTruncated )
            {
                papszFileList = osFileList.StealList();
                *pbGotFileList = true;
            }
        }
        else
        {
            papszFileList = ParseHTMLFileList(pszDirname,
                                              nMaxFiles,
                                              sWriteFuncData.pBuffer,
                                              pbGotFileList);
        }

        CPLFree(sWriteFuncData.pBuffer);
        return papszFileList;
    }

    return NULL;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSICurlFilesystemHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                                    int nFlags )
{
    const CPLString osFilename(pszFilename);

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    const char* pszOptionVal =
        CPLGetConfigOption( "GDAL_DISABLE_READDIR_ON_OPEN", "NO" );
    const bool bSkipReadDir = EQUAL(pszOptionVal, "EMPTY_DIR") ||
                              CSLTestBoolean(pszOptionVal);

    /* Does it look like a FTP directory ? */
    if (STARTS_WITH(osFilename, "/vsicurl/ftp") &&
        pszFilename[strlen(osFilename) - 1] == '/' && !bSkipReadDir)
    {
        char** papszFileList = ReadDirEx(osFilename, 0);
        if (papszFileList)
        {
            pStatBuf->st_mode = S_IFDIR;
            pStatBuf->st_size = 0;

            CSLDestroy(papszFileList);

            return 0;
        }
        return -1;
    }
    else if (strchr(CPLGetFilename(osFilename), '.') != NULL &&
             !STARTS_WITH_CI(CPLGetExtension(osFilename), "zip") &&
             strstr(osFilename, ".zip.") != NULL &&
             strstr(osFilename, ".ZIP.") != NULL &&
             !bSkipReadDir)
    {
        bool bGotFileList;
        char** papszFileList = ReadDirInternal(CPLGetDirname(osFilename), 0, &bGotFileList);
        const bool bFound = (VSICurlIsFileInList(papszFileList, CPLGetFilename(osFilename)) != -1);
        CSLDestroy(papszFileList);
        if (bGotFileList && !bFound)
        {
            return -1;
        }
    }

    VSICurlHandle* poHandle = CreateFileHandle( osFilename + strlen(GetFSPrefix()) );
    if( poHandle == NULL )
        return -1;

    if ( poHandle->IsKnownFileSize() ||
         ((nFlags & VSI_STAT_SIZE_FLAG) && !poHandle->IsDirectory() &&
           CSLTestBoolean(CPLGetConfigOption("CPL_VSIL_CURL_SLOW_GET_SIZE", "YES"))) )
        pStatBuf->st_size = poHandle->GetFileSize();

    int nRet = (poHandle->Exists()) ? 0 : -1;
    pStatBuf->st_mtime = poHandle->GetMTime();
    pStatBuf->st_mode = poHandle->IsDirectory() ? S_IFDIR : S_IFREG;
    delete poHandle;
    return nRet;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSICurlFilesystemHandler::Unlink( CPL_UNUSED const char *pszFilename )
{
    return -1;
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSICurlFilesystemHandler::Rename( CPL_UNUSED const char *oldpath,
                                      CPL_UNUSED const char *newpath )
{
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSICurlFilesystemHandler::Mkdir( CPL_UNUSED const char *pszDirname,
                                     CPL_UNUSED long nMode )
{
    return -1;
}
/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSICurlFilesystemHandler::Rmdir( CPL_UNUSED const char *pszDirname )
{
    return -1;
}

/************************************************************************/
/*                             ReadDirInternal()                        */
/************************************************************************/

char** VSICurlFilesystemHandler::ReadDirInternal( const char *pszDirname,
                                          int nMaxFiles,
                                          bool* pbGotFileList )
{
    CPLString osDirname(pszDirname);
    while (osDirname[strlen(osDirname) - 1] == '/')
        osDirname.erase(strlen(osDirname) - 1);

    const char* pszUpDir = strstr(osDirname, "/..");
    if (pszUpDir != NULL)
    {
        int pos = static_cast<int>(pszUpDir - osDirname.c_str() - 1);
        while(pos >= 0 && osDirname[pos] != '/')
            pos --;
        if (pos >= 1)
        {
            osDirname = osDirname.substr(0, pos) + CPLString(pszUpDir + 3);
        }
    }

    if( osDirname.size() <= GetFSPrefix().size() )
    {
        if (pbGotFileList)
            *pbGotFileList = true;
        return NULL;
    }

    CPLMutexHolder oHolder( &hMutex );

    /* If we know the file exists and is not a directory, then don't try to list its content */
    CachedFileProp* cachedFileProp = GetCachedFileProp(GetURLFromDirname(osDirname));
    if (cachedFileProp->eExists == EXIST_YES && !cachedFileProp->bIsDirectory)
    {
        if (pbGotFileList)
            *pbGotFileList = true;
        return NULL;
    }

    CachedDirList* psCachedDirList = cacheDirList[osDirname];
    if (psCachedDirList == NULL)
    {
        psCachedDirList = (CachedDirList*) CPLMalloc(sizeof(CachedDirList));
        psCachedDirList->papszFileList = GetFileList(osDirname, nMaxFiles,
                                                     &psCachedDirList->bGotFileList);
        cacheDirList[osDirname] = psCachedDirList;
    }

    if (pbGotFileList)
        *pbGotFileList = psCachedDirList->bGotFileList;

    return CSLDuplicate(psCachedDirList->papszFileList);
}

/************************************************************************/
/*                        InvalidateDirContent()                        */
/************************************************************************/

void VSICurlFilesystemHandler::InvalidateDirContent( const char *pszDirname )
{
    CPLMutexHolder oHolder( &hMutex );
    std::map<CPLString, CachedDirList*>::iterator oIter =
        cacheDirList.find(pszDirname);
    if( oIter != cacheDirList.end() )
    {
        CSLDestroy( oIter->second->papszFileList );
        CPLFree( oIter->second );
        cacheDirList.erase(oIter);
    }
}

/************************************************************************/
/*                             ReadDirEx()                              */
/************************************************************************/

char** VSICurlFilesystemHandler::ReadDirEx( const char *pszDirname,
                                            int nMaxFiles )
{
    return ReadDirInternal(pszDirname, nMaxFiles, NULL);
}

/************************************************************************/
/*                   VSIInstallCurlFileHandler()                        */
/************************************************************************/

/**
 * \brief Install /vsicurl/ HTTP/FTP file system handler (requires libcurl)
 *
 * A special file handler is installed that allows on-the-fly random reading of files
 * available through HTTP/FTP web protocols, without prior download of the entire file.
 *
 * Recognized filenames are of the form /vsicurl/http://path/to/remote/resource or
 * /vsicurl/ftp://path/to/remote/resource where path/to/remote/resource is the
 * URL of a remote resource.
 *
 * Partial downloads (requires the HTTP server to support random reading) are done
 * with a 16 KB granularity by default. If the driver detects sequential reading
 * it will progressively increase the chunk size up to 2 MB to improve download
 * performance.
 *
 * The GDAL_HTTP_PROXY, GDAL_HTTP_PROXYUSERPWD and GDAL_PROXY_AUTH configuration options can be
 * used to define a proxy server. The syntax to use is the one of Curl CURLOPT_PROXY,
 * CURLOPT_PROXYUSERPWD and CURLOPT_PROXYAUTH options.
 *
 * Starting with GDAL 1.10, the file can be cached in RAM by setting the configuration option
 * VSI_CACHE to TRUE. The cache size defaults to 25 MB, but can be modified by setting
 * the configuration option VSI_CACHE_SIZE (in bytes).
 *
 * VSIStatL() will return the size in st_size member and file
 * nature- file or directory - in st_mode member (the later only reliable with FTP
 * resources for now).
 *
 * VSIReadDir() should be able to parse the HTML directory listing returned by the
 * most popular web servers, such as Apache or Microsoft IIS.
 *
 * This special file handler can be combined with other virtual filesystems handlers,
 * such as /vsizip. For example, /vsizip//vsicurl/path/to/remote/file.zip/path/inside/zip
 *
 * @since GDAL 1.8.0
 */
void VSIInstallCurlFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsicurl/", new VSICurlFilesystemHandler );
}

/************************************************************************/
/*                      VSICurlInstallReadCbk()                         */
/************************************************************************/

int VSICurlInstallReadCbk (VSILFILE* fp,
                           VSICurlReadCbkFunc pfnReadCbk,
                           void* pfnUserData,
                           int bStopOnInterrruptUntilUninstall)
{
    return ((VSICurlHandle*)fp)->InstallReadCbk(pfnReadCbk, pfnUserData,
                                                bStopOnInterrruptUntilUninstall);
}


/************************************************************************/
/*                    VSICurlUninstallReadCbk()                         */
/************************************************************************/

int VSICurlUninstallReadCbk(VSILFILE* fp)
{
    return ((VSICurlHandle*)fp)->UninstallReadCbk();
}


/************************************************************************/
/*                         VSIS3FSHandler                               */
/************************************************************************/

class VSIS3FSHandler: public VSICurlFilesystemHandler
{
    std::map< CPLString, VSIS3UpdateParams > oMapBucketsToS3Params;

protected:
    virtual CPLString GetFSPrefix() { return "/vsis3/"; }
    virtual VSICurlHandle* CreateFileHandle(const char* pszURL);
    virtual char** GetFileList(const char *pszFilename,
                               int nMaxFiles,
                               bool* pbGotFileList);
    virtual CPLString GetURLFromDirname( const CPLString& osDirname );

public:
        VSIS3FSHandler() {}

        virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                        const char *pszAccess);
        virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );
        virtual int      Unlink( const char *pszFilename );

        void UpdateMapFromHandle(VSIS3HandleHelper * poS3HandleHelper);
        void UpdateHandleFromMap(VSIS3HandleHelper * poS3HandleHelper);
};

/************************************************************************/
/*                            VSIS3Handle                               */
/************************************************************************/

class VSIS3Handle: public VSICurlHandle
{
    VSIS3HandleHelper* m_poS3HandleHelper;

  protected:
        virtual struct curl_slist* GetCurlHeaders(const CPLString& osVerb);
        virtual bool CanRestartOnError(const char*);
        virtual bool UseLimitRangeGetInsteadOfHead() { return true; }
        virtual void ProcessGetFileSizeResult(const char* pszContent);

    public:
        VSIS3Handle(VSIS3FSHandler* poFS,
                    VSIS3HandleHelper* poS3HandleHelper);
        ~VSIS3Handle();
};

/************************************************************************/
/*                            VSIS3WriteHandle                          */
/************************************************************************/

class VSIS3WriteHandle: public VSIVirtualHandle
{
    VSIS3FSHandler     *m_poFS;
    CPLString           m_osFilename;
    VSIS3HandleHelper  *m_poS3HandleHelper;
    vsi_l_offset        m_nCurOffset;
    int                 m_nBufferOff;
    int                 m_nBufferSize;
    int                 m_nBufferOffReadCallback;
    bool                m_bClosed;
    GByte              *m_pabyBuffer;
    CPLString           m_osUploadID;
    int                 m_nPartNumber;
    std::vector<CPLString> m_aosEtags;
    CPLString           m_osXML;
    int                 m_nOffsetInXML;
    bool                m_bError;

    static size_t       ReadCallBackBuffer( char *buffer, size_t size,
                                            size_t nitems, void *instream );
    bool                InitiateMultipartUpload();
    bool                UploadPart();
    static size_t       ReadCallBackXML( char *buffer, size_t size,
                                         size_t nitems, void *instream );
    bool                CompleteMultipart();
    bool                AbortMultipart();
    bool                DoSinglePartPUT();

    public:
        VSIS3WriteHandle(VSIS3FSHandler* poFS,
                         const char* pszFilename,
                         VSIS3HandleHelper* poS3HandleHelper);
        ~VSIS3WriteHandle();

        virtual int       Seek( vsi_l_offset nOffset, int nWhence );
        virtual vsi_l_offset Tell();
        virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
        virtual size_t    Write( const void *pBuffer, size_t nSize
                                 ,size_t nMemb );
        virtual int       Eof();
        virtual int       Close();

        bool              IsOK() { return m_pabyBuffer != NULL; }
};

/************************************************************************/
/*                         VSIS3WriteHandle()                           */
/************************************************************************/

VSIS3WriteHandle::VSIS3WriteHandle(VSIS3FSHandler* poFS,
                                   const char* pszFilename,
                                   VSIS3HandleHelper* poS3HandleHelper) :
        m_poFS(poFS), m_osFilename(pszFilename),
        m_poS3HandleHelper(poS3HandleHelper),
        m_nCurOffset(0),
        m_nBufferOff(0),
        m_nBufferOffReadCallback(0),
        m_bClosed(false),
        m_nPartNumber(0),
        m_nOffsetInXML(0),
        m_bError(false)
{
    int nChunkSizeMB = atoi(CPLGetConfigOption("VSIS3_CHUNK_SIZE", "50"));
    if( nChunkSizeMB <= 0 || nChunkSizeMB > 1000 )
        m_nBufferSize = 0;
    else
        m_nBufferSize = nChunkSizeMB * 1024 * 1024;
    m_pabyBuffer = (GByte*)VSIMalloc(m_nBufferSize);
    if( m_pabyBuffer == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot allocate working buffer for /vsis3");
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
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIS3WriteHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if( (nWhence == SEEK_SET && nOffset != m_nCurOffset) ||
        nOffset != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Seek not supported on writable /vsis3 files");
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
             "Read not supported on writable /vsis3 files");
    m_bError = true;
    return 0;
}

/************************************************************************/
/*                        InitiateMultipartUpload()                     */
/************************************************************************/

bool VSIS3WriteHandle::InitiateMultipartUpload()
{
    bool bSuccess = true;
    bool bGoOn;
    do
    {
        bGoOn = false;
        CURL* hCurlHandle = curl_easy_init();
        m_poS3HandleHelper->AddQueryParameter("uploads", "");
        curl_easy_setopt(hCurlHandle, CURLOPT_URL, m_poS3HandleHelper->GetURL().c_str());
        CPLHTTPSetOptions(hCurlHandle, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

        struct curl_slist* headers = m_poS3HandleHelper->GetCurlHeaders("POST");
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        m_poS3HandleHelper->ResetQueryParameters();

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

        curl_easy_perform(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 || sWriteFuncData.pBuffer == NULL )
        {
            if( sWriteFuncData.pBuffer != NULL &&
                m_poS3HandleHelper->CanRestartOnError( (const char*)sWriteFuncData.pBuffer) )
            {
                m_poFS->UpdateMapFromHandle(m_poS3HandleHelper);
                bGoOn = true;
            }
            else
            {
                CPLDebug("S3", "%s", (sWriteFuncData.pBuffer) ? (const char*)sWriteFuncData.pBuffer : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "InitiateMultipartUpload of %s failed",
                            m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            m_poFS->InvalidateCachedFileProp( m_poS3HandleHelper->GetURL().c_str() );
            m_poFS->InvalidateDirContent( CPLGetDirname(m_osFilename) );

            CPLXMLNode* psNode = CPLParseXMLString( (const char*)sWriteFuncData.pBuffer );
            if( psNode )
            {
                m_osUploadID = CPLGetXMLValue(psNode, "=InitiateMultipartUploadResult.UploadId", "");
                CPLDebug("S3", "UploadId: %s", m_osUploadID.c_str());
                CPLDestroyXMLNode(psNode);
            }
            if( m_osUploadID.size() == 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "InitiateMultipartUpload of %s failed: cannot get UploadId",
                         m_osFilename.c_str());
                bSuccess = false;
            }
        }

        CPLFree(sWriteFuncData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bGoOn );
    return bSuccess;
}

/************************************************************************/
/*                         ReadCallBackBuffer()                         */
/************************************************************************/

size_t VSIS3WriteHandle::ReadCallBackBuffer( char *buffer, size_t size,
                                             size_t nitems, void *instream)
{
    VSIS3WriteHandle* poThis = (VSIS3WriteHandle*)instream;
    int nSizeMax = (int)(size * nitems);
    int nSizeToWrite = MIN(nSizeMax, poThis->m_nBufferOff - poThis->m_nBufferOffReadCallback);
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
    ++ m_nPartNumber;
    if( m_nPartNumber > 10000 )
    {
        m_bError = true;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "10000 parts have been uploaded for %s failed. This is the maximum. "
                 "Increase VSIS3_CHUNK_SIZE to a higher value (e.g. 500 for 500 MB)",
                 m_osFilename.c_str());
        return false;
    }

    bool bSuccess = true;

    m_nBufferOffReadCallback = 0;
    CURL* hCurlHandle = curl_easy_init();
    m_poS3HandleHelper->AddQueryParameter("partNumber", CPLSPrintf("%d", m_nPartNumber));
    m_poS3HandleHelper->AddQueryParameter("uploadId", m_osUploadID);
    curl_easy_setopt(hCurlHandle, CURLOPT_URL, m_poS3HandleHelper->GetURL().c_str());
    CPLHTTPSetOptions(hCurlHandle, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
    curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
    curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);

    struct curl_slist* headers = m_poS3HandleHelper->GetCurlHeaders("PUT",
                                                                    m_pabyBuffer,
                                                                    m_nBufferOff);
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    m_poS3HandleHelper->ResetQueryParameters();

    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    WriteFuncStruct sWriteFuncHeaderData;
    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, VSICurlHandleWriteFunc);

    curl_easy_perform(hCurlHandle);

    curl_slist_free_all(headers);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
    if( response_code != 200 || sWriteFuncHeaderData.pBuffer == NULL )
    {
        CPLDebug("S3", "%s", (sWriteFuncData.pBuffer) ? (const char*)sWriteFuncData.pBuffer : "(null)");
        CPLError(CE_Failure, CPLE_AppDefined, "UploadPart(%d) of %s failed",
                    m_nPartNumber, m_osFilename.c_str());
        bSuccess = false;
    }
    else
    {
        const char* pszEtag = strstr((const char*)sWriteFuncHeaderData.pBuffer, "ETag: ");
        if( pszEtag != NULL )
        {
            CPLString osEtag = pszEtag + strlen("ETag: ");
            size_t nPos = osEtag.find("\r");
            if( nPos != std::string::npos )
                osEtag.resize(nPos);
            CPLDebug("S3", "Etag for part %d is %s", m_nPartNumber, osEtag.c_str());
            m_aosEtags.push_back(osEtag);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "UploadPart(%d) of %s (uploadId = %s) failed",
                        m_nPartNumber, m_osFilename.c_str(), m_osUploadID.c_str());
            bSuccess = false;
        }
    }

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);

    curl_easy_cleanup(hCurlHandle);

    return bSuccess;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIS3WriteHandle::Write( const void *pBuffer, size_t nSize,size_t nMemb)
{
    size_t nBytesToWrite = nSize * nMemb;

    if( m_bError )
        return false;

    while( nBytesToWrite > 0 )
    {
        int nToWriteInBuffer = (int)MIN((size_t)(m_nBufferSize - m_nBufferOff), nBytesToWrite);
        memcpy(m_pabyBuffer + m_nBufferOff, pBuffer, nToWriteInBuffer);
        m_nBufferOff += nToWriteInBuffer;
        m_nCurOffset += nToWriteInBuffer;
        nBytesToWrite -= nToWriteInBuffer;
        if( m_nBufferOff == m_nBufferSize )
        {
            if( m_nCurOffset == (vsi_l_offset)m_nBufferSize )
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
/*                           DoSinglePartPUT()                          */
/************************************************************************/

bool VSIS3WriteHandle::DoSinglePartPUT()
{
    bool bSuccess = true;
    bool bGoOn;
    do
    {
        bGoOn = false;
        m_nBufferOffReadCallback = 0;
        CURL* hCurlHandle = curl_easy_init();
        curl_easy_setopt(hCurlHandle, CURLOPT_URL, m_poS3HandleHelper->GetURL().c_str());
        CPLHTTPSetOptions(hCurlHandle, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
        curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);

        struct curl_slist* headers = m_poS3HandleHelper->GetCurlHeaders("PUT",
                                                                        m_pabyBuffer,
                                                                        m_nBufferOff);
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

        curl_easy_perform(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            if( sWriteFuncData.pBuffer != NULL &&
                m_poS3HandleHelper->CanRestartOnError( (const char*)sWriteFuncData.pBuffer) )
            {
                m_poFS->UpdateMapFromHandle(m_poS3HandleHelper);
                bGoOn = true;
            }
            else
            {
                CPLDebug("S3", "%s", (sWriteFuncData.pBuffer) ? (const char*)sWriteFuncData.pBuffer : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "DoSinglePartPUT of %s failed",
                            m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            m_poFS->InvalidateCachedFileProp( m_poS3HandleHelper->GetURL().c_str() );
            m_poFS->InvalidateDirContent( CPLGetDirname(m_osFilename) );
        }

        CPLFree(sWriteFuncData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bGoOn );
    return bSuccess;
}

/************************************************************************/
/*                            ReadCallBackXML()                         */
/************************************************************************/

size_t VSIS3WriteHandle::ReadCallBackXML(char *buffer, size_t size, size_t nitems, void *instream)
{
    VSIS3WriteHandle* poThis = (VSIS3WriteHandle*)instream;
    int nSizeMax = (int)(size * nitems);
    int nSizeToWrite = MIN(nSizeMax, (int)poThis->m_osXML.size() - poThis->m_nOffsetInXML);
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
    for(size_t i=0;i<m_aosEtags.size();i++)
    {
        m_osXML += "<Part>\n";
        m_osXML += CPLSPrintf("<PartNumber>%d</PartNumber>", (int)(i+1));
        m_osXML += "<ETag>" + m_aosEtags[i] + "</ETag>";
        m_osXML += "</Part>\n";
    }
    m_osXML += "</CompleteMultipartUpload>\n";

    m_nOffsetInXML = 0;
    CURL* hCurlHandle = curl_easy_init();
    m_poS3HandleHelper->AddQueryParameter("uploadId", m_osUploadID);
    curl_easy_setopt(hCurlHandle, CURLOPT_URL, m_poS3HandleHelper->GetURL().c_str());
    CPLHTTPSetOptions(hCurlHandle, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackXML);
    curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
    curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, (int)m_osXML.size());
    curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

    struct curl_slist* headers = m_poS3HandleHelper->GetCurlHeaders("POST",
                                                                    m_osXML.c_str(),
                                                                    m_osXML.size());
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    m_poS3HandleHelper->ResetQueryParameters();

    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    curl_easy_perform(hCurlHandle);

    curl_slist_free_all(headers);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
    if( response_code != 200 )
    {
        CPLDebug("S3", "%s", (sWriteFuncData.pBuffer) ? (const char*)sWriteFuncData.pBuffer : "(null)");
        CPLError(CE_Failure, CPLE_AppDefined, "CompleteMultipart of %s (uploadId=%s) failed",
                    m_osFilename.c_str(), m_osUploadID.c_str());
        bSuccess = false;
    }

    CPLFree(sWriteFuncData.pBuffer);

    curl_easy_cleanup(hCurlHandle);

    return bSuccess;
}

/************************************************************************/
/*                          AbortMultipart()                            */
/************************************************************************/

bool VSIS3WriteHandle::AbortMultipart()
{
    bool bSuccess = true;

    CURL* hCurlHandle = curl_easy_init();
    m_poS3HandleHelper->AddQueryParameter("uploadId", m_osUploadID);
    curl_easy_setopt(hCurlHandle, CURLOPT_URL, m_poS3HandleHelper->GetURL().c_str());
    CPLHTTPSetOptions(hCurlHandle, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

    struct curl_slist* headers = m_poS3HandleHelper->GetCurlHeaders("DELETE");
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    m_poS3HandleHelper->ResetQueryParameters();

    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    curl_easy_perform(hCurlHandle);

    curl_slist_free_all(headers);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
    if( response_code != 204 )
    {
        CPLDebug("S3", "%s", (sWriteFuncData.pBuffer) ? (const char*)sWriteFuncData.pBuffer : "(null)");
        CPLError(CE_Failure, CPLE_AppDefined, "AbortMultipart of %s (uploadId=%s) failed",
                    m_osFilename.c_str(), m_osUploadID.c_str());
        bSuccess = false;
    }

    CPLFree(sWriteFuncData.pBuffer);

    curl_easy_cleanup(hCurlHandle);

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
        if( m_osUploadID.size() == 0 )
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
                                        const char *pszAccess)
{
    if (strchr(pszAccess, 'w') != NULL )
    {
        /*if( strchr(pszAccess, '+') != NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "w+ not supported for /vsis3. Only w");
            return NULL;
        }*/
        VSIS3HandleHelper* poS3HandleHelper =
            VSIS3HandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str(), false);
        if( poS3HandleHelper == NULL )
            return NULL;
        UpdateHandleFromMap(poS3HandleHelper);
        VSIS3WriteHandle* poHandle = new VSIS3WriteHandle(this, pszFilename, poS3HandleHelper);
        if( !poHandle->IsOK() )
        {
            delete poHandle;
            poHandle = NULL;
        }
        return poHandle;
    }
    else
    {
        return VSICurlFilesystemHandler::Open(pszFilename, pszAccess);
    }
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIS3FSHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                          int nFlags )
{
    CPLString osFilename(pszFilename);
    if( osFilename.find('/', GetFSPrefix().size()) == std::string::npos )
        osFilename += "/";
    return VSICurlFilesystemHandler::Stat(osFilename, pStatBuf, nFlags);
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIS3FSHandler::CreateFileHandle(const char* pszURL)
{
    VSIS3HandleHelper* poS3HandleHelper =
            VSIS3HandleHelper::BuildFromURI(pszURL, GetFSPrefix().c_str(), false);
    if( poS3HandleHelper )
    {
        UpdateHandleFromMap(poS3HandleHelper);
        return new VSIS3Handle(this, poS3HandleHelper);
    }
    return NULL;
}

/************************************************************************/
/*                          GetURLFromDirname()                         */
/************************************************************************/

CPLString VSIS3FSHandler::GetURLFromDirname( const CPLString& osDirname )
{
    CPLString osDirnameWithoutPrefix = osDirname.substr(GetFSPrefix().size());

    VSIS3HandleHelper* poS3HandleHelper =
            VSIS3HandleHelper::BuildFromURI(osDirnameWithoutPrefix, GetFSPrefix().c_str(), true);
    if( poS3HandleHelper == NULL )
    {
        return "";
    }
    UpdateHandleFromMap(poS3HandleHelper);
    CPLString osBaseURL(poS3HandleHelper->GetURL());
    if( osBaseURL.size() && osBaseURL[osBaseURL.size()-1] == '/' )
        osBaseURL.resize(osBaseURL.size()-1);
    delete poS3HandleHelper;

    return osBaseURL;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSIS3FSHandler::Unlink( const char *pszFilename )
{
    CPLString osNameWithoutPrefix = pszFilename + GetFSPrefix().size();
    VSIS3HandleHelper* poS3HandleHelper =
            VSIS3HandleHelper::BuildFromURI(osNameWithoutPrefix, GetFSPrefix().c_str(), false);
    if( poS3HandleHelper == NULL )
    {
        return -1;
    }
    UpdateHandleFromMap(poS3HandleHelper);

    int nRet = 0;

    bool bGoOn;
    do
    {
        bGoOn = false;
        CURL* hCurlHandle = curl_easy_init();
        curl_easy_setopt(hCurlHandle, CURLOPT_URL, poS3HandleHelper->GetURL().c_str());
        CPLHTTPSetOptions(hCurlHandle, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

        struct curl_slist* headers = poS3HandleHelper->GetCurlHeaders("DELETE");
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

        curl_easy_perform(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 204 )
        {
            if( sWriteFuncData.pBuffer != NULL &&
                poS3HandleHelper->CanRestartOnError( (const char*)sWriteFuncData.pBuffer) )
            {
                UpdateMapFromHandle(poS3HandleHelper);
                bGoOn = true;
            }
            else
            {
                CPLDebug("S3", "%s", (sWriteFuncData.pBuffer) ? (const char*)sWriteFuncData.pBuffer : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "Delete of %s failed",
                         pszFilename);
                nRet = -1;
            }
        }
        else
        {
            InvalidateCachedFileProp(poS3HandleHelper->GetURL().c_str());
            InvalidateDirContent( CPLGetDirname(pszFilename) );
        }

        CPLFree(sWriteFuncData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bGoOn );

    delete poS3HandleHelper;
    return nRet;
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** VSIS3FSHandler::GetFileList( const char *pszDirname,
                                    int nMaxFiles,
                                    bool* pbGotFileList )
{
    // TODO: to implement
    if (ENABLE_DEBUG)
        CPLDebug("S3", "GetFileList(%s)" , pszDirname);
    *pbGotFileList = false;
    CPLString osDirnameWithoutPrefix = pszDirname + GetFSPrefix().size();


    VSIS3HandleHelper* poS3HandleHelper =
            VSIS3HandleHelper::BuildFromURI(osDirnameWithoutPrefix, GetFSPrefix().c_str(), true);
    if( poS3HandleHelper == NULL )
    {
        return NULL;
    }
    UpdateHandleFromMap(poS3HandleHelper);

    CPLString osObjectKey = poS3HandleHelper->GetObjectKey();
    poS3HandleHelper->SetObjectKey("");

    WriteFuncStruct sWriteFuncData;

    CPLStringList osFileList;
    CPLString osNextMarker;

    CPLString osMaxKeys = CPLGetConfigOption("AWS_MAX_KEYS", "");

    while(true)
    {
        poS3HandleHelper->ResetQueryParameters();
        CPLString osBaseURL(poS3HandleHelper->GetURL());

    #if LIBCURL_VERSION_NUM < 0x070B00
        /* Curl 7.10.X doesn't manage to unset the CURLOPT_RANGE that would have been */
        /* previously set, so we have to reinit the connection handle */
        GetCurlHandleFor("");
    #endif

        CURL* hCurlHandle = GetCurlHandleFor(osBaseURL);

        poS3HandleHelper->AddQueryParameter("delimiter", "/");
        if( osNextMarker.size() )
            poS3HandleHelper->AddQueryParameter("marker", osNextMarker);
        if( osMaxKeys.size() )
             poS3HandleHelper->AddQueryParameter("max-keys", osMaxKeys);
        if( osObjectKey.size() )
             poS3HandleHelper->AddQueryParameter("prefix", osObjectKey + "/");

        VSICurlSetOptions(hCurlHandle, poS3HandleHelper->GetURL());

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1];
        szCurlErrBuf[0] = '\0';
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        struct curl_slist* headers = poS3HandleHelper->GetCurlHeaders("GET");
        if( headers != NULL )
            curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        curl_easy_perform(hCurlHandle);

        if( headers != NULL )
            curl_slist_free_all(headers);

        if (sWriteFuncData.pBuffer == NULL)
        {
            delete poS3HandleHelper;
            return NULL;
        }

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            if( sWriteFuncData.pBuffer != NULL &&
                poS3HandleHelper->CanRestartOnError( (const char*)sWriteFuncData.pBuffer) )
            {
                UpdateMapFromHandle(poS3HandleHelper);
                CPLFree(sWriteFuncData.pBuffer);
            }
            else
            {
                CPLDebug("S3", "%s", sWriteFuncData.pBuffer ? (const char*)sWriteFuncData.pBuffer : "(null)");
                CPLFree(sWriteFuncData.pBuffer);
                delete poS3HandleHelper;
                return NULL;
            }
        }
        else
        {
            *pbGotFileList = true;
            bool bIsTrucated;
            AnalyseS3FileList( osBaseURL,
                               (const char*)sWriteFuncData.pBuffer,
                               osFileList,
                               nMaxFiles,
                               bIsTrucated,
                               osNextMarker );

            CPLFree(sWriteFuncData.pBuffer);

            if( osNextMarker.size() == 0 )
            {
                delete poS3HandleHelper;
                return osFileList.StealList();
            }
        }
    }
}

/************************************************************************/
/*                         UpdateMapFromHandle()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateMapFromHandle(VSIS3HandleHelper * poS3HandleHelper)
{
    CPLMutexHolder oHolder( &hMutex );

    oMapBucketsToS3Params[ poS3HandleHelper->GetBucket() ] =
        VSIS3UpdateParams ( poS3HandleHelper->GetAWSRegion(),
                      poS3HandleHelper->GetAWSS3Endpoint(),
                      poS3HandleHelper->GetVirtualHosting() );
}

/************************************************************************/
/*                         UpdateHandleFromMap()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateHandleFromMap(VSIS3HandleHelper * poS3HandleHelper)
{
    CPLMutexHolder oHolder( &hMutex );

    std::map< CPLString, VSIS3UpdateParams>::iterator oIter =
        oMapBucketsToS3Params.find(poS3HandleHelper->GetBucket());
    if( oIter != oMapBucketsToS3Params.end() )
    {
        poS3HandleHelper->SetAWSRegion( oIter->second.m_osAWSRegion );
        poS3HandleHelper->SetAWSS3Endpoint( oIter->second.m_osAWSS3Endpoint );
        poS3HandleHelper->SetVirtualHosting( oIter->second.m_bUseVirtualHosting );
    }
}

/************************************************************************/
/*                             VSIS3Handle()                            */
/************************************************************************/

VSIS3Handle::VSIS3Handle(VSIS3FSHandler* poFSIn, VSIS3HandleHelper* poS3HandleHelper) :
        VSICurlHandle(poFSIn, poS3HandleHelper->GetURL()),
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

struct curl_slist* VSIS3Handle::GetCurlHeaders(const CPLString& osVerb)
{
    return m_poS3HandleHelper->GetCurlHeaders(osVerb);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIS3Handle::CanRestartOnError(const char* pszErrorMsg)
{
    if( m_poS3HandleHelper->CanRestartOnError(pszErrorMsg) )
    {
        ((VSIS3FSHandler*) poFS)->UpdateMapFromHandle(m_poS3HandleHelper);

        SetURL(m_poS3HandleHelper->GetURL());
        return true;
    }
    return false;
}

/************************************************************************/
/*                    ProcessGetFileSizeResult()                        */
/************************************************************************/

void VSIS3Handle::ProcessGetFileSizeResult(const char* pszContent)
{
    bIsDirectory = strstr(pszContent, "ListBucketResult") != NULL;
}

/************************************************************************/
/*                      VSIInstallS3FileHandler()                       */
/************************************************************************/

/**
 * \brief Install /vsis3/ Amazon S3 file system handler (requires libcurl)
 *
 * A special file handler is installed that allows on-the-fly random reading of files
 * available in AWS S3 buckets, without prior download of the entire file.
 * It also allows sequential writing of files (no seeks or read operations are then
 * allowed).
 *
 * Recognized filenames are of the form /vsis3/bucket/key where
 * bucket is the name of the S3 bucket and key the S3 object "key", i.e.
 * a filename potentially containing subdirectories.
 *
 * Partial downloads are done with a 16 KB granularity by default.
 * If the driver detects sequential reading
 * it will progressively increase the chunk size up to 2 MB to improve download
 * performance.
 *
 * The AWS_SECRET_ACCESS_KEY and AWS_ACCESS_KEY_ID configuration options *must* be
 * set.
 * The AWS_REGION configuration option may be set to one of the supported
 * <a href="http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region">S3 regions</a>
 * and defaults to 'us-east-1'
 * The AWS_S3_ENDPOINT configuration option defaults to s3.amazonaws.com.
 *
 * The GDAL_HTTP_PROXY, GDAL_HTTP_PROXYUSERPWD and GDAL_PROXY_AUTH configuration options can be
 * used to define a proxy server. The syntax to use is the one of Curl CURLOPT_PROXY,
 * CURLOPT_PROXYUSERPWD and CURLOPT_PROXYAUTH options.
 *
 * On reading, the file can be cached in RAM by setting the configuration option
 * VSI_CACHE to TRUE. The cache size defaults to 25 MB, but can be modified by setting
 * the configuration option VSI_CACHE_SIZE (in bytes).
 *
 * On writing, the file is uploaded using the S3 <a href="http://docs.aws.amazon.com/AmazonS3/latest/API/mpUploadInitiate.html">multipart upload API</a>.
 * The size of chunks is set to 50 MB by default, allowing creating files up to
 * 500 GB (10000 parts of 50 MB each). If larger files are needed, then increase the
 * value of the VSIS3_CHUNK_SIZE config option to a larger value (expressed in MB).
 * In case the process is killed and the file not properly closed, the multipart upload
 * will remain open, causing Amazon to charge you for the parts storage. You'll have to
 * abort yourself with other means such "ghost" uploads
 * (e.g. with the <a href="http://s3tools.org/s3cmd">s3cmd</a> utility)
 * For files smaller than the chunk size, a simple PUT request is used instead
 * of the multipart upload API.
 *
 * VSIStatL() will return the size in st_size member.
 *
 * @since GDAL 2.1
 */
void VSIInstallS3FileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsis3/", new VSIS3FSHandler );
}



#endif /* HAVE_CURL */
