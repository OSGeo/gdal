/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
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

#include <cassert>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "cpl_error.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "cpl_worker_thread_pool.h"
#include "ogr_proj_p.h"  // OSRGetProjTLSContext()
#include "tif_jxl.h"
#include "tifvsi.h"
#include "xtiffio.h"

static const GTIFFTag asTIFFTags[] = {
    {"TIFFTAG_DOCUMENTNAME", TIFFTAG_DOCUMENTNAME, GTIFFTAGTYPE_STRING},
    {"TIFFTAG_IMAGEDESCRIPTION", TIFFTAG_IMAGEDESCRIPTION, GTIFFTAGTYPE_STRING},
    {"TIFFTAG_SOFTWARE", TIFFTAG_SOFTWARE, GTIFFTAGTYPE_STRING},
    {"TIFFTAG_DATETIME", TIFFTAG_DATETIME, GTIFFTAGTYPE_STRING},
    {"TIFFTAG_ARTIST", TIFFTAG_ARTIST, GTIFFTAGTYPE_STRING},
    {"TIFFTAG_HOSTCOMPUTER", TIFFTAG_HOSTCOMPUTER, GTIFFTAGTYPE_STRING},
    {"TIFFTAG_COPYRIGHT", TIFFTAG_COPYRIGHT, GTIFFTAGTYPE_STRING},
    {"TIFFTAG_XRESOLUTION", TIFFTAG_XRESOLUTION, GTIFFTAGTYPE_FLOAT},
    {"TIFFTAG_YRESOLUTION", TIFFTAG_YRESOLUTION, GTIFFTAGTYPE_FLOAT},
    // Dealt as special case.
    {"TIFFTAG_RESOLUTIONUNIT", TIFFTAG_RESOLUTIONUNIT, GTIFFTAGTYPE_SHORT},
    {"TIFFTAG_MINSAMPLEVALUE", TIFFTAG_MINSAMPLEVALUE, GTIFFTAGTYPE_SHORT},
    {"TIFFTAG_MAXSAMPLEVALUE", TIFFTAG_MAXSAMPLEVALUE, GTIFFTAGTYPE_SHORT},

    // GeoTIFF DGIWG tags
    {"GEO_METADATA", TIFFTAG_GEO_METADATA, GTIFFTAGTYPE_BYTE_STRING},
    {"TIFF_RSID", TIFFTAG_TIFF_RSID, GTIFFTAGTYPE_STRING},
    {nullptr, 0, GTIFFTAGTYPE_STRING},
};

/************************************************************************/
/*                            GetTIFFTags()                             */
/************************************************************************/

const GTIFFTag *GTiffDataset::GetTIFFTags()
{
    return asTIFFTags;
}

/************************************************************************/
/*                            GTiffDataset()                            */
/************************************************************************/

GTiffDataset::GTiffDataset()
    : m_bStreamingIn(false), m_bStreamingOut(false), m_bScanDeferred(true),
      m_bSingleIFDOpened(false), m_bLoadedBlockDirty(false),
      m_bWriteError(false), m_bLookedForProjection(false),
      m_bLookedForMDAreaOrPoint(false), m_bGeoTransformValid(false),
      m_bCrystalized(true), m_bGeoTIFFInfoChanged(false),
      m_bForceUnsetGTOrGCPs(false), m_bForceUnsetProjection(false),
      m_bNoDataChanged(false), m_bNoDataSet(false), m_bNoDataSetAsInt64(false),
      m_bNoDataSetAsUInt64(false), m_bMetadataChanged(false),
      m_bColorProfileMetadataChanged(false), m_bForceUnsetRPC(false),
      m_bNeedsRewrite(false), m_bLoadingOtherBands(false), m_bIsOverview(false),
      m_bWriteEmptyTiles(true), m_bFillEmptyTilesAtClosing(false),
      m_bTreatAsSplit(false), m_bTreatAsSplitBitmap(false), m_bClipWarn(false),
      m_bIMDRPCMetadataLoaded(false), m_bEXIFMetadataLoaded(false),
      m_bICCMetadataLoaded(false),
      m_bHasWarnedDisableAggressiveBandCaching(false),
      m_bDontReloadFirstBlock(false), m_bWebPLossless(false),
      m_bPromoteTo8Bits(false),
      m_bDebugDontWriteBlocks(
          CPLTestBool(CPLGetConfigOption("GTIFF_DONT_WRITE_BLOCKS", "NO"))),
      m_bIsFinalized(false),
      m_bIgnoreReadErrors(
          CPLTestBool(CPLGetConfigOption("GTIFF_IGNORE_READ_ERRORS", "NO"))),
      m_bDirectIO(CPLTestBool(CPLGetConfigOption("GTIFF_DIRECT_IO", "NO"))),
      m_bReadGeoTransform(false), m_bLoadPam(false),
      m_bHasGotSiblingFiles(false),
      m_bHasIdentifiedAuthorizedGeoreferencingSources(false),
      m_bLayoutIFDSBeforeData(false), m_bBlockOrderRowMajor(false),
      m_bLeaderSizeAsUInt4(false), m_bTrailerRepeatedLast4BytesRepeated(false),
      m_bMaskInterleavedWithImagery(false), m_bKnownIncompatibleEdition(false),
      m_bWriteKnownIncompatibleEdition(false), m_bHasUsedReadEncodedAPI(false),
      m_bWriteCOGLayout(false)
{
    // CPLDebug("GDAL", "sizeof(GTiffDataset) = %d bytes", static_cast<int>(
    //     sizeof(GTiffDataset)));

    const char *pszVirtualMemIO =
        CPLGetConfigOption("GTIFF_VIRTUAL_MEM_IO", "NO");
    if (EQUAL(pszVirtualMemIO, "IF_ENOUGH_RAM"))
        m_eVirtualMemIOUsage = VirtualMemIOEnum::IF_ENOUGH_RAM;
    else if (CPLTestBool(pszVirtualMemIO))
        m_eVirtualMemIOUsage = VirtualMemIOEnum::YES;

    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

/************************************************************************/
/*                           ~GTiffDataset()                            */
/************************************************************************/

GTiffDataset::~GTiffDataset()

{
    GTiffDataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr GTiffDataset::Close()
{
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        auto [eErr, bDroppedRef] = Finalize();

        if (m_pszTmpFilename)
        {
            VSIUnlink(m_pszTmpFilename);
            CPLFree(m_pszTmpFilename);
        }

        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
        return eErr;
    }
    return CE_None;
}

/************************************************************************/
/*                             Finalize()                               */
/************************************************************************/

// Return a tuple (CPLErr, bool) to indicate respectively if an I/O error has
// occurred and if a reference to an auxiliary dataset has been dropped.
std::tuple<CPLErr, bool> GTiffDataset::Finalize()
{
    bool bDroppedRef = false;
    if (m_bIsFinalized)
        return std::tuple(CE_None, bDroppedRef);

    CPLErr eErr = CE_None;
    Crystalize();

    if (m_bColorProfileMetadataChanged)
    {
        SaveICCProfile(this, nullptr, nullptr, 0);
        m_bColorProfileMetadataChanged = false;
    }

    /* -------------------------------------------------------------------- */
    /*      Handle forcing xml:ESRI data to be written to PAM.              */
    /* -------------------------------------------------------------------- */
    if (CPLTestBool(CPLGetConfigOption("ESRI_XML_PAM", "NO")))
    {
        char **papszESRIMD = GTiffDataset::GetMetadata("xml:ESRI");
        if (papszESRIMD)
        {
            GDALPamDataset::SetMetadata(papszESRIMD, "xml:ESRI");
        }
    }

    if (m_psVirtualMemIOMapping)
        CPLVirtualMemFree(m_psVirtualMemIOMapping);
    m_psVirtualMemIOMapping = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Fill in missing blocks with empty data.                         */
    /* -------------------------------------------------------------------- */
    if (m_bFillEmptyTilesAtClosing)
    {
        /* --------------------------------------------------------------------
         */
        /*  Ensure any blocks write cached by GDAL gets pushed through libtiff.
         */
        /* --------------------------------------------------------------------
         */
        if (FlushCacheInternal(true, /* at closing */
                               false /* do not call FlushDirectory */) !=
            CE_None)
        {
            eErr = CE_Failure;
        }

        if (FillEmptyTiles() != CE_None)
        {
            eErr = CE_Failure;
        }
        m_bFillEmptyTilesAtClosing = false;
    }

    /* -------------------------------------------------------------------- */
    /*      Force a complete flush, including either rewriting(moving)      */
    /*      of writing in place the current directory.                      */
    /* -------------------------------------------------------------------- */
    if (FlushCacheInternal(true /* at closing */, true) != CE_None)
    {
        eErr = CE_Failure;
    }

    // Destroy compression queue
    if (m_poCompressQueue)
    {
        m_poCompressQueue->WaitCompletion();

        for (int i = 0; i < static_cast<int>(m_asCompressionJobs.size()); ++i)
        {
            CPLFree(m_asCompressionJobs[i].pabyBuffer);
            if (m_asCompressionJobs[i].pszTmpFilename)
            {
                VSIUnlink(m_asCompressionJobs[i].pszTmpFilename);
                CPLFree(m_asCompressionJobs[i].pszTmpFilename);
            }
        }
        CPLDestroyMutex(m_hCompressThreadPoolMutex);
        m_hCompressThreadPoolMutex = nullptr;
        m_poCompressQueue.reset();
    }

    /* -------------------------------------------------------------------- */
    /*      If there is still changed metadata, then presumably we want     */
    /*      to push it into PAM.                                            */
    /* -------------------------------------------------------------------- */
    if (m_bMetadataChanged)
    {
        PushMetadataToPam();
        m_bMetadataChanged = false;
        GDALPamDataset::FlushCache(false);
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup overviews.                                              */
    /* -------------------------------------------------------------------- */
    if (!m_poBaseDS)
    {
        // Nullify m_nOverviewCount before deleting overviews, otherwise
        // GTiffDataset::FlushDirectory() might try to access an overview
        // that is being deleted (#5580)
        const int nOldOverviewCount = m_nOverviewCount;
        m_nOverviewCount = 0;
        for (int i = 0; i < nOldOverviewCount; ++i)
        {
            delete m_papoOverviewDS[i];
            bDroppedRef = true;
        }

        for (int i = 0; i < m_nJPEGOverviewCountOri; ++i)
        {
            delete m_papoJPEGOverviewDS[i];
            bDroppedRef = true;
        }
        m_nJPEGOverviewCount = 0;
        m_nJPEGOverviewCountOri = 0;
        CPLFree(m_papoJPEGOverviewDS);
        m_papoJPEGOverviewDS = nullptr;
    }

    // If we are a mask dataset, we can have overviews, but we don't
    // own them. We can only free the array, not the overviews themselves.
    CPLFree(m_papoOverviewDS);
    m_papoOverviewDS = nullptr;

    // m_poMaskDS is owned by the main image and the overviews
    // so because of the latter case, we can delete it even if
    // we are not the base image.
    if (m_poMaskDS)
    {
        // Nullify m_nOverviewCount before deleting overviews, otherwise
        // GTiffDataset::FlushDirectory() might try to access it while being
        // deleted. (#5580)
        auto poMaskDS = m_poMaskDS;
        m_poMaskDS = nullptr;
        delete poMaskDS;
        bDroppedRef = true;
    }

    if (m_poColorTable != nullptr)
        delete m_poColorTable;
    m_poColorTable = nullptr;

    if (m_hTIFF)
    {
        XTIFFClose(m_hTIFF);
        m_hTIFF = nullptr;
    }

    if (!m_poBaseDS)
    {
        if (m_fpL != nullptr)
        {
            if (m_bWriteKnownIncompatibleEdition)
            {
                GByte abyHeader[4096];
                VSIFSeekL(m_fpL, 0, SEEK_SET);
                VSIFReadL(abyHeader, 1, sizeof(abyHeader), m_fpL);
                const char *szKeyToLook =
                    "KNOWN_INCOMPATIBLE_EDITION=NO\n ";  // trailing space
                                                         // intended
                for (size_t i = 0; i < sizeof(abyHeader) - strlen(szKeyToLook);
                     i++)
                {
                    if (memcmp(abyHeader + i, szKeyToLook,
                               strlen(szKeyToLook)) == 0)
                    {
                        const char *szNewKey =
                            "KNOWN_INCOMPATIBLE_EDITION=YES\n";
                        CPLAssert(strlen(szKeyToLook) == strlen(szNewKey));
                        memcpy(abyHeader + i, szNewKey, strlen(szNewKey));
                        VSIFSeekL(m_fpL, 0, SEEK_SET);
                        VSIFWriteL(abyHeader, 1, sizeof(abyHeader), m_fpL);
                        break;
                    }
                }
            }
            if (VSIFCloseL(m_fpL) != 0)
            {
                eErr = CE_Failure;
                ReportError(CE_Failure, CPLE_FileIO, "I/O error");
            }
            m_fpL = nullptr;
        }
    }

    if (m_fpToWrite != nullptr)
    {
        if (VSIFCloseL(m_fpToWrite) != 0)
        {
            eErr = CE_Failure;
            ReportError(CE_Failure, CPLE_FileIO, "I/O error");
        }
        m_fpToWrite = nullptr;
    }

    m_aoGCPs.clear();

    CSLDestroy(m_papszCreationOptions);
    m_papszCreationOptions = nullptr;

    CPLFree(m_pabyTempWriteBuffer);
    m_pabyTempWriteBuffer = nullptr;

    m_bIMDRPCMetadataLoaded = false;
    CSLDestroy(m_papszMetadataFiles);
    m_papszMetadataFiles = nullptr;

    VSIFree(m_pTempBufferForCommonDirectIO);
    m_pTempBufferForCommonDirectIO = nullptr;

    CPLFree(m_panMaskOffsetLsb);
    m_panMaskOffsetLsb = nullptr;

    CPLFree(m_pszVertUnit);
    m_pszVertUnit = nullptr;

    CPLFree(m_pszFilename);
    m_pszFilename = nullptr;

    CPLFree(m_pszGeorefFilename);
    m_pszGeorefFilename = nullptr;

    CPLFree(m_pszXMLFilename);
    m_pszXMLFilename = nullptr;

    m_bIsFinalized = true;

    return std::tuple(eErr, bDroppedRef);
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int GTiffDataset::CloseDependentDatasets()
{
    if (m_poBaseDS)
        return FALSE;

    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    // We ignore eErr as it is not relevant for CloseDependentDatasets(),
    // which is called in a "garbage collection" context.
    auto [eErr, bHasDroppedRefInFinalize] = Finalize();
    if (bHasDroppedRefInFinalize)
        bHasDroppedRef = true;

    return bHasDroppedRef;
}

/************************************************************************/
/*                        IsWholeBlock()                                */
/************************************************************************/

bool GTiffDataset::IsWholeBlock(int nXOff, int nYOff, int nXSize,
                                int nYSize) const
{
    if ((nXOff % m_nBlockXSize) != 0 || (nYOff % m_nBlockYSize) != 0)
    {
        return false;
    }
    if (TIFFIsTiled(m_hTIFF))
    {
        return nXSize == m_nBlockXSize && nYSize == m_nBlockYSize;
    }
    else
    {
        return nXSize == m_nBlockXSize &&
               (nYSize == m_nBlockYSize || nYOff + nYSize == nRasterYSize);
    }
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GTiffDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                               int nXSize, int nYSize, void *pData,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, int nBandCount,
                               int *panBandMap, GSpacing nPixelSpace,
                               GSpacing nLineSpace, GSpacing nBandSpace,
                               GDALRasterIOExtraArg *psExtraArg)

{
    // Try to pass the request to the most appropriate overview dataset.
    if (nBufXSize < nXSize && nBufYSize < nYSize)
    {
        int bTried = FALSE;
        if (psExtraArg->eResampleAlg == GRIORA_NearestNeighbour)
            ++m_nJPEGOverviewVisibilityCounter;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);
        if (psExtraArg->eResampleAlg == GRIORA_NearestNeighbour)
            --m_nJPEGOverviewVisibilityCounter;
        if (bTried)
            return eErr;
    }

    if (m_eVirtualMemIOUsage != VirtualMemIOEnum::NO)
    {
        const int nErr =
            VirtualMemIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                         nBufXSize, nBufYSize, eBufType, nBandCount, panBandMap,
                         nPixelSpace, nLineSpace, nBandSpace, psExtraArg);
        if (nErr >= 0)
            return static_cast<CPLErr>(nErr);
    }
    if (m_bDirectIO)
    {
        const int nErr =
            DirectIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
                     nBufYSize, eBufType, nBandCount, panBandMap, nPixelSpace,
                     nLineSpace, nBandSpace, psExtraArg);
        if (nErr >= 0)
            return static_cast<CPLErr>(nErr);
    }

    bool bCanUseMultiThreadedRead = false;
    if (m_nDisableMultiThreadedRead == 0 && m_poThreadPool &&
        eRWFlag == GF_Read && nBufXSize == nXSize && nBufYSize == nYSize &&
        IsMultiThreadedReadCompatible())
    {
        const int nBlockX1 = nXOff / m_nBlockXSize;
        const int nBlockY1 = nYOff / m_nBlockYSize;
        const int nBlockX2 = (nXOff + nXSize - 1) / m_nBlockXSize;
        const int nBlockY2 = (nYOff + nYSize - 1) / m_nBlockYSize;
        const int nXBlocks = nBlockX2 - nBlockX1 + 1;
        const int nYBlocks = nBlockY2 - nBlockY1 + 1;
        const int nBlocks =
            nXBlocks * nYBlocks *
            (m_nPlanarConfig == PLANARCONFIG_CONTIG ? 1 : nBandCount);
        if (nBlocks > 1)
        {
            bCanUseMultiThreadedRead = true;
        }
    }

    void *pBufferedData = nullptr;
    const auto poFirstBand = cpl::down_cast<GTiffRasterBand *>(papoBands[0]);
    const auto eDataType = poFirstBand->GetRasterDataType();

    if (eAccess == GA_ReadOnly && eRWFlag == GF_Read &&
        (nBands == 1 || m_nPlanarConfig == PLANARCONFIG_CONTIG) &&
        HasOptimizedReadMultiRange() &&
        !(bCanUseMultiThreadedRead &&
          VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF))->HasPRead()))
    {
        pBufferedData = poFirstBand->CacheMultiRange(
            nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, psExtraArg);
    }
    else if (bCanUseMultiThreadedRead)
    {
        return MultiThreadedRead(nXOff, nYOff, nXSize, nYSize, pData, eBufType,
                                 nBandCount, panBandMap, nPixelSpace,
                                 nLineSpace, nBandSpace);
    }

    // Write optimization when writing whole blocks, by-passing the block cache.
    // We require the block cache to be non instantiated to simplify things
    // (otherwise we might need to evict corresponding existing blocks from the
    // block cache).
    else if (eRWFlag == GF_Write && nBands > 1 &&
             m_nPlanarConfig == PLANARCONFIG_CONTIG &&
             // Could be extended to "odd bit" case, but more work
             m_nBitsPerSample == GDALGetDataTypeSize(eDataType) &&
             nXSize == nBufXSize && nYSize == nBufYSize &&
             nBandCount == nBands && !m_bLoadedBlockDirty &&
             (nXOff % m_nBlockXSize) == 0 && (nYOff % m_nBlockYSize) == 0 &&
             (nXOff + nXSize == nRasterXSize ||
              (nXSize % m_nBlockXSize) == 0) &&
             (nYOff + nYSize == nRasterYSize || (nYSize % m_nBlockYSize) == 0))
    {
        bool bOptimOK = true;
        bool bOrderedBands = true;
        for (int i = 0; i < nBands; ++i)
        {
            if (panBandMap[i] != i + 1)
            {
                bOrderedBands = false;
            }
            if (cpl::down_cast<GTiffRasterBand *>(papoBands[panBandMap[i] - 1])
                    ->HasBlockCache())
            {
                bOptimOK = false;
                break;
            }
        }
        if (bOptimOK)
        {
            Crystalize();

            if (m_bDebugDontWriteBlocks)
                return CE_None;

            const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
            if (bOrderedBands && nXSize == m_nBlockXSize &&
                nYSize == m_nBlockYSize && eBufType == eDataType &&
                nBandSpace == nDTSize &&
                nPixelSpace == static_cast<GSpacing>(nDTSize) * nBands &&
                nLineSpace == nPixelSpace * m_nBlockXSize)
            {
                // If writing one single block with the right data type and
                // layout (interleaved per pixel), we don't need a temporary
                // buffer
                const int nBlockId = poFirstBand->ComputeBlockId(
                    nXOff / m_nBlockXSize, nYOff / m_nBlockYSize);
                return WriteEncodedTileOrStrip(nBlockId, pData,
                                               /* bPreserveDataBuffer= */ true);
            }

            // Make sure m_poGDS->m_pabyBlockBuf is allocated.
            // We could actually use any temporary buffer
            if (LoadBlockBuf(/* nBlockId = */ -1,
                             /* bReadFromDisk = */ false) != CE_None)
            {
                return CE_Failure;
            }

            // Iterate over all blocks defined by
            // [nXOff, nXOff+nXSize[ * [nYOff, nYOff+nYSize[
            // and write their content as a nBlockXSize x nBlockYSize strile
            // in a temporary buffer, before calling WriteEncodedTileOrStrip()
            // on it
            const int nYBlockStart = nYOff / m_nBlockYSize;
            const int nYBlockEnd = 1 + (nYOff + nYSize - 1) / m_nBlockYSize;
            const int nXBlockStart = nXOff / m_nBlockXSize;
            const int nXBlockEnd = 1 + (nXOff + nXSize - 1) / m_nBlockXSize;
            for (int nYBlock = nYBlockStart; nYBlock < nYBlockEnd; ++nYBlock)
            {
                const int nValidY = std::min(
                    m_nBlockYSize, nRasterYSize - nYBlock * m_nBlockYSize);
                for (int nXBlock = nXBlockStart; nXBlock < nXBlockEnd;
                     ++nXBlock)
                {
                    const int nValidX = std::min(
                        m_nBlockXSize, nRasterXSize - nXBlock * m_nBlockXSize);
                    if (nValidY < m_nBlockYSize || nValidX < m_nBlockXSize)
                    {
                        // Make sure padding bytes at the right/bottom of the
                        // tile are initialized to zero.
                        memset(m_pabyBlockBuf, 0,
                               static_cast<size_t>(m_nBlockXSize) *
                                   m_nBlockYSize * nBands * nDTSize);
                    }
                    const auto nBufDTSize = GDALGetDataTypeSizeBytes(eBufType);
                    const GByte *pabySrcData =
                        static_cast<const GByte *>(pData) +
                        static_cast<size_t>(nYBlock - nYBlockStart) *
                            m_nBlockYSize * nLineSpace +
                        static_cast<size_t>(nXBlock - nXBlockStart) *
                            m_nBlockXSize * nPixelSpace;
                    if (bOrderedBands && nBandSpace == nBufDTSize &&
                        nPixelSpace == nBands * nBandSpace)
                    {
                        // Input buffer is pixel interleaved
                        for (int iY = 0; iY < nValidY; ++iY)
                        {
                            GDALCopyWords64(
                                pabySrcData +
                                    static_cast<size_t>(iY) * nLineSpace,
                                eBufType, nBufDTSize,
                                m_pabyBlockBuf + static_cast<size_t>(iY) *
                                                     m_nBlockXSize * nBands *
                                                     nDTSize,
                                eDataType, nDTSize,
                                static_cast<GPtrDiff_t>(nValidX) * nBands);
                        }
                    }
                    else
                    {
                        // "Random" spacing for input buffer
                        for (int iBand = 0; iBand < nBands; ++iBand)
                        {
                            for (int iY = 0; iY < nValidY; ++iY)
                            {
                                GDALCopyWords64(
                                    pabySrcData +
                                        static_cast<size_t>(iY) * nLineSpace,
                                    eBufType, static_cast<int>(nPixelSpace),
                                    m_pabyBlockBuf +
                                        (panBandMap[iBand] - 1 +
                                         static_cast<size_t>(iY) *
                                             m_nBlockXSize * nBands) *
                                            nDTSize,
                                    eDataType, nDTSize * nBands, nValidX);
                            }
                            pabySrcData += nBandSpace;
                        }
                    }

                    const int nBlockId =
                        poFirstBand->ComputeBlockId(nXBlock, nYBlock);
                    if (WriteEncodedTileOrStrip(
                            nBlockId, m_pabyBlockBuf,
                            /* bPreserveDataBuffer= */ false) != CE_None)
                    {
                        return CE_Failure;
                    }
                }
            }
            return CE_None;
        }
    }

    if (psExtraArg->eResampleAlg == GRIORA_NearestNeighbour)
        ++m_nJPEGOverviewVisibilityCounter;
    const CPLErr eErr = GDALPamDataset::IRasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace,
        psExtraArg);
    if (psExtraArg->eResampleAlg == GRIORA_NearestNeighbour)
        m_nJPEGOverviewVisibilityCounter--;

    if (pBufferedData)
    {
        VSIFree(pBufferedData);
        VSI_TIFFSetCachedRanges(TIFFClientdata(m_hTIFF), 0, nullptr, nullptr,
                                nullptr);
    }

    return eErr;
}

/************************************************************************/
/*                       GetGTIFFKeysFlavor()                           */
/************************************************************************/

GTIFFKeysFlavorEnum GetGTIFFKeysFlavor(CSLConstList papszOptions)
{
    const char *pszGeoTIFFKeysFlavor =
        CSLFetchNameValueDef(papszOptions, "GEOTIFF_KEYS_FLAVOR", "STANDARD");
    if (EQUAL(pszGeoTIFFKeysFlavor, "ESRI_PE"))
        return GEOTIFF_KEYS_ESRI_PE;
    return GEOTIFF_KEYS_STANDARD;
}

/************************************************************************/
/*                       GetGeoTIFFVersion()                            */
/************************************************************************/

GeoTIFFVersionEnum GetGeoTIFFVersion(CSLConstList papszOptions)
{
    const char *pszVersion =
        CSLFetchNameValueDef(papszOptions, "GEOTIFF_VERSION", "AUTO");
    if (EQUAL(pszVersion, "1.0"))
        return GEOTIFF_VERSION_1_0;
    if (EQUAL(pszVersion, "1.1"))
        return GEOTIFF_VERSION_1_1;
    return GEOTIFF_VERSION_AUTO;
}

/************************************************************************/
/*                      InitCreationOrOpenOptions()                     */
/************************************************************************/

void GTiffDataset::InitCreationOrOpenOptions(bool bUpdateMode,
                                             CSLConstList papszOptions)
{
    InitCompressionThreads(bUpdateMode, papszOptions);

    m_eGeoTIFFKeysFlavor = GetGTIFFKeysFlavor(papszOptions);
    m_eGeoTIFFVersion = GetGeoTIFFVersion(papszOptions);
}

/************************************************************************/
/*                          IsBlockAvailable()                          */
/*                                                                      */
/*      Return true if the indicated strip/tile is available.  We       */
/*      establish this by testing if the stripbytecount is zero.  If    */
/*      zero then the block has never been committed to disk.           */
/************************************************************************/

bool GTiffDataset::IsBlockAvailable(int nBlockId, vsi_l_offset *pnOffset,
                                    vsi_l_offset *pnSize, bool *pbErrOccurred)

{
    if (pbErrOccurred)
        *pbErrOccurred = false;

    std::pair<vsi_l_offset, vsi_l_offset> oPair;
    if (m_oCacheStrileToOffsetByteCount.tryGet(nBlockId, oPair))
    {
        if (pnOffset)
            *pnOffset = oPair.first;
        if (pnSize)
            *pnSize = oPair.second;
        return oPair.first != 0;
    }

    WaitCompletionForBlock(nBlockId);

    // Optimization to avoid fetching the whole Strip/TileCounts and
    // Strip/TileOffsets arrays.
    if (eAccess == GA_ReadOnly && !m_bStreamingIn)
    {
        int nErrOccurred = 0;
        auto bytecount =
            TIFFGetStrileByteCountWithErr(m_hTIFF, nBlockId, &nErrOccurred);
        if (nErrOccurred && pbErrOccurred)
            *pbErrOccurred = true;
        if (pnOffset)
        {
            *pnOffset =
                TIFFGetStrileOffsetWithErr(m_hTIFF, nBlockId, &nErrOccurred);
            if (nErrOccurred && pbErrOccurred)
                *pbErrOccurred = true;
        }
        if (pnSize)
            *pnSize = bytecount;
        return bytecount != 0;
    }

    toff_t *panByteCounts = nullptr;
    toff_t *panOffsets = nullptr;
    const bool bIsTiled = CPL_TO_BOOL(TIFFIsTiled(m_hTIFF));

    if ((bIsTiled &&
         TIFFGetField(m_hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts) &&
         (pnOffset == nullptr ||
          TIFFGetField(m_hTIFF, TIFFTAG_TILEOFFSETS, &panOffsets))) ||
        (!bIsTiled &&
         TIFFGetField(m_hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts) &&
         (pnOffset == nullptr ||
          TIFFGetField(m_hTIFF, TIFFTAG_STRIPOFFSETS, &panOffsets))))
    {
        if (panByteCounts == nullptr ||
            (pnOffset != nullptr && panOffsets == nullptr))
        {
            if (pbErrOccurred)
                *pbErrOccurred = true;
            return false;
        }
        const int nBlockCount =
            bIsTiled ? TIFFNumberOfTiles(m_hTIFF) : TIFFNumberOfStrips(m_hTIFF);
        if (nBlockId >= nBlockCount)
        {
            if (pbErrOccurred)
                *pbErrOccurred = true;
            return false;
        }

        if (pnOffset)
            *pnOffset = panOffsets[nBlockId];
        if (pnSize)
            *pnSize = panByteCounts[nBlockId];
        return panByteCounts[nBlockId] != 0;
    }
    else
    {
        if (pbErrOccurred)
            *pbErrOccurred = true;
    }

    return false;
}

/************************************************************************/
/*                           ReloadDirectory()                          */
/************************************************************************/

void GTiffDataset::ReloadDirectory(bool bReopenHandle)
{
    bool bNeedSetInvalidDir = true;
    if (bReopenHandle)
    {
        // When issuing a TIFFRewriteDirectory() or when a TIFFFlush() has
        // caused a move of the directory, we would need to invalidate the
        // tif_lastdiroff member, but it is not possible to do so without
        // re-opening the TIFF handle.
        auto hTIFFNew = VSI_TIFFReOpen(m_hTIFF);
        if (hTIFFNew != nullptr)
        {
            m_hTIFF = hTIFFNew;
            bNeedSetInvalidDir = false;  // we could do it, but not needed
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot re-open TIFF handle for file %s. "
                     "Directory chaining may be corrupted !",
                     m_pszFilename);
        }
    }
    if (bNeedSetInvalidDir)
    {
        TIFFSetSubDirectory(m_hTIFF, 0);
    }
    CPL_IGNORE_RET_VAL(SetDirectory());
}

/************************************************************************/
/*                            SetDirectory()                            */
/************************************************************************/

bool GTiffDataset::SetDirectory()

{
    Crystalize();

    if (TIFFCurrentDirOffset(m_hTIFF) == m_nDirOffset)
    {
        return true;
    }

    const int nSetDirResult = TIFFSetSubDirectory(m_hTIFF, m_nDirOffset);
    if (!nSetDirResult)
        return false;

    RestoreVolatileParameters(m_hTIFF);

    return true;
}

/************************************************************************/
/*                     GTiffSetDeflateSubCodec()                        */
/************************************************************************/

void GTiffSetDeflateSubCodec(TIFF *hTIFF)
{
    (void)hTIFF;

#if defined(TIFFTAG_DEFLATE_SUBCODEC) && defined(LIBDEFLATE_SUPPORT)
    // Mostly for strict reproducibility purposes
    if (EQUAL(CPLGetConfigOption("GDAL_TIFF_DEFLATE_SUBCODEC", ""), "ZLIB"))
    {
        TIFFSetField(hTIFF, TIFFTAG_DEFLATE_SUBCODEC, DEFLATE_SUBCODEC_ZLIB);
    }
#endif
}

/************************************************************************/
/*                     RestoreVolatileParameters()                      */
/************************************************************************/

void GTiffDataset::RestoreVolatileParameters(TIFF *hTIFF)
{

    /* -------------------------------------------------------------------- */
    /*      YCbCr JPEG compressed images should be translated on the fly    */
    /*      to RGB by libtiff/libjpeg unless specifically requested         */
    /*      otherwise.                                                      */
    /* -------------------------------------------------------------------- */
    if (m_nCompression == COMPRESSION_JPEG &&
        m_nPhotometric == PHOTOMETRIC_YCBCR &&
        CPLTestBool(CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES")))
    {
        int nColorMode = JPEGCOLORMODE_RAW;  // Initialize to 0;

        TIFFGetField(hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode);
        if (nColorMode != JPEGCOLORMODE_RGB)
        {
            TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }
    }

    if (m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        m_nCompression == COMPRESSION_LERC)
    {
        GTiffSetDeflateSubCodec(hTIFF);
    }

    /* -------------------------------------------------------------------- */
    /*      Propagate any quality settings.                                 */
    /* -------------------------------------------------------------------- */
    if (eAccess == GA_Update)
    {
        // Now, reset zip and jpeg quality.
        if (m_nJpegQuality > 0 && m_nCompression == COMPRESSION_JPEG)
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("GTiff", "Propagate JPEG_QUALITY(%d) in SetDirectory()",
                     m_nJpegQuality);
#endif
            TIFFSetField(hTIFF, TIFFTAG_JPEGQUALITY, m_nJpegQuality);
        }
        if (m_nJpegTablesMode >= 0 && m_nCompression == COMPRESSION_JPEG)
            TIFFSetField(hTIFF, TIFFTAG_JPEGTABLESMODE, m_nJpegTablesMode);
        if (m_nZLevel > 0 && (m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
                              m_nCompression == COMPRESSION_LERC))
            TIFFSetField(hTIFF, TIFFTAG_ZIPQUALITY, m_nZLevel);
        if (m_nLZMAPreset > 0 && m_nCompression == COMPRESSION_LZMA)
            TIFFSetField(hTIFF, TIFFTAG_LZMAPRESET, m_nLZMAPreset);
        if (m_nZSTDLevel > 0 && (m_nCompression == COMPRESSION_ZSTD ||
                                 m_nCompression == COMPRESSION_LERC))
            TIFFSetField(hTIFF, TIFFTAG_ZSTD_LEVEL, m_nZSTDLevel);
        if (m_nCompression == COMPRESSION_LERC)
        {
            TIFFSetField(hTIFF, TIFFTAG_LERC_MAXZERROR, m_dfMaxZError);
        }
        if (m_nWebPLevel > 0 && m_nCompression == COMPRESSION_WEBP)
            TIFFSetField(hTIFF, TIFFTAG_WEBP_LEVEL, m_nWebPLevel);
        if (m_bWebPLossless && m_nCompression == COMPRESSION_WEBP)
            TIFFSetField(hTIFF, TIFFTAG_WEBP_LOSSLESS, 1);
#ifdef HAVE_JXL
        if (m_nCompression == COMPRESSION_JXL)
        {
            TIFFSetField(hTIFF, TIFFTAG_JXL_LOSSYNESS,
                         m_bJXLLossless ? JXL_LOSSLESS : JXL_LOSSY);
            TIFFSetField(hTIFF, TIFFTAG_JXL_EFFORT, m_nJXLEffort);
            TIFFSetField(hTIFF, TIFFTAG_JXL_DISTANCE, m_fJXLDistance);
            TIFFSetField(hTIFF, TIFFTAG_JXL_ALPHA_DISTANCE,
                         m_fJXLAlphaDistance);
        }
#endif
    }
}

/************************************************************************/
/*                     ComputeBlocksPerColRowAndBand()                  */
/************************************************************************/

bool GTiffDataset::ComputeBlocksPerColRowAndBand(int l_nBands)
{
    m_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, m_nBlockYSize);
    m_nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, m_nBlockXSize);
    if (m_nBlocksPerColumn > INT_MAX / m_nBlocksPerRow)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Too many blocks: %d x %d",
                    m_nBlocksPerRow, m_nBlocksPerColumn);
        return false;
    }

    // Note: we could potentially go up to UINT_MAX blocks, but currently
    // we use a int nBlockId
    m_nBlocksPerBand = m_nBlocksPerColumn * m_nBlocksPerRow;
    if (m_nPlanarConfig == PLANARCONFIG_SEPARATE &&
        m_nBlocksPerBand > INT_MAX / l_nBands)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Too many blocks: %d x %d x %d bands", m_nBlocksPerRow,
                    m_nBlocksPerColumn, l_nBands);
        return false;
    }
    return true;
}

/************************************************************************/
/*                   SetStructuralMDFromParent()                        */
/************************************************************************/

void GTiffDataset::SetStructuralMDFromParent(GTiffDataset *poParentDS)
{
    m_bBlockOrderRowMajor = poParentDS->m_bBlockOrderRowMajor;
    m_bLeaderSizeAsUInt4 = poParentDS->m_bLeaderSizeAsUInt4;
    m_bTrailerRepeatedLast4BytesRepeated =
        poParentDS->m_bTrailerRepeatedLast4BytesRepeated;
    m_bMaskInterleavedWithImagery = poParentDS->m_bMaskInterleavedWithImagery;
    m_bWriteEmptyTiles = poParentDS->m_bWriteEmptyTiles;
}

/************************************************************************/
/*                          ScanDirectories()                           */
/*                                                                      */
/*      Scan through all the directories finding overviews, masks       */
/*      and subdatasets.                                                */
/************************************************************************/

void GTiffDataset::ScanDirectories()

{
    /* -------------------------------------------------------------------- */
    /*      We only scan once.  We do not scan for non-base datasets.       */
    /* -------------------------------------------------------------------- */
    if (!m_bScanDeferred)
        return;

    m_bScanDeferred = false;

    if (m_poBaseDS)
        return;

    Crystalize();

    CPLDebug("GTiff", "ScanDirectories()");

    /* ==================================================================== */
    /*      Scan all directories.                                           */
    /* ==================================================================== */
    CPLStringList aosSubdatasets;
    int iDirIndex = 0;

    FlushDirectory();

    do
    {
        toff_t nTopDir = TIFFCurrentDirOffset(m_hTIFF);
        uint32_t nSubType = 0;

        ++iDirIndex;

        toff_t *tmpSubIFDOffsets = nullptr;
        toff_t *subIFDOffsets = nullptr;
        uint16_t nSubIFDs = 0;
        if (TIFFGetField(m_hTIFF, TIFFTAG_SUBIFD, &nSubIFDs,
                         &tmpSubIFDOffsets) &&
            iDirIndex == 1)
        {
            subIFDOffsets =
                static_cast<toff_t *>(CPLMalloc(nSubIFDs * sizeof(toff_t)));
            for (uint16_t iSubIFD = 0; iSubIFD < nSubIFDs; iSubIFD++)
            {
                subIFDOffsets[iSubIFD] = tmpSubIFDOffsets[iSubIFD];
            }
        }

        // early break for backwards compatibility: if the first directory read
        // is also the last, and there are no subIFDs, no use continuing
        if (iDirIndex == 1 && nSubIFDs == 0 && TIFFLastDirectory(m_hTIFF))
        {
            CPLFree(subIFDOffsets);
            break;
        }

        for (uint16_t iSubIFD = 0; iSubIFD <= nSubIFDs; iSubIFD++)
        {
            toff_t nThisDir = nTopDir;
            if (iSubIFD > 0 && iDirIndex > 1)  // don't read subIFDs if we are
                                               // not in the original directory
                break;
            if (iSubIFD > 0)
            {
                // make static analyzer happy. subIFDOffsets cannot be null if
                // iSubIFD>0
                assert(subIFDOffsets != nullptr);
                nThisDir = subIFDOffsets[iSubIFD - 1];
                // CPLDebug("GTiff", "Opened subIFD %d/%d at offset %llu.",
                // iSubIFD, nSubIFDs, nThisDir);
                if (!TIFFSetSubDirectory(m_hTIFF, nThisDir))
                    break;
            }

            if (!TIFFGetField(m_hTIFF, TIFFTAG_SUBFILETYPE, &nSubType))
                nSubType = 0;

            /* Embedded overview of the main image */
            if ((nSubType & FILETYPE_REDUCEDIMAGE) != 0 &&
                (nSubType & FILETYPE_MASK) == 0 &&
                ((nSubIFDs == 0 && iDirIndex != 1) || iSubIFD > 0) &&
                m_nOverviewCount < 30 /* to avoid DoS */)
            {
                GTiffDataset *poODS = new GTiffDataset();
                poODS->ShareLockWithParentDataset(this);
                poODS->SetStructuralMDFromParent(this);
                poODS->m_pszFilename = CPLStrdup(m_pszFilename);
                if (poODS->OpenOffset(VSI_TIFFOpenChild(m_hTIFF), nThisDir,
                                      eAccess) != CE_None ||
                    poODS->GetRasterCount() != GetRasterCount())
                {
                    delete poODS;
                }
                else
                {
                    CPLDebug("GTiff", "Opened %dx%d overview.",
                             poODS->GetRasterXSize(), poODS->GetRasterYSize());
                    ++m_nOverviewCount;
                    m_papoOverviewDS = static_cast<GTiffDataset **>(CPLRealloc(
                        m_papoOverviewDS, m_nOverviewCount * (sizeof(void *))));
                    m_papoOverviewDS[m_nOverviewCount - 1] = poODS;
                    poODS->m_poBaseDS = this;
                    poODS->m_bIsOverview = true;

                    // Propagate a few compression related settings that are
                    // no preserved at the TIFF tag level, but may be set in
                    // the GDAL_METADATA tag in the IMAGE_STRUCTURE domain
                    // Note: this might not be totally reflecting the reality
                    // if users have created overviews with different settings
                    // but this is probably better than the default ones
                    poODS->m_nWebPLevel = m_nWebPLevel;
                    // below is not a copy & paste error: we transfer the
                    // m_dfMaxZErrorOverview overview of the parent to
                    // m_dfMaxZError of the overview
                    poODS->m_dfMaxZError = m_dfMaxZErrorOverview;
                    poODS->m_dfMaxZErrorOverview = m_dfMaxZErrorOverview;
#if HAVE_JXL
                    poODS->m_bJXLLossless = m_bJXLLossless;
                    poODS->m_fJXLDistance = m_fJXLDistance;
                    poODS->m_fJXLAlphaDistance = m_fJXLAlphaDistance;
                    poODS->m_nJXLEffort = m_nJXLEffort;
#endif
                    // Those ones are not serialized currently..
                    // poODS->m_nZLevel = m_nZLevel;
                    // poODS->m_nLZMAPreset = m_nLZMAPreset;
                    // poODS->m_nZSTDLevel = m_nZSTDLevel;
                }
            }
            // Embedded mask of the main image.
            else if ((nSubType & FILETYPE_MASK) != 0 &&
                     (nSubType & FILETYPE_REDUCEDIMAGE) == 0 &&
                     ((nSubIFDs == 0 && iDirIndex != 1) || iSubIFD > 0) &&
                     m_poMaskDS == nullptr)
            {
                m_poMaskDS = new GTiffDataset();
                m_poMaskDS->ShareLockWithParentDataset(this);
                m_poMaskDS->SetStructuralMDFromParent(this);
                m_poMaskDS->m_pszFilename = CPLStrdup(m_pszFilename);

                // The TIFF6 specification - page 37 - only allows 1
                // SamplesPerPixel and 1 BitsPerSample Here we support either 1
                // or 8 bit per sample and we support either 1 sample per pixel
                // or as many samples as in the main image We don't check the
                // value of the PhotometricInterpretation tag, which should be
                // set to "Transparency mask" (4) according to the specification
                // (page 36).  However, the TIFF6 specification allows image
                // masks to have a higher resolution than the main image, what
                // we don't support here.

                if (m_poMaskDS->OpenOffset(VSI_TIFFOpenChild(m_hTIFF), nThisDir,
                                           eAccess) != CE_None ||
                    m_poMaskDS->GetRasterCount() == 0 ||
                    !(m_poMaskDS->GetRasterCount() == 1 ||
                      m_poMaskDS->GetRasterCount() == GetRasterCount()) ||
                    m_poMaskDS->GetRasterXSize() != GetRasterXSize() ||
                    m_poMaskDS->GetRasterYSize() != GetRasterYSize() ||
                    m_poMaskDS->GetRasterBand(1)->GetRasterDataType() !=
                        GDT_Byte)
                {
                    delete m_poMaskDS;
                    m_poMaskDS = nullptr;
                }
                else
                {
                    CPLDebug("GTiff", "Opened band mask.");
                    m_poMaskDS->m_poBaseDS = this;
                    m_poMaskDS->m_poImageryDS = this;

                    m_poMaskDS->m_bPromoteTo8Bits =
                        CPLTestBool(CPLGetConfigOption(
                            "GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
                }
            }

            // Embedded mask of an overview.  The TIFF6 specification allows the
            // combination of the FILETYPE_xxxx masks.
            else if ((nSubType & FILETYPE_REDUCEDIMAGE) != 0 &&
                     (nSubType & FILETYPE_MASK) != 0 &&
                     ((nSubIFDs == 0 && iDirIndex != 1) || iSubIFD > 0))
            {
                GTiffDataset *poDS = new GTiffDataset();
                poDS->ShareLockWithParentDataset(this);
                poDS->SetStructuralMDFromParent(this);
                poDS->m_pszFilename = CPLStrdup(m_pszFilename);
                if (poDS->OpenOffset(VSI_TIFFOpenChild(m_hTIFF), nThisDir,
                                     eAccess) != CE_None ||
                    poDS->GetRasterCount() == 0 ||
                    poDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
                {
                    delete poDS;
                }
                else
                {
                    int i = 0;  // Used after for.
                    for (; i < m_nOverviewCount; ++i)
                    {
                        auto poOvrDS = cpl::down_cast<GTiffDataset *>(
                            GDALDataset::FromHandle(m_papoOverviewDS[i]));
                        if (poOvrDS->m_poMaskDS == nullptr &&
                            poDS->GetRasterXSize() ==
                                m_papoOverviewDS[i]->GetRasterXSize() &&
                            poDS->GetRasterYSize() ==
                                m_papoOverviewDS[i]->GetRasterYSize() &&
                            (poDS->GetRasterCount() == 1 ||
                             poDS->GetRasterCount() == GetRasterCount()))
                        {
                            CPLDebug(
                                "GTiff", "Opened band mask for %dx%d overview.",
                                poDS->GetRasterXSize(), poDS->GetRasterYSize());
                            poDS->m_poImageryDS = poOvrDS;
                            poOvrDS->m_poMaskDS = poDS;
                            poDS->m_bPromoteTo8Bits =
                                CPLTestBool(CPLGetConfigOption(
                                    "GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
                            poDS->m_poBaseDS = this;
                            break;
                        }
                    }
                    if (i == m_nOverviewCount)
                    {
                        delete poDS;
                    }
                }
            }
            else if (!m_bSingleIFDOpened &&
                     (nSubType == 0 || nSubType == FILETYPE_PAGE))
            {
                uint32_t nXSize = 0;
                uint32_t nYSize = 0;

                TIFFGetField(m_hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize);
                TIFFGetField(m_hTIFF, TIFFTAG_IMAGELENGTH, &nYSize);

                // For Geodetic TIFF grids (GTG)
                // (https://proj.org/specifications/geodetictiffgrids.html)
                // extract the grid_name to put it in the description
                std::string osFriendlyName;
                char *pszText = nullptr;
                if (TIFFGetField(m_hTIFF, TIFFTAG_GDAL_METADATA, &pszText) &&
                    strstr(pszText, "grid_name") != nullptr)
                {
                    CPLXMLNode *psRoot = CPLParseXMLString(pszText);
                    const CPLXMLNode *psItem =
                        psRoot ? CPLGetXMLNode(psRoot, "=GDALMetadata")
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
                        int nBand =
                            atoi(CPLGetXMLValue(psItem, "sample", "-1"));
                        if (pszKey && pszValue && nBand <= 0 &&
                            EQUAL(pszKey, "grid_name"))
                        {
                            osFriendlyName = ": ";
                            osFriendlyName += pszValue;
                            break;
                        }
                    }

                    CPLDestroyXMLNode(psRoot);
                }

                if (nXSize > INT_MAX || nYSize > INT_MAX)
                {
                    CPLDebug("GTiff",
                             "Skipping directory with too large image: %u x %u",
                             nXSize, nYSize);
                }
                else
                {
                    uint16_t nSPP = 0;
                    if (!TIFFGetField(m_hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSPP))
                        nSPP = 1;

                    CPLString osName, osDesc;
                    osName.Printf("SUBDATASET_%d_NAME=GTIFF_DIR:%d:%s",
                                  iDirIndex, iDirIndex, m_pszFilename);
                    osDesc.Printf(
                        "SUBDATASET_%d_DESC=Page %d (%dP x %dL x %dB)",
                        iDirIndex, iDirIndex, static_cast<int>(nXSize),
                        static_cast<int>(nYSize), nSPP);
                    osDesc += osFriendlyName;

                    aosSubdatasets.AddString(osName);
                    aosSubdatasets.AddString(osDesc);
                }
            }
        }
        CPLFree(subIFDOffsets);

        // Make sure we are stepping from the expected directory regardless
        // of churn done processing the above.
        if (TIFFCurrentDirOffset(m_hTIFF) != nTopDir)
            TIFFSetSubDirectory(m_hTIFF, nTopDir);
    } while (!m_bSingleIFDOpened && !TIFFLastDirectory(m_hTIFF) &&
             TIFFReadDirectory(m_hTIFF) != 0);

    ReloadDirectory();

    // If we have a mask for the main image, loop over the overviews, and if
    // they have a mask, let's set this mask as an overview of the main mask.
    if (m_poMaskDS != nullptr)
    {
        for (int i = 0; i < m_nOverviewCount; ++i)
        {
            if (cpl::down_cast<GTiffDataset *>(
                    GDALDataset::FromHandle(m_papoOverviewDS[i]))
                    ->m_poMaskDS != nullptr)
            {
                ++m_poMaskDS->m_nOverviewCount;
                m_poMaskDS->m_papoOverviewDS =
                    static_cast<GTiffDataset **>(CPLRealloc(
                        m_poMaskDS->m_papoOverviewDS,
                        m_poMaskDS->m_nOverviewCount * (sizeof(void *))));
                m_poMaskDS->m_papoOverviewDS[m_poMaskDS->m_nOverviewCount - 1] =
                    cpl::down_cast<GTiffDataset *>(
                        GDALDataset::FromHandle(m_papoOverviewDS[i]))
                        ->m_poMaskDS;
            }
        }
    }

    // Assign color interpretation from main dataset
    const int l_nBands = GetRasterCount();
    for (int iOvr = 0; iOvr < m_nOverviewCount; ++iOvr)
    {
        for (int i = 1; i <= l_nBands; i++)
        {
            auto poBand = dynamic_cast<GTiffRasterBand *>(
                m_papoOverviewDS[iOvr]->GetRasterBand(i));
            if (poBand)
                poBand->m_eBandInterp =
                    GetRasterBand(i)->GetColorInterpretation();
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Only keep track of subdatasets if we have more than one         */
    /*      subdataset (pair).                                              */
    /* -------------------------------------------------------------------- */
    if (aosSubdatasets.size() > 2)
    {
        m_oGTiffMDMD.SetMetadata(aosSubdatasets.List(), "SUBDATASETS");
    }
}

/************************************************************************/
/*                         GetInternalHandle()                          */
/************************************************************************/

void *GTiffDataset::GetInternalHandle(const char * /* pszHandleName */)

{
    return m_hTIFF;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **GTiffDataset::GetFileList()

{
    LoadGeoreferencingAndPamIfNeeded();

    char **papszFileList = GDALPamDataset::GetFileList();

    LoadMetadata();
    if (nullptr != m_papszMetadataFiles)
    {
        for (int i = 0; m_papszMetadataFiles[i] != nullptr; ++i)
        {
            if (CSLFindString(papszFileList, m_papszMetadataFiles[i]) < 0)
            {
                papszFileList =
                    CSLAddString(papszFileList, m_papszMetadataFiles[i]);
            }
        }
    }

    if (m_pszGeorefFilename &&
        CSLFindString(papszFileList, m_pszGeorefFilename) == -1)
    {
        papszFileList = CSLAddString(papszFileList, m_pszGeorefFilename);
    }

    if (m_nXMLGeorefSrcIndex >= 0)
        LookForProjection();

    if (m_pszXMLFilename &&
        CSLFindString(papszFileList, m_pszXMLFilename) == -1)
    {
        papszFileList = CSLAddString(papszFileList, m_pszXMLFilename);
    }

    return papszFileList;
}

/************************************************************************/
/*                        GetRawBinaryLayout()                          */
/************************************************************************/

bool GTiffDataset::GetRawBinaryLayout(GDALDataset::RawBinaryLayout &sLayout)
{
    if (eAccess == GA_Update)
    {
        FlushCache(false);
        Crystalize();
    }

    if (m_nCompression != COMPRESSION_NONE)
        return false;
    if (!CPLIsPowerOfTwo(m_nBitsPerSample) || m_nBitsPerSample < 8)
        return false;
    const auto eDT = GetRasterBand(1)->GetRasterDataType();
    if (GDALDataTypeIsComplex(eDT))
        return false;

    toff_t *panByteCounts = nullptr;
    toff_t *panOffsets = nullptr;
    const bool bIsTiled = CPL_TO_BOOL(TIFFIsTiled(m_hTIFF));

    if (!((bIsTiled &&
           TIFFGetField(m_hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts) &&
           TIFFGetField(m_hTIFF, TIFFTAG_TILEOFFSETS, &panOffsets)) ||
          (!bIsTiled &&
           TIFFGetField(m_hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts) &&
           TIFFGetField(m_hTIFF, TIFFTAG_STRIPOFFSETS, &panOffsets))))
    {
        return false;
    }

    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    vsi_l_offset nImgOffset = panOffsets[0];
    GIntBig nPixelOffset = (m_nPlanarConfig == PLANARCONFIG_CONTIG)
                               ? static_cast<GIntBig>(nDTSize) * nBands
                               : nDTSize;
    GIntBig nLineOffset = nPixelOffset * nRasterXSize;
    GIntBig nBandOffset =
        (m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands > 1) ? nDTSize : 0;
    RawBinaryLayout::Interleaving eInterleaving =
        (nBands == 1) ? RawBinaryLayout::Interleaving::UNKNOWN
        : (m_nPlanarConfig == PLANARCONFIG_CONTIG)
            ? RawBinaryLayout::Interleaving::BIP
            : RawBinaryLayout::Interleaving::BSQ;
    if (bIsTiled)
    {
        // Only a single block tiled file with same dimension as the raster
        // might be acceptable
        if (m_nBlockXSize != nRasterXSize || m_nBlockYSize != nRasterYSize)
            return false;
        if (nBands > 1 && m_nPlanarConfig != PLANARCONFIG_CONTIG)
        {
            nBandOffset = static_cast<GIntBig>(panOffsets[1]) -
                          static_cast<GIntBig>(panOffsets[0]);
            for (int i = 2; i < nBands; i++)
            {
                if (static_cast<GIntBig>(panOffsets[i]) -
                        static_cast<GIntBig>(panOffsets[i - 1]) !=
                    nBandOffset)
                    return false;
            }
        }
    }
    else
    {
        const int nStrips = DIV_ROUND_UP(nRasterYSize, m_nRowsPerStrip);
        if (nBands == 1 || m_nPlanarConfig == PLANARCONFIG_CONTIG)
        {
            vsi_l_offset nLastStripEnd = panOffsets[0] + panByteCounts[0];
            for (int iStrip = 1; iStrip < nStrips; iStrip++)
            {
                if (nLastStripEnd != panOffsets[iStrip])
                    return false;
                nLastStripEnd = panOffsets[iStrip] + panByteCounts[iStrip];
            }
        }
        else
        {
            // Note: we could potentially have BIL order with m_nRowsPerStrip ==
            // 1 and if strips are ordered strip_line_1_band_1, ...,
            // strip_line_1_band_N, strip_line2_band1, ... strip_line2_band_N,
            // etc.... but that'd be faily exotic ! So only detect BSQ layout
            // here
            nBandOffset = static_cast<GIntBig>(panOffsets[nStrips]) -
                          static_cast<GIntBig>(panOffsets[0]);
            for (int i = 0; i < nBands; i++)
            {
                uint32_t iStripOffset = nStrips * i;
                vsi_l_offset nLastStripEnd =
                    panOffsets[iStripOffset] + panByteCounts[iStripOffset];
                for (int iStrip = 1; iStrip < nStrips; iStrip++)
                {
                    if (nLastStripEnd != panOffsets[iStripOffset + iStrip])
                        return false;
                    nLastStripEnd = panOffsets[iStripOffset + iStrip] +
                                    panByteCounts[iStripOffset + iStrip];
                }
                if (i >= 2 && static_cast<GIntBig>(panOffsets[iStripOffset]) -
                                      static_cast<GIntBig>(
                                          panOffsets[iStripOffset - nStrips]) !=
                                  nBandOffset)
                {
                    return false;
                }
            }
        }
    }

    sLayout.osRawFilename = m_pszFilename;
    sLayout.eInterleaving = eInterleaving;
    sLayout.eDataType = eDT;
#ifdef CPL_LSB
    sLayout.bLittleEndianOrder = !TIFFIsByteSwapped(m_hTIFF);
#else
    sLayout.bLittleEndianOrder = TIFFIsByteSwapped(m_hTIFF);
#endif
    sLayout.nImageOffset = nImgOffset;
    sLayout.nPixelOffset = nPixelOffset;
    sLayout.nLineOffset = nLineOffset;
    sLayout.nBandOffset = nBandOffset;

    return true;
}

/************************************************************************/
/*               GTiffDatasetLibGeotiffErrorCallback()                  */
/************************************************************************/

static void GTiffDatasetLibGeotiffErrorCallback(GTIF *, int level,
                                                const char *pszMsg, ...)
{
    va_list ap;
    va_start(ap, pszMsg);
    CPLErrorV((level == LIBGEOTIFF_WARNING) ? CE_Warning : CE_Failure,
              CPLE_AppDefined, pszMsg, ap);
    va_end(ap);
}

/************************************************************************/
/*                           GTIFNew()                                  */
/************************************************************************/

/* static */ GTIF *GTiffDataset::GTIFNew(TIFF *hTIFF)
{
    GTIF *gtif = GTIFNewEx(hTIFF, GTiffDatasetLibGeotiffErrorCallback, nullptr);
    if (gtif)
    {
        GTIFAttachPROJContext(gtif, OSRGetProjTLSContext());
    }
    return gtif;
}
