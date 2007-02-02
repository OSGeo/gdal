/*****************************************************************************
 * $Id: $
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Types and constants definition
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
 *****************************************************************************
 *
 * $Log: $
 *
 */

#ifndef INGR_TYPES_H_INCLUDED
#define INGR_TYPES_H_INCLUDED

#include "cpl_port.h"
#include "gdal.h"
#include "gdal_priv.h"

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

typedef char                byte;
typedef char                int8;
typedef unsigned char       uint8;
typedef short               int16;
typedef unsigned short      uint16;
typedef int                 int32;
typedef unsigned int        uint32;
typedef long long           int64;
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
} IngrHeaderType;

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
} IngrMinMax;

//  ----------------------------------------------------------------------------
//    Raster Format Types
//  ----------------------------------------------------------------------------

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
    TiledRasterData                  = 65,  // ** NOT IN THE ORIGINAL TABLE ** //
    NotUsedReserved                  = 66,  // ** NOT IN THE ORIGINAL TABLE ** // 
    ContinuousTone                   = 67,  // CYMK  
    LineArt                          = 68   // CYMK/RGB  
} IngrFormatType;

struct hIngrFormatType {
	IngrFormatType   eFormatCode;
	char            *pszName;
    GDALDataType     eDataType;
};

static hIngrFormatType ahIngrFormatTypeTab[] = {
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

#define FORMAT_TYPE_COUNT (sizeof(ahIngrFormatTypeTab) / sizeof(hIngrFormatType))

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
} IngrApplicationType;

//  ----------------------------------------------------------------------------
//    Scan line orientation codes
//  ----------------------------------------------------------------------------

typedef enum {
    UpperLeftHorizontal              = 4,
    UpperLeftVertical                = 0,
    UpperRightHorizontal             = 5,
    UpperRightVertical               = 1,
    LowerLeftHorizontal              = 6,
    LowerLeftVertical                = 2,
    LowerRightHorizontal             = 7,
    LowerRightVertical               = 3
} IngrOriginOrientation;

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
} IngrIndexingMethod;


//  ----------------------------------------------------------------------------
//    Color Table Values ( CTV )
//  ----------------------------------------------------------------------------

typedef enum {
    NoColorTable                     = 0,
    IGDSColorTable                   = 1,
    EnvironVColorTable               = 2
} IngrColorTableType;

//  ----------------------------------------------------------------------------
//    Environ-V Color Tables Entrie
//  ----------------------------------------------------------------------------

struct vlt_slot
{
    uint16  v_slot;
    uint16  v_red;
    uint16  v_green;
    uint16  v_blue;
};

//  ----------------------------------------------------------------------------
//    IGDS Color Tables Entrie
//  ----------------------------------------------------------------------------

struct igds_slot
{
    uint8  v_red;
    uint8  v_green;
    uint8  v_blue;
};

//  ----------------------------------------------------------------------------
//    RSvc_entry Structure
//  ----------------------------------------------------------------------------

struct RSvc_entry
{
    uint8   type;
    uint8   flags;
    uint16  rgb[3];
    uint16  c[4];
    uint8   spot[12];
    int32   trans_flag;
    uint8   spot_index;
    uint8   res2;
    uint16  res3;
    uint32  res4;
    uint8   name[INGR_RSVC_MAX_NAME];
};

//  ----------------------------------------------------------------------------
//    RSptclsub14_Data Format
//  ----------------------------------------------------------------------------

struct RSptclsub14_data
{
    uint16  version;
    uint16  num_colors;
    uint64  res1;
    uint64  colors;
};

//  ----------------------------------------------------------------------------
//    Header Block One data items
//  ----------------------------------------------------------------------------

typedef    struct {
    IngrHeaderType  HeaderType;                 
    uint16          WordsToFollow;              
    uint16          DataTypeCode;               
    uint16          ApplicationType;            
    real64          XViewOrigin;                
    real64          YViewOrigin;                
    real64          ZViewOrigin;                
    real64          XViewExtent;                
    real64          YViewExtent;                
    real64          ZViewExtent;                
    real64          TransformationMatrix[16];   
    uint32          PixelsPerLine;              
    uint32          NumberOfLines;              
    int16           DeviceResolution;           
    uint8           ScanlineOrientation;        
    uint8           ScannableFlag;              
    real64          RotationAngle;              
    real64          SkewAngle;                  
    uint16          DataTypeModifier;           
    byte            DesignFileName[66];         
    byte            DataBaseFileName[66];       
    byte            ParentGridFileName[66];     
    byte            FileDescription[80];        
    IngrMinMax      Minimum;                    
    IngrMinMax      Maximum;                    
    byte            Reserved[3];                
    uint8           GridFileVersion;            
} IngrHeaderOne;

//  ----------------------------------------------------------------------------
//    Block two field descriptions
//  ----------------------------------------------------------------------------

typedef    struct {
    uint8           Gain;                       
    uint8           OffsetThreshold;            
    uint8           View1;                      
    uint8           View2;                      
    uint8           ViewNumber;                 
    uint8           Reserved2;                  
    uint16          Reserved3;                  
    real64          AspectRatio;                
    uint32          CatenatedFilePointer;       
    uint16          ColorTableType;             
    uint16          Reserved8;                  
    uint32          NumberOfCTEntries;          
    uint32          ApplicationPacketPointer;   
    uint32          ApplicationPacketLength;    
    uint16          Reserved[110];              
} IngrHeaderTwoA;

typedef    struct {
    uint16          ApplicationData[128];       
} IngrHeaderTwoB;

//  ----------------------------------------------------------------------------
//    2nd half of Block two plus another block as a static 256 entries color table
//  ----------------------------------------------------------------------------

typedef    struct {
    igds_slot       Entry[256];
} IngrColorTable;

//  ----------------------------------------------------------------------------
//    Extra Block( s ) for dynamic allocated color table with intensit level entries
//  ----------------------------------------------------------------------------

typedef    struct {
    vlt_slot       *Entry;
} IngrEnvVTable;

//  ----------------------------------------------------------------------------
//    TILE DIRECTORY FORMAT
//  ----------------------------------------------------------------------------

typedef     struct {
    uint32          Start;
    uint32          Allocated;
    uint32          Used;
} IngrOneTile;

typedef    struct {
    uint16          ApplicationType;
    uint16          SubTypeCode;
    uint32          WordsToFollow;
    uint16          PacketVersion;
    uint16          Identifier;
    uint16          Reserved[2];
    uint16          Properties;
    uint16          DataTypeCode;
    uint8           Reserved2[100];
    uint32          TileSize; 
    uint32          Reserved3;
    IngrOneTile     First;
} IngrTileDir;

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
            +-------------+  -  512  -  Header Block Two
            |     256     |             ( First Half )
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
            +-------------+  ( n+2 )x512  Extra Header Info or Image Data  
            |     ...     |

*/

#define SIZEOF_HDR1     sizeof( IngrHeaderOne )
#define SIZEOF_HDR2_A   sizeof( IngrHeaderTwoA )
#define SIZEOF_HDR2_B   sizeof( IngrHeaderTwoB )
#define SIZEOF_CTAB     sizeof( IngrColorTable )
#define SIZEOF_TDIR     sizeof( IngrTileDir )
#define SIZEOF_TILE     sizeof( IngrOneTile )

//  ----------------------------------------------------------------------------
//                                 Functions
//  ----------------------------------------------------------------------------

//  ------------------------------------------------------------------
//    Compression, Data Format, Data Type related funtions
//  ------------------------------------------------------------------

const IngrFormatType GetFormatCodeFromName( GDALDataType eType, const char *pszCompression );
const char * CPL_STDCALL GetFormatNameFromCode( uint16 eCode );
const GDALDataType CPL_STDCALL GetDataTypeForFormat( uint16 eCode );

//  ------------------------------------------------------------------
//    Transformation Matrix conversion
//  ------------------------------------------------------------------

void CPL_STDCALL GetTransforMatrix( real64 *padfMatrix, double *padfGeoTransform );
void CPL_STDCALL SetTransforMatrix( real64 *padfMatrix, double *padfGeoTransform );

//  ------------------------------------------------------------------
//    Color Table conversion
//  ------------------------------------------------------------------

void CPL_STDCALL GetIGDSColorTable( GDALColorTable *poColorTable, 
                                    IngrColorTable *pColorTableIGDS );
void CPL_STDCALL GetEnvironColorTable( GDALColorTable *poColorTable,
                                       IngrEnvVTable *pEnvironTable,
                                       uint32 nColorsCount );
uint32 CPL_STDCALL SetEnvironColorTable( GDALColorTable *poColorTable,
                                         IngrEnvVTable *pEnvironTable );
uint32 CPL_STDCALL SetIGDSColorTable( GDALColorTable *poColorTable,
                                      IngrColorTable *pColorTableIGDS );

//  ------------------------------------------------------------------
//    Miselaneous
//  ------------------------------------------------------------------

IngrMinMax CPL_STDCALL SetMinMax( GDALDataType eType, double dVal );
double CPL_STDCALL GetMinMax( GDALDataType eType, IngrMinMax hVal );
const char *GetOrientationName( uint8 eType );
const char *GetApplicationTypeName( uint8 eType );

//  ------------------------------------------------------------------
//    Decoder
//  ------------------------------------------------------------------

CPLErr DecodePackedBinary( GByte *pabyDst, 
                           int nDataSize, 
                           int nBlockXSize, 
                           int nBlockYSize,
                           GByte *pabySrc );

CPLErr DecodeRunLengthEncoded( GByte *pabyDst, 
                               int nDataSize, 
                               int nBlockXSize, 
                               int nBlockYSize,
                               GByte *pabySrc );

CPLErr DecodeCCITTGroup4( GByte *pabyDst, 
                          int nDataSize, 
                          int nBlockXSize, 
                          int nBlockYSize,
                          GByte *pabySrc );

CPLErr DecodeAdaptiveRGB( GByte *pabyDst,  
                          int nDataSize, 
                          int nBlockXSize, 
                          int nBlockYSize,
                          GByte *pabySrc );

CPLErr DecodeAdaptiveGrayScale( GByte *pabyDst, 
                                int nDataSize, 
                                int nBlockXSize, 
                                int nBlockYSize,
                                GByte *pabySrc );

CPLErr DecodeContinuousTone( GByte *pabyDst, 
                             int nDataSize, 
                             int nBlockXSize, 
                             int nBlockYSize,
                             GByte *pabySrc );

CPLErr DecodeJPEG( GByte *pabyDst, 
                   int nDataSize, 
                   int nBlockXSize, 
                   int nBlockYSize,
                   GByte *pabySrc );

#endif
