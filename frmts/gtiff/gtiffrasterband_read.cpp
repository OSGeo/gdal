/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Read/get operations on GTiffRasterBand
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
#include "gtiffjpegoverviewds.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>

#include "cpl_vsi_virtual.h"
#include "fetchbufferdirectio.h"
#include "gtiff.h"
#include "tifvsi.h"

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *GTiffRasterBand::GetDefaultRAT()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    return GDALPamRasterBand::GetDefaultRAT();
}

/************************************************************************/
/*                           GetHistogram()                             */
/************************************************************************/

CPLErr GTiffRasterBand::GetHistogram(double dfMin, double dfMax, int nBuckets,
                                     GUIntBig *panHistogram,
                                     int bIncludeOutOfRange, int bApproxOK,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    return GDALPamRasterBand::GetHistogram(dfMin, dfMax, nBuckets, panHistogram,
                                           bIncludeOutOfRange, bApproxOK,
                                           pfnProgress, pProgressData);
}

/************************************************************************/
/*                       GetDefaultHistogram()                          */
/************************************************************************/

CPLErr GTiffRasterBand::GetDefaultHistogram(
    double *pdfMin, double *pdfMax, int *pnBuckets, GUIntBig **ppanHistogram,
    int bForce, GDALProgressFunc pfnProgress, void *pProgressData)
{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    return GDALPamRasterBand::GetDefaultHistogram(pdfMin, pdfMax, pnBuckets,
                                                  ppanHistogram, bForce,
                                                  pfnProgress, pProgressData);
}

/************************************************************************/
/*                           DirectIO()                                 */
/************************************************************************/

// Reads directly bytes from the file using ReadMultiRange(), and by-pass
// block reading. Restricted to simple TIFF configurations
// (uncompressed data, standard data types). Particularly useful to extract
// sub-windows of data on a large /vsicurl dataset).
// Returns -1 if DirectIO() can't be supported on that file.

int GTiffRasterBand::DirectIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                              int nXSize, int nYSize, void *pData,
                              int nBufXSize, int nBufYSize,
                              GDALDataType eBufType, GSpacing nPixelSpace,
                              GSpacing nLineSpace,
                              GDALRasterIOExtraArg *psExtraArg)
{
    const int nDTSizeBits = GDALGetDataTypeSizeBits(eDataType);
    if (!(eRWFlag == GF_Read && m_poGDS->m_nCompression == COMPRESSION_NONE &&
          (m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISBLACK ||
           m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB ||
           m_poGDS->m_nPhotometric == PHOTOMETRIC_PALETTE) &&
          IsBaseGTiffClass()))
    {
        return -1;
    }
    m_poGDS->Crystalize();

    // Only know how to deal with nearest neighbour in this optimized routine.
    if ((nXSize != nBufXSize || nYSize != nBufYSize) && psExtraArg != nullptr &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
    {
        return -1;
    }

#if DEBUG_VERBOSE
    CPLDebug("GTiff", "DirectIO(%d,%d,%d,%d -> %dx%d)", nXOff, nYOff, nXSize,
             nYSize, nBufXSize, nBufYSize);
#endif

    // Make sure that TIFFTAG_STRIPOFFSETS is up-to-date.
    if (m_poGDS->GetAccess() == GA_Update)
    {
        m_poGDS->FlushCache(false);
        VSI_TIFFFlushBufferedWrite(TIFFClientdata(m_poGDS->m_hTIFF));
    }

    if (TIFFIsTiled(m_poGDS->m_hTIFF))
    {
        const int nDTSize = nDTSizeBits / 8;
        const size_t nTempBufferForCommonDirectIOSize = static_cast<size_t>(
            static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize * nDTSize *
            (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG ? m_poGDS->nBands
                                                             : 1));
        if (m_poGDS->m_pTempBufferForCommonDirectIO == nullptr)
        {
            m_poGDS->m_pTempBufferForCommonDirectIO = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(nTempBufferForCommonDirectIOSize));
            if (m_poGDS->m_pTempBufferForCommonDirectIO == nullptr)
                return CE_Failure;
        }

        VSILFILE *fp = VSI_TIFFGetVSILFile(TIFFClientdata(m_poGDS->m_hTIFF));
        FetchBufferDirectIO oFetcher(fp,
                                     m_poGDS->m_pTempBufferForCommonDirectIO,
                                     nTempBufferForCommonDirectIOSize);

        return m_poGDS->CommonDirectIOClassic(
            oFetcher, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, 1, &nBand, nPixelSpace, nLineSpace, 0);
    }

    // Get strip offsets.
    toff_t *panTIFFOffsets = nullptr;
    if (!TIFFGetField(m_poGDS->m_hTIFF, TIFFTAG_STRIPOFFSETS,
                      &panTIFFOffsets) ||
        panTIFFOffsets == nullptr)
    {
        return CE_Failure;
    }

    // Sub-sampling or over-sampling can only be done at last stage.
    int nReqXSize = nXSize;
    // Can do sub-sampling at the extraction stage.
    const int nReqYSize = std::min(nBufYSize, nYSize);
    // TODO(schwehr): Make ppData be GByte**.
    void **ppData =
        static_cast<void **>(VSI_MALLOC_VERBOSE(nReqYSize * sizeof(void *)));
    vsi_l_offset *panOffsets = static_cast<vsi_l_offset *>(
        VSI_MALLOC_VERBOSE(nReqYSize * sizeof(vsi_l_offset)));
    size_t *panSizes =
        static_cast<size_t *>(VSI_MALLOC_VERBOSE(nReqYSize * sizeof(size_t)));
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    void *pTmpBuffer = nullptr;
    int eErr = CE_None;
    int nContigBands =
        m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG ? m_poGDS->nBands : 1;
    int nSrcPixelSize = nDTSize * nContigBands;

    if (ppData == nullptr || panOffsets == nullptr || panSizes == nullptr)
        eErr = CE_Failure;
    else if (nXSize != nBufXSize || nYSize != nBufYSize ||
             eBufType != eDataType ||
             nPixelSpace != GDALGetDataTypeSizeBytes(eBufType) ||
             nContigBands > 1)
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
        if (pTmpBuffer == nullptr)
            ppData[iLine] = static_cast<GByte *>(pData) + iLine * nLineSpace;
        else
            ppData[iLine] =
                static_cast<GByte *>(pTmpBuffer) +
                static_cast<size_t>(iLine) * nReqXSize * nSrcPixelSize;
        int nSrcLine = 0;
        if (nBufYSize < nYSize)  // Sub-sampling in y.
            nSrcLine = nYOff + static_cast<int>((iLine + 0.5) * dfSrcYInc);
        else
            nSrcLine = nYOff + iLine;

        const int nBlockXOff = 0;
        const int nBlockYOff = nSrcLine / nBlockYSize;
        const int nYOffsetInBlock = nSrcLine % nBlockYSize;
        const int nBlockId = ComputeBlockId(nBlockXOff, nBlockYOff);

        panOffsets[iLine] = panTIFFOffsets[nBlockId];
        if (panOffsets[iLine] == 0)  // We don't support sparse files.
            eErr = -1;

        panOffsets[iLine] +=
            (nXOff + static_cast<vsi_l_offset>(nYOffsetInBlock) * nBlockXSize) *
            nSrcPixelSize;
        panSizes[iLine] = static_cast<size_t>(nReqXSize) * nSrcPixelSize;
    }

    // Extract data from the file.
    if (eErr == CE_None)
    {
        VSILFILE *fp = VSI_TIFFGetVSILFile(TIFFClientdata(m_poGDS->m_hTIFF));
        const int nRet =
            VSIFReadMultiRangeL(nReqYSize, ppData, panOffsets, panSizes, fp);
        if (nRet != 0)
            eErr = CE_Failure;
    }

    // Byte-swap if necessary.
    if (eErr == CE_None && TIFFIsByteSwapped(m_poGDS->m_hTIFF))
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
        const bool bOneByteCopy =
            (eDataType == eBufType &&
             (eDataType == GDT_Byte || eDataType == GDT_Int8));
        for (int iY = 0; iY < nBufYSize; ++iY)
        {
            const int iSrcY = nBufYSize <= nYSize
                                  ? iY
                                  : static_cast<int>((iY + 0.5) * dfSrcYInc);

            GByte *pabySrcData = static_cast<GByte *>(ppData[iSrcY]) +
                                 (nContigBands > 1 ? (nBand - 1) : 0) * nDTSize;
            GByte *pabyDstData = static_cast<GByte *>(pData) + iY * nLineSpace;
            if (nBufXSize == nXSize)
            {
                GDALCopyWords(pabySrcData, eDataType, nSrcPixelSize,
                              pabyDstData, eBufType,
                              static_cast<int>(nPixelSpace), nBufXSize);
            }
            else
            {
                if (bOneByteCopy)
                {
                    double dfSrcX = 0.5 * dfSrcXInc;
                    for (int iX = 0; iX < nBufXSize; ++iX, dfSrcX += dfSrcXInc)
                    {
                        const int iSrcX = static_cast<int>(dfSrcX);
                        pabyDstData[iX * nPixelSpace] =
                            pabySrcData[iSrcX * nSrcPixelSize];
                    }
                }
                else
                {
                    double dfSrcX = 0.5 * dfSrcXInc;
                    for (int iX = 0; iX < nBufXSize; ++iX, dfSrcX += dfSrcXInc)
                    {
                        const int iSrcX = static_cast<int>(dfSrcX);
                        GDALCopyWords(
                            pabySrcData + iSrcX * nSrcPixelSize, eDataType, 0,
                            pabyDstData + iX * nPixelSpace, eBufType, 0, 1);
                    }
                }
            }
        }
    }

    // Cleanup.
    CPLFree(pTmpBuffer);
    CPLFree(ppData);
    CPLFree(panOffsets);
    CPLFree(panSizes);

    return eErr;
}

/************************************************************************/
/*                           GetVirtualMemAuto()                        */
/************************************************************************/

CPLVirtualMem *GTiffRasterBand::GetVirtualMemAuto(GDALRWFlag eRWFlag,
                                                  int *pnPixelSpace,
                                                  GIntBig *pnLineSpace,
                                                  char **papszOptions)
{
    const char *pszImpl = CSLFetchNameValueDef(
        papszOptions, "USE_DEFAULT_IMPLEMENTATION", "AUTO");
    if (EQUAL(pszImpl, "YES") || EQUAL(pszImpl, "ON") || EQUAL(pszImpl, "1") ||
        EQUAL(pszImpl, "TRUE"))
    {
        return GDALRasterBand::GetVirtualMemAuto(eRWFlag, pnPixelSpace,
                                                 pnLineSpace, papszOptions);
    }

    CPLVirtualMem *psRet = GetVirtualMemAutoInternal(eRWFlag, pnPixelSpace,
                                                     pnLineSpace, papszOptions);
    if (psRet != nullptr)
    {
        CPLDebug("GTiff", "GetVirtualMemAuto(): Using memory file mapping");
        return psRet;
    }

    if (EQUAL(pszImpl, "NO") || EQUAL(pszImpl, "OFF") || EQUAL(pszImpl, "0") ||
        EQUAL(pszImpl, "FALSE"))
    {
        return nullptr;
    }

    CPLDebug("GTiff", "GetVirtualMemAuto(): Defaulting to base implementation");
    return GDALRasterBand::GetVirtualMemAuto(eRWFlag, pnPixelSpace, pnLineSpace,
                                             papszOptions);
}

/************************************************************************/
/*                     DropReferenceVirtualMem()                        */
/************************************************************************/

void GTiffRasterBand::DropReferenceVirtualMem(void *pUserData)
{
    // This function may also be called when the dataset and rasterband
    // objects have been destroyed.
    // If they are still alive, it updates the reference counter of the
    // base mapping to invalidate the pointer to it if needed.

    GTiffRasterBand **ppoSelf = static_cast<GTiffRasterBand **>(pUserData);
    GTiffRasterBand *poSelf = *ppoSelf;

    if (poSelf != nullptr)
    {
        if (--(poSelf->m_poGDS->m_nRefBaseMapping) == 0)
        {
            poSelf->m_poGDS->m_pBaseMapping = nullptr;
        }
        poSelf->m_aSetPSelf.erase(ppoSelf);
    }
    CPLFree(pUserData);
}

/************************************************************************/
/*                     GetVirtualMemAutoInternal()                      */
/************************************************************************/

CPLVirtualMem *GTiffRasterBand::GetVirtualMemAutoInternal(GDALRWFlag eRWFlag,
                                                          int *pnPixelSpace,
                                                          GIntBig *pnLineSpace,
                                                          char **papszOptions)
{
    int nLineSize = nBlockXSize * GDALGetDataTypeSizeBytes(eDataType);
    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
        nLineSize *= m_poGDS->nBands;

    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
    {
        // In case of a pixel interleaved file, we save virtual memory space
        // by reusing a base mapping that embraces the whole imagery.
        if (m_poGDS->m_pBaseMapping != nullptr)
        {
            // Offset between the base mapping and the requested mapping.
            vsi_l_offset nOffset = static_cast<vsi_l_offset>(nBand - 1) *
                                   GDALGetDataTypeSizeBytes(eDataType);

            GTiffRasterBand **ppoSelf = static_cast<GTiffRasterBand **>(
                CPLCalloc(1, sizeof(GTiffRasterBand *)));
            *ppoSelf = this;

            CPLVirtualMem *pVMem = CPLVirtualMemDerivedNew(
                m_poGDS->m_pBaseMapping, nOffset,
                CPLVirtualMemGetSize(m_poGDS->m_pBaseMapping) - nOffset,
                GTiffRasterBand::DropReferenceVirtualMem, ppoSelf);
            if (pVMem == nullptr)
            {
                CPLFree(ppoSelf);
                return nullptr;
            }

            // Mechanism used so that the memory mapping object can be
            // destroyed after the raster band.
            m_aSetPSelf.insert(ppoSelf);
            ++m_poGDS->m_nRefBaseMapping;
            *pnPixelSpace = GDALGetDataTypeSizeBytes(eDataType);
            if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
                *pnPixelSpace *= m_poGDS->nBands;
            *pnLineSpace = nLineSize;
            return pVMem;
        }
    }

    VSILFILE *fp = VSI_TIFFGetVSILFile(TIFFClientdata(m_poGDS->m_hTIFF));

    vsi_l_offset nLength = static_cast<vsi_l_offset>(nRasterYSize) * nLineSize;

    if (!(CPLIsVirtualMemFileMapAvailable() &&
          VSIFGetNativeFileDescriptorL(fp) != nullptr &&
#if SIZEOF_VOIDP == 4
          nLength == static_cast<size_t>(nLength) &&
#endif
          m_poGDS->m_nCompression == COMPRESSION_NONE &&
          (m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISBLACK ||
           m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB ||
           m_poGDS->m_nPhotometric == PHOTOMETRIC_PALETTE) &&
          m_poGDS->m_nBitsPerSample == GDALGetDataTypeSizeBits(eDataType) &&
          !TIFFIsTiled(m_poGDS->m_hTIFF) &&
          !TIFFIsByteSwapped(m_poGDS->m_hTIFF)))
    {
        return nullptr;
    }

    // Make sure that TIFFTAG_STRIPOFFSETS is up-to-date.
    if (m_poGDS->GetAccess() == GA_Update)
    {
        m_poGDS->FlushCache(false);
        VSI_TIFFFlushBufferedWrite(TIFFClientdata(m_poGDS->m_hTIFF));
    }

    // Get strip offsets.
    toff_t *panTIFFOffsets = nullptr;
    if (!TIFFGetField(m_poGDS->m_hTIFF, TIFFTAG_STRIPOFFSETS,
                      &panTIFFOffsets) ||
        panTIFFOffsets == nullptr)
    {
        return nullptr;
    }

    GPtrDiff_t nBlockSize = static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize *
                            GDALGetDataTypeSizeBytes(eDataType);
    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
        nBlockSize *= m_poGDS->nBands;

    int nBlocks = m_poGDS->m_nBlocksPerBand;
    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
        nBlocks *= m_poGDS->nBands;
    int i = 0;  // Used after for.
    for (; i < nBlocks; ++i)
    {
        if (panTIFFOffsets[i] != 0)
            break;
    }
    if (i == nBlocks)
    {
        // All zeroes.
        if (m_poGDS->eAccess == GA_Update)
        {
            // Initialize the file with empty blocks so that the file has
            // the appropriate size.

            toff_t *panByteCounts = nullptr;
            if (!TIFFGetField(m_poGDS->m_hTIFF, TIFFTAG_STRIPBYTECOUNTS,
                              &panByteCounts) ||
                panByteCounts == nullptr)
            {
                return nullptr;
            }
            if (VSIFSeekL(fp, 0, SEEK_END) != 0)
                return nullptr;
            vsi_l_offset nBaseOffset = VSIFTellL(fp);

            // Just write one tile with libtiff to put it in appropriate state.
            GByte *pabyData =
                static_cast<GByte *>(VSI_CALLOC_VERBOSE(1, nBlockSize));
            if (pabyData == nullptr)
            {
                return nullptr;
            }
            const auto ret = TIFFWriteEncodedStrip(m_poGDS->m_hTIFF, 0,
                                                   pabyData, nBlockSize);
            VSI_TIFFFlushBufferedWrite(TIFFClientdata(m_poGDS->m_hTIFF));
            VSIFree(pabyData);
            if (ret != nBlockSize)
            {
                return nullptr;
            }
            CPLAssert(panTIFFOffsets[0] == nBaseOffset);
            CPLAssert(panByteCounts[0] == static_cast<toff_t>(nBlockSize));

            // Now simulate the writing of other blocks.
            const vsi_l_offset nDataSize =
                static_cast<vsi_l_offset>(nBlockSize) * nBlocks;
            if (VSIFTruncateL(fp, nBaseOffset + nDataSize) != 0)
                return nullptr;

            for (i = 1; i < nBlocks; ++i)
            {
                panTIFFOffsets[i] =
                    nBaseOffset + i * static_cast<toff_t>(nBlockSize);
                panByteCounts[i] = nBlockSize;
            }
        }
        else
        {
            CPLDebug("GTiff", "Sparse files not supported in file mapping");
            return nullptr;
        }
    }

    GIntBig nBlockSpacing = 0;
    bool bCompatibleSpacing = true;
    toff_t nPrevOffset = 0;
    for (i = 0; i < m_poGDS->m_nBlocksPerBand; ++i)
    {
        toff_t nCurOffset = 0;
        if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
            nCurOffset =
                panTIFFOffsets[m_poGDS->m_nBlocksPerBand * (nBand - 1) + i];
        else
            nCurOffset = panTIFFOffsets[i];
        if (nCurOffset == 0)
        {
            bCompatibleSpacing = false;
            break;
        }
        if (i > 0)
        {
            const GIntBig nCurSpacing = nCurOffset - nPrevOffset;
            if (i == 1)
            {
                if (nCurSpacing !=
                    static_cast<GIntBig>(nBlockYSize) * nLineSize)
                {
                    bCompatibleSpacing = false;
                    break;
                }
                nBlockSpacing = nCurSpacing;
            }
            else if (nBlockSpacing != nCurSpacing)
            {
                bCompatibleSpacing = false;
                break;
            }
        }
        nPrevOffset = nCurOffset;
    }

    if (!bCompatibleSpacing)
    {
        return nullptr;
    }

    vsi_l_offset nOffset = 0;
    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
    {
        CPLAssert(m_poGDS->m_pBaseMapping == nullptr);
        nOffset = panTIFFOffsets[0];
    }
    else
    {
        nOffset = panTIFFOffsets[m_poGDS->m_nBlocksPerBand * (nBand - 1)];
    }
    CPLVirtualMem *pVMem = CPLVirtualMemFileMapNew(
        fp, nOffset, nLength,
        eRWFlag == GF_Write ? VIRTUALMEM_READWRITE : VIRTUALMEM_READONLY,
        nullptr, nullptr);
    if (pVMem == nullptr)
    {
        return nullptr;
    }

    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
    {
        // TODO(schwehr): Revisit this block.
        m_poGDS->m_pBaseMapping = pVMem;
        pVMem = GetVirtualMemAutoInternal(eRWFlag, pnPixelSpace, pnLineSpace,
                                          papszOptions);
        // Drop ref on base mapping.
        CPLVirtualMemFree(m_poGDS->m_pBaseMapping);
        if (pVMem == nullptr)
            m_poGDS->m_pBaseMapping = nullptr;
    }
    else
    {
        *pnPixelSpace = GDALGetDataTypeSizeBytes(eDataType);
        if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
            *pnPixelSpace *= m_poGDS->nBands;
        *pnLineSpace = nLineSize;
    }
    return pVMem;
}

/************************************************************************/
/*                         CacheMultiRange()                            */
/************************************************************************/

static bool CheckTrailer(const GByte *strileData, vsi_l_offset nStrileSize)
{
    GByte abyTrailer[4];
    memcpy(abyTrailer, strileData + nStrileSize, 4);
    GByte abyLastBytes[4] = {};
    if (nStrileSize >= 4)
        memcpy(abyLastBytes, strileData + nStrileSize - 4, 4);
    else
    {
        // The last bytes will be zero due to the above {} initialization,
        // and that's what should be in abyTrailer too when the trailer is
        // correct.
        memcpy(abyLastBytes, strileData, static_cast<size_t>(nStrileSize));
    }
    return memcmp(abyTrailer, abyLastBytes, 4) == 0;
}

void *GTiffRasterBand::CacheMultiRange(int nXOff, int nYOff, int nXSize,
                                       int nYSize, int nBufXSize, int nBufYSize,
                                       GDALRasterIOExtraArg *psExtraArg)
{
    void *pBufferedData = nullptr;
    // Same logic as in GDALRasterBand::IRasterIO()
    double dfXOff = nXOff;
    double dfYOff = nYOff;
    double dfXSize = nXSize;
    double dfYSize = nYSize;
    if (psExtraArg->bFloatingPointWindowValidity)
    {
        dfXOff = psExtraArg->dfXOff;
        dfYOff = psExtraArg->dfYOff;
        dfXSize = psExtraArg->dfXSize;
        dfYSize = psExtraArg->dfYSize;
    }
    const double dfSrcXInc = dfXSize / static_cast<double>(nBufXSize);
    const double dfSrcYInc = dfYSize / static_cast<double>(nBufYSize);
    const double EPS = 1e-10;
    const int nBlockX1 =
        static_cast<int>(std::max(0.0, (0 + 0.5) * dfSrcXInc + dfXOff + EPS)) /
        nBlockXSize;
    const int nBlockY1 =
        static_cast<int>(std::max(0.0, (0 + 0.5) * dfSrcYInc + dfYOff + EPS)) /
        nBlockYSize;
    const int nBlockX2 =
        static_cast<int>(
            std::min(static_cast<double>(nRasterXSize - 1),
                     (nBufXSize - 1 + 0.5) * dfSrcXInc + dfXOff + EPS)) /
        nBlockXSize;
    const int nBlockY2 =
        static_cast<int>(
            std::min(static_cast<double>(nRasterYSize - 1),
                     (nBufYSize - 1 + 0.5) * dfSrcYInc + dfYOff + EPS)) /
        nBlockYSize;

    const int nBlockCount = nBlocksPerRow * nBlocksPerColumn;

    struct StrileData
    {
        vsi_l_offset nOffset;
        vsi_l_offset nByteCount;
        bool bTryMask;
    };

    std::map<int, StrileData> oMapStrileToOffsetByteCount;

    // Dedicated method to retrieved the offset and size in an efficient way
    // when m_bBlockOrderRowMajor and m_bLeaderSizeAsUInt4 conditions are
    // met.
    // Except for the last block, we just read the offset from the TIFF offset
    // array, and retrieve the size in the leader 4 bytes that come before the
    // payload.
    auto OptimizedRetrievalOfOffsetSize =
        [&](int nBlockId, vsi_l_offset &nOffset, vsi_l_offset &nSize,
            size_t nTotalSize, size_t nMaxRawBlockCacheSize)
    {
        bool bTryMask = m_poGDS->m_bMaskInterleavedWithImagery;
        nOffset = TIFFGetStrileOffset(m_poGDS->m_hTIFF, nBlockId);
        if (nOffset >= 4)
        {
            if (nBlockId == nBlockCount - 1)
            {
                // Special case for the last block. As there is no next block
                // from which to retrieve an offset, use the good old method
                // that consists in reading the ByteCount array.
                if (bTryMask && m_poGDS->GetRasterBand(1)->GetMaskBand() &&
                    m_poGDS->m_poMaskDS)
                {
                    auto nMaskOffset = TIFFGetStrileOffset(
                        m_poGDS->m_poMaskDS->m_hTIFF, nBlockId);
                    if (nMaskOffset)
                    {
                        nSize = nMaskOffset +
                                TIFFGetStrileByteCount(
                                    m_poGDS->m_poMaskDS->m_hTIFF, nBlockId) -
                                nOffset;
                    }
                    else
                    {
                        bTryMask = false;
                    }
                }
                if (nSize == 0)
                {
                    nSize = TIFFGetStrileByteCount(m_poGDS->m_hTIFF, nBlockId);
                }
                if (nSize && m_poGDS->m_bTrailerRepeatedLast4BytesRepeated)
                {
                    nSize += 4;
                }
            }
            else
            {
                auto nOffsetNext =
                    TIFFGetStrileOffset(m_poGDS->m_hTIFF, nBlockId + 1);
                if (nOffsetNext > nOffset)
                {
                    nSize = nOffsetNext - nOffset;
                }
                else
                {
                    // Shouldn't happen for a compliant file
                    if (nOffsetNext != 0)
                    {
                        CPLDebug("GTiff", "Tile %d is not located after %d",
                                 nBlockId + 1, nBlockId);
                    }
                    bTryMask = false;
                    nSize = TIFFGetStrileByteCount(m_poGDS->m_hTIFF, nBlockId);
                    if (m_poGDS->m_bTrailerRepeatedLast4BytesRepeated)
                        nSize += 4;
                }
            }
            if (nSize)
            {
                nOffset -= 4;
                nSize += 4;
                if (nTotalSize + nSize < nMaxRawBlockCacheSize)
                {
                    StrileData data;
                    data.nOffset = nOffset;
                    data.nByteCount = nSize;
                    data.bTryMask = bTryMask;
                    oMapStrileToOffsetByteCount[nBlockId] = data;
                }
            }
        }
        else
        {
            // Sparse tile
            StrileData data;
            data.nOffset = 0;
            data.nByteCount = 0;
            data.bTryMask = false;
            oMapStrileToOffsetByteCount[nBlockId] = data;
        }
    };

    // This lambda fills m_poDS->m_oCacheStrileToOffsetByteCount (and
    // m_poDS->m_poMaskDS->m_oCacheStrileToOffsetByteCount, when there is a
    // mask) from the temporary oMapStrileToOffsetByteCount.
    auto FillCacheStrileToOffsetByteCount =
        [&](const std::vector<vsi_l_offset> &anOffsets,
            const std::vector<size_t> &anSizes,
            const std::vector<void *> &apData)
    {
        CPLAssert(m_poGDS->m_bLeaderSizeAsUInt4);
        size_t i = 0;
        vsi_l_offset nLastOffset = 0;
        for (const auto &entry : oMapStrileToOffsetByteCount)
        {
            const auto nBlockId = entry.first;
            const auto nOffset = entry.second.nOffset;
            const auto nSize = entry.second.nByteCount;
            if (nOffset == 0)
            {
                // Sparse tile
                m_poGDS->m_oCacheStrileToOffsetByteCount.insert(
                    nBlockId, std::pair(0, 0));
                continue;
            }

            if (nOffset < nLastOffset)
            {
                // shouldn't happen normally if tiles are sorted
                i = 0;
            }
            nLastOffset = nOffset;
            while (i < anOffsets.size() &&
                   !(nOffset >= anOffsets[i] &&
                     nOffset + nSize <= anOffsets[i] + anSizes[i]))
            {
                i++;
            }
            CPLAssert(i < anOffsets.size());
            CPLAssert(nOffset >= anOffsets[i]);
            CPLAssert(nOffset + nSize <= anOffsets[i] + anSizes[i]);
            GUInt32 nSizeFromLeader;
            memcpy(&nSizeFromLeader,
                   // cppcheck-suppress containerOutOfBounds
                   static_cast<GByte *>(apData[i]) + nOffset - anOffsets[i],
                   sizeof(nSizeFromLeader));
            CPL_LSBPTR32(&nSizeFromLeader);
            bool bOK = true;
            constexpr int nLeaderSize = 4;
            const int nTrailerSize =
                (m_poGDS->m_bTrailerRepeatedLast4BytesRepeated ? 4 : 0);
            if (nSizeFromLeader > nSize - nLeaderSize - nTrailerSize)
            {
                CPLDebug("GTiff",
                         "Inconsistent block size from in leader of block %d",
                         nBlockId);
                bOK = false;
            }
            else if (m_poGDS->m_bTrailerRepeatedLast4BytesRepeated)
            {
                // Check trailer consistency
                const GByte *strileData = static_cast<GByte *>(apData[i]) +
                                          nOffset - anOffsets[i] + nLeaderSize;
                if (!CheckTrailer(strileData, nSizeFromLeader))
                {
                    CPLDebug("GTiff", "Inconsistent trailer of block %d",
                             nBlockId);
                    bOK = false;
                }
            }
            if (!bOK)
            {
                return false;
            }

            {
                const vsi_l_offset nRealOffset = nOffset + nLeaderSize;
                const vsi_l_offset nRealSize = nSizeFromLeader;
#ifdef DEBUG_VERBOSE
                CPLDebug("GTiff",
                         "Block %d found at offset " CPL_FRMT_GUIB
                         " with size " CPL_FRMT_GUIB,
                         nBlockId, nRealOffset, nRealSize);
#endif
                m_poGDS->m_oCacheStrileToOffsetByteCount.insert(
                    nBlockId, std::pair(nRealOffset, nRealSize));
            }

            // Processing of mask
            if (!(entry.second.bTryMask &&
                  m_poGDS->m_bMaskInterleavedWithImagery &&
                  m_poGDS->GetRasterBand(1)->GetMaskBand() &&
                  m_poGDS->m_poMaskDS))
            {
                continue;
            }

            bOK = false;
            const vsi_l_offset nMaskOffsetWithLeader =
                nOffset + nLeaderSize + nSizeFromLeader + nTrailerSize;
            if (nMaskOffsetWithLeader + nLeaderSize <=
                anOffsets[i] + anSizes[i])
            {
                GUInt32 nMaskSizeFromLeader;
                memcpy(&nMaskSizeFromLeader,
                       static_cast<GByte *>(apData[i]) + nMaskOffsetWithLeader -
                           anOffsets[i],
                       sizeof(nMaskSizeFromLeader));
                CPL_LSBPTR32(&nMaskSizeFromLeader);
                if (nMaskOffsetWithLeader + nLeaderSize + nMaskSizeFromLeader +
                        nTrailerSize <=
                    anOffsets[i] + anSizes[i])
                {
                    bOK = true;
                    if (m_poGDS->m_bTrailerRepeatedLast4BytesRepeated)
                    {
                        // Check trailer consistency
                        const GByte *strileMaskData =
                            static_cast<GByte *>(apData[i]) + nOffset -
                            anOffsets[i] + nLeaderSize + nSizeFromLeader +
                            nTrailerSize + nLeaderSize;
                        if (!CheckTrailer(strileMaskData, nMaskSizeFromLeader))
                        {
                            CPLDebug("GTiff",
                                     "Inconsistent trailer of mask of block %d",
                                     nBlockId);
                            bOK = false;
                        }
                    }
                }
                if (bOK)
                {
                    const vsi_l_offset nRealOffset = nOffset + nLeaderSize +
                                                     nSizeFromLeader +
                                                     nTrailerSize + nLeaderSize;
                    const vsi_l_offset nRealSize = nMaskSizeFromLeader;
#ifdef DEBUG_VERBOSE
                    CPLDebug("GTiff",
                             "Mask of block %d found at offset " CPL_FRMT_GUIB
                             " with size " CPL_FRMT_GUIB,
                             nBlockId, nRealOffset, nRealSize);
#endif

                    m_poGDS->m_poMaskDS->m_oCacheStrileToOffsetByteCount.insert(
                        nBlockId, std::pair(nRealOffset, nRealSize));
                }
            }
            if (!bOK)
            {
                CPLDebug("GTiff",
                         "Mask for block %d is not properly interleaved with "
                         "imagery block",
                         nBlockId);
            }
        }
        return true;
    };

    thandle_t th = TIFFClientdata(m_poGDS->m_hTIFF);
    if (!VSI_TIFFHasCachedRanges(th))
    {
        std::vector<std::pair<vsi_l_offset, size_t>> aOffsetSize;
        size_t nTotalSize = 0;
        const unsigned int nMaxRawBlockCacheSize = atoi(
            CPLGetConfigOption("GDAL_MAX_RAW_BLOCK_CACHE_SIZE", "10485760"));
        bool bGoOn = true;
        for (int iY = nBlockY1; bGoOn && iY <= nBlockY2; iY++)
        {
            for (int iX = nBlockX1; bGoOn && iX <= nBlockX2; iX++)
            {
                GDALRasterBlock *poBlock = TryGetLockedBlockRef(iX, iY);
                if (poBlock != nullptr)
                {
                    poBlock->DropLock();
                    continue;
                }
                int nBlockId = iX + iY * nBlocksPerRow;
                if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
                    nBlockId += (nBand - 1) * m_poGDS->m_nBlocksPerBand;
                vsi_l_offset nOffset = 0;
                vsi_l_offset nSize = 0;

                if ((m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG ||
                     m_poGDS->nBands == 1) &&
                    !m_poGDS->m_bStreamingIn &&
                    m_poGDS->m_bBlockOrderRowMajor &&
                    m_poGDS->m_bLeaderSizeAsUInt4)
                {
                    OptimizedRetrievalOfOffsetSize(nBlockId, nOffset, nSize,
                                                   nTotalSize,
                                                   nMaxRawBlockCacheSize);
                }
                else
                {
                    CPL_IGNORE_RET_VAL(
                        m_poGDS->IsBlockAvailable(nBlockId, &nOffset, &nSize));
                }
                if (nSize)
                {
                    if (nTotalSize + nSize < nMaxRawBlockCacheSize)
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("GTiff",
                                 "Precaching for block (%d, %d), " CPL_FRMT_GUIB
                                 "-" CPL_FRMT_GUIB,
                                 iX, iY, nOffset,
                                 nOffset + static_cast<size_t>(nSize) - 1);
#endif
                        aOffsetSize.push_back(
                            std::pair(nOffset, static_cast<size_t>(nSize)));
                        nTotalSize += static_cast<size_t>(nSize);
                    }
                    else
                    {
                        bGoOn = false;
                    }
                }
            }
        }

        std::sort(aOffsetSize.begin(), aOffsetSize.end());

        if (nTotalSize > 0)
        {
            pBufferedData = VSI_MALLOC_VERBOSE(nTotalSize);
            if (pBufferedData)
            {
                std::vector<vsi_l_offset> anOffsets;
                std::vector<size_t> anSizes;
                std::vector<void *> apData;
                anOffsets.push_back(aOffsetSize[0].first);
                apData.push_back(static_cast<GByte *>(pBufferedData));
                size_t nChunkSize = aOffsetSize[0].second;
                size_t nAccOffset = 0;
                // Try to merge contiguous or slightly overlapping ranges
                for (size_t i = 0; i < aOffsetSize.size() - 1; i++)
                {
                    if (aOffsetSize[i].first < aOffsetSize[i + 1].first &&
                        aOffsetSize[i].first + aOffsetSize[i].second >=
                            aOffsetSize[i + 1].first)
                    {
                        const auto overlap = aOffsetSize[i].first +
                                             aOffsetSize[i].second -
                                             aOffsetSize[i + 1].first;
                        // That should always be the case for well behaved
                        // TIFF files.
                        if (aOffsetSize[i + 1].second > overlap)
                        {
                            nChunkSize += static_cast<size_t>(
                                aOffsetSize[i + 1].second - overlap);
                        }
                    }
                    else
                    {
                        // terminate current block
                        anSizes.push_back(nChunkSize);
#ifdef DEBUG_VERBOSE
                        CPLDebug("GTiff",
                                 "Requesting range [" CPL_FRMT_GUIB
                                 "-" CPL_FRMT_GUIB "]",
                                 anOffsets.back(),
                                 anOffsets.back() + anSizes.back() - 1);
#endif
                        nAccOffset += nChunkSize;
                        // start a new range
                        anOffsets.push_back(aOffsetSize[i + 1].first);
                        apData.push_back(static_cast<GByte *>(pBufferedData) +
                                         nAccOffset);
                        nChunkSize = aOffsetSize[i + 1].second;
                    }
                }
                // terminate last block
                anSizes.push_back(nChunkSize);
#ifdef DEBUG_VERBOSE
                CPLDebug(
                    "GTiff",
                    "Requesting range [" CPL_FRMT_GUIB "-" CPL_FRMT_GUIB "]",
                    anOffsets.back(), anOffsets.back() + anSizes.back() - 1);
#endif

                VSILFILE *fp = VSI_TIFFGetVSILFile(th);

                if (VSIFReadMultiRangeL(static_cast<int>(anSizes.size()),
                                        &apData[0], &anOffsets[0], &anSizes[0],
                                        fp) == 0)
                {
                    if (!oMapStrileToOffsetByteCount.empty() &&
                        !FillCacheStrileToOffsetByteCount(anOffsets, anSizes,
                                                          apData))
                    {
                        // Retry without optimization
                        CPLFree(pBufferedData);
                        m_poGDS->m_bLeaderSizeAsUInt4 = false;
                        void *pRet =
                            CacheMultiRange(nXOff, nYOff, nXSize, nYSize,
                                            nBufXSize, nBufYSize, psExtraArg);
                        m_poGDS->m_bLeaderSizeAsUInt4 = true;
                        return pRet;
                    }

                    VSI_TIFFSetCachedRanges(
                        th, static_cast<int>(anSizes.size()), &apData[0],
                        &anOffsets[0], &anSizes[0]);
                }
            }
        }
    }
    return pBufferedData;
}

/************************************************************************/
/*                       IGetDataCoverageStatus()                       */
/************************************************************************/

int GTiffRasterBand::IGetDataCoverageStatus(int nXOff, int nYOff, int nXSize,
                                            int nYSize, int nMaskFlagStop,
                                            double *pdfDataPct)
{
    if (eAccess == GA_Update)
        m_poGDS->FlushCache(false);

    const int iXBlockStart = nXOff / nBlockXSize;
    const int iXBlockEnd = (nXOff + nXSize - 1) / nBlockXSize;
    const int iYBlockStart = nYOff / nBlockYSize;
    const int iYBlockEnd = (nYOff + nYSize - 1) / nBlockYSize;
    int nStatus = 0;
    VSILFILE *fp = VSI_TIFFGetVSILFile(TIFFClientdata(m_poGDS->m_hTIFF));
    GIntBig nPixelsData = 0;
    for (int iY = iYBlockStart; iY <= iYBlockEnd; ++iY)
    {
        for (int iX = iXBlockStart; iX <= iXBlockEnd; ++iX)
        {
            const int nBlockIdBand0 = iX + iY * nBlocksPerRow;
            int nBlockId = nBlockIdBand0;
            if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
                nBlockId =
                    nBlockIdBand0 + (nBand - 1) * m_poGDS->m_nBlocksPerBand;
            vsi_l_offset nOffset = 0;
            vsi_l_offset nLength = 0;
            bool bHasData = false;
            if (!m_poGDS->IsBlockAvailable(nBlockId, &nOffset, &nLength))
            {
                nStatus |= GDAL_DATA_COVERAGE_STATUS_EMPTY;
            }
            else
            {
                if (m_poGDS->m_nCompression == COMPRESSION_NONE &&
                    m_poGDS->eAccess == GA_ReadOnly &&
                    ((!m_bNoDataSet && !m_bNoDataSetAsInt64 &&
                      !m_bNoDataSetAsUInt64) ||
                     (m_bNoDataSet && m_dfNoDataValue == 0.0) ||
                     (m_bNoDataSetAsInt64 && m_nNoDataValueInt64 == 0) ||
                     (m_bNoDataSetAsUInt64 && m_nNoDataValueUInt64 == 0)))
                {
                    VSIRangeStatus eStatus =
                        VSIFGetRangeStatusL(fp, nOffset, nLength);
                    if (eStatus == VSI_RANGE_STATUS_HOLE)
                    {
                        nStatus |= GDAL_DATA_COVERAGE_STATUS_EMPTY;
                    }
                    else
                    {
                        bHasData = true;
                    }
                }
                else
                {
                    bHasData = true;
                }
            }
            if (bHasData)
            {
                const int nXBlockRight =
                    (iX * nBlockXSize > INT_MAX - nBlockXSize)
                        ? INT_MAX
                        : (iX + 1) * nBlockXSize;
                const int nYBlockBottom =
                    (iY * nBlockYSize > INT_MAX - nBlockYSize)
                        ? INT_MAX
                        : (iY + 1) * nBlockYSize;

                nPixelsData += (static_cast<GIntBig>(
                                    std::min(nXBlockRight, nXOff + nXSize)) -
                                std::max(iX * nBlockXSize, nXOff)) *
                               (std::min(nYBlockBottom, nYOff + nYSize) -
                                std::max(iY * nBlockYSize, nYOff));
                nStatus |= GDAL_DATA_COVERAGE_STATUS_DATA;
            }
            if (nMaskFlagStop != 0 && (nMaskFlagStop & nStatus) != 0)
            {
                if (pdfDataPct)
                    *pdfDataPct = -1.0;
                return nStatus;
            }
        }
    }
    if (pdfDataPct)
        *pdfDataPct =
            100.0 * nPixelsData / (static_cast<GIntBig>(nXSize) * nYSize);
    return nStatus;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    m_poGDS->Crystalize();

    GPtrDiff_t nBlockBufSize = 0;
    if (TIFFIsTiled(m_poGDS->m_hTIFF))
    {
        nBlockBufSize = static_cast<GPtrDiff_t>(TIFFTileSize(m_poGDS->m_hTIFF));
    }
    else
    {
        CPLAssert(nBlockXOff == 0);
        nBlockBufSize =
            static_cast<GPtrDiff_t>(TIFFStripSize(m_poGDS->m_hTIFF));
    }

    const int nBlockId = ComputeBlockId(nBlockXOff, nBlockYOff);

    /* -------------------------------------------------------------------- */
    /*      The bottom most partial tiles and strips are sometimes only     */
    /*      partially encoded.  This code reduces the requested data so     */
    /*      an error won't be reported in this case. (#1179)                */
    /* -------------------------------------------------------------------- */
    auto nBlockReqSize = nBlockBufSize;

    if (nBlockYOff * nBlockYSize > nRasterYSize - nBlockYSize)
    {
        nBlockReqSize =
            (nBlockBufSize / nBlockYSize) *
            (nBlockYSize -
             static_cast<int>(
                 (static_cast<GIntBig>(nBlockYOff + 1) * nBlockYSize) %
                 nRasterYSize));
    }

    /* -------------------------------------------------------------------- */
    /*      Handle the case of a strip or tile that doesn't exist yet.      */
    /*      Just set to zeros and return.                                   */
    /* -------------------------------------------------------------------- */
    vsi_l_offset nOffset = 0;
    bool bErrOccurred = false;
    if (nBlockId != m_poGDS->m_nLoadedBlock &&
        !m_poGDS->IsBlockAvailable(nBlockId, &nOffset, nullptr, &bErrOccurred))
    {
        NullBlock(pImage);
        if (bErrOccurred)
            return CE_Failure;
        return CE_None;
    }

    if (m_poGDS->m_bStreamingIn &&
        !(m_poGDS->nBands > 1 &&
          m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
          nBlockId == m_poGDS->m_nLoadedBlock))
    {
        if (nOffset < VSIFTellL(m_poGDS->m_fpL))
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Trying to load block %d at offset " CPL_FRMT_GUIB
                        " whereas current pos is " CPL_FRMT_GUIB
                        " (backward read not supported)",
                        nBlockId, static_cast<GUIntBig>(nOffset),
                        static_cast<GUIntBig>(VSIFTellL(m_poGDS->m_fpL)));
            return CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Handle simple case (separate, onesampleperpixel)                */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    if (m_poGDS->nBands == 1 ||
        m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
    {
        if (nBlockReqSize < nBlockBufSize)
            memset(pImage, 0, nBlockBufSize);

        if (!m_poGDS->ReadStrile(nBlockId, pImage, nBlockReqSize))
        {
            memset(pImage, 0, nBlockBufSize);
            return CE_Failure;
        }
    }
    else
    {
        /* --------------------------------------------------------------------
         */
        /*      Load desired block */
        /* --------------------------------------------------------------------
         */
        eErr = m_poGDS->LoadBlockBuf(nBlockId);
        if (eErr != CE_None)
        {
            memset(pImage, 0,
                   static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize *
                       GDALGetDataTypeSizeBytes(eDataType));
            return eErr;
        }

        bool bDoCopyWords = true;
        if (nBand == 1 && !m_poGDS->m_bLoadingOtherBands &&
            eAccess == GA_ReadOnly &&
            (m_poGDS->nBands == 3 || m_poGDS->nBands == 4) &&
            ((eDataType == GDT_Byte && m_poGDS->m_nBitsPerSample == 8) ||
             (eDataType == GDT_Int16 && m_poGDS->m_nBitsPerSample == 16) ||
             (eDataType == GDT_UInt16 && m_poGDS->m_nBitsPerSample == 16)) &&
            static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize *
                    GDALGetDataTypeSizeBytes(eDataType) <
                GDALGetCacheMax64() / m_poGDS->nBands)
        {
            bDoCopyWords = false;
            void *ppDestBuffers[4];
            GDALRasterBlock *apoLockedBlocks[4] = {nullptr, nullptr, nullptr,
                                                   nullptr};
            for (int iBand = 1; iBand <= m_poGDS->nBands; ++iBand)
            {
                if (iBand == nBand)
                {
                    ppDestBuffers[iBand - 1] = pImage;
                }
                else
                {
                    GDALRasterBlock *poBlock =
                        m_poGDS->GetRasterBand(iBand)->GetLockedBlockRef(
                            nBlockXOff, nBlockYOff, true);
                    if (poBlock == nullptr)
                    {
                        bDoCopyWords = true;
                        break;
                    }
                    ppDestBuffers[iBand - 1] = poBlock->GetDataRef();
                    apoLockedBlocks[iBand - 1] = poBlock;
                }
            }
            if (!bDoCopyWords)
            {
                GDALDeinterleave(m_poGDS->m_pabyBlockBuf, eDataType,
                                 m_poGDS->nBands, ppDestBuffers, eDataType,
                                 static_cast<size_t>(nBlockXSize) *
                                     nBlockYSize);
            }
            for (int iBand = 1; iBand <= m_poGDS->nBands; ++iBand)
            {
                if (apoLockedBlocks[iBand - 1])
                {
                    apoLockedBlocks[iBand - 1]->DropLock();
                }
            }
        }

        if (bDoCopyWords)
        {
            const int nWordBytes = m_poGDS->m_nBitsPerSample / 8;
            GByte *pabyImage =
                m_poGDS->m_pabyBlockBuf + (nBand - 1) * nWordBytes;

            GDALCopyWords64(pabyImage, eDataType, m_poGDS->nBands * nWordBytes,
                            pImage, eDataType, nWordBytes,
                            static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize);

            eErr = FillCacheForOtherBands(nBlockXOff, nBlockYOff);
        }
    }

    CacheMaskForBlock(nBlockXOff, nBlockYOff);

    return eErr;
}

/************************************************************************/
/*                           CacheMaskForBlock()                       */
/************************************************************************/

void GTiffRasterBand::CacheMaskForBlock(int nBlockXOff, int nBlockYOff)

{
    // Preload mask data if layout compatible and we have cached ranges
    if (m_poGDS->m_bMaskInterleavedWithImagery && m_poGDS->m_poMaskDS &&
        VSI_TIFFHasCachedRanges(TIFFClientdata(m_poGDS->m_hTIFF)))
    {
        auto poBand = cpl::down_cast<GTiffRasterBand *>(
            m_poGDS->m_poMaskDS->GetRasterBand(1));
        if (m_poGDS->m_poMaskDS->m_oCacheStrileToOffsetByteCount.contains(
                poBand->ComputeBlockId(nBlockXOff, nBlockYOff)))
        {
            GDALRasterBlock *poBlock =
                poBand->GetLockedBlockRef(nBlockXOff, nBlockYOff);
            if (poBlock)
                poBlock->DropLock();
        }
    }
}

/************************************************************************/
/*                       FillCacheForOtherBands()                       */
/************************************************************************/

CPLErr GTiffRasterBand::FillCacheForOtherBands(int nBlockXOff, int nBlockYOff)

{
    /* -------------------------------------------------------------------- */
    /*      In the fairly common case of pixel interleaved 8bit data        */
    /*      that is multi-band, lets push the rest of the data into the     */
    /*      block cache too, to avoid (hopefully) having to redecode it.    */
    /*                                                                      */
    /*      Our following logic actually depends on the fact that the       */
    /*      this block is already loaded, so subsequent calls will end      */
    /*      up back in this method and pull from the loaded block.          */
    /*                                                                      */
    /*      Be careful not entering this portion of code from               */
    /*      the other bands, otherwise we'll get very deep nested calls     */
    /*      and O(nBands^2) performance !                                   */
    /*                                                                      */
    /*      If there are many bands and the block cache size is not big     */
    /*      enough to accommodate the size of all the blocks, don't enter   */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    if (m_poGDS->nBands != 1 &&
        m_poGDS->nBands <
            128 &&  // avoid caching for datasets with too many bands
        !m_poGDS->m_bLoadingOtherBands &&
        static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize *
                GDALGetDataTypeSizeBytes(eDataType) <
            GDALGetCacheMax64() / m_poGDS->nBands)
    {
        m_poGDS->m_bLoadingOtherBands = true;

        for (int iOtherBand = 1; iOtherBand <= m_poGDS->nBands; ++iOtherBand)
        {
            if (iOtherBand == nBand)
                continue;

            GDALRasterBlock *poBlock =
                m_poGDS->GetRasterBand(iOtherBand)
                    ->GetLockedBlockRef(nBlockXOff, nBlockYOff);
            if (poBlock == nullptr)
            {
                eErr = CE_Failure;
                break;
            }
            poBlock->DropLock();
        }

        m_poGDS->m_bLoadingOtherBands = false;
    }

    return eErr;
}

/************************************************************************/
/*                           GetDescription()                           */
/************************************************************************/

const char *GTiffRasterBand::GetDescription() const
{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    return m_osDescription;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GTiffRasterBand::GetOffset(int *pbSuccess)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (pbSuccess)
        *pbSuccess = m_bHaveOffsetScale;
    return m_dfOffset;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GTiffRasterBand::GetScale(int *pbSuccess)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (pbSuccess)
        *pbSuccess = m_bHaveOffsetScale;
    return m_dfScale;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *GTiffRasterBand::GetUnitType()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    if (m_osUnitType.empty())
    {
        m_poGDS->LookForProjection();
        if (m_poGDS->m_pszVertUnit)
            return m_poGDS->m_pszVertUnit;
    }

    return m_osUnitType.c_str();
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GTiffRasterBand::GetMetadataDomainList()
{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    return CSLDuplicate(m_oGTiffMDMD.GetDomainList());
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GTiffRasterBand::GetMetadata(const char *pszDomain)

{
    if (pszDomain == nullptr || !EQUAL(pszDomain, "IMAGE_STRUCTURE"))
    {
        m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    }

    return m_oGTiffMDMD.GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GTiffRasterBand::GetMetadataItem(const char *pszName,
                                             const char *pszDomain)

{
    if (pszDomain == nullptr || !EQUAL(pszDomain, "IMAGE_STRUCTURE"))
    {
        m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    }

    if (pszName != nullptr && pszDomain != nullptr && EQUAL(pszDomain, "TIFF"))
    {
        int nBlockXOff = 0;
        int nBlockYOff = 0;

        if (EQUAL(pszName, "JPEGTABLES"))
        {
            uint32_t nJPEGTableSize = 0;
            void *pJPEGTable = nullptr;
            if (TIFFGetField(m_poGDS->m_hTIFF, TIFFTAG_JPEGTABLES,
                             &nJPEGTableSize, &pJPEGTable) != 1 ||
                pJPEGTable == nullptr || nJPEGTableSize > INT_MAX)
            {
                return nullptr;
            }
            char *const pszHex = CPLBinaryToHex(
                nJPEGTableSize, static_cast<const GByte *>(pJPEGTable));
            const char *pszReturn = CPLSPrintf("%s", pszHex);
            CPLFree(pszHex);

            return pszReturn;
        }

        if (EQUAL(pszName, "IFD_OFFSET"))
        {
            return CPLSPrintf(CPL_FRMT_GUIB,
                              static_cast<GUIntBig>(m_poGDS->m_nDirOffset));
        }

        if (sscanf(pszName, "BLOCK_OFFSET_%d_%d", &nBlockXOff, &nBlockYOff) ==
            2)
        {
            if (nBlockXOff < 0 || nBlockXOff >= nBlocksPerRow ||
                nBlockYOff < 0 || nBlockYOff >= nBlocksPerColumn)
                return nullptr;

            int nBlockId = nBlockYOff * nBlocksPerRow + nBlockXOff;
            if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
            {
                nBlockId += (nBand - 1) * m_poGDS->m_nBlocksPerBand;
            }

            vsi_l_offset nOffset = 0;
            if (!m_poGDS->IsBlockAvailable(nBlockId, &nOffset))
            {
                return nullptr;
            }

            return CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nOffset));
        }

        if (sscanf(pszName, "BLOCK_SIZE_%d_%d", &nBlockXOff, &nBlockYOff) == 2)
        {
            if (nBlockXOff < 0 || nBlockXOff >= nBlocksPerRow ||
                nBlockYOff < 0 || nBlockYOff >= nBlocksPerColumn)
                return nullptr;

            int nBlockId = nBlockYOff * nBlocksPerRow + nBlockXOff;
            if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
            {
                nBlockId += (nBand - 1) * m_poGDS->m_nBlocksPerBand;
            }

            vsi_l_offset nByteCount = 0;
            if (!m_poGDS->IsBlockAvailable(nBlockId, nullptr, &nByteCount))
            {
                return nullptr;
            }

            return CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nByteCount));
        }
    }
    else if (pszName && pszDomain && EQUAL(pszDomain, "_DEBUG_"))
    {
        if (EQUAL(pszName, "HAS_BLOCK_CACHE"))
            return HasBlockCache() ? "1" : "0";
    }

    const char *pszRet = m_oGTiffMDMD.GetMetadataItem(pszName, pszDomain);

    if (pszRet == nullptr && eDataType == GDT_Byte && pszName && pszDomain &&
        EQUAL(pszDomain, "IMAGE_STRUCTURE") && EQUAL(pszName, "PIXELTYPE"))
    {
        // to get a chance of emitting the warning about this legacy usage
        pszRet = GDALRasterBand::GetMetadataItem(pszName, pszDomain);
    }
    return pszRet;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRasterBand::GetColorInterpretation()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    return m_eBandInterp;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffRasterBand::GetColorTable()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (nBand == 1)
        return m_poGDS->m_poColorTable;

    return nullptr;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GTiffRasterBand::GetNoDataValue(int *pbSuccess)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    int bSuccess = FALSE;
    double dfNoDataValue = GDALPamRasterBand::GetNoDataValue(&bSuccess);
    if (bSuccess)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return dfNoDataValue;
    }

    if (m_bNoDataSet)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return m_dfNoDataValue;
    }

    if (m_poGDS->m_bNoDataSet)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return m_poGDS->m_dfNoDataValue;
    }

    if (m_bNoDataSetAsInt64)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return GDALGetNoDataValueCastToDouble(m_nNoDataValueInt64);
    }

    if (m_poGDS->m_bNoDataSetAsInt64)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return GDALGetNoDataValueCastToDouble(m_poGDS->m_nNoDataValueInt64);
    }

    if (m_bNoDataSetAsUInt64)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return GDALGetNoDataValueCastToDouble(m_nNoDataValueUInt64);
    }

    if (m_poGDS->m_bNoDataSetAsUInt64)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return GDALGetNoDataValueCastToDouble(m_poGDS->m_nNoDataValueUInt64);
    }

    if (pbSuccess)
        *pbSuccess = FALSE;
    return dfNoDataValue;
}

/************************************************************************/
/*                       GetNoDataValueAsInt64()                        */
/************************************************************************/

int64_t GTiffRasterBand::GetNoDataValueAsInt64(int *pbSuccess)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (eDataType == GDT_UInt64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetNoDataValueAsUInt64() should be called instead");
        if (pbSuccess)
            *pbSuccess = FALSE;
        return GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;
    }
    if (eDataType != GDT_Int64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetNoDataValue() should be called instead");
        if (pbSuccess)
            *pbSuccess = FALSE;
        return GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;
    }

    int bSuccess = FALSE;
    const auto nNoDataValue =
        GDALPamRasterBand::GetNoDataValueAsInt64(&bSuccess);
    if (bSuccess)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return nNoDataValue;
    }

    if (m_bNoDataSetAsInt64)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return m_nNoDataValueInt64;
    }

    if (m_poGDS->m_bNoDataSetAsInt64)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return m_poGDS->m_nNoDataValueInt64;
    }

    if (pbSuccess)
        *pbSuccess = FALSE;
    return nNoDataValue;
}

/************************************************************************/
/*                      GetNoDataValueAsUInt64()                        */
/************************************************************************/

uint64_t GTiffRasterBand::GetNoDataValueAsUInt64(int *pbSuccess)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (eDataType == GDT_Int64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetNoDataValueAsInt64() should be called instead");
        if (pbSuccess)
            *pbSuccess = FALSE;
        return GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;
    }
    if (eDataType != GDT_UInt64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetNoDataValue() should be called instead");
        if (pbSuccess)
            *pbSuccess = FALSE;
        return GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;
    }

    int bSuccess = FALSE;
    const auto nNoDataValue =
        GDALPamRasterBand::GetNoDataValueAsUInt64(&bSuccess);
    if (bSuccess)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return nNoDataValue;
    }

    if (m_bNoDataSetAsUInt64)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return m_nNoDataValueUInt64;
    }

    if (m_poGDS->m_bNoDataSetAsUInt64)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;

        return m_poGDS->m_nNoDataValueUInt64;
    }

    if (pbSuccess)
        *pbSuccess = FALSE;
    return nNoDataValue;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GTiffRasterBand::GetOverviewCount()

{
    if (!m_poGDS->AreOverviewsEnabled())
        return 0;

    m_poGDS->ScanDirectories();

    if (m_poGDS->m_nOverviewCount > 0)
    {
        return m_poGDS->m_nOverviewCount;
    }

    const int nOverviewCount = GDALRasterBand::GetOverviewCount();
    if (nOverviewCount > 0)
        return nOverviewCount;

    // Implicit JPEG overviews are normally hidden, except when doing
    // IRasterIO() operations.
    if (m_poGDS->m_nJPEGOverviewVisibilityCounter)
        return m_poGDS->GetJPEGOverviewCount();

    return 0;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetOverview(int i)

{
    m_poGDS->ScanDirectories();

    if (m_poGDS->m_nOverviewCount > 0)
    {
        // Do we have internal overviews?
        if (i < 0 || i >= m_poGDS->m_nOverviewCount)
            return nullptr;

        return m_poGDS->m_papoOverviewDS[i]->GetRasterBand(nBand);
    }

    GDALRasterBand *const poOvrBand = GDALRasterBand::GetOverview(i);
    if (poOvrBand != nullptr)
        return poOvrBand;

    // For consistency with GetOverviewCount(), we should also test
    // m_nJPEGOverviewVisibilityCounter, but it is also convenient to be able
    // to query them for testing purposes.
    if (i >= 0 && i < m_poGDS->GetJPEGOverviewCount())
        return m_poGDS->m_papoJPEGOverviewDS[i]->GetRasterBand(nBand);

    return nullptr;
}

/************************************************************************/
/*                           GetMaskFlags()                             */
/************************************************************************/

int GTiffRasterBand::GetMaskFlags()
{
    m_poGDS->ScanDirectories();

    if (m_poGDS->m_poExternalMaskDS != nullptr)
    {
        return GMF_PER_DATASET;
    }

    if (m_poGDS->m_poMaskDS != nullptr)
    {
        if (m_poGDS->m_poMaskDS->GetRasterCount() == 1)
        {
            return GMF_PER_DATASET;
        }

        return 0;
    }

    if (m_poGDS->m_bIsOverview)
    {
        return m_poGDS->m_poBaseDS->GetRasterBand(nBand)->GetMaskFlags();
    }

    return GDALPamRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetMaskBand()
{
    m_poGDS->ScanDirectories();

    if (m_poGDS->m_poExternalMaskDS != nullptr)
    {
        return m_poGDS->m_poExternalMaskDS->GetRasterBand(1);
    }

    if (m_poGDS->m_poMaskDS != nullptr)
    {
        if (m_poGDS->m_poMaskDS->GetRasterCount() == 1)
            return m_poGDS->m_poMaskDS->GetRasterBand(1);

        return m_poGDS->m_poMaskDS->GetRasterBand(nBand);
    }

    if (m_poGDS->m_bIsOverview)
    {
        GDALRasterBand *poBaseMask =
            m_poGDS->m_poBaseDS->GetRasterBand(nBand)->GetMaskBand();
        if (poBaseMask)
        {
            const int nOverviews = poBaseMask->GetOverviewCount();
            for (int i = 0; i < nOverviews; i++)
            {
                GDALRasterBand *poOvr = poBaseMask->GetOverview(i);
                if (poOvr && poOvr->GetXSize() == GetXSize() &&
                    poOvr->GetYSize() == GetYSize())
                {
                    return poOvr;
                }
            }
        }
    }

    return GDALPamRasterBand::GetMaskBand();
}

/************************************************************************/
/*                            IsMaskBand()                              */
/************************************************************************/

bool GTiffRasterBand::IsMaskBand() const
{
    return (m_poGDS->m_poImageryDS != nullptr &&
            m_poGDS->m_poImageryDS->m_poMaskDS == m_poGDS) ||
           m_eBandInterp == GCI_AlphaBand ||
           m_poGDS->GetMetadataItem("INTERNAL_MASK_FLAGS_1") != nullptr;
}

/************************************************************************/
/*                         GetMaskValueRange()                          */
/************************************************************************/

GDALMaskValueRange GTiffRasterBand::GetMaskValueRange() const
{
    if (!IsMaskBand())
        return GMVR_UNKNOWN;
    if (m_poGDS->m_nBitsPerSample == 1)
        return m_poGDS->m_bPromoteTo8Bits ? GMVR_0_AND_255_ONLY
                                          : GMVR_0_AND_1_ONLY;
    return GMVR_UNKNOWN;
}
