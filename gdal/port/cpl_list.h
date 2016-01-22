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

#ifndef _CPL_LIST_H_INCLUDED
#define _CPL_LIST_H_INCLUDED

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
typedef struct _CPLList
{
    /*! Pointer to the data object. Should be allocated and freed by the
     * caller.
     * */
    void        *pData;
    /*! Pointer to the next element in list. NULL, if current element is the
     * last one.
     */
    struct _CPLList    *psNext;
} CPLList;

CPLList CPL_DLL *CPLListAppend( CPLList *psList, void *pData );
CPLList CPL_DLL *CPLListInsert( CPLList *psList, void *pData, int nPosition );
CPLList CPL_DLL *CPLListGetLast( CPLList *psList );
CPLList CPL_DLL *CPLListGet( CPLList *psList, int nPosition );
int CPL_DLL CPLListCount( CPLList *psList );
CPLList CPL_DLL *CPLListRemove( CPLList *psList, int nPosition );
void CPL_DLL CPLListDestroy( CPLList *psList );
CPLList CPL_DLL *CPLListGetNext( CPLList *psElement );
void CPL_DLL *CPLListGetData( CPLList *psElement );

CPL_C_END

#endif /* _CPL_LIST_H_INCLUDED */
