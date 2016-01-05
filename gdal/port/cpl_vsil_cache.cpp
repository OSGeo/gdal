/******************************************************************************
 * $Id$
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of caching IO layer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <map>

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                             VSICacheChunk                            */
/* ==================================================================== */
/************************************************************************/

class VSICacheChunk
{
public:
  VSICacheChunk() :
      bDirty(FALSE),
      iBlock(0),
      poLRUPrev(NULL),
      poLRUNext(NULL),
      nDataFilled(0),
      pabyData(NULL)
    { }

    virtual ~VSICacheChunk()
    {
        VSIFree( pabyData );
    }

    bool Allocate( size_t nChunkSize )
    {
        CPLAssert( pabyData == NULL );
        pabyData = (GByte *)VSIMalloc( nChunkSize );
        return (pabyData != NULL);
    }

    int            bDirty;
    vsi_l_offset   iBlock;

    VSICacheChunk *poLRUPrev;
    VSICacheChunk *poLRUNext;

    vsi_l_offset   nDataFilled;
    GByte          *pabyData;
};

/************************************************************************/
/* ==================================================================== */
/*                             VSICachedFile                            */
/* ==================================================================== */
/************************************************************************/

class VSICachedFile CPL_FINAL : public VSIVirtualHandle
{ 
  public:
    VSICachedFile( VSIVirtualHandle *poBaseHandle, 
                   size_t nChunkSize,
                   size_t nCacheSize );
    ~VSICachedFile() { Close(); }

    void          FlushLRU();
    int           LoadBlocks( vsi_l_offset nStartBlock, size_t nBlockCount, 
                              void *pBuffer, size_t nBufferSize );
    void          Demote( VSICacheChunk * );

    VSIVirtualHandle *poBase;

    vsi_l_offset  nOffset;
    vsi_l_offset  nFileSize;

    GUIntBig      nCacheUsed;
    GUIntBig      nCacheMax;

    size_t        m_nChunkSize;

    VSICacheChunk *poLRUStart;
    VSICacheChunk *poLRUEnd;

    std::map<vsi_l_offset, VSICacheChunk*> oMapOffsetToCache;

    int            bEOF;

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();
    virtual void     *GetNativeFileDescriptor() { return poBase->GetNativeFileDescriptor(); }
};

/************************************************************************/
/*                           VSICachedFile()                            */
/************************************************************************/

VSICachedFile::VSICachedFile( VSIVirtualHandle *poBaseHandle, size_t nChunkSize, size_t nCacheSize )

{
    poBase = poBaseHandle;
    m_nChunkSize = nChunkSize;

    nCacheUsed = 0;
    if ( nCacheSize == 0 )
        nCacheMax = CPLScanUIntBig( 
             CPLGetConfigOption( "VSI_CACHE_SIZE", "25000000" ), 40 );
    else
        nCacheMax = nCacheSize;

    poLRUStart = NULL;
    poLRUEnd = NULL;

    poBase->Seek( 0, SEEK_END );
    nFileSize = poBase->Tell();

    nOffset = 0;
    bEOF = FALSE;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSICachedFile::Close()

{
    for( std::map<vsi_l_offset, VSICacheChunk*>::iterator oIter = oMapOffsetToCache.begin();
         oIter != oMapOffsetToCache.end(); ++oIter )
    {
        delete oIter->second;
    }
    oMapOffsetToCache.clear();

    poLRUStart = NULL;
    poLRUEnd = NULL;

    nCacheUsed = 0;

    if( poBase )
    {
        poBase->Close();
        delete poBase;
        poBase = NULL;
    }

    return 0;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICachedFile::Seek( vsi_l_offset nReqOffset, int nWhence )

{
    bEOF = FALSE;

    if( nWhence == SEEK_SET )
    {
        // use offset directly.
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
    CPLAssert( poLRUStart != NULL );

    VSICacheChunk *poBlock = poLRUStart;

    CPLAssert( nCacheUsed >= poBlock->nDataFilled );

    nCacheUsed -= poBlock->nDataFilled;

    poLRUStart = poBlock->poLRUNext;
    if( poLRUEnd == poBlock )
        poLRUEnd = NULL;

    if( poBlock->poLRUNext != NULL )
        poBlock->poLRUNext->poLRUPrev = NULL;

    CPLAssert( !poBlock->bDirty );

    oMapOffsetToCache[poBlock->iBlock] = NULL;

    delete poBlock;
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
    // already at end?
    if( poLRUEnd == poBlock )
        return;

    if( poLRUStart == poBlock )
        poLRUStart = poBlock->poLRUNext;

    if( poBlock->poLRUPrev != NULL )
        poBlock->poLRUPrev->poLRUNext = poBlock->poLRUNext;

    if( poBlock->poLRUNext != NULL )
        poBlock->poLRUNext->poLRUPrev = poBlock->poLRUPrev;

    poBlock->poLRUNext = NULL;
    poBlock->poLRUPrev = NULL;

    if( poLRUEnd != NULL )
        poLRUEnd->poLRUNext = poBlock;
    poLRUEnd = poBlock;

    if( poLRUStart == NULL )
        poLRUStart = poBlock;
}

/************************************************************************/
/*                             LoadBlocks()                             */
/*                                                                      */
/*      Load the desired set of blocks.  Use pBuffer as a temporary     */
/*      buffer if it would be helpful.                                  */
/************************************************************************/

int VSICachedFile::LoadBlocks( vsi_l_offset nStartBlock, size_t nBlockCount,
                               void *pBuffer, size_t nBufferSize )

{
    if( nBlockCount == 0 )
        return 1;

/* -------------------------------------------------------------------- */
/*      When we want to load only one block, we can directly load it    */
/*      into the target buffer with no concern about intermediaries.    */
/* -------------------------------------------------------------------- */
    if( nBlockCount == 1 )
    {
        poBase->Seek( (vsi_l_offset)nStartBlock * m_nChunkSize, SEEK_SET );

        VSICacheChunk *poBlock = new VSICacheChunk();
        if ( !poBlock || !poBlock->Allocate( m_nChunkSize ) )
        {
            delete poBlock;
            return 0;
        }

        oMapOffsetToCache[nStartBlock] = poBlock;

        poBlock->iBlock = nStartBlock;
        poBlock->nDataFilled = poBase->Read( poBlock->pabyData, 1, m_nChunkSize );
        nCacheUsed += poBlock->nDataFilled;

        // Merges into the LRU list. 
        Demote( poBlock );

        return 1;
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
            return 0;

        return LoadBlocks( nStartBlock+2, nBlockCount-2, pBuffer, nBufferSize );
    }

    if( poBase->Seek( (vsi_l_offset)nStartBlock * m_nChunkSize, SEEK_SET ) != 0 )
        return 0;

/* -------------------------------------------------------------------- */
/*      Do we need to allocate our own buffer?                          */
/* -------------------------------------------------------------------- */
    GByte *pabyWorkBuffer = (GByte *) pBuffer;

    if( nBufferSize < m_nChunkSize * nBlockCount )
        pabyWorkBuffer = (GByte *) CPLMalloc(m_nChunkSize * nBlockCount);

/* -------------------------------------------------------------------- */
/*      Read the whole request into the working buffer.                 */
/* -------------------------------------------------------------------- */

    size_t nDataRead = poBase->Read( pabyWorkBuffer, 1, nBlockCount*m_nChunkSize);

    if( nBlockCount * m_nChunkSize > nDataRead + m_nChunkSize - 1 )
        nBlockCount = (nDataRead + m_nChunkSize - 1) / m_nChunkSize;

    for( size_t i = 0; i < nBlockCount; i++ )
    {
        VSICacheChunk *poBlock = new VSICacheChunk();
        if ( !poBlock || !poBlock->Allocate( m_nChunkSize ) )
        {
            delete poBlock;
            return 0;
        }

        poBlock->iBlock = nStartBlock + i;

        CPLAssert( oMapOffsetToCache[i+nStartBlock] == NULL );

        oMapOffsetToCache[i + nStartBlock] = poBlock;

        if( nDataRead >= (i+1) * m_nChunkSize )
            poBlock->nDataFilled = m_nChunkSize;
        else
            poBlock->nDataFilled = nDataRead - i*m_nChunkSize;

        memcpy( poBlock->pabyData, pabyWorkBuffer + i*m_nChunkSize,
                (size_t) poBlock->nDataFilled );

        nCacheUsed += poBlock->nDataFilled;

        // Merges into the LRU list. 
        Demote( poBlock );
    }

    if( pabyWorkBuffer != pBuffer )
        CPLFree( pabyWorkBuffer );

    return 1;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSICachedFile::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    if( nOffset >= nFileSize )
    {
        bEOF = TRUE;
        return 0;
    }

/* ==================================================================== */
/*      Make sure the cache is loaded for the whole request region.     */
/* ==================================================================== */
    vsi_l_offset nStartBlock = nOffset / m_nChunkSize;
    vsi_l_offset nEndBlock = (nOffset + nSize * nCount - 1) / m_nChunkSize;

    for( vsi_l_offset iBlock = nStartBlock; iBlock <= nEndBlock; iBlock++ )
    {
        if( oMapOffsetToCache[iBlock] == NULL )
        {
            size_t nBlocksToLoad = 1;
            while( iBlock + nBlocksToLoad <= nEndBlock
                   && (oMapOffsetToCache[iBlock+nBlocksToLoad] == NULL) )
                nBlocksToLoad++;

            LoadBlocks( iBlock, nBlocksToLoad, pBuffer, nSize * nCount );
        }
    }

/* ==================================================================== */
/*      Copy data into the target buffer to the extent possible.        */
/* ==================================================================== */
    size_t nAmountCopied = 0;

    while( nAmountCopied < nSize * nCount )
    {
        vsi_l_offset iBlock = (nOffset + nAmountCopied) / m_nChunkSize;
        size_t nThisCopy;
        VSICacheChunk *poBlock = oMapOffsetToCache[iBlock];
        if( poBlock == NULL )
        {
            /* We can reach that point when the amount to read exceeds */
            /* the cache size */
            LoadBlocks( iBlock, 1, ((GByte *) pBuffer) + nAmountCopied,
                        MIN(nSize * nCount - nAmountCopied, m_nChunkSize) );
            poBlock = oMapOffsetToCache[iBlock];
            CPLAssert(poBlock != NULL);
        }

        vsi_l_offset nStartOffset = (vsi_l_offset)iBlock * m_nChunkSize;
        nThisCopy = (size_t)
            ((nStartOffset + poBlock->nDataFilled) 
             - nAmountCopied - nOffset);

        if( nThisCopy > nSize * nCount - nAmountCopied )
            nThisCopy = nSize * nCount - nAmountCopied;

        if( nThisCopy == 0 )
            break;

        memcpy( ((GByte *) pBuffer) + nAmountCopied,
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

    size_t nRet = nAmountCopied / nSize;
    if (nRet != nCount)
        bEOF = TRUE;
    return nRet;
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

/************************************************************************/
/*                        VSICreateCachedFile()                         */
/************************************************************************/

VSIVirtualHandle *
VSICreateCachedFile( VSIVirtualHandle *poBaseHandle, size_t nChunkSize, size_t nCacheSize )

{
    return new VSICachedFile( poBaseHandle, nChunkSize, nCacheSize );
}
