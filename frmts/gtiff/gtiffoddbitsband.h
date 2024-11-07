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

#ifndef GTIFFODDBITSBAND_H_INCLUDED
#define GTIFFODDBITSBAND_H_INCLUDED

#include "gtiffrasterband.h"

/************************************************************************/
/* ==================================================================== */
/*                             GTiffOddBitsBand                         */
/* ==================================================================== */
/************************************************************************/

class GTiffOddBitsBand CPL_NON_FINAL : public GTiffRasterBand
{
    friend class GTiffDataset;

  public:
    GTiffOddBitsBand(GTiffDataset *, int);

    virtual ~GTiffOddBitsBand()
    {
    }

    bool IsBaseGTiffClass() const override
    {
        return false;
    }

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
};

#endif  // GTIFFODDBITSBAND_H_INCLUDED
