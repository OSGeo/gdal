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

#include "blockdir/binarytilelayer.h"
#include "blockdir/blockfile.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"
#include <algorithm>
#include <limits>

using namespace PCIDSK;

/************************************************************************/
/*                            BinaryTileLayer()                         */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poBlockDir The associated block directory.
 * @param nLayer The index of the block layer.
 * @param psBlockLayer The block layer info.
 * @param psTileLayer The tile layer info.
 */
BinaryTileLayer::BinaryTileLayer(BlockDir * poBlockDir, uint32 nLayer,
                                 BlockLayerInfo * psBlockLayer,
                                 TileLayerInfo * psTileLayer)
    : BlockTileLayer(poBlockDir, nLayer, psBlockLayer, psTileLayer)
{
}

/************************************************************************/
/*                             WriteTileList()                          */
/************************************************************************/

/**
 * Writes the tile list to disk.
 */
void BinaryTileLayer::WriteTileList(void)
{
    BlockTileInfoList oTileList = moTileList;

    SwapBlockTile(&oTileList.front(), oTileList.size());

    WriteToLayer(&oTileList.front(), 0,
                 oTileList.size() * sizeof(BlockTileInfo));
}

/************************************************************************/
/*                              ReadTileList()                          */
/************************************************************************/

/**
 * Reads the tile list from disk.
 */
void BinaryTileLayer::ReadTileList(void)
{
    uint32 nTileCount = GetTileCount();

    uint64 nSize = static_cast<uint64>(nTileCount) * sizeof(BlockTileInfo);

    if (nSize > GetLayerSize() || !GetFile()->IsValidFileOffset(nSize))
        return ThrowPCIDSKException("The tile layer is corrupted.");

#if SIZEOF_VOIDP < 8
    if (nSize > std::numeric_limits<size_t>::max())
        return ThrowPCIDSKException("Unable to read extremely large tile layer on 32-bit system.");
#endif

    try
    {
        moTileList.resize(nTileCount);
    }
    catch (const std::exception & ex)
    {
        return ThrowPCIDSKException("Out of memory in BinaryTileDir::ReadTileList(): %s", ex.what());
    }

    ReadFromLayer(&moTileList.front(), 0,
                  moTileList.size() * sizeof(BlockTileInfo));

    SwapBlockTile(&moTileList.front(), moTileList.size());
}

/************************************************************************/
/*                             SwapBlockTile()                          */
/************************************************************************/

/**
 * Swaps the specified block tile info array.
 *
 * @param psTile The block tile info array to swap.
 * @param nCount The number of block tile info.
 */
void BinaryTileLayer::SwapBlockTile(BlockTileInfo * psTile, size_t nCount)
{
    if (!mpoBlockDir->NeedsSwap())
        return;

    for (BlockTileInfo * psEnd = psTile + nCount;
         psTile < psEnd; psTile++)
    {
        SwapData(&psTile->nOffset, 8, 1);
        SwapData(&psTile->nSize, 4, 1);
    }
}
