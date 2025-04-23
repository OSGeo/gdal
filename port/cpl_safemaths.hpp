/**********************************************************************
 *
 * Name:     cpl_safemaths.hpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Arithmetic overflow checking
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_SAFEMATHS_INCLUDED
#define CPL_SAFEMATHS_INCLUDED

#include <exception>
#include <limits>

#include <cstdint>

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if (__GNUC__ >= 5 && !defined(__INTEL_COMPILER)) ||                           \
    __has_builtin(__builtin_sadd_overflow)
#define BUILTIN_OVERFLOW_CHECK_AVAILABLE

#elif defined(_MSC_VER)

#include "safeint.h"

#endif

template <typename T> struct CPLSafeInt
{
    const T val;

    inline explicit CPLSafeInt(T valIn) : val(valIn)
    {
    }

    inline T v() const
    {
        return val;
    }
};

class CPLSafeIntOverflow : public std::exception
{
  public:
    inline CPLSafeIntOverflow()
    {
    }
};

class CPLSafeIntOverflowDivisionByZero : public CPLSafeIntOverflow
{
  public:
    inline CPLSafeIntOverflowDivisionByZero()
    {
    }
};

/** Convenience functions to build a CPLSafeInt */
inline CPLSafeInt<int> CPLSM(int x)
{
    return CPLSafeInt<int>(x);
}

inline CPLSafeInt<unsigned> CPLSM(unsigned x)
{
    return CPLSafeInt<unsigned>(x);
}

inline CPLSafeInt<unsigned> CPLSM_TO_UNSIGNED(int x)
{
    if (x < 0)
        throw CPLSafeIntOverflow();
    return CPLSafeInt<unsigned>(static_cast<unsigned>(x));
}

inline CPLSafeInt<int64_t> CPLSM(int64_t x)
{
    return CPLSafeInt<int64_t>(x);
}

inline CPLSafeInt<uint64_t> CPLSM(uint64_t x)
{
    return CPLSafeInt<uint64_t>(x);
}

// Unimplemented for now
inline CPLSafeInt<unsigned> CPLSM_TO_UNSIGNED(unsigned x);
inline CPLSafeInt<unsigned> CPLSM_TO_UNSIGNED(int64_t x);
inline CPLSafeInt<unsigned> CPLSM_TO_UNSIGNED(uint64_t x);

#if !defined(BUILTIN_OVERFLOW_CHECK_AVAILABLE) && defined(_MSC_VER)
class CPLMSVCSafeIntException : public msl::utilities::SafeIntException
{
  public:
    static void SafeIntOnOverflow()
    {
        throw CPLSafeIntOverflow();
    }

    static void SafeIntOnDivZero()
    {
        throw CPLSafeIntOverflowDivisionByZero();
    }
};
#endif

template <class T>
inline CPLSafeInt<T> SafeAddSigned(const CPLSafeInt<T> &A,
                                   const CPLSafeInt<T> &B)
{
    const auto a = A.v();
    const auto b = B.v();
    if (a > 0 && b > 0 && a > std::numeric_limits<T>::max() - b)
        throw CPLSafeIntOverflow();
    if (a < 0 && b < 0 && a < std::numeric_limits<T>::min() - b)
        throw CPLSafeIntOverflow();
    return CPLSM(a + b);
}

inline CPLSafeInt<int> operator+(const CPLSafeInt<int> &A,
                                 const CPLSafeInt<int> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    int res;
    if (__builtin_sadd_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<int>(A2 + B2));
#else
    const int a = A.v();
    const int b = B.v();
    const int64_t res = static_cast<int64_t>(a) + b;
    if (res < std::numeric_limits<int>::min() ||
        res > std::numeric_limits<int>::max())
    {
        throw CPLSafeIntOverflow();
    }
    return CPLSM(static_cast<int>(res));
#endif
}

#if defined(GDAL_COMPILATION)
inline CPLSafeInt<int64_t> operator+(const CPLSafeInt<int64_t> &A,
                                     const CPLSafeInt<int64_t> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    long long res;
    if (__builtin_saddll_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(static_cast<int64_t>(res));
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<int64_t, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<int64_t, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<int64_t>(A2 + B2));
#else
    return SafeAddSigned(A, B);
#endif
}
#endif  // GDAL_COMPILATION

inline CPLSafeInt<unsigned> operator+(const CPLSafeInt<unsigned> &A,
                                      const CPLSafeInt<unsigned> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    unsigned res;
    if (__builtin_uadd_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<unsigned>(A2 + B2));
#else
    const unsigned a = A.v();
    const unsigned b = B.v();
    if (a > std::numeric_limits<unsigned>::max() - b)
        throw CPLSafeIntOverflow();
    return CPLSM(a + b);
#endif
}

#if defined(GDAL_COMPILATION)
inline CPLSafeInt<uint64_t> operator+(const CPLSafeInt<uint64_t> &A,
                                      const CPLSafeInt<uint64_t> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    unsigned long long res;
    if (__builtin_uaddll_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(static_cast<uint64_t>(res));
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<uint64_t, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<uint64_t, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<uint64_t>(A2 + B2));
#else
    const uint64_t a = A.v();
    const uint64_t b = B.v();
    if (a > std::numeric_limits<uint64_t>::max() - b)
        throw CPLSafeIntOverflow();
    return CPLSM(a + b);
#endif
}
#endif  // GDAL_COMPILATION

template <class T>
inline CPLSafeInt<T> SafeSubSigned(const CPLSafeInt<T> &A,
                                   const CPLSafeInt<T> &B)
{
    const auto a = A.v();
    const auto b = B.v();
    /* caution we must catch a == 0 && b = INT_MIN */
    if (a >= 0 && b < 0 && a > std::numeric_limits<T>::max() + b)
        throw CPLSafeIntOverflow();
    if (a < 0 && b > 0 && a < std::numeric_limits<T>::min() + b)
        throw CPLSafeIntOverflow();
    return CPLSM(a - b);
}

inline CPLSafeInt<int> operator-(const CPLSafeInt<int> &A,
                                 const CPLSafeInt<int> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    int res;
    if (__builtin_ssub_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<int>(A2 - B2));
#else
    const int a = A.v();
    const int b = B.v();
    const int64_t res = static_cast<int64_t>(a) - b;
    if (res < std::numeric_limits<int>::min() ||
        res > std::numeric_limits<int>::max())
    {
        throw CPLSafeIntOverflow();
    }
    return CPLSM(static_cast<int>(res));
#endif
}

inline CPLSafeInt<int64_t> operator-(const CPLSafeInt<int64_t> &A,
                                     const CPLSafeInt<int64_t> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    long long res;
    if (__builtin_ssubll_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(static_cast<int64_t>(res));
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<int64_t, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<int64_t, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<int64_t>(A2 - B2));
#else
    return SafeSubSigned(A, B);
#endif
}

inline CPLSafeInt<unsigned> operator-(const CPLSafeInt<unsigned> &A,
                                      const CPLSafeInt<unsigned> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    unsigned res;
    if (__builtin_usub_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<unsigned>(A2 - B2));
#else
    const unsigned a = A.v();
    const unsigned b = B.v();
    if (a < b)
        throw CPLSafeIntOverflow();
    return CPLSM(a - b);
#endif
}

template <class T>
inline CPLSafeInt<T> SafeMulSigned(const CPLSafeInt<T> &A,
                                   const CPLSafeInt<T> &B)
{
    const auto a = A.v();
    const auto b = B.v();
    if (a > 0 && b > 0 && a > std::numeric_limits<T>::max() / b)
        throw CPLSafeIntOverflow();
    if (a > 0 && b < 0 && b < std::numeric_limits<T>::min() / a)
        throw CPLSafeIntOverflow();
    if (a < 0 && b > 0 && a < std::numeric_limits<T>::min() / b)
        throw CPLSafeIntOverflow();
    else if (a == std::numeric_limits<T>::min())
    {
        if (b != 0 && b != 1)
            throw CPLSafeIntOverflow();
    }
    else if (b == std::numeric_limits<T>::min())
    {
        if (a != 0 && a != 1)
            throw CPLSafeIntOverflow();
    }
    else if (a < 0 && b < 0 && -a > std::numeric_limits<T>::max() / (-b))
        throw CPLSafeIntOverflow();

    return CPLSM(a * b);
}

inline CPLSafeInt<int> operator*(const CPLSafeInt<int> &A,
                                 const CPLSafeInt<int> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    int res;
    if (__builtin_smul_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<int>(A2 * B2));
#else
    const int a = A.v();
    const int b = B.v();
    const int64_t res = static_cast<int64_t>(a) * b;
    if (res < std::numeric_limits<int>::min() ||
        res > std::numeric_limits<int>::max())
    {
        throw CPLSafeIntOverflow();
    }
    return CPLSM(static_cast<int>(res));
#endif
}

inline CPLSafeInt<int64_t> operator*(const CPLSafeInt<int64_t> &A,
                                     const CPLSafeInt<int64_t> &B)
{
#if defined(BUILTIN_OVERFLOW_CHECK_AVAILABLE) && defined(__x86_64__)
    long long res;
    if (__builtin_smulll_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(static_cast<int64_t>(res));
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<int64_t, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<int64_t, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<int64_t>(A2 * B2));
#else
    return SafeMulSigned(A, B);
#endif
}

inline CPLSafeInt<unsigned> operator*(const CPLSafeInt<unsigned> &A,
                                      const CPLSafeInt<unsigned> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    unsigned res;
    if (__builtin_umul_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<unsigned>(A2 * B2));
#else
    const unsigned a = A.v();
    const unsigned b = B.v();
    const uint64_t res = static_cast<uint64_t>(a) * b;
    if (res > std::numeric_limits<unsigned>::max())
    {
        throw CPLSafeIntOverflow();
    }
    return CPLSM(static_cast<unsigned>(res));
#endif
}

inline CPLSafeInt<uint64_t> operator*(const CPLSafeInt<uint64_t> &A,
                                      const CPLSafeInt<uint64_t> &B)
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    unsigned long long res;
    if (__builtin_umulll_overflow(A.v(), B.v(), &res))
        throw CPLSafeIntOverflow();
    return CPLSM(static_cast<uint64_t>(res));
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<uint64_t, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<uint64_t, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<uint64_t>(A2 * B2));
#else
    const uint64_t a = A.v();
    const uint64_t b = B.v();
    if (b > 0 && a > std::numeric_limits<uint64_t>::max() / b)
        throw CPLSafeIntOverflow();
    return CPLSM(a * b);
#endif
}

template <class T>
inline CPLSafeInt<T> SafeDivSigned(const CPLSafeInt<T> &A,
                                   const CPLSafeInt<T> &B)
{
    const auto a = A.v();
    const auto b = B.v();
    if (b == 0)
        throw CPLSafeIntOverflowDivisionByZero();
    if (a == std::numeric_limits<T>::min() && b == -1)
        throw CPLSafeIntOverflow();
    return CPLSM(a / b);
}

inline CPLSafeInt<int> operator/(const CPLSafeInt<int> &A,
                                 const CPLSafeInt<int> &B)
{
    return SafeDivSigned(A, B);
}

inline CPLSafeInt<int64_t> operator/(const CPLSafeInt<int64_t> &A,
                                     const CPLSafeInt<int64_t> &B)
{
    return SafeDivSigned(A, B);
}

inline CPLSafeInt<unsigned> operator/(const CPLSafeInt<unsigned> &A,
                                      const CPLSafeInt<unsigned> &B)
{
    const unsigned a = A.v();
    const unsigned b = B.v();
    if (b == 0)
        throw CPLSafeIntOverflowDivisionByZero();
    return CPLSM(a / b);
}

#endif  // CPL_SAFEMATHS_INCLUDED
