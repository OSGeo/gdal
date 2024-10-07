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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gtiffsplitband.h"

#include "gtiffdataset.h"

/************************************************************************/
/*                           GTiffSplitBand()                           */
/************************************************************************/

GTiffSplitBand::GTiffSplitBand(GTiffDataset *poDSIn, int nBandIn)
    : GTiffRasterBand(poDSIn, nBandIn)
{
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                       IGetDataCoverageStatus()                       */
/************************************************************************/

int GTiffSplitBand::IGetDataCoverageStatus(int, int, int, int, int, double *)
{
    return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
           GDAL_DATA_COVERAGE_STATUS_DATA;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBand::IReadBlock(int /* nBlockXOff */, int nBlockYOff,
                                  void *pImage)

{
    m_poGDS->Crystalize();

    // Optimization when reading the same line in a contig multi-band TIFF.
    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        m_poGDS->nBands > 1 && m_poGDS->m_nLoadedBlock == nBlockYOff)
    {
        goto extract_band_data;
    }

    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG && m_poGDS->nBands > 1)
    {
        if (m_poGDS->m_pabyBlockBuf == nullptr)
        {
            m_poGDS->m_pabyBlockBuf = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(TIFFScanlineSize(m_poGDS->m_hTIFF)));
            if (m_poGDS->m_pabyBlockBuf == nullptr)
            {
                return CE_Failure;
            }
        }
    }
    else
    {
        CPLAssert(TIFFScanlineSize(m_poGDS->m_hTIFF) == nBlockXSize);
    }

    /* -------------------------------------------------------------------- */
    /*      Read through to target scanline.                                */
    /* -------------------------------------------------------------------- */
    if (m_poGDS->m_nLoadedBlock >= nBlockYOff)
        m_poGDS->m_nLoadedBlock = -1;

    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE &&
        m_poGDS->nBands > 1)
    {
        // If we change of band, we must start reading the
        // new strip from its beginning.
        if (m_poGDS->m_nLastBandRead != nBand)
            m_poGDS->m_nLoadedBlock = -1;
        m_poGDS->m_nLastBandRead = nBand;
    }

    while (m_poGDS->m_nLoadedBlock < nBlockYOff)
    {
        ++m_poGDS->m_nLoadedBlock;
        if (TIFFReadScanline(m_poGDS->m_hTIFF,
                             m_poGDS->m_pabyBlockBuf ? m_poGDS->m_pabyBlockBuf
                                                     : pImage,
                             m_poGDS->m_nLoadedBlock,
                             (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
                                 ? static_cast<uint16_t>(nBand - 1)
                                 : 0) == -1 &&
            !m_poGDS->m_bIgnoreReadErrors)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "TIFFReadScanline() failed.");
            m_poGDS->m_nLoadedBlock = -1;
            return CE_Failure;
        }
    }

extract_band_data:
    /* -------------------------------------------------------------------- */
    /*      Extract band data from contig buffer.                           */
    /* -------------------------------------------------------------------- */
    if (m_poGDS->m_pabyBlockBuf != nullptr)
    {
        for (int iPixel = 0, iSrcOffset = nBand - 1, iDstOffset = 0;
             iPixel < nBlockXSize;
             ++iPixel, iSrcOffset += m_poGDS->nBands, ++iDstOffset)
        {
            static_cast<GByte *>(pImage)[iDstOffset] =
                m_poGDS->m_pabyBlockBuf[iSrcOffset];
        }
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBand::IWriteBlock(int /* nBlockXOff */, int /* nBlockYOff */,
                                   void * /* pImage */)

{
    ReportError(CE_Failure, CPLE_AppDefined, "Split bands are read-only.");
    return CE_Failure;
}
