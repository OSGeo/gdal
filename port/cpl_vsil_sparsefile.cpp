/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of sparse file virtual io driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include <cstdlib>
#include <cstring>

#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi_virtual.h"

CPL_CVSID("$Id$")

class SFRegion {
public:
    CPLString     osFilename{};
    VSILFILE     *fp = nullptr;
    GUIntBig      nDstOffset = 0;
    GUIntBig      nSrcOffset = 0;
    GUIntBig      nLength = 0;
    GByte         byValue = 0;
    bool          bTriedOpen = false;
};

/************************************************************************/
/* ==================================================================== */
/*                         VSISparseFileHandle                          */
/* ==================================================================== */
/************************************************************************/

class VSISparseFileFilesystemHandler;

class VSISparseFileHandle : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSISparseFileHandle)

    VSISparseFileFilesystemHandler* m_poFS = nullptr;
    bool               bEOF = false;

  public:
    explicit VSISparseFileHandle(VSISparseFileFilesystemHandler* poFS) :
                m_poFS(poFS) {}

    GUIntBig           nOverallLength = 0;
    GUIntBig           nCurOffset = 0;

    std::vector<SFRegion> aoRegions{};

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Close() override;
};

/************************************************************************/
/* ==================================================================== */
/*                   VSISparseFileFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

class VSISparseFileFilesystemHandler : public VSIFilesystemHandler
{
    std::map<GIntBig, int> oRecOpenCount{};
    CPL_DISALLOW_COPY_ASSIGN(VSISparseFileFilesystemHandler)

public:
    VSISparseFileFilesystemHandler() = default;
    ~VSISparseFileFilesystemHandler() override = default;

    int              DecomposePath( const char *pszPath,
                                    CPLString &osFilename,
                                    vsi_l_offset &nSparseFileOffset,
                                    vsi_l_offset &nSparseFileSize );

    // TODO(schwehr): Fix VSISparseFileFilesystemHandler::Stat to not need using.
    using VSIFilesystemHandler::Open;

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
    for( unsigned int i = 0; i < aoRegions.size(); i++ )
    {
        if( aoRegions[i].fp != nullptr )
            CPL_IGNORE_RET_VAL(VSIFCloseL( aoRegions[i].fp ));
    }

    return 0;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSISparseFileHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    bEOF = false;
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
    if( nCurOffset >= nOverallLength )
    {
        bEOF = true;
        return 0;
    }

/* -------------------------------------------------------------------- */
/*      Find what region we are in, searching linearly from the         */
/*      start.                                                          */
/* -------------------------------------------------------------------- */
    unsigned int iRegion = 0;  // Used after for.

    for( ; iRegion < aoRegions.size(); iRegion++ )
    {
        if( nCurOffset >= aoRegions[iRegion].nDstOffset &&
            nCurOffset <
                aoRegions[iRegion].nDstOffset + aoRegions[iRegion].nLength )
            break;
    }

    size_t nBytesRequested = nSize * nCount;
    if( nBytesRequested == 0 )
    {
        return 0;
    }
    if( nCurOffset + nBytesRequested > nOverallLength )
    {
        nBytesRequested = static_cast<size_t>(nOverallLength - nCurOffset);
        bEOF = true;
    }

/* -------------------------------------------------------------------- */
/*      Default to zeroing the buffer if no corresponding region was    */
/*      found.                                                          */
/* -------------------------------------------------------------------- */
    if( iRegion == aoRegions.size() )
    {
        memset( pBuffer, 0, nBytesRequested);
        nCurOffset += nBytesRequested;
        return nBytesRequested / nSize;
    }

/* -------------------------------------------------------------------- */
/*      If this request crosses region boundaries, split it into two    */
/*      requests.                                                       */
/* -------------------------------------------------------------------- */
    size_t nBytesReturnCount = 0;
    const GUIntBig nEndOffsetOfRegion =
        aoRegions[iRegion].nDstOffset + aoRegions[iRegion].nLength;

    if( nCurOffset + nBytesRequested > nEndOffsetOfRegion )
    {
        const size_t nExtraBytes =
            static_cast<size_t>(nCurOffset + nBytesRequested - nEndOffsetOfRegion);
        // Recurse to get the rest of the request.

        const GUIntBig nCurOffsetSave = nCurOffset;
        nCurOffset += nBytesRequested - nExtraBytes;
        bool bEOFSave = bEOF;
        bEOF = false;
        const size_t nBytesRead =
            this->Read( static_cast<char *>(pBuffer) + nBytesRequested - nExtraBytes,
                        1, nExtraBytes );
        nCurOffset = nCurOffsetSave;
        bEOF = bEOFSave;

        nBytesReturnCount += nBytesRead;
        nBytesRequested -= nExtraBytes;
    }

/* -------------------------------------------------------------------- */
/*      Handle a constant region.                                       */
/* -------------------------------------------------------------------- */
    if( aoRegions[iRegion].osFilename.empty() )
    {
        memset( pBuffer, aoRegions[iRegion].byValue,
                static_cast<size_t>(nBytesRequested) );

        nBytesReturnCount += nBytesRequested;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise handle as a file.                                     */
/* -------------------------------------------------------------------- */
    else
    {
        if( aoRegions[iRegion].fp == nullptr )
        {
            if( !aoRegions[iRegion].bTriedOpen )
            {
                aoRegions[iRegion].fp =
                    VSIFOpenL( aoRegions[iRegion].osFilename, "r" );
                if( aoRegions[iRegion].fp == nullptr )
                {
                    CPLDebug( "/vsisparse/", "Failed to open '%s'.",
                              aoRegions[iRegion].osFilename.c_str() );
                }
                aoRegions[iRegion].bTriedOpen = true;
            }
            if( aoRegions[iRegion].fp == nullptr )
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

        m_poFS->IncRecCounter();
        const size_t nBytesRead =
            VSIFReadL( pBuffer, 1, static_cast<size_t>(nBytesRequested),
                       aoRegions[iRegion].fp );
        m_poFS->DecRecCounter();

        nBytesReturnCount += nBytesRead;
    }

    nCurOffset += nBytesReturnCount;

    return nBytesReturnCount / nSize;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSISparseFileHandle::Write( const void * /* pBuffer */,
                                   size_t /* nSize */,
                                   size_t /* nCount */ )
{
    errno = EBADF;
    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSISparseFileHandle::Eof()

{
    return bEOF ? 1 : 0;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSISparseFileFilesystemHandler                 */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSISparseFileFilesystemHandler::Open( const char *pszFilename,
                                      const char *pszAccess,
                                      bool /* bSetError */,
                                      CSLConstList /* papszOptions */ )

{
    if( !STARTS_WITH_CI(pszFilename, "/vsisparse/") )
        return nullptr;

    if( !EQUAL(pszAccess, "r") && !EQUAL(pszAccess, "rb") )
    {
        errno = EACCES;
        return nullptr;
    }

    // Arbitrary number.
    if( GetRecCounter() == 32 )
        return nullptr;

    const CPLString osSparseFilePath = pszFilename + 11;

/* -------------------------------------------------------------------- */
/*      Does this file even exist?                                      */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osSparseFilePath, "r" );
    if( fp == nullptr )
        return nullptr;
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

/* -------------------------------------------------------------------- */
/*      Read the XML file.                                              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psXMLRoot = CPLParseXMLFile( osSparseFilePath );

    if( psXMLRoot == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Setup the file handle on this file.                             */
/* -------------------------------------------------------------------- */
    VSISparseFileHandle *poHandle = new VSISparseFileHandle(this);

/* -------------------------------------------------------------------- */
/*      Translate the desired fields out of the XML tree.               */
/* -------------------------------------------------------------------- */
    for( CPLXMLNode *psRegion = psXMLRoot->psChild;
         psRegion != nullptr;
         psRegion = psRegion->psNext )
    {
        if( psRegion->eType != CXT_Element )
            continue;

        if( !EQUAL(psRegion->pszValue, "SubfileRegion")
            && !EQUAL(psRegion->pszValue, "ConstantRegion") )
            continue;

        SFRegion oRegion;

        oRegion.osFilename = CPLGetXMLValue( psRegion, "Filename", "" );
        if( atoi(CPLGetXMLValue( psRegion, "Filename.relative", "0" )) != 0 )
        {
            const CPLString osSFPath = CPLGetPath(osSparseFilePath);
            oRegion.osFilename = CPLFormFilename( osSFPath,
                                                  oRegion.osFilename, nullptr );
        }

        // TODO(schwehr): Symbolic constant and an explanation for 32.
        oRegion.nDstOffset =
            CPLScanUIntBig( CPLGetXMLValue(psRegion, "DestinationOffset", "0"),
                            32 );

        oRegion.nSrcOffset =
            CPLScanUIntBig( CPLGetXMLValue(psRegion, "SourceOffset", "0"), 32);

        oRegion.nLength =
            CPLScanUIntBig( CPLGetXMLValue(psRegion, "RegionLength", "0"), 32);

        oRegion.byValue = static_cast<GByte>(
            atoi(CPLGetXMLValue(psRegion, "Value", "0")));

        poHandle->aoRegions.push_back( oRegion );
    }

/* -------------------------------------------------------------------- */
/*      Get sparse file length, use maximum bound of regions if not     */
/*      explicit in file.                                               */
/* -------------------------------------------------------------------- */
    poHandle->nOverallLength =
        CPLScanUIntBig( CPLGetXMLValue(psXMLRoot, "Length", "0" ), 32);
    if( poHandle->nOverallLength == 0 )
    {
        for( unsigned int i = 0; i < poHandle->aoRegions.size(); i++ )
        {
            poHandle->nOverallLength =
                std::max(poHandle->nOverallLength,
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
    // TODO(schwehr): Fix this so that the using statement is not needed.
    // Will just adding the bool for bSetError be okay?
    VSIVirtualHandle *poFile = Open( pszFilename, "r" );

    memset( psStatBuf, 0, sizeof(VSIStatBufL) );

    if( poFile == nullptr )
        return -1;

    poFile->Seek( 0, SEEK_END );
    const size_t nLength = static_cast<size_t>(poFile->Tell());
    delete poFile;

    const int nResult =
        VSIStatExL( pszFilename + strlen("/vsisparse/"), psStatBuf, nFlags );

    psStatBuf->st_size = nLength;

    return nResult;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSISparseFileFilesystemHandler::Unlink( const char * /* pszFilename */ )
{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSISparseFileFilesystemHandler::Mkdir( const char * /* pszPathname */,
                                           long /* nMode */ )
{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSISparseFileFilesystemHandler::Rmdir( const char * /* pszPathname */ )
{
    errno = EACCES;
    return -1;
}

/************************************************************************/
/*                              ReadDir()                               */
/************************************************************************/

char **VSISparseFileFilesystemHandler::ReadDir( const char * /* pszPath */ )
{
    errno = EACCES;
    return nullptr;
}

/************************************************************************/
/*                 VSIInstallSparseFileFilesystemHandler()              */
/************************************************************************/

/*!
 \brief Install /vsisparse/ virtual file handler.

 \verbatim embed:rst
 See :ref:`/vsisparse/ documentation <vsisparse>`
 \endverbatim
 */

void VSIInstallSparseFileHandler()
{
    VSIFileManager::InstallHandler( "/vsisparse/",
                                    new VSISparseFileFilesystemHandler );
}
