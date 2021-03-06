/******************************************************************************
 *
 * Purpose:  Implementation of the CTiledChannel class.
 *
 * This class is used to implement band interleaved channels within a
 * PCIDSK file (which are always packed, and FILE interleaved data from
 * external raw files which may not be packed.
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

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_exception.h"
#include "channel/ctiledchannel.h"
#include "segment/systiledir.h"
#include "blockdir/blocktilelayer.h"
#include "core/cpcidskfile.h"
#include "core/cpcidskblockfile.h"
#include "core/pcidsk_raster.h"
#include "core/pcidsk_utils.h"
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace PCIDSK
{

/************************************************************************/
/*                           CTiledChannel()                            */
/************************************************************************/

CTiledChannel::CTiledChannel( PCIDSKBuffer &image_headerIn,
                              uint64 ih_offsetIn,
                              CPL_UNUSED PCIDSKBuffer &file_headerIn,
                              int channelnumIn,
                              CPCIDSKFile *fileIn,
                              eChanType pixel_typeIn )
        : CPCIDSKChannel( image_headerIn, ih_offsetIn, fileIn, pixel_typeIn, channelnumIn)

{
    std::string filename;

    image_headerIn.Get(64,64,filename);

    assert( strstr(filename.c_str(),"SIS=") != nullptr );

    image = atoi(strstr(filename.c_str(),"SIS=") + 4);

    mpoTileLayer = nullptr;
}

/************************************************************************/
/*                           ~CTiledChannel()                           */
/************************************************************************/
CTiledChannel::~CTiledChannel()
{
    try
    {
        Synchronize();
    }
    catch( const PCIDSKException& e )
    {
        fprintf(stderr, "Exception in ~CTiledChannel(): %s", e.what()); // ok
    }
}

/************************************************************************/
/*                          EstablishAccess()                           */
/************************************************************************/
void CTiledChannel::EstablishAccess() const
{
    if (mpoTileLayer)
        return;

    CPCIDSKBlockFile oBlockFile(file);

    SysTileDir * poTileDir = oBlockFile.GetTileDir();

    if (!poTileDir)
        return ThrowPCIDSKException("Unable to find the tile directory segment.");

    mpoTileLayer = poTileDir->GetTileLayer((uint32) image);

    if (!mpoTileLayer)
        return ThrowPCIDSKException("Unable to find the tiled channel: %d", image);

    const char * pszDataType = mpoTileLayer->GetDataType();

    if (GetDataTypeFromName(pszDataType) == CHN_UNKNOWN)
        return ThrowPCIDSKException("Unknown channel type: %s", pszDataType);
}

/************************************************************************/
/*                            Synchronize()                             */
/*                                                                      */
/*      Flush updated blockmap to disk if it is dirty.                  */
/************************************************************************/
void CTiledChannel::Synchronize()
{
    if (mpoTileLayer)
        mpoTileLayer->Sync();
}

/************************************************************************/
/*                                ReadTile()                            */
/************************************************************************/
void CTiledChannel::ReadTile(void * buffer, uint32 nCol, uint32 nRow)
{
    int nTileXSize = (int) mpoTileLayer->GetTileXSize();
    int nTileYSize = (int) mpoTileLayer->GetTileYSize();

    eChanType nDataType = GetType();

    // Check if we can read an sparse tile.
    if (mpoTileLayer->ReadSparseTile(buffer, nCol, nRow))
    {
        // Do byte swapping if needed.
        if( needs_swap )
        {
            SwapPixels( buffer, nDataType, nTileXSize * nTileYSize );
        }

        return;
    }

    const char * compression = mpoTileLayer->GetCompressType();

    if (strcmp(compression, "NONE") == 0)
    {
        mpoTileLayer->ReadTile(buffer, nCol, nRow, mpoTileLayer->GetTileSize());

        // Do byte swapping if needed.
        if( needs_swap )
        {
            SwapPixels( buffer, nDataType, nTileXSize * nTileYSize );
        }

        return;
    }

    uint32 nTileDataSize = mpoTileLayer->GetTileDataSize(nCol, nRow);

    PCIDSKBuffer oCompressedData(nTileDataSize);
    PCIDSKBuffer oUncompressedData(mpoTileLayer->GetTileSize());

    mpoTileLayer->ReadTile(oCompressedData.buffer, nCol, nRow, nTileDataSize);

    if (strcmp(compression, "RLE") == 0)
    {
        RLEDecompressBlock( oCompressedData, oUncompressedData );
    }
    else if (STARTS_WITH(compression, "JPEG"))
    {
        JPEGDecompressBlock( oCompressedData, oUncompressedData );
    }
    else
    {
        return ThrowPCIDSKException(
            "Unable to read tile of unsupported compression type: %s",
            compression);
    }

/* -------------------------------------------------------------------- */
/*      Swap if necessary.  TODO: there is some reason to doubt that    */
/*      the old implementation properly byte swapped compressed         */
/*      data.  Perhaps this should be conditional?                      */
/* -------------------------------------------------------------------- */
    if( needs_swap )
        SwapPixels( oUncompressedData.buffer, nDataType,
                    nTileXSize * nTileYSize );

    memcpy(buffer, oUncompressedData.buffer, oUncompressedData.buffer_size);
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/
int CTiledChannel::ReadBlock( int iBlock, void *buffer,
                              int xoff, int yoff,
                              int xsize, int ysize )
{
    EstablishAccess();

    // Validate the block index.
    int nTileCount = (int) mpoTileLayer->GetTileCount();

    if( iBlock < 0 || iBlock >= nTileCount )
    {
        return ThrowPCIDSKException(0, "Requested non-existent block (%d)",
                              iBlock );
    }

    int nTileXSize = (int) mpoTileLayer->GetTileXSize();
    int nTileYSize = (int) mpoTileLayer->GetTileYSize();

    // Default window.
    if (xoff == -1 && yoff == -1 && xsize == -1 && ysize == -1)
    {
        xoff = 0;
        yoff = 0;
        xsize = nTileXSize;
        ysize = nTileYSize;
    }

    // Validate the requested window.
    if (xoff < 0 || xoff + xsize > nTileXSize ||
        yoff < 0 || yoff + ysize > nTileYSize)
    {
        return ThrowPCIDSKException(0,
            "Invalid window in ReadBlock(): xoff=%d,yoff=%d,xsize=%d,ysize=%d",
            xoff, yoff, xsize, ysize );
    }

    uint32 nTilePerRow = mpoTileLayer->GetTilePerRow();

    if (nTilePerRow == 0)
        return ThrowPCIDSKException(0, "Invalid number of tiles per row.");

    uint32 nCol = iBlock % nTilePerRow;
    uint32 nRow = iBlock / nTilePerRow;

    // Check if the entire tile was requested.
    if (xoff == 0 && xsize == nTileXSize &&
        yoff == 0 && ysize == nTileYSize)
    {
        ReadTile(buffer, nCol, nRow);

        return 1;
    }

    eChanType nDataType = GetType();
    int nPixelSize = DataTypeSize(nDataType);
    int nPixelCount = xsize * ysize;

    // Check if we can read an sparse tile.
    if (!mpoTileLayer->IsTileValid(nCol, nRow))
    {
        if (xoff == 0 && xsize == nTileXSize)
        {
            mpoTileLayer->ReadPartialSparseTile
                (buffer, nCol, nRow,
                 yoff * nTileXSize * nPixelSize,
                 nPixelCount * nPixelSize);
        }
        else
        {
            for (int iy = 0; iy < ysize; iy++)
            {
                mpoTileLayer->ReadPartialSparseTile
                    ((char*) buffer + iy * xsize * nPixelSize, nCol, nRow,
                     ((iy + yoff) * nTileXSize + xoff) * nPixelSize,
                     xsize * nPixelSize);
            }
        }

        // Do byte swapping if needed.
        if( needs_swap )
            SwapPixels( buffer, nDataType, nPixelCount );

        return 1;
    }

    const char * compression = mpoTileLayer->GetCompressType();

    // Read the requested window.
    if (strcmp(compression, "NONE") == 0 && xoff == 0 && xsize == nTileXSize)
    {
        mpoTileLayer->ReadPartialTile(buffer, nCol, nRow,
                                      yoff * nTileXSize * nPixelSize,
                                      nPixelCount * nPixelSize);

        // Do byte swapping if needed.
        if( needs_swap )
            SwapPixels( buffer, nDataType, nPixelCount );
    }
    // Read the requested window line by line.
    else if (strcmp(compression, "NONE") == 0)
    {
        for (int iy = 0; iy < ysize; iy++)
        {
            mpoTileLayer->ReadPartialTile
                ((char*) buffer + iy * xsize * nPixelSize, nCol, nRow,
                 ((iy + yoff) * nTileXSize + xoff) * nPixelSize,
                 xsize * nPixelSize);
        }

        // Do byte swapping if needed.
        if( needs_swap )
            SwapPixels( buffer, nDataType, nPixelCount );
    }
    // Read the entire tile and copy the requested window.
    else
    {
        PCIDSKBuffer oTileData(mpoTileLayer->GetTileSize());

        ReadTile(oTileData.buffer, nCol, nRow);

        for (int iy = 0; iy < ysize; iy++)
        {
            memcpy((char*) buffer + iy * xsize * nPixelSize,
                   oTileData.buffer + ((iy + yoff) * nTileXSize + xoff) * nPixelSize,
                   xsize * nPixelSize);
        }
    }

    return 1;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/
int CTiledChannel::WriteBlock( int iBlock, void *buffer )
{
    if( !file->GetUpdatable() )
        return ThrowPCIDSKException(0, "File not open for update in WriteBlock()" );

    InvalidateOverviews();

    EstablishAccess();

    // Validate the block index.
    int nTileCount = (int) mpoTileLayer->GetTileCount();

    if( iBlock < 0 || iBlock >= nTileCount )
    {
        return ThrowPCIDSKException(0, "Requested non-existent block (%d)",
                              iBlock );
    }

    int nTileXSize = GetBlockWidth();
    int nTileYSize = GetBlockHeight();

    eChanType nDataType = GetType();
    int nPixelCount = nTileXSize * nTileYSize;

    uint32 nTilePerRow = mpoTileLayer->GetTilePerRow();

    if (nTilePerRow == 0)
        return ThrowPCIDSKException(0, "Invalid number of tiles per row.");

    uint32 nCol = iBlock % nTilePerRow;
    uint32 nRow = iBlock / nTilePerRow;

    // Do byte swapping if needed.
    if( needs_swap )
        SwapPixels( buffer, nDataType, nPixelCount );

    // Check if we can write an sparse tile.
    if (mpoTileLayer->WriteSparseTile(buffer, nCol, nRow))
    {
        if( needs_swap )
            SwapPixels( buffer, nDataType, nPixelCount );

        return 1;
    }

    const char * compression = mpoTileLayer->GetCompressType();

/* -------------------------------------------------------------------- */
/*      The simplest case it an uncompressed direct and complete       */
/*      tile read into the destination buffer.                          */
/* -------------------------------------------------------------------- */
    if (strcmp(compression, "NONE") == 0)
    {
        mpoTileLayer->WriteTile(buffer, nCol, nRow);

        if( needs_swap )
            SwapPixels( buffer, nDataType, nPixelCount );

        return 1;
    }

/* -------------------------------------------------------------------- */
/*      Copy the uncompressed data into a PCIDSKBuffer, and byte        */
/*      swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer oUncompressedData(mpoTileLayer->GetTileSize());

    memcpy(oUncompressedData.buffer, buffer,
           oUncompressedData.buffer_size);

    if( needs_swap )
        SwapPixels( buffer, nDataType, nPixelCount );

/* -------------------------------------------------------------------- */
/*      Compress the imagery.                                           */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer oCompressedData;

    if (strcmp(compression, "NONE") == 0)
    {
        oCompressedData = oUncompressedData;
    }
    else if (strcmp(compression, "RLE") == 0)
    {
        RLECompressBlock( oUncompressedData, oCompressedData );
    }
    else if (STARTS_WITH(compression, "JPEG"))
    {
        JPEGCompressBlock( oUncompressedData, oCompressedData );
    }
    else
    {
        return ThrowPCIDSKException(0,
            "Unable to write tile of unsupported compression type: %s",
            compression);
    }

    mpoTileLayer->WriteTile(oCompressedData.buffer, nCol, nRow,
                            oCompressedData.buffer_size);

    return 1;
}

/************************************************************************/
/*                           GetBlockWidth()                            */
/************************************************************************/
int CTiledChannel::GetBlockWidth(void) const
{
    EstablishAccess();

    return (int) mpoTileLayer->GetTileXSize();
}

/************************************************************************/
/*                           GetBlockHeight()                           */
/************************************************************************/
int CTiledChannel::GetBlockHeight(void) const
{
    EstablishAccess();

    return (int) mpoTileLayer->GetTileYSize();
}

/************************************************************************/
/*                              GetWidth()                              */
/************************************************************************/
int CTiledChannel::GetWidth(void) const
{
    EstablishAccess();

    return (int) mpoTileLayer->GetXSize();
}

/************************************************************************/
/*                             GetHeight()                              */
/************************************************************************/
int CTiledChannel::GetHeight(void) const
{
    EstablishAccess();

    return (int) mpoTileLayer->GetYSize();
}

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/
eChanType CTiledChannel::GetType(void) const
{
    eChanType nDataType = CPCIDSKChannel::GetType();

    if (nDataType != CHN_UNKNOWN)
        return nDataType;

    EstablishAccess();

    return GetDataTypeFromName(mpoTileLayer->GetDataType());
}

/************************************************************************/
/*                         RLEDecompressBlock()                         */
/************************************************************************/

void CTiledChannel::RLEDecompressBlock( PCIDSKBuffer &oCompressedData,
                                        PCIDSKBuffer &oDecompressedData )


{
    int    src_offset=0, dst_offset=0;
    uint8  *src = (uint8 *) oCompressedData.buffer;
    uint8  *dst = (uint8 *) oDecompressedData.buffer;
    int    nPixelSize = DataTypeSize(GetType());

/* -------------------------------------------------------------------- */
/*      Process till we are out of source data, or our destination      */
/*      buffer is full.  These conditions should be satisfied at       */
/*      the same time!                                                  */
/* -------------------------------------------------------------------- */
    while( src_offset + 1 + nPixelSize <= oCompressedData.buffer_size
           && dst_offset < oDecompressedData.buffer_size )
    {
/* -------------------------------------------------------------------- */
/*      Extract a repeat run                                            */
/* -------------------------------------------------------------------- */
        if( src[src_offset] > 127 )
        {
            int count = src[src_offset++] - 128;
            int i;

            if( dst_offset + count * nPixelSize > oDecompressedData.buffer_size)
            {
                return ThrowPCIDSKException( "RLE compressed tile corrupt, overrun avoided." );
            }

            while( count-- > 0 )
            {
                for( i = 0; i < nPixelSize; i++ )
                    dst[dst_offset++] = src[src_offset+i];
            }
            src_offset += nPixelSize;
        }

/* -------------------------------------------------------------------- */
/*      Extract a literal run.                                          */
/* -------------------------------------------------------------------- */
        else
        {
            int count = src[src_offset++];

            if( dst_offset + count*nPixelSize > oDecompressedData.buffer_size
                || src_offset + count*nPixelSize > oCompressedData.buffer_size)
            {
                return ThrowPCIDSKException( "RLE compressed tile corrupt, overrun avoided." );
            }

            memcpy( dst + dst_offset, src + src_offset,
                    nPixelSize * count );
            src_offset += nPixelSize * count;
            dst_offset += nPixelSize * count;
        }

    }

/* -------------------------------------------------------------------- */
/*      Final validation.                                               */
/* -------------------------------------------------------------------- */
    if( src_offset != oCompressedData.buffer_size
        || dst_offset != oDecompressedData.buffer_size )
    {
        return ThrowPCIDSKException( "RLE compressed tile corrupt, result incomplete." );
    }
}

/************************************************************************/
/*                         RLECompressBlock()                           */
/*                                                                      */
/*      TODO: There does not seem to be any byte order logic in here!   */
/************************************************************************/

void CTiledChannel::RLECompressBlock( PCIDSKBuffer &oUncompressedData,
                                      PCIDSKBuffer &oCompressedData )


{
    int    src_bytes = oUncompressedData.buffer_size;
    int    nPixelSize = DataTypeSize(GetType());
    int    src_offset = 0, dst_offset = 0;
    int    i;
    uint8  *src = (uint8 *) oUncompressedData.buffer;

/* -------------------------------------------------------------------- */
/*      Loop till input exhausted.                                       */
/* -------------------------------------------------------------------- */
    while( src_offset < src_bytes )
    {
        bool    bGotARun = false;

/* -------------------------------------------------------------------- */
/*      Establish the run length, and emit if greater than 3.           */
/* -------------------------------------------------------------------- */
        if( src_offset + 3*nPixelSize < src_bytes )
        {
            int         count = 1;

            while( count < 127
                   && src_offset + count*nPixelSize < src_bytes )
            {
                bool    bWordMatch = true;

                for( i = 0; i < nPixelSize; i++ )
                {
                    if( src[src_offset+i]
                        != src[src_offset+i+count*nPixelSize] )
                        bWordMatch = false;
                }

                if( !bWordMatch )
                    break;

                count++;
            }

            if( count >= 3 )
            {
                if( oCompressedData.buffer_size < dst_offset + nPixelSize+1 )
                    oCompressedData.SetSize( oCompressedData.buffer_size*2+100);

                oCompressedData.buffer[dst_offset++] = (char) (count+128);

                for( i = 0; i < nPixelSize; i++ )
                    oCompressedData.buffer[dst_offset++] = src[src_offset+i];

                src_offset += count * nPixelSize;

                bGotARun = true;
            }
            else
                bGotARun = false;
        }

/* -------------------------------------------------------------------- */
/*      Otherwise emit a literal till we encounter at least a three     */
/*      word series.                                                    */
/* -------------------------------------------------------------------- */
        if( !bGotARun )
        {
            int         count = 1;
            int         match_count = 0;

            while( count < 127
                   && src_offset + count*nPixelSize < src_bytes )
            {
                bool    bWordMatch = true;

                for( i = 0; i < nPixelSize; i++ )
                {
                    if( src[src_offset+i]
                        != src[src_offset+i+count*nPixelSize] )
                        bWordMatch = false;
                }

                if( bWordMatch )
                    match_count++;
                else
                    match_count = 0;

                if( match_count > 2 )
                    break;

                count++;
            }

            assert( src_offset + count*nPixelSize <= src_bytes );

            while( oCompressedData.buffer_size
                   < dst_offset + count*nPixelSize+1 )
                oCompressedData.SetSize( oCompressedData.buffer_size*2+100 );

            oCompressedData.buffer[dst_offset++] = (char) count;
            memcpy( oCompressedData.buffer + dst_offset,
                    src + src_offset,
                    count * nPixelSize );
            src_offset += count * nPixelSize;
            dst_offset += count * nPixelSize;
        }
    }

    oCompressedData.buffer_size = dst_offset;
}

/************************************************************************/
/*                        JPEGDecompressBlock()                         */
/************************************************************************/

void CTiledChannel::JPEGDecompressBlock( PCIDSKBuffer &oCompressedData,
                                         PCIDSKBuffer &oDecompressedData )


{
    if( file->GetInterfaces()->JPEGDecompressBlock == nullptr )
        return ThrowPCIDSKException( "JPEG decompression not enabled in the PCIDSKInterfaces of this build." );

    file->GetInterfaces()->JPEGDecompressBlock(
        (uint8 *) oCompressedData.buffer, oCompressedData.buffer_size,
        (uint8 *) oDecompressedData.buffer, oDecompressedData.buffer_size,
        GetBlockWidth(), GetBlockHeight(), GetType() );
}

/************************************************************************/
/*                         JPEGCompressBlock()                          */
/************************************************************************/

void CTiledChannel::JPEGCompressBlock( PCIDSKBuffer &oDecompressedData,
                                       PCIDSKBuffer &oCompressedData )



{
    if( file->GetInterfaces()->JPEGCompressBlock == nullptr )
        return ThrowPCIDSKException( "JPEG compression not enabled in the PCIDSKInterfaces of this build." );

/* -------------------------------------------------------------------- */
/*      What quality should we be using?                                */
/* -------------------------------------------------------------------- */
    int quality = 75;

    const char * compression = mpoTileLayer->GetCompressType();

    if (strlen(compression) > 4 && isdigit(compression[4]))
        quality = atoi(compression + 4);

/* -------------------------------------------------------------------- */
/*      Make the output buffer plenty big to hold any conceivable        */
/*      result.                                                         */
/* -------------------------------------------------------------------- */
    oCompressedData.SetSize( oDecompressedData.buffer_size * 2 + 1000 );

/* -------------------------------------------------------------------- */
/*      invoke.                                                         */
/* -------------------------------------------------------------------- */
    file->GetInterfaces()->JPEGCompressBlock(
        (uint8 *) oDecompressedData.buffer, oDecompressedData.buffer_size,
        (uint8 *) oCompressedData.buffer, oCompressedData.buffer_size,
        GetBlockWidth(), GetBlockHeight(), GetType(), quality );
}

} // namespace PCIDSK;
