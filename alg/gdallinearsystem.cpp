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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

/*! @cond Doxygen_Suppress */

#if defined(HAVE_ARMADILLO) && !defined(DO_NOT_USE_DEBUG_BOOL)
#define DO_NOT_USE_DEBUG_BOOL
#endif

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

CPL_CVSID("$Id$")

#ifndef HAVE_ARMADILLO
namespace
{
    // LU decomposition of the quadratic matrix A
    // see https://en.wikipedia.org/wiki/LU_decomposition#C_code_examples
    bool solve( GDALMatrix & A, GDALMatrix & RHS, GDALMatrix & X, double eps )
    {
        assert(A.getNumRows() == A.getNumCols());
        if(eps < 0) return false;
        int const m = A.getNumRows();
        int const n = RHS.getNumCols();
        // row permutations
        std::vector<int> perm(m);
        for(int iRow = 0; iRow < m; ++iRow)
            perm[iRow] = iRow;

        for(int step = 0; step < m - 1; ++step)
        {
            // determine pivot element
            int iMax = step;
            double dMax = std::abs(A(step, step));
            for(int i = step + 1; i < m; ++i)
            {
                if(std::abs(A(i, step)) > dMax)
                {
                    iMax = i;
                    dMax = std::abs(A(i, step));
                }
            }
            if(dMax <= eps)
            {
                CPLError( CE_Failure, CPLE_AppDefined, "GDALLinearSystemSolve: matrix not invertible" );
                return false;
            }
            // swap rows
            if(iMax != step)
            {
                std::swap(perm[iMax], perm[step]);
                for(int iCol = 0; iCol < m; ++iCol)
                {
                    std::swap(A(iMax, iCol), A(step, iCol));
                }
            }
            for(int iRow = step + 1; iRow < m; ++iRow)
            {
                A(iRow, step) /= A(step, step);
            }
            for(int iCol = step + 1; iCol < m; ++iCol)
            {
                for(int iRow = step + 1; iRow < m; ++iRow)
                {
                    A(iRow, iCol) -= A(iRow, step) * A(step, iCol);
                }
            }
        }

        // LUP solve;
        for(int iCol = 0; iCol < n; ++iCol)
        {
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
        return true;
    }
}
#endif
/************************************************************************/
/*                       GDALLinearSystemSolve()                        */
/*                                                                      */
/*   Solves the linear system A*X_i = RHS_i for each column i           */
/*   where A is a square matrix.                                        */
/************************************************************************/
bool GDALLinearSystemSolve( GDALMatrix  & A, GDALMatrix  & RHS, GDALMatrix & X )
{
    assert(A.getNumRows() == RHS.getNumRows());
    assert(A.getNumCols() == X.getNumRows());
    assert(RHS.getNumCols() == X.getNumCols());
    try
    {
#ifdef HAVE_ARMADILLO
        arma::mat matA( A.data(), A.getNumRows(), A.getNumCols(), false, true );
        arma::mat matRHS(RHS.data(), RHS.getNumRows(), RHS.getNumCols(), false, true );
        arma::mat matOut(X.data(), X.getNumRows(), X.getNumCols(), false, true);
#if ARMA_VERSION_MAJOR > 6 || (ARMA_VERSION_MAJOR == 6 && ARMA_VERSION_MINOR >= 500 )
        // Perhaps available in earlier versions, but didn't check
        return arma::solve( matOut, matA, matRHS,
          arma::solve_opts::equilibrate + arma::solve_opts::no_approx);
#else
        return arma::solve( matOut, matA, matRHS );
#endif

#else //HAVE_ARMADILLO
        return solve(A, RHS, X, 0);
#endif
    }
    catch(std::exception const & e) {
        CPLError( CE_Failure, CPLE_AppDefined,
        "GDALLinearSystemSolve: %s", e.what() );
        return false;
    }
}

/*! @endcond */
