/**********************************************************************
 * $Id$
 *
 * Name:     cpl_hash_set.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Hash set functions.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_HASH_SET_H_INCLUDED
#define CPL_HASH_SET_H_INCLUDED

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

/** Opaque type for a hash set */
typedef struct _CPLHashSet CPLHashSet;

/** CPLHashSetHashFunc */
typedef unsigned long (*CPLHashSetHashFunc)(const void *elt);

/** CPLHashSetEqualFunc */
typedef int (*CPLHashSetEqualFunc)(const void *elt1, const void *elt2);

/** CPLHashSetFreeEltFunc */
typedef void (*CPLHashSetFreeEltFunc)(void *elt);

/** CPLHashSetIterEltFunc */
typedef int (*CPLHashSetIterEltFunc)(void *elt, void *user_data);

/* Functions */

CPLHashSet CPL_DLL *CPLHashSetNew(CPLHashSetHashFunc fnHashFunc,
                                  CPLHashSetEqualFunc fnEqualFunc,
                                  CPLHashSetFreeEltFunc fnFreeEltFunc);

void CPL_DLL CPLHashSetDestroy(CPLHashSet *set);

void CPL_DLL CPLHashSetClear(CPLHashSet *set);

int CPL_DLL CPLHashSetSize(const CPLHashSet *set);

void CPL_DLL CPLHashSetForeach(CPLHashSet *set,
                               CPLHashSetIterEltFunc fnIterFunc,
                               void *user_data);

int CPL_DLL CPLHashSetInsert(CPLHashSet *set, void *elt);

void CPL_DLL *CPLHashSetLookup(CPLHashSet *set, const void *elt);

int CPL_DLL CPLHashSetRemove(CPLHashSet *set, const void *elt);
int CPL_DLL CPLHashSetRemoveDeferRehash(CPLHashSet *set, const void *elt);

unsigned long CPL_DLL CPLHashSetHashPointer(const void *elt);

int CPL_DLL CPLHashSetEqualPointer(const void *elt1, const void *elt2);

unsigned long CPL_DLL CPLHashSetHashStr(const void *pszStr);

int CPL_DLL CPLHashSetEqualStr(const void *pszStr1, const void *pszStr2);

CPL_C_END

#endif /* CPL_HASH_SET_H_INCLUDED */
