/*
 * m3d.h
 *
 *  Created on: Jul 2, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_M3D_H_
#define FRMTS_HDF5R_M3D_H_

#include <cmath>
#include <sstream>

/**
 * @brief Math 3D namespace
 * The Math-3D name space provides basic real Euclidean three dimensional
 * vector and matrix containers and associated operations.
 */
namespace m3d
{
class Matrix; // forward declaration

/**
 * @brief Three dimensional vector container and operators.
 * This class provides a container for 3D vectors. It is built for speed: all
 * calls are in-line and iterative loops are avoided. An associated (friend)
 * Matrix class provides for 3D matrix-vector operations.
 */
class Vector
{
public:
    /**
     * Default constructor initializes to the zero vector.
     */
    Vector() : v_{ 0.0 } {}

    /**
     * Constructor to initialize from three doubles.
     * @param x X component.
     * @param y Y component.
     * @param z Z component.
     */
    Vector( double x, double y, double z ) : v_{ x, y, z } {}

    /**
     * Constructor from a C three element array.
     * @param v Pointer to a three element C array (unchecked).
     */
    Vector( const double v[3] ) : v_{v[0], v[1], v[2]} {}

    /**
     * Access the coefficient of the X-axis unit vector, i.
     * @return The coefficient of the X-axis unit vector, i
     */
    double i() const {return v_[0];}

    /**
     * Access the coefficient of the Y-axis unit vector, j.
     * @return The coefficient of the Y-axis unit vector, j
     */
    double j() const {return v_[1];}

    /**
     * Access the coefficient of the Z-axis unit vector, k.
     * @return The coefficient of the Z-axis unit vector, k
     */
    double k() const {return v_[2];}

    /**
     * Compute and return the sum of the squares of the vector components.
     * @return Sum of the components squared.
     */
    double sumsq() const
    {
        return (v_[0] * v_[0]) + (v_[1] * v_[1]) + (v_[2] * v_[2]);
    }

    /**
     * Compute the magnitude of the vector as the square root of
     * the sum of the components squared.
     * @return Vector magnitude.
     */
    double magnitude() const
    {
        return std::sqrt( sumsq() );
    }

    /**
     * Normalize the vector in-place by dividing each component by the vector
     * magnitude.
     * @return A reference to the this class that has just been normalized.
     */
    Vector& normalize()
    {
        double mag = magnitude();
        if (mag > 0.0)
        {
            v_[0] /= mag;
            v_[1] /= mag;
            v_[2] /= mag;
        }
        return *this;
    }

    /**
     * Return a normalized copy of this m3dVector
     * @return a normalized copy of this m3dVector
     */
    Vector getUnitVector() const
    {
        return (*this)/magnitude();
    }

    /**
     * Vector unary addition, element-by-element, to this vector
     * @param W vector to add
     * @return  V += W
     */
    Vector& operator+=( const Vector& W )
    {
        v_[0] += W.v_[0];
        v_[1] += W.v_[1];
        v_[2] += W.v_[2];
        return *this;
    }

    /**
     * Vector unary subtraction, element-by-element, from this vector
     * @param W vector to subtract
     * @return  V -= W
     */
    Vector& operator-=( const Vector& W )
    {
        v_[0] -= W.v_[0];
        v_[1] -= W.v_[1];
        v_[2] -= W.v_[2];
        return *this;
    }

    /**
     * Scalar unary multiplication times each element
     * @param a Value to multiply each element by
     * @return  V *= a
     */
    Vector& operator*=( double a )
    {
        v_[0] *= a;
        v_[1] *= a;
        v_[2] *= a;
        return *this;
    }

    /**
     * Scalar unary addition to each element
     * @param a Value to add to each element
     * @return  V += a
     */
    Vector& operator+=( double a )
    {
        v_[0] += a;
        v_[1] += a;
        v_[2] += a;
        return *this;
    }

    /**
     * Scalar unary subtraction from each element
     * @param a Value to subtract from each element
     * @return  V -= a
     */
    Vector& operator-=( double a )
    {
        v_[0] -= a;
        v_[1] -= a;
        v_[2] -= a;
        return *this;
    }

    /**
     * Scalar unary division into each element
     * @param a Value to divide each element by
     * @return  V /= a
     */
    Vector& operator/=( double a )
    {
        v_[0] /= a;
        v_[1] /= a;
        v_[2] /= a;
        return *this;
    }

    /**
     * A friend operator to return the sum of two three element vectors.
     * @param V lhs
     * @param W rhs
     * @return V + W
     */
    friend Vector operator+( Vector V, const Vector& W )
    {
        V += W;
        return V;
    }

    /**
     * A friend operator to return the difference of two three element vectors.
     * @param V lhs
     * @param W rhs
     * @return V - W
     */
    friend Vector operator-( Vector V, const Vector& W )
    {
        V -= W;
        return V;
    }

    /**
     * Sign inversion operator returns the negative of the vector.
     * @param V Input vector to negate.
     * @return -V (a negated copy).
     */
    friend Vector operator-( const Vector& V )
    {
        return Vector( -V.i(), -V.j(), -V.k() );
    }

    /**
     * Vector outer or cross product of two vectors
     * @param V lhs
     * @param W rhs
     * @return  V x W
     */
    friend Vector crossprod( const Vector& V, const Vector& W )
    {
        return Vector( V.v_[1] * W.v_[2] - V.v_[2] * W.v_[1],
                       V.v_[2] * W.v_[0] - V.v_[0] * W.v_[2],
                       V.v_[0] * W.v_[1] - V.v_[1] * W.v_[0] );
    }

    /**
     * Vector inner or dot product of two vectors. Which is equivalent to
     * a matrix multiply of a 1x3 row vector times a 3x1 column vector.
     * @param V lhs
     * @param W rhs
     * @return V dot W (a scalar)
     */
    friend double operator*( const Vector& V, const Vector& W )
    {
        return V.v_[0] * W.v_[0] + V.v_[1] * W.v_[1] + V.v_[2] * W.v_[2];
    }

    /**
     * A friend operator to return the result of a vector multiplied by a scalar.
     * @param a lhs
     * @param V rhs
     * @return a*V
     */
    friend Vector operator*( double a, Vector V )
    {
        V *= a;
        return V;
    }

    /**
     * A friend operator to return the result of a vector divided by a scalar.
     * @param a rhs
     * @param V lhs
     * @return V/a
     */
    friend Vector operator/( Vector V, double a )
    {
        V /= a;
        return V;
    }

    /**
     * A friend operator to return the result of a scalar added to each element
     * of a vector.
     * @param a lhs
     * @param V rhs
     * @return a+V
     */
    friend Vector operator+( double a, Vector V )
    {
        V += a;
        return V;
    }

    /**
     * A friend operator to return the result of a scalar subtracted from each
     * element of a vector.
     * @param a rhs
     * @param V lhs
     * @return V-a
     */
    friend Vector operator-( Vector V, double a )
    {
        V -= a;
        return V;
    }

    /**
     * A friend operator to multiply a 3D Matrix times a 3D Vector.
     * @param M lhs
     * @param V rhs
     * @return M*V (a three element vector)
     */
    friend Vector operator*( const Matrix& M, const Vector& V );

    /**
     * Convert the three element vector to a string.
     * @return std::string containing a space separated list of the vector
     *         contents.
     */
    std::string toString() const
    {
        std::ostringstream oss;
        oss <<  v_[0] <<  " " << v_[1] << " " << v_[2];
        return oss.str();
    }

private:
    double v_[3];

};

/**
 * Utility function to compute the sum of the squares of the m3d::Vector
 * elements.
 * @param V Three element vector.
 * @return Sum of the squares of the elements.
 */
inline double sumsq( const Vector& V )
{
    return V.sumsq();
}

/**
 * Utility function to compute the magnitude of a vector.
 * @param V Three element vector.
 * @return Magnitude (square root of the sum of the elements squared).
 */
inline double magnitude( const Vector& V )
{
    return V.magnitude();
}

/**
 * @brief Three dimensional matrix container and operations.
 */
class Matrix
{
public:
    /**
     * Default constructor initiales the matrix to all zeroes.
     */
    Matrix() : m3x3_{0.0} {}

    /**
     * Constructor from a 9 element C array -- not checked. Elements are loaded
     * in order of indexes as shown below:
     *        0   1   2
     *        3   4   5
     *        6   7   8
     * @param rowMajorArray9 9 element C array.
     */
    Matrix( const double* rowMajorArray9 ) : m3x3_{ *rowMajorArray9 }
    {}

    /**
     * Constructor from 9 doubles loaded into the array as follows:
     *      m00   m01   m02
     *      m10   m11   m12
     *      m20   m21   m22
     */
    Matrix( double m00, double m01, double m02,
            double m10, double m11, double m12,
            double m20, double m21, double m22 )
    : m3x3_{m00, m01, m02,
            m10, m11, m12,
            m20, m21, m22}
    {}

    /**
     * Constructor from 3 row vectors.
     * @param row0 Components are loaded into the matrix row 0.
     * @param row1 Components are loaded into the matrix row 1.
     * @param row2 Components are loaded into the matrix row 2.
     */
    Matrix( const Vector& row0,
            const Vector& row1,
            const Vector& row2 )
    : m3x3_{row0.i(), row0.j(), row0.k(),
            row1.i(), row1.j(), row1.k(),
            row2.i(), row2.j(), row2.k()}
    {}

    /**
     * This function operator allows read-only access of the matrix addressed by
     * (row, column).
     * @param row The row number starting at 0.
     * @param col The column number starting at 0.
     * @return The value of the (row,col) element.
     */
    double operator() ( unsigned row, unsigned col ) const
    {
        return m3x3_[3*row + col];
    }

    /**
     * This function operator allows read-write access of the matrix addressed by
     * (row, column).
     * @param row The row number starting at 0.
     * @param col The column number starting at 0.
     * @return A reference to the (row,col) element.
     */
    double& operator() ( unsigned row, unsigned col )
    {
        return m3x3_[3*row + col];
    }

    /**
     * Return a transposed copy of this matrix.
     * @return Transposed of this matrix.
     */
    Matrix transpose() const
    {
        return Matrix( (*this)(0, 0), (*this)(1, 0), (*this)(2, 0),
                       (*this)(0, 1), (*this)(1, 1), (*this)(2, 1),
                       (*this)(0, 2), (*this)(1, 2), (*this)(2, 2) );
    }

private:
    // the matrix in row major order
    // (elements of each row are contiguous in memory)
    double m3x3_[9];
};

/**
 * In-line definition of the Matrix times Vector method.
 * @param M lhs
 * @param V rhs
 * @return M*V
 */
inline Vector operator*( const Matrix& M, const Vector& V )
{
    Vector U( M(0,0) * V.i() + M(0,1) * V.j() + M(0,2) * V.k(),
              M(1,0) * V.i() + M(1,1) * V.j() + M(1,2) * V.k(),
              M(2,0) * V.i() + M(2,1) * V.j() + M(2,2) * V.k() );
    return U;
}

}  // namespace m3d
#endif /* FRMTS_HDF5R_M3D_H_ */
