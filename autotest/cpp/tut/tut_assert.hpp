#ifndef TUT_ASSERT_H_GUARD
#define TUT_ASSERT_H_GUARD

#include "tut_exception.hpp"
#include <limits>
#include <iomanip>

#if defined(TUT_USE_POSIX)
#include <errno.h>
#include <cstring>
#endif

namespace tut
{

    namespace detail
    {
        template<typename M>
        std::ostream &msg_prefix(std::ostream &str, const M &msg)
        {
            std::stringstream ss;
            ss << msg;

            if(!ss.str().empty())
            {
                str << ss.rdbuf() << ": ";
            }

            return str;
        }
    }


namespace
{

/**
 * Tests provided condition.
 * Throws if false.
 */
static inline void ensure(bool cond)
{
    if (!cond)
    {
        // TODO: default ctor?
        throw failure("");
    }
}

/**
 * Tests provided condition.
 * Throws if true.
 */
static inline void ensure_not(bool cond)
{
    ensure(!cond);
}

/**
 * Tests provided condition.
 * Throws if false.
 */
template <typename M>
void ensure(const M& msg, bool cond)
{
    if (!cond)
    {
        throw failure(msg);
    }
}

/**
 * Tests provided condition.
 * Throws if true.
 */
template <typename M>
void ensure_not(const M& msg, bool cond)
{
    ensure(msg, !cond);
}

/**
 * Tests two objects for being equal.
 * Throws if false.
 *
 * NB: both T and Q must have operator << defined somewhere, or
 * client code will not compile at all!
 */
template <typename M, typename LHS, typename RHS>
void ensure_equals(const M& msg, const LHS& actual, const RHS& expected)
{
    if (expected != actual)
    {
        std::stringstream ss;
        detail::msg_prefix(ss,msg)
           << "expected '"
           << expected
           << "' actual '"
           << actual
           << '\'';
        throw failure(ss.str());
    }
}

template <typename LHS, typename RHS>
void ensure_equals(const LHS& actual, const RHS& expected)
{
    ensure_equals("Values are not equal", actual, expected);
}

template<typename M>
void ensure_equals(const M& msg, const double& actual, const double& expected,
                   const double& epsilon = std::numeric_limits<double>::epsilon())
{
    const double diff = actual - expected;

    if ( !((diff <= epsilon) && (diff >= -epsilon )) )
    {
        std::stringstream ss;
        detail::msg_prefix(ss,msg)
           << std::scientific
           << std::showpoint
           << std::setprecision(16)
           << "expected " << expected
           << " actual " << actual
           << " with precision " << epsilon;
        throw failure(ss.str());
    }
}
/**
 * Tests two objects for being at most in given distance one from another.
 * Borders are excluded.
 * Throws if false.
 *
 * NB: T must have operator << defined somewhere, or
 * client code will not compile at all! Also, T shall have
 * operators + and -, and be comparable.
 *
 * TODO: domains are wrong, T - T might not yield T, but Q
 */
template <typename M, class T>
void ensure_distance(const M& msg, const T& actual, const T& expected, const T& distance)
{
    if (expected-distance >= actual || expected+distance <= actual)
    {
        std::stringstream ss;
        detail::msg_prefix(ss,msg)
            << " expected ("
            << expected-distance
            << " - "
            << expected+distance
            << ") actual '"
            << actual
            << '\'';
        throw failure(ss.str());
    }
}

template <class T>
void ensure_distance(const T& actual, const T& expected, const T& distance)
{
    ensure_distance<>("Distance is wrong", actual, expected, distance);
}

template<typename M>
void ensure_errno(const M& msg, bool cond)
{
    if(!cond)
    {
#if defined(TUT_USE_POSIX)
        char e[512];
        std::stringstream ss;
        detail::msg_prefix(ss,msg)
           << strerror_r(errno, e, sizeof(e));
        throw failure(ss.str());
#else
        throw failure(msg);
#endif
    }
}

/**
 * Unconditionally fails with message.
 */
static inline void fail(const char* msg = "")
{
    throw failure(msg);
}

template<typename M>
void fail(const M& msg)
{
    throw failure(msg);
}

} // end of namespace

}

#endif

