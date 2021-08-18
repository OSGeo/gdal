/******************************************************************************
 *
 * Purpose:  Implementation of the Create() function to create new PCIDSK files.
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
#include "pcidsk.h"
#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_exception.h"
#include "pcidsk_file.h"
#include "pcidsk_georef.h"
#include "core/pcidsk_utils.h"
#include "core/cpcidskblockfile.h"
#include "core/clinksegment.h"
#include "segment/systiledir.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <memory>

using namespace PCIDSK;

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

/**
 * Create a PCIDSK (.pix) file.
 *
 * @param filename the name of the PCIDSK file to create.
 * @param pixels the width of the new file in pixels.
 * @param lines the height of the new file in scanlines.
 * @param channel_count the number of channels to create.
 * @param channel_types an array of types for all the channels, or NULL for
 * all CHN_8U channels.
 * @param options creation options (interleaving, etc)
 * @param interfaces Either NULL to use default interfaces, or a pointer
 * to a populated interfaces object.
 *
 * @return a pointer to a file object for accessing the PCIDSK file.
 */

PCIDSKFile PCIDSK_DLL *
PCIDSK::Create( std::string filename, int pixels, int lines,
                int channel_count, eChanType *channel_types,
                std::string options, const PCIDSKInterfaces *interfaces )

{
    if( pixels < 0 || pixels > 99999999 ||
        lines < 0 || lines > 99999999 ||
        channel_count < 0 || channel_count > 99999999 )
    {
        return (PCIDSKFile*)ThrowPCIDSKExceptionPtr(
            "PCIDSK::Create(): invalid dimensions / band count." );
    }

/* -------------------------------------------------------------------- */
/*      Use default interfaces if none are passed in.                   */
/* -------------------------------------------------------------------- */
    PCIDSKInterfaces default_interfaces;
    if( interfaces == nullptr )
        interfaces = &default_interfaces;

/* -------------------------------------------------------------------- */
/*      Default the channel types to all 8U if not provided.            */
/* -------------------------------------------------------------------- */
    std::vector<eChanType> default_channel_types;

    if( channel_types == nullptr )
    {
        default_channel_types.resize( channel_count+1, CHN_8U );
        channel_types = &(default_channel_types[0]);
    }

/* -------------------------------------------------------------------- */
/*      Validate parameters.                                            */
/* -------------------------------------------------------------------- */
    const char *interleaving = nullptr;
    std::string compression = "NONE";
    bool nocreate = false;
    int  tilesize = PCIDSK_DEFAULT_TILE_SIZE;
    std::string oLinkFilename;

    std::string oOrigOptions = options;
    UCaseStr( options );
    for(auto & c : options)
    {
        if(c == ',')
            c = ' ';
    }

    //The code down below assumes that the interleaving
    //will be first in the string, so let's make sure
    //that that is true
    auto apszInterleavingOptions =
        {"FILE", "PIXEL", "BAND", "TILED", "NOZERO"};
    for(auto pszInterleavingOption : apszInterleavingOptions)
    {
        std::size_t nPos = options.find(pszInterleavingOption);
        if(nPos > 0 && nPos < options.size())
        {
            std::size_t nInterleavingStart = nPos;
            //Some options are more than just this string
            //so we cannot take strlen(pszInterleavingOption) as the
            //endpoint of the option.
            std::size_t nInterleavingEnd = options.find(" ", nPos);
            if(nInterleavingEnd == std::string::npos)
                nInterleavingEnd = options.size();
            std::string sSubstring =
                options.substr(nInterleavingStart,
                               nInterleavingEnd - nInterleavingStart);
            options.erase(options.begin() + nInterleavingStart,
                          options.begin() + nInterleavingEnd);
            options = std::move(sSubstring) + ' ' + std::move(options);
            break;
        }
        else if(nPos == 0)
            break;
    }

    if(STARTS_WITH(options.c_str(), "PIXEL") )
        interleaving = "PIXEL";
    else if( STARTS_WITH(options.c_str(), "BAND") )
        interleaving = "BAND";
    else if( STARTS_WITH(options.c_str(), "TILED") )
    {
        interleaving = "FILE";
        ParseTileFormat( options, tilesize, compression );
    }
    else if(STARTS_WITH(options.c_str(), "NOZERO"))
    {
        interleaving = "BAND";
    }
    else if( STARTS_WITH(options.c_str(), "FILE") )
    {
        if( STARTS_WITH(options.c_str(), "FILENOCREATE") )
        {
            nocreate = true;
            oLinkFilename = ParseLinkedFilename(oOrigOptions);
        }
        interleaving = "FILE";
    }
    else
        return (PCIDSKFile*)ThrowPCIDSKExceptionPtr( "PCIDSK::Create() options '%s' not recognised.",
                              options.c_str() );

    bool nozero = options.find("NOZERO") != std::string::npos;

/* -------------------------------------------------------------------- */
/*      Validate the channel types.                                     */
/* -------------------------------------------------------------------- */
    int16 channels[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    int chan_index;
    bool regular = true;

    for( chan_index=0; chan_index < channel_count; chan_index++ )
    {
        if( chan_index > 0
            && ((int) channel_types[chan_index])
                < ((int) channel_types[chan_index-1]) )
            regular = false;

        channels[((int) channel_types[chan_index])]++;
    }

    if( !regular && strcmp(interleaving,"FILE") != 0 )
    {
        return (PCIDSKFile*)ThrowPCIDSKExceptionPtr(
           "The order of the channel types is not valid for interleaving=%s. "
           "Channels must be packed in the following order: "
           "8U, 16S, 16U, 32S, 32U, 32R, 64S, 64U, 64R, C16S, C16U, C32S, C32U, C32R",
           interleaving);
    }

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    void *io_handle = interfaces->io->Open( filename, "w+" );

    assert( io_handle != nullptr );

/* -------------------------------------------------------------------- */
/*      Use the following structure instead of a try / catch.           */
/* -------------------------------------------------------------------- */
    struct IOHandleAutoPtr
    {
        const PCIDSKInterfaces * interfaces;
        void * io_handle;

        IOHandleAutoPtr(const PCIDSKInterfaces * interfacesIn,
                        void * io_handleIn)
            : interfaces(interfacesIn),
              io_handle(io_handleIn)
        {
        }
        ~IOHandleAutoPtr()
        {
            Close();
        }
        void Close()
        {
            if (interfaces && io_handle)
                interfaces->io->Close(io_handle);

            io_handle = nullptr;
        }
    };

    IOHandleAutoPtr oHandleAutoPtr(interfaces, io_handle);

/* ==================================================================== */
/*      Establish some key file layout information.                     */
/* ==================================================================== */
    int image_header_start = 1;                    // in blocks
    uint64 image_data_start, image_data_size=0;    // in blocks
    uint64 segment_ptr_start, segment_ptr_size=64; // in blocks
    int pixel_group_size, line_size;               // in bytes
    int image_header_count = channel_count;

/* -------------------------------------------------------------------- */
/*      Pixel interleaved.                                              */
/* -------------------------------------------------------------------- */
    if( strcmp(interleaving,"PIXEL") == 0 )
    {
        pixel_group_size =
            channels[CHN_8U]  * DataTypeSize(CHN_8U) +
            channels[CHN_16S] * DataTypeSize(CHN_16S) +
            channels[CHN_16U] * DataTypeSize(CHN_16U) +
            channels[CHN_32S] * DataTypeSize(CHN_32S) +
            channels[CHN_32U] * DataTypeSize(CHN_32U) +
            channels[CHN_32R] * DataTypeSize(CHN_32R) +
            channels[CHN_64S] * DataTypeSize(CHN_64S) +
            channels[CHN_64U] * DataTypeSize(CHN_64U) +
            channels[CHN_64R] * DataTypeSize(CHN_64R) +
            channels[CHN_C16S] * DataTypeSize(CHN_C16S) +
            channels[CHN_C16U] * DataTypeSize(CHN_C16U) +
            channels[CHN_C32S] * DataTypeSize(CHN_C32S) +
            channels[CHN_C32U] * DataTypeSize(CHN_C32U) +
            channels[CHN_C32R] * DataTypeSize(CHN_C32R);
        line_size = ((pixel_group_size * pixels + 511) / 512) * 512;
        image_data_size = (((uint64)line_size) * lines) / 512;

        // TODO: Old code enforces a 1TB limit for some reason.
    }

/* -------------------------------------------------------------------- */
/*      Band interleaved.                                               */
/* -------------------------------------------------------------------- */
    else if( strcmp(interleaving,"BAND") == 0 )
    {
        pixel_group_size =
            channels[CHN_8U]  * DataTypeSize(CHN_8U) +
            channels[CHN_16S] * DataTypeSize(CHN_16S) +
            channels[CHN_16U] * DataTypeSize(CHN_16U) +
            channels[CHN_32S] * DataTypeSize(CHN_32S) +
            channels[CHN_32U] * DataTypeSize(CHN_32U) +
            channels[CHN_32R] * DataTypeSize(CHN_32R) +
            channels[CHN_64S] * DataTypeSize(CHN_64S) +
            channels[CHN_64U] * DataTypeSize(CHN_64U) +
            channels[CHN_64R] * DataTypeSize(CHN_64R) +
            channels[CHN_C16S] * DataTypeSize(CHN_C16S) +
            channels[CHN_C16U] * DataTypeSize(CHN_C16U) +
            channels[CHN_C32S] * DataTypeSize(CHN_C32S) +
            channels[CHN_C32U] * DataTypeSize(CHN_C32U) +
            channels[CHN_C32R] * DataTypeSize(CHN_C32R);
        // BAND interleaved bands are tightly packed.
        image_data_size =
            (((uint64)pixel_group_size) * pixels * lines + 511) / 512;

        // TODO: Old code enforces a 1TB limit for some reason.
    }

/* -------------------------------------------------------------------- */
/*      FILE/Tiled.                                                     */
/* -------------------------------------------------------------------- */
    else if( strcmp(interleaving,"FILE") == 0 )
    {
        // For some reason we reserve extra space, but only for FILE.
        if( channel_count < 64 )
            image_header_count = 64;

        image_data_size = 0;

        // TODO: Old code enforces a 1TB limit on the fattest band.
    }

/* -------------------------------------------------------------------- */
/*      Place components.                                               */
/* -------------------------------------------------------------------- */
    segment_ptr_start = image_header_start + image_header_count*2;
    image_data_start = segment_ptr_start + segment_ptr_size;

/* ==================================================================== */
/*      Prepare the file header.                                        */
/* ==================================================================== */
    PCIDSKBuffer fh(512);

    char current_time[17];
    GetCurrentDateTime( current_time );

    // Initialize everything to spaces.
    fh.Put( "", 0, 512 );

/* -------------------------------------------------------------------- */
/*      File Type, Version, and Size                                    */
/*      Notice: we get the first 4 characters from PCIVERSIONAME.       */
/* -------------------------------------------------------------------- */
    // FH1 - magic format string.
    fh.Put( "PCIDSK", 0, 8 );

    // FH2 - TODO: Allow caller to pass this in.
    fh.Put( "SDK V1.0", 8, 8 );

    // FH3 - file size later.
    fh.Put( (image_data_start + image_data_size), 16, 16 );

    // FH4 - 16 characters reserved - spaces.

    // FH5 - Description
    fh.Put( filename.c_str(), 48, 64 );

    // FH6 - Facility
    fh.Put( "PCI Geomatics, Markham, Canada.", 112, 32 );

    // FH7.1 / FH7.2 - left blank (64+64 bytes @ 144)

    // FH8 Creation date/time
    fh.Put( current_time, 272, 16 );

    // FH9 Update date/time
    fh.Put( current_time, 288, 16 );

/* -------------------------------------------------------------------- */
/*      Image Data                                                      */
/* -------------------------------------------------------------------- */
    // FH10 - start block of image data
    fh.Put( image_data_start+1, 304, 16 );

    // FH11 - number of blocks of image data.
    fh.Put( image_data_size, 320, 16 );

    // FH12 - start block of image headers.
    fh.Put( image_header_start+1, 336, 16 );

    // FH13 - number of blocks of image headers.
    fh.Put( image_header_count*2, 352, 8);

    // FH14 - interleaving.
    fh.Put( interleaving, 360, 8);

    // FH15 - reserved - MIXED is for some ancient backwards compatibility.
    fh.Put( "MIXED", 368, 8);

    // FH16 - number of image bands.
    fh.Put( channel_count, 376, 8 );

    // FH17 - width of image in pixels.
    fh.Put( pixels, 384, 8 );

    // FH18 - height of image in pixels.
    fh.Put( lines, 392, 8 );

    // FH19 - pixel ground size interpretation.
    fh.Put( "METRE", 400, 8 );

    // TODO:
    //PrintDouble( fh->XPixelSize, "%16.9f", 1.0 );
    //PrintDouble( fh->YPixelSize, "%16.9f", 1.0 );
    fh.Put( "1.0", 408, 16 );
    fh.Put( "1.0", 424, 16 );

/* -------------------------------------------------------------------- */
/*      Segment Pointers                                                */
/* -------------------------------------------------------------------- */
    // FH22 - start block of segment pointers.
    fh.Put( segment_ptr_start+1, 440, 16 );

    // fH23 - number of blocks of segment pointers.
    fh.Put( segment_ptr_size, 456, 8 );

/* -------------------------------------------------------------------- */
/*      Number of different types of Channels                           */
/* -------------------------------------------------------------------- */
    // FH24.1 - 8U bands.
    fh.Put( channels[CHN_8U], 464, 4 );

    // FH24.2 - 16S bands.
    fh.Put( channels[CHN_16S], 468, 4 );

    // FH24.3 - 16U bands.
    fh.Put( channels[CHN_16U], 472, 4 );

    // FH24.4 - 32R bands.
    fh.Put( channels[CHN_32R], 476, 4 );

    // FH24.5 - C16U bands
    fh.Put( channels[CHN_C16U], 480, 4 );

    // FH24.6 - C16S bands
    fh.Put( channels[CHN_C16S], 484, 4 );

    // FH24.7 - C32R bands
    fh.Put( channels[CHN_C32R], 488, 4 );

    if (!BigEndianSystem())
    {
        SwapData(channels + CHN_32S, 2, 1);
        SwapData(channels + CHN_32U, 2, 1);
        SwapData(channels + CHN_64S, 2, 1);
        SwapData(channels + CHN_64U, 2, 1);
        SwapData(channels + CHN_64R, 2, 1);
        SwapData(channels + CHN_C32S, 2, 1);
        SwapData(channels + CHN_C32U, 2, 1);
    }

    fh.PutBin(channels[CHN_32S], 492);
    fh.PutBin(channels[CHN_32U], 494);
    fh.PutBin(channels[CHN_64S], 496);
    fh.PutBin(channels[CHN_64U], 498);
    fh.PutBin(channels[CHN_64R], 500);
    fh.PutBin(channels[CHN_C32S], 502);
    fh.PutBin(channels[CHN_C32U], 504);

/* -------------------------------------------------------------------- */
/*      Write out the file header.                                      */
/* -------------------------------------------------------------------- */
    interfaces->io->Write( fh.buffer, 512, 1, io_handle );

/* ==================================================================== */
/*      Write out the image headers.                                    */
/* ==================================================================== */
    PCIDSKBuffer ih( 1024 );

    ih.Put( " ", 0, 1024 );

    // IHi.1 - Text describing Channel Contents
    ih.Put( "Contents Not Specified", 0, 64 );

    // IHi.2 - Filename storing image.
    if( STARTS_WITH(interleaving, "FILE") )
        ih.Put( "<uninitialized>", 64, 64 );

    // IHi.3 - Creation time and date.
    ih.Put( current_time, 128, 16 );

    // IHi.4 - Creation time and date.
    ih.Put( current_time, 144, 16 );

    interfaces->io->Seek( io_handle, image_header_start*512, SEEK_SET );

    for( chan_index = 0; chan_index < channel_count; chan_index++ )
    {
        ih.Put(DataTypeName(channel_types[chan_index]), 160, 8);

        if( STARTS_WITH(options.c_str(), "TILED") )
        {
            char sis_filename[65];
            snprintf( sis_filename, sizeof(sis_filename), "/SIS=%d", chan_index );
            ih.Put( sis_filename, 64, 64 );

            // IHi.6.7 - IHi.6.10
            ih.Put( 0, 250, 8 );
            ih.Put( 0, 258, 8 );
            ih.Put( pixels, 266, 8 );
            ih.Put( lines, 274, 8 );

            // IHi.6.11
            ih.Put( 1, 282, 8 );
        }
        else if( nocreate )
        {
            std::string oName(64, ' ');

            if( oLinkFilename.size() <= 64 )
            {
                std::stringstream oSS(oName);
                oSS << oLinkFilename;
                oName = oSS.str();
            }

            ih.Put( oName.c_str(), 64, 64 );

            // IHi.6.7 - IHi.6.10
            ih.Put( 0, 250, 8 );
            ih.Put( 0, 258, 8 );
            ih.Put( pixels, 266, 8 );
            ih.Put( lines, 274, 8 );

            // IHi.6.11
            ih.Put( chan_index+1, 282, 8 );
        }

        interfaces->io->Write( ih.buffer, 1024, 1, io_handle );
    }

    for( chan_index = channel_count;
         chan_index < image_header_count;
         chan_index++ )
    {
        ih.Put( "", 160, 8 );
        ih.Put( "<uninitialized>", 64, 64 );
        ih.Put( "", 250, 40 );

        interfaces->io->Write( ih.buffer, 1024, 1, io_handle );
    }

/* ==================================================================== */
/*      Write out the segment pointers, all spaces.                     */
/* ==================================================================== */
    PCIDSKBuffer segment_pointers( (int) (segment_ptr_size*512) );
    segment_pointers.Put( " ", 0, (int) (segment_ptr_size*512) );

    interfaces->io->Seek( io_handle, segment_ptr_start*512, SEEK_SET );
    interfaces->io->Write( segment_pointers.buffer, segment_ptr_size, 512,
                           io_handle );

/* -------------------------------------------------------------------- */
/*      Ensure we write out something at the end of the image data      */
/*      to force the file size.                                         */
/* -------------------------------------------------------------------- */
    if( image_data_size > 0 && !nozero)
    {
/* -------------------------------------------------------------------- */
/*      This prezero operation using the Win32 API is slow. Doing it    */
/*      ourselves allow the creation to be 50% faster.                  */
/* -------------------------------------------------------------------- */
        size_t nBufSize = 524288; // Number of 512 blocks for 256MB
        std::unique_ptr<char[]> oZeroAutoPtr(new char[nBufSize*512]);
        char* puBuf = oZeroAutoPtr.get();
        std::memset(puBuf,0,nBufSize*512); //prezero

        uint64 nBlocksRest = image_data_size;
        uint64 nOff = image_data_start;
        while(nBlocksRest > 0)
        {
            size_t nWriteBlocks = nBufSize;
            if(nBlocksRest < nBufSize)
            {
                nWriteBlocks = static_cast<size_t>(nBlocksRest);
                nBlocksRest = 0;
            }
            interfaces->io->Seek( io_handle, nOff*512, SEEK_SET );
            interfaces->io->Write( puBuf, nWriteBlocks, 512, io_handle );

            nOff+=nWriteBlocks;
            if(nBlocksRest != 0)
            {
                nBlocksRest -= nWriteBlocks;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Close the raw file, and reopen as a pcidsk file.                */
/* -------------------------------------------------------------------- */
    oHandleAutoPtr.Close();

    PCIDSKFile *file = Open( filename, "r+", interfaces );

    std::unique_ptr<PCIDSKFile> oFileAutoPtr(file);

    if(oLinkFilename.size() > 64)
    {
        int nSegId = file->CreateSegment( "Link", "Sys_Link", SEG_SYS, 1);

        CLinkSegment * poSeg =
            dynamic_cast<CLinkSegment*>(file->GetSegment(nSegId));

        if(nullptr != poSeg)
        {
            poSeg->SetPath(oLinkFilename);
            poSeg->Synchronize();
        }

        for( chan_index = 0; chan_index < channel_count; chan_index++ )
        {
            uint64 ih_offset = (uint64)image_header_start*512 + (uint64)chan_index*1024;

            file->ReadFromFile( ih.buffer, ih_offset, 1024 );

            std::string oName(64, ' ');
            std::stringstream oSS(oName);
            oSS << "LNK=";
            oSS << std::setw(4) << nSegId;
            oSS << "File requires a newer PCIDSK file reader to read";
            oName = oSS.str();

            ih.Put( oName.c_str(), 64, 64 );

            file->WriteToFile( ih.buffer, ih_offset, 1024 );
        }

        oFileAutoPtr.reset(nullptr);
        file = Open( filename, "r+", interfaces );
        oFileAutoPtr.reset(file);
    }

/* -------------------------------------------------------------------- */
/*      Create a default georeferencing segment.                        */
/* -------------------------------------------------------------------- */
    file->CreateSegment( "GEOref",
                         "Master Georeferencing Segment for File",
                         SEG_GEO, 6 );

/* -------------------------------------------------------------------- */
/*      If the dataset is tiled, create the file band data.             */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH(options.c_str(), "TILED") )
    {
        file->SetMetadataValue( "_DBLayout", options );

        CPCIDSKBlockFile oBlockFile(file);

        SysTileDir * poTileDir = oBlockFile.CreateTileDir();

        for( chan_index = 0; chan_index < channel_count; chan_index++ )
        {
            poTileDir->CreateTileLayer(pixels, lines, tilesize, tilesize,
                                       channel_types[chan_index],
                                       compression);
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have a non-tiled FILE interleaved file, should we         */
/*      create external band files now?                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH(interleaving, "FILE")
        && !STARTS_WITH(options.c_str(), "TILED")
        && !nocreate )
    {
        for( chan_index = 0; chan_index < channel_count; chan_index++ )
        {
            PCIDSKChannel *channel = file->GetChannel( chan_index + 1 );
            int pixel_size = DataTypeSize(channel->GetType());

            // build a band filename that uses the basename of the PCIDSK
            // file, and adds ".nnn" based on the band.
            std::string band_filename = filename;
            char ext[16];
            CPLsnprintf( ext, sizeof(ext), ".%03d", chan_index+1 );

            size_t last_dot = band_filename.find_last_of(".");
            if( last_dot != std::string::npos
                && (band_filename.find_last_of("/\\:") == std::string::npos
                    || band_filename.find_last_of("/\\:") < last_dot) )
            {
                band_filename.resize( last_dot );
            }

            band_filename += ext;

            // Now build a version without a path.
            std::string relative_band_filename;
            size_t path_div = band_filename.find_last_of( "/\\:" );
            if( path_div == std::string::npos )
                relative_band_filename = band_filename;
            else
                relative_band_filename = band_filename.c_str() + path_div + 1;

            // create the file - ought we write the whole file?
            void *band_io_handle = interfaces->io->Open( band_filename, "w" );
            interfaces->io->Write( "\0", 1, 1, band_io_handle );
            interfaces->io->Close( band_io_handle );

            // Set the channel header information.
            channel->SetChanInfo( relative_band_filename, 0, pixel_size,
                                  pixel_size * pixels, true );
        }
    }

    oFileAutoPtr.release();

    return file;
}
