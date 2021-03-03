/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation VSI*L File API and other file system access
 *           methods going through file virtualization.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include <cstdarg>
#include <cstddef>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
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
/*                             VSISiblingFiles()                        */
/************************************************************************/

/**
 * \brief Return related filenames
  *
  * This function is essentially meant at being used by GDAL internals.
 *
 * @param pszFilename the path of a filename to inspect
 * UTF-8 encoded.
 * @return The list of entries, relative to the directory, of all sidecar
 * files available or NULL if the list is not known. 
 * Filenames are returned in UTF-8 encoding.
 * Most implementations will return NULL, and a subsequent ReadDir will
 * list all files available in the file's directory. This function will be
 * overridden by VSI FilesystemHandlers that wish to force e.g. an empty list
 * to avoid opening non-existant files on slow filesystems. The return value shall be destroyed with CSLDestroy()
 * @since GDAL 3.2
 */
char **VSISiblingFiles( const char *pszFilename)
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->SiblingFiles( pszFilename );
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
    CPLStringList oFiles;
    char **papszFiles = nullptr;
    VSIStatBufL psStatBuf;
    CPLString osTemp1;
    CPLString osTemp2;
    int i = 0;
    int nCount = -1;

    std::vector<VSIReadDirRecursiveTask> aoStack;
    char* pszPath = CPLStrdup(pszPathIn);
    char* pszDisplayedPath = nullptr;

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
            if( !osTemp1.empty() && osTemp1.back() != '/' )
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
                    if( !osTemp1.empty() && osTemp1.back() != '/' )
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
                if( !osTemp2.empty() && osTemp2.back() != '/' )
                    osTemp2.append( "/" );
                oFiles.AddString( osTemp2.c_str() );

                VSIReadDirRecursiveTask sTask;
                sTask.papszFiles = papszFiles;
                sTask.nCount = nCount;
                sTask.i = i;
                sTask.pszPath = CPLStrdup(pszPath);
                sTask.pszDisplayedPath =
                    pszDisplayedPath ? CPLStrdup(pszDisplayedPath) : nullptr;
                aoStack.push_back(sTask);

                CPLFree(pszPath);
                pszPath = CPLStrdup( osTemp1.c_str() );

                char* pszDisplayedPathNew = nullptr;
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
                papszFiles = nullptr;
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
/*                             VSIOpenDir()                             */
/************************************************************************/

/**
 * \brief Open a directory to read its entries.
 *
 * This function is close to the POSIX opendir() function.
 *
 * For /vsis3/, /vsigs/, /vsioss/, /vsiaz/ and /vsiadls/, this function has an efficient
 * implementation, minimizing the number of network requests, when invoked with
 * nRecurseDepth <= 0.
 *
 * Entries are read by calling VSIGetNextDirEntry() on the handled returned by
 * that function, until it returns NULL. VSICloseDir() must be called once done
 * with the returned directory handle.
 *
 * @param pszPath the relative, or absolute path of a directory to read.
 * UTF-8 encoded.
 * @param nRecurseDepth 0 means do not recurse in subdirectories, 1 means
 * recurse only in the first level of subdirectories, etc. -1 means unlimited
 * recursion level
 * @param papszOptions NULL terminated list of options, or NULL.
 *
 * @return a handle, or NULL in case of error
 * @since GDAL 2.4
 *
 */

VSIDIR *VSIOpenDir( const char *pszPath,
                    int nRecurseDepth,
                    const char* const *papszOptions)
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszPath );

    return poFSHandler->OpenDir( pszPath, nRecurseDepth, papszOptions );
}

/************************************************************************/
/*                          VSIGetNextDirEntry()                        */
/************************************************************************/

/**
 * \brief Return the next entry of the directory
 *
 * This function is close to the POSIX readdir() function. It actually returns
 * more information (file size, last modification time), which on 'real' file
 * systems involve one 'stat' call per file.
 *
 * For filesystems that can have both a regular file and a directory name of
 * the same name (typically /vsis3/), when this situation of duplicate happens,
 * the directory name will be suffixed by a slash character. Otherwise directory
 * names are not suffixed by slash.
 *
 * The returned entry remains valid until the next call to VSINextDirEntry()
 * or VSICloseDir() with the same handle.
 *
 * @param dir Directory handled returned by VSIOpenDir(). Must not be NULL.
 *
 * @return a entry, or NULL if there is no more entry in the directory. This
 * return value must not be freed.
 * @since GDAL 2.4
 *
 */

const VSIDIREntry *VSIGetNextDirEntry(VSIDIR* dir)
{
    return dir->NextDirEntry();
}

/************************************************************************/
/*                             VSICloseDir()                            */
/************************************************************************/

/**
 * \brief Close a directory
 *
 * This function is close to the POSIX closedir() function.
 *
 * @param dir Directory handled returned by VSIOpenDir().
 *
 * @since GDAL 2.4
 */

void VSICloseDir(VSIDIR* dir)
{
    delete dir;
}

/************************************************************************/
/*                              VSIMkdir()                              */
/************************************************************************/

/**
 * \brief Create a directory.
 *
 * Create a new directory with the indicated mode. For POSIX-style systems,
 * the mode is modified by the file creation mask (umask). However, some
 * file systems and platforms may not use umask, or they may ignore the mode
 * completely. So a reasonable cross-platform default mode value is 0755.
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
/*                       VSIMkdirRecursive()                            */
/************************************************************************/

/**
 * \brief Create a directory and all its ancestors
 *
 * @param pszPathname the path to the directory to create. UTF-8 encoded.
 * @param mode the permissions mode.
 *
 * @return 0 on success or -1 on an error.
 * @since GDAL 2.3
 */

int VSIMkdirRecursive( const char *pszPathname, long mode )
{
    if( pszPathname == nullptr || pszPathname[0] == '\0' ||
        strncmp("/", pszPathname, 2) == 0 )
    {
        return -1;
    }

    const CPLString osPathname(pszPathname);
    VSIStatBufL sStat;
    if( VSIStatL(osPathname, &sStat) == 0 &&
        VSI_ISDIR(sStat.st_mode) )
    {
        return 0;
    }
    const CPLString osParentPath(CPLGetPath(osPathname));

    // Prevent crazy paths from recursing forever.
    if( osParentPath == osPathname ||
        osParentPath.length() >= osPathname.length() )
    {
        return -1;
    }

    if( VSIStatL(osParentPath, &sStat) != 0 )
    {
        if( VSIMkdirRecursive(osParentPath, mode) != 0 )
            return -1;
    }

    return VSIMkdir(osPathname, mode);
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
/*                           VSIUnlinkBatch()                           */
/************************************************************************/

/**
 * \brief Delete several files, possibly in a batch.
 *
 * All files should belong to the same file system handler.
 *
 * @param papszFiles NULL terminated list of files. UTF-8 encoded.
 *
 * @return an array of size CSLCount(papszFiles), whose values are TRUE or FALSE
 * depending on the success of deletion of the corresponding file. The array
 * should be freed with VSIFree().
 * NULL might be return in case of a more general error (for example,
 * files belonging to different file system handlers)
 *
 * @since GDAL 3.1
 */

int *VSIUnlinkBatch( CSLConstList papszFiles )
{
    VSIFilesystemHandler *poFSHandler = nullptr;
    for( CSLConstList papszIter = papszFiles;
            papszIter && *papszIter; ++papszIter )
    {
        auto poFSHandlerThisFile = VSIFileManager::GetHandler( *papszIter );
        if( poFSHandler == nullptr )
            poFSHandler = poFSHandlerThisFile;
        else if( poFSHandler != poFSHandlerThisFile )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Files belong to different file system handlers");
            poFSHandler = nullptr;
            break;
        }
    }
    if( poFSHandler == nullptr )
        return nullptr;
    return poFSHandler->UnlinkBatch(papszFiles);
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
/*                             VSISync()                                */
/************************************************************************/

/**
 * \brief Synchronize a source file/directory with a target file/directory.
 *
 * This is a analog of the 'rsync' utility. In the current implementation,
 * rsync would be more efficient for local file copying, but VSISync() main
 * interest is when the source or target is a remote
 * file system like /vsis3/ or /vsigs/, in which case it can take into account
 * the timestamps of the files (or optionally the ETag/MD5Sum) to avoid
 * unneeded copy operations.
 *
 * This is only implemented efficiently for:
 * <ul>
 * <li> local filesystem <--> remote filesystem.</li>
 * <li> remote filesystem <--> remote filesystem (starting with GDAL 3.1).
 * Where the source and target remote filesystems are the same and one of
 * /vsis3/, /vsigs/ or /vsiaz/</li>
 * </ul>
 *
 * Similarly to rsync behavior, if the source filename ends with a slash,
 * it means that the content of the directory must be copied, but not the
 * directory name. For example, assuming "/home/even/foo" contains a file "bar",
 * VSISync("/home/even/foo/", "/mnt/media", ...) will create a "/mnt/media/bar"
 * file. Whereas VSISync("/home/even/foo", "/mnt/media", ...) will create a
 * "/mnt/media/foo" directory which contains a bar file.
 *
 * @param pszSource Source file or directory.  UTF-8 encoded.
 * @param pszTarget Target file or directory.  UTF-8 encoded.
 * @param papszOptions Null terminated list of options, or NULL.
 * Currently accepted options are:
 * <ul>
 * <li>RECURSIVE=NO (the default is YES)</li>
 * <li>SYNC_STRATEGY=TIMESTAMP/ETAG/OVERWRITE. Determines which criterion is used to
 *     determine if a target file must be replaced when it already exists and
 *     has the same file size as the source.
 *     Only applies for a source or target being a network filesystem.
 *
 *     The default is TIMESTAMP (similarly to how 'aws s3 sync' works), that is
 *     to say that for an upload operation, a remote file is
 *     replaced if it has a different size or if it is older than the source.
 *     For a download operation, a local file is  replaced if it has a different
 *     size or if it is newer than the remote file.
 *
 *     The ETAG strategy assumes that the ETag metadata of the remote file is
 *     the MD5Sum of the file content, which is only true in the case of /vsis3/
 *     for files not using KMS server side encryption and uploaded in a single
 *     PUT operation (so smaller than 50 MB given the default used by GDAL).
 *     Only to be used for /vsis3/, /vsigs/ or other filesystems using a
 *     MD5Sum as ETAG.
 *
 *     The OVERWRITE strategy (GDAL >= 3.2) will always overwrite the target file
 *     with the source one.
 * </li>
 * <li>NUM_THREADS=integer. (GDAL >= 3.1) Number of threads to use for parallel file copying.
 *     Only use for when /vsis3/, /vsigs/, /vsiaz/ or /vsiadls/ is in source or target.
 *     The default is 10 since GDAL 3.3</li>
 * <li>CHUNK_SIZE=integer. (GDAL >= 3.1) Maximum size of chunk (in bytes) to use to split
 *     large objects when downloading them from /vsis3/, /vsigs/, /vsiaz/ or /vsiadls/ to
 *     local file system, or for upload to /vsis3/, /vsiaz/ or /vsiadls/ from local file system.
 *     Only used if NUM_THREADS > 1.
 *     For upload to /vsis3/, this chunk size must be set at least to 5 MB.
 *     The default is 8 MB since GDAL 3.3</li>
 * </ul>
 * @param pProgressFunc Progress callback, or NULL.
 * @param pProgressData User data of progress callback, or NULL.
 * @param ppapszOutputs Unused. Should be set to NULL for now.
 *
 * @return TRUE on success or FALSE on an error.
 * @since GDAL 2.4
 */

int VSISync( const char* pszSource, const char* pszTarget,
              const char* const * papszOptions,
              GDALProgressFunc pProgressFunc,
              void *pProgressData,
              char*** ppapszOutputs  )

{
    if( pszSource[0] == '\0' || pszTarget[0] == '\0' )
    {
        return FALSE;
    }

    VSIFilesystemHandler *poFSHandlerSource =
        VSIFileManager::GetHandler( pszSource );
    VSIFilesystemHandler *poFSHandlerTarget =
        VSIFileManager::GetHandler( pszTarget );
    VSIFilesystemHandler *poFSHandlerLocal =
        VSIFileManager::GetHandler( "" );
    VSIFilesystemHandler *poFSHandlerMem =
        VSIFileManager::GetHandler( "/vsimem/" );
    VSIFilesystemHandler* poFSHandler = poFSHandlerSource;
    if( poFSHandlerTarget != poFSHandlerLocal &&
        poFSHandlerTarget != poFSHandlerMem )
    {
        poFSHandler = poFSHandlerTarget;
    }

    return poFSHandler->Sync( pszSource, pszTarget, papszOptions,
                               pProgressFunc, pProgressData, ppapszOutputs ) ?
                TRUE : FALSE;
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
/*                         VSIRmdirRecursive()                          */
/************************************************************************/

/**
 * \brief Delete a directory recursively
 *
 * Deletes a directory object and its content from the file system.
 *
 * Starting with GDAL 3.1, /vsis3/ has an efficient implementation of this
 * function.
 *
 * @return 0 on success or -1 on an error.
 * @since GDAL 2.3
 */

int VSIRmdirRecursive( const char* pszDirname )
{
    if( pszDirname == nullptr || pszDirname[0] == '\0' ||
        strncmp("/", pszDirname, 2) == 0 )
    {
        return -1;
    }
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszDirname );
    return poFSHandler->RmdirRecursive( pszDirname );
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
    if( pszFilename[0] != '\0' && pszFilename[1] == ':' &&
        pszFilename[2] == '\0' )
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
/*                       VSIGetFileMetadata()                           */
/************************************************************************/

/**
 * \brief Get metadata on files.
 *
 * Implemented currently only for network-like filesystems.
 *
 * @param pszFilename the path of the filesystem object to be queried.
 * UTF-8 encoded.
 * @param pszDomain Metadata domain to query. Depends on the file system.
 * The following are supported:
 * <ul>
 * <li>HEADERS: to get HTTP headers for network-like filesystems (/vsicurl/, /vsis3/, etc)</li>
 * <li>TAGS:
 *    <ul>
 *      <li>/vsis3/: to get S3 Object tagging information</li>
 *      <li>/vsiaz/: to get blob tags. Refer to https://docs.microsoft.com/en-us/rest/api/storageservices/get-blob-tags</li>
 *    </ul>
 * </li>
 * <li>STATUS: specific to /vsiadls/: returns all system defined properties for a path (seems in practice to be a subset of HEADERS)</li>
 * <li>ACL: specific to /vsiadls/: returns the access control list for a path.</li>
 * <li>METADATA: specific to /vsiaz/: to set blob metadata. Refer to https://docs.microsoft.com/en-us/rest/api/storageservices/get-blob-metadata. Note: this will be a subset of what pszDomain=HEADERS returns</li>
 * </ul>
 * @param papszOptions Unused. Should be set to NULL.
 *
 * @return a NULL-terminated list of key=value strings, to be freed with CSLDestroy()
 * or NULL in case of error / empty list.
 *
 * @since GDAL 3.1.0
 */

char** VSIGetFileMetadata( const char * pszFilename, const char* pszDomain,
                           CSLConstList papszOptions )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );
    return poFSHandler->GetFileMetadata( pszFilename, pszDomain, papszOptions );
}

/************************************************************************/
/*                       VSISetFileMetadata()                           */
/************************************************************************/

/**
 * \brief Set metadata on files.
 *
 * Implemented currently only for /vsis3/, /vsiaz/ and /vsiadls/
 *
 * @param pszFilename the path of the filesystem object to be set.
 * UTF-8 encoded.
 * @param papszMetadata NULL-terminated list of key=value strings.
 * @param pszDomain Metadata domain to set. Depends on the file system.
 * The following are supported:
 * <ul>
 * <li>HEADERS: specific to /vsis3/: to set HTTP headers</li>
 * <li>TAGS: Content of papszMetadata should be KEY=VALUE pairs.
 *    <ul>
 *      <li>/vsis3/: to set S3 Object tagging information</li>
 *      <li>/vsiaz/: to set blob tags. Refer to https://docs.microsoft.com/en-us/rest/api/storageservices/set-blob-tags. Note: storageV2 must be enabled on the account</li>
 *    </ul>
 * </li>
 * <li>PROPERTIES:
 *    <ul>
 *      <li>to /vsiaz/: to set properties. Refer to https://docs.microsoft.com/en-us/rest/api/storageservices/set-blob-properties.</li>
 *      <li>to /vsiadls/: to set properties. Refer to https://docs.microsoft.com/en-us/rest/api/storageservices/datalakestoragegen2/path/update for headers valid for action=setProperties.</li>
 *    </ul>
 * </li>
 * <li>ACL: specific to /vsiadls/: to set access control list. Refer to https://docs.microsoft.com/en-us/rest/api/storageservices/datalakestoragegen2/path/update for headers valid for action=setAccessControl or setAccessControlRecursive. In setAccessControlRecursive, x-ms-acl must be specified in papszMetadata</li>
 * <li>METADATA: specific to /vsiaz/: to set blob metadata. Refer to https://docs.microsoft.com/en-us/rest/api/storageservices/set-blob-metadata. Content of papszMetadata should be strings in the form x-ms-meta-name=value</li>
 * </ul>
 * @param papszOptions NULL or NULL terminated list of options.
 *                     For /vsiadls/ and pszDomain=ACL, "RECURSIVE=TRUE" can be
 *                     set to set the access control list recursively. When
 *                     RECURSIVE=TRUE is set, MODE should also be set to one of
 *                     "set", "modify" or "remove".
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 3.1.0
 */

int VSISetFileMetadata( const char * pszFilename,
                           CSLConstList papszMetadata,
                           const char* pszDomain,
                           CSLConstList papszOptions )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );
    return poFSHandler->SetFileMetadata( pszFilename, papszMetadata, pszDomain,
                                         papszOptions ) ? 1 : 0;
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
/*                     VSIHasOptimizedReadMultiRange()                  */
/************************************************************************/

/**
 * \brief Returns if the filesystem supports efficient multi-range reading.
 *
 * Currently only returns TRUE for /vsicurl/ and derived file systems.
 *
 * @param pszPath the path of the filesystem object to be tested.
 * UTF-8 encoded.
 *
 * @return TRUE if the file system is known to have an efficient multi-range
 * reading.
 *
 * @since GDAL 2.3
 */

int VSIHasOptimizedReadMultiRange( const char* pszPath )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszPath );

    return poFSHandler->HasOptimizedReadMultiRange( pszPath );
}

/************************************************************************/
/*                        VSIGetActualURL()                             */
/************************************************************************/

/**
 * \brief Returns the actual URL of a supplied filename.
 *
 * Currently only returns a non-NULL value for network-based virtual file systems.
 * For example "/vsis3/bucket/filename" will be expanded as
 * "https://bucket.s3.amazon.com/filename"
 * 
 * Note that the lifetime of the returned string, is short, and may be
 * invalidated by any following GDAL functions.
 *
 * @param pszFilename the path of the filesystem object. UTF-8 encoded.
 *
 * @return the actual URL corresponding to the supplied filename, or NULL. Should not
 * be freed.
 *
 * @since GDAL 2.3
 */

const char* VSIGetActualURL( const char* pszFilename )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->GetActualURL( pszFilename );
}

/************************************************************************/
/*                        VSIGetSignedURL()                             */
/************************************************************************/

/**
 * \brief Returns a signed URL of a supplied filename.
 *
 * Currently only returns a non-NULL value for /vsis3/, /vsigs/, /vsiaz/ and /vsioss/
 * For example "/vsis3/bucket/filename" will be expanded as
 * "https://bucket.s3.amazon.com/filename?X-Amz-Algorithm=AWS4-HMAC-SHA256..."
 * Configuration options that apply for file opening (typically to provide
 * credentials), and are returned by VSIGetFileSystemOptions(), are also valid
 * in that context.
 *
 * @param pszFilename the path of the filesystem object. UTF-8 encoded.
 * @param papszOptions list of options, or NULL. Depend on file system handler.
 * For /vsis3/, /vsigs/, /vsiaz/ and /vsioss/, the following options are supported:
 * <ul>
 * <li>START_DATE=YYMMDDTHHMMSSZ: date and time in UTC following ISO 8601
 *     standard, corresponding to the start of validity of the URL.
 *     If not specified, current date time.</li>
 * <li>EXPIRATION_DELAY=number_of_seconds: number between 1 and 604800 (seven days)
 * for the validity of the signed URL. Defaults to 3600 (one hour)</li>
 * <li>VERB=GET/HEAD/DELETE/PUT/POST: HTTP VERB for which the request will be
 * used. Default to GET.</li>
 * </ul>
 *
 * /vsiaz/ supports additional options:
 * <ul>
 * <li>SIGNEDIDENTIFIER=value: to relate the given shared access signature
 * to a corresponding stored access policy.</li>
 * <li>SIGNEDPERMISSIONS=r|w: permissions associated with the shared access
 * signature. Normally deduced from VERB.</li>
 * </ul>
 *
 * @return a signed URL, or NULL. Should be freed with CPLFree().
 * @since GDAL 2.3
 */

char* VSIGetSignedURL( const char* pszFilename, CSLConstList papszOptions )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->GetSignedURL( pszFilename, papszOptions );
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
    return Open(pszFilename, pszAccess, false, nullptr);
}

/************************************************************************/
/*                               Sync()                                 */
/************************************************************************/

bool VSIFilesystemHandler::Sync( const char* pszSource, const char* pszTarget,
                            const char* const * papszOptions,
                            GDALProgressFunc pProgressFunc,
                            void *pProgressData,
                            char*** ppapszOutputs  )
{
    if( ppapszOutputs )
    {
        *ppapszOutputs = nullptr;
    }

    VSIStatBufL sSource;
    CPLString osSource(pszSource);
    CPLString osSourceWithoutSlash(pszSource);
    if( osSourceWithoutSlash.back() == '/' )
    {
        osSourceWithoutSlash.resize(osSourceWithoutSlash.size()-1);
    }
    if( VSIStatL(osSourceWithoutSlash, &sSource) < 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "%s does not exist", pszSource);
        return false;
    }

    if( VSI_ISDIR(sSource.st_mode) )
    {
        CPLString osTargetDir(pszTarget);
        if( osSource.back() != '/' )
        {
            osTargetDir = CPLFormFilename(osTargetDir,
                                          CPLGetFilename(pszSource), nullptr);
        }

        VSIStatBufL sTarget;
        bool ret = true;
        if( VSIStatL(osTargetDir, &sTarget) < 0 )
        {
            if( VSIMkdirRecursive(osTargetDir, 0755) < 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot create directory %s", osTargetDir.c_str());
                return false;
            }
        }

        if( !CPLFetchBool(papszOptions, "STOP_ON_DIR", false) )
        {
            CPLStringList aosChildOptions(CSLDuplicate(papszOptions));
            if( !CPLFetchBool(papszOptions, "RECURSIVE", true) )
            {
                aosChildOptions.SetNameValue("RECURSIVE", nullptr);
                aosChildOptions.AddString("STOP_ON_DIR=TRUE");
            }

            char** papszSrcFiles = VSIReadDir(osSourceWithoutSlash);
            int nFileCount = 0;
            for( auto iter = papszSrcFiles ; iter && *iter; ++iter )
            {
                if( strcmp(*iter, ".") != 0 && strcmp(*iter, "..") != 0 )
                {
                    nFileCount ++;
                }
            }
            int iFile = 0;
            for( auto iter = papszSrcFiles ; iter && *iter; ++iter, ++iFile )
            {
                if( strcmp(*iter, ".") == 0 || strcmp(*iter, "..") == 0 )
                {
                    continue;
                }
                CPLString osSubSource(
                    CPLFormFilename(osSourceWithoutSlash, *iter, nullptr) );
                CPLString osSubTarget(
                    CPLFormFilename(osTargetDir, *iter, nullptr) );
                // coverity[divide_by_zero]
                void* pScaledProgress = GDALCreateScaledProgress(
                    double(iFile) / nFileCount, double(iFile + 1) / nFileCount,
                    pProgressFunc, pProgressData);
                ret = Sync( (osSubSource + '/').c_str(), osSubTarget,
                            aosChildOptions.List(),
                            GDALScaledProgress, pScaledProgress,
                            nullptr );
                GDALDestroyScaledProgress(pScaledProgress);
                if( !ret )
                {
                    break;
                }
            }
            CSLDestroy(papszSrcFiles);
        }
        return ret;
    }

    VSIStatBufL sTarget;
    CPLString osTarget(pszTarget);
    if( VSIStatL(osTarget, &sTarget) == 0 )
    {
        bool bTargetIsFile = true;
        if ( VSI_ISDIR(sTarget.st_mode) )
        {
            osTarget = CPLFormFilename(osTarget,
                                       CPLGetFilename(pszSource), nullptr);
            bTargetIsFile = VSIStatL(osTarget, &sTarget) == 0 && 
                            !CPL_TO_BOOL(VSI_ISDIR(sTarget.st_mode));
        }
        if( bTargetIsFile )
        {
            if( sSource.st_size == sTarget.st_size &&
                sSource.st_mtime == sTarget.st_mtime &&
                sSource.st_mtime != 0 )
            {
                CPLDebug("VSI", "%s and %s have same size and modification "
                         "date. Skipping copying",
                         osSourceWithoutSlash.c_str(),
                         osTarget.c_str());
                return true;
            }
        }
    }

    VSILFILE* fpIn = VSIFOpenExL(osSourceWithoutSlash, "rb", TRUE);
    if( fpIn == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                 osSourceWithoutSlash.c_str());
        return false;
    }

    VSILFILE* fpOut = VSIFOpenExL(osTarget.c_str(), "wb", TRUE);
    if( fpOut == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", osTarget.c_str());
        VSIFCloseL(fpIn);
        return false;
    }

    bool ret = true;
    constexpr size_t nBufferSize = 10 * 4096;
    std::vector<GByte> abyBuffer(nBufferSize, 0);
    GUIntBig nOffset = 0;
    CPLString osMsg;
    osMsg.Printf("Copying of %s", osSourceWithoutSlash.c_str());
    while( true )
    {
        size_t nRead = VSIFReadL(&abyBuffer[0], 1, nBufferSize, fpIn);
        size_t nWritten = VSIFWriteL(&abyBuffer[0], 1, nRead, fpOut);
        if( nWritten != nRead )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Copying of %s to %s failed",
                     osSourceWithoutSlash.c_str(), osTarget.c_str());
            ret = false;
            break;
        }
        nOffset += nRead;
        if( pProgressFunc && !pProgressFunc(
                double(nOffset) / sSource.st_size, osMsg.c_str(),
                pProgressData) )
        {
            ret = false;
            break;
        }
        if( nRead < nBufferSize )
        {
            break;
        }
    }

    VSIFCloseL(fpIn);
    if( VSIFCloseL(fpOut) != 0 )
    {
        ret = false;
    }
    return ret;
}

/************************************************************************/
/*                            VSIDIREntry()                             */
/************************************************************************/

VSIDIREntry::VSIDIREntry(): pszName(nullptr), nMode(0), nSize(0), nMTime(0),
                   bModeKnown(false), bSizeKnown(false), bMTimeKnown(false),
                   papszExtra(nullptr)
{
}

/************************************************************************/
/*                            VSIDIREntry()                             */
/************************************************************************/

VSIDIREntry::VSIDIREntry(const VSIDIREntry& other):
    pszName(VSIStrdup(other.pszName)),
    nMode(other.nMode),
    nSize(other.nSize),
    nMTime(other.nMTime),
    bModeKnown(other.bModeKnown),
    bSizeKnown(other.bSizeKnown),
    bMTimeKnown(other.bMTimeKnown),
    papszExtra(CSLDuplicate(other.papszExtra))
{}

/************************************************************************/
/*                           ~VSIDIREntry()                             */
/************************************************************************/

VSIDIREntry::~VSIDIREntry()
{
    CPLFree(pszName);
    CSLDestroy(papszExtra);
}

/************************************************************************/
/*                              ~VSIDIR()                               */
/************************************************************************/

VSIDIR::~VSIDIR()
{
}

/************************************************************************/
/*                            VSIDIRGeneric                             */
/************************************************************************/

namespace {
struct VSIDIRGeneric: public VSIDIR
{
    CPLString osRootPath{};
    CPLString osBasePath{};
    char** papszContent = nullptr;
    int nRecurseDepth = 0;
    int nPos = 0;
    VSIDIREntry entry{};
    std::vector<VSIDIRGeneric*> aoStackSubDir{};
    VSIFilesystemHandler* poFS = nullptr;

    explicit VSIDIRGeneric(VSIFilesystemHandler* poFSIn): poFS(poFSIn) {}
    ~VSIDIRGeneric();

    const VSIDIREntry* NextDirEntry() override;

    VSIDIRGeneric(const VSIDIRGeneric&) = delete;
    VSIDIRGeneric& operator=(const VSIDIRGeneric&) = delete;
};

/************************************************************************/
/*                         ~VSIDIRGeneric()                             */
/************************************************************************/

VSIDIRGeneric::~VSIDIRGeneric()
{
    while( !aoStackSubDir.empty() )
    {
        delete aoStackSubDir.back();
        aoStackSubDir.pop_back();
    }
    CSLDestroy(papszContent);
}

}

/************************************************************************/
/*                            OpenDir()                                 */
/************************************************************************/

VSIDIR* VSIFilesystemHandler::OpenDir( const char *pszPath,
                                       int nRecurseDepth,
                                       const char* const *)
{
    char** papszContent = VSIReadDir(pszPath);
    VSIStatBufL sStatL;
    if( papszContent == nullptr &&
        (VSIStatL(pszPath, &sStatL) != 0 || !VSI_ISDIR(sStatL.st_mode)) )
    {
        return nullptr;
    }
    VSIDIRGeneric* dir = new VSIDIRGeneric(this);
    dir->osRootPath = pszPath;
    dir->nRecurseDepth = nRecurseDepth;
    dir->papszContent = papszContent;
    return dir;
}

/************************************************************************/
/*                           NextDirEntry()                             */
/************************************************************************/

const VSIDIREntry* VSIDIRGeneric::NextDirEntry()
{
    if( VSI_ISDIR(entry.nMode) && nRecurseDepth != 0 )
    {
        CPLString osCurFile(osRootPath);
        if( !osCurFile.empty() )
            osCurFile += '/';
        osCurFile += entry.pszName;
        auto subdir = static_cast<VSIDIRGeneric*>(
            poFS->VSIFilesystemHandler::OpenDir(osCurFile,
                nRecurseDepth - 1, nullptr));
        if( subdir )
        {
            subdir->osRootPath = osRootPath;
            subdir->osBasePath = entry.pszName;
            aoStackSubDir.push_back(subdir);
        }
        entry.nMode = 0;
    }

    while( !aoStackSubDir.empty() )
    {
        auto l_entry = aoStackSubDir.back()->NextDirEntry();
        if( l_entry )
        {
            return l_entry;
        }
        delete aoStackSubDir.back();
        aoStackSubDir.pop_back();
    }

    if( papszContent == nullptr )
    {
        return nullptr;
    }

    while( true )
    {
        if( !papszContent[nPos] )
        {
            return nullptr;
        }
        // Skip . and ..entries
        if( papszContent[nPos][0] == '.' &&
            (papszContent[nPos][1] == '\0' ||
             (papszContent[nPos][1] == '.' &&
              papszContent[nPos][2] == '\0')) )
        {
            nPos ++;
        }
        else
        {
            break;
        }
    }

    CPLFree(entry.pszName);
    CPLString osName(osBasePath);
    if( !osName.empty() )
        osName += '/';
    osName += papszContent[nPos];
    entry.pszName = CPLStrdup(osName);
    VSIStatBufL sStatL;
    CPLString osCurFile(osRootPath);
    if( !osCurFile.empty() )
        osCurFile += '/';
    osCurFile += entry.pszName;
    if( VSIStatL(osCurFile, &sStatL) == 0 )
    {
        entry.nMode = sStatL.st_mode;
        entry.nSize = sStatL.st_size;
        entry.nMTime = sStatL.st_mtime;
        entry.bModeKnown = true;
        entry.bSizeKnown = true;
        entry.bMTimeKnown = true;
    }
    else
    {
        entry.nMode = 0;
        entry.nSize = 0;
        entry.nMTime = 0;
        entry.bModeKnown = false;
        entry.bSizeKnown = false;
        entry.bMTimeKnown = false;
    }
    nPos ++;

    return &(entry);
}

/************************************************************************/
/*                           UnlinkBatch()                              */
/************************************************************************/

int* VSIFilesystemHandler::UnlinkBatch( CSLConstList papszFiles )
{
    int* panRet = static_cast<int*>(
        CPLMalloc(sizeof(int) * CSLCount(papszFiles)));
    for( int i = 0; papszFiles && papszFiles[i]; ++i )
    {
        panRet[i] = VSIUnlink(papszFiles[i]) == 0;
    }
    return panRet;
}

/************************************************************************/
/*                          RmdirRecursive()                            */
/************************************************************************/

int VSIFilesystemHandler::RmdirRecursive( const char* pszDirname )
{
    CPLString osDirnameWithoutEndSlash(pszDirname);
    if( !osDirnameWithoutEndSlash.empty() && osDirnameWithoutEndSlash.back() == '/' )
        osDirnameWithoutEndSlash.resize( osDirnameWithoutEndSlash.size() - 1 );

    CPLStringList aosOptions;
    auto poDir = std::unique_ptr<VSIDIR>(OpenDir(pszDirname, -1, aosOptions.List()));
    if( !poDir )
        return -1;
    std::vector<std::string> aosDirs;
    while( true )
    {
        auto entry = poDir->NextDirEntry();
        if( !entry )
            break;

        const CPLString osFilename(osDirnameWithoutEndSlash + '/' + entry->pszName);
        if( (entry->nMode & S_IFDIR) )
        {
            aosDirs.push_back(osFilename);
        }
        else
        {
            if( VSIUnlink(osFilename) != 0 )
                return -1;
        }
    }

    // Sort in reverse order, so that inner-most directories are deleted first
    std::sort(aosDirs.begin(), aosDirs.end(),
              [](const std::string& a, const std::string& b) {return a > b; });

    for(const auto& osDir: aosDirs )
    {
        if( VSIRmdir(osDir.c_str()) != 0 )
            return -1;
    }

    return VSIRmdir(pszDirname);
}

/************************************************************************/
/*                          GetFileMetadata()                           */
/************************************************************************/

char** VSIFilesystemHandler::GetFileMetadata( const char * /* pszFilename*/, const char* /*pszDomain*/,
                                    CSLConstList /*papszOptions*/ )
{
    return nullptr;
}

/************************************************************************/
/*                          SetFileMetadata()                           */
/************************************************************************/

bool VSIFilesystemHandler::SetFileMetadata( const char * /* pszFilename*/,
                                    CSLConstList /* papszMetadata */,
                                    const char* /* pszDomain */,
                                    CSLConstList /* papszOptions */ )
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetFileMetadata() not supported");
    return false;
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
    return VSIFOpenEx2L(pszFilename, pszAccess, bSetError, nullptr);
}

/************************************************************************/
/*                            VSIFOpenEx2L()                            */
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
 * @param papszOptions NULL or NULL-terminated list of strings. The content is
 *                     highly file system dependent. Currently only MIME headers
 *                     such as Content-Type and Content-Encoding are supported
 *                     for the /vsis3/, /vsigs/, /vsiaz/, /vsiadls/ file systems.
 *
 * @return NULL on failure, or the file handle.
 *
 * @since GDAL 3.3
 */

VSILFILE *VSIFOpenEx2L( const char * pszFilename, const char * pszAccess,
                        int bSetError, CSLConstList papszOptions )

{
    // Too long filenames can cause excessive memory allocation due to
    // recursion in some filesystem handlers
    constexpr size_t knMaxPath = 8192;
    if( CPLStrnlen(pszFilename, knMaxPath) == knMaxPath )
        return nullptr;

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );

    VSILFILE* fp = reinterpret_cast<VSILFILE *>(
        poFSHandler->Open( pszFilename, pszAccess, CPL_TO_BOOL(bSetError), papszOptions ) );

    VSIDebug4( "VSIFOpenEx2L(%s,%s,%d) = %p",
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
 * extent is filled with zeroes! It must be interpreted as "may
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
 * extent is filled with zeroes! It must be interpreted as "may
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
    if( fp == nullptr && pszFilename == nullptr )
        return FALSE;
    if( ppabyRet == nullptr )
        return FALSE;

    *ppabyRet = nullptr;
    if( pnSize != nullptr )
        *pnSize = 0;

    bool bFreeFP = false;
    if( nullptr == fp )
    {
        fp = VSIFOpenL( pszFilename, "rb" );
        if( nullptr == fp )
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

    if( pszFilename == nullptr ||
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
                    *ppabyRet = nullptr;
                    if( bFreeFP )
                        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
                    return FALSE;
                }
                GByte* pabyNew = static_cast<GByte *>(
                    VSIRealloc(*ppabyRet, static_cast<size_t>(nDataAlloc)) );
                if( pabyNew == nullptr )
                {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "Cannot allocate " CPL_FRMT_GIB " bytes",
                              nDataAlloc );
                    VSIFree( *ppabyRet );
                    *ppabyRet = nullptr;
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
                *ppabyRet = nullptr;
                if( pnSize != nullptr )
                    *pnSize = 0;
                if( bFreeFP )
                    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
                return FALSE;
            }

            if( pnSize != nullptr )
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
            // opening a directory returns nDataLen = INT_MAX (on 32bit) or INT64_MAX (on 64bit)
            || nDataLen + 1 > std::numeric_limits<size_t>::max() / 2
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
        if( nullptr == *ppabyRet )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Cannot allocate " CPL_FRMT_GIB " bytes",
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
            *ppabyRet = nullptr;
            if( bFreeFP )
                CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
            return FALSE;
        }
        if( pnSize != nullptr )
            *pnSize = nDataLen;
    }
    if( bFreeFP )
        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
    return TRUE;
}


/************************************************************************/
/*                         VSIOverwriteFile()                           */
/************************************************************************/

/**
 * \brief Overwrite an existing file with content from another one
 *
 * @param fpTarget file handle opened with VSIFOpenL() with "rb+" flag.
 * @param pszSourceFilename source filename
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 3.1
 */

int VSIOverwriteFile( VSILFILE* fpTarget, const char* pszSourceFilename )
{
    VSILFILE* fpSource = VSIFOpenL(pszSourceFilename, "rb");
    if( fpSource == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot open %s", pszSourceFilename);
        return false;
    }

    const size_t nBufferSize = 4096;
    void* pBuffer = CPLMalloc(nBufferSize);
    VSIFSeekL( fpTarget, 0, SEEK_SET );
    bool bRet = true;
    while( true )
    {
        size_t nRead = VSIFReadL( pBuffer, 1, nBufferSize, fpSource );
        size_t nWritten = VSIFWriteL( pBuffer, 1, nRead, fpTarget );
        if( nWritten != nRead )
        {
            bRet = false;
            break;
        }
        if( nRead < nBufferSize )
            break;
    }

    if( bRet )
    {
        bRet = VSIFTruncateL( fpTarget, VSIFTellL(fpTarget) ) == 0;
        if( !bRet )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Truncation failed");
        }
    }

    CPLFree(pBuffer);
    VSIFCloseL(fpSource);
    return bRet;
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
/*                    VSIGetFileSystemsPrefixes()                       */
/************************************************************************/

/**
 * \brief Return the list of prefixes for virtual file system handlers
 * currently registered.
 *
 * Typically: "", "/vsimem/", "/vsicurl/", etc
 *
 * @return a NULL terminated list of prefixes. Must be freed with CSLDestroy()
 * @since GDAL 2.3
 */

char **VSIGetFileSystemsPrefixes( void )
{
    return VSIFileManager::GetPrefixes();
}

/************************************************************************/
/*                     VSIGetFileSystemOptions()                        */
/************************************************************************/

/**
 * \brief Return the list of options associated with a virtual file system handler
 * as a serialized XML string.
 *
 * Those options may be set as configuration options with CPLSetConfigOption().
 *
 * @param pszFilename a filename, or prefix of a virtual file system handler.
 * @return a string, which must not be freed, or NULL if no options is declared.
 * @since GDAL 2.3
 */

const char* VSIGetFileSystemOptions( const char* pszFilename )
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->GetOptions();
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
** file, etc).  It should be thread safe.
**/

/************************************************************************/
/*                           VSIFileManager()                           */
/************************************************************************/

VSIFileManager::VSIFileManager() :
    poDefaultHandler(nullptr)
{}

/************************************************************************/
/*                          ~VSIFileManager()                           */
/************************************************************************/

VSIFileManager::~VSIFileManager()
{
    std::set<VSIFilesystemHandler*> oSetAlreadyDeleted;
    for( std::map<std::string, VSIFilesystemHandler*>::const_iterator iter =
             oHandlers.begin();
         iter != oHandlers.end();
         ++iter )
    {
        if( oSetAlreadyDeleted.find(iter->second) == oSetAlreadyDeleted.end() )
        {
            oSetAlreadyDeleted.insert(iter->second);
            delete iter->second;
        }
    }

    delete poDefaultHandler;
}

/************************************************************************/
/*                                Get()                                 */
/************************************************************************/

static VSIFileManager *poManager = nullptr;
static CPLMutex* hVSIFileManagerMutex = nullptr;

VSIFileManager *VSIFileManager::Get()
{
      CPLMutexHolder oHolder(&hVSIFileManagerMutex);
      if ( poManager != nullptr ) {
        return poManager;
      }

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
      VSIInstallAzureFileHandler();
      VSIInstallAzureStreamingFileHandler();
      VSIInstallADLSFileHandler();
      VSIInstallOSSFileHandler();
      VSIInstallOSSStreamingFileHandler();
      VSIInstallSwiftFileHandler();
      VSIInstallSwiftStreamingFileHandler();
      VSIInstallWebHdfsHandler();
#endif
      VSIInstallStdinHandler();
      VSIInstallHdfsHandler();
      VSIInstallStdoutHandler();
      VSIInstallSparseFileHandler();
      VSIInstallTarFileHandler();
      VSIInstallCryptFileHandler();

      return poManager;

}

/************************************************************************/
/*                           GetPrefixes()                              */
/************************************************************************/

char ** VSIFileManager::GetPrefixes()
{
    CPLMutexHolder oHolder( &hVSIFileManagerMutex );
    CPLStringList aosList;
    for( const auto& oIter: Get()->oHandlers )
    {
        if( oIter.first != "/vsicurl?" )
        {
            aosList.AddString( oIter.first.c_str() );
        }
    }
    return aosList.StealList();
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
        poManager = nullptr;
    }

    if( hVSIFileManagerMutex != nullptr )
    {
        CPLDestroyMutex(hVSIFileManagerMutex);
        hVSIFileManagerMutex = nullptr;
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
            constexpr vsi_l_offset nMaxOffset = 4096;
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
