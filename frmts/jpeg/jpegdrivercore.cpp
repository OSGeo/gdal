/******************************************************************************
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement GDAL JPEG Support based on IJG libjpeg.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "jpegdrivercore.h"

// So that D_LOSSLESS_SUPPORTED is visible if defined in jmorecfg of libjpeg-turbo >= 2.2
#define JPEG_INTERNAL_OPTIONS
#include "jpeglib.h"

/************************************************************************/
/*                    JPEGDatasetIsJPEGLS()                             */
/************************************************************************/

bool JPEGDatasetIsJPEGLS(GDALOpenInfo *poOpenInfo)

{
    GByte *pabyHeader = poOpenInfo->pabyHeader;
    int nHeaderBytes = poOpenInfo->nHeaderBytes;

    if (nHeaderBytes < 10)
        return false;

    if (pabyHeader[0] != 0xff || pabyHeader[1] != 0xd8)
        return false;

    for (int nOffset = 2; nOffset + 4 < nHeaderBytes;)
    {
        if (pabyHeader[nOffset] != 0xFF)
            return false;

        int nMarker = pabyHeader[nOffset + 1];
        if (nMarker == 0xDA)
            return false;

        if (nMarker == 0xF7)  // JPEG Extension 7, JPEG-LS.
            return true;
        if (nMarker == 0xF8)  // JPEG Extension 8, JPEG-LS Extension.
            return true;
        if (nMarker == 0xC3)  // Start of Frame 3 (Lossless Huffman)
            return true;
        if (nMarker ==
            0xC7)  // Start of Frame 7 (Differential Lossless Huffman)
            return true;
        if (nMarker == 0xCB)  // Start of Frame 11 (Lossless Arithmetic)
            return true;
        if (nMarker ==
            0xCF)  // Start of Frame 15 (Differential Lossless Arithmetic)
            return true;

        nOffset += 2 + pabyHeader[nOffset + 2] * 256 + pabyHeader[nOffset + 3];
    }

    return false;
}

/************************************************************************/
/*                     JPEGDriverIdentify()                             */
/************************************************************************/

int JPEGDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    // If it is a subfile, read the JPEG header.
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "JPEG_SUBFILE:"))
        return TRUE;
    if (STARTS_WITH(poOpenInfo->pszFilename, "JPEG:"))
        return TRUE;

    // First we check to see if the file has the expected header bytes.
    const int nHeaderBytes = poOpenInfo->nHeaderBytes;

    if (nHeaderBytes < 10)
        return FALSE;

    GByte *const pabyHeader = poOpenInfo->pabyHeader;
    if (pabyHeader[0] != 0xff || pabyHeader[1] != 0xd8 || pabyHeader[2] != 0xff)
        return FALSE;

        // libjpeg-turbo >= 2.2 supports lossless mode
#if !defined(D_LOSSLESS_SUPPORTED)
    if (JPEGDatasetIsJPEGLS(poOpenInfo))
    {
        return FALSE;
    }
#endif

    // Some files like
    // http://dionecanali.hd.free.fr/~mdione/mapzen/N65E039.hgt.gz could be
    // mis-identfied as JPEG
    CPLString osFilenameLower = CPLString(poOpenInfo->pszFilename).tolower();
    if (osFilenameLower.endsWith(".hgt") ||
        osFilenameLower.endsWith(".hgt.gz") ||
        osFilenameLower.endsWith(".hgt.zip"))
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                     JPEGDriverSetCommonMetadata()                    */
/************************************************************************/

void JPEGDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "JPEG JFIF");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/jpeg.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jpg");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "jpg jpeg");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jpeg");

#if defined(JPEG_LIB_MK1_OR_12BIT) || defined(JPEG_DUAL_MODE_8_12)
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte UInt16");
#else
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte");
#endif
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    const char *pszOpenOptions =
        "<OpenOptionList>\n"
        "   <Option name='USE_INTERNAL_OVERVIEWS' type='boolean' "
        "description='whether to use implicit internal overviews' "
        "default='YES'/>\n"
        "   <Option name='APPLY_ORIENTATION' type='boolean' "
        "description='whether to take into account EXIF Orientation to "
        "rotate/flip the image' default='NO'/>\n"
        "</OpenOptionList>\n";
    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST, pszOpenOptions);

#ifdef D_LOSSLESS_SUPPORTED
    // For autotest purposes
    poDriver->SetMetadataItem("LOSSLESS_JPEG_SUPPORTED", "YES", "JPEG");
#endif

    poDriver->pfnIdentify = JPEGDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                    DeclareDeferredJPEGPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredJPEGPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    JPEGDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
