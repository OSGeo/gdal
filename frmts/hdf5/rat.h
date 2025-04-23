/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  RAT utility
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef RAT_H_INCLUDED
#define RAT_H_INCLUDED

#include "gdal_rat.h"
#include "gdal_priv.h"

#include <memory>

std::unique_ptr<GDALRasterAttributeTable>
HDF5CreateRAT(const std::shared_ptr<GDALMDArray> &poValues,
              bool bFirstColIsMinMax);

#endif  // RAT_H_INCLUDED
