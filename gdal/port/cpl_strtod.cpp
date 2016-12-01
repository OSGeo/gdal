/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Functions to convert ASCII string to floating point number.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu.
 *
 ******************************************************************************
 * Copyright (c) 2006, Andrey Kiselev
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "cpl_conv.h"

#include <cerrno>
#include <clocale>
#include <cstring>
#include <cstdlib>
#include <limits>

#include "cpl_config.h"

CPL_CVSID("$Id$");

// XXX: with GCC 2.95 strtof() function is only available when in c99 mode.
// Fix it here not touching the compiler options.
#if defined(HAVE_STRTOF) && !HAVE_DECL_STRTOF
extern "C" {
extern float strtof(const char *nptr, char **endptr);
}
#endif

/************************************************************************/
/*                            CPLAtofDelim()                            */
/************************************************************************/

/**
 * Converts ASCII string to floating point number.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. The behaviour is the
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
  return CPLStrtodDelim(nptr, NULL, point);
}

/************************************************************************/
/*                              CPLAtof()                               */
/************************************************************************/

/**
 * Converts ASCII string to floating point number.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. The behaviour is the
 * same as
 *
 *   CPLStrtod(nptr, (char **)NULL);
 *
 * This function does the same as standard atof(3), but does not take
 * locale in account. That means, the decimal delimiter is always '.'
 * (decimal point). Use CPLAtofDelim() function if you want to specify
 * custom delimiter.
 *
 * IMPORTANT NOTE.
 * Existence of this function does not mean you should always use it.
 * Sometimes you should use standard locale aware atof(3) and its family. When
 * you need to process the user's input (for example, command line parameters)
 * use atof(3), because the user works in a localized environment and the user's input will
 * be done according to the locale set. In particular that means we should not
 * make assumptions about character used as decimal delimiter, it can be
 * either "." or ",".
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
  return CPLStrtod(nptr, NULL);
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

double CPLAtofM( const char *nptr )

{
    const int nMaxSearch = 50;

    for( int i = 0; i < nMaxSearch; i++ )
    {
        if( nptr[i] == ',' )
            return CPLStrtodDelim( nptr, NULL, ',' );
        if( nptr[i] == '.' || nptr[i] == '\0' )
            return CPLStrtodDelim( nptr, NULL, '.' );
    }

    return CPLStrtodDelim( nptr, NULL, '.' );
}

/************************************************************************/
/*                      CPLReplacePointByLocalePoint()                  */
/************************************************************************/

static char* CPLReplacePointByLocalePoint(const char* pszNumber, char point)
{
#if defined(__ANDROID__)
    static char byPoint = 0;
    if (byPoint == 0)
    {
        char szBuf[16];
        snprintf(szBuf, sizeof(szBuf), "%.1f", 1.0);
        byPoint = szBuf[1];
    }
    if (point != byPoint)
    {
        const char* pszPoint = strchr(pszNumber, point);
        if (pszPoint)
        {
            char* pszNew = CPLStrdup(pszNumber);
            pszNew[pszPoint - pszNumber] = byPoint;
            return pszNew;
        }
    }
#else  // ndef __ANDROID__
    struct lconv *poLconv = localeconv();
    if ( poLconv
         && poLconv->decimal_point
         && poLconv->decimal_point[0] != '\0' )
    {
        char byPoint = poLconv->decimal_point[0];

        if (point != byPoint)
        {
            const char* pszLocalePoint = strchr(pszNumber, byPoint);
            const char* pszPoint = strchr(pszNumber, point);
            if (pszPoint || pszLocalePoint)
            {
                char* pszNew = CPLStrdup(pszNumber);
                if( pszLocalePoint )
                    pszNew[pszLocalePoint - pszNumber] = ' ';
                if( pszPoint )
                    pszNew[pszPoint - pszNumber] = byPoint;
                return pszNew;
            }
        }
    }
#endif  // __ANDROID__

    return const_cast<char*>( pszNumber );
}

/************************************************************************/
/*                          CPLStrtodDelim()                            */
/************************************************************************/

/**
 * Converts ASCII string to floating point number using specified delimiter.
 *
 * This function converts the initial portion of the string pointed to
 * by nptr to double floating point representation. This function does the
 * same as standard strtod(3), but does not take locale in account. Instead of
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
double CPLStrtodDelim(const char *nptr, char **endptr, char point)
{
    while( *nptr == ' ' )
        nptr ++;

    if (nptr[0] == '-')
    {
        if (strcmp(nptr, "-1.#QNAN") == 0 ||
            strcmp(nptr, "-1.#IND") == 0)
        {
            if( endptr ) *endptr = (char*)nptr + strlen(nptr);
            // While it is possible on some platforms to flip the sign
            // of NAN to negative, this function will always return a positive
            // quiet (non-signalling) NaN.
            return std::numeric_limits<double>::quiet_NaN();
        }

        if (strcmp(nptr,"-inf") == 0 ||
            STARTS_WITH_CI(nptr, "-1.#INF"))
        {
            if( endptr ) *endptr = (char*)nptr + strlen(nptr);
            return -std::numeric_limits<double>::infinity();
        }
    }
    else if (nptr[0] == '1')
    {
        if (strcmp(nptr, "1.#QNAN") == 0)
        {
            if( endptr ) *endptr = (char*)nptr + strlen(nptr);
            return std::numeric_limits<double>::quiet_NaN();
        }
        if( STARTS_WITH_CI(nptr, "1.#INF") )
        {
            if( endptr ) *endptr = (char*)nptr + strlen(nptr);
            return std::numeric_limits<double>::infinity();
        }
    }
    else if (nptr[0] == 'i' && strcmp(nptr,"inf") == 0)
    {
        if( endptr ) *endptr = (char*)nptr + strlen(nptr);
        return std::numeric_limits<double>::infinity();
    }
    else if (nptr[0] == 'n' && strcmp(nptr,"nan") == 0)
    {
        if( endptr ) *endptr = (char*)nptr + strlen(nptr);
        return std::numeric_limits<double>::quiet_NaN();
    }

/* -------------------------------------------------------------------- */
/*  We are implementing a simple method here: copy the input string     */
/*  into the temporary buffer, replace the specified decimal delimiter  */
/*  with the one, taken from locale settings and use standard strtod()  */
/*  on that buffer.                                                     */
/* -------------------------------------------------------------------- */
    char* pszNumber = CPLReplacePointByLocalePoint(nptr, point);

    const double dfValue = strtod( pszNumber, endptr );
    const int nError = errno;

    if ( endptr )
        *endptr = (char *)nptr + (*endptr - pszNumber);

    if (pszNumber != (char*) nptr)
        CPLFree( pszNumber );

    errno = nError;
    return dfValue;
}

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
/*                          CPLStrtofDelim()                            */
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
#if defined(HAVE_STRTOF)
/* -------------------------------------------------------------------- */
/*  We are implementing a simple method here: copy the input string     */
/*  into the temporary buffer, replace the specified decimal delimiter  */
/*  with the one, taken from locale settings and use standard strtof()  */
/*  on that buffer.                                                     */
/* -------------------------------------------------------------------- */
    char * const pszNumber = CPLReplacePointByLocalePoint(nptr, point);
    double dfValue = strtof( pszNumber, endptr );
    const int nError = errno;

    if ( endptr )
        *endptr = const_cast<char *>(nptr) + (*endptr - pszNumber);

    if (pszNumber != nptr)
        CPLFree( pszNumber );

    errno = nError;
    return static_cast<float>(dfValue);

#else

    return static_cast<float>( CPLStrtodDelim(nptr, endptr, point) );

#endif  // HAVE_STRTOF
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
