/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "gzip" codec
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include "cpl_compressor.h"

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/gzip/index.html

/************************************************************************/
/*                          ZarrV3CodecGZip()                           */
/************************************************************************/

ZarrV3CodecGZip::ZarrV3CodecGZip() : ZarrV3CodecAbstractCompressor(NAME)
{
}

/************************************************************************/
/*                          GetConfiguration()                          */
/************************************************************************/

/* static */ CPLJSONObject ZarrV3CodecGZip::GetConfiguration(int nLevel)
{
    CPLJSONObject oConfig;
    oConfig.Add("level", nLevel);
    return oConfig;
}

/************************************************************************/
/*               ZarrV3CodecGZip::InitFromConfiguration()               */
/************************************************************************/

bool ZarrV3CodecGZip::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_pCompressor = CPLGetCompressor("gzip");
    m_pDecompressor = CPLGetDecompressor("gzip");
    if (!m_pCompressor || !m_pDecompressor)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "gzip compressor not available");
        return false;
    }

    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    // byte->byte codec
    oOutputArrayMetadata = oInputArrayMetadata;

    int nLevel = 6;

    if (configuration.IsValid())
    {
        if (configuration.GetType() != CPLJSONObject::Type::Object)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec gzip: configuration is not an object");
            return false;
        }

        for (const auto &oChild : configuration.GetChildren())
        {
            if (oChild.GetName() != "level")
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Codec gzip: configuration contains a unhandled member: %s",
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
                         "Codec gzip: level is not an integer");
                return false;
            }
            nLevel = oLevel.ToInteger();
            if (nLevel < 0 || nLevel > 9)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec gzip: invalid value for level: %d", nLevel);
                return false;
            }
        }
    }

    m_aosCompressorOptions.SetNameValue("LEVEL", CPLSPrintf("%d", nLevel));

    return true;
}

/************************************************************************/
/*                       ZarrV3CodecGZip::Clone()                       */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecGZip::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecGZip>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}
