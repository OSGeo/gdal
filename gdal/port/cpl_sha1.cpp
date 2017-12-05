/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement SHA1
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 * SHA1 computation coming from Public Domain code at:
 * https://github.com/B-Con/crypto-algorithms/blob/master/sha1.c
 * by Brad Conte (brad AT bradconte.com)
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#include <string.h>
#include "cpl_sha1.h"

CPL_CVSID("$Id$")


typedef struct {
    GByte data[64];
    GUInt32 datalen;
    GUIntBig bitlen;
    GUInt32 state[5];
} CPL_SHA1Context;

#define ROTLEFT(a, b) ((a << b) | (a >> (32 - b)))

/************************************************************************/
/*                         sha1_transform()                             */
/************************************************************************/

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static
void sha1_transform(CPL_SHA1Context *ctx, const GByte data[])
{
    GUInt32 a, b, c, d, e, i, j, t, m[80];

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) + (data[j + 1] << 16) +
               (data[j + 2] << 8) + (data[j + 3]);
    for ( ; i < 80; ++i) {
        m[i] = (m[i - 3] ^ m[i - 8] ^ m[i - 14] ^ m[i - 16]);
        m[i] = (m[i] << 1) | (m[i] >> 31);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for (i = 0; i < 20; ++i) {
        t = ROTLEFT(a, 5) + ((b & c) ^ (~b & d)) + e + 0x5a827999U + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for ( ; i < 40; ++i) {
        t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + 0x6ed9eba1U + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for ( ; i < 60; ++i) {
        t = ROTLEFT(a, 5) + ((b & c) ^ (b & d) ^ (c & d))  + e +
            0x8f1bbcdcU + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for ( ; i < 80; ++i) {
        t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + 0xca62c1d6U + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

/************************************************************************/
/*                           CPL_SHA1Init()                             */
/************************************************************************/

static
void CPL_SHA1Init(CPL_SHA1Context *ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x67452301U;
    ctx->state[1] = 0xEFCDAB89U;
    ctx->state[2] = 0x98BADCFEU;
    ctx->state[3] = 0x10325476U;
    ctx->state[4] = 0xc3d2e1f0U;
}

/************************************************************************/
/*                          CPL_SHA1Update()                            */
/************************************************************************/

static
void CPL_SHA1Update(CPL_SHA1Context *ctx, const GByte data[], size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
                sha1_transform(ctx, ctx->data);
                ctx->bitlen += 512;
                ctx->datalen = 0;
        }
    }
}

/************************************************************************/
/*                           CPL_SHA1Final()                            */
/************************************************************************/

static
void CPL_SHA1Final(CPL_SHA1Context *ctx, GByte hash[CPL_SHA1_HASH_SIZE])
{
    GUInt32 i;

    i = ctx->datalen;

    // Pad whatever data is left in the buffer.
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0x00;
    }
    else {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0x00;
        sha1_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    // Append to the padding the total message's length in bits and transform.
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = static_cast<GByte>((ctx->bitlen) & 0xFFU);
    ctx->data[62] = static_cast<GByte>((ctx->bitlen >> 8) & 0xFFU);
    ctx->data[61] = static_cast<GByte>((ctx->bitlen >> 16) & 0xFFU);
    ctx->data[60] = static_cast<GByte>((ctx->bitlen >> 24) & 0xFFU);
    ctx->data[59] = static_cast<GByte>((ctx->bitlen >> 32) & 0xFFU);
    ctx->data[58] = static_cast<GByte>((ctx->bitlen >> 40) & 0xFFU);
    ctx->data[57] = static_cast<GByte>((ctx->bitlen >> 48) & 0xFFU);
    ctx->data[56] = static_cast<GByte>((ctx->bitlen >> 56) & 0xFFU);
    sha1_transform(ctx, ctx->data);

    // Since this implementation uses little endian byte ordering and MD uses big endian,
    // reverse all the bytes when copying the final state to the output hash.
    for (i = 0; i < 4; ++i) {
        hash[i]      = static_cast<GByte>((ctx->state[0] >> (24 - i * 8)) & 0x000000ffU);
        hash[i + 4]  = static_cast<GByte>((ctx->state[1] >> (24 - i * 8)) & 0x000000ffU);
        hash[i + 8]  = static_cast<GByte>((ctx->state[2] >> (24 - i * 8)) & 0x000000ffU);
        hash[i + 12] = static_cast<GByte>((ctx->state[3] >> (24 - i * 8)) & 0x000000ffU);
        hash[i + 16] = static_cast<GByte>((ctx->state[4] >> (24 - i * 8)) & 0x000000ffU);
    }
}

/************************************************************************/
/*                              CPL_SHA1()                              */
/************************************************************************/

static
void CPL_SHA1( const void *data, size_t len, GByte hash[CPL_SHA1_HASH_SIZE] )
{
    CPL_SHA1Context sSHA1Ctxt;
    CPL_SHA1Init(&sSHA1Ctxt);
    CPL_SHA1Update(&sSHA1Ctxt, static_cast<const GByte*>(data), len);
    CPL_SHA1Final(&sSHA1Ctxt, hash);
    memset(&sSHA1Ctxt, 0, sizeof(sSHA1Ctxt));
}

/************************************************************************/
/*                           CPL_HMAC_SHA1()                            */
/************************************************************************/

#define CPL_HMAC_SHA1_BLOCKSIZE 64U

// See https://en.wikipedia.org/wiki/Hash-based_message_authentication_code#Implementation
void CPL_HMAC_SHA1(const void *pKey, size_t nKeyLen,
                             const void *pabyMessage, size_t nMessageLen,
                             GByte abyDigest[CPL_SHA1_HASH_SIZE])
{
    GByte abyPad[CPL_HMAC_SHA1_BLOCKSIZE] = {};
    if( nKeyLen > CPL_HMAC_SHA1_BLOCKSIZE )
    {
        CPL_SHA1(pKey, nKeyLen, abyPad);
    }
    else
    {
        memcpy(abyPad, pKey, nKeyLen);
    }

    // Compute ipad.
    for( size_t i = 0; i < CPL_HMAC_SHA1_BLOCKSIZE; i++ )
        abyPad[i] = 0x36 ^ abyPad[i];

    CPL_SHA1Context sSHA1Ctxt;
    CPL_SHA1Init(&sSHA1Ctxt);
    CPL_SHA1Update(&sSHA1Ctxt, abyPad, CPL_HMAC_SHA1_BLOCKSIZE);
    CPL_SHA1Update(&sSHA1Ctxt, static_cast<const GByte*>(pabyMessage),
                   nMessageLen);
    CPL_SHA1Final(&sSHA1Ctxt, abyDigest);

    // Compute opad.
    for( size_t i = 0; i < CPL_HMAC_SHA1_BLOCKSIZE; i++ )
        abyPad[i] = (0x36 ^ 0x5C) ^ abyPad[i];

    CPL_SHA1Init(&sSHA1Ctxt);
    CPL_SHA1Update(&sSHA1Ctxt, abyPad, CPL_HMAC_SHA1_BLOCKSIZE);
    CPL_SHA1Update(&sSHA1Ctxt, abyDigest, CPL_SHA1_HASH_SIZE);
    CPL_SHA1Final(&sSHA1Ctxt, abyDigest);

    memset(&sSHA1Ctxt, 0, sizeof(sSHA1Ctxt));
    memset(abyPad, 0, CPL_HMAC_SHA1_BLOCKSIZE);
}
