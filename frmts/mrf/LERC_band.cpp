/*
Copyright 2013-2021 Esri
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

Authors:  Lucian Plesea
*/

#include "marfa.h"
#include <algorithm>
#include <vector>
#include "LERCV1/Lerc1Image.h"

// Requires lerc at least 2v4, where the c_api changed, but there is no good way to check
#include <Lerc_c_api.h>
#include <Lerc_types.h>

#ifndef LERC_AT_LEAST_VERSION
#define LERC_AT_LEAST_VERSION(maj,min,patch) 0
#endif

// name of internal or external libLerc namespace
#if defined(USING_NAMESPACE_LERC)
#define L2NS GDAL_LercNS
#else
// External lerc
#define L2NS LercNS
#endif

USING_NAMESPACE_LERC1
NAMESPACE_MRF_START

// Read an unaligned 4 byte little endian int from location p, advances pointer
static void READ_GINT32(int& X, const char*& p) {
    memcpy(&X, p, sizeof(GInt32));
    p+= sizeof(GInt32);
}

static void READ_FLOAT(float& X, const char*& p) {
    memcpy(&X, p, sizeof(float));
    p+= sizeof(float);
}

//
// Check that a buffer contains a supported Lerc1 blob, the type supported by MRF
// Can't really check everything without decoding, this just checks the main structure
// returns actual size if it is Lerc1 with size < sz
// returns 0 if format doesn't match
// returns -1 if Lerc1 but size can't be determined
//
// returns -<actual size> if actual size > sz

static int checkV1(const char *s, size_t sz)
{
    GInt32 nBytesMask, nBytesData;

    // Header is 34 bytes
    // band header is 16, first mask band then data band
    if (sz < static_cast<size_t>(Lerc1Image::computeNumBytesNeededToWriteVoidImage()))
        return 0;
    // First ten bytes are ASCII signature
    if (!STARTS_WITH(s, "CntZImage "))
        return 0;
    s += 10;

    // Version 11
    int i;
    READ_GINT32(i, s);
    if (i != 11) return 0;

    // Type 8 is CntZ
    READ_GINT32(i, s);
    if (i != 8) return 0;

    // Height
    READ_GINT32(i, s); // Arbitrary number in Lerc1Image::read()
    if (i > 20000 || i <= 0) return 0;

    // Width
    READ_GINT32(i, s);
    if (i > 20000 || i <= 0) return 0;

    // Skip the max val stored as double
    s += sizeof(double);

    // First header should be the mask, which mean 0 blocks
    // Height
    READ_GINT32(i, s);
    if (i != 0) return 0;

    // WIDTH
    READ_GINT32(i, s);
    if (i != 0) return 0;

    READ_GINT32(nBytesMask, s);
    if (nBytesMask < 0) return 0;

    // mask max value, 0 or 1 as float
    float val;
    READ_FLOAT(val, s);
    if (val != 0.0f && val != 1.0f) return 0;

    // If data header can't be read the actual size is unknown
    if (nBytesMask > INT_MAX - 66 ||
        static_cast<size_t>(66 + nBytesMask) >= sz)
    {
        return -1;
    }

    s += nBytesMask;

    // Data Band header
    READ_GINT32(i, s); // number of full height blocks, never single pixel blocks
    if (i <= 0 || i > 10000)
        return 0;

    READ_GINT32(i, s); // number of full width blocks, never single pixel blocks
    if (i <= 0 || i > 10000)
        return 0;

    READ_GINT32(nBytesData, s);
    if (nBytesData < 0) return 0;

    // Actual LERC blob size
    if( 66 + nBytesMask > INT_MAX - nBytesData )
        return -1;
    int size = static_cast<int>(66 + nBytesMask + nBytesData);
    return (static_cast<size_t>(size) > sz) ? -size : size;
}


// Load a buffer of type T into a LERC1 zImg, with a given stride
template <typename T> static void Lerc1ImgFill(Lerc1Image& zImg, T* src, const ILImage& img, GInt32 stride)
{
    int w = img.pagesize.x;
    int h = img.pagesize.y;
    zImg.resize(w, h);
    const float ndv = static_cast<float>(img.hasNoData ? img.NoDataValue : 0);
    if (stride == 1) {
        for (int row = 0; row < h; row++)
            for (int col = 0; col < w; col++) {
                float val = static_cast<float>(*src++);
                zImg(row, col) = val;
                zImg.SetMask(row, col, !CPLIsEqual(ndv, val));
            }
        return;
    }
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++) {
            float val = static_cast<float>(*src);
            src += stride;
            zImg(row, col) = val;
            zImg.SetMask(row, col, !CPLIsEqual(ndv, val));
        }
}

// Unload LERC1 zImg into a type T buffer
template <typename T> static bool Lerc1ImgUFill(Lerc1Image &zImg, T *dst, const ILImage &img, GInt32 stride)
{
    const T ndv = static_cast<T>(img.hasNoData ? img.NoDataValue : 0);
    if (img.pagesize.y != zImg.getHeight() || img.pagesize.x != zImg.getWidth())
        return false;
    int w = img.pagesize.x;
    int h = img.pagesize.y;
    if (1 == stride) {
        for (int row = 0; row < h; row++)
            for (int col = 0; col < w; col++)
                *dst++ = zImg.IsValid(row, col) ? static_cast<T>(zImg(row, col)) : ndv;
        return true;
    }
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++) {
            *dst = zImg.IsValid(row, col) ? static_cast<T>(zImg(row, col)) : ndv;
            dst += stride;
        }
    return true;
}

static CPLErr CompressLERC1(buf_mgr &dst, buf_mgr &src, const ILImage &img, double precision)
{
    Lerc1Image zImg;
    GInt32 stride = img.pagesize.c;
    Lerc1NS::Byte* ptr = reinterpret_cast<Lerc1NS::Byte*>(dst.buffer);

    for (int c = 0; c < stride; c++) {
#define FILL(T) Lerc1ImgFill(zImg, reinterpret_cast<T *>(src.buffer) + c, img, stride)
        switch (img.dt) {
        case GDT_Byte:      FILL(GByte);    break;
        case GDT_UInt16:    FILL(GUInt16);  break;
        case GDT_Int16:     FILL(GInt16);   break;
        case GDT_Int32:     FILL(GInt32);   break;
        case GDT_UInt32:    FILL(GUInt32);  break;
        case GDT_Float32:   FILL(float);    break;
        case GDT_Float64:   FILL(double);   break;
        default: break;
        }
#undef FILL
        if (!zImg.write(&ptr, precision)) {
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during LERC compression");
            return CE_Failure;
        }
    }

    // write changes the value of the pointer, we can find the size by testing how far it moved
    // Add a couple of bytes, to avoid buffer overflow on reading
    dst.size = reinterpret_cast<char *>(ptr) - dst.buffer + PADDING_BYTES;
    CPLDebug("MRF_LERC","LERC Compressed to %d\n", (int)dst.size);
    return CE_None;
}

// LERC 1 Decompression
static CPLErr DecompressLERC1(buf_mgr &dst, buf_mgr &src, const ILImage &img)
{
    Lerc1Image zImg;

    // need to add the padding bytes so that out-of-buffer-access
    size_t nRemainingBytes = src.size + PADDING_BYTES;
    Lerc1NS::Byte *ptr = reinterpret_cast<Lerc1NS::Byte *>(src.buffer);
    GInt32 stride = img.pagesize.c;
    for (int c = 0; c < stride; c++) {
        // Check that input passes snicker test
        if (checkV1(reinterpret_cast<char *>(ptr), nRemainingBytes) <= 0) {
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: LERC1 tile format error");
            return CE_Failure;
        }

        if (!zImg.read(&ptr, nRemainingBytes, 1e12))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during LERC decompression");
            return CE_Failure;
        }

        // Unpack from zImg to dst buffer, calling the right type
        bool success = false;
#define UFILL(T) success = Lerc1ImgUFill(zImg, reinterpret_cast<T *>(dst.buffer) + c, img, stride)
        switch (img.dt) {
        case GDT_Byte:      UFILL(GByte);   break;
        case GDT_UInt16:    UFILL(GUInt16); break;
        case GDT_Int16:     UFILL(GInt16);  break;
        case GDT_Int32:     UFILL(GInt32);  break;
        case GDT_UInt32:    UFILL(GUInt32); break;
        case GDT_Float32:   UFILL(float);   break;
        case GDT_Float64:   UFILL(double);  break;
        default: break;
        }
#undef UFILL
        if (!success) {
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during LERC decompression");
            return CE_Failure;
        }
    }

    return CE_None;
}

// Lerc2

static GDALDataType L2toGDT(L2NS::DataType L2type) {
    GDALDataType dt;
    switch (L2type) {
    case L2NS::DataType::dt_short: dt = GDT_Int16; break;
    case L2NS::DataType::dt_ushort: dt = GDT_UInt16; break;
    case L2NS::DataType::dt_int: dt = GDT_Int32; break;
    case L2NS::DataType::dt_uint: dt = GDT_UInt32; break;
    case L2NS::DataType::dt_float: dt = GDT_Float32; break;
    case L2NS::DataType::dt_double: dt = GDT_Float64; break;
    default: dt = GDT_Byte; // GDAL doesn't have a signed char type
    }
    return dt;
}

static L2NS::DataType GDTtoL2(GDALDataType dt) {
    L2NS::DataType L2dt;
    switch (dt) {
    case GDT_Int16: L2dt = L2NS::DataType::dt_short; break;
    case GDT_UInt16: L2dt = L2NS::DataType::dt_ushort; break;
    case GDT_Int32: L2dt = L2NS::DataType::dt_int; break;
    case GDT_UInt32: L2dt = L2NS::DataType::dt_uint; break;
    case GDT_Float32: L2dt = L2NS::DataType::dt_float; break;
    case GDT_Float64: L2dt = L2NS::DataType::dt_double; break;
    default: L2dt = L2NS::DataType::dt_uchar;
    }
    return L2dt;
}

// Populate a LERC2 bitmask based on comparison with the image no data value
// Returns the number of NoData values found
template <typename T> static size_t MaskFill(std::vector<Lerc1NS::Byte> &bm, const T *src, const ILImage &img)
{
    size_t w = static_cast<size_t>(img.pagesize.x);
    size_t h = static_cast<size_t>(img.pagesize.y);
    size_t stride = static_cast<size_t>(img.pagesize.c);
    size_t nndv = 0;

    bm.resize(w * h);

    T ndv = static_cast<T>(img.NoDataValue);
    if (!img.hasNoData) ndv = 0; // It really doesn't get called when img doesn't have NoDataValue
    for (size_t i = 0; i < bm.size(); i++) {
        if (ndv == src[i * stride]) {
            bm[i] = 0;
            nndv++;
        }
        else {
            bm[i] = 1;
        }
    }

    return nndv;
}

// Fill in no data values based on a LERC2 bitmask
template <typename T> static void UnMask(std::vector<Lerc1NS::Byte>& bm, T* data, const ILImage& img)
{
    size_t w = static_cast<size_t>(img.pagesize.x);
    size_t h = static_cast<size_t>(img.pagesize.y);
    size_t stride = static_cast<size_t>(img.pagesize.c);

    if (bm.size() != w * h)
        return;

    T ndv = T(img.NoDataValue);
    if (stride == 1) {
        for (size_t i = 0; i < w * h; i++)
            if (!bm[i])
                data[i] = ndv;
    }
    else {
        for (size_t i = 0; i < w * h; i++)
            if (!bm[i])
                for (size_t c = 0; c < stride; c++)
                    data[i * stride + c] = ndv;
    }
}

static CPLErr CompressLERC2(buf_mgr &dst, buf_mgr &src, const ILImage &img, double precision, int l2ver)
{
    auto w = static_cast<int>(img.pagesize.x);
    auto h = static_cast<int>(img.pagesize.y);
    auto stride = static_cast<int>(img.pagesize.c);

    // build a mask
    std::vector<Lerc1NS::Byte> bm;
    size_t nndv = 0;
    if (img.hasNoData) { // Only build a bitmask if no data value is defined
        switch (img.dt) {

#define MASK(T) nndv = MaskFill(bm, reinterpret_cast<T *>(src.buffer), img)

        case GDT_Byte:          MASK(GByte);    break;
        case GDT_UInt16:        MASK(GUInt16);  break;
        case GDT_Int16:         MASK(GInt16);   break;
        case GDT_Int32:         MASK(GInt32);   break;
        case GDT_UInt32:        MASK(GUInt32);  break;
        case GDT_Float32:       MASK(float);    break;
        case GDT_Float64:       MASK(double);   break;
        default:                break;

#undef MASK
        }
    }

    unsigned int sz = 0;
    auto pbm = bm.data();
    if (!bm.empty() && nndv != bm.size())
        pbm = nullptr;
    auto status = lerc_encodeForVersion(
        reinterpret_cast<void*>(src.buffer), l2ver,
        static_cast<unsigned int>(GDTtoL2(img.dt)), stride, w, h, 1,
#if LERC_AT_LEAST_VERSION(3,0,0)
        pbm ? 1 : 0,
#endif
        pbm, precision,
        reinterpret_cast<Lerc1NS::Byte*>(dst.buffer), static_cast<unsigned int>(dst.size), &sz);

    if (L2NS::ErrCode::Ok != static_cast<L2NS::ErrCode>(status) || sz > dst.size) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during LERC2 compression");
        return CE_Failure;
    }

    dst.size = static_cast<size_t>(sz);
    return CE_None;
}

// LERC1 splits of early, so this is mostly LERC2
CPLErr LERC_Band::Decompress(buf_mgr& dst, buf_mgr& src)
{
    if (src.size >= Lerc1Image::computeNumBytesNeededToWriteVoidImage() && IsLerc1(src.buffer))
        return DecompressLERC1(dst, src, img);

    // Can only be LERC2 here, verify
    if (src.size < 50 || !IsLerc2(src.buffer)) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Not a lerc tile");
        return CE_Failure;
    }

    auto w = static_cast<int>(img.pagesize.x);
    auto h = static_cast<int>(img.pagesize.y);
    auto stride = static_cast<int>(img.pagesize.c);

    std::vector<Lerc1NS::Byte> bm;
    if (img.hasNoData)
        bm.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    auto pbm = bm.data();
    if (bm.empty())
        pbm = nullptr;

    // Decoding may fail for many different reasons, including input not matching tile expectations
    auto status = lerc_decode(reinterpret_cast<Lerc1NS::Byte*>(src.buffer),
        static_cast<unsigned int>(src.size),
#if LERC_AT_LEAST_VERSION(3,0,0)
        pbm ? 1 : 0,
#endif
        pbm, stride, w, h, 1, static_cast<unsigned int>(GDTtoL2(img.dt)), dst.buffer);
    if (L2NS::ErrCode::Ok != static_cast<L2NS::ErrCode>(status)) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error decoding Lerc");
        return CE_Failure;
    }

    // No mask means we're done
    if (bm.empty())
        return CE_None;

    // Fill in no data values
    switch (img.dt) {
#define UNMASK(T) UnMask(bm, reinterpret_cast<T *>(dst.buffer), img)
    case GDT_Byte:      UNMASK(GByte);      break;
    case GDT_UInt16:    UNMASK(GUInt16);    break;
    case GDT_Int16:     UNMASK(GInt16);     break;
    case GDT_Int32:     UNMASK(GInt32);     break;
    case GDT_UInt32:    UNMASK(GUInt32);    break;
    case GDT_Float32:   UNMASK(float);      break;
    case GDT_Float64:   UNMASK(double);     break;
    default:            break;
#undef DECODE
    }
    return CE_None;
}

CPLErr LERC_Band::Compress(buf_mgr &dst, buf_mgr &src)
{
    if (version == 2)
        return CompressLERC2(dst, src, img, precision, l2ver);
    else
        return CompressLERC1(dst, src, img, precision);
}

CPLXMLNode *LERC_Band::GetMRFConfig(GDALOpenInfo *poOpenInfo)
{
    // Header of Lerc2 takes 58 bytes, an empty area 62 or more, depending on the subversion.
    // Size of Lerc1 empty file is 67
    // Anything under 50 bytes can't be lerc
    if (poOpenInfo->eAccess != GA_ReadOnly
        || poOpenInfo->pszFilename == nullptr
        || poOpenInfo->pabyHeader == nullptr
        || strlen(poOpenInfo->pszFilename) < 1
        || poOpenInfo->nHeaderBytes < 50)
        return nullptr;

    // Check the header too
    char *psz = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    CPLString sHeader;
    sHeader.assign(psz, psz + poOpenInfo->nHeaderBytes);
    if (!(IsLerc1(sHeader) || IsLerc2(sHeader)))
        return nullptr;

    GDALDataType dt = GDT_Unknown; // Use this as a validity flag

    // Use this structure to fetch width and height
    ILSize size(-1, -1, 1, 1, 1);

    if (IsLerc1(sHeader) && sHeader.size() >= Lerc1Image::computeNumBytesNeededToWriteVoidImage()) {
        if (Lerc1Image::getwh(reinterpret_cast<Lerc1NS::Byte*>(psz), poOpenInfo->nHeaderBytes,
            size.x, size.y))
            dt = GDALGetDataTypeByName(
                CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "DATATYPE", "Byte"));
    }
    else if (IsLerc2(sHeader)) {
        // getBlobInfo will fail without the whole LERC blob
        // Wasteful, but that's the only choice given by the LERC C API
        // This will only work if the Lerc2 file is under the constant defined here
        static const GIntBig MAX_L2SIZE(10 * 1024 * 1024); // 10MB
        GByte* buffer = nullptr;
        vsi_l_offset l2size;

#define INFOIDX(T) static_cast<size_t>(L2NS::InfoArrOrder::T)

        if (VSIIngestFile(nullptr, poOpenInfo->pszFilename, &buffer, &l2size, MAX_L2SIZE)) {
            //! Info returned in infoArray is { version, dataType, nDim, nCols, nRows, nBands, nValidPixels... }, see Lerc_types.h .
            std::vector<unsigned int> info(INFOIDX(nValidPixels) + 1);
            auto status = lerc_getBlobInfo(reinterpret_cast<Lerc1NS::Byte*>(buffer), static_cast<unsigned int>(l2size),
                info.data(), nullptr, static_cast<int>(info.size()), 0);
            VSIFree(buffer);
            if (L2NS::ErrCode::Ok == static_cast<L2NS::ErrCode>(status) && 1 == info[INFOIDX(nBands)]) {
                size.x = info[INFOIDX(nCols)];
                size.y = info[INFOIDX(nRows)];
                if (info[INFOIDX(version)] > 3) // Single band before version 4
                    size.c = info[INFOIDX(nDim)];
                dt = L2toGDT(static_cast<L2NS::DataType>(info[INFOIDX(dataType)]));
            }
        }
    }

    if (size.x <=0 || size.y <=0 || dt == GDT_Unknown)
        return nullptr;

    // Build and return the MRF configuration for a single tile reader
    CPLXMLNode *config = CPLCreateXMLNode(nullptr, CXT_Element, "MRF_META");
    CPLXMLNode *raster = CPLCreateXMLNode(config, CXT_Element, "Raster");
    XMLSetAttributeVal(raster, "Size", size, "%.0f");
    XMLSetAttributeVal(raster, "PageSize", size, "%.0f");
    CPLCreateXMLElementAndValue(raster, "Compression", CompName(IL_LERC));
    CPLCreateXMLElementAndValue(raster, "DataType", GDALGetDataTypeName(dt));
    CPLCreateXMLElementAndValue(raster, "DataFile", poOpenInfo->pszFilename);
    // Set a magic index file name to prevent the driver from attempting to open it
    CPLCreateXMLElementAndValue(raster, "IndexFile", "(null)");
    // The NDV could be passed as an open option
    const char* pszNDV = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "NDV", "");
    if (strlen(pszNDV) > 0) {
        CPLXMLNode* values = CPLCreateXMLNode(raster, CXT_Element, "DataValues");
        XMLSetAttributeVal(values, "NoData", pszNDV);
    }
    return config;
}

LERC_Band::LERC_Band(MRFDataset *pDS, const ILImage &image,
                      int b, int level ) :
    MRFRasterBand(pDS, image, b, level)
{
    // Pick 1/1000 for floats and 0.5 losless for integers.
    if (eDataType == GDT_Float32 || eDataType == GDT_Float64 )
        precision = strtod(GetOptionValue( "LERC_PREC" , ".001" ),nullptr);
    else
        precision =
            std::max(0.5, strtod(GetOptionValue("LERC_PREC", ".5"), nullptr));

    // Encode in V2 by default.
    version = GetOptlist().FetchBoolean("V1", FALSE) ? 1 : 2;
    // For LERC 2 there are multiple versions too, -1 means use the library default
    // Use v2.2 for single band encoding
    l2ver = atoi(GetOptlist().FetchNameValueDef("L2_VER", (img.pagesize.c == 1) ? "2" : "-1"));

    if( image.pageSizeBytes > INT_MAX / 4 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "LERC page too large");
        return;
    }
    // Enlarge the page buffer, LERC may expand data.
    pDS->SetPBufferSize( 2 * image.pageSizeBytes);
}

LERC_Band::~LERC_Band() {}

NAMESPACE_MRF_END
