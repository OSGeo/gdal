/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Linear system solver
 * Author:   VIZRT Development Team.
 *
 ******************************************************************************
 * Copyright (c) 2017 Alan Thomas <alant@outlook.com.au>
 *
 * SPDX-License-Identifier: MIT
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
    GDALMatrix(int rows, int cols)
        : n_rows(rows), n_cols(cols), v(rows * cols, 0.)
    {
    }

    /// Returns the number or rows of the matrix
    inline int getNumRows() const
    {
        return n_rows;
    }

    /// Returns the number or columns of the matrix.
    inline int getNumCols() const
    {
        return n_cols;
    }

    /// Returns the reference to the element at the position \a row, \a col.
    inline double &operator()(int row, int col)
    {
        return v[row + col * static_cast<size_t>(n_rows)];
    }

    /// Returns the element at the position \a row, \a col by value.
    inline double operator()(int row, int col) const
    {
        return v[row + col * static_cast<size_t>(n_rows)];
    }

    /// Returns the values of the matrix in column major order.
    double const *data() const
    {
        return v.data();
    }

    /// Returns the values of the matrix in column major order.
    double *data()
    {
        return v.data();
    }

    /// Resizes the matrix. All values are set to zero.
    void resize(int iRows, int iCols)
    {
        n_rows = iRows;
        n_cols = iCols;
        v.clear();
        v.resize(static_cast<size_t>(iRows) * iCols);
    }

  private:
    int n_rows = 0;
    int n_cols = 0;
    std::vector<double> v;
};

bool GDALLinearSystemSolve(GDALMatrix &A, GDALMatrix &RHS, GDALMatrix &X);

#endif /* #ifndef GDALLINEARSYSTEM_H_INCLUDED */

/*! @endcond */
