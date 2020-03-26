/******************************************************************************************************************************
	MapInfo Pro Raster API

	Copyright © 1985-2016,Pitney Bowes Software Inc.
	All rights reserved.
******************************************************************************************************************************/

/*!	\file	APIDEF.h
	\brief	MapInfo Pro Raster C-API header definitions.

	This file contains definitions used by the MapInfo Pro Raster API.
	Copyright © 2014,Pitney Bowes Software Inc.
	All rights reserved.
*/

#ifndef APIDEF_H
#define APIDEF_H

#pragma once

#include <cstdint>
#ifdef __linux__
#include <time.h>
#endif
#include "APICodes.h"
#ifdef __cplusplus
extern "C" 
{
#endif

/*****************************************************************************************************************************/
//	Constants

typedef int32_t				MIRResult;									//!> Result code returned by API functions

static const int32_t		MIRSuccess					= 0;			//!< Indicates successful result for an operation
static const uint32_t		MIRInvalidHandle			= UINT32_MAX;	//!< Default value of an invalid resource handle
static const uint32_t		InvalidTracker				= 0u;			//!< Indicates no tracker is supplied to an operation
static const uint32_t		InvalidBand					= UINT32_MAX;	//!< Indicates no band index is supplied to an operation
static const size_t			MAX_FILEPATH				= 256u;			//!< Maximum number of chars in the file path
static const size_t			MAX_COORDSYS				= 512u;			//!< Maximum number of coordinate system objects
static const uint32_t		DefaultSampleCount			= 100u;			//!< Default number of samples to be created along a line
static const double			DefaultNullValue			= -9999.0;		//!< Default null value for legacy rasters
static const uint32_t		DefaultExportMaxDecimal		= 16u;			//!< Default number of decimal points written on export
static const wchar_t		DefaultExportDelimiter		= L' ';			//!< Default Delimiter used in export
static const uint32_t		InvalidBinCount				= UINT32_MAX;	//!< Indicates no bin count is supplied to an operation
static const uint32_t		InvalidColor				= 0xFEFFFFFF;	//!< Indicates no color is supplied to this operation
static const uint32_t		MaxGroupCount				= 1024;			//!< Default maximum number of groups ro return

/*****************************************************************************************************************************/
//	General enumerations

/*! \enum	MIR_DataType
	\brief	Data types representable by a variant.

	MIR_DATETIME_OLE
	The DATE type is implemented using an 8-byte floating-point number. 
	Days are represented by whole number increments starting with 30 December 1899, midnight as time zero. 
	Hour values are expressed as the absolute value of the fractional part of the number. 
	The DATE date type represents dates and times as a classic number line. 
	The DATE timeline becomes discontinuous for date values less than 0 (before 30 December 1899). 
	This is because the whole-number portion of the date value is treated as signed, while the fractional part is treated as 
	unsigned. In other words, the whole-number part of the date value may be positive or negative, while the fractional part 
	of the date value is always added to the overall logical date.

	MIR_DATETIME_CPP
	The number of seconds since January 1, 1970, 0:00 UTC.
*/
enum MIR_DataType
{
	//	Invalid
	MIR_UNDEFINED			= 0,		//!< Undefined data type
	MIR_NULL,							//!< Null data type
	MIR_EMPTY,							//!< Empty data type
	//	Boolean
	MIR_BOOLEAN_INT8		= 10,		//!< 8-bit boolean type
	MIR_BOOLEAN_INT32,					//!< 32-bit boolean type
	//	Bit
	MIR_BIT1				= 20,		//!< 1-bit data
	MIR_BIT2,							//!< 2-bit crumb
	MIR_BIT4,							//!< 4-bit nibble
	//	Unsigned integers
	MIR_UNSIGNED_INT8		= 30,		//!< 8-bit unsigned integer
	MIR_UNSIGNED_INT16,					//!< 16-bit unsigned integer
	MIR_UNSIGNED_INT32,					//!< 32-bit unsigned integer
	MIR_UNSIGNED_INT64,					//!< 64-bit unsigned integer
	//	Signed integers
	MIR_SIGNED_INT8			= 40,		//!< 8-bit signed integer
	MIR_SIGNED_INT16,					//!< 16-bit signed integer
	MIR_SIGNED_INT32,					//!< 32-bit signed integer
	MIR_SIGNED_INT64,					//!< 64-bit signed integer
	//	Floating point
	MIR_REAL2				= 50,		//!< 2 byte real (unimplemented)
	MIR_REAL4,							//!< 4 byte real
	MIR_REAL8,							//!< 8 byte real
	MIR_REAL_LONG,						//!< 8 byte real
	//	Complex numbers
	MIR_COMPLEX_INT16		= 60,		//!< 16-bit signed integer complex number (real, imaginary)
	MIR_COMPLEX_INT32,					//!< 32-bit signed integer complex number (real, imaginary)
	MIR_COMPLEX_REAL4,					//!< 4 byte real complex number (real, imaginary)
	MIR_COMPLEX_REAL8,					//!< 8 byte real complex number (real, imaginary)
	//	Time - Date
	MIR_DATETIME_OLE		= 70,		//!< Windows DATE, 8 byte real
	MIR_DATETIME_CPP,					//!< Standard time_t, 64 bit integer
	//	String
	MIR_STRING				= 80,		//!< ASCII, variable length
	MIR_FIXED_STRING,					//!< ASCII, Fixed length
	MIR_STRING_UTF8,					//!< Unicode, variable length, std::string
	MIR_STRING_UTF16,					//!< Unicode, variable length, std::wstring
	MIR_STRING_UTF32,					//!< Unicode, variable length, std::u32string
	//	Binary large objects
	MIR_BINARY_OBJECT		= 90,		//!< Variable length
	MIR_FIXED_BINARY_OBJECT,			//!< Fixed length
	//	Color
	MIR_RED					= 100,		//!< 8 bit red
	MIR_GREEN,							//!< 8 bit green
	MIR_BLUE,							//!< 8 bit blue
	MIR_GREY,							//!< 8 bit grey (minimum is black)
	MIR_ALPHA,							//!< 8 bit opacity, (minimum is transparent)
	MIR_RED_ALPHA,						//!< 8|8 bit
	MIR_GREEN_ALPHA,					//!< 8|8 bit
	MIR_BLUE_ALPHA,						//!< 8|8 bit
	MIR_GREY_ALPHA,						//!< 8|8 bit
	MIR_RGB,							//!< 8|8|8 bit
	MIR_RGBA,							//!< 8|8|8|8 bit
	MIR_BGR,							//!< 8|8|8 bit
	MIR_BGRA,							//!< 8|8|8|8 bit
	MIR_HSI_Hue,						//!< 8 bit hue (HSI)
	MIR_HSI_Saturation,					//!< 8 bit saturation (HSI)
	MIR_HSI_Intensity,					//!< 8 bit intensity (HSI)
	MIR_HSL_Hue,						//!< 8 bit hue (HSL/HLS)
	MIR_HSL_Saturation,					//!< 8 bit saturation (HSL/HLS)
	MIR_HSL_Lightness,					//!< 8 bit lightness (HSL/HLS)
	MIR_HSV_Hue,						//!< 8 bit hue (HSV/HSB)
	MIR_HSV_Saturation,					//!< 8 bit saturation (HSV/HSB)
	MIR_HSV_Value,						//!< 8 bit value (HSV/HSB)
	MIR_HSI,							//!< 8|8|8 bit
	MIR_HSL,							//!< 8|8|8 bit
	MIR_HSV,							//!< 8|8|8 bit
	MIR_HSIA,							//!< 8|8|8|8 bit
	MIR_HSLA,							//!< 8|8|8|8 bit
	MIR_HSVA,							//!< 8|8|8|8 bit
	MIR_MINISBLACK1,					//!< 1 bit grey (minimum is black)
	MIR_MINISBLACK2,					//!< 2 bit grey (minimum is black)
	MIR_MINISBLACK4,					//!< 4 bit grey (minimum is black)
	MIR_MINISBLACK8,					//!< 8 bit grey (minimum is black)
	MIR_MINISWHITE1,					//!< 1 bit grey (minimum is white)
	MIR_MINISWHITE2,					//!< 2 bit grey (minimum is white)
	MIR_MINISWHITE4,					//!< 4 bit grey (minimum is white)
	MIR_MINISWHITE8						//!< 8 bit grey (minimum is white)
};

/*! \enum	MIR_UnitCode
	\brief	MapInfo units.
*/
enum MIR_UnitCode
{
	MIR_Undefined = -1,				//!< Undefined Unit Type
	MIR_Miles = 0,					//!< MapInfo Unit Type - Miles
	MIR_Kilometers = 1,				//!< MapInfo Unit Type - Kilometres
	MIR_Inches,						//!< MapInfo Unit Type - Inches
	MIR_Feet,						//!< MapInfo Unit Type - Feet
	MIR_Yards,						//!< MapInfo Unit Type - Yards
	MIR_Millimeters,				//!< MapInfo Unit Type - Millimeters
	MIR_Centimeters,				//!< MapInfo Unit Type - Centimeters
	MIR_Meters,						//!< MapInfo Unit Type - Meters
	MIR_USSurveyFeet,				//!< MapInfo Unit Type - US Survey Feet
	MIR_NauticalMiles,				//!< MapInfo Unit Type - Nautical Miles
	MIR_Links = 30,					//!< MapInfo Unit Type - Links
	MIR_Chains = 31,				//!< MapInfo Unit Type - Chains
	MIR_Rods = 32,					//!< MapInfo Unit Type - Rods
	MIR_Degree = 64,				//!< MapInfo Unit Type - Degree
	MIR_ArcMinute = 65,				//!< MapInfo Unit Type - Arc Minute
	MIR_ArcSecond = 66,				//!< MapInfo Unit Type - Arc Second
	MIR_MilliArcSecond = 67,		//!< MapInfo Unit Type - Milli Arc Second

	MIR_Microseconds		= 100,
	MIR_Milliseconds,
	MIR_Seconds,
	MIR_Minutes,
	MIR_Hours,
	MIR_Days,
	MIR_Weeks,
	MIR_Years,

	MIR_dB                =  128,
	MIR_dBm,
	MIR_dBW,
	MIR_dBuV_m,
	MIR_Radians,
	MIR_Percent,
	MIR_DegreeDBP,
	MIR_Calls_HR_KM2,
	MIR_Msgs_HR_KM2,
	MIR_Erlangs_HR_KM2,
	MIR_SimCalls_KM2,

	MIR_Erlang			= 140,
	MIR_Bits_Cell,
	MIR_KBits_KM2,
	MIR_MBits_KM2,
	MIR_Events_Sec,
	MIR_Kbps,
	MIR_Kbps_KM2_Floor,
	MIR_Subscribers,
	MIR_Subscribers_KM2,
	MIR_Subscribers_KM2_Floor,
	MIR_Erlangs_KM2,
	MIR_Erlangs_KM2_Floor,
	MIR_Mbps,
	MIR_Bits_S_Hz,
	MIR_Kbps_KM2,
	MIR_Kbps_MHz,
	MIR_Calls,

	MIR_Kilometres = MIR_Kilometers,
	MIR_Millimetres = MIR_Millimeters,
	MIR_Centimetres = MIR_Centimeters,
	MIR_Metres = MIR_Meters,
	MIR_Degrees = MIR_Degree
};

/*****************************************************************************************************************************/
//	Vehicles, drivers and driver capabilities

/*! \struct	SMIR_VehicleCapabilities
	\brief	Capabilities of the vehicle, applicable to all drivers it supports.
*/
struct SMIR_VehicleCapabilities
{
	char		sUniqueID[256];
	uint32_t	nIdentificationStrength;

	wchar_t		sName[256];
	wchar_t		sDescription[256];

	uint32_t	bMultipleDriver		: 1;	//!< Supports multiple drivers
	uint32_t	bThreadSafeNative	: 1;	//!< Supports multiple thread access in Native support mode (ReadOnly & EditCell)
	uint32_t	bThreadSafeBase		: 1;	//!< Supports multiple thread access in Base support mode (ReadOnly & EditCell)
	uint32_t	bThreadSafeFull		: 1;	//!< Supports multiple thread access in Full support mode (ReadOnly & EditCell)
};

/*! \struct	SMIR_DriverCapabilities
	\brief	Capabilities of a driver.
*/
struct SMIR_DriverCapabilities
{
	char		sUniqueID[256];
	uint32_t	nIdentificationStrength;

	wchar_t		sName[256];
	wchar_t		sDescription[256];

	wchar_t		sExtension[32];
	wchar_t		sExtensionList[256];

	//	Support for fields, bands and events
	uint32_t	bMultipleField						: 1;	//!< Supports multiple unrelated fields
	uint32_t	bMultipleBand						: 1;	//!< Supports multi-banded fields
	uint32_t	bMultipleEvent						: 1;	//!< Supports the time dimension

	//	Support for field types
	uint32_t	bClassifiedField					: 1;	//!< Supports classified field type
	uint32_t	bImageField							: 1;	//!< Supports image field type
	uint32_t	bImagePaletteField					: 1;	//!< Supports image palette field type
	uint32_t	bContinuousField					: 1;	//!< Supports continuous field type

	//	Support for operations
	uint32_t	bEditCell							: 1;	//!< Supports edit cell operation
	uint32_t	bEditStructure						: 1;	//!< Supports edit operation: extension of the raster structure
	uint32_t	bCreate								: 1;	//!< Supports create operation

	//	Support for tile access
	uint32_t	bRandomRead							: 1;	//!< Supports random access for reading a tile
	uint32_t	bRandomWrite						: 1;	//!< Supports random access for writing a tile
	uint32_t	nLoadBandOnDemand					: 2;	//!< Supports loading bands on demand

	//	Support for overviews and statistics
	uint32_t	bStoredOverviews					: 1;	//!< Supports overviews stored in the raster file
	uint32_t	bSuppliesUnderviews					: 1;	//!< Supports underviews acquired from the driver
	uint32_t	bRequireStoreOverview				: 1;	//!< Writing overviews may be optional or required
	uint32_t	bAllowPermanentCache				: 1;	//!< Allow a permanent full or partial pyramid cache to be created as a raster companion file
	uint32_t	nPriorWriteStatistics				: 3;	//!< Requires statistics prior to writing
																//!< 0 = None, 1 = Count, 2 = Summary, 3 = Distribution, 4 = Spatial
	uint32_t	nStoreWriteStatistics				: 3;	//!< Supports statistics stored in the raster file
																//!< 0 = None, 1 = Count, 2 = Summary, 3 = Distribution, 4 = Spatial
	uint32_t	bRequireStoreStatistics				: 1;	//!< Writing statistics may be optional or required

	uint32_t	bVariableCellExtent					: 1;	//!< Supports extendable and modifiable cell count
	uint32_t	bSparseTiles						: 1;	//!< Supports sparse tile arrangements

	uint32_t	bFixedAnchor						: 1;	//!< Global anchor position for cell (0,0) in any level
	uint32_t	bPower2CellSize						: 1;	//!< Cell size varies by 2 for each level
	uint32_t	bDataASCII							: 1;	//!< Raster data file is ASCII format
	uint32_t	nCellValidityMethod					: 2;	//!< 0 = none,1 = numeric compare,2 = string compare,3 = mask
	uint32_t	bExtendedValidity					: 1;	//!< Supports invalid cell classification

	//	Data storage arrangement within the raster
	//	TODO	Make this a mask of 4 bits to allow combinations.
	uint32_t	nCellArrangement_Storage			: 2;	//!< Cell/Row/Strip/Tile	
	
	//	Order of tiles within the raster
	uint32_t	nCellArrangement_RasterXSense		: 2;	//!< W - E / E - W
	uint32_t	nCellArrangement_RasterYSense		: 2;	//!< S - N / N - S

	//	Order of cells within a tile
	uint32_t	nCellArrangement_TileXSense			: 2;	//!< W - E / E - W
	uint32_t	nCellArrangement_TileYSense			: 2;	//!< S - N / N - S

	//	The level at which fields are interleaved
	uint32_t	nCellArrangement_InterleaveField	: 2;	//!< Cell/Row/Tile/Raster

	//	The level at which bands are interleaved
	uint32_t	nCellArrangement_InterleaveBand		: 2;	//!< Cell/Row/Tile/Raster

	uint32_t	bThreadSafeNative					: 1;	//!< Supports multiple thread access in Native support mode (ReadOnly & EditCell)
	uint32_t	bThreadSafeBase						: 1;	//!< Supports multiple thread access in Base support mode (ReadOnly & EditCell)
	uint32_t	bThreadSafeFull						: 1;	//!< Supports multiple thread access in Full support mode (ReadOnly & EditCell)

	uint32_t	bCompression						: 1;	//!< Supports compression (methods not specified here)

	uint32_t	bFixedTableStructure				: 1;	//!< Supports fixed classification table structure
	uint32_t	nEndian								: 2;	//!< 0 = None, 1 = Little, 2 = Big, 3 = Little and Big
};

/*****************************************************************************************************************************/
//	Raster properties

/*!	\enum	MIR_FieldType
	\brief	Raster field type.
*/
enum MIR_FieldType
{
	MIR_FIELD_Default		= -1,
	MIR_FIELD_Classified	= 0,
	MIR_FIELD_Image			= 1,
	MIR_FIELD_ImagePalette	= 2,
	MIR_FIELD_Continuous	= 3
};

/*!	\enum	MIR_ClassTableFieldType
	\brief	Raster classification table field type.
*/
enum MIR_ClassTableFieldType
{
	MIR_TFT_Undefined = -1,		//!< Classified field type is undefined (system may define it appropriately)
	MIR_TFT_Class = 0,			//!< Classified field contains original class identifier
	MIR_TFT_Value,				//!< Classified field contains primary data value
	MIR_TFT_Colour,				//!< Classified field contains primary color value
	MIR_TFT_Label,				//!< Classified field contains primary text label
	MIR_TFT_Data,				//!< Classified field contains data
	MIR_TFT_ColourR,			//!< Classified field contains primary color red value
	MIR_TFT_ColourG,			//!< Classified field contains primary color green value
	MIR_TFT_ColourB				//!< Classified field contains primary color blue value
};

/*! \enum	MIR_CompressionType
	\brief	Compression types supported by MIRaster IO API.
*/
enum MIR_CompressionType
{
	//	MRR
	MIR_NoCompression				= -1,		//!< No Compression. Compression Level is ignored.
	MIR_Compression_Zip				= 0,		//!< Zip Compression. Supported compression Levels are 1 to 9.
	MIR_Compression_LZMA			= 1,		//!< LZMA Compression. Supported compression Levels are 0 to 9.
	MIR_Compression_PNG				= 2,		//!< PNG Compression.
	MIR_Compression_JPEG			= 3,		//!< JPEG Compression. Supported compression Levels are 0+, maps to Quality 100-(3*C).
	MIR_Compression_LZ4				= 4,		//!< LZ4 high speed lossless compression.
	//MIR_Compression_LZ4HC

	//Grouped into Balanced, speed and space for data and imagery compression.
	MIR_Compression_DataBalanced		= 50,	//!< Lossless data compression, balanced.
	MIR_Compression_DataSpeed			= 51,	//!< Lossless data compression, favor higher encoding speed.
	MIR_Compression_DataSpace			= 52,	//!< Lossless data compression, favor higher compression.
	MIR_Compression_ImageBalanced		= 53,	//!< Lossless image compression, balanced.
	MIR_Compression_ImageSpeed			= 54,	//!< Lossless image compression, favor higher encoding speed.
	MIR_Compression_ImageSpace			= 55,	//!< Lossless image compression, favor higher compression.
	MIR_Compression_ImageLossyBalanced	= 56,	//!< Lossy image compression, balanced.
	MIR_Compression_ImageLossySpeed		= 57,	//!< Lossy image compression, favor higher encoding speed.
	MIR_Compression_ImageLossySpace		= 58,	//!< Lossy image compression, favor higher compression.

	//	GeoTIFF
	MIR_Compression_TIFF_NONE		= 1000,		//!< dump mode
	MIR_Compression_TIFF_CCITTRLE	= 1001,		//!< CCITT modified Huffman RLE
	MIR_Compression_TIFF_CCITTFAX3	= 1002,		//!< CCITT Group 3 fax encoding
	MIR_Compression_TIFF_T4			= 1003,		//!< CCITT T.4 (TIFF 6 name)
	MIR_Compression_TIFF_CCITTFAX4	= 1004,		//!< CCITT Group 4 fax encoding
	MIR_Compression_TIFF_CCITT_T6	= 1005,		//!< CCITT T.6 (TIFF 6 name)
	MIR_Compression_TIFF_LZW		= 1006,		//!< Lempel-Ziv  & Welch
	MIR_Compression_TIFF_OJPEG		= 1007,		//!< !6.0 JPEG
	MIR_Compression_TIFF_JPEG		= 1008,		//!< %JPEG DCT compression
	MIR_Compression_TIFF_ADOBE_DEFLATE= 1009,	//!< Deflate compression,as recognized by Adobe
	MIR_Compression_TIFF_T85		= 1010,		//!< !TIFF/FX T.85 JBIG compression
	MIR_Compression_TIFF_T43		= 1011,		//!< !TIFF/FX T.43 colour by layered JBIG compression
	MIR_Compression_TIFF_NEXT		= 1012,		//!< NeXT 2-bit RLE
	MIR_Compression_TIFF_CCITTRLEW	= 1013,		//!< #1 w/ word alignment
	MIR_Compression_TIFF_PACKBITS	= 1014,		//!< Macintosh RLE
	MIR_Compression_TIFF_THUNDERSCAN= 1015,		//!< ThunderScan RLE
	MIR_Compression_TIFF_IT8CTPAD	= 1016,		//!< IT8 CT w/padding
	MIR_Compression_TIFF_IT8LW		= 1017,		//!< IT8 Linework RLE
	MIR_Compression_TIFF_IT8MP		= 1018,		//!< IT8 Monochrome picture
	MIR_Compression_TIFF_IT8BL		= 1019,		//!< IT8 Binary line art
	MIR_Compression_TIFF_PIXARFILM	= 1020,		//!< Pixar companded 10bit LZW
	MIR_Compression_TIFF_PIXARLOG	= 1021,		//!< Pixar companded 11bit ZIP
	MIR_Compression_TIFF_DEFLATE	= 1022,		//!< Deflate compression
	MIR_Compression_TIFF_DCS		= 1023,		//!< Kodak DCS encoding
	MIR_Compression_TIFF_JBIG		= 1024,		//!< ISO JBIG
	MIR_Compression_TIFF_SGILOG		= 1025,		//!< SGI Log Luminance RLE
	MIR_Compression_TIFF_SGILOG24	= 1026,		//!< SGI Log 24-bit packed
	MIR_Compression_TIFF_JP2000		= 1027,		//!< Leadtools JPEG2000
	MIR_Compression_TIFF_LZMA		= 1028		//!< LZMA2
};

/*!	\enum	MIR_BandType
	\brief	Raster band type.
*/
enum MIR_BandType
{
	MIR_Concrete			= 0,			//!< Band data is stored in the raster
	MIR_Component			= 1,			//!< Band data is an acquired component of another concrete band
	MIR_TableField			= 2,			//!< Band data is acquired from a classification table field
	MIR_TableField_Component= 3				//!< Band data is an acquired component of another band acquired from a classification table field
};

/*!	\enum	MIR_NULLType
	\brief	Raster null cell identification method.
*/
enum MIR_NULLType
{
	MIR_NULL_NONE						= 0x00000001,	//	Specify one of these
	MIR_NULL_NUMERIC_COMPARE			= 0x00000002,
	MIR_NULL_STRING_COMPARE				= 0x00000004,
	MIR_NULL_MASK						= 0x00000008,
	MIR_NULL_METHOD_FIXED				= 0x00010000,	//	Optional
	MIR_NULL_VALUE_FIXED				= 0x00020000	//	Optional
};

/*!	\enum	MIR_PredictiveEncoding
	\brief	Predictive encoding schemes.
*/
enum MIR_PredictiveEncoding
{
	MIR_Encoding_None			= -1,				/*!< No encoding.*/
	MIR_PreviousColumnValue		= 0,				/*!< Predict the value from previous column.*/
	MIR_PreviousColumnLinear	= 1,				/*!< Linear estimate the value from previous two columns.*/
	MIR_RunLength				= 2					/*!<Only supported for Classified and ImagePalette fields.*/

};

/*!	\enum	MIR_EventType
	\brief	Raster event edit type.
*/
enum MIR_EventType
{
	MIR_EET_Partial		= 0,
	MIR_EET_Total		= 1
};

/*!	\enum	MIR_SmoothingType
	\brief	Smoothing type for raster interpolation.
*/
enum MIR_SmoothingType
{
	MIR_NONE = 0,
	MIR_AVERAGE_KERNEL,
	MIR_GAUSSIAN
};

/*!	\enum	MIR_RasterProperty
	\brief	Raster property.
*/
enum MIR_RasterProperty
{
	MIR_RasterName = 0,					/*!< Raster Name property.*/
	MIR_RasterCoordinateSystem,			/*!< Coordsys of the Raster.*/
	MIR_RasterColour,					/*!< Color inflections of the raster.*/
	MIR_FieldName,						/*!< Raster Field Name.*/
	MIR_FieldMetaData,					/*!< Raster Field metadata.*/
	MIR_FieldTransform,					/*!< Raster field Transform.*/
	MIR_BandName,						/*!< Raster Band Name.*/
	MIR_BandMetaData,					/*!< Raster Band metadata.*/
	MIR_BandUnit,						/*!< Raster Band unit.*/
	MIR_TableName,						/*!< Name of the table if any in Raster field.*/
	MIR_TableFieldName,					/*!< Raster Table Field Name.*/
	MIR_TableFieldType,					/*!< Raster Table Field Type.*/
	MIR_TableFieldMetaData				/*!< Raster Table field metadata.*/
};

/*****************************************************************************************************************************/
//	Open and create rasters

/*! \enum	MIR_RasterSupportMode
	\brief	Raster access support modes.
*/
enum MIR_RasterSupportMode
{
	MIR_Support_Native,						/*!< Guarantees sequential tile access to the base level only. 
												 Random access to the base level is not guaranteed.
												 The existence of an overview pyramid is not guaranteed.
												 Actual capabilities depend on the raster format.
												 Does not generate any default cache file, but if already
												 present will utilize it. */
	MIR_Support_Base,						/*!< Guarantees high performance random access to the base level.
												 The existence of an overview pyramid is not guaranteed.
												 Actual capabilities depend on the raster format.
												 If a cache file is already present, utilizes it, else generates 
												 a temporary cache file in the temp directory and deletes
												 the cache file on close. */
	MIR_Support_Full,						/*!< Guarantees high performance random access to the base level
												 and overview pyramid. Guarantees an overview pyramid will exist.
												 If a cache file is already present, utilizes it, else generates 
												 a permanent cache file. If the raster location is read only,
												 generates a temporary in the temp directory and deletes
												 the cache file on close. May generate a pyramid by decimation
												 if supported by the raster driver and meets all criteria. */
	MIR_Support_Full_Quality,				/*!< Guarantees high performance random access to the base level
												 and overview pyramid. Guarantees an overview pyramid will exist.
												 If a cache file is already present, utilizes it, else generates 
												 a permanent cache file. If the raster location is read only,
												 generates a temporary in the temp directory and deletes
												 the cache file on close. Never generates a pyramid by decimation. */
	MIR_Support_Full_Speed					/*!< Guarantees high performance random access to the base level
												 and overview pyramid. Guarantees an overview pyramid will exist.
												 If a cache file is already present, utilizes it, else generates 
												 a permanent cache file. If the raster location is read only,
												 generates a temporary in the temp directory and deletes
												 the cache file on close. Always generates a pyramid by decimation
												 if supported by the raster driver. */
};

/*! \enum	MIR_FILE_CLASS
	\brief	Classification of files associated with a raster.
*/
enum MIR_FILE_CLASS
{
	MIR_FILECLASS_NONE		= 0,
	MIR_FILECLASS_USER,				//	Filename supplied by user, may not be actual file
	MIR_FILECLASS_ARCHIVE,			//	Filename of archive (zip) containing raster
	MIR_FILECLASS_RASTER,
	MIR_FILECLASS_HEADER,
	MIR_FILECLASS_GEOREF,
	MIR_FILECLASS_COORDSYS,
	MIR_FILECLASS_INDEX,
	MIR_FILECLASS_DATA,
	MIR_FILECLASS_TILE,
	MIR_FILECLASS_STATISTICS,
	MIR_FILECLASS_COLOUR,
	MIR_FILECLASS_PERC,
	MIR_FILECLASS_PPRC,
	MIR_FILECLASS_GHX,
	MIR_FILECLASS_TEMP,
	MIR_FILECLASS_DIRECTORY,
	MIR_FILECLASS_TAB
};

/*! \enum	MIR_AlgLayerType
	\brief	Type description of a layer of a rendering algorithm.
*/
enum MIR_AlgLayerType
{
	MIR_ALGLAYERTYPE_NONE			= -1,	/*!< Invalid rendering algorithm layer type*/
	MIR_ALGLAYERTYPE_MASK			= 0,	/*!< Pixel mask rendering algorithm layer type*/
	MIR_ALGLAYERTYPE_IMAGE			= 1,	/*!< Image rendering algorithm layer type*/
	MIR_ALGLAYERTYPE_LUTCOLOR		= 2,	/*!< Look up table color modulated rendering algorithm layer type*/
	MIR_ALGLAYERTYPE_RGBCOLOR		= 3,	/*!< Red Green Blue color modulated rendering algorithm layer type*/
	MIR_ALGLAYERTYPE_CONTOUR		= 4		/*!< Contour rendering algorithm layer type*/
};

/*! \enum	MIR_AlgComponentType
	\brief	Type description of a component of a rendering algorithm layer.
*/
enum MIR_AlgComponentType
{
	MIR_ALGCOMTYPE_NONE			= -1,	/*!< Invalid rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_MASK			= 0,	/*!< Pixel mask rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_IMAGE		= 1,	/*!< Image rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_COLOR		= 2,	/*!< Color rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_RED			= 3,	/*!< Red rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_GREEN		= 4,	/*!< Green rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_BLUE			= 5,	/*!< Blue rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_OPACITY		= 6,	/*!< Opacity rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_INTENSITY	= 7,	/*!< Intensity rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_CONTOUR		= 8,	/*!< Contour rendering algorithm layer component type*/
	MIR_ALGCOMTYPE_PRIMARY		= 9,	/*!< Variable code for the primary component type*/
	MIR_ALGCOMTYPE_SECONDARY	=10,	/*!< Variable code for the secondary component type*/
	MIR_ALGCOMTYPE_PRIMARYCOLOR	=11		/*!< Variable code for the primary color component type*/
};

/*! \enum	MIR_LayerBlendingRule
	\brief	A set of standard data to color space data transformation definitions.
*/
enum MIR_LayerBlendingRule
{
	MIR_BLEND_OVERRIDE		= 0,	/*!<Opaque output with no layer blending or opacity modulation.*/
	MIR_BLEND_OVERPRINT		= 1,	/*!<Opacity is carried through to output pixels if present and layers overprint.*/
	MIR_BLEND_LIGHTTABLE	= 2,	/*!<Opaque output simulating a light table by blending layers equally.*/
	MIR_BLEND_BLENDED		= 3		/*!<Blend layers and enable layer opacity modulation.*/
};

/*! \enum	MIR_ValidCellByComponentRule
	\brief	Define which components of a rendering layer must contain valid source data to generate a valid rendered pixel.
*/
enum MIR_ValidCellByComponentRule
{
	MIR_VCBCRULE_ANY			= 0,	/*!<If any component data is valid, the pixel will be valid.*/
	MIR_VCBCRULE_PRIMARY		= 1,	/*!<If the primary component data is valid, the pixel will be valid.*/
	MIR_VCBCRULE_ALL			= 2,	/*!<If all component data is valid, the pixel will be valid.*/
	MIR_VCBCRULE_ALLTOANY		= 3,	/*!<If all or any component data is valid, the pixel will be valid.*/
	MIR_VCBCRULE_PRIMARYTOANY	= 4		/*!<If the primary or any component data is valid, the pixel will be valid.*/
};

/*! \enum	MIR_StandardDataTransform
	\brief	A set of standard data to color space data transformation definitions.
*/
enum MIR_StandardDataTransform
{
	//	No standard transform
	MIR_SDT_NONSTANDARD = 0,		/*!<User defined transform.*/
	MIR_SDT_PASS,					/*!<The data value is a scaled index from 0.0 to 1.0 and can be converted to a color via a LUT.*/
	MIR_SDT_PASSINDEX,				/*!<The data value is an integer index and can be converted to a color via a LUT.*/
	MIR_SDT_PASSVALUE,				/*!<The data value is a color and can be used without transformation.*/
	MIR_SDT_8BITCOLOR,				/*!<A linear transform between 0 and 255.*/
	MIR_SDT_9BITCOLOR,				/*!<A linear transform between 0 and 511.*/
	MIR_SDT_10BITCOLOR,				/*!<A linear transform between 0 and 1023.*/
	MIR_SDT_11BITCOLOR,				/*!<A linear transform between 0 and 2047.*/
	MIR_SDT_12BITCOLOR,				/*!<A linear transform between 0 and 4096.*/
	MIR_SDT_13BITCOLOR,				/*!<A linear transform between 0 and 8191.*/
	MIR_SDT_14BITCOLOR,				/*!<A linear transform between 0 and 16383.*/
	MIR_SDT_15BITCOLOR,				/*!<A linear transform between 0 and 32767.*/
	MIR_SDT_16BITCOLOR,				/*!<A linear transform between 0 and 65535.*/
	MIR_SDT_LINEAR,					/*!<A linear transform across the full data range.*/
	MIR_SDT_LINEAR_1PCNT,			/*!<A linear transform across 1 - 99 percent of the data range.*/
	MIR_SDT_LINEAR_5PCNT,			/*!<A linear transform across 5 - 95 percent of the data range.*/
	MIR_SDT_LINEAR_10PCNT,			/*!<A linear transform across 10 - 90 percent of the data range.*/
	MIR_SDT_LINEAR_P5PTLE,			/*!<A linear transform across 0.5 - 99.5 quantiles of the data range.*/
	MIR_SDT_LINEAR_2PTLE,			/*!<A linear transform across 2 - 98 quantiles of the data range.*/
	MIR_SDT_LINEAR_5PTLE,			/*!<A linear transform across 5 - 95 quantiles of the data range.*/
	MIR_SDT_LOG10_1,				/*!<A base 10 logarithmic transform, linear between -1 to +1.*/
	MIR_SDT_LOG10_10,				/*!<A base 10 logarithmic transform, linear between -10 to +10.*/
	MIR_SDT_LOG10_100,				/*!<A base 10 logarithmic transform, linear between -100 to +100.*/
	MIR_SDT_LOG10_1000,				/*!<A base 10 logarithmic transform, linear between -1000 to +1000.*/
	MIR_SDT_LOG10_P1,				/*!<A base 10 logarithmic transform, linear between -0.1 to +0.1.*/
	MIR_SDT_LOG10_P01,				/*!<A base 10 logarithmic transform, linear between -0.01 to +0.01.*/
	MIR_SDT_LOG10_P001,				/*!<A base 10 logarithmic transform, linear between -0.001 to +0.001.*/
	MIR_SDT_LOG10_P0001,			/*!<A base 10 logarithmic transform, linear between -0.0001 to +0.0001.*/
	MIR_SDT_LIGHTEN,				/*!<A lightening sigmoid transform across the full data range.*/
	MIR_SDT_DARKEN,					/*!<A darkening sigmoid transform across the full data range.*/
	MIR_SDT_EQAREA,					/*!<An equal area transform across the full data range.*/
	MIR_SDT_EQAREA_1PCNT,			/*!<An equal area transform across 1 - 99 percent of the data range.*/
	MIR_SDT_EQAREA_5PCNT,			/*!<An equal area transform across 5 - 95 percent of the data range.*/
	MIR_SDT_EQAREA_10PCNT,			/*!<An equal area transform across 10 - 90 percent of the data range.*/
	MIR_SDT_EQAREANL,				/*!<An outlier resilient equal area transform across the full data range.*/
	MIR_SDT_EQAREANL_1PCNT,			/*!<An outlier resilient equal area transform across 1 - 99 percent of the data range.*/
	MIR_SDT_EQAREANL_5PCNT,			/*!<An outlier resilient equal area transform across 5 - 95 percent of the data range.*/
	MIR_SDT_EQAREANL_10PCNT,		/*!<An outlier resilient equal area transform across 10 - 90 percent of the data range.*/	
	MIR_SDT_PCNT_N,					/*!<N color bins equally spaced across the data range.*/
	MIR_SDT_PCNT_2,					/*!<Two color bins equally spaced across the data range.*/
	MIR_SDT_PCNT_4,					/*!<Four color bins equally spaced across the data range.*/
	MIR_SDT_PCNT_8,					/*!<Eight color bins equally spaced across the data range.*/
	MIR_SDT_PCNT_12,				/*!<Twelve color bins equally spaced across the data range.*/
	MIR_SDT_PTLE_N,					/*!<N color bins equally spaced by quantile across the data range.*/
	MIR_SDT_PTLE_2,					/*!<Two color bins equally spaced by quantile across the data range.*/
	MIR_SDT_PTLE_4,					/*!<Four color bins equally spaced by quantile across the data range.*/
	MIR_SDT_PTLE_8,					/*!<Eight color bins equally spaced by quantile across the data range.*/
	MIR_SDT_PTLE_12,				/*!<Twelve color bins equally spaced by quantile across the data range.*/
	MIR_SDT_MEAN_N,					/*!<N color bins spaced across the data range, centered about the mean.*/
	MIR_SDT_MEAN_2,					/*!<Two color bins spaced across the data range, centered about the mean.*/
	MIR_SDT_MEAN_4,					/*!<Four color bins spaced across the data range, centered about the mean.*/
	MIR_SDT_MEAN_8,					/*!<Eight color bins spaced across the data range, centered about the mean.*/
	MIR_SDT_MEAN_12,				/*!<Twelve color bins spaced across the data range, centered about the mean.*/
	MIR_SDT_MED_N,					/*!<N color bins spaced across the data range, centered about the median.*/
	MIR_SDT_MED_2,					/*!<Two color bins spaced across the data range, centered about the median.*/
	MIR_SDT_MED_4,					/*!<Four color bins spaced across the data range, centered about the median.*/
	MIR_SDT_MED_8,					/*!<Eight color bins spaced across the data range, centered about the median.*/
	MIR_SDT_MED_12,					/*!<Twelve color bins spaced across the data range, centered about the median.*/
	MIR_SDT_MODE_N,					/*!<N color bins spaced across the data range, centered about the mode.*/
	MIR_SDT_MODE_2,					/*!<Two color bins spaced across the data range, centered about the mode.*/
	MIR_SDT_MODE_4,					/*!<Four color bins spaced across the data range, centered about the mode.*/
	MIR_SDT_MODE_8,					/*!<Eight color bins spaced across the data range, centered about the mode.*/
	MIR_SDT_MODE_12					/*!<Twelve color bins spaced across the data range, centered about the mode.*/,
	MIR_SDT_MEANSTD_N,				/*!<N color bins spaced by standard deviation or to the data range, centered about the mean.*/
	MIR_SDT_MEANSTD_4,				/*!<Four color bins spaced by standard deviation or to the data range, centered about the mean.*/
	MIR_SDT_MEANSTD_8,				/*!<Eight color bins spaced by standard deviation or to the data range, centered about the mean.*/
	MIR_SDT_MEANSTD_12,				/*!<Twelve color bins spaced by standard deviation or to the data range, centered about the mean.*/
	MIR_SDT_JENKS_N,				/*!<N color bins spaced across the data range using Jenks Natural Breaks.*/
	MIR_SDT_JENKS_4,				/*!<Four color bins spaced across the data range using Jenks Natural Breaks.*/
	MIR_SDT_JENKS_8,				/*!<Eight color bins spaced across the data range using Jenks Natural Breaks.*/
	MIR_SDT_JENKS_12,				/*!<Twelve color bins spaced across the data range using Jenks Natural Breaks.*/
	MIR_SDT_HISTMATCH				/*!<A transform from one value space to another by matching distribution histograms.*/
};

/*! \enum	MIR_DataColorTransformType
	\brief	Data to color space data transformation definitions.
*/
enum MIR_DataColorTransformType
{
	MIR_DCTTYPE_Undefined = -1,				/*!<No transform defined.*/
	MIR_DCTTYPE_Pass = 0,					/*!<The data value is a scaled index (0 - 1), converted to a color via a LUT.*/
	MIR_DCTTYPE_PassIndex,					/*!<The data value is an integer index (0 - N), converted to a color via a LUT.*/
	MIR_DCTTYPE_PassValue,					/*!<The data value is a color and is not converted.*/
	MIR_DCTTYPE_NBitColor,					/*!<Requires a bit count to be supplied.*/
	MIR_DCTTYPE_Linear,						/*!<Build from supplied bandpass or from statistics.*/
	MIR_DCTTYPE_Log,						/*!<Build from supplied bandpass or from statistics.*/
	MIR_DCTTYPE_Sigmoid,					/*!<Build from supplied bandpass or from statistics.*/
	MIR_DCTTYPE_EqualArea,					/*!<Build from supplied array or from statistics.*/
	MIR_DCTTYPE_EqualAreaNonLinear,			/*!<Build from supplied array or from statistics.*/
	MIR_DCTTYPE_UserLinearTable,			/*!<Build from range and supplied array of index.*/
	MIR_DCTTYPE_UserNonLinearTable,			/*!<Build from supplied array of data values, index.*/
	MIR_DCTTYPE_UserNonLinearPTGTable,		/*!<Build from supplied array of percentage values, index (requires summary statistics).*/
	MIR_DCTTYPE_UserNonLinearPTLTable,		/*!<Build from supplied array of percentile values, index (requires distribution statistics).*/
	MIR_DCTTYPE_UserDiscreteValue,			/*!<Build from supplied arrays of data value, index.*/
	MIR_DCTTYPE_UserDiscreteRange,			/*!<Build from supplied arrays of data range, index.*/
	MIR_DCTTYPE_UserDiscreteString,			/*!<Build from supplied array of strings, index.*/
	MIR_DCTTYPE_BreaksAboutMean,			/*!<Build N breaks about the Mean.*/
	MIR_DCTTYPE_BreaksAboutMedian,			/*!<Build N breaks about the Median.*/
	MIR_DCTTYPE_BreaksAboutMode,			/*!<Build N breaks about the Mode.*/
	MIR_DCTTYPE_BreaksAboutMeanByStdDev,	/*!<Build N breaks about the Mean with Standard Deviation width.*/
	MIR_DCTTYPE_BreaksNatural				/*!<Build N Jenks Natural Breaks.*/
};

/*! \enum	MIR_DataColorUnits
	\brief	Data value units used in data color transforms.
*/
enum MIR_DataColorUnits
{
	MIR_DCUNITS_Undefined		= -1,	/*!<No units defined.*/
	MIR_DCUNITS_Absolute		= 0,	/*!<Data values supplied.*/
	MIR_DCUNITS_Percentage		= 1,	/*!<Percentage of data range (0 - 1) supplied.*/
	MIR_DCUNITS_Percentile		= 2		/*!<Percentile of data distribution (0 - 1) supplied.*/
};

/*! \enum	MIR_DataIndexPosition
	\brief	The position within a data range at which an index value is associated.
*/
enum MIR_DataIndexPosition
{
	MIR_DIPOSITION_Bottom = 0,		/*!<.Index is at bottom of range.*/
	MIR_DIPOSITION_MidPoint, 		/*!<.Index is at the mid point of the range.*/
	MIR_DIPOSITION_Top				/*!<.Index is at the top of the range.*/
};

/*! \enum	MIR_FieldBandFilterMode
	\brief	Copy filter mode for convert.
*/
enum MIR_FieldBandFilterMode
{
	FieldAndBand,				/*!<One field and one band */
	FieldAndBands,				/*!<One field and a selection of bands */
	FieldAndAllBands,			/*!<One field and all bands */
	AllFieldsAndAllBands		/*!<All fields and all bands */
};

/*! \struct	SMIR_FieldBandFilter
	\brief	Structure for defining FieldBand filter.
*/
struct SMIR_FieldBandFilter
{
	MIR_FieldBandFilterMode		nMode;			/*!< Mode to specify the number of fields and bands to be copied while converting a raster from one format into another. */
	uint32_t					nField;			/*!< The field index to copy. */
	uint32_t					nNumBands;		/*!< The number of bands to copy. */
	uint32_t*					pBandIndices;	/*!< The array of band indices to copy. */

	SMIR_FieldBandFilter() :
		nMode(MIR_FieldBandFilterMode::AllFieldsAndAllBands),
		nField(0u),
		nNumBands(0u),
		pBandIndices(nullptr)
	{}

	SMIR_FieldBandFilter(uint32_t ninField) :
		nMode(MIR_FieldBandFilterMode::FieldAndAllBands),
		nField(ninField),
		nNumBands(0u),
		pBandIndices(nullptr)
	{}
};

/*! \struct SMIR_CompressionOptions
	\brief  Structure for defining the compression options for creating a raster.
*/
struct SMIR_CompressionOptions
{
	bool bIsValid;								/*!< Whether options are valid. If not valid default system compression settings will be used. */
	MIR_CompressionType nCompressionType;	/*!< Compression type to use. */
	int32_t nCompressionLevel;						/*!< Compression level to use. */

	SMIR_CompressionOptions(bool bInIsValid) :
		bIsValid(bInIsValid)
	{}

	SMIR_CompressionOptions() :
		bIsValid(false), nCompressionType(MIR_CompressionType::MIR_NoCompression), nCompressionLevel(0u)
	{}
};

/*! \struct SMIR_CreationOptions
	\brief	Structure for defining the options for creating a raster in processing operations.
*/
struct SMIR_CreationOptions
{
	SMIR_CompressionOptions compressionOptions;		/*!< Compression options. */
	MIR_PredictiveEncoding predictiveEncoding;		/*!< Predictive encoding. */

	SMIR_CreationOptions() : compressionOptions(), predictiveEncoding(MIR_PredictiveEncoding::MIR_Encoding_None)
	{}
};

/*! \struct SMIR_FinalisationOptions
	\brief	Structure for defining finalisation options on closing a raster.
*/
struct SMIR_FinalisationOptions
{
	SMIR_FinalisationOptions() : nBuildOverviews(1), nComputeStatistics(1), nStatisticsLevel(4), bDiscard(0), bDelete(0)
	{}
	SMIR_FinalisationOptions(uint32_t _nBuildOverviews,uint32_t _nComputeStatistics) : nBuildOverviews(_nBuildOverviews), nComputeStatistics(_nComputeStatistics), nStatisticsLevel(4), bDiscard(0), bDelete(0)
	{}
	SMIR_FinalisationOptions(uint32_t _nBuildOverviews,uint32_t _nComputeStatistics, 	uint32_t _nStatisticsLevel, 	uint32_t	 _bDiscard, uint32_t _bDelete)
	: nBuildOverviews(_nBuildOverviews), nComputeStatistics(_nComputeStatistics), nStatisticsLevel(_nStatisticsLevel), bDiscard(_bDiscard), bDelete(_bDelete)
	{}
	uint32_t	nBuildOverviews		: 2;	/*!< 0 = no, 1 = if internal, 2 = always */
	uint32_t	nComputeStatistics	: 2;	/*!< 0 = no, 1 = if internal, 2 = always */
	uint32_t	nStatisticsLevel	: 3;	/*!< 0 = none, 1 = Count, 2 = Summary, 3 = Distribution, 4 = Spatial */
	uint32_t	bDiscard			: 1;	/*!< 0 = no, 1 = yes */
	uint32_t	bDelete				: 1;	/*!< 0 = no, 1 = yes (and discard) */
};


/*! \struct SMIR_ClipExtent
\brief	Structure for raster extent which will be used for processing and analysis operations.
*/
struct SMIR_ClipExtent
{
	double		xMin;
	double		yMin;
	double		xMax;
	double		yMax;
	bool		bIsValid;	/*!< Whether clipExtent Option is valid. If not valid use whole grid extent. */


	// ToDo : This constructor need to be removed along with all of the constructor in APIDef.h for C parity.
	SMIR_ClipExtent ( bool bInIsValid ) : bIsValid ( bInIsValid )
	{}
};

/*! \struct SMIR_APIOptions
\brief	Structure for defining the various API options for creating raster, defining finalization options on closing a raster or defining FieldBand filter.
*/
struct SMIR_APIOptions
{
	SMIR_CreationOptions			cCreationOptions;
	SMIR_FinalisationOptions		cFinalisationOptions;
	SMIR_FieldBandFilter			cFieldBandFilter;
	SMIR_ClipExtent					cClipExtent;

	SMIR_APIOptions ( ) : cCreationOptions ( ), cFinalisationOptions ( 0, 0 ), cFieldBandFilter ( ), cClipExtent (false)
	{}

	SMIR_APIOptions ( SMIR_CreationOptions creationOptions, SMIR_FinalisationOptions finalisationOptions, SMIR_FieldBandFilter fieldBandFilter, SMIR_ClipExtent clipExtent)
		: cCreationOptions ( creationOptions ), cFinalisationOptions ( finalisationOptions ), cFieldBandFilter ( fieldBandFilter ), cClipExtent (clipExtent)
	{}
};

/*! \struct SMIR_RasterInput
\brief	Structure for defining the input raster attributes.
*/
struct SMIR_RasterInput
{
	wchar_t					pwsRasterPath[MAX_FILEPATH];		/*!< Input Raster Path.*/
	uint32_t				nField;								/*!< The zero based field index of input raster to be used.*/
	uint32_t				nBand;								/*!< The zero based band index of input raster to be used.*/
	uint32_t				nEvent;								/*!< The zero based event index of input raster to be used.*/

	SMIR_RasterInput() : nField(0), nBand(0), nEvent(0)
	{}
};

/*! \struct SMIR_OutputRasterDetail
\brief	Structure for defining the output raster attributes like raster path, driver, creation options etc.
*/
struct SMIR_OutputRasterDetail
{
	const wchar_t*					pwsOutputRasterFilePath;					/*!< Output raster file path. */
	const wchar_t*					pwsOutputFileDriver;						/*!< The driver to create the output file with. */
	SMIR_CreationOptions			cCreationOptions;							/*!< Options for creating raster. */
	SMIR_FinalisationOptions		cFinalisationOptions;						/*!< Finalisation options required while writing it on disk. */
};

/*****************************************************************************************************************************/
//	Raster information

/*! \enum	MIR_RealNumberRepresentation
	\brief	Representation of a real number, decimal or fraction.
*/
enum MIR_RealNumberRepresentation
{
	MIR_FRACTION,					/*!< Represent in the form of a numerator and a denominator. */
	MIR_DECIMAL						/*!< Represent in decimal form. */
};

/*!	\struct	SMIR_RealNumber
	\brief	A number, represented as either a decimal (double) or a fraction (num/den).
*/
struct SMIR_RealNumber
{
	MIR_RealNumberRepresentation		nType;
	double								m_dDecimal;
	int64_t								m_nNumerator;
	int64_t								m_nDenominator;
};

/*!	\struct	SMIR_Variant
	\brief	A 'variant' for data types <256 characters in size.
*/
struct SMIR_Variant
{
	MIR_DataType				nType;
	uint8_t						nSize;
	uint8_t						ucData[256];
};

/*!	\struct	SMIR_VariantArray
	\brief	An 'array' of 'variants' for data types <256 characters in size and <256 items.
*/
struct SMIR_VariantArray
{
	uint8_t						nSize;
	SMIR_Variant				vVariant[256];
};

/*! \struct SMIR_LevelInfoState
	\brief	Initial and final states for each level property.
*/
struct SMIR_LevelInfoState
{
	char								nResolution[2];
	char								nCellBBox[2];
};

/*! \struct SMIR_LevelInfo
	\brief	Resolution level properties.
*/
struct SMIR_LevelInfo
{
	int32_t				nResolution;
	int64_t				nCellBBoxXMin,nCellBBoxYMin;
	int64_t				nCellBBoxXMax,nCellBBoxYMax;
	//	TODO	Transform
	SMIR_LevelInfoState	DataState;	//	User modifiable variable states
};

/*! \struct SMIR_EventInfoState
	\brief	Initial and final states for each event property.

	When you create a raster you set an initial state for each property and after it has been created you can find out a final state for that property.

	The initial state can be : Default (0), Request (1), or Require (2)
	The Final state can be : OK (0), Warning (1), or Error (2)
*/
struct SMIR_EventInfoState
{
	char								nTime[2];
	char								nEditType[2];
};

/*! \struct SMIR_EventInfo
	\brief	Event information structure.
*/
struct SMIR_EventInfo
{
	//	User modifiable variables
	time_t								nTime;					/*!< Time of the event. */
	int32_t								nEditType;				/*!< Type of the event <code>MIR_EventType</code>. */

	//	User modifiable variable states
	SMIR_EventInfoState					DataState;
};

/*! \struct SMIR_BandInfoState
	\brief	Initial and final states for each band property.

	When you create a raster you set an initial state for each property and after it has been created you can find out a final state for that property
	(See the XQDO class in MINTSystem.h)

	The initial state can be : Default (0), Request (1), or Require (2)
	The Final state can be : OK (0), Warning (1), or Error (2)

	The char arrays below correspond to Initial and Final state values.
	    If you &quot;Request&quot; a property and it is denied, you may receive a &quot;Warning&quot;.
	    If you &quot;Require&quot; a property and it is denied, you may receive an &quot;Error&quot;.
*/
struct SMIR_BandInfoState
{
	char								nType[2];
	char								nName[2];
	char								nXMLMetaData[2];
	char								nUnitCode[2];
	char								nDataType[2];
	char								nStoreDataType[2];
	char								nDiscreteValue[2];
	char								nNullValueType[2],nNullValue[2];
	char								nRestrictDecimals[2],nMaxDecimals[2];
	char								nTransform[2],nScale[2],nOffset[2];
	char								nClip[2],nClipMin[2],nClipMax[2];
	char								nPredictiveEncoding[2];
};

/*! \struct SMIR_BandInfo
	\brief	Structure for defining the composition of a band in a raster.
*/
struct SMIR_BandInfo
{
	MIR_BandType					nType;						/*!< The type of band in the source raster as defined in \link MIR_BandType \endlink. */
	wchar_t							sName[256];					/*!< The name of the band. */
	wchar_t							sXMLMetaData[4096];			/*!< The band metadata. */

	MIR_UnitCode					nUnitCode;					/*!<Defines the MapInfo distance units. */

	MIR_DataType					nDataType;					/*!<Specifies the in-memory data type for this band. */
	MIR_DataType					nStoreDataType;				/*!<Specifies the data type of the band when stored on the disk. */

	bool							bDiscreteValue;				/*!<Denotes whether the value of the cell represents the average value of the measured quantity over the cell region. */

	MIR_NULLType					nNullValueType;				/*!<Specifies the type of null value to be used inside a band. */
	SMIR_Variant					vNullValue;					/*!<Specifies the null value variant for data types <=256 characters in size. */
	bool							bRestrictDecimals;			/*!<Flag to specify whether decimal values need to be removed for band values. */
	int32_t							nMaxDecimals;				/*!<Specifies the maximum decimal precision for band values. */

	bool							bTransform;					/*!<Band offset and scaling options pair &lt;offset,scale&gt;.True, if scaling is required. Else, false.*/
	double							dScale;						/*!< Band scale, valid only if \link SMIR_FieldInfo \endlink \a bTransform is set to True. */
	double							dOffset;					/*!< Band offset, valid only if \link SMIR_FieldInfo \endlink \a bTransform is set to True. */

	bool							bClip;						/*!<Flag to indicate whether clipping to a user-defined bound is enabled inside a band. */
	SMIR_Variant					vClipMin;					/*!<The minimum clip value. */
	SMIR_Variant					vClipMax;					/*!<The maximum clip value. */	

	MIR_PredictiveEncoding			nPredictiveEncoding;		/*!<Flag to indicate whether value of a cell can be estimated from previous column(s). */

	//	User modifiable variable states
	SMIR_BandInfoState				DataState;					/*!<Denotes the modifiable state of the variables inside a band. */
};


/*! \struct SMIR_PointInspCustomColInfo
	\brief	Point inspection data.
*/
struct SMIR_PointInspCustomColInfo
{
	wchar_t szColName[256];	/*!< Column name */
	//	Following parameters are required only in case of Numeric raster
	bool bDefaultNullVal;	/*!< Use default null value*/
	float fNullVal;			/*!< Null value*/
	bool bDefaultNoCellVal;	/*!< Use default no cell value*/
	float fNoCellVal;		/*!< No cell value*/
};

/*! \enum	SMIR_PointInspOutputTABMode
	\brief	Point inspection output mode.
*/
enum SMIR_PointInspOutputTABMode
{
	CREATE_OUTPUT_TAB = 0,	/*!< Create TAB*/
	EDIT_INPUT_TAB			/*!< Edit TAB*/
};

/*! \struct SMIR_ClassTableFieldInfo
	\brief	Structure for defining the properties of a classification table field.
*/
struct SMIR_ClassTableFieldInfo
{
	wchar_t							sName[256];	/*!< Name of the classification table field. */
	MIR_ClassTableFieldType				nType;	/*!< Primary classification table meta data. See \link MIR_ClassTableFieldType \endlink for more details.*/
	MIR_DataType					nDataType;	/*!< Data type of the classification table field. */
};

/*! \struct SMIR_ClassTableInfo
	\brief	Structure for defining the properties of a classification table.
*/
struct SMIR_ClassTableInfo
{
	wchar_t							sName[256];					/*!< Name of the classification table. */
	uint32_t						nFieldCount;				/*!< Number of fields in the classification table.*/
	SMIR_ClassTableFieldInfo		vFieldInfo[256];			//	TODO? Make unlimited?
};

/*! \struct SMIR_CompressionMethod
	\brief	Structure for defining the properties of a compression method.
*/
struct SMIR_CompressionMethod
{
	MIR_CompressionType				nCompressionCodec;
	SMIR_VariantArray				vCompressionParams;
};

/*! \enum MIR_OverviewCellCoverage
\brief	This is used to determine if an overview cell is valid, based on the validity of the four cells that it overlaps in the level below. It is only used when the overview pyramid is created.
*/
enum MIR_OverviewCellCoverage
{
	Coverage_Any		= 1,		/*!< If one or more cells are valid, the overview cell is valid. */
	Coverage_Half,					/*!< If two or more cells are valid, the overview cell is valid, it is also default value. */
	Coverage_Majority,				/*!< If three of more cells are valid, the overview cell is valid. */
	Coverage_Full,					/*!< If all four cells are valid, the overview cell is valid. */
};

/*! \struct SMIR_FieldInfoState
	\brief	Initial and final states for each field property.
*/
struct SMIR_FieldInfoState
{
	char								nType[2];
	char								nName[2];
	char								nXMLMetaData[2];
	char								nCompressionMethod[2];
	char								nValidFlagPerBand[2];
	char								nClassTable[2];
	char								nRegistration[2];
	char								nOverviewCellCoverage[2];
};

/*! \struct SMIR_FieldInfo
	\brief	Structure for defining the Raster Fields Composition.
*/
struct SMIR_FieldInfo
{
	MIR_FieldType					nType;							/*!<Type of the Field. Defined by enum <code>MIR_FieldType</code>. */
	wchar_t							sName[256];						/*!<Name of the Field. */
	wchar_t							sXMLMetaData[4096];
	SMIR_CompressionMethod			cCompressionMethod;
	bool							bValidFlagPerBand;
	SMIR_ClassTableInfo				cClassTable;

	//	Origin coordinate
	/*	The origin is the coordinate of the bottom left corner of cell (0,0).
		When the user creates a legacy raster of fixed size the system will
		interpret this as the bottom left corner of grid cell (0,0). It may
		create some offset between cell and grid coordinates in order to 
		accommodate the legacy raster tiling system and this will shift the 
		origin coordinate.
	*/
	SMIR_RealNumber						cTileOriginX,cTileOriginY;
	//	Cell size at base resolution
	SMIR_RealNumber						cCellSizeX,cCellSizeY;

	//	User modifiable variable states
	SMIR_FieldInfoState					DataState;

	//	The cell range of the raster at base level for all events
	//	This is a read-only property.
	int64_t								nCellBBoxXMin,nCellBBoxYMin;
	int64_t								nCellBBoxXMax,nCellBBoxYMax;

	//	The coordinate range of the grid at base level for all events
	//	This is a read-only property.
	double								dCoordBBoxXMin,dCoordBBoxYMin;
	double								dCoordBBoxXMax,dCoordBBoxYMax;

	//	The cell offset from the tile origin to the grid origin
	//	This is a read-only property.
	int64_t								nCellAtGridOriginX,nCellAtGridOriginY;

	// The size of the grid
	uint64_t							nGridSizeX,nGridSizeY;
	// Decides how overviews pyramid will be generated for this field.
	MIR_OverviewCellCoverage			nOverviewCellCoverage;
};

/*! \struct SMIR_RasterInfoState
	\brief	Initial and final state flags for SMIR_RasterInfo data
*/
struct SMIR_RasterInfoState
{
	char	nVersion[2];			/*!<Version state flags*/
	char	nName[2];				/*!<Name state flags*/
	char	nFileName[2];			/*!<File name state flags*/
	char	nFileList[2];			/*!<File list state flags*/
	char	nGridSize[2];			/*!<Grid size state flags*/
	char	nCoordinateSystem[2];	/*!<Coordinate system state flags*/
	char	nUnderviewMapSize[2];	/*!<Underview map size state flags*/
	char	nUnderviewTileSize[2];	/*!<Underview tile size state flags*/
	char	nBaseMapSize[2];		/*!<Base map size state flags*/
	char	nBaseTileSize[2];		/*!<Base tile size state flags*/
	char	nOverviewMapSize[2];	/*!<Overview map size state flags*/
	char	nOverviewTileSize[2];	/*!<Overview tile size state flags*/
	char	nColour[2];				/*!<Color state flags*/
};

/*! \struct SMIR_RasterInfo
	\brief	Structure for defining the composition of a raster.
*/
struct SMIR_RasterInfo
{
	int32_t								nMajorVersion,nMinorVersion;	/*!<Version numbers of the input raster. */
	wchar_t								sName[256];						/*!<File name of the input raster. */
	wchar_t								sFileName[1024];				/*!<File path of the input raster. */
	char								sDriverID[256];					/*!<The driver to create the output file with. */
	//	TODO	FileList

	uint64_t							nGridSizeX,nGridSizeY;			/*!<Number of columns and rows in the input raster. */

	wchar_t								sCoordinateSystem[MAX_COORDSYS];		/*!<MapInfo coordinate system string. */

	uint32_t							nUnderviewMapSizeX,nUnderviewMapSizeY;	/*!<Size of the raster underview. */
	uint32_t							nUnderviewTileSizeX,nUnderviewTileSizeY; /*!<Size of a tile in the raster underview. */

	uint32_t							nBaseMapSizeX,nBaseMapSizeY;			/*!<Size of the base map. */
	uint32_t							nBaseTileSizeX,nBaseTileSizeY;			/*!<Size of a tile in the base map. */

	uint32_t							nOverviewMapSizeX,nOverviewMapSizeY;	/*!<Size of an overview in the base map. */
	uint32_t							nOverviewTileSizeX,nOverviewTileSizeY;	/*!<Size of a tile in the overview of a base map. */

	//	User modifiable variable states
	SMIR_RasterInfoState				DataState;			/*!<Fields in the \link SMIR_RasterInfoState \endlink structure. */
};

/*****************************************************************************************************************************/
//	Statistics

/*!	\enum	MIR_StatisticsMode
	\brief
*/
enum MIR_StatisticsMode
{
	MIR_StatsMode_None			= 0,	//	No statistics
	MIR_StatsMode_Count,				//	Sample count and validity statistics
	MIR_StatsMode_Summary,				//	Count + Summary statistics
	MIR_StatsMode_Distribution,			//	Count + Summary + Histogram
	MIR_StatsMode_Spatial				//	Count + Summary + Histogram + cell to cell statistics
};

/*!	\struct	SMIR_HistogramBin
	\brief	A histogram bin.
*/
struct SMIR_HistogramBin
{
	//	The range of the bin - continuous from bin to bin
	double								dBottom;			/*!<Bottom of bin. Data >= dBottom*/
	double								dTop;				/*!<Top of bin. Data <=  dTop*/
	//	The range of the actual samples in the bin
	double								dValBottom;			/*!<Minimum value in the bin*/
	double								dValTop;			/*!<Maximum value in the bin*/
	//	Count of samples in bin
	double								dCount;				/*!<Number of samples in the bin*/
	//	Percentage of samples up to and including this bin >0 and <=1
	double								dCumulativeCount;	/*!<Cumulative percentage of samples including this bin.*/		
};

/*!	\struct	SMIR_Histogram
	\brief	A histogram.
*/
struct SMIR_Histogram
{
	uint32_t						nBinCount; /*!< Count of bins.*/
	SMIR_HistogramBin*				pvcBins;   /*!< Collection of bins.*/
};

/*!	\struct	SMIR_Statistics
	\brief	Statistical data.
*/
struct SMIR_Statistics
{
	//	Count statistics
	//	Acquired with Mode MIR_StatsMode_Count & MIR_StatsMode_Summary & MIR_StatsMode_Distribution & MIR_StatsMode_Spatial
	uint64_t						nSampleCount;			/*!< Total Sample Count. */
	uint64_t						nValidSampleCount;		/*!< Total valid sample count. */
	uint64_t						nInvalidSampleCount;	/*!< Total invalid sample count */
	uint64_t						nInvalidNumberCount;	/*!< Number of valid samples with invalid values */

	//	Valid cell extent, in cell coordinates, inclusive
	int64_t							nValidCellMinX;			/*!< Cell origin X coordinate of the valid cell range*/
	int64_t							nValidCellMinY;			/*!< Cell origin Y coordinate of the valid cell range*/
	int64_t							nValidCellMaxX;			/*!< Cell extent X coordinate of the valid cell range*/
	int64_t							nValidCellMaxY;			/*!< Cell extent Y coordinate of the valid cell range*/

	//	Summary statistics
	//	Acquired with Mode MIR_StatsMode_Summary & MIR_StatsMode_Distribution & MIR_StatsMode_Spatial
	double							dMin;					/*!< Minimum sample value */
	double							dMax;					/*!< Maximum sample value */
	double							dMean;					/*!< Mean Value */
	double							dVariance;				/*!< Variance of the valid sample count */
	double							dStdDev;				/*!< First standard deviation */
	double							dSignal2Noise;			/*!< Signal to noise value */

	//	Histogram
	//	Acquired with Mode MIR_StatsMode_Distribution & MIR_StatsMode_Spatial
	SMIR_Histogram					cHistogram;				/*!< Histogram Structure representing equal frequency histogram */
	SMIR_Histogram					cEWHistogram;			/*!< Histogram Structure representing equal width histogram */

	//	Cell to cell difference statistics
	//	Acquired with Mode MIR_StatsMode_Spatial
	uint64_t						nC2CSampleCount;		/*!< Total samples considered for cell to cell statistics calculation */
	double							dC2CMin;				/*!< Minimum cell to cell difference */
	double							dC2CMax;				/*!< Maximum cell to cell difference */
	double							dC2CMean;				/*!< Mean of cell to cell differences  */
	double							dC2CVariance;			/*!< Variance of cell to cell differences  */
	double							dC2CStdDev;				/*!< Standard deviation of cell to cell differences  */

	MIR_StatisticsMode				nStatMode;				/*!< Statistics mode in which statistics are actually calculated/dicovered and returned. */
};

/*****************************************************************************************************************************/
//	Process tracker

/*!	\enum	ProgressMessageType
	\brief	Type of message string being returned in the ProcessProgress callback structure for progress tracking.
*/
enum ProgressMessageType
{
	ProgressMessage_None = 0,		/*!<no type. */
	ProgressMessage_ProcessTitle,	/*!<The message string is a process title. */
	ProgressMessage_TaskTitle,		/*!<The message string is a task title. */
	ProgressMessage_Update,			/*!<The message string is a user interface update. */
	ProgressMessage_Information		/*!<The message string contains information. */
};

/*! \struct SMIR_ProcessProgress
	\brief	Process progress structure. Gets returned to the callback method for progress tracking.
*/
struct SMIR_ProcessProgress
{
	uint32_t		nTrackerHandle;		/*!<Tracker handle. */
	int32_t			nMsgType;			/*!<ProgressMessageType of the message. */
	int32_t			nMessageLogType;	/*!<Type of the log message. */
	wchar_t			sMessage[512];		/*!<Message.*/
	double			dProcessProgress;	/*!<Current process progress (total).*/
	double			dTaskProgress;		/*!<Current task progress*/
};

typedef void (*ProgressCallback) (SMIR_ProcessProgress *progress);

/*****************************************************************************************************************************/
//	Operations

/*!	\enum	MIR_CoincidentPointMethod
\brief	Types of coincident point method used to analyse coincident points when reading input data points for raster interpolation.
*/
enum MIR_CoincidentPointMethod
{
	MIR_None = 0,
	MIR_First,
	MIR_Last,
	MIR_FirstStation,
	MIR_LastStation,
	MIR_Minimum,
	MIR_Maximum,
	MIR_MidPoint,
	MIR_Mean
};

/*!	\enum	MIR_InterpolationMode
	\brief	Types of interpolation modes supported by the MIRaster IO API for the point interpolator.
*/
enum MIR_InterpolationMode
{
	MIR_Point					= 0,
	MIR_Integration,
	MIR_AreaWeighted,
	MIR_AreaMax
};

/*! \enum	MIR_PointInterpolationMethod
	\brief	Enum for defining the various types of point interpolation methods supported by the MIRaster IO API.
*/
enum MIR_PointInterpolationMethod
{
	MIR_Nearest					= 0,		// Nearest neighbour interpolation
	MIR_Bilinear,							// Bilinear interpolation
	MIR_Bicubic								// Bicubic interpolation
};

/*! \enum	MIR_InterpolationMethod
	\brief	Enum for defining the various types of interpolation methods supported by the MIRaster IO API for overviews and underviews.
*/
enum MIR_InterpolationMethod
{
	Interp_Nearest		= 0,				// Nearest neighbour interpolation
	Interp_Linear		= 1,				// linear interpolation
	Interp_CubicOperator= 2,				// cubic interpolation (local)
	Interp_Cubic		= 3,				// cubic interpolation (global)
	Interp_Default							//	Always make this the last entry
};

/*! \enum	MIR_InterpolationNullHandlingMode
	\brief	Enum for defining the various ways of handling null values for during the interpolation methods supported by the MIRaster IO API.
*/
enum MIR_InterpolationNullHandlingMode
{
	MIR_AnyInvalid					= 0,	//  Null if any part of input is invalid
	MIR_FiftyPercentOrMoreInvalid,			//  Null if 50% or more of the input is invalid
	MIR_MoreThanFiftyPercentInvalid,		//  Null if more than 50% of the input is invalid
	MIR_AllInvalid							//  Null if 100% of the input is invalid
};

/*! \enum	MIR_MergeOperator
	\brief	Enum for defining the various Merge operators when input grid cells are overlapping.
*/
enum MIR_MergeOperator
{
	MIR_Stamp					=0, //Use cell value of last Grid of input Grids
	MIR_Min,						//Use Min cell value
	MIR_Max,						//Use Max cell value
	MIR_Average,					//Use Average value of all the overlapping cells
	MIR_Sum,						//Use Sum of all the overlapping cells
	MIR_Median,						//Use Median Value
	MIR_AvgMinMax,					//Use Average of min / max
	MIR_Count					//Use count of all overlapping cells
	//TO DO
	/*MIR_First,
	MIR_Last*/

};

/*! \enum	MIR_MergeMRTMode
\brief	Enum for defining various Merge MRT modes to guide the decimation factor for output raster block, when input grid cells are overlapping.
*/
enum MIR_MergeMRTMode
{
	MIR_OptimumMin = 0,				//Decimation factor will be decided around Minimum resolution of all overlapping Rasters
	MIR_OptimumMax,					//Decimation factor will be decided around Maximum resolution of all overlapping Rasters
	MIR_MRTStamp					//Decimation factor will be decided based on order of overlapping input Raster resolutions
};

/*! \enum	MIR_MergeType
	\brief	Enum for defining the various Merge operators when input grid cells are overlapping.
*/
enum MIR_MergeType
{
	MIR_Union					=0, //Merge by union of inputGrids
	MIR_UserRect					//Merge inside user specified rectangle
};

/*! \enum	MIR_RasterizeForegroundValueType
	\brief	Enum for defining possible foreground value types for VectorToGrid operation.
*/
enum MIR_RasterizeForegroundValueType
{
	MIR_Rasterize_Null = 0, //Use null as foreground value
	MIR_Rasterize_Value,	//Use constant foreground value
	MIR_Rasterize_Field		//Use source table fields value

};

/*!	\enum	MIR_RasterizeOperator
	\brief
*/
enum MIR_RasterizeOperator
{
	MIR_Rasterize_First					=0, //Use Field value of First vector feature 
	MIR_Rasterize_Last,						//Use Field value of Last vector feature 
	MIR_Rasterize_Min,						//Use Min Field Value from overlapping Vectors
	MIR_Rasterize_Max,						//Use Max Field Value from overlapping Vectors
	MIR_Rasterize_Average,					//Use Average Field Value of overlapping Vectors
	MIR_Rasterize_Sum,						//Use Sum of all the overlapping Vectors field value
	MIR_Rasterize_Median,					//Use Median of all the overlapping Vectors field value
	MIR_Rasterize_Range						//Use Range of all the overlapping Vectors field value
};

/*!	\enum	MIR_IntegrationInterpolationTests
	\brief	Enum for defining the number of iterations to be used when using the MIR_Integration interpolation method.
*/
enum MIR_IntegrationInterpolationTests
{
	MIR_2x2						= 0,		//
	MIR_4x4,
	MIR_8x8
};

/*!	\enum	MIR_HeatMapType
	\brief	Type of heat map operation to be performed.
*/
enum MIR_HeatMapType
{
	HeatMap_Estimate			= 0,	/*!< Smooth sample density estimate. */
	HeatMap_WeightedEstimate,			/*!< Smooth weighted sample density estimate. */
	HeatMap_SampleCount,				/*!< Sample count integration. */
	HeatMap_SampleDensity,				/*!< True sample spatial density integration. */
	HeatMap_Advanced					/*!< Advanced access to all properties. */
};

/*****************************************************************************************************************************/

/*! \struct SMIR_PolygonStatFlags
	\brief	Structure to define the flags that specifies the type of stats for the cell values lying in each polygon region(s) contained in a vector file.
*/
struct SMIR_PolygonStatFlags
{
	//Flags indicating if the respective stat val is required
	bool bFlgMin; 				/*!< The minimum value of cell(s) in the region(s). */
	bool bFlgMax;				/*!< The maximum value of cell(s) in the region(s). */
	bool bFlgMean;				/*!< The mean value of cell(s) in the region(s). */
	bool bFlgMedian;			/*!< The median value of cell(s) in the region(s). */
	bool bFlgMode;				/*!< The mode value of cell(s) in the region(s). */
	bool bFlgRange;				/*!< The range of values of cell(s) in the region(s). */
	bool bFlgStdDev;			/*!< The standard deviation of the cell(s) in the region(s). */
	bool bFlgLowerQuart;		/*!< The upper quartile of the cell(s) in the region(s). */
	bool bFlgUpperQuart;		/*!< The lower quartile of the cell(s) in the region(s). */
	bool bFlgInterQuartRange;	/*!< The interquartile (IQR) of the cell(s) in the region(s). */
	bool bFlgNodeSum;			/*!< The sum of all the cell(s) in the region(s). */
	bool bFlgPctNullCells;		/*!< The percentage of null cell(s) in the region(s). */
	bool bFlgCoeffOfVariance;	/*!< The coefficient of variance of the cell(s) in the region(s). */
	bool bFlgNumCells;			/*!< The count of all the non-null cell(s) in the region(s). */
	bool bFlgNumNullCells;		/*!< The count of all the null cell(s) in the region(s). */
};

/*! \struct SMIR_LineStatFlags
	\brief	Structure to define the flags for statistical values to be used in the \link MIR_GetLineStats \endlink API.
*/
struct SMIR_LineStatFlags
{
	//Flags indicating if the respective stat val is required
	bool bFlgMin;					/*!<The minimum cell value on the line or polyline. */
	bool bFlgMax;					/*!<The maximum cell value on the line or polyline. */
	bool bFlgMean;					/*!<The mean of all cells on the line or polyline. */
	bool bFlgRange;					/*!<The range of all cells on the line or polyline. */
	bool bFlgStartVal;				/*!<The value if the first cell on the line or polyline. */
	bool bFlgEndVal;				/*!<The value of the last cell on the line or polyline. */
	bool bFlgMiddleVal;				/*!<The value of the middle cell on the line or polyline. */
};

/*****************************************************************************************************************************/

/*!	\enum	SMIR_GridClassificationType
	\brief	Deprecated.
*/
enum SMIR_GridClassificationType
{
	GridClassificationType_Numeric,
	GridClassificationType_Classified
};

/*!	\struct	SMIR_ClassificationInfo
	\brief	Classification parameters.
*/
struct SMIR_ClassificationInfo
{
	float fFromVal;				/*!< All original grid cells with a value greater than or equal to fBottomVal and less than fTopVal */
	float fToVal;				/*!< All original grid cells with a value greater than or equal to fBottomVal and less than fTopVal */
	float fNewGridValue;		/*!< Value to use in the new grid when the original grid value is greater than or equal to gridValue */
	wchar_t newClassName[255];	/*!< The class name to use when creating a classified grid*/
	uint32_t newClassColor;		/*!< RGB color in format Byte0: Red, Byte1: Green, Byte2: Blue */
};

/*!	\struct	SMIR_ReclassInfo
	\brief	Reclassification data.
*/
struct SMIR_ReclassInfo
{
	wchar_t szClassLabel[255];	/*!< Existing class Label in the grid */
	uint32_t nClassValue;		/*!< Existing class value*/
	wchar_t newClassName[255];	/*!< The class name to use when creating a classified grid */
	uint32_t nNewClassVal;		/*!< New class value*/
	bool bOutNullClass;			/*!< Output nulls when classification fails*/
	/*!	\struct	SMIR_ReclassInfo::colorInfo
		\brief	Reclassification color data.
	*/
	struct colorInfo
	{
		uint8_t redVal;
		uint8_t greenVal;
		uint8_t blueVal;
		uint32_t RGBVal;
	} newClassColor;			/*!< New class color */
};

/*!	\struct	SMIR_TableRecordEditInfo
	\brief	Class index and its corresponding new color or new label.
*/
struct SMIR_TableRecordEditInfo
{
	uint32_t		nClassIndex;					/* Class Index of the record to be modified. */
	uint32_t		newClassColor;					/* New color to be assigned.*/
	wchar_t*		pszNewClassName;				/* New label  to be assigned.*/

	SMIR_TableRecordEditInfo() : newClassColor(InvalidColor), pszNewClassName(0)
	{}
};

/*****************************************************************************************************************************/
//	Contouring

/*! \struct SMIR_ContourStyle
	\brief	Structure for defining contour style.
*/
struct SMIR_ContourStyle
{ 
	SMIR_ContourStyle() :
		nLineWidth(1), nLinePattern(2), nLineColour(0),
		nBrushPattern(2), nBrushForeColour(0), nBrushBackColour(0)
	{}

	int32_t				nLineWidth;			/*!< style line width */
	int32_t				nLinePattern;		/*!< style line pattern */
	uint32_t			nLineColour;		/*!< style line colour in RGB format */
	int32_t				nBrushPattern;		/*!< style brush pattern */
	uint32_t			nBrushForeColour;	/*!< style brush foreground colour in RGB format */
	uint32_t			nBrushBackColour;	/*!< style brush background colour in RGB format */
};

/*! \struct SMIR_ManualContourLevel
	\brief	Structure for defining manual contouring levels.
*/
struct SMIR_ManualContourLevel
{ 
    double				dLevel;	/*!< Contour level value. */ 
	SMIR_ContourStyle	cStyle;	/*!< Manual contour style. */
};

/*! \struct SMIR_ContourOptions
	\brief	Structure for defining all the options for contouring a raster.
*/
struct SMIR_ContourOptions
{
	SMIR_ContourOptions() : nInterpolationMethod(Interp_Default), nLevelType(0), dConstantSpacing(100.0), dMinContour(0.0), 
		dMaxContour(0.0), nMajorStep(0), bSetMinMaxContourLevel(false), nManualLevels(0), pManualLevels(nullptr),
		nRangeType(0), dMinX(0.0), dMinY(0.0), dMaxX(0.0), dMaxY(0.0), bColourFromSourceRaster(false),
		bCreateSeamlessTAB(false), bAutoRegions(false), nAutoRegionCellsX(0), nAutoRegionCellsY(0),
		bUseMinimumPolygonArea(false), dMinimumPolygonArea(0.0)
	{
	}

	MIR_InterpolationMethod			nInterpolationMethod;	/*!< interpolation method to use for underviews, that is if resolution is less than 0 */

	int32_t							nLevelType;				/*!< 0 = constant, 1 = manual, 2 = constantminmax */ 

	// constant and constantminmax level options
	double							dConstantSpacing;		/*!< contour spacing if nLevelType = 0 or 2 */
	double							dMinContour;			/*!< minimum contour level */
	double							dMaxContour;			/*!< maximum contour level */
	int32_t							nMajorStep;				/*!< 0 = no major contours, 1 or greater is step factor of constant spacing */
	bool							bSetMinMaxContourLevel; /*!< true = use dMinContour and dMaxContour levels for min and max contour level, false = use all levels */

	// manual level options
	int32_t							nManualLevels;			/*!< count of levels defined in pdManualLevelValue and pbManualLevelMajor, maximum number is 256 */
	SMIR_ManualContourLevel*		pManualLevels;			/*!< list of manual contour level values defined by SMIR_ManualContourLevel */

	// contour region options
	int32_t							nRangeType;				/*!< 0 = complete, 1 = manually defined range */
	double							dMinX;					/*!< Minimum X coordinate for manual range */
	double							dMinY;					/*!< Minimum Y coordinate for manual range */
	double							dMaxX;					/*!< Maximum X coordinate for manual range */
	double							dMaxY;					/*!< Maximum Y coordinate for manual range */

	// major and minor line/brush styles
	bool							bColourFromSourceRaster;/*!< use color mapping defined in the associated ghx file if it exists */
	SMIR_ContourStyle				cMajorStyle;			/*!< major contour style. */
	SMIR_ContourStyle				cMinorStyle;			/*!< minor contour style. */

	bool							bCreateSeamlessTAB;		/*!< creates a seamless tab composed of all the output files that may have been created. only created if more than one output file. */

	// Automatically break large rasters into regions when performing contouring
	bool							bAutoRegions;			/*!< break raster into regions to contour, if nAutoRegionCellsX or nAutoRegionCellsY is equal to zero then the system will choose the size */
	uint32_t						nAutoRegionCellsX;		/*!< user defined cell count in X of region size */
	uint32_t						nAutoRegionCellsY;		/*!< user defined cell count in Y of region size */

	// remove polygon contours under a specified area
	bool							bUseMinimumPolygonArea; /*!< true=remove all polygons under dMinimumPolygonArea in size, false=keep all polygons */
	double							dMinimumPolygonArea;	/*!< minimum polygon area size */
};

/*!	\enum	MIR_PolygoiniseType
	\brief Enum to describe how raster data needs to be polygonised.
*/
enum MIR_PolygoiniseType
{
	SameValueCells		= 0,								/*!< Indicates to polygonise all connected cells of same value */
	ValidInvalid,											/*!< Indicates to polygonise Valid and invalid cells irrespective of valid cell values.*/
	RasterExtent,											/*!< Indicates to polygonise as per the extent of the raster.*/
	UserDefinedRange,										/*!< Indicates to polygonise as per the range provided by the user. */
	ColourRange												/*!< Indicates to polygonise colour of the cells, this is valid for image type raster.*/
};

/*!	\struct	SMIR_PolygonisationLevel
	\brief This describes a range of cells with a value greater than or equal to dFromVal and less than dTopVal.
*/
struct SMIR_PolygonisationLevel
{
	double								dFromVal;			/* Bottom value of the range */
	double								dToVal;				/* Top value of the range */
	SMIR_ContourStyle					cStyle;				/* contour style to be applied to this range. */
};

/*!	\struct	SMIR_PolygoniseParameter
	\brief Structure to describe various parameters for the Polygonise API.
*/
struct SMIR_PolygoniseParameter
{
	bool								bFillFromSourceRaster;			/* Use color mapping defined in the associated ghx file if it exists will be used to fill the color of the polygons.*/
	bool								bOutlineFromSourceRaster;		/* The polygon will be outlined using the color of the source raster else will be defaulted to black. */
	uint32_t							nCount;							/* The count of levels in pPolygonisationLevels array. */
	SMIR_PolygonisationLevel*			pPolygonisationLevels;			/* An array of levels to be applied. */
};

/*****************************************************************************************************************************/

/*!	\struct	RasterDPNT
	\brief Structure to describe a spatial point location.
*/
struct RasterDPNT
{
	double x;									/*!< X location of the point. */
	double y;									/*!< Y location of the point. */
};

/*!	\enum	SamplePointStatus
	\brief	Cell validity status.
*/
enum SamplePointStatus
{
	SamplePoint_Valid = 0,						/*!< Point has valid value */
	SamplePoint_Null,							/*!< Point has null value */
	SamplePoint_OffGrid							/*!< Point is outside grid bounds */
};

/*! \struct SMIR_RasterXSection
	\brief	Structure for defining the properties of a XSection sample point.
*/
struct SMIR_RasterXSection
{
	double					dX;					/*!< The X coordinate of the sample point.*/
	double					dY;					/*!< The Y coordinate of the sample point.*/
	double					dValue;				/*!< The value of the sample.*/
	double					dDistance;			/*!< The distance in MapInfo units that a sample point covers.*/
	SamplePointStatus		nPointStatus;		/*!< The validity of the sample point in a cross section.*/

	SMIR_RasterXSection() : dX(0.0), dY(0.0), dValue(0), dDistance(0), nPointStatus(SamplePointStatus::SamplePoint_Valid)
	{
	}
};

/*! \struct SMIR_RasterXSectionData
\brief	This structure describes the data returned by the XSection API.
*/
struct SMIR_RasterXSectionData
{
	uint32_t				nField;					/*!< The zero-based field index of input raster to which this XSection data belongs to.*/
	uint32_t				nBand;					/*!< The zero-based band index of input raster to which this XSection data belongs to.*/
	uint32_t				nCount;					/*!< The count of sample points in pRasterXSection array.*/
	SMIR_RasterXSection		*pRasterXSection;		/*!< An array of sample points.*/
};


/*****************************************************************************************************************************/
//	Surface analysis

/*! \enum	SurfaceAnalysisType
	\brief
*/
enum SurfaceAnalysisType
{
       MIR_Slope        = 0,
       MIR_Aspect       = 1,
       MIR_Curvature    = 2,
};

/*!	\enum	CurvatureType
	\brief
*/
enum  CurvatureType
{
	MIR_Surface      = 0,
	MIR_Profile      = 1,
	MIR_Plan         = 2
};

/*!	\enum	SlopeType
	\brief
*/
enum  SlopeType
{
	MIR_SLOPETYPE_Degree			=		0,
	MIR_SLOPETYPE_Percentage
};

/*! \struct SMIR_SurfaceAnalysisOptions
	\brief	Structure for defining the type of raster to be created using Surface Analysis APIs.
*/
struct SMIR_SurfaceAnalysisOptions
{ 
	CurvatureType	Curvature;		/*!< Defines the type of curvature grid - Surface, Plan, and Profile.*/
	SlopeType		SlopeUnits;		/*!< Calculate the slope in degrees or percentages.*/

	SMIR_SurfaceAnalysisOptions() : Curvature (CurvatureType::MIR_Surface), SlopeUnits(SlopeType::MIR_SLOPETYPE_Degree)
	{
	}
};

/*! \enum	VolumeMethod
	\brief	Enum to indicate whether volume to be computed against the constant plane or against the secondary raster.
*/
enum VolumeMethod
{
	ConstantPlane = 0,			/*!< Indicates to compute volume against the constant plane.*/
	BetweenRasters				/*!< Indicates to compute volume against the secondary raster.*/
};

/*! \enum	VolumeAction
	\brief	Volume action with respect to primary raster.
*/
enum VolumeAction
{
	AboveRaster = 0,								/*!< Compute volume above primary raster, i.e. only for those areas where primary raster is below.*/
	BelowRaster,									/*!< Compute volume below primary raster, i.e. only for those areas where primary raster is above.*/
	Between											/*!< Compute the volume between the two rasters.*/
};

/*! \struct SMIR_VolumeParameters
	\brief	struct to define volume API parameters.
*/
struct SMIR_VolumeParameters
{
	VolumeMethod				nVolumeMethod;					/*!< Indicates whether to compute volume against the plane or secondary raster.*/
	VolumeAction				nVolumeAction;					/*!< Indicates whether to compute volume below or above the primary raster.*/
	SMIR_RasterInput			cSecondaryRaster;				/*!< Secondary raster details, needed only if nVolumeMethod is VolumeMethod::BetweenRasters.*/
	double						dZContantPlane;					/*!< Constant plane value, needed only if nVolumeMethod is VolumeMethod::ConstantPlane.*/
	MIR_UnitCode				nRasterVerticalUnit;			/*!< Specify the Z-unit if it is different than horizontal unit, if it is MIR_UnitCode::MIR_Undefined it is considered same as horizontal.*/
	MIR_UnitCode				nVolumeOutputUnit;				/*!< Specify the unit in which volume output is desired.*/
};

/*! \struct SMIR_VolumeOutput
\brief	Volume API computed volume and cell count matching the criteria.
*/
struct SMIR_VolumeOutput
{
	double						dVolume;				/*!< Computed volume.*/
	uint64_t					nCellCount;				/*!< Total cells count in input raster which matched the criteria.*/
	MIR_UnitCode				nVolumeUnit;			/*!< Computed volume unit, it is same as SMIR_VolumeParameters::nVolumeOutputUnit if specified.*/
};

/*! \struct SMIR_Viewpoint
	\brief	Structure for defining the viewpoint for LineOfSight API.
*/
struct SMIR_Viewpoint
{
	double						dOriginX;				/*!< Represents the X origin. */
	double						dOriginY;				/*!< Represents the Y origin. */
	double						dOffset;				/*!< Represents the height above the terrain. */

	SMIR_Viewpoint() : dOriginX(0), dOriginY(0), dOffset(0){}
};

/*! \enum EarthCurvatureModel
	\brief	Enum for defining the earth curvature model.
*/
enum EarthCurvatureModel
{
	NoCorrection,				/*!< No earth correction. */
	Normal,						/*!< Normal earth correction.In this case, the corrected earth's radius will be (6,378,137) meters * 1.*/
	FourThird,					/*!< 4 / 3 earth correction.In this case, the corrected earth's radius will be (6,378,137) meters * (4/3). */
	TwoThird					/*!< 2 / 3 earth correction.In this case, the corrected earth's radius will be (6,378,137) meters * (2/3). */
};

/*! \struct SMIR_LineOfSightPoint
	\brief	Structure for defining the properties of sample point to be created for a raster.
*/
struct SMIR_LineOfSightPoint
{
	double						dX;						/*!< The X coordinate of the sample point. */
	double						dY;						/*!< The Y coordinate of the sample point. */
	double						dValue;					/*!< The value of the sample. */
	double						dDistance;				/*!< The distance in MapInfo units that a sample point covers. */
	SamplePointStatus			nPointStatus;			/*!< The validity of the sample point. */
	bool						bVisible;				/*!< Whether point is visible from the observer. */
	double						dOffsetRequired;		/*!< If point is not visible, relative offset adjustment required to raise the height of the sample point to make it visible from source. */

	SMIR_LineOfSightPoint() : dX(0), dY(0), dValue(0), dDistance(0), nPointStatus(SamplePointStatus::SamplePoint_Null), bVisible(false), dOffsetRequired(0)
	{}
};

/*! \enum	LineOfSightOutputType
	\brief	Enum to indicate whether line or point geo-object is required in the output TAB file.
*/
enum LineOfSightOutputType
{
	Lines,					/*!< Indicates a line geo-object connecting points is required in the output TAB file. */
	Points					/*!< Indicates a point geo-object is required in the output TAB file. */
};

/*! \struct SMIR_LineOfSightParameters
	\brief	Structure for defining the input parameters for LineOfSight API. Value of nSampleCount should be passed as zero for default behaviour.
*/
struct SMIR_LineOfSightParameters
{
	SMIR_Viewpoint				srcViewPoint;				/*!< Represents source view point location and height.*/
	SMIR_Viewpoint				destViewPoint;				/*!< Represents destination view point location and height.*/
	MIR_UnitCode				nParameterUnits;			/*!< Units for parameters such as view point height. */
	MIR_UnitCode				nRasterVerticalUnits;		/*!< Specifies the grid vertical units (Z values of the grid). Represented by MapInfo units. If no vertical unit is specified in the call to this method, then it defaults to the same as the horizontal unit of the grid. */
	MIR_UnitCode				nDistanceUnitCode;			/*!< User specified unit code in which distance of the sample point from the observer to be represented. */
	EarthCurvatureModel			nEarthCurvatureModel;		/*!< Represents the earth curvature correction. If the curvature correction is zero, the vertical units (Z values of the grid) will be the same as horizontal units.*/
	LineOfSightOutputType		nOutputType;				/*!< Indicates whether to output lines or points in output TAB file. */
	uint32_t					nSampleCount;				/*!< Number of times distance should be sampled at equal distance. if passed as 0 API will create one point on each cell along the route, 
															if passed a valid value the assumption is that an approximation is required rather than complete and accurate result.*/
};

/*! \struct SMIR_LineOfSightOutputData
\brief	Structure to define output of LineOfSight API.
*/
struct SMIR_LineOfSightOutputData
{
	uint32_t					nCount;						/*!< Number of points in the array ppLineOfSightPoints.*/
	SMIR_LineOfSightPoint		**ppLineOfSightPoints;		/*!< If a valid pointer is passed, API returns array of sample points between source viewpoint and destination. Later this pointer should be passed to MIR_ReleaseData API to release memory. */
	bool						bEndPointVisible;			/*!< Indicates whether end point is visible from source.*/

	SMIR_LineOfSightOutputData() : nCount(0), ppLineOfSightPoints(0), bEndPointVisible(false) {}
};



/*! \struct SMIR_CombineRasterBandInfo
\brief	Structure to represent a band to be created in output raster.
*/
struct SMIR_CombineRasterBandInfo
{
	SMIR_RasterInput			sRasterInputs;						/*!< Input raster detail like raster path, band id etc.*/
	wchar_t*					pwsBandName;						/*!< New band name to be assigned.*/

	SMIR_CombineRasterBandInfo() : sRasterInputs(), pwsBandName(0)
	{
	}
};

/*! \struct SMIR_CombineRasterEventInfo
\brief	Structure to represent an event to be created in output raster.
*/
struct SMIR_CombineRasterEventInfo
{
	time_t						nStartTime;							/*!< Start time of the event.*/
	time_t						nEndTime;							/*!< End time of the event.*/
	MIR_EventType				nEventType;							/*!< Event type.*/
	uint32_t					nBandCount;							/*!< size of the pCombineRasterBandInfo array.*/
	SMIR_CombineRasterBandInfo*	pCombineRasterBandInfo;				/*!< Array of bands to be created in output raster.*/

	SMIR_CombineRasterEventInfo() : nEventType(MIR_EET_Total), nBandCount(0), pCombineRasterBandInfo(0)
	{
	}
};


/*! \struct SMIR_CombineRasterFieldInfo
\brief	Structure to represent multiple raster to form different fields and their bands in the output raster.
*/
struct SMIR_CombineRasterFieldInfo
{
	wchar_t*						pwsFieldName;						/*!< New field Name.*/
	SMIR_CompressionOptions			cCompressionOptions;				/*!< Compression options for each field.*/
	bool							bValidFlagPerBand;					/*!< Indicates whether cell validity flag to be saved per band.*/

	uint32_t						nBandCount;							/*!< Size of the pCombineRasterBandInfo array.*/
	SMIR_CombineRasterBandInfo*		pCombineRasterBandInfo;				/*!< Array that represents all the bands to be created.*/

	uint32_t						nEventCount;						/*!< Size of the pCombineRasterEventInfo array.*/
	SMIR_CombineRasterEventInfo*	pCombineRasterEventInfo;			/*!< Array that represents all the events to be created.*/

	SMIR_CombineRasterFieldInfo() : pwsFieldName(0), cCompressionOptions(SMIR_CompressionOptions()), bValidFlagPerBand(true),
		nBandCount(0), 
		pCombineRasterBandInfo(0), 
		nEventCount(0), 
		pCombineRasterEventInfo(0)
	{
	}
};

/*****************************************************************************************************************************/

/*! \struct SMIR_ExportToTabOptions
\brief	Structure for defining the common ExportToTab parameters
*/
struct SMIR_ExportToTabOptions
{
	double		dNullValue;
	bool		bWriteNullCells;
	bool		bOutputColor;
	bool		bWriteCellXYCoordinates;
	bool		bIsXYLLCorner;
	bool		bRectangleOutPut;
	bool		bIsValid;	/*!< Whether SMIR_ExportToTabOptions Option is valid. If not valid use defaults */


	// ToDo : This constructor need to be removed along with all of the constructor in APIDef.h for C parity.
	SMIR_ExportToTabOptions() : bIsValid(false)
	{}
};

/*****************************************************************************************************************************/

/*! \struct SMIR_ExportGridOptions
	\brief	Structure for defining the common SMIR_ExportGridEx parameters
*/
 struct SMIR_ExportGridOptions
  {
	double			dNullValue;
	uint32_t		nMaxDecimals;
	bool			bWriteCellXYCoordinates;
	bool			bIsXYLLCorner;
	bool			bIsOriginTopLeft;
	wchar_t			wcDelimiter;
	bool			bIsValid;	/*!< Whether SMIR_ExportToTabOptions Option is valid. If not valid use defaults */
	
	// ToDo : This constructor need to be removed along with all of the constructor in APIDef.h for C parity.

	SMIR_ExportGridOptions() : bIsValid(false)
	{
	}
};

/*****************************************************************************************************************************/
//	Viewshed

/*! \struct SMIR_ViewshedCommonParameters
	\brief	Structure for defining the common viewshed parameters to be used while calculating viewshed for the single and multiple towers.
*/
struct SMIR_ViewshedCommonParameters
{
	bool bSpecifyParameterUnits;			/*!< Boolean flag to specify the parameter units. If set to false, they are assumed to be the same as the horizontal units of the raster. */
	MIR_UnitCode nParameterUnits;		/*!< Units for parameters such as view point height, viewshed offset, and radius. Used only if above \c bSpecifyParameterUnits is true. */
	bool bSpecifyRasterVerticalUnits;		/*!< This parameter depends on the earth correction model. The horizontal units are calculated from the input raster's projection information. If no projection information is available or the raster's projection is in Lat/Long, the horizontal units defaults to meters. */
	MIR_UnitCode nRasterVerticalUnits;	/*!< Specifies the grid vertical units (Z values of the grid). Represented by MapInfo units. If no vertical unit is specified in the call to this method, then it defaults to the same as the horizontal unit of the grid. */
	double dViewshedOffset;					/*!< Represents the observer height above the terrain. */
	int32_t nEarthCurvatureModel;				/*!< Represents the earth curvature correction. If the curvature correction is zero, the vertical units (Z values of the grid) will be the same as horizontal units. The options are:
		- 0 - No earth correction.
		- 1 - Normal earth correction. In this case, the corrected earth's radius will be (6,378,137) meters * 1.
		- 2 - 4/3 earth correction. In this case, the corrected earth's radius will be (6,378,137) meters * (4/3).
		- 3 - 3 - 2/3 earth correction. In this case, the corrected earth's radius will be (6,378,137) meters * (2/3).
	*/
	bool bOutputClassifiedRaster;			/*!< Boolean flag to specify if the output viewshed is to be written as a classified raster. */
	bool bComplexCalculation;				/*!< Boolean flag to specify if distance waypoint needs to be raised to be just visisble is to be computed. */
	bool bNullCellsOutsideViewshed;			/*!< Boolean flag to specify if the cells that fall outside the viewshed radius are set to null values. If true, all cells outside the radius are null. */
	bool bClipToViewshedRadii;				/*!< Boolean flag to specify if output raster is clipped to the minimum bounding box which includes all viewshed radii. */
	bool bSmooth;							/*!< Boolean flag to specify if output raster is to be smoothed using the Gaussian filter. */
	int32_t nFilterSize;						/*!< Represents the dimensions of the filter kernel. Must be an odd number. This parameter will be used if \c bSmooth is true. */
	int32_t nClassification;					/*!< Represents how the output viewshed is classified.
		- 0 - No classification.
		- 1 - Two classification types - Visible and Invisible.
		- 2 - Three classification types - Visible, Fringe, and Invisible.
	*/
};

/*! \struct SMIR_ViewshedSingleTowerParameters
	\brief	Structure to define the parameters of the viewshed API when the viewshed is calculated from a single tower.
*/
struct SMIR_ViewshedSingleTowerParameters
{
	double dOriginX;		 /*!< Represents the X origin of the viewshed tower.*/
	double dOriginY;		 /*!< Represents the Y origin of the viewshed tower. */
	double dRadius;			 /*!< Maximum radial distance for viewshed from tower. */
	double dViewPointHeight; /*!< Height of the tower or object being observed from the viewshed tower. */
	bool   bUseSweep;		 /*!< Boolean flag to specify if sweep angle and sweep azimuth are to be calculated for the output viewshed. */
	double dSweepAzimuth;	 /*!< Sweep azimuth to be used if \c bUseSweep is true. */
	double dSweepAngle;		 /*!< Sweep angle to be used if \c bUseSweep is true. */
	bool   bLimitVerticalAngles; /*!< Boolean flag to specify if minimum vertical angle and maximum vertical angle are to be used for the viewshed. */
	double dMinimumVerticalAngle; /*!< The minimum vertical angle to be used if \c bLimitVerticalAngles is true */
	double dMaximumVerticalAngle; /*!< The maximum vertical angle to be used if \c bLimitVerticalAngles is true */
	bool bUseMinimumRadius; /*!< Boolean flag to specify if minimum radius is to be used. */
	double dMinimumRadius; /*!< The minimum radius. Points within this radius are not visible. */
	bool bUseRefractivity; /*!< Boolean flag to specify whether to use refractivity. */
	double dRefractivity; /*!< Refractivity coefficient. */
};

/*! \struct SMIR_ViewshedMultipleTowerParameters
	\brief	Structure for defining the parameters of the viewshed API when multiple viewsheds are calculated from one or more towers.
*/
struct SMIR_ViewshedMultipleTowerParameters
{
	const wchar_t* sTABFilePath; /*!< The file path of the TAB file that contains information about each tower, such as, height of every tower, parameter units, radius, and height of objects being viewed from each tower. */

	bool		bComputeVisibilityCount; /*!< If true, each output cell will contain the number of towers which are visible from this location. */

	bool		bUseSweep;

	// Only used if bUseSweep = true
	bool		bUseConstantSweepAzimuth;
	double		dConstantSweepAzimuth; /*!< The sweep azimuth to be used if bUseSweep and bUseConstantSweepAzimuth are true.*/
	int32_t		nSweepAzimuthTABFieldIndex; /*!< The zero-based TAB file field index to read the sweep azimuths from. Used if bUseSweep is true and bUseConstantSweepAzimuth is false.*/

	// Only used if bUseSweep = true
	bool		bUseConstantSweepAngle;
	double		dConstantSweepAngle; /*!< The sweep angle to be used if bUseSweep and bUseConstantSweepAngle are true.*/
	int32_t		nSweepAngleTABFieldIndex; /*!< The zero-based TAB file field index to read the sweep angles from. Used if bUseSweep is true and bUseConstantSweepAngle is false.*/

	bool bLimitVerticalAngles;

	bool   bUseConstantMinimumVerticalAngle;
	double dConstantMinimumVerticalAngle;
	int32_t nMinimumVerticalAngleTABFieldIndex;

	bool   bUseConstantMaximumVerticalAngle;
	double dConstantMaximumVerticalAngle;
	int32_t nMaximumVerticalAngleTABFieldIndex;

	bool bUseMinimumRadius; /*!< Boolean flag to specify if minimum radius is to be used. */
	bool bUseConstantMinimumRadius;
	double dConstantMinimumRadius; /*!< The minimum radius. Points within this radius are not visible. */
	int32_t	nMinimumRadiusTABFieldIndex; /*!< The zero-based TAB file field index for the minimum radius. */
	
	bool		bUseConstantRadius;
	double		dConstantRadius;	/*!< The radius to be used if bUseConstantRadius is true.*/
	int32_t		nRadiusTABFieldIndex; /*!< The zero-based TAB file field index to read the view point radii from. Used if bUseConstantRadius is false.*/

	bool		bUseConstantViewPointHeight;
	double		dConstantViewPointHeight; /*!< The viewpoint height to be used if bUseConstantViewPointHeight is true.*/
	int32_t		nViewPointHeightTABFieldIndex; /*!< The zero-based TAB file field index to read the view point heights from. Used if bUseConstantViewPointHeight is false.*/

	bool bUseRefractivity; /*!< Boolean flag to specify whether to use refractivity. */
	bool bUseConstantRefractivity; /*!< Boolean flag to specify whether to use constant refractivity. */
	double dConstantRefractivity; /*!< Refractivity coefficient. */
	int32_t nRefractivityTABFieldIndex; /*!< Refractivity coefficient TAB file field index. */
};


/*! \struct MIR_Rect
	\brief	A world coordinate rectangle.
*/
struct MIR_Rect
{
	double x1;	/*!< Minimum X coordinate. */
	double y1;	/*!< Minimum Y coordinate. */
	double x2;	/*!< Maximum X coordinate. */
	double y2;	/*!< Maximum Y coordinate. */
};

/*! \struct worldTileRect
	\brief	World coordinate rectangles.
*/
struct worldTileRect
{
	MIR_Rect srcRect;	/*!< Source rectangle. */
	MIR_Rect dstRect;	/*!< Destination rectangle. */
	uint64_t nTileY;	/*!< Tile Y coordinate. */
	uint64_t nTileX;	/*!< Tile X coordinate. */
};

/*****************************************************************************************************************************/
//	Filter

/*!	\enum	FilterType
	\brief Focal filter type applied on the input data.
*/
enum  FilterType
{
	MIR_FILTER_CONVOLUTION	= 0,
	MIR_FILTER_FOCAL_MIN, /*!< Calculates the minimum value in the specified neighborhood around every cell in an input raster.*/
	MIR_FILTER_FOCAL_MAX, /*!< Calculates the highest value in the specified neighborhood around every cell in an input raster.*/
	MIR_FILTER_FOCAL_STDDEV, /*!< Calculates the majority value in the specified neighborhood around every cell in an input raster.*/
	MIR_FILTER_FOCAL_MAJORITY, /*!< Calculates the standard deviation in the specified neighborhood around every cell in an input raster.*/
	MIR_FILTER_CLASSIFIED
};

/*! \struct SMIR_InflectionPoint
\brief	Structure for defining Inflection point of a raster, i.e. value, color pair.
*/
struct SMIR_InflectionPoint
{
	wchar_t value[256]; /*!< value of inflection point*/
	uint32_t color;		/*!< color of the inflection point. (AABBGGRR)*/
	uint64_t count;
};

/*****************************************************************************************************************************/

/*! \struct SMIR_RasterExpressionInputs
	\brief	Structure for defining a field from within a source raster. This is used by the Calculator API.
*/
struct SMIR_RasterExpressionInputs 
{ 
       const wchar_t * sSrcFilePath; 
       const wchar_t * sSrcAlias; 
       uint32_t nSrcField; 
};

/*****************************************************************************************************************************/

/*! \enum	MIR_WeightingModel
	\brief	Weighting models used for interpolating rasters using the inverse distance weighted method (IDW).
*/
enum MIR_WeightingModel
{
	MIR_IDW_Linear = 0,		/*!< Linear model.*/
	MIR_IDW_Exponential,	/*!< Exponential model.*/
	MIR_IDW_Power,			/*!< Power model.*/
	MIR_IDW_Gaussian,		/*!< Gaussian model.*/
	MIR_IDW_Quartic,		/*!< Quartic model.*/
	MIR_IDW_Triweight,		/*!< Triweight model.*/
	MIR_IDW_Tricube			/*!< Tricube model.*/
};

/*! \enum	MIR_DensityKernel
	\brief	Kernel model used for interpolating a raster using the density method.
*/
enum MIR_DensityKernel
{
	MIR_Kernel_DataCount = 0, /*!< Point Count Density Estimation.*/
	MIR_Kernel_Uniform, /*!< Kernel Density Estimation using a uniform model.*/
	MIR_Kernel_Triangle, /*!< Kernel Density Estimation using a Triangle model.*/
	MIR_Kernel_Epanechnikov, /*!< Kernel Density Estimation using a Epanechnikov model.*/
	MIR_Kernel_Quartic, /*!< Kernel Density Estimation using a Quartic model.*/
	MIR_Kernel_Triweight, /*!< Kernel Density Estimation using a Triweight model.*/
	MIR_Kernel_Gaussian, /*!< Kernel Density Estimation using a Gaussian model.*/
	MIR_Kernel_Cosinus, /*!< Kernel Density Estimation using a Cosinus model.*/
	MIR_Kernel_Tricube, /*!< Kernel Density Estimation using a Tricube model.*/
	MIR_Kernel_SharpenedGaussian /*!< Kernel Density Estimation using a sharpened Gaussian model.*/
};

/*! \enum	MIR_MinimumCurvature_StampMethod
	\brief	Method of stamping used when interpolating a raster using the minimum curvature method.
*/
enum MIR_MinimumCurvature_StampMethod
{
	MIR_MinimumCurvature_FirstOnly	= 0, /*!< First only stamping method.*/
	MIR_MinimumCurvature_LastOnly,	/*!< Last only stamping method.*/
	MIR_MinimumCurvature_AverageLastInWeighted, /*!< Average all (last in weighted) stamping method.*/
	MIR_MinimumCurvature_Average, /*!< Average all stamping method.*/
	MIR_MinimumCurvature_AverageIDWWeighted /*!< Average all (inverse distance weighted) stamping method.*/	
};

/*! \enum	MIR_Stamp_StampMethod
	\brief	Method of stamping used when interpolating a raster using the stamp method.
*/
enum MIR_Stamp_StampMethod
{
	MIR_Stamp_FirstOnly	= 0,			/*!< Stamp first sample only method.*/
	MIR_Stamp_LastOnly,					/*!< Stamp last sample only method.*/
	MIR_Stamp_Sum,						/*!< Stamp sum of samples method.*/
	MIR_Stamp_Minimum,					/*!< Stamp minimum sample value method.*/
	MIR_Stamp_Maximum,					/*!< Stamp maximum sample value method.*/
	MIR_Stamp_AverageLastInWeighted,	/*!< Stamp average sample value method (weighted to last).*/
	MIR_Stamp_Average,					/*!< Stamp average sample value method.*/
	MIR_Stamp_Count						/*!< Stamp count of samples method.*/
};

/*! \enum	MIR_CoordinateConditioningMethod
	\brief	Method of coordinate conditioning when interpolating a raster.
*/enum MIR_CoordinateConditioningMethod
{
	MIR_CoordinateConditioning_None = 0, /*!< No coordinate conditioning.*/
	MIR_CoordinateConditioning_Rectangle, /*!< Use a rectangle to define coordinate conditioning.*/
	MIR_CoordinateConditioning_Polygon /*!< Use polygons to define coordinate conditioning.*/
};

/*! \enum	MIR_ClipMethod
\brief	Method of clipping when interpolating a raster.
*/enum MIR_ClipMethod
{
	MIR_Clip_None = 0, /*!< No clipping.*/
	MIR_Clip_Near, /*!< Use the near zone clipping method.*/
	MIR_Clip_NearFar, /*!< Use the near and far zone clipping method.*/
	MIR_Clip_Polygon /*!< Use polygons to define clipping.*/
};

/*! \enum	MIR_IterationIntensity
	\brief	Intensity of iterations when interpolating a raster using the minimum curvature method.
*/enum MIR_IterationIntensity
{
	MIR_Iteration_Minimum = 0, /*!< Minimum iteration intensity used.*/
	MIR_Iteration_VeryLow, /*!< Very low iteration intensity used.*/
	MIR_Iteration_Low, /*!< Low iteration intensity used.*/
	MIR_Iteration_Normal, /*!< Normal iteration intensity used.*/
	MIR_Iteration_High, /*!< High iteration intensity used.*/
	MIR_Iteration_VeryHigh, /*!< Very high iteration intensity used.*/
	MIR_Iteration_Maximum /*!< Maximum iteration intensity used.*/
};

/*! \enum	MIR_InputFileType
	\brief	Intensity of iterations when interpolating a raster using the minimum curvature method.
*/enum MIR_InputFileType
{
	MIR_IFT_ASCII_TAB = 0, /*!< File type is ASCII, tab delimited.*/
	MIR_IFT_ASCII_SPACE, /*!< File type is ASCII, space delimited.*/
	MIR_IFT_ASCII_CSV, /*!< File type is CSV, comma delimited.*/
	MIR_IFT_LAS, /*!< File type is LAS or LASZip (LAZ).*/
	MIR_IFT_MAPINFO_TAB, /*!< File type is Mapinfo TAB format.*/
	MIR_IFT_ASCII_USER_DELIMITER /*!< File type is ASCII, delimited by a user defined character.*/
};

/*! \enum	MIR_BoundsResolution
	\brief	Resolution at which to parse input source points when the raster interpolation is determining the input bounds.
*/enum MIR_BoundsResolution
{
	MIR_BR_FULL = 0, /*!< Use every input.*/
	MIR_BR_HIGH, /*!< Use a high amount of input points.*/
	MIR_BR_MEDIUM, /*!< Use a medium amount of input points.*/
	MIR_BR_LOW /*!< Use a low amount of input points.*/
};

/*! \enum	MIR_ExtentType
\brief	Determines which parameters to use in the SMIR_IMP_Geometry structure when defining the geometry extents.
*/enum MIR_ExtentType
{
	MIR_ET_CELLS = 0, /*!< Use nRows and nColumns cell counts to define the extents.*/
	MIR_ET_EXTENTS /*!< Use dExtentX and dExtentY to define the extents.*/
};

/*! \enum	MIR_InterpolationType
	\brief	Type of interpolation to use when computing default values. Different interpolation methods pad differently and can produce different bounds.
*/enum MIR_InterpolationType
{
	MIR_IT_TRIANGULATION = 0,	/*!< Triangulation interpolation method.*/
	MIR_IT_IDW,					/*!< Inverse Distance Weighted interpolation method.*/
	MIR_IT_DENSITY,				/*!< Density interpolation method.*/
	MIR_IT_DISTANCE,			/*!< Distance interpolation method.*/
	MIR_IT_MINIMUM_CURVATURE,	/*!< Minimum Curvature interpolation method.*/
	MIR_IT_STAMP,				/*!< Stamp interpolation method.*/
	MIR_IT_NEARESTNEIGHBOUR,	/*!< Nearest Neighbour interpolation method.*/
	MIR_IT_NATURALNEIGHBOUR,	/*!< Natural Neighbour interpolation method.*/
	MIT_IT_TREECOVERAGE,		/*!< Tree Canopy Coverage analysis method.*/
	MIT_IT_TREEDENSITY,			/*!< Tree Canopy Density analysis method.*/
	MIT_IT_TREEHEIGHT			/*!< Tree Canopy Height analysis method.*/
};

/*! \enum	MIR_ParameterUnitsType
\brief	Type of parameter units used in interpolation.
*/enum MIR_ParameterUnitsType
{
	MIR_PUT_CELL = 0, /*!< Parameters are stored in cell count units.*/
	MIR_PUT_DISTANCE /*!< Parameters are stored in distance units.*/
};

/*! \enum	MIR_DateTimeGrouping
\brief	Grouping types for date/time columns.
*/enum MIR_DateTimeGrouping
{
	MIR_UNIQUE_VALUE = 0,		/*!< Group by unique date/time values.*/
	MIR_YEAR,					/*!< Group by year value.*/
	MIR_MONTH_OF_YEAR,			/*!< Group by month value, valid values are 1 to 12.*/
	MIR_DAY_OF_YEAR,			/*!< Group by day of the year value, valid values are 1 to 366.*/
	MIR_DAY_OF_MONTH,			/*!< Group by day of the month value, valid values are 1 to 31.*/
	MIR_DAY_OF_WEEK,			/*!< Group by day of the week value, valid values are 1 to 7 which represent Monday (1) to Sunday (7).*/
	MIR_HOUR_OF_DAY,			/*!< Group by hour of the day value, valid values are 0 to 23.*/
	MIR_MINUTE_OF_HOUR,			/*!< Group by minute of the hour value, valid values are 0 to 59.*/
	MIR_SECOND_OF_MINUTE,		/*!< Group by second of the minute value, valid values are 0 to 59.*/
	MIR_MONTH_AND_YEAR			/*!< Group by month and year values, valid values are of the format MM-YYYY.*/
};

/*! \struct SMIR_IMP_Grouping
\brief	Structure for defining parameters associated with grouping when interpolating a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Grouping
{
	bool					bApplyGrouping; /*!< Flag defining if grouping is turned on. */
	uint32_t				nColumnIndex; /*!< Grouping column index. */
	MIR_DataType			nColumnDataType; /*!< Data type of the grouping column, used for range comparisons. */
	wchar_t					sDateTimeFormat[64]; /*!< Specifies the format of date/time values specified in vValue, vRangeMin and vRangeMax. For TAB files specify YYYYMMDD for date field, hhmmss for Time field and YYYYMMDDhhmmss for a DateTime field. For ascii use the same character qualifiers but specify as data exists such as DD/MM/YYYY for a date field. */
	MIR_DateTimeGrouping	nDateTimeGrouping; /*!< Specifies the type of grouping required for a date/time column.*/
	SMIR_VariantArray		vValue;	/*!< List of unique grouping values. */
	SMIR_VariantArray		vRangeMin; /*!< List of minimum grouping range values, must have corresponding maximum range value in vRangeMax. */
	SMIR_VariantArray		vRangeMax; /*!< List of maximum grouping range values, must have corresponding minimum range value in vRangeMin. */
};

/*! \struct SMIR_IMP_InputFile
	\brief	Structure for defining parameters per input file used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_InputFile 
{ 
	MIR_InputFileType	nType; /*!< Used to define type of input file. */
	wchar_t				sFile[1024]; /*!< Path to the input file. */
	uint32_t			nHeaderRows; /*!< Number of rows to ignore as they may contain header information. */
	wchar_t				sCoordinateSystem[512]; /*!< MapInfo coordinate system string for the input file. */
	wchar_t				sSubFile[1024]; /*!< Internal sub file name if sFile refers to a zip archive. */
	int32_t				nXFieldIndex; /*!< X coordinate field index. If set to -1 the coordinate will come from the geometry object in TAB input. */
	int32_t				nYFieldIndex; /*!< Y coordinate field index. If set to -1 the coordinate will come from the geometry object in TAB input. */
	uint32_t			nDataFieldIndexes; /*!< Count of data fields to interpolate that are defined in pDataFieldIndexes. */
	uint32_t*			pDataFieldIndexes; /*!< List of data fields to interpolate, 0 based. */
	uint8_t				cDelimiter; /*!< User defined delimiter if nType==MIR_IFT_USER_DELIM. */
	SMIR_IMP_Grouping	cGrouping; /*!< User defined grouping values and ranges. */
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Preferences
	\brief	Structure for defining preference parameters used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Preferences 
{
	bool		bAutoCacheSize;	/*!< If true the system will determine the cache size to use. If false nTotalCache is used. */
	uint64_t	nTotalCache; /*!< Size of the cache if bAutoCacheSize is false. */
	wchar_t		sTempDir[1024];	/*!< Folder to use to store temporary files, if blank the system temp directory is used. */			
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_DataConditioning_Values
	\brief	Structure for defining data conditioning values used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_DataConditioning_Values
{
	int32_t	nConditionValues; /*!< Count of values to convert defined in pConditionValues. */
	double*	pConditionValues; /*!< List of values to convert. */
};

/*! \struct SMIR_IMP_DataConditioning_Ranges
	\brief	Structure for defining data conditioning ranges used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_DataConditioning_Ranges
{
	int32_t	nConditionRangeValues; /*!< Count of values defined in pConvertRangeValuesMin and pConvertRangeValuesMax. */
	double*	pConditionRangeValuesMin; /*!< List of minimum range values. */
	double*	pConditionRangeValuesMax; /*!< List of maximum range values. */
};

/*! \struct SMIR_IMP_DataConditioning_Background
	\brief	Structure for defining data conditioning background value used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_DataConditioning_Background
{
	bool	bConvert2Bkgd; /*!< Conversion of invalid value to a background value. */
	double	dConvert2Bkgd; /*!< Background value. */
};

/*! \struct SMIR_IMP_DataConditioning_CapMinimum
	\brief	Structure for defining data conditioning minimum capping used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_DataConditioning_CapMinimum
{
	bool	bCapMinimum; /*!< Converts values below the minimum cap value to the cap value. */
	double	dCapMinimum; /*!< Minimum cap value. */
};

/*! \struct SMIR_IMP_DataConditioning_CapMaximum
	\brief	Structure for defining data conditioning maximum capping used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_DataConditioning_CapMaximum
{
	bool	bCapMaximum; /*!< Converts values above the maximum cap value to the cap value. */
	double	dCapMaximum; /*!< Maximum cap value. */
};
/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_DataConditioning
	\brief	Structure for defining data conditioning parameters used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_DataConditioning 
{
	uint32_t								nBandIndex; /*!< Band index to apply conditioning to. */
	SMIR_IMP_DataConditioning_Values		cValues; /*!< Defines values to condition. */
	SMIR_IMP_DataConditioning_Ranges		cRanges; /*!< Defines value ranges to condition. */
	SMIR_IMP_DataConditioning_Background	cBackground; /*!< Defines background value. */
	SMIR_IMP_DataConditioning_CapMinimum	cCapMinimum; /*!< Defines minimum value capping. */
	SMIR_IMP_DataConditioning_CapMaximum	cCapMaximum; /*!< Defines maximum value capping. */
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_CoordinateConditioning
	\brief	Structure for defining coordinate conditioning parameters used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_CoordinateConditioning 
{
	MIR_CoordinateConditioningMethod	nMethod; /*!< Flag indicating if coordinate conditioning should be applied. */
	double								dMinimumX; /*!< Minimum valid X coordinate value, when nMethod = MIR_CoordinateConditioning_Rectangle. */
	double								dMaximumX; /*!< Maximum valid X coordinate value, when nMethod = MIR_CoordinateConditioning_Rectangle. */
	double								dMinimumY; /*!< Minimum valid Y coordinate value, when nMethod = MIR_CoordinateConditioning_Rectangle. */
	double								dMaximumY; /*!< Maximum valid Y coordinate value, when nMethod = MIR_CoordinateConditioning_Rectangle. */
	bool								bKeepWithinPolygon; /*!< When nMethod = MIR_CoordinateConditioning_Polygon, if true all input point data within the given polygons will be kept otherwise if false all data outside the given polygons will be kept.*/
	wchar_t								sPolygonTabFile[1024]; /*!< Path to the a TAB file that provides polygons to clip the input point data to when nMethod = MIR_CoordinateConditioning_Polygon*/
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Clip
\brief	Structure for defining clipping parameters to be applied to the raster geometry when interpolating a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Clip
{
	MIR_ClipMethod	nMethod; /*!< Clipping method for grid extents to use.*/
	double			dNear; /*!< Near clip distance, specified in nParameterUnitsType.*/
	double			dFar; /*!< Far clip distance, specified in nParameterUnitsType.*/
	bool			bKeepWithinPolygon; /*!< When ClipMethod = MIR_Clip_Polygon, if true all data within the given polygons will be kept otherwise if false all data outside the given polygons will be kept.*/
	wchar_t			sPolygonTabFile[1024]; /*!< Path to the a TAB file that provides polygons to clip the data to when ClipMethod = MIR_Clip_Polygon*/
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_CoincidentPoint
\brief	Structure for defining coincident point parameters to be applied to reading input points when interpolating a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_CoincidentPoint
{
	MIR_CoincidentPointMethod	nMethod; /*!< Method of coincident point analysis to use.*/
	bool						bAutoRange; /*!< If true the system will calculate the coincident point range to use, otherwise the dRange will be used.*/
	double						dRange; /*!< Coincident point range.*/
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Smoothing
\brief	Structure for defining smoothing parameters to be applied to the grid post processing when interpolating a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Smoothing
{
	MIR_SmoothingType			nType; /*!< Method of coincident point analysis to use.*/
	uint32_t					nLevel; /*!< Smoothing level to be applied, valid values are 0 (no smoothing) to 6 (high smoothing).*/
};

/*****************************************************************************************************************************/

/*! \enum	MIR_LASFilter_Rule
	\brief	LAS return filtering rule.
*/
enum MIR_LASFilter_Rule
{
	MIR_LASFILTER_RULE_NONE = 0,				/*!< Do not apply filter. */
	MIR_LASFILTER_RULE_ALL,						/*!< Select all returns. */
	MIR_LASFILTER_RULE_BYRETURN,				/*!< Select by return number or placement. */
	MIR_LASFILTER_RULE_BYINTENSITY,				/*!< Select by intensity placement. */
	MIR_LASFILTER_RULE_BYCLASSANDRETURN,		/*!< Select by matching classification and then by return number of placement. */
	MIR_LASFILTER_RULE_BYNOTCLASSANDRETURN,		/*!< Select by not matching classification and then by return number of placement. */
	MIR_LASFILTER_RULE_BYCLASSANDINTENSITY,		/*!< Select by matching classification and then by intensity placement. */
	MIR_LASFILTER_RULE_BYNOTCLASSANDINTENSITY	/*!< Select by not matching classification and then by intensity placement. */
};

/*! \enum	MIR_LASFilter_ReturnRule
	\brief	LAS return filtering rule, specific to return number selection.
*/
enum MIR_LASFilter_ReturnRule
{
	MIR_LASFILTER_RETURNRULE_ALL = 0,			/*!< All returns are considered. */
	MIR_LASFILTER_RETURNRULE_NUMBERED,			/*!< Matches one of the supplied return numbers. */
	MIR_LASFILTER_RETURNRULE_NOTNUMBERED,		/*!< Does not match one of the supplied numbers. */
	MIR_LASFILTER_RETURNRULE_FIRST,				/*!< First return. */
	MIR_LASFILTER_RETURNRULE_LAST,				/*!< Last return. */
	MIR_LASFILTER_RETURNRULE_FIRSTN,			/*!< First N returns (or less). */
	MIR_LASFILTER_RETURNRULE_LASTN,				/*!< Last N returns (or less). */
	MIR_LASFILTER_RETURNRULE_FIRSTNOFP,			/*!< First N returns of P returns in the set N<=P. */
	MIR_LASFILTER_RETURNRULE_LASTNOFP,			/*!< Last N returns of P returns in the set N<=P. */
};

/*! \enum	MIR_LASFilter_IntensityRule
	\brief	LAS return filtering rule, specific to intensity filtering.
*/
enum MIR_LASFilter_IntensityRule
{
	MIR_LASFILTER_INTENSITYRULE_LOWEST = 0,		/*!< Select return with lowest intensity. */
	MIR_LASFILTER_INTENSITYRULE_HIGHEST			/*!< Select return with highest intensity. */
};

/*! \enum	MIR_LASFILTER_Classification
	\brief	Standard classification codes for LIDAR returns.
*/
enum MIR_LASFILTER_Classification
{
	MIR_LASFILTER_CLASSIFICATION_NEVERCLASSIFIED	= 0,
	MIR_LASFILTER_CLASSIFICATION_UNASSIGNED			= 1,
	MIR_LASFILTER_CLASSIFICATION_GROUND				= 2,
	MIR_LASFILTER_CLASSIFICATION_LOWVEGETATION		= 3,
	MIR_LASFILTER_CLASSIFICATION_MEDIUMVEGETATION	= 4,
	MIR_LASFILTER_CLASSIFICATION_HIGHVEGETATION		= 5,
	MIR_LASFILTER_CLASSIFICATION_BUILDING			= 6,
	MIR_LASFILTER_CLASSIFICATION_NOISE				= 7,
	MIR_LASFILTER_CLASSIFICATION_MODELKEY			= 8,
	MIR_LASFILTER_CLASSIFICATION_WATER				= 9,
	MIR_LASFILTER_CLASSIFICATION_RAIL				= 10,
	MIR_LASFILTER_CLASSIFICATION_ROADSURFACE		= 11,
	MIR_LASFILTER_CLASSIFICATION_OVERLAP			= 12,
	MIR_LASFILTER_CLASSIFICATION_WIREGUARD			= 13,
	MIR_LASFILTER_CLASSIFICATION_WIRECONDUCTOR		= 14,
	MIR_LASFILTER_CLASSIFICATION_TRANSMISSIONTOWER	= 15,
	MIR_LASFILTER_CLASSIFICATION_WIRECONNECTOR		= 16,
	MIR_LASFILTER_CLASSIFICATION_BRIDGEDECK			= 17,
	MIR_LASFILTER_CLASSIFICATION_HIGHNOISE			= 18
};

/*! \struct SMIR_IMP_LASFilter_Bandpass
	\brief	Structure for declaring bandpass filters for specific LAS fields.
*/
struct SMIR_IMP_LASFilter_Bandpass
{
	bool						bApply;			/*!< Apply bandpass filter. */
	double						dMinimum;		/*!< Minimum allowable value. */
	double						dMaximum;		/*!< Maximum allowable value. */
};

/*! \struct SMIR_IMP_LASFilter
	\brief	Structure for defining LAS return filtering parameters. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_LASFilter
{
	MIR_LASFilter_Rule			eRule;					/*!< LAS filtering rule. */
	MIR_LASFilter_ReturnRule	eReturnRule;			/*!< Rule for selection by return number or placement. */
	MIR_LASFilter_IntensityRule	eIntensityRule;			/*!< Rule for selection by intensity placement. */
	uint8_t						vnClassNumbers[32];		/*!< Matching classification numbers. */
	uint8_t						nClassNumberCount;		/*!< Number of supplied classification numbers. */
	uint8_t						vnReturnNumbers[16];	/*!< Matching return numbers. */
	uint8_t						nReturnNumberCount;		/*!< Number of supplied return numbers. */
	SMIR_IMP_LASFilter_Bandpass	cZBandpass;				/*!< Elevation bandpass filter. */
	SMIR_IMP_LASFilter_Bandpass	cIntensityBandpass;		/*!< Intensity bandpass filter. */
	SMIR_IMP_LASFilter_Bandpass	cScanAngleBandpass;		/*!< Scan angle bandpass filter. */
	uint8_t						nReturnCount;			/*!< Number of required returns. */
	uint8_t						nReturnSetCount;		/*!< Number of required returns in the return set. */
	bool						bSynthetic;				/*!< Retain or ignore returns marked Synthetic. */
	bool						bKeyPoint;				/*!< Retain or ignore returns marked KeyPoint (LAS 1.4+). */
	bool						bWithheld;				/*!< Retain or ignore returns marked Withheld. */
	bool						bOverlap;				/*!< Retain or ignore returns marked Overlap (LAS 1.4+). */
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Input
	\brief	Structure for defining input parameters used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Input 
{
	int32_t							nFileInputs;				/*!< Count of file inputs defined in pFileInputs. */
	SMIR_IMP_InputFile*				pFileInputs;				/*!< List of file inputs defined by SMIR_IMP_InputFile. */
	int32_t							nDataConditioning;			/*!< Count of data conditioning parameters defined in pDataConditioning. */
	SMIR_IMP_DataConditioning*		pDataConditioning;			/*!< List of data conditioning parameters, create one per input band. */
	SMIR_IMP_CoordinateConditioning cCoordinateConditioning;	/*!< Coordinate conditioning parameters. */
	MIR_BoundsResolution			eBoundsResolution;			/*!< Bounds resolution input point granularity. */
	SMIR_IMP_CoincidentPoint		cCoincidentPoint;			/*!< Coincident point parameters. */
	SMIR_IMP_LASFilter				cLASFilter;					/*!< LAS return filtering parameters. */
	bool							bInterpolateFeatures;		/*!< Interpolate points along the boundary of TAB file features such as polylines and regions. > */
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Output
	\brief	Structure for defining output parameters used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Output 
{
	wchar_t			sFile[1024]; /*!< Path to the output file. */
	wchar_t			sDriverId[64]; /*!< DriverId used to create the output raster.*/
	bool			bAutoDataType; /*!< If true the system will choose the best output data type, otherwise define them per band in pDataTypes. */
	int32_t			nDataTypes; /*!< Count of data types defined in pDataTypes. */
	MIR_DataType*	pDataTypes; /*!< Output grid data types. Valid MIR_DataType types are UnsignedInt8, SignedInt8, UnsignedInt16, SignedInt16, UnsignedInt32, SignedInt32, RealSingle, RealDouble. */
	wchar_t			sCoordinateSystem[512]; /*!< MapInfo coordinate system string for the output file. */
	bool			bAllowMultiBand; /*!< If true and output format can handle multi-banded data then a multi-banded raster will be created. */
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Geometry
	\brief	Structure for defining geometry parameters used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Geometry 
{
	bool					bAutoGridExtents; /*!< If false supply grid origin, columns and rows, otherwise if true the system will define.*/
	bool					bAutoGridCellSize; /*!< If false supply cell size, otherwise if true the system will define.*/
	double					dOriginX; /*!< X origin of the output grid.*/
	double					dOriginY; /*!< Y origin of the output grid.*/
	MIR_ExtentType			nExtentType; /*!< Determines if dExtentX and dExtentY are in coordinate values or cell values.*/
	double					dExtentX; /*!< Maximum X coordinate or number of columns of the output grid.*/
	double					dExtentY; /*!< Maximum Y coordinate or number of rows of the output grid.*/
	double					dCellSizeX; /*!< X Cell size of the output grid.*/
	double					dCellSizeY; /*!< Y Cell size of the output grid.*/
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Parameters
	\brief	Structure for defining general parameters used to interpolate a raster. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Parameters 
{
	SMIR_IMP_Preferences	cPreferences; /*!< Structure defining interpolation preference parameters.*/
	SMIR_IMP_Input			cInput; /*!< Structure defining interpolation input parameters.*/
	SMIR_IMP_Output			cOutput; /*!< Structure defining interpolation output parameters.*/
	SMIR_IMP_Geometry		cGeometry; /*!< Structure defining interpolation geometry parameters.*/
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Triangulation
	\brief	Structure for defining parameters used to interpolate a raster using the triangulation method. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Triangulation 
{ 
	MIR_ParameterUnitsType	nParameterUnitsType;	/*!< Type of units that the specified parameters are stored in.*/
	MIR_UnitCode			nParameterUnitCode;		/*!< Units that all parameters are specified in when nParameterUnitsType is set to Distance.*/
	uint8_t					nPatchMultiplier;		/*!< Side length of the triangulation patch, expressed as a number of raster tiles (ranges from 1 to 5).*/
	double					dLongTriangle;			/*!< Maximum side length of a triangle, specified in nParameterUnitsType.*/
	SMIR_IMP_Clip			cClip;					/*!< Structure defining clipping parameters.*/
	SMIR_IMP_Smoothing		cSmoothing;				/*!< Smoothing parameters to be applied post processing.*/
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_IDW_Sector
\brief	Structure for defining sector parameters used to interpolate a raster using the inverse distance weighted method. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_IDW_Sector
{
	bool					bApply;				/*!< Apply sector support.*/
	int32_t					nCount;				/*!< Count of sectors to use, valid values are 1 to 32.*/
	double					dOrientation;		/*!< Starting orientation of sectors in degrees, valid values are 0 to 360.*/
	int32_t					nMinimumPoints;		/*!< Minimum number of points per sector to validate sector, valid values are 1 and greater.*/
	int32_t					nMaximumPoints;		/*!< Maximum number of points to use per sector when bNearestPoints is true, if bNearestPoints is false this parameter is not used. Valid values are 1 and greater.*/
	bool					bNearestPoints;		/*!< Use nearest nMaximumPoints number of points.*/
	int32_t					nMinimumCount;		/*!< Minimum number of valid sectors required, valid values are 1 and greater.*/
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_IDW
	\brief	Structure for defining parameters used to interpolate a raster using the inverse distance weighted method. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_IDW 
{ 
	MIR_ParameterUnitsType	nParameterUnitsType; /*!< Type of units that the specified parameters are stored in.*/
	MIR_UnitCode			nParameterUnitCode; /*!< Units that all parameters are specified in when nParameterUnitsType is set to Distance.*/

	double					dRadiusX; /*!< X Radius of influence, specified in nParameterUnitsType.*/
	double					dRadiusY; /*!< Y Radius of influence, specified in nParameterUnitsType.*/
	bool					bElliptical; /*!< Radius of influence elliptical.*/
	double					dOrientation; /*!< Ellipse orientation.*/
	uint32_t				nIncrement; /*!< Search increment factor, valid values are 1 or greater.*/

	//	Weighting parameters
	MIR_WeightingModel		nModel;		/*!< Weighting model.*/
	double					dPower;		/*!< Distance weighting.*/
	double					dNugget;	/*!< Minimum distance, specified in nParameterUnitsType.*/
	double					dRange;		/*!< Maximum distance, specified in nParameterUnitsType.*/
	double					dScale;		/*!< Scaling distance, specified in nParameterUnitsType.*/

	//	Tapering of input data
	bool					bTaper; /*!< If true a distance tapering function is applied to the interpolated values.*/
	double					dTaperFrom; /*!< Minimum taper distance, specified in nParameterUnitsType.*/
	double					dTaperTo; /*!< Maximum taper distance, specified in nParameterUnitsType.*/
	double					dTaperBackground; /*!< Defined background value.*/

	SMIR_IMP_IDW_Sector		cSectors; /*!< Structure defining IDW sector parameters.*/

	SMIR_IMP_Clip			cClip; /*!< Structure defining clipping parameters.*/

	SMIR_IMP_Smoothing		cSmoothing; /*!< Smoothing parameters to be applied post processing.*/
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Distance
	\brief	Structure for defining parameters used to interpolate a raster using the data distance method. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Distance 
{ 
	MIR_ParameterUnitsType	nParameterUnitsType; /*!< Type of units that the specified parameters are stored in.*/
	MIR_UnitCode			nParameterUnitCode; /*!< Units that all parameters are specified in when nParameterUnitsType is set to Distance.*/
	double					dRadiusX; /*!< X Radius of influence, specified in nParameterUnitsType.*/
	double					dRadiusY; /*!< Y Radius of influence, specified in nParameterUnitsType.*/
	bool					bElliptical; /*!< Radius of influence elliptical.*/
	double					dOrientation; /*!< Ellipse orientation.*/
	SMIR_IMP_Clip			cClip; /*!< Structure defining clipping parameters.*/
	SMIR_IMP_Smoothing		cSmoothing; /*!< Smoothing parameters to be applied post processing.*/
};

/*****************************************************************************************************************************/

/*! \struct SMIR_IMP_Density
	\brief	Structure for defining parameters used to interpolate a raster using the data density method. 
	This is used by the Raster Interpolation API.

	Kernel Model

	Point Count Density Estimation		Model = 0

	Accumulates the number of points within the bandwidth. Optionally, bias
	this count by the input data value. Optionally return either the count
	or the true spatial density by dividing by the elliptical area (PI.A.B).

	Kernel Density Estimation			Model = 1 - 7

	This method is well known as a robust statistical technique to compute the
	point density of any collection of points at a point in space. Optionally
	return either the estimated value or the normalized density by dividing by
	the elliptical area (PI.A.B).

	D = (1/(n*h)) * sum (K * U); or is it (1/n) * sum (K(U))?
	n = number of input samples
	h = bandwidth (a distance)
	K = kernel function
	U = (Xi - X)/h

	So we need to find the distance of every point to the density location.
	Generally, we only consider points that are within h distance.
	h can be defined as (hx,hy,hz) for anisotropic density computations.

	K is defined in the following way.

	1	Uniform			1/2
	2	Triangle		1-|U|
	3	Epanechnikov	3/4 * (1-U^2)
	4	Quartic			15/16 * (1-U^2)^2
	5	Triweight		35/32 * (1-U^2)^3
	6	Gaussian		(1/sqrt(2PI)) * exp(-1/2 * U^2)
	7	Cosinus			PI/4 * cos(U*PI/2)

	Generally, the kernel is only evaluated for |U| <= 1.
	For gaussian, h ought to be infinity as it is perfectly smooth.
	Note that U and |U| are interchangeable in all cases except Triangle.
*/
struct SMIR_IMP_Density 
{ 
	MIR_ParameterUnitsType	nParameterUnitsType; /*!< Type of units that the specified parameters are stored in.*/
	MIR_UnitCode			nParameterUnitCode; /*!< Units that all parameters are specified in when nParameterUnitsType is set to Distance.*/
	double					dRadiusX; /*!< X Radius of influence, specified in nParameterUnitsType.*/
	double					dRadiusY; /*!< Y Radius of influence, specified in nParameterUnitsType.*/
	bool					bElliptical; /*!< Radius of influence elliptical.*/
	double					dOrientation; /*!< Ellipse orientation.*/
	MIR_DensityKernel		nKernel; /*!< Kernel model used to interpolate value.*/
	bool					bBiasByInput; /*!< If true interpret point value as a count.*/
	bool					bNormalise; /*!< Normalize data frequency values into data density values.*/
	SMIR_IMP_Clip			cClip; /*!< Structure defining clipping parameters.*/
	SMIR_IMP_Smoothing		cSmoothing; /*!< Smoothing parameters to be applied post processing.*/
	double					dGaussianSharpening; /*!< Sharpening to Gaussian kernel.*/
};

/*! \struct SMIR_IMP_Stamp
	\brief	Structure for defining parameters used to interpolate a raster using the stamp method. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Stamp 
{ 
	MIR_Stamp_StampMethod	nStampMethod; /*!< Stamping method to use. */
	SMIR_IMP_Clip			cClip; /*!< Structure defining clipping parameters.*/
	SMIR_IMP_Smoothing		cSmoothing; /*!< Smoothing parameters to be applied post processing.*/
};

/*! \struct SMIR_IMP_MinimumCurvature
	\brief	Structure for defining parameters used to interpolate a raster using the minimum curvature method. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_MinimumCurvature 
{ 
	MIR_ParameterUnitsType				nParameterUnitsType; /*!< Type of units that the specified parameters are stored in.*/
	MIR_UnitCode						nParameterUnitCode; /*!< Units that all parameters are specified in when nParameterUnitsType is set to Distance.*/
	MIR_IterationIntensity				nIterations; /*!< Iteration intensity.*/
	double								dPercentChange; /*!< Degree of bending constraint.*/
	double								dTension;	/*!< Spline tension.*/
	MIR_MinimumCurvature_StampMethod	nStampMethod; /*!< Stamping method to use.*/
	double								dIDWRadius;  /*!< Search radius around a grid cell, specified in nParameterUnitsType.*/
	double								dIDWRange;  /*!< Search range around a grid cell, specified in nParameterUnitsType.*/
	SMIR_IMP_Clip						cClip; /*!< Structure defining clipping parameters.*/
	SMIR_IMP_Smoothing					cSmoothing; /*!< Smoothing parameters to be applied post processing.*/
};

/*! \struct SMIR_IMP_NearestNeighbour
\brief	Structure for defining parameters used to interpolate a raster using the nearest neaighbour method. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_NearestNeighbour
{
	MIR_ParameterUnitsType	nParameterUnitsType; /*!< Type of units that the specified parameters are stored in.*/
	MIR_UnitCode			nParameterUnitCode; /*!< Units that all parameters are specified in when nParameterUnitsType is set to Distance.*/
	double					dMaxSearchDistance; /*!< Maximum search distance (must be greater than 0), specified in nParameterUnitsType.*/
	SMIR_IMP_Clip			cClip; /*!< Structure defining clipping parameters.*/
	SMIR_IMP_Smoothing		cSmoothing; /*!< Smoothing parameters to be applied post processing.*/
};

/*! \struct SMIR_IMP_NaturalNeighbourIntegration
\brief	Structure for defining parameters used to interpolate a raster using the natural neighbour integration method. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_NaturalNeighbourIntegration
{
	MIR_ParameterUnitsType	nParameterUnitsType; /*!< Type of units that the specified parameters are stored in.*/
	MIR_UnitCode			nParameterUnitCode; /*!< Units that all parameters are specified in when nParameterUnitsType is set to Distance.*/
	double					dMaxSearchDistance; /*!< Maximum search distance (must be greater than 0), specified in nParameterUnitsType.*/
	bool					bGaussianDistanceWeighted; /*!< Use Gaussian weighting by distance.*/
	bool					bAutoGaussianRange; /*!< If true automatically determines the Gaussian range, if false uses dGaussianRange.*/
	double					dGaussianRange; /*!< Manually defined Gaussian range (must be greater than 0), used when bAutoGaussianRange is false, specified in nParameterUnitsType.*/
	SMIR_IMP_Clip			cClip; /*!< Structure defining clipping parameters.*/
	SMIR_IMP_Smoothing		cSmoothing; /*!< Smoothing parameters to be applied post processing.*/
};


/*! \struct SMIR_IMP_LiDAR_TREECANOPY
\brief	Common LAS filtering parameters for tree canopy analysis operations.
*/
struct SMIR_IMP_LiDAR_TREECANOPY
{
	bool						bExtendedClassification;/*!< Use extended Classification data (LAS 1.4 onwards) instead of standard Classification data.*/
	bool						bEmptyIsZero;			/*!< Populate raster cells with no assigned value with zero.*/
	uint32_t					nSuppliedClassRule;		/*!< Use Ground classes only (0), Vegetation classes only (1) or Ground and Vegetation classes (2).*/
	uint32_t					nGroundClassCount;		/*!< Number of ground classification values supplied.*/
	uint8_t*					pvnGroundClass;			/*!< Array of ground classification values.*/
	uint32_t					nVegetationClassCount;	/*!< Number of vegetation classification values supplied.*/
	uint8_t*					pvnVegetationClass;		/*!< Array of vegetation classification values.*/
	SMIR_IMP_Clip				cCellClip;				/*!< Raster cell clipping parameters.*/
};

/*! \struct SMIR_IMP_LiDAR_TREECANOPY_COVERAGE
\brief	LiDAR analysis Tree Canopy Coverage parameters.
*/
struct SMIR_IMP_LiDAR_TREECANOPY_COVERAGE
{
	SMIR_IMP_LiDAR_TREECANOPY	cCommonLASFilter;		/*!< Common LAS filtering parameters.*/
	bool						bIntegrateOverCell;		/*!< Integrate over cell (true) or over a supplied radius (false).*/
	double						dSearchRadius;			/*!< Integration radius.*/
	bool						bQuarticKernel;			/*!< Use a Quartic weighting model for data within the integration radius.*/
};

/*! \struct SMIR_IMP_LiDAR_TREECANOPY_DENSITY
\brief	LiDAR analysis Tree Canopy Density parameters.
*/
struct SMIR_IMP_LiDAR_TREECANOPY_DENSITY
{
	SMIR_IMP_LiDAR_TREECANOPY	cCommonLASFilter;		/*!< Common LAS filtering parameters.*/
	bool						bIntegrateOverCell;		/*!< Integrate over cell (true) or over a supplied radius (false).*/
	double						dSearchRadius;			/*!< Integration radius.*/
	bool						bQuarticKernel;			/*!< Use a Quartic weighting model for data within the integration radius.*/
};

/*! \struct SMIR_IMP_LiDAR_TREECANOPY_HEIGHT
\brief	LiDAR analysis Tree Canopy Height parameters.
*/
struct SMIR_IMP_LiDAR_TREECANOPY_HEIGHT
{
	SMIR_IMP_LiDAR_TREECANOPY	cCommonLASFilter;		/*!< Common LAS filtering parameters.*/
	uint8_t						nPatchMultiplier;		/*!< Side length of the triangulation patch, expressed as a number of raster tiles (ranges from 1 to 5).*/
};

/*! \struct SMIR_IMP_Defaults
	\brief	Structure for defining recommended default parameters that interpolation system can auto compute. This is used by the Raster Interpolation API.
*/
struct SMIR_IMP_Defaults
{
	double		dCellSize; /*!< Cell size to use for output raster.*/

	// only computed if bComputeBounds for method is set to true.
	double		dMinimumX; /*!< Minimum X coordinate.*/
	double		dMinimumY; /*!< Minimum Y coordinate.*/
	double		dMaximumX; /*!< Maximum X coordinate.*/
	double		dMaximumY; /*!< Maximum Y coordinate.*/
	uint32_t	nRows; /*!< Number of rows at dCellSize for the computed bounds.*/
	uint32_t	nColumns; /*!< Number of columns at dCellSize for the computed bounds.*/
};

/*! \struct SMIR_IMP_Column
	\brief	Column data.
*/
struct SMIR_IMP_Column
{
	wchar_t	     sName[1024]; /*!< Column name.*/
	MIR_DataType nDataType;   /*!< Column data type if can be determined.*/
};

/*! \struct SMIR_IMP_ColumnInfo
	\brief	Structure for defining the number and names of the columns in a file that can be interpolated using the raster interpolation engine.
*/
struct SMIR_IMP_ColumnInfo
{
	uint32_t		nColumnCount;	/*!< Number of columns defined in vColumn.*/
	SMIR_IMP_Column	vColumns[1024];	/*!< List of column names.*/
};

/*! \struct SMIR_IMP_FileName
	\brief	File name.
*/
struct SMIR_IMP_FileName
{
	wchar_t	sFileName[1024]; /*!< File name.*/
};

/*! \struct SMIR_IMP_Filenames
	\brief	Structure for defining the number and names of the output files that will be created when interpolated using the raster interpolation engine.
*/
struct SMIR_IMP_Filenames
{
	uint32_t			nFileCount;				/*!< Number of files defined in vFileName.*/
	SMIR_IMP_FileName	vFileName[1024];		/*!< List of file names.*/
};

/*! \struct SMIR_IMP_Group
\brief	Group info.
*/
struct SMIR_IMP_Group
{
	wchar_t	 sItem[1024]; /*!< Group name.*/
	uint32_t nCount;      /*!< Count of items in this group.*/
};

/*! \struct SMIR_IMP_Groups
\brief	Structure for defining the number and names of the groups that are defined the given grouping field in the file.
*/
struct SMIR_IMP_Groups
{
	uint32_t		nCount;		/*!< Number of groups defined in pvGroups.*/
    SMIR_IMP_Group*	pvGroups;	/*!< List of group names.*/
};

/*!	\struct MIR_RegistrationPoint
\brief	Structure top control a registration point
*/
typedef struct
{
	double dWorldX, dWorldY;
	double nPixelX, nPixelY;
} MIR_RegistrationPoint;


/*!	\enum	MIR_WarpTransformType
\brief	Types of image warp transforms.
*/
enum MIR_WarpTransformType
{
	Auto = -1,
	Conformal = 0,
	Affine,
	Projective,
	Polynomial_O2,
	Conformal_Polynomial_O2,
	Polynomial_O3,
};


#ifdef __cplusplus
}
#endif

#endif