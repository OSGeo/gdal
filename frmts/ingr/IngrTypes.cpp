/*****************************************************************************
 * $Id$
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Types support function
 * Author:   Ivan Lucena, ivan.lucena@pmldnet.com
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
#include "JpegHelper.h"

static INGR_FormatDescription INGR_FormatTable[] = {
    {PackedBinary,            "Packed Binary",               GDT_Byte},
    {ByteInteger,             "Byte Integer",                GDT_Byte},
    {WordIntegers,            "Word Integers",               GDT_Int16},
    {Integers32Bit,           "Integers 32Bit",              GDT_Int32},
    {FloatingPoint32Bit,      "Floating Point 32Bit",        GDT_Float32},
    {FloatingPoint64Bit,      "Floating Point 64Bit",        GDT_Float64},
    {Complex,                 "Complex",                     GDT_CFloat32},
    {DoublePrecisionComplex,  "Double Precision Complex",    GDT_CFloat64},
    {RunLengthEncoded,        "Run Length Encoded Bitonal",  GDT_Byte},
    {RunLengthEncodedC,       "Run Length Encoded Color",    GDT_UInt16},
    {FigureOfMerit,           "Figure of Merit",             GDT_Byte},
    {DTMFlags,                "DTMFlags",                    GDT_Byte},
    {RLEVariableValuesWithZS, "RLE Variable Values With ZS", GDT_Byte},
    {RLEBinaryValues,         "RLE Binary Values",           GDT_Byte},
    {RLEVariableValues,       "RLE Variable Values",         GDT_Byte},
    {RLEVariableValuesWithZ,  "RLE Variable Values With Z",  GDT_Byte},
    {RLEVariableValuesC,      "RLE Variable Values C",       GDT_Byte},
    {RLEVariableValuesN,      "RLE Variable Values N",       GDT_Byte},
    {QuadTreeEncoded,         "Quad Tree Encoded",           GDT_Byte},
    {CCITTGroup4,             "CCITT Group 4",               GDT_Byte},
    {RunLengthEncodedRGB,     "Run Length Encoded RGB",      GDT_Byte},
    {VariableRunLength,       "Variable Run Length",         GDT_Byte},
    {AdaptiveRGB,             "Adaptive RGB",                GDT_Byte},
    {Uncompressed24bit,       "Uncompressed 24bit",          GDT_Byte},
    {AdaptiveGrayScale,       "Adaptive Gray Scale",         GDT_Byte},
    {JPEGGRAY,                "JPEG GRAY",                   GDT_Byte},
    {JPEGRGB,                 "JPEG RGB",                    GDT_Byte},
    {JPEGCYMK,                "JPEG CYMK",                   GDT_Byte},
    {TiledRasterData,         "Tiled Raste Data",            GDT_Byte},
    {NotUsedReserved,         "Not Used( Reserved )",        GDT_Byte},
    {ContinuousTone,          "Continuous Tone",             GDT_Byte},
    {LineArt,                 "LineArt",                     GDT_Byte}
};

static char *IngrOrientation[] = {
    "Upper Left Vertical",
    "Upper Right Vertical",
    "Lower Left Vertical",
    "Lower Right Vertical",
    "Upper Left Horizontal",
    "Upper Right Horizontal",
    "Lower Left Horizontal",
    "Lower Right Horizontal"};

static GByte BitReverseTable[256] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

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
//                                                         INGR_GetOrientation()
// -----------------------------------------------------------------------------

const char * CPL_STDCALL INGR_GetOrientation( uint8 nIndex )
{
    return IngrOrientation[nIndex];
}

// -----------------------------------------------------------------------------
//                                                              INGR_GetFormat()
// -----------------------------------------------------------------------------

const INGR_Format CPL_STDCALL INGR_GetFormat( GDALDataType eType, 
                                              const char *pszCompression )
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
    unsigned int i;

    for( i = 0; i < 15; i++ )
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
//                                                       INGR_GetTileDirectory()
// -----------------------------------------------------------------------------

uint32 CPL_STDCALL INGR_GetTileDirectory( FILE *fp,
                                          uint32 nOffset,
                                          int nBandXSize,
                                          int nBandYSize,
                                          INGR_TileHeader *pTileDir,
                                          INGR_TileItem **pahTiles)
{
    if( fp == NULL ||
        nOffset < 0 ||
        nBandXSize < 1 ||
        nBandYSize < 1 ||
        pTileDir == NULL )
    {
        return 0;
    }

    // -------------------------------------------------------------
    // Read it from the begging of the data segment
    // -------------------------------------------------------------

    if( ( VSIFSeekL( fp, nOffset, SEEK_SET ) == -1 ) ||
        ( VSIFReadL( pTileDir, 1, SIZEOF_TDIR, fp ) == 0 ) )
    {
        CPLDebug("INGR", "Error reading tiles header");
        return 0;
    }

    // ----------------------------------------------------------------
    // Calculate the number of tiles
    // ----------------------------------------------------------------

    int nTilesPerCol = (int) ceil( (float) nBandXSize / pTileDir->TileSize );
    int nTilesPerRow = (int) ceil( (float) nBandYSize / pTileDir->TileSize );

    uint32 nTiles = nTilesPerCol * nTilesPerRow;

    // ----------------------------------------------------------------
    // Load the tile table (first tile s already read)
    // ----------------------------------------------------------------

    *pahTiles  = (INGR_TileItem*) CPLCalloc( nTiles, SIZEOF_TILE );

    (*pahTiles)[0].Start      = pTileDir->First.Start;
    (*pahTiles)[0].Allocated  = pTileDir->First.Allocated;
    (*pahTiles)[0].Used       = pTileDir->First.Used;

    if( nTiles > 1 &&
      ( VSIFReadL( &((*pahTiles)[1]), nTiles - 1, SIZEOF_TILE, fp ) == 0 ) )
    {
        CPLDebug("INGR", "Error reading tiles table");
        return 1;
    }

    return nTiles;
}

// -----------------------------------------------------------------------------
//                                                          INGR_GetIGDSColors()
// -----------------------------------------------------------------------------

void CPL_STDCALL INGR_GetIGDSColors( FILE *fp,
                                     uint32 nOffset,
                                     uint32 nEntries,
                                     GDALColorTable *poColorTable )
{
    if( fp == NULL ||
        nOffset < 0 ||
        nEntries == 0 ||
        poColorTable == NULL )
    {
        return;
    }

    // -------------------------------------------------------------
    // Read it from the middle of the second block
    // -------------------------------------------------------------

    uint32 nStart = nOffset + SIZEOF_HDR1 + SIZEOF_HDR2_A;

    INGR_ColorTable256 hIGDSColors;

    if( ( VSIFSeekL( fp, nStart, SEEK_SET ) == -1 ) ||
        ( VSIFReadL( hIGDSColors.Entry, nEntries, SIZEOF_IGDS, fp ) == 0 ) )
    {
        return;
    }

    // -------------------------------------------------------------
    // Read it to poColorTable
    // -------------------------------------------------------------

    GDALColorEntry oEntry;
    unsigned int i;

    for( i = 0; i < nEntries; i++ )
    {
        oEntry.c1 = (short) hIGDSColors.Entry[i].v_red;
        oEntry.c2 = (short) hIGDSColors.Entry[i].v_green;
        oEntry.c2 = (short) hIGDSColors.Entry[i].v_blue;
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

void INGR_GetEnvironVColors( FILE *fp,
                             uint32 nOffset,
                             uint32 nEntries,
                             GDALColorTable *poColorTable )
{
    if( fp == NULL ||
        nOffset < 0 ||
        nEntries == 0 ||
        poColorTable == NULL )
    {
        return;
    }

    // -------------------------------------------------------------
    // Read it from the third block
    // -------------------------------------------------------------

    uint32 nStart = nOffset + SIZEOF_HDR1 + SIZEOF_HDR2;

    INGR_ColorTableVar hVLTColors;

    hVLTColors.Entry = (vlt_slot*) CPLCalloc( nEntries, SIZEOF_VLTS );

    if( ( VSIFSeekL( fp, nStart, SEEK_SET ) == -1 ) ||
        ( VSIFReadL( hVLTColors.Entry, nEntries, SIZEOF_VLTS, fp ) == 0 ) )
    {
        CPLFree( hVLTColors.Entry );
        return;
    }

    // -------------------------------------------------------------
    // Sort records by crescent order of "slot" 
    // -------------------------------------------------------------

    vlt_slot hSwapSlot;
    bool bContinue = true; // Inproved bubble sort.
    uint32 i;              // This is the best sort techique when you
    uint32 j;              // suspect that the data is already sorted

    for( j = 1; j < nEntries && bContinue; j++ )
    {
        bContinue = false;
        for( i = 0; i < nEntries - j; i++ )
        {
            if( hVLTColors.Entry[i].v_slot > 
                hVLTColors.Entry[i + 1].v_slot )
            {
                hSwapSlot = hVLTColors.Entry[i];
                hVLTColors.Entry[i]   = hVLTColors.Entry[i+1];
                hVLTColors.Entry[i+1] = hSwapSlot;
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

    for( i = 0; i < nEntries; i++ )
    {
        nMaxIndex = MAX( nMaxIndex, hVLTColors.Entry[i].v_slot );
        fMaxRed   = MAX( fMaxRed  , hVLTColors.Entry[i].v_red );
        fMaxGreen = MAX( fMaxGreen, hVLTColors.Entry[i].v_green );
        fMaxBlues = MAX( fMaxBlues, hVLTColors.Entry[i].v_blue );
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
        if( hVLTColors.Entry[i].v_slot == i )
        {
            oEntry.c1 = (short) ( hVLTColors.Entry[i].v_red   * fNormFactor );
            oEntry.c2 = (short) ( hVLTColors.Entry[i].v_green * fNormFactor );
            oEntry.c3 = (short) ( hVLTColors.Entry[i].v_blue  * fNormFactor );
            oEntry.c4 = (short) 255;
            poColorTable->SetColorEntry( i, &oEntry );
            j++;
        }
        else
        {
            poColorTable->SetColorEntry( i, &oNullEntry );
        }
    }

    CPLFree( hVLTColors.Entry );
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
//                                                       INGR_GetDataBlockSize()
// -----------------------------------------------------------------------------

uint32 CPL_STDCALL INGR_GetDataBlockSize( const char *pszFilename,
                                          uint32 nBandOffset,
                                          uint32 nDataOffset )
{
    if( nBandOffset == 0 )
    {
        // -------------------------------------------------------------
        // Until the end of the file
        // -------------------------------------------------------------

        VSIStatBuf  sStat;
        VSIStatL( pszFilename, &sStat );
        return sStat.st_size - nDataOffset;
    }
    else
    {
        // -------------------------------------------------------------
        // Until the end of the band
        // -------------------------------------------------------------

        return nBandOffset - nDataOffset;
    }
}

// -----------------------------------------------------------------------------
//                                                      INGR_CreateVirtualFile()
// -----------------------------------------------------------------------------

INGR_VirtualFile CPL_STDCALL INGR_CreateVirtualFile( const char *pszFilename,
                                         INGR_Format eFormat,
                                         int nXSize, 
                                         int nYSize,
                                         int nQuality,
                                         GByte *pabyBuffer,
                                         int nBufferSize,
                                         int nBand )
{
    INGR_VirtualFile hVirtual;

    hVirtual.pszFileName = CPLSPrintf( // "/vsimem/%s.virtual",
        "./%s.virtual", //TODO: remove it
        CPLGetBasename( pszFilename ) );

    int nJPGComponents = 1;

    switch( eFormat )
    {
    case JPEGCYMK:
        nJPGComponents = 4;
    case JPEGRGB: 
        nJPGComponents = 3;
    case JPEGGRAY:
        {
            GByte *pabyHeader = (GByte*) CPLCalloc( 1, 2048 );
            int nHeaderSize   = JPGHLP_HeaderMaker( pabyHeader,
                                                    nXSize,
                                                    nYSize,
                                                    nJPGComponents,
                                                    0,
                                                    nQuality );
            FILE *fp = VSIFOpenL( hVirtual.pszFileName, "w+" );
            VSIFWriteL( pabyHeader, 1, nHeaderSize, fp );
            VSIFWriteL( pabyBuffer, 1, nBufferSize, fp );
            VSIFCloseL( fp );
            CPLFree( pabyHeader );
            break;
        }
    case CCITTGroup4:
        {
            REVERSEBITSBUFFER( pabyBuffer, nBufferSize );
            TIFF *hTIFF = VSI_TIFFOpen( hVirtual.pszFileName, "w+" );
            TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH,      nXSize );
            TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH,     nYSize );
            TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,   1 );
            TIFFSetField( hTIFF, TIFFTAG_SAMPLEFORMAT,    SAMPLEFORMAT_UINT );
            TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG,    PLANARCONFIG_CONTIG );
            TIFFSetField( hTIFF, TIFFTAG_FILLORDER,       FILLORDER_MSB2LSB );
            TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP,    -1 );
            TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC,     PHOTOMETRIC_MINISWHITE );
            TIFFSetField( hTIFF, TIFFTAG_COMPRESSION,     COMPRESSION_CCITTFAX4 );
            TIFFWriteRawStrip( hTIFF, 0, pabyBuffer, nBufferSize );
            TIFFWriteDirectory( hTIFF );
            TIFFClose( hTIFF );
            break;
        }
    default:
        return hVirtual;
    }

    hVirtual.poDS   = (GDALDataset*) GDALOpen( hVirtual.pszFileName, GA_ReadOnly );

    if( hVirtual.poDS )
    {
        hVirtual.poBand = (GDALRasterBand*) GDALGetRasterBand( hVirtual.poDS, nBand );
    }

    return hVirtual;
}

// -----------------------------------------------------------------------------
//                                                            INGR_ReleaseTiff()
// -----------------------------------------------------------------------------

void CPL_STDCALL INGR_ReleaseTiff( INGR_VirtualFile *poTiffMem )
{
    delete poTiffMem->poDS;
//  VSIUnlink( poTiffMem->pszFileName );
}

// -----------------------------------------------------------------------------
//                                                            INGR_ReleaseTiff()
// -----------------------------------------------------------------------------

int CPL_STDCALL INGR_ReadJpegQuality( FILE *fp, uint32 nAppDataOfseet,
                                    uint32 nSeekLimit )
{
    if( nAppDataOfseet == 0  )
    {
        return INGR_JPEGQDEFAULT;
    }

    INGR_JPEGAppData hJpegData;
    uint32 nNext = nAppDataOfseet;

    do
    {
        if( ( VSIFSeekL( fp, nNext, SEEK_SET ) == -1 ) ||
            ( VSIFReadL( &hJpegData, 1, SIZEOF_JPGAD, fp ) == 0 ) )
        {
            return INGR_JPEGQDEFAULT;
        }

        nNext += hJpegData.RemainingLength;

        if( nNext > ( nSeekLimit - SIZEOF_JPGAD ) )
        {
            return INGR_JPEGQDEFAULT;
        }
    } 
    while( ! ( hJpegData.ApplicationType == 2 &&
        hJpegData.SubTypeCode == 12 ) );

    return hJpegData.JpegQuality;
}

// -----------------------------------------------------------------------------
//                                                     INGR_DecodeRunLenth()
// -----------------------------------------------------------------------------

int CPL_STDCALL INGR_DecodeRunLenth( GByte *pabySrcData, GByte *pabyDstData,
                                     uint32 nSrcBytes, uint32 nBlockSize )
{
    signed char cAtomHead;

    unsigned int nRun;
    unsigned int i; 
    unsigned int iInput;
    unsigned int iOutput;

    iInput = 0;
    iOutput = 0;

    do
    {
        cAtomHead = (char) pabySrcData[iInput++];

        if( cAtomHead > 0 )
        {
            nRun = cAtomHead;

            for( i = 0; i < nRun && iOutput < nBlockSize; i++ )
            {
                pabyDstData[iOutput++] = pabySrcData[iInput++];
            }
        }
        else if( cAtomHead < 0 )
        {
            nRun = abs( cAtomHead );

            for( i = 0; i < nRun && iOutput < nBlockSize; i++ )
            {
                pabyDstData[iOutput++] = pabySrcData[iInput];
            }
            iInput++;
        }
    }
    while( (iInput < nSrcBytes) && (iOutput < nBlockSize) );

    return iOutput;
}
