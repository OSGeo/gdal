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
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <vector>
#include <algorithm>

CPL_CVSID("$Id$")

static int matrixInvert( int N, const double input[], double output[] );

/************************************************************************/
/*                       GDALLinearSystemSolve()                        */
/*                                                                      */
/*   Solves the linear system adfA*adfOut = adfRHS for adfOut, where    */
/*   adfA is a square matrix.  The matrices are given as flat 1D        */
/*   arrays with the entries in row-major order.                        */
/*   nDim is the number of rows and columns in adfA, and nRHS is the    */
/*   number of right-hand sides (columns) in adfRHS.                    */
/************************************************************************/

bool GDALLinearSystemSolve( const int nDim, const int nRHS,
    const double adfA[], const double adfRHS[], double adfOut[] )
{
#ifdef HAVE_ARMADILLO
    try
    {
        arma::mat matA( const_cast<double*>( adfA ), nDim, nDim, false );
        arma::inplace_trans( matA );
        arma::mat matRHS( const_cast<double*>( adfRHS ), nRHS, nDim, false );
        arma::inplace_trans( matRHS );

        arma::mat matOut;
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#if ARMA_VERSION_MAJOR > 6 || (ARMA_VERSION_MAJOR == 6 && ARMA_VERSION_MINOR >= 500 )
        // Perhaps available in earlier versions, but didn't check
        if( arma::solve( matOut, matA, matRHS, arma::solve_opts::equilibrate +
            arma::solve_opts::no_approx ) )
#else
        if( arma::solve( matOut, matA, matRHS ) )
#endif

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
#pragma GCC diagnostic pop
#endif
        {
            for( int iEq = 0; iEq < nDim; iEq++ )
                for( int iRHS = 0; iRHS < nRHS; iRHS++ )
                    adfOut[iEq * nRHS + iRHS] = matOut.at(iEq, iRHS);

            return true;
        }
    }
    catch(...) {}

#endif
    double* adfAInverse = new double[nDim * nDim];

    if( !matrixInvert( nDim, adfA, adfAInverse ) )
    {
        // I guess adfA is singular
        delete[] adfAInverse;
        return false;
    }

    // calculate the coefficients
    for( int iRHS = 0; iRHS < nRHS; iRHS++ )
    {
        for( int iEq = 0; iEq < nDim; iEq++ )
        {
            adfOut[iEq * nRHS + iRHS] = 0.0;
            for( int iVar = 0; iVar < nDim; iVar++ )
            {
                adfOut[iEq * nRHS + iRHS] +=
                    adfAInverse[iEq * nDim + iVar] *
                    adfRHS[iVar * nRHS + iRHS];
            }
        }
    }

    delete[] adfAInverse;
    return true;
}

static int matrixInvert( int N, const double input[], double output[] )
{
    // Receives an array of dimension NxN as input.  This is passed as a one-
    // dimensional array of N-squared size.  It produces the inverse of the
    // input matrix, returned as output, also of size N-squared.  The Gauss-
    // Jordan Elimination method is used.  (Adapted from a BASIC routine in
    // "Basic Scientific Subroutines Vol. 1", courtesy of Scott Edwards.)

    // Array elements 0...N-1 are for the first row, N...2N-1 are for the
    // second row, etc.

    // We need to have a temporary array of size N x 2N.  We'll refer to the
    // "left" and "right" halves of this array.

#if DEBUG_VERBOSE
    fprintf(stderr, "Matrix Inversion input matrix (N=%d)\n", N);/*ok*/
    for( int row = 0; row < N; row++ )
    {
        for( int col = 0; col < N; col++ )
        {
            fprintf(stderr, "%5.2f ", input[row*N + col]);/*ok*/
        }
        fprintf(stderr, "\n");/*ok*/
    }
#endif

    const int tempSize = 2 * N * N;
    double* temp = new double[tempSize];

    if( temp == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "matrixInvert(): ERROR - memory allocation failed.");
        return false;
    }

    // First create a double-width matrix with the input array on the left
    // and the identity matrix on the right.

    for( int row = 0; row < N; row++ )
    {
        for( int col = 0; col<N; col++ )
        {
            // Our index into the temp array is X2 because it's twice as wide
            // as the input matrix.

            temp[ 2*row*N + col ] = input[ row*N+col ];  // left = input matrix
            temp[ 2*row*N + col + N ] = 0.0;            // right = 0
        }
        temp[ 2*row*N + row + N ] = 1.0;  // 1 on the diagonal of RHS
    }

    // Now perform row-oriented operations to convert the left hand side
    // of temp to the identity matrix.  The inverse of input will then be
    // on the right.

    int max = 0;
    int k = 0;
    for( k = 0; k < N; k++ )
    {
        if( k + 1 < N )  // If not on the last row.
        {
            max = k;
            for( int row = k + 1; row < N; row++ )  // Find the maximum element.
            {
                if( fabs( temp[row*2*N + k] ) > fabs( temp[max*2*N + k] ) )
                {
                    max = row;
                }
            }

            if( max != k )  // Swap all the elements in the two rows.
            {
                for( int col = k; col < 2 * N; col++ )
                {
                    std::swap(temp[k*2*N + col], temp[max*2*N + col]);
                }
            }
        }

        const double ftemp = temp[k*2*N + k];
        if( ftemp == 0.0 )  // Matrix cannot be inverted.
        {
            delete[] temp;
            return false;
        }

        for( int col = k; col < 2 * N; col++ )
        {
            temp[k*2*N + col] /= ftemp;
        }

        const int i2 = k * 2 * N;
        for( int row = 0; row < N; row++ )
        {
            if( row != k )
            {
                const int i1 = row * 2 * N;
                const double ftemp2 = temp[ i1 + k ];
                for( int col = k; col < 2*N; col++ )
                {
                    temp[i1 + col] -= ftemp2 * temp[i2 + col];
                }
            }
        }
    }

    // Retrieve inverse from the right side of temp.
    for( int row = 0; row < N; row++ )
    {
        for( int col = 0; col < N; col++ )
        {
            output[row*N + col] = temp[row*2*N + col + N ];
        }
    }
    delete [] temp;

#if DEBUG_VERBOSE
    fprintf(stderr, "Matrix Inversion result matrix:\n");/*ok*/
    for( int row = 0; row < N; row++ )
    {
        for( int col = 0; col < N; col++ )
        {
            fprintf(stderr, "%5.2f ", output[row*N + col]);/*ok*/
        }
        fprintf(stderr, "\n");/*ok*/
    }
#endif

    return true;
}

/*! @endcond */
