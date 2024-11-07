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

#ifndef PCIDSK_ASCII_TILE_LAYER_H
#define PCIDSK_ASCII_TILE_LAYER_H

#include "blockdir/blocktilelayer.h"
#include "blockdir/asciitiledir.h"

namespace PCIDSK
{

/************************************************************************/
/*                           class AsciiTileLayer                       */
/************************************************************************/

/**
 * Class used to manage a ascii block tile layer.
 *
 * @see BlockTileLayer
 */
class PCIDSK_DLL AsciiTileLayer : public BlockTileLayer
{
protected:
    virtual void        WriteTileList(void) override;
    virtual void        ReadTileList(void) override;

    // We need the system block directory implementation class to be friend
    // since it is responsible to fill in the block list.
    friend class AsciiTileDir;

public:
    AsciiTileLayer(BlockDir * poBlockDir, uint32 nLayer,
                   BlockLayerInfo * psBlockLayer,
                   TileLayerInfo * psTileLayer);

    void                ReadHeader(void);
};

} // namespace PCIDSK

#endif
