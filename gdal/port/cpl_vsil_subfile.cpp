/******************************************************************************
 * $Id$
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of subfile virtual IO functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
#include <map>

#if defined(WIN32CE)
#  include <wce_errno.h>
#endif

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                           VSISubFileHandle                           */
/* ==================================================================== */
/************************************************************************/

class VSISubFileHandle : public VSIVirtualHandle
{ 
  public:
    FILE          *fp;
    vsi_l_offset  nSubregionOffset;
    vsi_l_offset  nSubregionSize;
    int           bUpdate;

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Close();
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

    int              DecomposePath( const char *pszPath, 
                                    CPLString &osFilename, 
                                    vsi_l_offset &nSubFileOffset,
                                    vsi_l_offset &nSubFileSize );

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf );
    virtual int      Unlink( const char *pszFilename );
    virtual int      Mkdir( const char *pszDirname, long nMode );
    virtual int      Rmdir( const char *pszDirname );
    virtual char   **ReadDir( const char *pszDirname );
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
    VSIFCloseL( fp );
    fp = NULL;

    return 0;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSISubFileHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
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
    return VSIFTellL( fp ) - nSubregionOffset;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSISubFileHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    return VSIFReadL( pBuffer, nSize, nCount, fp );
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSISubFileHandle::Write( const void * pBuffer, size_t nSize, size_t nCount )

{
    return VSIFWriteL( pBuffer, nSize, nCount, fp );
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSISubFileHandle::Eof()

{
    if (nSubregionSize != 0)
        return VSIFTellL( fp ) >= nSubregionOffset + nSubregionSize;
    else
        return VSIFEofL( fp );
}

/************************************************************************/
/* ==================================================================== */
/*                       VSISubFileFilesystemHandler                    */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                      VSISubFileFilesystemHandler()                   */
/************************************************************************/

VSISubFileFilesystemHandler::VSISubFileFilesystemHandler()

{
}

/************************************************************************/
/*                      ~VSISubFileFilesystemHandler()                  */
/************************************************************************/

VSISubFileFilesystemHandler::~VSISubFileFilesystemHandler()

{
}

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
    int i;

    osFilename = "";
    nSubFileOffset = 0;
    nSubFileSize = 0;

    if( strncmp(pszPath,"/vsisubfile/",12) != 0 )
        return FALSE;

    nSubFileOffset = CPLScanUIntBig(pszPath+12, strlen(pszPath + 12));
    for( i = 12; pszPath[i] != '\0'; i++ )
    {
        if( pszPath[i] == '_' && nSubFileSize == 0 )
            nSubFileSize = CPLScanUIntBig(pszPath + i + 1, strlen(pszPath + i + 1));
        else if( pszPath[i] == ',' )
        {
            osFilename = pszPath + i + 1;
            return TRUE;
        }
        else if( pszPath[i] == '/' )
        {
            // missing comma!
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
                                   const char *pszAccess )

{
    CPLString osSubFilePath;
    vsi_l_offset nOff, nSize;

    if( !DecomposePath( pszFilename, osSubFilePath, nOff, nSize ) )
    {
        errno = ENOENT;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the underlying file.                                       */
/* -------------------------------------------------------------------- */
    FILE *fp = VSIFOpenL( osSubFilePath, pszAccess );
    
    if( fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Setup the file handle on this file.                             */
/* -------------------------------------------------------------------- */
    VSISubFileHandle *poHandle = new VSISubFileHandle;

    poHandle->fp = fp;
    poHandle->nSubregionOffset = nOff;
    poHandle->nSubregionSize = nSize;

    VSIFSeekL( fp, nOff, SEEK_SET );

    return poHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSISubFileFilesystemHandler::Stat( const char * pszFilename, 
                                       VSIStatBufL * psStatBuf )
    
{
    CPLString osSubFilePath;
    vsi_l_offset nOff, nSize;

    if( !DecomposePath( pszFilename, osSubFilePath, nOff, nSize ) )
    {
        errno = ENOENT;
        return -1;
    }

    int nResult = VSIStatL( osSubFilePath, psStatBuf );
    
    if( nResult == 0 )
    {
        if( nSize != 0 )
            psStatBuf->st_size = (long)nSize;
        else
            psStatBuf->st_size -= (long)nOff;
    }

    return nResult;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSISubFileFilesystemHandler::Unlink( const char * pszFilename )

{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSISubFileFilesystemHandler::Mkdir( const char * pszPathname,
                                    long nMode )

{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSISubFileFilesystemHandler::Rmdir( const char * pszPathname )

{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                              ReadDir()                               */
/************************************************************************/

char **VSISubFileFilesystemHandler::ReadDir( const char *pszPath )

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
 *   /vsisubfile/<offset>[_<size>],<filename>
 *
 * The size parameter is optional.  Without it the remainder of the
 * file from the start offset as treated as part of the subfile.  Otherwise
 * only <size> bytes from <offset> are treated as part of the subfile. 
 * The <filename> portion may be a relative or absolute path using normal 
 * rules.   The <offset> and <size> values are in bytes. 
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
                            
