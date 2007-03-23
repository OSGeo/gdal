/*****************************************************************************
 * $Id$
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Types support function
 * Author:   Ivan Lucena, ivan@ilucena.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Ivan Lucena
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files ( the "Software" ),
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
 *****************************************************************************/

#include "IngrTypes.h"

// ------------------------------------------------------------------------
//                                                   INGR_GetDataType()
// ------------------------------------------------------------------------

const GDALDataType CPL_STDCALL INGR_GetDataType( uint16 eCode )
{
    unsigned int i;

    for( i = 0; i < FORMAT_TAB_COUNT; i++ )
    {
		if( eCode == INGR_FormatTable[i].eFormatCode )
        {
            return INGR_FormatTable[i].eDataType;
        }
    }

    return GDT_Unknown;
}

// ------------------------------------------------------------------------
//                                                  INGR_GetFormatName()
// ------------------------------------------------------------------------

const char * CPL_STDCALL INGR_GetFormatName( uint16 eCode )
{
    unsigned int i;

    for( i = 0; i < FORMAT_TAB_COUNT; i++ )
    {
        if( eCode == INGR_FormatTable[i].eFormatCode )
        {
            return CPLStrdup( INGR_FormatTable[i].pszName );
        }
    }

    return "Not Identified";
}


// ------------------------------------------------------------------------
//                                                  INGR_GetFormat()
// ------------------------------------------------------------------------

const INGR_Format CPL_STDCALL INGR_GetFormat( GDALDataType eType, const char *pszCompression )
{
    if( EQUAL( pszCompression, "None" ) ||
        EQUAL( pszCompression, "" ) )
    {
        switch ( eType )
        {
        case GDT_Byte:      return ByteInteger;
        case GDT_Int16:     return WordIntegers;
        case GDT_UInt16:    return WordIntegers;
        case GDT_Int32:     return Integers32Bit;
        case GDT_UInt32:    return Integers32Bit;
        case GDT_Float32:   return FloatingPoint32Bit;
        case GDT_Float64:   return FloatingPoint64Bit;
        default:            return ByteInteger;
        }
    }

    unsigned int i;

    for( i = 0; i < FORMAT_TAB_COUNT; i++ )
    {
        if( EQUAL( pszCompression, INGR_FormatTable[i].pszName ) )
        {
            return (INGR_Format) INGR_FormatTable[i].eFormatCode;
        }
    }

    return ByteInteger;
}

// ------------------------------------------------------------------------
//                                                      INGR_GetTransMatrix()
// ------------------------------------------------------------------------

void CPL_STDCALL INGR_GetTransMatrix( real64 *padfMatrix, double *padfGeoTransform )
{
    padfGeoTransform[0] = padfMatrix[3];
    padfGeoTransform[1] = padfMatrix[0];
    padfGeoTransform[2] = padfMatrix[1];
    padfGeoTransform[3] = padfMatrix[7];
    padfGeoTransform[4] = padfMatrix[4];
    padfGeoTransform[5] = padfMatrix[5];

    padfGeoTransform[0] -= ( padfGeoTransform[1] / 2 );
    padfGeoTransform[3] += ( padfGeoTransform[5] / 2 );
    padfGeoTransform[5] *= -1;
}

// ------------------------------------------------------------------------
//                                                      INGR_SetTransMatrix()
// ------------------------------------------------------------------------

void CPL_STDCALL INGR_SetTransMatrix( real64 *padfMatrix, 
                                    double *padfGeoTransform )
{
    for( int i = 0; i < 15; i++ )
    {
        padfMatrix[i] = 0.0;
    }

    padfMatrix[15] = 1.0;

    padfMatrix[3] = padfGeoTransform[0];
    padfMatrix[0] = padfGeoTransform[1];
    padfMatrix[1] = padfGeoTransform[2];
    padfMatrix[7] = padfGeoTransform[3];
    padfMatrix[4] = padfGeoTransform[4];
    padfMatrix[5] = padfGeoTransform[5];

    padfMatrix[5] *= -1;
    padfMatrix[3] -= ( padfMatrix[5] / 2 );
    padfMatrix[7] -= ( padfMatrix[5] / 2 );
}

// ------------------------------------------------------------------------
//                                                      INGR_SetIGDSColors()
// ------------------------------------------------------------------------

uint32 CPL_STDCALL INGR_SetIGDSColors( GDALColorTable *poColorTable,
                                      INGR_ColorTable256 *pColorTableIGDS )
{
    GDALColorEntry oEntry;
    int i;

    for( i = 0; i < poColorTable->GetColorEntryCount(); i++ )
    {
        poColorTable->GetColorEntryAsRGB( i, &oEntry );
        pColorTableIGDS->Entry->v_red   = (uint8) oEntry.c1;
        pColorTableIGDS->Entry->v_green = (uint8) oEntry.c2;
        pColorTableIGDS->Entry->v_blue  = (uint8) oEntry.c3;
    }

    return i;
}

// ------------------------------------------------------------------------
//                                                      INGR_GetIGDSColors()
// ------------------------------------------------------------------------

void CPL_STDCALL INGR_GetIGDSColors( GDALColorTable *poColorTable,
                                    INGR_ColorTable256 *pColorTableIGDS )
{
    GDALColorEntry oEntry;
    int i;

    for( i = 0; i < 256; i++ )
    {
        oEntry.c1 = (short) pColorTableIGDS->Entry->v_red;
        oEntry.c2 = (short) pColorTableIGDS->Entry->v_green;
        oEntry.c2 = (short) pColorTableIGDS->Entry->v_blue;
        poColorTable->SetColorEntry( i, &oEntry );
    }
}

// ------------------------------------------------------------------------
//                                                   INGR_SetEnvironColors()
// ------------------------------------------------------------------------

uint32 CPL_STDCALL INGR_SetEnvironColors( GDALColorTable *poColorTable,
                                         INGR_ColorTableVar *pEnvironTable )
{
    GDALColorEntry oEntry;
    real32 fNormFactor = 0xfff / 255;
    int i;

    for( i = 0; i < poColorTable->GetColorEntryCount(); i++ )
    {
        poColorTable->GetColorEntryAsRGB( i, &oEntry );
        pEnvironTable->Entry->v_slot  = (uint16) i;
        pEnvironTable->Entry->v_red   = (uint16) ( oEntry.c1 * fNormFactor );
        pEnvironTable->Entry->v_green = (uint16) ( oEntry.c2 * fNormFactor );
        pEnvironTable->Entry->v_blue  = (uint16) ( oEntry.c3 * fNormFactor );
    }

    return i;
}

// ------------------------------------------------------------------------
//                                                 INGR_GetEnvironVColors()
// ------------------------------------------------------------------------

void CPL_STDCALL INGR_GetEnvironVColors( GDALColorTable *poColorTable,
                                       INGR_ColorTableVar *pEnvironTable,
                                       uint32 nColorsCount )
{
    // -------------------------------------------------------------
    // Sort records by crescent order of "slot" 
    // -------------------------------------------------------------

    vlt_slot hSwapSlot;
    bool bContinue = true; // Inproved bubble sort.
    uint32 i;              // This is the best sort techique when you
    uint32 j;              // suspect that the data is already sorted

    for( j = 1; j < nColorsCount && bContinue; j++ )
    {
        bContinue = false;
        for( i = 0; i < nColorsCount - j; i++ )
        {
            if( pEnvironTable->Entry[i].v_slot > 
                pEnvironTable->Entry[i + 1].v_slot )
            {
                hSwapSlot = pEnvironTable->Entry[i];
                pEnvironTable->Entry[i]   = pEnvironTable->Entry[i+1];
                pEnvironTable->Entry[i+1] = hSwapSlot;
                bContinue = true;
            }
        }
    }

    // -------------------------------------------------------------
    // Get Maximum Intensity and Index Values
    // -------------------------------------------------------------

    uint32 nMaxIndex    = 0;
    real32 fMaxRed      = 0.0;
    real32 fMaxGreen    = 0.0;
    real32 fMaxBlues    = 0.0;

    for( i = 0; i < nColorsCount; i++ )
    {
        if( pEnvironTable->Entry[i].v_slot > nMaxIndex )
            nMaxIndex = pEnvironTable->Entry[i].v_slot;
        if( pEnvironTable->Entry[i].v_red > fMaxRed )
            fMaxRed = pEnvironTable->Entry[i].v_red;
        if( pEnvironTable->Entry[i].v_green > fMaxGreen )
            fMaxGreen = pEnvironTable->Entry[i].v_green;
        if( pEnvironTable->Entry[i].v_blue > fMaxBlues )
            fMaxBlues = pEnvironTable->Entry[i].v_blue;
    }

    // -------------------------------------------------------------
    // Calculate Normalization Factor
    // -------------------------------------------------------------

    real32 fNormFactor  = 0.0;

    fNormFactor  = ( fMaxRed > fMaxGreen ? fMaxRed : fMaxGreen );
    fNormFactor  = ( fNormFactor > fMaxBlues ? fNormFactor : fMaxBlues );
    fNormFactor  = 256 / fNormFactor;

    // -------------------------------------------------------------
    // Loads GDAL Color Table ( filling the wholes )
    // -------------------------------------------------------------

    GDALColorEntry oEntry;
    GDALColorEntry oNullEntry;

    oNullEntry.c1 = (short) 0;
    oNullEntry.c2 = (short) 0;
    oNullEntry.c3 = (short) 0;
    oNullEntry.c4 = (short) 255;

    for( i = 0, j = 0; i <= nMaxIndex; i++ )
    {
        if( pEnvironTable->Entry[j].v_slot == i )
        {
            oEntry.c1 = (short) ( pEnvironTable->Entry[j].v_red   * fNormFactor );
            oEntry.c2 = (short) ( pEnvironTable->Entry[j].v_green * fNormFactor );
            oEntry.c3 = (short) ( pEnvironTable->Entry[j].v_blue  * fNormFactor );
            oEntry.c4 = (short) 255;
            poColorTable->SetColorEntry( i, &oEntry );
            j++;
        }
        else
        {
            poColorTable->SetColorEntry( i, &oNullEntry );
        }
    }
}

// ------------------------------------------------------------------------
//                                                              SetMiniMax()
// ------------------------------------------------------------------------

INGR_MinMax CPL_STDCALL INGR_SetMinMax( GDALDataType eType, double dValue )
{
    INGR_MinMax uResult;

    switch ( eType )
    {
    case GDT_Byte:
        uResult.AsUint8   = (uint8) dValue;
        break;
    case GDT_Int16:
        uResult.AsUint16  = (int16) dValue;
        break;
    case GDT_UInt16:
        uResult.AsUint16  = (uint16) dValue;
        break;
    case GDT_Int32:
        uResult.AsUint32  = (int32) dValue;
        break;
    case GDT_UInt32:
        uResult.AsUint32  = (uint32) dValue;
        break;
    case GDT_Float32:
        uResult.AsReal32  = (real32) dValue;
        break;
    case GDT_Float64:
        uResult.AsReal64  = (real64) dValue;
    default:
        uResult.AsUint8   = (uint8) 0;
    }

    return uResult;
}

// ------------------------------------------------------------------------
//                                                              INGR_GetMinMax()
// ------------------------------------------------------------------------

double CPL_STDCALL INGR_GetMinMax( GDALDataType eType, INGR_MinMax hValue )
{
    switch ( eType )
    {
    case GDT_Byte:    return (double) hValue.AsUint8;
    case GDT_Int16:   return (double) hValue.AsUint16;
    case GDT_UInt16:  return (double) hValue.AsUint16;
    case GDT_Int32:   return (double) hValue.AsUint32;
    case GDT_UInt32:  return (double) hValue.AsUint32;
    case GDT_Float32: return (double) hValue.AsReal32;
    case GDT_Float64: return (double) hValue.AsReal64;
    default:          return (double) 0.0;
    }
}

// ------------------------------------------------------------------------
//                                                 DecodeRunLengthEncoded()
// ------------------------------------------------------------------------

CPLErr DecodeRunLengthEncoded( GByte *pabySrcData, int nSrcBytes, 
                               int nBlockXSize, int nBlockYSize )
{
/*
Type 9: Run Length Encoded

    The type 9 format compresses bi-level raster data. Each 16 bit unsigned
    word stores the number of pixels with an identical level.

    This format consists of a series of on and off runs. Each line starts with an
    "OFF" run (background level) followed by an "ON" run (foreground level).
    This OFF/ON pattern repeats until the end of the line and a new line starts.
    This continues until the end of the file. Each line ends with an "OFF" run.
    A zero length "OFF" run is placed at the end of a line if necessary. Zero
    length runs are valid runs any where in the file. Runs are always less than
    65536 pixels in length. It is strongly recommended that applications cut the
    run at 32767 pixels for consistency with the majority of applications.

    Data stored in this format is generally referred to as run length, RLE or
    type 9 data. The most popular file extension is ".RLE".

    A large number of files exist in this format throughout the Intergraph
    installed customer base. It has been primarily the storage format for bilevel
    image data. It is not as efficient as CCITT-Group IV compression.
    This format should be read by all applications, and only written when
    specifically required. The preferred storage format is type 24 or type
    65/24.
*/
    CPLError( CE_Failure, CPLE_FileIO, "DecodeRunLengthEncoded not implemented yet");
    return CE_Fatal;
}

/*
 Type 27: Compressed RGB

    Type 27 format is a compressed RGB format for 24-bit color data. A file
    of this format can contain both run length encoded data and unencoded
    data for separate red, green, and blue components.
    The RGB data is band interleaved by line. Each line of RGB data is stored
    as a line of red data, followed by a line of green data and a line of blue
    data. The end of line is reached when the appropriate number of pixels
    have been represented.

    Each line of raster data is composed of one or more atoms. An atom is a
    string of bytes consisting of an atom head and an atom tail. The first byte,
    or the atom head, is a signed value that determines the size and format of
    the remaining byte(s), the atom tail. If the atom is positive (1 to 127), then
    the atom tail contains that number of bytes. These bytes are to be
    interpreted as a string of individual intensity values. If the atom head is
    negative (-1 to -128), then it signifies a constant shade run length. In this
    case, the absolute value of the atom head is the length of the run, and the
    atom tail is a single byte that specifies the intensity of the run. Atom head
    values of zero are ignored, and the atom is skipped.
    
    The tiled version of this format is the recommended method of storing 24
    bit data color raster data. It is strongly suggested that this format be used
    by all new applications.
*/

// ------------------------------------------------------------------------
// Copied from ..\frmts\aigrid\aigccitt.c               DecodeCCITTGroup4()
// ------------------------------------------------------------------------

CPLErr DecodeCCITTGroup4( GByte *pabyCur, int nDataSize, int nMin,
                          int nBlockXSize, int nBlockYSize,
                          GByte *panData )
{
    CPLError( CE_Failure, CPLE_FileIO, "DecodeCCITTGroup4 not implemented yet");
    return CE_None;
}

// ------------------------------------------------------------------------
//                                                      DecodeAdaptiveRGB()
// ------------------------------------------------------------------------

CPLErr DecodeAdaptiveRGB( GByte *pabySrcData, int nSrcBytes, 
                          int nBlockXSize, int nBlockYSize )
{
    CPLError( CE_Failure, CPLE_FileIO, "DecodeAdaptiveRGB not implemented yet");
    return CE_Fatal;
}

// ------------------------------------------------------------------------
//                                                DecodeAdaptiveGrayScale()
// ------------------------------------------------------------------------

CPLErr DecodeAdaptiveGrayScale( GByte *pabySrcData, int nSrcBytes, 
                                int nBlockXSize, int nBlockYSize )
{
    CPLError( CE_Failure, CPLE_FileIO, "DecodeAdaptiveGrayScale not implemented yet");
    return CE_Fatal;
}

// ------------------------------------------------------------------------
//                                                   DecodeContinuousTone()
// ------------------------------------------------------------------------

CPLErr DecodeContinuousTone( GByte *pabySrcData, int nSrcBytes, 
                             int nBlockXSize, int nBlockYSize )
{
    CPLError( CE_Failure, CPLE_FileIO, "DecodeContinuousTone not implemented yet");
    return CE_Fatal;
}

// ------------------------------------------------------------------------
//                                                             DecodeJPEG()
// ------------------------------------------------------------------------

CPLErr DecodeJPEG( GByte *pabySrcData, int nSrcBytes, 
                   int nBlockXSize, int nBlockYSize )
{
    CPLError( CE_Failure, CPLE_FileIO, "DecodeJPEG not implemented yet");
    return CE_Fatal;
}

/*
Type 29: Compressed 8 Bit

    The type 29 format is a compressed 8 bit data format that combines a run
    length encoding technique with straight byte per pixel format. This format
    is the 8 bit counterpart to the 24 bit compressed RGB format (type 27).

    Each line of raster data is composed of one or more atoms. An atom is a
    string of bytes consisting of an atom head and an atom tail. The first byte,
    or the atom head, is a signed value that determines the size and format of
    the remaining byte(s), the atom tail. If the atom is positive (1 to 127), then
    the atom tail contains that number of bytes. These bytes are to be
    interpreted as a string of individual intensity values. If the atom head is
    negative (-1 to -128), then it signifies a constant intensity run length. In
    this case, the absolute value of the atom head is the length of the run, and
    the atom tail is a single byte that specifies the intensity of the run. Atom
    heads of zero are ignored and the atom is skipped.

    The tiled version of this format is the recommended method of storing 8 bit
    raster data. It is strongly suggested that this format be used by all new
    applications.
*/
/*
Type 67: Continuous Tone CMYK

    Type 67 files are used to store continuous tone CMYK images. These
    images are representative of images used in the lithographic printing
    process. Each color ink used in the printing process is represented by an
    intensity in the image file. The three subtractive primaries of Cyan,
    Magenta and Yellow, and a fourth printing ink in Black are used in this
    image format. Each pixel in the file is represented as a set of eight bit
    values corresponding to these four colors.

    In the Type 67 file, an eight bit value of zero represents 100% ink
    coverage, and a value of 255 represents 0% ink. Even though this is a
    continuous tone format, it includes a facility for run length encoding to
    allow large contiguous areas of solid color to be represented compactly.

    Each row is made up of a line of cyan atoms, followed by a line of magenta
    atoms, followed by a line of yellow, and finally black. Atoms are composed
    of a string of bytes which represent a run length. The first byte of the atom
    is the atom head. The atom head determines the length of the atom tail.

    If the atom head positive, then that number of bytes follow the atom head
    and the bytes are individual pixel values.

    A negative atom head indicates that the next bytes represent a string of
    pixels that are all the same intensity. The absolute value of the head is the
    length of the run, The single unsigned byte value immediately following the
    atom head is the value of the run.

    When the number of pixels per row are represented for each color, the
    current line is terminated. Note that an atom head with a zero value is
    meaningless.
*/
 /*
Type 30, 31 and 32: JPEG

    JPEG is a compression technique defined by the "Joint Photographic
    Experts Group," that is used primarily for full color, pictorial images. It
    uses a block by block conversion to frequency space, and stores a discrete
    cosine series representation of the frequency space. Compression is
    achieved by defining the number of terms to retain in the cosine series. The
    image is converted (if necessary) into HSI color space, and then
    compressed. The Hue and Saturation images can be greatly compressed
    without loosing pictorial quality. This is the only "lossy" compression
    technique Intergraph supports.

    Type 30 is JPEG compressed greyscale imagery with eight bits per pixel in
    the original image. By default, no color table association exists for this
    type of data. Type 31 is JPEG compressed RGB data, 24 bit color raster
    images, and type 32 is JPEG compressed CYMK, four band imagery.

    As the JPEG formats contain a private application packet that contains
    critical information necessary to decode the JPEG data, contact the
    Intergraph Raster Review Board Chairman at the address in the beginning
    of this document for information about this application packet.

    Tiled JPEG files must have "full" tiles for proper compression and
    decompression without image anomalies. Tiles should be padded with the
    last pixel value in each line to give the least distortion.
*/

/*
        unsigned int nBitsCount = nBufferRead * 8;

        GByte *pabyBitAsByte = (GByte*) CPLMalloc( nBitsCount );

        for( unsigned int i = 0; i < nBitsCount; i++ )
        {
            if( (pabyIntermediate[i>>3] & (1 << (i & 0x7))) )
                pabyBitAsByte[i] = 1;
            else
                pabyBitAsByte[i] = 0;
        }
                
        for( unsigned int i = 0; i < ( nBitsCount - 24 ); i++ )
        {
            if( pabyBitAsByte[i]    == 0 &&
                pabyBitAsByte[i+1]  == 0 &&
                pabyBitAsByte[i+2]  == 0 && 
                pabyBitAsByte[i+3]  == 0 && 
                pabyBitAsByte[i+4]  == 0 && 
                pabyBitAsByte[i+5]  == 0 &&
                pabyBitAsByte[i+6]  == 0 &&
                pabyBitAsByte[i+7]  == 0 &&
                pabyBitAsByte[i+8]  == 0 &&
                pabyBitAsByte[i+9]  == 0 &&
                pabyBitAsByte[i+10] == 0 &&
                pabyBitAsByte[i+11] == 1 &&
                pabyBitAsByte[i+12] == 0 &&
                pabyBitAsByte[i+13] == 0 &&
                pabyBitAsByte[i+14] == 0 &&
                pabyBitAsByte[i+15] == 0 &&
                pabyBitAsByte[i+16] == 0 &&
                pabyBitAsByte[i+17] == 0 &&
                pabyBitAsByte[i+18] == 0 &&
                pabyBitAsByte[i+19] == 0 &&
                pabyBitAsByte[i+20] == 0 &&
                pabyBitAsByte[i+21] == 0 &&
                pabyBitAsByte[i+22] == 0 &&
                pabyBitAsByte[i+23] == 1 )
            {
                this->nBytesRead = nBufferRead;
                break;
            }
        }

        CPLFree( pabyBitAsByte );
*/


//  ----------------------------------------------------------------------------
//                                                      INGR_LoadJPEGTables()
//  ----------------------------------------------------------------------------

void INGR_LoadJPEGTables( j_compress_ptr cinfo, int n, int nQLevel )
{
    if( nQLevel < 1 )
        return;

/* -------------------------------------------------------------------- */
/*      Load quantization table						                    */
/* -------------------------------------------------------------------- */
    int i;
    JQUANT_TBL  *quant_ptr;
    const int *panQTable;

    if( nQLevel == 1 )
        panQTable = Q1table;
    else if( nQLevel == 2 )
        panQTable = Q2table;
    else if( nQLevel == 3 )
        panQTable = Q3table;
    else if( nQLevel == 4 )
        panQTable = Q4table;
    else if( nQLevel == 5 )
        panQTable = Q5table;
    else
        return;

    if (cinfo->quant_tbl_ptrs[n] == NULL)
    {
        cinfo->quant_tbl_ptrs[n] = 
            jpeg_alloc_quant_table( (j_common_ptr) cinfo );
    }

    quant_ptr = cinfo->quant_tbl_ptrs[n];	/* quant_ptr is JQUANT_TBL* */

    for (i = 0; i < 64; i++) 
    {
        /* Qtable[] is desired quantization table, in natural array order */
        quant_ptr->quantval[i] = panQTable[i];
    }

/* -------------------------------------------------------------------- */
/*      Load AC huffman table.                                          */
/* -------------------------------------------------------------------- */
    JHUFF_TBL  *huff_ptr;

    if (cinfo->ac_huff_tbl_ptrs[n] == NULL)
    {
        cinfo->ac_huff_tbl_ptrs[n] =
            jpeg_alloc_huff_table( (j_common_ptr) cinfo );
    }

    huff_ptr = cinfo->ac_huff_tbl_ptrs[n];	/* huff_ptr is JHUFF_TBL* */

    for (i = 1; i <= 16; i++) 
    {
        /* counts[i] is number of Huffman codes of length i bits, i=1..16 */
        huff_ptr->bits[i] = AC_BITS[i-1];
    }

    for (i = 0; i < 256; i++) 
    {
        /* symbols[] is the list of Huffman symbols, in code-length order */
        huff_ptr->huffval[i] = AC_HUFFVAL[i];
    }

/* -------------------------------------------------------------------- */
/*      Load DC huffman table.                                          */
/* -------------------------------------------------------------------- */
    if (cinfo->dc_huff_tbl_ptrs[n] == NULL)
    {
        cinfo->dc_huff_tbl_ptrs[n] =
            jpeg_alloc_huff_table( (j_common_ptr) cinfo );
    }

    huff_ptr = cinfo->dc_huff_tbl_ptrs[n];	/* huff_ptr is JHUFF_TBL* */

    for (i = 1; i <= 16; i++) 
    {
        /* counts[i] is number of Huffman codes of length i bits, i=1..16 */
        huff_ptr->bits[i] = DC_BITS[i-1];
    }

    for (i = 0; i < 256; i++) 
    {
        /* symbols[] is the list of Huffman symbols, in code-length order */
        huff_ptr->huffval[i] = DC_HUFFVAL[i];
    }

}
