/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
  * SPDX-License-Identifier: MIT
 *******************************************************************************/

#include "caddictionary.h"

using namespace std;
//
// CADDictionaryRecord
//

CADDictionaryRecord::CADDictionaryRecord() :
    objType(CADObject::UNUSED)
{
}

CADObject::ObjectType CADDictionaryRecord::getType() const
{
    return objType;
}

//
// CADXRecord
//

CADXRecord::CADXRecord()
{
    objType = CADObject::XRECORD;
}

const string CADXRecord::getRecordData() const
{
    return sRecordData;
}

void CADXRecord::setRecordData( const string& data )
{
    sRecordData = data;
}

//
// CADDictionary
//

CADDictionary::CADDictionary()
{
    objType = CADObject::DICTIONARY;
}

CADDictionary::~CADDictionary()
{
}

size_t CADDictionary::getRecordsCount()
{
    return astXRecords.size();
}

CADDictionaryItem CADDictionary::getRecord( size_t index )
{
    return astXRecords[index];
}

void CADDictionary::addRecord( CADDictionaryItem record )
{
    astXRecords.emplace_back( record );
}

string CADDictionary::getRecordByName(const string& name) const
{
    for( size_t i = 0; i < astXRecords.size(); ++i )
    {
        if( astXRecords[i].first.compare(name) == 0 )
        {
            std::shared_ptr<CADDictionaryRecord> XRecordPtr = astXRecords[i].second;
            if(XRecordPtr == nullptr ||
               XRecordPtr->getType() != CADObject::XRECORD)
                continue;
            CADXRecord * poXRecord = static_cast<CADXRecord*>(XRecordPtr.get() );
            return poXRecord->getRecordData();
        }
    }
    return "";
}
