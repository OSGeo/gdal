/******************************************************************************
 * Project:  GDAL Core
 * Purpose:  Utility functions for matrix multiplication
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_MATRIX_AVX2_FMA_HPP
#define GDAL_MATRIX_AVX2_FMA_HPP

#include "cpl_port.h"

void GDALMatrixMultiplyAByTransposeAUpperTriangle_AVX2_FMA(
    int nNumThreads,
    const double *A,  // rows * cols
    double *res,      // rows * rows
    int rows, size_t cols);

#endif
