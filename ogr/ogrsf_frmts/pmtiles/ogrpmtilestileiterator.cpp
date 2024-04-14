/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Planet Labs
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

#include "ogr_pmtiles.h"

#include <algorithm>
#include <limits>

/************************************************************************/
/*                 find_tile_idx_lesser_or_equal()                      */
/************************************************************************/

static int
find_tile_idx_lesser_or_equal(const std::vector<pmtiles::entryv3> &entries,
                              uint64_t tile_id)
{
    if (!entries.empty() && tile_id <= entries[0].tile_id)
        return 0;

    int m = 0;
    int n = static_cast<int>(entries.size()) - 1;
    while (m <= n)
    {
        int k = (n + m) >> 1;
        if (tile_id > entries[k].tile_id)
        {
            m = k + 1;
        }
        else if (tile_id < entries[k].tile_id)
        {
            n = k - 1;
        }
        else
        {
            return k;
        }
    }

    return n;
}

/************************************************************************/
/*                      LoadRootDirectory()                             */
/************************************************************************/

bool OGRPMTilesTileIterator::LoadRootDirectory()
{
    if (m_nZoomLevel >= 0)
    {
        CPLDebugOnly("PMTiles", "minx=%d miny=%d maxx=%d maxy=%d", m_nMinX,
                     m_nMinY, m_nMaxX, m_nMaxY);
        // If we don't query too many tiles, establish the minimum
        // and maximum tile id, we are interested in.
        // (is there a clever way of figuring out this?)
        if (m_nMinX >= 0 && m_nMinY >= 0 && m_nMaxX >= m_nMinX &&
            m_nMaxY >= m_nMinY &&
            (m_nMaxX - m_nMinX + 1) <= 100 / (m_nMaxY - m_nMinY + 1))
        {
            for (int iY = m_nMinY; iY <= m_nMaxY; ++iY)
            {
                for (int iX = m_nMinX; iX <= m_nMaxX; ++iX)
                {
                    const uint64_t nTileId = pmtiles::zxy_to_tileid(
                        static_cast<uint8_t>(m_nZoomLevel), iX, iY);
                    m_nMinTileId = std::min(m_nMinTileId, nTileId);
                    m_nMaxTileId = std::max(m_nMaxTileId, nTileId);
                }
            }
        }
        else
        {
            m_nMinTileId = pmtiles::zxy_to_tileid(
                static_cast<uint8_t>(m_nZoomLevel), 0, 0);
            m_nMaxTileId = pmtiles::zxy_to_tileid(
                               static_cast<uint8_t>(m_nZoomLevel) + 1, 0, 0) -
                           1;
        }

        // If filtering by bbox and that the gap between min and max
        // tile id is too big, use an iteration over (x, y) space rather
        // than tile id space.

        // Config option for debugging purposes
        const unsigned nThreshold = static_cast<unsigned>(atoi(
            CPLGetConfigOption("OGR_PMTILES_ITERATOR_THRESHOLD", "10000")));
        if (m_nMinX >= 0 && m_nMinY >= 0 && m_nMaxX >= m_nMinX &&
            m_nMaxY >= m_nMinY &&
            !(m_nMinX == 0 && m_nMinY == 0 &&
              m_nMaxX == (1 << m_nZoomLevel) - 1 &&
              m_nMaxY == (1 << m_nZoomLevel) - 1) &&
            m_nMaxTileId - m_nMinTileId > nThreshold)
        {
            m_nCurX = m_nMinX;
            m_nCurY = m_nMinY;
            m_nMinTileId = pmtiles::zxy_to_tileid(
                static_cast<uint8_t>(m_nZoomLevel), m_nCurX, m_nCurY);
            m_nMaxTileId = m_nMinTileId;
        }
    }

    const auto &sHeader = m_poDS->GetHeader();
    const auto *posStr = m_poDS->ReadInternal(
        sHeader.root_dir_offset, static_cast<uint32_t>(sHeader.root_dir_bytes),
        "header");
    if (!posStr)
    {
        return false;
    }

    DirectoryContext sContext;
    sContext.sEntries = pmtiles::deserialize_directory(*posStr);

    if (m_nZoomLevel >= 0)
    {
        if (m_nCurX >= 0)
        {
            while (true)
            {
                const int nMinEntryIdx = find_tile_idx_lesser_or_equal(
                    sContext.sEntries, m_nMinTileId);
                if (nMinEntryIdx < 0)
                {
                    m_nCurX++;
                    if (m_nCurX > m_nMaxX)
                    {
                        m_nCurX = m_nMinX;
                        m_nCurY++;
                        if (m_nCurY > m_nMaxY)
                        {
                            return false;
                        }
                    }
                    m_nMinTileId = pmtiles::zxy_to_tileid(
                        static_cast<uint8_t>(m_nZoomLevel), m_nCurX, m_nCurY);
                    m_nMaxTileId = m_nMinTileId;
                }
                else
                {
                    sContext.nIdxInEntries = nMinEntryIdx;
                    break;
                }
            }
        }
        else
        {
            const int nMinEntryIdx =
                find_tile_idx_lesser_or_equal(sContext.sEntries, m_nMinTileId);
            if (nMinEntryIdx < 0)
            {
                return false;
            }
            sContext.nIdxInEntries = nMinEntryIdx;
        }
    }

    m_aoStack.emplace(std::move(sContext));
    return true;
}

/************************************************************************/
/*                        SkipRunLength()                               */
/************************************************************************/

void OGRPMTilesTileIterator::SkipRunLength()
{
    if (!m_aoStack.empty())
    {
        auto &topContext = m_aoStack.top();
        if (topContext.nIdxInEntries < topContext.sEntries.size())
        {
            const auto &sCurrentEntry =
                topContext.sEntries[topContext.nIdxInEntries];
            if (sCurrentEntry.run_length > 1)
            {
                m_nLastTileId =
                    sCurrentEntry.tile_id + sCurrentEntry.run_length - 1;
                topContext.nIdxInRunLength = sCurrentEntry.run_length;
            }
        }
    }
}

/************************************************************************/
/*                          GetNextTile()                               */
/************************************************************************/

pmtiles::entry_zxy OGRPMTilesTileIterator::GetNextTile(uint32_t *pnRunLength)
{
    if (m_bEOF)
        return pmtiles::entry_zxy(0, 0, 0, 0, 0);

    const auto &sHeader = m_poDS->GetHeader();
    try
    {
        // Put the root directory as the first element of the stack
        // of directories, if the stack is empty
        if (m_aoStack.empty())
        {
            if (!LoadRootDirectory())
            {
                m_bEOF = true;
                return pmtiles::entry_zxy(0, 0, 0, 0, 0);
            }
        }

        const auto AdvanceToNextTile = [this]()
        {
            if (m_nCurX >= 0)
            {
                while (true)
                {
                    m_nCurX++;
                    if (m_nCurX > m_nMaxX)
                    {
                        m_nCurX = m_nMinX;
                        m_nCurY++;
                        if (m_nCurY > m_nMaxY)
                        {
                            m_bEOF = true;
                            return false;
                        }
                    }
                    if (!m_bEOF)
                    {
                        m_nMinTileId = pmtiles::zxy_to_tileid(
                            static_cast<uint8_t>(m_nZoomLevel), m_nCurX,
                            m_nCurY);
                        m_nMaxTileId = m_nMinTileId;
                        m_nLastTileId = INVALID_LAST_TILE_ID;
                        while (m_aoStack.size() > 1)
                            m_aoStack.pop();
                        const int nMinEntryIdx = find_tile_idx_lesser_or_equal(
                            m_aoStack.top().sEntries, m_nMinTileId);
                        if (nMinEntryIdx < 0)
                        {
                            continue;
                        }
                        m_aoStack.top().nIdxInEntries = nMinEntryIdx;
                        m_aoStack.top().nIdxInRunLength = 0;
                        break;
                    }
                }
                return true;
            }
            return false;
        };

        while (true)
        {
            if (m_aoStack.top().nIdxInEntries ==
                m_aoStack.top().sEntries.size())
            {
                if (m_aoStack.size() == 1 && AdvanceToNextTile())
                    continue;

                m_aoStack.pop();
                if (m_aoStack.empty())
                    break;
                continue;
            }
            auto &topContext = m_aoStack.top();
            const auto &sCurrentEntry =
                topContext.sEntries[topContext.nIdxInEntries];
            if (sCurrentEntry.run_length == 0)
            {
                // Arbitrary limit. 5 seems to be the maximum value supported
                // by pmtiles.hpp::get_tile()
                if (m_aoStack.size() == 5)
                {
                    m_bEOF = true;
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Too many levels of nested directories");
                    break;
                }

                if (sHeader.leaf_dirs_offset >
                    std::numeric_limits<uint64_t>::max() - sCurrentEntry.offset)
                {
                    m_bEOF = true;
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid directory offset");
                    break;
                }
                const auto *posStr = m_poDS->ReadInternal(
                    sHeader.leaf_dirs_offset + sCurrentEntry.offset,
                    sCurrentEntry.length, "directory");
                if (!posStr)
                {
                    m_bEOF = true;
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "PMTILES: cannot read directory of size " CPL_FRMT_GUIB
                        " at "
                        "offset " CPL_FRMT_GUIB,
                        static_cast<GUIntBig>(sHeader.leaf_dirs_offset +
                                              sCurrentEntry.offset),
                        static_cast<GUIntBig>(sCurrentEntry.length));
                    break;
                }

                DirectoryContext sContext;
                sContext.sEntries = pmtiles::deserialize_directory(*posStr);
                if (sContext.sEntries.empty())
                {
                    m_bEOF = true;
                    // In theory empty directories could exist, but for now
                    // do not allow this to be more robust against hostile files
                    // that could create many such empty directories
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Empty directory found");
                    break;
                }

                if (m_nLastTileId != INVALID_LAST_TILE_ID &&
                    sContext.sEntries[0].tile_id <= m_nLastTileId)
                {
                    m_bEOF = true;
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Non increasing tile_id");
                    break;
                }

                if (m_nZoomLevel >= 0)
                {
                    const int nMinEntryIdx = find_tile_idx_lesser_or_equal(
                        sContext.sEntries, m_nMinTileId);
                    if (nMinEntryIdx < 0)
                    {
                        if (AdvanceToNextTile())
                            continue;
                        break;
                    }
                    sContext.nIdxInEntries = nMinEntryIdx;
                }
                m_nLastTileId =
                    sContext.sEntries[sContext.nIdxInEntries].tile_id;

                m_aoStack.emplace(std::move(sContext));

                topContext.nIdxInEntries++;
            }
            else
            {
                if (topContext.nIdxInRunLength == sCurrentEntry.run_length)
                {
                    topContext.nIdxInEntries++;
                    topContext.nIdxInRunLength = 0;
                }
                else
                {
                    const auto nIdxInRunLength = topContext.nIdxInRunLength;
                    const uint64_t nTileId =
                        sCurrentEntry.tile_id + nIdxInRunLength;
                    m_nLastTileId = nTileId;
                    const pmtiles::zxy zxy = pmtiles::tileid_to_zxy(nTileId);

                    // Sanity check to limit risk of iterating forever on
                    // broken run_length value
                    if (nIdxInRunLength == 0 && sCurrentEntry.run_length > 1 &&
                        sCurrentEntry.run_length >
                            pmtiles::zxy_to_tileid(zxy.z + 1, 0, 0) - nTileId)
                    {
                        m_bEOF = true;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Invalid run_length");
                        break;
                    }

                    topContext.nIdxInRunLength++;

                    if (m_nZoomLevel >= 0)
                    {
                        if (nTileId < m_nMinTileId)
                        {
                            if (sCurrentEntry.run_length > 1)
                            {
                                if (sCurrentEntry.tile_id +
                                        sCurrentEntry.run_length <=
                                    m_nMinTileId)
                                {
                                    topContext.nIdxInRunLength =
                                        sCurrentEntry.run_length;
                                }
                                else
                                {
                                    topContext.nIdxInRunLength =
                                        static_cast<uint32_t>(
                                            m_nMinTileId -
                                            sCurrentEntry.tile_id);
                                }
                                m_nLastTileId = sCurrentEntry.tile_id +
                                                topContext.nIdxInRunLength - 1;
                            }
                            continue;
                        }
                        else if (nTileId > m_nMaxTileId)
                        {
                            if (AdvanceToNextTile())
                                continue;
                            break;
                        }

                        if (m_nMinX >= 0 &&
                            zxy.x < static_cast<uint32_t>(m_nMinX))
                            continue;
                        if (m_nMinY >= 0 &&
                            zxy.y < static_cast<uint32_t>(m_nMinY))
                            continue;
                        if (m_nMaxX >= 0 &&
                            zxy.x > static_cast<uint32_t>(m_nMaxX))
                            continue;
                        if (m_nMaxY >= 0 &&
                            zxy.y > static_cast<uint32_t>(m_nMaxY))
                            continue;
                    }

                    if (sHeader.tile_data_offset >
                        std::numeric_limits<uint64_t>::max() -
                            sCurrentEntry.offset)
                    {
                        m_bEOF = true;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Invalid tile offset");
                        break;
                    }

                    if (pnRunLength)
                    {
                        *pnRunLength =
                            sCurrentEntry.run_length - nIdxInRunLength;
                    }

                    // We must capture the result, before the below code
                    // that updates (m_nCurX, m_nCurY) and invalidates
                    // sCurrentEntry
                    const auto res = pmtiles::entry_zxy(
                        zxy.z, zxy.x, zxy.y,
                        sHeader.tile_data_offset + sCurrentEntry.offset,
                        sCurrentEntry.length);

                    AdvanceToNextTile();

                    return res;
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetNextTile() failed with %s",
                 e.what());
    }

    m_bEOF = true;
    return pmtiles::entry_zxy(0, 0, 0, 0, 0);
}

/************************************************************************/
/*                           DumpTiles()                                */
/************************************************************************/

#ifdef DEBUG_PMTILES
void OGRPMTilesTileIterator::DumpTiles()
{
    int count = 0;
    while (true)
    {
        const auto sTile = GetNextTile();
        if (sTile.offset == 0)
            break;
        ++count;
        printf("%d -> %d %d %d %d %d\n",  // ok
               count, int(sTile.z), int(sTile.x), int(sTile.y),
               int(sTile.offset), int(sTile.length));
    }
}
#endif
