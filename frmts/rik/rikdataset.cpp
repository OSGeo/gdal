/******************************************************************************
 * $Id$
 *
 * Project:  RIK Reader
 * Purpose:  All code for RIK Reader
 * Author:   Daniel Wallner, daniel.wallner@bredband.net
 *
 ******************************************************************************
 * Copyright (c) 2005, Daniel Wallner <daniel.wallner@bredband.net>
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2005/08/17 15:44:23  fwarmerdam
 * patch up for win32/vc6 compatibility
 *
 * Revision 1.1  2005/08/17 07:20:55  dwallner
 * Import
 *
 *
 */

#include <float.h>
#include "gdal_pam.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_RIK(void);
CPL_C_END

#define RIK_HEADER_DEBUG 0
#define RIK_RESTART_DEBUG 0
#define RIK_PIXEL_DEBUG 0

//#define RIK_SINGLE_BLOCK 0

#define RIK_ALLOW_BLOCK_ERRORS 1

//
// The RIK file format information was extracted from the trikpanel project:
// http://sourceforge.net/projects/trikpanel/
//
// There is currently no public information on RIK version 3 available.
// The information below only applies to formats prior to version 3.
//
// A RIK file consists of the following elements:
//
// +--------------------+
// | Map name           | (The first two bytes is the string length.)
// +--------------------+
// | Header             | (Variable length.)
// +--------------------+
// | Color palette      |
// +--------------------+
// | Block offset array | (Only in compressed formats)
// +--------------------+
// | Image blocks       |
// +--------------------+
//
// All numbers are stored in little endian.
//
// There are three different image block formats:
//
// 1. Uncompressed image block
//
//   A stream of palette indexes.
//
// 2. RLE image block
//
//   The RLE image block is a stream of byte pairs:
//   |  Run length - 1 (byte)  |  Pixel value (byte)  |  Run length - 1 ...
//
// 3. LZW image block
//
//   The LZW image block uses the same LZW encoding as a GIF file
//   except that there is no EOF code and maximum code length is 13 bits.
//   The block starts with 5 unknown bytes and each restart code
//   is followed by an unknown number of unknown bytes.
//   The LZW block read function handles the unknown bytes by
//   restarting with different settings when an error has ocurred.
//   These blocks are upside down compared to RLE blocks (and GDAL).

typedef struct
{
    GUInt16     iUnknown;
    double      fSouth;         // Map bounds
    double      fWest;
    double      fNorth;
    double      fEast;
    GUInt32     iScale;         // Source map scale
    float       iMPPNum;        // Meters per pixel numerator
    GUInt32     iMPPDen;        // Meters per pixel denominator
                                // Only used if fSouth < 4000000
    GUInt32     iBlockWidth;
    GUInt32     iBlockHeight;
    GUInt32     iHorBlocks;     // Number of horizontal blocks
    GUInt32     iVertBlocks;    // Number of vertical blocks
                                // Only used if fSouth >= 4000000
    GByte       iBitsPerPixel;
    GByte       iOptions;
} RIKHeader;

/************************************************************************/
/* ==================================================================== */
/*				RIKDataset				*/
/* ==================================================================== */
/************************************************************************/

class RIKRasterBand;

class RIKDataset : public GDALPamDataset
{
    friend class RIKRasterBand;

    FILE        *fp;

    double      fTransform[6];

    GUInt32     nBlockXSize;
    GUInt32     nBlockYSize;
    GUInt32     nHorBlocks;
    GUInt32     nVertBlocks;
    GUInt32     nFileSize;
    GUInt32     *pOffsets;
    GByte       options;

    GDALColorTable *poColorTable;

  public:
    ~RIKDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            RIKRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class RIKRasterBand : public GDALPamRasterBand
{
    friend class RIKDataset;

  public:

    RIKRasterBand( RIKDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};

/************************************************************************/
/*                           RIKRasterBand()                            */
/************************************************************************/

RIKRasterBand::RIKRasterBand( RIKDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->nBlockXSize;
    nBlockYSize = poDS->nBlockYSize;
}

/************************************************************************/
/*                             GetNextLZWCode()                         */
/************************************************************************/

static int GetNextLZWCode( int codeBits,
                           GByte *blockData,
                           GUInt32 &filePos,
                           int &bitsTaken )

{
    const int BitMask[] = {
        0x0000, 0x0001, 0x0003, 0x0007,
        0x000f, 0x001f, 0x003f, 0x007f,
        0x00ff, 0x01ff, 0x03ff, 0x07ff,
        0x0fff
    };

    int ret = 0;
    int bitsLeftToGo = codeBits;

    while( bitsLeftToGo > 0 )
    {
        int tmp;

        tmp = blockData[filePos];
        tmp = tmp >> bitsTaken;

        if( bitsLeftToGo < 8 )
            tmp &= BitMask[bitsLeftToGo];

        tmp = tmp << (codeBits - bitsLeftToGo);

        ret |= tmp;

        bitsLeftToGo -= (8 - bitsTaken);
        bitsTaken = 0;

        if( bitsLeftToGo < 0 )
            bitsTaken = 8 + bitsLeftToGo;

        if( bitsTaken == 0 )
            filePos++;
    }

#if RIK_PIXEL_DEBUG
    printf( "\nc%d", ret );
#endif

    return ret;
}

/************************************************************************/
/*                             OutputPixel()                            */
/************************************************************************/

static void OutputPixel( GByte pixel,
                         void * image,
                         GUInt32 imageWidth,
                         GUInt32 lineBreak,
                         int &imageLine,
                         GUInt32 &imagePos )

{
    if( imagePos < imageWidth && imageLine >= 0)
        ((GByte *) image)[imagePos + imageLine * imageWidth] = pixel;

    imagePos++;

#if RIK_PIXEL_DEBUG
    printf( "_%02X", pixel );
#endif

    // Check if we need to change line

    if( imagePos == lineBreak )
    {
#if RIK_PIXEL_DEBUG
        printf( "\n\n", pixel );
#endif

        imagePos = 0;

        imageLine--;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RIKRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    RIKDataset *poRDS = (RIKDataset *) poDS;
    GByte *blockData;
    GUInt32 blocks;
    GUInt32 nBlockIndex;
    GUInt32 nBlockOffset;
    GUInt32 nBlockSize;

    blocks = poRDS->nHorBlocks * poRDS->nVertBlocks;
    nBlockIndex = nBlockXOff + nBlockYOff * poRDS->nHorBlocks;
    nBlockOffset = poRDS->pOffsets[nBlockIndex];

    if( nBlockIndex < (blocks - 1) )
    {
        nBlockSize = poRDS->pOffsets[nBlockIndex + 1];
    }
    else
    {
        nBlockSize = poRDS->nFileSize;
    }
    nBlockSize -= nBlockOffset;

    GUInt32 pixels;

    pixels = poRDS->nBlockXSize * poRDS->nBlockYSize;

    if( !nBlockSize
#ifdef RIK_SINGLE_BLOCK
        || nBlockIndex != RIK_SINGLE_BLOCK
#endif
        )
    {
        for( GUInt32 i = 0; i < pixels; i++ )
        ((GByte *) pImage)[i] = 0;
        return CE_None;
    }

    VSIFSeekL( poRDS->fp, nBlockOffset, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Read uncompressed block.                                        */
/* -------------------------------------------------------------------- */

    if( poRDS->options == 0x00 )
    {
        VSIFRead( pImage, 1, nBlockSize, poRDS->fp );
        return CE_None;
    }

    // Read block to memory
    blockData = (GByte *) CPLMalloc(nBlockSize);
    VSIFRead( blockData, 1, nBlockSize, poRDS->fp );

    GUInt32 filePos = 0;
    GUInt32 imagePos = 0;

/* -------------------------------------------------------------------- */
/*      Read RLE block.                                                 */
/* -------------------------------------------------------------------- */

    if( poRDS->options != 0x0b ) do
    {
        GByte count = blockData[filePos++];
        GByte color = blockData[filePos++];

        for (GByte i = 0; i <= count; i++)
        {
            ((GByte *) pImage)[imagePos++] = color;
        }
    } while( filePos < nBlockSize && imagePos < pixels );
    else

/* -------------------------------------------------------------------- */
/*      Read LZW block.                                                 */
/* -------------------------------------------------------------------- */

    {
        const int LZW_BITS_PER_PIXEL = 8;
        const int LZW_CLEAR = 1 << LZW_BITS_PER_PIXEL;
        const int LZW_MAX_BITS = 13;
        const int LZW_CODES = 1 << LZW_MAX_BITS;
        const int LZW_NO_SUCH_CODE = LZW_CODES + 1;
        const int LZW_OFFSET = 5;

        int lastAdded = LZW_CLEAR;
        int codeBits = LZW_BITS_PER_PIXEL + 1;

        int code;
        int lastCode;
        int prefixChar;
        int bitsTaken = 0;
        int breakOffset = 0;

        int prefix[LZW_CODES], i;
        GByte character[LZW_CODES];

        for( i = 0; i < LZW_CLEAR; i++ )
            character[i] = i;
        for( i = 0; i < LZW_CODES; i++ )
            prefix[i] = LZW_NO_SUCH_CODE;

        filePos = LZW_OFFSET;
        int imageLine = poRDS->nBlockYSize - 1;

        GUInt32 lineBreak = poRDS->nBlockXSize;

        // 32 bit alignment
        if( lineBreak & 3 )
            lineBreak = (lineBreak & 0xfffffffc) + 4;

        code = GetNextLZWCode( codeBits, blockData, filePos, bitsTaken );
        OutputPixel( code, pImage, poRDS->nBlockXSize,
                     lineBreak, imageLine, imagePos );
        prefixChar = code;

        while( imageLine >= 0 &&
               (imageLine || imagePos < poRDS->nBlockXSize - 1) &&
               filePos < nBlockSize ) try
        {
            lastCode = code;
            code = GetNextLZWCode( codeBits, blockData, filePos, bitsTaken );
            if( VSIFEofL( poRDS->fp ) )
            {
                CPLFree( blockData );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "RIK decompression failed. "
                          "Read past end of file.\n" );
                return CE_Failure;
            }

            if (code == LZW_CLEAR)
            {
#if RIK_RESTART_DEBUG
                CPLDebug( "RIK",
                          "Clearing block %d \n"
                          " x=%d y=%d\n"
                          " pos=%d size=%d\n",
                          breakOffset,
                          imagePos, imageLine,
                          filePos, nBlockSize );
#endif

                // Clear prefix table
                for( int i = LZW_CLEAR; i < LZW_CODES; i++ )
                    prefix[i] = LZW_NO_SUCH_CODE;
                lastAdded = LZW_CLEAR;
                codeBits = LZW_BITS_PER_PIXEL + 1;

                if( filePos > 13 ) do
                {
                    filePos++;
                } while( blockData[filePos] == blockData[filePos - 13] );

                filePos += breakOffset;

                if( bitsTaken == 0 )
                    filePos--;
                else
                    bitsTaken = 0;

                code = GetNextLZWCode( codeBits, blockData,
                                       filePos, bitsTaken );

                if( code > lastAdded )
                {
                    throw "Restart Error";
                }

                OutputPixel( code, pImage, poRDS->nBlockXSize,
                             lineBreak, imageLine, imagePos );
                prefixChar = code;
            }
            else
            {
                // Set-up decoding

                GByte stack[LZW_CODES];

                int stackPtr = 0;
                int decodeCode = 0;

                if( prefix[code] == LZW_NO_SUCH_CODE )
                {
                    if( code < LZW_CLEAR )
                    {
                        // Normal character
                        *stack = code;
                        stackPtr = 1;
                    }
                    else if( code == lastAdded + 1 )
                    {
                        // Handle special case
                        *stack = prefixChar;
                        stackPtr = 1;
                        decodeCode = lastCode;
       	            }
                    else
                    {
                        throw "Too high code";
                    }
                }
                else
       	        {
                    // Normal code
                    decodeCode = code;
       	        }

                // Decode

                if( decodeCode )
                {
                    int j = 0;
                    while( ++j < LZW_CODES &&
       	                   decodeCode > LZW_CLEAR )
                    {
                      stack[stackPtr++] = character[decodeCode];
                      decodeCode = prefix[decodeCode];
                    }
                    stack[stackPtr++] = decodeCode;

                    if( j == LZW_CODES )
                    {
                        throw "Decode error";
                    }
       	        }

                // Output stack

                prefixChar = stack[stackPtr - 1];

                while( stackPtr != 0 && imagePos < pixels )
                {
                    OutputPixel( stack[--stackPtr], pImage, poRDS->nBlockXSize,
                                 lineBreak, imageLine, imagePos );
                }

                // Add code to string table

                if( lastCode != LZW_NO_SUCH_CODE &&
                    lastAdded != LZW_CODES - 1 )
                {
                    prefix[++lastAdded] = lastCode;
                    character[lastAdded] = prefixChar;
                }

                // Check if we need to use more bits

                if( lastAdded == (1 << codeBits) - 1 &&
                    codeBits != LZW_MAX_BITS )
                {
                     codeBits++;
                }
            }
        }
        catch (const char *errStr)
        {
#if RIK_RESTART_DEBUG
            CPLDebug( "RIK",
                      "Restarting block %d %s\n"
                      " x=%d y=%d lastAdded=%d\n"
                      " code=%X pos=%d size=%d\n",
                      breakOffset, errStr,
                      imagePos, imageLine, lastAdded,
                      code, filePos, nBlockSize );
#endif
            lastAdded = LZW_CLEAR;
            codeBits = LZW_BITS_PER_PIXEL + 1;
            bitsTaken = 0;

            for( int i = LZW_CLEAR; i < LZW_CODES; i++ )
                prefix[i] = LZW_NO_SUCH_CODE;

            filePos = LZW_OFFSET;
            imagePos = 0;
            imageLine = poRDS->nBlockYSize - 1;

            filePos = 5;
            code = GetNextLZWCode( codeBits, blockData,
                                   filePos, bitsTaken );
            OutputPixel( code, pImage, poRDS->nBlockXSize,
                         lineBreak, imageLine, imagePos );
            prefixChar = code;
            if( breakOffset == 0)
            {
                breakOffset = -1;
                continue;
            }
            else if( breakOffset == -1)
            {
                breakOffset = 1;
                continue;
            }
            else
            {
#if RIK_ALLOW_BLOCK_ERRORS
                CPLDebug( "RIK",
                          "Restart failed\n"
                          " blocks: %d\n"
                          " blockindex: %d\n"
                          " blockoffset: %X\n"
                          " blocksize: %d\n",
                          blocks, nBlockIndex,
                          nBlockOffset, nBlockSize );
                break;
#else
                CPLFree( blockData );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "RIK decompression failed. "
                          "Corrupt image block." );
                          return CE_Failure;
#endif
            }
        }
    }

    CPLFree( blockData );

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp RIKRasterBand::GetColorInterpretation()

{
    return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *RIKRasterBand::GetColorTable()
{
    RIKDataset *poRDS = (RIKDataset *) poDS;

    return poRDS->poColorTable;
}

/************************************************************************/
/* ==================================================================== */
/*				RIKDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ~RIKDataset()                             */
/************************************************************************/

RIKDataset::~RIKDataset()

{
    FlushCache();
    CPLFree( pOffsets );
    if( fp != NULL )
        VSIFClose( fp );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr RIKDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, &fTransform, sizeof(double) * 6 );

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *RIKDataset::GetProjectionRef()

{
    // http://www.sm5sxl.net/~mats/text/gis/Geodesi/geodesi/refsys/sweref-rt/sweref99-rt90.htm

    return( "GEOGCS[\"RT90\",DATUM[\"Rikets_koordinatsystem_1990\",SPHEROID[\"Bessel 1841\",6377397.155,299.1528128,AUTHORITY[\"EPSG\",7004]],TOWGS84[414.1055246174,41.3265500042,603.0582474221,0.8551163377,-2.1413174055,7.0227298286,0],AUTHORITY[\"EPSG\",6124]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"degree\",0.01745329251994328,AUTHORITY[\"EPSG\",9122]],AUTHORITY[\"EPSG\",4124]]" );

}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RIKDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 50 )
        return NULL;

    if( EQUALN((const char *) poOpenInfo->pabyHeader, "RIK3", 4) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "File %s is in unsupported RIK3 format.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the map name.                                              */
/* -------------------------------------------------------------------- */

    GUInt32 nameLength;
    char name[1024];

    VSIFSeekL( poOpenInfo->fp, 0, SEEK_SET );

    VSIFReadL( &nameLength, 1, 2, poOpenInfo->fp );
#ifdef CPL_MSB
    CPL_SWAP16PTR( &nameLength );
#endif

    if( nameLength > 1023 )
    {
        // Unreasonable string length, assume wrong format
        return NULL;
    }

    VSIFReadL( name, 1, nameLength, poOpenInfo->fp );
    name[nameLength] = '\0';

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */

    RIKHeader header;

    VSIFReadL( &header.iUnknown, 1, sizeof(header.iUnknown), poOpenInfo->fp );
    VSIFReadL( &header.fSouth, 1, sizeof(header.fSouth), poOpenInfo->fp );
    VSIFReadL( &header.fWest, 1, sizeof(header.fWest), poOpenInfo->fp );
    VSIFReadL( &header.fNorth, 1, sizeof(header.fNorth), poOpenInfo->fp );
    VSIFReadL( &header.fEast, 1, sizeof(header.fEast), poOpenInfo->fp );
    VSIFReadL( &header.iScale, 1, sizeof(header.iScale), poOpenInfo->fp );
    VSIFReadL( &header.iMPPNum, 1, sizeof(header.iMPPNum), poOpenInfo->fp );
#ifdef CPL_MSB
    CPL_SWAP64PTR( &header.fSouth );
    CPL_SWAP64PTR( &header.fWest );
    CPL_SWAP64PTR( &header.fNorth );
    CPL_SWAP64PTR( &header.fEast );
    CPL_SWAP32PTR( &header.iScale );
    CPL_SWAP32PTR( &header.iMPPNum );
#endif

    if (!CPLIsFinite(header.fSouth) |
        !CPLIsFinite(header.fWest) |
        !CPLIsFinite(header.fNorth) |
        !CPLIsFinite(header.fEast))
        return NULL;

    bool offsetBounds;

    offsetBounds = header.fSouth < 4000000;

    header.iMPPDen = 1;

    if( offsetBounds )
    {
        header.fSouth += 4002995;
        header.fNorth += 5004000;
        header.fWest += 201000;
        header.fEast += 302005;

        VSIFReadL( &header.iMPPDen, 1, sizeof(header.iMPPDen), poOpenInfo->fp );
#ifdef CPL_MSB
        CPL_SWAP32PTR( &header.iMPPDen );
#endif
    }

    double metersPerPixel;
    metersPerPixel = header.iMPPNum / double(header.iMPPDen);

    VSIFReadL( &header.iBlockWidth, 1, sizeof(header.iBlockWidth), poOpenInfo->fp );
    VSIFReadL( &header.iBlockHeight, 1, sizeof(header.iBlockHeight), poOpenInfo->fp );
    VSIFReadL( &header.iHorBlocks, 1, sizeof(header.iHorBlocks), poOpenInfo->fp );
#ifdef CPL_MSB
    CPL_SWAP32PTR( &header.iBlockWidth );
    CPL_SWAP32PTR( &header.iBlockHeight );
    CPL_SWAP32PTR( &header.iHorBlocks );
#endif

    if(( header.iBlockWidth > 2000 ) || ( header.iBlockWidth < 10 ) ||
       ( header.iBlockHeight > 2000 ) || ( header.iBlockHeight < 10 ))
       return NULL;

    if( !offsetBounds )
    {
        VSIFReadL( &header.iVertBlocks, 1, sizeof(header.iVertBlocks), poOpenInfo->fp );
#ifdef CPL_MSB
        CPL_SWAP32PTR( &header.iVertBlocks );
#endif
    }

    if( offsetBounds || !header.iVertBlocks )
    {
        header.iVertBlocks = (GUInt32)
            ceil( (header.fNorth - header.fSouth) /
                  (header.iBlockHeight * metersPerPixel) );
    }

#if RIK_HEADER_DEBUG
    CPLDebug( "RIK",
              "Original vertical blocks %d\n",
              header.iVertBlocks );
#endif

    VSIFReadL( &header.iBitsPerPixel, 1, sizeof(header.iBitsPerPixel), poOpenInfo->fp );

    if( header.iBitsPerPixel != 8 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "File %s has unsupported number of bits per pixel.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

    VSIFReadL( &header.iOptions, 1, sizeof(header.iOptions), poOpenInfo->fp );

    if( !header.iHorBlocks || !header.iVertBlocks )
       return NULL;

/* -------------------------------------------------------------------- */
/*      Check image options.                                            */
/* -------------------------------------------------------------------- */

    if( header.iOptions != 0x00 && // Uncompressed
        header.iOptions != 0x01 && // RLE
        header.iOptions != 0x41 && // RLE
        header.iOptions != 0x0B )  // LZW
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unknown map options.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the palette.                                               */
/* -------------------------------------------------------------------- */

    GByte palette[768];

    for( GUInt16 i = 0; i < 256; i++ )
    {
        VSIFReadL( &palette[i * 3 + 2], 1, 1, poOpenInfo->fp );
        VSIFReadL( &palette[i * 3 + 1], 1, 1, poOpenInfo->fp );
        VSIFReadL( &palette[i * 3 + 0], 1, 1, poOpenInfo->fp );
    }

/* -------------------------------------------------------------------- */
/*      Find block offsets.                                             */
/* -------------------------------------------------------------------- */

    GUInt32 blocks;
    GUInt32 *offsets;

    blocks = header.iHorBlocks * header.iVertBlocks;
    offsets = (GUInt32 *)CPLMalloc( blocks * sizeof(GUInt32) );

    if( !offsets )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to allocate offset table.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

    if( header.iOptions == 0x00 )
    {
        offsets[0] = VSIFTellL( poOpenInfo->fp );

        for( GUInt32 i = 1; i < blocks; i++ )
        {
            offsets[i] = offsets[i - 1] +
                header.iBlockWidth * header.iBlockHeight;
        }
    }
    else
    {
        VSIFReadL( offsets, 1, blocks * sizeof(GUInt32), poOpenInfo->fp );
#ifdef CPL_MSB
        for( GUInt32 i = 0; i < blocks; i++ )
        {
            CPL_SWAP32PTR( &offsets[i] );
        }
#endif
    }

/* -------------------------------------------------------------------- */
/*      Final checks.                                                   */
/* -------------------------------------------------------------------- */

    // File size

    if( VSIFEofL( poOpenInfo->fp ) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Read past end of file.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

    VSIFSeekL( poOpenInfo->fp, 0, SEEK_END );
    GUInt32 fileSize = VSIFTellL( poOpenInfo->fp );

#if RIK_HEADER_DEBUG
    CPLDebug( "RIK",
              "File size %d\n",
              fileSize );
#endif

    // Make sure the offset table is valid

    GUInt32 lastoffset = 0;

    for( GUInt32 y = 0; y < header.iVertBlocks; y++)
    {
        for( GUInt32 x = 0; x < header.iHorBlocks; x++)
        {
            if( offsets[x + y * header.iHorBlocks] >= fileSize )
            {
                if( !y )
                {
                    CPLError( CE_Failure, CPLE_OpenFailed,
                              "File too short.\n",
                              poOpenInfo->pszFilename );
                    return NULL;
                }
                header.iVertBlocks = y;
                break;
            }

            if( offsets[x + y * header.iHorBlocks] < lastoffset )
            {
                if( !y )
                {
                    CPLError( CE_Failure, CPLE_OpenFailed,
                              "Corrupt offset table.\n",
                              poOpenInfo->pszFilename );
                    return NULL;
                }
                header.iVertBlocks = y;
                break;
            }

            lastoffset = offsets[x + y * header.iHorBlocks];
        }
    }

#if RIK_HEADER_DEBUG
    CPLDebug( "RIK",
              "first offset %d\n"
              "last offset %d\n",
              offsets[0],
              lastoffset );
#endif

    char *compression = "RLE";

    if( header.iOptions == 0x00 )
        compression = "Uncompressed";
    if( header.iOptions == 0x0b )
        compression = "LZW";

    CPLDebug( "RIK",
              "RIK file parameters:\n"
              " name: %s\n"
              " unknown: 0x%X\n"
              " south: %lf\n"
              " west: %lf\n"
              " north: %lf\n"
              " east: %lf\n"
              " calculated east: %lf\n"
              " original scale: %d\n"
              " meters per pixel: %lf\n"
              " block width: %d\n"
              " block height: %d\n"
              " horizontal blocks: %d\n"
              " vertical blocks: %d\n"
              " bits per pixel: %d\n"
              " options: 0x%X\n"
              " compression: %s\n",
              name, header.iUnknown,
              header.fSouth, header.fWest, header.fNorth, header.fEast,
              header.fWest + header.iHorBlocks * metersPerPixel * header.iBlockWidth,
              header.iScale, metersPerPixel,
              header.iBlockWidth, header.iBlockHeight,
              header.iHorBlocks, header.iVertBlocks,
              header.iBitsPerPixel, header.iOptions, compression);

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */

    RIKDataset 	*poDS;

    poDS = new RIKDataset();

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

    poDS->fTransform[0] = header.fWest - metersPerPixel / 2.0;
    poDS->fTransform[1] = metersPerPixel;
    poDS->fTransform[2] = 0.0;
    poDS->fTransform[3] = header.fNorth + metersPerPixel / 2.0;
    poDS->fTransform[4] = 0.0;
    poDS->fTransform[5] = -metersPerPixel;

    poDS->nBlockXSize = header.iBlockWidth;
    poDS->nBlockYSize = header.iBlockHeight;
    poDS->nHorBlocks = header.iHorBlocks;
    poDS->nVertBlocks = header.iVertBlocks;
    poDS->pOffsets = offsets;
    poDS->options = header.iOptions;
    poDS->nFileSize = fileSize;

    poDS->nRasterXSize = header.iBlockWidth * header.iHorBlocks;
    poDS->nRasterYSize = header.iBlockHeight * header.iVertBlocks;

    poDS->nBands = 1;

    GDALColorEntry oEntry;
    poDS->poColorTable = new GDALColorTable();
    for( i = 0; i < 256; i++ )
    {
        oEntry.c1 = palette[i * 3 + 2]; // Red
        oEntry.c2 = palette[i * 3 + 1]; // Green
        oEntry.c3 = palette[i * 3];     // Blue
        oEntry.c4 = 255;

        poDS->poColorTable->SetColorEntry( i, &oEntry );
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */

    poDS->SetBand( 1, new RIKRasterBand( poDS, 1 ));

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */

    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_RIK()                          */
/************************************************************************/

void GDALRegister_RIK()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "RIK" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "RIK" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Swedish RIK (.rik)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#RIK" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "rik" );

        poDriver->pfnOpen = RIKDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
