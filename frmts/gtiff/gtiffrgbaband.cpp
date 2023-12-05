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

#include "gtiffrgbaband.h"
#include "gtiffdataset.h"

#include "tiffio.h"

/************************************************************************/
/*                           GTiffRGBABand()                            */
/************************************************************************/

GTiffRGBABand::GTiffRGBABand(GTiffDataset *poDSIn, int nBandIn)
    : GTiffRasterBand(poDSIn, nBandIn)
{
    eDataType = GDT_Byte;
}

/************************************************************************/
/*                       IGetDataCoverageStatus()                       */
/************************************************************************/

int GTiffRGBABand::IGetDataCoverageStatus(int, int, int, int, int, double *)
{
    return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
           GDAL_DATA_COVERAGE_STATUS_DATA;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffRGBABand::IWriteBlock(int, int, void *)

{
    ReportError(CE_Failure, CPLE_AppDefined,
                "RGBA interpreted raster bands are read-only.");
    return CE_Failure;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRGBABand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    m_poGDS->Crystalize();

    const auto nBlockBufSize =
        4 * static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize;
    const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
    {
        for (int iBand = 0; iBand < m_poGDS->m_nSamplesPerPixel; iBand++)
        {
            int nBlockIdBand = nBlockId + iBand * m_poGDS->m_nBlocksPerBand;
            if (!m_poGDS->IsBlockAvailable(nBlockIdBand))
                return CE_Failure;
        }
    }
    else
    {
        if (!m_poGDS->IsBlockAvailable(nBlockId))
            return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Allocate a temporary buffer for this strip.                     */
    /* -------------------------------------------------------------------- */
    if (m_poGDS->m_pabyBlockBuf == nullptr)
    {
        m_poGDS->m_pabyBlockBuf = static_cast<GByte *>(
            VSI_MALLOC3_VERBOSE(4, nBlockXSize, nBlockYSize));
        if (m_poGDS->m_pabyBlockBuf == nullptr)
            return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the strip                                                  */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if (m_poGDS->m_nLoadedBlock != nBlockId)
    {
        if (TIFFIsTiled(m_poGDS->m_hTIFF))
        {
            if (TIFFReadRGBATileExt(
                    m_poGDS->m_hTIFF, nBlockXOff * nBlockXSize,
                    nBlockYOff * nBlockYSize,
                    reinterpret_cast<uint32_t *>(m_poGDS->m_pabyBlockBuf),
                    !m_poGDS->m_bIgnoreReadErrors) == 0 &&
                !m_poGDS->m_bIgnoreReadErrors)
            {
                // Once TIFFError() is properly hooked, this can go away.
                ReportError(CE_Failure, CPLE_AppDefined,
                            "TIFFReadRGBATile() failed.");

                memset(m_poGDS->m_pabyBlockBuf, 0, nBlockBufSize);

                eErr = CE_Failure;
            }
        }
        else
        {
            if (TIFFReadRGBAStripExt(
                    m_poGDS->m_hTIFF, nBlockId * nBlockYSize,
                    reinterpret_cast<uint32_t *>(m_poGDS->m_pabyBlockBuf),
                    !m_poGDS->m_bIgnoreReadErrors) == 0 &&
                !m_poGDS->m_bIgnoreReadErrors)
            {
                // Once TIFFError() is properly hooked, this can go away.
                ReportError(CE_Failure, CPLE_AppDefined,
                            "TIFFReadRGBAStrip() failed.");

                memset(m_poGDS->m_pabyBlockBuf, 0, nBlockBufSize);

                eErr = CE_Failure;
            }
        }
    }

    m_poGDS->m_nLoadedBlock = eErr == CE_None ? nBlockId : -1;

    /* -------------------------------------------------------------------- */
    /*      Handle simple case of eight bit data, and pixel interleaving.   */
    /* -------------------------------------------------------------------- */
    int nThisBlockYSize = nBlockYSize;

    if (nBlockYOff * nBlockYSize > GetYSize() - nBlockYSize &&
        !TIFFIsTiled(m_poGDS->m_hTIFF))
        nThisBlockYSize = GetYSize() - nBlockYOff * nBlockYSize;

#ifdef CPL_LSB
    const int nBO = nBand - 1;
#else
    const int nBO = 4 - nBand;
#endif

    for (int iDestLine = 0; iDestLine < nThisBlockYSize; ++iDestLine)
    {
        const auto nSrcOffset =
            static_cast<GPtrDiff_t>(nThisBlockYSize - iDestLine - 1) *
            nBlockXSize * 4;

        GDALCopyWords(m_poGDS->m_pabyBlockBuf + nBO + nSrcOffset, GDT_Byte, 4,
                      static_cast<GByte *>(pImage) +
                          static_cast<GPtrDiff_t>(iDestLine) * nBlockXSize,
                      GDT_Byte, 1, nBlockXSize);
    }

    if (eErr == CE_None)
        eErr = FillCacheForOtherBands(nBlockXOff, nBlockYOff);

    return eErr;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRGBABand::GetColorInterpretation()

{
    if (nBand == 1)
        return GCI_RedBand;
    if (nBand == 2)
        return GCI_GreenBand;
    if (nBand == 3)
        return GCI_BlueBand;

    return GCI_AlphaBand;
}
