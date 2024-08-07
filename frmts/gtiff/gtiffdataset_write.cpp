/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Write/set operations on GTiffDataset
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
#include "gtiffoddbitsband.h"

#include <cassert>
#include <cerrno>

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "cpl_error.h"
#include "cpl_error_internal.h"  // CPLErrorHandlerAccumulatorStruct
#include "cpl_md5.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "cpl_worker_thread_pool.h"
#include "fetchbufferdirectio.h"
#include "gdal_mdreader.h"          // GDALWriteRPCTXTFile()
#include "gdal_priv_templates.hpp"  // GDALIsValueInRange<>
#include "gdal_thread_pool.h"       // GDALGetGlobalThreadPool()
#include "geovalues.h"              // RasterPixelIsPoint
#include "gt_jpeg_copy.h"
#include "gt_overview.h"  // GTIFFBuildOverviewMetadata()
#include "quant_table_md5sum.h"
#include "quant_table_md5sum_jpeg9e.h"
#include "tif_jxl.h"
#include "tifvsi.h"
#include "xtiffio.h"

#if LIFFLIB_VERSION > 20230908 || defined(INTERNAL_LIBTIFF)
/* libtiff < 4.6.1 doesn't generate a LERC mask for multi-band contig configuration */
#define LIBTIFF_MULTIBAND_LERC_NAN_OK
#endif

static const int knGTIFFJpegTablesModeDefault = JPEGTABLESMODE_QUANT;

static constexpr const char szPROFILE_BASELINE[] = "BASELINE";
static constexpr const char szPROFILE_GeoTIFF[] = "GeoTIFF";
static constexpr const char szPROFILE_GDALGeoTIFF[] = "GDALGeoTIFF";

// Due to libgeotiff/xtiff.c declaring TIFFTAG_GEOTIEPOINTS with field_readcount
// and field_writecount == -1 == TIFF_VARIABLE, we are limited to writing
// 65535 values in that tag. That could potentially be overcome by changing the tag
// declaration to using TIFF_VARIABLE2 where the count is a uint32_t.
constexpr int knMAX_GCP_COUNT =
    static_cast<int>(std::numeric_limits<uint16_t>::max() / 6);

enum
{
    ENDIANNESS_NATIVE,
    ENDIANNESS_LITTLE,
    ENDIANNESS_BIG
};

static signed char GTiffGetWebPLevel(CSLConstList papszOptions)
{
    int nWebPLevel = DEFAULT_WEBP_LEVEL;
    const char *pszValue = CSLFetchNameValue(papszOptions, "WEBP_LEVEL");
    if (pszValue != nullptr)
    {
        nWebPLevel = atoi(pszValue);
        if (!(nWebPLevel >= 1 && nWebPLevel <= 100))
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "WEBP_LEVEL=%s value not recognised, ignoring.", pszValue);
            nWebPLevel = DEFAULT_WEBP_LEVEL;
        }
    }
    return static_cast<signed char>(nWebPLevel);
}

static bool GTiffGetWebPLossless(CSLConstList papszOptions)
{
    return CPLFetchBool(papszOptions, "WEBP_LOSSLESS", false);
}

static double GTiffGetLERCMaxZError(CSLConstList papszOptions)
{
    return CPLAtof(CSLFetchNameValueDef(papszOptions, "MAX_Z_ERROR", "0.0"));
}

static double GTiffGetLERCMaxZErrorOverview(CSLConstList papszOptions)
{
    return CPLAtof(CSLFetchNameValueDef(
        papszOptions, "MAX_Z_ERROR_OVERVIEW",
        CSLFetchNameValueDef(papszOptions, "MAX_Z_ERROR", "0.0")));
}

#if HAVE_JXL
static bool GTiffGetJXLLossless(CSLConstList papszOptions)
{
    return CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "JXL_LOSSLESS", "TRUE"));
}

static uint32_t GTiffGetJXLEffort(CSLConstList papszOptions)
{
    return atoi(CSLFetchNameValueDef(papszOptions, "JXL_EFFORT", "5"));
}

static float GTiffGetJXLDistance(CSLConstList papszOptions)
{
    return static_cast<float>(
        CPLAtof(CSLFetchNameValueDef(papszOptions, "JXL_DISTANCE", "1.0")));
}

static float GTiffGetJXLAlphaDistance(CSLConstList papszOptions)
{
    return static_cast<float>(CPLAtof(
        CSLFetchNameValueDef(papszOptions, "JXL_ALPHA_DISTANCE", "-1.0")));
}

#endif

/************************************************************************/
/*                           FillEmptyTiles()                           */
/************************************************************************/

CPLErr GTiffDataset::FillEmptyTiles()

{
    /* -------------------------------------------------------------------- */
    /*      How many blocks are there in this file?                         */
    /* -------------------------------------------------------------------- */
    const int nBlockCount = m_nPlanarConfig == PLANARCONFIG_SEPARATE
                                ? m_nBlocksPerBand * nBands
                                : m_nBlocksPerBand;

    /* -------------------------------------------------------------------- */
    /*      Fetch block maps.                                               */
    /* -------------------------------------------------------------------- */
    toff_t *panByteCounts = nullptr;

    if (TIFFIsTiled(m_hTIFF))
        TIFFGetField(m_hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts);
    else
        TIFFGetField(m_hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts);

    if (panByteCounts == nullptr)
    {
        // Got here with libtiff 3.9.3 and tiff_write_8 test.
        ReportError(CE_Failure, CPLE_AppDefined,
                    "FillEmptyTiles() failed because panByteCounts == NULL");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Prepare a blank data buffer to write for uninitialized blocks.  */
    /* -------------------------------------------------------------------- */
    const GPtrDiff_t nBlockBytes =
        TIFFIsTiled(m_hTIFF) ? static_cast<GPtrDiff_t>(TIFFTileSize(m_hTIFF))
                             : static_cast<GPtrDiff_t>(TIFFStripSize(m_hTIFF));

    GByte *pabyData = static_cast<GByte *>(VSI_CALLOC_VERBOSE(nBlockBytes, 1));
    if (pabyData == nullptr)
    {
        return CE_Failure;
    }

    // Force tiles completely filled with the nodata value to be written.
    m_bWriteEmptyTiles = true;

    /* -------------------------------------------------------------------- */
    /*      If set, fill data buffer with no data value.                    */
    /* -------------------------------------------------------------------- */
    if ((m_bNoDataSet && m_dfNoDataValue != 0.0) ||
        (m_bNoDataSetAsInt64 && m_nNoDataValueInt64 != 0) ||
        (m_bNoDataSetAsUInt64 && m_nNoDataValueUInt64 != 0))
    {
        const GDALDataType eDataType = GetRasterBand(1)->GetRasterDataType();
        const int nDataTypeSize = GDALGetDataTypeSizeBytes(eDataType);
        if (nDataTypeSize &&
            nDataTypeSize * 8 == static_cast<int>(m_nBitsPerSample))
        {
            if (m_bNoDataSetAsInt64)
            {
                GDALCopyWords64(&m_nNoDataValueInt64, GDT_Int64, 0, pabyData,
                                eDataType, nDataTypeSize,
                                nBlockBytes / nDataTypeSize);
            }
            else if (m_bNoDataSetAsUInt64)
            {
                GDALCopyWords64(&m_nNoDataValueUInt64, GDT_UInt64, 0, pabyData,
                                eDataType, nDataTypeSize,
                                nBlockBytes / nDataTypeSize);
            }
            else
            {
                double dfNoData = m_dfNoDataValue;
                GDALCopyWords64(&dfNoData, GDT_Float64, 0, pabyData, eDataType,
                                nDataTypeSize, nBlockBytes / nDataTypeSize);
            }
        }
        else if (nDataTypeSize)
        {
            // Handle non power-of-two depths.
            // Ideally make a packed buffer, but that is a bit tedious,
            // so use the normal I/O interfaces.

            CPLFree(pabyData);

            pabyData = static_cast<GByte *>(VSI_MALLOC3_VERBOSE(
                m_nBlockXSize, m_nBlockYSize, nDataTypeSize));
            if (pabyData == nullptr)
                return CE_Failure;
            if (m_bNoDataSetAsInt64)
            {
                GDALCopyWords64(&m_nNoDataValueInt64, GDT_Int64, 0, pabyData,
                                eDataType, nDataTypeSize,
                                static_cast<GPtrDiff_t>(m_nBlockXSize) *
                                    m_nBlockYSize);
            }
            else if (m_bNoDataSetAsUInt64)
            {
                GDALCopyWords64(&m_nNoDataValueUInt64, GDT_UInt64, 0, pabyData,
                                eDataType, nDataTypeSize,
                                static_cast<GPtrDiff_t>(m_nBlockXSize) *
                                    m_nBlockYSize);
            }
            else
            {
                GDALCopyWords64(&m_dfNoDataValue, GDT_Float64, 0, pabyData,
                                eDataType, nDataTypeSize,
                                static_cast<GPtrDiff_t>(m_nBlockXSize) *
                                    m_nBlockYSize);
            }
            CPLErr eErr = CE_None;
            for (int iBlock = 0; iBlock < nBlockCount; ++iBlock)
            {
                if (panByteCounts[iBlock] == 0)
                {
                    if (m_nPlanarConfig == PLANARCONFIG_SEPARATE || nBands == 1)
                    {
                        if (GetRasterBand(1 + iBlock / m_nBlocksPerBand)
                                ->WriteBlock((iBlock % m_nBlocksPerBand) %
                                                 m_nBlocksPerRow,
                                             (iBlock % m_nBlocksPerBand) /
                                                 m_nBlocksPerRow,
                                             pabyData) != CE_None)
                        {
                            eErr = CE_Failure;
                        }
                    }
                    else
                    {
                        // In contig case, don't directly call WriteBlock(), as
                        // it could cause useless decompression-recompression.
                        const int nXOff =
                            (iBlock % m_nBlocksPerRow) * m_nBlockXSize;
                        const int nYOff =
                            (iBlock / m_nBlocksPerRow) * m_nBlockYSize;
                        const int nXSize =
                            (nXOff + m_nBlockXSize <= nRasterXSize)
                                ? m_nBlockXSize
                                : nRasterXSize - nXOff;
                        const int nYSize =
                            (nYOff + m_nBlockYSize <= nRasterYSize)
                                ? m_nBlockYSize
                                : nRasterYSize - nYOff;
                        for (int iBand = 1; iBand <= nBands; ++iBand)
                        {
                            if (GetRasterBand(iBand)->RasterIO(
                                    GF_Write, nXOff, nYOff, nXSize, nYSize,
                                    pabyData, nXSize, nYSize, eDataType, 0, 0,
                                    nullptr) != CE_None)
                            {
                                eErr = CE_Failure;
                            }
                        }
                    }
                }
            }
            CPLFree(pabyData);
            return eErr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      When we must fill with zeroes, try to create non-sparse file    */
    /*      w.r.t TIFF spec ... as a sparse file w.r.t filesystem, ie by    */
    /*      seeking to end of file instead of writing zero blocks.          */
    /* -------------------------------------------------------------------- */
    else if (m_nCompression == COMPRESSION_NONE && (m_nBitsPerSample % 8) == 0)
    {
        CPLErr eErr = CE_None;
        // Only use libtiff to write the first sparse block to ensure that it
        // will serialize offset and count arrays back to disk.
        int nCountBlocksToZero = 0;
        for (int iBlock = 0; iBlock < nBlockCount; ++iBlock)
        {
            if (panByteCounts[iBlock] == 0)
            {
                if (nCountBlocksToZero == 0)
                {
                    const bool bWriteEmptyTilesBak = m_bWriteEmptyTiles;
                    m_bWriteEmptyTiles = true;
                    const bool bOK = WriteEncodedTileOrStrip(iBlock, pabyData,
                                                             FALSE) == CE_None;
                    m_bWriteEmptyTiles = bWriteEmptyTilesBak;
                    if (!bOK)
                    {
                        eErr = CE_Failure;
                        break;
                    }
                }
                nCountBlocksToZero++;
            }
        }
        CPLFree(pabyData);

        --nCountBlocksToZero;

        // And then seek to end of file for other ones.
        if (nCountBlocksToZero > 0)
        {
            toff_t *panByteOffsets = nullptr;

            if (TIFFIsTiled(m_hTIFF))
                TIFFGetField(m_hTIFF, TIFFTAG_TILEOFFSETS, &panByteOffsets);
            else
                TIFFGetField(m_hTIFF, TIFFTAG_STRIPOFFSETS, &panByteOffsets);

            if (panByteOffsets == nullptr)
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "FillEmptyTiles() failed because panByteOffsets == NULL");
                return CE_Failure;
            }

            VSILFILE *fpTIF = VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF));
            VSIFSeekL(fpTIF, 0, SEEK_END);
            const vsi_l_offset nOffset = VSIFTellL(fpTIF);

            vsi_l_offset iBlockToZero = 0;
            for (int iBlock = 0; iBlock < nBlockCount; ++iBlock)
            {
                if (panByteCounts[iBlock] == 0)
                {
                    panByteOffsets[iBlock] = static_cast<toff_t>(
                        nOffset + iBlockToZero * nBlockBytes);
                    panByteCounts[iBlock] = nBlockBytes;
                    iBlockToZero++;
                }
            }
            CPLAssert(iBlockToZero ==
                      static_cast<vsi_l_offset>(nCountBlocksToZero));

            if (VSIFTruncateL(fpTIF, nOffset + iBlockToZero * nBlockBytes) != 0)
            {
                eErr = CE_Failure;
                ReportError(CE_Failure, CPLE_FileIO,
                            "Cannot initialize empty blocks");
            }
        }

        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Check all blocks, writing out data for uninitialized blocks.    */
    /* -------------------------------------------------------------------- */

    GByte *pabyRaw = nullptr;
    vsi_l_offset nRawSize = 0;
    CPLErr eErr = CE_None;
    for (int iBlock = 0; iBlock < nBlockCount; ++iBlock)
    {
        if (panByteCounts[iBlock] == 0)
        {
            if (pabyRaw == nullptr)
            {
                if (WriteEncodedTileOrStrip(iBlock, pabyData, FALSE) != CE_None)
                {
                    eErr = CE_Failure;
                    break;
                }

                vsi_l_offset nOffset = 0;
                if (!IsBlockAvailable(iBlock, &nOffset, &nRawSize))
                    break;

                // When using compression, get back the compressed block
                // so we can use the raw API to write it faster.
                if (m_nCompression != COMPRESSION_NONE)
                {
                    pabyRaw = static_cast<GByte *>(
                        VSI_MALLOC_VERBOSE(static_cast<size_t>(nRawSize)));
                    if (pabyRaw)
                    {
                        VSILFILE *fp =
                            VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF));
                        const vsi_l_offset nCurOffset = VSIFTellL(fp);
                        VSIFSeekL(fp, nOffset, SEEK_SET);
                        VSIFReadL(pabyRaw, 1, static_cast<size_t>(nRawSize),
                                  fp);
                        VSIFSeekL(fp, nCurOffset, SEEK_SET);
                    }
                }
            }
            else
            {
                WriteRawStripOrTile(iBlock, pabyRaw,
                                    static_cast<GPtrDiff_t>(nRawSize));
            }
        }
    }

    CPLFree(pabyData);
    VSIFree(pabyRaw);
    return eErr;
}

/************************************************************************/
/*                         HasOnlyNoData()                              */
/************************************************************************/

bool GTiffDataset::HasOnlyNoData(const void *pBuffer, int nWidth, int nHeight,
                                 int nLineStride, int nComponents)
{
    if (m_nSampleFormat == SAMPLEFORMAT_COMPLEXINT ||
        m_nSampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP)
        return false;
    if (m_bNoDataSetAsInt64 || m_bNoDataSetAsUInt64)
        return false;  // FIXME: over pessimistic
    return GDALBufferHasOnlyNoData(
        pBuffer, m_bNoDataSet ? m_dfNoDataValue : 0.0, nWidth, nHeight,
        nLineStride, nComponents, m_nBitsPerSample,
        m_nSampleFormat == SAMPLEFORMAT_UINT  ? GSF_UNSIGNED_INT
        : m_nSampleFormat == SAMPLEFORMAT_INT ? GSF_SIGNED_INT
                                              : GSF_FLOATING_POINT);
}

/************************************************************************/
/*                     IsFirstPixelEqualToNoData()                      */
/************************************************************************/

inline bool GTiffDataset::IsFirstPixelEqualToNoData(const void *pBuffer)
{
    const GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
    const double dfEffectiveNoData = (m_bNoDataSet) ? m_dfNoDataValue : 0.0;
    if (m_bNoDataSetAsInt64 || m_bNoDataSetAsUInt64)
        return true;  // FIXME: over pessimistic
    if (m_nBitsPerSample == 8 ||
        (m_nBitsPerSample < 8 && dfEffectiveNoData == 0))
    {
        if (eDT == GDT_Int8)
        {
            return GDALIsValueInRange<signed char>(dfEffectiveNoData) &&
                   *(static_cast<const signed char *>(pBuffer)) ==
                       static_cast<signed char>(dfEffectiveNoData);
        }
        return GDALIsValueInRange<GByte>(dfEffectiveNoData) &&
               *(static_cast<const GByte *>(pBuffer)) ==
                   static_cast<GByte>(dfEffectiveNoData);
    }
    if (m_nBitsPerSample == 16 && eDT == GDT_UInt16)
    {
        return GDALIsValueInRange<GUInt16>(dfEffectiveNoData) &&
               *(static_cast<const GUInt16 *>(pBuffer)) ==
                   static_cast<GUInt16>(dfEffectiveNoData);
    }
    if (m_nBitsPerSample == 16 && eDT == GDT_Int16)
    {
        return GDALIsValueInRange<GInt16>(dfEffectiveNoData) &&
               *(static_cast<const GInt16 *>(pBuffer)) ==
                   static_cast<GInt16>(dfEffectiveNoData);
    }
    if (m_nBitsPerSample == 32 && eDT == GDT_UInt32)
    {
        return GDALIsValueInRange<GUInt32>(dfEffectiveNoData) &&
               *(static_cast<const GUInt32 *>(pBuffer)) ==
                   static_cast<GUInt32>(dfEffectiveNoData);
    }
    if (m_nBitsPerSample == 32 && eDT == GDT_Int32)
    {
        return GDALIsValueInRange<GInt32>(dfEffectiveNoData) &&
               *(static_cast<const GInt32 *>(pBuffer)) ==
                   static_cast<GInt32>(dfEffectiveNoData);
    }
    if (m_nBitsPerSample == 64 && eDT == GDT_UInt64)
    {
        return GDALIsValueInRange<std::uint64_t>(dfEffectiveNoData) &&
               *(static_cast<const std::uint64_t *>(pBuffer)) ==
                   static_cast<std::uint64_t>(dfEffectiveNoData);
    }
    if (m_nBitsPerSample == 64 && eDT == GDT_Int64)
    {
        return GDALIsValueInRange<std::int64_t>(dfEffectiveNoData) &&
               *(static_cast<const std::int64_t *>(pBuffer)) ==
                   static_cast<std::int64_t>(dfEffectiveNoData);
    }
    if (m_nBitsPerSample == 32 && eDT == GDT_Float32)
    {
        if (CPLIsNan(m_dfNoDataValue))
            return CPL_TO_BOOL(
                CPLIsNan(*(static_cast<const float *>(pBuffer))));
        return GDALIsValueInRange<float>(dfEffectiveNoData) &&
               *(static_cast<const float *>(pBuffer)) ==
                   static_cast<float>(dfEffectiveNoData);
    }
    if (m_nBitsPerSample == 64 && eDT == GDT_Float64)
    {
        if (CPLIsNan(dfEffectiveNoData))
            return CPL_TO_BOOL(
                CPLIsNan(*(static_cast<const double *>(pBuffer))));
        return *(static_cast<const double *>(pBuffer)) == dfEffectiveNoData;
    }
    return false;
}

/************************************************************************/
/*                      WriteDealWithLercAndNan()                       */
/************************************************************************/

template <typename T>
void GTiffDataset::WriteDealWithLercAndNan(T *pBuffer, int nActualBlockWidth,
                                           int nActualBlockHeight,
                                           int nStrileHeight)
{
    // This method does 2 things:
    // - warn the user if he tries to write NaN values with libtiff < 4.6.1
    //   and multi-band PlanarConfig=Contig configuration
    // - and in right-most and bottom-most tiles, replace non accessible
    //   pixel values by a safe one.

    const auto fPaddingValue =
#if !defined(LIBTIFF_MULTIBAND_LERC_NAN_OK)
        m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands > 1
            ? 0
            :
#endif
            std::numeric_limits<T>::quiet_NaN();

    const int nBandsPerStrile =
        m_nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;
    for (int j = 0; j < nActualBlockHeight; ++j)
    {
#if !defined(LIBTIFF_MULTIBAND_LERC_NAN_OK)
        static bool bHasWarned = false;
        if (m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands > 1 && !bHasWarned)
        {
            for (int i = 0; i < nActualBlockWidth * nBandsPerStrile; ++i)
            {
                if (std::isnan(
                        pBuffer[j * m_nBlockXSize * nBandsPerStrile + i]))
                {
                    bHasWarned = true;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "libtiff < 4.6.1 does not handle properly NaN "
                             "values for multi-band PlanarConfig=Contig "
                             "configuration. As a workaround, you can set the "
                             "INTERLEAVE=BAND creation option.");
                    break;
                }
            }
        }
#endif
        for (int i = nActualBlockWidth * nBandsPerStrile;
             i < m_nBlockXSize * nBandsPerStrile; ++i)
        {
            pBuffer[j * m_nBlockXSize * nBandsPerStrile + i] = fPaddingValue;
        }
    }
    for (int j = nActualBlockHeight; j < nStrileHeight; ++j)
    {
        for (int i = 0; i < m_nBlockXSize * nBandsPerStrile; ++i)
        {
            pBuffer[j * m_nBlockXSize * nBandsPerStrile + i] = fPaddingValue;
        }
    }
}

/************************************************************************/
/*                        WriteEncodedTile()                            */
/************************************************************************/

bool GTiffDataset::WriteEncodedTile(uint32_t tile, GByte *pabyData,
                                    int bPreserveDataBuffer)
{

    const int iColumn = (tile % m_nBlocksPerBand) % m_nBlocksPerRow;
    const int iRow = (tile % m_nBlocksPerBand) / m_nBlocksPerRow;

    const int nActualBlockWidth = (iColumn == m_nBlocksPerRow - 1)
                                      ? nRasterXSize - iColumn * m_nBlockXSize
                                      : m_nBlockXSize;
    const int nActualBlockHeight = (iRow == m_nBlocksPerColumn - 1)
                                       ? nRasterYSize - iRow * m_nBlockYSize
                                       : m_nBlockYSize;

    /* -------------------------------------------------------------------- */
    /*      Don't write empty blocks in some cases.                         */
    /* -------------------------------------------------------------------- */
    if (!m_bWriteEmptyTiles && IsFirstPixelEqualToNoData(pabyData))
    {
        if (!IsBlockAvailable(tile))
        {
            const int nComponents =
                m_nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;

            if (HasOnlyNoData(pabyData, nActualBlockWidth, nActualBlockHeight,
                              m_nBlockXSize, nComponents))
            {
                return true;
            }
        }
    }

    // Is this a partial right edge or bottom edge tile?
    const bool bPartialTile = (nActualBlockWidth < m_nBlockXSize) ||
                              (nActualBlockHeight < m_nBlockYSize);

    const bool bIsLercFloatingPoint =
        m_nCompression == COMPRESSION_LERC &&
        (GetRasterBand(1)->GetRasterDataType() == GDT_Float32 ||
         GetRasterBand(1)->GetRasterDataType() == GDT_Float64);

    // Do we need to spread edge values right or down for a partial
    // JPEG encoded tile?  We do this to avoid edge artifacts.
    // We also need to be careful with LERC and NaN values
    const bool bNeedTempBuffer =
        bPartialTile &&
        (m_nCompression == COMPRESSION_JPEG || bIsLercFloatingPoint);

    // If we need to fill out the tile, or if we want to prevent
    // TIFFWriteEncodedTile from altering the buffer as part of
    // byte swapping the data on write then we will need a temporary
    // working buffer.  If not, we can just do a direct write.
    const GPtrDiff_t cc = static_cast<GPtrDiff_t>(TIFFTileSize(m_hTIFF));

    if (bPreserveDataBuffer &&
        (TIFFIsByteSwapped(m_hTIFF) || bNeedTempBuffer || m_panMaskOffsetLsb))
    {
        if (m_pabyTempWriteBuffer == nullptr)
        {
            m_pabyTempWriteBuffer = CPLMalloc(cc);
        }
        memcpy(m_pabyTempWriteBuffer, pabyData, cc);

        pabyData = static_cast<GByte *>(m_pabyTempWriteBuffer);
    }

    // Perform tile fill if needed.
    // TODO: we should also handle the case of nBitsPerSample == 12
    // but this is more involved.
    if (bPartialTile && m_nCompression == COMPRESSION_JPEG &&
        m_nBitsPerSample == 8)
    {
        const int nComponents =
            m_nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;

        CPLDebug("GTiff", "Filling out jpeg edge tile on write.");

        const int nRightPixelsToFill =
            iColumn == m_nBlocksPerRow - 1
                ? m_nBlockXSize * (iColumn + 1) - nRasterXSize
                : 0;
        const int nBottomPixelsToFill =
            iRow == m_nBlocksPerColumn - 1
                ? m_nBlockYSize * (iRow + 1) - nRasterYSize
                : 0;

        // Fill out to the right.
        const int iSrcX = m_nBlockXSize - nRightPixelsToFill - 1;

        for (int iX = iSrcX + 1; iX < m_nBlockXSize; ++iX)
        {
            for (int iY = 0; iY < m_nBlockYSize; ++iY)
            {
                memcpy(pabyData +
                           (static_cast<GPtrDiff_t>(m_nBlockXSize) * iY + iX) *
                               nComponents,
                       pabyData + (static_cast<GPtrDiff_t>(m_nBlockXSize) * iY +
                                   iSrcX) *
                                      nComponents,
                       nComponents);
            }
        }

        // Now fill out the bottom.
        const int iSrcY = m_nBlockYSize - nBottomPixelsToFill - 1;
        for (int iY = iSrcY + 1; iY < m_nBlockYSize; ++iY)
        {
            memcpy(pabyData + static_cast<GPtrDiff_t>(m_nBlockXSize) *
                                  nComponents * iY,
                   pabyData + static_cast<GPtrDiff_t>(m_nBlockXSize) *
                                  nComponents * iSrcY,
                   static_cast<GPtrDiff_t>(m_nBlockXSize) * nComponents);
        }
    }

    if (bIsLercFloatingPoint &&
        (bPartialTile
#if !defined(LIBTIFF_MULTIBAND_LERC_NAN_OK)
         /* libtiff < 4.6.1 doesn't generate a LERC mask for multi-band contig configuration */
         || (m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands > 1)
#endif
             ))
    {
        if (GetRasterBand(1)->GetRasterDataType() == GDT_Float32)
            WriteDealWithLercAndNan(reinterpret_cast<float *>(pabyData),
                                    nActualBlockWidth, nActualBlockHeight,
                                    m_nBlockYSize);
        else
            WriteDealWithLercAndNan(reinterpret_cast<double *>(pabyData),
                                    nActualBlockWidth, nActualBlockHeight,
                                    m_nBlockYSize);
    }

    if (m_panMaskOffsetLsb)
    {
        const int iBand = m_nPlanarConfig == PLANARCONFIG_SEPARATE
                              ? static_cast<int>(tile) / m_nBlocksPerBand
                              : -1;
        DiscardLsb(pabyData, cc, iBand);
    }

    if (m_bStreamingOut)
    {
        if (tile != static_cast<uint32_t>(m_nLastWrittenBlockId + 1))
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Attempt to write block %d whereas %d was expected",
                        tile, m_nLastWrittenBlockId + 1);
            return false;
        }
        if (static_cast<GPtrDiff_t>(VSIFWriteL(pabyData, 1, cc, m_fpToWrite)) !=
            cc)
        {
            ReportError(CE_Failure, CPLE_FileIO,
                        "Could not write " CPL_FRMT_GUIB " bytes",
                        static_cast<GUIntBig>(cc));
            return false;
        }
        m_nLastWrittenBlockId = tile;
        return true;
    }

    /* -------------------------------------------------------------------- */
    /*      Should we do compression in a worker thread ?                   */
    /* -------------------------------------------------------------------- */
    if (SubmitCompressionJob(tile, pabyData, cc, m_nBlockYSize))
        return true;

    return TIFFWriteEncodedTile(m_hTIFF, tile, pabyData, cc) == cc;
}

/************************************************************************/
/*                        WriteEncodedStrip()                           */
/************************************************************************/

bool GTiffDataset::WriteEncodedStrip(uint32_t strip, GByte *pabyData,
                                     int bPreserveDataBuffer)
{
    GPtrDiff_t cc = static_cast<GPtrDiff_t>(TIFFStripSize(m_hTIFF));
    const auto ccFull = cc;

    /* -------------------------------------------------------------------- */
    /*      If this is the last strip in the image, and is partial, then    */
    /*      we need to trim the number of scanlines written to the          */
    /*      amount of valid data we have. (#2748)                           */
    /* -------------------------------------------------------------------- */
    const int nStripWithinBand = strip % m_nBlocksPerBand;
    int nStripHeight = m_nRowsPerStrip;

    if (nStripWithinBand * nStripHeight > GetRasterYSize() - nStripHeight)
    {
        nStripHeight = GetRasterYSize() - nStripWithinBand * m_nRowsPerStrip;
        cc = (cc / m_nRowsPerStrip) * nStripHeight;
        CPLDebug("GTiff",
                 "Adjusted bytes to write from " CPL_FRMT_GUIB
                 " to " CPL_FRMT_GUIB ".",
                 static_cast<GUIntBig>(TIFFStripSize(m_hTIFF)),
                 static_cast<GUIntBig>(cc));
    }

    /* -------------------------------------------------------------------- */
    /*      Don't write empty blocks in some cases.                         */
    /* -------------------------------------------------------------------- */
    if (!m_bWriteEmptyTiles && IsFirstPixelEqualToNoData(pabyData))
    {
        if (!IsBlockAvailable(strip))
        {
            const int nComponents =
                m_nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;

            if (HasOnlyNoData(pabyData, m_nBlockXSize, nStripHeight,
                              m_nBlockXSize, nComponents))
            {
                return true;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      TIFFWriteEncodedStrip can alter the passed buffer if            */
    /*      byte-swapping is necessary so we use a temporary buffer         */
    /*      before calling it.                                              */
    /* -------------------------------------------------------------------- */
    if (bPreserveDataBuffer &&
        (TIFFIsByteSwapped(m_hTIFF) || m_panMaskOffsetLsb))
    {
        if (m_pabyTempWriteBuffer == nullptr)
        {
            m_pabyTempWriteBuffer = CPLMalloc(ccFull);
        }
        memcpy(m_pabyTempWriteBuffer, pabyData, cc);
        pabyData = static_cast<GByte *>(m_pabyTempWriteBuffer);
    }

#if !defined(LIBTIFF_MULTIBAND_LERC_NAN_OK)
    const bool bIsLercFloatingPoint =
        m_nCompression == COMPRESSION_LERC &&
        (GetRasterBand(1)->GetRasterDataType() == GDT_Float32 ||
         GetRasterBand(1)->GetRasterDataType() == GDT_Float64);
    if (bIsLercFloatingPoint &&
        /* libtiff < 4.6.1 doesn't generate a LERC mask for multi-band contig configuration */
        m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands > 1)
    {
        if (GetRasterBand(1)->GetRasterDataType() == GDT_Float32)
            WriteDealWithLercAndNan(reinterpret_cast<float *>(pabyData),
                                    m_nBlockXSize, nStripHeight, nStripHeight);
        else
            WriteDealWithLercAndNan(reinterpret_cast<double *>(pabyData),
                                    m_nBlockXSize, nStripHeight, nStripHeight);
    }
#endif

    if (m_panMaskOffsetLsb)
    {
        int iBand = m_nPlanarConfig == PLANARCONFIG_SEPARATE
                        ? static_cast<int>(strip) / m_nBlocksPerBand
                        : -1;
        DiscardLsb(pabyData, cc, iBand);
    }

    if (m_bStreamingOut)
    {
        if (strip != static_cast<uint32_t>(m_nLastWrittenBlockId + 1))
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Attempt to write block %d whereas %d was expected",
                        strip, m_nLastWrittenBlockId + 1);
            return false;
        }
        if (static_cast<GPtrDiff_t>(VSIFWriteL(pabyData, 1, cc, m_fpToWrite)) !=
            cc)
        {
            ReportError(CE_Failure, CPLE_FileIO,
                        "Could not write " CPL_FRMT_GUIB " bytes",
                        static_cast<GUIntBig>(cc));
            return false;
        }
        m_nLastWrittenBlockId = strip;
        return true;
    }

    /* -------------------------------------------------------------------- */
    /*      Should we do compression in a worker thread ?                   */
    /* -------------------------------------------------------------------- */
    if (SubmitCompressionJob(strip, pabyData, cc, nStripHeight))
        return true;

    return TIFFWriteEncodedStrip(m_hTIFF, strip, pabyData, cc) == cc;
}

/************************************************************************/
/*                        InitCompressionThreads()                      */
/************************************************************************/

void GTiffDataset::InitCompressionThreads(bool bUpdateMode,
                                          CSLConstList papszOptions)
{
    // Raster == tile, then no need for threads
    if (m_nBlockXSize == nRasterXSize && m_nBlockYSize == nRasterYSize)
        return;

    const char *pszValue = CSLFetchNameValue(papszOptions, "NUM_THREADS");
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
            if ((bUpdateMode && m_nCompression != COMPRESSION_NONE) ||
                (nBands >= 1 && IsMultiThreadedReadCompatible()))
            {
                CPLDebug("GTiff",
                         "Using up to %d threads for compression/decompression",
                         nThreads);

                m_poThreadPool = GDALGetGlobalThreadPool(nThreads);
                if (bUpdateMode && m_poThreadPool)
                    m_poCompressQueue = m_poThreadPool->CreateJobQueue();

                if (m_poCompressQueue != nullptr)
                {
                    // Add a margin of an extra job w.r.t thread number
                    // so as to optimize compression time (enables the main
                    // thread to do boring I/O while all CPUs are working).
                    m_asCompressionJobs.resize(nThreads + 1);
                    memset(&m_asCompressionJobs[0], 0,
                           m_asCompressionJobs.size() *
                               sizeof(GTiffCompressionJob));
                    for (int i = 0;
                         i < static_cast<int>(m_asCompressionJobs.size()); ++i)
                    {
                        m_asCompressionJobs[i].pszTmpFilename =
                            CPLStrdup(CPLSPrintf("/vsimem/gtiff/thread/job/%p",
                                                 &m_asCompressionJobs[i]));
                        m_asCompressionJobs[i].nStripOrTile = -1;
                    }
                    m_hCompressThreadPoolMutex = CPLCreateMutex();
                    CPLReleaseMutex(m_hCompressThreadPoolMutex);

                    // This is kind of a hack, but basically using
                    // TIFFWriteRawStrip/Tile and then TIFFReadEncodedStrip/Tile
                    // does not work on a newly created file, because
                    // TIFF_MYBUFFER is not set in tif_flags
                    // (if using TIFFWriteEncodedStrip/Tile first,
                    // TIFFWriteBufferSetup() is automatically called).
                    // This should likely rather fixed in libtiff itself.
                    CPL_IGNORE_RET_VAL(
                        TIFFWriteBufferSetup(m_hTIFF, nullptr, -1));
                }
            }
        }
        else if (nThreads < 0 ||
                 (!EQUAL(pszValue, "0") && !EQUAL(pszValue, "1") &&
                  !EQUAL(pszValue, "ALL_CPUS")))
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "Invalid value for NUM_THREADS: %s", pszValue);
        }
    }
}

/************************************************************************/
/*                      ThreadCompressionFunc()                         */
/************************************************************************/

void GTiffDataset::ThreadCompressionFunc(void *pData)
{
    GTiffCompressionJob *psJob = static_cast<GTiffCompressionJob *>(pData);
    GTiffDataset *poDS = psJob->poDS;

    VSILFILE *fpTmp = VSIFOpenL(psJob->pszTmpFilename, "wb+");
    TIFF *hTIFFTmp = VSI_TIFFOpen(
        psJob->pszTmpFilename, psJob->bTIFFIsBigEndian ? "wb+" : "wl+", fpTmp);
    CPLAssert(hTIFFTmp != nullptr);
    TIFFSetField(hTIFFTmp, TIFFTAG_IMAGEWIDTH, poDS->m_nBlockXSize);
    TIFFSetField(hTIFFTmp, TIFFTAG_IMAGELENGTH, psJob->nHeight);
    TIFFSetField(hTIFFTmp, TIFFTAG_BITSPERSAMPLE, poDS->m_nBitsPerSample);
    TIFFSetField(hTIFFTmp, TIFFTAG_COMPRESSION, poDS->m_nCompression);
    TIFFSetField(hTIFFTmp, TIFFTAG_PHOTOMETRIC, poDS->m_nPhotometric);
    TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLEFORMAT, poDS->m_nSampleFormat);
    TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLESPERPIXEL, poDS->m_nSamplesPerPixel);
    TIFFSetField(hTIFFTmp, TIFFTAG_ROWSPERSTRIP, poDS->m_nBlockYSize);
    TIFFSetField(hTIFFTmp, TIFFTAG_PLANARCONFIG, poDS->m_nPlanarConfig);
    if (psJob->nPredictor != PREDICTOR_NONE)
        TIFFSetField(hTIFFTmp, TIFFTAG_PREDICTOR, psJob->nPredictor);
    if (poDS->m_nCompression == COMPRESSION_LERC)
    {
        TIFFSetField(hTIFFTmp, TIFFTAG_LERC_PARAMETERS, 2,
                     poDS->m_anLercAddCompressionAndVersion);
    }
    if (psJob->nExtraSampleCount)
    {
        TIFFSetField(hTIFFTmp, TIFFTAG_EXTRASAMPLES, psJob->nExtraSampleCount,
                     psJob->pExtraSamples);
    }

    poDS->RestoreVolatileParameters(hTIFFTmp);

    bool bOK = TIFFWriteEncodedStrip(hTIFFTmp, 0, psJob->pabyBuffer,
                                     psJob->nBufferSize) == psJob->nBufferSize;

    toff_t nOffset = 0;
    if (bOK)
    {
        toff_t *panOffsets = nullptr;
        toff_t *panByteCounts = nullptr;
        TIFFGetField(hTIFFTmp, TIFFTAG_STRIPOFFSETS, &panOffsets);
        TIFFGetField(hTIFFTmp, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts);

        nOffset = panOffsets[0];
        psJob->nCompressedBufferSize =
            static_cast<GPtrDiff_t>(panByteCounts[0]);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error when compressing strip/tile %d", psJob->nStripOrTile);
    }

    XTIFFClose(hTIFFTmp);
    if (VSIFCloseL(fpTmp) != 0)
    {
        if (bOK)
        {
            bOK = false;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error when compressing strip/tile %d",
                     psJob->nStripOrTile);
        }
    }

    if (bOK)
    {
        vsi_l_offset nFileSize = 0;
        GByte *pabyCompressedBuffer =
            VSIGetMemFileBuffer(psJob->pszTmpFilename, &nFileSize, FALSE);
        CPLAssert(static_cast<vsi_l_offset>(
                      nOffset + psJob->nCompressedBufferSize) <= nFileSize);
        psJob->pabyCompressedBuffer = pabyCompressedBuffer + nOffset;
    }
    else
    {
        psJob->pabyCompressedBuffer = nullptr;
        psJob->nCompressedBufferSize = 0;
    }

    auto mutex = poDS->m_poBaseDS ? poDS->m_poBaseDS->m_hCompressThreadPoolMutex
                                  : poDS->m_hCompressThreadPoolMutex;
    if (mutex)
    {
        CPLAcquireMutex(mutex, 1000.0);
        psJob->bReady = true;
        CPLReleaseMutex(mutex);
    }
}

/************************************************************************/
/*                        WriteRawStripOrTile()                         */
/************************************************************************/

void GTiffDataset::WriteRawStripOrTile(int nStripOrTile,
                                       GByte *pabyCompressedBuffer,
                                       GPtrDiff_t nCompressedBufferSize)
{
#ifdef DEBUG_VERBOSE
    CPLDebug("GTIFF", "Writing raw strip/tile %d, size " CPL_FRMT_GUIB,
             nStripOrTile, static_cast<GUIntBig>(nCompressedBufferSize));
#endif
    toff_t *panOffsets = nullptr;
    toff_t *panByteCounts = nullptr;
    bool bWriteAtEnd = true;
    bool bWriteLeader = m_bLeaderSizeAsUInt4;
    bool bWriteTrailer = m_bTrailerRepeatedLast4BytesRepeated;
    if (TIFFGetField(m_hTIFF,
                     TIFFIsTiled(m_hTIFF) ? TIFFTAG_TILEOFFSETS
                                          : TIFFTAG_STRIPOFFSETS,
                     &panOffsets) &&
        panOffsets != nullptr && panOffsets[nStripOrTile] != 0)
    {
        // Forces TIFFAppendStrip() to consider if the location of the
        // tile/strip can be reused or if the strile should be written at end of
        // file.
        TIFFSetWriteOffset(m_hTIFF, 0);

        if (m_bBlockOrderRowMajor)
        {
            if (TIFFGetField(m_hTIFF,
                             TIFFIsTiled(m_hTIFF) ? TIFFTAG_TILEBYTECOUNTS
                                                  : TIFFTAG_STRIPBYTECOUNTS,
                             &panByteCounts) &&
                panByteCounts != nullptr)
            {
                if (static_cast<GUIntBig>(nCompressedBufferSize) >
                    panByteCounts[nStripOrTile])
                {
                    GTiffDataset *poRootDS = m_poBaseDS ? m_poBaseDS : this;
                    if (!poRootDS->m_bKnownIncompatibleEdition &&
                        !poRootDS->m_bWriteKnownIncompatibleEdition)
                    {
                        ReportError(
                            CE_Warning, CPLE_AppDefined,
                            "A strile cannot be rewritten in place, which "
                            "invalidates the BLOCK_ORDER optimization.");
                        poRootDS->m_bKnownIncompatibleEdition = true;
                        poRootDS->m_bWriteKnownIncompatibleEdition = true;
                    }
                }
                // For mask interleaving, if the size is not exactly the same,
                // completely give up (we could potentially move the mask in
                // case the imagery is smaller)
                else if (m_poMaskDS && m_bMaskInterleavedWithImagery &&
                         static_cast<GUIntBig>(nCompressedBufferSize) !=
                             panByteCounts[nStripOrTile])
                {
                    GTiffDataset *poRootDS = m_poBaseDS ? m_poBaseDS : this;
                    if (!poRootDS->m_bKnownIncompatibleEdition &&
                        !poRootDS->m_bWriteKnownIncompatibleEdition)
                    {
                        ReportError(
                            CE_Warning, CPLE_AppDefined,
                            "A strile cannot be rewritten in place, which "
                            "invalidates the MASK_INTERLEAVED_WITH_IMAGERY "
                            "optimization.");
                        poRootDS->m_bKnownIncompatibleEdition = true;
                        poRootDS->m_bWriteKnownIncompatibleEdition = true;
                    }
                    bWriteLeader = false;
                    bWriteTrailer = false;
                    if (m_bLeaderSizeAsUInt4)
                    {
                        // If there was a valid leader, invalidat it
                        VSI_TIFFSeek(m_hTIFF, panOffsets[nStripOrTile] - 4,
                                     SEEK_SET);
                        uint32_t nOldSize;
                        VSIFReadL(&nOldSize, 1, 4,
                                  VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF)));
                        CPL_LSBPTR32(&nOldSize);
                        if (nOldSize == panByteCounts[nStripOrTile])
                        {
                            uint32_t nInvalidatedSize = 0;
                            VSI_TIFFSeek(m_hTIFF, panOffsets[nStripOrTile] - 4,
                                         SEEK_SET);
                            VSI_TIFFWrite(m_hTIFF, &nInvalidatedSize,
                                          sizeof(nInvalidatedSize));
                        }
                    }
                }
                else
                {
                    bWriteAtEnd = false;
                }
            }
        }
    }
    if (bWriteLeader &&
        static_cast<GUIntBig>(nCompressedBufferSize) <= 0xFFFFFFFFU)
    {
        // cppcheck-suppress knownConditionTrueFalse
        if (bWriteAtEnd)
        {
            VSI_TIFFSeek(m_hTIFF, 0, SEEK_END);
        }
        else
        {
            // If we rewrite an existing strile in place with an existing
            // leader, check that the leader is valid, before rewriting it. And
            // if it is not valid, then do not write the trailer, as we could
            // corrupt other data.
            VSI_TIFFSeek(m_hTIFF, panOffsets[nStripOrTile] - 4, SEEK_SET);
            uint32_t nOldSize;
            VSIFReadL(&nOldSize, 1, 4,
                      VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF)));
            CPL_LSBPTR32(&nOldSize);
            bWriteLeader =
                panByteCounts && nOldSize == panByteCounts[nStripOrTile];
            bWriteTrailer = bWriteLeader;
            VSI_TIFFSeek(m_hTIFF, panOffsets[nStripOrTile] - 4, SEEK_SET);
        }
        // cppcheck-suppress knownConditionTrueFalse
        if (bWriteLeader)
        {
            uint32_t nSize = static_cast<uint32_t>(nCompressedBufferSize);
            CPL_LSBPTR32(&nSize);
            if (!VSI_TIFFWrite(m_hTIFF, &nSize, sizeof(nSize)))
                m_bWriteError = true;
        }
    }
    tmsize_t written;
    if (TIFFIsTiled(m_hTIFF))
        written = TIFFWriteRawTile(m_hTIFF, nStripOrTile, pabyCompressedBuffer,
                                   nCompressedBufferSize);
    else
        written = TIFFWriteRawStrip(m_hTIFF, nStripOrTile, pabyCompressedBuffer,
                                    nCompressedBufferSize);
    if (written != nCompressedBufferSize)
        m_bWriteError = true;
    if (bWriteTrailer &&
        static_cast<GUIntBig>(nCompressedBufferSize) <= 0xFFFFFFFFU)
    {
        GByte abyLastBytes[4] = {};
        if (nCompressedBufferSize >= 4)
            memcpy(abyLastBytes,
                   pabyCompressedBuffer + nCompressedBufferSize - 4, 4);
        else
            memcpy(abyLastBytes, pabyCompressedBuffer, nCompressedBufferSize);
        if (!VSI_TIFFWrite(m_hTIFF, abyLastBytes, 4))
            m_bWriteError = true;
    }
}

/************************************************************************/
/*                        WaitCompletionForJobIdx()                     */
/************************************************************************/

void GTiffDataset::WaitCompletionForJobIdx(int i)
{
    auto poQueue = m_poBaseDS ? m_poBaseDS->m_poCompressQueue.get()
                              : m_poCompressQueue.get();
    auto &oQueue = m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
    auto &asJobs =
        m_poBaseDS ? m_poBaseDS->m_asCompressionJobs : m_asCompressionJobs;
    auto mutex = m_poBaseDS ? m_poBaseDS->m_hCompressThreadPoolMutex
                            : m_hCompressThreadPoolMutex;

    CPLAssert(i >= 0 && static_cast<size_t>(i) < asJobs.size());
    CPLAssert(asJobs[i].nStripOrTile >= 0);
    CPLAssert(!oQueue.empty());

    bool bHasWarned = false;
    while (true)
    {
        CPLAcquireMutex(mutex, 1000.0);
        const bool bReady = asJobs[i].bReady;
        CPLReleaseMutex(mutex);
        if (!bReady)
        {
            if (!bHasWarned)
            {
                CPLDebug("GTIFF",
                         "Waiting for worker job to finish handling block %d",
                         asJobs[i].nStripOrTile);
                bHasWarned = true;
            }
            poQueue->GetPool()->WaitEvent();
        }
        else
        {
            break;
        }
    }

    if (asJobs[i].nCompressedBufferSize)
    {
        asJobs[i].poDS->WriteRawStripOrTile(asJobs[i].nStripOrTile,
                                            asJobs[i].pabyCompressedBuffer,
                                            asJobs[i].nCompressedBufferSize);
    }
    asJobs[i].pabyCompressedBuffer = nullptr;
    asJobs[i].nBufferSize = 0;
    asJobs[i].bReady = false;
    asJobs[i].nStripOrTile = -1;
    oQueue.pop();
}

/************************************************************************/
/*                        WaitCompletionForBlock()                      */
/************************************************************************/

void GTiffDataset::WaitCompletionForBlock(int nBlockId)
{
    auto poQueue = m_poBaseDS ? m_poBaseDS->m_poCompressQueue.get()
                              : m_poCompressQueue.get();
    // cppcheck-suppress constVariableReference
    auto &oQueue = m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
    // cppcheck-suppress constVariableReference
    auto &asJobs =
        m_poBaseDS ? m_poBaseDS->m_asCompressionJobs : m_asCompressionJobs;

    if (poQueue != nullptr && !oQueue.empty())
    {
        for (int i = 0; i < static_cast<int>(asJobs.size()); ++i)
        {
            if (asJobs[i].poDS == this && asJobs[i].nStripOrTile == nBlockId)
            {
                while (!oQueue.empty() &&
                       !(asJobs[oQueue.front()].poDS == this &&
                         asJobs[oQueue.front()].nStripOrTile == nBlockId))
                {
                    WaitCompletionForJobIdx(oQueue.front());
                }
                CPLAssert(!oQueue.empty() &&
                          asJobs[oQueue.front()].poDS == this &&
                          asJobs[oQueue.front()].nStripOrTile == nBlockId);
                WaitCompletionForJobIdx(oQueue.front());
            }
        }
    }
}

/************************************************************************/
/*                      SubmitCompressionJob()                          */
/************************************************************************/

bool GTiffDataset::SubmitCompressionJob(int nStripOrTile, GByte *pabyData,
                                        GPtrDiff_t cc, int nHeight)
{
    /* -------------------------------------------------------------------- */
    /*      Should we do compression in a worker thread ?                   */
    /* -------------------------------------------------------------------- */
    auto poQueue = m_poBaseDS ? m_poBaseDS->m_poCompressQueue.get()
                              : m_poCompressQueue.get();

    if (poQueue && m_nCompression == COMPRESSION_NONE)
    {
        // We don't do multi-threaded compression for uncompressed...
        // but we must wait for other related compression tasks (e.g mask)
        // to be completed
        poQueue->WaitCompletion();

        // Flush remaining data
        // cppcheck-suppress constVariableReference
        auto &oQueue =
            m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
        while (!oQueue.empty())
        {
            WaitCompletionForJobIdx(oQueue.front());
        }
    }

    const auto SetupJob =
        [this, pabyData, cc, nHeight, nStripOrTile](GTiffCompressionJob &sJob)
    {
        sJob.poDS = this;
        sJob.bTIFFIsBigEndian = CPL_TO_BOOL(TIFFIsBigEndian(m_hTIFF));
        sJob.pabyBuffer = static_cast<GByte *>(CPLRealloc(sJob.pabyBuffer, cc));
        memcpy(sJob.pabyBuffer, pabyData, cc);
        sJob.nBufferSize = cc;
        sJob.nHeight = nHeight;
        sJob.nStripOrTile = nStripOrTile;
        sJob.nPredictor = PREDICTOR_NONE;
        if (GTIFFSupportsPredictor(m_nCompression))
        {
            TIFFGetField(m_hTIFF, TIFFTAG_PREDICTOR, &sJob.nPredictor);
        }

        sJob.pExtraSamples = nullptr;
        sJob.nExtraSampleCount = 0;
        TIFFGetField(m_hTIFF, TIFFTAG_EXTRASAMPLES, &sJob.nExtraSampleCount,
                     &sJob.pExtraSamples);
    };

    if (poQueue == nullptr || !(m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
                                m_nCompression == COMPRESSION_LZW ||
                                m_nCompression == COMPRESSION_PACKBITS ||
                                m_nCompression == COMPRESSION_LZMA ||
                                m_nCompression == COMPRESSION_ZSTD ||
                                m_nCompression == COMPRESSION_LERC ||
                                m_nCompression == COMPRESSION_JXL ||
                                m_nCompression == COMPRESSION_WEBP ||
                                m_nCompression == COMPRESSION_JPEG))
    {
        if (m_bBlockOrderRowMajor || m_bLeaderSizeAsUInt4 ||
            m_bTrailerRepeatedLast4BytesRepeated)
        {
            GTiffCompressionJob sJob;
            memset(&sJob, 0, sizeof(sJob));
            SetupJob(sJob);
            sJob.pszTmpFilename =
                CPLStrdup(CPLSPrintf("/vsimem/gtiff/%p", this));

            ThreadCompressionFunc(&sJob);

            if (sJob.nCompressedBufferSize)
            {
                sJob.poDS->WriteRawStripOrTile(sJob.nStripOrTile,
                                               sJob.pabyCompressedBuffer,
                                               sJob.nCompressedBufferSize);
            }

            CPLFree(sJob.pabyBuffer);
            VSIUnlink(sJob.pszTmpFilename);
            CPLFree(sJob.pszTmpFilename);
            return sJob.nCompressedBufferSize > 0 && !m_bWriteError;
        }

        return false;
    }

    auto &oQueue = m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
    auto &asJobs =
        m_poBaseDS ? m_poBaseDS->m_asCompressionJobs : m_asCompressionJobs;

    int nNextCompressionJobAvail = -1;

    if (oQueue.size() == asJobs.size())
    {
        CPLAssert(!oQueue.empty());
        nNextCompressionJobAvail = oQueue.front();
        WaitCompletionForJobIdx(nNextCompressionJobAvail);
    }
    else
    {
        const int nJobs = static_cast<int>(asJobs.size());
        for (int i = 0; i < nJobs; ++i)
        {
            if (asJobs[i].nBufferSize == 0)
            {
                nNextCompressionJobAvail = i;
                break;
            }
        }
    }
    CPLAssert(nNextCompressionJobAvail >= 0);

    GTiffCompressionJob *psJob = &asJobs[nNextCompressionJobAvail];
    SetupJob(*psJob);
    poQueue->SubmitJob(ThreadCompressionFunc, psJob);
    oQueue.push(nNextCompressionJobAvail);

    return true;
}

/************************************************************************/
/*                          DiscardLsb()                                */
/************************************************************************/

template <class T> bool MustNotDiscardLsb(T value, bool bHasNoData, T nodata)
{
    return bHasNoData && value == nodata;
}

template <>
bool MustNotDiscardLsb<float>(float value, bool bHasNoData, float nodata)
{
    return (bHasNoData && value == nodata) || !std::isfinite(value);
}

template <>
bool MustNotDiscardLsb<double>(double value, bool bHasNoData, double nodata)
{
    return (bHasNoData && value == nodata) || !std::isfinite(value);
}

template <class T> T AdjustValue(T value, uint64_t nRoundUpBitTest);

template <class T> T AdjustValueInt(T value, uint64_t nRoundUpBitTest)
{
    if (value >=
        static_cast<T>(std::numeric_limits<T>::max() - (nRoundUpBitTest << 1)))
        return static_cast<T>(value - (nRoundUpBitTest << 1));
    return static_cast<T>(value + (nRoundUpBitTest << 1));
}

template <> int8_t AdjustValue<int8_t>(int8_t value, uint64_t nRoundUpBitTest)
{
    return AdjustValueInt(value, nRoundUpBitTest);
}

template <>
uint8_t AdjustValue<uint8_t>(uint8_t value, uint64_t nRoundUpBitTest)
{
    return AdjustValueInt(value, nRoundUpBitTest);
}

template <>
int16_t AdjustValue<int16_t>(int16_t value, uint64_t nRoundUpBitTest)
{
    return AdjustValueInt(value, nRoundUpBitTest);
}

template <>
uint16_t AdjustValue<uint16_t>(uint16_t value, uint64_t nRoundUpBitTest)
{
    return AdjustValueInt(value, nRoundUpBitTest);
}

template <>
int32_t AdjustValue<int32_t>(int32_t value, uint64_t nRoundUpBitTest)
{
    return AdjustValueInt(value, nRoundUpBitTest);
}

template <>
uint32_t AdjustValue<uint32_t>(uint32_t value, uint64_t nRoundUpBitTest)
{
    return AdjustValueInt(value, nRoundUpBitTest);
}

template <>
int64_t AdjustValue<int64_t>(int64_t value, uint64_t nRoundUpBitTest)
{
    return AdjustValueInt(value, nRoundUpBitTest);
}

template <>
uint64_t AdjustValue<uint64_t>(uint64_t value, uint64_t nRoundUpBitTest)
{
    return AdjustValueInt(value, nRoundUpBitTest);
}

template <> float AdjustValue<float>(float value, uint64_t)
{
    return std::nextafter(value, std::numeric_limits<float>::max());
}

template <> double AdjustValue<double>(double value, uint64_t)
{
    return std::nextafter(value, std::numeric_limits<double>::max());
}

template <class Teffective, class T>
T RoundValueDiscardLsb(const void *ptr, uint64_t nMask,
                       uint64_t nRoundUpBitTest);

template <class T>
T RoundValueDiscardLsbUnsigned(const void *ptr, uint64_t nMask,
                               uint64_t nRoundUpBitTest)
{
    if ((*reinterpret_cast<const T *>(ptr) & nMask) >
        static_cast<uint64_t>(std::numeric_limits<T>::max()) -
            (nRoundUpBitTest << 1U))
    {
        return static_cast<T>(std::numeric_limits<T>::max() & nMask);
    }
    const uint64_t newval =
        (*reinterpret_cast<const T *>(ptr) & nMask) + (nRoundUpBitTest << 1U);
    return static_cast<T>(newval);
}

template <class T>
T RoundValueDiscardLsbSigned(const void *ptr, uint64_t nMask,
                             uint64_t nRoundUpBitTest)
{
    T oldval = *reinterpret_cast<const T *>(ptr);
    if (oldval < 0)
    {
        return static_cast<T>(oldval & nMask);
    }
    const uint64_t newval =
        (*reinterpret_cast<const T *>(ptr) & nMask) + (nRoundUpBitTest << 1U);
    if (newval > static_cast<uint64_t>(std::numeric_limits<T>::max()))
        return static_cast<T>(std::numeric_limits<T>::max() & nMask);
    return static_cast<T>(newval);
}

template <>
uint16_t RoundValueDiscardLsb<uint16_t, uint16_t>(const void *ptr,
                                                  uint64_t nMask,
                                                  uint64_t nRoundUpBitTest)
{
    return RoundValueDiscardLsbUnsigned<uint16_t>(ptr, nMask, nRoundUpBitTest);
}

template <>
uint32_t RoundValueDiscardLsb<uint32_t, uint32_t>(const void *ptr,
                                                  uint64_t nMask,
                                                  uint64_t nRoundUpBitTest)
{
    return RoundValueDiscardLsbUnsigned<uint32_t>(ptr, nMask, nRoundUpBitTest);
}

template <>
uint64_t RoundValueDiscardLsb<uint64_t, uint64_t>(const void *ptr,
                                                  uint64_t nMask,
                                                  uint64_t nRoundUpBitTest)
{
    return RoundValueDiscardLsbUnsigned<uint64_t>(ptr, nMask, nRoundUpBitTest);
}

template <>
int8_t RoundValueDiscardLsb<int8_t, int8_t>(const void *ptr, uint64_t nMask,
                                            uint64_t nRoundUpBitTest)
{
    return RoundValueDiscardLsbSigned<int8_t>(ptr, nMask, nRoundUpBitTest);
}

template <>
int16_t RoundValueDiscardLsb<int16_t, int16_t>(const void *ptr, uint64_t nMask,
                                               uint64_t nRoundUpBitTest)
{
    return RoundValueDiscardLsbSigned<int16_t>(ptr, nMask, nRoundUpBitTest);
}

template <>
int32_t RoundValueDiscardLsb<int32_t, int32_t>(const void *ptr, uint64_t nMask,
                                               uint64_t nRoundUpBitTest)
{
    return RoundValueDiscardLsbSigned<int32_t>(ptr, nMask, nRoundUpBitTest);
}

template <>
int64_t RoundValueDiscardLsb<int64_t, int64_t>(const void *ptr, uint64_t nMask,
                                               uint64_t nRoundUpBitTest)
{
    return RoundValueDiscardLsbSigned<int64_t>(ptr, nMask, nRoundUpBitTest);
}

template <>
uint32_t RoundValueDiscardLsb<float, uint32_t>(const void *ptr, uint64_t nMask,
                                               uint64_t nRoundUpBitTest)
{
    return RoundValueDiscardLsbUnsigned<uint32_t>(ptr, nMask, nRoundUpBitTest);
}

template <>
uint64_t RoundValueDiscardLsb<double, uint64_t>(const void *ptr, uint64_t nMask,
                                                uint64_t nRoundUpBitTest)
{
    return RoundValueDiscardLsbUnsigned<uint64_t>(ptr, nMask, nRoundUpBitTest);
}

template <class Teffective, class T>
static void DiscardLsbT(GByte *pabyBuffer, size_t nBytes, int iBand, int nBands,
                        uint16_t nPlanarConfig,
                        const GTiffDataset::MaskOffset *panMaskOffsetLsb,
                        bool bHasNoData, Teffective nNoDataValue)
{
    static_assert(sizeof(Teffective) == sizeof(T),
                  "sizeof(Teffective) == sizeof(T)");
    if (nPlanarConfig == PLANARCONFIG_SEPARATE)
    {
        const auto nMask = panMaskOffsetLsb[iBand].nMask;
        const auto nRoundUpBitTest = panMaskOffsetLsb[iBand].nRoundUpBitTest;
        for (size_t i = 0; i < nBytes / sizeof(T); ++i)
        {
            if (MustNotDiscardLsb(reinterpret_cast<Teffective *>(pabyBuffer)[i],
                                  bHasNoData, nNoDataValue))
            {
                continue;
            }

            if (reinterpret_cast<T *>(pabyBuffer)[i] & nRoundUpBitTest)
            {
                reinterpret_cast<T *>(pabyBuffer)[i] =
                    RoundValueDiscardLsb<Teffective, T>(
                        &(reinterpret_cast<T *>(pabyBuffer)[i]), nMask,
                        nRoundUpBitTest);
            }
            else
            {
                reinterpret_cast<T *>(pabyBuffer)[i] = static_cast<T>(
                    reinterpret_cast<T *>(pabyBuffer)[i] & nMask);
            }

            // Make sure that by discarding LSB we don't end up to a value
            // that is no the nodata value
            if (MustNotDiscardLsb(reinterpret_cast<Teffective *>(pabyBuffer)[i],
                                  bHasNoData, nNoDataValue))
            {
                reinterpret_cast<Teffective *>(pabyBuffer)[i] =
                    AdjustValue(nNoDataValue, nRoundUpBitTest);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < nBytes / sizeof(T); i += nBands)
        {
            for (int j = 0; j < nBands; ++j)
            {
                if (MustNotDiscardLsb(
                        reinterpret_cast<Teffective *>(pabyBuffer)[i + j],
                        bHasNoData, nNoDataValue))
                {
                    continue;
                }

                if (reinterpret_cast<T *>(pabyBuffer)[i + j] &
                    panMaskOffsetLsb[j].nRoundUpBitTest)
                {
                    reinterpret_cast<T *>(pabyBuffer)[i + j] =
                        RoundValueDiscardLsb<Teffective, T>(
                            &(reinterpret_cast<T *>(pabyBuffer)[i + j]),
                            panMaskOffsetLsb[j].nMask,
                            panMaskOffsetLsb[j].nRoundUpBitTest);
                }
                else
                {
                    reinterpret_cast<T *>(pabyBuffer)[i + j] = static_cast<T>(
                        (reinterpret_cast<T *>(pabyBuffer)[i + j] &
                         panMaskOffsetLsb[j].nMask));
                }

                // Make sure that by discarding LSB we don't end up to a value
                // that is no the nodata value
                if (MustNotDiscardLsb(
                        reinterpret_cast<Teffective *>(pabyBuffer)[i + j],
                        bHasNoData, nNoDataValue))
                {
                    reinterpret_cast<Teffective *>(pabyBuffer)[i + j] =
                        AdjustValue(nNoDataValue,
                                    panMaskOffsetLsb[j].nRoundUpBitTest);
                }
            }
        }
    }
}

static void DiscardLsb(GByte *pabyBuffer, GPtrDiff_t nBytes, int iBand,
                       int nBands, uint16_t nSampleFormat,
                       uint16_t nBitsPerSample, uint16_t nPlanarConfig,
                       const GTiffDataset::MaskOffset *panMaskOffsetLsb,
                       bool bHasNoData, double dfNoDataValue)
{
    if (nBitsPerSample == 8 && nSampleFormat == SAMPLEFORMAT_UINT)
    {
        uint8_t nNoDataValue = 0;
        if (bHasNoData && GDALIsValueExactAs<uint8_t>(dfNoDataValue))
        {
            nNoDataValue = static_cast<uint8_t>(dfNoDataValue);
        }
        else
        {
            bHasNoData = false;
        }
        if (nPlanarConfig == PLANARCONFIG_SEPARATE)
        {
            const auto nMask =
                static_cast<unsigned>(panMaskOffsetLsb[iBand].nMask);
            const auto nRoundUpBitTest =
                static_cast<unsigned>(panMaskOffsetLsb[iBand].nRoundUpBitTest);
            for (decltype(nBytes) i = 0; i < nBytes; ++i)
            {
                if (bHasNoData && pabyBuffer[i] == nNoDataValue)
                    continue;

                // Keep 255 in case it is alpha.
                if (pabyBuffer[i] != 255)
                {
                    if (pabyBuffer[i] & nRoundUpBitTest)
                        pabyBuffer[i] = static_cast<GByte>(
                            std::min(255U, (pabyBuffer[i] & nMask) +
                                               (nRoundUpBitTest << 1U)));
                    else
                        pabyBuffer[i] =
                            static_cast<GByte>(pabyBuffer[i] & nMask);

                    // Make sure that by discarding LSB we don't end up to a
                    // value that is no the nodata value
                    if (bHasNoData && pabyBuffer[i] == nNoDataValue)
                        pabyBuffer[i] =
                            AdjustValue(nNoDataValue, nRoundUpBitTest);
                }
            }
        }
        else
        {
            for (decltype(nBytes) i = 0; i < nBytes; i += nBands)
            {
                for (int j = 0; j < nBands; ++j)
                {
                    if (bHasNoData && pabyBuffer[i + j] == nNoDataValue)
                        continue;

                    // Keep 255 in case it is alpha.
                    if (pabyBuffer[i + j] != 255)
                    {
                        if (pabyBuffer[i + j] &
                            panMaskOffsetLsb[j].nRoundUpBitTest)
                        {
                            pabyBuffer[i + j] = static_cast<GByte>(std::min(
                                255U,
                                (pabyBuffer[i + j] &
                                 static_cast<unsigned>(
                                     panMaskOffsetLsb[j].nMask)) +
                                    (static_cast<unsigned>(
                                         panMaskOffsetLsb[j].nRoundUpBitTest)
                                     << 1U)));
                        }
                        else
                        {
                            pabyBuffer[i + j] = static_cast<GByte>(
                                pabyBuffer[i + j] & panMaskOffsetLsb[j].nMask);
                        }

                        // Make sure that by discarding LSB we don't end up to a
                        // value that is no the nodata value
                        if (bHasNoData && pabyBuffer[i + j] == nNoDataValue)
                            pabyBuffer[i + j] = AdjustValue(
                                nNoDataValue,
                                panMaskOffsetLsb[j].nRoundUpBitTest);
                    }
                }
            }
        }
    }
    else if (nBitsPerSample == 8 && nSampleFormat == SAMPLEFORMAT_INT)
    {
        int8_t nNoDataValue = 0;
        if (bHasNoData && GDALIsValueExactAs<int8_t>(dfNoDataValue))
        {
            nNoDataValue = static_cast<int8_t>(dfNoDataValue);
        }
        else
        {
            bHasNoData = false;
        }
        DiscardLsbT<int8_t, int8_t>(pabyBuffer, nBytes, iBand, nBands,
                                    nPlanarConfig, panMaskOffsetLsb, bHasNoData,
                                    nNoDataValue);
    }
    else if (nBitsPerSample == 16 && nSampleFormat == SAMPLEFORMAT_INT)
    {
        int16_t nNoDataValue = 0;
        if (bHasNoData && GDALIsValueExactAs<int16_t>(dfNoDataValue))
        {
            nNoDataValue = static_cast<int16_t>(dfNoDataValue);
        }
        else
        {
            bHasNoData = false;
        }
        DiscardLsbT<int16_t, int16_t>(pabyBuffer, nBytes, iBand, nBands,
                                      nPlanarConfig, panMaskOffsetLsb,
                                      bHasNoData, nNoDataValue);
    }
    else if (nBitsPerSample == 16 && nSampleFormat == SAMPLEFORMAT_UINT)
    {
        uint16_t nNoDataValue = 0;
        if (bHasNoData && GDALIsValueExactAs<uint16_t>(dfNoDataValue))
        {
            nNoDataValue = static_cast<uint16_t>(dfNoDataValue);
        }
        else
        {
            bHasNoData = false;
        }
        DiscardLsbT<uint16_t, uint16_t>(pabyBuffer, nBytes, iBand, nBands,
                                        nPlanarConfig, panMaskOffsetLsb,
                                        bHasNoData, nNoDataValue);
    }
    else if (nBitsPerSample == 32 && nSampleFormat == SAMPLEFORMAT_INT)
    {
        int32_t nNoDataValue = 0;
        if (bHasNoData && GDALIsValueExactAs<int32_t>(dfNoDataValue))
        {
            nNoDataValue = static_cast<int32_t>(dfNoDataValue);
        }
        else
        {
            bHasNoData = false;
        }
        DiscardLsbT<int32_t, int32_t>(pabyBuffer, nBytes, iBand, nBands,
                                      nPlanarConfig, panMaskOffsetLsb,
                                      bHasNoData, nNoDataValue);
    }
    else if (nBitsPerSample == 32 && nSampleFormat == SAMPLEFORMAT_UINT)
    {
        uint32_t nNoDataValue = 0;
        if (bHasNoData && GDALIsValueExactAs<uint32_t>(dfNoDataValue))
        {
            nNoDataValue = static_cast<uint32_t>(dfNoDataValue);
        }
        else
        {
            bHasNoData = false;
        }
        DiscardLsbT<uint32_t, uint32_t>(pabyBuffer, nBytes, iBand, nBands,
                                        nPlanarConfig, panMaskOffsetLsb,
                                        bHasNoData, nNoDataValue);
    }
    else if (nBitsPerSample == 64 && nSampleFormat == SAMPLEFORMAT_INT)
    {
        // FIXME: we should not rely on dfNoDataValue when we support native
        // data type for nodata
        int64_t nNoDataValue = 0;
        if (bHasNoData && GDALIsValueExactAs<int64_t>(dfNoDataValue))
        {
            nNoDataValue = static_cast<int64_t>(dfNoDataValue);
        }
        else
        {
            bHasNoData = false;
        }
        DiscardLsbT<int64_t, int64_t>(pabyBuffer, nBytes, iBand, nBands,
                                      nPlanarConfig, panMaskOffsetLsb,
                                      bHasNoData, nNoDataValue);
    }
    else if (nBitsPerSample == 64 && nSampleFormat == SAMPLEFORMAT_UINT)
    {
        // FIXME: we should not rely on dfNoDataValue when we support native
        // data type for nodata
        uint64_t nNoDataValue = 0;
        if (bHasNoData && GDALIsValueExactAs<uint64_t>(dfNoDataValue))
        {
            nNoDataValue = static_cast<uint64_t>(dfNoDataValue);
        }
        else
        {
            bHasNoData = false;
        }
        DiscardLsbT<uint64_t, uint64_t>(pabyBuffer, nBytes, iBand, nBands,
                                        nPlanarConfig, panMaskOffsetLsb,
                                        bHasNoData, nNoDataValue);
    }
    else if (nBitsPerSample == 32 && nSampleFormat == SAMPLEFORMAT_IEEEFP)
    {
        float fNoDataValue = static_cast<float>(dfNoDataValue);
        DiscardLsbT<float, uint32_t>(pabyBuffer, nBytes, iBand, nBands,
                                     nPlanarConfig, panMaskOffsetLsb,
                                     bHasNoData, fNoDataValue);
    }
    else if (nBitsPerSample == 64 && nSampleFormat == SAMPLEFORMAT_IEEEFP)
    {
        DiscardLsbT<double, uint64_t>(pabyBuffer, nBytes, iBand, nBands,
                                      nPlanarConfig, panMaskOffsetLsb,
                                      bHasNoData, dfNoDataValue);
    }
}

void GTiffDataset::DiscardLsb(GByte *pabyBuffer, GPtrDiff_t nBytes,
                              int iBand) const
{
    ::DiscardLsb(pabyBuffer, nBytes, iBand, nBands, m_nSampleFormat,
                 m_nBitsPerSample, m_nPlanarConfig, m_panMaskOffsetLsb,
                 m_bNoDataSet, m_dfNoDataValue);
}

/************************************************************************/
/*                  WriteEncodedTileOrStrip()                           */
/************************************************************************/

CPLErr GTiffDataset::WriteEncodedTileOrStrip(uint32_t tile_or_strip, void *data,
                                             int bPreserveDataBuffer)
{
    CPLErr eErr = CE_None;

    if (TIFFIsTiled(m_hTIFF))
    {
        if (!(WriteEncodedTile(tile_or_strip, static_cast<GByte *>(data),
                               bPreserveDataBuffer)))
        {
            eErr = CE_Failure;
        }
    }
    else
    {
        if (!(WriteEncodedStrip(tile_or_strip, static_cast<GByte *>(data),
                                bPreserveDataBuffer)))
        {
            eErr = CE_Failure;
        }
    }

    return eErr;
}

/************************************************************************/
/*                           FlushBlockBuf()                            */
/************************************************************************/

CPLErr GTiffDataset::FlushBlockBuf()

{
    if (m_nLoadedBlock < 0 || !m_bLoadedBlockDirty)
        return CE_None;

    m_bLoadedBlockDirty = false;

    const CPLErr eErr =
        WriteEncodedTileOrStrip(m_nLoadedBlock, m_pabyBlockBuf, true);
    if (eErr != CE_None)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "WriteEncodedTile/Strip() failed.");
        m_bWriteError = true;
    }

    return eErr;
}

/************************************************************************/
/*                   GTiffFillStreamableOffsetAndCount()                */
/************************************************************************/

static void GTiffFillStreamableOffsetAndCount(TIFF *hTIFF, int nSize)
{
    uint32_t nXSize = 0;
    uint32_t nYSize = 0;
    TIFFGetField(hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize);
    TIFFGetField(hTIFF, TIFFTAG_IMAGELENGTH, &nYSize);
    const bool bIsTiled = CPL_TO_BOOL(TIFFIsTiled(hTIFF));
    const int nBlockCount =
        bIsTiled ? TIFFNumberOfTiles(hTIFF) : TIFFNumberOfStrips(hTIFF);

    toff_t *panOffset = nullptr;
    TIFFGetField(hTIFF, bIsTiled ? TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS,
                 &panOffset);
    toff_t *panSize = nullptr;
    TIFFGetField(hTIFF,
                 bIsTiled ? TIFFTAG_TILEBYTECOUNTS : TIFFTAG_STRIPBYTECOUNTS,
                 &panSize);
    toff_t nOffset = nSize;
    // Trick to avoid clang static analyzer raising false positive about
    // divide by zero later.
    int nBlocksPerBand = 1;
    uint32_t nRowsPerStrip = 0;
    if (!bIsTiled)
    {
        TIFFGetField(hTIFF, TIFFTAG_ROWSPERSTRIP, &nRowsPerStrip);
        if (nRowsPerStrip > static_cast<uint32_t>(nYSize))
            nRowsPerStrip = nYSize;
        nBlocksPerBand = DIV_ROUND_UP(nYSize, nRowsPerStrip);
    }
    for (int i = 0; i < nBlockCount; ++i)
    {
        GPtrDiff_t cc = bIsTiled
                            ? static_cast<GPtrDiff_t>(TIFFTileSize(hTIFF))
                            : static_cast<GPtrDiff_t>(TIFFStripSize(hTIFF));
        if (!bIsTiled)
        {
            /* --------------------------------------------------------------------
             */
            /*      If this is the last strip in the image, and is partial, then
             */
            /*      we need to trim the number of scanlines written to the */
            /*      amount of valid data we have. (#2748) */
            /* --------------------------------------------------------------------
             */
            int nStripWithinBand = i % nBlocksPerBand;
            if (nStripWithinBand * nRowsPerStrip > nYSize - nRowsPerStrip)
            {
                cc = (cc / nRowsPerStrip) *
                     (nYSize - nStripWithinBand * nRowsPerStrip);
            }
        }
        panOffset[i] = nOffset;
        panSize[i] = cc;
        nOffset += cc;
    }
}

/************************************************************************/
/*                             Crystalize()                             */
/*                                                                      */
/*      Make sure that the directory information is written out for     */
/*      a new file, require before writing any imagery data.            */
/************************************************************************/

void GTiffDataset::Crystalize()

{
    if (m_bCrystalized)
        return;

    // TODO: libtiff writes extended tags in the order they are specified
    // and not in increasing order.
    WriteMetadata(this, m_hTIFF, true, m_eProfile, m_pszFilename,
                  m_papszCreationOptions);
    WriteGeoTIFFInfo();
    if (m_bNoDataSet)
        WriteNoDataValue(m_hTIFF, m_dfNoDataValue);
    else if (m_bNoDataSetAsInt64)
        WriteNoDataValue(m_hTIFF, m_nNoDataValueInt64);
    else if (m_bNoDataSetAsUInt64)
        WriteNoDataValue(m_hTIFF, m_nNoDataValueUInt64);

    m_bMetadataChanged = false;
    m_bGeoTIFFInfoChanged = false;
    m_bNoDataChanged = false;
    m_bNeedsRewrite = false;

    m_bCrystalized = true;

    TIFFWriteCheck(m_hTIFF, TIFFIsTiled(m_hTIFF), "GTiffDataset::Crystalize");

    TIFFWriteDirectory(m_hTIFF);
    if (m_bStreamingOut)
    {
        // We need to write twice the directory to be sure that custom
        // TIFF tags are correctly sorted and that padding bytes have been
        // added.
        TIFFSetDirectory(m_hTIFF, 0);
        TIFFWriteDirectory(m_hTIFF);

        if (VSIFSeekL(m_fpL, 0, SEEK_END) != 0)
        {
            ReportError(CE_Failure, CPLE_FileIO, "Could not seek");
        }
        const int nSize = static_cast<int>(VSIFTellL(m_fpL));

        TIFFSetDirectory(m_hTIFF, 0);
        GTiffFillStreamableOffsetAndCount(m_hTIFF, nSize);
        TIFFWriteDirectory(m_hTIFF);

        vsi_l_offset nDataLength = 0;
        void *pabyBuffer =
            VSIGetMemFileBuffer(m_pszTmpFilename, &nDataLength, FALSE);
        if (static_cast<int>(VSIFWriteL(
                pabyBuffer, 1, static_cast<int>(nDataLength), m_fpToWrite)) !=
            static_cast<int>(nDataLength))
        {
            ReportError(CE_Failure, CPLE_FileIO, "Could not write %d bytes",
                        static_cast<int>(nDataLength));
        }
        // In case of single strip file, there's a libtiff check that would
        // issue a warning since the file hasn't the required size.
        CPLPushErrorHandler(CPLQuietErrorHandler);
        TIFFSetDirectory(m_hTIFF, 0);
        CPLPopErrorHandler();
    }
    else
    {
        TIFFSetDirectory(
            m_hTIFF, static_cast<tdir_t>(TIFFNumberOfDirectories(m_hTIFF) - 1));
    }

    RestoreVolatileParameters(m_hTIFF);

    m_nDirOffset = TIFFCurrentDirOffset(m_hTIFF);
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we can also flush out local tiff strip      */
/*      cache if need be.                                               */
/************************************************************************/

CPLErr GTiffDataset::FlushCache(bool bAtClosing)

{
    return FlushCacheInternal(bAtClosing, true);
}

CPLErr GTiffDataset::FlushCacheInternal(bool bAtClosing, bool bFlushDirectory)
{
    if (m_bIsFinalized)
        return CE_None;

    CPLErr eErr = GDALPamDataset::FlushCache(bAtClosing);

    if (m_bLoadedBlockDirty && m_nLoadedBlock != -1)
    {
        if (FlushBlockBuf() != CE_None)
            eErr = CE_Failure;
    }

    CPLFree(m_pabyBlockBuf);
    m_pabyBlockBuf = nullptr;
    m_nLoadedBlock = -1;
    m_bLoadedBlockDirty = false;

    // Finish compression
    auto poQueue = m_poBaseDS ? m_poBaseDS->m_poCompressQueue.get()
                              : m_poCompressQueue.get();
    if (poQueue)
    {
        poQueue->WaitCompletion();

        // Flush remaining data
        // cppcheck-suppress constVariableReference

        auto &oQueue =
            m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
        while (!oQueue.empty())
        {
            WaitCompletionForJobIdx(oQueue.front());
        }
    }

    if (bFlushDirectory && GetAccess() == GA_Update)
    {
        if (FlushDirectory() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                           FlushDirectory()                           */
/************************************************************************/

CPLErr GTiffDataset::FlushDirectory()

{
    CPLErr eErr = CE_None;

    const auto ReloadAllOtherDirectories = [this]()
    {
        const auto poBaseDS = m_poBaseDS ? m_poBaseDS : this;
        if (poBaseDS->m_papoOverviewDS)
        {
            for (int i = 0; i < poBaseDS->m_nOverviewCount; ++i)
            {
                if (poBaseDS->m_papoOverviewDS[i]->m_bCrystalized &&
                    poBaseDS->m_papoOverviewDS[i] != this)
                {
                    poBaseDS->m_papoOverviewDS[i]->ReloadDirectory(true);
                }

                if (poBaseDS->m_papoOverviewDS[i]->m_poMaskDS &&
                    poBaseDS->m_papoOverviewDS[i]->m_poMaskDS != this &&
                    poBaseDS->m_papoOverviewDS[i]->m_poMaskDS->m_bCrystalized)
                {
                    poBaseDS->m_papoOverviewDS[i]->m_poMaskDS->ReloadDirectory(
                        true);
                }
            }
        }
        if (poBaseDS->m_poMaskDS && poBaseDS->m_poMaskDS != this &&
            poBaseDS->m_poMaskDS->m_bCrystalized)
        {
            poBaseDS->m_poMaskDS->ReloadDirectory(true);
        }
        if (poBaseDS->m_bCrystalized && poBaseDS != this)
        {
            poBaseDS->ReloadDirectory(true);
        }
    };

    if (eAccess == GA_Update)
    {
        if (m_bMetadataChanged)
        {
            m_bNeedsRewrite =
                WriteMetadata(this, m_hTIFF, true, m_eProfile, m_pszFilename,
                              m_papszCreationOptions);
            m_bMetadataChanged = false;

            if (m_bForceUnsetRPC)
            {
                double *padfRPCTag = nullptr;
                uint16_t nCount;
                if (TIFFGetField(m_hTIFF, TIFFTAG_RPCCOEFFICIENT, &nCount,
                                 &padfRPCTag))
                {
                    std::vector<double> zeroes(92);
                    TIFFSetField(m_hTIFF, TIFFTAG_RPCCOEFFICIENT, 92,
                                 zeroes.data());
                    TIFFUnsetField(m_hTIFF, TIFFTAG_RPCCOEFFICIENT);
                    m_bNeedsRewrite = true;
                }

                GDALWriteRPCTXTFile(m_pszFilename, nullptr);
                GDALWriteRPBFile(m_pszFilename, nullptr);
            }
        }

        if (m_bGeoTIFFInfoChanged)
        {
            WriteGeoTIFFInfo();
            m_bGeoTIFFInfoChanged = false;
        }

        if (m_bNoDataChanged)
        {
            if (m_bNoDataSet)
            {
                WriteNoDataValue(m_hTIFF, m_dfNoDataValue);
            }
            else if (m_bNoDataSetAsInt64)
            {
                WriteNoDataValue(m_hTIFF, m_nNoDataValueInt64);
            }
            else if (m_bNoDataSetAsUInt64)
            {
                WriteNoDataValue(m_hTIFF, m_nNoDataValueUInt64);
            }
            else
            {
                UnsetNoDataValue(m_hTIFF);
            }
            m_bNeedsRewrite = true;
            m_bNoDataChanged = false;
        }

        if (m_bNeedsRewrite)
        {
            if (!m_bCrystalized)
            {
                Crystalize();
            }
            else
            {
                const TIFFSizeProc pfnSizeProc = TIFFGetSizeProc(m_hTIFF);

                m_nDirOffset = pfnSizeProc(TIFFClientdata(m_hTIFF));
                if ((m_nDirOffset % 2) == 1)
                    ++m_nDirOffset;

                if (TIFFRewriteDirectory(m_hTIFF) == 0)
                    eErr = CE_Failure;

                TIFFSetSubDirectory(m_hTIFF, m_nDirOffset);

                ReloadAllOtherDirectories();

                if (m_bLayoutIFDSBeforeData && m_bBlockOrderRowMajor &&
                    m_bLeaderSizeAsUInt4 &&
                    m_bTrailerRepeatedLast4BytesRepeated &&
                    !m_bKnownIncompatibleEdition &&
                    !m_bWriteKnownIncompatibleEdition)
                {
                    ReportError(CE_Warning, CPLE_AppDefined,
                                "The IFD has been rewritten at the end of "
                                "the file, which breaks COG layout.");
                    m_bKnownIncompatibleEdition = true;
                    m_bWriteKnownIncompatibleEdition = true;
                }
            }

            m_bNeedsRewrite = false;
        }
    }

    // There are some circumstances in which we can reach this point
    // without having made this our directory (SetDirectory()) in which
    // case we should not risk a flush.
    if (GetAccess() == GA_Update &&
        TIFFCurrentDirOffset(m_hTIFF) == m_nDirOffset)
    {
        const TIFFSizeProc pfnSizeProc = TIFFGetSizeProc(m_hTIFF);

        toff_t nNewDirOffset = pfnSizeProc(TIFFClientdata(m_hTIFF));
        if ((nNewDirOffset % 2) == 1)
            ++nNewDirOffset;

        if (TIFFFlush(m_hTIFF) == 0)
            eErr = CE_Failure;

        if (m_nDirOffset != TIFFCurrentDirOffset(m_hTIFF))
        {
            m_nDirOffset = nNewDirOffset;
            ReloadAllOtherDirectories();
            CPLDebug("GTiff",
                     "directory moved during flush in FlushDirectory()");
        }
    }

    SetDirectory();
    return eErr;
}

/************************************************************************/
/*                           CleanOverviews()                           */
/************************************************************************/

CPLErr GTiffDataset::CleanOverviews()

{
    CPLAssert(!m_poBaseDS);

    ScanDirectories();

    FlushDirectory();

    /* -------------------------------------------------------------------- */
    /*      Cleanup overviews objects, and get offsets to all overview      */
    /*      directories.                                                    */
    /* -------------------------------------------------------------------- */
    std::vector<toff_t> anOvDirOffsets;

    for (int i = 0; i < m_nOverviewCount; ++i)
    {
        anOvDirOffsets.push_back(m_papoOverviewDS[i]->m_nDirOffset);
        if (m_papoOverviewDS[i]->m_poMaskDS)
            anOvDirOffsets.push_back(
                m_papoOverviewDS[i]->m_poMaskDS->m_nDirOffset);
        delete m_papoOverviewDS[i];
    }

    /* -------------------------------------------------------------------- */
    /*      Loop through all the directories, translating the offsets       */
    /*      into indexes we can use with TIFFUnlinkDirectory().             */
    /* -------------------------------------------------------------------- */
    std::vector<uint16_t> anOvDirIndexes;
    int iThisOffset = 1;

    TIFFSetDirectory(m_hTIFF, 0);

    while (true)
    {
        for (toff_t nOffset : anOvDirOffsets)
        {
            if (nOffset == TIFFCurrentDirOffset(m_hTIFF))
            {
                anOvDirIndexes.push_back(static_cast<uint16_t>(iThisOffset));
            }
        }

        if (TIFFLastDirectory(m_hTIFF))
            break;

        TIFFReadDirectory(m_hTIFF);
        ++iThisOffset;
    }

    /* -------------------------------------------------------------------- */
    /*      Actually unlink the target directories.  Note that we do        */
    /*      this from last to first so as to avoid renumbering any of       */
    /*      the earlier directories we need to remove.                      */
    /* -------------------------------------------------------------------- */
    while (!anOvDirIndexes.empty())
    {
        TIFFUnlinkDirectory(m_hTIFF, anOvDirIndexes.back());
        anOvDirIndexes.pop_back();
    }

    CPLFree(m_papoOverviewDS);
    m_nOverviewCount = 0;
    m_papoOverviewDS = nullptr;

    if (m_poMaskDS)
    {
        CPLFree(m_poMaskDS->m_papoOverviewDS);
        m_poMaskDS->m_nOverviewCount = 0;
        m_poMaskDS->m_papoOverviewDS = nullptr;
    }

    if (!SetDirectory())
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                   RegisterNewOverviewDataset()                       */
/************************************************************************/

CPLErr GTiffDataset::RegisterNewOverviewDataset(toff_t nOverviewOffset,
                                                int l_nJpegQuality,
                                                CSLConstList papszOptions)
{
    if (m_nOverviewCount == 127)
        return CE_Failure;

    const auto GetOptionValue =
        [papszOptions](const char *pszOptionKey, const char *pszConfigOptionKey,
                       const char **ppszKeyUsed = nullptr)
    {
        const char *pszVal = CSLFetchNameValue(papszOptions, pszOptionKey);
        if (pszVal)
        {
            if (ppszKeyUsed)
                *ppszKeyUsed = pszOptionKey;
            return pszVal;
        }
        pszVal = CSLFetchNameValue(papszOptions, pszConfigOptionKey);
        if (pszVal)
        {
            if (ppszKeyUsed)
                *ppszKeyUsed = pszConfigOptionKey;
            return pszVal;
        }
        pszVal = CPLGetConfigOption(pszConfigOptionKey, nullptr);
        if (pszVal && ppszKeyUsed)
            *ppszKeyUsed = pszConfigOptionKey;
        return pszVal;
    };

    int nZLevel = m_nZLevel;
    if (const char *opt = GetOptionValue("ZLEVEL", "ZLEVEL_OVERVIEW"))
    {
        nZLevel = atoi(opt);
    }

    int nZSTDLevel = m_nZSTDLevel;
    if (const char *opt = GetOptionValue("ZSTD_LEVEL", "ZSTD_LEVEL_OVERVIEW"))
    {
        nZSTDLevel = atoi(opt);
    }

    bool bWebpLossless = m_bWebPLossless;
    const char *pszWebPLosslessOverview =
        GetOptionValue("WEBP_LOSSLESS", "WEBP_LOSSLESS_OVERVIEW");
    if (pszWebPLosslessOverview)
    {
        bWebpLossless = CPLTestBool(pszWebPLosslessOverview);
    }

    int nWebpLevel = m_nWebPLevel;
    const char *pszKeyWebpLevel = "";
    if (const char *opt = GetOptionValue("WEBP_LEVEL", "WEBP_LEVEL_OVERVIEW",
                                         &pszKeyWebpLevel))
    {
        if (pszWebPLosslessOverview == nullptr && m_bWebPLossless)
        {
            CPLDebug("GTiff",
                     "%s specified, but not WEBP_LOSSLESS_OVERVIEW. "
                     "Assuming WEBP_LOSSLESS_OVERVIEW=NO",
                     pszKeyWebpLevel);
            bWebpLossless = false;
        }
        else if (bWebpLossless)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s is specified, but WEBP_LOSSLESS_OVERVIEW=YES. "
                     "%s will be ignored.",
                     pszKeyWebpLevel, pszKeyWebpLevel);
        }
        nWebpLevel = atoi(opt);
    }

    double dfMaxZError = m_dfMaxZErrorOverview;
    if (const char *opt = GetOptionValue("MAX_Z_ERROR", "MAX_Z_ERROR_OVERVIEW"))
    {
        dfMaxZError = CPLAtof(opt);
    }

    GTiffDataset *poODS = new GTiffDataset();
    poODS->ShareLockWithParentDataset(this);
    poODS->m_pszFilename = CPLStrdup(m_pszFilename);
    const char *pszSparseOK = GetOptionValue("SPARSE_OK", "SPARSE_OK_OVERVIEW");
    if (pszSparseOK && CPLTestBool(pszSparseOK))
    {
        poODS->m_bWriteEmptyTiles = false;
        poODS->m_bFillEmptyTilesAtClosing = false;
    }
    else
    {
        poODS->m_bWriteEmptyTiles = m_bWriteEmptyTiles;
        poODS->m_bFillEmptyTilesAtClosing = m_bFillEmptyTilesAtClosing;
    }
    poODS->m_nJpegQuality = static_cast<signed char>(l_nJpegQuality);
    poODS->m_nWebPLevel = static_cast<signed char>(nWebpLevel);
    poODS->m_nZLevel = static_cast<signed char>(nZLevel);
    poODS->m_nLZMAPreset = m_nLZMAPreset;
    poODS->m_nZSTDLevel = static_cast<signed char>(nZSTDLevel);
    poODS->m_bWebPLossless = bWebpLossless;
    poODS->m_nJpegTablesMode = m_nJpegTablesMode;
    poODS->m_dfMaxZError = dfMaxZError;
    poODS->m_dfMaxZErrorOverview = dfMaxZError;
    memcpy(poODS->m_anLercAddCompressionAndVersion,
           m_anLercAddCompressionAndVersion,
           sizeof(m_anLercAddCompressionAndVersion));
#ifdef HAVE_JXL
    poODS->m_bJXLLossless = m_bJXLLossless;
    poODS->m_fJXLDistance = m_fJXLDistance;
    poODS->m_fJXLAlphaDistance = m_fJXLAlphaDistance;
    poODS->m_nJXLEffort = m_nJXLEffort;
#endif

    if (poODS->OpenOffset(VSI_TIFFOpenChild(m_hTIFF), nOverviewOffset,
                          GA_Update) != CE_None)
    {
        delete poODS;
        return CE_Failure;
    }

    // Assign color interpretation from main dataset
    const int l_nBands = GetRasterCount();
    for (int i = 1; i <= l_nBands; i++)
    {
        auto poBand = dynamic_cast<GTiffRasterBand *>(poODS->GetRasterBand(i));
        if (poBand)
            poBand->m_eBandInterp = GetRasterBand(i)->GetColorInterpretation();
    }

    // Do that now that m_nCompression is set
    poODS->RestoreVolatileParameters(poODS->m_hTIFF);

    ++m_nOverviewCount;
    m_papoOverviewDS = static_cast<GTiffDataset **>(
        CPLRealloc(m_papoOverviewDS, m_nOverviewCount * (sizeof(void *))));
    m_papoOverviewDS[m_nOverviewCount - 1] = poODS;
    poODS->m_poBaseDS = this;
    poODS->m_bIsOverview = true;
    return CE_None;
}

/************************************************************************/
/*                     CreateTIFFColorTable()                           */
/************************************************************************/

static void CreateTIFFColorTable(GDALColorTable *poColorTable, int nBits,
                                 std::vector<unsigned short> &anTRed,
                                 std::vector<unsigned short> &anTGreen,
                                 std::vector<unsigned short> &anTBlue,
                                 unsigned short *&panRed,
                                 unsigned short *&panGreen,
                                 unsigned short *&panBlue)
{
    int nColors;

    if (nBits == 8)
        nColors = 256;
    else if (nBits < 8)
        nColors = 1 << nBits;
    else
        nColors = 65536;

    anTRed.resize(nColors, 0);
    anTGreen.resize(nColors, 0);
    anTBlue.resize(nColors, 0);

    for (int iColor = 0; iColor < nColors; ++iColor)
    {
        if (iColor < poColorTable->GetColorEntryCount())
        {
            GDALColorEntry sRGB;

            poColorTable->GetColorEntryAsRGB(iColor, &sRGB);

            anTRed[iColor] = static_cast<unsigned short>(257 * sRGB.c1);
            anTGreen[iColor] = static_cast<unsigned short>(257 * sRGB.c2);
            anTBlue[iColor] = static_cast<unsigned short>(257 * sRGB.c3);
        }
        else
        {
            anTRed[iColor] = 0;
            anTGreen[iColor] = 0;
            anTBlue[iColor] = 0;
        }
    }

    panRed = &(anTRed[0]);
    panGreen = &(anTGreen[0]);
    panBlue = &(anTBlue[0]);
}

/************************************************************************/
/*                        GetOverviewParameters()                       */
/************************************************************************/

bool GTiffDataset::GetOverviewParameters(
    int &nCompression, uint16_t &nPlanarConfig, uint16_t &nPredictor,
    uint16_t &nPhotometric, int &nOvrJpegQuality, std::string &osNoData,
    uint16_t *&panExtraSampleValues, uint16_t &nExtraSamples,
    CSLConstList papszOptions) const
{
    const auto GetOptionValue =
        [papszOptions](const char *pszOptionKey, const char *pszConfigOptionKey,
                       const char **ppszKeyUsed = nullptr)
    {
        const char *pszVal = CSLFetchNameValue(papszOptions, pszOptionKey);
        if (pszVal)
        {
            if (ppszKeyUsed)
                *ppszKeyUsed = pszOptionKey;
            return pszVal;
        }
        pszVal = CSLFetchNameValue(papszOptions, pszConfigOptionKey);
        if (pszVal)
        {
            if (ppszKeyUsed)
                *ppszKeyUsed = pszConfigOptionKey;
            return pszVal;
        }
        pszVal = CPLGetConfigOption(pszConfigOptionKey, nullptr);
        if (pszVal && ppszKeyUsed)
            *ppszKeyUsed = pszConfigOptionKey;
        return pszVal;
    };

    /* -------------------------------------------------------------------- */
    /*      Determine compression method.                                   */
    /* -------------------------------------------------------------------- */
    nCompression = m_nCompression;
    const char *pszOptionKey = "";
    const char *pszCompressValue =
        GetOptionValue("COMPRESS", "COMPRESS_OVERVIEW", &pszOptionKey);
    if (pszCompressValue != nullptr)
    {
        nCompression =
            GTIFFGetCompressionMethod(pszCompressValue, pszOptionKey);
        if (nCompression < 0)
        {
            nCompression = m_nCompression;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Determine planar configuration.                                 */
    /* -------------------------------------------------------------------- */
    nPlanarConfig = m_nPlanarConfig;
    if (nCompression == COMPRESSION_WEBP)
    {
        nPlanarConfig = PLANARCONFIG_CONTIG;
    }
    const char *pszInterleave =
        GetOptionValue("INTERLEAVE", "INTERLEAVE_OVERVIEW", &pszOptionKey);
    if (pszInterleave != nullptr && pszInterleave[0] != '\0')
    {
        if (EQUAL(pszInterleave, "PIXEL"))
            nPlanarConfig = PLANARCONFIG_CONTIG;
        else if (EQUAL(pszInterleave, "BAND"))
            nPlanarConfig = PLANARCONFIG_SEPARATE;
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s=%s unsupported, "
                     "value must be PIXEL or BAND. ignoring",
                     pszOptionKey, pszInterleave);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Determine predictor tag                                         */
    /* -------------------------------------------------------------------- */
    nPredictor = PREDICTOR_NONE;
    if (GTIFFSupportsPredictor(nCompression))
    {
        const char *pszPredictor =
            GetOptionValue("PREDICTOR", "PREDICTOR_OVERVIEW");
        if (pszPredictor != nullptr)
        {
            nPredictor = static_cast<uint16_t>(atoi(pszPredictor));
        }
        else if (GTIFFSupportsPredictor(m_nCompression))
            TIFFGetField(m_hTIFF, TIFFTAG_PREDICTOR, &nPredictor);
    }

    /* -------------------------------------------------------------------- */
    /*      Determine photometric tag                                       */
    /* -------------------------------------------------------------------- */
    nPhotometric = m_nPhotometric;
    const char *pszPhotometric =
        GetOptionValue("PHOTOMETRIC", "PHOTOMETRIC_OVERVIEW", &pszOptionKey);
    if (!GTIFFUpdatePhotometric(pszPhotometric, pszOptionKey, nCompression,
                                pszInterleave, nBands, nPhotometric,
                                nPlanarConfig))
    {
        return false;
    }

    /* -------------------------------------------------------------------- */
    /*      Determine JPEG quality                                          */
    /* -------------------------------------------------------------------- */
    nOvrJpegQuality = m_nJpegQuality;
    if (nCompression == COMPRESSION_JPEG)
    {
        const char *pszJPEGQuality =
            GetOptionValue("JPEG_QUALITY", "JPEG_QUALITY_OVERVIEW");
        if (pszJPEGQuality != nullptr)
        {
            nOvrJpegQuality = atoi(pszJPEGQuality);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Set nodata.                                                     */
    /* -------------------------------------------------------------------- */
    if (m_bNoDataSet)
    {
        osNoData = GTiffFormatGDALNoDataTagValue(m_dfNoDataValue);
    }

    /* -------------------------------------------------------------------- */
    /*      Fetch extra sample tag                                          */
    /* -------------------------------------------------------------------- */
    panExtraSampleValues = nullptr;
    nExtraSamples = 0;
    if (TIFFGetField(m_hTIFF, TIFFTAG_EXTRASAMPLES, &nExtraSamples,
                     &panExtraSampleValues))
    {
        uint16_t *panExtraSampleValuesNew = static_cast<uint16_t *>(
            CPLMalloc(nExtraSamples * sizeof(uint16_t)));
        memcpy(panExtraSampleValuesNew, panExtraSampleValues,
               nExtraSamples * sizeof(uint16_t));
        panExtraSampleValues = panExtraSampleValuesNew;
    }
    else
    {
        panExtraSampleValues = nullptr;
        nExtraSamples = 0;
    }

    return true;
}

/************************************************************************/
/*                  CreateOverviewsFromSrcOverviews()                   */
/************************************************************************/

// If poOvrDS is not null, it is used and poSrcDS is ignored.

CPLErr GTiffDataset::CreateOverviewsFromSrcOverviews(GDALDataset *poSrcDS,
                                                     GDALDataset *poOvrDS,
                                                     int nOverviews)
{
    CPLAssert(poSrcDS->GetRasterCount() != 0);
    CPLAssert(m_nOverviewCount == 0);

    ScanDirectories();

    FlushDirectory();

    int nOvBitsPerSample = m_nBitsPerSample;

    /* -------------------------------------------------------------------- */
    /*      Do we need some metadata for the overviews?                     */
    /* -------------------------------------------------------------------- */
    CPLString osMetadata;

    GTIFFBuildOverviewMetadata("NONE", this, false, osMetadata);

    int nCompression;
    uint16_t nPlanarConfig;
    uint16_t nPredictor;
    uint16_t nPhotometric;
    int nOvrJpegQuality;
    std::string osNoData;
    uint16_t *panExtraSampleValues = nullptr;
    uint16_t nExtraSamples = 0;
    if (!GetOverviewParameters(nCompression, nPlanarConfig, nPredictor,
                               nPhotometric, nOvrJpegQuality, osNoData,
                               panExtraSampleValues, nExtraSamples,
                               /*papszOptions=*/nullptr))
    {
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have a palette?  If so, create a TIFF compatible version. */
    /* -------------------------------------------------------------------- */
    std::vector<unsigned short> anTRed;
    std::vector<unsigned short> anTGreen;
    std::vector<unsigned short> anTBlue;
    unsigned short *panRed = nullptr;
    unsigned short *panGreen = nullptr;
    unsigned short *panBlue = nullptr;

    if (nPhotometric == PHOTOMETRIC_PALETTE && m_poColorTable != nullptr)
    {
        CreateTIFFColorTable(m_poColorTable, nOvBitsPerSample, anTRed, anTGreen,
                             anTBlue, panRed, panGreen, panBlue);
    }

    int nOvrBlockXSize = 0;
    int nOvrBlockYSize = 0;
    GTIFFGetOverviewBlockSize(GDALRasterBand::ToHandle(GetRasterBand(1)),
                              &nOvrBlockXSize, &nOvrBlockYSize);

    CPLErr eErr = CE_None;

    for (int i = 0; i < nOverviews && eErr == CE_None; ++i)
    {
        GDALRasterBand *poOvrBand =
            poOvrDS ? ((i == 0) ? poOvrDS->GetRasterBand(1)
                                : poOvrDS->GetRasterBand(1)->GetOverview(i - 1))
                    : poSrcDS->GetRasterBand(1)->GetOverview(i);

        int nOXSize = poOvrBand->GetXSize();
        int nOYSize = poOvrBand->GetYSize();

        toff_t nOverviewOffset = GTIFFWriteDirectory(
            m_hTIFF, FILETYPE_REDUCEDIMAGE, nOXSize, nOYSize, nOvBitsPerSample,
            nPlanarConfig, m_nSamplesPerPixel, nOvrBlockXSize, nOvrBlockYSize,
            TRUE, nCompression, nPhotometric, m_nSampleFormat, nPredictor,
            panRed, panGreen, panBlue, nExtraSamples, panExtraSampleValues,
            osMetadata,
            nOvrJpegQuality >= 0 ? CPLSPrintf("%d", nOvrJpegQuality) : nullptr,
            CPLSPrintf("%d", m_nJpegTablesMode),
            osNoData.empty() ? nullptr : osNoData.c_str(),
            m_anLercAddCompressionAndVersion, m_bWriteCOGLayout);

        if (nOverviewOffset == 0)
            eErr = CE_Failure;
        else
            eErr = RegisterNewOverviewDataset(nOverviewOffset, nOvrJpegQuality,
                                              nullptr);
    }

    // For directory reloading, so that the chaining to the next directory is
    // reloaded, as well as compression parameters.
    ReloadDirectory();

    CPLFree(panExtraSampleValues);
    panExtraSampleValues = nullptr;

    return eErr;
}

/************************************************************************/
/*                       CreateInternalMaskOverviews()                  */
/************************************************************************/

CPLErr GTiffDataset::CreateInternalMaskOverviews(int nOvrBlockXSize,
                                                 int nOvrBlockYSize)
{
    ScanDirectories();

    /* -------------------------------------------------------------------- */
    /*      Create overviews for the mask.                                  */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if (m_poMaskDS != nullptr && m_poMaskDS->GetRasterCount() == 1)
    {
        int nMaskOvrCompression;
        if (strstr(GDALGetMetadataItem(GDALGetDriverByName("GTiff"),
                                       GDAL_DMD_CREATIONOPTIONLIST, nullptr),
                   "<Value>DEFLATE</Value>") != nullptr)
            nMaskOvrCompression = COMPRESSION_ADOBE_DEFLATE;
        else
            nMaskOvrCompression = COMPRESSION_PACKBITS;

        for (int i = 0; i < m_nOverviewCount; ++i)
        {
            if (m_papoOverviewDS[i]->m_poMaskDS == nullptr)
            {
                const toff_t nOverviewOffset = GTIFFWriteDirectory(
                    m_hTIFF, FILETYPE_REDUCEDIMAGE | FILETYPE_MASK,
                    m_papoOverviewDS[i]->nRasterXSize,
                    m_papoOverviewDS[i]->nRasterYSize, 1, PLANARCONFIG_CONTIG,
                    1, nOvrBlockXSize, nOvrBlockYSize, TRUE,
                    nMaskOvrCompression, PHOTOMETRIC_MASK, SAMPLEFORMAT_UINT,
                    PREDICTOR_NONE, nullptr, nullptr, nullptr, 0, nullptr, "",
                    nullptr, nullptr, nullptr, nullptr, m_bWriteCOGLayout);

                if (nOverviewOffset == 0)
                {
                    eErr = CE_Failure;
                    continue;
                }

                GTiffDataset *poODS = new GTiffDataset();
                poODS->ShareLockWithParentDataset(this);
                poODS->m_pszFilename = CPLStrdup(m_pszFilename);
                if (poODS->OpenOffset(VSI_TIFFOpenChild(m_hTIFF),
                                      nOverviewOffset, GA_Update) != CE_None)
                {
                    delete poODS;
                    eErr = CE_Failure;
                }
                else
                {
                    poODS->m_bPromoteTo8Bits = CPLTestBool(CPLGetConfigOption(
                        "GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
                    poODS->m_poBaseDS = this;
                    poODS->m_poImageryDS = m_papoOverviewDS[i];
                    m_papoOverviewDS[i]->m_poMaskDS = poODS;
                    ++m_poMaskDS->m_nOverviewCount;
                    m_poMaskDS->m_papoOverviewDS =
                        static_cast<GTiffDataset **>(CPLRealloc(
                            m_poMaskDS->m_papoOverviewDS,
                            m_poMaskDS->m_nOverviewCount * (sizeof(void *))));
                    m_poMaskDS
                        ->m_papoOverviewDS[m_poMaskDS->m_nOverviewCount - 1] =
                        poODS;
                }
            }
        }
    }

    ReloadDirectory();

    return eErr;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr GTiffDataset::IBuildOverviews(const char *pszResampling, int nOverviews,
                                     const int *panOverviewList, int nBandsIn,
                                     const int *panBandList,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData,
                                     CSLConstList papszOptions)

{
    ScanDirectories();

    // Make implicit JPEG overviews invisible, but do not destroy
    // them in case they are already used (not sure that the client
    // has the right to do that.  Behavior maybe undefined in GDAL API.
    m_nJPEGOverviewCount = 0;

    /* -------------------------------------------------------------------- */
    /*      If RRD or external OVR overviews requested, then invoke         */
    /*      generic handling.                                               */
    /* -------------------------------------------------------------------- */
    bool bUseGenericHandling = false;

    if (CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "USE_RRD", CPLGetConfigOption("USE_RRD", "NO"))) ||
        CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "TIFF_USE_OVR",
                                 CPLGetConfigOption("TIFF_USE_OVR", "NO"))))
    {
        bUseGenericHandling = true;
    }

    /* -------------------------------------------------------------------- */
    /*      If we don't have read access, then create the overviews         */
    /*      externally.                                                     */
    /* -------------------------------------------------------------------- */
    if (GetAccess() != GA_Update)
    {
        CPLDebug("GTiff", "File open for read-only accessing, "
                          "creating overviews externally.");

        bUseGenericHandling = true;
    }

    if (bUseGenericHandling)
    {
        if (m_nOverviewCount != 0)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Cannot add external overviews when there are already "
                        "internal overviews");
            return CE_Failure;
        }

        CPLStringList aosOptions(papszOptions);
        if (!m_bWriteEmptyTiles)
        {
            aosOptions.SetNameValue("SPARSE_OK", "YES");
        }

        CPLErr eErr = GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList, nBandsIn, panBandList,
            pfnProgress, pProgressData, aosOptions);
        if (eErr == CE_None && m_poMaskDS)
        {
            ReportError(
                CE_Warning, CPLE_NotSupported,
                "Building external overviews whereas there is an internal "
                "mask is not fully supported. "
                "The overviews of the non-mask bands will be created, "
                "but not the overviews of the mask band.");
        }
        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Our TIFF overview support currently only works safely if all    */
    /*      bands are handled at the same time.                             */
    /* -------------------------------------------------------------------- */
    if (nBandsIn != GetRasterCount())
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Generation of overviews in TIFF currently only "
                    "supported when operating on all bands.  "
                    "Operation failed.");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      If zero overviews were requested, we need to clear all          */
    /*      existing overviews.                                             */
    /* -------------------------------------------------------------------- */
    if (nOverviews == 0)
    {
        if (m_nOverviewCount == 0)
            return GDALDataset::IBuildOverviews(
                pszResampling, nOverviews, panOverviewList, nBandsIn,
                panBandList, pfnProgress, pProgressData, papszOptions);

        return CleanOverviews();
    }

    CPLErr eErr = CE_None;

    /* -------------------------------------------------------------------- */
    /*      Initialize progress counter.                                    */
    /* -------------------------------------------------------------------- */
    if (!pfnProgress(0.0, nullptr, pProgressData))
    {
        ReportError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return CE_Failure;
    }

    FlushDirectory();

    /* -------------------------------------------------------------------- */
    /*      If we are averaging bit data to grayscale we need to create     */
    /*      8bit overviews.                                                 */
    /* -------------------------------------------------------------------- */
    int nOvBitsPerSample = m_nBitsPerSample;

    if (STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2"))
        nOvBitsPerSample = 8;

    /* -------------------------------------------------------------------- */
    /*      Do we need some metadata for the overviews?                     */
    /* -------------------------------------------------------------------- */
    CPLString osMetadata;

    const bool bIsForMaskBand = nBands == 1 && GetRasterBand(1)->IsMaskBand();
    GTIFFBuildOverviewMetadata(pszResampling, this, bIsForMaskBand, osMetadata);

    int nCompression;
    uint16_t nPlanarConfig;
    uint16_t nPredictor;
    uint16_t nPhotometric;
    int nOvrJpegQuality;
    std::string osNoData;
    uint16_t *panExtraSampleValues = nullptr;
    uint16_t nExtraSamples = 0;
    if (!GetOverviewParameters(nCompression, nPlanarConfig, nPredictor,
                               nPhotometric, nOvrJpegQuality, osNoData,
                               panExtraSampleValues, nExtraSamples,
                               papszOptions))
    {
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have a palette?  If so, create a TIFF compatible version. */
    /* -------------------------------------------------------------------- */
    std::vector<unsigned short> anTRed;
    std::vector<unsigned short> anTGreen;
    std::vector<unsigned short> anTBlue;
    unsigned short *panRed = nullptr;
    unsigned short *panGreen = nullptr;
    unsigned short *panBlue = nullptr;

    if (nPhotometric == PHOTOMETRIC_PALETTE && m_poColorTable != nullptr)
    {
        CreateTIFFColorTable(m_poColorTable, nOvBitsPerSample, anTRed, anTGreen,
                             anTBlue, panRed, panGreen, panBlue);
    }

    /* -------------------------------------------------------------------- */
    /*      Establish which of the overview levels we already have, and     */
    /*      which are new.  We assume that band 1 of the file is            */
    /*      representative.                                                 */
    /* -------------------------------------------------------------------- */
    int nOvrBlockXSize = 0;
    int nOvrBlockYSize = 0;
    GTIFFGetOverviewBlockSize(GDALRasterBand::ToHandle(GetRasterBand(1)),
                              &nOvrBlockXSize, &nOvrBlockYSize);
    std::vector<bool> abRequireNewOverview(nOverviews, true);
    for (int i = 0; i < nOverviews && eErr == CE_None; ++i)
    {
        for (int j = 0; j < m_nOverviewCount && eErr == CE_None; ++j)
        {
            GTiffDataset *poODS = m_papoOverviewDS[j];

            const int nOvFactor =
                GDALComputeOvFactor(poODS->GetRasterXSize(), GetRasterXSize(),
                                    poODS->GetRasterYSize(), GetRasterYSize());

            // If we already have a 1x1 overview and this new one would result
            // in it too, then don't create it.
            if (poODS->GetRasterXSize() == 1 && poODS->GetRasterYSize() == 1 &&
                (GetRasterXSize() + panOverviewList[i] - 1) /
                        panOverviewList[i] ==
                    1 &&
                (GetRasterYSize() + panOverviewList[i] - 1) /
                        panOverviewList[i] ==
                    1)
            {
                abRequireNewOverview[i] = false;
                break;
            }

            if (nOvFactor == panOverviewList[i] ||
                nOvFactor == GDALOvLevelAdjust2(panOverviewList[i],
                                                GetRasterXSize(),
                                                GetRasterYSize()))
            {
                abRequireNewOverview[i] = false;
                break;
            }
        }

        if (abRequireNewOverview[i])
        {
            if (m_bLayoutIFDSBeforeData && !m_bKnownIncompatibleEdition &&
                !m_bWriteKnownIncompatibleEdition)
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                            "Adding new overviews invalidates the "
                            "LAYOUT=IFDS_BEFORE_DATA property");
                m_bKnownIncompatibleEdition = true;
                m_bWriteKnownIncompatibleEdition = true;
            }

            const int nOXSize = (GetRasterXSize() + panOverviewList[i] - 1) /
                                panOverviewList[i];
            const int nOYSize = (GetRasterYSize() + panOverviewList[i] - 1) /
                                panOverviewList[i];

            const toff_t nOverviewOffset = GTIFFWriteDirectory(
                m_hTIFF, FILETYPE_REDUCEDIMAGE, nOXSize, nOYSize,
                nOvBitsPerSample, nPlanarConfig, m_nSamplesPerPixel,
                nOvrBlockXSize, nOvrBlockYSize, TRUE, nCompression,
                nPhotometric, m_nSampleFormat, nPredictor, panRed, panGreen,
                panBlue, nExtraSamples, panExtraSampleValues, osMetadata,
                nOvrJpegQuality >= 0 ? CPLSPrintf("%d", nOvrJpegQuality)
                                     : nullptr,
                CPLSPrintf("%d", m_nJpegTablesMode),
                osNoData.empty() ? nullptr : osNoData.c_str(),
                m_anLercAddCompressionAndVersion, false);

            if (nOverviewOffset == 0)
                eErr = CE_Failure;
            else
                eErr = RegisterNewOverviewDataset(
                    nOverviewOffset, nOvrJpegQuality, papszOptions);
        }
    }

    CPLFree(panExtraSampleValues);
    panExtraSampleValues = nullptr;

    ReloadDirectory();

    /* -------------------------------------------------------------------- */
    /*      Create overviews for the mask.                                  */
    /* -------------------------------------------------------------------- */
    if (eErr != CE_None)
        return eErr;

    eErr = CreateInternalMaskOverviews(nOvrBlockXSize, nOvrBlockYSize);

    /* -------------------------------------------------------------------- */
    /*      Refresh overviews for the mask                                  */
    /* -------------------------------------------------------------------- */
    const bool bHasInternalMask =
        m_poMaskDS != nullptr && m_poMaskDS->GetRasterCount() == 1;
    const bool bHasExternalMask =
        !bHasInternalMask && oOvManager.HaveMaskFile();
    const bool bHasMask = bHasInternalMask || bHasExternalMask;

    if (bHasInternalMask)
    {
        int nMaskOverviews = 0;

        GDALRasterBand **papoOverviewBands = static_cast<GDALRasterBand **>(
            CPLCalloc(sizeof(void *), m_nOverviewCount));
        for (int i = 0; i < m_nOverviewCount; ++i)
        {
            if (m_papoOverviewDS[i]->m_poMaskDS != nullptr)
            {
                papoOverviewBands[nMaskOverviews++] =
                    m_papoOverviewDS[i]->m_poMaskDS->GetRasterBand(1);
            }
        }

        void *pScaledProgressData = GDALCreateScaledProgress(
            0, 1.0 / (nBands + 1), pfnProgress, pProgressData);
        eErr = GDALRegenerateOverviewsEx(
            m_poMaskDS->GetRasterBand(1), nMaskOverviews,
            reinterpret_cast<GDALRasterBandH *>(papoOverviewBands),
            pszResampling, GDALScaledProgress, pScaledProgressData,
            papszOptions);
        GDALDestroyScaledProgress(pScaledProgressData);
        CPLFree(papoOverviewBands);
    }
    else if (bHasExternalMask)
    {
        void *pScaledProgressData = GDALCreateScaledProgress(
            0, 1.0 / (nBands + 1), pfnProgress, pProgressData);
        eErr = oOvManager.BuildOverviewsMask(
            pszResampling, nOverviews, panOverviewList, GDALScaledProgress,
            pScaledProgressData, papszOptions);
        GDALDestroyScaledProgress(pScaledProgressData);
    }

    // If we have an alpha band, we want it to be generated before downsampling
    // other bands
    bool bHasAlphaBand = false;
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        if (papoBands[iBand]->GetColorInterpretation() == GCI_AlphaBand)
            bHasAlphaBand = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Refresh old overviews that were listed.                         */
    /* -------------------------------------------------------------------- */
    const auto poColorTable = GetRasterBand(panBandList[0])->GetColorTable();
    if ((m_nPlanarConfig == PLANARCONFIG_CONTIG || bHasAlphaBand) &&
        GDALDataTypeIsComplex(
            GetRasterBand(panBandList[0])->GetRasterDataType()) == FALSE &&
        (poColorTable == nullptr || STARTS_WITH_CI(pszResampling, "NEAR") ||
         poColorTable->IsIdentity()) &&
        (STARTS_WITH_CI(pszResampling, "NEAR") ||
         EQUAL(pszResampling, "AVERAGE") || EQUAL(pszResampling, "RMS") ||
         EQUAL(pszResampling, "GAUSS") || EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") || EQUAL(pszResampling, "BILINEAR") ||
         EQUAL(pszResampling, "MODE")))
    {
        // In the case of pixel interleaved compressed overviews, we want to
        // generate the overviews for all the bands block by block, and not
        // band after band, in order to write the block once and not loose
        // space in the TIFF file.  We also use that logic for uncompressed
        // overviews, since GDALRegenerateOverviewsMultiBand() will be able to
        // trigger cascading overview regeneration even in the presence
        // of an alpha band.

        int nNewOverviews = 0;

        GDALRasterBand ***papapoOverviewBands = static_cast<GDALRasterBand ***>(
            CPLCalloc(sizeof(void *), nBandsIn));
        GDALRasterBand **papoBandList =
            static_cast<GDALRasterBand **>(CPLCalloc(sizeof(void *), nBandsIn));
        for (int iBand = 0; iBand < nBandsIn; ++iBand)
        {
            GDALRasterBand *poBand = GetRasterBand(panBandList[iBand]);

            papoBandList[iBand] = poBand;
            papapoOverviewBands[iBand] = static_cast<GDALRasterBand **>(
                CPLCalloc(sizeof(void *), poBand->GetOverviewCount()));

            int iCurOverview = 0;
            std::vector<bool> abAlreadyUsedOverviewBand(
                poBand->GetOverviewCount(), false);

            for (int i = 0; i < nOverviews; ++i)
            {
                for (int j = 0; j < poBand->GetOverviewCount(); ++j)
                {
                    if (abAlreadyUsedOverviewBand[j])
                        continue;

                    int nOvFactor;
                    GDALRasterBand *poOverview = poBand->GetOverview(j);

                    nOvFactor = GDALComputeOvFactor(
                        poOverview->GetXSize(), poBand->GetXSize(),
                        poOverview->GetYSize(), poBand->GetYSize());

                    GDALCopyNoDataValue(poOverview, poBand);

                    if (nOvFactor == panOverviewList[i] ||
                        nOvFactor == GDALOvLevelAdjust2(panOverviewList[i],
                                                        poBand->GetXSize(),
                                                        poBand->GetYSize()))
                    {
                        if (iBand == 0)
                        {
                            const auto osNewResampling =
                                GDALGetNormalizedOvrResampling(pszResampling);
                            const char *pszExistingResampling =
                                poOverview->GetMetadataItem("RESAMPLING");
                            if (pszExistingResampling &&
                                pszExistingResampling != osNewResampling)
                            {
                                poOverview->SetMetadataItem(
                                    "RESAMPLING", osNewResampling.c_str());
                            }
                        }

                        abAlreadyUsedOverviewBand[j] = true;
                        CPLAssert(iCurOverview < poBand->GetOverviewCount());
                        papapoOverviewBands[iBand][iCurOverview] = poOverview;
                        ++iCurOverview;
                        break;
                    }
                }
            }

            if (nNewOverviews == 0)
            {
                nNewOverviews = iCurOverview;
            }
            else if (nNewOverviews != iCurOverview)
            {
                CPLAssert(false);
                return CE_Failure;
            }
        }

        void *pScaledProgressData =
            bHasMask ? GDALCreateScaledProgress(1.0 / (nBands + 1), 1.0,
                                                pfnProgress, pProgressData)
                     : GDALCreateScaledProgress(0.0, 1.0, pfnProgress,
                                                pProgressData);
        GDALRegenerateOverviewsMultiBand(nBandsIn, papoBandList, nNewOverviews,
                                         papapoOverviewBands, pszResampling,
                                         GDALScaledProgress,
                                         pScaledProgressData, papszOptions);
        GDALDestroyScaledProgress(pScaledProgressData);

        for (int iBand = 0; iBand < nBandsIn; ++iBand)
        {
            CPLFree(papapoOverviewBands[iBand]);
        }
        CPLFree(papapoOverviewBands);
        CPLFree(papoBandList);
    }
    else
    {
        GDALRasterBand **papoOverviewBands = static_cast<GDALRasterBand **>(
            CPLCalloc(sizeof(void *), nOverviews));

        const int iBandOffset = bHasMask ? 1 : 0;

        for (int iBand = 0; iBand < nBandsIn && eErr == CE_None; ++iBand)
        {
            GDALRasterBand *poBand = GetRasterBand(panBandList[iBand]);
            if (poBand == nullptr)
            {
                eErr = CE_Failure;
                break;
            }

            std::vector<bool> abAlreadyUsedOverviewBand(
                poBand->GetOverviewCount(), false);

            int nNewOverviews = 0;
            for (int i = 0; i < nOverviews; ++i)
            {
                for (int j = 0; j < poBand->GetOverviewCount(); ++j)
                {
                    if (abAlreadyUsedOverviewBand[j])
                        continue;

                    GDALRasterBand *poOverview = poBand->GetOverview(j);

                    GDALCopyNoDataValue(poOverview, poBand);

                    const int nOvFactor = GDALComputeOvFactor(
                        poOverview->GetXSize(), poBand->GetXSize(),
                        poOverview->GetYSize(), poBand->GetYSize());

                    if (nOvFactor == panOverviewList[i] ||
                        nOvFactor == GDALOvLevelAdjust2(panOverviewList[i],
                                                        poBand->GetXSize(),
                                                        poBand->GetYSize()))
                    {
                        if (iBand == 0)
                        {
                            const auto osNewResampling =
                                GDALGetNormalizedOvrResampling(pszResampling);
                            const char *pszExistingResampling =
                                poOverview->GetMetadataItem("RESAMPLING");
                            if (pszExistingResampling &&
                                pszExistingResampling != osNewResampling)
                            {
                                poOverview->SetMetadataItem(
                                    "RESAMPLING", osNewResampling.c_str());
                            }
                        }

                        abAlreadyUsedOverviewBand[j] = true;
                        CPLAssert(nNewOverviews < poBand->GetOverviewCount());
                        papoOverviewBands[nNewOverviews++] = poOverview;
                        break;
                    }
                }
            }

            void *pScaledProgressData = GDALCreateScaledProgress(
                (iBand + iBandOffset) /
                    static_cast<double>(nBandsIn + iBandOffset),
                (iBand + iBandOffset + 1) /
                    static_cast<double>(nBandsIn + iBandOffset),
                pfnProgress, pProgressData);

            eErr = GDALRegenerateOverviewsEx(
                poBand, nNewOverviews,
                reinterpret_cast<GDALRasterBandH *>(papoOverviewBands),
                pszResampling, GDALScaledProgress, pScaledProgressData,
                papszOptions);

            GDALDestroyScaledProgress(pScaledProgressData);
        }

        /* --------------------------------------------------------------------
         */
        /*      Cleanup */
        /* --------------------------------------------------------------------
         */
        CPLFree(papoOverviewBands);
    }

    pfnProgress(1.0, nullptr, pProgressData);

    return eErr;
}

/************************************************************************/
/*                      GTiffWriteDummyGeokeyDirectory()                */
/************************************************************************/

static void GTiffWriteDummyGeokeyDirectory(TIFF *hTIFF)
{
    // If we have existing geokeys, try to wipe them
    // by writing a dummy geokey directory. (#2546)
    uint16_t *panVI = nullptr;
    uint16_t nKeyCount = 0;

    if (TIFFGetField(hTIFF, TIFFTAG_GEOKEYDIRECTORY, &nKeyCount, &panVI))
    {
        GUInt16 anGKVersionInfo[4] = {1, 1, 0, 0};
        double adfDummyDoubleParams[1] = {0.0};
        TIFFSetField(hTIFF, TIFFTAG_GEOKEYDIRECTORY, 4, anGKVersionInfo);
        TIFFSetField(hTIFF, TIFFTAG_GEODOUBLEPARAMS, 1, adfDummyDoubleParams);
        TIFFSetField(hTIFF, TIFFTAG_GEOASCIIPARAMS, "");
    }
}

/************************************************************************/
/*                    IsSRSCompatibleOfGeoTIFF()                        */
/************************************************************************/

static bool IsSRSCompatibleOfGeoTIFF(const OGRSpatialReference *poSRS,
                                     GTIFFKeysFlavorEnum eGeoTIFFKeysFlavor)
{
    char *pszWKT = nullptr;
    if ((poSRS->IsGeographic() || poSRS->IsProjected()) && !poSRS->IsCompound())
    {
        const char *pszAuthName = poSRS->GetAuthorityName(nullptr);
        const char *pszAuthCode = poSRS->GetAuthorityCode(nullptr);
        if (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
            return true;
    }
    OGRErr eErr;
    {
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
        if (poSRS->IsDerivedGeographic() ||
            (poSRS->IsProjected() && !poSRS->IsCompound() &&
             poSRS->GetAxesCount() == 3))
        {
            eErr = OGRERR_FAILURE;
        }
        else
        {
            // Geographic3D CRS can't be exported to WKT1, but are
            // valid GeoTIFF 1.1
            const char *const apszOptions[] = {
                poSRS->IsGeographic() ? nullptr : "FORMAT=WKT1", nullptr};
            eErr = poSRS->exportToWkt(&pszWKT, apszOptions);
            if (eErr == OGRERR_FAILURE && poSRS->IsProjected() &&
                eGeoTIFFKeysFlavor == GEOTIFF_KEYS_ESRI_PE)
            {
                CPLFree(pszWKT);
                const char *const apszOptionsESRIWKT[] = {"FORMAT=WKT1_ESRI",
                                                          nullptr};
                eErr = poSRS->exportToWkt(&pszWKT, apszOptionsESRIWKT);
            }
        }
    }
    const bool bCompatibleOfGeoTIFF =
        (eErr == OGRERR_NONE && pszWKT != nullptr &&
         strstr(pszWKT, "custom_proj4") == nullptr);
    CPLFree(pszWKT);
    return bCompatibleOfGeoTIFF;
}

/************************************************************************/
/*                          WriteGeoTIFFInfo()                          */
/************************************************************************/

void GTiffDataset::WriteGeoTIFFInfo()

{
    bool bPixelIsPoint = false;
    bool bPointGeoIgnore = false;

    const char *pszAreaOrPoint =
        GTiffDataset::GetMetadataItem(GDALMD_AREA_OR_POINT);
    if (pszAreaOrPoint && EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT))
    {
        bPixelIsPoint = true;
        bPointGeoIgnore =
            CPLTestBool(CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE", "FALSE"));
    }

    if (m_bForceUnsetGTOrGCPs)
    {
        m_bNeedsRewrite = true;
        m_bForceUnsetGTOrGCPs = false;

        TIFFUnsetField(m_hTIFF, TIFFTAG_GEOPIXELSCALE);
        TIFFUnsetField(m_hTIFF, TIFFTAG_GEOTIEPOINTS);
        TIFFUnsetField(m_hTIFF, TIFFTAG_GEOTRANSMATRIX);
    }

    if (m_bForceUnsetProjection)
    {
        m_bNeedsRewrite = true;
        m_bForceUnsetProjection = false;

        TIFFUnsetField(m_hTIFF, TIFFTAG_GEOKEYDIRECTORY);
        TIFFUnsetField(m_hTIFF, TIFFTAG_GEODOUBLEPARAMS);
        TIFFUnsetField(m_hTIFF, TIFFTAG_GEOASCIIPARAMS);
    }

    /* -------------------------------------------------------------------- */
    /*      Write geotransform if valid.                                    */
    /* -------------------------------------------------------------------- */
    if (m_bGeoTransformValid)
    {
        m_bNeedsRewrite = true;

        /* --------------------------------------------------------------------
         */
        /*      Clear old tags to ensure we don't end up with conflicting */
        /*      information. (#2625) */
        /* --------------------------------------------------------------------
         */
        TIFFUnsetField(m_hTIFF, TIFFTAG_GEOPIXELSCALE);
        TIFFUnsetField(m_hTIFF, TIFFTAG_GEOTIEPOINTS);
        TIFFUnsetField(m_hTIFF, TIFFTAG_GEOTRANSMATRIX);

        /* --------------------------------------------------------------------
         */
        /*      Write the transform.  If we have a normal north-up image we */
        /*      use the tiepoint plus pixelscale otherwise we use a matrix. */
        /* --------------------------------------------------------------------
         */
        if (m_adfGeoTransform[2] == 0.0 && m_adfGeoTransform[4] == 0.0 &&
            m_adfGeoTransform[5] < 0.0)
        {
            double dfOffset = 0.0;
            if (m_eProfile != GTiffProfile::BASELINE)
            {
                // In the case the SRS has a vertical component and we have
                // a single band, encode its scale/offset in the GeoTIFF tags
                int bHasScale = FALSE;
                double dfScale = GetRasterBand(1)->GetScale(&bHasScale);
                int bHasOffset = FALSE;
                dfOffset = GetRasterBand(1)->GetOffset(&bHasOffset);
                const bool bApplyScaleOffset =
                    m_oSRS.IsVertical() && GetRasterCount() == 1;
                if (bApplyScaleOffset && !bHasScale)
                    dfScale = 1.0;
                if (!bApplyScaleOffset || !bHasOffset)
                    dfOffset = 0.0;
                const double adfPixelScale[3] = {
                    m_adfGeoTransform[1], fabs(m_adfGeoTransform[5]),
                    bApplyScaleOffset ? dfScale : 0.0};
                TIFFSetField(m_hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale);
            }

            double adfTiePoints[6] = {
                0.0,     0.0, 0.0, m_adfGeoTransform[0], m_adfGeoTransform[3],
                dfOffset};

            if (bPixelIsPoint && !bPointGeoIgnore)
            {
                adfTiePoints[3] +=
                    m_adfGeoTransform[1] * 0.5 + m_adfGeoTransform[2] * 0.5;
                adfTiePoints[4] +=
                    m_adfGeoTransform[4] * 0.5 + m_adfGeoTransform[5] * 0.5;
            }

            if (m_eProfile != GTiffProfile::BASELINE)
                TIFFSetField(m_hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints);
        }
        else
        {
            double adfMatrix[16] = {};

            adfMatrix[0] = m_adfGeoTransform[1];
            adfMatrix[1] = m_adfGeoTransform[2];
            adfMatrix[3] = m_adfGeoTransform[0];
            adfMatrix[4] = m_adfGeoTransform[4];
            adfMatrix[5] = m_adfGeoTransform[5];
            adfMatrix[7] = m_adfGeoTransform[3];
            adfMatrix[15] = 1.0;

            if (bPixelIsPoint && !bPointGeoIgnore)
            {
                adfMatrix[3] +=
                    m_adfGeoTransform[1] * 0.5 + m_adfGeoTransform[2] * 0.5;
                adfMatrix[7] +=
                    m_adfGeoTransform[4] * 0.5 + m_adfGeoTransform[5] * 0.5;
            }

            if (m_eProfile != GTiffProfile::BASELINE)
                TIFFSetField(m_hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix);
        }

        // Do we need a world file?
        if (CPLFetchBool(m_papszCreationOptions, "TFW", false))
            GDALWriteWorldFile(m_pszFilename, "tfw", m_adfGeoTransform);
        else if (CPLFetchBool(m_papszCreationOptions, "WORLDFILE", false))
            GDALWriteWorldFile(m_pszFilename, "wld", m_adfGeoTransform);
    }
    else if (GetGCPCount() > 0 && GetGCPCount() <= knMAX_GCP_COUNT &&
             m_eProfile != GTiffProfile::BASELINE)
    {
        m_bNeedsRewrite = true;

        double *padfTiePoints = static_cast<double *>(
            CPLMalloc(6 * sizeof(double) * GetGCPCount()));

        for (size_t iGCP = 0; iGCP < m_aoGCPs.size(); ++iGCP)
        {

            padfTiePoints[iGCP * 6 + 0] = m_aoGCPs[iGCP].Pixel();
            padfTiePoints[iGCP * 6 + 1] = m_aoGCPs[iGCP].Line();
            padfTiePoints[iGCP * 6 + 2] = 0;
            padfTiePoints[iGCP * 6 + 3] = m_aoGCPs[iGCP].X();
            padfTiePoints[iGCP * 6 + 4] = m_aoGCPs[iGCP].Y();
            padfTiePoints[iGCP * 6 + 5] = m_aoGCPs[iGCP].Z();

            if (bPixelIsPoint && !bPointGeoIgnore)
            {
                padfTiePoints[iGCP * 6 + 0] += 0.5;
                padfTiePoints[iGCP * 6 + 1] += 0.5;
            }
        }

        TIFFSetField(m_hTIFF, TIFFTAG_GEOTIEPOINTS, 6 * GetGCPCount(),
                     padfTiePoints);
        CPLFree(padfTiePoints);
    }

    /* -------------------------------------------------------------------- */
    /*      Write out projection definition.                                */
    /* -------------------------------------------------------------------- */
    const bool bHasProjection = !m_oSRS.IsEmpty();
    if ((bHasProjection || bPixelIsPoint) &&
        m_eProfile != GTiffProfile::BASELINE)
    {
        m_bNeedsRewrite = true;

        // If we have existing geokeys, try to wipe them
        // by writing a dummy geokey directory. (#2546)
        GTiffWriteDummyGeokeyDirectory(m_hTIFF);

        GTIF *psGTIF = GTiffDataset::GTIFNew(m_hTIFF);

        // Set according to coordinate system.
        if (bHasProjection)
        {
            if (IsSRSCompatibleOfGeoTIFF(&m_oSRS, m_eGeoTIFFKeysFlavor))
            {
                GTIFSetFromOGISDefnEx(psGTIF,
                                      OGRSpatialReference::ToHandle(&m_oSRS),
                                      m_eGeoTIFFKeysFlavor, m_eGeoTIFFVersion);
            }
            else
            {
                GDALPamDataset::SetSpatialRef(&m_oSRS);
            }
        }

        if (bPixelIsPoint)
        {
            GTIFKeySet(psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1,
                       RasterPixelIsPoint);
        }

        GTIFWriteKeys(psGTIF);
        GTIFFree(psGTIF);
    }
}

/************************************************************************/
/*                         AppendMetadataItem()                         */
/************************************************************************/

static void AppendMetadataItem(CPLXMLNode **ppsRoot, CPLXMLNode **ppsTail,
                               const char *pszKey, const char *pszValue,
                               int nBand, const char *pszRole,
                               const char *pszDomain)

{
    /* -------------------------------------------------------------------- */
    /*      Create the Item element, and subcomponents.                     */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psItem = CPLCreateXMLNode(nullptr, CXT_Element, "Item");
    CPLCreateXMLNode(CPLCreateXMLNode(psItem, CXT_Attribute, "name"), CXT_Text,
                     pszKey);

    if (nBand > 0)
    {
        char szBandId[32] = {};
        snprintf(szBandId, sizeof(szBandId), "%d", nBand - 1);
        CPLCreateXMLNode(CPLCreateXMLNode(psItem, CXT_Attribute, "sample"),
                         CXT_Text, szBandId);
    }

    if (pszRole != nullptr)
        CPLCreateXMLNode(CPLCreateXMLNode(psItem, CXT_Attribute, "role"),
                         CXT_Text, pszRole);

    if (pszDomain != nullptr && strlen(pszDomain) > 0)
        CPLCreateXMLNode(CPLCreateXMLNode(psItem, CXT_Attribute, "domain"),
                         CXT_Text, pszDomain);

    // Note: this escaping should not normally be done, as the serialization
    // of the tree to XML also does it, so we end up width double XML escaping,
    // but keep it for backward compatibility.
    char *pszEscapedItemValue = CPLEscapeString(pszValue, -1, CPLES_XML);
    CPLCreateXMLNode(psItem, CXT_Text, pszEscapedItemValue);
    CPLFree(pszEscapedItemValue);

    /* -------------------------------------------------------------------- */
    /*      Create root, if missing.                                        */
    /* -------------------------------------------------------------------- */
    if (*ppsRoot == nullptr)
        *ppsRoot = CPLCreateXMLNode(nullptr, CXT_Element, "GDALMetadata");

    /* -------------------------------------------------------------------- */
    /*      Append item to tail.  We keep track of the tail to avoid        */
    /*      O(nsquared) time as the list gets longer.                       */
    /* -------------------------------------------------------------------- */
    if (*ppsTail == nullptr)
        CPLAddXMLChild(*ppsRoot, psItem);
    else
        CPLAddXMLSibling(*ppsTail, psItem);

    *ppsTail = psItem;
}

/************************************************************************/
/*                         WriteMDMetadata()                            */
/************************************************************************/

static void WriteMDMetadata(GDALMultiDomainMetadata *poMDMD, TIFF *hTIFF,
                            CPLXMLNode **ppsRoot, CPLXMLNode **ppsTail,
                            int nBand, GTiffProfile eProfile)

{

    /* ==================================================================== */
    /*      Process each domain.                                            */
    /* ==================================================================== */
    CSLConstList papszDomainList = poMDMD->GetDomainList();
    for (int iDomain = 0; papszDomainList && papszDomainList[iDomain];
         ++iDomain)
    {
        char **papszMD = poMDMD->GetMetadata(papszDomainList[iDomain]);
        bool bIsXML = false;

        if (EQUAL(papszDomainList[iDomain], "IMAGE_STRUCTURE") ||
            EQUAL(papszDomainList[iDomain], "DERIVED_SUBDATASETS"))
            continue;  // Ignored.
        if (EQUAL(papszDomainList[iDomain], "COLOR_PROFILE"))
            continue;  // Handled elsewhere.
        if (EQUAL(papszDomainList[iDomain], MD_DOMAIN_RPC))
            continue;  // Handled elsewhere.
        if (EQUAL(papszDomainList[iDomain], "xml:ESRI") &&
            CPLTestBool(CPLGetConfigOption("ESRI_XML_PAM", "NO")))
            continue;  // Handled elsewhere.
        if (EQUAL(papszDomainList[iDomain], "xml:XMP"))
            continue;  // Handled in SetMetadata.

        if (STARTS_WITH_CI(papszDomainList[iDomain], "xml:"))
            bIsXML = true;

        /* --------------------------------------------------------------------
         */
        /*      Process each item in this domain. */
        /* --------------------------------------------------------------------
         */
        for (int iItem = 0; papszMD && papszMD[iItem]; ++iItem)
        {
            const char *pszItemValue = nullptr;
            char *pszItemName = nullptr;

            if (bIsXML)
            {
                pszItemName = CPLStrdup("doc");
                pszItemValue = papszMD[iItem];
            }
            else
            {
                pszItemValue = CPLParseNameValue(papszMD[iItem], &pszItemName);
                if (pszItemName == nullptr)
                {
                    CPLDebug("GTiff", "Invalid metadata item : %s",
                             papszMD[iItem]);
                    continue;
                }
            }

            /* --------------------------------------------------------------------
             */
            /*      Convert into XML item or handle as a special TIFF tag. */
            /* --------------------------------------------------------------------
             */
            if (strlen(papszDomainList[iDomain]) == 0 && nBand == 0 &&
                (STARTS_WITH_CI(pszItemName, "TIFFTAG_") ||
                 (EQUAL(pszItemName, "GEO_METADATA") &&
                  eProfile == GTiffProfile::GDALGEOTIFF) ||
                 (EQUAL(pszItemName, "TIFF_RSID") &&
                  eProfile == GTiffProfile::GDALGEOTIFF)))
            {
                if (EQUAL(pszItemName, "TIFFTAG_RESOLUTIONUNIT"))
                {
                    // ResolutionUnit can't be 0, which is the default if
                    // atoi() fails.  Set to 1=Unknown.
                    int v = atoi(pszItemValue);
                    if (!v)
                        v = RESUNIT_NONE;
                    TIFFSetField(hTIFF, TIFFTAG_RESOLUTIONUNIT, v);
                }
                else
                {
                    bool bFoundTag = false;
                    size_t iTag = 0;  // Used after for.
                    const auto *pasTIFFTags = GTiffDataset::GetTIFFTags();
                    for (; pasTIFFTags[iTag].pszTagName; ++iTag)
                    {
                        if (EQUAL(pszItemName, pasTIFFTags[iTag].pszTagName))
                        {
                            bFoundTag = true;
                            break;
                        }
                    }

                    if (bFoundTag &&
                        pasTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING)
                        TIFFSetField(hTIFF, pasTIFFTags[iTag].nTagVal,
                                     pszItemValue);
                    else if (bFoundTag &&
                             pasTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT)
                        TIFFSetField(hTIFF, pasTIFFTags[iTag].nTagVal,
                                     CPLAtof(pszItemValue));
                    else if (bFoundTag &&
                             pasTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT)
                        TIFFSetField(hTIFF, pasTIFFTags[iTag].nTagVal,
                                     atoi(pszItemValue));
                    else if (bFoundTag && pasTIFFTags[iTag].eType ==
                                              GTIFFTAGTYPE_BYTE_STRING)
                    {
                        uint32_t nLen =
                            static_cast<uint32_t>(strlen(pszItemValue));
                        if (nLen)
                        {
                            TIFFSetField(hTIFF, pasTIFFTags[iTag].nTagVal, nLen,
                                         pszItemValue);
                        }
                    }
                    else
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "%s metadata item is unhandled and "
                                 "will not be written",
                                 pszItemName);
                }
            }
            else if (nBand == 0 && EQUAL(pszItemName, GDALMD_AREA_OR_POINT))
            {
                /* Do nothing, handled elsewhere. */;
            }
            else
            {
                AppendMetadataItem(ppsRoot, ppsTail, pszItemName, pszItemValue,
                                   nBand, nullptr, papszDomainList[iDomain]);
            }

            CPLFree(pszItemName);
        }

        /* --------------------------------------------------------------------
         */
        /*      Remove TIFFTAG_xxxxxx that are already set but no longer in */
        /*      the metadata list (#5619) */
        /* --------------------------------------------------------------------
         */
        if (strlen(papszDomainList[iDomain]) == 0 && nBand == 0)
        {
            const auto *pasTIFFTags = GTiffDataset::GetTIFFTags();
            for (size_t iTag = 0; pasTIFFTags[iTag].pszTagName; ++iTag)
            {
                uint32_t nCount = 0;
                char *pszText = nullptr;
                int16_t nVal = 0;
                float fVal = 0.0f;
                const char *pszVal =
                    CSLFetchNameValue(papszMD, pasTIFFTags[iTag].pszTagName);
                if (pszVal == nullptr &&
                    ((pasTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING &&
                      TIFFGetField(hTIFF, pasTIFFTags[iTag].nTagVal,
                                   &pszText)) ||
                     (pasTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT &&
                      TIFFGetField(hTIFF, pasTIFFTags[iTag].nTagVal, &nVal)) ||
                     (pasTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT &&
                      TIFFGetField(hTIFF, pasTIFFTags[iTag].nTagVal, &fVal)) ||
                     (pasTIFFTags[iTag].eType == GTIFFTAGTYPE_BYTE_STRING &&
                      TIFFGetField(hTIFF, pasTIFFTags[iTag].nTagVal, &nCount,
                                   &pszText))))
                {
                    TIFFUnsetField(hTIFF, pasTIFFTags[iTag].nTagVal);
                }
            }
        }
    }
}

/************************************************************************/
/*                           WriteRPC()                                 */
/************************************************************************/

void GTiffDataset::WriteRPC(GDALDataset *poSrcDS, TIFF *l_hTIFF,
                            int bSrcIsGeoTIFF, GTiffProfile eProfile,
                            const char *pszTIFFFilename,
                            CSLConstList papszCreationOptions,
                            bool bWriteOnlyInPAMIfNeeded)
{
    /* -------------------------------------------------------------------- */
    /*      Handle RPC data written to TIFF RPCCoefficient tag, RPB file,   */
    /*      RPCTEXT file or PAM.                                            */
    /* -------------------------------------------------------------------- */
    char **papszRPCMD = poSrcDS->GetMetadata(MD_DOMAIN_RPC);
    if (papszRPCMD != nullptr)
    {
        bool bRPCSerializedOtherWay = false;

        if (eProfile == GTiffProfile::GDALGEOTIFF)
        {
            if (!bWriteOnlyInPAMIfNeeded)
                GTiffDatasetWriteRPCTag(l_hTIFF, papszRPCMD);
            bRPCSerializedOtherWay = true;
        }

        // Write RPB file if explicitly asked, or if a non GDAL specific
        // profile is selected and RPCTXT is not asked.
        bool bRPBExplicitlyAsked =
            CPLFetchBool(papszCreationOptions, "RPB", false);
        bool bRPBExplicitlyDenied =
            !CPLFetchBool(papszCreationOptions, "RPB", true);
        if ((eProfile != GTiffProfile::GDALGEOTIFF &&
             !CPLFetchBool(papszCreationOptions, "RPCTXT", false) &&
             !bRPBExplicitlyDenied) ||
            bRPBExplicitlyAsked)
        {
            if (!bWriteOnlyInPAMIfNeeded)
                GDALWriteRPBFile(pszTIFFFilename, papszRPCMD);
            bRPCSerializedOtherWay = true;
        }

        if (CPLFetchBool(papszCreationOptions, "RPCTXT", false))
        {
            if (!bWriteOnlyInPAMIfNeeded)
                GDALWriteRPCTXTFile(pszTIFFFilename, papszRPCMD);
            bRPCSerializedOtherWay = true;
        }

        if (!bRPCSerializedOtherWay && bWriteOnlyInPAMIfNeeded && bSrcIsGeoTIFF)
            cpl::down_cast<GTiffDataset *>(poSrcDS)
                ->GDALPamDataset::SetMetadata(papszRPCMD, MD_DOMAIN_RPC);
    }
}

/************************************************************************/
/*                           WriteMetadata()                            */
/************************************************************************/

bool GTiffDataset::WriteMetadata(GDALDataset *poSrcDS, TIFF *l_hTIFF,
                                 bool bSrcIsGeoTIFF, GTiffProfile eProfile,
                                 const char *pszTIFFFilename,
                                 CSLConstList papszCreationOptions,
                                 bool bExcludeRPBandIMGFileWriting)

{
    /* -------------------------------------------------------------------- */
    /*      Convert all the remaining metadata into a simple XML            */
    /*      format.                                                         */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot = nullptr;
    CPLXMLNode *psTail = nullptr;

    if (bSrcIsGeoTIFF)
    {
        GTiffDataset *poSrcDSGTiff = cpl::down_cast<GTiffDataset *>(poSrcDS);
        assert(poSrcDSGTiff);
        WriteMDMetadata(&poSrcDSGTiff->m_oGTiffMDMD, l_hTIFF, &psRoot, &psTail,
                        0, eProfile);
    }
    else
    {
        const char *pszCopySrcMDD =
            CSLFetchNameValueDef(papszCreationOptions, "COPY_SRC_MDD", "AUTO");
        char **papszSrcMDD =
            CSLFetchNameValueMultiple(papszCreationOptions, "SRC_MDD");
        if (EQUAL(pszCopySrcMDD, "AUTO") || CPLTestBool(pszCopySrcMDD) ||
            papszSrcMDD)
        {
            GDALMultiDomainMetadata l_oMDMD;
            char **papszMD = poSrcDS->GetMetadata();
            if (CSLCount(papszMD) > 0 &&
                (!papszSrcMDD || CSLFindString(papszSrcMDD, "") >= 0 ||
                 CSLFindString(papszSrcMDD, "_DEFAULT_") >= 0))
            {
                l_oMDMD.SetMetadata(papszMD);
            }

            if ((!EQUAL(pszCopySrcMDD, "AUTO") && CPLTestBool(pszCopySrcMDD)) ||
                papszSrcMDD)
            {
                char **papszDomainList = poSrcDS->GetMetadataDomainList();
                for (char **papszIter = papszDomainList;
                     papszIter && *papszIter; ++papszIter)
                {
                    const char *pszDomain = *papszIter;
                    if (pszDomain[0] != 0 &&
                        (!papszSrcMDD ||
                         CSLFindString(papszSrcMDD, pszDomain) >= 0))
                    {
                        l_oMDMD.SetMetadata(poSrcDS->GetMetadata(pszDomain),
                                            pszDomain);
                    }
                }
                CSLDestroy(papszDomainList);
            }

            WriteMDMetadata(&l_oMDMD, l_hTIFF, &psRoot, &psTail, 0, eProfile);
        }
        CSLDestroy(papszSrcMDD);
    }

    if (!bExcludeRPBandIMGFileWriting)
    {
        WriteRPC(poSrcDS, l_hTIFF, bSrcIsGeoTIFF, eProfile, pszTIFFFilename,
                 papszCreationOptions);

        /* --------------------------------------------------------------------
         */
        /*      Handle metadata data written to an IMD file. */
        /* --------------------------------------------------------------------
         */
        char **papszIMDMD = poSrcDS->GetMetadata(MD_DOMAIN_IMD);
        if (papszIMDMD != nullptr)
        {
            GDALWriteIMDFile(pszTIFFFilename, papszIMDMD);
        }
    }

    uint16_t nPhotometric = 0;
    if (!TIFFGetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, &(nPhotometric)))
        nPhotometric = PHOTOMETRIC_MINISBLACK;

    const bool bStandardColorInterp = GTIFFIsStandardColorInterpretation(
        GDALDataset::ToHandle(poSrcDS), nPhotometric, papszCreationOptions);

    /* -------------------------------------------------------------------- */
    /*      We also need to address band specific metadata, and special     */
    /*      "role" metadata.                                                */
    /* -------------------------------------------------------------------- */
    for (int nBand = 1; nBand <= poSrcDS->GetRasterCount(); ++nBand)
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand(nBand);

        if (bSrcIsGeoTIFF)
        {
            GTiffRasterBand *poSrcBandGTiff =
                cpl::down_cast<GTiffRasterBand *>(poBand);
            assert(poSrcBandGTiff);
            WriteMDMetadata(&poSrcBandGTiff->m_oGTiffMDMD, l_hTIFF, &psRoot,
                            &psTail, nBand, eProfile);
        }
        else
        {
            char **papszMD = poBand->GetMetadata();

            if (CSLCount(papszMD) > 0)
            {
                GDALMultiDomainMetadata l_oMDMD;
                l_oMDMD.SetMetadata(papszMD);

                WriteMDMetadata(&l_oMDMD, l_hTIFF, &psRoot, &psTail, nBand,
                                eProfile);
            }
        }

        const double dfOffset = poBand->GetOffset();
        const double dfScale = poBand->GetScale();
        bool bGeoTIFFScaleOffsetInZ = false;
        double adfGeoTransform[6];
        // Check if we have already encoded scale/offset in the GeoTIFF tags
        if (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None &&
            adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 &&
            adfGeoTransform[5] < 0.0 && poSrcDS->GetSpatialRef() &&
            poSrcDS->GetSpatialRef()->IsVertical() &&
            poSrcDS->GetRasterCount() == 1)
        {
            bGeoTIFFScaleOffsetInZ = true;
        }

        if ((dfOffset != 0.0 || dfScale != 1.0) && !bGeoTIFFScaleOffsetInZ)
        {
            char szValue[128] = {};

            CPLsnprintf(szValue, sizeof(szValue), "%.18g", dfOffset);
            AppendMetadataItem(&psRoot, &psTail, "OFFSET", szValue, nBand,
                               "offset", "");
            CPLsnprintf(szValue, sizeof(szValue), "%.18g", dfScale);
            AppendMetadataItem(&psRoot, &psTail, "SCALE", szValue, nBand,
                               "scale", "");
        }

        const char *pszUnitType = poBand->GetUnitType();
        if (pszUnitType != nullptr && pszUnitType[0] != '\0')
        {
            bool bWriteUnit = true;
            auto poSRS = poSrcDS->GetSpatialRef();
            if (poSRS && poSRS->IsCompound())
            {
                const char *pszVertUnit = nullptr;
                poSRS->GetTargetLinearUnits("COMPD_CS|VERT_CS", &pszVertUnit);
                if (pszVertUnit && EQUAL(pszVertUnit, pszUnitType))
                {
                    bWriteUnit = false;
                }
            }
            if (bWriteUnit)
            {
                AppendMetadataItem(&psRoot, &psTail, "UNITTYPE", pszUnitType,
                                   nBand, "unittype", "");
            }
        }

        if (strlen(poBand->GetDescription()) > 0)
        {
            AppendMetadataItem(&psRoot, &psTail, "DESCRIPTION",
                               poBand->GetDescription(), nBand, "description",
                               "");
        }

        if (!bStandardColorInterp &&
            !(nBand <= 3 && EQUAL(CSLFetchNameValueDef(papszCreationOptions,
                                                       "PHOTOMETRIC", ""),
                                  "RGB")))
        {
            AppendMetadataItem(&psRoot, &psTail, "COLORINTERP",
                               GDALGetColorInterpretationName(
                                   poBand->GetColorInterpretation()),
                               nBand, "colorinterp", "");
        }
    }

    const char *pszTilingSchemeName =
        CSLFetchNameValue(papszCreationOptions, "@TILING_SCHEME_NAME");
    if (pszTilingSchemeName)
    {
        AppendMetadataItem(&psRoot, &psTail, "NAME", pszTilingSchemeName, 0,
                           nullptr, "TILING_SCHEME");

        const char *pszZoomLevel = CSLFetchNameValue(
            papszCreationOptions, "@TILING_SCHEME_ZOOM_LEVEL");
        if (pszZoomLevel)
        {
            AppendMetadataItem(&psRoot, &psTail, "ZOOM_LEVEL", pszZoomLevel, 0,
                               nullptr, "TILING_SCHEME");
        }

        const char *pszAlignedLevels = CSLFetchNameValue(
            papszCreationOptions, "@TILING_SCHEME_ALIGNED_LEVELS");
        if (pszAlignedLevels)
        {
            AppendMetadataItem(&psRoot, &psTail, "ALIGNED_LEVELS",
                               pszAlignedLevels, 0, nullptr, "TILING_SCHEME");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Write information about some codecs.                            */
    /* -------------------------------------------------------------------- */
    if (CPLTestBool(
            CPLGetConfigOption("GTIFF_WRITE_IMAGE_STRUCTURE_METADATA", "YES")))
    {
        const char *pszCompress =
            CSLFetchNameValue(papszCreationOptions, "COMPRESS");
        if (pszCompress && EQUAL(pszCompress, "WEBP"))
        {
            if (GTiffGetWebPLossless(papszCreationOptions))
            {
                AppendMetadataItem(&psRoot, &psTail,
                                   "COMPRESSION_REVERSIBILITY", "LOSSLESS", 0,
                                   nullptr, "IMAGE_STRUCTURE");
            }
            else
            {
                AppendMetadataItem(
                    &psRoot, &psTail, "WEBP_LEVEL",
                    CPLSPrintf("%d", GTiffGetWebPLevel(papszCreationOptions)),
                    0, nullptr, "IMAGE_STRUCTURE");
            }
        }
        else if (pszCompress && STARTS_WITH_CI(pszCompress, "LERC"))
        {
            const double dfMaxZError =
                GTiffGetLERCMaxZError(papszCreationOptions);
            const double dfMaxZErrorOverview =
                GTiffGetLERCMaxZErrorOverview(papszCreationOptions);
            if (dfMaxZError == 0.0 && dfMaxZErrorOverview == 0.0)
            {
                AppendMetadataItem(&psRoot, &psTail,
                                   "COMPRESSION_REVERSIBILITY", "LOSSLESS", 0,
                                   nullptr, "IMAGE_STRUCTURE");
            }
            else
            {
                AppendMetadataItem(&psRoot, &psTail, "MAX_Z_ERROR",
                                   CSLFetchNameValueDef(papszCreationOptions,
                                                        "MAX_Z_ERROR", ""),
                                   0, nullptr, "IMAGE_STRUCTURE");
                if (dfMaxZError != dfMaxZErrorOverview)
                {
                    AppendMetadataItem(
                        &psRoot, &psTail, "MAX_Z_ERROR_OVERVIEW",
                        CSLFetchNameValueDef(papszCreationOptions,
                                             "MAX_Z_ERROR_OVERVIEW", ""),
                        0, nullptr, "IMAGE_STRUCTURE");
                }
            }
        }
#if HAVE_JXL
        else if (pszCompress && EQUAL(pszCompress, "JXL"))
        {
            float fDistance = 0.0f;
            if (GTiffGetJXLLossless(papszCreationOptions))
            {
                AppendMetadataItem(&psRoot, &psTail,
                                   "COMPRESSION_REVERSIBILITY", "LOSSLESS", 0,
                                   nullptr, "IMAGE_STRUCTURE");
            }
            else
            {
                fDistance = GTiffGetJXLDistance(papszCreationOptions);
                AppendMetadataItem(&psRoot, &psTail, "JXL_DISTANCE",
                                   CPLSPrintf("%f", fDistance), 0, nullptr,
                                   "IMAGE_STRUCTURE");
            }
            const float fAlphaDistance =
                GTiffGetJXLAlphaDistance(papszCreationOptions);
            if (fAlphaDistance >= 0.0f && fAlphaDistance != fDistance)
            {
                AppendMetadataItem(&psRoot, &psTail, "JXL_ALPHA_DISTANCE",
                                   CPLSPrintf("%f", fAlphaDistance), 0, nullptr,
                                   "IMAGE_STRUCTURE");
            }
            AppendMetadataItem(
                &psRoot, &psTail, "JXL_EFFORT",
                CPLSPrintf("%d", GTiffGetJXLEffort(papszCreationOptions)), 0,
                nullptr, "IMAGE_STRUCTURE");
        }
#endif
    }

    /* -------------------------------------------------------------------- */
    /*      Write out the generic XML metadata if there is any.             */
    /* -------------------------------------------------------------------- */
    if (psRoot != nullptr)
    {
        bool bRet = true;

        if (eProfile == GTiffProfile::GDALGEOTIFF)
        {
            char *pszXML_MD = CPLSerializeXMLTree(psRoot);
            TIFFSetField(l_hTIFF, TIFFTAG_GDAL_METADATA, pszXML_MD);
            CPLFree(pszXML_MD);
        }
        else
        {
            if (bSrcIsGeoTIFF)
                cpl::down_cast<GTiffDataset *>(poSrcDS)->PushMetadataToPam();
            else
                bRet = false;
        }

        CPLDestroyXMLNode(psRoot);

        return bRet;
    }

    // If we have no more metadata but it existed before,
    // remove the GDAL_METADATA tag.
    if (eProfile == GTiffProfile::GDALGEOTIFF)
    {
        char *pszText = nullptr;
        if (TIFFGetField(l_hTIFF, TIFFTAG_GDAL_METADATA, &pszText))
        {
            TIFFUnsetField(l_hTIFF, TIFFTAG_GDAL_METADATA);
        }
    }

    return true;
}

/************************************************************************/
/*                         PushMetadataToPam()                          */
/*                                                                      */
/*      When producing a strict profile TIFF or if our aggregate        */
/*      metadata is too big for a single tiff tag we may end up         */
/*      needing to write it via the PAM mechanisms.  This method        */
/*      copies all the appropriate metadata into the PAM level          */
/*      metadata object but with special care to avoid copying          */
/*      metadata handled in other ways in TIFF format.                  */
/************************************************************************/

void GTiffDataset::PushMetadataToPam()

{
    if (GetPamFlags() & GPF_DISABLED)
        return;

    const bool bStandardColorInterp = GTIFFIsStandardColorInterpretation(
        GDALDataset::ToHandle(this), m_nPhotometric, m_papszCreationOptions);

    for (int nBand = 0; nBand <= GetRasterCount(); ++nBand)
    {
        GDALMultiDomainMetadata *poSrcMDMD = nullptr;
        GTiffRasterBand *poBand = nullptr;

        if (nBand == 0)
        {
            poSrcMDMD = &(this->m_oGTiffMDMD);
        }
        else
        {
            poBand = cpl::down_cast<GTiffRasterBand *>(GetRasterBand(nBand));
            poSrcMDMD = &(poBand->m_oGTiffMDMD);
        }

        /* --------------------------------------------------------------------
         */
        /*      Loop over the available domains. */
        /* --------------------------------------------------------------------
         */
        CSLConstList papszDomainList = poSrcMDMD->GetDomainList();
        for (int iDomain = 0; papszDomainList && papszDomainList[iDomain];
             ++iDomain)
        {
            char **papszMD = poSrcMDMD->GetMetadata(papszDomainList[iDomain]);

            if (EQUAL(papszDomainList[iDomain], MD_DOMAIN_RPC) ||
                EQUAL(papszDomainList[iDomain], MD_DOMAIN_IMD) ||
                EQUAL(papszDomainList[iDomain], "_temporary_") ||
                EQUAL(papszDomainList[iDomain], "IMAGE_STRUCTURE") ||
                EQUAL(papszDomainList[iDomain], "COLOR_PROFILE"))
                continue;

            papszMD = CSLDuplicate(papszMD);

            for (int i = CSLCount(papszMD) - 1; i >= 0; --i)
            {
                if (STARTS_WITH_CI(papszMD[i], "TIFFTAG_") ||
                    EQUALN(papszMD[i], GDALMD_AREA_OR_POINT,
                           strlen(GDALMD_AREA_OR_POINT)))
                    papszMD = CSLRemoveStrings(papszMD, i, 1, nullptr);
            }

            if (nBand == 0)
                GDALPamDataset::SetMetadata(papszMD, papszDomainList[iDomain]);
            else
                poBand->GDALPamRasterBand::SetMetadata(
                    papszMD, papszDomainList[iDomain]);

            CSLDestroy(papszMD);
        }

        /* --------------------------------------------------------------------
         */
        /*      Handle some "special domain" stuff. */
        /* --------------------------------------------------------------------
         */
        if (poBand != nullptr)
        {
            poBand->GDALPamRasterBand::SetOffset(poBand->GetOffset());
            poBand->GDALPamRasterBand::SetScale(poBand->GetScale());
            poBand->GDALPamRasterBand::SetUnitType(poBand->GetUnitType());
            poBand->GDALPamRasterBand::SetDescription(poBand->GetDescription());
            if (!bStandardColorInterp)
            {
                poBand->GDALPamRasterBand::SetColorInterpretation(
                    poBand->GetColorInterpretation());
            }
        }
    }
    MarkPamDirty();
}

/************************************************************************/
/*                         WriteNoDataValue()                           */
/************************************************************************/

void GTiffDataset::WriteNoDataValue(TIFF *hTIFF, double dfNoData)

{
    CPLString osVal(GTiffFormatGDALNoDataTagValue(dfNoData));
    TIFFSetField(hTIFF, TIFFTAG_GDAL_NODATA, osVal.c_str());
}

void GTiffDataset::WriteNoDataValue(TIFF *hTIFF, int64_t nNoData)

{
    TIFFSetField(hTIFF, TIFFTAG_GDAL_NODATA,
                 CPLSPrintf(CPL_FRMT_GIB, static_cast<GIntBig>(nNoData)));
}

void GTiffDataset::WriteNoDataValue(TIFF *hTIFF, uint64_t nNoData)

{
    TIFFSetField(hTIFF, TIFFTAG_GDAL_NODATA,
                 CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nNoData)));
}

/************************************************************************/
/*                         UnsetNoDataValue()                           */
/************************************************************************/

void GTiffDataset::UnsetNoDataValue(TIFF *l_hTIFF)

{
    TIFFUnsetField(l_hTIFF, TIFFTAG_GDAL_NODATA);
}

/************************************************************************/
/*                             SaveICCProfile()                         */
/*                                                                      */
/*      Save ICC Profile or colorimetric data into file                 */
/* pDS:                                                                 */
/*      Dataset that contains the metadata with the ICC or colorimetric */
/*      data. If this argument is specified, all other arguments are    */
/*      ignored. Set them to NULL or 0.                                 */
/* hTIFF:                                                               */
/*      Pointer to TIFF handle. Only needed if pDS is NULL or           */
/*      pDS->m_hTIFF is NULL.                                             */
/* papszParamList:                                                       */
/*      Options containing the ICC profile or colorimetric metadata.    */
/*      Ignored if pDS is not NULL.                                     */
/* nBitsPerSample:                                                      */
/*      Bits per sample. Ignored if pDS is not NULL.                    */
/************************************************************************/

void GTiffDataset::SaveICCProfile(GTiffDataset *pDS, TIFF *l_hTIFF,
                                  char **papszParamList,
                                  uint32_t l_nBitsPerSample)
{
    if ((pDS != nullptr) && (pDS->eAccess != GA_Update))
        return;

    if (l_hTIFF == nullptr)
    {
        if (pDS == nullptr)
            return;

        l_hTIFF = pDS->m_hTIFF;
        if (l_hTIFF == nullptr)
            return;
    }

    if ((papszParamList == nullptr) && (pDS == nullptr))
        return;

    const char *pszValue = nullptr;
    if (pDS != nullptr)
        pszValue = pDS->GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE");
    else
        pszValue = CSLFetchNameValue(papszParamList, "SOURCE_ICC_PROFILE");
    if (pszValue != nullptr)
    {
        char *pEmbedBuffer = CPLStrdup(pszValue);
        int32_t nEmbedLen =
            CPLBase64DecodeInPlace(reinterpret_cast<GByte *>(pEmbedBuffer));

        TIFFSetField(l_hTIFF, TIFFTAG_ICCPROFILE, nEmbedLen, pEmbedBuffer);

        CPLFree(pEmbedBuffer);
    }
    else
    {
        // Output colorimetric data.
        float pCHR[6] = {};     // Primaries.
        uint16_t pTXR[6] = {};  // Transfer range.
        const char *pszCHRNames[] = {"SOURCE_PRIMARIES_RED",
                                     "SOURCE_PRIMARIES_GREEN",
                                     "SOURCE_PRIMARIES_BLUE"};
        const char *pszTXRNames[] = {"TIFFTAG_TRANSFERRANGE_BLACK",
                                     "TIFFTAG_TRANSFERRANGE_WHITE"};

        // Output chromacities.
        bool bOutputCHR = true;
        for (int i = 0; i < 3 && bOutputCHR; ++i)
        {
            if (pDS != nullptr)
                pszValue =
                    pDS->GetMetadataItem(pszCHRNames[i], "COLOR_PROFILE");
            else
                pszValue = CSLFetchNameValue(papszParamList, pszCHRNames[i]);
            if (pszValue == nullptr)
            {
                bOutputCHR = false;
                break;
            }

            char **papszTokens = CSLTokenizeString2(pszValue, ",",
                                                    CSLT_ALLOWEMPTYTOKENS |
                                                        CSLT_STRIPLEADSPACES |
                                                        CSLT_STRIPENDSPACES);

            if (CSLCount(papszTokens) != 3)
            {
                bOutputCHR = false;
                CSLDestroy(papszTokens);
                break;
            }

            for (int j = 0; j < 3; ++j)
            {
                float v = static_cast<float>(CPLAtof(papszTokens[j]));

                if (j == 2)
                {
                    // Last term of xyY color must be 1.0.
                    if (v != 1.0)
                    {
                        bOutputCHR = false;
                        break;
                    }
                }
                else
                {
                    pCHR[i * 2 + j] = v;
                }
            }

            CSLDestroy(papszTokens);
        }

        if (bOutputCHR)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PRIMARYCHROMATICITIES, pCHR);
        }

        // Output whitepoint.
        if (pDS != nullptr)
            pszValue =
                pDS->GetMetadataItem("SOURCE_WHITEPOINT", "COLOR_PROFILE");
        else
            pszValue = CSLFetchNameValue(papszParamList, "SOURCE_WHITEPOINT");
        if (pszValue != nullptr)
        {
            char **papszTokens = CSLTokenizeString2(pszValue, ",",
                                                    CSLT_ALLOWEMPTYTOKENS |
                                                        CSLT_STRIPLEADSPACES |
                                                        CSLT_STRIPENDSPACES);

            bool bOutputWhitepoint = true;
            float pWP[2] = {0.0f, 0.0f};  // Whitepoint
            if (CSLCount(papszTokens) != 3)
            {
                bOutputWhitepoint = false;
            }
            else
            {
                for (int j = 0; j < 3; ++j)
                {
                    const float v = static_cast<float>(CPLAtof(papszTokens[j]));

                    if (j == 2)
                    {
                        // Last term of xyY color must be 1.0.
                        if (v != 1.0)
                        {
                            bOutputWhitepoint = false;
                            break;
                        }
                    }
                    else
                    {
                        pWP[j] = v;
                    }
                }
            }
            CSLDestroy(papszTokens);

            if (bOutputWhitepoint)
            {
                TIFFSetField(l_hTIFF, TIFFTAG_WHITEPOINT, pWP);
            }
        }

        // Set transfer function metadata.
        char const *pszTFRed = nullptr;
        if (pDS != nullptr)
            pszTFRed = pDS->GetMetadataItem("TIFFTAG_TRANSFERFUNCTION_RED",
                                            "COLOR_PROFILE");
        else
            pszTFRed = CSLFetchNameValue(papszParamList,
                                         "TIFFTAG_TRANSFERFUNCTION_RED");

        char const *pszTFGreen = nullptr;
        if (pDS != nullptr)
            pszTFGreen = pDS->GetMetadataItem("TIFFTAG_TRANSFERFUNCTION_GREEN",
                                              "COLOR_PROFILE");
        else
            pszTFGreen = CSLFetchNameValue(papszParamList,
                                           "TIFFTAG_TRANSFERFUNCTION_GREEN");

        char const *pszTFBlue = nullptr;
        if (pDS != nullptr)
            pszTFBlue = pDS->GetMetadataItem("TIFFTAG_TRANSFERFUNCTION_BLUE",
                                             "COLOR_PROFILE");
        else
            pszTFBlue = CSLFetchNameValue(papszParamList,
                                          "TIFFTAG_TRANSFERFUNCTION_BLUE");

        if ((pszTFRed != nullptr) && (pszTFGreen != nullptr) &&
            (pszTFBlue != nullptr))
        {
            // Get length of table.
            const int nTransferFunctionLength =
                1 << ((pDS != nullptr) ? pDS->m_nBitsPerSample
                                       : l_nBitsPerSample);

            char **papszTokensRed = CSLTokenizeString2(
                pszTFRed, ",",
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES |
                    CSLT_STRIPENDSPACES);
            char **papszTokensGreen = CSLTokenizeString2(
                pszTFGreen, ",",
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES |
                    CSLT_STRIPENDSPACES);
            char **papszTokensBlue = CSLTokenizeString2(
                pszTFBlue, ",",
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES |
                    CSLT_STRIPENDSPACES);

            if ((CSLCount(papszTokensRed) == nTransferFunctionLength) &&
                (CSLCount(papszTokensGreen) == nTransferFunctionLength) &&
                (CSLCount(papszTokensBlue) == nTransferFunctionLength))
            {
                uint16_t *pTransferFuncRed = static_cast<uint16_t *>(
                    CPLMalloc(sizeof(uint16_t) * nTransferFunctionLength));
                uint16_t *pTransferFuncGreen = static_cast<uint16_t *>(
                    CPLMalloc(sizeof(uint16_t) * nTransferFunctionLength));
                uint16_t *pTransferFuncBlue = static_cast<uint16_t *>(
                    CPLMalloc(sizeof(uint16_t) * nTransferFunctionLength));

                // Convert our table in string format into int16_t format.
                for (int i = 0; i < nTransferFunctionLength; ++i)
                {
                    pTransferFuncRed[i] =
                        static_cast<uint16_t>(atoi(papszTokensRed[i]));
                    pTransferFuncGreen[i] =
                        static_cast<uint16_t>(atoi(papszTokensGreen[i]));
                    pTransferFuncBlue[i] =
                        static_cast<uint16_t>(atoi(papszTokensBlue[i]));
                }

                TIFFSetField(l_hTIFF, TIFFTAG_TRANSFERFUNCTION,
                             pTransferFuncRed, pTransferFuncGreen,
                             pTransferFuncBlue);

                CPLFree(pTransferFuncRed);
                CPLFree(pTransferFuncGreen);
                CPLFree(pTransferFuncBlue);
            }

            CSLDestroy(papszTokensRed);
            CSLDestroy(papszTokensGreen);
            CSLDestroy(papszTokensBlue);
        }

        // Output transfer range.
        bool bOutputTransferRange = true;
        for (int i = 0; (i < 2) && bOutputTransferRange; ++i)
        {
            if (pDS != nullptr)
                pszValue =
                    pDS->GetMetadataItem(pszTXRNames[i], "COLOR_PROFILE");
            else
                pszValue = CSLFetchNameValue(papszParamList, pszTXRNames[i]);
            if (pszValue == nullptr)
            {
                bOutputTransferRange = false;
                break;
            }

            char **papszTokens = CSLTokenizeString2(pszValue, ",",
                                                    CSLT_ALLOWEMPTYTOKENS |
                                                        CSLT_STRIPLEADSPACES |
                                                        CSLT_STRIPENDSPACES);

            if (CSLCount(papszTokens) != 3)
            {
                bOutputTransferRange = false;
                CSLDestroy(papszTokens);
                break;
            }

            for (int j = 0; j < 3; ++j)
            {
                pTXR[i + j * 2] = static_cast<uint16_t>(atoi(papszTokens[j]));
            }

            CSLDestroy(papszTokens);
        }

        if (bOutputTransferRange)
        {
            const int TIFFTAG_TRANSFERRANGE = 0x0156;
            TIFFSetField(l_hTIFF, TIFFTAG_TRANSFERRANGE, pTXR);
        }
    }
}

static signed char GTiffGetLZMAPreset(char **papszOptions)
{
    int nLZMAPreset = -1;
    const char *pszValue = CSLFetchNameValue(papszOptions, "LZMA_PRESET");
    if (pszValue != nullptr)
    {
        nLZMAPreset = atoi(pszValue);
        if (!(nLZMAPreset >= 0 && nLZMAPreset <= 9))
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "LZMA_PRESET=%s value not recognised, ignoring.",
                     pszValue);
            nLZMAPreset = -1;
        }
    }
    return static_cast<signed char>(nLZMAPreset);
}

static signed char GTiffGetZSTDPreset(char **papszOptions)
{
    int nZSTDLevel = -1;
    const char *pszValue = CSLFetchNameValue(papszOptions, "ZSTD_LEVEL");
    if (pszValue != nullptr)
    {
        nZSTDLevel = atoi(pszValue);
        if (!(nZSTDLevel >= 1 && nZSTDLevel <= 22))
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "ZSTD_LEVEL=%s value not recognised, ignoring.", pszValue);
            nZSTDLevel = -1;
        }
    }
    return static_cast<signed char>(nZSTDLevel);
}

static signed char GTiffGetZLevel(char **papszOptions)
{
    int nZLevel = -1;
    const char *pszValue = CSLFetchNameValue(papszOptions, "ZLEVEL");
    if (pszValue != nullptr)
    {
        nZLevel = atoi(pszValue);
#ifdef TIFFTAG_DEFLATE_SUBCODEC
        constexpr int nMaxLevel = 12;
#ifndef LIBDEFLATE_SUPPORT
        if (nZLevel > 9 && nZLevel <= nMaxLevel)
        {
            CPLDebug("GTiff",
                     "ZLEVEL=%d not supported in a non-libdeflate enabled "
                     "libtiff build. Capping to 9",
                     nZLevel);
            nZLevel = 9;
        }
#endif
#else
        constexpr int nMaxLevel = 9;
#endif
        if (nZLevel < 1 || nZLevel > nMaxLevel)
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "ZLEVEL=%s value not recognised, ignoring.", pszValue);
            nZLevel = -1;
        }
    }
    return static_cast<signed char>(nZLevel);
}

static signed char GTiffGetJpegQuality(char **papszOptions)
{
    int nJpegQuality = -1;
    const char *pszValue = CSLFetchNameValue(papszOptions, "JPEG_QUALITY");
    if (pszValue != nullptr)
    {
        nJpegQuality = atoi(pszValue);
        if (nJpegQuality < 1 || nJpegQuality > 100)
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "JPEG_QUALITY=%s value not recognised, ignoring.",
                     pszValue);
            nJpegQuality = -1;
        }
    }
    return static_cast<signed char>(nJpegQuality);
}

static signed char GTiffGetJpegTablesMode(char **papszOptions)
{
    return static_cast<signed char>(atoi(
        CSLFetchNameValueDef(papszOptions, "JPEGTABLESMODE",
                             CPLSPrintf("%d", knGTIFFJpegTablesModeDefault))));
}

/************************************************************************/
/*                        GetDiscardLsbOption()                         */
/************************************************************************/

static GTiffDataset::MaskOffset *GetDiscardLsbOption(TIFF *hTIFF,
                                                     char **papszOptions)
{
    const char *pszBits = CSLFetchNameValue(papszOptions, "DISCARD_LSB");
    if (pszBits == nullptr)
        return nullptr;

    uint16_t nPhotometric = 0;
    TIFFGetFieldDefaulted(hTIFF, TIFFTAG_PHOTOMETRIC, &nPhotometric);

    uint16_t nBitsPerSample = 0;
    if (!TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &nBitsPerSample))
        nBitsPerSample = 1;

    uint16_t nSamplesPerPixel = 0;
    if (!TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSamplesPerPixel))
        nSamplesPerPixel = 1;

    uint16_t nSampleFormat = 0;
    if (!TIFFGetField(hTIFF, TIFFTAG_SAMPLEFORMAT, &nSampleFormat))
        nSampleFormat = SAMPLEFORMAT_UINT;

    if (nPhotometric == PHOTOMETRIC_PALETTE)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DISCARD_LSB ignored on a paletted image");
        return nullptr;
    }
    if (!(nBitsPerSample == 8 || nBitsPerSample == 16 || nBitsPerSample == 32 ||
          nBitsPerSample == 64))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DISCARD_LSB ignored on non 8, 16, 32 or 64 bits images");
        return nullptr;
    }

    const CPLStringList aosTokens(CSLTokenizeString2(pszBits, ",", 0));
    const int nTokens = aosTokens.size();
    GTiffDataset::MaskOffset *panMaskOffsetLsb = nullptr;
    if (nTokens == 1 || nTokens == nSamplesPerPixel)
    {
        panMaskOffsetLsb = static_cast<GTiffDataset::MaskOffset *>(
            CPLCalloc(nSamplesPerPixel, sizeof(GTiffDataset::MaskOffset)));
        for (int i = 0; i < nSamplesPerPixel; ++i)
        {
            const int nBits = atoi(aosTokens[nTokens == 1 ? 0 : i]);
            const int nMaxBits =
                (nSampleFormat == SAMPLEFORMAT_IEEEFP && nBits == 32)   ? 23 - 1
                : (nSampleFormat == SAMPLEFORMAT_IEEEFP && nBits == 64) ? 53 - 1
                : nSampleFormat == SAMPLEFORMAT_INT ? nBitsPerSample - 2
                                                    : nBitsPerSample - 1;

            if (nBits < 0 || nBits > nMaxBits)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "DISCARD_LSB ignored: values should be in [0,%d] range",
                    nMaxBits);
                VSIFree(panMaskOffsetLsb);
                return nullptr;
            }
            panMaskOffsetLsb[i].nMask =
                ~((static_cast<uint64_t>(1) << nBits) - 1);
            if (nBits > 1)
            {
                panMaskOffsetLsb[i].nRoundUpBitTest = static_cast<uint64_t>(1)
                                                      << (nBits - 1);
            }
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DISCARD_LSB ignored: wrong number of components");
    }
    return panMaskOffsetLsb;
}

void GTiffDataset::GetDiscardLsbOption(char **papszOptions)
{
    m_panMaskOffsetLsb = ::GetDiscardLsbOption(m_hTIFF, papszOptions);
}

/************************************************************************/
/*                             GetProfile()                             */
/************************************************************************/

static GTiffProfile GetProfile(const char *pszProfile)
{
    GTiffProfile eProfile = GTiffProfile::GDALGEOTIFF;
    if (pszProfile != nullptr)
    {
        if (EQUAL(pszProfile, szPROFILE_BASELINE))
            eProfile = GTiffProfile::BASELINE;
        else if (EQUAL(pszProfile, szPROFILE_GeoTIFF))
            eProfile = GTiffProfile::GEOTIFF;
        else if (!EQUAL(pszProfile, szPROFILE_GDALGeoTIFF))
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for PROFILE: %s", pszProfile);
        }
    }
    return eProfile;
}

/************************************************************************/
/*                            GTiffCreate()                             */
/*                                                                      */
/*      Shared functionality between GTiffDataset::Create() and         */
/*      GTiffCreateCopy() for creating TIFF file based on a set of      */
/*      options and a configuration.                                    */
/************************************************************************/

TIFF *GTiffDataset::CreateLL(const char *pszFilename, int nXSize, int nYSize,
                             int l_nBands, GDALDataType eType,
                             double dfExtraSpaceForOverviews,
                             char **papszParamList, VSILFILE **pfpL,
                             CPLString &l_osTmpFilename)

{
    GTiffOneTimeInit();

    /* -------------------------------------------------------------------- */
    /*      Blow on a few errors.                                           */
    /* -------------------------------------------------------------------- */
    if (nXSize < 1 || nYSize < 1 || l_nBands < 1)
    {
        ReportError(
            pszFilename, CE_Failure, CPLE_AppDefined,
            "Attempt to create %dx%dx%d TIFF file, but width, height and bands"
            "must be positive.",
            nXSize, nYSize, l_nBands);

        return nullptr;
    }

    if (l_nBands > 65535)
    {
        ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
                    "Attempt to create %dx%dx%d TIFF file, but bands "
                    "must be lesser or equal to 65535.",
                    nXSize, nYSize, l_nBands);

        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Setup values based on options.                                  */
    /* -------------------------------------------------------------------- */
    const GTiffProfile eProfile =
        GetProfile(CSLFetchNameValue(papszParamList, "PROFILE"));

    const bool bTiled = CPLFetchBool(papszParamList, "TILED", false);

    int l_nBlockXSize = 0;
    const char *pszValue = CSLFetchNameValue(papszParamList, "BLOCKXSIZE");
    if (pszValue != nullptr)
    {
        l_nBlockXSize = atoi(pszValue);
        if (l_nBlockXSize < 0)
        {
            ReportError(pszFilename, CE_Failure, CPLE_IllegalArg,
                        "Invalid value for BLOCKXSIZE");
            return nullptr;
        }
    }

    int l_nBlockYSize = 0;
    pszValue = CSLFetchNameValue(papszParamList, "BLOCKYSIZE");
    if (pszValue != nullptr)
    {
        l_nBlockYSize = atoi(pszValue);
        if (l_nBlockYSize < 0)
        {
            ReportError(pszFilename, CE_Failure, CPLE_IllegalArg,
                        "Invalid value for BLOCKYSIZE");
            return nullptr;
        }
    }

    if (bTiled)
    {
        if (l_nBlockXSize == 0)
            l_nBlockXSize = 256;

        if (l_nBlockYSize == 0)
            l_nBlockYSize = 256;
    }

    int nPlanar = 0;
    pszValue = CSLFetchNameValue(papszParamList, "INTERLEAVE");
    if (pszValue != nullptr)
    {
        if (EQUAL(pszValue, "PIXEL"))
            nPlanar = PLANARCONFIG_CONTIG;
        else if (EQUAL(pszValue, "BAND"))
        {
            nPlanar = PLANARCONFIG_SEPARATE;
        }
        else
        {
            ReportError(
                pszFilename, CE_Failure, CPLE_IllegalArg,
                "INTERLEAVE=%s unsupported, value must be PIXEL or BAND.",
                pszValue);
            return nullptr;
        }
    }
    else
    {
        nPlanar = PLANARCONFIG_CONTIG;
    }

    int l_nCompression = COMPRESSION_NONE;
    pszValue = CSLFetchNameValue(papszParamList, "COMPRESS");
    if (pszValue != nullptr)
    {
        l_nCompression = GTIFFGetCompressionMethod(pszValue, "COMPRESS");
        if (l_nCompression < 0)
            return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      How many bits per sample?  We have a special case if NBITS      */
    /*      specified for GDT_Byte, GDT_UInt16, GDT_UInt32.                 */
    /* -------------------------------------------------------------------- */
    int l_nBitsPerSample = GDALGetDataTypeSizeBits(eType);
    if (CSLFetchNameValue(papszParamList, "NBITS") != nullptr)
    {
        int nMinBits = 0;
        int nMaxBits = 0;
        l_nBitsPerSample = atoi(CSLFetchNameValue(papszParamList, "NBITS"));
        if (eType == GDT_Byte)
        {
            nMinBits = 1;
            nMaxBits = 8;
        }
        else if (eType == GDT_UInt16)
        {
            nMinBits = 9;
            nMaxBits = 16;
        }
        else if (eType == GDT_UInt32)
        {
            nMinBits = 17;
            nMaxBits = 32;
        }
        else if (eType == GDT_Float32)
        {
            if (l_nBitsPerSample != 16 && l_nBitsPerSample != 32)
            {
                ReportError(pszFilename, CE_Warning, CPLE_NotSupported,
                            "Only NBITS=16 is supported for data type Float32");
                l_nBitsPerSample = GDALGetDataTypeSizeBits(eType);
            }
        }
        else
        {
            ReportError(pszFilename, CE_Warning, CPLE_NotSupported,
                        "NBITS is not supported for data type %s",
                        GDALGetDataTypeName(eType));
            l_nBitsPerSample = GDALGetDataTypeSizeBits(eType);
        }

        if (nMinBits != 0)
        {
            if (l_nBitsPerSample < nMinBits)
            {
                ReportError(
                    pszFilename, CE_Warning, CPLE_AppDefined,
                    "NBITS=%d is invalid for data type %s. Using NBITS=%d",
                    l_nBitsPerSample, GDALGetDataTypeName(eType), nMinBits);
                l_nBitsPerSample = nMinBits;
            }
            else if (l_nBitsPerSample > nMaxBits)
            {
                ReportError(
                    pszFilename, CE_Warning, CPLE_AppDefined,
                    "NBITS=%d is invalid for data type %s. Using NBITS=%d",
                    l_nBitsPerSample, GDALGetDataTypeName(eType), nMaxBits);
                l_nBitsPerSample = nMaxBits;
            }
        }
    }

#ifdef HAVE_JXL
    if (l_nCompression == COMPRESSION_JXL)
    {
        // Reflects tif_jxl's GetJXLDataType()
        if (eType != GDT_Byte && eType != GDT_UInt16 && eType != GDT_Float32)
        {
            ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                        "Data type %s not supported for JXL compression. Only "
                        "Byte, UInt16, Float32 are supported",
                        GDALGetDataTypeName(eType));
            return nullptr;
        }

        const struct
        {
            GDALDataType eDT;
            int nBitsPerSample;
        } asSupportedDTBitsPerSample[] = {
            {GDT_Byte, 8},
            {GDT_UInt16, 16},
            {GDT_Float32, 32},
        };

        for (const auto &sSupportedDTBitsPerSample : asSupportedDTBitsPerSample)
        {
            if (eType == sSupportedDTBitsPerSample.eDT &&
                l_nBitsPerSample != sSupportedDTBitsPerSample.nBitsPerSample)
            {
                ReportError(
                    pszFilename, CE_Failure, CPLE_NotSupported,
                    "Bits per sample=%d not supported for JXL compression. "
                    "Only %d is supported for %s data type.",
                    l_nBitsPerSample, sSupportedDTBitsPerSample.nBitsPerSample,
                    GDALGetDataTypeName(eType));
                return nullptr;
            }
        }
    }
#endif

    int nPredictor = PREDICTOR_NONE;
    pszValue = CSLFetchNameValue(papszParamList, "PREDICTOR");
    if (pszValue != nullptr)
    {
        nPredictor = atoi(pszValue);
    }

    // Do early checks as libtiff will only error out when starting to write.
    if (nPredictor != PREDICTOR_NONE &&
        CPLTestBool(CPLGetConfigOption("GDAL_GTIFF_PREDICTOR_CHECKS", "YES")))
    {
#if (TIFFLIB_VERSION > 20210416) || defined(INTERNAL_LIBTIFF)
#define HAVE_PREDICTOR_2_FOR_64BIT
#endif
        if (nPredictor == 2)
        {
            if (l_nBitsPerSample != 8 && l_nBitsPerSample != 16 &&
                l_nBitsPerSample != 32
#ifdef HAVE_PREDICTOR_2_FOR_64BIT
                && l_nBitsPerSample != 64
#endif
            )
            {
#if !defined(HAVE_PREDICTOR_2_FOR_64BIT)
                if (l_nBitsPerSample == 64)
                {
                    ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
                                "PREDICTOR=2 is supported on 64 bit samples "
                                "starting with libtiff > 4.3.0.");
                }
                else
#endif
                {
                    ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
#ifdef HAVE_PREDICTOR_2_FOR_64BIT
                                "PREDICTOR=2 is only supported with 8/16/32/64 "
                                "bit samples."
#else
                                "PREDICTOR=2 is only supported with 8/16/32 "
                                "bit samples."
#endif
                    );
                }
                return nullptr;
            }
        }
        else if (nPredictor == 3)
        {
            if (eType != GDT_Float32 && eType != GDT_Float64)
            {
                ReportError(
                    pszFilename, CE_Failure, CPLE_AppDefined,
                    "PREDICTOR=3 is only supported with Float32 or Float64.");
                return nullptr;
            }
        }
        else
        {
            ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
                        "PREDICTOR=%s is not supported.", pszValue);
            return nullptr;
        }
    }

    const int l_nZLevel = GTiffGetZLevel(papszParamList);
    const int l_nLZMAPreset = GTiffGetLZMAPreset(papszParamList);
    const int l_nZSTDLevel = GTiffGetZSTDPreset(papszParamList);
    const int l_nWebPLevel = GTiffGetWebPLevel(papszParamList);
    const bool l_bWebPLossless = GTiffGetWebPLossless(papszParamList);
    const int l_nJpegQuality = GTiffGetJpegQuality(papszParamList);
    const int l_nJpegTablesMode = GTiffGetJpegTablesMode(papszParamList);
    const double l_dfMaxZError = GTiffGetLERCMaxZError(papszParamList);
#if HAVE_JXL
    const bool l_bJXLLossless = GTiffGetJXLLossless(papszParamList);
    const uint32_t l_nJXLEffort = GTiffGetJXLEffort(papszParamList);
    const float l_fJXLDistance = GTiffGetJXLDistance(papszParamList);
    const float l_fJXLAlphaDistance = GTiffGetJXLAlphaDistance(papszParamList);
#endif
    /* -------------------------------------------------------------------- */
    /*      Streaming related code                                          */
    /* -------------------------------------------------------------------- */
    const CPLString osOriFilename(pszFilename);
    bool bStreaming = strcmp(pszFilename, "/vsistdout/") == 0 ||
                      CPLFetchBool(papszParamList, "STREAMABLE_OUTPUT", false);
#ifdef S_ISFIFO
    if (!bStreaming)
    {
        VSIStatBufL sStat;
        if (VSIStatExL(pszFilename, &sStat,
                       VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
            S_ISFIFO(sStat.st_mode))
        {
            bStreaming = true;
        }
    }
#endif
    if (bStreaming && !EQUAL("NONE", CSLFetchNameValueDef(papszParamList,
                                                          "COMPRESS", "NONE")))
    {
        ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                    "Streaming only supported to uncompressed TIFF");
        return nullptr;
    }
    if (bStreaming && CPLFetchBool(papszParamList, "SPARSE_OK", false))
    {
        ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                    "Streaming not supported with SPARSE_OK");
        return nullptr;
    }
    const bool bCopySrcOverviews =
        CPLFetchBool(papszParamList, "COPY_SRC_OVERVIEWS", false);
    if (bStreaming && bCopySrcOverviews)
    {
        ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                    "Streaming not supported with COPY_SRC_OVERVIEWS");
        return nullptr;
    }
    if (bStreaming)
    {
        static int nCounter = 0;
        l_osTmpFilename = CPLSPrintf("/vsimem/vsistdout_%d.tif", ++nCounter);
        pszFilename = l_osTmpFilename.c_str();
    }

    /* -------------------------------------------------------------------- */
    /*      Compute the uncompressed size.                                  */
    /* -------------------------------------------------------------------- */
    const unsigned nTileXCount =
        bTiled ? DIV_ROUND_UP(nXSize, l_nBlockXSize) : 0;
    const unsigned nTileYCount =
        bTiled ? DIV_ROUND_UP(nYSize, l_nBlockYSize) : 0;
    const double dfUncompressedImageSize =
        (bTiled ? (static_cast<double>(nTileXCount) * nTileYCount *
                   l_nBlockXSize * l_nBlockYSize)
                : (nXSize * static_cast<double>(nYSize))) *
            l_nBands * GDALGetDataTypeSizeBytes(eType) +
        dfExtraSpaceForOverviews;

    /* -------------------------------------------------------------------- */
    /*      Should the file be created as a bigtiff file?                   */
    /* -------------------------------------------------------------------- */
    const char *pszBIGTIFF = CSLFetchNameValue(papszParamList, "BIGTIFF");

    if (pszBIGTIFF == nullptr)
        pszBIGTIFF = "IF_NEEDED";

    bool bCreateBigTIFF = false;
    if (EQUAL(pszBIGTIFF, "IF_NEEDED"))
    {
        if (l_nCompression == COMPRESSION_NONE &&
            dfUncompressedImageSize > 4200000000.0)
            bCreateBigTIFF = true;
    }
    else if (EQUAL(pszBIGTIFF, "IF_SAFER"))
    {
        if (dfUncompressedImageSize > 2000000000.0)
            bCreateBigTIFF = true;
    }
    else
    {
        bCreateBigTIFF = CPLTestBool(pszBIGTIFF);
        if (!bCreateBigTIFF && l_nCompression == COMPRESSION_NONE &&
            dfUncompressedImageSize > 4200000000.0)
        {
            ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                        "The TIFF file will be larger than 4GB, so BigTIFF is "
                        "necessary.  Creation failed.");
            return nullptr;
        }
    }

    if (bCreateBigTIFF)
        CPLDebug("GTiff", "File being created as a BigTIFF.");

    /* -------------------------------------------------------------------- */
    /*      Sanity check.                                                   */
    /* -------------------------------------------------------------------- */
    if (bTiled)
    {
        // libtiff implementation limitation
        if (nTileXCount > 0x80000000U / (bCreateBigTIFF ? 8 : 4) / nTileYCount)
        {
            ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                        "File too large regarding tile size. This would result "
                        "in a file with tile arrays larger than 2GB");
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Check free space (only for big, non sparse, uncompressed)       */
    /* -------------------------------------------------------------------- */
    if (l_nCompression == COMPRESSION_NONE && dfUncompressedImageSize >= 1e9 &&
        !CPLFetchBool(papszParamList, "SPARSE_OK", false) &&
        osOriFilename != "/vsistdout/" &&
        osOriFilename != "/vsistdout_redirect/" &&
        CPLTestBool(CPLGetConfigOption("CHECK_DISK_FREE_SPACE", "TRUE")))
    {
        GIntBig nFreeDiskSpace =
            VSIGetDiskFreeSpace(CPLGetDirname(pszFilename));
        if (nFreeDiskSpace >= 0 && nFreeDiskSpace < dfUncompressedImageSize)
        {
            ReportError(pszFilename, CE_Failure, CPLE_FileIO,
                        "Free disk space available is " CPL_FRMT_GIB " bytes, "
                        "whereas " CPL_FRMT_GIB " are at least necessary. "
                        "You can disable this check by defining the "
                        "CHECK_DISK_FREE_SPACE configuration option to FALSE.",
                        nFreeDiskSpace,
                        static_cast<GIntBig>(dfUncompressedImageSize));
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Check if the user wishes a particular endianness                */
    /* -------------------------------------------------------------------- */

    int eEndianness = ENDIANNESS_NATIVE;
    pszValue = CSLFetchNameValue(papszParamList, "ENDIANNESS");
    if (pszValue == nullptr)
        pszValue = CPLGetConfigOption("GDAL_TIFF_ENDIANNESS", nullptr);
    if (pszValue != nullptr)
    {
        if (EQUAL(pszValue, "LITTLE"))
        {
            eEndianness = ENDIANNESS_LITTLE;
        }
        else if (EQUAL(pszValue, "BIG"))
        {
            eEndianness = ENDIANNESS_BIG;
        }
        else if (EQUAL(pszValue, "INVERTED"))
        {
#ifdef CPL_LSB
            eEndianness = ENDIANNESS_BIG;
#else
            eEndianness = ENDIANNESS_LITTLE;
#endif
        }
        else if (!EQUAL(pszValue, "NATIVE"))
        {
            ReportError(pszFilename, CE_Warning, CPLE_NotSupported,
                        "ENDIANNESS=%s not supported. Defaulting to NATIVE",
                        pszValue);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try opening the dataset.                                        */
    /* -------------------------------------------------------------------- */

    const bool bAppend =
        CPLFetchBool(papszParamList, "APPEND_SUBDATASET", false);

    char szOpeningFlag[5] = {};
    strcpy(szOpeningFlag, bAppend ? "r+" : "w+");
    if (bCreateBigTIFF)
        strcat(szOpeningFlag, "8");
    if (eEndianness == ENDIANNESS_BIG)
        strcat(szOpeningFlag, "b");
    else if (eEndianness == ENDIANNESS_LITTLE)
        strcat(szOpeningFlag, "l");

    VSILFILE *l_fpL = VSIFOpenL(pszFilename, bAppend ? "r+b" : "w+b");
    if (l_fpL == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Attempt to create new tiff file `%s' failed: %s", pszFilename,
                 VSIStrerror(errno));
        return nullptr;
    }
    TIFF *l_hTIFF = VSI_TIFFOpen(pszFilename, szOpeningFlag, l_fpL);
    if (l_hTIFF == nullptr)
    {
        if (CPLGetLastErrorNo() == 0)
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Attempt to create new tiff file `%s' "
                     "failed in XTIFFOpen().",
                     pszFilename);
        CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
        return nullptr;
    }

    if (bAppend)
    {
        // This is a bit of a hack to cause (*tif->tif_cleanup)(tif); to be
        // called. See https://trac.osgeo.org/gdal/ticket/2055
        TIFFSetField(l_hTIFF, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        TIFFFreeDirectory(l_hTIFF);
        TIFFCreateDirectory(l_hTIFF);
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have a custom pixel type (just used for signed byte now). */
    /* -------------------------------------------------------------------- */
    const char *pszPixelType = CSLFetchNameValue(papszParamList, "PIXELTYPE");
    if (pszPixelType == nullptr)
        pszPixelType = "";
    if (eType == GDT_Byte && EQUAL(pszPixelType, "SIGNEDBYTE"))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Using PIXELTYPE=SIGNEDBYTE with Byte data type is deprecated "
                 "(but still works). "
                 "Using Int8 data type instead is now recommended.");
    }

    /* -------------------------------------------------------------------- */
    /*      Setup some standard flags.                                      */
    /* -------------------------------------------------------------------- */
    TIFFSetField(l_hTIFF, TIFFTAG_IMAGEWIDTH, nXSize);
    TIFFSetField(l_hTIFF, TIFFTAG_IMAGELENGTH, nYSize);
    TIFFSetField(l_hTIFF, TIFFTAG_BITSPERSAMPLE, l_nBitsPerSample);

    uint16_t l_nSampleFormat = 0;
    if ((eType == GDT_Byte && EQUAL(pszPixelType, "SIGNEDBYTE")) ||
        eType == GDT_Int8 || eType == GDT_Int16 || eType == GDT_Int32 ||
        eType == GDT_Int64)
        l_nSampleFormat = SAMPLEFORMAT_INT;
    else if (eType == GDT_CInt16 || eType == GDT_CInt32)
        l_nSampleFormat = SAMPLEFORMAT_COMPLEXINT;
    else if (eType == GDT_Float32 || eType == GDT_Float64)
        l_nSampleFormat = SAMPLEFORMAT_IEEEFP;
    else if (eType == GDT_CFloat32 || eType == GDT_CFloat64)
        l_nSampleFormat = SAMPLEFORMAT_COMPLEXIEEEFP;
    else
        l_nSampleFormat = SAMPLEFORMAT_UINT;

    TIFFSetField(l_hTIFF, TIFFTAG_SAMPLEFORMAT, l_nSampleFormat);
    TIFFSetField(l_hTIFF, TIFFTAG_SAMPLESPERPIXEL, l_nBands);
    TIFFSetField(l_hTIFF, TIFFTAG_PLANARCONFIG, nPlanar);

    /* -------------------------------------------------------------------- */
    /*      Setup Photometric Interpretation. Take this value from the user */
    /*      passed option or guess correct value otherwise.                 */
    /* -------------------------------------------------------------------- */
    int nSamplesAccountedFor = 1;
    bool bForceColorTable = false;

    pszValue = CSLFetchNameValue(papszParamList, "PHOTOMETRIC");
    if (pszValue != nullptr)
    {
        if (EQUAL(pszValue, "MINISBLACK"))
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        else if (EQUAL(pszValue, "MINISWHITE"))
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
        }
        else if (EQUAL(pszValue, "PALETTE"))
        {
            if (eType == GDT_Byte || eType == GDT_UInt16)
            {
                TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
                nSamplesAccountedFor = 1;
                bForceColorTable = true;
            }
            else
            {
                ReportError(
                    pszFilename, CE_Warning, CPLE_AppDefined,
                    "PHOTOMETRIC=PALETTE only compatible with Byte or UInt16");
            }
        }
        else if (EQUAL(pszValue, "RGB"))
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            nSamplesAccountedFor = 3;
        }
        else if (EQUAL(pszValue, "CMYK"))
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
            nSamplesAccountedFor = 4;
        }
        else if (EQUAL(pszValue, "YCBCR"))
        {
            // Because of subsampling, setting YCBCR without JPEG compression
            // leads to a crash currently. Would need to make
            // GTiffRasterBand::IWriteBlock() aware of subsampling so that it
            // doesn't overrun buffer size returned by libtiff.
            if (l_nCompression != COMPRESSION_JPEG)
            {
                ReportError(
                    pszFilename, CE_Failure, CPLE_NotSupported,
                    "Currently, PHOTOMETRIC=YCBCR requires COMPRESS=JPEG");
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }

            if (nPlanar == PLANARCONFIG_SEPARATE)
            {
                ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                            "PHOTOMETRIC=YCBCR requires INTERLEAVE=PIXEL");
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }

            // YCBCR strictly requires 3 bands. Not less, not more Issue an
            // explicit error message as libtiff one is a bit cryptic:
            // TIFFVStripSize64:Invalid td_samplesperpixel value.
            if (l_nBands != 3)
            {
                ReportError(
                    pszFilename, CE_Failure, CPLE_NotSupported,
                    "PHOTOMETRIC=YCBCR not supported on a %d-band raster: "
                    "only compatible of a 3-band (RGB) raster",
                    l_nBands);
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }

            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_YCBCR);
            nSamplesAccountedFor = 3;

            // Explicitly register the subsampling so that JPEGFixupTags
            // is a no-op (helps for cloud optimized geotiffs)
            TIFFSetField(l_hTIFF, TIFFTAG_YCBCRSUBSAMPLING, 2, 2);
        }
        else if (EQUAL(pszValue, "CIELAB"))
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CIELAB);
            nSamplesAccountedFor = 3;
        }
        else if (EQUAL(pszValue, "ICCLAB"))
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ICCLAB);
            nSamplesAccountedFor = 3;
        }
        else if (EQUAL(pszValue, "ITULAB"))
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ITULAB);
            nSamplesAccountedFor = 3;
        }
        else
        {
            ReportError(pszFilename, CE_Warning, CPLE_IllegalArg,
                        "PHOTOMETRIC=%s value not recognised, ignoring.  "
                        "Set the Photometric Interpretation as MINISBLACK.",
                        pszValue);
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        }

        if (l_nBands < nSamplesAccountedFor)
        {
            ReportError(pszFilename, CE_Warning, CPLE_IllegalArg,
                        "PHOTOMETRIC=%s value does not correspond to number "
                        "of bands (%d), ignoring.  "
                        "Set the Photometric Interpretation as MINISBLACK.",
                        pszValue, l_nBands);
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        }
    }
    else
    {
        // If image contains 3 or 4 bands and datatype is Byte then we will
        // assume it is RGB. In all other cases assume it is MINISBLACK.
        if (l_nBands == 3 && eType == GDT_Byte)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            nSamplesAccountedFor = 3;
        }
        else if (l_nBands == 4 && eType == GDT_Byte)
        {
            uint16_t v[1] = {
                GTiffGetAlphaValue(CSLFetchNameValue(papszParamList, "ALPHA"),
                                   DEFAULT_ALPHA_TYPE)};

            TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            nSamplesAccountedFor = 4;
        }
        else
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            nSamplesAccountedFor = 1;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      If there are extra samples, we need to mark them with an        */
    /*      appropriate extrasamples definition here.                       */
    /* -------------------------------------------------------------------- */
    if (l_nBands > nSamplesAccountedFor)
    {
        const int nExtraSamples = l_nBands - nSamplesAccountedFor;

        uint16_t *v = static_cast<uint16_t *>(
            CPLMalloc(sizeof(uint16_t) * nExtraSamples));

        v[0] = GTiffGetAlphaValue(CSLFetchNameValue(papszParamList, "ALPHA"),
                                  EXTRASAMPLE_UNSPECIFIED);

        for (int i = 1; i < nExtraSamples; ++i)
            v[i] = EXTRASAMPLE_UNSPECIFIED;

        TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, nExtraSamples, v);

        CPLFree(v);
    }

    // Set the ICC color profile.
    if (eProfile != GTiffProfile::BASELINE)
    {
        SaveICCProfile(nullptr, l_hTIFF, papszParamList, l_nBitsPerSample);
    }

    // Set the compression method before asking the default strip size
    // This is useful when translating to a JPEG-In-TIFF file where
    // the default strip size is 8 or 16 depending on the photometric value.
    TIFFSetField(l_hTIFF, TIFFTAG_COMPRESSION, l_nCompression);

    if (l_nCompression == COMPRESSION_LERC)
    {
        const char *pszCompress =
            CSLFetchNameValueDef(papszParamList, "COMPRESS", "");
        if (EQUAL(pszCompress, "LERC_DEFLATE"))
        {
            TIFFSetField(l_hTIFF, TIFFTAG_LERC_ADD_COMPRESSION,
                         LERC_ADD_COMPRESSION_DEFLATE);
        }
        else if (EQUAL(pszCompress, "LERC_ZSTD"))
        {
            if (TIFFSetField(l_hTIFF, TIFFTAG_LERC_ADD_COMPRESSION,
                             LERC_ADD_COMPRESSION_ZSTD) != 1)
            {
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }
        }
    }
    // TODO later: take into account LERC version

    /* -------------------------------------------------------------------- */
    /*      Setup tiling/stripping flags.                                   */
    /* -------------------------------------------------------------------- */
    if (bTiled)
    {
        if (!TIFFSetField(l_hTIFF, TIFFTAG_TILEWIDTH, l_nBlockXSize) ||
            !TIFFSetField(l_hTIFF, TIFFTAG_TILELENGTH, l_nBlockYSize))
        {
            XTIFFClose(l_hTIFF);
            CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
            return nullptr;
        }
    }
    else
    {
        const uint32_t l_nRowsPerStrip = std::min(
            nYSize, l_nBlockYSize == 0
                        ? static_cast<int>(TIFFDefaultStripSize(l_hTIFF, 0))
                        : l_nBlockYSize);

        TIFFSetField(l_hTIFF, TIFFTAG_ROWSPERSTRIP, l_nRowsPerStrip);
    }

    /* -------------------------------------------------------------------- */
    /*      Set compression related tags.                                   */
    /* -------------------------------------------------------------------- */
    if (GTIFFSupportsPredictor(l_nCompression))
        TIFFSetField(l_hTIFF, TIFFTAG_PREDICTOR, nPredictor);
    if (l_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        l_nCompression == COMPRESSION_LERC)
    {
        GTiffSetDeflateSubCodec(l_hTIFF);

        if (l_nZLevel != -1)
            TIFFSetField(l_hTIFF, TIFFTAG_ZIPQUALITY, l_nZLevel);
    }
    if (l_nCompression == COMPRESSION_JPEG && l_nJpegQuality != -1)
        TIFFSetField(l_hTIFF, TIFFTAG_JPEGQUALITY, l_nJpegQuality);
    if (l_nCompression == COMPRESSION_LZMA && l_nLZMAPreset != -1)
        TIFFSetField(l_hTIFF, TIFFTAG_LZMAPRESET, l_nLZMAPreset);
    if ((l_nCompression == COMPRESSION_ZSTD ||
         l_nCompression == COMPRESSION_LERC) &&
        l_nZSTDLevel != -1)
        TIFFSetField(l_hTIFF, TIFFTAG_ZSTD_LEVEL, l_nZSTDLevel);
    if (l_nCompression == COMPRESSION_LERC)
    {
        TIFFSetField(l_hTIFF, TIFFTAG_LERC_MAXZERROR, l_dfMaxZError);
    }
#if HAVE_JXL
    if (l_nCompression == COMPRESSION_JXL)
    {
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_LOSSYNESS,
                     l_bJXLLossless ? JXL_LOSSLESS : JXL_LOSSY);
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_EFFORT, l_nJXLEffort);
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_DISTANCE, l_fJXLDistance);
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_ALPHA_DISTANCE, l_fJXLAlphaDistance);
    }
#endif
    if (l_nCompression == COMPRESSION_WEBP)
        TIFFSetField(l_hTIFF, TIFFTAG_WEBP_LEVEL, l_nWebPLevel);
    if (l_nCompression == COMPRESSION_WEBP && l_bWebPLossless)
        TIFFSetField(l_hTIFF, TIFFTAG_WEBP_LOSSLESS, 1);

    if (l_nCompression == COMPRESSION_JPEG)
        TIFFSetField(l_hTIFF, TIFFTAG_JPEGTABLESMODE, l_nJpegTablesMode);

    /* -------------------------------------------------------------------- */
    /*      If we forced production of a file with photometric=palette,     */
    /*      we need to push out a default color table.                      */
    /* -------------------------------------------------------------------- */
    if (bForceColorTable)
    {
        const int nColors = eType == GDT_Byte ? 256 : 65536;

        unsigned short *panTRed = static_cast<unsigned short *>(
            CPLMalloc(sizeof(unsigned short) * nColors));
        unsigned short *panTGreen = static_cast<unsigned short *>(
            CPLMalloc(sizeof(unsigned short) * nColors));
        unsigned short *panTBlue = static_cast<unsigned short *>(
            CPLMalloc(sizeof(unsigned short) * nColors));

        for (int iColor = 0; iColor < nColors; ++iColor)
        {
            if (eType == GDT_Byte)
            {
                panTRed[iColor] = static_cast<unsigned short>(257 * iColor);
                panTGreen[iColor] = static_cast<unsigned short>(257 * iColor);
                panTBlue[iColor] = static_cast<unsigned short>(257 * iColor);
            }
            else
            {
                panTRed[iColor] = static_cast<unsigned short>(iColor);
                panTGreen[iColor] = static_cast<unsigned short>(iColor);
                panTBlue[iColor] = static_cast<unsigned short>(iColor);
            }
        }

        TIFFSetField(l_hTIFF, TIFFTAG_COLORMAP, panTRed, panTGreen, panTBlue);

        CPLFree(panTRed);
        CPLFree(panTGreen);
        CPLFree(panTBlue);
    }

    // This trick
    // creates a temporary in-memory file and fetches its JPEG tables so that
    // we can directly set them, before tif_jpeg.c compute them at the first
    // strip/tile writing, which is too late, since we have already crystalized
    // the directory. This way we avoid a directory rewriting.
    if (l_nCompression == COMPRESSION_JPEG &&
        !STARTS_WITH(pszFilename, szJPEGGTiffDatasetTmpPrefix) &&
        CPLTestBool(
            CSLFetchNameValueDef(papszParamList, "WRITE_JPEGTABLE_TAG", "YES")))
    {
        GTiffWriteJPEGTables(
            l_hTIFF, CSLFetchNameValue(papszParamList, "PHOTOMETRIC"),
            CSLFetchNameValue(papszParamList, "JPEG_QUALITY"),
            CSLFetchNameValue(papszParamList, "JPEGTABLESMODE"));
    }

    *pfpL = l_fpL;

    return l_hTIFF;
}

/************************************************************************/
/*                            GuessJPEGQuality()                        */
/*                                                                      */
/*      Guess JPEG quality from JPEGTABLES tag.                         */
/************************************************************************/

static const GByte *GTIFFFindNextTable(const GByte *paby, GByte byMarker,
                                       int nLen, int *pnLenTable)
{
    for (int i = 0; i + 1 < nLen;)
    {
        if (paby[i] != 0xFF)
            return nullptr;
        ++i;
        if (paby[i] == 0xD8)
        {
            ++i;
            continue;
        }
        if (i + 2 >= nLen)
            return nullptr;
        int nMarkerLen = paby[i + 1] * 256 + paby[i + 2];
        if (i + 1 + nMarkerLen >= nLen)
            return nullptr;
        if (paby[i] == byMarker)
        {
            if (pnLenTable)
                *pnLenTable = nMarkerLen;
            return paby + i + 1;
        }
        i += 1 + nMarkerLen;
    }
    return nullptr;
}

constexpr GByte MARKER_HUFFMAN_TABLE = 0xC4;
constexpr GByte MARKER_QUANT_TABLE = 0xDB;

// We assume that if there are several quantization tables, they are
// in the same order. Which is a reasonable assumption for updating
// a file generated by ourselves.
static bool GTIFFQuantizationTablesEqual(const GByte *paby1, int nLen1,
                                         const GByte *paby2, int nLen2)
{
    bool bFound = false;
    while (true)
    {
        int nLenTable1 = 0;
        int nLenTable2 = 0;
        const GByte *paby1New =
            GTIFFFindNextTable(paby1, MARKER_QUANT_TABLE, nLen1, &nLenTable1);
        const GByte *paby2New =
            GTIFFFindNextTable(paby2, MARKER_QUANT_TABLE, nLen2, &nLenTable2);
        if (paby1New == nullptr && paby2New == nullptr)
            return bFound;
        if (paby1New == nullptr || paby2New == nullptr)
            return false;
        if (nLenTable1 != nLenTable2)
            return false;
        if (memcmp(paby1New, paby2New, nLenTable1) != 0)
            return false;
        paby1New += nLenTable1;
        paby2New += nLenTable2;
        nLen1 -= static_cast<int>(paby1New - paby1);
        nLen2 -= static_cast<int>(paby2New - paby2);
        paby1 = paby1New;
        paby2 = paby2New;
        bFound = true;
    }
}

// Guess the JPEG quality by comparing against the MD5Sum of precomputed
// quantization tables
static int GuessJPEGQualityFromMD5(const uint8_t md5JPEGQuantTable[][16],
                                   const GByte *const pabyJPEGTable,
                                   int nJPEGTableSize)
{
    int nRemainingLen = nJPEGTableSize;
    const GByte *pabyCur = pabyJPEGTable;

    struct CPLMD5Context context;
    CPLMD5Init(&context);

    while (true)
    {
        int nLenTable = 0;
        const GByte *pabyNew = GTIFFFindNextTable(pabyCur, MARKER_QUANT_TABLE,
                                                  nRemainingLen, &nLenTable);
        if (pabyNew == nullptr)
            break;
        CPLMD5Update(&context, pabyNew, nLenTable);
        pabyNew += nLenTable;
        nRemainingLen -= static_cast<int>(pabyNew - pabyCur);
        pabyCur = pabyNew;
    }

    GByte digest[16];
    CPLMD5Final(digest, &context);

    for (int i = 0; i < 100; i++)
    {
        if (memcmp(md5JPEGQuantTable[i], digest, 16) == 0)
        {
            return i + 1;
        }
    }
    return -1;
}

int GTiffDataset::GuessJPEGQuality(bool &bOutHasQuantizationTable,
                                   bool &bOutHasHuffmanTable)
{
    CPLAssert(m_nCompression == COMPRESSION_JPEG);
    uint32_t nJPEGTableSize = 0;
    void *pJPEGTable = nullptr;
    if (!TIFFGetField(m_hTIFF, TIFFTAG_JPEGTABLES, &nJPEGTableSize,
                      &pJPEGTable))
    {
        bOutHasQuantizationTable = false;
        bOutHasHuffmanTable = false;
        return -1;
    }

    bOutHasQuantizationTable =
        GTIFFFindNextTable(static_cast<const GByte *>(pJPEGTable),
                           MARKER_QUANT_TABLE, nJPEGTableSize,
                           nullptr) != nullptr;
    bOutHasHuffmanTable =
        GTIFFFindNextTable(static_cast<const GByte *>(pJPEGTable),
                           MARKER_HUFFMAN_TABLE, nJPEGTableSize,
                           nullptr) != nullptr;
    if (!bOutHasQuantizationTable)
        return -1;

    if ((nBands == 1 && m_nBitsPerSample == 8) ||
        (nBands == 3 && m_nBitsPerSample == 8 &&
         m_nPhotometric == PHOTOMETRIC_RGB) ||
        (nBands == 4 && m_nBitsPerSample == 8 &&
         m_nPhotometric == PHOTOMETRIC_SEPARATED))
    {
        return GuessJPEGQualityFromMD5(md5JPEGQuantTable_generic_8bit,
                                       static_cast<const GByte *>(pJPEGTable),
                                       static_cast<int>(nJPEGTableSize));
    }

    if (nBands == 3 && m_nBitsPerSample == 8 &&
        m_nPhotometric == PHOTOMETRIC_YCBCR)
    {
        int nRet =
            GuessJPEGQualityFromMD5(md5JPEGQuantTable_3_YCBCR_8bit,
                                    static_cast<const GByte *>(pJPEGTable),
                                    static_cast<int>(nJPEGTableSize));
        if (nRet < 0)
        {
            // libjpeg 9e has modified the YCbCr quantization tables.
            nRet =
                GuessJPEGQualityFromMD5(md5JPEGQuantTable_3_YCBCR_8bit_jpeg9e,
                                        static_cast<const GByte *>(pJPEGTable),
                                        static_cast<int>(nJPEGTableSize));
        }
        return nRet;
    }

    char **papszLocalParameters = nullptr;
    papszLocalParameters =
        CSLSetNameValue(papszLocalParameters, "COMPRESS", "JPEG");
    if (m_nPhotometric == PHOTOMETRIC_YCBCR)
        papszLocalParameters =
            CSLSetNameValue(papszLocalParameters, "PHOTOMETRIC", "YCBCR");
    else if (m_nPhotometric == PHOTOMETRIC_SEPARATED)
        papszLocalParameters =
            CSLSetNameValue(papszLocalParameters, "PHOTOMETRIC", "CMYK");
    papszLocalParameters =
        CSLSetNameValue(papszLocalParameters, "BLOCKYSIZE", "16");
    if (m_nBitsPerSample == 12)
        papszLocalParameters =
            CSLSetNameValue(papszLocalParameters, "NBITS", "12");

    CPLString osTmpFilenameIn;
    osTmpFilenameIn.Printf("/vsimem/gtiffdataset_guess_jpeg_quality_tmp_%p",
                           this);

    int nRet = -1;
    for (int nQuality = 0; nQuality <= 100 && nRet < 0; ++nQuality)
    {
        VSILFILE *fpTmp = nullptr;
        if (nQuality == 0)
            papszLocalParameters =
                CSLSetNameValue(papszLocalParameters, "JPEG_QUALITY", "75");
        else
            papszLocalParameters =
                CSLSetNameValue(papszLocalParameters, "JPEG_QUALITY",
                                CPLSPrintf("%d", nQuality));

        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLString osTmp;
        TIFF *hTIFFTmp =
            CreateLL(osTmpFilenameIn, 16, 16, (nBands <= 4) ? nBands : 1,
                     GetRasterBand(1)->GetRasterDataType(), 0.0,
                     papszLocalParameters, &fpTmp, osTmp);
        CPLPopErrorHandler();
        if (!hTIFFTmp)
        {
            break;
        }

        TIFFWriteCheck(hTIFFTmp, FALSE, "CreateLL");
        TIFFWriteDirectory(hTIFFTmp);
        TIFFSetDirectory(hTIFFTmp, 0);
        // Now reset jpegcolormode.
        if (m_nPhotometric == PHOTOMETRIC_YCBCR &&
            CPLTestBool(CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES")))
        {
            TIFFSetField(hTIFFTmp, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }

        GByte abyZeroData[(16 * 16 * 4 * 3) / 2] = {};
        const int nBlockSize =
            (16 * 16 * ((nBands <= 4) ? nBands : 1) * m_nBitsPerSample) / 8;
        TIFFWriteEncodedStrip(hTIFFTmp, 0, abyZeroData, nBlockSize);

        uint32_t nJPEGTableSizeTry = 0;
        void *pJPEGTableTry = nullptr;
        if (TIFFGetField(hTIFFTmp, TIFFTAG_JPEGTABLES, &nJPEGTableSizeTry,
                         &pJPEGTableTry))
        {
            if (GTIFFQuantizationTablesEqual(
                    static_cast<GByte *>(pJPEGTable), nJPEGTableSize,
                    static_cast<GByte *>(pJPEGTableTry), nJPEGTableSizeTry))
            {
                nRet = (nQuality == 0) ? 75 : nQuality;
            }
        }

        XTIFFClose(hTIFFTmp);
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTmp));
    }

    CSLDestroy(papszLocalParameters);
    VSIUnlink(osTmpFilenameIn);

    return nRet;
}

/************************************************************************/
/*               SetJPEGQualityAndTablesModeFromFile()                  */
/************************************************************************/

void GTiffDataset::SetJPEGQualityAndTablesModeFromFile(
    int nQuality, bool bHasQuantizationTable, bool bHasHuffmanTable)
{
    if (nQuality > 0)
    {
        CPLDebug("GTiff", "Guessed JPEG quality to be %d", nQuality);
        m_nJpegQuality = static_cast<signed char>(nQuality);
        TIFFSetField(m_hTIFF, TIFFTAG_JPEGQUALITY, nQuality);

        // This means we will use the quantization tables from the
        // JpegTables tag.
        m_nJpegTablesMode = JPEGTABLESMODE_QUANT;
    }
    else
    {
        uint32_t nJPEGTableSize = 0;
        void *pJPEGTable = nullptr;
        if (!TIFFGetField(m_hTIFF, TIFFTAG_JPEGTABLES, &nJPEGTableSize,
                          &pJPEGTable))
        {
            toff_t *panByteCounts = nullptr;
            const int nBlockCount = m_nPlanarConfig == PLANARCONFIG_SEPARATE
                                        ? m_nBlocksPerBand * nBands
                                        : m_nBlocksPerBand;
            if (TIFFIsTiled(m_hTIFF))
                TIFFGetField(m_hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts);
            else
                TIFFGetField(m_hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts);

            bool bFoundNonEmptyBlock = false;
            if (panByteCounts != nullptr)
            {
                for (int iBlock = 0; iBlock < nBlockCount; ++iBlock)
                {
                    if (panByteCounts[iBlock] != 0)
                    {
                        bFoundNonEmptyBlock = true;
                        break;
                    }
                }
            }
            if (bFoundNonEmptyBlock)
            {
                CPLDebug("GTiff", "Could not guess JPEG quality. "
                                  "JPEG tables are missing, so going in "
                                  "TIFFTAG_JPEGTABLESMODE = 0/2 mode");
                // Write quantization tables in each strile.
                m_nJpegTablesMode = 0;
            }
        }
        else
        {
            if (bHasQuantizationTable)
            {
                // FIXME in libtiff: this is likely going to cause issues
                // since libtiff will reuse in each strile the number of
                // the global quantization table, which is invalid.
                CPLDebug("GTiff",
                         "Could not guess JPEG quality although JPEG "
                         "quantization tables are present, so going in "
                         "TIFFTAG_JPEGTABLESMODE = 0/2 mode");
            }
            else
            {
                CPLDebug("GTiff",
                         "Could not guess JPEG quality since JPEG "
                         "quantization tables are not present, so going in "
                         "TIFFTAG_JPEGTABLESMODE = 0/2 mode");
            }

            // Write quantization tables in each strile.
            m_nJpegTablesMode = 0;
        }
    }
    if (bHasHuffmanTable)
    {
        // If there are Huffman tables in header use them, otherwise
        // if we use optimized tables, libtiff will currently reuse
        // the number of the Huffman tables of the header for the
        // optimized version of each strile, which is illegal.
        m_nJpegTablesMode |= JPEGTABLESMODE_HUFF;
    }
    if (m_nJpegTablesMode >= 0)
        TIFFSetField(m_hTIFF, TIFFTAG_JPEGTABLESMODE, m_nJpegTablesMode);
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new GeoTIFF or TIFF file.                              */
/************************************************************************/

GDALDataset *GTiffDataset::Create(const char *pszFilename, int nXSize,
                                  int nYSize, int l_nBands, GDALDataType eType,
                                  char **papszParamList)

{
    VSILFILE *l_fpL = nullptr;
    CPLString l_osTmpFilename;

    /* -------------------------------------------------------------------- */
    /*      Create the underlying TIFF file.                                */
    /* -------------------------------------------------------------------- */
    TIFF *l_hTIFF = CreateLL(pszFilename, nXSize, nYSize, l_nBands, eType, 0,
                             papszParamList, &l_fpL, l_osTmpFilename);
    const bool bStreaming = !l_osTmpFilename.empty();

    if (l_hTIFF == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create the new GTiffDataset object.                             */
    /* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->m_hTIFF = l_hTIFF;
    poDS->m_fpL = l_fpL;
    if (bStreaming)
    {
        poDS->m_bStreamingOut = true;
        poDS->m_pszTmpFilename = CPLStrdup(l_osTmpFilename);
        poDS->m_fpToWrite = VSIFOpenL(pszFilename, "wb");
        if (poDS->m_fpToWrite == nullptr)
        {
            VSIUnlink(l_osTmpFilename);
            delete poDS;
            return nullptr;
        }
    }
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->m_bCrystalized = false;
    poDS->m_nSamplesPerPixel = static_cast<uint16_t>(l_nBands);
    poDS->m_pszFilename = CPLStrdup(pszFilename);

    // Don't try to load external metadata files (#6597).
    poDS->m_bIMDRPCMetadataLoaded = true;

    // Avoid premature crystalization that will cause directory re-writing if
    // GetProjectionRef() or GetGeoTransform() are called on the newly created
    // GeoTIFF.
    poDS->m_bLookedForProjection = true;

    TIFFGetField(l_hTIFF, TIFFTAG_SAMPLEFORMAT, &(poDS->m_nSampleFormat));
    TIFFGetField(l_hTIFF, TIFFTAG_PLANARCONFIG, &(poDS->m_nPlanarConfig));
    // Weird that we need this, but otherwise we get a Valgrind warning on
    // tiff_write_124.
    if (!TIFFGetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, &(poDS->m_nPhotometric)))
        poDS->m_nPhotometric = PHOTOMETRIC_MINISBLACK;
    TIFFGetField(l_hTIFF, TIFFTAG_BITSPERSAMPLE, &(poDS->m_nBitsPerSample));
    TIFFGetField(l_hTIFF, TIFFTAG_COMPRESSION, &(poDS->m_nCompression));

    if (TIFFIsTiled(l_hTIFF))
    {
        TIFFGetField(l_hTIFF, TIFFTAG_TILEWIDTH, &(poDS->m_nBlockXSize));
        TIFFGetField(l_hTIFF, TIFFTAG_TILELENGTH, &(poDS->m_nBlockYSize));
    }
    else
    {
        if (!TIFFGetField(l_hTIFF, TIFFTAG_ROWSPERSTRIP,
                          &(poDS->m_nRowsPerStrip)))
            poDS->m_nRowsPerStrip = 1;  // Dummy value.

        poDS->m_nBlockXSize = nXSize;
        poDS->m_nBlockYSize =
            std::min(static_cast<int>(poDS->m_nRowsPerStrip), nYSize);
    }

    if (!poDS->ComputeBlocksPerColRowAndBand(l_nBands))
    {
        delete poDS;
        return nullptr;
    }

    poDS->m_eProfile = GetProfile(CSLFetchNameValue(papszParamList, "PROFILE"));

    /* -------------------------------------------------------------------- */
    /*      YCbCr JPEG compressed images should be translated on the fly    */
    /*      to RGB by libtiff/libjpeg unless specifically requested         */
    /*      otherwise.                                                      */
    /* -------------------------------------------------------------------- */
    if (poDS->m_nCompression == COMPRESSION_JPEG &&
        poDS->m_nPhotometric == PHOTOMETRIC_YCBCR &&
        CPLTestBool(CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES")))
    {
        int nColorMode = 0;

        poDS->SetMetadataItem("SOURCE_COLOR_SPACE", "YCbCr", "IMAGE_STRUCTURE");
        if (!TIFFGetField(l_hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode) ||
            nColorMode != JPEGCOLORMODE_RGB)
            TIFFSetField(l_hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
    }

    if (poDS->m_nCompression == COMPRESSION_LERC)
    {
        uint32_t nLercParamCount = 0;
        uint32_t *panLercParams = nullptr;
        if (TIFFGetField(l_hTIFF, TIFFTAG_LERC_PARAMETERS, &nLercParamCount,
                         &panLercParams) &&
            nLercParamCount == 2)
        {
            memcpy(poDS->m_anLercAddCompressionAndVersion, panLercParams,
                   sizeof(poDS->m_anLercAddCompressionAndVersion));
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Read palette back as a color table if it has one.               */
    /* -------------------------------------------------------------------- */
    unsigned short *panRed = nullptr;
    unsigned short *panGreen = nullptr;
    unsigned short *panBlue = nullptr;

    if (poDS->m_nPhotometric == PHOTOMETRIC_PALETTE &&
        TIFFGetField(l_hTIFF, TIFFTAG_COLORMAP, &panRed, &panGreen, &panBlue))
    {

        poDS->m_poColorTable = new GDALColorTable();

        const int nColorCount = 1 << poDS->m_nBitsPerSample;

        for (int iColor = nColorCount - 1; iColor >= 0; iColor--)
        {
            const unsigned short divisor = 257;
            const GDALColorEntry oEntry = {
                static_cast<short>(panRed[iColor] / divisor),
                static_cast<short>(panGreen[iColor] / divisor),
                static_cast<short>(panBlue[iColor] / divisor),
                static_cast<short>(255)};

            poDS->m_poColorTable->SetColorEntry(iColor, &oEntry);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Do we want to ensure all blocks get written out on close to     */
    /*      avoid sparse files?                                             */
    /* -------------------------------------------------------------------- */
    if (!CPLFetchBool(papszParamList, "SPARSE_OK", false))
        poDS->m_bFillEmptyTilesAtClosing = true;

    poDS->m_bWriteEmptyTiles =
        bStreaming || (poDS->m_nCompression != COMPRESSION_NONE &&
                       poDS->m_bFillEmptyTilesAtClosing);
    // Only required for people writing non-compressed striped files in the
    // right order and wanting all tstrips to be written in the same order
    // so that the end result can be memory mapped without knowledge of each
    // strip offset.
    if (CPLTestBool(CSLFetchNameValueDef(
            papszParamList, "WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE")) ||
        CPLTestBool(CSLFetchNameValueDef(
            papszParamList, "@WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE")))
    {
        poDS->m_bWriteEmptyTiles = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Preserve creation options for consulting later (for instance    */
    /*      to decide if a TFW file should be written).                     */
    /* -------------------------------------------------------------------- */
    poDS->m_papszCreationOptions = CSLDuplicate(papszParamList);

    poDS->m_nZLevel = GTiffGetZLevel(papszParamList);
    poDS->m_nLZMAPreset = GTiffGetLZMAPreset(papszParamList);
    poDS->m_nZSTDLevel = GTiffGetZSTDPreset(papszParamList);
    poDS->m_nWebPLevel = GTiffGetWebPLevel(papszParamList);
    poDS->m_bWebPLossless = GTiffGetWebPLossless(papszParamList);
    if (poDS->m_nWebPLevel != 100 && poDS->m_bWebPLossless &&
        CSLFetchNameValue(papszParamList, "WEBP_LEVEL"))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "WEBP_LEVEL is specified, but WEBP_LOSSLESS=YES. "
                 "WEBP_LEVEL will be ignored.");
    }
    poDS->m_nJpegQuality = GTiffGetJpegQuality(papszParamList);
    poDS->m_nJpegTablesMode = GTiffGetJpegTablesMode(papszParamList);
    poDS->m_dfMaxZError = GTiffGetLERCMaxZError(papszParamList);
    poDS->m_dfMaxZErrorOverview = GTiffGetLERCMaxZErrorOverview(papszParamList);
#if HAVE_JXL
    poDS->m_bJXLLossless = GTiffGetJXLLossless(papszParamList);
    poDS->m_nJXLEffort = GTiffGetJXLEffort(papszParamList);
    poDS->m_fJXLDistance = GTiffGetJXLDistance(papszParamList);
    poDS->m_fJXLAlphaDistance = GTiffGetJXLAlphaDistance(papszParamList);
#endif
    poDS->InitCreationOrOpenOptions(true, papszParamList);

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    for (int iBand = 0; iBand < l_nBands; ++iBand)
    {
        if (poDS->m_nBitsPerSample == 8 ||
            (poDS->m_nBitsPerSample == 16 && eType != GDT_Float32) ||
            poDS->m_nBitsPerSample == 32 || poDS->m_nBitsPerSample == 64 ||
            poDS->m_nBitsPerSample == 128)
        {
            poDS->SetBand(iBand + 1, new GTiffRasterBand(poDS, iBand + 1));
        }
        else
        {
            poDS->SetBand(iBand + 1, new GTiffOddBitsBand(poDS, iBand + 1));
            poDS->GetRasterBand(iBand + 1)->SetMetadataItem(
                "NBITS", CPLString().Printf("%d", poDS->m_nBitsPerSample),
                "IMAGE_STRUCTURE");
        }
    }

    poDS->GetDiscardLsbOption(papszParamList);

    if (poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG && l_nBands != 1)
        poDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    else
        poDS->SetMetadataItem("INTERLEAVE", "BAND", "IMAGE_STRUCTURE");

    poDS->oOvManager.Initialize(poDS, pszFilename);

    return poDS;
}

/************************************************************************/
/*                           CopyImageryAndMask()                       */
/************************************************************************/

CPLErr GTiffDataset::CopyImageryAndMask(GTiffDataset *poDstDS,
                                        GDALDataset *poSrcDS,
                                        GDALRasterBand *poSrcMaskBand,
                                        GDALProgressFunc pfnProgress,
                                        void *pProgressData)
{
    CPLErr eErr = CE_None;

    const auto eType = poDstDS->GetRasterBand(1)->GetRasterDataType();
    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eType);
    const int l_nBands = poDstDS->GetRasterCount();
    void *pBlockBuffer =
        VSI_MALLOC3_VERBOSE(poDstDS->m_nBlockXSize, poDstDS->m_nBlockYSize,
                            cpl::fits_on<int>(l_nBands * nDataTypeSize));
    if (pBlockBuffer == nullptr)
    {
        eErr = CE_Failure;
    }
    const int nYSize = poDstDS->nRasterYSize;
    const int nXSize = poDstDS->nRasterXSize;
    const int nBlocks = poDstDS->m_nBlocksPerBand;

    CPLAssert(l_nBands == 1 || poDstDS->m_nPlanarConfig == PLANARCONFIG_CONTIG);

    const bool bIsOddBand =
        dynamic_cast<GTiffOddBitsBand *>(poDstDS->GetRasterBand(1)) != nullptr;

    if (poDstDS->m_poMaskDS)
    {
        CPLAssert(poDstDS->m_poMaskDS->m_nBlockXSize == poDstDS->m_nBlockXSize);
        CPLAssert(poDstDS->m_poMaskDS->m_nBlockYSize == poDstDS->m_nBlockYSize);
    }

    int iBlock = 0;
    for (int iY = 0, nYBlock = 0; iY < nYSize && eErr == CE_None;
         iY = ((nYSize - iY < poDstDS->m_nBlockYSize)
                   ? nYSize
                   : iY + poDstDS->m_nBlockYSize),
             nYBlock++)
    {
        const int nReqYSize = std::min(nYSize - iY, poDstDS->m_nBlockYSize);
        for (int iX = 0, nXBlock = 0; iX < nXSize && eErr == CE_None;
             iX = ((nXSize - iX < poDstDS->m_nBlockXSize)
                       ? nXSize
                       : iX + poDstDS->m_nBlockXSize),
                 nXBlock++)
        {
            const int nReqXSize = std::min(nXSize - iX, poDstDS->m_nBlockXSize);
            if (nReqXSize < poDstDS->m_nBlockXSize ||
                nReqYSize < poDstDS->m_nBlockYSize)
            {
                memset(pBlockBuffer, 0,
                       static_cast<size_t>(poDstDS->m_nBlockXSize) *
                           poDstDS->m_nBlockYSize * l_nBands * nDataTypeSize);
            }

            if (!bIsOddBand)
            {
                eErr = poSrcDS->RasterIO(
                    GF_Read, iX, iY, nReqXSize, nReqYSize, pBlockBuffer,
                    nReqXSize, nReqYSize, eType, l_nBands, nullptr,
                    static_cast<GSpacing>(nDataTypeSize) * l_nBands,
                    static_cast<GSpacing>(nDataTypeSize) * l_nBands *
                        poDstDS->m_nBlockXSize,
                    nDataTypeSize, nullptr);
                if (eErr == CE_None)
                {
                    eErr = poDstDS->WriteEncodedTileOrStrip(
                        iBlock, pBlockBuffer, false);
                }
            }
            else
            {
                // In the odd bit case, this is a bit messy to ensure
                // the strile gets written synchronously.
                // We load the content of the n-1 bands in the cache,
                // and for the last band we invoke WriteBlock() directly
                // We also force FlushBlockBuf()
                std::vector<GDALRasterBlock *> apoLockedBlocks;
                for (int i = 0; eErr == CE_None && i < l_nBands - 1; i++)
                {
                    auto poBlock =
                        poDstDS->GetRasterBand(i + 1)->GetLockedBlockRef(
                            nXBlock, nYBlock, TRUE);
                    if (poBlock)
                    {
                        eErr = poSrcDS->GetRasterBand(i + 1)->RasterIO(
                            GF_Read, iX, iY, nReqXSize, nReqYSize,
                            poBlock->GetDataRef(), nReqXSize, nReqYSize, eType,
                            nDataTypeSize,
                            static_cast<GSpacing>(nDataTypeSize) *
                                poDstDS->m_nBlockXSize,
                            nullptr);
                        poBlock->MarkDirty();
                        apoLockedBlocks.emplace_back(poBlock);
                    }
                    else
                    {
                        eErr = CE_Failure;
                    }
                }
                if (eErr == CE_None)
                {
                    eErr = poSrcDS->GetRasterBand(l_nBands)->RasterIO(
                        GF_Read, iX, iY, nReqXSize, nReqYSize, pBlockBuffer,
                        nReqXSize, nReqYSize, eType, nDataTypeSize,
                        static_cast<GSpacing>(nDataTypeSize) *
                            poDstDS->m_nBlockXSize,
                        nullptr);
                }
                if (eErr == CE_None)
                {
                    // Avoid any attempt to load from disk
                    poDstDS->m_nLoadedBlock = iBlock;
                    eErr = poDstDS->GetRasterBand(l_nBands)->WriteBlock(
                        nXBlock, nYBlock, pBlockBuffer);
                    if (eErr == CE_None)
                        eErr = poDstDS->FlushBlockBuf();
                }
                for (auto poBlock : apoLockedBlocks)
                {
                    poBlock->MarkClean();
                    poBlock->DropLock();
                }
            }

            if (eErr == CE_None && poDstDS->m_poMaskDS)
            {
                if (nReqXSize < poDstDS->m_nBlockXSize ||
                    nReqYSize < poDstDS->m_nBlockYSize)
                {
                    memset(pBlockBuffer, 0,
                           static_cast<size_t>(poDstDS->m_nBlockXSize) *
                               poDstDS->m_nBlockYSize);
                }
                eErr = poSrcMaskBand->RasterIO(
                    GF_Read, iX, iY, nReqXSize, nReqYSize, pBlockBuffer,
                    nReqXSize, nReqYSize, GDT_Byte, 1, poDstDS->m_nBlockXSize,
                    nullptr);
                if (eErr == CE_None)
                {
                    // Avoid any attempt to load from disk
                    poDstDS->m_poMaskDS->m_nLoadedBlock = iBlock;
                    eErr = poDstDS->m_poMaskDS->GetRasterBand(1)->WriteBlock(
                        nXBlock, nYBlock, pBlockBuffer);
                    if (eErr == CE_None)
                        eErr = poDstDS->m_poMaskDS->FlushBlockBuf();
                }
            }
            if (poDstDS->m_bWriteError)
                eErr = CE_Failure;

            iBlock++;
            if (pfnProgress &&
                !pfnProgress(static_cast<double>(iBlock) / nBlocks, nullptr,
                             pProgressData))
            {
                eErr = CE_Failure;
            }
        }
    }
    poDstDS->FlushCache(false);  // mostly to wait for thread completion
    VSIFree(pBlockBuffer);

    return eErr;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *GTiffDataset::CreateCopy(const char *pszFilename,
                                      GDALDataset *poSrcDS, int bStrict,
                                      char **papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData)

{
    if (poSrcDS->GetRasterCount() == 0)
    {
        ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
                    "Unable to export GeoTIFF files with zero bands.");
        return nullptr;
    }

    GDALRasterBand *const poPBand = poSrcDS->GetRasterBand(1);
    GDALDataType eType = poPBand->GetRasterDataType();

    /* -------------------------------------------------------------------- */
    /*      Check, whether all bands in input dataset has the same type.    */
    /* -------------------------------------------------------------------- */
    const int l_nBands = poSrcDS->GetRasterCount();
    for (int iBand = 2; iBand <= l_nBands; ++iBand)
    {
        if (eType != poSrcDS->GetRasterBand(iBand)->GetRasterDataType())
        {
            if (bStrict)
            {
                ReportError(
                    pszFilename, CE_Failure, CPLE_AppDefined,
                    "Unable to export GeoTIFF file with different datatypes "
                    "per different bands. All bands should have the same "
                    "types in TIFF.");
                return nullptr;
            }
            else
            {
                ReportError(
                    pszFilename, CE_Warning, CPLE_AppDefined,
                    "Unable to export GeoTIFF file with different datatypes "
                    "per different bands. All bands should have the same "
                    "types in TIFF.");
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Capture the profile.                                            */
    /* -------------------------------------------------------------------- */
    const GTiffProfile eProfile =
        GetProfile(CSLFetchNameValue(papszOptions, "PROFILE"));

    const bool bGeoTIFF = eProfile != GTiffProfile::BASELINE;

    /* -------------------------------------------------------------------- */
    /*      Special handling for NBITS.  Copy from band metadata if found.  */
    /* -------------------------------------------------------------------- */
    char **papszCreateOptions = CSLDuplicate(papszOptions);

    if (poPBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE") != nullptr &&
        atoi(poPBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE")) > 0 &&
        CSLFetchNameValue(papszCreateOptions, "NBITS") == nullptr)
    {
        papszCreateOptions = CSLSetNameValue(
            papszCreateOptions, "NBITS",
            poPBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE"));
    }

    if (CSLFetchNameValue(papszOptions, "PIXELTYPE") == nullptr &&
        eType == GDT_Byte)
    {
        poPBand->EnablePixelTypeSignedByteWarning(false);
        const char *pszPixelType =
            poPBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        poPBand->EnablePixelTypeSignedByteWarning(true);
        if (pszPixelType)
        {
            papszCreateOptions =
                CSLSetNameValue(papszCreateOptions, "PIXELTYPE", pszPixelType);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Color profile.  Copy from band metadata if found.              */
    /* -------------------------------------------------------------------- */
    if (bGeoTIFF)
    {
        const char *pszOptionsMD[] = {"SOURCE_ICC_PROFILE",
                                      "SOURCE_PRIMARIES_RED",
                                      "SOURCE_PRIMARIES_GREEN",
                                      "SOURCE_PRIMARIES_BLUE",
                                      "SOURCE_WHITEPOINT",
                                      "TIFFTAG_TRANSFERFUNCTION_RED",
                                      "TIFFTAG_TRANSFERFUNCTION_GREEN",
                                      "TIFFTAG_TRANSFERFUNCTION_BLUE",
                                      "TIFFTAG_TRANSFERRANGE_BLACK",
                                      "TIFFTAG_TRANSFERRANGE_WHITE",
                                      nullptr};

        // Copy all the tags.  Options will override tags in the source.
        int i = 0;
        while (pszOptionsMD[i] != nullptr)
        {
            char const *pszMD =
                CSLFetchNameValue(papszOptions, pszOptionsMD[i]);
            if (pszMD == nullptr)
                pszMD =
                    poSrcDS->GetMetadataItem(pszOptionsMD[i], "COLOR_PROFILE");

            if ((pszMD != nullptr) && !EQUAL(pszMD, ""))
            {
                papszCreateOptions =
                    CSLSetNameValue(papszCreateOptions, pszOptionsMD[i], pszMD);

                // If an ICC profile exists, other tags are not needed.
                if (EQUAL(pszOptionsMD[i], "SOURCE_ICC_PROFILE"))
                    break;
            }

            ++i;
        }
    }

    double dfExtraSpaceForOverviews = 0;
    const bool bCopySrcOverviews =
        CPLFetchBool(papszCreateOptions, "COPY_SRC_OVERVIEWS", false);
    std::unique_ptr<GDALDataset> poOvrDS;
    int nSrcOverviews = 0;
    if (bCopySrcOverviews)
    {
        const char *pszOvrDS =
            CSLFetchNameValue(papszCreateOptions, "@OVERVIEW_DATASET");
        if (pszOvrDS)
        {
            // Empty string is used by COG driver to indicate that we want
            // to ignore source overviews.
            if (!EQUAL(pszOvrDS, ""))
            {
                poOvrDS.reset(GDALDataset::Open(pszOvrDS));
                if (!poOvrDS)
                {
                    CSLDestroy(papszCreateOptions);
                    return nullptr;
                }
                if (poOvrDS->GetRasterCount() != l_nBands)
                {
                    CSLDestroy(papszCreateOptions);
                    return nullptr;
                }
                nSrcOverviews =
                    poOvrDS->GetRasterBand(1)->GetOverviewCount() + 1;
            }
        }
        else
        {
            nSrcOverviews = poSrcDS->GetRasterBand(1)->GetOverviewCount();
        }

        // Limit number of overviews if specified
        const char *pszOverviewCount =
            CSLFetchNameValue(papszCreateOptions, "@OVERVIEW_COUNT");
        if (pszOverviewCount)
            nSrcOverviews =
                std::max(0, std::min(nSrcOverviews, atoi(pszOverviewCount)));

        if (nSrcOverviews)
        {
            for (int j = 1; j <= l_nBands; ++j)
            {
                const int nOtherBandOverviewCount =
                    poOvrDS ? poOvrDS->GetRasterBand(j)->GetOverviewCount() + 1
                            : poSrcDS->GetRasterBand(j)->GetOverviewCount();
                if (nOtherBandOverviewCount < nSrcOverviews)
                {
                    ReportError(
                        pszFilename, CE_Failure, CPLE_NotSupported,
                        "COPY_SRC_OVERVIEWS cannot be used when the bands have "
                        "not the same number of overview levels.");
                    CSLDestroy(papszCreateOptions);
                    return nullptr;
                }
                for (int i = 0; i < nSrcOverviews; ++i)
                {
                    GDALRasterBand *poOvrBand =
                        poOvrDS
                            ? (i == 0 ? poOvrDS->GetRasterBand(j)
                                      : poOvrDS->GetRasterBand(j)->GetOverview(
                                            i - 1))
                            : poSrcDS->GetRasterBand(j)->GetOverview(i);
                    if (poOvrBand == nullptr)
                    {
                        ReportError(
                            pszFilename, CE_Failure, CPLE_NotSupported,
                            "COPY_SRC_OVERVIEWS cannot be used when one "
                            "overview band is NULL.");
                        CSLDestroy(papszCreateOptions);
                        return nullptr;
                    }
                    GDALRasterBand *poOvrFirstBand =
                        poOvrDS
                            ? (i == 0 ? poOvrDS->GetRasterBand(1)
                                      : poOvrDS->GetRasterBand(1)->GetOverview(
                                            i - 1))
                            : poSrcDS->GetRasterBand(1)->GetOverview(i);
                    if (poOvrBand->GetXSize() != poOvrFirstBand->GetXSize() ||
                        poOvrBand->GetYSize() != poOvrFirstBand->GetYSize())
                    {
                        ReportError(
                            pszFilename, CE_Failure, CPLE_NotSupported,
                            "COPY_SRC_OVERVIEWS cannot be used when the "
                            "overview bands have not the same dimensions "
                            "among bands.");
                        CSLDestroy(papszCreateOptions);
                        return nullptr;
                    }
                }
            }

            for (int i = 0; i < nSrcOverviews; ++i)
            {
                GDALRasterBand *poOvrFirstBand =
                    poOvrDS
                        ? (i == 0
                               ? poOvrDS->GetRasterBand(1)
                               : poOvrDS->GetRasterBand(1)->GetOverview(i - 1))
                        : poSrcDS->GetRasterBand(1)->GetOverview(i);
                dfExtraSpaceForOverviews +=
                    static_cast<double>(poOvrFirstBand->GetXSize()) *
                    poOvrFirstBand->GetYSize();
            }
            dfExtraSpaceForOverviews *=
                l_nBands * GDALGetDataTypeSizeBytes(eType);
        }
        else
        {
            CPLDebug("GTiff", "No source overviews to copy");
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we use optimized way of copying from an input JPEG       */
/*      dataset?                                                        */
/* -------------------------------------------------------------------- */

// TODO(schwehr): Refactor bDirectCopyFromJPEG to be a const.
#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
    bool bDirectCopyFromJPEG = false;
#endif

    // Note: JPEG_DIRECT_COPY is not defined by default, because it is mainly
    // useful for debugging purposes.
#ifdef JPEG_DIRECT_COPY
    if (CPLFetchBool(papszCreateOptions, "JPEG_DIRECT_COPY", false) &&
        GTIFF_CanDirectCopyFromJPEG(poSrcDS, papszCreateOptions))
    {
        CPLDebug("GTiff", "Using special direct copy mode from a JPEG dataset");

        bDirectCopyFromJPEG = true;
    }
#endif

#ifdef HAVE_LIBJPEG
    bool bCopyFromJPEG = false;

    // When CreateCopy'ing() from a JPEG dataset, and asking for COMPRESS=JPEG,
    // use DCT coefficients (unless other options are incompatible, like
    // strip/tile dimensions, specifying JPEG_QUALITY option, incompatible
    // PHOTOMETRIC with the source colorspace, etc.) to avoid the lossy steps
    // involved by decompression/recompression.
    if (!bDirectCopyFromJPEG &&
        GTIFF_CanCopyFromJPEG(poSrcDS, papszCreateOptions))
    {
        CPLDebug("GTiff", "Using special copy mode from a JPEG dataset");

        bCopyFromJPEG = true;
    }
#endif

    /* -------------------------------------------------------------------- */
    /*      If the source is RGB, then set the PHOTOMETRIC=RGB value        */
    /* -------------------------------------------------------------------- */

    const bool bForcePhotometric =
        CSLFetchNameValue(papszOptions, "PHOTOMETRIC") != nullptr;

    if (l_nBands >= 3 && !bForcePhotometric &&
#ifdef HAVE_LIBJPEG
        !bCopyFromJPEG &&
#endif
        poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_RedBand &&
        poSrcDS->GetRasterBand(2)->GetColorInterpretation() == GCI_GreenBand &&
        poSrcDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand)
    {
        papszCreateOptions =
            CSLSetNameValue(papszCreateOptions, "PHOTOMETRIC", "RGB");
    }

    /* -------------------------------------------------------------------- */
    /*      Create the file.                                                */
    /* -------------------------------------------------------------------- */
    VSILFILE *l_fpL = nullptr;
    CPLString l_osTmpFilename;

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    TIFF *l_hTIFF = CreateLL(pszFilename, nXSize, nYSize, l_nBands, eType,
                             dfExtraSpaceForOverviews, papszCreateOptions,
                             &l_fpL, l_osTmpFilename);
    const bool bStreaming = !l_osTmpFilename.empty();

    CSLDestroy(papszCreateOptions);
    papszCreateOptions = nullptr;

    if (l_hTIFF == nullptr)
    {
        if (bStreaming)
            VSIUnlink(l_osTmpFilename);
        return nullptr;
    }

    uint16_t l_nPlanarConfig = 0;
    TIFFGetField(l_hTIFF, TIFFTAG_PLANARCONFIG, &l_nPlanarConfig);

    uint16_t l_nCompression = 0;

    if (!TIFFGetField(l_hTIFF, TIFFTAG_COMPRESSION, &(l_nCompression)))
        l_nCompression = COMPRESSION_NONE;

    /* -------------------------------------------------------------------- */
    /*      Set the alpha channel if we find one.                           */
    /* -------------------------------------------------------------------- */
    uint16_t *extraSamples = nullptr;
    uint16_t nExtraSamples = 0;
    if (TIFFGetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, &nExtraSamples,
                     &extraSamples) &&
        nExtraSamples > 0)
    {
        // We need to allocate a new array as (current) libtiff
        // versions will not like that we reuse the array we got from
        // TIFFGetField().
        uint16_t *pasNewExtraSamples = static_cast<uint16_t *>(
            CPLMalloc(nExtraSamples * sizeof(uint16_t)));
        memcpy(pasNewExtraSamples, extraSamples,
               nExtraSamples * sizeof(uint16_t));
        uint16_t nAlpha = GTiffGetAlphaValue(
            CPLGetConfigOption("GTIFF_ALPHA",
                               CSLFetchNameValue(papszOptions, "ALPHA")),
            DEFAULT_ALPHA_TYPE);
        const int nBaseSamples = l_nBands - nExtraSamples;
        for (int iExtraBand = nBaseSamples + 1; iExtraBand <= l_nBands;
             iExtraBand++)
        {
            if (poSrcDS->GetRasterBand(iExtraBand)->GetColorInterpretation() ==
                GCI_AlphaBand)
            {
                pasNewExtraSamples[iExtraBand - nBaseSamples - 1] = nAlpha;
            }
        }
        TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, nExtraSamples,
                     pasNewExtraSamples);

        CPLFree(pasNewExtraSamples);
    }

    /* -------------------------------------------------------------------- */
    /*      If the output is jpeg compressed, and the input is RGB make     */
    /*      sure we note that.                                              */
    /* -------------------------------------------------------------------- */

    if (l_nCompression == COMPRESSION_JPEG)
    {
        if (l_nBands >= 3 &&
            (poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
             GCI_YCbCr_YBand) &&
            (poSrcDS->GetRasterBand(2)->GetColorInterpretation() ==
             GCI_YCbCr_CbBand) &&
            (poSrcDS->GetRasterBand(3)->GetColorInterpretation() ==
             GCI_YCbCr_CrBand))
        {
            // Do nothing.
        }
        else
        {
            // Assume RGB if it is not explicitly YCbCr.
            CPLDebug("GTiff", "Setting JPEGCOLORMODE_RGB");
            TIFFSetField(l_hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Does the source image consist of one band, with a palette?      */
    /*      If so, copy over.                                               */
    /* -------------------------------------------------------------------- */
    if ((l_nBands == 1 || l_nBands == 2) &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr &&
        eType == GDT_Byte)
    {
        unsigned short anTRed[256] = {0};
        unsigned short anTGreen[256] = {0};
        unsigned short anTBlue[256] = {0};
        GDALColorTable *poCT = poSrcDS->GetRasterBand(1)->GetColorTable();

        for (int iColor = 0; iColor < 256; ++iColor)
        {
            if (iColor < poCT->GetColorEntryCount())
            {
                GDALColorEntry sRGB = {0, 0, 0, 0};

                poCT->GetColorEntryAsRGB(iColor, &sRGB);

                anTRed[iColor] = static_cast<unsigned short>(257 * sRGB.c1);
                anTGreen[iColor] = static_cast<unsigned short>(257 * sRGB.c2);
                anTBlue[iColor] = static_cast<unsigned short>(257 * sRGB.c3);
            }
            else
            {
                anTRed[iColor] = 0;
                anTGreen[iColor] = 0;
                anTBlue[iColor] = 0;
            }
        }

        if (!bForcePhotometric)
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
        TIFFSetField(l_hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue);
    }
    else if ((l_nBands == 1 || l_nBands == 2) &&
             poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr &&
             eType == GDT_UInt16)
    {
        unsigned short *panTRed = static_cast<unsigned short *>(
            CPLMalloc(65536 * sizeof(unsigned short)));
        unsigned short *panTGreen = static_cast<unsigned short *>(
            CPLMalloc(65536 * sizeof(unsigned short)));
        unsigned short *panTBlue = static_cast<unsigned short *>(
            CPLMalloc(65536 * sizeof(unsigned short)));

        GDALColorTable *poCT = poSrcDS->GetRasterBand(1)->GetColorTable();

        for (int iColor = 0; iColor < 65536; ++iColor)
        {
            if (iColor < poCT->GetColorEntryCount())
            {
                GDALColorEntry sRGB = {0, 0, 0, 0};

                poCT->GetColorEntryAsRGB(iColor, &sRGB);

                panTRed[iColor] = static_cast<unsigned short>(257 * sRGB.c1);
                panTGreen[iColor] = static_cast<unsigned short>(257 * sRGB.c2);
                panTBlue[iColor] = static_cast<unsigned short>(257 * sRGB.c3);
            }
            else
            {
                panTRed[iColor] = 0;
                panTGreen[iColor] = 0;
                panTBlue[iColor] = 0;
            }
        }

        if (!bForcePhotometric)
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
        TIFFSetField(l_hTIFF, TIFFTAG_COLORMAP, panTRed, panTGreen, panTBlue);

        CPLFree(panTRed);
        CPLFree(panTGreen);
        CPLFree(panTBlue);
    }
    else if (poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr)
        ReportError(
            pszFilename, CE_Failure, CPLE_AppDefined,
            "Unable to export color table to GeoTIFF file.  Color tables "
            "can only be written to 1 band or 2 bands Byte or "
            "UInt16 GeoTIFF files.");

    if (l_nCompression == COMPRESSION_JPEG)
    {
        uint16_t l_nPhotometric = 0;
        TIFFGetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, &l_nPhotometric);
        // Check done in tif_jpeg.c later, but not with a very clear error
        // message
        if (l_nPhotometric == PHOTOMETRIC_PALETTE)
        {
            ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                        "JPEG compression not supported with paletted image");
            XTIFFClose(l_hTIFF);
            VSIUnlink(l_osTmpFilename);
            CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
            return nullptr;
        }
    }

    if (l_nBands == 2 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr &&
        (eType == GDT_Byte || eType == GDT_UInt16))
    {
        uint16_t v[1] = {EXTRASAMPLE_UNASSALPHA};

        TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
    }

    const int nMaskFlags = poSrcDS->GetRasterBand(1)->GetMaskFlags();
    bool bCreateMask = false;
    CPLString osHiddenStructuralMD;
    if ((l_nBands == 1 || l_nPlanarConfig == PLANARCONFIG_CONTIG) &&
        bCopySrcOverviews)
    {
        osHiddenStructuralMD += "LAYOUT=IFDS_BEFORE_DATA\n";
        osHiddenStructuralMD += "BLOCK_ORDER=ROW_MAJOR\n";
        osHiddenStructuralMD += "BLOCK_LEADER=SIZE_AS_UINT4\n";
        osHiddenStructuralMD += "BLOCK_TRAILER=LAST_4_BYTES_REPEATED\n";
        osHiddenStructuralMD +=
            "KNOWN_INCOMPATIBLE_EDITION=NO\n ";  // Final space intended, so
                                                 // this can be replaced by YES
    }
    if (!(nMaskFlags & (GMF_ALL_VALID | GMF_ALPHA | GMF_NODATA)) &&
        (nMaskFlags & GMF_PER_DATASET) && !bStreaming)
    {
        bCreateMask = true;
        if (GTiffDataset::MustCreateInternalMask() &&
            !osHiddenStructuralMD.empty())
        {
            osHiddenStructuralMD += "MASK_INTERLEAVED_WITH_IMAGERY=YES\n";
        }
    }
    if (!osHiddenStructuralMD.empty())
    {
        const int nHiddenMDSize = static_cast<int>(osHiddenStructuralMD.size());
        osHiddenStructuralMD =
            CPLOPrintf("GDAL_STRUCTURAL_METADATA_SIZE=%06d bytes\n",
                       nHiddenMDSize) +
            osHiddenStructuralMD;
        VSI_TIFFWrite(l_hTIFF, osHiddenStructuralMD.c_str(),
                      osHiddenStructuralMD.size());
    }

    // FIXME? libtiff writes extended tags in the order they are specified
    // and not in increasing order.

    /* -------------------------------------------------------------------- */
    /*      Transfer some TIFF specific metadata, if available.             */
    /*      The return value will tell us if we need to try again later with*/
    /*      PAM because the profile doesn't allow to write some metadata    */
    /*      as TIFF tag                                                     */
    /* -------------------------------------------------------------------- */
    const bool bHasWrittenMDInGeotiffTAG = GTiffDataset::WriteMetadata(
        poSrcDS, l_hTIFF, false, eProfile, pszFilename, papszOptions);

    /* -------------------------------------------------------------------- */
    /*      Write NoData value, if exist.                                   */
    /* -------------------------------------------------------------------- */
    if (eProfile == GTiffProfile::GDALGEOTIFF)
    {
        int bSuccess = FALSE;
        GDALRasterBand *poFirstBand = poSrcDS->GetRasterBand(1);
        if (poFirstBand->GetRasterDataType() == GDT_Int64)
        {
            const auto nNoData = poFirstBand->GetNoDataValueAsInt64(&bSuccess);
            if (bSuccess)
                GTiffDataset::WriteNoDataValue(l_hTIFF, nNoData);
        }
        else if (poFirstBand->GetRasterDataType() == GDT_UInt64)
        {
            const auto nNoData = poFirstBand->GetNoDataValueAsUInt64(&bSuccess);
            if (bSuccess)
                GTiffDataset::WriteNoDataValue(l_hTIFF, nNoData);
        }
        else
        {
            const auto dfNoData = poFirstBand->GetNoDataValue(&bSuccess);
            if (bSuccess)
                GTiffDataset::WriteNoDataValue(l_hTIFF, dfNoData);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Are we addressing PixelIsPoint mode?                            */
    /* -------------------------------------------------------------------- */
    bool bPixelIsPoint = false;
    bool bPointGeoIgnore = false;

    if (poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT) &&
        EQUAL(poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT), GDALMD_AOP_POINT))
    {
        bPixelIsPoint = true;
        bPointGeoIgnore =
            CPLTestBool(CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE", "FALSE"));
    }

    /* -------------------------------------------------------------------- */
    /*      Write affine transform if it is meaningful.                     */
    /* -------------------------------------------------------------------- */
    const OGRSpatialReference *l_poSRS = nullptr;
    double l_adfGeoTransform[6] = {0.0};

    if (poSrcDS->GetGeoTransform(l_adfGeoTransform) == CE_None)
    {
        if (bGeoTIFF)
        {
            l_poSRS = poSrcDS->GetSpatialRef();

            if (l_adfGeoTransform[2] == 0.0 && l_adfGeoTransform[4] == 0.0 &&
                l_adfGeoTransform[5] < 0.0)
            {
                double dfOffset = 0.0;
                {
                    // In the case the SRS has a vertical component and we have
                    // a single band, encode its scale/offset in the GeoTIFF
                    // tags
                    int bHasScale = FALSE;
                    double dfScale =
                        poSrcDS->GetRasterBand(1)->GetScale(&bHasScale);
                    int bHasOffset = FALSE;
                    dfOffset =
                        poSrcDS->GetRasterBand(1)->GetOffset(&bHasOffset);
                    const bool bApplyScaleOffset =
                        l_poSRS && l_poSRS->IsVertical() &&
                        poSrcDS->GetRasterCount() == 1;
                    if (bApplyScaleOffset && !bHasScale)
                        dfScale = 1.0;
                    if (!bApplyScaleOffset || !bHasOffset)
                        dfOffset = 0.0;
                    const double adfPixelScale[3] = {
                        l_adfGeoTransform[1], fabs(l_adfGeoTransform[5]),
                        bApplyScaleOffset ? dfScale : 0.0};

                    TIFFSetField(l_hTIFF, TIFFTAG_GEOPIXELSCALE, 3,
                                 adfPixelScale);
                }

                double adfTiePoints[6] = {0.0,
                                          0.0,
                                          0.0,
                                          l_adfGeoTransform[0],
                                          l_adfGeoTransform[3],
                                          dfOffset};

                if (bPixelIsPoint && !bPointGeoIgnore)
                {
                    adfTiePoints[3] +=
                        l_adfGeoTransform[1] * 0.5 + l_adfGeoTransform[2] * 0.5;
                    adfTiePoints[4] +=
                        l_adfGeoTransform[4] * 0.5 + l_adfGeoTransform[5] * 0.5;
                }

                TIFFSetField(l_hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints);
            }
            else
            {
                double adfMatrix[16] = {0.0};

                adfMatrix[0] = l_adfGeoTransform[1];
                adfMatrix[1] = l_adfGeoTransform[2];
                adfMatrix[3] = l_adfGeoTransform[0];
                adfMatrix[4] = l_adfGeoTransform[4];
                adfMatrix[5] = l_adfGeoTransform[5];
                adfMatrix[7] = l_adfGeoTransform[3];
                adfMatrix[15] = 1.0;

                if (bPixelIsPoint && !bPointGeoIgnore)
                {
                    adfMatrix[3] +=
                        l_adfGeoTransform[1] * 0.5 + l_adfGeoTransform[2] * 0.5;
                    adfMatrix[7] +=
                        l_adfGeoTransform[4] * 0.5 + l_adfGeoTransform[5] * 0.5;
                }

                TIFFSetField(l_hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix);
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Do we need a TFW file? */
        /* --------------------------------------------------------------------
         */
        if (CPLFetchBool(papszOptions, "TFW", false))
            GDALWriteWorldFile(pszFilename, "tfw", l_adfGeoTransform);
        else if (CPLFetchBool(papszOptions, "WORLDFILE", false))
            GDALWriteWorldFile(pszFilename, "wld", l_adfGeoTransform);
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise write tiepoints if they are available.                */
    /* -------------------------------------------------------------------- */
    else if (poSrcDS->GetGCPCount() > 0 && bGeoTIFF)
    {
        const GDAL_GCP *pasGCPs = poSrcDS->GetGCPs();
        double *padfTiePoints = static_cast<double *>(
            CPLMalloc(6 * sizeof(double) * poSrcDS->GetGCPCount()));

        for (int iGCP = 0; iGCP < poSrcDS->GetGCPCount(); ++iGCP)
        {

            padfTiePoints[iGCP * 6 + 0] = pasGCPs[iGCP].dfGCPPixel;
            padfTiePoints[iGCP * 6 + 1] = pasGCPs[iGCP].dfGCPLine;
            padfTiePoints[iGCP * 6 + 2] = 0;
            padfTiePoints[iGCP * 6 + 3] = pasGCPs[iGCP].dfGCPX;
            padfTiePoints[iGCP * 6 + 4] = pasGCPs[iGCP].dfGCPY;
            padfTiePoints[iGCP * 6 + 5] = pasGCPs[iGCP].dfGCPZ;

            if (bPixelIsPoint && !bPointGeoIgnore)
            {
                padfTiePoints[iGCP * 6 + 0] -= 0.5;
                padfTiePoints[iGCP * 6 + 1] -= 0.5;
            }
        }

        TIFFSetField(l_hTIFF, TIFFTAG_GEOTIEPOINTS, 6 * poSrcDS->GetGCPCount(),
                     padfTiePoints);
        CPLFree(padfTiePoints);

        l_poSRS = poSrcDS->GetGCPSpatialRef();

        if (CPLFetchBool(papszOptions, "TFW", false) ||
            CPLFetchBool(papszOptions, "WORLDFILE", false))
        {
            ReportError(
                pszFilename, CE_Warning, CPLE_AppDefined,
                "TFW=ON or WORLDFILE=ON creation options are ignored when "
                "GCPs are available");
        }
    }
    else
    {
        l_poSRS = poSrcDS->GetSpatialRef();
    }

    /* -------------------------------------------------------------------- */
    /*      Copy xml:XMP data                                               */
    /* -------------------------------------------------------------------- */
    char **papszXMP = poSrcDS->GetMetadata("xml:XMP");
    if (papszXMP != nullptr && *papszXMP != nullptr)
    {
        int nTagSize = static_cast<int>(strlen(*papszXMP));
        TIFFSetField(l_hTIFF, TIFFTAG_XMLPACKET, nTagSize, *papszXMP);
    }

    /* -------------------------------------------------------------------- */
    /*      Write the projection information, if possible.                  */
    /* -------------------------------------------------------------------- */
    const bool bHasProjection = l_poSRS != nullptr;
    bool bExportSRSToPAM = false;
    if ((bHasProjection || bPixelIsPoint) && bGeoTIFF)
    {
        GTIF *psGTIF = GTiffDataset::GTIFNew(l_hTIFF);

        if (bHasProjection)
        {
            const auto eGeoTIFFKeysFlavor = GetGTIFFKeysFlavor(papszOptions);
            if (IsSRSCompatibleOfGeoTIFF(l_poSRS, eGeoTIFFKeysFlavor))
            {
                GTIFSetFromOGISDefnEx(
                    psGTIF,
                    OGRSpatialReference::ToHandle(
                        const_cast<OGRSpatialReference *>(l_poSRS)),
                    eGeoTIFFKeysFlavor, GetGeoTIFFVersion(papszOptions));
            }
            else
            {
                bExportSRSToPAM = true;
            }
        }

        if (bPixelIsPoint)
        {
            GTIFKeySet(psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1,
                       RasterPixelIsPoint);
        }

        GTIFWriteKeys(psGTIF);
        GTIFFree(psGTIF);
    }

    bool l_bDontReloadFirstBlock = false;

#ifdef HAVE_LIBJPEG
    if (bCopyFromJPEG)
    {
        GTIFF_CopyFromJPEG_WriteAdditionalTags(l_hTIFF, poSrcDS);
    }
#endif

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    if (bCopySrcOverviews)
    {
        TIFFDeferStrileArrayWriting(l_hTIFF);
    }
    TIFFWriteCheck(l_hTIFF, TIFFIsTiled(l_hTIFF), "GTiffCreateCopy()");
    TIFFWriteDirectory(l_hTIFF);
    if (bStreaming)
    {
        // We need to write twice the directory to be sure that custom
        // TIFF tags are correctly sorted and that padding bytes have been
        // added.
        TIFFSetDirectory(l_hTIFF, 0);
        TIFFWriteDirectory(l_hTIFF);

        if (VSIFSeekL(l_fpL, 0, SEEK_END) != 0)
            ReportError(pszFilename, CE_Failure, CPLE_FileIO, "Cannot seek");
        const int nSize = static_cast<int>(VSIFTellL(l_fpL));

        vsi_l_offset nDataLength = 0;
        VSIGetMemFileBuffer(l_osTmpFilename, &nDataLength, FALSE);
        TIFFSetDirectory(l_hTIFF, 0);
        GTiffFillStreamableOffsetAndCount(l_hTIFF, nSize);
        TIFFWriteDirectory(l_hTIFF);
    }
    const auto nDirCount = TIFFNumberOfDirectories(l_hTIFF);
    if (nDirCount >= 1)
    {
        TIFFSetDirectory(l_hTIFF, static_cast<tdir_t>(nDirCount - 1));
    }
    const toff_t l_nDirOffset = TIFFCurrentDirOffset(l_hTIFF);
    TIFFFlush(l_hTIFF);
    XTIFFClose(l_hTIFF);

    VSIFSeekL(l_fpL, 0, SEEK_SET);

    // fpStreaming will assigned to the instance and not closed here.
    VSILFILE *fpStreaming = nullptr;
    if (bStreaming)
    {
        vsi_l_offset nDataLength = 0;
        void *pabyBuffer =
            VSIGetMemFileBuffer(l_osTmpFilename, &nDataLength, FALSE);
        fpStreaming = VSIFOpenL(pszFilename, "wb");
        if (fpStreaming == nullptr)
        {
            VSIUnlink(l_osTmpFilename);
            CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
            return nullptr;
        }
        if (static_cast<vsi_l_offset>(VSIFWriteL(pabyBuffer, 1,
                                                 static_cast<int>(nDataLength),
                                                 fpStreaming)) != nDataLength)
        {
            ReportError(pszFilename, CE_Failure, CPLE_FileIO,
                        "Could not write %d bytes",
                        static_cast<int>(nDataLength));
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpStreaming));
            VSIUnlink(l_osTmpFilename);
            CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Re-open as a dataset and copy over missing metadata using       */
    /*      PAM facilities.                                                 */
    /* -------------------------------------------------------------------- */
    l_hTIFF = VSI_TIFFOpen(bStreaming ? l_osTmpFilename.c_str() : pszFilename,
                           "r+", l_fpL);
    if (l_hTIFF == nullptr)
    {
        if (bStreaming)
            VSIUnlink(l_osTmpFilename);
        CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->SetDescription(pszFilename);
    poDS->eAccess = GA_Update;
    poDS->m_pszFilename = CPLStrdup(pszFilename);
    poDS->m_fpL = l_fpL;
    poDS->m_bIMDRPCMetadataLoaded = true;

    const bool bAppend = CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false);
    if (poDS->OpenOffset(l_hTIFF,
                         bAppend ? l_nDirOffset : TIFFCurrentDirOffset(l_hTIFF),
                         GA_Update,
                         false,  // bAllowRGBAInterface
                         true    // bReadGeoTransform
                         ) != CE_None)
    {
        delete poDS;
        if (bStreaming)
            VSIUnlink(l_osTmpFilename);
        return nullptr;
    }

    // Legacy... Patch back GDT_Int8 type to GDT_Byte if the user used
    // PIXELTYPE=SIGNEDBYTE
    const char *pszPixelType = CSLFetchNameValue(papszOptions, "PIXELTYPE");
    if (pszPixelType == nullptr)
        pszPixelType = "";
    if (eType == GDT_Byte && EQUAL(pszPixelType, "SIGNEDBYTE"))
    {
        for (int i = 0; i < poDS->nBands; ++i)
        {
            auto poBand = static_cast<GTiffRasterBand *>(poDS->papoBands[i]);
            poBand->eDataType = GDT_Byte;
            poBand->EnablePixelTypeSignedByteWarning(false);
            poBand->SetMetadataItem("PIXELTYPE", "SIGNEDBYTE",
                                    "IMAGE_STRUCTURE");
            poBand->EnablePixelTypeSignedByteWarning(true);
        }
    }

    poDS->oOvManager.Initialize(poDS, pszFilename);

    if (bStreaming)
    {
        VSIUnlink(l_osTmpFilename);
        poDS->m_fpToWrite = fpStreaming;
    }
    poDS->m_eProfile = eProfile;

    int nCloneInfoFlags = GCIF_PAM_DEFAULT & ~GCIF_MASK;

    // If we explicitly asked not to tag the alpha band as such, do not
    // reintroduce this alpha color interpretation in PAM.
    if (poSrcDS->GetRasterBand(l_nBands)->GetColorInterpretation() ==
            GCI_AlphaBand &&
        GTiffGetAlphaValue(
            CPLGetConfigOption("GTIFF_ALPHA",
                               CSLFetchNameValue(papszOptions, "ALPHA")),
            DEFAULT_ALPHA_TYPE) == EXTRASAMPLE_UNSPECIFIED)
    {
        nCloneInfoFlags &= ~GCIF_COLORINTERP;
    }
    // Ignore source band color interpretation if requesting PHOTOMETRIC=RGB
    else if (l_nBands >= 3 &&
             EQUAL(CSLFetchNameValueDef(papszOptions, "PHOTOMETRIC", ""),
                   "RGB"))
    {
        for (int i = 1; i <= 3; i++)
        {
            poDS->GetRasterBand(i)->SetColorInterpretation(
                static_cast<GDALColorInterp>(GCI_RedBand + (i - 1)));
        }
        nCloneInfoFlags &= ~GCIF_COLORINTERP;
        if (!(l_nBands == 4 &&
              CSLFetchNameValue(papszOptions, "ALPHA") != nullptr))
        {
            for (int i = 4; i <= l_nBands; i++)
            {
                poDS->GetRasterBand(i)->SetColorInterpretation(
                    poSrcDS->GetRasterBand(i)->GetColorInterpretation());
            }
        }
    }

    CPLString osOldGTIFF_REPORT_COMPD_CSVal(
        CPLGetConfigOption("GTIFF_REPORT_COMPD_CS", ""));
    CPLSetThreadLocalConfigOption("GTIFF_REPORT_COMPD_CS", "YES");
    poDS->CloneInfo(poSrcDS, nCloneInfoFlags);
    CPLSetThreadLocalConfigOption("GTIFF_REPORT_COMPD_CS",
                                  osOldGTIFF_REPORT_COMPD_CSVal.empty()
                                      ? nullptr
                                      : osOldGTIFF_REPORT_COMPD_CSVal.c_str());

    if ((!bGeoTIFF || bExportSRSToPAM) &&
        (poDS->GetPamFlags() & GPF_DISABLED) == 0)
    {
        // Copy georeferencing info to PAM if the profile is not GeoTIFF
        poDS->GDALPamDataset::SetSpatialRef(poDS->GetSpatialRef());
        double adfGeoTransform[6];
        if (poDS->GetGeoTransform(adfGeoTransform) == CE_None)
        {
            poDS->GDALPamDataset::SetGeoTransform(adfGeoTransform);
        }
        poDS->GDALPamDataset::SetGCPs(poDS->GetGCPCount(), poDS->GetGCPs(),
                                      poDS->GetGCPSpatialRef());
    }

    poDS->m_papszCreationOptions = CSLDuplicate(papszOptions);
    poDS->m_bDontReloadFirstBlock = l_bDontReloadFirstBlock;

    /* -------------------------------------------------------------------- */
    /*      CloneInfo() does not merge metadata, it just replaces it        */
    /*      totally.  So we have to merge it.                               */
    /* -------------------------------------------------------------------- */

    char **papszSRC_MD = poSrcDS->GetMetadata();
    char **papszDST_MD = CSLDuplicate(poDS->GetMetadata());

    papszDST_MD = CSLMerge(papszDST_MD, papszSRC_MD);

    poDS->SetMetadata(papszDST_MD);
    CSLDestroy(papszDST_MD);

    // Depending on the PHOTOMETRIC tag, the TIFF file may not have the same
    // band count as the source. Will fail later in GDALDatasetCopyWholeRaster
    // anyway.
    for (int nBand = 1;
         nBand <= std::min(poDS->GetRasterCount(), poSrcDS->GetRasterCount());
         ++nBand)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(nBand);
        GDALRasterBand *poDstBand = poDS->GetRasterBand(nBand);
        papszSRC_MD = poSrcBand->GetMetadata();
        papszDST_MD = CSLDuplicate(poDstBand->GetMetadata());

        papszDST_MD = CSLMerge(papszDST_MD, papszSRC_MD);

        poDstBand->SetMetadata(papszDST_MD);
        CSLDestroy(papszDST_MD);

        char **papszCatNames = poSrcBand->GetCategoryNames();
        if (nullptr != papszCatNames)
            poDstBand->SetCategoryNames(papszCatNames);
    }

    l_hTIFF = static_cast<TIFF *>(poDS->GetInternalHandle(nullptr));

    /* -------------------------------------------------------------------- */
    /*      Handle forcing xml:ESRI data to be written to PAM.              */
    /* -------------------------------------------------------------------- */
    if (CPLTestBool(CPLGetConfigOption("ESRI_XML_PAM", "NO")))
    {
        char **papszESRIMD = poSrcDS->GetMetadata("xml:ESRI");
        if (papszESRIMD)
        {
            poDS->SetMetadata(papszESRIMD, "xml:ESRI");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Second chance: now that we have a PAM dataset, it is possible   */
    /*      to write metadata that we could not write as a TIFF tag.        */
    /* -------------------------------------------------------------------- */
    if (!bHasWrittenMDInGeotiffTAG && !bStreaming)
    {
        GTiffDataset::WriteMetadata(
            poDS, l_hTIFF, true, eProfile, pszFilename, papszOptions,
            true /* don't write RPC and IMD file again */);
    }

    if (!bStreaming)
        GTiffDataset::WriteRPC(poDS, l_hTIFF, true, eProfile, pszFilename,
                               papszOptions,
                               true /* write only in PAM AND if needed */);

    // Propagate ISIS3 or VICAR metadata, but only as PAM metadata.
    for (const char *pszMDD : {"json:ISIS3", "json:VICAR"})
    {
        char **papszMD = poSrcDS->GetMetadata(pszMDD);
        if (papszMD)
        {
            poDS->SetMetadata(papszMD, pszMDD);
            poDS->PushMetadataToPam();
        }
    }

    poDS->m_bWriteCOGLayout = bCopySrcOverviews;

    // To avoid unnecessary directory rewriting.
    poDS->m_bMetadataChanged = false;
    poDS->m_bGeoTIFFInfoChanged = false;
    poDS->m_bNoDataChanged = false;
    poDS->m_bForceUnsetGTOrGCPs = false;
    poDS->m_bForceUnsetProjection = false;
    poDS->m_bStreamingOut = bStreaming;

    // Don't try to load external metadata files (#6597).
    poDS->m_bIMDRPCMetadataLoaded = true;

    // We must re-set the compression level at this point, since it has been
    // lost a few lines above when closing the newly create TIFF file The
    // TIFFTAG_ZIPQUALITY & TIFFTAG_JPEGQUALITY are not store in the TIFF file.
    // They are just TIFF session parameters.

    poDS->m_nZLevel = GTiffGetZLevel(papszOptions);
    poDS->m_nLZMAPreset = GTiffGetLZMAPreset(papszOptions);
    poDS->m_nZSTDLevel = GTiffGetZSTDPreset(papszOptions);
    poDS->m_nWebPLevel = GTiffGetWebPLevel(papszOptions);
    poDS->m_bWebPLossless = GTiffGetWebPLossless(papszOptions);
    if (poDS->m_nWebPLevel != 100 && poDS->m_bWebPLossless &&
        CSLFetchNameValue(papszOptions, "WEBP_LEVEL"))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "WEBP_LEVEL is specified, but WEBP_LOSSLESS=YES. "
                 "WEBP_LEVEL will be ignored.");
    }
    poDS->m_nJpegQuality = GTiffGetJpegQuality(papszOptions);
    poDS->m_nJpegTablesMode = GTiffGetJpegTablesMode(papszOptions);
    poDS->GetDiscardLsbOption(papszOptions);
    poDS->m_dfMaxZError = GTiffGetLERCMaxZError(papszOptions);
    poDS->m_dfMaxZErrorOverview = GTiffGetLERCMaxZErrorOverview(papszOptions);
#if HAVE_JXL
    poDS->m_bJXLLossless = GTiffGetJXLLossless(papszOptions);
    poDS->m_nJXLEffort = GTiffGetJXLEffort(papszOptions);
    poDS->m_fJXLDistance = GTiffGetJXLDistance(papszOptions);
    poDS->m_fJXLAlphaDistance = GTiffGetJXLAlphaDistance(papszOptions);
#endif
    poDS->InitCreationOrOpenOptions(true, papszOptions);

    if (l_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        l_nCompression == COMPRESSION_LERC)
    {
        GTiffSetDeflateSubCodec(l_hTIFF);

        if (poDS->m_nZLevel != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_ZIPQUALITY, poDS->m_nZLevel);
        }
    }
    if (l_nCompression == COMPRESSION_JPEG)
    {
        if (poDS->m_nJpegQuality != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_JPEGQUALITY, poDS->m_nJpegQuality);
        }
        TIFFSetField(l_hTIFF, TIFFTAG_JPEGTABLESMODE, poDS->m_nJpegTablesMode);
    }
    if (l_nCompression == COMPRESSION_LZMA)
    {
        if (poDS->m_nLZMAPreset != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_LZMAPRESET, poDS->m_nLZMAPreset);
        }
    }
    if (l_nCompression == COMPRESSION_ZSTD ||
        l_nCompression == COMPRESSION_LERC)
    {
        if (poDS->m_nZSTDLevel != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_ZSTD_LEVEL, poDS->m_nZSTDLevel);
        }
    }
    if (l_nCompression == COMPRESSION_LERC)
    {
        TIFFSetField(l_hTIFF, TIFFTAG_LERC_MAXZERROR, poDS->m_dfMaxZError);
    }
#if HAVE_JXL
    if (l_nCompression == COMPRESSION_JXL)
    {
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_LOSSYNESS,
                     poDS->m_bJXLLossless ? JXL_LOSSLESS : JXL_LOSSY);
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_EFFORT, poDS->m_nJXLEffort);
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_DISTANCE, poDS->m_fJXLDistance);
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_ALPHA_DISTANCE,
                     poDS->m_fJXLAlphaDistance);
    }
#endif
    if (l_nCompression == COMPRESSION_WEBP)
    {
        if (poDS->m_nWebPLevel != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_WEBP_LEVEL, poDS->m_nWebPLevel);
        }

        if (poDS->m_bWebPLossless)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_WEBP_LOSSLESS, poDS->m_bWebPLossless);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Do we want to ensure all blocks get written out on close to     */
    /*      avoid sparse files?                                             */
    /* -------------------------------------------------------------------- */
    if (!CPLFetchBool(papszOptions, "SPARSE_OK", false))
        poDS->m_bFillEmptyTilesAtClosing = true;

    poDS->m_bWriteEmptyTiles =
        (bCopySrcOverviews && poDS->m_bFillEmptyTilesAtClosing) || bStreaming ||
        (poDS->m_nCompression != COMPRESSION_NONE &&
         poDS->m_bFillEmptyTilesAtClosing);
    // Only required for people writing non-compressed striped files in the
    // rightorder and wanting all tstrips to be written in the same order
    // so that the end result can be memory mapped without knowledge of each
    // strip offset
    if (CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE")) ||
        CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "@WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE")))
    {
        poDS->m_bWriteEmptyTiles = true;
    }

    // Precreate (internal) mask, so that the IBuildOverviews() below
    // has a chance to create also the overviews of the mask.
    CPLErr eErr = CE_None;

    if (bCreateMask)
    {
        eErr = poDS->CreateMaskBand(nMaskFlags);
        if (poDS->m_poMaskDS)
        {
            poDS->m_poMaskDS->m_bFillEmptyTilesAtClosing =
                poDS->m_bFillEmptyTilesAtClosing;
            poDS->m_poMaskDS->m_bWriteEmptyTiles = poDS->m_bWriteEmptyTiles;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create and then copy existing overviews if requested            */
    /*  We do it such that all the IFDs are at the beginning of the file,   */
    /*  and that the imagery data for the smallest overview is written      */
    /*  first, that way the file is more usable when embedded in a          */
    /*  compressed stream.                                                  */
    /* -------------------------------------------------------------------- */

    // For scaled progress due to overview copying.
    const int nBandsWidthMask = l_nBands + (bCreateMask ? 1 : 0);
    double dfTotalPixels =
        static_cast<double>(nXSize) * nYSize * nBandsWidthMask;
    double dfCurPixels = 0;

    if (eErr == CE_None && bCopySrcOverviews)
    {
        std::unique_ptr<GDALDataset> poMaskOvrDS;
        const char *pszMaskOvrDS =
            CSLFetchNameValue(papszOptions, "@MASK_OVERVIEW_DATASET");
        if (pszMaskOvrDS)
        {
            poMaskOvrDS.reset(GDALDataset::Open(pszMaskOvrDS));
            if (!poMaskOvrDS)
            {
                delete poDS;
                return nullptr;
            }
            if (poMaskOvrDS->GetRasterCount() != 1)
            {
                delete poDS;
                return nullptr;
            }
        }
        if (nSrcOverviews)
        {
            eErr = poDS->CreateOverviewsFromSrcOverviews(poSrcDS, poOvrDS.get(),
                                                         nSrcOverviews);

            if (eErr == CE_None &&
                (poMaskOvrDS != nullptr ||
                 (poSrcDS->GetRasterBand(1)->GetOverview(0) &&
                  poSrcDS->GetRasterBand(1)->GetOverview(0)->GetMaskFlags() ==
                      GMF_PER_DATASET)))
            {
                int nOvrBlockXSize = 0;
                int nOvrBlockYSize = 0;
                GTIFFGetOverviewBlockSize(
                    GDALRasterBand::ToHandle(poDS->GetRasterBand(1)),
                    &nOvrBlockXSize, &nOvrBlockYSize);
                eErr = poDS->CreateInternalMaskOverviews(nOvrBlockXSize,
                                                         nOvrBlockYSize);
            }
        }

        TIFFForceStrileArrayWriting(poDS->m_hTIFF);

        if (poDS->m_poMaskDS)
        {
            TIFFForceStrileArrayWriting(poDS->m_poMaskDS->m_hTIFF);
        }

        for (int i = 0; i < poDS->m_nOverviewCount; i++)
        {
            TIFFForceStrileArrayWriting(poDS->m_papoOverviewDS[i]->m_hTIFF);

            if (poDS->m_papoOverviewDS[i]->m_poMaskDS)
            {
                TIFFForceStrileArrayWriting(
                    poDS->m_papoOverviewDS[i]->m_poMaskDS->m_hTIFF);
            }
        }

        if (eErr == CE_None && nSrcOverviews)
        {
            if (poDS->m_nOverviewCount != nSrcOverviews)
            {
                ReportError(
                    pszFilename, CE_Failure, CPLE_AppDefined,
                    "Did only manage to instantiate %d overview levels, "
                    "whereas source contains %d",
                    poDS->m_nOverviewCount, nSrcOverviews);
                eErr = CE_Failure;
            }

            for (int i = 0; eErr == CE_None && i < nSrcOverviews; ++i)
            {
                GDALRasterBand *poOvrBand =
                    poOvrDS
                        ? (i == 0
                               ? poOvrDS->GetRasterBand(1)
                               : poOvrDS->GetRasterBand(1)->GetOverview(i - 1))
                        : poSrcDS->GetRasterBand(1)->GetOverview(i);
                const double dfOvrPixels =
                    static_cast<double>(poOvrBand->GetXSize()) *
                    poOvrBand->GetYSize();
                dfTotalPixels += dfOvrPixels * l_nBands;
                if (poOvrBand->GetMaskFlags() == GMF_PER_DATASET ||
                    poMaskOvrDS != nullptr)
                {
                    dfTotalPixels += dfOvrPixels;
                }
                else if (i == 0 && poDS->GetRasterBand(1)->GetMaskFlags() ==
                                       GMF_PER_DATASET)
                {
                    ReportError(pszFilename, CE_Warning, CPLE_AppDefined,
                                "Source dataset has a mask band on full "
                                "resolution, overviews on the regular bands, "
                                "but lacks overviews on the mask band.");
                }
            }

            char *papszCopyWholeRasterOptions[2] = {nullptr, nullptr};
            if (l_nCompression != COMPRESSION_NONE)
                papszCopyWholeRasterOptions[0] =
                    const_cast<char *>("COMPRESSED=YES");
            // Now copy the imagery.
            // Begin with the smallest overview.
            for (int iOvrLevel = nSrcOverviews - 1;
                 eErr == CE_None && iOvrLevel >= 0; --iOvrLevel)
            {
                auto poDstDS = poDS->m_papoOverviewDS[iOvrLevel];

                // Create a fake dataset with the source overview level so that
                // GDALDatasetCopyWholeRaster can cope with it.
                GDALDataset *poSrcOvrDS =
                    poOvrDS
                        ? (iOvrLevel == 0 ? poOvrDS.get()
                                          : GDALCreateOverviewDataset(
                                                poOvrDS.get(), iOvrLevel - 1,
                                                /* bThisLevelOnly = */ true))
                        : GDALCreateOverviewDataset(
                              poSrcDS, iOvrLevel,
                              /* bThisLevelOnly = */ true);
                GDALRasterBand *poSrcOvrBand =
                    poOvrDS ? (iOvrLevel == 0
                                   ? poOvrDS->GetRasterBand(1)
                                   : poOvrDS->GetRasterBand(1)->GetOverview(
                                         iOvrLevel - 1))
                            : poSrcDS->GetRasterBand(1)->GetOverview(iOvrLevel);
                double dfNextCurPixels =
                    dfCurPixels +
                    static_cast<double>(poSrcOvrBand->GetXSize()) *
                        poSrcOvrBand->GetYSize() * l_nBands;

                poDstDS->m_bBlockOrderRowMajor = true;
                poDstDS->m_bLeaderSizeAsUInt4 = true;
                poDstDS->m_bTrailerRepeatedLast4BytesRepeated = true;
                poDstDS->m_bFillEmptyTilesAtClosing =
                    poDS->m_bFillEmptyTilesAtClosing;
                poDstDS->m_bWriteEmptyTiles = poDS->m_bWriteEmptyTiles;
                GDALRasterBand *poSrcMaskBand = nullptr;
                if (poDstDS->m_poMaskDS)
                {
                    poDstDS->m_poMaskDS->m_bBlockOrderRowMajor = true;
                    poDstDS->m_poMaskDS->m_bLeaderSizeAsUInt4 = true;
                    poDstDS->m_poMaskDS->m_bTrailerRepeatedLast4BytesRepeated =
                        true;
                    poDstDS->m_poMaskDS->m_bFillEmptyTilesAtClosing =
                        poDS->m_bFillEmptyTilesAtClosing;
                    poDstDS->m_poMaskDS->m_bWriteEmptyTiles =
                        poDS->m_bWriteEmptyTiles;

                    poSrcMaskBand =
                        poMaskOvrDS
                            ? (iOvrLevel == 0
                                   ? poMaskOvrDS->GetRasterBand(1)
                                   : poMaskOvrDS->GetRasterBand(1)->GetOverview(
                                         iOvrLevel - 1))
                            : poSrcOvrBand->GetMaskBand();
                }

                if (l_nBands == 1 ||
                    poDstDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
                {
                    if (poDstDS->m_poMaskDS)
                    {
                        dfNextCurPixels +=
                            static_cast<double>(poSrcOvrBand->GetXSize()) *
                            poSrcOvrBand->GetYSize();
                    }
                    void *pScaledData = GDALCreateScaledProgress(
                        dfCurPixels / dfTotalPixels,
                        dfNextCurPixels / dfTotalPixels, pfnProgress,
                        pProgressData);

                    eErr =
                        CopyImageryAndMask(poDstDS, poSrcOvrDS, poSrcMaskBand,
                                           GDALScaledProgress, pScaledData);

                    dfCurPixels = dfNextCurPixels;
                    GDALDestroyScaledProgress(pScaledData);
                }
                else
                {
                    void *pScaledData = GDALCreateScaledProgress(
                        dfCurPixels / dfTotalPixels,
                        dfNextCurPixels / dfTotalPixels, pfnProgress,
                        pProgressData);

                    eErr = GDALDatasetCopyWholeRaster(
                        GDALDataset::ToHandle(poSrcOvrDS),
                        GDALDataset::ToHandle(poDstDS),
                        papszCopyWholeRasterOptions, GDALScaledProgress,
                        pScaledData);

                    dfCurPixels = dfNextCurPixels;
                    GDALDestroyScaledProgress(pScaledData);

                    poDstDS->FlushCache(false);

                    // Copy mask of the overview.
                    if (eErr == CE_None &&
                        (poMaskOvrDS ||
                         poSrcOvrBand->GetMaskFlags() == GMF_PER_DATASET) &&
                        poDstDS->m_poMaskDS != nullptr)
                    {
                        dfNextCurPixels +=
                            static_cast<double>(poSrcOvrBand->GetXSize()) *
                            poSrcOvrBand->GetYSize();
                        pScaledData = GDALCreateScaledProgress(
                            dfCurPixels / dfTotalPixels,
                            dfNextCurPixels / dfTotalPixels, pfnProgress,
                            pProgressData);
                        eErr = GDALRasterBandCopyWholeRaster(
                            poSrcMaskBand,
                            poDstDS->m_poMaskDS->GetRasterBand(1),
                            papszCopyWholeRasterOptions, GDALScaledProgress,
                            pScaledData);
                        dfCurPixels = dfNextCurPixels;
                        GDALDestroyScaledProgress(pScaledData);
                        poDstDS->m_poMaskDS->FlushCache(false);
                    }
                }

                if (poSrcOvrDS != poOvrDS.get())
                    delete poSrcOvrDS;
                poSrcOvrDS = nullptr;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Copy actual imagery.                                            */
    /* -------------------------------------------------------------------- */
    double dfNextCurPixels =
        dfCurPixels + static_cast<double>(nXSize) * nYSize * l_nBands;
    void *pScaledData = GDALCreateScaledProgress(
        dfCurPixels / dfTotalPixels, dfNextCurPixels / dfTotalPixels,
        pfnProgress, pProgressData);

#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
    bool bTryCopy = true;
#endif

#ifdef HAVE_LIBJPEG
    if (bCopyFromJPEG)
    {
        eErr = GTIFF_CopyFromJPEG(poDS, poSrcDS, pfnProgress, pProgressData,
                                  bTryCopy);

        // In case of failure in the decompression step, try normal copy.
        if (bTryCopy)
            eErr = CE_None;
    }
#endif

#ifdef JPEG_DIRECT_COPY
    if (bDirectCopyFromJPEG)
    {
        eErr = GTIFF_DirectCopyFromJPEG(poDS, poSrcDS, pfnProgress,
                                        pProgressData, bTryCopy);

        // In case of failure in the reading step, try normal copy.
        if (bTryCopy)
            eErr = CE_None;
    }
#endif

    bool bWriteMask = true;
    if (
#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
        bTryCopy &&
#endif
        (poDS->m_bTreatAsSplit || poDS->m_bTreatAsSplitBitmap))
    {
        // For split bands, we use TIFFWriteScanline() interface.
        CPLAssert(poDS->m_nBitsPerSample == 8 || poDS->m_nBitsPerSample == 1);

        if (poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG && poDS->nBands > 1)
        {
            GByte *pabyScanline = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(TIFFScanlineSize(l_hTIFF)));
            if (pabyScanline == nullptr)
                eErr = CE_Failure;
            for (int j = 0; j < nYSize && eErr == CE_None; ++j)
            {
                eErr = poSrcDS->RasterIO(GF_Read, 0, j, nXSize, 1, pabyScanline,
                                         nXSize, 1, GDT_Byte, l_nBands, nullptr,
                                         poDS->nBands, 0, 1, nullptr);
                if (eErr == CE_None &&
                    TIFFWriteScanline(l_hTIFF, pabyScanline, j, 0) == -1)
                {
                    ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
                                "TIFFWriteScanline() failed.");
                    eErr = CE_Failure;
                }
                if (!GDALScaledProgress((j + 1) * 1.0 / nYSize, nullptr,
                                        pScaledData))
                    eErr = CE_Failure;
            }
            CPLFree(pabyScanline);
        }
        else
        {
            GByte *pabyScanline =
                static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));
            if (pabyScanline == nullptr)
                eErr = CE_Failure;
            else
                eErr = CE_None;
            for (int iBand = 1; iBand <= l_nBands && eErr == CE_None; ++iBand)
            {
                for (int j = 0; j < nYSize && eErr == CE_None; ++j)
                {
                    eErr = poSrcDS->GetRasterBand(iBand)->RasterIO(
                        GF_Read, 0, j, nXSize, 1, pabyScanline, nXSize, 1,
                        GDT_Byte, 0, 0, nullptr);
                    if (poDS->m_bTreatAsSplitBitmap)
                    {
                        for (int i = 0; i < nXSize; ++i)
                        {
                            const GByte byVal = pabyScanline[i];
                            if ((i & 0x7) == 0)
                                pabyScanline[i >> 3] = 0;
                            if (byVal)
                                pabyScanline[i >> 3] |= 0x80 >> (i & 0x7);
                        }
                    }
                    if (eErr == CE_None &&
                        TIFFWriteScanline(l_hTIFF, pabyScanline, j,
                                          static_cast<uint16_t>(iBand - 1)) ==
                            -1)
                    {
                        ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
                                    "TIFFWriteScanline() failed.");
                        eErr = CE_Failure;
                    }
                    if (!GDALScaledProgress((j + 1 + (iBand - 1) * nYSize) *
                                                1.0 / (l_nBands * nYSize),
                                            nullptr, pScaledData))
                        eErr = CE_Failure;
                }
            }
            CPLFree(pabyScanline);
        }

        // Necessary to be able to read the file without re-opening.
        TIFFSizeProc pfnSizeProc = TIFFGetSizeProc(l_hTIFF);

        TIFFFlushData(l_hTIFF);

        toff_t nNewDirOffset = pfnSizeProc(TIFFClientdata(l_hTIFF));
        if ((nNewDirOffset % 2) == 1)
            ++nNewDirOffset;

        TIFFFlush(l_hTIFF);

        if (poDS->m_nDirOffset != TIFFCurrentDirOffset(l_hTIFF))
        {
            poDS->m_nDirOffset = nNewDirOffset;
            CPLDebug("GTiff", "directory moved during flush.");
        }
    }
    else if (
#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
        bTryCopy &&
#endif
        eErr == CE_None)
    {
        const char *papszCopyWholeRasterOptions[3] = {nullptr, nullptr,
                                                      nullptr};
        int iNextOption = 0;
        papszCopyWholeRasterOptions[iNextOption++] = "SKIP_HOLES=YES";
        if (l_nCompression != COMPRESSION_NONE)
        {
            papszCopyWholeRasterOptions[iNextOption++] = "COMPRESSED=YES";
        }
        // For streaming with separate, we really want that bands are written
        // after each other, even if the source is pixel interleaved.
        else if (bStreaming && poDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
        {
            papszCopyWholeRasterOptions[iNextOption++] = "INTERLEAVE=BAND";
        }

        if (bCopySrcOverviews &&
            (l_nBands == 1 || poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG))
        {
            poDS->m_bBlockOrderRowMajor = true;
            poDS->m_bLeaderSizeAsUInt4 = true;
            poDS->m_bTrailerRepeatedLast4BytesRepeated = true;
            if (poDS->m_poMaskDS)
            {
                poDS->m_poMaskDS->m_bBlockOrderRowMajor = true;
                poDS->m_poMaskDS->m_bLeaderSizeAsUInt4 = true;
                poDS->m_poMaskDS->m_bTrailerRepeatedLast4BytesRepeated = true;
            }

            if (poDS->m_poMaskDS)
            {
                GDALDestroyScaledProgress(pScaledData);
                pScaledData =
                    GDALCreateScaledProgress(dfCurPixels / dfTotalPixels, 1.0,
                                             pfnProgress, pProgressData);
            }

            eErr = CopyImageryAndMask(poDS, poSrcDS,
                                      poSrcDS->GetRasterBand(1)->GetMaskBand(),
                                      GDALScaledProgress, pScaledData);
            if (poDS->m_poMaskDS)
            {
                bWriteMask = false;
            }
        }
        else
        {
            eErr = GDALDatasetCopyWholeRaster(
                /* (GDALDatasetH) */ poSrcDS,
                /* (GDALDatasetH) */ poDS, papszCopyWholeRasterOptions,
                GDALScaledProgress, pScaledData);
        }
    }

    GDALDestroyScaledProgress(pScaledData);

    if (eErr == CE_None && !bStreaming && bWriteMask)
    {
        pScaledData = GDALCreateScaledProgress(dfNextCurPixels / dfTotalPixels,
                                               1.0, pfnProgress, pProgressData);
        if (poDS->m_poMaskDS)
        {
            const char *l_papszOptions[2] = {"COMPRESSED=YES", nullptr};
            eErr = GDALRasterBandCopyWholeRaster(
                poSrcDS->GetRasterBand(1)->GetMaskBand(),
                poDS->GetRasterBand(1)->GetMaskBand(),
                const_cast<char **>(l_papszOptions), GDALScaledProgress,
                pScaledData);
        }
        else
        {
            eErr =
                GDALDriver::DefaultCopyMasks(poSrcDS, poDS, bStrict, nullptr,
                                             GDALScaledProgress, pScaledData);
        }
        GDALDestroyScaledProgress(pScaledData);
    }

    poDS->m_bWriteCOGLayout = false;

    if (eErr == CE_Failure)
    {
        delete poDS;
        poDS = nullptr;

        if (CPLTestBool(CPLGetConfigOption("GTIFF_DELETE_ON_ERROR", "YES")))
        {
            if (!bStreaming)
            {
                // Should really delete more carefully.
                VSIUnlink(pszFilename);
            }
        }
    }

    return poDS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr GTiffDataset::SetSpatialRef(const OGRSpatialReference *poSRS)

{
    if (m_bStreamingOut && m_bCrystalized)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Cannot modify projection at that point in "
                    "a streamed output file");
        return CE_Failure;
    }

    LoadGeoreferencingAndPamIfNeeded();
    LookForProjection();

    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        if ((m_eProfile == GTiffProfile::BASELINE) &&
            (GetPamFlags() & GPF_DISABLED) == 0)
        {
            eErr = GDALPamDataset::SetSpatialRef(poSRS);
        }
        else
        {
            if (GDALPamDataset::GetSpatialRef() != nullptr)
            {
                // Cancel any existing SRS from PAM file.
                GDALPamDataset::SetSpatialRef(nullptr);
            }
            m_bGeoTIFFInfoChanged = true;
        }
    }
    else
    {
        CPLDebug("GTIFF", "SetSpatialRef() goes to PAM instead of TIFF tags");
        eErr = GDALPamDataset::SetSpatialRef(poSRS);
    }

    if (eErr == CE_None)
    {
        if (poSRS == nullptr || poSRS->IsEmpty())
        {
            if (!m_oSRS.IsEmpty())
            {
                m_bForceUnsetProjection = true;
            }
            m_oSRS.Clear();
        }
        else
        {
            m_oSRS = *poSRS;
            m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }
    }

    return eErr;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::SetGeoTransform(double *padfTransform)

{
    if (m_bStreamingOut && m_bCrystalized)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Cannot modify geotransform at that point in a "
                    "streamed output file");
        return CE_Failure;
    }

    LoadGeoreferencingAndPamIfNeeded();

    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        if (!m_aoGCPs.empty())
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "GCPs previously set are going to be cleared "
                        "due to the setting of a geotransform.");
            m_bForceUnsetGTOrGCPs = true;
            m_aoGCPs.clear();
        }
        else if (padfTransform[0] == 0.0 && padfTransform[1] == 0.0 &&
                 padfTransform[2] == 0.0 && padfTransform[3] == 0.0 &&
                 padfTransform[4] == 0.0 && padfTransform[5] == 0.0)
        {
            if (m_bGeoTransformValid)
            {
                m_bForceUnsetGTOrGCPs = true;
                m_bGeoTIFFInfoChanged = true;
            }
            m_bGeoTransformValid = false;
            memcpy(m_adfGeoTransform, padfTransform, sizeof(double) * 6);
            return CE_None;
        }

        if ((m_eProfile == GTiffProfile::BASELINE) &&
            !CPLFetchBool(m_papszCreationOptions, "TFW", false) &&
            !CPLFetchBool(m_papszCreationOptions, "WORLDFILE", false) &&
            (GetPamFlags() & GPF_DISABLED) == 0)
        {
            eErr = GDALPamDataset::SetGeoTransform(padfTransform);
        }
        else
        {
            // Cancel any existing geotransform from PAM file.
            GDALPamDataset::DeleteGeoTransform();
            m_bGeoTIFFInfoChanged = true;
        }
    }
    else
    {
        CPLDebug("GTIFF", "SetGeoTransform() goes to PAM instead of TIFF tags");
        eErr = GDALPamDataset::SetGeoTransform(padfTransform);
    }

    if (eErr == CE_None)
    {
        memcpy(m_adfGeoTransform, padfTransform, sizeof(double) * 6);
        m_bGeoTransformValid = true;
    }

    return eErr;
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr GTiffDataset::SetGCPs(int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                             const OGRSpatialReference *poGCPSRS)
{
    CPLErr eErr = CE_None;
    LoadGeoreferencingAndPamIfNeeded();
    LookForProjection();

    if (eAccess == GA_Update)
    {
        if (!m_aoGCPs.empty() && nGCPCountIn == 0)
        {
            m_bForceUnsetGTOrGCPs = true;
        }
        else if (nGCPCountIn > 0 && m_bGeoTransformValid)
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "A geotransform previously set is going to be cleared "
                        "due to the setting of GCPs.");
            m_adfGeoTransform[0] = 0.0;
            m_adfGeoTransform[1] = 1.0;
            m_adfGeoTransform[2] = 0.0;
            m_adfGeoTransform[3] = 0.0;
            m_adfGeoTransform[4] = 0.0;
            m_adfGeoTransform[5] = 1.0;
            m_bGeoTransformValid = false;
            m_bForceUnsetGTOrGCPs = true;
        }
        if ((m_eProfile == GTiffProfile::BASELINE) &&
            (GetPamFlags() & GPF_DISABLED) == 0)
        {
            eErr = GDALPamDataset::SetGCPs(nGCPCountIn, pasGCPListIn, poGCPSRS);
        }
        else
        {
            if (nGCPCountIn > knMAX_GCP_COUNT)
            {
                if (GDALPamDataset::GetGCPCount() == 0 && !m_aoGCPs.empty())
                {
                    m_bForceUnsetGTOrGCPs = true;
                }
                ReportError(CE_Warning, CPLE_AppDefined,
                            "Trying to write %d GCPs, whereas the maximum "
                            "supported in GeoTIFF tag is %d. "
                            "Falling back to writing them to PAM",
                            nGCPCountIn, knMAX_GCP_COUNT);
                eErr = GDALPamDataset::SetGCPs(nGCPCountIn, pasGCPListIn,
                                               poGCPSRS);
            }
            else if (GDALPamDataset::GetGCPCount() > 0)
            {
                // Cancel any existing GCPs from PAM file.
                GDALPamDataset::SetGCPs(
                    0, nullptr,
                    static_cast<const OGRSpatialReference *>(nullptr));
            }
            m_bGeoTIFFInfoChanged = true;
        }
    }
    else
    {
        CPLDebug("GTIFF", "SetGCPs() goes to PAM instead of TIFF tags");
        eErr = GDALPamDataset::SetGCPs(nGCPCountIn, pasGCPListIn, poGCPSRS);
    }

    if (eErr == CE_None)
    {
        if (poGCPSRS == nullptr || poGCPSRS->IsEmpty())
        {
            if (!m_oSRS.IsEmpty())
            {
                m_bForceUnsetProjection = true;
            }
            m_oSRS.Clear();
        }
        else
        {
            m_oSRS = *poGCPSRS;
            m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }

        m_aoGCPs = gdal::GCP::fromC(pasGCPListIn, nGCPCountIn);
    }

    return eErr;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/
CPLErr GTiffDataset::SetMetadata(char **papszMD, const char *pszDomain)

{
    LoadGeoreferencingAndPamIfNeeded();

    if (m_bStreamingOut && m_bCrystalized)
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify metadata at that point in a streamed output file");
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        if (pszDomain != nullptr && EQUAL(pszDomain, MD_DOMAIN_RPC))
        {
            // So that a subsequent GetMetadata() wouldn't override our new
            // values
            LoadMetadata();
            m_bForceUnsetRPC = (CSLCount(papszMD) == 0);
        }

        if ((papszMD != nullptr) && (pszDomain != nullptr) &&
            EQUAL(pszDomain, "COLOR_PROFILE"))
        {
            m_bColorProfileMetadataChanged = true;
        }
        else if (pszDomain == nullptr || !EQUAL(pszDomain, "_temporary_"))
        {
            m_bMetadataChanged = true;
            // Cancel any existing metadata from PAM file.
            if (GDALPamDataset::GetMetadata(pszDomain) != nullptr)
                GDALPamDataset::SetMetadata(nullptr, pszDomain);
        }

        if ((pszDomain == nullptr || EQUAL(pszDomain, "")) &&
            CSLFetchNameValue(papszMD, GDALMD_AREA_OR_POINT) != nullptr)
        {
            const char *pszPrevValue = GetMetadataItem(GDALMD_AREA_OR_POINT);
            const char *pszNewValue =
                CSLFetchNameValue(papszMD, GDALMD_AREA_OR_POINT);
            if (pszPrevValue == nullptr || pszNewValue == nullptr ||
                !EQUAL(pszPrevValue, pszNewValue))
            {
                LookForProjection();
                m_bGeoTIFFInfoChanged = true;
            }
        }

        if (pszDomain != nullptr && EQUAL(pszDomain, "xml:XMP"))
        {
            if (papszMD != nullptr && *papszMD != nullptr)
            {
                int nTagSize = static_cast<int>(strlen(*papszMD));
                TIFFSetField(m_hTIFF, TIFFTAG_XMLPACKET, nTagSize, *papszMD);
            }
            else
            {
                TIFFUnsetField(m_hTIFF, TIFFTAG_XMLPACKET);
            }
        }
    }
    else
    {
        CPLDebug(
            "GTIFF",
            "GTiffDataset::SetMetadata() goes to PAM instead of TIFF tags");
        eErr = GDALPamDataset::SetMetadata(papszMD, pszDomain);
    }

    if (eErr == CE_None)
    {
        eErr = m_oGTiffMDMD.SetMetadata(papszMD, pszDomain);
    }
    return eErr;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GTiffDataset::SetMetadataItem(const char *pszName, const char *pszValue,
                                     const char *pszDomain)

{
    LoadGeoreferencingAndPamIfNeeded();

    if (m_bStreamingOut && m_bCrystalized)
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify metadata at that point in a streamed output file");
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        if ((pszDomain != nullptr) && EQUAL(pszDomain, "COLOR_PROFILE"))
        {
            m_bColorProfileMetadataChanged = true;
        }
        else if (pszDomain == nullptr || !EQUAL(pszDomain, "_temporary_"))
        {
            m_bMetadataChanged = true;
            // Cancel any existing metadata from PAM file.
            if (GDALPamDataset::GetMetadataItem(pszName, pszDomain) != nullptr)
                GDALPamDataset::SetMetadataItem(pszName, nullptr, pszDomain);
        }

        if ((pszDomain == nullptr || EQUAL(pszDomain, "")) &&
            pszName != nullptr && EQUAL(pszName, GDALMD_AREA_OR_POINT))
        {
            LookForProjection();
            m_bGeoTIFFInfoChanged = true;
        }
    }
    else
    {
        CPLDebug(
            "GTIFF",
            "GTiffDataset::SetMetadataItem() goes to PAM instead of TIFF tags");
        eErr = GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
    }

    if (eErr == CE_None)
    {
        eErr = m_oGTiffMDMD.SetMetadataItem(pszName, pszValue, pszDomain);
    }

    return eErr;
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr GTiffDataset::CreateMaskBand(int nFlagsIn)
{
    ScanDirectories();

    if (m_poMaskDS != nullptr)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "This TIFF dataset has already an internal mask band");
        return CE_Failure;
    }
    else if (MustCreateInternalMask())
    {
        if (nFlagsIn != GMF_PER_DATASET)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "The only flag value supported for internal mask is "
                        "GMF_PER_DATASET");
            return CE_Failure;
        }

        int l_nCompression = COMPRESSION_PACKBITS;
        if (strstr(GDALGetMetadataItem(GDALGetDriverByName("GTiff"),
                                       GDAL_DMD_CREATIONOPTIONLIST, nullptr),
                   "<Value>DEFLATE</Value>") != nullptr)
            l_nCompression = COMPRESSION_ADOBE_DEFLATE;

        /* --------------------------------------------------------------------
         */
        /*      If we don't have read access, then create the mask externally.
         */
        /* --------------------------------------------------------------------
         */
        if (GetAccess() != GA_Update)
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "File open for read-only accessing, "
                        "creating mask externally.");

            return GDALPamDataset::CreateMaskBand(nFlagsIn);
        }

        if (m_bLayoutIFDSBeforeData && !m_bKnownIncompatibleEdition &&
            !m_bWriteKnownIncompatibleEdition)
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "Adding a mask invalidates the "
                        "LAYOUT=IFDS_BEFORE_DATA property");
            m_bKnownIncompatibleEdition = true;
            m_bWriteKnownIncompatibleEdition = true;
        }

        bool bIsOverview = false;
        uint32_t nSubType = 0;
        if (TIFFGetField(m_hTIFF, TIFFTAG_SUBFILETYPE, &nSubType))
        {
            bIsOverview = (nSubType & FILETYPE_REDUCEDIMAGE) != 0;

            if ((nSubType & FILETYPE_MASK) != 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot create a mask on a TIFF mask IFD !");
                return CE_Failure;
            }
        }

        const int bIsTiled = TIFFIsTiled(m_hTIFF);

        FlushDirectory();

        const toff_t nOffset = GTIFFWriteDirectory(
            m_hTIFF,
            bIsOverview ? FILETYPE_REDUCEDIMAGE | FILETYPE_MASK : FILETYPE_MASK,
            nRasterXSize, nRasterYSize, 1, PLANARCONFIG_CONTIG, 1,
            m_nBlockXSize, m_nBlockYSize, bIsTiled, l_nCompression,
            PHOTOMETRIC_MASK, PREDICTOR_NONE, SAMPLEFORMAT_UINT, nullptr,
            nullptr, nullptr, 0, nullptr, "", nullptr, nullptr, nullptr,
            nullptr, m_bWriteCOGLayout);

        ReloadDirectory();

        if (nOffset == 0)
            return CE_Failure;

        m_poMaskDS = new GTiffDataset();
        m_poMaskDS->m_poBaseDS = this;
        m_poMaskDS->m_poImageryDS = this;
        m_poMaskDS->ShareLockWithParentDataset(this);
        m_poMaskDS->m_bPromoteTo8Bits = CPLTestBool(
            CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
        if (m_poMaskDS->OpenOffset(VSI_TIFFOpenChild(m_hTIFF), nOffset,
                                   GA_Update) != CE_None)
        {
            delete m_poMaskDS;
            m_poMaskDS = nullptr;
            return CE_Failure;
        }

        return CE_None;
    }

    return GDALPamDataset::CreateMaskBand(nFlagsIn);
}

/************************************************************************/
/*                        MustCreateInternalMask()                      */
/************************************************************************/

bool GTiffDataset::MustCreateInternalMask()
{
    return CPLTestBool(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", "YES"));
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr GTiffRasterBand::CreateMaskBand(int nFlagsIn)
{
    m_poGDS->ScanDirectories();

    if (m_poGDS->m_poMaskDS != nullptr)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "This TIFF dataset has already an internal mask band");
        return CE_Failure;
    }

    const char *pszGDAL_TIFF_INTERNAL_MASK =
        CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", nullptr);
    if ((pszGDAL_TIFF_INTERNAL_MASK &&
         CPLTestBool(pszGDAL_TIFF_INTERNAL_MASK)) ||
        nFlagsIn == GMF_PER_DATASET)
    {
        return m_poGDS->CreateMaskBand(nFlagsIn);
    }

    return GDALPamRasterBand::CreateMaskBand(nFlagsIn);
}
