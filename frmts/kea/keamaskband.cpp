/*
 *  keamaskband.cpp
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "keamaskband.h"

// constructor
KEAMaskBand::KEAMaskBand(GDALRasterBand *pParent, kealib::KEAImageIO *pImageIO,
                         LockedRefCount *pRefCount)
{
    m_nSrcBand = pParent->GetBand();
    poDS = nullptr;
    nBand = 0;

    nRasterXSize = pParent->GetXSize();
    nRasterYSize = pParent->GetYSize();

    eDataType = GDT_Byte;
    pParent->GetBlockSize(&nBlockXSize, &nBlockYSize);
    eAccess = pParent->GetAccess();

    // grab the imageio class and its refcount
    this->m_pImageIO = pImageIO;
    this->m_pRefCount = pRefCount;
    // increment the refcount as we now have a reference to imageio
    this->m_pRefCount->IncRef();
}

KEAMaskBand::~KEAMaskBand()
{
    // according to the docs, this is required
    this->FlushCache(true);

    // decrement the recount and delete if needed
    if (m_pRefCount->DecRef())
    {
        try
        {
            m_pImageIO->close();
        }
        catch (const kealib::KEAIOException &)
        {
        }
        delete m_pImageIO;
        delete m_pRefCount;
    }
}

// overridden implementation - calls readImageBlock2BandMask instead
CPLErr KEAMaskBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)
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
        this->m_pImageIO->readImageBlock2BandMask(
            this->m_nSrcBand, pImage, this->nBlockXSize * nBlockXOff,
            this->nBlockYSize * nBlockYOff, nxsize, nysize, this->nBlockXSize,
            this->nBlockYSize, kealib::kea_8uint);
    }
    catch (kealib::KEAIOException &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to read file: %s",
                 e.what());
        return CE_Failure;
    }
    return CE_None;
}

// overridden implementation - calls writeImageBlock2BandMask instead
CPLErr KEAMaskBand::IWriteBlock(int nBlockXOff, int nBlockYOff, void *pImage)
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

        this->m_pImageIO->writeImageBlock2BandMask(
            this->m_nSrcBand, pImage, this->nBlockXSize * nBlockXOff,
            this->nBlockYSize * nBlockYOff, nxsize, nysize, this->nBlockXSize,
            this->nBlockYSize, kealib::kea_8uint);
    }
    catch (kealib::KEAIOException &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to write file: %s",
                 e.what());
        return CE_Failure;
    }
    return CE_None;
}
