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
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.1  2005/07/19 19:35:32  fwarmerdam
 * New
 *
 *
 */

#define	ord(e)	((int)e)
#define EXIFOFFSETTAG 0x8769
#define INTEROPERABILITYOFFSET 0xA005
#define MAXSTRINGLENGTH 65535
#define TIFFHEADER 12


static struct tagname {
  GUInt16 tag;
  char*  name;
} tagnames [] = {

//	{ 0x100,	"EXIF_Image_Width"},
//	{ 0x101,	"EXIF_Image_Length"},
	{ 0x102,	"EXIF_Bits_Per_Sample"},
	{ 0x103,	"EXIF_Compression"},
	{ 0x106,	"EXIF_Photometric+Interpretation"},
	{ 0x10A,	"EXIF_Fill_Order"},
	{ 0x10D,	"EXIF_Document_Name"},
	{ 0x10E,	"EXIF_Image_Description"},
	{ 0x10F,	"EXIF_Make"},
	{ 0x110,	"EXIF_Model"},
	{ 0x111,	"EXIF_Strip_Offsets"},
	{ 0x112,	"EXIF_Orientation"},
	{ 0x115,	"EXIF_Samples_Per_Pixel"},
	{ 0x116,	"EXIF_Rows_Per_Strip"},
	{ 0x117,	"EXIF_Strip_Byte_Counts"},
	{ 0x11A,	"EXIF_X_Resolution"},
	{ 0x11B,	"EXIF_Y_Resolution"},
	{ 0x11C,	"EXIF_Planar_Configuration"},
	{ 0x128,	"EXIF_Resolution_Unit"},
	{ 0x12D,	"EXIF_Transfer_Function"},
	{ 0x131,	"EXIF_Software"},
	{ 0x132,	"EXIF_Date_Time"},
	{ 0x13B,	"EXIF_Artist"},
	{ 0x13E,	"EXIF_White_Point"},
	{ 0x13F,	"EXIF_Primary_Chromaticities"},
	{ 0x156,	"EXIF_Transfer_Range"},
	{ 0x200,	"EXIF_JPEG_Proc"},
	{ 0x201,	"EXIF_JPEG_Interchange_Format"},
	{ 0x202,	"EXIF_JPEG_Interchange_Format_Length"},
	{ 0x211,	"EXIF_YCbCr_Coefficients"},
	{ 0x212,	"EXIF_YCbCr_Sub_Sampling"},
	{ 0x213,	"EXIF_YCbCr_Positioning"},
	{ 0x214,	"EXIF_Reference_Black_White"},
	{ 0x828D,	"EXIF_CFA_Repeat_Pattern_Dim"},
	{ 0x828E,	"EXIF_CFA_Pattern"},
	{ 0x828F,	"EXIF_Battery_Level"},
	{ 0x8298,	"EXIF_Copyright"},
	{ 0x829A,	"EXIF_Exposure_Time"},
	{ 0x829D,	"EXIF_F_Number"},
	{ 0x83BB,	"EXIF_IPTC/NAA"},
	{ 0x8769,	"EXIF_Offset"},
	{ 0x8773,	"EXIF_Inter_Color_Profile"},
	{ 0x8822,	"EXIF_Exposure_Program"},
	{ 0x8824,	"EXIF_Spectral_Sensitivity"},
	{ 0x8825,	"EXIF_GPS_Info"},
	{ 0x8827,	"EXIF_ISO_Speed_Ratings"},
	{ 0x8828,	"EXIF_OECF"},
	{ 0x9000,	"EXIF_Version"},
	{ 0x9003,	"EXIF_Date_Time_Original"},
	{ 0x9004,	"EXIF_Date_Time_Digitized"},
	{ 0x9101,	"EXIF_Components_Configuration"},
	{ 0x9102,	"EXIF_Compressed_Bits_Per_Pixel"},
	{ 0x9201,	"EXIF_Shutter_Speed_Value"},
	{ 0x9202,	"EXIF_Aperture_Value"},
	{ 0x9203,	"EXIF_Brightness_Value"},
	{ 0x9204,	"EXIF_Exposure_Bias_Value"},
	{ 0x9205,	"EXIF_Max_Aperture_Value"},
	{ 0x9206,	"EXIF_Subject_Distance"},
	{ 0x9207,	"EXIF_Metering_Mode"},
	{ 0x9208,	"EXIF_Light_Source"},
	{ 0x9209,	"EXIF_Flash"},
	{ 0x920A,	"EXIF_Focal_Length"},
//	{ 0x927C,	"EXIF_Maker_Note"},
	{ 0x9286,	"EXIF_User_Comment"},
	{ 0x9290,	"EXIF_Sub_Sec_Time"},
	{ 0x9291,	"EXIF_Sub_Sec_Time_Original"},
	{ 0x9292,	"EXIF_Sub_Sec_Time_Digitized"},
	{ 0xA000,	"EXIF_Flash_Pix_Version"},
	{ 0xA001,	"EXIF_Color_Space"},
//	{ 0xA002,	"EXIF_Image_Width"},
//	{ 0xA003,	"EXIF_Image_Length"},
	{ 0xA005,	"EXIF_Interoperability_Offset"},
	{ 0xA20B,	"EXIF_Flash_Energy"},	  // 0x920B in TIFF/EP
	{ 0xA20C,	"EXIF_Spatial_Frequency_Response"},   // 0x920C    -  -
	{ 0xA20E,	"EXIF_Focal_Plane_X_Resolution"},     // 0x920E    -  -
	{ 0xA20F,	"EXIF_Focal_Plane_Y_Resolution"},     // 0x920F    -  -
	{ 0xA210,	"EXIF_Focal_Plane_Resolution_Unit"},  // 0x9210    -  -
	{ 0xA214,	"EXIF_Subject_Location"},	// 0x9214    -  -
	{ 0xA215,	"EXIF_Exposure_Index"},		// 0x9215    -  -
	{ 0xA217,	"EXIF_Sensing_Method"},		// 0x9217    -  -
	{ 0xA300,	"EXIF_File_Source"},
	{ 0xA301,	"EXIF_Scene_Type"},
	{ 0x0000,       ""}
};

static struct intr_tag {
  GInt16 tag;
  char*  name;
} intr_tags [] = {

	{ 0x1,	"EXIF_Interoperability_Index"},
	{ 0x2,	"EXIF_Interoperability_Version"},
	{ 0x1000,	"EXIF_Related_Image_File_Format"},
	{ 0x1001,	"EXIF_Related_Image_Width"},
	{ 0x1002,	"EXIF_Related_Image_Length"},
	{ 0x0000,       ""}
};


static int datawidth[] = {
    0,	/* nothing */
    1,	/* TIFF_BYTE */
    1,	/* TIFF_ASCII */
    2,	/* TIFF_SHORT */
    4,	/* TIFF_LONG */
    8,	/* TIFF_RATIONAL */
    1,	/* TIFF_SBYTE */
    1,	/* TIFF_UNDEFINED */
    2,	/* TIFF_SSHORT */
    4,	/* TIFF_SLONG */
    8,	/* TIFF_SRATIONAL */
    4,	/* TIFF_FLOAT */
    8,	/* TIFF_DOUBLE */
};

#define TIFF_VERSION            42
#define TIFF_BIGTIFF_VERSION    43

#define TIFF_BIGENDIAN          0x4d4d
#define TIFF_LITTLEENDIAN       0x4949

/*
 * TIFF header.
 */
typedef struct {
        GUInt16  tiff_magic;     /* magic number (defines byte order) */
#define TIFF_MAGIC_SIZE         2
        GUInt16  tiff_version;   /* TIFF version number */
#define TIFF_VERSION_SIZE       2
        GUInt32  tiff_diroff;    /* byte offset to first directory */
#define TIFF_DIROFFSET_SIZE     4
} TIFFHeader;


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

typedef GUInt32 tsize_t;          /* i/o size in bytes */

#define	NWIDTHS	(sizeof (datawidth) / sizeof (datawidth[0]))


CPL_C_START
extern	int TIFFDataWidth(TIFFDataType);    /* table of tag datatype widths */
extern	void TIFFSwabShort(GUInt16*);
extern	void TIFFSwabLong(GUInt32*);
extern	void TIFFSwabDouble(double*);
extern	void TIFFSwabArrayOfShort(GUInt16*, unsigned long);
extern	void TIFFSwabArrayOfTriples(GByte*, unsigned long);
extern	void TIFFSwabArrayOfLong(GUInt32*, unsigned long);
extern	void TIFFSwabArrayOfDouble(double*, unsigned long);
CPL_C_END
