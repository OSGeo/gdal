/**********************************************************************
 *
 * Name:     cpl_list.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  List functions.
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 **********************************************************************
 * Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
 * Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_list.h"

#include <cstddef>

#include "cpl_conv.h"

CPL_CVSID("$Id$")

/*=====================================================================
                    List manipulation functions.
 =====================================================================*/

/************************************************************************/
/*                          CPLListAppend()                             */
/************************************************************************/

/**
 * Append an object list and return a pointer to the modified list.
 * If the input list is NULL, then a new list is created.
 *
 * @param psList pointer to list head.
 * @param pData pointer to inserted data object. May be NULL.
 *
 * @return pointer to the head of modified list.
 */

CPLList *CPLListAppend( CPLList *psList, void *pData )
{
    CPLList *psLast = nullptr;

    // Allocate room for the new object.
    if( psList == nullptr )
    {
      psLast = static_cast<CPLList *>( CPLMalloc( sizeof(CPLList) ) );
      psList = psLast;
    }
    else
    {
        psLast = CPLListGetLast( psList );
        psLast = psLast->psNext = static_cast<CPLList *>(
            CPLMalloc( sizeof(CPLList) ) );
    }

    // Append object to the end of list.
    psLast->pData = pData;
    psLast->psNext = nullptr;

    return psList;
}

/************************************************************************/
/*                          CPLListInsert()                             */
/************************************************************************/

/**
 * Insert an object into list at specified position (zero based).
 * If the input list is NULL, then a new list is created.
 *
 * @param psList pointer to list head.
 * @param pData pointer to inserted data object. May be NULL.
 * @param nPosition position number to insert an object.
 *
 * @return pointer to the head of modified list.
 */

CPLList *CPLListInsert( CPLList *psList, void *pData, int nPosition )
{
    if( nPosition < 0 )
        return psList;  // Nothing to do.

    if( nPosition == 0 )
    {
        CPLList *psNew = static_cast<CPLList *>( CPLMalloc( sizeof(CPLList) ) );
        psNew->pData = pData;
        psNew->psNext = psList;
        psList = psNew;
        return psList;
    }

    const int nCount = CPLListCount( psList );

    if( nCount < nPosition )
    {
        // Allocate room for the new object.
        CPLList* psLast = CPLListGetLast(psList);
        for( int i = nCount; i <= nPosition - 1; i++ )
        {
            psLast = CPLListAppend( psLast, nullptr );
            if( psList == nullptr )
                psList = psLast;
            else
                psLast = psLast->psNext;
        }
        psLast = CPLListAppend( psLast, pData );
        if( psList == nullptr )
            psList = psLast;

        /* coverity[leaked_storage] */
        return psList;
    }

    CPLList *psNew = static_cast<CPLList *>( CPLMalloc( sizeof(CPLList) ) );
    psNew->pData = pData;

    CPLList *psCurrent = psList;
    for( int i = 0; i < nPosition - 1; i++ )
        psCurrent = psCurrent->psNext;
    psNew->psNext = psCurrent->psNext;
    psCurrent->psNext = psNew;

    return psList;
}

/************************************************************************/
/*                          CPLListGetLast()                            */
/************************************************************************/

/**
 * Return the pointer to last element in a list.
 *
 * @param psList pointer to list head.
 *
 * @return pointer to last element in a list.
 */

CPLList *CPLListGetLast( CPLList * const psList )
{
    if( psList == nullptr )
        return nullptr;

    CPLList * psCurrent = psList;
    while( psCurrent->psNext )
        psCurrent = psCurrent->psNext;

    return psCurrent;
}

/************************************************************************/
/*                          CPLListGet()                                */
/************************************************************************/

/**
 * Return the pointer to the specified element in a list.
 *
 * @param psList pointer to list head.
 * @param nPosition the index of the element in the list, 0 being the
 * first element.
 *
 * @return pointer to the specified element in a list.
 */

CPLList *CPLListGet( CPLList *psList, int nPosition )
{
    if( nPosition < 0 )
        return nullptr;

    CPLList *psCurrent = psList;
    int iItem = 0;
    while( iItem < nPosition && psCurrent )
    {
        psCurrent = psCurrent->psNext;
        iItem++;
    }

    return psCurrent;
}

/************************************************************************/
/*                          CPLListCount()                              */
/************************************************************************/

/**
 * Return the number of elements in a list.
 *
 * @param psList pointer to list head.
 *
 * @return number of elements in a list.
 */

int CPLListCount( const CPLList *psList )
{
    int nItems = 0;
    const CPLList *psCurrent = psList;

    while( psCurrent )
    {
        nItems++;
        psCurrent = psCurrent->psNext;
    }

    return nItems;
}

/************************************************************************/
/*                          CPLListRemove()                             */
/************************************************************************/

/**
 * Remove the element from the specified position (zero based) in a list. Data
 * object contained in removed element must be freed by the caller first.
 *
 * @param psList pointer to list head.
 * @param nPosition position number to delete an element.
 *
 * @return pointer to the head of modified list.
 */

CPLList *CPLListRemove( CPLList *psList, int nPosition )
{

    if( psList == nullptr )
        return nullptr;

    if( nPosition < 0 )
        return psList;      /* Nothing to do. */

    if( nPosition == 0 )
    {
        CPLList *psCurrent = psList->psNext;
        CPLFree( psList );
        psList = psCurrent;
        return psList;
    }

    CPLList *psCurrent = psList;
    for( int i = 0; i < nPosition - 1; i++ )
    {
        psCurrent = psCurrent->psNext;
        // psCurrent == NULL if nPosition >= CPLListCount(psList).
        if( psCurrent == nullptr )
            return psList;
    }
    CPLList *psRemoved = psCurrent->psNext;
    // psRemoved == NULL if nPosition >= CPLListCount(psList).
    if( psRemoved == nullptr )
        return psList;
    psCurrent->psNext = psRemoved->psNext;
    CPLFree( psRemoved );

    return psList;
}

/************************************************************************/
/*                          CPLListDestroy()                            */
/************************************************************************/

/**
 * Destroy a list. Caller responsible for freeing data objects contained in
 * list elements.
 *
 * @param psList pointer to list head.
 *
 */

void CPLListDestroy( CPLList *psList )
{
    CPLList *psCurrent = psList;

    while( psCurrent )
    {
        CPLList * const psNext = psCurrent->psNext;
        CPLFree( psCurrent );
        psCurrent = psNext;
    }
}

/************************************************************************/
/*                          CPLListGetNext()                            */
/************************************************************************/

/**
 * Return the pointer to next element in a list.
 *
 * @param psElement pointer to list element.
 *
 * @return pointer to the list element preceded by the given element.
 */

CPLList *CPLListGetNext( const CPLList *psElement )
{
    if( psElement == nullptr )
        return nullptr;

    return psElement->psNext;
}

/************************************************************************/
/*                          CPLListGetData()                            */
/************************************************************************/

/**
 * Return pointer to the data object contained in given list element.
 *
 * @param psElement pointer to list element.
 *
 * @return pointer to the data object contained in given list element.
 */

void *CPLListGetData( const CPLList *psElement )
{
    if( psElement == nullptr )
        return nullptr;

    return psElement->pData;
}
