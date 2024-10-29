/**********************************************************************
 * $Id$
 *
 * Name:     tif_hash_set.h
 * Project:  TIFF - Common Portability Library
 * Purpose:  Hash set functions.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef TIFF_HASH_SET_H_INCLUDED
#define TIFF_HASH_SET_H_INCLUDED

#include <stdbool.h>

/**
 * \file tif_hash_set.h
 *
 * Hash set implementation.
 *
 * An hash set is a data structure that holds elements that are unique
 * according to a comparison function. Operations on the hash set, such as
 * insertion, removal or lookup, are supposed to be fast if an efficient
 * "hash" function is provided.
 */

#ifdef __cplusplus
extern "C"
{
#endif

    /* Types */

    /** Opaque type for a hash set */
    typedef struct _TIFFHashSet TIFFHashSet;

    /** TIFFHashSetHashFunc */
    typedef unsigned long (*TIFFHashSetHashFunc)(const void *elt);

    /** TIFFHashSetEqualFunc */
    typedef bool (*TIFFHashSetEqualFunc)(const void *elt1, const void *elt2);

    /** TIFFHashSetFreeEltFunc */
    typedef void (*TIFFHashSetFreeEltFunc)(void *elt);

    /* Functions */

    TIFFHashSet *TIFFHashSetNew(TIFFHashSetHashFunc fnHashFunc,
                                TIFFHashSetEqualFunc fnEqualFunc,
                                TIFFHashSetFreeEltFunc fnFreeEltFunc);

    void TIFFHashSetDestroy(TIFFHashSet *set);

    int TIFFHashSetSize(const TIFFHashSet *set);

#ifdef notused
    void TIFFHashSetClear(TIFFHashSet *set);

    /** TIFFHashSetIterEltFunc */
    typedef int (*TIFFHashSetIterEltFunc)(void *elt, void *user_data);

    void TIFFHashSetForeach(TIFFHashSet *set, TIFFHashSetIterEltFunc fnIterFunc,
                            void *user_data);
#endif

    bool TIFFHashSetInsert(TIFFHashSet *set, void *elt);

    void *TIFFHashSetLookup(TIFFHashSet *set, const void *elt);

    bool TIFFHashSetRemove(TIFFHashSet *set, const void *elt);

#ifdef notused
    bool TIFFHashSetRemoveDeferRehash(TIFFHashSet *set, const void *elt);
#endif

#ifdef __cplusplus
}
#endif

#endif /* TIFF_HASH_SET_H_INCLUDED */
