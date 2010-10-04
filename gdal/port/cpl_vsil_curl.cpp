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
/*                     VSICurlFilesystemHandler                         */
/************************************************************************/


class VSICurlFilesystemHandler : public VSIFilesystemHandler 
{
    void           *hMutex;

    CachedRegion  **papsRegions;
    int             nRegions;

    std::map<unsigned long, CachedFileProp*>   cacheFileSize;
    std::map<CPLString, CachedDirList*>        cacheDirList;

    int             bUseCacheDisk;

public:
    VSICurlFilesystemHandler();
    ~VSICurlFilesystemHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf );
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
    vsi_l_offset    lastCurlOffset;
    CURL*           hCurlHandle;


    vsi_l_offset    lastDownloadedOffset;
    int             nBlocksToDownload;

    int             DownloadRegion(vsi_l_offset startOffset, int nBlocks);

  public:

    VSICurlHandle(VSICurlFilesystemHandler* poFS, const char* pszURL);
    ~VSICurlHandle();

    virtual int          Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t       Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t       Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int          Eof();
    virtual int          Flush();
    virtual int          Close();

    vsi_l_offset         GetFileSize();
    int                  Exists();
};

/************************************************************************/
/*                           VSICurlHandle()                            */
/************************************************************************/

VSICurlHandle::VSICurlHandle(VSICurlFilesystemHandler* poFS, const char* pszURL)
{
    this->poFS = poFS;
    this->pszURL = CPLStrdup(pszURL);

    curOffset = 0;
    fileSize = 0;
    bHastComputedFileSize = FALSE;
    eExists = EXIST_UNKNOWN;
    hCurlHandle = NULL;

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    if (cachedFileProp)
    {
        eExists = cachedFileProp->eExists;
        fileSize = cachedFileProp->fileSize;
        bHastComputedFileSize = cachedFileProp->bHastComputedFileSize;
    }

    lastDownloadedOffset = -1;
    nBlocksToDownload = 1;
}

/************************************************************************/
/*                          ~VSICurlHandle()                            */
/************************************************************************/

VSICurlHandle::~VSICurlHandle()
{
    CPLFree(pszURL);
    if (hCurlHandle)
        curl_easy_cleanup(hCurlHandle );
}


/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICurlHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if (nWhence == SEEK_SET)
        curOffset = nOffset;
    else if (nWhence == SEEK_CUR)
        curOffset = curOffset + nOffset;
    else
    {
        curOffset = GetFileSize() + nOffset;
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

}


typedef struct
{
    char*           pBuffer;
    size_t          nSize;
} WriteFuncStruct;

/************************************************************************/
/*                    VSICURLInitWriteFuncStruct()                      */
/************************************************************************/

static void VSICURLInitWriteFuncStruct(WriteFuncStruct* psStruct)
{
    psStruct->pBuffer = NULL;
    psStruct->nSize = 0;
}

/************************************************************************/
/*                       VSICurlHandleWriteFunc()                       */
/************************************************************************/

static int VSICurlHandleWriteFunc(void *buffer, size_t count, size_t nmemb, void *req)
{
    WriteFuncStruct* psStruct = (WriteFuncStruct*) req;
    size_t nSize = count * nmemb;

    char* pNewBuffer = (char*) VSIRealloc(psStruct->pBuffer, psStruct->nSize + nSize + 1);
    if (pNewBuffer)
    {
        psStruct->pBuffer = pNewBuffer;
        memcpy(psStruct->pBuffer + psStruct->nSize, buffer, nSize);
        psStruct->pBuffer[psStruct->nSize + nSize] = '\0';
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

    CURL* hCurlHandle = curl_easy_init();

    VSICurlSetOptions(hCurlHandle, pszURL);
    curl_easy_setopt(hCurlHandle, CURLOPT_NOBODY, 1);

    /* Bug with older curl versions (<=7.16.4) and FTP. See http://curl.haxx.se/mail/lib-2007-08/0312.html */
    VSICURLInitWriteFuncStruct(&sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    double dfSize = 0;
    curl_easy_perform(hCurlHandle);

    if (strncmp(pszURL, "ftp", 3) == 0)
    {
        if (sWriteFuncData.pBuffer != NULL &&
            strncmp(sWriteFuncData.pBuffer, "Content-Length: ", strlen( "Content-Length: ")) == 0)
        {
            const char* pszBuffer = sWriteFuncData.pBuffer + strlen("Content-Length: ");
            eExists = EXIST_YES;
            fileSize = CPLScanUIntBig(pszBuffer, sWriteFuncData.nSize - strlen("Content-Length: "));
        }
        else
        {
            eExists = EXIST_NO;
            fileSize = 0;
        }

        if (ENABLE_DEBUG)
            CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB,
                    pszURL, fileSize);
    }
    else
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
        curl_easy_getinfo(hCurlHandle, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200)
        {
            eExists = EXIST_NO;
            fileSize = 0;
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

    curl_easy_cleanup(hCurlHandle );

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

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    if (cachedFileProp->eExists == EXIST_NO)
        return FALSE;

    if ( hCurlHandle == NULL)
    {
        hCurlHandle = curl_easy_init();
        VSICurlSetOptions(hCurlHandle, pszURL);
    }

    VSICURLInitWriteFuncStruct(&sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

    char rangeStr[512];
    sprintf(rangeStr, CPL_FRMT_GUIB "-" CPL_FRMT_GUIB, startOffset, startOffset + nBlocks * DOWNLOAD_CHUNCK_SIZE - 1);

    if (ENABLE_DEBUG)
        CPLDebug("VSICURL", "Downloading %s (%s)...", rangeStr, pszURL);

    curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, rangeStr);

    curl_easy_perform(hCurlHandle);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_RESPONSE_CODE, &response_code);

    char *content_type = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_CONTENT_TYPE, &content_type);

    if (response_code != 200 && response_code != 206 &&
        response_code != 226 && response_code != 426)
    {
        CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
        cachedFileProp->bHastComputedFileSize = TRUE;
        cachedFileProp->fileSize = 0;
        cachedFileProp->eExists = EXIST_NO;
        CPLFree(sWriteFuncData.pBuffer);
        return FALSE;
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

    vsi_l_offset iterOffset = curOffset;
    while (nBufferRequestSize)
    {
        const CachedRegion* psRegion = poFS->GetRegion(pszURL, iterOffset);
        if (psRegion == NULL)
        {
            vsi_l_offset nOffsetToDownload = (iterOffset / DOWNLOAD_CHUNCK_SIZE) * DOWNLOAD_CHUNCK_SIZE;
            if (nOffsetToDownload == lastDownloadedOffset)
            {
                if (nBlocksToDownload < 100)
                    nBlocksToDownload *= 2;
            }
            else
                nBlocksToDownload = 1;
            if (DownloadRegion(nOffsetToDownload, nBlocksToDownload) == FALSE)
                return 0;
            psRegion = poFS->GetRegion(pszURL, iterOffset);
        }
        if (psRegion == NULL || psRegion->pData == NULL)
            return 0;
        int nToCopy = MIN(nBufferRequestSize, psRegion->nSize - (iterOffset - psRegion->nFileOffsetStart));
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

    size_t ret = (iterOffset - curOffset) / nSize;

    curOffset = iterOffset;

    return ret;
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
    return curOffset == GetFileSize();
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

    std::map<unsigned long, CachedFileProp*>::const_iterator iterCacheFileSize;

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

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
}


/************************************************************************/
/*                   GetRegionFromCacheDisk()                           */
/************************************************************************/

const CachedRegion* 
VSICurlFilesystemHandler::GetRegionFromCacheDisk(const char* pszURL,
                                                 vsi_l_offset nFileOffsetStart)
{
    nFileOffsetStart = (nFileOffsetStart / DOWNLOAD_CHUNCK_SIZE) * DOWNLOAD_CHUNCK_SIZE;
    FILE* fp = VSIFOpenL(VSICurlGetCacheFileName(), "rb");
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
    FILE* fp = VSIFOpenL(VSICurlGetCacheFileName(), "r+b");
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

    unsigned long   pszURLHash = CPLHashSetHashStr(pszURL);
    CachedFileProp* cachedFileProp = cacheFileSize[pszURLHash];
    if (cachedFileProp == NULL)
    {
        cachedFileProp = (CachedFileProp*) CPLMalloc(sizeof(CachedFileProp));
        cachedFileProp->eExists = EXIST_UNKNOWN;
        cachedFileProp->bHastComputedFileSize = FALSE;
        cachedFileProp->fileSize = 0;
        cacheFileSize[pszURLHash] = cachedFileProp;
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

    CPLString osFilename(pszFilename);
    if (strchr(CPLGetFilename(osFilename), '.') != NULL)
    {
        int bGotFileList;
        char** papszFileList = ReadDir(CPLGetDirname(osFilename), &bGotFileList);
        int bFound = (CSLFindString(papszFileList, CPLGetFilename(osFilename)) != -1);
        CSLDestroy(papszFileList);
        if (bGotFileList && !bFound)
        {
            return NULL;
        }
    }

    return new VSICurlHandle( this, osFilename + strlen("/vsicurl/"));
}


static char** VSICurlGetFileList(const char *pszFilename, int* pbGotFileList)
{
    if (ENABLE_DEBUG)
        CPLDebug("VSICURL", "GetFileList(%s)" , pszFilename);

    *pbGotFileList = FALSE;

    if (strncmp(pszFilename, "/vsicurl/ftp", strlen("/vsicurl/ftp")) == 0)
    {
        WriteFuncStruct sWriteFuncData;

        CURL* hCurlHandle = curl_easy_init();

        CPLString osFilename(pszFilename + strlen("/vsicurl/"));
        osFilename += '/';

        VSICurlSetOptions(hCurlHandle, osFilename.c_str());

/* 7.16.4 */
#if LIBCURL_VERSION_NUM <= 0x071004
        curl_easy_setopt(hCurlHandle, CURLOPT_FTPLISTONLY, 1);
#elif LIBCURL_VERSION_NUM > 0x071004
        curl_easy_setopt(hCurlHandle, CURLOPT_DIRLISTONLY, 1);
#endif
        VSICURLInitWriteFuncStruct(&sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

        curl_easy_perform(hCurlHandle);

        curl_easy_cleanup(hCurlHandle);

        if (sWriteFuncData.pBuffer == NULL)
            return NULL;

        char** papszFileList = NULL;
        char* iter = sWriteFuncData.pBuffer;
        char* c;
        int nCount = 0;
        while( (c = strchr(iter, '\n')) != NULL)
        {
            *c = 0;
            papszFileList = CSLAddString(papszFileList, iter);
            if (ENABLE_DEBUG)
                CPLDebug("VSICURL", "File[%d] = %s", nCount, iter);
            iter = c + 1;
            nCount ++;
        }

        *pbGotFileList = TRUE;

        CPLFree(sWriteFuncData.pBuffer);
        return papszFileList;
    }

    /* Try to recognize HTML pages that list the content of a directory */
    /* Currently this supports what Apache and shttpd can return */
    else if (strncmp(pszFilename, "/vsicurl/http://", strlen("/vsicurl/http://")) == 0 &&
             strchr(pszFilename + strlen("/vsicurl/http://"), '/') != NULL)
    {
        WriteFuncStruct sWriteFuncData;

        CURL* hCurlHandle = curl_easy_init();

        CPLString osFilename(pszFilename + strlen("/vsicurl/"));
        osFilename += '/';

        VSICurlSetOptions(hCurlHandle, osFilename.c_str());

        VSICURLInitWriteFuncStruct(&sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, VSICurlHandleWriteFunc);

        curl_easy_perform(hCurlHandle);

        curl_easy_cleanup(hCurlHandle);

        if (sWriteFuncData.pBuffer == NULL)
            return NULL;

        char** papszFileList = NULL;
        char* iter = sWriteFuncData.pBuffer;
        char* c;
        int nCount = 0;
        int bIsHTMLDirList = FALSE;
        CPLString osExpectedString;
        CPLString osExpectedString2;
        /* Apache */
        osExpectedString = "<title>Index of ";
        osExpectedString += strchr(pszFilename + strlen("/vsicurl/http://"), '/');
        osExpectedString += "</title>";
        /* shttpd */
        osExpectedString2 = "<title>Index of ";
        osExpectedString2 += strchr(pszFilename + strlen("/vsicurl/http://"), '/');
        osExpectedString2 += "/</title>";
        while( (c = strchr(iter, '\n')) != NULL)
        {
            *c = 0;
            if (strstr(iter, osExpectedString.c_str()) || strstr(iter, osExpectedString2.c_str()))
            {
                bIsHTMLDirList = TRUE;
                *pbGotFileList = TRUE;
            }
            else if (bIsHTMLDirList && strstr(iter, "<a href=\""))
            {
                char *beginFilename = strstr(iter, "<a href=\"") + strlen("<a href=\"");
                char *endQuote = strchr(beginFilename, '"');
                if (endQuote && strncmp(beginFilename, "?C=", 3) != 0)
                {
                    *endQuote = '\0';
                    /* shttpd links include slashes from the root directory. Skip them */
                    while(strchr(beginFilename, '/'))
                        beginFilename = strchr(beginFilename, '/') + 1;
                    papszFileList = CSLAddString(papszFileList, beginFilename);
                    if (ENABLE_DEBUG)
                        CPLDebug("VSICURL", "File[%d] = %s", nCount, beginFilename);
                    nCount ++;
                }
            }
            iter = c + 1;
        }
        CPLFree(sWriteFuncData.pBuffer);
        return papszFileList;
    }

    return NULL;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSICurlFilesystemHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf )
{
    CPLString osFilename(pszFilename);

    /* Does it look like a FTP directory ? */
    if (strncmp(osFilename, "/vsicurl/ftp", strlen("/vsicurl/ftp")) == 0 &&
        pszFilename[strlen(osFilename) - 1] == '/')
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
    else if (strchr(CPLGetFilename(osFilename), '.') != NULL)
    {
        int bGotFileList;
        char** papszFileList = ReadDir(CPLGetDirname(osFilename), &bGotFileList);
        int bFound = (CSLFindString(papszFileList, CPLGetFilename(osFilename)) != -1);
        CSLDestroy(papszFileList);
        if (bGotFileList && !bFound)
        {
            return -1;
        }
    }

    VSICurlHandle oHandle( this, osFilename + strlen("/vsicurl/"));

    pStatBuf->st_mode = S_IFREG;
    pStatBuf->st_size = oHandle.GetFileSize();

    return (oHandle.Exists()) ? 0 : -1;
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

    CachedDirList* psCachedDirList = cacheDirList[osDirname];
    if (psCachedDirList == NULL)
    {
        psCachedDirList = (CachedDirList*) CPLMalloc(sizeof(CachedDirList));
        psCachedDirList->papszFileList = VSICurlGetFileList(osDirname, &psCachedDirList->bGotFileList);
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

void VSIInstallCurlFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsicurl/", new VSICurlFilesystemHandler );
}


#endif /* HAVE_CURL */
