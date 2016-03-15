/*
Copyright 2013-2015 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
A local copy of the license and additional notices are located with the
source distribution at:
http://github.com/Esri/lerc/

LERC band implementation
LERC page compression and decompression functions

Contributors:  Lucian Plesea
*/

#include "marfa.h"
#include <algorithm>
#include <CntZImage.h>
#include <Lerc2.h>

USING_NAMESPACE_LERC

NAMESPACE_MRF_START

// Load a buffer into a zImg
template <typename T> void CntZImgFill(CntZImage &zImg, T *src, const ILImage &img)
{
    int w = img.pagesize.x;
    int h = img.pagesize.y;

    zImg.resize(w, h);
    T *ptr = src;

    // No data value
    float ndv = float(img.NoDataValue);
    if (!img.hasNoData) ndv = 0;

    for (int i = 0; i < h; i++)
	for (int j = 0; j < w; j++) {
	    zImg(i, j).z = float(*ptr++);
	    zImg(i, j).cnt = !CPLIsEqual(zImg(i, j).z, ndv);
	}
    return;
}

// Unload a zImg into a buffer
template <typename T> void CntZImgUFill(CntZImage &zImg, T *dst, const ILImage &img)
{
    int h = static_cast<int>(zImg.getHeight());
    int w = static_cast<int>(zImg.getWidth());
    T *ptr = dst;
    T ndv = (T)(img.NoDataValue);
    // Use 0 if nodata is not defined
    if (!img.hasNoData) ndv = 0;
    for (int i = 0; i < h; i++)
	for (int j = 0; j < w; j++)
	    *ptr++ = (zImg(i, j).cnt == 0) ? ndv : (T)(zImg(i, j).z);
}

//  LERC 1 compression
static CPLErr CompressLERC(buf_mgr &dst, buf_mgr &src, const ILImage &img, double precision)
{
    CntZImage zImg;
    // Fill data into zImg
#define FILL(T) CntZImgFill(zImg, (T *)(src.buffer), img)
    switch (img.dt) {
    case GDT_Byte:	FILL(GByte);	break;
    case GDT_UInt16:	FILL(GUInt16);	break;
    case GDT_Int16:	FILL(GInt16);	break;
    case GDT_Int32:	FILL(GInt32);	break;
    case GDT_UInt32:	FILL(GUInt32);	break;
    case GDT_Float32:	FILL(float);	break;
    case GDT_Float64:	FILL(double);	break;
    default: break;
    }
#undef FILL

    Byte *ptr = (Byte *)dst.buffer;

    // if it can't compress in output buffer it will crash
    if (!zImg.write(&ptr, precision)) {
	CPLError(CE_Failure,CPLE_AppDefined,"MRF: Error during LERC compression");
	return CE_Failure;
    }

    // write changes the value of the pointer, we can find the size by testing how far it moved
    dst.size = ptr - (Byte *)dst.buffer;
    CPLDebug("MRF_LERC","LERC Compressed to %d\n", (int)dst.size);
    return CE_None;
}

static CPLErr DecompressLERC(buf_mgr &dst, buf_mgr &src, const ILImage &img)
{
    CntZImage zImg;
    Byte *ptr = (Byte *)src.buffer;
    if (!zImg.read(&ptr, 1e12))
    {
	CPLError(CE_Failure,CPLE_AppDefined,"MRF: Error during LERC decompression");
	return CE_Failure;
    }

// Unpack from zImg to dst buffer, calling the right type
#define UFILL(T) CntZImgUFill(zImg, (T *)(dst.buffer), img)
    switch (img.dt) {
    case GDT_Byte:	UFILL(GByte);	break;
    case GDT_UInt16:	UFILL(GUInt16); break;
    case GDT_Int16:	UFILL(GInt16);	break;
    case GDT_Int32:	UFILL(GInt32);	break;
    case GDT_UInt32:	UFILL(GUInt32); break;
    case GDT_Float32:	UFILL(float);	break;
    case GDT_Float64:	UFILL(double);	break;
    default: break;
    }
#undef UFILL

    return CE_None;
}

// Populate a bitmask based on comparison with the image no data value
// Returns the number of NoData values found
template <typename T> int MaskFill(BitMask2 &bitMask, T *src, const ILImage &img)
{
    int w = img.pagesize.x;
    int h = img.pagesize.y;
    int count = 0;

    bitMask.SetSize(w, h);
    bitMask.SetAllValid();
    T *ptr = src;

    // No data value
    T ndv = T(img.NoDataValue);
    if (!img.hasNoData) ndv = 0; // It really doesn't get called when img doesn't have NoDataValue

    for (int i = 0; i < h; i++)
	for (int j = 0; j < w; j++)
	    if (ndv == *ptr++) {
		bitMask.SetInvalid(i, j);
		count++;
	    }

    return count;
}

static CPLErr CompressLERC2(buf_mgr &dst, buf_mgr &src, const ILImage &img, double precision)
{
    int w = img.pagesize.x;
    int h = img.pagesize.y;
    // So we build a bitmask to pass a pointer to bytes, which gets converted to a bitmask?
    BitMask2 bitMask;
    int ndv_count = 0;
    if (img.hasNoData) { // Only build a bitmask if no data value is defined
	switch (img.dt) {

#define MASK(T) ndv_count = MaskFill(bitMask, (T *)(src.buffer), img)

	case GDT_Byte:		MASK(GByte);	break;
	case GDT_UInt16:	MASK(GUInt16);	break;
	case GDT_Int16:		MASK(GInt16);	break;
	case GDT_Int32:		MASK(GInt32);	break;
	case GDT_UInt32:	MASK(GUInt32);	break;
	case GDT_Float32:	MASK(float);	break;
	case GDT_Float64:	MASK(double);	break;
        default:                CPLAssert(FALSE); break;

#undef MASK
	}
    }
    // Set bitmask if it has some ndvs
    Lerc2 lerc2(w, h, (ndv_count == 0) ? NULL : bitMask.Bits());
    bool success = false;
    Byte *ptr = (Byte *)dst.buffer;

    long sz = 0;
    switch (img.dt) {

#define ENCODE(T) if (true) { \
    sz = lerc2.ComputeNumBytesNeededToWrite((T *)(src.buffer), precision, ndv_count != 0);\
    success = lerc2.Encode((T *)(src.buffer), &ptr);\
    }

    case GDT_Byte:	ENCODE(GByte);	    break;
    case GDT_UInt16:	ENCODE(GUInt16);    break;
    case GDT_Int16:	ENCODE(GInt16);	    break;
    case GDT_Int32:	ENCODE(GInt32);	    break;
    case GDT_UInt32:	ENCODE(GUInt32);    break;
    case GDT_Float32:	ENCODE(float);	    break;
    case GDT_Float64:	ENCODE(double);	    break;
    default:            CPLAssert(FALSE); break;

#undef ENCODE
    }

    // write changes the value of the pointer, we can find the size by testing how far it moved
    dst.size = (char *)ptr - dst.buffer;
    if (!success || sz != static_cast<long>(dst.size)) {
	CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during LERC2 compression");
	return CE_Failure;
    }
    CPLDebug("MRF_LERC", "LERC2 Compressed to %d\n", (int)sz);
    return CE_None;
}

// Populate a bitmask based on comparison with the image no data value
template <typename T> void UnMask(BitMask2 &bitMask, T *arr, const ILImage &img)
{
    int w = img.pagesize.x;
    int h = img.pagesize.y;
    if (w * h == bitMask.CountValidBits())
	return;
    T *ptr = arr;
    T ndv = T(img.NoDataValue);
    if (!img.hasNoData) ndv = 0; // It doesn't get called when img doesn't have NoDataValue
    for (int i = 0; i < h; i++)
	for (int j = 0; j < w; j++, ptr++)
	    if (!bitMask.IsValid(i, j))
		*ptr = ndv;
    return;
}

CPLErr LERC_Band::Decompress(buf_mgr &dst, buf_mgr &src)
{
    const Byte *ptr = (Byte *)(src.buffer);
    Lerc2::HeaderInfo hdInfo;
    Lerc2 lerc2;
    if (!lerc2.GetHeaderInfo(ptr, hdInfo))
	return DecompressLERC(dst, src, img);
    // It is lerc2 here
    bool success = false;
    BitMask2 bitMask(img.pagesize.x, img.pagesize.y);
    switch (img.dt) {
#define DECODE(T) success = lerc2.Decode(&ptr, reinterpret_cast<T *>(dst.buffer), bitMask.Bits())
    case GDT_Byte:	DECODE(GByte);	    break;
    case GDT_UInt16:	DECODE(GUInt16);    break;
    case GDT_Int16:	DECODE(GInt16);	    break;
    case GDT_Int32:	DECODE(GInt32);	    break;
    case GDT_UInt32:	DECODE(GUInt32);    break;
    case GDT_Float32:	DECODE(float);	    break;
    case GDT_Float64:	DECODE(double);	    break;
    default:            CPLAssert(FALSE);   break;
#undef DECODE
    }
    if (!success) {
	CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during LERC2 decompression");
	return CE_Failure;
    }
    if (!img.hasNoData)
	return CE_None;

    // Fill in no data values
    switch (img.dt) {
#define UNMASK(T) UnMask(bitMask, reinterpret_cast<T *>(dst.buffer), img)
    case GDT_Byte:	UNMASK(GByte);	    break;
    case GDT_UInt16:	UNMASK(GUInt16);    break;
    case GDT_Int16:	UNMASK(GInt16);	    break;
    case GDT_Int32:	UNMASK(GInt32);	    break;
    case GDT_UInt32:	UNMASK(GUInt32);    break;
    case GDT_Float32:	UNMASK(float);	    break;
    case GDT_Float64:	UNMASK(double);	    break;
    default:            CPLAssert(FALSE);   break;
#undef DECODE
    }
    return CE_None;
}

CPLErr LERC_Band::Compress(buf_mgr &dst, buf_mgr &src)
{
    if (version == 2)
	return CompressLERC2(dst, src, img, precision);
    else
	return CompressLERC(dst, src, img, precision);
}

LERC_Band::LERC_Band(GDALMRFDataset *pDS, const ILImage &image, int b, int level):
    GDALMRFRasterBand(pDS, image, b, level)
{
    // Pick 1/1000 for floats and 0.5 losless for integers
    if (eDataType == GDT_Float32 || eDataType == GDT_Float64 )
	precision = strtod(GetOptionValue( "LERC_PREC" , ".001" ),NULL);
    else
	precision = std::max(0.5, strtod(GetOptionValue("LERC_PREC", ".5"), NULL));

    // Encode in V2 by default
    version = GetOptlist().FetchBoolean("V1", FALSE) ? 1:2;

    // Enlarge the page buffer in this case, LERC may expand data
    pDS->SetPBufferSize( 2 * image.pageSizeBytes);
}

LERC_Band::~LERC_Band()
{
}


NAMESPACE_MRF_END
