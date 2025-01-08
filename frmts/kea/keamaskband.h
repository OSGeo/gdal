/*
 *  keamaskband.h
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef KEAMASKBAND_H
#define KEAMASKBAND_H

#include "gdal_priv.h"

#include "libkea_headers.h"
#include "keadataset.h"

class KEAMaskBand final : public GDALRasterBand
{
    int m_nSrcBand;
    kealib::KEAImageIO *m_pImageIO;  // our image access pointer - refcounted
    LockedRefCount *m_pRefCount;     // reference count of m_pImageIO
  public:
    KEAMaskBand(GDALRasterBand *pParent, kealib::KEAImageIO *pImageIO,
                LockedRefCount *pRefCount);
    ~KEAMaskBand();

    virtual bool IsMaskBand() const override
    {
        return true;
    }

  protected:
    // we just override these functions from GDALRasterBand
    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
};

#endif  // KEAMASKBAND_H
