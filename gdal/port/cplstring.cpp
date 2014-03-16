/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  CPLString implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_string.h"
#include <string>

CPL_CVSID("$Id$");

/*
 * The CPLString class is derived from std::string, so the vast majority 
 * of the implementation comes from that.  This module is just the extensions
 * we add. 
 */

/************************************************************************/
/*                               Printf()                               */
/************************************************************************/

CPLString &CPLString::Printf( const char *pszFormat, ... )

{
    va_list args;

    va_start( args, pszFormat );
    vPrintf( pszFormat, args );
    va_end( args );

    return *this;
}

/************************************************************************/
/*                              vPrintf()                               */
/************************************************************************/

CPLString &CPLString::vPrintf( const char *pszFormat, va_list args )

{
/* -------------------------------------------------------------------- */
/*      This implementation for platforms without vsnprintf() will      */
/*      just plain fail if the formatted contents are too large.        */
/* -------------------------------------------------------------------- */

#if !defined(HAVE_VSNPRINTF)
    char *pszBuffer = (char *) CPLMalloc(30000);
    if( vsprintf( pszBuffer, pszFormat, args) > 29998 )
    {
        CPLError( CE_Fatal, CPLE_AppDefined, 
                  "CPLString::vPrintf() ... buffer overrun." );
    }
    *this = pszBuffer;
    CPLFree( pszBuffer );

/* -------------------------------------------------------------------- */
/*      This should grow a big enough buffer to hold any formatted      */
/*      result.                                                         */
/* -------------------------------------------------------------------- */
#else
    char szModestBuffer[500];
    int nPR;
    va_list wrk_args;

#ifdef va_copy
    va_copy( wrk_args, args );
#else
    wrk_args = args;
#endif
    
    nPR = vsnprintf( szModestBuffer, sizeof(szModestBuffer), pszFormat, 
                     wrk_args );
    if( nPR == -1 || nPR >= (int) sizeof(szModestBuffer)-1 )
    {
        int nWorkBufferSize = 2000;
        char *pszWorkBuffer = (char *) CPLMalloc(nWorkBufferSize);

#ifdef va_copy
        va_end( wrk_args );
        va_copy( wrk_args, args );
#else
        wrk_args = args;
#endif
        while( (nPR=vsnprintf( pszWorkBuffer, nWorkBufferSize, pszFormat,wrk_args))
               >= nWorkBufferSize-1 
               || nPR == -1 )
        {
            nWorkBufferSize *= 4;
            pszWorkBuffer = (char *) CPLRealloc(pszWorkBuffer, 
                                                nWorkBufferSize );
#ifdef va_copy
            va_end( wrk_args );
            va_copy( wrk_args, args );
#else
            wrk_args = args;
#endif
        }
        *this = pszWorkBuffer;
        CPLFree( pszWorkBuffer );
    }
    else
    {
        *this = szModestBuffer;
    }
    va_end( wrk_args );
#endif

    return *this;
}

/************************************************************************/
/*                              FormatC()                               */
/************************************************************************/

/**
 * Format double in C locale.
 * 
 * The passed value is formatted using the C locale (period as decimal 
 * seperator) and appended to the target CPLString. 
 *
 * @param dfValue the value to format. 
 * @param pszFormat the sprintf() style format to use or omit for default.
 * Note that this format string should only include one substitution argument 
 * and it must be for a double (%f or %g). 
 *
 * @return a reference to the CPLString. 
 */

CPLString &CPLString::FormatC( double dfValue, const char *pszFormat )

{
    if( pszFormat == NULL )
        pszFormat = "%g";

    char szWork[512]; // presumably long enough for any number?

    sprintf( szWork, pszFormat, dfValue );
    CPLAssert( strlen(szWork) < sizeof(szWork) );
    
    if( strchr( szWork, ',' ) != NULL )
    {
        char *pszDelim = strchr( szWork, ',' );
        *pszDelim = '.';
    }

    *this += szWork;
    
    return *this;
}

/************************************************************************/
/*                                Trim()                                */
/************************************************************************/

/**
 * Trim white space.
 *
 * Trims white space off the let and right of the string.  White space
 * is any of a space, a tab, a newline ('\n') or a carriage control ('\r').
 *
 * @return a reference to the CPLString. 
 */

CPLString &CPLString::Trim()

{
    size_t iLeft, iRight;
    static const char szWhitespace[] = " \t\r\n";

    iLeft = find_first_not_of( szWhitespace );
    iRight = find_last_not_of( szWhitespace );

    if( iLeft == std::string::npos )
    {
        erase();
        return *this;
    }
    
    assign( substr( iLeft, iRight - iLeft + 1 ) );

    return *this;
}

/************************************************************************/
/*                               Recode()                               */
/************************************************************************/

CPLString &CPLString::Recode( const char *pszSrcEncoding,
                              const char *pszDstEncoding )

{
    if( pszSrcEncoding == NULL )
        pszSrcEncoding = CPL_ENC_UTF8;
    if( pszDstEncoding == NULL )
        pszDstEncoding = CPL_ENC_UTF8;

    if( strcmp(pszSrcEncoding,pszDstEncoding) == 0 )
        return *this;

    char *pszRecoded = CPLRecode( c_str(), 
                                  pszSrcEncoding,
                                  pszDstEncoding );

    if( pszRecoded == NULL )
        return *this;

    assign( pszRecoded );
    CPLFree( pszRecoded );

    return *this;
}

/************************************************************************/
/*                               ifind()                                */
/************************************************************************/

/** 
 * Case insensitive find() alternative.
 *
 * @param str substring to find.
 * @param pos offset in the string at which the search starts.
 * @return the position of substring in the string or std::string::npos if not found.
 * @since GDAL 1.9.0
 */

size_t CPLString::ifind( const std::string & str, size_t pos ) const

{
    return ifind( str.c_str(), pos );
}

/**
 * Case insensitive find() alternative.
 *
 * @param s substring to find.
 * @param nPos offset in the string at which the search starts.
 * @return the position of the substring in the string or std::string::npos if not found.
 * @since GDAL 1.9.0
 */

size_t CPLString::ifind( const char *s, size_t nPos ) const

{
    const char *pszHaystack = c_str();
    char chFirst = (char) ::tolower( s[0] );
    int nTargetLen = strlen(s);

    if( nPos > size() )
        nPos = size();

    pszHaystack += nPos;

    while( *pszHaystack != '\0' )
    {
        if( chFirst == ::tolower(*pszHaystack) )
        {
            if( EQUALN(pszHaystack,s,nTargetLen) )
                return nPos;
        }

        nPos++;
        pszHaystack++;
    }

    return std::string::npos;
}

/************************************************************************/
/*                              toupper()                               */
/************************************************************************/

/**
 * Convert to upper case in place.
 */

CPLString &CPLString::toupper()

{
    size_t i;

    for( i = 0; i < size(); i++ )
        (*this)[i] = (char) ::toupper( (*this)[i] );

    return *this;
}

/************************************************************************/
/*                              tolower()                               */
/************************************************************************/

/**
 * Convert to lower case in place.
 */

CPLString &CPLString::tolower()

{
    size_t i;

    for( i = 0; i < size(); i++ )
        (*this)[i] = (char) ::tolower( (*this)[i] );

    return *this;
}

/************************************************************************/
/*                         CPLURLGetValue()                             */
/************************************************************************/

/**
 * Return the value matching a key from a key=value pair in a URL.
 *
 * @param pszURL the URL.
 * @param pszKey the key to find.
 * @return the value of empty string if not found.
 * @since GDAL 1.9.0
 */
CPLString CPLURLGetValue(const char* pszURL, const char* pszKey)
{
    CPLString osKey(pszKey);
    osKey += "=";
    size_t nKeyPos = CPLString(pszURL).ifind(osKey);
    if (nKeyPos != std::string::npos && nKeyPos > 0 &&
        (pszURL[nKeyPos-1] == '?' || pszURL[nKeyPos-1] == '&'))
    {
        CPLString osValue(pszURL + nKeyPos + strlen(osKey));
        const char* pszValue = osValue.c_str();
        const char* pszSep = strchr(pszValue, '&');
        if (pszSep)
        {
            osValue.resize(pszSep - pszValue);
        }
        return osValue;
    }
    return "";
}

/************************************************************************/
/*                          CPLURLAddKVP()                              */
/************************************************************************/

/**
 * Return a new URL with a new key=value pair.
 *
 * @param pszURL the URL.
 * @param pszKey the key to find.
 * @param pszValue the value of the key (may be NULL to unset an existing KVP).
 * @return the modified URL.
 * @since GDAL 1.9.0
 */
CPLString CPLURLAddKVP(const char* pszURL, const char* pszKey,
                       const char* pszValue)
{
    CPLString osURL(pszURL);
    if (strchr(osURL, '?') == NULL)
        osURL += "?";
    pszURL = osURL.c_str();

    CPLString osKey(pszKey);
    osKey += "=";
    size_t nKeyPos = osURL.ifind(osKey);
    if (nKeyPos != std::string::npos && nKeyPos > 0 &&
        (pszURL[nKeyPos-1] == '?' || pszURL[nKeyPos-1] == '&'))
    {
        CPLString osNewURL(osURL);
        osNewURL.resize(nKeyPos);
        if (pszValue)
        {
            osNewURL += osKey;
            osNewURL += pszValue;
        }
        const char* pszNext = strchr(pszURL + nKeyPos, '&');
        if (pszNext)
        {
            if (osNewURL[osNewURL.size()-1] == '&'
                || osNewURL[osNewURL.size()-1] == '?' )
                osNewURL += pszNext + 1;
            else
                osNewURL += pszNext;
        }
        return osNewURL;
    }
    else
    {
        if (pszValue)
        {
            if (osURL[osURL.size()-1] != '&' && osURL[osURL.size()-1] != '?')
                osURL += '&';
            osURL += osKey;
            osURL += pszValue;
        }
        return osURL;
    }
}

/************************************************************************/
/*                            CPLOPrintf()                              */
/************************************************************************/

CPLString CPLOPrintf( const char *pszFormat, ... )

{
    va_list args;
    CPLString osTarget;

    va_start( args, pszFormat );
    osTarget.vPrintf( pszFormat, args );
    va_end( args );

    return osTarget;
}

/************************************************************************/
/*                            CPLOvPrintf()                             */
/************************************************************************/

CPLString CPLOvPrintf( const char *pszFormat, va_list args )

{
    CPLString osTarget;
    osTarget.vPrintf( pszFormat, args);
    return osTarget;
}
