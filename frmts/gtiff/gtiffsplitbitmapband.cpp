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

#include "gtiffsplitbitmapband.h"

#include "gtiffdataset.h"

#include "cpl_error_internal.h"

/************************************************************************/
/*                       GTiffSplitBitmapBand()                         */
/************************************************************************/

GTiffSplitBitmapBand::GTiffSplitBitmapBand(GTiffDataset *poDSIn, int nBandIn)
    : GTiffBitmapBand(poDSIn, nBandIn)

{
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                      ~GTiffSplitBitmapBand()                         */
/************************************************************************/

GTiffSplitBitmapBand::~GTiffSplitBitmapBand()
{
}

/************************************************************************/
/*                       IGetDataCoverageStatus()                       */
/************************************************************************/

int GTiffSplitBitmapBand::IGetDataCoverageStatus(int, int, int, int, int,
                                                 double *)
{
    return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
           GDAL_DATA_COVERAGE_STATUS_DATA;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBitmapBand::IReadBlock(int /* nBlockXOff */, int nBlockYOff,
                                        void *pImage)

{
    m_poGDS->Crystalize();

    if (m_nLastLineValid >= 0 && nBlockYOff > m_nLastLineValid)
        return CE_Failure;

    if (m_poGDS->m_pabyBlockBuf == nullptr)
    {
        m_poGDS->m_pabyBlockBuf = static_cast<GByte *>(
            VSI_MALLOC_VERBOSE(TIFFScanlineSize(m_poGDS->m_hTIFF)));
        if (m_poGDS->m_pabyBlockBuf == nullptr)
        {
            return CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Read through to target scanline.                                */
    /* -------------------------------------------------------------------- */
    if (m_poGDS->m_nLoadedBlock >= nBlockYOff)
        m_poGDS->m_nLoadedBlock = -1;

    // Set to 1 to allow GTiffErrorHandler to implement limitation on error
    // messages
    GTIFFGetThreadLocalLibtiffError() = 1;
    while (m_poGDS->m_nLoadedBlock < nBlockYOff)
    {
        ++m_poGDS->m_nLoadedBlock;

        CPLErrorAccumulator oErrorAccumulator;
        int nRet;
        {
            auto oAccumulator = oErrorAccumulator.InstallForCurrentScope();
            nRet = TIFFReadScanline(m_poGDS->m_hTIFF, m_poGDS->m_pabyBlockBuf,
                                    m_poGDS->m_nLoadedBlock, 0);
        }

        for (const auto &oError : oErrorAccumulator.GetErrors())
        {
            ReportError(oError.type, oError.no, "%s", oError.msg.c_str());
            // FAX decoding only handles EOF condition as a warning, so
            // catch it so as to turn on error when attempting to read
            // following lines, to avoid performance issues.
            if (!m_poGDS->m_bIgnoreReadErrors &&
                oError.msg.find("Premature EOF") != std::string::npos)
            {
                m_nLastLineValid = nBlockYOff;
                nRet = -1;
            }
        }

        if (nRet == -1 && !m_poGDS->m_bIgnoreReadErrors)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "TIFFReadScanline() failed.");
            m_poGDS->m_nLoadedBlock = -1;
            GTIFFGetThreadLocalLibtiffError() = 0;
            return CE_Failure;
        }
    }
    GTIFFGetThreadLocalLibtiffError() = 0;

    /* -------------------------------------------------------------------- */
    /*      Translate 1bit data to eight bit.                               */
    /* -------------------------------------------------------------------- */
    int iSrcOffset = 0;
    int iDstOffset = 0;

    for (int iPixel = 0; iPixel < nBlockXSize; ++iPixel, ++iSrcOffset)
    {
        if (m_poGDS->m_pabyBlockBuf[iSrcOffset >> 3] &
            (0x80 >> (iSrcOffset & 0x7)))
            static_cast<GByte *>(pImage)[iDstOffset++] = 1;
        else
            static_cast<GByte *>(pImage)[iDstOffset++] = 0;
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBitmapBand::IWriteBlock(int /* nBlockXOff */,
                                         int /* nBlockYOff */,
                                         void * /* pImage */)

{
    ReportError(CE_Failure, CPLE_AppDefined,
                "Split bitmap bands are read-only.");
    return CE_Failure;
}
