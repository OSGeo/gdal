/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of Memory Buffer virtual IO functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_vsi_virtual.h"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <ctime>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#if HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include <map>
#include <string>
#include <utility>

#include "cpl_atomic_ops.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"

//! @cond Doxygen_Suppress

CPL_CVSID("$Id$")

/*
** Notes on Multithreading:
**
** VSIMemFilesystemHandler: This class maintains a mutex to protect
** access and update of the oFileList array which has all the "files" in
** the memory filesystem area.  It is expected that multiple threads would
** want to create and read different files at the same time and so might
** collide access oFileList without the mutex.
**
** VSIMemFile: In theory we could allow different threads to update the
** the same memory file, but for simplicity we restrict to single writer,
** multiple reader as an expectation on the application code (not enforced
** here), which means we don't need to do any protection of this class.
**
** VSIMemHandle: This is essentially a "current location" representing
** on accessor to a file, and is inherently intended only to be used in
** a single thread.
**
** In General:
**
** Multiple threads accessing the memory filesystem are ok as long as
**  1) A given VSIMemHandle (i.e. FILE * at app level) isn't used by multiple
**     threads at once.
**  2) A given memory file isn't accessed by more than one thread unless
**     all threads are just reading.
*/

/************************************************************************/
/* ==================================================================== */
/*                              VSIMemFile                              */
/* ==================================================================== */
/************************************************************************/

class VSIMemFile
{
public:
    CPLString     osFilename;
    volatile int  nRefCount;

    bool          bIsDirectory;

    bool          bOwnData;
    GByte        *pabyData;
    vsi_l_offset  nLength;
    vsi_l_offset  nAllocLength;
    vsi_l_offset  nMaxLength;

    time_t        mTime;

                  VSIMemFile();
    virtual       ~VSIMemFile();

    bool          SetLength( vsi_l_offset nNewSize );
};

/************************************************************************/
/* ==================================================================== */
/*                             VSIMemHandle                             */
/* ==================================================================== */
/************************************************************************/

class VSIMemHandle CPL_FINAL : public VSIVirtualHandle
{
  public:
    VSIMemFile    *poFile;
    vsi_l_offset  m_nOffset;
    bool          bUpdate;
    bool          bEOF;
    bool          bExtendFileAtNextWrite;

    VSIMemHandle() :
        poFile(NULL),
        m_nOffset(0),
        bUpdate(false),
        bEOF(false),
        bExtendFileAtNextWrite(false) {}
    virtual ~VSIMemHandle() {}

    virtual int       Seek( vsi_l_offset nOffset, int nWhence ) override;
    virtual vsi_l_offset Tell() override;
    virtual size_t    Read( void *pBuffer, size_t nSize,
                            size_t nMemb ) override;
    virtual size_t    Write( const void *pBuffer, size_t nSize,
                             size_t nMemb ) override;
    virtual int       Eof() override;
    virtual int       Close() override;
    virtual int       Truncate( vsi_l_offset nNewSize ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                       VSIMemFilesystemHandler                        */
/* ==================================================================== */
/************************************************************************/

class VSIMemFilesystemHandler CPL_FINAL : public VSIFilesystemHandler
{
  public:
    std::map<CPLString, VSIMemFile*> oFileList;
    CPLMutex        *hMutex;

                     VSIMemFilesystemHandler();
    virtual          ~VSIMemFilesystemHandler();

    // TODO(schwehr): Fix VSIFileFromMemBuffer so that using is not needed.
    using VSIFilesystemHandler::Open;

    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess,
                                    bool bSetError ) override;
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                           int nFlags ) override;
    virtual int      Unlink( const char *pszFilename ) override;
    virtual int      Mkdir( const char *pszDirname, long nMode ) override;
    virtual int      Rmdir( const char *pszDirname ) override;
    virtual char   **ReadDirEx( const char *pszDirname,
                                int nMaxFiles ) override;
    virtual int      Rename( const char *oldpath,
                             const char *newpath ) override;
    virtual GIntBig  GetDiskFreeSpace( const char* pszDirname ) override;

    static  void     NormalizePath( CPLString & );

    int              Unlink_unlocked( const char *pszFilename );
};

/************************************************************************/
/* ==================================================================== */
/*                              VSIMemFile                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             VSIMemFile()                             */
/************************************************************************/

VSIMemFile::VSIMemFile() :
    nRefCount(0),
    bIsDirectory(false),
    bOwnData(true),
    pabyData(NULL),
    nLength(0),
    nAllocLength(0),
    nMaxLength(GUINTBIG_MAX)
{
    time(&mTime);
}

/************************************************************************/
/*                            ~VSIMemFile()                             */
/************************************************************************/

VSIMemFile::~VSIMemFile()

{
    if( nRefCount != 0 )
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Memory file %s deleted with %d references.",
                  osFilename.c_str(), nRefCount );

    if( bOwnData && pabyData )
        CPLFree( pabyData );
}

/************************************************************************/
/*                             SetLength()                              */
/************************************************************************/

bool VSIMemFile::SetLength( vsi_l_offset nNewLength )

{
    if( nNewLength > nMaxLength )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Maximum file size reached!");
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Grow underlying array if needed.                                */
/* -------------------------------------------------------------------- */
    if( nNewLength > nAllocLength )
    {
        // If we don't own the buffer, we cannot reallocate it because
        // the return address might be different from the one passed by
        // the caller. Hence, the caller would not be able to free
        // the buffer.
        if( !bOwnData )
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Cannot extended in-memory file whose ownership was not "
                "transferred" );
            return false;
        }

        const vsi_l_offset nNewAlloc = (nNewLength + nNewLength / 10) + 5000;
        GByte *pabyNewData = NULL;
        if( static_cast<vsi_l_offset>(static_cast<size_t>(nNewAlloc))
            == nNewAlloc )
        {
            pabyNewData = static_cast<GByte *>(
                VSIRealloc(pabyData, static_cast<size_t>(nNewAlloc) ));
        }
        if( pabyNewData == NULL )
        {
            CPLError(
                CE_Failure, CPLE_OutOfMemory,
                "Cannot extend in-memory file to " CPL_FRMT_GUIB
                " bytes due to out-of-memory situation",
                nNewAlloc);
            return false;
        }

        // Clear the new allocated part of the buffer.
        memset(pabyNewData + nAllocLength, 0,
               static_cast<size_t>(nNewAlloc - nAllocLength));

        pabyData = pabyNewData;
        nAllocLength = nNewAlloc;
    }

    nLength = nNewLength;
    time(&mTime);

    return true;
}

/************************************************************************/
/* ==================================================================== */
/*                             VSIMemHandle                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIMemHandle::Close()

{
    if( CPLAtomicDec(&(poFile->nRefCount)) == 0 )
        delete poFile;

    poFile = NULL;

    return 0;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIMemHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    bExtendFileAtNextWrite = false;
    if( nWhence == SEEK_CUR )
    {
        if( nOffset > INT_MAX )
        {
            //printf("likely negative offset intended\n");
        }
        m_nOffset += nOffset;
    }
    else if( nWhence == SEEK_SET )
    {
        m_nOffset = nOffset;
    }
    else if( nWhence == SEEK_END )
    {
        m_nOffset = poFile->nLength + nOffset;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    bEOF = false;

    if( m_nOffset > poFile->nLength )
    {
        if( !bUpdate )  // Read-only files cannot be extended by seek.
        {
            CPLDebug(
                "VSIMemHandle",
                "Attempt to extend read-only file '%s' to length " CPL_FRMT_GUIB
                " from " CPL_FRMT_GUIB ".",
                poFile->osFilename.c_str(),
                m_nOffset, poFile->nLength);

            m_nOffset = poFile->nLength;
            errno = EACCES;
            return -1;
        }
        else  // Writable files are zero-extended by seek past end.
        {
            bExtendFileAtNextWrite = true;
        }
    }

    return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIMemHandle::Tell()

{
    return m_nOffset;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIMemHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    // FIXME: Integer overflow check should be placed here:
    size_t nBytesToRead = nSize * nCount;

    if( poFile->nLength < m_nOffset )
    {
        bEOF = true;
        return 0;
    }
    if( nBytesToRead + m_nOffset > poFile->nLength )
    {
        nBytesToRead = static_cast<size_t>(poFile->nLength - m_nOffset);
        nCount = nBytesToRead / nSize;
        bEOF = true;
    }

    if( nBytesToRead )
        memcpy( pBuffer, poFile->pabyData + m_nOffset,
                static_cast<size_t>(nBytesToRead) );
    m_nOffset += nBytesToRead;

    return nCount;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIMemHandle::Write( const void * pBuffer, size_t nSize, size_t nCount )

{
    if( !bUpdate )
    {
        errno = EACCES;
        return 0;
    }
    if( bExtendFileAtNextWrite )
    {
        bExtendFileAtNextWrite = false;
        if( !poFile->SetLength( m_nOffset ) )
            return 0;
    }

    // FIXME: Integer overflow check should be placed here:
    const size_t nBytesToWrite = nSize * nCount;

    if( nBytesToWrite + m_nOffset > poFile->nLength )
    {
        if( !poFile->SetLength( nBytesToWrite + m_nOffset ) )
            return 0;
    }

    if( nBytesToWrite )
        memcpy( poFile->pabyData + m_nOffset, pBuffer, nBytesToWrite );
    m_nOffset += nBytesToWrite;

    time(&poFile->mTime);

    return nCount;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIMemHandle::Eof()

{
    return bEOF;
}

/************************************************************************/
/*                             Truncate()                               */
/************************************************************************/

int VSIMemHandle::Truncate( vsi_l_offset nNewSize )
{
    if( !bUpdate )
    {
        errno = EACCES;
        return -1;
    }

    bExtendFileAtNextWrite = false;
    if( poFile->SetLength( nNewSize ) )
        return 0;

    return -1;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIMemFilesystemHandler                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                      VSIMemFilesystemHandler()                       */
/************************************************************************/

VSIMemFilesystemHandler::VSIMemFilesystemHandler() :
    hMutex(NULL)
{}

/************************************************************************/
/*                      ~VSIMemFilesystemHandler()                      */
/************************************************************************/

VSIMemFilesystemHandler::~VSIMemFilesystemHandler()

{
    for( std::map<CPLString, VSIMemFile*>::const_iterator iter =
             oFileList.begin();
         iter != oFileList.end();
         ++iter )
    {
        CPLAtomicDec(&(iter->second->nRefCount));
        delete iter->second;
    }

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIMemFilesystemHandler::Open( const char *pszFilename,
                               const char *pszAccess,
                               bool bSetError )

{
    CPLMutexHolder oHolder( &hMutex );
    CPLString osFilename = pszFilename;
    NormalizePath( osFilename );

    vsi_l_offset nMaxLength = GUINTBIG_MAX;
    const size_t iPos = osFilename.find("||maxlength=");
    if( iPos != std::string::npos )
    {
        nMaxLength = static_cast<vsi_l_offset>(CPLAtoGIntBig(
                    osFilename.substr(iPos + strlen("||maxlength=")).c_str()));
    }

/* -------------------------------------------------------------------- */
/*      Get the filename we are opening, create if needed.              */
/* -------------------------------------------------------------------- */
    VSIMemFile *poFile = NULL;
    if( oFileList.find(osFilename) != oFileList.end() )
        poFile = oFileList[osFilename];

    // If no file and opening in read, error out.
    if( strstr(pszAccess, "w") == NULL
        && strstr(pszAccess, "a") == NULL
        && poFile == NULL )
    {
        if( bSetError )
        {
            VSIError(VSIE_FileError, "No such file or directory");
        }
        errno = ENOENT;
        return NULL;
    }

    // Create.
    if( poFile == NULL )
    {
        poFile = new VSIMemFile;
        poFile->osFilename = osFilename;
        oFileList[poFile->osFilename] = poFile;
        CPLAtomicInc(&(poFile->nRefCount));  // For file list.
        poFile->nMaxLength = nMaxLength;
    }
    // Overwrite
    else if( strstr(pszAccess, "w") )
    {
        poFile->SetLength(0);
        poFile->nMaxLength = nMaxLength;
    }

    if( poFile->bIsDirectory )
    {
        errno = EISDIR;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup the file handle on this file.                             */
/* -------------------------------------------------------------------- */
    VSIMemHandle *poHandle = new VSIMemHandle;

    poHandle->poFile = poFile;
    poHandle->m_nOffset = 0;
    poHandle->bEOF = false;
    poHandle->bUpdate =
        strstr(pszAccess, "w") ||
        strstr(pszAccess, "+") ||
        strstr(pszAccess, "a");

    CPLAtomicInc(&(poFile->nRefCount));

    if( strstr(pszAccess, "a") )
        poHandle->m_nOffset = poFile->nLength;

    return poHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIMemFilesystemHandler::Stat( const char * pszFilename,
                                   VSIStatBufL * pStatBuf,
                                   int /* nFlags */ )

{
    CPLMutexHolder oHolder( &hMutex );

    CPLString osFilename = pszFilename;
    NormalizePath( osFilename );

    memset( pStatBuf, 0, sizeof(VSIStatBufL) );

    if( osFilename == "/vsimem/" )
    {
        pStatBuf->st_size = 0;
        pStatBuf->st_mode = S_IFDIR;
        return 0;
    }

    if( oFileList.find(osFilename) == oFileList.end() )
    {
        errno = ENOENT;
        return -1;
    }

    VSIMemFile *poFile = oFileList[osFilename];

    memset( pStatBuf, 0, sizeof(VSIStatBufL) );

    if( poFile->bIsDirectory )
    {
        pStatBuf->st_size = 0;
        pStatBuf->st_mode = S_IFDIR;
    }
    else
    {
        pStatBuf->st_size = poFile->nLength;
        pStatBuf->st_mode = S_IFREG;
        pStatBuf->st_mtime = poFile->mTime;
    }

    return 0;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSIMemFilesystemHandler::Unlink( const char * pszFilename )

{
    CPLMutexHolder oHolder( &hMutex );
    return Unlink_unlocked(pszFilename);
}

/************************************************************************/
/*                           Unlink_unlocked()                          */
/************************************************************************/

int VSIMemFilesystemHandler::Unlink_unlocked( const char * pszFilename )

{
    CPLString osFilename = pszFilename;
    NormalizePath( osFilename );

    if( oFileList.find(osFilename) == oFileList.end() )
    {
        errno = ENOENT;
        return -1;
    }

    VSIMemFile *poFile = oFileList[osFilename];

    if( CPLAtomicDec(&(poFile->nRefCount)) == 0 )
        delete poFile;

    oFileList.erase( oFileList.find(osFilename) );

    return 0;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIMemFilesystemHandler::Mkdir( const char * pszPathname,
                                    long /* nMode */ )

{
    CPLMutexHolder oHolder( &hMutex );

    CPLString osPathname = pszPathname;

    NormalizePath( osPathname );

    if( oFileList.find(osPathname) != oFileList.end() )
    {
        errno = EEXIST;
        return -1;
    }

    VSIMemFile *poFile = new VSIMemFile;

    poFile->osFilename = osPathname;
    poFile->bIsDirectory = true;
    oFileList[osPathname] = poFile;
    CPLAtomicInc(&(poFile->nRefCount));  // Referenced by file list.

    return 0;
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIMemFilesystemHandler::Rmdir( const char * pszPathname )

{
    return Unlink( pszPathname );
}

/************************************************************************/
/*                             ReadDirEx()                              */
/************************************************************************/

char **VSIMemFilesystemHandler::ReadDirEx( const char *pszPath,
                                           int nMaxFiles )

{
    CPLMutexHolder oHolder( &hMutex );

    CPLString osPath = pszPath;

    NormalizePath( osPath );

    char **papszDir = NULL;
    size_t nPathLen = osPath.size();

    if( nPathLen > 0 && osPath.back() == '/' )
        nPathLen--;

    // In case of really big number of files in the directory, CSLAddString
    // can be slow (see #2158). We then directly build the list.
    int nItems = 0;
    int nAllocatedItems = 0;

    for( std::map<CPLString, VSIMemFile*>::const_iterator iter =
             oFileList.begin();
         iter != oFileList.end();
         ++iter )
    {
        const char *pszFilePath = iter->second->osFilename.c_str();
        if( EQUALN(osPath, pszFilePath, nPathLen)
            && pszFilePath[nPathLen] == '/'
            && strstr(pszFilePath+nPathLen+1, "/") == NULL )
        {
            if( nItems == 0 )
            {
                papszDir = static_cast<char**>(CPLCalloc(2, sizeof(char*)));
                nAllocatedItems = 1;
            }
            else if( nItems >= nAllocatedItems )
            {
                nAllocatedItems = nAllocatedItems * 2;
                papszDir = static_cast<char**>(
                    CPLRealloc(papszDir, (nAllocatedItems + 2)*sizeof(char*)) );
            }

            papszDir[nItems] = CPLStrdup(pszFilePath+nPathLen+1);
            papszDir[nItems+1] = NULL;

            nItems++;
            if( nMaxFiles > 0 && nItems > nMaxFiles )
                break;
        }
    }

    return papszDir;
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSIMemFilesystemHandler::Rename( const char *pszOldPath,
                                     const char *pszNewPath )

{
    CPLMutexHolder oHolder( &hMutex );

    CPLString osOldPath = pszOldPath;
    CPLString osNewPath = pszNewPath;

    NormalizePath( osOldPath );
    NormalizePath( osNewPath );

    if( osOldPath.compare(osNewPath) == 0 )
        return 0;

    if( oFileList.find(osOldPath) == oFileList.end() )
    {
        errno = ENOENT;
        return -1;
    }

    std::map<CPLString, VSIMemFile*>::iterator it = oFileList.find(osOldPath);
    while( it != oFileList.end() && it->first.ifind(osOldPath) == 0 )
    {
        const CPLString osRemainder = it->first.substr(osOldPath.size());
        if( osRemainder.empty() || osRemainder[0] == '/' )
        {
            const CPLString osNewFullPath = osNewPath + osRemainder;
            Unlink_unlocked(osNewFullPath);
            oFileList[osNewFullPath] = it->second;
            it->second->osFilename = osNewFullPath;
            oFileList.erase(it++);
        }
        else
        {
            ++it;
        }
    }

    return 0;
}

/************************************************************************/
/*                           NormalizePath()                            */
/************************************************************************/

void VSIMemFilesystemHandler::NormalizePath( CPLString &oPath )

{
    size_t nPos = 0;
    while( (nPos = oPath.find('\\', nPos)) != std::string::npos )
    {
        oPath[nPos] = '/';
        nPos ++;
    }
}

/************************************************************************/
/*                        GetDiskFreeSpace()                            */
/************************************************************************/

GIntBig VSIMemFilesystemHandler::GetDiskFreeSpace( const char* /*pszDirname*/ )
{
    const GIntBig nRet = CPLGetUsablePhysicalRAM();
    if( nRet <= 0 )
        return -1;
    return nRet;
}

//! @endcond

/************************************************************************/
/*                     VSIInstallLargeFileHandler()                     */
/************************************************************************/

/**
 * \brief Install "memory" file system handler.
 *
 * A special file handler is installed that allows block of memory to be
 * treated as files.   All portions of the file system underneath the base
 * path "/vsimem/" will be handled by this driver.
 *
 * Normal VSI*L functions can be used freely to create and destroy memory
 * arrays treating them as if they were real file system objects.  Some
 * additional methods exist to efficient create memory file system objects
 * without duplicating original copies of the data or to "steal" the block
 * of memory associated with a memory file.
 *
 * At this time the memory handler does not properly handle directory
 * semantics for the memory portion of the filesystem.  The VSIReadDir()
 * function is not supported though this will be corrected in the future.
 *
 * Calling this function repeatedly should do no harm, though it is not
 * necessary.  It is already called the first time a virtualizable
 * file access function (i.e. VSIFOpenL(), VSIMkDir(), etc) is called.
 *
 * This code example demonstrates using GDAL to translate from one memory
 * buffer to another.
 *
 * \code
 * GByte *ConvertBufferFormat( GByte *pabyInData, vsi_l_offset nInDataLength,
 *                             vsi_l_offset *pnOutDataLength )
 * {
 *     // create memory file system object from buffer.
 *     VSIFCloseL( VSIFileFromMemBuffer( "/vsimem/work.dat", pabyInData,
 *                                       nInDataLength, FALSE ) );
 *
 *     // Open memory buffer for read.
 *     GDALDatasetH hDS = GDALOpen( "/vsimem/work.dat", GA_ReadOnly );
 *
 *     // Get output format driver.
 *     GDALDriverH hDriver = GDALGetDriverByName( "GTiff" );
 *     GDALDatasetH hOutDS;
 *
 *     hOutDS = GDALCreateCopy( hDriver, "/vsimem/out.tif", hDS, TRUE, NULL,
 *                              NULL, NULL );
 *
 *     // close source file, and "unlink" it.
 *     GDALClose( hDS );
 *     VSIUnlink( "/vsimem/work.dat" );
 *
 *     // seize the buffer associated with the output file.
 *
 *     return VSIGetMemFileBuffer( "/vsimem/out.tif", pnOutDataLength, TRUE );
 * }
 * \endcode
 */

void VSIInstallMemFileHandler()
{
    VSIFileManager::InstallHandler( "/vsimem/", new VSIMemFilesystemHandler );
}

/************************************************************************/
/*                        VSIFileFromMemBuffer()                        */
/************************************************************************/

/**
 * \brief Create memory "file" from a buffer.
 *
 * A virtual memory file is created from the passed buffer with the indicated
 * filename.  Under normal conditions the filename would need to be absolute
 * and within the /vsimem/ portion of the filesystem.
 *
 * If bTakeOwnership is TRUE, then the memory file system handler will take
 * ownership of the buffer, freeing it when the file is deleted.  Otherwise
 * it remains the responsibility of the caller, but should not be freed as
 * long as it might be accessed as a file.  In no circumstances does this
 * function take a copy of the pabyData contents.
 *
 * @param pszFilename the filename to be created.
 * @param pabyData the data buffer for the file.
 * @param nDataLength the length of buffer in bytes.
 * @param bTakeOwnership TRUE to transfer "ownership" of buffer or FALSE.
 *
 * @return open file handle on created file (see VSIFOpenL()).
 */

VSILFILE *VSIFileFromMemBuffer( const char *pszFilename,
                                GByte *pabyData,
                                vsi_l_offset nDataLength,
                                int bTakeOwnership )

{
    if( VSIFileManager::GetHandler("")
        == VSIFileManager::GetHandler("/vsimem/") )
        VSIInstallMemFileHandler();

    VSIMemFilesystemHandler *poHandler =
        static_cast<VSIMemFilesystemHandler *>(
                VSIFileManager::GetHandler("/vsimem/"));

    if( pszFilename == NULL )
        return NULL;

    CPLString osFilename = pszFilename;
    VSIMemFilesystemHandler::NormalizePath( osFilename );

    VSIMemFile *poFile = new VSIMemFile;

    poFile->osFilename = osFilename;
    poFile->bOwnData = CPL_TO_BOOL(bTakeOwnership);
    poFile->pabyData = pabyData;
    poFile->nLength = nDataLength;
    poFile->nAllocLength = nDataLength;

    {
        CPLMutexHolder oHolder( &poHandler->hMutex );
        poHandler->Unlink_unlocked(osFilename);
        poHandler->oFileList[poFile->osFilename] = poFile;
        CPLAtomicInc(&(poFile->nRefCount));
    }

    // TODO(schwehr): Fix this so that the using statement is not needed.
    // Will just adding the bool for bSetError be okay?
    return reinterpret_cast<VSILFILE *>( poHandler->Open( osFilename, "r+" ) );
}

/************************************************************************/
/*                        VSIGetMemFileBuffer()                         */
/************************************************************************/

/**
 * \brief Fetch buffer underlying memory file.
 *
 * This function returns a pointer to the memory buffer underlying a
 * virtual "in memory" file.  If bUnlinkAndSeize is TRUE the filesystem
 * object will be deleted, and ownership of the buffer will pass to the
 * caller otherwise the underlying file will remain in existence.
 *
 * @param pszFilename the name of the file to grab the buffer of.
 * @param pnDataLength (file) length returned in this variable.
 * @param bUnlinkAndSeize TRUE to remove the file, or FALSE to leave unaltered.
 *
 * @return pointer to memory buffer or NULL on failure.
 */

GByte *VSIGetMemFileBuffer( const char *pszFilename,
                            vsi_l_offset *pnDataLength,
                            int bUnlinkAndSeize )

{
    VSIMemFilesystemHandler *poHandler =
        static_cast<VSIMemFilesystemHandler *>(
            VSIFileManager::GetHandler("/vsimem/"));

    if( pszFilename == NULL )
        return NULL;

    CPLString osFilename = pszFilename;
    VSIMemFilesystemHandler::NormalizePath( osFilename );

    CPLMutexHolder oHolder( &poHandler->hMutex );

    if( poHandler->oFileList.find(osFilename) == poHandler->oFileList.end() )
        return NULL;

    VSIMemFile *poFile = poHandler->oFileList[osFilename];
    GByte *pabyData = poFile->pabyData;
    if( pnDataLength != NULL )
        *pnDataLength = poFile->nLength;

    if( bUnlinkAndSeize )
    {
        if( !poFile->bOwnData )
            CPLDebug( "VSIMemFile",
                      "File doesn't own data in VSIGetMemFileBuffer!" );
        else
            poFile->bOwnData = false;

        poHandler->oFileList.erase( poHandler->oFileList.find(osFilename) );
        CPLAtomicDec(&(poFile->nRefCount));
        delete poFile;
    }

    return pabyData;
}
