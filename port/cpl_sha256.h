/* $Id$ */

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

#ifndef CPL_SHA256_INCLUDED_H
#define CPL_SHA256_INCLUDED_H

#ifndef DOXYGEN_SKIP

#include "cpl_port.h"

#define CPL_SHA256_HASH_SIZE 32     /* 256 bit */
#define CPL_SHA256_HASH_WORDS 8

#ifndef GUInt64
#define GUInt64 GUIntBig
#endif

CPL_C_START

struct _CPL_SHA256Context {
        GUInt64 totalLength;
        GUInt32 hash[CPL_SHA256_HASH_WORDS];
        GUInt32 bufferLength;
        union {
                GUInt32 words[16];
                GByte bytes[64];
        } buffer;
};
typedef struct _CPL_SHA256Context CPL_SHA256Context;

void CPL_DLL CPL_SHA256Init(CPL_SHA256Context * sc);

void CPL_DLL CPL_SHA256Update(CPL_SHA256Context * sc, const void *data, size_t len);

void CPL_DLL CPL_SHA256Final(CPL_SHA256Context * sc, GByte hash[CPL_SHA256_HASH_SIZE]);

void CPL_DLL CPL_SHA256(const void *data, size_t len, GByte hash[CPL_SHA256_HASH_SIZE]);

void CPL_DLL CPL_HMAC_SHA256(const void *pKey, size_t nKeyLen,
                             const void *pabyMessage, size_t nMessageLen,
                             GByte abyDigest[CPL_SHA256_HASH_SIZE]);

// Not exported for now
GByte* CPL_RSA_SHA256_Sign(const char* pszPrivateKey,
                                  const void* pabyData,
                                  unsigned int nDataLen,
                                  unsigned int* pnSignatureLen);

CPL_C_END

#endif /* #ifndef DOXYGEN_SKIP */

#endif  /* CPL_SHA256_INCLUDED_H */
