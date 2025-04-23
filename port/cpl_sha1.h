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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_SHA1_INCLUDED_H
#define CPL_SHA1_INCLUDED_H

#ifndef DOXYGEN_SKIP

#include "cpl_port.h"

#define CPL_SHA1_HASH_SIZE 20  // SHA1 outputs a 20 byte digest

CPL_C_START

void CPL_SHA1(const void *data, size_t len, GByte hash[CPL_SHA1_HASH_SIZE]);

/* Not CPL_DLL exported */
void CPL_HMAC_SHA1(const void *pKey, size_t nKeyLen, const void *pabyMessage,
                   size_t nMessageLen, GByte abyDigest[CPL_SHA1_HASH_SIZE]);

CPL_C_END

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_SHA1_INCLUDED_H */
