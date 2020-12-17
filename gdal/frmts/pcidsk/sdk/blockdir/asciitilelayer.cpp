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

#include "blockdir/asciitilelayer.h"
#include "blockdir/blockfile.h"
#include "core/pcidsk_scanint.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

using namespace PCIDSK;

/************************************************************************/
/*                             AsciiTileLayer()                         */
/************************************************************************/

/**
 * Constructor.
 *
 * @param poBlockDir The associated block directory.
 * @param nLayer The index of the block layer.
 * @param psBlockLayer The block layer info.
 * @param psTileLayer The tile layer info.
 */
AsciiTileLayer::AsciiTileLayer(BlockDir * poBlockDir, uint32 nLayer,
                               BlockLayerInfo * psBlockLayer,
                               TileLayerInfo * psTileLayer)
    : BlockTileLayer(poBlockDir, nLayer, psBlockLayer, psTileLayer)
{
}

/************************************************************************/
/*                               ReadHeader()                           */
/************************************************************************/

/**
 * Reads the tile layer header from disk.
 */
void AsciiTileLayer::ReadHeader(void)
{
    uint8 abyHeader[128];

    uint8 * pabyHeaderIter = abyHeader;

    ReadFromLayer(abyHeader, 0, 128);

    mpsTileLayer->nXSize = ScanInt8(pabyHeaderIter);
    pabyHeaderIter += 8;

    mpsTileLayer->nYSize = ScanInt8(pabyHeaderIter);
    pabyHeaderIter += 8;

    mpsTileLayer->nTileXSize = ScanInt8(pabyHeaderIter);
    pabyHeaderIter += 8;

    mpsTileLayer->nTileYSize = ScanInt8(pabyHeaderIter);
    pabyHeaderIter += 8;

    memcpy(mpsTileLayer->szDataType, pabyHeaderIter, 4);
    pabyHeaderIter += 4;

    std::string oNoDataValue((char *) pabyHeaderIter,
                             (char *) pabyHeaderIter + 18);

    mpsTileLayer->bNoDataValid = false;
    mpsTileLayer->dfNoDataValue = 0.0;
    pabyHeaderIter += 18;

    memcpy(mpsTileLayer->szCompress, pabyHeaderIter, 8);
    pabyHeaderIter += 8;
}

/************************************************************************/
/*                             WriteTileList()                          */
/************************************************************************/

/**
 * Writes the tile list to disk.
 */
void AsciiTileLayer::WriteTileList(void)
{
    uint32 nTileCount = GetTileCount();

    size_t nSize = 128 + nTileCount * 20;

    char * pabyTileLayer = (char *) malloc(nSize + 1); // +1 for '\0'.

    // Write the tile layer header to disk.
    char * pabyHeaderIter = pabyTileLayer;

    memset(pabyHeaderIter, ' ', 128);

    snprintf(pabyHeaderIter, 9, "%8d", mpsTileLayer->nXSize);
    pabyHeaderIter += 8;

    snprintf(pabyHeaderIter, 9, "%8d", mpsTileLayer->nYSize);
    pabyHeaderIter += 8;

    snprintf(pabyHeaderIter, 9, "%8d", mpsTileLayer->nTileXSize);
    pabyHeaderIter += 8;

    snprintf(pabyHeaderIter, 9, "%8d", mpsTileLayer->nTileYSize);
    pabyHeaderIter += 8;

    memcpy(pabyHeaderIter, mpsTileLayer->szDataType, 4);
    pabyHeaderIter += 4;

    if (mpsTileLayer->bNoDataValid)
        snprintf(pabyHeaderIter, 19, "%18.10E", mpsTileLayer->dfNoDataValue);
    pabyHeaderIter += 18;

    memcpy(pabyHeaderIter, mpsTileLayer->szCompress, 8);
    pabyHeaderIter += 8;

    // Write the tile list to disk.
    char * pabyTileListIter = pabyTileLayer + 128;

    for (uint32 iTile = 0; iTile < nTileCount; iTile++)
    {
        BlockTileInfo * psTile = &moTileList[iTile];

        snprintf(pabyTileListIter, 13, "%12" PCIDSK_FRMT_64_WITHOUT_PREFIX "u", psTile->nOffset);
        pabyTileListIter += 12;
    }

    // We cannot write the offset and the size at the same time because
    // snprintf() inserts a '\0' in the first character of the first size.
    for (uint32 iTile = 0; iTile < nTileCount; iTile++)
    {
        BlockTileInfo * psTile = &moTileList[iTile];

        snprintf(pabyTileListIter, 9, "%8d", psTile->nSize);
        pabyTileListIter += 8;
    }

    WriteToLayer(pabyTileLayer, 0, nSize);

    free(pabyTileLayer);
}

/************************************************************************/
/*                              ReadTileList()                          */
/************************************************************************/

/**
 * Reads the tile list from disk.
 */
void AsciiTileLayer::ReadTileList(void)
{
    uint32 nTileCount = GetTileCount();

    size_t nSize = nTileCount * 20;

    uint8 * pabyTileList = (uint8 *) malloc(nSize);

    ReadFromLayer(pabyTileList, 128, nSize);

    uint8 * pabyTileOffsetIter = pabyTileList;
    uint8 * pabyTileSizeIter = pabyTileList + nTileCount * 12;

    moTileList.resize(nTileCount);

    for (uint32 iTile = 0; iTile < nTileCount; iTile++)
    {
        BlockTileInfo * psTile = &moTileList[iTile];

        psTile->nOffset = ScanInt12(pabyTileOffsetIter);
        pabyTileOffsetIter += 12;

        psTile->nSize = ScanInt8(pabyTileSizeIter);
        pabyTileSizeIter += 8;
    }

    free(pabyTileList);
}
