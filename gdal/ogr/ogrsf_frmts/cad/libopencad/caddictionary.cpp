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
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
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
