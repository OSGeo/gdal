/******************************************************************************
 *
 * Purpose:  Commandline utility for listing contents of a PCIDSK file.
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
#include "pcidsk_vectorsegment.h"
#include "pcidsk_georef.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>

#ifdef DEBUG
#include "segment/cpcidskgeoref.h"
#endif

#include <iostream>

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: pcidsk_read [-p] [-l] <src_filename> [<dst_filename>]\n"
            "                   [-ls] [-lc] [-lv] [-lg]\n" );
    exit( 1 );
}

namespace {
/************************************************************************/
/*                          PrintVector()                               */
/************************************************************************/
void PrintVector(const std::vector<double> &vec)
{
    std::vector<double>::const_iterator iter = vec.begin();
    while (iter != vec.end()) {
        std::cout << *iter << " ";
        iter++;
    }
}

void ReportRPCSegment(PCIDSK::PCIDSKRPCSegment *rpcseg)
{
    printf("\tSensor Name: %s\n", rpcseg->GetSensorName().c_str());
    
    printf("\tRaster Dimensions: %u (lines) x %u (pixels)\n",
        rpcseg->GetLines(), rpcseg->GetPixels());
    
    printf("\tIs%s a nominal model\n", rpcseg->IsNominalModel() ? "" : " NOT");
    printf("\tIs%s user generated\n", rpcseg->IsUserGenerated() ? "" : " NOT");
    
    std::vector<double> xnum = rpcseg->GetXNumerator();
    std::vector<double> xdenom = rpcseg->GetXDenominator();
    std::vector<double> ynum = rpcseg->GetYNumerator();
    std::vector<double> ydenom = rpcseg->GetYDenominator();
    
    printf("\tX Numerator Coeffs: ");
    PrintVector(xnum);
    printf("\n\tX Denominator Coeffs: ");
    PrintVector(xdenom);
    printf("\n\tY Numerator Coeffs: ");
    PrintVector(ynum);
    printf("\n\tY Denominator Coeffs:");
    PrintVector(ydenom);
    printf("\n");
    
    double xoffset, xscale, yoffset, yscale, zoffset, zscale, pixoffset, 
        pixscale, lineoffset, linescale;
    rpcseg->GetRPCTranslationCoeffs(xoffset, xscale, yoffset, yscale, zoffset,
        zscale, pixoffset, pixscale, lineoffset, linescale);
        
    printf("\tX offset: %f\n", xoffset);
    printf("\tX scale: %f\n", xscale);

    printf("\tY offset: %f\n", yoffset);
    printf("\tY scale: %f\n", yscale);

    printf("\tZ offset: %f\n", zoffset);
    printf("\tZ scale: %f\n", zscale);
    
    printf("\tPixel offset: %f\n", pixoffset);
    printf("\tPixel scale: %f\n", pixscale);

    printf("\tLine offset: %f\n", lineoffset);
    printf("\tLine scale: %f\n", linescale);
    
    printf("\tGeosys String: [%s]\n", rpcseg->GetGeosysString().c_str());
}

} // end anonymous namespace for helper functions

/************************************************************************/
/*                          ReportGeoSegment()                          */
/************************************************************************/

static void ReportGeoSegment( PCIDSK::PCIDSKSegment *segobj )

{
    try 
    { 
        double a1, a2, xrot, b1, yrot, b3;
        PCIDSK::PCIDSKGeoref *geoseg 
            = dynamic_cast<PCIDSK::PCIDSKGeoref*>( segobj );

        std::string geosys = geoseg->GetGeosys();
        geoseg->GetTransform( a1, a2, xrot, b1, yrot, b3 );

        printf( "  Geosys = '%s'\n", geosys.c_str() );
        printf( "  A1=%20.16g,   A2=%20.16g, XROT=%20.16g\n", a1, a2, xrot );
        printf( "  B1=%20.16g, YROT=%20.16g,   B3=%20.16g\n", b1, yrot, b3 );

        std::vector<double> parms;
        unsigned int i;

        parms = geoseg->GetParameters();
        for( i = 0; i < parms.size(); i++ )
            printf( "    Parameter[%d] = %.16g\n", i, parms[i] );

#ifdef DEBUG
        parms = (dynamic_cast<PCIDSK::CPCIDSKGeoref *>(segobj))->GetUSGSParameters();
        for( i = 0; i < parms.size(); i++ )
             printf( "    USGS Parameter[%d] = %.16g\n", i, parms[i] );
#endif

    }
    catch( PCIDSK::PCIDSKException ex )
    {
        fprintf( stderr, "PCIDSKException:\n%s\n", ex.what() );
        exit( 1 );
    }
}

/************************************************************************/
/*                        ReportVectorSegment()                         */
/************************************************************************/

static void ReportVectorSegment( PCIDSK::PCIDSKSegment *segobj )

{
    try 
    { 
        PCIDSK::PCIDSKVectorSegment *vecseg 
            = dynamic_cast<PCIDSK::PCIDSKVectorSegment*>( segobj );
        std::vector<PCIDSK::ShapeVertex> vertices;
        unsigned int i, field_count = vecseg->GetFieldCount();
        std::vector<PCIDSK::ShapeField> field_list;

        printf( "  Attribute fields:\n" );
        for( i = 0; i < field_count; i++ )
        {
            printf( "    %s (%s) %d/%s fmt:%s\n",
                    vecseg->GetFieldName(i).c_str(), 
                    vecseg->GetFieldDescription(i).c_str(), 
                    (int) vecseg->GetFieldType(i), 
                    PCIDSK::ShapeFieldTypeName(vecseg->GetFieldType(i)).c_str(),
                    vecseg->GetFieldFormat(i).c_str() );
        }
        
        printf( "\n" );
        
        
        for( PCIDSK::ShapeIterator it = vecseg->begin(); 
             it != vecseg->end(); it++ )
        {
            unsigned int i;
            
            vecseg->GetVertices( *it, vertices );
            
            printf( "  ShapeId: %d,  #vert=%d\n", *it, (int) vertices.size() );
            
            vecseg->GetFields( *it, field_list );
            for( i = 0; i < field_count; i++ )
            {
                std::string format = vecseg->GetFieldFormat(i);

                printf( "    %s: ", vecseg->GetFieldName(i).c_str() );
                switch( field_list[i].GetType() )
                {
                  case PCIDSK::FieldTypeInteger:
                    if( format.size() > 0 )
                        printf( format.c_str(), 
                                field_list[i].GetValueInteger() );
                    else
                        printf( "%d", field_list[i].GetValueInteger() );
                    break;

                  case PCIDSK::FieldTypeFloat:
                    if( format.size() > 0 )
                        printf( format.c_str(), 
                                field_list[i].GetValueFloat() );
                    else
                        printf( "%g", field_list[i].GetValueFloat() );
                    break;

                  case PCIDSK::FieldTypeDouble:
                    if( format.size() > 0 )
                        printf( format.c_str(), 
                                field_list[i].GetValueDouble() );
                    else
                        printf( "%g", field_list[i].GetValueDouble() );
                    break;

                  case PCIDSK::FieldTypeString:
                    if( format.size() > 0 )
                        printf( format.c_str(), 
                                field_list[i].GetValueString().c_str() );
                    else
                        printf( "%s", field_list[i].GetValueString().c_str() );
                    break;

                  case PCIDSK::FieldTypeCountedInt:
                  {
                      unsigned int ii;
                      std::vector<PCIDSK::int32> values = 
                          field_list[i].GetValueCountedInt();

                      printf( "(%d:", (int) values.size() );
                      for( ii = 0; ii < values.size(); ii++ )
                      {
                          if( format.size() > 0 )
                              printf( format.c_str(), values[ii] );
                          else
                              printf( "%d", values[ii] );

                          if( ii != values.size()-1 )
                              printf( "," );
                      }
                      printf( ")" );
                      break;
                  }

                  default:
                    printf( "NULL" );
                }

                printf( "\n" );
            }

            for( i = 0; i < vertices.size(); i++ )
                printf( "    %d: %.15g,%.15g,%.15g\n",
                        i,
                        vertices[i].x, 
                        vertices[i].y, 
                        vertices[i].z );
        }
    }
    catch( PCIDSK::PCIDSKException ex )
    {
        fprintf( stderr, "PCIDSKException:\n%s\n", ex.what() );
        exit( 1 );
    }
}


/************************************************************************/
/*                                main()                                */
/************************************************************************/
int main( int argc, char **argv)
{
/* -------------------------------------------------------------------- */
/*      Process options.                                                */
/* -------------------------------------------------------------------- */
    const char *src_file = NULL;
    const char *dst_file = NULL;
    std::string strategy = "-b";
    int i_arg;
    bool list_segments = false;
    bool list_channels = false;
    bool list_vectors = false;
    bool list_geo = false;

    for( i_arg = 1; i_arg < argc; i_arg++ )
    {
        if( strcmp(argv[i_arg],"-p") == 0 )
            strategy = argv[i_arg];
        else if( strcmp(argv[i_arg],"-l") == 0 )
            strategy = argv[i_arg];
        else if( strcmp(argv[i_arg],"-ls") == 0 )
            list_segments = true;
        else if( strcmp(argv[i_arg],"-lv") == 0 )
            list_vectors = true;
        else if( strcmp(argv[i_arg],"-lg") == 0 )
            list_geo = true;
        else if( strcmp(argv[i_arg],"-lc") == 0 )
            list_channels = true;
        else if( argv[i_arg][0] == '-' )
            Usage();
        else if( src_file == NULL )
            src_file = argv[i_arg];
        else if( dst_file == NULL )
            dst_file = argv[i_arg];
        else
            Usage();
    }

    if( src_file == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    try
    {
        PCIDSK::PCIDSKFile *file = PCIDSK::Open( src_file, "r", NULL );
        int channel_count = file->GetChannels();
        int channel_index;

        printf( "File: %dC x %dR x %dC (%s)\n", 
                file->GetWidth(), file->GetHeight(), file->GetChannels(),
                file->GetInterleaving().c_str() );

/* -------------------------------------------------------------------- */
/*      Report file level metadata if there is any.                     */
/* -------------------------------------------------------------------- */
        std::vector<std::string> keys = file->GetMetadataKeys();  
        size_t i_key;

        if( keys.size() > 0 )
            printf( "  Metadata:\n" );
        for( i_key = 0; i_key < keys.size(); i_key++ )
        {
            printf( "    %s: %s\n", 
                    keys[i_key].c_str(), 
                    file->GetMetadataValue(keys[i_key].c_str()).c_str() );
        }

/* -------------------------------------------------------------------- */
/*      If a destination raw file is requested, open it now.            */
/* -------------------------------------------------------------------- */
        FILE *fp_raw = NULL;

        if( dst_file != NULL )
            fp_raw = fopen(dst_file,"wb");

/* -------------------------------------------------------------------- */
/*      List channels if requested.                                     */
/* -------------------------------------------------------------------- */
        if( list_channels )
        {
            for( int channel = 1; channel <= file->GetChannels(); channel++ )
            {
                PCIDSK::PCIDSKChannel *chanobj = file->GetChannel( channel );

                printf( "Channel %d of type %s.\n",
                        channel, 
                        PCIDSK::DataTypeName(chanobj->GetType()).c_str() );;

                keys = chanobj->GetMetadataKeys();  

                if( keys.size() > 0 )
                    printf( "  Metadata:\n" );
                for( i_key = 0; i_key < keys.size(); i_key++ )
                {
                    printf( "    %s: %s\n", 
                            keys[i_key].c_str(), 
                            chanobj->GetMetadataValue(keys[i_key].c_str()).c_str() );
                }

                if( chanobj->GetOverviewCount() > 0 )
                {
                    int   io;

                    printf( "  Overviews: " );
                    for( io=0; io < chanobj->GetOverviewCount(); io++ )
                    {
                        PCIDSK::PCIDSKChannel *overobj = 
                            chanobj->GetOverview(io);

                        printf( "%dx%d ", 
                                overobj->GetWidth(), overobj->GetHeight() );
                    }
                    printf( "\n" );
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      List segments if requested.                                     */
/* -------------------------------------------------------------------- */
        if( list_segments || list_vectors || list_geo )
        {
            for( int segment = 1; segment <= 1024; segment++ )
            {
                try 
                {
                    PCIDSK::PCIDSKSegment *segobj = file->GetSegment( segment );

                    if( segobj != NULL && list_segments )
                    {
                        printf( "Segment %d/%s of type %d/%s, %d bytes.\n",
                                segment, 
                                segobj->GetName().c_str(), 
                                segobj->GetSegmentType(),
                                PCIDSK::SegmentTypeName(segobj->GetSegmentType()).c_str(),
                                (int) segobj->GetContentSize() );
                        keys = segobj->GetMetadataKeys();  
                        
                        if( keys.size() > 0 )
                            printf( "  Metadata:\n" );
                        for( i_key = 0; i_key < keys.size(); i_key++ )
                        {
                            printf( "    %s: %s\n", 
                                    keys[i_key].c_str(), 
                                    segobj->GetMetadataValue( keys[i_key].c_str() ).c_str() );
                        }
                    }

                    if( segobj != NULL
                        && segobj->GetSegmentType() == PCIDSK::SEG_VEC 
                        && list_vectors )
                        ReportVectorSegment( segobj );

                    if( segobj != NULL
                        && segobj->GetSegmentType() == PCIDSK::SEG_GEO 
                        && list_geo )
                        ReportGeoSegment( segobj );
                    
                    PCIDSK::PCIDSKRPCSegment *rpcseg = NULL;
                    if ( segobj != NULL &&
                         (rpcseg = dynamic_cast<PCIDSK::PCIDSKRPCSegment*>(segobj)))
                    {
                        ReportRPCSegment(rpcseg);
                    }
                }
                catch( PCIDSK::PCIDSKException ex )
                {
                    fprintf( stderr, "PCIDSKException:\n%s\n", ex.what() );
                    // continue...
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Process the imagery, channel by channel.                        */
/* -------------------------------------------------------------------- */
        if( strategy == "-b" )
        {
            for( channel_index = 1; channel_index <= channel_count; channel_index++ )
            {
                PCIDSK::PCIDSKChannel *channel = file->GetChannel( channel_index );

                assert( channel != NULL );

                int i_block;
                int x_block_count = 
                    (channel->GetWidth() + channel->GetBlockWidth()-1) 
                    / channel->GetBlockWidth();
                int y_block_count = 
                    (channel->GetHeight() + channel->GetBlockHeight()-1) 
                    / channel->GetBlockHeight();
                int block_size = PCIDSK::DataTypeSize(channel->GetType())
                    * channel->GetBlockWidth() 
                    * channel->GetBlockHeight();
                void *block_buffer = malloc( block_size );
            
                int block_count = x_block_count * y_block_count;
            
                printf( "Process %d blocks on channel %d (%s)...",
                        block_count, channel_index,
                        PCIDSK::DataTypeName( channel->GetType()).c_str() );
                fflush(stdout);
            
                for( i_block = 0; i_block < block_count; i_block++ )
                {
                    channel->ReadBlock( i_block, block_buffer );
                    if( fp_raw != NULL )
                        fwrite( block_buffer, 1, block_size, fp_raw );
                }
                printf( "done.\n" );
            
                free( block_buffer );
            }
        }

/* -------------------------------------------------------------------- */
/*      Process the imagery line interleaved.                           */
/* -------------------------------------------------------------------- */
        else if( strategy == "-l" )
        {
            int i_block;
            PCIDSK::PCIDSKChannel *channel = file->GetChannel(1);
            int x_block_count = 
                (channel->GetWidth() + channel->GetBlockWidth()-1) 
                / channel->GetBlockWidth();
            int y_block_count = 
                (channel->GetHeight() + channel->GetBlockHeight()-1) 
                / channel->GetBlockHeight();
            int max_block_size = channel_count * 16 
                * channel->GetBlockWidth() * channel->GetBlockHeight();
            void *block_buffer = malloc( max_block_size );
            int block_count = x_block_count * y_block_count;

            // check for consistency.
            for( channel_index=2; channel_index <= channel_count; channel_index++ )
            {
                PCIDSK::PCIDSKChannel *other_channel = 
                    file->GetChannel( channel_index );
                if( other_channel->GetBlockWidth() != channel->GetBlockWidth()
                    || other_channel->GetBlockHeight() != channel->GetBlockHeight() )
                {
                    fprintf( stderr, 
                             "Channels are not all of matching block size,\n"
                             "interleaved access unavailable.\n" );
                    exit( 1 );
                }
            }

            printf( "Process %d blocks over %d channels...",
                    block_count, channel_count );
            fflush( stdout );

            // actually process imagery.
            for( i_block = 0; i_block < block_count; i_block++ )
            {
                for( channel_index = 1; 
                     channel_index <= channel_count; 
                     channel_index++ )
                {
                    PCIDSK::PCIDSKChannel *channel = file->GetChannel( channel_index );
                    int block_size = PCIDSK::DataTypeSize(channel->GetType())
                        * channel->GetBlockWidth() 
                        * channel->GetBlockHeight();
                
                    channel->ReadBlock( i_block, block_buffer );
                    if( fp_raw != NULL )
                        fwrite( block_buffer, 1, block_size, fp_raw );
                }
            }
        
            printf( "done\n" );

            free( block_buffer );
        }
        
/* -------------------------------------------------------------------- */
/*      Process imagery pixel interleaved.                              */
/* -------------------------------------------------------------------- */
        else if( strategy == "-p" )
        {
            int i_block;
            PCIDSK::PCIDSKChannel *channel = file->GetChannel(1);
            int x_block_count = 
                (channel->GetWidth() + channel->GetBlockWidth()-1) 
                / channel->GetBlockWidth();
            int y_block_count = 
                (channel->GetHeight() + channel->GetBlockHeight()-1) 
                / channel->GetBlockHeight();
            int block_size = (int) file->GetPixelGroupSize() * file->GetWidth();
            int block_count = x_block_count * y_block_count;

            if( file->GetInterleaving() != "PIXEL" )
            {
                fprintf( stderr, 
                         "Pixel Interleaved access only possible on pixel interleaved files.\n" );
                exit( 1 );
            }

            printf( "Process %d blocks over %d channels...",
                    block_count, channel_count );
            fflush( stdout );

            // actually process imagery.
            for( i_block = 0; i_block < block_count; i_block++ )
            {
                void *buffer = file->ReadAndLockBlock( i_block );
                if( fp_raw != NULL )
                    fwrite( buffer, 1, block_size, fp_raw );

                file->UnlockBlock( 0 );
            }
        
            printf( "done\n" );
        }
        
/* -------------------------------------------------------------------- */
/*      Close and cleanup.                                              */
/* -------------------------------------------------------------------- */
        if( fp_raw != NULL )
            fclose( fp_raw );
        delete file;
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
