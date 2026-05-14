/******************************************************************************
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements DDFRecordIndex class.  This class is used to cache
 *           ISO8211 records for spatial objects so they can be efficiently
 *           assembled later as features.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "ddfrecordindex.h"

#include <algorithm>

/************************************************************************/
/*                           DDFRecordIndex()                           */
/************************************************************************/

DDFRecordIndex::DDFRecordIndex() = default;

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
    bSorted = false;
    asRecords.clear();
    oMapKeyToRecord.clear();
}

/************************************************************************/
/*                             AddRecord()                              */
/*                                                                      */
/*      Add a record to the index.  The index will assume ownership     */
/*      of the record.  If passing a record just read from a            */
/*      DDFModule it is imperative that the caller Clone()'s the        */
/*      record first.                                                   */
/************************************************************************/

void DDFRecordIndex::AddRecord(int nKey, std::unique_ptr<DDFRecord> poRecord)

{
    DDFIndexedRecord indexRec;
    indexRec.nKey = nKey;
    indexRec.poRecord = std::move(poRecord);
    asRecords.push_back(std::move(indexRec));
    oMapKeyToRecord[nKey] = asRecords.back().poRecord.get();
    bSorted = false;
}

/************************************************************************/
/*                             FindRecord()                             */
/*                                                                      */
/*      Though the returned pointer is not const, it should not         */
/*      be freed by application code.                                   */
/************************************************************************/

DDFRecord *DDFRecordIndex::FindRecord(int nKey) const

{
    auto oIter = oMapKeyToRecord.find(nKey);
    if (oIter == oMapKeyToRecord.end())
        return nullptr;
    return oIter->second;
}

/************************************************************************/
/*                            RemoveRecord()                            */
/************************************************************************/

bool DDFRecordIndex::RemoveRecord(int nKey)

{
    if (!bSorted)
        Sort();

    if (asRecords.empty() || nKey < asRecords[0].nKey)
        return false;

    /* -------------------------------------------------------------------- */
    /*      Do a binary search based on the key to find the desired record. */
    /* -------------------------------------------------------------------- */
    size_t nMinIndex = 0;
    size_t nMaxIndex = asRecords.size() - 1;
    size_t nTestIndex = 0;

    while (nMinIndex <= nMaxIndex)
    {
        nTestIndex = (nMaxIndex + nMinIndex) / 2;

        if (asRecords[nTestIndex].nKey < nKey)
            nMinIndex = nTestIndex + 1;
        else if (asRecords[nTestIndex].nKey > nKey)
            nMaxIndex = nTestIndex - 1;
        else
            break;
    }

    if (nMinIndex > nMaxIndex)
        return false;

    /* -------------------------------------------------------------------- */
    /*      Delete this record.                                             */
    /* -------------------------------------------------------------------- */
    asRecords.erase(asRecords.begin() + nTestIndex);

    auto oIter = oMapKeyToRecord.find(nKey);
    CPLAssert(oIter != oMapKeyToRecord.end());
    oMapKeyToRecord.erase(oIter);

    return true;
}

/************************************************************************/
/*                                Sort()                                */
/*                                                                      */
/*      Sort the records based on the key.                              */
/************************************************************************/

void DDFRecordIndex::Sort() const

{
    if (bSorted)
        return;

    std::sort(asRecords.begin(), asRecords.end(),
              [](const DDFIndexedRecord &a, const DDFIndexedRecord &b)
              { return a.nKey < b.nKey; });

    bSorted = true;
}

/************************************************************************/
/*                             GetByIndex()                             */
/************************************************************************/

const DDFRecord *DDFRecordIndex::GetByIndex(int nIndex) const

{
    if (!bSorted)
        Sort();

    if (nIndex < 0 || nIndex >= GetCount())
        return nullptr;

    return asRecords[nIndex].poRecord.get();
}

/************************************************************************/
/*                        GetClientInfoByIndex()                        */
/************************************************************************/

const void *DDFRecordIndex::GetClientInfoByIndex(int nIndex) const

{
    if (!bSorted)
        Sort();

    if (nIndex < 0 || nIndex >= GetCount())
        return nullptr;

    return asRecords[nIndex].pClientData;
}

/************************************************************************/
/*                        SetClientInfoByIndex()                        */
/************************************************************************/

void DDFRecordIndex::SetClientInfoByIndex(int nIndex, const void *pClientData)

{
    if (!bSorted)
        Sort();

    if (nIndex < 0 || nIndex >= GetCount())
        return;

    asRecords[nIndex].pClientData = pClientData;
}
