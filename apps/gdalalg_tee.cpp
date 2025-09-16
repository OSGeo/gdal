/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "tee" pipeline step
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_tee.h"

//! @cond Doxygen_Suppress

GDALTeeStepAlgorithmAbstract::~GDALTeeStepAlgorithmAbstract() = default;

GDALTeeRasterAlgorithm::~GDALTeeRasterAlgorithm() = default;

GDALTeeVectorAlgorithm::~GDALTeeVectorAlgorithm() = default;

//! @endcond
