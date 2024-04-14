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

#include "netcdf_cf_constants.h"  // for CF_UNITS, etc

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

/************************************************************************/
/*                       ZarrV2Array::ZarrV2Array()                     */
/************************************************************************/

ZarrV2Array::ZarrV2Array(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const GDALExtendedDataType &oType, const std::vector<DtypeElt> &aoDtypeElts,
    const std::vector<GUInt64> &anBlockSize, bool bFortranOrder)
    : GDALAbstractMDArray(osParentName, osName),
      ZarrArray(poSharedResource, osParentName, osName, aoDims, oType,
                aoDtypeElts, anBlockSize),
      m_bFortranOrder(bFortranOrder)
{
    m_oCompressorJSon.Deinit();
}

/************************************************************************/
/*                         ZarrV2Array::Create()                        */
/************************************************************************/

std::shared_ptr<ZarrV2Array>
ZarrV2Array::Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                    const std::string &osParentName, const std::string &osName,
                    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                    const GDALExtendedDataType &oType,
                    const std::vector<DtypeElt> &aoDtypeElts,
                    const std::vector<GUInt64> &anBlockSize, bool bFortranOrder)
{
    auto arr = std::shared_ptr<ZarrV2Array>(
        new ZarrV2Array(poSharedResource, osParentName, osName, aoDims, oType,
                        aoDtypeElts, anBlockSize, bFortranOrder));
    if (arr->m_nTotalTileCount == 0)
        return nullptr;
    arr->SetSelf(arr);

    return arr;
}

/************************************************************************/
/*                             ~ZarrV2Array()                           */
/************************************************************************/

ZarrV2Array::~ZarrV2Array()
{
    ZarrV2Array::Flush();
}

/************************************************************************/
/*                                Flush()                               */
/************************************************************************/

void ZarrV2Array::Flush()
{
    if (!m_bValid)
        return;

    ZarrV2Array::FlushDirtyTile();

    if (m_bDefinitionModified)
    {
        Serialize();
        m_bDefinitionModified = false;
    }

    CPLJSONArray j_ARRAY_DIMENSIONS;
    bool bDimensionsModified = false;
    if (!m_aoDims.empty())
    {
        for (const auto &poDim : m_aoDims)
        {
            const auto poZarrDim =
                dynamic_cast<const ZarrDimension *>(poDim.get());
            if (poZarrDim && poZarrDim->IsXArrayDimension())
            {
                if (poZarrDim->IsModified())
                    bDimensionsModified = true;
                j_ARRAY_DIMENSIONS.Add(poDim->GetName());
            }
            else
            {
                j_ARRAY_DIMENSIONS = CPLJSONArray();
                break;
            }
        }
    }

    if (m_oAttrGroup.IsModified() || bDimensionsModified ||
        (m_bNew && j_ARRAY_DIMENSIONS.Size() != 0) || m_bUnitModified ||
        m_bOffsetModified || m_bScaleModified || m_bSRSModified)
    {
        m_bNew = false;

        auto oAttrs = SerializeSpecialAttributes();

        if (j_ARRAY_DIMENSIONS.Size() != 0)
        {
            oAttrs.Delete("_ARRAY_DIMENSIONS");
            oAttrs.Add("_ARRAY_DIMENSIONS", j_ARRAY_DIMENSIONS);
        }

        CPLJSONDocument oDoc;
        oDoc.SetRoot(oAttrs);
        const std::string osAttrFilename = CPLFormFilename(
            CPLGetDirname(m_osFilename.c_str()), ".zattrs", nullptr);
        oDoc.Save(osAttrFilename);
        m_poSharedResource->SetZMetadataItem(osAttrFilename, oAttrs);
    }
}

/************************************************************************/
/*           StripUselessItemsFromCompressorConfiguration()             */
/************************************************************************/

static void StripUselessItemsFromCompressorConfiguration(CPLJSONObject &o)
{
    if (o.GetType() == CPLJSONObject::Type::Object)
    {
        o.Delete("num_threads");  // Blosc
        o.Delete("typesize");     // Blosc
        o.Delete("header");       // LZ4
    }
}

/************************************************************************/
/*                    ZarrV2Array::Serialize()                          */
/************************************************************************/

void ZarrV2Array::Serialize()
{
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot = oDoc.GetRoot();

    CPLJSONArray oChunks;
    for (const auto nBlockSize : m_anBlockSize)
    {
        oChunks.Add(static_cast<GInt64>(nBlockSize));
    }
    oRoot.Add("chunks", oChunks);

    if (m_oCompressorJSon.IsValid())
    {
        oRoot.Add("compressor", m_oCompressorJSon);
        CPLJSONObject compressor = oRoot["compressor"];
        StripUselessItemsFromCompressorConfiguration(compressor);
    }
    else
    {
        oRoot.AddNull("compressor");
    }

    if (m_dtype.GetType() == CPLJSONObject::Type::Object)
        oRoot.Add("dtype", m_dtype["dummy"]);
    else
        oRoot.Add("dtype", m_dtype);

    if (m_pabyNoData == nullptr)
    {
        oRoot.AddNull("fill_value");
    }
    else
    {
        switch (m_oType.GetClass())
        {
            case GEDTC_NUMERIC:
            {
                SerializeNumericNoData(oRoot);
                break;
            }

            case GEDTC_STRING:
            {
                char *pszStr;
                char **ppszStr = reinterpret_cast<char **>(m_pabyNoData);
                memcpy(&pszStr, ppszStr, sizeof(pszStr));
                if (pszStr)
                {
                    const size_t nNativeSize =
                        m_aoDtypeElts.back().nativeOffset +
                        m_aoDtypeElts.back().nativeSize;
                    char *base64 = CPLBase64Encode(
                        static_cast<int>(std::min(nNativeSize, strlen(pszStr))),
                        reinterpret_cast<const GByte *>(pszStr));
                    oRoot.Add("fill_value", base64);
                    CPLFree(base64);
                }
                else
                {
                    oRoot.AddNull("fill_value");
                }
                break;
            }

            case GEDTC_COMPOUND:
            {
                const size_t nNativeSize = m_aoDtypeElts.back().nativeOffset +
                                           m_aoDtypeElts.back().nativeSize;
                std::vector<GByte> nativeNoData(nNativeSize);
                EncodeElt(m_aoDtypeElts, m_pabyNoData, &nativeNoData[0]);
                char *base64 = CPLBase64Encode(static_cast<int>(nNativeSize),
                                               nativeNoData.data());
                oRoot.Add("fill_value", base64);
                CPLFree(base64);
            }
        }
    }

    if (m_oFiltersArray.Size() == 0)
        oRoot.AddNull("filters");
    else
        oRoot.Add("filters", m_oFiltersArray);

    oRoot.Add("order", m_bFortranOrder ? "F" : "C");

    CPLJSONArray oShape;
    for (const auto &poDim : m_aoDims)
    {
        oShape.Add(static_cast<GInt64>(poDim->GetSize()));
    }
    oRoot.Add("shape", oShape);

    oRoot.Add("zarr_format", 2);

    if (m_osDimSeparator != ".")
    {
        oRoot.Add("dimension_separator", m_osDimSeparator);
    }

    oDoc.Save(m_osFilename);

    m_poSharedResource->SetZMetadataItem(m_osFilename, oRoot);
}

/************************************************************************/
/*                  ZarrV2Array::NeedDecodedBuffer()                    */
/************************************************************************/

bool ZarrV2Array::NeedDecodedBuffer() const
{
    const size_t nSourceSize =
        m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;
    if (m_oType.GetClass() == GEDTC_COMPOUND &&
        nSourceSize != m_oType.GetSize())
    {
        return true;
    }
    else if (m_oType.GetClass() != GEDTC_STRING)
    {
        for (const auto &elt : m_aoDtypeElts)
        {
            if (elt.needByteSwapping || elt.gdalTypeIsApproxOfNative ||
                elt.nativeType == DtypeElt::NativeType::STRING_ASCII ||
                elt.nativeType == DtypeElt::NativeType::STRING_UNICODE)
            {
                return true;
            }
        }
    }
    return false;
}

/************************************************************************/
/*               ZarrV2Array::AllocateWorkingBuffers()                  */
/************************************************************************/

bool ZarrV2Array::AllocateWorkingBuffers() const
{
    if (m_bAllocateWorkingBuffersDone)
        return m_bWorkingBuffersOK;

    m_bAllocateWorkingBuffersDone = true;

    size_t nSizeNeeded = m_nTileSize;
    if (m_bFortranOrder || m_oFiltersArray.Size() != 0)
    {
        if (nSizeNeeded > std::numeric_limits<size_t>::max() / 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large chunk size");
            return false;
        }
        nSizeNeeded *= 2;
    }
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

    m_bWorkingBuffersOK = AllocateWorkingBuffers(
        m_abyRawTileData, m_abyTmpRawTileData, m_abyDecodedTileData);
    return m_bWorkingBuffersOK;
}

bool ZarrV2Array::AllocateWorkingBuffers(
    ZarrByteVectorQuickResize &abyRawTileData,
    ZarrByteVectorQuickResize &abyTmpRawTileData,
    ZarrByteVectorQuickResize &abyDecodedTileData) const
{
    // This method should NOT modify any ZarrArray member, as it is going to
    // be called concurrently from several threads.

    // Set those #define to avoid accidental use of some global variables
#define m_abyTmpRawTileData cannot_use_here
#define m_abyRawTileData cannot_use_here
#define m_abyDecodedTileData cannot_use_here

    try
    {
        abyRawTileData.resize(m_nTileSize);
        if (m_bFortranOrder || m_oFiltersArray.Size() != 0)
            abyTmpRawTileData.resize(m_nTileSize);
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
#undef m_abyTmpRawTileData
#undef m_abyRawTileData
#undef m_abyDecodedTileData
}

/************************************************************************/
/*                      ZarrV2Array::LoadTileData()                     */
/************************************************************************/

bool ZarrV2Array::LoadTileData(const uint64_t *tileIndices,
                               bool &bMissingTileOut) const
{
    return LoadTileData(tileIndices,
                        false,  // use mutex
                        m_psDecompressor, m_abyRawTileData, m_abyTmpRawTileData,
                        m_abyDecodedTileData, bMissingTileOut);
}

bool ZarrV2Array::LoadTileData(const uint64_t *tileIndices, bool bUseMutex,
                               const CPLCompressor *psDecompressor,
                               ZarrByteVectorQuickResize &abyRawTileData,
                               ZarrByteVectorQuickResize &abyTmpRawTileData,
                               ZarrByteVectorQuickResize &abyDecodedTileData,
                               bool &bMissingTileOut) const
{
    // This method should NOT modify any ZarrArray member, as it is going to
    // be called concurrently from several threads.

    // Set those #define to avoid accidental use of some global variables
#define m_abyTmpRawTileData cannot_use_here
#define m_abyRawTileData cannot_use_here
#define m_abyDecodedTileData cannot_use_here
#define m_psDecompressor cannot_use_here

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
    bool bRet = true;
    size_t nRawDataSize = abyRawTileData.size();
    if (psDecompressor == nullptr)
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
            ZarrByteVectorQuickResize abyCompressedData;
            try
            {
                abyCompressedData.resize(static_cast<size_t>(nSize));
            }
            catch (const std::exception &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate memory for tile %s",
                         osFilename.c_str());
                bRet = false;
            }

            if (bRet &&
                (abyCompressedData.empty() ||
                 VSIFReadL(&abyCompressedData[0], 1, abyCompressedData.size(),
                           fp) != abyCompressedData.size()))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Could not read tile %s correctly",
                         osFilename.c_str());
                bRet = false;
            }
            else
            {
                void *out_buffer = &abyRawTileData[0];
                if (!psDecompressor->pfnFunc(
                        abyCompressedData.data(), abyCompressedData.size(),
                        &out_buffer, &nRawDataSize, nullptr,
                        psDecompressor->user_data))
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

    for (int i = m_oFiltersArray.Size(); i > 0;)
    {
        --i;
        const auto &oFilter = m_oFiltersArray[i];
        const auto osFilterId = oFilter["id"].ToString();
        const auto psFilterDecompressor =
            CPLGetDecompressor(osFilterId.c_str());
        CPLAssert(psFilterDecompressor);

        CPLStringList aosOptions;
        for (const auto &obj : oFilter.GetChildren())
        {
            aosOptions.SetNameValue(obj.GetName().c_str(),
                                    obj.ToString().c_str());
        }
        void *out_buffer = &abyTmpRawTileData[0];
        size_t nOutSize = abyTmpRawTileData.size();
        if (!psFilterDecompressor->pfnFunc(
                abyRawTileData.data(), nRawDataSize, &out_buffer, &nOutSize,
                aosOptions.List(), psFilterDecompressor->user_data))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Filter %s for tile %s failed", osFilterId.c_str(),
                     osFilename.c_str());
            return false;
        }

        nRawDataSize = nOutSize;
        std::swap(abyRawTileData, abyTmpRawTileData);
    }
    if (nRawDataSize != abyRawTileData.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Decompressed tile %s has not expected size after filters",
                 osFilename.c_str());
        return false;
    }

    if (m_bFortranOrder && !m_aoDims.empty())
    {
        BlockTranspose(abyRawTileData, abyTmpRawTileData, true);
        std::swap(abyRawTileData, abyTmpRawTileData);
    }

    if (!abyDecodedTileData.empty())
    {
        const size_t nSourceSize =
            m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;
        const auto nDTSize = m_oType.GetSize();
        const size_t nValues = abyDecodedTileData.size() / nDTSize;
        const GByte *pSrc = abyRawTileData.data();
        GByte *pDst = &abyDecodedTileData[0];
        for (size_t i = 0; i < nValues;
             i++, pSrc += nSourceSize, pDst += nDTSize)
        {
            DecodeSourceElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    return true;

#undef m_abyTmpRawTileData
#undef m_abyRawTileData
#undef m_abyDecodedTileData
#undef m_psDecompressor
}

/************************************************************************/
/*                      ZarrV2Array::IAdviseRead()                      */
/************************************************************************/

bool ZarrV2Array::IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
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

        const ZarrV2Array *poArray = nullptr;
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
        ZarrByteVectorQuickResize abyTmpRawTileData;
        const CPLCompressor *psDecompressor =
            CPLGetDecompressor(poArray->m_osDecompressorId.c_str());

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

            if (!poArray->AllocateWorkingBuffers(
                    abyRawTileData, abyTmpRawTileData, abyDecodedTileData))
            {
                std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
                *jobStruct->pbGlobalStatus = false;
                break;
            }

            bool bIsEmpty = false;
            bool success = poArray->LoadTileData(tileIndices,
                                                 true,  // use mutex
                                                 psDecompressor, abyRawTileData,
                                                 abyTmpRawTileData,
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
/*                    ZarrV2Array::FlushDirtyTile()                     */
/************************************************************************/

bool ZarrV2Array::FlushDirtyTile() const
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

    if (m_bFortranOrder && !m_aoDims.empty())
    {
        BlockTranspose(m_abyRawTileData, m_abyTmpRawTileData, false);
        std::swap(m_abyRawTileData, m_abyTmpRawTileData);
    }

    size_t nRawDataSize = m_abyRawTileData.size();
    for (const auto &oFilter : m_oFiltersArray)
    {
        const auto osFilterId = oFilter["id"].ToString();
        const auto psFilterCompressor = CPLGetCompressor(osFilterId.c_str());
        CPLAssert(psFilterCompressor);

        CPLStringList aosOptions;
        for (const auto &obj : oFilter.GetChildren())
        {
            aosOptions.SetNameValue(obj.GetName().c_str(),
                                    obj.ToString().c_str());
        }
        void *out_buffer = &m_abyTmpRawTileData[0];
        size_t nOutSize = m_abyTmpRawTileData.size();
        if (!psFilterCompressor->pfnFunc(
                m_abyRawTileData.data(), nRawDataSize, &out_buffer, &nOutSize,
                aosOptions.List(), psFilterCompressor->user_data))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Filter %s for tile %s failed", osFilterId.c_str(),
                     osFilename.c_str());
            return false;
        }

        nRawDataSize = nOutSize;
        std::swap(m_abyRawTileData, m_abyTmpRawTileData);
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
                return false;
            }
        }
    }

    VSILFILE *fp = VSIFOpenL(osFilename.c_str(), "wb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create tile %s",
                 osFilename.c_str());
        return false;
    }

    bool bRet = true;
    if (m_psCompressor == nullptr)
    {
        if (VSIFWriteL(m_abyRawTileData.data(), 1, nRawDataSize, fp) !=
            nRawDataSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not write tile %s correctly", osFilename.c_str());
            bRet = false;
        }
    }
    else
    {
        std::vector<GByte> abyCompressedData;
        try
        {
            constexpr size_t MIN_BUF_SIZE = 64;  // somewhat arbitrary
            abyCompressedData.resize(static_cast<size_t>(
                MIN_BUF_SIZE + nRawDataSize + nRawDataSize / 3));
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate memory for tile %s", osFilename.c_str());
            bRet = false;
        }

        if (bRet)
        {
            void *out_buffer = &abyCompressedData[0];
            size_t out_size = abyCompressedData.size();
            CPLStringList aosOptions;
            const auto &compressorConfig = m_oCompressorJSon;
            for (const auto &obj : compressorConfig.GetChildren())
            {
                aosOptions.SetNameValue(obj.GetName().c_str(),
                                        obj.ToString().c_str());
            }
            if (EQUAL(m_psCompressor->pszId, "blosc") &&
                m_oType.GetClass() == GEDTC_NUMERIC)
            {
                aosOptions.SetNameValue(
                    "TYPESIZE",
                    CPLSPrintf("%d", GDALGetDataTypeSizeBytes(
                                         GDALGetNonComplexDataType(
                                             m_oType.GetNumericDataType()))));
            }

            if (!m_psCompressor->pfnFunc(
                    m_abyRawTileData.data(), nRawDataSize, &out_buffer,
                    &out_size, aosOptions.List(), m_psCompressor->user_data))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Compression of tile %s failed", osFilename.c_str());
                bRet = false;
            }
            abyCompressedData.resize(out_size);
        }

        if (bRet &&
            VSIFWriteL(abyCompressedData.data(), 1, abyCompressedData.size(),
                       fp) != abyCompressedData.size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not write tile %s correctly", osFilename.c_str());
            bRet = false;
        }
    }
    VSIFCloseL(fp);

    return bRet;
}

/************************************************************************/
/*                          BuildTileFilename()                         */
/************************************************************************/

std::string ZarrV2Array::BuildTileFilename(const uint64_t *tileIndices) const
{
    std::string osFilename;
    if (m_aoDims.empty())
    {
        osFilename = "0";
    }
    else
    {
        for (size_t i = 0; i < m_aoDims.size(); ++i)
        {
            if (!osFilename.empty())
                osFilename += m_osDimSeparator;
            osFilename += std::to_string(tileIndices[i]);
        }
    }

    return CPLFormFilename(CPLGetDirname(m_osFilename.c_str()),
                           osFilename.c_str(), nullptr);
}

/************************************************************************/
/*                          GetDataDirectory()                          */
/************************************************************************/

std::string ZarrV2Array::GetDataDirectory() const
{
    return std::string(CPLGetDirname(m_osFilename.c_str()));
}

/************************************************************************/
/*                        GetTileIndicesFromFilename()                  */
/************************************************************************/

CPLStringList
ZarrV2Array::GetTileIndicesFromFilename(const char *pszFilename) const
{
    return CPLStringList(
        CSLTokenizeString2(pszFilename, m_osDimSeparator.c_str(), 0));
}

/************************************************************************/
/*                             ParseDtype()                             */
/************************************************************************/

static size_t GetAlignment(const CPLJSONObject &obj)
{
    if (obj.GetType() == CPLJSONObject::Type::String)
    {
        const auto str = obj.ToString();
        if (str.size() < 3)
            return 1;
        const char chType = str[1];
        const int nBytes = atoi(str.c_str() + 2);
        if (chType == 'S')
            return sizeof(char *);
        if (chType == 'c' && nBytes == 8)
            return sizeof(float);
        if (chType == 'c' && nBytes == 16)
            return sizeof(double);
        return nBytes;
    }
    else if (obj.GetType() == CPLJSONObject::Type::Array)
    {
        const auto oArray = obj.ToArray();
        size_t nAlignment = 1;
        for (const auto &oElt : oArray)
        {
            const auto oEltArray = oElt.ToArray();
            if (!oEltArray.IsValid() || oEltArray.Size() != 2 ||
                oEltArray[0].GetType() != CPLJSONObject::Type::String)
            {
                return 1;
            }
            nAlignment = std::max(nAlignment, GetAlignment(oEltArray[1]));
            if (nAlignment == sizeof(void *))
                break;
        }
        return nAlignment;
    }
    return 1;
}

static GDALExtendedDataType ParseDtype(const CPLJSONObject &obj,
                                       std::vector<DtypeElt> &elts)
{
    const auto AlignOffsetOn = [](size_t offset, size_t alignment)
    { return offset + (alignment - (offset % alignment)) % alignment; };

    do
    {
        if (obj.GetType() == CPLJSONObject::Type::String)
        {
            const auto str = obj.ToString();
            char chEndianness = 0;
            char chType;
            int nBytes;
            DtypeElt elt;
            if (str.size() < 3)
                break;
            chEndianness = str[0];
            chType = str[1];
            nBytes = atoi(str.c_str() + 2);
            if (nBytes <= 0 || nBytes >= 1000)
                break;

            elt.needByteSwapping = false;
            if ((nBytes > 1 && chType != 'S') || chType == 'U')
            {
                if (chEndianness == '<')
                    elt.needByteSwapping = (CPL_IS_LSB == 0);
                else if (chEndianness == '>')
                    elt.needByteSwapping = (CPL_IS_LSB != 0);
            }

            GDALDataType eDT;
            if (!elts.empty())
            {
                elt.nativeOffset =
                    elts.back().nativeOffset + elts.back().nativeSize;
            }
            elt.nativeSize = nBytes;
            if (chType == 'b' && nBytes == 1)  // boolean
            {
                elt.nativeType = DtypeElt::NativeType::BOOLEAN;
                eDT = GDT_Byte;
            }
            else if (chType == 'u' && nBytes == 1)
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_Byte;
            }
            else if (chType == 'i' && nBytes == 1)
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int8;
            }
            else if (chType == 'i' && nBytes == 2)
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int16;
            }
            else if (chType == 'i' && nBytes == 4)
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int32;
            }
            else if (chType == 'i' && nBytes == 8)
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int64;
            }
            else if (chType == 'u' && nBytes == 2)
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt16;
            }
            else if (chType == 'u' && nBytes == 4)
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt32;
            }
            else if (chType == 'u' && nBytes == 8)
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt64;
            }
            else if (chType == 'f' && nBytes == 2)
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Float32;
            }
            else if (chType == 'f' && nBytes == 4)
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float32;
            }
            else if (chType == 'f' && nBytes == 8)
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float64;
            }
            else if (chType == 'c' && nBytes == 8)
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat32;
            }
            else if (chType == 'c' && nBytes == 16)
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat64;
            }
            else if (chType == 'S')
            {
                elt.nativeType = DtypeElt::NativeType::STRING_ASCII;
                elt.gdalType = GDALExtendedDataType::CreateString(nBytes);
                elt.gdalSize = elt.gdalType.GetSize();
                elts.emplace_back(elt);
                return GDALExtendedDataType::CreateString(nBytes);
            }
            else if (chType == 'U')
            {
                elt.nativeType = DtypeElt::NativeType::STRING_UNICODE;
                // the dtype declaration is number of UCS4 characters. Store it
                // as bytes
                elt.nativeSize *= 4;
                // We can really map UCS4 size to UTF-8
                elt.gdalType = GDALExtendedDataType::CreateString();
                elt.gdalSize = elt.gdalType.GetSize();
                elts.emplace_back(elt);
                return GDALExtendedDataType::CreateString();
            }
            else
                break;
            elt.gdalType = GDALExtendedDataType::Create(eDT);
            elt.gdalSize = elt.gdalType.GetSize();
            elts.emplace_back(elt);
            return GDALExtendedDataType::Create(eDT);
        }
        else if (obj.GetType() == CPLJSONObject::Type::Array)
        {
            bool error = false;
            const auto oArray = obj.ToArray();
            std::vector<std::unique_ptr<GDALEDTComponent>> comps;
            size_t offset = 0;
            size_t alignmentMax = 1;
            for (const auto &oElt : oArray)
            {
                const auto oEltArray = oElt.ToArray();
                if (!oEltArray.IsValid() || oEltArray.Size() != 2 ||
                    oEltArray[0].GetType() != CPLJSONObject::Type::String)
                {
                    error = true;
                    break;
                }
                GDALExtendedDataType subDT = ParseDtype(oEltArray[1], elts);
                if (subDT.GetClass() == GEDTC_NUMERIC &&
                    subDT.GetNumericDataType() == GDT_Unknown)
                {
                    error = true;
                    break;
                }

                const std::string osName = oEltArray[0].ToString();
                // Add padding for alignment
                const size_t alignmentSub = GetAlignment(oEltArray[1]);
                assert(alignmentSub);
                alignmentMax = std::max(alignmentMax, alignmentSub);
                offset = AlignOffsetOn(offset, alignmentSub);
                comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
                    new GDALEDTComponent(osName, offset, subDT)));
                offset += subDT.GetSize();
            }
            if (error)
                break;
            size_t nTotalSize = offset;
            nTotalSize = AlignOffsetOn(nTotalSize, alignmentMax);
            return GDALExtendedDataType::Create(obj.ToString(), nTotalSize,
                                                std::move(comps));
        }
    } while (false);
    CPLError(CE_Failure, CPLE_AppDefined,
             "Invalid or unsupported format for dtype: %s",
             obj.ToString().c_str());
    return GDALExtendedDataType::Create(GDT_Unknown);
}

static void SetGDALOffset(const GDALExtendedDataType &dt,
                          const size_t nBaseOffset, std::vector<DtypeElt> &elts,
                          size_t &iCurElt)
{
    if (dt.GetClass() == GEDTC_COMPOUND)
    {
        const auto &comps = dt.GetComponents();
        for (const auto &comp : comps)
        {
            const size_t nBaseOffsetSub = nBaseOffset + comp->GetOffset();
            SetGDALOffset(comp->GetType(), nBaseOffsetSub, elts, iCurElt);
        }
    }
    else
    {
        elts[iCurElt].gdalOffset = nBaseOffset;
        iCurElt++;
    }
}

/************************************************************************/
/*                     ZarrV2Group::LoadArray()                         */
/************************************************************************/

std::shared_ptr<ZarrArray>
ZarrV2Group::LoadArray(const std::string &osArrayName,
                       const std::string &osZarrayFilename,
                       const CPLJSONObject &oRoot, bool bLoadedFromZMetadata,
                       const CPLJSONObject &oAttributesIn) const
{
    // Add osZarrayFilename to m_poSharedResource during the scope
    // of this function call.
    ZarrSharedResource::SetFilenameAdder filenameAdder(m_poSharedResource,
                                                       osZarrayFilename);
    if (!filenameAdder.ok())
        return nullptr;

    const auto osFormat = oRoot["zarr_format"].ToString();
    if (osFormat != "2")
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid value for zarr_format");
        return nullptr;
    }

    bool bFortranOrder = false;
    const char *orderKey = "order";
    const auto osOrder = oRoot[orderKey].ToString();
    if (osOrder == "C")
    {
        // ok
    }
    else if (osOrder == "F")
    {
        bFortranOrder = true;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid value for %s",
                 orderKey);
        return nullptr;
    }

    const auto oShape = oRoot["shape"].ToArray();
    if (!oShape.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "shape missing or not an array");
        return nullptr;
    }

    const char *chunksKey = "chunks";
    const auto oChunks = oRoot[chunksKey].ToArray();
    if (!oChunks.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s missing or not an array",
                 chunksKey);
        return nullptr;
    }

    if (oShape.Size() != oChunks.Size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "shape and chunks arrays are of different size");
        return nullptr;
    }

    CPLJSONObject oAttributes(oAttributesIn);
    if (!bLoadedFromZMetadata)
    {
        CPLJSONDocument oDoc;
        const std::string osZattrsFilename(CPLFormFilename(
            CPLGetDirname(osZarrayFilename.c_str()), ".zattrs", nullptr));
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
        if (oDoc.Load(osZattrsFilename))
        {
            oAttributes = oDoc.GetRoot();
        }
    }

    // Deep-clone of oAttributes
    {
        CPLJSONDocument oTmpDoc;
        oTmpDoc.SetRoot(oAttributes);
        CPL_IGNORE_RET_VAL(oTmpDoc.LoadMemory(oTmpDoc.SaveAsString()));
        oAttributes = oTmpDoc.GetRoot();
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

    // XArray extension
    const auto arrayDimensionsObj = oAttributes["_ARRAY_DIMENSIONS"];

    const auto FindDimension =
        [this, &aoDims, bLoadedFromZMetadata, &osArrayName,
         &oAttributes](const std::string &osDimName,
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

        // If loading from zmetadata, we should have normally
        // already loaded the dimension variables, unless they
        // are in a upper level.
        if (bLoadedFromZMetadata && osArrayName != osDimName &&
            m_oMapMDArrays.find(osDimName) == m_oMapMDArrays.end())
        {
            auto poParent = m_poParent.lock();
            while (poParent != nullptr)
            {
                oIter = poParent->m_oMapDimensions.find(osDimName);
                if (oIter != poParent->m_oMapDimensions.end() &&
                    oIter->second->GetSize() == poDim->GetSize())
                {
                    poDim = oIter->second;
                    return true;
                }
                poParent = poParent->m_poParent.lock();
            }
        }

        // Not loading from zmetadata, and not in m_oMapMDArrays,
        // then stat() the indexing variable.
        else if (!bLoadedFromZMetadata && osArrayName != osDimName &&
                 m_oMapMDArrays.find(osDimName) == m_oMapMDArrays.end())
        {
            std::string osDirName = m_osDirectoryName;
            while (true)
            {
                const std::string osArrayFilenameDim =
                    CPLFormFilename(CPLFormFilename(osDirName.c_str(),
                                                    osDimName.c_str(), nullptr),
                                    ".zarray", nullptr);
                VSIStatBufL sStat;
                if (VSIStatL(osArrayFilenameDim.c_str(), &sStat) == 0)
                {
                    CPLJSONDocument oDoc;
                    if (oDoc.Load(osArrayFilenameDim))
                    {
                        LoadArray(osDimName, osArrayFilenameDim, oDoc.GetRoot(),
                                  false, CPLJSONObject());
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

    if (arrayDimensionsObj.GetType() == CPLJSONObject::Type::Array)
    {
        const auto arrayDims = arrayDimensionsObj.ToArray();
        if (arrayDims.Size() == oShape.Size())
        {
            bool ok = true;
            for (int i = 0; i < oShape.Size(); ++i)
            {
                if (arrayDims[i].GetType() == CPLJSONObject::Type::String)
                {
                    const auto osDimName = arrayDims[i].ToString();
                    ok &= FindDimension(osDimName, aoDims[i], i);
                }
            }
            if (ok)
            {
                oAttributes.Delete("_ARRAY_DIMENSIONS");
            }
        }
        else
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Size of _ARRAY_DIMENSIONS different from the one of shape");
        }
    }

    // _NCZARR_ARRAY extension
    const auto nczarrArrayDimrefs = oRoot["_NCZARR_ARRAY"]["dimrefs"].ToArray();
    if (nczarrArrayDimrefs.IsValid())
    {
        const auto arrayDims = nczarrArrayDimrefs.ToArray();
        if (arrayDims.Size() == oShape.Size())
        {
            auto poRG =
                std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock());
            CPLAssert(poRG != nullptr);
            while (true)
            {
                auto poNewRG = poRG->m_poParent.lock();
                if (poNewRG == nullptr)
                    break;
                poRG = std::move(poNewRG);
            }

            for (int i = 0; i < oShape.Size(); ++i)
            {
                if (arrayDims[i].GetType() == CPLJSONObject::Type::String)
                {
                    const auto osDimFullpath = arrayDims[i].ToString();
                    const std::string osArrayFullname =
                        (GetFullName() != "/" ? GetFullName() : std::string()) +
                        '/' + osArrayName;
                    if (aoDims.size() == 1 &&
                        (osDimFullpath == osArrayFullname ||
                         osDimFullpath == "/" + osArrayFullname))
                    {
                        // If this is an indexing variable, then fetch the
                        // dimension type and direction, and patch the dimension
                        std::string osType;
                        std::string osDirection;
                        ZarrArray::GetDimensionTypeDirection(
                            oAttributes, osType, osDirection);

                        auto poDimLocal = std::make_shared<ZarrDimension>(
                            m_poSharedResource,
                            std::dynamic_pointer_cast<ZarrGroupBase>(
                                m_pSelf.lock()),
                            GetFullName(), osArrayName, osType, osDirection,
                            aoDims[i]->GetSize());
                        aoDims[i] = poDimLocal;

                        m_oMapDimensions[osArrayName] = std::move(poDimLocal);
                    }
                    else if (auto poDim =
                                 poRG->OpenDimensionFromFullname(osDimFullpath))
                    {
                        if (poDim->GetSize() != aoDims[i]->GetSize())
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Inconsistency in size between NCZarr "
                                     "dimension %s and regular dimension",
                                     osDimFullpath.c_str());
                        }
                        else
                        {
                            aoDims[i] = std::move(poDim);
                        }
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot find NCZarr dimension %s",
                                 osDimFullpath.c_str());
                    }
                }
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Size of _NCZARR_ARRAY.dimrefs different from the one of "
                     "shape");
        }
    }

    constexpr const char *dtypeKey = "dtype";
    auto oDtype = oRoot[dtypeKey];
    if (!oDtype.IsValid())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "%s missing", dtypeKey);
        return nullptr;
    }
    std::vector<DtypeElt> aoDtypeElts;
    const auto oType = ParseDtype(oDtype, aoDtypeElts);
    if (oType.GetClass() == GEDTC_NUMERIC &&
        oType.GetNumericDataType() == GDT_Unknown)
        return nullptr;
    size_t iCurElt = 0;
    SetGDALOffset(oType, 0, aoDtypeElts, iCurElt);

    std::vector<GUInt64> anBlockSize;
    if (!ZarrArray::ParseChunkSize(oChunks, oType, anBlockSize))
        return nullptr;

    std::string osDimSeparator = oRoot["dimension_separator"].ToString();
    if (osDimSeparator.empty())
        osDimSeparator = ".";

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

    NoDataFreer NoDataFreer(abyNoData, oType);

    auto oFillValue = oRoot["fill_value"];
    auto eFillValueType = oFillValue.GetType();

    // Normally arrays are not supported, but that's what NCZarr 4.8.0 outputs
    if (eFillValueType == CPLJSONObject::Type::Array &&
        oFillValue.ToArray().Size() == 1)
    {
        oFillValue = oFillValue.ToArray()[0];
        eFillValueType = oFillValue.GetType();
    }

    if (!oFillValue.IsValid())
    {
        // fill_value is normally required but some implementations
        // are lacking it: https://github.com/Unidata/netcdf-c/issues/2059
        CPLError(CE_Warning, CPLE_AppDefined, "fill_value missing");
    }
    else if (eFillValueType == CPLJSONObject::Type::Null)
    {
        // Nothing to do
    }
    else if (eFillValueType == CPLJSONObject::Type::String)
    {
        const auto osFillValue = oFillValue.ToString();
        if (oType.GetClass() == GEDTC_NUMERIC &&
            CPLGetValueType(osFillValue.c_str()) != CPL_VALUE_STRING)
        {
            abyNoData.resize(oType.GetSize());
            // Be tolerant with numeric values serialized as strings.
            if (oType.GetNumericDataType() == GDT_Int64)
            {
                const int64_t nVal = static_cast<int64_t>(
                    std::strtoll(osFillValue.c_str(), nullptr, 10));
                GDALCopyWords(&nVal, GDT_Int64, 0, &abyNoData[0],
                              oType.GetNumericDataType(), 0, 1);
            }
            else if (oType.GetNumericDataType() == GDT_UInt64)
            {
                const uint64_t nVal = static_cast<uint64_t>(
                    std::strtoull(osFillValue.c_str(), nullptr, 10));
                GDALCopyWords(&nVal, GDT_UInt64, 0, &abyNoData[0],
                              oType.GetNumericDataType(), 0, 1);
            }
            else
            {
                const double dfNoDataValue = CPLAtof(osFillValue.c_str());
                GDALCopyWords(&dfNoDataValue, GDT_Float64, 0, &abyNoData[0],
                              oType.GetNumericDataType(), 0, 1);
            }
        }
        else if (oType.GetClass() == GEDTC_NUMERIC)
        {
            double dfNoDataValue;
            if (osFillValue == "NaN")
            {
                dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
            }
            else if (osFillValue == "Infinity")
            {
                dfNoDataValue = std::numeric_limits<double>::infinity();
            }
            else if (osFillValue == "-Infinity")
            {
                dfNoDataValue = -std::numeric_limits<double>::infinity();
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            if (oType.GetNumericDataType() == GDT_Float32)
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
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
        }
        else if (oType.GetClass() == GEDTC_STRING)
        {
            // zarr.open('unicode_be.zarr', mode = 'w', shape=(1,), dtype =
            // '>U1', compressor = None) oddly generates "fill_value": "0"
            if (osFillValue != "0")
            {
                std::vector<GByte> abyNativeFillValue(osFillValue.size() + 1);
                memcpy(&abyNativeFillValue[0], osFillValue.data(),
                       osFillValue.size());
                int nBytes = CPLBase64DecodeInPlace(&abyNativeFillValue[0]);
                abyNativeFillValue.resize(nBytes + 1);
                abyNativeFillValue[nBytes] = 0;
                abyNoData.resize(oType.GetSize());
                char *pDstStr = CPLStrdup(
                    reinterpret_cast<const char *>(&abyNativeFillValue[0]));
                char **pDstPtr = reinterpret_cast<char **>(&abyNoData[0]);
                memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
            }
        }
        else
        {
            std::vector<GByte> abyNativeFillValue(osFillValue.size() + 1);
            memcpy(&abyNativeFillValue[0], osFillValue.data(),
                   osFillValue.size());
            int nBytes = CPLBase64DecodeInPlace(&abyNativeFillValue[0]);
            abyNativeFillValue.resize(nBytes);
            if (abyNativeFillValue.size() !=
                aoDtypeElts.back().nativeOffset + aoDtypeElts.back().nativeSize)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            abyNoData.resize(oType.GetSize());
            ZarrArray::DecodeSourceElt(aoDtypeElts, abyNativeFillValue.data(),
                                       &abyNoData[0]);
        }
    }
    else if (eFillValueType == CPLJSONObject::Type::Boolean ||
             eFillValueType == CPLJSONObject::Type::Integer ||
             eFillValueType == CPLJSONObject::Type::Long ||
             eFillValueType == CPLJSONObject::Type::Double)
    {
        if (oType.GetClass() == GEDTC_NUMERIC)
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

    const CPLCompressor *psCompressor = nullptr;
    const CPLCompressor *psDecompressor = nullptr;
    const auto oCompressor = oRoot["compressor"];
    std::string osDecompressorId("NONE");

    if (!oCompressor.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "compressor missing");
        return nullptr;
    }
    if (oCompressor.GetType() == CPLJSONObject::Type::Null)
    {
        // nothing to do
    }
    else if (oCompressor.GetType() == CPLJSONObject::Type::Object)
    {
        osDecompressorId = oCompressor["id"].ToString();
        if (osDecompressorId.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing compressor id");
            return nullptr;
        }
        psCompressor = CPLGetCompressor(osDecompressorId.c_str());
        psDecompressor = CPLGetDecompressor(osDecompressorId.c_str());
        if (psCompressor == nullptr || psDecompressor == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Decompressor %s not handled",
                     osDecompressorId.c_str());
            return nullptr;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid compressor");
        return nullptr;
    }

    CPLJSONArray oFiltersArray;
    const auto oFilters = oRoot["filters"];
    if (!oFilters.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "filters missing");
        return nullptr;
    }
    if (oFilters.GetType() == CPLJSONObject::Type::Null)
    {
    }
    else if (oFilters.GetType() == CPLJSONObject::Type::Array)
    {
        oFiltersArray = oFilters.ToArray();
        for (const auto &oFilter : oFiltersArray)
        {
            const auto osFilterId = oFilter["id"].ToString();
            if (osFilterId.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Missing filter id");
                return nullptr;
            }
            const auto psFilterCompressor =
                CPLGetCompressor(osFilterId.c_str());
            const auto psFilterDecompressor =
                CPLGetDecompressor(osFilterId.c_str());
            if (psFilterCompressor == nullptr ||
                psFilterDecompressor == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Filter %s not handled",
                         osFilterId.c_str());
                return nullptr;
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid filters");
        return nullptr;
    }

    auto poArray = ZarrV2Array::Create(m_poSharedResource, GetFullName(),
                                       osArrayName, aoDims, oType, aoDtypeElts,
                                       anBlockSize, bFortranOrder);
    if (!poArray)
        return nullptr;
    poArray->SetCompressorJson(oCompressor);
    poArray->SetUpdatable(m_bUpdatable);  // must be set before SetAttributes()
    poArray->SetFilename(osZarrayFilename);
    poArray->SetDimSeparator(osDimSeparator);
    poArray->SetCompressorDecompressor(osDecompressorId, psCompressor,
                                       psDecompressor);
    poArray->SetFilters(oFiltersArray);
    if (!abyNoData.empty())
    {
        poArray->RegisterNoDataValue(abyNoData.data());
    }

    const auto gridMapping = oAttributes["grid_mapping"];
    if (gridMapping.GetType() == CPLJSONObject::Type::String)
    {
        const std::string gridMappingName = gridMapping.ToString();
        if (m_oMapMDArrays.find(gridMappingName) == m_oMapMDArrays.end())
        {
            const std::string osArrayFilenameDim = CPLFormFilename(
                CPLFormFilename(m_osDirectoryName.c_str(),
                                gridMappingName.c_str(), nullptr),
                ".zarray", nullptr);
            VSIStatBufL sStat;
            if (VSIStatL(osArrayFilenameDim.c_str(), &sStat) == 0)
            {
                CPLJSONDocument oDoc;
                if (oDoc.Load(osArrayFilenameDim))
                {
                    LoadArray(gridMappingName, osArrayFilenameDim,
                              oDoc.GetRoot(), false, CPLJSONObject());
                }
            }
        }
    }

    poArray->ParseSpecialAttributes(m_pSelf.lock(), oAttributes);
    poArray->SetAttributes(oAttributes);
    poArray->SetDtype(oDtype);
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
