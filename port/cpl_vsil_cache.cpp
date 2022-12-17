/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of caching IO layer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_vsi_virtual.h"

#include <cstddef>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "cpl_mem_cache.h"
#include "cpl_noncopyablevector.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/* ==================================================================== */
/*                             VSICachedFile                            */
/* ==================================================================== */
/************************************************************************/

class VSICachedFile final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSICachedFile)

  public:
    VSICachedFile(VSIVirtualHandle *poBaseHandle, size_t nChunkSize,
                  size_t nCacheSize);
    ~VSICachedFile() override
    {
        VSICachedFile::Close();
    }

    bool LoadBlocks(vsi_l_offset nStartBlock, size_t nBlockCount, void *pBuffer,
                    size_t nBufferSize);

    std::unique_ptr<VSIVirtualHandle> m_poBase{};

    vsi_l_offset m_nOffset = 0;
    vsi_l_offset m_nFileSize = 0;

    size_t m_nChunkSize = 0;
    lru11::Cache<vsi_l_offset, cpl::NonCopyableVector<GByte>>
        m_oCache;  // can only been initialized in constructor

    bool m_bEOF = false;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    int ReadMultiRange(int nRanges, void **ppData,
                       const vsi_l_offset *panOffsets,
                       const size_t *panSizes) override;

    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
    void *GetNativeFileDescriptor() override
    {
        return m_poBase->GetNativeFileDescriptor();
    }

    bool HasPRead() const override
    {
        return m_poBase->HasPRead();
    }
    size_t PRead(void *pBuffer, size_t nSize,
                 vsi_l_offset nOffset) const override
    {
        return m_poBase->PRead(pBuffer, nSize, nOffset);
    }
};

/************************************************************************/
/*                           GetCacheMax()                              */
/************************************************************************/

static size_t GetCacheMax(size_t nCacheSize)
{
    return nCacheSize ? nCacheSize
                      : static_cast<size_t>(std::min(
                            static_cast<GUIntBig>(
                                std::numeric_limits<size_t>::max() / 2),
                            CPLScanUIntBig(CPLGetConfigOption("VSI_CACHE_SIZE",
                                                              "25000000"),
                                           40)));
}

/************************************************************************/
/*                           DIV_ROUND_UP()                             */
/************************************************************************/

template <class T> inline T DIV_ROUND_UP(T a, T b)
{
    return a / b + (((a % b) == 0) ? 0 : 1);
}

/************************************************************************/
/*                           VSICachedFile()                            */
/************************************************************************/

VSICachedFile::VSICachedFile(VSIVirtualHandle *poBaseHandle, size_t nChunkSize,
                             size_t nCacheSize)
    : m_poBase(poBaseHandle), m_nChunkSize(nChunkSize ? nChunkSize : 32768),
      m_oCache{DIV_ROUND_UP(GetCacheMax(nCacheSize), m_nChunkSize), 0}
{
    m_poBase->Seek(0, SEEK_END);
    m_nFileSize = m_poBase->Tell();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSICachedFile::Close()

{
    m_oCache.clear();

    if (m_poBase)
    {
        m_poBase->Close();
        m_poBase.reset();
    }

    return 0;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICachedFile::Seek(vsi_l_offset nReqOffset, int nWhence)

{
    m_bEOF = false;

    if (nWhence == SEEK_SET)
    {
        // Use offset directly.
    }
    else if (nWhence == SEEK_CUR)
    {
        nReqOffset += m_nOffset;
    }
    else if (nWhence == SEEK_END)
    {
        nReqOffset += m_nFileSize;
    }

    m_nOffset = nReqOffset;

    return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSICachedFile::Tell()

{
    return m_nOffset;
}

/************************************************************************/
/*                             LoadBlocks()                             */
/*                                                                      */
/*      Load the desired set of blocks.  Use pBuffer as a temporary     */
/*      buffer if it would be helpful.                                  */
/************************************************************************/

bool VSICachedFile::LoadBlocks(vsi_l_offset nStartBlock, size_t nBlockCount,
                               void *pBuffer, size_t nBufferSize)

{
    if (nBlockCount == 0)
        return true;

    /* -------------------------------------------------------------------- */
    /*      When we want to load only one block, we can directly load it    */
    /*      into the target buffer with no concern about intermediaries.    */
    /* -------------------------------------------------------------------- */
    if (nBlockCount == 1)
    {
        if (m_poBase->Seek(static_cast<vsi_l_offset>(nStartBlock) *
                               m_nChunkSize,
                           SEEK_SET) != 0)
        {
            return false;
        }

        try
        {
            cpl::NonCopyableVector<GByte> oData(m_nChunkSize);
            const auto nDataRead =
                m_poBase->Read(oData.data(), 1, m_nChunkSize);
            if (nDataRead == 0)
                return false;
            oData.resize(nDataRead);

            m_oCache.insert(nStartBlock, std::move(oData));
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory situation in VSICachedFile::LoadBlocks()");
            return false;
        }

        return true;
    }

    /* -------------------------------------------------------------------- */
    /*      If the buffer is quite large but not quite large enough to      */
    /*      hold all the blocks we will take the pain of splitting the      */
    /*      io request in two in order to avoid allocating a large          */
    /*      temporary buffer.                                               */
    /* -------------------------------------------------------------------- */
    if (nBufferSize > m_nChunkSize * 20 &&
        nBufferSize < nBlockCount * m_nChunkSize)
    {
        if (!LoadBlocks(nStartBlock, 2, pBuffer, nBufferSize))
            return false;

        return LoadBlocks(nStartBlock + 2, nBlockCount - 2, pBuffer,
                          nBufferSize);
    }

    if (m_poBase->Seek(static_cast<vsi_l_offset>(nStartBlock) * m_nChunkSize,
                       SEEK_SET) != 0)
        return false;

    /* -------------------------------------------------------------------- */
    /*      Do we need to allocate our own buffer?                          */
    /* -------------------------------------------------------------------- */
    GByte *pabyWorkBuffer = static_cast<GByte *>(pBuffer);

    if (nBufferSize < m_nChunkSize * nBlockCount)
    {
        pabyWorkBuffer = static_cast<GByte *>(
            VSI_MALLOC_VERBOSE(m_nChunkSize * nBlockCount));
        if (pabyWorkBuffer == nullptr)
            return false;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the whole request into the working buffer.                 */
    /* -------------------------------------------------------------------- */

    const size_t nDataRead =
        m_poBase->Read(pabyWorkBuffer, 1, nBlockCount * m_nChunkSize);

    if (nBlockCount * m_nChunkSize > nDataRead + m_nChunkSize - 1)
        nBlockCount = (nDataRead + m_nChunkSize - 1) / m_nChunkSize;

    bool ret = true;
    for (size_t i = 0; i < nBlockCount; i++)
    {
        const vsi_l_offset iBlock = nStartBlock + i;

        const auto nDataFilled = (nDataRead >= (i + 1) * m_nChunkSize)
                                     ? m_nChunkSize
                                     : nDataRead - i * m_nChunkSize;
        try
        {
            cpl::NonCopyableVector<GByte> oData(nDataFilled);

            memcpy(oData.data(), pabyWorkBuffer + i * m_nChunkSize,
                   nDataFilled);

            m_oCache.insert(iBlock, std::move(oData));
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory situation in VSICachedFile::LoadBlocks()");
            ret = false;
            break;
        }
    }

    if (pabyWorkBuffer != pBuffer)
        CPLFree(pabyWorkBuffer);

    return ret;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSICachedFile::Read(void *pBuffer, size_t nSize, size_t nCount)

{
    if (nSize == 0 || nCount == 0)
        return 0;
    const size_t nRequestedBytes = nSize * nCount;

    // nFileSize might be set wrongly to 0 by underlying layers, such as
    // /vsicurl_streaming/https://query.data.world/s/jgsghstpphjhicstradhy5kpjwrnfy
    if (m_nFileSize > 0 && m_nOffset >= m_nFileSize)
    {
        m_bEOF = true;
        return 0;
    }

    /* ==================================================================== */
    /*      Make sure the cache is loaded for the whole request region.     */
    /* ==================================================================== */
    const vsi_l_offset nStartBlock = m_nOffset / m_nChunkSize;
    const vsi_l_offset nEndBlock =
        (m_nOffset + nRequestedBytes - 1) / m_nChunkSize;

    for (vsi_l_offset iBlock = nStartBlock; iBlock <= nEndBlock; iBlock++)
    {
        if (!m_oCache.contains(iBlock))
        {
            size_t nBlocksToLoad = 1;
            while (iBlock + nBlocksToLoad <= nEndBlock &&
                   !m_oCache.contains(iBlock + nBlocksToLoad))
            {
                nBlocksToLoad++;
            }

            LoadBlocks(iBlock, nBlocksToLoad, pBuffer, nRequestedBytes);
        }
    }

    /* ==================================================================== */
    /*      Copy data into the target buffer to the extent possible.        */
    /* ==================================================================== */
    size_t nAmountCopied = 0;

    while (nAmountCopied < nRequestedBytes)
    {
        const vsi_l_offset iBlock = (m_nOffset + nAmountCopied) / m_nChunkSize;
        const cpl::NonCopyableVector<GByte> *poData = m_oCache.getPtr(iBlock);
        if (poData == nullptr)
        {
            // We can reach that point when the amount to read exceeds
            // the cache size.
            LoadBlocks(iBlock, 1, static_cast<GByte *>(pBuffer) + nAmountCopied,
                       std::min(nRequestedBytes - nAmountCopied, m_nChunkSize));
            poData = m_oCache.getPtr(iBlock);
            if (poData == nullptr)
            {
                break;
            }
        }

        const vsi_l_offset nStartOffset =
            static_cast<vsi_l_offset>(iBlock) * m_nChunkSize;
        if (nStartOffset + poData->size() < nAmountCopied + m_nOffset)
            break;
        const size_t nThisCopy =
            std::min(nRequestedBytes - nAmountCopied,
                     static_cast<size_t>(((nStartOffset + poData->size()) -
                                          nAmountCopied - m_nOffset)));
        if (nThisCopy == 0)
            break;

        memcpy(static_cast<GByte *>(pBuffer) + nAmountCopied,
               poData->data() + (m_nOffset + nAmountCopied) - nStartOffset,
               nThisCopy);

        nAmountCopied += nThisCopy;
    }

    m_nOffset += nAmountCopied;

    const size_t nRet = nAmountCopied / nSize;
    if (nRet != nCount)
        m_bEOF = true;
    return nRet;
}

/************************************************************************/
/*                           ReadMultiRange()                           */
/************************************************************************/

int VSICachedFile::ReadMultiRange(int const nRanges, void **const ppData,
                                  const vsi_l_offset *const panOffsets,
                                  const size_t *const panSizes)
{
    // If the base is /vsicurl/
    return m_poBase->ReadMultiRange(nRanges, ppData, panOffsets, panSizes);
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSICachedFile::Write(const void * /* pBuffer */, size_t /*nSize */,
                            size_t /* nCount */)
{
    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSICachedFile::Eof()

{
    return m_bEOF;
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSICachedFile::Flush()

{
    return 0;
}

//! @endcond

/************************************************************************/
/*                        VSICreateCachedFile()                         */
/************************************************************************/

VSIVirtualHandle *VSICreateCachedFile(VSIVirtualHandle *poBaseHandle,
                                      size_t nChunkSize, size_t nCacheSize)

{
    return new VSICachedFile(poBaseHandle, nChunkSize, nCacheSize);
}
