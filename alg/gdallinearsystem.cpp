/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Linear system solver
 * Author:   VIZRT Development Team.
 *
 * This code was provided by Gilad Ronnen (gro at visrt dot com) with
 * permission to reuse under the following license.
 *
 ******************************************************************************
 * Copyright (c) 2004, VIZRT Inc.
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2019, Martin Franzke <martin dot franzke at telekom dot de>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

/*! @cond Doxygen_Suppress */

#include "cpl_port.h"
#include "cpl_conv.h"
#include "gdallinearsystem.h"

#ifdef HAVE_ARMADILLO
#include "armadillo_headers.h"
#endif

#include <cstdio>
#include <algorithm>
#include <cassert>
#include <cmath>

namespace
{
// LU decomposition of the quadratic matrix A
// see https://en.wikipedia.org/wiki/LU_decomposition#C_code_examples
bool solve(GDALMatrix &A, GDALMatrix &RHS, GDALMatrix &X, double eps)
{
    assert(A.getNumRows() == A.getNumCols());
    if (eps < 0)
        return false;
    int const m = A.getNumRows();
    int const n = RHS.getNumCols();
    // row permutations
    std::vector<int> perm(m);
    for (int iRow = 0; iRow < m; ++iRow)
        perm[iRow] = iRow;

    // Arbitrary threshold to trigger progress in debug mode
    const bool bDebug = (m > 10000);
    int nLastPct = -1;

    for (int step = 0; step < m - 1; ++step)
    {
        if (bDebug)
        {
            const int nPct = (step * 100 * 10 / m) / 2;
            if (nPct != nLastPct)
            {
                CPLDebug("GDAL", "solve(): %d.%d %%", nPct / 10, nPct % 10);
                nLastPct = nPct;
            }
        }

        // determine pivot element
        int iMax = step;
        double dMax = std::abs(A(step, step));
        for (int i = step + 1; i < m; ++i)
        {
            if (std::abs(A(i, step)) > dMax)
            {
                iMax = i;
                dMax = std::abs(A(i, step));
            }
        }
        if (dMax <= eps)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALLinearSystemSolve: matrix not invertible");
            return false;
        }
        // swap rows
        if (iMax != step)
        {
            std::swap(perm[iMax], perm[step]);
            for (int iCol = 0; iCol < m; ++iCol)
            {
                std::swap(A(iMax, iCol), A(step, iCol));
            }
        }
        for (int iRow = step + 1; iRow < m; ++iRow)
        {
            A(iRow, step) /= A(step, step);
        }
        for (int iCol = step + 1; iCol < m; ++iCol)
        {
            for (int iRow = step + 1; iRow < m; ++iRow)
            {
                A(iRow, iCol) -= A(iRow, step) * A(step, iCol);
            }
        }
    }

    // LUP solve;
    for (int iCol = 0; iCol < n; ++iCol)
    {
        if (bDebug)
        {
            const int nPct = 500 + (iCol * 100 * 10 / n) / 2;
            if (nPct != nLastPct)
            {
                CPLDebug("GDAL", "solve(): %d.%d %%", nPct / 10, nPct % 10);
                nLastPct = nPct;
            }
        }

        for (int iRow = 0; iRow < m; ++iRow)
        {
            X(iRow, iCol) = RHS(perm[iRow], iCol);
            for (int k = 0; k < iRow; ++k)
            {
                X(iRow, iCol) -= A(iRow, k) * X(k, iCol);
            }
        }
        for (int iRow = m - 1; iRow >= 0; --iRow)
        {
            for (int k = iRow + 1; k < m; ++k)
            {
                X(iRow, iCol) -= A(iRow, k) * X(k, iCol);
            }
            X(iRow, iCol) /= A(iRow, iRow);
        }
    }

    if (bDebug)
    {
        CPLDebug("GDAL", "solve(): 100.0 %%");
    }

    return true;
}
}  // namespace

/************************************************************************/
/*                       GDALLinearSystemSolve()                        */
/*                                                                      */
/*   Solves the linear system A*X_i = RHS_i for each column i           */
/*   where A is a square matrix.                                        */
/************************************************************************/
bool GDALLinearSystemSolve(GDALMatrix &A, GDALMatrix &RHS, GDALMatrix &X,
                           [[maybe_unused]] bool bForceBuiltinMethod)
{
    assert(A.getNumRows() == RHS.getNumRows());
    assert(A.getNumCols() == X.getNumRows());
    assert(RHS.getNumCols() == X.getNumCols());

#ifdef HAVE_ARMADILLO
    if (!bForceBuiltinMethod)
    {
        try
        {
            arma::mat matA(A.data(), A.getNumRows(), A.getNumCols(), false,
                           true);
            arma::mat matRHS(RHS.data(), RHS.getNumRows(), RHS.getNumCols(),
                             false, true);
            arma::mat matOut(X.data(), X.getNumRows(), X.getNumCols(), false,
                             true);
#if ARMA_VERSION_MAJOR > 6 ||                                                  \
    (ARMA_VERSION_MAJOR == 6 && ARMA_VERSION_MINOR >= 500)
            // Perhaps available in earlier versions, but didn't check
            return arma::solve(matOut, matA, matRHS,
                               arma::solve_opts::equilibrate +
                                   arma::solve_opts::no_approx);
#else
            return arma::solve(matOut, matA, matRHS);
#endif
        }
        catch (std::exception const &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALLinearSystemSolve: %s",
                     e.what());
            return false;
        }
    }
#endif  // HAVE_ARMADILLO

    return solve(A, RHS, X, 0);
}

/*! @endcond */
