/******************************************************************************
 *
 * Project:  COG Driver
 * Purpose:  Cloud optimized GeoTIFF write support.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef COGDRIVER_H_INCLUDED
#define COGDRIVER_H_INCLUDED

#include "gdal_priv.h"
#include "cpl_string.h"

bool COGHasWarpingOptions(CSLConstList papszOptions);

bool COGGetTargetSRS(const char *const *papszOptions, CPLString &osTargetSRS);

std::string COGGetResampling(GDALDataset *poSrcDS,
                             const char *const *papszOptions);

bool COGGetWarpingCharacteristics(GDALDataset *poSrcDS,
                                  const char *const *papszOptions,
                                  CPLString &osResampling,
                                  CPLString &osTargetSRS, int &nXSize,
                                  int &nYSize, double &dfMinX, double &dfMinY,
                                  double &dfMaxX, double &dfMaxY);
void COGRemoveWarpingOptions(CPLStringList &aosOptions);

#endif  // COGDRIVER_H_INCLUDED
