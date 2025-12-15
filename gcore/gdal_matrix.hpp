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

#ifndef GDAL_MATRIX_HPP
#define GDAL_MATRIX_HPP

#include "cpl_port.h"
#include <algorithm>

/************************************************************************/
/*               GDALMatrixMultiplyAByTransposeAUpperTriangle()         */
/************************************************************************/

// Compute res = A * A.transpose(), by filling only the upper triangle.
// Do that in a cache friendly way.
// We accumulate values into the output array, so generally the caller must
// have make sure to zero-initialize it before (unless they want to add into it)
template <class T>
// CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW because it seems the uses of openmp-simd
// causes that to happen
static CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW void
GDALMatrixMultiplyAByTransposeAUpperTriangle([[maybe_unused]] int nNumThreads,
                                             const T *A,  // rows * cols
                                             T *res,      // rows * rows
                                             int rows, size_t cols)
{
    constexpr int BLOCK_SIZE = 64;
    constexpr size_t BLOCK_SIZE_COLS = 256;
    constexpr T ZERO{0};

#ifdef HAVE_OPENMP
#pragma omp parallel for schedule(static) num_threads(nNumThreads)
#endif
    for (int64_t ii64 = 0; ii64 < rows; ii64 += BLOCK_SIZE)
    {
        const int ii = static_cast<int>(ii64);
        const int i_end = ii + std::min(BLOCK_SIZE, rows - ii);
        for (int jj = ii; jj < rows;
             jj += std::min(BLOCK_SIZE, rows - jj))  // upper triangle
        {
            const int j_end = jj + std::min(BLOCK_SIZE, rows - jj);
            for (size_t cc = 0; cc < cols; /* increment done at end of loop */)
            {
                const size_t c_end = cc + std::min(BLOCK_SIZE_COLS, cols - cc);

                for (int i = ii; i < i_end; ++i)
                {
                    const T *const Ai = &A[i * cols];
                    int j = std::max(i, jj);
                    for (; j < j_end - 7; j += 8)
                    {
                        const T *const Ajp0 = A + (j + 0) * cols;
                        const T *const Ajp1 = A + (j + 1) * cols;
                        const T *const Ajp2 = A + (j + 2) * cols;
                        const T *const Ajp3 = A + (j + 3) * cols;
                        const T *const Ajp4 = A + (j + 4) * cols;
                        const T *const Ajp5 = A + (j + 5) * cols;
                        const T *const Ajp6 = A + (j + 6) * cols;
                        const T *const Ajp7 = A + (j + 7) * cols;

                        T dfSum0 = ZERO, dfSum1 = ZERO, dfSum2 = ZERO,
                          dfSum3 = ZERO, dfSum4 = ZERO, dfSum5 = ZERO,
                          dfSum6 = ZERO, dfSum7 = ZERO;
#ifdef HAVE_OPENMP_SIMD
#pragma omp simd reduction(+ : dfSum0, dfSum1, dfSum2, dfSum3, dfSum4, dfSum5, dfSum6, dfSum7)
#endif
                        for (size_t c = cc; c < c_end; ++c)
                        {
                            dfSum0 += Ai[c] * Ajp0[c];
                            dfSum1 += Ai[c] * Ajp1[c];
                            dfSum2 += Ai[c] * Ajp2[c];
                            dfSum3 += Ai[c] * Ajp3[c];
                            dfSum4 += Ai[c] * Ajp4[c];
                            dfSum5 += Ai[c] * Ajp5[c];
                            dfSum6 += Ai[c] * Ajp6[c];
                            dfSum7 += Ai[c] * Ajp7[c];
                        }

                        const auto nResOffset =
                            static_cast<size_t>(i) * rows + j;
                        res[nResOffset + 0] += dfSum0;
                        res[nResOffset + 1] += dfSum1;
                        res[nResOffset + 2] += dfSum2;
                        res[nResOffset + 3] += dfSum3;
                        res[nResOffset + 4] += dfSum4;
                        res[nResOffset + 5] += dfSum5;
                        res[nResOffset + 6] += dfSum6;
                        res[nResOffset + 7] += dfSum7;
                    }
                    for (; j < j_end; ++j)
                    {
                        const T *const Aj = A + j * cols;

                        T dfSum = ZERO;
#ifdef HAVE_OPENMP_SIMD
#pragma omp simd reduction(+ : dfSum)
#endif
                        for (size_t c = cc; c < c_end; ++c)
                        {
                            dfSum += Ai[c] * Aj[c];
                        }

                        res[static_cast<size_t>(i) * rows + j] += dfSum;
                    }
                }

                cc = c_end;
            }
        }
    }
}

#endif
