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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.8  2006/09/21 07:55:22  dron
 * Fixed typo in documentation.
 *
 * Revision 1.7  2006/03/27 18:36:17  fwarmerdam
 * VSIFWriteL has const buffer now
 *
 * Revision 1.6  2006/01/11 00:29:54  fwarmerdam
 * added brief not on multithreading
 *
 * Revision 1.5  2006/01/10 17:03:56  fwarmerdam
 * added VSI Rename support
 *
 * Revision 1.4  2005/10/07 00:26:27  fwarmerdam
 * add documentation
 *
 * Revision 1.3  2005/09/15 18:39:00  fwarmerdam
 * fixedup filemanager cleanup
 *
 * Revision 1.2  2005/09/15 18:32:35  fwarmerdam
 * added VSICleanupFileManager
 *
 * Revision 1.1  2005/09/11 18:00:55  fwarmerdam
 * New
 *
 */

#include "cpl_vsi_private.h"

CPL_CVSID("$Id$");

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
 * @param pszPathname the path to the directory to create. 
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
 * @param pszFilename the path of the file to be deleted.
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
 * @param oldpath the name of the file to be renamed.
 * @param newpath the name the file should be given. 
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
 * @param pszDirname the path of the directory to be deleted.
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
 * @param pszFilename the path of the filesystem object to be queried.
 * @param psStatBuf the structure to load with information. 
 *
 * @return 0 on success or -1 on an error.
 */

int VSIStatL( const char * pszFilename, VSIStatBufL *psStatBuf )

{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->Stat( pszFilename, psStatBuf );
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
 * Note that the "FILE *" returned by this function is not really a 
 * standard C library FILE *, and cannot be used with any functions other
 * than the "VSI*L" family of functions.  They aren't "real" FILE objects.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fopen() function.
 *
 * @param pszFilename the file to open.
 * @param pszAccess access requested (ie. "r", "r+", "w".  
 *
 * @return NULL on failure, or the file handle.
 */

FILE *VSIFOpenL( const char * pszFilename, const char * pszAccess )

{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszFilename );

    return (FILE *) poFSHandler->Open( pszFilename, pszAccess );
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

int VSIFCloseL( FILE * fp )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
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

int VSIFSeekL( FILE * fp, vsi_l_offset nOffset, int nWhence )

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

vsi_l_offset VSIFTellL( FILE * fp )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Tell();
}

/************************************************************************/
/*                             VSIRewindL()                             */
/************************************************************************/

void VSIRewindL( FILE * fp )

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

int VSIFFlushL( FILE * fp )

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

size_t VSIFReadL( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

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

size_t VSIFWriteL( const void *pBuffer, size_t nSize, size_t nCount, FILE *fp )

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

int VSIFEofL( FILE * fp )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Eof();
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
         iter++ )
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
        VSIInstallMemFileHandler();
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

    for( iter = poThis->oHandlers.begin();
         iter != poThis->oHandlers.end();
         iter++ )
    {
        if( strncmp(pszPath,iter->first.c_str(),iter->first.size()) == 0 )
            return iter->second;
    }
    
    return poThis->poDefaultHandler;
}

/************************************************************************/
/*                           InstallHandler()                           */
/************************************************************************/

void VSIFileManager::InstallHandler( std::string osPrefix,
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
