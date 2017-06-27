/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation VSI*L File API and other file system access
 *           methods going through file virtualization.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <cassert>
#include <cstring>
#include <cstdarg>
#include <cstddef>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi_virtual.h"


CPL_CVSID("$Id$")

/************************************************************************/
/*                             VSIReadDir()                             */
/************************************************************************/

/**
 * \brief Read names in a directory.
 *
 * This function abstracts access to directory contains.  It returns a
 * list of strings containing the names of files, and directories in this
 * directory.  The resulting string list becomes the responsibility of the
 * application and should be freed with CSLDestroy() when no longer needed.
 *
 * Note that no error is issued via CPLError() if the directory path is
 * invalid, though NULL is returned.
 *
 * This function used to be known as CPLReadDir(), but the old name is now
 * deprecated.
 *
 * @param pszPath the relative, or absolute path of a directory to read.
 * UTF-8 encoded.
 * @return The list of entries in the directory, or NULL if the directory
 * doesn't exist.  Filenames are returned in UTF-8 encoding.
 */

char **VSIReadDir( const char *pszPath )
{
    return VSIReadDirEx(pszPath, 0);
}

/************************************************************************/
/*                             VSIReadDirEx()                           */
/************************************************************************/

/**
 * \brief Read names in a directory.
 *
 * This function abstracts access to directory contains.  It returns a
 * list of strings containing the names of files, and directories in this
 * directory.  The resulting string list becomes the responsibility of the
 * application and should be freed with CSLDestroy() when no longer needed.
 *
 * Note that no error is issued via CPLError() if the directory path is
 * invalid, though NULL is returned.
 *
 * If nMaxFiles is set to a positive number, directory listing will stop after
 * that limit has been reached. Note that to indicate truncate, at least one
 * element more than the nMaxFiles limit will be returned. If CSLCount() on the
 * result is lesser or equal to nMaxFiles, then no truncation occurred.
 *
 * @param pszPath the relative, or absolute path of a directory to read.
 * UTF-8 encoded.
 * @param nMaxFiles maximum number of files after which to stop, or 0 for no
 * limit.
 * @return The list of entries in the directory, or NULL if the directory
 * doesn't exist.  Filenames are returned in UTF-8 encoding.
 * @since GDAL 2.1
 */

char **VSIReadDirEx( const char *pszPath, int nMaxFiles )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszPath );

    return poFSHandler->ReadDirEx( pszPath, nMaxFiles );
}

/************************************************************************/
/*                             VSIReadRecursive()                       */
/************************************************************************/

typedef struct
{
    char **papszFiles;
    int nCount;
    int i;
    char* pszPath;
    char* pszDisplayedPath;
} VSIReadDirRecursiveTask;

/**
 * \brief Read names in a directory recursively.
 *
 * This function abstracts access to directory contents and subdirectories.
 * It returns a list of strings containing the names of files and directories
 * in this directory and all subdirectories.  The resulting string list becomes
 * the responsibility of the application and should be freed with CSLDestroy()
 *  when no longer needed.
 *
 * Note that no error is issued via CPLError() if the directory path is
 * invalid, though NULL is returned.
 *
 * @param pszPathIn the relative, or absolute path of a directory to read.
 * UTF-8 encoded.
 *
 * @return The list of entries in the directory and subdirectories
 * or NULL if the directory doesn't exist.  Filenames are returned in UTF-8
 * encoding.
 * @since GDAL 1.10.0
 *
 */

char **VSIReadDirRecursive( const char *pszPathIn )
{
    CPLStringList oFiles = NULL;
    char **papszFiles = NULL;
    VSIStatBufL psStatBuf;
    CPLString osTemp1;
    CPLString osTemp2;
    int i = 0;
    int nCount = -1;

    std::vector<VSIReadDirRecursiveTask> aoStack;
    char* pszPath = CPLStrdup(pszPathIn);
    char* pszDisplayedPath = NULL;

    while( true )
    {
        if( nCount < 0 )
        {
            // Get listing.
            papszFiles = VSIReadDir( pszPath );

            // Get files and directories inside listing.
            nCount = papszFiles ? CSLCount( papszFiles ) : 0;
            i = 0;
        }

        for( ; i < nCount; i++ )
        {
            // Do not recurse up the tree.
            if( EQUAL(".", papszFiles[i]) || EQUAL("..", papszFiles[i]) )
              continue;

            // Build complete file name for stat.
            osTemp1.clear();
            osTemp1.append( pszPath );
            osTemp1.append( "/" );
            osTemp1.append( papszFiles[i] );

            // If is file, add it.
            if( VSIStatL( osTemp1.c_str(), &psStatBuf ) != 0 )
                continue;

            if( VSI_ISREG( psStatBuf.st_mode ) )
            {
                if( pszDisplayedPath )
                {
                    osTemp1.clear();
                    osTemp1.append( pszDisplayedPath );
                    osTemp1.append( "/" );
                    osTemp1.append( papszFiles[i] );
                    oFiles.AddString( osTemp1 );
                }
                else
                    oFiles.AddString( papszFiles[i] );
            }
            else if( VSI_ISDIR( psStatBuf.st_mode ) )
            {
                // Add directory entry.
                osTemp2.clear();
                if( pszDisplayedPath )
                {
                    osTemp2.append( pszDisplayedPath );
                    osTemp2.append( "/" );
                }
                osTemp2.append( papszFiles[i] );
                osTemp2.append( "/" );
                oFiles.AddString( osTemp2.c_str() );

                VSIReadDirRecursiveTask sTask;
                sTask.papszFiles = papszFiles;
                sTask.nCount = nCount;
                sTask.i = i;
                sTask.pszPath = CPLStrdup(pszPath);
                sTask.pszDisplayedPath =
                    pszDisplayedPath ? CPLStrdup(pszDisplayedPath) : NULL;
                aoStack.push_back(sTask);

                CPLFree(pszPath);
                pszPath = CPLStrdup( osTemp1.c_str() );

                char* pszDisplayedPathNew = NULL;
                if( pszDisplayedPath )
                {
                    pszDisplayedPathNew =
                        CPLStrdup(
                            CPLSPrintf("%s/%s",
                                       pszDisplayedPath, papszFiles[i]));
                }
                else
                {
                    pszDisplayedPathNew = CPLStrdup( papszFiles[i] );
                }
                CPLFree(pszDisplayedPath);
                pszDisplayedPath = pszDisplayedPathNew;

                i = 0;
                papszFiles = NULL;
                nCount = -1;

                break;
            }
        }

        if( nCount >= 0 )
        {
            CSLDestroy( papszFiles );

            if( !aoStack.empty() )
            {
                const int iLast = static_cast<int>(aoStack.size()) - 1;
                CPLFree(pszPath);
                CPLFree(pszDisplayedPath);
                nCount = aoStack[iLast].nCount;
                papszFiles = aoStack[iLast].papszFiles;
                i = aoStack[iLast].i + 1;
                pszPath = aoStack[iLast].pszPath;
                pszDisplayedPath = aoStack[iLast].pszDisplayedPath;

                aoStack.resize(iLast);
            }
            else
            {
                break;
            }
        }
    }

    CPLFree(pszPath);
    CPLFree(pszDisplayedPath);

    return oFiles.StealList();
}

/************************************************************************/
/*                             CPLReadDir()                             */
/*                                                                      */
/*      This is present only to provide ABI compatibility with older    */
/*      versions.                                                       */
/************************************************************************/
#undef CPLReadDir

CPL_C_START
char CPL_DLL **CPLReadDir( const char *pszPath );
CPL_C_END

char **CPLReadDir( const char *pszPath )
{
    return VSIReadDir( pszPath );
}

/************************************************************************/
/*                              VSIMkdir()                              */
/************************************************************************/

/**
 * \brief Create a directory.
 *
 * Create a new directory with the indicated mode.  The mode is ignored
 * on some platforms.  A reasonable default mode value would be 0666.
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX mkdir() function.
 *
 * @param pszPathname the path to the directory to create. UTF-8 encoded.
 * @param mode the permissions mode.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIMkdir( const char *pszPathname, long mode )

{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszPathname );

    return poFSHandler->Mkdir( pszPathname, mode );
}

/************************************************************************/
/*                             VSIUnlink()                              */
/************************************************************************/

/**
 * \brief Delete a file.
 *
 * Deletes a file object from the file system.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX unlink() function.
 *
 * @param pszFilename the path of the file to be deleted. UTF-8 encoded.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIUnlink( const char * pszFilename )

{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->Unlink( pszFilename );
}

/************************************************************************/
/*                             VSIRename()                              */
/************************************************************************/

/**
 * \brief Rename a file.
 *
 * Renames a file object in the file system.  It should be possible
 * to rename a file onto a new filesystem, but it is safest if this
 * function is only used to rename files that remain in the same directory.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX rename() function.
 *
 * @param oldpath the name of the file to be renamed.  UTF-8 encoded.
 * @param newpath the name the file should be given.  UTF-8 encoded.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIRename( const char * oldpath, const char * newpath )

{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( oldpath );

    return poFSHandler->Rename( oldpath, newpath );
}

/************************************************************************/
/*                              VSIRmdir()                              */
/************************************************************************/

/**
 * \brief Delete a directory.
 *
 * Deletes a directory object from the file system.  On some systems
 * the directory must be empty before it can be deleted.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX rmdir() function.
 *
 * @param pszDirname the path of the directory to be deleted.  UTF-8 encoded.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIRmdir( const char * pszDirname )

{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszDirname );

    return poFSHandler->Rmdir( pszDirname );
}

/************************************************************************/
/*                              VSIStatL()                              */
/************************************************************************/

/**
 * \brief Get filesystem object info.
 *
 * Fetches status information about a filesystem object (file, directory, etc).
 * The returned information is placed in the VSIStatBufL structure.   For
 * portability, only use the st_size (size in bytes) and st_mode (file type).
 * This method is similar to VSIStat(), but will work on large files on
 * systems where this requires special calls.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX stat() function.
 *
 * @param pszFilename the path of the filesystem object to be queried.
 * UTF-8 encoded.
 * @param psStatBuf the structure to load with information.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIStatL( const char * pszFilename, VSIStatBufL *psStatBuf )

{
    return VSIStatExL(pszFilename, psStatBuf, 0);
}

/************************************************************************/
/*                            VSIStatExL()                              */
/************************************************************************/

/**
 * \brief Get filesystem object info.
 *
 * Fetches status information about a filesystem object (file, directory, etc).
 * The returned information is placed in the VSIStatBufL structure.   For
 * portability, only use the st_size (size in bytes) and st_mode (file type).
 * This method is similar to VSIStat(), but will work on large files on
 * systems where this requires special calls.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX stat() function, with an extra parameter to
 * specify which information is needed, which offers a potential for
 * speed optimizations on specialized and potentially slow virtual
 * filesystem objects (/vsigzip/, /vsicurl/)
 *
 * @param pszFilename the path of the filesystem object to be queried.
 * UTF-8 encoded.
 * @param psStatBuf the structure to load with information.
 * @param nFlags 0 to get all information, or VSI_STAT_EXISTS_FLAG,
 *                  VSI_STAT_NATURE_FLAG or VSI_STAT_SIZE_FLAG, or a
 *                  combination of those to get partial info.
 *
 * @return 0 on success or -1 on an error.
 *
 * @since GDAL 1.8.0
 */

int VSIStatExL( const char * pszFilename, VSIStatBufL *psStatBuf, int nFlags )

{
    char szAltPath[4] = { '\0' };

    // Enable to work on "C:" as if it were "C:\".
    if( strlen(pszFilename) == 2 && pszFilename[1] == ':' )
    {
        szAltPath[0] = pszFilename[0];
        szAltPath[1] = pszFilename[1];
        szAltPath[2] = '\\';
        szAltPath[3] = '\0';

        pszFilename = szAltPath;
    }

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );

    if( nFlags == 0 )
        nFlags = VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG |
            VSI_STAT_SIZE_FLAG;

    return poFSHandler->Stat( pszFilename, psStatBuf, nFlags );
}

/************************************************************************/
/*                       VSIIsCaseSensitiveFS()                         */
/************************************************************************/

/**
 * \brief Returns if the filenames of the filesystem are case sensitive.
 *
 * This method retrieves to which filesystem belongs the passed filename
 * and return TRUE if the filenames of that filesystem are case sensitive.
 *
 * Currently, this will return FALSE only for Windows real filenames. Other
 * VSI virtual filesystems are case sensitive.
 *
 * This methods avoid ugly \#ifndef WIN32 / \#endif code, that is wrong when
 * dealing with virtual filenames.
 *
 * @param pszFilename the path of the filesystem object to be tested.
 * UTF-8 encoded.
 *
 * @return TRUE if the filenames of the filesystem are case sensitive.
 *
 * @since GDAL 1.8.0
 */

int VSIIsCaseSensitiveFS( const char * pszFilename )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->IsCaseSensitive( pszFilename );
}

/************************************************************************/
/*                       VSISupportsSparseFiles()                       */
/************************************************************************/

/**
 * \brief Returns if the filesystem supports sparse files.
 *
 * Only supported on Linux (and no other Unix derivatives) and
 * Windows.  On Linux, the answer depends on a few hardcoded
 * signatures for common filesystems. Other filesystems will be
 * considered as not supporting sparse files.
 *
 * @param pszPath the path of the filesystem object to be tested.
 * UTF-8 encoded.
 *
 * @return TRUE if the file system is known to support sparse files. FALSE may
 *              be returned both in cases where it is known to not support them,
 *              or when it is unknown.
 *
 * @since GDAL 2.2
 */

int VSISupportsSparseFiles( const char* pszPath )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszPath );

    return poFSHandler->SupportsSparseFiles( pszPath );
}

/************************************************************************/
/*                             VSIFOpenL()                              */
/************************************************************************/

/**
 * \brief Open file.
 *
 * This function opens a file with the desired access.  Large files (larger
 * than 2GB) should be supported.  Binary access is always implied and
 * the "b" does not need to be included in the pszAccess string.
 *
 * Note that the "VSILFILE *" returned since GDAL 1.8.0 by this function is
 * *NOT* a standard C library FILE *, and cannot be used with any functions
 * other than the "VSI*L" family of functions.  They aren't "real" FILE objects.
 *
 * On windows it is possible to define the configuration option
 * GDAL_FILE_IS_UTF8 to have pszFilename treated as being in the local
 * encoding instead of UTF-8, restoring the pre-1.8.0 behavior of VSIFOpenL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fopen() function.
 *
 * @param pszFilename the file to open.  UTF-8 encoded.
 * @param pszAccess access requested (i.e. "r", "r+", "w")
 *
 * @return NULL on failure, or the file handle.
 */

VSILFILE *VSIFOpenL( const char * pszFilename, const char * pszAccess )

{
    return VSIFOpenExL(pszFilename, pszAccess, false);
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

#ifndef DOXYGEN_SKIP

VSIVirtualHandle *VSIFilesystemHandler::Open( const char *pszFilename,
                                              const char *pszAccess )
{
    return Open(pszFilename, pszAccess, false);
}

#endif

/************************************************************************/
/*                             VSIFOpenExL()                            */
/************************************************************************/

/**
 * \brief Open file.
 *
 * This function opens a file with the desired access.  Large files (larger
 * than 2GB) should be supported.  Binary access is always implied and
 * the "b" does not need to be included in the pszAccess string.
 *
 * Note that the "VSILFILE *" returned by this function is
 * *NOT* a standard C library FILE *, and cannot be used with any functions
 * other than the "VSI*L" family of functions.  They aren't "real" FILE objects.
 *
 * On windows it is possible to define the configuration option
 * GDAL_FILE_IS_UTF8 to have pszFilename treated as being in the local
 * encoding instead of UTF-8, restoring the pre-1.8.0 behavior of VSIFOpenL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fopen() function.
 *
 * @param pszFilename the file to open.  UTF-8 encoded.
 * @param pszAccess access requested (i.e. "r", "r+", "w")
 * @param bSetError flag determining whether or not this open call
 * should set VSIErrors on failure.
 *
 * @return NULL on failure, or the file handle.
 *
 * @since GDAL 2.1
 */

VSILFILE *VSIFOpenExL( const char * pszFilename, const char * pszAccess,
                       int bSetError )

{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );

    VSILFILE* fp = reinterpret_cast<VSILFILE *>(
        poFSHandler->Open( pszFilename, pszAccess, CPL_TO_BOOL(bSetError) ) );

    VSIDebug4( "VSIFOpenExL(%s,%s,%d) = %p",
               pszFilename, pszAccess, bSetError, fp );

    return fp;
}

/************************************************************************/
/*                             VSIFCloseL()                             */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Close()
 * \brief Close file.
 *
 * This function closes the indicated file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fclose() function.
 *
 * @return 0 on success or -1 on failure.
 */

/**
 * \brief Close file.
 *
 * This function closes the indicated file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fclose() function.
 *
 * @param fp file handle opened with VSIFOpenL().  Passing a nullptr produces
 * undefined behavior.
 *
 * @return 0 on success or -1 on failure.
 */

int VSIFCloseL( VSILFILE * fp )

{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>( fp );

    VSIDebug1( "VSIFCloseL(%p)", fp );

    const int nResult = poFileHandle->Close();

    delete poFileHandle;

    return nResult;
}

/************************************************************************/
/*                             VSIFSeekL()                              */
/************************************************************************/

/**
 * \fn int VSIVirtualHandle::Seek( vsi_l_offset nOffset, int nWhence )
 * \brief Seek to requested offset.
 *
 * Seek to the desired offset (nOffset) in the indicated file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fseek() call.
 *
 * Caution: vsi_l_offset is a unsigned type, so SEEK_CUR can only be used
 * for positive seek. If negative seek is needed, use
 * handle->Seek( handle->Tell() + negative_offset, SEEK_SET ).
 *
 * @param nOffset offset in bytes.
 * @param nWhence one of SEEK_SET, SEEK_CUR or SEEK_END.
 *
 * @return 0 on success or -1 one failure.
 */

/**
 * \brief Seek to requested offset.
 *
 * Seek to the desired offset (nOffset) in the indicated file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fseek() call.
 *
 * Caution: vsi_l_offset is a unsigned type, so SEEK_CUR can only be used
 * for positive seek. If negative seek is needed, use
 * VSIFSeekL( fp, VSIFTellL(fp) + negative_offset, SEEK_SET ).
 * 
 * @param fp file handle opened with VSIFOpenL().
 * @param nOffset offset in bytes.
 * @param nWhence one of SEEK_SET, SEEK_CUR or SEEK_END.
 *
 * @return 0 on success or -1 one failure.
 */

int VSIFSeekL( VSILFILE * fp, vsi_l_offset nOffset, int nWhence )

{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>(fp);

    return poFileHandle->Seek( nOffset, nWhence );
}

/************************************************************************/
/*                             VSIFTellL()                              */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Tell()
 * \brief Tell current file offset.
 *
 * Returns the current file read/write offset in bytes from the beginning of
 * the file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX ftell() call.
 *
 * @return file offset in bytes.
 */

/**
 * \brief Tell current file offset.
 *
 * Returns the current file read/write offset in bytes from the beginning of
 * the file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX ftell() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return file offset in bytes.
 */

vsi_l_offset VSIFTellL( VSILFILE * fp )

{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>( fp );

    return poFileHandle->Tell();
}

/************************************************************************/
/*                             VSIRewindL()                             */
/************************************************************************/

/**
 * \brief Rewind the file pointer to the beginning of the file.
 *
 * This is equivalent to VSIFSeekL( fp, 0, SEEK_SET )
 *
 * Analog of the POSIX rewind() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 */

void VSIRewindL( VSILFILE * fp )

{
    CPL_IGNORE_RET_VAL(VSIFSeekL( fp, 0, SEEK_SET ));
}

/************************************************************************/
/*                             VSIFFlushL()                             */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Flush()
 * \brief Flush pending writes to disk.
 *
 * For files in write or update mode and on filesystem types where it is
 * applicable, all pending output on the file is flushed to the physical disk.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fflush() call.
 *
 * @return 0 on success or -1 on error.
 */

/**
 * \brief Flush pending writes to disk.
 *
 * For files in write or update mode and on filesystem types where it is
 * applicable, all pending output on the file is flushed to the physical disk.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fflush() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return 0 on success or -1 on error.
 */

int VSIFFlushL( VSILFILE * fp )

{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>( fp );

    return poFileHandle->Flush();
}

/************************************************************************/
/*                             VSIFReadL()                              */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Read( void *pBuffer, size_t nSize, size_t nCount )
 * \brief Read bytes from file.
 *
 * Reads nCount objects of nSize bytes from the indicated file at the
 * current offset into the indicated buffer.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fread() call.
 *
 * @param pBuffer the buffer into which the data should be read (at least
 * nCount * nSize bytes in size.
 * @param nSize size of objects to read in bytes.
 * @param nCount number of objects to read.
 *
 * @return number of objects successfully read.
 */

/**
 * \brief Read bytes from file.
 *
 * Reads nCount objects of nSize bytes from the indicated file at the
 * current offset into the indicated buffer.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fread() call.
 *
 * @param pBuffer the buffer into which the data should be read (at least
 * nCount * nSize bytes in size.
 * @param nSize size of objects to read in bytes.
 * @param nCount number of objects to read.
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return number of objects successfully read.
 */

size_t VSIFReadL( void * pBuffer, size_t nSize, size_t nCount, VSILFILE * fp )

{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>( fp );

    return poFileHandle->Read( pBuffer, nSize, nCount );
}

/************************************************************************/
/*                       VSIFReadMultiRangeL()                          */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::ReadMultiRange( int nRanges, void ** ppData,
 *                                       const vsi_l_offset* panOffsets,
 *                                       const size_t* panSizes )
 * \brief Read several ranges of bytes from file.
 *
 * Reads nRanges objects of panSizes[i] bytes from the indicated file at the
 * offset panOffsets[i] into the buffer ppData[i].
 *
 * Ranges must be sorted in ascending start offset, and must not overlap each
 * other.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory or /vsicurl/.
 *
 * @param nRanges number of ranges to read.
 * @param ppData array of nRanges buffer into which the data should be read
 *               (ppData[i] must be at list panSizes[i] bytes).
 * @param panOffsets array of nRanges offsets at which the data should be read.
 * @param panSizes array of nRanges sizes of objects to read (in bytes).
 *
 * @return 0 in case of success, -1 otherwise.
 * @since GDAL 1.9.0
 */

/**
 * \brief Read several ranges of bytes from file.
 *
 * Reads nRanges objects of panSizes[i] bytes from the indicated file at the
 * offset panOffsets[i] into the buffer ppData[i].
 *
 * Ranges must be sorted in ascending start offset, and must not overlap each
 * other.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory or /vsicurl/.
 *
 * @param nRanges number of ranges to read.
 * @param ppData array of nRanges buffer into which the data should be read
 *               (ppData[i] must be at list panSizes[i] bytes).
 * @param panOffsets array of nRanges offsets at which the data should be read.
 * @param panSizes array of nRanges sizes of objects to read (in bytes).
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return 0 in case of success, -1 otherwise.
 * @since GDAL 1.9.0
 */

int VSIFReadMultiRangeL( int nRanges, void ** ppData,
                         const vsi_l_offset* panOffsets,
                         const size_t* panSizes, VSILFILE * fp )
{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>(fp);

    return poFileHandle->ReadMultiRange(nRanges, ppData, panOffsets, panSizes);
}

/************************************************************************/
/*                             VSIFWriteL()                             */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Write( const void *pBuffer,
 *                              size_t nSize, size_t nCount )
 * \brief Write bytes to file.
 *
 * Writess nCount objects of nSize bytes to the indicated file at the
 * current offset into the indicated buffer.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fwrite() call.
 *
 * @param pBuffer the buffer from which the data should be written (at least
 * nCount * nSize bytes in size.
 * @param nSize size of objects to read in bytes.
 * @param nCount number of objects to read.
 *
 * @return number of objects successfully written.
 */

/**
 * \brief Write bytes to file.
 *
 * Writess nCount objects of nSize bytes to the indicated file at the
 * current offset into the indicated buffer.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fwrite() call.
 *
 * @param pBuffer the buffer from which the data should be written (at least
 * nCount * nSize bytes in size.
 * @param nSize size of objects to read in bytes.
 * @param nCount number of objects to read.
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return number of objects successfully written.
 */

size_t VSIFWriteL( const void *pBuffer, size_t nSize, size_t nCount,
                   VSILFILE *fp )

{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>( fp );

    return poFileHandle->Write( pBuffer, nSize, nCount );
}

/************************************************************************/
/*                              VSIFEofL()                              */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Eof()
 * \brief Test for end of file.
 *
 * Returns TRUE (non-zero) if an end-of-file condition occurred during the
 * previous read operation. The end-of-file flag is cleared by a successful
 * VSIFSeekL() call.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX feof() call.
 *
 * @return TRUE if at EOF else FALSE.
 */

/**
 * \brief Test for end of file.
 *
 * Returns TRUE (non-zero) if an end-of-file condition occurred during the
 * previous read operation. The end-of-file flag is cleared by a successful
 * VSIFSeekL() call.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX feof() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return TRUE if at EOF else FALSE.
 */

int VSIFEofL( VSILFILE * fp )

{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>( fp );

    return poFileHandle->Eof();
}

/************************************************************************/
/*                            VSIFTruncateL()                           */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Truncate( vsi_l_offset nNewSize )
 * \brief Truncate/expand the file to the specified size

 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX ftruncate() call.
 *
 * @param nNewSize new size in bytes.
 *
 * @return 0 on success
 * @since GDAL 1.9.0
 */

/**
 * \brief Truncate/expand the file to the specified size

 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX ftruncate() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 * @param nNewSize new size in bytes.
 *
 * @return 0 on success
 * @since GDAL 1.9.0
 */

int VSIFTruncateL( VSILFILE * fp, vsi_l_offset nNewSize )

{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>( fp );

    return poFileHandle->Truncate(nNewSize);
}

/************************************************************************/
/*                            VSIFPrintfL()                             */
/************************************************************************/

/**
 * \brief Formatted write to file.
 *
 * Provides fprintf() style formatted output to a VSI*L file.  This formats
 * an internal buffer which is written using VSIFWriteL().
 *
 * Analog of the POSIX fprintf() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 * @param pszFormat the printf() style format string.
 *
 * @return the number of bytes written or -1 on an error.
 */

int VSIFPrintfL( VSILFILE *fp, CPL_FORMAT_STRING(const char *pszFormat), ... )

{
    va_list args;

    va_start( args, pszFormat );
    CPLString osResult;
    osResult.vPrintf( pszFormat, args );
    va_end( args );

    return static_cast<int>(
        VSIFWriteL( osResult.c_str(), 1, osResult.length(), fp ));
}

/************************************************************************/
/*                              VSIFPutcL()                              */
/************************************************************************/

// TODO: should we put in conformance with POSIX regarding the return
// value. As of today (2015-08-29), no code in GDAL sources actually
// check the return value.

/**
 * \brief Write a single byte to the file
 *
 * Writes the character nChar, cast to an unsigned char, to file.
 *
 * Almost an analog of the POSIX  fputc() call, except that it returns
 * the  number of  character  written (1  or 0),  and  not the  (cast)
 * character itself or EOF.
 *
 * @param nChar character to write.
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return 1 in case of success, 0 on error.
 */

int VSIFPutcL( int nChar, VSILFILE * fp )

{
    const unsigned char cChar = static_cast<unsigned char>(nChar);
    return static_cast<int>(VSIFWriteL(&cChar, 1, 1, fp));
}

/************************************************************************/
/*                        VSIFGetRangeStatusL()                        */
/************************************************************************/

// TODO(rouault): "exte,t" in r34586?

/**
 * \fn VSIVirtualHandle::GetRangeStatus( vsi_l_offset nOffset,
 *                                       vsi_l_offset nLength )
 * \brief Return if a given file range contains data or holes filled with zeroes
 *
 * This uses the filesystem capabilities of querying which regions of
 * a sparse file are allocated or not. This is currently only
 * implemented for Linux (and no other Unix derivatives) and Windows.
 *
 * Note: A return of VSI_RANGE_STATUS_DATA doesn't exclude that the
 * exte,t is filled with zeroes! It must be interpreted as "may
 * contain non-zero data".
 *
 * @param nOffset offset of the start of the extent.
 * @param nLength extent length.
 *
 * @return extent status: VSI_RANGE_STATUS_UNKNOWN, VSI_RANGE_STATUS_DATA or
 *         VSI_RANGE_STATUS_HOLE
 * @since GDAL 2.2
 */

/**
 * \brief Return if a given file range contains data or holes filled with zeroes
 *
 * This uses the filesystem capabilities of querying which regions of
 * a sparse file are allocated or not. This is currently only
 * implemented for Linux (and no other Unix derivatives) and Windows.
 *
 * Note: A return of VSI_RANGE_STATUS_DATA doesn't exclude that the
 * exte,t is filled with zeroes! It must be interpreted as "may
 * contain non-zero data".
 *
 * @param fp file handle opened with VSIFOpenL().
 * @param nOffset offset of the start of the extent.
 * @param nLength extent length.
 *
 * @return extent status: VSI_RANGE_STATUS_UNKNOWN, VSI_RANGE_STATUS_DATA or
 *         VSI_RANGE_STATUS_HOLE
 * @since GDAL 2.2
 */

VSIRangeStatus VSIFGetRangeStatusL( VSILFILE * fp, vsi_l_offset nOffset,
                                    vsi_l_offset nLength )
{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>( fp );

    return poFileHandle->GetRangeStatus(nOffset, nLength);
}

/************************************************************************/
/*                           VSIIngestFile()                            */
/************************************************************************/

/**
 * \brief Ingest a file into memory.
 *
 * Read the whole content of a file into a memory buffer.
 *
 * Either fp or pszFilename can be NULL, but not both at the same time.
 *
 * If fp is passed non-NULL, it is the responsibility of the caller to
 * close it.
 *
 * If non-NULL, the returned buffer is guaranteed to be NUL-terminated.
 *
 * @param fp file handle opened with VSIFOpenL().
 * @param pszFilename filename.
 * @param ppabyRet pointer to the target buffer. *ppabyRet must be freed with
 *                 VSIFree()
 * @param pnSize pointer to variable to store the file size. May be NULL.
 * @param nMaxSize maximum size of file allowed. If no limit, set to a negative
 *                 value.
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 1.11
 */

int VSIIngestFile( VSILFILE* fp,
                   const char* pszFilename,
                   GByte** ppabyRet,
                   vsi_l_offset* pnSize,
                   GIntBig nMaxSize )
{
    if( fp == NULL && pszFilename == NULL )
        return FALSE;
    if( ppabyRet == NULL )
        return FALSE;

    *ppabyRet = NULL;
    if( pnSize != NULL )
        *pnSize = 0;

    bool bFreeFP = false;
    if( NULL == fp )
    {
        fp = VSIFOpenL( pszFilename, "rb" );
        if( NULL == fp )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Cannot open file '%s'", pszFilename );
            return FALSE;
        }
        bFreeFP = true;
    }
    else
    {
        if( VSIFSeekL(fp, 0, SEEK_SET) != 0 )
            return FALSE;
    }

    vsi_l_offset nDataLen = 0;

    if( pszFilename == NULL ||
        strcmp(pszFilename, "/vsistdin/") == 0 )
    {
        vsi_l_offset nDataAlloc = 0;
        if( VSIFSeekL( fp, 0, SEEK_SET ) != 0 )
        {
            if( bFreeFP )
                CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
            return FALSE;
        }
        while( true )
        {
            if( nDataLen + 8192 + 1 > nDataAlloc )
            {
                nDataAlloc = (nDataAlloc * 4) / 3 + 8192 + 1;
                if( nDataAlloc >
                    static_cast<vsi_l_offset>(static_cast<size_t>(nDataAlloc)) )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Input file too large to be opened" );
                    VSIFree( *ppabyRet );
                    *ppabyRet = NULL;
                    if( bFreeFP )
                        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
                    return FALSE;
                }
                GByte* pabyNew = static_cast<GByte *>(
                    VSIRealloc(*ppabyRet, static_cast<size_t>(nDataAlloc)) );
                if( pabyNew == NULL )
                {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "Cannot allocated " CPL_FRMT_GIB " bytes",
                              nDataAlloc );
                    VSIFree( *ppabyRet );
                    *ppabyRet = NULL;
                    if( bFreeFP )
                        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
                    return FALSE;
                }
                *ppabyRet = pabyNew;
            }
            const int nRead = static_cast<int>(
                VSIFReadL( *ppabyRet + nDataLen, 1, 8192, fp ) );
            nDataLen += nRead;

            if( nMaxSize >= 0 &&
                nDataLen > static_cast<vsi_l_offset>(nMaxSize) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Input file too large to be opened" );
                VSIFree( *ppabyRet );
                *ppabyRet = NULL;
                if( pnSize != NULL )
                    *pnSize = 0;
                if( bFreeFP )
                    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
                return FALSE;
            }

            if( pnSize != NULL )
                *pnSize += nRead;
            (*ppabyRet)[nDataLen] = '\0';
            if( nRead == 0 )
                break;
        }
    }
    else
    {
        if( VSIFSeekL( fp, 0, SEEK_END ) != 0 )
        {
            if( bFreeFP )
                CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
            return FALSE;
        }
        nDataLen = VSIFTellL( fp );

        // With "large" VSI I/O API we can read data chunks larger than
        // VSIMalloc could allocate. Catch it here.
        if( nDataLen != static_cast<vsi_l_offset>(static_cast<size_t>(nDataLen))
            || nDataLen + 1 < nDataLen
            || (nMaxSize >= 0 &&
                nDataLen > static_cast<vsi_l_offset>(nMaxSize)) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Input file too large to be opened" );
            if( bFreeFP )
                CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
            return FALSE;
        }

        if( VSIFSeekL( fp, 0, SEEK_SET ) != 0 )
        {
            if( bFreeFP )
                CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
            return FALSE;
        }

        *ppabyRet = static_cast<GByte *>(
            VSIMalloc(static_cast<size_t>(nDataLen + 1)) );
        if( NULL == *ppabyRet )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Cannot allocated " CPL_FRMT_GIB " bytes",
                      nDataLen + 1 );
            if( bFreeFP )
                CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
            return FALSE;
        }

        (*ppabyRet)[nDataLen] = '\0';
        if( nDataLen !=
            VSIFReadL(*ppabyRet, 1, static_cast<size_t>(nDataLen), fp) )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Cannot read " CPL_FRMT_GIB " bytes",
                      nDataLen );
            VSIFree( *ppabyRet );
            *ppabyRet = NULL;
            if( bFreeFP )
                CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
            return FALSE;
        }
        if( pnSize != NULL )
            *pnSize = nDataLen;
    }
    if( bFreeFP )
        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
    return TRUE;
}

/************************************************************************/
/*                        VSIFGetNativeFileDescriptorL()                */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::GetNativeFileDescriptor()
 * \brief Returns the "native" file descriptor for the virtual handle.
 *
 * This will only return a non-NULL value for "real" files handled by the
 * operating system (to be opposed to GDAL virtual file systems).
 *
 * On POSIX systems, this will be a integer value ("fd") cast as a void*.
 * On Windows systems, this will be the HANDLE.
 *
 * @return the native file descriptor, or NULL.
 */

/**
 * \brief Returns the "native" file descriptor for the virtual handle.
 *
 * This will only return a non-NULL value for "real" files handled by the
 * operating system (to be opposed to GDAL virtual file systems).
 *
 * On POSIX systems, this will be a integer value ("fd") cast as a void*.
 * On Windows systems, this will be the HANDLE.
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return the native file descriptor, or NULL.
 */

void *VSIFGetNativeFileDescriptorL( VSILFILE* fp )
{
    VSIVirtualHandle *poFileHandle = reinterpret_cast<VSIVirtualHandle *>( fp );

    return poFileHandle->GetNativeFileDescriptor();
}

/************************************************************************/
/*                      VSIGetDiskFreeSpace()                           */
/************************************************************************/

/**
 * \brief Return free disk space available on the filesystem.
 *
 * This function returns the free disk space available on the filesystem.
 *
 * @param pszDirname a directory of the filesystem to query.
 * @return The free space in bytes. Or -1 in case of error.
 * @since GDAL 2.1
 */

GIntBig VSIGetDiskFreeSpace( const char *pszDirname )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszDirname );

    return poFSHandler->GetDiskFreeSpace( pszDirname );
}

/************************************************************************/
/* ==================================================================== */
/*                           VSIFileManager()                           */
/* ==================================================================== */
/************************************************************************/

#ifndef DOXYGEN_SKIP

/*
** Notes on Multithreading:
**
** The VSIFileManager maintains a list of file type handlers (mem, large
** file, etc).  It should be thread safe as long as all the handlers are
** instantiated before multiple threads begin to operate.
**/

/************************************************************************/
/*                           VSIFileManager()                           */
/************************************************************************/

VSIFileManager::VSIFileManager() :
    poDefaultHandler(NULL)
{}

/************************************************************************/
/*                          ~VSIFileManager()                           */
/************************************************************************/

VSIFileManager::~VSIFileManager()
{
    for( std::map<std::string, VSIFilesystemHandler*>::const_iterator iter =
             oHandlers.begin();
         iter != oHandlers.end();
         ++iter )
    {
        delete iter->second;
    }

    delete poDefaultHandler;
}

/************************************************************************/
/*                                Get()                                 */
/************************************************************************/

static VSIFileManager *poManager = NULL;
static CPLMutex* hVSIFileManagerMutex = NULL;

VSIFileManager *VSIFileManager::Get()

{
    static volatile GPtrDiff_t nConstructerPID = 0;
    if( poManager != NULL )
    {
        if( nConstructerPID != 0 )
        {
            GPtrDiff_t nCurrentPID = static_cast<GPtrDiff_t>(CPLGetPID());
            if( nConstructerPID != nCurrentPID )
            {
                {
                    CPLMutexHolder oHolder( &hVSIFileManagerMutex );
                }
                if( nConstructerPID != 0 )
                {
                    VSIDebug1( "nConstructerPID != 0: %d", nConstructerPID);
                    assert(false);
                }
            }
        }
        return poManager;
    }

    CPLMutexHolder oHolder2( &hVSIFileManagerMutex );
    if( poManager == NULL )
    {
        nConstructerPID = static_cast<GPtrDiff_t>(CPLGetPID());
#ifdef DEBUG_VERBOSE
        printf("Thread " CPL_FRMT_GIB": VSIFileManager in construction\n",  // ok
               static_cast<GIntBig>(nConstructerPID));
#endif
        poManager = new VSIFileManager;
        VSIInstallLargeFileHandler();
        VSIInstallSubFileHandler();
        VSIInstallMemFileHandler();
#ifdef HAVE_LIBZ
        VSIInstallGZipFileHandler();
        VSIInstallZipFileHandler();
#endif
#ifdef HAVE_CURL
        VSIInstallCurlFileHandler();
        VSIInstallCurlStreamingFileHandler();
        VSIInstallS3FileHandler();
        VSIInstallS3StreamingFileHandler();
        VSIInstallGSFileHandler();
        VSIInstallGSStreamingFileHandler();
#endif
        VSIInstallStdinHandler();
        VSIInstallStdoutHandler();
        VSIInstallSparseFileHandler();
        VSIInstallTarFileHandler();
        VSIInstallCryptFileHandler();

#ifdef DEBUG_VERBOSE
        printf("Thread " CPL_FRMT_GIB": VSIFileManager construction finished\n",  // ok
               static_cast<GIntBig>(nConstructerPID));
#endif
        nConstructerPID = 0;
    }

    return poManager;
}

/************************************************************************/
/*                             GetHandler()                             */
/************************************************************************/

VSIFilesystemHandler *VSIFileManager::GetHandler( const char *pszPath )

{
    VSIFileManager *poThis = Get();
    const size_t nPathLen = strlen(pszPath);

    for( std::map<std::string, VSIFilesystemHandler*>::const_iterator iter =
             poThis->oHandlers.begin();
         iter != poThis->oHandlers.end();
         ++iter )
    {
        const char* pszIterKey = iter->first.c_str();
        const size_t nIterKeyLen = iter->first.size();
        if( strncmp(pszPath, pszIterKey, nIterKeyLen) == 0 )
            return iter->second;

        // "/vsimem\foo" should be handled as "/vsimem/foo".
        if( nIterKeyLen && nPathLen > nIterKeyLen &&
            pszIterKey[nIterKeyLen-1] == '/' &&
            pszPath[nIterKeyLen-1] == '\\' &&
            strncmp(pszPath, pszIterKey, nIterKeyLen - 1) == 0 )
            return iter->second;

        // /vsimem should be treated as a match for /vsimem/.
        if( nPathLen + 1 == nIterKeyLen
            && strncmp(pszPath, pszIterKey, nPathLen) == 0 )
            return iter->second;
    }

    return poThis->poDefaultHandler;
}

/************************************************************************/
/*                           InstallHandler()                           */
/************************************************************************/

void VSIFileManager::InstallHandler( const std::string& osPrefix,
                                     VSIFilesystemHandler *poHandler )

{
    if( osPrefix == "" )
        Get()->poDefaultHandler = poHandler;
    else
        Get()->oHandlers[osPrefix] = poHandler;
}

/************************************************************************/
/*                       VSICleanupFileManager()                        */
/************************************************************************/

void VSICleanupFileManager()

{
    if( poManager )
    {
        delete poManager;
        poManager = NULL;
    }

    if( hVSIFileManagerMutex != NULL )
    {
        CPLDestroyMutex(hVSIFileManagerMutex);
        hVSIFileManagerMutex = NULL;
    }
}

/************************************************************************/
/*                            Truncate()                                */
/************************************************************************/

int VSIVirtualHandle::Truncate( vsi_l_offset nNewSize )
{
    const vsi_l_offset nOriginalPos = Tell();
    if( Seek(0, SEEK_END) == 0 && nNewSize >= Tell() )
    {
        // Fill with zeroes
        std::vector<GByte> aoBytes(4096, 0);
        vsi_l_offset nCurOffset = nOriginalPos;
        while( nCurOffset < nNewSize )
        {
            const vsi_l_offset nMaxOffset = 4096;
            const int nSize =
                static_cast<int>(
                    std::min(nMaxOffset, nNewSize - nCurOffset));
            if( Write(&aoBytes[0], nSize, 1) != 1 )
            {
                Seek( nOriginalPos, SEEK_SET );
                return -1;
            }
            nCurOffset += nSize;
        }
        return Seek(nOriginalPos, SEEK_SET) == 0 ? 0 : -1;
    }

    CPLDebug("VSI",
             "Truncation is not supported in generic implementation "
             "of Truncate()");
    Seek( nOriginalPos, SEEK_SET );
    return -1;
}

/************************************************************************/
/*                           ReadMultiRange()                           */
/************************************************************************/

int VSIVirtualHandle::ReadMultiRange( int nRanges, void ** ppData,
                                      const vsi_l_offset* panOffsets,
                                      const size_t* panSizes )
{
    int nRet = 0;
    const vsi_l_offset nCurOffset = Tell();
    for( int i=0; i<nRanges; i++ )
    {
        if( Seek(panOffsets[i], SEEK_SET) < 0 )
        {
            nRet = -1;
            break;
        }

        const size_t nRead = Read(ppData[i], 1, panSizes[i]);
        if( panSizes[i] != nRead )
        {
            nRet = -1;
            break;
        }
    }

    Seek(nCurOffset, SEEK_SET);

    return nRet;
}

#endif  // #ifndef DOXYGEN_SKIP
