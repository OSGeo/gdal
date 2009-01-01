/**********************************************************************
 * $Id$
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
 * An hash set is a data structure that holds elements that are unique
 * according to a comparison function. Operations on the hash set, such as
 * insertion, removal or lookup, are supposed to be fast if an efficient
 * "hash" function is provided.
 */

CPL_C_START

/* Types */

typedef struct _CPLHashSet CPLHashSet;

typedef unsigned long (*CPLHashSetHashFunc)(const void* elt);

typedef int          (*CPLHashSetEqualFunc)(const void* elt1, const void* elt2);

typedef void         (*CPLHashSetFreeEltFunc)(void* elt);

typedef int          (*CPLHashSetIterEltFunc)(void* elt, void* user_data);

/* Functions */

CPLHashSet CPL_DLL * CPLHashSetNew(CPLHashSetHashFunc fnHashFunc,
                                   CPLHashSetEqualFunc fnEqualFunc,
                                   CPLHashSetFreeEltFunc fnFreeEltFunc);

void         CPL_DLL CPLHashSetDestroy(CPLHashSet* set);

int          CPL_DLL CPLHashSetSize(const CPLHashSet* set);

void         CPL_DLL CPLHashSetForeach(CPLHashSet* set,
                                       CPLHashSetIterEltFunc fnIterFunc,
                                       void* user_data);

int          CPL_DLL CPLHashSetInsert(CPLHashSet* set, void* elt);

void         CPL_DLL * CPLHashSetLookup(CPLHashSet* set, const void* elt);

int          CPL_DLL CPLHashSetRemove(CPLHashSet* set, const void* elt);

unsigned long CPL_DLL CPLHashSetHashPointer(const void* elt);

int          CPL_DLL CPLHashSetEqualPointer(const void* elt1, const void* elt2);

unsigned long CPL_DLL CPLHashSetHashStr(const void * pszStr);

int          CPL_DLL CPLHashSetEqualStr(const void* pszStr1, const void* pszStr2);

CPL_C_END

#endif /* _CPL_HASH_SET_H_INCLUDED */

