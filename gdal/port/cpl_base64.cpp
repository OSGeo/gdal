/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library
 * Purpose:  Encoding/Decoding Base64 strings
 * Author:   Paul Ramsey <pramsey@cleverelephant.ca>
 *           Dave Blasby <dblasby@gmail.com>
 *           René Nyffenegger
 *
 ******************************************************************************
 * Copyright (c) 2008 Paul Ramsey
 * Copyright (c) 2002 Refractions Research
 * Copyright (C) 2004-2008 René Nyffenegger
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * (see also part way down the file for license terms for René's code)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies of this Software or works derived from this Software.
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

CPL_CVSID("$Id$");

/* Derived from MapServer's mappostgis.c */

/*
** Decode a base64 character.
*/
static const unsigned char CPLBase64DecodeChar[256] = {
    /* not Base64 characters */
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,
    /*  +  */
    62,
    /* not Base64 characters */
    64,64,64,
    /*  /  */
    63,
    /* 0-9 */
    52,53,54,55,56,57,58,59,60,61,
    /* not Base64 characters */
    64,64,64,64,64,64,64,
    /* A-Z */
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
    /* not Base64 characters */
    64,64,64,64,64,64,
    /* a-z */
    26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    /* not Base64 characters */
    64,64,64,64,64,
    /* not Base64 characters (upper 128 characters) */
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
    };

/************************************************************************/
/*                       CPLBase64DecodeInPlace()                       */
/*                                                                      */
/*      Decode base64 string "pszBase64" (null terminated) in place     */
/*      Returns length of decoded array or 0 on failure.                */
/************************************************************************/

int CPLBase64DecodeInPlace(GByte* pszBase64)
{
    if (pszBase64 && *pszBase64) {

        unsigned char *p = pszBase64;
        int i, j, k;

        /* Drop illegal chars first */
        for (i=0, j=0; pszBase64[i]; i++) {
            unsigned char c = pszBase64[i];
            if ( (CPLBase64DecodeChar[c] != 64) || (c == '=') ) {
                pszBase64[j++] = c;
            }
        }

        for (k=0; k<j; k+=4) {
            register unsigned char b1, b2, b3, b4, c3, c4;

            b1 = CPLBase64DecodeChar[pszBase64[k]];

            if (k+3<j) {
                b2 = CPLBase64DecodeChar[pszBase64[k+1]];
                c3 = pszBase64[k+2];
                c4 = pszBase64[k+3];
            }
            else if (k+2<j) {
                b2 = CPLBase64DecodeChar[pszBase64[k+1]];
                c3 = pszBase64[k+2];
                c4 = 'A';
            }
            else if (k+1<j) {
                b2 = CPLBase64DecodeChar[pszBase64[k+1]];
                c3 = 'A';
                c4 = 'A';
            }
            else
            {
                b2 = 0;
                c3 = 'A';
                c4 = 'A';
            }

            b3 = CPLBase64DecodeChar[c3];
            b4 = CPLBase64DecodeChar[c4];

            *p++=((b1<<2)|(b2>>4) );
            if( p - pszBase64 == i )
                break;
            if (c3 != '=') {
                *p++=(((b2&0xf)<<4)|(b3>>2) );
                if( p - pszBase64 == i )
                    break;
            }
            if (c4 != '=') {
                *p++=(((b3&0x3)<<6)|b4 );
                if( p - pszBase64 == i )
                    break;
            }
        }
        return(p-pszBase64);
    }
    return 0;
}

/*
 * This function was extracted from the base64 cpp utility published by
 * René Nyffenegger. The code was modified into a form suitable for use in
 * CPL.  The original code can be found at 
 * http://www.adp-gmbh.ch/cpp/common/base64.html.
 *
 * The following is the original notice of this function.
 *
 * base64.cpp and base64.h
 *
 *  Copyright (C) 2004-2008 René Nyffenegger
 *
 *  This source code is provided 'as-is', without any express or implied
 *  warranty. In no event will the author be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1. The origin of this source code must not be misrepresented; you must not
 *     claim that you wrote the original source code. If you use this source code
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *
 *  2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original source code.
 *
 *  3. This notice may not be removed or altered from any source distribution.
 *
 *  René Nyffenegger rene.nyffenegger@adp-gmbh.ch
*/

/************************************************************************/
/*                          CPLBase64Encode()                           */
/************************************************************************/

char *CPLBase64Encode( int nDataLen, const GByte *pabyBytesToEncode )

{
    static const std::string base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int           i = 0;
    int           j = 0;
    std::string   result("");
    unsigned char charArray3[3];
    unsigned char charArray4[4];

    while( nDataLen-- )
    {
        charArray3[i++] = *(pabyBytesToEncode++);

        if( i == 3 )
        {
            charArray4[0] = (charArray3[0] & 0xfc) >> 2;
            charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
            charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
            charArray4[3] = charArray3[2] & 0x3f;

            for( i = 0; i < 4; i++ )
            {
                result += base64Chars[charArray4[i]];
            }

            i = 0;
        }
    }

    if( i )
    {
        for( j = i; j < 3; j++ )
        {
            charArray3[j] = '\0';
        }

        charArray4[0] = (charArray3[0]  & 0xfc) >> 2;
        charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
        charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
        charArray4[3] = charArray3[2] & 0x3f;

        for ( j = 0; j < (i + 1); j++ )
        {
            result += base64Chars[charArray4[j]];
        }

        while( i++ < 3 )
            result += '=';
    }

    return (CPLStrdup(result.c_str()));
}

