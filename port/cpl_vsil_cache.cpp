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
#include <map>
#include <memory>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"

//! @cond Doxygen_Suppress

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                             VSICacheChunk                            */
/* ==================================================================== */
/************************************************************************/

class VSICacheChunk
{
    CPL_DISALLOW_COPY_ASSIGN(VSICacheChunk)

public:
    VSICacheChunk() = default;

    virtual ~VSICacheChunk()
    {
        VSIFree( pabyData );
    }

    bool Allocate( size_t nChunkSize )
    {
        CPLAssert( pabyData == nullptr );
        pabyData = static_cast<GByte *>(VSIMalloc( nChunkSize ));
        return (pabyData != nullptr);
    }

    vsi_l_offset   iBlock = 0;

    VSICacheChunk *poLRUPrev = nullptr;
    VSICacheChunk *poLRUNext = nullptr;

    size_t          nDataFilled = 0;
    GByte          *pabyData = nullptr;
};

/************************************************************************/
/* ==================================================================== */
/*                             VSICachedFile                            */
/* ==================================================================== */
/************************************************************************/

class VSICachedFile final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSICachedFile)

  public:
    VSICachedFile( VSIVirtualHandle *poBaseHandle,
                   size_t nChunkSize,
                   size_t nCacheSize );
    ~VSICachedFile() override { VSICachedFile::Close(); }

    void          FlushLRU();
    int           LoadBlocks( vsi_l_offset nStartBlock, size_t nBlockCount,
                              void *pBuffer, size_t nBufferSize );
    void          Demote( VSICacheChunk * );

    VSIVirtualHandle *poBase = nullptr;

    vsi_l_offset  nOffset = 0;
    vsi_l_offset  nFileSize = 0;

    GUIntBig      nCacheUsed = 0;
    GUIntBig      nCacheMax = 0;

    size_t        m_nChunkSize = 0;

    VSICacheChunk *poLRUStart = nullptr;
    VSICacheChunk *poLRUEnd = nullptr;

    std::map<vsi_l_offset, std::unique_ptr<VSICacheChunk>> oMapOffsetToCache{};

    bool           bEOF = false;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize,
                 size_t nMemb ) override;
    int ReadMultiRange( int nRanges, void ** ppData,
                        const vsi_l_offset* panOffsets,
                        const size_t* panSizes ) override;

    size_t Write( const void *pBuffer, size_t nSize,
                  size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
    void *GetNativeFileDescriptor() override
        { return poBase->GetNativeFileDescriptor(); }
};

/************************************************************************/
/*                           VSICachedFile()                            */
/************************************************************************/

VSICachedFile::VSICachedFile( VSIVirtualHandle *poBaseHandle, size_t nChunkSize,
                              size_t nCacheSize ) :
    poBase(poBaseHandle),
    nCacheMax(nCacheSize),
    m_nChunkSize(nChunkSize)
{
    if( nCacheSize == 0 )
        nCacheMax = CPLScanUIntBig(
             CPLGetConfigOption( "VSI_CACHE_SIZE", "25000000" ), 40 );

    poBase->Seek( 0, SEEK_END );
    nFileSize = poBase->Tell();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSICachedFile::Close()

{
    oMapOffsetToCache.clear();

    poLRUStart = nullptr;
    poLRUEnd = nullptr;

    nCacheUsed = 0;

    if( poBase )
    {
        poBase->Close();
        delete poBase;
        poBase = nullptr;
    }

    return 0;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICachedFile::Seek( vsi_l_offset nReqOffset, int nWhence )

{
    bEOF = false;

    if( nWhence == SEEK_SET )
    {
        // Use offset directly.
    }
    else if( nWhence == SEEK_CUR )
    {
        nReqOffset += nOffset;
    }
    else if( nWhence == SEEK_END )
    {
        nReqOffset += nFileSize;
    }

    nOffset = nReqOffset;

    return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSICachedFile::Tell()

{
    return nOffset;
}

/************************************************************************/
/*                              FlushLRU()                              */
/************************************************************************/

void VSICachedFile::FlushLRU()

{
    CPLAssert( poLRUStart != nullptr );

    VSICacheChunk *poBlock = poLRUStart;

    CPLAssert( nCacheUsed >= poBlock->nDataFilled );

    nCacheUsed -= poBlock->nDataFilled;

    poLRUStart = poBlock->poLRUNext;
    if( poLRUEnd == poBlock )
        poLRUEnd = nullptr;

    if( poBlock->poLRUNext != nullptr )
        poBlock->poLRUNext->poLRUPrev = nullptr;

    oMapOffsetToCache.erase(oMapOffsetToCache.find(poBlock->iBlock));
}

/************************************************************************/
/*                               Demote()                               */
/*                                                                      */
/*      Demote the indicated block to the end of the LRU list.          */
/*      Potentially integrate the link into the list if it is not       */
/*      already there.                                                  */
/************************************************************************/

void VSICachedFile::Demote( VSICacheChunk *poBlock )

{
    // Already at end?
    if( poLRUEnd == poBlock )
        return;

    if( poLRUStart == poBlock )
        poLRUStart = poBlock->poLRUNext;

    if( poBlock->poLRUPrev != nullptr )
        poBlock->poLRUPrev->poLRUNext = poBlock->poLRUNext;

    if( poBlock->poLRUNext != nullptr )
        poBlock->poLRUNext->poLRUPrev = poBlock->poLRUPrev;

    poBlock->poLRUNext = nullptr;
    poBlock->poLRUPrev = nullptr;

    if( poLRUEnd != nullptr )
        poLRUEnd->poLRUNext = poBlock;

    poLRUEnd = poBlock;

    if( poLRUStart == nullptr )
        poLRUStart = poBlock;
}

/************************************************************************/
/*                             LoadBlocks()                             */
/*                                                                      */
/*      Load the desired set of blocks.  Use pBuffer as a temporary     */
/*      buffer if it would be helpful.                                  */
/*                                                                      */
/*  RETURNS: TRUE on success; FALSE on failure.                         */
/************************************************************************/

int VSICachedFile::LoadBlocks( vsi_l_offset nStartBlock, size_t nBlockCount,
                               void *pBuffer, size_t nBufferSize )

{
    if( nBlockCount == 0 )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      When we want to load only one block, we can directly load it    */
/*      into the target buffer with no concern about intermediaries.    */
/* -------------------------------------------------------------------- */
    if( nBlockCount == 1 )
    {
        if( poBase->Seek( static_cast<vsi_l_offset>(nStartBlock) * m_nChunkSize,
                          SEEK_SET ) != 0 )
        {
            return FALSE;
        }

        auto poBlock = cpl::make_unique<VSICacheChunk>();
        if( !poBlock || !poBlock->Allocate( m_nChunkSize ) )
        {
            return FALSE;
        }

        poBlock->iBlock = nStartBlock;
        poBlock->nDataFilled =
            poBase->Read( poBlock->pabyData, 1, m_nChunkSize );
        if( poBlock->nDataFilled == 0 )
            return FALSE;
        nCacheUsed += poBlock->nDataFilled;

        // Merges into the LRU list.
        Demote( poBlock.get() );

        oMapOffsetToCache[nStartBlock] = std::move(poBlock);

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      If the buffer is quite large but not quite large enough to      */
/*      hold all the blocks we will take the pain of splitting the      */
/*      io request in two in order to avoid allocating a large          */
/*      temporary buffer.                                               */
/* -------------------------------------------------------------------- */
    if( nBufferSize > m_nChunkSize * 20
        && nBufferSize < nBlockCount * m_nChunkSize )
    {
        if( !LoadBlocks( nStartBlock, 2, pBuffer, nBufferSize ) )
            return FALSE;

        return LoadBlocks( nStartBlock+2, nBlockCount-2, pBuffer, nBufferSize );
    }

    if( poBase->Seek( static_cast<vsi_l_offset>(nStartBlock) * m_nChunkSize,
                      SEEK_SET ) != 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do we need to allocate our own buffer?                          */
/* -------------------------------------------------------------------- */
    GByte *pabyWorkBuffer = static_cast<GByte *>(pBuffer);

    if( nBufferSize < m_nChunkSize * nBlockCount )
        pabyWorkBuffer =
            static_cast<GByte *>( CPLMalloc(m_nChunkSize * nBlockCount) );

/* -------------------------------------------------------------------- */
/*      Read the whole request into the working buffer.                 */
/* -------------------------------------------------------------------- */

    const size_t nDataRead =
        poBase->Read( pabyWorkBuffer, 1, nBlockCount*m_nChunkSize);

    if( nBlockCount * m_nChunkSize > nDataRead + m_nChunkSize - 1 )
        nBlockCount = (nDataRead + m_nChunkSize - 1) / m_nChunkSize;

    for( size_t i = 0; i < nBlockCount; i++ )
    {
        const vsi_l_offset iBlock = nStartBlock + i;

        auto poBlock = cpl::make_unique<VSICacheChunk>();
        if( !poBlock || !poBlock->Allocate( m_nChunkSize ) )
        {
            return FALSE;
        }

        poBlock->iBlock = iBlock;

        if( nDataRead >= (i+1) * m_nChunkSize )
            poBlock->nDataFilled = m_nChunkSize;
        else
            poBlock->nDataFilled = nDataRead - i*m_nChunkSize;

        memcpy( poBlock->pabyData, pabyWorkBuffer + i*m_nChunkSize,
                poBlock->nDataFilled );

        nCacheUsed += poBlock->nDataFilled;

        // Merges into the LRU list.
        Demote( poBlock.get() );

        oMapOffsetToCache[iBlock] = std::move(poBlock);
    }

    if( pabyWorkBuffer != pBuffer )
        CPLFree( pabyWorkBuffer );

    return TRUE;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSICachedFile::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    if( nSize == 0 || nCount == 0 )
        return 0;
    const size_t nRequestedBytes = nSize * nCount;

    // nFileSize might be set wrongly to 0 by underlying layers, such as
    // /vsicurl_streaming/https://query.data.world/s/jgsghstpphjhicstradhy5kpjwrnfy
    if( nFileSize > 0 && nOffset >= nFileSize )
    {
        bEOF = true;
        return 0;
    }

/* ==================================================================== */
/*      Make sure the cache is loaded for the whole request region.     */
/* ==================================================================== */
    const vsi_l_offset nStartBlock = nOffset / m_nChunkSize;
    const vsi_l_offset nEndBlock =
        (nOffset + nRequestedBytes - 1) / m_nChunkSize;

    for( vsi_l_offset iBlock = nStartBlock; iBlock <= nEndBlock; iBlock++ )
    {
        auto oIter = oMapOffsetToCache.find(iBlock);
        if( oIter == oMapOffsetToCache.end() )
        {
            size_t nBlocksToLoad = 1;
            while( iBlock + nBlocksToLoad <= nEndBlock &&
                   oMapOffsetToCache.find(iBlock + nBlocksToLoad) == oMapOffsetToCache.end() )
            {
                nBlocksToLoad++;
            }

            LoadBlocks( iBlock, nBlocksToLoad, pBuffer, nRequestedBytes );
        }
    }

/* ==================================================================== */
/*      Copy data into the target buffer to the extent possible.        */
/* ==================================================================== */
    size_t nAmountCopied = 0;

    while( nAmountCopied < nRequestedBytes )
    {
        const vsi_l_offset iBlock = (nOffset + nAmountCopied) / m_nChunkSize;
        auto oIter = oMapOffsetToCache.find(iBlock);
        if( oIter == oMapOffsetToCache.end() )
        {
            // We can reach that point when the amount to read exceeds
            // the cache size.
            LoadBlocks(iBlock, 1,
                       static_cast<GByte *>(pBuffer) + nAmountCopied,
                       std::min(nRequestedBytes - nAmountCopied, m_nChunkSize));
            oIter = oMapOffsetToCache.find(iBlock);
            if( oIter == oMapOffsetToCache.end() )
            {
                break;
            }
        }

        VSICacheChunk* poBlock = oIter->second.get();
        const vsi_l_offset nStartOffset =
            static_cast<vsi_l_offset>(iBlock) * m_nChunkSize;
        if(nStartOffset + poBlock->nDataFilled < nAmountCopied + nOffset)
            break;
        const size_t nThisCopy = std::min(
            nRequestedBytes - nAmountCopied,
            static_cast<size_t>(
                ((nStartOffset + poBlock->nDataFilled)
                 - nAmountCopied - nOffset)));
        if( nThisCopy == 0 )
            break;

        memcpy( static_cast<GByte *>(pBuffer) + nAmountCopied,
                poBlock->pabyData
                + (nOffset + nAmountCopied) - nStartOffset,
                nThisCopy );

        nAmountCopied += nThisCopy;
    }

    nOffset += nAmountCopied;

/* -------------------------------------------------------------------- */
/*      Ensure the cache is reduced to our limit.                       */
/* -------------------------------------------------------------------- */
    while( nCacheUsed > nCacheMax )
        FlushLRU();

    const size_t nRet = nAmountCopied / nSize;
    if( nRet != nCount )
        bEOF = true;
    return nRet;
}

/************************************************************************/
/*                           ReadMultiRange()                           */
/************************************************************************/

int VSICachedFile::ReadMultiRange( int const nRanges, void ** const ppData,
                                   const vsi_l_offset* const panOffsets,
                                   const size_t* const panSizes )
{
    // If the base is /vsicurl/
    return poBase->ReadMultiRange( nRanges, ppData, panOffsets, panSizes );
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSICachedFile::Write( const void * /* pBuffer */,
                             size_t /*nSize */ ,
                             size_t /* nCount */ )
{
    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSICachedFile::Eof()

{
    return bEOF;
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

VSIVirtualHandle *
VSICreateCachedFile( VSIVirtualHandle *poBaseHandle,
                     size_t nChunkSize, size_t nCacheSize )

{
    return new VSICachedFile( poBaseHandle, nChunkSize, nCacheSize );
}
