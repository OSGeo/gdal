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

#ifndef PCIDSK_BINARY_TILE_LAYER_H
#define PCIDSK_BINARY_TILE_LAYER_H

#include "blockdir/blocktilelayer.h"
#include "blockdir/binarytiledir.h"

namespace PCIDSK
{

/************************************************************************/
/*                          class BinaryTileLayer                       */
/************************************************************************/

/**
 * Class used to manage a binary block tile layer.
 *
 * @see BlockTileLayer
 */
class PCIDSK_DLL BinaryTileLayer : public BlockTileLayer
{
protected:
    virtual void        WriteTileList(void) override;
    virtual void        ReadTileList(void) override;

    void                SwapBlockTile(BlockTileInfo * psTile, size_t nCount);

    // We need the tile block directory implementation class to be friend
    // since it is responsible to fill in the block list.
    friend class BinaryTileDir;

public:
    BinaryTileLayer(BlockDir * poBlockDir, uint32 nLayer,
                    BlockLayerInfo * psBlockLayer,
                    TileLayerInfo * psTileLayer);
};

} // namespace PCIDSK

#endif
