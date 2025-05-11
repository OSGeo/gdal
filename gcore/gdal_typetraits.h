/******************************************************************************
 * Name:     gdal_typetraits.h
 * Project:  GDAL Core
 * Purpose:  Type traits for mapping C++ types to and from GDAL/OGR types.
 * Author:   Robin Princeley, <rprinceley at esri dot com>
 *
 ******************************************************************************
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
#if !defined(GDAL_TYPETRAITS_H_INCLUDED)
#define GDAL_TYPETRAITS_H_INCLUDED

#include "gdal_priv.h"
#include "cpl_float.h"

#include <complex>

namespace gdal
{

/** Trait accepting a C++ type ([u]int[8/16/32/64], float, double,
 * std::complex<float>, std::complex<double> or std::string)
 * and mapping it to GDALDataType / OGRFieldType.
 *
 * Each specialization has the following members:
 * static constexpr GDALDataType gdal_type;
 * static constexpr size_t size;
 * static constexpr OGRFieldType ogr_type;
 * static constexpr OGRFieldSubType ogr_subtype;
 *
 * @since 3.11
 */
template <typename T> struct CXXTypeTraits
{
};

//! @cond Doxygen_Suppress
template <> struct CXXTypeTraits<int8_t>
{
    static constexpr GDALDataType gdal_type = GDT_Int8;
    static constexpr size_t size = sizeof(int8_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Int8);
    }
};

template <> struct CXXTypeTraits<uint8_t>
{
    static constexpr GDALDataType gdal_type = GDT_Byte;
    static constexpr size_t size = sizeof(uint8_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Byte);
    }
};

template <> struct CXXTypeTraits<int16_t>
{
    static constexpr GDALDataType gdal_type = GDT_Int16;
    static constexpr size_t size = sizeof(int16_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTInt16;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Int16);
    }
};

template <> struct CXXTypeTraits<uint16_t>
{
    static constexpr GDALDataType gdal_type = GDT_UInt16;
    static constexpr size_t size = sizeof(uint16_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_UInt16);
    }
};

template <> struct CXXTypeTraits<int32_t>
{
    static constexpr GDALDataType gdal_type = GDT_Int32;
    static constexpr size_t size = sizeof(int32_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Int32);
    }
};

template <> struct CXXTypeTraits<uint32_t>
{
    static constexpr GDALDataType gdal_type = GDT_UInt32;
    static constexpr size_t size = sizeof(uint32_t);
    static constexpr OGRFieldType ogr_type = OFTInteger64;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_UInt32);
    }
};

template <> struct CXXTypeTraits<int64_t>
{
    static constexpr GDALDataType gdal_type = GDT_Int64;
    static constexpr size_t size = sizeof(int64_t);
    static constexpr OGRFieldType ogr_type = OFTInteger64;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Int64);
    }
};

template <> struct CXXTypeTraits<uint64_t>
{
    static constexpr GDALDataType gdal_type = GDT_UInt64;
    static constexpr size_t size = sizeof(uint64_t);
    // Mapping to Real is questionable...
    static constexpr OGRFieldType ogr_type = OFTReal;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_UInt64);
    }
};

template <> struct CXXTypeTraits<GFloat16>
{
    static constexpr GDALDataType gdal_type = GDT_Float16;
    static constexpr size_t size = sizeof(GFloat16);
    static constexpr OGRFieldType ogr_type = OFTReal;
    // We could introduce OFSTFloat16
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Float16);
    }
};

template <> struct CXXTypeTraits<float>
{
    static constexpr GDALDataType gdal_type = GDT_Float32;
    static constexpr size_t size = sizeof(float);
    static constexpr OGRFieldType ogr_type = OFTReal;
    static constexpr OGRFieldSubType ogr_subtype = OFSTFloat32;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Float32);
    }
};

template <> struct CXXTypeTraits<double>
{
    static constexpr GDALDataType gdal_type = GDT_Float64;
    static constexpr size_t size = sizeof(double);
    static constexpr OGRFieldType ogr_type = OFTReal;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Float64);
    }
};

template <> struct CXXTypeTraits<std::complex<GFloat16>>
{
    static constexpr GDALDataType gdal_type = GDT_CFloat16;
    static constexpr size_t size = sizeof(GFloat16) * 2;
    static constexpr OGRFieldType ogr_type = OFTMaxType;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_CFloat16);
    }
};

template <> struct CXXTypeTraits<std::complex<float>>
{
    static constexpr GDALDataType gdal_type = GDT_CFloat32;
    static constexpr size_t size = sizeof(float) * 2;
    static constexpr OGRFieldType ogr_type = OFTMaxType;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_CFloat32);
    }
};

template <> struct CXXTypeTraits<std::complex<double>>
{
    static constexpr GDALDataType gdal_type = GDT_CFloat64;
    static constexpr size_t size = sizeof(double) * 2;
    static constexpr OGRFieldType ogr_type = OFTMaxType;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_CFloat64);
    }
};

template <> struct CXXTypeTraits<std::string>
{
    static constexpr GDALDataType gdal_type = GDT_Unknown;
    static constexpr size_t size = 0;
    static constexpr OGRFieldType ogr_type = OFTString;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::CreateString();
    }
};

//! @endcond

/** Trait accepting a GDALDataType and mapping it to corresponding C++ type and
 * OGRFieldType
 *
 * Each specialization has the following members:
 * typedef T type; (except for GDT_CInt16 and GDT_CInt32)
 * static constexpr size_t size;
 * static constexpr OGRFieldType ogr_type;
 * static constexpr OGRFieldSubType ogr_subtype;
 * static inline GDALExtendedDataType GetExtendedDataType();
 *
 * @since 3.11
 */
template <GDALDataType T> struct GDALDataTypeTraits
{
};

//! @cond Doxygen_Suppress
template <> struct GDALDataTypeTraits<GDT_Int8>
{
    typedef int8_t type;
    static constexpr size_t size = sizeof(int8_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Int8);
    }
};

template <> struct GDALDataTypeTraits<GDT_Byte>
{
    typedef uint8_t type;
    static constexpr size_t size = sizeof(uint8_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Byte);
    }
};

template <> struct GDALDataTypeTraits<GDT_Int16>
{
    typedef int16_t type;
    static constexpr size_t size = sizeof(int16_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTInt16;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Int16);
    }
};

template <> struct GDALDataTypeTraits<GDT_UInt16>
{
    typedef uint16_t type;
    static constexpr size_t size = sizeof(uint16_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_UInt16);
    }
};

template <> struct GDALDataTypeTraits<GDT_Int32>
{
    typedef int32_t type;
    static constexpr size_t size = sizeof(int32_t);
    static constexpr OGRFieldType ogr_type = OFTInteger;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Int32);
    }
};

template <> struct GDALDataTypeTraits<GDT_UInt32>
{
    typedef uint32_t type;
    static constexpr size_t size = sizeof(uint32_t);
    static constexpr OGRFieldType ogr_type = OFTInteger64;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_UInt32);
    }
};

template <> struct GDALDataTypeTraits<GDT_Int64>
{
    typedef int64_t type;
    static constexpr size_t size = sizeof(int64_t);
    static constexpr OGRFieldType ogr_type = OFTInteger64;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Int64);
    }
};

template <> struct GDALDataTypeTraits<GDT_UInt64>
{
    typedef uint64_t type;
    static constexpr size_t size = sizeof(uint64_t);
    // Mapping to Real is questionable...
    static constexpr OGRFieldType ogr_type = OFTReal;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_UInt64);
    }
};

template <> struct GDALDataTypeTraits<GDT_Float32>
{
    typedef float type;
    static constexpr size_t size = sizeof(float);
    static constexpr OGRFieldType ogr_type = OFTReal;
    static constexpr OGRFieldSubType ogr_subtype = OFSTFloat32;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Float32);
    }
};

template <> struct GDALDataTypeTraits<GDT_Float64>
{
    typedef double type;
    static constexpr size_t size = sizeof(double);
    static constexpr OGRFieldType ogr_type = OFTReal;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_Float64);
    }
};

template <> struct GDALDataTypeTraits<GDT_CInt16>
{
    // typedef type not available !
    static constexpr size_t size = sizeof(int16_t) * 2;
    static constexpr OGRFieldType ogr_type = OFTMaxType;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_CInt16);
    }
};

template <> struct GDALDataTypeTraits<GDT_CInt32>
{
    // typedef type not available !
    static constexpr size_t size = sizeof(int32_t) * 2;
    static constexpr OGRFieldType ogr_type = OFTMaxType;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_CInt32);
    }
};

template <> struct GDALDataTypeTraits<GDT_CFloat32>
{
    typedef std::complex<float> type;
    static constexpr size_t size = sizeof(float) * 2;
    static constexpr OGRFieldType ogr_type = OFTMaxType;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_CFloat32);
    }
};

template <> struct GDALDataTypeTraits<GDT_CFloat64>
{
    typedef std::complex<double> type;
    static constexpr size_t size = sizeof(double) * 2;
    static constexpr OGRFieldType ogr_type = OFTMaxType;
    static constexpr OGRFieldSubType ogr_subtype = OFSTNone;

    static inline GDALExtendedDataType GetExtendedDataType()
    {
        return GDALExtendedDataType::Create(GDT_CFloat64);
    }
};

//! @endcond

/** Map a GDALDataType to the most suitable OGRFieldType.
 *
 * Note that GDT_UInt32 is mapped to OFTInteger64 to avoid data losses.
 * GDT_UInt64 is mapped to OFTReal, which can be lossy. If values are
 * guaranteed to be in [0, INT64_MAX] range, callers might want to use
 * OFTInteger64 instead.
 * There is no mapping for complex data types.
 *
 * @since 3.11
 */
inline OGRFieldType GetOGRFieldType(const GDALDataType gdal_type)
{
    switch (gdal_type)
    {
        case GDT_Byte:
        case GDT_Int8:
        case GDT_Int16:
        case GDT_Int32:
        case GDT_UInt16:
            return OFTInteger;
        case GDT_UInt32:
        case GDT_Int64:
            return OFTInteger64;
        case GDT_UInt64:  // Questionable
        case GDT_Float16:
        case GDT_Float32:
        case GDT_Float64:
            return OFTReal;
        case GDT_CInt16:
        case GDT_CInt32:
        case GDT_CFloat16:
        case GDT_CFloat32:
        case GDT_CFloat64:
        case GDT_Unknown:
        case GDT_TypeCount:
            break;
    }
    return OFTMaxType;
}

/** Map a GDALExtendedDataType to the most suitable OGRFieldType.
 *
 * Note that GDT_UInt32 is mapped to OFTInteger64 to avoid data losses.
 * GDT_UInt64 is mapped to OFTReal, which can be lossy. If values are
 * guaranteed to be in [0, INT64_MAX] range, callers might want to use
 * OFTInteger64 instead.
 *
 * @since 3.11
 */
inline OGRFieldType GetOGRFieldType(const GDALExtendedDataType &oEDT)
{
    if (oEDT.GetClass() == GEDTC_NUMERIC)
        return GetOGRFieldType(oEDT.GetNumericDataType());
    else if (oEDT.GetClass() == GEDTC_STRING)
        return OFTString;
    return OFTMaxType;
}

}  // namespace gdal

#endif  // GDAL_TYPETRAITS_H_INCLUDED
