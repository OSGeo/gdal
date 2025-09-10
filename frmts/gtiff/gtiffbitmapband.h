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

#ifndef GTIFFBITMAPBAND_H_INCLUDED
#define GTIFFBITMAPBAND_H_INCLUDED

#include "gtiffoddbitsband.h"

/************************************************************************/
/* ==================================================================== */
/*                             GTiffBitmapBand                          */
/* ==================================================================== */
/************************************************************************/

class GTiffBitmapBand /* non final */ : public GTiffOddBitsBand
{
    friend class GTiffDataset;

    GDALColorTable *m_poColorTable = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GTiffBitmapBand)

  public:
    GTiffBitmapBand(GTiffDataset *, int);
    ~GTiffBitmapBand() override;

    GDALColorInterp GetColorInterpretation() override;
    CPLErr SetColorInterpretation(GDALColorInterp eInterp) override;
    GDALColorTable *GetColorTable() override;
};

#endif  // GTIFFBITMAPBAND_H_INCLUDED
