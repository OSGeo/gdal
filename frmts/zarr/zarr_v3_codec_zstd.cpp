/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "zstd" (extension) codec
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include "cpl_compressor.h"

// Implements https://github.com/zarr-developers/zarr-extensions/tree/main/codecs/zstd

/************************************************************************/
/*                        ZarrV3CodecZstd()                             */
/************************************************************************/

ZarrV3CodecZstd::ZarrV3CodecZstd() : ZarrV3CodecAbstractCompressor(NAME)
{
}

/************************************************************************/
/*                           GetConfiguration()                         */
/************************************************************************/

/* static */ CPLJSONObject ZarrV3CodecZstd::GetConfiguration(int nLevel,
                                                             bool checksum)
{
    CPLJSONObject oConfig;
    oConfig.Add("level", nLevel);
    oConfig.Add("checksum", checksum);
    return oConfig;
}

/************************************************************************/
/*                   ZarrV3CodecZstd::InitFromConfiguration()           */
/************************************************************************/

bool ZarrV3CodecZstd::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_pCompressor = CPLGetCompressor("zstd");
    m_pDecompressor = CPLGetDecompressor("zstd");
    if (!m_pCompressor || !m_pDecompressor)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "zstd compressor not available");
        return false;
    }

    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    // byte->byte codec
    oOutputArrayMetadata = oInputArrayMetadata;

    int nLevel = 13;
    bool bChecksum = false;

    if (configuration.IsValid())
    {
        if (configuration.GetType() != CPLJSONObject::Type::Object)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec zstd: configuration is not an object");
            return false;
        }

        for (const auto &oChild : configuration.GetChildren())
        {
            if (oChild.GetName() != "level" && oChild.GetName() != "checksum")
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Codec zstd: configuration contains a unhandled member: %s",
                    oChild.GetName().c_str());
                return false;
            }
        }

        const auto oLevel = configuration.GetObj("level");
        if (oLevel.IsValid())
        {
            if (oLevel.GetType() != CPLJSONObject::Type::Integer)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec zstd: level is not an integer");
                return false;
            }
            nLevel = oLevel.ToInteger();
            if (nLevel < 0 || nLevel > 22)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec zstd: invalid value for level: %d", nLevel);
                return false;
            }
        }

        const auto oChecksum = configuration.GetObj("checksum");
        if (oChecksum.IsValid())
        {
            if (oChecksum.GetType() != CPLJSONObject::Type::Boolean)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec zstd: checksum is not a boolean");
                return false;
            }
            bChecksum = oChecksum.ToBool();
        }
    }

    m_aosCompressorOptions.SetNameValue("LEVEL", CPLSPrintf("%d", nLevel));
    if (bChecksum)
        m_aosCompressorOptions.SetNameValue("CHECKSUM", "YES");

    return true;
}

/************************************************************************/
/*                      ZarrV3CodecZstd::Clone()                        */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecZstd::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecZstd>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}
