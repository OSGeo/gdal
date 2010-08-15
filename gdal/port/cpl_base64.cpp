/******************************************************************************
 * $Id$
 *
 * Project:  MapServer
 * Purpose:  Decoding Base64 strings
 * Author:   Paul Ramsey <pramsey@cleverelephant.ca>
 *           Dave Blasby <dblasby@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2008 Paul Ramsey
 * Copyright (c) 2002 Refractions Research
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

#include "cpl_base64.h"
#include "cpl_string.h"

/* From MapServer's mappostgis.c */

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

/*
** Decode base64 string "src" (null terminated)
** into "dest" (not null terminated).
** Returns length of decoded array or 0 on failure.
*/
int CPLBase64Decode(unsigned char *dest, const char *src, int srclen) {

    if (src && *src) {

        unsigned char *p = dest;
        int i, j, k;
        unsigned char *buf = (unsigned char*) CPLCalloc(srclen + 1, sizeof(unsigned char));

        /* Drop illegal chars first */
        for (i=0, j=0; src[i]; i++) {
            unsigned char c = src[i];
            if ( (CPLBase64DecodeChar[c] != 64) || (c == '=') ) {
                buf[j++] = c;
            }
        }

        for (k=0; k<j; k+=4) {
            register unsigned char c1='A', c2='A', c3='A', c4='A';
            register unsigned char b1=0, b2=0, b3=0, b4=0;

            c1 = buf[k];

            if (k+1<j) {
                c2 = buf[k+1];
            }
            if (k+2<j) {
                c3 = buf[k+2];
            }
            if (k+3<j) {
                c4 = buf[k+3];
            }

            b1 = CPLBase64DecodeChar[c1];
            b2 = CPLBase64DecodeChar[c2];
            b3 = CPLBase64DecodeChar[c3];
            b4 = CPLBase64DecodeChar[c4];

            *p++=((b1<<2)|(b2>>4) );
            if (c3 != '=') {
                *p++=(((b2&0xf)<<4)|(b3>>2) );
            }
            if (c4 != '=') {
                *p++=(((b3&0x3)<<6)|b4 );
            }
        }
        CPLFree(buf);
        return(p-dest);
    }
    return 0;
}
