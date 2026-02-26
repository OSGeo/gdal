/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_enumerate.h"
#include "cpl_float.h"
#include "cpl_vsi_virtual.h"
#include "cpl_worker_thread_pool.h"
#include "gdal_thread_pool.h"
#include "zarr.h"
#include "zarr_v3_codec.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

/************************************************************************/
/*                      ZarrV3Array::ZarrV3Array()                      */
/************************************************************************/

ZarrV3Array::ZarrV3Array(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::shared_ptr<ZarrGroupBase> &poParent, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const GDALExtendedDataType &oType, const std::vector<DtypeElt> &aoDtypeElts,
    const std::vector<GUInt64> &anOuterBlockSize,
    const std::vector<GUInt64> &anInnerBlockSize)
    : GDALAbstractMDArray(poParent->GetFullName(), osName),
      ZarrArray(poSharedResource, poParent, osName, aoDims, oType, aoDtypeElts,
                anOuterBlockSize, anInnerBlockSize)
{
}

/************************************************************************/
/*                        ZarrV3Array::Create()                         */
/************************************************************************/

std::shared_ptr<ZarrV3Array> ZarrV3Array::Create(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::shared_ptr<ZarrGroupBase> &poParent, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const GDALExtendedDataType &oType, const std::vector<DtypeElt> &aoDtypeElts,
    const std::vector<GUInt64> &anOuterBlockSize,
    const std::vector<GUInt64> &anInnerBlockSize)
{
    auto arr = std::shared_ptr<ZarrV3Array>(
        new ZarrV3Array(poSharedResource, poParent, osName, aoDims, oType,
                        aoDtypeElts, anOuterBlockSize, anInnerBlockSize));
    if (arr->m_nTotalInnerChunkCount == 0)
        return nullptr;
    arr->SetSelf(arr);

    return arr;
}

/************************************************************************/
/*                            ~ZarrV3Array()                            */
/************************************************************************/

ZarrV3Array::~ZarrV3Array()
{
    ZarrV3Array::Flush();
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

bool ZarrV3Array::Flush()
{
    if (!m_bValid)
        return true;

    // Flush last dirty block (may add to shard write cache)
    bool ret = ZarrV3Array::FlushDirtyBlock();

    // Encode and write all cached shards
    if (!ZarrV3Array::FlushShardCache())
        ret = false;

    m_anCachedBlockIndices.clear();

    if (!m_aoDims.empty())
    {
        for (const auto &poDim : m_aoDims)
        {
            const auto poZarrDim =
                dynamic_cast<const ZarrDimension *>(poDim.get());
            if (poZarrDim && poZarrDim->IsXArrayDimension())
            {
                if (poZarrDim->IsModified())
                    m_bDefinitionModified = true;
            }
            else
            {
                break;
            }
        }
    }

    CPLJSONObject oAttrs;
    if (m_oAttrGroup.IsModified() || m_bUnitModified || m_bOffsetModified ||
        m_bScaleModified || m_bSRSModified)
    {
        m_bNew = false;

        oAttrs = SerializeSpecialAttributes();

        m_bDefinitionModified = true;
    }

    if (m_bDefinitionModified)
    {
        if (!Serialize(oAttrs))
            ret = false;
        m_bDefinitionModified = false;
    }

    return ret;
}

/************************************************************************/
/*                       ZarrV3Array::Serialize()                       */
/************************************************************************/

bool ZarrV3Array::Serialize(const CPLJSONObject &oAttrs)
{
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot = oDoc.GetRoot();

    oRoot.Add("zarr_format", 3);
    oRoot.Add("node_type", "array");

    CPLJSONArray oShape;
    for (const auto &poDim : m_aoDims)
    {
        oShape.Add(static_cast<GInt64>(poDim->GetSize()));
    }
    oRoot.Add("shape", oShape);

    oRoot.Add("data_type", m_dtype.ToString());

    {
        CPLJSONObject oChunkGrid;
        oRoot.Add("chunk_grid", oChunkGrid);
        oChunkGrid.Add("name", "regular");
        CPLJSONObject oConfiguration;
        oChunkGrid.Add("configuration", oConfiguration);
        CPLJSONArray oChunks;
        for (const auto nBlockSize : m_anOuterBlockSize)
        {
            oChunks.Add(static_cast<GInt64>(nBlockSize));
        }
        oConfiguration.Add("chunk_shape", oChunks);
    }

    {
        CPLJSONObject oChunkKeyEncoding;
        oRoot.Add("chunk_key_encoding", oChunkKeyEncoding);
        oChunkKeyEncoding.Add("name", m_bV2ChunkKeyEncoding ? "v2" : "default");
        CPLJSONObject oConfiguration;
        oChunkKeyEncoding.Add("configuration", oConfiguration);
        oConfiguration.Add("separator", m_osDimSeparator);
    }

    if (m_pabyNoData == nullptr)
    {
        if (m_oType.GetNumericDataType() == GDT_Float16 ||
            m_oType.GetNumericDataType() == GDT_Float32 ||
            m_oType.GetNumericDataType() == GDT_Float64)
        {
            oRoot.Add("fill_value", "NaN");
        }
        else
        {
            oRoot.AddNull("fill_value");
        }
    }
    else
    {
        if (m_oType.GetNumericDataType() == GDT_CFloat16 ||
            m_oType.GetNumericDataType() == GDT_CFloat32 ||
            m_oType.GetNumericDataType() == GDT_CFloat64)
        {
            double adfNoDataValue[2];
            GDALCopyWords(m_pabyNoData, m_oType.GetNumericDataType(), 0,
                          adfNoDataValue, GDT_CFloat64, 0, 1);
            CPLJSONArray oArray;
            for (int i = 0; i < 2; ++i)
            {
                if (std::isnan(adfNoDataValue[i]))
                    oArray.Add("NaN");
                else if (adfNoDataValue[i] ==
                         std::numeric_limits<double>::infinity())
                    oArray.Add("Infinity");
                else if (adfNoDataValue[i] ==
                         -std::numeric_limits<double>::infinity())
                    oArray.Add("-Infinity");
                else
                    oArray.Add(adfNoDataValue[i]);
            }
            oRoot.Add("fill_value", oArray);
        }
        else
        {
            SerializeNumericNoData(oRoot);
        }
    }

    if (m_poCodecs)
    {
        oRoot.Add("codecs", m_poCodecs->GetJSon());
    }

    oRoot.Add("attributes", oAttrs);

    // Set dimension_names
    if (!m_aoDims.empty())
    {
        CPLJSONArray oDimensions;
        for (const auto &poDim : m_aoDims)
        {
            const auto poZarrDim =
                dynamic_cast<const ZarrDimension *>(poDim.get());
            if (poZarrDim && poZarrDim->IsXArrayDimension())
            {
                oDimensions.Add(poDim->GetName());
            }
            else
            {
                oDimensions = CPLJSONArray();
                break;
            }
        }
        if (oDimensions.Size() > 0)
        {
            oRoot.Add("dimension_names", oDimensions);
        }
    }

    // TODO: codecs

    const bool bRet = oDoc.Save(m_osFilename);
    if (bRet)
        m_poSharedResource->SetZMetadataItem(m_osFilename, oRoot);
    return bRet;
}

/************************************************************************/
/*                   ZarrV3Array::NeedDecodedBuffer()                   */
/************************************************************************/

bool ZarrV3Array::NeedDecodedBuffer() const
{
    for (const auto &elt : m_aoDtypeElts)
    {
        if (elt.needByteSwapping)
        {
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                ZarrV3Array::AllocateWorkingBuffers()                 */
/************************************************************************/

bool ZarrV3Array::AllocateWorkingBuffers() const
{
    if (m_bAllocateWorkingBuffersDone)
        return m_bWorkingBuffersOK;

    m_bAllocateWorkingBuffersDone = true;

    size_t nSizeNeeded = m_nInnerBlockSizeBytes;
    if (NeedDecodedBuffer())
    {
        size_t nDecodedBufferSize = m_oType.GetSize();
        for (const auto &nBlockSize : m_anInnerBlockSize)
        {
            if (nDecodedBufferSize > std::numeric_limits<size_t>::max() /
                                         static_cast<size_t>(nBlockSize))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too large chunk size");
                return false;
            }
            nDecodedBufferSize *= static_cast<size_t>(nBlockSize);
        }
        if (nSizeNeeded >
            std::numeric_limits<size_t>::max() - nDecodedBufferSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large chunk size");
            return false;
        }
        nSizeNeeded += nDecodedBufferSize;
    }

    // Reserve a buffer for tile content
    if (nSizeNeeded > 1024 * 1024 * 1024 &&
        !CPLTestBool(CPLGetConfigOption("ZARR_ALLOW_BIG_TILE_SIZE", "NO")))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Zarr tile allocation would require " CPL_FRMT_GUIB " bytes. "
                 "By default the driver limits to 1 GB. To allow that memory "
                 "allocation, set the ZARR_ALLOW_BIG_TILE_SIZE configuration "
                 "option to YES.",
                 static_cast<GUIntBig>(nSizeNeeded));
        return false;
    }

    m_bWorkingBuffersOK =
        AllocateWorkingBuffers(m_abyRawBlockData, m_abyDecodedBlockData);
    return m_bWorkingBuffersOK;
}

bool ZarrV3Array::AllocateWorkingBuffers(
    ZarrByteVectorQuickResize &abyRawBlockData,
    ZarrByteVectorQuickResize &abyDecodedBlockData) const
{
    // This method should NOT modify any ZarrArray member, as it is going to
    // be called concurrently from several threads.

    // Set those #define to avoid accidental use of some global variables
#define m_abyRawBlockData cannot_use_here
#define m_abyDecodedBlockData cannot_use_here

    const size_t nSizeNeeded = m_nInnerBlockSizeBytes;
    try
    {
        abyRawBlockData.resize(nSizeNeeded);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    if (NeedDecodedBuffer())
    {
        size_t nDecodedBufferSize = m_oType.GetSize();
        for (const auto &nBlockSize : m_anInnerBlockSize)
        {
            nDecodedBufferSize *= static_cast<size_t>(nBlockSize);
        }
        try
        {
            abyDecodedBlockData.resize(nDecodedBufferSize);
        }
        catch (const std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return false;
        }
    }

    return true;
#undef m_abyRawBlockData
#undef m_abyDecodedBlockData
}

/************************************************************************/
/*                     ZarrV3Array::LoadBlockData()                     */
/************************************************************************/

bool ZarrV3Array::LoadBlockData(const uint64_t *blockIndices,
                                bool &bMissingBlockOut) const
{
    return LoadBlockData(blockIndices,
                         false,  // use mutex
                         m_poCodecs.get(), m_abyRawBlockData,
                         m_abyDecodedBlockData, bMissingBlockOut);
}

bool ZarrV3Array::LoadBlockData(const uint64_t *blockIndices, bool bUseMutex,
                                ZarrV3CodecSequence *poCodecs,
                                ZarrByteVectorQuickResize &abyRawBlockData,
                                ZarrByteVectorQuickResize &abyDecodedBlockData,
                                bool &bMissingBlockOut) const
{
    // This method should NOT modify any ZarrArray member, as it is going to
    // be called concurrently from several threads.

    // Set those #define to avoid accidental use of some global variables
#define m_abyRawBlockData cannot_use_here
#define m_abyDecodedBlockData cannot_use_here
#define m_poCodecs cannot_use_here

    bMissingBlockOut = false;

    std::string osFilename;
    if (poCodecs && poCodecs->SupportsPartialDecoding())
    {
        std::vector<uint64_t> outerChunkIndices;
        for (size_t i = 0; i < GetDimensionCount(); ++i)
        {
            // Note: m_anOuterBlockSize[i]/m_anInnerBlockSize[i] is an integer
            outerChunkIndices.push_back(blockIndices[i] *
                                        m_anInnerBlockSize[i] /
                                        m_anOuterBlockSize[i]);
        }

        osFilename = BuildChunkFilename(outerChunkIndices.data());
    }
    else
    {
        osFilename = BuildChunkFilename(blockIndices);
    }

    // For network file systems, get the streaming version of the filename,
    // as we don't need arbitrary seeking in the file
    // ... unless we do partial decoding, in which case range requests within
    // a shard are much more efficient
    if (!(poCodecs && poCodecs->SupportsPartialDecoding()))
    {
        osFilename = VSIFileManager::GetHandler(osFilename.c_str())
                         ->GetStreamingFilename(osFilename);
    }

    // First if we have a tile presence cache, check tile presence from it
    bool bEarlyRet;
    if (bUseMutex)
    {
        std::lock_guard<std::mutex> oLock(m_oMutex);
        bEarlyRet = IsBlockMissingFromCacheInfo(osFilename, blockIndices);
    }
    else
    {
        bEarlyRet = IsBlockMissingFromCacheInfo(osFilename, blockIndices);
    }
    if (bEarlyRet)
    {
        bMissingBlockOut = true;
        return true;
    }
    VSIVirtualHandleUniquePtr fp;
    // This is the number of files returned in a S3 directory listing operation
    constexpr uint64_t MAX_TILES_ALLOWED_FOR_DIRECTORY_LISTING = 1000;
    const char *const apszOpenOptions[] = {"IGNORE_FILENAME_RESTRICTIONS=YES",
                                           nullptr};
    const auto nErrorBefore = CPLGetErrorCounter();
    if ((m_osDimSeparator == "/" && !m_anOuterBlockSize.empty() &&
         m_anOuterBlockSize.back() > MAX_TILES_ALLOWED_FOR_DIRECTORY_LISTING) ||
        (m_osDimSeparator != "/" &&
         m_nTotalInnerChunkCount > MAX_TILES_ALLOWED_FOR_DIRECTORY_LISTING))
    {
        // Avoid issuing ReadDir() when a lot of files are expected
        CPLConfigOptionSetter optionSetter("GDAL_DISABLE_READDIR_ON_OPEN",
                                           "YES", true);
        fp.reset(VSIFOpenEx2L(osFilename.c_str(), "rb", 0, apszOpenOptions));
    }
    else
    {
        fp.reset(VSIFOpenEx2L(osFilename.c_str(), "rb", 0, apszOpenOptions));
    }
    if (fp == nullptr)
    {
        if (nErrorBefore != CPLGetErrorCounter())
        {
            return false;
        }
        else
        {
            // Missing files are OK and indicate nodata_value
            CPLDebugOnly(ZARR_DEBUG_KEY, "Block %s missing (=nodata)",
                         osFilename.c_str());
            bMissingBlockOut = true;
            return true;
        }
    }

    bMissingBlockOut = false;

    if (poCodecs && poCodecs->SupportsPartialDecoding())
    {
        std::vector<size_t> anStartIdx;
        std::vector<size_t> anCount;
        for (size_t i = 0; i < GetDimensionCount(); ++i)
        {
            anStartIdx.push_back(
                static_cast<size_t>((blockIndices[i] * m_anInnerBlockSize[i]) %
                                    m_anOuterBlockSize[i]));
            anCount.push_back(static_cast<size_t>(m_anInnerBlockSize[i]));
        }
        if (!poCodecs->DecodePartial(fp.get(), abyRawBlockData, anStartIdx,
                                     anCount))
            return false;
    }
    else
    {
        CPLAssert(abyRawBlockData.capacity() >= m_nInnerBlockSizeBytes);
        // should not fail
        abyRawBlockData.resize(m_nInnerBlockSizeBytes);

        bool bRet = true;
        size_t nRawDataSize = abyRawBlockData.size();
        if (poCodecs == nullptr)
        {
            nRawDataSize = fp->Read(&abyRawBlockData[0], 1, nRawDataSize);
        }
        else
        {
            fp->Seek(0, SEEK_END);
            const auto nSize = fp->Tell();
            fp->Seek(0, SEEK_SET);
            if (nSize >
                static_cast<vsi_l_offset>(std::numeric_limits<int>::max()))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too large tile %s",
                         osFilename.c_str());
                bRet = false;
            }
            else
            {
                try
                {
                    abyRawBlockData.resize(static_cast<size_t>(nSize));
                }
                catch (const std::exception &)
                {
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Cannot allocate memory for tile %s",
                             osFilename.c_str());
                    bRet = false;
                }

                if (bRet &&
                    (abyRawBlockData.empty() ||
                     fp->Read(&abyRawBlockData[0], 1, abyRawBlockData.size()) !=
                         abyRawBlockData.size()))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Could not read tile %s correctly",
                             osFilename.c_str());
                    bRet = false;
                }
                else
                {
                    if (!poCodecs->Decode(abyRawBlockData))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Decompression of tile %s failed",
                                 osFilename.c_str());
                        bRet = false;
                    }
                }
            }
        }
        if (!bRet)
            return false;

        if (nRawDataSize != abyRawBlockData.size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Decompressed tile %s has not expected size. "
                     "Got %u instead of %u",
                     osFilename.c_str(),
                     static_cast<unsigned>(abyRawBlockData.size()),
                     static_cast<unsigned>(nRawDataSize));
            return false;
        }
    }

    if (!abyDecodedBlockData.empty())
    {
        const size_t nSourceSize =
            m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;
        const auto nDTSize = m_oType.GetSize();
        const size_t nValues = abyDecodedBlockData.size() / nDTSize;
        CPLAssert(nValues == m_nInnerBlockSizeBytes / nSourceSize);
        const GByte *pSrc = abyRawBlockData.data();
        GByte *pDst = &abyDecodedBlockData[0];
        for (size_t i = 0; i < nValues;
             i++, pSrc += nSourceSize, pDst += nDTSize)
        {
            DecodeSourceElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    return true;

#undef m_abyRawBlockData
#undef m_abyDecodedBlockData
#undef m_poCodecs
}

/************************************************************************/
/*                         ZarrV3Array::IRead()                         */
/************************************************************************/

bool ZarrV3Array::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                        const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                        const GDALExtendedDataType &bufferDataType,
                        void *pDstBuffer) const
{
    // For sharded arrays, pre-populate the block cache via ReadMultiRange()
    // so that the base-class block-by-block loop hits memory, not HTTP.
    if (m_poCodecs && m_poCodecs->SupportsPartialDecoding())
    {
        PreloadShardedBlocks(arrayStartIdx, count);
    }
    return ZarrArray::IRead(arrayStartIdx, count, arrayStep, bufferStride,
                            bufferDataType, pDstBuffer);
}

/************************************************************************/
/*                 ZarrV3Array::PreloadShardedBlocks()                  */
/************************************************************************/

void ZarrV3Array::PreloadShardedBlocks(const GUInt64 *arrayStartIdx,
                                       const size_t *count) const
{
    const size_t nDims = m_aoDims.size();
    if (nDims == 0)
        return;

    // Calculate needed block index range
    std::vector<uint64_t> anBlockMin(nDims), anBlockMax(nDims);
    size_t nTotalBlocks = 1;
    for (size_t i = 0; i < nDims; ++i)
    {
        anBlockMin[i] = arrayStartIdx[i] / m_anInnerBlockSize[i];
        anBlockMax[i] =
            (arrayStartIdx[i] + count[i] - 1) / m_anInnerBlockSize[i];
        nTotalBlocks *= static_cast<size_t>(anBlockMax[i] - anBlockMin[i] + 1);
    }

    if (nTotalBlocks <= 1)
        return;  // single block — no batching benefit

    CPLDebugOnly("ZARR", "PreloadShardedBlocks: %" PRIu64 " blocks to batch",
                 static_cast<uint64_t>(nTotalBlocks));

    // Enumerate all needed blocks, grouped by shard filename
    struct BlockInfo
    {
        std::vector<uint64_t> anBlockIndices{};
        std::vector<size_t> anStartIdx{};
        std::vector<size_t> anCount{};
    };

    std::map<std::string, std::vector<BlockInfo>> oShardToBlocks;

    // Iterate over all needed block indices
    std::vector<uint64_t> anCur(nDims);
    size_t dimIdx = 0;
lbl_next:
    if (dimIdx == nDims)
    {
        // Skip blocks already in cache
        const std::vector<uint64_t> cacheKey(anCur.begin(), anCur.end());
        if (m_oChunkCache.find(cacheKey) == m_oChunkCache.end())
        {
            // Compute shard filename and inner chunk start/count
            std::vector<uint64_t> outerIdx(nDims);
            BlockInfo info;
            info.anBlockIndices = anCur;
            info.anStartIdx.resize(nDims);
            info.anCount.resize(nDims);
            for (size_t i = 0; i < nDims; ++i)
            {
                outerIdx[i] =
                    anCur[i] * m_anInnerBlockSize[i] / m_anOuterBlockSize[i];
                info.anStartIdx[i] = static_cast<size_t>(
                    (anCur[i] * m_anInnerBlockSize[i]) % m_anOuterBlockSize[i]);
                info.anCount[i] = static_cast<size_t>(m_anInnerBlockSize[i]);
            }

            std::string osFilename = BuildChunkFilename(outerIdx.data());
            oShardToBlocks[osFilename].push_back(std::move(info));
        }
    }
    else
    {
        anCur[dimIdx] = anBlockMin[dimIdx];
        while (true)
        {
            dimIdx++;
            goto lbl_next;
        lbl_return:
            dimIdx--;
            if (anCur[dimIdx] == anBlockMax[dimIdx])
                break;
            ++anCur[dimIdx];
        }
    }
    if (dimIdx > 0)
        goto lbl_return;

    // Collect shards that qualify for batching (>1 block)
    struct ShardWork
    {
        const std::string *posFilename;
        std::vector<BlockInfo> *paBlocks;
    };

    std::vector<ShardWork> aShardWork;
    for (auto &[osFilename, aBlocks] : oShardToBlocks)
    {
        if (aBlocks.size() > 1)
            aShardWork.push_back({&osFilename, &aBlocks});
    }

    if (aShardWork.empty())
        return;

    const char *const apszOpenOptions[] = {"IGNORE_FILENAME_RESTRICTIONS=YES",
                                           nullptr};
    const bool bNeedDecode = NeedDecodedBuffer();

    // Process one shard: open file, batch-decode, type-convert, cache.
    // poCodecs: per-thread clone (parallel) or m_poCodecs (sequential).
    // oMutex: guards cache writes (uncontended in sequential path).
    const auto ProcessOneShard =
        [this, &apszOpenOptions, bNeedDecode](const ShardWork &work,
                                              ZarrV3CodecSequence *poCodecs,
                                              std::mutex &oMutex)
    {
        VSIVirtualHandleUniquePtr fp(
            VSIFOpenEx2L(work.posFilename->c_str(), "rb", 0, apszOpenOptions));
        if (!fp)
            return;

        const auto &aBlocks = *work.paBlocks;
        std::vector<std::pair<std::vector<size_t>, std::vector<size_t>>>
            anRequests;
        anRequests.reserve(aBlocks.size());
        for (const auto &info : aBlocks)
            anRequests.push_back({info.anStartIdx, info.anCount});

        std::vector<ZarrByteVectorQuickResize> aResults;
        if (!poCodecs->BatchDecodePartial(fp.get(), anRequests, aResults))
            return;

        // Type-convert outside mutex (CPU-bound, thread-local data)
        std::vector<ZarrByteVectorQuickResize> aDecoded;
        if (bNeedDecode)
        {
            const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                                       m_aoDtypeElts.back().nativeSize;
            const auto nGDALDTSize = m_oType.GetSize();
            aDecoded.resize(aBlocks.size());
            for (size_t i = 0; i < aBlocks.size(); ++i)
            {
                if (aResults[i].empty())
                    continue;
                const size_t nValues = aResults[i].size() / nSourceSize;
                aDecoded[i].resize(nValues * nGDALDTSize);
                const GByte *pSrc = aResults[i].data();
                GByte *pDst = aDecoded[i].data();
                for (size_t v = 0; v < nValues;
                     v++, pSrc += nSourceSize, pDst += nGDALDTSize)
                {
                    DecodeSourceElt(m_aoDtypeElts, pSrc, pDst);
                }
            }
        }

        // Store in cache under mutex
        std::lock_guard<std::mutex> oLock(oMutex);
        for (size_t i = 0; i < aBlocks.size(); ++i)
        {
            CachedBlock cachedBlock;
            if (!aResults[i].empty())
            {
                if (bNeedDecode)
                    std::swap(cachedBlock.abyDecoded, aDecoded[i]);
                else
                    std::swap(cachedBlock.abyDecoded, aResults[i]);
            }
            m_oChunkCache[aBlocks[i].anBlockIndices] = std::move(cachedBlock);
        }
    };

    const int nMaxThreads = GDALGetNumThreads();

    const int nShards = static_cast<int>(aShardWork.size());
    std::mutex oMutex;

    // Sequential: single thread, single shard, or no thread pool
    CPLWorkerThreadPool *wtp = (nMaxThreads > 1 && nShards > 1)
                                   ? GDALGetGlobalThreadPool(nMaxThreads)
                                   : nullptr;
    if (!wtp)
    {
        for (const auto &work : aShardWork)
            ProcessOneShard(work, m_poCodecs.get(), oMutex);
        return;
    }

    CPLDebugOnly("ZARR",
                 "PreloadShardedBlocks: parallel across %d shards (%d threads)",
                 nShards, std::min(nMaxThreads, nShards));

    // Clone codecs upfront on main thread (Clone is not thread-safe)
    std::vector<std::unique_ptr<ZarrV3CodecSequence>> apoCodecs(nShards);
    for (int i = 0; i < nShards; ++i)
        apoCodecs[i] = m_poCodecs->Clone();

    auto poQueue = wtp->CreateJobQueue();
    for (int i = 0; i < nShards; ++i)
    {
        poQueue->SubmitJob([&work = aShardWork[i], pCodecs = apoCodecs[i].get(),
                            &oMutex, &ProcessOneShard]()
                           { ProcessOneShard(work, pCodecs, oMutex); });
    }
    poQueue->WaitCompletion();
}

/************************************************************************/
/*                      ZarrV3Array::IAdviseRead()                      */
/************************************************************************/

bool ZarrV3Array::IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                              CSLConstList papszOptions) const
{
    // For sharded arrays, batch all needed inner chunks via
    // PreloadShardedBlocks (BatchDecodePartial + ReadMultiRange) instead
    // of the per-block LoadBlockData path below.
    if (m_poCodecs && m_poCodecs->SupportsPartialDecoding())
    {
        PreloadShardedBlocks(arrayStartIdx, count);
        return true;
    }

    std::vector<uint64_t> anIndicesCur;
    int nThreadsMax = 0;
    std::vector<uint64_t> anReqBlocksIndices;
    size_t nReqBlocks = 0;
    if (!IAdviseReadCommon(arrayStartIdx, count, papszOptions, anIndicesCur,
                           nThreadsMax, anReqBlocksIndices, nReqBlocks))
    {
        return false;
    }
    if (nThreadsMax <= 1)
    {
        return true;
    }

    const int nThreads = static_cast<int>(
        std::min(static_cast<size_t>(nThreadsMax), nReqBlocks));

    CPLWorkerThreadPool *wtp = GDALGetGlobalThreadPool(nThreadsMax);
    if (wtp == nullptr)
        return false;

    struct JobStruct
    {
        JobStruct() = default;

        JobStruct(const JobStruct &) = delete;
        JobStruct &operator=(const JobStruct &) = delete;

        JobStruct(JobStruct &&) = default;
        JobStruct &operator=(JobStruct &&) = default;

        const ZarrV3Array *poArray = nullptr;
        bool *pbGlobalStatus = nullptr;
        int *pnRemainingThreads = nullptr;
        const std::vector<uint64_t> *panReqBlocksIndices = nullptr;
        size_t nFirstIdx = 0;
        size_t nLastIdxNotIncluded = 0;
    };

    std::vector<JobStruct> asJobStructs;

    bool bGlobalStatus = true;
    int nRemainingThreads = nThreads;
    // Check for very highly overflow in below loop
    assert(static_cast<size_t>(nThreads) <
           std::numeric_limits<size_t>::max() / nReqBlocks);

    // Setup jobs
    for (int i = 0; i < nThreads; i++)
    {
        JobStruct jobStruct;
        jobStruct.poArray = this;
        jobStruct.pbGlobalStatus = &bGlobalStatus;
        jobStruct.pnRemainingThreads = &nRemainingThreads;
        jobStruct.panReqBlocksIndices = &anReqBlocksIndices;
        jobStruct.nFirstIdx = static_cast<size_t>(i * nReqBlocks / nThreads);
        jobStruct.nLastIdxNotIncluded = std::min(
            static_cast<size_t>((i + 1) * nReqBlocks / nThreads), nReqBlocks);
        asJobStructs.emplace_back(std::move(jobStruct));
    }

    const auto JobFunc = [](void *pThreadData)
    {
        const JobStruct *jobStruct =
            static_cast<const JobStruct *>(pThreadData);

        const auto poArray = jobStruct->poArray;
        const size_t l_nDims = poArray->GetDimensionCount();
        ZarrByteVectorQuickResize abyRawBlockData;
        ZarrByteVectorQuickResize abyDecodedBlockData;
        std::unique_ptr<ZarrV3CodecSequence> poCodecs;
        if (poArray->m_poCodecs)
        {
            std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
            poCodecs = poArray->m_poCodecs->Clone();
        }

        for (size_t iReq = jobStruct->nFirstIdx;
             iReq < jobStruct->nLastIdxNotIncluded; ++iReq)
        {
            // Check if we must early exit
            {
                std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
                if (!(*jobStruct->pbGlobalStatus))
                    return;
            }

            const uint64_t *blockIndices =
                jobStruct->panReqBlocksIndices->data() + iReq * l_nDims;

            if (!poArray->AllocateWorkingBuffers(abyRawBlockData,
                                                 abyDecodedBlockData))
            {
                std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
                *jobStruct->pbGlobalStatus = false;
                break;
            }

            bool bIsEmpty = false;
            bool success = poArray->LoadBlockData(
                blockIndices,
                true,  // use mutex
                poCodecs.get(), abyRawBlockData, abyDecodedBlockData, bIsEmpty);

            std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
            if (!success)
            {
                *jobStruct->pbGlobalStatus = false;
                break;
            }

            CachedBlock cachedBlock;
            if (!bIsEmpty)
            {
                if (!abyDecodedBlockData.empty())
                    std::swap(cachedBlock.abyDecoded, abyDecodedBlockData);
                else
                    std::swap(cachedBlock.abyDecoded, abyRawBlockData);
            }
            const std::vector<uint64_t> cacheKey{blockIndices,
                                                 blockIndices + l_nDims};
            poArray->m_oChunkCache[cacheKey] = std::move(cachedBlock);
        }

        std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
        (*jobStruct->pnRemainingThreads)--;
    };

    // Start jobs
    for (int i = 0; i < nThreads; i++)
    {
        if (!wtp->SubmitJob(JobFunc, &asJobStructs[i]))
        {
            std::lock_guard<std::mutex> oLock(m_oMutex);
            bGlobalStatus = false;
            nRemainingThreads = i;
            break;
        }
    }

    // Wait for all jobs to be finished
    while (true)
    {
        {
            std::lock_guard<std::mutex> oLock(m_oMutex);
            if (nRemainingThreads == 0)
                break;
        }
        wtp->WaitEvent();
    }

    return bGlobalStatus;
}

/************************************************************************/
/*                    ZarrV3Array::FlushDirtyBlock()                    */
/************************************************************************/

bool ZarrV3Array::FlushDirtyBlock() const
{
    if (!m_bDirtyBlock)
        return true;
    m_bDirtyBlock = false;

    // Sharded arrays need special handling: the block cache operates at
    // inner chunk granularity but we must write complete shards.
    if (m_poCodecs && m_poCodecs->SupportsPartialDecoding())
    {
        return FlushDirtyBlockSharded();
    }

    std::string osFilename = BuildChunkFilename(m_anCachedBlockIndices.data());

    const size_t nSourceSize =
        m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;
    const auto &abyBlock = m_abyDecodedBlockData.empty()
                               ? m_abyRawBlockData
                               : m_abyDecodedBlockData;

    if (IsEmptyBlock(abyBlock))
    {
        m_bCachedBlockEmpty = true;

        VSIStatBufL sStat;
        if (VSIStatL(osFilename.c_str(), &sStat) == 0)
        {
            CPLDebugOnly(ZARR_DEBUG_KEY,
                         "Deleting tile %s that has now empty content",
                         osFilename.c_str());
            return VSIUnlink(osFilename.c_str()) == 0;
        }
        return true;
    }

    if (!m_abyDecodedBlockData.empty())
    {
        const size_t nDTSize = m_oType.GetSize();
        const size_t nValues = m_abyDecodedBlockData.size() / nDTSize;
        GByte *pDst = &m_abyRawBlockData[0];
        const GByte *pSrc = m_abyDecodedBlockData.data();
        for (size_t i = 0; i < nValues;
             i++, pDst += nSourceSize, pSrc += nDTSize)
        {
            EncodeElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    const size_t nSizeBefore = m_abyRawBlockData.size();
    if (m_poCodecs)
    {
        if (!m_poCodecs->Encode(m_abyRawBlockData))
        {
            m_abyRawBlockData.resize(nSizeBefore);
            return false;
        }
    }

    if (m_osDimSeparator == "/")
    {
        std::string osDir = CPLGetDirnameSafe(osFilename.c_str());
        VSIStatBufL sStat;
        if (VSIStatL(osDir.c_str(), &sStat) != 0)
        {
            if (VSIMkdirRecursive(osDir.c_str(), 0755) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot create directory %s", osDir.c_str());
                m_abyRawBlockData.resize(nSizeBefore);
                return false;
            }
        }
    }

    VSILFILE *fp = VSIFOpenL(osFilename.c_str(), "wb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create tile %s",
                 osFilename.c_str());
        m_abyRawBlockData.resize(nSizeBefore);
        return false;
    }

    bool bRet = true;
    const size_t nRawDataSize = m_abyRawBlockData.size();
    if (VSIFWriteL(m_abyRawBlockData.data(), 1, nRawDataSize, fp) !=
        nRawDataSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not write tile %s correctly", osFilename.c_str());
        bRet = false;
    }
    VSIFCloseL(fp);

    m_abyRawBlockData.resize(nSizeBefore);

    return bRet;
}

/************************************************************************/
/*                ZarrV3Array::FlushDirtyBlockSharded()                 */
/************************************************************************/

// Accumulates dirty inner chunks into a per-shard write cache.
// Actual encoding and writing happens in FlushShardCache().
// This avoids the O(N) decode-encode cost of re-encoding the full shard
// for every inner chunk write (N = inner chunks per shard).
// Single-writer only: concurrent writes to the same shard are not supported.

bool ZarrV3Array::FlushDirtyBlockSharded() const
{
    const size_t nDims = GetDimensionCount();
    const size_t nSourceSize =
        m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;

    // 1. Convert dirty inner block from GDAL format to native format
    if (!m_abyDecodedBlockData.empty())
    {
        const size_t nDTSize = m_oType.GetSize();
        const size_t nValues = m_abyDecodedBlockData.size() / nDTSize;
        GByte *pDst = &m_abyRawBlockData[0];
        const GByte *pSrc = m_abyDecodedBlockData.data();
        for (size_t i = 0; i < nValues;
             i++, pDst += nSourceSize, pSrc += nDTSize)
        {
            EncodeElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    // 2. Compute shard indices and inner block position within shard
    std::vector<uint64_t> anShardIndices(nDims);
    std::vector<size_t> anPosInShard(nDims);
    for (size_t i = 0; i < nDims; ++i)
    {
        anShardIndices[i] = m_anCachedBlockIndices[i] * m_anInnerBlockSize[i] /
                            m_anOuterBlockSize[i];
        anPosInShard[i] = static_cast<size_t>(m_anCachedBlockIndices[i] %
                                              m_anCountInnerBlockInOuter[i]);
    }

    std::string osFilename = BuildChunkFilename(anShardIndices.data());

    // 3. Get or create shard cache entry
    size_t nShardElements = 1;
    for (size_t i = 0; i < nDims; ++i)
        nShardElements *= static_cast<size_t>(m_anOuterBlockSize[i]);

    size_t nTotalInnerChunks = 1;
    for (size_t i = 0; i < nDims; ++i)
        nTotalInnerChunks *= static_cast<size_t>(m_anCountInnerBlockInOuter[i]);

    auto oIt = m_oShardWriteCache.find(osFilename);
    if (oIt == m_oShardWriteCache.end())
    {
        ShardWriteEntry entry;
        try
        {
            entry.abyShardBuffer.resize(nShardElements * nSourceSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate memory for shard buffer");
            return false;
        }
        try
        {
            entry.abDirtyInnerChunks.resize(nTotalInnerChunks, false);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate memory for dirty chunk tracking");
            return false;
        }

        // Read existing shard or fill with nodata
        VSIStatBufL sStat;
        if (VSIStatL(osFilename.c_str(), &sStat) == 0)
        {
            VSILFILE *fpRead = VSIFOpenL(osFilename.c_str(), "rb");
            if (fpRead == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot open shard file %s for reading",
                         osFilename.c_str());
                return false;
            }

            ZarrByteVectorQuickResize abyFileData;
            try
            {
                abyFileData.resize(static_cast<size_t>(sStat.st_size));
            }
            catch (const std::exception &)
            {
                CPLError(
                    CE_Failure, CPLE_OutOfMemory,
                    "Cannot allocate " CPL_FRMT_GUIB " bytes for shard file %s",
                    static_cast<GUIntBig>(sStat.st_size), osFilename.c_str());
                VSIFCloseL(fpRead);
                return false;
            }
            if (VSIFReadL(abyFileData.data(), 1, abyFileData.size(), fpRead) !=
                abyFileData.size())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot read shard file %s", osFilename.c_str());
                VSIFCloseL(fpRead);
                return false;
            }
            VSIFCloseL(fpRead);

            entry.abyShardBuffer = std::move(abyFileData);
            if (!m_poCodecs->Decode(entry.abyShardBuffer))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot decode existing shard %s", osFilename.c_str());
                return false;
            }

            if (entry.abyShardBuffer.size() != nShardElements * nSourceSize)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Decoded shard %s has unexpected size",
                         osFilename.c_str());
                return false;
            }
        }
        else
        {
            if (m_pabyNoData == nullptr ||
                (m_oType.GetClass() == GEDTC_NUMERIC &&
                 GetNoDataValueAsDouble() == 0.0))
            {
                memset(entry.abyShardBuffer.data(), 0,
                       entry.abyShardBuffer.size());
            }
            else
            {
                for (size_t i = 0; i < nShardElements; ++i)
                {
                    memcpy(entry.abyShardBuffer.data() + i * nSourceSize,
                           m_pabyNoData, nSourceSize);
                }
            }
        }

        oIt = m_oShardWriteCache.emplace(osFilename, std::move(entry)).first;
    }

    // cppcheck-suppress derefInvalidIteratorRedundantCheck
    auto &entry = oIt->second;

    // 4. Compute inner chunk linear index and mark dirty
    size_t nInnerChunkIdx = 0;
    for (size_t i = 0; i < nDims; ++i)
    {
        nInnerChunkIdx = nInnerChunkIdx * static_cast<size_t>(
                                              m_anCountInnerBlockInOuter[i]) +
                         anPosInShard[i];
    }
    entry.abDirtyInnerChunks[nInnerChunkIdx] = true;
    const bool bAllDirty =
        std::all_of(entry.abDirtyInnerChunks.begin(),
                    entry.abDirtyInnerChunks.end(), [](bool b) { return b; });

    // 5. Copy dirty inner block into shard buffer at correct position.
    // Same strided N-D copy pattern as CopySubArrayIntoLargerOne() in
    // zarr_v3_codec_sharding.cpp (operates on GUInt64 block sizes here).
    {
        std::vector<size_t> anShardStride(nDims);
        size_t nStride = nSourceSize;
        for (size_t iDim = nDims; iDim > 0;)
        {
            --iDim;
            anShardStride[iDim] = nStride;
            nStride *= static_cast<size_t>(m_anOuterBlockSize[iDim]);
        }

        GByte *pShardDst = entry.abyShardBuffer.data();
        for (size_t iDim = 0; iDim < nDims; ++iDim)
        {
            pShardDst += anPosInShard[iDim] *
                         static_cast<size_t>(m_anInnerBlockSize[iDim]) *
                         anShardStride[iDim];
        }

        const GByte *pInnerSrc = m_abyRawBlockData.data();
        const size_t nLastDimBytes =
            static_cast<size_t>(m_anInnerBlockSize.back()) * nSourceSize;

        if (nDims == 1)
        {
            memcpy(pShardDst, pInnerSrc, nLastDimBytes);
        }
        else
        {
            std::vector<GByte *> dstPtrStack(nDims + 1);
            std::vector<size_t> count(nDims + 1);
            dstPtrStack[0] = pShardDst;
            size_t dimIdx = 0;
        lbl_next_depth:
            if (dimIdx + 1 == nDims)
            {
                memcpy(dstPtrStack[dimIdx], pInnerSrc, nLastDimBytes);
                pInnerSrc += nLastDimBytes;
            }
            else
            {
                count[dimIdx] = static_cast<size_t>(m_anInnerBlockSize[dimIdx]);
                while (true)
                {
                    dimIdx++;
                    dstPtrStack[dimIdx] = dstPtrStack[dimIdx - 1];
                    goto lbl_next_depth;
                lbl_return_to_caller:
                    dimIdx--;
                    if (--count[dimIdx] == 0)
                        break;
                    dstPtrStack[dimIdx] += anShardStride[dimIdx];
                }
            }
            if (dimIdx > 0)
                goto lbl_return_to_caller;
        }
    }

    // 6. Flush shard immediately if all inner chunks have been written,
    // to bound memory usage during sequential writes.
    if (bAllDirty)
    {
        const bool bOK = FlushSingleShard(osFilename, entry);
        m_oShardWriteCache.erase(osFilename);
        return bOK;
    }

    return true;
}

/************************************************************************/
/*                   ZarrV3Array::FlushSingleShard()                    */
/************************************************************************/

bool ZarrV3Array::FlushSingleShard(const std::string &osFilename,
                                   ShardWriteEntry &entry) const
{
    // Encode mutates abyShardBuffer in-place. On failure the buffer
    // is left in an undefined state, but the shard is not written.
    if (!m_poCodecs->Encode(entry.abyShardBuffer))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot encode shard for %s",
                 osFilename.c_str());
        return false;
    }

    // All-nodata shard: skip writing (or delete stale file from prior write)
    if (entry.abyShardBuffer.empty())
    {
        VSIStatBufL sStat;
        if (VSIStatL(osFilename.c_str(), &sStat) == 0)
            VSIUnlink(osFilename.c_str());
        return true;
    }

    // Create directory if needed
    if (m_osDimSeparator == "/")
    {
        std::string osDir = CPLGetDirnameSafe(osFilename.c_str());
        VSIStatBufL sStatDir;
        if (VSIStatL(osDir.c_str(), &sStatDir) != 0)
        {
            if (VSIMkdirRecursive(osDir.c_str(), 0755) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot create directory %s", osDir.c_str());
                return false;
            }
        }
    }

    VSILFILE *fp = VSIFOpenL(osFilename.c_str(), "wb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create shard file %s",
                 osFilename.c_str());
        return false;
    }

    const size_t nEncodedSize = entry.abyShardBuffer.size();
    bool bRet = true;
    if (VSIFWriteL(entry.abyShardBuffer.data(), 1, nEncodedSize, fp) !=
        nEncodedSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not write shard file %s correctly", osFilename.c_str());
        bRet = false;
    }
    VSIFCloseL(fp);
    return bRet;
}

/************************************************************************/
/*                    ZarrV3Array::FlushShardCache()                    */
/************************************************************************/

// Encodes and writes all cached shards. Called from Flush().
// Each shard is encoded exactly once regardless of how many inner chunks
// were written.

bool ZarrV3Array::FlushShardCache() const
{
    if (m_oShardWriteCache.empty())
        return true;

    bool bRet = true;
    for (auto &[osFilename, entry] : m_oShardWriteCache)
    {
        if (!FlushSingleShard(osFilename, entry))
            bRet = false;
    }

    m_oShardWriteCache.clear();
    return bRet;
}

/************************************************************************/
/*                          ExtractSubArray()                           */
/************************************************************************/

static void ExtractSubArray(const GByte *const pabySrc,
                            const std::vector<size_t> &anSrcStart,
                            const std::vector<GPtrDiff_t> &anSrcStrideElts,
                            const std::vector<size_t> &anCount,
                            GByte *const pabyDst,
                            const std::vector<GPtrDiff_t> &anDstStrideElts,
                            const size_t nDTSize)
{
    const auto nDims = anSrcStart.size();
    CPLAssert(nDims > 0);
    CPLAssert(nDims == anSrcStrideElts.size());
    CPLAssert(nDims == anCount.size());
    CPLAssert(nDims == anDstStrideElts.size());

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
    std::vector<const GByte *> srcPtrStack(nDims);
    std::vector<GByte *> dstPtrStack(nDims);
    std::vector<GPtrDiff_t> anSrcStrideBytes(nDims);
    std::vector<GPtrDiff_t> anDstStrideBytes(nDims);
    std::vector<size_t> count(nDims);

    srcPtrStack[0] = pabySrc;
    for (size_t i = 0; i < nDims; ++i)
    {
        anSrcStrideBytes[i] = anSrcStrideElts[i] * nDTSize;
        anDstStrideBytes[i] = anDstStrideElts[i] * nDTSize;
        srcPtrStack[0] += anSrcStart[i] * anSrcStrideBytes[i];
    }
    dstPtrStack[0] = pabyDst;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    const size_t nLastDimSize = anCount.back() * nDTSize;
    size_t dimIdx = 0;
lbl_next_depth:
    if (dimIdx + 1 == nDims)
    {
        memcpy(dstPtrStack[dimIdx], srcPtrStack[dimIdx], nLastDimSize);
    }
    else
    {
        count[dimIdx] = anCount[dimIdx];
        while (true)
        {
            dimIdx++;
            srcPtrStack[dimIdx] = srcPtrStack[dimIdx - 1];
            dstPtrStack[dimIdx] = dstPtrStack[dimIdx - 1];
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if (--count[dimIdx] == 0)
                break;
            srcPtrStack[dimIdx] += anSrcStrideBytes[dimIdx];
            dstPtrStack[dimIdx] += anDstStrideBytes[dimIdx];
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;
}

/************************************************************************/
/*                 ZarrV3Array::WriteChunksThreadSafe()                 */
/************************************************************************/

bool ZarrV3Array::WriteChunksThreadSafe(
    const GUInt64 *arrayStartIdx, const size_t *count,
    [[maybe_unused]] const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
    [[maybe_unused]] const GDALExtendedDataType &bufferDataType,
    const void *pSrcBuffer, const int iThread, const int nThreads,
    std::string &osErrorMsg) const
{
    CPLAssert(m_oType == bufferDataType);

    const auto nDims = GetDimensionCount();
    std::vector<size_t> anChunkCount(nDims);
    std::vector<size_t> anChunkCoord(nDims);
    size_t nChunks = 1;
    for (size_t i = 0; i < nDims; ++i)
    {
        CPLAssert(count[i] == 1 || arrayStep[i] == 1);
        anChunkCount[i] = static_cast<size_t>(cpl::div_round_up(
            static_cast<uint64_t>(count[i]), m_anOuterBlockSize[i]));
        nChunks *= anChunkCount[i];
    }

    const size_t iFirstChunk = static_cast<size_t>(
        (static_cast<uint64_t>(iThread) * nChunks) / nThreads);
    const size_t iLastChunkExcluded = static_cast<size_t>(
        (static_cast<uint64_t>(iThread + 1) * nChunks) / nThreads);

    std::vector<size_t> anSrcStart(nDims);
    const std::vector<GPtrDiff_t> anSrcStrideElts(bufferStride,
                                                  bufferStride + nDims);
    std::vector<GPtrDiff_t> anDstStrideElts(nDims);

    size_t nDstStride = 1;
    for (size_t i = nDims, iChunkCur = iFirstChunk; i > 0;)
    {
        --i;
        anChunkCoord[i] = iChunkCur % anChunkCount[i];
        iChunkCur /= anChunkCount[i];

        anDstStrideElts[i] = nDstStride;
        nDstStride *= static_cast<size_t>(m_anOuterBlockSize[i]);
    }

    const auto StoreError = [this, &osErrorMsg](const std::string &s)
    {
        std::lock_guard oLock(m_oMutex);
        if (!osErrorMsg.empty())
            osErrorMsg += '\n';
        osErrorMsg = s;
        return false;
    };

    const size_t nDTSize = m_oType.GetSize();
    const size_t nDstSize =
        static_cast<size_t>(MultiplyElements(m_anOuterBlockSize)) * nDTSize;
    ZarrByteVectorQuickResize abyDst;
    try
    {
        abyDst.resize(nDstSize);
    }
    catch (const std::exception &)
    {
        return StoreError("Out of memory allocating temporary buffer");
    }

    std::unique_ptr<ZarrV3CodecSequence> poCodecs;
    if (m_poCodecs)
    {
        // Codec cloning is not thread safe
        std::lock_guard oLock(m_oMutex);
        poCodecs = m_poCodecs->Clone();
    }

    std::vector<uint64_t> anChunkIndex(nDims);
    std::vector<size_t> anCount(nDims);
    for (size_t iChunk = iFirstChunk; iChunk < iLastChunkExcluded; ++iChunk)
    {
        if (iChunk > iFirstChunk)
        {
            size_t iDimToIncrement = nDims - 1;
            while (++anChunkCoord[iDimToIncrement] ==
                   anChunkCount[iDimToIncrement])
            {
                anChunkCoord[iDimToIncrement] = 0;
                CPLAssert(iDimToIncrement >= 1);
                --iDimToIncrement;
            }
        }

        bool bPartialChunk = false;
        for (size_t i = 0; i < nDims; ++i)
        {
            anChunkIndex[i] =
                anChunkCoord[i] + arrayStartIdx[i] / m_anOuterBlockSize[i];
            anSrcStart[i] =
                anChunkCoord[i] * static_cast<size_t>(m_anOuterBlockSize[i]);
            anCount[i] = static_cast<size_t>(std::min(
                m_aoDims[i]->GetSize() - arrayStartIdx[i] - anSrcStart[i],
                m_anOuterBlockSize[i]));
            bPartialChunk = bPartialChunk || anCount[i] < m_anOuterBlockSize[i];
        }

        // Resize to target size, as a previous iteration may have shorten it
        // during compression.
        abyDst.resize(nDstSize);
        if (bPartialChunk)
            memset(abyDst.data(), 0, nDstSize);

        ExtractSubArray(static_cast<const GByte *>(pSrcBuffer), anSrcStart,
                        anSrcStrideElts, anCount, abyDst.data(),
                        anDstStrideElts, nDTSize);

        const std::string osFilename = BuildChunkFilename(anChunkIndex.data());
        if (IsEmptyBlock(abyDst))
        {
            VSIStatBufL sStat;
            if (VSIStatL(osFilename.c_str(), &sStat) == 0)
            {
                CPLDebugOnly(ZARR_DEBUG_KEY,
                             "Deleting chunk %s that has now empty content",
                             osFilename.c_str());
                if (VSIUnlink(osFilename.c_str()) != 0)
                {
                    return StoreError("Chunk " + osFilename +
                                      " deletion failed");
                }
            }
            continue;
        }

        if (poCodecs)
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            if (!poCodecs->Encode(abyDst))
            {
                return StoreError(CPLGetLastErrorMsg());
            }
        }

        if (m_osDimSeparator == "/")
        {
            const std::string osDir = CPLGetDirnameSafe(osFilename.c_str());
            VSIStatBufL sStat;
            if (VSIStatL(osDir.c_str(), &sStat) != 0 &&
                VSIMkdirRecursive(osDir.c_str(), 0755) != 0)
            {
                return StoreError("Cannot create directory " + osDir);
            }
        }

        auto fp = VSIFilesystemHandler::OpenStatic(osFilename.c_str(), "wb");
        if (fp == nullptr)
        {
            return StoreError("Cannot create file " + osFilename);
        }

        if (fp->Write(abyDst.data(), abyDst.size()) != abyDst.size() ||
            fp->Close() != 0)
        {
            return StoreError("Write error while writing " + osFilename);
        }
    }

    return true;
}

/************************************************************************/
/*                        ZarrV3Array::IWrite()                         */
/************************************************************************/

bool ZarrV3Array::IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                         const GInt64 *arrayStep,
                         const GPtrDiff_t *bufferStride,
                         const GDALExtendedDataType &bufferDataType,
                         const void *pSrcBuffer)
{
    if (m_oType.GetClass() == GEDTC_STRING)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Writing Zarr V3 string data types is not yet supported");
        return false;
    }

    // Multithreading writing if window is aligned on chunk boundaries.
    if (m_oType == bufferDataType && m_oType.GetClass() == GEDTC_NUMERIC)
    {
        const auto nDims = GetDimensionCount();
        bool bCanUseMultiThreading = true;
        size_t nChunks = 1;
        for (size_t i = 0; i < nDims; ++i)
        {
            if ((arrayStartIdx[i] % m_anOuterBlockSize[i]) != 0 ||
                (count[i] != 1 && arrayStep[i] != 1) ||
                !((count[i] % m_anOuterBlockSize[i]) == 0 ||
                  arrayStartIdx[i] + count[i] == m_aoDims[i]->GetSize()))
            {
                bCanUseMultiThreading = false;
                break;
            }
            nChunks *= static_cast<size_t>(cpl::div_round_up(
                static_cast<uint64_t>(count[i]), m_anOuterBlockSize[i]));
        }
        if (bCanUseMultiThreading && nChunks >= 2)
        {
            const int nMaxThreads = static_cast<int>(
                std::min<size_t>(nChunks, GDAL_DEFAULT_MAX_THREAD_COUNT));
            const int nThreads =
                GDALGetNumThreads(nMaxThreads, /* bDefaultAllCPUs=*/false);
            CPLWorkerThreadPool *wtp =
                nThreads >= 2 ? GDALGetGlobalThreadPool(nThreads) : nullptr;

            if (wtp)
            {
                m_oChunkCache.clear();

                if (!FlushDirtyBlock())
                    return false;

                CPLDebug("Zarr", "Using %d threads for writing", nThreads);
                auto poJobQueue = wtp->CreateJobQueue();
                std::atomic<bool> bSuccess = true;
                std::string osErrorMsg;
                for (int iThread = 0; iThread < nThreads; ++iThread)
                {
                    auto job = [this, iThread, nThreads, arrayStartIdx, count,
                                arrayStep, bufferStride, pSrcBuffer,
                                &bufferDataType, &bSuccess, &osErrorMsg]()
                    {
                        if (bSuccess &&
                            !WriteChunksThreadSafe(
                                arrayStartIdx, count, arrayStep, bufferStride,
                                bufferDataType, pSrcBuffer, iThread, nThreads,
                                osErrorMsg))
                        {
                            bSuccess = false;
                        }
                    };
                    if (!poJobQueue->SubmitJob(job))
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "ZarrV3Array::IWrite(): job submission failed");
                        return false;
                    }
                }

                poJobQueue->WaitCompletion();

                if (!bSuccess)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "ZarrV3Array::IWrite(): %s", osErrorMsg.c_str());
                }

                return bSuccess;
            }
        }
    }

    return ZarrArray::IWrite(arrayStartIdx, count, arrayStep, bufferStride,
                             bufferDataType, pSrcBuffer);
}

/************************************************************************/
/*                         BuildChunkFilename()                         */
/************************************************************************/

std::string ZarrV3Array::BuildChunkFilename(const uint64_t *blockIndices) const
{
    if (m_aoDims.empty())
    {
        return CPLFormFilenameSafe(
            CPLGetDirnameSafe(m_osFilename.c_str()).c_str(),
            m_bV2ChunkKeyEncoding ? "0" : "c", nullptr);
    }
    else
    {
        std::string osFilename(CPLGetDirnameSafe(m_osFilename.c_str()));
        osFilename += '/';
        if (!m_bV2ChunkKeyEncoding)
        {
            osFilename += 'c';
        }
        for (size_t i = 0; i < m_aoDims.size(); ++i)
        {
            if (i > 0 || !m_bV2ChunkKeyEncoding)
                osFilename += m_osDimSeparator;
            osFilename += std::to_string(blockIndices[i]);
        }
        return osFilename;
    }
}

/************************************************************************/
/*                          GetDataDirectory()                          */
/************************************************************************/

std::string ZarrV3Array::GetDataDirectory() const
{
    return std::string(CPLGetDirnameSafe(m_osFilename.c_str()));
}

/************************************************************************/
/*                    GetChunkIndicesFromFilename()                     */
/************************************************************************/

CPLStringList
ZarrV3Array::GetChunkIndicesFromFilename(const char *pszFilename) const
{
    if (!m_bV2ChunkKeyEncoding)
    {
        if (pszFilename[0] != 'c')
            return CPLStringList();
        if (m_osDimSeparator == "/")
        {
            if (pszFilename[1] != '/' && pszFilename[1] != '\\')
                return CPLStringList();
        }
        else if (pszFilename[1] != m_osDimSeparator[0])
        {
            return CPLStringList();
        }
    }
    return CPLStringList(
        CSLTokenizeString2(pszFilename + (m_bV2ChunkKeyEncoding ? 0 : 2),
                           m_osDimSeparator.c_str(), 0));
}

/************************************************************************/
/*                            ParseDtypeV3()                            */
/************************************************************************/

static GDALExtendedDataType ParseDtypeV3(const CPLJSONObject &obj,
                                         std::vector<DtypeElt> &elts)
{
    do
    {
        if (obj.GetType() == CPLJSONObject::Type::String)
        {
            const auto str = obj.ToString();
            DtypeElt elt;
            GDALDataType eDT = GDT_Unknown;

            if (str == "bool")  // boolean
            {
                elt.nativeType = DtypeElt::NativeType::BOOLEAN;
                eDT = GDT_UInt8;
            }
            else if (str == "int8")
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int8;
            }
            else if (str == "uint8")
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt8;
            }
            else if (str == "int16")
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int16;
            }
            else if (str == "uint16")
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt16;
            }
            else if (str == "int32")
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int32;
            }
            else if (str == "uint32")
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt32;
            }
            else if (str == "int64")
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int64;
            }
            else if (str == "uint64")
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt64;
            }
            else if (str == "float16")
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float16;
            }
            else if (str == "float32")
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float32;
            }
            else if (str == "float64")
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float64;
            }
            else if (str == "complex64")
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat32;
            }
            else if (str == "complex128")
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat64;
            }
            else
                break;

            elt.gdalType = GDALExtendedDataType::Create(eDT);
            elt.gdalSize = elt.gdalType.GetSize();
            if (!elt.gdalTypeIsApproxOfNative)
                elt.nativeSize = elt.gdalSize;
            if (elt.nativeSize > 1)
            {
                elt.needByteSwapping = (CPL_IS_LSB == 0);
            }
            elts.emplace_back(elt);
            return GDALExtendedDataType::Create(eDT);
        }
        else if (obj.GetType() == CPLJSONObject::Type::Object)
        {
            const auto osName = obj["name"].ToString();
            const auto oConfig = obj["configuration"];
            DtypeElt elt;

            if (osName == "null_terminated_bytes" && oConfig.IsValid())
            {
                const int nBytes = oConfig["length_bytes"].ToInteger();
                if (nBytes <= 0 || nBytes > 10 * 1024 * 1024)
                    break;
                elt.nativeType = DtypeElt::NativeType::STRING_ASCII;
                elt.nativeSize = static_cast<size_t>(nBytes);
                elt.gdalType = GDALExtendedDataType::CreateString(
                    static_cast<size_t>(nBytes));
                elt.gdalSize = elt.gdalType.GetSize();
                elts.emplace_back(elt);
                return GDALExtendedDataType::CreateString(
                    static_cast<size_t>(nBytes));
            }
            else if (osName == "fixed_length_utf32" && oConfig.IsValid())
            {
                const int nBytes = oConfig["length_bytes"].ToInteger();
                if (nBytes <= 0 || nBytes % 4 != 0 || nBytes > 10 * 1024 * 1024)
                    break;
                elt.nativeType = DtypeElt::NativeType::STRING_UNICODE;
                elt.nativeSize = static_cast<size_t>(nBytes);
                // Endianness handled by the bytes codec in v3
                elt.gdalType = GDALExtendedDataType::CreateString();
                elt.gdalSize = elt.gdalType.GetSize();
                elts.emplace_back(elt);
                return GDALExtendedDataType::CreateString();
            }
            else if (osName == "numpy.datetime64" ||
                     osName == "numpy.timedelta64")
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                elt.gdalType = GDALExtendedDataType::Create(GDT_Int64);
                elt.gdalSize = elt.gdalType.GetSize();
                elt.nativeSize = elt.gdalSize;
                elt.needByteSwapping = (CPL_IS_LSB == 0);
                elts.emplace_back(elt);
                return GDALExtendedDataType::Create(GDT_Int64);
            }
        }
    } while (false);
    CPLError(CE_Failure, CPLE_AppDefined,
             "Invalid or unsupported format for data_type: %s",
             obj.ToString().c_str());
    return GDALExtendedDataType::Create(GDT_Unknown);
}

/************************************************************************/
/*                     ParseNoDataStringAsDouble()                      */
/************************************************************************/

static double ParseNoDataStringAsDouble(const std::string &osVal, bool &bOK)
{
    double dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
    if (osVal == "NaN")
    {
        // initialized above
    }
    else if (osVal == "Infinity" || osVal == "+Infinity")
    {
        dfNoDataValue = std::numeric_limits<double>::infinity();
    }
    else if (osVal == "-Infinity")
    {
        dfNoDataValue = -std::numeric_limits<double>::infinity();
    }
    else
    {
        bOK = false;
    }
    return dfNoDataValue;
}

/************************************************************************/
/*                        ParseNoDataComponent()                        */
/************************************************************************/

template <typename T, typename Tint>
static T ParseNoDataComponent(const CPLJSONObject &oObj, bool &bOK)
{
    if (oObj.GetType() == CPLJSONObject::Type::Integer ||
        oObj.GetType() == CPLJSONObject::Type::Long ||
        oObj.GetType() == CPLJSONObject::Type::Double)
    {
        return static_cast<T>(oObj.ToDouble());
    }
    else if (oObj.GetType() == CPLJSONObject::Type::String)
    {
        const auto osVal = oObj.ToString();
        if (STARTS_WITH(osVal.c_str(), "0x"))
        {
            if (osVal.size() > 2 + 2 * sizeof(T))
            {
                bOK = false;
                return 0;
            }
            Tint nVal = static_cast<Tint>(
                std::strtoull(osVal.c_str() + 2, nullptr, 16));
            T fVal;
            static_assert(sizeof(nVal) == sizeof(fVal),
                          "sizeof(nVal) == sizeof(dfVal)");
            memcpy(&fVal, &nVal, sizeof(nVal));
            return fVal;
        }
        else
        {
            return static_cast<T>(ParseNoDataStringAsDouble(osVal, bOK));
        }
    }
    else
    {
        bOK = false;
        return 0;
    }
}

/************************************************************************/
/*                       ZarrV3Group::LoadArray()                       */
/************************************************************************/

std::shared_ptr<ZarrArray>
ZarrV3Group::LoadArray(const std::string &osArrayName,
                       const std::string &osZarrayFilename,
                       const CPLJSONObject &oRoot) const
{
    // Add osZarrayFilename to m_poSharedResource during the scope
    // of this function call.
    ZarrSharedResource::SetFilenameAdder filenameAdder(m_poSharedResource,
                                                       osZarrayFilename);
    if (!filenameAdder.ok())
        return nullptr;

    // Warn about unknown members (the spec suggests to error out, but let be
    // a bit more lenient)
    for (const auto &oNode : oRoot.GetChildren())
    {
        const auto osName = oNode.GetName();
        if (osName != "zarr_format" && osName != "node_type" &&
            osName != "shape" && osName != "chunk_grid" &&
            osName != "data_type" && osName != "chunk_key_encoding" &&
            osName != "fill_value" &&
            // Below are optional
            osName != "dimension_names" && osName != "codecs" &&
            osName != "storage_transformers" && osName != "attributes")
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s array definition contains a unknown member (%s). "
                     "Interpretation of the array might be wrong.",
                     osZarrayFilename.c_str(), osName.c_str());
        }
    }

    const auto oStorageTransformers = oRoot["storage_transformers"].ToArray();
    if (oStorageTransformers.Size() > 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "storage_transformers are not supported.");
        return nullptr;
    }

    const auto oShape = oRoot["shape"].ToArray();
    if (!oShape.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "shape missing or not an array");
        return nullptr;
    }

    // Parse chunk_grid
    const auto oChunkGrid = oRoot["chunk_grid"];
    if (oChunkGrid.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "chunk_grid missing or not an object");
        return nullptr;
    }

    const auto oChunkGridName = oChunkGrid["name"];
    if (oChunkGridName.ToString() != "regular")
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only chunk_grid.name = regular supported");
        return nullptr;
    }

    const auto oChunks = oChunkGrid["configuration"]["chunk_shape"].ToArray();
    if (!oChunks.IsValid())
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "chunk_grid.configuration.chunk_shape missing or not an array");
        return nullptr;
    }

    if (oShape.Size() != oChunks.Size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "shape and chunks arrays are of different size");
        return nullptr;
    }

    // Parse chunk_key_encoding
    const auto oChunkKeyEncoding = oRoot["chunk_key_encoding"];
    if (oChunkKeyEncoding.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "chunk_key_encoding missing or not an object");
        return nullptr;
    }

    std::string osDimSeparator;
    bool bV2ChunkKeyEncoding = false;
    const auto oChunkKeyEncodingName = oChunkKeyEncoding["name"];
    if (oChunkKeyEncodingName.ToString() == "default")
    {
        osDimSeparator = "/";
    }
    else if (oChunkKeyEncodingName.ToString() == "v2")
    {
        osDimSeparator = ".";
        bV2ChunkKeyEncoding = true;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported chunk_key_encoding.name");
        return nullptr;
    }

    {
        auto oConfiguration = oChunkKeyEncoding["configuration"];
        if (oConfiguration.GetType() == CPLJSONObject::Type::Object)
        {
            auto oSeparator = oConfiguration["separator"];
            if (oSeparator.IsValid())
            {
                osDimSeparator = oSeparator.ToString();
                if (osDimSeparator != "/" && osDimSeparator != ".")
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Separator can only be '/' or '.'");
                    return nullptr;
                }
            }
        }
    }

    CPLJSONObject oAttributes = oRoot["attributes"];

    // Deep-clone of oAttributes
    if (oAttributes.IsValid())
    {
        oAttributes = oAttributes.Clone();
    }

    std::vector<std::shared_ptr<GDALDimension>> aoDims;
    for (int i = 0; i < oShape.Size(); ++i)
    {
        const auto nSize = static_cast<GUInt64>(oShape[i].ToLong());
        if (nSize == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid content for shape");
            return nullptr;
        }
        aoDims.emplace_back(std::make_shared<ZarrDimension>(
            m_poSharedResource,
            std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock()),
            std::string(), CPLSPrintf("dim%d", i), std::string(), std::string(),
            nSize));
    }

    // Deal with dimension_names
    const auto dimensionNames = oRoot["dimension_names"];

    const auto FindDimension = [this, &aoDims, &osArrayName, &oAttributes](
                                   const std::string &osDimName,
                                   std::shared_ptr<GDALDimension> &poDim, int i)
    {
        auto oIter = m_oMapDimensions.find(osDimName);
        if (oIter != m_oMapDimensions.end())
        {
            if (m_bDimSizeInUpdate ||
                oIter->second->GetSize() == poDim->GetSize())
            {
                poDim = oIter->second;
                return true;
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Size of _ARRAY_DIMENSIONS[%d] different "
                         "from the one of shape",
                         i);
                return false;
            }
        }

        // Try to load the indexing variable.
        // Not in m_oMapMDArrays,
        // then stat() the indexing variable.
        else if (osArrayName != osDimName &&
                 m_oMapMDArrays.find(osDimName) == m_oMapMDArrays.end())
        {
            std::string osDirName = m_osDirectoryName;
            while (true)
            {
                const std::string osArrayFilenameDim = CPLFormFilenameSafe(
                    CPLFormFilenameSafe(osDirName.c_str(), osDimName.c_str(),
                                        nullptr)
                        .c_str(),
                    "zarr.json", nullptr);
                VSIStatBufL sStat;
                if (VSIStatL(osArrayFilenameDim.c_str(), &sStat) == 0)
                {
                    CPLJSONDocument oDoc;
                    if (oDoc.Load(osArrayFilenameDim))
                    {
                        LoadArray(osDimName, osArrayFilenameDim,
                                  oDoc.GetRoot());
                    }
                }
                else
                {
                    // Recurse to upper level for datasets such as
                    // /vsis3/hrrrzarr/sfc/20210809/20210809_00z_anl.zarr/0.1_sigma_level/HAIL_max_fcst/0.1_sigma_level/HAIL_max_fcst
                    std::string osDirNameNew =
                        CPLGetPathSafe(osDirName.c_str());
                    if (!osDirNameNew.empty() && osDirNameNew != osDirName)
                    {
                        osDirName = std::move(osDirNameNew);
                        continue;
                    }
                }
                break;
            }
        }

        oIter = m_oMapDimensions.find(osDimName);
        // cppcheck-suppress knownConditionTrueFalse
        if (oIter != m_oMapDimensions.end() &&
            oIter->second->GetSize() == poDim->GetSize())
        {
            poDim = oIter->second;
            return true;
        }

        std::string osType;
        std::string osDirection;
        if (aoDims.size() == 1 && osArrayName == osDimName)
        {
            ZarrArray::GetDimensionTypeDirection(oAttributes, osType,
                                                 osDirection);
        }

        auto poDimLocal = std::make_shared<ZarrDimension>(
            m_poSharedResource,
            std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock()),
            GetFullName(), osDimName, osType, osDirection, poDim->GetSize());
        poDimLocal->SetXArrayDimension();
        m_oMapDimensions[osDimName] = poDimLocal;
        poDim = poDimLocal;
        return true;
    };

    if (dimensionNames.GetType() == CPLJSONObject::Type::Array)
    {
        const auto arrayDims = dimensionNames.ToArray();
        if (arrayDims.Size() == oShape.Size())
        {
            for (int i = 0; i < oShape.Size(); ++i)
            {
                if (arrayDims[i].GetType() == CPLJSONObject::Type::String)
                {
                    const auto osDimName = arrayDims[i].ToString();
                    FindDimension(osDimName, aoDims[i], i);
                }
            }
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Size of dimension_names[] different from the one of shape");
            return nullptr;
        }
    }
    else if (dimensionNames.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dimension_names should be an array");
        return nullptr;
    }

    auto oDtype = oRoot["data_type"];
    if (!oDtype.IsValid())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "data_type missing");
        return nullptr;
    }
    const auto oOrigDtype = oDtype;
    if (oDtype["fallback"].IsValid())
        oDtype = oDtype["fallback"];
    std::vector<DtypeElt> aoDtypeElts;
    const auto oType = ParseDtypeV3(oDtype, aoDtypeElts);
    if (oType.GetClass() == GEDTC_NUMERIC &&
        oType.GetNumericDataType() == GDT_Unknown)
        return nullptr;

    std::vector<GUInt64> anOuterBlockSize;
    if (!ZarrArray::ParseChunkSize(oChunks, oType, anOuterBlockSize))
        return nullptr;

    std::vector<GByte> abyNoData;

    struct NoDataFreer
    {
        std::vector<GByte> &m_abyNodata;
        const GDALExtendedDataType &m_oType;

        NoDataFreer(std::vector<GByte> &abyNoDataIn,
                    const GDALExtendedDataType &oTypeIn)
            : m_abyNodata(abyNoDataIn), m_oType(oTypeIn)
        {
        }

        ~NoDataFreer()
        {
            if (!m_abyNodata.empty())
                m_oType.FreeDynamicMemory(&m_abyNodata[0]);
        }
    };

    NoDataFreer noDataFreer(abyNoData, oType);

    auto oFillValue = oRoot["fill_value"];
    auto eFillValueType = oFillValue.GetType();

    if (!oFillValue.IsValid())
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Missing fill_value is invalid");
    }
    else if (eFillValueType == CPLJSONObject::Type::Null)
    {
        CPLError(CE_Warning, CPLE_AppDefined, "fill_value = null is invalid");
    }
    else if (GDALDataTypeIsComplex(oType.GetNumericDataType()) &&
             eFillValueType != CPLJSONObject::Type::Array)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
        return nullptr;
    }
    else if (eFillValueType == CPLJSONObject::Type::String)
    {
        const auto osFillValue = oFillValue.ToString();
        if (oType.GetClass() == GEDTC_STRING)
        {
            abyNoData.resize(oType.GetSize());
            char *pDstStr = CPLStrdup(osFillValue.c_str());
            char **pDstPtr = reinterpret_cast<char **>(&abyNoData[0]);
            memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
        }
        else if (STARTS_WITH(osFillValue.c_str(), "0x"))
        {
            if (osFillValue.size() > 2 + 2 * oType.GetSize())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            uint64_t nVal = static_cast<uint64_t>(
                std::strtoull(osFillValue.c_str() + 2, nullptr, 16));
            if (oType.GetSize() == 4)
            {
                abyNoData.resize(oType.GetSize());
                uint32_t nTmp = static_cast<uint32_t>(nVal);
                memcpy(&abyNoData[0], &nTmp, sizeof(nTmp));
            }
            else if (oType.GetSize() == 8)
            {
                abyNoData.resize(oType.GetSize());
                memcpy(&abyNoData[0], &nVal, sizeof(nVal));
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Hexadecimal representation of fill_value no "
                         "supported for this data type");
                return nullptr;
            }
        }
        else if (STARTS_WITH(osFillValue.c_str(), "0b"))
        {
            if (osFillValue.size() > 2 + 8 * oType.GetSize())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            uint64_t nVal = static_cast<uint64_t>(
                std::strtoull(osFillValue.c_str() + 2, nullptr, 2));
            if (oType.GetSize() == 4)
            {
                abyNoData.resize(oType.GetSize());
                uint32_t nTmp = static_cast<uint32_t>(nVal);
                memcpy(&abyNoData[0], &nTmp, sizeof(nTmp));
            }
            else if (oType.GetSize() == 8)
            {
                abyNoData.resize(oType.GetSize());
                memcpy(&abyNoData[0], &nVal, sizeof(nVal));
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Binary representation of fill_value no supported for "
                         "this data type");
                return nullptr;
            }
        }
        else
        {
            // Handle "NaT" fill_value for numpy.datetime64/timedelta64
            // NaT is equivalent to INT64_MIN per the zarr extension spec
            if (osFillValue == "NaT" && oType.GetNumericDataType() == GDT_Int64)
            {
                const int64_t nNaT = std::numeric_limits<int64_t>::min();
                abyNoData.resize(oType.GetSize());
                memcpy(&abyNoData[0], &nNaT, sizeof(nNaT));
            }
            else
            {
                bool bOK = true;
                double dfNoDataValue =
                    ParseNoDataStringAsDouble(osFillValue, bOK);
                if (!bOK)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                    return nullptr;
                }
                else if (oType.GetNumericDataType() == GDT_Float16)
                {
                    const GFloat16 hfNoDataValue =
                        static_cast<GFloat16>(dfNoDataValue);
                    abyNoData.resize(sizeof(hfNoDataValue));
                    memcpy(&abyNoData[0], &hfNoDataValue,
                           sizeof(hfNoDataValue));
                }
                else if (oType.GetNumericDataType() == GDT_Float32)
                {
                    const float fNoDataValue =
                        static_cast<float>(dfNoDataValue);
                    abyNoData.resize(sizeof(fNoDataValue));
                    memcpy(&abyNoData[0], &fNoDataValue, sizeof(fNoDataValue));
                }
                else if (oType.GetNumericDataType() == GDT_Float64)
                {
                    abyNoData.resize(sizeof(dfNoDataValue));
                    memcpy(&abyNoData[0], &dfNoDataValue,
                           sizeof(dfNoDataValue));
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid fill_value for this data type");
                    return nullptr;
                }
            }
        }
    }
    else if (eFillValueType == CPLJSONObject::Type::Boolean ||
             eFillValueType == CPLJSONObject::Type::Integer ||
             eFillValueType == CPLJSONObject::Type::Long ||
             eFillValueType == CPLJSONObject::Type::Double)
    {
        const double dfNoDataValue = oFillValue.ToDouble();
        if (oType.GetNumericDataType() == GDT_Int64)
        {
            const int64_t nNoDataValue =
                static_cast<int64_t>(oFillValue.ToLong());
            abyNoData.resize(oType.GetSize());
            GDALCopyWords(&nNoDataValue, GDT_Int64, 0, &abyNoData[0],
                          oType.GetNumericDataType(), 0, 1);
        }
        else if (oType.GetNumericDataType() == GDT_UInt64 &&
                 /* we can't really deal with nodata value between */
                 /* int64::max and uint64::max due to json-c limitations */
                 dfNoDataValue >= 0)
        {
            const int64_t nNoDataValue =
                static_cast<int64_t>(oFillValue.ToLong());
            abyNoData.resize(oType.GetSize());
            GDALCopyWords(&nNoDataValue, GDT_Int64, 0, &abyNoData[0],
                          oType.GetNumericDataType(), 0, 1);
        }
        else
        {
            abyNoData.resize(oType.GetSize());
            GDALCopyWords(&dfNoDataValue, GDT_Float64, 0, &abyNoData[0],
                          oType.GetNumericDataType(), 0, 1);
        }
    }
    else if (eFillValueType == CPLJSONObject::Type::Array)
    {
        const auto oFillValueArray = oFillValue.ToArray();
        if (oFillValueArray.Size() == 2 &&
            GDALDataTypeIsComplex(oType.GetNumericDataType()))
        {
            if (oType.GetNumericDataType() == GDT_CFloat64)
            {
                bool bOK = true;
                const double adfNoDataValue[2] = {
                    ParseNoDataComponent<double, uint64_t>(oFillValueArray[0],
                                                           bOK),
                    ParseNoDataComponent<double, uint64_t>(oFillValueArray[1],
                                                           bOK),
                };
                if (!bOK)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                    return nullptr;
                }
                abyNoData.resize(oType.GetSize());
                CPLAssert(sizeof(adfNoDataValue) == oType.GetSize());
                memcpy(abyNoData.data(), adfNoDataValue,
                       sizeof(adfNoDataValue));
            }
            else
            {
                CPLAssert(oType.GetNumericDataType() == GDT_CFloat32);
                bool bOK = true;
                const float afNoDataValue[2] = {
                    ParseNoDataComponent<float, uint32_t>(oFillValueArray[0],
                                                          bOK),
                    ParseNoDataComponent<float, uint32_t>(oFillValueArray[1],
                                                          bOK),
                };
                if (!bOK)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                    return nullptr;
                }
                abyNoData.resize(oType.GetSize());
                CPLAssert(sizeof(afNoDataValue) == oType.GetSize());
                memcpy(abyNoData.data(), afNoDataValue, sizeof(afNoDataValue));
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
            return nullptr;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
        return nullptr;
    }

    const auto oCodecs = oRoot["codecs"].ToArray();
    std::unique_ptr<ZarrV3CodecSequence> poCodecs;
    std::vector<GUInt64> anInnerBlockSize = anOuterBlockSize;
    if (oCodecs.Size() > 0)
    {
        poCodecs = ZarrV3Array::SetupCodecs(oCodecs, anOuterBlockSize,
                                            anInnerBlockSize,
                                            aoDtypeElts.back(), abyNoData);
        if (!poCodecs)
        {
            return nullptr;
        }
    }

    auto poArray = ZarrV3Array::Create(m_poSharedResource, Self(), osArrayName,
                                       aoDims, oType, aoDtypeElts,
                                       anOuterBlockSize, anInnerBlockSize);
    if (!poArray)
        return nullptr;
    poArray->SetUpdatable(m_bUpdatable);  // must be set before SetAttributes()
    poArray->SetFilename(osZarrayFilename);
    poArray->SetIsV2ChunkKeyEncoding(bV2ChunkKeyEncoding);
    poArray->SetDimSeparator(osDimSeparator);
    if (!abyNoData.empty())
    {
        poArray->RegisterNoDataValue(abyNoData.data());
    }
    poArray->SetAttributes(Self(), oAttributes);
    poArray->SetDtype(oDtype);
    // Expose extension data type configuration as structural info
    if (oOrigDtype.GetType() == CPLJSONObject::Type::Object)
    {
        const auto osName = oOrigDtype.GetString("name");
        if (!osName.empty())
        {
            poArray->SetStructuralInfo("data_type.name", osName.c_str());
        }
        const auto oConfig = oOrigDtype["configuration"];
        if (oConfig.IsValid() &&
            oConfig.GetType() == CPLJSONObject::Type::Object)
        {
            const auto osUnit = oConfig.GetString("unit");
            if (!osUnit.empty())
            {
                poArray->SetStructuralInfo("data_type.unit", osUnit.c_str());
            }
            const auto nScaleFactor = oConfig.GetInteger("scale_factor", -1);
            if (nScaleFactor > 0)
            {
                poArray->SetStructuralInfo("data_type.scale_factor",
                                           CPLSPrintf("%d", nScaleFactor));
            }
        }
    }
    if (oCodecs.Size() > 0 &&
        oCodecs[oCodecs.Size() - 1].GetString("name") != "bytes")
    {
        poArray->SetStructuralInfo(
            "COMPRESSOR", oCodecs[oCodecs.Size() - 1].ToString().c_str());
    }
    if (poCodecs)
        poArray->SetCodecs(oCodecs, std::move(poCodecs));
    RegisterArray(poArray);

    // If this is an indexing variable, attach it to the dimension.
    if (aoDims.size() == 1 && aoDims[0]->GetName() == poArray->GetName())
    {
        auto oIter = m_oMapDimensions.find(poArray->GetName());
        if (oIter != m_oMapDimensions.end())
        {
            oIter->second->SetIndexingVariable(poArray);
        }
    }

    if (CPLTestBool(m_poSharedResource->GetOpenOptions().FetchNameValueDef(
            "CACHE_TILE_PRESENCE", "NO")))
    {
        poArray->BlockCachePresence();
    }

    return poArray;
}

/************************************************************************/
/*                  ZarrV3Array::GetRawBlockInfoInfo()                  */
/************************************************************************/

CPLStringList ZarrV3Array::GetRawBlockInfoInfo() const
{
    CPLStringList aosInfo(m_aosStructuralInfo);
    if (m_oType.GetSize() > 1)
    {
        // By default, assume that the ENDIANNESS is the native one.
        // Otherwise there will be a ZarrV3CodecBytes instance.
        if constexpr (CPL_IS_LSB)
            aosInfo.SetNameValue("ENDIANNESS", "LITTLE");
        else
            aosInfo.SetNameValue("ENDIANNESS", "BIG");
    }

    if (m_poCodecs)
    {
        bool bHasOtherCodec = false;
        for (const auto &poCodec : m_poCodecs->GetCodecs())
        {
            if (poCodec->GetName() == ZarrV3CodecBytes::NAME &&
                m_oType.GetSize() > 1)
            {
                auto poBytesCodec =
                    dynamic_cast<const ZarrV3CodecBytes *>(poCodec.get());
                if (poBytesCodec)
                {
                    if (poBytesCodec->IsLittle())
                        aosInfo.SetNameValue("ENDIANNESS", "LITTLE");
                    else
                        aosInfo.SetNameValue("ENDIANNESS", "BIG");
                }
            }
            else if (poCodec->GetName() == ZarrV3CodecTranspose::NAME &&
                     m_aoDims.size() > 1)
            {
                auto poTransposeCodec =
                    dynamic_cast<const ZarrV3CodecTranspose *>(poCodec.get());
                if (poTransposeCodec && !poTransposeCodec->IsNoOp())
                {
                    const auto &anOrder = poTransposeCodec->GetOrder();
                    const int nDims = static_cast<int>(anOrder.size());
                    std::string osOrder("[");
                    for (int i = 0; i < nDims; ++i)
                    {
                        if (i > 0)
                            osOrder += ',';
                        osOrder += std::to_string(anOrder[i]);
                    }
                    osOrder += ']';
                    aosInfo.SetNameValue("TRANSPOSE_ORDER", osOrder.c_str());
                }
            }
            else if (poCodec->GetName() != ZarrV3CodecGZip::NAME &&
                     poCodec->GetName() != ZarrV3CodecBlosc::NAME &&
                     poCodec->GetName() != ZarrV3CodecZstd::NAME)
            {
                bHasOtherCodec = true;
            }
        }
        if (bHasOtherCodec)
        {
            aosInfo.SetNameValue("CODECS", m_oJSONCodecs.ToString().c_str());
        }

        if (m_poCodecs->SupportsPartialDecoding())
        {
            aosInfo.SetNameValue("CHUNK_TYPE", "INNER");
        }
    }

    return aosInfo;
}

/************************************************************************/
/*                      ZarrV3Array::SetupCodecs()                      */
/************************************************************************/

/* static */ std::unique_ptr<ZarrV3CodecSequence> ZarrV3Array::SetupCodecs(
    const CPLJSONArray &oCodecs, const std::vector<GUInt64> &anOuterBlockSize,
    std::vector<GUInt64> &anInnerBlockSize, DtypeElt &zarrDataType,
    const std::vector<GByte> &abyNoData)

{
    // Byte swapping will be done by the codec chain
    zarrDataType.needByteSwapping = false;

    ZarrArrayMetadata oInputArrayMetadata;
    if (!abyNoData.empty() && zarrDataType.gdalTypeIsApproxOfNative)
    {
        // This cannot happen today with the data types we support, but
        // might in the future. In which case we'll have to translate the
        // nodata value from its GDAL representation to the native one
        // (since that's what zarr_v3_codec_sharding::FillWithNoData()
        // expects
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Zarr driver issue: gdalTypeIsApproxOfNative is not taken "
                 "into account by codecs. Nodata will be assumed to be zero by "
                 "sharding codec");
    }
    else if (!abyNoData.empty() &&
             (zarrDataType.nativeType == DtypeElt::NativeType::STRING_ASCII ||
              zarrDataType.nativeType == DtypeElt::NativeType::STRING_UNICODE))
    {
        // Convert from GDAL representation (char* pointer) to native
        // format (fixed-size null-padded buffer) for FillWithNoData()
        char *pStr = nullptr;
        memcpy(&pStr, abyNoData.data(), sizeof(pStr));
        oInputArrayMetadata.abyNoData.resize(zarrDataType.nativeSize, 0);
        if (pStr &&
            zarrDataType.nativeType == DtypeElt::NativeType::STRING_ASCII)
        {
            const size_t nCopy =
                std::min(strlen(pStr), zarrDataType.nativeSize > 0
                                           ? zarrDataType.nativeSize - 1
                                           : static_cast<size_t>(0));
            memcpy(oInputArrayMetadata.abyNoData.data(), pStr, nCopy);
        }
        // STRING_UNICODE non-empty fill would need UTF-8 to UCS4
        // conversion; zero-fill is correct for the common "" case
    }
    else
    {
        oInputArrayMetadata.abyNoData = abyNoData;
    }
    for (auto &nSize : anOuterBlockSize)
    {
        oInputArrayMetadata.anBlockSizes.push_back(static_cast<size_t>(nSize));
    }
    oInputArrayMetadata.oElt = zarrDataType;
    auto poCodecs = std::make_unique<ZarrV3CodecSequence>(oInputArrayMetadata);
    ZarrArrayMetadata oOutputArrayMetadata;
    if (!poCodecs->InitFromJson(oCodecs, oOutputArrayMetadata))
    {
        return nullptr;
    }
    std::vector<size_t> anOuterBlockSizeSizet;
    for (auto nVal : oOutputArrayMetadata.anBlockSizes)
    {
        anOuterBlockSizeSizet.push_back(static_cast<size_t>(nVal));
    }
    anInnerBlockSize.clear();
    for (size_t nVal : poCodecs->GetInnerMostBlockSize(anOuterBlockSizeSizet))
    {
        anInnerBlockSize.push_back(nVal);
    }
    return poCodecs;
}

/************************************************************************/
/*                       ZarrV3Array::SetCodecs()                       */
/************************************************************************/

void ZarrV3Array::SetCodecs(const CPLJSONArray &oJSONCodecs,
                            std::unique_ptr<ZarrV3CodecSequence> &&poCodecs)
{
    m_oJSONCodecs = oJSONCodecs;
    m_poCodecs = std::move(poCodecs);
}

/************************************************************************/
/*                     ZarrV3Array::LoadOverviews()                     */
/************************************************************************/

void ZarrV3Array::LoadOverviews() const
{
    if (m_bOverviewsLoaded)
        return;
    m_bOverviewsLoaded = true;

    // Cf https://github.com/zarr-conventions/multiscales
    // and https://github.com/zarr-conventions/spatial

    const auto poRG = GetRootGroup();
    if (!poRG)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "LoadOverviews(): cannot access root group");
        return;
    }

    auto poGroup = GetParentGroup();
    if (!poGroup)
    {
        CPLDebugOnly(ZARR_DEBUG_KEY,
                     "LoadOverviews(): cannot access parent group");
        return;
    }

    // Look for "zarr_conventions" and "multiscales" attributes in our
    // immediate parent, or in our grandparent if not found.
    auto poAttrZarrConventions = poGroup->GetAttribute("zarr_conventions");
    auto poAttrMultiscales = poGroup->GetAttribute("multiscales");
    if (!poAttrZarrConventions || !poAttrMultiscales)
    {
        poGroup = poGroup->GetParentGroup();
        if (poGroup)
        {
            poAttrZarrConventions = poGroup->GetAttribute("zarr_conventions");
            poAttrMultiscales = poGroup->GetAttribute("multiscales");
        }
        if (!poAttrZarrConventions || !poAttrMultiscales)
        {
            return;
        }
    }

    const char *pszZarrConventions = poAttrZarrConventions->ReadAsString();
    const char *pszMultiscales = poAttrMultiscales->ReadAsString();
    if (!pszZarrConventions || !pszMultiscales)
        return;

    CPLJSONDocument oDoc;
    if (!oDoc.LoadMemory(pszZarrConventions))
        return;
    const auto oZarrConventions = oDoc.GetRoot();

    if (!oDoc.LoadMemory(pszMultiscales))
        return;
    const auto oMultiscales = oDoc.GetRoot();

    if (!oZarrConventions.IsValid() ||
        oZarrConventions.GetType() != CPLJSONObject::Type::Array ||
        !oMultiscales.IsValid() ||
        oMultiscales.GetType() != CPLJSONObject::Type::Object)
    {
        return;
    }

    const auto oZarrConventionsArray = oZarrConventions.ToArray();
    const auto hasMultiscalesUUIDLambda = [](const CPLJSONObject &obj)
    { return obj.GetString("uuid") == ZARR_MULTISCALES_UUID; };
    const bool bFoundMultiScalesUUID =
        std::find_if(oZarrConventionsArray.begin(), oZarrConventionsArray.end(),
                     hasMultiscalesUUIDLambda) != oZarrConventionsArray.end();
    if (!bFoundMultiScalesUUID)
        return;

    const auto hasSpatialUUIDLambda = [](const CPLJSONObject &obj)
    {
        constexpr const char *SPATIAL_UUID =
            "689b58e2-cf7b-45e0-9fff-9cfc0883d6b4";
        return obj.GetString("uuid") == SPATIAL_UUID;
    };
    const bool bFoundSpatialUUID =
        std::find_if(oZarrConventionsArray.begin(), oZarrConventionsArray.end(),
                     hasSpatialUUIDLambda) != oZarrConventionsArray.end();

    const auto oLayout = oMultiscales["layout"];
    if (!oLayout.IsValid() && oLayout.GetType() != CPLJSONObject::Type::Array)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "layout not found in multiscales");
        return;
    }

    // is pixel-is-area ?
    auto poSpatialRegistration = poGroup->GetAttribute("spatial:registration");
    const char *pszSpatialRegistration =
        poSpatialRegistration ? poSpatialRegistration->ReadAsString() : nullptr;
    const bool bHasExplicitPixelSpatialRegistration =
        bFoundSpatialUUID && pszSpatialRegistration &&
        strcmp(pszSpatialRegistration, "pixel") == 0;

    std::vector<std::string> aosSpatialDimensions;
    std::set<std::string> oSetSpatialDimensionNames;
    auto poSpatialDimensions = poGroup->GetAttribute("spatial:dimensions");
    if (bFoundSpatialUUID && poSpatialDimensions)
    {
        aosSpatialDimensions = poSpatialDimensions->ReadAsStringArray();
        for (const auto &osDimName : aosSpatialDimensions)
        {
            oSetSpatialDimensionNames.insert(osDimName);
        }
    }

    // Multiscales convention: asset/derived_from paths are relative to
    // the group holding the convention metadata, not the store root.
    const std::string osGroupPrefix = poGroup->GetFullName().back() == '/'
                                          ? poGroup->GetFullName()
                                          : poGroup->GetFullName() + '/';
    const auto resolveAssetPath =
        [&osGroupPrefix](const std::string &osRelative) -> std::string
    { return osGroupPrefix + osRelative; };

    // Check whether this multiscales describes our array's pyramid.
    // The first layout entry is the base (full-resolution) level; its
    // "asset" field identifies the target array.  If it refers to a
    // different array, the entire layout is irrelevant to us.
    {
        const auto oFirstItem = oLayout.ToArray()[0];
        const std::string osBaseAsset = oFirstItem.GetString("asset");
        if (!osBaseAsset.empty())
        {
            std::shared_ptr<GDALGroup> poBaseGroup;
            {
                CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                poBaseGroup =
                    poRG->OpenGroupFromFullname(resolveAssetPath(osBaseAsset));
            }
            if (poBaseGroup)
            {
                // Group-based layout (e.g. OME-Zarr "0", "1", ...):
                // skip if the base group has no array with our name.
                CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                if (!poBaseGroup->OpenMDArray(GetName()))
                    return;
            }
            else
            {
                // Extract array name from path (e.g. "level0/ar" -> "ar").
                const auto nSlash = osBaseAsset.rfind('/');
                const std::string osArrayName =
                    nSlash != std::string::npos ? osBaseAsset.substr(nSlash + 1)
                                                : osBaseAsset;
                if (osArrayName != GetName())
                    return;
            }
        }
    }

    for (const auto &oLayoutItem : oLayout.ToArray())
    {
        const std::string osAsset = oLayoutItem.GetString("asset");
        if (osAsset.empty())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "multiscales.layout[].asset not found");
            continue;
        }

        // Resolve "asset" to a MDArray
        std::shared_ptr<GDALGroup> poAssetGroup;
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            poAssetGroup =
                poRG->OpenGroupFromFullname(resolveAssetPath(osAsset));
        }
        std::shared_ptr<GDALMDArray> poAssetArray;
        if (poAssetGroup)
        {
            poAssetArray = poAssetGroup->OpenMDArray(GetName());
        }
        else if (osAsset.find('/') == std::string::npos)
        {
            poAssetArray = poGroup->OpenMDArray(osAsset);
            if (!poAssetArray)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "multiscales.layout[].asset=%s ignored, because it is "
                         "not a valid group or array name",
                         osAsset.c_str());
                continue;
            }
        }
        else
        {
            poAssetArray =
                poRG->OpenMDArrayFromFullname(resolveAssetPath(osAsset));
            if (poAssetArray && poAssetArray->GetName() != GetName())
            {
                continue;
            }
        }
        if (!poAssetArray)
        {
            continue;
        }
        if (poAssetArray->GetDimensionCount() != GetDimensionCount())
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "multiscales.layout[].asset=%s (%s) ignored, because it  has "
                "not the same dimension count as %s (%s)",
                osAsset.c_str(), poAssetArray->GetFullName().c_str(),
                GetName().c_str(), GetFullName().c_str());
            continue;
        }
        if (poAssetArray->GetDataType() != GetDataType())
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "multiscales.layout[].asset=%s (%s) ignored, because it has "
                "not the same data type as %s (%s)",
                osAsset.c_str(), poAssetArray->GetFullName().c_str(),
                GetName().c_str(), GetFullName().c_str());
            continue;
        }

        bool bAssetIsDownsampledOfThis = false;
        for (size_t iDim = 0; iDim < GetDimensionCount(); ++iDim)
        {
            if (poAssetArray->GetDimensions()[iDim]->GetSize() <
                GetDimensions()[iDim]->GetSize())
            {
                bAssetIsDownsampledOfThis = true;
                break;
            }
        }
        if (!bAssetIsDownsampledOfThis)
        {
            // not an error
            continue;
        }

        // Inspect dimensions of the asset
        std::map<std::string, size_t> oMapAssetDimNameToIdx;
        const auto &apoAssetDims = poAssetArray->GetDimensions();
        size_t nCountSpatialDimsFoundInAsset = 0;
        for (const auto &[idx, poDim] : cpl::enumerate(apoAssetDims))
        {
            oMapAssetDimNameToIdx[poDim->GetName()] = idx;
            if (cpl::contains(oSetSpatialDimensionNames, poDim->GetName()))
                ++nCountSpatialDimsFoundInAsset;
        }
        const bool bAssetHasAllSpatialDims =
            (nCountSpatialDimsFoundInAsset == aosSpatialDimensions.size());

        // Consistency checks on "derived_from" and "transform"
        const auto oDerivedFrom = oLayoutItem["derived_from"];
        const auto oTransform = oLayoutItem["transform"];
        if (oDerivedFrom.IsValid() && oTransform.IsValid() &&
            oDerivedFrom.GetType() == CPLJSONObject::Type::String &&
            oTransform.GetType() == CPLJSONObject::Type::Object)
        {
            const std::string osDerivedFrom = oDerivedFrom.ToString();
            // Resolve "derived_from" to a MDArray
            std::shared_ptr<GDALGroup> poDerivedFromGroup;
            {
                CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                poDerivedFromGroup = poRG->OpenGroupFromFullname(
                    resolveAssetPath(osDerivedFrom));
            }
            std::shared_ptr<GDALMDArray> poDerivedFromArray;
            if (poDerivedFromGroup)
            {
                poDerivedFromArray = poDerivedFromGroup->OpenMDArray(GetName());
            }
            else if (osDerivedFrom.find('/') == std::string::npos)
            {
                poDerivedFromArray = poGroup->OpenMDArray(osDerivedFrom);
                if (!poDerivedFromArray)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "multiscales.layout[].asset=%s refers to "
                             "derived_from=%s which does not exist",
                             osAsset.c_str(), osDerivedFrom.c_str());
                    poDerivedFromArray.reset();
                }
            }
            else
            {
                poDerivedFromArray = poRG->OpenMDArrayFromFullname(
                    resolveAssetPath(osDerivedFrom));
            }
            if (poDerivedFromArray && bAssetHasAllSpatialDims)
            {
                if (poDerivedFromArray->GetDimensionCount() !=
                    GetDimensionCount())
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "multiscales.layout[].asset=%s refers to "
                             "derived_from=%s that does not have the expected "
                             "number of dimensions. Ignoring that asset",
                             osAsset.c_str(), osDerivedFrom.c_str());
                    continue;
                }

                const auto oScale = oTransform["scale"];
                if (oScale.GetType() == CPLJSONObject::Type::Array &&
                    bHasExplicitPixelSpatialRegistration)
                {
                    const auto oScaleArray = oScale.ToArray();
                    if (oScaleArray.size() != GetDimensionCount())
                    {

                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "multiscales.layout[].asset=%s has a "
                                 "transform.scale array with an unexpected "
                                 "number of values. Ignoring the asset",
                                 osAsset.c_str());
                        continue;
                    }

                    for (size_t iDim = 0; iDim < GetDimensionCount(); ++iDim)
                    {
                        const double dfScale = oScaleArray[iDim].ToDouble();
                        const double dfExpectedScale =
                            static_cast<double>(
                                poDerivedFromArray->GetDimensions()[iDim]
                                    ->GetSize()) /
                            static_cast<double>(
                                poAssetArray->GetDimensions()[iDim]->GetSize());
                        constexpr double EPSILON = 1e-3;
                        if (std::fabs(dfScale - dfExpectedScale) >
                            EPSILON * dfExpectedScale)
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "multiscales.layout[].asset=%s has a "
                                     "transform.scale[%d]=%f value whereas %f "
                                     "was expected. "
                                     "Assuming that later value as the scale.",
                                     osAsset.c_str(), static_cast<int>(iDim),
                                     dfScale, dfExpectedScale);
                        }
                    }
                }

                const auto oTranslation = oTransform["translation"];
                if (oTranslation.GetType() == CPLJSONObject::Type::Array &&
                    bHasExplicitPixelSpatialRegistration)
                {
                    const auto oTranslationArray = oTranslation.ToArray();
                    if (oTranslationArray.size() != GetDimensionCount())
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "multiscales.layout[].asset=%s has a "
                                 "transform.translation array with an "
                                 "unexpected number of values. "
                                 "Ignoring the asset",
                                 osAsset.c_str());
                        continue;
                    }

                    for (size_t iDim = 0; iDim < GetDimensionCount(); ++iDim)
                    {
                        const double dfOffset =
                            oTranslationArray[iDim].ToDouble();
                        if (dfOffset != 0)
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "multiscales.layout[].asset=%s has a "
                                     "transform.translation[%d]=%f value. "
                                     "Ignoring that offset.",
                                     osAsset.c_str(), static_cast<int>(iDim),
                                     dfOffset);
                        }
                    }
                }
            }
        }

        if (bFoundSpatialUUID && bAssetHasAllSpatialDims)
        {
            const auto oSpatialShape = oLayoutItem["spatial:shape"];
            if (oSpatialShape.IsValid())
            {
                if (oSpatialShape.GetType() != CPLJSONObject::Type::Array)
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "multiscales.layout[].asset=%s ignored, because its "
                        "spatial:shape property is not an array",
                        osAsset.c_str());
                    continue;
                }
                const auto oSpatialShapeArray = oSpatialShape.ToArray();
                if (oSpatialShapeArray.size() != aosSpatialDimensions.size())
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "multiscales.layout[].asset=%s ignored, because its "
                        "spatial:shape property has not the expected number "
                        "of values",
                        osAsset.c_str());
                    continue;
                }

                bool bSkip = false;
                for (const auto &[idx, oShapeVal] :
                     cpl::enumerate(oSpatialShapeArray))
                {
                    const auto oIter =
                        oMapAssetDimNameToIdx.find(aosSpatialDimensions[idx]);
                    if (oIter != oMapAssetDimNameToIdx.end())
                    {
                        const auto poDim = apoAssetDims[oIter->second];
                        if (poDim->GetSize() !=
                            static_cast<uint64_t>(oShapeVal.ToLong()))
                        {
                            bSkip = true;
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "multiscales.layout[].asset=%s ignored, "
                                     "because its "
                                     "spatial:shape[%d] value is %" PRIu64
                                     " whereas %" PRIu64 " was expected.",
                                     osAsset.c_str(), static_cast<int>(idx),
                                     static_cast<uint64_t>(oShapeVal.ToLong()),
                                     static_cast<uint64_t>(poDim->GetSize()));
                        }
                    }
                }
                if (bSkip)
                    continue;
            }
        }

        m_apoOverviews.push_back(std::move(poAssetArray));
    }
}

/************************************************************************/
/*                   ZarrV3Array::GetOverviewCount()                    */
/************************************************************************/

int ZarrV3Array::GetOverviewCount() const
{
    LoadOverviews();
    return static_cast<int>(m_apoOverviews.size());
}

/************************************************************************/
/*                      ZarrV3Array::GetOverview()                      */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrV3Array::GetOverview(int idx) const
{
    if (idx < 0 || idx >= GetOverviewCount())
        return nullptr;
    return m_apoOverviews[idx];
}

/************************************************************************/
/*         ZarrV3Array::ReconstructCreationOptionsFromCodecs()          */
/************************************************************************/

// When an array is opened from disk (LoadArray), m_aosCreationOptions is
// empty because SetCreationOptions() is only called during CreateMDArray().
// BuildOverviews() needs the creation options so that overview arrays
// inherit the same codec (compression, sharding).  This method reverse-maps
// the stored codec JSON back to creation option key/value pairs.

void ZarrV3Array::ReconstructCreationOptionsFromCodecs()
{
    if (!m_poCodecs || m_aosCreationOptions.FetchNameValue("COMPRESS"))
        return;

    CPLJSONArray oCodecArray = m_poCodecs->GetJSon().ToArray();

    // Detect sharding: if the sole top-level codec is sharding_indexed,
    // extract SHARD_CHUNK_SHAPE and use the inner codecs for compression.
    for (int i = 0; i < oCodecArray.Size(); ++i)
    {
        const auto oCodec = oCodecArray[i];
        if (oCodec.GetString("name") == "sharding_indexed")
        {
            const auto oConfig = oCodec["configuration"];

            // Inner chunk shape
            const auto oChunkShape = oConfig.GetArray("chunk_shape");
            if (oChunkShape.IsValid() && oChunkShape.Size() > 0)
            {
                std::string osShape;
                for (int j = 0; j < oChunkShape.Size(); ++j)
                {
                    if (!osShape.empty())
                        osShape += ',';
                    osShape += CPLSPrintf(
                        CPL_FRMT_GUIB,
                        static_cast<GUIntBig>(oChunkShape[j].ToLong()));
                }
                m_aosCreationOptions.SetNameValue("SHARD_CHUNK_SHAPE",
                                                  osShape.c_str());
            }

            // Use inner codecs for compression detection
            oCodecArray = oConfig.GetArray("codecs");
            break;
        }
    }

    // Scan codecs for compression algorithm
    for (int i = 0; i < oCodecArray.Size(); ++i)
    {
        const auto oCodec = oCodecArray[i];
        const auto osName = oCodec.GetString("name");
        const auto oConfig = oCodec["configuration"];

        if (osName == "gzip")
        {
            m_aosCreationOptions.SetNameValue("COMPRESS", "GZIP");
            if (oConfig.IsValid())
            {
                m_aosCreationOptions.SetNameValue(
                    "GZIP_LEVEL",
                    CPLSPrintf("%d", oConfig.GetInteger("level")));
            }
        }
        else if (osName == "zstd")
        {
            m_aosCreationOptions.SetNameValue("COMPRESS", "ZSTD");
            if (oConfig.IsValid())
            {
                m_aosCreationOptions.SetNameValue(
                    "ZSTD_LEVEL",
                    CPLSPrintf("%d", oConfig.GetInteger("level")));
                if (oConfig.GetBool("checksum"))
                    m_aosCreationOptions.SetNameValue("ZSTD_CHECKSUM", "YES");
            }
        }
        else if (osName == "blosc")
        {
            m_aosCreationOptions.SetNameValue("COMPRESS", "BLOSC");
            if (oConfig.IsValid())
            {
                const auto osCName = oConfig.GetString("cname");
                if (!osCName.empty())
                    m_aosCreationOptions.SetNameValue("BLOSC_CNAME",
                                                      osCName.c_str());
                m_aosCreationOptions.SetNameValue(
                    "BLOSC_CLEVEL",
                    CPLSPrintf("%d", oConfig.GetInteger("clevel")));
                const auto osShuffle = oConfig.GetString("shuffle");
                if (osShuffle == "noshuffle")
                    m_aosCreationOptions.SetNameValue("BLOSC_SHUFFLE", "NONE");
                else if (osShuffle == "bitshuffle")
                    m_aosCreationOptions.SetNameValue("BLOSC_SHUFFLE", "BIT");
                else
                    m_aosCreationOptions.SetNameValue("BLOSC_SHUFFLE", "BYTE");
            }
        }
    }
}

/************************************************************************/
/*                    ZarrV3Array::BuildOverviews()                     */
/************************************************************************/

CPLErr ZarrV3Array::BuildOverviews(const char *pszResampling, int nOverviews,
                                   const int *panOverviewList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList /* papszOptions */)
{
    const size_t nDimCount = GetDimensionCount();
    if (nDimCount < 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "BuildOverviews() requires at least 2 dimensions");
        return CE_Failure;
    }

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    auto poParentGroup =
        std::static_pointer_cast<ZarrV3Group>(GetParentGroup());
    if (!poParentGroup)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot access parent group");
        return CE_Failure;
    }

    if (!pfnProgress)
        pfnProgress = GDALDummyProgress;

    // Identify spatial dimensions via GDALDimension::GetType().
    // Fall back to last two dimensions (Y, X) if types are not set.
    const auto &apoSrcDims = GetDimensions();
    size_t iYDim = nDimCount - 2;
    size_t iXDim = nDimCount - 1;

    for (size_t i = 0; i < nDimCount; ++i)
    {
        if (apoSrcDims[i]->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X)
            iXDim = i;
        else if (apoSrcDims[i]->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y)
            iYDim = i;
    }

    if (iXDim == iYDim)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot identify two distinct spatial dimensions. "
                 "Set dimension types (HORIZONTAL_X / HORIZONTAL_Y) "
                 "or ensure the array has at least 2 dimensions.");
        return CE_Failure;
    }

    // Delete existing overview groups (ovr_*) for idempotent rebuild.
    // Also handles nOverviews==0 ("clear overviews").
    for (const auto &osName : poParentGroup->GetGroupNames())
    {
        if (STARTS_WITH(osName.c_str(), "ovr_") &&
            !poParentGroup->DeleteGroup(osName))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot delete existing overview group '%s'",
                     osName.c_str());
            return CE_Failure;
        }
    }

    if (nOverviews == 0)
    {
        poParentGroup->GenerateMultiscalesMetadata(nullptr);
        m_bOverviewsLoaded = false;
        m_apoOverviews.clear();
        return CE_None;
    }

    if (nOverviews < 0 || !panOverviewList)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Invalid overview list");
        return CE_Failure;
    }

    if (!pszResampling || pszResampling[0] == '\0')
        pszResampling = "NEAREST";

    // Sort and deduplicate factors for sequential resampling chain.
    std::vector<int> anFactors(panOverviewList, panOverviewList + nOverviews);
    std::sort(anFactors.begin(), anFactors.end());
    anFactors.erase(std::unique(anFactors.begin(), anFactors.end()),
                    anFactors.end());
    for (const int nFactor : anFactors)
    {
        if (nFactor < 2)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Overview factor %d is invalid (must be >= 2)", nFactor);
            return CE_Failure;
        }
    }

    // Ensure creation options are populated (they are empty when the array
    // was opened from disk rather than freshly created).
    ReconstructCreationOptionsFromCodecs();

    // Inherit creation options from source array (codec settings, etc.).
    // Only override BLOCKSIZE and SHARD_CHUNK_SHAPE per level.
    CPLStringList aosCreateOptions(m_aosCreationOptions);

    const std::string &osArrayName = GetName();
    const void *pRawNoData = GetRawNoDataValue();

    // Build each level sequentially: 2x from base, 4x from 2x, etc.
    // poChainSource starts as the base array and advances to each
    // newly created overview so each level resamples from the previous.
    std::shared_ptr<GDALMDArray> poChainSource =
        std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if (!poChainSource)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot obtain shared_ptr to self");
        return CE_Failure;
    }

    // Pre-compute total output pixels for pixel-weighted progress.
    double dfTotalPixels = 0.0;
    for (const int nF : anFactors)
    {
        dfTotalPixels += static_cast<double>(
                             DIV_ROUND_UP(apoSrcDims[iYDim]->GetSize(), nF)) *
                         DIV_ROUND_UP(apoSrcDims[iXDim]->GetSize(), nF);
    }
    double dfPixelsProcessed = 0.0;

    const int nFactorCount = static_cast<int>(anFactors.size());
    for (int iOvr = 0; iOvr < nFactorCount; ++iOvr)
    {
        const int nFactor = anFactors[iOvr];

        // Create sibling group for this overview level.
        const std::string osGroupName = CPLSPrintf("ovr_%dx", nFactor);
        auto poOvrGroup = poParentGroup->CreateGroup(osGroupName);
        if (!poOvrGroup)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot create group '%s'",
                     osGroupName.c_str());
            return CE_Failure;
        }

        // Create dimensions: downsample spatial, preserve non-spatial.
        std::vector<std::shared_ptr<GDALDimension>> aoOvrDims;
        std::string osBlockSize;
        for (size_t i = 0; i < nDimCount; ++i)
        {
            const bool bSpatial = (i == iYDim || i == iXDim);
            const GUInt64 nSrcSize = apoSrcDims[i]->GetSize();
            const GUInt64 nOvrSize =
                bSpatial ? DIV_ROUND_UP(nSrcSize, nFactor) : nSrcSize;

            auto poDim = poOvrGroup->CreateDimension(
                apoSrcDims[i]->GetName(), apoSrcDims[i]->GetType(),
                apoSrcDims[i]->GetDirection(), nOvrSize);
            if (!poDim)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot create dimension '%s' in group '%s'",
                         apoSrcDims[i]->GetName().c_str(), osGroupName.c_str());
                return CE_Failure;
            }
            aoOvrDims.push_back(std::move(poDim));

            // Block size: inherit from source, cap to overview dim size.
            const GUInt64 nBlock = std::min(m_anOuterBlockSize[i], nOvrSize);
            if (!osBlockSize.empty())
                osBlockSize += ',';
            osBlockSize +=
                CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nBlock));

            // Build 1D coordinate array for spatial dimensions.
            if (bSpatial)
            {
                auto poSrcVar = apoSrcDims[i]->GetIndexingVariable();
                if (poSrcVar && poSrcVar->GetDimensionCount() == 1)
                {
                    if (nOvrSize > 100 * 1000 * 1000)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Overview dimension too large for "
                                 "coordinate array");
                        return CE_Failure;
                    }
                    const size_t nOvrCount = static_cast<size_t>(nOvrSize);
                    auto poCoordArray = poOvrGroup->CreateMDArray(
                        poSrcVar->GetName(), {aoOvrDims.back()},
                        poSrcVar->GetDataType());
                    if (poCoordArray)
                    {
                        std::vector<double> adfValues;
                        try
                        {
                            adfValues.resize(nOvrCount);
                        }
                        catch (const std::exception &)
                        {
                            CPLError(CE_Failure, CPLE_OutOfMemory,
                                     "Cannot allocate coordinate array");
                            return CE_Failure;
                        }
                        double dfStart = 0;
                        double dfIncrement = 0;
                        if (poSrcVar->IsRegularlySpaced(dfStart, dfIncrement))
                        {
                            // Recalculate from spacing: overview pixels are
                            // centered at (j * factor + (factor-1)/2.0) in
                            // source pixel space.
                            for (size_t j = 0; j < nOvrCount; ++j)
                            {
                                adfValues[j] =
                                    dfStart +
                                    (static_cast<double>(j) * nFactor +
                                     (nFactor - 1) / 2.0) *
                                        dfIncrement;
                            }
                        }
                        else
                        {
                            // Irregular spacing: subsample by stride.
                            if (nSrcSize > 100 * 1000 * 1000)
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Source dimension too large "
                                         "for coordinate array");
                                return CE_Failure;
                            }
                            const size_t nSrcCount =
                                static_cast<size_t>(nSrcSize);
                            std::vector<double> adfSrc;
                            try
                            {
                                adfSrc.resize(nSrcCount);
                            }
                            catch (const std::exception &)
                            {
                                CPLError(CE_Failure, CPLE_OutOfMemory,
                                         "Cannot allocate source "
                                         "coordinate array");
                                return CE_Failure;
                            }
                            const GUInt64 anSrcStart[1] = {0};
                            const size_t anSrcCount[1] = {nSrcCount};
                            if (!poSrcVar->Read(
                                    anSrcStart, anSrcCount, nullptr, nullptr,
                                    GDALExtendedDataType::Create(GDT_Float64),
                                    adfSrc.data()))
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Failed to read coordinate "
                                         "variable '%s'",
                                         poSrcVar->GetName().c_str());
                                return CE_Failure;
                            }
                            for (size_t j = 0; j < nOvrCount; ++j)
                            {
                                // Pick the source index closest to the
                                // overview pixel center.
                                const size_t nSrcIdx = std::min(
                                    static_cast<size_t>(static_cast<double>(j) *
                                                            nFactor +
                                                        nFactor / 2),
                                    nSrcCount - 1);
                                adfValues[j] = adfSrc[nSrcIdx];
                            }
                        }
                        const GUInt64 anStart[1] = {0};
                        const size_t anCount[1] = {nOvrCount};
                        if (!poCoordArray->Write(
                                anStart, anCount, nullptr, nullptr,
                                GDALExtendedDataType::Create(GDT_Float64),
                                adfValues.data()))
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Failed to write coordinate "
                                     "variable for overview");
                            return CE_Failure;
                        }
                        aoOvrDims.back()->SetIndexingVariable(
                            std::move(poCoordArray));
                    }
                }
            }
        }
        aosCreateOptions.SetNameValue("BLOCKSIZE", osBlockSize.c_str());

        // Validate SHARD_CHUNK_SHAPE: inner chunks must divide
        // the (capped) block size evenly. Drop sharding if not.
        const char *pszShardShape =
            aosCreateOptions.FetchNameValue("SHARD_CHUNK_SHAPE");
        if (pszShardShape)
        {
            const CPLStringList aosShard(
                CSLTokenizeString2(pszShardShape, ",", 0));
            const CPLStringList aosBlock(
                CSLTokenizeString2(osBlockSize.c_str(), ",", 0));
            bool bShardValid = (aosShard.size() == aosBlock.size());
            for (int iDim = 0; bShardValid && iDim < aosShard.size(); ++iDim)
            {
                const auto nInner = static_cast<GUInt64>(atoll(aosShard[iDim]));
                const auto nOuter = static_cast<GUInt64>(atoll(aosBlock[iDim]));
                if (nInner == 0 || nOuter < nInner || nOuter % nInner != 0)
                    bShardValid = false;
            }
            if (!bShardValid)
                aosCreateOptions.SetNameValue("SHARD_CHUNK_SHAPE", nullptr);
        }

        auto poOvrArray = poOvrGroup->CreateMDArray(
            osArrayName, aoOvrDims, GetDataType(), aosCreateOptions.List());
        if (!poOvrArray)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create overview array for factor %d", nFactor);
            return CE_Failure;
        }

        if (pRawNoData)
            poOvrArray->SetRawNoDataValue(pRawNoData);

        // Wrap as classic datasets for GDALRegenerateOverviews.
        // Non-spatial dims become bands automatically.
        std::unique_ptr<GDALDataset> poPrevDS(
            poChainSource->AsClassicDataset(iXDim, iYDim));
        std::unique_ptr<GDALDataset> poOvrDS(
            poOvrArray->AsClassicDataset(iXDim, iYDim));
        if (!poPrevDS || !poOvrDS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create classic dataset wrapper for resampling");
            return CE_Failure;
        }

        // Resample all bands from previous level into this overview.
        const int nBands = poPrevDS->GetRasterCount();
        const double dfLevelPixels =
            static_cast<double>(poOvrDS->GetRasterXSize()) *
            poOvrDS->GetRasterYSize();
        void *pLevelData = GDALCreateScaledProgress(
            dfPixelsProcessed / dfTotalPixels,
            (dfPixelsProcessed + dfLevelPixels) / dfTotalPixels, pfnProgress,
            pProgressData);

        CPLErr eErr = CE_None;
        for (int iBand = 1; iBand <= nBands && eErr == CE_None; ++iBand)
        {
            const double dfBandBase = static_cast<double>(iBand - 1) / nBands;
            const double dfBandEnd = static_cast<double>(iBand) / nBands;
            void *pBandData = GDALCreateScaledProgress(
                dfBandBase, dfBandEnd, GDALScaledProgress, pLevelData);

            GDALRasterBandH hOvrBand =
                GDALRasterBand::ToHandle(poOvrDS->GetRasterBand(iBand));
            eErr = GDALRegenerateOverviews(
                GDALRasterBand::ToHandle(poPrevDS->GetRasterBand(iBand)), 1,
                &hOvrBand, pszResampling, GDALScaledProgress, pBandData);

            GDALDestroyScaledProgress(pBandData);
        }

        GDALDestroyScaledProgress(pLevelData);
        dfPixelsProcessed += dfLevelPixels;

        if (eErr != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALRegenerateOverviews failed for factor %d", nFactor);
            return CE_Failure;
        }

        poChainSource = std::move(poOvrArray);
    }

    // Write multiscales metadata on parent group.
    poParentGroup->GenerateMultiscalesMetadata(pszResampling);

    // Reset overview cache so GetOverviewCount() rediscovers.
    m_bOverviewsLoaded = false;
    m_apoOverviews.clear();

    return CE_None;
}
