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
 * Revision 1.1  2005/09/11 18:00:55  fwarmerdam
 * New
 *
 */

#include "cpl_vsi_private.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                              VSIMkdir()                              */
/************************************************************************/

int VSIMkdir( const char *pszPathname, long mode )

{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszPathname );

    return poFSHandler->Mkdir( pszPathname, mode );
}

/************************************************************************/
/*                             VSIUnlink()                              */
/*************************a***********************************************/

int VSIUnlink( const char * pszFilename )

{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->Unlink( pszFilename );
}

/************************************************************************/
/*                              VSIRmdir()                              */
/************************************************************************/

int VSIRmdir( const char * pszFilename )

{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->Rmdir( pszFilename );
}

/************************************************************************/
/*                              VSIStatL()                              */
/************************************************************************/

int VSIStatL( const char * pszFilename, VSIStatBufL *psStatBuf )

{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszFilename );

    return poFSHandler->Stat( pszFilename, psStatBuf );
}

/************************************************************************/
/*                              VSIFOpen()                              */
/************************************************************************/

FILE *VSIFOpenL( const char * pszFilename, const char * pszAccess )

{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszFilename );

    return (FILE *) poFSHandler->Open( pszFilename, pszAccess );
}

/************************************************************************/
/*                             VSIFCloseL()                             */
/************************************************************************/

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

int VSIFSeekL( FILE * fp, vsi_l_offset nOffset, int nWhence )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Seek( nOffset, nWhence );
}

/************************************************************************/
/*                             VSIFTellL()                              */
/************************************************************************/

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

int VSIFFlushL( FILE * fp )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Flush();
}

/************************************************************************/
/*                             VSIFReadL()                              */
/************************************************************************/

size_t VSIFReadL( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Read( pBuffer, nSize, nCount );
}

/************************************************************************/
/*                             VSIFWriteL()                             */
/************************************************************************/

size_t VSIFWriteL( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    VSIVirtualHandle *poFileHandle = (VSIVirtualHandle *) fp;
    
    return poFileHandle->Write( pBuffer, nSize, nCount );
}

/************************************************************************/
/*                              VSIFEofL()                              */
/************************************************************************/

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

/************************************************************************/
/*                           VSIFileManager()                           */
/************************************************************************/

VSIFileManager::VSIFileManager()

{
    poDefaultHandler = NULL;
}

/************************************************************************/
/*                                Get()                                 */
/************************************************************************/

VSIFileManager *VSIFileManager::Get()

{
    static VSIFileManager *poManager = NULL;
    
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
