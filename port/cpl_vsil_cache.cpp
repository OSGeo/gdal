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

    VSIVirtualHandleUniquePtr m_poBase{};

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

    void AdviseRead(int nRanges, const vsi_l_offset *panOffsets,
                    const size_t *panSizes) override
    {
        m_poBase->AdviseRead(nRanges, panOffsets, panSizes);
    }

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
    : m_poBase(poBaseHandle),
      m_nChunkSize(nChunkSize ? nChunkSize : VSI_CACHED_DEFAULT_CHUNK_SIZE),
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
    m_poBase.reset();

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

    bool ret = true;
    if (nBlockCount * m_nChunkSize > nDataRead + m_nChunkSize - 1)
    {
        size_t nNewBlockCount = (nDataRead + m_nChunkSize - 1) / m_nChunkSize;
        if (nNewBlockCount < nBlockCount)
        {
            nBlockCount = nNewBlockCount;
            ret = false;
        }
    }

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
    // Calculate last block
    const vsi_l_offset nLastBlock = m_nFileSize / m_nChunkSize;
    vsi_l_offset nEndBlock = (m_nOffset + nRequestedBytes - 1) / m_nChunkSize;

    // if nLastBlock is not 0 consider the min value to avoid out-of-range reads
    if (nLastBlock != 0 && nEndBlock > nLastBlock)
    {
        nEndBlock = nLastBlock;
    }

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

            if (!LoadBlocks(iBlock, nBlocksToLoad, pBuffer, nRequestedBytes))
                break;
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

/************************************************************************/
/*                      VSICachedFilesystemHandler                      */
/************************************************************************/

class VSICachedFilesystemHandler final : public VSIFilesystemHandler
{
    static bool AnalyzeFilename(const char *pszFilename,
                                std::string &osUnderlyingFilename,
                                size_t &nChunkSize, size_t &nCacheSize);

  public:
    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError, CSLConstList papszOptions) override;
    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;
};

/************************************************************************/
/*                               ParseSize()                            */
/************************************************************************/

static bool ParseSize(const char *pszKey, const char *pszValue, size_t nMaxVal,
                      size_t &nOutVal)
{
    char *end = nullptr;
    auto nVal = std::strtoull(pszValue, &end, 10);
    if (!end || end == pszValue || nVal >= nMaxVal)
    {
        CPLError(
            CE_Failure, CPLE_IllegalArg,
            "Invalid value for %s: %s. Max supported value = " CPL_FRMT_GUIB,
            pszKey, pszValue, static_cast<GUIntBig>(nMaxVal));
        return false;
    }
    if (*end != '\0')
    {
        if (strcmp(end, "KB") == 0)
        {
            if (nVal > nMaxVal / 1024)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid value for %s: %s. Max supported value "
                         "= " CPL_FRMT_GUIB,
                         pszKey, pszValue, static_cast<GUIntBig>(nMaxVal));
                return false;
            }
            nVal *= 1024;
        }
        else if (strcmp(end, "MB") == 0)
        {
            if (nVal > nMaxVal / (1024 * 1024))
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid value for %s: %s. Max supported value "
                         "= " CPL_FRMT_GUIB,
                         pszKey, pszValue, static_cast<GUIntBig>(nMaxVal));
                return false;
            }
            nVal *= (1024 * 1024);
        }
        else
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid value for %s: %s",
                     pszKey, pszValue);
            return false;
        }
    }
    nOutVal = static_cast<size_t>(nVal);
    return true;
}

/************************************************************************/
/*                          AnalyzeFilename()                           */
/************************************************************************/

bool VSICachedFilesystemHandler::AnalyzeFilename(
    const char *pszFilename, std::string &osUnderlyingFilename,
    size_t &nChunkSize, size_t &nCacheSize)
{

    if (!STARTS_WITH(pszFilename, "/vsicached?"))
        return false;

    const CPLStringList aosTokens(
        CSLTokenizeString2(pszFilename + strlen("/vsicached?"), "&", 0));

    osUnderlyingFilename.clear();
    nChunkSize = 0;
    nCacheSize = 0;

    for (int i = 0; i < aosTokens.size(); ++i)
    {
        char *pszUnescaped =
            CPLUnescapeString(aosTokens[i], nullptr, CPLES_URL);
        std::string osUnescaped(pszUnescaped);
        CPLFree(pszUnescaped);
        char *pszKey = nullptr;
        const char *pszValue = CPLParseNameValue(osUnescaped.c_str(), &pszKey);
        if (pszKey && pszValue)
        {
            if (strcmp(pszKey, "file") == 0)
            {
                osUnderlyingFilename = pszValue;
            }
            else if (strcmp(pszKey, "chunk_size") == 0)
            {
                if (!ParseSize(pszKey, pszValue, 1024 * 1024 * 1024,
                               nChunkSize))
                {
                    CPLFree(pszKey);
                    return false;
                }
            }
            else if (strcmp(pszKey, "cache_size") == 0)
            {
                if (!ParseSize(pszKey, pszValue,
                               std::numeric_limits<size_t>::max(), nCacheSize))
                {
                    CPLFree(pszKey);
                    return false;
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Unsupported option: %s", pszKey);
            }
        }
        CPLFree(pszKey);
    }

    if (osUnderlyingFilename.empty())
    {
        CPLError(CE_Warning, CPLE_NotSupported, "Missing 'file' option");
    }

    return !osUnderlyingFilename.empty();
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

VSIVirtualHandle *VSICachedFilesystemHandler::Open(const char *pszFilename,
                                                   const char *pszAccess,
                                                   bool bSetError,
                                                   CSLConstList papszOptions)
{
    std::string osUnderlyingFilename;
    size_t nChunkSize = 0;
    size_t nCacheSize = 0;
    if (!AnalyzeFilename(pszFilename, osUnderlyingFilename, nChunkSize,
                         nCacheSize))
        return nullptr;
    if (strcmp(pszAccess, "r") != 0 && strcmp(pszAccess, "rb") != 0)
    {
        if (bSetError)
        {
            VSIError(VSIE_FileError,
                     "/vsicached? supports only 'r' and 'rb' access modes");
        }
        return nullptr;
    }

    auto fp = VSIFOpenEx2L(osUnderlyingFilename.c_str(), pszAccess, bSetError,
                           papszOptions);
    if (!fp)
        return nullptr;
    return VSICreateCachedFile(fp, nChunkSize, nCacheSize);
}

/************************************************************************/
/*                               Stat()                                 */
/************************************************************************/

int VSICachedFilesystemHandler::Stat(const char *pszFilename,
                                     VSIStatBufL *pStatBuf, int nFlags)
{
    std::string osUnderlyingFilename;
    size_t nChunkSize = 0;
    size_t nCacheSize = 0;
    if (!AnalyzeFilename(pszFilename, osUnderlyingFilename, nChunkSize,
                         nCacheSize))
        return -1;
    return VSIStatExL(osUnderlyingFilename.c_str(), pStatBuf, nFlags);
}

/************************************************************************/
/*                          ReadDirEx()                                 */
/************************************************************************/

char **VSICachedFilesystemHandler::ReadDirEx(const char *pszDirname,
                                             int nMaxFiles)
{
    std::string osUnderlyingFilename;
    size_t nChunkSize = 0;
    size_t nCacheSize = 0;
    if (!AnalyzeFilename(pszDirname, osUnderlyingFilename, nChunkSize,
                         nCacheSize))
        return nullptr;
    return VSIReadDirEx(osUnderlyingFilename.c_str(), nMaxFiles);
}

//! @endcond

/************************************************************************/
/*                        VSICreateCachedFile()                         */
/************************************************************************/

/** Wraps a file handle in another one, which has caching for read-operations.
 *
 * This takes a virtual file handle and returns a new handle that caches
 * read-operations on the input file handle. The cache is RAM based and
 * the content of the cache is discarded when the file handle is closed.
 * The cache is a least-recently used lists of blocks of 32KB each.
 *
 * @param poBaseHandle base handle
 * @param nChunkSize chunk size, in bytes. If 0, defaults to 32 KB
 * @param nCacheSize total size of the cache for the file, in bytes.
 *                   If 0, defaults to the value of the VSI_CACHE_SIZE
 *                   configuration option, which defaults to 25 MB.
 * @return a new handle
 */
VSIVirtualHandle *VSICreateCachedFile(VSIVirtualHandle *poBaseHandle,
                                      size_t nChunkSize, size_t nCacheSize)

{
    return new VSICachedFile(poBaseHandle, nChunkSize, nCacheSize);
}

/************************************************************************/
/*                   VSIInstallCachedFileHandler()                      */
/************************************************************************/

/*!
 \brief Install /vsicached? file system handler

 \verbatim embed:rst
 See :ref:`/vsicached? documentation <vsicached>`
 \endverbatim

 @since GDAL 3.8.0
 */
void VSIInstallCachedFileHandler(void)
{
    VSIFilesystemHandler *poHandler = new VSICachedFilesystemHandler;
    VSIFileManager::InstallHandler("/vsicached?", poHandler);
}
