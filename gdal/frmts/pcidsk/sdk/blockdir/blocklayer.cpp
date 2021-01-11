/******************************************************************************
 *
 * Purpose:  Block directory API.
 *
 ******************************************************************************
 * Copyright (c) 2011
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

#include "blockdir/blocklayer.h"
#include "blockdir/blockfile.h"
#include "pcidsk_exception.h"

using namespace PCIDSK;

/************************************************************************/
/*                               BlockLayer()                           */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poBlockDir The associated block directory.
 * @param nLayer The index of the block layer.
 */
BlockLayer::BlockLayer(BlockDir * poBlockDir, uint32 nLayer)
    : mpoBlockDir(poBlockDir),
      mnLayer(nLayer)
{
}

/************************************************************************/
/*                              ~BlockLayer()                           */
/************************************************************************/

/**
 * Destructor.
 */
BlockLayer::~BlockLayer(void)
{
}

/************************************************************************/
/*                              GetBlockInfo()                          */
/************************************************************************/

/**
 * Gets the layer block at the specified index.
 *
 * @param iBlock The index of the layer block.
 *
 * @return The layer block at the specified index.
 */
BlockInfo * BlockLayer::GetBlockInfo(uint32 iBlock)
{
    if (!IsValid())
        return nullptr;

    uint32 nBlockCount = GetBlockCount();

    if (nBlockCount != moBlockList.size())
    {
        mpoBlockDir->ReadLayerBlocks(mnLayer);

        if (moBlockList.size() != nBlockCount)
            ThrowPCIDSKExceptionPtr("Corrupted block directory.");
    }

    if (iBlock >= moBlockList.size())
        return nullptr;

    return &moBlockList[iBlock];
}

/************************************************************************/
/*                             AllocateBlocks()                         */
/************************************************************************/

/**
 * Allocates the blocks of the specified data.
 *
 * @param nOffset The offset of the data.
 * @param nSize The size of the data.
 */
void BlockLayer::AllocateBlocks(uint64 nOffset, uint64 nSize)
{
    uint32 nBlockSize = mpoBlockDir->GetBlockSize();

    uint32 iStartBlock = (uint32) (nOffset / nBlockSize);
    uint32 nStartOffset = (uint32) (nOffset % nBlockSize);

    uint32 nNumBlocks = (uint32)
        ((nSize + nStartOffset + nBlockSize - 1) / nBlockSize);

    for (uint32 iBlock = 0; iBlock < nNumBlocks; iBlock++)
    {
        BlockInfo * psBlock = GetBlockInfo(iStartBlock + iBlock);

        if (!psBlock)
            break;

        if (psBlock->nSegment == INVALID_SEGMENT ||
            psBlock->nStartBlock == INVALID_BLOCK)
        {
            *psBlock = mpoBlockDir->GetFreeBlock();
        }
    }
}

/************************************************************************/
/*                           AreBlocksAllocated()                       */
/************************************************************************/

/**
 * Checks if the blocks of the specified data are allocated.
 *
 * @param nOffset The offset of the data.
 * @param nSize The size of the data.
 *
 * @return If the blocks of the specified data are allocated.
 */
bool BlockLayer::AreBlocksAllocated(uint64 nOffset, uint64 nSize)
{
    uint32 nBlockSize = mpoBlockDir->GetBlockSize();

    uint32 iStartBlock = (uint32) (nOffset / nBlockSize);
    uint32 nStartOffset = (uint32) (nOffset % nBlockSize);

    uint32 nNumBlocks = (uint32)
        ((nSize + nStartOffset + nBlockSize - 1) / nBlockSize);

    for (uint32 iBlock = 0; iBlock < nNumBlocks; iBlock++)
    {
        BlockInfo * psBlock = GetBlockInfo(iStartBlock + iBlock);

        if (!psBlock)
            return false;

        if (psBlock->nSegment == INVALID_SEGMENT ||
            psBlock->nStartBlock == INVALID_BLOCK)
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                           GetContiguousCount()                       */
/************************************************************************/

/**
 * Gets the number of contiguous blocks for the specified data.
 *
 * @param nOffset The offset of the data.
 * @param nSize The size of the data.
 *
 * @return The number of contiguous blocks for the specified data.
 */
uint32 BlockLayer::GetContiguousCount(uint64 nOffset, uint64 nSize)
{
    uint32 nBlockSize = mpoBlockDir->GetBlockSize();

    uint32 iStartBlock = (uint32) (nOffset / nBlockSize);
    uint32 nStartOffset = (uint32) (nOffset % nBlockSize);

    uint32 nNumBlocks = (uint32)
        ((nSize + nStartOffset + nBlockSize - 1) / nBlockSize);

    BlockInfo * psStartBlock = GetBlockInfo(iStartBlock);

    if (!psStartBlock)
        return 0;

    uint32 nContiguousCount = 1;

    for (uint32 iBlock = 1; iBlock < nNumBlocks; iBlock++)
    {
        BlockInfo * psNextBlock = GetBlockInfo(iStartBlock + iBlock);

        if (!psNextBlock)
            break;

        if (psNextBlock->nSegment != psStartBlock->nSegment)
            break;

        if (psNextBlock->nStartBlock != psStartBlock->nStartBlock + iBlock)
            break;

        nContiguousCount++;
    }

    return nContiguousCount;
}

/************************************************************************/
/*                               FreeBlocks()                           */
/************************************************************************/

/**
 * Frees the blocks of the specified data.
 *
 * @param nOffset The offset of the data.
 * @param nSize The size of the data.
 */
void BlockLayer::FreeBlocks(uint64 nOffset, uint64 nSize)
{
    uint32 nBlockSize = mpoBlockDir->GetBlockSize();

    uint32 iStartBlock = (uint32) ((nOffset + nBlockSize - 1) / nBlockSize);
    uint32 iEndBlock = (uint32) ((nOffset + nSize) / nBlockSize);

    uint32 nNumBlocks = iStartBlock < iEndBlock ? iEndBlock - iStartBlock : 0;

    BlockInfoList oFreeBlocks;

    oFreeBlocks.reserve(nNumBlocks);

    for (uint32 iBlock = 0; iBlock < nNumBlocks; iBlock++)
    {
        BlockInfo * psBlock = GetBlockInfo(iStartBlock + iBlock);

        if (!psBlock)
            break;

        if (psBlock->nSegment != INVALID_SEGMENT &&
            psBlock->nStartBlock != INVALID_BLOCK)
        {
            oFreeBlocks.push_back(*psBlock);

            psBlock->nSegment = INVALID_SEGMENT;
            psBlock->nStartBlock = INVALID_BLOCK;
        }
    }

    mpoBlockDir->AddFreeBlocks(oFreeBlocks);
}

/************************************************************************/
/*                              WriteToLayer()                          */
/************************************************************************/

/**
 * Writes the specified data to the layer.
 *
 * @param pData The data buffer to write.
 * @param nOffset The offset of the data.
 * @param nSize The size of the data.
 */
void BlockLayer::WriteToLayer(const void * pData, uint64 nOffset, uint64 nSize)
{
    if (nOffset + nSize > GetLayerSize())
        Resize(nOffset + nSize);

    AllocateBlocks(nOffset, nSize);

    uint32 nBlockSize = mpoBlockDir->GetBlockSize();

    uint8 * pabyData = (uint8 *) pData;

    for (uint64 iByte = 0; iByte < nSize; )
    {
        uint32 nContiguousCount =
            GetContiguousCount(nOffset + iByte, nSize - iByte);

        uint32 iBlock = (uint32) ((nOffset + iByte) / nBlockSize);
        uint32 iWork  = (uint32) ((nOffset + iByte) % nBlockSize);

        uint64 nWorkSize = nContiguousCount * nBlockSize - iWork;

        if (nWorkSize > nSize - iByte)
            nWorkSize = nSize - iByte;

        BlockInfo * psBlock = GetBlockInfo(iBlock);

        uint64 nWorkOffset = (uint64) psBlock->nStartBlock * nBlockSize + iWork;

        GetFile()->WriteToSegment(psBlock->nSegment, pabyData + iByte,
                                  nWorkOffset, nWorkSize);

        iByte += nWorkSize;
    }
}

/************************************************************************/
/*                             ReadFromLayer()                          */
/************************************************************************/

/**
 * Reads the specified data from the layer.
 *
 * @param pData The data buffer to read.
 * @param nOffset The offset of the data.
 * @param nSize The size of the data.
 */
bool BlockLayer::ReadFromLayer(void * pData, uint64 nOffset, uint64 nSize)
{
    uint64 nLayerSize = GetLayerSize();

    if (nSize > nLayerSize ||
        nOffset > nLayerSize ||
        nOffset + nSize > nLayerSize)
    {
        return false;
    }

    if (!AreBlocksAllocated(nOffset, nSize))
        return false;

    uint32 nBlockSize = mpoBlockDir->GetBlockSize();

    uint8 * pabyData = (uint8 *) pData;

    for (uint64 iByte = 0; iByte < nSize; )
    {
        uint32 nContiguousCount =
            GetContiguousCount(nOffset + iByte, nSize - iByte);

        uint32 iBlock = (uint32) ((nOffset + iByte) / nBlockSize);
        uint32 iWork  = (uint32) ((nOffset + iByte) % nBlockSize);

        uint64 nWorkSize = nContiguousCount * nBlockSize - iWork;

        if (nWorkSize > nSize - iByte)
            nWorkSize = nSize - iByte;

        BlockInfo * psBlock = GetBlockInfo(iBlock);

        uint64 nWorkOffset = (uint64) psBlock->nStartBlock * nBlockSize + iWork;

        GetFile()->ReadFromSegment(psBlock->nSegment, pabyData + iByte,
                                   nWorkOffset, nWorkSize);

        iByte += nWorkSize;
    }

    return true;
}

/************************************************************************/
/*                                GetFile()                             */
/************************************************************************/

/**
 * Gets the associated file of the block layer.
 *
 * @return The associated file of the block layer.
 */
BlockFile * BlockLayer::GetFile(void) const
{
    return mpoBlockDir->GetFile();
}

/************************************************************************/
/*                               NeedsSwap()                            */
/************************************************************************/

/**
 * Checks if the block directory on disk needs swapping.
 *
 * @return If the block directory on disk needs swapping.
 */
bool BlockLayer::NeedsSwap(void) const
{
    return mpoBlockDir->NeedsSwap();
}

/************************************************************************/
/*                                IsValid()                             */
/************************************************************************/

/**
 * Checks if the block layer is valid.
 *
 * @return If the block layer is valid.
 */
bool BlockLayer::IsValid(void) const
{
    return GetLayerType() != BLTDead;
}

/************************************************************************/
/*                                 Resize()                             */
/************************************************************************/

/**
 * Resizes the block layer to the specified size in bytes.
 *
 * @param nLayerSize The new block layer size in bytes.
 */
void BlockLayer::Resize(uint64 nLayerSize)
{
    if (!IsValid())
        return;

    if (nLayerSize == GetLayerSize())
        return;

    uint32 nBlockCount = GetBlockCount();

    uint32 nBlockSize = mpoBlockDir->GetBlockSize();

    // Check how many blocks are needed.
    uint32 nNeededBlocks =
        (uint32) ((nLayerSize + nBlockSize - 1) / nBlockSize);

    // Create new blocks.
    if (nNeededBlocks > nBlockCount)
    {
        uint32 nNewBlocks = nNeededBlocks - nBlockCount;

        PushBlocks(mpoBlockDir->CreateNewBlocks(nNewBlocks));
    }
    // Free blocks.
    else if (nNeededBlocks < nBlockCount)
    {
        uint32 nFreeBlocks = nBlockCount - nNeededBlocks;

        mpoBlockDir->AddFreeBlocks(PopBlocks(nFreeBlocks));
    }

    _SetLayerSize(nLayerSize);
}

/************************************************************************/
/*                               PushBlocks()                           */
/************************************************************************/

/**
 * Pushes the specified block list at the end of the layer's block list.
 *
 * @param oBlockList The block list to add.
 */
void BlockLayer::PushBlocks(const BlockInfoList & oBlockList)
{
    uint32 nBlockCount = GetBlockCount();

    if (nBlockCount != moBlockList.size())
    {
        mpoBlockDir->ReadLayerBlocks(mnLayer);

        if (moBlockList.size() != nBlockCount)
            ThrowPCIDSKException("Corrupted block directory.");
    }

    try
    {
        moBlockList.resize(nBlockCount + oBlockList.size());
    }
    catch (const std::exception & ex)
    {
        return ThrowPCIDSKException("Out of memory in BlockLayer::PushBlocks(): %s", ex.what());
    }

    for (size_t iBlock = 0; iBlock < oBlockList.size(); iBlock++)
        moBlockList[nBlockCount + iBlock] = oBlockList[iBlock];

    _SetBlockCount((uint32) moBlockList.size());
}

/************************************************************************/
/*                               PopBlocks()                            */
/************************************************************************/

/**
 * Pops the specified number of blocks from the end of the layer's block list.
 *
 * @param nBlockCount The number of blocks to remove.
 *
 * @return The removed block list.
 */
BlockInfoList BlockLayer::PopBlocks(uint32 nBlockCount)
{
    uint32 nCurrentBlockCount = GetBlockCount();

    if (nCurrentBlockCount != moBlockList.size())
    {
        mpoBlockDir->ReadLayerBlocks(mnLayer);

        if (moBlockList.size() != nCurrentBlockCount)
            ThrowPCIDSKException("Corrupted block directory.");
    }

    uint32 nRemainingBlockCount;

    BlockInfoList oRemovedBlocks;

    if (nBlockCount < nCurrentBlockCount)
    {
        nRemainingBlockCount = nCurrentBlockCount - nBlockCount;

        oRemovedBlocks =
            BlockInfoList(moBlockList.begin() + nRemainingBlockCount,
                          moBlockList.begin() + nCurrentBlockCount);
    }
    else
    {
        nRemainingBlockCount = 0;

        oRemovedBlocks = moBlockList;
    }

    try
    {
        moBlockList.resize(nRemainingBlockCount);
    }
    catch (const std::exception & ex)
    {
        ThrowPCIDSKException("Out of memory in BlockLayer::PopBlocks(): %s", ex.what());
    }

    _SetBlockCount(nRemainingBlockCount);

    return oRemovedBlocks;
}
