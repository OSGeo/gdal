/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Generate a mapping table from a 1-byte encoding to unicode,
 *           for ogr_expat.cpp
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static unsigned utf8decode(const char* p, const char* end, int* len)
{
  unsigned char c = *(unsigned char*)p;
  if (c < 0x80) {
    *len = 1;
    return c;
#if ERRORS_TO_CP1252
  } else if (c < 0xa0) {
    *len = 1;
    return cp1252[c-0x80];
#endif
  } else if (c < 0xc2) {
    goto FAIL;
  }
  if (p+1 >= end || (p[1]&0xc0) != 0x80) goto FAIL;
  if (c < 0xe0) {
    *len = 2;
    return
      ((p[0] & 0x1f) << 6) +
      ((p[1] & 0x3f));
  } else if (c == 0xe0) {
    if (((unsigned char*)p)[1] < 0xa0) goto FAIL;
    goto UTF8_3;
#if STRICT_RFC3629
  } else if (c == 0xed) {
    // RFC 3629 says surrogate chars are illegal.
    if (((unsigned char*)p)[1] >= 0xa0) goto FAIL;
    goto UTF8_3;
  } else if (c == 0xef) {
    // 0xfffe and 0xffff are also illegal characters
    if (((unsigned char*)p)[1]==0xbf &&
    ((unsigned char*)p)[2]>=0xbe) goto FAIL;
    goto UTF8_3;
#endif
  } else if (c < 0xf0) {
  UTF8_3:
    if (p+2 >= end || (p[2]&0xc0) != 0x80) goto FAIL;
    *len = 3;
    return
      ((p[0] & 0x0f) << 12) +
      ((p[1] & 0x3f) << 6) +
      ((p[2] & 0x3f));
  } else if (c == 0xf0) {
    if (((unsigned char*)p)[1] < 0x90) goto FAIL;
    goto UTF8_4;
  } else if (c < 0xf4) {
  UTF8_4:
    if (p+3 >= end || (p[2]&0xc0) != 0x80 || (p[3]&0xc0) != 0x80) goto FAIL;
    *len = 4;
#if STRICT_RFC3629
    // RFC 3629 says all codes ending in fffe or ffff are illegal:
    if ((p[1]&0xf)==0xf &&
    ((unsigned char*)p)[2] == 0xbf &&
    ((unsigned char*)p)[3] >= 0xbe) goto FAIL;
#endif
    return
      ((p[0] & 0x07) << 18) +
      ((p[1] & 0x3f) << 12) +
      ((p[2] & 0x3f) << 6) +
      ((p[3] & 0x3f));
  } else if (c == 0xf4) {
    if (((unsigned char*)p)[1] > 0x8f) goto FAIL; // after 0x10ffff
    goto UTF8_4;
  } else {
  FAIL:
    *len = 1;
#if ERRORS_TO_ISO8859_1
    return c;
#else
    return 0xfffd; // Unicode REPLACEMENT CHARACTER
#endif
  }
}

int main(int argc, char* argv[])
{
    iconv_t sConv;
    const char* pszSrcEncoding;
    const char* pszDstEncoding = "UTF-8";
    int i;
    int nLastIdentical = -1;

    if( argc != 2 )
    {
        fprintf(stderr, "Usage: generate_encoding_table encoding_name\n");
        return 1;
    }

    pszSrcEncoding = argv[1];

    sConv = iconv_open( pszDstEncoding, pszSrcEncoding );

    if ( sConv == (iconv_t)-1 )
    {
        fprintf(stderr, 
                  "Recode from %s to %s failed with the error: \"%s\".", 
                  pszSrcEncoding, pszDstEncoding, strerror(errno) );
        return 1;
    }

    for(i = 0; i < 256; i++)
    {
        char szSrcBuf[2] = {(char)i, 0};
        char szDstBuf[5] = {0,0,0,0,0};
        char *pszSrcBuf = szSrcBuf;
        char *pszDstBuf = szDstBuf;
        size_t  nSrcLen = strlen( szSrcBuf );
        size_t  nDstLen = sizeof(szDstBuf);
        size_t  nConverted =
            iconv( sConv, &pszSrcBuf, &nSrcLen, &pszDstBuf, &nDstLen );

        int nUnicode = -1;
        if( nConverted == -1 )
        {
            if ( errno == EILSEQ )
            {
                /* fprintf(stderr, "EILSEQ for %d\n", i); */
            }

            else if ( errno == E2BIG )
            {
                fprintf(stderr, "E2BIG for %d\n", i);
                return 1;
            }
            else
            {
                fprintf(stderr, "other error for %d\n", i);
                return 1;
            }
        }
        else
        {
            int len;
            nUnicode = utf8decode(szDstBuf, szDstBuf + strlen(szDstBuf), &len);
            if( nUnicode == 0xfffd )
                nUnicode = -1;
        }

        if( nLastIdentical >= 0 && i != nUnicode )
        {
            if( nLastIdentical + 1 == i )
                printf("info->map[0x%02X] = 0x%02X;\n", nLastIdentical, nLastIdentical);
            else
            {
                printf("for(i = 0x%02X; i < 0x%02X; i++)\n", nLastIdentical, i);
                printf("    info->map[i] = i;\n");
            }
            nLastIdentical = -1;
        }

        if( nUnicode < 0 )
            printf("info->map[0x%02X] = -1;\n", i);
        else if (nUnicode <= 0xFF )
        {
            if( i == nUnicode )
            {
                if( nLastIdentical < 0 )
                    nLastIdentical = i;
            }
            else
                printf("info->map[0x%02X] = 0x%02X;\n", i, nUnicode);
        }
        else if (nUnicode <= 0xFFFF )
            printf("info->map[0x%02X] = 0x%04X;\n", i, nUnicode);
        else if (nUnicode <= 0xFFFFFF )
            printf("info->map[0x%02X] = 0x%06X;\n", i, nUnicode);
        else
            printf("info->map[0x%02X] = 0x%08X;\n", i, nUnicode);
    }

    if( nLastIdentical >= 0 )
    {
        if( nLastIdentical + 1 == i )
            printf("info->map[0x%02X] = 0x%02X;\n", nLastIdentical, nLastIdentical);
        else
        {
            printf("for(i = 0x%02X; i < 0x%02X; i++)\n", nLastIdentical, i);
            printf("    info->map[i] = i;\n");
        }
        nLastIdentical = -1;
    }

    iconv_close( sConv );

    return 0;
}
