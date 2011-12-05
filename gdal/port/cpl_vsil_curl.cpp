/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for HTTP/FTP files
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

CPL_CVSID("$Id$");

#ifndef HAVE_CURL

void VSIInstallCurlFileHandler(void)
{
    /* not supported */
}

#else

#include <curl/curl.h>

#include <map>

#define ENABLE_DEBUG 1

#define N_MAX_REGIONS       1000

#define DOWNLOAD_CHUNCK_SIZE    16384

typedef enum
{
    EXIST_UNKNOWN = -1,
    EXIST_NO,
    EXIST_YES,
} ExistStatus;

typedef struct
{
    ExistStatus     eExists;
    int             bHastComputedFileSize;
    vsi_l_offset    fileSize;
    int             bIsDirectory;
    time_t          mTime;
} CachedFileProp;

typedef struct
{
    int             bGotFileList;
    char**          papszFileList; /* only file name without path */
} CachedDirList;

typedef struct
{
    unsigned long   pszURLHash;
    vsi_l_offset    nFileOffsetStart;
    size_t          nSize;
    char           *pData;
} CachedRegion;


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
    int         i;

    if( papszList == NULL )
        return -1;

    for( i = 0; papszList[i] != NULL; i++ )
    {
        const char* pszIter1 = papszList[i];
        const char* pszIter2 = pszTarget;
        char ch1, ch2;
        /* The comparison is case-sensitive, escape for escaped */
        /* sequences where letters of the hexadecimal sequence */
        /* can be uppercase or lowercase depending on the quoting algorithm */
        while(TRUE)
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


class VSICurlFilesystemHandler : public VSIFilesystemHandler 
{
    void           *hMutex;

    CachedRegion  **papsRegions;
    int             nRegions;

    std::map<CPLString, CachedFileProp*>   cacheFileSize;
    std::map<CPLString, CachedDirList*>        cacheDirList;

    int             bUseCacheDisk;

    /* Per-thread Curl connection cache */
    std::map<GIntBig, CachedConnection*> mapConnections;

    char** GetFileList(const char *pszFilename, int* pbGotFileList);

    char**              ParseHTMLFileList(const char* pszFilename,
                                          char* pszData,
                                          int* pbGotFileList);
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
    virtual char   **ReadDir( const char *pszDirname );
    virtual char   **ReadDir( const char *pszDirname, int* pbGotFileList );


    const CachedRegion* GetRegion(const char*     pszURL,
                                  vsi_l_offset    nFileOffsetStart);

    void                AddRegion(const char*     pszURL,
                                  vsi_l_offset    nFileOffsetStart,
                                  size_t          nSize,
                                  const char     *pData);

    CachedFileProp*     GetCachedFileProp(const char*     pszURL);

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
  private:
    VSICurlFilesystemHandler* poFS;

    char*           pszURL;
    unsigned long   pszURLHash;

    vsi_l_offset    curOffset;
    vsi_l_offset    fileSize;
    int             bHastComputedFileSize;
    ExistStatus     eExists;
    int             bIsDirectory;
    time_t          mTime;

    vsi_l_offset    lastDownloadedOffset;
    int             nBlocksToDownload;
    int             bEOF;

    int             DownloadRegion(vsi_l_offset startOffset, int nBlocks);

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

    int                  IsKnownFileSize() const { return bHastComputedFileSize; }
    vsi_l_offset         GetFileSize();
    int                  Exists();
    int                  IsDirectory() const { return bIsDirectory; }
    time_t               GetMTime() const { return mTime; }
};

/************************************************************************/
/*                           VSICurlHandle()                            */
/************************************************************************/

VSICurlHandle::VSICurlHandle(VSICurlFilesystemHandler* poFS, const char* pszURL)
{
    this->poFS = poFS;
    this->pszURL = CPLStrdup(pszURL);

    curOffset = 0;

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    eExists = cachedFileProp->eExists;
    fileSize = cachedFileProp->fileSize;
    bHastComputedFileSize = cachedFileProp->bHastComputedFileSize;
    bIsDirectory = cachedFileProp->bIsDirectory;
    mTime = cachedFileProp->mTime;

    lastDownloadedOffset = -1;
    nBlocksToDownload = 1;
    bEOF = FALSE;
}

/************************************************************************/
/*                          ~VSICurlHandle()                            */
/************************************************************************/

VSICurlHandle::~VSICurlHandle()
{
    CPLFree(pszURL);
}


/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICurlHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if (nWhence == SEEK_SET)
    {
        curOffset = nOffset;
        bEOF = FALSE;
    }
    else if (nWhence == SEEK_CUR)
    {
        curOffset = curOffset + nOffset;
        bEOF = FALSE;
    }
    else
    {
        curOffset = GetFileSize() + nOffset;
        bEOF = TRUE;
    }
    return 0;
}

/************************************************************************/
/*                       VSICurlSetOptions()                            */
/************************************************************************/

static void VSICurlSetOptions(CURL* hCurlHandle, const char* pszURL)
{
    curl_easy_setopt(hCurlHandle, CURLOPT_URL, pszURL);
    if (CSLTestBoolean(CPLGetConfigOption("CPL_CURL_VERBOSE", "NO")))
        curl_easy_setopt(hCurlHandle, CURLOPT_VERBOSE, 1);

    /* Set Proxy parameters */
    const char* pszProxy = CPLGetConfigOption("GDAL_HTTP_PROXY", NULL);
    if (pszProxy)
        curl_easy_setopt(hCurlHandle,CURLOPT_PROXY,pszProxy);

    const char* pszProxyUserPwd = CPLGetConfigOption("GDAL_HTTP_PROXYUSERPWD", NULL);
    if (pszProxyUserPwd)
        curl_easy_setopt(hCurlHandle,CURLOPT_PROXYUSERPWD,pszProxyUserPwd);
        
    /* Enable following redirections.  Requires libcurl 7.10.1 at least */
    curl_easy_setopt(hCurlHandle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(hCurlHandle, CURLOPT_MAXREDIRS, 10);

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

    /* NOSIGNAL should be set to true for timeout to work in multithread
    environments on Unix, requires libcurl 7.10 or more recent.
    (this force avoiding the use of sgnal handlers) */

/* 7.10 */
#if LIBCURL_VERSION_NUM >= 0x070A00
    curl_easy_setopt(hCurlHandle, CURLOPT_NOSIGNAL, 1);
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


typedef struct
{
    char*           pBuffer;
    size_t          nSize;
    int             bIsHTTP;
    int             bIsInHeader;
    int             bMultiRange;
    vsi_l_offset    nStartOffset;
    vsi_l_offset    nEndOffset;
    int             nHTTPCode;
    vsi_l_offset    nContentLength;
    int             bFoundContentRange;
    int             bError;
} WriteFuncStruct;

/************************************************************************/
/*                    VSICURLInitWriteFuncStruct()                      */
/************************************************************************/

static void VSICURLInitWriteFuncStruct(WriteFuncStruct* psStruct)
{
    psStruct->pBuffer = NULL;
    psStruct->nSize = 0;
    psStruct->bIsHTTP = FALSE;
    psStruct->bIsInHeader = TRUE;
    psStruct->bMultiRange = FALSE;
    psStruct->nStartOffset = 0;
    psStruct->nEndOffset = 0;
    psStruct->nHTTPCode = 0;
    psStruct->nContentLength = 0;
    psStruct->bFoundContentRange = FALSE;
    psStruct->bError = FALSE;
}

/************************************************************************/
/*                       VSICurlHandleWriteFunc()                       */
/************************************************************************/

static int VSICurlHandleWriteFunc(void *buffer, size_t count, size_t nmemb, void *req)
{
    WriteFuncStruct* psStruct = (WriteFuncStruct*) req;
    size_t nSize = count * nmemb;

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
            if (EQUALN(pszLine, "HTTP/1.0 ", 9) ||
                EQUALN(pszLine, "HTTP/1.1 ", 9))
                psStruct->nHTTPCode = atoi(pszLine + 9);
            else if (EQUALN(pszLine, "Content-Length: ", 16))
                psStruct->nContentLength = CPLScanUIntBig(pszLine + 16,
                                                          strlen(pszLine + 16));
            else if (EQUALN(pszLine, "Content-Range: ", 15))
                psStruct->bFoundContentRange = TRUE;

            /*if (nSize > 2 && pszLine[nSize - 2] == '\r' &&
                pszLine[nSize - 1] == '\n')
            {
                pszLine[nSize - 2] = 0;
                CPLDebug("VSICURL", "%s", pszLine);
                pszLine[nSize - 2] = '\r';
            }*/

            if (pszLine[0] == '\r' || pszLine[0] == '\n')
            {
                psStruct->bIsInHeader = FALSE;

                /* Detect servers that don't support range downloading */
                if (psStruct->nHTTPCode == 200 &&
                    !psStruct->bMultiRange &&
                    !psStruct->bFoundContentRange &&
                    (psStruct->nStartOffset != 0 || psStruct->nContentLength > 10 *
                        (psStruct->nEndOffset - psStruct->nStartOffset + 1)))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Range downloading not supported by this server !");
                    psStruct->bError = TRUE;
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

    if (bHastComputedFileSize)
        return fileSize;

    bHastComputedFileSize = TRUE;

    /* Consider that only the files whose extension ends up with one that is */
    /* listed in CPL_VSIL_CURL_ALLOWED_EXTENSIONS exist on the server */
    /* This can speeds up dramatically open experience, in case the server */
    /* cannot return a file list */
    /* For example : */
    /* gdalinfo --config CPL_VSIL_CURL_ALLOWED_EXTENSIONS ".tif" /vsicurl/http://igskmncngs506.cr.usgs.gov/gmted/Global_tiles_GMTED/075darcsec/bln/W030/30N030W_20101117_gmted_bln075.tif */
    const char* pszAllowedExtensions =
        CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", NULL);
    if (pszAllowedExtensions)
    {
        char** papszExtensions = CSLTokenizeString2( pszAllowedExtensions, ", ", 0 );
        int nURLLen = strlen(pszURL);
        int bFound = FALSE;
        for(int i=0;papszExtensions[i] != NULL;i++)
        {
            int nExtensionLen = strlen(papszExtensions[i]);
            if (nURLLen > nExtensionLen &&
                EQUAL(pszURL + nURLLen - nExtensionLen, papszExtensions[i]))
            {
                bFound = TRUE;
                break;
            }
        }

        if (!bFound)
        {
            eExists = EXIST_NO;
            fileSize = 0;

            CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
            cachedFileProp->bHastComputedFileSize = TRUE;
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
    curl_easy_setopt(hCurlHandle, CURLOPT_NOBODY, 1);
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPGET, 0); 
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADER, 1);

    /* We need that otherwise OSGEO4W's libcurl issue a dummy range request */
    /* when doing a HEAD when recycling connections */
    curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

    /* Bug with older curl versions (<=7.16.4) and FTP. See http://curl.haxx.se/mail/lib-2007-08/0312.html */
    VSICURLInitWriteFuncStruct(&sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    char szCurlErrBuf[CURL_ERROR_SIZE+1];
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    double dfSize = 0;
    curl_easy_perform(hCurlHandle);

    eExists = EXIST_UNKNOWN;

    if (strncmp(pszURL, "ftp", 3) == 0)
    {
        if (sWriteFuncData.pBuffer != NULL &&
            strncmp(sWriteFuncData.pBuffer, "Content-Length: ", strlen( "Content-Length: ")) == 0)
        {
            const char* pszBuffer = sWriteFuncData.pBuffer + strlen("Content-Length: ");
            eExists = EXIST_YES;
            fileSize = CPLScanUIntBig(pszBuffer, sWriteFuncData.nSize - strlen("Content-Length: "));
            if (ENABLE_DEBUG)
                CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB,
                        pszURL, fileSize);
        }
    }
    
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
        if (response_code != 200)
        {
            eExists = EXIST_NO;
            fileSize = 0;
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
            bIsDirectory = TRUE;
        }

        if (ENABLE_DEBUG)
            CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB "  response_code=%d",
                    pszURL, fileSize, (int)response_code);
    }

    CPLFree(sWriteFuncData.pBuffer);

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    cachedFileProp->bHastComputedFileSize = TRUE;
    cachedFileProp->fileSize = fileSize;
    cachedFileProp->eExists = eExists;
    cachedFileProp->bIsDirectory = bIsDirectory;

    return fileSize;
}

/************************************************************************/
/*                                 Exists()                             */
/************************************************************************/

int VSICurlHandle::Exists()
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

int VSICurlHandle::DownloadRegion(vsi_l_offset startOffset, int nBlocks)
{
    WriteFuncStruct sWriteFuncData;
    WriteFuncStruct sWriteFuncHeaderData;

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    if (cachedFileProp->eExists == EXIST_NO)
        return FALSE;

    CURL* hCurlHandle = poFS->GetCurlHandleFor(pszURL);
    VSICurlSetOptions(hCurlHandle, pszURL);

    VSICURLInitWriteFuncStruct(&sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, VSICurlHandleWriteFunc);
    sWriteFuncHeaderData.bIsHTTP = strncmp(pszURL, "http", 4) == 0;
    sWriteFuncHeaderData.nStartOffset = startOffset;
    sWriteFuncHeaderData.nEndOffset = startOffset + nBlocks * DOWNLOAD_CHUNCK_SIZE - 1;

    char rangeStr[512];
    sprintf(rangeStr, CPL_FRMT_GUIB "-" CPL_FRMT_GUIB, startOffset, startOffset + nBlocks * DOWNLOAD_CHUNCK_SIZE - 1);

    if (ENABLE_DEBUG)
        CPLDebug("VSICURL", "Downloading %s (%s)...", rangeStr, pszURL);

    curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, rangeStr);

    char szCurlErrBuf[CURL_ERROR_SIZE+1];
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    curl_easy_perform(hCurlHandle);

    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, NULL);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

    char *content_type = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_CONTENT_TYPE, &content_type);

    if (ENABLE_DEBUG)
        CPLDebug("VSICURL", "Got reponse_code=%ld", response_code);

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
        if (!bHastComputedFileSize && startOffset == 0)
        {
            cachedFileProp->bHastComputedFileSize = bHastComputedFileSize = TRUE;
            cachedFileProp->fileSize = fileSize = 0;
            cachedFileProp->eExists = eExists = EXIST_NO;
        }
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        return FALSE;
    }

    if (!bHastComputedFileSize && sWriteFuncHeaderData.pBuffer)
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
                    fileSize = CPLScanUIntBig(pszSlash, strlen(pszSlash));
                }
            }
        }
        else if (strncmp(pszURL, "ftp", 3) == 0)
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

                    fileSize = CPLScanUIntBig(pszSize, strlen(pszSize));
                }
            }
        }

        if (fileSize != 0)
        {
            eExists = EXIST_YES;

            if (ENABLE_DEBUG)
                CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB "  response_code=%d",
                        pszURL, fileSize, (int)response_code);

            bHastComputedFileSize = cachedFileProp->bHastComputedFileSize = TRUE;
            cachedFileProp->fileSize = fileSize;
            cachedFileProp->eExists = eExists;
        }
    }

    lastDownloadedOffset = startOffset + nBlocks * DOWNLOAD_CHUNCK_SIZE;

    char* pBuffer = sWriteFuncData.pBuffer;
    int nSize = sWriteFuncData.nSize;

    if (nSize > nBlocks * DOWNLOAD_CHUNCK_SIZE)
    {
        if (ENABLE_DEBUG)
            CPLDebug("VSICURL", "Got more data than expected : %d instead of %d",
                     nSize, nBlocks * DOWNLOAD_CHUNCK_SIZE);
    }
    
    while(nSize > 0)
    {
        //if (ENABLE_DEBUG)
        //    CPLDebug("VSICURL", "Add region %d - %d", startOffset, MIN(DOWNLOAD_CHUNCK_SIZE, nSize));
        poFS->AddRegion(pszURL, startOffset, MIN(DOWNLOAD_CHUNCK_SIZE, nSize), pBuffer);
        startOffset += DOWNLOAD_CHUNCK_SIZE;
        pBuffer += DOWNLOAD_CHUNCK_SIZE;
        nSize -= DOWNLOAD_CHUNCK_SIZE;
    }

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);

    return TRUE;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSICurlHandle::Read( void *pBuffer, size_t nSize, size_t nMemb )
{
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
                (iterOffset / DOWNLOAD_CHUNCK_SIZE) * DOWNLOAD_CHUNCK_SIZE;
            
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
                ((iterOffset + nBufferRequestSize) / DOWNLOAD_CHUNCK_SIZE) * DOWNLOAD_CHUNCK_SIZE;
            int nMinBlocksToDownload = 1 + (int)
                ((nEndOffsetToDownload - nOffsetToDownload) / DOWNLOAD_CHUNCK_SIZE);
            if (nBlocksToDownload < nMinBlocksToDownload)
                nBlocksToDownload = nMinBlocksToDownload;
                
            int i;
            /* Avoid reading already cached data */
            for(i=1;i<nBlocksToDownload;i++)
            {
                if (poFS->GetRegion(pszURL, nOffsetToDownload + i * DOWNLOAD_CHUNCK_SIZE) != NULL)
                {
                    nBlocksToDownload = i;
                    break;
                }
            }

            if (DownloadRegion(nOffsetToDownload, nBlocksToDownload) == FALSE)
            {
                bEOF = TRUE;
                return 0;
            }
            psRegion = poFS->GetRegion(pszURL, iterOffset);
        }
        if (psRegion == NULL || psRegion->pData == NULL)
        {
            bEOF = TRUE;
            return 0;
        }
        int nToCopy = (int) MIN(nBufferRequestSize, psRegion->nSize - (iterOffset - psRegion->nFileOffsetStart));
        memcpy(pBuffer, psRegion->pData + iterOffset - psRegion->nFileOffsetStart,
                nToCopy);
        pBuffer = (char*) pBuffer + nToCopy;
        iterOffset += nToCopy;
        nBufferRequestSize -= nToCopy;
        if (psRegion->nSize != DOWNLOAD_CHUNCK_SIZE && nBufferRequestSize != 0)
        {
            break;
        }
    }

    size_t ret = (size_t) ((iterOffset - curOffset) / nSize);
    if (ret != nMemb)
        bEOF = TRUE;

    curOffset = iterOffset;

    return ret;
}


/************************************************************************/
/*                           ReadMultiRange()                           */
/************************************************************************/

int VSICurlHandle::ReadMultiRange( int nRanges, void ** ppData,
                                   const vsi_l_offset* panOffsets,
                                   const size_t* panSizes )
{
    WriteFuncStruct sWriteFuncData;
    WriteFuncStruct sWriteFuncHeaderData;

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

    VSICURLInitWriteFuncStruct(&sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, VSICurlHandleWriteFunc);
    sWriteFuncHeaderData.bIsHTTP = strncmp(pszURL, "http", 4) == 0;
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

    curl_easy_perform(hCurlHandle);

    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, NULL);

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
        if (!bHastComputedFileSize && startOffset == 0)
        {
            cachedFileProp->bHastComputedFileSize = bHastComputedFileSize = TRUE;
            cachedFileProp->fileSize = fileSize = 0;
            cachedFileProp->eExists = eExists = EXIST_NO;
        }
        */
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        return -1;
    }

    char* pBuffer = sWriteFuncData.pBuffer;
    int nSize = sWriteFuncData.nSize;

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
        int nAccSize = 0;
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
        int bExpectedRange = FALSE;

        while( *pszNext != '\n' && *pszNext != '\r' && *pszNext != '\0' )
        {
            char *pszEOL = strstr(pszNext,"\n");

            if( pszEOL == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Error while parsing multipart content (at line %d)", __LINE__);
                goto end;
            }

            *pszEOL = '\0';
            int bRestoreAntislashR = FALSE;
            if (pszEOL - pszNext > 1 && pszEOL[-1] == '\r')
            {
                bRestoreAntislashR = TRUE;
                pszEOL[-1] = '\0';
            }

            if (EQUALN(pszNext, "Content-Range: bytes ", strlen("Content-Range: bytes ")))
            {
                bExpectedRange = TRUE; /* FIXME */
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

        while(TRUE)
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
        if( strncmp(pszNext,"--",2) == 0 )
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

size_t VSICurlHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
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
        int bReinitConnection = strncmp(psCachedConnection->osURL,
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
    nFileOffsetStart = (nFileOffsetStart / DOWNLOAD_CHUNCK_SIZE) * DOWNLOAD_CHUNCK_SIZE;
    VSILFILE* fp = VSIFOpenL(VSICurlGetCacheFileName(), "rb");
    if (fp)
    {
        unsigned long   pszURLHash = CPLHashSetHashStr(pszURL);
        unsigned long   pszURLHashCached;
        vsi_l_offset    nFileOffsetStartCached;
        size_t          nSizeCached;
        while(TRUE)
        {
            if (VSIFReadL(&pszURLHashCached, 1, sizeof(unsigned long), fp) == 0)
                break;
            VSIFReadL(&nFileOffsetStartCached, 1, sizeof(vsi_l_offset), fp);
            VSIFReadL(&nSizeCached, 1, sizeof(size_t), fp);
            if (pszURLHash == pszURLHashCached &&
                nFileOffsetStart == nFileOffsetStartCached)
            {
                if (ENABLE_DEBUG)
                    CPLDebug("VSICURL", "Got data at offset " CPL_FRMT_GUIB " from disk" , nFileOffsetStart);
                if (nSizeCached)
                {
                    char* pBuffer = (char*) CPLMalloc(nSizeCached);
                    VSIFReadL(pBuffer, 1, nSizeCached, fp);
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
                VSIFSeekL(fp, nSizeCached, SEEK_CUR);
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
        while(TRUE)
        {
            if (VSIFReadL(&pszURLHashCached, 1, sizeof(unsigned long), fp) == 0)
                break;
            VSIFReadL(&nFileOffsetStartCached, 1, sizeof(vsi_l_offset), fp);
            VSIFReadL(&nSizeCached, 1, sizeof(size_t), fp);
            if (psRegion->pszURLHash == pszURLHashCached &&
                psRegion->nFileOffsetStart == nFileOffsetStartCached)
            {
                CPLAssert(psRegion->nSize == nSizeCached);
                VSIFCloseL(fp);
                return;
            }
            else
            {
                VSIFSeekL(fp, nSizeCached, SEEK_CUR);
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
        VSIFWriteL(&psRegion->pszURLHash, 1, sizeof(unsigned long), fp);
        VSIFWriteL(&psRegion->nFileOffsetStart, 1, sizeof(vsi_l_offset), fp);
        VSIFWriteL(&psRegion->nSize, 1, sizeof(size_t), fp);
        if (psRegion->nSize)
            VSIFWriteL(psRegion->pData, 1, psRegion->nSize, fp);

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

    nFileOffsetStart = (nFileOffsetStart / DOWNLOAD_CHUNCK_SIZE) * DOWNLOAD_CHUNCK_SIZE;
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
        cachedFileProp->bHastComputedFileSize = FALSE;
        cachedFileProp->fileSize = 0;
        cachedFileProp->bIsDirectory = FALSE;
        cacheFileSize[pszURL] = cachedFileProp;
    }

    return cachedFileProp;
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
    int bSkipReadDir = EQUAL(pszOptionVal, "EMPTY_DIR") ||
                       CSLTestBoolean(pszOptionVal);

    CPLString osFilename(pszFilename);
    int bGotFileList = TRUE;
    if (strchr(CPLGetFilename(osFilename), '.') != NULL &&
        strncmp(CPLGetExtension(osFilename), "zip", 3) != 0 && !bSkipReadDir)
    {
        char** papszFileList = ReadDir(CPLGetDirname(osFilename), &bGotFileList);
        int bFound = (VSICurlIsFileInList(papszFileList, CPLGetFilename(osFilename)) != -1);
        CSLDestroy(papszFileList);
        if (bGotFileList && !bFound)
        {
            return NULL;
        }
    }

    VSICurlHandle* poHandle = new VSICurlHandle( this, osFilename + strlen("/vsicurl/"));
    if (!bGotFileList)
    {
        /* If we didn't get a filelist, check that the file really exists */
        if (!poHandle->Exists())
        {
            delete poHandle;
            poHandle = NULL;
        }
    }
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
    while( *pszData != '\0' && *pszData != '\n' && !EQUALN(pszData,"<br>",4) )
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

static int VSICurlParseHTMLDateTimeFileSize(const char* pszStr,
                                            struct tm& brokendowntime,
                                            GUIntBig& nFileSize,
                                            GIntBig& mTime)
{
    int iMonth;
    for(iMonth=0;iMonth<12;iMonth++)
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

                    return TRUE;
                }
            }
            return FALSE;
        }

        /* Microsoft IIS */
        szMonth[0] = ' ';
        strcpy(szMonth + 1, apszMonths[iMonth]);
        strcat(szMonth, " ");
        pszMonthFound = strstr(pszStr, szMonth);
        if (pszMonthFound)
        {
            int nLenMonth = strlen(apszMonths[iMonth]);
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
                if (strncmp(pszMonthFound + nCurOffset, "p.m.", 4) == 0)
                    nHour += 12;
                else if (strncmp(pszMonthFound + nCurOffset, "a.m.", 4) != 0)
                    nHour = -1;
                nCurOffset += 4;

                const char* pszFilesize = pszMonthFound + nCurOffset;
                while(*pszFilesize == ' ')
                    pszFilesize ++;
                if (*pszFilesize >= '1' && *pszFilesize <= '9')
                    nFileSize = CPLScanUIntBig(pszFilesize, strlen(pszFilesize));

                if (nDay >= 1 && nDay <= 31 && nYear >= 1900 &&
                    nHour >= 0 && nHour <= 24 && nMin >= 0 && nMin < 60)
                {
                    brokendowntime.tm_year = nYear - 1900;
                    brokendowntime.tm_mon = iMonth;
                    brokendowntime.tm_mday = nDay;
                    brokendowntime.tm_hour = nHour;
                    brokendowntime.tm_min = nMin;
                    mTime = CPLYMDHMSToUnixTime(&brokendowntime);

                    return TRUE;
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
                if (strncmp(pszMonthFound + nCurOffset, "PM", 2) == 0)
                    nHour += 12;
                else if (strncmp(pszMonthFound + nCurOffset, "AM", 2) != 0)
                    nHour = -1;
                nCurOffset += 2;

                const char* pszFilesize = pszMonthFound + nCurOffset;
                while(*pszFilesize == ' ')
                    pszFilesize ++;
                if (*pszFilesize >= '1' && *pszFilesize <= '9')
                    nFileSize = CPLScanUIntBig(pszFilesize, strlen(pszFilesize));

                if (nDay >= 1 && nDay <= 31 && nYear >= 1900 &&
                    nHour >= 0 && nHour <= 24 && nMin >= 0 && nMin < 60)
                {
                    brokendowntime.tm_year = nYear - 1900;
                    brokendowntime.tm_mon = iMonth;
                    brokendowntime.tm_mday = nDay;
                    brokendowntime.tm_hour = nHour;
                    brokendowntime.tm_min = nMin;
                    mTime = CPLYMDHMSToUnixTime(&brokendowntime);

                    return TRUE;
                }
                nFileSize = 0;
            }
            return FALSE;
        }
    }

    return FALSE;
}

/************************************************************************/
/*                          ParseHTMLFileList()                         */
/*                                                                      */
/*      Parse a file list document and return all the components.       */
/************************************************************************/

char** VSICurlFilesystemHandler::ParseHTMLFileList(const char* pszFilename,
                                       char* pszData,
                                       int* pbGotFileList)
{
    CPLStringList oFileList;
    char* pszLine = pszData;
    char* c;
    int nCount = 0;
    int bIsHTMLDirList = FALSE;
    CPLString osExpectedString;
    CPLString osExpectedString2;
    CPLString osExpectedString3;
    CPLString osExpectedString4;
    CPLString osExpectedString_unescaped;
    
    *pbGotFileList = FALSE;

    const char* pszDir;
    if (EQUALN(pszFilename, "/vsicurl/http://", strlen("/vsicurl/http://")))
        pszDir = strchr(pszFilename + strlen("/vsicurl/http://"), '/');
    else if (EQUALN(pszFilename, "/vsicurl/https://", strlen("/vsicurl/https://")))
        pszDir = strchr(pszFilename + strlen("/vsicurl/https://"), '/');
    else
        pszDir = strchr(pszFilename + strlen("/vsicurl/ftp://"), '/');
    if (pszDir == NULL)
        pszDir = "";
    /* Apache */
    osExpectedString = "<title>Index of ";
    osExpectedString += pszDir;
    osExpectedString += "</title>";
    /* shttpd */
    osExpectedString2 = "<title>Index of ";
    osExpectedString2 += pszDir;
    osExpectedString2 += "/</title>";
    /* FTP */
    osExpectedString3 = "FTP Listing of ";
    osExpectedString3 += pszDir;
    osExpectedString3 += "/";
    /* Apache 1.3.33 */
    osExpectedString4 = "<TITLE>Index of ";
    osExpectedString4 += pszDir;
    osExpectedString4 += "</TITLE>";

    /* The listing of http://dds.cr.usgs.gov/srtm/SRTM_image_sample/picture%20examples/ */
    /* has "<title>Index of /srtm/SRTM_image_sample/picture examples</title>" so we must */
    /* try unescaped %20 also */
    /* Similar with http://datalib.usask.ca/gis/Data/Central_America_goodbutdoweown%3f/ */
    if (strchr(pszDir, '%'))
    {
        char* pszUnescapedDir = CPLUnescapeString(pszDir, NULL, CPLES_URL);
        osExpectedString_unescaped = "<title>Index of ";
        osExpectedString_unescaped += pszUnescapedDir;
        osExpectedString_unescaped += "</title>";
        CPLFree(pszUnescapedDir);
    }

    int nCountTable = 0;
    
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
                *pbGotFileList = FALSE;
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
            bIsHTMLDirList = TRUE;
            *pbGotFileList = TRUE;
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
                        bIsHTMLDirList = TRUE;
                        *pbGotFileList = TRUE;
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
            if (endQuote && strncmp(beginFilename, "?C=", 3) != 0 && strncmp(beginFilename, "?N=", 3) != 0)
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
                int bIsDirectory = FALSE;
                if (endQuote[-1] == '/')
                {
                    bIsDirectory = TRUE;
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
                    cachedFileProp->mTime = mTime;
                    cachedFileProp->bHastComputedFileSize = nFileSize > 0;
                    cachedFileProp->fileSize = nFileSize;

                    oFileList.AddString( beginFilename );
                    if (ENABLE_DEBUG)
                        CPLDebug("VSICURL", "File[%d] = %s, is_dir = %d, size = " CPL_FRMT_GUIB ", time = %04d/%02d/%02d %02d:%02d:%02d",
                                nCount, beginFilename, bIsDirectory, nFileSize,
                                brokendowntime.tm_year + 1900, brokendowntime.tm_mon + 1, brokendowntime.tm_mday,
                                brokendowntime.tm_hour, brokendowntime.tm_min, brokendowntime.tm_sec);
                    nCount ++;
                }
            }
        }
        pszLine = c + 1;
    }

    return oFileList.StealList();
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

static int VSICurlParseFullFTPLine(char* pszLine,
                                   char*& pszFilename,
                                   int& bSizeValid,
                                   GUIntBig& nSize,
                                   int& bIsDirectory,
                                   GIntBig& nUnixTime)
{
    char* pszNextToken = pszLine;
    char* pszPermissions = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszPermissions == NULL || strlen(pszPermissions) != 10)
        return FALSE;
    bIsDirectory = (pszPermissions[0] == 'd');

    int i;
    for(i = 0; i < 3; i++)
    {
        if (VSICurlGetToken(pszNextToken, &pszNextToken) == NULL)
            return FALSE;
    }

    char* pszSize = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszSize == NULL)
        return FALSE;

    if (pszPermissions[0] == '-')
    {
        /* Regular file */
        bSizeValid = TRUE;
        nSize = CPLScanUIntBig(pszSize, strlen(pszSize));
    }

    struct tm brokendowntime;
    memset(&brokendowntime, 0, sizeof(brokendowntime));
    int bBrokenDownTimeValid = TRUE;

    char* pszMonth = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszMonth == NULL || strlen(pszMonth) != 3)
        return FALSE;

    for(i = 0; i < 12; i++)
    {
        if (EQUALN(pszMonth, apszMonths[i], 3))
            break;
    }
    if (i < 12)
        brokendowntime.tm_mon = i;
    else
        bBrokenDownTimeValid = FALSE;

    char* pszDay = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszDay == NULL || (strlen(pszDay) != 1 && strlen(pszDay) != 2))
        return FALSE;
    int nDay = atoi(pszDay);
    if (nDay >= 1 && nDay <= 31)
        brokendowntime.tm_mday = nDay;
    else
        bBrokenDownTimeValid = FALSE;

    char* pszHourOrYear = VSICurlGetToken(pszNextToken, &pszNextToken);
    if (pszHourOrYear == NULL || (strlen(pszHourOrYear) != 4 && strlen(pszHourOrYear) != 5))
        return FALSE;
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
        return FALSE;

    pszFilename = pszNextToken;

    char* pszCurPtr = pszFilename;
    while( *pszCurPtr != '\0')
    {
        /* In case of a link, stop before the pointed part of the link */
        if (pszPermissions[0] == 'l' && strncmp(pszCurPtr, " -> ", 4) == 0)
        {
            break;
        }
        pszCurPtr ++;
    }
    *pszCurPtr = '\0';

    return TRUE;
}

/************************************************************************/
/*                          GetFileList()                               */
/************************************************************************/

char** VSICurlFilesystemHandler::GetFileList(const char *pszDirname, int* pbGotFileList)
{
    if (ENABLE_DEBUG)
        CPLDebug("VSICURL", "GetFileList(%s)" , pszDirname);

    *pbGotFileList = FALSE;

    if (strncmp(pszDirname, "/vsicurl/ftp", strlen("/vsicurl/ftp")) == 0)
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
            /* informations (filename, file/directory, size). If that */
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

            VSICURLInitWriteFuncStruct(&sWriteFuncData);
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

            if (EQUALN(pszLine, "<!DOCTYPE HTML", strlen("<!DOCTYPE HTML")) ||
                EQUALN(pszLine, "<HTML>", 6))
            {
                papszFileList = ParseHTMLFileList(pszDirname,
                                                  sWriteFuncData.pBuffer,
                                                  pbGotFileList);
                break;
            }
            else if (iTry == 0)
            {
                CPLStringList oFileList;
                *pbGotFileList = TRUE;

                while( (c = strchr(pszLine, '\n')) != NULL)
                {
                    *c = 0;
                    if (c - pszLine > 0 && c[-1] == '\r')
                        c[-1] = 0;

                    char* pszFilename = NULL;
                    int bSizeValid = FALSE;
                    GUIntBig nFileSize = 0;
                    int bIsDirectory = FALSE;
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
                        cachedFileProp->bHastComputedFileSize = bSizeValid;
                        cachedFileProp->fileSize = nFileSize;
                        cachedFileProp->bIsDirectory = bIsDirectory;
                        cachedFileProp->mTime = mUnixTime;

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
                *pbGotFileList = TRUE;

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
    else if (strncmp(pszDirname, "/vsicurl/http://", strlen("/vsicurl/http://")) == 0 ||
             strncmp(pszDirname, "/vsicurl/https://", strlen("/vsicurl/https://")) == 0)
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

        VSICURLInitWriteFuncStruct(&sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1];
        szCurlErrBuf[0] = '\0';
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        curl_easy_perform(hCurlHandle);

        if (sWriteFuncData.pBuffer == NULL)
            return NULL;
            
        char** papszFileList = ParseHTMLFileList(pszDirname,
                                                 sWriteFuncData.pBuffer,
                                                 pbGotFileList);

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
    CPLString osFilename(pszFilename);
    
    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    const char* pszOptionVal =
        CPLGetConfigOption( "GDAL_DISABLE_READDIR_ON_OPEN", "NO" );
    int bSkipReadDir = EQUAL(pszOptionVal, "EMPTY_DIR") ||
                       CSLTestBoolean(pszOptionVal);

    /* Does it look like a FTP directory ? */
    if (strncmp(osFilename, "/vsicurl/ftp", strlen("/vsicurl/ftp")) == 0 &&
        pszFilename[strlen(osFilename) - 1] == '/' && !bSkipReadDir)
    {
        char** papszFileList = ReadDir(osFilename);
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
             strncmp(CPLGetExtension(osFilename), "zip", 3) != 0 &&
             !bSkipReadDir)
    {
        int bGotFileList;
        char** papszFileList = ReadDir(CPLGetDirname(osFilename), &bGotFileList);
        int bFound = (VSICurlIsFileInList(papszFileList, CPLGetFilename(osFilename)) != -1);
        CSLDestroy(papszFileList);
        if (bGotFileList && !bFound)
        {
            return -1;
        }
    }

    VSICurlHandle oHandle( this, osFilename + strlen("/vsicurl/"));

    if ( oHandle.IsKnownFileSize() ||
         ((nFlags & VSI_STAT_SIZE_FLAG) && !oHandle.IsDirectory() &&
           CSLTestBoolean(CPLGetConfigOption("CPL_VSIL_CURL_SLOW_GET_SIZE", "YES"))) )
        pStatBuf->st_size = oHandle.GetFileSize();

    int nRet = (oHandle.Exists()) ? 0 : -1;
    pStatBuf->st_mtime = oHandle.GetMTime();
    pStatBuf->st_mode = oHandle.IsDirectory() ? S_IFDIR : S_IFREG;
    return nRet;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSICurlFilesystemHandler::Unlink( const char *pszFilename )
{
    return -1;
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSICurlFilesystemHandler::Rename( const char *oldpath, const char *newpath )
{
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSICurlFilesystemHandler::Mkdir( const char *pszDirname, long nMode )
{
    return -1;
}
/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSICurlFilesystemHandler::Rmdir( const char *pszDirname )
{
    return -1;
}

/************************************************************************/
/*                             ReadDir()                                */
/************************************************************************/

char** VSICurlFilesystemHandler::ReadDir( const char *pszDirname, int* pbGotFileList )
{
    CPLString osDirname(pszDirname);
    while (osDirname[strlen(osDirname) - 1] == '/')
        osDirname.erase(strlen(osDirname) - 1);

    const char* pszUpDir = strstr(osDirname, "/..");
    if (pszUpDir != NULL)
    {
        int pos = pszUpDir - osDirname.c_str() - 1;
        while(pos >= 0 && osDirname[pos] != '/')
            pos --;
        if (pos >= 1)
        {
            osDirname = osDirname.substr(0, pos) + CPLString(pszUpDir + 3);
        }
    }

    CPLMutexHolder oHolder( &hMutex );

    /* If we know the file exists and is not a directory, then don't try to list its content */
    CachedFileProp* cachedFileProp = GetCachedFileProp(osDirname.c_str() + strlen("/vsicurl/"));
    if (cachedFileProp->eExists == EXIST_YES && !cachedFileProp->bIsDirectory)
    {
        if (pbGotFileList)
            *pbGotFileList = TRUE;
        return NULL;
    }

    CachedDirList* psCachedDirList = cacheDirList[osDirname];
    if (psCachedDirList == NULL)
    {
        psCachedDirList = (CachedDirList*) CPLMalloc(sizeof(CachedDirList));
        psCachedDirList->papszFileList = GetFileList(osDirname, &psCachedDirList->bGotFileList);
        cacheDirList[osDirname] = psCachedDirList;
    }

    if (pbGotFileList)
        *pbGotFileList = psCachedDirList->bGotFileList;

    return CSLDuplicate(psCachedDirList->papszFileList);
}

/************************************************************************/
/*                             ReadDir()                                */
/************************************************************************/

char** VSICurlFilesystemHandler::ReadDir( const char *pszDirname )
{
    return ReadDir(pszDirname, NULL);
}

/************************************************************************/
/*                   VSIInstallCurlFileHandler()                        */
/************************************************************************/

/**
 * \brief Install /vsicurl/ HTTP/FTP file system handler (requires libcurl)
 *
 * A special file handler is installed that allows reading on-the-fly of files
 * available through HTTP/FTP web protocols, without downloading the entire file.
 *
 * Recognized filenames are of the form /vsicurl/http://path/to/remote/ressource or
 * /vsicurl/ftp://path/to/remote/ressource where path/to/remote/ressource is the
 * URL of a remote ressource.
 *
 * Partial downloads (requires the HTTP server to support random reading) are done
 * with a 16 KB granularity by default. If the driver detects sequential reading
 * it will progressively increase the chunk size up to 2 MB to improve download
 * performance.
 *
 * The GDAL_HTTP_PROXY and GDAL_HTTP_PROXYUSERPWD configuration options can be
 * used to define a proxy server. The syntax to use is the one of Curl CURLOPT_PROXY
 * and CURLOPT_PROXYUSERPWD options.
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


#endif /* HAVE_CURL */
