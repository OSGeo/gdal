/******************************************************************************
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

#include "cpl_port.h"
#include "gdal_priv.h"
#include "gdalexif.h"

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

using std::vector;

CPL_CVSID("$Id$")

static const int MAXSTRINGLENGTH = 65535;
static const int EXIFOFFSETTAG = 0x8769;
static const int INTEROPERABILITYOFFSET = 0xA005;
static const int GPSOFFSETTAG = 0x8825;

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
      snprintf(szTemp, sizeof(szTemp), "%s%#02x", sep, *data++);
      sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;

  case TIFF_SBYTE:
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%d", sep, *(const char *)data++);
      sep = " ";
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
      snprintf(szTemp, sizeof(szTemp), "%s%u", sep, *wp++);
      sep = " ";
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
      snprintf(szTemp, sizeof(szTemp), "%s%ld", sep, (long) *lp++);
      sep = " ";
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
      if( (lp[0]==0) || (lp[1] == 0) ) {
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
      if( (lp[0]==0) || (lp[1] == 0) ) {
          snprintf(szTemp, sizeof(szTemp), "%s(0)",sep);
      }
      else{
        CPLsnprintf(szTemp, sizeof(szTemp), "%s(%g)", sep,
            (float) lp[0]/ (float) lp[1]);
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
  case TIFF_FLOAT: {
    const float *fp = (const float *)data;
    for(;count>0;count--) {
      CPLsnprintf(szTemp, sizeof(szTemp), "%s%g", sep, *fp++);
      sep = " ";
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
      CPLsnprintf(szTemp, sizeof(szTemp), "%s%g", sep, *dp++);
      sep = " ";
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


/*
 * Return size of TIFFDataType in bytes
 */
static int EXIF_TIFFDataWidth(GDALEXIFTIFFDataType type)
{
    switch(type)
    {
        case 0:  /* nothing */
        case TIFF_BYTE:
        case TIFF_ASCII:
        case TIFF_SBYTE:
        case TIFF_UNDEFINED:
            return 1;
        case TIFF_SHORT:
        case TIFF_SSHORT:
            return 2;
        case TIFF_LONG:
        case TIFF_SLONG:
        case TIFF_FLOAT:
        case TIFF_IFD:
            return 4;
        case TIFF_RATIONAL:
        case TIFF_SRATIONAL:
        case TIFF_DOUBLE:
        //case TIFF_LONG8:
        //case TIFF_SLONG8:
        //case TIFF_IFD8:
            return 8;
        default:
            return 0; /* will return 0 for unknown types */
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
/* -------------------------------------------------------------------- */
/*      Read number of entry in directory                               */
/* -------------------------------------------------------------------- */
    GUInt16 nEntryCount;
    VSILFILE * const fp = static_cast<VSILFILE *>(fpInL);

    if( nOffset > INT_MAX - nTIFFHEADER ||
        VSIFSeekL(fp, nOffset+nTIFFHEADER, SEEK_SET) != 0
        || VSIFReadL(&nEntryCount,1,sizeof(GUInt16),fp) != sizeof(GUInt16) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error reading EXIF Directory count at " CPL_FRMT_GUIB,
                  static_cast<vsi_l_offset>(nOffset) + nTIFFHEADER );
        return CE_Failure;
    }

    if (bSwabflag)
        CPL_SWAP16PTR(&nEntryCount);

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

    GDALEXIFTIFFDirEntry *poTIFFDir = static_cast<GDALEXIFTIFFDirEntry *>(
        CPLMalloc(nEntryCount * sizeof(GDALEXIFTIFFDirEntry)) );

/* -------------------------------------------------------------------- */
/*      Read all directory entries                                      */
/* -------------------------------------------------------------------- */
    {
        const unsigned int n = static_cast<int>(VSIFReadL(
            poTIFFDir, 1,nEntryCount*sizeof(GDALEXIFTIFFDirEntry),fp));
        if (n != nEntryCount*sizeof(GDALEXIFTIFFDirEntry))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Could not read all directories");
            CPLFree(poTIFFDir);
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse all entry information in this directory                   */
/* -------------------------------------------------------------------- */
    vector<char> oTempStorage(MAXSTRINGLENGTH+1, 0);
    char * const szTemp = &oTempStorage[0];

    char szName[128];

    GDALEXIFTIFFDirEntry *poTIFFDirEntry = poTIFFDir;

    for( unsigned int i = nEntryCount; i > 0; i--,poTIFFDirEntry++ ) {
        if (bSwabflag) {
            CPL_SWAP16PTR(&poTIFFDirEntry->tdir_tag);
            CPL_SWAP16PTR(&poTIFFDirEntry->tdir_type);
            CPL_SWAP32PTR(&poTIFFDirEntry->tdir_count);
            CPL_SWAP32PTR(&poTIFFDirEntry->tdir_offset);
        }

/* -------------------------------------------------------------------- */
/*      Find Tag name in table                                          */
/* -------------------------------------------------------------------- */
        szName[0] = '\0';
        szTemp[0] = '\0';

        for ( const struct tagname *poExifTags = tagnames;
              poExifTags->tag;
              poExifTags++)
        {
            if(poExifTags->tag == poTIFFDirEntry->tdir_tag) {
                CPLAssert( NULL != poExifTags->name );

                CPLStrlcpy(szName, poExifTags->name, sizeof(szName));
                break;
            }
        }

        if( nOffset == nGPSOffset) {
            for( const struct gpsname *poGPSTags = gpstags;
                 poGPSTags->tag != 0xffff;
                 poGPSTags++ )
            {
                if( poGPSTags->tag == poTIFFDirEntry->tdir_tag ) {
                    CPLAssert( NULL != poGPSTags->name );
                    CPLStrlcpy(szName, poGPSTags->name, sizeof(szName));
                    break;
                }
            }
        }
/* -------------------------------------------------------------------- */
/*      If the tag was not found, look into the interoperability table  */
/* -------------------------------------------------------------------- */
        if( nOffset == nInterOffset ) {
            const struct intr_tag *poInterTags = intr_tags;
            for( ; poInterTags->tag; poInterTags++)
                if(poInterTags->tag == poTIFFDirEntry->tdir_tag) {
                    CPLAssert( NULL != poInterTags->name );
                    CPLStrlcpy(szName, poInterTags->name, sizeof(szName));
                    break;
                }
        }

/* -------------------------------------------------------------------- */
/*      Save important directory tag offset                             */
/* -------------------------------------------------------------------- */

        // Our current API uses int32 and not uint32
        if( poTIFFDirEntry->tdir_offset < INT_MAX )
        {
            if( poTIFFDirEntry->tdir_tag == EXIFOFFSETTAG )
                nExifOffset=poTIFFDirEntry->tdir_offset;
            else if( poTIFFDirEntry->tdir_tag == INTEROPERABILITYOFFSET )
                nInterOffset=poTIFFDirEntry->tdir_offset;
            else if( poTIFFDirEntry->tdir_tag == GPSOFFSETTAG )
                nGPSOffset=poTIFFDirEntry->tdir_offset;
        }

/* -------------------------------------------------------------------- */
/*      If we didn't recognise the tag just ignore it.  To see all      */
/*      tags comment out the continue.                                  */
/* -------------------------------------------------------------------- */
        if( szName[0] == '\0' )
        {
            snprintf( szName, sizeof(szName), "EXIF_%u", poTIFFDirEntry->tdir_tag );
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
        const int nDataWidth =
            EXIF_TIFFDataWidth((GDALEXIFTIFFDataType) poTIFFDirEntry->tdir_type);
        const int space = poTIFFDirEntry->tdir_count * nDataWidth;

        /* Previous multiplication could overflow, hence this additional check */
        if( poTIFFDirEntry->tdir_count > static_cast<GUInt32>(MAXSTRINGLENGTH) )
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
                GUInt32 nValUInt32;
                // Unswab 32bit value, and reswab per data type.
                memcpy(&nValUInt32, data, 4);
                CPL_SWAP32PTR(&nValUInt32);
                memcpy(data, &nValUInt32, 4);

                switch (poTIFFDirEntry->tdir_type) {
                  case TIFF_LONG:
                  case TIFF_SLONG:
                  case TIFF_FLOAT:
                    memcpy(&nValUInt32, data, 4);
                    CPL_SWAP32PTR(&nValUInt32);
                    memcpy(data, &nValUInt32, 4);
                    break;

                  case TIFF_SSHORT:
                  case TIFF_SHORT:
                    for( unsigned j = 0; j < poTIFFDirEntry->tdir_count; j++ )
                    {
                        CPL_SWAP16PTR( reinterpret_cast<GUInt16*>(data) + j );
                    }
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
                      {
                        for( unsigned j = 0; j < poTIFFDirEntry->tdir_count; j++ )
                        {
                            CPL_SWAP16PTR( reinterpret_cast<GUInt16*>(data) + j );
                        }
                        break;
                      }
                      case TIFF_LONG:
                      case TIFF_SLONG:
                      case TIFF_FLOAT:
                      {
                        for( unsigned j = 0; j < poTIFFDirEntry->tdir_count; j++ )
                        {
                            CPL_SWAP32PTR( reinterpret_cast<GUInt32*>(data) + j );
                        }
                        break;
                      }
                      case TIFF_RATIONAL:
                      case TIFF_SRATIONAL:
                      {
                        for( unsigned j = 0; j < 2 * poTIFFDirEntry->tdir_count; j++ )
                        {
                            CPL_SWAP32PTR( reinterpret_cast<GUInt32*>(data) + j );
                        }
                        break;
                      }
                      case TIFF_DOUBLE:
                      {
                        for( unsigned j = 0; j < poTIFFDirEntry->tdir_count; j++ )
                        {
                            CPL_SWAPDOUBLE( reinterpret_cast<double*>(data) + j );
                        }
                        break;
                      }
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
