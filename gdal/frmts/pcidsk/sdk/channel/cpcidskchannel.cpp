/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKChannel Abstract class.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"
#include "pcidsk_channel.h"
#include "core/cpcidskfile.h"
#include "channel/cpcidskchannel.h"
#include "channel/ctiledchannel.h"
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "cpl_port.h"

using namespace PCIDSK;

/************************************************************************/
/*                           CPCIDSKChannel()                           */
/************************************************************************/

CPCIDSKChannel::CPCIDSKChannel( PCIDSKBuffer &image_header, 
                                uint64 ih_offset,
                                CPCIDSKFile *file, 
                                eChanType pixel_type,
                                int channel_number )

{
    this->pixel_type = pixel_type;
    this->file = file;
    this->channel_number = channel_number;
    this->ih_offset = ih_offset;

    width = file->GetWidth();
    height = file->GetHeight();

    block_width = width;
    block_height = 1;

/* -------------------------------------------------------------------- */
/*      Establish if we need to byte swap the data on load/store.       */
/* -------------------------------------------------------------------- */
    if( channel_number != -1 )
    {
        unsigned short test_value = 1;

        byte_order = image_header.buffer[201];
        if( ((uint8 *) &test_value)[0] == 1 )
            needs_swap = (byte_order != 'S');
        else
            needs_swap = (byte_order == 'S');
        
        if( pixel_type == CHN_8U )
            needs_swap = 0;

        LoadHistory( image_header );

/* -------------------------------------------------------------------- */
/*      Initialize the metadata object, but do not try to load till     */
/*      needed.  We avoid doing this for unassociated channels such     */
/*      as overviews.                                                   */
/* -------------------------------------------------------------------- */
        metadata.Initialize( file, "IMG", channel_number );
    }

/* -------------------------------------------------------------------- */
/*      No overviews for unassociated files, so just mark them as       */
/*      initialized.                                                    */
/* -------------------------------------------------------------------- */
    overviews_initialized = (channel_number == -1);
}

/************************************************************************/
/*                          ~CPCIDSKChannel()                           */
/************************************************************************/

CPCIDSKChannel::~CPCIDSKChannel()

{
    InvalidateOverviewInfo();
}

/************************************************************************/
/*                       InvalidateOverviewInfo()                       */
/*                                                                      */
/*      This is called when CreateOverviews() creates overviews - we    */
/*      invalidate our loaded info and re-establish on a next request.  */
/************************************************************************/

void CPCIDSKChannel::InvalidateOverviewInfo()

{
    for( size_t io=0; io < overview_bands.size(); io++ )
    {
        if( overview_bands[io] != NULL )
        {
            delete overview_bands[io];
            overview_bands[io] = NULL;
        }
    }

    overview_infos.clear();
    overview_bands.clear();
    overview_decimations.clear();

    overviews_initialized = false;
}

/************************************************************************/
/*                       EstablishOverviewInfo()                        */
/************************************************************************/
void CPCIDSKChannel::EstablishOverviewInfo() const

{
    if( overviews_initialized )
        return;

    overviews_initialized = true;

    std::vector<std::string> keys = GetMetadataKeys();
    size_t i;

    for( i = 0; i < keys.size(); i++ )
    {
        if( strncmp(keys[i].c_str(),"_Overview_",10) != 0 )
            continue;

        std::string value = GetMetadataValue( keys[i] );

        overview_infos.push_back( value );
        overview_bands.push_back( NULL );
        overview_decimations.push_back( atoi(keys[i].c_str()+10) );
    }
}

/************************************************************************/
/*                           GetBlockCount()                            */
/************************************************************************/

int CPCIDSKChannel::GetBlockCount() const

{
    // We deliberately call GetBlockWidth() and GetWidth() to trigger
    // computation of the values for tiled layers.  At some point it would
    // be good to cache the block count as this computation is a bit expensive

    int x_block_count = (GetWidth() + GetBlockWidth() - 1) / GetBlockWidth();
    int y_block_count = (GetHeight() + GetBlockHeight() - 1) / GetBlockHeight();

    return x_block_count * y_block_count;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int CPCIDSKChannel::GetOverviewCount()

{
    EstablishOverviewInfo();

    return overview_infos.size();
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

PCIDSKChannel *CPCIDSKChannel::GetOverview( int overview_index )

{
    EstablishOverviewInfo();

    if( overview_index < 0 || overview_index >= (int) overview_infos.size() )
        ThrowPCIDSKException( "Non existent overview (%d) requested.", 
                              overview_index );

    if( overview_bands[overview_index] == NULL )
    {
        PCIDSKBuffer image_header(1024), file_header(1024);
        char  pseudo_filename[65];

        sprintf( pseudo_filename, "/SIS=%d", 
                 atoi(overview_infos[overview_index].c_str()) );

        image_header.Put( pseudo_filename, 64, 64 );
        
        overview_bands[overview_index] = 
            new CTiledChannel( image_header, 0, file_header, -1, file, 
                               CHN_UNKNOWN );
    }

    return overview_bands[overview_index];
}

/************************************************************************/
/*                          IsOverviewValid()                           */
/************************************************************************/

bool CPCIDSKChannel::IsOverviewValid( int overview_index )

{
    EstablishOverviewInfo();

    if( overview_index < 0 || overview_index >= (int) overview_infos.size() )
        ThrowPCIDSKException( "Non existent overview (%d) requested.", 
                              overview_index );

    int sis_id, validity=0;

    sscanf( overview_infos[overview_index].c_str(), "%d %d", 
            &sis_id, &validity );
    
    return validity != 0;
}

/************************************************************************/
/*                       GetOverviewResampling()                        */
/************************************************************************/

std::string CPCIDSKChannel::GetOverviewResampling( int overview_index )

{
    EstablishOverviewInfo();

    if( overview_index < 0 || overview_index >= (int) overview_infos.size() )
        ThrowPCIDSKException( "Non existent overview (%d) requested.", 
                              overview_index );

    int sis_id, validity=0;
    char resampling[17];

    sscanf( overview_infos[overview_index].c_str(), "%d %d %16s", 
            &sis_id, &validity, &(resampling[0]) );
    
    return resampling;
}

/************************************************************************/
/*                        SetOverviewValidity()                         */
/************************************************************************/

void CPCIDSKChannel::SetOverviewValidity( int overview_index, 
                                          bool new_validity )

{
    EstablishOverviewInfo();

    if( overview_index < 0 || overview_index >= (int) overview_infos.size() )
        ThrowPCIDSKException( "Non existent overview (%d) requested.", 
                              overview_index );

    int sis_id, validity=0;
    char resampling[17];
    
    sscanf( overview_infos[overview_index].c_str(), "%d %d %16s", 
            &sis_id, &validity, &(resampling[0]) );
    
    // are we already set to this value?
    if( new_validity == (validity != 0) )
        return;

    char new_info[48];

    sprintf( new_info, "%d %d %s", 
             sis_id, (new_validity ? 1 : 0 ), resampling );

    overview_infos[overview_index] = new_info;

    // write back to metadata.
    char key[20];
    sprintf( key, "_Overview_%d", overview_decimations[overview_index] );

    SetMetadataValue( key, new_info );
}

/************************************************************************/
/*                        InvalidateOverviews()                         */
/*                                                                      */
/*      Whenever a write is done on this band, we will invalidate       */
/*      any previously valid overviews.                                 */
/************************************************************************/

void CPCIDSKChannel::InvalidateOverviews()

{
    EstablishOverviewInfo();

    for( int i = 0; i < GetOverviewCount(); i++ )
        SetOverviewValidity( i, false );
}

/************************************************************************/
/*                  GetOverviewLevelMapping()                           */
/************************************************************************/

std::vector<int> CPCIDSKChannel::GetOverviewLevelMapping() const
{
    EstablishOverviewInfo();
    
    return overview_decimations;
}

/************************************************************************/
/*                           GetDescription()                           */
/************************************************************************/

std::string CPCIDSKChannel::GetDescription() 

{
    if( ih_offset == 0 )
        return "";

    PCIDSKBuffer ih_1(64);
    std::string ret;

    file->ReadFromFile( ih_1.buffer, ih_offset, 64 );
    ih_1.Get(0,64,ret);

    return ret;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void CPCIDSKChannel::SetDescription( const std::string &description )

{
    if( ih_offset == 0 )
        ThrowPCIDSKException( "Description cannot be set on overviews." );
        
    PCIDSKBuffer ih_1(64);
    ih_1.Put( description.c_str(), 0, 64 );
    file->WriteToFile( ih_1.buffer, ih_offset, 64 );
}

/************************************************************************/
/*                            LoadHistory()                             */
/************************************************************************/

void CPCIDSKChannel::LoadHistory( const PCIDSKBuffer &image_header )

{
    // Read the history from the image header. PCIDSK supports
    // 8 history entries per channel.

    std::string hist_msg;
    history_.clear();
    for (unsigned int i = 0; i < 8; i++)
    {
        image_header.Get(384 + i * 80, 80, hist_msg);

        // Some programs seem to push history records with a trailing '\0'
        // so do some extra processing to cleanup.  FUN records on segment
        // 3 of eltoro.pix are an example of this.
        size_t size = hist_msg.size();
        while( size > 0 
               && (hist_msg[size-1] == ' ' || hist_msg[size-1] == '\0') )
            size--;

        hist_msg.resize(size);
        
        history_.push_back(hist_msg);
    }
}

/************************************************************************/
/*                         GetHistoryEntries()                          */
/************************************************************************/

std::vector<std::string> CPCIDSKChannel::GetHistoryEntries() const
{
    return history_;
}

/************************************************************************/
/*                         SetHistoryEntries()                          */
/************************************************************************/

void CPCIDSKChannel::SetHistoryEntries(const std::vector<std::string> &entries)

{
    if( ih_offset == 0 )
        ThrowPCIDSKException( "Attempt to update history on a raster that is not\na conventional band with an image header." );

    PCIDSKBuffer image_header(1024);

    file->ReadFromFile( image_header.buffer, ih_offset, 1024 );
    
    for( unsigned int i = 0; i < 8; i++ )
    {
        const char *msg = "";
        if( entries.size() > i )
            msg = entries[i].c_str();

        image_header.Put( msg, 384 + i * 80, 80 );
    }

    file->WriteToFile( image_header.buffer, ih_offset, 1024 );

    // Force reloading of history_
    LoadHistory( image_header );
}

/************************************************************************/
/*                            PushHistory()                             */
/************************************************************************/

void CPCIDSKChannel::PushHistory( const std::string &app,
                                  const std::string &message )

{
#define MY_MIN(a,b)      ((a<b) ? a : b)

    char current_time[17];
    char history[81];

    GetCurrentDateTime( current_time );

    memset( history, ' ', 80 );
    history[80] = '\0';

    memcpy( history + 0, app.c_str(), MY_MIN(app.size(),7) );
    history[7] = ':';
    
    memcpy( history + 8, message.c_str(), MY_MIN(message.size(),56) );
    memcpy( history + 64, current_time, 16 );

    std::vector<std::string> history_entries = GetHistoryEntries();

    history_entries.insert( history_entries.begin(), history );
    history_entries.resize(8);

    SetHistoryEntries( history_entries );
}

/************************************************************************/
/*                            GetChanInfo()                             */
/************************************************************************/
void CPCIDSKChannel::GetChanInfo( std::string &filename, uint64 &image_offset, 
                                  uint64 &pixel_offset, uint64 &line_offset, 
                                  bool &little_endian ) const

{
    image_offset = 0;
    pixel_offset = 0;
    line_offset = 0;
    little_endian = true;
    filename = "";
}

/************************************************************************/
/*                            SetChanInfo()                             */
/************************************************************************/

void CPCIDSKChannel::SetChanInfo( CPL_UNUSED std::string filename,
                                  CPL_UNUSED uint64 image_offset,
                                  CPL_UNUSED uint64 pixel_offset,
                                  CPL_UNUSED uint64 line_offset,
                                  CPL_UNUSED bool little_endian )
{
    ThrowPCIDSKException( "Attempt to SetChanInfo() on a channel that is not FILE interleaved." );
}

/************************************************************************/
/*                            GetEChanInfo()                            */
/************************************************************************/
void CPCIDSKChannel::GetEChanInfo( std::string &filename, int &echannel,
                                   int &exoff, int &eyoff, 
                                   int &exsize, int &eysize ) const

{
    echannel = 0;
    exoff = 0;
    eyoff = 0;
    exsize = 0;
    eysize = 0;
    filename = "";
}

/************************************************************************/
/*                            SetEChanInfo()                            */
/************************************************************************/

void CPCIDSKChannel::SetEChanInfo( CPL_UNUSED std::string filename,
                                   CPL_UNUSED int echannel,
                                   CPL_UNUSED int exoff,
                                   CPL_UNUSED int eyoff,
                                   CPL_UNUSED int exsize,
                                   CPL_UNUSED int eysize )
{
    ThrowPCIDSKException( "Attempt to SetEChanInfo() on a channel that is not FILE interleaved." );
}
