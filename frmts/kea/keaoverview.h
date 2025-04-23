/*
 *  keaoverview.h
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef KEAOVERVIEW_H
#define KEAOVERVIEW_H

#include "cpl_port.h"
#include "keaband.h"

// overview class. Derives from our band class
// and just overrides the read/write block functions
class KEAOverview final : public KEARasterBand
{
    int m_nOverviewIndex;  // the index of this overview
  public:
    KEAOverview(KEADataset *pDataset, int nSrcBand, GDALAccess eAccess,
                kealib::KEAImageIO *pImageIO, LockedRefCount *pRefCount,
                int nOverviewIndex, uint64_t nXSize, uint64_t nYSize);
    ~KEAOverview();

    // virtual methods for RATs - not implemented for overviews
    GDALRasterAttributeTable *GetDefaultRAT() override;

    CPLErr SetDefaultRAT(const GDALRasterAttributeTable *poRAT) override;

    // note that Color Table stuff implemented in base class
    // so could be some duplication if overview asked for color table

  protected:
    // we just override these functions from KEARasterBand
    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
};

#endif  // KEAOVERVIEW_H
