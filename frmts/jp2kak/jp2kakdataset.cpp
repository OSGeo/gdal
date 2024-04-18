/******************************************************************************
 *
 * Project:  JPEG-2000
 * Purpose:  Implementation of the ISO/IEC 15444-1 standard based on Kakadu.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "jp2kakdataset.h"
#include "jp2kakdrivercore.h"

#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdaljp2abstractdataset.h"
#include "gdaljp2metadata.h"

#include "jp2kak_headers.h"

#include "subfile_source.h"
#include "vsil_target.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#if KDU_MAJOR_VERSION > 7 || (KDU_MAJOR_VERSION == 7 && KDU_MINOR_VERSION >= 5)
using namespace kdu_core;
using namespace kdu_supp;
#endif

// #define KAKADU_JPX 1

static bool kakadu_initialized = false;

/* -------------------------------------------------------------------- */
/*      The number of tiles at a time we will push through the          */
/*      encoder per flush when writing jpeg2000 streams.                */
/* -------------------------------------------------------------------- */
constexpr int TILE_CHUNK_SIZE = 1024;

/************************************************************************/
/* ==================================================================== */
/*                            JP2KAKRasterBand                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           JP2KAKRasterBand()                         */
/************************************************************************/

JP2KAKRasterBand::JP2KAKRasterBand(int nBandIn, kdu_codestream oCodeStreamIn,
                                   kdu_client *jpip_clientIn,
                                   jp2_channels oJP2Channels,
                                   JP2KAKDataset *poBaseDSIn)
    : poBaseDS(poBaseDSIn), jpip_client(jpip_clientIn),
      oCodeStream(oCodeStreamIn), eInterp(GCI_Undefined)
{
    nBand = nBandIn;  // From GDALRasterBand.

    if (oCodeStream.get_bit_depth(nBand - 1) > 8 &&
        oCodeStream.get_bit_depth(nBand - 1) <= 16 &&
        oCodeStream.get_signed(nBand - 1))
        eDataType = GDT_Int16;
    else if (oCodeStream.get_bit_depth(nBand - 1) > 8 &&
             oCodeStream.get_bit_depth(nBand - 1) <= 16 &&
             !oCodeStream.get_signed(nBand - 1))
        eDataType = GDT_UInt16;
    else if (oCodeStream.get_bit_depth(nBand - 1) > 16 &&
             oCodeStream.get_signed(nBand - 1))
        eDataType = GDT_Int32;
    else if (oCodeStream.get_bit_depth(nBand - 1) > 16 &&
             !oCodeStream.get_signed(nBand - 1))
        eDataType = GDT_UInt32;
    else
        eDataType = GDT_Byte;

    oCodeStream.apply_input_restrictions(0, 0, poBaseDSIn->m_nDiscardLevels, 0,
                                         nullptr);
    oCodeStream.get_dims(0, band_dims);

    nRasterXSize = band_dims.size.x;
    nRasterYSize = band_dims.size.y;

    // Capture some useful metadata.
    if (oCodeStream.get_bit_depth(nBand - 1) % 8 != 0 &&
        !poBaseDSIn->bPromoteTo8Bit)
    {
        SetMetadataItem(
            "NBITS",
            CPLString().Printf("%d", oCodeStream.get_bit_depth(nBand - 1)),
            "IMAGE_STRUCTURE");
    }
    SetMetadataItem("COMPRESSION", "JP2000", "IMAGE_STRUCTURE");

    // Use tile dimension as block size, unless it is too big
    kdu_dims valid_tiles;
    kdu_dims tile_dims;
    oCodeStream.get_valid_tiles(valid_tiles);
    oCodeStream.get_tile_dims(valid_tiles.pos, -1, tile_dims);
    // Configuration option only for testing purposes
    if (CPLTestBool(CPLGetConfigOption("USE_TILE_AS_BLOCK", "NO")))
    {
        nBlockXSize = std::min(tile_dims.size.x, nRasterXSize);
        nBlockYSize = std::min(tile_dims.size.y, nRasterYSize);
    }
    else
    {
        nBlockXSize = std::min(std::min(tile_dims.size.x, 2048), nRasterXSize);
        nBlockYSize = std::min(std::min(tile_dims.size.y, 2048), nRasterYSize);
    }
    CPLDebug("JP2KAK",
             "JP2KAKRasterBand::JP2KAKRasterBand() : "
             "Tile dimension : %d X %d\n",
             nBlockXSize, nBlockYSize);

    // Figure out the color interpretation for this band.
    eInterp = GCI_Undefined;

    if (oJP2Channels.exists())
    {
        int nRedIndex = -1;
        int nGreenIndex = -1;
        int nBlueIndex = -1;
        int nLutIndex = 0;
        int nCSI = 0;

#if KDU_MAJOR_VERSION > 7 || (KDU_MAJOR_VERSION == 7 && KDU_MINOR_VERSION >= 8)
        int nFMT = 0;
        if (oJP2Channels.get_num_colours() == 3)
        {
            oJP2Channels.get_colour_mapping(0, nRedIndex, nLutIndex, nCSI,
                                            nFMT);
            oJP2Channels.get_colour_mapping(1, nGreenIndex, nLutIndex, nCSI,
                                            nFMT);
            oJP2Channels.get_colour_mapping(2, nBlueIndex, nLutIndex, nCSI,
                                            nFMT);
        }
        else
        {
            oJP2Channels.get_colour_mapping(0, nRedIndex, nLutIndex, nCSI,
                                            nFMT);
            if (nBand == 1)
                eInterp = GCI_GrayIndex;
        }
#else
        if (oJP2Channels.get_num_colours() == 3)
        {
            oJP2Channels.get_colour_mapping(0, nRedIndex, nLutIndex, nCSI);
            oJP2Channels.get_colour_mapping(1, nGreenIndex, nLutIndex, nCSI);
            oJP2Channels.get_colour_mapping(2, nBlueIndex, nLutIndex, nCSI);
        }
        else
        {
            oJP2Channels.get_colour_mapping(0, nRedIndex, nLutIndex, nCSI);
            if (nBand == 1)
                eInterp = GCI_GrayIndex;
        }
#endif

        if (eInterp != GCI_Undefined)
            /* nothing to do */;
        // If we have LUT info, it is a palette image.
        else if (nLutIndex != -1)
            eInterp = GCI_PaletteIndex;
        // Establish color band this is.
        else if (nRedIndex == nBand - 1)
            eInterp = GCI_RedBand;
        else if (nGreenIndex == nBand - 1)
            eInterp = GCI_GreenBand;
        else if (nBlueIndex == nBand - 1)
            eInterp = GCI_BlueBand;
        else
            eInterp = GCI_Undefined;

        // Could this band be an alpha band?
        if (eInterp == GCI_Undefined)
        {
            for (int color_idx = 0; color_idx < oJP2Channels.get_num_colours();
                 color_idx++)
            {
                int opacity_idx = 0;
                int lut_idx = 0;

                // get_opacity_mapping sets that last 3 args by non-const refs.
#if KDU_MAJOR_VERSION > 7 || (KDU_MAJOR_VERSION == 7 && KDU_MINOR_VERSION >= 8)
                if (oJP2Channels.get_opacity_mapping(color_idx, opacity_idx,
                                                     lut_idx, nCSI, nFMT))
#else
                if (oJP2Channels.get_opacity_mapping(color_idx, opacity_idx,
                                                     lut_idx, nCSI))
#endif
                {
                    if (opacity_idx == nBand - 1)
                        eInterp = GCI_AlphaBand;
                }
#if KDU_MAJOR_VERSION > 7 || (KDU_MAJOR_VERSION == 7 && KDU_MINOR_VERSION >= 8)
                if (oJP2Channels.get_premult_mapping(color_idx, opacity_idx,
                                                     lut_idx, nCSI, nFMT))
#else
                if (oJP2Channels.get_premult_mapping(color_idx, opacity_idx,
                                                     lut_idx, nCSI))
#endif
                {
                    if (opacity_idx == nBand - 1)
                        eInterp = GCI_AlphaBand;
                }
            }
        }
    }
    else if (nBand == 1)
    {
        eInterp = GCI_RedBand;
    }
    else if (nBand == 2)
    {
        eInterp = GCI_GreenBand;
    }
    else if (nBand == 3)
    {
        eInterp = GCI_BlueBand;
    }
    else
    {
        eInterp = GCI_GrayIndex;
    }
}

/************************************************************************/
/*                         ~JP2KAKRasterBand()                          */
/************************************************************************/

JP2KAKRasterBand::~JP2KAKRasterBand() = default;

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int JP2KAKRasterBand::GetOverviewCount()

{
    if (!poBaseDS->AreOverviewsEnabled())
        return 0;

    const int nExtOvrCount = GDALPamRasterBand::GetOverviewCount();
    if (nExtOvrCount > 0)
        return nExtOvrCount;

    return static_cast<int>(poBaseDS->m_apoOverviews.size());
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *JP2KAKRasterBand::GetOverview(int iOverviewIndex)

{
    if (GDALPamRasterBand::GetOverviewCount() > 0)
        return GDALPamRasterBand::GetOverview(iOverviewIndex);

    if (iOverviewIndex < 0 ||
        iOverviewIndex >= static_cast<int>(poBaseDS->m_apoOverviews.size()))
        return nullptr;

    return poBaseDS->m_apoOverviews[iOverviewIndex]->GetRasterBand(nBand);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JP2KAKRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                    void *pImage)
{
    const int nWordSize = GDALGetDataTypeSizeBytes(eDataType);

    CPLDebug("JP2KAK", "IReadBlock(%d,%d) on band %d.", nBlockXOff, nBlockYOff,
             nBand);

    // Compute the normal window, and buffer size.
    const int nWXOff = nBlockXOff * nBlockXSize;
    const int nWYOff = nBlockYOff * nBlockYSize;
    int nWXSize = nBlockXSize;
    int nWYSize = nBlockYSize;

    int nXSize = nBlockXSize;
    int nYSize = nBlockYSize;

    // Adjust if we have a partial block on the right or bottom of
    // the image.  Unfortunately despite some care I can't seem to
    // always get partial tiles to come from the desired overview
    // level depending on how various things round - hopefully not
    // a big deal.
    if (nWXOff + nWXSize > poBaseDS->GetRasterXSize())
    {
        nWXSize = poBaseDS->GetRasterXSize() - nWXOff;
        nXSize = nRasterXSize - nBlockXSize * nBlockXOff;
    }

    if (nWYOff + nWYSize > poBaseDS->GetRasterYSize())
    {
        nWYSize = poBaseDS->GetRasterYSize() - nWYOff;
        nYSize = nRasterYSize - nBlockYSize * nBlockYOff;
    }

    if (nXSize != nBlockXSize || nYSize != nBlockYSize)
        memset(pImage, 0,
               static_cast<size_t>(nBlockXSize) * nBlockYSize * nWordSize);

    // By default we invoke just for the requested band, directly
    // into the target buffer.
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    if (!poBaseDS->bUseYCC)
    {
        return poBaseDS->DirectRasterIO(GF_Read, nWXOff, nWYOff, nWXSize,
                                        nWYSize, pImage, nXSize, nYSize,
                                        eDataType, 1, &nBand, nWordSize,
                                        nWordSize * nBlockXSize, 0, &sExtraArg);
    }

    // But for YCC or possible other effectively pixel interleaved
    // products, we read all bands into a single buffer, fetch out
    // what we want, and push the rest into the block cache.
    std::vector<int> anBands;

    for (int iBand = 0; iBand < poBaseDS->GetRasterCount(); iBand++)
    {
        GDALRasterBand *poBand = poBaseDS->GetRasterBand(iBand + 1);
        if (poBand->GetRasterDataType() != eDataType)
            continue;
        anBands.push_back(iBand + 1);
    }

    GByte *pabyWrkBuffer = static_cast<GByte *>(
        VSIMalloc3(nWordSize * anBands.size(), nBlockXSize, nBlockYSize));
    if (pabyWrkBuffer == nullptr)
        return CE_Failure;

    const CPLErr eErr = poBaseDS->DirectRasterIO(
        GF_Read, nWXOff, nWYOff, nWXSize, nWYSize, pabyWrkBuffer, nXSize,
        nYSize, eDataType, static_cast<int>(anBands.size()), &anBands[0],
        nWordSize, nWordSize * nBlockXSize,
        static_cast<GSpacing>(nWordSize) * nBlockXSize * nBlockYSize,
        &sExtraArg);

    if (eErr == CE_None)
    {
        int nBandStart = 0;
        const int nTotalBands = static_cast<int>(anBands.size());
        for (int iBand = 0; iBand < nTotalBands; iBand++)
        {
            if (anBands[iBand] == nBand)
            {
                // Application requested band.
                memcpy(pImage, pabyWrkBuffer + nBandStart,
                       static_cast<size_t>(nWordSize) * nBlockXSize *
                           nBlockYSize);
            }
            else
            {
                // All others are pushed into cache.
                GDALRasterBand *poBaseBand =
                    poBaseDS->GetRasterBand(anBands[iBand]);
                JP2KAKRasterBand *poBand =
                    cpl::down_cast<JP2KAKRasterBand *>(poBaseBand);

                GDALRasterBlock *poBlock = nullptr;

                if (poBand != nullptr)
                    poBlock =
                        poBand->GetLockedBlockRef(nBlockXOff, nBlockYOff, TRUE);

                if (poBlock)
                {
                    memcpy(poBlock->GetDataRef(), pabyWrkBuffer + nBandStart,
                           static_cast<size_t>(nWordSize) * nBlockXSize *
                               nBlockYSize);
                    poBlock->DropLock();
                }
            }

            nBandStart +=
                static_cast<size_t>(nWordSize) * nBlockXSize * nBlockYSize;
        }
    }

    VSIFree(pabyWrkBuffer);
    return eErr;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JP2KAKRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                   int nXSize, int nYSize, void *pData,
                                   int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType, GSpacing nPixelSpace,
                                   GSpacing nLineSpace,
                                   GDALRasterIOExtraArg *psExtraArg)

{
    // Try to pass the request to the most appropriate overview dataset.
    if (nBufXSize < nXSize && nBufYSize < nYSize)
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    // We need various criteria to skip out to block based methods.
    if (poBaseDS->TestUseBlockIO(nXSize, nYSize, nBufXSize, nBufYSize, eBufType,
                                 1, &nBand))
        return GDALPamRasterBand::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg);

    return poBaseDS->DirectRasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, 1, &nBand, nPixelSpace, nLineSpace, 0, psExtraArg);
}

/************************************************************************/
/*                            ApplyPalette()                            */
/************************************************************************/

namespace
{

inline short GetColorValue(const float *pafLUT, int nPos)
{
    const short nVal = static_cast<short>(pafLUT[nPos] * 256.0f + 128.0f);
    const short nMin = 0;
    const short nMax = 255;
    return std::max(nMin, std::min(nMax, nVal));
}

}  // namespace

void JP2KAKRasterBand::ApplyPalette(jp2_palette oJP2Palette)

{
    // Do we have a reasonable LUT configuration?  RGB or RGBA?
    if (!oJP2Palette.exists())
        return;

    if (oJP2Palette.get_num_luts() == 0 || oJP2Palette.get_num_entries() == 0)
        return;

    if (oJP2Palette.get_num_luts() < 3)
    {
        CPLDebug("JP2KAK",
                 "JP2KAKRasterBand::ApplyPalette()\n"
                 "Odd get_num_luts() value (%d)",
                 oJP2Palette.get_num_luts());
        return;
    }

    // Fetch lut entries.  They are normalized in the -0.5 to 0.5 range. */
    const int nCount = oJP2Palette.get_num_entries();

    float *const pafLUT =
        static_cast<float *>(CPLCalloc(sizeof(float) * 4, nCount));

    const int nRed = 0;
    const int nGreen = 1;
    const int nBlue = 2;
    const int nAlpha = 3;
    oJP2Palette.get_lut(nRed, pafLUT + 0);
    oJP2Palette.get_lut(nGreen, pafLUT + nCount);
    oJP2Palette.get_lut(nBlue, pafLUT + nCount * 2);

    if (oJP2Palette.get_num_luts() == 4)
    {
        oJP2Palette.get_lut(nAlpha, pafLUT + nCount * 3);
    }
    else
    {
        for (int iColor = 0; iColor < nCount; iColor++)
        {
            pafLUT[nCount * 3 + iColor] = 0.5;
        }
    }

    // Apply to GDAL colortable.
    const int nRedOffset = nCount * nRed;
    const int nGreenOffset = nCount * nGreen;
    const int nBlueOffset = nCount * nBlue;
    const int nAlphaOffset = nCount * nAlpha;

    for (int iColor = 0; iColor < nCount; iColor++)
    {
        const GDALColorEntry sEntry = {
            GetColorValue(pafLUT, iColor + nRedOffset),
            GetColorValue(pafLUT, iColor + nGreenOffset),
            GetColorValue(pafLUT, iColor + nBlueOffset),
            GetColorValue(pafLUT, iColor + nAlphaOffset)};

        oCT.SetColorEntry(iColor, &sEntry);
    }

    CPLFree(pafLUT);

    eInterp = GCI_PaletteIndex;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JP2KAKRasterBand::GetColorInterpretation()
{
    return eInterp;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *JP2KAKRasterBand::GetColorTable()

{
    if (oCT.GetColorEntryCount() > 0)
        return &oCT;

    return nullptr;
}

/************************************************************************/
/* ==================================================================== */
/*                           JP2KAKDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           JP2KAKDataset()                           */
/************************************************************************/

JP2KAKDataset::JP2KAKDataset() = default;

/************************************************************************/
/*                           JP2KAKDataset()                           */
/************************************************************************/

// Constructor for overview dataset
JP2KAKDataset::JP2KAKDataset(JP2KAKDataset *poMainDS, int nDiscardLevels,
                             const kdu_dims &dimsIn)
    : m_osFilename(poMainDS->m_osFilename), oCodeStream(poMainDS->oCodeStream),
      poInput(poMainDS->poInput), poRawInput(poMainDS->poRawInput),
      family(poMainDS->family), jpip_client(poMainDS->jpip_client),
      dims(dimsIn), nResCount(poMainDS->nResCount),
      bPreferNPReads(poMainDS->bPreferNPReads),
      poThreadEnv(poMainDS->poThreadEnv), m_nDiscardLevels(nDiscardLevels),
      bCached(poMainDS->bCached), bResilient(poMainDS->bResilient),
      bFussy(poMainDS->bFussy), bUseYCC(poMainDS->bUseYCC),
      bPromoteTo8Bit(poMainDS->bPromoteTo8Bit)
{
    nRasterXSize = dims.size.x;
    nRasterYSize = dims.size.y;
}

/************************************************************************/
/*                            ~JP2KAKDataset()                         */
/************************************************************************/

JP2KAKDataset::~JP2KAKDataset()

{
    FlushCache(true);

    if (m_nDiscardLevels == 0)
    {
        if (poInput != nullptr)
        {
            oCodeStream.destroy();
            poInput->close();
            delete poInput;
            if (family)
            {
                family->close();
                delete family;
            }
            if (poRawInput != nullptr)
                delete poRawInput;
#ifdef USE_JPIP
            if (jpip_client != NULL)
            {
                jpip_client->close();
                delete jpip_client;
            }
#endif
        }

        if (poThreadEnv != nullptr)
        {
            poThreadEnv->terminate(nullptr, true);
            poThreadEnv->destroy();
            delete poThreadEnv;
        }
    }
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr JP2KAKDataset::IBuildOverviews(const char *pszResampling, int nOverviews,
                                      const int *panOverviewList,
                                      int nListBands, const int *panBandList,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData,
                                      CSLConstList papszOptions)

{
    // In order for building external overviews to work properly, we
    // discard any concept of internal overviews when the user
    // first requests to build external overviews.
    m_apoOverviews.clear();

    return GDALPamDataset::IBuildOverviews(
        pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
        pfnProgress, pProgressData, papszOptions);
}

/************************************************************************/
/*                          KakaduInitialize()                          */
/************************************************************************/

void JP2KAKDataset::KakaduInitialize()

{
    // Initialize Kakadu warning/error reporting subsystem.
    if (kakadu_initialized)
        return;

    kakadu_initialized = true;

    kdu_cpl_error_message oErrHandler(CE_Failure);
    kdu_cpl_error_message oWarningHandler(CE_Warning);
    CPL_IGNORE_RET_VAL(oErrHandler);
    CPL_IGNORE_RET_VAL(oWarningHandler);

    kdu_customize_warnings(new kdu_cpl_error_message(CE_Warning));
    kdu_customize_errors(new kdu_cpl_error_message(CE_Failure));
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JP2KAKDataset::Open(GDALOpenInfo *poOpenInfo)

{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if (!JP2KAKDatasetIdentify(poOpenInfo))
        return nullptr;
#endif

    subfile_source *poRawInput = nullptr;
    bool bIsJPIP = false;
    bool bIsSubfile = false;
    const GByte *pabyHeader = nullptr;

    const bool bResilient =
        CPLTestBool(CPLGetConfigOption("JP2KAK_RESILIENT", "NO"));

    // Doesn't seem to bring any real performance gain on Linux.
    const bool bBuffered = CPLTestBool(CPLGetConfigOption("JP2KAK_BUFFERED",
#ifdef _WIN32
                                                          "YES"
#else
                                                          "NO"
#endif
                                                          ));

    KakaduInitialize();

    // Handle setting up datasource for JPIP.
    const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
    std::vector<GByte> abySubfileHeader(16);  // leave in this scope
    if (poOpenInfo->nHeaderBytes < 16)
    {
        if ((STARTS_WITH_CI(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "jpip://")) &&
            EQUAL(pszExtension, "jp2"))
        {
            bIsJPIP = true;
        }
        else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "J2K_SUBFILE:"))
        {
            try
            {
                poRawInput = new subfile_source;
                poRawInput->open(poOpenInfo->pszFilename, bResilient,
                                 bBuffered);
                poRawInput->seek(0);

                poRawInput->read(&abySubfileHeader[0], 16);
                poRawInput->seek(0);
            }
            catch (...)
            {
                return nullptr;
            }

            pabyHeader = abySubfileHeader.data();

            bIsSubfile = true;
        }
        else
        {
            return nullptr;
        }
    }
    else
    {
        pabyHeader = poOpenInfo->pabyHeader;
    }

    // If we think this should be access via vsil, then open it using
    // subfile_source.  We do this if it does not seem to open normally
    // or if we want to operate in resilient (sequential) mode.
    VSIStatBuf sStat;
    if (poRawInput == nullptr && !bIsJPIP &&
        (bBuffered || bResilient ||
         VSIStat(poOpenInfo->pszFilename, &sStat) != 0))
    {
        try
        {
            poRawInput = new subfile_source;
            poRawInput->open(poOpenInfo->pszFilename, bResilient, bBuffered);
            poRawInput->seek(0);
        }
        catch (...)
        {
            delete poRawInput;
            return nullptr;
        }
    }

    // If the header is a JP2 header, mark this as a JP2 dataset.
    if (pabyHeader && memcmp(pabyHeader, jp2_header, sizeof(jp2_header)) == 0)
        pszExtension = "jp2";

    // Try to open the file in a manner depending on the extension.
    kdu_compressed_source *poInput = nullptr;
    kdu_client *jpip_client = nullptr;
    jp2_palette oJP2Palette;
    jp2_channels oJP2Channels;

    jp2_family_src *family = nullptr;

    try
    {
        if (bIsJPIP)
        {
#ifdef USE_JPIP
            char *pszWrk =
                CPLStrdup(strstr(poOpenInfo->pszFilename, "://") + 3);
            char *pszRequest = strstr(pszWrk, "/");

            if (pszRequest == NULL)
            {
                CPLDebug("JP2KAK", "Failed to parse JPIP server and request.");
                CPLFree(pszWrk);
                return NULL;
            }

            *(pszRequest++) = '\0';

            CPLDebug("JP2KAK", "server=%s, request=%s", pszWrk, pszRequest);

            CPLSleep(15.0);
            jpip_client = new kdu_client;
            jpip_client->connect(pszWrk, NULL, pszRequest, "http-tcp", "");

            CPLDebug("JP2KAK", "After connect()");

            bool bin0_complete = false;

            while (jpip_client->get_databin_length(KDU_META_DATABIN, 0, 0,
                                                   &bin0_complete) <= 0 ||
                   !bin0_complete)
                CPLSleep(0.25);

            family = new jp2_family_src;
            family->open(jpip_client);

            // TODO(schwehr): Check for memory leaks.
            jp2_source *jp2_src = new jp2_source;
            jp2_src->open(family);
            jp2_src->read_header();

            while (!jpip_client->is_idle())
                CPLSleep(0.25);

            if (jpip_client->is_alive())
            {
                CPLDebug("JP2KAK", "connect() seems to be complete.");
            }
            else
            {
                CPLDebug("JP2KAK", "connect() seems to have failed.");
                return NULL;
            }

            oJP2Channels = jp2_src->access_channels();

            poInput = jp2_src;
#else
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "JPIP Protocol not supported by GDAL with "
                     "Kakadu 3.4 or on Unix.");
            return nullptr;
#endif
        }
        else if (pszExtension != nullptr &&
                 (EQUAL(pszExtension, "jp2") || EQUAL(pszExtension, "jpx")))
        {
            family = new jp2_family_src;
            if (poRawInput != nullptr)
                family->open(poRawInput);
            else
                family->open(poOpenInfo->pszFilename, true);
            jp2_source *jp2_src = new jp2_source;
            poInput = jp2_src;
            if (!jp2_src->open(family) || !jp2_src->read_header())
            {
                CPLDebug("JP2KAK", "Cannot read JP2 boxes");
                delete jp2_src;
                delete family;
                delete poRawInput;
                return nullptr;
            }

            oJP2Palette = jp2_src->access_palette();
            oJP2Channels = jp2_src->access_channels();

            jp2_colour oColors = jp2_src->access_colour();
            if (oColors.get_space() != JP2_sRGB_SPACE &&
                oColors.get_space() != JP2_sLUM_SPACE)
            {
                CPLDebug("JP2KAK",
                         "Unusual ColorSpace=%d, not further interpreted.",
                         static_cast<int>(oColors.get_space()));
            }
        }
        else if (poRawInput == nullptr)
        {
            poInput = new kdu_simple_file_source(poOpenInfo->pszFilename);
        }
        else
        {
            poInput = poRawInput;
            poRawInput = nullptr;
        }
    }
    catch (...)
    {
        CPLDebug("JP2KAK", "Trapped Kakadu exception.");
        delete family;
        delete poRawInput;
        delete poInput;
        return nullptr;
    }

    // Create a corresponding GDALDataset.
    JP2KAKDataset *poDS = nullptr;

    try
    {
        poDS = new JP2KAKDataset();

        poDS->poInput = poInput;
        poDS->poRawInput = poRawInput;
        poDS->family = family;
        poDS->oCodeStream.create(poInput);
        poDS->oCodeStream.set_persistent();

        poDS->bCached = bBuffered;
        poDS->bResilient = bResilient;
        poDS->bFussy = CPLTestBool(CPLGetConfigOption("JP2KAK_FUSSY", "NO"));

        if (poDS->bFussy)
            poDS->oCodeStream.set_fussy();
        if (poDS->bResilient)
            poDS->oCodeStream.set_resilient();

        poDS->jpip_client = jpip_client;

        // Get overall image size.
        poDS->oCodeStream.get_dims(0, poDS->dims);

        poDS->nRasterXSize = poDS->dims.size.x;
        poDS->nRasterYSize = poDS->dims.size.y;

        // Ensure that all the components have the same dimensions.  If
        // not, just process the first dimension.
        auto l_nBands = poDS->oCodeStream.get_num_components();

        if (l_nBands > 1)
        {
            for (int iDim = 1; iDim < l_nBands; iDim++)
            {
                kdu_dims dim_this_comp;

                poDS->oCodeStream.get_dims(iDim, dim_this_comp);

                if (dim_this_comp != poDS->dims)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Some components have mismatched dimensions, "
                             "ignoring all but first.");
                    l_nBands = 1;
                    break;
                }
            }
        }

        // Setup the thread environment.

        int nNumThreads = atoi(CPLGetConfigOption("JP2KAK_THREADS", "-1"));
        if (nNumThreads == -1)
            nNumThreads = kdu_get_num_processors() - 1;
        if (nNumThreads > 1024)
            nNumThreads = 1024;

        if (nNumThreads > 0)
        {
            poDS->poThreadEnv = new kdu_thread_env;
            poDS->poThreadEnv->create();

            for (int iThread = 0; iThread < nNumThreads; iThread++)
            {
                if (!poDS->poThreadEnv->add_thread())
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "JP2KAK_THREADS: Unable to create thread.");
                    break;
                }
            }
            CPLDebug("JP2KAK", "Using %d threads.", nNumThreads);
        }
        else
        {
            CPLDebug("JP2KAK", "Operating in singlethreaded mode.");
        }

        // Is this a file with poor internal navigation that will end
        // up using a great deal of memory if we use keep persistent
        // parsed information around?  (#3295)
        siz_params *siz = poDS->oCodeStream.access_siz();
        kdu_params *cod = siz->access_cluster(COD_params);
        bool use_precincts = false;

        cod->get(Cuse_precincts, 0, 0, use_precincts);

        const char *pszPersist = CPLGetConfigOption("JP2KAK_PERSIST", "AUTO");
        if (EQUAL(pszPersist, "AUTO"))
        {
            if (!use_precincts && !bIsJPIP &&
                (poDS->nRasterXSize * static_cast<double>(poDS->nRasterYSize)) >
                    100000000.0)
                poDS->bPreferNPReads = true;
        }
        else
        {
            poDS->bPreferNPReads = !CPLTestBool(pszPersist);
        }

        CPLDebug("JP2KAK", "Cuse_precincts=%d, PreferNonPersistentReads=%d",
                 use_precincts ? 1 : 0, poDS->bPreferNPReads ? 1 : 0);

        //  Deduce some other info about the dataset.
        int order = 0;

        cod->get(Corder, 0, 0, order);

        const char *pszOrder = nullptr;
        switch (order)
        {
            case Corder_LRCP:
                pszOrder = "LRCP";
                break;
            case Corder_RPCL:
                pszOrder = "RPCL";
                break;
            case Corder_PCRL:
                pszOrder = "PCRL";
                break;
            case Corder_CPRL:
                pszOrder = "CPRL";
                break;
            default:
                break;
        }
        if (pszOrder)
        {
            poDS->SetMetadataItem("Corder", pszOrder, "IMAGE_STRUCTURE");
        }

        poDS->bUseYCC = false;
        cod->get(Cycc, 0, 0, poDS->bUseYCC);
        if (poDS->bUseYCC)
            CPLDebug("JP2KAK", "ycc=true");

        // Find out how many resolutions levels are available.
        kdu_dims tile_indices;
        poDS->oCodeStream.get_valid_tiles(tile_indices);

        kdu_tile tile = poDS->oCodeStream.open_tile(tile_indices.pos);
        poDS->nResCount = tile.access_component(0).get_num_resolutions();
        tile.close();

        CPLDebug("JP2KAK", "nResCount=%d", poDS->nResCount);

        // Should we promote alpha channel to 8 bits?
        poDS->bPromoteTo8Bit = l_nBands == 4 &&
                               poDS->oCodeStream.get_bit_depth(0) == 8 &&
                               poDS->oCodeStream.get_bit_depth(1) == 8 &&
                               poDS->oCodeStream.get_bit_depth(2) == 8 &&
                               poDS->oCodeStream.get_bit_depth(3) == 1 &&
                               CPLFetchBool(poOpenInfo->papszOpenOptions,
                                            "1BIT_ALPHA_PROMOTION", true);
        if (poDS->bPromoteTo8Bit)
            CPLDebug("JP2KAK",
                     "Fourth (alpha) band is promoted from 1 bit to 8 bit");

        // Create band information objects.
        bool bHasExternalOverviews = false;
        for (int iBand = 1; iBand <= l_nBands; iBand++)
        {
            JP2KAKRasterBand *poBand = new JP2KAKRasterBand(
                iBand, poDS->oCodeStream, jpip_client, oJP2Channels, poDS);

            if (iBand == 1)
                bHasExternalOverviews =
                    poBand->GDALPamRasterBand::GetOverviewCount() > 0;

            if (iBand == 1 && oJP2Palette.exists())
                poBand->ApplyPalette(oJP2Palette);

            poDS->SetBand(iBand, poBand);
        }

        poDS->m_osFilename = poOpenInfo->pszFilename;

        // Create overviews
        if (!bHasExternalOverviews)
        {
            int nXSize = poDS->nRasterXSize;
            int nYSize = poDS->nRasterYSize;

            for (int nDiscard = 1; nDiscard < poDS->nResCount; nDiscard++)
            {
                nXSize = (nXSize + 1) / 2;
                nYSize = (nYSize + 1) / 2;

                if ((nXSize + nYSize) < 128 || nXSize < 4 || nYSize < 4)
                    continue;  // Skip super reduced resolution layers.

                poDS->oCodeStream.apply_input_restrictions(0, 0, nDiscard, 0,
                                                           nullptr);
                kdu_dims dims;  // Struct with default constructor.
                poDS->oCodeStream.get_dims(0, dims);

                if ((dims.size.x == nXSize || dims.size.x == nXSize - 1) &&
                    (dims.size.y == nYSize || dims.size.y == nYSize - 1))
                {
                    auto poOvrDS =
                        std::make_unique<JP2KAKDataset>(poDS, nDiscard, dims);

                    for (int iBand = 1; iBand <= l_nBands; iBand++)
                    {
                        JP2KAKRasterBand *poBand = new JP2KAKRasterBand(
                            iBand, poDS->oCodeStream, jpip_client, oJP2Channels,
                            poOvrDS.get());

                        if (iBand == 1 && oJP2Palette.exists())
                            poBand->ApplyPalette(oJP2Palette);

                        poOvrDS->SetBand(iBand, poBand);
                    }
                    poDS->m_apoOverviews.emplace_back(std::move(poOvrDS));
                }
                else
                {
                    CPLDebug("GDAL",
                             "Discard %dx%d JPEG2000 overview layer,\n"
                             "expected %dx%d.",
                             dims.size.x, dims.size.y, nXSize, nYSize);
                }
            }
        }

        // Look for supporting coordinate system information.
        if (poOpenInfo->nHeaderBytes != 0)
        {
            poDS->LoadJP2Metadata(poOpenInfo);
        }

        // Establish our corresponding physical file.
        CPLString osPhysicalFilename = poOpenInfo->pszFilename;

        if (bIsSubfile ||
            STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsisubfile/"))
        {
            const char *comma = strstr(poOpenInfo->pszFilename, ",");
            if (comma != nullptr)
                osPhysicalFilename = comma + 1;
        }

        /* --------------------------------------------------------------------
         */
        /*      Initialize any PAM information. */
        /* --------------------------------------------------------------------
         */
        poDS->SetDescription(poOpenInfo->pszFilename);
        if (!bIsSubfile)
            poDS->TryLoadXML();
        else
            poDS->nPamFlags |= GPF_NOSAVE;

        // Check for external overviews.
        poDS->oOvManager.Initialize(poDS, osPhysicalFilename);

        // Confirm the requested access is supported.
        if (poOpenInfo->eAccess == GA_Update)
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_NotSupported,
                     "The JP2KAK driver does not support "
                     "update access to existing datasets.");
            return nullptr;
        }

        // Vector layers.
        if (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR)
        {
            poDS->LoadVectorLayers(CPLFetchBool(poOpenInfo->papszOpenOptions,
                                                "OPEN_REMOTE_GML", false));

            // If file opened in vector-only mode and there's no vector,
            // return.
            if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
                poDS->GetLayerCount() == 0)
            {
                delete poDS;
                return nullptr;
            }
        }

        return poDS;
    }
    catch (...)
    {
        CPLDebug("JP2KAK", "JP2KAKDataset::Open() - caught exception.");
        if (poDS != nullptr)
            delete poDS;

        return nullptr;
    }
}

namespace
{
// std::vector<bool> is Unfortunately a specialized implementation such as &v[0]
// doesn't work
// https://codereview.stackexchange.com/questions/241629/stdvectorbool-workaround-in-c
class vector_safe_bool
{
    bool value;

  public:
    // cppcheck-suppress uninitMemberVar
    vector_safe_bool() = default;

    // cppcheck-suppress noExplicitConstructor
    vector_safe_bool(bool b) : value{b}
    {
    }

    bool *operator&() noexcept
    {
        return &value;
    }

    const bool *operator&() const noexcept
    {
        return &value;
    }

    operator const bool &() const noexcept
    {
        return value;
    }

    operator bool &() noexcept
    {
        return value;
    }
};
}  // namespace

/************************************************************************/
/*                           DirectRasterIO()                           */
/************************************************************************/

CPLErr JP2KAKDataset::DirectRasterIO(GDALRWFlag /* eRWFlag */, int nXOff,
                                     int nYOff, int nXSize, int nYSize,
                                     void *pData, int nBufXSize, int nBufYSize,
                                     GDALDataType eBufType, int nBandCount,
                                     int *panBandMap, GSpacing nPixelSpace,
                                     GSpacing nLineSpace, GSpacing nBandSpace,
                                     GDALRasterIOExtraArg *psExtraArg)

{
    if (psExtraArg->eResampleAlg != GRIORA_NearestNeighbour &&
        (nXSize != nBufXSize || nYSize != nBufYSize))
    {
        return RasterIOResampled(GF_Read, nXOff, nYOff, nXSize, nYSize, pData,
                                 nBufXSize, nBufYSize, eBufType, nBandCount,
                                 panBandMap, nPixelSpace, nLineSpace,
                                 nBandSpace, psExtraArg);
    }

    CPLAssert(eBufType == GDT_Byte || eBufType == GDT_Int16 ||
              eBufType == GDT_UInt16 || eBufType == GDT_Int32 ||
              eBufType == GDT_UInt32);

    kdu_codestream *poCodeStream = &oCodeStream;
    const char *pszPersistency = "";

    // Do we want to do this non-persistently?  If so, we need to
    // open the file, and establish a local codestream.
    subfile_source subfile_src;
    jp2_source wrk_jp2_src;
    jp2_family_src wrk_family;
    kdu_codestream oWCodeStream;

    if (bPreferNPReads)
    {
        try
        {
            subfile_src.open(m_osFilename.c_str(), bResilient, bCached);
        }
        catch (...)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "subfile_src.open(%s) failed",
                     m_osFilename.c_str());
            return CE_Failure;
        }

        if (family != nullptr)
        {
            wrk_family.open(&subfile_src);
            wrk_jp2_src.open(&wrk_family);
            wrk_jp2_src.read_header();

            oWCodeStream.create(&wrk_jp2_src, poThreadEnv);
        }
        else
        {
            oWCodeStream.create(&subfile_src, poThreadEnv);
        }

        if (bFussy)
            oWCodeStream.set_fussy();
        if (bResilient)
            oWCodeStream.set_resilient();

        poCodeStream = &oWCodeStream;

        pszPersistency = "(non-persistent)";
    }

    // Prepare component indices list.
    CPLErr eErr = CE_None;

    std::vector<int> component_indices(nBandCount);
    std::vector<int> stripe_heights(nBandCount);
    std::vector<int> sample_offsets(nBandCount);
    std::vector<int> sample_gaps(nBandCount);
    std::vector<int> row_gaps(nBandCount);
    std::vector<int> precisions(nBandCount);
    std::vector<vector_safe_bool> is_signed(nBandCount);

    for (int i = 0; i < nBandCount; i++)
        component_indices[i] = panBandMap[i] - 1;

    // Setup a ROI matching the block requested, and select desired
    // bands (components).
    try
    {
        poCodeStream->apply_input_restrictions(0, 0, m_nDiscardLevels, 0,
                                               nullptr);
        kdu_dims l_dims;
        poCodeStream->get_dims(0, l_dims);
        const int nOvrCanvasXSize = l_dims.pos.x + l_dims.size.x;
        const int nOvrCanvasYSize = l_dims.pos.y + l_dims.size.y;

        l_dims.pos.x = l_dims.pos.x + nXOff;
        l_dims.pos.y = l_dims.pos.y + nYOff;
        l_dims.size.x = nXSize;
        l_dims.size.y = nYSize;

        // Check if rounding helps detecting when data is being requested
        // exactly at the current resolution.
        if (nBufXSize != l_dims.size.x &&
            static_cast<int>(0.5 + nXSize) == nBufXSize)
        {
            l_dims.size.x = nBufXSize;
        }
        if (nBufYSize != l_dims.size.y &&
            static_cast<int>(0.5 + nYSize) == nBufYSize)
        {
            l_dims.size.y = nBufYSize;
        }
        if (l_dims.pos.x + l_dims.size.x > nOvrCanvasXSize)
            l_dims.size.x = nOvrCanvasXSize - l_dims.pos.x;
        if (l_dims.pos.y + l_dims.size.y > nOvrCanvasYSize)
            l_dims.size.y = nOvrCanvasYSize - l_dims.pos.y;

        kdu_dims l_dims_roi;

        poCodeStream->map_region(0, l_dims, l_dims_roi);
        poCodeStream->apply_input_restrictions(
            nBandCount, component_indices.data(), m_nDiscardLevels, 0,
            &l_dims_roi, KDU_WANT_OUTPUT_COMPONENTS);

        // Special case where the data is being requested exactly at
        // this resolution.  Avoid any extra sampling pass.
        const int nBufDTSize = GDALGetDataTypeSizeBytes(eBufType);
        if (nBufXSize == l_dims.size.x && nBufYSize == l_dims.size.y &&
            (nBandCount - 1) * nBandSpace / nBufDTSize < INT_MAX)
        {
            kdu_stripe_decompressor decompressor;
            decompressor.start(*poCodeStream, false, false, poThreadEnv);

            CPLDebug("JP2KAK",
                     "DirectRasterIO() for ovr=%d: %d,%d,%d,%d -> %dx%d "
                     "(no intermediate) %s",
                     m_nDiscardLevels - 1, nXOff, nYOff, nXSize, nYSize,
                     nBufXSize, nBufYSize, pszPersistency);

            for (int i = 0; i < nBandCount; i++)
            {
                stripe_heights[i] = l_dims.size.y;
                precisions[i] = poCodeStream->get_bit_depth(i);
                is_signed[i] = GDALDataTypeIsSigned(eBufType) == TRUE;
                sample_offsets[i] =
                    static_cast<int>(i * nBandSpace / nBufDTSize);
                sample_gaps[i] = static_cast<int>(nPixelSpace / nBufDTSize);
                row_gaps[i] = static_cast<int>(nLineSpace) / nBufDTSize;
            }

            if (eBufType == GDT_Byte)
                decompressor.pull_stripe(static_cast<kdu_byte *>(pData),
                                         &stripe_heights[0], &sample_offsets[0],
                                         &sample_gaps[0], &row_gaps[0],
                                         &precisions[0]);
            else if (nBufDTSize == 2)
                decompressor.pull_stripe(static_cast<kdu_int16 *>(pData),
                                         &stripe_heights[0], &sample_offsets[0],
                                         &sample_gaps[0], &row_gaps[0],
                                         &precisions[0], &is_signed[0]);
            else
                decompressor.pull_stripe(static_cast<kdu_int32 *>(pData),
                                         &stripe_heights[0], &sample_offsets[0],
                                         &sample_gaps[0], &row_gaps[0],
                                         &precisions[0], &is_signed[0]);
            decompressor.finish();
        }
        else
        {
            // More general case - first pull into working buffer.

            const int nDataTypeSize = GDALGetDataTypeSizeBytes(eBufType);
            GByte *pabyIntermediate = static_cast<GByte *>(VSI_MALLOC3_VERBOSE(
                l_dims.size.x, l_dims.size.y, nDataTypeSize * nBandCount));
            if (pabyIntermediate == nullptr)
            {
                return CE_Failure;
            }

            CPLDebug(
                "JP2KAK",
                "DirectRasterIO() for ovr=%d: %d,%d,%d,%d -> %dx%d -> %dx%d %s",
                m_nDiscardLevels - 1, nXOff, nYOff, nXSize, nYSize,
                l_dims.size.x, l_dims.size.y, nBufXSize, nBufYSize,
                pszPersistency);

            kdu_stripe_decompressor decompressor;
            decompressor.start(*poCodeStream, false, false, poThreadEnv);

            for (int i = 0; i < nBandCount; i++)
            {
                stripe_heights[i] = l_dims.size.y;
                precisions[i] = poCodeStream->get_bit_depth(i);

                if (eBufType == GDT_Int16 || eBufType == GDT_UInt16)
                {
                    is_signed[i] = eBufType == GDT_Int16;
                }
            }

            if (eBufType == GDT_Byte)
                decompressor.pull_stripe(
                    reinterpret_cast<kdu_byte *>(pabyIntermediate),
                    &stripe_heights[0], nullptr, nullptr, nullptr,
                    &precisions[0]);
            else if (nBufDTSize == 2)
                decompressor.pull_stripe(
                    reinterpret_cast<kdu_int16 *>(pabyIntermediate),
                    &stripe_heights[0], nullptr, nullptr, nullptr,
                    &precisions[0], &is_signed[0]);
            else
                decompressor.pull_stripe(
                    reinterpret_cast<kdu_int32 *>(pabyIntermediate),
                    &stripe_heights[0], nullptr, nullptr, nullptr,
                    &precisions[0], &is_signed[0]);
            decompressor.finish();

            // Then resample (normally downsample) from the intermediate
            // buffer into the final buffer in the desired output layout.
            const double dfYRatio =
                l_dims.size.y / static_cast<double>(nBufYSize);
            const double dfXRatio =
                l_dims.size.x / static_cast<double>(nBufXSize);

            for (int iY = 0; iY < nBufYSize; iY++)
            {
                const int iSrcY =
                    std::min(static_cast<int>(floor((iY + 0.5) * dfYRatio)),
                             l_dims.size.y - 1);

                for (int iX = 0; iX < nBufXSize; iX++)
                {
                    const int iSrcX =
                        std::min(static_cast<int>(floor((iX + 0.5) * dfXRatio)),
                                 l_dims.size.x - 1);

                    for (int i = 0; i < nBandCount; i++)
                    {
                        // TODO(schwehr): Cleanup this block.
                        if (eBufType == GDT_Byte)
                            ((GByte *)pData)[iX * nPixelSpace +
                                             iY * nLineSpace + i * nBandSpace] =
                                pabyIntermediate[iSrcX * nBandCount +
                                                 static_cast<GPtrDiff_t>(
                                                     iSrcY) *
                                                     l_dims.size.x *
                                                     nBandCount +
                                                 i];
                        else if (eBufType == GDT_Int16 ||
                                 eBufType == GDT_UInt16)
                            ((GUInt16 *)pData)[iX * nPixelSpace / 2 +
                                               iY * nLineSpace / 2 +
                                               i * nBandSpace / 2] =
                                ((GUInt16 *)pabyIntermediate)
                                    [iSrcX * nBandCount +
                                     static_cast<GPtrDiff_t>(iSrcY) *
                                         l_dims.size.x * nBandCount +
                                     i];
                        else if (eBufType == GDT_Int32 ||
                                 eBufType == GDT_UInt32)
                            ((GUInt32 *)pData)[iX * nPixelSpace / 4 +
                                               iY * nLineSpace / 4 +
                                               i * nBandSpace / 4] =
                                ((GUInt32 *)pabyIntermediate)
                                    [iSrcX * nBandCount +
                                     static_cast<GPtrDiff_t>(iSrcY) *
                                         l_dims.size.x * nBandCount +
                                     i];
                    }
                }
            }

            CPLFree(pabyIntermediate);
        }
    }
    catch (...)
    {
        // Catch internal Kakadu errors.
        eErr = CE_Failure;
    }

    // 1-bit alpha promotion.
    if (nBandCount == 4 && bPromoteTo8Bit)
    {
        for (int j = 0; j < nBufYSize; j++)
        {
            for (int i = 0; i < nBufXSize; i++)
            {
                static_cast<GByte *>(
                    pData)[j * nLineSpace + i * nPixelSpace + 3 * nBandSpace] *=
                    255;
            }
        }
    }
    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    if (poCodeStream == &oWCodeStream)
    {
        oWCodeStream.destroy();
        wrk_jp2_src.close();
        wrk_family.close();
        subfile_src.close();
    }

    return eErr;
}

/************************************************************************/
/*                           TestUseBlockIO()                           */
/*                                                                      */
/*      Check whether we should use blocked IO (true) or direct io      */
/*      (FALSE) for a given request configuration and environment.      */
/************************************************************************/

bool JP2KAKDataset::TestUseBlockIO(int nXSize, int nYSize, int nBufXSize,
                                   int nBufYSize, GDALDataType eDataType,
                                   int nBandCount, const int *panBandList)

{

    if (eDataType != GetRasterBand(1)->GetRasterDataType() ||
        (eDataType != GDT_Byte && eDataType != GDT_Int16 &&
         eDataType != GDT_UInt16 && eDataType != GDT_Int32 &&
         eDataType != GDT_UInt32))
        return true;

    // Due to limitations in DirectRasterIO() we can only handle
    // with no duplicates in the band list.
    for (int i = 0; i < nBandCount; i++)
    {
        for (int j = i + 1; j < nBandCount; j++)
            if (panBandList[j] == panBandList[i])
                return true;
    }

    // The rest of the rules are io strategy stuff and configuration checks.
    bool bUseBlockedIO = bForceCachedIO;

    if (nYSize == 1 || nXSize * static_cast<double>(nYSize) < 100.0)
        bUseBlockedIO = true;

    if (nBufYSize == 1 || nBufXSize * static_cast<double>(nBufYSize) < 100.0)
        bUseBlockedIO = true;

    const char *pszOneBigRead =
        CPLGetConfigOption("GDAL_ONE_BIG_READ", nullptr);
    if (pszOneBigRead)
        bUseBlockedIO = !CPLTestBool(pszOneBigRead);

    return bUseBlockedIO;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JP2KAKDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
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
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    // We need various criteria to skip out to block based methods.
    if (TestUseBlockIO(nXSize, nYSize, nBufXSize, nBufYSize, eBufType,
                       nBandCount, panBandMap))
        return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nBandCount, panBandMap, nPixelSpace,
                                         nLineSpace, nBandSpace, psExtraArg);

    return DirectRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                          nBufXSize, nBufYSize, eBufType, nBandCount,
                          panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                          psExtraArg);
}

/************************************************************************/
/*                           JP2KAKWriteBox()                           */
/*                                                                      */
/*      Write out the passed box and delete it.                         */
/************************************************************************/

static void JP2KAKWriteBox(jp2_family_tgt *jp2_family, GDALJP2Box *poBox)

{
    if (poBox == nullptr)
        return;

    jp2_output_box jp2_out;

    GUInt32 nBoxType = 0;
    memcpy(&nBoxType, poBox->GetType(), sizeof(nBoxType));
    CPL_MSBPTR32(&nBoxType);

    int length = static_cast<int>(poBox->GetDataLength());

    // Write to a box on the JP2 file.
    jp2_out.open(jp2_family, nBoxType);
    jp2_out.set_target_size(length);
    jp2_out.write(const_cast<kdu_byte *>(poBox->GetWritableData()), length);
    jp2_out.close();

    delete poBox;
}

/************************************************************************/
/*                     JP2KAKCreateCopy_WriteTile()                     */
/************************************************************************/

static bool JP2KAKCreateCopy_WriteTile(
    GDALDataset *poSrcDS, kdu_tile &oTile, kdu_thread_env *poThreadEnv,
    kdu_roi_image *poROIImage, int nXOff, int nYOff, int nXSize, int nYSize,
    int nBits, GDALDataType eType, kdu_codestream &oCodeStream,
    bool bFlushEnabled, kdu_long *layer_bytes, int layer_count,
    GDALProgressFunc pfnProgress, void *pProgressData, bool bComseg)

{
    kdu_multi_analysis engine;
    const int num_components = oTile.get_num_components();
    oTile.set_components_of_interest(num_components);
    int flags = KDU_MULTI_XFORM_DEFAULT_FLAGS;
    if (CPLTestBool(CPLGetConfigOption("JP2KAK_PRECISE", "NO")))
        flags |= KDU_MULTI_XFORM_PRECISE;
    // if (want_fastest) flags |= KDU_MULTI_XFORM_FAST;
    // if (double_buffering) flags |= KDU_MULTI_XFORM_MT_DWT;

    kdu_thread_queue *env_queue = nullptr;
    if (poThreadEnv)
        env_queue =
            poThreadEnv->add_queue(nullptr, nullptr, "tile compression");

    engine.create(oCodeStream, oTile, poThreadEnv, env_queue, flags,
                  poROIImage);

    std::vector<kdu_line_buf *> apoLines(num_components);
    std::vector<int> anCurLine(num_components);
    std::vector<GDALRasterBand *> apoSrcBand(num_components);
    for (int c = 0; c < num_components; c++)
    {
        apoSrcBand[c] = poSrcDS->GetRasterBand(c + 1);
    }

    // Write whole image by batch of 1024 lines when flushing.
    // This gives the rate computing machine all components to make good
    // estimates.
    int iLinesWritten = 0;

    void *pabyBuffer = CPLMalloc(nXSize * GDALGetDataTypeSizeBytes(eType));

    CPLAssert(!oTile.get_ycc());

    bool bRet = true;
    while (true)
    {
        bool bHasDoneSomething = false;
        for (int c = 0; c < num_components; c++)
        {
            if (anCurLine[c] == nYSize)
                continue;
            if (apoLines[c] == nullptr)
            {
                apoLines[c] = engine.exchange_line(c, nullptr, poThreadEnv);
                if (apoLines[c] == nullptr)
                {
                    continue;  // This component is not yet ready for writing
                }
            }

            bHasDoneSomething = true;
            if (apoSrcBand[c]->RasterIO(GF_Read, nXOff, nYOff + anCurLine[c],
                                        nXSize, 1, pabyBuffer, nXSize, 1, eType,
                                        0, 0, nullptr) == CE_Failure)
            {
                bRet = false;
                break;
            }

            anCurLine[c]++;

            const bool bIsAbsolute = apoLines[c]->is_absolute();
            if (eType == GDT_Byte)
            {
                const kdu_byte *sp = static_cast<const kdu_byte *>(pabyBuffer);
                const kdu_int16 nOffset = 1 << (nBits - 1);
                if (kdu_sample16 *dest16 = apoLines[c]->get_buf16())
                {
                    for (int n = nXSize; n > 0; n--, dest16++, sp++)
                    {
                        if (*sp > (1 << nBits) - 1)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value outside of domain allowed by NBITS "
                                     "value");
                            bRet = false;
                            break;
                        }
                        if (!bIsAbsolute)
                            dest16->ival =
                                (static_cast<kdu_int16>(*sp) - nOffset)
                                << (KDU_FIX_POINT - nBits);
                        else
                            dest16->ival =
                                static_cast<kdu_int16>(*sp) - nOffset;
                    }
                }
                else if (kdu_sample32 *dest32 = apoLines[c]->get_buf32())
                {
                    // We go here in precise mode
                    const float fScale = 1.0f / (1 << nBits);

                    for (int n = nXSize; n > 0; n--, dest32++, sp++)
                    {
                        if (*sp > (1 << nBits) - 1)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value outside of domain allowed by NBITS "
                                     "value");
                            bRet = false;
                            break;
                        }
                        if (!bIsAbsolute)
                            dest32->fval =
                                (static_cast<kdu_int16>(*sp) - nOffset) *
                                fScale;
                        else
                            dest32->ival =
                                static_cast<kdu_int16>(*sp) - nOffset;
                    }
                }
                else
                {
                    CPLAssert(false);
                }
            }

            else if (eType == GDT_Int16)
            {
                const GInt16 *sp = static_cast<const GInt16 *>(pabyBuffer);
                if (kdu_sample16 *dest16 = apoLines[c]->get_buf16())
                {
                    for (int n = nXSize; n > 0; n--, dest16++, sp++)
                    {
                        if (*sp < -(1 << (nBits - 1)) ||
                            *sp > (1 << (nBits - 1)) - 1)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value outside of domain allowed by NBITS "
                                     "value");
                            bRet = false;
                            break;
                        }
                        if (!bIsAbsolute)  // we go here for NBITS <= 10 and
                                           // !bReversible
                            dest16->ival = (*sp) << (KDU_FIX_POINT - nBits);
                        else
                            dest16->ival = *sp;
                    }
                }
                else if (kdu_sample32 *dest32 = apoLines[c]->get_buf32())
                {
                    const float fScale = 1.0f / (1 << nBits);
                    for (int n = nXSize; n > 0; n--, dest32++, sp++)
                    {
                        if (*sp < -(1 << (nBits - 1)) ||
                            *sp > (1 << (nBits - 1)) - 1)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value outside of domain allowed by NBITS "
                                     "value");
                            bRet = false;
                            break;
                        }
                        if (!bIsAbsolute)
                            dest32->fval = (*sp) * fScale;
                        else
                            dest32->ival = *sp;
                    }
                }
                else
                {
                    CPLAssert(false);
                }
            }

            else if (eType == GDT_UInt16)
            {
                const GUInt16 *sp = static_cast<const GUInt16 *>(pabyBuffer);
                const kdu_int32 nOffset = 1 << (nBits - 1);
                if (kdu_sample16 *dest16 = apoLines[c]->get_buf16())
                {
                    // We go here for NBITS < 16
                    for (int n = nXSize; n > 0; n--, dest16++, sp++)
                    {
                        if (*sp > (1 << nBits) - 1)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value outside of domain allowed by NBITS "
                                     "value");
                            bRet = false;
                            break;
                        }
                        if (!bIsAbsolute)  // we go here for NBITS <= 10 and
                                           // !bReversible
                            dest16->ival =
                                static_cast<kdu_int16>((*sp) - nOffset)
                                << (KDU_FIX_POINT - nBits);
                        else
                            dest16->ival =
                                static_cast<kdu_int16>((*sp) - nOffset);
                    }
                }
                else if (kdu_sample32 *dest32 = apoLines[c]->get_buf32())
                {
                    const float fScale = 1.0f / (1 << nBits);
                    for (int n = nXSize; n > 0; n--, dest32++, sp++)
                    {
                        if (*sp > (1 << nBits) - 1)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value outside of domain allowed by NBITS "
                                     "value");
                            bRet = false;
                            break;
                        }
                        if (!bIsAbsolute)
                            dest32->fval = ((*sp) - nOffset) * fScale;
                        else
                            dest32->ival = (*sp) - nOffset;
                    }
                }
                else
                {
                    CPLAssert(false);
                }
            }

            else if (eType == GDT_Int32)
            {
                const GInt32 *sp = static_cast<const GInt32 *>(pabyBuffer);
                if (kdu_sample32 *dest32 = apoLines[c]->get_buf32())
                {
                    const float fScale = static_cast<float>(
                        pow(2.0, -static_cast<double>(nBits)));
                    const int32_t nMin =
                        (nBits == 32) ? std::numeric_limits<int32_t>::min()
                                      : -(1 << (nBits - 1));
                    const int32_t nMax =
                        (nBits == 32) ? std::numeric_limits<int32_t>::max()
                                      : (1 << (nBits - 1)) - 1;
                    for (int n = nXSize; n > 0; n--, dest32++, sp++)
                    {
                        if (*sp < nMin || *sp > nMax)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value outside of domain allowed by NBITS "
                                     "value");
                            bRet = false;
                            break;
                        }
                        if (!bIsAbsolute)
                            dest32->fval = (*sp) * fScale;
                        else
                            dest32->ival = *sp;
                    }
                }
                else
                {
                    CPLAssert(false);
                }
            }

            else if (eType == GDT_UInt32)
            {
                const GUInt32 *sp = static_cast<const GUInt32 *>(pabyBuffer);
                if (kdu_sample32 *dest32 = apoLines[c]->get_buf32())
                {
                    const float fScale = static_cast<float>(
                        pow(2.0, -static_cast<double>(nBits)));
                    const uint32_t nMax =
                        (nBits == 32)
                            ? std::numeric_limits<uint32_t>::max()
                            : static_cast<uint32_t>((1U << nBits) - 1);
                    const kdu_int32 nOffset =
                        static_cast<kdu_int32>(1U << (nBits - 1));
                    for (int n = nXSize; n > 0; n--, dest32++, sp++)
                    {
                        if (*sp > nMax)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value outside of domain allowed by NBITS "
                                     "value");
                            bRet = false;
                            break;
                        }
                        if (!bIsAbsolute)
                            dest32->fval =
                                static_cast<kdu_int32>(*sp - nOffset) * fScale;
                        else
                            dest32->ival =
                                static_cast<kdu_int32>(*sp - nOffset);
                    }
                }
                else
                {
                    CPLAssert(false);
                }
            }

            else if (eType == GDT_Float32)
            {
                const float *sp = static_cast<const float *>(pabyBuffer);
                if (kdu_sample32 *dest32 = apoLines[c]->get_buf32())
                {
                    for (int n = nXSize; n > 0; n--, dest32++, sp++)
                        dest32->fval = *sp;  // Scale it?
                }
                else
                {
                    CPLAssert(false);
                }
            }

            if (bRet == false)
                break;

            apoLines[c] = engine.exchange_line(c, apoLines[c], poThreadEnv);

            iLinesWritten++;

            if (!pfnProgress(iLinesWritten /
                                 static_cast<double>(num_components * nYSize),
                             nullptr, pProgressData))
            {
                bRet = false;
                break;
            }
        }

        if (bFlushEnabled &&
            (!bHasDoneSomething || (anCurLine[0] % TILE_CHUNK_SIZE) == 0))
        {
            if (oCodeStream.ready_for_flush(poThreadEnv))
            {
                CPLDebug("JP2KAK", "Calling oCodeStream.flush() at line %d",
                         anCurLine[0]);

                try
                {
                    oCodeStream.flush(layer_bytes, layer_count, nullptr, true,
                                      bComseg, 0.0, poThreadEnv);
                }
                catch (...)
                {
                    CPLDebug("JP2KAK", "JP2KAKCreateCopy_WriteTile() - caught "
                                       "exception in oCodeStream.flush()");
                    bRet = false;
                    break;
                }
            }
            else
            {
                CPLDebug("JP2KAK", "read_for_flush() is false at line %d.",
                         anCurLine[0]);
            }
        }

        if (!bHasDoneSomething)
            break;
    }

    if (poThreadEnv)
    {
        poThreadEnv->join(env_queue, true);  // Joins with descendants only
    }
    engine.destroy(poThreadEnv);

    CPLFree(pabyBuffer);

    return bRet;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

static GDALDataset *JP2KAKCreateCopy(const char *pszFilename,
                                     GDALDataset *poSrcDS, int bStrict,
                                     char **papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)

{
    if (poSrcDS->GetRasterCount() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Creating zero band files not supported by JP2KAK driver.");
        return nullptr;
    }

    // Initialize Kakadu warning/error reporting subsystem.
    if (!kakadu_initialized)
    {
        kakadu_initialized = true;

        kdu_cpl_error_message oErrHandler(CE_Failure);
        kdu_cpl_error_message oWarningHandler(CE_Warning);
        CPL_IGNORE_RET_VAL(oErrHandler);
        CPL_IGNORE_RET_VAL(oWarningHandler);

        kdu_customize_warnings(new kdu_cpl_error_message(CE_Warning));
        kdu_customize_errors(new kdu_cpl_error_message(CE_Failure));
    }

    // What data type should we use?  We assume all datatypes match
    // the first band.
    GDALRasterBand *poPrototypeBand = poSrcDS->GetRasterBand(1);

    GDALDataType eType = poPrototypeBand->GetRasterDataType();
    if (eType != GDT_Byte && eType != GDT_Int16 && eType != GDT_UInt16 &&
        eType != GDT_Int32 && eType != GDT_UInt32 && eType != GDT_Float32)
    {
        if (bStrict)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JP2KAK (JPEG2000) driver does not support data type %s.",
                     GDALGetDataTypeName(eType));
            return nullptr;
        }

        CPLError(CE_Warning, CPLE_AppDefined,
                 "JP2KAK (JPEG2000) driver does not support data type %s, "
                 "forcing to Float32.",
                 GDALGetDataTypeName(eType));

        eType = GDT_Float32;
    }

    // Do we want to write a pseudo-colored image?
    const int bHaveCT = poPrototypeBand->GetColorTable() != nullptr &&
                        poSrcDS->GetRasterCount() == 1;

    // How many layers?
    int layer_count = 12;

    if (CSLFetchNameValue(papszOptions, "LAYERS") != nullptr)
        layer_count = atoi(CSLFetchNameValue(papszOptions, "LAYERS"));
    else if (CSLFetchNameValue(papszOptions, "Clayers") != nullptr)
        layer_count = atoi(CSLFetchNameValue(papszOptions, "Clayers"));

    // Establish how many bytes of data we want for each layer.
    // We take the quality as a percentage, so if QUALITY of 50 is
    // selected, we will set the base layer to 50% the default size.
    // We let the other layers be computed internally.

    const double dfQuality =
        CSLFetchNameValue(papszOptions, "QUALITY") != nullptr
            ? CPLAtof(CSLFetchNameValue(papszOptions, "QUALITY"))
            : 20.0;

    if (dfQuality < 0.01 || dfQuality > 100.0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "QUALITY=%s is not a legal value in the range 0.01-100.",
                 CSLFetchNameValue(papszOptions, "QUALITY"));
        return nullptr;
    }

    const bool bReversible =
        CSLFetchNameValue(papszOptions, "Creversible") != nullptr
            ? CPLFetchBool(papszOptions, "Creversible", false)
            : (dfQuality >= 99.5);

    std::vector<kdu_long> layer_bytes;

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const double dfPixelsTotal = nXSize * static_cast<double>(nYSize);

    // Override Quality parameter if RATE option is used
    // TODO: highest quality that can
    //       be achieved with a quality factor of QUALITY and
    //       also introducing some visual weighting to the rate-distortion
    //       optimization objective associated with all N quality layers.
    //       That means: combines both QUALITY and RATE
    const char *pszRate = CSLFetchNameValue(papszOptions, "RATE");
    if (pszRate != nullptr)
    {
        // Use RATE option.
        CPLStringList aosRate(CSLTokenizeStringComplex(
            CSLFetchNameValue(papszOptions, "RATE"), ",", FALSE, FALSE));

        int rate_count = CSLCount(aosRate);
        if (rate_count <= 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "RATE argument must be followed by a string identifying "
                     "one or more bit-rates, separated by commas.");
            return nullptr;
        }
        else
        {
            bool bValid = true;
            // compare NUMBER of layers defined by LAYERS or CLayers and the
            // number of layers defined by RATE and is not 1 or 2
            //  1) the number of rates specified here is identical to the
            //  specified number of layers;
            // E.g RATE=1,3 LAYERS=1 => not valid
            // E.g RATE=1,3 LAYERS=2 => valid
            // E.g RATE=1,3 LAYERS=6 => valid
            // The number of layers must be 2 or more and intervening  layers
            // will be assigned roughly logarithmically spaced bit-rates. When
            // only one rate is specified, an internal heuristic determines  a
            // lower bound and logarithmically spaces the layer rates over the
            // rang
            if ((rate_count > layer_count) || layer_count == 0)
            {
                bValid = false;
            }

            // E.g RATE=1,3,4,5 LAYERS=3 => not valid
            if (rate_count > 2 && rate_count != layer_count)
            {
                bValid = false;
            }

            if (!bValid)
            {
                CPLError(
                    CE_Failure, CPLE_IllegalArg,
                    "The relationship between the number of bit-rates "
                    "specified by the \"RATE\" argument and the number of "
                    "quality layers "
                    "explicitly specified via \"Layers\" does not conform to "
                    "the rules "
                    "supplied in the description of the \"RATE\" argument");
                return nullptr;
            }
        }

        layer_bytes.resize(layer_count);

        int i, j = 0;
        double currentBitRate;
        bool bDash = false;

        if (strcmp(aosRate[0], "-") == 0)
        {
            bDash = true;
            j = 1;
            // shift array to the left and force last element to 0 to assign all
            // remaining bits
        }

        const int nb_elements = rate_count - j;
        CPLDebug("JP2KAK", "dfPixelTotal = %g\n", dfPixelsTotal);
        for (i = 0; i < nb_elements; i++, j++)
        {
            currentBitRate = CPLAtof(aosRate[j]);
            if (currentBitRate < 0.0 || currentBitRate > 100.0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "One of the bit-rate is not a legal value in the "
                         "range 0.0-100.");
                return nullptr;
            }

            layer_bytes[i] = static_cast<kdu_long>(
                floor(dfPixelsTotal * currentBitRate * 0.125F));
        }

        // re-arrange order
        // e.g with f(x) = floor(dfPixelsTotal * x * 0.125F)
        // RATE=1,3 CLAYERS=3 => r = f(1),0,f(3)
        // RATE=1,3 CLAYERS=5 => r = f(1),0,0,0,f(3)
        // RATE=1 CLAYERS=5 => r = 0,0,0,0,f(1)
        if (rate_count == 1)
        {
            // swap to end
            layer_bytes[layer_count - 1] = layer_bytes[0];
            layer_bytes[0] = 0;
        }
        else if (rate_count == 2 && layer_count > 2)
        {
            // 2 elements
            layer_bytes[layer_count - 1] = layer_bytes[rate_count - 1];
            layer_bytes[rate_count - 1] = 0;
        }

        if (bDash)
        {
            // force assign all remaining compressed bits
            layer_bytes[layer_count - 1] = 0;
        }

        for (uint32_t k = 0; k < layer_bytes.size(); k++)
        {
            CPLDebug("JP2KAK", "layer_bytes[%d] = %g\n", k,
                     static_cast<double>(layer_bytes[k]));
        }
    }
    else if (!bReversible)
    {
        layer_bytes.resize(layer_count);
        double dfLayerBytes =
            (nXSize * static_cast<double>(nYSize) * dfQuality / 100.0) *
            GDALGetDataTypeSizeBytes(eType) * GDALGetRasterCount(poSrcDS);

        if (dfLayerBytes > 2000000000.0 && sizeof(kdu_long) == 4)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Trimming maximum size of file 2GB from %.1fGB\n"
                     "to avoid overflow of kdu_long layer size.",
                     dfLayerBytes / 1000000000.0);
            dfLayerBytes = 2000000000.0;
        }

        layer_bytes[layer_count - 1] = static_cast<kdu_long>(dfLayerBytes);

        CPLDebug("JP2KAK", "layer_bytes[] = %g\n",
                 static_cast<double>(layer_bytes[layer_count - 1]));
    }

    // Do we want to use more than one tile?
    int nTileXSize = nXSize;
    int nTileYSize = nYSize;

    if (nTileXSize > 25000)
    {
        // Don't generate tiles that are terrible wide by default, as
        // they consume a lot of memory for the compression engine.
        nTileXSize = 20000;
    }

    // TODO(schwehr): Why 253 and not 255?
    if ((nTileYSize / TILE_CHUNK_SIZE) > 253)
    {
        // We don't want to process a tile in more than 255 chunks as there
        // is a limit on the number of tile parts in a tile and we are likely
        // to flush out a tile part for each processing chunk.  If we might
        // go over try trimming our Y tile size such that we will get about
        // 200 tile parts.
        nTileYSize = 200 * TILE_CHUNK_SIZE;
    }

    if (CSLFetchNameValue(papszOptions, "BLOCKXSIZE") != nullptr)
        nTileXSize = atoi(CSLFetchNameValue(papszOptions, "BLOCKXSIZE"));

    if (CSLFetchNameValue(papszOptions, "BLOCKYSIZE") != nullptr)
        nTileYSize = atoi(CSLFetchNameValue(papszOptions, "BLOCKYSIZE"));
    if (nTileXSize <= 0 || nTileYSize <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Wrong value for BLOCKXSIZE/BLOCKYSIZE");
        return nullptr;
    }

    // Avoid splitting into too many tiles - apparently limiting to 64K tiles.
    // There is a hard limit on the number of tiles allowed in JPEG2000.
    const double dfXbyY = static_cast<double>(nXSize) * nYSize / (1024 * 64);
    while (dfXbyY >= static_cast<double>(nTileXSize) * nTileYSize)
    {
        nTileXSize *= 2;
        nTileYSize *= 2;
    }

    if (nTileXSize > nXSize)
        nTileXSize = nXSize;
    if (nTileYSize > nYSize)
        nTileYSize = nYSize;

    CPLDebug("JP2KAK", "Final JPEG2000 Tile Size is %dP x %dL.", nTileXSize,
             nTileYSize);

    // Do we want a comment segment emitted?
    const bool bComseg = CPLFetchBool(papszOptions, "COMSEG", true);

    // Work out the precision.
    int nBits = 0;

    const char *pszNBITS = CSLFetchNameValue(papszOptions, "NBITS");
    if (pszNBITS == nullptr)
        pszNBITS = poPrototypeBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
    if (pszNBITS != nullptr)
    {
        nBits = atoi(pszNBITS);

        const auto nDataTypeSizeBytes = GDALGetDataTypeSizeBytes(eType);
        if ((nDataTypeSizeBytes == 1 && !(nBits <= 8)) ||
            (nDataTypeSizeBytes == 2 && !(nBits >= 9 && nBits <= 16)) ||
            (nDataTypeSizeBytes == 4 && !(nBits >= 17 && nBits <= 32)))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Wrong value for NBITS compared to data type");
            return nullptr;
        }
    }
    else
    {
        nBits = GDALGetDataTypeSize(eType);
        // Otherwise: we get a "Insufficient implementation precision available
        // for true reversible compression!" error or the data is not actually
        // reversible (on autotest/gcore/data/int32.tif / uint32.tif)
        if (bReversible && nBits == 32)
            nBits = 27;
    }

    // Establish the general image parameters.
    siz_params oSizeParams;

    oSizeParams.set(Scomponents, 0, 0, poSrcDS->GetRasterCount());
    oSizeParams.set(Sdims, 0, 0, nYSize);
    oSizeParams.set(Sdims, 0, 1, nXSize);
    oSizeParams.set(Sprecision, 0, 0, nBits);
    if (GDALDataTypeIsSigned(eType))
        oSizeParams.set(Ssigned, 0, 0, true);
    else
        oSizeParams.set(Ssigned, 0, 0, false);

    if (nTileXSize != nXSize || nTileYSize != nYSize)
    {
        oSizeParams.set(Stiles, 0, 0, nTileYSize);
        oSizeParams.set(Stiles, 0, 1, nTileXSize);

        CPLDebug("JP2KAK", "Stiles=%d,%d", nTileYSize, nTileXSize);
    }

    kdu_params *poSizeRef = &oSizeParams;
    poSizeRef->finalize();

    // Open output file, and setup codestream.
    if (!pfnProgress(0.0, nullptr, pProgressData))
        return nullptr;

    jp2_family_tgt family;
#ifdef KAKADU_JPX
    jpx_family_tgt jpx_family;
    jpx_target jpx_out;
    const bool bIsJPX = !EQUAL(CPLGetExtension(pszFilename), "jpf") &&
                        !EQUAL(CPLGetExtension(pszFilename), "jpc") &&
                        !EQUAL(CPLGetExtension(pszFilename), "j2k") &&
                        !(pszCodec != NULL && EQUAL(pszCodec, "J2K"));
#endif

    kdu_compressed_target *poOutputFile = nullptr;
    jp2_target jp2_out;
    const char *pszCodec = CSLFetchNameValueDef(papszOptions, "CODEC", nullptr);
    const bool bIsJP2 = (!EQUAL(CPLGetExtension(pszFilename), "jpc") &&
                         !EQUAL(CPLGetExtension(pszFilename), "j2k") &&
#ifdef KAKADU_JPX
                         !bIsJPX &&
#endif
                         !(pszCodec != nullptr && EQUAL(pszCodec, "J2K"))) ||
                        (pszCodec != nullptr && EQUAL(pszCodec, "JP2"));
    kdu_codestream oCodeStream;

    vsil_target oVSILTarget;

    try
    {
        oVSILTarget.open(pszFilename, "w");

        if (bIsJP2)
        {
            // family.open( pszFilename );
            family.open(&oVSILTarget);

            jp2_out.open(&family);
            poOutputFile = &jp2_out;
        }
#ifdef KAKADU_JPX
        else if (bIsJPX)
        {
            jpx_family.open(pszFilename);

            jpx_out.open(&jpx_family);
            jpx_out.add_codestream();
        }
#endif
        else
        {
            poOutputFile = &oVSILTarget;
        }

        oCodeStream.create(&oSizeParams, poOutputFile);
    }
    catch (...)
    {
        return nullptr;
    }

    // Do we have a high res region of interest?
    kdu_roi_image *poROIImage = nullptr;
#ifdef KDU_HAS_ROI_RECT
    const char *pszROI = CSLFetchNameValue(papszOptions, "ROI");
    if (pszROI)
    {
        CPLStringList aosTokens(
            CSLTokenizeStringComplex(pszROI, ",", FALSE, FALSE));

        if (aosTokens.size() != 4)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Skipping corrupt ROI def = \n%s", pszROI);
        }
        else
        {
            kdu_dims region;
            region.pos.x = atoi(aosTokens[0]);
            region.pos.y = atoi(aosTokens[1]);
            region.size.x = atoi(aosTokens[2]);
            region.size.y = atoi(aosTokens[3]);

            poROIImage = new kdu_roi_rect(oCodeStream, region);
        }
    }
#endif

    // Set some particular parameters.
    oCodeStream.access_siz()->parse_string(
        CPLString().Printf("Clayers=%d", layer_count).c_str());
    oCodeStream.access_siz()->parse_string("Cycc=no");
    if (eType == GDT_Int16 || eType == GDT_UInt16)
        oCodeStream.access_siz()->parse_string(
            "Qstep=0.0000152588");  // 1. / (1 << 16)
    // else if( eType == GDT_Int32 || eType == GDT_UInt32 )
    //     oCodeStream.access_siz()->parse_string("Qstep=0.00000000023883064");
    //     // 1. / (1 << 32)

    if (bReversible)
        oCodeStream.access_siz()->parse_string("Creversible=yes");
    else
        oCodeStream.access_siz()->parse_string("Creversible=no");

    // Set some user-overridable parameters.
    const char *const apszParams[] = {
        "Corder",
        "PCRL",
        "Cprecincts",
        "{512,512},{256,512},{128,512},{64,512},{32,512},{16,512},{8,512},{4,"
        "512},{2,512}",
        "ORGgen_plt",
        "yes",
        "ORGgen_tlm",
        nullptr,
        "ORGtparts",
        nullptr,
        "Qguard",
        nullptr,
        "Cmodes",
        nullptr,
        "Clevels",
        nullptr,
        "Cblk",
        nullptr,
        "Rshift",
        nullptr,
        "Rlevels",
        nullptr,
        "Rweight",
        nullptr,
        "Sprofile",
        nullptr,
        nullptr,
        nullptr};

    for (int iParam = 0; apszParams[iParam] != nullptr; iParam += 2)
    {
        const char *pszValue =
            CSLFetchNameValue(papszOptions, apszParams[iParam]);

        if (pszValue == nullptr)
            pszValue = apszParams[iParam + 1];

        if (pszValue != nullptr)
        {
            CPLString osOpt;

            osOpt.Printf("%s=%s", apszParams[iParam], pszValue);
            try
            {
                oCodeStream.access_siz()->parse_string(osOpt);
            }
            catch (...)
            {
                if (bIsJP2)
                {
                    jp2_out.close();
                    family.close();
                }
                else
                {
                    poOutputFile->close();
                }
                return nullptr;
            }

            CPLDebug("JP2KAK", "parse_string(%s)", osOpt.c_str());
        }
    }

    oCodeStream.access_siz()->finalize_all();

    // Some JP2 specific parameters.
    if (bIsJP2)
    {
        // Set dimensional information.
        // All redundant with the SIZ marker segment.
        jp2_dimensions dims = jp2_out.access_dimensions();
        dims.init(&oSizeParams);

        // Set colour space information (mandatory).
        jp2_colour colour = jp2_out.access_colour();

        if (bHaveCT || poSrcDS->GetRasterCount() == 3)
        {
            colour.init(JP2_sRGB_SPACE);
        }
        else if (poSrcDS->GetRasterCount() >= 4 &&
                 poSrcDS->GetRasterBand(4)->GetColorInterpretation() ==
                     GCI_AlphaBand)
        {
            colour.init(JP2_sRGB_SPACE);
            jp2_out.access_channels().init(3);
            jp2_out.access_channels().set_colour_mapping(0, 0);
            jp2_out.access_channels().set_colour_mapping(1, 1);
            jp2_out.access_channels().set_colour_mapping(2, 2);
            jp2_out.access_channels().set_opacity_mapping(0, 3);
            jp2_out.access_channels().set_opacity_mapping(1, 3);
            jp2_out.access_channels().set_opacity_mapping(2, 3);
        }
        else if (poSrcDS->GetRasterCount() >= 2 &&
                 poSrcDS->GetRasterBand(2)->GetColorInterpretation() ==
                     GCI_AlphaBand)
        {
            colour.init(JP2_sLUM_SPACE);
            jp2_out.access_channels().init(1);
            jp2_out.access_channels().set_colour_mapping(0, 0);
            jp2_out.access_channels().set_opacity_mapping(0, 1);
        }
        else
        {
            colour.init(JP2_sLUM_SPACE);
        }

        // Resolution.
        if (poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION") != nullptr &&
            poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION") != nullptr &&
            poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT") != nullptr)
        {
            jp2_resolution res = jp2_out.access_resolution();
            double dfXRes =
                CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION"));
            double dfYRes =
                CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION"));

            if (atoi(poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT")) == 2)
            {
                // Convert pixels per inch to pixels per cm.
                // TODO(schwehr): Change this to 1.0 / 2.54 if correct.
                const double dfInchToCm = 39.37 / 100.0;
                dfXRes *= dfInchToCm;
                dfYRes *= dfInchToCm;
            }

            // Convert to pixels per meter.
            dfXRes *= 100.0;
            dfYRes *= 100.0;

            if (dfXRes != 0.0 && dfYRes != 0.0)
            {
                if (fabs(dfXRes / dfYRes - 1.0) > 0.00001)
                    res.init(static_cast<float>(dfYRes / dfXRes));
                else
                    res.init(1.0);
                res.set_resolution(static_cast<float>(dfXRes), true);
            }
        }
    }

    // Write JP2 pseudocolor table if available.
    if (bIsJP2 && bHaveCT)
    {
        GDALColorTable *const poCT = poPrototypeBand->GetColorTable();
        const int nCount = poCT->GetColorEntryCount();
        kdu_int32 *panLUT =
            static_cast<kdu_int32 *>(CPLMalloc(sizeof(kdu_int32) * nCount * 3));

        jp2_palette oJP2Palette = jp2_out.access_palette();
        oJP2Palette.init(3, nCount);

        for (int iColor = 0; iColor < nCount; iColor++)
        {
            GDALColorEntry sEntry = {0, 0, 0, 0};

            poCT->GetColorEntryAsRGB(iColor, &sEntry);
            panLUT[iColor + nCount * 0] = sEntry.c1;
            panLUT[iColor + nCount * 1] = sEntry.c2;
            panLUT[iColor + nCount * 2] = sEntry.c3;
        }

        oJP2Palette.set_lut(0, panLUT + nCount * 0, 8, false);
        oJP2Palette.set_lut(1, panLUT + nCount * 1, 8, false);
        oJP2Palette.set_lut(2, panLUT + nCount * 2, 8, false);

        CPLFree(panLUT);

        jp2_channels oJP2Channels = jp2_out.access_channels();

        oJP2Channels.init(3);
        oJP2Channels.set_colour_mapping(0, 0, 0);
        oJP2Channels.set_colour_mapping(1, 0, 1);
        oJP2Channels.set_colour_mapping(2, 0, 2);
    }

    if (bIsJP2)
    {
        try
        {
            jp2_out.write_header();
        }
        catch (...)
        {
            CPLDebug("JP2KAK", "jp2_out.write_header() - caught exception.");
            oCodeStream.destroy();
            return nullptr;
        }
    }

    // Set the GeoTIFF and GML boxes if georeferencing is available,
    // and this is a JP2 file.
    double adfGeoTransform[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    // cppcheck-suppress knownConditionTrueFalse
    if (bIsJP2 &&
        ((poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None &&
          (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0 ||
           adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0 ||
           adfGeoTransform[4] != 0.0 || std::abs(adfGeoTransform[5]) != 1.0)) ||
         poSrcDS->GetGCPCount() > 0 || poSrcDS->GetMetadata("RPC") != nullptr))
    {
        GDALJP2Metadata oJP2MD;

        if (poSrcDS->GetGCPCount() > 0)
        {
            oJP2MD.SetSpatialRef(poSrcDS->GetGCPSpatialRef());
            oJP2MD.SetGCPs(poSrcDS->GetGCPCount(), poSrcDS->GetGCPs());
        }
        else
        {
            oJP2MD.SetSpatialRef(poSrcDS->GetSpatialRef());
            oJP2MD.SetGeoTransform(adfGeoTransform);
        }

        oJP2MD.SetRPCMD(poSrcDS->GetMetadata("RPC"));

        const char *const pszAreaOrPoint =
            poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        oJP2MD.bPixelIsPoint = pszAreaOrPoint != nullptr &&
                               EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT);

        if (CPLFetchBool(papszOptions, "GMLJP2", true))
        {
            const char *pszGMLJP2V2Def =
                CSLFetchNameValue(papszOptions, "GMLJP2V2_DEF");
            GDALJP2Box *poBox = nullptr;
            if (pszGMLJP2V2Def != nullptr)
            {
                poBox = oJP2MD.CreateGMLJP2V2(nXSize, nYSize, pszGMLJP2V2Def,
                                              poSrcDS);
            }
            else
            {
                const OGRSpatialReference *poSRS =
                    poSrcDS->GetGCPCount() > 0 ? poSrcDS->GetGCPSpatialRef()
                                               : poSrcDS->GetSpatialRef();
                if (!poSRS || poSRS->IsEmpty() ||
                    GDALJP2Metadata::IsSRSCompatible(poSRS))
                {
                    poBox = oJP2MD.CreateGMLJP2(nXSize, nYSize);
                }
                else if (CSLFetchNameValue(papszOptions, "GMLJP2"))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "GMLJP2 box was explicitly required but cannot be "
                             "written due "
                             "to lack of georeferencing and/or unsupported "
                             "georeferencing "
                             "for GMLJP2");
                }
                else
                {
                    CPLDebug("JP2KAK",
                             "Cannot write GMLJP2 box due to unsupported SRS");
                }
            }
            if (poBox)
            {
                try
                {
                    JP2KAKWriteBox(&family, poBox);
                }
                catch (...)
                {
                    CPLDebug("JP2KAK", "JP2KAKWriteBox) - caught exception.");
                    oCodeStream.destroy();
                    delete poBox;
                    return nullptr;
                }
            }
        }
        if (CPLFetchBool(papszOptions, "GeoJP2", true))
        {
            GDALJP2Box *poBox = oJP2MD.CreateJP2GeoTIFF();
            try
            {
                JP2KAKWriteBox(&family, poBox);
            }
            catch (...)
            {
                CPLDebug("JP2KAK", "JP2KAKWriteBox) - caught exception.");
                oCodeStream.destroy();
                delete poBox;
                return nullptr;
            }
        }
    }

    // Do we have any XML boxes we want to preserve?

    for (int iBox = 0; true; iBox++)
    {
        CPLString oName;
        oName.Printf("xml:BOX_%d", iBox);
        char **papszMD = poSrcDS->GetMetadata(oName);

        if (papszMD == nullptr || CSLCount(papszMD) != 1)
            break;

        GDALJP2Box *poXMLBox = new GDALJP2Box();

        poXMLBox->SetType("xml ");
        poXMLBox->SetWritableData(static_cast<int>(strlen(papszMD[0]) + 1),
                                  reinterpret_cast<GByte *>(papszMD[0]));
        JP2KAKWriteBox(&family, poXMLBox);
    }

    // Open codestream box.
    // cppcheck-suppress knownConditionTrueFalse
    if (bIsJP2)
        jp2_out.open_codestream();

    // Setup the thread environment.

    int nNumThreads = atoi(CPLGetConfigOption("JP2KAK_THREADS", "-1"));
    if (nNumThreads == -1)
        nNumThreads = kdu_get_num_processors() - 1;
    if (nNumThreads > 1024)
        nNumThreads = 1024;

    kdu_thread_env *poThreadEnv = nullptr;
    if (nNumThreads > 0)
    {
        poThreadEnv = new kdu_thread_env;
        poThreadEnv->create();

        for (int iThread = 0; iThread < nNumThreads; iThread++)
        {
            if (!poThreadEnv->add_thread())
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "JP2KAK_THREADS: Unable to create thread.");
                break;
            }
        }
        CPLDebug("JP2KAK", "Using %d threads.", nNumThreads);
    }
    else
    {
        CPLDebug("JP2KAK", "Operating in singlethreaded mode.");
    }

    bool bOK = true;
    const bool bFlushEnabled = CPLFetchBool(papszOptions, "FLUSH", true);

    const int num_components = poSrcDS->GetRasterCount();
    const int nDataTypeSizeBytes = GDALGetDataTypeSizeBytes(eType);

    // Determine if we can use the kdu_stripe_compressor logic
    const auto nMaxBufferSize = std::strtoull(
        CPLGetConfigOption("JP2KAK_MAX_BUFFER_SIZE",
                           CPLSPrintf(CPL_FRMT_GUIB, GDALGetCacheMax64() / 4)),
        nullptr, 10);
    const auto nLineBufferSize = static_cast<unsigned long long>(nXSize) *
                                 num_components * nDataTypeSizeBytes;
    const auto nNeededBufferSize = nLineBufferSize * nTileYSize;
    const char *pszUseStripeCompressor =
        CPLGetConfigOption("JP2KAK_USE_STRIPE_COMPRESSOR", nullptr);
    if (poROIImage == nullptr && pszUseStripeCompressor == nullptr &&
        (nTileXSize < nXSize || nTileYSize < nYSize))
    {
        // We want to be able to push a stripe of the raster width and tile
        // height
        if (nNeededBufferSize > nMaxBufferSize)
        {
            CPLDebug("JP2KAK",
                     "Using kdu_multi_analysis because "
                     "nNeededBufferSize = " CPL_FRMT_GUIB
                     " is > JP2KAK_MAX_BUFFER_SIZE = " CPL_FRMT_GUIB,
                     static_cast<GUIntBig>(nNeededBufferSize),
                     static_cast<GUIntBig>(nMaxBufferSize));
            pszUseStripeCompressor = "NO";
        }
    }
    if (pszUseStripeCompressor == nullptr)
        pszUseStripeCompressor = poROIImage == nullptr ? "YES" : "NO";

    if (CPLTestBool(pszUseStripeCompressor))
    {
        CPLDebug("JP2KAK", "Using stripe compressor");

        kdu_stripe_compressor compressor;
        compressor.start(oCodeStream, layer_count, layer_bytes.data(),
                         /*const kdu_uint16 *layer_slopes=*/nullptr,
                         /*kdu_uint16 min_slope_threshold=*/0,
                         /*bool no_auto_complexity_control=*/false,
                         /*bool force_precise=*/false,
                         /*bool record_layer_info_in_comment=*/bComseg,
                         /*double size_tolerance=*/0.0,
                         /*int num_components=*/0,
                         /*bool want_fastest=*/false,
                         /*kdu_thread_env *env=*/poThreadEnv,
                         /*kdu_thread_queue *env_queue=*/nullptr);

        std::vector<int> recommended_stripe_heights(num_components);
        compressor.get_recommended_stripe_heights(
            1,
            std::max(1, static_cast<int>(
                            std::min(nMaxBufferSize, nNeededBufferSize) /
                            nLineBufferSize)),
            &recommended_stripe_heights[0], nullptr);
        const int stripe_height = recommended_stripe_heights[0];
        CPLDebug("JP2KAK", "stripe_height = %d", stripe_height);
        GByte *pBuffer = static_cast<GByte *>(VSI_MALLOC3_VERBOSE(
            num_components * nDataTypeSizeBytes, nXSize, stripe_height));
        if (pBuffer == nullptr)
            bOK = false;
        std::vector<kdu_byte *> stripe_bufs(num_components);
        std::vector<int> stripe_heights = recommended_stripe_heights;
        std::vector<vector_safe_bool> is_signed(num_components);
        std::vector<int> precisions(num_components);
        for (int i = 0; i < num_components; ++i)
        {
            stripe_bufs[i] = pBuffer + nXSize * nDataTypeSizeBytes * i;
            is_signed[i] = CPL_TO_BOOL(GDALDataTypeIsSigned(eType));
            precisions[i] = nBits;
        }
        const int flush_period = bFlushEnabled ? TILE_CHUNK_SIZE : 0;
        int nHeight = stripe_height;

        for (int iY = 0; bOK && iY < nYSize; iY += stripe_height)
        {
            if (iY + nHeight > nYSize)
            {
                nHeight = nYSize - iY;
                for (int i = 0; i < num_components; ++i)
                {
                    stripe_heights[i] = nHeight;
                }
            }
            if (poSrcDS->RasterIO(GF_Read, 0, iY, nXSize, nHeight, pBuffer,
                                  nXSize, nHeight, eType,
                                  poSrcDS->GetRasterCount(), nullptr, 0, 0, 0,
                                  nullptr) != CE_None)
            {
                bOK = false;
                break;
            }

            if (!pfnProgress((iY + nHeight) / static_cast<double>(nYSize),
                             nullptr, pProgressData))
            {
                bOK = false;
                break;
            }

            if (eType == GDT_Byte)
                compressor.push_stripe(stripe_bufs.data(),
                                       stripe_heights.data(),
                                       /*const int *sample_gaps=*/nullptr,
                                       /*const int *row_gaps=*/nullptr,
                                       precisions.data(), flush_period);
            else if (nDataTypeSizeBytes == 2)
                compressor.push_stripe(
                    reinterpret_cast<kdu_int16 **>(stripe_bufs.data()),
                    stripe_heights.data(),
                    /*const int *sample_gaps=*/nullptr,
                    /*const int *row_gaps=*/nullptr, precisions.data(),
                    &is_signed[0], flush_period);
            else
                compressor.push_stripe(
                    reinterpret_cast<kdu_int32 **>(stripe_bufs.data()),
                    stripe_heights.data(),
                    /*const int *sample_gaps=*/nullptr,
                    /*const int *row_gaps=*/nullptr, precisions.data(),
                    &is_signed[0], flush_period);
        }

        const bool bFinishOK = compressor.finish();
        if (bOK && !bFinishOK)
        {
            // shouldn't happen unless we provided a wrong number of lines
            CPLError(CE_Failure, CPLE_AppDefined, "compressor.finish() failed");
        }
        bOK &= bFinishOK;
    }
    else
    {
        // Iterate over tiles
        double dfPixelsDone = 0.0;

        for (int iTileYOff = 0; bOK && iTileYOff < nYSize;
             iTileYOff += nTileYSize)
        {
            for (int iTileXOff = 0; bOK && iTileXOff < nXSize;
                 iTileXOff += nTileXSize)
            {
                kdu_tile oTile = oCodeStream.open_tile(
                    kdu_coords(iTileXOff / nTileXSize, iTileYOff / nTileYSize),
                    poThreadEnv);

                // Is this a partial tile on the right or bottom?
                const int nThisTileXSize = iTileXOff + nTileXSize < nXSize
                                               ? nTileXSize
                                               : nXSize - iTileXOff;

                const int nThisTileYSize = iTileYOff + nTileYSize < nYSize
                                               ? nTileYSize
                                               : nYSize - iTileYOff;

                // Setup scaled progress monitor.

                const double dfPixelsDoneAfter =
                    dfPixelsDone +
                    static_cast<double>(nThisTileXSize) * nThisTileYSize;

                void *pScaledProgressData =
                    GDALCreateScaledProgress(dfPixelsDone / dfPixelsTotal,
                                             dfPixelsDoneAfter / dfPixelsTotal,
                                             pfnProgress, pProgressData);

                if (!JP2KAKCreateCopy_WriteTile(
                        poSrcDS, oTile, poThreadEnv, poROIImage, iTileXOff,
                        iTileYOff, nThisTileXSize, nThisTileYSize, nBits, eType,
                        oCodeStream, bFlushEnabled, layer_bytes.data(),
                        layer_count, GDALScaledProgress, pScaledProgressData,
                        bComseg))
                {
                    bOK = false;
                }

                GDALDestroyScaledProgress(pScaledProgressData);
                dfPixelsDone = dfPixelsDoneAfter;

                try
                {
                    oTile.close(poThreadEnv);
                }
                catch (...)
                {
                    CPLDebug(
                        "JP2KAK",
                        "CreateCopy() - caught exception in oTile.close()");
                    bOK = false;
                    break;
                }
            }
        }

        delete poROIImage;
    }

    if (poThreadEnv)
    {
        poThreadEnv->cs_terminate(oCodeStream);
        poThreadEnv->destroy();
        delete poThreadEnv;
    }

    // Finish flushing out results.
    try
    {
        oCodeStream.flush(layer_bytes.data(), layer_count, nullptr, true,
                          bComseg);
    }
    catch (...)
    {
        CPLDebug("JP2KAK",
                 "CreateCopy() - caught exception in oCodeStream.flush()");
        bOK = false;
    }

    oCodeStream.destroy();

    if (bIsJP2)
    {
        jp2_out.close();
        family.close();
    }
    else
    {
        poOutputFile->close();
    }

    oVSILTarget.close();
    if (!bOK)
    {
        VSIUnlink(pszFilename);
        return nullptr;
    }

    if (!pfnProgress(1.0, nullptr, pProgressData))
        return nullptr;

    // Re-open dataset, and copy any auxiliary pam information.
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    GDALPamDataset *poDS =
        static_cast<GDALPamDataset *>(JP2KAKDataset::Open(&oOpenInfo));

    if (poDS)
        poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_JP2KAK()                         */
/************************************************************************/

void GDALRegister_JP2KAK()

{
    if (!GDAL_CHECK_VERSION("JP2KAK driver"))
        return;

    if (GDALGetDriverByName("JP2KAK") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    JP2KAKDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = JP2KAKDataset::Open;
    poDriver->pfnCreateCopy = JP2KAKCreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
