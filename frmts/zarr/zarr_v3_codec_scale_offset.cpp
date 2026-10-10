/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "scale_offset" codec
 * Author:   Emmanuel Mathot
 *
 ******************************************************************************
 * Copyright (c) 2026, Emmanuel Mathot
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include "cpl_float.h"
#include "cpl_safemaths.hpp"

#include <cstring>
#include <limits>
#include <type_traits>

// Implements https://github.com/zarr-developers/zarr-extensions/tree/main/codecs/scale_offset
// Behavior matches zarr-python (src/zarr/codecs/scale_offset.py): arithmetic in
// the data type for floats, and an error for integer results that are out of
// range or not exact.

/************************************************************************/
/*                       ZarrV3CodecScaleOffset()                       */
/************************************************************************/

ZarrV3CodecScaleOffset::ZarrV3CodecScaleOffset() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                          ApplyFloat()                                */
/************************************************************************/

// Arithmetic is done in T, as required by the specification.
template <class T>
static void ApplyFloat(const GByte *pabySrc, GByte *pabyDst, size_t nCount,
                       const GByte *pabyOffset, const GByte *pabyScale,
                       bool bEncode)
{
    T offset, scale;
    memcpy(&offset, pabyOffset, sizeof(T));
    memcpy(&scale, pabyScale, sizeof(T));
    const T *src = reinterpret_cast<const T *>(pabySrc);
    T *dst = reinterpret_cast<T *>(pabyDst);
    if (bEncode)
    {
        for (size_t i = 0; i < nCount; ++i)
            dst[i] = static_cast<T>((src[i] - offset) * scale);
    }
    else
    {
        for (size_t i = 0; i < nCount; ++i)
            dst[i] = static_cast<T>(src[i] / scale + offset);
    }
}

/************************************************************************/
/*                            CheckedSub()                              */
/************************************************************************/

// CPLSafeInt has no subtraction for uint64_t
template <class W> static W CheckedSub(W a, W b)
{
    if constexpr (std::is_signed_v<W>)
        return (CPLSM(a) - CPLSM(b)).v();
    else
    {
        if (a < b)
            throw CPLSafeIntOverflow();
        return a - b;
    }
}

/************************************************************************/
/*                           ApplyInt()                                 */
/************************************************************************/

// Arithmetic is done in (u)int64 with overflow checks. A result that is not
// representable in T, or a division with a remainder, is an error.
template <class T>
static bool ApplyInt(const GByte *pabySrc, GByte *pabyDst, size_t nCount,
                     const GByte *pabyOffset, const GByte *pabyScale,
                     bool bEncode)
{
    using W = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;
    T offsetT, scaleT;
    memcpy(&offsetT, pabyOffset, sizeof(T));
    memcpy(&scaleT, pabyScale, sizeof(T));
    const W offset = offsetT;
    const W scale = scaleT;
    const T *src = reinterpret_cast<const T *>(pabySrc);
    T *dst = reinterpret_cast<T *>(pabyDst);
    try
    {
        for (size_t i = 0; i < nCount; ++i)
        {
            const W v = src[i];
            W r;
            if (bEncode)
            {
                r = (CPLSM(CheckedSub(v, offset)) * CPLSM(scale)).v();
            }
            else
            {
                W q;
                // Avoid INT64_MIN / -1
                if (std::is_signed_v<W> && scale == static_cast<W>(-1))
                    q = CheckedSub(static_cast<W>(0), v);
                else if (v % scale != 0)
                    throw CPLSafeIntOverflow();
                else
                    q = v / scale;
                r = (CPLSM(q) + CPLSM(offset)).v();
            }
            if (r < static_cast<W>(std::numeric_limits<T>::lowest()) ||
                r > static_cast<W>(std::numeric_limits<T>::max()))
            {
                throw CPLSafeIntOverflow();
            }
            dst[i] = static_cast<T>(r);
        }
    }
    catch (const CPLSafeIntOverflow &)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec %s: %s produced a value that is not representable "
                 "in the data type",
                 ZarrV3CodecScaleOffset::NAME,
                 bEncode ? "encoding" : "decoding");
        return false;
    }
    return true;
}

/************************************************************************/
/*                            ApplyTyped()                              */
/************************************************************************/

static bool ApplyTyped(GDALDataType eDT, const GByte *pabySrc, GByte *pabyDst,
                       size_t nCount, const GByte *pabyOffset,
                       const GByte *pabyScale, bool bEncode)
{
    switch (eDT)
    {
        case GDT_Int8:
            return ApplyInt<int8_t>(pabySrc, pabyDst, nCount, pabyOffset,
                                    pabyScale, bEncode);
        case GDT_UInt8:
            return ApplyInt<uint8_t>(pabySrc, pabyDst, nCount, pabyOffset,
                                     pabyScale, bEncode);
        case GDT_Int16:
            return ApplyInt<int16_t>(pabySrc, pabyDst, nCount, pabyOffset,
                                     pabyScale, bEncode);
        case GDT_UInt16:
            return ApplyInt<uint16_t>(pabySrc, pabyDst, nCount, pabyOffset,
                                      pabyScale, bEncode);
        case GDT_Int32:
            return ApplyInt<int32_t>(pabySrc, pabyDst, nCount, pabyOffset,
                                     pabyScale, bEncode);
        case GDT_UInt32:
            return ApplyInt<uint32_t>(pabySrc, pabyDst, nCount, pabyOffset,
                                      pabyScale, bEncode);
        case GDT_Int64:
            return ApplyInt<int64_t>(pabySrc, pabyDst, nCount, pabyOffset,
                                     pabyScale, bEncode);
        case GDT_UInt64:
            return ApplyInt<uint64_t>(pabySrc, pabyDst, nCount, pabyOffset,
                                      pabyScale, bEncode);
        case GDT_Float16:
            ApplyFloat<GFloat16>(pabySrc, pabyDst, nCount, pabyOffset,
                                 pabyScale, bEncode);
            return true;
        case GDT_Float32:
            ApplyFloat<float>(pabySrc, pabyDst, nCount, pabyOffset, pabyScale,
                              bEncode);
            return true;
        case GDT_Float64:
            ApplyFloat<double>(pabySrc, pabyDst, nCount, pabyOffset, pabyScale,
                               bEncode);
            return true;
        default:
            break;
    }
    CPLAssert(false);
    return false;
}

/************************************************************************/
/*           ZarrV3CodecScaleOffset::InitFromConfiguration()            */
/************************************************************************/

bool ZarrV3CodecScaleOffset::InitFromConfiguration(
    const std::string & /* osArrayName */, const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    oOutputArrayMetadata = oInputArrayMetadata;

    const auto &oElt = oInputArrayMetadata.oElt;
    if (oElt.nativeType != DtypeElt::NativeType::SIGNED_INT &&
        oElt.nativeType != DtypeElt::NativeType::UNSIGNED_INT &&
        oElt.nativeType != DtypeElt::NativeType::IEEEFP)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Codec %s: unsupported data type", NAME);
        return false;
    }

    // Default values: additive and multiplicative identity elements
    const GDALDataType eDT = oElt.gdalType.GetNumericDataType();
    const size_t nDTSize = GDALGetDataTypeSizeBytes(eDT);
    m_abyOffset.assign(nDTSize, 0);
    m_abyScale.resize(nDTSize);
    const double dfOne = 1.0;
    GDALCopyWords64(&dfOne, GDT_Float64, 0, m_abyScale.data(), eDT, 0, 1);
    const std::vector<GByte> abyOne = m_abyScale;

    if (configuration.IsValid())
    {
        if (configuration.GetType() != CPLJSONObject::Type::Object)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec %s: configuration is not an object", NAME);
            return false;
        }
        for (const auto &oChild : configuration.GetChildren())
        {
            const auto osName = oChild.GetName();
            std::vector<GByte> *pabyVal = osName == "offset"  ? &m_abyOffset
                                          : osName == "scale" ? &m_abyScale
                                                              : nullptr;
            if (!pabyVal)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec %s: configuration contains a unhandled "
                         "member: %s",
                         NAME, osName.c_str());
                return false;
            }
            if (!ParseScalar(oChild, oElt, *pabyVal))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec %s: invalid value for %s", NAME,
                         osName.c_str());
                return false;
            }
        }
    }

    double dfScale = 0;
    GDALCopyWords64(m_abyScale.data(), eDT, 0, &dfScale, GDT_Float64, 0, 1);
    if (dfScale == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Codec %s: scale must not be 0",
                 NAME);
        return false;
    }

    m_bIsNoOp =
        m_abyOffset == std::vector<GByte>(nDTSize, 0) && m_abyScale == abyOne;

    // Propagate the fill value through the codec
    if (!m_bIsNoOp && !oInputArrayMetadata.abyNoData.empty())
    {
        oOutputArrayMetadata.abyNoData.resize(nDTSize);
        if (!ApplyTyped(eDT, oInputArrayMetadata.abyNoData.data(),
                        oOutputArrayMetadata.abyNoData.data(), 1,
                        m_abyOffset.data(), m_abyScale.data(),
                        /* bEncode = */ true))
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                   ZarrV3CodecScaleOffset::Clone()                    */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecScaleOffset::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecScaleOffset>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(std::string(), m_oConfiguration,
                                   m_oInputArrayMetadata, oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}

/************************************************************************/
/*                   ZarrV3CodecScaleOffset::Apply()                    */
/************************************************************************/

bool ZarrV3CodecScaleOffset::Apply(const ZarrByteVectorQuickResize &abySrc,
                                   ZarrByteVectorQuickResize &abyDst,
                                   bool bEncode) const
{
    const GDALDataType eDT =
        m_oInputArrayMetadata.oElt.gdalType.GetNumericDataType();
    const size_t nDTSize = m_abyScale.size();
    if ((abySrc.size() % nDTSize) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec %s: input buffer size is not a multiple of the data "
                 "type size",
                 NAME);
        return false;
    }
    abyDst.resize(abySrc.size());
    return ApplyTyped(eDT, abySrc.data(), abyDst.data(),
                      abySrc.size() / nDTSize, m_abyOffset.data(),
                      m_abyScale.data(), bEncode);
}

/************************************************************************/
/*                   ZarrV3CodecScaleOffset::Encode()                   */
/************************************************************************/

bool ZarrV3CodecScaleOffset::Encode(const ZarrByteVectorQuickResize &abySrc,
                                    ZarrByteVectorQuickResize &abyDst) const
{
    return Apply(abySrc, abyDst, /* bEncode = */ true);
}

/************************************************************************/
/*                   ZarrV3CodecScaleOffset::Decode()                   */
/************************************************************************/

bool ZarrV3CodecScaleOffset::Decode(const ZarrByteVectorQuickResize &abySrc,
                                    ZarrByteVectorQuickResize &abyDst) const
{
    return Apply(abySrc, abyDst, /* bEncode = */ false);
}

/************************************************************************/
/*               ZarrV3CodecScaleOffset::DecodePartial()                */
/************************************************************************/

bool ZarrV3CodecScaleOffset::DecodePartial(
    VSIVirtualHandle * /* poFile */, const ZarrByteVectorQuickResize &abySrc,
    ZarrByteVectorQuickResize &abyDst, std::vector<size_t> & /* anStartIdx */,
    std::vector<size_t> & /* anCount */)
{
    // Element-wise codec: the shape does not matter
    return Decode(abySrc, abyDst);
}
