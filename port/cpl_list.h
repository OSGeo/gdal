/**********************************************************************
 * $Id$
 *
 * Name:     cpl_list.h
 * Project:  CPL - Common Portability Library
 * Purpose:  List functions.
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 **********************************************************************
 * Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_LIST_H_INCLUDED
#define CPL_LIST_H_INCLUDED

#include "cpl_port.h"

/**
 * \file cpl_list.h
 *
 * Simplest list implementation.  List contains only pointers to stored
 * objects, not objects itself. All operations regarding allocation and
 * freeing memory for objects should be performed by the caller.
 *
 */

CPL_C_START

/** List element structure. */
typedef struct _CPLList CPLList;

/** List element structure. */
struct _CPLList
{
    /*! Pointer to the data object. Should be allocated and freed by the
     * caller.
     * */
    void *pData;
    /*! Pointer to the next element in list. NULL, if current element is the
     * last one.
     */
    struct _CPLList *psNext;
};

CPLList CPL_DLL *CPLListAppend(CPLList *psList, void *pData);
CPLList CPL_DLL *CPLListInsert(CPLList *psList, void *pData, int nPosition);
CPLList CPL_DLL *CPLListGetLast(CPLList *psList);
CPLList CPL_DLL *CPLListGet(CPLList *const psList, int nPosition);
int CPL_DLL CPLListCount(const CPLList *psList);
CPLList CPL_DLL *CPLListRemove(CPLList *psList, int nPosition);
void CPL_DLL CPLListDestroy(CPLList *psList);
CPLList CPL_DLL *CPLListGetNext(const CPLList *psElement);
void CPL_DLL *CPLListGetData(const CPLList *psElement);

CPL_C_END

#endif /* CPL_LIST_H_INCLUDED */
