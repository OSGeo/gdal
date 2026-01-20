/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, ZarrV3CodecAbstractCompressor class
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include "cpl_compressor.h"

/************************************************************************/
/*                      ZarrV3CodecAbstractCompressor()                 */
/************************************************************************/

ZarrV3CodecAbstractCompressor::ZarrV3CodecAbstractCompressor(
    const std::string &osName)
    : ZarrV3Codec(osName)
{
}

/************************************************************************/
/*                 ZarrV3CodecAbstractCompressor::Encode()              */
/************************************************************************/

bool ZarrV3CodecAbstractCompressor::Encode(
    const ZarrByteVectorQuickResize &abySrc,
    ZarrByteVectorQuickResize &abyDst) const
{
    abyDst.resize(abyDst.capacity());
    void *pOutputData = abyDst.data();
    size_t nOutputSize = abyDst.size();
    bool bRet = m_pCompressor->pfnFunc(
        abySrc.data(), abySrc.size(), &pOutputData, &nOutputSize,
        m_aosCompressorOptions.List(), m_pCompressor->user_data);
    if (bRet)
    {
        abyDst.resize(nOutputSize);
    }
    else if (nOutputSize > abyDst.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s codec:Encode(): output buffer too small",
                 m_osName.c_str());
    }
    return bRet;
}

/************************************************************************/
/*                 ZarrV3CodecAbstractCompressor::Decode()              */
/************************************************************************/

bool ZarrV3CodecAbstractCompressor::Decode(
    const ZarrByteVectorQuickResize &abySrc,
    ZarrByteVectorQuickResize &abyDst) const
{
    abyDst.resize(abyDst.capacity());
    void *pOutputData = abyDst.data();
    size_t nOutputSize = abyDst.size();
    bool bRet = m_pDecompressor->pfnFunc(abySrc.data(), abySrc.size(),
                                         &pOutputData, &nOutputSize, nullptr,
                                         m_pDecompressor->user_data);
    if (bRet)
    {
        abyDst.resize(nOutputSize);
    }
    else if (nOutputSize > abyDst.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s codec:Decode(): output buffer too small",
                 m_osName.c_str());
    }
    return bRet;
}
