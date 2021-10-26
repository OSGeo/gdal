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

#ifndef CPL_SAFEMATHS_INCLUDED
#define CPL_SAFEMATHS_INCLUDED

#include <exception>
#include <limits>

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if (__GNUC__ >= 5 && !defined( __INTEL_COMPILER)) || __has_builtin(__builtin_sadd_overflow)
#  define BUILTIN_OVERFLOW_CHECK_AVAILABLE

#elif defined(_MSC_VER)

#  include "safeint.h"

#elif defined(GDAL_COMPILATION)
#  include "cpl_port.h"

#elif !defined(DISABLE_GINT64) && !defined(CPL_HAS_GINT64)
#  if defined(WIN32) && defined(_MSC_VER)
typedef _int64 GInt64;
typedef unsigned _int64 GUInt64;
#   else
typedef long long GInt64;
typedef unsigned long long GUInt64;
#  endif
#  define CPL_HAS_GINT64
#endif

template<typename T> struct CPLSafeInt
{
    T val;

    inline explicit CPLSafeInt(T valIn): val(valIn) {}

    inline T v() const { return val; }
};

class CPLSafeIntOverflow: public std::exception
{
public:
    inline CPLSafeIntOverflow() {}
};

class CPLSafeIntOverflowDivisionByZero: public CPLSafeIntOverflow
{
public:
    inline CPLSafeIntOverflowDivisionByZero() {}
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
    if( x < 0 )
        throw CPLSafeIntOverflow();
    return CPLSafeInt<unsigned>(static_cast<unsigned>(x));
}

// Unimplemented for now
inline CPLSafeInt<unsigned> CPLSM_TO_UNSIGNED(unsigned x);
inline CPLSafeInt<int> CPLSM(long x);
inline CPLSafeInt<unsigned> CPLSM_TO_UNSIGNED(long x);
inline CPLSafeInt<int> CPLSM(unsigned long x);
inline CPLSafeInt<unsigned> CPLSM_TO_UNSIGNED(unsigned long x);
#if defined(GDAL_COMPILATION)
inline CPLSafeInt<GInt64> CPLSM(GInt64 x)
{
    return CPLSafeInt<GInt64>(x);
}
inline CPLSafeInt<unsigned> CPLSM_TO_UNSIGNED(GInt64 x);
inline CPLSafeInt<GUInt64> CPLSM(GUInt64 x)
{
    return CPLSafeInt<GUInt64>(x);
}
inline CPLSafeInt<unsigned> CPLSM_TO_UNSIGNED(GUInt64 x);
#endif

#if defined(_MSC_VER)
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

template<class T> inline CPLSafeInt<T> SafeAddSigned( const CPLSafeInt<T>& A,
                                                      const CPLSafeInt<T>& B )
{
    const auto a = A.v();
    const auto b = B.v();
    if( a > 0 && b > 0 && a > std::numeric_limits<T>::max() - b )
        throw CPLSafeIntOverflow();
    if( a < 0 && b < 0 && a < std::numeric_limits<T>::min() - b )
        throw CPLSafeIntOverflow();
    return CPLSM(a+b);
}

inline CPLSafeInt<int> operator+( const CPLSafeInt<int>& A,
                                  const CPLSafeInt<int>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    int res;
    if( __builtin_sadd_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<int>(A2 + B2));
#elif defined(CPL_HAS_GINT64)
    const int a = A.v();
    const int b = B.v();
    const GInt64 res = static_cast<GInt64>(a) + b;
    if( res < std::numeric_limits<int>::min() ||
        res > std::numeric_limits<int>::max() )
    {
        throw CPLSafeIntOverflow();
    }
    return CPLSM(static_cast<int>(res));
#else
    return SafeAddSigned(A,B);
#endif
}

#if defined(GDAL_COMPILATION)
inline CPLSafeInt<GInt64> operator+( const CPLSafeInt<GInt64>& A,
                                     const CPLSafeInt<GInt64>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    GInt64 res;
    if( __builtin_saddll_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<GInt64, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<GInt64, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<GInt64>(A2 + B2));
#else
    return SafeAddSigned(A,B);
#endif
}
#endif //GDAL_COMPILATION

inline CPLSafeInt<unsigned> operator+( const CPLSafeInt<unsigned>& A,
                                       const CPLSafeInt<unsigned>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    unsigned res;
    if( __builtin_uadd_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<unsigned>(A2 + B2));
#else
    const unsigned a = A.v();
    const unsigned b = B.v();
    if( a > std::numeric_limits<unsigned>::max() - b )
        throw CPLSafeIntOverflow();
    return CPLSM(a+b);
#endif
}

#if defined(GDAL_COMPILATION)
inline CPLSafeInt<GUInt64> operator+( const CPLSafeInt<GUInt64>& A,
                                      const CPLSafeInt<GUInt64>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    GUInt64 res;
    if( __builtin_uaddll_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<GUInt64, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<GUInt64, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<GUInt64>(A2 + B2));
#else
    const GUInt64 a = A.v();
    const GUInt64 b = B.v();
    if( a > std::numeric_limits<GUInt64>::max() - b )
        throw CPLSafeIntOverflow();
    return CPLSM(a+b);
#endif
}
#endif //GDAL_COMPILATION

template<class T> inline CPLSafeInt<T> SafeSubSigned( const CPLSafeInt<T>& A,
                                                      const CPLSafeInt<T>& B )
{
    const auto a = A.v();
    const auto b = B.v();
    /* caution we must catch a == 0 && b = INT_MIN */
    if( a >= 0 && b < 0 && a > std::numeric_limits<T>::max() + b )
        throw CPLSafeIntOverflow();
    if( a < 0 && b > 0 && a < std::numeric_limits<T>::min() + b )
        throw CPLSafeIntOverflow();
    return CPLSM(a-b);
}

inline CPLSafeInt<int> operator-( const CPLSafeInt<int>& A,
                                  const CPLSafeInt<int>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    int res;
    if( __builtin_ssub_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<int>(A2 - B2));
#elif defined(CPL_HAS_GINT64)
    const int a = A.v();
    const int b = B.v();
    const GInt64 res = static_cast<GInt64>(a) - b;
    if( res < std::numeric_limits<int>::min() ||
        res > std::numeric_limits<int>::max() )
    {
        throw CPLSafeIntOverflow();
    }
    return CPLSM(static_cast<int>(res));
#else
    return SafeSubSigned(A,B);
#endif
}

#if defined(GDAL_COMPILATION)
inline CPLSafeInt<GInt64> operator-( const CPLSafeInt<GInt64>& A,
                                     const CPLSafeInt<GInt64>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    GInt64 res;
    if( __builtin_ssubll_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<GInt64, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<GInt64, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<GInt64>(A2 - B2));
#else
    return SafeSubSigned(A,B);
#endif
}
#endif //GDAL_COMPILATION

inline CPLSafeInt<unsigned> operator-( const CPLSafeInt<unsigned>& A,
                                       const CPLSafeInt<unsigned>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    unsigned res;
    if( __builtin_usub_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<unsigned>(A2 - B2));
#else
    const unsigned a = A.v();
    const unsigned b = B.v();
    if( a < b )
        throw CPLSafeIntOverflow();
    return CPLSM(a-b);
#endif
}

template<class T> inline CPLSafeInt<T> SafeMulSigned( const CPLSafeInt<T>& A,
                                                      const CPLSafeInt<T>& B )
{
    const auto a = A.v();
    const auto b = B.v();
    if( a > 0 && b > 0 && a > std::numeric_limits<T>::max() / b )
        throw CPLSafeIntOverflow();
    if( a > 0 && b < 0 && b < std::numeric_limits<T>::min() / a )
        throw CPLSafeIntOverflow();
    if( a < 0 && b > 0 && a < std::numeric_limits<T>::min() / b )
        throw CPLSafeIntOverflow();
    else if( a == std::numeric_limits<T>::min() )
    {
        if( b != 0 && b != 1 )
            throw CPLSafeIntOverflow();
    }
    else if( b == std::numeric_limits<T>::min() )
    {
        if( a != 0 && a != 1 )
            throw CPLSafeIntOverflow();
    }
    else if( a < 0 && b < 0 && -a > std::numeric_limits<T>::max() / (-b) )
        throw CPLSafeIntOverflow();

    return CPLSM(a*b);
}

inline CPLSafeInt<int> operator*( const CPLSafeInt<int>& A,
                                  const CPLSafeInt<int>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    int res;
    if( __builtin_smul_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<int, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<int>(A2 * B2));
#elif defined(CPL_HAS_GINT64)
    const int a = A.v();
    const int b = B.v();
    const GInt64 res = static_cast<GInt64>(a) * b;
    if( res < std::numeric_limits<int>::min() ||
        res > std::numeric_limits<int>::max() )
    {
        throw CPLSafeIntOverflow();
    }
    return CPLSM(static_cast<int>(res));
#else
    return SafeMulSigned(A,B);
#endif
}

#if defined(GDAL_COMPILATION)
inline CPLSafeInt<GInt64> operator*( const CPLSafeInt<GInt64>& A,
                                     const CPLSafeInt<GInt64>& B )
{
#if defined(BUILTIN_OVERFLOW_CHECK_AVAILABLE) && defined(__x86_64__)
    GInt64 res;
    if( __builtin_smulll_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<GInt64, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<GInt64, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<GInt64>(A2 * B2));
#else
    return SafeMulSigned(A,B);
#endif
}
#endif //GDAL_COMPILATION

inline CPLSafeInt<unsigned> operator*( const CPLSafeInt<unsigned>& A,
                                       const CPLSafeInt<unsigned>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    unsigned res;
    if( __builtin_umul_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<unsigned, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<unsigned>(A2 * B2));
#elif defined(CPL_HAS_GINT64)
    const unsigned a = A.v();
    const unsigned b = B.v();
    const GUInt64 res = static_cast<GUInt64>(a) * b;
    if( res > std::numeric_limits<unsigned>::max() )
    {
        throw CPLSafeIntOverflow();
    }
    return CPLSM(static_cast<unsigned>(res));
#else
    const unsigned a = A.v();
    const unsigned b = B.v();
    if( b > 0 && a > std::numeric_limits<unsigned>::max() / b )
        throw CPLSafeIntOverflow();
    return CPLSM(a*b);
#endif
}

#if defined(GDAL_COMPILATION)
inline CPLSafeInt<GUInt64> operator*( const CPLSafeInt<GUInt64>& A,
                                      const CPLSafeInt<GUInt64>& B )
{
#ifdef BUILTIN_OVERFLOW_CHECK_AVAILABLE
    GUInt64 res;
    if( __builtin_umulll_overflow(A.v(), B.v(), &res) )
        throw CPLSafeIntOverflow();
    return CPLSM(res);
#elif defined(_MSC_VER)
    msl::utilities::SafeInt<GUInt64, CPLMSVCSafeIntException> A2(A.v());
    msl::utilities::SafeInt<GUInt64, CPLMSVCSafeIntException> B2(B.v());
    return CPLSM(static_cast<GUInt64>(A2 * B2));
#else
    const GUInt64 a = A.v();
    const GUInt64 b = B.v();
    if( b > 0 && a > std::numeric_limits<GUInt64>::max() / b )
        throw CPLSafeIntOverflow();
    return CPLSM(a*b);
#endif
}
#endif //GDAL_COMPILATION

template<class T> inline CPLSafeInt<T> SafeDivSigned( const CPLSafeInt<T>& A,
                                                      const CPLSafeInt<T>& B )
{
    const auto a = A.v();
    const auto b = B.v();
    if( b == 0 )
        throw CPLSafeIntOverflowDivisionByZero();
    if( a == std::numeric_limits<T>::min() && b == -1 )
        throw CPLSafeIntOverflow();
    return CPLSM(a/b);
}

inline CPLSafeInt<int> operator/( const CPLSafeInt<int>& A,
                                  const CPLSafeInt<int>& B )
{
    return SafeDivSigned(A, B);
}

#if defined(GDAL_COMPILATION)
inline CPLSafeInt<GInt64> operator/( const CPLSafeInt<GInt64>& A,
                                     const CPLSafeInt<GInt64>& B )
{
    return SafeDivSigned(A, B);
}
#endif //GDAL_COMPILATION

inline CPLSafeInt<unsigned> operator/( const CPLSafeInt<unsigned>& A,
                                       const CPLSafeInt<unsigned>& B )
{
    const unsigned a = A.v();
    const unsigned b = B.v();
    if( b == 0 )
        throw CPLSafeIntOverflowDivisionByZero();
    return CPLSM(a/b);
}

#endif // CPL_SAFEMATHS_INCLUDED
