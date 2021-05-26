/******************************************************************************
 *
 * Purpose:  Implementation of the MetadataSegment class.
 *
 * This class is used to manage access to the SYS METADATA segment.  This
 * segment holds all the metadata for objects in the PCIDSK file.
 *
 * This class is closely partnered with the MetadataSet class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

#include "pcidsk_exception.h"
#include "pcidsk_file.h"
#include "segment/metadatasegment.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <map>

using namespace PCIDSK;

/************************************************************************/
/*                          MetadataSegment()                           */
/************************************************************************/

MetadataSegment::MetadataSegment( PCIDSKFile *fileIn, int segmentIn,
                                  const char *segment_pointer )
        : CPCIDSKSegment( fileIn, segmentIn, segment_pointer )

{
    loaded = false;
}

/************************************************************************/
/*                          ~MetadataSegment()                          */
/************************************************************************/

MetadataSegment::~MetadataSegment()

{
    try
    {
        Synchronize();
    }
    catch( const PCIDSKException& ex )
    {
        fprintf( stderr, /*ok*/
                 "Exception in MetadataSegment destructor: %s\n",
                 ex.what() );
    }
    catch( ... )
    {
        fprintf( stderr, /*ok*/
                 "PCIDSK SDK Failure in MetadataSegment destructor, "
                 "unexpected exception.\n" );
    }
}

/************************************************************************/
/*                            Synchronize()                             */
/************************************************************************/

void MetadataSegment::Synchronize()
{
    if( loaded && !update_list.empty() &&
        this->file->GetUpdatable())
        Save();
}

/************************************************************************/
/*                                Load()                                */
/************************************************************************/

void MetadataSegment::Load()

{
    if( loaded )
        return;

    // TODO: this should likely be protected by a mutex.

/* -------------------------------------------------------------------- */
/*      Load the segment contents into a buffer.                        */
/* -------------------------------------------------------------------- */

    // data_size < 1024 will throw an exception in SetSize()
    seg_data.SetSize( data_size < 1024 ? -1 : (int) (data_size - 1024) );

    ReadFromFile( seg_data.buffer, 0, data_size - 1024 );

    loaded = true;
}

/************************************************************************/
/*                           FetchGroupMetadata()                       */
/************************************************************************/

void MetadataSegment::FetchGroupMetadata( const char *group, int id,
                                     std::map<std::string,std::string> &md_set)

{
/* -------------------------------------------------------------------- */
/*      Load the metadata segment if not already loaded.                */
/* -------------------------------------------------------------------- */
    Load();

/* -------------------------------------------------------------------- */
/*      Establish the key prefix we are searching for.                  */
/* -------------------------------------------------------------------- */
    char key_prefix[200];
    size_t  prefix_len;

    snprintf( key_prefix, sizeof(key_prefix), "METADATA_%s_%d_", group, id );
    prefix_len = std::strlen(key_prefix);

/* -------------------------------------------------------------------- */
/*      Process all the metadata entries in this segment, searching     */
/*      for those that match our prefix.                                */
/* -------------------------------------------------------------------- */
    const char *pszNext;

    for( pszNext = (const char *) seg_data.buffer; *pszNext != '\0'; )
    {
/* -------------------------------------------------------------------- */
/*      Identify the end of this line, and the split character (:).     */
/* -------------------------------------------------------------------- */
        int i_split = -1, i;

        for( i=0;
             pszNext[i] != 10 && pszNext[i] != 12 && pszNext[i] != 0;
             i++)
        {
            if( i_split == -1 && pszNext[i] == ':' )
                i_split = i;
        }

        if( pszNext[i] == '\0' )
            break;

/* -------------------------------------------------------------------- */
/*      If this matches our prefix, capture the key and value.          */
/* -------------------------------------------------------------------- */
        if( i_split != -1 && std::strncmp(pszNext,key_prefix,prefix_len) == 0 )
        {
            std::string key, value;

            key.assign( pszNext+prefix_len, i_split-prefix_len );

            if( pszNext[i_split+1] == ' ' )
                value.assign( pszNext+i_split+2, i-i_split-2 );
            else
                value.assign( pszNext+i_split+1, i-i_split-1 );

            md_set[key] = value;
        }

/* -------------------------------------------------------------------- */
/*      Advance to start of next line.                                  */
/* -------------------------------------------------------------------- */
        pszNext = pszNext + i;
        while( *pszNext == 10 || *pszNext == 12 )
            pszNext++;
    }
}

/************************************************************************/
/*                         SetGroupMetadataValue()                      */
/************************************************************************/

void MetadataSegment::SetGroupMetadataValue( const char *group, int id,
                                        const std::string& key, const std::string& value )

{
    Load();

    char key_prefix[200];

    snprintf( key_prefix, sizeof(key_prefix), "METADATA_%s_%d_", group, id );

    std::string full_key;

    full_key = key_prefix;
    full_key += key;

    update_list[full_key] = value;
}

/************************************************************************/
/*                                Save()                                */
/*                                                                      */
/*      When saving we first need to merge in any updates.  We put      */
/*      this off since scanning and updating the metadata doc could     */
/*      be expensive if done for each item.                             */
/************************************************************************/

void MetadataSegment::Save()

{
    std::string new_data;

/* -------------------------------------------------------------------- */
/*      Process all the metadata entries in this segment, searching     */
/*      for those that match our prefix.                                */
/* -------------------------------------------------------------------- */
    const char *pszNext;

    for( pszNext = (const char *) seg_data.buffer; *pszNext != '\0'; )
    {
/* -------------------------------------------------------------------- */
/*      Identify the end of this line, and the split character (:).     */
/* -------------------------------------------------------------------- */
        int i_split = -1, i;

        for( i=0;
             pszNext[i] != 10 && pszNext[i] != 12 && pszNext[i] != 0;
             i++)
        {
            if( i_split == -1 && pszNext[i] == ':' )
                i_split = i;
        }

        if( pszNext[i] == '\0' )
            break;
/* -------------------------------------------------------------------- */
/*      If we have a new value for this key, do not copy over the       */
/*      old value.  Otherwise append the old value to our new image.    */
/* -------------------------------------------------------------------- */
        if (i_split != -1)
        {
            std::string full_key;

            full_key.assign( pszNext, i_split );

            if( update_list.count(full_key) == 1 )
                /* do not transfer - we will append later */;
            else
                new_data.append( pszNext, i+1 );
        }

/* -------------------------------------------------------------------- */
/*      Advance to start of next line.                                  */
/* -------------------------------------------------------------------- */
        pszNext = pszNext + i;
        while( *pszNext == 10 || *pszNext == 12 )
            pszNext++;
    }

/* -------------------------------------------------------------------- */
/*      Append all the update items with non-empty values.              */
/* -------------------------------------------------------------------- */
    std::map<std::string,std::string>::iterator it;

    for( it = update_list.begin(); it != update_list.end(); ++it )
    {
        if( it->second.empty() )
            continue;

        std::string line;

        line = it->first;
        line += ": ";
        line += it->second;
        line += "\n";

        new_data += line;
    }

    update_list.clear();

/* -------------------------------------------------------------------- */
/*      Move the new value into our buffer, and write to disk.          */
/* -------------------------------------------------------------------- */
    if( new_data.size() % 512 != 0 ) // zero fill the last block.
    {
        new_data.resize( new_data.size() + (512 - (new_data.size() % 512)),
                         '\0' );
    }

    seg_data.SetSize( static_cast<int>(new_data.size()) );
    std::memcpy( seg_data.buffer, new_data.c_str(), new_data.size() );

    WriteToFile( seg_data.buffer, 0, seg_data.buffer_size );
}
