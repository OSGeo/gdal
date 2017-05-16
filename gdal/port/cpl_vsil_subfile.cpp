/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of subfile virtual IO functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_vsi.h"

#include <cerrno>
#include <cstddef>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi_virtual.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                           VSISubFileHandle                           */
/* ==================================================================== */
/************************************************************************/

class VSISubFileHandle : public VSIVirtualHandle
{
  public:
    VSILFILE     *fp;
    vsi_l_offset  nSubregionOffset;
    vsi_l_offset  nSubregionSize;
    bool          bAtEOF;

                      VSISubFileHandle() : fp(NULL), nSubregionOffset(0),
                                           nSubregionSize(0), bAtEOF(false) {}

    virtual int       Seek( vsi_l_offset nOffset, int nWhence ) override;
    virtual vsi_l_offset Tell() override;
    virtual size_t    Read( void *pBuffer, size_t nSize,
                            size_t nMemb ) override;
    virtual size_t    Write( const void *pBuffer, size_t nSize,
                             size_t nMemb ) override;
    virtual int       Eof() override;
    virtual int       Close() override;
};

/************************************************************************/
/* ==================================================================== */
/*                   VSISubFileFilesystemHandler                        */
/* ==================================================================== */
/************************************************************************/

class VSISubFileFilesystemHandler : public VSIFilesystemHandler
{
public:
                     VSISubFileFilesystemHandler();
    virtual          ~VSISubFileFilesystemHandler();

    static int              DecomposePath( const char *pszPath,
                                    CPLString &osFilename,
                                    vsi_l_offset &nSubFileOffset,
                                    vsi_l_offset &nSubFileSize );

    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess,
                                    bool bSetError ) override;
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                           int nFlags ) override;
    virtual int      Unlink( const char *pszFilename ) override;
    virtual int      Mkdir( const char *pszDirname, long nMode ) override;
    virtual int      Rmdir( const char *pszDirname ) override;
    virtual char   **ReadDir( const char *pszDirname ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                             VSISubFileHandle                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSISubFileHandle::Close()

{
    int nRet = VSIFCloseL( fp );
    fp = NULL;

    return nRet;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSISubFileHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    bAtEOF = false;

    if( nWhence == SEEK_SET )
        nOffset += nSubregionOffset;
    else if( nWhence == SEEK_CUR )
    {
        // handle normally.
    }
    else if( nWhence == SEEK_END )
    {
        if( nSubregionSize != 0 )
        {
            nOffset = nSubregionOffset + nSubregionSize;
            nWhence = SEEK_SET;
        }
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    return VSIFSeekL( fp, nOffset, nWhence );
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSISubFileHandle::Tell()

{
    vsi_l_offset nBasePos = VSIFTellL( fp );
    if( nBasePos >= nSubregionOffset )
        return nBasePos - nSubregionOffset;
    return 0;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSISubFileHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    size_t nRet = 0;
    if( nSubregionSize == 0 )
    {
        nRet = VSIFReadL( pBuffer, nSize, nCount, fp );
    }
    else
    {
        if( nSize == 0 )
            return 0;

        const vsi_l_offset nCurOffset = VSIFTellL(fp);
        if( nCurOffset >= nSubregionOffset + nSubregionSize )
        {
            bAtEOF = true;
            return 0;
        }

        const size_t nByteToRead = nSize * nCount;
        if( nCurOffset + nByteToRead > nSubregionOffset + nSubregionSize )
        {
            const int nRead = static_cast<int>(
                VSIFReadL(
                    pBuffer, 1,
                    static_cast<size_t>(nSubregionOffset + nSubregionSize -
                                        nCurOffset), fp));
            nRet = nRead / nSize;
        }
        else
        {
            nRet = VSIFReadL( pBuffer, nSize, nCount, fp );
        }
    }

    if( nRet < nCount )
        bAtEOF = true;

    return nRet;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSISubFileHandle::Write( const void * pBuffer, size_t nSize,
                                size_t nCount )

{
    bAtEOF = false;

    if( nSubregionSize == 0 )
        return VSIFWriteL( pBuffer, nSize, nCount, fp );

    if( nSize == 0 )
        return 0;

    const vsi_l_offset nCurOffset = VSIFTellL(fp);
    if( nCurOffset >= nSubregionOffset + nSubregionSize )
        return 0;

    const size_t nByteToWrite = nSize * nCount;
    if( nCurOffset + nByteToWrite > nSubregionOffset + nSubregionSize )
    {
        const int nWritten = static_cast<int>(
            VSIFWriteL(
                pBuffer, 1,
                static_cast<size_t>(nSubregionOffset + nSubregionSize -
                                     nCurOffset),
                fp));
        return nWritten / nSize;
    }

    return VSIFWriteL( pBuffer, nSize, nCount, fp );
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSISubFileHandle::Eof()

{
    return bAtEOF;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSISubFileFilesystemHandler                    */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                      VSISubFileFilesystemHandler()                   */
/************************************************************************/

VSISubFileFilesystemHandler::VSISubFileFilesystemHandler() {}

/************************************************************************/
/*                      ~VSISubFileFilesystemHandler()                  */
/************************************************************************/

VSISubFileFilesystemHandler::~VSISubFileFilesystemHandler() {}

/************************************************************************/
/*                           DecomposePath()                            */
/*                                                                      */
/*      Parse a path like /vsisubfile/1000_2000,data/abc.tif into an    */
/*      offset (1000), a size (2000) and a path (data/abc.tif).         */
/************************************************************************/

int
VSISubFileFilesystemHandler::DecomposePath( const char *pszPath,
                                            CPLString &osFilename,
                                            vsi_l_offset &nSubFileOffset,
                                            vsi_l_offset &nSubFileSize )

{
    if( !STARTS_WITH(pszPath, "/vsisubfile/") )
        return FALSE;

    osFilename = "";
    nSubFileOffset = 0;
    nSubFileSize = 0;

    nSubFileOffset =
        CPLScanUIntBig(pszPath+12, static_cast<int>(strlen(pszPath + 12)));
    for( int i = 12; pszPath[i] != '\0'; i++ )
    {
        if( pszPath[i] == '_' && nSubFileSize == 0 )
        {
            // -1 is sometimes passed to mean that we don't know the file size
            // for example when creating a JPEG2000 datastream in a NITF file
            // Transform it into 0 for correct behaviour of Read(), Write() and
            // Eof().
            if( pszPath[i + 1] == '-' )
                nSubFileSize = 0;
            else
                nSubFileSize =
                    CPLScanUIntBig(pszPath + i + 1,
                                   static_cast<int>(strlen(pszPath + i + 1)));
        }
        else if( pszPath[i] == ',' )
        {
            osFilename = pszPath + i + 1;
            return TRUE;
        }
        else if( pszPath[i] == '/' )
        {
            // Missing comma!
            return FALSE;
        }
    }

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSISubFileFilesystemHandler::Open( const char *pszFilename,
                                   const char *pszAccess,
                                   bool /* bSetError */ )

{
    if( !STARTS_WITH_CI(pszFilename, "/vsisubfile/") )
        return NULL;

    CPLString osSubFilePath;
    vsi_l_offset nOff = 0;
    vsi_l_offset nSize = 0;

    if( !DecomposePath( pszFilename, osSubFilePath, nOff, nSize ) )
    {
        errno = ENOENT;
        return NULL;
    }
    if( nOff + nSize < nOff )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      We can't open the containing file with "w" access, so if tht    */
/*      is requested use "r+" instead to update in place.               */
/* -------------------------------------------------------------------- */
    if( pszAccess[0] == 'w' )
        pszAccess = "r+";

/* -------------------------------------------------------------------- */
/*      Open the underlying file.                                       */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osSubFilePath, pszAccess );

    if( fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Setup the file handle on this file.                             */
/* -------------------------------------------------------------------- */
    VSISubFileHandle *poHandle = new VSISubFileHandle;

    poHandle->fp = fp;
    poHandle->nSubregionOffset = nOff;
    poHandle->nSubregionSize = nSize;

    // In read-only mode validate (offset, size) against underlying file size
    if( strchr(pszAccess, 'r') != NULL && strchr(pszAccess, '+') == NULL )
    {
        if( VSIFSeekL( fp, 0, SEEK_END ) != 0 )
        {
            poHandle->Close();
            delete poHandle;
            return NULL;
        }
        vsi_l_offset nFpSize = VSIFTellL(fp);
        // For a directory, the size will be max(vsi_l_offset) / 2
        if( nFpSize == ~((vsi_l_offset)(0)) / 2 || nOff > nFpSize )
        {
            poHandle->Close();
            delete poHandle;
            return NULL;
        }
        if( nOff + nSize > nFpSize )
        {
            nSize = nFpSize - nOff;
            poHandle->nSubregionSize = nSize;
        }
    }

    if( VSIFSeekL( fp, nOff, SEEK_SET ) != 0 )
    {
        poHandle->Close();
        delete poHandle;
        poHandle = NULL;
    }

    return poHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSISubFileFilesystemHandler::Stat( const char * pszFilename,
                                       VSIStatBufL * psStatBuf,
                                       int nFlags )

{
    if( !STARTS_WITH_CI(pszFilename, "/vsisubfile/") )
        return -1;

    CPLString osSubFilePath;
    vsi_l_offset nOff = 0;
    vsi_l_offset nSize = 0;

    memset( psStatBuf, 0, sizeof(VSIStatBufL) );

    if( !DecomposePath( pszFilename, osSubFilePath, nOff, nSize ) )
    {
        errno = ENOENT;
        return -1;
    }

    const int nResult = VSIStatExL( osSubFilePath, psStatBuf, nFlags );

    if( nResult == 0 )
    {
        if( nSize != 0 )
            psStatBuf->st_size = nSize;
        else
            psStatBuf->st_size -= nOff;
    }

    return nResult;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSISubFileFilesystemHandler::Unlink( const char * /* pszFilename */ )
{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSISubFileFilesystemHandler::Mkdir( const char * /* pszPathname */,
                                        long /* nMode */ )
{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSISubFileFilesystemHandler::Rmdir( const char * /* pszPathname */ )

{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                              ReadDir()                               */
/************************************************************************/

char **VSISubFileFilesystemHandler::ReadDir( const char * /* pszPath */ )
{
    errno = EACCES;
    return NULL;
}

/************************************************************************/
/*                 VSIInstallSubFileFilesystemHandler()                 */
/************************************************************************/

/**
 * Install /vsisubfile/ virtual file handler.
 *
 * This virtual file system handler allows access to subregions of
 * files, treating them as a file on their own to the virtual file
 * system functions (VSIFOpenL(), etc).
 *
 * A special form of the filename is used to indicate a subportion
 * of another file:
 *
 *   /vsisubfile/&lt;offset&gt;[_&lt;size&gt;],&lt;filename&gt;
 *
 * The size parameter is optional.  Without it the remainder of the file from
 * the start offset as treated as part of the subfile.  Otherwise only
 * &lt;size&gt; bytes from &lt;offset&gt; are treated as part of the subfile.
 * The &lt;filename&gt; portion may be a relative or absolute path using normal
 * rules.  The &lt;offset&gt; and &lt;size&gt; values are in bytes.
 *
 * eg.
 *   /vsisubfile/1000_3000,/data/abc.ntf
 *   /vsisubfile/5000,../xyz/raw.dat
 *
 * Unlike the /vsimem/ or conventional file system handlers, there
 * is no meaningful support for filesystem operations for creating new
 * files, traversing directories, and deleting files within the /vsisubfile/
 * area.  Only the VSIStatL(), VSIFOpenL() and operations based on the file
 * handle returned by VSIFOpenL() operate properly.
 */

void VSIInstallSubFileHandler()
{
    VSIFileManager::InstallHandler( "/vsisubfile/",
                                    new VSISubFileFilesystemHandler );
}
