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
#include "LERCV1/Lerc1Image.h"
#include <Lerc2.h>

USING_NAMESPACE_LERC1
USING_NAMESPACE_LERC

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

static GDALDataType GetL2DataType(Lerc2::DataType L2type) {
    GDALDataType dt;
    switch (L2type) {
    case Lerc2::DT_Byte:  dt = GDT_Byte; break;
    case Lerc2::DT_Short: dt = GDT_Int16; break;
    case Lerc2::DT_UShort: dt = GDT_UInt16; break;
    case Lerc2::DT_Int: dt = GDT_Int32; break;
    case Lerc2::DT_UInt: dt = GDT_UInt32; break;
    case Lerc2::DT_Float: dt = GDT_Float32; break;
    case Lerc2::DT_Double: dt = GDT_Float64; break;
    default: dt = GDT_Unknown;
    }
    return dt;
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

// Populate a LERC2 bitmask based on comparison with the image no data value
// Returns the number of NoData values found
template <typename T> static int MaskFill(BitMask &bitMask, T *src, const ILImage &img)
{
    int w = img.pagesize.x;
    int h = img.pagesize.y;
    int stride = img.pagesize.c;
    int count = 0;

    bitMask.SetSize(w, h);
    bitMask.SetAllValid();

    // No data value
    T ndv = static_cast<T>(img.NoDataValue);
    if (!img.hasNoData) ndv = 0; // It really doesn't get called when img doesn't have NoDataValue

    if (1 == stride) {
        for (int row = 0; row < h; row++)
            for (int col = 0; col < w; col++)
                if (ndv == *src++) {
                    bitMask.SetInvalid(row, col);
                    count++;
                }
    } else {
        // Test only the first band for the ndv value
        for (int row = 0; row < h; row++)
            for (int col = 0; col < w; col++, src += stride)
                if (ndv == *src) {
                    bitMask.SetInvalid(row, col);
                    count++;
                }
    }

    return count;
}

static CPLErr CompressLERC2(buf_mgr &dst, buf_mgr &src, const ILImage &img, double precision)
{
    int w = img.pagesize.x;
    int h = img.pagesize.y;
    int stride = img.pagesize.c;

    // So we build a bitmask to pass a pointer to bytes, which gets converted to a bitmask?
    BitMask bitMask;
    int ndv_count = 0;
    if (img.hasNoData) { // Only build a bitmask if no data value is defined
        switch (img.dt) {

#define MASK(T) ndv_count = MaskFill(bitMask, reinterpret_cast<T *>(src.buffer), img)

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

    GDAL_LercNS::Byte* ptr = reinterpret_cast<GDAL_LercNS::Byte*>(dst.buffer);
    size_t sz = 0;
    bool success = false;

    // Set bitmask if it has the ndv defined
    Lerc2 lerc2(stride, w, h, (ndv_count == 0) ? nullptr : bitMask.Bits());
    // Default to LERC2 V2 for single band, otherwise go with the default, currently >= 4
    if (stride == 1)
        lerc2.SetEncoderToOldVersion(2);

    switch (img.dt) {

#define ENCODE(T) if (true) {\
    sz = lerc2.ComputeNumBytesNeededToWrite(reinterpret_cast<T *>(src.buffer), precision, ndv_count != 0);\
    success = lerc2.Encode(reinterpret_cast<T *>(src.buffer), &ptr);\
}

    case GDT_Byte:      ENCODE(GByte);      break;
    case GDT_UInt16:    ENCODE(GUInt16);    break;
    case GDT_Int16:     ENCODE(GInt16);     break;
    case GDT_Int32:     ENCODE(GInt32);     break;
    case GDT_UInt32:    ENCODE(GUInt32);    break;
    case GDT_Float32:   ENCODE(float);      break;
    case GDT_Float64:   ENCODE(double);     break;
    default:            break;

#undef ENCODE
    }

    // write changes the value of the pointer, we can find the size by testing how far it moved
    dst.size = reinterpret_cast<char *>(ptr) - dst.buffer;
    if (!success || sz != dst.size) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during LERC2 compression");
        return CE_Failure;
    }
    CPLDebug("MRF_LERC", "LERC2 Compressed to %d\n", static_cast<int>(sz));
    return CE_None;
}

// Fill in no data values based on a LERC2 bitmask
template <typename T> static void UnMask(BitMask &bitMask, T *arr, const ILImage &img)
{
    int w = img.pagesize.x;
    int h = img.pagesize.y;
    int stride = img.pagesize.c;
    if (w * h == bitMask.CountValidBits())
        return;
    T *ptr = arr;
    T ndv = T(img.NoDataValue);
    if (!img.hasNoData) ndv = 0; // It doesn't get called when img doesn't have NoDataValue
    if (1 == stride) {
        for (int row = 0; row < h; row++)
            for (int col = 0; col < w; col++, ptr++)
                if (!bitMask.IsValid(row, col))
                    *ptr = ndv;
    }
    else {
        for (int row = 0; row < h; row++)
            for (int col = 0; col < w; col++, ptr += stride)
                if (!bitMask.IsValid(row, col))
                    for (int c = 0 ; c < stride; c++)
                        ptr[c] = ndv;
    }
    return;
}

// LERC1 splits of at the beginning, so this is mostly LERC2
CPLErr LERC_Band::Decompress(buf_mgr &dst, buf_mgr &src)
{
    const GDAL_LercNS::Byte *ptr = reinterpret_cast<GDAL_LercNS::Byte *>(src.buffer);
    Lerc2::HeaderInfo hdInfo;
    Lerc2 lerc2;

    // If not Lerc2 switch to Lerc
    if (!lerc2.GetHeaderInfo(ptr, src.size, hdInfo))
        return DecompressLERC1(dst, src, img);

    // It is Lerc2 test that it looks reasonable
    if (static_cast<size_t>(hdInfo.blobSize) > src.size) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Lerc2 object too large");
        return CE_Failure;
    }

    if (img.pagesize.x != hdInfo.nCols
        || img.pagesize.y != hdInfo.nRows
        || img.dt != GetL2DataType(hdInfo.dt)
        || img.pagesize.c != hdInfo.nDim
        || dst.size < static_cast<size_t>(hdInfo.nCols * hdInfo.nRows * hdInfo.nDim * GDALGetDataTypeSizeBytes(img.dt))) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Lerc2 format error");
        return CE_Failure;
    }

    bool success = false;
    // we need to add the padding bytes so that out-of-buffer-access checksum
    // don't false-positively trigger.  Is this still needed with Lerc2?
    size_t nRemainingBytes = src.size + PADDING_BYTES;
    BitMask bitMask(img.pagesize.x, img.pagesize.y);
    switch (img.dt) {
#define DECODE(T) success = lerc2.Decode(&ptr, nRemainingBytes, reinterpret_cast<T *>(dst.buffer), bitMask.Bits())
    case GDT_Byte:      DECODE(GByte);      break;
    case GDT_UInt16:    DECODE(GUInt16);    break;
    case GDT_Int16:     DECODE(GInt16);     break;
    case GDT_Int32:     DECODE(GInt32);     break;
    case GDT_UInt32:    DECODE(GUInt32);    break;
    case GDT_Float32:   DECODE(float);      break;
    case GDT_Float64:   DECODE(double);     break;
    default:            break;
#undef DECODE
    }
    if (!success) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during LERC2 decompression");
        return CE_Failure;
    }

    if (!img.hasNoData || bitMask.CountValidBits() == img.pagesize.x * img.pagesize.y)
        return CE_None;

    // Fill in no data values
    switch (img.dt) {
#define UNMASK(T) UnMask(bitMask, reinterpret_cast<T *>(dst.buffer), img)
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
        return CompressLERC2(dst, src, img, precision);
    else
        return CompressLERC1(dst, src, img, precision);
}

CPLXMLNode *LERC_Band::GetMRFConfig(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess != GA_ReadOnly
        || poOpenInfo->pszFilename == nullptr
        || poOpenInfo->pabyHeader == nullptr
        || strlen(poOpenInfo->pszFilename) < 1)
        // Header of Lerc2 takes 58 bytes, an empty area 62 or more, depending on the subversion.
        // Size of Lerc1 empty file is 67
        // || poOpenInfo->nHeaderBytes < static_cast<int>(Lerc2::ComputeNumBytesHeader()))
        return nullptr;

    // Check the header too
    char *psz = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    CPLString sHeader;
    sHeader.assign(psz, psz + poOpenInfo->nHeaderBytes);
    if (!IsLerc(sHeader))
        return nullptr;

    GDALDataType dt = GDT_Unknown; // Use this as a validity flag

    // Use this structure to fetch width and height
    ILSize size(-1, -1, 1, 1, 1);

    // Try lerc2
    {
        Lerc2 l2;
        Lerc2::HeaderInfo hinfo;
        hinfo.RawInit();
        if (l2.GetHeaderInfo(reinterpret_cast<GDAL_LercNS::Byte *>(psz), poOpenInfo->nHeaderBytes, hinfo)) {
            size.x = hinfo.nCols;
            size.y = hinfo.nRows;
            if (hinfo.version >= 4) // subversion 4 introduces bands
                size.c = hinfo.nDim;
            // Set the datatype, which marks it as valid
            dt = GetL2DataType(hinfo.dt);
        }
    }

    // Try Lerc1
    if (size.x <= 0 && sHeader.size() >= Lerc1Image::computeNumBytesNeededToWriteVoidImage()) {
        if (Lerc1Image::getwh(reinterpret_cast<Lerc1NS::Byte*>(psz), poOpenInfo->nHeaderBytes,
            size.x, size.y)) {
            // Get the desired type, if set by caller
            dt = GDALGetDataTypeByName(
                CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "DATATYPE", "Byte"));
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
