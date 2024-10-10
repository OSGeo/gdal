/******************************************************************************
 * Project:  GDAL Point abstraction
 * Purpose:
 * Author:   Javier Jimenez Shaw
 *
 ******************************************************************************
 * Copyright (c) 2024, Javier Jimenez Shaw
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

#ifndef GDAL_POINT_TEMPL_H_INCLUDED
#define GDAL_POINT_TEMPL_H_INCLUDED

/*! @cond Doxygen_Suppress */

#include <array>
#include <functional>

namespace gdal
{
template <typename T, std::size_t N> class RawPoint
{
  public:
    using value_type = T;
    using size_type = std::size_t;
    using self_type = RawPoint<T, N>;

    static constexpr size_type size() noexcept
    {
        return N;
    }

    RawPoint()
    {
    }

    template <typename... Args> RawPoint(Args... args) : _values({args...})
    {
        static_assert(N == sizeof...(Args),
                      "Invalid number of constructor params");
    }

    const std::array<T, N> &array() const
    {
        return _values;
    }

    T &operator[](size_type pos)
    {
        return _values[pos];
    }

    const T &operator[](size_type pos) const
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

    T scalarProd(const self_type &arg) const
    {
        T accum{};
        for (size_t i = 0; i < N; i++)
            accum += _values[i] * arg[i];
        return accum;
    }

    T norm2() const
    {
        return scalarProd(*this);
    }

    template <typename U> RawPoint<U, N> cast() const
    {
        RawPoint<U, N> res;
        for (size_t i = 0; i < N; i++)
            res[i] = static_cast<U>(_values[i]);
        return res;
    }

    template <class UnaryOp> self_type apply(UnaryOp op) const
    {
        self_type res;
        for (size_t i = 0; i < N; i++)
            res[i] = op(_values[i]);
        return res;
    }

    self_type floor() const
    {
        self_type res;
        for (size_t i = 0; i < N; i++)
            res[i] = std::floor(_values[i]);
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
        self_type res;
        for (size_t i = 0; i < N; i++)
            res[i] = -_values[i];
        return res;
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

    friend RawPoint<T, N> operator-(T t, RawPoint<T, N> arg)
    {
        RawPoint<T, N> res;
        for (size_t i = 0; i < N; i++)
            res._values[i] = t - arg._values[i];
        return res;
    }

    template <typename U>
    friend RawPoint<T, N> operator/(T t, RawPoint<T, N> arg)
    {
        RawPoint<T, N> res;
        for (size_t i = 0; i < N; i++)
            res._values[i] = t / arg._values[i];
        return res;
    }

  private:
    std::array<T, N> _values{};

    template <class Op> RawPoint<T, N> operatorImpl(T arg, Op op) const
    {
        RawPoint<T, N> res;
        for (size_t i = 0; i < N; i++)
            res[i] = op(_values[i], arg);
        return res;
    }

    template <class Op> RawPoint<T, N> &operatorEqImpl(T arg, Op op)
    {
        for (size_t i = 0; i < N; i++)
            _values[i] = op(_values[i], arg);
        return *this;
    }

    template <class Op>
    RawPoint<T, N> operatorImpl(const RawPoint<T, N> &arg, Op op) const
    {
        RawPoint<T, N> res;
        for (size_t i = 0; i < N; i++)
            res[i] = op(_values[i], arg[i]);
        return res;
    }

    template <class Op>
    RawPoint<T, N> &operatorEqImpl(const RawPoint<T, N> &arg, Op op)
    {
        for (size_t i = 0; i < N; i++)
            _values[i] = op(_values[i], arg[i]);
        return *this;
    }
};

using RawPoint2d = RawPoint<double, 2>;
using RawPoint2i = RawPoint<int, 2>;
using RawPoint3d = RawPoint<double, 3>;
using RawPoint3i = RawPoint<int, 3>;
}  // namespace gdal

/*! @endcond */

#endif /* ndef GDAL_POINT_TEMPL_H_INCLUDED */
