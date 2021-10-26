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

#include "blockdir/blocktilelayer.h"
#include "blockdir/blockdir.h"
#include "blockdir/blockfile.h"
#include "blockdir/asciitiledir.h"
#include "blockdir/binarytiledir.h"
#include "core/mutexholder.h"
#include "pcidsk_types.h"
#include "pcidsk_exception.h"
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <limits>

#ifdef PCIMAJORVERSION
#include "raster/memcmp.hh"
#include "raster/memset.hh"
#endif

namespace PCIDSK
{

/************************************************************************/
/*                             BlockTileLayer()                         */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poBlockDir The associated block directory.
 * @param nLayer The index of the block layer.
 * @param psBlockLayer The block layer info.
 * @param psTileLayer The tile layer info.
 */
BlockTileLayer::BlockTileLayer(BlockDir * poBlockDir, uint32 nLayer,
                               BlockLayerInfo * psBlockLayer,
                               TileLayerInfo * psTileLayer)
    : BlockLayer(poBlockDir, nLayer),
      mpsBlockLayer(psBlockLayer),
      mpsTileLayer(psTileLayer),
      mbModified(false)
{
    memset(mszDataType, 0, sizeof(mszDataType));
    memset(mszCompress, 0, sizeof(mszCompress));

    mpoTileListMutex = DefaultCreateMutex();
}

/************************************************************************/
/*                            ~BlockTileLayer()                         */
/************************************************************************/

/**
 * Destructor.
 */
BlockTileLayer::~BlockTileLayer(void)
{
    delete mpoTileListMutex;
}

/************************************************************************/
/*                              GetTileInfo()                           */
/************************************************************************/

/**
 * Gets the tile at the specified index.
 *
 * @param nCol The column of the tile.
 * @param nRow The row of the tile.
 *
 * @return The tile at the specified index.
 */
BlockTileLayer::BlockTileInfo *
BlockTileLayer::GetTileInfo(uint32 nCol, uint32 nRow)
{
    if (!IsValid())
        return nullptr;

    uint32 nTilesPerRow = GetTilePerRow();

    uint32 iTile = nRow * nTilesPerRow + nCol;

    MutexHolder oLock(mpoTileListMutex);

    if (moTileList.empty())
        ReadTileList();

    return &moTileList.at(iTile);
}

/************************************************************************/
/*                                  Sync()                              */
/************************************************************************/

/**
 * Synchronizes the block tile layer to disk.
 */
void BlockTileLayer::Sync(void)
{
    if (!mbModified)
        return;

    try
    {
        if (!GetFile()->GetUpdatable())
            return;
    }
    catch (...)
    {
        return;
    }

    MutexHolder oLock(mpoTileListMutex);

    if (!mbModified)
        return;

    WriteTileList();

    mbModified = false;
}

/************************************************************************/
/*                              IsCorrupted()                           */
/************************************************************************/
bool BlockTileLayer::IsCorrupted(void) const
{
    // Dead layers have a tile size of 0, but it should be considered valid.
    if (GetLayerType() == BLTDead)
        return false;

    // The tile layer is corrupted when the image size is 0.
    if (GetXSize() == 0 || GetYSize() == 0)
        return true;

    uint64 nTileSize =
        static_cast<uint64>(GetTileXSize()) * GetTileYSize() * GetDataTypeSize();

    return nTileSize == 0 || nTileSize > std::numeric_limits<uint32>::max();
}

/************************************************************************/
/*                              GetTileCount()                          */
/************************************************************************/

/**
 * Gets the number of tiles in the tile layer.
 *
 * @return The number of tiles in the tile layer.
 */
uint32 BlockTileLayer::GetTileCount(void) const
{
    return (uint32) (((static_cast<uint64>(GetXSize()) + GetTileXSize() - 1) / GetTileXSize()) *
                     ((static_cast<uint64>(GetYSize()) + GetTileYSize() - 1) / GetTileYSize()));
}

/************************************************************************/
/*                             GetTilePerRow()                          */
/************************************************************************/

/**
 * Gets the number of tiles per row in the tile layer.
 *
 * @return The number of tiles per row in the tile layer.
 */
uint32 BlockTileLayer::GetTilePerRow(void) const
{
    return (uint32) (static_cast<uint64>(GetXSize()) + GetTileXSize() - 1) / GetTileXSize();
}

/************************************************************************/
/*                             GetTilePerCol()                          */
/************************************************************************/

/**
 * Gets the number of tiles per column in the tile layer.
 *
 * @return The number of tiles per column in the tile layer.
 */
uint32 BlockTileLayer::GetTilePerCol(void) const
{
    return (uint32) (static_cast<uint64>(GetYSize()) + GetTileYSize() - 1) / GetTileYSize();
}

/************************************************************************/
/*                              GetTileSize()                           */
/************************************************************************/

/**
 * Gets the size in bytes of a tile.
 *
 * @return The size in bytes of a tile.
 */
uint32 BlockTileLayer::GetTileSize(void) const
{
    return GetTileXSize() * GetTileYSize() * GetDataTypeSize();
}

/************************************************************************/
/*                            GetDataTypeSize()                         */
/************************************************************************/

/**
 * Gets the data type size in bytes.
 *
 * @return The data type size in bytes.
 */
uint32 BlockTileLayer::GetDataTypeSize(void) const
{
    return DataTypeSize(GetDataTypeFromName(GetDataType()));
}

/************************************************************************/
/*                              IsTileValid()                           */
/************************************************************************/

/**
 * Checks if the specified tile is valid.
 *
 * @param nCol The column of the tile.
 * @param nRow The row of the tile.
 *
 * @return If the specified tile is valid.
 */
bool BlockTileLayer::IsTileValid(uint32 nCol, uint32 nRow)
{
    BlockTileInfo * psTile = GetTileInfo(nCol, nRow);

    return (psTile && psTile->nOffset != INVALID_OFFSET && psTile->nSize != 0 &&
            AreBlocksAllocated(psTile->nOffset, psTile->nSize));
}

/************************************************************************/
/*                            GetTileDataSize()                         */
/************************************************************************/

/**
 * Gets the size in bytes of the specified tile.
 *
 * @param nCol The column of the tile.
 * @param nRow The row of the tile.
 *
 * @return The size in bytes of the specified tile.
 */
uint32 BlockTileLayer::GetTileDataSize(uint32 nCol, uint32 nRow)
{
    BlockTileInfo * psTile = GetTileInfo(nCol, nRow);

    return psTile ? psTile->nSize : 0;
}

/************************************************************************/
/*                            WriteSparseTile()                         */
/************************************************************************/

/**
 * Writes the specified tile only if the data is sparse.
 *
 * @param pData The data of the tile.
 * @param nCol The column of the tile.
 * @param nRow The row of the tile.
 *
 * @return If the specified tile data is sparse.
 */
bool BlockTileLayer::WriteSparseTile(const void * pData,
                                     uint32 nCol, uint32 nRow)
{
    MutexHolder oLock(mpoTileListMutex);

    uint32 nValue = 0;

    bool bIsSparse = true;

    uint32 nTileSize = GetTileSize();

    // Check if we can use a sparse tile with a 4 byte value.
    if (dynamic_cast<BinaryTileDir *>(mpoBlockDir) && nTileSize % 4 == 0)
    {
        uint32 * pnIter = (uint32 *) pData;

        nValue = *pnIter;

#ifdef PCIMAJORVERSION
        bIsSparse = raster::memcmp32(pnIter, nValue,
                                     nTileSize / sizeof(uint32));
#else
        uint32 * pnEnd = pnIter + nTileSize / sizeof(uint32);
        for (; pnIter < pnEnd; ++pnIter)
        {
            if (*pnIter != nValue)
            {
                bIsSparse = false;
                break;
            }
        }
#endif
    }
    // Check if we can use a sparse tile with a value of 0.
    else
    {
        nValue = 0;

#ifdef PCIMAJORVERSION
        bIsSparse = raster::memcmp8((uchar *) pData, 0, nTileSize);
#else
        uchar * pnIter = (uchar *) pData;
        uchar * pnEnd = pnIter + nTileSize;
        for (; pnIter < pnEnd; ++pnIter)
        {
            if (*pnIter != nValue)
            {
                bIsSparse = false;
                break;
            }
        }
#endif
    }

    // If the tile data is sparse store the sparse value in the nSize member
    // of the BlockTileInfo structure.
    if (bIsSparse)
    {
        BlockTileInfo * psTile = GetTileInfo(nCol, nRow);
        if( psTile != nullptr ) // TODO: what if it is null
        {
            // Free the blocks used by the tile.
            if (psTile->nOffset != INVALID_OFFSET)
                FreeBlocks(psTile->nOffset, psTile->nSize);

            psTile->nOffset = INVALID_OFFSET;

            psTile->nSize = nValue;

            mbModified = true;
        }
    }

    return bIsSparse;
}

/************************************************************************/
/*                               WriteTile()                            */
/************************************************************************/

/**
 * Writes the specified tile.
 *
 * @param pData The data of the tile.
 * @param nCol The column of the tile.
 * @param nRow The row of the tile.
 * @param nSize The size of the tile.
 */
void BlockTileLayer::WriteTile(const void * pData,
                               uint32 nCol, uint32 nRow, uint32 nSize)
{
    MutexHolder oLock(mpoTileListMutex);

    if (!IsValid())
        return;

    BlockTileInfo * psTile = GetTileInfo(nCol, nRow);

    if (!psTile)
        return;

    if (nSize == 0)
        nSize = GetTileSize();

    if (psTile->nOffset == INVALID_OFFSET)
    {
        psTile->nOffset = GetLayerSize();

        psTile->nSize = nSize;

        mbModified = true;
    }

    if (psTile->nSize < nSize)
    {
        psTile->nOffset = GetLayerSize();

        psTile->nSize = nSize;

        mbModified = true;
    }
    else if (psTile->nSize > nSize)
    {
        psTile->nSize = nSize;

        mbModified = true;
    }

    WriteToLayer(pData, psTile->nOffset, psTile->nSize);
}

/************************************************************************/
/*                             ReadSparseTile()                         */
/************************************************************************/

/**
 * Reads the specified tile only if the data is sparse.
 *
 * @param pData The data of the tile.
 * @param nCol The column of the tile.
 * @param nRow The row of the tile.
 *
 * @return If the specified tile data is sparse.
 */
bool BlockTileLayer::ReadSparseTile(void * pData, uint32 nCol, uint32 nRow)
{
    if (!IsValid())
        return false;

    BlockTileInfo * psTile = GetTileInfo(nCol, nRow);

    if (!psTile)
        return false;

    if (psTile->nOffset != INVALID_OFFSET)
        return false;

    uint32 nTileSize = GetTileSize();

    // Check if we can use a sparse tile with a 4 byte value.
    if (dynamic_cast<BinaryTileDir *>(mpoBlockDir) && nTileSize % 4 == 0)
    {
#ifdef PCIMAJORVERSION
        raster::memset32((uint32 *) pData, psTile->nSize,
                         nTileSize / sizeof(uint32));
#else
        uint32 * pnIter = (uint32 *) pData;
        uint32 * pnEnd = pnIter + nTileSize / sizeof(uint32);
        for (; pnIter < pnEnd; ++pnIter)
            *pnIter = psTile->nSize;
#endif
    }
    // Check if we can use a sparse tile with a value of 0.
    else
    {
        memset(pData, 0, nTileSize);
    }

    return true;
}

/************************************************************************/
/*                                ReadTile()                            */
/************************************************************************/

/**
 * Reads the specified tile.
 *
 * @param pData The data of the tile.
 * @param nCol The column of the tile.
 * @param nRow The row of the tile.
 * @param nSize The buffer size.
 *
 * @return The size of the tile.
 */
uint32 BlockTileLayer::ReadTile(void * pData, uint32 nCol, uint32 nRow, uint32 nSize)
{
    if (!IsValid())
        return 0;

    BlockTileInfo * psTile = GetTileInfo(nCol, nRow);

    if (!psTile)
        return 0;

    if (psTile->nOffset == INVALID_OFFSET)
        return 0;

    if (psTile->nSize == 0)
        return 0;

    uint32 nReadSize = std::min(nSize, psTile->nSize);

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    assert(psTile->nSize == nSize);
#endif

    if (!ReadFromLayer(pData, psTile->nOffset, nReadSize))
        return 0;

    return nReadSize;
}

/************************************************************************/
/*                         ReadPartialSparseTile()                      */
/************************************************************************/

/**
 * Reads the specified tile only if the data is sparse.
 *
 * @param pData The data of the tile.
 * @param nCol The column of the tile.
 * @param nRow The row of the tile.
 * @param nOffset The offset of the data.
 * @param nSize The size of the data.
 *
 * @return If the specified tile data is sparse.
 */
bool BlockTileLayer::ReadPartialSparseTile(void * pData,
                                           uint32 nCol, uint32 nRow,
                                           uint32 nOffset, uint32 nSize)
{
    if (!IsValid())
        return false;

    BlockTileInfo * psTile = GetTileInfo(nCol, nRow);

    if (!psTile)
        return false;

    if (psTile->nOffset != INVALID_OFFSET)
        return false;

    uint32 nTileSize = GetTileSize();

    // Check if we can use a sparse tile with a 4 byte value.
    if (dynamic_cast<BinaryTileDir *>(mpoBlockDir) && nTileSize % 4 == 0)
    {
        uint32 nValue = psTile->nSize;

        // We need to bitwise shift the value if the offset isn't aligned.
        uint32 nByteOffset = nOffset % 4;

        if (nByteOffset != 0)
        {
            uint32 nBitOffset = nByteOffset * 8;

            nValue = (nValue << nBitOffset) | (nValue >> (32 - nBitOffset));
        }

        uint32 nAlign = nSize / sizeof(uint32);

#ifdef PCIMAJORVERSION
        raster::memset32((uint32 *) pData, nValue, nAlign);
#else
        uint32 * pnIter = (uint32 *) pData;
        uint32 * pnEnd = pnIter + nAlign;
        for (; pnIter < pnEnd; ++pnIter)
            *pnIter = nValue;
#endif

        uint32 nRemaining = nSize % 4;

        if (nRemaining != 0)
        {
            uchar * pbyIter = (uchar *) pData + nAlign * 4;

            do
            {
                nValue = (nValue << 8) | (nValue >> 24);

                *pbyIter++ = (uchar) nValue;
            }
            while (--nRemaining);
        }
    }
    // Check if we can use a sparse tile with a value of 0.
    else
    {
        memset(pData, 0, nSize);
    }

    return true;
}

/************************************************************************/
/*                            ReadPartialTile()                         */
/************************************************************************/

/**
 * Reads the specified tile.
 *
 * @param pData The data of the tile.
 * @param nCol The column of the tile.
 * @param nRow The row of the tile.
 * @param nOffset The offset of the data.
 * @param nSize The size of the data.
 *
 * @return If the read was successful.
 */
bool BlockTileLayer::ReadPartialTile(void * pData, uint32 nCol, uint32 nRow,
                                     uint32 nOffset, uint32 nSize)
{
    if (!IsValid())
        return false;

    BlockTileInfo * psTile = GetTileInfo(nCol, nRow);

    if (!psTile)
        return false;

    if (psTile->nOffset == INVALID_OFFSET)
        return false;

    if (psTile->nSize == 0 || psTile->nSize < nOffset + nSize)
        return false;

    if (!ReadFromLayer(pData, psTile->nOffset + nOffset, nSize))
        return false;

    return true;
}

/************************************************************************/
/*                            SetTileLayerInfo()                        */
/************************************************************************/

/**
 * Sets the tile layer information.
 *
 * @param nXSize The width of the tile layer.
 * @param nYSize The height of the tile layer.
 * @param nTileXSize The width of a tile.
 * @param nTileYSize The height of a tile.
 * @param oDataType The data type of the tile layer.
 * @param oCompress The compress type of the tile layer.
 * @param bNoDataValid If the NoData value is valid.
 * @param dfNoDataValue The NoData value of the tile layer.
 */
void BlockTileLayer::SetTileLayerInfo(uint32 nXSize, uint32 nYSize,
                                      uint32 nTileXSize, uint32 nTileYSize,
                                      const std::string & oDataType,
                                      const std::string & oCompress,
                                      bool bNoDataValid, double dfNoDataValue)
{
    uint64 nTileSize =
        static_cast<uint64>(nTileXSize) * nTileYSize *
        DataTypeSize(GetDataTypeFromName(oDataType.c_str()));

    if (nTileSize == 0 || nTileSize > std::numeric_limits<uint32>::max())
    {
        return ThrowPCIDSKException("Invalid tile dimensions: %d x %d",
                                    nTileXSize, nTileYSize);
    }

    if (nXSize == 0 || nYSize == 0)
    {
        return ThrowPCIDSKException("Invalid tile layer dimensions: %d x %d",
                                    nXSize, nYSize);
    }

    mpsTileLayer->nXSize = nXSize;
    mpsTileLayer->nYSize = nYSize;
    mpsTileLayer->nTileXSize = nTileXSize;
    mpsTileLayer->nTileYSize = nTileYSize;
    mpsTileLayer->bNoDataValid = bNoDataValid;
    mpsTileLayer->dfNoDataValue = dfNoDataValue;

    memset(mpsTileLayer->szDataType, ' ', 4);
    memcpy(mpsTileLayer->szDataType, oDataType.data(), oDataType.size());

    memset(mpsTileLayer->szCompress, ' ', 8);
    memcpy(mpsTileLayer->szCompress, oCompress.data(), oCompress.size());

    // Invalidate the cache variables.
    *mszDataType = 0;
    *mszCompress = 0;

    // Initialize the tile list.
    uint32 nTileCount = GetTileCount();

    MutexHolder oLock(mpoTileListMutex);

    try
    {
        moTileList.resize(nTileCount);
    }
    catch (const std::exception & ex)
    {
        return ThrowPCIDSKException("Out of memory in BlockTileLayer::SetTileLayerInfo(): %s", ex.what());
    }

    for (uint32 iTile = 0; iTile < nTileCount; iTile++)
    {
        BlockTileInfo * psTile = &moTileList[iTile];

        psTile->nOffset = INVALID_OFFSET;
        psTile->nSize = 0;
    }

    // Write the tile list to disk.
    WriteTileList();

    mbModified = false;

    oLock.Release();

    // Make sure that the first tile starts on a block boundary.
    uint64 nLayerSize = GetLayerSize();
    uint32 nBlockSize = mpoBlockDir->GetBlockSize();

    if (nLayerSize % nBlockSize != 0)
        Resize((nLayerSize / nBlockSize + 1) * nBlockSize);
}

/************************************************************************/
/*                              GetDataType()                           */
/************************************************************************/

/**
 * Gets the data type of the tile layer.
 *
 * @return The data type of the tile layer.
 */
const char * BlockTileLayer::GetDataType(void) const
{
    if (*mszDataType)
        return mszDataType;

    MutexHolder oLock(mpoTileListMutex);

    if (*mszDataType)
        return mszDataType;

    memcpy(mszDataType, mpsTileLayer->szDataType, 4);

    int nIter = 3;

    while (nIter > 0 && mszDataType[nIter] == ' ')
        mszDataType[nIter--] = '\0';

    return mszDataType;
}

/************************************************************************/
/*                            GetCompressType()                         */
/************************************************************************/

/**
 * Gets the compress type of the tile layer.
 *
 * @return The compress type of the tile layer.
 */
const char * BlockTileLayer::GetCompressType(void) const
{
    if (*mszCompress)
        return mszCompress;

    MutexHolder oLock(mpoTileListMutex);

    if (*mszCompress)
        return mszCompress;

    memcpy(mszCompress, mpsTileLayer->szCompress, 8);

    int nIter = 7;

    while (nIter > 0 && mszCompress[nIter] == ' ')
        mszCompress[nIter--] = '\0';

    return mszCompress;
}

} // namespace PCIDSK
