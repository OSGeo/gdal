/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Read/get operations on GTiffDataset
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "gtiffdataset.h"
#include "gtiffrasterband.h"
#include "gtiffjpegoverviewds.h"
#include "gtiffrgbaband.h"
#include "gtiffbitmapband.h"
#include "gtiffsplitband.h"
#include "gtiffsplitbitmapband.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <queue>
#include <tuple>
#include <utility>

#include "cpl_error.h"
#include "cpl_error_internal.h"  // CPLErrorHandlerAccumulatorStruct
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "cpl_worker_thread_pool.h"
#include "fetchbufferdirectio.h"
#include "gdal_mdreader.h"    // MD_DOMAIN_RPC
#include "geovalues.h"        // RasterPixelIsPoint
#include "gt_wkt_srs_priv.h"  // GDALGTIFKeyGetSHORT()
#include "tif_jxl.h"
#include "tifvsi.h"
#include "xtiffio.h"

/************************************************************************/
/*                        GetJPEGOverviewCount()                        */
/************************************************************************/

int GTiffDataset::GetJPEGOverviewCount()
{
    if (m_nJPEGOverviewCount >= 0)
        return m_nJPEGOverviewCount;

    m_nJPEGOverviewCount = 0;
    if (m_poBaseDS || eAccess != GA_ReadOnly ||
        m_nCompression != COMPRESSION_JPEG ||
        (nRasterXSize < 256 && nRasterYSize < 256) ||
        !CPLTestBool(CPLGetConfigOption("GTIFF_IMPLICIT_JPEG_OVR", "YES")) ||
        GDALGetDriverByName("JPEG") == nullptr)
    {
        return 0;
    }
    const char *pszSourceColorSpace =
        m_oGTiffMDMD.GetMetadataItem("SOURCE_COLOR_SPACE", "IMAGE_STRUCTURE");
    if (pszSourceColorSpace != nullptr && EQUAL(pszSourceColorSpace, "CMYK"))
    {
        // We cannot handle implicit overviews on JPEG CMYK datasets converted
        // to RGBA This would imply doing the conversion in
        // GTiffJPEGOverviewBand.
        return 0;
    }

    // libjpeg-6b only supports 2, 4 and 8 scale denominators.
    // TODO: Later versions support more.
    for (signed char i = 2; i >= 0; i--)
    {
        if (nRasterXSize >= (256 << i) || nRasterYSize >= (256 << i))
        {
            m_nJPEGOverviewCount = i + 1;
            break;
        }
    }
    if (m_nJPEGOverviewCount == 0)
        return 0;

    // Get JPEG tables.
    uint32_t nJPEGTableSize = 0;
    void *pJPEGTable = nullptr;
    GByte abyFFD8[] = {0xFF, 0xD8};
    if (TIFFGetField(m_hTIFF, TIFFTAG_JPEGTABLES, &nJPEGTableSize, &pJPEGTable))
    {
        if (pJPEGTable == nullptr || nJPEGTableSize > INT_MAX ||
            static_cast<GByte *>(pJPEGTable)[nJPEGTableSize - 1] != 0xD9)
        {
            m_nJPEGOverviewCount = 0;
            return 0;
        }
        nJPEGTableSize--;  // Remove final 0xD9.
    }
    else
    {
        pJPEGTable = abyFFD8;
        nJPEGTableSize = 2;
    }

    m_papoJPEGOverviewDS = static_cast<GTiffJPEGOverviewDS **>(
        CPLMalloc(sizeof(GTiffJPEGOverviewDS *) * m_nJPEGOverviewCount));
    for (int i = 0; i < m_nJPEGOverviewCount; ++i)
    {
        m_papoJPEGOverviewDS[i] = new GTiffJPEGOverviewDS(
            this, i + 1, pJPEGTable, static_cast<int>(nJPEGTableSize));
    }

    m_nJPEGOverviewCountOri = m_nJPEGOverviewCount;

    return m_nJPEGOverviewCount;
}

/************************************************************************/
/*                       GetCompressionFormats()                        */
/************************************************************************/

CPLStringList GTiffDataset::GetCompressionFormats(int nXOff, int nYOff,
                                                  int nXSize, int nYSize,
                                                  int nBandCount,
                                                  const int *panBandList)
{
    if (m_nCompression != COMPRESSION_NONE &&
        IsWholeBlock(nXOff, nYOff, nXSize, nYSize) &&
        ((nBandCount == 1 && (panBandList || nBands == 1) &&
          m_nPlanarConfig == PLANARCONFIG_SEPARATE) ||
         (IsAllBands(nBandCount, panBandList) &&
          m_nPlanarConfig == PLANARCONFIG_CONTIG)))
    {
        CPLStringList aosList;
        int nBlockId =
            (nXOff / m_nBlockXSize) + (nYOff / m_nBlockYSize) * m_nBlocksPerRow;
        if (m_nPlanarConfig == PLANARCONFIG_SEPARATE && panBandList != nullptr)
            nBlockId += panBandList[0] * m_nBlocksPerBand;

        vsi_l_offset nOffset = 0;
        vsi_l_offset nSize = 0;
        if (IsBlockAvailable(nBlockId, &nOffset, &nSize) &&
            nSize <
                static_cast<vsi_l_offset>(std::numeric_limits<tmsize_t>::max()))
        {
            switch (m_nCompression)
            {
                case COMPRESSION_JPEG:
                {
                    if (m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands == 4 &&
                        m_nPhotometric == PHOTOMETRIC_RGB &&
                        GetRasterBand(4)->GetColorInterpretation() ==
                            GCI_AlphaBand)
                    {
                        // as a hint for the JPEG and JPEGXL drivers to not use it!
                        aosList.AddString("JPEG;colorspace=RGBA");
                    }
                    else
                    {
                        aosList.AddString("JPEG");
                    }
                    break;
                }

                case COMPRESSION_WEBP:
                    aosList.AddString("WEBP");
                    break;

                case COMPRESSION_JXL:
                    aosList.AddString("JXL");
                    break;

                default:
                    break;
            }
        }
        return aosList;
    }
    return CPLStringList();
}

/************************************************************************/
/*                       ReadCompressedData()                           */
/************************************************************************/

CPLErr GTiffDataset::ReadCompressedData(const char *pszFormat, int nXOff,
                                        int nYOff, int nXSize, int nYSize,
                                        int nBandCount, const int *panBandList,
                                        void **ppBuffer, size_t *pnBufferSize,
                                        char **ppszDetailedFormat)
{
    if (m_nCompression != COMPRESSION_NONE &&
        IsWholeBlock(nXOff, nYOff, nXSize, nYSize) &&
        ((nBandCount == 1 && (panBandList != nullptr || nBands == 1) &&
          m_nPlanarConfig == PLANARCONFIG_SEPARATE) ||
         (IsAllBands(nBandCount, panBandList) &&
          m_nPlanarConfig == PLANARCONFIG_CONTIG)))
    {
        const CPLStringList aosTokens(CSLTokenizeString2(pszFormat, ";", 0));
        if (aosTokens.size() != 1)
            return CE_Failure;

        // We don't want to handle CMYK JPEG for now
        if ((m_nCompression == COMPRESSION_JPEG &&
             EQUAL(aosTokens[0], "JPEG") &&
             (m_nPlanarConfig == PLANARCONFIG_SEPARATE ||
              m_nPhotometric != PHOTOMETRIC_SEPARATED)) ||
            (m_nCompression == COMPRESSION_WEBP &&
             EQUAL(aosTokens[0], "WEBP")) ||
            (m_nCompression == COMPRESSION_JXL && EQUAL(aosTokens[0], "JXL")))
        {
            std::string osDetailedFormat = aosTokens[0];

            int nBlockId = (nXOff / m_nBlockXSize) +
                           (nYOff / m_nBlockYSize) * m_nBlocksPerRow;
            if (m_nPlanarConfig == PLANARCONFIG_SEPARATE &&
                panBandList != nullptr)
                nBlockId += panBandList[0] * m_nBlocksPerBand;

            vsi_l_offset nOffset = 0;
            vsi_l_offset nSize = 0;
            if (IsBlockAvailable(nBlockId, &nOffset, &nSize) &&
                nSize < static_cast<vsi_l_offset>(
                            std::numeric_limits<tmsize_t>::max()))
            {
                uint32_t nJPEGTableSize = 0;
                void *pJPEGTable = nullptr;
                if (m_nCompression == COMPRESSION_JPEG)
                {
                    if (TIFFGetField(m_hTIFF, TIFFTAG_JPEGTABLES,
                                     &nJPEGTableSize, &pJPEGTable) &&
                        pJPEGTable != nullptr && nJPEGTableSize > 4 &&
                        static_cast<GByte *>(pJPEGTable)[0] == 0xFF &&
                        static_cast<GByte *>(pJPEGTable)[1] == 0xD8 &&
                        static_cast<GByte *>(pJPEGTable)[nJPEGTableSize - 2] ==
                            0xFF &&
                        static_cast<GByte *>(pJPEGTable)[nJPEGTableSize - 1] ==
                            0xD9)
                    {
                        pJPEGTable = static_cast<GByte *>(pJPEGTable) + 2;
                        nJPEGTableSize -= 4;
                    }
                    else
                    {
                        nJPEGTableSize = 0;
                    }
                }

                size_t nSizeSize = static_cast<size_t>(nSize + nJPEGTableSize);
                if (ppBuffer)
                {
                    if (!pnBufferSize)
                        return CE_Failure;
                    bool bFreeOnError = false;
                    if (*ppBuffer)
                    {
                        if (*pnBufferSize < nSizeSize)
                            return CE_Failure;
                    }
                    else
                    {
                        *ppBuffer = VSI_MALLOC_VERBOSE(nSizeSize);
                        if (*ppBuffer == nullptr)
                            return CE_Failure;
                        bFreeOnError = true;
                    }
                    const auto nTileSize = static_cast<tmsize_t>(nSize);
                    bool bOK;
                    if (TIFFIsTiled(m_hTIFF))
                    {
                        bOK = TIFFReadRawTile(m_hTIFF, nBlockId, *ppBuffer,
                                              nTileSize) == nTileSize;
                    }
                    else
                    {
                        bOK = TIFFReadRawStrip(m_hTIFF, nBlockId, *ppBuffer,
                                               nTileSize) == nTileSize;
                    }
                    if (!bOK)
                    {
                        if (bFreeOnError)
                        {
                            VSIFree(*ppBuffer);
                            *ppBuffer = nullptr;
                        }
                        return CE_Failure;
                    }
                    if (nJPEGTableSize > 0)
                    {
                        GByte *pabyBuffer = static_cast<GByte *>(*ppBuffer);
                        memmove(pabyBuffer + 2 + nJPEGTableSize, pabyBuffer + 2,
                                static_cast<size_t>(nSize) - 2);
                        memcpy(pabyBuffer + 2, pJPEGTable, nJPEGTableSize);
                    }

                    if (m_nCompression == COMPRESSION_JPEG)
                    {
                        osDetailedFormat = GDALGetCompressionFormatForJPEG(
                            *ppBuffer, nSizeSize);
                        const CPLStringList aosTokens2(CSLTokenizeString2(
                            osDetailedFormat.c_str(), ";", 0));
                        if (m_nPlanarConfig == PLANARCONFIG_CONTIG &&
                            nBands == 4 && m_nPhotometric == PHOTOMETRIC_RGB &&
                            GetRasterBand(4)->GetColorInterpretation() ==
                                GCI_AlphaBand)
                        {
                            osDetailedFormat = aosTokens2[0];
                            for (int i = 1; i < aosTokens2.size(); ++i)
                            {
                                if (!STARTS_WITH_CI(aosTokens2[i],
                                                    "colorspace="))
                                {
                                    osDetailedFormat += ';';
                                    osDetailedFormat += aosTokens2[i];
                                }
                            }
                            osDetailedFormat += ";colorspace=RGBA";
                        }
                    }
                }
                if (ppszDetailedFormat)
                    *ppszDetailedFormat = VSIStrdup(osDetailedFormat.c_str());
                if (pnBufferSize)
                    *pnBufferSize = nSizeSize;
                return CE_None;
            }
        }
    }
    return CE_Failure;
}

struct GTiffDecompressContext
{
    // The mutex must be recursive because ThreadDecompressionFuncErrorHandler()
    // which acquires the mutex can be called from a section where the mutex is
    // already acquired.
    std::recursive_mutex oMutex{};
    bool bSuccess = true;

    std::vector<CPLErrorHandlerAccumulatorStruct> aoErrors{};

    VSIVirtualHandle *poHandle = nullptr;
    GTiffDataset *poDS = nullptr;
    GDALDataType eDT = GDT_Unknown;
    int nXOff = 0;
    int nYOff = 0;
    int nXSize = 0;
    int nYSize = 0;
    int nBlockXStart = 0;
    int nBlockYStart = 0;
    int nBlockXEnd = 0;
    int nBlockYEnd = 0;
    GByte *pabyData = nullptr;
    GDALDataType eBufType = GDT_Unknown;
    int nBufDTSize = 0;
    int nBandCount = 0;
    const int *panBandMap = nullptr;
    GSpacing nPixelSpace = 0;
    GSpacing nLineSpace = 0;
    GSpacing nBandSpace = 0;
    bool bHasPRead = false;
    bool bCacheAllBands = false;
    bool bSkipBlockCache = false;
    bool bUseBIPOptim = false;
    bool bUseDeinterleaveOptimNoBlockCache = false;
    bool bUseDeinterleaveOptimBlockCache = false;
    bool bIsTiled = false;
    bool bTIFFIsBigEndian = false;
    int nBlocksPerRow = 0;

    uint16_t nPredictor = 0;

    uint32_t nJPEGTableSize = 0;
    void *pJPEGTable = nullptr;
    uint16_t nYCrbCrSubSampling0 = 2;
    uint16_t nYCrbCrSubSampling1 = 2;

    uint16_t *pExtraSamples = nullptr;
    uint16_t nExtraSampleCount = 0;
};

struct GTiffDecompressJob
{
    GTiffDecompressContext *psContext = nullptr;
    int iSrcBandIdxSeparate =
        0;  // in [0, GetRasterCount()-1] in PLANARCONFIG_SEPARATE, or -1 in PLANARCONFIG_CONTIG
    int iDstBandIdxSeparate =
        0;  // in [0, nBandCount-1] in PLANARCONFIG_SEPARATE, or -1 in PLANARCONFIG_CONTIG
    int nXBlock = 0;
    int nYBlock = 0;
    vsi_l_offset nOffset = 0;
    vsi_l_offset nSize = 0;
};

/************************************************************************/
/*                  ThreadDecompressionFuncErrorHandler()               */
/************************************************************************/

static void CPL_STDCALL ThreadDecompressionFuncErrorHandler(
    CPLErr eErr, CPLErrorNum eErrorNum, const char *pszMsg)
{
    GTiffDecompressContext *psContext =
        static_cast<GTiffDecompressContext *>(CPLGetErrorHandlerUserData());
    std::lock_guard<std::recursive_mutex> oLock(psContext->oMutex);
    psContext->aoErrors.emplace_back(eErr, eErrorNum, pszMsg);
}

/************************************************************************/
/*                     ThreadDecompressionFunc()                        */
/************************************************************************/

/* static */ void GTiffDataset::ThreadDecompressionFunc(void *pData)
{
    const auto psJob = static_cast<const GTiffDecompressJob *>(pData);
    auto psContext = psJob->psContext;
    auto poDS = psContext->poDS;

    CPLErrorHandlerPusher oErrorHandler(ThreadDecompressionFuncErrorHandler,
                                        psContext);

    const int nBandsPerStrile =
        poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG ? poDS->nBands : 1;
    const int nBandsToWrite = poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG
                                  ? psContext->nBandCount
                                  : 1;

    const int nXOffsetInBlock = psJob->nXBlock == psContext->nBlockXStart
                                    ? psContext->nXOff % poDS->m_nBlockXSize
                                    : 0;
    const int nXOffsetInData =
        psJob->nXBlock == psContext->nBlockXStart
            ? 0
            : (psJob->nXBlock - psContext->nBlockXStart) * poDS->m_nBlockXSize -
                  (psContext->nXOff % poDS->m_nBlockXSize);
    const int nXSize =
        psJob->nXBlock == psContext->nBlockXStart
            ? (psJob->nXBlock == psContext->nBlockXEnd
                   ? psContext->nXSize
                   : poDS->m_nBlockXSize -
                         (psContext->nXOff % poDS->m_nBlockXSize))
        : psJob->nXBlock == psContext->nBlockXEnd
            ? (((psContext->nXOff + psContext->nXSize) % poDS->m_nBlockXSize) ==
                       0
                   ? poDS->m_nBlockXSize
                   : ((psContext->nXOff + psContext->nXSize) %
                      poDS->m_nBlockXSize))
            : poDS->m_nBlockXSize;

    const int nYOffsetInBlock = psJob->nYBlock == psContext->nBlockYStart
                                    ? psContext->nYOff % poDS->m_nBlockYSize
                                    : 0;
    const int nYOffsetInData =
        psJob->nYBlock == psContext->nBlockYStart
            ? 0
            : (psJob->nYBlock - psContext->nBlockYStart) * poDS->m_nBlockYSize -
                  (psContext->nYOff % poDS->m_nBlockYSize);
    const int nYSize =
        psJob->nYBlock == psContext->nBlockYStart
            ? (psJob->nYBlock == psContext->nBlockYEnd
                   ? psContext->nYSize
                   : poDS->m_nBlockYSize -
                         (psContext->nYOff % poDS->m_nBlockYSize))
        : psJob->nYBlock == psContext->nBlockYEnd
            ? (((psContext->nYOff + psContext->nYSize) % poDS->m_nBlockYSize) ==
                       0
                   ? poDS->m_nBlockYSize
                   : ((psContext->nYOff + psContext->nYSize) %
                      poDS->m_nBlockYSize))
            : poDS->m_nBlockYSize;
#if 0
    CPLDebug("GTiff",
             "nXBlock = %d, nYBlock = %d, "
             "nXOffsetInBlock = %d, nXOffsetInData = %d, nXSize = %d, "
             "nYOffsetInBlock = %d, nYOffsetInData = %d, nYSize = %d\n",
             psJob->nXBlock, psJob->nYBlock,
             nXOffsetInBlock, nXOffsetInData, nXSize,
             nYOffsetInBlock, nYOffsetInData, nYSize);
#endif

    if (psJob->nSize == 0)
    {
        {
            std::lock_guard<std::recursive_mutex> oLock(psContext->oMutex);
            if (!psContext->bSuccess)
                return;
        }
        const double dfNoDataValue =
            poDS->m_bNoDataSet ? poDS->m_dfNoDataValue : 0;
        for (int y = 0; y < nYSize; ++y)
        {
            for (int i = 0; i < nBandsToWrite; ++i)
            {
                const int iDstBandIdx =
                    poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG
                        ? i
                        : psJob->iDstBandIdxSeparate;
                GDALCopyWords64(
                    &dfNoDataValue, GDT_Float64, 0,
                    psContext->pabyData + iDstBandIdx * psContext->nBandSpace +
                        (y + nYOffsetInData) * psContext->nLineSpace +
                        nXOffsetInData * psContext->nPixelSpace,
                    psContext->eBufType,
                    static_cast<int>(psContext->nPixelSpace), nXSize);
            }
        }
        return;
    }

    const int nBandsToCache =
        psContext->bCacheAllBands ? poDS->nBands : nBandsToWrite;
    std::vector<GDALRasterBlock *> apoBlocks(nBandsToCache);
    std::vector<bool> abAlreadyLoadedBlocks(nBandsToCache);
    int nAlreadyLoadedBlocks = 0;
    std::vector<GByte> abyInput;

    struct FreeBlocks
    {
        std::vector<GDALRasterBlock *> &m_apoBlocks;

        explicit FreeBlocks(std::vector<GDALRasterBlock *> &apoBlocksIn)
            : m_apoBlocks(apoBlocksIn)
        {
        }

        ~FreeBlocks()
        {
            for (auto *poBlock : m_apoBlocks)
            {
                if (poBlock)
                    poBlock->DropLock();
            }
        }
    };

    FreeBlocks oFreeBlocks(apoBlocks);

    const auto LoadBlocks = [&]()
    {
        for (int i = 0; i < nBandsToCache; ++i)
        {
            const int iBand = psContext->bCacheAllBands ? i + 1
                              : poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG
                                  ? psContext->panBandMap[i]
                                  : psJob->iSrcBandIdxSeparate + 1;
            apoBlocks[i] = poDS->GetRasterBand(iBand)->TryGetLockedBlockRef(
                psJob->nXBlock, psJob->nYBlock);
            if (apoBlocks[i] == nullptr)
            {
                // Temporary disabling of dirty block flushing, otherwise
                // we can be in a deadlock situation, where the
                // GTiffDataset::SubmitCompressionJob() method waits for jobs
                // to be finished, that can't finish (actually be started)
                // because this task and its siblings are taking all the
                // available workers allowed by the global thread pool.
                GDALRasterBlock::EnterDisableDirtyBlockFlush();
                apoBlocks[i] = poDS->GetRasterBand(iBand)->GetLockedBlockRef(
                    psJob->nXBlock, psJob->nYBlock, TRUE);
                GDALRasterBlock::LeaveDisableDirtyBlockFlush();
                if (apoBlocks[i] == nullptr)
                    return false;
            }
            else
            {
                abAlreadyLoadedBlocks[i] = true;
                nAlreadyLoadedBlocks++;
            }
        }
        return true;
    };

    const auto AllocInputBuffer = [&]()
    {
        bool bError = false;
#if SIZEOF_VOIDP == 4
        if (psJob->nSize != static_cast<size_t>(psJob->nSize))
        {
            bError = true;
        }
        else
#endif
        {
            try
            {
                abyInput.resize(static_cast<size_t>(psJob->nSize));
            }
            catch (const std::exception &)
            {
                bError = true;
            }
        }
        if (bError)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate working buffer of size " CPL_FRMT_GUIB,
                     static_cast<GUIntBig>(psJob->nSize));
            return false;
        }
        return true;
    };

    if (psContext->bHasPRead)
    {
        {
            std::lock_guard<std::recursive_mutex> oLock(psContext->oMutex);
            if (!psContext->bSuccess)
                return;

            // Coverity Scan notices that GDALRasterBlock::Internalize() calls
            // CPLSleep() in a debug code path, and warns about that while
            // holding the above mutex.
            // coverity[sleep]
            if (!psContext->bSkipBlockCache && !LoadBlocks())
            {
                psContext->bSuccess = false;
                return;
            }
        }
        if (nAlreadyLoadedBlocks != nBandsToCache)
        {
            if (!AllocInputBuffer())
            {
                std::lock_guard<std::recursive_mutex> oLock(psContext->oMutex);
                psContext->bSuccess = false;
                return;
            }
            if (psContext->poHandle->PRead(abyInput.data(), abyInput.size(),
                                           psJob->nOffset) != abyInput.size())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot read " CPL_FRMT_GUIB
                         " bytes at offset " CPL_FRMT_GUIB,
                         static_cast<GUIntBig>(psJob->nSize),
                         static_cast<GUIntBig>(psJob->nOffset));

                std::lock_guard<std::recursive_mutex> oLock(psContext->oMutex);
                psContext->bSuccess = false;
                return;
            }
        }
    }
    else
    {
        std::lock_guard<std::recursive_mutex> oLock(psContext->oMutex);
        if (!psContext->bSuccess)
            return;

        // Coverity Scan notices that GDALRasterBlock::Internalize() calls
        // CPLSleep() in a debug code path, and warns about that while
        // holding the above mutex.
        // coverity[sleep]
        if (!psContext->bSkipBlockCache && !LoadBlocks())
        {
            psContext->bSuccess = false;
            return;
        }

        if (nAlreadyLoadedBlocks != nBandsToCache)
        {
            if (!AllocInputBuffer())
            {
                psContext->bSuccess = false;
                return;
            }
            if (psContext->poHandle->Seek(psJob->nOffset, SEEK_SET) != 0 ||
                psContext->poHandle->Read(abyInput.data(), abyInput.size(),
                                          1) != 1)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot read " CPL_FRMT_GUIB
                         " bytes at offset " CPL_FRMT_GUIB,
                         static_cast<GUIntBig>(psJob->nSize),
                         static_cast<GUIntBig>(psJob->nOffset));
                psContext->bSuccess = false;
                return;
            }
        }
    }

    const int nDTSize = GDALGetDataTypeSizeBytes(psContext->eDT);
    GByte *pDstPtr = psContext->pabyData +
                     nYOffsetInData * psContext->nLineSpace +
                     nXOffsetInData * psContext->nPixelSpace;

    if (nAlreadyLoadedBlocks != nBandsToCache)
    {
        // Generate a dummy in-memory TIFF file that has all the needed tags
        // from the original file
        CPLString osTmpFilename;
        osTmpFilename.Printf("/vsimem/decompress_%p.tif", psJob);
        VSILFILE *fpTmp = VSIFOpenL(osTmpFilename.c_str(), "wb+");
        TIFF *hTIFFTmp =
            VSI_TIFFOpen(osTmpFilename.c_str(),
                         psContext->bTIFFIsBigEndian ? "wb+" : "wl+", fpTmp);
        CPLAssert(hTIFFTmp != nullptr);
        const int nBlockYSize =
            (psContext->bIsTiled ||
             psJob->nYBlock < poDS->m_nBlocksPerColumn - 1)
                ? poDS->m_nBlockYSize
            : (poDS->nRasterYSize % poDS->m_nBlockYSize) == 0
                ? poDS->m_nBlockYSize
                : poDS->nRasterYSize % poDS->m_nBlockYSize;
        TIFFSetField(hTIFFTmp, TIFFTAG_IMAGEWIDTH, poDS->m_nBlockXSize);
        TIFFSetField(hTIFFTmp, TIFFTAG_IMAGELENGTH, nBlockYSize);
        TIFFSetField(hTIFFTmp, TIFFTAG_BITSPERSAMPLE, poDS->m_nBitsPerSample);
        TIFFSetField(hTIFFTmp, TIFFTAG_COMPRESSION, poDS->m_nCompression);
        TIFFSetField(hTIFFTmp, TIFFTAG_PHOTOMETRIC, poDS->m_nPhotometric);
        TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLEFORMAT, poDS->m_nSampleFormat);
        TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLESPERPIXEL,
                     poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG
                         ? poDS->m_nSamplesPerPixel
                         : 1);
        TIFFSetField(hTIFFTmp, TIFFTAG_ROWSPERSTRIP, nBlockYSize);
        TIFFSetField(hTIFFTmp, TIFFTAG_PLANARCONFIG, poDS->m_nPlanarConfig);
        if (psContext->nPredictor != PREDICTOR_NONE)
            TIFFSetField(hTIFFTmp, TIFFTAG_PREDICTOR, psContext->nPredictor);
        if (poDS->m_nCompression == COMPRESSION_LERC)
        {
            TIFFSetField(hTIFFTmp, TIFFTAG_LERC_PARAMETERS, 2,
                         poDS->m_anLercAddCompressionAndVersion);
        }
        else if (poDS->m_nCompression == COMPRESSION_JPEG)
        {
            if (psContext->pJPEGTable)
            {
                TIFFSetField(hTIFFTmp, TIFFTAG_JPEGTABLES,
                             psContext->nJPEGTableSize, psContext->pJPEGTable);
            }
            if (poDS->m_nPhotometric == PHOTOMETRIC_YCBCR)
            {
                TIFFSetField(hTIFFTmp, TIFFTAG_YCBCRSUBSAMPLING,
                             psContext->nYCrbCrSubSampling0,
                             psContext->nYCrbCrSubSampling1);
            }
        }
        if (poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
        {
            if (psContext->pExtraSamples)
            {
                TIFFSetField(hTIFFTmp, TIFFTAG_EXTRASAMPLES,
                             psContext->nExtraSampleCount,
                             psContext->pExtraSamples);
            }
            else
            {
                const int nSamplesAccountedFor =
                    poDS->m_nPhotometric == PHOTOMETRIC_RGB          ? 3
                    : poDS->m_nPhotometric == PHOTOMETRIC_MINISBLACK ? 1
                                                                     : 0;
                if (nSamplesAccountedFor > 0 &&
                    poDS->m_nSamplesPerPixel > nSamplesAccountedFor)
                {
                    // If the input image is not compliant regarndig ExtraSamples,
                    // generate a synthetic one to avoid gazillons of warnings
                    const auto nExtraSampleCount = static_cast<uint16_t>(
                        poDS->m_nSamplesPerPixel - nSamplesAccountedFor);
                    std::vector<uint16_t> anExtraSamples(
                        nExtraSampleCount, EXTRASAMPLE_UNSPECIFIED);
                    TIFFSetField(hTIFFTmp, TIFFTAG_EXTRASAMPLES,
                                 nExtraSampleCount, anExtraSamples.data());
                }
            }
        }
        TIFFWriteCheck(hTIFFTmp, FALSE, "ThreadDecompressionFunc");
        TIFFWriteDirectory(hTIFFTmp);
        XTIFFClose(hTIFFTmp);

        // Re-open file
        hTIFFTmp = VSI_TIFFOpen(osTmpFilename.c_str(), "r", fpTmp);
        CPLAssert(hTIFFTmp != nullptr);
        poDS->RestoreVolatileParameters(hTIFFTmp);

        bool bRet = true;
        // Request m_nBlockYSize line in the block, except on the bottom-most
        // tile/strip.
        const int nBlockReqYSize =
            (psJob->nYBlock < poDS->m_nBlocksPerColumn - 1)
                ? poDS->m_nBlockYSize
            : (poDS->nRasterYSize % poDS->m_nBlockYSize) == 0
                ? poDS->m_nBlockYSize
                : poDS->nRasterYSize % poDS->m_nBlockYSize;

        const size_t nReqSize = static_cast<size_t>(poDS->m_nBlockXSize) *
                                nBlockReqYSize * nBandsPerStrile * nDTSize;

        GByte *pabyOutput;
        std::vector<GByte> abyOutput;
        if (poDS->m_nCompression == COMPRESSION_NONE &&
            !TIFFIsByteSwapped(poDS->m_hTIFF) && abyInput.size() >= nReqSize &&
            (psContext->bSkipBlockCache || nBandsPerStrile > 1))
        {
            pabyOutput = abyInput.data();
        }
        else
        {
            if (psContext->bSkipBlockCache || nBandsPerStrile > 1)
            {
                abyOutput.resize(nReqSize);
                pabyOutput = abyOutput.data();
            }
            else
            {
                pabyOutput = static_cast<GByte *>(apoBlocks[0]->GetDataRef());
            }
            if (!TIFFReadFromUserBuffer(hTIFFTmp, 0, abyInput.data(),
                                        abyInput.size(), pabyOutput,
                                        nReqSize) &&
                !poDS->m_bIgnoreReadErrors)
            {
                bRet = false;
            }
        }
        XTIFFClose(hTIFFTmp);
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTmp));
        VSIUnlink(osTmpFilename.c_str());

        if (!bRet)
        {
            std::lock_guard<std::recursive_mutex> oLock(psContext->oMutex);
            psContext->bSuccess = false;
            return;
        }

        if (!psContext->bSkipBlockCache && nBandsPerStrile > 1)
        {
            // Copy pixel-interleaved all-band buffer to cached blocks

            if (psContext->bUseDeinterleaveOptimBlockCache)
            {
                // Optimization
                std::vector<void *> ppDestBuffers(poDS->nBands);
                for (int i = 0; i < poDS->nBands; ++i)
                {
                    ppDestBuffers[i] = apoBlocks[i]->GetDataRef();
                }
                GDALDeinterleave(pabyOutput, psContext->eDT, poDS->nBands,
                                 ppDestBuffers.data(), psContext->eDT,
                                 static_cast<size_t>(nBlockReqYSize) *
                                     poDS->m_nBlockXSize);
            }
            else
            {
                // General case
                for (int i = 0; i < nBandsToCache; ++i)
                {
                    if (!abAlreadyLoadedBlocks[i])
                    {
                        const int iBand = psContext->bCacheAllBands
                                              ? i
                                              : psContext->panBandMap[i] - 1;
                        GDALCopyWords64(pabyOutput + iBand * nDTSize,
                                        psContext->eDT, nDTSize * poDS->nBands,
                                        apoBlocks[i]->GetDataRef(),
                                        psContext->eDT, nDTSize,
                                        static_cast<size_t>(nBlockReqYSize) *
                                            poDS->m_nBlockXSize);
                    }
                }
            }
        }

        const GByte *pSrcPtr =
            pabyOutput +
            (static_cast<size_t>(nYOffsetInBlock) * poDS->m_nBlockXSize +
             nXOffsetInBlock) *
                nDTSize * nBandsPerStrile;
        const size_t nSrcLineInc = static_cast<size_t>(poDS->m_nBlockXSize) *
                                   nDTSize * nBandsPerStrile;

        // Optimization when writing to BIP buffer.
        if (psContext->bUseBIPOptim)
        {
            for (int y = 0; y < nYSize; ++y)
            {
                GDALCopyWords64(pSrcPtr, psContext->eDT, nDTSize, pDstPtr,
                                psContext->eBufType, psContext->nBufDTSize,
                                static_cast<size_t>(nXSize) * poDS->nBands);
                pSrcPtr += nSrcLineInc;
                pDstPtr += psContext->nLineSpace;
            }
            return;
        }

        if (psContext->bSkipBlockCache)
        {
            // Copy from pixel-interleaved all-band buffer (or temporary buffer
            // for single-band/separate case) into final buffer
            if (psContext->bUseDeinterleaveOptimNoBlockCache)
            {
                // Optimization
                std::vector<void *> ppDestBuffers(psContext->nBandCount);
                for (int i = 0; i < psContext->nBandCount; ++i)
                {
                    ppDestBuffers[i] =
                        pDstPtr +
                        (psContext->panBandMap[i] - 1) * psContext->nBandSpace;
                }
                for (int y = 0; y < nYSize; ++y)
                {
                    GDALDeinterleave(
                        pSrcPtr, psContext->eDT, psContext->nBandCount,
                        ppDestBuffers.data(), psContext->eDT, nXSize);
                    pSrcPtr += nSrcLineInc;
                    for (int i = 0; i < psContext->nBandCount; ++i)
                    {
                        ppDestBuffers[i] =
                            static_cast<GByte *>(ppDestBuffers[i]) +
                            psContext->nLineSpace;
                    }
                }
                return;
            }

            // General case
            for (int y = 0; y < nYSize; ++y)
            {
                for (int i = 0; i < nBandsToWrite; ++i)
                {
                    const int iSrcBandIdx =
                        poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG
                            ? psContext->panBandMap[i] - 1
                            : 0;
                    const int iDstBandIdx =
                        poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG
                            ? i
                            : psJob->iDstBandIdxSeparate;
                    GDALCopyWords64(
                        pSrcPtr + iSrcBandIdx * nDTSize + y * nSrcLineInc,
                        psContext->eDT, nDTSize * nBandsPerStrile,
                        pDstPtr + iDstBandIdx * psContext->nBandSpace +
                            y * psContext->nLineSpace,
                        psContext->eBufType,
                        static_cast<int>(psContext->nPixelSpace), nXSize);
                }
            }
            return;
        }
    }

    CPLAssert(!psContext->bSkipBlockCache);

    // Compose cached blocks into final buffer
    for (int i = 0; i < nBandsToWrite; ++i)
    {
        const int iSrcBandIdx =
            psContext->bCacheAllBands ? psContext->panBandMap[i] - 1
            : poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG ? i
                                                           : 0;
        assert(iSrcBandIdx >= 0);
        const int iDstBandIdx = poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG
                                    ? i
                                    : psJob->iDstBandIdxSeparate;
        const GByte *pSrcPtr =
            static_cast<GByte *>(apoBlocks[iSrcBandIdx]->GetDataRef()) +
            (static_cast<size_t>(nYOffsetInBlock) * poDS->m_nBlockXSize +
             nXOffsetInBlock) *
                nDTSize;
        for (int y = 0; y < nYSize; ++y)
        {
            GDALCopyWords64(pSrcPtr + static_cast<size_t>(y) *
                                          poDS->m_nBlockXSize * nDTSize,
                            psContext->eDT, nDTSize,
                            pDstPtr + iDstBandIdx * psContext->nBandSpace +
                                y * psContext->nLineSpace,
                            psContext->eBufType,
                            static_cast<int>(psContext->nPixelSpace), nXSize);
        }
    }
}

/************************************************************************/
/*                    IsMultiThreadedReadCompatible()                   */
/************************************************************************/

bool GTiffDataset::IsMultiThreadedReadCompatible() const
{
    return cpl::down_cast<GTiffRasterBand *>(papoBands[0])
               ->IsBaseGTiffClass() &&
           !m_bStreamingIn && !m_bStreamingOut &&
           (m_nCompression == COMPRESSION_NONE ||
            m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
            m_nCompression == COMPRESSION_LZW ||
            m_nCompression == COMPRESSION_PACKBITS ||
            m_nCompression == COMPRESSION_LZMA ||
            m_nCompression == COMPRESSION_ZSTD ||
            m_nCompression == COMPRESSION_LERC ||
            m_nCompression == COMPRESSION_JXL ||
            m_nCompression == COMPRESSION_WEBP ||
            m_nCompression == COMPRESSION_JPEG);
}

/************************************************************************/
/*                        MultiThreadedRead()                           */
/************************************************************************/

CPLErr GTiffDataset::MultiThreadedRead(int nXOff, int nYOff, int nXSize,
                                       int nYSize, void *pData,
                                       GDALDataType eBufType, int nBandCount,
                                       const int *panBandMap,
                                       GSpacing nPixelSpace,
                                       GSpacing nLineSpace, GSpacing nBandSpace)
{
    auto poQueue = m_poThreadPool->CreateJobQueue();
    if (poQueue == nullptr)
    {
        return CE_Failure;
    }

    const int nBlockXStart = nXOff / m_nBlockXSize;
    const int nBlockYStart = nYOff / m_nBlockYSize;
    const int nBlockXEnd = (nXOff + nXSize - 1) / m_nBlockXSize;
    const int nBlockYEnd = (nYOff + nYSize - 1) / m_nBlockYSize;
    const int nXBlocks = nBlockXEnd - nBlockXStart + 1;
    const int nYBlocks = nBlockYEnd - nBlockYStart + 1;
    const int nStrilePerBlock =
        m_nPlanarConfig == PLANARCONFIG_CONTIG ? 1 : nBandCount;
    const int nBlocks = nXBlocks * nYBlocks * nStrilePerBlock;

    GTiffDecompressContext sContext;
    sContext.poHandle = VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF));
    sContext.bHasPRead =
        sContext.poHandle->HasPRead()
#ifdef DEBUG
        && CPLTestBool(CPLGetConfigOption("GTIFF_ALLOW_PREAD", "YES"))
#endif
        ;
    sContext.poDS = this;
    sContext.eDT = GetRasterBand(1)->GetRasterDataType();
    sContext.nXOff = nXOff;
    sContext.nYOff = nYOff;
    sContext.nXSize = nXSize;
    sContext.nYSize = nYSize;
    sContext.nBlockXStart = nBlockXStart;
    sContext.nBlockXEnd = nBlockXEnd;
    sContext.nBlockYStart = nBlockYStart;
    sContext.nBlockYEnd = nBlockYEnd;
    sContext.pabyData = static_cast<GByte *>(pData);
    sContext.eBufType = eBufType;
    sContext.nBufDTSize = GDALGetDataTypeSizeBytes(eBufType);
    sContext.nBandCount = nBandCount;
    sContext.panBandMap = panBandMap;
    sContext.nPixelSpace = nPixelSpace;
    sContext.nLineSpace = nLineSpace;
    // Setting nBandSpace to a dummy value when nBandCount == 1 helps detecting
    // bad computations of target buffer address
    // (https://github.com/rasterio/rasterio/issues/2847)
    sContext.nBandSpace = nBandCount == 1 ? 0xDEADBEEF : nBandSpace;
    sContext.bIsTiled = CPL_TO_BOOL(TIFFIsTiled(m_hTIFF));
    sContext.bTIFFIsBigEndian = CPL_TO_BOOL(TIFFIsBigEndian(m_hTIFF));
    sContext.nPredictor = PREDICTOR_NONE;
    sContext.nBlocksPerRow = m_nBlocksPerRow;

    if (m_bDirectIO)
    {
        sContext.bSkipBlockCache = true;
    }
    else if (nXOff == 0 && nYOff == 0 && nXSize == nRasterXSize &&
             nYSize == nRasterYSize)
    {
        if (m_nPlanarConfig == PLANARCONFIG_SEPARATE)
        {
            sContext.bSkipBlockCache = true;
        }
        else if (nBandCount == nBands)
        {
            sContext.bSkipBlockCache = true;
            for (int i = 0; i < nBands; ++i)
            {
                if (panBandMap[i] != i + 1)
                {
                    sContext.bSkipBlockCache = false;
                    break;
                }
            }
        }
    }

    if (m_nPlanarConfig == PLANARCONFIG_CONTIG && nBandCount == nBands &&
        nPixelSpace == nBands * static_cast<GSpacing>(sContext.nBufDTSize))
    {
        sContext.bUseBIPOptim = true;
        for (int i = 0; i < nBands; ++i)
        {
            if (panBandMap[i] != i + 1)
            {
                sContext.bUseBIPOptim = false;
                break;
            }
        }
    }

    if (m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        (nBands == 3 || nBands == 4) && nBands == nBandCount &&
        (sContext.eDT == GDT_Byte || sContext.eDT == GDT_Int16 ||
         sContext.eDT == GDT_UInt16))
    {
        if (sContext.bSkipBlockCache)
        {
            if (sContext.eBufType == sContext.eDT &&
                nPixelSpace == sContext.nBufDTSize)
            {
                sContext.bUseDeinterleaveOptimNoBlockCache = true;
            }
        }
        else
        {
            sContext.bUseDeinterleaveOptimBlockCache = true;
            for (int i = 0; i < nBands; ++i)
            {
                if (panBandMap[i] != i + 1)
                {
                    sContext.bUseDeinterleaveOptimBlockCache = false;
                    break;
                }
            }
        }
    }

    // In contig mode, if only one band is requested, check if we have
    // enough cache to cache all bands.
    if (!sContext.bSkipBlockCache && nBands != 1 &&
        m_nPlanarConfig == PLANARCONFIG_CONTIG && nBandCount == 1)
    {
        const GIntBig nRequiredMem = static_cast<GIntBig>(nBands) * nXBlocks *
                                     nYBlocks * m_nBlockXSize * m_nBlockYSize *
                                     GDALGetDataTypeSizeBytes(sContext.eDT);
        if (nRequiredMem > GDALGetCacheMax64())
        {
            if (!m_bHasWarnedDisableAggressiveBandCaching)
            {
                CPLDebug("GTiff",
                         "Disable aggressive band caching. "
                         "Cache not big enough. "
                         "At least " CPL_FRMT_GIB " bytes necessary",
                         nRequiredMem);
                m_bHasWarnedDisableAggressiveBandCaching = true;
            }
        }
        else
        {
            sContext.bCacheAllBands = true;
            if ((nBands == 3 || nBands == 4) &&
                (sContext.eDT == GDT_Byte || sContext.eDT == GDT_Int16 ||
                 sContext.eDT == GDT_UInt16))
            {
                sContext.bUseDeinterleaveOptimBlockCache = true;
            }
        }
    }

    if (eAccess == GA_Update)
    {
        std::vector<int> anBandsToCheck;
        if (m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands > 1)
        {
            for (int i = 0; i < nBands; ++i)
            {
                anBandsToCheck.push_back(i);
            }
        }
        else
        {
            for (int i = 0; i < nBandCount; ++i)
            {
                anBandsToCheck.push_back(panBandMap[i] - 1);
            }
        }
        if (!anBandsToCheck.empty())
        {
            // If at least one block in the region of intersest is dirty,
            // fallback to normal reading code path to be able to retrieve
            // content partly from the block cache.
            // An alternative that was implemented in GDAL 3.6 to 3.8.0 was
            // to flush dirty blocks, but this could cause many write&read&write
            // cycles in some gdalwarp scenarios.
            // Cf https://github.com/OSGeo/gdal/issues/8729
            bool bUseBaseImplementation = false;
            for (int y = 0; y < nYBlocks; ++y)
            {
                for (int x = 0; x < nXBlocks; ++x)
                {
                    for (const int iBand : anBandsToCheck)
                    {
                        if (m_nLoadedBlock >= 0 && m_bLoadedBlockDirty &&
                            cpl::down_cast<GTiffRasterBand *>(papoBands[iBand])
                                    ->ComputeBlockId(nBlockXStart + x,
                                                     nBlockYStart + y) ==
                                m_nLoadedBlock)
                        {
                            bUseBaseImplementation = true;
                            goto after_loop;
                        }
                        auto poBlock = papoBands[iBand]->TryGetLockedBlockRef(
                            nBlockXStart + x, nBlockYStart + y);
                        if (poBlock)
                        {
                            if (poBlock->GetDirty())
                            {
                                poBlock->DropLock();
                                bUseBaseImplementation = true;
                                goto after_loop;
                            }
                            poBlock->DropLock();
                        }
                    }
                }
            }
        after_loop:
            if (bUseBaseImplementation)
            {
                ++m_nDisableMultiThreadedRead;
                GDALRasterIOExtraArg sExtraArg;
                INIT_RASTERIO_EXTRA_ARG(sExtraArg);
                const CPLErr eErr = GDALDataset::IRasterIO(
                    GF_Read, nXOff, nYOff, nXSize, nYSize, pData, nXSize,
                    nYSize, eBufType, nBandCount, const_cast<int *>(panBandMap),
                    nPixelSpace, nLineSpace, nBandSpace, &sExtraArg);
                --m_nDisableMultiThreadedRead;
                return eErr;
            }
        }

        // Make sure that all blocks that we are going to read and that are
        // being written by a worker thread are completed.
        // cppcheck-suppress constVariableReference
        auto &oQueue =
            m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
        if (!oQueue.empty())
        {
            for (int y = 0; y < nYBlocks; ++y)
            {
                for (int x = 0; x < nXBlocks; ++x)
                {
                    for (int i = 0; i < nStrilePerBlock; ++i)
                    {
                        int nBlockId =
                            nBlockXStart + x +
                            (nBlockYStart + y) * sContext.nBlocksPerRow;
                        if (m_nPlanarConfig == PLANARCONFIG_SEPARATE)
                            nBlockId += (panBandMap[i] - 1) * m_nBlocksPerBand;

                        WaitCompletionForBlock(nBlockId);
                    }
                }
            }
        }

        // Flush to file, and then to disk if using pread() interface
        VSI_TIFFFlushBufferedWrite(TIFFClientdata(m_hTIFF));
        if (sContext.bHasPRead)
            sContext.poHandle->Flush();
    }

    if (GTIFFSupportsPredictor(m_nCompression))
    {
        TIFFGetField(m_hTIFF, TIFFTAG_PREDICTOR, &sContext.nPredictor);
    }
    else if (m_nCompression == COMPRESSION_JPEG)
    {
        TIFFGetField(m_hTIFF, TIFFTAG_JPEGTABLES, &sContext.nJPEGTableSize,
                     &sContext.pJPEGTable);
        if (m_nPhotometric == PHOTOMETRIC_YCBCR)
        {
            TIFFGetFieldDefaulted(m_hTIFF, TIFFTAG_YCBCRSUBSAMPLING,
                                  &sContext.nYCrbCrSubSampling0,
                                  &sContext.nYCrbCrSubSampling1);
        }
    }
    if (m_nPlanarConfig == PLANARCONFIG_CONTIG)
    {
        TIFFGetField(m_hTIFF, TIFFTAG_EXTRASAMPLES, &sContext.nExtraSampleCount,
                     &sContext.pExtraSamples);
    }

    // Create one job per tile/strip
    vsi_l_offset nFileSize = 0;
    std::vector<GTiffDecompressJob> asJobs(nBlocks);
    std::vector<vsi_l_offset> anOffsets(nBlocks);
    std::vector<size_t> anSizes(nBlocks);
    int iJob = 0;
    int nAdviseReadRanges = 0;
    const size_t nAdviseReadTotalBytesLimit =
        sContext.poHandle->GetAdviseReadTotalBytesLimit();
    size_t nAdviseReadAccBytes = 0;
    for (int y = 0; y < nYBlocks; ++y)
    {
        for (int x = 0; x < nXBlocks; ++x)
        {
            for (int i = 0; i < nStrilePerBlock; ++i)
            {
                asJobs[iJob].psContext = &sContext;
                asJobs[iJob].iSrcBandIdxSeparate =
                    m_nPlanarConfig == PLANARCONFIG_CONTIG ? -1
                                                           : panBandMap[i] - 1;
                asJobs[iJob].iDstBandIdxSeparate =
                    m_nPlanarConfig == PLANARCONFIG_CONTIG ? -1 : i;
                asJobs[iJob].nXBlock = nBlockXStart + x;
                asJobs[iJob].nYBlock = nBlockYStart + y;

                int nBlockId = asJobs[iJob].nXBlock +
                               asJobs[iJob].nYBlock * sContext.nBlocksPerRow;
                if (m_nPlanarConfig == PLANARCONFIG_SEPARATE)
                    nBlockId +=
                        asJobs[iJob].iSrcBandIdxSeparate * m_nBlocksPerBand;

                if (!sContext.bHasPRead)
                {
                    // Taking the mutex here is only needed when bHasPRead ==
                    // false since we could have concurrent uses of the handle,
                    // when when reading the TIFF TileOffsets / TileByteCounts
                    // array
                    std::lock_guard<std::recursive_mutex> oLock(
                        sContext.oMutex);

                    IsBlockAvailable(nBlockId, &asJobs[iJob].nOffset,
                                     &asJobs[iJob].nSize);
                }
                else
                {
                    IsBlockAvailable(nBlockId, &asJobs[iJob].nOffset,
                                     &asJobs[iJob].nSize);
                }

                // Sanity check on block size
                if (asJobs[iJob].nSize > 100U * 1024 * 1024)
                {
                    if (nFileSize == 0)
                    {
                        std::lock_guard<std::recursive_mutex> oLock(
                            sContext.oMutex);
                        sContext.poHandle->Seek(0, SEEK_END);
                        nFileSize = sContext.poHandle->Tell();
                    }
                    if (asJobs[iJob].nSize > nFileSize)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot read " CPL_FRMT_GUIB
                                 " bytes at offset " CPL_FRMT_GUIB,
                                 static_cast<GUIntBig>(asJobs[iJob].nSize),
                                 static_cast<GUIntBig>(asJobs[iJob].nOffset));

                        std::lock_guard<std::recursive_mutex> oLock(
                            sContext.oMutex);
                        sContext.bSuccess = false;
                        break;
                    }
                }

                // Only request in AdviseRead() ranges for blocks we don't
                // have in cache.
                bool bAddToAdviseRead = true;
                if (m_nPlanarConfig == PLANARCONFIG_SEPARATE)
                {
                    auto poBlock =
                        GetRasterBand(panBandMap[i])
                            ->TryGetLockedBlockRef(asJobs[iJob].nXBlock,
                                                   asJobs[iJob].nYBlock);
                    if (poBlock)
                    {
                        poBlock->DropLock();
                        bAddToAdviseRead = false;
                    }
                }
                else
                {
                    bool bAllCached = true;
                    for (int iBand = 0; iBand < nBandCount; ++iBand)
                    {
                        auto poBlock =
                            GetRasterBand(panBandMap[iBand])
                                ->TryGetLockedBlockRef(asJobs[iJob].nXBlock,
                                                       asJobs[iJob].nYBlock);
                        if (poBlock)
                        {
                            poBlock->DropLock();
                        }
                        else
                        {
                            bAllCached = false;
                            break;
                        }
                    }
                    if (bAllCached)
                        bAddToAdviseRead = false;
                }

                if (bAddToAdviseRead)
                {
                    anOffsets[nAdviseReadRanges] = asJobs[iJob].nOffset;
                    anSizes[nAdviseReadRanges] =
                        static_cast<size_t>(std::min<vsi_l_offset>(
                            std::numeric_limits<size_t>::max(),
                            asJobs[iJob].nSize));

                    // If the total number of bytes we must read excess the
                    // capacity of AdviseRead(), then split the RasterIO()
                    // request in 2 halves.
                    if (nAdviseReadTotalBytesLimit > 0 &&
                        anSizes[nAdviseReadRanges] <
                            nAdviseReadTotalBytesLimit &&
                        anSizes[nAdviseReadRanges] >
                            nAdviseReadTotalBytesLimit - nAdviseReadAccBytes &&
                        nYBlocks >= 2)
                    {
                        const int nYOff2 =
                            (nBlockYStart + nYBlocks / 2) * m_nBlockYSize;
                        CPLDebugOnly("GTiff",
                                     "Splitting request (%d,%d,%dx%d) into "
                                     "(%d,%d,%dx%d) and (%d,%d,%dx%d)",
                                     nXOff, nYOff, nXSize, nYSize, nXOff, nYOff,
                                     nXSize, nYOff2 - nYOff, nXOff, nYOff2,
                                     nXSize, nYOff + nYSize - nYOff2);

                        asJobs.clear();
                        anOffsets.clear();
                        anSizes.clear();
                        poQueue.reset();

                        CPLErr eErr = MultiThreadedRead(
                            nXOff, nYOff, nXSize, nYOff2 - nYOff, pData,
                            eBufType, nBandCount, panBandMap, nPixelSpace,
                            nLineSpace, nBandSpace);
                        if (eErr == CE_None)
                        {
                            eErr = MultiThreadedRead(
                                nXOff, nYOff2, nXSize, nYOff + nYSize - nYOff2,
                                static_cast<GByte *>(pData) +
                                    (nYOff2 - nYOff) * nLineSpace,
                                eBufType, nBandCount, panBandMap, nPixelSpace,
                                nLineSpace, nBandSpace);
                        }
                        return eErr;
                    }
                    nAdviseReadAccBytes += anSizes[nAdviseReadRanges];

                    ++nAdviseReadRanges;
                }

                ++iJob;
            }
        }
    }

    if (sContext.bSuccess)
    {
        // Potentially start asynchronous fetching of ranges depending on file
        // implementation
        if (nAdviseReadRanges > 0)
        {
            sContext.poHandle->AdviseRead(nAdviseReadRanges, anOffsets.data(),
                                          anSizes.data());
        }

        // We need to do that as threads will access the block cache
        TemporarilyDropReadWriteLock();

        for (auto &sJob : asJobs)
        {
            poQueue->SubmitJob(ThreadDecompressionFunc, &sJob);
        }

        // Wait for all jobs to have been completed
        poQueue->WaitCompletion();

        // Undo effect of above TemporarilyDropReadWriteLock()
        ReacquireReadWriteLock();

        // Re-emit errors caught in threads
        for (const auto &oError : sContext.aoErrors)
        {
            CPLError(oError.type, oError.no, "%s", oError.msg.c_str());
        }
    }

    return sContext.bSuccess ? CE_None : CE_Failure;
}

/************************************************************************/
/*                        FetchBufferVirtualMemIO                       */
/************************************************************************/

class FetchBufferVirtualMemIO final
{
    const GByte *pabySrcData;
    size_t nMappingSize;
    GByte *pTempBuffer;

  public:
    FetchBufferVirtualMemIO(const GByte *pabySrcDataIn, size_t nMappingSizeIn,
                            GByte *pTempBufferIn)
        : pabySrcData(pabySrcDataIn), nMappingSize(nMappingSizeIn),
          pTempBuffer(pTempBufferIn)
    {
    }

    const GByte *FetchBytes(vsi_l_offset nOffset, int nPixels, int nDTSize,
                            bool bIsByteSwapped, bool bIsComplex, int nBlockId)
    {
        if (nOffset + static_cast<size_t>(nPixels) * nDTSize > nMappingSize)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Missing data for block %d",
                     nBlockId);
            return nullptr;
        }
        if (!bIsByteSwapped)
            return pabySrcData + nOffset;
        memcpy(pTempBuffer, pabySrcData + nOffset,
               static_cast<size_t>(nPixels) * nDTSize);
        if (bIsComplex)
            GDALSwapWords(pTempBuffer, nDTSize / 2, 2 * nPixels, nDTSize / 2);
        else
            GDALSwapWords(pTempBuffer, nDTSize, nPixels, nDTSize);
        return pTempBuffer;
    }

    bool FetchBytes(GByte *pabyDstBuffer, vsi_l_offset nOffset, int nPixels,
                    int nDTSize, bool bIsByteSwapped, bool bIsComplex,
                    int nBlockId)
    {
        if (nOffset + static_cast<size_t>(nPixels) * nDTSize > nMappingSize)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Missing data for block %d",
                     nBlockId);
            return false;
        }
        memcpy(pabyDstBuffer, pabySrcData + nOffset,
               static_cast<size_t>(nPixels) * nDTSize);
        if (bIsByteSwapped)
        {
            if (bIsComplex)
                GDALSwapWords(pabyDstBuffer, nDTSize / 2, 2 * nPixels,
                              nDTSize / 2);
            else
                GDALSwapWords(pabyDstBuffer, nDTSize, nPixels, nDTSize);
        }
        return true;
    }

    // cppcheck-suppress unusedStructMember
    static const bool bMinimizeIO = false;
};

/************************************************************************/
/*                         VirtualMemIO()                               */
/************************************************************************/

int GTiffDataset::VirtualMemIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                               int nXSize, int nYSize, void *pData,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, int nBandCount,
                               int *panBandMap, GSpacing nPixelSpace,
                               GSpacing nLineSpace, GSpacing nBandSpace,
                               GDALRasterIOExtraArg *psExtraArg)
{
    if (eAccess == GA_Update || eRWFlag == GF_Write || m_bStreamingIn)
        return -1;

    // Only know how to deal with nearest neighbour in this optimized routine.
    if ((nXSize != nBufXSize || nYSize != nBufYSize) && psExtraArg != nullptr &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
    {
        return -1;
    }

    const GDALDataType eDataType = GetRasterBand(1)->GetRasterDataType();
    const int nDTSizeBits = GDALGetDataTypeSizeBits(eDataType);
    if (!(m_nCompression == COMPRESSION_NONE &&
          (m_nPhotometric == PHOTOMETRIC_MINISBLACK ||
           m_nPhotometric == PHOTOMETRIC_RGB ||
           m_nPhotometric == PHOTOMETRIC_PALETTE) &&
          m_nBitsPerSample == nDTSizeBits))
    {
        m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
        return -1;
    }

    size_t nMappingSize = 0;
    GByte *pabySrcData = nullptr;
    if (STARTS_WITH(m_pszFilename, "/vsimem/"))
    {
        vsi_l_offset nDataLength = 0;
        pabySrcData = VSIGetMemFileBuffer(m_pszFilename, &nDataLength, FALSE);
        nMappingSize = static_cast<size_t>(nDataLength);
        if (pabySrcData == nullptr)
            return -1;
    }
    else if (m_psVirtualMemIOMapping == nullptr)
    {
        VSILFILE *fp = VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF));
        if (!CPLIsVirtualMemFileMapAvailable() ||
            VSIFGetNativeFileDescriptorL(fp) == nullptr)
        {
            m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
            return -1;
        }
        if (VSIFSeekL(fp, 0, SEEK_END) != 0)
        {
            m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
            return -1;
        }
        const vsi_l_offset nLength = VSIFTellL(fp);
        if (static_cast<size_t>(nLength) != nLength)
        {
            m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
            return -1;
        }
        if (m_eVirtualMemIOUsage == VirtualMemIOEnum::IF_ENOUGH_RAM)
        {
            GIntBig nRAM = CPLGetUsablePhysicalRAM();
            if (static_cast<GIntBig>(nLength) > nRAM)
            {
                CPLDebug("GTiff",
                         "Not enough RAM to map whole file into memory.");
                m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
                return -1;
            }
        }
        m_psVirtualMemIOMapping = CPLVirtualMemFileMapNew(
            fp, 0, nLength, VIRTUALMEM_READONLY, nullptr, nullptr);
        if (m_psVirtualMemIOMapping == nullptr)
        {
            m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
            return -1;
        }
        m_eVirtualMemIOUsage = VirtualMemIOEnum::YES;
    }

    if (m_psVirtualMemIOMapping)
    {
#ifdef DEBUG
        CPLDebug("GTiff", "Using VirtualMemIO");
#endif
        nMappingSize = CPLVirtualMemGetSize(m_psVirtualMemIOMapping);
        pabySrcData =
            static_cast<GByte *>(CPLVirtualMemGetAddr(m_psVirtualMemIOMapping));
    }

    if (TIFFIsByteSwapped(m_hTIFF) && m_pTempBufferForCommonDirectIO == nullptr)
    {
        const int nDTSize = nDTSizeBits / 8;
        size_t nTempBufferForCommonDirectIOSize = static_cast<size_t>(
            m_nBlockXSize * nDTSize *
            (m_nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1));
        if (TIFFIsTiled(m_hTIFF))
            nTempBufferForCommonDirectIOSize *= m_nBlockYSize;

        m_pTempBufferForCommonDirectIO = static_cast<GByte *>(
            VSI_MALLOC_VERBOSE(nTempBufferForCommonDirectIOSize));
        if (m_pTempBufferForCommonDirectIO == nullptr)
            return CE_Failure;
    }
    FetchBufferVirtualMemIO oFetcher(pabySrcData, nMappingSize,
                                     m_pTempBufferForCommonDirectIO);

    return CommonDirectIO(oFetcher, nXOff, nYOff, nXSize, nYSize, pData,
                          nBufXSize, nBufYSize, eBufType, nBandCount,
                          panBandMap, nPixelSpace, nLineSpace, nBandSpace);
}

/************************************************************************/
/*                   CopyContigByteMultiBand()                          */
/************************************************************************/

static inline void CopyContigByteMultiBand(const GByte *CPL_RESTRICT pabySrc,
                                           int nSrcStride,
                                           GByte *CPL_RESTRICT pabyDest,
                                           int nDestStride, int nIters,
                                           int nBandCount)
{
    if (nBandCount == 3)
    {
        if (nSrcStride == 3 && nDestStride == 4)
        {
            while (nIters >= 8)
            {
                pabyDest[4 * 0 + 0] = pabySrc[3 * 0 + 0];
                pabyDest[4 * 0 + 1] = pabySrc[3 * 0 + 1];
                pabyDest[4 * 0 + 2] = pabySrc[3 * 0 + 2];
                pabyDest[4 * 1 + 0] = pabySrc[3 * 1 + 0];
                pabyDest[4 * 1 + 1] = pabySrc[3 * 1 + 1];
                pabyDest[4 * 1 + 2] = pabySrc[3 * 1 + 2];
                pabyDest[4 * 2 + 0] = pabySrc[3 * 2 + 0];
                pabyDest[4 * 2 + 1] = pabySrc[3 * 2 + 1];
                pabyDest[4 * 2 + 2] = pabySrc[3 * 2 + 2];
                pabyDest[4 * 3 + 0] = pabySrc[3 * 3 + 0];
                pabyDest[4 * 3 + 1] = pabySrc[3 * 3 + 1];
                pabyDest[4 * 3 + 2] = pabySrc[3 * 3 + 2];
                pabyDest[4 * 4 + 0] = pabySrc[3 * 4 + 0];
                pabyDest[4 * 4 + 1] = pabySrc[3 * 4 + 1];
                pabyDest[4 * 4 + 2] = pabySrc[3 * 4 + 2];
                pabyDest[4 * 5 + 0] = pabySrc[3 * 5 + 0];
                pabyDest[4 * 5 + 1] = pabySrc[3 * 5 + 1];
                pabyDest[4 * 5 + 2] = pabySrc[3 * 5 + 2];
                pabyDest[4 * 6 + 0] = pabySrc[3 * 6 + 0];
                pabyDest[4 * 6 + 1] = pabySrc[3 * 6 + 1];
                pabyDest[4 * 6 + 2] = pabySrc[3 * 6 + 2];
                pabyDest[4 * 7 + 0] = pabySrc[3 * 7 + 0];
                pabyDest[4 * 7 + 1] = pabySrc[3 * 7 + 1];
                pabyDest[4 * 7 + 2] = pabySrc[3 * 7 + 2];
                pabySrc += 3 * 8;
                pabyDest += 4 * 8;
                nIters -= 8;
            }
            while (nIters-- > 0)
            {
                pabyDest[0] = pabySrc[0];
                pabyDest[1] = pabySrc[1];
                pabyDest[2] = pabySrc[2];
                pabySrc += 3;
                pabyDest += 4;
            }
        }
        else
        {
            while (nIters-- > 0)
            {
                pabyDest[0] = pabySrc[0];
                pabyDest[1] = pabySrc[1];
                pabyDest[2] = pabySrc[2];
                pabySrc += nSrcStride;
                pabyDest += nDestStride;
            }
        }
    }
    else
    {
        while (nIters-- > 0)
        {
            for (int iBand = 0; iBand < nBandCount; ++iBand)
                pabyDest[iBand] = pabySrc[iBand];
            pabySrc += nSrcStride;
            pabyDest += nDestStride;
        }
    }
}

/************************************************************************/
/*                         CommonDirectIO()                             */
/************************************************************************/

// #define DEBUG_REACHED_VIRTUAL_MEM_IO
#ifdef DEBUG_REACHED_VIRTUAL_MEM_IO
static int anReachedVirtualMemIO[52] = {0};
#define REACHED(x) anReachedVirtualMemIO[x] = 1
#else
#define REACHED(x)
#endif

template <class FetchBuffer>
CPLErr GTiffDataset::CommonDirectIO(FetchBuffer &oFetcher, int nXOff, int nYOff,
                                    int nXSize, int nYSize, void *pData,
                                    int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType, int nBandCount,
                                    int *panBandMap, GSpacing nPixelSpace,
                                    GSpacing nLineSpace, GSpacing nBandSpace)
{
    const auto poFirstBand =
        cpl::down_cast<GTiffRasterBand *>(GetRasterBand(1));
    const GDALDataType eDataType = poFirstBand->GetRasterDataType();
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    const bool bIsComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eDataType));
    const int nBufDTSize = GDALGetDataTypeSizeBytes(eBufType);

    // Get strip offsets.
    toff_t *panOffsets = nullptr;
    if (!TIFFGetField(m_hTIFF,
                      (TIFFIsTiled(m_hTIFF)) ? TIFFTAG_TILEOFFSETS
                                             : TIFFTAG_STRIPOFFSETS,
                      &panOffsets) ||
        panOffsets == nullptr)
    {
        return CE_Failure;
    }

    bool bUseContigImplementation = m_nPlanarConfig == PLANARCONFIG_CONTIG &&
                                    nBandCount > 1 && nBandSpace == nBufDTSize;
    if (bUseContigImplementation)
    {
        for (int iBand = 0; iBand < nBandCount; ++iBand)
        {
            const int nBand = panBandMap[iBand];
            if (nBand != iBand + 1)
            {
                bUseContigImplementation = false;
                break;
            }
        }
    }

    const int nBandsPerBlock =
        m_nPlanarConfig == PLANARCONFIG_SEPARATE ? 1 : nBands;
    const int nBandsPerBlockDTSize = nBandsPerBlock * nDTSize;
    const bool bNoTypeChange = (eDataType == eBufType);
    const bool bNoXResampling = (nXSize == nBufXSize);
    const bool bNoXResamplingNoTypeChange = (bNoTypeChange && bNoXResampling);
    const bool bByteOnly = (bNoTypeChange && nDTSize == 1);
    const bool bByteNoXResampling = (bByteOnly && bNoXResamplingNoTypeChange);
    const bool bIsByteSwapped = CPL_TO_BOOL(TIFFIsByteSwapped(m_hTIFF));
    const double dfSrcXInc = nXSize / static_cast<double>(nBufXSize);
    const double dfSrcYInc = nYSize / static_cast<double>(nBufYSize);

    int bNoDataSetIn = FALSE;
    double dfNoData = poFirstBand->GetNoDataValue(&bNoDataSetIn);
    GByte abyNoData = 0;
    if (!bNoDataSetIn)
        dfNoData = 0;
    else if (dfNoData >= 0 && dfNoData <= 255)
        abyNoData = static_cast<GByte>(dfNoData + 0.5);

    // cppcheck-suppress knownConditionTrueFalse
    if (FetchBuffer::bMinimizeIO && TIFFIsTiled(m_hTIFF) && bNoXResampling &&
        (nYSize == nBufYSize) && m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        nBandCount > 1)
    {
        GByte *pabyData = static_cast<GByte *>(pData);
        for (int y = 0; y < nBufYSize;)
        {
            const int nSrcLine = nYOff + y;
            const int nBlockYOff = nSrcLine / m_nBlockYSize;
            const int nYOffsetInBlock = nSrcLine % m_nBlockYSize;
            const int nUsedBlockHeight =
                std::min(nBufYSize - y, m_nBlockYSize - nYOffsetInBlock);

            int nBlockXOff = nXOff / m_nBlockXSize;
            int nXOffsetInBlock = nXOff % m_nBlockXSize;
            int nBlockId = poFirstBand->ComputeBlockId(nBlockXOff, nBlockYOff);

            int x = 0;
            while (x < nBufXSize)
            {
                const toff_t nCurOffset = panOffsets[nBlockId];
                const int nUsedBlockWidth =
                    std::min(m_nBlockXSize - nXOffsetInBlock, nBufXSize - x);

                if (nCurOffset == 0)
                {
                    REACHED(30);
                    for (int k = 0; k < nUsedBlockHeight; ++k)
                    {
                        GByte *pabyLocalData =
                            pabyData + (y + k) * nLineSpace + x * nPixelSpace;
                        for (int iBand = 0; iBand < nBandCount; ++iBand)
                        {
                            GByte *pabyLocalDataBand =
                                pabyLocalData + iBand * nBandSpace;

                            GDALCopyWords(&dfNoData, GDT_Float64, 0,
                                          pabyLocalDataBand, eBufType,
                                          static_cast<int>(nPixelSpace),
                                          nUsedBlockWidth);
                        }
                    }
                }
                else
                {
                    const int nByteOffsetInBlock =
                        nYOffsetInBlock * m_nBlockXSize * nBandsPerBlockDTSize;
                    const GByte *pabyLocalSrcDataK0 = oFetcher.FetchBytes(
                        nCurOffset + nByteOffsetInBlock,
                        m_nBlockXSize * nUsedBlockHeight * nBandsPerBlock,
                        nDTSize, bIsByteSwapped, bIsComplex, nBlockId);
                    if (pabyLocalSrcDataK0 == nullptr)
                        return CE_Failure;

                    for (int k = 0; k < nUsedBlockHeight; ++k)
                    {
                        GByte *pabyLocalData =
                            pabyData + (y + k) * nLineSpace + x * nPixelSpace;
                        const GByte *pabyLocalSrcData =
                            pabyLocalSrcDataK0 +
                            (k * m_nBlockXSize + nXOffsetInBlock) *
                                nBandsPerBlockDTSize;

                        if (bUseContigImplementation && nBands == nBandCount &&
                            nPixelSpace == nBandsPerBlockDTSize)
                        {
                            REACHED(31);
                            GDALCopyWords(pabyLocalSrcData, eDataType, nDTSize,
                                          pabyLocalData, eBufType, nBufDTSize,
                                          nUsedBlockWidth * nBands);
                        }
                        else
                        {
                            REACHED(32);
                            for (int iBand = 0; iBand < nBandCount; ++iBand)
                            {
                                GByte *pabyLocalDataBand =
                                    pabyLocalData + iBand * nBandSpace;
                                const GByte *pabyLocalSrcDataBand =
                                    pabyLocalSrcData +
                                    (panBandMap[iBand] - 1) * nDTSize;

                                GDALCopyWords(pabyLocalSrcDataBand, eDataType,
                                              nBandsPerBlockDTSize,
                                              pabyLocalDataBand, eBufType,
                                              static_cast<int>(nPixelSpace),
                                              nUsedBlockWidth);
                            }
                        }
                    }
                }

                nXOffsetInBlock = 0;
                ++nBlockXOff;
                ++nBlockId;
                x += nUsedBlockWidth;
            }

            y += nUsedBlockHeight;
        }
    }
    // cppcheck-suppress knownConditionTrueFalse
    else if (FetchBuffer::bMinimizeIO && TIFFIsTiled(m_hTIFF) &&
             bNoXResampling && (nYSize == nBufYSize))
    // && (m_nPlanarConfig == PLANARCONFIG_SEPARATE || nBandCount == 1) )
    {
        for (int iBand = 0; iBand < nBandCount; ++iBand)
        {
            GByte *pabyData = static_cast<GByte *>(pData) + iBand * nBandSpace;
            const int nBand = panBandMap[iBand];
            auto poCurBand =
                cpl::down_cast<GTiffRasterBand *>(GetRasterBand(nBand));
            for (int y = 0; y < nBufYSize;)
            {
                const int nSrcLine = nYOff + y;
                const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                const int nUsedBlockHeight =
                    std::min(nBufYSize - y, m_nBlockYSize - nYOffsetIm_nBlock);

                int nBlockXOff = nXOff / m_nBlockXSize;
                int nXOffsetInBlock = nXOff % m_nBlockXSize;
                int nBlockId =
                    poCurBand->ComputeBlockId(nBlockXOff, m_nBlockYOff);

                int x = 0;
                while (x < nBufXSize)
                {
                    const toff_t nCurOffset = panOffsets[nBlockId];
                    const int nUsedBlockWidth = std::min(
                        m_nBlockXSize - nXOffsetInBlock, nBufXSize - x);

                    if (nCurOffset == 0)
                    {
                        REACHED(35);
                        for (int k = 0; k < nUsedBlockHeight; ++k)
                        {
                            GByte *pabyLocalData = pabyData +
                                                   (y + k) * nLineSpace +
                                                   x * nPixelSpace;

                            GDALCopyWords(&dfNoData, GDT_Float64, 0,
                                          pabyLocalData, eBufType,
                                          static_cast<int>(nPixelSpace),
                                          nUsedBlockWidth);
                        }
                    }
                    else
                    {
                        const int nByteOffsetIm_nBlock = nYOffsetIm_nBlock *
                                                         m_nBlockXSize *
                                                         nBandsPerBlockDTSize;
                        const GByte *pabyLocalSrcDataK0 = oFetcher.FetchBytes(
                            nCurOffset + nByteOffsetIm_nBlock,
                            m_nBlockXSize * nUsedBlockHeight * nBandsPerBlock,
                            nDTSize, bIsByteSwapped, bIsComplex, nBlockId);
                        if (pabyLocalSrcDataK0 == nullptr)
                            return CE_Failure;

                        if (m_nPlanarConfig == PLANARCONFIG_CONTIG)
                        {
                            REACHED(36);
                            pabyLocalSrcDataK0 += (nBand - 1) * nDTSize;
                        }
                        else
                        {
                            REACHED(37);
                        }

                        for (int k = 0; k < nUsedBlockHeight; ++k)
                        {
                            GByte *pabyLocalData = pabyData +
                                                   (y + k) * nLineSpace +
                                                   x * nPixelSpace;
                            const GByte *pabyLocalSrcData =
                                pabyLocalSrcDataK0 +
                                (k * m_nBlockXSize + nXOffsetInBlock) *
                                    nBandsPerBlockDTSize;

                            GDALCopyWords(
                                pabyLocalSrcData, eDataType,
                                nBandsPerBlockDTSize, pabyLocalData, eBufType,
                                static_cast<int>(nPixelSpace), nUsedBlockWidth);
                        }
                    }

                    nXOffsetInBlock = 0;
                    ++nBlockXOff;
                    ++nBlockId;
                    x += nUsedBlockWidth;
                }

                y += nUsedBlockHeight;
            }
        }
    }
    // cppcheck-suppress knownConditionTrueFalse
    else if (FetchBuffer::bMinimizeIO && TIFFIsTiled(m_hTIFF) &&
             m_nPlanarConfig == PLANARCONFIG_CONTIG && nBandCount > 1)
    {
        GByte *pabyData = static_cast<GByte *>(pData);
        int anSrcYOffset[256] = {0};
        for (int y = 0; y < nBufYSize;)
        {
            const double dfYOffStart = nYOff + (y + 0.5) * dfSrcYInc;
            const int nSrcLine = static_cast<int>(dfYOffStart);
            const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
            const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
            const int nBaseByteOffsetIm_nBlock =
                nYOffsetIm_nBlock * m_nBlockXSize * nBandsPerBlockDTSize;
            int ychunk = 1;
            int nLastSrcLineK = nSrcLine;
            anSrcYOffset[0] = 0;
            for (int k = 1; k < nBufYSize - y; ++k)
            {
                int nSrcLineK =
                    nYOff + static_cast<int>((y + k + 0.5) * dfSrcYInc);
                const int nBlockYOffK = nSrcLineK / m_nBlockYSize;
                if (k < 256)
                    anSrcYOffset[k] =
                        ((nSrcLineK % m_nBlockYSize) - nYOffsetIm_nBlock) *
                        m_nBlockXSize * nBandsPerBlockDTSize;
                if (nBlockYOffK != m_nBlockYOff)
                {
                    break;
                }
                ++ychunk;
                nLastSrcLineK = nSrcLineK;
            }
            const int nUsedBlockHeight = nLastSrcLineK - nSrcLine + 1;
            // CPLAssert(nUsedBlockHeight <= m_nBlockYSize);

            double dfSrcX = nXOff + 0.5 * dfSrcXInc;
            int nCurBlockXOff = 0;
            int nNextBlockXOff = 0;
            toff_t nCurOffset = 0;
            const GByte *pabyLocalSrcDataStartLine = nullptr;
            for (int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc)
            {
                const int nSrcPixel = static_cast<int>(dfSrcX);
                if (nSrcPixel >= nNextBlockXOff)
                {
                    const int nBlockXOff = nSrcPixel / m_nBlockXSize;
                    nCurBlockXOff = nBlockXOff * m_nBlockXSize;
                    nNextBlockXOff = nCurBlockXOff + m_nBlockXSize;
                    const int nBlockId =
                        poFirstBand->ComputeBlockId(nBlockXOff, m_nBlockYOff);
                    nCurOffset = panOffsets[nBlockId];
                    if (nCurOffset != 0)
                    {
                        pabyLocalSrcDataStartLine = oFetcher.FetchBytes(
                            nCurOffset + nBaseByteOffsetIm_nBlock,
                            m_nBlockXSize * nBandsPerBlock * nUsedBlockHeight,
                            nDTSize, bIsByteSwapped, bIsComplex, nBlockId);
                        if (pabyLocalSrcDataStartLine == nullptr)
                            return CE_Failure;
                    }
                }

                if (nCurOffset == 0)
                {
                    REACHED(38);

                    for (int k = 0; k < ychunk; ++k)
                    {
                        GByte *const pabyLocalData =
                            pabyData + (y + k) * nLineSpace + x * nPixelSpace;
                        for (int iBand = 0; iBand < nBandCount; ++iBand)
                        {
                            GDALCopyWords(&dfNoData, GDT_Float64, 0,
                                          pabyLocalData + nBandSpace * iBand,
                                          eBufType, 0, 1);
                        }
                    }
                }
                else
                {
                    const int nXOffsetInBlock = nSrcPixel - nCurBlockXOff;
                    double dfYOff = dfYOffStart;
                    const GByte *const pabyLocalSrcDataK0 =
                        pabyLocalSrcDataStartLine +
                        nXOffsetInBlock * nBandsPerBlockDTSize;
                    GByte *pabyLocalData =
                        pabyData + y * nLineSpace + x * nPixelSpace;
                    for (int k = 0; k < ychunk;
                         ++k, pabyLocalData += nLineSpace)
                    {
                        const GByte *pabyLocalSrcData = nullptr;
                        if (ychunk <= 256)
                        {
                            REACHED(39);
                            pabyLocalSrcData =
                                pabyLocalSrcDataK0 + anSrcYOffset[k];
                        }
                        else
                        {
                            REACHED(40);
                            const int nYOffsetIm_nBlockK =
                                static_cast<int>(dfYOff) % m_nBlockYSize;
                            // CPLAssert(
                            //     nYOffsetIm_nBlockK - nYOffsetIm_nBlock <=
                            //     nUsedBlockHeight);
                            pabyLocalSrcData =
                                pabyLocalSrcDataK0 +
                                (nYOffsetIm_nBlockK - nYOffsetIm_nBlock) *
                                    m_nBlockXSize * nBandsPerBlockDTSize;
                            dfYOff += dfSrcYInc;
                        }

                        if (bByteOnly)
                        {
                            REACHED(41);
                            for (int iBand = 0; iBand < nBandCount; ++iBand)
                            {
                                GByte *pabyLocalDataBand =
                                    pabyLocalData + iBand * nBandSpace;
                                const GByte *pabyLocalSrcDataBand =
                                    pabyLocalSrcData + (panBandMap[iBand] - 1);
                                *pabyLocalDataBand = *pabyLocalSrcDataBand;
                            }
                        }
                        else
                        {
                            REACHED(42);
                            for (int iBand = 0; iBand < nBandCount; ++iBand)
                            {
                                GByte *pabyLocalDataBand =
                                    pabyLocalData + iBand * nBandSpace;
                                const GByte *pabyLocalSrcDataBand =
                                    pabyLocalSrcData +
                                    (panBandMap[iBand] - 1) * nDTSize;

                                GDALCopyWords(pabyLocalSrcDataBand, eDataType,
                                              0, pabyLocalDataBand, eBufType, 0,
                                              1);
                            }
                        }
                    }
                }
            }

            y += ychunk;
        }
    }
    // cppcheck-suppress knownConditionTrueFalse
    else if (FetchBuffer::bMinimizeIO && TIFFIsTiled(m_hTIFF))
    // && (m_nPlanarConfig == PLANARCONFIG_SEPARATE || nBandCount == 1) )
    {
        for (int iBand = 0; iBand < nBandCount; ++iBand)
        {
            GByte *pabyData = static_cast<GByte *>(pData) + iBand * nBandSpace;
            const int nBand = panBandMap[iBand];
            auto poCurBand =
                cpl::down_cast<GTiffRasterBand *>(GetRasterBand(nBand));
            int anSrcYOffset[256] = {0};
            for (int y = 0; y < nBufYSize;)
            {
                const double dfYOffStart = nYOff + (y + 0.5) * dfSrcYInc;
                const int nSrcLine = static_cast<int>(dfYOffStart);
                const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                const int nBaseByteOffsetIm_nBlock =
                    nYOffsetIm_nBlock * m_nBlockXSize * nBandsPerBlockDTSize;
                int ychunk = 1;
                int nLastSrcLineK = nSrcLine;
                anSrcYOffset[0] = 0;
                for (int k = 1; k < nBufYSize - y; ++k)
                {
                    const int nSrcLineK =
                        nYOff + static_cast<int>((y + k + 0.5) * dfSrcYInc);
                    const int nBlockYOffK = nSrcLineK / m_nBlockYSize;
                    if (k < 256)
                        anSrcYOffset[k] =
                            ((nSrcLineK % m_nBlockYSize) - nYOffsetIm_nBlock) *
                            m_nBlockXSize * nBandsPerBlockDTSize;
                    if (nBlockYOffK != m_nBlockYOff)
                    {
                        break;
                    }
                    ++ychunk;
                    nLastSrcLineK = nSrcLineK;
                }
                const int nUsedBlockHeight = nLastSrcLineK - nSrcLine + 1;
                // CPLAssert(nUsedBlockHeight <= m_nBlockYSize);

                double dfSrcX = nXOff + 0.5 * dfSrcXInc;
                int nCurBlockXOff = 0;
                int nNextBlockXOff = 0;
                toff_t nCurOffset = 0;
                const GByte *pabyLocalSrcDataStartLine = nullptr;
                for (int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc)
                {
                    int nSrcPixel = static_cast<int>(dfSrcX);
                    if (nSrcPixel >= nNextBlockXOff)
                    {
                        const int nBlockXOff = nSrcPixel / m_nBlockXSize;
                        nCurBlockXOff = nBlockXOff * m_nBlockXSize;
                        nNextBlockXOff = nCurBlockXOff + m_nBlockXSize;
                        const int nBlockId =
                            poCurBand->ComputeBlockId(nBlockXOff, m_nBlockYOff);
                        nCurOffset = panOffsets[nBlockId];
                        if (nCurOffset != 0)
                        {
                            pabyLocalSrcDataStartLine = oFetcher.FetchBytes(
                                nCurOffset + nBaseByteOffsetIm_nBlock,
                                m_nBlockXSize * nBandsPerBlock *
                                    nUsedBlockHeight,
                                nDTSize, bIsByteSwapped, bIsComplex, nBlockId);
                            if (pabyLocalSrcDataStartLine == nullptr)
                                return CE_Failure;

                            if (m_nPlanarConfig == PLANARCONFIG_CONTIG)
                            {
                                REACHED(45);
                                pabyLocalSrcDataStartLine +=
                                    (nBand - 1) * nDTSize;
                            }
                            else
                            {
                                REACHED(46);
                            }
                        }
                    }

                    if (nCurOffset == 0)
                    {
                        REACHED(47);

                        for (int k = 0; k < ychunk; ++k)
                        {
                            GByte *const pabyLocalData = pabyData +
                                                         (y + k) * nLineSpace +
                                                         x * nPixelSpace;

                            GDALCopyWords(&dfNoData, GDT_Float64, 0,
                                          pabyLocalData, eBufType, 0, 1);
                        }
                    }
                    else
                    {
                        const int nXOffsetInBlock = nSrcPixel - nCurBlockXOff;
                        double dfYOff = dfYOffStart;
                        const GByte *const pabyLocalSrcDataK0 =
                            pabyLocalSrcDataStartLine +
                            nXOffsetInBlock * nBandsPerBlockDTSize;
                        GByte *pabyLocalData =
                            pabyData + y * nLineSpace + x * nPixelSpace;
                        for (int k = 0; k < ychunk;
                             ++k, pabyLocalData += nLineSpace)
                        {
                            const GByte *pabyLocalSrcData = nullptr;
                            if (ychunk <= 256)
                            {
                                REACHED(48);
                                pabyLocalSrcData =
                                    pabyLocalSrcDataK0 + anSrcYOffset[k];
                            }
                            else
                            {
                                REACHED(49);
                                const int nYOffsetIm_nBlockK =
                                    static_cast<int>(dfYOff) % m_nBlockYSize;
                                // CPLAssert(
                                //     nYOffsetIm_nBlockK - nYOffsetIm_nBlock <=
                                //     nUsedBlockHeight);
                                pabyLocalSrcData =
                                    pabyLocalSrcDataK0 +
                                    (nYOffsetIm_nBlockK - nYOffsetIm_nBlock) *
                                        m_nBlockXSize * nBandsPerBlockDTSize;
                                dfYOff += dfSrcYInc;
                            }

                            if (bByteOnly)
                            {
                                REACHED(50);

                                *pabyLocalData = *pabyLocalSrcData;
                            }
                            else
                            {
                                REACHED(51);

                                GDALCopyWords(pabyLocalSrcData, eDataType, 0,
                                              pabyLocalData, eBufType, 0, 1);
                            }
                        }
                    }
                }

                y += ychunk;
            }
        }
    }
    else if (bUseContigImplementation)
    {
        // cppcheck-suppress knownConditionTrueFalse
        if (!FetchBuffer::bMinimizeIO && TIFFIsTiled(m_hTIFF))
        {
            GByte *pabyData = static_cast<GByte *>(pData);
            for (int y = 0; y < nBufYSize; ++y)
            {
                const int nSrcLine =
                    nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                const int nBaseByteOffsetIm_nBlock =
                    nYOffsetIm_nBlock * m_nBlockXSize * nBandsPerBlockDTSize;

                if (bNoXResampling)
                {
                    GByte *pabyLocalData = pabyData + y * nLineSpace;
                    int nBlockXOff = nXOff / m_nBlockXSize;
                    int nXOffsetInBlock = nXOff % m_nBlockXSize;
                    int nBlockId =
                        poFirstBand->ComputeBlockId(nBlockXOff, m_nBlockYOff);

                    int x = 0;
                    while (x < nBufXSize)
                    {
                        const int nByteOffsetIm_nBlock =
                            nBaseByteOffsetIm_nBlock +
                            nXOffsetInBlock * nBandsPerBlockDTSize;
                        const toff_t nCurOffset = panOffsets[nBlockId];
                        const int nUsedBlockWidth = std::min(
                            m_nBlockXSize - nXOffsetInBlock, nBufXSize - x);

                        int nIters = nUsedBlockWidth;
                        if (nCurOffset == 0)
                        {
                            if (bByteNoXResampling)
                            {
                                REACHED(0);
                                while (nIters-- > 0)
                                {
                                    for (int iBand = 0; iBand < nBandCount;
                                         ++iBand)
                                    {
                                        pabyLocalData[iBand] = abyNoData;
                                    }
                                    pabyLocalData += nPixelSpace;
                                }
                            }
                            else
                            {
                                REACHED(1);
                                while (nIters-- > 0)
                                {
                                    GDALCopyWords(&dfNoData, GDT_Float64, 0,
                                                  pabyLocalData, eBufType,
                                                  static_cast<int>(nBandSpace),
                                                  nBandCount);
                                    pabyLocalData += nPixelSpace;
                                }
                            }
                        }
                        else
                        {
                            if (bNoTypeChange && nBands == nBandCount &&
                                nPixelSpace == nBandsPerBlockDTSize)
                            {
                                REACHED(2);
                                if (!oFetcher.FetchBytes(
                                        pabyLocalData,
                                        nCurOffset + nByteOffsetIm_nBlock,
                                        nIters * nBandsPerBlock, nDTSize,
                                        bIsByteSwapped, bIsComplex, nBlockId))
                                {
                                    return CE_Failure;
                                }
                                pabyLocalData +=
                                    nIters * nBandsPerBlock * nDTSize;
                            }
                            else
                            {
                                const GByte *pabyLocalSrcData =
                                    oFetcher.FetchBytes(
                                        nCurOffset + nByteOffsetIm_nBlock,
                                        nIters * nBandsPerBlock, nDTSize,
                                        bIsByteSwapped, bIsComplex, nBlockId);
                                if (pabyLocalSrcData == nullptr)
                                    return CE_Failure;
                                if (bByteNoXResampling)
                                {
                                    REACHED(3);
                                    CopyContigByteMultiBand(
                                        pabyLocalSrcData, nBandsPerBlockDTSize,
                                        pabyLocalData,
                                        static_cast<int>(nPixelSpace), nIters,
                                        nBandCount);
                                    pabyLocalData += nIters * nPixelSpace;
                                }
                                else
                                {
                                    REACHED(4);
                                    while (nIters-- > 0)
                                    {
                                        GDALCopyWords(
                                            pabyLocalSrcData, eDataType,
                                            nDTSize, pabyLocalData, eBufType,
                                            static_cast<int>(nBandSpace),
                                            nBandCount);
                                        pabyLocalSrcData +=
                                            nBandsPerBlockDTSize;
                                        pabyLocalData += nPixelSpace;
                                    }
                                }
                            }
                        }

                        nXOffsetInBlock = 0;
                        ++nBlockXOff;
                        ++nBlockId;
                        x += nUsedBlockWidth;
                    }
                }
                else  // Contig, tiled, potential resampling & data type change.
                {
                    const GByte *pabyLocalSrcDataStartLine = nullptr;
                    GByte *pabyLocalData = pabyData + y * nLineSpace;
                    double dfSrcX = nXOff + 0.5 * dfSrcXInc;
                    int nCurBlockXOff = 0;
                    int nNextBlockXOff = 0;
                    toff_t nCurOffset = 0;
                    for (int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc)
                    {
                        int nSrcPixel = static_cast<int>(dfSrcX);
                        if (nSrcPixel >= nNextBlockXOff)
                        {
                            const int nBlockXOff = nSrcPixel / m_nBlockXSize;
                            nCurBlockXOff = nBlockXOff * m_nBlockXSize;
                            nNextBlockXOff = nCurBlockXOff + m_nBlockXSize;
                            const int nBlockId = poFirstBand->ComputeBlockId(
                                nBlockXOff, m_nBlockYOff);
                            nCurOffset = panOffsets[nBlockId];
                            if (nCurOffset != 0)
                            {
                                pabyLocalSrcDataStartLine = oFetcher.FetchBytes(
                                    nCurOffset + nBaseByteOffsetIm_nBlock,
                                    m_nBlockXSize * nBandsPerBlock, nDTSize,
                                    bIsByteSwapped, bIsComplex, nBlockId);
                                if (pabyLocalSrcDataStartLine == nullptr)
                                    return CE_Failure;
                            }
                        }
                        const int nXOffsetInBlock = nSrcPixel - nCurBlockXOff;

                        if (nCurOffset == 0)
                        {
                            REACHED(5);
                            GDALCopyWords(&dfNoData, GDT_Float64, 0,
                                          pabyLocalData, eBufType,
                                          static_cast<int>(nBandSpace),
                                          nBandCount);
                            pabyLocalData += nPixelSpace;
                        }
                        else
                        {
                            const GByte *pabyLocalSrcData =
                                pabyLocalSrcDataStartLine +
                                nXOffsetInBlock * nBandsPerBlockDTSize;

                            REACHED(6);
                            if (bByteOnly)
                            {
                                for (int iBand = 0; iBand < nBands; ++iBand)
                                    pabyLocalData[iBand] =
                                        pabyLocalSrcData[iBand];
                            }
                            else
                            {
                                GDALCopyWords(pabyLocalSrcData, eDataType,
                                              nDTSize, pabyLocalData, eBufType,
                                              static_cast<int>(nBandSpace),
                                              nBandCount);
                            }
                            pabyLocalData += nPixelSpace;
                        }
                    }
                }
            }
        }
        else  // Contig, striped organized.
        {
            GByte *pabyData = static_cast<GByte *>(pData);
            for (int y = 0; y < nBufYSize; ++y)
            {
                const int nSrcLine =
                    nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                const int nBlockId = m_nBlockYOff;
                const toff_t nCurOffset = panOffsets[nBlockId];
                if (nCurOffset == 0)
                {
                    REACHED(7);
                    for (int x = 0; x < nBufXSize; ++x)
                    {
                        GDALCopyWords(
                            &dfNoData, GDT_Float64, 0,
                            pabyData + y * nLineSpace + x * nPixelSpace,
                            eBufType, static_cast<int>(nBandSpace), nBandCount);
                    }
                }
                else
                {
                    GByte *pabyLocalData = pabyData + y * nLineSpace;
                    const int nBaseByteOffsetIm_nBlock =
                        (nYOffsetIm_nBlock * m_nBlockXSize + nXOff) *
                        nBandsPerBlockDTSize;

                    if (bNoXResamplingNoTypeChange && nBands == nBandCount &&
                        nPixelSpace == nBandsPerBlockDTSize)
                    {
                        REACHED(8);
                        if (!oFetcher.FetchBytes(
                                pabyLocalData,
                                nCurOffset + nBaseByteOffsetIm_nBlock,
                                nXSize * nBandsPerBlock, nDTSize,
                                bIsByteSwapped, bIsComplex, nBlockId))
                        {
                            return CE_Failure;
                        }
                    }
                    else
                    {
                        const GByte *pabyLocalSrcData = oFetcher.FetchBytes(
                            nCurOffset + nBaseByteOffsetIm_nBlock,
                            nXSize * nBandsPerBlock, nDTSize, bIsByteSwapped,
                            bIsComplex, nBlockId);
                        if (pabyLocalSrcData == nullptr)
                            return CE_Failure;

                        if (bByteNoXResampling)
                        {
                            REACHED(9);
                            CopyContigByteMultiBand(
                                pabyLocalSrcData, nBandsPerBlockDTSize,
                                pabyLocalData, static_cast<int>(nPixelSpace),
                                nBufXSize, nBandCount);
                        }
                        else if (bByteOnly)
                        {
                            REACHED(10);
                            double dfSrcX = 0.5 * dfSrcXInc;
                            for (int x = 0; x < nBufXSize;
                                 ++x, dfSrcX += dfSrcXInc)
                            {
                                const int nSrcPixelMinusXOff =
                                    static_cast<int>(dfSrcX);
                                for (int iBand = 0; iBand < nBandCount; ++iBand)
                                {
                                    pabyLocalData[x * nPixelSpace + iBand] =
                                        pabyLocalSrcData
                                            [nSrcPixelMinusXOff *
                                                 nBandsPerBlockDTSize +
                                             iBand];
                                }
                            }
                        }
                        else
                        {
                            REACHED(11);
                            double dfSrcX = 0.5 * dfSrcXInc;
                            for (int x = 0; x < nBufXSize;
                                 ++x, dfSrcX += dfSrcXInc)
                            {
                                int nSrcPixelMinusXOff =
                                    static_cast<int>(dfSrcX);
                                GDALCopyWords(
                                    pabyLocalSrcData + nSrcPixelMinusXOff *
                                                           nBandsPerBlockDTSize,
                                    eDataType, nDTSize,
                                    pabyLocalData + x * nPixelSpace, eBufType,
                                    static_cast<int>(nBandSpace), nBandCount);
                            }
                        }
                    }
                }
            }
        }
    }
    else  // Non-contig reading case.
    {
        // cppcheck-suppress knownConditionTrueFalse
        if (!FetchBuffer::bMinimizeIO && TIFFIsTiled(m_hTIFF))
        {
            for (int iBand = 0; iBand < nBandCount; ++iBand)
            {
                const int nBand = panBandMap[iBand];
                auto poCurBand =
                    cpl::down_cast<GTiffRasterBand *>(GetRasterBand(nBand));
                GByte *const pabyData =
                    static_cast<GByte *>(pData) + iBand * nBandSpace;
                for (int y = 0; y < nBufYSize; ++y)
                {
                    const int nSrcLine =
                        nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                    const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                    const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;

                    int nBaseByteOffsetIm_nBlock = nYOffsetIm_nBlock *
                                                   m_nBlockXSize *
                                                   nBandsPerBlockDTSize;
                    if (m_nPlanarConfig == PLANARCONFIG_CONTIG)
                    {
                        REACHED(12);
                        nBaseByteOffsetIm_nBlock += (nBand - 1) * nDTSize;
                    }
                    else
                    {
                        REACHED(13);
                    }

                    if (bNoXResampling)
                    {
                        GByte *pabyLocalData = pabyData + y * nLineSpace;
                        int nBlockXOff = nXOff / m_nBlockXSize;
                        int nBlockId =
                            poCurBand->ComputeBlockId(nBlockXOff, m_nBlockYOff);
                        int nXOffsetInBlock = nXOff % m_nBlockXSize;

                        int x = 0;
                        while (x < nBufXSize)
                        {
                            const int nByteOffsetIm_nBlock =
                                nBaseByteOffsetIm_nBlock +
                                nXOffsetInBlock * nBandsPerBlockDTSize;
                            const toff_t nCurOffset = panOffsets[nBlockId];
                            const int nUsedBlockWidth = std::min(
                                m_nBlockXSize - nXOffsetInBlock, nBufXSize - x);
                            int nIters = nUsedBlockWidth;

                            if (nCurOffset == 0)
                            {
                                REACHED(16);
                                GDALCopyWords(&dfNoData, GDT_Float64, 0,
                                              pabyLocalData, eBufType,
                                              static_cast<int>(nPixelSpace),
                                              nIters);
                                pabyLocalData += nIters * nPixelSpace;
                            }
                            else
                            {
                                if (bNoTypeChange &&
                                    nPixelSpace == nBandsPerBlockDTSize)
                                {
                                    REACHED(17);
                                    if (!oFetcher.FetchBytes(
                                            pabyLocalData,
                                            nCurOffset + nByteOffsetIm_nBlock,
                                            (nIters - 1) * nBandsPerBlock + 1,
                                            nDTSize, bIsByteSwapped, bIsComplex,
                                            nBlockId))
                                    {
                                        return CE_Failure;
                                    }
                                    pabyLocalData += nIters * nPixelSpace;
                                }
                                else
                                {
                                    const GByte *pabyLocalSrcData =
                                        oFetcher.FetchBytes(
                                            nCurOffset + nByteOffsetIm_nBlock,
                                            (nIters - 1) * nBandsPerBlock + 1,
                                            nDTSize, bIsByteSwapped, bIsComplex,
                                            nBlockId);
                                    if (pabyLocalSrcData == nullptr)
                                        return CE_Failure;

                                    REACHED(18);
                                    GDALCopyWords(pabyLocalSrcData, eDataType,
                                                  nBandsPerBlockDTSize,
                                                  pabyLocalData, eBufType,
                                                  static_cast<int>(nPixelSpace),
                                                  nIters);
                                    pabyLocalData += nIters * nPixelSpace;
                                }
                            }

                            nXOffsetInBlock = 0;
                            ++nBlockXOff;
                            ++nBlockId;
                            x += nUsedBlockWidth;
                        }
                    }
                    else
                    {
                        // Non-contig reading, tiled, potential resampling and
                        // data type change.

                        const GByte *pabyLocalSrcDataStartLine = nullptr;
                        GByte *pabyLocalData = pabyData + y * nLineSpace;
                        double dfSrcX = nXOff + 0.5 * dfSrcXInc;
                        int nCurBlockXOff = 0;
                        int nNextBlockXOff = 0;
                        toff_t nCurOffset = 0;
                        for (int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc)
                        {
                            const int nSrcPixel = static_cast<int>(dfSrcX);
                            if (nSrcPixel >= nNextBlockXOff)
                            {
                                const int nBlockXOff =
                                    nSrcPixel / m_nBlockXSize;
                                nCurBlockXOff = nBlockXOff * m_nBlockXSize;
                                nNextBlockXOff = nCurBlockXOff + m_nBlockXSize;
                                const int nBlockId = poCurBand->ComputeBlockId(
                                    nBlockXOff, m_nBlockYOff);
                                nCurOffset = panOffsets[nBlockId];
                                if (nCurOffset != 0)
                                {
                                    pabyLocalSrcDataStartLine =
                                        oFetcher.FetchBytes(
                                            nCurOffset +
                                                nBaseByteOffsetIm_nBlock,
                                            m_nBlockXSize * nBandsPerBlock,
                                            nDTSize, bIsByteSwapped, bIsComplex,
                                            nBlockId);
                                    if (pabyLocalSrcDataStartLine == nullptr)
                                        return CE_Failure;
                                }
                            }
                            const int nXOffsetInBlock =
                                nSrcPixel - nCurBlockXOff;

                            if (nCurOffset == 0)
                            {
                                REACHED(21);
                                GDALCopyWords(&dfNoData, GDT_Float64, 0,
                                              pabyLocalData, eBufType, 0, 1);
                                pabyLocalData += nPixelSpace;
                            }
                            else
                            {
                                const GByte *pabyLocalSrcData =
                                    pabyLocalSrcDataStartLine +
                                    nXOffsetInBlock * nBandsPerBlockDTSize;

                                REACHED(22);
                                if (bByteOnly)
                                {
                                    *pabyLocalData = *pabyLocalSrcData;
                                }
                                else
                                {
                                    GDALCopyWords(pabyLocalSrcData, eDataType,
                                                  0, pabyLocalData, eBufType, 0,
                                                  1);
                                }
                                pabyLocalData += nPixelSpace;
                            }
                        }
                    }
                }
            }
        }
        else  // Non-contig reading, striped.
        {
            for (int iBand = 0; iBand < nBandCount; ++iBand)
            {
                const int nBand = panBandMap[iBand];
                GByte *pabyData =
                    static_cast<GByte *>(pData) + iBand * nBandSpace;
                for (int y = 0; y < nBufYSize; ++y)
                {
                    const int nSrcLine =
                        nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                    const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                    const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                    int nBlockId = m_nBlockYOff;
                    if (m_nPlanarConfig == PLANARCONFIG_SEPARATE)
                    {
                        REACHED(23);
                        nBlockId += m_nBlocksPerBand * (nBand - 1);
                    }
                    else
                    {
                        REACHED(24);
                    }
                    const toff_t nCurOffset = panOffsets[nBlockId];
                    if (nCurOffset == 0)
                    {
                        REACHED(25);
                        GDALCopyWords(&dfNoData, GDT_Float64, 0,
                                      pabyData + y * nLineSpace, eBufType,
                                      static_cast<int>(nPixelSpace), nBufXSize);
                    }
                    else
                    {
                        int nBaseByteOffsetIm_nBlock =
                            (nYOffsetIm_nBlock * m_nBlockXSize + nXOff) *
                            nBandsPerBlockDTSize;
                        if (m_nPlanarConfig == PLANARCONFIG_CONTIG)
                            nBaseByteOffsetIm_nBlock += (nBand - 1) * nDTSize;

                        GByte *pabyLocalData = pabyData + y * nLineSpace;
                        if (bNoXResamplingNoTypeChange &&
                            nPixelSpace == nBandsPerBlockDTSize)
                        {
                            REACHED(26);
                            if (!oFetcher.FetchBytes(
                                    pabyLocalData,
                                    nCurOffset + nBaseByteOffsetIm_nBlock,
                                    (nXSize - 1) * nBandsPerBlock + 1, nDTSize,
                                    bIsByteSwapped, bIsComplex, nBlockId))
                            {
                                return CE_Failure;
                            }
                        }
                        else
                        {
                            const GByte *pabyLocalSrcData = oFetcher.FetchBytes(
                                nCurOffset + nBaseByteOffsetIm_nBlock,
                                (nXSize - 1) * nBandsPerBlock + 1, nDTSize,
                                bIsByteSwapped, bIsComplex, nBlockId);
                            if (pabyLocalSrcData == nullptr)
                                return CE_Failure;

                            if (bNoXResamplingNoTypeChange)
                            {
                                REACHED(27);
                                GDALCopyWords(pabyLocalSrcData, eDataType,
                                              nBandsPerBlockDTSize,
                                              pabyLocalData, eBufType,
                                              static_cast<int>(nPixelSpace),
                                              nBufXSize);
                            }
                            else if (bByteOnly)
                            {
                                REACHED(28);
                                double dfSrcX = 0.5 * dfSrcXInc;
                                for (int x = 0; x < nBufXSize;
                                     ++x, dfSrcX += dfSrcXInc)
                                {
                                    const int nSrcPixelMinusXOff =
                                        static_cast<int>(dfSrcX);
                                    pabyLocalData[x * nPixelSpace] =
                                        pabyLocalSrcData[nSrcPixelMinusXOff *
                                                         nBandsPerBlockDTSize];
                                }
                            }
                            else
                            {
                                REACHED(29);
                                double dfSrcX = 0.5 * dfSrcXInc;
                                for (int x = 0; x < nBufXSize;
                                     ++x, dfSrcX += dfSrcXInc)
                                {
                                    const int nSrcPixelMinusXOff =
                                        static_cast<int>(dfSrcX);
                                    GDALCopyWords(pabyLocalSrcData +
                                                      nSrcPixelMinusXOff *
                                                          nBandsPerBlockDTSize,
                                                  eDataType, 0,
                                                  pabyLocalData +
                                                      x * nPixelSpace,
                                                  eBufType, 0, 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                           DirectIO()                                 */
/************************************************************************/

CPLErr GTiffDataset::CommonDirectIOClassic(
    FetchBufferDirectIO &oFetcher, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, int *panBandMap, GSpacing nPixelSpace, GSpacing nLineSpace,
    GSpacing nBandSpace)
{
    return CommonDirectIO<FetchBufferDirectIO>(
        oFetcher, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace);
}

/************************************************************************/
/*                           DirectIO()                                 */
/************************************************************************/

// Reads directly bytes from the file using ReadMultiRange(), and by-pass
// block reading. Restricted to simple TIFF configurations
// (uncompressed data, standard data types). Particularly useful to extract
// sub-windows of data on a large /vsicurl dataset).
// Returns -1 if DirectIO() can't be supported on that file.

int GTiffDataset::DirectIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                           int nYSize, void *pData, int nBufXSize,
                           int nBufYSize, GDALDataType eBufType, int nBandCount,
                           int *panBandMap, GSpacing nPixelSpace,
                           GSpacing nLineSpace, GSpacing nBandSpace,
                           GDALRasterIOExtraArg *psExtraArg)
{
    auto poProtoBand = cpl::down_cast<GTiffRasterBand *>(papoBands[0]);
    const GDALDataType eDataType = poProtoBand->GetRasterDataType();
    const int nDTSizeBits = GDALGetDataTypeSizeBits(eDataType);
    if (!(eRWFlag == GF_Read && m_nCompression == COMPRESSION_NONE &&
          (m_nPhotometric == PHOTOMETRIC_MINISBLACK ||
           m_nPhotometric == PHOTOMETRIC_RGB ||
           m_nPhotometric == PHOTOMETRIC_PALETTE) &&
          poProtoBand->IsBaseGTiffClass()))
    {
        return -1;
    }
    Crystalize();

    // Only know how to deal with nearest neighbour in this optimized routine.
    if ((nXSize != nBufXSize || nYSize != nBufYSize) && psExtraArg != nullptr &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
    {
        return -1;
    }

    // If the file is band interleave or only one band is requested, then
    // fallback to band DirectIO.
    bool bUseBandRasterIO = false;
    if (m_nPlanarConfig == PLANARCONFIG_SEPARATE || nBandCount == 1)
    {
        bUseBandRasterIO = true;
    }
    else
    {
        // For simplicity, only deals with "naturally ordered" bands.
        for (int iBand = 0; iBand < nBandCount; ++iBand)
        {
            if (panBandMap[iBand] != iBand + 1)
            {
                bUseBandRasterIO = true;
                break;
            }
        }
    }
    if (bUseBandRasterIO)
    {
        CPLErr eErr = CE_None;
        for (int iBand = 0; eErr == CE_None && iBand < nBandCount; ++iBand)
        {
            eErr =
                GetRasterBand(panBandMap[iBand])
                    ->RasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                               static_cast<GByte *>(pData) + iBand * nBandSpace,
                               nBufXSize, nBufYSize, eBufType, nPixelSpace,
                               nLineSpace, psExtraArg);
        }
        return eErr;
    }

#if DEBUG_VERBOSE
    CPLDebug("GTiff", "DirectIO(%d,%d,%d,%d -> %dx%d)", nXOff, nYOff, nXSize,
             nYSize, nBufXSize, nBufYSize);
#endif

    // No need to look if overviews can satisfy the request as it has already */
    // been done in GTiffDataset::IRasterIO().

    // Make sure that TIFFTAG_STRIPOFFSETS is up-to-date.
    if (eAccess == GA_Update)
    {
        FlushCache(false);
        VSI_TIFFFlushBufferedWrite(TIFFClientdata(m_hTIFF));
    }

    if (TIFFIsTiled(m_hTIFF))
    {
        const int nDTSize = nDTSizeBits / 8;
        const size_t nTempBufferForCommonDirectIOSize = static_cast<size_t>(
            static_cast<GPtrDiff_t>(m_nBlockXSize) * m_nBlockYSize * nDTSize *
            ((m_nPlanarConfig == PLANARCONFIG_CONTIG) ? nBands : 1));
        if (m_pTempBufferForCommonDirectIO == nullptr)
        {
            m_pTempBufferForCommonDirectIO = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(nTempBufferForCommonDirectIOSize));
            if (m_pTempBufferForCommonDirectIO == nullptr)
                return CE_Failure;
        }

        VSILFILE *fp = VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF));
        FetchBufferDirectIO oFetcher(fp, m_pTempBufferForCommonDirectIO,
                                     nTempBufferForCommonDirectIOSize);

        return CommonDirectIOClassic(oFetcher, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nBandCount, panBandMap, nPixelSpace,
                                     nLineSpace, nBandSpace);
    }

    // Get strip offsets.
    toff_t *panTIFFOffsets = nullptr;
    if (!TIFFGetField(m_hTIFF, TIFFTAG_STRIPOFFSETS, &panTIFFOffsets) ||
        panTIFFOffsets == nullptr)
    {
        return CE_Failure;
    }

    // Sub-sampling or over-sampling can only be done at last stage.
    int nReqXSize = nXSize;
    // Can do sub-sampling at the extraction stage.
    const int nReqYSize = std::min(nBufYSize, nYSize);
    void **ppData =
        static_cast<void **>(VSI_MALLOC_VERBOSE(nReqYSize * sizeof(void *)));
    vsi_l_offset *panOffsets = static_cast<vsi_l_offset *>(
        VSI_MALLOC_VERBOSE(nReqYSize * sizeof(vsi_l_offset)));
    size_t *panSizes =
        static_cast<size_t *>(VSI_MALLOC_VERBOSE(nReqYSize * sizeof(size_t)));
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    void *pTmpBuffer = nullptr;
    int eErr = CE_None;
    int nContigBands = nBands;
    const int nSrcPixelSize = nDTSize * nContigBands;

    if (ppData == nullptr || panOffsets == nullptr || panSizes == nullptr)
    {
        eErr = CE_Failure;
    }
    // For now we always allocate a temp buffer as it is easier.
    else
    // if( nXSize != nBufXSize || nYSize != nBufYSize ||
    //   eBufType != eDataType ||
    //   nPixelSpace != GDALGetDataTypeSizeBytes(eBufType) ||
    //   check if the user buffer is large enough )
    {
        // We need a temporary buffer for over-sampling/sub-sampling
        // and/or data type conversion.
        pTmpBuffer = VSI_MALLOC3_VERBOSE(nReqXSize, nReqYSize, nSrcPixelSize);
        if (pTmpBuffer == nullptr)
            eErr = CE_Failure;
    }

    // Prepare data extraction.
    const double dfSrcYInc = nYSize / static_cast<double>(nBufYSize);

    for (int iLine = 0; eErr == CE_None && iLine < nReqYSize; ++iLine)
    {
        ppData[iLine] = static_cast<GByte *>(pTmpBuffer) +
                        static_cast<size_t>(iLine) * nReqXSize * nSrcPixelSize;
        int nSrcLine = 0;
        if (nBufYSize < nYSize)  // Sub-sampling in y.
            nSrcLine = nYOff + static_cast<int>((iLine + 0.5) * dfSrcYInc);
        else
            nSrcLine = nYOff + iLine;

        const int nBlockXOff = 0;
        const int nBlockYOff = nSrcLine / m_nBlockYSize;
        const int nYOffsetInBlock = nSrcLine % m_nBlockYSize;
        const int nBlockId =
            poProtoBand->ComputeBlockId(nBlockXOff, nBlockYOff);

        panOffsets[iLine] = panTIFFOffsets[nBlockId];
        if (panOffsets[iLine] == 0)  // We don't support sparse files.
            eErr = -1;

        panOffsets[iLine] +=
            (nXOff +
             static_cast<vsi_l_offset>(nYOffsetInBlock) * m_nBlockXSize) *
            nSrcPixelSize;
        panSizes[iLine] = static_cast<size_t>(nReqXSize) * nSrcPixelSize;
    }

    // Extract data from the file.
    if (eErr == CE_None)
    {
        VSILFILE *fp = VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF));
        const int nRet =
            VSIFReadMultiRangeL(nReqYSize, ppData, panOffsets, panSizes, fp);
        if (nRet != 0)
            eErr = CE_Failure;
    }

    // Byte-swap if necessary.
    if (eErr == CE_None && TIFFIsByteSwapped(m_hTIFF))
    {
        for (int iLine = 0; iLine < nReqYSize; ++iLine)
        {
            if (GDALDataTypeIsComplex(eDataType))
                GDALSwapWords(ppData[iLine], nDTSize / 2,
                              2 * nReqXSize * nContigBands, nDTSize / 2);
            else
                GDALSwapWords(ppData[iLine], nDTSize, nReqXSize * nContigBands,
                              nDTSize);
        }
    }

    // Over-sampling/sub-sampling and/or data type conversion.
    const double dfSrcXInc = nXSize / static_cast<double>(nBufXSize);
    if (eErr == CE_None && pTmpBuffer != nullptr)
    {
        for (int iY = 0; iY < nBufYSize; ++iY)
        {
            const int iSrcY = nBufYSize <= nYSize
                                  ? iY
                                  : static_cast<int>((iY + 0.5) * dfSrcYInc);
            // Optimization: no resampling, no data type change, number of
            // bands requested == number of bands and buffer is packed
            // pixel-interleaved.
            if (nBufXSize == nXSize && nContigBands == nBandCount &&
                eDataType == eBufType && nBandSpace == nDTSize &&
                nPixelSpace == nBandCount * nBandSpace)
            {
                memcpy(static_cast<GByte *>(pData) + iY * nLineSpace,
                       ppData[iSrcY],
                       static_cast<size_t>(nReqXSize * nPixelSpace));
            }
            // Other optimization: no resampling, no data type change,
            // data type is Byte/Int8.
            else if (nBufXSize == nXSize && eDataType == eBufType &&
                     (eDataType == GDT_Byte || eDataType == GDT_Int8))
            {
                GByte *pabySrcData = static_cast<GByte *>(ppData[iSrcY]);
                GByte *pabyDstData =
                    static_cast<GByte *>(pData) + iY * nLineSpace;
                if (nBandSpace == 1 && nPixelSpace > nBandCount)
                {
                    // Buffer is pixel-interleaved (with some stridding
                    // between pixels).
                    CopyContigByteMultiBand(
                        pabySrcData, nSrcPixelSize, pabyDstData,
                        static_cast<int>(nPixelSpace), nBufXSize, nBandCount);
                }
                else
                {
                    for (int iBand = 0; iBand < nBandCount; ++iBand)
                    {
                        GDALCopyWords(
                            pabySrcData + iBand, GDT_Byte, nSrcPixelSize,
                            pabyDstData + iBand * nBandSpace, GDT_Byte,
                            static_cast<int>(nPixelSpace), nBufXSize);
                    }
                }
            }
            else  // General case.
            {
                for (int iBand = 0; iBand < nBandCount; ++iBand)
                {
                    GByte *pabySrcData =
                        static_cast<GByte *>(ppData[iSrcY]) + iBand * nDTSize;
                    GByte *pabyDstData = static_cast<GByte *>(pData) +
                                         iBand * nBandSpace + iY * nLineSpace;
                    if ((eDataType == GDT_Byte && eBufType == GDT_Byte) ||
                        (eDataType == GDT_Int8 && eBufType == GDT_Int8))
                    {
                        double dfSrcX = 0.5 * dfSrcXInc;
                        for (int iX = 0; iX < nBufXSize;
                             ++iX, dfSrcX += dfSrcXInc)
                        {
                            int iSrcX = static_cast<int>(dfSrcX);
                            pabyDstData[iX * nPixelSpace] =
                                pabySrcData[iSrcX * nSrcPixelSize];
                        }
                    }
                    else
                    {
                        double dfSrcX = 0.5 * dfSrcXInc;
                        for (int iX = 0; iX < nBufXSize;
                             ++iX, dfSrcX += dfSrcXInc)
                        {
                            int iSrcX = static_cast<int>(dfSrcX);
                            GDALCopyWords(pabySrcData + iSrcX * nSrcPixelSize,
                                          eDataType, 0,
                                          pabyDstData + iX * nPixelSpace,
                                          eBufType, 0, 1);
                        }
                    }
                }
            }
        }
    }

    CPLFree(pTmpBuffer);
    CPLFree(ppData);
    CPLFree(panOffsets);
    CPLFree(panSizes);

    return eErr;
}

/************************************************************************/
/*                             ReadStrile()                             */
/************************************************************************/

bool GTiffDataset::ReadStrile(int nBlockId, void *pOutputBuffer,
                              GPtrDiff_t nBlockReqSize)
{
    // Optimization by which we can save some libtiff buffer copy
    std::pair<vsi_l_offset, vsi_l_offset> oPair;
    if (
#if TIFFLIB_VERSION <= 20220520 && !defined(INTERNAL_LIBTIFF)
        // There's a bug, up to libtiff 4.4.0, in TIFFReadFromUserBuffer()
        // which clears the TIFF_CODERSETUP flag of tif->tif_flags, which
        // causes the codec SetupDecode method to be called for each strile,
        // whereas it should normally be called only for the first decoded one.
        // For JPEG, that causes TIFFjpeg_read_header() to be called. Most
        // of the time, that works. But for some files, at some point, the
        // libjpeg machinery is not in the appropriate state for that.
        m_nCompression != COMPRESSION_JPEG &&
#endif
        m_oCacheStrileToOffsetByteCount.tryGet(nBlockId, oPair))
    {
        // For the mask, use the parent TIFF handle to get cached ranges
        auto th = TIFFClientdata(m_poImageryDS && m_bMaskInterleavedWithImagery
                                     ? m_poImageryDS->m_hTIFF
                                     : m_hTIFF);
        void *pInputBuffer = VSI_TIFFGetCachedRange(
            th, oPair.first, static_cast<size_t>(oPair.second));
        if (pInputBuffer &&
            TIFFReadFromUserBuffer(m_hTIFF, nBlockId, pInputBuffer,
                                   static_cast<size_t>(oPair.second),
                                   pOutputBuffer, nBlockReqSize))
        {
            return true;
        }
    }

    // For debugging
    if (m_poBaseDS)
        m_poBaseDS->m_bHasUsedReadEncodedAPI = true;
    else
        m_bHasUsedReadEncodedAPI = true;

#if 0
    // Can be useful to test TIFFReadFromUserBuffer() for local files
    VSILFILE* fpTIF = VSI_TIFFGetVSILFile(TIFFClientdata( m_hTIFF ));
    std::vector<GByte> tmp(TIFFGetStrileByteCount(m_hTIFF, nBlockId));
    VSIFSeekL(fpTIF, TIFFGetStrileOffset(m_hTIFF, nBlockId), SEEK_SET);
    VSIFReadL(&tmp[0], 1, TIFFGetStrileByteCount(m_hTIFF, nBlockId), fpTIF);
    if( !TIFFReadFromUserBuffer( m_hTIFF, nBlockId,
                                &tmp[0], tmp.size(),
                                pOutputBuffer, nBlockReqSize ) )
    {
        return false;
    }
#else
    // Set to 1 to allow GTiffErrorHandler to implement limitation on error
    // messages
    GTIFFGetThreadLocalLibtiffError() = 1;
    if (TIFFIsTiled(m_hTIFF))
    {
        if (TIFFReadEncodedTile(m_hTIFF, nBlockId, pOutputBuffer,
                                nBlockReqSize) == -1 &&
            !m_bIgnoreReadErrors)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "TIFFReadEncodedTile() failed.");
            GTIFFGetThreadLocalLibtiffError() = 0;
            return false;
        }
    }
    else
    {
        if (TIFFReadEncodedStrip(m_hTIFF, nBlockId, pOutputBuffer,
                                 nBlockReqSize) == -1 &&
            !m_bIgnoreReadErrors)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "TIFFReadEncodedStrip() failed.");
            GTIFFGetThreadLocalLibtiffError() = 0;
            return false;
        }
    }
    GTIFFGetThreadLocalLibtiffError() = 0;
#endif
    return true;
}

/************************************************************************/
/*                            LoadBlockBuf()                            */
/*                                                                      */
/*      Load working block buffer with request block (tile/strip).      */
/************************************************************************/

CPLErr GTiffDataset::LoadBlockBuf(int nBlockId, bool bReadFromDisk)

{
    if (m_nLoadedBlock == nBlockId && m_pabyBlockBuf != nullptr)
        return CE_None;

    /* -------------------------------------------------------------------- */
    /*      If we have a dirty loaded block, flush it out first.            */
    /* -------------------------------------------------------------------- */
    if (m_nLoadedBlock != -1 && m_bLoadedBlockDirty)
    {
        const CPLErr eErr = FlushBlockBuf();
        if (eErr != CE_None)
            return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Get block size.                                                 */
    /* -------------------------------------------------------------------- */
    const GPtrDiff_t nBlockBufSize = static_cast<GPtrDiff_t>(
        TIFFIsTiled(m_hTIFF) ? TIFFTileSize(m_hTIFF) : TIFFStripSize(m_hTIFF));
    if (!nBlockBufSize)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Bogus block size; unable to allocate a buffer.");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Allocate a temporary buffer for this strip.                     */
    /* -------------------------------------------------------------------- */
    if (m_pabyBlockBuf == nullptr)
    {
        m_pabyBlockBuf =
            static_cast<GByte *>(VSI_CALLOC_VERBOSE(1, nBlockBufSize));
        if (m_pabyBlockBuf == nullptr)
        {
            return CE_Failure;
        }
    }

    if (m_nLoadedBlock == nBlockId)
        return CE_None;

    /* -------------------------------------------------------------------- */
    /*  When called from ::IWriteBlock in separate cases (or in single band */
    /*  geotiffs), the ::IWriteBlock will override the content of the buffer*/
    /*  with pImage, so we don't need to read data from disk                */
    /* -------------------------------------------------------------------- */
    if (!bReadFromDisk || m_bStreamingOut)
    {
        m_nLoadedBlock = nBlockId;
        return CE_None;
    }

    // libtiff 3.X doesn't like mixing read&write of JPEG compressed blocks
    // The below hack is necessary due to another hack that consist in
    // writing zero block to force creation of JPEG tables.
    if (nBlockId == 0 && m_bDontReloadFirstBlock)
    {
        m_bDontReloadFirstBlock = false;
        memset(m_pabyBlockBuf, 0, nBlockBufSize);
        m_nLoadedBlock = nBlockId;
        return CE_None;
    }

    /* -------------------------------------------------------------------- */
    /*      The bottom most partial tiles and strips are sometimes only     */
    /*      partially encoded.  This code reduces the requested data so     */
    /*      an error won't be reported in this case. (#1179)                */
    /*      We exclude tiled WEBP, because as it is a new codec, whole tiles*/
    /*      are written by libtiff. This helps avoiding creating a temporary*/
    /*      decode buffer.                                                  */
    /* -------------------------------------------------------------------- */
    auto nBlockReqSize = nBlockBufSize;
    const int nBlockYOff = (nBlockId % m_nBlocksPerBand) / m_nBlocksPerRow;

    if (nBlockYOff * m_nBlockYSize > nRasterYSize - m_nBlockYSize &&
        !(m_nCompression == COMPRESSION_WEBP && TIFFIsTiled(m_hTIFF)))
    {
        nBlockReqSize =
            (nBlockBufSize / m_nBlockYSize) *
            (m_nBlockYSize -
             static_cast<int>(
                 (static_cast<GIntBig>(nBlockYOff + 1) * m_nBlockYSize) %
                 nRasterYSize));
        memset(m_pabyBlockBuf, 0, nBlockBufSize);
    }

    /* -------------------------------------------------------------------- */
    /*      If we don't have this block already loaded, and we know it      */
    /*      doesn't yet exist on disk, just zero the memory buffer and      */
    /*      pretend we loaded it.                                           */
    /* -------------------------------------------------------------------- */
    bool bErrOccurred = false;
    if (!IsBlockAvailable(nBlockId, nullptr, nullptr, &bErrOccurred))
    {
        memset(m_pabyBlockBuf, 0, nBlockBufSize);
        m_nLoadedBlock = nBlockId;
        if (bErrOccurred)
            return CE_Failure;
        return CE_None;
    }

    /* -------------------------------------------------------------------- */
    /*      Load the block, if it isn't our current block.                  */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if (!ReadStrile(nBlockId, m_pabyBlockBuf, nBlockReqSize))
    {
        memset(m_pabyBlockBuf, 0, nBlockBufSize);
        eErr = CE_Failure;
    }

    if (eErr == CE_None)
    {
        if (m_nCompression == COMPRESSION_WEBP && TIFFIsTiled(m_hTIFF) &&
            nBlockYOff * m_nBlockYSize > nRasterYSize - m_nBlockYSize)
        {
            const auto nValidBytes =
                (nBlockBufSize / m_nBlockYSize) *
                (m_nBlockYSize -
                 static_cast<int>(
                     (static_cast<GIntBig>(nBlockYOff + 1) * m_nBlockYSize) %
                     nRasterYSize));
            // Zero-out unused area
            memset(m_pabyBlockBuf + nValidBytes, 0,
                   nBlockBufSize - nValidBytes);
        }

        m_nLoadedBlock = nBlockId;
    }
    else
    {
        m_nLoadedBlock = -1;
    }
    m_bLoadedBlockDirty = false;

    return eErr;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int GTiffDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;
    if (STARTS_WITH_CI(pszFilename, "GTIFF_RAW:"))
    {
        pszFilename += strlen("GTIFF_RAW:");
        GDALOpenInfo oOpenInfo(pszFilename, poOpenInfo->eAccess);
        return Identify(&oOpenInfo);
    }

    /* -------------------------------------------------------------------- */
    /*      We have a special hook for handling opening a specific          */
    /*      directory of a TIFF file.                                       */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszFilename, "GTIFF_DIR:"))
        return TRUE;

    /* -------------------------------------------------------------------- */
    /*      First we check to see if the file has the expected header       */
    /*      bytes.                                                          */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes < 2)
        return FALSE;

    if ((poOpenInfo->pabyHeader[0] != 'I' ||
         poOpenInfo->pabyHeader[1] != 'I') &&
        (poOpenInfo->pabyHeader[0] != 'M' || poOpenInfo->pabyHeader[1] != 'M'))
        return FALSE;

    if ((poOpenInfo->pabyHeader[2] != 0x2A || poOpenInfo->pabyHeader[3] != 0) &&
        (poOpenInfo->pabyHeader[3] != 0x2A || poOpenInfo->pabyHeader[2] != 0) &&
        (poOpenInfo->pabyHeader[2] != 0x2B || poOpenInfo->pabyHeader[3] != 0) &&
        (poOpenInfo->pabyHeader[3] != 0x2B || poOpenInfo->pabyHeader[2] != 0))
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                          GTIFFExtendMemoryFile()                     */
/************************************************************************/

static bool GTIFFExtendMemoryFile(const CPLString &osTmpFilename,
                                  VSILFILE *fpTemp, VSILFILE *fpL,
                                  int nNewLength, GByte *&pabyBuffer,
                                  vsi_l_offset &nDataLength)
{
    if (nNewLength <= static_cast<int>(nDataLength))
        return true;
    if (VSIFSeekL(fpTemp, nNewLength - 1, SEEK_SET) != 0)
        return false;
    char ch = 0;
    if (VSIFWriteL(&ch, 1, 1, fpTemp) != 1)
        return false;
    const int nOldDataLength = static_cast<int>(nDataLength);
    pabyBuffer = static_cast<GByte *>(
        VSIGetMemFileBuffer(osTmpFilename, &nDataLength, FALSE));
    const int nToRead = nNewLength - nOldDataLength;
    const int nRead = static_cast<int>(
        VSIFReadL(pabyBuffer + nOldDataLength, 1, nToRead, fpL));
    if (nRead != nToRead)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Needed to read %d bytes. Only %d got", nToRead, nRead);
        return false;
    }
    return true;
}

/************************************************************************/
/*                         GTIFFMakeBufferedStream()                    */
/************************************************************************/

static bool GTIFFMakeBufferedStream(GDALOpenInfo *poOpenInfo)
{
    CPLString osTmpFilename;
    static int nCounter = 0;
    osTmpFilename.Printf("/vsimem/stream_%d.tif", ++nCounter);
    VSILFILE *fpTemp = VSIFOpenL(osTmpFilename, "wb+");
    if (fpTemp == nullptr)
        return false;
    // The seek is needed for /vsistdin/ that has some rewind capabilities.
    if (VSIFSeekL(poOpenInfo->fpL, poOpenInfo->nHeaderBytes, SEEK_SET) != 0)
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
        return false;
    }
    CPLAssert(static_cast<int>(VSIFTellL(poOpenInfo->fpL)) ==
              poOpenInfo->nHeaderBytes);
    if (VSIFWriteL(poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes, 1,
                   fpTemp) != 1)
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
        return false;
    }
    vsi_l_offset nDataLength = 0;
    GByte *pabyBuffer = static_cast<GByte *>(
        VSIGetMemFileBuffer(osTmpFilename, &nDataLength, FALSE));
    const bool bLittleEndian = (pabyBuffer[0] == 'I');
#if CPL_IS_LSB
    const bool bSwap = !bLittleEndian;
#else
    const bool bSwap = bLittleEndian;
#endif
    const bool bBigTIFF = pabyBuffer[2] == 43 || pabyBuffer[3] == 43;
    vsi_l_offset nMaxOffset = 0;
    if (bBigTIFF)
    {
        GUInt64 nTmp = 0;
        memcpy(&nTmp, pabyBuffer + 8, 8);
        if (bSwap)
            CPL_SWAP64PTR(&nTmp);
        if (nTmp != 16)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "IFD start should be at offset 16 for a streamed BigTIFF");
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        memcpy(&nTmp, pabyBuffer + 16, 8);
        if (bSwap)
            CPL_SWAP64PTR(&nTmp);
        if (nTmp > 1024)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many tags : " CPL_FRMT_GIB, nTmp);
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        const int nTags = static_cast<int>(nTmp);
        const int nSpaceForTags = nTags * 20;
        if (!GTIFFExtendMemoryFile(osTmpFilename, fpTemp, poOpenInfo->fpL,
                                   24 + nSpaceForTags, pabyBuffer, nDataLength))
        {
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        nMaxOffset = 24 + nSpaceForTags + 8;
        for (int i = 0; i < nTags; ++i)
        {
            GUInt16 nTmp16 = 0;
            memcpy(&nTmp16, pabyBuffer + 24 + i * 20, 2);
            if (bSwap)
                CPL_SWAP16PTR(&nTmp16);
            const int nTag = nTmp16;
            memcpy(&nTmp16, pabyBuffer + 24 + i * 20 + 2, 2);
            if (bSwap)
                CPL_SWAP16PTR(&nTmp16);
            const int nDataType = nTmp16;
            memcpy(&nTmp, pabyBuffer + 24 + i * 20 + 4, 8);
            if (bSwap)
                CPL_SWAP64PTR(&nTmp);
            if (nTmp >= 16 * 1024 * 1024)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Too many elements for tag %d : " CPL_FRMT_GUIB, nTag,
                         nTmp);
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
                VSIUnlink(osTmpFilename);
                return false;
            }
            const GUInt32 nCount = static_cast<GUInt32>(nTmp);
            const GUInt32 nTagSize =
                TIFFDataWidth(static_cast<TIFFDataType>(nDataType)) * nCount;
            if (nTagSize > 8)
            {
                memcpy(&nTmp, pabyBuffer + 24 + i * 20 + 12, 8);
                if (bSwap)
                    CPL_SWAP64PTR(&nTmp);
                if (nTmp > GUINT64_MAX - nTagSize)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Overflow with tag %d", nTag);
                    CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
                    VSIUnlink(osTmpFilename);
                    return false;
                }
                if (static_cast<vsi_l_offset>(nTmp + nTagSize) > nMaxOffset)
                    nMaxOffset = nTmp + nTagSize;
            }
        }
    }
    else
    {
        GUInt32 nTmp = 0;
        memcpy(&nTmp, pabyBuffer + 4, 4);
        if (bSwap)
            CPL_SWAP32PTR(&nTmp);
        if (nTmp != 8)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "IFD start should be at offset 8 for a streamed TIFF");
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        GUInt16 nTmp16 = 0;
        memcpy(&nTmp16, pabyBuffer + 8, 2);
        if (bSwap)
            CPL_SWAP16PTR(&nTmp16);
        if (nTmp16 > 1024)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Too many tags : %d",
                     nTmp16);
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        const int nTags = nTmp16;
        const int nSpaceForTags = nTags * 12;
        if (!GTIFFExtendMemoryFile(osTmpFilename, fpTemp, poOpenInfo->fpL,
                                   10 + nSpaceForTags, pabyBuffer, nDataLength))
        {
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        nMaxOffset = 10 + nSpaceForTags + 4;
        for (int i = 0; i < nTags; ++i)
        {
            memcpy(&nTmp16, pabyBuffer + 10 + i * 12, 2);
            if (bSwap)
                CPL_SWAP16PTR(&nTmp16);
            const int nTag = nTmp16;
            memcpy(&nTmp16, pabyBuffer + 10 + i * 12 + 2, 2);
            if (bSwap)
                CPL_SWAP16PTR(&nTmp16);
            const int nDataType = nTmp16;
            memcpy(&nTmp, pabyBuffer + 10 + i * 12 + 4, 4);
            if (bSwap)
                CPL_SWAP32PTR(&nTmp);
            if (nTmp >= 16 * 1024 * 1024)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Too many elements for tag %d : %u", nTag, nTmp);
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
                VSIUnlink(osTmpFilename);
                return false;
            }
            const GUInt32 nCount = nTmp;
            const GUInt32 nTagSize =
                TIFFDataWidth(static_cast<TIFFDataType>(nDataType)) * nCount;
            if (nTagSize > 4)
            {
                memcpy(&nTmp, pabyBuffer + 10 + i * 12 + 8, 4);
                if (bSwap)
                    CPL_SWAP32PTR(&nTmp);
                if (nTmp > static_cast<GUInt32>(UINT_MAX - nTagSize))
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Overflow with tag %d", nTag);
                    CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
                    VSIUnlink(osTmpFilename);
                    return false;
                }
                if (nTmp + nTagSize > nMaxOffset)
                    nMaxOffset = nTmp + nTagSize;
            }
        }
    }
    if (nMaxOffset > 10 * 1024 * 1024)
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
        VSIUnlink(osTmpFilename);
        return false;
    }
    if (!GTIFFExtendMemoryFile(osTmpFilename, fpTemp, poOpenInfo->fpL,
                               static_cast<int>(nMaxOffset), pabyBuffer,
                               nDataLength))
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
        VSIUnlink(osTmpFilename);
        return false;
    }
    CPLAssert(nDataLength == VSIFTellL(poOpenInfo->fpL));
    poOpenInfo->fpL = VSICreateBufferedReaderHandle(
        poOpenInfo->fpL, pabyBuffer, static_cast<vsi_l_offset>(INT_MAX) << 32);
    if (VSIFCloseL(fpTemp) != 0)
        return false;
    VSIUnlink(osTmpFilename);

    return true;
}

/************************************************************************/
/*                       AssociateExternalMask()                        */
/************************************************************************/

// Used by GTIFFBuildOverviewsEx() for the COG driver
bool GTiffDataset::AssociateExternalMask()
{
    if (m_poMaskExtOvrDS->GetRasterBand(1)->GetOverviewCount() !=
        GetRasterBand(1)->GetOverviewCount())
        return false;
    if (m_papoOverviewDS == nullptr)
        return false;
    if (m_poMaskDS)
        return false;
    if (m_poMaskExtOvrDS->GetRasterXSize() != nRasterXSize ||
        m_poMaskExtOvrDS->GetRasterYSize() != nRasterYSize)
        return false;
    m_poExternalMaskDS = m_poMaskExtOvrDS.get();
    for (int i = 0; i < m_nOverviewCount; i++)
    {
        if (m_papoOverviewDS[i]->m_poMaskDS)
            return false;
        m_papoOverviewDS[i]->m_poExternalMaskDS =
            m_poMaskExtOvrDS->GetRasterBand(1)->GetOverview(i)->GetDataset();
        if (!m_papoOverviewDS[i]->m_poExternalMaskDS)
            return false;
        auto poOvrBand = m_papoOverviewDS[i]->GetRasterBand(1);
        if (m_papoOverviewDS[i]->m_poExternalMaskDS->GetRasterXSize() !=
                poOvrBand->GetXSize() ||
            m_papoOverviewDS[i]->m_poExternalMaskDS->GetRasterYSize() !=
                poOvrBand->GetYSize())
            return false;
    }
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTiffDataset::Open(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;

    /* -------------------------------------------------------------------- */
    /*      Check if it looks like a TIFF file.                             */
    /* -------------------------------------------------------------------- */
    if (!Identify(poOpenInfo))
        return nullptr;

    bool bAllowRGBAInterface = true;
    if (STARTS_WITH_CI(pszFilename, "GTIFF_RAW:"))
    {
        bAllowRGBAInterface = false;
        pszFilename += strlen("GTIFF_RAW:");
    }

    /* -------------------------------------------------------------------- */
    /*      We have a special hook for handling opening a specific          */
    /*      directory of a TIFF file.                                       */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszFilename, "GTIFF_DIR:"))
        return OpenDir(poOpenInfo);

    GTiffOneTimeInit();

    /* -------------------------------------------------------------------- */
    /*      Try opening the dataset.                                        */
    /* -------------------------------------------------------------------- */
    bool bStreaming = false;
    const char *pszReadStreaming =
        CPLGetConfigOption("TIFF_READ_STREAMING", nullptr);
    if (poOpenInfo->fpL == nullptr)
    {
        poOpenInfo->fpL = VSIFOpenL(
            pszFilename, poOpenInfo->eAccess == GA_ReadOnly ? "rb" : "r+b");
        if (poOpenInfo->fpL == nullptr)
            return nullptr;
    }
    else if (!(pszReadStreaming && !CPLTestBool(pszReadStreaming)) &&
             poOpenInfo->nHeaderBytes >= 24 &&
             // A pipe has no seeking capability, so its position is 0 despite
             // having read bytes.
             (static_cast<int>(VSIFTellL(poOpenInfo->fpL)) ==
                  poOpenInfo->nHeaderBytes ||
              strcmp(pszFilename, "/vsistdin/") == 0 ||
              // STARTS_WITH(pszFilename, "/vsicurl_streaming/") ||
              (pszReadStreaming && CPLTestBool(pszReadStreaming))))
    {
        bStreaming = true;
        if (!GTIFFMakeBufferedStream(poOpenInfo))
            return nullptr;
    }

    // Store errors/warnings and emit them later.
    std::vector<CPLErrorHandlerAccumulatorStruct> aoErrors;
    CPLInstallErrorHandlerAccumulator(aoErrors);
    CPLSetCurrentErrorHandlerCatchDebug(FALSE);
    const bool bDeferStrileLoading = CPLTestBool(
        CPLGetConfigOption("GTIFF_USE_DEFER_STRILE_LOADING", "YES"));
    TIFF *l_hTIFF = VSI_TIFFOpen(
        pszFilename,
        poOpenInfo->eAccess == GA_ReadOnly
            ? ((bStreaming || !bDeferStrileLoading) ? "rC" : "rDOC")
            : (!bDeferStrileLoading ? "r+C" : "r+DC"),
        poOpenInfo->fpL);
    CPLUninstallErrorHandlerAccumulator();

    // Now emit errors and change their criticality if needed
    // We only emit failures if we didn't manage to open the file.
    // Otherwise it makes Python bindings unhappy (#5616).
    for (size_t iError = 0; iError < aoErrors.size(); ++iError)
    {
        ReportError(pszFilename,
                    (l_hTIFF == nullptr && aoErrors[iError].type == CE_Failure)
                        ? CE_Failure
                        : CE_Warning,
                    aoErrors[iError].no, "%s", aoErrors[iError].msg.c_str());
    }
    aoErrors.resize(0);

    if (l_hTIFF == nullptr)
        return nullptr;

    uint32_t nXSize = 0;
    TIFFGetField(l_hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize);
    uint32_t nYSize = 0;
    TIFFGetField(l_hTIFF, TIFFTAG_IMAGELENGTH, &nYSize);

    if (nXSize > INT_MAX || nYSize > INT_MAX)
    {
        // GDAL only supports signed 32bit dimensions.
        ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                    "Too large image size: %u x %u", nXSize, nYSize);
        XTIFFClose(l_hTIFF);
        return nullptr;
    }

    uint16_t l_nCompression = 0;
    if (!TIFFGetField(l_hTIFF, TIFFTAG_COMPRESSION, &(l_nCompression)))
        l_nCompression = COMPRESSION_NONE;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->SetDescription(pszFilename);
    poDS->m_pszFilename = CPLStrdup(pszFilename);
    poDS->m_fpL = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    poDS->m_bStreamingIn = bStreaming;
    poDS->m_nCompression = l_nCompression;

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
        poDS->m_bLayoutIFDSBeforeData =
            strstr(pszStructuralMD, "LAYOUT=IFDS_BEFORE_DATA") != nullptr;
        poDS->m_bBlockOrderRowMajor =
            strstr(pszStructuralMD, "BLOCK_ORDER=ROW_MAJOR") != nullptr;
        poDS->m_bLeaderSizeAsUInt4 =
            strstr(pszStructuralMD, "BLOCK_LEADER=SIZE_AS_UINT4") != nullptr;
        poDS->m_bTrailerRepeatedLast4BytesRepeated =
            strstr(pszStructuralMD, "BLOCK_TRAILER=LAST_4_BYTES_REPEATED") !=
            nullptr;
        poDS->m_bMaskInterleavedWithImagery =
            strstr(pszStructuralMD, "MASK_INTERLEAVED_WITH_IMAGERY=YES") !=
            nullptr;
        poDS->m_bKnownIncompatibleEdition =
            strstr(pszStructuralMD, "KNOWN_INCOMPATIBLE_EDITION=YES") !=
            nullptr;
        if (poDS->m_bKnownIncompatibleEdition)
        {
            poDS->ReportError(
                CE_Warning, CPLE_AppDefined,
                "This file used to have optimizations in its layout, "
                "but those have been, at least partly, invalidated by "
                "later changes");
        }
        else if (poDS->m_bLayoutIFDSBeforeData && poDS->m_bBlockOrderRowMajor &&
                 poDS->m_bLeaderSizeAsUInt4 &&
                 poDS->m_bTrailerRepeatedLast4BytesRepeated)
        {
            if (poOpenInfo->eAccess == GA_Update &&
                !CPLTestBool(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                  "IGNORE_COG_LAYOUT_BREAK",
                                                  "FALSE")))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "File %s has C(loud) O(ptimized) G(eoTIFF) layout. "
                         "Updating it will generally result in losing part of "
                         "the optimizations (but will still produce a valid "
                         "GeoTIFF file). If this is acceptable, open the file "
                         "with the IGNORE_COG_LAYOUT_BREAK open option set "
                         "to YES.",
                         pszFilename);
                delete poDS;
                return nullptr;
            }
            poDS->m_oGTiffMDMD.SetMetadataItem("LAYOUT", "COG",
                                               "IMAGE_STRUCTURE");
        }
    }

    // In the case of GDAL_DISABLE_READDIR_ON_OPEN = NO / EMPTY_DIR
    if (poOpenInfo->AreSiblingFilesLoaded() &&
        CSLCount(poOpenInfo->GetSiblingFiles()) <= 1)
    {
        poDS->oOvManager.TransferSiblingFiles(
            CSLDuplicate(poOpenInfo->GetSiblingFiles()));
        poDS->m_bHasGotSiblingFiles = true;
    }

    if (poDS->OpenOffset(l_hTIFF, TIFFCurrentDirOffset(l_hTIFF),
                         poOpenInfo->eAccess, bAllowRGBAInterface,
                         true) != CE_None)
    {
        delete poDS;
        return nullptr;
    }

    // Do we want blocks that are set to zero and that haven't yet being
    // allocated as tile/strip to remain implicit?
    if (CPLFetchBool(poOpenInfo->papszOpenOptions, "SPARSE_OK", false))
        poDS->m_bWriteEmptyTiles = false;

    poDS->InitCreationOrOpenOptions(poOpenInfo->eAccess == GA_Update,
                                    poOpenInfo->papszOpenOptions);

    poDS->m_bLoadPam = true;
    poDS->m_bColorProfileMetadataChanged = false;
    poDS->m_bMetadataChanged = false;
    poDS->m_bGeoTIFFInfoChanged = false;
    poDS->m_bNoDataChanged = false;
    poDS->m_bForceUnsetGTOrGCPs = false;
    poDS->m_bForceUnsetProjection = false;

    // Used by GTIFFBuildOverviewsEx() for the COG driver
    const char *pszMaskOverviewDS = CSLFetchNameValue(
        poOpenInfo->papszOpenOptions, "MASK_OVERVIEW_DATASET");
    if (pszMaskOverviewDS)
    {
        poDS->m_poMaskExtOvrDS.reset(GDALDataset::Open(
            pszMaskOverviewDS, GDAL_OF_RASTER | GDAL_OF_INTERNAL));
        if (!poDS->m_poMaskExtOvrDS || !poDS->AssociateExternalMask())
        {
            CPLDebug("GTiff",
                     "Association with external mask overview file failed");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize info for external overviews.                         */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS, pszFilename);
    if (poOpenInfo->AreSiblingFilesLoaded())
        poDS->oOvManager.TransferSiblingFiles(poOpenInfo->StealSiblingFiles());

    // For backward compatibility, in case GTIFF_POINT_GEO_IGNORE is defined
    // load georeferencing right now so as to not require it to be defined
    // at the GetGeoTransform() time.
    if (CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE", nullptr) != nullptr)
    {
        poDS->LoadGeoreferencingAndPamIfNeeded();
    }

    return poDS;
}

/************************************************************************/
/*                      GTiffDatasetSetAreaOrPointMD()                  */
/************************************************************************/

static void GTiffDatasetSetAreaOrPointMD(GTIF *hGTIF,
                                         GDALMultiDomainMetadata &m_oGTiffMDMD)
{
    // Is this a pixel-is-point dataset?
    unsigned short nRasterType = 0;

    if (GDALGTIFKeyGetSHORT(hGTIF, GTRasterTypeGeoKey, &nRasterType, 0, 1) == 1)
    {
        if (nRasterType == static_cast<short>(RasterPixelIsPoint))
            m_oGTiffMDMD.SetMetadataItem(GDALMD_AREA_OR_POINT,
                                         GDALMD_AOP_POINT);
        else
            m_oGTiffMDMD.SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_AREA);
    }
}

/************************************************************************/
/*                         LoadMDAreaOrPoint()                          */
/************************************************************************/

// This is a light version of LookForProjection(), which saves the
// potential costly cost of GTIFGetOGISDefn(), since we just need to
// access to a raw GeoTIFF key, and not build the full projection object.

void GTiffDataset::LoadMDAreaOrPoint()
{
    if (m_bLookedForProjection || m_bLookedForMDAreaOrPoint ||
        m_oGTiffMDMD.GetMetadataItem(GDALMD_AREA_OR_POINT) != nullptr)
        return;

    m_bLookedForMDAreaOrPoint = true;

    GTIF *hGTIF = GTiffDataset::GTIFNew(m_hTIFF);

    if (!hGTIF)
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                    "GeoTIFF tags apparently corrupt, they are being ignored.");
    }
    else
    {
        GTiffDatasetSetAreaOrPointMD(hGTIF, m_oGTiffMDMD);

        GTIFFree(hGTIF);
    }
}

/************************************************************************/
/*                         LookForProjection()                          */
/************************************************************************/

void GTiffDataset::LookForProjection()

{
    if (m_bLookedForProjection)
        return;

    m_bLookedForProjection = true;

    IdentifyAuthorizedGeoreferencingSources();

    m_oSRS.Clear();

    std::set<signed char> aoSetPriorities;
    if (m_nINTERNALGeorefSrcIndex >= 0)
        aoSetPriorities.insert(m_nINTERNALGeorefSrcIndex);
    if (m_nXMLGeorefSrcIndex >= 0)
        aoSetPriorities.insert(m_nXMLGeorefSrcIndex);
    for (const auto nIndex : aoSetPriorities)
    {
        if (m_nINTERNALGeorefSrcIndex == nIndex)
        {
            LookForProjectionFromGeoTIFF();
        }
        else if (m_nXMLGeorefSrcIndex == nIndex)
        {
            LookForProjectionFromXML();
        }
    }
}

/************************************************************************/
/*                      LookForProjectionFromGeoTIFF()                  */
/************************************************************************/

void GTiffDataset::LookForProjectionFromGeoTIFF()
{
    /* -------------------------------------------------------------------- */
    /*      Capture the GeoTIFF projection, if available.                   */
    /* -------------------------------------------------------------------- */

    GTIF *hGTIF = GTiffDataset::GTIFNew(m_hTIFF);

    if (!hGTIF)
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                    "GeoTIFF tags apparently corrupt, they are being ignored.");
    }
    else
    {
        GTIFDefn *psGTIFDefn = GTIFAllocDefn();

        bool bHasErrorBefore = CPLGetLastErrorType() != 0;
        // Collect (PROJ) error messages and remit them later as warnings
        std::vector<CPLErrorHandlerAccumulatorStruct> aoErrors;
        CPLInstallErrorHandlerAccumulator(aoErrors);
        const int ret = GTIFGetDefn(hGTIF, psGTIFDefn);
        CPLUninstallErrorHandlerAccumulator();

        bool bWarnAboutEllipsoid = true;

        if (ret)
        {
            CPLInstallErrorHandlerAccumulator(aoErrors);

            if (psGTIFDefn->Ellipsoid == 4326 &&
                psGTIFDefn->SemiMajor == 6378137 &&
                psGTIFDefn->SemiMinor == 6356752.314245)
            {
                // Buggy Sentinel1 geotiff files use a wrong 4326 code for the
                // ellipsoid instead of 7030.
                psGTIFDefn->Ellipsoid = 7030;
                bWarnAboutEllipsoid = false;
            }

            OGRSpatialReferenceH hSRS = GTIFGetOGISDefnAsOSR(hGTIF, psGTIFDefn);
            CPLUninstallErrorHandlerAccumulator();

            if (hSRS)
            {
                CPLFree(m_pszXMLFilename);
                m_pszXMLFilename = nullptr;

                m_oSRS = *(OGRSpatialReference::FromHandle(hSRS));
                OSRDestroySpatialReference(hSRS);
            }
        }

        std::set<std::string> oSetErrorMsg;
        for (const auto &oError : aoErrors)
        {
            if (!bWarnAboutEllipsoid &&
                oError.msg.find("ellipsoid not found") != std::string::npos)
            {
                continue;
            }

            // Some error messages might be duplicated in GTIFGetDefn()
            // and GTIFGetOGISDefnAsOSR(). Emit them just once.
            if (oSetErrorMsg.find(oError.msg) == oSetErrorMsg.end())
            {
                oSetErrorMsg.insert(oError.msg);
                CPLError(oError.type == CE_Failure ? CE_Warning : oError.type,
                         oError.no, "%s", oError.msg.c_str());
            }
        }

        if (!bHasErrorBefore && oSetErrorMsg.empty())
        {
            CPLErrorReset();
        }

        if (ret && m_oSRS.IsCompound())
        {
            const char *pszVertUnit = nullptr;
            m_oSRS.GetTargetLinearUnits("COMPD_CS|VERT_CS", &pszVertUnit);
            if (pszVertUnit && !EQUAL(pszVertUnit, "unknown"))
            {
                CPLFree(m_pszVertUnit);
                m_pszVertUnit = CPLStrdup(pszVertUnit);
            }

            int versions[3];
            GTIFDirectoryInfo(hGTIF, versions, nullptr);

            // If GeoTIFF 1.0, strip vertical by default
            const char *pszDefaultReportCompdCS =
                (versions[0] == 1 && versions[1] == 1 && versions[2] == 0)
                    ? "NO"
                    : "YES";

            // Should we simplify away vertical CS stuff?
            if (!CPLTestBool(CPLGetConfigOption("GTIFF_REPORT_COMPD_CS",
                                                pszDefaultReportCompdCS)))
            {
                CPLDebug("GTiff", "Got COMPD_CS, but stripping it.");

                m_oSRS.StripVertical();
            }
        }

        GTIFFreeDefn(psGTIFDefn);

        GTiffDatasetSetAreaOrPointMD(hGTIF, m_oGTiffMDMD);

        GTIFFree(hGTIF);
    }
}

/************************************************************************/
/*                      LookForProjectionFromXML()                      */
/************************************************************************/

void GTiffDataset::LookForProjectionFromXML()
{
    char **papszSiblingFiles = GetSiblingFiles();

    if (!GDALCanFileAcceptSidecarFile(m_pszFilename))
        return;

    const std::string osXMLFilenameLowerCase =
        CPLResetExtension(m_pszFilename, "xml");

    CPLString osXMLFilename;
    if (papszSiblingFiles &&
        GDALCanReliablyUseSiblingFileList(osXMLFilenameLowerCase.c_str()))
    {
        const int iSibling = CSLFindString(
            papszSiblingFiles, CPLGetFilename(osXMLFilenameLowerCase.c_str()));
        if (iSibling >= 0)
        {
            osXMLFilename = m_pszFilename;
            osXMLFilename.resize(strlen(m_pszFilename) -
                                 strlen(CPLGetFilename(m_pszFilename)));
            osXMLFilename += papszSiblingFiles[iSibling];
        }
        else
        {
            return;
        }
    }

    if (osXMLFilename.empty())
    {
        VSIStatBufL sStatBuf;
        bool bGotXML = VSIStatExL(osXMLFilenameLowerCase.c_str(), &sStatBuf,
                                  VSI_STAT_EXISTS_FLAG) == 0;

        if (bGotXML)
        {
            osXMLFilename = osXMLFilenameLowerCase;
        }
        else if (VSIIsCaseSensitiveFS(osXMLFilenameLowerCase.c_str()))
        {
            const std::string osXMLFilenameUpperCase =
                CPLResetExtension(m_pszFilename, "XML");
            bGotXML = VSIStatExL(osXMLFilenameUpperCase.c_str(), &sStatBuf,
                                 VSI_STAT_EXISTS_FLAG) == 0;
            if (bGotXML)
            {
                osXMLFilename = osXMLFilenameUpperCase;
            }
        }

        if (osXMLFilename.empty())
        {
            return;
        }
    }

    GByte *pabyRet = nullptr;
    vsi_l_offset nSize = 0;
    constexpr int nMaxSize = 10 * 1024 * 1024;
    if (!VSIIngestFile(nullptr, osXMLFilename.c_str(), &pabyRet, &nSize,
                       nMaxSize))
        return;
    CPLXMLTreeCloser oXML(
        CPLParseXMLString(reinterpret_cast<const char *>(pabyRet)));
    VSIFree(pabyRet);
    if (!oXML.get())
        return;
    const char *pszCode = CPLGetXMLValue(
        oXML.get(), "=metadata.refSysInfo.RefSystem.refSysID.identCode.code",
        "0");
    const int nCode = atoi(pszCode);
    if (nCode <= 0)
        return;
    if (nCode <= 32767)
        m_oSRS.importFromEPSG(nCode);
    else
        m_oSRS.SetFromUserInput(CPLSPrintf("ESRI:%d", nCode));

    CPLFree(m_pszXMLFilename);
    m_pszXMLFilename = CPLStrdup(osXMLFilename.c_str());
}

/************************************************************************/
/*                            ApplyPamInfo()                            */
/*                                                                      */
/*      PAM Information, if available, overrides the GeoTIFF            */
/*      geotransform and projection definition.  Check for them         */
/*      now.                                                            */
/************************************************************************/

void GTiffDataset::ApplyPamInfo()

{
    bool bGotGTFromPAM = false;

    if (m_nPAMGeorefSrcIndex >= 0 &&
        ((m_bGeoTransformValid &&
          m_nPAMGeorefSrcIndex < m_nGeoTransformGeorefSrcIndex) ||
         m_nGeoTransformGeorefSrcIndex < 0 || !m_bGeoTransformValid))
    {
        double adfPamGeoTransform[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        if (GDALPamDataset::GetGeoTransform(adfPamGeoTransform) == CE_None)
        {
            if (m_nGeoTransformGeorefSrcIndex == m_nWORLDFILEGeorefSrcIndex)
            {
                CPLFree(m_pszGeorefFilename);
                m_pszGeorefFilename = nullptr;
            }
            memcpy(m_adfGeoTransform, adfPamGeoTransform, sizeof(double) * 6);
            m_bGeoTransformValid = true;
            bGotGTFromPAM = true;
        }
    }

    if (m_nPAMGeorefSrcIndex >= 0)
    {
        if ((m_nTABFILEGeorefSrcIndex < 0 ||
             m_nPAMGeorefSrcIndex < m_nTABFILEGeorefSrcIndex) &&
            (m_nINTERNALGeorefSrcIndex < 0 ||
             m_nPAMGeorefSrcIndex < m_nINTERNALGeorefSrcIndex))
        {
            const auto *poPamSRS = GDALPamDataset::GetSpatialRef();
            if (poPamSRS)
            {
                m_oSRS = *poPamSRS;
                m_bLookedForProjection = true;
                // m_nProjectionGeorefSrcIndex = m_nPAMGeorefSrcIndex;
            }
        }
        else
        {
            if (m_nINTERNALGeorefSrcIndex >= 0)
                LookForProjection();
            if (m_oSRS.IsEmpty())
            {
                const auto *poPamSRS = GDALPamDataset::GetSpatialRef();
                if (poPamSRS)
                {
                    m_oSRS = *poPamSRS;
                    m_bLookedForProjection = true;
                    // m_nProjectionGeorefSrcIndex = m_nPAMGeorefSrcIndex;
                }
            }
        }
    }

    int nPamGCPCount;
    if (m_nPAMGeorefSrcIndex >= 0 && !oMDMD.GetMetadata("xml:ESRI") &&
        (nPamGCPCount = GDALPamDataset::GetGCPCount()) > 0 &&
        ((!m_aoGCPs.empty() &&
          m_nPAMGeorefSrcIndex < m_nGeoTransformGeorefSrcIndex) ||
         m_nGeoTransformGeorefSrcIndex < 0 || m_aoGCPs.empty()))
    {
        m_aoGCPs = gdal::GCP::fromC(GDALPamDataset::GetGCPs(), nPamGCPCount);

        // Invalidate Geotransorm got from less prioritary sources
        if (!m_aoGCPs.empty() && m_bGeoTransformValid && !bGotGTFromPAM &&
            m_nPAMGeorefSrcIndex == 0)
        {
            m_bGeoTransformValid = false;
        }

        // m_nProjectionGeorefSrcIndex = m_nPAMGeorefSrcIndex;

        const auto *poPamGCPSRS = GDALPamDataset::GetGCPSpatialRef();
        if (poPamGCPSRS)
            m_oSRS = *poPamGCPSRS;
        else
            m_oSRS.Clear();

        m_bLookedForProjection = true;
    }

    if (m_nPAMGeorefSrcIndex >= 0)
    {
        CPLXMLNode *psValueAsXML = nullptr;
        CPLXMLNode *psGeodataXform = nullptr;
        char **papszXML = oMDMD.GetMetadata("xml:ESRI");
        if (CSLCount(papszXML) == 1)
        {
            psValueAsXML = CPLParseXMLString(papszXML[0]);
            if (psValueAsXML)
                psGeodataXform = CPLGetXMLNode(psValueAsXML, "=GeodataXform");
        }

        const char *pszTIFFTagResUnit =
            GetMetadataItem("TIFFTAG_RESOLUTIONUNIT");
        const char *pszTIFFTagXRes = GetMetadataItem("TIFFTAG_XRESOLUTION");
        const char *pszTIFFTagYRes = GetMetadataItem("TIFFTAG_YRESOLUTION");
        if (psGeodataXform && pszTIFFTagXRes && pszTIFFTagYRes &&
            pszTIFFTagResUnit && atoi(pszTIFFTagResUnit) == 2)
        {
            CPLXMLNode *psSourceGCPs =
                CPLGetXMLNode(psGeodataXform, "SourceGCPs");
            CPLXMLNode *psTargetGCPs =
                CPLGetXMLNode(psGeodataXform, "TargetGCPs");
            if (psSourceGCPs && psTargetGCPs)
            {
                std::vector<double> adfSourceGCPs, adfTargetGCPs;
                for (CPLXMLNode *psIter = psSourceGCPs->psChild;
                     psIter != nullptr; psIter = psIter->psNext)
                {
                    if (psIter->eType == CXT_Element &&
                        EQUAL(psIter->pszValue, "Double"))
                    {
                        adfSourceGCPs.push_back(
                            CPLAtof(CPLGetXMLValue(psIter, nullptr, "")));
                    }
                }
                for (CPLXMLNode *psIter = psTargetGCPs->psChild;
                     psIter != nullptr; psIter = psIter->psNext)
                {
                    if (psIter->eType == CXT_Element &&
                        EQUAL(psIter->pszValue, "Double"))
                    {
                        adfTargetGCPs.push_back(
                            CPLAtof(CPLGetXMLValue(psIter, nullptr, "")));
                    }
                }
                if (adfSourceGCPs.size() == adfTargetGCPs.size() &&
                    (adfSourceGCPs.size() % 2) == 0)
                {
                    const char *pszESRI_WKT = CPLGetXMLValue(
                        psGeodataXform, "SpatialReference.WKT", nullptr);
                    if (pszESRI_WKT)
                    {
                        m_bLookedForProjection = true;
                        m_oSRS.SetAxisMappingStrategy(
                            OAMS_TRADITIONAL_GIS_ORDER);
                        if (m_oSRS.importFromWkt(pszESRI_WKT) != OGRERR_NONE)
                        {
                            m_oSRS.Clear();
                        }
                    }

                    m_aoGCPs.clear();
                    const size_t nNewGCPCount = adfSourceGCPs.size() / 2;
                    for (size_t i = 0; i < nNewGCPCount; ++i)
                    {
                        m_aoGCPs.emplace_back(
                            "", "",
                            // The origin used is the bottom left corner,
                            // and raw values to be multiplied by the
                            // TIFFTAG_XRESOLUTION/TIFFTAG_YRESOLUTION
                            /* pixel  = */
                            adfSourceGCPs[2 * i] * CPLAtof(pszTIFFTagXRes),
                            /* line = */
                            nRasterYSize - adfSourceGCPs[2 * i + 1] *
                                               CPLAtof(pszTIFFTagYRes),
                            /* X = */ adfTargetGCPs[2 * i],
                            /* Y = */ adfTargetGCPs[2 * i + 1]);
                    }

                    // Invalidate Geotransform got from less prioritary sources
                    if (!m_aoGCPs.empty() && m_bGeoTransformValid &&
                        !bGotGTFromPAM && m_nPAMGeorefSrcIndex == 0)
                    {
                        m_bGeoTransformValid = false;
                    }
                }
            }
        }

        if (psValueAsXML)
            CPLDestroyXMLNode(psValueAsXML);
    }

    /* -------------------------------------------------------------------- */
    /*      Copy any PAM metadata into our GeoTIFF context, and with        */
    /*      the PAM info overriding the GeoTIFF context.                    */
    /* -------------------------------------------------------------------- */
    CSLConstList papszPamDomains = oMDMD.GetDomainList();

    for (int iDomain = 0;
         papszPamDomains && papszPamDomains[iDomain] != nullptr; ++iDomain)
    {
        const char *pszDomain = papszPamDomains[iDomain];
        char **papszGT_MD = CSLDuplicate(m_oGTiffMDMD.GetMetadata(pszDomain));
        char **papszPAM_MD = oMDMD.GetMetadata(pszDomain);

        papszGT_MD = CSLMerge(papszGT_MD, papszPAM_MD);

        m_oGTiffMDMD.SetMetadata(papszGT_MD, pszDomain);
        CSLDestroy(papszGT_MD);
    }

    for (int i = 1; i <= GetRasterCount(); ++i)
    {
        GTiffRasterBand *poBand =
            cpl::down_cast<GTiffRasterBand *>(GetRasterBand(i));
        papszPamDomains = poBand->oMDMD.GetDomainList();

        for (int iDomain = 0;
             papszPamDomains && papszPamDomains[iDomain] != nullptr; ++iDomain)
        {
            const char *pszDomain = papszPamDomains[iDomain];
            char **papszGT_MD =
                CSLDuplicate(poBand->m_oGTiffMDMD.GetMetadata(pszDomain));
            char **papszPAM_MD = poBand->oMDMD.GetMetadata(pszDomain);

            papszGT_MD = CSLMerge(papszGT_MD, papszPAM_MD);

            poBand->m_oGTiffMDMD.SetMetadata(papszGT_MD, pszDomain);
            CSLDestroy(papszGT_MD);
        }
    }

    for (int i = 1; i <= nBands; ++i)
    {
        GTiffRasterBand *poBand =
            cpl::down_cast<GTiffRasterBand *>(GetRasterBand(i));

        /* Load scale, offset and unittype from PAM if available */
        int nHaveOffsetScale = false;
        double dfScale = poBand->GDALPamRasterBand::GetScale(&nHaveOffsetScale);
        if (nHaveOffsetScale)
        {
            poBand->m_bHaveOffsetScale = true;
            poBand->m_dfScale = dfScale;
            poBand->m_dfOffset = poBand->GDALPamRasterBand::GetOffset();
        }

        const char *pszUnitType = poBand->GDALPamRasterBand::GetUnitType();
        if (pszUnitType && pszUnitType[0])
            poBand->m_osUnitType = pszUnitType;

        const char *pszDescription =
            poBand->GDALPamRasterBand::GetDescription();
        if (pszDescription && pszDescription[0])
            poBand->m_osDescription = pszDescription;

        GDALColorInterp ePAMColorInterp =
            poBand->GDALPamRasterBand::GetColorInterpretation();
        if (ePAMColorInterp != GCI_Undefined)
            poBand->m_eBandInterp = ePAMColorInterp;

        if (i == 1)
        {
            auto poCT = poBand->GDALPamRasterBand::GetColorTable();
            if (poCT)
            {
                delete m_poColorTable;
                m_poColorTable = poCT->Clone();
            }
        }
    }
}

/************************************************************************/
/*                              OpenDir()                               */
/*                                                                      */
/*      Open a specific directory as encoded into a filename.           */
/************************************************************************/

GDALDataset *GTiffDataset::OpenDir(GDALOpenInfo *poOpenInfo)

{
    bool bAllowRGBAInterface = true;
    const char *pszFilename = poOpenInfo->pszFilename;
    if (STARTS_WITH_CI(pszFilename, "GTIFF_RAW:"))
    {
        bAllowRGBAInterface = false;
        pszFilename += strlen("GTIFF_RAW:");
    }

    if (!STARTS_WITH_CI(pszFilename, "GTIFF_DIR:") ||
        pszFilename[strlen("GTIFF_DIR:")] == '\0')
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Split out filename, and dir#/offset.                            */
    /* -------------------------------------------------------------------- */
    pszFilename += strlen("GTIFF_DIR:");
    bool bAbsolute = false;

    if (STARTS_WITH_CI(pszFilename, "off:"))
    {
        bAbsolute = true;
        pszFilename += 4;
    }

    toff_t nOffset = atol(pszFilename);
    pszFilename += 1;

    while (*pszFilename != '\0' && pszFilename[-1] != ':')
        ++pszFilename;

    if (*pszFilename == '\0' || nOffset == 0)
    {
        ReportError(
            pszFilename, CE_Failure, CPLE_OpenFailed,
            "Unable to extract offset or filename, should take the form:\n"
            "GTIFF_DIR:<dir>:filename or GTIFF_DIR:off:<dir_offset>:filename");
        return nullptr;
    }

    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportError(pszFilename, CE_Warning, CPLE_AppDefined,
                    "Opening a specific TIFF directory is not supported in "
                    "update mode. Switching to read-only");
    }

    /* -------------------------------------------------------------------- */
    /*      Try opening the dataset.                                        */
    /* -------------------------------------------------------------------- */
    GTiffOneTimeInit();

    const char *pszFlag = poOpenInfo->eAccess == GA_Update ? "r+DC" : "rDOC";
    VSILFILE *l_fpL = VSIFOpenL(pszFilename, pszFlag);
    if (l_fpL == nullptr)
        return nullptr;
    TIFF *l_hTIFF = VSI_TIFFOpen(pszFilename, pszFlag, l_fpL);
    if (l_hTIFF == nullptr)
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      If a directory was requested by index, advance to it now.       */
    /* -------------------------------------------------------------------- */
    if (!bAbsolute)
    {
        const toff_t nOffsetRequested = nOffset;
        while (nOffset > 1)
        {
            if (TIFFReadDirectory(l_hTIFF) == 0)
            {
                XTIFFClose(l_hTIFF);
                ReportError(pszFilename, CE_Failure, CPLE_OpenFailed,
                            "Requested directory %lu not found.",
                            static_cast<long unsigned int>(nOffsetRequested));
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }
            nOffset--;
        }

        nOffset = TIFFCurrentDirOffset(l_hTIFF);
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->m_pszFilename = CPLStrdup(pszFilename);
    poDS->m_fpL = l_fpL;
    poDS->m_hTIFF = l_hTIFF;
    poDS->m_bSingleIFDOpened = true;

    if (!EQUAL(pszFilename, poOpenInfo->pszFilename) &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "GTIFF_RAW:"))
    {
        poDS->SetPhysicalFilename(pszFilename);
        poDS->SetSubdatasetName(poOpenInfo->pszFilename);
    }

    if (poOpenInfo->AreSiblingFilesLoaded())
        poDS->oOvManager.TransferSiblingFiles(poOpenInfo->StealSiblingFiles());

    if (poDS->OpenOffset(l_hTIFF, nOffset, poOpenInfo->eAccess,
                         bAllowRGBAInterface, true) != CE_None)
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                   ConvertTransferFunctionToString()                  */
/*                                                                      */
/*      Convert a transfer function table into a string.                */
/*      Used by LoadICCProfile().                                       */
/************************************************************************/
static CPLString ConvertTransferFunctionToString(const uint16_t *pTable,
                                                 uint32_t nTableEntries)
{
    CPLString sValue;

    for (uint32_t i = 0; i < nTableEntries; ++i)
    {
        if (i > 0)
            sValue += ", ";
        sValue += CPLSPrintf("%d", static_cast<uint32_t>(pTable[i]));
    }

    return sValue;
}

/************************************************************************/
/*                             LoadICCProfile()                         */
/*                                                                      */
/*      Load ICC Profile or colorimetric data into metadata             */
/************************************************************************/

void GTiffDataset::LoadICCProfile()
{
    if (m_bICCMetadataLoaded)
        return;
    m_bICCMetadataLoaded = true;

    uint32_t nEmbedLen = 0;
    uint8_t *pEmbedBuffer = nullptr;

    if (TIFFGetField(m_hTIFF, TIFFTAG_ICCPROFILE, &nEmbedLen, &pEmbedBuffer))
    {
        char *pszBase64Profile = CPLBase64Encode(
            nEmbedLen, reinterpret_cast<const GByte *>(pEmbedBuffer));

        m_oGTiffMDMD.SetMetadataItem("SOURCE_ICC_PROFILE", pszBase64Profile,
                                     "COLOR_PROFILE");

        CPLFree(pszBase64Profile);

        return;
    }

    // Check for colorimetric tiff.
    float *pCHR = nullptr;
    float *pWP = nullptr;
    uint16_t *pTFR = nullptr;
    uint16_t *pTFG = nullptr;
    uint16_t *pTFB = nullptr;
    uint16_t *pTransferRange = nullptr;

    if (TIFFGetField(m_hTIFF, TIFFTAG_PRIMARYCHROMATICITIES, &pCHR))
    {
        if (TIFFGetField(m_hTIFF, TIFFTAG_WHITEPOINT, &pWP))
        {
            if (!TIFFGetFieldDefaulted(m_hTIFF, TIFFTAG_TRANSFERFUNCTION, &pTFR,
                                       &pTFG, &pTFB) ||
                pTFR == nullptr || pTFG == nullptr || pTFB == nullptr)
            {
                return;
            }

            const int TIFFTAG_TRANSFERRANGE = 0x0156;
            TIFFGetFieldDefaulted(m_hTIFF, TIFFTAG_TRANSFERRANGE,
                                  &pTransferRange);

            // Set all the colorimetric metadata.
            m_oGTiffMDMD.SetMetadataItem(
                "SOURCE_PRIMARIES_RED",
                CPLString().Printf("%.9f, %.9f, 1.0",
                                   static_cast<double>(pCHR[0]),
                                   static_cast<double>(pCHR[1])),
                "COLOR_PROFILE");
            m_oGTiffMDMD.SetMetadataItem(
                "SOURCE_PRIMARIES_GREEN",
                CPLString().Printf("%.9f, %.9f, 1.0",
                                   static_cast<double>(pCHR[2]),
                                   static_cast<double>(pCHR[3])),
                "COLOR_PROFILE");
            m_oGTiffMDMD.SetMetadataItem(
                "SOURCE_PRIMARIES_BLUE",
                CPLString().Printf("%.9f, %.9f, 1.0",
                                   static_cast<double>(pCHR[4]),
                                   static_cast<double>(pCHR[5])),
                "COLOR_PROFILE");

            m_oGTiffMDMD.SetMetadataItem(
                "SOURCE_WHITEPOINT",
                CPLString().Printf("%.9f, %.9f, 1.0",
                                   static_cast<double>(pWP[0]),
                                   static_cast<double>(pWP[1])),
                "COLOR_PROFILE");

            // Set transfer function metadata.

            // Get length of table.
            const uint32_t nTransferFunctionLength = 1 << m_nBitsPerSample;

            m_oGTiffMDMD.SetMetadataItem(
                "TIFFTAG_TRANSFERFUNCTION_RED",
                ConvertTransferFunctionToString(pTFR, nTransferFunctionLength),
                "COLOR_PROFILE");

            m_oGTiffMDMD.SetMetadataItem(
                "TIFFTAG_TRANSFERFUNCTION_GREEN",
                ConvertTransferFunctionToString(pTFG, nTransferFunctionLength),
                "COLOR_PROFILE");

            m_oGTiffMDMD.SetMetadataItem(
                "TIFFTAG_TRANSFERFUNCTION_BLUE",
                ConvertTransferFunctionToString(pTFB, nTransferFunctionLength),
                "COLOR_PROFILE");

            // Set transfer range.
            if (pTransferRange)
            {
                m_oGTiffMDMD.SetMetadataItem(
                    "TIFFTAG_TRANSFERRANGE_BLACK",
                    CPLString().Printf("%d, %d, %d",
                                       static_cast<int>(pTransferRange[0]),
                                       static_cast<int>(pTransferRange[2]),
                                       static_cast<int>(pTransferRange[4])),
                    "COLOR_PROFILE");
                m_oGTiffMDMD.SetMetadataItem(
                    "TIFFTAG_TRANSFERRANGE_WHITE",
                    CPLString().Printf("%d, %d, %d",
                                       static_cast<int>(pTransferRange[1]),
                                       static_cast<int>(pTransferRange[3]),
                                       static_cast<int>(pTransferRange[5])),
                    "COLOR_PROFILE");
            }
        }
    }
}

/************************************************************************/
/*                             OpenOffset()                             */
/*                                                                      */
/*      Initialize the GTiffDataset based on a passed in file           */
/*      handle, and directory offset to utilize.  This is called for    */
/*      full res, and overview pages.                                   */
/************************************************************************/

CPLErr GTiffDataset::OpenOffset(TIFF *hTIFFIn, toff_t nDirOffsetIn,
                                GDALAccess eAccessIn, bool bAllowRGBAInterface,
                                bool bReadGeoTransform)

{
    if (!hTIFFIn)
        return CE_Failure;

    eAccess = eAccessIn;

    m_hTIFF = hTIFFIn;

    m_nDirOffset = nDirOffsetIn;

    if (!SetDirectory())
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      Capture some information from the file that is of interest.     */
    /* -------------------------------------------------------------------- */
    uint32_t nXSize = 0;
    uint32_t nYSize = 0;
    TIFFGetField(m_hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize);
    TIFFGetField(m_hTIFF, TIFFTAG_IMAGELENGTH, &nYSize);

    // Unlikely to occur, but could happen on a disk full situation.
    if (nXSize == 0 || nYSize == 0)
        return CE_Failure;

    if (nXSize > INT_MAX || nYSize > INT_MAX)
    {
        // GDAL only supports signed 32bit dimensions.
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Too large image size: %u x %u", nXSize, nYSize);
        return CE_Failure;
    }
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    if (!TIFFGetField(m_hTIFF, TIFFTAG_SAMPLESPERPIXEL, &m_nSamplesPerPixel))
        nBands = 1;
    else
        nBands = m_nSamplesPerPixel;

    if (!TIFFGetField(m_hTIFF, TIFFTAG_BITSPERSAMPLE, &(m_nBitsPerSample)))
        m_nBitsPerSample = 1;

    if (!TIFFGetField(m_hTIFF, TIFFTAG_PLANARCONFIG, &(m_nPlanarConfig)))
        m_nPlanarConfig = PLANARCONFIG_CONTIG;

    if (!TIFFGetField(m_hTIFF, TIFFTAG_PHOTOMETRIC, &(m_nPhotometric)))
        m_nPhotometric = PHOTOMETRIC_MINISBLACK;

    if (!TIFFGetField(m_hTIFF, TIFFTAG_SAMPLEFORMAT, &(m_nSampleFormat)))
        m_nSampleFormat = SAMPLEFORMAT_UINT;

    if (!TIFFGetField(m_hTIFF, TIFFTAG_COMPRESSION, &(m_nCompression)))
        m_nCompression = COMPRESSION_NONE;

    if (m_nCompression != COMPRESSION_NONE &&
        !TIFFIsCODECConfigured(m_nCompression))
    {
        const char *pszCompressionMethodName =
            GTIFFGetCompressionMethodName(m_nCompression);
        if (pszCompressionMethodName)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot open TIFF file due to missing codec %s.",
                        pszCompressionMethodName);
        }
        else
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Cannot open TIFF file due to missing codec of code %d.",
                m_nCompression);
        }
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      YCbCr JPEG compressed images should be translated on the fly    */
    /*      to RGB by libtiff/libjpeg unless specifically requested         */
    /*      otherwise.                                                      */
    /* -------------------------------------------------------------------- */
    if (m_nCompression == COMPRESSION_JPEG &&
        m_nPhotometric == PHOTOMETRIC_YCBCR &&
        CPLTestBool(CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES")))
    {
        m_oGTiffMDMD.SetMetadataItem("SOURCE_COLOR_SPACE", "YCbCr",
                                     "IMAGE_STRUCTURE");
        int nColorMode = 0;
        if (!TIFFGetField(m_hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode) ||
            nColorMode != JPEGCOLORMODE_RGB)
        {
            TIFFSetField(m_hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Get strip/tile layout.                                          */
    /* -------------------------------------------------------------------- */
    if (TIFFIsTiled(m_hTIFF))
    {
        uint32_t l_nBlockXSize = 0;
        uint32_t l_nBlockYSize = 0;
        TIFFGetField(m_hTIFF, TIFFTAG_TILEWIDTH, &(l_nBlockXSize));
        TIFFGetField(m_hTIFF, TIFFTAG_TILELENGTH, &(l_nBlockYSize));
        if (l_nBlockXSize > INT_MAX || l_nBlockYSize > INT_MAX)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Too large block size: %u x %u", l_nBlockXSize,
                        l_nBlockYSize);
            return CE_Failure;
        }
        m_nBlockXSize = static_cast<int>(l_nBlockXSize);
        m_nBlockYSize = static_cast<int>(l_nBlockYSize);
    }
    else
    {
        if (!TIFFGetField(m_hTIFF, TIFFTAG_ROWSPERSTRIP, &(m_nRowsPerStrip)))
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "RowsPerStrip not defined ... assuming all one strip.");
            m_nRowsPerStrip = nYSize;  // Dummy value.
        }

        // If the rows per strip is larger than the file we will get
        // confused.  libtiff internally will treat the rowsperstrip as
        // the image height and it is best if we do too. (#4468)
        if (m_nRowsPerStrip > static_cast<uint32_t>(nRasterYSize))
            m_nRowsPerStrip = nRasterYSize;

        m_nBlockXSize = nRasterXSize;
        m_nBlockYSize = m_nRowsPerStrip;
    }

    if (!ComputeBlocksPerColRowAndBand(nBands))
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      Should we handle this using the GTiffBitmapBand?                */
    /* -------------------------------------------------------------------- */
    bool bTreatAsBitmap = false;

    if (m_nBitsPerSample == 1 && nBands == 1)
    {
        bTreatAsBitmap = true;

        // Lets treat large "one row" bitmaps using the scanline api.
        if (!TIFFIsTiled(m_hTIFF) && m_nBlockYSize == nRasterYSize &&
            nRasterYSize > 2000
            // libtiff does not support reading JBIG files with
            // TIFFReadScanline().
            && m_nCompression != COMPRESSION_JBIG)
        {
            m_bTreatAsSplitBitmap = true;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Should we treat this via the RGBA interface?                    */
    /* -------------------------------------------------------------------- */
    bool bTreatAsRGBA = false;
    if (
#ifdef DEBUG
        CPLTestBool(CPLGetConfigOption("GTIFF_FORCE_RGBA", "NO")) ||
#endif
        (bAllowRGBAInterface && !bTreatAsBitmap && !(m_nBitsPerSample > 8) &&
         (m_nPhotometric == PHOTOMETRIC_CIELAB ||
          m_nPhotometric == PHOTOMETRIC_LOGL ||
          m_nPhotometric == PHOTOMETRIC_LOGLUV ||
          m_nPhotometric == PHOTOMETRIC_SEPARATED ||
          (m_nPhotometric == PHOTOMETRIC_YCBCR &&
           m_nCompression != COMPRESSION_JPEG))))
    {
        char szMessage[1024] = {};

        if (TIFFRGBAImageOK(m_hTIFF, szMessage) == 1)
        {
            const char *pszSourceColorSpace = nullptr;
            nBands = 4;
            switch (m_nPhotometric)
            {
                case PHOTOMETRIC_CIELAB:
                    pszSourceColorSpace = "CIELAB";
                    break;
                case PHOTOMETRIC_LOGL:
                    pszSourceColorSpace = "LOGL";
                    break;
                case PHOTOMETRIC_LOGLUV:
                    pszSourceColorSpace = "LOGLUV";
                    break;
                case PHOTOMETRIC_SEPARATED:
                    pszSourceColorSpace = "CMYK";
                    break;
                case PHOTOMETRIC_YCBCR:
                    pszSourceColorSpace = "YCbCr";
                    nBands = 3;  // probably true for other photometric values
                    break;
            }
            if (pszSourceColorSpace)
                m_oGTiffMDMD.SetMetadataItem("SOURCE_COLOR_SPACE",
                                             pszSourceColorSpace,
                                             "IMAGE_STRUCTURE");
            bTreatAsRGBA = true;
        }
        else
        {
            CPLDebug("GTiff", "TIFFRGBAImageOK says:\n%s", szMessage);
        }
    }

    // libtiff has various issues with OJPEG compression and chunky-strip
    // support with the "classic" scanline/strip/tile interfaces, and that
    // wouldn't work either, so better bail out.
    if (m_nCompression == COMPRESSION_OJPEG && !bTreatAsRGBA)
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Old-JPEG compression only supported through RGBA interface, "
            "which cannot be used probably because the file is corrupted");
        return CE_Failure;
    }

    // If photometric is YCbCr, scanline/strip/tile interfaces assumes that
    // we are ready with downsampled data. And we are not.
    if (m_nCompression != COMPRESSION_JPEG &&
        m_nCompression != COMPRESSION_OJPEG &&
        m_nPhotometric == PHOTOMETRIC_YCBCR &&
        m_nPlanarConfig == PLANARCONFIG_CONTIG && !bTreatAsRGBA)
    {
        uint16_t nF1, nF2;
        TIFFGetFieldDefaulted(m_hTIFF, TIFFTAG_YCBCRSUBSAMPLING, &nF1, &nF2);
        if (nF1 != 1 || nF2 != 1)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot open TIFF file with YCbCr, subsampling and "
                        "BitsPerSample > 8 that is not JPEG compressed");
            return CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Should we treat this via the split interface?                   */
    /* -------------------------------------------------------------------- */
    if (!TIFFIsTiled(m_hTIFF) && m_nBitsPerSample == 8 &&
        m_nBlockYSize == nRasterYSize && nRasterYSize > 2000 && !bTreatAsRGBA &&
        CPLTestBool(CPLGetConfigOption("GDAL_ENABLE_TIFF_SPLIT", "YES")))
    {
        m_bTreatAsSplit = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Should we treat this via the odd bits interface?                */
    /* -------------------------------------------------------------------- */
    bool bTreatAsOdd = false;
    if (m_nSampleFormat == SAMPLEFORMAT_IEEEFP)
    {
        if (m_nBitsPerSample == 16 || m_nBitsPerSample == 24)
            bTreatAsOdd = true;
        else if (m_nBitsPerSample != 32 && m_nBitsPerSample != 64)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot open TIFF file with SampleFormat=IEEEFP "
                        "and BitsPerSample=%d",
                        m_nBitsPerSample);
            return CE_Failure;
        }
    }
    else if (!bTreatAsRGBA && !bTreatAsBitmap && m_nBitsPerSample != 8 &&
             m_nBitsPerSample != 16 && m_nBitsPerSample != 32 &&
             m_nBitsPerSample != 64 && m_nBitsPerSample != 128)
    {
        bTreatAsOdd = true;
    }

/* -------------------------------------------------------------------- */
/*      We can't support 'chunks' bigger than 2GB on 32 bit builds      */
/* -------------------------------------------------------------------- */
#if SIZEOF_VOIDP == 4
    uint64_t nChunkSize = 0;
    if (m_bTreatAsSplit || m_bTreatAsSplitBitmap)
    {
        nChunkSize = TIFFScanlineSize64(m_hTIFF);
    }
    else
    {
        if (TIFFIsTiled(m_hTIFF))
            nChunkSize = TIFFTileSize64(m_hTIFF);
        else
            nChunkSize = TIFFStripSize64(m_hTIFF);
    }
    if (bTreatAsRGBA)
    {
        nChunkSize =
            std::max(nChunkSize,
                     4 * static_cast<uint64_t>(m_nBlockXSize) * m_nBlockYSize);
    }
    if (nChunkSize > static_cast<uint64_t>(INT_MAX))
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Scanline/tile/strip size bigger than 2GB unsupported "
                    "on 32-bit builds.");
        return CE_Failure;
    }
#endif

    const bool bMinIsWhite = m_nPhotometric == PHOTOMETRIC_MINISWHITE;

    /* -------------------------------------------------------------------- */
    /*      Check for NODATA                                                */
    /* -------------------------------------------------------------------- */
    char *pszText = nullptr;
    if (TIFFGetField(m_hTIFF, TIFFTAG_GDAL_NODATA, &pszText) &&
        !EQUAL(pszText, ""))
    {
        if (m_nBitsPerSample > 32 && m_nBitsPerSample <= 64 &&
            m_nSampleFormat == SAMPLEFORMAT_INT)
        {
            m_bNoDataSetAsInt64 = true;
            m_nNoDataValueInt64 =
                static_cast<int64_t>(std::strtoll(pszText, nullptr, 10));
        }
        else if (m_nBitsPerSample > 32 && m_nBitsPerSample <= 64 &&
                 m_nSampleFormat == SAMPLEFORMAT_UINT)
        {
            m_bNoDataSetAsUInt64 = true;
            m_nNoDataValueUInt64 =
                static_cast<uint64_t>(std::strtoull(pszText, nullptr, 10));
        }
        else
        {
            m_bNoDataSet = true;
            m_dfNoDataValue = CPLAtofM(pszText);
            if (m_nBitsPerSample == 32 &&
                m_nSampleFormat == SAMPLEFORMAT_IEEEFP)
            {
                m_dfNoDataValue =
                    GDALAdjustNoDataCloseToFloatMax(m_dfNoDataValue);
                m_dfNoDataValue = static_cast<float>(m_dfNoDataValue);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Capture the color table if there is one.                        */
    /* -------------------------------------------------------------------- */
    unsigned short *panRed = nullptr;
    unsigned short *panGreen = nullptr;
    unsigned short *panBlue = nullptr;

    if (bTreatAsRGBA || m_nBitsPerSample > 16 ||
        TIFFGetField(m_hTIFF, TIFFTAG_COLORMAP, &panRed, &panGreen, &panBlue) ==
            0)
    {
        // Build inverted palette if we have inverted photometric.
        // Pixel values remains unchanged.  Avoid doing this for *deep*
        // data types (per #1882)
        if (m_nBitsPerSample <= 16 && m_nPhotometric == PHOTOMETRIC_MINISWHITE)
        {
            m_poColorTable = new GDALColorTable();
            const int nColorCount = 1 << m_nBitsPerSample;

            for (int iColor = 0; iColor < nColorCount; ++iColor)
            {
                const short nValue = static_cast<short>(
                    ((255 * (nColorCount - 1 - iColor)) / (nColorCount - 1)));
                const GDALColorEntry oEntry = {nValue, nValue, nValue,
                                               static_cast<short>(255)};
                m_poColorTable->SetColorEntry(iColor, &oEntry);
            }

            m_nPhotometric = PHOTOMETRIC_PALETTE;
        }
        else
        {
            m_poColorTable = nullptr;
        }
    }
    else
    {
        unsigned short nMaxColor = 0;

        m_poColorTable = new GDALColorTable();

        const int nColorCount = 1 << m_nBitsPerSample;

        for (int iColor = nColorCount - 1; iColor >= 0; iColor--)
        {
            // TODO(schwehr): Ensure the color entries are never negative?
            const unsigned short divisor = 257;
            const GDALColorEntry oEntry = {
                static_cast<short>(panRed[iColor] / divisor),
                static_cast<short>(panGreen[iColor] / divisor),
                static_cast<short>(panBlue[iColor] / divisor),
                static_cast<short>(
                    m_bNoDataSet && static_cast<int>(m_dfNoDataValue) == iColor
                        ? 0
                        : 255)};

            m_poColorTable->SetColorEntry(iColor, &oEntry);

            nMaxColor = std::max(nMaxColor, panRed[iColor]);
            nMaxColor = std::max(nMaxColor, panGreen[iColor]);
            nMaxColor = std::max(nMaxColor, panBlue[iColor]);
        }

        // Bug 1384 - Some TIFF files are generated with color map entry
        // values in range 0-255 instead of 0-65535 - try to handle these
        // gracefully.
        if (nMaxColor > 0 && nMaxColor < 256)
        {
            CPLDebug(
                "GTiff",
                "TIFF ColorTable seems to be improperly scaled, fixing up.");

            for (int iColor = nColorCount - 1; iColor >= 0; iColor--)
            {
                // TODO(schwehr): Ensure the color entries are never negative?
                const GDALColorEntry oEntry = {
                    static_cast<short>(panRed[iColor]),
                    static_cast<short>(panGreen[iColor]),
                    static_cast<short>(panBlue[iColor]),
                    m_bNoDataSet && static_cast<int>(m_dfNoDataValue) == iColor
                        ? static_cast<short>(0)
                        : static_cast<short>(255)};

                m_poColorTable->SetColorEntry(iColor, &oEntry);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    for (int iBand = 0; iBand < nBands; ++iBand)
    {
        if (bTreatAsRGBA)
            SetBand(iBand + 1, new GTiffRGBABand(this, iBand + 1));
        else if (m_bTreatAsSplitBitmap)
            SetBand(iBand + 1, new GTiffSplitBitmapBand(this, iBand + 1));
        else if (m_bTreatAsSplit)
            SetBand(iBand + 1, new GTiffSplitBand(this, iBand + 1));
        else if (bTreatAsBitmap)
            SetBand(iBand + 1, new GTiffBitmapBand(this, iBand + 1));
        else if (bTreatAsOdd)
            SetBand(iBand + 1, new GTiffOddBitsBand(this, iBand + 1));
        else
            SetBand(iBand + 1, new GTiffRasterBand(this, iBand + 1));
    }

    if (GetRasterBand(1)->GetRasterDataType() == GDT_Unknown)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Unsupported TIFF configuration: BitsPerSample(=%d) and "
                    "SampleType(=%d)",
                    m_nBitsPerSample, m_nSampleFormat);
        return CE_Failure;
    }

    m_bReadGeoTransform = bReadGeoTransform;

    /* -------------------------------------------------------------------- */
    /*      Capture some other potentially interesting information.         */
    /* -------------------------------------------------------------------- */
    char szWorkMDI[200] = {};
    uint16_t nShort = 0;

    const auto *pasTIFFTags = GetTIFFTags();
    for (size_t iTag = 0; pasTIFFTags[iTag].pszTagName; ++iTag)
    {
        if (pasTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING)
        {
            if (TIFFGetField(m_hTIFF, pasTIFFTags[iTag].nTagVal, &pszText))
                m_oGTiffMDMD.SetMetadataItem(pasTIFFTags[iTag].pszTagName,
                                             pszText);
        }
        else if (pasTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT)
        {
            float fVal = 0.0;
            if (TIFFGetField(m_hTIFF, pasTIFFTags[iTag].nTagVal, &fVal))
            {
                CPLsnprintf(szWorkMDI, sizeof(szWorkMDI), "%.8g", fVal);
                m_oGTiffMDMD.SetMetadataItem(pasTIFFTags[iTag].pszTagName,
                                             szWorkMDI);
            }
        }
        else if (pasTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT &&
                 pasTIFFTags[iTag].nTagVal != TIFFTAG_RESOLUTIONUNIT)
        {
            if (TIFFGetField(m_hTIFF, pasTIFFTags[iTag].nTagVal, &nShort))
            {
                snprintf(szWorkMDI, sizeof(szWorkMDI), "%d", nShort);
                m_oGTiffMDMD.SetMetadataItem(pasTIFFTags[iTag].pszTagName,
                                             szWorkMDI);
            }
        }
        else if (pasTIFFTags[iTag].eType == GTIFFTAGTYPE_BYTE_STRING)
        {
            uint32_t nCount = 0;
            if (TIFFGetField(m_hTIFF, pasTIFFTags[iTag].nTagVal, &nCount,
                             &pszText))
            {
                std::string osStr;
                osStr.assign(pszText, nCount);
                m_oGTiffMDMD.SetMetadataItem(pasTIFFTags[iTag].pszTagName,
                                             osStr.c_str());
            }
        }
    }

    if (TIFFGetField(m_hTIFF, TIFFTAG_RESOLUTIONUNIT, &nShort))
    {
        if (nShort == RESUNIT_NONE)
            snprintf(szWorkMDI, sizeof(szWorkMDI), "%d (unitless)", nShort);
        else if (nShort == RESUNIT_INCH)
            snprintf(szWorkMDI, sizeof(szWorkMDI), "%d (pixels/inch)", nShort);
        else if (nShort == RESUNIT_CENTIMETER)
            snprintf(szWorkMDI, sizeof(szWorkMDI), "%d (pixels/cm)", nShort);
        else
            snprintf(szWorkMDI, sizeof(szWorkMDI), "%d", nShort);
        m_oGTiffMDMD.SetMetadataItem("TIFFTAG_RESOLUTIONUNIT", szWorkMDI);
    }

    int nTagSize = 0;
    void *pData = nullptr;
    if (TIFFGetField(m_hTIFF, TIFFTAG_XMLPACKET, &nTagSize, &pData))
    {
        char *pszXMP = static_cast<char *>(VSI_MALLOC_VERBOSE(nTagSize + 1));
        if (pszXMP)
        {
            memcpy(pszXMP, pData, nTagSize);
            pszXMP[nTagSize] = '\0';

            char *apszMDList[2] = {pszXMP, nullptr};
            m_oGTiffMDMD.SetMetadata(apszMDList, "xml:XMP");

            CPLFree(pszXMP);
        }
    }

    if (m_nCompression != COMPRESSION_NONE)
    {
        const char *pszCompressionMethodName =
            GTIFFGetCompressionMethodName(m_nCompression);
        if (pszCompressionMethodName)
        {
            m_oGTiffMDMD.SetMetadataItem(
                "COMPRESSION", pszCompressionMethodName, "IMAGE_STRUCTURE");
        }
        else
        {
            CPLString oComp;
            oComp.Printf("%d", m_nCompression);
            m_oGTiffMDMD.SetMetadataItem("COMPRESSION", oComp.c_str());
        }
    }

    if (m_nCompression == COMPRESSION_JPEG &&
        m_nPhotometric == PHOTOMETRIC_YCBCR)
    {
        m_oGTiffMDMD.SetMetadataItem("COMPRESSION", "YCbCr JPEG",
                                     "IMAGE_STRUCTURE");
    }
    else if (m_nCompression == COMPRESSION_LERC)
    {
        uint32_t nLercParamCount = 0;
        uint32_t *panLercParams = nullptr;
        if (TIFFGetField(m_hTIFF, TIFFTAG_LERC_PARAMETERS, &nLercParamCount,
                         &panLercParams) &&
            nLercParamCount == 2)
        {
            memcpy(m_anLercAddCompressionAndVersion, panLercParams,
                   sizeof(m_anLercAddCompressionAndVersion));
        }

        uint32_t nAddVersion = LERC_ADD_COMPRESSION_NONE;
        if (TIFFGetField(m_hTIFF, TIFFTAG_LERC_ADD_COMPRESSION, &nAddVersion) &&
            nAddVersion != LERC_ADD_COMPRESSION_NONE)
        {
            if (nAddVersion == LERC_ADD_COMPRESSION_DEFLATE)
            {
                m_oGTiffMDMD.SetMetadataItem("COMPRESSION", "LERC_DEFLATE",
                                             "IMAGE_STRUCTURE");
            }
            else if (nAddVersion == LERC_ADD_COMPRESSION_ZSTD)
            {
                m_oGTiffMDMD.SetMetadataItem("COMPRESSION", "LERC_ZSTD",
                                             "IMAGE_STRUCTURE");
            }
        }
        uint32_t nLercVersion = LERC_VERSION_2_4;
        if (TIFFGetField(m_hTIFF, TIFFTAG_LERC_VERSION, &nLercVersion))
        {
            if (nLercVersion == LERC_VERSION_2_4)
            {
                m_oGTiffMDMD.SetMetadataItem("LERC_VERSION", "2.4",
                                             "IMAGE_STRUCTURE");
            }
            else
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                            "Unknown Lerc version: %d", nLercVersion);
            }
        }
    }

    if (m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands != 1)
        m_oGTiffMDMD.SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    else
        m_oGTiffMDMD.SetMetadataItem("INTERLEAVE", "BAND", "IMAGE_STRUCTURE");

    if ((GetRasterBand(1)->GetRasterDataType() == GDT_Byte &&
         m_nBitsPerSample != 8) ||
        (GetRasterBand(1)->GetRasterDataType() == GDT_UInt16 &&
         m_nBitsPerSample != 16) ||
        ((GetRasterBand(1)->GetRasterDataType() == GDT_UInt32 ||
          GetRasterBand(1)->GetRasterDataType() == GDT_Float32) &&
         m_nBitsPerSample != 32))
    {
        for (int i = 0; i < nBands; ++i)
            cpl::down_cast<GTiffRasterBand *>(GetRasterBand(i + 1))
                ->m_oGTiffMDMD.SetMetadataItem(
                    "NBITS",
                    CPLString().Printf("%d",
                                       static_cast<int>(m_nBitsPerSample)),
                    "IMAGE_STRUCTURE");
    }

    if (bMinIsWhite)
        m_oGTiffMDMD.SetMetadataItem("MINISWHITE", "YES", "IMAGE_STRUCTURE");

    if (TIFFGetField(m_hTIFF, TIFFTAG_GDAL_METADATA, &pszText))
    {
        CPLXMLNode *psRoot = CPLParseXMLString(pszText);
        const CPLXMLNode *psItem =
            psRoot ? CPLGetXMLNode(psRoot, "=GDALMetadata") : nullptr;
        if (psItem)
            psItem = psItem->psChild;
        bool bMaxZErrorFound = false;
        bool bMaxZErrorOverviewFound = false;
        for (; psItem != nullptr; psItem = psItem->psNext)
        {

            if (psItem->eType != CXT_Element ||
                !EQUAL(psItem->pszValue, "Item"))
                continue;

            const char *pszKey = CPLGetXMLValue(psItem, "name", nullptr);
            const char *pszValue = CPLGetXMLValue(psItem, nullptr, nullptr);
            int nBand = atoi(CPLGetXMLValue(psItem, "sample", "-1"));
            if (nBand < -1 || nBand > 65535)
                continue;
            nBand++;
            const char *pszRole = CPLGetXMLValue(psItem, "role", "");
            const char *pszDomain = CPLGetXMLValue(psItem, "domain", "");

            if (pszKey == nullptr || pszValue == nullptr)
                continue;
            if (EQUAL(pszDomain, "IMAGE_STRUCTURE"))
            {
                if (m_nCompression == COMPRESSION_WEBP &&
                    EQUAL(pszKey, "COMPRESSION_REVERSIBILITY"))
                {
                    if (EQUAL(pszValue, "LOSSLESS"))
                        m_bWebPLossless = true;
                    else if (EQUAL(pszValue, "LOSSY"))
                        m_bWebPLossless = false;
                }
                else if (m_nCompression == COMPRESSION_WEBP &&
                         EQUAL(pszKey, "WEBP_LEVEL"))
                {
                    const int nLevel = atoi(pszValue);
                    if (nLevel >= 1 && nLevel <= 100)
                    {
                        m_oGTiffMDMD.SetMetadataItem(
                            "COMPRESSION_REVERSIBILITY", "LOSSY",
                            "IMAGE_STRUCTURE");
                        m_bWebPLossless = false;
                        m_nWebPLevel = static_cast<signed char>(nLevel);
                    }
                }
                else if (m_nCompression == COMPRESSION_LERC &&
                         EQUAL(pszKey, "MAX_Z_ERROR"))
                {
                    bMaxZErrorFound = true;
                    m_dfMaxZError = CPLAtof(pszValue);
                }
                else if (m_nCompression == COMPRESSION_LERC &&
                         EQUAL(pszKey, "MAX_Z_ERROR_OVERVIEW"))
                {
                    bMaxZErrorOverviewFound = true;
                    m_dfMaxZErrorOverview = CPLAtof(pszValue);
                }
#if HAVE_JXL
                else if (m_nCompression == COMPRESSION_JXL &&
                         EQUAL(pszKey, "COMPRESSION_REVERSIBILITY"))
                {
                    if (EQUAL(pszValue, "LOSSLESS"))
                        m_bJXLLossless = true;
                    else if (EQUAL(pszValue, "LOSSY"))
                        m_bJXLLossless = false;
                }
                else if (m_nCompression == COMPRESSION_JXL &&
                         EQUAL(pszKey, "JXL_DISTANCE"))
                {
                    const double dfVal = CPLAtof(pszValue);
                    if (dfVal > 0 && dfVal <= 15)
                    {
                        m_oGTiffMDMD.SetMetadataItem(
                            "COMPRESSION_REVERSIBILITY", "LOSSY",
                            "IMAGE_STRUCTURE");
                        m_bJXLLossless = false;
                        m_fJXLDistance = static_cast<float>(dfVal);
                    }
                }
                else if (m_nCompression == COMPRESSION_JXL &&
                         EQUAL(pszKey, "JXL_ALPHA_DISTANCE"))
                {
                    const double dfVal = CPLAtof(pszValue);
                    if (dfVal > 0 && dfVal <= 15)
                    {
                        m_oGTiffMDMD.SetMetadataItem(
                            "COMPRESSION_REVERSIBILITY", "LOSSY",
                            "IMAGE_STRUCTURE");
                        m_fJXLAlphaDistance = static_cast<float>(dfVal);
                    }
                }
                else if (m_nCompression == COMPRESSION_JXL &&
                         EQUAL(pszKey, "JXL_EFFORT"))
                {
                    const int nEffort = atoi(pszValue);
                    if (nEffort >= 1 && nEffort <= 9)
                    {
                        m_nJXLEffort = nEffort;
                    }
                }
#endif
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
                    m_oGTiffMDMD.SetMetadata(apszMD, pszDomain);
                }
                else
                {
                    m_oGTiffMDMD.SetMetadataItem(pszKey, pszUnescapedValue,
                                                 pszDomain);
                }
            }
            else
            {
                GTiffRasterBand *poBand =
                    cpl::down_cast<GTiffRasterBand *>(GetRasterBand(nBand));
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
                        poBand->m_dfOffset = CPLAtofM(pszUnescapedValue);
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
                        poBand->m_eBandInterp =
                            GDALGetColorInterpretationByName(pszUnescapedValue);
                    }
                    else
                    {
                        if (bIsXML)
                        {
                            char *apszMD[2] = {pszUnescapedValue, nullptr};
                            poBand->m_oGTiffMDMD.SetMetadata(apszMD, pszDomain);
                        }
                        else
                        {
                            poBand->m_oGTiffMDMD.SetMetadataItem(
                                pszKey, pszUnescapedValue, pszDomain);
                        }
                    }
                }
            }
            CPLFree(pszUnescapedValue);
        }

        if (bMaxZErrorFound && !bMaxZErrorOverviewFound)
        {
            m_dfMaxZErrorOverview = m_dfMaxZError;
        }

        CPLDestroyXMLNode(psRoot);
    }

    if (m_bStreamingIn)
    {
        toff_t *panOffsets = nullptr;
        TIFFGetField(m_hTIFF,
                     TIFFIsTiled(m_hTIFF) ? TIFFTAG_TILEOFFSETS
                                          : TIFFTAG_STRIPOFFSETS,
                     &panOffsets);
        if (panOffsets)
        {
            int nBlockCount = TIFFIsTiled(m_hTIFF)
                                  ? TIFFNumberOfTiles(m_hTIFF)
                                  : TIFFNumberOfStrips(m_hTIFF);
            for (int i = 1; i < nBlockCount; ++i)
            {
                if (panOffsets[i] < panOffsets[i - 1])
                {
                    m_oGTiffMDMD.SetMetadataItem("UNORDERED_BLOCKS", "YES",
                                                 "TIFF");
                    CPLDebug("GTIFF",
                             "Offset of block %d is lower than previous block. "
                             "Reader must be careful",
                             i);
                    break;
                }
            }
        }
    }

    if (m_nCompression == COMPRESSION_JPEG)
    {
        bool bHasQuantizationTable = false;
        bool bHasHuffmanTable = false;
        int nQuality =
            GuessJPEGQuality(bHasQuantizationTable, bHasHuffmanTable);
        if (nQuality > 0)
        {
            m_oGTiffMDMD.SetMetadataItem(
                "JPEG_QUALITY", CPLSPrintf("%d", nQuality), "IMAGE_STRUCTURE");
            int nJpegTablesMode = JPEGTABLESMODE_QUANT;
            if (bHasHuffmanTable)
            {
                nJpegTablesMode |= JPEGTABLESMODE_HUFF;
            }
            m_oGTiffMDMD.SetMetadataItem("JPEGTABLESMODE",
                                         CPLSPrintf("%d", nJpegTablesMode),
                                         "IMAGE_STRUCTURE");
        }
        if (eAccess == GA_Update)
        {
            SetJPEGQualityAndTablesModeFromFile(nQuality, bHasQuantizationTable,
                                                bHasHuffmanTable);
        }
    }
    else if (eAccess == GA_Update &&
             m_oGTiffMDMD.GetMetadataItem("COMPRESSION_REVERSIBILITY",
                                          "IMAGE_STRUCTURE") == nullptr)
    {
        if (m_nCompression == COMPRESSION_WEBP)
        {
            const char *pszReversibility =
                GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE");
            if (pszReversibility && strstr(pszReversibility, "LOSSLESS"))
            {
                m_bWebPLossless = true;
            }
            else if (pszReversibility && strstr(pszReversibility, "LOSSY"))
            {
                m_bWebPLossless = false;
            }
        }
#ifdef HAVE_JXL
        else if (m_nCompression == COMPRESSION_JXL)
        {
            const char *pszReversibility =
                GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE");
            if (pszReversibility && strstr(pszReversibility, "LOSSLESS"))
            {
                m_bJXLLossless = true;
            }
            else if (pszReversibility && strstr(pszReversibility, "LOSSY"))
            {
                m_bJXLLossless = false;
            }
        }
#endif
    }

    if (GTIFFSupportsPredictor(m_nCompression))
    {
        uint16_t nPredictor = 0;
        if (TIFFGetField(m_hTIFF, TIFFTAG_PREDICTOR, &nPredictor) &&
            nPredictor > 1)
        {
            m_oGTiffMDMD.SetMetadataItem(
                "PREDICTOR", CPLSPrintf("%d", nPredictor), "IMAGE_STRUCTURE");
        }
    }

    CPLAssert(m_bReadGeoTransform == bReadGeoTransform);
    CPLAssert(!m_bMetadataChanged);
    m_bMetadataChanged = false;

    return CE_None;
}

/************************************************************************/
/*                         GetSiblingFiles()                            */
/************************************************************************/

char **GTiffDataset::GetSiblingFiles()
{
    if (m_bHasGotSiblingFiles)
    {
        return oOvManager.GetSiblingFiles();
    }

    m_bHasGotSiblingFiles = true;
    const int nMaxFiles =
        atoi(CPLGetConfigOption("GDAL_READDIR_LIMIT_ON_OPEN", "1000"));
    char **papszSiblingFiles =
        VSIReadDirEx(CPLGetDirname(m_pszFilename), nMaxFiles);
    if (nMaxFiles > 0 && CSLCount(papszSiblingFiles) > nMaxFiles)
    {
        CPLDebug("GTiff", "GDAL_READDIR_LIMIT_ON_OPEN reached on %s",
                 CPLGetDirname(m_pszFilename));
        CSLDestroy(papszSiblingFiles);
        papszSiblingFiles = nullptr;
    }
    oOvManager.TransferSiblingFiles(papszSiblingFiles);

    return papszSiblingFiles;
}

/************************************************************************/
/*                   IdentifyAuthorizedGeoreferencingSources()          */
/************************************************************************/

void GTiffDataset::IdentifyAuthorizedGeoreferencingSources()
{
    if (m_bHasIdentifiedAuthorizedGeoreferencingSources)
        return;
    m_bHasIdentifiedAuthorizedGeoreferencingSources = true;
    CPLString osGeorefSources = CSLFetchNameValueDef(
        papszOpenOptions, "GEOREF_SOURCES",
        CPLGetConfigOption("GDAL_GEOREF_SOURCES",
                           "PAM,INTERNAL,TABFILE,WORLDFILE,XML"));
    char **papszTokens = CSLTokenizeString2(osGeorefSources, ",", 0);
    m_nPAMGeorefSrcIndex =
        static_cast<signed char>(CSLFindString(papszTokens, "PAM"));
    m_nINTERNALGeorefSrcIndex =
        static_cast<signed char>(CSLFindString(papszTokens, "INTERNAL"));
    m_nTABFILEGeorefSrcIndex =
        static_cast<signed char>(CSLFindString(papszTokens, "TABFILE"));
    m_nWORLDFILEGeorefSrcIndex =
        static_cast<signed char>(CSLFindString(papszTokens, "WORLDFILE"));
    m_nXMLGeorefSrcIndex =
        static_cast<signed char>(CSLFindString(papszTokens, "XML"));
    CSLDestroy(papszTokens);
}

/************************************************************************/
/*                     LoadGeoreferencingAndPamIfNeeded()               */
/************************************************************************/

void GTiffDataset::LoadGeoreferencingAndPamIfNeeded()

{
    if (!m_bReadGeoTransform && !m_bLoadPam)
        return;

    IdentifyAuthorizedGeoreferencingSources();

    /* -------------------------------------------------------------------- */
    /*      Get the transform or gcps from the GeoTIFF file.                */
    /* -------------------------------------------------------------------- */
    if (m_bReadGeoTransform)
    {
        m_bReadGeoTransform = false;

        char *pszTabWKT = nullptr;
        double *padfTiePoints = nullptr;
        double *padfScale = nullptr;
        double *padfMatrix = nullptr;
        uint16_t nCount = 0;
        bool bPixelIsPoint = false;
        unsigned short nRasterType = 0;
        bool bPointGeoIgnore = false;

        std::set<signed char> aoSetPriorities;
        if (m_nINTERNALGeorefSrcIndex >= 0)
            aoSetPriorities.insert(m_nINTERNALGeorefSrcIndex);
        if (m_nTABFILEGeorefSrcIndex >= 0)
            aoSetPriorities.insert(m_nTABFILEGeorefSrcIndex);
        if (m_nWORLDFILEGeorefSrcIndex >= 0)
            aoSetPriorities.insert(m_nWORLDFILEGeorefSrcIndex);
        for (const auto nIndex : aoSetPriorities)
        {
            if (m_nINTERNALGeorefSrcIndex == nIndex)
            {
                GTIF *psGTIF =
                    GTiffDataset::GTIFNew(m_hTIFF);  // How expensive this is?

                if (psGTIF)
                {
                    if (GDALGTIFKeyGetSHORT(psGTIF, GTRasterTypeGeoKey,
                                            &nRasterType, 0, 1) == 1 &&
                        nRasterType == static_cast<short>(RasterPixelIsPoint))
                    {
                        bPixelIsPoint = true;
                        bPointGeoIgnore = CPLTestBool(CPLGetConfigOption(
                            "GTIFF_POINT_GEO_IGNORE", "FALSE"));
                    }

                    GTIFFree(psGTIF);
                }

                m_adfGeoTransform[0] = 0.0;
                m_adfGeoTransform[1] = 1.0;
                m_adfGeoTransform[2] = 0.0;
                m_adfGeoTransform[3] = 0.0;
                m_adfGeoTransform[4] = 0.0;
                m_adfGeoTransform[5] = 1.0;

                uint16_t nCountScale = 0;
                if (TIFFGetField(m_hTIFF, TIFFTAG_GEOPIXELSCALE, &nCountScale,
                                 &padfScale) &&
                    nCountScale >= 2 && padfScale[0] != 0.0 &&
                    padfScale[1] != 0.0)
                {
                    m_adfGeoTransform[1] = padfScale[0];
                    if (padfScale[1] < 0)
                    {
                        const char *pszOptionVal = CPLGetConfigOption(
                            "GTIFF_HONOUR_NEGATIVE_SCALEY", nullptr);
                        if (pszOptionVal == nullptr)
                        {
                            ReportError(
                                CE_Warning, CPLE_AppDefined,
                                "File with negative value for ScaleY in "
                                "GeoPixelScale tag. This is rather "
                                "unusual. GDAL, contrary to the GeoTIFF "
                                "specification, assumes that the file "
                                "was intended to be north-up, and will "
                                "treat this file as if ScaleY was "
                                "positive. You may override this behavior "
                                "by setting the GTIFF_HONOUR_NEGATIVE_SCALEY "
                                "configuration option to YES");
                            m_adfGeoTransform[5] = padfScale[1];
                        }
                        else if (CPLTestBool(pszOptionVal))
                        {
                            m_adfGeoTransform[5] = -padfScale[1];
                        }
                        else
                        {
                            m_adfGeoTransform[5] = padfScale[1];
                        }
                    }
                    else
                    {
                        m_adfGeoTransform[5] = -padfScale[1];
                    }

                    if (TIFFGetField(m_hTIFF, TIFFTAG_GEOTIEPOINTS, &nCount,
                                     &padfTiePoints) &&
                        nCount >= 6)
                    {
                        m_adfGeoTransform[0] =
                            padfTiePoints[3] -
                            padfTiePoints[0] * m_adfGeoTransform[1];
                        m_adfGeoTransform[3] =
                            padfTiePoints[4] -
                            padfTiePoints[1] * m_adfGeoTransform[5];

                        if (bPixelIsPoint && !bPointGeoIgnore)
                        {
                            m_adfGeoTransform[0] -=
                                (m_adfGeoTransform[1] * 0.5 +
                                 m_adfGeoTransform[2] * 0.5);
                            m_adfGeoTransform[3] -=
                                (m_adfGeoTransform[4] * 0.5 +
                                 m_adfGeoTransform[5] * 0.5);
                        }

                        m_bGeoTransformValid = true;
                        m_nGeoTransformGeorefSrcIndex = nIndex;

                        if (nCountScale >= 3 && GetRasterCount() == 1 &&
                            (padfScale[2] != 0.0 || padfTiePoints[2] != 0.0 ||
                             padfTiePoints[5] != 0.0))
                        {
                            LookForProjection();
                            if (!m_oSRS.IsEmpty() && m_oSRS.IsVertical())
                            {
                                /* modelTiePointTag = (pixel, line, z0, X, Y,
                                 * Z0) */
                                /* thus Z(some_point) = (z(some_point) - z0) *
                                 * scaleZ + Z0 */
                                /* equivalently written as */
                                /* Z(some_point) = z(some_point) * scaleZ +
                                 * offsetZ with */
                                /* offsetZ = - z0 * scaleZ + Z0 */
                                double dfScale = padfScale[2];
                                double dfOffset = -padfTiePoints[2] * dfScale +
                                                  padfTiePoints[5];
                                GTiffRasterBand *poBand =
                                    cpl::down_cast<GTiffRasterBand *>(
                                        GetRasterBand(1));
                                poBand->m_bHaveOffsetScale = true;
                                poBand->m_dfScale = dfScale;
                                poBand->m_dfOffset = dfOffset;
                            }
                        }
                    }
                }

                else if (TIFFGetField(m_hTIFF, TIFFTAG_GEOTRANSMATRIX, &nCount,
                                      &padfMatrix) &&
                         nCount == 16)
                {
                    m_adfGeoTransform[0] = padfMatrix[3];
                    m_adfGeoTransform[1] = padfMatrix[0];
                    m_adfGeoTransform[2] = padfMatrix[1];
                    m_adfGeoTransform[3] = padfMatrix[7];
                    m_adfGeoTransform[4] = padfMatrix[4];
                    m_adfGeoTransform[5] = padfMatrix[5];

                    if (bPixelIsPoint && !bPointGeoIgnore)
                    {
                        m_adfGeoTransform[0] -= m_adfGeoTransform[1] * 0.5 +
                                                m_adfGeoTransform[2] * 0.5;
                        m_adfGeoTransform[3] -= m_adfGeoTransform[4] * 0.5 +
                                                m_adfGeoTransform[5] * 0.5;
                    }

                    m_bGeoTransformValid = true;
                    m_nGeoTransformGeorefSrcIndex = nIndex;
                }
                if (m_bGeoTransformValid)
                    break;
            }

            /* --------------------------------------------------------------------
             */
            /*      Otherwise try looking for a .tab, .tfw, .tifw or .wld file.
             */
            /* --------------------------------------------------------------------
             */
            if (m_nTABFILEGeorefSrcIndex == nIndex)
            {
                char *pszGeorefFilename = nullptr;

                char **papszSiblingFiles = GetSiblingFiles();

                // Begin with .tab since it can also have projection info.
                int nGCPCount = 0;
                GDAL_GCP *pasGCPList = nullptr;
                const int bTabFileOK = GDALReadTabFile2(
                    m_pszFilename, m_adfGeoTransform, &pszTabWKT, &nGCPCount,
                    &pasGCPList, papszSiblingFiles, &pszGeorefFilename);

                if (bTabFileOK)
                {
                    m_nGeoTransformGeorefSrcIndex = nIndex;
                    // if( pszTabWKT )
                    // {
                    //     m_nProjectionGeorefSrcIndex = nIndex;
                    // }
                    m_aoGCPs = gdal::GCP::fromC(pasGCPList, nGCPCount);
                    if (m_aoGCPs.empty())
                    {
                        m_bGeoTransformValid = true;
                    }
                }

                if (nGCPCount)
                {
                    GDALDeinitGCPs(nGCPCount, pasGCPList);
                    CPLFree(pasGCPList);
                }

                if (pszGeorefFilename)
                {
                    CPLFree(m_pszGeorefFilename);
                    m_pszGeorefFilename = pszGeorefFilename;
                    pszGeorefFilename = nullptr;
                }
                if (m_bGeoTransformValid)
                    break;
            }

            if (m_nWORLDFILEGeorefSrcIndex == nIndex)
            {
                char *pszGeorefFilename = nullptr;

                char **papszSiblingFiles = GetSiblingFiles();

                m_bGeoTransformValid = CPL_TO_BOOL(GDALReadWorldFile2(
                    m_pszFilename, nullptr, m_adfGeoTransform,
                    papszSiblingFiles, &pszGeorefFilename));

                if (!m_bGeoTransformValid)
                {
                    m_bGeoTransformValid = CPL_TO_BOOL(GDALReadWorldFile2(
                        m_pszFilename, "wld", m_adfGeoTransform,
                        papszSiblingFiles, &pszGeorefFilename));
                }
                if (m_bGeoTransformValid)
                    m_nGeoTransformGeorefSrcIndex = nIndex;

                if (pszGeorefFilename)
                {
                    CPLFree(m_pszGeorefFilename);
                    m_pszGeorefFilename = pszGeorefFilename;
                    pszGeorefFilename = nullptr;
                }
                if (m_bGeoTransformValid)
                    break;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Check for GCPs. */
        /* --------------------------------------------------------------------
         */
        if (m_nINTERNALGeorefSrcIndex >= 0 &&
            TIFFGetField(m_hTIFF, TIFFTAG_GEOTIEPOINTS, &nCount,
                         &padfTiePoints) &&
            !m_bGeoTransformValid)
        {
            m_aoGCPs.clear();
            const int nNewGCPCount = nCount / 6;
            for (int iGCP = 0; iGCP < nNewGCPCount; ++iGCP)
            {
                m_aoGCPs.emplace_back(CPLSPrintf("%d", iGCP + 1), "",
                                      /* pixel = */ padfTiePoints[iGCP * 6 + 0],
                                      /* line = */ padfTiePoints[iGCP * 6 + 1],
                                      /* X = */ padfTiePoints[iGCP * 6 + 3],
                                      /* Y = */ padfTiePoints[iGCP * 6 + 4],
                                      /* Z = */ padfTiePoints[iGCP * 6 + 5]);

                if (bPixelIsPoint && !bPointGeoIgnore)
                {
                    m_aoGCPs.back().Pixel() += 0.5;
                    m_aoGCPs.back().Line() += 0.5;
                }
            }
            m_nGeoTransformGeorefSrcIndex = m_nINTERNALGeorefSrcIndex;
        }

        /* --------------------------------------------------------------------
         */
        /*      Did we find a tab file?  If so we will use its coordinate */
        /*      system and give it precedence. */
        /* --------------------------------------------------------------------
         */
        if (pszTabWKT != nullptr && m_oSRS.IsEmpty())
        {
            m_oSRS.importFromWkt(pszTabWKT);
            m_bLookedForProjection = true;
        }

        CPLFree(pszTabWKT);
    }

    if (m_bLoadPam && m_nPAMGeorefSrcIndex >= 0)
    {
        /* --------------------------------------------------------------------
         */
        /*      Initialize any PAM information. */
        /* --------------------------------------------------------------------
         */
        CPLAssert(!m_bColorProfileMetadataChanged);
        CPLAssert(!m_bMetadataChanged);
        CPLAssert(!m_bGeoTIFFInfoChanged);
        CPLAssert(!m_bNoDataChanged);

        // We must absolutely unset m_bLoadPam now, otherwise calling
        // GetFileList() on a .tif with a .aux will result in an (almost)
        // endless sequence of calls.
        m_bLoadPam = false;

        TryLoadXML(GetSiblingFiles());
        ApplyPamInfo();

        m_bColorProfileMetadataChanged = false;
        m_bMetadataChanged = false;
        m_bGeoTIFFInfoChanged = false;
        m_bNoDataChanged = false;
    }
    m_bLoadPam = false;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *GTiffDataset::GetSpatialRef() const

{
    const_cast<GTiffDataset *>(this)->LoadGeoreferencingAndPamIfNeeded();
    if (m_aoGCPs.empty())
    {
        const_cast<GTiffDataset *>(this)->LookForProjection();
    }

    return m_aoGCPs.empty() && !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::GetGeoTransform(double *padfTransform)

{
    LoadGeoreferencingAndPamIfNeeded();

    memcpy(padfTransform, m_adfGeoTransform, sizeof(double) * 6);

    if (!m_bGeoTransformValid)
        return CE_Failure;

    // Same logic as in the .gtx driver, for the benefit of
    // GDALOpenVerticalShiftGrid() when used with PROJ-data's US geoids.
    if (CPLFetchBool(papszOpenOptions, "SHIFT_ORIGIN_IN_MINUS_180_PLUS_180",
                     false))
    {
        if (padfTransform[0] < -180.0 - padfTransform[1])
            padfTransform[0] += 360.0;
        else if (padfTransform[0] > 180.0)
            padfTransform[0] -= 360.0;
    }

    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GTiffDataset::GetGCPCount()

{
    LoadGeoreferencingAndPamIfNeeded();

    return static_cast<int>(m_aoGCPs.size());
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *GTiffDataset::GetGCPSpatialRef() const

{
    const_cast<GTiffDataset *>(this)->LoadGeoreferencingAndPamIfNeeded();

    if (!m_aoGCPs.empty())
    {
        const_cast<GTiffDataset *>(this)->LookForProjection();
    }
    return !m_aoGCPs.empty() && !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GTiffDataset::GetGCPs()

{
    LoadGeoreferencingAndPamIfNeeded();

    return gdal::GCP::c_ptr(m_aoGCPs);
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GTiffDataset::GetMetadataDomainList()
{
    LoadGeoreferencingAndPamIfNeeded();

    char **papszDomainList = CSLDuplicate(m_oGTiffMDMD.GetDomainList());
    char **papszBaseList = GDALDataset::GetMetadataDomainList();

    const int nbBaseDomains = CSLCount(papszBaseList);

    for (int domainId = 0; domainId < nbBaseDomains; ++domainId)
    {
        if (CSLFindString(papszDomainList, papszBaseList[domainId]) < 0)
        {
            papszDomainList =
                CSLAddString(papszDomainList, papszBaseList[domainId]);
        }
    }

    CSLDestroy(papszBaseList);

    return BuildMetadataDomainList(papszDomainList, TRUE, "",
                                   "ProxyOverviewRequest", MD_DOMAIN_RPC,
                                   MD_DOMAIN_IMD, "SUBDATASETS", "EXIF",
                                   "xml:XMP", "COLOR_PROFILE", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GTiffDataset::GetMetadata(const char *pszDomain)

{
    if (pszDomain != nullptr && EQUAL(pszDomain, "IMAGE_STRUCTURE"))
    {
        GTiffDataset::GetMetadataItem("COMPRESSION_REVERSIBILITY", pszDomain);
    }
    else
    {
        LoadGeoreferencingAndPamIfNeeded();
    }

    if (pszDomain != nullptr && EQUAL(pszDomain, "ProxyOverviewRequest"))
        return GDALPamDataset::GetMetadata(pszDomain);

    if (pszDomain != nullptr && EQUAL(pszDomain, "DERIVED_SUBDATASETS"))
    {
        return GDALDataset::GetMetadata(pszDomain);
    }

    else if (pszDomain != nullptr && (EQUAL(pszDomain, MD_DOMAIN_RPC) ||
                                      EQUAL(pszDomain, MD_DOMAIN_IMD) ||
                                      EQUAL(pszDomain, MD_DOMAIN_IMAGERY)))
        LoadMetadata();

    else if (pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS"))
        ScanDirectories();

    else if (pszDomain != nullptr && EQUAL(pszDomain, "EXIF"))
        LoadEXIFMetadata();

    else if (pszDomain != nullptr && EQUAL(pszDomain, "COLOR_PROFILE"))
        LoadICCProfile();

    else if (pszDomain == nullptr || EQUAL(pszDomain, ""))
        LoadMDAreaOrPoint();  // To set GDALMD_AREA_OR_POINT.

    return m_oGTiffMDMD.GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GTiffDataset::GetMetadataItem(const char *pszName,
                                          const char *pszDomain)

{
    if (pszDomain != nullptr && EQUAL(pszDomain, "IMAGE_STRUCTURE"))
    {
        if ((m_nCompression == COMPRESSION_WEBP ||
             m_nCompression == COMPRESSION_JXL) &&
            EQUAL(pszName, "COMPRESSION_REVERSIBILITY") &&
            m_oGTiffMDMD.GetMetadataItem("COMPRESSION_REVERSIBILITY",
                                         "IMAGE_STRUCTURE") == nullptr)
        {
            const char *pszDriverName =
                m_nCompression == COMPRESSION_WEBP ? "WEBP" : "JPEGXL";
            auto poTileDriver = GDALGetDriverByName(pszDriverName);
            if (poTileDriver)
            {
                vsi_l_offset nOffset = 0;
                vsi_l_offset nSize = 0;
                IsBlockAvailable(0, &nOffset, &nSize);
                if (nSize > 0)
                {
                    const std::string osSubfile(
                        CPLSPrintf("/vsisubfile/" CPL_FRMT_GUIB "_%d,%s",
                                   static_cast<GUIntBig>(nOffset),
                                   static_cast<int>(std::min(
                                       static_cast<vsi_l_offset>(1024), nSize)),
                                   m_pszFilename));
                    const char *const apszDrivers[] = {pszDriverName, nullptr};
                    auto poWebPDataset =
                        std::unique_ptr<GDALDataset>(GDALDataset::Open(
                            osSubfile.c_str(), GDAL_OF_RASTER, apszDrivers));
                    if (poWebPDataset)
                    {
                        const char *pszReversibility =
                            poWebPDataset->GetMetadataItem(
                                "COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE");
                        if (pszReversibility)
                            m_oGTiffMDMD.SetMetadataItem(
                                "COMPRESSION_REVERSIBILITY", pszReversibility,
                                "IMAGE_STRUCTURE");
                    }
                }
            }
        }
    }
    else
    {
        LoadGeoreferencingAndPamIfNeeded();
    }

    if (pszDomain != nullptr && EQUAL(pszDomain, "ProxyOverviewRequest"))
    {
        return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
    }
    else if (pszDomain != nullptr && (EQUAL(pszDomain, MD_DOMAIN_RPC) ||
                                      EQUAL(pszDomain, MD_DOMAIN_IMD) ||
                                      EQUAL(pszDomain, MD_DOMAIN_IMAGERY)))
    {
        LoadMetadata();
    }
    else if (pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS"))
    {
        ScanDirectories();
    }
    else if (pszDomain != nullptr && EQUAL(pszDomain, "EXIF"))
    {
        LoadEXIFMetadata();
    }
    else if (pszDomain != nullptr && EQUAL(pszDomain, "COLOR_PROFILE"))
    {
        LoadICCProfile();
    }
    else if ((pszDomain == nullptr || EQUAL(pszDomain, "")) &&
             pszName != nullptr && EQUAL(pszName, GDALMD_AREA_OR_POINT))
    {
        LoadMDAreaOrPoint();  // To set GDALMD_AREA_OR_POINT.
    }
    else if (pszDomain != nullptr && EQUAL(pszDomain, "_DEBUG_") &&
             pszName != nullptr)
    {
#ifdef DEBUG_REACHED_VIRTUAL_MEM_IO
        if (EQUAL(pszName, "UNREACHED_VIRTUALMEMIO_CODE_PATH"))
        {
            CPLString osMissing;
            for (int i = 0;
                 i < static_cast<int>(CPL_ARRAYSIZE(anReachedVirtualMemIO));
                 ++i)
            {
                if (!anReachedVirtualMemIO[i])
                {
                    if (!osMissing.empty())
                        osMissing += ",";
                    osMissing += CPLSPrintf("%d", i);
                }
            }
            return (osMissing.size()) ? CPLSPrintf("%s", osMissing.c_str())
                                      : nullptr;
        }
        else
#endif
            if (EQUAL(pszName, "TIFFTAG_EXTRASAMPLES"))
        {
            CPLString osRet;
            uint16_t *v = nullptr;
            uint16_t count = 0;

            if (TIFFGetField(m_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v))
            {
                for (int i = 0; i < static_cast<int>(count); ++i)
                {
                    if (i > 0)
                        osRet += ",";
                    osRet += CPLSPrintf("%d", v[i]);
                }
            }
            return (osRet.size()) ? CPLSPrintf("%s", osRet.c_str()) : nullptr;
        }
        else if (EQUAL(pszName, "TIFFTAG_PHOTOMETRIC"))
        {
            return CPLSPrintf("%d", m_nPhotometric);
        }

        else if (EQUAL(pszName, "TIFFTAG_GDAL_METADATA"))
        {
            char *pszText = nullptr;
            if (!TIFFGetField(m_hTIFF, TIFFTAG_GDAL_METADATA, &pszText))
                return nullptr;

            return pszText;
        }
        else if (EQUAL(pszName, "HAS_USED_READ_ENCODED_API"))
        {
            return m_bHasUsedReadEncodedAPI ? "1" : "0";
        }
        else if (EQUAL(pszName, "WEBP_LOSSLESS"))
        {
            return m_bWebPLossless ? "1" : "0";
        }
        else if (EQUAL(pszName, "WEBP_LEVEL"))
        {
            return CPLSPrintf("%d", m_nWebPLevel);
        }
        else if (EQUAL(pszName, "MAX_Z_ERROR"))
        {
            return CPLSPrintf("%f", m_dfMaxZError);
        }
        else if (EQUAL(pszName, "MAX_Z_ERROR_OVERVIEW"))
        {
            return CPLSPrintf("%f", m_dfMaxZErrorOverview);
        }
#if HAVE_JXL
        else if (EQUAL(pszName, "JXL_LOSSLESS"))
        {
            return m_bJXLLossless ? "1" : "0";
        }
        else if (EQUAL(pszName, "JXL_DISTANCE"))
        {
            return CPLSPrintf("%f", m_fJXLDistance);
        }
        else if (EQUAL(pszName, "JXL_ALPHA_DISTANCE"))
        {
            return CPLSPrintf("%f", m_fJXLAlphaDistance);
        }
        else if (EQUAL(pszName, "JXL_EFFORT"))
        {
            return CPLSPrintf("%u", m_nJXLEffort);
        }
#endif
        return nullptr;
    }

    else if (pszDomain != nullptr && EQUAL(pszDomain, "TIFF") &&
             pszName != nullptr)
    {
        if (EQUAL(pszName, "GDAL_STRUCTURAL_METADATA"))
        {
            const auto nOffset = VSIFTellL(m_fpL);
            VSIFSeekL(m_fpL, 0, SEEK_SET);
            GByte abyData[1024];
            size_t nRead = VSIFReadL(abyData, 1, sizeof(abyData) - 1, m_fpL);
            abyData[nRead] = 0;
            VSIFSeekL(m_fpL, nOffset, SEEK_SET);
            if (nRead > 4)
            {
                const int nOffsetOfStructuralMetadata =
                    (abyData[2] == 0x2B || abyData[3] == 0x2B) ? 16 : 8;
                const int nSizePatternLen =
                    static_cast<int>(strlen("XXXXXX bytes\n"));
                if (nRead > nOffsetOfStructuralMetadata +
                                strlen("GDAL_STRUCTURAL_METADATA_SIZE=") +
                                nSizePatternLen &&
                    memcmp(abyData + nOffsetOfStructuralMetadata,
                           "GDAL_STRUCTURAL_METADATA_SIZE=",
                           strlen("GDAL_STRUCTURAL_METADATA_SIZE=")) == 0)
                {
                    char *pszStructuralMD = reinterpret_cast<char *>(
                        abyData + nOffsetOfStructuralMetadata);
                    const int nLenMD =
                        atoi(pszStructuralMD +
                             strlen("GDAL_STRUCTURAL_METADATA_SIZE="));
                    if (nOffsetOfStructuralMetadata +
                            strlen("GDAL_STRUCTURAL_METADATA_SIZE=") +
                            nSizePatternLen + nLenMD <=
                        nRead)
                    {
                        pszStructuralMD[strlen(
                                            "GDAL_STRUCTURAL_METADATA_SIZE=") +
                                        nSizePatternLen + nLenMD] = 0;
                        return CPLSPrintf("%s", pszStructuralMD);
                    }
                }
            }
            return nullptr;
        }
    }

    return m_oGTiffMDMD.GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                         LoadEXIFMetadata()                           */
/************************************************************************/

void GTiffDataset::LoadEXIFMetadata()
{
    if (m_bEXIFMetadataLoaded)
        return;
    m_bEXIFMetadataLoaded = true;

    VSILFILE *fp = VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF));

    GByte abyHeader[2] = {0};
    if (VSIFSeekL(fp, 0, SEEK_SET) != 0 || VSIFReadL(abyHeader, 1, 2, fp) != 2)
        return;

    const bool bLittleEndian = abyHeader[0] == 'I' && abyHeader[1] == 'I';
    const bool bLeastSignificantBit = CPL_IS_LSB != 0;
    const bool bSwabflag = bLittleEndian != bLeastSignificantBit;  // != is XOR.

    char **papszMetadata = nullptr;
    toff_t nOffset = 0;  // TODO(b/28199387): Refactor to simplify casting.

    if (TIFFGetField(m_hTIFF, TIFFTAG_EXIFIFD, &nOffset))
    {
        int nExifOffset = static_cast<int>(nOffset);
        int nInterOffset = 0;
        int nGPSOffset = 0;
        EXIFExtractMetadata(papszMetadata, fp, static_cast<int>(nOffset),
                            bSwabflag, 0, nExifOffset, nInterOffset,
                            nGPSOffset);
    }

    if (TIFFGetField(m_hTIFF, TIFFTAG_GPSIFD, &nOffset))
    {
        int nExifOffset = 0;  // TODO(b/28199387): Refactor to simplify casting.
        int nInterOffset = 0;
        int nGPSOffset = static_cast<int>(nOffset);
        EXIFExtractMetadata(papszMetadata, fp, static_cast<int>(nOffset),
                            bSwabflag, 0, nExifOffset, nInterOffset,
                            nGPSOffset);
    }

    if (papszMetadata)
    {
        m_oGTiffMDMD.SetMetadata(papszMetadata, "EXIF");
        CSLDestroy(papszMetadata);
    }
}

/************************************************************************/
/*                           LoadMetadata()                             */
/************************************************************************/
void GTiffDataset::LoadMetadata()
{
    if (m_bIMDRPCMetadataLoaded)
        return;
    m_bIMDRPCMetadataLoaded = true;

    GDALMDReaderManager mdreadermanager;
    GDALMDReaderBase *mdreader = mdreadermanager.GetReader(
        m_pszFilename, oOvManager.GetSiblingFiles(), MDR_ANY);

    if (nullptr != mdreader)
    {
        mdreader->FillMetadata(&m_oGTiffMDMD);

        if (mdreader->GetMetadataDomain(MD_DOMAIN_RPC) == nullptr)
        {
            char **papszRPCMD = GTiffDatasetReadRPCTag(m_hTIFF);
            if (papszRPCMD)
            {
                m_oGTiffMDMD.SetMetadata(papszRPCMD, MD_DOMAIN_RPC);
                CSLDestroy(papszRPCMD);
            }
        }

        m_papszMetadataFiles = mdreader->GetMetadataFiles();
    }
    else
    {
        char **papszRPCMD = GTiffDatasetReadRPCTag(m_hTIFF);
        if (papszRPCMD)
        {
            m_oGTiffMDMD.SetMetadata(papszRPCMD, MD_DOMAIN_RPC);
            CSLDestroy(papszRPCMD);
        }
    }
}

/************************************************************************/
/*                     HasOptimizedReadMultiRange()                     */
/************************************************************************/

bool GTiffDataset::HasOptimizedReadMultiRange()
{
    if (m_nHasOptimizedReadMultiRange >= 0)
        return m_nHasOptimizedReadMultiRange != 0;
    m_nHasOptimizedReadMultiRange = static_cast<signed char>(
        VSIHasOptimizedReadMultiRange(m_pszFilename)
        // Config option for debug and testing purposes only
        || CPLTestBool(CPLGetConfigOption(
               "GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE", "NO")));
    return m_nHasOptimizedReadMultiRange != 0;
}
