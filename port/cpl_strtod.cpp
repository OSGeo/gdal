/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Functions to convert ASCII string to floating point number.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu.
 *
 ******************************************************************************
 * Copyright (c) 2006, Andrey Kiselev
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include <cerrno>
#include <clocale>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <optional>

// Coverity complains about CPLAtof(CPLGetConfigOption(...)) causing
// a "untrusted loop bound" in the loop "Find a reasonable position for the end
// of the string to provide to fast_float"
#ifndef __COVERITY__
#define USE_FAST_FLOAT
#endif

#ifdef USE_FAST_FLOAT
#include "include_fast_float.h"
#endif

#include "cpl_config.h"

/************************************************************************/
/*                            CPLAtofDelim()                            */
/************************************************************************/

/**
 * Converts ASCII string to floating point number.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. The behavior is the
 * same as
 *
 *   CPLStrtodDelim(nptr, (char **)NULL, point);
 *
 * This function does the same as standard atof(3), but does not take locale
 * in account. Instead of locale defined decimal delimiter you can specify
 * your own one. Also see notes for CPLAtof() function.
 *
 * @param nptr Pointer to string to convert.
 * @param point Decimal delimiter.
 *
 * @return Converted value, if any.
 */
double CPLAtofDelim(const char *nptr, char point)
{
    return CPLStrtodDelim(nptr, nullptr, point);
}

/************************************************************************/
/*                              CPLAtof()                               */
/************************************************************************/

/**
 * Converts ASCII string to floating point number.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. The behavior is the
 * same as
 *
 *   CPLStrtod(nptr, (char **)NULL);
 *
 * This function does the same as standard atof(3), but does not take
 * locale in account. That means, the decimal delimiter is always '.'
 * (decimal point). Use CPLAtofDelim() function if you want to specify
 * custom delimiter.
 *
 * IMPORTANT NOTE:
 *
 * Existence of this function does not mean you should always use it.  Sometimes
 * you should use standard locale aware atof(3) and its family. When you need to
 * process the user's input (for example, command line parameters) use atof(3),
 * because the user works in a localized environment and the user's input will
 * be done according to the locale set. In particular that means we should not
 * make assumptions about character used as decimal delimiter, it can be either
 * "." or ",".
 *
 * But when you are parsing some ASCII file in predefined format, you most
 * likely need CPLAtof(), because such files distributed across the systems
 * with different locales and floating point representation should be
 * considered as a part of file format. If the format uses "." as a delimiter
 * the same character must be used when parsing number regardless of actual
 * locale setting.
 *
 * @param nptr Pointer to string to convert.
 *
 * @return Converted value, if any.
 */
double CPLAtof(const char *nptr)
{
    return CPLStrtod(nptr, nullptr);
}

/************************************************************************/
/*                              CPLAtofM()                              */
/************************************************************************/

/**
 * Converts ASCII string to floating point number using any numeric locale.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. This function does the
 * same as standard atof(), but it allows a variety of locale representations.
 * That is it supports numeric values with either a comma or a period for
 * the decimal delimiter.
 *
 * PS. The M stands for Multi-lingual.
 *
 * @param nptr The string to convert.
 *
 * @return Converted value, if any.  Zero on failure.
 */

double CPLAtofM(const char *nptr)

{
    const int nMaxSearch = 50;

    for (int i = 0; i < nMaxSearch; i++)
    {
        if (nptr[i] == ',')
            return CPLStrtodDelim(nptr, nullptr, ',');
        if (nptr[i] == '.' || nptr[i] == '\0')
            return CPLStrtodDelim(nptr, nullptr, '.');
    }

    return CPLStrtodDelim(nptr, nullptr, '.');
}

/************************************************************************/
/*                    CPLReplacePointByLocalePoint()                    */
/************************************************************************/

/* Return a newly allocated variable if substitution was done, or NULL
 * otherwise.
 */
static char *CPLReplacePointByLocalePoint(const char *pszNumber, char point)
{
#if defined(__ANDROID__) && __ANDROID_API__ < 20
    // localeconv() only available since API 20
    static char byPoint = 0;
    if (byPoint == 0)
    {
        char szBuf[16] = {};
        snprintf(szBuf, sizeof(szBuf), "%.1f", 1.0);
        byPoint = szBuf[1];
    }
    if (point != byPoint)
    {
        const char *pszPoint = strchr(pszNumber, point);
        if (pszPoint)
        {
            char *pszNew = CPLStrdup(pszNumber);
            pszNew[pszPoint - pszNumber] = byPoint;
            return pszNew;
        }
    }
#else   // ndef __ANDROID__
    struct lconv *poLconv = localeconv();
    if (poLconv && poLconv->decimal_point && poLconv->decimal_point[0] != '\0')
    {
        char byPoint = poLconv->decimal_point[0];

        if (point != byPoint)
        {
            const char *pszLocalePoint = strchr(pszNumber, byPoint);
            const char *pszPoint = strchr(pszNumber, point);
            if (pszPoint || pszLocalePoint)
            {
                char *pszNew = CPLStrdup(pszNumber);
                if (pszLocalePoint)
                    pszNew[pszLocalePoint - pszNumber] = ' ';
                if (pszPoint)
                    pszNew[pszPoint - pszNumber] = byPoint;
                return pszNew;
            }
        }
    }
#endif  // __ANDROID__

    return nullptr;
}

/************************************************************************/
/*                           CPLStrtodDelim()                           */
/************************************************************************/

/**
 * Converts ASCII string to floating point number using specified delimiter.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. This function does the
 * same as standard strtod(3), but does not take locale into account. Instead of
 * the locale-defined decimal delimiter you can specify your own one. Also see
 * notes for CPLAtof() function.
 *
 * @param nptr Pointer to string to convert.
 * @param endptr If is not NULL, a pointer to the character after the last
 * character used in the conversion is stored in the location referenced
 * by endptr.
 * @param point Decimal delimiter.
 *
 * @return Converted value, if any.
 */
double CPLStrtodDelim(const char *nptr, char **endptr, char point)
{
    return cpl::strtod_delim(nptr, endptr, point);
}

namespace cpl
{
/**
 * Converts ASCII string to floating point number using specified delimiter.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. This function does the
 * same as standard strtod(3), but does not take locale into account. Instead of
 * the locale-defined decimal delimiter you can specify your own one. Also see
 * notes for CPLAtof() function.
 *
 * @param str String view to convert.
 * @param endptr If is not NULL, a pointer to the character after the last
 * character used in the conversion is stored in the location referenced
 * by endptr.
 * @param point Decimal delimiter.
 *
 * @return Converted value, if any.
 */
double strtod_delim(std::string_view str, char **endptr, char point)
{
    str = trim(str);

    if (str.empty())
    {
        if (endptr)
            *endptr = const_cast<char *>(str.data());
        return 0;
    }

    if (str[0] == '-')
    {
        if (starts_with(str, "-1.#QNAN") || starts_with(str, "-1.#IND"))
        {
            if (endptr)
                *endptr = const_cast<char *>(str.data()) + str.size();
            // While it is possible on some platforms to flip the sign
            // of NAN to negative, this function will always return a positive
            // quiet (non-signalling) NaN.
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (starts_with_ci(str, "-1.#INF"))
        {
            if (endptr)
                *endptr = const_cast<char *>(str.data()) + str.size();
            return -std::numeric_limits<double>::infinity();
        }
    }
    else if (str[0] == '1')
    {
        if (starts_with(str, "1.#QNAN") || starts_with(str, "1.#SNAN"))
        {
            if (endptr)
                *endptr = const_cast<char *>(str.data()) + str.size();
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (starts_with_ci(str, "1.#INF"))
        {
            if (endptr)
                *endptr = const_cast<char *>(str.data()) + str.size();
            return std::numeric_limits<double>::infinity();
        }
    }

    // Skip leading '+' as non-handled by fast_float
    if (str[0] == '+')
        str = str.substr(1);

    // Find a reasonable position for the end of the string to provide to
    // fast_float
    std::ptrdiff_t endPos = 0;
    while (endPos < static_cast<std::ptrdiff_t>(str.size()) &&
           ((str[endPos] >= '0' && str[endPos] <= '9') ||
            str[endPos] == point || str[endPos] == '+' || str[endPos] == '-' ||
            str[endPos] == 'e' || str[endPos] == 'E'))
    {
        ++endPos;
    }

    double dfValue = 0;
    const fast_float::parse_options options{fast_float::chars_format::general,
                                            point};
    auto answer = fast_float::from_chars_advanced(
        str.data(), str.data() + endPos, dfValue, options);
    if (answer.ec != std::errc())
    {
        if (
            // Triggered by ogr_pg tests
            starts_with_ci(str, "-Infinity"))
        {
            dfValue = -std::numeric_limits<double>::infinity();
            answer.ptr = str.data() + strlen("-Infinity");
        }
        else if (starts_with_ci(str, "-inf"))
        {
            dfValue = -std::numeric_limits<double>::infinity();
            answer.ptr = str.data() + strlen("-inf");
        }
        else if (
            // Triggered by ogr_pg tests
            starts_with_ci(str, "Infinity"))
        {
            dfValue = std::numeric_limits<double>::infinity();
            answer.ptr = str.data() + strlen("Infinity");
        }
        else if (cpl::starts_with_ci(str, "inf"))
        {
            dfValue = std::numeric_limits<double>::infinity();
            answer.ptr = str.data() + strlen("inf");
        }
        else if (cpl::starts_with_ci(str, "nan"))
        {
            dfValue = std::numeric_limits<double>::quiet_NaN();
            answer.ptr = str.data() + strlen("nan");
        }
        else
        {
            errno = answer.ptr == str.data() ? 0 : ERANGE;
        }
    }
    if (endptr)
    {
        *endptr = const_cast<char *>(answer.ptr);
    }

    return dfValue;
}
}  // namespace cpl

/************************************************************************/
/*                             CPLStrtod()                              */
/************************************************************************/

/**
 * Converts ASCII string to floating point number.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. This function does the
 * same as standard strtod(3), but does not take locale in account. That
 * means, the decimal delimiter is always '.' (decimal point). Use
 * CPLStrtodDelim() function if you want to specify custom delimiter. Also
 * see notes for CPLAtof() function.
 *
 * @param nptr Pointer to string to convert.
 * @param endptr If is not NULL, a pointer to the character after the last
 * character used in the conversion is stored in the location referenced
 * by endptr.
 *
 * @return Converted value, if any.
 */
double CPLStrtod(const char *nptr, char **endptr)
{
    return CPLStrtodDelim(nptr, endptr, '.');
}

/************************************************************************/
/*                             CPLStrtodM()                             */
/************************************************************************/

/**
 * Converts ASCII string to floating point number.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. This function does the
 * same as standard strtod(3), but does not take locale in account.
 *
 * That function accepts '.' (decimal point) or ',' (comma) as decimal
 * delimiter.
 *
 * @param nptr Pointer to string to convert.
 * @param endptr If is not NULL, a pointer to the character after the last
 * character used in the conversion is stored in the location referenced
 * by endptr.
 *
 * @return Converted value, if any.
 * @since GDAL 3.9
 */
double CPLStrtodM(const char *nptr, char **endptr)

{
    const int nMaxSearch = 50;

    for (int i = 0; i < nMaxSearch; i++)
    {
        if (nptr[i] == ',')
            return CPLStrtodDelim(nptr, endptr, ',');
        if (nptr[i] == '.' || nptr[i] == '\0')
            return CPLStrtodDelim(nptr, endptr, '.');
    }

    return CPLStrtodDelim(nptr, endptr, '.');
}

/************************************************************************/
/*                           CPLStrtofDelim()                           */
/************************************************************************/

/**
 * Converts ASCII string to floating point number using specified delimiter.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to single floating point representation. This function does the
 * same as standard strtof(3), but does not take locale in account. Instead of
 * locale defined decimal delimiter you can specify your own one. Also see
 * notes for CPLAtof() function.
 *
 * @param nptr Pointer to string to convert.
 * @param endptr If is not NULL, a pointer to the character after the last
 * character used in the conversion is stored in the location referenced
 * by endptr.
 * @param point Decimal delimiter.
 *
 * @return Converted value, if any.
 */
float CPLStrtofDelim(const char *nptr, char **endptr, char point)
{
    /* -------------------------------------------------------------------- */
    /*  We are implementing a simple method here: copy the input string     */
    /*  into the temporary buffer, replace the specified decimal delimiter  */
    /*  with the one, taken from locale settings and use standard strtof()  */
    /*  on that buffer.                                                     */
    /* -------------------------------------------------------------------- */
    char *const pszNewNumberOrNull = CPLReplacePointByLocalePoint(nptr, point);
    const char *pszNumber = pszNewNumberOrNull ? pszNewNumberOrNull : nptr;
    const float fValue = strtof(pszNumber, endptr);
    const int nError = errno;

    if (endptr)
        *endptr = const_cast<char *>(nptr) + (*endptr - pszNumber);

    if (pszNewNumberOrNull)
        CPLFree(pszNewNumberOrNull);

    errno = nError;
    return fValue;
}

/************************************************************************/
/*                             CPLStrtof()                              */
/************************************************************************/

/**
 * Converts ASCII string to floating point number.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to single floating point representation. This function does the
 * same as standard strtof(3), but does not take locale in account. That
 * means, the decimal delimiter is always '.' (decimal point). Use
 * CPLStrtofDelim() function if you want to specify custom delimiter. Also
 * see notes for CPLAtof() function.
 *
 * @param nptr Pointer to string to convert.
 * @param endptr If is not NULL, a pointer to the character after the last
 * character used in the conversion is stored in the location referenced
 * by endptr.
 *
 * @return Converted value, if any.
 */
float CPLStrtof(const char *nptr, char **endptr)
{
    return CPLStrtofDelim(nptr, endptr, '.');
}
