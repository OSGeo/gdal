/**********************************************************************
 * $Id$
 *
 * Name:     cpl_recode_iconv.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Character set recoding and char/wchar_t conversions implemented
 *           using the iconv() functionality.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 **********************************************************************
 * Copyright (c) 2011, Andrey Kiselev <dron@ak4719.spb.edu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **********************************************************************/

#include "cpl_port.h"

CPL_CVSID("$Id$");

#ifdef CPL_RECODE_ICONV

#include <iconv.h>
#include "cpl_string.h"

#ifndef ICONV_CPP_CONST
#define ICONV_CPP_CONST ICONV_CONST
#endif

#define CPL_RECODE_DSTBUF_SIZE 32768

/************************************************************************/
/*                          CPLRecodeIconv()                            */
/************************************************************************/

/**
 * Convert a string from a source encoding to a destination encoding
 * using the iconv() function.
 *
 * If an error occurs an error may, or may not be posted with CPLError(). 
 *
 * @param pszSource a NULL terminated string.
 * @param pszSrcEncoding the source encoding.
 * @param pszDstEncoding the destination encoding.
 *
 * @return a NULL terminated string which should be freed with CPLFree().
 */

char *CPLRecodeIconv( const char *pszSource, 
                      const char *pszSrcEncoding, 
                      const char *pszDstEncoding )

{
    iconv_t sConv;

    sConv = iconv_open( pszDstEncoding, pszSrcEncoding );

    if ( sConv == (iconv_t)-1 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Recode from %s to %s failed with the error: \"%s\".", 
                  pszSrcEncoding, pszDstEncoding, strerror(errno) );

        return CPLStrdup(pszSource);
    }

/* -------------------------------------------------------------------- */
/*      XXX: There is a portability issue: iconv() function could be    */
/*      declared differently on different platforms. The second         */
/*      argument could be declared as char** (as POSIX defines) or      */
/*      as a const char**. Handle it with the ICONV_CPP_CONST macro here.   */
/* -------------------------------------------------------------------- */
    ICONV_CPP_CONST char *pszSrcBuf = (ICONV_CPP_CONST char *)pszSource;
    size_t  nSrcLen = strlen( pszSource );
    size_t  nDstCurLen = MAX(CPL_RECODE_DSTBUF_SIZE, nSrcLen + 1);
    size_t  nDstLen = nDstCurLen;
    char    *pszDestination = (char *)CPLCalloc( nDstCurLen, sizeof(char) );
    char    *pszDstBuf = pszDestination;

    while ( nSrcLen > 0 )
    {
        size_t  nConverted =
            iconv( sConv, &pszSrcBuf, &nSrcLen, &pszDstBuf, &nDstLen );

        if ( nConverted == (size_t)-1 )
        {
            if ( errno == EILSEQ )
            {
                // Skip the invalid sequence in the input string.
                static int bHasWarned = FALSE;
                if (!bHasWarned)
                {
                    bHasWarned = TRUE;
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "One or several characters couldn't be converted correctly from %s to %s.\n"
                            "This warning will not be emitted anymore",
                             pszSrcEncoding, pszDstEncoding);
                }
                nSrcLen--, pszSrcBuf++;
                continue;
            }

            else if ( errno == E2BIG )
            {
                // We are running out of the output buffer.
                // Dynamically increase the buffer size.
                size_t nTmp = nDstCurLen;
                nDstCurLen *= 2;
                pszDestination =
                    (char *)CPLRealloc( pszDestination, nDstCurLen );
                pszDstBuf = pszDestination + nTmp - nDstLen;
                nDstLen += nDstCurLen - nTmp;
                continue;
            }

            else
                break;
        }
    }

    pszDestination[nDstCurLen - nDstLen] = '\0';

    iconv_close( sConv );

    return pszDestination;
}

/************************************************************************/
/*                      CPLRecodeFromWCharIconv()                       */
/************************************************************************/

/**
 * Convert wchar_t string to UTF-8. 
 *
 * Convert a wchar_t string into a multibyte utf-8 string
 * using the iconv() function.
 *
 * Note that the wchar_t type varies in size on different systems. On
 * win32 it is normally 2 bytes, and on unix 4 bytes.
 *
 * If an error occurs an error may, or may not be posted with CPLError(). 
 *
 * @param pwszSource the source wchar_t string, terminated with a 0 wchar_t.
 * @param pszSrcEncoding the source encoding, typically CPL_ENC_UCS2.
 * @param pszDstEncoding the destination encoding, typically CPL_ENC_UTF8.
 *
 * @return a zero terminated multi-byte string which should be freed with 
 * CPLFree(), or NULL if an error occurs. 
 */

char *CPLRecodeFromWCharIconv( const wchar_t *pwszSource, 
                               const char *pszSrcEncoding, 
                               const char *pszDstEncoding )

{
/* -------------------------------------------------------------------- */
/*      What is the source length.                                      */
/* -------------------------------------------------------------------- */
    size_t  nSrcLen = 0;

    while ( pwszSource[nSrcLen] != 0 )
        nSrcLen++;

/* -------------------------------------------------------------------- */
/*      iconv() does not support wchar_t so we need to repack the       */
/*      characters according to the width of a character in the         */
/*      source encoding.  For instance if wchar_t is 4 bytes but our    */
/*      source is UTF16 then we need to pack down into 2 byte           */
/*      characters before passing to iconv().                           */
/* -------------------------------------------------------------------- */
    int nTargetCharWidth = CPLEncodingCharSize( pszSrcEncoding );

    if( nTargetCharWidth < 1 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Recode from %s with CPLRecodeFromWChar() failed because"
                  " the width of characters in the encoding are not known.",
                  pszSrcEncoding );
        return CPLStrdup("");
    }

    GByte *pszIconvSrcBuf = (GByte*) CPLCalloc((nSrcLen+1),nTargetCharWidth);
    unsigned int iSrc;

    for( iSrc = 0; iSrc <= nSrcLen; iSrc++ )
    {
        if( nTargetCharWidth == 1 )
            pszIconvSrcBuf[iSrc] = (GByte) pwszSource[iSrc];
        else if( nTargetCharWidth == 2 )
            ((short *)pszIconvSrcBuf)[iSrc] = (short) pwszSource[iSrc];
        else if( nTargetCharWidth == 4 )
            ((GInt32 *)pszIconvSrcBuf)[iSrc] = pwszSource[iSrc];
    }

/* -------------------------------------------------------------------- */
/*      Create the iconv() translation object.                          */
/* -------------------------------------------------------------------- */
    iconv_t sConv;

    sConv = iconv_open( pszDstEncoding, pszSrcEncoding );

    if ( sConv == (iconv_t)-1 )
    {
        CPLFree( pszIconvSrcBuf );
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Recode from %s to %s failed with the error: \"%s\".", 
                  pszSrcEncoding, pszDstEncoding, strerror(errno) );

        return CPLStrdup( "" );
    }

/* -------------------------------------------------------------------- */
/*      XXX: There is a portability issue: iconv() function could be    */
/*      declared differently on different platforms. The second         */
/*      argument could be declared as char** (as POSIX defines) or      */
/*      as a const char**. Handle it with the ICONV_CPP_CONST macro here.   */
/* -------------------------------------------------------------------- */
    ICONV_CPP_CONST char *pszSrcBuf = (ICONV_CPP_CONST char *) pszIconvSrcBuf;

    /* iconv expects a number of bytes, not characters */
    nSrcLen *= sizeof(wchar_t);

/* -------------------------------------------------------------------- */
/*      Allocate destination buffer.                                    */
/* -------------------------------------------------------------------- */
    size_t  nDstCurLen = MAX(CPL_RECODE_DSTBUF_SIZE, nSrcLen + 1);
    size_t  nDstLen = nDstCurLen;
    char    *pszDestination = (char *)CPLCalloc( nDstCurLen, sizeof(char) );
    char    *pszDstBuf = pszDestination;

    while ( nSrcLen > 0 )
    {
        size_t  nConverted =
            iconv( sConv, &pszSrcBuf, &nSrcLen, &pszDstBuf, &nDstLen );

        if ( nConverted == (size_t)-1 )
        {
            if ( errno == EILSEQ )
            {
                // Skip the invalid sequence in the input string.
                nSrcLen--;
                pszSrcBuf += sizeof(wchar_t);
                static int bHasWarned = FALSE;
                if (!bHasWarned)
                {
                    bHasWarned = TRUE;
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "One or several characters couldn't be converted correctly from %s to %s.\n"
                            "This warning will not be emitted anymore",
                             pszSrcEncoding, pszDstEncoding);
                }
                continue;
            }

            else if ( errno == E2BIG )
            {
                // We are running out of the output buffer.
                // Dynamically increase the buffer size.
                size_t nTmp = nDstCurLen;
                nDstCurLen *= 2;
                pszDestination =
                    (char *)CPLRealloc( pszDestination, nDstCurLen );
                pszDstBuf = pszDestination + nTmp - nDstLen;
                nDstLen += nDstCurLen - nTmp;
                continue;
            }

            else
                break;
        }
    }

    pszDestination[nDstCurLen - nDstLen] = '\0';

    iconv_close( sConv );

    CPLFree( pszIconvSrcBuf );

    return pszDestination;
}

/************************************************************************/
/*                        CPLRecodeToWCharIconv()                       */
/************************************************************************/

/**
 * Convert UTF-8 string to a wchar_t string.
 *
 * Convert a 8bit, multi-byte per character input string into a wide
 * character (wchar_t) string using the iconv() function.
 *
 * Note that the wchar_t type varies in size on different systems. On
 * win32 it is normally 2 bytes, and on unix 4 bytes.
 *
 * If an error occurs an error may, or may not be posted with CPLError(). 
 *
 * @param pszSource input multi-byte character string.
 * @param pszSrcEncoding source encoding, typically CPL_ENC_UTF8.
 * @param pszDstEncoding destination encoding, typically CPL_ENC_UCS2. 
 *
 * @return the zero terminated wchar_t string (to be freed with CPLFree()) or
 * NULL on error.
 */

wchar_t *CPLRecodeToWCharIconv( const char *pszSource,
                                const char *pszSrcEncoding, 
                                const char *pszDstEncoding )

{
    return (wchar_t *)CPLRecodeIconv( pszSource,
                                      pszSrcEncoding, pszDstEncoding);
}

#endif /* CPL_RECODE_ICONV */
