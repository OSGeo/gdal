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
 * Revision 1.23  2005/07/19 15:21:57  dnadeau
 * added exif support
 *
 * Revision 1.22  2005/05/23 06:57:12  fwarmerdam
 * Use lockedblockrefs
 *
 * Revision 1.21  2005/04/27 16:35:58  fwarmerdam
 * PAM enable
 *
 * Revision 1.20  2005/03/23 00:27:32  fwarmerdam
 * First pass try at support for the libjpeg Mk1 library.
 *
 * Revision 1.19  2005/03/22 21:48:11  fwarmerdam
 * Added preliminary Mk1 libjpeg support.  Compress still not working.
 *
 * Revision 1.18  2005/02/25 15:17:35  fwarmerdam
 * Use dataset io to fetch to provide (potentially) faster interleaved
 * reads.
 *
 * Revision 1.17  2003/10/31 00:58:55  warmerda
 * Added support for .jpgw as per request from Markus.
 *
 * Revision 1.16  2003/04/04 13:45:12  warmerda
 * Made casting to JSAMPLE explicit.
 *
 * Revision 1.15  2002/11/23 18:57:06  warmerda
 * added CREATIONOPTIONS metadata on driver
 *
 * Revision 1.14  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.13  2002/07/13 04:16:39  warmerda
 * added WORLDFILE support
 *
 * Revision 1.12  2002/06/20 19:57:04  warmerda
 * ensure GetGeoTransform always sets geotransform.
 *
 * Revision 1.11  2002/06/18 02:50:20  warmerda
 * fixed multiline string constants
 *
 * Revision 1.10  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.9  2001/12/11 16:50:16  warmerda
 * try to push green and blue values into cache when reading red
 *
 * Revision 1.8  2001/11/11 23:51:00  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.7  2001/08/22 17:11:30  warmerda
 * added support for .wld world files
 *
 * Revision 1.6  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.5  2001/06/01 14:13:12  warmerda
 * Improved magic number testing.
 *
 * Revision 1.4  2001/05/01 18:18:28  warmerda
 * added world file support
 *
 * Revision 1.3  2001/01/12 21:19:25  warmerda
 * added progressive support
 *
 * Revision 1.2  2000/07/07 15:11:01  warmerda
 * added QUALITY=n creation option
 *
 * Revision 1.1  2000/04/28 20:57:57  warmerda
 * New
 *
 */

#include "gdal_pam.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
#include "jpeglib.h"
#include "tiffio.h"
CPL_C_END

CPL_C_START
void	GDALRegister_JPEG(void);
CPL_C_END


#define	ord(e)	((int)e)
#define EXIFOFFSETTAG 0x8769
#define INTEROPERABILITYOFFSET 0xA005
#define MAXSTRINGLENGTH 65535
#define TIFFHEADER 12


static struct tagname {
  UINT16 tag;
  char*  name;
} tagnames [] = {

	{ 0x100,	"EXIF_Image_Width"},
	{ 0x101,	"EXIF_Image_Length"},
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
	{ 0x927C,	"EXIF_Maker_Note"},
	{ 0x9286,	"EXIF_User_Comment"},
	{ 0x9290,	"EXIF_Sub_Sec_Time"},
	{ 0x9291,	"EXIF_Sub_Sec_Time_Original"},
	{ 0x9292,	"EXIF_Sub_Sec_Time_Digitized"},
	{ 0xA000,	"EXIF_Flash_Pix_Version"},
	{ 0xA001,	"EXIF_Color_Space"},
	{ 0xA002,	"EXIF_Image_Width"},
	{ 0xA003,	"EXIF_Image_Length"},
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
  UINT16 tag;
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

#define	NWIDTHS	(sizeof (datawidth) / sizeof (datawidth[0]))

/************************************************************************/
/* ==================================================================== */
/*				JPGDataset				*/
/* ==================================================================== */
/************************************************************************/

class JPGRasterBand;

class JPGDataset : public GDALPamDataset
{
    friend class JPGRasterBand;

    struct jpeg_decompress_struct sDInfo;
    struct jpeg_error_mgr sJErr;

    int	   bGeoTransformValid;
    double adfGeoTransform[6];

    FILE   *fpImage;
    int    nLoadedScanline;
    GByte  *pabyScanline;

    char   **papszMetadata;
    char   **papszSubDatasets;
    int	   bigendian;
    int    nExifOffset;
    int    nInterOffset;
    int	   bSwabflag;
    int    nTiffDirStart;

    CPLErr LoadScanline(int);
    void   Restart();
    
    CPLErr EXIFExtractMetadata(FILE *, int);
    CPLErr EXIFInit(FILE *);
    void   EXIFPrintByte(char *, const char*, TIFFDirEntry* );
    void   EXIFPrintShort(char *, const char*, TIFFDirEntry*);
    void   EXIFPrintData(char *, uint16, uint32, unsigned char* );

  public:
                 JPGDataset();
                 ~JPGDataset();

    virtual CPLErr GetGeoTransform( double * );
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            JPGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class JPGRasterBand : public GDALPamRasterBand
{
    friend class JPGDataset;

  public:

                   JPGRasterBand( JPGDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                         EXIFPrintByte()                              */
/************************************************************************/
void JPGDataset::EXIFPrintByte(char *pszData, 
			       const char* fmt, TIFFDirEntry* dp)
{
  char* sep = "";
  
  if (bSwabflag) {
    switch ((int)dp->tdir_count) {
    case 4: sprintf(pszData, fmt, sep, dp->tdir_offset&0xff);
      sep = " ";
    case 3: sprintf(pszData, fmt, sep, (dp->tdir_offset>>8)&0xff);
      sep = " ";
    case 2: sprintf(pszData, fmt, sep, (dp->tdir_offset>>16)&0xff);
      sep = " ";
    case 1: sprintf(pszData, fmt, sep, dp->tdir_offset>>24);
    }
  } else {
    switch ((int)dp->tdir_count) {
    case 4: sprintf(pszData, fmt, sep, dp->tdir_offset>>24);
      sep = " ";
    case 3: sprintf(pszData, fmt, sep, (dp->tdir_offset>>16)&0xff);
      sep = " ";
    case 2: sprintf(pszData, fmt, sep, (dp->tdir_offset>>8)&0xff);
      sep = " ";
    case 1: sprintf(pszData, fmt, sep, dp->tdir_offset&0xff);
    }
  }
}

/************************************************************************/
/*                         EXIFPrintShort()                             */
/************************************************************************/
void JPGDataset::EXIFPrintShort(char *pszData, const char* fmt, 
			     TIFFDirEntry* dp)
{
  char *sep = "";
  if (bSwabflag) {
    switch (dp->tdir_count) {
    case 2: sprintf(pszData, fmt, sep, dp->tdir_offset&0xffff);
      sep = " ";
    case 1: sprintf(pszData, fmt, sep, dp->tdir_offset>>16);
    }
  } else {
    switch (dp->tdir_count) {
    case 2: sprintf(pszData, fmt, sep, dp->tdir_offset>>16);
      sep = " ";
    case 1: sprintf(pszData, fmt, sep, dp->tdir_offset&0xffff);
    }
  }
}

/************************************************************************/
/*                         EXIFPrintData()                              */
/************************************************************************/
void JPGDataset::EXIFPrintData(char* pszData, uint16 type, 
			    uint32 count, unsigned char* data)
{
  char* sep = "";
  char  pszTemp[MAXSTRINGLENGTH];
  pszData[0]='\0';
  switch (type) {
  case TIFF_UNDEFINED:
  case TIFF_BYTE:
    while (count-- > 0){
      sprintf(pszTemp, "%s%#02x", sep, *data++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;

  case TIFF_SBYTE:
    while (count-- > 0){
      sprintf(pszTemp, "%s%d", sep, *(char *)data++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
	  
  case TIFF_ASCII:
    sprintf(pszData, "%s", data);
    break;

  case TIFF_SHORT: {
    register uint16 *wp = (uint16*)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s%u", sep, *wp++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_SSHORT: {
    register int16 *wp = (int16*)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s%d", sep, *wp++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_LONG: {
    register uint32 *lp = (uint32*)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s%lu", sep, (unsigned long) *lp++);
      sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_SLONG: {
    register int32 *lp = (int32*)data;
    while (count-- > 0){
      sprintf(pszTemp, "%s%ld", sep, (long) *lp++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_RATIONAL: {
    register uint32 *lp = (uint32*)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s(%g)", sep,
	      (double) lp[0]/ (double)lp[1]);
      sep = " ";
      lp += 2;
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_SRATIONAL: {
    register int32 *lp = (int32*)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s(%g)", sep,
	      (double) lp[0]/ (double) lp[1]);
      sep = " ";
      lp += 2;
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_FLOAT: {
    register float *fp = (float *)data;
    while (count-- > 0){
      sprintf(pszTemp, "%s%g", sep, *fp++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_DOUBLE: {
    register double *dp = (double *)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s%g", sep, *dp++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  }
}



/************************************************************************/
/*                        EXIFExtractMetadata()                         */
/*                                                                      */
/*           Create Metadata from Information file directory APP1       */
/************************************************************************/
CPLErr JPGDataset::EXIFInit(FILE *fp)
{
  int           one = 1;
  TIFFHeader    hdr;
  
  bigendian = (*(char *)&one == 0);


/* -------------------------------------------------------------------- */
/*      Read TIFF header                                                */
/* -------------------------------------------------------------------- */
  VSIFSeek(fp, TIFFHEADER, SEEK_SET);
  if(VSIFRead(&hdr,1,sizeof(hdr),fp) != sizeof(hdr)) 
    CPLError( CE_Failure, CPLE_FileIO,
	      "Failed to read %d byte from image header.",
	      sizeof(hdr));

  if (hdr.tiff_magic != TIFF_BIGENDIAN && hdr.tiff_magic != TIFF_LITTLEENDIAN)
    CPLError( CE_Failure, CPLE_AppDefined,
	      "Not a TIFF file, bad magic number %u (%#x)",
	      hdr.tiff_magic, hdr.tiff_magic);

  if (hdr.tiff_magic == TIFF_BIGENDIAN)    bSwabflag = !bigendian;
  if (hdr.tiff_magic == TIFF_LITTLEENDIAN) bSwabflag = bigendian;

  if (bSwabflag) {
    TIFFSwabShort(&hdr.tiff_version);
    TIFFSwabLong(&hdr.tiff_diroff);
  }


  if (hdr.tiff_version != TIFF_VERSION)
    CPLError(CE_Failure, CPLE_AppDefined,
	     "Not a TIFF file, bad version number %u (%#x)",
	     hdr.tiff_version, hdr.tiff_version); 
  nTiffDirStart = hdr.tiff_diroff;

  printf("Magic: %#x <%s-endian> Version: %#x\n",
	 hdr.tiff_magic,
	 hdr.tiff_magic == TIFF_BIGENDIAN ? "big" : "little",
	 hdr.tiff_version);

  return (CE_None);
}

/************************************************************************/
/*                        EXIFExtractMetadata()                         */
/*                                                                      */
/*      Extract all entry from a IFD                                    */
/************************************************************************/
CPLErr JPGDataset::EXIFExtractMetadata(FILE *fp, int nOffset)
{
  uint16        nEntryCount;
  int space;
  unsigned int           n,i;
  char          pszTemp[MAXSTRINGLENGTH];
  char          pszName[MAXSTRINGLENGTH];

  TIFFDirEntry *poTIFFDirEntry;
  TIFFDirEntry *poTIFFDir;
  struct tagname *poExifTags;
  struct intr_tag *poInterTags;

/* -------------------------------------------------------------------- */
/*      Read number of entry in directory                               */
/* -------------------------------------------------------------------- */
  VSIFSeek(fp, nOffset+TIFFHEADER, SEEK_SET);

  if(VSIFRead(&nEntryCount,1,sizeof(UINT16),fp) != sizeof(UINT16)) 
    CPLError( CE_Failure, CPLE_AppDefined,
	      "Error directory count");

  if (bSwabflag)
    TIFFSwabShort(&nEntryCount);

  poTIFFDir = (TIFFDirEntry *)CPLMalloc(nEntryCount * sizeof(TIFFDirEntry));

  if (poTIFFDir == NULL) 
    CPLError( CE_Failure, CPLE_AppDefined,
	      "No space for TIFF directory");
  
/* -------------------------------------------------------------------- */
/*      Read all directory entries                                      */
/* -------------------------------------------------------------------- */
  n = VSIFRead(poTIFFDir, 1,nEntryCount*sizeof(TIFFDirEntry),fp);
  if (n != nEntryCount*sizeof(TIFFDirEntry)) 
    CPLError( CE_Failure, CPLE_AppDefined,
	      "Could not read all directories");

/* -------------------------------------------------------------------- */
/*      Parse all entry information in this directory                   */
/* -------------------------------------------------------------------- */
  for(poTIFFDirEntry = poTIFFDir,i=nEntryCount; i > 0; i--,poTIFFDirEntry++) {
    if (bSwabflag) {
      TIFFSwabShort(&poTIFFDirEntry->tdir_tag);
      TIFFSwabShort(&poTIFFDirEntry->tdir_type);
      TIFFSwabLong (&poTIFFDirEntry->tdir_count);
      TIFFSwabLong (&poTIFFDirEntry->tdir_offset);
    }

/* -------------------------------------------------------------------- */
/*      Find Tag name in table                                          */
/* -------------------------------------------------------------------- */
    for (poExifTags = tagnames; poExifTags->tag; poExifTags++)
      if(poExifTags->tag == poTIFFDirEntry->tdir_tag){
	  strcpy(pszName, poExifTags->name);
      }
/* -------------------------------------------------------------------- */
/*      If the tag was not found, look into the interoperability table  */
/* -------------------------------------------------------------------- */
      if(!poExifTags->tag)
	for(poInterTags = intr_tags; poInterTags->tag; poInterTags++)
	  if(poInterTags->tag == poTIFFDirEntry->tdir_tag) 
	    strcpy(pszName, poInterTags->name);

/* -------------------------------------------------------------------- */
/*      Save important directory tag offset                             */
/* -------------------------------------------------------------------- */
      if(poTIFFDirEntry->tdir_tag == EXIFOFFSETTAG)
	nExifOffset=poTIFFDirEntry->tdir_offset;
      if(poTIFFDirEntry->tdir_tag == INTEROPERABILITYOFFSET)
	nInterOffset=poTIFFDirEntry->tdir_offset;

/* -------------------------------------------------------------------- */
/*      Print tags                                                      */
/* -------------------------------------------------------------------- */
      space = poTIFFDirEntry->tdir_count * 
	datawidth[poTIFFDirEntry->tdir_type];

/* -------------------------------------------------------------------- */
/*      This is at most 4 byte data so we can read it from tdir_offset  */
/* -------------------------------------------------------------------- */
      if (space >= 0 && space <= 4) {
	switch (poTIFFDirEntry->tdir_type) {
	case TIFF_FLOAT:
	case TIFF_UNDEFINED:
	case TIFF_ASCII: {
	  unsigned char data[4];
	  memcpy(data, &poTIFFDirEntry->tdir_offset, 4);
	  if (bSwabflag)
	    TIFFSwabLong((uint32*) data);

	  EXIFPrintData(pszTemp,
			poTIFFDirEntry->tdir_type, 
			poTIFFDirEntry->tdir_count, data);
	  break;
	}

	case TIFF_BYTE:
	  EXIFPrintByte(pszTemp, "%s%#02x", poTIFFDirEntry);
	  break;
	case TIFF_SBYTE:
	  EXIFPrintByte(pszTemp, "%s%d", poTIFFDirEntry);
	  break;
	case TIFF_SHORT:
	  EXIFPrintShort(pszTemp, "%s%u", poTIFFDirEntry);
	  break;
	case TIFF_SSHORT:
	  EXIFPrintShort(pszTemp, "%s%d", poTIFFDirEntry);
	  break;
	case TIFF_LONG:
	  sprintf(pszTemp, "%lu",(long) poTIFFDirEntry->tdir_offset);	
	  break;
	case TIFF_SLONG:
	  sprintf(pszTemp, "%lu",(long) poTIFFDirEntry->tdir_offset);
	  break;
	}
	
      }
/* -------------------------------------------------------------------- */
/*      The data is being read where tdir_offset point to in the file   */
/* -------------------------------------------------------------------- */
      else {
	unsigned char *data = (unsigned char *)CPLMalloc(space);
	if (data) {
	  int width = TIFFDataWidth((TIFFDataType) poTIFFDirEntry->tdir_type);
	  tsize_t cc = poTIFFDirEntry->tdir_count * width;
	  VSIFSeek(fp,poTIFFDirEntry->tdir_offset+TIFFHEADER,SEEK_SET);
	  VSIFRead(data, 1, cc, fp);

	  if (bSwabflag) {
	    switch (poTIFFDir->tdir_type) {
	    case TIFF_SHORT:
	    case TIFF_SSHORT:
	      TIFFSwabArrayOfShort((uint16*) data, poTIFFDir->tdir_count);
	      break;
	    case TIFF_LONG:
	    case TIFF_SLONG:
	    case TIFF_FLOAT:
	      TIFFSwabArrayOfLong((uint32*) data, poTIFFDir->tdir_count);
	      break;
	    case TIFF_RATIONAL:
	    case TIFF_SRATIONAL:
	      TIFFSwabArrayOfLong((uint32*) data, 2*poTIFFDir->tdir_count);
	      break;
	    case TIFF_DOUBLE:
	      TIFFSwabArrayOfDouble((double*) data, poTIFFDir->tdir_count);
	      break;
	    }
	  }

	  EXIFPrintData(pszTemp, poTIFFDirEntry->tdir_type,
			poTIFFDirEntry->tdir_count, data);
	  if (data) CPLFree(data);
	}
      }
      papszMetadata = CSLSetNameValue(papszMetadata, pszName, pszTemp);
  }

  return CE_None;
}

/************************************************************************/
/*                           JPGRasterBand()                            */
/************************************************************************/

JPGRasterBand::JPGRasterBand( JPGDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    if( poDS->sDInfo.data_precision == 12 )
        eDataType = GDT_UInt16;
    else
        eDataType = GDT_Byte;

    nBlockXSize = poDS->nRasterXSize;;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    JPGDataset	*poGDS = (JPGDataset *) poDS;
    CPLErr      eErr;
    int         nXSize = GetXSize();
    int         nWordSize = GDALGetDataTypeSize(eDataType) / 8;
    
    CPLAssert( nBlockXOff == 0 );

/* -------------------------------------------------------------------- */
/*      Load the desired scanline into the working buffer.              */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadScanline( nBlockYOff );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Transfer between the working buffer the the callers buffer.     */
/* -------------------------------------------------------------------- */
    if( poGDS->GetRasterCount() == 1 )
    {
#ifdef JPEG_LIB_MK1
        GDALCopyWords( poGDS->pabyScanline, GDT_UInt16, 2, 
                       pImage, eDataType, nWordSize, 
                       nXSize );
#else
        memcpy( pImage, poGDS->pabyScanline, nXSize * nWordSize );
#endif
    }
    else
    {
#ifdef JPEG_LIB_MK1
        GDALCopyWords( poGDS->pabyScanline + (nBand-1) * 2, 
                       GDT_UInt16, 6, 
                       pImage, eDataType, nWordSize, 
                       nXSize );
#else
        GDALCopyWords( poGDS->pabyScanline + (nBand-1) * nWordSize, 
                       eDataType, nWordSize * 3, 
                       pImage, eDataType, nWordSize, 
                       nXSize );
#endif
    }

/* -------------------------------------------------------------------- */
/*      Forceably load the other bands associated with this scanline.   */
/* -------------------------------------------------------------------- */
    if( poGDS->GetRasterCount() == 3 && nBand == 1 )
    {
        GDALRasterBlock *poBlock;

        poBlock = 
            poGDS->GetRasterBand(2)->GetLockedBlockRef(nBlockXOff,nBlockYOff);
        poBlock->DropLock();

        poBlock = 
            poGDS->GetRasterBand(3)->GetLockedBlockRef(nBlockXOff,nBlockYOff);
        poBlock->DropLock();
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JPGRasterBand::GetColorInterpretation()

{
    JPGDataset	*poGDS = (JPGDataset *) poDS;

    if( poGDS->nBands == 1 )
        return GCI_GrayIndex;

    else if( nBand == 1 )
        return GCI_RedBand;

    else if( nBand == 2 )
        return GCI_GreenBand;

    else 
        return GCI_BlueBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             JPGDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            JPGDataset()                              */
/************************************************************************/

JPGDataset::JPGDataset()

{
    pabyScanline = NULL;
    nLoadedScanline = -1;

    papszMetadata   = NULL;						
    papszSubDatasets= NULL;
    nExifOffset     = -1;
    nInterOffset    = -1;
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~JPGDataset()                            */
/************************************************************************/

JPGDataset::~JPGDataset()

{
    FlushCache();

    jpeg_abort_decompress( &sDInfo );
    jpeg_destroy_decompress( &sDInfo );

    if( fpImage != NULL )
        VSIFClose( fpImage );

    if( pabyScanline != NULL )
        CPLFree( pabyScanline );
    if( papszMetadata != NULL )
      CSLDestroy( papszMetadata );
}
/************************************************************************/
/*                            LoadScanline()                            */
/************************************************************************/

CPLErr JPGDataset::LoadScanline( int iLine )

{
    if( nLoadedScanline == iLine )
        return CE_None;

    if( pabyScanline == NULL )
        pabyScanline = (GByte *)
            CPLMalloc(GetRasterCount() * GetRasterXSize() * 2);

    if( iLine < nLoadedScanline )
        Restart();
        
    while( nLoadedScanline < iLine )
    {
        JSAMPLE	*ppSamples;
            
        ppSamples = (JSAMPLE *) pabyScanline;
        jpeg_read_scanlines( &sDInfo, &ppSamples, 1 );
        nLoadedScanline++;
    }

    return CE_None;
}

/************************************************************************/
/*                              Restart()                               */
/*                                                                      */
/*      Restart compressor at the beginning of the file.                */
/************************************************************************/

void JPGDataset::Restart()

{
    jpeg_abort_decompress( &sDInfo );
    jpeg_destroy_decompress( &sDInfo );
    jpeg_create_decompress( &sDInfo );

    VSIRewind( fpImage );

    jpeg_stdio_src( &sDInfo, fpImage );
    jpeg_read_header( &sDInfo, TRUE );
    
    if( GetRasterCount() == 1 )
        sDInfo.out_color_space = JCS_GRAYSCALE;
    else
        sDInfo.out_color_space = JCS_RGB;
    nLoadedScanline = -1;
    jpeg_start_decompress( &sDInfo );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JPGDataset::GetGeoTransform( double * padfTransform )

{
    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
        
        return CE_None;
    }
    else 
        return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JPGDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 10 )
        return NULL;

    if( poOpenInfo->pabyHeader[0] != 0xff
        || poOpenInfo->pabyHeader[1] != 0xd8
        || poOpenInfo->pabyHeader[2] != 0xff )
        return NULL;

    /* Some files lack the JFIF marker, like IMG_0519.JPG. For these we
       require the .jpg extension */
    if(   !((poOpenInfo->pabyHeader[3] == 0xe0
	   && poOpenInfo->pabyHeader[6] == 'J'
	   && poOpenInfo->pabyHeader[7] == 'F'
	   && poOpenInfo->pabyHeader[8] == 'I'
	   && poOpenInfo->pabyHeader[9] == 'F')
	||(poOpenInfo->pabyHeader[3] == 0xe1
	   && poOpenInfo->pabyHeader[6] == 'E'
	   && poOpenInfo->pabyHeader[7] == 'x'
	   && poOpenInfo->pabyHeader[8] == 'i'
	   && poOpenInfo->pabyHeader[9] == 'f')
        && EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"jpg")) )
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The JPEG driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JPGDataset 	*poDS;

    poDS = new JPGDataset();

/* -------------------------------------------------------------------- */
/*      Take care of EXIF Metadata                                      */
/* -------------------------------------------------------------------- */
    if (poOpenInfo->pabyHeader [3] == 0xe1) {
      poDS->EXIFInit(poOpenInfo->fp);

      poDS->EXIFExtractMetadata(poOpenInfo->fp,poDS->nTiffDirStart);

      if(poDS->nExifOffset  > 0){ 
      	poDS->EXIFExtractMetadata(poOpenInfo->fp,poDS->nExifOffset);
      }
      if(poDS->nInterOffset > 0) {
      	poDS->EXIFExtractMetadata(poOpenInfo->fp,poDS->nInterOffset);
      }
      poDS->SetMetadata( poDS->papszMetadata );
    }

    poDS->eAccess = GA_ReadOnly;

    poDS->sDInfo.err = jpeg_std_error( &(poDS->sJErr) );

    jpeg_create_decompress( &(poDS->sDInfo) );

/* -------------------------------------------------------------------- */
/*	Read pre-image data after ensuring the file is rewound.         */
/* -------------------------------------------------------------------- */
    VSIRewind( poOpenInfo->fp );

    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

    jpeg_stdio_src( &(poDS->sDInfo), poDS->fpImage );
    jpeg_read_header( &(poDS->sDInfo), TRUE );

    if( poDS->sDInfo.data_precision != 8
        && poDS->sDInfo.data_precision != 12 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "GDAL JPEG Driver doesn't support files with precision of"
                  " other than 8 or 12 bits." );
        delete poDS;
        return NULL;
    }

    jpeg_start_decompress( &(poDS->sDInfo) );

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poDS->sDInfo.image_width;
    poDS->nRasterYSize = poDS->sDInfo.image_height;

    if( poDS->sDInfo.jpeg_color_space == JCS_GRAYSCALE )
    {
        poDS->nBands = 1;
        poDS->sDInfo.out_color_space = JCS_GRAYSCALE;
    }
    else if( poDS->sDInfo.jpeg_color_space == JCS_RGB 
             || poDS->sDInfo.jpeg_color_space == JCS_YCbCr )
    {
        poDS->nBands = 3;
        poDS->sDInfo.out_color_space = JCS_RGB;
    }
    else
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Unrecognised jpeg_color_space value of %d.\n", 
                  poDS->sDInfo.jpeg_color_space );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
        poDS->SetBand( iBand+1, new JPGRasterBand( poDS, iBand+1 ) );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    poDS->bGeoTransformValid = 
        GDALReadWorldFile( poOpenInfo->pszFilename, ".jgw", 
                           poDS->adfGeoTransform )
        || GDALReadWorldFile( poOpenInfo->pszFilename, ".jpgw", 
                              poDS->adfGeoTransform )
        || GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                              poDS->adfGeoTransform );

    return poDS;
}

/************************************************************************/
/*                           JPEGCreateCopy()                           */
/************************************************************************/

static GDALDataset *
JPEGCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                int bStrict, char ** papszOptions, 
                GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    int  anBandList[3] = {1,2,3};
    int  nQuality = 75;
    int  bProgressive = FALSE;

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "JPEG driver doesn't support %d bands.  Must be 1 (grey) "
                  "or 3 (RGB) bands.\n", nBands );

        return NULL;
    }

    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();

#ifdef JPEG_LIB_MK1
    if( eDT != GDT_Byte && eDT != GDT_UInt16 && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "JPEG driver doesn't support data type %s. "
                  "Only eight and twelve bit bands supported (Mk1 libjpeg).\n",
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

    if( eDT == GDT_UInt16 || eDT == GDT_Int16 )
        eDT = GDT_UInt16;
    else
        eDT = GDT_Byte;

#else
    if( eDT != GDT_Byte && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "JPEG driver doesn't support data type %s. "
                  "Only eight bit byte bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }
    
    eDT = GDT_Byte; // force to 8bit. 
#endif

/* -------------------------------------------------------------------- */
/*      What options has the user selected?                             */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszOptions,"QUALITY") != NULL )
    {
        nQuality = atoi(CSLFetchNameValue(papszOptions,"QUALITY"));
        if( nQuality < 10 || nQuality > 100 )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "QUALITY=%s is not a legal value in the range 10-100.",
                      CSLFetchNameValue(papszOptions,"QUALITY") );
            return NULL;
        }
    }

    bProgressive = CSLFetchBoolean( papszOptions, "PROGRESSIVE", FALSE );

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    FILE	*fpImage;

    fpImage = VSIFOpen( pszFilename, "wb" );
    if( fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create jpeg file %s.\n", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize JPG access to the file.                              */
/* -------------------------------------------------------------------- */
    struct jpeg_compress_struct sCInfo;
    struct jpeg_error_mgr sJErr;
    
    sCInfo.err = jpeg_std_error( &sJErr );
    jpeg_create_compress( &sCInfo );
    
    jpeg_stdio_dest( &sCInfo, fpImage );
    
    sCInfo.image_width = nXSize;
    sCInfo.image_height = nYSize;
    sCInfo.input_components = nBands;

    if( nBands == 1 )
    {
        sCInfo.in_color_space = JCS_GRAYSCALE;
    }
    else
    {
        sCInfo.in_color_space = JCS_RGB;
    }

    jpeg_set_defaults( &sCInfo );
    
#ifdef JPEG_LIB_MK1
    if( eDT == GDT_UInt16 )
    {
        sCInfo.data_precision = 12;
        sCInfo.bits_in_jsample = 12;
    }
    else
    {
        sCInfo.data_precision = 8;
        sCInfo.bits_in_jsample = 8;
    }
#endif

    jpeg_set_quality( &sCInfo, nQuality, TRUE );

    if( bProgressive )
        jpeg_simple_progression( &sCInfo );

    jpeg_start_compress( &sCInfo, TRUE );

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GByte 	*pabyScanline;
    CPLErr      eErr = CE_None;

    pabyScanline = (GByte *) CPLMalloc( nBands * nXSize * 2 );

    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
    {
        JSAMPLE      *ppSamples;

#ifdef JPEG_LIB_MK1
        eErr = poSrcDS->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                  pabyScanline, nXSize, 1, GDT_UInt16,
                                  nBands, anBandList, 
                                  nBands*2, nBands * nXSize * 2, 2 );
#else
        eErr = poSrcDS->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                  pabyScanline, nXSize, 1, GDT_Byte,
                                  nBands, anBandList, 
                                  nBands, nBands * nXSize, 1 );
#endif

        // Should we clip values over 4095 (12bit)? 

        ppSamples = (JSAMPLE *) pabyScanline;

        if( eErr == CE_None )
            jpeg_write_scanlines( &sCInfo, &ppSamples, 1 );
    }

    CPLFree( pabyScanline );

    jpeg_finish_compress( &sCInfo );
    jpeg_destroy_compress( &sCInfo );

    VSIFClose( fpImage );

/* -------------------------------------------------------------------- */
/*      Do we need a world file?                                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "WORLDFILE", FALSE ) )
    {
    	double      adfGeoTransform[6];
	
	poSrcDS->GetGeoTransform( adfGeoTransform );
	GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    JPGDataset *poDS = (JPGDataset *) GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_JPEG()                          */
/************************************************************************/

void GDALRegister_JPEG()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "JPEG" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "JPEG" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "JPEG JFIF/Exif" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_jpeg.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jpg" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jpeg" );

#ifdef JPEG_LIB_MK1
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16" );
#else
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte" );
#endif
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>\n"
"   <Option name='PROGRESSIVE' type='boolean'/>\n"
"   <Option name='QUALITY' type='int' description='good=100, bad=0, default=75'/>\n"
"   <Option name='WORLDFILE' type='boolean'/>\n"
"</CreationOptionList>\n" );

        poDriver->pfnOpen = JPGDataset::Open;
        poDriver->pfnCreateCopy = JPEGCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

