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

#include "blockdir/asciitiledir.h"
#include "blockdir/asciitilelayer.h"
#include "blockdir/blockfile.h"
#include "core/pcidsk_utils.h"
#include "core/pcidsk_scanint.h"
#include "pcidsk_exception.h"
#include "pcidsk_buffer.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <set>

using namespace PCIDSK;

#define ASCII_TILEDIR_VERSION 1

#define SYS_BLOCK_SIZE                  8192
#define SYS_BLOCK_INFO_SIZE             28
#define SYS_BLOCK_LAYER_INFO_SIZE       24

struct SysBlockInfo
{
    uint16 nSegment;
    uint32 nStartBlock;
    uint32 nNextBlock;
};

typedef std::vector<SysBlockInfo> SysBlockInfoList;

/************************************************************************/
/*                              GetBlockList()                          */
/************************************************************************/
static BlockInfoList GetBlockList(const SysBlockInfoList & oBlockInfoList,
                                  uint32 iStartBlock)
{
    uint32 iBlock = iStartBlock;

    BlockInfoList oBlockList;
    oBlockList.reserve(oBlockInfoList.size());

    while (iBlock < oBlockInfoList.size() &&
           oBlockList.size() <= oBlockInfoList.size())
    {
        const SysBlockInfo * psBlockInfo = &oBlockInfoList[iBlock];

        BlockInfo sBlock;
        sBlock.nSegment = psBlockInfo->nSegment;
        sBlock.nStartBlock = psBlockInfo->nStartBlock;

        oBlockList.push_back(sBlock);

        iBlock = psBlockInfo->nNextBlock;
    }

    // If the block list is larger than the block info list, it means that the
    // file is corrupted so look for a loop in the block list.
    if (oBlockList.size() > oBlockInfoList.size())
    {
        iBlock = iStartBlock;

        std::set<uint32> oBlockSet;

        oBlockList.clear();

        while (iBlock < oBlockInfoList.size())
        {
            const SysBlockInfo * psBlockInfo = &oBlockInfoList[iBlock];

            BlockInfo sBlock;
            sBlock.nSegment = psBlockInfo->nSegment;
            sBlock.nStartBlock = psBlockInfo->nStartBlock;

            oBlockList.push_back(sBlock);

            oBlockSet.insert(iBlock);

            iBlock = psBlockInfo->nNextBlock;

            if (oBlockSet.find(iBlock) != oBlockSet.end())
                break;
        }
    }

    return oBlockList;
}

/************************************************************************/
/*                          GetOptimizedDirSize()                       */
/************************************************************************/
size_t AsciiTileDir::GetOptimizedDirSize(BlockFile * poFile)
{
    std::string oFileOptions = poFile->GetFileOptions();

    for (char & chIter : oFileOptions)
        chIter = (char) toupper((uchar) chIter);

    // Compute the ratio.
    double dfRatio = 0.0;

    // The 35% is for the overviews.
    if (oFileOptions.find("TILED") != std::string::npos)
        dfRatio = 1.35;
    else
        dfRatio = 0.35;

    // The 5% is for the new blocks.
    dfRatio += 0.05;

    double dfFileSize = poFile->GetImageFileSize() * dfRatio;

    uint32 nBlockSize = SYS_BLOCK_SIZE;

    uint64 nBlockCount = (uint64) (dfFileSize / nBlockSize);

    uint64 nLayerCount = poFile->GetChannels();

    // The 12 is for the overviews.
    nLayerCount *= 12;

    uint64 nDirSize = 512 +
        (nBlockCount * SYS_BLOCK_INFO_SIZE +
         nLayerCount * SYS_BLOCK_LAYER_INFO_SIZE +
         nLayerCount * sizeof(TileLayerInfo));

    if (nDirSize > std::numeric_limits<size_t>::max())
        return ThrowPCIDSKException(0, "Unable to create extremely large file on 32-bit system.");

    return static_cast<size_t>(nDirSize);
}

/************************************************************************/
/*                              AsciiTileDir()                          */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poFile The associated file object.
 * @param nSegment The segment of the block directory.
 */
AsciiTileDir::AsciiTileDir(BlockFile * poFile, uint16 nSegment)
    : BlockTileDir(poFile, nSegment)
{
    // Read the block directory header from disk.
    uint8 abyHeader[512];

    mpoFile->ReadFromSegment(mnSegment, abyHeader, 0, 512);

    // Get the version of the block directory.
    mnVersion = ScanInt3(abyHeader + 7);

    // Read the block directory info from the header.
    msBlockDir.nLayerCount     = ScanInt8(abyHeader + 10);
    msBlockDir.nBlockCount     = ScanInt8(abyHeader + 18);
    msBlockDir.nFirstFreeBlock = ScanInt8(abyHeader + 26);

    // The third last byte is for the endianness.
    mchEndianness = abyHeader[512 - 3];
    mbNeedsSwap = (mchEndianness == 'B' ?
                   !BigEndianSystem() : BigEndianSystem());

    // The last 2 bytes of the header are for the validity info.
    memcpy(&mnValidInfo, abyHeader + 512 - 2, 2);

    SwapValue(&mnValidInfo);

    // Check that we support the tile directory version.
    if (mnVersion > ASCII_TILEDIR_VERSION)
    {
        ThrowPCIDSKException("The tile directory version %d is not supported.", mnVersion);
        return;
    }

    // The size of the block layers.
    uint64 nReadSize = (static_cast<uint64>(msBlockDir.nBlockCount) * SYS_BLOCK_INFO_SIZE +
                        static_cast<uint64>(msBlockDir.nLayerCount) * SYS_BLOCK_LAYER_INFO_SIZE);

    if (mpoFile->IsCorruptedSegment(mnSegment, 512, nReadSize))
    {
        ThrowPCIDSKException("The tile directory is corrupted.");
        return;
    }

    if (nReadSize > std::numeric_limits<size_t>::max())
    {
        ThrowPCIDSKException("Unable to open extremely large file on 32-bit system.");
        return;
    }

    // Initialize the block layers.
    try
    {
        moLayerInfoList.resize(msBlockDir.nLayerCount);
        moTileLayerInfoList.resize(msBlockDir.nLayerCount);

        moLayerList.resize(msBlockDir.nLayerCount);
    }
    catch (const std::exception & ex)
    {
        ThrowPCIDSKException("Out of memory in AsciiTileDir(): %s", ex.what());
        return;
    }

    for (uint32 iLayer = 0; iLayer < msBlockDir.nLayerCount; iLayer++)
    {
        moLayerInfoList[iLayer] = new BlockLayerInfo;
        moTileLayerInfoList[iLayer] = new TileLayerInfo;

        moLayerList[iLayer] = new AsciiTileLayer(this, iLayer,
                                                 moLayerInfoList[iLayer],
                                                 moTileLayerInfoList[iLayer]);
    }

    // Read the block directory from disk.
    if (memcmp(abyHeader + 128, "SUBVERSION 1", 12) != 0)
    {
        ReadFullDir();

        for (uint32 iLayer = 0; iLayer < msBlockDir.nLayerCount; iLayer++)
            GetTileLayer(iLayer)->ReadHeader();
    }
    else
    {
        ReadPartialDir();
    }

    // Check if any of the tile layers are corrupted.
    for (BlockLayer * poLayer : moLayerList)
    {
        BlockTileLayer * poTileLayer = dynamic_cast<BlockTileLayer *>(poLayer);

        if (poTileLayer->IsCorrupted())
        {
            ThrowPCIDSKException("The tile directory is corrupted.");
            return;
        }
    }
}

/************************************************************************/
/*                              AsciiTileDir()                          */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poFile The associated file object.
 * @param nSegment The segment of the block directory.
 * @param nBlockSize The size of the blocks.
 */
AsciiTileDir::AsciiTileDir(BlockFile * poFile, uint16 nSegment,
                           CPL_UNUSED uint32 nBlockSize)
    : BlockTileDir(poFile, nSegment, ASCII_TILEDIR_VERSION)
{
    // Initialize the directory info.
    msBlockDir.nLayerCount = 0;
    msBlockDir.nBlockCount = 0;
    msBlockDir.nFirstFreeBlock = 0;

    // Create an empty free block layer.
    msFreeBlockLayer.nLayerType = BLTFree;
    msFreeBlockLayer.nStartBlock = INVALID_BLOCK;
    msFreeBlockLayer.nBlockCount = 0;
    msFreeBlockLayer.nLayerSize = 0;

    mpoFreeBlockLayer = new AsciiTileLayer(this, INVALID_LAYER,
                                           &msFreeBlockLayer, nullptr);
}

/************************************************************************/
/*                              GetTileLayer()                          */
/************************************************************************/

/**
 * Gets the block layer at the specified index.
 *
 * @param iLayer The index of the block layer.
 *
 * @return The block layer at the specified index.
 */
AsciiTileLayer * AsciiTileDir::GetTileLayer(uint32 iLayer)
{
    return (AsciiTileLayer *) BlockDir::GetLayer(iLayer);
}

/************************************************************************/
/*                              GetBlockSize()                          */
/************************************************************************/

/**
 * Gets the block size of the block directory.
 *
 * @return The block size of the block directory.
 */
uint32 AsciiTileDir::GetBlockSize(void) const
{
    return SYS_BLOCK_SIZE;
}

/************************************************************************/
/*                              ReadFullDir()                           */
/************************************************************************/
void AsciiTileDir::ReadFullDir(void)
{
    // The size of the block layers.
    uint64 nReadSize = (static_cast<uint64>(msBlockDir.nBlockCount) * SYS_BLOCK_INFO_SIZE +
                        static_cast<uint64>(msBlockDir.nLayerCount) * SYS_BLOCK_LAYER_INFO_SIZE);

    if (mpoFile->IsCorruptedSegment(mnSegment, 512, nReadSize))
        return ThrowPCIDSKException("The tile directory is corrupted.");

    if (nReadSize > std::numeric_limits<size_t>::max())
        return ThrowPCIDSKException("Unable to open extremely large file on 32-bit system.");

    // Read the block layers from disk.
    uint8 * pabyBlockDir = (uint8 *) malloc(static_cast<size_t>(nReadSize));

    if (pabyBlockDir == nullptr)
        return ThrowPCIDSKException("Out of memory in AsciiTileDir::ReadFullDir().");

    PCIDSKBuffer oBlockDirAutoPtr;
    oBlockDirAutoPtr.buffer = (char *) pabyBlockDir;

    uint8 * pabyBlockDirIter = pabyBlockDir;

    mpoFile->ReadFromSegment(mnSegment, pabyBlockDir, 512, nReadSize);

    // Read the block list.
    SysBlockInfoList oBlockInfoList(msBlockDir.nBlockCount);

    for (uint32 iBlock = 0; iBlock < msBlockDir.nBlockCount; iBlock++)
    {
        SysBlockInfo * psBlock = &oBlockInfoList[iBlock];

        psBlock->nSegment    = ScanInt4(pabyBlockDirIter);
        pabyBlockDirIter += 4;

        psBlock->nStartBlock = ScanInt8(pabyBlockDirIter);
        pabyBlockDirIter += 8;

        //psBlock->nLayer      = ScanInt8(pabyBlockDirIter);
        pabyBlockDirIter += 8;

        psBlock->nNextBlock  = ScanInt8(pabyBlockDirIter);
        pabyBlockDirIter += 8;
    }

    // Read the block layers.
    for (uint32 iLayer = 0; iLayer < msBlockDir.nLayerCount; iLayer++)
    {
        BlockLayerInfo * psLayer = moLayerInfoList[iLayer];

        psLayer->nLayerType  = ScanInt4(pabyBlockDirIter);
        pabyBlockDirIter += 4;

        psLayer->nStartBlock = ScanInt8(pabyBlockDirIter);
        pabyBlockDirIter += 8;

        psLayer->nLayerSize  = ScanInt12(pabyBlockDirIter);
        pabyBlockDirIter += 12;
    }

    // Create all the block layers.
    for (uint32 iLayer = 0; iLayer < msBlockDir.nLayerCount; iLayer++)
    {
        BlockLayerInfo * psLayer = moLayerInfoList[iLayer];

        AsciiTileLayer * poLayer = GetTileLayer((uint32) iLayer);

        poLayer->moBlockList =
            GetBlockList(oBlockInfoList, psLayer->nStartBlock);

        // We need to validate the block count field.
        psLayer->nBlockCount = (uint32) poLayer->moBlockList.size();
    }

    // Create the free block layer.
    msFreeBlockLayer.nLayerType = BLTFree;
    msFreeBlockLayer.nStartBlock = msBlockDir.nFirstFreeBlock;
    msFreeBlockLayer.nBlockCount = 0;
    msFreeBlockLayer.nLayerSize = 0;

    mpoFreeBlockLayer = new AsciiTileLayer(this, INVALID_LAYER,
                                           &msFreeBlockLayer, nullptr);

    ((AsciiTileLayer *) mpoFreeBlockLayer)->moBlockList =
        GetBlockList(oBlockInfoList, msFreeBlockLayer.nStartBlock);

    // We need to validate the block count field.
    msFreeBlockLayer.nBlockCount = (uint32)
        ((AsciiTileLayer *) mpoFreeBlockLayer)->moBlockList.size();
}

/************************************************************************/
/*                             ReadPartialDir()                         */
/************************************************************************/
void AsciiTileDir::ReadPartialDir(void)
{
    // The offset of the block layers.
    uint64 nOffset = static_cast<uint64>(msBlockDir.nBlockCount) * SYS_BLOCK_INFO_SIZE;

    // The size of the block layers.
    uint64 nReadSize = (static_cast<uint64>(msBlockDir.nLayerCount) * SYS_BLOCK_LAYER_INFO_SIZE +
                        static_cast<uint64>(msBlockDir.nLayerCount) * sizeof(TileLayerInfo));

    if (mpoFile->IsCorruptedSegment(mnSegment, 512 + nOffset, nReadSize))
        return ThrowPCIDSKException("The tile directory is corrupted.");

    if (nReadSize > std::numeric_limits<size_t>::max())
        return ThrowPCIDSKException("Unable to open extremely large file on 32-bit system.");

    // Read the block layers from disk.
    uint8 * pabyBlockDir = (uint8 *) malloc(static_cast<size_t>(nReadSize));

    if (pabyBlockDir == nullptr)
        return ThrowPCIDSKException("Out of memory in AsciiTileDir::ReadPartialDir().");

    PCIDSKBuffer oBlockDirAutoPtr;
    oBlockDirAutoPtr.buffer = (char *) pabyBlockDir;

    uint8 * pabyBlockDirIter = pabyBlockDir;

    mpoFile->ReadFromSegment(mnSegment, pabyBlockDir, 512 + nOffset, nReadSize);

    // Read the block layers.
    BlockLayerInfo * psPreviousLayer = nullptr;

    for (uint32 iLayer = 0; iLayer < msBlockDir.nLayerCount; iLayer++)
    {
        BlockLayerInfo * psLayer = moLayerInfoList[iLayer];

        psLayer->nLayerType  = ScanInt4(pabyBlockDirIter);
        pabyBlockDirIter += 4;

        psLayer->nStartBlock = ScanInt8(pabyBlockDirIter);
        pabyBlockDirIter += 8;

        psLayer->nLayerSize  = ScanInt12(pabyBlockDirIter);
        pabyBlockDirIter += 12;

        if (psLayer->nStartBlock != INVALID_BLOCK)
        {
            if (psPreviousLayer)
            {
                if (psLayer->nStartBlock < psPreviousLayer->nStartBlock)
                    return ThrowPCIDSKException("The tile directory is corrupted.");

                psPreviousLayer->nBlockCount =
                    psLayer->nStartBlock - psPreviousLayer->nStartBlock;
            }

            psPreviousLayer = psLayer;
        }
        else
        {
            psLayer->nBlockCount = 0;
        }
    }

    // Read the tile layers.
    for (uint32 iLayer = 0; iLayer < msBlockDir.nLayerCount; iLayer++)
    {
        size_t nSize = sizeof(TileLayerInfo);
        SwapTileLayer((TileLayerInfo *) pabyBlockDirIter);
        memcpy(moTileLayerInfoList[iLayer], pabyBlockDirIter, nSize);
        pabyBlockDirIter += nSize;
    }

    // Read the free block layer.
    msFreeBlockLayer.nLayerType = BLTFree;
    msFreeBlockLayer.nStartBlock = msBlockDir.nFirstFreeBlock;
    msFreeBlockLayer.nBlockCount = 0;
    msFreeBlockLayer.nLayerSize = 0;

    if (msFreeBlockLayer.nStartBlock != INVALID_BLOCK)
    {
        if (psPreviousLayer)
        {
            if (msFreeBlockLayer.nStartBlock < psPreviousLayer->nStartBlock)
                return ThrowPCIDSKException("The tile directory is corrupted.");

            psPreviousLayer->nBlockCount =
                msFreeBlockLayer.nStartBlock - psPreviousLayer->nStartBlock;
        }

        if (msBlockDir.nBlockCount < msFreeBlockLayer.nStartBlock)
            return ThrowPCIDSKException("The tile directory is corrupted.");

        msFreeBlockLayer.nBlockCount =
            msBlockDir.nBlockCount - msFreeBlockLayer.nStartBlock;
    }
    else
    {
        if (psPreviousLayer)
        {
            if (msBlockDir.nBlockCount < psPreviousLayer->nStartBlock)
                return ThrowPCIDSKException("The tile directory is corrupted.");

            psPreviousLayer->nBlockCount =
                msBlockDir.nBlockCount - psPreviousLayer->nStartBlock;
        }

        msFreeBlockLayer.nBlockCount = 0;
    }
}

/************************************************************************/
/*                               GetDirSize()                           */
/************************************************************************/

/**
 * Gets the size in bytes of the block tile directory.
 *
 * @return The size in bytes of the block tile directory.
 */
size_t AsciiTileDir::GetDirSize(void) const
{
    uint64 nDirSize = 0;

    // Add the size of the header.
    nDirSize += 512;

    // Add the size of the blocks.
    for (size_t iLayer = 0; iLayer < moLayerInfoList.size(); iLayer++)
    {
        const BlockLayerInfo * psLayer = moLayerInfoList[iLayer];

        nDirSize += static_cast<uint64>(psLayer->nBlockCount) * SYS_BLOCK_INFO_SIZE;
    }

    // Add the size of the free blocks.
    nDirSize += static_cast<uint64>(msFreeBlockLayer.nBlockCount) * SYS_BLOCK_INFO_SIZE;

    // Add the size of the block layers.
    nDirSize += static_cast<uint64>(moLayerInfoList.size()) * SYS_BLOCK_LAYER_INFO_SIZE;

    // Add the size of the tile layers.
    nDirSize += static_cast<uint64>(moTileLayerInfoList.size()) * sizeof(TileLayerInfo);

    if (nDirSize > std::numeric_limits<size_t>::max())
        return ThrowPCIDSKException(0, "Unable to open extremely large file on 32-bit system or the tile directory is corrupted.");

    return static_cast<size_t>(nDirSize);
}

/************************************************************************/
/*                           GetLayerBlockCount()                       */
/************************************************************************/
uint32 AsciiTileDir::GetLayerBlockCount(void) const
{
    uint32 nLayerBlockCount = 0;

    for (size_t iLayer = 0; iLayer < moLayerInfoList.size(); iLayer++)
    {
        const BlockLayerInfo * psLayer = moLayerInfoList[iLayer];

        nLayerBlockCount += psLayer->nBlockCount;
    }

    return nLayerBlockCount;
}

/************************************************************************/
/*                           GetFreeBlockCount()                        */
/************************************************************************/
uint32 AsciiTileDir::GetFreeBlockCount(void) const
{
    return msFreeBlockLayer.nBlockCount;
}

/************************************************************************/
/*                           UpdateBlockDirInfo()                       */
/************************************************************************/
void AsciiTileDir::UpdateBlockDirInfo(void)
{
    uint32 nLayerBlockCount = GetLayerBlockCount();
    uint32 nFreeBlockCount = GetFreeBlockCount();

    // Update the block directory info.
    msBlockDir.nLayerCount = (uint32) moLayerInfoList.size();
    msBlockDir.nBlockCount = nLayerBlockCount + nFreeBlockCount;
    msBlockDir.nFirstFreeBlock = nLayerBlockCount;
}

/************************************************************************/
/*                             InitBlockList()                          */
/************************************************************************/
void AsciiTileDir::InitBlockList(AsciiTileLayer * poLayer)
{
    if (!poLayer || poLayer->mpsBlockLayer->nBlockCount == 0)
    {
        poLayer->moBlockList = BlockInfoList();
        return;
    }

    BlockLayerInfo * psLayer = poLayer->mpsBlockLayer;

    // The offset of the blocks.
    uint64 nOffset = static_cast<uint64>(psLayer->nStartBlock) * SYS_BLOCK_INFO_SIZE;

    // The size of the blocks.
    uint64 nReadSize = static_cast<uint64>(psLayer->nBlockCount) * SYS_BLOCK_INFO_SIZE;

    if (mpoFile->IsCorruptedSegment(mnSegment, 512 + nOffset, nReadSize))
        return ThrowPCIDSKException("The tile directory is corrupted.");

    if (nReadSize > std::numeric_limits<size_t>::max())
        return ThrowPCIDSKException("Unable to open extremely large file on 32-bit system.");

    // Read the blocks from disk.
    uint8 * pabyBlockDir = (uint8 *) malloc(static_cast<size_t>(nReadSize));

    if (pabyBlockDir == nullptr)
        return ThrowPCIDSKException("Out of memory in AsciiTileDir::InitBlockList().");

    PCIDSKBuffer oBlockDirAutoPtr;
    oBlockDirAutoPtr.buffer = (char *) pabyBlockDir;

    uint8 * pabyBlockDirIter = pabyBlockDir;

    mpoFile->ReadFromSegment(mnSegment, pabyBlockDir, 512 + nOffset, nReadSize);

    // Setup the block list.
    try
    {
        poLayer->moBlockList.resize(psLayer->nBlockCount);
    }
    catch (const std::exception & ex)
    {
        return ThrowPCIDSKException("Out of memory in AsciiTileDir::InitBlockList(): %s", ex.what());
    }

    for (uint32 iBlock = 0; iBlock < psLayer->nBlockCount; iBlock++)
    {
        BlockInfo * psBlock = &poLayer->moBlockList[iBlock];

        psBlock->nSegment    = ScanInt4(pabyBlockDirIter);
        pabyBlockDirIter += 4;

        psBlock->nStartBlock = ScanInt8(pabyBlockDirIter);
        pabyBlockDirIter += 8;

        //psBlock->nLayer      = ScanInt8(pabyBlockDirIter);
        pabyBlockDirIter += 8;

        //psBlock->nNextBlock  = ScanInt8(pabyBlockDirIter);
        pabyBlockDirIter += 8;
    }
}

/************************************************************************/
/*                            ReadLayerBlocks()                         */
/************************************************************************/
void AsciiTileDir::ReadLayerBlocks(uint32 iLayer)
{
    InitBlockList((AsciiTileLayer *) moLayerList[iLayer]);
}

/************************************************************************/
/*                           ReadFreeBlockLayer()                       */
/************************************************************************/
void AsciiTileDir::ReadFreeBlockLayer(void)
{
    mpoFreeBlockLayer = new AsciiTileLayer(this, INVALID_LAYER,
                                           &msFreeBlockLayer, nullptr);

    InitBlockList((AsciiTileLayer *) mpoFreeBlockLayer);
}

/************************************************************************/
/*                                WriteDir()                            */
/************************************************************************/
void AsciiTileDir::WriteDir(void)
{
    UpdateBlockDirInfo();

    // Make sure all the layer's block list are valid.
    if (mbOnDisk)
    {
        for (size_t iLayer = 0; iLayer < moLayerList.size(); iLayer++)
        {
            AsciiTileLayer * poLayer = GetTileLayer((uint32) iLayer);

            if (poLayer->moBlockList.size() != poLayer->GetBlockCount())
                InitBlockList(poLayer);
        }
    }

    // What is the size of the block directory.
    size_t nDirSize = GetDirSize();

    // If we are resizing the segment, resize it to the optimized size.
    if (nDirSize > mpoFile->GetSegmentSize(mnSegment))
        nDirSize = std::max(nDirSize, GetOptimizedDirSize(mpoFile));

    // Write the block directory to disk.
    char * pabyBlockDir = (char *) malloc(nDirSize + 1); // +1 for '\0'.

    if (pabyBlockDir == nullptr)
        return ThrowPCIDSKException("Out of memory in AsciiTileDir::WriteDir().");

    PCIDSKBuffer oBlockDirAutoPtr;
    oBlockDirAutoPtr.buffer = pabyBlockDir;

    char * pabyBlockDirIter = pabyBlockDir;

    // Initialize the header.
    memset(pabyBlockDir, ' ', 512);

    // The first 10 bytes are for the version.
    memcpy(pabyBlockDirIter, "VERSION", 7);
    snprintf(pabyBlockDirIter + 7, 9, "%3d", mnVersion);
    pabyBlockDirIter += 10;

    // Write the block directory info.
    snprintf(pabyBlockDirIter, 9, "%8d", msBlockDir.nLayerCount);
    pabyBlockDirIter += 8;

    snprintf(pabyBlockDirIter, 9, "%8d", msBlockDir.nBlockCount);
    pabyBlockDirIter += 8;

    snprintf(pabyBlockDirIter, 9, "%8d", msBlockDir.nFirstFreeBlock);

    // The bytes from 128 to 140 are for the subversion.
    memcpy(pabyBlockDir + 128, "SUBVERSION 1", 12);

    // The third last byte is for the endianness.
    pabyBlockDir[512 - 3] = mchEndianness;

    // The last 2 bytes of the header are for the validity info.
    uint16 nValidInfo = ++mnValidInfo;
    SwapValue(&nValidInfo);
    memcpy(pabyBlockDir + 512 - 2, &nValidInfo, 2);

    // The header is 512 bytes.
    pabyBlockDirIter = pabyBlockDir + 512;

    // Write the block info list.
    uint32 nNextBlock = 1;

    for (size_t iLayer = 0; iLayer < moLayerInfoList.size(); iLayer++)
    {
        BlockLayerInfo * psLayer = moLayerInfoList[iLayer];

        AsciiTileLayer * poLayer = GetTileLayer((uint32) iLayer);

        for (size_t iBlock = 0; iBlock < psLayer->nBlockCount; iBlock++)
        {
            BlockInfo * psBlock = &poLayer->moBlockList[iBlock];

            snprintf(pabyBlockDirIter, 9, "%4d", psBlock->nSegment);
            pabyBlockDirIter += 4;

            snprintf(pabyBlockDirIter, 9, "%8d", psBlock->nStartBlock);
            pabyBlockDirIter += 8;

            snprintf(pabyBlockDirIter, 9, "%8d", (uint32) iLayer);
            pabyBlockDirIter += 8;

            if (iBlock != psLayer->nBlockCount - 1)
                snprintf(pabyBlockDirIter, 9, "%8d", nNextBlock);
            else
                snprintf(pabyBlockDirIter, 9, "%8d", -1);
            pabyBlockDirIter += 8;

            nNextBlock++;
        }
    }

    // Write the free block info list.
    AsciiTileLayer * poLayer = (AsciiTileLayer *) mpoFreeBlockLayer;

    for (size_t iBlock = 0; iBlock < msFreeBlockLayer.nBlockCount; iBlock++)
    {
        BlockInfo * psBlock = &poLayer->moBlockList[iBlock];

        snprintf(pabyBlockDirIter, 9, "%4d", psBlock->nSegment);
        pabyBlockDirIter += 4;

        snprintf(pabyBlockDirIter, 9, "%8d", psBlock->nStartBlock);
        pabyBlockDirIter += 8;

        snprintf(pabyBlockDirIter, 9, "%8d", -1);
        pabyBlockDirIter += 8;

        if (iBlock != msFreeBlockLayer.nBlockCount - 1)
            snprintf(pabyBlockDirIter, 9, "%8d", nNextBlock);
        else
            snprintf(pabyBlockDirIter, 9, "%8d", -1);
        pabyBlockDirIter += 8;

        nNextBlock++;
    }

    // Write the block layers.
    uint32 nStartBlock = 0;

    for (BlockLayerInfo * psLayer : moLayerInfoList)
    {
        snprintf(pabyBlockDirIter, 9, "%4d", psLayer->nLayerType);
        pabyBlockDirIter += 4;

        if (psLayer->nBlockCount != 0)
            snprintf(pabyBlockDirIter, 9, "%8d", nStartBlock);
        else
            snprintf(pabyBlockDirIter, 9, "%8d", -1);
        pabyBlockDirIter += 8;

        snprintf(pabyBlockDirIter, 13, "%12" PCIDSK_FRMT_64_WITHOUT_PREFIX "d", psLayer->nLayerSize);
        pabyBlockDirIter += 12;

        nStartBlock += psLayer->nBlockCount;
    }

    // Write the tile layers.
    for (uint32 iLayer = 0; iLayer < msBlockDir.nLayerCount; iLayer++)
    {
        size_t nSize = sizeof(TileLayerInfo);
        memcpy(pabyBlockDirIter, moTileLayerInfoList[iLayer], nSize);
        SwapTileLayer((TileLayerInfo *) pabyBlockDirIter);
        pabyBlockDirIter += nSize;
    }

    // Initialize the remaining bytes so that Valgrind doesn't complain.
    size_t nRemainingBytes = pabyBlockDir + nDirSize - pabyBlockDirIter;

    if (nRemainingBytes)
        memset(pabyBlockDirIter, 0, nRemainingBytes);

    // Write the block directory to disk.
    mpoFile->WriteToSegment(mnSegment, pabyBlockDir, 0, nDirSize);
}

/************************************************************************/
/*                              _CreateLayer()                          */
/************************************************************************/

/**
 * Creates a block layer of the specified type at the specified index.
 *
 * @param nLayerType The type of the block layer to create.
 * @param iLayer The index of the block layer to create.
 *
 * @return The new block layer.
 */
BlockLayer * AsciiTileDir::_CreateLayer(uint16 nLayerType, uint32 iLayer)
{
    if (iLayer == moLayerInfoList.size())
    {
        try
        {
            moLayerInfoList.resize(moLayerInfoList.size() + 1);
            moTileLayerInfoList.resize(moLayerInfoList.size());
        }
        catch (const std::exception & ex)
        {
            return (BlockLayer *) ThrowPCIDSKExceptionPtr("Out of memory in AsciiTileDir::_CreateLayer(): %s", ex.what());
        }

        moLayerInfoList[iLayer] = new BlockLayerInfo;
        moTileLayerInfoList[iLayer] = new TileLayerInfo;
    }

    // Setup the block layer info.
    BlockLayerInfo * psBlockLayer = moLayerInfoList[iLayer];

    psBlockLayer->nLayerType = nLayerType;
    psBlockLayer->nBlockCount = 0;
    psBlockLayer->nLayerSize = 0;

    // Setup the tile layer info.
    TileLayerInfo * psTileLayer = moTileLayerInfoList[iLayer];

    memset(psTileLayer, 0, sizeof(TileLayerInfo));

    return new AsciiTileLayer(this, iLayer, psBlockLayer, psTileLayer);
}

/************************************************************************/
/*                              _DeleteLayer()                          */
/************************************************************************/

/**
 * Deletes the block layer with the specified index.
 *
 * @param iLayer The index of the block layer to delete.
 */
void AsciiTileDir::_DeleteLayer(uint32 iLayer)
{
    // Invalidate the block layer info.
    BlockLayerInfo * psBlockLayer = moLayerInfoList[iLayer];

    psBlockLayer->nLayerType = BLTDead;
    psBlockLayer->nBlockCount = 0;
    psBlockLayer->nLayerSize = 0;

    // Invalidate the tile layer info.
    TileLayerInfo * psTileLayer = moTileLayerInfoList[iLayer];

    memset(psTileLayer, 0, sizeof(TileLayerInfo));
}

/************************************************************************/
/*                           GetDataSegmentName()                       */
/************************************************************************/
std::string AsciiTileDir::GetDataSegmentName(void) const
{
    return "SysBData";
}

/************************************************************************/
/*                           GetDataSegmentDesc()                       */
/************************************************************************/
std::string AsciiTileDir::GetDataSegmentDesc(void) const
{
    return "Block Tile Data - Do not modify.";
}

/************************************************************************/
/*                           ValidateNewBlocks()                        */
/************************************************************************/
void AsciiTileDir::ValidateNewBlocks(uint32 & nNewBlockCount, bool bFreeBlocks)
{
    uint32 nLimitBlockCount = 99999999;
    uint32 nTotalBlockCount = GetLayerBlockCount() + GetFreeBlockCount();

    if (nTotalBlockCount >= nLimitBlockCount)
    {
        Sync(); // Make sure the directory is synchronized to disk.

        ThrowPCIDSKException("The file size limit has been reached.");
    }

    if (nTotalBlockCount + nNewBlockCount > nLimitBlockCount)
    {
        if (!bFreeBlocks)
        {
            Sync(); // Make sure the directory is synchronized to disk.

            ThrowPCIDSKException("The file size limit has been reached.");
        }

        nNewBlockCount = nLimitBlockCount - nTotalBlockCount;
    }
}
