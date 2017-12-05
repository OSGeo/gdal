/*****************************************************************************
 *
 * Project:  Creates a jpeg header
 * Purpose:  Abbreviated JPEG support
 * Author:   Ivan Lucena, [lucena_ivan at hotmail.com]
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
 *
 *****************************************************************************/

#include "JpegHelper.h"

CPL_CVSID("$Id$")

static const GByte JPGHLP_1DC_Codes[] = {
    0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
};

static const GByte JPGHLP_1AC_Codes[] = {
    0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125,
};

static const GByte JPGHLP_1DC_Symbols[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static const GByte JPGHLP_1AC_Symbols[] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xCA, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa,
};

static const GByte JPGHLP_2AC_Codes[] = {
    0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119,
};

static const GByte JPGHLP_2DC_Codes[] = {
    0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};

static const GByte JPGHLP_2DC_Symbols[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static const GByte JPGHLP_2AC_Symbols[] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
    0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xCA, 0xd2,
    0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
    0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa,
};

static const GByte JPGHLP_DQT_luminace[64] = {
     10,  7,  6, 10, 14, 24, 31, 37,
      7,  7,  8, 11, 16, 35, 36, 33,
      8,  8, 10, 14, 24, 34, 41, 34,
      8, 10, 13, 17, 31, 52, 48, 37,
     11, 13, 22, 34, 41, 65, 62, 46,
     14, 21, 33, 38, 49, 62, 68, 55,
     29, 38, 47, 52, 62, 73, 72, 61,
     43, 55, 57, 59, 67, 60, 62, 59
};

static const GByte JPGHLP_DQT_chrominance[64] = {
     10, 11, 14, 28, 59, 59, 59, 59,
     11, 13, 16, 40, 59, 59, 59, 59,
     14, 16, 34, 59, 59, 59, 59, 59,
     28, 40, 59, 59, 59, 59, 59, 59,
     59, 59, 59, 59, 59, 59, 59, 59,
     59, 59, 59, 59, 59, 59, 59, 59,
     59, 59, 59, 59, 59, 59, 59, 59,
     59, 59, 59, 59, 59, 59, 59, 59
};

static const GByte ZIGZAG[64] = {
      0,  1,  5,  6, 14, 15, 27, 28,
      2,  4,  7, 13, 16, 26, 29, 42,
      3,  8, 12, 17, 25, 30, 41, 43,
      9, 11, 18, 24, 31, 40, 44, 53,
     10, 19, 23, 32, 39, 45, 52, 54,
     20, 22, 33, 38, 46, 51, 55, 60,
     21, 34, 37, 47, 50, 56, 59, 61,
     35, 36, 48, 49, 57, 58, 62, 63
};

#define ZIGZAGCPY(ou, in) \
  { int i; for( i = 0; i < 64; i++ ) ou[ZIGZAG[i]] = in[i]; }

#define ADJUST(tb, op, vl) \
  { int i; for( i = 0; i < 64; i++ ) tb[i] = (GByte) (tb[i] op vl); }

int JPGHLP_HeaderMaker( GByte *pabyBuffer,
                        const int nCols,
                        const int nRows,
                        const int nComponents,
                        CPL_UNUSED const int nRestart,
                        const int nQuality )
{
    GByte *pabNext = pabyBuffer;

    // ------------------------------------------------------------------------
    // Start of Image
    // ------------------------------------------------------------------------

    *( pabNext++ )      = 0xFF;         // Tag Mark
    *( pabNext++ )      = 0xD8;         // SOI

    // ------------------------------------------------------------------------
    // Application Segment
    // ------------------------------------------------------------------------

    *( pabNext++ )      = 0xFF;         // Tag Mark
    *( pabNext++ )      = 0xE0;         // APP0
    *( pabNext++ )      = 0x00;         // Segment Length (msb)
    *( pabNext++ )      = 0x10;         // Segment Length (lsb)
    *( pabNext++ )      = 0x4a;         // 'J'
    *( pabNext++ )      = 0x46;         // 'F'
    *( pabNext++ )      = 0x49;         // 'I'
    *( pabNext++ )      = 0x46;         // 'F'
    *( pabNext++ )      = 0x00;         // '\0'
    *( pabNext++ )      = 0x01;         // Version 1
    *( pabNext++ )      = 0x01;         // Sub Version 1
    *( pabNext++ )      = 0x00;         // Pixels per inch, 4 Bits for X, 4 Bits for Y
    *( pabNext++ )      = 0x00;         // Horizontal Pixel Density (msb)
    *( pabNext++ )      = 0x01;         // Horizontal Pixel Density (lsb)
    *( pabNext++ )      = 0x00;         // Vertical Pixel Density (msb)
    *( pabNext++ )      = 0x01;         // Vertical Pixel Density (lsb)
    *( pabNext++ )      = 0x00;         // Thumbnail Width
    *( pabNext++ )      = 0x00;         // Thumbnail Height

    // ------------------------------------------------------------------------
    // Quantization Table Segment
    // ------------------------------------------------------------------------

    GByte abQuantTables[2][64];
    ZIGZAGCPY( abQuantTables[0], JPGHLP_DQT_luminace );
    ZIGZAGCPY( abQuantTables[1], JPGHLP_DQT_chrominance );

    if( nQuality == 30 )
    {
        ADJUST( abQuantTables[0], *, 0.5 );
        ADJUST( abQuantTables[1], *, 0.5 );
    }

    for( int i = 0; i < 2 && i < nComponents; i++ )
    {
        *( pabNext++ )  = 0xFF;         // Tag Mark
        *( pabNext++ )  = 0xDB;         // DQT
        *( pabNext++ )  = 0;            // Segment Length (msb)
        *( pabNext++ )  = 67;           // Length (msb)
        *( pabNext++ )  = (GByte) i;    // Table ID
        memcpy( pabNext, abQuantTables[i], 64 );
        pabNext += 64;
    }

    // ------------------------------------------------------------------------
    // Start Of Frame Segment
    // ------------------------------------------------------------------------

    *( pabNext++ )      = 0xFF;
    *( pabNext++ )      = 0xC0;         // SOF
    *( pabNext++ )      = 0;            // Segment Length (msb)
    if ( nComponents > 1 )
        *( pabNext++ )  = 17;           // Segment Length (lsb)
    else
        *( pabNext++ )  = 11;           // Segment Length (lsb)
    *( pabNext++ )      = 8;            // 8-bit Precision
    *( pabNext++ )      = (GByte) (nRows >> 8); // Height in rows (msb)
    *( pabNext++ )      = (GByte) nRows;// Height in rows (lsb)
    *( pabNext++ )      = (GByte) (nCols >> 8); // Width in columns (msb)
    *( pabNext++ )      = (GByte) nCols;// Width in columns (lsb)
    *( pabNext++ )      = (GByte) nComponents;// Number of components
    *( pabNext++ )      = 0;            // Component ID
    *( pabNext++ )      = 0x21;         // Hozontal/Vertical Sampling
    *( pabNext++ )      = 0;            // Quantization table ID
    if ( nComponents > 1 )
    {
        *( pabNext++ )  = 1;            // Component ID
        *( pabNext++ )  = 0x11;         // Hozontal/Vertical Sampling
        *( pabNext++ )  = 1;            // Quantization table ID
        *( pabNext++ )  = 2;            // Component ID
        *( pabNext++ )  = 0x11;         // Hozontal/Vertical Sampling
        *( pabNext++ )  = 1;            // Quantization table ID
    }

    // ------------------------------------------------------------------------
    // Huffman Table Segments
    // ------------------------------------------------------------------------

    const GByte *pabHuffTab[2][4];
    pabHuffTab[0][0]    = JPGHLP_1DC_Codes;
    pabHuffTab[0][1]    = JPGHLP_1AC_Codes;
    pabHuffTab[0][2]    = JPGHLP_1DC_Symbols;
    pabHuffTab[0][3]    = JPGHLP_1AC_Symbols;

    pabHuffTab[1][0]    = JPGHLP_2DC_Codes;
    pabHuffTab[1][1]    = JPGHLP_2AC_Codes;
    pabHuffTab[1][2]    = JPGHLP_2DC_Symbols;
    pabHuffTab[1][3]    = JPGHLP_2AC_Symbols;

    int pnHTs[2][4];
    pnHTs[0][0]         = sizeof(JPGHLP_1DC_Codes);
    pnHTs[0][1]         = sizeof(JPGHLP_1AC_Codes);
    pnHTs[0][2]         = sizeof(JPGHLP_1DC_Symbols);
    pnHTs[0][3]         = sizeof(JPGHLP_1AC_Symbols);

    pnHTs[1][0]         = sizeof(JPGHLP_2DC_Codes);
    pnHTs[1][1]         = sizeof(JPGHLP_2AC_Codes);
    pnHTs[1][2]         = sizeof(JPGHLP_2DC_Symbols);
    pnHTs[1][3]         = sizeof(JPGHLP_2AC_Symbols);

    for( int i = 0; i < 2 && i < nComponents; i++ )
    {
        for( int j = 0; j < 2; j++ )
        {
            const int k = j + 2;
            const int nCodes  = pnHTs[i][j];
            const int nSymbols = pnHTs[i][k];
            *( pabNext++ ) = 0xFF;                  // Tag Mark
            *( pabNext++ ) = 0xc4;                  // DHT
            *( pabNext++ ) = 0;                     // Segment Length (msb)
            *( pabNext++ ) = (GByte) (3 + nCodes + nSymbols); // Segment Length (lsb)
            *( pabNext++ ) = (GByte) ((j << 4) | i);          // Table ID
            memcpy( pabNext, pabHuffTab[i][j], nCodes );
            pabNext += nCodes;
            memcpy( pabNext, pabHuffTab[i][k], nSymbols );
            pabNext += nSymbols;
        }
    }

    // ------------------------------------------------------------------------
    // Start Of Scan Segment
    // ------------------------------------------------------------------------

    *( pabNext++ )      = 0xFF;         // Tag Mark
    *( pabNext++ )      = 0xDA;         // SOS
    if (nComponents > 1 )
    {
        *( pabNext++ )  = 0;            // Segment Length (msb)
        *( pabNext++ )  = 12;           // Segment Length (lsb)
        *( pabNext++ )  = 3;            // Number of components
        *( pabNext++ )  = 0;            // Components 0
        *( pabNext++ )  = 0;            // Huffman table ID
        *( pabNext++ )  = 1;            // Components 1
        *( pabNext++ )  = 0x11;         // Huffman table ID
        *( pabNext++ )  = 2;            // Components 2
        *( pabNext++ )  = 0x11;         // Huffman table ID
    }
    else
    {
        *( pabNext++ )  = 0;            // Segment Length (msb)
        *( pabNext++ )  = 8;            // Segment Length (lsb)
        *( pabNext++ )  = 1;            // Number of components
        *( pabNext++ )  = 0;            // Components 0
        *( pabNext++ )  = 0;            // Huffman table ID
    }
    *( pabNext++ )      = 0;            // First DCT coefficient
    *( pabNext++ )      = 63;           // Last DCT coefficient
    *( pabNext++ )      = 0;            // Spectral selection

    return static_cast<int>(pabNext - pabyBuffer);
}
