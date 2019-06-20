/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of buffered reader IO functions.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2011, Even Rouault <even dot rouault at spatialys.com>
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

//! @cond Doxygen_Suppress

// The intent of this class is to be a wrapper around an underlying virtual
// handle and add very basic caching of last read bytes, so that a backward
// seek of a few bytes doesn't require a seek on the underlying virtual handle.
// This enable us to improve dramatically the performance of CPLReadLine2L() on
// a gzip file.

#include "cpl_port.h"
#include "cpl_vsi_virtual.h"

#include <cstddef>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <algorithm>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

constexpr int MAX_BUFFER_SIZE = 65536;

CPL_CVSID("$Id$")

class VSIBufferedReaderHandle final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIBufferedReaderHandle)

    VSIVirtualHandle* m_poBaseHandle = nullptr;
    GByte*            pabyBuffer = nullptr;
    GUIntBig          nBufferOffset = 0;
    int               nBufferSize = 0;
    GUIntBig          nCurOffset = 0;
    bool              bNeedBaseHandleSeek = false;
    bool              bEOF =  false;
    vsi_l_offset      nCheatFileSize = 0;

    int               SeekBaseTo( vsi_l_offset nTargetOffset );

  public:
    explicit VSIBufferedReaderHandle( VSIVirtualHandle* poBaseHandle );
    VSIBufferedReaderHandle( VSIVirtualHandle* poBaseHandle,
                             const GByte* pabyBeginningContent,
                             vsi_l_offset nCheatFileSizeIn );
    // TODO(schwehr): Add override when support dropped for VS2008.
    ~VSIBufferedReaderHandle() override;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize,
                 size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize,
                  size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
};

//! @endcond

/************************************************************************/
/*                    VSICreateBufferedReaderHandle()                   */
/************************************************************************/

VSIVirtualHandle *
VSICreateBufferedReaderHandle( VSIVirtualHandle* poBaseHandle )
{
    return new VSIBufferedReaderHandle(poBaseHandle);
}

VSIVirtualHandle* VSICreateBufferedReaderHandle(
    VSIVirtualHandle* poBaseHandle,
    const GByte* pabyBeginningContent,
    vsi_l_offset nCheatFileSizeIn )
{
    return new VSIBufferedReaderHandle(poBaseHandle,
                                       pabyBeginningContent,
                                       nCheatFileSizeIn);
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        VSIBufferedReaderHandle()                     */
/************************************************************************/

VSIBufferedReaderHandle::VSIBufferedReaderHandle(
    VSIVirtualHandle* poBaseHandle) :
    m_poBaseHandle(poBaseHandle),
    pabyBuffer(static_cast<GByte*>(CPLMalloc(MAX_BUFFER_SIZE)))
{}

VSIBufferedReaderHandle::VSIBufferedReaderHandle(
    VSIVirtualHandle* poBaseHandle,
    const GByte* pabyBeginningContent,
    vsi_l_offset nCheatFileSizeIn ) :
    m_poBaseHandle(poBaseHandle),
    pabyBuffer(static_cast<GByte *>(
        CPLMalloc(std::max(MAX_BUFFER_SIZE,
                           static_cast<int>(poBaseHandle->Tell()))))),
    nBufferOffset(0),
    nBufferSize(static_cast<int>(poBaseHandle->Tell())),
    nCurOffset(0),
    bNeedBaseHandleSeek(true),
    bEOF(false),
    nCheatFileSize(nCheatFileSizeIn)
{
    memcpy(pabyBuffer, pabyBeginningContent, nBufferSize);
}

/************************************************************************/
/*                        ~VSIBufferedReaderHandle()                    */
/************************************************************************/

VSIBufferedReaderHandle::~VSIBufferedReaderHandle()
{
    delete m_poBaseHandle;
    CPLFree(pabyBuffer);
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIBufferedReaderHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
#ifdef DEBUG_VERBOSE
    CPLDebug( "BUFFERED", "Seek(%d,%d)",
              static_cast<int>(nOffset), static_cast<int>(nWhence) );
#endif
    bEOF = false;
    int ret = 0;
    if( nWhence == SEEK_CUR )
    {
        nCurOffset += nOffset;
    }
    else if( nWhence == SEEK_END )
    {
        if( nCheatFileSize )
        {
            nCurOffset = nCheatFileSize;
        }
        else
        {
            ret = m_poBaseHandle->Seek(nOffset, nWhence);
            nCurOffset = m_poBaseHandle->Tell();
            bNeedBaseHandleSeek = true;
        }
    }
    else
    {
        nCurOffset = nOffset;
    }

    return ret;
}

/************************************************************************/
/*                               Tell()                                 */
/************************************************************************/

vsi_l_offset VSIBufferedReaderHandle::Tell()
{
#ifdef DEBUG_VERBOSE
    CPLDebug( "BUFFERED", "Tell() = %d", static_cast<int>(nCurOffset));
#endif
    return nCurOffset;
}

/************************************************************************/
/*                           SeekBaseTo()                               */
/************************************************************************/

int VSIBufferedReaderHandle::SeekBaseTo( vsi_l_offset nTargetOffset )
{
    if( m_poBaseHandle->Seek(nTargetOffset, SEEK_SET) == 0 )
        return TRUE;

    nCurOffset = m_poBaseHandle->Tell();
    if( nCurOffset > nTargetOffset )
        return FALSE;

    const vsi_l_offset nMaxOffset = 8192;

    std::vector<char> oTemp(nMaxOffset, 0);
    char *pabyTemp = &oTemp[0];

    while( true )
    {
        const size_t nToRead = static_cast<size_t>(
            std::min(nMaxOffset, nTargetOffset - nCurOffset));
        const size_t nRead = m_poBaseHandle->Read(pabyTemp, 1, nToRead);

        nCurOffset += nRead;

        if( nRead < nToRead )
        {
            bEOF = true;
            return FALSE;
        }
        if( nToRead < nMaxOffset )
            break;
    }
    return TRUE;
}

/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

size_t VSIBufferedReaderHandle::Read( void *pBuffer, size_t nSize,
                                      size_t nMemb )
{
    const size_t nTotalToRead = nSize * nMemb;
#ifdef DEBUG_VERBOSE
    CPLDebug( "BUFFERED", "Read(%d)", static_cast<int>(nTotalToRead));
#endif

    if( nSize == 0 )
        return 0;

    if( nBufferSize != 0 &&
        nCurOffset >= nBufferOffset &&
        nCurOffset <= nBufferOffset + nBufferSize )
    {
        // We try to read from an offset located within the buffer.
        const size_t nReadInBuffer =
            static_cast<size_t>(
                std::min(nTotalToRead,
                         static_cast<size_t>(nBufferOffset + nBufferSize -
                                             nCurOffset)));
        memcpy(pBuffer, pabyBuffer + nCurOffset - nBufferOffset, nReadInBuffer);
        const size_t nToReadInFile = nTotalToRead - nReadInBuffer;
        if( nToReadInFile > 0 )
        {
            // The beginning of the data to read is located in the buffer
            // but the end must be read from the file.
            if( bNeedBaseHandleSeek )
            {
                if( !SeekBaseTo(nBufferOffset + nBufferSize) )
                {
                    nCurOffset += nReadInBuffer;
                    return nReadInBuffer / nSize;
                }
            }
            bNeedBaseHandleSeek = false;
#ifdef DEBUG_VERBOSE
            CPLAssert(m_poBaseHandle->Tell() == nBufferOffset + nBufferSize);
#endif

            const size_t nReadInFile =
                m_poBaseHandle->Read(
                    static_cast<GByte *>(pBuffer) + nReadInBuffer,
                    1, nToReadInFile);
            const size_t nRead = nReadInBuffer + nReadInFile;

            nBufferSize = static_cast<int>(
                std::min(nRead, static_cast<size_t>(MAX_BUFFER_SIZE)));
            nBufferOffset = nCurOffset + nRead - nBufferSize;
            memcpy(pabyBuffer,
                   static_cast<GByte *>(pBuffer) + nRead - nBufferSize,
                   nBufferSize);

            nCurOffset += nRead;
#ifdef DEBUG_VERBOSE
            CPLAssert(m_poBaseHandle->Tell() == nBufferOffset + nBufferSize);
            CPLAssert(m_poBaseHandle->Tell() == nCurOffset);
#endif

            bEOF = CPL_TO_BOOL(m_poBaseHandle->Eof());

            return nRead / nSize;
        }
        else
        {
            // The data to read is completely located within the buffer.
            nCurOffset += nTotalToRead;
            return nTotalToRead / nSize;
        }
    }
    else
    {
        // We try either to read before or after the buffer, so a seek is
        // necessary.
        if( !SeekBaseTo(nCurOffset) )
            return 0;
        bNeedBaseHandleSeek = false;
        const size_t nReadInFile =
            m_poBaseHandle->Read(pBuffer, 1, nTotalToRead);
        nBufferSize = static_cast<int>(
            std::min(nReadInFile, static_cast<size_t>(MAX_BUFFER_SIZE)));
        nBufferOffset = nCurOffset + nReadInFile - nBufferSize;
        memcpy(pabyBuffer,
               static_cast<GByte *>(pBuffer) + nReadInFile - nBufferSize,
               nBufferSize);

        nCurOffset += nReadInFile;
#ifdef DEBUG_VERBOSE
        CPLAssert(m_poBaseHandle->Tell() == nBufferOffset + nBufferSize);
        CPLAssert(m_poBaseHandle->Tell() == nCurOffset);
#endif

        bEOF = CPL_TO_BOOL(m_poBaseHandle->Eof());

        return nReadInFile / nSize;
    }
}

/************************************************************************/
/*                              Write()                                 */
/************************************************************************/

size_t VSIBufferedReaderHandle::Write( const void * /* pBuffer */,
                                       size_t /* nSize */,
                                       size_t /* nMemb */)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFWriteL is not supported on buffer reader streams");
    return 0;
}

/************************************************************************/
/*                               Eof()                                  */
/************************************************************************/

int VSIBufferedReaderHandle::Eof()
{
    return bEOF;
}

/************************************************************************/
/*                              Flush()                                 */
/************************************************************************/

int VSIBufferedReaderHandle::Flush()
{
    return 0;
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

int VSIBufferedReaderHandle::Close()
{
    if( m_poBaseHandle )
    {
        m_poBaseHandle->Close();
        delete m_poBaseHandle;
        m_poBaseHandle = nullptr;
    }
    return 0;
}

//! @endcond
