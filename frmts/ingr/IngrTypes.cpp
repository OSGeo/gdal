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

// -----------------------------------------------------------------------------
//                                                            INGR_GetDataType()
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//                                                          INGR_GetFormatName()
// -----------------------------------------------------------------------------

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


// -----------------------------------------------------------------------------
//                                                              INGR_GetFormat()
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//                                                         INGR_GetTransMatrix()
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//                                                         INGR_SetTransMatrix()
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//                                                          INGR_SetIGDSColors()
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//                                                          INGR_GetIGDSColors()
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//                                                       INGR_SetEnvironColors()
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//                                                      INGR_GetEnvironVColors()
// -----------------------------------------------------------------------------

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
    fNormFactor  = 255 / fNormFactor;

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

// -----------------------------------------------------------------------------
//                                                                  SetMiniMax()
// -----------------------------------------------------------------------------

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
// -----------------------------------------------------------------------------
//                                                              INGR_GetMinMax()
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//                                                             INGR_CreateTiff()
// -----------------------------------------------------------------------------

INGR_TiffMem CPL_STDCALL INGR_CreateTiff( const char *pszFilename,
                                          INGR_Format eFormat,
                                          int nTiffXSize, 
                                          int nTiffYSize,
                                          GByte *pabyBuffer,
                                          int nBufferSize)
{
    INGR_TiffMem hMemTiff;
	uint16	hCT[255] = {0xFFFF, 0x0000};

    hMemTiff.pszFileName = CPLSPrintf( "/vsimem/%s.virtual.tiff", 
        CPLGetBasename( pszFilename ) );

    TIFF *hTIFF = VSI_TIFFOpen( hMemTiff.pszFileName, "w+" );

    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH,      nTiffXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH,     nTiffYSize );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,   1 );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLEFORMAT,    SAMPLEFORMAT_UINT );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG,    PLANARCONFIG_CONTIG );
    TIFFSetField( hTIFF, TIFFTAG_FILLORDER,       FILLORDER_MSB2LSB );

	switch( eFormat )
    {
    case JPEGGRAY:
		uint16 nGrays[256];
#define	SCALE(x)	(((x)*((1L<<16)-1))/255)
		for( int i = 0; i < 256; i ++ )
		{
			nGrays[i] = SCALE(i);
		}
        TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP,   -1 );
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
        TIFFSetField( hTIFF, TIFFTAG_COLORMAP,		  nGrays, nGrays, nGrays );
        TIFFSetField( hTIFF, TIFFTAG_COMPRESSION,     COMPRESSION_JPEG );
        TIFFSetField( hTIFF, TIFFTAG_COMPRESSION,     COMPRESSION_JPEG );
        break;
    case JPEGRGB: 
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 3 );
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC,     PHOTOMETRIC_RGB );
        TIFFSetField( hTIFF, TIFFTAG_COMPRESSION,     COMPRESSION_JPEG );
        break;
    case JPEGCYMK:
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 4 );
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC,     PHOTOMETRIC_YCBCR );
        TIFFSetField( hTIFF, TIFFTAG_COMPRESSION,     COMPRESSION_JPEG );
        break;
    case CCITTGroup4:

        REVERSEBITSBUFFER( pabyBuffer, nBufferSize );

        TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP,   -1 );
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC,     PHOTOMETRIC_MINISWHITE );
        TIFFSetField( hTIFF, TIFFTAG_COMPRESSION,     COMPRESSION_CCITTFAX4 );
	default:
		return hMemTiff;
    }

    TIFFWriteRawStrip( hTIFF, 0, pabyBuffer, nBufferSize );
    TIFFWriteDirectory( hTIFF );

    TIFFClose( hTIFF );

    hMemTiff.poDS   = (GDALDataset*) GDALOpen( hMemTiff.pszFileName, GA_ReadOnly );
    hMemTiff.poBand = (GDALRasterBand*) GDALGetRasterBand( hMemTiff.poDS, 1 );

    return hMemTiff;
}

// -----------------------------------------------------------------------------
//                                                      DecodeRunLengthEncoded()
// -----------------------------------------------------------------------------

CPLErr DecodeRunLengthEncoded( GByte *pabySrcData, int nSrcBytes, 
                               int nBlockXSize, int nBlockYSize )
{
    CPLError( CE_Failure, CPLE_FileIO, "DecodeRunLengthEncoded not implemented yet");
    return CE_Fatal;
}

// -----------------------------------------------------------------------------
//                                                           DecodeAdaptiveRGB()
// -----------------------------------------------------------------------------

CPLErr DecodeAdaptiveRGB( GByte *pabySrcData, int nSrcBytes, 
                          int nBlockXSize, int nBlockYSize )
{
    CPLError( CE_Failure, CPLE_FileIO, "DecodeAdaptiveRGB not implemented yet");
    return CE_Fatal;
}

// -----------------------------------------------------------------------------
//                                                     DecodeAdaptiveGrayScale()
// -----------------------------------------------------------------------------

CPLErr DecodeAdaptiveGrayScale( GByte *pabySrcData, int nSrcBytes, 
                                int nBlockXSize, int nBlockYSize )
{
    CPLError( CE_Failure, CPLE_FileIO, "DecodeAdaptiveGrayScale not implemented yet");
    return CE_Fatal;
}

// -----------------------------------------------------------------------------
//                                                        DecodeContinuousTone()
// -----------------------------------------------------------------------------

CPLErr DecodeContinuousTone( GByte *pabySrcData, int nSrcBytes, 
                             int nBlockXSize, int nBlockYSize )
{
    CPLError( CE_Failure, CPLE_FileIO, "DecodeContinuousTone not implemented yet");
    return CE_Fatal;
}

//  ----------------------------------------------------------------------------
//                                                         INGR_LoadJPEGTables()
//  ----------------------------------------------------------------------------

#ifdef undercontruction 

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

#endif

