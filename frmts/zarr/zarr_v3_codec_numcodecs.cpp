/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "numcodecs.zlib" and "numcodecs.shuffle" codecs
 *
 ******************************************************************************
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"
#include "zarr.h"

#include "cpl_compressor.h"

// Zarr v3 names of the numcodecs zlib compressor and shuffle filter, as
// written by zarr-python's numcodecs.zarr3 wrappers (and by tools that map
// TIFF DEFLATE / HDF5 shuffle+deflate chunks to Zarr v3, e.g. virtual
// references in Icechunk or kerchunk).

/************************************************************************/
/*                       ZarrV3CodecNumcodecsZlib                       */
/************************************************************************/

ZarrV3CodecNumcodecsZlib::ZarrV3CodecNumcodecsZlib()
    : ZarrV3CodecAbstractCompressor(NAME)
{
}

bool ZarrV3CodecNumcodecsZlib::InitFromConfiguration(
    const std::string & /* osArrayName */, const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_pCompressor = CPLGetCompressor("zlib");
    m_pDecompressor = CPLGetDecompressor("zlib");
    if (!m_pCompressor || !m_pDecompressor)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "zlib compressor not available");
        return false;
    }

    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    // byte->byte codec
    oOutputArrayMetadata = oInputArrayMetadata;

    int nLevel = 1;  // numcodecs.zlib default
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
            if (oChild.GetName() != "level")
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec %s: configuration contains a unhandled "
                         "member: %s",
                         NAME, oChild.GetName().c_str());
                return false;
            }
        }
        const auto oLevel = configuration.GetObj("level");
        if (oLevel.IsValid())
        {
            if (oLevel.GetType() != CPLJSONObject::Type::Integer)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec %s: level is not an integer", NAME);
                return false;
            }
            nLevel = oLevel.ToInteger();
            if (nLevel < 0 || nLevel > 9)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec %s: invalid value for level: %d", NAME, nLevel);
                return false;
            }
        }
    }

    m_aosCompressorOptions.SetNameValue("LEVEL", CPLSPrintf("%d", nLevel));
    return true;
}

std::unique_ptr<ZarrV3Codec> ZarrV3CodecNumcodecsZlib::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecNumcodecsZlib>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(std::string(), m_oConfiguration,
                                   m_oInputArrayMetadata, oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}

/************************************************************************/
/*                     ZarrV3CodecNumcodecsShuffle                      */
/************************************************************************/

ZarrV3CodecNumcodecsShuffle::ZarrV3CodecNumcodecsShuffle()
    : ZarrV3CodecAbstractCompressor(NAME)
{
}

bool ZarrV3CodecNumcodecsShuffle::InitFromConfiguration(
    const std::string & /* osArrayName */, const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_pCompressor = ZarrGetShuffleCompressor();
    m_pDecompressor = ZarrGetShuffleDecompressor();

    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    // byte->byte codec
    oOutputArrayMetadata = oInputArrayMetadata;

    int nElementSize = 4;  // numcodecs.shuffle default
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
            if (oChild.GetName() != "elementsize")
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec %s: configuration contains a unhandled "
                         "member: %s",
                         NAME, oChild.GetName().c_str());
                return false;
            }
        }
        const auto oSize = configuration.GetObj("elementsize");
        if (oSize.IsValid())
        {
            if (oSize.GetType() != CPLJSONObject::Type::Integer)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec %s: elementsize is not an integer", NAME);
                return false;
            }
            nElementSize = oSize.ToInteger();
        }
    }
    if (nElementSize != 1 && nElementSize != 2 && nElementSize != 4 &&
        nElementSize != 8)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Codec %s: only elementsize = 1, 2, 4 or 8 is supported "
                 "(got %d)",
                 NAME, nElementSize);
        return false;
    }

    m_aosCompressorOptions.SetNameValue("ELEMENTSIZE",
                                        CPLSPrintf("%d", nElementSize));
    return true;
}

bool ZarrV3CodecNumcodecsShuffle::Decode(
    const ZarrByteVectorQuickResize &abySrc,
    ZarrByteVectorQuickResize &abyDst) const
{
    abyDst.resize(abySrc.size());
    void *pOutputData = abyDst.data();
    size_t nOutputSize = abyDst.size();
    const bool bRet = m_pDecompressor->pfnFunc(
        abySrc.data(), abySrc.size(), &pOutputData, &nOutputSize,
        m_aosCompressorOptions.List(), m_pDecompressor->user_data);
    if (bRet)
        abyDst.resize(nOutputSize);
    return bRet;
}

std::unique_ptr<ZarrV3Codec> ZarrV3CodecNumcodecsShuffle::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecNumcodecsShuffle>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(std::string(), m_oConfiguration,
                                   m_oInputArrayMetadata, oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}
