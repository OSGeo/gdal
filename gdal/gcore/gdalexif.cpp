/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Implements a EXIF directory reader
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

#include "gdal_priv.h"
#include "gdalexif.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         EXIFPrintData()                              */
/************************************************************************/
static void EXIFPrintData(char* pszData, GUInt16 type,
                   GUInt32 count, const unsigned char* data)
{
  const char* sep = "";
  char  szTemp[128];
  char* pszDataEnd = pszData;

  pszData[0]='\0';

  switch (type) {

  case TIFF_UNDEFINED:
  case TIFF_BYTE:
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%#02x", sep, *data++), sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;

  case TIFF_SBYTE:
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%d", sep, *(const char *)data++), sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;

  case TIFF_ASCII:
    memcpy( pszData, data, count );
    pszData[count] = '\0';
    break;

  case TIFF_SHORT: {
    const GUInt16 *wp = (const GUInt16*)data;
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%u", sep, *wp++), sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_SSHORT: {
    const GInt16 *wp = (const GInt16*)data;
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%d", sep, *wp++);
      sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_LONG: {
    const GUInt32 *lp = (const GUInt32*)data;
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%lu", sep, (unsigned long) *lp++);
      sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_SLONG: {
    const GInt32 *lp = (const GInt32*)data;
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%ld", sep, (long) *lp++), sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_RATIONAL: {
    const GUInt32 *lp = (const GUInt32*)data;
      //      if(bSwabflag)
      //      TIFFSwabArrayOfLong((GUInt32*) data, 2*count);
    for(;count>0;count--) {
      if( (lp[0]==0) && (lp[1] == 0) ) {
          snprintf(szTemp, sizeof(szTemp), "%s(0)",sep);
      }
      else{
          CPLsnprintf(szTemp, sizeof(szTemp), "%s(%g)", sep,
              (double) lp[0]/ (double)lp[1]);
      }
      sep = " ";
      lp += 2;
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_SRATIONAL: {
    const GInt32 *lp = (const GInt32*)data;
    for(;count>0;count--) {
      CPLsnprintf(szTemp, sizeof(szTemp), "%s(%g)", sep,
          (float) lp[0]/ (float) lp[1]);
      sep = " ";
      lp += 2;
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_FLOAT: {
    const float *fp = (const float *)data;
    for(;count>0;count--) {
      CPLsnprintf(szTemp, sizeof(szTemp), "%s%g", sep, *fp++), sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_DOUBLE: {
    const double *dp = (const double *)data;
    for(;count>0;count--) {
      CPLsnprintf(szTemp, sizeof(szTemp), "%s%g", sep, *dp++), sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }

  default:
    return;
  }

  if (type != TIFF_ASCII && count != 0)
  {
      CPLError(CE_Warning, CPLE_AppDefined, "EXIF metadata truncated");
  }
}


/************************************************************************/
/*                        EXIFExtractMetadata()                         */
/*                                                                      */
/*      Extract all entry from a IFD                                    */
/************************************************************************/
CPLErr EXIFExtractMetadata(char**& papszMetadata,
                           void *fpInL, int nOffset,
                           int bSwabflag, int nTIFFHEADER,
                           int& nExifOffset, int& nInterOffset, int& nGPSOffset)
{
    GUInt16        nEntryCount;
    int space;
    unsigned int           n,i;
    char          szTemp[MAXSTRINGLENGTH+1];
    char          szName[128];

    VSILFILE* fp = (VSILFILE* )fpInL;

    GDALEXIFTIFFDirEntry *poTIFFDirEntry;
    GDALEXIFTIFFDirEntry *poTIFFDir;
    const struct tagname *poExifTags ;
    const struct intr_tag *poInterTags = intr_tags;
    const struct gpsname *poGPSTags;

/* -------------------------------------------------------------------- */
/*      Read number of entry in directory                               */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL(fp, nOffset+nTIFFHEADER, SEEK_SET) != 0
        || VSIFReadL(&nEntryCount,1,sizeof(GUInt16),fp) != sizeof(GUInt16) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error reading EXIF Directory count at %d.",
                  nOffset + nTIFFHEADER );
        return CE_Failure;
    }

    if (bSwabflag)
        TIFFSwabShort(&nEntryCount);

    // Some apps write empty directories - see bug 1523.
    if( nEntryCount == 0 )
        return CE_None;

    // Some files are corrupt, a large entry count is a sign of this.
    if( nEntryCount > 125 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Ignoring EXIF directory with unlikely entry count (%d).",
                  nEntryCount );
        return CE_Warning;
    }

    poTIFFDir = (GDALEXIFTIFFDirEntry *)CPLMalloc(nEntryCount * sizeof(GDALEXIFTIFFDirEntry));

/* -------------------------------------------------------------------- */
/*      Read all directory entries                                      */
/* -------------------------------------------------------------------- */
    n = static_cast<int>(VSIFReadL(poTIFFDir, 1,nEntryCount*sizeof(GDALEXIFTIFFDirEntry),fp));
    if (n != nEntryCount*sizeof(GDALEXIFTIFFDirEntry))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Could not read all directories");
        CPLFree(poTIFFDir);
        return CE_Failure;
    }

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
        szName[0] = '\0';
        szTemp[0] = '\0';

        for (poExifTags = tagnames; poExifTags->tag; poExifTags++)
            if(poExifTags->tag == poTIFFDirEntry->tdir_tag) {
                CPLAssert( NULL != poExifTags && NULL != poExifTags->name );

                CPLStrlcpy(szName, poExifTags->name, sizeof(szName));
                break;
            }


        if( nOffset == nGPSOffset) {
            for( poGPSTags = gpstags; poGPSTags->tag != 0xffff; poGPSTags++ )
                if( poGPSTags->tag == poTIFFDirEntry->tdir_tag ) {
                    CPLAssert( NULL != poGPSTags && NULL != poGPSTags->name );
                    CPLStrlcpy(szName, poGPSTags->name, sizeof(szName));
                    break;
                }
        }
/* -------------------------------------------------------------------- */
/*      If the tag was not found, look into the interoperability table  */
/* -------------------------------------------------------------------- */
        if( nOffset == nInterOffset ) {
            for(poInterTags = intr_tags; poInterTags->tag; poInterTags++)
                if(poInterTags->tag == poTIFFDirEntry->tdir_tag) {
                    CPLAssert( NULL != poInterTags && NULL != poInterTags->name );
                    CPLStrlcpy(szName, poInterTags->name, sizeof(szName));
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
        if( szName[0] == '\0' )
        {
            snprintf( szName, sizeof(szName), "EXIF_%d", poTIFFDirEntry->tdir_tag );
            continue;
        }

/* -------------------------------------------------------------------- */
/*      For UserComment we need to ignore the language binding and      */
/*      just return the actual contents.                                */
/* -------------------------------------------------------------------- */
        if( EQUAL(szName,"EXIF_UserComment")  )
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
        if( EQUAL(szName,"EXIF_ExifVersion")
            || EQUAL(szName,"EXIF_FlashPixVersion")
            || EQUAL(szName,"EXIF_MakerNote")
            || EQUAL(szName,"GPSProcessingMethod") )
            poTIFFDirEntry->tdir_type = TIFF_ASCII;

/* -------------------------------------------------------------------- */
/*      Print tags                                                      */
/* -------------------------------------------------------------------- */
        int nDataWidth = TIFFDataWidth((GDALEXIFTIFFDataType) poTIFFDirEntry->tdir_type);
        space = poTIFFDirEntry->tdir_count * nDataWidth;

        /* Previous multiplication could overflow, hence this additional check */
        if (poTIFFDirEntry->tdir_count > MAXSTRINGLENGTH)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Too many bytes in tag: %u, ignoring tag.",
                      poTIFFDirEntry->tdir_count );
        }
        else if (nDataWidth == 0 || poTIFFDirEntry->tdir_type >= TIFF_IFD )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Invalid or unhandled EXIF data type: %d, ignoring tag.",
                      poTIFFDirEntry->tdir_type );
        }
/* -------------------------------------------------------------------- */
/*      This is at most 4 byte data so we can read it from tdir_offset  */
/* -------------------------------------------------------------------- */
        else if (space >= 0 && space <= 4) {

            unsigned char data[4];
            memcpy(data, &poTIFFDirEntry->tdir_offset, 4);
            if (bSwabflag)
            {
                // Unswab 32bit value, and reswab per data type.
                TIFFSwabLong((GUInt32*) data);

                switch (poTIFFDirEntry->tdir_type) {
                  case TIFF_LONG:
                  case TIFF_SLONG:
                  case TIFF_FLOAT:
                    TIFFSwabLong((GUInt32*) data);
                    break;

                  case TIFF_SSHORT:
                  case TIFF_SHORT:
                    TIFFSwabArrayOfShort((GUInt16*) data,
                                         poTIFFDirEntry->tdir_count);
                  break;

                  default:
                    break;
                }
            }

            /* coverity[overrun-buffer-arg] */
            EXIFPrintData(szTemp,
                          poTIFFDirEntry->tdir_type,
                          poTIFFDirEntry->tdir_count, data);
        }
/* -------------------------------------------------------------------- */
/*      The data is being read where tdir_offset point to in the file   */
/* -------------------------------------------------------------------- */
        else if (space > 0 && space < MAXSTRINGLENGTH)
        {
            unsigned char *data = (unsigned char *)VSIMalloc(space);

            if (data) {
                CPL_IGNORE_RET_VAL(VSIFSeekL(fp,poTIFFDirEntry->tdir_offset+nTIFFHEADER,SEEK_SET));
                CPL_IGNORE_RET_VAL(VSIFReadL(data, 1, space, fp));

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

                EXIFPrintData(szTemp, poTIFFDirEntry->tdir_type,
                              poTIFFDirEntry->tdir_count, data);
                CPLFree(data);
            }
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Invalid EXIF header size: %ld, ignoring tag.",
                      (long) space );
        }

        papszMetadata = CSLSetNameValue(papszMetadata, szName, szTemp);
    }
    CPLFree(poTIFFDir);

    return CE_None;
}
