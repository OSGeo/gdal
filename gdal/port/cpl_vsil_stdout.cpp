/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for stdout
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

static VSIWriteFunction pWriteFunction = fwrite;
static FILE* pWriteStream = stdout;

/************************************************************************/
/*                        VSIStdoutSetRedirection()                     */
/************************************************************************/

/** Set an alternative write function and output file handle instead of
 *  fwrite() / stdout.
 * 
 * @param pFct Function with same signature as fwrite()
 * @param stream File handle on which to output. Passed to pFct.
 * 
 * @since GDAL 2.0
 */
void VSIStdoutSetRedirection( VSIWriteFunction pFct, FILE* stream )
{
    pWriteFunction = pFct;
    pWriteStream = stream;
}


/************************************************************************/
/* ==================================================================== */
/*                       VSIStdoutFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutFilesystemHandler : public VSIFilesystemHandler
{
public:
    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIStdoutHandle                               */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutHandle : public VSIVirtualHandle
{
    vsi_l_offset      nOffset;

  public:
                      VSIStdoutHandle() : nOffset(0) {}

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();
};

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIStdoutHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    if( nOffset == 0 && (nWhence == SEEK_END || nWhence == SEEK_CUR) )
        return 0;
    if( nWhence == SEEK_SET && nOffset == Tell() )
        return 0;
    CPLError(CE_Failure, CPLE_NotSupported, "Seek() unsupported on /vsistdout");
    return -1;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIStdoutHandle::Tell()
{
    return nOffset;
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIStdoutHandle::Flush()

{
    if( pWriteStream == stdout )
        return fflush( stdout );
    else
        return 0;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIStdoutHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    CPLError(CE_Failure, CPLE_NotSupported, "Read() unsupported on /vsistdout");
    return 0;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIStdoutHandle::Write( const void * pBuffer, size_t nSize, 
                                  size_t nCount )

{
    size_t nRet = pWriteFunction(pBuffer, nSize, nCount, pWriteStream);
    nOffset += nSize * nRet;
    return nRet;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIStdoutHandle::Eof()

{
    return 0;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIStdoutHandle::Close()

{
    return 0;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIStdoutFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIStdoutFilesystemHandler::Open( const char *pszFilename, 
                                  const char *pszAccess )

{
    if ( strchr(pszAccess, 'r') != NULL ||
         strchr(pszAccess, '+') != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Read or update mode not supported on /vsistdout");
        return NULL;
    }

#ifdef WIN32
    if ( strchr(pszAccess, 'b') != NULL )
        setmode( fileno( stdout ), O_BINARY );
#endif

    return new VSIStdoutHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIStdoutFilesystemHandler::Stat( const char * pszFilename,
                                      VSIStatBufL * pStatBuf,
                                      int nFlags )

{
    memset( pStatBuf, 0, sizeof(VSIStatBufL) );

    return -1;
}



/************************************************************************/
/* ==================================================================== */
/*                   VSIStdoutRedirectFilesystemHandler                 */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutRedirectFilesystemHandler : public VSIFilesystemHandler
{
public:
    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIStdoutRedirectHandle                       */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutRedirectHandle : public VSIVirtualHandle
{
    VSIVirtualHandle* poHandle;
  public:
                      VSIStdoutRedirectHandle(VSIVirtualHandle* poHandle);
                     ~VSIStdoutRedirectHandle();

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();
};

/************************************************************************/
/*                        VSIStdoutRedirectHandle()                    */
/************************************************************************/

VSIStdoutRedirectHandle::VSIStdoutRedirectHandle(VSIVirtualHandle* poHandle)
{
    this->poHandle = poHandle;
}

/************************************************************************/
/*                        ~VSIStdoutRedirectHandle()                    */
/************************************************************************/

VSIStdoutRedirectHandle::~VSIStdoutRedirectHandle()
{
    delete poHandle;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIStdoutRedirectHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    CPLError(CE_Failure, CPLE_NotSupported, "Seek() unsupported on /vsistdout_redirect");
    return -1;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIStdoutRedirectHandle::Tell()
{
    return poHandle->Tell();
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIStdoutRedirectHandle::Flush()

{
    return poHandle->Flush();
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIStdoutRedirectHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    CPLError(CE_Failure, CPLE_NotSupported, "Read() unsupported on /vsistdout_redirect");
    return 0;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIStdoutRedirectHandle::Write( const void * pBuffer, size_t nSize,
                                  size_t nCount )

{
    return poHandle->Write(pBuffer, nSize, nCount);
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIStdoutRedirectHandle::Eof()

{
    return poHandle->Eof();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIStdoutRedirectHandle::Close()

{
    return poHandle->Close();
}

/************************************************************************/
/* ==================================================================== */
/*                 VSIStdoutRedirectFilesystemHandler                   */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIStdoutRedirectFilesystemHandler::Open( const char *pszFilename,
                                          const char *pszAccess )

{
    if ( strchr(pszAccess, 'r') != NULL ||
         strchr(pszAccess, '+') != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Read or update mode not supported on /vsistdout_redirect");
        return NULL;
    }

    VSIVirtualHandle* poHandle = (VSIVirtualHandle* )VSIFOpenL(
            pszFilename + strlen("/vsistdout_redirect/"), pszAccess);
    if (poHandle == NULL)
        return NULL;

    return new VSIStdoutRedirectHandle(poHandle);
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIStdoutRedirectFilesystemHandler::Stat( const char * pszFilename,
                                      VSIStatBufL * pStatBuf,
                                      int nFlags )

{
    memset( pStatBuf, 0, sizeof(VSIStatBufL) );

    return -1;
}

/************************************************************************/
/*                       VSIInstallStdoutHandler()                      */
/************************************************************************/

/**
 * \brief Install /vsistdout/ file system handler
 *
 * A special file handler is installed that allows writing to the standard
 * output stream.
 *
 * The file operations available are of course limited to Write().
 *
 * @since GDAL 1.8.0
 */

void VSIInstallStdoutHandler()

{
    VSIFileManager::InstallHandler( "/vsistdout/", new VSIStdoutFilesystemHandler );
    VSIFileManager::InstallHandler( "/vsistdout_redirect/", new VSIStdoutRedirectFilesystemHandler );
}
