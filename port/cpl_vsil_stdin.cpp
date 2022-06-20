/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for stdin
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "cpl_port.h"
#include "cpl_vsi.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <algorithm>
#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi_virtual.h"

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

CPL_CVSID("$Id$")

static std::string gosStdinFilename{};
static FILE* gStdinFile = stdin;
static GByte* gpabyBuffer = nullptr;
static size_t gnBufferLimit = 0; // maximum that can be allocated
static size_t gnBufferAlloc = 0; // current allocation
static size_t gnBufferLen = 0;   // number of valid bytes in gpabyBuffer
static uint64_t gnRealPos = 0;     // current offset on stdin
static bool   gbHasSeekedToEnd = false;
static uint64_t gnFileSize = 0;

/************************************************************************/
/*                           VSIStdinInit()                             */
/************************************************************************/

static void VSIStdinInit()
{
    if( gpabyBuffer == nullptr )
    {
#ifdef WIN32
        setmode( fileno( stdin ), O_BINARY );
#endif
        constexpr size_t MAX_INITIAL_ALLOC = 1024 * 1024;
        gnBufferAlloc = std::min(gnBufferAlloc, MAX_INITIAL_ALLOC);
        gpabyBuffer = static_cast<GByte *>(CPLMalloc(gnBufferAlloc));
    }
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIStdinFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

class VSIStdinFilesystemHandler final : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIStdinFilesystemHandler)

  public:
    VSIStdinFilesystemHandler();
    ~VSIStdinFilesystemHandler() override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError,
                            CSLConstList /* papszOptions */ ) override;
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIStdinHandle                               */
/* ==================================================================== */
/************************************************************************/

class VSIStdinHandle final : public VSIVirtualHandle
{
  private:
    CPL_DISALLOW_COPY_ASSIGN(VSIStdinHandle)

    bool              m_bEOF = false;
    uint64_t          m_nCurOff = 0;
    size_t            ReadAndCache( void* pBuffer, size_t nToRead );

  public:
    VSIStdinHandle() = default;
    ~VSIStdinHandle() override { VSIStdinHandle::Close(); }

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Close() override;
};

/************************************************************************/
/*                              ReadAndCache()                          */
/************************************************************************/

size_t VSIStdinHandle::ReadAndCache( void* pUserBuffer, size_t nToRead )
{
    CPLAssert(m_nCurOff == gnRealPos);

    const size_t nRead = fread(pUserBuffer, 1, nToRead, gStdinFile);

    if( gnRealPos < gnBufferLimit )
    {
        bool bCopyInBuffer = true;
        const size_t nToCopy = static_cast<size_t>(std::min(
            gnBufferLimit - gnRealPos, static_cast<uint64_t>(nRead)));
        if( gnRealPos + nToCopy > gnBufferAlloc )
        {
            auto newAlloc = gnRealPos + nToCopy;
            if( newAlloc < gnBufferLimit - newAlloc / 3 )
                newAlloc += newAlloc / 3;
            else
                newAlloc = gnBufferLimit;
            GByte* newBuffer = static_cast<GByte*>(
                VSI_REALLOC_VERBOSE(gpabyBuffer, static_cast<size_t>(newAlloc)));
            if( newBuffer == nullptr )
            {
                bCopyInBuffer = false;
            }
            else
            {
                gpabyBuffer = newBuffer;
                gnBufferAlloc = static_cast<size_t>(newAlloc);
            }
        }
        if( bCopyInBuffer )
        {
            memcpy(gpabyBuffer + static_cast<size_t>(gnRealPos),
                   pUserBuffer, nToCopy);
            gnBufferLen += nToCopy;
        }
    }

    m_nCurOff += nRead;
    gnRealPos = m_nCurOff;

    if( nRead < nToRead )
    {
        gnFileSize = gnRealPos;
        gbHasSeekedToEnd = true;
    }

    return nRead;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIStdinHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    m_bEOF = false;

    if( nWhence == SEEK_SET && nOffset == m_nCurOff )
        return 0;

    VSIStdinInit();

    if( nWhence == SEEK_END )
    {
        if( nOffset != 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Seek(xx != 0, SEEK_END) unsupported on /vsistdin");
            return -1;
        }

        if( gbHasSeekedToEnd )
        {
            m_nCurOff = gnFileSize;
            return 0;
        }

        nOffset = static_cast<vsi_l_offset>(-1);
    }
    else if( nWhence == SEEK_CUR )
    {
        nOffset += m_nCurOff;
    }

    if( nWhence != SEEK_END && gnRealPos >= gnBufferLimit && nOffset >= gnBufferLimit )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Backward Seek() unsupported on /vsistdin beyond "
                 "maximum buffer limit (" CPL_FRMT_GUIB " bytes).\n"
                 "This limit can be extended by setting the CPL_VSISTDIN_BUFFER_LIMIT "
                 "configuration option to a number of bytes, or by using the "
                 "'/vsistdin?buffer_limit=number_of_bytes' filename.\n"
                 "A limit of -1 means unlimited.",
                 static_cast<GUIntBig>(gnBufferLimit));
        return -1;
    }

    if( nOffset < gnBufferLen )
    {
        m_nCurOff = nOffset;
        return 0;
    }

    if( nOffset == m_nCurOff )
        return 0;

    CPLDebug("VSI", "Forward seek from " CPL_FRMT_GUIB " to " CPL_FRMT_GUIB,
             static_cast<GUIntBig>(m_nCurOff), nOffset);

    char abyTemp[8192] = {};
    m_nCurOff = gnRealPos;
    while( true )
    {
        const size_t nToRead = static_cast<size_t>(std::min(
            static_cast<uint64_t>(sizeof(abyTemp)),
            static_cast<uint64_t>(nOffset - m_nCurOff)));
        const size_t nRead = ReadAndCache(abyTemp, nToRead);

        if( nRead < nToRead )
        {
            return nWhence == SEEK_END ? 0 : -1;
        }
        if( nToRead < sizeof(abyTemp) )
            break;
    }

    return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIStdinHandle::Tell()
{
    return m_nCurOff;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIStdinHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    VSIStdinInit();

    const size_t nBytesToRead = nSize * nCount;
    if( nBytesToRead == 0 )
        return 0;

    if( m_nCurOff < gnRealPos &&
        gnRealPos >= gnBufferLimit &&
        m_nCurOff + nBytesToRead > gnBufferLimit )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Backward Seek() unsupported on /vsistdin beyond "
                 "maximum buffer limit (" CPL_FRMT_GUIB " bytes).\n"
                 "This limit can be extended by setting the CPL_VSISTDIN_BUFFER_LIMIT "
                 "configuration option to a number of bytes, or by using the "
                 "'/vsistdin?buffer_limit=number_of_bytes' filename.\n"
                 "A limit of -1 means unlimited.",
                 static_cast<GUIntBig>(gnBufferLimit));
        return 0;
    }

    if( m_nCurOff < gnBufferLen )
    {
        const size_t nAlreadyCached = static_cast<size_t>(gnBufferLen - m_nCurOff);
        if( nBytesToRead <= nAlreadyCached )
        {
            memcpy(pBuffer, gpabyBuffer + static_cast<size_t>(m_nCurOff), nBytesToRead);
            m_nCurOff += nBytesToRead;
            return nCount;
        }

        memcpy(pBuffer, gpabyBuffer + static_cast<size_t>(m_nCurOff), nAlreadyCached);
        m_nCurOff += nAlreadyCached;

        const size_t nRead =
            ReadAndCache( static_cast<GByte *>(pBuffer) + nAlreadyCached,
                          nBytesToRead - nAlreadyCached );
        m_bEOF = nRead < nBytesToRead - nAlreadyCached;

        return (nRead + nAlreadyCached) / nSize;
    }

    const size_t nRead = ReadAndCache( pBuffer, nBytesToRead );
    m_bEOF = nRead < nBytesToRead;
    return nRead / nSize;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIStdinHandle::Write( const void * /* pBuffer */,
                              size_t /* nSize */,
                              size_t /* nCount */ )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Write() unsupported on /vsistdin");
    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIStdinHandle::Eof()

{
    return m_bEOF;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIStdinHandle::Close()

{
    if( !gosStdinFilename.empty() &&
        CPLTestBool(CPLGetConfigOption("CPL_VSISTDIN_FILE_CLOSE", "NO")) )
    {
        if( gStdinFile != stdin )
            fclose(gStdinFile);
        gStdinFile = stdin;
        gosStdinFilename.clear();
        gnRealPos = ftell(stdin);
        gnBufferLen = 0;
        gbHasSeekedToEnd = false;
        gnFileSize = 0;
    }
    return 0;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIStdinFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VSIStdinFilesystemHandler()                   */
/************************************************************************/

VSIStdinFilesystemHandler::VSIStdinFilesystemHandler()
{
}

/************************************************************************/
/*                       ~VSIStdinFilesystemHandler()                   */
/************************************************************************/

VSIStdinFilesystemHandler::~VSIStdinFilesystemHandler()
{
    if( gStdinFile != stdin )
        fclose(gStdinFile);
    gStdinFile = stdin;
    CPLFree(gpabyBuffer);
    gpabyBuffer = nullptr;
    gnBufferLimit = 0;
    gnBufferAlloc = 0;
    gnBufferLen = 0;
    gnRealPos = 0;
    gosStdinFilename.clear();
}

/************************************************************************/
/*                           GetBufferLimit()                           */
/************************************************************************/

static size_t GetBufferLimit(const char* pszBufferLimit)
{
    uint64_t nVal = static_cast<uint64_t>(
                        std::strtoull(pszBufferLimit, nullptr, 10));

    // -1 because on 64-bit builds with size_t==uint64_t, a static analyzer
    // could complain that the ending nVal > MAX_BUFFER_LIMIT test is always
    // false
    constexpr size_t MAX_BUFFER_LIMIT = std::numeric_limits<size_t>::max()-1;
    if( strstr(pszBufferLimit, "MB") != nullptr)
    {
        constexpr size_t ONE_MB = 1024 * 1024;
        if( nVal > MAX_BUFFER_LIMIT / ONE_MB )
        {
            nVal = MAX_BUFFER_LIMIT;
        }
        else
        {
            nVal *= ONE_MB;
        }
    }
    else if( strstr(pszBufferLimit, "GB") != nullptr )
    {
        constexpr size_t ONE_GB = 1024 * 1024 * 1024;
        if( nVal > MAX_BUFFER_LIMIT / ONE_GB )
        {
            nVal = MAX_BUFFER_LIMIT;
        }
        else
        {
            nVal *= ONE_GB;
        }
    }
    if( nVal > MAX_BUFFER_LIMIT )
    {
        nVal = MAX_BUFFER_LIMIT;
    }
    return static_cast<size_t>(nVal);
}

/************************************************************************/
/*                           ParseFilename()                            */
/************************************************************************/

static bool ParseFilename(const char* pszFilename)
{
    if( !(EQUAL(pszFilename, "/vsistdin/") ||
          ((STARTS_WITH(pszFilename, "/vsistdin/?") ||
            STARTS_WITH(pszFilename, "/vsistdin?")) && strchr(pszFilename, '.') == nullptr) ) )
    {
        return false;
    }

    if( !CPLTestBool(CPLGetConfigOption("CPL_ALLOW_VSISTDIN", "YES")) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "/vsistdin/ disabled. Set CPL_ALLOW_VSISTDIN to YES to "
                "enable it");
        return false;
    }

    const char* pszBufferLimit =
        CPLGetConfigOption("CPL_VSISTDIN_BUFFER_LIMIT", "1048576");
    size_t nBufferLimit = GetBufferLimit(pszBufferLimit);

    pszFilename += strlen("/vsistdin/");
    if( *pszFilename == '?' )
        pszFilename ++;
    char** papszTokens = CSLTokenizeString2( pszFilename, "&", 0 );
    for( int i = 0; papszTokens[i] != nullptr; i++ )
    {
        char* pszUnescaped = CPLUnescapeString( papszTokens[i], nullptr,
                                                CPLES_URL );
        CPLFree(papszTokens[i]);
        papszTokens[i] = pszUnescaped;
    }

    for( int i = 0; papszTokens[i]; i++ )
    {
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(papszTokens[i], &pszKey);
        if( pszKey && pszValue )
        {
            if( EQUAL(pszKey, "buffer_limit") )
            {
                nBufferLimit = GetBufferLimit(pszValue);
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                            "Unsupported option: %s", pszKey);
            }
        }
        CPLFree(pszKey);
    }

    CSLDestroy(papszTokens);

    // For testing purposes
    const char* pszStdinFilename = CPLGetConfigOption("CPL_VSISTDIN_FILE", "stdin");
    if( EQUAL(pszStdinFilename, "stdin") )
    {
        if( !gosStdinFilename.empty() )
        {
            if( gStdinFile != stdin )
                fclose(gStdinFile);
            gStdinFile = stdin;
            gosStdinFilename.clear();
            gnRealPos = ftell(stdin);
            gnBufferLen = 0;
            gbHasSeekedToEnd = false;
            gnFileSize = 0;
        }
    }
    else
    {
        bool bReset = false;
        if( gosStdinFilename != pszStdinFilename )
        {
            if( gStdinFile != stdin )
                fclose(gStdinFile);
            gStdinFile = fopen(pszStdinFilename, "rb");
            if( gStdinFile == nullptr )
            {
                gStdinFile = stdin;
                return false;
            }
            gosStdinFilename = pszStdinFilename;
            bReset = true;
        }
        else
        {
            bReset = CPLTestBool(
                CPLGetConfigOption("CPL_VSISTDIN_RESET_POSITION", "NO"));
        }
        if( bReset )
        {
            gnBufferLimit = 0;
            gnBufferLen = 0;
            gnRealPos = 0;
            gbHasSeekedToEnd = false;
            gnFileSize = 0;
        }
    }

    gnBufferLimit = std::max(gnBufferLimit, nBufferLimit);

    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIStdinFilesystemHandler::Open( const char *pszFilename,
                                 const char *pszAccess,
                                 bool /* bSetError */,
                                 CSLConstList /* papszOptions */ )

{
    if( !ParseFilename(pszFilename) )
    {
        return nullptr;
    }

    if( strchr(pszAccess, 'w') != nullptr ||
        strchr(pszAccess, '+') != nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Write or update mode not supported on /vsistdin");
        return nullptr;
    }

    return new VSIStdinHandle();
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIStdinFilesystemHandler::Stat( const char * pszFilename,
                                     VSIStatBufL * pStatBuf,
                                     int nFlags )

{
    memset( pStatBuf, 0, sizeof(VSIStatBufL) );

    if( !ParseFilename(pszFilename) )
    {
        return -1;
    }

    if( nFlags & VSI_STAT_SIZE_FLAG )
    {
        if( gbHasSeekedToEnd )
            pStatBuf->st_size = gnFileSize;
        else
        {
            auto handle = Open(pszFilename, "rb", false, nullptr);
            if( handle == nullptr )
                return -1;
            handle->Seek(0, SEEK_END);
            pStatBuf->st_size = handle->Tell();
            delete handle;
        }
    }

    pStatBuf->st_mode = S_IFREG;
    return 0;
}

//! @endcond

/************************************************************************/
/*                       VSIInstallStdinHandler()                       */
/************************************************************************/

/*!
 \brief Install /vsistdin/ file system handler

 A special file handler is installed that allows reading from the standard
 input stream.

 The file operations available are of course limited to Read() and
 forward Seek() (full seek in the first MB of a file by default).

 Starting with GDAL 3.6, this limit can be configured either by setting
 the CPL_VSISTDIN_BUFFER_LIMIT configuration option to a number of bytes
 (can be -1 for unlimited), or using the "/vsistdin?buffer_limit=value"
 filename.

 \verbatim embed:rst
 See :ref:`/vsistdin/ documentation <vsistdin>`
 \endverbatim

 @since GDAL 1.8.0
 */
void VSIInstallStdinHandler()

{
    auto poHandler = new VSIStdinFilesystemHandler;
    VSIFileManager::InstallHandler("/vsistdin/", poHandler);
    VSIFileManager::InstallHandler("/vsistdin?", poHandler);
}
