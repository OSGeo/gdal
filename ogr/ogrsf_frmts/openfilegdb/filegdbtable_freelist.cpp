/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements management of FileGDB .freelist
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"

#include "filegdbtable.h"
#include "filegdbtable_priv.h"

#include <algorithm>
#include <limits>
#include <set>

#include "cpl_string.h"

namespace OpenFileGDB
{

constexpr uint32_t MINUS_ONE = 0xFFFFFFFFU;

constexpr int MINIMUM_SIZE_FOR_FREELIST = 8;

constexpr int nTrailerSize = 344;
constexpr int nTrailerEntrySize = 2 * static_cast<int>(sizeof(uint32_t));

constexpr int nPageSize = 4096;
constexpr int nPageHeaderSize = 2 * static_cast<int>(sizeof(uint32_t));

/************************************************************************/
/*                    FindFreelistRangeSlot()                           */
/************************************************************************/

// Fibonacci suite
static const uint32_t anHoleSizes[] = {
    0,
    8,
    16,
    24,
    40,
    64,
    104,
    168,
    272,
    440,
    712,
    1152,
    1864,
    3016,
    4880,
    7896,
    12776,
    20672,
    33448,
    54120,
    87568,
    141688,
    229256,
    370944,
    600200,
    971144,
    1571344,
    2542488,
    4113832,
    6656320,
    10770152,
    17426472,
    28196624,
    45623096,
    73819720,
    119442816,
    193262536,
    312705352,
    505967888,
    818673240,
    1324641128,
    2143314368,
    3467955496U
};

static int FindFreelistRangeSlot(uint32_t nSize)
{
    for( size_t i = 0; i < CPL_ARRAYSIZE(anHoleSizes) - 1; i++ )
    {
        if( /* nSize >= anHoleSizes[i] && */ nSize < anHoleSizes[i+1] )
        {
            return static_cast<int>(i);
        }
    }

    CPLDebug("OpenFileGDB", "Hole larger than can be handled");
    return -1;
}

/************************************************************************/
/*                        AddEntryToFreelist()                          */
/************************************************************************/

void FileGDBTable::AddEntryToFreelist(uint64_t nOffset, uint32_t nSize)
{
    if( nSize < MINIMUM_SIZE_FOR_FREELIST )
        return;

    const std::string osFilename = CPLResetExtension(m_osFilename.c_str(),
                                                     "freelist");
    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "rb+");
    if( fp == nullptr )
    {
        // Initialize an empty .freelist file
        fp = VSIFOpenL(osFilename.c_str(), "wb+");
        if( fp == nullptr )
            return;
        std::vector<GByte> abyTrailer;
        WriteUInt32(abyTrailer, 1);
        WriteUInt32(abyTrailer, MINUS_ONE);
        for( int i = 0; i < (nTrailerSize - nTrailerEntrySize) / nTrailerEntrySize; i++ )
        {
            WriteUInt32(abyTrailer, MINUS_ONE);
            WriteUInt32(abyTrailer, 0);
        }
        CPLAssert( static_cast<int>(abyTrailer.size()) == nTrailerSize );
        if( VSIFWriteL( abyTrailer.data(), abyTrailer.size(), 1, fp ) != 1 )
        {
            VSIFCloseL(fp);
            return;
        }
    }

    m_nHasFreeList = true;

    // Read trailer
    VSIFSeekL(fp, 0, SEEK_END);
    auto nFileSize = VSIFTellL(fp);
    if( (nFileSize % nPageSize) != nTrailerSize )
    {
        VSIFCloseL(fp);
        return;
    }

    VSIFSeekL(fp, nFileSize - nTrailerSize, SEEK_SET);
    std::vector<GByte> abyTrailer(nTrailerSize);
    if( VSIFReadL(abyTrailer.data(), abyTrailer.size(), 1, fp ) != 1 )
    {
        VSIFCloseL(fp);
        return;
    }

    // Determine in which "slot" of hole size the new entry belongs to
    const int iSlot = FindFreelistRangeSlot(nSize);
    if( iSlot < 0 )
    {
        VSIFCloseL(fp);
        return;
    }

    // Read the last page index of the identified slot
    uint32_t nPageIdx = GetUInt32(abyTrailer.data() + nTrailerEntrySize * iSlot, 0);
    uint32_t nPageCount;

    std::vector<GByte> abyPage;
    bool bRewriteTrailer = false;

    const int nEntrySize = static_cast<int>(sizeof(uint32_t)) + m_nTablxOffsetSize;
    const int nMaxEntriesPerPage = (nPageSize - nPageHeaderSize) / nEntrySize;
    int nNumEntries = 0;

    if( nPageIdx == MINUS_ONE )
    {
        // There's no allocate page for that range
        // So allocate one.

        WriteUInt32(abyPage, nNumEntries);
        WriteUInt32(abyPage, MINUS_ONE);
        abyPage.resize(nPageSize);

        // Update trailer
        bRewriteTrailer = true;
        nPageIdx = static_cast<uint32_t>((nFileSize - nTrailerSize) / nPageSize);
        nPageCount = 1;

        nFileSize += nPageSize; // virtual extension
    }
    else
    {
        nPageCount = GetUInt32(abyTrailer.data() + nTrailerEntrySize * iSlot +
                               sizeof(uint32_t), 0);

        VSIFSeekL(fp, static_cast<uint64_t>(nPageIdx) * nPageSize, 0);
        abyPage.resize(nPageSize);
        if( VSIFReadL(abyPage.data(), abyPage.size(), 1, fp ) != 1 )
        {
            VSIFCloseL(fp);
            return;
        }

        nNumEntries = GetUInt32(abyPage.data(), 0);
        if( nNumEntries >= nMaxEntriesPerPage )
        {
            // Allocate new page
            abyPage.clear();
            nNumEntries = 0;
            WriteUInt32(abyPage, nNumEntries);
            WriteUInt32(abyPage, nPageIdx); // Link to previous page
            abyPage.resize(nPageSize);

            // Update trailer
            bRewriteTrailer = true;
            nPageIdx = static_cast<uint32_t>((nFileSize - nTrailerSize) / nPageSize);
            nPageCount ++;

            nFileSize += nPageSize; // virtual extension
        }
    }

    // Add new entry into page
    WriteUInt32(abyPage, nSize, nPageHeaderSize + nNumEntries * nEntrySize);
    WriteFeatureOffset(
       nOffset,
       abyPage.data() + nPageHeaderSize + nNumEntries * nEntrySize + sizeof(uint32_t));

    // Update page header
    ++ nNumEntries;
    WriteUInt32(abyPage, nNumEntries, 0);

    // Flush page
    VSIFSeekL(fp, static_cast<uint64_t>(nPageIdx) * nPageSize, 0);
    if( VSIFWriteL(abyPage.data(), abyPage.size(), 1, fp ) != 1 )
    {
        VSIFCloseL(fp);
        return;
    }

    if( bRewriteTrailer )
    {
        WriteUInt32(abyTrailer, nPageIdx, nTrailerEntrySize * iSlot);
        WriteUInt32(abyTrailer, nPageCount, nTrailerEntrySize * iSlot + sizeof(uint32_t));

        VSIFSeekL(fp, nFileSize - nTrailerSize, 0);
        if( VSIFWriteL(abyTrailer.data(), abyTrailer.size(), 1, fp ) != 1 )
        {
            VSIFCloseL(fp);
            return;
        }
    }

    m_bFreelistCanBeDeleted = false;

    VSIFCloseL(fp);
}

/************************************************************************/
/*                   GetOffsetOfFreeAreaFromFreeList()                  */
/************************************************************************/

uint64_t FileGDBTable::GetOffsetOfFreeAreaFromFreeList(uint32_t nSize)
{
    if( nSize < MINIMUM_SIZE_FOR_FREELIST || m_nHasFreeList == FALSE || m_bFreelistCanBeDeleted )
        return OFFSET_MINUS_ONE;

    const std::string osFilename = CPLResetExtension(m_osFilename.c_str(),
                                                     "freelist");
    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "rb+");
    m_nHasFreeList = fp != nullptr;
    if( fp == nullptr )
        return OFFSET_MINUS_ONE;

    // Read trailer
    VSIFSeekL(fp, 0, SEEK_END);
    auto nFileSize = VSIFTellL(fp);

    if( (nFileSize % nPageSize) != nTrailerSize )
    {
        VSIFCloseL(fp);
        return OFFSET_MINUS_ONE;
    }

    VSIFSeekL(fp, nFileSize - nTrailerSize, SEEK_SET);
    std::vector<GByte> abyTrailer(nTrailerSize);
    if( VSIFReadL(abyTrailer.data(), abyTrailer.size(), 1, fp ) != 1 )
    {
        VSIFCloseL(fp);
        return OFFSET_MINUS_ONE;
    }

    // Determine in which "slot" of hole size the new entry belongs to
    const int iSlot = FindFreelistRangeSlot(nSize);
    if( iSlot < 0 )
    {
        VSIFCloseL(fp);
        return OFFSET_MINUS_ONE;
    }

    // Read the last page index of the identified slot
    uint32_t nPageIdx = GetUInt32(abyTrailer.data() + nTrailerEntrySize * iSlot, 0);
    if( nPageIdx == MINUS_ONE )
    {
        VSIFCloseL(fp);
        return OFFSET_MINUS_ONE;
    }

    VSIFSeekL(fp, static_cast<uint64_t>(nPageIdx) * nPageSize, 0);
    std::vector<GByte> abyPage(nPageSize);
    if( VSIFReadL(abyPage.data(), abyPage.size(), 1, fp ) != 1 )
    {
        CPLDebug("OpenFileGDB", "Can't read freelist page %u", nPageIdx);
        VSIFCloseL(fp);
        return OFFSET_MINUS_ONE;
    }

    const int nEntrySize = static_cast<int>(sizeof(uint32_t)) + m_nTablxOffsetSize;
    const int nMaxEntriesPerPage = (nPageSize - nPageHeaderSize) / nEntrySize;

    // Index of page that links to us
    uint32_t nReferencingPage = MINUS_ONE;
    std::vector<GByte> abyReferencingPage;

    int nBestCandidateNumEntries = 0;
    uint32_t nBestCandidatePageIdx = MINUS_ONE;
    uint32_t nBestCandidateSize = std::numeric_limits<uint32_t>::max();
    int iBestCandidateEntry = -1;
    uint32_t nBestCandidateReferencingPage = MINUS_ONE;
    std::vector<GByte> abyBestCandidateReferencingPage;
    std::vector<GByte> abyBestCandidatePage;

    std::set<uint32_t> aSetReadPages = { nPageIdx };
    while( true )
    {
        int nNumEntries = static_cast<int>(
            std::min(GetUInt32(abyPage.data(), 0),
                     static_cast<uint32_t>(nMaxEntriesPerPage)));
        bool bExactMatch = false;
        for( int i = nNumEntries - 1; i >= 0; i-- )
        {
            const uint32_t nFreeAreaSize = GetUInt32(
                abyPage.data() + nPageHeaderSize + i * nEntrySize, 0);
            if( nFreeAreaSize < anHoleSizes[iSlot] ||
                nFreeAreaSize >= anHoleSizes[iSlot+1] )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Page %u of %s contains free area of unexpected size at entry %d",
                         nPageIdx, osFilename.c_str(), i);
            }
            else if( nFreeAreaSize == nSize ||
                     (nFreeAreaSize > nSize && nFreeAreaSize < nBestCandidateSize) )
            {
                if( nBestCandidatePageIdx != nPageIdx )
                {
                    abyBestCandidatePage = abyPage;
                    abyBestCandidateReferencingPage = abyReferencingPage;
                }
                nBestCandidatePageIdx = nPageIdx;
                nBestCandidateReferencingPage = nReferencingPage;
                iBestCandidateEntry = i;
                nBestCandidateSize = nFreeAreaSize;
                nBestCandidateNumEntries = nNumEntries;
                if( nFreeAreaSize == nSize )
                {
                    bExactMatch = true;
                    break;
                }
            }
        }

        if( !bExactMatch )
        {
            const uint32_t nPrevPage = GetUInt32(abyPage.data() + sizeof(uint32_t), 0);
            if( nPrevPage == MINUS_ONE )
            {
                break;
            }

            if( aSetReadPages.find(nPrevPage) != aSetReadPages.end() )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cyclic page refererencing in %s",
                         osFilename.c_str());
                VSIFCloseL(fp);
                return OFFSET_MINUS_ONE;
            }
            aSetReadPages.insert(nPrevPage);

            abyReferencingPage = abyPage;
            nReferencingPage = nPageIdx;
            nPageIdx = nPrevPage;
            VSIFSeekL(fp, static_cast<uint64_t>(nPageIdx) * nPageSize, 0);
            if( VSIFReadL(abyPage.data(), abyPage.size(), 1, fp ) != 1 )
            {
                CPLDebug("OpenFileGDB", "Can't read freelist page %u", nPageIdx);
                break;
            }
        }
        else
        {
            break;
        }
    }

    if( nBestCandidatePageIdx == MINUS_ONE )
    {
        // If we go here, it means that the trailer section references empty
        // pages or pages with features of unexpected size.
        // Shouldn't happen for well-behaved .freelist files
        VSIFCloseL(fp);
        return OFFSET_MINUS_ONE;
    }

    nPageIdx = nBestCandidatePageIdx;
    nReferencingPage = nBestCandidateReferencingPage;
    abyPage = std::move(abyBestCandidatePage);
    abyReferencingPage = std::move(abyBestCandidateReferencingPage);

    uint64_t nCandidateOffset = ReadFeatureOffset(
        abyPage.data() + nPageHeaderSize + iBestCandidateEntry * nEntrySize + sizeof(uint32_t));

    // Remove entry from page
    if( iBestCandidateEntry < nBestCandidateNumEntries - 1 )
    {
        memmove(abyPage.data() + nPageHeaderSize + iBestCandidateEntry * nEntrySize,
                abyPage.data() + nPageHeaderSize + (iBestCandidateEntry + 1) * nEntrySize,
                (nBestCandidateNumEntries - 1 - iBestCandidateEntry) * nEntrySize);
    }
    memset(abyPage.data() + nPageHeaderSize + (nBestCandidateNumEntries - 1) * nEntrySize,
           0,
           nEntrySize);

    nBestCandidateNumEntries --;
    WriteUInt32(abyPage, nBestCandidateNumEntries, 0);

    if( nBestCandidateNumEntries > 0 )
    {
        // Rewrite updated page
        VSIFSeekL(fp, static_cast<uint64_t>(nPageIdx) * nPageSize, 0);
        CPL_IGNORE_RET_VAL(VSIFWriteL(abyPage.data(), abyPage.size(), 1, fp ));
    }
    else
    {
        const uint32_t nPrevPage = GetUInt32(abyPage.data() + sizeof(uint32_t), 0);

        // Link this newly free page to the previous one
        const uint32_t nLastFreePage = GetUInt32(abyTrailer.data() + sizeof(uint32_t), 0);
        WriteUInt32(abyPage, nLastFreePage, sizeof(uint32_t));

        // Rewrite updated page
        VSIFSeekL(fp, static_cast<uint64_t>(nPageIdx) * nPageSize, 0);
        CPL_IGNORE_RET_VAL(VSIFWriteL(abyPage.data(), abyPage.size(), 1, fp ));

        // Update trailer to add a new free page
        WriteUInt32(abyTrailer, nPageIdx, sizeof(uint32_t));

        if( nReferencingPage != MINUS_ONE )
        {
            // Links referencing page to previous page
            WriteUInt32(abyReferencingPage, nPrevPage, sizeof(uint32_t));
            VSIFSeekL(fp, static_cast<uint64_t>(nReferencingPage) * nPageSize, 0);
            CPL_IGNORE_RET_VAL(VSIFWriteL(abyReferencingPage.data(), abyReferencingPage.size(), 1, fp ));
        }
        else
        {
            // and make the slot points to the previous page
            WriteUInt32(abyTrailer, nPrevPage, nTrailerEntrySize * iSlot);
        }

        uint32_t nPageCount = GetUInt32(abyTrailer.data() + nTrailerEntrySize * iSlot + sizeof(uint32_t), 0);
        if( nPageCount == 0 )
        {
            CPLDebug("OpenFileGDB", "Wrong page count for %s at slot %d",
                     osFilename.c_str(), iSlot);
        }
        else
        {
            nPageCount --;
            WriteUInt32(abyTrailer, nPageCount, nTrailerEntrySize * iSlot + sizeof(uint32_t));
            if( nPageCount == 0 )
            {
                // Check if the freelist no longer contains pages with free slots
                m_bFreelistCanBeDeleted = true;
                for( int i = 1; i < nTrailerSize / nTrailerEntrySize; i++ )
                {
                    if( GetUInt32(abyTrailer.data() + i * nTrailerEntrySize + sizeof(uint32_t), 0) != 0 )
                    {
                        m_bFreelistCanBeDeleted = false;
                        break;
                    }
                }
            }
        }

        VSIFSeekL(fp, nFileSize - nTrailerSize, 0);
        CPL_IGNORE_RET_VAL(VSIFWriteL(abyTrailer.data(), abyTrailer.size(), 1, fp ));
    }

    // Extra precaution: check that the uint32_t at offset nOffset is a
    // negated compatible size
    auto nOffset = nCandidateOffset;
    VSIFSeekL(m_fpTable, nOffset, 0);
    uint32_t nOldSize = 0;
    if( !ReadUInt32(m_fpTable, nOldSize) || (nOldSize >> 31) == 0 )
    {
        nOffset = OFFSET_MINUS_ONE;
    }
    else
    {
        nOldSize = static_cast<uint32_t>(-static_cast<int>(nOldSize));
        if( nOldSize < nSize - sizeof(uint32_t) )
        {
            nOffset = OFFSET_MINUS_ONE;
        }
    }
    if( nOffset == OFFSET_MINUS_ONE )
    {
        CPLDebug("OpenFileGDB", "%s references a free area at offset "
                 CPL_FRMT_GUIB ", but it does not appear to match a deleted "
                 "feature",
                 osFilename.c_str(),
                 static_cast<GUIntBig>(nCandidateOffset));
    }

    VSIFCloseL(fp);
    return nOffset;
}

/************************************************************************/
/*                        CheckFreeListConsistency()                    */
/************************************************************************/

bool FileGDBTable::CheckFreeListConsistency()
{
    const std::string osFilename = CPLResetExtension(m_osFilename.c_str(),
                                                     "freelist");
    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "rb");
    if( fp == nullptr )
        return true;

    // Read trailer
    VSIFSeekL(fp, 0, SEEK_END);
    auto nFileSize = VSIFTellL(fp);

    if( (nFileSize % nPageSize) != nTrailerSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Bad file size");
        VSIFCloseL(fp);
        return false;
    }

    VSIFSeekL(fp, nFileSize - nTrailerSize, SEEK_SET);
    std::vector<GByte> abyTrailer(nTrailerSize);
    if( VSIFReadL(abyTrailer.data(), abyTrailer.size(), 1, fp ) != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read trailer section");
        VSIFCloseL(fp);
        return false;
    }

    if( GetUInt32(abyTrailer.data(), 0) != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unexpected value for first uint32 of trailer section");
        VSIFCloseL(fp);
        return false;
    }

    std::vector<GByte> abyPage(nPageSize);
    std::set<uint32_t> setVisitedPages;

    // Check free pages
    uint32_t nFreePage = GetUInt32(abyTrailer.data() + sizeof(uint32_t), 0);
    while( nFreePage != MINUS_ONE )
    {
        if( setVisitedPages.find(nFreePage) != setVisitedPages.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cyclic page refererencing in free pages");
            VSIFCloseL(fp);
            return false;
        }

        VSIFSeekL(fp, static_cast<uint64_t>(nFreePage) * nPageSize, 0);
        if( VSIFReadL(abyPage.data(), abyPage.size(), 1, fp ) != 1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Can't read freelist page %u", nFreePage);
            VSIFCloseL(fp);
            return false;
        }

        setVisitedPages.insert(nFreePage);

        if( GetUInt32(abyPage.data(), 0) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected value for first uint32 of free page");
            VSIFCloseL(fp);
            return false;
        }

        nFreePage = GetUInt32(abyPage.data() + sizeof(uint32_t), 0);
    }

    // Check active pages
    const int nEntrySize = static_cast<int>(sizeof(uint32_t)) + m_nTablxOffsetSize;
    const int nMaxEntriesPerPage = (nPageSize - nPageHeaderSize) / nEntrySize;

    std::set<uint64_t> aSetOffsets;

    for( int iSlot = 1; iSlot < (nTrailerSize / nTrailerEntrySize); iSlot++ )
    {
        uint32_t nPageIdx = GetUInt32(abyTrailer.data() + iSlot * nTrailerEntrySize, 0);
        uint32_t nActualCount = 0;
        while( nPageIdx != MINUS_ONE )
        {
            if( setVisitedPages.find(nPageIdx) != setVisitedPages.end() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cyclic page refererencing or page referenced more than once");
                VSIFCloseL(fp);
                return false;
            }

            VSIFSeekL(fp, static_cast<uint64_t>(nPageIdx) * nPageSize, 0);
            if( VSIFReadL(abyPage.data(), abyPage.size(), 1, fp ) != 1 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Can't read active page %u", nPageIdx);
                VSIFCloseL(fp);
                return false;
            }

            setVisitedPages.insert(nPageIdx);
            nActualCount ++;

            const uint32_t nEntries = GetUInt32(abyPage.data(), 0);
            if( nEntries == 0 || nEntries > static_cast<uint32_t>(nMaxEntriesPerPage) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unexpected value for entries count of active page %u: %d",
                         nPageIdx, nEntries);
                VSIFCloseL(fp);
                return false;
            }

            for( uint32_t i = 0; i < nEntries; ++i )
            {
                const uint32_t nFreeAreaSize = GetUInt32(
                    abyPage.data() + nPageHeaderSize + i * nEntrySize, 0);
                if( nFreeAreaSize < anHoleSizes[iSlot] ||
                    nFreeAreaSize >= anHoleSizes[iSlot+1] )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Page %u contains free area of unexpected size at entry %u",
                             nPageIdx, i);
                    VSIFCloseL(fp);
                    return false;
                }

                const uint64_t nOffset = ReadFeatureOffset(
                    abyPage.data() + nPageHeaderSize + i * nEntrySize + sizeof(uint32_t));

                VSIFSeekL(m_fpTable, nOffset, 0);
                uint32_t nOldSize = 0;
                if( !ReadUInt32(m_fpTable, nOldSize) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Page %u contains free area that points to invalid offset " CPL_FRMT_GUIB,
                             nPageIdx, static_cast<GUIntBig>(nOffset));
                    VSIFCloseL(fp);
                    return false;
                }
                if( (nOldSize >> 31) == 0 ||
                    (nOldSize = static_cast<uint32_t>(-static_cast<int>(nOldSize))) != nFreeAreaSize - sizeof(uint32_t) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Page %u contains free area that points to dead "
                             "zone at offset " CPL_FRMT_GUIB " of unexpected size: %u",
                             nPageIdx, static_cast<GUIntBig>(nOffset), nOldSize);
                    VSIFCloseL(fp);
                    return false;
                }

                if( aSetOffsets.find(nOffset) != aSetOffsets.end() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Page %u contains free area that points to "
                             "offset " CPL_FRMT_GUIB " already referenced",
                             nPageIdx, static_cast<GUIntBig>(nOffset));
                    VSIFCloseL(fp);
                    return false;
                }
                aSetOffsets.insert(nOffset);
            }

            nPageIdx = GetUInt32(abyPage.data() + sizeof(uint32_t), 0);
        }

        const uint32_t nPageCount = GetUInt32(
            abyTrailer.data() + iSlot * nTrailerEntrySize + sizeof(uint32_t), 0);
        if( nPageCount != nActualCount )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected value for page count of slot %d: %u vs %u",
                     iSlot, nPageCount, nActualCount);
            VSIFCloseL(fp);
            return false;
        }
    }

    const auto nExpectedPageCount = (nFileSize - nTrailerSize ) / nPageSize;
    if( setVisitedPages.size() != nExpectedPageCount )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%u pages have been visited, but there are %u pages in total",
                 static_cast<uint32_t>(setVisitedPages.size()),
                 static_cast<uint32_t>(nExpectedPageCount));
        VSIFCloseL(fp);
        return false;
    }

    VSIFCloseL(fp);
    return true;
}

/************************************************************************/
/*                         DeleteFreeList()                             */
/************************************************************************/

void FileGDBTable::DeleteFreeList()
{
    m_bFreelistCanBeDeleted = false;
    m_nHasFreeList = -1;
    VSIUnlink( CPLResetExtension(m_osFilename.c_str(), "freelist") );
}

} /* namespace OpenFileGDB */
