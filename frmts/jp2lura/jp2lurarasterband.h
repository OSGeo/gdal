/******************************************************************************
 * Project:  GDAL
 * Author:   Raul Alonso Reyes <raul dot alonsoreyes at satcen dot europa dot
 *eu> Author:   Even Rouault, <even dot rouault at spatialys dot com> Purpose:
 *JPEG-2000 driver based on Lurawave library, driver developed by SatCen
 *
 ******************************************************************************
 * Copyright (c) 2016, SatCen - European Union Satellite Centre
 * Copyright (c) 2016, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef JP2LURARASTERBAND_H_INCLUDED
#define JP2LURARASTERBAND_H_INCLUDED

#include "gdal_pam.h"

class JP2LuraDataset;

class JP2LuraRasterBand final : public GDALPamRasterBand
{
    friend class JP2LuraDataset;

  public:
    JP2LuraRasterBand(JP2LuraDataset *poDS, int nBand, GDALDataType eDataType,
                      int nBits, int nBlockXSize, int nBlockYSize);
    ~JP2LuraRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int iOvrLevel) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;
};

#endif
