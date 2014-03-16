/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for stdin
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_vsi_virtual.h"

#include <stdio.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

CPL_CVSID("$Id$");

/* We buffer the first 1MB of standard input to enable drivers */
/* to autodetect data. In the first MB, backward and forward seeking */
/* is allowed, after only forward seeking will work */
#define BUFFER_SIZE (1024 * 1024)

static GByte* pabyBuffer;
static GUInt32 nBufferLen;
static GUIntBig nRealPos;

/************************************************************************/
/*                           VSIStdinInit()                             */
/************************************************************************/

static void VSIStdinInit()
{
    if (pabyBuffer == NULL)
    {
#ifdef WIN32
        setmode( fileno( stdin ), O_BINARY );
#endif
        pabyBuffer = (GByte*)CPLMalloc(BUFFER_SIZE);
    }
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIStdinFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

class VSIStdinFilesystemHandler : public VSIFilesystemHandler
{
public:
                              VSIStdinFilesystemHandler();
    virtual                  ~VSIStdinFilesystemHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int               Stat( const char *pszFilename,
                                    VSIStatBufL *pStatBuf, int nFlags );
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIStdinHandle                               */
/* ==================================================================== */
/************************************************************************/

class VSIStdinHandle : public VSIVirtualHandle
{
  private:
    GUIntBig nCurOff;
    int               ReadAndCache( void* pBuffer, int nToRead );

  public:
                      VSIStdinHandle();
    virtual          ~VSIStdinHandle();

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Close();
};

/************************************************************************/
/*                           VSIStdinHandle()                           */
/************************************************************************/

VSIStdinHandle::VSIStdinHandle()
{
    nCurOff = 0;
}

/************************************************************************/
/*                          ~VSIStdinHandle()                           */
/************************************************************************/

VSIStdinHandle::~VSIStdinHandle()
{
}


/************************************************************************/
/*                              ReadAndCache()                          */
/************************************************************************/

int VSIStdinHandle::ReadAndCache( void* pBuffer, int nToRead )
{
    CPLAssert(nCurOff == nRealPos);

    int nRead = fread(pBuffer, 1, nToRead, stdin);

    if (nRealPos < BUFFER_SIZE)
    {
        int nToCopy = MIN(BUFFER_SIZE - (int)nRealPos, nRead);
        memcpy(pabyBuffer + nRealPos, pBuffer, nToCopy);
        nBufferLen += nToCopy;
    }

    nCurOff += nRead;
    nRealPos = nCurOff;

    return nRead;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIStdinHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    if (nWhence == SEEK_SET && nOffset == nCurOff)
        return 0;

    VSIStdinInit();
    if (nBufferLen == 0)
        nRealPos = nBufferLen = fread(pabyBuffer, 1, BUFFER_SIZE, stdin);

    if (nWhence == SEEK_END)
    {
        if (nOffset != 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Seek(xx != 0, SEEK_END) unsupported on /vsistdin");
            return -1;
        }

        if (nBufferLen < BUFFER_SIZE)
        {
            nCurOff = nBufferLen;
            return 0;
        }

        CPLError(CE_Failure, CPLE_NotSupported,
                 "Seek(SEEK_END) unsupported on /vsistdin when stdin > 1 MB");
        return -1;
    }

    if (nWhence == SEEK_CUR)
        nOffset += nCurOff;

    if (nRealPos > nBufferLen && nOffset < nRealPos)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "backward Seek() unsupported on /vsistdin above first MB");
        return -1;
    }

    if (nOffset < nBufferLen)
    {
        nCurOff = nOffset;
        return 0;
    }

    if (nOffset == nCurOff)
        return 0;

    CPLDebug("VSI", "Forward seek from " CPL_FRMT_GUIB " to " CPL_FRMT_GUIB,
             nCurOff, nOffset);

    char abyTemp[8192];
    nCurOff = nRealPos;
    while(TRUE)
    {
        int nToRead = (int) MIN(8192, nOffset - nCurOff);
        int nRead = ReadAndCache( abyTemp, nToRead );

        if (nRead < nToRead)
            return -1;
        if (nToRead < 8192)
            break;
    }

    return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIStdinHandle::Tell()
{
    return nCurOff;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIStdinHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    VSIStdinInit();

    if (nCurOff < nBufferLen)
    {
        if (nCurOff + nSize * nCount < nBufferLen)
        {
            memcpy(pBuffer, pabyBuffer + nCurOff, nSize * nCount);
            nCurOff += nSize * nCount;
            return nCount;
        }

        int nAlreadyCached = (int)(nBufferLen - nCurOff);
        memcpy(pBuffer, pabyBuffer + nCurOff, nAlreadyCached);

        nCurOff += nAlreadyCached;

        int nRead = ReadAndCache( (GByte*)pBuffer + nAlreadyCached,
                                  (int)(nSize*nCount - nAlreadyCached) );

        return ((nRead + nAlreadyCached) / nSize);
    }

    int nRead = ReadAndCache( pBuffer, (int)(nSize * nCount) );
    return nRead / nSize;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIStdinHandle::Write( const void * pBuffer, size_t nSize, 
                                  size_t nCount )

{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Write() unsupported on /vsistdin");
    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIStdinHandle::Eof()

{
    if (nCurOff < nBufferLen)
        return FALSE;
    return feof(stdin);
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIStdinHandle::Close()

{
    return 0;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIStdinFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VSIStdinFilesystemHandler()                   */
/************************************************************************/

VSIStdinFilesystemHandler::VSIStdinFilesystemHandler()
{
    pabyBuffer = NULL;
    nBufferLen = 0;
    nRealPos = 0;
}

/************************************************************************/
/*                       ~VSIStdinFilesystemHandler()                   */
/************************************************************************/

VSIStdinFilesystemHandler::~VSIStdinFilesystemHandler()
{
    CPLFree(pabyBuffer);
    pabyBuffer = NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIStdinFilesystemHandler::Open( const char *pszFilename, 
                                 const char *pszAccess )

{
    if (strcmp(pszFilename, "/vsistdin/") != 0)
        return NULL;

    if ( strchr(pszAccess, 'w') != NULL ||
         strchr(pszAccess, '+') != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Write or update mode not supported on /vsistdin");
        return NULL;
    }

    return new VSIStdinHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIStdinFilesystemHandler::Stat( const char * pszFilename,
                                     VSIStatBufL * pStatBuf,
                                     int nFlags )

{
    memset( pStatBuf, 0, sizeof(VSIStatBufL) );

    if (strcmp(pszFilename, "/vsistdin/") != 0)
        return -1;

    if ((nFlags & VSI_STAT_SIZE_FLAG))
    {
        VSIStdinInit();
        if (nBufferLen == 0)
            nRealPos = nBufferLen = fread(pabyBuffer, 1, BUFFER_SIZE, stdin);

        pStatBuf->st_size = nBufferLen;
    }

    pStatBuf->st_mode = S_IFREG;
    return 0;
}

/************************************************************************/
/*                       VSIInstallStdinHandler()                       */
/************************************************************************/

/**
 * \brief Install /vsistdin/ file system handler
 *
 * A special file handler is installed that allows reading from the standard
 * input steam.
 *
 * The file operations available are of course limited to Read() and
 * forward Seek() (full seek in the first MB of a file).
 *
 * @since GDAL 1.8.0
 */
void VSIInstallStdinHandler()

{
    VSIFileManager::InstallHandler( "/vsistdin/", new VSIStdinFilesystemHandler );
}
