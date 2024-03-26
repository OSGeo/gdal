/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  General methods of GTiffRasterBand
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

#include "gtiffrasterband.h"
#include "gtiffdataset.h"

#include <algorithm>
#include <set>

#include "cpl_vsi_virtual.h"
#include "tifvsi.h"

/************************************************************************/
/*                           GTiffRasterBand()                          */
/************************************************************************/

GTiffRasterBand::GTiffRasterBand(GTiffDataset *poDSIn, int nBandIn)
    : m_poGDS(poDSIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

    /* -------------------------------------------------------------------- */
    /*      Get the GDAL data type.                                         */
    /* -------------------------------------------------------------------- */
    const uint16_t nBitsPerSample = m_poGDS->m_nBitsPerSample;
    const uint16_t nSampleFormat = m_poGDS->m_nSampleFormat;

    eDataType = GDT_Unknown;

    if (nBitsPerSample <= 8)
    {
        if (nSampleFormat == SAMPLEFORMAT_INT)
            eDataType = GDT_Int8;
        else
            eDataType = GDT_Byte;
    }
    else if (nBitsPerSample <= 16)
    {
        if (nSampleFormat == SAMPLEFORMAT_INT)
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;
    }
    else if (nBitsPerSample == 32)
    {
        if (nSampleFormat == SAMPLEFORMAT_COMPLEXINT)
            eDataType = GDT_CInt16;
        else if (nSampleFormat == SAMPLEFORMAT_IEEEFP)
            eDataType = GDT_Float32;
        else if (nSampleFormat == SAMPLEFORMAT_INT)
            eDataType = GDT_Int32;
        else
            eDataType = GDT_UInt32;
    }
    else if (nBitsPerSample == 64)
    {
        if (nSampleFormat == SAMPLEFORMAT_IEEEFP)
            eDataType = GDT_Float64;
        else if (nSampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP)
            eDataType = GDT_CFloat32;
        else if (nSampleFormat == SAMPLEFORMAT_COMPLEXINT)
            eDataType = GDT_CInt32;
        else if (nSampleFormat == SAMPLEFORMAT_INT)
            eDataType = GDT_Int64;
        else
            eDataType = GDT_UInt64;
    }
    else if (nBitsPerSample == 128)
    {
        if (nSampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP)
            eDataType = GDT_CFloat64;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to work out band color interpretation.                      */
    /* -------------------------------------------------------------------- */
    bool bLookForExtraSamples = false;

    if (m_poGDS->m_poColorTable != nullptr && nBand == 1)
    {
        m_eBandInterp = GCI_PaletteIndex;
    }
    else if (m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB ||
             (m_poGDS->m_nPhotometric == PHOTOMETRIC_YCBCR &&
              m_poGDS->m_nCompression == COMPRESSION_JPEG &&
              CPLTestBool(CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES"))))
    {
        if (nBand == 1)
            m_eBandInterp = GCI_RedBand;
        else if (nBand == 2)
            m_eBandInterp = GCI_GreenBand;
        else if (nBand == 3)
            m_eBandInterp = GCI_BlueBand;
        else
            bLookForExtraSamples = true;
    }
    else if (m_poGDS->m_nPhotometric == PHOTOMETRIC_YCBCR)
    {
        if (nBand == 1)
            m_eBandInterp = GCI_YCbCr_YBand;
        else if (nBand == 2)
            m_eBandInterp = GCI_YCbCr_CbBand;
        else if (nBand == 3)
            m_eBandInterp = GCI_YCbCr_CrBand;
        else
            bLookForExtraSamples = true;
    }
    else if (m_poGDS->m_nPhotometric == PHOTOMETRIC_SEPARATED)
    {
        if (nBand == 1)
            m_eBandInterp = GCI_CyanBand;
        else if (nBand == 2)
            m_eBandInterp = GCI_MagentaBand;
        else if (nBand == 3)
            m_eBandInterp = GCI_YellowBand;
        else if (nBand == 4)
            m_eBandInterp = GCI_BlackBand;
        else
            bLookForExtraSamples = true;
    }
    else if (m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISBLACK && nBand == 1)
    {
        m_eBandInterp = GCI_GrayIndex;
    }
    else
    {
        bLookForExtraSamples = true;
    }

    if (bLookForExtraSamples)
    {
        uint16_t *v = nullptr;
        uint16_t count = 0;

        if (TIFFGetField(m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v))
        {
            const int nBaseSamples = m_poGDS->m_nSamplesPerPixel - count;
            const int nExpectedBaseSamples =
                (m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISBLACK)   ? 1
                : (m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISWHITE) ? 1
                : (m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB)        ? 3
                : (m_poGDS->m_nPhotometric == PHOTOMETRIC_YCBCR)      ? 3
                : (m_poGDS->m_nPhotometric == PHOTOMETRIC_SEPARATED)  ? 4
                                                                      : 0;

            if (nExpectedBaseSamples > 0 && nBand == nExpectedBaseSamples + 1 &&
                nBaseSamples != nExpectedBaseSamples)
            {
                ReportError(
                    CE_Warning, CPLE_AppDefined,
                    "Wrong number of ExtraSamples : %d. %d were expected",
                    count, m_poGDS->m_nSamplesPerPixel - nExpectedBaseSamples);
            }

            if (nBand > nBaseSamples && nBand - nBaseSamples - 1 < count &&
                (v[nBand - nBaseSamples - 1] == EXTRASAMPLE_ASSOCALPHA ||
                 v[nBand - nBaseSamples - 1] == EXTRASAMPLE_UNASSALPHA))
                m_eBandInterp = GCI_AlphaBand;
            else
                m_eBandInterp = GCI_Undefined;
        }
        else
        {
            m_eBandInterp = GCI_Undefined;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Establish block size for strip or tiles.                        */
    /* -------------------------------------------------------------------- */
    nBlockXSize = m_poGDS->m_nBlockXSize;
    nBlockYSize = m_poGDS->m_nBlockYSize;
    nRasterXSize = m_poGDS->nRasterXSize;
    nRasterYSize = m_poGDS->nRasterYSize;
    nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);
}

/************************************************************************/
/*                          ~GTiffRasterBand()                          */
/************************************************************************/

GTiffRasterBand::~GTiffRasterBand()
{
    // So that any future DropReferenceVirtualMem() will not try to access the
    // raster band object, but this would not conform to the advertised
    // contract.
    if (!m_aSetPSelf.empty())
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                    "Virtual memory objects still exist at GTiffRasterBand "
                    "destruction");
        std::set<GTiffRasterBand **>::iterator oIter = m_aSetPSelf.begin();
        for (; oIter != m_aSetPSelf.end(); ++oIter)
            *(*oIter) = nullptr;
    }
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GTiffRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                  int nXSize, int nYSize, void *pData,
                                  int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType, GSpacing nPixelSpace,
                                  GSpacing nLineSpace,
                                  GDALRasterIOExtraArg *psExtraArg)
{
#if DEBUG_VERBOSE
    CPLDebug("GTiff", "RasterIO(%d, %d, %d, %d, %d, %d)", nXOff, nYOff, nXSize,
             nYSize, nBufXSize, nBufYSize);
#endif

    // Try to pass the request to the most appropriate overview dataset.
    if (nBufXSize < nXSize && nBufYSize < nYSize)
    {
        int bTried = FALSE;
        if (psExtraArg->eResampleAlg == GRIORA_NearestNeighbour)
            ++m_poGDS->m_nJPEGOverviewVisibilityCounter;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg, &bTried);
        if (psExtraArg->eResampleAlg == GRIORA_NearestNeighbour)
            --m_poGDS->m_nJPEGOverviewVisibilityCounter;
        if (bTried)
            return eErr;
    }

    if (m_poGDS->m_eVirtualMemIOUsage != GTiffDataset::VirtualMemIOEnum::NO)
    {
        const int nErr = m_poGDS->VirtualMemIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, 1, &nBand, nPixelSpace, nLineSpace, 0, psExtraArg);
        if (nErr >= 0)
            return static_cast<CPLErr>(nErr);
    }
    if (m_poGDS->m_bDirectIO)
    {
        int nErr =
            DirectIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
                     nBufYSize, eBufType, nPixelSpace, nLineSpace, psExtraArg);
        if (nErr >= 0)
            return static_cast<CPLErr>(nErr);
    }

    bool bCanUseMultiThreadedRead = false;
    if (m_poGDS->m_nDisableMultiThreadedRead == 0 && eRWFlag == GF_Read &&
        m_poGDS->m_poThreadPool != nullptr && nXSize == nBufXSize &&
        nYSize == nBufYSize && m_poGDS->IsMultiThreadedReadCompatible())
    {
        const int nBlockX1 = nXOff / nBlockXSize;
        const int nBlockY1 = nYOff / nBlockYSize;
        const int nBlockX2 = (nXOff + nXSize - 1) / nBlockXSize;
        const int nBlockY2 = (nYOff + nYSize - 1) / nBlockYSize;
        const int nXBlocks = nBlockX2 - nBlockX1 + 1;
        const int nYBlocks = nBlockY2 - nBlockY1 + 1;
        if (nXBlocks > 1 || nYBlocks > 1)
        {
            bCanUseMultiThreadedRead = true;
        }
    }

    // Cleanup data cached by below CacheMultiRange() call.
    struct BufferedDataFreer
    {
        void *m_pBufferedData = nullptr;
        TIFF *m_hTIFF = nullptr;

        void Init(void *pBufferedData, TIFF *hTIFF)
        {
            m_pBufferedData = pBufferedData;
            m_hTIFF = hTIFF;
        }

        ~BufferedDataFreer()
        {
            if (m_pBufferedData)
            {
                VSIFree(m_pBufferedData);
                VSI_TIFFSetCachedRanges(TIFFClientdata(m_hTIFF), 0, nullptr,
                                        nullptr, nullptr);
            }
        }
    };

    // bufferedDataFreer must be left in this scope !
    BufferedDataFreer bufferedDataFreer;

    if (m_poGDS->eAccess == GA_ReadOnly && eRWFlag == GF_Read &&
        m_poGDS->HasOptimizedReadMultiRange())
    {
        if (bCanUseMultiThreadedRead &&
            VSI_TIFFGetVSILFile(TIFFClientdata(m_poGDS->m_hTIFF))->HasPRead())
        {
            // use the multi-threaded implementation rather than the multi-range
            // one
        }
        else
        {
            bCanUseMultiThreadedRead = false;
            GTiffRasterBand *poBandForCache = this;
            if (!m_poGDS->m_bStreamingIn && m_poGDS->m_bBlockOrderRowMajor &&
                m_poGDS->m_bLeaderSizeAsUInt4 &&
                m_poGDS->m_bMaskInterleavedWithImagery &&
                m_poGDS->m_poImageryDS)
            {
                poBandForCache = cpl::down_cast<GTiffRasterBand *>(
                    m_poGDS->m_poImageryDS->GetRasterBand(1));
            }
            bufferedDataFreer.Init(poBandForCache->CacheMultiRange(
                                       nXOff, nYOff, nXSize, nYSize, nBufXSize,
                                       nBufYSize, psExtraArg),
                                   poBandForCache->m_poGDS->m_hTIFF);
        }
    }

    if (eRWFlag == GF_Read && nXSize == nBufXSize && nYSize == nBufYSize)
    {
        const int nBlockX1 = nXOff / nBlockXSize;
        const int nBlockY1 = nYOff / nBlockYSize;
        const int nBlockX2 = (nXOff + nXSize - 1) / nBlockXSize;
        const int nBlockY2 = (nYOff + nYSize - 1) / nBlockYSize;
        const int nXBlocks = nBlockX2 - nBlockX1 + 1;
        const int nYBlocks = nBlockY2 - nBlockY1 + 1;

        if (bCanUseMultiThreadedRead)
        {
            return m_poGDS->MultiThreadedRead(nXOff, nYOff, nXSize, nYSize,
                                              pData, eBufType, 1, &nBand,
                                              nPixelSpace, nLineSpace, 0);
        }
        else if (m_poGDS->nBands != 1 &&
                 m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
        {
            const GIntBig nRequiredMem =
                static_cast<GIntBig>(m_poGDS->nBands) * nXBlocks * nYBlocks *
                nBlockXSize * nBlockYSize * GDALGetDataTypeSizeBytes(eDataType);
            if (nRequiredMem > GDALGetCacheMax64())
            {
                if (!m_poGDS->m_bHasWarnedDisableAggressiveBandCaching)
                {
                    CPLDebug("GTiff",
                             "Disable aggressive band caching. "
                             "Cache not big enough. "
                             "At least " CPL_FRMT_GIB " bytes necessary",
                             nRequiredMem);
                    m_poGDS->m_bHasWarnedDisableAggressiveBandCaching = true;
                }
                m_poGDS->m_bLoadingOtherBands = true;
            }
        }
    }

    // Write optimization when writing whole blocks, by-passing the block cache.
    // We require the block cache to be non instantiated to simplify things
    // (otherwise we might need to evict corresponding existing blocks from the
    // block cache).
    else if (eRWFlag == GF_Write &&
             // Could be extended to "odd bit" case, but more work
             m_poGDS->m_nBitsPerSample == GDALGetDataTypeSize(eDataType) &&
             nXSize == nBufXSize && nYSize == nBufYSize && !HasBlockCache() &&
             !m_poGDS->m_bLoadedBlockDirty &&
             (m_poGDS->nBands == 1 ||
              m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE) &&
             (nXOff % nBlockXSize) == 0 && (nYOff % nBlockYSize) == 0 &&
             (nXOff + nXSize == nRasterXSize || (nXSize % nBlockXSize) == 0) &&
             (nYOff + nYSize == nRasterYSize || (nYSize % nBlockYSize) == 0))
    {
        m_poGDS->Crystalize();

        if (m_poGDS->m_bDebugDontWriteBlocks)
            return CE_None;

        const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
        if (nXSize == nBlockXSize && nYSize == nBlockYSize &&
            eBufType == eDataType && nPixelSpace == nDTSize &&
            nLineSpace == nPixelSpace * nBlockXSize)
        {
            // If writing one single block with the right data type and layout,
            // we don't need a temporary buffer
            const int nBlockId =
                ComputeBlockId(nXOff / nBlockXSize, nYOff / nBlockYSize);
            return m_poGDS->WriteEncodedTileOrStrip(
                nBlockId, pData, /* bPreserveDataBuffer= */ true);
        }

        // Make sure m_poGDS->m_pabyBlockBuf is allocated.
        // We could actually use any temporary buffer
        if (m_poGDS->LoadBlockBuf(/* nBlockId = */ -1,
                                  /* bReadFromDisk = */ false) != CE_None)
        {
            return CE_Failure;
        }

        // Iterate over all blocks defined by
        // [nXOff, nXOff+nXSize[ * [nYOff, nYOff+nYSize[
        // and write their content as a nBlockXSize x nBlockYSize strile
        // in a temporary buffer, before calling WriteEncodedTileOrStrip()
        // on it
        const int nYBlockStart = nYOff / nBlockYSize;
        const int nYBlockEnd = 1 + (nYOff + nYSize - 1) / nBlockYSize;
        const int nXBlockStart = nXOff / nBlockXSize;
        const int nXBlockEnd = 1 + (nXOff + nXSize - 1) / nBlockXSize;
        for (int nYBlock = nYBlockStart; nYBlock < nYBlockEnd; ++nYBlock)
        {
            const int nValidY =
                std::min(nBlockYSize, nRasterYSize - nYBlock * nBlockYSize);
            for (int nXBlock = nXBlockStart; nXBlock < nXBlockEnd; ++nXBlock)
            {
                const int nValidX =
                    std::min(nBlockXSize, nRasterXSize - nXBlock * nBlockXSize);
                if (nValidY < nBlockYSize || nValidX < nBlockXSize)
                {
                    // Make sure padding bytes at the right/bottom of the
                    // tile are initialized to zero.
                    memset(m_poGDS->m_pabyBlockBuf, 0,
                           static_cast<size_t>(nBlockXSize) * nBlockYSize *
                               nDTSize);
                }
                const GByte *pabySrcData =
                    static_cast<const GByte *>(pData) +
                    static_cast<size_t>(nYBlock - nYBlockStart) * nBlockYSize *
                        nLineSpace +
                    static_cast<size_t>(nXBlock - nXBlockStart) * nBlockXSize *
                        nPixelSpace;
                for (int iY = 0; iY < nValidY; ++iY)
                {
                    GDALCopyWords64(
                        pabySrcData + static_cast<size_t>(iY) * nLineSpace,
                        eBufType, static_cast<int>(nPixelSpace),
                        m_poGDS->m_pabyBlockBuf +
                            static_cast<size_t>(iY) * nBlockXSize * nDTSize,
                        eDataType, nDTSize, nValidX);
                }
                const int nBlockId = ComputeBlockId(nXBlock, nYBlock);
                if (m_poGDS->WriteEncodedTileOrStrip(
                        nBlockId, m_poGDS->m_pabyBlockBuf,
                        /* bPreserveDataBuffer= */ false) != CE_None)
                {
                    return CE_Failure;
                }
            }
        }
        return CE_None;
    }

    if (psExtraArg->eResampleAlg == GRIORA_NearestNeighbour)
        ++m_poGDS->m_nJPEGOverviewVisibilityCounter;
    const CPLErr eErr = GDALPamRasterBand::IRasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nPixelSpace, nLineSpace, psExtraArg);
    if (psExtraArg->eResampleAlg == GRIORA_NearestNeighbour)
        --m_poGDS->m_nJPEGOverviewVisibilityCounter;

    m_poGDS->m_bLoadingOtherBands = false;

    return eErr;
}

/************************************************************************/
/*                        ComputeBlockId()                              */
/************************************************************************/

/** Computes the TIFF block identifier from the tile coordinate, band
 * number and planar configuration.
 */
int GTiffRasterBand::ComputeBlockId(int nBlockXOff, int nBlockYOff) const
{
    const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
    {
        return nBlockId + (nBand - 1) * m_poGDS->m_nBlocksPerBand;
    }
    return nBlockId;
}
