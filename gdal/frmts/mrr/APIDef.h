/******************************************************************************************************************************
	MapInfo Professional MRR Raster API

	Copyright © 1985-2016,Pitney Bowes Software Inc.
	All rights reserved.
	Confidential Property of Pitney Bowes Software
	
******************************************************************************************************************************/

/*!	\file	APIDEF.h
	\brief	MapInfo Professional Raster C-API header definitions.

	This file contains definitions used by the MapInfo MRR Raster API.
	Copyright © 2014,Pitney Bowes Software Inc.
	All rights reserved.
	Confidential Property of Pitney Bowes Software
*/

#ifndef APIDEF_H
#define APIDEF_H

#pragma once

#include <cstdint>
#ifdef __linux__
#include <time.h>
#endif
#ifdef __cplusplus
extern "C" 
{
#endif

//	Constants
typedef int32_t				MIRResult;									//!> Result code returned by API functions

static const int32_t		MIRSuccess					= 0;			//!< Indicates successful result for an operation
static const uint32_t		InvalidTracker				= 0u;			//!< Indicates no tracker is supplied to an operation
static const size_t			MAX_COORDSYS				= 256u;			//!< Maximum number of coordinate system objects
static const uint32_t		InvalidBinCount				= UINT32_MAX;	//!< Indicates no bin count is supplied to an operation

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
	//	Invalid					0 - 9
	MIR_UNDEFINED			= 0,		//!< Undefined data type
	MIR_NULL,							//!< Null data type
	MIR_EMPTY,							//!< Empty data type
	//	Boolean					10 - 19
	MIR_BOOLEAN_INT8		= 10,		//!< 8-bit boolean type
	MIR_BOOLEAN_INT32,					//!< 32-bit boolean type
	//	Bit						20 - 29
	MIR_BIT1				= 20,		//!< 1-bit data
	MIR_BIT2,							//!< 2-bit crumb
	MIR_BIT4,							//!< 4-bit nibble
	//	Unsigned integers		30 - 39
	MIR_UNSIGNED_INT8		= 30,		//!< 8-bit unsigned integer
	MIR_UNSIGNED_INT16,					//!< 16-bit unsigned integer
	MIR_UNSIGNED_INT32,					//!< 32-bit unsigned integer
	MIR_UNSIGNED_INT64,					//!< 64-bit unsigned integer
	//	Signed integers			40 - 49
	MIR_SIGNED_INT8			= 40,		//!< 8-bit signed integer
	MIR_SIGNED_INT16,					//!< 16-bit signed integer
	MIR_SIGNED_INT32,					//!< 32-bit signed integer
	MIR_SIGNED_INT64,					//!< 64-bit signed integer
	//	Floating point			50 - 59
	MIR_REAL2				= 50,		//!< 2 byte real (unimplemented)
	MIR_REAL4,							//!< 4 byte real
	MIR_REAL8,							//!< 8 byte real
	MIR_REAL_LONG,						//!< 8 byte real
	//	Complex					60 - 69
	MIR_COMPLEX_INT16		= 60,		//!< 16-bit signed integer complex number (real, imaginary)
	MIR_COMPLEX_INT32,					//!< 32-bit signed integer complex number (real, imaginary)
	MIR_COMPLEX_REAL4,					//!< 4 byte real complex number (real, imaginary)
	MIR_COMPLEX_REAL8,					//!< 8 byte real complex number (real, imaginary)
	//	Time					70 - 79
	MIR_DATETIME_OLE		= 70,		//!< Windows DATE, 8 byte real
	MIR_DATETIME_CPP,					//!< Standard time_t, 64 bit integer
	//	String					80 - 89
	MIR_STRING				= 80,		//!< ASCII, variable length
	MIR_FIXED_STRING,					//!< ASCII, Fixed length
	MIR_STRING_UTF8,					//!< Unicode, variable length, std::string
	MIR_STRING_UTF16,					//!< Unicode, variable length, std::wstring
	MIR_STRING_UTF32,					//!< Unicode, variable length, std::u32string
	/*	UTF-7
	UCS-2
	UCS-4	*/
	//	Blobs					90 - 99
	MIR_BINARY_OBJECT		= 90,		//!< Variable length
	MIR_FIXED_BINARY_OBJECT,			//!< Fixed length
	//	Color					100 - 149
	MIR_RED					= 100,		//!< 8 bit
	MIR_GREEN,							//!< 8 bit
	MIR_BLUE,							//!< 8 bit
	MIR_GREY,							//!< 8 bit
	MIR_ALPHA,							//!< 8 bit, Alpha = opacity: 0=transparent, 1=opaque
	MIR_RED_ALPHA,						//!< 8|8 bit
	MIR_GREEN_ALPHA,					//!< 8|8 bit
	MIR_BLUE_ALPHA,						//!< 8|8 bit
	MIR_GREY_ALPHA,						//!< 8|8 bit
	MIR_RGB,							//!< 8|8|8 bit
	MIR_RGBA,							//!< 8|8|8|8 bit
	MIR_BGR,							//!< 8|8|8 bit
	MIR_BGRA							//!< 8|8|8|8 bit
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
//	Raster properties

/*!	\enum	MIR_FieldType
	\brief	Raster field type.
*/
enum MIR_FieldType
{
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

	//Grouped into Balnaced, speed and space for data and imagery compression.
	MIR_Compression_DataBalanced		= 50,	//!< Lossless data compression, balanced.
	MIR_Compression_DataSpeed			= 51,	//!< Lossless data compression, favour higher encoding speed.
	MIR_Compression_DataSpace			= 52,	//!< Lossless data compression, favour higher compression.
	MIR_Compression_ImageBalanced		= 53,	//!< Lossless image compression, balanced.
	MIR_Compression_ImageSpeed			= 54,	//!< Lossless image compression, favour higher encoding speed.
	MIR_Compression_ImageSpace			= 55,	//!< Lossless image compression, favour higher compression.
	MIR_Compression_ImageLossyBalanced	= 56,	//!< Lossy image compression, balanced.
	MIR_Compression_ImageLossySpeed		= 57,	//!< Lossy image compression, favour higher encoding speed.
	MIR_Compression_ImageLossySpace		= 58,	//!< Lossy image compression, favour higher compression.

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
	MIR_Encoding_None		= -1,				/*!< No encoding.*/
	MIR_PreviousColumnValue = 0,				/*!< Predict the value from previous column.*/
	MIR_PreviousColumnLinear = 1				/*!< Linear estimate the value from previous two columns.*/
};

/*!	\enum	MIR_EventType
	\brief	Raster event edit type.
*/
enum MIR_EventType
{
	MIR_EET_Partial		= 0,
	MIR_EET_Total		= 1
};

/*****************************************************************************************************************************/
//	Open and create rasters

/*! \enum	MIR_RasterSupportMode
	\brief	Raster access support modes.
*/
enum MIR_RasterSupportMode
{
	MIR_Support_Native,					/*!< Guarantees sequential tile access to the base level only. 
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
	MIR_Support_Full_Speed				/*!< Guarantees high performance random access to the base level
												 and overview pyramid. Guarantees an overview pyramid will exist.
												 If a cache file is already present, utilizes it, else generates 
												 a permanent cache file. If the raster location is read only,
												 generates a temporary in the temp directory and deletes
												 the cache file on close. Always generates a pyramid by decimation
												 if supported by the raster driver. */
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
	\brief	A 'variant' for data types <=256 characters in size.
*/
struct SMIR_Variant
{
	MIR_DataType				nType;
	uint8_t						nSize;
	uint8_t						ucData[256];
};

/*!	\struct	SMIR_VariantArray
	\brief	An 'array' of 'variants' for data types <=256 characters in size and <=256 items.
*/
struct SMIR_VariantArray
{
	uint8_t						nSize;
	SMIR_Variant				vVariant[256];
};

/*! \struct SMIR_LevelInfoState
	\brief	
*/
struct SMIR_LevelInfoState
{
	char								nResolution[2];
	char								nCellBBox[2];
};

/*! \struct SMIR_LevelInfo
	\brief	
*/
struct SMIR_LevelInfo
{
	int32_t								nResolution;
	int64_t								nCellBBoxXMin,nCellBBoxYMin;
	int64_t								nCellBBoxXMax,nCellBBoxYMax;

	//	User modifiable variable states
	SMIR_LevelInfoState				DataState;
};

/*! \struct SMIR_EventInfoState
	\brief	
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
	MIR_BandType					nType;						/*!< The type of band in the source raster as defined in \link subsection17 MIR_BandType \endlink. */
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

/*! \struct SMIR_ClassTableFieldInfo
	\brief	Structure for defining the properties of a classification table field.
*/
struct SMIR_ClassTableFieldInfo
{
	wchar_t							sName[256];					/*!< Name of the classification table field. */
	MIR_ClassTableFieldType				nType;						/*!< Primary classification table meta data. See \link subsection20 MIR_ClassTableFieldType \endlink for more details.*/
	MIR_DataType					nDataType;					/*!< Data type of the classification table field. */
};

/*! \struct SMIR_ClassTableInfo
	\brief	Structure for defining the properties of a classification table.
*/
struct SMIR_ClassTableInfo
{
	wchar_t							sName[256];					/*!< Name of the classification table. */
	uint32_t						nFieldCount;				/*!< Number of fields in the classification table.*/
	SMIR_ClassTableFieldInfo		vFieldInfo[256];			//	
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
	\brief	
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
	\brief	
*/
struct SMIR_RasterInfoState
{
	char								nVersion[2];
	char								nName[2];
	char								nFileName[2];
	char								nFileList[2];
	char								nGridSize[2];
	char								nCoordinateSystem[2];
	char								nUnderviewMapSize[2];
	char								nUnderviewTileSize[2];
	char								nBaseMapSize[2];
	char								nBaseTileSize[2];
	char								nOverviewMapSize[2];
	char								nOverviewTileSize[2];
	char								nColour[2];
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
	\brief
*/
struct SMIR_HistogramBin
{
	//	The range of the bin - continuous from bin to bin
	double								dBottom;		/// Data >= dBottom	
	double								dTop;			//	Data <=  dTop
	//	The range of the actual samples in the bin
	double								dValBottom;
	double								dValTop;
	//	Count of samples in bin
	double								dCount;			
	//	Percentage of samples up to and including this bin >0 and <=1
	double								dCumulativeCount;		
};

/*!	\struct	SMIR_Histogram
	\brief
*/
struct SMIR_Histogram
{
	uint32_t						nBinCount; /*!< Count of bins.*/
	SMIR_HistogramBin*				pvcBins;   /*!< Collection of bins.*/
};

/*!	\struct	SMIR_Statistics
	\brief
*/
struct SMIR_Statistics
{
	//	Count statistics
	//	Acquired with Mode MIR_StatsMode_Count & MIR_StatsMode_Summary & MIR_StatsMode_Distribution & MIR_StatsMode_Spatial
	uint64_t						nSampleCount;			/*!< Total Sample Count. */
	uint64_t						nValidSampleCount;		/*!< Total valid sample count. */
	uint64_t						nInvalidSampleCount;	/*!< Total invalid sample count */
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

/*! \enum	MIR_InterpolationMethod
	\brief	Enum for defining the various types of interpolation methods supported by the MIRaster IO API for overviews and underviews.
*/
enum MIR_InterpolationMethod
{
	Interp_Nearest		= 0,
	Interp_Linear		= 1,
	Interp_CubicOperator= 2,
	Interp_Cubic		= 3,
	Interp_Default					//	Always make this the last entry
};

#ifdef __cplusplus
}
#endif

#endif