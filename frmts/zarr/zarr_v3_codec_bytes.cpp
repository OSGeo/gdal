/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "bytes" codec
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/bytes/index.html

/************************************************************************/
/*                          ZarrV3CodecBytes()                          */
/************************************************************************/

ZarrV3CodecBytes::ZarrV3CodecBytes() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                          GetConfiguration()                          */
/************************************************************************/

/* static */ CPLJSONObject ZarrV3CodecBytes::GetConfiguration(bool bLittle)
{
    CPLJSONObject oConfig;
    oConfig.Add("endian", bLittle ? "little" : "big");
    return oConfig;
}

/************************************************************************/
/*              ZarrV3CodecBytes::InitFromConfiguration()               */
/************************************************************************/

bool ZarrV3CodecBytes::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_oConfiguration = configuration.Clone();
    m_bLittle = true;
    m_oInputArrayMetadata = oInputArrayMetadata;
    oOutputArrayMetadata = oInputArrayMetadata;

    if (configuration.IsValid())
    {
        if (configuration.GetType() != CPLJSONObject::Type::Object)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec endian: configuration is not an object");
            return false;
        }

        for (const auto &oChild : configuration.GetChildren())
        {
            if (oChild.GetName() != "endian")
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec endian: configuration contains a unhandled "
                         "member: %s",
                         oChild.GetName().c_str());
                return false;
            }
        }

        const auto oEndian = configuration.GetObj("endian");
        if (oEndian.IsValid())
        {
            if (oEndian.GetType() != CPLJSONObject::Type::String)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec gzip: endian is not a string");
                return false;
            }
            if (oEndian.ToString() == "little")
                m_bLittle = true;
            else if (oEndian.ToString() == "big")
                m_bLittle = false;
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec gzip: invalid value for endian");
                return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                      ZarrV3CodecBytes::Clone()                       */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecBytes::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecBytes>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}

/************************************************************************/
/*                      ZarrV3CodecBytes::Encode()                      */
/************************************************************************/

bool ZarrV3CodecBytes::Encode(const ZarrByteVectorQuickResize &abySrc,
                              ZarrByteVectorQuickResize &abyDst) const
{
    CPLAssert(!IsNoOp());

    size_t nEltCount = MultiplyElements(m_oInputArrayMetadata.anBlockSizes);
    size_t nNativeSize = m_oInputArrayMetadata.oElt.nativeSize;
    if (abySrc.size() < nEltCount * nNativeSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ZarrV3CodecBytes::Encode(): input buffer too small");
        return false;
    }
    CPLAssert(abySrc.size() >= nEltCount * nNativeSize);
    abyDst.resize(nEltCount * nNativeSize);

    const GByte *pabySrc = abySrc.data();
    GByte *pabyDst = abyDst.data();

    if (m_oInputArrayMetadata.oElt.nativeType ==
        DtypeElt::NativeType::COMPLEX_IEEEFP)
    {
        nEltCount *= 2;
        nNativeSize /= 2;
    }
    if (nNativeSize == 2)
    {
        for (size_t i = 0; i < nEltCount; ++i)
        {
            const uint16_t val = CPL_SWAP16(*reinterpret_cast<const uint16_t *>(
                pabySrc + sizeof(uint16_t) * i));
            memcpy(pabyDst + sizeof(uint16_t) * i, &val, sizeof(val));
        }
    }
    else if (nNativeSize == 4)
    {
        for (size_t i = 0; i < nEltCount; ++i)
        {
            const uint32_t val = CPL_SWAP32(*reinterpret_cast<const uint32_t *>(
                pabySrc + sizeof(uint32_t) * i));
            memcpy(pabyDst + sizeof(uint32_t) * i, &val, sizeof(val));
        }
    }
    else if (nNativeSize == 8)
    {
        for (size_t i = 0; i < nEltCount; ++i)
        {
            const uint64_t val = CPL_SWAP64(*reinterpret_cast<const uint64_t *>(
                pabySrc + sizeof(uint64_t) * i));
            memcpy(pabyDst + sizeof(uint64_t) * i, &val, sizeof(val));
        }
    }
    else
    {
        CPLAssert(false);
    }
    return true;
}

/************************************************************************/
/*                      ZarrV3CodecBytes::Decode()                      */
/************************************************************************/

bool ZarrV3CodecBytes::Decode(const ZarrByteVectorQuickResize &abySrc,
                              ZarrByteVectorQuickResize &abyDst) const
{
    return Encode(abySrc, abyDst);
}
