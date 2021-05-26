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

#include "blockdir/blockdir.h"
#include "blockdir/blocklayer.h"
#include "blockdir/blockfile.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"
#include <sstream>
#include <cstring>
#include <cassert>
#include <algorithm>

using namespace PCIDSK;

/************************************************************************/
/*                                BlockDir()                            */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poFile The associated file object.
 * @param nSegment The segment of the block directory.
 */
BlockDir::BlockDir(BlockFile * poFile, uint16 nSegment)
    : mpoFile(poFile),
      mnSegment(nSegment),
      mnVersion(0),
      mchEndianness(BigEndianSystem() ? 'B' : 'L'),
      mbNeedsSwap(false),
      mnValidInfo(0),
      mbModified(false),
      mbOnDisk(true)
{
    assert(poFile && nSegment != INVALID_SEGMENT);

    mpoFreeBlockLayer = nullptr;
}

/************************************************************************/
/*                                BlockDir()                            */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poFile The associated file object.
 * @param nSegment The segment of the block directory.
 * @param nVersion The version of the block directory.
 */
BlockDir::BlockDir(BlockFile * poFile, uint16 nSegment, uint16 nVersion)

    : mpoFile(poFile),
      mnSegment(nSegment),
      mnVersion(nVersion),
      mchEndianness(BigEndianSystem() ? 'B' : 'L'),
      mbNeedsSwap(false),
      mnValidInfo(0),
      mbModified(true),
      mbOnDisk(false)
{
    assert(poFile && nSegment != INVALID_SEGMENT);

    mpoFreeBlockLayer = nullptr;
}

/************************************************************************/
/*                               ~BlockDir()                            */
/************************************************************************/

/**
 * Destructor.
 */
BlockDir::~BlockDir(void)
{
    for (size_t iLayer = 0; iLayer < moLayerList.size(); iLayer++)
        delete moLayerList[iLayer];

    delete mpoFreeBlockLayer;

    delete mpoFile;
}

/************************************************************************/
/*                                  Sync()                              */
/************************************************************************/

/**
 * Synchronizes the block directory to disk.
 */
void BlockDir::Sync(void)
{
    if (!mbModified)
        return;

    if (!mpoFile->GetUpdatable())
        return;

    if (!IsValid())
    {
        ThrowPCIDSKException("Failed to save: %s",
                             mpoFile->GetFilename().c_str());
    }

    WriteDir();

    mbModified = false;
}

/************************************************************************/
/*                                GetFile()                             */
/************************************************************************/

/**
 * Gets the associated file of the block directory.
 *
 * @return The associated file of the block directory.
 */
BlockFile * BlockDir::GetFile(void) const
{
    return mpoFile;
}

/************************************************************************/
/*                            GetSegmentIndex()                         */
/************************************************************************/

/**
 * Gets the index of the block directory segment.
 *
 * @return The index of the block directory segment.
 */
uint16 BlockDir::GetSegmentIndex(void) const
{
    return mnSegment;
}

/************************************************************************/
/*                               GetVersion()                           */
/************************************************************************/

/**
 * Gets the version of the block directory.
 *
 * @return The version of the block directory.
 */
uint16 BlockDir::GetVersion(void) const
{
    return mnVersion;
}

/************************************************************************/
/*                               NeedsSwap()                            */
/************************************************************************/

/**
 * Checks if the block directory on disk needs swapping.
 *
 * @return If the block directory on disk needs swapping.
 */
bool BlockDir::NeedsSwap(void) const
{
    return mbNeedsSwap;
}

/************************************************************************/
/*                                IsValid()                             */
/************************************************************************/

/**
 * Checks if the block directory is valid.
 *
 * @note The block directory is considered valid if the last
 *       two bytes of the header are the same as when the header was read.
 *
 * @return If the block directory is valid.
 */
bool BlockDir::IsValid(void) const
{
    if (!mbOnDisk)
        return true;

    // Read the block directory header from disk.
    uint8 abyHeader[512];

    mpoFile->ReadFromSegment(mnSegment, abyHeader, 0, 512);

    // The last 2 bytes of the header are for the valid info.
    uint16 nValidInfo;

    memcpy(&nValidInfo, abyHeader + 512 - 2, 2);

    SwapValue(&nValidInfo);

    // Check if the valid info has changed since the last read.
    return nValidInfo == mnValidInfo;
}

/************************************************************************/
/*                               IsModified()                           */
/************************************************************************/

/**
 * Checks if the block directory is modified.
 *
 * @return If the block directory is modified.
 */
bool BlockDir::IsModified(void) const
{
    return mbModified;
}

/************************************************************************/
/*                             GetLayerCount()                          */
/************************************************************************/

/**
 * Gets the number of block layers.
 *
 * @return The number of block layers.
 */
uint32 BlockDir::GetLayerCount(void) const
{
    return (uint32) moLayerList.size();
}

/************************************************************************/
/*                              GetLayerType()                          */
/************************************************************************/

/**
 * Gets the type of the block layer specified index.
 *
 * @param iLayer The index of the block layer.
 *
 * @return The type of the specified block layer.
 */
uint16 BlockDir::GetLayerType(uint32 iLayer) const
{
    if (iLayer >= moLayerList.size())
        return BLTDead;

    return moLayerList[iLayer]->GetLayerType();
}

/************************************************************************/
/*                              GetLayerSize()                          */
/************************************************************************/

/**
 * Gets the size in bytes of the block layer specified index.
 *
 * @param iLayer The index of the block layer.
 *
 * @return The size in bytes of the block layer specified index.
 */
uint64 BlockDir::GetLayerSize(uint32 iLayer) const
{
    if (iLayer >= moLayerList.size())
        return 0;

    return moLayerList[iLayer]->GetLayerSize();
}

/************************************************************************/
/*                                IsValid()                             */
/************************************************************************/

/**
 * Checks if the block layer at the specified index is valid.
 *
 * @param iLayer The index of the block layer.
 *
 * @return If the the specified block layer is valid.
 */
bool BlockDir::IsLayerValid(uint32 iLayer) const
{
    return GetLayerType(iLayer) != BLTDead;
}

/************************************************************************/
/*                                GetLayer()                            */
/************************************************************************/

/**
 * Gets the block layer at the specified index.
 *
 * @param iLayer The index of the block layer.
 *
 * @return The block layer at the specified index.
 */
BlockLayer * BlockDir::GetLayer(uint32 iLayer)
{
    if (iLayer >= moLayerList.size())
        return nullptr;

    return moLayerList[iLayer];
}

/************************************************************************/
/*                              CreateLayer()                           */
/************************************************************************/

/**
 * Creates a block layer of the specified type.
 *
 * @param nLayerType The type of the block layer to create.
 *
 * @return The index of the new block layer.
 */
uint32 BlockDir::CreateLayer(int16 nLayerType)
{
    // Try to find an invalid layer.
    uint32 nNewLayerIndex = INVALID_LAYER;

    for (size_t iLayer = 0; iLayer < moLayerList.size(); iLayer++)
    {
        if (!moLayerList[iLayer]->IsValid())
        {
            nNewLayerIndex = (uint32) iLayer;

            break;
        }
    }

    if (nNewLayerIndex == INVALID_LAYER)
    {
        nNewLayerIndex = (uint32) moLayerList.size();

        try
        {
            moLayerList.resize(moLayerList.size() + 1);
        }
        catch (const std::exception & ex)
        {
            return ThrowPCIDSKException(0, "Out of memory in BlockDir::CreateLayer(): %s", ex.what());
        }
    }
    else
    {
        delete moLayerList[nNewLayerIndex];
    }

    // Call the virtual method _CreateLayer() to create the layer.
    moLayerList[nNewLayerIndex] = _CreateLayer(nLayerType, nNewLayerIndex);

    mbModified = true;

    return nNewLayerIndex;
}

/************************************************************************/
/*                              DeleteLayer()                           */
/************************************************************************/

/**
 * Deletes the block layer with the specified index.
 *
 * @param iLayer The index of the block layer to delete.
 */
void BlockDir::DeleteLayer(uint32 iLayer)
{
    BlockLayer * poLayer = GetLayer(iLayer);

    assert(poLayer && poLayer->IsValid());
    if (!poLayer || !poLayer->IsValid())
        return;

    poLayer->Resize(0);

    // Call the virtual method _DeleteLayer() to delete the layer.
    _DeleteLayer(iLayer);

    mbModified = true;
}

/************************************************************************/
/*                            CreateNewBlocks()                         */
/************************************************************************/

/**
 * Creates the specified number of new blocks.
 *
 * @param nBlockCount The number of blocks to create.
 *
 * @return The specified number of new blocks.
 */
BlockInfoList BlockDir::CreateNewBlocks(uint32 nBlockCount)
{
    ValidateNewBlocks(nBlockCount, false);

    BlockInfoList oNewBlocks(nBlockCount);

    BlockInfoList::iterator oIter = oNewBlocks.begin();
    BlockInfoList::iterator oEnd = oNewBlocks.end();

    for (; oIter != oEnd; ++oIter)
    {
        oIter->nSegment = INVALID_SEGMENT;
        oIter->nStartBlock = INVALID_BLOCK;
    }

    mbModified = true;

    return oNewBlocks;
}

/************************************************************************/
/*                            CreateFreeBlocks()                        */
/************************************************************************/

/**
 * Creates the specified number of free blocks.
 *
 * @note The new blocks are going to be added to the free block layer.
 *
 * @param nBlockCount The number of blocks to create.
 */
void BlockDir::CreateFreeBlocks(uint32 nBlockCount)
{
    if (!mpoFreeBlockLayer)
        ReadFreeBlockLayer();

    ValidateNewBlocks(nBlockCount, true);

    uint32 nBlockSize = GetBlockSize();

    uint16 nDataSegment =
        mpoFile->ExtendSegment(GetDataSegmentName(), GetDataSegmentDesc(),
                               (uint64) nBlockCount * nBlockSize);

    uint64 nBlockOffset = mpoFile->GetSegmentSize(nDataSegment);

    assert(nBlockOffset % nBlockSize == 0);

    // Reverse the block list because GetFreeBlock() is LIFO.
    BlockInfoList oFreeBlockList;

    oFreeBlockList.reserve(nBlockCount);

    for (uint32 iBlock = 0; iBlock < nBlockCount; iBlock++)
    {
        BlockInfo sFreeBlock;

        nBlockOffset -= nBlockSize;

        sFreeBlock.nSegment = nDataSegment;
        sFreeBlock.nStartBlock = (uint32) (nBlockOffset / nBlockSize);

        oFreeBlockList.push_back(sFreeBlock);
    }

    mpoFreeBlockLayer->PushBlocks(oFreeBlockList);

    mbModified = true;
}

/************************************************************************/
/*                             AddFreeBlocks()                          */
/************************************************************************/

/**
 * Adds the the specified block list to the free block layer.
 *
 * @note Only the blocks which are allocated will be added to the
 *       free block layer.
 *
 * @param oBlockList The block list to add.
 */
void BlockDir::AddFreeBlocks(const BlockInfoList & oBlockList)
{
    if (!mpoFreeBlockLayer)
        ReadFreeBlockLayer();

    BlockInfoList oValidBlockList;

    oValidBlockList.reserve(oBlockList.size());

    // Reverse the block list because GetFreeBlock() is LIFO.
    BlockInfoList::const_reverse_iterator oIter = oBlockList.rbegin();
    BlockInfoList::const_reverse_iterator oEnd = oBlockList.rend();

    for (; oIter != oEnd; ++oIter)
    {
        if (oIter->nSegment != INVALID_SEGMENT &&
            oIter->nStartBlock != INVALID_BLOCK)
        {
            oValidBlockList.push_back(*oIter);
        }
    }

    mpoFreeBlockLayer->PushBlocks(oValidBlockList);

    mbModified = true;
}

/************************************************************************/
/*                              GetFreeBlock()                          */
/************************************************************************/

/**
 * Gets a free block from the free block layer.
 *
 * @note The block will be removed from the free block layer.
 *
 * @return A free block from the free block layer.
 */
BlockInfo BlockDir::GetFreeBlock(void)
{
    if (!mpoFreeBlockLayer)
        ReadFreeBlockLayer();

    // If we need more free blocks, create a minimum of 16 blocks.
    if (mpoFreeBlockLayer->GetBlockCount() == 0)
        CreateFreeBlocks(std::max((uint32) 16, GetNewBlockCount()));

    if (mpoFreeBlockLayer->GetBlockCount() <= 0)
        ThrowPCIDSKException("Cannot create new blocks.");

    BlockInfo sFreeBlock;
    sFreeBlock.nSegment = INVALID_SEGMENT;
    sFreeBlock.nStartBlock = INVALID_BLOCK;

    const BlockInfoList & oFreeBlockList = mpoFreeBlockLayer->PopBlocks(1);

    assert(oFreeBlockList.size() == 1);

    if (!oFreeBlockList.empty())
        sFreeBlock = oFreeBlockList[0];

    mbModified = true;

    return sFreeBlock;
}

/************************************************************************/
/*                               SwapValue()                            */
/************************************************************************/

/**
 * Swaps the specified value.
 *
 * @param pnValue The value to swap.
 */
void BlockDir::SwapValue(uint16 * pnValue) const
{
    if (!mbNeedsSwap)
        return;

    SwapData(pnValue, 2, 1);
}
