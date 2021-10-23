/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Linear system solver
 * Author:   VIZRT Development Team.
 *
 ******************************************************************************
 * Copyright (c) 2017 Alan Thomas <alant@outlook.com.au>
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

#ifndef GDALLINEARSYSTEM_H_INCLUDED
#define GDALLINEARSYSTEM_H_INCLUDED

#include <vector>
/*
* Matrix class with double entries.
* The elements are stored in column major order in a vector.
*/
struct GDALMatrix
{
    /// Creates a matrix with zero rows and columns.
    GDALMatrix() = default;
    /// Creates a matrix with \a rows rows and \a col columns
    /// Its elements are initialized to 0.
    GDALMatrix(int rows, int cols) : n_rows(rows), n_cols(cols), v(rows * cols, 0.) {}
    /// Returns the number or rows of the matrix
    inline int getNumRows() const { return n_rows; }
    /// Returns the number or columns of the matrix.
    inline int getNumCols() const { return n_cols; }
    /// Returns the reference to the element at the position \a row, \a col.
    inline double & operator()(int row, int col) { return v[row + col * n_rows]; }
    /// Returns the element at the position \a row, \a col by value.
    inline double operator()(int row, int col) const { return  v[row + col * n_rows]; }
    /// Returns the values of the matrix in column major order.
    double const * data() const { return v.data(); }
    /// Returns the values of the matrix in column major order.
    double * data() { return v.data(); }
    /// Resizes the matrix. All values are set to zero.
    void resize(int iRows, int iCols)
    {
        n_rows = iRows;
        n_cols = iCols;
        v.clear();
        v.resize(iRows * iCols);
    }
private:
    int n_rows = 0;
    int n_cols = 0;
    std::vector<double> v;
};

bool GDALLinearSystemSolve( GDALMatrix & A, GDALMatrix & RHS, GDALMatrix & X );


#endif /* #ifndef GDALLINEARSYSTEM_H_INCLUDED */

/*! @endcond */
