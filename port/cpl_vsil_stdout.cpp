/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for stdout
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_vsi.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include "cpl_error.h"
#include "cpl_vsi_virtual.h"

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

CPL_CVSID("$Id$")

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

//! @cond Doxygen_Suppress

/************************************************************************/
/* ==================================================================== */
/*                       VSIStdoutFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutFilesystemHandler final : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIStdoutFilesystemHandler)

  public:
    VSIStdoutFilesystemHandler() = default;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError,
                            CSLConstList /* papszOptions */ ) override;
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIStdoutHandle                               */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutHandle final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIStdoutHandle)

    vsi_l_offset      m_nOffset = 0;

  public:
    VSIStdoutHandle() = default;
    ~VSIStdoutHandle() override = default;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
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
    return m_nOffset;
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

size_t VSIStdoutHandle::Read( void * /* pBuffer */,
                              size_t /* nSize */,
                              size_t /* nCount */ )
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
    m_nOffset += nSize * nRet;
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
    return Flush();
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
VSIStdoutFilesystemHandler::Open( const char * /* pszFilename */,
                                  const char *pszAccess,
                                  bool /* bSetError */,
                                  CSLConstList /* papszOptions */ )
{
    if ( strchr(pszAccess, 'r') != nullptr ||
         strchr(pszAccess, '+') != nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Read or update mode not supported on /vsistdout");
        return nullptr;
    }

#ifdef WIN32
    if ( strchr(pszAccess, 'b') != nullptr )
        setmode( fileno( stdout ), O_BINARY );
#endif

    return new VSIStdoutHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIStdoutFilesystemHandler::Stat( const char * /* pszFilename */,
                                      VSIStatBufL * pStatBuf,
                                      int /* nFlags */ )

{
    memset( pStatBuf, 0, sizeof(VSIStatBufL) );

    return -1;
}

/************************************************************************/
/* ==================================================================== */
/*                   VSIStdoutRedirectFilesystemHandler                 */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutRedirectFilesystemHandler final : public VSIFilesystemHandler
{
  public:
    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError,
                            CSLConstList /* papszOptions */ ) override;
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIStdoutRedirectHandle                       */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutRedirectHandle final : public VSIVirtualHandle
{
    VSIVirtualHandle* m_poHandle = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(VSIStdoutRedirectHandle)

  public:
    explicit VSIStdoutRedirectHandle( VSIVirtualHandle* poHandle );
    ~VSIStdoutRedirectHandle() override;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
};

/************************************************************************/
/*                        VSIStdoutRedirectHandle()                    */
/************************************************************************/

VSIStdoutRedirectHandle::VSIStdoutRedirectHandle(VSIVirtualHandle* poHandle):
    m_poHandle(poHandle)
{
}

/************************************************************************/
/*                        ~VSIStdoutRedirectHandle()                    */
/************************************************************************/

VSIStdoutRedirectHandle::~VSIStdoutRedirectHandle()
{
    delete m_poHandle;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIStdoutRedirectHandle::Seek( vsi_l_offset /* nOffset */,
                                   int /* nWhence */ )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Seek() unsupported on /vsistdout_redirect");
    return -1;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIStdoutRedirectHandle::Tell()
{
    return m_poHandle->Tell();
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIStdoutRedirectHandle::Flush()

{
    return m_poHandle->Flush();
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIStdoutRedirectHandle::Read( void * /* pBuffer */,
                                      size_t /* nSize */,
                                      size_t /* nCount */ )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Read() unsupported on /vsistdout_redirect");
    return 0;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIStdoutRedirectHandle::Write( const void * pBuffer, size_t nSize,
                                       size_t nCount )

{
    return m_poHandle->Write(pBuffer, nSize, nCount);
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIStdoutRedirectHandle::Eof()

{
    return m_poHandle->Eof();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIStdoutRedirectHandle::Close()

{
    return m_poHandle->Close();
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
                                          const char *pszAccess,
                                          bool /* bSetError */,
                                          CSLConstList /* papszOptions */ )

{
    if ( strchr(pszAccess, 'r') != nullptr ||
         strchr(pszAccess, '+') != nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Read or update mode not supported on /vsistdout_redirect");
        return nullptr;
    }

    VSIVirtualHandle* poHandle = reinterpret_cast<VSIVirtualHandle*>(
        VSIFOpenL(pszFilename + strlen("/vsistdout_redirect/"), pszAccess));
    if (poHandle == nullptr)
        return nullptr;

    return new VSIStdoutRedirectHandle(poHandle);
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIStdoutRedirectFilesystemHandler::Stat( const char * /* pszFilename */,
                                              VSIStatBufL * pStatBuf,
                                              int /* nFlags */ )
{
    memset( pStatBuf, 0, sizeof(VSIStatBufL) );

    return -1;
}

//! @endcond

/************************************************************************/
/*                       VSIInstallStdoutHandler()                      */
/************************************************************************/

/*!
 \brief Install /vsistdout/ file system handler

 A special file handler is installed that allows writing to the standard
 output stream.

 The file operations available are of course limited to Write().

 A variation of this file system exists as the /vsistdout_redirect/ file
 system handler, where the output function can be defined with
 VSIStdoutSetRedirection().

 \verbatim embed:rst
 See :ref:`/vsistdout/ documentation <vsistdout>`
 \endverbatim

 @since GDAL 1.8.0
 */

void VSIInstallStdoutHandler()

{
    VSIFileManager::InstallHandler( "/vsistdout/", new VSIStdoutFilesystemHandler );
    VSIFileManager::InstallHandler( "/vsistdout_redirect/", new VSIStdoutRedirectFilesystemHandler );
}
