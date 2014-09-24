/******************************************************************************
 * $Id$
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement GDAL JPEG Support based on IJG libjpeg.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#define EXIFOFFSETTAG 0x8769
#define INTEROPERABILITYOFFSET 0xA005
#define GPSOFFSETTAG     0x8825
#define MAXSTRINGLENGTH 65535

#ifdef RENAME_INTERNAL_LIBTIFF_SYMBOLS
#include "../frmts/gtiff/libtiff/gdal_libtiff_symbol_rename.h"
#endif

static const struct gpsname {
    GUInt16     tag;
    const char* name;
} gpstags [] = {
    { 0x00, "EXIF_GPSVersionID" },
    { 0x01, "EXIF_GPSLatitudeRef" },
    { 0x02, "EXIF_GPSLatitude" },
    { 0x03, "EXIF_GPSLongitudeRef" },
    { 0x04, "EXIF_GPSLongitude" },
    { 0x05, "EXIF_GPSAltitudeRef" },
    { 0x06, "EXIF_GPSAltitude" },
    { 0x07, "EXIF_GPSTimeStamp" }, 
    { 0x08, "EXIF_GPSSatellites" }, 
    { 0x09, "EXIF_GPSStatus" }, 
    { 0x0a, "EXIF_GPSMeasureMode" }, 
    { 0x0b, "EXIF_GPSDOP" },
    { 0x0c, "EXIF_GPSSpeedRef"},
    { 0x0d, "EXIF_GPSSpeed"},
    { 0x0e, "EXIF_GPSTrackRef"},
    { 0x0f, "EXIF_GPSTrack"},
    { 0x10, "EXIF_GPSImgDirectionRef"},
    { 0x11, "EXIF_GPSImgDirection"},
    { 0x12, "EXIF_GPSMapDatum"},
    { 0x13, "EXIF_GPSDestLatitudeRef"},
    { 0x14, "EXIF_GPSDestLatitude"},
    { 0x15, "EXIF_GPSDestLongitudeRef"},
    { 0x16, "EXIF_GPSDestLongitude"},
    { 0x17, "EXIF_GPSDestBearingRef"},
    { 0x18, "EXIF_GPSDestBearing"},
    { 0x19, "EXIF_GPSDestDistanceRef"},
    { 0x1a, "EXIF_GPSDestDistance"},
    { 0x1b, "EXIF_GPSProcessingMethod"},
    { 0x1c, "EXIF_GPSAreaInformation"},
    { 0x1d, "EXIF_GPSDateStamp"},
    { 0x1e, "EXIF_GPSDifferential"},  
    { 0xffff,       ""}
}; 

static const struct tagname {
  GUInt16       tag;
  const char*   name;
} tagnames [] = {

//    { 0x100,	"EXIF_Image_Width"},
//    { 0x101,	"EXIF_Image_Length"},
    { 0x102,	"EXIF_BitsPerSample"},
    { 0x103,	"EXIF_Compression"},
    { 0x106,	"EXIF_PhotometricInterpretation"},
    { 0x10A,	"EXIF_Fill_Order"},
    { 0x10D,	"EXIF_Document_Name"},
    { 0x10E,	"EXIF_ImageDescription"},
    { 0x10F,	"EXIF_Make"},
    { 0x110,	"EXIF_Model"},
    { 0x111,	"EXIF_StripOffsets"},
    { 0x112,	"EXIF_Orientation"},
    { 0x115,	"EXIF_SamplesPerPixel"},
    { 0x116,	"EXIF_RowsPerStrip"},
    { 0x117,	"EXIF_StripByteCounts"},
    { 0x11A,	"EXIF_XResolution"},
    { 0x11B,	"EXIF_YResolution"},
    { 0x11C,	"EXIF_PlanarConfiguration"},
    { 0x128,	"EXIF_ResolutionUnit"},
    { 0x12D,	"EXIF_TransferFunction"},
    { 0x131,	"EXIF_Software"},
    { 0x132,	"EXIF_DateTime"},
    { 0x13B,	"EXIF_Artist"},
    { 0x13E,	"EXIF_WhitePoint"},
    { 0x13F,	"EXIF_PrimaryChromaticities"},
    { 0x156,	"EXIF_Transfer_Range"},
    { 0x200,	"EXIF_JPEG_Proc"},
    { 0x201,	"EXIF_JPEGInterchangeFormat"},
    { 0x202,	"EXIF_JPEGInterchangeFormatLength"},
    { 0x211,	"EXIF_YCbCrCoefficients"},
    { 0x212,	"EXIF_YCbCrSubSampling"},
    { 0x213,	"EXIF_YCbCrPositioning"},
    { 0x214,	"EXIF_ReferenceBlackWhite"},
    { 0x828D,	"EXIF_CFA_Repeat_Pattern_Dim"},
    { 0x828E,	"EXIF_CFA_Pattern"},
    { 0x828F,	"EXIF_Battery_Level"},
    { 0x8298,	"EXIF_Copyright"},
    { 0x829A,	"EXIF_ExposureTime"},
    { 0x829D,	"EXIF_FNumber"},
    { 0x83BB,	"EXIF_IPTC/NAA"},
//	{ 0x8769,	"EXIF_Offset"},
    { 0x8773,	"EXIF_Inter_Color_Profile"},
    { 0x8822,	"EXIF_ExposureProgram"},
    { 0x8824,	"EXIF_SpectralSensitivity"},
//	{ 0x8825,	"EXIF_GPSOffset"},
    { 0x8827,	"EXIF_ISOSpeedRatings"},
    { 0x8828,	"EXIF_OECF"},
    { 0x9000,	"EXIF_ExifVersion"},
    { 0x9003,	"EXIF_DateTimeOriginal"},
    { 0x9004,	"EXIF_DateTimeDigitized"},
    { 0x9101,	"EXIF_ComponentsConfiguration"},
    { 0x9102,	"EXIF_CompressedBitsPerPixel"},
    { 0x9201,	"EXIF_ShutterSpeedValue"},
    { 0x9202,	"EXIF_ApertureValue"},
    { 0x9203,	"EXIF_BrightnessValue"},
    { 0x9204,	"EXIF_ExposureBiasValue"},
    { 0x9205,	"EXIF_MaxApertureValue"},
    { 0x9206,	"EXIF_SubjectDistance"},
    { 0x9207,	"EXIF_MeteringMode"},
    { 0x9208,	"EXIF_LightSource"},
    { 0x9209,	"EXIF_Flash"},
    { 0x920A,	"EXIF_FocalLength"},
    { 0x9214,   "EXIF_SubjectArea"},
    { 0x927C,	"EXIF_MakerNote"},
    { 0x9286,	"EXIF_UserComment"},
    { 0x9290,	"EXIF_SubSecTime"},
    { 0x9291,	"EXIF_SubSecTime_Original"},
    { 0x9292,	"EXIF_SubSecTime_Digitized"},
    { 0xA000,	"EXIF_FlashpixVersion"},
    { 0xA001,	"EXIF_ColorSpace"},
    { 0xA002,	"EXIF_PixelXDimension"},
    { 0xA003,	"EXIF_PixelYDimension"},
    { 0xA004,       "EXIF_RelatedSoundFile"},
//	{ 0xA005,	"EXIF_InteroperabilityOffset"},
    { 0xA20B,	"EXIF_FlashEnergy"},	  // 0x920B in TIFF/EP
    { 0xA20C,	"EXIF_SpatialFrequencyResponse"},   // 0x920C    -  -
    { 0xA20E,	"EXIF_FocalPlaneXResolution"},     // 0x920E    -  -
    { 0xA20F,	"EXIF_FocalPlaneYResolution"},     // 0x920F    -  -
    { 0xA210,	"EXIF_FocalPlaneResolutionUnit"},  // 0x9210    -  -
    { 0xA214,	"EXIF_SubjectLocation"},	// 0x9214    -  -
    { 0xA215,	"EXIF_ExposureIndex"},		// 0x9215    -  -
    { 0xA217,	"EXIF_SensingMethod"},		// 0x9217    -  -
    { 0xA300,	"EXIF_FileSource"},
    { 0xA301,	"EXIF_SceneType"},
    { 0xA302,   "EXIF_CFAPattern"},
    { 0xA401,   "EXIF_CustomRendered"},
    { 0xA402,   "EXIF_ExposureMode"},
    { 0XA403,   "EXIF_WhiteBalance"},
    { 0xA404,   "EXIF_DigitalZoomRatio"},
    { 0xA405,   "EXIF_FocalLengthIn35mmFilm"},
    { 0xA406,   "EXIF_SceneCaptureType"},
    { 0xA407,   "EXIF_GainControl"},
    { 0xA408,   "EXIF_Contrast"},
    { 0xA409,   "EXIF_Saturation"},
    { 0xA40A,   "EXIF_Sharpness"},
    { 0xA40B,   "EXIF_DeviceSettingDescription"},
    { 0xA40C,   "EXIF_SubjectDistanceRange"},
    { 0xA420,   "EXIF_ImageUniqueID"},
    { 0x0000,       ""}
};


static const struct intr_tag {
  GInt16        tag;
  const char*   name;
} intr_tags [] = {

    { 0x1,	"EXIF_Interoperability_Index"},
    { 0x2,	"EXIF_Interoperability_Version"},
    { 0x1000,	"EXIF_Related_Image_File_Format"},
    { 0x1001,	"EXIF_Related_Image_Width"},
    { 0x1002,	"EXIF_Related_Image_Length"},
    { 0x0000,       ""}
};

typedef enum {
        TIFF_NOTYPE     = 0,    /* placeholder */
        TIFF_BYTE       = 1,    /* 8-bit unsigned integer */
        TIFF_ASCII      = 2,    /* 8-bit bytes w/ last byte null */
        TIFF_SHORT      = 3,    /* 16-bit unsigned integer */
        TIFF_LONG       = 4,    /* 32-bit unsigned integer */
        TIFF_RATIONAL   = 5,    /* 64-bit unsigned fraction */
        TIFF_SBYTE      = 6,    /* !8-bit signed integer */
        TIFF_UNDEFINED  = 7,    /* !8-bit untyped data */
        TIFF_SSHORT     = 8,    /* !16-bit signed integer */
        TIFF_SLONG      = 9,    /* !32-bit signed integer */
        TIFF_SRATIONAL  = 10,   /* !64-bit signed fraction */
        TIFF_FLOAT      = 11,   /* !32-bit IEEE floating point */
        TIFF_DOUBLE     = 12,   /* !64-bit IEEE floating point */
        TIFF_IFD        = 13    /* %32-bit unsigned integer (offset) */
} TIFFDataType;

/*
 * TIFF Image File Directories are comprised of a table of field
 * descriptors of the form shown below.  The table is sorted in
 * ascending order by tag.  The values associated with each entry are
 * disjoint and may appear anywhere in the file (so long as they are
 * placed on a word boundary).
 *
 * If the value is 4 bytes or less, then it is placed in the offset
 * field to save space.  If the value is less than 4 bytes, it is
 * left-justified in the offset field.
 */
typedef struct {
        GUInt16          tdir_tag;       /* see below */
        GUInt16          tdir_type;      /* data type; see below */
        GUInt32          tdir_count;     /* number of items; length in spec */
        GUInt32          tdir_offset;    /* byte offset to field data */
} TIFFDirEntry;

CPL_C_START
extern	int TIFFDataWidth(TIFFDataType);    /* table of tag datatype widths */
extern	void TIFFSwabShort(GUInt16*);
extern	void TIFFSwabLong(GUInt32*);
extern	void TIFFSwabDouble(double*);
extern	void TIFFSwabArrayOfShort(GUInt16*, unsigned long);
extern	void TIFFSwabArrayOfLong(GUInt32*, unsigned long);
extern	void TIFFSwabArrayOfDouble(double*, unsigned long);
CPL_C_END

