/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_vsi_virtual.h"
#include "gdal_thread_pool.h"
#include "zarr.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

/************************************************************************/
/*                       ZarrV3Array::ZarrV3Array()                     */
/************************************************************************/

ZarrV3Array::ZarrV3Array(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const GDALExtendedDataType &oType, const std::vector<DtypeElt> &aoDtypeElts,
    const std::vector<GUInt64> &anBlockSize)
    : GDALAbstractMDArray(osParentName, osName),
      ZarrArray(poSharedResource, osParentName, osName, aoDims, oType,
                aoDtypeElts, anBlockSize)
{
}

/************************************************************************/
/*                         ZarrV3Array::Create()                        */
/************************************************************************/

std::shared_ptr<ZarrV3Array>
ZarrV3Array::Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                    const std::string &osParentName, const std::string &osName,
                    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                    const GDALExtendedDataType &oType,
                    const std::vector<DtypeElt> &aoDtypeElts,
                    const std::vector<GUInt64> &anBlockSize)
{
    auto arr = std::shared_ptr<ZarrV3Array>(
        new ZarrV3Array(poSharedResource, osParentName, osName, aoDims, oType,
                        aoDtypeElts, anBlockSize));
    if (arr->m_nTotalTileCount == 0)
        return nullptr;
    arr->SetSelf(arr);

    return arr;
}

/************************************************************************/
/*                             ~ZarrV3Array()                           */
/************************************************************************/

ZarrV3Array::~ZarrV3Array()
{
    ZarrV3Array::Flush();
}

/************************************************************************/
/*                                Flush()                               */
/************************************************************************/

void ZarrV3Array::Flush()
{
    if (!m_bValid)
        return;

    ZarrV3Array::FlushDirtyTile();

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
        Serialize(oAttrs);
        m_bDefinitionModified = false;
    }
}

/************************************************************************/
/*                    ZarrV3Array::Serialize()                          */
/************************************************************************/

void ZarrV3Array::Serialize(const CPLJSONObject &oAttrs)
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
        for (const auto nBlockSize : m_anBlockSize)
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
        if (m_oType.GetNumericDataType() == GDT_Float32 ||
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
        if (m_oType.GetNumericDataType() == GDT_CFloat32 ||
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

    oDoc.Save(m_osFilename);
}

/************************************************************************/
/*                  ZarrV3Array::NeedDecodedBuffer()                    */
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
/*               ZarrV3Array::AllocateWorkingBuffers()                  */
/************************************************************************/

bool ZarrV3Array::AllocateWorkingBuffers() const
{
    if (m_bAllocateWorkingBuffersDone)
        return m_bWorkingBuffersOK;

    m_bAllocateWorkingBuffersDone = true;

    size_t nSizeNeeded = m_nTileSize;
    if (NeedDecodedBuffer())
    {
        size_t nDecodedBufferSize = m_oType.GetSize();
        for (const auto &nBlockSize : m_anBlockSize)
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
        AllocateWorkingBuffers(m_abyRawTileData, m_abyDecodedTileData);
    return m_bWorkingBuffersOK;
}

bool ZarrV3Array::AllocateWorkingBuffers(
    ZarrByteVectorQuickResize &abyRawTileData,
    ZarrByteVectorQuickResize &abyDecodedTileData) const
{
    // This method should NOT modify any ZarrArray member, as it is going to
    // be called concurrently from several threads.

    // Set those #define to avoid accidental use of some global variables
#define m_abyRawTileData cannot_use_here
#define m_abyDecodedTileData cannot_use_here

    try
    {
        abyRawTileData.resize(m_nTileSize);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    if (NeedDecodedBuffer())
    {
        size_t nDecodedBufferSize = m_oType.GetSize();
        for (const auto &nBlockSize : m_anBlockSize)
        {
            nDecodedBufferSize *= static_cast<size_t>(nBlockSize);
        }
        try
        {
            abyDecodedTileData.resize(nDecodedBufferSize);
        }
        catch (const std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return false;
        }
    }

    return true;
#undef m_abyRawTileData
#undef m_abyDecodedTileData
}

/************************************************************************/
/*                      ZarrV3Array::LoadTileData()                     */
/************************************************************************/

bool ZarrV3Array::LoadTileData(const uint64_t *tileIndices,
                               bool &bMissingTileOut) const
{
    return LoadTileData(tileIndices,
                        false,  // use mutex
                        m_poCodecs.get(), m_abyRawTileData,
                        m_abyDecodedTileData, bMissingTileOut);
}

bool ZarrV3Array::LoadTileData(const uint64_t *tileIndices, bool bUseMutex,
                               ZarrV3CodecSequence *poCodecs,
                               ZarrByteVectorQuickResize &abyRawTileData,
                               ZarrByteVectorQuickResize &abyDecodedTileData,
                               bool &bMissingTileOut) const
{
    // This method should NOT modify any ZarrArray member, as it is going to
    // be called concurrently from several threads.

    // Set those #define to avoid accidental use of some global variables
#define m_abyRawTileData cannot_use_here
#define m_abyDecodedTileData cannot_use_here
#define m_poCodecs cannot_use_here

    bMissingTileOut = false;

    std::string osFilename = BuildTileFilename(tileIndices);

    // For network file systems, get the streaming version of the filename,
    // as we don't need arbitrary seeking in the file
    osFilename = VSIFileManager::GetHandler(osFilename.c_str())
                     ->GetStreamingFilename(osFilename);

    // First if we have a tile presence cache, check tile presence from it
    if (bUseMutex)
        m_oMutex.lock();
    auto poTilePresenceArray = OpenTilePresenceCache(false);
    if (poTilePresenceArray)
    {
        std::vector<GUInt64> anTileIdx(m_aoDims.size());
        const std::vector<size_t> anCount(m_aoDims.size(), 1);
        const std::vector<GInt64> anArrayStep(m_aoDims.size(), 0);
        const std::vector<GPtrDiff_t> anBufferStride(m_aoDims.size(), 0);
        const auto eByteDT = GDALExtendedDataType::Create(GDT_Byte);
        for (size_t i = 0; i < m_aoDims.size(); ++i)
        {
            anTileIdx[i] = static_cast<GUInt64>(tileIndices[i]);
        }
        GByte byValue = 0;
        if (poTilePresenceArray->Read(anTileIdx.data(), anCount.data(),
                                      anArrayStep.data(), anBufferStride.data(),
                                      eByteDT, &byValue) &&
            byValue == 0)
        {
            if (bUseMutex)
                m_oMutex.unlock();
            CPLDebugOnly(ZARR_DEBUG_KEY, "Tile %s missing (=nodata)",
                         osFilename.c_str());
            bMissingTileOut = true;
            return true;
        }
    }
    if (bUseMutex)
        m_oMutex.unlock();

    VSILFILE *fp = nullptr;
    // This is the number of files returned in a S3 directory listing operation
    constexpr uint64_t MAX_TILES_ALLOWED_FOR_DIRECTORY_LISTING = 1000;
    const char *const apszOpenOptions[] = {"IGNORE_FILENAME_RESTRICTIONS=YES",
                                           nullptr};
    if ((m_osDimSeparator == "/" && !m_anBlockSize.empty() &&
         m_anBlockSize.back() > MAX_TILES_ALLOWED_FOR_DIRECTORY_LISTING) ||
        (m_osDimSeparator != "/" &&
         m_nTotalTileCount > MAX_TILES_ALLOWED_FOR_DIRECTORY_LISTING))
    {
        // Avoid issuing ReadDir() when a lot of files are expected
        CPLConfigOptionSetter optionSetter("GDAL_DISABLE_READDIR_ON_OPEN",
                                           "YES", true);
        fp = VSIFOpenEx2L(osFilename.c_str(), "rb", 0, apszOpenOptions);
    }
    else
    {
        fp = VSIFOpenEx2L(osFilename.c_str(), "rb", 0, apszOpenOptions);
    }
    if (fp == nullptr)
    {
        // Missing files are OK and indicate nodata_value
        CPLDebugOnly(ZARR_DEBUG_KEY, "Tile %s missing (=nodata)",
                     osFilename.c_str());
        bMissingTileOut = true;
        return true;
    }

    bMissingTileOut = false;

    CPLAssert(abyRawTileData.capacity() >= m_nTileSize);
    // should not fail
    abyRawTileData.resize(m_nTileSize);

    bool bRet = true;
    size_t nRawDataSize = abyRawTileData.size();
    if (poCodecs == nullptr)
    {
        nRawDataSize = VSIFReadL(&abyRawTileData[0], 1, nRawDataSize, fp);
    }
    else
    {
        VSIFSeekL(fp, 0, SEEK_END);
        const auto nSize = VSIFTellL(fp);
        VSIFSeekL(fp, 0, SEEK_SET);
        if (nSize > static_cast<vsi_l_offset>(std::numeric_limits<int>::max()))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large tile %s",
                     osFilename.c_str());
            bRet = false;
        }
        else
        {
            try
            {
                abyRawTileData.resize(static_cast<size_t>(nSize));
            }
            catch (const std::exception &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate memory for tile %s",
                         osFilename.c_str());
                bRet = false;
            }

            if (bRet && (abyRawTileData.empty() ||
                         VSIFReadL(&abyRawTileData[0], 1, abyRawTileData.size(),
                                   fp) != abyRawTileData.size()))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Could not read tile %s correctly",
                         osFilename.c_str());
                bRet = false;
            }
            else
            {
                if (!poCodecs->Decode(abyRawTileData))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Decompression of tile %s failed",
                             osFilename.c_str());
                    bRet = false;
                }
            }
        }
    }
    VSIFCloseL(fp);
    if (!bRet)
        return false;

    if (nRawDataSize != abyRawTileData.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Decompressed tile %s has not expected size. "
                 "Got %u instead of %u",
                 osFilename.c_str(),
                 static_cast<unsigned>(abyRawTileData.size()),
                 static_cast<unsigned>(nRawDataSize));
        return false;
    }

    if (!abyDecodedTileData.empty())
    {
        const size_t nSourceSize =
            m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;
        const auto nDTSize = m_oType.GetSize();
        const size_t nValues = abyDecodedTileData.size() / nDTSize;
        CPLAssert(nValues == m_nTileSize / nSourceSize);
        const GByte *pSrc = abyRawTileData.data();
        GByte *pDst = &abyDecodedTileData[0];
        for (size_t i = 0; i < nValues;
             i++, pSrc += nSourceSize, pDst += nDTSize)
        {
            DecodeSourceElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    return true;

#undef m_abyRawTileData
#undef m_abyDecodedTileData
#undef m_poCodecs
}

/************************************************************************/
/*                      ZarrV3Array::IAdviseRead()                      */
/************************************************************************/

bool ZarrV3Array::IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                              CSLConstList papszOptions) const
{
    std::vector<uint64_t> anIndicesCur;
    int nThreadsMax = 0;
    std::vector<uint64_t> anReqTilesIndices;
    size_t nReqTiles = 0;
    if (!IAdviseReadCommon(arrayStartIdx, count, papszOptions, anIndicesCur,
                           nThreadsMax, anReqTilesIndices, nReqTiles))
    {
        return false;
    }
    if (nThreadsMax <= 1)
    {
        return true;
    }

    const int nThreads =
        static_cast<int>(std::min(static_cast<size_t>(nThreadsMax), nReqTiles));

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
        const std::vector<uint64_t> *panReqTilesIndices = nullptr;
        size_t nFirstIdx = 0;
        size_t nLastIdxNotIncluded = 0;
    };

    std::vector<JobStruct> asJobStructs;

    bool bGlobalStatus = true;
    int nRemainingThreads = nThreads;
    // Check for very highly overflow in below loop
    assert(static_cast<size_t>(nThreads) <
           std::numeric_limits<size_t>::max() / nReqTiles);

    // Setup jobs
    for (int i = 0; i < nThreads; i++)
    {
        JobStruct jobStruct;
        jobStruct.poArray = this;
        jobStruct.pbGlobalStatus = &bGlobalStatus;
        jobStruct.pnRemainingThreads = &nRemainingThreads;
        jobStruct.panReqTilesIndices = &anReqTilesIndices;
        jobStruct.nFirstIdx = static_cast<size_t>(i * nReqTiles / nThreads);
        jobStruct.nLastIdxNotIncluded = std::min(
            static_cast<size_t>((i + 1) * nReqTiles / nThreads), nReqTiles);
        asJobStructs.emplace_back(std::move(jobStruct));
    }

    const auto JobFunc = [](void *pThreadData)
    {
        const JobStruct *jobStruct =
            static_cast<const JobStruct *>(pThreadData);

        const auto poArray = jobStruct->poArray;
        const auto &aoDims = poArray->GetDimensions();
        const size_t l_nDims = poArray->GetDimensionCount();
        ZarrByteVectorQuickResize abyRawTileData;
        ZarrByteVectorQuickResize abyDecodedTileData;
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

            const uint64_t *tileIndices =
                jobStruct->panReqTilesIndices->data() + iReq * l_nDims;

            uint64_t nTileIdx = 0;
            for (size_t j = 0; j < l_nDims; ++j)
            {
                if (j > 0)
                    nTileIdx *= aoDims[j - 1]->GetSize();
                nTileIdx += tileIndices[j];
            }

            if (!poArray->AllocateWorkingBuffers(abyRawTileData,
                                                 abyDecodedTileData))
            {
                std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
                *jobStruct->pbGlobalStatus = false;
                break;
            }

            bool bIsEmpty = false;
            bool success = poArray->LoadTileData(tileIndices,
                                                 true,  // use mutex
                                                 poCodecs.get(), abyRawTileData,
                                                 abyDecodedTileData, bIsEmpty);

            std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
            if (!success)
            {
                *jobStruct->pbGlobalStatus = false;
                break;
            }

            CachedTile cachedTile;
            if (!bIsEmpty)
            {
                if (!abyDecodedTileData.empty())
                    std::swap(cachedTile.abyDecoded, abyDecodedTileData);
                else
                    std::swap(cachedTile.abyDecoded, abyRawTileData);
            }
            poArray->m_oMapTileIndexToCachedTile[nTileIdx] =
                std::move(cachedTile);
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
/*                    ZarrV3Array::FlushDirtyTile()                     */
/************************************************************************/

bool ZarrV3Array::FlushDirtyTile() const
{
    if (!m_bDirtyTile)
        return true;
    m_bDirtyTile = false;

    std::string osFilename = BuildTileFilename(m_anCachedTiledIndices.data());

    const size_t nSourceSize =
        m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;
    const auto &abyTile =
        m_abyDecodedTileData.empty() ? m_abyRawTileData : m_abyDecodedTileData;

    if (IsEmptyTile(abyTile))
    {
        m_bCachedTiledEmpty = true;

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

    if (!m_abyDecodedTileData.empty())
    {
        const size_t nDTSize = m_oType.GetSize();
        const size_t nValues = m_abyDecodedTileData.size() / nDTSize;
        GByte *pDst = &m_abyRawTileData[0];
        const GByte *pSrc = m_abyDecodedTileData.data();
        for (size_t i = 0; i < nValues;
             i++, pDst += nSourceSize, pSrc += nDTSize)
        {
            EncodeElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    const size_t nSizeBefore = m_abyRawTileData.size();
    if (m_poCodecs)
    {
        if (!m_poCodecs->Encode(m_abyRawTileData))
        {
            m_abyRawTileData.resize(nSizeBefore);
            return false;
        }
    }

    if (m_osDimSeparator == "/")
    {
        std::string osDir = CPLGetDirname(osFilename.c_str());
        VSIStatBufL sStat;
        if (VSIStatL(osDir.c_str(), &sStat) != 0)
        {
            if (VSIMkdirRecursive(osDir.c_str(), 0755) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot create directory %s", osDir.c_str());
                m_abyRawTileData.resize(nSizeBefore);
                return false;
            }
        }
    }

    VSILFILE *fp = VSIFOpenL(osFilename.c_str(), "wb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create tile %s",
                 osFilename.c_str());
        m_abyRawTileData.resize(nSizeBefore);
        return false;
    }

    bool bRet = true;
    const size_t nRawDataSize = m_abyRawTileData.size();
    if (VSIFWriteL(m_abyRawTileData.data(), 1, nRawDataSize, fp) !=
        nRawDataSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not write tile %s correctly", osFilename.c_str());
        bRet = false;
    }
    VSIFCloseL(fp);

    m_abyRawTileData.resize(nSizeBefore);

    return bRet;
}

/************************************************************************/
/*                          BuildTileFilename()                         */
/************************************************************************/

std::string ZarrV3Array::BuildTileFilename(const uint64_t *tileIndices) const
{
    if (m_aoDims.empty())
    {
        return CPLFormFilename(CPLGetDirname(m_osFilename.c_str()),
                               m_bV2ChunkKeyEncoding ? "0" : "c", nullptr);
    }
    else
    {
        std::string osFilename(CPLGetDirname(m_osFilename.c_str()));
        osFilename += '/';
        if (!m_bV2ChunkKeyEncoding)
        {
            osFilename += 'c';
        }
        for (size_t i = 0; i < m_aoDims.size(); ++i)
        {
            if (i > 0 || !m_bV2ChunkKeyEncoding)
                osFilename += m_osDimSeparator;
            osFilename += std::to_string(tileIndices[i]);
        }
        return osFilename;
    }
}

/************************************************************************/
/*                          GetDataDirectory()                          */
/************************************************************************/

std::string ZarrV3Array::GetDataDirectory() const
{
    return std::string(CPLGetDirname(m_osFilename.c_str()));
}

/************************************************************************/
/*                        GetTileIndicesFromFilename()                  */
/************************************************************************/

CPLStringList
ZarrV3Array::GetTileIndicesFromFilename(const char *pszFilename) const
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
/*                           ParseDtypeV3()                             */
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
                eDT = GDT_Byte;
            }
            else if (str == "int8")
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int8;
            }
            else if (str == "uint8")
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_Byte;
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
                elt.nativeSize = 2;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Float32;
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
/*                    ParseNoDataStringAsDouble()                       */
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
/*                     ParseNoDataComponent()                           */
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
/*                     ZarrV3Group::LoadArray()                         */
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
                const std::string osArrayFilenameDim =
                    CPLFormFilename(CPLFormFilename(osDirName.c_str(),
                                                    osDimName.c_str(), nullptr),
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
                    const std::string osDirNameNew =
                        CPLGetPath(osDirName.c_str());
                    if (!osDirNameNew.empty() && osDirNameNew != osDirName)
                    {
                        osDirName = osDirNameNew;
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

    std::vector<GUInt64> anBlockSize;
    if (!ZarrArray::ParseChunkSize(oChunks, oType, anBlockSize))
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
    if (oCodecs.Size() > 0)
    {
        // Byte swapping will be done by the codec chain
        aoDtypeElts.back().needByteSwapping = false;

        ZarrArrayMetadata oInputArrayMetadata;
        for (auto &nSize : anBlockSize)
            oInputArrayMetadata.anBlockSizes.push_back(
                static_cast<size_t>(nSize));
        oInputArrayMetadata.oElt = aoDtypeElts.back();
        poCodecs = std::make_unique<ZarrV3CodecSequence>(oInputArrayMetadata);
        if (!poCodecs->InitFromJson(oCodecs))
            return nullptr;
    }

    auto poArray =
        ZarrV3Array::Create(m_poSharedResource, GetFullName(), osArrayName,
                            aoDims, oType, aoDtypeElts, anBlockSize);
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
    poArray->ParseSpecialAttributes(m_pSelf.lock(), oAttributes);
    poArray->SetAttributes(oAttributes);
    poArray->SetDtype(oDtype);
    if (poCodecs)
        poArray->SetCodecs(std::move(poCodecs));
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
        poArray->CacheTilePresence();
    }

    return poArray;
}
