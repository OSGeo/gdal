/******************************************************************************
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
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "cpl_string.h"

#include <string>

#include "cpl_conv.h"

CPL_CVSID("$Id$")

// Derived from MapServer's mappostgis.c.

/*
** Decode a base64 character.
*/
constexpr unsigned char CPLBase64DecodeChar[256] = {
    // Not Base64 characters.
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,
    // +
    62,
    // Not Base64 characters.
    64,64,64,
    //  /
    63,
    // 0-9
    52,53,54,55,56,57,58,59,60,61,
    // Not Base64 characters.
    64,64,64,64,64,64,64,
    // A-Z
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
    // Not Base64 characters.
    64,64,64,64,64,64,
    // a-z
    26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,
    51,
    // Not Base64 characters.
    64,64,64,64,64,
    // Not Base64 characters (upper 128 characters).
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
/************************************************************************/

/** Decode base64 string "pszBase64" (null terminated) in place.
 *
 * Returns length of decoded array or 0 on failure.
 */
int CPLBase64DecodeInPlace( GByte* pszBase64 )
{
    if( pszBase64 && *pszBase64 )
    {
        unsigned char *p = pszBase64;
        int offset_1 = 0;
        int offset_2 = 0;

        // Drop illegal chars first.
        for( ; pszBase64[offset_1]; ++offset_1 )
        {
            unsigned char c = pszBase64[offset_1];
            if( (CPLBase64DecodeChar[c] != 64) || (c == '=') )
            {
                pszBase64[offset_2++] = c;
            }
        }

        for( int idx = 0; idx < offset_2; idx += 4 )
        {
            unsigned char b1 = CPLBase64DecodeChar[pszBase64[idx]];
            unsigned char b2 = 0;
            unsigned char c3 = 'A';
            unsigned char c4 = 'A';

            if( idx + 3 < offset_2 )
            {
                b2 = CPLBase64DecodeChar[pszBase64[idx+1]];
                c3 = pszBase64[idx+2];
                c4 = pszBase64[idx+3];
            }
            else if( idx + 2 < offset_2 )
            {
                b2 = CPLBase64DecodeChar[pszBase64[idx+1]];
                c3 = pszBase64[idx+2];
            }
            else if( idx + 1 < offset_2 )
            {
                b2 = CPLBase64DecodeChar[pszBase64[idx+1]];
                // c3 = 'A';
            }  // Else: Use the default values.

            const unsigned char b3 = CPLBase64DecodeChar[c3];
            const unsigned char b4 = CPLBase64DecodeChar[c4];

            *p++ = ( (b1 << 2) | (b2 >> 4) );
            if( p - pszBase64 == offset_1 )
                break;
            if( c3 != '=' )
            {
                *p++ = ( ((b2 & 0xf) << 4) | (b3 >> 2) );
                if( p - pszBase64 == offset_1 )
                    break;
            }
            if( c4 != '=' )
            {
                *p++ = ( ((b3 & 0x3) << 6) | b4);
                if( p - pszBase64 == offset_1 )
                    break;
            }
        }
        return static_cast<int>(p-pszBase64);
    }
    return 0;
}

/*
 * This function was extracted from the base64 cpp utility published by
 * René Nyffenegger. The code was modified into a form suitable for use in
 * CPL.  The original code can be found at:
 *
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
 *     claim that you wrote the original source code. If you use this source
 *     code in a product, an acknowledgment in the product documentation would
 *     be appreciated but is not required.
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

/** Base64 encode a buffer. */

char *CPLBase64Encode(int nDataLen, const GByte *pabyBytesToEncode)
{
    constexpr char base64Chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    const int kCharArray3Size = 3;
    const int kCharArray4Size = 4;
    unsigned char charArray3[kCharArray3Size] = {};

    std::string result("");
    int array3_idx = 0;
    while( nDataLen-- )
    {
        charArray3[array3_idx++] = *(pabyBytesToEncode++);

        if( array3_idx == kCharArray3Size )
        {
            const unsigned char charArray4[kCharArray4Size] = {
                static_cast<unsigned char>( (charArray3[0] & 0xfc) >> 2),
                static_cast<unsigned char>(((charArray3[0] & 0x03) << 4) +
                                           ((charArray3[1] & 0xf0) >> 4)),
                static_cast<unsigned char>(((charArray3[1] & 0x0f) << 2) +
                                           ((charArray3[2] & 0xc0) >> 6)),
                static_cast<unsigned char>(  charArray3[2] & 0x3f)
            };

            for( int idx = 0; idx < kCharArray4Size; ++idx )
            {
                result += base64Chars[charArray4[idx]];
            }

            array3_idx = 0;
        }
    }

    if( array3_idx )
    {
        for( int idx = array3_idx; idx < kCharArray3Size; ++idx )
        {
            charArray3[idx] = '\0';
        }

        const unsigned char charArray4[kCharArray4Size] = {
            static_cast<unsigned char>( (charArray3[0] & 0xfc) >> 2),
            static_cast<unsigned char>(((charArray3[0] & 0x03) << 4) +
                                       ((charArray3[1] & 0xf0) >> 4)),
            static_cast<unsigned char>(((charArray3[1] & 0x0f) << 2) +
                                       ((charArray3[2] & 0xc0) >> 6)),
            static_cast<unsigned char>(  charArray3[2] & 0x3f)
        };

        for( int idx = 0; idx < (array3_idx + 1); ++idx )
        {
            result += base64Chars[charArray4[idx]];
        }

        while( array3_idx++ < kCharArray3Size )
            result += '=';
    }

    return CPLStrdup(result.c_str());
}
