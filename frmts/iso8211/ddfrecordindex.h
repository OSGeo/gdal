/******************************************************************************
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements DDFRecordIndex class. This class is used to cache
 *           ISO8211 records for spatial objects so they can be efficiently
 *           assembled later as features.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef DDFRECORDINDEX_H_INCLUDED
#define DDFRECORDINDEX_H_INCLUDED

#include "iso8211.h"

#include <map>

/************************************************************************/
/*                            DDFRecordIndex                            */
/*                                                                      */
/*      Maintain an index of DDF records based on an integer key.       */
/************************************************************************/

struct DDFIndexedRecord
{
    int nKey = 0;
    std::unique_ptr<DDFRecord> poRecord{};
    const void *pClientData = nullptr;
};

class CPL_DLL DDFRecordIndex
{
    mutable bool bSorted = false;
    mutable std::vector<DDFIndexedRecord> asRecords{};
    std::map<int, DDFRecord *> oMapKeyToRecord{};

    void Sort() const;

    DDFRecordIndex(const DDFRecordIndex &) = delete;
    DDFRecordIndex &operator=(const DDFRecordIndex &) = delete;
    DDFRecordIndex(DDFRecordIndex &&) = delete;
    DDFRecordIndex &operator=(DDFRecordIndex &&) = delete;

  public:
    DDFRecordIndex();
    ~DDFRecordIndex();

    void AddRecord(int nKey, std::unique_ptr<DDFRecord> poRecord);
    bool RemoveRecord(int nKey);

    DDFRecord *FindRecord(int nKey) const;

    void Clear();

    int GetCount() const
    {
        return static_cast<int>(asRecords.size());
    }

    const DDFRecord *GetByIndex(int i) const;
    const void *GetClientInfoByIndex(int i) const;
    void SetClientInfoByIndex(int i, const void *pClientInfo);
};

#endif
