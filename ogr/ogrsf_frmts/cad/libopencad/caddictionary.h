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
