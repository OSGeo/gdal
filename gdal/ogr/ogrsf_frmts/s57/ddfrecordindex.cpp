/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements DDFRecordIndex class.  This class is used to cache
 *           ISO8211 records for spatial objects so they can be efficiently
 *           assembled later as features.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001, Frank Warmerdam
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

#include "s57.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           DDFRecordIndex()                           */
/************************************************************************/

DDFRecordIndex::DDFRecordIndex()

{
    bSorted = FALSE;

    nRecordCount = 0;
    nRecordMax = 0;
    pasRecords = NULL;

    nLastObjlPos = 0;             /* rjensen. added for FindRecordByObjl() */
    nLastObjl    = 0;             /* rjensen. added for FindRecordByObjl() */
}

/************************************************************************/
/*                          ~DDFRecordIndex()                           */
/************************************************************************/

DDFRecordIndex::~DDFRecordIndex()

{
    Clear();
}

/************************************************************************/
/*                               Clear()                                */
/*                                                                      */
/*      Clear all entries from the index and deallocate all index       */
/*      resources.                                                      */
/************************************************************************/

void DDFRecordIndex::Clear()

{
    // It turns out that deleting these records here is very expensive
    // due to the linear search in DDFModule::RemoveClone().  For now we
    // just leave the clones depending on DDFModule::~DDFModule() to clean
    // them up eventually.

    //for( int i = 0; i < nRecordCount; i++ )
    //  delete pasRecords[i].poRecord;

    CPLFree( pasRecords );
    pasRecords = NULL;

    nRecordCount = 0;
    nRecordMax = 0;

    nLastObjlPos = 0;             /* rjensen. added for FindRecordByObjl() */
    nLastObjl      = 0;             /* rjensen. added for FindRecordByObjl() */

    bSorted = FALSE;
}

/************************************************************************/
/*                             AddRecord()                              */
/*                                                                      */
/*      Add a record to the index.  The index will assume ownership     */
/*      of the record.  If passing a record just read from a            */
/*      DDFModule it is imperitive that the caller Clone()'s the        */
/*      record first.                                                   */
/************************************************************************/

void DDFRecordIndex::AddRecord( int nKey, DDFRecord * poRecord )

{
    if( nRecordCount == nRecordMax )
    {
        nRecordMax = (int) (nRecordCount * 1.3 + 100);
        pasRecords = (DDFIndexedRecord *)
            CPLRealloc( pasRecords, sizeof(DDFIndexedRecord)*nRecordMax );
    }

    bSorted = FALSE;

    pasRecords[nRecordCount].nKey = nKey;
    pasRecords[nRecordCount].poRecord = poRecord;
    pasRecords[nRecordCount].pClientData = NULL;

    nRecordCount++;
}

/************************************************************************/
/*                             FindRecord()                             */
/*                                                                      */
/*      Though the returned pointer is not const, it should be          */
/*      considered internal to the index and not modified or freed      */
/*      by application code.                                            */
/************************************************************************/

DDFRecord * DDFRecordIndex::FindRecord( int nKey )

{
    if( !bSorted )
        Sort();

/* -------------------------------------------------------------------- */
/*      Do a binary search based on the key to find the desired record. */
/* -------------------------------------------------------------------- */
    int         nMinIndex = 0, nMaxIndex = nRecordCount-1;

    while( nMinIndex <= nMaxIndex )
    {
        int     nTestIndex = (nMaxIndex + nMinIndex) / 2;

        if( pasRecords[nTestIndex].nKey < nKey )
            nMinIndex = nTestIndex + 1;
        else if( pasRecords[nTestIndex].nKey > nKey )
            nMaxIndex = nTestIndex - 1;
        else
            return pasRecords[nTestIndex].poRecord;
    }

    return NULL;
}

/************************************************************************/
/*                       FindRecordByObjl()                             */
/*      Rodney Jensen                                                   */
/*      Though the returned pointer is not const, it should be          */
/*      considered internal to the index and not modified or freed      */
/*      by application code.                                            */
/************************************************************************/

DDFRecord * DDFRecordIndex::FindRecordByObjl( int nObjl )
{
    if( !bSorted )
        Sort();

/* -------------------------------------------------------------------- */
/*      Do a linear search based on the nObjl to find the desired record. */
/* -------------------------------------------------------------------- */
    int nMinIndex = 0;
    if (nLastObjl != nObjl) nLastObjlPos=0;

    for (nMinIndex = nLastObjlPos; nMinIndex < nRecordCount; nMinIndex++)
    {
        if (nObjl == pasRecords[nMinIndex].poRecord->GetIntSubfield( "FRID", 0, "OBJL", 0 ) )
        {
            nLastObjlPos=nMinIndex+1;  /* add 1, don't want to look at same again */
            nLastObjl=nObjl;
            return pasRecords[nMinIndex].poRecord;
        }
    }

    nLastObjlPos=0;
    nLastObjl=0;

    return NULL;
}

/************************************************************************/
/*                            RemoveRecord()                            */
/************************************************************************/

int DDFRecordIndex::RemoveRecord( int nKey )

{
    if( !bSorted )
        Sort();

/* -------------------------------------------------------------------- */
/*      Do a binary search based on the key to find the desired record. */
/* -------------------------------------------------------------------- */
    int         nMinIndex = 0, nMaxIndex = nRecordCount-1;
    int         nTestIndex = 0;

    while( nMinIndex <= nMaxIndex )
    {
        nTestIndex = (nMaxIndex + nMinIndex) / 2;

        if( pasRecords[nTestIndex].nKey < nKey )
            nMinIndex = nTestIndex + 1;
        else if( pasRecords[nTestIndex].nKey > nKey )
            nMaxIndex = nTestIndex - 1;
        else
            break;
    }

    if( nMinIndex > nMaxIndex )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Delete this record.                                             */
/* -------------------------------------------------------------------- */
    delete pasRecords[nTestIndex].poRecord;

/* -------------------------------------------------------------------- */
/*      Move all the list entries back one to fill the hole, and        */
/*      update the total count.                                         */
/* -------------------------------------------------------------------- */
    memmove( pasRecords + nTestIndex,
             pasRecords + nTestIndex + 1,
             (nRecordCount - nTestIndex - 1) * sizeof(DDFIndexedRecord) );

    nRecordCount--;

    return TRUE;
}

/************************************************************************/
/*                             DDFCompare()                             */
/*                                                                      */
/*      Compare two DDFIndexedRecord objects for qsort().               */
/************************************************************************/

static int DDFCompare( const void *pRec1, const void *pRec2 )

{
    if( ((const DDFIndexedRecord *) pRec1)->nKey
        == ((const DDFIndexedRecord *) pRec2)->nKey )
        return 0;
    else if( ((const DDFIndexedRecord *) pRec1)->nKey
             < ((const DDFIndexedRecord *) pRec2)->nKey )
        return -1;
    else
        return 1;
}

/************************************************************************/
/*                                Sort()                                */
/*                                                                      */
/*      Sort the records based on the key.  This is currently           */
/*      implemented as a bubble sort, and could gain in efficiency      */
/*      by reimplementing as a quick sort; however, I believe that      */
/*      the keys will always be in order so a bubble sort should        */
/*      only require one pass to verify this.                           */
/************************************************************************/

void DDFRecordIndex::Sort()

{
    if( bSorted )
        return;

    qsort( pasRecords, nRecordCount, sizeof(DDFIndexedRecord), DDFCompare );

    bSorted = TRUE;
}

/************************************************************************/
/*                             GetByIndex()                             */
/************************************************************************/

DDFRecord * DDFRecordIndex::GetByIndex( int nIndex )

{
    if( !bSorted )
        Sort();

    if( nIndex < 0 || nIndex >= nRecordCount )
        return NULL;
    else
        return pasRecords[nIndex].poRecord;
}

/************************************************************************/
/*                        GetClientInfoByIndex()                        */
/************************************************************************/

void * DDFRecordIndex::GetClientInfoByIndex( int nIndex )

{
    if( !bSorted )
        Sort();

    if( nIndex < 0 || nIndex >= nRecordCount )
        return NULL;
    else
        return pasRecords[nIndex].pClientData;
}

/************************************************************************/
/*                        SetClientInfoByIndex()                        */
/************************************************************************/

void DDFRecordIndex::SetClientInfoByIndex( int nIndex, void *pClientData )

{
    if( !bSorted )
        Sort();

    if( nIndex < 0 || nIndex >= nRecordCount )
        return;
    else
        pasRecords[nIndex].pClientData = pClientData;
}

