/******************************************************************************
 * $Id$
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of Memory Buffer virtual IO functions.
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
 * Revision 1.3  2005/09/12 00:37:55  fwarmerdam
 * fixed ownership in buffer to file function
 *
 * Revision 1.2  2005/09/11 18:32:07  fwarmerdam
 * tweak bigint expression to avoid vc6 problems
 *
 * Revision 1.1  2005/09/11 18:00:30  fwarmerdam
 * New
 *
 */

#include "cpl_vsi_private.h"
#include "cpl_string.h"
#include <map>

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                              VSIMemFile                              */
/* ==================================================================== */
/************************************************************************/

class VSIMemFile
{
public:
    CPLString     osFilename;
    int           nRefCount;

    int           bIsDirectory;

    int           bOwnData;
    GByte        *pabyData;
    vsi_l_offset  nLength;
    vsi_l_offset  nAllocLength;

                  VSIMemFile();
    virtual       ~VSIMemFile();

    bool          SetLength( vsi_l_offset nNewSize );
};

/************************************************************************/
/* ==================================================================== */
/*                             VSIMemHandle                             */
/* ==================================================================== */
/************************************************************************/

class VSIMemHandle : public VSIVirtualHandle
{ 
  public:
    VSIMemFile    *poFile;
    vsi_l_offset  nOffset;

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Close();
};

/************************************************************************/
/* ==================================================================== */
/*                       VSIMemFilesystemHandler                        */
/* ==================================================================== */
/************************************************************************/

class VSIMemFilesystemHandler : public VSIFilesystemHandler 
{
public:
    std::map<CPLString,VSIMemFile*>   oFileList;

    virtual          ~VSIMemFilesystemHandler();

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
/*                              VSIMemFile                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             VSIMemFile()                             */
/************************************************************************/

VSIMemFile::VSIMemFile()

{
    nRefCount = 0;
    bIsDirectory = FALSE;
    bOwnData = TRUE;
    pabyData = NULL;
    nLength = 0;
    nAllocLength = 0;
}

/************************************************************************/
/*                            ~VSIMemFile()                             */
/************************************************************************/

VSIMemFile::~VSIMemFile()

{
    if( nRefCount != 0 )
        CPLDebug( "VSIMemFile", "Memory file %s deleted with %d references.",
                  osFilename.c_str(), nRefCount );

    if( bOwnData && pabyData )
        CPLFree( pabyData );
}

/************************************************************************/
/*                             SetLength()                              */
/************************************************************************/

bool VSIMemFile::SetLength( vsi_l_offset nNewLength )

{
/* -------------------------------------------------------------------- */
/*      Grow underlying array if needed.                                */
/* -------------------------------------------------------------------- */
    if( nNewLength > nAllocLength )
    {
        GByte *pabyNewData;
        vsi_l_offset nNewAlloc = (nNewLength + nNewLength / 10) + 5000;

        pabyNewData = (GByte *) CPLRealloc(pabyData, nNewAlloc);
        if( pabyNewData == NULL )
            return false;

        pabyData = pabyNewData;
        nAllocLength = nNewAlloc;
    }

    nLength = nNewLength;

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
    poFile->nRefCount--;
    poFile = NULL;

    return 0;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIMemHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    if( nWhence == SEEK_CUR )
        this->nOffset += nOffset;
    else if( nWhence == SEEK_SET )
        this->nOffset = nOffset;
    else if( nWhence == SEEK_END )
        this->nOffset = poFile->nLength + nOffset;
    else
    {
        errno = EINVAL;
        return -1;
    }

    if( this->nOffset < 0 )
    {
        this->nOffset = 0;
        return -1;
    }
    
    if( this->nOffset > poFile->nLength )
    {
        if( !poFile->SetLength( this->nOffset ) )
            return -1;
    }

    return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIMemHandle::Tell()

{
    return nOffset;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIMemHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    int nBytesToRead = nSize * nCount; 

    if( nBytesToRead + nOffset > poFile->nLength )
    {
        nBytesToRead = poFile->nLength - nOffset;
        nCount = nBytesToRead / nSize;
    }

    memcpy( pBuffer, poFile->pabyData + nOffset, nBytesToRead );
    nOffset += nBytesToRead;

    return nCount;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIMemHandle::Write( void * pBuffer, size_t nSize, size_t nCount )

{
    int nBytesToWrite = nSize * nCount; 

    if( nBytesToWrite + nOffset > poFile->nLength )
    {
        if( !poFile->SetLength( nBytesToWrite + nOffset ) )
            return 0;
    }

    memcpy( poFile->pabyData + nOffset, pBuffer, nBytesToWrite );
    nOffset += nBytesToWrite;

    return nCount;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIMemHandle::Eof()

{
    return nOffset == poFile->nLength;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIMemFilesystemHandler                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                      ~VSIMemFilesystemHandler()                      */
/************************************************************************/

VSIMemFilesystemHandler::~VSIMemFilesystemHandler()

{
    std::map<CPLString,VSIMemFile*>::const_iterator iter;

    for( iter = oFileList.begin(); iter != oFileList.end(); iter++ )
        delete iter->second;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIMemFilesystemHandler::Open( const char *pszFilename, 
                               const char *pszAccess )

{
    VSIMemFile *poFile;

/* -------------------------------------------------------------------- */
/*      Get the filename we are opening, create if needed.              */
/* -------------------------------------------------------------------- */
    if( oFileList.find(pszFilename) == oFileList.end() )
        poFile = NULL;
    else
        poFile = oFileList[pszFilename];

    if( strstr(pszAccess,"w") == NULL && poFile == NULL )
    {
        errno = ENOENT;
        return NULL;
    }

    if( strstr(pszAccess,"w") )
    {
        if( poFile )
            poFile->SetLength( 0 );
        else
        {
            poFile = new VSIMemFile;
            poFile->osFilename = pszFilename;
            oFileList[poFile->osFilename] = poFile;
        }
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
    poHandle->nOffset = 0;

    poFile->nRefCount++;

    if( strstr(pszAccess,"a") )
        poHandle->nOffset = poFile->nLength;

    return poHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIMemFilesystemHandler::Stat( const char * pszFilename, 
                                   VSIStatBufL * pStatBuf )
    
{
    if( oFileList.find(pszFilename) == oFileList.end() )
    {
        errno = ENOENT;
        return -1;
    }

    VSIMemFile *poFile = oFileList[pszFilename];

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
    }

    return 0;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSIMemFilesystemHandler::Unlink( const char * pszFilename )

{
    VSIMemFile *poFile;

    if( oFileList.find(pszFilename) == oFileList.end() )
    {
        errno = ENOENT;
        return -1;
    }
    else
    {
        poFile = oFileList[pszFilename];
        delete poFile;

        oFileList.erase( oFileList.find(pszFilename) );

        return 0;
    }
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIMemFilesystemHandler::Mkdir( const char * pszPathname,
                                    long nMode )

{
    if( oFileList.find(pszPathname) != oFileList.end() )
    {
        errno = EEXIST;
        return -1;
    }

    VSIMemFile *poFile = new VSIMemFile;

    poFile->osFilename = pszPathname;
    poFile->bIsDirectory = TRUE;
    oFileList[pszPathname] = poFile;

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
/*                              ReadDir()                               */
/************************************************************************/

char **VSIMemFilesystemHandler::ReadDir( const char *pszPath )

{
    return NULL;
}

/************************************************************************/
/*                     VSIInstallLargeFileHandler()                     */
/************************************************************************/

void VSIInstallMemFileHandler()

{
    VSIFileManager::InstallHandler( string("/vsimem/"), 
                                    new VSIMemFilesystemHandler );
}

/************************************************************************/
/*                        VSIFileFromMemBuffer()                        */
/************************************************************************/

FILE *VSIFileFromMemBuffer( const char *pszFilename, 
                          GByte *pabyData, 
                          vsi_l_offset nDataLength,
                          int bTakeOwnership )

{
    if( VSIFileManager::GetHandler("") 
        == VSIFileManager::GetHandler("/vsimem/") )
        VSIInstallMemFileHandler();

    VSIMemFilesystemHandler *poHandler = (VSIMemFilesystemHandler *) 
        VSIFileManager::GetHandler("/vsimem/");

    VSIMemFile *poFile = new VSIMemFile;

    poFile->osFilename = pszFilename;
    poFile->bOwnData = bTakeOwnership;
    poFile->pabyData = pabyData;
    poFile->nLength = nDataLength;
    poFile->nAllocLength = nDataLength;

    poHandler->oFileList[poFile->osFilename] = poFile;

    return (FILE *) poHandler->Open( pszFilename, "r+" );
}

