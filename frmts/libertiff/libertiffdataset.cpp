/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  GeoTIFF thread safe reader using libertiff library.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_compressor.h"
#include "cpl_mem_cache.h"
#include "cpl_minixml.h"
#include "cpl_vsi_virtual.h"
#include "cpl_multiproc.h"           // CPLGetNumCPUs()
#include "cpl_worker_thread_pool.h"  // CPLJobQueue, CPLWorkerThreadPool

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <map>
#include <mutex>
#include <type_traits>

#include "gdal_pam.h"
#include "gdal_mdreader.h"
#include "gdal_interpolateatpoint.h"
#include "gdal_thread_pool.h"
#include "memdataset.h"

#define LIBERTIFF_NS GDAL_libertiff
#include "libertiff.hpp"

#include "libtiff_codecs.h"

#define STRINGIFY(x) #x
#define XSTRINGIFY(x) STRINGIFY(x)

/************************************************************************/
/*                       LIBERTIFFDatasetFileReader                     */
/************************************************************************/

struct LIBERTIFFDatasetFileReader final : public LIBERTIFF_NS::FileReader
{
    VSILFILE *const m_fp;
    const bool m_bHasPread;
    mutable bool m_bPReadAllowed = false;
    mutable uint64_t m_nFileSize = 0;
    mutable std::mutex m_oMutex{};

    explicit LIBERTIFFDatasetFileReader(VSILFILE *fp)
        : m_fp(fp), m_bHasPread(m_fp->HasPRead())
    {
    }

    uint64_t size() const override
    {
        std::lock_guard oLock(m_oMutex);
        if (m_nFileSize == 0)
        {
            m_fp->Seek(0, SEEK_END);
            m_nFileSize = m_fp->Tell();
        }
        return m_nFileSize;
    }

    size_t read(uint64_t offset, size_t count, void *buffer) const override
    {
        if (m_bHasPread && m_bPReadAllowed)
        {
            return m_fp->PRead(buffer, count, offset);
        }
        else
        {
            std::lock_guard oLock(m_oMutex);
            return m_fp->Seek(offset, SEEK_SET) == 0
                       ? m_fp->Read(buffer, 1, count)
                       : 0;
        }
    }

    void setPReadAllowed() const
    {
        m_bPReadAllowed = true;
    }

    CPL_DISALLOW_COPY_ASSIGN(LIBERTIFFDatasetFileReader)
};

/************************************************************************/
/*                         LIBERTIFFDataset                             */
/************************************************************************/

class LIBERTIFFDataset final : public GDALPamDataset
{
  public:
    LIBERTIFFDataset() = default;

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *OpenStatic(GDALOpenInfo *poOpenInfo);

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_aoGCPs.empty() && !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
    }

    CPLErr GetGeoTransform(double *padfGeoTransform) override
    {
        memcpy(padfGeoTransform, m_geotransform.data(),
               m_geotransform.size() * sizeof(double));
        return m_geotransformValid ? CE_None : CE_Failure;
    }

    int GetGCPCount() override
    {
        return static_cast<int>(m_aoGCPs.size());
    }

    const OGRSpatialReference *GetGCPSpatialRef() const override
    {
        return !m_aoGCPs.empty() && !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
    }

    const GDAL_GCP *GetGCPs() override
    {
        return gdal::GCP::c_ptr(m_aoGCPs);
    }

  protected:
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount,
                     BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    CPLErr BlockBasedRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                              int nXSize, int nYSize, void *pData,
                              int nBufXSize, int nBufYSize,
                              GDALDataType eBufType, int nBandCount,
                              const int *panBandMap, GSpacing nPixelSpace,
                              GSpacing nLineSpace, GSpacing nBandSpace,
                              GDALRasterIOExtraArg *psExtraArg) override
    {
        return IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                         nBufXSize, nBufYSize, eBufType, nBandCount,
                         const_cast<BANDMAP_TYPE>(panBandMap), nPixelSpace,
                         nLineSpace, nBandSpace, psExtraArg);
    }

  private:
    friend class LIBERTIFFBand;
    VSIVirtualHandleUniquePtr m_poFile{};
    std::shared_ptr<const LIBERTIFFDatasetFileReader> m_fileReader{};
    std::unique_ptr<const LIBERTIFF_NS::Image> m_image{};
    const CPLCompressor *m_decompressor = nullptr;
    std::shared_ptr<int> m_validityPtr = std::make_shared<int>(0);
    OGRSpatialReference m_oSRS{};
    bool m_geotransformValid = false;
    std::array<double, 6> m_geotransform{1, 0, 0, 0, 0, 1};
    std::vector<gdal::GCP> m_aoGCPs{};
    std::vector<std::unique_ptr<LIBERTIFFDataset>> m_apoOvrDSOwned{};
    std::vector<LIBERTIFFDataset *> m_apoOvrDS{};
    GDALRasterBand *m_poAlphaBand = nullptr;
    std::unique_ptr<LIBERTIFFDataset> m_poMaskDS{};
    bool m_bExpand1To255 = false;
    std::vector<uint8_t> m_jpegTablesOri{};
    std::vector<uint8_t> m_jpegTables{};
    std::vector<uint32_t> m_tileOffsets{};
    std::vector<uint64_t> m_tileOffsets64{};
    std::vector<uint32_t> m_tileByteCounts{};
    int m_lercVersion = LERC_VERSION_2_4;
    int m_lercAdditionalCompression = LERC_ADD_COMPRESSION_NONE;
    std::vector<uint16_t> m_extraSamples{};
    CPLWorkerThreadPool *m_poThreadPool = nullptr;

    struct ThreadLocalState
    {
      private:
        std::weak_ptr<int> m_validityTest{};

      public:
        explicit ThreadLocalState(const LIBERTIFFDataset *ds)
            : m_validityTest(ds->m_validityPtr)
        {
            memset(&m_tiff, 0, sizeof(m_tiff));
        }

        ~ThreadLocalState()
        {
            if (m_tiff.tif_cleanup)
                m_tiff.tif_cleanup(&m_tiff);
        }

        inline bool isValid() const
        {
            return m_validityTest.lock() != nullptr;
        }

        // Used by IRasterIO()
        std::vector<GByte> m_abyIRasterIOBuffer{};

        // Used by ReadBlock()
        uint64_t m_curStrileIdx = std::numeric_limits<uint64_t>::max();
        bool m_curStrileMissing = false;
        std::vector<GByte> m_decompressedBuffer{};
        std::vector<GByte> m_compressedBuffer{};
        std::vector<GByte> m_bufferForOneBitExpansion{};
        std::vector<void *> m_apabyDest{};
        std::vector<uint8_t> m_floatingPointHorizPredictorDecodeTmpBuffer{};

        TIFF m_tiff{};
    };

    GDALDataType ComputeGDALDataType() const;
    bool ProcessCompressionMethod();

    bool Open(std::unique_ptr<const LIBERTIFF_NS::Image> image);

    bool Open(GDALOpenInfo *poOpenInfo);

    ThreadLocalState &GetTLSState() const;

    void ReadSRS();
    void ReadGeoTransform();
    void ReadRPCTag();

    bool ReadBlock(GByte *pabyBlockData, int nBlockXOff, int nBlockYOff,
                   int nBandCount, BANDMAP_TYPE panBandMap,
                   GDALDataType eBufType, GSpacing nPixelSpace,
                   GSpacing nLineSpace, GSpacing nBandSpace) const;

    CPL_DISALLOW_COPY_ASSIGN(LIBERTIFFDataset)
};

/************************************************************************/
/*                          LIBERTIFFBand                               */
/************************************************************************/

class LIBERTIFFBand final : public GDALPamRasterBand
{
  public:
    LIBERTIFFBand(LIBERTIFFDataset *poDSIn, int nBandIn, GDALDataType eDT,
                  int nBlockXSizeIn, int nBlockYSizeIn)
    {
        poDS = poDSIn;
        nBand = nBandIn;
        eDataType = eDT;
        nBlockXSize = nBlockXSizeIn;
        nBlockYSize = nBlockYSizeIn;
    }

    double GetNoDataValue(int *pbHasNoData) override
    {
        if (pbHasNoData)
            *pbHasNoData = m_bHasNoData;
        return m_dfNoData;
    }

    double GetScale(int *pbHasNoData) override
    {
        if (pbHasNoData)
            *pbHasNoData = m_bHaveOffsetScale;
        return m_dfScale;
    }

    double GetOffset(int *pbHasNoData) override
    {
        if (pbHasNoData)
            *pbHasNoData = m_bHaveOffsetScale;
        return m_dfOffset;
    }

    const char *GetDescription() const override
    {
        return m_osDescription.c_str();
    }

    const char *GetUnitType() override
    {
        return m_osUnitType.c_str();
    }

    GDALColorInterp GetColorInterpretation() override
    {
        return m_eColorInterp;
    }

    GDALColorTable *GetColorTable() override
    {
        return m_poColorTable.get();
    }

    int GetOverviewCount() override
    {
        auto l_poDS = cpl::down_cast<LIBERTIFFDataset *>(poDS);
        return static_cast<int>(l_poDS->m_apoOvrDS.size());
    }

    GDALRasterBand *GetOverview(int idx) override
    {
        auto l_poDS = cpl::down_cast<LIBERTIFFDataset *>(poDS);
        if (idx >= 0 && idx < GetOverviewCount())
            return l_poDS->m_apoOvrDS[idx]->GetRasterBand(nBand);
        return nullptr;
    }

    int GetMaskFlags() override
    {
        return nMaskFlags;
    }

    GDALRasterBand *GetMaskBand() override
    {
        return poMask.get();
    }

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override;

    CPLErr InterpolateAtPoint(double dfPixel, double dfLine,
                              GDALRIOResampleAlg eInterpolation,
                              double *pdfRealValue,
                              double *pdfImagValue = nullptr) const override;

    // We could do a smarter implementation by manually managing blocks in
    // the TLS structure, but given we should rarely use that method, the
    // current approach with a mutex should be good enough
    GDALRasterBlock *GetLockedBlockRef(int nXBlockOff, int nYBlockOff,
                                       int bJustInitialize = FALSE) override
    {
        if (!m_bDebugGetLockedBlockRef)
        {
            m_bDebugGetLockedBlockRef = true;
            CPLDebug("LIBERTIFF", "GetLockedBlockRef() called");
        }
        std::lock_guard oLock(m_oMutexBlockCache);
        // coverity[sleep]
        return GDALRasterBand::GetLockedBlockRef(nXBlockOff, nYBlockOff,
                                                 bJustInitialize);
    }

    GDALRasterBlock *TryGetLockedBlockRef(int nXBlockOff,
                                          int nYBlockOff) override
    {
        std::lock_guard oLock(m_oMutexBlockCache);
        return GDALRasterBand::TryGetLockedBlockRef(nXBlockOff, nYBlockOff);
    }

    CPLErr FlushBlock(int nXBlockOff, int nYBlockOff,
                      int bWriteDirtyBlock = TRUE) override
    {
        std::lock_guard oLock(m_oMutexBlockCache);
        return GDALRasterBand::FlushBlock(nXBlockOff, nYBlockOff,
                                          bWriteDirtyBlock);
    }

  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) override;

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override
    {
        int anBand[] = {nBand};
        return cpl::down_cast<LIBERTIFFDataset *>(poDS)->IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, 1, anBand, nPixelSpace, nLineSpace, 0, psExtraArg);
    }

  private:
    friend class LIBERTIFFDataset;

    std::recursive_mutex m_oMutexBlockCache{};
    GDALColorInterp m_eColorInterp = GCI_Undefined;
    std::unique_ptr<GDALColorTable> m_poColorTable{};
    bool m_bHasNoData = false;
    bool m_bHaveOffsetScale = false;
    bool m_bDebugGetLockedBlockRef = false;
    double m_dfNoData = 0;
    double m_dfScale = 1.0;
    double m_dfOffset = 0.0;
    std::string m_osUnitType{};
    std::string m_osDescription{};

    struct ThreadLocalState
    {
      private:
        std::weak_ptr<int> m_validityTest{};

      public:
        explicit ThreadLocalState(const LIBERTIFFBand *band)
            : m_validityTest(
                  cpl::down_cast<LIBERTIFFDataset *>(band->poDS)->m_validityPtr)
        {
        }

        inline bool isValid() const
        {
            return m_validityTest.lock() != nullptr;
        }

        GDALDoublePointsCache m_pointsCache{};
    };

    ThreadLocalState &GetTLSState() const;

    void ReadColorMap();

    void InitMaskBand();
};

/************************************************************************/
/*                           IReadBlock()                               */
/************************************************************************/

CPLErr LIBERTIFFBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pData)
{
    int nXValid, nYValid;
    GetActualBlockSize(nBlockXOff, nBlockYOff, &nXValid, &nYValid);
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    int anBand[] = {nBand};
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    return cpl::down_cast<LIBERTIFFDataset *>(poDS)->IRasterIO(
        GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize, nXValid,
        nYValid, pData, nXValid, nYValid, eDataType, 1, anBand, nDTSize,
        static_cast<GSpacing>(nDTSize) * nBlockXSize, 0, &sExtraArg);
}

/************************************************************************/
/*                           ReadColorMap()                             */
/************************************************************************/

void LIBERTIFFBand::ReadColorMap()
{
    auto l_poDS = cpl::down_cast<LIBERTIFFDataset *>(poDS);

    const auto psTagColorMap =
        l_poDS->m_image->tag(LIBERTIFF_NS::TagCode::ColorMap);
    if (psTagColorMap && psTagColorMap->type == LIBERTIFF_NS::TagType::Short &&
        psTagColorMap->count >= 3 && (psTagColorMap->count % 3) == 0 &&
        psTagColorMap->count == (1U << l_poDS->m_image->bitsPerSample()) * 3U &&
        !psTagColorMap->invalid_value_offset)
    {
        bool ok = true;
        const auto colorMap =
            l_poDS->m_image->readTagAsVector<uint16_t>(*psTagColorMap, ok);
        if (colorMap.size() == psTagColorMap->count)
        {
            constexpr int DEFAULT_COLOR_TABLE_MULTIPLIER_257 = 257;
            const int nColorCount = static_cast<int>(psTagColorMap->count) / 3;
            const auto *panRed = colorMap.data();
            const auto *panGreen = colorMap.data() + nColorCount;
            const auto *panBlue = colorMap.data() + 2 * nColorCount;
            int nColorTableMultiplier = 0;
            m_poColorTable = gdal::tiff_common::TIFFColorMapTagToColorTable(
                panRed, panGreen, panBlue, nColorCount, nColorTableMultiplier,
                DEFAULT_COLOR_TABLE_MULTIPLIER_257, m_bHasNoData, m_dfNoData);
            m_eColorInterp = GCI_PaletteIndex;
        }
    }
}

/************************************************************************/
/*                          GetTLSState()                               */
/************************************************************************/

LIBERTIFFBand::ThreadLocalState &LIBERTIFFBand::GetTLSState() const
{
    static thread_local lru11::Cache<const LIBERTIFFBand *,
                                     std::shared_ptr<ThreadLocalState>>
        tlsState;
    std::shared_ptr<ThreadLocalState> value;
    if (tlsState.tryGet(this, value))
    {
        if (value->isValid())
            return *value.get();
    }
    value = std::make_shared<ThreadLocalState>(this);
    tlsState.insert(this, value);
    return *value.get();
}

/************************************************************************/
/*                        InterpolateAtPoint()                          */
/************************************************************************/

CPLErr LIBERTIFFBand::InterpolateAtPoint(double dfPixel, double dfLine,
                                         GDALRIOResampleAlg eInterpolation,
                                         double *pdfRealValue,
                                         double *pdfImagValue) const
{
    if (eInterpolation != GRIORA_NearestNeighbour &&
        eInterpolation != GRIORA_Bilinear && eInterpolation != GRIORA_Cubic &&
        eInterpolation != GRIORA_CubicSpline)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only nearest, bilinear, cubic and cubicspline interpolation "
                 "methods "
                 "allowed");

        return CE_Failure;
    }

    LIBERTIFFBand *pBand = const_cast<LIBERTIFFBand *>(this);
    auto &pointsCache = GetTLSState().m_pointsCache;
    const bool res =
        GDALInterpolateAtPoint(pBand, eInterpolation, pointsCache.cache,
                               dfPixel, dfLine, pdfRealValue, pdfImagValue);

    return res ? CE_None : CE_Failure;
}

/************************************************************************/
/*                           InitMaskBand()                             */
/************************************************************************/

void LIBERTIFFBand::InitMaskBand()
{
    auto l_poDS = cpl::down_cast<LIBERTIFFDataset *>(poDS);
    if (m_bHasNoData)
    {
        nMaskFlags = GMF_NODATA;
        poMask.reset(std::make_unique<GDALNoDataMaskBand>(this));
    }
    else if (l_poDS->m_poMaskDS)
    {
        nMaskFlags = GMF_PER_DATASET;
        poMask.reset(l_poDS->m_poMaskDS->GetRasterBand(1), false);
    }
    else if (l_poDS->m_poAlphaBand && l_poDS->m_poAlphaBand != this)
    {
        nMaskFlags = GMF_PER_DATASET | GMF_ALPHA;
        poMask.reset(l_poDS->m_poAlphaBand, false);
    }
    else
    {
        nMaskFlags = GMF_ALL_VALID;
        poMask.reset(std::make_unique<GDALAllValidMaskBand>(this));
    }
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *LIBERTIFFBand::GetMetadataItem(const char *pszName,
                                           const char *pszDomain)
{

    if (pszName != nullptr && pszDomain != nullptr && EQUAL(pszDomain, "TIFF"))
    {
        int nBlockXOff = 0;
        int nBlockYOff = 0;

        auto l_poDS = cpl::down_cast<LIBERTIFFDataset *>(poDS);

        if (EQUAL(pszName, "JPEGTABLES"))
        {
            if (l_poDS->m_jpegTablesOri.empty())
                return nullptr;
            char *const pszHex =
                CPLBinaryToHex(static_cast<int>(l_poDS->m_jpegTablesOri.size()),
                               l_poDS->m_jpegTablesOri.data());
            const char *pszReturn = CPLSPrintf("%s", pszHex);
            CPLFree(pszHex);

            return pszReturn;
        }

        if (EQUAL(pszName, "IFD_OFFSET"))
        {
            return CPLSPrintf(CPL_FRMT_GUIB,
                              static_cast<GUIntBig>(l_poDS->m_image->offset()));
        }

        if (sscanf(pszName, "BLOCK_OFFSET_%d_%d", &nBlockXOff, &nBlockYOff) ==
            2)
        {
            if (nBlockXOff < 0 ||
                nBlockXOff >= DIV_ROUND_UP(nRasterXSize, nBlockXSize) ||
                nBlockYOff < 0 ||
                nBlockYOff >= DIV_ROUND_UP(nRasterYSize, nBlockYSize))
                return nullptr;

            uint64_t curStrileIdx =
                static_cast<uint64_t>(nBlockYOff) *
                    DIV_ROUND_UP(nRasterXSize, nBlockXSize) +
                nBlockXOff;
            if (l_poDS->m_image->planarConfiguration() ==
                LIBERTIFF_NS::PlanarConfiguration::Separate)
            {
                curStrileIdx += (nBand - 1) *
                                static_cast<uint64_t>(
                                    DIV_ROUND_UP(nRasterXSize, nBlockXSize)) *
                                DIV_ROUND_UP(nRasterYSize, nBlockYSize);
            }

            bool ok = true;
            const uint64_t offset =
                l_poDS->m_image->strileOffset(curStrileIdx, ok);
            if (!offset)
            {
                return nullptr;
            }

            return CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(offset));
        }

        if (sscanf(pszName, "BLOCK_SIZE_%d_%d", &nBlockXOff, &nBlockYOff) == 2)
        {
            if (nBlockXOff < 0 ||
                nBlockXOff >= DIV_ROUND_UP(nRasterXSize, nBlockXSize) ||
                nBlockYOff < 0 ||
                nBlockYOff >= DIV_ROUND_UP(nRasterYSize, nBlockYSize))
                return nullptr;

            uint64_t curStrileIdx =
                static_cast<uint64_t>(nBlockYOff) *
                    DIV_ROUND_UP(nRasterXSize, nBlockXSize) +
                nBlockXOff;
            if (l_poDS->m_image->planarConfiguration() ==
                LIBERTIFF_NS::PlanarConfiguration::Separate)
            {
                curStrileIdx += (nBand - 1) *
                                static_cast<uint64_t>(
                                    DIV_ROUND_UP(nRasterXSize, nBlockXSize)) *
                                DIV_ROUND_UP(nRasterYSize, nBlockYSize);
            }

            bool ok = true;
            const uint64_t size =
                l_poDS->m_image->strileByteCount(curStrileIdx, ok);
            if (!size)
            {
                return nullptr;
            }

            return CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(size));
        }
    }
    return GDALPamRasterBand::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                          GetTLSState()                               */
/************************************************************************/

LIBERTIFFDataset::ThreadLocalState &LIBERTIFFDataset::GetTLSState() const
{
    static thread_local lru11::Cache<const LIBERTIFFDataset *,
                                     std::shared_ptr<ThreadLocalState>>
        tlsState;

    std::shared_ptr<ThreadLocalState> value;
    if (tlsState.tryGet(this, value))
    {
        if (value->isValid())
            return *value.get();
    }
    value = std::make_shared<ThreadLocalState>(this);
    tlsState.insert(this, value);
    return *value.get();
}

/************************************************************************/
/*                           IRasterIO()                                */
/************************************************************************/

CPLErr LIBERTIFFDataset::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
    GSpacing nLineSpace, GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArg)
{
    if (eRWFlag != GF_Read)
        return CE_Failure;

    // Try to pass the request to the most appropriate overview dataset.
    if (nBufXSize < nXSize && nBufYSize < nYSize)
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    const GDALDataType eNativeDT = papoBands[0]->GetRasterDataType();
    const size_t nNativeDTSize =
        static_cast<size_t>(GDALGetDataTypeSizeBytes(eNativeDT));
    int nBlockXSize, nBlockYSize;
    papoBands[0]->GetBlockSize(&nBlockXSize, &nBlockYSize);

    const int iXBlockMin = nXOff / nBlockXSize;
    const int iYBlockMin = nYOff / nBlockYSize;
    if (nXSize == 1 && nYSize == 1 && nBufXSize == 1 && nBufYSize == 1)
    {
        ThreadLocalState &tlsState = GetTLSState();
        const double dfNoData =
            cpl::down_cast<LIBERTIFFBand *>(papoBands[0])->m_dfNoData;
        const size_t nXYOffset =
            static_cast<size_t>(nYOff % nBlockYSize) * nBlockXSize +
            (nXOff % nBlockXSize);
        if (m_image->planarConfiguration() ==
            LIBERTIFF_NS::PlanarConfiguration::Separate)
        {
            for (int iBand = 0; iBand < nBandCount; ++iBand)
            {
                int anBand[] = {panBandMap[iBand]};
                if (!ReadBlock(nullptr, iXBlockMin, iYBlockMin, 1, anBand,
                               eBufType, nPixelSpace, nLineSpace, nBandSpace))
                {
                    return CE_Failure;
                }
                if (tlsState.m_curStrileMissing)
                {
                    GDALCopyWords64(&dfNoData, GDT_Float64, 0,
                                    static_cast<GByte *>(pData) +
                                        iBand * nBandSpace,
                                    eBufType, 0, 1);
                }
                else
                {
                    GDALCopyWords64(tlsState.m_decompressedBuffer.data() +
                                        nNativeDTSize * nXYOffset,
                                    eNativeDT, 0,
                                    static_cast<GByte *>(pData) +
                                        iBand * nBandSpace,
                                    eBufType, 0, 1);
                }
            }
        }
        else
        {
            if (!ReadBlock(nullptr, iXBlockMin, iYBlockMin, nBandCount,
                           panBandMap, eBufType, nPixelSpace, nLineSpace,
                           nBandSpace))
            {
                return CE_Failure;
            }
            for (int iBand = 0; iBand < nBandCount; ++iBand)
            {
                if (tlsState.m_curStrileMissing)
                {
                    GDALCopyWords64(&dfNoData, GDT_Float64, 0,
                                    static_cast<GByte *>(pData) +
                                        iBand * nBandSpace,
                                    eBufType, 0, 1);
                }
                else
                {
                    GDALCopyWords64(
                        tlsState.m_decompressedBuffer.data() +
                            nNativeDTSize *
                                (panBandMap[iBand] - 1 + nXYOffset * nBands),
                        eNativeDT, 0,
                        static_cast<GByte *>(pData) + iBand * nBandSpace,
                        eBufType, 0, 1);
                }
            }
        }
        return CE_None;
    }

    // Check that request is full resolution and aligned on block boundaries
    // (with the exception of the right and bottom most blocks that can be
    // truncated)
    if (nXSize != nBufXSize || nYSize != nBufYSize ||
        (nXOff % nBlockXSize) != 0 || (nYOff % nBlockYSize) != 0 ||
        !(nXOff + nXSize == nRasterXSize || (nBufXSize % nBlockXSize) == 0) ||
        !(nYOff + nYSize == nRasterYSize || (nBufYSize % nBlockYSize) == 0))
    {
        const int nXOffMod = (nXOff / nBlockXSize) * nBlockXSize;
        const int nYOffMod = (nYOff / nBlockYSize) * nBlockYSize;
        const int nXOff2Mod = static_cast<int>(std::min(
            static_cast<int64_t>(nRasterXSize),
            static_cast<int64_t>(DIV_ROUND_UP(nXOff + nXSize, nBlockXSize)) *
                nBlockXSize));
        const int nYOff2Mod = static_cast<int>(std::min(
            static_cast<int64_t>(nRasterYSize),
            static_cast<int64_t>(DIV_ROUND_UP(nYOff + nYSize, nBlockYSize)) *
                nBlockYSize));
        const int nXSizeMod = nXOff2Mod - nXOffMod;
        const int nYSizeMod = nYOff2Mod - nYOffMod;
        std::vector<GByte> &abyTmp = GetTLSState().m_abyIRasterIOBuffer;

        try
        {
            if (nNativeDTSize * nBandCount >
                std::numeric_limits<size_t>::max() /
                    (static_cast<uint64_t>(nXSizeMod) * nYSizeMod))
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Out of memory allocating temporary buffer");
                return CE_Failure;
            }
            const size_t nSize =
                nNativeDTSize * nBandCount * nXSizeMod * nYSizeMod;
            if (abyTmp.size() < nSize)
                abyTmp.resize(nSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory allocating temporary buffer");
            return CE_Failure;
        }

        {
            GDALRasterIOExtraArg sExtraArg;
            INIT_RASTERIO_EXTRA_ARG(sExtraArg);

            if (IRasterIO(GF_Read, nXOffMod, nYOffMod, nXSizeMod, nYSizeMod,
                          abyTmp.data(), nXSizeMod, nYSizeMod, eNativeDT,
                          nBandCount, panBandMap,
                          GDALGetDataTypeSizeBytes(eNativeDT),
                          nNativeDTSize * nXSizeMod,
                          nNativeDTSize * nXSizeMod * nYSizeMod,
                          &sExtraArg) != CE_None)
            {
                return CE_Failure;
            }
        }

        auto poMEMDS = std::unique_ptr<MEMDataset>(MEMDataset::Create(
            "", nXSizeMod, nYSizeMod, 0, GDT_Unknown, nullptr));
        if (!poMEMDS)
        {
            return CE_Failure;
        }
        for (int i = 0; i < nBandCount; ++i)
        {
            GByte *pabyData =
                abyTmp.data() + i * nNativeDTSize * nXSizeMod * nYSizeMod;
            GDALRasterBandH hMEMBand = MEMCreateRasterBandEx(
                poMEMDS.get(), i + 1, pabyData, eNativeDT, nNativeDTSize,
                nNativeDTSize * nXSizeMod, false);
            poMEMDS->AddMEMBand(hMEMBand);
        }

        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);
        // cppcheck-suppress redundantAssignment
        sExtraArg.eResampleAlg = psExtraArg->eResampleAlg;
        sExtraArg.bFloatingPointWindowValidity =
            psExtraArg->bFloatingPointWindowValidity;
        if (sExtraArg.bFloatingPointWindowValidity)
        {
            sExtraArg.dfXOff = psExtraArg->dfXOff - nXOffMod;
            sExtraArg.dfYOff = psExtraArg->dfYOff - nYOffMod;
            sExtraArg.dfXSize = psExtraArg->dfXSize;
            sExtraArg.dfYSize = psExtraArg->dfYSize;
        }
        return poMEMDS->RasterIO(GF_Read, nXOff - nXOffMod, nYOff - nYOffMod,
                                 nXSize, nYSize, pData, nBufXSize, nBufYSize,
                                 eBufType, nBandCount, nullptr, nPixelSpace,
                                 nLineSpace, nBandSpace, &sExtraArg);
    }

    const int iYBlockMax = DIV_ROUND_UP(nYOff + nBufYSize, nBlockYSize);
    const int iXBlockMax = DIV_ROUND_UP(nXOff + nBufXSize, nBlockXSize);

    CPLJobQueuePtr poQueue;
    const bool bIsSeparate = m_image->planarConfiguration() ==
                             LIBERTIFF_NS::PlanarConfiguration::Separate;
    if (m_poThreadPool &&
        (iYBlockMax - iYBlockMin > 1 || iXBlockMax - iXBlockMin > 1 ||
         (bIsSeparate && nBandCount > 1)))
    {
        poQueue = m_poThreadPool->CreateJobQueue();
    }
    std::atomic<bool> bSuccess(true);

    for (int iYBlock = iYBlockMin, iY = 0; iYBlock < iYBlockMax && bSuccess;
         ++iYBlock, ++iY)
    {
        for (int iXBlock = iXBlockMin, iX = 0; iXBlock < iXBlockMax && bSuccess;
             ++iXBlock, ++iX)
        {
            if (bIsSeparate)
            {
                for (int iBand = 0; iBand < nBandCount; ++iBand)
                {
                    const auto lambda = [this, &bSuccess, iBand, panBandMap,
                                         pData, iY, nLineSpace, nBlockYSize, iX,
                                         nPixelSpace, nBlockXSize, nBandSpace,
                                         iXBlock, iYBlock, eBufType]()
                    {
                        int anBand[] = {panBandMap[iBand]};
                        if (!ReadBlock(static_cast<GByte *>(pData) +
                                           iY * nLineSpace * nBlockYSize +
                                           iX * nPixelSpace * nBlockXSize +
                                           iBand * nBandSpace,
                                       iXBlock, iYBlock, 1, anBand, eBufType,
                                       nPixelSpace, nLineSpace, nBandSpace))
                        {
                            bSuccess = false;
                        }
                    };
                    if (poQueue)
                    {
                        poQueue->SubmitJob(lambda);
                    }
                    else
                    {
                        lambda();
                    }
                }
            }
            else
            {
                const auto lambda = [this, &bSuccess, nBandCount, panBandMap,
                                     pData, iY, nLineSpace, nBlockYSize, iX,
                                     nPixelSpace, nBlockXSize, nBandSpace,
                                     iXBlock, iYBlock, eBufType]()
                {
                    if (!ReadBlock(static_cast<GByte *>(pData) +
                                       iY * nLineSpace * nBlockYSize +
                                       iX * nPixelSpace * nBlockXSize,
                                   iXBlock, iYBlock, nBandCount, panBandMap,
                                   eBufType, nPixelSpace, nLineSpace,
                                   nBandSpace))
                    {
                        bSuccess = false;
                    }
                };
                if (poQueue)
                {
                    poQueue->SubmitJob(lambda);
                }
                else
                {
                    lambda();
                }
            }
        }
    }

    if (poQueue)
        poQueue->WaitCompletion();

    return bSuccess ? CE_None : CE_Failure;
}

/************************************************************************/
/*                       HorizPredictorDecode()                         */
/************************************************************************/

template <class T, class U>
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW static void
HorizPredictorDecode1Component(void *bufferIn, size_t nPixelCount)
{
    T *buffer = static_cast<T *>(bufferIn);
    constexpr T mask = std::numeric_limits<T>::max();
    U acc = buffer[0];
    size_t i = 1;
    for (; i + 3 < nPixelCount; i += 4)
    {
        acc += buffer[i];
        buffer[i] = static_cast<T>(acc & mask);
        acc += buffer[i + 1];
        buffer[i + 1] = static_cast<T>(acc & mask);
        acc += buffer[i + 2];
        buffer[i + 2] = static_cast<T>(acc & mask);
        acc += buffer[i + 3];
        buffer[i + 3] = static_cast<T>(acc & mask);
    }
    for (; i < nPixelCount; ++i)
    {
        acc += buffer[i];
        buffer[i] = static_cast<T>(acc & mask);
    }
}

template <class T, class U>
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW static void
HorizPredictorDecode2Components(void *bufferIn, size_t nPixelCount)
{
    T *buffer = static_cast<T *>(bufferIn);
    constexpr T mask = std::numeric_limits<T>::max();
    U acc0 = buffer[0];
    U acc1 = buffer[1];
    for (size_t i = 1; i < nPixelCount; ++i)
    {
        acc0 += buffer[i * 2 + 0];
        acc1 += buffer[i * 2 + 1];
        buffer[i * 2 + 0] = static_cast<T>(acc0 & mask);
        buffer[i * 2 + 1] = static_cast<T>(acc1 & mask);
    }
}

template <class T, class U>
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW static void
HorizPredictorDecode3Components(void *bufferIn, size_t nPixelCount)
{
    T *buffer = static_cast<T *>(bufferIn);
    constexpr T mask = std::numeric_limits<T>::max();
    U acc0 = buffer[0];
    U acc1 = buffer[1];
    U acc2 = buffer[2];
    for (size_t i = 1; i < nPixelCount; ++i)
    {
        acc0 += buffer[i * 3 + 0];
        acc1 += buffer[i * 3 + 1];
        acc2 += buffer[i * 3 + 2];
        buffer[i * 3 + 0] = static_cast<T>(acc0 & mask);
        buffer[i * 3 + 1] = static_cast<T>(acc1 & mask);
        buffer[i * 3 + 2] = static_cast<T>(acc2 & mask);
    }
}

template <class T, class U>
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW static void
HorizPredictorDecode4Components(void *bufferIn, size_t nPixelCount)
{
    T *buffer = static_cast<T *>(bufferIn);
    constexpr T mask = std::numeric_limits<T>::max();
    U acc0 = buffer[0];
    U acc1 = buffer[1];
    U acc2 = buffer[2];
    U acc3 = buffer[3];
    for (size_t i = 1; i < nPixelCount; ++i)
    {
        acc0 += buffer[i * 4 + 0];
        acc1 += buffer[i * 4 + 1];
        acc2 += buffer[i * 4 + 2];
        acc3 += buffer[i * 4 + 3];
        buffer[i * 4 + 0] = static_cast<T>(acc0 & mask);
        buffer[i * 4 + 1] = static_cast<T>(acc1 & mask);
        buffer[i * 4 + 2] = static_cast<T>(acc2 & mask);
        buffer[i * 4 + 3] = static_cast<T>(acc3 & mask);
    }
}

template <class T>
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW static void
HorizPredictorDecode(void *bufferIn, size_t nPixelCount,
                     int nComponentsPerPixel)
{
    static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                  std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>);

    if (nComponentsPerPixel == 1)
    {
        // cppcheck-suppress duplicateBranch
        if constexpr (sizeof(T) < sizeof(uint64_t))
        {
            HorizPredictorDecode1Component<T, uint32_t>(bufferIn, nPixelCount);
        }
        else
        {
            HorizPredictorDecode1Component<T, T>(bufferIn, nPixelCount);
        }
    }
    else if (nComponentsPerPixel == 2)
    {
        // cppcheck-suppress duplicateBranch
        if constexpr (sizeof(T) < sizeof(uint64_t))
        {
            HorizPredictorDecode2Components<T, uint32_t>(bufferIn, nPixelCount);
        }
        else
        {
            HorizPredictorDecode2Components<T, T>(bufferIn, nPixelCount);
        }
    }
    else if (nComponentsPerPixel == 3)
    {
        // cppcheck-suppress duplicateBranch
        if constexpr (sizeof(T) < sizeof(uint64_t))
        {
            HorizPredictorDecode3Components<T, uint32_t>(bufferIn, nPixelCount);
        }
        else
        {
            HorizPredictorDecode3Components<T, T>(bufferIn, nPixelCount);
        }
    }
    else if (nComponentsPerPixel == 4)
    {
        // cppcheck-suppress duplicateBranch
        if constexpr (sizeof(T) < sizeof(uint64_t))
        {
            HorizPredictorDecode4Components<T, uint32_t>(bufferIn, nPixelCount);
        }
        else
        {
            HorizPredictorDecode4Components<T, T>(bufferIn, nPixelCount);
        }
    }
    else
    {
        T *buffer = static_cast<T *>(bufferIn);
        constexpr T mask = std::numeric_limits<T>::max();
        for (size_t i = 1; i < nPixelCount; ++i)
        {
            for (int j = 0; j < nComponentsPerPixel; j++)
            {
                buffer[i * nComponentsPerPixel + j] =
                    static_cast<T>((buffer[i * nComponentsPerPixel + j] +
                                    buffer[(i - 1) * nComponentsPerPixel + j]) &
                                   mask);
            }
        }
    }
}

/************************************************************************/
/*                FloatingPointHorizPredictorDecode()                   */
/************************************************************************/

template <class T>
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW static bool
FloatingPointHorizPredictorDecode(std::vector<uint8_t> &tmpBuffer,
                                  void *bufferIn, size_t nPixelCount,
                                  int nComponentsPerPixel)
{
    uint8_t *buffer = static_cast<uint8_t *>(bufferIn);
    HorizPredictorDecode<uint8_t>(buffer, nPixelCount * sizeof(T),
                                  nComponentsPerPixel);

    const size_t tmpBufferSize = nPixelCount * nComponentsPerPixel * sizeof(T);
    if (tmpBuffer.size() < tmpBufferSize)
    {
        try
        {
            tmpBuffer.resize(tmpBufferSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory in FloatingPointHorizPredictorDecode()");
            return false;
        }
    }
    memcpy(tmpBuffer.data(), buffer, tmpBufferSize);
    constexpr uint32_t bytesPerWords = static_cast<uint32_t>(sizeof(T));
    const size_t wordCount = nPixelCount * nComponentsPerPixel;
    for (size_t iWord = 0; iWord < wordCount; iWord++)
    {
        for (uint32_t iByte = 0; iByte < bytesPerWords; iByte++)
        {
#ifdef CPL_MSB
            buffer[bytesPerWords * iWord + iByte] =
                tmpBuffer[iByte * wordCount + iWord];
#else
            buffer[bytesPerWords * iWord + iByte] =
                tmpBuffer[(bytesPerWords - iByte - 1) * wordCount + iWord];
#endif
        }
    }
    return true;
}

/************************************************************************/
/*                           ReadBlock()                                */
/************************************************************************/

bool LIBERTIFFDataset::ReadBlock(GByte *pabyBlockData, int nBlockXOff,
                                 int nBlockYOff, int nBandCount,
                                 BANDMAP_TYPE panBandMap, GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GSpacing nBandSpace) const
{
    uint64_t offset = 0;
    size_t size = 0;
    const bool bSeparate = m_image->planarConfiguration() ==
                           LIBERTIFF_NS::PlanarConfiguration::Separate;

    ThreadLocalState &tlsState = GetTLSState();

    const int iBandTIFFFirst = bSeparate ? panBandMap[0] - 1 : 0;
    uint64_t curStrileIdx;
    if (m_image->isTiled())
    {
        bool ok = true;
        curStrileIdx = m_image->tileCoordinateToIdx(nBlockXOff, nBlockYOff,
                                                    iBandTIFFFirst, ok);
    }
    else
    {
        if (bSeparate)
            curStrileIdx =
                nBlockYOff + DIV_ROUND_UP(m_image->height(),
                                          m_image->rowsPerStripSanitized()) *
                                 static_cast<uint64_t>(iBandTIFFFirst);
        else
            curStrileIdx = nBlockYOff;
    }
    if (curStrileIdx != tlsState.m_curStrileIdx)
    {
        bool ok = true;
        offset = curStrileIdx < m_tileOffsets.size()
                     ? m_tileOffsets[static_cast<size_t>(curStrileIdx)]
                 : curStrileIdx < m_tileOffsets64.size()
                     ? m_tileOffsets64[static_cast<size_t>(curStrileIdx)]
                     : m_image->strileOffset(curStrileIdx, ok);
        if (!ok)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read strile offset");
            return false;
        }
        const uint64_t size64 =
            curStrileIdx < m_tileByteCounts.size()
                ? m_tileByteCounts[static_cast<size_t>(curStrileIdx)]
                : m_image->strileByteCount(curStrileIdx, ok);
        if (!ok)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read strile size");
            return false;
        }

        if constexpr (sizeof(size_t) < sizeof(uint64_t))
        {
            if (size64 > std::numeric_limits<size_t>::max() - 1)
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Too large strile");
                return false;
            }
        }
        size = static_cast<size_t>(size64);
        // Avoid doing non-sensical memory allocations
        constexpr size_t THRESHOLD_CHECK_FILE_SIZE = 10 * 1024 * 1024;
        if (size > THRESHOLD_CHECK_FILE_SIZE &&
            size > m_image->readContext()->size())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Strile size larger than file size");
            return false;
        }
    }

    const GDALDataType eNativeDT = papoBands[0]->GetRasterDataType();
    int nBlockXSize, nBlockYSize;
    papoBands[0]->GetBlockSize(&nBlockXSize, &nBlockYSize);
    const int nBlockActualXSize =
        std::min(nBlockXSize, nRasterXSize - nBlockXOff * nBlockXSize);
    const int nBlockActualYSize =
        std::min(nBlockYSize, nRasterYSize - nBlockYOff * nBlockYSize);

    // Sparse block?
    if ((curStrileIdx != tlsState.m_curStrileIdx && size == 0) ||
        (curStrileIdx == tlsState.m_curStrileIdx &&
         tlsState.m_curStrileMissing))
    {
        if (pabyBlockData)
        {
            const double dfNoData =
                cpl::down_cast<LIBERTIFFBand *>(papoBands[0])->m_dfNoData;
            for (int iBand = 0; iBand < nBandCount; ++iBand)
            {
                for (int iY = 0; iY < nBlockActualYSize; ++iY)
                {
                    GDALCopyWords64(&dfNoData, GDT_Float64, 0,
                                    pabyBlockData + iBand * nBandSpace +
                                        iY * nLineSpace,
                                    eBufType, static_cast<int>(nPixelSpace),
                                    nBlockActualXSize);
                }
            }
        }

        tlsState.m_curStrileIdx = curStrileIdx;
        tlsState.m_curStrileMissing = true;
        return true;
    }

    std::vector<GByte> &abyDecompressedStrile = tlsState.m_decompressedBuffer;
    const size_t nNativeDTSize =
        static_cast<size_t>(GDALGetDataTypeSizeBytes(eNativeDT));

    if (curStrileIdx != tlsState.m_curStrileIdx)
    {
        std::vector<GByte> &bufferForOneBitExpansion =
            tlsState.m_bufferForOneBitExpansion;

        // Overflow in multiplication checked in Open() method
        const int nComponentsPerPixel = bSeparate ? 1 : nBands;
        const size_t nActualPixelCount =
            static_cast<size_t>(m_image->isTiled() ? nBlockYSize
                                                   : nBlockActualYSize) *
            nBlockXSize;
        const int nLineSizeBytes =
            m_image->bitsPerSample() == 1 ? (nBlockXSize + 7) / 8 : nBlockXSize;
        const size_t nActualUncompressedSize =
            nNativeDTSize *
            static_cast<size_t>(m_image->isTiled() ? nBlockYSize
                                                   : nBlockActualYSize) *
            nLineSizeBytes * nComponentsPerPixel;

        // Allocate buffer for decompressed strile
        if (abyDecompressedStrile.empty())
        {
            const size_t nMaxUncompressedSize =
                nNativeDTSize * nBlockXSize * nBlockYSize * nComponentsPerPixel;
            try
            {
                abyDecompressedStrile.resize(nMaxUncompressedSize);
                if (m_image->bitsPerSample() == 1)
                    bufferForOneBitExpansion.resize(nMaxUncompressedSize);
            }
            catch (const std::exception &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Out of memory allocating temporary buffer");
                return false;
            }
        }

        if (m_image->compression() != LIBERTIFF_NS::Compression::None)
        {
            std::vector<GByte> &abyCompressedStrile =
                tlsState.m_compressedBuffer;
            if (size > 128 && size / 16 > nActualUncompressedSize)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Compressed strile size is much larger than "
                         "uncompressed size");
                return false;
            }
            if (abyCompressedStrile.size() < size + m_jpegTables.size())
            {
                try
                {
                    abyCompressedStrile.resize(size + m_jpegTables.size());
                }
                catch (const std::exception &)
                {
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Out of memory allocating temporary buffer");
                    return false;
                }
            }

            bool ok = true;
            m_image->readContext()->read(offset, size,
                                         abyCompressedStrile.data(), ok);
            if (!ok)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot read strile from disk");
                return false;
            }

            if (!tlsState.m_tiff.tif_decodestrip)
            {
                if (m_image->compression() == LIBERTIFF_NS::Compression::LZW)
                {
                    TIFFInitLZW(&tlsState.m_tiff, m_image->compression());
                }
                else if (m_image->compression() ==
                         LIBERTIFF_NS::Compression::PackBits)
                {
                    TIFFInitPackBits(&tlsState.m_tiff, m_image->compression());
                }
#ifdef LERC_SUPPORT
                else if (m_image->compression() ==
                         LIBERTIFF_NS::Compression::LERC)
                {
                    TIFFInitLERC(&tlsState.m_tiff, m_image->compression());
                    LERCState *sp =
                        reinterpret_cast<LERCState *>(tlsState.m_tiff.tif_data);
                    sp->lerc_version = m_lercVersion;
                    sp->additional_compression = m_lercAdditionalCompression;
                }
#endif

                if (tlsState.m_tiff.tif_decodestrip)
                {
                    tlsState.m_tiff.tif_name =
                        const_cast<char *>(GetDescription());
                    tlsState.m_tiff.tif_dir.td_sampleformat =
                        static_cast<uint16_t>(m_image->sampleFormat());
                    tlsState.m_tiff.tif_dir.td_bitspersample =
                        static_cast<uint16_t>(m_image->bitsPerSample());
                    if (m_image->isTiled())
                    {
                        tlsState.m_tiff.tif_flags = TIFF_ISTILED;
                        tlsState.m_tiff.tif_dir.td_tilewidth =
                            m_image->tileWidth();
                        tlsState.m_tiff.tif_dir.td_tilelength =
                            m_image->tileHeight();
                    }
                    else
                    {
                        tlsState.m_tiff.tif_dir.td_imagewidth =
                            m_image->width();
                        tlsState.m_tiff.tif_dir.td_imagelength =
                            m_image->height();
                        tlsState.m_tiff.tif_dir.td_rowsperstrip =
                            m_image->rowsPerStripSanitized();
                    }
                    tlsState.m_tiff.tif_dir.td_samplesperpixel =
                        static_cast<uint16_t>(m_image->samplesPerPixel());
                    tlsState.m_tiff.tif_dir.td_planarconfig =
                        static_cast<uint16_t>(m_image->planarConfiguration());
                    if (m_extraSamples.size() < 65536)
                    {
                        tlsState.m_tiff.tif_dir.td_extrasamples =
                            static_cast<uint16_t>(m_extraSamples.size());
                        tlsState.m_tiff.tif_dir.td_sampleinfo =
                            const_cast<uint16_t *>(m_extraSamples.data());
                    }
                }
            }

            if (tlsState.m_tiff.tif_decodestrip)
            {
                tlsState.m_tiff.tif_row = nBlockYOff * nBlockYSize;
                tlsState.m_tiff.tif_rawcc = size;
                tlsState.m_tiff.tif_rawdata = abyCompressedStrile.data();
                tlsState.m_tiff.tif_rawcp = tlsState.m_tiff.tif_rawdata;
                if ((tlsState.m_tiff.tif_predecode &&
                     tlsState.m_tiff.tif_predecode(&tlsState.m_tiff, 0) == 0) ||
                    tlsState.m_tiff.tif_decodestrip(
                        &tlsState.m_tiff, abyDecompressedStrile.data(),
                        nActualUncompressedSize, 0) == 0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Decompression failed");
                    return false;
                }
            }
            else if (m_image->compression() ==
                         LIBERTIFF_NS::Compression::JPEG ||
                     m_image->compression() ==
                         LIBERTIFF_NS::Compression::WEBP ||
                     m_image->compression() == LIBERTIFF_NS::Compression::JXL ||
                     m_image->compression() ==
                         LIBERTIFF_NS::Compression::JXL_DNG_1_7)
            {
                size_t blobSize = size;
                const char *drvName =
                    m_image->compression() == LIBERTIFF_NS::Compression::JPEG
                        ? "JPEG"
                    : m_image->compression() == LIBERTIFF_NS::Compression::WEBP
                        ? "WEBP"
                        : "JPEGXL";
                if (m_image->compression() == LIBERTIFF_NS::Compression::JPEG &&
                    size > 2 && !m_jpegTables.empty())
                {
                    // Insert JPEG tables into JPEG blob
                    memmove(abyCompressedStrile.data() + 2 +
                                m_jpegTables.size(),
                            abyCompressedStrile.data() + 2, size - 2);
                    memcpy(abyCompressedStrile.data() + 2, m_jpegTables.data(),
                           m_jpegTables.size());
                    blobSize += m_jpegTables.size();
                }
                const std::string osTmpFilename = VSIMemGenerateHiddenFilename(
                    std::string("tmp.").append(drvName).c_str());
                VSIFCloseL(VSIFileFromMemBuffer(
                    osTmpFilename.c_str(), abyCompressedStrile.data(), blobSize,
                    /* bTakeOwnership = */ false));
                const char *const apszAllowedDrivers[] = {drvName, nullptr};

                CPLConfigOptionSetter oJPEGtoRGBSetter(
                    "GDAL_JPEG_TO_RGB",
                    m_image->compression() == LIBERTIFF_NS::Compression::JPEG &&
                            m_image->samplesPerPixel() == 4 &&
                            m_image->planarConfiguration() ==
                                LIBERTIFF_NS::PlanarConfiguration::Contiguous
                        ? "NO"
                        : "YES",
                    false);

                const char *const apszOpenOptions[] = {
                    m_image->compression() == LIBERTIFF_NS::Compression::WEBP &&
                            nComponentsPerPixel == 4
                        ? "@FORCE_4BANDS=YES"
                        : nullptr,
                    nullptr};

                auto poTmpDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                    osTmpFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    apszAllowedDrivers, apszOpenOptions, nullptr));
                VSIUnlink(osTmpFilename.c_str());
                if (!poTmpDS)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Not a %s blob",
                             drvName);
                    return false;
                }
                if (poTmpDS->GetRasterCount() != nComponentsPerPixel ||
                    poTmpDS->GetRasterXSize() != nBlockXSize ||
                    poTmpDS->GetRasterYSize() !=
                        (m_image->isTiled() ? nBlockYSize : nBlockActualYSize))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s blob has no expected dimensions (%dx%d "
                             "whereas %dx%d expected) or band count (%d "
                             "whereas %d expected)",
                             drvName, poTmpDS->GetRasterXSize(),
                             poTmpDS->GetRasterYSize(), nBlockXSize,
                             m_image->isTiled() ? nBlockYSize
                                                : nBlockActualYSize,
                             poTmpDS->GetRasterCount(), nComponentsPerPixel);
                    return false;
                }
                GDALRasterIOExtraArg sExtraArg;
                INIT_RASTERIO_EXTRA_ARG(sExtraArg);
                if (poTmpDS->RasterIO(
                        GF_Read, 0, 0, poTmpDS->GetRasterXSize(),
                        poTmpDS->GetRasterYSize(), abyDecompressedStrile.data(),
                        poTmpDS->GetRasterXSize(), poTmpDS->GetRasterYSize(),
                        eNativeDT, poTmpDS->GetRasterCount(), nullptr,
                        nNativeDTSize * nComponentsPerPixel,
                        nNativeDTSize * nComponentsPerPixel * nBlockXSize,
                        nNativeDTSize, &sExtraArg) != CE_None)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Decompression failed");
                    return false;
                }
            }
            else
            {
                CPLAssert(m_decompressor);
                void *output_data = abyDecompressedStrile.data();
                size_t output_size = nActualUncompressedSize;
                if (!m_decompressor->pfnFunc(
                        abyCompressedStrile.data(), size, &output_data,
                        &output_size, nullptr, m_decompressor->user_data) ||
                    output_size != nActualUncompressedSize)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Decompression failed");
                    return false;
                }
                CPLAssert(output_data == abyDecompressedStrile.data());
            }
        }
        else
        {
            if (size != nActualUncompressedSize)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Strile size != expected size");
                return false;
            }

            bool ok = true;
            m_image->readContext()->read(offset, size,
                                         abyDecompressedStrile.data(), ok);
            if (!ok)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot read strile from disk");
                return false;
            }
        }

        if (m_image->bitsPerSample() == 1)
        {
            const GByte *CPL_RESTRICT pabySrc = abyDecompressedStrile.data();
            GByte *CPL_RESTRICT pabyDst = bufferForOneBitExpansion.data();
            for (int iY = 0; iY < nBlockActualYSize; ++iY)
            {
                if (m_bExpand1To255)
                {
                    GDALExpandPackedBitsToByteAt0Or255(pabySrc, pabyDst,
                                                       nBlockXSize);
                }
                else
                {
                    GDALExpandPackedBitsToByteAt0Or1(pabySrc, pabyDst,
                                                     nBlockXSize);
                }
                pabySrc += (nBlockXSize + 7) / 8;
                pabyDst += nBlockXSize;
            }

            std::swap(abyDecompressedStrile, bufferForOneBitExpansion);
        }
        else if (m_image->compression() == LIBERTIFF_NS::Compression::None ||
                 m_image->compression() == LIBERTIFF_NS::Compression::LZW ||
                 m_decompressor)
        {
            if (m_image->readContext()->mustByteSwap() &&
                m_image->predictor() != 3)
            {
                if (GDALDataTypeIsComplex(eNativeDT))
                {
                    GDALSwapWordsEx(abyDecompressedStrile.data(),
                                    static_cast<int>(nNativeDTSize) / 2,
                                    nActualPixelCount * nComponentsPerPixel * 2,
                                    static_cast<int>(nNativeDTSize) / 2);
                }
                else
                {
                    GDALSwapWordsEx(abyDecompressedStrile.data(),
                                    static_cast<int>(nNativeDTSize),
                                    nActualPixelCount * nComponentsPerPixel,
                                    static_cast<int>(nNativeDTSize));
                }
            }

            if (m_image->predictor() == 2)
            {
                for (int iY = 0; iY < nBlockActualYSize; ++iY)
                {
                    auto ptr =
                        abyDecompressedStrile.data() +
                        nNativeDTSize * iY * nBlockXSize * nComponentsPerPixel;
                    if (nNativeDTSize == sizeof(uint8_t))
                    {
                        HorizPredictorDecode<uint8_t>(ptr, nBlockXSize,
                                                      nComponentsPerPixel);
                    }
                    else if (nNativeDTSize == sizeof(uint16_t))
                    {
                        HorizPredictorDecode<uint16_t>(ptr, nBlockXSize,
                                                       nComponentsPerPixel);
                    }
                    else if (nNativeDTSize == sizeof(uint32_t))
                    {
                        HorizPredictorDecode<uint32_t>(ptr, nBlockXSize,
                                                       nComponentsPerPixel);
                    }
                    else if (nNativeDTSize == sizeof(uint64_t))
                    {
                        HorizPredictorDecode<uint64_t>(ptr, nBlockXSize,
                                                       nComponentsPerPixel);
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
            }
            else if (m_image->predictor() == 3)
            {
                for (int iY = 0; iY < nBlockActualYSize; ++iY)
                {
                    auto ptr =
                        abyDecompressedStrile.data() +
                        nNativeDTSize * iY * nBlockXSize * nComponentsPerPixel;
                    bool ok = true;
                    if (nNativeDTSize == sizeof(uint32_t))
                    {
                        ok = FloatingPointHorizPredictorDecode<uint32_t>(
                            tlsState
                                .m_floatingPointHorizPredictorDecodeTmpBuffer,
                            ptr, nBlockXSize, nComponentsPerPixel);
                    }
                    else if (nNativeDTSize == sizeof(uint64_t))
                    {
                        ok = FloatingPointHorizPredictorDecode<uint64_t>(
                            tlsState
                                .m_floatingPointHorizPredictorDecodeTmpBuffer,
                            ptr, nBlockXSize, nComponentsPerPixel);
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                    if (!ok)
                        return false;
                }
            }
        }
    }

    // Copy decompress strile into user buffer
    if (pabyBlockData)
    {
        const auto IsContiguousBandMap = [nBandCount, panBandMap]()
        {
            for (int i = 0; i < nBandCount; ++i)
            {
                if (panBandMap[i] != i + 1)
                    return false;
            }
            return true;
        };

        const int nBufTypeSize = GDALGetDataTypeSizeBytes(eBufType);
        if (!bSeparate && nBands > 1 && nBands == nBandCount &&
            nBufTypeSize == nPixelSpace && IsContiguousBandMap())
        {
            // Optimization: reading a pixel-interleaved buffer into a band-interleaved buffer
            std::vector<void *> &apabyDest = tlsState.m_apabyDest;
            apabyDest.resize(nBands);
            for (int iBand = 0; iBand < nBandCount; ++iBand)
            {
                apabyDest[iBand] = pabyBlockData + iBand * nBandSpace;
            }
            for (int iY = 0; iY < nBlockActualYSize; ++iY)
            {
                if (iY > 0)
                {
                    for (int iBand = 0; iBand < nBandCount; ++iBand)
                    {
                        apabyDest[iBand] =
                            static_cast<GByte *>(apabyDest[iBand]) + nLineSpace;
                    }
                }
                GDALDeinterleave(abyDecompressedStrile.data() +
                                     nNativeDTSize * iY * nBlockXSize * nBands,
                                 eNativeDT, nBands, apabyDest.data(), eBufType,
                                 nBlockActualXSize);
            }
        }
        else if (!bSeparate && nBands == nBandCount &&
                 nBufTypeSize == nBandSpace &&
                 nPixelSpace == nBandSpace * nBandCount &&
                 IsContiguousBandMap())
        {
            // Optimization reading a pixel-interleaved buffer into a pixel-interleaved buffer
            for (int iY = 0; iY < nBlockActualYSize; ++iY)
            {
                GDALCopyWords64(
                    abyDecompressedStrile.data() +
                        nNativeDTSize * iY * nBlockXSize * nBands,
                    eNativeDT, static_cast<int>(nNativeDTSize),
                    pabyBlockData + iY * nLineSpace, eBufType, nBufTypeSize,
                    static_cast<GPtrDiff_t>(
                        static_cast<GIntBig>(nBlockActualXSize) * nBands));
            }
        }
        else if (!bSeparate && nBands == nBandCount &&
                 nBufTypeSize == nBandSpace &&
                 eBufType == papoBands[0]->GetRasterDataType() &&
                 nPixelSpace > nBandSpace * nBandCount &&
                 nLineSpace >= nPixelSpace * nBlockXSize &&
                 IsContiguousBandMap())
        {
            // Optimization for typically reading a pixel-interleaved RGB buffer
            // into a pixel-interleaved RGBA buffer
            for (int iY = 0; iY < nBlockActualYSize; ++iY)
            {
                GByte *const pabyDst = pabyBlockData + iY * nLineSpace;
                const GByte *const pabySrc =
                    abyDecompressedStrile.data() +
                    nNativeDTSize * iY * nBlockXSize * nBands;
                if (nBands == 3 && nPixelSpace == 4 && nBufTypeSize == 1)
                {
                    for (int iX = 0; iX < nBlockActualXSize; ++iX)
                    {
                        memcpy(pabyDst + iX * 4, pabySrc + iX * 3, 3);
                    }
                }
                else
                {
                    for (int iX = 0; iX < nBlockActualXSize; ++iX)
                    {
                        memcpy(pabyDst + iX * nPixelSpace,
                               pabySrc + iX * nBands, nBands * nBufTypeSize);
                    }
                }
            }
        }
        else
        {
            // General case
            const int nSrcPixels = bSeparate ? 1 : nBands;
            for (int iBand = 0; iBand < nBandCount; ++iBand)
            {
                const int iSrcBand = bSeparate ? 0 : panBandMap[iBand] - 1;
                for (int iY = 0; iY < nBlockActualYSize; ++iY)
                {
                    GDALCopyWords64(
                        abyDecompressedStrile.data() +
                            nNativeDTSize *
                                (iY * nBlockXSize * nSrcPixels + iSrcBand),
                        eNativeDT, static_cast<int>(nSrcPixels * nNativeDTSize),
                        pabyBlockData + iBand * nBandSpace + iY * nLineSpace,
                        eBufType, static_cast<int>(nPixelSpace),
                        nBlockActualXSize);
                }
            }
        }
    }

    tlsState.m_curStrileIdx = curStrileIdx;
    tlsState.m_curStrileMissing = false;

    return true;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

/* static */ int LIBERTIFFDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    return poOpenInfo->eAccess != GA_Update &&
           (STARTS_WITH_CI(poOpenInfo->pszFilename, "GTIFF_DIR:") ||
            (poOpenInfo->fpL && poOpenInfo->nHeaderBytes >= 8 &&
             (((poOpenInfo->pabyHeader[0] == 'I' &&
                poOpenInfo->pabyHeader[1] == 'I') &&
               ((poOpenInfo->pabyHeader[2] == 0x2A &&
                 poOpenInfo->pabyHeader[3] == 0) ||
                (poOpenInfo->pabyHeader[2] == 0x2B &&
                 poOpenInfo->pabyHeader[3] == 0))) ||
              ((poOpenInfo->pabyHeader[0] == 'M' &&
                poOpenInfo->pabyHeader[1] == 'M') &&
               ((poOpenInfo->pabyHeader[2] == 0 &&
                 poOpenInfo->pabyHeader[3] == 0x2A) ||
                (poOpenInfo->pabyHeader[2] == 0 &&
                 poOpenInfo->pabyHeader[3] == 0x2B))))));
}

/************************************************************************/
/*                        ComputeGDALDataType()                         */
/************************************************************************/

GDALDataType LIBERTIFFDataset::ComputeGDALDataType() const
{

    GDALDataType eDT = GDT_Unknown;

    switch (m_image->sampleFormat())
    {
        case LIBERTIFF_NS::SampleFormat::UnsignedInt:
        {
            if (m_image->bitsPerSample() == 1 &&
                (m_image->samplesPerPixel() == 1 ||
                 m_image->planarConfiguration() ==
                     LIBERTIFF_NS::PlanarConfiguration::Separate))
            {
                eDT = GDT_Byte;
            }
            else if (m_image->bitsPerSample() == 8)
                eDT = GDT_Byte;
            else if (m_image->bitsPerSample() == 16)
                eDT = GDT_UInt16;
            else if (m_image->bitsPerSample() == 32)
                eDT = GDT_UInt32;
            else if (m_image->bitsPerSample() == 64)
                eDT = GDT_UInt64;
            break;
        }

        case LIBERTIFF_NS::SampleFormat::SignedInt:
        {
            if (m_image->bitsPerSample() == 8)
                eDT = GDT_Int8;
            else if (m_image->bitsPerSample() == 16)
                eDT = GDT_Int16;
            else if (m_image->bitsPerSample() == 32)
                eDT = GDT_Int32;
            else if (m_image->bitsPerSample() == 64)
                eDT = GDT_Int64;
            break;
        }

        case LIBERTIFF_NS::SampleFormat::IEEEFP:
        {
            if (m_image->bitsPerSample() == 32)
                eDT = GDT_Float32;
            else if (m_image->bitsPerSample() == 64)
                eDT = GDT_Float64;
            break;
        }

        case LIBERTIFF_NS::SampleFormat::ComplexInt:
        {
            if (m_image->bitsPerSample() == 32)
                eDT = GDT_CInt16;
            else if (m_image->bitsPerSample() == 64)
                eDT = GDT_CInt32;
            break;
        }

        case LIBERTIFF_NS::SampleFormat::ComplexIEEEFP:
        {
            if (m_image->bitsPerSample() == 64)
                eDT = GDT_CFloat32;
            else if (m_image->bitsPerSample() == 128)
                eDT = GDT_CFloat64;
            break;
        }

        default:
            break;
    }

    if (m_image->bitsPerSample() == 12 &&
        m_image->compression() == LIBERTIFF_NS::Compression::JPEG)
    {
        auto poJPEGDrv = GetGDALDriverManager()->GetDriverByName("JPEG");
        if (poJPEGDrv)
        {
            const char *pszJPEGDataTypes =
                poJPEGDrv->GetMetadataItem(GDAL_DMD_CREATIONDATATYPES);
            if (pszJPEGDataTypes && strstr(pszJPEGDataTypes, "UInt16"))
                eDT = GDT_UInt16;
        }
    }

    return eDT;
}

/************************************************************************/
/*                       ProcessCompressionMethod()                     */
/************************************************************************/

bool LIBERTIFFDataset::ProcessCompressionMethod()
{
    if (m_image->compression() == LIBERTIFF_NS::Compression::PackBits)
    {
        GDALDataset::SetMetadataItem("COMPRESSION", "PACKBITS",
                                     "IMAGE_STRUCTURE");
    }
    else if (m_image->compression() == LIBERTIFF_NS::Compression::Deflate ||
             m_image->compression() == LIBERTIFF_NS::Compression::LegacyDeflate)
    {
        m_decompressor = CPLGetDecompressor("zlib");
        GDALDataset::SetMetadataItem("COMPRESSION", "DEFLATE",
                                     "IMAGE_STRUCTURE");
    }
    else if (m_image->compression() == LIBERTIFF_NS::Compression::ZSTD)
    {
        m_decompressor = CPLGetDecompressor("zstd");
        if (!m_decompressor)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Compression = ZSTD unhandled because GDAL "
                        "has not been built against libzstd");
            return false;
        }
        GDALDataset::SetMetadataItem("COMPRESSION", "ZSTD", "IMAGE_STRUCTURE");
    }
    else if (m_image->compression() == LIBERTIFF_NS::Compression::LZMA)
    {
        m_decompressor = CPLGetDecompressor("lzma");
        if (!m_decompressor)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Compression = LZMA unhandled because GDAL "
                        "has not been built against liblzma");
            return false;
        }
        GDALDataset::SetMetadataItem("COMPRESSION", "LZMA", "IMAGE_STRUCTURE");
    }
    else if (m_image->compression() == LIBERTIFF_NS::Compression::LZW)
    {
        GDALDataset::SetMetadataItem("COMPRESSION", "LZW", "IMAGE_STRUCTURE");
    }
    else if (m_image->compression() == LIBERTIFF_NS::Compression::JPEG)
    {
        if (!GDALGetDriverByName("JPEG"))
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "Compression = JPEG not supported because JPEG driver missing");
            return false;
        }
        if (m_image->photometricInterpretation() ==
                LIBERTIFF_NS::PhotometricInterpretation::YCbCr &&
            m_image->samplesPerPixel() == 3)
        {
            GDALDataset::SetMetadataItem("SOURCE_COLOR_SPACE", "YCbCr",
                                         "IMAGE_STRUCTURE");
            GDALDataset::SetMetadataItem("COMPRESSION", "YCbCr JPEG",
                                         "IMAGE_STRUCTURE");
        }
        else
        {
            GDALDataset::SetMetadataItem("COMPRESSION", "JPEG",
                                         "IMAGE_STRUCTURE");
        }
        if (m_image->samplesPerPixel() != 1 &&
            m_image->samplesPerPixel() != 3 &&
            m_image->samplesPerPixel() != 4 &&
            m_image->planarConfiguration() ==
                LIBERTIFF_NS::PlanarConfiguration::Contiguous)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Compression = JPEG not supported when samplesPerPixel "
                        "!= 1, 3 or 4 and planarConfiguration = Contiguous");
            return false;
        }

        const auto psJPEGTablesTag =
            m_image->tag(LIBERTIFF_NS::TagCode::JPEGTables);
        if (psJPEGTablesTag &&
            psJPEGTablesTag->type == LIBERTIFF_NS::TagType::Undefined &&
            psJPEGTablesTag->count > 4 &&
            !psJPEGTablesTag->invalid_value_offset &&
            psJPEGTablesTag->count < 65536)
        {
            bool ok = true;
            m_jpegTablesOri =
                m_image->readTagAsVector<uint8_t>(*psJPEGTablesTag, ok);
            if (m_jpegTablesOri.size() >= 4 && m_jpegTablesOri[0] == 0xff &&
                m_jpegTablesOri[1] == 0xd8 &&
                m_jpegTablesOri[m_jpegTablesOri.size() - 2] == 0xff &&
                m_jpegTablesOri.back() == 0xd9)
            {
                m_jpegTables.insert(
                    m_jpegTables.end(), m_jpegTablesOri.data() + 2,
                    m_jpegTablesOri.data() + m_jpegTablesOri.size() - 2);
            }
        }

        if (m_image->samplesPerPixel() == 4 &&
            m_image->planarConfiguration() ==
                LIBERTIFF_NS::PlanarConfiguration::Contiguous)
        {
            const GByte abyAdobeAPP14RGB[] = {
                0xFF, 0xEE, 0x00, 0x0E, 0x41, 0x64, 0x6F, 0x62,
                0x65, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00};
            m_jpegTables.insert(m_jpegTables.end(), abyAdobeAPP14RGB,
                                abyAdobeAPP14RGB + sizeof(abyAdobeAPP14RGB));
        }
    }
    else if (m_image->compression() == LIBERTIFF_NS::Compression::WEBP)
    {
        if (!GDALGetDriverByName("WEBP"))
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "Compression = WEBP not supported because WEBP driver missing");
            return false;
        }
        GDALDataset::SetMetadataItem("COMPRESSION", "WEBP", "IMAGE_STRUCTURE");
    }
    else if (m_image->compression() == LIBERTIFF_NS::Compression::JXL ||
             m_image->compression() == LIBERTIFF_NS::Compression::JXL_DNG_1_7)
    {
        if (!GDALGetDriverByName("JPEGXL"))
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "Compression = JXL not supported because JXL driver missing");
            return false;
        }
        GDALDataset::SetMetadataItem("COMPRESSION", "JXL", "IMAGE_STRUCTURE");
    }
    else if (m_image->compression() == LIBERTIFF_NS::Compression::LERC)
    {
#ifndef LERC_SUPPORT
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Compression = LERC not supported because GDAL "
                    "has not been built against liblerc");
        return false;
#else
        const auto *psLercParametersTag =
            m_image->tag(LIBERTIFF_NS::TagCode::LERCParameters);
        if (psLercParametersTag &&
            psLercParametersTag->type == LIBERTIFF_NS::TagType::Long &&
            psLercParametersTag->count == 2)
        {
            bool ok = true;
            const auto lercParameters =
                m_image->readTagAsVector<uint32_t>(*psLercParametersTag, ok);
            if (!ok || lercParameters.size() != 2)
            {
                ReportError(CE_Failure, CPLE_NotSupported,
                            "Tag LERCParameters is invalid");
                return false;
            }
            m_lercVersion = lercParameters[0];
            m_lercAdditionalCompression = lercParameters[1];
#ifndef ZSTD_SUPPORT
            if (m_lercAdditionalCompression == LERC_ADD_COMPRESSION_ZSTD)
            {
                ReportError(
                    CE_Failure, CPLE_NotSupported,
                    "Compression = LERC_ZSTD not supported because GDAL "
                    "has not been built against libzstd");
                return false;
            }
#endif
        }

        GDALDataset::SetMetadataItem(
            "COMPRESSION",
            m_lercAdditionalCompression == LERC_ADD_COMPRESSION_DEFLATE
                ? "LERC_DEFLATE"
            : m_lercAdditionalCompression == LERC_ADD_COMPRESSION_ZSTD
                ? "LERC_ZSTD"
                : "LERC",
            "IMAGE_STRUCTURE");

        if (m_lercVersion == LERC_VERSION_2_4)
        {
            GDALDataset::SetMetadataItem("LERC_VERSION", "2.4",
                                         "IMAGE_STRUCTURE");
        }
#endif
    }
    else if (m_image->compression() != LIBERTIFF_NS::Compression::None)
    {
        CPLDebug("LIBERTIFF", "Compression = %s unhandled",
                 LIBERTIFF_NS::compressionName(m_image->compression()));
        return false;
    }

    return true;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

bool LIBERTIFFDataset::Open(std::unique_ptr<const LIBERTIFF_NS::Image> image)
{
    m_image = std::move(image);

    // Basic sanity checks
    if (m_image->width() == 0 ||
        m_image->width() > static_cast<uint32_t>(INT_MAX) ||
        m_image->height() == 0 ||
        m_image->height() > static_cast<uint32_t>(INT_MAX) ||
        m_image->samplesPerPixel() == 0 ||
        m_image->samplesPerPixel() > static_cast<uint32_t>(INT_MAX))
    {
        CPLDebug("LIBERTIFF", "Invalid width, height, or samplesPerPixel");
        return false;
    }

    nRasterXSize = static_cast<int>(m_image->width());
    nRasterYSize = static_cast<int>(m_image->height());
    const int l_nBands = static_cast<int>(m_image->samplesPerPixel());
    if (!GDALCheckBandCount(l_nBands, false))
        return false;

    if (!ProcessCompressionMethod())
        return false;

    // Compute block size
    int nBlockXSize;
    int nBlockYSize;
    if (m_image->isTiled())
    {
        if (m_image->tileWidth() == 0 ||
            m_image->tileWidth() > static_cast<uint32_t>(INT_MAX) ||
            m_image->tileHeight() == 0 ||
            m_image->tileHeight() > static_cast<uint32_t>(INT_MAX))
        {
            CPLDebug("LIBERTIFF", "Invalid tileWidth or tileHeight");
            return false;
        }
        nBlockXSize = static_cast<int>(m_image->tileWidth());
        nBlockYSize = static_cast<int>(m_image->tileHeight());
    }
    else
    {
        if (m_image->rowsPerStripSanitized() == 0)
        {
            CPLDebug("LIBERTIFF", "Invalid rowsPerStrip");
            return false;
        }
        nBlockXSize = nRasterXSize;
        nBlockYSize = static_cast<int>(m_image->rowsPerStripSanitized());
    }

    const GDALDataType eDT = ComputeGDALDataType();
    if (eDT == GDT_Unknown)
    {
        CPLDebug("LIBERTIFF",
                 "BitsPerSample = %u and SampleFormat=%u unhandled",
                 m_image->bitsPerSample(), m_image->sampleFormat());
        return false;
    }

    // Deal with Predictor tag
    if (m_image->predictor() == 2)
    {
        GDALDataset::SetMetadataItem("PREDICTOR", "2", "IMAGE_STRUCTURE");
    }
    else if (m_image->predictor() == 3)
    {
        if (eDT != GDT_Float32 && eDT != GDT_Float64)
        {
            CPLDebug("LIBERTIFF", "Unhandled predictor=3 with non-float data");
            return false;
        }
        GDALDataset::SetMetadataItem("PREDICTOR", "3", "IMAGE_STRUCTURE");
    }
    else if (m_image->predictor() > 3)
    {
        CPLDebug("LIBERTIFF", "Predictor = %u unhandled", m_image->predictor());
        return false;
    }

    // Deal with PlanarConfiguration tag
    if (m_image->planarConfiguration() ==
            LIBERTIFF_NS::PlanarConfiguration::Separate ||
        m_image->samplesPerPixel() == 1)
        GDALDataset::SetMetadataItem("INTERLEAVE", "BAND", "IMAGE_STRUCTURE");
    else
        GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    const int nNativeDTSize = GDALGetDataTypeSizeBytes(eDT);
    const bool bSeparate = m_image->planarConfiguration() ==
                           LIBERTIFF_NS::PlanarConfiguration::Separate;
    // Sanity check that a strile can its on SIZE_MAX, to avoid further
    // issues in ReadBlock()
    if (static_cast<uint64_t>(nNativeDTSize) * (bSeparate ? 1 : l_nBands) >
        std::numeric_limits<size_t>::max() /
            (static_cast<uint64_t>(nBlockXSize) * nBlockYSize))
    {
        CPLDebug("LIBERTIFF", "Too large block");
        return false;
    }

    // Process GDAL_NODATA tag
    bool bHasNoData = false;
    double dfNoData = 0;
    const auto *tagNoData = m_image->tag(LIBERTIFF_NS::TagCode::GDAL_NODATA);
    if (tagNoData && tagNoData->type == LIBERTIFF_NS::TagType::ASCII &&
        !(tagNoData->count > 4 && tagNoData->invalid_value_offset) &&
        tagNoData->count < 256)
    {
        bool ok = true;
        const std::string noData = m_image->readTagAsString(*tagNoData, ok);
        if (ok && !noData.empty())
        {
            bHasNoData = true;
            dfNoData = CPLAtof(noData.c_str());
        }
    }

    // Process ExtraSamples tag
    int nRegularChannels = 0;
    if (m_image->photometricInterpretation() ==
        LIBERTIFF_NS::PhotometricInterpretation::MinIsBlack)
    {
        nRegularChannels = 1;
    }
    else if (m_image->photometricInterpretation() ==
             LIBERTIFF_NS::PhotometricInterpretation::RGB)
    {
        nRegularChannels = 3;
    }
    const auto *psExtraSamplesTag =
        m_image->tag(LIBERTIFF_NS::TagCode::ExtraSamples);
    if (nRegularChannels > 0 && l_nBands > nRegularChannels &&
        psExtraSamplesTag &&
        psExtraSamplesTag->type == LIBERTIFF_NS::TagType::Short &&
        psExtraSamplesTag->count ==
            static_cast<unsigned>(l_nBands - nRegularChannels))
    {
        bool ok = true;
        m_extraSamples =
            m_image->readTagAsVector<uint16_t>(*psExtraSamplesTag, ok);
    }

    // Preload TileOffsets and TileByteCounts if not too big
    if (m_image->isTiled())
    {
        const auto *psTileOffsets =
            m_image->tag(LIBERTIFF_NS::TagCode::TileOffsets);
        const auto *psTileByteCounts =
            m_image->tag(LIBERTIFF_NS::TagCode::TileByteCounts);
        if (psTileOffsets &&
            (psTileOffsets->type == LIBERTIFF_NS::TagType::Long ||
             psTileOffsets->type == LIBERTIFF_NS::TagType::Long8) &&
            !psTileOffsets->invalid_value_offset &&
            psTileOffsets->count <= 4096 && psTileByteCounts &&
            psTileByteCounts->type == LIBERTIFF_NS::TagType::Long &&
            !psTileByteCounts->invalid_value_offset &&
            psTileByteCounts->count <= 4096)
        {
            bool ok = true;
            if (psTileOffsets->type == LIBERTIFF_NS::TagType::Long)
                m_tileOffsets =
                    m_image->readTagAsVector<uint32_t>(*psTileOffsets, ok);
            else
                m_tileOffsets64 =
                    m_image->readTagAsVector<uint64_t>(*psTileOffsets, ok);
            m_tileByteCounts =
                m_image->readTagAsVector<uint32_t>(*psTileByteCounts, ok);
            if (!ok)
            {
                m_tileOffsets.clear();
                m_tileOffsets64.clear();
                m_tileByteCounts.clear();
            }
        }
    }

    // Create raster bands
    for (int i = 0; i < l_nBands; ++i)
    {
        auto poBand = std::make_unique<LIBERTIFFBand>(this, i + 1, eDT,
                                                      nBlockXSize, nBlockYSize);
        poBand->m_bHasNoData = bHasNoData;
        poBand->m_dfNoData = dfNoData;
        if (m_image->photometricInterpretation() ==
            LIBERTIFF_NS::PhotometricInterpretation::MinIsBlack)
        {
            if (i == 0)
                poBand->m_eColorInterp = GCI_GrayIndex;
        }
        else if (m_image->photometricInterpretation() ==
                     LIBERTIFF_NS::PhotometricInterpretation::RGB ||
                 (m_image->photometricInterpretation() ==
                      LIBERTIFF_NS::PhotometricInterpretation::YCbCr &&
                  m_image->samplesPerPixel() == 3))
        {
            if (i < 3)
                poBand->m_eColorInterp =
                    static_cast<GDALColorInterp>(GCI_RedBand + i);
        }
        if (i >= nRegularChannels && !m_extraSamples.empty())
        {
            if (m_extraSamples[i - nRegularChannels] ==
                LIBERTIFF_NS::ExtraSamples::UnAssociatedAlpha)
            {
                poBand->m_eColorInterp = GCI_AlphaBand;
                if (!m_poAlphaBand)
                    m_poAlphaBand = poBand.get();
            }
            else if (m_extraSamples[i - nRegularChannels] ==
                     LIBERTIFF_NS::ExtraSamples::AssociatedAlpha)
            {
                poBand->m_eColorInterp = GCI_AlphaBand;
                poBand->GDALRasterBand::SetMetadataItem(
                    "ALPHA", "PREMULTIPLIED", "IMAGE_STRUCTURE");
                if (!m_poAlphaBand)
                    m_poAlphaBand = poBand.get();
            }
        }

        if (m_image->bitsPerSample() != 8 && m_image->bitsPerSample() != 16 &&
            m_image->bitsPerSample() != 32 && m_image->bitsPerSample() != 64 &&
            m_image->bitsPerSample() != 128)
        {
            poBand->GDALRasterBand::SetMetadataItem(
                "NBITS", CPLSPrintf("%u", m_image->bitsPerSample()),
                "IMAGE_STRUCTURE");
        }

        if (l_nBands == 1 && eDT == GDT_Byte)
        {
            poBand->ReadColorMap();
        }

        if (m_image->photometricInterpretation() ==
            LIBERTIFF_NS::PhotometricInterpretation::MinIsWhite)
        {
            GDALDataset::SetMetadataItem("MINISWHITE", "YES",
                                         "IMAGE_STRUCTURE");
        }

        if (m_image->bitsPerSample() == 1 && !poBand->m_poColorTable)
        {
            poBand->m_poColorTable = std::make_unique<GDALColorTable>();
            const GDALColorEntry oEntryBlack = {0, 0, 0, 255};
            const GDALColorEntry oEntryWhite = {255, 255, 255, 255};
            if (m_image->photometricInterpretation() ==
                LIBERTIFF_NS::PhotometricInterpretation::MinIsWhite)
            {
                poBand->m_poColorTable->SetColorEntry(0, &oEntryWhite);
                poBand->m_poColorTable->SetColorEntry(1, &oEntryBlack);
            }
            else
            {
                poBand->m_poColorTable->SetColorEntry(0, &oEntryBlack);
                poBand->m_poColorTable->SetColorEntry(1, &oEntryWhite);
            }
            poBand->m_eColorInterp = GCI_PaletteIndex;
        }

        SetBand(i + 1, std::move(poBand));
    }

    nOpenFlags = GDAL_OF_RASTER | GDAL_OF_THREAD_SAFE;

    return true;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

bool LIBERTIFFDataset::Open(GDALOpenInfo *poOpenInfo)
{
    SetDescription(poOpenInfo->pszFilename);

    int iSelectedSubDS = -1;
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "GTIFF_DIR:"))
    {
        iSelectedSubDS = atoi(poOpenInfo->pszFilename + strlen("GTIFF_DIR:"));
        if (iSelectedSubDS <= 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid subdataset syntax");
            return false;
        }
        const char *pszNextColon =
            strchr(poOpenInfo->pszFilename + strlen("GTIFF_DIR:"), ':');
        if (!pszNextColon)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid subdataset syntax");
            return false;
        }
        m_poFile.reset(VSIFOpenL(pszNextColon + 1, "rb"));
        if (!m_poFile)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                     pszNextColon + 1);
            return false;
        }
        m_fileReader =
            std::make_shared<const LIBERTIFFDatasetFileReader>(m_poFile.get());
    }
    else
    {
        m_fileReader =
            std::make_shared<const LIBERTIFFDatasetFileReader>(poOpenInfo->fpL);
    }

    auto mainImage = LIBERTIFF_NS::open(m_fileReader);
    if (!mainImage)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open TIFF image");
        return false;
    }

    if (mainImage->subFileType() != LIBERTIFF_NS::SubFileTypeFlags::Page &&
        mainImage->subFileType() != 0)
    {
        CPLDebug("LIBERTIFF", "Invalid subFileType value for first image");
        return false;
    }

    // Check structural metadata (for COG)
    const int nOffsetOfStructuralMetadata =
        poOpenInfo->nHeaderBytes && ((poOpenInfo->pabyHeader[2] == 0x2B ||
                                      poOpenInfo->pabyHeader[3] == 0x2B))
            ? 16
            : 8;
    if (poOpenInfo->nHeaderBytes >
            nOffsetOfStructuralMetadata +
                static_cast<int>(strlen("GDAL_STRUCTURAL_METADATA_SIZE=")) &&
        memcmp(poOpenInfo->pabyHeader + nOffsetOfStructuralMetadata,
               "GDAL_STRUCTURAL_METADATA_SIZE=",
               strlen("GDAL_STRUCTURAL_METADATA_SIZE=")) == 0)
    {
        const char *pszStructuralMD = reinterpret_cast<const char *>(
            poOpenInfo->pabyHeader + nOffsetOfStructuralMetadata);
        const bool bLayoutIFDSBeforeData =
            strstr(pszStructuralMD, "LAYOUT=IFDS_BEFORE_DATA") != nullptr;
        const bool bBlockOrderRowMajor =
            strstr(pszStructuralMD, "BLOCK_ORDER=ROW_MAJOR") != nullptr;
        const bool bLeaderSizeAsUInt4 =
            strstr(pszStructuralMD, "BLOCK_LEADER=SIZE_AS_UINT4") != nullptr;
        const bool bTrailerRepeatedLast4BytesRepeated =
            strstr(pszStructuralMD, "BLOCK_TRAILER=LAST_4_BYTES_REPEATED") !=
            nullptr;
        const bool bKnownIncompatibleEdition =
            strstr(pszStructuralMD, "KNOWN_INCOMPATIBLE_EDITION=YES") !=
            nullptr;
        if (bKnownIncompatibleEdition)
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "This file used to have optimizations in its layout, "
                        "but those have been, at least partly, invalidated by "
                        "later changes");
        }
        else if (bLayoutIFDSBeforeData && bBlockOrderRowMajor &&
                 bLeaderSizeAsUInt4 && bTrailerRepeatedLast4BytesRepeated)
        {
            GDALDataset::SetMetadataItem("LAYOUT", "COG", "IMAGE_STRUCTURE");
        }
    }

    if (!Open(std::move(mainImage)))
        return false;

    // Iterate over overviews
    LIBERTIFFDataset *poLastNonMaskDS = this;
    auto imageNext = m_image->next();
    if (imageNext &&
        (m_image->subFileType() == 0 ||
         m_image->subFileType() == LIBERTIFF_NS::SubFileTypeFlags::Page) &&
        (imageNext->subFileType() == 0 ||
         imageNext->subFileType() == LIBERTIFF_NS::SubFileTypeFlags::Page))
    {
        int iSubDS = 1;
        CPLStringList aosList;
        auto curImage = std::move(m_image);
        do
        {
            if (iSelectedSubDS > 0 && iSubDS == iSelectedSubDS)
            {
                m_image = std::move(curImage);
                break;
            }
            if (iSelectedSubDS < 0)
            {
                aosList.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_NAME", iSubDS),
                    CPLSPrintf("GTIFF_DIR:%d:%s", iSubDS, GetDescription()));
                aosList.AddNameValue(CPLSPrintf("SUBDATASET_%d_DESC", iSubDS),
                                     CPLSPrintf("Page %d (%uP x %uL x %uB)",
                                                iSubDS, curImage->width(),
                                                curImage->height(),
                                                curImage->samplesPerPixel()));
            }
            ++iSubDS;
            if (iSubDS == 65536)
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                            "Stopping IFD scanning at 65536th one");
                break;
            }
            curImage = curImage->next();
        } while (curImage);
        if (iSelectedSubDS < 0)
        {
            for (int i = 0; i < nBands; ++i)
                delete papoBands[i];
            CPLFree(papoBands);
            papoBands = nullptr;
            nRasterXSize = 0;
            nRasterYSize = 0;
            GDALDataset::SetMetadata(aosList.List(), "SUBDATASETS");
            return true;
        }
        else if (!m_image)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %dth image",
                     iSelectedSubDS);
            return false;
        }
    }
    else if (iSelectedSubDS < 0)
    {
        auto curImage = std::move(imageNext);
        int iters = 0;
        while (curImage)
        {
            auto nextImage = curImage->next();
            if (curImage->subFileType() ==
                LIBERTIFF_NS::SubFileTypeFlags::ReducedImage)
            {
                // Overview IFD
                auto poOvrDS = std::make_unique<LIBERTIFFDataset>();
                if (poOvrDS->Open(std::move(curImage)) &&
                    poOvrDS->GetRasterCount() == nBands &&
                    poOvrDS->GetRasterXSize() <= nRasterXSize &&
                    poOvrDS->GetRasterYSize() <= nRasterYSize &&
                    poOvrDS->GetRasterBand(1)->GetRasterDataType() ==
                        GetRasterBand(1)->GetRasterDataType())
                {
                    m_apoOvrDSOwned.push_back(std::move(poOvrDS));
                    auto poOvrDSRaw = m_apoOvrDSOwned.back().get();
                    m_apoOvrDS.push_back(poOvrDSRaw);
                    poLastNonMaskDS = poOvrDSRaw;
                }
            }
            else if ((curImage->subFileType() &
                      LIBERTIFF_NS::SubFileTypeFlags::Mask) != 0)
            {
                // Mask IFD
                if (!poLastNonMaskDS->m_poMaskDS)
                {
                    auto poMaskDS = std::make_unique<LIBERTIFFDataset>();
                    if (poMaskDS->Open(std::move(curImage)) &&
                        poMaskDS->GetRasterCount() == 1 &&
                        poMaskDS->GetRasterXSize() ==
                            poLastNonMaskDS->nRasterXSize &&
                        poMaskDS->GetRasterYSize() ==
                            poLastNonMaskDS->nRasterYSize &&
                        poMaskDS->GetRasterBand(1)->GetRasterDataType() ==
                            GDT_Byte)
                    {
                        poMaskDS->m_bExpand1To255 = true;
                        poLastNonMaskDS->m_poMaskDS = std::move(poMaskDS);
                        if (poLastNonMaskDS != this && m_poMaskDS)
                        {
                            // Also register the mask as the overview of the main
                            // mask
                            m_poMaskDS->m_apoOvrDS.push_back(
                                poLastNonMaskDS->m_poMaskDS.get());
                        }
                    }
                }
            }
            else
            {
                CPLDebug("LIBERTIFF",
                         "Unhandled subFileType value for auxiliary image");
                return false;
            }
            curImage = std::move(nextImage);

            ++iters;
            if (iters == 64)
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                            "Stopping IFD scanning at 64th one");
                break;
            }
        }
    }

    static const struct
    {
        LIBERTIFF_NS::TagCodeType code;
        const char *mditem;
    } strTags[] = {
        {LIBERTIFF_NS::TagCode::DocumentName, "TIFFTAG_DOCUMENTNAME"},
        {LIBERTIFF_NS::TagCode::ImageDescription, "TIFFTAG_IMAGEDESCRIPTION"},
        {LIBERTIFF_NS::TagCode::Software, "TIFFTAG_SOFTWARE"},
        {LIBERTIFF_NS::TagCode::DateTime, "TIFFTAG_DATETIME"},
        {LIBERTIFF_NS::TagCode::Copyright, "TIFFTAG_COPYRIGHT"},
    };

    for (const auto &strTag : strTags)
    {
        const auto *tag = m_image->tag(strTag.code);
        constexpr size_t ARBITRARY_MAX_SIZE = 65536;
        if (tag && tag->type == LIBERTIFF_NS::TagType::ASCII &&
            !(tag->count > 4 && tag->invalid_value_offset) &&
            tag->count < ARBITRARY_MAX_SIZE)
        {
            bool ok = true;
            const std::string str = m_image->readTagAsString(*tag, ok);
            if (ok)
            {
                GDALDataset::SetMetadataItem(strTag.mditem, str.c_str());
            }
        }
    }

    ReadSRS();
    ReadGeoTransform();
    ReadRPCTag();

    const auto *psGDALMetadataTag =
        m_image->tag(LIBERTIFF_NS::TagCode::GDAL_METADATA);
    constexpr size_t ARBITRARY_MAX_SIZE_GDAL_METADATA = 10 * 1024 * 1024;
    if (psGDALMetadataTag &&
        psGDALMetadataTag->type == LIBERTIFF_NS::TagType::ASCII &&
        !(psGDALMetadataTag->count > 4 &&
          psGDALMetadataTag->invalid_value_offset) &&
        psGDALMetadataTag->count < ARBITRARY_MAX_SIZE_GDAL_METADATA)
    {
        bool ok = true;
        const std::string str =
            m_image->readTagAsString(*psGDALMetadataTag, ok);
        if (ok)
        {
            auto oRoot = CPLXMLTreeCloser(CPLParseXMLString(str.c_str()));
            if (oRoot.get())
            {
                const CPLXMLNode *psItem =
                    oRoot.get() ? CPLGetXMLNode(oRoot.get(), "=GDALMetadata")
                                : nullptr;
                if (psItem)
                    psItem = psItem->psChild;
                for (; psItem != nullptr; psItem = psItem->psNext)
                {
                    if (psItem->eType != CXT_Element ||
                        !EQUAL(psItem->pszValue, "Item"))
                        continue;

                    const char *pszKey =
                        CPLGetXMLValue(psItem, "name", nullptr);
                    const char *pszValue =
                        CPLGetXMLValue(psItem, nullptr, nullptr);
                    int nBand = atoi(CPLGetXMLValue(psItem, "sample", "-1"));
                    if (nBand < -1 || nBand > 65535)
                        continue;
                    nBand++;
                    const char *pszRole = CPLGetXMLValue(psItem, "role", "");
                    const char *pszDomain =
                        CPLGetXMLValue(psItem, "domain", "");

                    if (pszKey == nullptr || pszValue == nullptr)
                        continue;
                    if (EQUAL(pszDomain, "IMAGE_STRUCTURE"))
                    {
                        if (m_image->compression() ==
                                LIBERTIFF_NS::Compression::WEBP &&
                            EQUAL(pszKey, "COMPRESSION_REVERSIBILITY"))
                        {
                            // go on
                        }
                        else if (m_image->compression() ==
                                     LIBERTIFF_NS::Compression::WEBP &&
                                 EQUAL(pszKey, "WEBP_LEVEL"))
                        {
                            const int nLevel = atoi(pszValue);
                            if (nLevel >= 1 && nLevel <= 100)
                            {
                                GDALDataset::SetMetadataItem(
                                    "COMPRESSION_REVERSIBILITY", "LOSSY",
                                    "IMAGE_STRUCTURE");
                            }
                        }
                        else if (m_image->compression() ==
                                     LIBERTIFF_NS::Compression::LERC &&
                                 EQUAL(pszKey, "MAX_Z_ERROR"))
                        {
                            // go on
                        }
                        else if (m_image->compression() ==
                                     LIBERTIFF_NS::Compression::LERC &&
                                 EQUAL(pszKey, "MAX_Z_ERROR_OVERVIEW"))
                        {
                            // go on
                        }
                        else if (m_image->compression() ==
                                     LIBERTIFF_NS::Compression::JXL &&
                                 EQUAL(pszKey, "COMPRESSION_REVERSIBILITY"))
                        {
                            // go on
                        }
                        else if (m_image->compression() ==
                                     LIBERTIFF_NS::Compression::JXL &&
                                 EQUAL(pszKey, "JXL_DISTANCE"))
                        {
                            const double dfVal = CPLAtof(pszValue);
                            if (dfVal > 0 && dfVal <= 15)
                            {
                                GDALDataset::SetMetadataItem(
                                    "COMPRESSION_REVERSIBILITY", "LOSSY",
                                    "IMAGE_STRUCTURE");
                            }
                        }
                        else if (m_image->compression() ==
                                     LIBERTIFF_NS::Compression::JXL &&
                                 EQUAL(pszKey, "JXL_ALPHA_DISTANCE"))
                        {
                            const double dfVal = CPLAtof(pszValue);
                            if (dfVal > 0 && dfVal <= 15)
                            {
                                GDALDataset::SetMetadataItem(
                                    "COMPRESSION_REVERSIBILITY", "LOSSY",
                                    "IMAGE_STRUCTURE");
                            }
                        }
                        else if (m_image->compression() ==
                                     LIBERTIFF_NS::Compression::JXL &&
                                 EQUAL(pszKey, "JXL_EFFORT"))
                        {
                            // go on
                        }
                        else
                        {
                            continue;
                        }
                    }

                    bool bIsXML = false;

                    if (STARTS_WITH_CI(pszDomain, "xml:"))
                        bIsXML = TRUE;

                    // Note: this un-escaping should not normally be done, as the
                    // deserialization of the tree from XML also does it, so we end up
                    // width double XML escaping, but keep it for backward
                    // compatibility.
                    char *pszUnescapedValue =
                        CPLUnescapeString(pszValue, nullptr, CPLES_XML);
                    if (nBand == 0)
                    {
                        if (bIsXML)
                        {
                            char *apszMD[2] = {pszUnescapedValue, nullptr};
                            GDALDataset::SetMetadata(apszMD, pszDomain);
                        }
                        else
                        {
                            GDALDataset::SetMetadataItem(
                                pszKey, pszUnescapedValue, pszDomain);
                        }
                    }
                    else
                    {
                        auto poBand = cpl::down_cast<LIBERTIFFBand *>(
                            GetRasterBand(nBand));
                        if (poBand != nullptr)
                        {
                            if (EQUAL(pszRole, "scale"))
                            {
                                poBand->m_bHaveOffsetScale = true;
                                poBand->m_dfScale = CPLAtofM(pszUnescapedValue);
                            }
                            else if (EQUAL(pszRole, "offset"))
                            {
                                poBand->m_bHaveOffsetScale = true;
                                poBand->m_dfOffset =
                                    CPLAtofM(pszUnescapedValue);
                            }
                            else if (EQUAL(pszRole, "unittype"))
                            {
                                poBand->m_osUnitType = pszUnescapedValue;
                            }
                            else if (EQUAL(pszRole, "description"))
                            {
                                poBand->m_osDescription = pszUnescapedValue;
                            }
                            else if (EQUAL(pszRole, "colorinterp"))
                            {
                                if (EQUAL(pszUnescapedValue, "undefined"))
                                    poBand->m_eColorInterp = GCI_Undefined;
                                else
                                {
                                    poBand->m_eColorInterp =
                                        GDALGetColorInterpretationByName(
                                            pszUnescapedValue);
                                    if (poBand->m_eColorInterp == GCI_Undefined)
                                    {
                                        poBand->GDALRasterBand::SetMetadataItem(
                                            "COLOR_INTERPRETATION",
                                            pszUnescapedValue);
                                    }
                                }
                            }
                            else
                            {
                                if (bIsXML)
                                {
                                    char *apszMD[2] = {pszUnescapedValue,
                                                       nullptr};
                                    poBand->GDALRasterBand::SetMetadata(
                                        apszMD, pszDomain);
                                }
                                else
                                {
                                    poBand->GDALRasterBand::SetMetadataItem(
                                        pszKey, pszUnescapedValue, pszDomain);
                                }
                            }
                        }
                    }
                    CPLFree(pszUnescapedValue);
                }
            }
        }
    }

    if ((m_image->compression() == LIBERTIFF_NS::Compression::WEBP ||
         m_image->compression() == LIBERTIFF_NS::Compression::JXL ||
         m_image->compression() == LIBERTIFF_NS::Compression::JXL_DNG_1_7) &&
        GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE") ==
            nullptr)
    {
        const char *pszDriverName =
            m_image->compression() == LIBERTIFF_NS::Compression::WEBP
                ? "WEBP"
                : "JPEGXL";
        auto poTileDriver = GDALGetDriverByName(pszDriverName);
        if (poTileDriver)
        {
            bool ok = true;
            const uint64_t offset = m_image->strileOffset(0, ok);
            const uint64_t bytecount = m_image->strileByteCount(0, ok);
            if (ok && bytecount > 0)
            {
                const std::string osSubfile(
                    CPLSPrintf("/vsisubfile/" CPL_FRMT_GUIB "_%d,%s",
                               static_cast<GUIntBig>(offset),
                               static_cast<int>(std::min(
                                   static_cast<uint64_t>(1024), bytecount)),
                               GetDescription()));
                const char *const apszDrivers[] = {pszDriverName, nullptr};
                auto poTileDataset =
                    std::unique_ptr<GDALDataset>(GDALDataset::Open(
                        osSubfile.c_str(), GDAL_OF_RASTER, apszDrivers));
                if (poTileDataset)
                {
                    const char *pszReversibility =
                        poTileDataset->GetMetadataItem(
                            "COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE");
                    if (pszReversibility)
                        GDALDataset::SetMetadataItem(
                            "COMPRESSION_REVERSIBILITY", pszReversibility,
                            "IMAGE_STRUCTURE");
                }
            }
        }
    }

    // Init mask bands
    for (int i = 0; i < nBands; ++i)
    {
        cpl::down_cast<LIBERTIFFBand *>(papoBands[i])->InitMaskBand();
    }
    for (auto &poOvrDS : m_apoOvrDS)
    {
        for (int i = 0; i < nBands; ++i)
        {
            cpl::down_cast<LIBERTIFFBand *>(poOvrDS->papoBands[i])
                ->InitMaskBand();
        }
    }

    m_fileReader->setPReadAllowed();

    if (poOpenInfo->fpL)
    {
        m_poFile.reset(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
    }

    const char *pszValue =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "NUM_THREADS");
    if (pszValue == nullptr)
        pszValue = CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
    if (pszValue)
    {
        int nThreads =
            EQUAL(pszValue, "ALL_CPUS") ? CPLGetNumCPUs() : atoi(pszValue);
        if (nThreads > 1024)
            nThreads = 1024;  // to please Coverity
        if (nThreads > 1)
        {
            m_poThreadPool = GDALGetGlobalThreadPool(nThreads);
        }
    }

    return true;
}

/************************************************************************/
/*                             ReadSRS()                                */
/************************************************************************/

// Simplified GeoTIFF SRS reader, assuming the SRS is encoded as a EPSG code
void LIBERTIFFDataset::ReadSRS()
{
    const auto psGeoKeysTag =
        m_image->tag(LIBERTIFF_NS::TagCode::GeoTIFFGeoKeyDirectory);
    constexpr int VALUES_PER_GEOKEY = 4;
    if (psGeoKeysTag && psGeoKeysTag->type == LIBERTIFF_NS::TagType::Short &&
        !psGeoKeysTag->invalid_value_offset &&
        psGeoKeysTag->count >= VALUES_PER_GEOKEY &&
        (psGeoKeysTag->count % VALUES_PER_GEOKEY) == 0 &&
        // Sanity check
        psGeoKeysTag->count < 1000)
    {
        bool ok = true;
        const auto values =
            m_image->readTagAsVector<uint16_t>(*psGeoKeysTag, ok);
        if (values.size() >= 4)
        {
            const uint16_t geokeysCount = values[3];
            constexpr uint16_t GEOTIFF_KEY_DIRECTORY_VERSION_V1 = 1;
            constexpr uint16_t GEOTIFF_KEY_VERSION_MAJOR_V1 = 1;
            if (values[0] == GEOTIFF_KEY_DIRECTORY_VERSION_V1 &&
                // GeoTIFF 1.x
                values[1] == GEOTIFF_KEY_VERSION_MAJOR_V1 &&
                // No equality for autotest/gcore/data/ycbcr_with_mask.tif
                geokeysCount <= psGeoKeysTag->count / VALUES_PER_GEOKEY - 1)
            {
                constexpr uint16_t GeoTIFFTypeShort = 0;
                constexpr uint16_t GeoTIFFTypeDouble =
                    LIBERTIFF_NS::TagCode::GeoTIFFDoubleParams;

                constexpr uint16_t GTModelTypeGeoKey = 1024;
                constexpr uint16_t ModelTypeProjected = 1;
                constexpr uint16_t ModelTypeGeographic = 2;

                constexpr uint16_t GTRasterTypeGeoKey = 1025;
                constexpr uint16_t RasterPixelIsArea = 1;
                constexpr uint16_t RasterPixelIsPoint = 2;

                constexpr uint16_t GeodeticCRSGeoKey = 2048;
                constexpr uint16_t ProjectedCRSGeoKey = 3072;

                constexpr uint16_t VerticalGeoKey = 4096;

                constexpr uint16_t CoordinateEpochGeoKey = 5120;

                uint16_t nModelType = 0;
                uint16_t nEPSGCode = 0;
                uint16_t nEPSGCodeVertical = 0;
                double dfCoordEpoch = 0;
                bool bHasCoordEpoch = false;
                for (uint32_t i = 1; i <= geokeysCount; ++i)
                {
                    const auto geokey = values[VALUES_PER_GEOKEY * i];
                    const auto geokeyType = values[VALUES_PER_GEOKEY * i + 1];
                    const auto geokeyCount = values[VALUES_PER_GEOKEY * i + 2];
                    const auto geokeyValue = values[VALUES_PER_GEOKEY * i + 3];
                    if (geokey == GTModelTypeGeoKey)
                    {
                        nModelType = geokeyValue;
                    }
                    else if (geokey == GeodeticCRSGeoKey &&
                             nModelType == ModelTypeGeographic &&
                             geokeyType == GeoTIFFTypeShort &&
                             geokeyCount == 1 && geokeyValue > 0)
                    {
                        nEPSGCode = geokeyValue;
                    }
                    else if (geokey == ProjectedCRSGeoKey &&
                             nModelType == ModelTypeProjected &&
                             geokeyType == GeoTIFFTypeShort &&
                             geokeyCount == 1 && geokeyValue > 0)
                    {
                        nEPSGCode = geokeyValue;
                    }
                    else if (geokey == GTRasterTypeGeoKey &&
                             geokeyType == GeoTIFFTypeShort && geokeyCount == 1)
                    {
                        if (geokeyValue == RasterPixelIsArea)
                        {
                            GDALDataset::SetMetadataItem(GDALMD_AREA_OR_POINT,
                                                         GDALMD_AOP_AREA);
                        }
                        else if (geokeyValue == RasterPixelIsPoint)
                        {
                            GDALDataset::SetMetadataItem(GDALMD_AREA_OR_POINT,
                                                         GDALMD_AOP_POINT);
                        }
                    }
                    else if (values[2] == 1 /* GeoTIFF 1.1 */ &&
                             geokey == VerticalGeoKey &&
                             geokeyType == GeoTIFFTypeShort && geokeyCount == 1)
                    {
                        nEPSGCodeVertical = geokeyValue;
                    }
                    else if (geokey == CoordinateEpochGeoKey &&
                             geokeyType == GeoTIFFTypeDouble &&
                             geokeyCount == 1)
                    {
                        const auto psGeoDoubleParamsTag = m_image->tag(
                            LIBERTIFF_NS::TagCode::GeoTIFFDoubleParams);
                        if (psGeoDoubleParamsTag &&
                            psGeoDoubleParamsTag->type ==
                                LIBERTIFF_NS::TagType::Double &&
                            psGeoDoubleParamsTag->count > geokeyValue)
                        {
                            ok = true;
                            const auto doubleValues =
                                m_image->readTagAsVector<double>(
                                    *psGeoDoubleParamsTag, ok);
                            if (ok && doubleValues.size() > geokeyValue)
                            {
                                bHasCoordEpoch = true;
                                dfCoordEpoch = doubleValues[geokeyValue];
                            }
                        }
                    }
                }

                if (nEPSGCode > 0 && nEPSGCode != 32767 &&
                    nEPSGCodeVertical != 32767)
                {
                    m_oSRS.importFromEPSG(nEPSGCode);

                    if (nEPSGCodeVertical > 0)
                    {
                        OGRSpatialReference oSRSVertical;
                        oSRSVertical.importFromEPSG(nEPSGCodeVertical);
                        if (oSRSVertical.IsGeographic() &&
                            oSRSVertical.GetAxesCount() == 3)
                        {
                            m_oSRS = std::move(oSRSVertical);
                        }
                        else
                        {
                            m_oSRS.SetFromUserInput(CPLSPrintf(
                                "EPSG:%d+%d", nEPSGCode, nEPSGCodeVertical));
                        }
                    }

                    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    if (bHasCoordEpoch)
                        m_oSRS.SetCoordinateEpoch(dfCoordEpoch);
                    return;
                }

                const char *const apszAllowedDrivers[] = {"GTiff", nullptr};
                auto poTmpDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                    GetDescription(), GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    apszAllowedDrivers, nullptr, nullptr));
                if (poTmpDS)
                {
                    const OGRSpatialReference *poSRS = poTmpDS->GetSpatialRef();
                    if (!poSRS)
                        poSRS = poTmpDS->GetGCPSpatialRef();
                    if (poSRS)
                        m_oSRS = *poSRS;
                }
            }
        }
    }
}

/************************************************************************/
/*                       ReadGeoTransform()                             */
/************************************************************************/

void LIBERTIFFDataset::ReadGeoTransform()
{
    // Number of values per GCP in the GeoTIFFTiePoints tag
    constexpr int VALUES_PER_GCP = 6;

    constexpr int GCP_PIXEL = 0;
    constexpr int GCP_LINE = 1;
    // constexpr int GCP_DEPTH = 2;
    constexpr int GCP_X = 3;
    constexpr int GCP_Y = 4;
    constexpr int GCP_Z = 5;

    const auto *psTagTiePoints =
        m_image->tag(LIBERTIFF_NS::TagCode::GeoTIFFTiePoints);
    const auto *psTagPixelScale =
        m_image->tag(LIBERTIFF_NS::TagCode::GeoTIFFPixelScale);
    const auto *psTagGeoTransMatrix =
        m_image->tag(LIBERTIFF_NS::TagCode::GeoTIFFGeoTransMatrix);
    if (psTagTiePoints &&
        psTagTiePoints->type == LIBERTIFF_NS::TagType::Double &&
        !psTagTiePoints->invalid_value_offset &&
        psTagTiePoints->count == VALUES_PER_GCP && psTagPixelScale &&
        psTagPixelScale->type == LIBERTIFF_NS::TagType::Double &&
        !psTagPixelScale->invalid_value_offset && psTagPixelScale->count == 3)
    {
        bool ok = true;
        const auto tiepoints =
            m_image->readTagAsVector<double>(*psTagTiePoints, ok);
        const auto pixelScale =
            m_image->readTagAsVector<double>(*psTagPixelScale, ok);
        if (!ok)
            return;

        m_geotransformValid = true;
        m_geotransform[1] = pixelScale[GCP_PIXEL];
        m_geotransform[5] = -pixelScale[GCP_LINE];
        m_geotransform[0] =
            tiepoints[GCP_X] - tiepoints[GCP_PIXEL] * m_geotransform[1];
        m_geotransform[3] =
            tiepoints[GCP_Y] - tiepoints[GCP_LINE] * m_geotransform[5];
    }
    else if (psTagGeoTransMatrix &&
             psTagGeoTransMatrix->type == LIBERTIFF_NS::TagType::Double &&
             !psTagGeoTransMatrix->invalid_value_offset &&
             psTagGeoTransMatrix->count == 16)
    {
        bool ok = true;
        const auto matrix =
            m_image->readTagAsVector<double>(*psTagGeoTransMatrix, ok);
        if (ok)
        {
            m_geotransformValid = true;
            m_geotransform[0] = matrix[3];
            m_geotransform[1] = matrix[0];
            m_geotransform[2] = matrix[1];
            m_geotransform[3] = matrix[7];
            m_geotransform[4] = matrix[4];
            m_geotransform[5] = matrix[5];
        }
    }
    else if (psTagTiePoints &&
             psTagTiePoints->type == LIBERTIFF_NS::TagType::Double &&
             !psTagTiePoints->invalid_value_offset &&
             psTagTiePoints->count > VALUES_PER_GCP &&
             (psTagTiePoints->count % VALUES_PER_GCP) == 0 &&
             psTagTiePoints->count <= 10000 * VALUES_PER_GCP)
    {
        bool ok = true;
        const auto tiepoints =
            m_image->readTagAsVector<double>(*psTagTiePoints, ok);
        if (ok)
        {
            bool pixelIsPoint = false;
            if (const char *pszAreaOrPoint =
                    GetMetadataItem(GDALMD_AREA_OR_POINT))
            {
                pixelIsPoint = EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT);
            }
            const int gcpCount =
                static_cast<int>(psTagTiePoints->count / VALUES_PER_GCP);
            for (int iGCP = 0; iGCP < gcpCount; ++iGCP)
            {
                m_aoGCPs.emplace_back(
                    CPLSPrintf("%d", iGCP + 1), "",
                    /* pixel = */ tiepoints[iGCP * VALUES_PER_GCP + GCP_PIXEL],
                    /* line = */ tiepoints[iGCP * VALUES_PER_GCP + GCP_LINE],
                    /* X = */ tiepoints[iGCP * VALUES_PER_GCP + GCP_X],
                    /* Y = */ tiepoints[iGCP * VALUES_PER_GCP + GCP_Y],
                    /* Z = */ tiepoints[iGCP * VALUES_PER_GCP + GCP_Z]);

                if (pixelIsPoint)
                {
                    m_aoGCPs.back().Pixel() += 0.5;
                    m_aoGCPs.back().Line() += 0.5;
                }
            }
        }
    }

    if (m_geotransformValid)
    {
        if (const char *pszAreaOrPoint = GetMetadataItem(GDALMD_AREA_OR_POINT))
        {
            if (EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT))
            {
                m_geotransform[0] -=
                    (m_geotransform[1] * 0.5 + m_geotransform[2] * 0.5);
                m_geotransform[3] -=
                    (m_geotransform[4] * 0.5 + m_geotransform[5] * 0.5);
            }
        }
    }
}

/************************************************************************/
/*                             ReadRPCTag()                             */
/*                                                                      */
/*      Format a TAG according to:                                      */
/*                                                                      */
/*      http://geotiff.maptools.org/rpc_prop.html                       */
/************************************************************************/

void LIBERTIFFDataset::ReadRPCTag()

{
    const auto *psTagRPCCoefficients =
        m_image->tag(LIBERTIFF_NS::TagCode::RPCCoefficients);
    if (psTagRPCCoefficients &&
        psTagRPCCoefficients->type == LIBERTIFF_NS::TagType::Double &&
        !psTagRPCCoefficients->invalid_value_offset &&
        psTagRPCCoefficients->count == 92)
    {
        bool ok = true;
        const auto adfRPC =
            m_image->readTagAsVector<double>(*psTagRPCCoefficients, ok);
        if (ok && adfRPC.size() == 92)
        {
            GDALDataset::SetMetadata(
                gdal::tiff_common::TIFFRPCTagToRPCMetadata(adfRPC.data())
                    .List(),
                "RPC");
        }
    }
}

/************************************************************************/
/*                           OpenStatic()                               */
/************************************************************************/

/* static */ GDALDataset *LIBERTIFFDataset::OpenStatic(GDALOpenInfo *poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    auto poDS = std::make_unique<LIBERTIFFDataset>();
    if (!poDS->Open(poOpenInfo))
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                       GDALRegister_LIBERTIFF()                       */
/************************************************************************/

void GDALRegister_LIBERTIFF()

{
    if (GDALGetDriverByName("LIBERTIFF") != nullptr)
        return;

    auto poDriver = std::make_unique<GDALDriver>();
    poDriver->SetDescription("LIBERTIFF");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "GeoTIFF (using LIBERTIFF library)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/libertiff.html");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/tiff");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "tif tiff");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_COORDINATE_EPOCH, "YES");

    poDriver->pfnIdentify = LIBERTIFFDataset::Identify;
    poDriver->pfnOpen = LIBERTIFFDataset::OpenStatic;

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='NUM_THREADS' type='string' description='Number of "
        "worker threads for compression. Can be set to ALL_CPUS' default='1'/>"
        "</OpenOptionList>");

    if (CPLGetDecompressor("lzma"))
    {
        poDriver->SetMetadataItem("LZMA_SUPPORT", "YES", "LIBERTIFF");
    }
#ifdef ZSTD_SUPPORT
    poDriver->SetMetadataItem("ZSTD_SUPPORT", "YES", "LIBERTIFF");
#endif
#if defined(LERC_SUPPORT)
    poDriver->SetMetadataItem("LERC_SUPPORT", "YES", "LIBERTIFF");
#if defined(LERC_VERSION_MAJOR)
    poDriver->SetMetadataItem("LERC_VERSION_MAJOR",
                              XSTRINGIFY(LERC_VERSION_MAJOR), "LERC");
    poDriver->SetMetadataItem("LERC_VERSION_MINOR",
                              XSTRINGIFY(LERC_VERSION_MINOR), "LERC");
    poDriver->SetMetadataItem("LERC_VERSION_PATCH",
                              XSTRINGIFY(LERC_VERSION_PATCH), "LERC");
#endif
#endif

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
