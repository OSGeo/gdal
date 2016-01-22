/******************************************************************************
 * $Id$
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of sparse file virtual io driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_minixml.h"
#include <map>

#if defined(WIN32CE)
#  include <wce_errno.h>
#endif

CPL_CVSID("$Id$");

class SFRegion { 
public:
    SFRegion() : fp(NULL), nDstOffset(0), nSrcOffset(0), nLength(0), 
                 byValue(0), bTriedOpen(FALSE) {}

    CPLString     osFilename;
    VSILFILE     *fp;
    GUIntBig      nDstOffset;
    GUIntBig      nSrcOffset;
    GUIntBig      nLength;
    GByte         byValue;
    int           bTriedOpen;
};

/************************************************************************/
/* ==================================================================== */
/*                         VSISparseFileHandle                          */
/* ==================================================================== */
/************************************************************************/

class VSISparseFileFilesystemHandler;

class VSISparseFileHandle : public VSIVirtualHandle
{ 
    VSISparseFileFilesystemHandler* poFS;

  public:
    VSISparseFileHandle(VSISparseFileFilesystemHandler* poFS) : poFS(poFS), nOverallLength(0), nCurOffset(0) {}

    GUIntBig           nOverallLength;
    GUIntBig           nCurOffset;

    std::vector<SFRegion> aoRegions;

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Close();
};

/************************************************************************/
/* ==================================================================== */
/*                   VSISparseFileFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

class VSISparseFileFilesystemHandler : public VSIFilesystemHandler 
{
    std::map<GIntBig, int> oRecOpenCount;

public:
                     VSISparseFileFilesystemHandler();
    virtual          ~VSISparseFileFilesystemHandler();

    int              DecomposePath( const char *pszPath, 
                                    CPLString &osFilename, 
                                    vsi_l_offset &nSparseFileOffset,
                                    vsi_l_offset &nSparseFileSize );

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );
    virtual int      Unlink( const char *pszFilename );
    virtual int      Mkdir( const char *pszDirname, long nMode );
    virtual int      Rmdir( const char *pszDirname );
    virtual char   **ReadDir( const char *pszDirname );
    
    int              GetRecCounter() { return oRecOpenCount[CPLGetPID()]; }
    void             IncRecCounter() { oRecOpenCount[CPLGetPID()] ++; }
    void             DecRecCounter() { oRecOpenCount[CPLGetPID()] --; }
};

/************************************************************************/
/* ==================================================================== */
/*                             VSISparseFileHandle                      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSISparseFileHandle::Close()

{
    unsigned int i;

    for( i = 0; i < aoRegions.size(); i++ )
    {
        if( aoRegions[i].fp != NULL )
            VSIFCloseL( aoRegions[i].fp );
    }

    return 0;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSISparseFileHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    if( nWhence == SEEK_SET )
        nCurOffset = nOffset;
    else if( nWhence == SEEK_CUR )
    {
        nCurOffset += nOffset;
    }
    else if( nWhence == SEEK_END )
    {
        nCurOffset = nOverallLength + nOffset;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSISparseFileHandle::Tell()

{
    return nCurOffset;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSISparseFileHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
/* -------------------------------------------------------------------- */
/*      Find what region we are in, searching linearly from the         */
/*      start.                                                          */
/* -------------------------------------------------------------------- */
    unsigned int iRegion;

    for( iRegion = 0; iRegion < aoRegions.size(); iRegion++ )
    {
        if( nCurOffset >= aoRegions[iRegion].nDstOffset
            && nCurOffset < aoRegions[iRegion].nDstOffset + aoRegions[iRegion].nLength )
            break;
    }

/* -------------------------------------------------------------------- */
/*      Default to zeroing the buffer if no corresponding region was    */
/*      found.                                                          */
/* -------------------------------------------------------------------- */
    if( iRegion == aoRegions.size() )
    {
        memset( pBuffer, 0, nSize * nCount );
        nCurOffset += nSize * nSize;
        return nCount;
    }

/* -------------------------------------------------------------------- */
/*      If this request crosses region boundaries, split it into two    */
/*      requests.                                                       */
/* -------------------------------------------------------------------- */
    size_t nReturnCount = nCount;
    GUIntBig nBytesRequested = nSize * nCount;
    GUIntBig nBytesAvailable = 
        aoRegions[iRegion].nDstOffset + aoRegions[iRegion].nLength;

    if( nCurOffset + nBytesRequested > nBytesAvailable )
    {
        size_t nExtraBytes = 
            (size_t) (nCurOffset + nBytesRequested - nBytesAvailable);
        // Recurse to get the rest of the request.
        
        GUIntBig nCurOffsetSave = nCurOffset;
        nCurOffset += nBytesRequested - nExtraBytes;
        size_t nBytesRead = 
            this->Read( ((char *) pBuffer) + nBytesRequested - nExtraBytes,
                        1, nExtraBytes );
        nCurOffset = nCurOffsetSave;

        if( nBytesRead < nExtraBytes )
            nReturnCount -= (nExtraBytes-nBytesRead) / nSize;

        nBytesRequested -= nExtraBytes;
    }

/* -------------------------------------------------------------------- */
/*      Handle a constant region.                                       */
/* -------------------------------------------------------------------- */
    if( aoRegions[iRegion].osFilename.size() == 0 )
    {
        memset( pBuffer, aoRegions[iRegion].byValue, 
                (size_t) nBytesRequested );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise handle as a file.                                     */
/* -------------------------------------------------------------------- */
    else
    {
        if( aoRegions[iRegion].fp == NULL )
        {
            if( !aoRegions[iRegion].bTriedOpen )
            {
                aoRegions[iRegion].fp = 
                    VSIFOpenL( aoRegions[iRegion].osFilename, "r" );
                if( aoRegions[iRegion].fp == NULL )
                {
                    CPLDebug( "/vsisparse/", "Failed to open '%s'.", 
                              aoRegions[iRegion].osFilename.c_str() );
                }
                aoRegions[iRegion].bTriedOpen = TRUE;
            }
            if( aoRegions[iRegion].fp == NULL )
            {
                return 0;
            }
        }

        if( VSIFSeekL( aoRegions[iRegion].fp, 
                       nCurOffset 
                       - aoRegions[iRegion].nDstOffset 
                       + aoRegions[iRegion].nSrcOffset,
                       SEEK_SET ) != 0 )
            return 0;

        poFS->IncRecCounter();
        size_t nBytesRead = VSIFReadL( pBuffer, 1, (size_t) nBytesRequested, 
                                       aoRegions[iRegion].fp );
        poFS->DecRecCounter();

        if( nBytesAvailable < nBytesRequested )
            nReturnCount = nBytesRead / nSize;
    }

    nCurOffset += nReturnCount * nSize;

    return nReturnCount;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSISparseFileHandle::Write( CPL_UNUSED const void * pBuffer, CPL_UNUSED size_t nSize, CPL_UNUSED size_t nCount )
{
    errno = EBADF;
    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSISparseFileHandle::Eof()

{
    return nCurOffset >= nOverallLength;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSISparseFileFilesystemHandler                 */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                      VSISparseFileFilesystemHandler()                */
/************************************************************************/

VSISparseFileFilesystemHandler::VSISparseFileFilesystemHandler()

{
}

/************************************************************************/
/*                      ~VSISparseFileFilesystemHandler()               */
/************************************************************************/

VSISparseFileFilesystemHandler::~VSISparseFileFilesystemHandler()

{
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSISparseFileFilesystemHandler::Open( const char *pszFilename, 
                                      const char *pszAccess )

{
    CPLAssert( EQUALN(pszFilename,"/vsisparse/", 11) );

    if( !EQUAL(pszAccess,"r") && !EQUAL(pszAccess,"rb") )
    {
        errno = EACCES;
        return NULL;
    }

    /* Arbitrary number */
    if( GetRecCounter() == 32 )
        return NULL;

    CPLString osSparseFilePath = pszFilename + 11;

/* -------------------------------------------------------------------- */
/*      Does this file even exist?                                      */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osSparseFilePath, "r" );
    if( fp == NULL )
        return NULL;
    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Read the XML file.                                              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psXMLRoot = CPLParseXMLFile( osSparseFilePath );

    if( psXMLRoot == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Setup the file handle on this file.                             */
/* -------------------------------------------------------------------- */
    VSISparseFileHandle *poHandle = new VSISparseFileHandle(this);

/* -------------------------------------------------------------------- */
/*      Translate the desired fields out of the XML tree.               */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRegion;

    for( psRegion = psXMLRoot->psChild;
         psRegion != NULL;
         psRegion = psRegion->psNext )
    {
        if( psRegion->eType != CXT_Element )
            continue;

        if( !EQUAL(psRegion->pszValue,"SubfileRegion")
            && !EQUAL(psRegion->pszValue,"ConstantRegion") )
            continue;

        SFRegion oRegion;

        oRegion.osFilename = CPLGetXMLValue( psRegion, "Filename", "" );
        if( atoi(CPLGetXMLValue( psRegion, "Filename.relative", "0" )) != 0 )
        {
            CPLString osSFPath = CPLGetPath(osSparseFilePath);
            oRegion.osFilename = CPLFormFilename( osSFPath,
                                                  oRegion.osFilename, NULL );
        }

        oRegion.nDstOffset = 
            CPLScanUIntBig( CPLGetXMLValue(psRegion,"DestinationOffset","0" ),
                            32 );
                            
        oRegion.nSrcOffset = 
            CPLScanUIntBig( CPLGetXMLValue(psRegion,"SourceOffset","0" ), 32);

        oRegion.nLength = 
            CPLScanUIntBig( CPLGetXMLValue(psRegion,"RegionLength","0" ), 32);

        oRegion.byValue = (GByte) atoi(CPLGetXMLValue(psRegion,"Value","0" ));

        poHandle->aoRegions.push_back( oRegion );
    }

/* -------------------------------------------------------------------- */
/*      Get sparse file length, use maximum bound of regions if not     */
/*      explicit in file.                                               */
/* -------------------------------------------------------------------- */
    poHandle->nOverallLength = 
        CPLScanUIntBig( CPLGetXMLValue(psXMLRoot,"Length","0" ), 32);
    if( poHandle->nOverallLength == 0 )
    {
        unsigned int i;

        for( i = 0; i < poHandle->aoRegions.size(); i++ )
        {
            poHandle->nOverallLength = MAX(poHandle->nOverallLength,
                                           poHandle->aoRegions[i].nDstOffset
                                           + poHandle->aoRegions[i].nLength);
        }
    }

    CPLDestroyXMLNode( psXMLRoot );

    return poHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSISparseFileFilesystemHandler::Stat( const char * pszFilename, 
                                          VSIStatBufL * psStatBuf,
                                          int nFlags )
    
{
    VSIVirtualHandle *poFile = Open( pszFilename, "r" );

    memset( psStatBuf, 0, sizeof(VSIStatBufL) );

    if( poFile == NULL )
        return -1;

    poFile->Seek( 0, SEEK_END );
    size_t nLength = (size_t) poFile->Tell();
    delete poFile;

    int nResult = VSIStatExL( pszFilename + strlen("/vsisparse/"), 
                            psStatBuf, nFlags );

    psStatBuf->st_size = nLength;

    return nResult;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSISparseFileFilesystemHandler::Unlink( CPL_UNUSED const char * pszFilename )
{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSISparseFileFilesystemHandler::Mkdir( CPL_UNUSED const char * pszPathname,
                                           CPL_UNUSED long nMode )
{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSISparseFileFilesystemHandler::Rmdir( CPL_UNUSED const char * pszPathname )
{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                              ReadDir()                               */
/************************************************************************/

char **VSISparseFileFilesystemHandler::ReadDir( CPL_UNUSED const char *pszPath )
{
    errno = EACCES;
    return NULL;
}

/************************************************************************/
/*                 VSIInstallSparseFileFilesystemHandler()              */
/************************************************************************/

/**
 * Install /vsisparse/ virtual file handler. 
 *
 * The sparse virtual file handler allows a virtual file to be composed
 * from chunks of data in other files, potentially with large spaces in
 * the virtual file set to a constant value.  This can make it possible to
 * test some sorts of operations on what seems to be a large file with
 * image data set to a constant value.  It is also helpful when wanting to
 * add test files to the test suite that are too large, but for which most
 * of the data can be ignored.  It could, in theory, also be used to 
 * treat several files on different file systems as one large virtual file. 
 *
 * The file referenced by /vsisparse/ should be an XML control file 
 * formatted something like:
 *
 * 
\verbatim
<VSISparseFile>
  <Length>87629264</Length>
  <SubfileRegion>  Stuff at start of file.
    <Filename relative="1">251_head.dat</Filename>
    <DestinationOffset>0</DestinationOffset>
    <SourceOffset>0</SourceOffset>
    <RegionLength>2768</RegionLength>
  </SubfileRegion>

  <SubfileRegion>  RasterDMS node.
    <Filename relative="1">251_rasterdms.dat</Filename>
    <DestinationOffset>87313104</DestinationOffset>
    <SourceOffset>0</SourceOffset>
    <RegionLength>160</RegionLength>
  </SubfileRegion>

  <SubfileRegion>  Stuff at end of file.
    <Filename relative="1">251_tail.dat</Filename>
    <DestinationOffset>87611924</DestinationOffset>
    <SourceOffset>0</SourceOffset>
    <RegionLength>17340</RegionLength>
  </SubfileRegion>

  <ConstantRegion>  Default for the rest of the file.
    <DestinationOffset>0</DestinationOffset>
    <RegionLength>87629264</RegionLength>
    <Value>0</Value>
  </ConstantRegion>
</VSISparseFile>
\endverbatim
 *
 * Hopefully the values and semantics are fairly obvious. 
 *
 * This driver is installed by default. 
 */

void VSIInstallSparseFileHandler()
{
    VSIFileManager::InstallHandler( "/vsisparse/", 
                                    new VSISparseFileFilesystemHandler );
}
                            
