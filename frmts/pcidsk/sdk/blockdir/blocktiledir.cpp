/******************************************************************************
 *
 * Purpose:  Block directory API.
 *
 ******************************************************************************
 * Copyright (c) 2011
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "blockdir/blocktiledir.h"
#include "blockdir/blocktilelayer.h"
#include "blockdir/blockfile.h"
#include "core/pcidsk_utils.h"
#include <cassert>

using namespace PCIDSK;

/************************************************************************/
/*                              BlockTileDir()                          */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poFile The associated file object.
 * @param nSegment The segment of the block directory.
 */
BlockTileDir::BlockTileDir(BlockFile * poFile, uint16 nSegment)
    : BlockDir(poFile, nSegment)
{
}

/************************************************************************/
/*                              BlockTileDir()                          */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poFile The associated file object.
 * @param nSegment The segment of the block directory.
 * @param nVersion The version of the block directory.
 */
BlockTileDir::BlockTileDir(BlockFile * poFile, uint16 nSegment, uint16 nVersion)
    : BlockDir(poFile, nSegment, nVersion)
{
}

/************************************************************************/
/*                             ~BlockTileDir()                          */
/************************************************************************/

/**
 * Destructor.
 */
BlockTileDir::~BlockTileDir(void)
{
    assert(moLayerInfoList.size() == moTileLayerInfoList.size());

    for (auto poIter : moLayerInfoList)
        delete poIter;

    for (auto poIter : moTileLayerInfoList)
        delete poIter;
}

/************************************************************************/
/*                              GetTileLayer()                          */
/************************************************************************/

/**
 * Gets the block tile layer at the specified index.
 *
 * @param iLayer The index of the block tile layer.
 *
 * @return The block tile layer at the specified index.
 */
BlockTileLayer * BlockTileDir::GetTileLayer(uint32 iLayer)
{
    return dynamic_cast<BlockTileLayer *>(GetLayer(iLayer));
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

/**
 * Gets the number of new blocks to create.
 *
 * @return The number of new blocks to create.
 */
uint32 BlockTileDir::GetNewBlockCount(void) const
{
    return (uint32) ((unsigned long long)(mpoFile->GetImageFileSize() / GetBlockSize()) * 0.01);
}

/************************************************************************/
/*                             SwapBlockLayer()                         */
/************************************************************************/

/**
 * Swaps the specified block layer info.
 *
 * @param psBlockLayer The block layer info to swap.
 */
void BlockTileDir::SwapBlockLayer(BlockLayerInfo * psBlockLayer)
{
    if (!mbNeedsSwap)
        return;

    SwapData(&psBlockLayer->nLayerType, 2, 1);
    SwapData(&psBlockLayer->nStartBlock, 4, 1);
    SwapData(&psBlockLayer->nBlockCount, 4, 1);
    SwapData(&psBlockLayer->nLayerSize, 8, 1);
}

/************************************************************************/
/*                             SwapTileLayer()                          */
/************************************************************************/

/**
 * Swaps the specified tile layer info.
 *
 * @param psTileLayer The tile layer info to swap.
 */
void BlockTileDir::SwapTileLayer(TileLayerInfo * psTileLayer)
{
    if (!mbNeedsSwap)
        return;

    SwapData(&psTileLayer->nXSize, 4, 1);
    SwapData(&psTileLayer->nYSize, 4, 1);
    SwapData(&psTileLayer->nTileXSize, 4, 1);
    SwapData(&psTileLayer->nTileYSize, 4, 1);
    SwapData(&psTileLayer->bNoDataValid, 2, 1);
    SwapData(&psTileLayer->dfNoDataValue, 8, 1);
}

/************************************************************************/
/*                               SwapBlock()                            */
/************************************************************************/

/**
 * Swaps the specified block info array.
 *
 * @param psBlock The block info array to swap.
 * @param nCount The number of block info.
 */
void BlockTileDir::SwapBlock(BlockInfo * psBlock, size_t nCount)
{
    if (!mbNeedsSwap)
        return;

    for (BlockInfo * psEnd = psBlock + nCount;
         psBlock < psEnd; psBlock++)
    {
        SwapData(&psBlock->nSegment, 2, 1);
        SwapData(&psBlock->nStartBlock, 4, 1);
    }
}
