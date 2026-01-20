/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, ZarrV3CodecSequence class
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

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

    const auto InsertImplicitEndianCodecIfNeeded =
        [this, &oInputArrayMetadata, &eLastType, &osLastCodec]()
    {
        CPL_IGNORE_RET_VAL(this);
        if (eLastType == ZarrV3Codec::IOType::ARRAY &&
            oInputArrayMetadata.oElt.nativeSize > 1)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "'bytes' codec missing. Assuming little-endian storage, "
                     "but such tolerance may be removed in future versions");
            auto poEndianCodec = std::make_unique<ZarrV3CodecBytes>();
            ZarrArrayMetadata oTmpOutputArrayMetadata;
            poEndianCodec->InitFromConfiguration(
                ZarrV3CodecBytes::GetConfiguration(true), oInputArrayMetadata,
                oTmpOutputArrayMetadata, /* bEmitWarnings = */ true);
            oInputArrayMetadata = std::move(oTmpOutputArrayMetadata);
            eLastType = poEndianCodec->GetOutputType();
            osLastCodec = poEndianCodec->GetName();
            if constexpr (!CPL_IS_LSB)
            {
                // Insert a little endian codec if we are on a big endian target
                m_apoCodecs.emplace_back(std::move(poEndianCodec));
            }
        }
    };

    for (const auto &oCodec : oCodecsArray)
    {
        if (oCodec.GetType() != CPLJSONObject::Type::Object)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "codecs[] is not an object");
            return false;
        }
        const auto osName = oCodec["name"].ToString();
        std::unique_ptr<ZarrV3Codec> poCodec;
        if (osName == ZarrV3CodecGZip::NAME)
            poCodec = std::make_unique<ZarrV3CodecGZip>();
        else if (osName == ZarrV3CodecBlosc::NAME)
            poCodec = std::make_unique<ZarrV3CodecBlosc>();
        else if (osName == ZarrV3CodecZstd::NAME)
            poCodec = std::make_unique<ZarrV3CodecZstd>();
        else if (osName == ZarrV3CodecBytes::NAME ||
                 osName == "endian" /* endian is the old name */)
            poCodec = std::make_unique<ZarrV3CodecBytes>();
        else if (osName == ZarrV3CodecTranspose::NAME)
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
        else
        {
            InsertImplicitEndianCodecIfNeeded();
        }

        ZarrArrayMetadata oStepOutputArrayMetadata;
        if (!poCodec->InitFromConfiguration(oCodec["configuration"],
                                            oInputArrayMetadata,
                                            oStepOutputArrayMetadata,
                                            /* bEmitWarnings = */ true))
        {
            return false;
        }
        oInputArrayMetadata = std::move(oStepOutputArrayMetadata);
        eLastType = poCodec->GetOutputType();
        osLastCodec = poCodec->GetName();

        if (!poCodec->IsNoOp())
            m_apoCodecs.emplace_back(std::move(poCodec));
    }

    InsertImplicitEndianCodecIfNeeded();

    m_oCodecArray = oCodecs.Clone();
    return true;
}

/************************************************************************/
/*                  ZarrV3CodecBytes::AllocateBuffer()                 */
/************************************************************************/

bool ZarrV3CodecSequence::AllocateBuffer(ZarrByteVectorQuickResize &abyBuffer,
                                         size_t nEltCount)
{
    if (!m_apoCodecs.empty())
    {
        const size_t nRawSize =
            nEltCount * m_oInputArrayMetadata.oElt.nativeSize;
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
    if (!AllocateBuffer(abyBuffer,
                        MultiplyElements(m_oInputArrayMetadata.anBlockSizes)))
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
    if (!AllocateBuffer(abyBuffer,
                        MultiplyElements(m_oInputArrayMetadata.anBlockSizes)))
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
