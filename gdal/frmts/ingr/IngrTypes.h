/*****************************************************************************
 * $Id$
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Types, constants and functions definition
 * Author:   Ivan Lucena, [lucena_ivan at hotmail.com]
 *
 ******************************************************************************
 * Copyright (c) 2007, Ivan Lucena
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef INGR_TYPES_H_INCLUDED
#define INGR_TYPES_H_INCLUDED

#include "cpl_conv.h"
#include "cpl_port.h"
#include "gdal.h"
#include "gdal_priv.h"
#include <cmath>
#include <cstdint>

CPL_C_START
#include "tiffio.h"
CPL_C_END

//  ----------------------------------------------------------------------------
//    Magic number, identification and limits
//  ----------------------------------------------------------------------------

#define INGR_HEADER_TYPE     9
#define INGR_HEADER_VERSION  8
#define INGR_HEADER_2D       0
#define INGR_HEADER_3D       3
#define INGR_RSVC_MAX_NAME   32
#define INGR_JPEGQDEFAULT    30

//  ----------------------------------------------------------------------------
//    Data type convention
//  ----------------------------------------------------------------------------

typedef double              real64;
typedef float               real32;

//  ----------------------------------------------------------------------------
//    Header Element Type Word ( HTC )
//  ----------------------------------------------------------------------------

typedef struct {
#if defined(CPL_LSB)
    uint16_t Version   : 6;        // ??????00 00000000
    uint16_t Is2Dor3D  : 2;        // 000000?? 00000000
#else
    uint16_t Is2Dor3D  : 2;        // 000000?? 00000000
    uint16_t Version   : 6;        // ??????00 00000000
#endif
    uint16_t Type      : 8;        // 00000000 ????????
} INGR_HeaderType;

//  ----------------------------------------------------------------------------
//    Data type dependent Minimum and Maximum type
//  ----------------------------------------------------------------------------

typedef union
{
    uint8_t   AsUint8;
    uint16_t  AsUint16;
    uint32_t  AsUint32;
    real32  AsReal32;
    real64  AsReal64;
} INGR_MinMax;

//  ----------------------------------------------------------------------------
//    Raster Format Types
//  ----------------------------------------------------------------------------

typedef enum {
    IngrUnknownFrmt                  = 0,
    PackedBinary                     = 1,   // 1 bit / pixel
    ByteInteger                      = 2,   // 8 bits / pixel
    WordIntegers                     = 3,   // 16 bits / pixel
    Integers32Bit                    = 4,
    FloatingPoint32Bit               = 5,
    FloatingPoint64Bit               = 6,
    Complex                          = 7,   // 64 bits / pixel
    DoublePrecisionComplex           = 8,
    RunLengthEncoded                 = 9,   // Bi-level Images
    RunLengthEncodedC                = 10,  // Gray Scale, Color
    FigureOfMerit                    = 11,  // FOM
    DTMFlags                         = 12,
    RLEVariableValuesWithZS          = 13,  // Simple
    RLEBinaryValues                  = 14,  // w/ Edge Type
    RLEVariableValues                = 15,  // w/ Edge Type
    RLEVariableValuesWithZ           = 16,  // w/ Edge Type
    RLEVariableValuesC               = 17,  // Color Table and Shade
    RLEVariableValuesN               = 18,  // w/ Normals
    QuadTreeEncoded                  = 19,
    CCITTGroup4                      = 24,  // Bi-level Images
    RunLengthEncodedRGB              = 25,  // Full Color
    VariableRunLength                = 26,
    AdaptiveRGB                      = 27,  // Full Color
    Uncompressed24bit                = 28,  // Full Color
    AdaptiveGrayScale                = 29,
    JPEGGRAY                         = 30,  // Gray Scale
    JPEGRGB                          = 31,  // Full Color RGB
    JPEGCMYK                         = 32,  // CMYK
    TiledRasterData                  = 65,  // See tile directory Data Type Code (DTC)
    NotUsedReserved                  = 66,
    ContinuousTone                   = 67,  // CMYK
    LineArt                          = 68   // CMYK/RGB
} INGR_Format;

struct INGR_FormatDescription {
    INGR_Format      eFormatCode;
    const char       *pszName;
    GDALDataType     eDataType;
};

#define FORMAT_TAB_COUNT 32

//  ----------------------------------------------------------------------------
//    Raster Application Types
//  ----------------------------------------------------------------------------

typedef enum {
    GenericRasterImageFile           = 0,
    DigitalTerrainModeling           = 1,
    GridDataUtilities                = 2,
    DrawingScanning                  = 3,
    ImageProcessing                  = 4,
    HiddenSurfaces                   = 5,
    ImagitexScannerProduct           = 6,
    ScreenCopyPlotting               = 7,
    IMAGEandMicroStationImager       = 8,
    ModelView                        = 9
} INGR_Application;

//  ----------------------------------------------------------------------------
//    Scan line orientation codes
//  ----------------------------------------------------------------------------

typedef enum {
    UpperLeftVertical                = 0,
    UpperRightVertical               = 1,
    LowerLeftVertical                = 2,
    LowerRightVertical               = 3,
    UpperLeftHorizontal              = 4,
    UpperRightHorizontal             = 5,
    LowerLeftHorizontal              = 6,
    LowerRightHorizontal             = 7
} INGR_Orientation;

//  ----------------------------------------------------------------------------
//    Scannable flag field codes
//  ----------------------------------------------------------------------------

typedef enum {
    HasLineHeader                    = 1,
    // Every line of raster data has a 4 word
    // raster line header at the beginning of
    // the line. In the line header, the Words
    // to Follow field specifies the amount
    // of data following the field, indicating
    // the start of the next scanline of raster
    // data
    NoLineHeader                     = 0
    // No raster line headers exist. The application
    // must calculate where lines of raster data
    // start and end. This process is simple for
    // non-run length encoded data. It is a fixed
    // value; therefore, the line length can be
    // calculated from the data type ( DTC ) and
    // the number of pixels per line ( PPL ). In
    // a run length compression case, the data must
    // be decoded to find the end of a raster line.
} INGR_IndexingMethod;

//  ----------------------------------------------------------------------------
//    Color Table Values ( CTV )
//  ----------------------------------------------------------------------------

typedef enum {
    NoColorTable                     = 0,
    IGDSColorTable                   = 1,
    EnvironVColorTable               = 2
} INGR_ColorTableType;

//  ----------------------------------------------------------------------------
//    Environ-V Color Tables Entry
//  ----------------------------------------------------------------------------

struct vlt_slot
{
    uint16_t v_slot;
    uint16_t v_red;
    uint16_t v_green;
    uint16_t v_blue;
};

//  ----------------------------------------------------------------------------
//    IGDS Color Tables Entry
//  ----------------------------------------------------------------------------

struct igds_slot
{
    uint8_t v_red;
    uint8_t v_green;
    uint8_t v_blue;
};

//  ----------------------------------------------------------------------------
//    Header Block One data items
//  ----------------------------------------------------------------------------

typedef struct {
    INGR_HeaderType     HeaderType;
    uint16_t              WordsToFollow;
    uint16_t              DataTypeCode;
    uint16_t              ApplicationType;
    real64              XViewOrigin;
    real64              YViewOrigin;
    real64              ZViewOrigin;
    real64              XViewExtent;
    real64              YViewExtent;
    real64              ZViewExtent;
    real64              TransformationMatrix[16];
    uint32_t              PixelsPerLine;
    uint32_t              NumberOfLines;
    int16_t               DeviceResolution;
    uint8_t               ScanlineOrientation;
    uint8_t               ScannableFlag;
    real64              RotationAngle;
    real64              SkewAngle;
    uint16_t              DataTypeModifier;
    char                DesignFileName[66];
    char                DataBaseFileName[66];
    char                ParentGridFileName[66];
    char                FileDescription[80];
    INGR_MinMax         Minimum;
    INGR_MinMax         Maximum;
    char                Reserved[3];
    uint8_t               GridFileVersion;
} INGR_HeaderOne;

//  ----------------------------------------------------------------------------
//    Block two field descriptions
//  ----------------------------------------------------------------------------

typedef struct {
    uint8_t               Gain;
    uint8_t               OffsetThreshold;
    uint8_t               View1;
    uint8_t               View2;
    uint8_t               ViewNumber;
    uint8_t               Reserved2;
    uint16_t              Reserved3;
    real64              AspectRatio;
    uint32_t              CatenatedFilePointer;
    uint16_t              ColorTableType;
    uint16_t              Reserved8;
    uint32_t              NumberOfCTEntries;
    uint32_t              ApplicationPacketPointer;
    uint32_t              ApplicationPacketLength;
    uint16_t              Reserved[110];
} INGR_HeaderTwoA;

typedef    struct {
    uint16_t              ApplicationData[128];
} INGR_HeaderTwoB;

//  ----------------------------------------------------------------------------
//    2nd half of Block two plus another block as a static 256 entries color table
//  ----------------------------------------------------------------------------

typedef    struct {
    igds_slot           Entry[256];
} INGR_ColorTable256;

//  ----------------------------------------------------------------------------
//    Extra Block(s) for dynamic allocated color table with intensity level entries.
//  ----------------------------------------------------------------------------

typedef    struct {
    vlt_slot           *Entry;
} INGR_ColorTableVar;

//  ----------------------------------------------------------------------------
//    Tile Directory Item
//  ----------------------------------------------------------------------------

typedef     struct {
    uint32_t              Start;
    uint32_t              Allocated;
    uint32_t              Used;
} INGR_TileItem;

//  ----------------------------------------------------------------------------
//    Tile Directory Header
//  ----------------------------------------------------------------------------

typedef struct INGR_TileHeader {
    INGR_TileHeader();
    uint16_t              ApplicationType;
    uint16_t              SubTypeCode;
    uint32_t              WordsToFollow;
    uint16_t              PacketVersion;
    uint16_t              Identifier;
    uint16_t              Reserved[2];
    uint16_t              Properties;
    uint16_t              DataTypeCode;
    uint8_t               Reserved2[100];
    uint32_t              TileSize;
    uint32_t              Reserved3;
    INGR_TileItem       First;
} INGR_TileHeader;

//  ----------------------------------------------------------------------------
//    In Memory Tiff holder
//  ----------------------------------------------------------------------------

typedef     struct {
    GDALDataset     *poDS;
    GDALRasterBand  *poBand;
    const char      *pszFileName;
} INGR_VirtualFile;

//  ----------------------------------------------------------------------------
//    Header Size
//  ----------------------------------------------------------------------------

/*
                   Headers Blocks without Color Table

            +-------------+  -  0    -  Header Block One
            |     512     |
            |             |
            +-------------+  -  512  -  Header Block Two ( First Half )
            |     256     |
            +-------------+  -  768  -  Header Block Two ( Second Half )
            |     256     |             ( Application Data )
            +-------------+  -  1024 -  Extra Header Info or Image Data
            |     ...     |

                   Headers Blocks with IGDS Color Table

            +-------------+  -  0    -  Header Block One
            |     512     |
            |             |
            +-------------+  -  512  -  Header Block Two ( First Half )
            |     256     |
            +-------------+  -  768  -  IGDS 256 Entries
            |     768     |             Color Table
            |             |
            |             |
            +-------------+  -  1536 -  Extra Header Info or Image Data
            |     ...     |

                   Headers Blocks with EnvironV Color Table

            +-------------+  -  0    -  Header Block One
            |     512     |
            |             |
            +-------------+  -  512  -  Header Block Two
            |     512     |
            |             |
            +-------------+  -  1024 -  EnvironV Color
            :   n x 512   :             Table
            :             :
            :             :
            +-------------+  ( n+2 )x512  - Extra Header Info or Image Data
            |     ...     |

*/

#define SIZEOF_HDR1     512
#define SIZEOF_HDR2_A   256
#define SIZEOF_HDR2_B   256
#define SIZEOF_HDR2     512
#define SIZEOF_CTAB     768
#define SIZEOF_TDIR     140
#define SIZEOF_TILE     12
#define SIZEOF_JPGAD    12
#define SIZEOF_VLTS     8
#define SIZEOF_IGDS     3

//  ----------------------------------------------------------------------------
//                                 Functions
//  ----------------------------------------------------------------------------

//  ------------------------------------------------------------------
//    Copied the DNG OGR Driver
//  ------------------------------------------------------------------

void   INGR_DGN2IEEEDouble(void * dbl);

//  ------------------------------------------------------------------
//    Compression, Data Format, Data Type related functions
//  ------------------------------------------------------------------

uint32_t CPL_STDCALL INGR_GetDataBlockSize( const char *pszFileName,
                                          uint32_t nBandOffset,
                                          uint32_t nDataOffset );

uint32_t CPL_STDCALL INGR_GetTileDirectory( VSILFILE *fp,
                                          uint32_t nOffset,
                                          int nBandXSize,
                                          int nBandYSize,
                                          INGR_TileHeader *pTileDir,
                                          INGR_TileItem **pahTiles);

INGR_Format CPL_STDCALL INGR_GetFormat( GDALDataType eType,
                                        const char *pszCompression );

const char * CPL_STDCALL INGR_GetFormatName( uint16_t eCode );

GDALDataType CPL_STDCALL INGR_GetDataType( uint16_t eCode );

const char * CPL_STDCALL INGR_GetOrientation( uint8_t nIndex );

//  ------------------------------------------------------------------
//    Transformation Matrix conversion
//  ------------------------------------------------------------------

void CPL_STDCALL INGR_GetTransMatrix( INGR_HeaderOne *pHeaderOne,
                                      double *padfGeoTransform );
void CPL_STDCALL INGR_SetTransMatrix( real64 *padfMatrix,
                                      double *padfGeoTransform );

//  ------------------------------------------------------------------
//    Color Table conversion
//  ------------------------------------------------------------------

void CPL_STDCALL INGR_GetIGDSColors( VSILFILE *fp,
                                     uint32_t nOffset,
                                     uint32_t nEntries,
                                     GDALColorTable *poColorTable );
uint32_t CPL_STDCALL INGR_SetIGDSColors( GDALColorTable *poColorTable,
                                       INGR_ColorTable256 *pColorTableIGDS );

void CPL_STDCALL INGR_GetEnvironVColors( VSILFILE *fp,
                                         uint32_t nOffset,
                                         uint32_t nEntries,
                                         GDALColorTable *poColorTable );
uint32_t CPL_STDCALL INGR_SetEnvironColors( GDALColorTable *poColorTable,
                                          INGR_ColorTableVar *pEnvironTable );

//  ------------------------------------------------------------------
//    Get, Set Min & Max
//  ------------------------------------------------------------------

INGR_MinMax CPL_STDCALL INGR_SetMinMax( GDALDataType eType, double dVal );
double CPL_STDCALL INGR_GetMinMax( GDALDataType eType, INGR_MinMax hVal );

//  ------------------------------------------------------------------
//    Run Length decoders
//  ------------------------------------------------------------------

int CPL_STDCALL
INGR_Decode( INGR_Format eFormat,
             GByte *pabySrcData, GByte *pabyDstData,
             uint32_t nSrcBytes, uint32_t nBlockSize,
             uint32_t *pnBytesConsumed );

int CPL_STDCALL
INGR_DecodeRunLength( const GByte *pabySrcData, GByte *pabyDstData,
                      uint32_t nSrcBytes, uint32_t nBlockSize,
                      uint32_t *pnBytesConsumed );

int CPL_STDCALL
INGR_DecodeRunLengthBitonal( GByte *pabySrcData, GByte *pabyDstData,
                             uint32_t nSrcBytes, uint32_t nBlockSize,
                             uint32_t *pnBytesConsumed );

int CPL_STDCALL
INGR_DecodeRunLengthBitonalTiled( GByte *pabySrcData, GByte *pabyDstData,
                                  uint32_t nSrcBytes, uint32_t nBlockSize,
                                  uint32_t *pnBytesConsumed );

int CPL_STDCALL
INGR_DecodeRunLengthPaletted( GByte *pabySrcData, GByte *pabyDstData,
                              uint32_t nSrcBytes, uint32_t nBlockSize,
                              uint32_t *pnBytesConsumed );

//  ------------------------------------------------------------------
//    GeoTiff in memory helper
//  ------------------------------------------------------------------

#include "tifvsi.h"

INGR_VirtualFile CPL_STDCALL INGR_CreateVirtualFile( const char *pszFilename,
                                                     INGR_Format eFormat,
                                                     int nXSize,
                                                     int nYSize,
                                                     int nTileSize,
                                                     int nQuality,
                                                     GByte *pabyBuffer,
                                                     int nBufferSize,
                                                     int nBand );

void CPL_STDCALL INGR_ReleaseVirtual( INGR_VirtualFile *poTiffMen );

int CPL_STDCALL INGR_ReadJpegQuality( VSILFILE *fp,
                          uint32_t nAppDataOfseet,
                          uint32_t nSeekLimit );

typedef     struct {
    uint16_t              ApplicationType;
    uint16_t              SubTypeCode;
    uint32_t              RemainingLength;
    uint16_t              PacketVersion;
    uint16_t              JpegQuality;
} INGR_JPEGAppData;

//  ------------------------------------------------------------------
//    Reverse Bit order for CCITT data
//  ------------------------------------------------------------------

#define REVERSEBITS(b)            (BitReverseTable[b])
#define REVERSEBITSBUFFER(bb, bz)           \
    int ibb;                                \
    for( ibb = 0; ibb < bz; ibb++ )         \
        bb[ibb] = REVERSEBITS( bb[ibb] )

//  ------------------------------------------------------------------
//    Struct reading helpers
//  ------------------------------------------------------------------

static inline void BUF2STRC_fct( const GByte* bb, unsigned int& nn, void* pDest, size_t nSize )
{
    memcpy( pDest, &bb[nn], nSize );
    nn += static_cast<unsigned int>(nSize);
}

#define BUF2STRC(bb, nn, ff)    BUF2STRC_fct(bb, nn, &ff, sizeof(ff))

static inline void STRC2BUF_fct( GByte* bb, unsigned int& nn, const void* pSrc, size_t nSize )
{
    memcpy( &bb[nn], pSrc, nSize );
    nn += static_cast<unsigned int>(nSize);
}

#define STRC2BUF(bb, nn, ff)    STRC2BUF_fct(bb, nn, &ff, sizeof(ff))

//  ------------------------------------------------------------------
//    Fix Endianness issues
//  ------------------------------------------------------------------

void CPL_STDCALL INGR_HeaderOneDiskToMem(INGR_HeaderOne* pHeaderOne, const GByte *pabyBuf);
void CPL_STDCALL INGR_HeaderOneMemToDisk(const INGR_HeaderOne* pHeaderOne, GByte *pabyBuf);
void CPL_STDCALL INGR_HeaderTwoADiskToMem(INGR_HeaderTwoA* pHeaderTwo, const GByte *pabyBuf);
void CPL_STDCALL INGR_HeaderTwoAMemToDisk(const INGR_HeaderTwoA* pHeaderTwo, GByte *pabyBuf);
void CPL_STDCALL INGR_TileHeaderDiskToMem(INGR_TileHeader* pTileHeader, const GByte *pabyBuf);
void CPL_STDCALL INGR_TileItemDiskToMem(INGR_TileItem* pTileItem, const GByte *pabyBuf);
void CPL_STDCALL INGR_JPEGAppDataDiskToMem(INGR_JPEGAppData* pJPEGAppData, const GByte *pabyBuf);

#endif
