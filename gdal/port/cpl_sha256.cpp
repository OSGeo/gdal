/* $Id$ */

/* CPL_SHA256* functions derived from http://code.google.com/p/ulib/source/browse/trunk/src/base/sha256sum.c?r=39 */

/* The MIT License

   Copyright (C) 2011 Zilong Tan (tzlloch@gmail.com)
   Copyright (C) 2015 Even Rouault <even.rouault at spatialys.com>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

/*
 *  Original code is derived from the author:
 *  Allan Saddi
 */

#include <string.h>
#include "cpl_sha256.h"

CPL_CVSID("$Id$");

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

#define Ch(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define Maj(x, y, z) (((x) & ((y) | (z))) | ((y) & (z)))
#define SIGMA0(x) (ROTR((x), 2) ^ ROTR((x), 13) ^ ROTR((x), 22))
#define SIGMA1(x) (ROTR((x), 6) ^ ROTR((x), 11) ^ ROTR((x), 25))
#define sigma0(x) (ROTR((x), 7) ^ ROTR((x), 18) ^ ((x) >> 3))
#define sigma1(x) (ROTR((x), 17) ^ ROTR((x), 19) ^ ((x) >> 10))

#define DO_ROUND() {                                                    \
                t1 = h + SIGMA1(e) + Ch(e, f, g) + *(Kp++) + *(W++);    \
                t2 = SIGMA0(a) + Maj(a, b, c);                          \
                h = g;                                                  \
                g = f;                                                  \
                f = e;                                                  \
                e = d + t1;                                             \
                d = c;                                                  \
                c = b;                                                  \
                b = a;                                                  \
                a = t1 + t2;                                            \
        }

static const GUInt32 K[64] = {
        0x428a2f98L, 0x71374491L, 0xb5c0fbcfL, 0xe9b5dba5L,
        0x3956c25bL, 0x59f111f1L, 0x923f82a4L, 0xab1c5ed5L,
        0xd807aa98L, 0x12835b01L, 0x243185beL, 0x550c7dc3L,
        0x72be5d74L, 0x80deb1feL, 0x9bdc06a7L, 0xc19bf174L,
        0xe49b69c1L, 0xefbe4786L, 0x0fc19dc6L, 0x240ca1ccL,
        0x2de92c6fL, 0x4a7484aaL, 0x5cb0a9dcL, 0x76f988daL,
        0x983e5152L, 0xa831c66dL, 0xb00327c8L, 0xbf597fc7L,
        0xc6e00bf3L, 0xd5a79147L, 0x06ca6351L, 0x14292967L,
        0x27b70a85L, 0x2e1b2138L, 0x4d2c6dfcL, 0x53380d13L,
        0x650a7354L, 0x766a0abbL, 0x81c2c92eL, 0x92722c85L,
        0xa2bfe8a1L, 0xa81a664bL, 0xc24b8b70L, 0xc76c51a3L,
        0xd192e819L, 0xd6990624L, 0xf40e3585L, 0x106aa070L,
        0x19a4c116L, 0x1e376c08L, 0x2748774cL, 0x34b0bcb5L,
        0x391c0cb3L, 0x4ed8aa4aL, 0x5b9cca4fL, 0x682e6ff3L,
        0x748f82eeL, 0x78a5636fL, 0x84c87814L, 0x8cc70208L,
        0x90befffaL, 0xa4506cebL, 0xbef9a3f7L, 0xc67178f2L
};

#ifdef WORDS_BIGENDIAN

#define BYTESWAP(x) (x)
#define BYTESWAP64(x) (x)

#else                           /* WORDS_BIGENDIAN */

#define BYTESWAP(x) ((ROTR((x), 8) & 0xff00ff00L) |     \
                     (ROTL((x), 8) & 0x00ff00ffL))
#define BYTESWAP64(x) _byteswap64(x)

static inline GUInt64 _byteswap64(GUInt64 x)
{
        GUInt32 a = x >> 32;
        GUInt32 b = (GUInt32) x;
        return ((GUInt64) BYTESWAP(b) << 32) | (GUInt64) BYTESWAP(a);
}

#endif /* !(WORDS_BIGENDIAN) */

static const GByte padding[64] = {
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void CPL_SHA256Init(CPL_SHA256Context * sc)
{
        sc->totalLength = 0;
        sc->hash[0] = 0x6a09e667L;
        sc->hash[1] = 0xbb67ae85L;
        sc->hash[2] = 0x3c6ef372L;
        sc->hash[3] = 0xa54ff53aL;
        sc->hash[4] = 0x510e527fL;
        sc->hash[5] = 0x9b05688cL;
        sc->hash[6] = 0x1f83d9abL;
        sc->hash[7] = 0x5be0cd19L;
        sc->bufferLength = 0L;
}

static GUInt32 burnStack(int size)
{
        GByte buf[128];
        GUInt32 ret = 0;

        memset(buf, (GByte)(size & 0xff), sizeof(buf));
        for( size_t i = 0; i < sizeof(buf); i++ )
            ret += ret * buf[i];
        size -= sizeof(buf);
        if (size > 0)
                ret += burnStack(size);
        return ret;
}

static void CPL_SHA256Guts(CPL_SHA256Context * sc, const GUInt32 * cbuf)
{
        GUInt32 buf[64];
        GUInt32 *W, *W2, *W7, *W15, *W16;
        GUInt32 a, b, c, d, e, f, g, h;
        GUInt32 t1, t2;
        const GUInt32 *Kp;
        int i;

        W = buf;

        for (i = 15; i >= 0; i--) {
                *(W++) = BYTESWAP(*cbuf);
                cbuf++;
        }

        W16 = &buf[0];
        W15 = &buf[1];
        W7 = &buf[9];
        W2 = &buf[14];

        for (i = 47; i >= 0; i--) {
                *(W++) = sigma1(*W2) + *(W7++) + sigma0(*W15) + *(W16++);
                W2++;
                W15++;
        }

        a = sc->hash[0];
        b = sc->hash[1];
        c = sc->hash[2];
        d = sc->hash[3];
        e = sc->hash[4];
        f = sc->hash[5];
        g = sc->hash[6];
        h = sc->hash[7];

        Kp = K;
        W = buf;

#ifndef CPL_SHA256_UNROLL
#define CPL_SHA256_UNROLL 1
#endif                          /* !CPL_SHA256_UNROLL */

#if CPL_SHA256_UNROLL == 1
        for (i = 63; i >= 0; i--)
                DO_ROUND();
#elif CPL_SHA256_UNROLL == 2
        for (i = 31; i >= 0; i--) {
                DO_ROUND();
                DO_ROUND();
        }
#elif CPL_SHA256_UNROLL == 4
        for (i = 15; i >= 0; i--) {
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
        }
#elif CPL_SHA256_UNROLL == 8
        for (i = 7; i >= 0; i--) {
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
        }
#elif CPL_SHA256_UNROLL == 16
        for (i = 3; i >= 0; i--) {
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
        }
#elif CPL_SHA256_UNROLL == 32
        for (i = 1; i >= 0; i--) {
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
                DO_ROUND();
        }
#elif CPL_SHA256_UNROLL == 64
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
        DO_ROUND();
#else
#error "CPL_SHA256_UNROLL must be 1, 2, 4, 8, 16, 32, or 64!"
#endif

        sc->hash[0] += a;
        sc->hash[1] += b;
        sc->hash[2] += c;
        sc->hash[3] += d;
        sc->hash[4] += e;
        sc->hash[5] += f;
        sc->hash[6] += g;
        sc->hash[7] += h;
}

void CPL_SHA256Update(CPL_SHA256Context * sc, const void *data, size_t len)
{
        GUInt32 bufferBytesLeft;
        GUInt32 bytesToCopy;
        int needBurn = 0;

        if (sc->bufferLength) {
                bufferBytesLeft = 64L - sc->bufferLength;

                bytesToCopy = bufferBytesLeft;
                if (bytesToCopy > len)
                        bytesToCopy = (GUInt32)len;

                memcpy(&sc->buffer.bytes[sc->bufferLength], data, bytesToCopy);

                sc->totalLength += bytesToCopy * 8L;

                sc->bufferLength += bytesToCopy;
                data = ((GByte *) data) + bytesToCopy;
                len -= bytesToCopy;

                if (sc->bufferLength == 64L) {
                        CPL_SHA256Guts(sc, sc->buffer.words);
                        needBurn = 1;
                        sc->bufferLength = 0L;
                }
        }

        while (len > 63L) {
                sc->totalLength += 512L;

                CPL_SHA256Guts(sc, (const GUInt32 *)data);
                needBurn = 1;

                data = ((GByte *) data) + 64L;
                len -= 64L;
        }

        if (len) {
                memcpy(&sc->buffer.bytes[sc->bufferLength], data, len);

                sc->totalLength += (GUInt32)len * 8L;

                sc->bufferLength += (GUInt32)len;
        }

        if (needBurn)
        {
                // Clean stack state of CPL_SHA256Guts()
                // We add dummy side effects to avoid burnStack() to be optimized away (#6157)
                static GUInt32 accumulator = 0;
                accumulator += burnStack(
                        sizeof(GUInt32[74]) + sizeof(GUInt32 *[6]) +
                        sizeof(int) + ((len%2) ? sizeof(int) : 0));
                if( accumulator == 0xDEADBEEF )
                    fprintf(stderr, "%s", "");
        }
}

void CPL_SHA256Final(CPL_SHA256Context * sc, GByte hash[CPL_SHA256_HASH_SIZE])
{
        GUInt32 bytesToPad;
        GUInt64 lengthPad;
        int i;

        bytesToPad = 120L - sc->bufferLength;
        if (bytesToPad > 64L)
                bytesToPad -= 64L;

        lengthPad = BYTESWAP64(sc->totalLength);

        CPL_SHA256Update(sc, padding, bytesToPad);
        CPL_SHA256Update(sc, &lengthPad, 8L);

        if (hash) {
                for (i = 0; i < CPL_SHA256_HASH_WORDS; i++) {
                        *((GUInt32 *) hash) = BYTESWAP(sc->hash[i]);
                        hash += 4;
                }
        }
}

void CPL_SHA256(const void *data, size_t len, GByte hash[CPL_SHA256_HASH_SIZE])
{
    CPL_SHA256Context sSHA256Ctxt;
    CPL_SHA256Init(&sSHA256Ctxt);
    CPL_SHA256Update(&sSHA256Ctxt, data, len);
    CPL_SHA256Final(&sSHA256Ctxt, hash);
    memset(&sSHA256Ctxt, 0, sizeof(sSHA256Ctxt));
}

#define CPL_HMAC_SHA256_BLOCKSIZE 64U

/* See https://en.wikipedia.org/wiki/Hash-based_message_authentication_code#Implementation */
void CPL_HMAC_SHA256(const void *pKey, size_t nKeyLen,
                     const void *pabyMessage, size_t nMessageLen,
                     GByte abyDigest[CPL_SHA256_HASH_SIZE])
{
    GByte abyPad[CPL_HMAC_SHA256_BLOCKSIZE];
    memset(abyPad, 0, CPL_HMAC_SHA256_BLOCKSIZE);
    if( nKeyLen > CPL_HMAC_SHA256_BLOCKSIZE )
    {
        CPL_SHA256(pKey, nKeyLen, abyPad);
    }
    else
    {
        memcpy(abyPad, pKey, nKeyLen);
    }

    /* Compute ipad */
    for( size_t i = 0; i < CPL_HMAC_SHA256_BLOCKSIZE; i++ )
        abyPad[i] = 0x36 ^ abyPad[i];

    CPL_SHA256Context sSHA256Ctxt;
    CPL_SHA256Init(&sSHA256Ctxt);
    CPL_SHA256Update(&sSHA256Ctxt, abyPad, CPL_HMAC_SHA256_BLOCKSIZE);
    CPL_SHA256Update(&sSHA256Ctxt, pabyMessage, nMessageLen);
    CPL_SHA256Final(&sSHA256Ctxt, abyDigest);

    /* Compute opad */
    for( size_t i = 0; i < CPL_HMAC_SHA256_BLOCKSIZE; i++ )
        abyPad[i] = (0x36 ^ 0x5C) ^ abyPad[i];

    CPL_SHA256Init(&sSHA256Ctxt);
    CPL_SHA256Update(&sSHA256Ctxt, abyPad, CPL_HMAC_SHA256_BLOCKSIZE);
    CPL_SHA256Update(&sSHA256Ctxt, abyDigest, CPL_SHA256_HASH_SIZE);
    CPL_SHA256Final(&sSHA256Ctxt, abyDigest);

    memset(&sSHA256Ctxt, 0, sizeof(sSHA256Ctxt));
    memset(abyPad, 0, CPL_HMAC_SHA256_BLOCKSIZE);
}
