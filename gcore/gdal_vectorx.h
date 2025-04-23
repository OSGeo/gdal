/******************************************************************************
 * Project:  GDAL Vector abstraction
 * Purpose:
 * Author:   Javier Jimenez Shaw
 *
 ******************************************************************************
 * Copyright (c) 2024, Javier Jimenez Shaw
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_VECTORX_H_INCLUDED
#define GDAL_VECTORX_H_INCLUDED

/*! @cond Doxygen_Suppress */

#include <algorithm>
#include <array>
#include <functional>

namespace gdal
{

/**
 * @brief Template class to abstract a vector
 *
 * Inspired by Eigen3 Vector class, but much simpler.
 * For GDAL internal use for now.
 *
 * @tparam T type of the values stored
 * @tparam N size of the container/vector
 */
template <typename T, std::size_t N> class VectorX
{
  public:
    using value_type = T;
    using size_type = std::size_t;
    using self_type = VectorX<T, N>;

    /** Size of the container */
    static constexpr size_type size() noexcept
    {
        return N;
    }

    VectorX()
    {
    }

    // cppcheck-suppress noExplicitConstructor
    template <typename... Args> VectorX(Args... args) : _values({args...})
    {
        static_assert(N == sizeof...(Args),
                      "Invalid number of constructor params");
    }

    /** Container as an std::array */
    const std::array<T, N> &array() const
    {
        return _values;
    }

    T &operator[](const size_type &pos)
    {
        return _values[pos];
    }

    const T &operator[](const size_type &pos) const
    {
        return _values[pos];
    }

    T x() const
    {
        static_assert(N >= 1, "Invalid template size for x()");
        return _values[0];
    }

    T y() const
    {
        static_assert(N >= 2, "Invalid template size for y()");
        return _values[1];
    }

    T z() const
    {
        static_assert(N >= 3, "Invalid template size for z()");
        return _values[2];
    }

    T &x()
    {
        static_assert(N >= 1, "Invalid template size for x()");
        return _values[0];
    }

    T &y()
    {
        static_assert(N >= 2, "Invalid template size for y()");
        return _values[1];
    }

    T &z()
    {
        static_assert(N >= 3, "Invalid template size for z()");
        return _values[2];
    }

    /** Fill all elements of the vector with the same value
     * @return this
     */
    self_type &fill(T arg)
    {
        for (size_t i = 0; i < N; i++)
            (*this)[i] = arg;

        return *this;
    }

    /** Apply the unary operator to all the elements
     * @param op unary operator to apply to every element
     * @return a new object with the computed values
     */
    template <class UnaryOp> self_type apply(UnaryOp op) const
    {
        self_type res;
        for (size_t i = 0; i < N; i++)
            res[i] = op(_values[i]);
        return res;
    }

    self_type floor() const
    {
        return apply([](const value_type &v) { return std::floor(v); });
    }

    self_type ceil() const
    {
        return apply([](const value_type &v) { return std::ceil(v); });
    }

    /** Compute the scalar product of two vectors */
    T scalarProd(const self_type &arg) const
    {
        T accum{};
        for (size_t i = 0; i < N; i++)
            accum += _values[i] * arg[i];
        return accum;
    }

    /** Compute the norm squared of the vector */
    T norm2() const
    {
        return scalarProd(*this);
    }

    /**
     * @brief cast the type to a different one, and convert the elements
     *
     * @tparam U output value type
     * @return VectorX<U, N> object of new type, where all elements are casted
     */
    template <typename U> VectorX<U, N> cast() const
    {
        VectorX<U, N> res;
        for (size_t i = 0; i < N; i++)
            res[i] = static_cast<U>(_values[i]);
        return res;
    }

    self_type operator+(T arg) const
    {
        return operatorImpl(arg, std::plus());
    }

    self_type &operator+=(T arg)
    {
        return operatorEqImpl(arg, std::plus());
    }

    self_type operator-(T arg) const
    {
        return operatorImpl(arg, std::minus());
    }

    self_type &operator-=(T arg)
    {
        return operatorEqImpl(arg, std::minus());
    }

    self_type operator-() const
    {
        return apply([](const value_type &v) { return -v; });
    }

    template <typename U> self_type operator*(U arg) const
    {
        self_type res;
        for (size_t i = 0; i < N; i++)
            res[i] = T(_values[i] * arg);
        return res;
    }

    self_type operator*(T arg) const
    {
        return operatorImpl(arg, std::multiplies());
    }

    template <typename U> self_type operator/(U arg) const
    {
        self_type res;
        for (size_t i = 0; i < N; i++)
            res[i] = T(_values[i] / arg);
        return res;
    }

    self_type operator/(T arg) const
    {
        return operatorImpl(arg, std::divides());
    }

    self_type operator+(const self_type &arg) const
    {
        return operatorImpl(arg, std::plus());
    }

    self_type operator-(const self_type &arg) const
    {
        return operatorImpl(arg, std::minus());
    }

    friend VectorX<T, N> operator+(T t, const VectorX<T, N> &arg)
    {
        VectorX<T, N> res;
        for (size_t i = 0; i < N; i++)
            res._values[i] = t + arg._values[i];
        return res;
    }

    friend VectorX<T, N> operator-(T t, const VectorX<T, N> &arg)
    {
        VectorX<T, N> res;
        for (size_t i = 0; i < N; i++)
            res._values[i] = t - arg._values[i];
        return res;
    }

  private:
    std::array<T, N> _values{};

    template <class Op> self_type operatorImpl(T arg, Op op) const
    {
        self_type res;
        for (size_t i = 0; i < N; i++)
            res[i] = op(_values[i], arg);
        return res;
    }

    template <class Op> self_type &operatorEqImpl(T arg, Op op)
    {
        for (size_t i = 0; i < N; i++)
            _values[i] = op(_values[i], arg);
        return *this;
    }

    template <class Op>
    self_type operatorImpl(const self_type &arg, Op op) const
    {
        self_type res;
        for (size_t i = 0; i < N; i++)
            res[i] = op(_values[i], arg[i]);
        return res;
    }

    template <class Op> self_type &operatorEqImpl(const self_type &arg, Op op)
    {
        for (size_t i = 0; i < N; i++)
            _values[i] = op(_values[i], arg[i]);
        return *this;
    }
};

using Vector2d = VectorX<double, 2>;
using Vector2i = VectorX<int, 2>;
using Vector3d = VectorX<double, 3>;
using Vector3i = VectorX<int, 3>;
}  // namespace gdal

/*! @endcond */

#endif /* ndef GDAL_VECTORX_H_INCLUDED */
