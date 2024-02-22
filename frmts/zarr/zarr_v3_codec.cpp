/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
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

#include "zarr.h"

#include "cpl_compressor.h"

/************************************************************************/
/*                          ZarrV3Codec()                               */
/************************************************************************/

ZarrV3Codec::ZarrV3Codec(const std::string &osName) : m_osName(osName)
{
}

/************************************************************************/
/*                         ~ZarrV3Codec()                               */
/************************************************************************/

ZarrV3Codec::~ZarrV3Codec() = default;

/************************************************************************/
/*                        ZarrV3CodecGZip()                             */
/************************************************************************/

ZarrV3CodecGZip::ZarrV3CodecGZip() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                       ~ZarrV3CodecGZip()                             */
/************************************************************************/

ZarrV3CodecGZip::~ZarrV3CodecGZip() = default;

/************************************************************************/
/*                           GetConfiguration()                         */
/************************************************************************/

/* static */ CPLJSONObject ZarrV3CodecGZip::GetConfiguration(int nLevel)
{
    CPLJSONObject oConfig;
    oConfig.Add("level", nLevel);
    return oConfig;
}

/************************************************************************/
/*                   ZarrV3CodecGZip::InitFromConfiguration()           */
/************************************************************************/

bool ZarrV3CodecGZip::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata)
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
/*                      ZarrV3CodecGZip::Clone()                        */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecGZip::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecGZip>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata);
    return psClone;
}

/************************************************************************/
/*                      ZarrV3CodecGZip::Encode()                       */
/************************************************************************/

bool ZarrV3CodecGZip::Encode(const ZarrByteVectorQuickResize &abySrc,
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
                 "ZarrV3CodecGZip::Encode(): output buffer too small");
    }
    return bRet;
}

/************************************************************************/
/*                      ZarrV3CodecGZip::Decode()                       */
/************************************************************************/

bool ZarrV3CodecGZip::Decode(const ZarrByteVectorQuickResize &abySrc,
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
                 "ZarrV3CodecGZip::Decode(): output buffer too small");
    }
    return bRet;
}

/************************************************************************/
/*                       ZarrV3CodecBlosc()                             */
/************************************************************************/

ZarrV3CodecBlosc::ZarrV3CodecBlosc() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                      ~ZarrV3CodecBlosc()                             */
/************************************************************************/

ZarrV3CodecBlosc::~ZarrV3CodecBlosc() = default;

/************************************************************************/
/*                           GetConfiguration()                         */
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
/*                   ZarrV3CodecBlosc::InitFromConfiguration()           */
/************************************************************************/

bool ZarrV3CodecBlosc::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata)
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
                     "Codec blosc: clevel value for level: %d", nLevel);
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
/*                      ZarrV3CodecBlosc::Clone()                        */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecBlosc::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecBlosc>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata);
    return psClone;
}

/************************************************************************/
/*                      ZarrV3CodecBlosc::Encode()                       */
/************************************************************************/

bool ZarrV3CodecBlosc::Encode(const ZarrByteVectorQuickResize &abySrc,
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
                 "ZarrV3CodecBlosc::Encode(): output buffer too small");
    }
    return bRet;
}

/************************************************************************/
/*                      ZarrV3CodecBlosc::Decode()                       */
/************************************************************************/

bool ZarrV3CodecBlosc::Decode(const ZarrByteVectorQuickResize &abySrc,
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
                 "ZarrV3CodecBlosc::Decode(): output buffer too small");
    }
    return bRet;
}

/************************************************************************/
/*                       ZarrV3CodecEndian()                            */
/************************************************************************/

ZarrV3CodecEndian::ZarrV3CodecEndian() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                       ~ZarrV3CodecEndian()                           */
/************************************************************************/

ZarrV3CodecEndian::~ZarrV3CodecEndian() = default;

/************************************************************************/
/*                           GetConfiguration()                         */
/************************************************************************/

/* static */ CPLJSONObject ZarrV3CodecEndian::GetConfiguration(bool bLittle)
{
    CPLJSONObject oConfig;
    oConfig.Add("endian", bLittle ? "little" : "big");
    return oConfig;
}

/************************************************************************/
/*                 ZarrV3CodecEndian::InitFromConfiguration()           */
/************************************************************************/

bool ZarrV3CodecEndian::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata)
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
/*                     ZarrV3CodecEndian::Clone()                       */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecEndian::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecEndian>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata);
    return psClone;
}

/************************************************************************/
/*                      ZarrV3CodecEndian::Encode()                     */
/************************************************************************/

bool ZarrV3CodecEndian::Encode(const ZarrByteVectorQuickResize &abySrc,
                               ZarrByteVectorQuickResize &abyDst) const
{
    CPLAssert(!IsNoOp());

    size_t nEltCount = m_oInputArrayMetadata.GetEltCount();
    size_t nNativeSize = m_oInputArrayMetadata.oElt.nativeSize;
    if (abySrc.size() < nEltCount * nNativeSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ZarrV3CodecTranspose::Encode(): input buffer too small");
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
/*                      ZarrV3CodecEndian::Decode()                     */
/************************************************************************/

bool ZarrV3CodecEndian::Decode(const ZarrByteVectorQuickResize &abySrc,
                               ZarrByteVectorQuickResize &abyDst) const
{
    return Encode(abySrc, abyDst);
}

/************************************************************************/
/*                       ZarrV3CodecTranspose()                         */
/************************************************************************/

ZarrV3CodecTranspose::ZarrV3CodecTranspose() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                       ~ZarrV3CodecTranspose()                        */
/************************************************************************/

ZarrV3CodecTranspose::~ZarrV3CodecTranspose() = default;

/************************************************************************/
/*                             IsNoOp()                                 */
/************************************************************************/

bool ZarrV3CodecTranspose::IsNoOp() const
{
    for (int i = 0; i < static_cast<int>(m_anOrder.size()); ++i)
    {
        if (m_anOrder[i] != i)
            return false;
    }
    return true;
}

/************************************************************************/
/*                           GetConfiguration()                         */
/************************************************************************/

/* static */ CPLJSONObject
ZarrV3CodecTranspose::GetConfiguration(const std::vector<int> &anOrder)
{
    CPLJSONObject oConfig;
    CPLJSONArray oOrder;
    for (const auto nVal : anOrder)
        oOrder.Add(nVal);
    oConfig.Add("order", oOrder);
    return oConfig;
}

/************************************************************************/
/*                           GetConfiguration()                         */
/************************************************************************/

/* static */ CPLJSONObject
ZarrV3CodecTranspose::GetConfiguration(const std::string &osOrder)
{
    CPLJSONObject oConfig;
    CPLJSONArray oOrder;
    oConfig.Add("order", osOrder);
    return oConfig;
}

/************************************************************************/
/*                ZarrV3CodecTranspose::InitFromConfiguration()         */
/************************************************************************/

bool ZarrV3CodecTranspose::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata)
{
    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    oOutputArrayMetadata = oInputArrayMetadata;

    if (!configuration.IsValid() &&
        configuration.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec transpose: configuration missing or not an object");
        return false;
    }

    for (const auto &oChild : configuration.GetChildren())
    {
        if (oChild.GetName() != "order")
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec transpose: configuration contains a unhandled "
                     "member: %s",
                     oChild.GetName().c_str());
            return false;
        }
    }

    const auto oOrder = configuration.GetObj("order");
    const int nDims = static_cast<int>(oInputArrayMetadata.anBlockSizes.size());
    if (oOrder.GetType() == CPLJSONObject::Type::String)
    {
        const auto osOrder = oOrder.ToString();
        if (osOrder == "C")
        {
            for (int i = 0; i < nDims; ++i)
            {
                m_anOrder.push_back(i);
            }
        }
        else if (osOrder == "F")
        {
            for (int i = 0; i < nDims; ++i)
            {
                m_anOrder.push_back(nDims - 1 - i);
                oOutputArrayMetadata.anBlockSizes[i] =
                    oInputArrayMetadata.anBlockSizes[nDims - 1 - i];
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec transpose: invalid value for order");
            return false;
        }
    }
    else if (oOrder.GetType() == CPLJSONObject::Type::Array)
    {
        const auto oOrderArray = oOrder.ToArray();
        if (oOrderArray.Size() != nDims)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec transpose: order[] does not have the expected "
                     "number of elements");
            return false;
        }
        std::vector<int> oSet(nDims);
        oOutputArrayMetadata.anBlockSizes.clear();
        for (const auto &oVal : oOrderArray)
        {
            const int nVal = oVal.ToInteger();
            if (nVal < 0 || nVal >= nDims || oSet[nVal])
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec transpose: order[] does not define a valid "
                         "transposition");
                return false;
            }
            oSet[nVal] = true;
            m_anOrder.push_back(nVal);
            oOutputArrayMetadata.anBlockSizes.push_back(
                oInputArrayMetadata.anBlockSizes[nVal]);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec transpose: invalid value for order");
        return false;
    }

    int i = 0;
    m_anReverseOrder.resize(m_anOrder.size());
    for (const auto nVal : m_anOrder)
    {
        m_anReverseOrder[nVal] = i;
        ++i;
    }

    return true;
}

/************************************************************************/
/*                   ZarrV3CodecTranspose::Clone()                      */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecTranspose::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecTranspose>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata);
    return psClone;
}

/************************************************************************/
/*                  ZarrV3CodecTranspose::Transpose()                   */
/************************************************************************/

bool ZarrV3CodecTranspose::Transpose(const ZarrByteVectorQuickResize &abySrc,
                                     ZarrByteVectorQuickResize &abyDst,
                                     bool bEncodeDirection) const
{
    CPLAssert(m_anOrder.size() == m_oInputArrayMetadata.anBlockSizes.size());
    CPLAssert(m_anReverseOrder.size() ==
              m_oInputArrayMetadata.anBlockSizes.size());
    const size_t nDims = m_anOrder.size();
    const size_t nSourceSize = m_oInputArrayMetadata.oElt.nativeSize;
    const auto &anBlockSizes = m_oInputArrayMetadata.anBlockSizes;
    CPLAssert(nDims > 0);
    if (abySrc.size() < m_oInputArrayMetadata.GetEltCount() * nSourceSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ZarrV3CodecTranspose::Transpose(): input buffer too small");
        return false;
    }
    abyDst.resize(m_oInputArrayMetadata.GetEltCount() * nSourceSize);

    struct Stack
    {
        size_t nIters = 0;
        const GByte *src_ptr = nullptr;
        GByte *dst_ptr = nullptr;
        size_t src_inc_offset = 0;
        size_t dst_inc_offset = 0;
    };

    std::vector<Stack> stack(nDims);
    stack.emplace_back(
        Stack());  // to make gcc 9.3 -O2 -Wnull-dereference happy

    if (!bEncodeDirection)
    {
        stack[m_anReverseOrder[nDims - 1]].src_inc_offset = nSourceSize;
        size_t nStride = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            nStride *=
                static_cast<size_t>(anBlockSizes[m_anReverseOrder[i + 1]]);
            stack[m_anReverseOrder[i]].src_inc_offset = nStride;
        }

        stack[nDims - 1].dst_inc_offset = nSourceSize;
        nStride = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            nStride *= static_cast<size_t>(anBlockSizes[i + 1]);
            stack[i].dst_inc_offset = nStride;
        }
    }
    else
    {
        stack[m_anReverseOrder[nDims - 1]].dst_inc_offset = nSourceSize;
        size_t nStride = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            nStride *=
                static_cast<size_t>(anBlockSizes[m_anReverseOrder[i + 1]]);
            stack[m_anReverseOrder[i]].dst_inc_offset = nStride;
        }

        stack[nDims - 1].src_inc_offset = nSourceSize;
        nStride = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            nStride *= static_cast<size_t>(anBlockSizes[i + 1]);
            stack[i].src_inc_offset = nStride;
        }
    }

    stack[0].src_ptr = abySrc.data();
    stack[0].dst_ptr = &abyDst[0];

    size_t dimIdx = 0;
lbl_next_depth:
    if (dimIdx == nDims)
    {
        void *dst_ptr = stack[nDims].dst_ptr;
        const void *src_ptr = stack[nDims].src_ptr;
        if (nSourceSize == 1)
            *stack[nDims].dst_ptr = *stack[nDims].src_ptr;
        else if (nSourceSize == 2)
            *static_cast<uint16_t *>(dst_ptr) =
                *static_cast<const uint16_t *>(src_ptr);
        else if (nSourceSize == 4)
            *static_cast<uint32_t *>(dst_ptr) =
                *static_cast<const uint32_t *>(src_ptr);
        else if (nSourceSize == 8)
            *static_cast<uint64_t *>(dst_ptr) =
                *static_cast<const uint64_t *>(src_ptr);
        else
            memcpy(dst_ptr, src_ptr, nSourceSize);
    }
    else
    {
        stack[dimIdx].nIters = static_cast<size_t>(anBlockSizes[dimIdx]);
        while (true)
        {
            dimIdx++;
            stack[dimIdx].src_ptr = stack[dimIdx - 1].src_ptr;
            stack[dimIdx].dst_ptr = stack[dimIdx - 1].dst_ptr;
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if ((--stack[dimIdx].nIters) == 0)
                break;
            stack[dimIdx].src_ptr += stack[dimIdx].src_inc_offset;
            stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                    ZarrV3CodecTranspose::Encode()                    */
/************************************************************************/

bool ZarrV3CodecTranspose::Encode(const ZarrByteVectorQuickResize &abySrc,
                                  ZarrByteVectorQuickResize &abyDst) const
{
    CPLAssert(!IsNoOp());

    return Transpose(abySrc, abyDst, true);
}

/************************************************************************/
/*                    ZarrV3CodecTranspose::Decode()                    */
/************************************************************************/

bool ZarrV3CodecTranspose::Decode(const ZarrByteVectorQuickResize &abySrc,
                                  ZarrByteVectorQuickResize &abyDst) const
{
    CPLAssert(!IsNoOp());

    return Transpose(abySrc, abyDst, false);
}

/************************************************************************/
/*                    ZarrV3CodecSequence::Clone()                      */
/************************************************************************/

std::unique_ptr<ZarrV3CodecSequence> ZarrV3CodecSequence::Clone() const
{
    auto poClone = std::make_unique<ZarrV3CodecSequence>(m_oInputArrayMetadata);
    for (const auto &poCodec : m_apoCodecs)
        poClone->m_apoCodecs.emplace_back(poCodec->Clone());
    poClone->m_oCodecArray = m_oCodecArray.Clone();
    return poClone;
}

/************************************************************************/
/*                    ZarrV3CodecSequence::InitFromJson()               */
/************************************************************************/

bool ZarrV3CodecSequence::InitFromJson(const CPLJSONObject &oCodecs)
{
    if (oCodecs.GetType() != CPLJSONObject::Type::Array)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "codecs is not an array");
        return false;
    }
    auto oCodecsArray = oCodecs.ToArray();

    ZarrArrayMetadata oInputArrayMetadata = m_oInputArrayMetadata;
    ZarrV3Codec::IOType eLastType = ZarrV3Codec::IOType::ARRAY;
    std::string osLastCodec;

#if !CPL_IS_LSB
    const auto InsertImplicitEndianCodecIfNeeded =
        [this, &oInputArrayMetadata, &eLastType, &osLastCodec]()
    {
        // Insert a little endian codec if we are on a big endian target
        if (eLastType == ZarrV3Codec::IOType::ARRAY &&
            oInputArrayMetadata.oElt.nativeSize > 1)
        {
            auto poEndianCodec = std::make_unique<ZarrV3CodecEndian>();
            ZarrArrayMetadata oOutputArrayMetadata;
            poEndianCodec->InitFromConfiguration(
                ZarrV3CodecEndian::GetConfiguration(true), oInputArrayMetadata,
                oOutputArrayMetadata);
            oInputArrayMetadata = oOutputArrayMetadata;
            eLastType = poEndianCodec->GetOutputType();
            osLastCodec = poEndianCodec->GetName();
            m_apoCodecs.emplace_back(std::move(poEndianCodec));
        }
    };
#endif

    for (const auto &oCodec : oCodecsArray)
    {
        if (oCodec.GetType() != CPLJSONObject::Type::Object)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "codecs[] is not an array");
            return false;
        }
        const auto osName = oCodec["name"].ToString();
        std::unique_ptr<ZarrV3Codec> poCodec;
        if (osName == "gzip")
            poCodec = std::make_unique<ZarrV3CodecGZip>();
        else if (osName == "blosc")
            poCodec = std::make_unique<ZarrV3CodecBlosc>();
        else if (osName == "endian")
            poCodec = std::make_unique<ZarrV3CodecEndian>();
        else if (osName == "transpose")
            poCodec = std::make_unique<ZarrV3CodecTranspose>();
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Unsupported codec: %s",
                     osName.c_str());
            return false;
        }

        if (poCodec->GetInputType() == ZarrV3Codec::IOType::ARRAY)
        {
            if (eLastType == ZarrV3Codec::IOType::BYTES)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot chain codec %s with %s",
                         poCodec->GetName().c_str(), osLastCodec.c_str());
                return false;
            }
        }
#if !CPL_IS_LSB
        else
        {
            InsertImplicitEndianCodecIfNeeded();
        }
#endif

        ZarrArrayMetadata oOutputArrayMetadata;
        if (!poCodec->InitFromConfiguration(oCodec["configuration"],
                                            oInputArrayMetadata,
                                            oOutputArrayMetadata))
        {
            return false;
        }
        oInputArrayMetadata = std::move(oOutputArrayMetadata);
        eLastType = poCodec->GetOutputType();
        osLastCodec = poCodec->GetName();

        if (!poCodec->IsNoOp())
            m_apoCodecs.emplace_back(std::move(poCodec));
    }

#if !CPL_IS_LSB
    InsertImplicitEndianCodecIfNeeded();
#endif

    m_oCodecArray = oCodecs.Clone();
    return true;
}

/************************************************************************/
/*                  ZarrV3CodecEndian::AllocateBuffer()                 */
/************************************************************************/

bool ZarrV3CodecSequence::AllocateBuffer(ZarrByteVectorQuickResize &abyBuffer)
{
    if (!m_apoCodecs.empty())
    {
        const size_t nRawSize = m_oInputArrayMetadata.GetEltCount() *
                                m_oInputArrayMetadata.oElt.nativeSize;
        // Grow the temporary buffer a bit beyond the uncompressed size
        const size_t nMaxSize = nRawSize + nRawSize / 3 + 64;
        try
        {
            m_abyTmp.resize(nMaxSize);
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return false;
        }
        m_abyTmp.resize(nRawSize);

        // Grow the input/output buffer too if we have several steps
        if (m_apoCodecs.size() >= 2 && abyBuffer.capacity() < nMaxSize)
        {
            const size_t nSize = abyBuffer.size();
            try
            {
                abyBuffer.resize(nMaxSize);
            }
            catch (const std::exception &e)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
                return false;
            }
            abyBuffer.resize(nSize);
        }
    }
    return true;
}

/************************************************************************/
/*                    ZarrV3CodecSequence::Encode()                     */
/************************************************************************/

bool ZarrV3CodecSequence::Encode(ZarrByteVectorQuickResize &abyBuffer)
{
    if (!AllocateBuffer(abyBuffer))
        return false;
    for (const auto &poCodec : m_apoCodecs)
    {
        if (!poCodec->Encode(abyBuffer, m_abyTmp))
            return false;
        std::swap(abyBuffer, m_abyTmp);
    }
    return true;
}

/************************************************************************/
/*                    ZarrV3CodecSequence::Decode()                     */
/************************************************************************/

bool ZarrV3CodecSequence::Decode(ZarrByteVectorQuickResize &abyBuffer)
{
    if (!AllocateBuffer(abyBuffer))
        return false;
    for (auto iter = m_apoCodecs.rbegin(); iter != m_apoCodecs.rend(); ++iter)
    {
        const auto &poCodec = *iter;
        if (!poCodec->Decode(abyBuffer, m_abyTmp))
            return false;
        std::swap(abyBuffer, m_abyTmp);
    }
    return true;
}
