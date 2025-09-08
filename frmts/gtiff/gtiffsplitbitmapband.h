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

#ifndef GTIFFSPLITBITMAPBAND_H_INCLUDED
#define GTIFFSPLITBITMAPBAND_H_INCLUDED

#include "gtiffbitmapband.h"

/************************************************************************/
/* ==================================================================== */
/*                          GTiffSplitBitmapBand                        */
/* ==================================================================== */
/************************************************************************/

class GTiffSplitBitmapBand final : public GTiffBitmapBand
{
    friend class GTiffDataset;
    int m_nLastLineValid = -1;

  public:
    GTiffSplitBitmapBand(GTiffDataset *, int);
    ~GTiffSplitBitmapBand() override;

    virtual int IGetDataCoverageStatus(int nXOff, int nYOff, int nXSize,
                                       int nYSize, int nMaskFlagStop,
                                       double *pdfDataPct) override;

    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IWriteBlock(int, int, void *) override;
};

#endif  // GTIFFSPLITBITMAPBAND_H_INCLUDED
