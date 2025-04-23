/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of RasterBand classes for WCS.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef WCSRASTERBAND_H_INCLUDED
#define WCSRASTERBAND_H_INCLUDED

#include "gdal_pam.h"

/************************************************************************/
/* ==================================================================== */
/*                            WCSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class WCSRasterBand final : public GDALPamRasterBand
{
    friend class WCSDataset;

    int iOverview;
    int nResFactor;

    WCSDataset *poODS;

    int nOverviewCount;
    WCSRasterBand **papoOverviews;

    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, GSpacing nPixelSpace,
                             GSpacing nLineSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

  public:
    WCSRasterBand(WCSDataset *, int nBand, int iOverview);
    virtual ~WCSRasterBand();

    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;

    virtual CPLErr IReadBlock(int, int, void *) override;
};

#endif /* WCSRASTERBAND_H_INCLUDED */
