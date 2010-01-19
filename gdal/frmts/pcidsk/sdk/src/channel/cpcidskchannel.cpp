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
#include "pcidsk_exception.h"
#include "pcidsk_channel.h"
#include "core/cpcidskfile.h"
#include "channel/cpcidskchannel.h"
#include "channel/ctiledchannel.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>

using namespace PCIDSK;

/************************************************************************/
/*                           CPCIDSKChannel()                           */
/************************************************************************/

CPCIDSKChannel::CPCIDSKChannel( PCIDSKBuffer &image_header, 
                                CPCIDSKFile *file, 
                                eChanType pixel_type,
                                int channel_number )

{
    this->pixel_type = pixel_type;
    this->file = file;
    this->channel_number = channel_number;

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
    overviews_initialized = channel_number == -1;
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

    overviews_initialized = false;
}

/************************************************************************/
/*                       EstablishOverviewInfo()                        */
/************************************************************************/

void CPCIDSKChannel::EstablishOverviewInfo()

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
    }
}

/************************************************************************/
/*                           GetBlockCount()                            */
/************************************************************************/

int CPCIDSKChannel::GetBlockCount()

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

    if( overview_bands[overview_index] == NULL )
    {
        PCIDSKBuffer image_header(1024), file_header(1024);
        char  pseudo_filename[65];

        sprintf( pseudo_filename, "/SIS=%d", 
                 atoi(overview_infos[overview_index].c_str()) );

        image_header.Put( pseudo_filename, 64, 64 );
        
        overview_bands[overview_index] = 
            new CTiledChannel( image_header, file_header, -1, file, 
                               CHN_UNKNOWN );
    }

    return overview_bands[overview_index];
}

