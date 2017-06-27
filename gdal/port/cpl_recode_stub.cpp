/**********************************************************************
 *
 * Name:     cpl_recode_stub.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Character set recoding and char/wchar_t conversions, stub
 *           implementation to be used if iconv() functionality is not
 *           available.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * The bulk of this code is derived from the utf.c module from FLTK. It
 * was originally downloaded from:
 *    http://svn.easysw.com/public/fltk/fltk/trunk/src/utf.c
 *
 **********************************************************************
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright 2006 by Bill Spitzak and others.
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_string.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

#ifdef CPL_RECODE_STUB

static unsigned utf8decode(const char* p, const char* end, int* len);
static unsigned utf8towc(const char* src, unsigned srclen,
                         wchar_t* dst, unsigned dstlen);
static unsigned utf8toa(const char* src, unsigned srclen,
                        char* dst, unsigned dstlen);
static unsigned utf8fromwc(char* dst, unsigned dstlen,
                           const wchar_t* src, unsigned srclen);
static unsigned utf8froma(char* dst, unsigned dstlen,
                          const char* src, unsigned srclen);
static int utf8test(const char* src, unsigned srclen);

#ifdef _WIN32

#include <windows.h>
#include <winnls.h>

static char* CPLWin32Recode( const char* src, unsigned src_code_page,
                             unsigned dst_code_page )
    CPL_RETURNS_NONNULL;
#endif

/* used by cpl_recode.cpp */
extern void CPLClearRecodeStubWarningFlags();
extern char *CPLRecodeStub( const char *, const char *, const char * )
    CPL_RETURNS_NONNULL;
extern char *CPLRecodeFromWCharStub( const wchar_t *,
                                     const char *, const char * );
extern wchar_t *CPLRecodeToWCharStub( const char *,
                                      const char *, const char * );
extern int CPLIsUTF8Stub( const char *, int );

/************************************************************************/
/* ==================================================================== */
/*      Stub Implementation not depending on iconv() or WIN32 API.      */
/* ==================================================================== */
/************************************************************************/

static bool bHaveWarned1 = false;
static bool bHaveWarned2 = false;
static bool bHaveWarned3 = false;
static bool bHaveWarned4 = false;
static bool bHaveWarned5 = false;
static bool bHaveWarned6 = false;

/************************************************************************/
/*                 CPLClearRecodeStubWarningFlags()                     */
/************************************************************************/

void CPLClearRecodeStubWarningFlags()
{
    bHaveWarned1 = false;
    bHaveWarned2 = false;
    bHaveWarned3 = false;
    bHaveWarned4 = false;
    bHaveWarned5 = false;
    bHaveWarned6 = false;
}

/************************************************************************/
/*                           CPLRecodeStub()                            */
/************************************************************************/

/**
 * Convert a string from a source encoding to a destination encoding.
 *
 * The only guaranteed supported encodings are CPL_ENC_UTF8, CPL_ENC_ASCII
 * and CPL_ENC_ISO8859_1. Currently, the following conversions are supported :
 * <ul>
 *  <li>CPL_ENC_ASCII -> CPL_ENC_UTF8 or CPL_ENC_ISO8859_1 (no conversion in
 *  fact)</li>
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
 */

char *CPLRecodeStub( const char *pszSource,
                     const char *pszSrcEncoding,
                     const char *pszDstEncoding )

{
/* -------------------------------------------------------------------- */
/*      If the source or destination is current locale(), we change     */
/*      it to ISO8859-1 since our stub implementation does not          */
/*      attempt to address locales properly.                            */
/* -------------------------------------------------------------------- */

    if( pszSrcEncoding[0] == '\0' )
        pszSrcEncoding = CPL_ENC_ISO8859_1;

    if( pszDstEncoding[0] == '\0' )
        pszDstEncoding = CPL_ENC_ISO8859_1;

/* -------------------------------------------------------------------- */
/*      ISO8859 to UTF8                                                 */
/* -------------------------------------------------------------------- */
    if( strcmp(pszSrcEncoding, CPL_ENC_ISO8859_1) == 0
        && strcmp(pszDstEncoding, CPL_ENC_UTF8) == 0 )
    {
        const int nCharCount = static_cast<int>(strlen(pszSource));
        char *pszResult = static_cast<char *>(CPLCalloc(1, nCharCount * 2 + 1));

        utf8froma(pszResult, nCharCount * 2 + 1, pszSource, nCharCount);

        return pszResult;
    }

/* -------------------------------------------------------------------- */
/*      UTF8 to ISO8859                                                 */
/* -------------------------------------------------------------------- */
    if( strcmp(pszSrcEncoding, CPL_ENC_UTF8) == 0
        && strcmp(pszDstEncoding, CPL_ENC_ISO8859_1) == 0 )
    {
        int nCharCount = static_cast<int>(strlen(pszSource));
        char *pszResult = static_cast<char *>(CPLCalloc(1, nCharCount + 1));

        utf8toa(pszSource, nCharCount, pszResult, nCharCount + 1);

        return pszResult;
    }

#ifdef _WIN32
/* ---------------------------------------------------------------------*/
/*      CPXXX to UTF8                                                   */
/* ---------------------------------------------------------------------*/
    if( STARTS_WITH(pszSrcEncoding, "CP")
        && strcmp(pszDstEncoding, CPL_ENC_UTF8) == 0 )
    {
        int nCode = atoi( pszSrcEncoding + 2 );
        if( nCode > 0 ) {
           return CPLWin32Recode( pszSource, nCode, CP_UTF8 );
        }
        else if( EQUAL(pszSrcEncoding, "CP_OEMCP") )
            return CPLWin32Recode( pszSource, CP_OEMCP, CP_UTF8 );
    }

/* ---------------------------------------------------------------------*/
/*      UTF8 to CPXXX                                                   */
/* ---------------------------------------------------------------------*/
    if( strcmp(pszSrcEncoding, CPL_ENC_UTF8) == 0
        && STARTS_WITH(pszDstEncoding, "CP") )
    {
         int nCode = atoi( pszDstEncoding + 2 );
         if( nCode > 0 ) {
             return CPLWin32Recode( pszSource, CP_UTF8, nCode );
         }
    }
#endif

/* -------------------------------------------------------------------- */
/*      Anything else to UTF-8 is treated as ISO8859-1 to UTF-8 with    */
/*      a one-time warning.                                             */
/* -------------------------------------------------------------------- */
    if( strcmp(pszDstEncoding, CPL_ENC_UTF8) == 0 )
    {
        int nCharCount = static_cast<int>(strlen(pszSource));
        char *pszResult = static_cast<char *>(CPLCalloc(1, nCharCount * 2 + 1));

        if( EQUAL( pszSrcEncoding, "CP437") ) // For ZIP file handling.
        {
            bool bIsAllPrintableASCII = true;
            for( int i = 0; i <nCharCount; i++ )
            {
                if( pszSource[i] < 32 || pszSource[i] > 126 )
                {
                    bIsAllPrintableASCII = false;
                    break;
                }
            }
            if( bIsAllPrintableASCII )
            {
                if( nCharCount )
                    memcpy( pszResult, pszSource, nCharCount );
                return pszResult;
            }
        }

        if( !bHaveWarned1 )
        {
            bHaveWarned1 = true;
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Recode from %s to UTF-8 not supported, "
                      "treated as ISO8859-1 to UTF-8.",
                      pszSrcEncoding );
        }

        utf8froma( pszResult, nCharCount*2+1, pszSource, nCharCount );

        return pszResult;
    }

/* -------------------------------------------------------------------- */
/*      UTF-8 to anything else is treated as UTF-8 to ISO-8859-1        */
/*      with a warning.                                                 */
/* -------------------------------------------------------------------- */
    if( strcmp(pszSrcEncoding, CPL_ENC_UTF8) == 0
        && strcmp(pszDstEncoding, CPL_ENC_ISO8859_1) == 0 )
    {
        int nCharCount = static_cast<int>(strlen(pszSource));
        char *pszResult = static_cast<char *>(CPLCalloc(1, nCharCount + 1));

        if( !bHaveWarned2 )
        {
            bHaveWarned2 = true;
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Recode from UTF-8 to %s not supported, "
                      "treated as UTF-8 to ISO8859-1.",
                      pszDstEncoding );
        }

        utf8toa(pszSource, nCharCount, pszResult, nCharCount + 1);

        return pszResult;
    }

/* -------------------------------------------------------------------- */
/*      Everything else is treated as a no-op with a warning.           */
/* -------------------------------------------------------------------- */
    {
        if( !bHaveWarned3 )
        {
            bHaveWarned3 = true;
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Recode from %s to %s not supported, no change applied.",
                      pszSrcEncoding, pszDstEncoding );
        }

        return CPLStrdup(pszSource);
    }
}

/************************************************************************/
/*                       CPLRecodeFromWCharStub()                       */
/************************************************************************/

/**
 * Convert wchar_t string to UTF-8.
 *
 * Convert a wchar_t string into a multibyte utf-8 string.  The only
 * guaranteed supported source encoding is CPL_ENC_UCS2, and the only
 * guaranteed supported destination encodings are CPL_ENC_UTF8, CPL_ENC_ASCII
 * and CPL_ENC_ISO8859_1.  In some cases (i.e. using iconv()) other encodings
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
 */

char *CPLRecodeFromWCharStub( const wchar_t *pwszSource,
                              const char *pszSrcEncoding,
                              const char *pszDstEncoding )

{
/* -------------------------------------------------------------------- */
/*      We try to avoid changes of character set.  We are just          */
/*      providing for unicode to unicode.                               */
/* -------------------------------------------------------------------- */
    if( strcmp(pszSrcEncoding, "WCHAR_T") != 0 &&
        strcmp(pszSrcEncoding, CPL_ENC_UTF8) != 0
        && strcmp(pszSrcEncoding, CPL_ENC_UTF16) != 0
        && strcmp(pszSrcEncoding, CPL_ENC_UCS2) != 0
        && strcmp(pszSrcEncoding, CPL_ENC_UCS4) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Stub recoding implementation does not support "
                  "CPLRecodeFromWCharStub(...,%s,%s)",
                  pszSrcEncoding, pszDstEncoding );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      What is the source length.                                      */
/* -------------------------------------------------------------------- */
    int nSrcLen = 0;

    while( pwszSource[nSrcLen] != 0 )
        nSrcLen++;

/* -------------------------------------------------------------------- */
/*      Allocate destination buffer plenty big.                         */
/* -------------------------------------------------------------------- */
    const int nDstBufSize = nSrcLen * 4 + 1;
    // Nearly worst case.
    char *pszResult = static_cast<char *>(CPLMalloc(nDstBufSize));

    if( nSrcLen == 0 )
    {
        pszResult[0] = '\0';
        return pszResult;
    }

/* -------------------------------------------------------------------- */
/*      Convert, and confirm we had enough space.                       */
/* -------------------------------------------------------------------- */
    const int nDstLen =
        utf8fromwc( pszResult, nDstBufSize, pwszSource, nSrcLen );
    if( nDstLen >= nDstBufSize )
    {
        CPLAssert( false ); // too small!
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If something other than UTF-8 was requested, recode now.        */
/* -------------------------------------------------------------------- */
    if( strcmp(pszDstEncoding, CPL_ENC_UTF8) == 0 )
        return pszResult;

    char *pszFinalResult =
        CPLRecodeStub( pszResult, CPL_ENC_UTF8, pszDstEncoding );

    CPLFree( pszResult );

    return pszFinalResult;
}

/************************************************************************/
/*                        CPLRecodeToWCharStub()                        */
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

wchar_t *CPLRecodeToWCharStub( const char *pszSource,
                               const char *pszSrcEncoding,
                               const char *pszDstEncoding )

{
    char *pszUTF8Source = const_cast<char *>(pszSource);

    if( strcmp(pszSrcEncoding, CPL_ENC_UTF8) != 0
        && strcmp(pszSrcEncoding, CPL_ENC_ASCII) != 0 )
    {
        pszUTF8Source =
            CPLRecodeStub(pszSource, pszSrcEncoding, CPL_ENC_UTF8);
        if( pszUTF8Source == NULL )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      We try to avoid changes of character set.  We are just          */
/*      providing for unicode to unicode.                               */
/* -------------------------------------------------------------------- */
    if( strcmp(pszDstEncoding, "WCHAR_T") != 0
        && strcmp(pszDstEncoding, CPL_ENC_UCS2) != 0
        && strcmp(pszDstEncoding, CPL_ENC_UCS4) != 0
        && strcmp(pszDstEncoding, CPL_ENC_UTF16) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Stub recoding implementation does not support "
                  "CPLRecodeToWCharStub(...,%s,%s)",
                  pszSrcEncoding, pszDstEncoding );
        if( pszUTF8Source != pszSource )
            CPLFree( pszUTF8Source );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do the UTF-8 to UCS-2 recoding.                                 */
/* -------------------------------------------------------------------- */
    int nSrcLen = static_cast<int>(strlen(pszUTF8Source));
    wchar_t *pwszResult =
        static_cast<wchar_t *>(CPLCalloc(sizeof(wchar_t), nSrcLen + 1));

    utf8towc( pszUTF8Source, nSrcLen, pwszResult, nSrcLen+1 );

    if( pszUTF8Source != pszSource )
        CPLFree( pszUTF8Source );

    return pwszResult;
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
int CPLIsUTF8Stub(const char* pabyData, int nLen)
{
    if( nLen < 0 )
        nLen = static_cast<int>(strlen(pabyData));
    return utf8test(pabyData, (unsigned)nLen) != 0;
}

/************************************************************************/
/* ==================================================================== */
/*      UTF.C code from FLTK with some modifications.                   */
/* ==================================================================== */
/************************************************************************/

/* Set to 1 to turn bad UTF8 bytes into ISO-8859-1. If this is to zero
   they are instead turned into the Unicode REPLACEMENT CHARACTER, of
   value 0xfffd.
   If this is on utf8decode will correctly map most (perhaps all)
   human-readable text that is in ISO-8859-1. This may allow you
   to completely ignore character sets in your code because virtually
   everything is either ISO-8859-1 or UTF-8.
*/
#define ERRORS_TO_ISO8859_1 1

/* Set to 1 to turn bad UTF8 bytes in the 0x80-0x9f range into the
   Unicode index for Microsoft's CP1252 character set. You should
   also set ERRORS_TO_ISO8859_1. With this a huge amount of more
   available text (such as all web pages) are correctly converted
   to Unicode.
*/
#define ERRORS_TO_CP1252 1

/* A number of Unicode code points are in fact illegal and should not
   be produced by a UTF-8 converter. Turn this on will replace the
   bytes in those encodings with errors. If you do this then converting
   arbitrary 16-bit data to UTF-8 and then back is not an identity,
   which will probably break a lot of software.
*/
#define STRICT_RFC3629 0

#if ERRORS_TO_CP1252
// Codes 0x80..0x9f from the Microsoft CP1252 character set, translated
// to Unicode:
static const unsigned short cp1252[32] = {
    0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
    0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
    0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178
};
#endif

/************************************************************************/
/*                             utf8decode()                             */
/************************************************************************/

/*
    Decode a single UTF-8 encoded character starting at \e p. The
    resulting Unicode value (in the range 0-0x10ffff) is returned,
    and \e len is set the number of bytes in the UTF-8 encoding
    (adding \e len to \e p will point at the next character).

    If \a p points at an illegal UTF-8 encoding, including one that
    would go past \e end, or where a code is uses more bytes than
    necessary, then *(unsigned char*)p is translated as though it is
    in the Microsoft CP1252 character set and \e len is set to 1.
    Treating errors this way allows this to decode almost any
    ISO-8859-1 or CP1252 text that has been mistakenly placed where
    UTF-8 is expected, and has proven very useful.

    If you want errors to be converted to error characters (as the
    standards recommend), adding a test to see if the length is
    unexpectedly 1 will work:

\code
    if( *p & 0x80 )
    {  // What should be a multibyte encoding.
      code = utf8decode(p, end, &len);
      if( len<2 ) code = 0xFFFD;  // Turn errors into REPLACEMENT CHARACTER.
    }
    else
    {  // Handle the 1-byte utf8 encoding:
      code = *p;
      len = 1;
    }
\endcode

    Direct testing for the 1-byte case (as shown above) will also
    speed up the scanning of strings where the majority of characters
    are ASCII.
*/
static unsigned utf8decode(const char* p, const char* end, int* len)
{
  unsigned char c = *(unsigned char*)p;
  if( c < 0x80 )
  {
    *len = 1;
    return c;
#if ERRORS_TO_CP1252
  }
  else if( c < 0xa0 )
  {
    *len = 1;
    return cp1252[c-0x80];
#endif
  }
  else if( c < 0xc2 )
  {
    goto FAIL;
  }
  if( p+1 >= end || (p[1] & 0xc0) != 0x80 ) goto FAIL;
  if( c < 0xe0 )
  {
    *len = 2;
    return
      ((p[0] & 0x1f) << 6) +
      ((p[1] & 0x3f));
  }
  else if( c == 0xe0 )
  {
    if( ((unsigned char*)p)[1] < 0xa0 ) goto FAIL;
    goto UTF8_3;
#if STRICT_RFC3629
  }
  else if( c == 0xed )
  {
    // RFC 3629 says surrogate chars are illegal.
    if( ((unsigned char*)p)[1] >= 0xa0 ) goto FAIL;
    goto UTF8_3;
  }
  else if( c == 0xef )
  {
    // 0xfffe and 0xffff are also illegal characters.
    if( ((unsigned char*)p)[1]==0xbf &&
        ((unsigned char*)p)[2]>=0xbe ) goto FAIL;
    goto UTF8_3;
#endif
  }
  else if( c < 0xf0 )
  {
  UTF8_3:
    if( p+2 >= end || (p[2]&0xc0) != 0x80 ) goto FAIL;
    *len = 3;
    return
      ((p[0] & 0x0f) << 12) +
      ((p[1] & 0x3f) << 6) +
      ((p[2] & 0x3f));
  }
  else if( c == 0xf0 )
  {
    if( ((unsigned char*)p)[1] < 0x90 ) goto FAIL;
    goto UTF8_4;
  }
  else if( c < 0xf4 )
  {
  UTF8_4:
    if( p+3 >= end || (p[2]&0xc0) != 0x80 || (p[3]&0xc0) != 0x80 ) goto FAIL;
    *len = 4;
#if STRICT_RFC3629
    // RFC 3629 says all codes ending in fffe or ffff are illegal:
    if( (p[1]&0xf)==0xf &&
        ((unsigned char*)p)[2] == 0xbf &&
        ((unsigned char*)p)[3] >= 0xbe ) goto FAIL;
#endif
    return
      ((p[0] & 0x07) << 18) +
      ((p[1] & 0x3f) << 12) +
      ((p[2] & 0x3f) << 6) +
      ((p[3] & 0x3f));
  }
  else if( c == 0xf4 )
  {
    if( ((unsigned char*)p)[1] > 0x8f ) goto FAIL; // After 0x10ffff.
    goto UTF8_4;
  }
  else
  {
  FAIL:
    *len = 1;
#if ERRORS_TO_ISO8859_1
    return c;
#else
    return 0xfffd; // Unicode REPLACEMENT CHARACTER
#endif
  }
}

/************************************************************************/
/*                              utf8towc()                              */
/************************************************************************/

/*  Convert a UTF-8 sequence into an array of wchar_t. These
    are used by some system calls, especially on Windows.

    \a src points at the UTF-8, and \a srclen is the number of bytes to
    convert.

    \a dst points at an array to write, and \a dstlen is the number of
    locations in this array. At most \a dstlen-1 words will be
    written there, plus a 0 terminating word. Thus this function
    will never overwrite the buffer and will always return a
    zero-terminated string. If \a dstlen is zero then \a dst can be
    null and no data is written, but the length is returned.

    The return value is the number of words that \e would be written
    to \a dst if it were long enough, not counting the terminating
    zero. If the return value is greater or equal to \a dstlen it
    indicates truncation, you can then allocate a new array of size
    return+1 and call this again.

    Errors in the UTF-8 are converted as though each byte in the
    erroneous string is in the Microsoft CP1252 encoding. This allows
    ISO-8859-1 text mistakenly identified as UTF-8 to be printed
    correctly.

    Notice that sizeof(wchar_t) is 2 on Windows and is 4 on Linux
    and most other systems. Where wchar_t is 16 bits, Unicode
    characters in the range 0x10000 to 0x10ffff are converted to
    "surrogate pairs" which take two words each (this is called UTF-16
    encoding). If wchar_t is 32 bits this rather nasty problem is
    avoided.
*/
static unsigned utf8towc(const char* src, unsigned srclen,
                         wchar_t* dst, unsigned dstlen)
{
  const char* p = src;
  const char* e = src+srclen;
  unsigned count = 0;
  if( dstlen ) while( true )
  {
    if( p >= e )
    {
        dst[count] = 0;
        return count;
    }
    if( !(*p & 0x80) )
    {
        // ASCII
        dst[count] = *p++;
    }
    else
    {
      int len = 0;
      unsigned ucs = utf8decode(p, e, &len);
      p += len;
#ifdef _WIN32
      if( ucs < 0x10000 )
      {
          dst[count] = (wchar_t)ucs;
      }
      else
      {
        // Make a surrogate pair:
        if( count+2 >= dstlen)
        {
            dst[count] = 0;
            count += 2;
            break;
        }
        dst[count] = (wchar_t)((((ucs-0x10000u)>>10)&0x3ff) | 0xd800);
        dst[++count] = (wchar_t)((ucs&0x3ff) | 0xdc00);
      }
#else
      dst[count] = (wchar_t)ucs;
#endif
    }
    if( ++count == dstlen )
    {
        dst[count-1] = 0;
        break;
    }
  }
  // We filled dst, measure the rest:
  while( p < e )
  {
    if( !(*p & 0x80) )
    {
        p++;
    }
    else
    {
      int len = 0;
#ifdef _WIN32
      const unsigned ucs = utf8decode(p, e, &len);
      p += len;
      if( ucs >= 0x10000 ) ++count;
#else
      utf8decode(p, e, &len);
      p += len;
#endif
    }
    ++count;
  }

  return count;
}

/************************************************************************/
/*                              utf8toa()                               */
/************************************************************************/
/* Convert a UTF-8 sequence into an array of 1-byte characters.

    If the UTF-8 decodes to a character greater than 0xff then it is
    replaced with '?'.

    Errors in the UTF-8 are converted as individual bytes, same as
    utf8decode() does. This allows ISO-8859-1 text mistakenly identified
    as UTF-8 to be printed correctly (and possibly CP1512 on Windows).

    \a src points at the UTF-8, and \a srclen is the number of bytes to
    convert.

    Up to \a dstlen bytes are written to \a dst, including a null
    terminator. The return value is the number of bytes that would be
    written, not counting the null terminator. If greater or equal to
    \a dstlen then if you malloc a new array of size n+1 you will have
    the space needed for the entire string. If \a dstlen is zero then
    nothing is written and this call just measures the storage space
    needed.
*/
static unsigned int utf8toa( const char* src, unsigned srclen,
                             char* dst, unsigned dstlen )
{
  const char* p = src;
  const char* e = src+srclen;
  unsigned int count = 0;
  if( dstlen ) while( true )
  {
    if( p >= e )
    {
        dst[count] = 0;
        return count;
    }
    unsigned char c = *(unsigned char*)p;
    if( c < 0xC2 )
    {
        // ASCII or bad code.
        dst[count] = c;
        p++;
    }
    else
    {
        int len = 0;
        const unsigned int ucs = utf8decode(p, e, &len);
        p += len;
        if( ucs < 0x100 )
        {
            dst[count] = static_cast<char>(ucs);
        }
        else
        {
            if( !bHaveWarned4 )
            {
                bHaveWarned4 = true;
                CPLError(CE_Warning, CPLE_AppDefined,
                         "One or several characters couldn't be converted "
                         "correctly from UTF-8 to ISO-8859-1.  "
                         "This warning will not be emitted anymore.");
            }
            dst[count] = '?';
      }
    }
    if( ++count >= dstlen )
    {
        dst[count-1] = 0;
        break;
    }
  }
  // We filled dst, measure the rest:
  while( p < e )
  {
    if( !(*p & 0x80) )
    {
        p++;
    }
    else
    {
        int len = 0;
        utf8decode(p, e, &len);
        p += len;
    }
    ++count;
  }
  return count;
}

/************************************************************************/
/*                             utf8fromwc()                             */
/************************************************************************/
/* Turn "wide characters" as returned by some system calls
    (especially on Windows) into UTF-8.

    Up to \a dstlen bytes are written to \a dst, including a null
    terminator. The return value is the number of bytes that would be
    written, not counting the null terminator. If greater or equal to
    \a dstlen then if you malloc a new array of size n+1 you will have
    the space needed for the entire string. If \a dstlen is zero then
    nothing is written and this call just measures the storage space
    needed.

    \a srclen is the number of words in \a src to convert. On Windows
    this is not necessarily the number of characters, due to there
    possibly being "surrogate pairs" in the UTF-16 encoding used.
    On Unix wchar_t is 32 bits and each location is a character.

    On Unix if a src word is greater than 0x10ffff then this is an
    illegal character according to RFC 3629. These are converted as
    though they are 0xFFFD (REPLACEMENT CHARACTER). Characters in the
    range 0xd800 to 0xdfff, or ending with 0xfffe or 0xffff are also
    illegal according to RFC 3629. However I encode these as though
    they are legal, so that utf8towc will return the original data.

    On Windows "surrogate pairs" are converted to a single character
    and UTF-8 encoded (as 4 bytes). Mismatched halves of surrogate
    pairs are converted as though they are individual characters.
*/
static unsigned int utf8fromwc( char* dst, unsigned dstlen,
                                const wchar_t* src, unsigned srclen )
{
  unsigned int i = 0;
  unsigned int count = 0;
  if( dstlen ) while( true )
  {
      if( i >= srclen )
      {
          dst[count] = 0;
          return count;
      }
      unsigned int ucs = src[i++];
      if( ucs < 0x80U )
      {
          dst[count++] = static_cast<char>(ucs);
          if( count >= dstlen )
          {
              dst[count-1] = 0;
              break;
          }
      }
      else if( ucs < 0x800U )
      {
          // 2 bytes.
          if( count+2 >= dstlen )
          {
              dst[count] = 0;
              count += 2;
              break;
          }
          dst[count++] = 0xc0 | static_cast<char>(ucs >> 6);
          dst[count++] = 0x80 | static_cast<char>(ucs & 0x3F);
#ifdef _WIN32
      }
      else if( ucs >= 0xd800 && ucs <= 0xdbff && i < srclen &&
               src[i] >= 0xdc00 && src[i] <= 0xdfff)
      {
          // Surrogate pair.
          unsigned int ucs2 = src[i++];
          ucs = 0x10000U + ((ucs & 0x3ff) << 10) + (ucs2 & 0x3ff);
          // All surrogate pairs turn into 4-byte utf8.
#else
      }
      else if( ucs >= 0x10000 )
      {
          if( ucs > 0x10ffff )
          {
              ucs = 0xfffd;
              goto J1;
          }
#endif
          if( count+4 >= dstlen )
          {
              dst[count] = 0;
              count += 4;
              break;
          }
          dst[count++] = 0xf0 | static_cast<char>(ucs >> 18);
          dst[count++] = 0x80 | static_cast<char>((ucs >> 12) & 0x3F);
          dst[count++] = 0x80 | static_cast<char>((ucs >> 6) & 0x3F);
          dst[count++] = 0x80 | static_cast<char>(ucs & 0x3F);
      }
      else
      {
#ifndef _WIN32
    J1:
#endif
      // All others are 3 bytes:
          if( count+3 >= dstlen )
          {
              dst[count] = 0;
              count += 3;
              break;
          }
          dst[count++] = 0xe0 | static_cast<char>(ucs >> 12);
          dst[count++] = 0x80 | static_cast<char>((ucs >> 6) & 0x3F);
          dst[count++] = 0x80 | static_cast<char>(ucs & 0x3F);
      }
  }

  // We filled dst, measure the rest:
  while( i < srclen )
  {
      unsigned int ucs = src[i++];
      if( ucs < 0x80U )
      {
          count++;
      }
      else if( ucs < 0x800U )
      {
          // 2 bytes.
          count += 2;
#ifdef _WIN32
      }
      else if( ucs >= 0xd800 && ucs <= 0xdbff && i < srclen-1 &&
               src[i+1] >= 0xdc00 && src[i+1] <= 0xdfff )
      {
          // Surrogate pair.
          ++i;
#else
      }
      else if( ucs >= 0x10000 && ucs <= 0x10ffff )
      {
#endif
          count += 4;
      }
      else
      {
          count += 3;
      }
  }
  return count;
}

/************************************************************************/
/*                             utf8froma()                              */
/************************************************************************/

/* Convert an ISO-8859-1 (i.e. normal c-string) byte stream to UTF-8.

    It is possible this should convert Microsoft's CP1252 to UTF-8
    instead. This would translate the codes in the range 0x80-0x9f
    to different characters. Currently it does not do this.

    Up to \a dstlen bytes are written to \a dst, including a null
    terminator. The return value is the number of bytes that would be
    written, not counting the null terminator. If greater or equal to
    \a dstlen then if you malloc a new array of size n+1 you will have
    the space needed for the entire string. If \a dstlen is zero then
    nothing is written and this call just measures the storage space
    needed.

    \a srclen is the number of bytes in \a src to convert.

    If the return value equals \a srclen then this indicates that
    no conversion is necessary, as only ASCII characters are in the
    string.
*/
static unsigned utf8froma(char* dst, unsigned dstlen,
                          const char* src, unsigned srclen) {
    const char* p = src;
    const char* e = src+srclen;
    unsigned count = 0;
    if( dstlen ) while( true )
    {
        if( p >= e )
        {
            dst[count] = 0;
            return count;
        }
        unsigned char ucs = *(unsigned char*)p++;
        if( ucs < 0x80U )
        {
            dst[count++] = ucs;
            if( count >= dstlen )
            {
                dst[count-1] = 0;
                break;
            }
        }
        else
        {
            // 2 bytes (note that CP1252 translate could make 3 bytes!)
            if( count+2 >= dstlen )
            {
                dst[count] = 0;
                count += 2;
                break;
            }
            dst[count++] = 0xc0 | (ucs >> 6);
            dst[count++] = 0x80 | (ucs & 0x3F);
        }
    }

    // We filled dst, measure the rest:
    while( p < e )
    {
        unsigned char ucs = *(unsigned char*)p++;
        if( ucs < 0x80U )
        {
            count++;
        }
        else
        {
            count += 2;
        }
    }

    return count;
}

#ifdef _WIN32

/************************************************************************/
/*                            CPLWin32Recode()                          */
/************************************************************************/

/* Convert an CODEPAGE (i.e. normal c-string) byte stream
     to another CODEPAGE (i.e. normal c-string) byte stream.

    \a src is target c-string byte stream (including a null terminator).
    \a src_code_page is target c-string byte code page.
    \a dst_code_page is destination c-string byte code page.

   UTF7          65000
   UTF8          65001
   OEM-US          437
   OEM-ALABIC      720
   OEM-GREEK       737
   OEM-BALTIC      775
   OEM-MLATIN1     850
   OEM-LATIN2      852
   OEM-CYRILLIC    855
   OEM-TURKISH     857
   OEM-MLATIN1P    858
   OEM-HEBREW      862
   OEM-RUSSIAN     866

   THAI            874
   SJIS            932
   GBK             936
   KOREA           949
   BIG5            950

   EUROPE         1250
   CYRILLIC       1251
   LATIN1         1252
   GREEK          1253
   TURKISH        1254
   HEBREW         1255
   ARABIC         1256
   BALTIC         1257
   VIETNAM        1258

   ISO-LATIN1    28591
   ISO-LATIN2    28592
   ISO-LATIN3    28593
   ISO-BALTIC    28594
   ISO-CYRILLIC  28595
   ISO-ARABIC    28596
   ISO-HEBREW    28598
   ISO-TURKISH   28599
   ISO-LATIN9    28605

   ISO-2022-JP   50220

*/

char* CPLWin32Recode( const char* src, unsigned src_code_page,
                      unsigned dst_code_page )
{
    // Convert from source code page to Unicode.

    // Compute the length in wide characters.
    int wlen = MultiByteToWideChar( src_code_page, MB_ERR_INVALID_CHARS, src,
                                    -1, 0, 0 );
    if( wlen == 0 && GetLastError() == ERROR_NO_UNICODE_TRANSLATION )
    {
        if( !bHaveWarned5 )
        {
            bHaveWarned5 = true;
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "One or several characters could not be translated from CP%d. "
                "This warning will not be emitted anymore.", src_code_page);
        }

        // Retry now without MB_ERR_INVALID_CHARS flag.
        wlen = MultiByteToWideChar( src_code_page, 0, src, -1, 0, 0 );
    }

    // Do the actual conversion.
    wchar_t* tbuf =
        static_cast<wchar_t *>(CPLCalloc(sizeof(wchar_t), wlen + 1));
    tbuf[wlen] = 0;
    MultiByteToWideChar( src_code_page, 0, src, -1, tbuf, wlen+1 );

    // Convert from Unicode to destination code page.

    // Compute the length in chars.
    BOOL bUsedDefaultChar = FALSE;
    int len = 0;
    if( dst_code_page == CP_UTF7 || dst_code_page == CP_UTF8 )
        len = WideCharToMultiByte( dst_code_page, 0, tbuf, -1, 0, 0, 0, NULL );
    else
        len = WideCharToMultiByte( dst_code_page, 0, tbuf, -1, 0, 0, 0,
                                   &bUsedDefaultChar );
    if( bUsedDefaultChar )
    {
        if( !bHaveWarned6 )
        {
            bHaveWarned6 = true;
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "One or several characters could not be translated to CP%d. "
                "This warning will not be emitted anymore.", dst_code_page);
        }
    }

    // Do the actual conversion.
    char* pszResult = static_cast<char *>(CPLCalloc(sizeof(char), len + 1));
    WideCharToMultiByte(dst_code_page, 0, tbuf, -1, pszResult, len+1, 0, NULL);
    pszResult[len] = 0;

    CPLFree(tbuf);

    return pszResult;
}

#endif

/*
** For now we disable the rest which is locale() related.  We may need
** parts of it later.
*/

#ifdef notdef

#ifdef _WIN32
# include <windows.h>
#endif

/*! Return true if the "locale" seems to indicate that UTF-8 encoding
    is used. If true the utf8tomb and utf8frommb don't do anything
    useful.

    <i>It is highly recommended that you change your system so this
    does return true.</i> On Windows this is done by setting the
    "codepage" to CP_UTF8.  On Unix this is done by setting $LC_CTYPE
    to a string containing the letters "utf" or "UTF" in it, or by
    deleting all $LC* and $LANG environment variables. In the future
    it is likely that all non-Asian Unix systems will return true,
    due to the compatibility of UTF-8 with ISO-8859-1.
*/
int utf8locale( void )
{
    static int ret = 2;
    if( ret == 2 ) {
#ifdef _WIN32
        ret = GetACP() == CP_UTF8;
#else
        char* s;
        ret = 1; // assume UTF-8 if no locale
        if( ((s = getenv("LC_CTYPE")) && *s) ||
            ((s = getenv("LC_ALL"))   && *s) ||
            ((s = getenv("LANG"))     && *s) )
        {
            ret = strstr(s, "utf") || strstr(s, "UTF");
        }
#endif
    }

    return ret;
}

/*! Convert the UTF-8 used by FLTK to the locale-specific encoding
    used for filenames (and sometimes used for data in files).
    Unfortunately due to stupid design you will have to do this as
    needed for filenames. This is a bug on both Unix and Windows.

    Up to \a dstlen bytes are written to \a dst, including a null
    terminator. The return value is the number of bytes that would be
    written, not counting the null terminator. If greater or equal to
    \a dstlen then if you malloc a new array of size n+1 you will have
    the space needed for the entire string. If \a dstlen is zero then
    nothing is written and this call just measures the storage space
    needed.

    If utf8locale() returns true then this does not change the data.
    It is copied and truncated as necessary to
    the destination buffer and \a srclen is always returned.  */
unsigned utf8tomb( const char* src, unsigned srclen,
                   char* dst, unsigned dstlen )
{
  if( !utf8locale() )
  {
#ifdef _WIN32
    wchar_t lbuf[1024] = {};
    wchar_t* buf = lbuf;
    unsigned length = utf8towc(src, srclen, buf, 1024);
    unsigned ret;
    if( length >= 1024 )
    {
        buf = static_cast<wchar_t *>(malloc((length + 1) * sizeof(wchar_t)));
        utf8towc(src, srclen, buf, length + 1);
    }
    if( dstlen )
    {
      // apparently this does not null-terminate, even though msdn
      // documentation claims it does:
      ret =
        WideCharToMultiByte(GetACP(), 0, buf, length, dst, dstlen, 0, 0);
      dst[ret] = 0;
    }
    // if it overflows or measuring length, get the actual length:
    if( dstlen==0 || ret >= dstlen-1 )
        ret = WideCharToMultiByte(GetACP(), 0, buf, length, 0, 0, 0, 0);
    if( buf != lbuf ) free((void*)buf);
    return ret;
#else
    wchar_t lbuf[1024] = {};
    wchar_t* buf = lbuf;
    unsigned length = utf8towc(src, srclen, buf, 1024);
    if( length >= 1024 )
    {
        buf = static_cast<wchar_t *>(malloc((length + 1) * sizeof(wchar_t)));
        utf8towc(src, srclen, buf, length+1);
    }
    int ret = 0;
    if( dstlen )
    {
      ret = wcstombs(dst, buf, dstlen);
      if( ret >= dstlen - 1 ) ret = wcstombs(0, buf, 0);
    } else {
      ret = wcstombs(0, buf, 0);
    }
    if( buf != lbuf ) free((void*)buf);
    if( ret >= 0 ) return (unsigned)ret;
    // On any errors we return the UTF-8 as raw text...
#endif
  }
  // Identity transform:
  if( srclen < dstlen )
  {
    memcpy(dst, src, srclen);
    dst[srclen] = 0;
  } else {
    memcpy(dst, src, dstlen-1);
    dst[dstlen-1] = 0;
  }
  return srclen;
}

/*! Convert a filename from the locale-specific multibyte encoding
    used by Windows to UTF-8 as used by FLTK.

    Up to \a dstlen bytes are written to \a dst, including a null
    terminator. The return value is the number of bytes that would be
    written, not counting the null terminator. If greater or equal to
    \a dstlen then if you malloc a new array of size n+1 you will have
    the space needed for the entire string. If \a dstlen is zero then
    nothing is written and this call just measures the storage space
    needed.

    On Unix or on Windows when a UTF-8 locale is in effect, this
    does not change the data. It is copied and truncated as necessary to
    the destination buffer and \a srclen is always returned.
    You may also want to check if utf8test() returns non-zero, so that
    the filesystem can store filenames in UTF-8 encoding regardless of
    the locale.
*/
unsigned utf8frommb(char* dst, unsigned dstlen,
                    const char* src, unsigned srclen)
{
  if( !utf8locale() )
  {
#ifdef _WIN32
    wchar_t lbuf[1024] = {};
    wchar_t* buf = lbuf;
    unsigned ret;
    const unsigned length =
      MultiByteToWideChar(GetACP(), 0, src, srclen, buf, 1024);
    if( length >= 1024 )
    {
      length = MultiByteToWideChar(GetACP(), 0, src, srclen, 0, 0);
      buf = static_cast<wchar_t *>(malloc(length * sizeof(wchar_t)));
      MultiByteToWideChar(GetACP(), 0, src, srclen, buf, length);
    }
    ret = utf8fromwc(dst, dstlen, buf, length);
    if( buf != lbuf ) free(buf);
    return ret;
#else
    wchar_t lbuf[1024] = {};
    wchar_t* buf = lbuf;
    const int length = mbstowcs(buf, src, 1024);
    if( length >= 1024 )
    {
      length = mbstowcs(0, src, 0)+1;
      buf = static_cast<wchar_t *>(malloc(length*sizeof(unsigned short)));
      mbstowcs(buf, src, length);
    }
    if( length >= 0 )
    {
      const unsigned ret = utf8fromwc(dst, dstlen, buf, length);
      if( buf != lbuf ) free(buf);
      return ret;
    }
    // Errors in conversion return the UTF-8 unchanged.
#endif
  }
  // Identity transform:
  if( srclen < dstlen )
  {
    memcpy(dst, src, srclen);
    dst[srclen] = 0;
  }
  else
  {
    memcpy(dst, src, dstlen-1);
    dst[dstlen-1] = 0;
  }
  return srclen;
}

#endif // def notdef - disabled locale specific stuff.

/*! Examines the first \a srclen bytes in \a src and return a verdict
    on whether it is UTF-8 or not.
    - Returns 0 if there is any illegal UTF-8 sequences, using the
      same rules as utf8decode(). Note that some UCS values considered
      illegal by RFC 3629, such as 0xffff, are considered legal by this.
    - Returns 1 if there are only single-byte characters (i.e. no bytes
      have the high bit set). This is legal UTF-8, but also indicates
      plain ASCII. It also returns 1 if \a srclen is zero.
    - Returns 2 if there are only characters less than 0x800.
    - Returns 3 if there are only characters less than 0x10000.
    - Returns 4 if there are characters in the 0x10000 to 0x10ffff range.

    Because there are many illegal sequences in UTF-8, it is almost
    impossible for a string in another encoding to be confused with
    UTF-8. This is very useful for transitioning Unix to UTF-8
    filenames, you can simply test each filename with this to decide
    if it is UTF-8 or in the locale encoding. My hope is that if
    this is done we will be able to cleanly transition to a locale-less
    encoding.
*/

static int utf8test( const char* src, unsigned srclen )
{
    int ret = 1;
    const char* p = src;
    const char* e = src + srclen;
    while( p < e )
    {
        if( *p & 0x80 )
        {
            int len = 0;
            utf8decode(p, e, &len);
            if( len < 2 ) return 0;
            if( len > ret ) ret = len;
            p += len;
        } else {
            p++;
        }
    }
    return ret;
}

#endif /* defined(CPL_RECODE_STUB) */
