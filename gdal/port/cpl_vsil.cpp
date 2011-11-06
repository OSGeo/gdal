/******************************************************************************
 * $Id$
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation VSI*L File API and other file system access
 *           methods going through file virtualization.
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
#include <string>

CPL_CVSID("$Id$");

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

char **VSIReadDir(const char *pszPath)
{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszPath );

    return poFSHandler->ReadDir( pszPath );
}

/************************************************************************/
/*                             CPLReadDir()                             */
/*                                                                      */
/*      This is present only to provide ABI compatability with older    */
/*      versions.                                                       */
/************************************************************************/
#undef CPLReadDir

CPL_C_START
char CPL_DLL **CPLReadDir( const char *pszPath );
CPL_C_END

char **CPLReadDir( const char *pszPath )
{
    return VSIReadDir(pszPath);
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
/*************************a***********************************************/

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
 * portability only the st_size (size in bytes), and st_mode (file type). 
 * This method is similar to VSIStat(), but will work on large files on 
 * systems where this requires special calls. 
 * 
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX stat() function.
 *
 * @param pszFilename the path of the filesystem object to be queried.  UTF-8 encoded.
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
 * portability only the st_size (size in bytes), and st_mode (file type).
 * This method is similar to VSIStat(), but will work on large files on
 * systems where this requires special calls.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX stat() function, with an extra parameter to specify
 * which information is needed, which offers a potential for speed optimizations
 * on specialized and potentially slow virtual filesystem objects (/vsigzip/, /vsicurl/)
 *
 * @param pszFilename the path of the filesystem object to be queried.  UTF-8 encoded.
 * @param psStatBuf the structure to load with information.
 * @param nFlags 0 to get all information, or VSI_STAT_EXISTS_FLAG, VSI_STAT_NATURE_FLAG or
 *                  VSI_STAT_SIZE_FLAG, or a combination of those to get partial info.
 *
 * @return 0 on success or -1 on an error.
 *
 * @since GDAL 1.8.0
 */

int VSIStatExL( const char * pszFilename, VSIStatBufL *psStatBuf, int nFlags )

{
    char    szAltPath[4];
    /* enable to work on "C:" as if it were "C:\" */
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

    if (nFlags == 0)
        nFlags = VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG | VSI_STAT_SIZE_FLAG;

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
 * This methods avoid ugly #ifndef WIN32 / #endif code, that is wrong when
 * dealing with virtual filenames.
 *
 * @param pszFilename the path of the filesystem object to be tested.  UTF-8 encoded.
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
 * encoding instead of UTF-8, retoring the pre-1.8.0 behavior of VSIFOpenL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fopen() function.
 *
 * @param pszFilename the file to open.  UTF-8 encoded.
 * @param pszAccess access requested (ie. "r", "r+", "w".  
 *
 * @return NULL on failure, or the file handle.
 */

VSILFILE *VSIFOpenL( const char * pszFilename, const char * pszAccess )

{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszFilename );
        
    VSILFILE* fp = (VSILFILE *) poFSHandler->Open( pszFilename, pszAccess );

    VSIDebug3( "VSIFOpenL(%s,%s) = %p", pszFilename, pszAccess, fp );
        
    return fp;
}

/************************************************************************/
/*                             VSIFCloseL()                             */
/************************************************************************/

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
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return 0 on success or -1 on failure.
 */

int VSIFCloseL( VSILFILE * fp )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    VSIDebug1( "VSICloseL(%p)", fp );
    
    int nResult = poFileHandle->Close();
    
    delete poFileHandle;

    return nResult;
}

/************************************************************************/
/*                             VSIFSeekL()                              */
/************************************************************************/

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
 * @param fp file handle opened with VSIFOpenL(). 
 * @param nOffset offset in bytes.
 * @param nWhence one of SEEK_SET, SEEK_CUR or SEEK_END.
 *
 * @return 0 on success or -1 one failure.
 */

int VSIFSeekL( VSILFILE * fp, vsi_l_offset nOffset, int nWhence )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Seek( nOffset, nWhence );
}

/************************************************************************/
/*                             VSIFTellL()                              */
/************************************************************************/

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
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Tell();
}

/************************************************************************/
/*                             VSIRewindL()                             */
/************************************************************************/

void VSIRewindL( VSILFILE * fp )

{
    VSIFSeekL( fp, 0, SEEK_SET );
}

/************************************************************************/
/*                             VSIFFlushL()                             */
/************************************************************************/

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
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Flush();
}

/************************************************************************/
/*                             VSIFReadL()                              */
/************************************************************************/

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
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Read( pBuffer, nSize, nCount );
}

/************************************************************************/
/*                             VSIFWriteL()                             */
/************************************************************************/

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

size_t VSIFWriteL( const void *pBuffer, size_t nSize, size_t nCount, VSILFILE *fp )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Write( pBuffer, nSize, nCount );
}

/************************************************************************/
/*                              VSIFEofL()                              */
/************************************************************************/

/**
 * \brief Test for end of file.
 *
 * Returns TRUE (non-zero) if the file read/write offset is currently at the
 * end of the file. 
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
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Eof();
}

/************************************************************************/
/*                            VSIFTruncateL()                           */
/************************************************************************/

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
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;

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
 * @param pszFormat the printf style format string. 
 * 
 * @return the number of bytes written or -1 on an error.
 */

int VSIFPrintfL( VSILFILE *fp, const char *pszFormat, ... )

{
    va_list args;
    CPLString osResult;

    va_start( args, pszFormat );
    osResult.vPrintf( pszFormat, args );
    va_end( args );

    return VSIFWriteL( osResult.c_str(), 1, osResult.length(), fp );
}

/************************************************************************/
/*                              VSIFPutcL()                              */
/************************************************************************/

int VSIFPutcL( int nChar, VSILFILE * fp )

{
    unsigned char cChar = (unsigned char)nChar;
    return VSIFWriteL(&cChar, 1, 1, fp);
}

/************************************************************************/
/* ==================================================================== */
/*                           VSIFileManager()                           */
/* ==================================================================== */
/************************************************************************/

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

VSIFileManager::VSIFileManager()

{
    poDefaultHandler = NULL;
}

/************************************************************************/
/*                          ~VSIFileManager()                           */
/************************************************************************/

VSIFileManager::~VSIFileManager()
{
    std::map<std::string,VSIFilesystemHandler*>::const_iterator iter;

    for( iter = oHandlers.begin();
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

VSIFileManager *VSIFileManager::Get()

{
    
    if( poManager == NULL )
    {
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
#endif
        VSIInstallStdinHandler();
        VSIInstallStdoutHandler();
        VSIInstallSparseFileHandler();
        VSIInstallTarFileHandler();
    }
    
    return poManager;
}

/************************************************************************/
/*                             GetHandler()                             */
/************************************************************************/

VSIFilesystemHandler *VSIFileManager::GetHandler( const char *pszPath )

{
    VSIFileManager *poThis = Get();
    std::map<std::string,VSIFilesystemHandler*>::const_iterator iter;
    int nPathLen = strlen(pszPath);

    for( iter = poThis->oHandlers.begin();
         iter != poThis->oHandlers.end();
         ++iter )
    {
        const char* pszIterKey = iter->first.c_str();
        int nIterKeyLen = iter->first.size();
        if( strncmp(pszPath,pszIterKey,nIterKeyLen) == 0 )
            return iter->second;

        /* "/vsimem\foo" should be handled as "/vsimem/foo" */
        if (nIterKeyLen && nPathLen > nIterKeyLen &&
            pszIterKey[nIterKeyLen-1] == '/' &&
            pszPath[nIterKeyLen-1] == '\\' &&
            strncmp(pszPath,pszIterKey,nIterKeyLen-1) == 0 )
            return iter->second;

        /* /vsimem should be treated as a match for /vsimem/ */
        if( nPathLen == nIterKeyLen - 1
            && strncmp(pszPath,pszIterKey,nIterKeyLen-1) == 0 )
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
}
