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
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
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

#include "gdal_pam.h"
#include "cpl_string.h"
#include "gdalexif.h"

#include <setjmp.h>


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
    jmp_buf setjmp_buffer;

    int	   bGeoTransformValid;
    double adfGeoTransform[6];

    FILE   *fpImage;
    int    nSubfileOffset;

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

    int    nQLevel;
    void   LoadDefaultTables(int);

    static void ErrorExit(j_common_ptr cinfo);
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

    JPGDataset 	   *poGDS;

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
    this->poDS = poGDS = poDS;

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

    // setup to trap a fatal error.
    if (setjmp(setjmp_buffer)) 
        return CE_Failure;

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
/*                         LoadDefaultTables()                          */
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


void JPGDataset::LoadDefaultTables( int n )

{
    if( nQLevel < 1 )
        return;

/* -------------------------------------------------------------------- */
/*      Load quantization table						*/
/* -------------------------------------------------------------------- */
    int i;
    JQUANT_TBL  *quant_ptr;
    const int *panQTable;

    if( nQLevel == 1 )
        panQTable = Q1table;
    else if( nQLevel == 2 )
        panQTable = Q2table;
    else if( nQLevel == 3 )
        panQTable = Q3table;
    else if( nQLevel == 4 )
        panQTable = Q4table;
    else if( nQLevel == 5 )
        panQTable = Q5table;
    else
        return;

    if (sDInfo.quant_tbl_ptrs[n] == NULL)
        sDInfo.quant_tbl_ptrs[n] = 
            jpeg_alloc_quant_table((j_common_ptr) &(sDInfo));
    
    quant_ptr = sDInfo.quant_tbl_ptrs[n];	/* quant_ptr is JQUANT_TBL* */
    for (i = 0; i < 64; i++) {
        /* Qtable[] is desired quantization table, in natural array order */
        quant_ptr->quantval[i] = panQTable[i];
    }

/* -------------------------------------------------------------------- */
/*      Load AC huffman table.                                          */
/* -------------------------------------------------------------------- */
    JHUFF_TBL  *huff_ptr;

    if (sDInfo.ac_huff_tbl_ptrs[n] == NULL)
        sDInfo.ac_huff_tbl_ptrs[n] =
            jpeg_alloc_huff_table((j_common_ptr)&sDInfo);

    huff_ptr = sDInfo.ac_huff_tbl_ptrs[n];	/* huff_ptr is JHUFF_TBL* */

    for (i = 1; i <= 16; i++) {
        /* counts[i] is number of Huffman codes of length i bits, i=1..16 */
        huff_ptr->bits[i] = AC_BITS[i-1];
    }

    for (i = 0; i < 256; i++) {
        /* symbols[] is the list of Huffman symbols, in code-length order */
        huff_ptr->huffval[i] = AC_HUFFVAL[i];
    }

/* -------------------------------------------------------------------- */
/*      Load DC huffman table.                                          */
/* -------------------------------------------------------------------- */
    if (sDInfo.dc_huff_tbl_ptrs[n] == NULL)
        sDInfo.dc_huff_tbl_ptrs[n] =
            jpeg_alloc_huff_table((j_common_ptr)&sDInfo);

    huff_ptr = sDInfo.dc_huff_tbl_ptrs[n];	/* huff_ptr is JHUFF_TBL* */

    for (i = 1; i <= 16; i++) {
        /* counts[i] is number of Huffman codes of length i bits, i=1..16 */
        huff_ptr->bits[i] = DC_BITS[i-1];
    }

    for (i = 0; i < 256; i++) {
        /* symbols[] is the list of Huffman symbols, in code-length order */
        huff_ptr->huffval[i] = DC_HUFFVAL[i];
    }

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

    LoadDefaultTables( 0 );
    LoadDefaultTables( 1 );
    LoadDefaultTables( 2 );
    LoadDefaultTables( 3 );

/* -------------------------------------------------------------------- */
/*      restart io.                                                     */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fpImage, nSubfileOffset, SEEK_SET );

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
    int    bIsSubfile = FALSE;
    GByte  *pabyHeader = NULL;
    int    subfile_offset = 0, subfile_size;
    int    nHeaderBytes = poOpenInfo->nHeaderBytes;
    const char *real_filename = poOpenInfo->pszFilename;
    GByte abySubfileHeader[16];
    int nQLevel = -1;

/* -------------------------------------------------------------------- */
/*      If it is a subfile, read the JPEG header.                       */
/* -------------------------------------------------------------------- */
    if( ( poOpenInfo->fp == NULL ) &&
        ( EQUALN(poOpenInfo->pszFilename,"JPEG_SUBFILE:",13) ) )
    {
        /* static GByte abySubfileHeader[16]; */
        FILE *file;
        int bScan;

        if( EQUALN(poOpenInfo->pszFilename,"JPEG_SUBFILE:Q",14) )
            bScan = sscanf( poOpenInfo->pszFilename, "JPEG_SUBFILE:Q%d,%d,%d", 
                            &nQLevel, &subfile_offset, &subfile_size ) == 3;
        else
            bScan = sscanf( poOpenInfo->pszFilename, "JPEG_SUBFILE:%d,%d", 
                            &subfile_offset, &subfile_size ) == 2;

        if( !bScan ) 
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Corrupt subfile definition: %s", 
                      poOpenInfo->pszFilename );
            return NULL;
        }

        real_filename = strstr(poOpenInfo->pszFilename,",");
        if( real_filename != NULL )
            real_filename = strstr(real_filename+1,",");
        if( real_filename != NULL && nQLevel != -1 )
            real_filename = strstr(real_filename+1,",");
        if( real_filename != NULL )
            real_filename++;
        else
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Could not find filename in subfile definition.");
            return NULL;
        }

        CPLDebug( "JPG",
                  "real_filename %s, offset=%d, size=%d\n", 
                  real_filename, subfile_offset, subfile_size);

        file = VSIFOpenL( real_filename, "rb" );
        if( file == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to open compressed data file %s",
                      real_filename);
            return NULL;
        }

        /* seek to beginning of JPEG data */
        if( VSIFSeekL( file, subfile_offset, SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to seek in file %s",
                      real_filename);
            return NULL;
        }

        /* read header, then close file */
        nHeaderBytes = VSIFReadL(abySubfileHeader, 1, (size_t) 16, file);
        VSIFCloseL(file);

        pabyHeader = abySubfileHeader;

        bIsSubfile = TRUE;
    }
    else
    {
        pabyHeader = poOpenInfo->pabyHeader;
    }

/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( nHeaderBytes < 10 )
        return NULL;

    if( pabyHeader[0] != 0xff
        || pabyHeader[1] != 0xd8
        || pabyHeader[2] != 0xff )
        return NULL;

    int iChunkOff;

    for( iChunkOff = 2; 
         iChunkOff < nHeaderBytes-10; 
         iChunkOff += 10 ) 
    {
        if( pabyHeader[0+iChunkOff] != 0xff
            || (pabyHeader[1+iChunkOff] & 0xf0) < 0xe0 )
            return NULL;

        if( pabyHeader[1+iChunkOff] == 0xe0
            && pabyHeader[4+iChunkOff] == 'J'
            && pabyHeader[5+iChunkOff] == 'F'
            && pabyHeader[6+iChunkOff] == 'I'
            && pabyHeader[7+iChunkOff] == 'F' )
            break;
        else if( pabyHeader[1+iChunkOff] == 0xe1
                 && pabyHeader[4+iChunkOff] == 'E'
                 && pabyHeader[5+iChunkOff] == 'x'
                 && pabyHeader[6+iChunkOff] == 'i'
                 && pabyHeader[7+iChunkOff] == 'f' )
            break;
        else if( pabyHeader[1+iChunkOff] == 0xe6
                 && pabyHeader[4+iChunkOff] == 'N'
                 && pabyHeader[5+iChunkOff] == 'I'
                 && pabyHeader[6+iChunkOff] == 'T'
                 && pabyHeader[7+iChunkOff] == 'F' )
            break;
        else if( subfile_offset != 0 )
            /* ok - we don't require app segment on "subfiles" which 
               can be just part of a multi-image stream - ie. in nitf files */
            break;
        else if( CSLTestBoolean(
                     CPLGetConfigOption( "SIMPLE_JPEG_MAGIC", "NO" ) ) )
            /* Explicit request to allow non-app segment files */
            break;
    } /* next chunk */

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
    poDS->nQLevel = nQLevel;

/* -------------------------------------------------------------------- */
/*      Open the file using the large file api.                         */
/* -------------------------------------------------------------------- */
    poDS->fpImage = VSIFOpenL( real_filename, "rb" );
    
    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "VSIFOpenL(%s) failed unexpectedly in jpgdataset.cpp", 
                  real_filename );
        return NULL;
    }

    poDS->nSubfileOffset = subfile_offset;
    VSIFSeekL( poDS->fpImage, poDS->nSubfileOffset, SEEK_SET );

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
    poDS->sJErr.error_exit = JPGDataset::ErrorExit;
    poDS->sDInfo.client_data = (void *) poDS;

    jpeg_create_decompress( &(poDS->sDInfo) );

/* -------------------------------------------------------------------- */
/*      Preload default NITF JPEG quantization tables.                  */
/* -------------------------------------------------------------------- */
    poDS->LoadDefaultTables( 0 );
    poDS->LoadDefaultTables( 1 );
    poDS->LoadDefaultTables( 2 );
    poDS->LoadDefaultTables( 3 );

/* -------------------------------------------------------------------- */
/*      If a fatal error occurs after this, we will return NULL but     */
/*      not try to cleanup.  Cleaning up after a longjmp() can be       */
/*      pretty risky.                                                   */
/* -------------------------------------------------------------------- */
    if (setjmp(poDS->setjmp_buffer)) 
        return NULL;

/* -------------------------------------------------------------------- */
/*	Read pre-image data after ensuring the file is rewound.         */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fpImage, poDS->nSubfileOffset, SEEK_SET );

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
    
    if( !bIsSubfile )
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
/*                             ErrorExit()                              */
/************************************************************************/

void JPGDataset::ErrorExit(j_common_ptr cinfo)
{
    JPGDataset *poDS = (JPGDataset *) cinfo->client_data;
    char buffer[JMSG_LENGTH_MAX];

    /* Create the message */
    (*cinfo->err->format_message) (cinfo, buffer);

    CPLError( CE_Failure, CPLE_AppDefined,
              "libjpeg: %s", buffer );

    /* Return control to the setjmp point */
    longjmp(poDS->setjmp_buffer, 1);
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

