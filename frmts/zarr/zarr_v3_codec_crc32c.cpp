/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "crc32c" codec
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Development Seed
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include "crc32c.h"

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/crc32c/index.html

/************************************************************************/
/*                ZarrV3CodecCRC32C::ZarrV3CodecCRC32C()                */
/************************************************************************/

ZarrV3CodecCRC32C::ZarrV3CodecCRC32C() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                      ZarrV3CodecCRC32C::Clone()                      */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecCRC32C::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecCRC32C>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}

/************************************************************************/
/*               ZarrV3CodecCRC32C::InitFromConfiguration()             */
/************************************************************************/

bool ZarrV3CodecCRC32C::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    oOutputArrayMetadata = oInputArrayMetadata;

    // GDAL extension for tests !!!
    if (!m_oConfiguration.GetBool("check_crc", true))
        m_bCheckCRC = false;

    return true;
}

/************************************************************************/
/*                           ComputeCRC32C()                            */
/************************************************************************/

static uint32_t ComputeCRC32C(const GByte *pabyIn, size_t nLength)
{
    crc32c_init();
    return crc32c(0, pabyIn, nLength);
}

/************************************************************************/
/*                     ZarrV3CodecCRC32C::Encode()                      */
/************************************************************************/

bool ZarrV3CodecCRC32C::Encode(const ZarrByteVectorQuickResize &abySrc,
                               ZarrByteVectorQuickResize &abyDst) const
{
    abyDst.clear();
    abyDst.insert(abyDst.end(), abySrc.begin(), abySrc.end());

    const uint32_t nComputedCRC_le =
        CPL_LSBWORD32(ComputeCRC32C(abySrc.data(), abySrc.size()));
    const GByte *pabyCRC = reinterpret_cast<const GByte *>(&nComputedCRC_le);
    abyDst.insert(abyDst.end(), pabyCRC, pabyCRC + sizeof(uint32_t));

    return true;
}

/************************************************************************/
/*                        ZarrV3CodecCRC32C::Decode()                   */
/************************************************************************/

bool ZarrV3CodecCRC32C::Decode(const ZarrByteVectorQuickResize &abySrc,
                               ZarrByteVectorQuickResize &abyDst) const
{
    if (abySrc.size() < sizeof(uint32_t))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CRC32C decoder: not enough input bytes");
        return false;
    }

    const size_t nSrcLen = abySrc.size() - sizeof(uint32_t);
    abyDst.clear();
    abyDst.insert(abyDst.end(), abySrc.begin(), abySrc.begin() + nSrcLen);

    if (m_bCheckCRC)
    {
        const uint32_t nComputedCRC =
            ComputeCRC32C(abyDst.data(), abyDst.size());
        const uint32_t nExpectedCRC = CPL_LSBUINT32PTR(abySrc.data() + nSrcLen);
        if (nComputedCRC != nExpectedCRC)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "CRC32C decoder: computed CRC value is %08X whereas expected "
                "value is %08X",
                nComputedCRC, nExpectedCRC);
            return false;
        }
    }

    return true;
}
