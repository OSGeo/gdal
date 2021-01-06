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

#include "core/cpcidskblockfile.h"
#include "core/cpcidskfile.h"
#include "segment/systiledir.h"
#include "blockdir/asciitiledir.h"
#include "blockdir/binarytiledir.h"
#include "pcidsk_channel.h"
#include <cassert>
#include <algorithm>

using namespace PCIDSK;

CPCIDSKBlockFile::CPCIDSKBlockFile(PCIDSKFile * poFile)
    : mpoFile(dynamic_cast<CPCIDSKFile *>(poFile)),
      mnGrowingSegment(0)
{
    assert(mpoFile);
}

SysTileDir * CPCIDSKBlockFile::GetTileDir(void)
{
    SysTileDir * poTileDir = dynamic_cast<SysTileDir *>
        (mpoFile->GetSegment(SEG_SYS, "TileDir"));

    if (!poTileDir)
    {
        poTileDir = dynamic_cast<SysTileDir *>
            (mpoFile->GetSegment(SEG_SYS, "SysBMDir"));
    }

    return poTileDir;
}

SysTileDir * CPCIDSKBlockFile::CreateTileDir(void)
{
    SysTileDir * poTileDir = nullptr;

    std::string oFileOptions = GetFileOptions();

    for (char & chIter : oFileOptions)
        chIter = (char) toupper((uchar) chIter);

    // Check if we should create a TILEV1 or TILEV2 block directory.
    bool bTileV1 = oFileOptions.find("TILEV1") != std::string::npos;
    bool bTileV2 = oFileOptions.find("TILEV2") != std::string::npos;

    // The TILEV1 block directory has a limit of 762GB, so default
    // to TILEV2 if the image file size exceed 512GB.
    if (!bTileV2 && !bTileV1 &&
        GetImageFileSize() > (uint64) 549755813888ULL)
    {
        bTileV2 = true;
    }

    // We now use TILEV2 by default.
    if (bTileV2 || !bTileV1)
    {
        const char * pszDesc = "Block Tile Directory - Do not modify.";

        size_t nSegmentSize = BinaryTileDir::GetOptimizedDirSize(this);

        int nSegment =
            mpoFile->CreateSegment("TileDir", pszDesc, SEG_SYS,
                                   (int) ((nSegmentSize + 511) / 512));

        poTileDir = dynamic_cast<SysTileDir *>(mpoFile->GetSegment(nSegment));
    }
    else
    {
        const char * pszDesc =
            "System Block Map Directory - Do not modify.";

        size_t nSegmentSize = AsciiTileDir::GetOptimizedDirSize(this);

        int nSegment =
            mpoFile->CreateSegment("SysBMDir", pszDesc, SEG_SYS,
                                   (int) ((nSegmentSize + 511) / 512));

        poTileDir = dynamic_cast<SysTileDir *>(mpoFile->GetSegment(nSegment));
    }

    assert(poTileDir);

    poTileDir->CreateTileDir();

    return poTileDir;
}

std::string CPCIDSKBlockFile::GetFilename(void) const
{
    return mpoFile->GetFilename();
}

bool CPCIDSKBlockFile::GetUpdatable(void) const
{
    return mpoFile->GetUpdatable();
}

uint32 CPCIDSKBlockFile::GetWidth(void) const
{
    return mpoFile->GetWidth();
}

uint32 CPCIDSKBlockFile::GetHeight(void) const
{
    return mpoFile->GetHeight();
}

uint32 CPCIDSKBlockFile::GetChannels(void) const
{
    return mpoFile->GetChannels();
}

std::string CPCIDSKBlockFile::GetFileOptions(void) const
{
    return mpoFile->GetMetadataValue("_DBLayout");
}

uint64 CPCIDSKBlockFile::GetImageFileSize(void) const
{
    uint64 nImageSize = 0;

    int nChanCount = mpoFile->GetChannels();

    for (int iChan = 1; iChan <= nChanCount; iChan++)
    {
        PCIDSKChannel * poChannel = mpoFile->GetChannel(iChan);

        nImageSize += DataTypeSize(poChannel->GetType());
    }

    return nImageSize * mpoFile->GetWidth() * mpoFile->GetHeight();
}

bool CPCIDSKBlockFile::IsValidFileOffset(uint64 nOffset) const
{
    return nOffset <= mpoFile->GetFileSize() * 512;
}

bool CPCIDSKBlockFile::IsCorruptedSegment(uint16 nSegment, uint64 nOffset, uint64 nSize) const
{
    PCIDSKSegment * poSegment = mpoFile->GetSegment(nSegment);

    return (!poSegment ||
            nOffset + nSize > poSegment->GetContentSize() ||
            !IsValidFileOffset(nOffset + nSize + poSegment->GetContentOffset()));
}

uint16 CPCIDSKBlockFile::ExtendSegment(const std::string & oName,
                                       const std::string & oDesc,
                                       uint64 nExtendSize)
{
    // Check to see if the cached growing segment is still valid.
    if (mnGrowingSegment > 0)
    {
        PCIDSKSegment * poSegment = mpoFile->GetSegment(mnGrowingSegment);

        if (!poSegment->IsAtEOF() || !poSegment->CanExtend(nExtendSize))
            mnGrowingSegment = 0;
    }
    else
    {
        mnGrowingSegment = 0;
    }

    // Try to find an extendable segment.
    if (mnGrowingSegment < 1)
    {
        int nSegment = 0;

        PCIDSKSegment * poSegment;

        while ((poSegment =
                mpoFile->GetSegment(SEG_SYS, oName, nSegment)) != nullptr)
        {
            nSegment = poSegment->GetSegmentNumber();

            if (poSegment->IsAtEOF() && poSegment->CanExtend(nExtendSize))
            {
                mnGrowingSegment = (uint16) nSegment;
                break;
            }
        }
    }

    // Create a new segment if we could not found an extendable segment.
    if (mnGrowingSegment < 1)
    {
        mnGrowingSegment =
            (uint16) mpoFile->CreateSegment(oName, oDesc, SEG_SYS, 0);
    }

    mpoFile->ExtendSegment(mnGrowingSegment, (nExtendSize + 511) / 512,
                           false, false);

    return mnGrowingSegment;
}

uint64 CPCIDSKBlockFile::GetSegmentSize(uint16 nSegment)
{
    PCIDSKSegment * poSegment = mpoFile->GetSegment(nSegment);

    return poSegment ? poSegment->GetContentSize() : 0;
}

void CPCIDSKBlockFile::WriteToSegment(uint16 nSegment, const void * pData,
                                      uint64 nOffset, uint64 nSize)
{
    PCIDSKSegment * poSegment = mpoFile->GetSegment(nSegment);

    if (!poSegment)
        return;

    poSegment->WriteToFile(pData, nOffset, nSize);
}

void CPCIDSKBlockFile::ReadFromSegment(uint16 nSegment, void * pData,
                                       uint64 nOffset, uint64 nSize)
{
    PCIDSKSegment * poSegment = mpoFile->GetSegment(nSegment);

    if (!poSegment)
        return;

    poSegment->ReadFromFile(pData, nOffset, nSize);
}
