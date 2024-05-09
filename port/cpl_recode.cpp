/**********************************************************************
 *
 * Name:     cpl_recode.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Character set recoding and char/wchar_t conversions.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 **********************************************************************
 * Copyright (c) 2011, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "utf8.h"

#ifdef CPL_RECODE_ICONV
extern void CPLClearRecodeIconvWarningFlags();
extern char *CPLRecodeIconv(const char *, const char *,
                            const char *) CPL_RETURNS_NONNULL;
extern char *CPLRecodeFromWCharIconv(const wchar_t *, const char *,
                                     const char *);
extern wchar_t *CPLRecodeToWCharIconv(const char *, const char *, const char *);
#endif  // CPL_RECODE_ICONV

extern void CPLClearRecodeStubWarningFlags();
extern char *CPLRecodeStub(const char *, const char *,
                           const char *) CPL_RETURNS_NONNULL;
extern char *CPLRecodeFromWCharStub(const wchar_t *, const char *,
                                    const char *);
extern wchar_t *CPLRecodeToWCharStub(const char *, const char *, const char *);
extern int CPLIsUTF8Stub(const char *, int);

/************************************************************************/
/*                             CPLRecode()                              */
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
 *
 * @since GDAL 1.6.0
 */

char CPL_DLL *CPLRecode(const char *pszSource, const char *pszSrcEncoding,
                        const char *pszDstEncoding)

{
    /* -------------------------------------------------------------------- */
    /*      Handle a few common short cuts.                                 */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszSrcEncoding, pszDstEncoding))
        return CPLStrdup(pszSource);

    if (EQUAL(pszSrcEncoding, CPL_ENC_ASCII) &&
        (EQUAL(pszDstEncoding, CPL_ENC_UTF8) ||
         EQUAL(pszDstEncoding, CPL_ENC_ISO8859_1)))
        return CPLStrdup(pszSource);

    /* -------------------------------------------------------------------- */
    /*      For ZIP file handling                                           */
    /*      (CP437 might be missing even on some iconv, like on Mac)        */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszSrcEncoding, "CP437") &&
        EQUAL(pszDstEncoding, CPL_ENC_UTF8))  //
    {
        bool bIsAllPrintableASCII = true;
        const size_t nCharCount = strlen(pszSource);
        for (size_t i = 0; i < nCharCount; i++)
        {
            if (pszSource[i] < 32 || pszSource[i] > 126)
            {
                bIsAllPrintableASCII = false;
                break;
            }
        }
        if (bIsAllPrintableASCII)
        {
            return CPLStrdup(pszSource);
        }
    }

#ifdef CPL_RECODE_ICONV
    /* -------------------------------------------------------------------- */
    /*      CPL_ENC_ISO8859_1 -> CPL_ENC_UTF8                               */
    /*      and CPL_ENC_UTF8 -> CPL_ENC_ISO8859_1 conversions are handled   */
    /*      very well by the stub implementation which is faster than the   */
    /*      iconv() route. Use a stub for these two ones and iconv()        */
    /*      everything else.                                                */
    /* -------------------------------------------------------------------- */
    if ((EQUAL(pszSrcEncoding, CPL_ENC_ISO8859_1) &&
         EQUAL(pszDstEncoding, CPL_ENC_UTF8)) ||
        (EQUAL(pszSrcEncoding, CPL_ENC_UTF8) &&
         EQUAL(pszDstEncoding, CPL_ENC_ISO8859_1)))
    {
        return CPLRecodeStub(pszSource, pszSrcEncoding, pszDstEncoding);
    }
#ifdef _WIN32
    else if (((EQUAL(pszSrcEncoding, "CP_ACP") ||
               EQUAL(pszSrcEncoding, "CP_OEMCP")) &&
              EQUAL(pszDstEncoding, CPL_ENC_UTF8)) ||
             (EQUAL(pszSrcEncoding, CPL_ENC_UTF8) &&
              (EQUAL(pszDstEncoding, "CP_ACP") ||
               EQUAL(pszDstEncoding, "CP_OEMCP"))))
    {
        return CPLRecodeStub(pszSource, pszSrcEncoding, pszDstEncoding);
    }
#endif
    else
    {
        return CPLRecodeIconv(pszSource, pszSrcEncoding, pszDstEncoding);
    }
#else   // CPL_RECODE_STUB
    return CPLRecodeStub(pszSource, pszSrcEncoding, pszDstEncoding);
#endif  // CPL_RECODE_ICONV
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
 * and CPL_ENC_ISO8859_1.  In some cases (i.e. using iconv()) other encodings
 * may also be supported.
 *
 * Note that the wchar_t type varies in size on different systems. On
 * win32 it is normally 2 bytes, and on UNIX 4 bytes.
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

char CPL_DLL *CPLRecodeFromWChar(const wchar_t *pwszSource,
                                 const char *pszSrcEncoding,
                                 const char *pszDstEncoding)

{
#ifdef CPL_RECODE_ICONV
    /* -------------------------------------------------------------------- */
    /*      Conversions from CPL_ENC_UCS2                                   */
    /*      to CPL_ENC_UTF8, CPL_ENC_ISO8859_1 and CPL_ENC_ASCII are well   */
    /*      handled by the stub implementation.                             */
    /* -------------------------------------------------------------------- */
    if ((EQUAL(pszSrcEncoding, CPL_ENC_UCS2) ||
         EQUAL(pszSrcEncoding, "WCHAR_T")) &&
        (EQUAL(pszDstEncoding, CPL_ENC_UTF8) ||
         EQUAL(pszDstEncoding, CPL_ENC_ASCII) ||
         EQUAL(pszDstEncoding, CPL_ENC_ISO8859_1)))
    {
        return CPLRecodeFromWCharStub(pwszSource, pszSrcEncoding,
                                      pszDstEncoding);
    }

    return CPLRecodeFromWCharIconv(pwszSource, pszSrcEncoding, pszDstEncoding);

#else   // CPL_RECODE_STUB
    return CPLRecodeFromWCharStub(pwszSource, pszSrcEncoding, pszDstEncoding);
#endif  // CPL_RECODE_ICONV
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
 * win32 it is normally 2 bytes, and on UNIX 4 bytes.
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

wchar_t CPL_DLL *CPLRecodeToWChar(const char *pszSource,
                                  const char *pszSrcEncoding,
                                  const char *pszDstEncoding)

{
#ifdef CPL_RECODE_ICONV
    /* -------------------------------------------------------------------- */
    /*      Conversions to CPL_ENC_UCS2                                     */
    /*      from CPL_ENC_UTF8, CPL_ENC_ISO8859_1 and CPL_ENC_ASCII are well */
    /*      handled by the stub implementation.                             */
    /* -------------------------------------------------------------------- */
    if ((EQUAL(pszDstEncoding, CPL_ENC_UCS2) ||
         EQUAL(pszDstEncoding, "WCHAR_T")) &&
        (EQUAL(pszSrcEncoding, CPL_ENC_UTF8) ||
         EQUAL(pszSrcEncoding, CPL_ENC_ASCII) ||
         EQUAL(pszSrcEncoding, CPL_ENC_ISO8859_1)))
    {
        return CPLRecodeToWCharStub(pszSource, pszSrcEncoding, pszDstEncoding);
    }

    return CPLRecodeToWCharIconv(pszSource, pszSrcEncoding, pszDstEncoding);

#else   // CPL_RECODE_STUB
    return CPLRecodeToWCharStub(pszSource, pszSrcEncoding, pszDstEncoding);
#endif  // CPL_RECODE_ICONV
}

/************************************************************************/
/*                               CPLIsASCII()                           */
/************************************************************************/

/**
 * Test if a string is encoded as ASCII.
 *
 * @param pabyData input string to test
 * @param nLen length of the input string, or -1 if the function must compute
 *             the string length. In which case it must be null terminated.
 * @return true if the string is encoded as ASCII. false otherwise
 *
 * @since GDAL 3.6.0
 */
bool CPLIsASCII(const char *pabyData, size_t nLen)
{
    if (nLen == static_cast<size_t>(-1))
        nLen = strlen(pabyData);
    for (size_t i = 0; i < nLen; ++i)
    {
        if (static_cast<unsigned char>(pabyData[i]) > 127)
            return false;
    }
    return true;
}

/************************************************************************/
/*                          CPLForceToASCII()                           */
/************************************************************************/

/**
 * Return a new string that is made only of ASCII characters. If non-ASCII
 * characters are found in the input string, they will be replaced by the
 * provided replacement character.
 *
 * This function does not make any assumption on the encoding of the input
 * string (except it must be nul-terminated if nLen equals -1, or have at
 * least nLen bytes otherwise). CPLUTF8ForceToASCII() can be used instead when
 * the input string is known to be UTF-8 encoded.
 *
 * @param pabyData input string to test
 * @param nLen length of the input string, or -1 if the function must compute
 *             the string length. In which case it must be null terminated.

 * @param chReplacementChar character which will be used when the input stream
 *                          contains a non ASCII character. Must be valid ASCII!
 *
 * @return a new string that must be freed with CPLFree().
 *
 * @since GDAL 1.7.0
 */
char *CPLForceToASCII(const char *pabyData, int nLen, char chReplacementChar)
{
    const size_t nRealLen =
        (nLen >= 0) ? static_cast<size_t>(nLen) : strlen(pabyData);
    char *pszOutputString = static_cast<char *>(CPLMalloc(nRealLen + 1));
    const char *pszPtr = pabyData;
    const char *pszEnd = pabyData + nRealLen;
    size_t i = 0;
    while (pszPtr != pszEnd)
    {
        if (*reinterpret_cast<const unsigned char *>(pszPtr) > 127)
        {
            pszOutputString[i] = chReplacementChar;
            ++pszPtr;
            ++i;
        }
        else
        {
            pszOutputString[i] = *pszPtr;
            ++pszPtr;
            ++i;
        }
    }
    pszOutputString[i] = '\0';
    return pszOutputString;
}

/************************************************************************/
/*                       CPLUTF8ForceToASCII()                          */
/************************************************************************/

/**
 * Return a new string that is made only of ASCII characters. If non-ASCII
 * characters are found in the input string, for which an "equivalent" ASCII
 * character is not found, they will be replaced by the provided replacement
 * character.
 *
 * This function is aware of https://en.wikipedia.org/wiki/Latin-1_Supplement
 * and https://en.wikipedia.org/wiki/Latin_Extended-A to provide sensible
 * replacements for accented characters.

 * @param pszStr NUL-terminated UTF-8 string.
 * @param chReplacementChar character which will be used when the input stream
 *                          contains a non ASCII character that cannot be
 *                          substituted with an equivalent ASCII character.
 *                          Must be valid ASCII!
 *
 * @return a new string that must be freed with CPLFree().
 *
 * @since GDAL 3.9
 */
char *CPLUTF8ForceToASCII(const char *pszStr, char chReplacementChar)
{
    static const struct
    {
        short nCodePoint;
        char chFirst;
        char chSecond;
    } aLatinCharacters[] = {
        // https://en.wikipedia.org/wiki/Latin-1_Supplement
        {0xC0, 'A', 0},    // Latin Capital Letter A with grave
        {0xC1, 'A', 0},    // Latin Capital letter A with acute
        {0xC2, 'A', 0},    // Latin Capital letter A with circumflex
        {0xC3, 'A', 0},    // Latin Capital letter A with tilde
        {0xC4, 'A', 0},    // Latin Capital letter A with diaeresis
        {0xC5, 'A', 0},    // Latin Capital letter A with ring above
        {0xC6, 'A', 'E'},  // Latin Capital letter AE
        {0xC7, 'C', 0},    // Latin Capital letter C with cedilla
        {0xC8, 'E', 0},    // Latin Capital letter E with grave
        {0xC9, 'E', 0},    // Latin Capital letter E with acute
        {0xCA, 'E', 0},    // Latin Capital letter E with circumflex
        {0xCB, 'E', 0},    // Latin Capital letter E with diaeresis
        {0xCC, 'I', 0},    // Latin Capital letter I with grave
        {0xCD, 'I', 0},    // Latin Capital letter I with acute
        {0xCE, 'I', 0},    // Latin Capital letter I with circumflex
        {0xCF, 'I', 0},    // Latin Capital letter I with diaeresis
        // { 0xD0, '?', 0 }, // Latin Capital letter Eth
        {0xD1, 'N', 0},  // Latin Capital letter N with tilde
        {0xD2, 'O', 0},  // Latin Capital letter O with grave
        {0xD3, 'O', 0},  // Latin Capital letter O with acute
        {0xD4, 'O', 0},  // Latin Capital letter O with circumflex
        {0xD5, 'O', 0},  // Latin Capital letter O with tilde
        {0xD6, 'O', 0},  // Latin Capital letter O with diaeresis
        {0xD8, 'O', 0},  // Latin Capital letter O with stroke
        {0xD9, 'U', 0},  // Latin Capital letter U with grave
        {0xDA, 'U', 0},  // Latin Capital letter U with acute
        {0xDB, 'U', 0},  // Latin Capital Letter U with circumflex
        {0xDC, 'U', 0},  // Latin Capital Letter U with diaeresis
        {0xDD, 'Y', 0},  // Latin Capital Letter Y with acute
        // { 0xDE, '?', 0 }, // Latin Capital Letter Thorn
        {0xDF, 'S', 'S'},  // Latin Small Letter sharp S
        {0xE0, 'a', 0},    // Latin Small Letter A with grave
        {0xE1, 'a', 0},    // Latin Small Letter A with acute
        {0xE2, 'a', 0},    // Latin Small Letter A with circumflex
        {0xE3, 'a', 0},    // Latin Small Letter A with tilde
        {0xE4, 'a', 0},    // Latin Small Letter A with diaeresis
        {0xE5, 'a', 0},    // Latin Small Letter A with ring above
        {0xE6, 'a', 'e'},  // Latin Small Letter AE
        {0xE7, 'c', 0},    // Latin Small Letter C with cedilla
        {0xE8, 'e', 0},    // Latin Small Letter E with grave
        {0xE9, 'e', 0},    // Latin Small Letter E with acute
        {0xEA, 'e', 0},    // Latin Small Letter E with circumflex
        {0xEB, 'e', 0},    // Latin Small Letter E with diaeresis
        {0xEC, 'i', 0},    // Latin Small Letter I with grave
        {0xED, 'i', 0},    // Latin Small Letter I with acute
        {0xEE, 'i', 0},    // Latin Small Letter I with circumflex
        {0xEF, 'i', 0},    // Latin Small Letter I with diaeresis
        // { 0xF0, '?', 0 }, // Latin Small Letter Eth
        {0xF1, 'n', 0},  // Latin Small Letter N with tilde
        {0xF2, 'o', 0},  // Latin Small Letter O with grave
        {0xF3, 'o', 0},  // Latin Small Letter O with acute
        {0xF4, 'o', 0},  // Latin Small Letter O with circumflex
        {0xF5, 'o', 0},  // Latin Small Letter O with tilde
        {0xF6, 'o', 0},  // Latin Small Letter O with diaeresis
        {0xF8, 'o', 0},  // Latin Small Letter O with stroke
        {0xF9, 'u', 0},  // Latin Small Letter U with grave
        {0xFA, 'u', 0},  // Latin Small Letter U with acute
        {0xFB, 'u', 0},  // Latin Small Letter U with circumflex
        {0xFC, 'u', 0},  // Latin Small Letter U with diaeresis
        {0xFD, 'y', 0},  // Latin Small Letter Y with acute
        // { 0xFE, '?', 0 }, // Latin Small Letter Thorn
        {0xFF, 'u', 0},  // Latin Small Letter Y with diaeresis

        // https://en.wikipedia.org/wiki/Latin_Extended-A
        {
            0x0100,
            'A',
            0,
        },  // Latin Capital letter A with macron
        {
            0x0101,
            'a',
            0,
        },  // Latin Small letter A with macron
        {
            0x0102,
            'A',
            0,
        },  // Latin Capital letter A with breve
        {
            0x0103,
            'a',
            0,
        },  // Latin Small letter A with breve
        {
            0x0104,
            'A',
            0,
        },  // Latin Capital letter A with ogonek
        {
            0x0105,
            'a',
            0,
        },  // Latin Small letter A with ogonek
        {
            0x0106,
            'C',
            0,
        },  // Latin Capital letter C with acute
        {
            0x0107,
            'c',
            0,
        },  // Latin Small letter C with acute
        {
            0x0108,
            'C',
            0,
        },  // Latin Capital letter C with circumflex
        {
            0x0109,
            'c',
            0,
        },  // Latin Small letter C with circumflex
        {
            0x010A,
            'C',
            0,
        },  // Latin Capital letter C with dot above
        {
            0x010B,
            'c',
            0,
        },  // Latin Small letter C with dot above
        {
            0x010C,
            'C',
            0,
        },  // Latin Capital letter C with caron
        {
            0x010D,
            'c',
            0,
        },  // Latin Small letter C with caron
        {
            0x010E,
            'D',
            0,
        },  // Latin Capital letter D with caron
        {
            0x010F,
            'd',
            0,
        },  // Latin Small letter D with caron
        {
            0x0110,
            'D',
            0,
        },  // Latin Capital letter D with stroke
        {
            0x0111,
            'd',
            0,
        },  // Latin Small letter D with stroke
        {
            0x0112,
            'E',
            0,
        },  // Latin Capital letter E with macron
        {
            0x0113,
            'e',
            0,
        },  // Latin Small letter E with macron
        {
            0x0114,
            'E',
            0,
        },  // Latin Capital letter E with breve
        {
            0x0115,
            'e',
            0,
        },  // Latin Small letter E with breve
        {
            0x0116,
            'E',
            0,
        },  // Latin Capital letter E with dot above
        {
            0x0117,
            'e',
            0,
        },  // Latin Small letter E with dot above
        {
            0x0118,
            'E',
            0,
        },  // Latin Capital letter E with ogonek
        {
            0x0119,
            'e',
            0,
        },  // Latin Small letter E with ogonek
        {
            0x011A,
            'E',
            0,
        },  // Latin Capital letter E with caron
        {
            0x011B,
            'e',
            0,
        },  // Latin Small letter E with caron
        {
            0x011C,
            'G',
            0,
        },  // Latin Capital letter G with circumflex
        {
            0x011D,
            'g',
            0,
        },  // Latin Small letter G with circumflex
        {
            0x011E,
            'G',
            0,
        },  // Latin Capital letter G with breve
        {
            0x011F,
            'g',
            0,
        },  // Latin Small letter G with breve
        {
            0x0120,
            'G',
            0,
        },  // Latin Capital letter G with dot above
        {
            0x0121,
            'g',
            0,
        },  // Latin Small letter G with dot above
        {
            0x0122,
            'G',
            0,
        },  // Latin Capital letter G with cedilla
        {
            0x0123,
            'g',
            0,
        },  // Latin Small letter G with cedilla
        {
            0x0124,
            'H',
            0,
        },  // Latin Capital letter H with circumflex
        {
            0x0125,
            'h',
            0,
        },  // Latin Small letter H with circumflex
        {
            0x0126,
            'H',
            0,
        },  // Latin Capital letter H with stroke
        {
            0x0127,
            'h',
            0,
        },  // Latin Small letter H with stroke
        {
            0x0128,
            'I',
            0,
        },  // Latin Capital letter I with tilde
        {
            0x0129,
            'i',
            0,
        },  // Latin Small letter I with tilde
        {
            0x012A,
            'I',
            0,
        },  // Latin Capital letter I with macron
        {
            0x012B,
            'i',
            0,
        },  // Latin Small letter I with macron
        {
            0x012C,
            'I',
            0,
        },  // Latin Capital letter I with breve
        {
            0x012D,
            'i',
            0,
        },  // Latin Small letter I with breve
        {
            0x012E,
            'I',
            0,
        },  // Latin Capital letter I with ogonek
        {
            0x012F,
            'i',
            0,
        },  // Latin Small letter I with ogonek
        {
            0x0130,
            'I',
            0,
        },  // Latin Capital letter I with dot above
        {
            0x0131,
            'i',
            0,
        },  // Latin Small letter dotless I
        {
            0x0132,
            'I',
            'J',
        },  // Latin Capital Ligature IJ
        {
            0x0133,
            'i',
            'j',
        },  // Latin Small Ligature IJ
        {
            0x0134,
            'J',
            0,
        },  // Latin Capital letter J with circumflex
        {
            0x0135,
            'j',
            0,
        },  // Latin Small letter J with circumflex
        {
            0x0136,
            'K',
            0,
        },  // Latin Capital letter K with cedilla
        {
            0x0137,
            'k',
            0,
        },  // Latin Small letter K with cedilla
        {
            0x0138,
            'k',
            0,
        },  // Latin Small letter Kra
        {
            0x0139,
            'L',
            0,
        },  // Latin Capital letter L with acute
        {
            0x013A,
            'l',
            0,
        },  // Latin Small letter L with acute
        {
            0x013B,
            'L',
            0,
        },  // Latin Capital letter L with cedilla
        {
            0x013C,
            'l',
            0,
        },  // Latin Small letter L with cedilla
        {
            0x013D,
            'L',
            0,
        },  // Latin Capital letter L with caron
        {
            0x013E,
            'l',
            0,
        },  // Latin Small letter L with caron
        {
            0x013F,
            'L',
            0,
        },  // Latin Capital letter L with middle dot
        {
            0x0140,
            'l',
            0,
        },  // Latin Small letter L with middle dot
        {
            0x0141,
            'L',
            0,
        },  // Latin Capital letter L with stroke
        {
            0x0142,
            'l',
            0,
        },  // Latin Small letter L with stroke
        {
            0x0143,
            'N',
            0,
        },  // Latin Capital letter N with acute
        {
            0x0144,
            'n',
            0,
        },  // Latin Small letter N with acute
        {
            0x0145,
            'N',
            0,
        },  // Latin Capital letter N with cedilla
        {
            0x0146,
            'n',
            0,
        },  // Latin Small letter N with cedilla
        {
            0x0147,
            'N',
            0,
        },  // Latin Capital letter N with caron
        {
            0x0148,
            'n',
            0,
        },  // Latin Small letter N with caron
        // { 0x014A , '?' , 0, }, // Latin Capital letter Eng
        // { 0x014B , '?' , 0, }, // Latin Small letter Eng
        {
            0x014C,
            'O',
            0,
        },  // Latin Capital letter O with macron
        {
            0x014D,
            'o',
            0,
        },  // Latin Small letter O with macron
        {
            0x014E,
            'O',
            0,
        },  // Latin Capital letter O with breve
        {
            0x014F,
            'o',
            0,
        },  // Latin Small letter O with breve
        {
            0x0150,
            'O',
            0,
        },  // Latin Capital Letter O with double acute
        {
            0x0151,
            'o',
            0,
        },  // Latin Small Letter O with double acute
        {
            0x0152,
            'O',
            'E',
        },  // Latin Capital Ligature OE
        {
            0x0153,
            'o',
            'e',
        },  // Latin Small Ligature OE
        {
            0x0154,
            'R',
            0,
        },  // Latin Capital letter R with acute
        {
            0x0155,
            'r',
            0,
        },  // Latin Small letter R with acute
        {
            0x0156,
            'R',
            0,
        },  // Latin Capital letter R with cedilla
        {
            0x0157,
            'r',
            0,
        },  // Latin Small letter R with cedilla
        {
            0x0158,
            'R',
            0,
        },  // Latin Capital letter R with caron
        {
            0x0159,
            'r',
            0,
        },  // Latin Small letter R with caron
        {
            0x015A,
            'S',
            0,
        },  // Latin Capital letter S with acute
        {
            0x015B,
            's',
            0,
        },  // Latin Small letter S with acute
        {
            0x015C,
            'S',
            0,
        },  // Latin Capital letter S with circumflex
        {
            0x015D,
            's',
            0,
        },  // Latin Small letter S with circumflex
        {
            0x015E,
            'S',
            0,
        },  // Latin Capital letter S with cedilla
        {
            0x015F,
            's',
            0,
        },  // Latin Small letter S with cedilla
        {
            0x0160,
            'S',
            0,
        },  // Latin Capital letter S with caron
        {
            0x0161,
            's',
            0,
        },  // Latin Small letter S with caron
        {
            0x0162,
            'T',
            0,
        },  // Latin Capital letter T with cedilla
        {
            0x0163,
            't',
            0,
        },  // Latin Small letter T with cedilla
        {
            0x0164,
            'T',
            0,
        },  // Latin Capital letter T with caron
        {
            0x0165,
            't',
            0,
        },  // Latin Small letter T with caron
        {
            0x0166,
            'T',
            0,
        },  // Latin Capital letter T with stroke
        {
            0x0167,
            't',
            0,
        },  // Latin Small letter T with stroke
        {
            0x0168,
            'U',
            0,
        },  // Latin Capital letter U with tilde
        {
            0x0169,
            'u',
            0,
        },  // Latin Small letter U with tilde
        {
            0x016A,
            'U',
            0,
        },  // Latin Capital letter U with macron
        {
            0x016B,
            'u',
            0,
        },  // Latin Small letter U with macron
        {
            0x016C,
            'U',
            0,
        },  // Latin Capital letter U with breve
        {
            0x016D,
            'u',
            0,
        },  // Latin Small letter U with breve
        {
            0x016E,
            'U',
            0,
        },  // Latin Capital letter U with ring above
        {
            0x016F,
            'u',
            0,
        },  // Latin Small letter U with ring above
        {
            0x0170,
            'U',
            0,
        },  // Latin Capital Letter U with double acute
        {
            0x0171,
            'u',
            0,
        },  // Latin Small Letter U with double acute
        {
            0x0172,
            'U',
            0,
        },  // Latin Capital letter U with ogonek
        {
            0x0173,
            'u',
            0,
        },  // Latin Small letter U with ogonek
        {
            0x0174,
            'W',
            0,
        },  // Latin Capital letter W with circumflex
        {
            0x0175,
            'w',
            0,
        },  // Latin Small letter W with circumflex
        {
            0x0176,
            'Y',
            0,
        },  // Latin Capital letter Y with circumflex
        {
            0x0177,
            'y',
            0,
        },  // Latin Small letter Y with circumflex
        {
            0x0178,
            'Y',
            0,
        },  // Latin Capital letter Y with diaeresis
        {
            0x0179,
            'Z',
            0,
        },  // Latin Capital letter Z with acute
        {
            0x017A,
            'z',
            0,
        },  // Latin Small letter Z with acute
        {
            0x017B,
            'Z',
            0,
        },  // Latin Capital letter Z with dot above
        {
            0x017C,
            'z',
            0,
        },  // Latin Small letter Z with dot above
        {
            0x017D,
            'Z',
            0,
        },  // Latin Capital letter Z with caron
        {
            0x017E,
            'z',
            0,
        },  // Latin Small letter Z with caron
    };

    const size_t nLen = strlen(pszStr);
    char *pszOutputString = static_cast<char *>(CPLMalloc(nLen + 1));
    const char *pszPtr = pszStr;
    const char *pszEnd = pszStr + nLen;
    size_t i = 0;
    while (pszPtr != pszEnd)
    {
        if (*reinterpret_cast<const unsigned char *>(pszPtr) > 127)
        {
            utf8_int32_t codepoint;
            if (pszPtr + utf8codepointcalcsize(
                             reinterpret_cast<const utf8_int8_t *>(pszPtr)) >
                pszEnd)
                break;
            auto pszNext = reinterpret_cast<const char *>(utf8codepoint(
                reinterpret_cast<const utf8_int8_t *>(pszPtr), &codepoint));
            char ch = chReplacementChar;
            for (const auto &latin1char : aLatinCharacters)
            {
                if (codepoint == latin1char.nCodePoint)
                {
                    pszOutputString[i] = latin1char.chFirst;
                    ++i;
                    if (latin1char.chSecond)
                    {
                        pszOutputString[i] = latin1char.chSecond;
                        ++i;
                    }
                    ch = 0;
                    break;
                }
            }
            if (ch)
            {
                pszOutputString[i] = ch;
                ++i;
            }
            pszPtr = pszNext;
        }
        else
        {
            pszOutputString[i] = *pszPtr;
            ++pszPtr;
            ++i;
        }
    }
    pszOutputString[i] = '\0';
    return pszOutputString;
}

/************************************************************************/
/*                        CPLEncodingCharSize()                         */
/************************************************************************/

/**
 * Return bytes per character for encoding.
 *
 * This function returns the size in bytes of the smallest character
 * in this encoding.  For fixed width encodings (ASCII, UCS-2, UCS-4) this
 * is straight forward.  For encodings like UTF8 and UTF16 which represent
 * some characters as a sequence of atomic character sizes the function
 * still returns the atomic character size (1 for UTF8, 2 for UTF16).
 *
 * This function will return the correct value for well known encodings
 * with corresponding CPL_ENC_ values.  It may not return the correct value
 * for other encodings even if they are supported by the underlying iconv
 * or windows transliteration services.  Hopefully it will improve over time.
 *
 * @param pszEncoding the name of the encoding.
 *
 * @return the size of a minimal character in bytes or -1 if the size is
 * unknown.
 */

int CPLEncodingCharSize(const char *pszEncoding)

{
    if (EQUAL(pszEncoding, CPL_ENC_UTF8))
        return 1;
    else if (EQUAL(pszEncoding, CPL_ENC_UTF16) ||
             EQUAL(pszEncoding, "UTF-16LE"))
        return 2;
    else if (EQUAL(pszEncoding, CPL_ENC_UCS2) || EQUAL(pszEncoding, "UCS-2LE"))
        return 2;
    else if (EQUAL(pszEncoding, CPL_ENC_UCS4))
        return 4;
    else if (EQUAL(pszEncoding, CPL_ENC_ASCII))
        return 1;
    else if (STARTS_WITH_CI(pszEncoding, "ISO-8859-"))
        return 1;

    return -1;
}

/************************************************************************/
/*                    CPLClearRecodeWarningFlags()                      */
/************************************************************************/

void CPLClearRecodeWarningFlags()
{
#ifdef CPL_RECODE_ICONV
    CPLClearRecodeIconvWarningFlags();
#endif
    CPLClearRecodeStubWarningFlags();
}

/************************************************************************/
/*                         CPLStrlenUTF8()                              */
/************************************************************************/

/**
 * Return the number of UTF-8 characters of a nul-terminated string.
 *
 * This is different from strlen() which returns the number of bytes.
 *
 * @param pszUTF8Str a nul-terminated UTF-8 string
 *
 * @return the number of UTF-8 characters.
 */

int CPLStrlenUTF8(const char *pszUTF8Str)
{
    int nCharacterCount = 0;
    for (int i = 0; pszUTF8Str[i] != '\0'; ++i)
    {
        if ((pszUTF8Str[i] & 0xc0) != 0x80)
            ++nCharacterCount;
    }
    return nCharacterCount;
}

/************************************************************************/
/*                           CPLCanRecode()                             */
/************************************************************************/

/**
 * Checks if it is possible to recode a string from one encoding to another.
 *
 * @param pszTestStr a NULL terminated string.
 * @param pszSrcEncoding the source encoding.
 * @param pszDstEncoding the destination encoding.
 *
 * @return a TRUE if recode is possible.
 *
 * @since GDAL 3.1.0
 */
int CPLCanRecode(const char *pszTestStr, const char *pszSrcEncoding,
                 const char *pszDstEncoding)
{
    CPLClearRecodeWarningFlags();
    CPLErrorReset();

    CPLPushErrorHandler(CPLQuietErrorHandler);
    char *pszRec(CPLRecode(pszTestStr, pszSrcEncoding, pszDstEncoding));
    CPLPopErrorHandler();

    if (pszRec == nullptr)
    {
        return FALSE;
    }

    CPLFree(pszRec);

    if (CPLGetLastErrorType() != 0)
    {
        return FALSE;
    }

    return TRUE;
}
