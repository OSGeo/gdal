/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of subfile virtual IO functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include <limits>

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi_virtual.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                           VSISubFileHandle                           */
/* ==================================================================== */
/************************************************************************/

class VSISubFileHandle final: public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSISubFileHandle)

  public:
    VSILFILE     *fp = nullptr;
    vsi_l_offset  nSubregionOffset = 0;
    vsi_l_offset  nSubregionSize = 0;
    bool          bAtEOF = false;

    VSISubFileHandle() = default;
    ~VSISubFileHandle() override;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Close() override;
};

/************************************************************************/
/* ==================================================================== */
/*                   VSISubFileFilesystemHandler                        */
/* ==================================================================== */
/************************************************************************/

class VSISubFileFilesystemHandler final: public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSISubFileFilesystemHandler)

  public:
    VSISubFileFilesystemHandler() = default;
    ~VSISubFileFilesystemHandler() override = default;

    static int              DecomposePath( const char *pszPath,
                                    CPLString &osFilename,
                                    vsi_l_offset &nSubFileOffset,
                                    vsi_l_offset &nSubFileSize );

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError,
                            CSLConstList /* papszOptions */ ) override;
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;
    int Unlink( const char *pszFilename ) override;
    int Mkdir( const char *pszDirname, long nMode ) override;
    int Rmdir( const char *pszDirname ) override;
    char **ReadDir( const char *pszDirname ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                             VSISubFileHandle                         */
/* ==================================================================== */
/************************************************************************/

VSISubFileHandle::~VSISubFileHandle()
{
    VSISubFileHandle::Close();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSISubFileHandle::Close()

{
    if( fp == nullptr )
        return -1;
    int nRet = VSIFCloseL( fp );
    fp = nullptr;

    return nRet;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSISubFileHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    bAtEOF = false;

    if( nWhence == SEEK_SET )
    {
        if( nOffset > std::numeric_limits<vsi_l_offset>::max() - nSubregionOffset )
            return -1;
        nOffset += nSubregionOffset;
    }
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
            // Transform it into 0 for correct behavior of Read(), Write() and
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
                                   bool /* bSetError */,
                                   CSLConstList /* papszOptions */ )

{
    if( !STARTS_WITH_CI(pszFilename, "/vsisubfile/") )
        return nullptr;

    CPLString osSubFilePath;
    vsi_l_offset nOff = 0;
    vsi_l_offset nSize = 0;

    if( !DecomposePath( pszFilename, osSubFilePath, nOff, nSize ) )
    {
        errno = ENOENT;
        return nullptr;
    }
    if( nOff > std::numeric_limits<vsi_l_offset>::max() - nSize )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      We can't open the containing file with "w" access, so if        */
/*      that is requested use "r+" instead to update in place.          */
/* -------------------------------------------------------------------- */
    if( pszAccess[0] == 'w' )
        pszAccess = "r+";

/* -------------------------------------------------------------------- */
/*      Open the underlying file.                                       */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osSubFilePath, pszAccess );

    if( fp == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Setup the file handle on this file.                             */
/* -------------------------------------------------------------------- */
    VSISubFileHandle *poHandle = new VSISubFileHandle;

    poHandle->fp = fp;
    poHandle->nSubregionOffset = nOff;
    poHandle->nSubregionSize = nSize;

    // In read-only mode validate (offset, size) against underlying file size
    if( strchr(pszAccess, 'r') != nullptr && strchr(pszAccess, '+') == nullptr )
    {
        if( VSIFSeekL( fp, 0, SEEK_END ) != 0 )
        {
            poHandle->Close();
            delete poHandle;
            return nullptr;
        }
        vsi_l_offset nFpSize = VSIFTellL(fp);
        // For a directory, the size will be max(vsi_l_offset) / 2
        if( nFpSize == ~(static_cast<vsi_l_offset>(0)) / 2 || nOff > nFpSize )
        {
            poHandle->Close();
            delete poHandle;
            return nullptr;
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
        poHandle = nullptr;
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
        else if( static_cast<vsi_l_offset>(psStatBuf->st_size) >= nOff )
            psStatBuf->st_size -= nOff;
        else
            psStatBuf->st_size = 0;
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
    return nullptr;
}

/************************************************************************/
/*                 VSIInstallSubFileFilesystemHandler()                 */
/************************************************************************/

/**
 * Install /vsisubfile/ virtual file handler.
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_subfile">/vsisubfile/ documentation</a>
 */

void VSIInstallSubFileHandler()
{
    VSIFileManager::InstallHandler( "/vsisubfile/",
                                    new VSISubFileFilesystemHandler );
}
