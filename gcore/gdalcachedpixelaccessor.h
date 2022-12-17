/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fast access to individual pixels in a GDALRasterBand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#ifndef GDAL_CACHED_PIXEL_ACCESSOR_INCLUDED
#define GDAL_CACHED_PIXEL_ACCESSOR_INCLUDED

#include "gdal_priv.h"
#include "cpl_error.h"

#include <algorithm>
#include <array>
#include <vector>

/************************************************************************/
/*                      GDALCachedPixelAccessor                         */
/************************************************************************/

/** Class to have reasonably fast random pixel access to a raster band, when
 * accessing multiple pixels that are close to each other.
 *
 * This gives faster access than using GDALRasterBand::RasterIO() with
 * a 1x1 window.
 *
 * @since GDAL 3.5
 */
template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT = 4>
class GDALCachedPixelAccessor
{
    GDALRasterBand *m_poBand = nullptr;

    struct CachedTile
    {
        std::vector<Type> m_data{};
        int m_nTileX = -1;
        int m_nTileY = -1;
        bool m_bModified = false;
    };

    int m_nCachedTileCount = 0;
    std::array<CachedTile, CACHED_TILE_COUNT> m_aCachedTiles{};

    bool LoadTile(int nTileX, int nTileY);
    bool FlushTile(int iSlot);

    Type GetSlowPath(int nTileX, int nTileY, int nXInTile, int nYInTile,
                     bool *pbSuccess);
    bool SetSlowPath(int nTileX, int nTileY, int nXInTile, int nYInTile,
                     Type val);

    GDALCachedPixelAccessor(const GDALCachedPixelAccessor &) = delete;
    GDALCachedPixelAccessor &
    operator=(const GDALCachedPixelAccessor &) = delete;

  public:
    explicit GDALCachedPixelAccessor(GDALRasterBand *poBand);
    ~GDALCachedPixelAccessor();

    /** Assign the raster band if not known at construction time. */
    void SetBand(GDALRasterBand *poBand)
    {
        m_poBand = poBand;
    }

    Type Get(int nX, int nY, bool *pbSuccess = nullptr);
    bool Set(int nX, int nY, Type val);

    bool FlushCache();
    void ResetModifiedFlag();
};

/************************************************************************/
/*                      GDALCachedPixelAccessor()                       */
/************************************************************************/

/** Constructor.
 *
 * The template accepts the following parameters:
 * - Type: should be one of GByte, GUInt16, GInt16, GUInt32, GInt32, GUInt64,
 * GInt64, float or double
 * - TILE_SIZE: the tile size for the cache of GDALCachedPixelAccessor.
 *              Use a power of two for faster computation.
 *              It doesn't need to be the same of the underlying raster
 * - CACHED_TILE_COUNT: number of tiles to cache. Should be >= 1. Defaults to 4.
 *
 * @param poBand Raster band.
 */
template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
GDALCachedPixelAccessor<Type, TILE_SIZE, CACHED_TILE_COUNT>::
    GDALCachedPixelAccessor(GDALRasterBand *poBand)
    : m_poBand(poBand)
{
}

/************************************************************************/
/*                     ~GDALCachedPixelAccessor()                       */
/************************************************************************/

/** Destructor.
 *
 * Will call FlushCache()
 */
template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
GDALCachedPixelAccessor<Type, TILE_SIZE,
                        CACHED_TILE_COUNT>::~GDALCachedPixelAccessor()
{
    FlushCache();
}

/************************************************************************/
/*                            Get()                                     */
/************************************************************************/

/** Get the value of a pixel.
 *
 * No bound checking of nX, nY is done.
 *
 * @param nX X coordinate (between 0 and GetXSize()-1)
 * @param nY Y coordinate (between 0 and GetYSize()-1)
 * @param[out] pbSuccess Optional pointer to a success flag
 * @return the pixel value
 */
template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
inline Type GDALCachedPixelAccessor<Type, TILE_SIZE, CACHED_TILE_COUNT>::Get(
    int nX, int nY, bool *pbSuccess)
{
    const int nTileX = nX / TILE_SIZE;
    const int nTileY = nY / TILE_SIZE;
    const int nXInTile = nX % TILE_SIZE;
    const int nYInTile = nY % TILE_SIZE;
    if (m_aCachedTiles[0].m_nTileX == nTileX &&
        m_aCachedTiles[0].m_nTileY == nTileY)
    {
        if (pbSuccess)
            *pbSuccess = true;
        return m_aCachedTiles[0].m_data[nYInTile * TILE_SIZE + nXInTile];
    }
    return GetSlowPath(nTileX, nTileY, nXInTile, nYInTile, pbSuccess);
}

/************************************************************************/
/*                       GetSlowPath()                                  */
/************************************************************************/

template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
Type GDALCachedPixelAccessor<Type, TILE_SIZE, CACHED_TILE_COUNT>::GetSlowPath(
    int nTileX, int nTileY, int nXInTile, int nYInTile, bool *pbSuccess)
{
    for (int i = 1; i < m_nCachedTileCount; ++i)
    {
        const auto &cachedTile = m_aCachedTiles[i];
        if (cachedTile.m_nTileX == nTileX && cachedTile.m_nTileY == nTileY)
        {
            const auto ret = cachedTile.m_data[nYInTile * TILE_SIZE + nXInTile];
            CachedTile tmp = std::move(m_aCachedTiles[i]);
            for (int j = i; j >= 1; --j)
                m_aCachedTiles[j] = std::move(m_aCachedTiles[j - 1]);
            m_aCachedTiles[0] = std::move(tmp);
            if (pbSuccess)
                *pbSuccess = true;
            return ret;
        }
    }
    if (!LoadTile(nTileX, nTileY))
    {
        if (pbSuccess)
            *pbSuccess = false;
        return 0;
    }
    if (pbSuccess)
        *pbSuccess = true;
    return m_aCachedTiles[0].m_data[nYInTile * TILE_SIZE + nXInTile];
}

/************************************************************************/
/*                            Set()                                     */
/************************************************************************/

/** Set the value of a pixel.
 *
 * The actual modification of the underlying raster is deferred until the tile
 * is implicit flushed while loading a new tile, or an explicit call to
 * FlushCache().
 *
 * The destructor of GDALCachedPixelAccessor will take care of calling
 * FlushCache(), if the user hasn't done it explicitly.
 *
 * No bound checking of nX, nY is done.
 *
 * @param nX X coordinate (between 0 and GetXSize()-1)
 * @param nY Y coordinate (between 0 and GetYSize()-1)
 * @param val pixel value
 * @return true if success
 */
template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
inline bool
GDALCachedPixelAccessor<Type, TILE_SIZE, CACHED_TILE_COUNT>::Set(int nX, int nY,
                                                                 Type val)
{
    const int nTileX = nX / TILE_SIZE;
    const int nTileY = nY / TILE_SIZE;
    const int nXInTile = nX % TILE_SIZE;
    const int nYInTile = nY % TILE_SIZE;
    if (m_aCachedTiles[0].m_nTileX == nTileX &&
        m_aCachedTiles[0].m_nTileY == nTileY)
    {
        m_aCachedTiles[0].m_data[nYInTile * TILE_SIZE + nXInTile] = val;
        m_aCachedTiles[0].m_bModified = true;
        return true;
    }
    return SetSlowPath(nTileX, nTileY, nXInTile, nYInTile, val);
}

/************************************************************************/
/*                         SetSlowPath()                                */
/************************************************************************/

template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
bool GDALCachedPixelAccessor<Type, TILE_SIZE, CACHED_TILE_COUNT>::SetSlowPath(
    int nTileX, int nTileY, int nXInTile, int nYInTile, Type val)
{
    for (int i = 1; i < m_nCachedTileCount; ++i)
    {
        auto &cachedTile = m_aCachedTiles[i];
        if (cachedTile.m_nTileX == nTileX && cachedTile.m_nTileY == nTileY)
        {
            cachedTile.m_data[nYInTile * TILE_SIZE + nXInTile] = val;
            cachedTile.m_bModified = true;
            if (i > 0)
            {
                CachedTile tmp = std::move(m_aCachedTiles[i]);
                for (int j = i; j >= 1; --j)
                    m_aCachedTiles[j] = std::move(m_aCachedTiles[j - 1]);
                m_aCachedTiles[0] = std::move(tmp);
            }
            return true;
        }
    }
    if (!LoadTile(nTileX, nTileY))
    {
        return false;
    }
    m_aCachedTiles[0].m_data[nYInTile * TILE_SIZE + nXInTile] = val;
    m_aCachedTiles[0].m_bModified = true;
    return true;
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

/** Flush content of modified tiles and drop caches
 *
 * @return true if success
 */
template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
bool GDALCachedPixelAccessor<Type, TILE_SIZE, CACHED_TILE_COUNT>::FlushCache()
{
    bool bRet = true;
    for (int i = 0; i < m_nCachedTileCount; ++i)
    {
        if (!FlushTile(i))
            bRet = false;
        m_aCachedTiles[i].m_nTileX = -1;
        m_aCachedTiles[i].m_nTileY = -1;
    }
    return bRet;
}

/************************************************************************/
/*                      ResetModifiedFlag()                             */
/************************************************************************/

/** Reset the modified flag for cached tiles.
 */
template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
void GDALCachedPixelAccessor<Type, TILE_SIZE,
                             CACHED_TILE_COUNT>::ResetModifiedFlag()
{
    for (int i = 0; i < m_nCachedTileCount; ++i)
    {
        m_aCachedTiles[i].m_bModified = false;
    }
}

/************************************************************************/
/*                 GDALCachedPixelAccessorGetDataType                   */
/************************************************************************/

/*! @cond Doxygen_Suppress */
template <class T> struct GDALCachedPixelAccessorGetDataType
{
};

template <> struct GDALCachedPixelAccessorGetDataType<GByte>
{
    static constexpr GDALDataType DataType = GDT_Byte;
};
template <> struct GDALCachedPixelAccessorGetDataType<GInt8>
{
    static constexpr GDALDataType DataType = GDT_Int8;
};
template <> struct GDALCachedPixelAccessorGetDataType<GUInt16>
{
    static constexpr GDALDataType DataType = GDT_UInt16;
};
template <> struct GDALCachedPixelAccessorGetDataType<GInt16>
{
    static constexpr GDALDataType DataType = GDT_Int16;
};
template <> struct GDALCachedPixelAccessorGetDataType<GUInt32>
{
    static constexpr GDALDataType DataType = GDT_UInt32;
};
template <> struct GDALCachedPixelAccessorGetDataType<GInt32>
{
    static constexpr GDALDataType DataType = GDT_Int32;
};
#if SIZEOF_UNSIGNED_LONG == 8
// std::uint64_t on Linux 64-bit resolves as unsigned long
template <> struct GDALCachedPixelAccessorGetDataType<unsigned long>
{
    static constexpr GDALDataType DataType = GDT_UInt64;
};
template <> struct GDALCachedPixelAccessorGetDataType<long>
{
    static constexpr GDALDataType DataType = GDT_Int64;
};
#endif
template <> struct GDALCachedPixelAccessorGetDataType<GUInt64>
{
    static constexpr GDALDataType DataType = GDT_UInt64;
};
template <> struct GDALCachedPixelAccessorGetDataType<GInt64>
{
    static constexpr GDALDataType DataType = GDT_Int64;
};
template <> struct GDALCachedPixelAccessorGetDataType<float>
{
    static constexpr GDALDataType DataType = GDT_Float32;
};
template <> struct GDALCachedPixelAccessorGetDataType<double>
{
    static constexpr GDALDataType DataType = GDT_Float64;
};
/*! @endcond */

/************************************************************************/
/*                          LoadTile()                                  */
/************************************************************************/

template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
bool GDALCachedPixelAccessor<Type, TILE_SIZE, CACHED_TILE_COUNT>::LoadTile(
    int nTileX, int nTileY)
{
    if (m_nCachedTileCount == CACHED_TILE_COUNT)
    {
        if (!FlushTile(CACHED_TILE_COUNT - 1))
            return false;
        CachedTile tmp = std::move(m_aCachedTiles[CACHED_TILE_COUNT - 1]);
        for (int i = CACHED_TILE_COUNT - 1; i >= 1; --i)
            m_aCachedTiles[i] = std::move(m_aCachedTiles[i - 1]);
        m_aCachedTiles[0] = std::move(tmp);
    }
    else
    {
        if (m_nCachedTileCount > 0)
            std::swap(m_aCachedTiles[0], m_aCachedTiles[m_nCachedTileCount]);
        m_aCachedTiles[0].m_data.resize(TILE_SIZE * TILE_SIZE);
        m_nCachedTileCount++;
    }

#if 0
    CPLDebug("GDAL", "Load tile(%d, %d) of band %d of dataset %s",
              nTileX, nTileY, m_poBand->GetBand(),
              m_poBand->GetDataset() ? m_poBand->GetDataset()->GetDescription() : "(unknown)");
#endif
    CPLAssert(!m_aCachedTiles[0].m_bModified);
    const int nXOff = nTileX * TILE_SIZE;
    const int nYOff = nTileY * TILE_SIZE;
    const int nReqXSize = std::min(m_poBand->GetXSize() - nXOff, TILE_SIZE);
    const int nReqYSize = std::min(m_poBand->GetYSize() - nYOff, TILE_SIZE);
    if (m_poBand->RasterIO(
            GF_Read, nXOff, nYOff, nReqXSize, nReqYSize,
            m_aCachedTiles[0].m_data.data(), nReqXSize, nReqYSize,
            GDALCachedPixelAccessorGetDataType<Type>::DataType, sizeof(Type),
            TILE_SIZE * sizeof(Type), nullptr) != CE_None)
    {
        m_aCachedTiles[0].m_nTileX = -1;
        m_aCachedTiles[0].m_nTileY = -1;
        return false;
    }
    m_aCachedTiles[0].m_nTileX = nTileX;
    m_aCachedTiles[0].m_nTileY = nTileY;
    return true;
}

/************************************************************************/
/*                          FlushTile()                                 */
/************************************************************************/

template <class Type, int TILE_SIZE, int CACHED_TILE_COUNT>
bool GDALCachedPixelAccessor<Type, TILE_SIZE, CACHED_TILE_COUNT>::FlushTile(
    int iSlot)
{
    if (!m_aCachedTiles[iSlot].m_bModified)
        return true;

    m_aCachedTiles[iSlot].m_bModified = false;
    const int nXOff = m_aCachedTiles[iSlot].m_nTileX * TILE_SIZE;
    const int nYOff = m_aCachedTiles[iSlot].m_nTileY * TILE_SIZE;
    const int nReqXSize = std::min(m_poBand->GetXSize() - nXOff, TILE_SIZE);
    const int nReqYSize = std::min(m_poBand->GetYSize() - nYOff, TILE_SIZE);
    return m_poBand->RasterIO(
               GF_Write, nXOff, nYOff, nReqXSize, nReqYSize,
               m_aCachedTiles[iSlot].m_data.data(), nReqXSize, nReqYSize,
               GDALCachedPixelAccessorGetDataType<Type>::DataType, sizeof(Type),
               TILE_SIZE * sizeof(Type), nullptr) == CE_None;
}

#endif  // GDAL_PIXEL_ACCESSOR_INCLUDED
