/**********************************************************************
 * $Id$
 *
 * Name:     cpl_recode.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Character set recoding and char/wchar_t conversions.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 **********************************************************************
 * Copyright (c) 2011, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2008, Frank Warmerdam
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

#include "cpl_string.h"

CPL_CVSID("$Id$");

#ifdef CPL_RECODE_ICONV
extern char *CPLRecodeIconv( const char *, const char *, const char * );
extern char *CPLRecodeFromWCharIconv( const wchar_t *,
                                      const char *, const char * );
extern wchar_t *CPLRecodeToWCharIconv( const char *,
                                       const char *, const char * );
#endif /* CPL_RECODE_ICONV */

extern char *CPLRecodeStub( const char *, const char *, const char * );
extern char *CPLRecodeFromWCharStub( const wchar_t *,
                                     const char *, const char * );
extern wchar_t *CPLRecodeToWCharStub( const char *,
                                      const char *, const char * );
extern int CPLIsUTF8Stub( const char *, int );

/************************************************************************/
/*                             CPLRecode()                              */
/************************************************************************/

/**
 * Convert a string from a source encoding to a destination encoding.
 *
 * The only guaranteed supported encodings are CPL_ENC_UTF8, CPL_ENC_ASCII
 * and CPL_ENC_ISO8859_1. Currently, the following conversions are supported :
 * <ul>
 *  <li>CPL_ENC_ASCII -> CPL_ENC_UTF8 or CPL_ENC_ISO8859_1 (no conversion in fact)</li>
 *  <li>CPL_ENC_ISO8859_1 -> CPL_ENC_UTF8</li>
 *  <li>CPL_ENC_UTF8 -> CPL_ENC_ISO8859_1</li>
 * </ul>
 *
 * If an error occurs an error may, or may not be posted with CPLError(). 
 *
 * @param pszSource a NULL terminated string.
 * @param pszSrcEncoding the source encoding.
 * @param pszDstEncoding the destination encoding.
 *
 * @return a NULL terminated string which should be freed with CPLFree().
 *
 * @since GDAL 1.6.0
 */

char CPL_DLL *CPLRecode( const char *pszSource,
                         const char *pszSrcEncoding,
                         const char *pszDstEncoding )

{
/* -------------------------------------------------------------------- */
/*      Handle a few common short cuts.                                 */
/* -------------------------------------------------------------------- */
    if ( EQUAL(pszSrcEncoding, pszDstEncoding) )
        return CPLStrdup(pszSource);

    if ( EQUAL(pszSrcEncoding, CPL_ENC_ASCII) 
        && ( EQUAL(pszDstEncoding, CPL_ENC_UTF8) 
             || EQUAL(pszDstEncoding, CPL_ENC_ISO8859_1) ) )
        return CPLStrdup(pszSource);

#ifdef CPL_RECODE_ICONV
/* -------------------------------------------------------------------- */
/*      CPL_ENC_ISO8859_1 -> CPL_ENC_UTF8                               */
/*      and CPL_ENC_UTF8 -> CPL_ENC_ISO8859_1 conversions are hadled    */
/*      very well by the stub implementation which is faster than the   */
/*      iconv() route. Use a stub for these two ones and iconv()        */
/*      everything else.                                                */
/* -------------------------------------------------------------------- */
    if ( ( EQUAL(pszSrcEncoding, CPL_ENC_ISO8859_1)
           && EQUAL(pszDstEncoding, CPL_ENC_UTF8) )
         || ( EQUAL(pszSrcEncoding, CPL_ENC_UTF8)
              && EQUAL(pszDstEncoding, CPL_ENC_ISO8859_1) ) )
    {
        return CPLRecodeStub( pszSource, pszSrcEncoding, pszDstEncoding );
    }
    else
    {
        return CPLRecodeIconv( pszSource, pszSrcEncoding, pszDstEncoding );
    }
#else /* CPL_RECODE_STUB */
    return CPLRecodeStub( pszSource, pszSrcEncoding, pszDstEncoding );
#endif /* CPL_RECODE_ICONV */
}

/************************************************************************/
/*                         CPLRecodeFromWChar()                         */
/************************************************************************/

/**
 * Convert wchar_t string to UTF-8. 
 *
 * Convert a wchar_t string into a multibyte utf-8 string.  The only
 * guaranteed supported source encoding is CPL_ENC_UCS2, and the only
 * guaranteed supported destination encodings are CPL_ENC_UTF8, CPL_ENC_ASCII
 * and CPL_ENC_ISO8859_1.  In some cases (ie. using iconv()) other encodings 
 * may also be supported.
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
 *
 * @since GDAL 1.6.0
 */

char CPL_DLL *CPLRecodeFromWChar( const wchar_t *pwszSource,
                                  const char *pszSrcEncoding,
                                  const char *pszDstEncoding )

{
#ifdef CPL_RECODE_ICONV
/* -------------------------------------------------------------------- */
/*      Conversions from CPL_ENC_UCS2                                   */
/*      to CPL_ENC_UTF8, CPL_ENC_ISO8859_1 and CPL_ENC_ASCII are well   */
/*      handled by the stub implementation.                             */
/* -------------------------------------------------------------------- */
    if ( EQUAL(pszSrcEncoding, CPL_ENC_UCS2)
         && ( EQUAL(pszDstEncoding, CPL_ENC_UTF8)
              || EQUAL(pszDstEncoding, CPL_ENC_ASCII)
              || EQUAL(pszDstEncoding, CPL_ENC_ISO8859_1) ) )
    {
        return CPLRecodeFromWCharStub( pwszSource,
                                       pszSrcEncoding, pszDstEncoding );
    }
    else
    {
        return CPLRecodeFromWCharIconv( pwszSource,
                                        pszSrcEncoding, pszDstEncoding );
    }
#else /* CPL_RECODE_STUB */
    return CPLRecodeFromWCharStub( pwszSource,
                                   pszSrcEncoding, pszDstEncoding );
#endif /* CPL_RECODE_ICONV */
}

/************************************************************************/
/*                          CPLRecodeToWChar()                          */
/************************************************************************/

/**
 * Convert UTF-8 string to a wchar_t string.
 *
 * Convert a 8bit, multi-byte per character input string into a wide
 * character (wchar_t) string.  The only guaranteed supported source encodings
 * are CPL_ENC_UTF8, CPL_ENC_ASCII and CPL_ENC_ISO8869_1 (LATIN1).  The only
 * guaranteed supported destination encoding is CPL_ENC_UCS2.  Other source
 * and destination encodings may be supported depending on the underlying
 * implementation. 
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
 *
 * @since GDAL 1.6.0
 */

wchar_t CPL_DLL *CPLRecodeToWChar( const char *pszSource,
                                   const char *pszSrcEncoding,
                                   const char *pszDstEncoding )

{
#ifdef CPL_RECODE_ICONV
/* -------------------------------------------------------------------- */
/*      Conversions to CPL_ENC_UCS2                                     */
/*      from CPL_ENC_UTF8, CPL_ENC_ISO8859_1 and CPL_ENC_ASCII are well */
/*      handled by the stub implementation.                             */
/* -------------------------------------------------------------------- */
    if ( EQUAL(pszDstEncoding, CPL_ENC_UCS2)
         && ( EQUAL(pszSrcEncoding, CPL_ENC_UTF8)
              || EQUAL(pszSrcEncoding, CPL_ENC_ASCII)
              || EQUAL(pszSrcEncoding, CPL_ENC_ISO8859_1) ) )
    {
        return CPLRecodeToWCharStub( pszSource,
                                     pszSrcEncoding, pszDstEncoding );
    }
    else
    {
        return CPLRecodeToWCharIconv( pszSource,
                                      pszSrcEncoding, pszDstEncoding );
    }
#else /* CPL_RECODE_STUB */
    return CPLRecodeToWCharStub( pszSource, pszSrcEncoding, pszDstEncoding );
#endif /* CPL_RECODE_ICONV */
}

/************************************************************************/
/*                                 CPLIsUTF8()                          */
/************************************************************************/

/**
 * Test if a string is encoded as UTF-8.
 *
 * @param pabyData input string to test
 * @param nLen length of the input string, or -1 if the function must compute
 *             the string length. In which case it must be null terminated.
 * @return TRUE if the string is encoded as UTF-8. FALSE otherwise
 *
 * @since GDAL 1.7.0
 */
int CPLIsUTF8(const char* pabyData, int nLen)
{
    return CPLIsUTF8Stub( pabyData, nLen );
}

/************************************************************************/
/*                          CPLForceToASCII()                           */
/************************************************************************/

/**
 * Return a new string that is made only of ASCII characters. If non-ASCII
 * characters are found in the input string, they will be replaced by the
 * provided replacement character.
 *
 * @param pabyData input string to test
 * @param nLen length of the input string, or -1 if the function must compute
 *             the string length. In which case it must be null terminated.
 * @param chReplacementChar character which will be used when the input stream
 *                          contains a non ASCII character. Must be valid ASCII !
 *
 * @return a new string that must be freed with CPLFree().
 *
 * @since GDAL 1.7.0
 */
char CPL_DLL *CPLForceToASCII(const char* pabyData, int nLen, char chReplacementChar)
{
    if (nLen < 0)
        nLen = strlen(pabyData);
    char* pszOutputString = (char*)CPLMalloc(nLen + 1);
    int i;
    for(i=0;i<nLen;i++)
    {
        if (((unsigned char*)pabyData)[i] > 127)
            pszOutputString[i] = chReplacementChar;
        else
            pszOutputString[i] = pabyData[i];
    }
    pszOutputString[i] = '\0';
    return pszOutputString;
}
