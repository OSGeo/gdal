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
 * Revision 1.41  2006/03/21 19:46:37  fwarmerdam
 * Avoid calling jpeg_finish_compress() if there was an error or
 * libjpeg is likely to issue an error and call exit()!
 *
 * Revision 1.40  2006/02/26 14:32:32  fwarmerdam
 * Added accelerated dataset rasterio for common case c/o Mike Mazzella.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1046
 *
 * Revision 1.39  2006/02/02 01:07:36  fwarmerdam
 * Added support for finding EXIF information when APP1 chunk appears
 * after an APP0 JFIF chunk.  ie. albania.jpg
 *
 * Revision 1.38  2005/10/14 21:52:26  fwarmerdam
 * Slightly safer version of last fix, retaining ASCII setting for ucomments.
 *
 * Revision 1.37  2005/10/14 21:50:03  fwarmerdam
 * Don't flip out of usercomment is less than 8 characters.
 *
 * Revision 1.36  2005/09/26 14:52:57  fwarmerdam
 * Fixed UserComments to skip language encoding.  Force some new tags
 * to ASCII.
 *
 * Revision 1.35  2005/09/23 20:01:56  fwarmerdam
 * Fixed progress reporting.
 *
 * Revision 1.34  2005/09/22 14:59:47  fwarmerdam
 * Fixed bug with unrecognised exif tags sometimes being written over
 * metadata of a previous tag.
 * Hack so that UserComments is treated as ASCII.
 *
 * Revision 1.33  2005/09/11 17:15:33  fwarmerdam
 * direct io through VSI, use large file API
 *
 * Revision 1.32  2005/09/11 16:36:54  fwarmerdam
 * Turn raw EXIF printf() into a debug statement.
 *
 * Revision 1.31  2005/09/05 22:37:52  fwarmerdam
 * Added progress monitor support in createcopy.
 *
 * Revision 1.30  2005/08/10 20:53:24  dnadeau
 * correct GPS and logic problems for EXIF
 *
 * Revision 1.29  2005/08/09 20:19:19  dnadeau
 * add EXIF GPS IFD Tags
 *
 * Revision 1.28  2005/07/27 02:00:39  dnadeau
 * correct leak problem. Free poTIFFDir.
 *
 * Revision 1.27  2005/07/19 19:36:15  fwarmerdam
 * fixed libtiff conflicts
 *
 * Revision 1.26  2005/07/19 18:06:28  dnadeau
 * fix loop problem finding tags
 *
 * Revision 1.25  2005/07/19 15:38:33  fwarmerdam
 * Disable a few less interesting tags.
 *
 * Revision 1.24  2005/07/19 15:33:53  fwarmerdam
 * Removed /Exif in longname.
 *
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
#include "gdalexif.h"


CPL_CVSID("$Id$");

CPL_C_START
#include "jpeglib.h"
CPL_C_END

CPL_C_START
void	GDALRegister_JPEG(void);
CPL_C_END

void jpeg_vsiio_src (j_decompress_ptr cinfo, FILE * infile);
void jpeg_vsiio_dest (j_compress_ptr cinfo, FILE * outfile);

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
    int    nGPSOffset;
    int	   bSwabflag;
    int    nTiffDirStart;
    int    nTIFFHEADER;

    CPLErr LoadScanline(int);
    void   Restart();
    
    CPLErr EXIFExtractMetadata(FILE *, int);
    int    EXIFInit(FILE *);
    void   EXIFPrintByte(char *, const char*, TIFFDirEntry* );
    void   EXIFPrintShort(char *, const char*, TIFFDirEntry*);
    void   EXIFPrintData(char *, GUInt16, GUInt32, unsigned char* );

    

  public:
                 JPGDataset();
                 ~JPGDataset();

    virtual CPLErr      IRasterIO( GDALRWFlag, int, int, int, int,
                                   void *, int, int, GDALDataType,
                                   int, int *, int, int, int );

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
void JPGDataset::EXIFPrintData(char* pszData, GUInt16 type, 
			    GUInt32 count, unsigned char* data)
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
    register GUInt16 *wp = (GUInt16*)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s%u", sep, *wp++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_SSHORT: {
    register GInt16 *wp = (GInt16*)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s%d", sep, *wp++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_LONG: {
    register GUInt32 *lp = (GUInt32*)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s%lu", sep, (unsigned long) *lp++);
      sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_SLONG: {
    register GInt32 *lp = (GInt32*)data;
    while (count-- > 0){
      sprintf(pszTemp, "%s%ld", sep, (long) *lp++), sep = " ";
      strcat(pszData,pszTemp);
    }
    break;
  }
  case TIFF_RATIONAL: {
      register GUInt32 *lp = (GUInt32*)data;
      //      if(bSwabflag)
      //	  TIFFSwabArrayOfLong((GUInt32*) data, 2*count);
      while (count-- > 0) {
	  if( (lp[0]==0) && (lp[1] == 0) ) {
	      sprintf(pszTemp,"%s(0)",sep);
	  }
	  else{
	      sprintf(pszTemp, "%s(%g)", sep,
		      (double) lp[0]/ (double)lp[1]);
	  }
	  sep = " ";
	  lp += 2;
	  strcat(pszData,pszTemp);
      }
      break;
  }
  case TIFF_SRATIONAL: {
    register GInt32 *lp = (GInt32*)data;
    while (count-- > 0) {
      sprintf(pszTemp, "%s(%g)", sep,
	      (float) lp[0]/ (float) lp[1]);
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
/*                        EXIFInit()                                    */
/*                                                                      */
/*           Create Metadata from Information file directory APP1       */
/************************************************************************/
int JPGDataset::EXIFInit(FILE *fp)
{
    int           one = 1;
    TIFFHeader    hdr;
  
    bigendian = (*(char *)&one == 0);

/* -------------------------------------------------------------------- */
/*      Search for APP1 chunk.                                          */
/* -------------------------------------------------------------------- */
    GByte abyChunkHeader[10];
    int nChunkLoc = 2;

    for( ; TRUE; ) 
    {
        if( VSIFSeekL( fp, nChunkLoc, SEEK_SET ) != 0 )
            return FALSE;

        if( VSIFReadL( abyChunkHeader, sizeof(abyChunkHeader), 1, fp ) != 1 )
            return FALSE;

        if( abyChunkHeader[0] != 0xFF 
            || (abyChunkHeader[1] & 0xf0) != 0xe0 )
            return FALSE; // Not an APP chunk.

        if( abyChunkHeader[1] == 0xe1 
            && strncmp((const char *) abyChunkHeader + 4,"Exif",4) == 0 )
        {
            nTIFFHEADER = nChunkLoc + 10;
            break; // APP1 - Exif
        }

        nChunkLoc += 2 + abyChunkHeader[2] * 256 + abyChunkHeader[3];
    }

/* -------------------------------------------------------------------- */
/*      Read TIFF header                                                */
/* -------------------------------------------------------------------- */
    VSIFSeekL(fp, nTIFFHEADER, SEEK_SET);
    if(VSIFReadL(&hdr,1,sizeof(hdr),fp) != sizeof(hdr)) 
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

    CPLDebug( "JPEG", "Magic: %#x <%s-endian> Version: %#x\n",
              hdr.tiff_magic,
              hdr.tiff_magic == TIFF_BIGENDIAN ? "big" : "little",
              hdr.tiff_version );

    return TRUE;
}

/************************************************************************/
/*                        EXIFExtractMetadata()                         */
/*                                                                      */
/*      Extract all entry from a IFD                                    */
/************************************************************************/
CPLErr JPGDataset::EXIFExtractMetadata(FILE *fp, int nOffset)
{
    GUInt16        nEntryCount;
    int space;
    unsigned int           n,i;
    char          pszTemp[MAXSTRINGLENGTH];
    char          pszName[MAXSTRINGLENGTH];

    TIFFDirEntry *poTIFFDirEntry;
    TIFFDirEntry *poTIFFDir;
    struct tagname *poExifTags ;
    struct intr_tag *poInterTags = intr_tags;
    struct gpsname *poGPSTags;

/* -------------------------------------------------------------------- */
/*      Read number of entry in directory                               */
/* -------------------------------------------------------------------- */
    VSIFSeekL(fp, nOffset+nTIFFHEADER, SEEK_SET);

    if(VSIFReadL(&nEntryCount,1,sizeof(GUInt16),fp) != sizeof(GUInt16)) 
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
    n = VSIFReadL(poTIFFDir, 1,nEntryCount*sizeof(TIFFDirEntry),fp);
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
        pszName[0] = '\0';

        for (poExifTags = tagnames; poExifTags->tag; poExifTags++)
            if(poExifTags->tag == poTIFFDirEntry->tdir_tag){
                strcpy(pszName, poExifTags->name);
                break;
            }

    
        if( nOffset == nGPSOffset) {
            for( poGPSTags = gpstags; poGPSTags->tag != 0xffff; poGPSTags++ ) 
                if( poGPSTags->tag == poTIFFDirEntry->tdir_tag ) {
                    strcpy(pszName, poGPSTags->name);
                    break;
                }
        }
/* -------------------------------------------------------------------- */
/*      If the tag was not found, look into the interoperability table  */
/* -------------------------------------------------------------------- */
        if( nOffset == nInterOffset ) {
            for(poInterTags = intr_tags; poInterTags->tag; poInterTags++)
                if(poInterTags->tag == poTIFFDirEntry->tdir_tag) {
                    strcpy(pszName, poInterTags->name);
                    break;
                }
        }

/* -------------------------------------------------------------------- */
/*      Save important directory tag offset                             */
/* -------------------------------------------------------------------- */
        if( poTIFFDirEntry->tdir_tag == EXIFOFFSETTAG )
            nExifOffset=poTIFFDirEntry->tdir_offset;
        if( poTIFFDirEntry->tdir_tag == INTEROPERABILITYOFFSET )
            nInterOffset=poTIFFDirEntry->tdir_offset;
        if( poTIFFDirEntry->tdir_tag == GPSOFFSETTAG ) {
            nGPSOffset=poTIFFDirEntry->tdir_offset;
        }

/* -------------------------------------------------------------------- */
/*      If we didn't recognise the tag just ignore it.  To see all      */
/*      tags comment out the continue.                                  */
/* -------------------------------------------------------------------- */
        if( pszName[0] == '\0' )
        {
            sprintf( pszName, "EXIF_%d", poTIFFDirEntry->tdir_tag );
            continue;
        }

/* -------------------------------------------------------------------- */
/*      For UserComment we need to ignore the language binding and      */
/*      just return the actual contents.                                */
/* -------------------------------------------------------------------- */
        if( EQUAL(pszName,"EXIF_UserComment")  )
        {
            poTIFFDirEntry->tdir_type = TIFF_ASCII;
            
            if( poTIFFDirEntry->tdir_count >= 8 )
            {
                poTIFFDirEntry->tdir_count -= 8;
                poTIFFDirEntry->tdir_offset += 8;
            }
        }

/* -------------------------------------------------------------------- */
/*      Make some UNDEFINED or BYTE fields ASCII for readability.       */
/* -------------------------------------------------------------------- */
        if( EQUAL(pszName,"EXIF_ExifVersion")
            || EQUAL(pszName,"EXIF_FlashPixVersion")
            || EQUAL(pszName,"EXIF_MakerNote")
            || EQUAL(pszName,"GPSProcessingMethod") )
            poTIFFDirEntry->tdir_type = TIFF_ASCII;

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
                      TIFFSwabLong((GUInt32*) data);

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
                VSIFSeekL(fp,poTIFFDirEntry->tdir_offset+nTIFFHEADER,SEEK_SET);
                VSIFReadL(data, 1, cc, fp);

                if (bSwabflag) {
                    switch (poTIFFDirEntry->tdir_type) {
                      case TIFF_SHORT:
                      case TIFF_SSHORT:
                        TIFFSwabArrayOfShort((GUInt16*) data, 
                                             poTIFFDirEntry->tdir_count);
                        break;
                      case TIFF_LONG:
                      case TIFF_SLONG:
                      case TIFF_FLOAT:
                        TIFFSwabArrayOfLong((GUInt32*) data, 
                                            poTIFFDirEntry->tdir_count);
                        break;
                      case TIFF_RATIONAL:
                      case TIFF_SRATIONAL:
                        TIFFSwabArrayOfLong((GUInt32*) data, 
                                            2*poTIFFDirEntry->tdir_count);
                        break;
                      case TIFF_DOUBLE:
                        TIFFSwabArrayOfDouble((double*) data, 
                                              poTIFFDirEntry->tdir_count);
                        break;
                      default:
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
    CPLFree(poTIFFDir);

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
    nGPSOffset      = -1;
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
        VSIFCloseL( fpImage );

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

    VSIRewindL( fpImage );

    jpeg_vsiio_src( &sDInfo, fpImage );
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
/*                             IRasterIO()                              */
/*                                                                      */
/*      Checks for what might be the most common read case              */
/*      (reading an entire interleaved, 8bit, RGB JPEG), and            */
/*      optimizes for that case                                         */
/************************************************************************/

CPLErr JPGDataset::IRasterIO( GDALRWFlag eRWFlag, 
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize, 
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap, 
                              int nPixelSpace, int nLineSpace, int nBandSpace )

{
    if((eRWFlag == GF_Read) &&
       (nBandCount == 3) &&
       (nBands == 3) &&
       (nXOff == 0) && (nXOff == 0) &&
       (nXSize == nBufXSize) && (nXSize == nRasterXSize) &&
       (nYSize == nBufYSize) && (nYSize == nRasterYSize) &&
       (eBufType == GDT_Byte) && (sDInfo.data_precision != 12) &&
       /*(nPixelSpace >= 3)*/(nPixelSpace > 3) &&
       (nLineSpace == (nPixelSpace*nXSize)) &&
       (nBandSpace == 1) &&
       (pData != NULL) &&
       (panBandMap != NULL) &&
       (panBandMap[0] == 1) && (panBandMap[1] == 2) && (panBandMap[2] == 3))
    {
        Restart();
        int y;
        CPLErr tmpError;
        int x;

        // handles copy with padding case
        for(y = 0; y < nYSize; ++y)
        {
            tmpError = LoadScanline(y);
            if(tmpError != CE_None) return tmpError;

            for(x = 0; x < nXSize; ++x)
            {
                tmpError = LoadScanline(y);
                if(tmpError != CE_None) return tmpError;
                memcpy(&(((GByte*)pData)[(y*nLineSpace) + (x*nPixelSpace)]), 
                       (const GByte*)&(pabyScanline[x*3]), 3);
            }
        }

        return CE_None;
    }

    return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType, 
                                     nBandCount, panBandMap, 
                                     nPixelSpace, nLineSpace, nBandSpace);
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

    if( poOpenInfo->pabyHeader[3] == 0xe0
        && poOpenInfo->pabyHeader[6] == 'J'
        && poOpenInfo->pabyHeader[7] == 'F'
        && poOpenInfo->pabyHeader[8] == 'I'
        && poOpenInfo->pabyHeader[9] == 'F' )
        /* OK */;
    else if( poOpenInfo->pabyHeader[3] == 0xe1
             && poOpenInfo->pabyHeader[6] == 'E'
             && poOpenInfo->pabyHeader[7] == 'x'
             && poOpenInfo->pabyHeader[8] == 'i'
             && poOpenInfo->pabyHeader[9] == 'f' )
        /* OK */;
    else
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
/*      Open the file using the large file api.                         */
/* -------------------------------------------------------------------- */
    poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "VSIFOpenL(%s) failed unexpectedly in jpgdataset.cpp", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Take care of EXIF Metadata                                      */
/* -------------------------------------------------------------------- */
    if( poDS->EXIFInit(poDS->fpImage) )
    {
        poDS->EXIFExtractMetadata(poDS->fpImage,poDS->nTiffDirStart);

        if(poDS->nExifOffset  > 0){ 
            poDS->EXIFExtractMetadata(poDS->fpImage,poDS->nExifOffset);
        }
        if(poDS->nInterOffset > 0) {
            poDS->EXIFExtractMetadata(poDS->fpImage,poDS->nInterOffset);
        }
        if(poDS->nGPSOffset > 0) {
            poDS->EXIFExtractMetadata(poDS->fpImage,poDS->nGPSOffset);
        }
        poDS->SetMetadata( poDS->papszMetadata );
    }

    poDS->eAccess = GA_ReadOnly;

    poDS->sDInfo.err = jpeg_std_error( &(poDS->sJErr) );

    jpeg_create_decompress( &(poDS->sDInfo) );

/* -------------------------------------------------------------------- */
/*	Read pre-image data after ensuring the file is rewound.         */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fpImage, 0, SEEK_SET );

    jpeg_vsiio_src( &(poDS->sDInfo), poDS->fpImage );
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

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

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

    fpImage = VSIFOpenL( pszFilename, "wb" );
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
    
    jpeg_vsiio_dest( &sCInfo, fpImage );
    
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

        if( eErr == CE_None 
            && !pfnProgress( (iLine+1) / (double) nYSize,
                             NULL, pProgressData ) )
        {
            eErr = CE_Failure;
            CPLError( CE_Failure, CPLE_UserInterrupt, 
                      "User terminated CreateCopy()" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and close.                                              */
/* -------------------------------------------------------------------- */
    CPLFree( pabyScanline );

    if( eErr == CE_None )
        jpeg_finish_compress( &sCInfo );
    jpeg_destroy_compress( &sCInfo );

    VSIFCloseL( fpImage );

    if( eErr != CE_None )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }

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
                                   "JPEG JFIF" );
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

