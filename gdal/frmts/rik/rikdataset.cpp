/******************************************************************************
 * $Id$
 *
 * Project:  RIK Reader
 * Purpose:  All code for RIK Reader
 * Author:   Daniel Wallner, daniel.wallner@bredband.net
 *
 ******************************************************************************
 * Copyright (c) 2005, Daniel Wallner <daniel.wallner@bredband.net>
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <float.h>
#include <zlib.h>
#include "gdal_pam.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_RIK(void);
CPL_C_END

#define RIK_HEADER_DEBUG 0
#define RIK_CLEAR_DEBUG 0
#define RIK_PIXEL_DEBUG 0

//#define RIK_SINGLE_BLOCK 0

#define RIK_ALLOW_BLOCK_ERRORS 1

//
// The RIK file format information was extracted from the trikpanel project:
// http://sourceforge.net/projects/trikpanel/
//
// A RIK file consists of the following elements:
//
// +--------------------+
// | Magic "RIK3"       | (Only in RIK version 3)
// +--------------------+
// | Map name           | (The first two bytes is the string length)
// +--------------------+
// | Header             | (Three different formats exists)
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
// There are four different image block formats:
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
//   These blocks are upside down compared to GDAL.
//
// 4. ZLIB image block
//
//   These blocks are upside down compared to GDAL.
//

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

    VSILFILE        *fp;

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
    static int Identify( GDALOpenInfo * );

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
                           GUInt32 &fileAlign,
                           int &bitsTaken )

{
    if( filePos == fileAlign )
    {
        fileAlign += codeBits;
    }

    const int BitMask[] = {
        0x0000, 0x0001, 0x0003, 0x0007,
        0x000f, 0x001f, 0x003f, 0x007f };

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
    printf( "c%03X\n", ret );
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
    printf( "_%02X %d\n", pixel, imagePos );
#endif

    // Check if we need to change line

    if( imagePos == lineBreak )
    {
#if RIK_PIXEL_DEBUG
        printf( "\n%d\n", imageLine );
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

    nBlockSize = poRDS->nFileSize;
    for( GUInt32 bi = nBlockIndex + 1; bi < blocks; bi++ )
    {
        if( poRDS->pOffsets[bi] )
        {
            nBlockSize = poRDS->pOffsets[bi];
            break;
        }
    }
    nBlockSize -= nBlockOffset;

    GUInt32 pixels;

    pixels = poRDS->nBlockXSize * poRDS->nBlockYSize;

    if( !nBlockOffset || !nBlockSize
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

    if( poRDS->options == 0x00 || poRDS->options == 0x40 )
    {
        VSIFReadL( pImage, 1, nBlockSize, poRDS->fp );
        return CE_None;
    }

    // Read block to memory
    blockData = (GByte *) CPLMalloc(nBlockSize);
    VSIFReadL( blockData, 1, nBlockSize, poRDS->fp );

    GUInt32 filePos = 0;
    GUInt32 imagePos = 0;

/* -------------------------------------------------------------------- */
/*      Read RLE block.                                                 */
/* -------------------------------------------------------------------- */

    if( poRDS->options == 0x01 ||
        poRDS->options == 0x41 ) do
    {
        GByte count = blockData[filePos++];
        GByte color = blockData[filePos++];

        for (GByte i = 0; i <= count; i++)
        {
            ((GByte *) pImage)[imagePos++] = color;
        }
    } while( filePos < nBlockSize && imagePos < pixels );

/* -------------------------------------------------------------------- */
/*      Read LZW block.                                                 */
/* -------------------------------------------------------------------- */

    else if( poRDS->options == 0x0b )
    {
        const bool LZW_HAS_CLEAR_CODE = !!(blockData[4] & 0x80);
        const int LZW_MAX_BITS = blockData[4] & 0x1f; // Max 13
        const int LZW_BITS_PER_PIXEL = 8;
        const int LZW_OFFSET = 5;

        const int LZW_CLEAR = 1 << LZW_BITS_PER_PIXEL;
        const int LZW_CODES = 1 << LZW_MAX_BITS;
        const int LZW_NO_SUCH_CODE = LZW_CODES + 1;

        int lastAdded = LZW_HAS_CLEAR_CODE ? LZW_CLEAR : LZW_CLEAR - 1;
        int codeBits = LZW_BITS_PER_PIXEL + 1;

        int code;
        int lastCode;
        GByte lastOutput;
        int bitsTaken = 0;

        int prefix[8192];      // only need LZW_CODES for size.
        GByte character[8192]; // only need LZW_CODES for size.

        int i;

        for( i = 0; i < LZW_CLEAR; i++ )
            character[i] = (GByte)i;
        for( i = 0; i < LZW_CODES; i++ )
            prefix[i] = LZW_NO_SUCH_CODE;

        filePos = LZW_OFFSET;
        GUInt32 fileAlign = LZW_OFFSET;
        int imageLine = poRDS->nBlockYSize - 1;

        GUInt32 lineBreak = poRDS->nBlockXSize;

        // 32 bit alignment
        lineBreak += 3;
        lineBreak &= 0xfffffffc;

        code = GetNextLZWCode( codeBits, blockData, filePos,
                               fileAlign, bitsTaken );

        OutputPixel( (GByte)code, pImage, poRDS->nBlockXSize,
                     lineBreak, imageLine, imagePos );
        lastOutput = (GByte)code;

        while( imageLine >= 0 &&
               (imageLine || imagePos < poRDS->nBlockXSize) &&
               filePos < nBlockSize ) try
        {
            lastCode = code;
            code = GetNextLZWCode( codeBits, blockData,
                                   filePos, fileAlign, bitsTaken );
            if( VSIFEofL( poRDS->fp ) )
            {
                CPLFree( blockData );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "RIK decompression failed. "
                          "Read past end of file.\n" );
                return CE_Failure;
            }

            if( LZW_HAS_CLEAR_CODE && code == LZW_CLEAR )
            {
#if RIK_CLEAR_DEBUG
                CPLDebug( "RIK",
                          "Clearing block %d\n"
                          " x=%d y=%d\n"
                          " pos=%d size=%d\n",
                          nBlockIndex,
                          imagePos, imageLine,
                          filePos, nBlockSize );
#endif

                // Clear prefix table
                for( i = LZW_CLEAR; i < LZW_CODES; i++ )
                    prefix[i] = LZW_NO_SUCH_CODE;
                lastAdded = LZW_CLEAR;
                codeBits = LZW_BITS_PER_PIXEL + 1;

                filePos = fileAlign;
                bitsTaken = 0;

                code = GetNextLZWCode( codeBits, blockData,
                                       filePos, fileAlign, bitsTaken );

                if( code > lastAdded )
                {
                    throw "Clear Error";
                }

                OutputPixel( (GByte)code, pImage, poRDS->nBlockXSize,
                             lineBreak, imageLine, imagePos );
                lastOutput = (GByte)code;
            }
            else
            {
                // Set-up decoding

                GByte stack[8192]; // only need LZW_CODES for size.

                int stackPtr = 0;
                int decodeCode = code;

                if( code == lastAdded + 1 )
                {
                    // Handle special case
                    *stack = lastOutput;
                    stackPtr = 1;
                    decodeCode = lastCode;
       	        }
                else if( code > lastAdded + 1 )
                {
                    throw "Too high code";
                }

                // Decode

                i = 0;
                while( ++i < LZW_CODES &&
       	               decodeCode >= LZW_CLEAR &&
       	               decodeCode < LZW_NO_SUCH_CODE )
                {
                    stack[stackPtr++] = character[decodeCode];
                    decodeCode = prefix[decodeCode];
                }
                stack[stackPtr++] = (GByte)decodeCode;

                if( i == LZW_CODES || decodeCode >= LZW_NO_SUCH_CODE )
                {
                    throw "Decode error";
                }

                // Output stack

                lastOutput = stack[stackPtr - 1];

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
                    character[lastAdded] = lastOutput;
                }

                // Check if we need to use more bits

                if( lastAdded == (1 << codeBits) - 1 &&
                    codeBits != LZW_MAX_BITS )
                {
                     codeBits++;

                     filePos = fileAlign;
                     bitsTaken = 0;
                }
            }
        }
        catch (const char *errStr)
        {
#if RIK_ALLOW_BLOCK_ERRORS
                CPLDebug( "RIK",
                          "LZW Decompress Failed: %s\n"
                          " blocks: %d\n"
                          " blockindex: %d\n"
                          " blockoffset: %X\n"
                          " blocksize: %d\n",
                          errStr, blocks, nBlockIndex,
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

/* -------------------------------------------------------------------- */
/*      Read ZLIB block.                                                */
/* -------------------------------------------------------------------- */

    else if( poRDS->options == 0x0d )
    {
        uLong destLen = pixels;
        Byte *upsideDown = (Byte *) CPLMalloc( pixels );

        uncompress( upsideDown, &destLen, blockData, nBlockSize );

        for (GUInt32 i = 0; i < poRDS->nBlockYSize; i++)
        {
            memcpy( ((Byte *)pImage) + poRDS->nBlockXSize * i,
                    upsideDown + poRDS->nBlockXSize *
                                 (poRDS->nBlockYSize - i - 1),
                    poRDS->nBlockXSize );
        }

        CPLFree( upsideDown );
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
        VSIFCloseL( fp );
    delete poColorTable;
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
  return( "PROJCS[\"RT90 2.5 gon V\",GEOGCS[\"RT90\",DATUM[\"Rikets_koordinatsystem_1990\",SPHEROID[\"Bessel 1841\",6377397.155,299.1528128,AUTHORITY[\"EPSG\",\"7004\"]],TOWGS84[414.1055246174,41.3265500042,603.0582474221,-0.8551163377,2.1413174055,-7.0227298286,0],AUTHORITY[\"EPSG\",\"6124\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.01745329251994328,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4124\"]],PROJECTION[\"Transverse_Mercator\"],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",15.80827777777778],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",1500000],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AUTHORITY[\"EPSG\",\"3021\"]]" );
}

/************************************************************************/
/*                             GetRikString()                           */
/************************************************************************/

static GUInt16 GetRikString( VSILFILE *fp,
                             char *str,
                             GUInt16 strLength )

{
    GUInt16 actLength;

    VSIFReadL( &actLength, 1, sizeof(actLength), fp );
#ifdef CPL_MSB
    CPL_SWAP16PTR( &actLength );
#endif

    if( actLength + 2 > strLength )
    {
        return actLength;
    }

    VSIFReadL( str, 1, actLength, fp );

    str[actLength] = '\0';

    return actLength;
}

/************************************************************************/
/*                          Identify()                                  */
/************************************************************************/

int RIKDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->fpL == NULL || poOpenInfo->nHeaderBytes < 50 )
        return FALSE;

    if( EQUALN((const char *) poOpenInfo->pabyHeader, "RIK3", 4) )
    {
        return TRUE;
    }
    else
    {
        GUInt16 actLength;
        memcpy(&actLength, poOpenInfo->pabyHeader, 2);
#ifdef CPL_MSB
        CPL_SWAP16PTR( &actLength );
#endif
        if( actLength + 2 > 1024 )
        {
            return FALSE;
        }
        if( actLength == 0 )
            return -1;
        if( strlen( (const char*)poOpenInfo->pabyHeader + 2 ) != actLength )
        {
            return FALSE;
        }
        return TRUE;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RIKDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( Identify(poOpenInfo) == FALSE )
        return NULL;

    bool rik3header = false;

    if( EQUALN((const char *) poOpenInfo->pabyHeader, "RIK3", 4) )
    {
        rik3header = true;
        VSIFSeekL( poOpenInfo->fpL, 4, SEEK_SET );
    }
    else
        VSIFSeekL( poOpenInfo->fpL, 0, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Read the map name.                                              */
/* -------------------------------------------------------------------- */

    char name[1024];

    GUInt16 nameLength = GetRikString( poOpenInfo->fpL, name, sizeof(name) );

    if( nameLength > sizeof(name) - 1 )
    {
        return NULL;
    }

    if( !rik3header )
    {
        if( nameLength == 0 || nameLength != strlen(name) )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */

    RIKHeader header;
    double metersPerPixel;

    const char *headerType = "RIK3";

    if( rik3header )
    {
/* -------------------------------------------------------------------- */
/*      RIK3 header.                                                    */
/* -------------------------------------------------------------------- */

        // Read projection name

        char projection[1024];

        GUInt16 projLength = GetRikString( poOpenInfo->fpL,
                                           projection, sizeof(projection) );

        if( projLength > sizeof(projection) - 1 )
        {
            // Unreasonable string length, assume wrong format
            return NULL;
        }

        // Read unknown string

        projLength = GetRikString( poOpenInfo->fpL, projection, sizeof(projection) );

        // Read map north edge

        char tmpStr[16];

        GUInt16 tmpLength = GetRikString( poOpenInfo->fpL,
                                          tmpStr, sizeof(tmpStr) );

        if( tmpLength > sizeof(tmpStr) - 1 )
        {
            // Unreasonable string length, assume wrong format
            return NULL;
        }

        header.fNorth = atof( tmpStr );

        // Read map west edge

        tmpLength = GetRikString( poOpenInfo->fpL,
                                  tmpStr, sizeof(tmpStr) );

        if( tmpLength > sizeof(tmpStr) - 1 )
        {
            // Unreasonable string length, assume wrong format
            return NULL;
        }

        header.fWest = atof( tmpStr );

        // Read binary values

        VSIFReadL( &header.iScale, 1, sizeof(header.iScale), poOpenInfo->fpL );
        VSIFReadL( &header.iMPPNum, 1, sizeof(header.iMPPNum), poOpenInfo->fpL );
        VSIFReadL( &header.iBlockWidth, 1, sizeof(header.iBlockWidth), poOpenInfo->fpL );
        VSIFReadL( &header.iBlockHeight, 1, sizeof(header.iBlockHeight), poOpenInfo->fpL );
        VSIFReadL( &header.iHorBlocks, 1, sizeof(header.iHorBlocks), poOpenInfo->fpL );
        VSIFReadL( &header.iVertBlocks, 1, sizeof(header.iVertBlocks), poOpenInfo->fpL );
#ifdef CPL_MSB
        CPL_SWAP32PTR( &header.iScale );
        CPL_SWAP32PTR( &header.iMPPNum );
        CPL_SWAP32PTR( &header.iBlockWidth );
        CPL_SWAP32PTR( &header.iBlockHeight );
        CPL_SWAP32PTR( &header.iHorBlocks );
        CPL_SWAP32PTR( &header.iVertBlocks );
#endif

        VSIFReadL( &header.iBitsPerPixel, 1, sizeof(header.iBitsPerPixel), poOpenInfo->fpL );
        VSIFReadL( &header.iOptions, 1, sizeof(header.iOptions), poOpenInfo->fpL );
        header.iUnknown = header.iOptions;
        VSIFReadL( &header.iOptions, 1, sizeof(header.iOptions), poOpenInfo->fpL );

        header.fSouth = header.fNorth -
            header.iVertBlocks * header.iBlockHeight * header.iMPPNum;
        header.fEast = header.fWest +
            header.iHorBlocks * header.iBlockWidth * header.iMPPNum;

        metersPerPixel = header.iMPPNum;
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Old RIK header.                                                 */
/* -------------------------------------------------------------------- */

        VSIFReadL( &header.iUnknown, 1, sizeof(header.iUnknown), poOpenInfo->fpL );
        VSIFReadL( &header.fSouth, 1, sizeof(header.fSouth), poOpenInfo->fpL );
        VSIFReadL( &header.fWest, 1, sizeof(header.fWest), poOpenInfo->fpL );
        VSIFReadL( &header.fNorth, 1, sizeof(header.fNorth), poOpenInfo->fpL );
        VSIFReadL( &header.fEast, 1, sizeof(header.fEast), poOpenInfo->fpL );
        VSIFReadL( &header.iScale, 1, sizeof(header.iScale), poOpenInfo->fpL );
        VSIFReadL( &header.iMPPNum, 1, sizeof(header.iMPPNum), poOpenInfo->fpL );
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

            VSIFReadL( &header.iMPPDen, 1, sizeof(header.iMPPDen), poOpenInfo->fpL );
#ifdef CPL_MSB
            CPL_SWAP32PTR( &header.iMPPDen );
#endif

            headerType = "RIK1";
        }
        else
        {
            headerType = "RIK2";
        }

        metersPerPixel = header.iMPPNum / double(header.iMPPDen);

        VSIFReadL( &header.iBlockWidth, 1, sizeof(header.iBlockWidth), poOpenInfo->fpL );
        VSIFReadL( &header.iBlockHeight, 1, sizeof(header.iBlockHeight), poOpenInfo->fpL );
        VSIFReadL( &header.iHorBlocks, 1, sizeof(header.iHorBlocks), poOpenInfo->fpL );
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
            VSIFReadL( &header.iVertBlocks, 1, sizeof(header.iVertBlocks), poOpenInfo->fpL );
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

        VSIFReadL( &header.iBitsPerPixel, 1, sizeof(header.iBitsPerPixel), poOpenInfo->fpL );

        if( header.iBitsPerPixel != 8 )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "File %s has unsupported number of bits per pixel.\n",
                      poOpenInfo->pszFilename );
            return NULL;
        }

        VSIFReadL( &header.iOptions, 1, sizeof(header.iOptions), poOpenInfo->fpL );

        if( !header.iHorBlocks || !header.iVertBlocks )
           return NULL;

        if( header.iOptions != 0x00 && // Uncompressed
            header.iOptions != 0x40 && // Uncompressed
            header.iOptions != 0x01 && // RLE
            header.iOptions != 0x41 && // RLE
            header.iOptions != 0x0B && // LZW
            header.iOptions != 0x0D )  // ZLIB
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "File %s. Unknown map options.\n",
                      poOpenInfo->pszFilename );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Read the palette.                                               */
/* -------------------------------------------------------------------- */

    GByte palette[768];

    GUInt16 i;
    for( i = 0; i < 256; i++ )
    {
        VSIFReadL( &palette[i * 3 + 2], 1, 1, poOpenInfo->fpL );
        VSIFReadL( &palette[i * 3 + 1], 1, 1, poOpenInfo->fpL );
        VSIFReadL( &palette[i * 3 + 0], 1, 1, poOpenInfo->fpL );
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
                  "File %s. Unable to allocate offset table.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

    if( header.iOptions == 0x00 )
    {
        offsets[0] = VSIFTellL( poOpenInfo->fpL );

        for( GUInt32 i = 1; i < blocks; i++ )
        {
            offsets[i] = offsets[i - 1] +
                header.iBlockWidth * header.iBlockHeight;
        }
    }
    else
    {
        for( GUInt32 i = 0; i < blocks; i++ )
        {
            VSIFReadL( &offsets[i], 1, sizeof(offsets[i]), poOpenInfo->fpL );
#ifdef CPL_MSB
            CPL_SWAP32PTR( &offsets[i] );
#endif
            if( rik3header )
            {
                GUInt32 blockSize;
                VSIFReadL( &blockSize, 1, sizeof(blockSize), poOpenInfo->fpL );
#ifdef CPL_MSB
                CPL_SWAP32PTR( &blockSize );
#endif
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Final checks.                                                   */
/* -------------------------------------------------------------------- */

    // File size

    if( VSIFEofL( poOpenInfo->fpL ) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "File %s. Read past end of file.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

    VSIFSeekL( poOpenInfo->fpL, 0, SEEK_END );
    GUInt32 fileSize = VSIFTellL( poOpenInfo->fpL );

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
            if( !offsets[x + y * header.iHorBlocks] )
            {
                continue;
            }

            if( offsets[x + y * header.iHorBlocks] >= fileSize )
            {
                if( !y )
                {
                    CPLError( CE_Failure, CPLE_OpenFailed,
                              "File %s too short.\n",
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
                              "File %s. Corrupt offset table.\n",
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

    const char *compression = "RLE";

    if( header.iOptions == 0x00 ||
        header.iOptions == 0x40 )
        compression = "Uncompressed";
    if( header.iOptions == 0x0b )
        compression = "LZW";
    if( header.iOptions == 0x0d )
        compression = "ZLIB";

    CPLDebug( "RIK",
              "RIK file parameters:\n"
              " name: %s\n"
              " header: %s\n"
              " unknown: 0x%X\n"
              " south: %f\n"
              " west: %f\n"
              " north: %f\n"
              " east: %f\n"
              " original scale: %d\n"
              " meters per pixel: %f\n"
              " block width: %d\n"
              " block height: %d\n"
              " horizontal blocks: %d\n"
              " vertical blocks: %d\n"
              " bits per pixel: %d\n"
              " options: 0x%X\n"
              " compression: %s\n",
              name, headerType, header.iUnknown,
              header.fSouth, header.fWest, header.fNorth, header.fEast,
              header.iScale, metersPerPixel,
              header.iBlockWidth, header.iBlockHeight,
              header.iHorBlocks, header.iVertBlocks,
              header.iBitsPerPixel, header.iOptions, compression);

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */

    RIKDataset 	*poDS;

    poDS = new RIKDataset();

    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;

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

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The RIK driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Swedish Grid RIK (.rik)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#RIK" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "rik" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = RIKDataset::Open;
        poDriver->pfnIdentify = RIKDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
