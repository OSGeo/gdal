// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

// Canonical URL: https://github.com/libertiff/libertiff/blob/master/libertiff.hpp

#ifndef LIBERTIFF_HPP_INCLUDED
#define LIBERTIFF_HPP_INCLUDED

//////////////////////////////////////////////////////////////
// libertiff = libre TIFF or LIB E(ven) R(ouault) TIFF... ? //
//////////////////////////////////////////////////////////////

#if __cplusplus >= 202002L
#include <bit>  // std::endian
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#ifndef LIBERTIFF_NS
#define LIBERTIFF_NS libertiff
#endif

/** Libertiff is a C++11 simple, header-only, TIFF reader. It is MIT licensed.
 *
 * Handles both ClassicTIFF and BigTIFF, little-endian or big-endian ordered.
 *
 * The library does not (yet?) offer codec facilities. It is mostly aimed at
 * browsing through the linked chain of Image File Directory (IFD) and their tags.
 *
 * "Offline" tag values are not loaded at IFD opening time, but only upon
 * request, which helps handling files with tags with an arbitrarily large
 * number of values.
 *
 * The library is thread-safe (that is the instances that it returns can
 * be used from multiple threads), if passed FileReader instances are themselves
 * thread-safe.
 *
 * The library does not throw exceptions (but underlying std library might
 * throw exceptions in case of out-of-memory situations)
 *
 * Optional features:
 * - define LIBERTIFF_C_FILE_READER before including libertiff.hpp, so that
 *   the libertiff::CFileReader class is available
 */
namespace LIBERTIFF_NS
{

#if __cplusplus >= 201703L
#define LIBERTIFF_STATIC_ASSERT(x) static_assert(x)
#define LIBERTIFF_CONSTEXPR constexpr
#else
#define LIBERTIFF_STATIC_ASSERT(x) static_assert((x), #x)
#define LIBERTIFF_CONSTEXPR
#endif

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&...args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

/** Returns whether the host is little-endian ordered */
inline bool isHostLittleEndian()
{
#if __cplusplus >= 202002L
    return std::endian::native == std::endian::little;
#elif (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) &&          \
       (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) ||                         \
    defined(_MSC_VER)
    return true;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) &&              \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return false;
#else
    uint32_t one = 1;
    char one_as_char_array[sizeof(uint32_t)];
    std::memcpy(one_as_char_array, &one, sizeof(uint32_t));
    return one_as_char_array[0] == 1;
#endif
}

/** Byte-swap */
template <class T> inline T byteSwap(T v);

/** Byte-swap a uint8_t */
template <> inline uint8_t byteSwap(uint8_t v)
{
    return v;
}

/** Byte-swap a int8_t */
template <> inline int8_t byteSwap(int8_t v)
{
    return v;
}

/** Byte-swap a uint16_t */
template <> inline uint16_t byteSwap(uint16_t v)
{
    return uint16_t((v >> 8) | ((v & 0xff) << 8));
}

/** Byte-swap a int16_t */
template <> inline int16_t byteSwap(int16_t v)
{
    uint16_t u;
    LIBERTIFF_STATIC_ASSERT(sizeof(v) == sizeof(u));
    std::memcpy(&u, &v, sizeof(u));
    u = byteSwap(u);
    std::memcpy(&v, &u, sizeof(u));
    return v;
}

/** Byte-swap a uint32_t */
template <> inline uint32_t byteSwap(uint32_t v)
{
    return (v >> 24) | (((v >> 16) & 0xff) << 8) | (((v >> 8) & 0xff) << 16) |
           ((v & 0xff) << 24);
}

/** Byte-swap a int32_t */
template <> inline int32_t byteSwap(int32_t v)
{
    uint32_t u;
    LIBERTIFF_STATIC_ASSERT(sizeof(v) == sizeof(u));
    std::memcpy(&u, &v, sizeof(u));
    u = byteSwap(u);
    std::memcpy(&v, &u, sizeof(u));
    return v;
}

/** Byte-swap a uint64_t */
template <> inline uint64_t byteSwap(uint64_t v)
{
    return (uint64_t(byteSwap(uint32_t(v & ~(0U)))) << 32) |
           byteSwap(uint32_t(v >> 32));
}

/** Byte-swap a int64_t */
template <> inline int64_t byteSwap(int64_t v)
{
    uint64_t u;
    std::memcpy(&u, &v, sizeof(u));
    u = byteSwap(u);
    std::memcpy(&v, &u, sizeof(u));
    return v;
}

/** Byte-swap a float */
template <> inline float byteSwap(float v)
{
    uint32_t u;
    LIBERTIFF_STATIC_ASSERT(sizeof(v) == sizeof(u));
    std::memcpy(&u, &v, sizeof(u));
    u = byteSwap(u);
    std::memcpy(&v, &u, sizeof(u));
    return v;
}

/** Byte-swap a double */
template <> inline double byteSwap(double v)
{
    uint64_t u;
    LIBERTIFF_STATIC_ASSERT(sizeof(v) == sizeof(u));
    std::memcpy(&u, &v, sizeof(u));
    u = byteSwap(u);
    std::memcpy(&v, &u, sizeof(u));
    return v;
}
}  // namespace LIBERTIFF_NS

namespace LIBERTIFF_NS
{
/** Interface to read from a file. */
class FileReader
{
  public:
    virtual ~FileReader() = default;

    /** Return file size in bytes */
    virtual uint64_t size() const = 0;

    /** Read 'count' bytes from offset 'offset' into 'buffer' and
     * return the number of bytes actually read.
     */
    virtual size_t read(uint64_t offset, size_t count, void *buffer) const = 0;
};
}  // namespace LIBERTIFF_NS

namespace LIBERTIFF_NS
{
/** Read context: associates a file, and the byte ordering of the TIFF file */
class ReadContext
{
  public:
    /** Constructor */
    ReadContext(const std::shared_ptr<const FileReader> &file,
                bool mustByteSwap)
        : m_file(file), m_mustByteSwap(mustByteSwap)
    {
    }

    /** Return if values of more than 1-byte must be byte swapped.
     * To be only taken into account when reading pixels. Tag values are
     * automatically byte-swapped */
    inline bool mustByteSwap() const
    {
        return m_mustByteSwap;
    }

    /** Return file size */
    inline uint64_t size() const
    {
        return m_file->size();
    }

    /** Read count raw bytes at offset into buffer */
    void read(uint64_t offset, size_t count, void *buffer, bool &ok) const
    {
        if (m_file->read(offset, count, buffer) != count)
            ok = false;
    }

    /** Read single value at offset */
    template <class T> T read(uint64_t offset, bool &ok) const
    {
#if __cplusplus >= 201703L
        static_assert(
            std::is_same_v<T, uint8_t> || std::is_same_v<T, int8_t> ||
            std::is_same_v<T, uint16_t> || std::is_same_v<T, int16_t> ||
            std::is_same_v<T, uint32_t> || std::is_same_v<T, int32_t> ||
            std::is_same_v<T, uint64_t> || std::is_same_v<T, int64_t> ||
            std::is_same_v<T, float> || std::is_same_v<T, double>);
#endif

        T res = 0;
        if (m_file->read(offset, sizeof(res), &res) != sizeof(res))
        {
            ok = false;
            return 0;
        }
        if LIBERTIFF_CONSTEXPR (sizeof(T) > 1)
        {
            if (m_mustByteSwap)
                res = byteSwap(res);
        }
        return res;
    }

    /** Read a unsigned rational (type == Type::Rational) */
    template <class T = uint32_t>
    double readRational(uint64_t offset, bool &ok) const
    {
        const auto numerator = read<T>(offset, ok);
        const auto denominator = read<T>(offset + sizeof(T), ok);
        if (denominator == 0)
        {
            ok = false;
            return std::numeric_limits<double>::quiet_NaN();
        }
        return double(numerator) / denominator;
    }

    /** Read a signed rational (type == Type::SRational) */
    double readSignedRational(uint64_t offset, bool &ok) const
    {
        return readRational<int32_t>(offset, ok);
    }

    /** Read length bytes at offset (typically for ASCII tag) as a string */
    std::string readString(std::string &res, uint64_t offset, size_t length,
                           bool &ok) const
    {
        res.resize(length);
        if (length > 0 && m_file->read(offset, length, &res[0]) != length)
        {
            ok = false;
            res.clear();
            return res;
        }
        // Strip trailing nul byte if found
        if (length > 0 && res.back() == 0)
            res.pop_back();
        return res;
    }

    /** Read length bytes at offset (typically for ASCII tag) as a string */
    std::string readString(uint64_t offset, size_t length, bool &ok) const
    {
        std::string res;
        readString(res, offset, length, ok);
        return res;
    }

    /** Read an array of count values starting at offset */
    template <class T>
    void readArray(std::vector<T> &array, uint64_t offset, size_t count,
                   bool &ok) const
    {
#if __cplusplus >= 201703L
        static_assert(
            std::is_same_v<T, uint8_t> || std::is_same_v<T, int8_t> ||
            std::is_same_v<T, uint16_t> || std::is_same_v<T, int16_t> ||
            std::is_same_v<T, uint32_t> || std::is_same_v<T, int32_t> ||
            std::is_same_v<T, uint64_t> || std::is_same_v<T, int64_t> ||
            std::is_same_v<T, float> || std::is_same_v<T, double>);
#endif

        array.resize(count);
        const size_t countBytes = count * sizeof(T);
        if (count > 0 &&
            m_file->read(offset, countBytes, &array[0]) != countBytes)
        {
            ok = false;
            array.clear();
        }
        else if LIBERTIFF_CONSTEXPR (sizeof(T) > 1)
        {
            if (m_mustByteSwap)
            {
                if LIBERTIFF_CONSTEXPR (std::is_same<T, float>::value)
                {
                    uint32_t *uint32Array =
                        reinterpret_cast<uint32_t *>(array.data());
                    for (size_t i = 0; i < count; ++i)
                    {
                        uint32Array[i] = byteSwap(uint32Array[i]);
                    }
                }
                else if LIBERTIFF_CONSTEXPR (std::is_same<T, double>::value)
                {
                    uint64_t *uint64Array =
                        reinterpret_cast<uint64_t *>(array.data());
                    for (size_t i = 0; i < count; ++i)
                    {
                        uint64Array[i] = byteSwap(uint64Array[i]);
                    }
                }
                else
                {
                    for (size_t i = 0; i < count; ++i)
                    {
                        array[i] = byteSwap(array[i]);
                    }
                }
            }
        }
    }

    /** Read an array of count values starting at offset */
    template <class T>
    std::vector<T> readArray(uint64_t offset, size_t count, bool &ok) const
    {
        std::vector<T> array;
        readArray(array, offset, count, ok);
        return array;
    }

  private:
    const std::shared_ptr<const FileReader> m_file;
    const bool m_mustByteSwap;
};
}  // namespace LIBERTIFF_NS

namespace LIBERTIFF_NS
{
/** Type of a TIFF tag code */
typedef uint16_t TagCodeType;

/** TIFF tag codes */
namespace TagCode
{
constexpr TagCodeType SubFileType = 254;
constexpr TagCodeType OldSubFileType = 255;

// Base line and extended TIFF tags
constexpr TagCodeType ImageWidth = 256;
constexpr TagCodeType ImageLength = 257;
constexpr TagCodeType BitsPerSample = 258;
constexpr TagCodeType Compression = 259;
constexpr TagCodeType PhotometricInterpretation = 262;
constexpr TagCodeType DocumentName = 269;
constexpr TagCodeType ImageDescription = 270;
constexpr TagCodeType StripOffsets = 273;
constexpr TagCodeType SamplesPerPixel = 277;
constexpr TagCodeType RowsPerStrip = 278;
constexpr TagCodeType StripByteCounts = 279;
constexpr TagCodeType PlanarConfiguration = 284;
constexpr TagCodeType Software = 305;
constexpr TagCodeType DateTime = 306;
constexpr TagCodeType Predictor = 317;
constexpr TagCodeType ColorMap = 320;
constexpr TagCodeType TileWidth = 322;
constexpr TagCodeType TileLength = 323;
constexpr TagCodeType TileOffsets = 324;
constexpr TagCodeType TileByteCounts = 325;
constexpr TagCodeType ExtraSamples = 338;
constexpr TagCodeType SampleFormat = 339;
constexpr TagCodeType JPEGTables = 347;

constexpr TagCodeType Copyright = 33432;

// GeoTIFF tags
constexpr TagCodeType GeoTIFFPixelScale = 33550;
constexpr TagCodeType GeoTIFFTiePoints = 33922;
constexpr TagCodeType GeoTIFFGeoTransMatrix = 34264;
constexpr TagCodeType GeoTIFFGeoKeyDirectory = 34735;
constexpr TagCodeType GeoTIFFDoubleParams = 34736;
constexpr TagCodeType GeoTIFFAsciiParams = 34737;

// GDAL tags
constexpr TagCodeType GDAL_METADATA = 42112;
constexpr TagCodeType GDAL_NODATA = 42113;

// GeoTIFF related
constexpr TagCodeType RPCCoefficients = 50844;

// LERC compression related
constexpr TagCodeType LERCParameters =
    50674; /* Stores LERC version and additional compression method */

}  // namespace TagCode

/** Binary or'ed value of SubFileType flags */
namespace SubFileTypeFlags
{
constexpr uint32_t ReducedImage = 0x1; /* reduced resolution version */
constexpr uint32_t Page = 0x2;         /* one page of many */
constexpr uint32_t Mask = 0x4;         /* transparency mask */
}  // namespace SubFileTypeFlags

#define LIBERTIFF_CASE_TAGCODE_STR(x)                                          \
    case TagCode::x:                                                           \
        return #x

inline const char *tagCodeName(TagCodeType tagCode)
{
    switch (tagCode)
    {
        LIBERTIFF_CASE_TAGCODE_STR(SubFileType);
        LIBERTIFF_CASE_TAGCODE_STR(OldSubFileType);
        LIBERTIFF_CASE_TAGCODE_STR(ImageWidth);
        LIBERTIFF_CASE_TAGCODE_STR(ImageLength);
        LIBERTIFF_CASE_TAGCODE_STR(BitsPerSample);
        LIBERTIFF_CASE_TAGCODE_STR(Compression);
        LIBERTIFF_CASE_TAGCODE_STR(PhotometricInterpretation);
        LIBERTIFF_CASE_TAGCODE_STR(DocumentName);
        LIBERTIFF_CASE_TAGCODE_STR(ImageDescription);
        LIBERTIFF_CASE_TAGCODE_STR(StripOffsets);
        LIBERTIFF_CASE_TAGCODE_STR(SamplesPerPixel);
        LIBERTIFF_CASE_TAGCODE_STR(RowsPerStrip);
        LIBERTIFF_CASE_TAGCODE_STR(StripByteCounts);
        LIBERTIFF_CASE_TAGCODE_STR(PlanarConfiguration);
        LIBERTIFF_CASE_TAGCODE_STR(Software);
        LIBERTIFF_CASE_TAGCODE_STR(DateTime);
        LIBERTIFF_CASE_TAGCODE_STR(Predictor);
        LIBERTIFF_CASE_TAGCODE_STR(ColorMap);
        LIBERTIFF_CASE_TAGCODE_STR(TileWidth);
        LIBERTIFF_CASE_TAGCODE_STR(TileLength);
        LIBERTIFF_CASE_TAGCODE_STR(TileOffsets);
        LIBERTIFF_CASE_TAGCODE_STR(TileByteCounts);
        LIBERTIFF_CASE_TAGCODE_STR(ExtraSamples);
        LIBERTIFF_CASE_TAGCODE_STR(SampleFormat);
        LIBERTIFF_CASE_TAGCODE_STR(Copyright);
        LIBERTIFF_CASE_TAGCODE_STR(JPEGTables);
        LIBERTIFF_CASE_TAGCODE_STR(GeoTIFFPixelScale);
        LIBERTIFF_CASE_TAGCODE_STR(GeoTIFFTiePoints);
        LIBERTIFF_CASE_TAGCODE_STR(GeoTIFFGeoTransMatrix);
        LIBERTIFF_CASE_TAGCODE_STR(GeoTIFFGeoKeyDirectory);
        LIBERTIFF_CASE_TAGCODE_STR(GeoTIFFDoubleParams);
        LIBERTIFF_CASE_TAGCODE_STR(GeoTIFFAsciiParams);
        LIBERTIFF_CASE_TAGCODE_STR(GDAL_METADATA);
        LIBERTIFF_CASE_TAGCODE_STR(GDAL_NODATA);
        LIBERTIFF_CASE_TAGCODE_STR(RPCCoefficients);
        LIBERTIFF_CASE_TAGCODE_STR(LERCParameters);
        default:
            break;
    }
    return "(unknown)";
}

#undef LIBERTIFF_CASE_TAGCODE_STR

/** Type of a TIFF tag type */
typedef uint16_t TagTypeType;

/** TIFF tag data types */
namespace TagType
{
constexpr TagTypeType Byte = 1;  /*! Unsigned 8-bit integer */
constexpr TagTypeType ASCII = 2; /*! Character */
constexpr TagTypeType Short = 3; /*! Unsigned 16-bit integer */
constexpr TagTypeType Long = 4;  /*! Unsigned 32-bit integer */
constexpr TagTypeType Rational =
    5; /*! Positive number as a ratio of two unsigned 32-bit integers */
constexpr TagTypeType SByte = 6;     /*! Signed 8-bit integer */
constexpr TagTypeType Undefined = 7; /*! Untyped 8-bit data */
constexpr TagTypeType SShort = 8;    /*! Signed 16-bit integer */
constexpr TagTypeType SLong = 9;     /*! Signed 32-bit integer */
constexpr TagTypeType SRational =
    10; /*! Signed number as a ratio of two signed 32-bit integers */
constexpr TagTypeType Float = 11;  /*! 32-bit IEEE-754 floating point number */
constexpr TagTypeType Double = 12; /*! 64-bit IEEE-754 floating point number */

// BigTIFF additions
constexpr TagTypeType Long8 = 16;  /*! Unsigned 64-bit integer */
constexpr TagTypeType SLong8 = 17; /*! Signed 64-bit integer */
constexpr TagTypeType IFD8 = 18;   /*! Unsigned 64-bit IFD offset */
}  // namespace TagType

#define LIBERTIFF_CASE_TAGTYPE_STR(x)                                          \
    case TagType::x:                                                           \
        return #x

inline const char *tagTypeName(TagTypeType tagType)
{
    switch (tagType)
    {
        LIBERTIFF_CASE_TAGTYPE_STR(Byte);
        LIBERTIFF_CASE_TAGTYPE_STR(ASCII);
        LIBERTIFF_CASE_TAGTYPE_STR(Short);
        LIBERTIFF_CASE_TAGTYPE_STR(Long);
        LIBERTIFF_CASE_TAGTYPE_STR(Rational);
        LIBERTIFF_CASE_TAGTYPE_STR(SByte);
        LIBERTIFF_CASE_TAGTYPE_STR(Undefined);
        LIBERTIFF_CASE_TAGTYPE_STR(SShort);
        LIBERTIFF_CASE_TAGTYPE_STR(SLong);
        LIBERTIFF_CASE_TAGTYPE_STR(SRational);
        LIBERTIFF_CASE_TAGTYPE_STR(Float);
        LIBERTIFF_CASE_TAGTYPE_STR(Double);
        LIBERTIFF_CASE_TAGTYPE_STR(Long8);
        LIBERTIFF_CASE_TAGTYPE_STR(SLong8);
        LIBERTIFF_CASE_TAGTYPE_STR(IFD8);
        default:
            break;
    }
    return "(unknown)";
}

#undef LIBERTIFF_CASE_TAGTYPE_STR

/** Type of a PlanarConfiguration value */
typedef uint32_t PlanarConfigurationType;

/** Values of the PlanarConfiguration tag */
namespace PlanarConfiguration
{
constexpr PlanarConfigurationType Contiguous = 1; /*! Single image plane */
constexpr PlanarConfigurationType Separate =
    2; /*! Separate planes per sample */
}  // namespace PlanarConfiguration

#define LIBERTIFF_CASE_PLANAR_CONFIG_STR(x)                                    \
    case PlanarConfiguration::x:                                               \
        return #x

inline const char *
planarConfigurationName(PlanarConfigurationType planarConfiguration)
{
    switch (planarConfiguration)
    {
        LIBERTIFF_CASE_PLANAR_CONFIG_STR(Contiguous);
        LIBERTIFF_CASE_PLANAR_CONFIG_STR(Separate);
        default:
            break;
    }
    return "(unknown)";
}

#undef LIBERTIFF_CASE_PLANAR_CONFIG_STR

/** Type of a PlanarConfiguration value */
typedef uint32_t PhotometricInterpretationType;

/** Values of the PhotometricInterpretation tag */
namespace PhotometricInterpretation
{
constexpr PhotometricInterpretationType MinIsWhite = 0;
constexpr PhotometricInterpretationType MinIsBlack = 1;
constexpr PhotometricInterpretationType RGB = 2;
constexpr PhotometricInterpretationType Palette = 3;
constexpr PhotometricInterpretationType Mask = 4;
constexpr PhotometricInterpretationType Separated = 5;
constexpr PhotometricInterpretationType YCbCr = 6;
constexpr PhotometricInterpretationType CIELab = 8;
constexpr PhotometricInterpretationType ICCLab = 9;
constexpr PhotometricInterpretationType ITULab = 10;
}  // namespace PhotometricInterpretation

#define LIBERTIFF_CASE_PHOTOMETRIC_STR(x)                                      \
    case PhotometricInterpretation::x:                                         \
        return #x

inline const char *photometricInterpretationName(
    PhotometricInterpretationType photometricInterpretation)
{
    switch (photometricInterpretation)
    {
        LIBERTIFF_CASE_PHOTOMETRIC_STR(MinIsWhite);
        LIBERTIFF_CASE_PHOTOMETRIC_STR(MinIsBlack);
        LIBERTIFF_CASE_PHOTOMETRIC_STR(RGB);
        LIBERTIFF_CASE_PHOTOMETRIC_STR(Palette);
        LIBERTIFF_CASE_PHOTOMETRIC_STR(Mask);
        LIBERTIFF_CASE_PHOTOMETRIC_STR(Separated);
        LIBERTIFF_CASE_PHOTOMETRIC_STR(YCbCr);
        LIBERTIFF_CASE_PHOTOMETRIC_STR(CIELab);
        LIBERTIFF_CASE_PHOTOMETRIC_STR(ICCLab);
        LIBERTIFF_CASE_PHOTOMETRIC_STR(ITULab);
        default:
            break;
    }
    return "(unknown)";
}

#undef LIBERTIFF_CASE_PHOTOMETRIC_STR

/** Type of a Compression value */
typedef uint32_t CompressionType;

/** Compression methods */
namespace Compression
{
constexpr CompressionType None = 1;
constexpr CompressionType CCITT_RLE = 2;
constexpr CompressionType CCITT_FAX3 = 3;
constexpr CompressionType CCITT_FAX4 = 4;
constexpr CompressionType LZW = 5;
constexpr CompressionType OldJPEG = 6;
constexpr CompressionType JPEG = 7;
constexpr CompressionType Deflate =
    8; /* Deflate compression, as recognized by Adobe */
constexpr CompressionType PackBits = 32773;
constexpr CompressionType LegacyDeflate =
    32946;                              /* Deflate compression, legacy tag */
constexpr CompressionType JBIG = 34661; /* ISO JBIG */
constexpr CompressionType LERC =
    34887; /* ESRI Lerc codec: https://github.com/Esri/lerc */
constexpr CompressionType LZMA = 34925; /* LZMA2 */
constexpr CompressionType ZSTD =
    50000; /* ZSTD: WARNING not registered in Adobe-maintained registry */
constexpr CompressionType WEBP =
    50001; /* WEBP: WARNING not registered in Adobe-maintained registry */
constexpr CompressionType JXL =
    50002; /* JPEGXL: WARNING not registered in Adobe-maintained registry */
constexpr CompressionType JXL_DNG_1_7 =
    52546; /* JPEGXL from DNG 1.7 specification */
}  // namespace Compression

#define LIBERTIFF_CASE_COMPRESSION_STR(x)                                      \
    case Compression::x:                                                       \
        return #x

inline const char *compressionName(CompressionType compression)
{
    switch (compression)
    {
        LIBERTIFF_CASE_COMPRESSION_STR(None);
        LIBERTIFF_CASE_COMPRESSION_STR(CCITT_RLE);
        LIBERTIFF_CASE_COMPRESSION_STR(CCITT_FAX3);
        LIBERTIFF_CASE_COMPRESSION_STR(CCITT_FAX4);
        LIBERTIFF_CASE_COMPRESSION_STR(LZW);
        LIBERTIFF_CASE_COMPRESSION_STR(OldJPEG);
        LIBERTIFF_CASE_COMPRESSION_STR(JPEG);
        LIBERTIFF_CASE_COMPRESSION_STR(Deflate);
        LIBERTIFF_CASE_COMPRESSION_STR(PackBits);
        LIBERTIFF_CASE_COMPRESSION_STR(LegacyDeflate);
        LIBERTIFF_CASE_COMPRESSION_STR(JBIG);
        LIBERTIFF_CASE_COMPRESSION_STR(LERC);
        LIBERTIFF_CASE_COMPRESSION_STR(LZMA);
        LIBERTIFF_CASE_COMPRESSION_STR(ZSTD);
        LIBERTIFF_CASE_COMPRESSION_STR(WEBP);
        LIBERTIFF_CASE_COMPRESSION_STR(JXL);
        LIBERTIFF_CASE_COMPRESSION_STR(JXL_DNG_1_7);
        default:
            break;
    }
    return "(unknown)";
}

#undef LIBERTIFF_CASE_COMPRESSION_STR

/** Type of a SampleFormat value */
typedef uint32_t SampleFormatType;

/** Sample format */
namespace SampleFormat
{
constexpr SampleFormatType UnsignedInt = 1;
constexpr SampleFormatType SignedInt = 2;
constexpr SampleFormatType IEEEFP = 3;
constexpr SampleFormatType Void = 4;
constexpr SampleFormatType ComplexInt = 5;
constexpr SampleFormatType ComplexIEEEFP = 6;
}  // namespace SampleFormat

#define LIBERTIFF_CASE_SAMPLE_FORMAT_STR(x)                                    \
    case SampleFormat::x:                                                      \
        return #x

inline const char *sampleFormatName(SampleFormatType sampleFormat)
{
    switch (sampleFormat)
    {
        LIBERTIFF_CASE_SAMPLE_FORMAT_STR(UnsignedInt);
        LIBERTIFF_CASE_SAMPLE_FORMAT_STR(SignedInt);
        LIBERTIFF_CASE_SAMPLE_FORMAT_STR(IEEEFP);
        LIBERTIFF_CASE_SAMPLE_FORMAT_STR(Void);
        LIBERTIFF_CASE_SAMPLE_FORMAT_STR(ComplexInt);
        LIBERTIFF_CASE_SAMPLE_FORMAT_STR(ComplexIEEEFP);
        default:
            break;
    }
    return "(unknown)";
}

#undef LIBERTIFF_CASE_SAMPLE_FORMAT_STR

/** Type of a ExtraSamples value */
typedef uint32_t ExtraSamplesType;

/** Values of the ExtraSamples tag */
namespace ExtraSamples
{
constexpr ExtraSamplesType Unspecified = 0;
constexpr ExtraSamplesType AssociatedAlpha = 1;   /* premultiplied */
constexpr ExtraSamplesType UnAssociatedAlpha = 2; /* unpremultiplied */
}  // namespace ExtraSamples

/** Content of a tag entry in a Image File Directory (IFD) */
struct TagEntry
{
    TagCodeType tag = 0;
    TagTypeType type = 0;
    uint64_t count = 0;  // number of values in the tag

    // Inline values. Only valid if value_offset == 0.
    // The actual number in the arrays is count
    union
    {
        std::array<char, 8> charValues;
        std::array<uint8_t, 8> uint8Values;
        std::array<int8_t, 8> int8Values;
        std::array<uint16_t, 4> uint16Values;
        std::array<int16_t, 4> int16Values;
        std::array<uint32_t, 2> uint32Values;
        std::array<int32_t, 2> int32Values;
        std::array<float, 2> float32Values;
        std::array<double, 1>
            float64Values;  // Valid for Double, Rational, SRational
        std::array<uint64_t, 1> uint64Values = {0};
        std::array<int64_t, 1> int64Values;
    };

    uint64_t value_offset = 0;         // 0 for inline values
    bool invalid_value_offset = true;  // whether value_offset is invalid
};

// clang-format off

/** Return the size in bytes of a tag data type, or 0 if unknown */
inline uint32_t tagTypeSize(TagTypeType type)
{
    switch (type)
    {
        case TagType::Byte:      return 1;
        case TagType::ASCII:     return 1;
        case TagType::Short:     return 2;
        case TagType::Long:      return 4;
        case TagType::Rational:  return 8;  // 2 Long
        case TagType::SByte:     return 1;
        case TagType::Undefined: return 1;
        case TagType::SShort:    return 2;
        case TagType::SLong:     return 4;
        case TagType::SRational: return 8;  // 2 SLong
        case TagType::Float:     return 4;
        case TagType::Double:    return 8;
        case TagType::Long8:     return 8;
        case TagType::SLong8:    return 8;
        case TagType::IFD8:      return 8;
        default: break;
    }
    return 0;
}

// clang-format on

namespace detail
{
template <class T>
inline std::vector<T> readTagAsVectorInternal(const ReadContext &rc,
                                              const TagEntry &tag,
                                              TagTypeType expectedType,
                                              const T *inlineValues, bool &ok)
{
    if (tag.type == expectedType)
    {
        if (tag.value_offset)
        {
            if (!tag.invalid_value_offset)
            {
                if LIBERTIFF_CONSTEXPR (sizeof(tag.count) > sizeof(size_t))
                {
                    if (tag.count > std::numeric_limits<size_t>::max())
                    {
                        ok = false;
                        return {};
                    }
                }
                return rc.readArray<T>(tag.value_offset,
                                       static_cast<size_t>(tag.count), ok);
            }
        }
        else
        {
            return std::vector<T>(
                inlineValues, inlineValues + static_cast<size_t>(tag.count));
        }
    }
    ok = false;
    return {};
}

template <class T>
inline std::vector<T> readTagAsVector(const ReadContext &rc,
                                      const TagEntry &tag, bool &ok);

template <>
inline std::vector<int8_t> readTagAsVector(const ReadContext &rc,
                                           const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(rc, tag, TagType::SByte,
                                   tag.int8Values.data(), ok);
}

template <>
inline std::vector<uint8_t> readTagAsVector(const ReadContext &rc,
                                            const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(
        rc, tag, tag.type == TagType::Undefined ? tag.type : TagType::Byte,
        tag.uint8Values.data(), ok);
}

template <>
inline std::vector<int16_t> readTagAsVector(const ReadContext &rc,
                                            const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(rc, tag, TagType::SShort,
                                   tag.int16Values.data(), ok);
}

template <>
inline std::vector<uint16_t> readTagAsVector(const ReadContext &rc,
                                             const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(rc, tag, TagType::Short,
                                   tag.uint16Values.data(), ok);
}

template <>
inline std::vector<int32_t> readTagAsVector(const ReadContext &rc,
                                            const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(rc, tag, TagType::SLong,
                                   tag.int32Values.data(), ok);
}

template <>
inline std::vector<uint32_t> readTagAsVector(const ReadContext &rc,
                                             const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(rc, tag, TagType::Long,
                                   tag.uint32Values.data(), ok);
}

template <>
inline std::vector<int64_t> readTagAsVector(const ReadContext &rc,
                                            const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(rc, tag, TagType::SLong8,
                                   tag.int64Values.data(), ok);
}

template <>
inline std::vector<uint64_t> readTagAsVector(const ReadContext &rc,
                                             const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(rc, tag, TagType::Long8,
                                   tag.uint64Values.data(), ok);
}

template <>
inline std::vector<float> readTagAsVector(const ReadContext &rc,
                                          const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(rc, tag, TagType::Float,
                                   tag.float32Values.data(), ok);
}

template <>
inline std::vector<double> readTagAsVector(const ReadContext &rc,
                                           const TagEntry &tag, bool &ok)
{
    return readTagAsVectorInternal(rc, tag, TagType::Double,
                                   tag.float64Values.data(), ok);
}

}  // namespace detail

/** Represents a TIFF Image File Directory (IFD). */
class Image
{
  public:
    /** Constructor. Should not be called directly. Use the open() method */
    Image(const std::shared_ptr<const ReadContext> &rc, bool isBigTIFF)
        : m_rc(rc), m_isBigTIFF(isBigTIFF)
    {
    }

    /** Return read context */
    const std::shared_ptr<const ReadContext> &readContext() const
    {
        return m_rc;
    }

    /** Return whether the file is BigTIFF (if false, classic TIFF) */
    inline bool isBigTIFF() const
    {
        return m_isBigTIFF;
    }

    /** Return if values of more than 1-byte must be byte swapped.
     * To be only taken into account when reading pixels. Tag values are
     * automatically byte-swapped */
    inline bool mustByteSwap() const
    {
        return m_rc->mustByteSwap();
    }

    /** Return the offset of the this IFD */
    inline uint64_t offset() const
    {
        return m_offset;
    }

    /** Return the offset of the next IFD (to pass to Image::open()),
         * or 0 if there is no more */
    inline uint64_t nextImageOffset() const
    {
        return m_nextImageOffset;
    }

    /** Return value of SubFileType tag */
    inline uint32_t subFileType() const
    {
        return m_subFileType;
    }

    /** Return width of the image in pixels */
    inline uint32_t width() const
    {
        return m_width;
    }

    /** Return height of the image in pixels */
    inline uint32_t height() const
    {
        return m_height;
    }

    /** Return number of bits per sample */
    inline uint32_t bitsPerSample() const
    {
        return m_bitsPerSample;
    }

    /** Return number of samples (a.k.a. channels, bands) per pixel */
    inline uint32_t samplesPerPixel() const
    {
        return m_samplesPerPixel;
    }

    /** Return planar configuration */
    inline PlanarConfigurationType planarConfiguration() const
    {
        return m_planarConfiguration;
    }

    /** Return planar configuration */
    inline PhotometricInterpretationType photometricInterpretation() const
    {
        return m_photometricInterpretation;
    }

    /** Return compression method used */
    inline CompressionType compression() const
    {
        return m_compression;
    }

    /** Return predictor value (used for Deflate, LZW, ZStd, etc. compression) */
    inline uint32_t predictor() const
    {
        return m_predictor;
    }

    /** Return sample format */
    inline SampleFormatType sampleFormat() const
    {
        return m_sampleFormat;
    }

    /** Return the number of rows per strip */
    inline uint32_t rowsPerStrip() const
    {
        return m_rowsPerStrip;
    }

    /** Return the sanitized number of rows per strip */
    inline uint32_t rowsPerStripSanitized() const
    {
        return std::min(m_rowsPerStrip, m_height);
    }

    /** Return the number of strips/tiles.
     * Return 0 if inconsistent values between ByteCounts and Offsets arrays. */
    inline uint64_t strileCount() const
    {
        return m_strileCount;
    }

    /** Return whether image is tiled */
    inline bool isTiled() const
    {
        return m_isTiled;
    }

    /** Return tile width */
    inline uint32_t tileWidth() const
    {
        return m_tileWidth;
    }

    /** Return tile width */
    inline uint32_t tileHeight() const
    {
        return m_tileHeight;
    }

    /** Return number of tiles per row */
    uint32_t tilesPerRow() const
    {
        if (m_tileWidth > 0)
        {
            return uint32_t((uint64_t(m_width) + m_tileWidth - 1) /
                            m_tileWidth);
        }
        return 0;
    }

    /** Return number of tiles per column */
    uint32_t tilesPerCol() const
    {
        if (m_tileHeight > 0)
        {
            return uint32_t((uint64_t(m_height) + m_tileHeight - 1) /
                            m_tileHeight);
        }
        return 0;
    }

    /** Convert a tile coordinate (xtile, ytile, bandIdx) to a flat index */
    uint64_t tileCoordinateToIdx(uint32_t xtile, uint32_t ytile,
                                 uint32_t bandIdx, bool &ok) const
    {
        if (m_isTiled && m_tileWidth > 0 && m_tileHeight > 0)
        {
            const uint32_t lTilesPerRow = tilesPerRow();
            const uint32_t lTilesPerCol = tilesPerCol();
            if (xtile >= lTilesPerRow || ytile >= lTilesPerCol)
            {
                ok = false;
                return 0;
            }
            uint64_t idx = uint64_t(ytile) * lTilesPerRow + xtile;
            if (bandIdx &&
                m_planarConfiguration == PlanarConfiguration::Separate)
            {
                const uint64_t lTotalTiles =
                    uint64_t(lTilesPerCol) * lTilesPerRow;
                if (lTotalTiles >
                    std::numeric_limits<uint64_t>::max() / bandIdx)
                {
                    ok = false;
                    return 0;
                }
                idx += bandIdx * lTotalTiles;
            }
            return idx;
        }
        ok = false;
        return 0;
    }

    /** Return the offset of strip/tile of index idx */
    uint64_t strileOffset(uint64_t idx, bool &ok) const
    {
        return readUIntTag(m_strileOffsetsTag, idx, ok);
    }

    /** Return the offset of a tile from its coordinates */
    uint64_t tileOffset(uint32_t xtile, uint32_t ytile, uint32_t bandIdx,
                        bool &ok) const
    {
        const auto idx = tileCoordinateToIdx(xtile, ytile, bandIdx, ok);
        return ok ? strileOffset(idx, ok) : 0;
    }

    /** Return the byte count of strip/tile of index idx */
    uint64_t strileByteCount(uint64_t idx, bool &ok) const
    {
        return readUIntTag(m_strileByteCountsTag, idx, ok);
    }

    /** Return the offset of a tile from its coordinates */
    uint64_t tileByteCount(uint32_t xtile, uint32_t ytile, uint32_t bandIdx,
                           bool &ok) const
    {
        const auto idx = tileCoordinateToIdx(xtile, ytile, bandIdx, ok);
        return ok ? strileByteCount(idx, ok) : 0;
    }

    /** Return the list of tags */
    inline const std::vector<TagEntry> &tags() const
    {
        return m_tags;
    }

    /** Return the (first) tag corresponding to a code, or nullptr if not found */
    const TagEntry *tag(TagCodeType tagCode) const
    {
        for (const auto &tag : m_tags)
        {
            if (tag.tag == tagCode)
                return &tag;
        }
        return nullptr;
    }

    /** Read an ASCII tag as a string */
    std::string readTagAsString(const TagEntry &tag, bool &ok) const
    {
        if (tag.type == TagType::ASCII)
        {
            if (tag.value_offset)
            {
                if LIBERTIFF_CONSTEXPR (sizeof(tag.count) > sizeof(size_t))
                {
                    // coverity[result_independent_of_operands]
                    if (tag.count > std::numeric_limits<size_t>::max())
                    {
                        ok = false;
                        return std::string();
                    }
                }
                return readContext()->readString(
                    tag.value_offset, static_cast<size_t>(tag.count), ok);
            }
            if (tag.count)
            {
                std::string res(tag.charValues.data(),
                                static_cast<size_t>(tag.count));
                if (res.back() == 0)
                    res.pop_back();
                return res;
            }
        }
        ok = false;
        return std::string();
    }

    /** Read a numeric tag as a vector. You must use a type T which is
     * consistent with the tag.type value. For example, if
     * tag.type == libertiff::TagType::Short, T must be uint16_t.
     * libertiff::TagType::Undefined must be read with T=uint8_t.
     */
    template <class T>
    std::vector<T> readTagAsVector(const TagEntry &tag, bool &ok) const
    {
        return detail::readTagAsVector<T>(*(m_rc.get()), tag, ok);
    }

    /** Returns a new Image instance for the IFD starting at offset imageOffset */
    template <bool isBigTIFF>
    static std::unique_ptr<const Image>
    open(const std::shared_ptr<const ReadContext> &rc,
         const uint64_t imageOffset,
         const std::set<uint64_t> &alreadyVisitedImageOffsets =
             std::set<uint64_t>())
    {
        // To prevent infinite looping on corrupted files
        if (imageOffset == 0 || alreadyVisitedImageOffsets.find(imageOffset) !=
                                    alreadyVisitedImageOffsets.end())
        {
            return nullptr;
        }

        auto image = LIBERTIFF_NS::make_unique<Image>(rc, isBigTIFF);

        image->m_offset = imageOffset;
        image->m_alreadyVisitedImageOffsets = alreadyVisitedImageOffsets;
        image->m_alreadyVisitedImageOffsets.insert(imageOffset);

        bool ok = true;
        int tagCount = 0;
        uint64_t offset = imageOffset;
        if LIBERTIFF_CONSTEXPR (isBigTIFF)
        {
            // To prevent unsigned integer overflows in later additions. The
            // theoretical max should be much closer to UINT64_MAX, but half of
            // it is already more than needed in practice :-)
            if (offset >= std::numeric_limits<uint64_t>::max() / 2)
                return nullptr;

            const auto tagCount64Bit = rc->read<uint64_t>(offset, ok);
            // Artificially limit to the same number of entries as ClassicTIFF
            if (tagCount64Bit > std::numeric_limits<uint16_t>::max())
                return nullptr;
            tagCount = static_cast<int>(tagCount64Bit);
            offset += sizeof(uint64_t);
        }
        else
        {
            tagCount = rc->read<uint16_t>(offset, ok);
            offset += sizeof(uint16_t);
        }
        if (!ok)
            return nullptr;
        image->m_tags.reserve(tagCount);
        // coverity[tainted_data]
        for (int i = 0; i < tagCount; ++i)
        {
            TagEntry entry;

            // Read tag code
            entry.tag = rc->read<uint16_t>(offset, ok);
            offset += sizeof(uint16_t);

            // Read tag data type
            entry.type = rc->read<uint16_t>(offset, ok);
            offset += sizeof(uint16_t);

            // Read number of values
            if LIBERTIFF_CONSTEXPR (isBigTIFF)
            {
                auto count = rc->read<uint64_t>(offset, ok);
                entry.count = count;
                offset += sizeof(count);
            }
            else
            {
                auto count = rc->read<uint32_t>(offset, ok);
                entry.count = count;
                offset += sizeof(count);
            }

            uint32_t singleValue = 0;
            bool singleValueFitsInUInt32 = false;
            if (entry.count)
            {
                if LIBERTIFF_CONSTEXPR (isBigTIFF)
                {
                    image->ParseTagEntryDataOrOffset<uint64_t>(
                        entry, offset, singleValueFitsInUInt32, singleValue,
                        ok);
                }
                else
                {
                    image->ParseTagEntryDataOrOffset<uint32_t>(
                        entry, offset, singleValueFitsInUInt32, singleValue,
                        ok);
                }
            }
            if (!ok)
                return nullptr;

            image->processTag(entry, singleValueFitsInUInt32, singleValue);

            image->m_tags.push_back(entry);
        }

        image->finalTagProcessing();

        if LIBERTIFF_CONSTEXPR (isBigTIFF)
            image->m_nextImageOffset = rc->read<uint64_t>(offset, ok);
        else
            image->m_nextImageOffset = rc->read<uint32_t>(offset, ok);

        image->m_openFunc = open<isBigTIFF>;

        return std::unique_ptr<const Image>(image.release());
    }

    /** Returns a new Image instance at the next IFD, or nullptr if there is none */
    std::unique_ptr<const Image> next() const
    {
        return m_openFunc(m_rc, m_nextImageOffset,
                          m_alreadyVisitedImageOffsets);
    }

  private:
    const std::shared_ptr<const ReadContext> m_rc;
    std::unique_ptr<const Image> (*m_openFunc)(
        const std::shared_ptr<const ReadContext> &, const uint64_t,
        const std::set<uint64_t> &) = nullptr;

    std::set<uint64_t> m_alreadyVisitedImageOffsets{};
    uint64_t m_offset = 0;
    uint64_t m_nextImageOffset = 0;
    uint32_t m_subFileType = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_bitsPerSample = 0;
    uint32_t m_samplesPerPixel = 0;
    uint32_t m_rowsPerStrip = 0;
    CompressionType m_compression = Compression::None;
    SampleFormatType m_sampleFormat = SampleFormat::UnsignedInt;
    PlanarConfigurationType m_planarConfiguration =
        PlanarConfiguration::Contiguous;
    PhotometricInterpretationType m_photometricInterpretation =
        PhotometricInterpretation::MinIsBlack;
    uint32_t m_predictor = 0;

    const bool m_isBigTIFF;
    bool m_isTiled = false;
    uint32_t m_tileWidth = 0;
    uint32_t m_tileHeight = 0;
    uint64_t m_strileCount = 0;

    std::vector<TagEntry> m_tags{};
    const TagEntry *m_strileOffsetsTag = nullptr;
    const TagEntry *m_strileByteCountsTag = nullptr;

    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    /** Process tag */
    void processTag(const TagEntry &entry, bool singleValueFitsInUInt32,
                    uint32_t singleValue)
    {
        if (singleValueFitsInUInt32)
        {
            switch (entry.tag)
            {
                case TagCode::SubFileType:
                    m_subFileType = singleValue;
                    break;

                case TagCode::ImageWidth:
                    m_width = singleValue;
                    break;

                case TagCode::ImageLength:
                    m_height = singleValue;
                    break;

                case TagCode::Compression:
                    m_compression = singleValue;
                    break;

                case TagCode::SamplesPerPixel:
                    m_samplesPerPixel = singleValue;
                    break;

                case TagCode::RowsPerStrip:
                    m_rowsPerStrip = singleValue;
                    break;

                case TagCode::PlanarConfiguration:
                    m_planarConfiguration = singleValue;
                    break;

                case TagCode::PhotometricInterpretation:
                    m_photometricInterpretation = singleValue;
                    break;

                case TagCode::Predictor:
                    m_predictor = singleValue;
                    break;

                case TagCode::TileWidth:
                    m_tileWidth = singleValue;
                    break;

                case TagCode::TileLength:
                    m_tileHeight = singleValue;
                    break;

                default:
                    break;
            }
        }

        if (entry.count &&
            (entry.type == TagType::Byte || entry.type == TagType::Short ||
             entry.type == TagType::Long))
        {
            // Values of those 2 tags are repeated per sample, but should be
            // at the same value.
            if (entry.tag == TagCode::SampleFormat)
            {
                bool localOk = true;
                const auto sampleFormat =
                    static_cast<uint32_t>(readUIntTag(&entry, 0, localOk));
                if (localOk)
                {
                    m_sampleFormat = sampleFormat;
                }
            }
            else if (entry.tag == TagCode::BitsPerSample)
            {
                bool localOk = true;
                const auto bitsPerSample =
                    static_cast<uint32_t>(readUIntTag(&entry, 0, localOk));
                if (localOk)
                {
                    m_bitsPerSample = bitsPerSample;
                }
            }
        }
    }

    /** Final tag processing */
    void finalTagProcessing()
    {
        m_strileOffsetsTag = tag(TagCode::TileOffsets);
        if (m_strileOffsetsTag)
        {
            m_strileByteCountsTag = tag(TagCode::TileByteCounts);
            if (m_strileByteCountsTag &&
                m_strileOffsetsTag->count == m_strileByteCountsTag->count)
            {
                m_isTiled = true;
                m_strileCount = m_strileOffsetsTag->count;
            }
        }
        else
        {
            m_strileOffsetsTag = tag(TagCode::StripOffsets);
            if (m_strileOffsetsTag)
            {
                m_strileByteCountsTag = tag(TagCode::StripByteCounts);
                if (m_strileByteCountsTag &&
                    m_strileOffsetsTag->count == m_strileByteCountsTag->count)
                {
                    m_strileCount = m_strileOffsetsTag->count;
                }
            }
        }
    }

    /** Read a value from a byte/short/long/long8 array tag */
    uint64_t readUIntTag(const TagEntry *tag, uint64_t idx, bool &ok) const
    {
        if (tag && idx < tag->count)
        {
            if (tag->type == TagType::Byte)
            {
                if (tag->count <= (m_isBigTIFF ? 8 : 4))
                {
                    return tag->uint8Values[size_t(idx)];
                }
                return m_rc->read<uint8_t>(
                    tag->value_offset + sizeof(uint8_t) * idx, ok);
            }
            else if (tag->type == TagType::Short)
            {
                if (tag->count <= (m_isBigTIFF ? 4 : 2))
                {
                    return tag->uint16Values[size_t(idx)];
                }
                return m_rc->read<uint16_t>(
                    tag->value_offset + sizeof(uint16_t) * idx, ok);
            }
            else if (tag->type == TagType::Long)
            {
                if (tag->count <= (m_isBigTIFF ? 2 : 1))
                {
                    return tag->uint32Values[size_t(idx)];
                }
                return m_rc->read<uint32_t>(
                    tag->value_offset + sizeof(uint32_t) * idx, ok);
            }
            else if (m_isBigTIFF && tag->type == TagType::Long8)
            {
                if (tag->count <= 1)
                {
                    return tag->uint64Values[size_t(idx)];
                }
                return m_rc->read<uint64_t>(
                    tag->value_offset + sizeof(uint64_t) * idx, ok);
            }
        }
        ok = false;
        return 0;
    }

    template <class DataOrOffsetType>
    void ParseTagEntryDataOrOffset(TagEntry &entry, uint64_t &offset,
                                   bool &singleValueFitsInUInt32,
                                   uint32_t &singleValue, bool &ok)
    {
        LIBERTIFF_STATIC_ASSERT(
            (std::is_same<DataOrOffsetType, uint32_t>::value ||
             std::is_same<DataOrOffsetType, uint64_t>::value));
        assert(entry.count > 0);

        const uint32_t dataTypeSize = tagTypeSize(entry.type);
        if (dataTypeSize == 0)
        {
            return;
        }

        // There are 2 cases:
        // - either the number of values for the data type can fit
        //   in the next DataOrOffsetType bytes
        // - or it cannot, and then the next DataOrOffsetType bytes are an offset
        //   to the values
        if (dataTypeSize > sizeof(DataOrOffsetType) / entry.count)
        {
            // Out-of-line values. We read a file offset
            entry.value_offset = m_rc->read<DataOrOffsetType>(offset, ok);
            if (entry.value_offset == 0)
            {
                // value_offset = 0 for a out-of-line tag is obviously
                // wrong and would cause later confusion in readTagAsVector<>,
                // so better reject the file.
                ok = false;
                return;
            }
            if (dataTypeSize >
                std::numeric_limits<uint64_t>::max() / entry.count)
            {
                entry.invalid_value_offset = true;
            }
            else
            {
                const uint64_t byteCount = uint64_t(dataTypeSize) * entry.count;

                // Size of tag data beyond which we check the tag position and size
                // w.r.t the file size.
                constexpr uint32_t THRESHOLD_CHECK_FILE_SIZE = 10 * 1000 * 1000;

                entry.invalid_value_offset =
                    (byteCount > THRESHOLD_CHECK_FILE_SIZE &&
                     (m_rc->size() < byteCount ||
                      entry.value_offset > m_rc->size() - byteCount));
            }
        }
        else if (dataTypeSize == sizeof(uint8_t))
        {
            // Read up to 4 (classic) or 8 (BigTIFF) inline bytes
            m_rc->read(offset, size_t(entry.count), &entry.uint8Values[0], ok);
            if (entry.count == 1 && entry.type == TagType::Byte)
            {
                singleValueFitsInUInt32 = true;
                singleValue = entry.uint8Values[0];
            }
        }
        else if (dataTypeSize == sizeof(uint16_t))
        {
            // Read up to 2 (classic) or 4 (BigTIFF) inline 16-bit values
            for (uint32_t idx = 0; idx < entry.count; ++idx)
            {
                entry.uint16Values[idx] =
                    m_rc->read<uint16_t>(offset + idx * sizeof(uint16_t), ok);
            }
            if (entry.count == 1 && entry.type == TagType::Short)
            {
                singleValueFitsInUInt32 = true;
                singleValue = entry.uint16Values[0];
            }
        }
        else if (dataTypeSize == sizeof(uint32_t))
        {
            // Read up to 1 (classic) or 2 (BigTIFF) inline 32-bit values
            entry.uint32Values[0] = m_rc->read<uint32_t>(offset, ok);
            if (entry.count == 1 && entry.type == TagType::Long)
            {
                singleValueFitsInUInt32 = true;
                singleValue = entry.uint32Values[0];
            }
            if LIBERTIFF_CONSTEXPR (std::is_same<DataOrOffsetType,
                                                 uint64_t>::value)
            {
                if (entry.count == 2)
                {
                    entry.uint32Values[1] =
                        m_rc->read<uint32_t>(offset + sizeof(uint32_t), ok);
                }
            }
        }
        else if LIBERTIFF_CONSTEXPR (std::is_same<DataOrOffsetType,
                                                  uint64_t>::value)
        {
            if (dataTypeSize == sizeof(uint64_t))
            {
                // Read one inline 64-bit value
                if (entry.type == TagType::Rational)
                    entry.float64Values[0] = m_rc->readRational(offset, ok);
                else if (entry.type == TagType::SRational)
                    entry.float64Values[0] =
                        m_rc->readSignedRational(offset, ok);
                else
                    entry.uint64Values[0] = m_rc->read<uint64_t>(offset, ok);
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            // fprintf(stderr, "Unexpected case: tag=%u, dataType=%u, count=%u\n", entry.tag, entry.type, entry.count);
            assert(false);
        }

        offset += sizeof(DataOrOffsetType);
    }
};

/** Open a TIFF file and return its first Image File Directory
 */
template <bool acceptBigTIFF = true>
std::unique_ptr<const Image> open(const std::shared_ptr<const FileReader> &file)
{
    unsigned char signature[2] = {0, 0};
    (void)file->read(0, 2, signature);
    const bool littleEndian = signature[0] == 'I' && signature[1] == 'I';
    const bool bigEndian = signature[0] == 'M' && signature[1] == 'M';
    if (!littleEndian && !bigEndian)
        return nullptr;

    const bool mustByteSwap = littleEndian ^ isHostLittleEndian();

    auto rc = std::make_shared<ReadContext>(file, mustByteSwap);
    bool ok = true;
    const int version = rc->read<uint16_t>(2, ok);
    constexpr int CLASSIC_TIFF_VERSION = 42;
    if (version == CLASSIC_TIFF_VERSION)
    {
        const auto firstImageOffset = rc->read<uint32_t>(4, ok);
        return Image::open<false>(rc, firstImageOffset, {});
    }
    else if LIBERTIFF_CONSTEXPR (acceptBigTIFF)
    {
        constexpr int BIGTIFF_VERSION = 43;
        if (version == BIGTIFF_VERSION)
        {
            const auto byteSizeOfOffsets = rc->read<uint16_t>(4, ok);
            if (byteSizeOfOffsets != 8)
                return nullptr;
            const auto zeroWord = rc->read<uint16_t>(6, ok);
            if (zeroWord != 0 || !ok)
                return nullptr;
            const auto firstImageOffset = rc->read<uint64_t>(8, ok);
            return Image::open<true>(rc, firstImageOffset, {});
        }
    }

    return nullptr;
}
}  // namespace LIBERTIFF_NS

#ifdef LIBERTIFF_C_FILE_READER
#include <cstdio>
#include <mutex>

namespace LIBERTIFF_NS
{
/** Interface to read from a FILE* handle */
class CFileReader final : public FileReader
{
  public:
    explicit CFileReader(FILE *f) : m_f(f)
    {
    }

    ~CFileReader() override
    {
        fclose(m_f);
    }

    uint64_t size() const override
    {
        std::lock_guard<std::mutex> oLock(m_oMutex);
        fseek(m_f, 0, SEEK_END);
        return ftell(m_f);
    }

    size_t read(uint64_t offset, size_t count, void *buffer) const override
    {
        std::lock_guard<std::mutex> oLock(m_oMutex);
        if (fseek(m_f, static_cast<long>(offset), SEEK_SET) != 0)
            return 0;
        return fread(buffer, 1, count, m_f);
    }

  private:
    FILE *const m_f;
    mutable std::mutex m_oMutex{};

    CFileReader(const CFileReader &) = delete;
    CFileReader &operator=(const CFileReader &) = delete;
};
}  // namespace LIBERTIFF_NS
#endif

#endif  // LIBERTIFF_HPP_INCLUDED
