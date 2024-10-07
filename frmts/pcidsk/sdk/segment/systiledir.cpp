/******************************************************************************
 *
 * Purpose:  Declaration of the SysTileDir class.
 *
 * This class is used to manage access to the system block tile directory.
 * This segment is used to keep track of one or more tile layers stored in
 * system block data segments. These tile layers are used to hold tiled images
 * for primary bands or overviews.
 *
 * This class is closely partnered with the CTiledChannel class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "segment/systiledir.h"
#include "blockdir/asciitiledir.h"
#include "blockdir/binarytiledir.h"
#include "blockdir/blocktilelayer.h"
#include "core/cpcidskblockfile.h"
#include "pcidsk_exception.h"

using namespace PCIDSK;

/************************************************************************/
/*                               SysTileDir()                           */
/************************************************************************/
SysTileDir::SysTileDir(PCIDSKFile * poFile, int nSegment,
                       const char * pbySegmentData)
    : CPCIDSKSegment(poFile, nSegment, pbySegmentData)
{
    mpoTileDir = nullptr;
}

/************************************************************************/
/*                              ~SysTileDir()                           */
/************************************************************************/
SysTileDir::~SysTileDir(void)
{
    try
    {
        Synchronize();
    }
    catch( const PCIDSKException& )
    {
        // TODO ?
    }
    delete mpoTileDir;
}

/************************************************************************/
/*                               Initialize()                           */
/************************************************************************/
void SysTileDir::Initialize(void)
{
}

/************************************************************************/
/*                            Synchronize()                             */
/************************************************************************/
void SysTileDir::Synchronize(void)
{
    if (mpoTileDir)
        mpoTileDir->Sync();
}

/************************************************************************/
/*                              LoadTileDir()                           */
/************************************************************************/
void SysTileDir::LoadTileDir(void)
{
    if (mpoTileDir)
        return;

    CPCIDSKBlockFile * poBlockFile = new CPCIDSKBlockFile(file);

    if (segment_name == "SysBMDir")
    {
        mpoTileDir = new AsciiTileDir(poBlockFile, (uint16) segment);
    }
    else if (segment_name == "TileDir")
    {
        mpoTileDir = new BinaryTileDir(poBlockFile, (uint16) segment);
    }
    else
    {
        delete poBlockFile;
        return ThrowPCIDSKException("Unknown block tile directory name.");
    }
}

/************************************************************************/
/*                             CreateTileDir()                          */
/************************************************************************/
void SysTileDir::CreateTileDir(void)
{
    CPCIDSKBlockFile * poBlockFile = new CPCIDSKBlockFile(file);

    if (segment_name == "SysBMDir")
    {
        mpoTileDir = new AsciiTileDir(poBlockFile, (uint16) segment, 8192);
    }
    else if (segment_name == "TileDir")
    {
        uint32 nBlockSize = BinaryTileDir::GetOptimizedBlockSize(poBlockFile);

        mpoTileDir = new BinaryTileDir(poBlockFile, (uint16) segment,
                                       nBlockSize);
    }
    else
    {
        delete poBlockFile;
        return ThrowPCIDSKException("Unknown block tile directory name.");
    }
}

/************************************************************************/
/*                              GetTileLayer()                          */
/************************************************************************/
BlockTileLayer * SysTileDir::GetTileLayer(uint32 nLayer)
{
    LoadTileDir();

    return mpoTileDir->GetTileLayer(nLayer);
}

/************************************************************************/
/*                            CreateTileLayer()                         */
/************************************************************************/
uint32 SysTileDir::CreateTileLayer(uint32 nWidth, uint32 nHeight,
                                   uint32 nTileWidth, uint32 nTileHeight,
                                   eChanType nDataType, std::string oCompress)
{
    if (oCompress.empty())
        oCompress = "NONE";

    LoadTileDir();

    uint32 nLayer = mpoTileDir->CreateLayer(BLTImage);

    BlockTileLayer * poTileLayer = mpoTileDir->GetTileLayer(nLayer);

    poTileLayer->SetTileLayerInfo(nWidth, nHeight, nTileWidth, nTileHeight,
                                  DataTypeName(nDataType), oCompress);

    return nLayer;
}
