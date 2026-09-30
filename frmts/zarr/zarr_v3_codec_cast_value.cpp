/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "cast_value" codec
 * Author:   Emmanuel Mathot
 *
 ******************************************************************************
 * Copyright (c) 2026, Emmanuel Mathot
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

// Implements https://github.com/zarr-developers/zarr-extensions/tree/main/codecs/cast_value
// The per-element logic follows the reference implementation in
// https://github.com/zarr-developers/cast-value.rs/blob/main/core/src/lib.rs

using Rounding = ZarrV3CodecCastValue::Rounding;
using OutOfRange = ZarrV3CodecCastValue::OutOfRange;

/************************************************************************/
/*                       ZarrV3CodecCastValue()                         */
/************************************************************************/

ZarrV3CodecCastValue::ZarrV3CodecCastValue() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                           RoundWithMode()                            */
/************************************************************************/

static double RoundWithMode(double dfVal, Rounding eRounding)
{
    switch (eRounding)
    {
        case Rounding::NEAREST_EVEN:
            // Ties go to the even value
            if (std::fabs(dfVal - std::trunc(dfVal)) == 0.5)
                return 2.0 * std::round(dfVal / 2.0);
            return std::round(dfVal);
        case Rounding::TOWARDS_ZERO:
            return std::trunc(dfVal);
        case Rounding::TOWARDS_POSITIVE:
            return std::ceil(dfVal);
        case Rounding::TOWARDS_NEGATIVE:
            return std::floor(dfVal);
        case Rounding::NEAREST_AWAY:
            return std::round(dfVal);
    }
    return dfVal;
}

/************************************************************************/
/*                          AdjustRounding()                            */
/************************************************************************/

// Adjust the nearest-even result of a cast to a floating-point type,
// for the other rounding modes.
template <class Dst>
static Dst AdjustRounding(double dfVal, Dst result, Rounding eRounding)
{
    constexpr Dst INF = std::numeric_limits<Dst>::infinity();
    const double dfResult = static_cast<double>(result);
    if (dfVal == dfResult)
        return result;
    switch (eRounding)
    {
        case Rounding::NEAREST_EVEN:
            break;
        case Rounding::TOWARDS_ZERO:
            if (std::fabs(dfResult) > std::fabs(dfVal))
                return std::nextafter(result, dfVal >= 0 ? -INF : INF);
            break;
        case Rounding::TOWARDS_POSITIVE:
            if (dfResult < dfVal)
                return std::nextafter(result, INF);
            break;
        case Rounding::TOWARDS_NEGATIVE:
            if (dfResult > dfVal)
                return std::nextafter(result, -INF);
            break;
        case Rounding::NEAREST_AWAY:
        {
            const Dst candidate =
                std::nextafter(result, dfVal > dfResult ? INF : -INF);
            const double dfMid =
                (dfResult + static_cast<double>(candidate)) / 2;
            if (dfMid == dfVal && std::fabs(candidate) > std::fabs(result))
                return candidate;
            break;
        }
    }
    return result;
}

/************************************************************************/
/*                              CastElt()                               */
/************************************************************************/

// Cast one value, without the scalar map. Return false if the value cannot
// be cast.
template <class Src, class Dst>
static bool CastElt(Src val, Dst &out, Rounding eRounding,
                    OutOfRange eOutOfRange)
{
    if constexpr (std::is_floating_point_v<Dst>)
    {
        // float -> float, or int -> float
        Dst result;
        if constexpr (std::is_floating_point_v<Src> &&
                      sizeof(Src) > sizeof(Dst))
        {
            // A cast to a value outside of the range of Dst is undefined
            // behavior. With nearest-even rounding, values at or above
            // max + ulp(max) / 2 round to infinity.
            constexpr double MAX =
                static_cast<double>(std::numeric_limits<Dst>::max());
            constexpr double HALF_ULP =
                MAX / ((1ULL << std::numeric_limits<Dst>::digits) - 1) / 2;
            constexpr Dst INF = std::numeric_limits<Dst>::infinity();
            if (std::isfinite(val) && std::fabs(val) >= MAX + HALF_ULP)
                result = val > 0 ? INF : -INF;
            else
                result = static_cast<Dst>(val);
        }
        else
        {
            result = static_cast<Dst>(val);
        }

        bool bIsNaN = false;
        if constexpr (std::is_floating_point_v<Src>)
            bIsNaN = std::isnan(val);
        if (!bIsNaN && eRounding != Rounding::NEAREST_EVEN)
            result =
                AdjustRounding(static_cast<double>(val), result, eRounding);

        if constexpr (std::is_floating_point_v<Src>)
        {
            // Overflow: finite source became infinite result
            if (std::isfinite(val) && std::isinf(result) &&
                eOutOfRange != OutOfRange::CLAMP)
                return false;
        }
        out = result;
        return true;
    }
    else if constexpr (std::is_floating_point_v<Src>)
    {
        // float -> int
        if (std::isnan(val))
            return false;
        const double dfVal = RoundWithMode(static_cast<double>(val), eRounding);
        constexpr double LO =
            static_cast<double>(std::numeric_limits<Dst>::lowest());
        // Exclusive upper bound: 2^digits
        constexpr double HI_EXCL =
            static_cast<double>(std::numeric_limits<Dst>::max()) + 1.0;
        if (dfVal >= LO && dfVal < HI_EXCL)
        {
            out = static_cast<Dst>(dfVal);
            return true;
        }
        switch (eOutOfRange)
        {
            case OutOfRange::CLAMP:
                out = dfVal < LO ? std::numeric_limits<Dst>::lowest()
                                 : std::numeric_limits<Dst>::max();
                return true;
            case OutOfRange::WRAP:
            {
                if (std::isinf(dfVal))
                    return false;
                // Modulo 2^64, then truncation to the size of Dst
                uint64_t nVal;
                if (std::fabs(dfVal) < 0x1p63)
                {
                    nVal = static_cast<uint64_t>(static_cast<int64_t>(dfVal));
                }
                else
                {
                    double dfMod = std::fmod(dfVal, 0x1p64);
                    if (dfMod < 0)
                        dfMod += 0x1p64;
                    nVal = static_cast<uint64_t>(dfMod);
                }
                out = static_cast<Dst>(nVal);
                return true;
            }
            case OutOfRange::NONE:
                break;
        }
        return false;
    }
    else
    {
        // int -> int
        bool bNegative = false;
        if constexpr (std::is_signed_v<Src>)
            bNegative = val < 0;
        bool bBelow = false;
        bool bAbove = false;
        if (bNegative)
            bBelow =
                !std::is_signed_v<Dst> ||
                static_cast<int64_t>(val) <
                    static_cast<int64_t>(std::numeric_limits<Dst>::lowest());
        else
            bAbove = static_cast<uint64_t>(val) >
                     static_cast<uint64_t>(std::numeric_limits<Dst>::max());
        if (bBelow || bAbove)
        {
            if (eOutOfRange == OutOfRange::NONE)
                return false;
            if (eOutOfRange == OutOfRange::CLAMP)
            {
                out = bBelow ? std::numeric_limits<Dst>::lowest()
                             : std::numeric_limits<Dst>::max();
                return true;
            }
        }
        // In range, or wrap: truncation to the size of Dst
        out = static_cast<Dst>(val);
        return true;
    }
}

/************************************************************************/
/*                              IsSame()                                */
/************************************************************************/

template <class T> static bool IsSame(T a, T b)
{
    if constexpr (std::is_floating_point_v<T>)
        return a == b || (std::isnan(a) && std::isnan(b));
    else
        return a == b;
}

/************************************************************************/
/*                              RunTyped()                              */
/************************************************************************/

template <class Src, class Dst>
static bool RunTyped(const ZarrV3CodecCastValue::Cast &oCast,
                     const GByte *pabySrc, GByte *pabyDst, size_t nCount)
{
    const size_t nMapSize = oCast.abyMapKeys.size() / sizeof(Src);
    std::vector<Src> aKeys(nMapSize);
    std::vector<Dst> aValues(nMapSize);
    if (nMapSize)
    {
        memcpy(aKeys.data(), oCast.abyMapKeys.data(), nMapSize * sizeof(Src));
        memcpy(aValues.data(), oCast.abyMapValues.data(),
               nMapSize * sizeof(Dst));
    }

    const Src *src = reinterpret_cast<const Src *>(pabySrc);
    Dst *dst = reinterpret_cast<Dst *>(pabyDst);
    for (size_t i = 0; i < nCount; ++i)
    {
        const Src val = src[i];
        // The scalar map is evaluated first. The first matching key wins.
        size_t j = 0;
        while (j < nMapSize && !IsSame(aKeys[j], val))
            ++j;
        if (j < nMapSize)
        {
            dst[i] = aValues[j];
        }
        else if (!CastElt(val, dst[i], oCast.eRounding, oCast.eOutOfRange))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec %s: value %.17g cannot be cast from %s to %s",
                     ZarrV3CodecCastValue::NAME, static_cast<double>(val),
                     GDALGetDataTypeName(oCast.eSrcDT),
                     GDALGetDataTypeName(oCast.eDstDT));
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                           DispatchType()                             */
/************************************************************************/

// Call f with a value of the C++ type that matches eDT.
template <class F> static bool DispatchType(GDALDataType eDT, F &&f)
{
    switch (eDT)
    {
        case GDT_Int8:
            return f(int8_t{});
        case GDT_UInt8:
            return f(uint8_t{});
        case GDT_Int16:
            return f(int16_t{});
        case GDT_UInt16:
            return f(uint16_t{});
        case GDT_Int32:
            return f(int32_t{});
        case GDT_UInt32:
            return f(uint32_t{});
        case GDT_Int64:
            return f(int64_t{});
        case GDT_UInt64:
            return f(uint64_t{});
        case GDT_Float32:
            return f(float{});
        case GDT_Float64:
            return f(double{});
        default:
            break;
    }
    CPLAssert(false);
    return false;
}

/************************************************************************/
/*                  ZarrV3CodecCastValue::Cast::Run()                   */
/************************************************************************/

bool ZarrV3CodecCastValue::Cast::Run(const GByte *pabySrc, GByte *pabyDst,
                                     size_t nCount) const
{
    return DispatchType(
        eSrcDT,
        [&](auto src)
        {
            return DispatchType(
                eDstDT,
                [&](auto dst)
                {
                    return RunTyped<decltype(src), decltype(dst)>(
                        *this, pabySrc, pabyDst, nCount);
                });
        });
}

/************************************************************************/
/*                          IsSupportedType()                           */
/************************************************************************/

// Same data types as the reference implementation
static bool IsSupportedType(const DtypeElt &oElt)
{
    return oElt.nativeType == DtypeElt::NativeType::SIGNED_INT ||
           oElt.nativeType == DtypeElt::NativeType::UNSIGNED_INT ||
           (oElt.nativeType == DtypeElt::NativeType::IEEEFP &&
            oElt.nativeSize >= 4);
}

/************************************************************************/
/*            ZarrV3CodecCastValue::InitFromConfiguration()             */
/************************************************************************/

bool ZarrV3CodecCastValue::InitFromConfiguration(
    const std::string & /* osArrayName */, const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    oOutputArrayMetadata = oInputArrayMetadata;

    if (configuration.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec %s: configuration missing or not an object", NAME);
        return false;
    }

    for (const auto &oChild : configuration.GetChildren())
    {
        const auto osName = oChild.GetName();
        if (osName != "data_type" && osName != "rounding" &&
            osName != "out_of_range" && osName != "scalar_map")
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec %s: configuration contains a unhandled member: %s",
                     NAME, osName.c_str());
            return false;
        }
    }

    const auto &oInElt = oInputArrayMetadata.oElt;
    std::vector<DtypeElt> aoElts;
    const auto oDataType = configuration["data_type"];
    if (!oDataType.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Codec %s: data_type missing",
                 NAME);
        return false;
    }
    const auto oType = ParseDtypeV3(oDataType, aoElts);
    if (aoElts.empty() || !IsSupportedType(aoElts.back()) ||
        !IsSupportedType(oInElt))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Codec %s: unsupported data type", NAME);
        return false;
    }
    auto oOutElt = aoElts.back();
    // Byte swapping is done by the "bytes" codec
    oOutElt.needByteSwapping = false;

    m_oEncode = Cast();
    m_oEncode.eSrcDT = oInElt.gdalType.GetNumericDataType();
    m_oEncode.eDstDT = oType.GetNumericDataType();

    const auto osRounding = configuration.GetString("rounding", "nearest-even");
    if (osRounding == "nearest-even")
        m_oEncode.eRounding = Rounding::NEAREST_EVEN;
    else if (osRounding == "towards-zero")
        m_oEncode.eRounding = Rounding::TOWARDS_ZERO;
    else if (osRounding == "towards-positive")
        m_oEncode.eRounding = Rounding::TOWARDS_POSITIVE;
    else if (osRounding == "towards-negative")
        m_oEncode.eRounding = Rounding::TOWARDS_NEGATIVE;
    else if (osRounding == "nearest-away")
        m_oEncode.eRounding = Rounding::NEAREST_AWAY;
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec %s: invalid value for rounding: %s", NAME,
                 osRounding.c_str());
        return false;
    }

    const auto oOutOfRange = configuration["out_of_range"];
    if (oOutOfRange.IsValid())
    {
        const auto osOutOfRange = oOutOfRange.ToString();
        if (osOutOfRange == "clamp")
            m_oEncode.eOutOfRange = OutOfRange::CLAMP;
        else if (osOutOfRange == "wrap" &&
                 oOutElt.nativeType != DtypeElt::NativeType::IEEEFP)
            m_oEncode.eOutOfRange = OutOfRange::WRAP;
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec %s: invalid value for out_of_range: %s", NAME,
                     osOutOfRange.c_str());
            return false;
        }
    }

    // Same rounding and out of range rules in both directions
    m_oDecode = m_oEncode;
    std::swap(m_oDecode.eSrcDT, m_oDecode.eDstDT);

    const auto oScalarMap = configuration["scalar_map"];
    if (oScalarMap.IsValid())
    {
        if (oScalarMap.GetType() != CPLJSONObject::Type::Object)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec %s: scalar_map is not an object", NAME);
            return false;
        }
        for (const auto &oChild : oScalarMap.GetChildren())
        {
            const auto osName = oChild.GetName();
            const bool bEncode = osName == "encode";
            if ((!bEncode && osName != "decode") ||
                oChild.GetType() != CPLJSONObject::Type::Array)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec %s: invalid scalar_map member: %s", NAME,
                         osName.c_str());
                return false;
            }
            Cast &oCast = bEncode ? m_oEncode : m_oDecode;
            const DtypeElt &oKeyElt = bEncode ? oInElt : oOutElt;
            const DtypeElt &oValueElt = bEncode ? oOutElt : oInElt;
            for (const auto &oEntry : oChild.ToArray())
            {
                const auto oPair = oEntry.ToArray();
                std::vector<GByte> abyKey, abyValue;
                if (oEntry.GetType() != CPLJSONObject::Type::Array ||
                    oPair.Size() != 2 ||
                    !ParseScalar(oPair[0], oKeyElt, abyKey) ||
                    !ParseScalar(oPair[1], oValueElt, abyValue))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Codec %s: invalid scalar_map.%s entry: %s", NAME,
                             osName.c_str(),
                             oEntry.Format(CPLJSONObject::PrettyFormat::Plain)
                                 .c_str());
                    return false;
                }
                oCast.abyMapKeys.insert(oCast.abyMapKeys.end(), abyKey.begin(),
                                        abyKey.end());
                oCast.abyMapValues.insert(oCast.abyMapValues.end(),
                                          abyValue.begin(), abyValue.end());
            }
        }
    }

    oOutputArrayMetadata.oElt = oOutElt;

    // Propagate the fill value through the codec
    if (!oInputArrayMetadata.abyNoData.empty())
    {
        oOutputArrayMetadata.abyNoData.resize(oOutElt.nativeSize);
        if (!m_oEncode.Run(oInputArrayMetadata.abyNoData.data(),
                           oOutputArrayMetadata.abyNoData.data(), 1))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec %s: fill value cannot be cast", NAME);
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                    ZarrV3CodecCastValue::Clone()                     */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecCastValue::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecCastValue>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(std::string(), m_oConfiguration,
                                   m_oInputArrayMetadata, oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}

/************************************************************************/
/*                    ZarrV3CodecCastValue::Apply()                     */
/************************************************************************/

bool ZarrV3CodecCastValue::Apply(const Cast &oCast,
                                 const ZarrByteVectorQuickResize &abySrc,
                                 ZarrByteVectorQuickResize &abyDst) const
{
    const size_t nSrcSize = GDALGetDataTypeSizeBytes(oCast.eSrcDT);
    const size_t nDstSize = GDALGetDataTypeSizeBytes(oCast.eDstDT);
    if ((abySrc.size() % nSrcSize) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec %s: input buffer size is not a multiple of the data "
                 "type size",
                 NAME);
        return false;
    }
    const size_t nCount = abySrc.size() / nSrcSize;
    try
    {
        abyDst.resize(nCount * nDstSize);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }
    return oCast.Run(abySrc.data(), abyDst.data(), nCount);
}

/************************************************************************/
/*                    ZarrV3CodecCastValue::Encode()                    */
/************************************************************************/

bool ZarrV3CodecCastValue::Encode(const ZarrByteVectorQuickResize &abySrc,
                                  ZarrByteVectorQuickResize &abyDst) const
{
    return Apply(m_oEncode, abySrc, abyDst);
}

/************************************************************************/
/*                    ZarrV3CodecCastValue::Decode()                    */
/************************************************************************/

bool ZarrV3CodecCastValue::Decode(const ZarrByteVectorQuickResize &abySrc,
                                  ZarrByteVectorQuickResize &abyDst) const
{
    return Apply(m_oDecode, abySrc, abyDst);
}

/************************************************************************/
/*                ZarrV3CodecCastValue::DecodePartial()                 */
/************************************************************************/

bool ZarrV3CodecCastValue::DecodePartial(
    VSIVirtualHandle * /* poFile */, const ZarrByteVectorQuickResize &abySrc,
    ZarrByteVectorQuickResize &abyDst, std::vector<size_t> & /* anStartIdx */,
    std::vector<size_t> & /* anCount */)
{
    // Element-wise codec: the shape does not matter
    return Decode(abySrc, abyDst);
}
