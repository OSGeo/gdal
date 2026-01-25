/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "blosc" codec
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include "cpl_compressor.h"

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/blosc/index.html

/************************************************************************/
/*                          ZarrV3CodecBlosc()                          */
/************************************************************************/

ZarrV3CodecBlosc::ZarrV3CodecBlosc() : ZarrV3CodecAbstractCompressor(NAME)
{
}

/************************************************************************/
/*                          GetConfiguration()                          */
/************************************************************************/

/* static */ CPLJSONObject
ZarrV3CodecBlosc::GetConfiguration(const char *cname, int clevel,
                                   const char *shuffle, int typesize,
                                   int blocksize)
{
    CPLJSONObject oConfig;
    oConfig.Add("cname", cname);
    oConfig.Add("clevel", clevel);
    oConfig.Add("shuffle", shuffle);
    if (strcmp(shuffle, "noshuffle") != 0)
        oConfig.Add("typesize", typesize);
    oConfig.Add("blocksize", blocksize);
    return oConfig;
}

/************************************************************************/
/*              ZarrV3CodecBlosc::InitFromConfiguration()               */
/************************************************************************/

bool ZarrV3CodecBlosc::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_pCompressor = CPLGetCompressor("blosc");
    m_pDecompressor = CPLGetDecompressor("blosc");
    if (!m_pCompressor || !m_pDecompressor)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "blosc compressor not available");
        return false;
    }

    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    // byte->byte codec
    oOutputArrayMetadata = oInputArrayMetadata;

    if (!configuration.IsValid() ||
        configuration.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec blosc: configuration missing or not an object");
        return false;
    }

    for (const auto &oChild : configuration.GetChildren())
    {
        const auto osName = oChild.GetName();
        if (osName != "cname" && osName != "clevel" && osName != "shuffle" &&
            osName != "typesize" && osName != "blocksize")
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Codec blosc: configuration contains a unhandled member: %s",
                osName.c_str());
            return false;
        }
    }

    const auto oCname = configuration.GetObj("cname");
    if (oCname.GetType() != CPLJSONObject::Type::String)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec blosc: cname is missing or not a string");
        return false;
    }
    m_aosCompressorOptions.SetNameValue("CNAME", oCname.ToString().c_str());

    const auto oLevel = configuration.GetObj("clevel");
    if (oLevel.IsValid())
    {
        if (oLevel.GetType() != CPLJSONObject::Type::Integer)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec blosc: clevel is not an integer");
            return false;
        }
        const int nLevel = oLevel.ToInteger();
        if (nLevel < 0 || nLevel > 9)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec blosc: invalid clevel value for level: %d", nLevel);
            return false;
        }
        m_aosCompressorOptions.SetNameValue("CLEVEL", CPLSPrintf("%d", nLevel));
    }

    const auto oShuffle = configuration.GetObj("shuffle");
    if (oShuffle.GetType() != CPLJSONObject::Type::String)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec blosc: shuffle is missing or not a string");
        return false;
    }
    if (oShuffle.ToString() == "noshuffle")
        m_aosCompressorOptions.SetNameValue("SHUFFLE", "NONE");
    else if (oShuffle.ToString() == "shuffle")
        m_aosCompressorOptions.SetNameValue("SHUFFLE", "BYTE");
    else if (oShuffle.ToString() == "bitshuffle")
        m_aosCompressorOptions.SetNameValue("SHUFFLE", "BIT");
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec blosc: Invalid value for shuffle");
        return false;
    }

    const auto oTypesize = configuration.GetObj("typesize");
    if (oTypesize.IsValid())
    {
        if (oTypesize.GetType() != CPLJSONObject::Type::Integer)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec blosc: typesize is not an integer");
            return false;
        }
        const int nTypeSize = oTypesize.ToInteger();
        m_aosCompressorOptions.SetNameValue("TYPESIZE",
                                            CPLSPrintf("%d", nTypeSize));
    }

    const auto oBlocksize = configuration.GetObj("blocksize");
    if (oBlocksize.IsValid())
    {
        if (oBlocksize.GetType() != CPLJSONObject::Type::Integer)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec blosc: blocksize is not an integer");
            return false;
        }
        const int nBlocksize = oBlocksize.ToInteger();
        m_aosCompressorOptions.SetNameValue("BLOCKSIZE",
                                            CPLSPrintf("%d", nBlocksize));
    }

    return true;
}

/************************************************************************/
/*                      ZarrV3CodecBlosc::Clone()                       */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecBlosc::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecBlosc>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}
