/*
* Copyright (c) 2002-2012, California Institute of Technology.
* All rights reserved.  Based on Government Sponsored Research under contracts NAS7-1407 and/or NAS7-03001.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
*   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
*   2. Redistributions in binary form must reproduce the above copyright notice, 
*      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
*   3. Neither the name of the California Institute of Technology (Caltech), its operating division the Jet Propulsion Laboratory (JPL), 
*      the National Aeronautics and Space Administration (NASA), nor the names of its contributors may be used to 
*      endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
* IN NO EVENT SHALL THE CALIFORNIA INSTITUTE OF TECHNOLOGY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Copyright 2014-2015 Esri
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
* $Id$
*
* Project:  Meta Raster File Format Driver Implementation, RasterBand
* Purpose:  Implementation of Pile of Tile Format
*
* Author:   Lucian Plesea, Lucian.Plesea@jpl.nasa.gov, lplesea@esri.com
*
******************************************************************************
*
*
* 
*
* 
****************************************************************************/

#include "marfa.h"
#include <gdal_priv.h>
#include <ogr_srs_api.h>
#include <ogr_spatialref.h>

#include <vector>
#include <assert.h>
#include "../zlib/zlib.h"

using std::vector;
using std::string;

NAMESPACE_MRF_START

// packs a block of a given type, with a stride
// Count is the number of items that need to be copied
// These are separate to allow for optimization

template <typename T> void cpy_stride_in(void *dst, 
	const void *src, int c, int stride)
{
    T *s=(T *)src;
    T *d=(T *)dst;

    while (c--) {
	*d++=*s;
	s+=stride;
    }
}

template <typename T> void cpy_stride_out(void *dst, 
	const void *src, int c, int stride)
{
    T *s=(T *)src;
    T *d=(T *)dst;

    while (c--) {
	*d=*s++;
	d+=stride;
    }
}

// Does every value in the buffer have the same value, using strict comparison
template<typename T> inline int isAllVal(const T *b, size_t bytecount, double ndv)

{
    T val = (T)(ndv);
    size_t count = bytecount / sizeof(T);
    while (count--) {
        if (*(b++) != val) {
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
#define TEST_T(GType, type)\
    case GType: \
	isempty = isAllVal((type *)b, bytecount, ndv);\
	break

    switch (gt) {
	TEST_T(GDT_Byte, GByte);
	TEST_T(GDT_UInt16, GUInt16);
	TEST_T(GDT_Int16, GInt16);
	TEST_T(GDT_UInt32, GUInt32);
	TEST_T(GDT_Int32, GInt32);
	TEST_T(GDT_Float32, float);
	TEST_T(GDT_Float64, double);
	default : break;
    }
#undef TEST_T

    return isempty;
}

// Swap bytes in place, unconditional
static void swab_buff(buf_mgr &src, const ILImage &img)
{
    size_t i;
    switch (GDALGetDataTypeSize(img.dt)) {
    case 16: {
	short int *b = (short int*)src.buffer;
	for (i = src.size / 2; i; b++, i--)
	    *b = swab16(*b);
	break;
    }
    case 32: {
	int *b = (int*)src.buffer;
	for (i = src.size / 4; i; b++, i--)
	    *b = swab32(*b);
	break;
    }
    case 64: {
	long long *b = (long long*)src.buffer;
	for (i = src.size / 8; i; b++, i--)
	    *b = swab64(*b);
	break;
    }
    }
}

/**
*\brief Deflates a buffer, extrasize is the available size in the buffer past the input
*  If the output fits past the data, it uses that area
* otherwise it uses a temporary buffer and copies the data over the input on return, returning a pointer to it
*/
static void *DeflateBlock(buf_mgr &src, size_t extrasize, int flags) {
    // The one we might need to allocate
    void *dbuff = NULL;
    buf_mgr dst;
    // The one we could use, after the packed data
    dst.buffer = src.buffer + src.size;
    dst.size = extrasize;

    // Allocate a temp buffer if there is not sufficient space,
    // We need to have a bit more than half the buffer available
    if (extrasize < (src.size + 64)) {
	dst.size = src.size + 64;

	dbuff = VSIMalloc(dst.size);
	dst.buffer = (char *)dbuff;
	if (!dst.buffer)
	    return NULL;
    }

    if (!ZPack(src, dst, flags)) {
	CPLFree(dbuff); // Safe to call with NULL
	return NULL;
    }

    // source size is used to hold the output size
    src.size = dst.size;
    // If we didn't allocate a buffer, the receiver can use it already
    if (!dbuff) 
	return dst.buffer;

    // If we allocated a buffer, we need to copy the data to the input buffer 
    memcpy(src.buffer, dbuff, src.size);
    CPLFree(dbuff);
    return src.buffer;
}

//
// The deflate_flags are available in all bands even if the DEFLATE option 
// itself is not set.  This allows for PNG features to be controlled, as well
// as any other bands that use zlib by itself
//
GDALMRFRasterBand::GDALMRFRasterBand(GDALMRFDataset *parent_dataset,
					const ILImage &image, int band, int ov)
{
    poDS=parent_dataset;
    nBand=band;
    m_band=band-1;
    m_l=ov;
    img=image;
    eDataType=parent_dataset->current.dt;
    nRasterXSize = img.size.x;
    nRasterYSize = img.size.y;
    nBlockXSize = img.pagesize.x;
    nBlockYSize = img.pagesize.y;
    nBlocksPerRow = img.pagecount.x;
    nBlocksPerColumn = img.pagecount.y;
    img.NoDataValue = GetNoDataValue(&img.hasNoData);

    deflatep = GetOptlist().FetchBoolean("DEFLATE", FALSE);
    // Bring the quality to 0 to 9
    deflate_flags = img.quality / 10;
    // Pick up the twists, aka GZ, RAWZ headers
    if (GetOptlist().FetchBoolean("GZ", FALSE))
	deflate_flags |= ZFLAG_GZ;
    else if (GetOptlist().FetchBoolean("RAWZ", FALSE))
	deflate_flags |= ZFLAG_RAW;
    // And Pick up the ZLIB strategy, if any
    const char *zstrategy = GetOptlist().FetchNameValueDef("Z_STRATEGY", NULL);
    if (zstrategy) {
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
    }
    overview = 0;
}

// Clean up the overviews if they exist
GDALMRFRasterBand::~GDALMRFRasterBand()
{
    while (0!=overviews.size()) {
	delete overviews[overviews.size()-1];
	overviews.pop_back();
    };
}

// Look for a string from the dataset options or from the environment
const char * GDALMRFRasterBand::GetOptionValue(const char *opt, const char *def) const
{
    const char *optValue = poDS->optlist.FetchNameValue(opt);
    if (optValue) return optValue;
    return CPLGetConfigOption(opt, def);
}

// Utility function, returns a value from a vector corresponding to the band index
// or the first entry
static double getBandValue(std::vector<double> &v,int idx)
{
    if (static_cast<int>(v.size()) > idx)
	return v[idx];
    return v[0];
}

// Maybe we should check against the type range?
// It is not keeping track of how many values have been set,
// so the application should set none or all the bands
// This call is only valid during Create
CPLErr  GDALMRFRasterBand::SetNoDataValue(double val)
{
    if (poDS->bCrystalized) {
	CPLError(CE_Failure, CPLE_AssertionFailed, "MRF: NoData can be set only during file create");
	return CE_Failure;
    }
    if (GInt32(poDS->vNoData.size()) < nBand)
	poDS->vNoData.resize(nBand);
    poDS->vNoData[m_band] = val;
    // We also need to set it for this band
    img.NoDataValue = val;
    img.hasNoData = true;
    return CE_None;
}

double GDALMRFRasterBand::GetNoDataValue(int *pbSuccess)
{
    std::vector<double> &v=poDS->vNoData;
    if (v.size() == 0)
	return GDALPamRasterBand::GetNoDataValue(pbSuccess);
    if (pbSuccess) *pbSuccess=TRUE;
    return getBandValue(v, m_band);
}

double GDALMRFRasterBand::GetMinimum(int *pbSuccess)
{
    std::vector<double> &v=poDS->vMin;
    if (v.size() == 0)
	return GDALPamRasterBand::GetMinimum(pbSuccess);
    if (pbSuccess) *pbSuccess=TRUE;
    return getBandValue(v, m_band);
}

double GDALMRFRasterBand::GetMaximum(int *pbSuccess)
{
    std::vector<double> &v=poDS->vMax;
    if (v.size() == 0)
	return GDALPamRasterBand::GetMaximum(pbSuccess);
    if (pbSuccess) *pbSuccess=TRUE;
    return getBandValue(v, m_band);
}

// Fill, with ndv
template<typename T> CPLErr buff_fill(void *b, size_t count, const T ndv)
{
    T *buffer = static_cast<T*>(b);
    count /= sizeof(T);
    while (count--)
	*buffer++ = ndv;
    return CE_None;
}

/**
*\brief Fills a buffer with no data
*
*/
CPLErr GDALMRFRasterBand::FillBlock(void *buffer)
{
    int success;
    double ndv = GetNoDataValue(&success);
    if (!success) ndv = 0.0;

    size_t bsb = blockSizeBytes();

    // use memset for speed for bytes, or if nodata is zeros
    if (eDataType == GDT_Byte || 0.0L == ndv ) {
	memset(buffer, int(ndv), bsb);
	return CE_None;
    }

#define bf(T) buff_fill<T>(buffer, bsb, T(ndv));
    switch(eDataType) {
	case GDT_UInt16:    return bf(GUInt16);
	case GDT_Int16:     return bf(GInt16);
	case GDT_UInt32:    return bf(GUInt32);
	case GDT_Int32:     return bf(GInt32);
	case GDT_Float32:   return bf(float);
	case GDT_Float64:   return bf(double);
	default: break;
    }
#undef bf
    // Should exit before
    return CE_Failure;
}

/*\brief Interleave block read
 *
 *  Acquire space for all the other bands, unpack there, then drop the locks
 *  The current band output goes directly into the buffer
 */

CPLErr GDALMRFRasterBand::RB(int xblk, int yblk, buf_mgr /*src*/, void *buffer) {
    vector<GDALRasterBlock *> blocks;

    for (int i = 0; i < poDS->nBands; i++) {
	GDALRasterBand *b = poDS->GetRasterBand(i+1);
	if (b->GetOverviewCount() && m_l)
	    b = b->GetOverview(m_l-1);

	void *ob = buffer;
	// Get the other band blocks, keep them around until later
	if (b != this)
	{
	    GDALRasterBlock *poBlock = b->GetLockedBlockRef(xblk, yblk, 1);
            if( poBlock == NULL )
                break;
	    ob = poBlock->GetDataRef();
	    blocks.push_back(poBlock);
	} 

// Just the right mix of templates and macros make deinterleaving tidy
#define CpySI(T) cpy_stride_in<T> (ob, (T *)poDS->GetPBuffer() + i,\
    blockSizeBytes()/sizeof(T), img.pagesize.c)

	// Page is already in poDS->pbuffer, not empty
	// There are only four cases, since only the real data type matters
	switch (GDALGetDataTypeSize(eDataType)/8)
	{
	case 1: CpySI(GByte); break;
	case 2: CpySI(GInt16); break;
	case 4: CpySI(GInt32); break;
	case 8: CpySI(GIntBig); break;
	}
    }

#undef CpySI

    // Drop the locks we acquired
    for (int i=0; i < int(blocks.size()); i++)
	blocks[i]->DropLock();

    return CE_None;
}


/**
*\brief Fetch a block from the backing store dataset and keep a copy in the cache
*
* @xblk The X block number, zero based
* @yblk The Y block number, zero based
* @param tinfo The return, updated tinfo for this specific tile
*
*/
CPLErr GDALMRFRasterBand::FetchBlock(int xblk, int yblk, void *buffer)

{
    assert(!poDS->source.empty());
    CPLDebug("MRF_IB", "FetchBlock %d,%d,0,%d, level  %d\n", xblk, yblk, m_band, m_l);

    if (poDS->clonedSource)  // This is a clone
	return FetchClonedBlock(xblk, yblk, buffer);

    GDALDataset *poSrcDS;
    const GInt32 cstride = img.pagesize.c; // 1 if band separate
    ILSize req(xblk, yblk, 0, m_band / cstride, m_l);
    GUIntBig infooffset = IdxOffset(req, img);

    if ( NULL == (poSrcDS = poDS->GetSrcDS())) {
	CPLError( CE_Failure, CPLE_AppDefined, "MRF: Can't open source file %s", poDS->source.c_str());
	return CE_Failure;
    }

    // Scale to base resolution
    double scl = pow(poDS->scale, m_l);
    if ( 0 == m_l )
	scl = 1; // To allow for precision issues

    // Prepare parameters for RasterIO, they might be different from a full page
    int vsz = GDALGetDataTypeSize(eDataType)/8;
    int Xoff = int(xblk * img.pagesize.x * scl + 0.5);
    int Yoff = int(yblk * img.pagesize.y * scl + 0.5);
    int readszx = int(img.pagesize.x * scl + 0.5);
    int readszy = int(img.pagesize.y * scl + 0.5);

    // Compare with the full size and clip to the right and bottom if needed
    int clip=0;
    if (Xoff + readszx > poDS->full.size.x) {
	clip |= 1;
	readszx = poDS->full.size.x - Xoff;
    }
    if (Yoff + readszy > poDS->full.size.y) {
	clip |= 1;
	readszy = poDS->full.size.y - Yoff;
    }

    // This is where the whole page fits
    void *ob = buffer;
    if (cstride != 1) 
	ob = poDS->GetPBuffer();

    // Fill buffer with NoData if clipping
    if (clip)
	FillBlock(ob);

    // Use the dataset RasterIO to read one or all bands if interleaved
    CPLErr ret = poSrcDS->RasterIO(GF_Read, Xoff, Yoff, readszx, readszy,
	ob, pcount(readszx, int(scl)), pcount(readszy, int(scl)),
	eDataType, cstride, (1 == cstride)? &nBand: NULL,
	vsz * cstride, 	// pixel, line, band stride
	vsz * cstride * img.pagesize.x,
	(cstride != 1) ? vsz : vsz * img.pagesize.x * img.pagesize.y
#if GDAL_VERSION_MAJOR >= 2
	,NULL
#endif
	);

    if (ret != CE_None) return ret;

    // Might have the block in the pbuffer, mark it anyhow
    poDS->tile = req;
    buf_mgr filesrc;
    filesrc.buffer = (char *)ob;
    filesrc.size = static_cast<size_t>(img.pageSizeBytes);

    if (poDS->bypass_cache) { // No local caching, just return the data
	if (1 == cstride)
	    return CE_None;
	return RB(xblk, yblk, filesrc, buffer);
    }

    // Test to see if it needs to be written, or just marked as checked
    int success;
    double val = GetNoDataValue(&success);
    if (!success) val = 0.0;

    // TODO: test band by band if data is interleaved
    if (isAllVal(eDataType, ob, img.pageSizeBytes, val)) {
	// Mark it empty and checked, ignore the possible write error
	poDS->WriteTile((void *)1, infooffset, 0);
	return CE_None;
    }

    // Write the page in the local cache

    // Have to use a separate buffer for compression output.
    void *outbuff = VSIMalloc(poDS->pbsize);

    if (!outbuff) {
	CPLError(CE_Failure, CPLE_AppDefined, 
	    "Can't get buffer for writing page");
	// This is not really an error for a cache, the data is fine
	return CE_Failure;
    }

    buf_mgr filedst={(char *)outbuff, poDS->pbsize};
    Compress(filedst, filesrc);

    // Where the output is, in case we deflate
    void *usebuff = outbuff;
    if (deflatep) {
	usebuff = DeflateBlock( filedst, poDS->pbsize - filedst.size, deflate_flags);
	if (!usebuff) {
	    CPLError(CE_Failure,CPLE_AppDefined, "MRF: Deflate error");
	    return CE_Failure;
	}
    }

    // Write and update the tile index
    ret = poDS->WriteTile(usebuff, infooffset, filedst.size);
    CPLFree(outbuff);

    // If we hit an error or if unpaking is not needed
    if (ret != CE_None || cstride == 1)
	return ret;

    // data is already in filesrc buffer, deinterlace it in pixel blocks
    return RB(xblk, yblk, filesrc, buffer);
}


/**
*\brief Fetch for a cloned MRF
*
* @xblk The X block number, zero based
* @yblk The Y block number, zero based
* @param tinfo The return, updated tinfo for this specific tile
*
*/

CPLErr GDALMRFRasterBand::FetchClonedBlock(int xblk, int yblk, void *buffer)
{
    CPLDebug("MRF_IB","FetchClonedBlock %d,%d,0,%d, level  %d\n", xblk, yblk, m_band, m_l);

    VSILFILE *srcfd;
    // Paranoid check
    assert(poDS->clonedSource);

    GDALMRFDataset *poSrc;
    if ( NULL == (poSrc = static_cast<GDALMRFDataset *>(poDS->GetSrcDS()))) {
	CPLError( CE_Failure, CPLE_AppDefined, "MRF: Can't open source file %s", poDS->source.c_str());
	return CE_Failure;
    }

    if (poDS->bypass_cache || GF_Read == DataMode()) {
	// Can't store, so just fetch from source, which is an MRF with identical structure
	GDALMRFRasterBand *b = static_cast<GDALMRFRasterBand *>(poSrc->GetRasterBand(nBand));
	if (b->GetOverviewCount() && m_l)
	    b = static_cast<GDALMRFRasterBand *>(b->GetOverview(m_l-1));
        if( b == NULL )
            return CE_Failure;
	return b->IReadBlock(xblk,yblk,buffer);
    }

    ILSize req(xblk, yblk, 0, m_band/img.pagesize.c , m_l);
    ILIdx tinfo;

    // Get the cloned source tile info
    // The cloned source index is after the current one
    if (CE_None != poDS->ReadTileIdx(tinfo, req, img, poDS->idxSize)) {
	CPLError(CE_Failure, CPLE_AppDefined, "MRF: Unable to read cloned index entry");
	return CE_Failure;
    }

    GUIntBig infooffset = IdxOffset(req, img);
    CPLErr err;

    // Does the source have this tile?
    if (tinfo.size == 0) { // Nope, mark it empty and return fill
	err = poDS->WriteTile((void *)1, infooffset, 0);
	if (CE_None != err)
	    return err;
	return FillBlock(buffer);
    }

    srcfd = poSrc->DataFP();
    if (NULL == srcfd) {
	CPLError( CE_Failure, CPLE_AppDefined, "MRF: Can't open source data file %s", 
	    poDS->source.c_str());
	return CE_Failure;
    }

    // Need to read the tile from the source
    if( tinfo.size <= 0 || tinfo.size > INT_MAX )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Invalid tile size " CPL_FRMT_GIB,
                 tinfo.size);
        return CE_Failure;
    }
    char *buf = static_cast<char *>(VSIMalloc(static_cast<size_t>(tinfo.size)));
    if( buf == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate " CPL_FRMT_GIB " bytes",
                 tinfo.size);
        return CE_Failure;
    }

    VSIFSeekL(srcfd, tinfo.offset, SEEK_SET);
    if (tinfo.size != GIntBig(VSIFReadL( buf, 1, static_cast<size_t>(tinfo.size), srcfd))) {
	CPLFree(buf);
	CPLError( CE_Failure, CPLE_AppDefined, "MRF: Can't read data from source %s",
	    poSrc->current.datfname.c_str() );
	return CE_Failure;
    }

    // Write it then reissue the read
    err = poDS->WriteTile(buf, infooffset, tinfo.size);
    CPLFree(buf);
    if ( CE_None != err )
	return err;
    // Reissue read, it will work from the cloned data
    return IReadBlock(xblk, yblk, buffer);
}


/**
*\brief read a block in the provided buffer
* 
*  For separate band model, the DS buffer is not used, the read is direct in the buffer
*  For pixel interleaved model, the DS buffer holds the temp copy
*  and all the other bands are force read
*
*/

CPLErr GDALMRFRasterBand::IReadBlock(int xblk, int yblk, void *buffer)
{
    ILIdx tinfo;
    GInt32 cstride = img.pagesize.c;
    ILSize req(xblk, yblk, 0, m_band / cstride, m_l);
    CPLDebug("MRF_IB", "IReadBlock %d,%d,0,%d, level %d, idxoffset " CPL_FRMT_GIB "\n", 
	xblk, yblk, m_band, m_l, IdxOffset(req,img));

    // If this is a caching file and bypass is on, just do the fetch
    if (poDS->bypass_cache && !poDS->source.empty())
	return FetchBlock(xblk, yblk, buffer);

    if (CE_None != poDS->ReadTileIdx(tinfo, req, img)) {
	CPLError( CE_Failure, CPLE_AppDefined,
	    "MRF: Unable to read index at offset " CPL_FRMT_GIB, IdxOffset(req, img));
	return CE_Failure;
    }

    if (0 == tinfo.size) { // Could be missing or it could be caching
	// Offset != 0 means no data, Update mode is for local MRFs only
	// if caching index mode is RO don't try to fetch
	// Also, caching MRFs can't be opened in update mode
	if ( 0 != tinfo.offset || GA_Update == poDS->eAccess 
	    || poDS->source.empty() || IdxMode() == GF_Read )
	    return FillBlock(buffer);

	// caching MRF, need to fetch a block
	return FetchBlock(xblk, yblk, buffer);
    }

    CPLDebug("MRF_IB","Tinfo offset " CPL_FRMT_GIB ", size  " CPL_FRMT_GIB "\n", tinfo.offset, tinfo.size);
    // If we have a tile, read it

    // Should use a permanent buffer, like the pbuffer mechanism
    // Get a large buffer, in case we need to unzip

    // We add a padding of 3 bytes since in LERC1 decompression, we can
    // dereference a unsigned int at the end of the buffer, that can be
    // partially out of the buffer.
    // Can be reproduced with :
    // gdal_translate ../autotest/gcore/data/byte.tif out.mrf -of MRF -co COMPRESS=LERC -co OPTIONS=V1:YES -ot Float32
    // valgrind gdalinfo -checksum out.mrf
    // Invalid read of size 4
    // at BitStuffer::read(unsigned char**, std::vector<unsigned int, std::allocator<unsigned int> >&) const (BitStuffer.cpp:153)
    if( tinfo.size <= 0 || tinfo.size > INT_MAX - 3 )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Too big tile size: " CPL_FRMT_GIB, tinfo.size);
        return CE_Failure;
    }
    void *data = VSIMalloc(static_cast<size_t>(tinfo.size + 3));
    if( data == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Could not allocate memory for tile size: " CPL_FRMT_GIB, tinfo.size);
        return CE_Failure;
    }

    VSILFILE *dfp = DataFP();

    // No data file to read from
    if (dfp == NULL)
    {
        CPLFree(data);
	return CE_Failure;
    }

    // This part is not thread safe, but it is what GDAL expects
    VSIFSeekL(dfp, tinfo.offset, SEEK_SET);
    if (1 != VSIFReadL(data, static_cast<size_t>(tinfo.size), 1, dfp)) {
	CPLFree(data);
	CPLError(CE_Failure, CPLE_AppDefined, "Unable to read data page, %d@%x",
	    int(tinfo.size), int(tinfo.offset));
	return CE_Failure;
    }

    /* initialize padding bytes */
    memset(((char*)data) + static_cast<size_t>(tinfo.size), 0, 3);

    buf_mgr src = {(char *)data, static_cast<size_t>(tinfo.size)};
    buf_mgr dst;

    // We got the data, do we need to decompress it before decoding?
    if (deflatep) {
        if( img.pageSizeBytes > INT_MAX - 1440 )
        {
            CPLFree(data);
            CPLError(CE_Failure, CPLE_AppDefined, "Too big page size %d",
                     img.pageSizeBytes);
            return CE_Failure;
        }
	dst.size = img.pageSizeBytes + 1440; // in case the packed page is a bit larger than the raw one
	dst.buffer = (char *)VSIMalloc(dst.size);
        if( dst.buffer == NULL )
        {
            CPLFree(data);
            CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate %d bytes",
                     static_cast<int>(dst.size));
            return CE_Failure;
        }

	if (ZUnPack(src, dst, deflate_flags)) {
	    // Got it unpacked, update the pointers
	    CPLFree(data);
	    tinfo.size = dst.size;
	    data = dst.buffer;
	} else { // Warn and assume the data was not deflated
	    CPLError(CE_Warning, CPLE_AppDefined, "Can't inflate page!");
	    CPLFree(dst.buffer);
	}
    }

    src.buffer = (char *)data;
    src.size = static_cast<size_t>(tinfo.size);

    // After unpacking, the size has to be pageSizeBytes
    dst.buffer = (char *)buffer;
    dst.size = img.pageSizeBytes;

    // If pages are interleaved, use the dataset page buffer instead
    if (1!=cstride)
	dst.buffer = (char *)poDS->GetPBuffer();

    CPLErr ret = Decompress(dst, src);
    dst.size = img.pageSizeBytes; // In case the decompress failed, force it back
    CPLFree(data);

    // Swap whatever we decompressed if we need to
    if (is_Endianess_Dependent(img.dt,img.comp) && (img.nbo != NET_ORDER) ) 
	swab_buff(dst, img);

    // If pages are separate, we're done, the read was in the output buffer
    if ( 1 == cstride || CE_None != ret)
	return ret;

    // De-interleave page and return
    return RB(xblk, yblk, dst, buffer);
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

CPLErr GDALMRFRasterBand::IWriteBlock(int xblk, int yblk, void *buffer)

{
    GInt32 cstride = img.pagesize.c;
    ILSize req(xblk, yblk, 0, m_band/cstride, m_l);
    GUIntBig infooffset = IdxOffset(req, img);

    CPLDebug("MRF_IB", "IWriteBlock %d,%d,0,%d, level  %d, stride %d\n", xblk, yblk, 
	m_band, m_l, cstride);

    if (1 == cstride) {     // Separate bands, we can write it as is
	// Empty page skip

	int success;
	double val = GetNoDataValue(&success);
	if (!success) val = 0.0;
	if (isAllVal(eDataType, buffer, img.pageSizeBytes, val))
	    return poDS->WriteTile(NULL, infooffset, 0);

	// Use the pbuffer to hold the compressed page before writing it
	poDS->tile = ILSize(); // Mark it corrupt

	buf_mgr src;
        src.buffer = (char *)buffer;
        src.size = static_cast<size_t>(img.pageSizeBytes);
	buf_mgr dst = {(char *)poDS->GetPBuffer(), poDS->GetPBufferSize()};

	// Swab the source before encoding if we need to 
	if (is_Endianess_Dependent(img.dt, img.comp) && (img.nbo != NET_ORDER)) 
	    swab_buff(src, img);

	// Compress functions need to return the compressed size in
	// the bytes in buffer field
	Compress(dst, src);
	void *usebuff = dst.buffer;
	if (deflatep) {
	    usebuff = DeflateBlock(dst, poDS->pbsize - dst.size, deflate_flags);
	    if (!usebuff) {
		CPLError(CE_Failure,CPLE_AppDefined, "MRF: Deflate error");
		return CE_Failure;
	    }
	}
	return poDS->WriteTile(usebuff, infooffset , dst.size);
    }

    // Multiple bands per page, use a temporary to assemble the page
    // Temporary is large because we use it to hold both the uncompressed and the compressed
    poDS->tile=req; poDS->bdirty=0;

    // Keep track of what bands are empty
    GUIntBig empties=0;

    void *tbuffer = VSIMalloc(img.pageSizeBytes + poDS->pbsize);

    if (!tbuffer) {
	CPLError(CE_Failure,CPLE_AppDefined, "MRF: Can't allocate write buffer");
	return CE_Failure;
    }
	
    // Get the other bands from the block cache
    for (int iBand=0; iBand < poDS->nBands; iBand++ )
    {
	const char *pabyThisImage=NULL;
	GDALRasterBlock *poBlock=NULL;

	if (iBand == m_band)
	{
	    pabyThisImage = (char *) buffer;
	    poDS->bdirty |= bandbit();
	} else {
	    GDALRasterBand *band = poDS->GetRasterBand(iBand +1);
	    // Pick the right overview
	    if (m_l) band = band->GetOverview(m_l -1);
	    poBlock = ((GDALMRFRasterBand *)band)
		->TryGetLockedBlockRef(xblk, yblk);
	    if (NULL==poBlock) continue;
	    // This is where the image data is for this band

	    pabyThisImage = (char*) poBlock->GetDataRef();
	    poDS->bdirty |= bandbit(iBand);
	}

	// Keep track of empty bands, but encode them anyhow, in case some are not empty
	int success;
	double val = GetNoDataValue(&success);
	if (!success) val = 0.0;
        if (isAllVal(eDataType, (char *)pabyThisImage, blockSizeBytes(), val))
	    empties |= bandbit(iBand);

	// Copy the data into the dataset buffer here
	// Just the right mix of templates and macros make this real tidy
#define CpySO(T) cpy_stride_out<T> (((T *)tbuffer)+iBand, pabyThisImage,\
		blockSizeBytes()/sizeof(T), cstride)


	// Build the page in tbuffer
	switch (GDALGetDataTypeSize(eDataType)/8)
	{
	    case 1: CpySO(GByte); break;
	    case 2: CpySO(GInt16); break;
	    case 4: CpySO(GInt32); break;
	    case 8: CpySO(GIntBig); break;
	    default:
            {
		CPLError(CE_Failure,CPLE_AppDefined, "MRF: Write datatype of %d bytes "
			"not implemented", GDALGetDataTypeSize(eDataType)/8);
                if (poBlock != NULL)
                {
                    poBlock->MarkClean();
                    poBlock->DropLock();
                }
                CPLFree(tbuffer);
		return CE_Failure;
            }
	}

	if (poBlock != NULL)
	{
	    poBlock->MarkClean();
	    poBlock->DropLock();
	}
    }

    // Should keep track of the individual band buffers and only mix them if this is not
    // an empty page ( move the Copy with Stride Out from above below this test
    // This way works fine, but it does work extra for empty pages

    if (GIntBig(empties) == AllBandMask()) {
	CPLFree(tbuffer);
	return poDS->WriteTile(NULL, infooffset, 0);
    }

    if (poDS->bdirty != AllBandMask())
	CPLError(CE_Warning, CPLE_AppDefined,
	"MRF: IWrite, band dirty mask is " CPL_FRMT_GIB " instead of " CPL_FRMT_GIB,
	poDS->bdirty, AllBandMask());

    buf_mgr src;
    src.buffer = (char *)tbuffer;
    src.size = static_cast<size_t>(img.pageSizeBytes);

    // Use the space after pagesizebytes for compressed output, it is of pbsize
    char *outbuff = (char *)tbuffer + img.pageSizeBytes;

    buf_mgr dst = {outbuff, poDS->pbsize};
    CPLErr ret;

    ret = Compress(dst, src);
    if (ret != CE_None) {
        // Compress failed, write it as an empty tile
        CPLFree(tbuffer);
        poDS->WriteTile(NULL, infooffset, 0);
        return CE_None; // Should report the error, but it triggers partial band attempts
    }

    // Where the output is, in case we deflate
    void *usebuff = outbuff;
    if (deflatep) {
	// Move the packed part at the start of tbuffer, to make more space available
	memcpy(tbuffer, outbuff, dst.size);
	dst.buffer = (char *)tbuffer;
	usebuff = DeflateBlock(dst, img.pageSizeBytes + poDS->pbsize - dst.size, deflate_flags);
	if (!usebuff) {
	    CPLError(CE_Failure,CPLE_AppDefined, "MRF: Deflate error");
	    CPLFree(tbuffer);
            poDS->WriteTile(NULL, infooffset, 0);
            poDS->bdirty = 0;
	    return CE_Failure;
	}
    }

    ret = poDS->WriteTile(usebuff, infooffset, dst.size);
    CPLFree(tbuffer);

    poDS->bdirty = 0;
    return ret;
}

int GDALMRFRasterBand::GetOverviewCount()
{
    // First try internal overviews
    int nInternalOverviewCount = static_cast<int>(overviews.size());
    if( nInternalOverviewCount > 0 )
        return nInternalOverviewCount;
    // Fallback to PAM external overviews
    return GDALPamRasterBand::GetOverviewCount();
}

GDALRasterBand* GDALMRFRasterBand::GetOverview(int n)
{
    // First try internal overviews
    if( n >= 0 && n < (int)overviews.size() )
        return overviews[n];
    // Fallback to PAM external overviews
    return GDALPamRasterBand::GetOverview(n);
}

NAMESPACE_MRF_END
