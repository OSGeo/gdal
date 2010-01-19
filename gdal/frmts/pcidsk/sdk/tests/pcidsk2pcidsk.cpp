/******************************************************************************
 *
 * Purpose:  Commandline utility to copy a PCIDSK file.
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

#include "pcidsk.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

using namespace PCIDSK;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: pcidsk2pcidsk [-b buffersize_in_mb] src_file dst_file\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char **argv)
{

/* -------------------------------------------------------------------- */
/*      Process options.                                                */
/* -------------------------------------------------------------------- */
    const char *src_filename = NULL;
    const char *dst_filename = NULL;
    int buffersize = 0;
    int i_arg;

    for( i_arg = 1; i_arg < argc; i_arg++ )
    {
        if( strcmp(argv[i_arg],"-b") == 0 && i_arg < argc-1 )
            buffersize = (int) (atof(argv[++i_arg]) * 1024 * 1024);
        else if( argv[i_arg][0] == '-' )
            Usage();
        else if( src_filename == NULL )
            src_filename = argv[i_arg];
        else if( dst_filename == NULL )
            dst_filename = argv[i_arg];
        else
            Usage();
    }

    if( dst_filename == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open source file.                                               */
/* -------------------------------------------------------------------- */
    try
    {
        PCIDSKFile *src_file, *dst_file;

        src_file = Open( src_filename, "r", NULL );

/* -------------------------------------------------------------------- */
/*      Create output file with similar options and channel types.      */
/* -------------------------------------------------------------------- */
        std::vector<eChanType> channel_types;
        std::string options = src_file->GetInterleaving();
        int i;

        for( i = 0; i < src_file->GetChannels(); i++ )
            channel_types.push_back( src_file->GetChannel(i+1)->GetType() );

        if( channel_types.size() > 0 && options == "FILE" ) // TILED?
        {
            PCIDSKChannel *src_chan = src_file->GetChannel(1);

            if( src_chan->GetBlockHeight() > 1 )
            {
                char tile_size[128];
                sprintf( tile_size, "%d", src_chan->GetBlockWidth() );
                options = "TILED";
                options += tile_size;
            }
        }
    
        dst_file = Create( dst_filename, 
                           src_file->GetWidth(), src_file->GetHeight(),
                           channel_types.size(),
                           &(channel_types[0]), options, NULL );

/* -------------------------------------------------------------------- */
/*      Copy georeferencing.                                            */
/* -------------------------------------------------------------------- */
        PCIDSKGeoref*src_geo = dynamic_cast<PCIDSKGeoref*>(src_file->GetSegment(1));

        if( src_geo != NULL )
        {
            double a1, a2, xrot, b1, yrot, b3;

            PCIDSKGeoref *dst_geo = 
                dynamic_cast<PCIDSKGeoref*>(dst_file->GetSegment(1));
            std::vector<double> parms;

            src_geo->GetTransform( a1, a2, xrot, b1, yrot, b3 );
        
            dst_geo->WriteSimple( src_geo->GetGeosys(), 
                                  a1, a2, xrot, b1, yrot, b3 );

            parms = src_geo->GetParameters();
            dst_geo->WriteParameters( parms );
        }
    
/* -------------------------------------------------------------------- */
/*      Transfer over the contents band by band.                        */
/* -------------------------------------------------------------------- */
        int chan_num;

        for( chan_num = 1; chan_num <= src_file->GetChannels(); chan_num++ )
        {
            PCIDSKChannel *src_chan, *dst_chan;

            src_chan = src_file->GetChannel(chan_num);
            dst_chan = dst_file->GetChannel(chan_num);

            if( src_chan->GetBlockWidth() != dst_chan->GetBlockWidth()
                || src_chan->GetBlockHeight() != dst_chan->GetBlockHeight() )
                ThrowPCIDSKException( "Output file block size does not match source." );
            int block_size = src_chan->GetBlockWidth() * src_chan->GetBlockHeight()
                * DataTypeSize( src_chan->GetType() );
            int block_count = src_chan->GetBlockCount();

            int blocks_in_buffer = 1;
            if( buffersize >= 0 )
            {
                blocks_in_buffer = buffersize / block_size;
                if( blocks_in_buffer == 0 )
                    blocks_in_buffer = 1;
                if( blocks_in_buffer > block_count )
                    blocks_in_buffer = block_count;
            }

            uint8 *image_block = (uint8 *) malloc(block_size * blocks_in_buffer );

/* -------------------------------------------------------------------- */
/*      Process block by block.                                         */
/* -------------------------------------------------------------------- */
            int block;

            for( block = 0; block < block_count;  )
            {
                int blocks_this_time = blocks_in_buffer;
                int i;

                if( blocks_this_time + block > block_count )
                    blocks_this_time = block_count - block;
            
                for( i = 0; i < blocks_this_time; i++ )
                    src_chan->ReadBlock( block+i, image_block + i * block_size );
            
                for( i = 0; i < blocks_this_time; i++ )
                    dst_chan->WriteBlock( block+i, image_block + i * block_size );

                block += blocks_this_time;
            }

            printf( "%d blocks transferred on channel %d, %d blocks at a time.\n", 
                    block_count, chan_num, blocks_in_buffer );
            fflush( stdout );

            free( image_block );
        }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
        delete src_file;
        delete dst_file;
    }
/* ==================================================================== */
/*      Catch any exception and report the message.                     */
/* ==================================================================== */

    catch( PCIDSK::PCIDSKException ex )
    {
        fprintf( stderr, "PCIDSKException:\n%s\n", ex.what() );
        exit( 1 );
    }
}
