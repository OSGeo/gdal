/* $Id: avc_mbyte.c,v 1.4 2008/07/23 20:51:38 dmorissette Exp $
 *
 * Name:     avc_mbyte.c
 * Project:  Arc/Info vector coverage (AVC)  E00->BIN conversion library
 * Language: ANSI C
 * Purpose:  Functions to handle multibyte character conversions.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2005, Daniel Morissette
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "avc.h"

#ifdef _WIN32
#  include <mbctype.h>
#endif

static int _AVCDetectJapaneseEncoding(const GByte *pszLine);
static const GByte *_AVCJapanese2ArcDBCS(AVCDBCSInfo *psDBCSInfo,
                                         const GByte *pszLine,
                                         int nMaxOutputLen);
static const GByte *_AVCArcDBCS2JapaneseShiftJIS(AVCDBCSInfo *psDBCSInfo,
                                                 const GByte *pszLine,
                                                 int nMaxOutputLen);

/*=====================================================================
 * Functions to handle multibyte char conversions
 *====================================================================*/

#define IS_ASCII(c)           ((c) < 0x80)


/**********************************************************************
 *                          AVCAllocDBCSInfo()
 *
 * Alloc and init a new AVCDBCSInfo structure.
 **********************************************************************/
AVCDBCSInfo *AVCAllocDBCSInfo(void)
{
    AVCDBCSInfo *psInfo;

    psInfo = (AVCDBCSInfo*)CPLCalloc(1, sizeof(AVCDBCSInfo));

    psInfo->nDBCSCodePage = AVCGetDBCSCodePage();
    psInfo->nDBCSEncoding = AVC_CODE_UNKNOWN;
    psInfo->pszDBCSBuf    = NULL;
    psInfo->nDBCSBufSize  = 0;

    return psInfo;
}

/**********************************************************************
 *                          AVCFreeDBCSInfo()
 *
 * Release all memory associated with a AVCDBCSInfo structure.
 **********************************************************************/
void AVCFreeDBCSInfo(AVCDBCSInfo *psInfo)
{
    if (psInfo)
    {
        CPLFree(psInfo->pszDBCSBuf);
        CPLFree(psInfo);
    }
}

/**********************************************************************
 *                          AVCGetDBCSCodePage()
 *
 * Fetch current multibyte codepage on the system.
 * Returns a valid codepage number, or 0 if the codepage is single byte or
 * unsupported.
 **********************************************************************/
int AVCGetDBCSCodePage(void)
{
#ifdef _WIN32
    int nCP;
    nCP = _getmbcp();

    /* Check if that's a supported codepage */
    if (nCP == AVC_DBCS_JAPANESE)
        return nCP;
#endif

    return 0;
}


/**********************************************************************
 *                          AVCE00DetectEncoding()
 *
 * Try to detect the encoding used in the current file by examining lines
 * of input.
 *
 * Returns TRUE once the encoding is established, or FALSE if more lines
 * of input are required to establish the encoding.
 **********************************************************************/
GBool AVCE00DetectEncoding(AVCDBCSInfo *psDBCSInfo, const GByte *pszLine)
{
    if (psDBCSInfo == NULL || psDBCSInfo->nDBCSCodePage == 0 ||
        psDBCSInfo->nDBCSEncoding != AVC_CODE_UNKNOWN)
    {
        /* Either single byte codepage, or encoding has already been detected
         */
        return TRUE;
    }

    switch (psDBCSInfo->nDBCSCodePage)
    {
      case AVC_DBCS_JAPANESE:
        psDBCSInfo->nDBCSEncoding =
                  _AVCDetectJapaneseEncoding(pszLine);
        break;
      default:
        psDBCSInfo->nDBCSEncoding = AVC_CODE_UNKNOWN;
        return TRUE;  /* Codepage not supported... no need to scan more lines*/
    }

    if (psDBCSInfo->nDBCSEncoding != AVC_CODE_UNKNOWN)
        return TRUE;  /* We detected the encoding! */

    return FALSE;
}


/**********************************************************************
 *                          AVCE00Convert2ArcDBCS()
 *
 * If encoding is still unknown, try to detect the encoding used in the
 * current file, and then convert the string to an encoding validfor output
 * to a coverage.
 *
 * Returns a reference to a const buffer that should not be freed by the
 * caller.  It can be either the original string buffer or a ref. to an
 * internal buffer.
 **********************************************************************/
const GByte *AVCE00Convert2ArcDBCS(AVCDBCSInfo *psDBCSInfo,
                                       const GByte *pszLine,
                                       int nMaxOutputLen)
{
    const GByte *pszOutBuf = NULL;
    GByte *pszTmp = NULL;
    GBool bAllAscii;

    if (psDBCSInfo == NULL ||
        psDBCSInfo->nDBCSCodePage == 0 || pszLine == NULL)
    {
        /* Single byte codepage... nothing to do
         */
        return pszLine;
    }

    /* If string is all ASCII then there is nothing to do...
     */
    pszTmp = (GByte *)pszLine;
    for(bAllAscii = TRUE ; bAllAscii && pszTmp && *pszTmp; pszTmp++)
    {
        if ( !IS_ASCII(*pszTmp) )
            bAllAscii = FALSE;
    }
    if (bAllAscii)
        return pszLine;

    /* Make sure output buffer is large enough.
     * We add 2 chars to buffer size to simplify processing... no need to
     * check if second byte of a pair would overflow buffer.
     */
    if (psDBCSInfo->pszDBCSBuf == NULL ||
        psDBCSInfo->nDBCSBufSize < nMaxOutputLen+2)
    {
        psDBCSInfo->nDBCSBufSize = nMaxOutputLen+2;
        psDBCSInfo->pszDBCSBuf =
            (GByte *)CPLRealloc(psDBCSInfo->pszDBCSBuf,
                                psDBCSInfo->nDBCSBufSize*
                                sizeof(GByte));
    }

    /* Do the conversion according to current code page
     */
    switch (psDBCSInfo->nDBCSCodePage)
    {
      case AVC_DBCS_JAPANESE:
        pszOutBuf = _AVCJapanese2ArcDBCS(psDBCSInfo,
                                         pszLine,
                                         nMaxOutputLen);
        break;
      default:
        /* We should never get here anyways, but just in case return pszLine
         */
        CPLAssert( FALSE ); /* Should never get here. */
        pszOutBuf = pszLine;
    }

    return pszOutBuf;
}

/**********************************************************************
 *                          AVCE00ConvertFromArcDBCS()
 *
 * Convert DBCS encoding in binary coverage files to E00 encoding.
 *
 * Returns a reference to a const buffer that should not be freed by the
 * caller.  It can be either the original string buffer or a ref. to an
 * internal buffer.
 **********************************************************************/
const GByte *AVCE00ConvertFromArcDBCS(AVCDBCSInfo *psDBCSInfo,
                                      const GByte *pszLine,
                                      int nMaxOutputLen)
{
    const GByte *pszOutBuf = NULL;
    GByte *pszTmp;
    GBool bAllAscii;

    if (psDBCSInfo == NULL ||
        psDBCSInfo->nDBCSCodePage == 0 || pszLine == NULL)
    {
        /* Single byte codepage... nothing to do
         */
        return pszLine;
    }

    /* If string is all ASCII then there is nothing to do...
     */
    pszTmp = (GByte *)pszLine;
    for(bAllAscii = TRUE ; bAllAscii && pszTmp && *pszTmp; pszTmp++)
    {
        if ( !IS_ASCII(*pszTmp) )
            bAllAscii = FALSE;
    }
    if (bAllAscii)
        return pszLine;

    /* Make sure output buffer is large enough.
     * We add 2 chars to buffer size to simplify processing... no need to
     * check if second byte of a pair would overflow buffer.
     */
    if (psDBCSInfo->pszDBCSBuf == NULL ||
        psDBCSInfo->nDBCSBufSize < nMaxOutputLen+2)
    {
        psDBCSInfo->nDBCSBufSize = nMaxOutputLen+2;
        psDBCSInfo->pszDBCSBuf =
            (GByte *)CPLRealloc(psDBCSInfo->pszDBCSBuf,
                                psDBCSInfo->nDBCSBufSize*
                                sizeof(GByte));
    }

    /* Do the conversion according to current code page
     */
    switch (psDBCSInfo->nDBCSCodePage)
    {
      case AVC_DBCS_JAPANESE:
        pszOutBuf = _AVCArcDBCS2JapaneseShiftJIS(psDBCSInfo,
                                                 pszLine,
                                                 nMaxOutputLen);
        break;
      default:
        /* We should never get here anyways, but just in case return pszLine
         */
        pszOutBuf = pszLine;
    }

    return pszOutBuf;
}

/*=====================================================================
 *=====================================================================
 * Functions Specific to Japanese encoding (CodePage 932).
 *
 * For now we assume that we can receive only Katakana, Shift-JIS, or EUC
 * encoding as input.  Coverages use EUC encoding in most cases, except
 * for Katakana characters that are prefixed with a 0x8e byte.
 *
 * Most of the Japanese conversion functions are based on information and
 * algorithms found at:
 *  http://www.mars.dti.ne.jp/~torao/program/appendix/japanese-en.html
 *=====================================================================
 *====================================================================*/

/**********************************************************************
 *                          _AVCDetectJapaneseEncoding()
 *
 * Scan a line of text to try to establish the type of japanese encoding
 *
 * Returns the encoding number (AVC_CODE_JAP_*), or AVC_CODE_UNKNOWN if no
 * specific encoding was detected.
 **********************************************************************/

#define IS_JAP_SHIFTJIS_1(c)  ((c) >= 0x81 && (c) <= 0x9f)
#define IS_JAP_SHIFTJIS_2(c)  (((c) >= 0x40 && (c) <= 0x7e) ||   \
                               ((c) >= 0x80 && (c) <= 0xA0) )
#define IS_JAP_EUC_1(c)       ((c) >= 0xF0 && (c) <= 0xFE)
#define IS_JAP_EUC_2(c)       ((c) >= 0xFD && (c) <= 0xFE)
#define IS_JAP_KANA(c)        ((c) >= 0xA1 && (c) <= 0xDF)

static int _AVCDetectJapaneseEncoding(const GByte *pszLine)
{
    int nEncoding = AVC_CODE_UNKNOWN;

    for( ; nEncoding == AVC_CODE_UNKNOWN && pszLine && *pszLine; pszLine++)
    {
        if (IS_ASCII(*pszLine))
            continue;
        else if (IS_JAP_SHIFTJIS_1(*pszLine))
        {
            nEncoding = AVC_CODE_JAP_SHIFTJIS;
            break;
        }
        else if (IS_JAP_KANA(*pszLine) && *(pszLine+1) &&
                 (IS_ASCII(*(pszLine+1)) ||
                  (*(pszLine+1)>=0x80 && *(pszLine+1)<=0xA0) ) )
        {
            nEncoding = AVC_CODE_JAP_SHIFTJIS; /* SHIFT-JIS + Kana */
            break;
        }
        else if (IS_JAP_EUC_1(*pszLine))
        {
            nEncoding = AVC_CODE_JAP_EUC;
            break;
        }

        if (*(++pszLine) == '\0')
            break;

        if (IS_JAP_SHIFTJIS_2(*pszLine))
        {
            nEncoding = AVC_CODE_JAP_SHIFTJIS;
            break;
        }
        else if (IS_JAP_EUC_2(*pszLine))
        {
            nEncoding = AVC_CODE_JAP_EUC;
            break;
        }
    }

    return nEncoding;
}


/**********************************************************************
 *                          _AVCJapanese2ArcDBCS()
 *
 * Try to detect type of Japanese encoding if not done yet, and convert
 * string from Japanese to proper coverage DBCS encoding.
 **********************************************************************/
static const GByte *_AVCJapanese2ArcDBCS(AVCDBCSInfo *psDBCSInfo,
                                         const GByte *pszLine,
                                         int nMaxOutputLen)
{
    GByte *pszOut;
    int iDst;

    pszOut = psDBCSInfo->pszDBCSBuf;

    if (psDBCSInfo->nDBCSEncoding == AVC_CODE_UNKNOWN)
    {
        /* Type of encoding (Shift-JIS or EUC) not known yet... try to
         * detect it now.
         */
        psDBCSInfo->nDBCSEncoding = _AVCDetectJapaneseEncoding(pszLine);

/*
        if (psDBCSInfo->nDBCSEncoding == AVC_CODE_JAP_SHIFTJIS)
        {
            printf("Found Japanese Shift-JIS encoding\n");
        }
        else if (psDBCSInfo->nDBCSEncoding == AVC_CODE_JAP_EUC)
        {
            printf("Found Japanese EUC encoding\n");
        }
*/
    }

    for(iDst=0; *pszLine && iDst < nMaxOutputLen; pszLine++)
    {
        if (IS_ASCII(*pszLine))
        {
            /* No transformation required for ASCII */
            pszOut[iDst++] = *pszLine;
        }
        else if ( psDBCSInfo->nDBCSEncoding==AVC_CODE_JAP_EUC && *(pszLine+1) )
        {
            /* This must be a pair of EUC chars and both should be in
             * the range 0xA1-0xFE
             */
            pszOut[iDst++] = *(pszLine++);
            pszOut[iDst++] = *pszLine;
        }
        else if ( IS_JAP_KANA(*pszLine) )
        {
            /* Katakana char. prefix it with 0x8e */
            pszOut[iDst++] = 0x8e;
            pszOut[iDst++] = *pszLine;
        }
        else if ( *(pszLine+1) )
        {
            /* This must be a pair of Shift-JIS chars... convert them to EUC
             *
             * If we haven't been able to establish the encoding for sure
             * yet, then it is possible that a pair of EUC chars could be
             * treated as shift-JIS here... but there is not much we can do
             * about that unless we scan the whole E00 input before we
             * start the conversion.
             */
            unsigned char leader, trailer;
            leader = *(pszLine++);
            trailer = *pszLine;

            if(leader <= 0x9F)  leader -= 0x71;
            else                leader -= 0xB1;
            leader = (leader << 1) + 1;

            if(trailer > 0x7F)  trailer --;
            if(trailer >= 0x9E)
            {
                trailer -= 0x7D;
                leader ++;
            }
            else
            {
                trailer -= 0x1F;
            }

            pszOut[iDst++] = leader | 0x80;
            pszOut[iDst++] = trailer | 0x80;
        }
        else
        {
            /* We should never get here unless a double-byte pair was
             * truncated... but just in case...
             */
            pszOut[iDst++] = *pszLine;
        }

    }

    pszOut[iDst] = '\0';

    return psDBCSInfo->pszDBCSBuf;
}


/**********************************************************************
 *                          _AVCArcDBCS2JapaneseShiftJIS()
 *
 * Convert string from coverage DBCS (EUC) to Japanese Shift-JIS.
 *
 * We know that binary coverages use a custom EUC encoding for japanese
 * which is EUC + all Katakana chars are prefixed with 0x8e.  So this
 * function just does a simple conversion.
 **********************************************************************/
static const GByte *_AVCArcDBCS2JapaneseShiftJIS(AVCDBCSInfo *psDBCSInfo,
                                                 const GByte *pszLine,
                                                 int nMaxOutputLen)
{
    GByte *pszOut;
    int iDst;

    pszOut = psDBCSInfo->pszDBCSBuf;

    for(iDst=0; *pszLine && iDst < nMaxOutputLen; pszLine++)
    {
        if (IS_ASCII(*pszLine))
        {
            /* No transformation required for ASCII */
            pszOut[iDst++] = *pszLine;
        }
        else if (*pszLine == 0x8e && *(pszLine+1))
        {
            pszLine++;  /* Flush the 0x8e */
            pszOut[iDst++] = *pszLine;
        }
        else if (*(pszLine+1))
        {
            /* This is a pair of EUC chars... convert them to Shift-JIS
             */
            unsigned char leader, trailer;
            leader  = *(pszLine++) & 0x7F;
            trailer = *pszLine & 0x7F;

            if((leader & 0x01) != 0)    trailer += 0x1F;
            else                        trailer += 0x7D;
            if(trailer >= 0x7F)         trailer ++;

            leader = ((leader - 0x21) >> 1) + 0x81;
            if(leader > 0x9F)          leader += 0x40;

            pszOut[iDst++] = leader;
            pszOut[iDst++] = trailer;
        }
        else
        {
            /* We should never get here unless a double-byte pair was
             * truncated... but just in case...
             */
            pszOut[iDst++] = *pszLine;
        }

    }

    pszOut[iDst] = '\0';

    return psDBCSInfo->pszDBCSBuf;
}
