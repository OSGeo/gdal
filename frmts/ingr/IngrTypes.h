/*****************************************************************************
 * $Id: $
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Types, constants and functions definition
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

#ifndef INGR_TYPES_H_INCLUDED
#define INGR_TYPES_H_INCLUDED

#include "cpl_port.h"
#include "gdal.h"
#include "gdal_priv.h"

CPL_C_START
#include "jpeglib.h"
#include "jpegint.h"
CPL_C_END

//  ----------------------------------------------------------------------------
//    Magic number, identification and limits
//  ----------------------------------------------------------------------------

#define INGR_HEADER_TYPE     9
#define INGR_HEADER_VERSION  8
#define INGR_HEADER_2D       0
#define INGR_HEADER_3D       3
#define INGR_RSVC_MAX_NAME   32

//  ----------------------------------------------------------------------------
//    Data type convention
//  ----------------------------------------------------------------------------

typedef   signed char       byte;
typedef   signed char       int8;
typedef unsigned char       uint8;
typedef   signed short      int16;
typedef unsigned short      uint16;
typedef   signed int        int32;
typedef unsigned int        uint32;
typedef   signed long long  int64;
typedef unsigned long long  uint64;
typedef double              real64;
typedef float               real32;

//  ----------------------------------------------------------------------------
//    Header Element Type Word ( HTC )
//  ----------------------------------------------------------------------------

#pragma pack( 1 )

typedef struct {
    uint16 Version   : 6;        // ??????00 00000000 
    uint16 Is2Dor3D  : 2;        // 000000?? 00000000 
    uint16 Type      : 8;        // 00000000 ???????? 
} INGR_HeaderType;

//  ----------------------------------------------------------------------------
//    Data type dependent Minimum and Maximum type
//  ----------------------------------------------------------------------------

typedef union 
{
    uint8   AsUint8;          
    uint16  AsUint16;          
    uint32  AsUint32;          
    real32  AsReal32;          
    real64  AsReal64;
} INGR_MinMax;

//  ----------------------------------------------------------------------------
//    Raster Format Types
//  ----------------------------------------------------------------------------

//typedef enum : uint16 {
typedef enum {
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
    JPEGCYMK                         = 32,  // CYMK  
    TiledRasterData                  = 65,  // See tile directory Data Type Code (DTC)//
    NotUsedReserved                  = 66,   
    ContinuousTone                   = 67,  // CYMK  
    LineArt                          = 68   // CYMK/RGB  
} INGR_Format;

struct INGR_FormatDescription {
	INGR_Format       eFormatCode;
	char            *pszName;
    GDALDataType     eDataType;
};

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

#define FORMAT_TAB_COUNT (sizeof(INGR_FormatTable) / sizeof(INGR_FormatDescription))

//  ----------------------------------------------------------------------------
//    Raster Application Types
//  ----------------------------------------------------------------------------

//typedef enum : uint16 {
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

//typedef enum : uint8 {
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

static char *IngrOrientation[] = {
    "Upper Left Vertical",
    "Upper Right Vertical",
    "Lower Left Vertical",
    "Lower Right Vertical",
    "Upper Left Horizontal",
    "Upper Right Horizontal",
    "Lower Left Horizontal",
    "Lower Right Horizontal"};

//  ----------------------------------------------------------------------------
//    Scannable flag field codes
//  ----------------------------------------------------------------------------

//typedef enum : uint8 {
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

//typedef enum : uint16 {
typedef enum {
    NoColorTable                     = 0,
    IGDSColorTable                   = 1,
    EnvironVColorTable               = 2
} INGR_ColorTableType;

//  ----------------------------------------------------------------------------
//    Environ-V Color Tables Entrie
//  ----------------------------------------------------------------------------

struct vlt_slot
{
    uint16 v_slot;
    uint16 v_red;
    uint16 v_green;
    uint16 v_blue;
};

//  ----------------------------------------------------------------------------
//    IGDS Color Tables Entrie
//  ----------------------------------------------------------------------------

struct igds_slot
{
    uint8 v_red;
    uint8 v_green;
    uint8 v_blue;
};

//  ----------------------------------------------------------------------------
//    Header Block One data items
//  ----------------------------------------------------------------------------

typedef struct {
    INGR_HeaderType     HeaderType;                 
    uint16              WordsToFollow;              
    INGR_Format         DataTypeCode;               
    INGR_Application    ApplicationType;            
    real64              XViewOrigin;                
    real64              YViewOrigin;                
    real64              ZViewOrigin;                
    real64              XViewExtent;                
    real64              YViewExtent;                
    real64              ZViewExtent;                
    real64              TransformationMatrix[16];   
    uint32              PixelsPerLine;              
    uint32              NumberOfLines;              
    int16               DeviceResolution;           
    INGR_Orientation    ScanlineOrientation;        
    INGR_IndexingMethod ScannableFlag;              
    real64              RotationAngle;              
    real64              SkewAngle;                  
    uint16              DataTypeModifier;           
    byte                DesignFileName[66];         
    byte                DataBaseFileName[66];       
    byte                ParentGridFileName[66];     
    byte                FileDescription[80];        
    INGR_MinMax         Minimum;                    
    INGR_MinMax         Maximum;                    
    byte                Reserved[3];                
    uint8               GridFileVersion;            
} INGR_HeaderOne;

//  ----------------------------------------------------------------------------
//    Block two field descriptions
//  ----------------------------------------------------------------------------

typedef struct {
    uint8               Gain;                       
    uint8               OffsetThreshold;            
    uint8               View1;                      
    uint8               View2;                      
    uint8               ViewNumber;                 
    uint8               Reserved2;                  
    uint16              Reserved3;                  
    real64              AspectRatio;                
    uint32              CatenatedFilePointer;       
    INGR_ColorTableType ColorTableType;             
    uint16              Reserved8;                  
    uint32              NumberOfCTEntries;          
    uint32              ApplicationPacketPointer;   
    uint32              ApplicationPacketLength;    
    uint16              Reserved[110];              
} INGR_HeaderTwoA;

typedef    struct {
    uint16              ApplicationData[128];       
} INGR_HeaderTwoB;

//  ----------------------------------------------------------------------------
//    2nd half of Block two plus another block as a static 256 entries color table
//  ----------------------------------------------------------------------------

typedef    struct {
    igds_slot           Entry[256];
} INGR_ColorTable256;

//  ----------------------------------------------------------------------------
//    Extra Block( s ) for dynamic allocated color table with intensit level entries
//  ----------------------------------------------------------------------------

typedef    struct {
    vlt_slot           *Entry;
} INGR_ColorTableVar;

//  ----------------------------------------------------------------------------
//    Tile Directory Item
//  ----------------------------------------------------------------------------

typedef     struct {
    uint32              Start;
    uint32              Allocated;
    uint32              Used;
} INGR_TileItem;

//  ----------------------------------------------------------------------------
//    Tile Directory Header
//  ----------------------------------------------------------------------------

typedef    struct {
    uint16              ApplicationType;
    uint16              SubTypeCode;
    uint32              WordsToFollow;
    uint16              PacketVersion;
    uint16              Identifier;
    uint16              Reserved[2];
    uint16              Properties;
    INGR_Format         DataTypeCode;
    uint8               Reserved2[100];
    uint32              TileSize; 
    uint32              Reserved3;
    INGR_TileItem       First;
} INGR_TileHeader;

#pragma pack()

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

#define SIZEOF_HDR1     sizeof( INGR_HeaderOne )
#define SIZEOF_HDR2_A   sizeof( INGR_HeaderTwoA )
#define SIZEOF_HDR2_B   sizeof( INGR_HeaderTwoB )
#define SIZEOF_CTAB     sizeof( INGR_ColorTable256 )
#define SIZEOF_TDIR     sizeof( INGR_TileHeader )
#define SIZEOF_TILE     sizeof( INGR_TileItem )

//  ----------------------------------------------------------------------------
//                                 Functions
//  ----------------------------------------------------------------------------

//  ------------------------------------------------------------------
//    Compression, Data Format, Data Type related funtions
//  ------------------------------------------------------------------

const INGR_Format CPL_STDCALL INGR_GetFormat( GDALDataType eType, const char *pszCompression );
const char * CPL_STDCALL INGR_GetFormatName( uint16 eCode );
const GDALDataType CPL_STDCALL INGR_GetDataType( uint16 eCode );

//  ------------------------------------------------------------------
//    Transformation Matrix conversion
//  ------------------------------------------------------------------

void CPL_STDCALL INGR_GetTransMatrix( real64 *padfMatrix, double *padfGeoTransform );
void CPL_STDCALL INGR_SetTransMatrix( real64 *padfMatrix, double *padfGeoTransform );

//  ------------------------------------------------------------------
//    Color Table conversion
//  ------------------------------------------------------------------

void CPL_STDCALL INGR_GetIGDSColors( GDALColorTable *poColorTable, 
                                     INGR_ColorTable256 *pColorTableIGDS );
void CPL_STDCALL INGR_GetEnvironVColors( GDALColorTable *poColorTable,
                                        INGR_ColorTableVar *pEnvironTable,
                                        uint32 nColorsCount );
uint32 CPL_STDCALL INGR_SetEnvironColors( GDALColorTable *poColorTable,
                                          INGR_ColorTableVar *pEnvironTable );
uint32 CPL_STDCALL INGR_SetIGDSColors( GDALColorTable *poColorTable,
                                       INGR_ColorTable256 *pColorTableIGDS );

//  ------------------------------------------------------------------
//    Get, Set Min & Max
//  ------------------------------------------------------------------

INGR_MinMax CPL_STDCALL INGR_SetMinMax( GDALDataType eType, double dVal );
double CPL_STDCALL INGR_GetMinMax( GDALDataType eType, INGR_MinMax hVal );

//  ------------------------------------------------------------------
//    Decoders
//  ------------------------------------------------------------------

CPLErr DecodeRunLengthEncoded( GByte *pabySrcData, int nSrcBytes, 
                               int nBlockXSize, int nBlockYSize );

CPLErr DecodeCCITTGroup4( GByte *pabyCur, int nDataSize, int nMin,
                          int nBlockXSize, int nBlockYSize,
                          GByte *panData );

CPLErr DecodeAdaptiveRGB( GByte *pabySrcData, int nSrcBytes, 
                          int nBlockXSize, int nBlockYSize );

CPLErr DecodeAdaptiveGrayScale( GByte *pabySrcData, int nSrcBytes, 
                                int nBlockXSize, int nBlockYSize );

CPLErr DecodeContinuousTone( GByte *pabySrcData, int nSrcBytes, 
                             int nBlockXSize, int nBlockYSize );

CPLErr DecodeJPEG( GByte *pabySrcData, int nSrcBytes, 
                   int nBlockXSize, int nBlockYSize );

//  ------------------------------------------------------------------
//    JPEG stuff
//  ------------------------------------------------------------------

void jpeg_vsiio_src (j_decompress_ptr cinfo, FILE * infile);
void jpeg_vsiio_dest (j_compress_ptr cinfo, FILE * outfile);

/************************************************************************/
/*                      INGR_LoadJPEGTables()                        */
/************************************************************************/

const static int Q1table[256] = 
{
    8,    72,     72,     72,    72,    72,    72,    72,    72,    72,
    78,    74,     76,    74,    78,    89,    81,    84,    84,    81,
    89,    106,    93,    94,    99,    94,    93,    106,    129,    111,
    108,    116,    116,    108,    111,    129,    135,    128,    136,    
    145,    136,    128,    135,    155,    160,    177,    177,    160,
    155,    193,    213,    228,    213,    193,    255,    255,    255,    
    255
};

const static int Q2table[64] = 
{ 
    8, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 39, 37, 38, 37, 39, 45, 41, 42, 42, 41, 45, 53,
    47, 47, 50, 47, 47, 53, 65, 56, 54, 59, 59, 54, 56, 65, 68, 64, 69, 73,
    69, 64, 68, 78, 81, 89, 89, 81, 78, 98,108,115,108, 98,130,144,144,130,
    178,190,178,243,243,255
};

const static int Q3table[64] = 
{ 
     8, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 11, 10, 11, 10, 11, 13, 11, 12, 12, 11, 13, 15, 
    13, 13, 14, 13, 13, 15, 18, 16, 15, 16, 16, 15, 16, 18, 19, 18, 19, 21, 
    19, 18, 19, 22, 23, 25, 25, 23, 22, 27, 30, 32, 30, 27, 36, 40, 40, 36, 
    50, 53, 50, 68, 68, 91 
}; 

const static int Q4table[64] = 
{
    8, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 8, 7, 8, 7, 8, 9, 8, 8, 8, 8, 9, 11, 
    9, 9, 10, 9, 9, 11, 13, 11, 11, 12, 12, 11, 11, 13, 14, 13, 14, 15, 
    14, 13, 14, 16, 16, 18, 18, 16, 16, 20, 22, 23, 22, 20, 26, 29, 29, 26, 
    36, 38, 36, 49, 49, 65
};

const static int Q5table[64] = 
{
    4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 6, 5, 5, 6, 7, 6, 6, 6, 6, 6, 6, 7, 8, 7, 8, 8, 
    8, 7, 8, 9, 9, 10, 10, 9, 9, 11, 12, 13, 12, 11, 14, 16, 16, 14, 
    20, 21, 20, 27, 27, 36
};

static const int AC_BITS[16] = 
{ 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125 };

static const int AC_HUFFVAL[256] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,          
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
    0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
    0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
    0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
    0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
    0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const int DC_BITS[16] = 
{ 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };

static const int DC_HUFFVAL[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x08, 0x09, 0x0A, 0x0B };

void INGR_LoadJPEGTables( j_compress_ptr cinfo, int n, int nQLevel );


#endif
