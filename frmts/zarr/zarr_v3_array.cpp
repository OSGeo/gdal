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

    bool ret = ZarrV3Array::FlushDirtyBlock();

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
        if (elt.needByteSwapping || elt.gdalTypeIsApproxOfNative)
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

    // For each shard with >1 uncached block, batch-read
    const char *const apszOpenOptions[] = {"IGNORE_FILENAME_RESTRICTIONS=YES",
                                           nullptr};

    for (auto &[osFilename, aBlocks] : oShardToBlocks)
    {
        if (aBlocks.size() <= 1)
            continue;

        VSIVirtualHandleUniquePtr fp(
            VSIFOpenEx2L(osFilename.c_str(), "rb", 0, apszOpenOptions));
        if (!fp)
            continue;

        // Build request list
        std::vector<std::pair<std::vector<size_t>, std::vector<size_t>>>
            anRequests;
        anRequests.reserve(aBlocks.size());
        for (const auto &info : aBlocks)
        {
            anRequests.push_back({info.anStartIdx, info.anCount});
        }

        std::vector<ZarrByteVectorQuickResize> aResults;
        if (!m_poCodecs->BatchDecodePartial(fp.get(), anRequests, aResults))
            continue;

        // Store results in block cache
        const bool bNeedDecode = NeedDecodedBuffer();
        for (size_t i = 0; i < aBlocks.size(); ++i)
        {
            if (aResults[i].empty())
            {
                CachedBlock cachedBlock;
                m_oChunkCache[aBlocks[i].anBlockIndices] =
                    std::move(cachedBlock);
                continue;
            }

            CachedBlock cachedBlock;
            if (bNeedDecode)
            {
                const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                                           m_aoDtypeElts.back().nativeSize;
                const auto nGDALDTSize = m_oType.GetSize();
                const size_t nValues = aResults[i].size() / nSourceSize;
                ZarrByteVectorQuickResize abyDecoded;
                abyDecoded.resize(nValues * nGDALDTSize);
                const GByte *pSrc = aResults[i].data();
                GByte *pDst = abyDecoded.data();
                for (size_t v = 0; v < nValues;
                     v++, pSrc += nSourceSize, pDst += nGDALDTSize)
                {
                    DecodeSourceElt(m_aoDtypeElts, pSrc, pDst);
                }
                std::swap(cachedBlock.abyDecoded, abyDecoded);
            }
            else
            {
                std::swap(cachedBlock.abyDecoded, aResults[i]);
            }
            m_oChunkCache[aBlocks[i].anBlockIndices] = std::move(cachedBlock);
        }
    }
}

/************************************************************************/
/*                      ZarrV3Array::IAdviseRead()                      */
/************************************************************************/

bool ZarrV3Array::IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                              CSLConstList papszOptions) const
{
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
/*                        ZarrV3Array::IWrite()                         */
/************************************************************************/

bool ZarrV3Array::IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                         const GInt64 *arrayStep,
                         const GPtrDiff_t *bufferStride,
                         const GDALExtendedDataType &bufferDataType,
                         const void *pSrcBuffer)
{
    if (m_poCodecs && m_poCodecs->SupportsPartialDecoding())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Writing to sharded dataset is not supported");
        return false;
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
        if (STARTS_WITH(osFillValue.c_str(), "0x"))
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
            bool bOK = true;
            double dfNoDataValue = ParseNoDataStringAsDouble(osFillValue, bOK);
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
                memcpy(&abyNoData[0], &hfNoDataValue, sizeof(hfNoDataValue));
            }
            else if (oType.GetNumericDataType() == GDT_Float32)
            {
                const float fNoDataValue = static_cast<float>(dfNoDataValue);
                abyNoData.resize(sizeof(fNoDataValue));
                memcpy(&abyNoData[0], &fNoDataValue, sizeof(fNoDataValue));
            }
            else if (oType.GetNumericDataType() == GDT_Float64)
            {
                abyNoData.resize(sizeof(dfNoDataValue));
                memcpy(&abyNoData[0], &dfNoDataValue, sizeof(dfNoDataValue));
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid fill_value for this data type");
                return nullptr;
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
    {
        constexpr const char *MULTISCALES_UUID =
            "d35379db-88df-4056-af3a-620245f8e347";
        return obj.GetString("uuid") == MULTISCALES_UUID;
    };
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
