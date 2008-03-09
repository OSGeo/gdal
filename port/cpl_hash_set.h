/**********************************************************************
 * $Id: cpl_hash_set.h $
 *
 * Name:     cpl_hash_set.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Hash set functions.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2008, Even Rouault, <even dot rouault at mines dash paris dot org>
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
 ****************************************************************************/

#ifndef _CPL_HASH_SET_H_INCLUDED
#define _CPL_HASH_SET_H_INCLUDED

#include "cpl_port.h"

/**
 * \file cpl_hash_set.h
 *
 * Hash set implementation.
 *
 */

CPL_C_START

typedef struct _CPLHashSet CPLHashSet;

typedef unsigned int (*CPLHashSetHashFunc)(const void* elt);

typedef int (*CPLHashSetEqualFunc)(const void* elt1, const void* elt2);

typedef void (*CPLHashSetFreeEltFunc)(void* elt);

CPLHashSet* CPLHashSetNew(CPLHashSetHashFunc fnHashFunc, CPLHashSetEqualFunc fnEqualFunc,
                          CPLHashSetFreeEltFunc fnFreeEltFunc);

int CPLHashSetSize(const CPLHashSet* set);

void CPLHashSetDestroy(CPLHashSet* set);

int CPLHashSetInsert(CPLHashSet* set, void* elt);

int CPLHashSetFind(CPLHashSet* set, const void* elt);

int CPLHashSetRemove(CPLHashSet* set, const void* elt);

unsigned int CPLHashSetHashPointer(const void* elt);

int CPLHashSetEqualPointer(const void* elt1, const void* elt2);

unsigned int CPLHashSetHashStr(const void * pszStr);

int CPLHashSetEqualStr(const void* pszStr1, const void* pszStr2);

CPL_C_END

#endif /* _CPL_HASH_SET_H_INCLUDED */

