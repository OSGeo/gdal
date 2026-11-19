/**********************************************************************
 *
 * Name:     cpl_int_wrapper.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Strongly typed wrapper around int
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_INT_WRAPPER_H_INCLUDED
#define CPL_INT_WRAPPER_H_INCLUDED

namespace cpl
{
/** Strongly typed wrapper around an integer data type.
 *
 * Template parameter TagType is typically a tag structure (``struct MyTag{};``)
 */
template <class TagType, typename IntType = int> class IntWrapper
{
    IntType m_value;

  public:
    /** Constructor */
    // cppcheck-suppress noExplicitConstructor
    inline constexpr IntWrapper(IntType v) : m_value(v)
    {
    }

    /** Cast to integer */
    inline constexpr explicit operator IntType() const
    {
        return m_value;
    }

    /*! @cond Doxygen_Suppress */

    inline IntWrapper operator+(IntType value) const
    {
        return m_value + value;
    }

    inline IntWrapper &operator++()  // prefix ++x
    {
        ++m_value;
        return *this;
    }

#define COMPARISON_OPERATOR(op)                                                \
    inline bool operator op(const IntWrapper &other) const                     \
    {                                                                          \
        return m_value op other.m_value;                                       \
    }
    COMPARISON_OPERATOR(==)
    COMPARISON_OPERATOR(!=)
    COMPARISON_OPERATOR(<)
    COMPARISON_OPERATOR(<=)
    COMPARISON_OPERATOR(>)
    COMPARISON_OPERATOR(>=)
#undef COMPARISON_OPERATOR

#define COMPARISON_OPERATOR(op)                                                \
    inline bool operator op(IntType value) const                               \
    {                                                                          \
        return m_value op value;                                               \
    }
    COMPARISON_OPERATOR(==)
    COMPARISON_OPERATOR(!=)
    COMPARISON_OPERATOR(<)
    COMPARISON_OPERATOR(<=)
    COMPARISON_OPERATOR(>)
    COMPARISON_OPERATOR(>=)
#undef COMPARISON_OPERATOR
    /*! @endcond */
};

}  // namespace cpl

#endif  // CPL_INT_WRAPPER_H_INCLUDED
