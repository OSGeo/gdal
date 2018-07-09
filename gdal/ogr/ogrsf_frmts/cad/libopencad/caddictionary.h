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
#ifndef CADDICTIONARY_H
#define CADDICTIONARY_H

#include "cadobjects.h"
#include <memory>

/*
 * @brief Base-class for XRecord and Dictionary.
 */
class OCAD_EXTERN CADDictionaryRecord
{
public:
    CADDictionaryRecord();
    virtual ~CADDictionaryRecord(){}

    CADObject::ObjectType getType() const;

protected:
    CADObject::ObjectType objType;
};

/*
 * @brief Class which implements XRecord
 */
class OCAD_EXTERN CADXRecord : public CADDictionaryRecord
{
public:
    CADXRecord();
    virtual ~CADXRecord(){}

    const std::string getRecordData() const;
    void              setRecordData( const std::string& data );

private:
    std::string sRecordData;
};

/*
 * @brief Class which implements Dictionary
 */
typedef std::pair< std::string, std::shared_ptr<CADDictionaryRecord>> CADDictionaryItem;
class OCAD_EXTERN CADDictionary : public CADDictionaryRecord
{
public:
    CADDictionary();
    virtual ~CADDictionary();

    size_t getRecordsCount();
    void   addRecord( CADDictionaryItem );
    CADDictionaryItem getRecord( size_t index );
    std::string getRecordByName(const std::string& name) const;
private:
    std::vector< CADDictionaryItem > astXRecords;
};

#endif // CADDICTIONARY_H
