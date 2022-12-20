/*
 * Copyright (c) 2002-2012, California Institute of Technology.
 * All rights reserved.  Based on Government Sponsored Research under contracts
 * NAS7-1407 and/or NAS7-03001.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the California Institute of Technology (Caltech),
 * its operating division the Jet Propulsion Laboratory (JPL), the National
 * Aeronautics and Space Administration (NASA), nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE CALIFORNIA INSTITUTE OF TECHNOLOGY BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright 2014-2021 Esri
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/******************************************************************************
 *
 * Project:  Meta Raster File Format Driver Implementation, RasterBand
 * Purpose:  Implementation of MRF band
 *
 * Author:   Lucian Plesea, Lucian.Plesea jpl.nasa.gov, lplesea esri.com
 *
 ****************************************************************************/

#include "marfa.h"
#include "gdal_priv.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"

#include <vector>
#include <cassert>
#include <zlib.h>
#if defined(ZSTD_SUPPORT)
#include <zstd.h>
#endif

using namespace std::chrono;

NAMESPACE_MRF_START

// packs a block of a given type, with a stride
// Count is the number of items that need to be copied
// These are separate to allow for optimization

template <typename T>
static void cpy_stride_in(void *dst, void *src, int c, int stride)
{
    T *s = reinterpret_cast<T *>(src);
    T *d = reinterpret_cast<T *>(dst);

    while (c--)
    {
        *d++ = *s;
        s += stride;
    }
}

template <typename T>
static void cpy_stride_out(void *dst, void *src, int c, int stride)
{
    T *s = reinterpret_cast<T *>(src);
    T *d = reinterpret_cast<T *>(dst);

    while (c--)
    {
        *d = *s++;
        d += stride;
    }
}

// Does every value in the buffer have the same value, using strict comparison
template <typename T>
inline int isAllVal(const T *b, size_t bytecount, double ndv)
{
    T val = static_cast<T>(ndv);
    size_t count = bytecount / sizeof(T);
    for (; count; --count)
    {
        if (*(b++) != val)
        {
            return FALSE;
        }
    }
    return TRUE;
}

// Dispatcher based on gdal data type
static int isAllVal(GDALDataType gt, void *b, size_t bytecount, double ndv)
{
    // Test to see if it has data
    int isempty = false;

    // A case branch in a temporary macro, conversion from gdal enum to type
#define TEST_T(GType, T)                                                       \
    case GType:                                                                \
        isempty = isAllVal(reinterpret_cast<T *>(b), bytecount, ndv);          \
        break

    switch (gt)
    {
        TEST_T(GDT_Byte, GByte);
        TEST_T(GDT_UInt16, GUInt16);
        TEST_T(GDT_Int16, GInt16);
        TEST_T(GDT_UInt32, GUInt32);
        TEST_T(GDT_Int32, GInt32);
        TEST_T(GDT_Float32, float);
        TEST_T(GDT_Float64, double);
        default:
            break;
    }
#undef TEST_T

    return isempty;
}

// Swap bytes in place, unconditional
static void swab_buff(buf_mgr &src, const ILImage &img)
{
    size_t i;
    switch (GDALGetDataTypeSize(img.dt))
    {
        case 16:
        {
            short int *b = (short int *)src.buffer;
            for (i = src.size / 2; i; b++, i--)
                *b = swab16(*b);
            break;
        }
        case 32:
        {
            int *b = (int *)src.buffer;
            for (i = src.size / 4; i; b++, i--)
                *b = swab32(*b);
            break;
        }
        case 64:
        {
            long long *b = (long long *)src.buffer;
            for (i = src.size / 8; i; b++, i--)
                *b = swab64(*b);
            break;
        }
    }
}

// Similar to compress2() but with flags to control zlib features
// Returns true if it worked
static int ZPack(const buf_mgr &src, buf_mgr &dst, int flags)
{
    z_stream stream;
    int err;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef *)src.buffer;
    stream.avail_in = (uInt)src.size;
    stream.next_out = (Bytef *)dst.buffer;
    stream.avail_out = (uInt)dst.size;

    int level = flags & ZFLAG_LMASK;
    if (level > 9)
        level = 9;
    if (level < 1)
        level = 1;
    int wb = MAX_WBITS;
    // if gz flag is set, ignore raw request
    if (flags & ZFLAG_GZ)
        wb += 16;
    else if (flags & ZFLAG_RAW)
        wb = -wb;
    int memlevel = 8;  // Good compromise
    int strategy = (flags & ZFLAG_SMASK) >> 6;
    if (strategy > 4)
        strategy = 0;

    err = deflateInit2(&stream, level, Z_DEFLATED, wb, memlevel, strategy);
    if (err != Z_OK)
        return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END)
    {
        deflateEnd(&stream);
        return false;
    }
    dst.size = stream.total_out;
    err = deflateEnd(&stream);
    return err == Z_OK;
}

// Similar to uncompress() from zlib, accepts the ZFLAG_RAW
// Return true if it worked
static int ZUnPack(const buf_mgr &src, buf_mgr &dst, int flags)
{

    z_stream stream;
    int err;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef *)src.buffer;
    stream.avail_in = (uInt)src.size;
    stream.next_out = (Bytef *)dst.buffer;
    stream.avail_out = (uInt)dst.size;

    // 32 means autodetec gzip or zlib header, negative 15 is for raw
    int wb = (ZFLAG_RAW & flags) ? -MAX_WBITS : 32 + MAX_WBITS;
    err = inflateInit2(&stream, wb);
    if (err != Z_OK)
        return false;

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END)
    {
        inflateEnd(&stream);
        return false;
    }
    dst.size = stream.total_out;
    err = inflateEnd(&stream);
    return err == Z_OK;
}

/*
 * Deflates a buffer, extrasize is the available size in the buffer past the
 * input If the output fits past the data, it uses that area, otherwise it uses
 * a temporary buffer and copies the data over the input on return, returning a
 * pointer to it. The output size is returned in src.size Returns nullptr when
 * compression failed
 */
static void *DeflateBlock(buf_mgr &src, size_t extrasize, int flags)
{
    // The one we might need to allocate
    void *dbuff = nullptr;
    buf_mgr dst = {src.buffer + src.size, extrasize};

    // Allocate a temp buffer if there is not sufficient space,
    // We need to have a bit more than half the buffer available
    if (extrasize < (src.size + 64))
    {
        dst.size = src.size + 64;
        dbuff = VSIMalloc(dst.size);
        dst.buffer = (char *)dbuff;
        if (!dst.buffer)
            return nullptr;
    }

    if (!ZPack(src, dst, flags))
    {
        CPLFree(dbuff);  // Safe to call with NULL
        return nullptr;
    }
    if (dst.size > src.size)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "DeflateBlock(): dst.size > src.size");
        CPLFree(dbuff);  // Safe to call with NULL
        return nullptr;
    }

    // source size is used to hold the output size
    src.size = dst.size;
    // If we didn't allocate a buffer, the receiver can use it already
    if (!dbuff)
        return dst.buffer;

    // If we allocated a buffer, we need to copy the data to the input buffer.
    memcpy(src.buffer, dbuff, src.size);
    CPLFree(dbuff);
    return src.buffer;
}

#if defined(ZSTD_SUPPORT)

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW static void rankfilter(buf_mgr &src,
                                                            size_t factor)
{
    // Arange bytes by rank
    if (factor > 1)
    {
        std::vector<char> tempb(src.size);
        char *d = tempb.data();
        for (size_t j = 0; j < factor; j++)
            for (size_t i = j; i < src.size; i += factor)
                *d++ = src.buffer[i];
        memcpy(src.buffer, tempb.data(), src.size);
    }
    // byte delta
    auto p = reinterpret_cast<GByte *>(src.buffer);
    auto guard = p + src.size;
    GByte b(0);
    while (p < guard)
    {
        GByte temp = *p;
        *p -= b;
        b = temp;
        p++;
    }
}

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW static void derank(buf_mgr &src,
                                                        size_t factor)
{
    // undo delta
    auto p = reinterpret_cast<GByte *>(src.buffer);
    auto guard = p + src.size;
    GByte b(0);
    while (p < guard)
    {
        b += *p;
        *p = b;
        p++;
    }
    if (factor > 1)
    {  // undo rank separation
        std::vector<char> tempb(src.size);
        char *d = tempb.data();
        size_t chunk = src.size / factor;
        for (size_t i = 0; i < chunk; i++)
            for (size_t j = 0; j < factor; j++)
                *d++ = src.buffer[chunk * j + i];
        memcpy(src.buffer, tempb.data(), src.size);
    }
}

/*
 * Compress a buffer using zstd, extrasize is the available size in the buffer
 * past the input If ranks > 0, apply the rank filter If the output fits past
 * the data, it uses that area, otherwise it uses a temporary buffer and copies
 * the data over the input on return, returning a pointer to it. The output size
 * is returned in src.size Returns nullptr when compression failed
 */
static void *ZstdCompBlock(buf_mgr &src, size_t extrasize, int c_level,
                           ZSTD_CCtx *cctx, size_t ranks)
{
    if (!cctx)
        return nullptr;
    if (ranks && (src.size % ranks) == 0)
        rankfilter(src, ranks);

    // might need a buffer for the zstd output
    std::vector<char> dbuff;
    void *dst = src.buffer + src.size;
    size_t size = extrasize;
    // Allocate a temp buffer if there is not sufficient space.
    // Zstd bound is about (size * 1.004 + 64)
    if (size < ZSTD_compressBound(src.size))
    {
        size = ZSTD_compressBound(src.size);
        dbuff.resize(size);
        dst = dbuff.data();
    }

    size_t val =
        ZSTD_compressCCtx(cctx, dst, size, src.buffer, src.size, c_level);
    if (ZSTD_isError(val))
        return nullptr;

    // If we didn't need the buffer, packed data is already in the user buffer
    if (dbuff.empty())
    {
        src.size = val;
        return dst;
    }

    if (val > (src.size + extrasize))
    {  // Doesn't fit in user buffer
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "MRF: ZSTD compression buffer too small");
        return nullptr;  // Error
    }

    memcpy(src.buffer, dbuff.data(), val);
    src.size = val;
    return src.buffer;
}
#endif

//
// The deflate_flags are available in all bands even if the DEFLATE option
// itself is not set.  This allows for PNG features to be controlled, as well
// as any other bands that use zlib by itself
//
MRFRasterBand::MRFRasterBand(MRFDataset *parent_dataset, const ILImage &image,
                             int band, int ov)
    : poMRFDS(parent_dataset),
      dodeflate(GetOptlist().FetchBoolean("DEFLATE", FALSE)),
      // Bring the quality to 0 to 9
      deflate_flags(image.quality / 10),
      dozstd(GetOptlist().FetchBoolean("ZSTD", FALSE)), zstd_level(9), m_l(ov),
      img(image)
{
    nBand = band;
    eDataType = parent_dataset->current.dt;
    nRasterXSize = img.size.x;
    nRasterYSize = img.size.y;
    nBlockXSize = img.pagesize.x;
    nBlockYSize = img.pagesize.y;
    nBlocksPerRow = img.pagecount.x;
    nBlocksPerColumn = img.pagecount.y;
    img.NoDataValue = MRFRasterBand::GetNoDataValue(&img.hasNoData);

    // Pick up the twists, aka GZ, RAWZ headers
    if (GetOptlist().FetchBoolean("GZ", FALSE))
        deflate_flags |= ZFLAG_GZ;
    else if (GetOptlist().FetchBoolean("RAWZ", FALSE))
        deflate_flags |= ZFLAG_RAW;
    // And Pick up the ZLIB strategy, if any
    const char *zstrategy = GetOptlist().FetchNameValueDef("Z_STRATEGY", "");
    int zv = Z_DEFAULT_STRATEGY;
    if (EQUAL(zstrategy, "Z_HUFFMAN_ONLY"))
        zv = Z_HUFFMAN_ONLY;
    else if (EQUAL(zstrategy, "Z_RLE"))
        zv = Z_RLE;
    else if (EQUAL(zstrategy, "Z_FILTERED"))
        zv = Z_FILTERED;
    else if (EQUAL(zstrategy, "Z_FIXED"))
        zv = Z_FIXED;
    deflate_flags |= (zv << 6);
    if (image.quality < 23 && image.quality > 0)
        zstd_level = image.quality;

#if !defined(ZSTD_SUPPORT)
    if (dozstd)
    {  // signal error condition to caller
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "MRF: ZSTD support is not available");
        dozstd = FALSE;
    }
#endif
    // Chose zstd over deflate if both are enabled and available
    if (dozstd && dodeflate)
        dodeflate = FALSE;
}

// Clean up the overviews if they exist
MRFRasterBand::~MRFRasterBand()
{
    while (!overviews.empty())
    {
        delete overviews.back();
        overviews.pop_back();
    }
}

// Look for a string from the dataset options or from the environment
const char *MRFRasterBand::GetOptionValue(const char *opt,
                                          const char *def) const
{
    const char *optValue = poMRFDS->optlist.FetchNameValue(opt);
    if (optValue)
        return optValue;
    return CPLGetConfigOption(opt, def);
}

// Utility function, returns a value from a vector corresponding to the band
// index or the first entry
static double getBandValue(std::vector<double> &v, int idx)
{
    return (static_cast<int>(v.size()) > idx) ? v[idx] : v[0];
}

// Maybe we should check against the type range?
// It is not keeping track of how many values have been set,
// so the application should set none or all the bands
// This call is only valid during Create
CPLErr MRFRasterBand::SetNoDataValue(double val)
{
    if (poMRFDS->bCrystalized)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "MRF: NoData can be set only during file create");
        return CE_Failure;
    }
    if (GInt32(poMRFDS->vNoData.size()) < nBand)
        poMRFDS->vNoData.resize(nBand);
    poMRFDS->vNoData[nBand - 1] = val;
    // We also need to set it for this band
    img.NoDataValue = val;
    img.hasNoData = true;
    return CE_None;
}

double MRFRasterBand::GetNoDataValue(int *pbSuccess)
{
    std::vector<double> &v = poMRFDS->vNoData;
    if (v.empty())
        return GDALPamRasterBand::GetNoDataValue(pbSuccess);
    if (pbSuccess)
        *pbSuccess = TRUE;
    return getBandValue(v, nBand - 1);
}

double MRFRasterBand::GetMinimum(int *pbSuccess)
{
    std::vector<double> &v = poMRFDS->vMin;
    if (v.empty())
        return GDALPamRasterBand::GetMinimum(pbSuccess);
    if (pbSuccess)
        *pbSuccess = TRUE;
    return getBandValue(v, nBand - 1);
}

double MRFRasterBand::GetMaximum(int *pbSuccess)
{
    std::vector<double> &v = poMRFDS->vMax;
    if (v.empty())
        return GDALPamRasterBand::GetMaximum(pbSuccess);
    if (pbSuccess)
        *pbSuccess = TRUE;
    return getBandValue(v, nBand - 1);
}

// Fill with typed ndv, count is always in bytes
template <typename T>
static CPLErr buff_fill(void *b, size_t count, const T ndv)
{
    T *buffer = static_cast<T *>(b);
    count /= sizeof(T);
    while (count--)
        *buffer++ = ndv;
    return CE_None;
}

/**
 *\brief Fills a buffer with no data
 *
 */
CPLErr MRFRasterBand::FillBlock(void *buffer)
{
    int success;
    double ndv = GetNoDataValue(&success);
    if (!success)
        ndv = 0.0;
    size_t bsb = blockSizeBytes();

    // use memset for speed for bytes, or if nodata is zeros
    if (eDataType == GDT_Byte || 0.0L == ndv)
    {
        memset(buffer, int(ndv), bsb);
        return CE_None;
    }

#define bf(T) buff_fill<T>(buffer, bsb, T(ndv));
    switch (eDataType)
    {
        case GDT_UInt16:
            return bf(GUInt16);
        case GDT_Int16:
            return bf(GInt16);
        case GDT_UInt32:
            return bf(GUInt32);
        case GDT_Int32:
            return bf(GInt32);
        case GDT_Float32:
            return bf(float);
        case GDT_Float64:
            return bf(double);
        default:
            break;
    }
#undef bf
    // Should exit before
    return CE_Failure;
}

/*\brief Interleave block fill
 *
 *  Acquire space for all the other bands, fill each one then drop the locks
 *  The current band output goes directly into the buffer
 */

CPLErr MRFRasterBand::FillBlock(int xblk, int yblk, void *buffer)
{
    std::vector<GDALRasterBlock *> blocks;

    for (int i = 0; i < poMRFDS->nBands; i++)
    {
        GDALRasterBand *b = poMRFDS->GetRasterBand(i + 1);
        if (b->GetOverviewCount() && 0 != m_l)
            b = b->GetOverview(m_l - 1);

        // Get the other band blocks, keep them around until later
        if (b == this)
        {
            FillBlock(buffer);
        }
        else
        {
            GDALRasterBlock *poBlock = b->GetLockedBlockRef(xblk, yblk, 1);
            if (poBlock == nullptr)  // Didn't get this block
                break;
            FillBlock(poBlock->GetDataRef());
            blocks.push_back(poBlock);
        }
    }

    // Drop the locks for blocks we acquired
    for (int i = 0; i < int(blocks.size()); i++)
        blocks[i]->DropLock();

    return CE_None;
}

/*\brief Interleave block read
 *
 *  Acquire space for all the other bands, unpack from the dataset buffer, then
 * drop the locks The current band output goes directly into the buffer
 */

CPLErr MRFRasterBand::ReadInterleavedBlock(int xblk, int yblk, void *buffer)
{
    std::vector<GDALRasterBlock *> blocks;

    for (int i = 0; i < poMRFDS->nBands; i++)
    {
        GDALRasterBand *b = poMRFDS->GetRasterBand(i + 1);
        if (b->GetOverviewCount() && 0 != m_l)
            b = b->GetOverview(m_l - 1);

        void *ob = buffer;
        // Get the other band blocks, keep them around until later
        if (b != this)
        {
            GDALRasterBlock *poBlock = b->GetLockedBlockRef(xblk, yblk, 1);
            if (poBlock == nullptr)
                break;
            ob = poBlock->GetDataRef();
            blocks.push_back(poBlock);
        }

        // Just the right mix of templates and macros make deinterleaving tidy
        void *pbuffer = poMRFDS->GetPBuffer();
#define CpySI(T)                                                               \
    cpy_stride_in<T>(ob, reinterpret_cast<T *>(pbuffer) + i,                   \
                     blockSizeBytes() / sizeof(T), img.pagesize.c)

        // Page is already in poMRFDS->pbuffer, not empty
        // There are only four cases, since only the data size matters
        switch (GDALGetDataTypeSize(eDataType) / 8)
        {
            case 1:
                CpySI(GByte);
                break;
            case 2:
                CpySI(GInt16);
                break;
            case 4:
                CpySI(GInt32);
                break;
            case 8:
                CpySI(GIntBig);
                break;
        }
    }

#undef CpySI

    // Drop the locks we acquired
    for (int i = 0; i < int(blocks.size()); i++)
        blocks[i]->DropLock();

    return CE_None;
}

/**
 *\brief Fetch a block from the backing store dataset and keep a copy in the
 *cache
 *
 * @param xblk The X block number, zero based
 * @param yblk The Y block number, zero based
 * @param buffer buffer
 *
 */
CPLErr MRFRasterBand::FetchBlock(int xblk, int yblk, void *buffer)
{
    assert(!poMRFDS->source.empty());
    CPLDebug("MRF_IB", "FetchBlock %d,%d,0,%d, level  %d\n", xblk, yblk, nBand,
             m_l);

    if (poMRFDS->clonedSource)  // This is a clone
        return FetchClonedBlock(xblk, yblk, buffer);

    const GInt32 cstride = img.pagesize.c;  // 1 if band separate
    ILSize req(xblk, yblk, 0, (nBand - 1) / cstride, m_l);
    GUIntBig infooffset = IdxOffset(req, img);

    GDALDataset *poSrcDS = nullptr;
    if (nullptr == (poSrcDS = poMRFDS->GetSrcDS()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Can't open source file %s",
                 poMRFDS->source.c_str());
        return CE_Failure;
    }

    // Scale to base resolution
    double scl = pow(poMRFDS->scale, m_l);
    if (0 == m_l)
        scl = 1;  // To allow for precision issues

    // Prepare parameters for RasterIO, they might be different from a full page
    int vsz = GDALGetDataTypeSize(eDataType) / 8;
    int Xoff = int(xblk * img.pagesize.x * scl + 0.5);
    int Yoff = int(yblk * img.pagesize.y * scl + 0.5);
    int readszx = int(img.pagesize.x * scl + 0.5);
    int readszy = int(img.pagesize.y * scl + 0.5);

    // Compare with the full size and clip to the right and bottom if needed
    int clip = 0;
    if (Xoff + readszx > poMRFDS->full.size.x)
    {
        clip |= 1;
        readszx = poMRFDS->full.size.x - Xoff;
    }
    if (Yoff + readszy > poMRFDS->full.size.y)
    {
        clip |= 1;
        readszy = poMRFDS->full.size.y - Yoff;
    }

    // This is where the whole page fits
    void *ob = buffer;
    if (cstride != 1)
        ob = poMRFDS->GetPBuffer();

    // Fill buffer with NoData if clipping
    if (clip)
        FillBlock(ob);

    // Use the dataset RasterIO to read one or all bands if interleaved
    CPLErr ret = poSrcDS->RasterIO(
        GF_Read, Xoff, Yoff, readszx, readszy, ob, pcount(readszx, int(scl)),
        pcount(readszy, int(scl)), eDataType, cstride,
        (1 == cstride) ? &nBand : nullptr, vsz * cstride,
        vsz * cstride * img.pagesize.x,
        (cstride != 1) ? vsz : vsz * img.pagesize.x * img.pagesize.y, nullptr);

    if (ret != CE_None)
        return ret;

    // Might have the block in the pbuffer, mark it anyhow
    poMRFDS->tile = req;
    buf_mgr filesrc;
    filesrc.buffer = (char *)ob;
    filesrc.size = static_cast<size_t>(img.pageSizeBytes);

    if (poMRFDS->bypass_cache)
    {  // No local caching, just return the data
        if (1 == cstride)
            return CE_None;
        return ReadInterleavedBlock(xblk, yblk, buffer);
    }

    // Test to see if it needs to be written, or just marked as checked
    int success;
    double val = GetNoDataValue(&success);
    if (!success)
        val = 0.0;

    // TODO: test band by band if data is interleaved
    if (isAllVal(eDataType, ob, img.pageSizeBytes, val))
    {
        // Mark it empty and checked, ignore the possible write error
        poMRFDS->WriteTile((void *)1, infooffset, 0);
        if (1 == cstride)
            return CE_None;
        return ReadInterleavedBlock(xblk, yblk, buffer);
    }

    // Write the page in the local cache

    // Have to use a separate buffer for compression output.
    void *outbuff = VSIMalloc(poMRFDS->pbsize);
    if (nullptr == outbuff)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Can't get buffer for writing page");
        // This is not really an error for a cache, the data is fine
        return CE_Failure;
    }

    buf_mgr filedst = {static_cast<char *>(outbuff), poMRFDS->pbsize};
    auto start_time = steady_clock::now();
    Compress(filedst, filesrc);

    // Where the output is, in case we deflate
    void *usebuff = outbuff;
    if (dodeflate)
    {
        usebuff = DeflateBlock(filedst, poMRFDS->pbsize - filedst.size,
                               deflate_flags);
        if (!usebuff)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: Deflate error");
            return CE_Failure;
        }
    }

#if defined(ZSTD_SUPPORT)
    else if (dozstd)
    {
        size_t ranks = 0;  // Assume no need for byte rank sort
        if (img.comp == IL_NONE || img.comp == IL_ZSTD)
            ranks =
                static_cast<size_t>(GDALGetDataTypeSizeBytes(img.dt)) * cstride;
        usebuff = ZstdCompBlock(filedst, poMRFDS->pbsize - filedst.size,
                                zstd_level, poMRFDS->getzsc(), ranks);
        if (!usebuff)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "MRF: ZSTD compression error");
            return CE_Failure;
        }
    }
#endif

    poMRFDS->write_timer +=
        duration_cast<nanoseconds>(steady_clock::now() - start_time);

    // Write and update the tile index
    ret = poMRFDS->WriteTile(usebuff, infooffset, filedst.size);
    CPLFree(outbuff);

    // If we hit an error or if unpaking is not needed
    if (ret != CE_None || cstride == 1)
        return ret;

    // data is already in DS buffer, deinterlace it in pixel blocks
    return ReadInterleavedBlock(xblk, yblk, buffer);
}

/**
 *\brief Fetch for a cloned MRF
 *
 * @param xblk The X block number, zero based
 * @param yblk The Y block number, zero based
 * @param buffer buffer
 *
 */

CPLErr MRFRasterBand::FetchClonedBlock(int xblk, int yblk, void *buffer)
{
    CPLDebug("MRF_IB", "FetchClonedBlock %d,%d,0,%d, level  %d\n", xblk, yblk,
             nBand, m_l);

    // Paranoid check
    assert(poMRFDS->clonedSource);
    MRFDataset *poSrc = static_cast<MRFDataset *>(poMRFDS->GetSrcDS());
    if (nullptr == poSrc)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Can't open source file %s",
                 poMRFDS->source.c_str());
        return CE_Failure;
    }

    if (poMRFDS->bypass_cache || GF_Read == DataMode())
    {
        // Can't store, so just fetch from source, which is an MRF with
        // identical structure
        MRFRasterBand *b =
            static_cast<MRFRasterBand *>(poSrc->GetRasterBand(nBand));
        if (b->GetOverviewCount() && m_l)
            b = static_cast<MRFRasterBand *>(b->GetOverview(m_l - 1));
        if (b == nullptr)
            return CE_Failure;
        return b->IReadBlock(xblk, yblk, buffer);
    }

    ILSize req(xblk, yblk, 0, (nBand - 1) / img.pagesize.c, m_l);
    ILIdx tinfo;

    // Get the cloned source tile info
    // The cloned source index is after the current one
    if (CE_None != poMRFDS->ReadTileIdx(tinfo, req, img, poMRFDS->idxSize))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MRF: Unable to read cloned index entry");
        return CE_Failure;
    }

    GUIntBig infooffset = IdxOffset(req, img);
    CPLErr err;

    // Does the source have this tile?
    if (tinfo.size == 0)
    {  // Nope, mark it empty and return fill
        err = poMRFDS->WriteTile((void *)1, infooffset, 0);
        if (CE_None != err)
            return err;
        return FillBlock(buffer);
    }

    VSILFILE *srcfd = poSrc->DataFP();
    if (nullptr == srcfd)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MRF: Can't open source data file %s",
                 poMRFDS->source.c_str());
        return CE_Failure;
    }

    // Need to read the tile from the source
    if (tinfo.size <= 0 || tinfo.size > INT_MAX)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Invalid tile size " CPL_FRMT_GIB, tinfo.size);
        return CE_Failure;
    }
    char *buf = static_cast<char *>(VSIMalloc(static_cast<size_t>(tinfo.size)));
    if (buf == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate " CPL_FRMT_GIB " bytes", tinfo.size);
        return CE_Failure;
    }

    VSIFSeekL(srcfd, tinfo.offset, SEEK_SET);
    if (tinfo.size !=
        GIntBig(VSIFReadL(buf, 1, static_cast<size_t>(tinfo.size), srcfd)))
    {
        CPLFree(buf);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MRF: Can't read data from source %s",
                 poSrc->current.datfname.c_str());
        return CE_Failure;
    }

    // Write it then reissue the read
    err = poMRFDS->WriteTile(buf, infooffset, tinfo.size);
    CPLFree(buf);
    if (CE_None != err)
        return err;
    // Reissue read, it will work from the cloned data
    return IReadBlock(xblk, yblk, buffer);
}

/**
 *\brief read a block in the provided buffer
 *
 *  For separate band model, the DS buffer is not used, the read is direct in
 *the buffer For pixel interleaved model, the DS buffer holds the temp copy and
 *all the other bands are force read
 *
 */

CPLErr MRFRasterBand::IReadBlock(int xblk, int yblk, void *buffer)
{
    GInt32 cstride = img.pagesize.c;
    ILIdx tinfo;
    ILSize req(xblk, yblk, 0, (nBand - 1) / cstride, m_l);
    CPLDebug("MRF_IB",
             "IReadBlock %d,%d,0,%d, level %d, idxoffset " CPL_FRMT_GIB "\n",
             xblk, yblk, nBand - 1, m_l, IdxOffset(req, img));

    // If this is a caching file and bypass is on, just do the fetch
    if (poMRFDS->bypass_cache && !poMRFDS->source.empty())
        return FetchBlock(xblk, yblk, buffer);

    tinfo.size = 0;  // Just in case it is missing
    if (CE_None != poMRFDS->ReadTileIdx(tinfo, req, img))
    {
        if (!poMRFDS->no_errors)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "MRF: Unable to read index at offset " CPL_FRMT_GIB,
                     IdxOffset(req, img));
            return CE_Failure;
        }
        return FillBlock(buffer);
    }

    if (0 == tinfo.size)
    {  // Could be missing or it could be caching
        // Offset != 0 means no data, Update mode is for local MRFs only
        // if caching index mode is RO don't try to fetch
        // Also, caching MRFs can't be opened in update mode
        if (0 != tinfo.offset || GA_Update == poMRFDS->eAccess ||
            poMRFDS->source.empty() || IdxMode() == GF_Read)
            return FillBlock(buffer);

        // caching MRF, need to fetch a block
        return FetchBlock(xblk, yblk, buffer);
    }

    CPLDebug("MRF_IB", "Tinfo offset " CPL_FRMT_GIB ", size " CPL_FRMT_GIB "\n",
             tinfo.offset, tinfo.size);
    // If we have a tile, read it

    // Should use a permanent buffer, like the pbuffer mechanism
    // Get a large buffer, in case we need to unzip

    // We add a padding of 3 bytes since in LERC1 decompression, we can
    // dereference a unsigned int at the end of the buffer, that can be
    // partially out of the buffer.
    // Can be reproduced with :
    // gdal_translate ../autotest/gcore/data/byte.tif out.mrf -of MRF -co
    // COMPRESS=LERC -co OPTIONS=V1:YES -ot Float32 valgrind gdalinfo -checksum
    // out.mrf Invalid read of size 4 at BitStuffer::read(unsigned char**,
    // std::vector<unsigned int, std::allocator<unsigned int> >&) const
    // (BitStuffer.cpp:153)

    // No stored tile should be larger than twice the raw size.
    if (tinfo.size <= 0 || tinfo.size > poMRFDS->pbsize * 2)
    {
        if (!poMRFDS->no_errors)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Stored tile is too large: " CPL_FRMT_GIB, tinfo.size);
            return CE_Failure;
        }
        return FillBlock(buffer);
    }

    VSILFILE *dfp = DataFP();

    // No data file to read from
    if (dfp == nullptr)
        return CE_Failure;

    void *data = VSIMalloc(static_cast<size_t>(tinfo.size + PADDING_BYTES));
    if (data == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Could not allocate memory for tile size: " CPL_FRMT_GIB,
                 tinfo.size);
        return CE_Failure;
    }

    // This part is not thread safe, but it is what GDAL expects
    VSIFSeekL(dfp, tinfo.offset, SEEK_SET);
    if (1 != VSIFReadL(data, static_cast<size_t>(tinfo.size), 1, dfp))
    {
        CPLFree(data);
        if (poMRFDS->no_errors)
            return FillBlock(buffer);
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to read data page, %d@%x",
                 static_cast<int>(tinfo.size), static_cast<int>(tinfo.offset));
        return CE_Failure;
    }

    /* initialize padding bytes */
    memset(((char *)data) + static_cast<size_t>(tinfo.size), 0, PADDING_BYTES);
    buf_mgr src = {(char *)data, static_cast<size_t>(tinfo.size)};
    buf_mgr dst;

    auto start_time = steady_clock::now();

    // We got the data, do we need to decompress it before decoding?
    if (dodeflate)
    {
        if (img.pageSizeBytes > INT_MAX - 1440)
        {
            CPLFree(data);
            CPLError(CE_Failure, CPLE_AppDefined, "Page size is too big at %d",
                     img.pageSizeBytes);
            return CE_Failure;
        }
        dst.size =
            img.pageSizeBytes +
            1440;  // in case the packed page is a bit larger than the raw one
        dst.buffer = static_cast<char *>(VSIMalloc(dst.size));
        if (nullptr == dst.buffer)
        {
            CPLFree(data);
            CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate %d bytes",
                     static_cast<int>(dst.size));
            return CE_Failure;
        }

        if (ZUnPack(src, dst, deflate_flags))
        {  // Got it unpacked, update the pointers
            CPLFree(data);
            data = dst.buffer;
            tinfo.size = dst.size;
        }
        else
        {  // assume the page was not gzipped, warn only
            CPLFree(dst.buffer);
            if (!poMRFDS->no_errors)
                CPLError(CE_Warning, CPLE_AppDefined, "Can't inflate page!");
        }
    }

#if defined(ZSTD_SUPPORT)
    // undo ZSTD
    else if (dozstd)
    {
        auto ctx = poMRFDS->getzsd();
        if (!ctx)
        {
            CPLFree(data);
            CPLError(CE_Failure, CPLE_AppDefined, "Can't acquire ZSTD context");
            return CE_Failure;
        }
        if (img.pageSizeBytes > INT_MAX - 1440)
        {
            CPLFree(data);
            CPLError(CE_Failure, CPLE_AppDefined, "Page is too large at %d",
                     img.pageSizeBytes);
            return CE_Failure;
        }
        dst.size =
            img.pageSizeBytes +
            1440;  // Allow for a slight increase from previous compressions
        dst.buffer = static_cast<char *>(VSIMalloc(dst.size));
        if (nullptr == dst.buffer)
        {
            CPLFree(data);
            CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate %d bytes",
                     static_cast<int>(dst.size));
            return CE_Failure;
        }

        auto raw_size = ZSTD_decompressDCtx(ctx, dst.buffer, dst.size,
                                            src.buffer, src.size);
        if (ZSTD_isError(raw_size))
        {  // assume page was not packed, warn only
            CPLFree(dst.buffer);
            if (!poMRFDS->no_errors)
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Can't unpack ZSTD page!");
        }
        else
        {
            CPLFree(data);  // The compressed data
            data = dst.buffer;
            tinfo.size = raw_size;
            // Might need to undo the rank sort
            size_t ranks = 0;
            if (img.comp == IL_NONE || img.comp == IL_ZSTD)
                ranks = static_cast<size_t>(GDALGetDataTypeSizeBytes(img.dt)) *
                        img.pagesize.c;
            if (ranks)
            {
                src.buffer = static_cast<char *>(data);
                src.size = static_cast<size_t>(tinfo.size);
                derank(src, ranks);
            }
        }
    }
#endif

    src.buffer = static_cast<char *>(data);
    src.size = static_cast<size_t>(tinfo.size);

    // After unpacking, the size has to be pageSizeBytes
    // If pages are interleaved, use the dataset page buffer instead
    dst.buffer = reinterpret_cast<char *>(
        (1 == cstride) ? buffer : poMRFDS->GetPBuffer());
    dst.size = img.pageSizeBytes;

    if (poMRFDS->no_errors)
        CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLErr ret = Decompress(dst, src);

    poMRFDS->read_timer +=
        duration_cast<nanoseconds>(steady_clock::now() - start_time);

    dst.size =
        img.pageSizeBytes;  // In case the decompress failed, force it back

    // Swap whatever we decompressed if we need to
    if (is_Endianess_Dependent(img.dt, img.comp) && (img.nbo != NET_ORDER))
        swab_buff(dst, img);

    CPLFree(data);
    if (poMRFDS->no_errors)
    {
        CPLPopErrorHandler();
        if (ret != CE_None)  // Set each page buffer to the correct no data
                             // value, then proceed
            return (1 == cstride) ? FillBlock(buffer)
                                  : FillBlock(xblk, yblk, buffer);
    }

    // If pages are separate or we had errors, we're done
    if (1 == cstride || CE_None != ret)
        return ret;

    // De-interleave page from dataset buffer and return
    return ReadInterleavedBlock(xblk, yblk, buffer);
}

/**
 *\brief Write a block from the provided buffer
 *
 * Same trick as read, use a temporary tile buffer for pixel interleave
 * For band separate, use a
 * Write the block once it has all the bands, report
 * if a new block is started before the old one was completed
 *
 */

CPLErr MRFRasterBand::IWriteBlock(int xblk, int yblk, void *buffer)
{
    GInt32 cstride = img.pagesize.c;
    ILSize req(xblk, yblk, 0, (nBand - 1) / cstride, m_l);
    GUIntBig infooffset = IdxOffset(req, img);

    CPLDebug("MRF_IB", "IWriteBlock %d,%d,0,%d, level %d, stride %d\n", xblk,
             yblk, nBand, m_l, cstride);

    // Finish the Create call
    if (!poMRFDS->bCrystalized && !poMRFDS->Crystalize())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error creating files");
        return CE_Failure;
    }

    if (1 == cstride)
    {  // Separate bands, we can write it as is
        // Empty page skip
        int success;
        double val = GetNoDataValue(&success);
        if (!success)
            val = 0.0;
        if (isAllVal(eDataType, buffer, img.pageSizeBytes, val))
            return poMRFDS->WriteTile(nullptr, infooffset, 0);

        // Use the pbuffer to hold the compressed page before writing it
        poMRFDS->tile = ILSize();  // Mark it corrupt

        buf_mgr src;
        src.buffer = (char *)buffer;
        src.size = static_cast<size_t>(img.pageSizeBytes);
        buf_mgr dst = {(char *)poMRFDS->GetPBuffer(),
                       poMRFDS->GetPBufferSize()};

        // Swab the source before encoding if we need to
        if (is_Endianess_Dependent(img.dt, img.comp) && (img.nbo != NET_ORDER))
            swab_buff(src, img);

        auto start_time = steady_clock::now();

        // Compress functions need to return the compressed size in
        // the bytes in buffer field
        Compress(dst, src);
        void *usebuff = dst.buffer;
        if (dodeflate)
        {
            usebuff =
                DeflateBlock(dst, poMRFDS->pbsize - dst.size, deflate_flags);
            if (!usebuff)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "MRF: Deflate error");
                return CE_Failure;
            }
        }

#if defined(ZSTD_SUPPORT)
        else if (dozstd)
        {
            size_t ranks = 0;  // Assume no need for byte rank sort
            if (img.comp == IL_NONE || img.comp == IL_ZSTD)
                ranks = static_cast<size_t>(GDALGetDataTypeSizeBytes(img.dt));
            usebuff = ZstdCompBlock(dst, poMRFDS->pbsize - dst.size, zstd_level,
                                    poMRFDS->getzsc(), ranks);
            if (!usebuff)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "MRF: Zstd Compression error");
                return CE_Failure;
            }
        }
#endif
        poMRFDS->write_timer +=
            duration_cast<nanoseconds>(steady_clock::now() - start_time);
        return poMRFDS->WriteTile(usebuff, infooffset, dst.size);
    }

    // Multiple bands per page, use a temporary to assemble the page
    // Temporary is large because we use it to hold both the uncompressed and
    // the compressed
    poMRFDS->tile = req;
    poMRFDS->bdirty = 0;

    // Keep track of what bands are empty
    GUIntBig empties = 0;

    void *tbuffer = VSIMalloc(img.pageSizeBytes + poMRFDS->pbsize);

    if (!tbuffer)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MRF: Can't allocate write buffer");
        return CE_Failure;
    }

    // Get the other bands from the block cache
    for (int iBand = 0; iBand < poMRFDS->nBands; iBand++)
    {
        char *pabyThisImage = nullptr;
        GDALRasterBlock *poBlock = nullptr;

        if (iBand == nBand - 1)
        {
            pabyThisImage = reinterpret_cast<char *>(buffer);
            poMRFDS->bdirty |= bandbit();
        }
        else
        {
            GDALRasterBand *band = poMRFDS->GetRasterBand(iBand + 1);
            // Pick the right overview
            if (m_l)
                band = band->GetOverview(m_l - 1);
            poBlock = (reinterpret_cast<MRFRasterBand *>(band))
                          ->TryGetLockedBlockRef(xblk, yblk);
            if (nullptr == poBlock)
                continue;
            // This is where the image data is for this band

            pabyThisImage = reinterpret_cast<char *>(poBlock->GetDataRef());
            poMRFDS->bdirty |= bandbit(iBand);
        }

        // Keep track of empty bands, but encode them anyhow, in case some are
        // not empty
        int success;
        double val = GetNoDataValue(&success);
        if (!success)
            val = 0.0;
        if (isAllVal(eDataType, pabyThisImage, blockSizeBytes(), val))
            empties |= bandbit(iBand);

            // Copy the data into the dataset buffer here
            // Just the right mix of templates and macros make this real tidy
#define CpySO(T)                                                               \
    cpy_stride_out<T>((reinterpret_cast<T *>(tbuffer)) + iBand, pabyThisImage, \
                      blockSizeBytes() / sizeof(T), cstride)

        // Build the page in tbuffer
        switch (GDALGetDataTypeSize(eDataType) / 8)
        {
            case 1:
                CpySO(GByte);
                break;
            case 2:
                CpySO(GInt16);
                break;
            case 4:
                CpySO(GInt32);
                break;
            case 8:
                CpySO(GIntBig);
                break;
            default:
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "MRF: Write datatype of %d bytes "
                         "not implemented",
                         GDALGetDataTypeSize(eDataType) / 8);
                if (poBlock != nullptr)
                {
                    poBlock->MarkClean();
                    poBlock->DropLock();
                }
                CPLFree(tbuffer);
                return CE_Failure;
            }
        }

        if (poBlock != nullptr)
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    // Should keep track of the individual band buffers and only mix them if
    // this is not an empty page ( move the Copy with Stride Out from above
    // below this test This way works fine, but it does work extra for empty
    // pages

    if (GIntBig(empties) == AllBandMask())
    {
        CPLFree(tbuffer);
        return poMRFDS->WriteTile(nullptr, infooffset, 0);
    }

    if (poMRFDS->bdirty != AllBandMask())
        CPLError(CE_Warning, CPLE_AppDefined,
                 "MRF: IWrite, band dirty mask is " CPL_FRMT_GIB
                 " instead of " CPL_FRMT_GIB,
                 poMRFDS->bdirty, AllBandMask());

    buf_mgr src;
    src.buffer = (char *)tbuffer;
    src.size = static_cast<size_t>(img.pageSizeBytes);

    // Use the space after pagesizebytes for compressed output, it is of pbsize
    char *outbuff = (char *)tbuffer + img.pageSizeBytes;

    buf_mgr dst = {outbuff, poMRFDS->pbsize};
    CPLErr ret;

    auto start_time = steady_clock::now();

    ret = Compress(dst, src);
    if (ret != CE_None)
    {
        // Compress failed, write it as an empty tile
        CPLFree(tbuffer);
        poMRFDS->WriteTile(nullptr, infooffset, 0);
        return CE_None;  // Should report the error, but it triggers partial
                         // band attempts
    }

    // Where the output is, in case we deflate
    void *usebuff = outbuff;
    if (dodeflate)
    {
        // Move the packed part at the start of tbuffer, to make more space
        // available
        memcpy(tbuffer, outbuff, dst.size);
        dst.buffer = static_cast<char *>(tbuffer);
        usebuff = DeflateBlock(dst,
                               static_cast<size_t>(img.pageSizeBytes) +
                                   poMRFDS->pbsize - dst.size,
                               deflate_flags);
        if (!usebuff)
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: Deflate error");
    }

#if defined(ZSTD_SUPPORT)
    else if (dozstd)
    {
        memcpy(tbuffer, outbuff, dst.size);
        dst.buffer = static_cast<char *>(tbuffer);
        size_t ranks = 0;  // Assume no need for byte rank sort
        if (img.comp == IL_NONE || img.comp == IL_ZSTD)
            ranks =
                static_cast<size_t>(GDALGetDataTypeSizeBytes(img.dt)) * cstride;
        usebuff = ZstdCompBlock(dst,
                                static_cast<size_t>(img.pageSizeBytes) +
                                    poMRFDS->pbsize - dst.size,
                                zstd_level, poMRFDS->getzsc(), ranks);
        if (!usebuff)
            CPLError(CE_Failure, CPLE_AppDefined,
                     "MRF: ZStd compression error");
    }
#endif

    poMRFDS->write_timer +=
        duration_cast<nanoseconds>(steady_clock::now() - start_time);

    if (!usebuff)
    {  // Error was signaled
        CPLFree(tbuffer);
        poMRFDS->WriteTile(nullptr, infooffset, 0);
        poMRFDS->bdirty = 0;
        return CE_Failure;
    }

    ret = poMRFDS->WriteTile(usebuff, infooffset, dst.size);
    CPLFree(tbuffer);

    poMRFDS->bdirty = 0;
    return ret;
}

//
// Tests if a given block exists without reading it
// returns false only when it is definitely not existing
//
bool MRFRasterBand::TestBlock(int xblk, int yblk)
{
    // When bypassing the cache, assume all blocks are valid
    if (poMRFDS->bypass_cache && !poMRFDS->source.empty())
        return true;

    // Blocks outside of image have no data by default
    if (xblk < 0 || yblk < 0 || xblk >= img.pagecount.x ||
        yblk >= img.pagecount.y)
        return false;

    ILIdx tinfo;
    GInt32 cstride = img.pagesize.c;
    ILSize req(xblk, yblk, 0, (nBand - 1) / cstride, m_l);

    if (CE_None != poMRFDS->ReadTileIdx(tinfo, req, img))
        // Got an error reading the tile index
        return !poMRFDS->no_errors;

    // Got an index, if the size is readable, the block does exist
    if (0 < tinfo.size && tinfo.size < poMRFDS->pbsize * 2)
        return true;

    // We are caching, but the tile has not been checked, so it could exist
    return (!poMRFDS->source.empty() && 0 == tinfo.offset);
}

int MRFRasterBand::GetOverviewCount()
{
    // First try internal overviews
    int nInternalOverviewCount = static_cast<int>(overviews.size());
    if (nInternalOverviewCount > 0)
        return nInternalOverviewCount;
    return GDALPamRasterBand::GetOverviewCount();
}

GDALRasterBand *MRFRasterBand::GetOverview(int n)
{
    // First try internal overviews
    if (n >= 0 && n < (int)overviews.size())
        return overviews[n];
    return GDALPamRasterBand::GetOverview(n);
}

NAMESPACE_MRF_END
