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

#ifndef CPL_SHA1_INCLUDED_H
#define CPL_SHA1_INCLUDED_H

#ifndef DOXYGEN_SKIP

#include "cpl_port.h"

#define CPL_SHA1_HASH_SIZE 20 // SHA1 outputs a 20 byte digest

CPL_C_START

/* Not CPL_DLL exported */
void CPL_HMAC_SHA1(const void *pKey, size_t nKeyLen,
                   const void *pabyMessage, size_t nMessageLen,
                   GByte abyDigest[CPL_SHA1_HASH_SIZE]);

CPL_C_END

#endif /* #ifndef DOXYGEN_SKIP */

#endif  /* CPL_SHA1_INCLUDED_H */
