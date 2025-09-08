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

#ifndef GTIFFRGBABAND_H_INCLUDED
#define GTIFFRGBABAND_H_INCLUDED

#include "gtiffrasterband.h"

/************************************************************************/
/* ==================================================================== */
/*                             GTiffRGBABand                            */
/* ==================================================================== */
/************************************************************************/

class GTiffRGBABand final : public GTiffRasterBand
{
    friend class GTiffDataset;

  public:
    GTiffRGBABand(GTiffDataset *, int);

    bool IsBaseGTiffClass() const override
    {
        return false;
    }

    virtual int IGetDataCoverageStatus(int nXOff, int nYOff, int nXSize,
                                       int nYSize, int nMaskFlagStop,
                                       double *pdfDataPct) override;

    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IWriteBlock(int, int, void *) override;

    GDALColorInterp GetColorInterpretation() override;
};

#endif  // GTIFFRGBABAND_H_INCLUDED
