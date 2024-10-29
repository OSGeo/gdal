/******************************************************************************
 *
 * Purpose:  Implementation of the MetadataSet class.  This is a container
 *           for a set of metadata, and used by the file, channel and segment
 *           classes to manage metadata for themselves.  It is not public
 *           to SDK users.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "pcidsk_exception.h"
#include "core/metadataset.h"

#include "segment/metadatasegment.h"

#include <string>

using namespace PCIDSK;

/************************************************************************/
/*                            MetadataSet()                             */
/************************************************************************/

MetadataSet::MetadataSet()


{
    this->file = nullptr;
    id = -1;
    loaded = false;
}

/************************************************************************/
/*                            ~MetadataSet()                            */
/************************************************************************/

MetadataSet::~MetadataSet()

{
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void MetadataSet::Initialize( PCIDSKFile *fileIn, const std::string& groupIn, int idIn )

{
    this->file = fileIn;
    this->group = groupIn;
    this->id = idIn;
}

/************************************************************************/
/*                                Load()                                */
/************************************************************************/

void MetadataSet::Load()

{
    if( loaded )
        return;

    // This legitimately occurs in some situations, such for overview channel
    // objects.
    if( file == nullptr )
    {
        loaded = true;
        return;
    }

    PCIDSKSegment *seg = file->GetSegment( SEG_SYS , "METADATA");

    if( seg == nullptr )
    {
        loaded = true;
        return;
    }

    MetadataSegment *md_seg = dynamic_cast<MetadataSegment *>( seg );
    if( md_seg )
        md_seg->FetchGroupMetadata( group.c_str(), id, md_set );
    loaded = true;
}

/************************************************************************/
/*                          GetMetadataValue()                          */
/************************************************************************/

std::string MetadataSet::GetMetadataValue( const std::string& key )

{
    if( !loaded )
        Load();

    if( md_set.count(key) == 0 )
        return "";
    else
        return md_set[key];
}

/************************************************************************/
/*                          SetMetadataValue()                          */
/************************************************************************/

void MetadataSet::SetMetadataValue( const std::string& key, const std::string& value )
{
    if( !loaded )
        Load();

    if( file == nullptr )
    {
        return ThrowPCIDSKException( "Attempt to set metadata on an unassociated MetadataSet, likely an overview channel." );
    }

    md_set[key] = value;

    PCIDSKSegment *seg = file->GetSegment( SEG_SYS , "METADATA");

    if( seg == nullptr )
    {
        file->CreateSegment("METADATA",
                            "Please do not modify this metadata segment.",
                            SEG_SYS, 64); // Create a 32k segment by default.
        seg = file->GetSegment( SEG_SYS , "METADATA");
    }

    MetadataSegment *md_seg = dynamic_cast<MetadataSegment *>( seg );
    if( md_seg )
        md_seg->SetGroupMetadataValue( group.c_str(), id, key, value );
}

/************************************************************************/
/*                          GetMetadataKeys()                           */
/************************************************************************/

std::vector<std::string> MetadataSet::GetMetadataKeys()
{
    if( !loaded )
        Load();

    std::vector<std::string> keys;
    std::map<std::string,std::string>::iterator it;

    for( it = md_set.begin(); it != md_set.end(); ++it )
    {
        // If the value is empty the key value pair will be deleted in the file
        // This will ensure that the returned list key are in sync with the file
        if(!it->second.empty())
            keys.push_back(it->first);
    }

    return keys;
}
