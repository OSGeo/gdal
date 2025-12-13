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

#include "gdal_matrix_avx2_fma.h"

#define GDALMatrixMultiplyAByTransposeAUpperTriangle                           \
    GDALMatrixMultiplyAByTransposeAUpperTriangle_AVX2_FMA_internal
#include "gdal_matrix.hpp"

void GDALMatrixMultiplyAByTransposeAUpperTriangle_AVX2_FMA(
    int nNumThreads,
    const double *A,  // rows * cols
    double *res,      // rows * rows
    int rows, size_t cols)
{
    GDALMatrixMultiplyAByTransposeAUpperTriangle(nNumThreads, A, res, rows,
                                                 cols);
}
