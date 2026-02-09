/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "sharding_indexed" codec
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Development Seed
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include "cpl_vsi_virtual.h"

#include <algorithm>
#include <cinttypes>
#include <limits>

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/sharding-indexed/index.html

/************************************************************************/
/*                     ZarrV3CodecShardingIndexed()                     */
/************************************************************************/

ZarrV3CodecShardingIndexed::ZarrV3CodecShardingIndexed() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                 ZarrV3CodecShardingIndexed::Clone()                  */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecShardingIndexed::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecShardingIndexed>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}

/************************************************************************/
/*         ZarrV3CodecShardingIndexed::InitFromConfiguration()          */
/************************************************************************/

bool ZarrV3CodecShardingIndexed::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool bEmitWarnings)
{
    if (oInputArrayMetadata.anBlockSizes.empty())
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Codec sharding_indexed: sharding not supported for scalar array");
        return false;
    }

    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;

    if (!configuration.IsValid() ||
        configuration.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Codec sharding_indexed: configuration missing or not an object");
        return false;
    }

    const auto oChunkShape = configuration["chunk_shape"].ToArray();
    if (!oChunkShape.IsValid() ||
        oChunkShape.GetType() != CPLJSONObject::Type::Array)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec sharding_indexed: configuration.chunk_shape missing or "
                 "not an array");
        return false;
    }
    if (static_cast<size_t>(oChunkShape.Size()) !=
        m_oInputArrayMetadata.anBlockSizes.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec sharding_indexed: configuration.chunk_shape should "
                 "have the same shape as the array");
        return false;
    }
    std::vector<size_t> anCountInnerChunks;
    for (int i = 0; i < oChunkShape.Size(); ++i)
    {
        if (oChunkShape[i].GetType() != CPLJSONObject::Type::Integer &&
            oChunkShape[i].GetType() != CPLJSONObject::Type::Long)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec sharding_indexed: configuration.chunk_shape[%d] "
                     "should be an integer",
                     i);
            return false;
        }
        const int64_t nVal = oChunkShape[i].ToLong();
        if (nVal <= 0 ||
            static_cast<uint64_t>(nVal) >
                m_oInputArrayMetadata.anBlockSizes[i] ||
            (m_oInputArrayMetadata.anBlockSizes[i] % nVal) != 0)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Codec sharding_indexed: configuration.chunk_shape[%d]=%" PRId64
                " should be a strictly positive value that is a divisor of "
                "%" PRIu64,
                i, nVal,
                static_cast<uint64_t>(m_oInputArrayMetadata.anBlockSizes[i]));
            return false;
        }
        // The following cast is safe since ZarrArray::ParseChunkSize() has
        // previously validated that m_oInputArrayMetadata.anBlockSizes[i] fits
        // on size_t
        if constexpr (sizeof(size_t) < sizeof(uint64_t))
        {
            // coverity[result_independent_of_operands]
            CPLAssert(nVal <= std::numeric_limits<size_t>::max());
        }
        m_anInnerBlockSize.push_back(static_cast<size_t>(nVal));
        anCountInnerChunks.push_back(
            static_cast<size_t>(m_oInputArrayMetadata.anBlockSizes[i] / nVal));
    }

    const auto oCodecs = configuration["codecs"];
    if (!oCodecs.IsValid() || oCodecs.GetType() != CPLJSONObject::Type::Array)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec sharding_indexed: configuration.codecs missing or "
                 "not an array");
        return false;
    }
    if (oCodecs.ToArray().Size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec sharding_indexed: configuration.codecs[] is empty");
        return false;
    }
    ZarrArrayMetadata inputArrayMetadataCodecs = m_oInputArrayMetadata;
    inputArrayMetadataCodecs.anBlockSizes = m_anInnerBlockSize;
    m_poCodecSequence =
        std::make_unique<ZarrV3CodecSequence>(inputArrayMetadataCodecs);
    if (!m_poCodecSequence->InitFromJson(oCodecs, oOutputArrayMetadata))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec sharding_indexed: initialization of codecs failed");
        return false;
    }

    if (bEmitWarnings && m_poCodecSequence->SupportsPartialDecoding())
    {
        // Implementation limitation
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Nested sharding detected. For now, partial decoding is only "
                 "implemented on the outer-most shard level");
    }

    const auto oIndexCodecs = configuration["index_codecs"];
    if (!oIndexCodecs.IsValid() ||
        oIndexCodecs.GetType() != CPLJSONObject::Type::Array)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Codec sharding_indexed: configuration.index_codecs missing or "
            "not an array");
        return false;
    }
    if (oIndexCodecs.ToArray().Size() == 0)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Codec sharding_indexed: configuration.index_codecs[] is empty");
        return false;
    }
    ZarrArrayMetadata inputArrayMetadataIndex;
    inputArrayMetadataIndex.oElt.nativeType =
        DtypeElt::NativeType::UNSIGNED_INT;
    inputArrayMetadataIndex.oElt.nativeSize = sizeof(uint64_t);
    inputArrayMetadataIndex.oElt.gdalType =
        GDALExtendedDataType::Create(GDT_UInt64);
    inputArrayMetadataIndex.oElt.gdalSize = sizeof(uint64_t);
    inputArrayMetadataIndex.anBlockSizes = std::move(anCountInnerChunks);
    // 2 for offset and size
    inputArrayMetadataIndex.anBlockSizes.push_back(2);
    m_poIndexCodecSequence =
        std::make_unique<ZarrV3CodecSequence>(inputArrayMetadataIndex);
    ZarrArrayMetadata oOutputArrayMetadataIndex;
    if (!m_poIndexCodecSequence->InitFromJson(oIndexCodecs,
                                              oOutputArrayMetadataIndex))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Codec sharding_indexed: initialization of index_codecs failed");
        return false;
    }
    const auto &indexCodecs = m_poIndexCodecSequence->GetCodecs();
    if (indexCodecs.empty())
    {
        // ok, there is only a "bytes" codec, optimized away if the order
        // is the one of the native architecture
    }
    else if (indexCodecs[0]->GetName() == ZarrV3CodecBytes::NAME ||
             indexCodecs[0]->GetName() == ZarrV3CodecCRC32C::NAME)
    {
        // ok
    }
    else if (indexCodecs.size() == 2 &&
             indexCodecs[1]->GetName() == ZarrV3CodecCRC32C::NAME)
    {
        // ok
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Codec sharding_indexed: this implementation only supports "
                 "Bytes, possibly followed by CRC32C, as index_codecs");
        return false;
    }
    m_bIndexHasCRC32 = (!indexCodecs.empty() && indexCodecs.back()->GetName() ==
                                                    ZarrV3CodecCRC32C::NAME);

    const std::string osIndexLocation =
        configuration.GetString("index_location", "end");
    if (osIndexLocation != "start" && osIndexLocation != "end")
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec sharding_indexed: invalid value for index_location");
        return false;
    }
    m_bIndexLocationAtEnd = (osIndexLocation == "end");

    return true;
}

/************************************************************************/
/*                 ZarrV3CodecShardingIndexed::Encode()                 */
/************************************************************************/

bool ZarrV3CodecShardingIndexed::Encode(const ZarrByteVectorQuickResize &,
                                        ZarrByteVectorQuickResize &) const
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "ZarrV3CodecShardingIndexed::Encode() not supported");
    return false;
}

/************************************************************************/
/*                     CopySubArrayIntoLargerOne()                      */
/************************************************************************/

static void
CopySubArrayIntoLargerOne(const ZarrByteVectorQuickResize &abyChunk,
                          const std::vector<size_t> &anInnerBlockSize,
                          const std::vector<size_t> &anInnerBlockIndices,
                          ZarrByteVectorQuickResize &abyDst,
                          const std::vector<size_t> &anDstBlockSize,
                          const size_t nDTSize)
{
    const auto nDims = anInnerBlockSize.size();
    CPLAssert(nDims > 0);
    CPLAssert(nDims == anInnerBlockIndices.size());
    CPLAssert(nDims == anDstBlockSize.size());
    // +1 just to make some gcc versions not emit -Wnull-dereference false positives
    std::vector<GByte *> dstPtrStack(nDims + 1);
    std::vector<size_t> count(nDims + 1);
    std::vector<size_t> dstStride(nDims + 1);

    size_t nDstStride = nDTSize;
    for (size_t iDim = nDims; iDim > 0;)
    {
        --iDim;
        dstStride[iDim] = nDstStride;
        nDstStride *= anDstBlockSize[iDim];
    }

    dstPtrStack[0] = abyDst.data();
    for (size_t iDim = 0; iDim < nDims; ++iDim)
    {
        CPLAssert((anInnerBlockIndices[iDim] + 1) * anInnerBlockSize[iDim] <=
                  anDstBlockSize[iDim]);
        dstPtrStack[0] += anInnerBlockIndices[iDim] * anInnerBlockSize[iDim] *
                          dstStride[iDim];
    }
    const GByte *pabySrc = abyChunk.data();

    const size_t nLastDimSize = anInnerBlockSize.back() * nDTSize;
    size_t dimIdx = 0;
lbl_next_depth:
    if (dimIdx + 1 == nDims)
    {
        memcpy(dstPtrStack[dimIdx], pabySrc, nLastDimSize);
        pabySrc += nLastDimSize;
    }
    else
    {
        count[dimIdx] = anInnerBlockSize[dimIdx];
        while (true)
        {
            dimIdx++;
            dstPtrStack[dimIdx] = dstPtrStack[dimIdx - 1];
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if (--count[dimIdx] == 0)
                break;
            dstPtrStack[dimIdx] += dstStride[dimIdx];
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;
}

/************************************************************************/
/*                           FillWithNoData()                           */
/************************************************************************/

static void FillWithNoData(ZarrByteVectorQuickResize &abyDst,
                           const size_t nCount,
                           const ZarrArrayMetadata &metadata)
{
    const size_t nDTSize = metadata.oElt.nativeSize;
    if (metadata.abyNoData.empty() ||
        metadata.abyNoData == std::vector<GByte>(nDTSize, 0))
    {
        memset(abyDst.data(), 0, nDTSize * nCount);
    }
    else
    {
        CPLAssert(metadata.abyNoData.size() == nDTSize);
        for (size_t i = 0; i < nCount; ++i)
        {
            memcpy(abyDst.data() + i * nDTSize, metadata.abyNoData.data(),
                   nDTSize);
        }
    }
}

/************************************************************************/
/*                 ZarrV3CodecShardingIndexed::Decode()                 */
/************************************************************************/

bool ZarrV3CodecShardingIndexed::Decode(const ZarrByteVectorQuickResize &abySrc,
                                        ZarrByteVectorQuickResize &abyDst) const
{
    size_t nInnerChunks = 1;
    for (size_t i = 0; i < m_anInnerBlockSize.size(); ++i)
    {
        const size_t nCountInnerChunksThisdim =
            m_oInputArrayMetadata.anBlockSizes[i] / m_anInnerBlockSize[i];
        nInnerChunks *= nCountInnerChunksThisdim;
    }

    const size_t nIndexEncodedSize = nInnerChunks * sizeof(Location) +
                                     (m_bIndexHasCRC32 ? sizeof(uint32_t) : 0);
    ZarrByteVectorQuickResize abyIndex;
    if (m_bIndexLocationAtEnd)
    {
        abyIndex.insert(abyIndex.end(),
                        abySrc.begin() + (abySrc.size() - nIndexEncodedSize),
                        abySrc.end());
    }
    else
    {
        abyIndex.insert(abyIndex.end(), abySrc.begin(),
                        abySrc.end() + nIndexEncodedSize);
    }

    if (!m_poIndexCodecSequence->Decode(abyIndex))
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "ZarrV3CodecShardingIndexed::Decode(): cannot decode shard index");
        return false;
    }

    if (abyIndex.size() != nInnerChunks * sizeof(Location))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ZarrV3CodecShardingIndexed::Decode(): shard index has not "
                 "expected size");
        return false;
    }

    const Location *panLocations =
        reinterpret_cast<const Location *>(abyIndex.data());

    ZarrByteVectorQuickResize abyChunk;
    const auto nDTSize = m_oInputArrayMetadata.oElt.nativeSize;
    const size_t nExpectedDecodedChunkSize =
        nDTSize * MultiplyElements(m_anInnerBlockSize);
    const size_t nDstCount =
        MultiplyElements(m_oInputArrayMetadata.anBlockSizes);

    try
    {
        abyDst.resize(nDstCount * nDTSize);
    }
    catch (const std::exception &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for decoded shard");
        return false;
    }

    FillWithNoData(abyDst, nDstCount, m_oInputArrayMetadata);

    std::vector<size_t> anChunkIndices(m_anInnerBlockSize.size(), 0);
    for (size_t iChunk = 0; iChunk < nInnerChunks; ++iChunk)
    {
        if (iChunk > 0)
        {
            // Update chunk coordinates
            size_t iDim = m_anInnerBlockSize.size() - 1;
            while (++anChunkIndices[iDim] ==
                   m_oInputArrayMetadata.anBlockSizes[iDim] /
                       m_anInnerBlockSize[iDim])
            {
                anChunkIndices[iDim] = 0;
                --iDim;
            }
        }

#ifdef DEBUG_VERBOSE
        CPLDebug("ZARR", "Chunk %" PRIu64 ": offset %" PRIu64 ", size %" PRIu64,
                 static_cast<uint64_t>(iChunk), panLocations[iChunk].nOffset,
                 panLocations[iChunk].nSize);
#endif

        if (panLocations[iChunk].nOffset ==
                std::numeric_limits<uint64_t>::max() &&
            panLocations[iChunk].nSize == std::numeric_limits<uint64_t>::max())
        {
            // Empty chunk
            continue;
        }

        if (panLocations[iChunk].nOffset >= abySrc.size() ||
            panLocations[iChunk].nSize >
                abySrc.size() - panLocations[iChunk].nOffset)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "ZarrV3CodecShardingIndexed::Decode(): invalid chunk "
                     "location for chunk %" PRIu64 ": offset=%" PRIu64
                     ", size=%" PRIu64,
                     static_cast<uint64_t>(iChunk),
                     panLocations[iChunk].nOffset, panLocations[iChunk].nSize);
            return false;
        }

        abyChunk.clear();
        abyChunk.insert(
            abyChunk.end(),
            abySrc.begin() + static_cast<size_t>(panLocations[iChunk].nOffset),
            abySrc.begin() + static_cast<size_t>(panLocations[iChunk].nOffset +
                                                 panLocations[iChunk].nSize));
        if (!m_poCodecSequence->Decode(abyChunk))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "ZarrV3CodecShardingIndexed::Decode(): cannot decode "
                     "chunk %" PRIu64,
                     static_cast<uint64_t>(iChunk));
            return false;
        }

        if (abyChunk.size() != nExpectedDecodedChunkSize)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "ZarrV3CodecShardingIndexed::Decode(): decoded size for "
                     "chunk %" PRIu64 " is %" PRIu64 " whereas %" PRIu64
                     " is expected",
                     static_cast<uint64_t>(iChunk),
                     static_cast<uint64_t>(abyChunk.size()),
                     static_cast<uint64_t>(nExpectedDecodedChunkSize));
            return false;
        }

        CopySubArrayIntoLargerOne(abyChunk, m_anInnerBlockSize, anChunkIndices,
                                  abyDst, m_oInputArrayMetadata.anBlockSizes,
                                  nDTSize);
    }

    return true;
}

/************************************************************************/
/*             ZarrV3CodecShardingIndexed::DecodePartial()              */
/************************************************************************/

bool ZarrV3CodecShardingIndexed::DecodePartial(
    VSIVirtualHandle *poFile, const ZarrByteVectorQuickResize & /* abySrc */,
    ZarrByteVectorQuickResize &abyDst, std::vector<size_t> &anStartIdx,
    std::vector<size_t> &anCount)
{
    CPLAssert(anStartIdx.size() == m_oInputArrayMetadata.anBlockSizes.size());
    CPLAssert(anStartIdx.size() == anCount.size());

    size_t nInnerChunkCount = 1;
    size_t nInnerChunkIdx = 0;
    size_t nInnerChunkCountPrevDim = 1;
    for (size_t i = 0; i < anStartIdx.size(); ++i)
    {
        CPLAssert(anStartIdx[i] + anCount[i] <=
                  m_oInputArrayMetadata.anBlockSizes[i]);
        if ((anStartIdx[i] % m_anInnerBlockSize[i]) != 0 ||
            anCount[i] != m_anInnerBlockSize[i])
        {
            // Should not happen with the current call sites.
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ZarrV3CodecShardingIndexed::DecodePartial() only "
                     "supported on an exact inner chunk");
            return false;
        }

        const size_t nCountInnerChunksThisDim =
            m_oInputArrayMetadata.anBlockSizes[i] / m_anInnerBlockSize[i];
        nInnerChunkIdx *= nInnerChunkCountPrevDim;
        nInnerChunkIdx += anStartIdx[i] / m_anInnerBlockSize[i];
        nInnerChunkCount *= nCountInnerChunksThisDim;
        nInnerChunkCountPrevDim = nCountInnerChunksThisDim;
    }

    abyDst.clear();

    const auto nDTSize = m_oInputArrayMetadata.oElt.nativeSize;
    const auto nExpectedDecodedChunkSize = nDTSize * MultiplyElements(anCount);

    vsi_l_offset nLocationOffset =
        static_cast<vsi_l_offset>(nInnerChunkIdx) * sizeof(Location);
    if (m_bIndexLocationAtEnd)
    {
        poFile->Seek(0, SEEK_END);
        const auto nFileSize = poFile->Tell();
        vsi_l_offset nIndexSize =
            static_cast<vsi_l_offset>(nInnerChunkCount) * sizeof(Location);
        if (m_bIndexHasCRC32)
            nIndexSize += sizeof(uint32_t);
        if (nFileSize < nIndexSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ZarrV3CodecShardingIndexed::DecodePartial(): shard file "
                     "too small");
            return false;
        }
        nLocationOffset += nFileSize - nIndexSize;
    }

    Location loc;
    if (poFile->Seek(nLocationOffset, SEEK_SET) != 0 ||
        poFile->Read(&loc, 1, sizeof(loc)) != sizeof(loc))
    {

        CPLError(CE_Failure, CPLE_AppDefined,
                 "ZarrV3CodecShardingIndexed::DecodePartial(): "
                 "cannot read index for chunk %" PRIu64,
                 static_cast<uint64_t>(nInnerChunkIdx));
        return false;
    }

    if (!m_poIndexCodecSequence->GetCodecs().empty() &&
        m_poIndexCodecSequence->GetCodecs().front()->GetName() ==
            ZarrV3CodecBytes::NAME &&
        !m_poIndexCodecSequence->GetCodecs().front()->IsNoOp())
    {
        CPL_SWAP64PTR(&(loc.nOffset));
        CPL_SWAP64PTR(&(loc.nSize));
    }

    if (loc.nOffset == std::numeric_limits<uint64_t>::max() &&
        loc.nSize == std::numeric_limits<uint64_t>::max())
    {
        // Empty chunk
        try
        {
            abyDst.resize(nExpectedDecodedChunkSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate memory for decoded shard");
            return false;
        }
        FillWithNoData(abyDst, MultiplyElements(anCount),
                       m_oInputArrayMetadata);
        return true;
    }

    constexpr size_t THRESHOLD = 10 * 1024 * 1024;
    if (loc.nSize > THRESHOLD)
    {
        // When the chunk size is above a certain threshold, check it against
        // the actual file size to avoid excessive memory allocation attempts.

        poFile->Seek(0, SEEK_END);
        const auto nFileSize = poFile->Tell();

        if (loc.nOffset >= nFileSize || loc.nSize > nFileSize - loc.nOffset)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "ZarrV3CodecShardingIndexed::DecodePartial(): invalid chunk "
                "location for chunk %" PRIu64 ": offset=%" PRIu64
                ", size=%" PRIu64,
                static_cast<uint64_t>(nInnerChunkIdx), loc.nOffset, loc.nSize);
            return false;
        }
    }

    if constexpr (sizeof(size_t) < sizeof(uint64_t))
    {
        // coverity[result_independent_of_operands]
        if (loc.nSize > std::numeric_limits<size_t>::max())
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "ZarrV3CodecShardingIndexed::DecodePartial(): too large chunk "
                "size for chunk %" PRIu64 " for this platform: size=%" PRIu64,
                static_cast<uint64_t>(nInnerChunkIdx), loc.nSize);
            return false;
        }
    }

    try
    {
        abyDst.resize(static_cast<size_t>(loc.nSize));
    }
    catch (const std::exception &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for decoded shard");
        return false;
    }

    if (poFile->Seek(loc.nOffset, SEEK_SET) != 0 ||
        poFile->Read(abyDst.data(), 1, abyDst.size()) != abyDst.size())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ZarrV3CodecShardingIndexed::DecodePartial(): cannot read "
                 "data for chunk %" PRIu64 ": offset=%" PRIu64
                 ", size=%" PRIu64,
                 static_cast<uint64_t>(nInnerChunkIdx), loc.nOffset, loc.nSize);
        return false;
    }

    if (!m_poCodecSequence->Decode(abyDst))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ZarrV3CodecShardingIndexed::DecodePartial(): cannot decode "
                 "chunk %" PRIu64,
                 static_cast<uint64_t>(nInnerChunkIdx));
        return false;
    }

    if (abyDst.size() != nExpectedDecodedChunkSize)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "ZarrV3CodecShardingIndexed::DecodePartial(): decoded size for "
            "chunk %" PRIu64 " is %" PRIu64 " whereas %" PRIu64 " is expected",
            static_cast<uint64_t>(nInnerChunkIdx),
            static_cast<uint64_t>(abyDst.size()),
            static_cast<uint64_t>(nExpectedDecodedChunkSize));
        return false;
    }

    return true;
}

/************************************************************************/
/*           ZarrV3CodecShardingIndexed::BatchDecodePartial()           */
/************************************************************************/

bool ZarrV3CodecShardingIndexed::BatchDecodePartial(
    VSIVirtualHandle *poFile,
    const std::vector<std::pair<std::vector<size_t>, std::vector<size_t>>>
        &anRequests,
    std::vector<ZarrByteVectorQuickResize> &aResults)
{
    if (anRequests.empty())
        return true;

    const auto nDTSize = m_oInputArrayMetadata.oElt.nativeSize;

    // --- Compute inner chunk count and per-request inner chunk indices ---
    size_t nInnerChunkCount = 1;
    for (size_t i = 0; i < m_oInputArrayMetadata.anBlockSizes.size(); ++i)
    {
        nInnerChunkCount *=
            m_oInputArrayMetadata.anBlockSizes[i] / m_anInnerBlockSize[i];
    }

    // Determine whether index codec requires byte-swapping
    const bool bSwapIndex =
        !m_poIndexCodecSequence->GetCodecs().empty() &&
        m_poIndexCodecSequence->GetCodecs().front()->GetName() ==
            ZarrV3CodecBytes::NAME &&
        !m_poIndexCodecSequence->GetCodecs().front()->IsNoOp();

    // Compute index base offset. For index-at-end, we need the file size.
    vsi_l_offset nIndexBaseOffset = 0;
    if (m_bIndexLocationAtEnd)
    {
        poFile->Seek(0, SEEK_END);
        const auto nFileSize = poFile->Tell();
        vsi_l_offset nIndexSize =
            static_cast<vsi_l_offset>(nInnerChunkCount) * sizeof(Location);
        if (m_bIndexHasCRC32)
            nIndexSize += sizeof(uint32_t);
        if (nFileSize < nIndexSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "BatchDecodePartial: shard file too small");
            return false;
        }
        nIndexBaseOffset = nFileSize - nIndexSize;
    }

    // Build per-request inner chunk indices
    std::vector<size_t> anInnerChunkIndices(anRequests.size());
    for (size_t iReq = 0; iReq < anRequests.size(); ++iReq)
    {
        const auto &anStartIdx = anRequests[iReq].first;
        CPLAssert(anStartIdx.size() ==
                  m_oInputArrayMetadata.anBlockSizes.size());

        size_t nInnerChunkIdx = 0;
        size_t nInnerChunkCountPrevDim = 1;
        for (size_t i = 0; i < anStartIdx.size(); ++i)
        {
            nInnerChunkIdx *= nInnerChunkCountPrevDim;
            nInnerChunkIdx += anStartIdx[i] / m_anInnerBlockSize[i];
            nInnerChunkCountPrevDim =
                m_oInputArrayMetadata.anBlockSizes[i] / m_anInnerBlockSize[i];
        }
        anInnerChunkIndices[iReq] = nInnerChunkIdx;
    }

    // --- Pass 1: ReadMultiRange for index entries (16 bytes each) ---
    std::vector<vsi_l_offset> anIdxOffsets(anRequests.size());
    std::vector<size_t> anIdxSizes(anRequests.size(), sizeof(Location));
    std::vector<Location> aLocations(anRequests.size());
    std::vector<void *> ppIdxData(anRequests.size());

    for (size_t i = 0; i < anRequests.size(); ++i)
    {
        anIdxOffsets[i] = nIndexBaseOffset +
                          static_cast<vsi_l_offset>(anInnerChunkIndices[i]) *
                              sizeof(Location);
        ppIdxData[i] = &aLocations[i];
    }

    if (poFile->ReadMultiRange(static_cast<int>(anRequests.size()),
                               ppIdxData.data(), anIdxOffsets.data(),
                               anIdxSizes.data()) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "BatchDecodePartial: ReadMultiRange() failed for index");
        return false;
    }

    // Byte-swap if needed
    if (bSwapIndex)
    {
        for (auto &loc : aLocations)
        {
            CPL_SWAP64PTR(&(loc.nOffset));
            CPL_SWAP64PTR(&(loc.nSize));
        }
    }

    // --- Classify requests: empty chunks vs data chunks ---
    aResults.resize(anRequests.size());

    struct DataRange
    {
        size_t nReqIdx;
    };

    std::vector<DataRange> aDataRanges;
    std::vector<vsi_l_offset> anDataOffsets;
    std::vector<size_t> anDataSizes;

    for (size_t iReq = 0; iReq < anRequests.size(); ++iReq)
    {
        const auto &anCount = anRequests[iReq].second;
        const auto nExpectedDecodedChunkSize =
            nDTSize * MultiplyElements(anCount);
        const Location &loc = aLocations[iReq];

        if (loc.nOffset == std::numeric_limits<uint64_t>::max() &&
            loc.nSize == std::numeric_limits<uint64_t>::max())
        {
            // Empty chunk — fill with nodata
            try
            {
                aResults[iReq].resize(nExpectedDecodedChunkSize);
            }
            catch (const std::exception &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate memory for decoded chunk");
                return false;
            }
            FillWithNoData(aResults[iReq], MultiplyElements(anCount),
                           m_oInputArrayMetadata);
            continue;
        }

        if constexpr (sizeof(size_t) < sizeof(uint64_t))
        {
            if (loc.nSize > std::numeric_limits<size_t>::max())
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "BatchDecodePartial: too large chunk size");
                return false;
            }
        }

        aDataRanges.push_back({iReq});
        anDataOffsets.push_back(loc.nOffset);
        anDataSizes.push_back(static_cast<size_t>(loc.nSize));
    }

    if (aDataRanges.empty())
        return true;

    // Validate against file size (same threshold as DecodePartial)
    constexpr size_t THRESHOLD = 10 * 1024 * 1024;
    {
        size_t nMaxSize = 0;
        for (const auto &s : anDataSizes)
            nMaxSize = std::max(nMaxSize, s);
        if (nMaxSize > THRESHOLD)
        {
            poFile->Seek(0, SEEK_END);
            const auto nFileSize = poFile->Tell();
            for (size_t i = 0; i < aDataRanges.size(); ++i)
            {
                if (anDataOffsets[i] >= nFileSize ||
                    anDataSizes[i] > nFileSize - anDataOffsets[i])
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "BatchDecodePartial: invalid chunk location: "
                             "offset=%" PRIu64 ", size=%" PRIu64,
                             static_cast<uint64_t>(anDataOffsets[i]),
                             static_cast<uint64_t>(anDataSizes[i]));
                    return false;
                }
            }
        }
    }

    // --- Pass 2: ReadMultiRange for data chunks ---
    std::vector<ZarrByteVectorQuickResize> aCompressed(aDataRanges.size());
    std::vector<void *> ppData(aDataRanges.size());

    for (size_t i = 0; i < aDataRanges.size(); ++i)
    {
        try
        {
            aCompressed[i].resize(anDataSizes[i]);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate memory for compressed chunk");
            return false;
        }
        ppData[i] = aCompressed[i].data();
    }

    CPLDebugOnly("ZARR",
                 "BatchDecodePartial: ReadMultiRange() with %d data ranges",
                 static_cast<int>(aDataRanges.size()));

    if (poFile->ReadMultiRange(static_cast<int>(aDataRanges.size()),
                               ppData.data(), anDataOffsets.data(),
                               anDataSizes.data()) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "BatchDecodePartial: ReadMultiRange() failed for data");
        return false;
    }

    // --- Decompress each chunk ---
    for (size_t i = 0; i < aDataRanges.size(); ++i)
    {
        const size_t iReq = aDataRanges[i].nReqIdx;
        const auto &anCount = anRequests[iReq].second;
        const auto nExpectedDecodedChunkSize =
            nDTSize * MultiplyElements(anCount);

        if (!m_poCodecSequence->Decode(aCompressed[i]))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "BatchDecodePartial: cannot decode chunk %" PRIu64,
                     static_cast<uint64_t>(anInnerChunkIndices[iReq]));
            return false;
        }

        if (aCompressed[i].size() != nExpectedDecodedChunkSize)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "BatchDecodePartial: decoded size %" PRIu64
                     " != expected %" PRIu64,
                     static_cast<uint64_t>(aCompressed[i].size()),
                     static_cast<uint64_t>(nExpectedDecodedChunkSize));
            return false;
        }

        aResults[iReq] = std::move(aCompressed[i]);
    }

    return true;
}

/************************************************************************/
/*         ZarrV3CodecShardingIndexed::GetInnerMostBlockSize()          */
/************************************************************************/

std::vector<size_t> ZarrV3CodecShardingIndexed::GetInnerMostBlockSize(
    const std::vector<size_t> &) const
{
    return m_anInnerBlockSize;
    // TODO if we one day properly support nested sharding
    // return m_poCodecSequence->GetInnerMostBlockSize(m_anInnerBlockSize);
}
