/*
 *  keaoverview.cpp
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "keaoverview.h"

// constructor
KEAOverview::KEAOverview(KEADataset *pDataset, int nSrcBand,
                         GDALAccess eAccessIn, kealib::KEAImageIO *pImageIO,
                         LockedRefCount *pRefCount, int nOverviewIndex,
                         uint64_t nXSize, uint64_t nYSize)
    : KEARasterBand(pDataset, nSrcBand, eAccessIn, pImageIO, pRefCount)
{
    this->m_nOverviewIndex = nOverviewIndex;
    // overridden from the band - not the same size as the band obviously
    this->nBlockXSize =
        pImageIO->getOverviewBlockSize(nSrcBand, nOverviewIndex);
    this->nBlockYSize =
        pImageIO->getOverviewBlockSize(nSrcBand, nOverviewIndex);
    this->nRasterXSize = static_cast<int>(nXSize);
    this->nRasterYSize = static_cast<int>(nYSize);
}

KEAOverview::~KEAOverview()
{
    // according to the docs, this is required
    // otherwise not all tiles will be written.
    this->FlushCache();
}

// overridden implementation - calls readFromOverview instead
CPLErr KEAOverview::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)
{
    try
    {
        // GDAL deals in blocks - if we are at the end of a row
        // we need to adjust the amount read so we don't go over the edge
        int nxsize = this->nBlockXSize;
        int nxtotalsize = this->nBlockXSize * (nBlockXOff + 1);
        if (nxtotalsize > this->nRasterXSize)
        {
            nxsize -= (nxtotalsize - this->nRasterXSize);
        }
        int nysize = this->nBlockYSize;
        int nytotalsize = this->nBlockYSize * (nBlockYOff + 1);
        if (nytotalsize > this->nRasterYSize)
        {
            nysize -= (nytotalsize - this->nRasterYSize);
        }
        this->m_pImageIO->readFromOverview(
            this->nBand, this->m_nOverviewIndex, pImage,
            this->nBlockXSize * nBlockXOff, this->nBlockYSize * nBlockYOff,
            nxsize, nysize, this->nBlockXSize, this->nBlockYSize,
            this->m_eKEADataType);
        return CE_None;
    }
    catch (kealib::KEAIOException &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to read file: %s",
                 e.what());
        return CE_Failure;
    }
}

// overridden implementation - calls writeToOverview instead
CPLErr KEAOverview::IWriteBlock(int nBlockXOff, int nBlockYOff, void *pImage)
{
    try
    {
        // GDAL deals in blocks - if we are at the end of a row
        // we need to adjust the amount written so we don't go over the edge
        int nxsize = this->nBlockXSize;
        int nxtotalsize = this->nBlockXSize * (nBlockXOff + 1);
        if (nxtotalsize > this->nRasterXSize)
        {
            nxsize -= (nxtotalsize - this->nRasterXSize);
        }
        int nysize = this->nBlockYSize;
        int nytotalsize = this->nBlockYSize * (nBlockYOff + 1);
        if (nytotalsize > this->nRasterYSize)
        {
            nysize -= (nytotalsize - this->nRasterYSize);
        }

        this->m_pImageIO->writeToOverview(
            this->nBand, this->m_nOverviewIndex, pImage,
            this->nBlockXSize * nBlockXOff, this->nBlockYSize * nBlockYOff,
            nxsize, nysize, this->nBlockXSize, this->nBlockYSize,
            this->m_eKEADataType);
        return CE_None;
    }
    catch (kealib::KEAIOException &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to write file: %s",
                 e.what());
        return CE_Failure;
    }
}

GDALRasterAttributeTable *KEAOverview::GetDefaultRAT()
{
    // KEARasterBand implements this, but we don't want to
    return nullptr;
}

CPLErr
KEAOverview::SetDefaultRAT(CPL_UNUSED const GDALRasterAttributeTable *poRAT)
{
    // KEARasterBand implements this, but we don't want to
    return CE_Failure;
}
