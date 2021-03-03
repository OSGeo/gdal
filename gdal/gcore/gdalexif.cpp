/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements a EXIF directory reader
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2012,2017, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>
#include <limits>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

using std::vector;

CPL_CVSID("$Id$")

constexpr int MAXSTRINGLENGTH = 65535;
constexpr int EXIFOFFSETTAG = 0x8769;
constexpr int INTEROPERABILITYOFFSET = 0xA005;
constexpr int GPSOFFSETTAG = 0x8825;

constexpr GUInt16 TAG_SIZE = 12;
constexpr GUInt16 EXIF_HEADER_SIZE = 6;

constexpr char COND_MANDATORY = 'M';
constexpr char COND_RECOMMENDED = 'R';
constexpr char COND_OPTIONAL = 'O';
constexpr char COND_NOT_ALLOWED = 'N';
constexpr char COND_NOT_ALLOWED_EVEN_IN_JPEG_MARKER = 'J';

struct EXIFTagDesc
{
    GUInt16              tag;
    GDALEXIFTIFFDataType datatype;
    GUInt32              length;
    const char*          name;
    char                 comprCond;
};

static const EXIFTagDesc gpstags [] = {
    { 0x00, TIFF_BYTE, 4, "EXIF_GPSVersionID", COND_OPTIONAL },
    { 0x01, TIFF_ASCII, 2, "EXIF_GPSLatitudeRef", COND_OPTIONAL },
    { 0x02, TIFF_RATIONAL, 3, "EXIF_GPSLatitude", COND_OPTIONAL },
    { 0x03, TIFF_ASCII, 2, "EXIF_GPSLongitudeRef", COND_OPTIONAL },
    { 0x04, TIFF_RATIONAL, 3, "EXIF_GPSLongitude", COND_OPTIONAL },
    { 0x05, TIFF_BYTE, 1, "EXIF_GPSAltitudeRef", COND_OPTIONAL },
    { 0x06, TIFF_RATIONAL, 1, "EXIF_GPSAltitude", COND_OPTIONAL },
    { 0x07, TIFF_RATIONAL, 3, "EXIF_GPSTimeStamp", COND_OPTIONAL },
    { 0x08, TIFF_ASCII, 0, "EXIF_GPSSatellites", COND_OPTIONAL },
    { 0x09, TIFF_ASCII, 2, "EXIF_GPSStatus", COND_OPTIONAL },
    { 0x0a, TIFF_ASCII, 2, "EXIF_GPSMeasureMode", COND_OPTIONAL },
    { 0x0b, TIFF_RATIONAL, 1, "EXIF_GPSDOP", COND_OPTIONAL },
    { 0x0c, TIFF_ASCII, 2, "EXIF_GPSSpeedRef", COND_OPTIONAL },
    { 0x0d, TIFF_RATIONAL, 1, "EXIF_GPSSpeed", COND_OPTIONAL },
    { 0x0e, TIFF_ASCII, 2, "EXIF_GPSTrackRef", COND_OPTIONAL },
    { 0x0f, TIFF_RATIONAL, 1, "EXIF_GPSTrack", COND_OPTIONAL },
    { 0x10, TIFF_ASCII, 2, "EXIF_GPSImgDirectionRef", COND_OPTIONAL },
    { 0x11, TIFF_RATIONAL, 1, "EXIF_GPSImgDirection", COND_OPTIONAL },
    { 0x12, TIFF_ASCII, 0, "EXIF_GPSMapDatum", COND_OPTIONAL },
    { 0x13, TIFF_ASCII, 2, "EXIF_GPSDestLatitudeRef", COND_OPTIONAL },
    { 0x14, TIFF_RATIONAL, 3, "EXIF_GPSDestLatitude", COND_OPTIONAL },
    { 0x15, TIFF_ASCII, 2,  "EXIF_GPSDestLongitudeRef", COND_OPTIONAL },
    { 0x16, TIFF_RATIONAL, 3, "EXIF_GPSDestLongitude", COND_OPTIONAL },
    { 0x17, TIFF_ASCII, 2, "EXIF_GPSDestBearingRef", COND_OPTIONAL },
    { 0x18, TIFF_RATIONAL, 1, "EXIF_GPSDestBearing", COND_OPTIONAL },
    { 0x19, TIFF_ASCII, 2, "EXIF_GPSDestDistanceRef", COND_OPTIONAL },
    { 0x1a, TIFF_RATIONAL, 1, "EXIF_GPSDestDistance", COND_OPTIONAL },
    { 0x1b, TIFF_UNDEFINED, 0, "EXIF_GPSProcessingMethod", COND_OPTIONAL },
    { 0x1c, TIFF_UNDEFINED, 0, "EXIF_GPSAreaInformation", COND_OPTIONAL },
    { 0x1d, TIFF_ASCII, 11, "EXIF_GPSDateStamp", COND_OPTIONAL },
    { 0x1e, TIFF_SHORT, 1, "EXIF_GPSDifferential", COND_OPTIONAL },
    { 0x1f, TIFF_RATIONAL, 1, "EXIF_GPSHPositioningError", COND_OPTIONAL },
    { 0xffff, TIFF_NOTYPE, 0, "", COND_NOT_ALLOWED}
};

static const EXIFTagDesc exiftags [] = {
    //{ 0x100, "EXIF_Image_Width"},
    //  { 0x101, "EXIF_Image_Length"},
    { 0x102, TIFF_NOTYPE, 0, "EXIF_BitsPerSample", COND_NOT_ALLOWED_EVEN_IN_JPEG_MARKER},
    { 0x103, TIFF_NOTYPE, 0, "EXIF_Compression", COND_NOT_ALLOWED_EVEN_IN_JPEG_MARKER},
    { 0x106, TIFF_NOTYPE, 0, "EXIF_PhotometricInterpretation", COND_NOT_ALLOWED},
    { 0x10A, TIFF_NOTYPE, 0, "EXIF_Fill_Order", COND_NOT_ALLOWED_EVEN_IN_JPEG_MARKER}, // not sure of cond
    { 0x10D, TIFF_ASCII, 0, "EXIF_Document_Name", COND_OPTIONAL}, // not sure of cond
    { 0x10E, TIFF_ASCII, 0, "EXIF_ImageDescription", COND_RECOMMENDED},
    { 0x10F, TIFF_ASCII, 0, "EXIF_Make", COND_RECOMMENDED},
    { 0x110, TIFF_ASCII, 0, "EXIF_Model", COND_RECOMMENDED},
    { 0x111, TIFF_NOTYPE, 0, "EXIF_StripOffsets", COND_NOT_ALLOWED},
    { 0x112, TIFF_SHORT, 1, "EXIF_Orientation", COND_RECOMMENDED},
    { 0x115, TIFF_NOTYPE, 0, "EXIF_SamplesPerPixel", COND_NOT_ALLOWED_EVEN_IN_JPEG_MARKER},
    { 0x116, TIFF_NOTYPE, 0, "EXIF_RowsPerStrip", COND_NOT_ALLOWED},
    { 0x117, TIFF_NOTYPE, 0, "EXIF_StripByteCounts", COND_NOT_ALLOWED},
    { 0x11A, TIFF_RATIONAL, 1, "EXIF_XResolution", COND_MANDATORY},
    { 0x11B, TIFF_RATIONAL, 1, "EXIF_YResolution", COND_MANDATORY},
    { 0x11C, TIFF_NOTYPE, 0, "EXIF_PlanarConfiguration", COND_NOT_ALLOWED_EVEN_IN_JPEG_MARKER},
    { 0x128, TIFF_SHORT, 1, "EXIF_ResolutionUnit", COND_MANDATORY},
    { 0x12D, TIFF_SHORT, 768, "EXIF_TransferFunction", COND_OPTIONAL},
    { 0x131, TIFF_ASCII, 0, "EXIF_Software", COND_OPTIONAL},
    { 0x132, TIFF_ASCII, 20, "EXIF_DateTime", COND_RECOMMENDED},
    { 0x13B, TIFF_ASCII, 0, "EXIF_Artist", COND_OPTIONAL},
    { 0x13E, TIFF_RATIONAL, 2, "EXIF_WhitePoint", COND_OPTIONAL},
    { 0x13F, TIFF_RATIONAL, 6, "EXIF_PrimaryChromaticities", COND_OPTIONAL},
    { 0x156, TIFF_NOTYPE, 0, "EXIF_Transfer_Range", COND_NOT_ALLOWED}, // not sure of cond
    { 0x200, TIFF_NOTYPE, 0, "EXIF_JPEG_Proc", COND_NOT_ALLOWED}, // not sure of cond
    { 0x201, TIFF_NOTYPE, 0, "EXIF_JPEGInterchangeFormat", COND_NOT_ALLOWED},
    { 0x202, TIFF_NOTYPE, 0, "EXIF_JPEGInterchangeFormatLength", COND_NOT_ALLOWED},
    { 0x211, TIFF_RATIONAL, 3, "EXIF_YCbCrCoefficients", COND_OPTIONAL},
    { 0x212, TIFF_NOTYPE, 0, "EXIF_YCbCrSubSampling", COND_NOT_ALLOWED_EVEN_IN_JPEG_MARKER},
    { 0x213, TIFF_SHORT, 1, "EXIF_YCbCrPositioning", COND_MANDATORY},
    { 0x214, TIFF_RATIONAL, 6, "EXIF_ReferenceBlackWhite", COND_OPTIONAL},
    { 0x2BC, TIFF_ASCII, 0, "EXIF_XmlPacket", COND_OPTIONAL}, // not in the EXIF standard. But found in some images
    { 0x828D, TIFF_NOTYPE, 0, "EXIF_CFA_Repeat_Pattern_Dim", COND_OPTIONAL},
    { 0x828E, TIFF_NOTYPE, 0, "EXIF_CFA_Pattern", COND_OPTIONAL},
    { 0x828F, TIFF_NOTYPE, 0, "EXIF_Battery_Level", COND_OPTIONAL},
    { 0x8298, TIFF_ASCII, 0, "EXIF_Copyright", COND_OPTIONAL}, // that one is an exception: high tag number, but should go to main IFD
    { 0x829A, TIFF_RATIONAL, 1, "EXIF_ExposureTime", COND_RECOMMENDED},
    { 0x829D, TIFF_RATIONAL, 1, "EXIF_FNumber", COND_OPTIONAL},
    { 0x83BB, TIFF_NOTYPE, 0, "EXIF_IPTC/NAA", COND_OPTIONAL},
    // { 0x8769, "EXIF_Offset"},
    { 0x8773, TIFF_NOTYPE, 0, "EXIF_Inter_Color_Profile", COND_OPTIONAL},
    { 0x8822, TIFF_SHORT, 1, "EXIF_ExposureProgram", COND_OPTIONAL},
    { 0x8824, TIFF_ASCII, 0, "EXIF_SpectralSensitivity", COND_OPTIONAL},
    // { 0x8825, "EXIF_GPSOffset"},
    { 0x8827, TIFF_SHORT, 0, "EXIF_ISOSpeedRatings", COND_OPTIONAL},
    { 0x8828, TIFF_UNDEFINED, 0, "EXIF_OECF", COND_OPTIONAL},
    { 0x8830, TIFF_SHORT, 1, "EXIF_SensitivityType", COND_OPTIONAL},
    { 0x8831, TIFF_LONG, 1, "EXIF_StandardOutputSensitivity", COND_OPTIONAL},
    { 0x8832, TIFF_LONG, 1, "EXIF_RecommendedExposureIndex", COND_OPTIONAL},
    { 0x8833, TIFF_LONG, 1, "EXIF_ISOSpeed", COND_OPTIONAL},
    { 0x8834, TIFF_LONG, 1, "EXIF_ISOSpeedLatitudeyyy", COND_OPTIONAL},
    { 0x8835, TIFF_LONG, 1, "EXIF_ISOSpeedLatitudezzz", COND_OPTIONAL},
    { 0x9000, TIFF_UNDEFINED, 4, "EXIF_ExifVersion", COND_MANDATORY},
    { 0x9003, TIFF_ASCII, 20, "EXIF_DateTimeOriginal", COND_OPTIONAL},
    { 0x9004, TIFF_ASCII, 20, "EXIF_DateTimeDigitized", COND_OPTIONAL},
    { 0x9010, TIFF_ASCII, 7, "EXIF_OffsetTime", COND_OPTIONAL},
    { 0x9011, TIFF_ASCII, 7, "EXIF_OffsetTimeOriginal", COND_OPTIONAL},
    { 0x9012, TIFF_ASCII, 7, "EXIF_OffsetTimeDigitized", COND_OPTIONAL},
    { 0x9101, TIFF_UNDEFINED, 4, "EXIF_ComponentsConfiguration", COND_MANDATORY},
    { 0x9102, TIFF_RATIONAL, 1, "EXIF_CompressedBitsPerPixel", COND_OPTIONAL},
    { 0x9201, TIFF_SRATIONAL, 1, "EXIF_ShutterSpeedValue", COND_OPTIONAL},
    { 0x9202, TIFF_RATIONAL, 1,"EXIF_ApertureValue", COND_OPTIONAL},
    { 0x9203, TIFF_SRATIONAL, 1,"EXIF_BrightnessValue", COND_OPTIONAL},
    { 0x9204, TIFF_SRATIONAL, 1, "EXIF_ExposureBiasValue", COND_OPTIONAL},
    { 0x9205, TIFF_RATIONAL, 1, "EXIF_MaxApertureValue", COND_OPTIONAL},
    { 0x9206, TIFF_RATIONAL, 1, "EXIF_SubjectDistance", COND_OPTIONAL},
    { 0x9207, TIFF_SHORT, 1, "EXIF_MeteringMode", COND_OPTIONAL},
    { 0x9208, TIFF_SHORT, 1, "EXIF_LightSource", COND_OPTIONAL},
    { 0x9209, TIFF_SHORT, 1, "EXIF_Flash", COND_RECOMMENDED},
    { 0x920A, TIFF_RATIONAL, 1, "EXIF_FocalLength", COND_OPTIONAL},
    { 0x9214, TIFF_SHORT, 0, "EXIF_SubjectArea", COND_OPTIONAL}, // count = 2, 3 or 4
    { 0x927C, TIFF_UNDEFINED, 0, "EXIF_MakerNote", COND_OPTIONAL},
    { 0x9286, TIFF_UNDEFINED, 0, "EXIF_UserComment", COND_OPTIONAL},
    { 0x9290, TIFF_ASCII, 0, "EXIF_SubSecTime", COND_OPTIONAL},
    { 0x9291, TIFF_ASCII, 0, "EXIF_SubSecTime_Original", COND_OPTIONAL},
    { 0x9292, TIFF_ASCII, 0, "EXIF_SubSecTime_Digitized", COND_OPTIONAL},
    { 0xA000, TIFF_UNDEFINED, 4, "EXIF_FlashpixVersion", COND_MANDATORY},
    { 0xA001, TIFF_SHORT, 1, "EXIF_ColorSpace", COND_MANDATORY},
    { 0xA002, TIFF_LONG, 1, "EXIF_PixelXDimension", COND_MANDATORY}, // SHORT also OK
    { 0xA003, TIFF_LONG, 1, "EXIF_PixelYDimension", COND_MANDATORY}, // SHORT also OK
    { 0xA004, TIFF_ASCII, 13, "EXIF_RelatedSoundFile", COND_OPTIONAL},
    // { 0xA005, "EXIF_InteroperabilityOffset"},
    { 0xA20B, TIFF_RATIONAL, 1, "EXIF_FlashEnergy", COND_OPTIONAL},   // 0x920B in TIFF/EP
    { 0xA20C, TIFF_UNDEFINED, 0, "EXIF_SpatialFrequencyResponse", COND_OPTIONAL},   // 0x920C    -  -
    { 0xA20E, TIFF_RATIONAL, 1, "EXIF_FocalPlaneXResolution", COND_OPTIONAL},     // 0x920E    -  -
    { 0xA20F, TIFF_RATIONAL, 1, "EXIF_FocalPlaneYResolution", COND_OPTIONAL},     // 0x920F    -  -
    { 0xA210, TIFF_SHORT, 1, "EXIF_FocalPlaneResolutionUnit", COND_OPTIONAL},  // 0x9210    -  -
    { 0xA214, TIFF_SHORT, 2, "EXIF_SubjectLocation", COND_OPTIONAL}, // 0x9214    -  -
    { 0xA215, TIFF_RATIONAL, 1, "EXIF_ExposureIndex", COND_OPTIONAL},  // 0x9215    -  -
    { 0xA217, TIFF_SHORT, 1, "EXIF_SensingMethod", COND_OPTIONAL},  // 0x9217    -  -
    { 0xA300, TIFF_UNDEFINED, 1, "EXIF_FileSource", COND_OPTIONAL},
    { 0xA301, TIFF_UNDEFINED, 1, "EXIF_SceneType", COND_OPTIONAL},
    { 0xA302, TIFF_UNDEFINED, 0, "EXIF_CFAPattern", COND_OPTIONAL},
    { 0xA401, TIFF_SHORT, 1, "EXIF_CustomRendered", COND_OPTIONAL},
    { 0xA402, TIFF_SHORT, 1, "EXIF_ExposureMode", COND_RECOMMENDED},
    { 0XA403, TIFF_SHORT, 1, "EXIF_WhiteBalance", COND_RECOMMENDED},
    { 0xA404, TIFF_RATIONAL, 1, "EXIF_DigitalZoomRatio", COND_OPTIONAL},
    { 0xA405, TIFF_SHORT, 1, "EXIF_FocalLengthIn35mmFilm", COND_OPTIONAL},
    { 0xA406, TIFF_SHORT, 1, "EXIF_SceneCaptureType", COND_RECOMMENDED},
    { 0xA407, TIFF_RATIONAL, 1, "EXIF_GainControl", COND_OPTIONAL},
    { 0xA408, TIFF_SHORT, 1, "EXIF_Contrast", COND_OPTIONAL},
    { 0xA409, TIFF_SHORT, 1, "EXIF_Saturation", COND_OPTIONAL},
    { 0xA40A, TIFF_SHORT, 1, "EXIF_Sharpness", COND_OPTIONAL},
    { 0xA40B, TIFF_UNDEFINED, 0, "EXIF_DeviceSettingDescription", COND_OPTIONAL},
    { 0xA40C, TIFF_SHORT, 1, "EXIF_SubjectDistanceRange", COND_OPTIONAL},
    { 0xA420, TIFF_ASCII, 33, "EXIF_ImageUniqueID", COND_OPTIONAL},
    { 0xA430, TIFF_ASCII, 0, "EXIF_CameraOwnerName", COND_OPTIONAL},
    { 0xA431, TIFF_ASCII, 0, "EXIF_BodySerialNumber", COND_OPTIONAL},
    { 0xA432, TIFF_RATIONAL, 4, "EXIF_LensSpecification", COND_OPTIONAL},
    { 0xA433, TIFF_ASCII, 0, "EXIF_LensMake", COND_OPTIONAL},
    { 0xA434, TIFF_ASCII, 0, "EXIF_LensModel", COND_OPTIONAL},
    { 0xA435, TIFF_ASCII, 0, "EXIF_LensSerialNumber", COND_OPTIONAL},
    { 0x0000, TIFF_NOTYPE, 0, "", COND_NOT_ALLOWED}
};

static const struct intr_tag {
  GInt16        tag;
  const char*   name;
} intr_tags [] = {

    { 0x1, "EXIF_Interoperability_Index"},
    { 0x2, "EXIF_Interoperability_Version"},
    { 0x1000, "EXIF_Related_Image_File_Format"},
    { 0x1001, "EXIF_Related_Image_Width"},
    { 0x1002, "EXIF_Related_Image_Length"},
    { 0x0000,       ""}
};

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
      snprintf(szTemp, sizeof(szTemp), "%s0x%02x", sep, *data);
      data++;
      sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;

  case TIFF_SBYTE:
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%d", sep, *reinterpret_cast<const char *>(data));
      data++;
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
    const GUInt16 *wp = reinterpret_cast<const GUInt16 *>(data);
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%u", sep, *wp);
      wp++;
      sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_SSHORT: {
    const GInt16 *wp = reinterpret_cast<const GInt16 *>(data);
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%d", sep, *wp);
      wp++;
      sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_LONG: {
    const GUInt32 *lp = reinterpret_cast<const GUInt32 *>(data);
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%u", sep, *lp);
      lp++;
      sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_SLONG: {
    const GInt32 *lp = reinterpret_cast<const GInt32 *>(data);
    for(;count>0;count--) {
      snprintf(szTemp, sizeof(szTemp), "%s%d", sep, *lp);
      lp++;
      sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_RATIONAL: {
    const GUInt32 *lp = reinterpret_cast<const GUInt32 *>(data);
      //      if(bSwabflag)
      //      TIFFSwabArrayOfLong((GUInt32*) data, 2*count);
    for(;count>0;count--) {
      if( (lp[0]==0) || (lp[1] == 0) ) {
          snprintf(szTemp, sizeof(szTemp), "%s(0)",sep);
      }
      else{
          CPLsnprintf(szTemp, sizeof(szTemp), "%s(%g)", sep,
              static_cast<double>(lp[0])/ static_cast<double>(lp[1]));
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
    const GInt32 *lp = reinterpret_cast<const GInt32 *>(data);
    for(;count>0;count--) {
      if( (lp[0]==0) || (lp[1] == 0) ) {
          snprintf(szTemp, sizeof(szTemp), "%s(0)",sep);
      }
      else{
        CPLsnprintf(szTemp, sizeof(szTemp), "%s(%g)", sep,
            static_cast<double>(lp[0])/ static_cast<double>(lp[1]));
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
    const float *fp = reinterpret_cast<const float *>(data);
    for(;count>0;count--) {
      CPLsnprintf(szTemp, sizeof(szTemp), "%s%g", sep, *fp);
      fp++;
      sep = " ";
      if (strlen(szTemp) + pszDataEnd - pszData >= MAXSTRINGLENGTH)
          break;
      strcat(pszDataEnd,szTemp);
      pszDataEnd += strlen(pszDataEnd);
    }
    break;
  }
  case TIFF_DOUBLE: {
    const double *dp = reinterpret_cast<const double *>(data);
    for(;count>0;count--) {
      CPLsnprintf(szTemp, sizeof(szTemp), "%s%g", sep, *dp);
      dp++;
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
static int EXIF_TIFFDataWidth(int /* GDALEXIFTIFFDataType */ type)
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

        for ( const EXIFTagDesc *poExifTags = exiftags;
              poExifTags->tag;
              poExifTags++)
        {
            if(poExifTags->tag == poTIFFDirEntry->tdir_tag) {
                CPLAssert( nullptr != poExifTags->name );

                CPLStrlcpy(szName, poExifTags->name, sizeof(szName));
                break;
            }
        }

        if( nOffset == nGPSOffset) {
            for( const EXIFTagDesc *poGPSTags = gpstags;
                 poGPSTags->tag != 0xffff;
                 poGPSTags++ )
            {
                if( poGPSTags->tag == poTIFFDirEntry->tdir_tag ) {
                    CPLAssert( nullptr != poGPSTags->name );
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
                    CPLAssert( nullptr != poInterTags->name );
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

        vsi_l_offset nTagValueOffset = poTIFFDirEntry->tdir_offset;

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
                nTagValueOffset += 8;
            }
        }

/* -------------------------------------------------------------------- */
/*      Make some UNDEFINED or BYTE fields ASCII for readability.       */
/* -------------------------------------------------------------------- */
        if( EQUAL(szName,"EXIF_ExifVersion")
            || EQUAL(szName,"EXIF_FlashPixVersion")
            || EQUAL(szName,"EXIF_MakerNote")
            || EQUAL(szName,"GPSProcessingMethod")
            || EQUAL(szName,"EXIF_XmlPacket") )
            poTIFFDirEntry->tdir_type = TIFF_ASCII;

/* -------------------------------------------------------------------- */
/*      Print tags                                                      */
/* -------------------------------------------------------------------- */
        if( poTIFFDirEntry->tdir_count > static_cast<GUInt32>(MAXSTRINGLENGTH) )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Too many bytes in tag: %u, ignoring tag.",
                      poTIFFDirEntry->tdir_count );
            continue;
        }

        const int nDataWidth =
            EXIF_TIFFDataWidth(poTIFFDirEntry->tdir_type);
        if (nDataWidth == 0 || poTIFFDirEntry->tdir_type >= TIFF_IFD )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Invalid or unhandled EXIF data type: %d, ignoring tag.",
                      poTIFFDirEntry->tdir_type );
            continue;
        }

/* -------------------------------------------------------------------- */
/*      This is at most 4 byte data so we can read it from tdir_offset  */
/* -------------------------------------------------------------------- */
        const int space = poTIFFDirEntry->tdir_count * nDataWidth;
        if (space >= 0 && space <= 4) {

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
            unsigned char *data = static_cast<unsigned char *>(VSIMalloc(space));

            if (data) {
                CPL_IGNORE_RET_VAL(VSIFSeekL(fp,nTagValueOffset+nTIFFHEADER,SEEK_SET));
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
                      static_cast<long>(space) );
        }

        papszMetadata = CSLSetNameValue(papszMetadata, szName, szTemp);
    }
    CPLFree(poTIFFDir);

    return CE_None;
}

/************************************************************************/
/*                        WriteLEUInt16()                               */
/************************************************************************/

static void WriteLEUInt16(GByte* pabyData, GUInt32& nBufferOff, GUInt16 nVal)
{
    pabyData[nBufferOff] = static_cast<GByte>(nVal & 0xff);
    pabyData[nBufferOff+1] = static_cast<GByte>(nVal >> 8);
    nBufferOff += 2;
}

/************************************************************************/
/*                        WriteLEUInt32()                               */
/************************************************************************/

static void WriteLEUInt32(GByte* pabyData, GUInt32& nBufferOff, GUInt32 nVal)
{
    pabyData[nBufferOff] = static_cast<GByte>(nVal & 0xff);
    pabyData[nBufferOff+1] = static_cast<GByte>((nVal >> 8) & 0xff);
    pabyData[nBufferOff+2] = static_cast<GByte>((nVal >> 16) & 0xff);
    pabyData[nBufferOff+3] = static_cast<GByte>(nVal >> 24);
    nBufferOff += 4;
}

/************************************************************************/
/*                          GetHexValue()                               */
/************************************************************************/

static int GetHexValue(char ch)
{
    const char chDEC_ZERO = '0';
    if (ch >= chDEC_ZERO && ch <= '9')
        return ch - chDEC_ZERO;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

/************************************************************************/
/*                         ParseUndefined()                             */
/************************************************************************/

static GByte* ParseUndefined(const char* pszVal, GUInt32* pnLength)
{
    GUInt32 nSize = 0;
    bool bIsHexExcaped = true;
    const char chDEC_ZERO = '0';
    GByte* pabyData = reinterpret_cast<GByte*>(CPLMalloc(strlen(pszVal)+1));

    // Is it a hexadecimal string like "0xA 0x1E 00 0xDF..." ?
    for( size_t i = 0; pszVal[i] != '\0'; )
    {
        // 0xA
        if( pszVal[i] == chDEC_ZERO && pszVal[i+1] == 'x' &&
            GetHexValue(pszVal[i+2]) >= 0 && (
                pszVal[i+3] == ' ' || pszVal[i+3] == '\0') )
        {
            pabyData[nSize] = static_cast<GByte>(GetHexValue(pszVal[i+2]));
            nSize ++;
            if( pszVal[i+3] == '\0' )
                break;
            i += 4;
        }
        // 0xAA
        else if( pszVal[i] == chDEC_ZERO && pszVal[i+1] == 'x' &&
                 GetHexValue(pszVal[i+2]) >= 0 &&
                 GetHexValue(pszVal[i+3]) >= 0 &&
                 (pszVal[i+4] == ' ' || pszVal[i+4] == '\0') )
        {
            pabyData[nSize] = static_cast<GByte>(GetHexValue(pszVal[i+2]) * 16 +
                                    GetHexValue(pszVal[i+3]));
            nSize ++;
            if( pszVal[i+4] == '\0' )
                break;
            i += 5;
        }
        // 00
        else if( pszVal[i] == chDEC_ZERO && pszVal[i+1] == chDEC_ZERO &&
                 (pszVal[i+2] == ' ' || pszVal[i+2] == '\0') )
        {
            pabyData[nSize] = 0;
            nSize ++;
            if( pszVal[i+2] == '\0' )
                break;
            i += 3;
        }
        else
        {
            bIsHexExcaped = false;
            break;
        }
    }

    if( bIsHexExcaped )
    {
        *pnLength = nSize;
        return pabyData;
    }

    // Otherwise take the string value as a byte value
    memcpy(pabyData, pszVal, strlen(pszVal) + 1);
    *pnLength = static_cast<GUInt32>(strlen(pszVal));
    return pabyData;
}

/************************************************************************/
/*                           EXIFTagSort()                              */
/************************************************************************/

struct TagValue
{
    GUInt16              tag;
    GDALEXIFTIFFDataType datatype;
    GByte*               pabyVal;
    GUInt32              nLength;
    GUInt32              nLengthBytes;
    int                  nRelOffset;
};

static bool EXIFTagSort(const TagValue& a, const TagValue& b)
{
    return a.tag <= b.tag;
}

/************************************************************************/
/*                        GetNumDenomFromDouble()                       */
/************************************************************************/

static bool GetNumDenomFromDouble(GDALEXIFTIFFDataType datatype, double dfVal,
                                  GUInt32& nNum, GUInt32& nDenom)
{
    nNum = 0;
    nDenom = 1;
    if( CPLIsNan(dfVal) )
    {
        return false;
    }
    else if( datatype == TIFF_RATIONAL )
    {
        if( dfVal < 0 )
        {
            return false;
        }
        else if (dfVal <= std::numeric_limits<unsigned int>::max() &&
                 dfVal == static_cast<GUInt32>(dfVal))
        {
            nNum = static_cast<GUInt32>(dfVal);
            nDenom = 1;
        }
        else if (dfVal<1.0)
        {
            nNum = static_cast<GUInt32>(
                        dfVal*std::numeric_limits<unsigned int>::max());
            nDenom = std::numeric_limits<unsigned int>::max();
        }
        else
        {
            nNum = std::numeric_limits<unsigned int>::max();
            nDenom = static_cast<GUInt32>(
                        std::numeric_limits<unsigned int>::max()/dfVal);
        }
    }
    else if (dfVal < 0.0)
    {
        if( dfVal >= std::numeric_limits<int>::min() &&
            dfVal == static_cast<GInt32>(dfVal))
        {
            nNum = static_cast<GInt32>(dfVal);
            nDenom = 1;
        }
        else if (dfVal>-1.0)
        {
            nNum = -static_cast<GInt32>(
                        (-dfVal)*std::numeric_limits<int>::max());
            nDenom = std::numeric_limits<int>::max();
        }
        else
        {
            nNum = -std::numeric_limits<int>::max();
            nDenom = static_cast<GInt32>(
                        std::numeric_limits<int>::max()/(-dfVal));
        }
    }
    else
    {
        if (dfVal <= std::numeric_limits<int>::max() &&
                dfVal == static_cast<GInt32>(dfVal))
        {
            nNum = static_cast<GInt32>(dfVal);
            nDenom = 1;
        }
        else if (dfVal<1.0)
        {
            nNum = static_cast<GInt32>(dfVal*std::numeric_limits<int>::max());
            nDenom = std::numeric_limits<int>::max();
        }
        else
        {
            nNum = std::numeric_limits<int>::max();
            nDenom = static_cast<GInt32>(std::numeric_limits<int>::max()/dfVal);
        }
    }
    return true;
}

/************************************************************************/
/*                       EXIFFormatTagValue()                           */
/************************************************************************/

enum class EXIFLocation
{
    MAIN_IFD,
    EXIF_IFD,
    GPS_IFD
};

static
std::vector<TagValue> EXIFFormatTagValue(char** papszEXIFMetadata,
                                         EXIFLocation location,
                                         GUInt32* pnOfflineSize)
{
    std::vector<TagValue> tags;
    int nRelOffset = 0;
    const EXIFTagDesc* tagdescArray =
                                (location == EXIFLocation::GPS_IFD) ? gpstags: exiftags;

    for(char** papszIter = papszEXIFMetadata;
                            papszIter && *papszIter; ++papszIter )
    {
        if( !STARTS_WITH_CI(*papszIter, "EXIF_") )
            continue;
        if( location == EXIFLocation::GPS_IFD && !STARTS_WITH_CI(*papszIter, "EXIF_GPS") )
            continue;
        if( location != EXIFLocation::GPS_IFD && STARTS_WITH_CI(*papszIter, "EXIF_GPS") )
            continue;

        bool bFound = false;
        size_t i = 0; // needed after loop
        for( ; tagdescArray[i].name[0] != '\0'; i++ )
        {
            if( STARTS_WITH_CI(*papszIter, tagdescArray[i].name) &&
                (*papszIter)[strlen(tagdescArray[i].name)] == '=' )
            {
                bFound = true;
                break;
            }
        }

        if( location == EXIFLocation::MAIN_IFD )
        {
            if( tagdescArray[i].tag > 0x8298 ) // EXIF_Copyright
            {
                continue;
            }
        }
        else if( location == EXIFLocation::EXIF_IFD )
        {
            if( tagdescArray[i].tag <= 0x8298 ) // EXIF_Copyright
            {
                continue;
            }
        }

        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( !bFound || pszKey == nullptr || pszValue == nullptr )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Cannot write unknown %s tag",
                     pszKey ? pszKey : "");
        }
        else if( tagdescArray[i].datatype == TIFF_NOTYPE )
        {
            CPLDebug("EXIF", "Tag %s ignored on write", tagdescArray[i].name);
        }
        else
        {
            TagValue tag;
            tag.tag = tagdescArray[i].tag;
            tag.datatype = tagdescArray[i].datatype;
            tag.pabyVal = nullptr;
            tag.nLength = 0;
            tag.nLengthBytes = 0;
            tag.nRelOffset = -1;

            if( tag.datatype == TIFF_ASCII )
            {
                if( tagdescArray[i].length == 0 ||
                    strlen(pszValue) + 1 == tagdescArray[i].length )
                {
                    tag.pabyVal = reinterpret_cast<GByte*>(CPLStrdup(pszValue));
                    tag.nLength = 1 + static_cast<int>(strlen(pszValue));
                }
                else if( strlen(pszValue) >= tagdescArray[i].length )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Value of %s will be truncated",
                             tagdescArray[i].name);
                    tag.pabyVal = reinterpret_cast<GByte*>(
                                    CPLMalloc(tagdescArray[i].length));
                    memcpy(tag.pabyVal, pszValue, tagdescArray[i].length);
                    tag.nLength = tagdescArray[i].length;
                    tag.pabyVal[tag.nLength-1] = '\0';
                }
                else
                {
                    tag.pabyVal = reinterpret_cast<GByte*>(
                                    CPLMalloc(tagdescArray[i].length));
                    memset(tag.pabyVal, ' ', tagdescArray[i].length);
                    memcpy(tag.pabyVal, pszValue, strlen(pszValue));
                    tag.nLength = tagdescArray[i].length;
                    tag.pabyVal[tag.nLength-1] = '\0';
                }
                tag.nLengthBytes = tag.nLength;
            }
            else if( tag.datatype == TIFF_BYTE ||
                     tag.datatype == TIFF_UNDEFINED )
            {
                GUInt32 nValLength = 0;
                GByte* pabyVal = ParseUndefined(pszValue, &nValLength);
                if( tagdescArray[i].length == 0 ||
                    nValLength == tagdescArray[i].length )
                {
                    tag.pabyVal = pabyVal;
                    tag.nLength = nValLength;
                }
                else if( nValLength > tagdescArray[i].length )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Value of %s will be truncated",
                             tagdescArray[i].name);
                    tag.pabyVal = pabyVal;
                    tag.nLength = tagdescArray[i].length;
                }
                else
                {
                    tag.pabyVal = reinterpret_cast<GByte*>(
                                CPLRealloc(pabyVal, tagdescArray[i].length));
                    memset(tag.pabyVal + nValLength, '\0',
                           tagdescArray[i].length - nValLength);
                    tag.nLength = tagdescArray[i].length;
                }
                tag.nLengthBytes = tag.nLength;
            }
            else if( tag.datatype == TIFF_SHORT ||
                     tag.datatype == TIFF_LONG )
            {
                char** papszTokens = CSLTokenizeString2(pszValue, " ", 0);
                GUInt32 nTokens = static_cast<GUInt32>(CSLCount(papszTokens));
                const GUInt32 nDataTypeSize =
                                (tag.datatype == TIFF_SHORT) ? 2 : 4;
                if( tagdescArray[i].length == 0 ||
                    nTokens == tagdescArray[i].length )
                {
                    // ok
                }
                else if( nTokens > tagdescArray[i].length)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Value of %s will be truncated",
                             tagdescArray[i].name);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Not enough values for %s: %d expected. "
                             "Filling with zeroes",
                             tagdescArray[i].name, tagdescArray[i].length);
                }

                tag.nLength = (tagdescArray[i].length == 0) ? nTokens :
                                                        tagdescArray[i].length;
                tag.pabyVal = reinterpret_cast<GByte*>(
                        CPLCalloc(1, nDataTypeSize * tag.nLength));

                GUInt32 nOffset = 0;
                for( GUInt32 j = 0; j < std::min(nTokens, tag.nLength); j++ )
                {
                    GUInt32 nVal = atoi(papszTokens[j]);
                    if( tag.datatype == TIFF_SHORT )
                        WriteLEUInt16(tag.pabyVal, nOffset,
                                      static_cast<GUInt16>(nVal));
                    else
                        WriteLEUInt32(tag.pabyVal, nOffset, nVal);
                }
                CSLDestroy(papszTokens);

                tag.nLengthBytes = tag.nLength * nDataTypeSize;
            }
            else if( tag.datatype == TIFF_RATIONAL ||
                     tag.datatype == TIFF_SRATIONAL )
            {
                char** papszTokens = CSLTokenizeString2(pszValue, " ", 0);
                GUInt32 nTokens = static_cast<GUInt32>(CSLCount(papszTokens));
                const GUInt32 nDataTypeSize = 8;
                if( tagdescArray[i].length == 0 ||
                    nTokens == tagdescArray[i].length )
                {
                    // ok
                }
                else if( nTokens > tagdescArray[i].length)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Value of %s will be truncated",
                             tagdescArray[i].name);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Not enough values for %s: %d expected. "
                             "Filling with zeroes",
                             tagdescArray[i].name, tagdescArray[i].length);
                }

                tag.nLength = (tagdescArray[i].length == 0) ? nTokens :
                                                        tagdescArray[i].length;
                tag.pabyVal = reinterpret_cast<GByte*>(
                        CPLCalloc(1, nDataTypeSize * tag.nLength));

                GUInt32 nOffset = 0;
                for( GUInt32 j = 0; j < std::min(nTokens, tag.nLength); j++ )
                {
                    double dfVal = CPLAtof(papszTokens[j][0] == '(' ?
                                        papszTokens[j] + 1 : papszTokens[j]);
                    GUInt32 nNum = 1;
                    GUInt32 nDenom = 0;
                    if( !GetNumDenomFromDouble(tag.datatype, dfVal,
                                               nNum, nDenom) )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                    "Value %f is illegal for tag %s",
                                    dfVal, tagdescArray[i].name);
                    }

                    WriteLEUInt32(tag.pabyVal, nOffset, nNum);
                    WriteLEUInt32(tag.pabyVal, nOffset, nDenom);
                }
                CSLDestroy(papszTokens);

                tag.nLengthBytes = tag.nLength * nDataTypeSize;
            }
            else
            {
                // Shouldn't happen. Programming error
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Unhandled type %d for tag %s",
                         tag.datatype, tagdescArray[i].name);
            }

            if( tag.nLengthBytes != 0 )
            {
                if( tag.nLengthBytes > 4 )
                {
                    tag.nRelOffset = nRelOffset;
                    nRelOffset += tag.nLengthBytes + (tag.nLengthBytes % 1);
                }
                tags.push_back(tag);
            }

        }
        CPLFree(pszKey);
    }

    // Sort tags by ascending order
    std::sort(tags.begin(), tags.end(), EXIFTagSort);

#ifdef notdef
    if( location == EXIF_IFD &&
        CSLFetchNameValue(papszEXIFMetadata, "EXIF_ExifVersion") == nullptr )
    {
        const GUInt16 EXIF_VERSION = 0x9000;
        TagValue tag;
        tag.tag = EXIF_VERSION;
        tag.datatype = TIFF_UNDEFINED;
        tag.pabyVal = reinterpret_cast<GByte*>(CPLStrdup("0231"));
        tag.nLength = 4;
        tag.nLengthBytes = 4;
        tag.nRelOffset = -1;
        tags.push_back(tag);
    }
#endif

    *pnOfflineSize = nRelOffset;

    return tags;
}

/************************************************************************/
/*                            WriteTag()                                */
/************************************************************************/

static void WriteTag(GByte* pabyData, GUInt32& nBufferOff,
                     GUInt16 nTag, GDALEXIFTIFFDataType nType,
                     GUInt32 nCount, GUInt32 nVal)
{
    WriteLEUInt16(pabyData, nBufferOff, nTag);
    WriteLEUInt16(pabyData, nBufferOff, static_cast<GUInt16>(nType));
    WriteLEUInt32(pabyData, nBufferOff, nCount);
    WriteLEUInt32(pabyData, nBufferOff, nVal);
}

/************************************************************************/
/*                            WriteTags()                               */
/************************************************************************/

static void WriteTags(GByte* pabyData, GUInt32& nBufferOff,
                      GUInt32 offsetIFDData,
                      std::vector<TagValue>& tags)
{
    for( auto& tag: tags )
    {
        WriteLEUInt16(pabyData, nBufferOff, tag.tag);
        WriteLEUInt16(pabyData, nBufferOff,
                      static_cast<GUInt16>(tag.datatype));
        WriteLEUInt32(pabyData, nBufferOff, tag.nLength);
        if( tag.nRelOffset < 0 )
        {
            CPLAssert(tag.nLengthBytes <= 4);
            memcpy(pabyData + nBufferOff,
                tag.pabyVal,
                tag.nLengthBytes);
            nBufferOff += 4;
        }
        else
        {
            WriteLEUInt32(pabyData, nBufferOff,
                        tag.nRelOffset + offsetIFDData);
            memcpy(pabyData + EXIF_HEADER_SIZE +
                                tag.nRelOffset + offsetIFDData,
                   tag.pabyVal,
                   tag.nLengthBytes);
        }
    }
}

/************************************************************************/
/*                            FreeTags()                                */
/************************************************************************/

static void FreeTags(std::vector<TagValue>& tags)
{
    for( auto& tag: tags )
    {
        CPLFree(tag.pabyVal);
    }
}

/************************************************************************/
/*                          EXIFCreate()                                */
/************************************************************************/

GByte* EXIFCreate(char**   papszEXIFMetadata,
                  GByte*   pabyThumbnail,
                  GUInt32  nThumbnailSize,
                  GUInt32  nThumbnailWidth,
                  GUInt32  nThumbnailHeight,
                  GUInt32 *pnOutBufferSize)
{
    *pnOutBufferSize = 0;

    bool bHasEXIFMetadata = false;
    for(char** papszIter = papszEXIFMetadata;
                                        papszIter && *papszIter; ++papszIter )
    {
        if( STARTS_WITH_CI(*papszIter, "EXIF_") )
        {
            bHasEXIFMetadata = true;
            break;
        }
    }
    if( !bHasEXIFMetadata && pabyThumbnail == nullptr )
    {
        // Nothing to do
        return nullptr;
    }

    GUInt32 nOfflineSizeMain = 0;
    std::vector<TagValue> mainTags = EXIFFormatTagValue(papszEXIFMetadata,
                                                        EXIFLocation::MAIN_IFD,
                                                        &nOfflineSizeMain);

    GUInt32 nOfflineSizeEXIF = 0;
    std::vector<TagValue> exifTags = EXIFFormatTagValue(papszEXIFMetadata,
                                                        EXIFLocation::EXIF_IFD,
                                                        &nOfflineSizeEXIF);

    GUInt32 nOfflineSizeGPS = 0;
    std::vector<TagValue> gpsTags = EXIFFormatTagValue(papszEXIFMetadata,
                                                        EXIFLocation::GPS_IFD,
                                                        &nOfflineSizeGPS);

    const GUInt16 nEXIFTags = static_cast<GUInt16>(exifTags.size());
    const GUInt16 nGPSTags = static_cast<GUInt16>(gpsTags.size());

    // including TIFFTAG_EXIFIFD and TIFFTAG_GPSIFD
    GUInt16 nIFD0Entries = (nEXIFTags ? 1 : 0) +
                           (nGPSTags ? 1: 0) +
                           static_cast<GUInt16>(mainTags.size());

    GUInt32 nBufferSize =
        EXIF_HEADER_SIZE + // Exif header
        4 + // Tiff signature
        4 + // Offset of IFD0
        2 + // Number of entries of IFD0
        nIFD0Entries * TAG_SIZE + // Entries of IFD0
        nOfflineSizeMain;

    if( nEXIFTags )
    {
        nBufferSize +=
            2 + // Number of entries of private EXIF IFD
            nEXIFTags * TAG_SIZE + nOfflineSizeEXIF;
    }

    if( nGPSTags )
    {
        nBufferSize +=
            2 + // Number of entries of private GPS IFD
            nGPSTags * TAG_SIZE + nOfflineSizeGPS;
    }

    GUInt16 nIFD1Entries = 0;
    if( pabyThumbnail )
    {
        nIFD1Entries = 5;
        nBufferSize += 4 + // Offset of IFD1
                       2 + // Number of entries of IFD1
                       nIFD1Entries * TAG_SIZE + // Entries of IFD1
                       nThumbnailSize;
    }
    nBufferSize += 4; // Offset of next IFD

    GByte* pabyData = nullptr;
    if( nBufferSize > 65536 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                "Cannot write EXIF segment. "
                "The size of the EXIF segment exceeds 65536 bytes");
    }
    else
    {
        pabyData = static_cast<GByte*>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    }
    if( pabyData == nullptr )
    {
        FreeTags(mainTags);
        FreeTags(exifTags);
        FreeTags(gpsTags);
        return nullptr;
    }

    memcpy(pabyData, "Exif\0\0", EXIF_HEADER_SIZE);
    GUInt32 nBufferOff = EXIF_HEADER_SIZE;
    GUInt32 nTIFFStartOff = nBufferOff;

    // TIFF little-endian signature.
    const GUInt16 TIFF_LITTLEENDIAN = 0x4949;
    WriteLEUInt16(pabyData, nBufferOff, TIFF_LITTLEENDIAN);
    const GUInt16 TIFF_VERSION = 42;
    WriteLEUInt16(pabyData, nBufferOff, TIFF_VERSION);

    // Offset of IFD0
    WriteLEUInt32(pabyData, nBufferOff, nBufferOff - nTIFFStartOff + 4);

    // Number of entries of IFD0
    WriteLEUInt16(pabyData, nBufferOff, nIFD0Entries);

    if( !mainTags.empty() )
    {
        GUInt32 offsetIFDData =
                    nBufferOff - nTIFFStartOff + nIFD0Entries * TAG_SIZE + 4;
        WriteTags(pabyData, nBufferOff, offsetIFDData, mainTags);
    }

    GUInt32 nEXIFIFDOffset = 0;
    if( nEXIFTags )
    {
        WriteTag(pabyData, nBufferOff, EXIFOFFSETTAG, TIFF_LONG, 1, 0);
        nEXIFIFDOffset = nBufferOff - 4;
    }

    GUInt32 nGPSIFDOffset = 0;
    if( nGPSTags )
    {
        WriteTag(pabyData, nBufferOff, GPSOFFSETTAG, TIFF_LONG, 1, 0);
        nGPSIFDOffset = nBufferOff - 4; // offset to patch
    }

    // Offset of next IFD
    GUInt32 nOffsetOfIFDAfterIFD0 = nBufferOff;
    WriteLEUInt32(pabyData, nBufferOff, 0); // offset to patch

    // Space for offline tag values (already written)
    nBufferOff += nOfflineSizeMain;

    if( nEXIFTags )
    {
        // Patch value of EXIFOFFSETTAG
        {
            GUInt32 nTmp = nEXIFIFDOffset;
            WriteLEUInt32(pabyData, nTmp, nBufferOff - nTIFFStartOff);
        }

        // Number of entries of EXIF IFD
        WriteLEUInt16(pabyData, nBufferOff, nEXIFTags);

        GUInt32 offsetIFDData =
                    nBufferOff - nTIFFStartOff + nEXIFTags * TAG_SIZE;
        WriteTags(pabyData, nBufferOff, offsetIFDData, exifTags);

        // Space for offline tag values (already written)
        nBufferOff += nOfflineSizeEXIF;
    }

    if( nGPSTags )
    {
        // Patch value of GPSOFFSETTAG
        {
            GUInt32 nTmp = nGPSIFDOffset;
            WriteLEUInt32(pabyData, nTmp, nBufferOff - nTIFFStartOff);
        }

        // Number of entries of GPS IFD
        WriteLEUInt16(pabyData, nBufferOff, nGPSTags);

        GUInt32 offsetIFDData =
                    nBufferOff - nTIFFStartOff + nGPSTags * TAG_SIZE;
        WriteTags(pabyData, nBufferOff, offsetIFDData, gpsTags);

        // Space for offline tag values (already written)
        nBufferOff += nOfflineSizeGPS;
    }

    if( nIFD1Entries )
    {
        // Patch value of offset after next IFD
        {
            GUInt32 nTmp = nOffsetOfIFDAfterIFD0;
            WriteLEUInt32(pabyData, nTmp, nBufferOff - nTIFFStartOff);
        }

        // Number of entries of IFD1
        WriteLEUInt16(pabyData, nBufferOff, nIFD1Entries);

        const GUInt16 JPEG_TIFF_IMAGEWIDTH      = 0x100;
        const GUInt16 JPEG_TIFF_IMAGEHEIGHT     = 0x101;
        const GUInt16 JPEG_TIFF_COMPRESSION     = 0x103;
        const GUInt16 JPEG_EXIF_JPEGIFOFSET     = 0x201;
        const GUInt16 JPEG_EXIF_JPEGIFBYTECOUNT = 0x202;

        WriteTag(pabyData, nBufferOff, JPEG_TIFF_IMAGEWIDTH, TIFF_LONG, 1,
                 nThumbnailWidth);
        WriteTag(pabyData, nBufferOff, JPEG_TIFF_IMAGEHEIGHT, TIFF_LONG, 1,
                 nThumbnailHeight);
        WriteTag(pabyData, nBufferOff, JPEG_TIFF_COMPRESSION, TIFF_SHORT, 1,
                 6); // JPEG compression
        WriteTag(pabyData, nBufferOff, JPEG_EXIF_JPEGIFOFSET, TIFF_LONG, 1,
                 nBufferSize - EXIF_HEADER_SIZE - nThumbnailSize);
        WriteTag(pabyData, nBufferOff, JPEG_EXIF_JPEGIFBYTECOUNT, TIFF_LONG, 1,
                 nThumbnailSize);

        // Offset of next IFD
        WriteLEUInt32(pabyData, nBufferOff, 0);
    }

    CPLAssert( nBufferOff + nThumbnailSize == nBufferSize );
    if( pabyThumbnail != nullptr && nThumbnailSize )
        memcpy(pabyData + nBufferOff, pabyThumbnail, nThumbnailSize );

    FreeTags(mainTags);
    FreeTags(exifTags);
    FreeTags(gpsTags);

    *pnOutBufferSize = nBufferSize;
    return pabyData;
}

#ifdef DUMP_EXIF_ITEMS

// To help generate the doc page
// g++ -DDUMP_EXIF_ITEMS gcore/gdalexif.cpp -o dumpexif -Iport -Igcore -Iogr -L. -lgdal

int main()
{
    printf("<table border=\"1\">\n"); /* ok */
    printf("<tr><th>Metadata item name</th><th>Hex code</th>" /* ok */
           "<th>Type</th><th>Number of values</th><th>Optionality</th></tr>\n");
    for(size_t i = 0; exiftags[i].name[0] != '\0'; i++ )
    {
        if( exiftags[i].datatype == TIFF_NOTYPE )
            continue;
        printf("<tr><td>%s</td><td>0x%04X</td><td>%s</td><td>%s</td><td>%s</td></tr>\n", /* ok */
               exiftags[i].name,
               exiftags[i].tag,
               exiftags[i].datatype == TIFF_BYTE ?      "BYTE" :
               exiftags[i].datatype == TIFF_ASCII ?     "ASCII" :
               exiftags[i].datatype == TIFF_UNDEFINED ? "UNDEFINED" :
               exiftags[i].datatype == TIFF_SHORT ?     "SHORT" :
               exiftags[i].datatype == TIFF_LONG ?      "LONG" :
               exiftags[i].datatype == TIFF_RATIONAL ?  "RATIONAL" :
               exiftags[i].datatype == TIFF_SRATIONAL ? "SRATIONAL" :
                                                        "?????",
               exiftags[i].length ?
                    CPLSPrintf("%d", exiftags[i].length) :
                    "variable",
               exiftags[i].comprCond == COND_MANDATORY ?   "<b>Mandatory</b>" :
               exiftags[i].comprCond == COND_OPTIONAL ?    "Optional" :
               exiftags[i].comprCond == COND_RECOMMENDED ? "Recommended" :
                                                           "?????"
        );
    }
    printf("</table>\n"); /* ok */

    printf("<table border=\"1\">\n"); /* ok */
    printf("<tr><th>Metadata item name</th><th>Hex code</th>" /* ok */
           "<th>Type</th><th>Number of values</th><th>Optionality</th></tr>\n");
    for(size_t i = 0; gpstags[i].name[0] != '\0'; i++ )
    {
        if( gpstags[i].datatype == TIFF_NOTYPE )
            continue;
        printf("<tr><td>%s</td><td>0x%04X</td><td>%s</td><td>%s</td><td>%s</td></tr>\n", /* ok */
               gpstags[i].name,
               gpstags[i].tag,
               gpstags[i].datatype == TIFF_BYTE ?      "BYTE" :
               gpstags[i].datatype == TIFF_ASCII ?     "ASCII" :
               gpstags[i].datatype == TIFF_UNDEFINED ? "UNDEFINED" :
               gpstags[i].datatype == TIFF_SHORT ?     "SHORT" :
               gpstags[i].datatype == TIFF_LONG ?      "LONG" :
               gpstags[i].datatype == TIFF_RATIONAL ?  "RATIONAL" :
               gpstags[i].datatype == TIFF_SRATIONAL ? "SRATIONAL" :
                                                        "?????",
               gpstags[i].length ?
                    CPLSPrintf("%d", gpstags[i].length) :
                    "variable",
               gpstags[i].comprCond == COND_MANDATORY ?   "<b>Mandatory</b>" :
               gpstags[i].comprCond == COND_OPTIONAL ?    "Optional" :
               gpstags[i].comprCond == COND_RECOMMENDED ? "Recommended" :
                                                           "?????"
        );
    }
    printf("</table>\n"); /* ok */

    return 0;
}

#endif
