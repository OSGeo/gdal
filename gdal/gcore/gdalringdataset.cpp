/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of a ring view dataset
 * Author:   Momtchil Momtchev, <momtchil at momtchev dot com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Momtchil Momtchev, <momtchil at momtchev dot com>
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
#include "gdal_priv.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_mdreader.h"
#include "gdal_proxy.h"

CPL_CVSID("$Id$")

/**
 * A ring dataset is a dataset wrapper that provides a
 * continuous view that automatically wraps around one of its
 * axes
 */

static int GetEasternAxis(const OGRSpatialReference *poSRS)
{
    int i;
    for (i = 0; i < 3; i++)
    {
        OGRAxisOrientation eOrientation;
        poSRS->GetAxis("GEOGCS", i, &eOrientation);
        if (eOrientation == OAO_East)
            break;
    }
    if (i == 3)
        throw "Cannot create ring dataset without an eastern axis";
    return i;
}

static inline int WrapAxis(int nOffset, int nSize)
{
    return ((nOffset % nSize) + nSize) % nSize;
}

GDALRingDataset::GDALRingDataset(GDALDataset* poDS)
  : m_poUnderlying(poDS)
{
    try
    {
        OGRSpatialReference const* poSRS = m_poUnderlying->GetSpatialRef();
        if (!poSRS->IsGeographic())
            CPLError(CE_Warning,
                     CPLE_AppDefined,
                     "Creating a ring dataset for a projected dataset"
                     " (%s)",
                     poDS->GetDescription());
        switch (poSRS->GetAxisMappingStrategy())
        {
            case OAMS_TRADITIONAL_GIS_ORDER:
                m_nWrappedAxis = 1;
                break;
            case OAMS_AUTHORITY_COMPLIANT:
                m_nWrappedAxis = GetEasternAxis(poSRS) + 1;
                break;
            case OAMS_CUSTOM:
                m_nWrappedAxis =
                  poSRS->GetDataAxisToSRSAxisMapping()[GetEasternAxis(poSRS)];
                break;
        }
    }
    catch (const char* err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", err);
    }

    nRasterXSize = m_poUnderlying->GetRasterXSize();
    nRasterYSize = m_poUnderlying->GetRasterYSize();
}

GDALDataset *GDALRingDataset::RefUnderlyingDataset() const
{
    m_poUnderlying->Reference();
    return m_poUnderlying;
}

void GDALRingDataset::UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset) const
{
    CPLAssert(m_poUnderlying == poUnderlyingDataset);
    m_poUnderlying->Dereference();
}

GDALRasterBand * GDALRingDataset::GetRasterBand(int nBandId)
{
    GDALRasterBand* poBand = m_poUnderlying->GetRasterBand(nBandId);
    if (poBand == nullptr)
        return nullptr;

    GDALRingRasterBand* poRing;
    if (papoBands != nullptr && nBandId <= nBands)
    {
        poRing = reinterpret_cast<GDALRingRasterBand*>(papoBands[nBandId - 1]);
        if (poRing->m_poUnderlying == poBand)
            return papoBands[nBandId - 1];
        delete poRing;
    }
    poRing = new GDALRingRasterBand(poBand);
    poRing->nBand = nBandId;
    GDALDataset::SetBand(nBandId, poRing);
    return papoBands[nBandId - 1];
}

GDALRingRasterBand::GDALRingRasterBand(GDALRasterBand* poBand)
  : m_poUnderlying(poBand)
{
    nRasterXSize = m_poUnderlying->GetXSize();
    nRasterYSize = m_poUnderlying->GetYSize();
    m_poUnderlying->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

GDALRasterBand *GDALRingRasterBand::RefUnderlyingRasterBand()
{
    return m_poUnderlying;
}

void GDALRingRasterBand::UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand)
{
    CPLAssert(m_poUnderlying == poUnderlyingRasterBand);
}

CPLErr GDALRingRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GDALRasterIOExtraArg* psExtraArg )
{
    GDALRingDataset *poRDS = reinterpret_cast<GDALRingDataset*>(poDS);

    if (poRDS->m_nWrappedAxis == 1)
    {
        // Where does the reading start in underlying coordinates
        int nXStart;
        // Number of columns left to read and current column in ring coordinates
        int nColToRead, nColDstCurrent;

        nXStart = WrapAxis(nXOff, nRasterXSize);
        nColToRead = nXSize;
        nColDstCurrent = 0;

        while (nColToRead > 0)
        {
            int nChunkSize = nXStart + nColToRead > nRasterXSize ?
                    nRasterXSize - nXStart :
                    nColToRead;
            int nBufferChunkSize = nBufXSize * nChunkSize / nXSize;
            GByte *pDataChunkOffset = reinterpret_cast<GByte*>(pData) +
                    nColDstCurrent * nPixelSpace;

            CPLErr r = m_poUnderlying->RasterIO(eRWFlag, nXStart, nYOff, nChunkSize, nYSize,
                    pDataChunkOffset, nBufferChunkSize, nBufYSize, eBufType, nPixelSpace, nLineSpace,
                    psExtraArg);
            if (r != CE_None) return r;
            nColToRead -= nChunkSize;
            nColDstCurrent += nChunkSize;
            nXStart = (nXStart + nChunkSize) % nRasterXSize;
        }
    }

    if (poRDS->m_nWrappedAxis == 2)
    {
        // Where does the reading start in underlying coordinates
        int nYStart;
        // Number of row left to read and current row in ring coordinates
        int nRowToRead, nRowDstCurrent;

        nYStart = WrapAxis(nYOff, nRasterYSize);
        nRowToRead = nYSize;
        nRowDstCurrent = 0;

        while (nRowToRead > 0)
        {
            int nChunkSize = nYStart + nRowToRead > nRasterYSize ?
                    nRasterYSize - nYStart :
                    nRowToRead;
            int nBufferChunkSize = nBufXSize * nChunkSize / nXSize;
            GByte *pDataChunkOffset = reinterpret_cast<GByte*>(pData) +
                    nRowDstCurrent * nLineSpace;

            CPLErr r = m_poUnderlying->RasterIO(eRWFlag, nXOff, nYStart, nChunkSize, nYSize,
                    pDataChunkOffset, nBufferChunkSize, nBufYSize, eBufType, nPixelSpace, nLineSpace,
                    psExtraArg);
            if (r != CE_None) return r;
            nRowToRead -= nChunkSize;
            nRowDstCurrent += nChunkSize;
            nYStart = (nYStart + nChunkSize) % nRasterXSize;
        }
    }

    return CE_None;
}
