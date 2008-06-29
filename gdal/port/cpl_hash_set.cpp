/**********************************************************************
 * $Id: cpl_hash_set.cpp $
 *
 * Name:     cpl_hash_set.cpp
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

#include "cpl_conv.h"
#include "cpl_hash_set.h"
#include "cpl_list.h"

struct _CPLHashSet
{
    CPLHashSetHashFunc    fnHashFunc;
    CPLHashSetEqualFunc   fnEqualFunc;
    CPLHashSetFreeEltFunc fnFreeEltFunc;
    CPLList**             tabList;
    int                   nSize;
    int                   nIndiceAllocatedSize;
    int                   nAllocatedSize;
#ifdef HASH_DEBUG
    int                   nCollisions;
#endif
};

static const int anPrimes[] = 
{ 53, 97, 193, 389, 769, 1543, 3079, 6151,
  12289, 24593, 49157, 98317, 196613, 393241,
  786433, 1572869, 3145739, 6291469, 12582917,
  25165843, 50331653, 100663319, 201326611,
  402653189, 805306457, 1610612741 };

/************************************************************************/
/*                          CPLHashSetNew()                             */
/************************************************************************/

/**
 * Creates a new hash set
 * 
 * The hash function must return a hash value for the elements to insert.
 * If fnHashFunc is NULL, CPLHashSetHashPointer will be used.
 *
 * The equal function must return if two elements are equal.
 * If fnEqualFunc is NULL, CPLHashSetEqualPointer will be used.
 *
 * The free function is used to free elements inserted in the hash set,
 * when the hash set is destroyed, when elements are removed or replaced.
 * If fnFreeEltFunc is NULL, elements inserted into the hash set will not be freed.
 *
 * @param fnHashFunc hash function. May be NULL.
 * @param fnEqualFunc equal function. May be NULL.
 * @param fnFreeEltFunc element free function. May be NULL.
 *
 * @return a new hash set
 */

CPLHashSet* CPLHashSetNew(CPLHashSetHashFunc fnHashFunc,
                          CPLHashSetEqualFunc fnEqualFunc,
                          CPLHashSetFreeEltFunc fnFreeEltFunc)
{
    CPLHashSet* set = (CPLHashSet*) CPLMalloc(sizeof(CPLHashSet));
    set->fnHashFunc = (fnHashFunc) ? fnHashFunc : CPLHashSetHashPointer;
    set->fnEqualFunc = (fnEqualFunc) ? fnEqualFunc : CPLHashSetEqualPointer;
    set->fnFreeEltFunc = fnFreeEltFunc;
    set->nSize = 0;
    set->tabList = (CPLList**) CPLCalloc(sizeof(CPLList*), 53);
    set->nIndiceAllocatedSize = 0;
    set->nAllocatedSize = 53;
#ifdef HASH_DEBUG
    set->nCollisions = 0;
#endif
    return set;
}


/************************************************************************/
/*                          CPLHashSetSize()                            */
/************************************************************************/

/**
 * Returns the number of elements inserted in the hash set
 * 
 * Note: this is not the internal size of the hash set
 *
 * @param set the hash set
 *
 * @return the number of elements in the hash set
 */

int CPLHashSetSize(const CPLHashSet* set)
{
    CPLAssert(set != NULL);
    return set->nSize;
}

/************************************************************************/
/*                        CPLHashSetDestroy()                           */
/************************************************************************/

/**
 * Destroys an allocated hash set.
 *
 * This function also frees the elements if a free function was
 * provided at the creation of the hash set.
 * 
 * @param set the hash set
 */

void CPLHashSetDestroy(CPLHashSet* set)
{
    CPLAssert(set != NULL);
    for(int i=0;i<set->nAllocatedSize;i++)
    {
        if (set->fnFreeEltFunc)
        {
            CPLList* cur = set->tabList[i];
            while(cur)
            {
                set->fnFreeEltFunc(cur->pData);
                cur = cur->psNext;
            }
        }
        CPLListDestroy(set->tabList[i]);
    }
    CPLFree(set->tabList);
    CPLFree(set);
}

/************************************************************************/
/*                       CPLHashSetForeach()                            */
/************************************************************************/


/**
 * Walk through the hash set and runs the provided function on all the
 * elements
 *
 * This function is provided the user_data argument of CPLHashSetForeach.
 * It must return TRUE to go on the walk through the hash set, or FALSE to
 * make it stop.
 *
 * Note : the structure of the hash set must *NOT* be modified during the
 * walk.
 * 
 * @param set the hash set.
 * @param fnIterFunc the function called on each element.
 * @param user_data the user data provided to the function.
 */

void  CPLHashSetForeach(CPLHashSet* set,
                        CPLHashSetIterEltFunc fnIterFunc,
                        void* user_data)
{
    CPLAssert(set != NULL);
    if (!fnIterFunc) return;

    for(int i=0;i<set->nAllocatedSize;i++)
    {
        CPLList* cur = set->tabList[i];
        while(cur)
        {
            if (fnIterFunc(cur->pData, user_data) == FALSE)
                return;

            cur = cur->psNext;
        }
    }
}

/************************************************************************/
/*                        CPLHashSetRehash()                            */
/************************************************************************/

static void CPLHashSetRehash(CPLHashSet* set)
{
    int nNewAllocatedSize = anPrimes[set->nIndiceAllocatedSize];
    CPLList** newTabList = (CPLList**) CPLCalloc(sizeof(CPLList*), nNewAllocatedSize);
#ifdef HASH_DEBUG
    CPLDebug("CPLHASH", "hashSet=%p, nSize=%d, nCollisions=%d, fCollisionRate=%.02f",
             set, set->nSize, set->nCollisions, set->nCollisions * 100.0 / set->nSize);
    set->nCollisions = 0;
#endif
    for(int i=0;i<set->nAllocatedSize;i++)
    {
        CPLList* cur = set->tabList[i];
        while(cur)
        {
            unsigned long nNewHashVal = set->fnHashFunc(cur->pData) % nNewAllocatedSize;
#ifdef HASH_DEBUG
            if (newTabList[nNewHashVal])
                set->nCollisions ++;
#endif
            newTabList[nNewHashVal] = CPLListInsert(newTabList[nNewHashVal], cur->pData, 0);
            cur = cur->psNext;
        }
        CPLListDestroy(set->tabList[i]);
    }
    CPLFree(set->tabList);
    set->tabList = newTabList;
    set->nAllocatedSize = nNewAllocatedSize;
}


/************************************************************************/
/*                        CPLHashSetFindPtr()                           */
/************************************************************************/

static void** CPLHashSetFindPtr(CPLHashSet* set, const void* elt)
{
    unsigned long nHashVal = set->fnHashFunc(elt) % set->nAllocatedSize;
    CPLList* cur = set->tabList[nHashVal];
    while(cur)
    {
        if (set->fnEqualFunc(cur->pData, elt))
            return &cur->pData;
        cur = cur->psNext;
    }
    return NULL;
}

/************************************************************************/
/*                         CPLHashSetInsert()                           */
/************************************************************************/

/**
 * Inserts an element into a hash set.
 *
 * If the element was already inserted in the hash set, the previous
 * element is replaced by the new element. If a free function was provided,
 * it is used to free the previously inserted element
 * 
 * @param set the hash set
 * @param elt the new element to insert in the hash set
 *
 * @return TRUE if the element was not already in the hash set
 */

int CPLHashSetInsert(CPLHashSet* set, void* elt)
{
    CPLAssert(set != NULL);
    void** pElt = CPLHashSetFindPtr(set, elt);
    if (pElt)
    {
        if (set->fnFreeEltFunc)
            set->fnFreeEltFunc(*pElt);

        *pElt = elt;
        return FALSE;
    }

    if (set->nSize >= 2 * set->nAllocatedSize / 3)
    {
        set->nIndiceAllocatedSize++;
        CPLHashSetRehash(set);
    }

    unsigned long nHashVal = set->fnHashFunc(elt) % set->nAllocatedSize;
#ifdef HASH_DEBUG
    if (set->tabList[nHashVal])
        set->nCollisions ++;
#endif
    set->tabList[nHashVal] = CPLListInsert(set->tabList[nHashVal], (void*) elt, 0);
    set->nSize++;

    return TRUE;
}

/************************************************************************/
/*                        CPLHashSetLookup()                            */
/************************************************************************/

/**
 * Returns the element found in the hash set corresponding to the element to look up
 * The element must not be modified.
 * 
 * @param set the hash set
 * @param elt the element to look up in the hash set
 *
 * @return the element found in the hash set or NULL
 */

void* CPLHashSetLookup(CPLHashSet* set, const void* elt)
{
    CPLAssert(set != NULL);
    void** pElt = CPLHashSetFindPtr(set, elt);
    if (pElt)
        return *pElt;
    else
        return NULL;
}

/************************************************************************/
/*                         CPLHashSetRemove()                           */
/************************************************************************/

/**
 * Removes an element from a hash set
 * 
 * @param set the hash set
 * @param elt the new element to remove from the hash set
 *
 * @return TRUE if the element was in the hash set
 */

int CPLHashSetRemove(CPLHashSet* set, const void* elt)
{
    CPLAssert(set != NULL);
    if (set->nIndiceAllocatedSize > 0 && set->nSize <= set->nAllocatedSize / 2)
    {
        set->nIndiceAllocatedSize--;
        CPLHashSetRehash(set);
    }

    int nHashVal = set->fnHashFunc(elt) % set->nAllocatedSize;
    CPLList* cur = set->tabList[nHashVal];
    CPLList* prev = NULL;
    while(cur)
    {
        if (set->fnEqualFunc(cur->pData, elt))
        {
            if (prev)
                prev->psNext = cur->psNext;
            else
                set->tabList[nHashVal] = cur->psNext;

            if (set->fnFreeEltFunc)
                set->fnFreeEltFunc(cur->pData);

            CPLFree(cur);
#ifdef HASH_DEBUG
            if (set->tabList[nHashVal])
                set->nCollisions --;
#endif

            set->nSize--;
            return TRUE;
        }
        prev = cur;
        cur = cur->psNext;
    }
    return FALSE;
}


/************************************************************************/
/*                    CPLHashSetHashPointer()                           */
/************************************************************************/

/**
 * Hash function for an arbitrary pointer
 * 
 * @param elt the arbitrary pointer to hash
 *
 * @return the hash value of the pointer
 */

unsigned long CPLHashSetHashPointer(const void* elt)
{
    return (unsigned long)elt;
}

/************************************************************************/
/*                   CPLHashSetEqualPointer()                           */
/************************************************************************/

/**
 * Equality function for arbitrary pointers
 * 
 * @param elt1 the first arbitrary pointer to compare
 * @param elt2 the second arbitrary pointer to compare
 *
 * @return TRUE if the pointers are equal
 */

int CPLHashSetEqualPointer(const void* elt1, const void* elt2)
{
    return elt1 == elt2;
}

/************************************************************************/
/*                        CPLHashSetHashStr()                           */
/************************************************************************/

/**
 * Hash function for a zero-terminated string
 * 
 * @param elt the string to hash. May be NULL.
 *
 * @return the hash value of the string
 */

unsigned long CPLHashSetHashStr(const void *elt)
{
    unsigned char* pszStr = (unsigned char*)elt;
    unsigned long hash = 0;
    int c;

    if (pszStr == NULL)
        return 0;

    while ((c = *pszStr++) != '\0')
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

/************************************************************************/
/*                     CPLHashSetEqualStr()                             */
/************************************************************************/

/**
 * Equality function for strings
 * 
 * @param elt1 the first string to compare. May be NULL.
 * @param elt2 the second string to compare. May be NULL.
 *
 * @return TRUE if the strings are equal
 */

int CPLHashSetEqualStr(const void* elt1, const void* elt2)
{
    const char* pszStr1 = (const char*)elt1;
    const char* pszStr2 = (const char*)elt2;
    if (pszStr1 == NULL && pszStr2 != NULL)
        return FALSE;
    else if (pszStr1 != NULL && pszStr2 == NULL)
        return FALSE;
    else if (pszStr1 == NULL && pszStr2 == NULL)
        return TRUE;
    else
        return strcmp(pszStr1, pszStr2) == 0;
}
