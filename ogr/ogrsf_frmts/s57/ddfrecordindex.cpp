/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements DDFRecordIndex class.  This class is used to cache
 *           ISO8211 records for spatial objects so they can be efficiently
 *           assembled later as features.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/11/03 22:12:43  warmerda
 * New
 *
 */

#include "s57.h"
#include "cpl_conv.h"

/************************************************************************/
/*                           DDFRecordIndex()                           */
/************************************************************************/

DDFRecordIndex::DDFRecordIndex()

{
    bSorted = FALSE;

    nRecordCount = 0;
    nRecordMax = 0;
    panRecordKey = NULL;
    papoRecordList = NULL;
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
    for( int i = 0; i < nRecordCount; i++ )
        delete papoRecordList[i];

    CPLFree( panRecordKey );
    panRecordKey = NULL;
    
    CPLFree( papoRecordList );
    papoRecordList = NULL;

    nRecordCount = 0;
    nRecordMax = 0;

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
        panRecordKey = (int *)
            CPLRealloc( panRecordKey, sizeof(int)*nRecordMax );
        papoRecordList = (DDFRecord **)
            CPLRealloc( papoRecordList, sizeof(DDFRecord*)*nRecordMax );
    }

    bSorted = FALSE;

    panRecordKey[nRecordCount] = nKey;
    papoRecordList[nRecordCount] = poRecord;

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
    int		nMinIndex = 0, nMaxIndex = nRecordCount-1;

    while( nMinIndex <= nMaxIndex )
    {
        int	nTestIndex = (nMaxIndex + nMinIndex) / 2;

        if( panRecordKey[nTestIndex] < nKey )
            nMinIndex = nTestIndex + 1;
        else if( panRecordKey[nTestIndex] > nKey )
            nMaxIndex = nTestIndex - 1;
        else
            return papoRecordList[nTestIndex];
    }

    return NULL;
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

    int		i=0, j, bModified;

    do
    {
        bModified = FALSE;
        for( j = 0; j < nRecordCount - i - 1; j++ )
        {
            if( panRecordKey[j] > panRecordKey[j+1] )
            {
                int	nTemp;
                DDFRecord * poRecord;

                nTemp = panRecordKey[j];
                panRecordKey[j] = panRecordKey[j+1];
                panRecordKey[j+1] = nTemp;

                poRecord = papoRecordList[j];
                papoRecordList[j] = papoRecordList[j+1];
                papoRecordList[j+1] = poRecord;

                bModified = TRUE;
            }
        }
        i++;
    } while( bModified );

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
        return papoRecordList[nIndex];
}

