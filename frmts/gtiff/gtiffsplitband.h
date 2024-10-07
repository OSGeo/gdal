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

#ifndef GTIFFSPLITBAND_H_INCLUDED
#define GTIFFSPLITBAND_H_INCLUDED

#include "gtiffrasterband.h"

/************************************************************************/
/* ==================================================================== */
/*                             GTiffSplitBand                           */
/* ==================================================================== */
/************************************************************************/

class GTiffSplitBand final : public GTiffRasterBand
{
    friend class GTiffDataset;

  public:
    GTiffSplitBand(GTiffDataset *, int);

    virtual ~GTiffSplitBand()
    {
    }

    bool IsBaseGTiffClass() const override
    {
        return false;
    }

    virtual int IGetDataCoverageStatus(int nXOff, int nYOff, int nXSize,
                                       int nYSize, int nMaskFlagStop,
                                       double *pdfDataPct) override;

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
};

#endif  // GTIFFSPLITBAND_H_INCLUDED
