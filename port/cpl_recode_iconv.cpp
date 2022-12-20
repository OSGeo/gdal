/**********************************************************************
 *
 * Name:     cpl_recode_iconv.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Character set recoding and char/wchar_t conversions implemented
 *           using the iconv() functionality.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 **********************************************************************
 * Copyright (c) 2011, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>

#ifdef CPL_RECODE_ICONV

#include <iconv.h>
#include "cpl_string.h"

#ifndef ICONV_CPP_CONST
#define ICONV_CPP_CONST ICONV_CONST
#endif

constexpr size_t CPL_RECODE_DSTBUF_SIZE = 32768;

/* used by cpl_recode.cpp */
extern void CPLClearRecodeIconvWarningFlags();
extern char *CPLRecodeIconv(const char *, const char *,
                            const char *) CPL_RETURNS_NONNULL;
extern char *CPLRecodeFromWCharIconv(const wchar_t *, const char *,
                                     const char *);
extern wchar_t *CPLRecodeToWCharIconv(const char *, const char *, const char *);

/************************************************************************/
/*                 CPLClearRecodeIconvWarningFlags()                    */
/************************************************************************/

static bool bHaveWarned1 = false;
static bool bHaveWarned2 = false;

void CPLClearRecodeIconvWarningFlags()
{
    bHaveWarned1 = false;
    bHaveWarned2 = false;
}

/************************************************************************/
/*                      CPLFixInputEncoding()                           */
/************************************************************************/

static const char *CPLFixInputEncoding(const char *pszSrcEncoding,
                                       int nFirstByteVal)
{
    // iconv on Alpine Linux seems to assume BE order, when it is not explicit
    if (EQUAL(pszSrcEncoding, CPL_ENC_UCS2))
        pszSrcEncoding = "UCS-2LE";
    else if (EQUAL(pszSrcEncoding, CPL_ENC_UTF16) && nFirstByteVal != 0xFF &&
             nFirstByteVal != 0xFE)
    {
        // Only force UTF-16LE if there's no starting endianness marker
        pszSrcEncoding = "UTF-16LE";
    }
    return pszSrcEncoding;
}

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

char *CPLRecodeIconv(const char *pszSource, const char *pszSrcEncoding,
                     const char *pszDstEncoding)

{
    pszSrcEncoding = CPLFixInputEncoding(
        pszSrcEncoding, static_cast<unsigned char>(pszSource[0]));

    iconv_t sConv;

    sConv = iconv_open(pszDstEncoding, pszSrcEncoding);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    // iconv_t might be a integer or a pointer, so we have to fallback to
    // C-style cast
    if (sConv == (iconv_t)(-1))
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Recode from %s to %s failed with the error: \"%s\".",
                 pszSrcEncoding, pszDstEncoding, strerror(errno));

        return CPLStrdup(pszSource);
    }

    /* -------------------------------------------------------------------- */
    /*      XXX: There is a portability issue: iconv() function could be    */
    /*      declared differently on different platforms. The second         */
    /*      argument could be declared as char** (as POSIX defines) or      */
    /*      as a const char**. Handle it with the ICONV_CPP_CONST macro here. */
    /* -------------------------------------------------------------------- */
    ICONV_CPP_CONST char *pszSrcBuf =
        const_cast<ICONV_CPP_CONST char *>(pszSource);
    size_t nSrcLen = strlen(pszSource);
    size_t nDstCurLen = std::max(CPL_RECODE_DSTBUF_SIZE, nSrcLen);
    size_t nDstLen = nDstCurLen;
    char *pszDestination =
        static_cast<char *>(CPLCalloc(nDstCurLen + 1, sizeof(char)));
    char *pszDstBuf = pszDestination;

    while (nSrcLen > 0)
    {
        size_t nConverted =
            iconv(sConv, &pszSrcBuf, &nSrcLen, &pszDstBuf, &nDstLen);

        if (nConverted == static_cast<size_t>(-1))
        {
            if (errno == EILSEQ)
            {
                // Skip the invalid sequence in the input string.
                if (!bHaveWarned1)
                {
                    bHaveWarned1 = true;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "One or several characters couldn't be converted "
                             "correctly from %s to %s.  "
                             "This warning will not be emitted anymore",
                             pszSrcEncoding, pszDstEncoding);
                }
                if (nSrcLen == 0)
                    break;
                nSrcLen--;
                pszSrcBuf++;
                continue;
            }

            else if (errno == E2BIG)
            {
                // We are running out of the output buffer.
                // Dynamically increase the buffer size.
                size_t nTmp = nDstCurLen;
                nDstCurLen *= 2;
                pszDestination = static_cast<char *>(
                    CPLRealloc(pszDestination, nDstCurLen + 1));
                pszDstBuf = pszDestination + nTmp - nDstLen;
                nDstLen += nTmp;
                continue;
            }

            else
                break;
        }
    }

    pszDestination[nDstCurLen - nDstLen] = '\0';

    iconv_close(sConv);

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

char *CPLRecodeFromWCharIconv(const wchar_t *pwszSource,
                              const char *pszSrcEncoding,
                              const char *pszDstEncoding)

{
    pszSrcEncoding = CPLFixInputEncoding(pszSrcEncoding, pwszSource[0]);

    /* -------------------------------------------------------------------- */
    /*      What is the source length.                                      */
    /* -------------------------------------------------------------------- */
    size_t nSrcLen = 0;

    while (pwszSource[nSrcLen] != 0)
        nSrcLen++;

    /* -------------------------------------------------------------------- */
    /*      iconv() does not support wchar_t so we need to repack the       */
    /*      characters according to the width of a character in the         */
    /*      source encoding.  For instance if wchar_t is 4 bytes but our    */
    /*      source is UTF16 then we need to pack down into 2 byte           */
    /*      characters before passing to iconv().                           */
    /* -------------------------------------------------------------------- */
    const int nTargetCharWidth = CPLEncodingCharSize(pszSrcEncoding);

    if (nTargetCharWidth < 1)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Recode from %s with CPLRecodeFromWChar() failed because"
                 " the width of characters in the encoding are not known.",
                 pszSrcEncoding);
        return CPLStrdup("");
    }

    GByte *pszIconvSrcBuf =
        static_cast<GByte *>(CPLCalloc((nSrcLen + 1), nTargetCharWidth));

    for (unsigned int iSrc = 0; iSrc <= nSrcLen; iSrc++)
    {
        if (nTargetCharWidth == 1)
            pszIconvSrcBuf[iSrc] = static_cast<GByte>(pwszSource[iSrc]);
        else if (nTargetCharWidth == 2)
            (reinterpret_cast<short *>(pszIconvSrcBuf))[iSrc] =
                static_cast<short>(pwszSource[iSrc]);
        else if (nTargetCharWidth == 4)
            (reinterpret_cast<GInt32 *>(pszIconvSrcBuf))[iSrc] =
                pwszSource[iSrc];
    }

    /* -------------------------------------------------------------------- */
    /*      Create the iconv() translation object.                          */
    /* -------------------------------------------------------------------- */
    iconv_t sConv;

    sConv = iconv_open(pszDstEncoding, pszSrcEncoding);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    // iconv_t might be a integer or a pointer, so we have to fallback to
    // C-style cast
    if (sConv == (iconv_t)(-1))
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    {
        CPLFree(pszIconvSrcBuf);
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Recode from %s to %s failed with the error: \"%s\".",
                 pszSrcEncoding, pszDstEncoding, strerror(errno));

        return CPLStrdup("");
    }

    /* -------------------------------------------------------------------- */
    /*      XXX: There is a portability issue: iconv() function could be    */
    /*      declared differently on different platforms. The second         */
    /*      argument could be declared as char** (as POSIX defines) or      */
    /*      as a const char**. Handle it with the ICONV_CPP_CONST macro here. */
    /* -------------------------------------------------------------------- */
    ICONV_CPP_CONST char *pszSrcBuf = const_cast<ICONV_CPP_CONST char *>(
        reinterpret_cast<char *>(pszIconvSrcBuf));

    /* iconv expects a number of bytes, not characters */
    nSrcLen *= nTargetCharWidth;

    /* -------------------------------------------------------------------- */
    /*      Allocate destination buffer.                                    */
    /* -------------------------------------------------------------------- */
    size_t nDstCurLen = std::max(CPL_RECODE_DSTBUF_SIZE, nSrcLen + 1);
    size_t nDstLen = nDstCurLen;
    char *pszDestination =
        static_cast<char *>(CPLCalloc(nDstCurLen, sizeof(char)));
    char *pszDstBuf = pszDestination;

    while (nSrcLen > 0)
    {
        const size_t nConverted =
            iconv(sConv, &pszSrcBuf, &nSrcLen, &pszDstBuf, &nDstLen);

        if (nConverted == static_cast<size_t>(-1))
        {
            if (errno == EILSEQ)
            {
                // Skip the invalid sequence in the input string.
                nSrcLen -= nTargetCharWidth;
                pszSrcBuf += nTargetCharWidth;
                if (!bHaveWarned2)
                {
                    bHaveWarned2 = true;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "One or several characters couldn't be converted "
                             "correctly from %s to %s.  "
                             "This warning will not be emitted anymore",
                             pszSrcEncoding, pszDstEncoding);
                }
                continue;
            }

            else if (errno == E2BIG)
            {
                // We are running out of the output buffer.
                // Dynamically increase the buffer size.
                size_t nTmp = nDstCurLen;
                nDstCurLen *= 2;
                pszDestination =
                    static_cast<char *>(CPLRealloc(pszDestination, nDstCurLen));
                pszDstBuf = pszDestination + nTmp - nDstLen;
                nDstLen += nDstCurLen - nTmp;
                continue;
            }

            else
                break;
        }
    }

    if (nDstLen == 0)
    {
        ++nDstCurLen;
        pszDestination =
            static_cast<char *>(CPLRealloc(pszDestination, nDstCurLen));
        ++nDstLen;
    }
    pszDestination[nDstCurLen - nDstLen] = '\0';

    iconv_close(sConv);

    CPLFree(pszIconvSrcBuf);

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

wchar_t *CPLRecodeToWCharIconv(const char *pszSource,
                               const char *pszSrcEncoding,
                               const char *pszDstEncoding)

{
    return reinterpret_cast<wchar_t *>(
        CPLRecodeIconv(pszSource, pszSrcEncoding, pszDstEncoding));
}

#endif /* CPL_RECODE_ICONV */
