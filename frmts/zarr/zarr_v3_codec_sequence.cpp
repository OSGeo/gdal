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
/*                     ZarrV3CodecSequence::Clone()                     */
/************************************************************************/

std::unique_ptr<ZarrV3CodecSequence> ZarrV3CodecSequence::Clone() const
{
    auto poClone = std::make_unique<ZarrV3CodecSequence>(m_oInputArrayMetadata);
    for (const auto &poCodec : m_apoCodecs)
        poClone->m_apoCodecs.emplace_back(poCodec->Clone());
    poClone->m_oCodecArray = m_oCodecArray.Clone();
    poClone->m_bPartialDecodingPossible = m_bPartialDecodingPossible;
    return poClone;
}

/************************************************************************/
/*                 ZarrV3CodecSequence::InitFromJson()                  */
/************************************************************************/

bool ZarrV3CodecSequence::InitFromJson(const CPLJSONObject &oCodecs,
                                       ZarrArrayMetadata &oOutputArrayMetadata)
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

    bool bShardingFound = false;
    std::vector<size_t> anBlockSizesBeforeSharding;
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
        else if (osName == ZarrV3CodecCRC32C::NAME)
            poCodec = std::make_unique<ZarrV3CodecCRC32C>();
        else if (osName == ZarrV3CodecShardingIndexed::NAME)
        {
            bShardingFound = true;
            poCodec = std::make_unique<ZarrV3CodecShardingIndexed>();
        }
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
        if (osName == ZarrV3CodecShardingIndexed::NAME)
        {
            anBlockSizesBeforeSharding = oInputArrayMetadata.anBlockSizes;
        }
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

    if (bShardingFound)
    {
        m_bPartialDecodingPossible =
            (m_apoCodecs.back()->GetName() == ZarrV3CodecShardingIndexed::NAME);
        if (!m_bPartialDecodingPossible)
        {
            m_bPartialDecodingPossible = false;
            // This is not an implementation limitation, but the result of a
            // badly thought dataset. Zarr-Python also emits a similar warning.
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Sharding codec found, but not in last position. Consequently "
                "partial shard decoding will not be possible");
            oInputArrayMetadata.anBlockSizes =
                std::move(anBlockSizesBeforeSharding);
        }
    }

    InsertImplicitEndianCodecIfNeeded();

    m_oCodecArray = oCodecs.Clone();
    oOutputArrayMetadata = std::move(oInputArrayMetadata);
    return true;
}

/************************************************************************/
/*                  ZarrV3CodecBytes::AllocateBuffer()                  */
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

/************************************************************************/
/*                 ZarrV3CodecSequence::DecodePartial()                 */
/************************************************************************/

bool ZarrV3CodecSequence::DecodePartial(VSIVirtualHandle *poFile,
                                        ZarrByteVectorQuickResize &abyBuffer,
                                        const std::vector<size_t> &anStartIdxIn,
                                        const std::vector<size_t> &anCountIn)
{
    CPLAssert(anStartIdxIn.size() == m_oInputArrayMetadata.anBlockSizes.size());
    CPLAssert(anStartIdxIn.size() == anCountIn.size());

    if (!AllocateBuffer(abyBuffer, MultiplyElements(anCountIn)))
        return false;

    // anStartIdxIn and anCountIn are expressed in the shape *before* encoding
    // We need to apply the potential transpositions before submitting them
    // to the decoder of the Array->Bytes decoder
    std::vector<size_t> anStartIdx(anStartIdxIn);
    std::vector<size_t> anCount(anCountIn);
    for (auto &poCodec : m_apoCodecs)
    {
        poCodec->ChangeArrayShapeForward(anStartIdx, anCount);
    }

    for (auto iter = m_apoCodecs.rbegin(); iter != m_apoCodecs.rend(); ++iter)
    {
        const auto &poCodec = *iter;

        if (!poCodec->DecodePartial(poFile, abyBuffer, m_abyTmp, anStartIdx,
                                    anCount))
            return false;
        std::swap(abyBuffer, m_abyTmp);
    }
    return true;
}

/************************************************************************/
/*              ZarrV3CodecSequence::BatchDecodePartial()               */
/************************************************************************/

bool ZarrV3CodecSequence::BatchDecodePartial(
    VSIVirtualHandle *poFile,
    const std::vector<std::pair<std::vector<size_t>, std::vector<size_t>>>
        &anRequests,
    std::vector<ZarrByteVectorQuickResize> &aResults)
{
    // Only batch-decode when sharding is the sole codec. If other codecs
    // (e.g. transpose) precede it, indices and output need codec-specific
    // transformations that BatchDecodePartial does not handle.
    if (m_apoCodecs.size() == 1)
    {
        auto *poSharding = dynamic_cast<ZarrV3CodecShardingIndexed *>(
            m_apoCodecs.back().get());
        if (poSharding)
        {
            return poSharding->BatchDecodePartial(poFile, anRequests, aResults);
        }
    }

    // Fallback: sequential DecodePartial for non-sharding codec chains
    aResults.resize(anRequests.size());
    for (size_t i = 0; i < anRequests.size(); ++i)
    {
        if (!DecodePartial(poFile, aResults[i], anRequests[i].first,
                           anRequests[i].second))
            return false;
    }
    return true;
}

/************************************************************************/
/*             ZarrV3CodecSequence::GetInnerMostBlockSize()             */
/************************************************************************/

std::vector<size_t> ZarrV3CodecSequence::GetInnerMostBlockSize(
    const std::vector<size_t> &anOuterBlockSize) const
{
    auto chunkSize = anOuterBlockSize;
    for (auto iter = m_apoCodecs.rbegin(); iter != m_apoCodecs.rend(); ++iter)
    {
        const auto &poCodec = *iter;
        if (m_bPartialDecodingPossible ||
            poCodec->GetName() != ZarrV3CodecShardingIndexed::NAME)
        {
            chunkSize = poCodec->GetInnerMostBlockSize(chunkSize);
        }
    }
    return chunkSize;
}
