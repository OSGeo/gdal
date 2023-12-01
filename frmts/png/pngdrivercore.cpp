/******************************************************************************
 *
 * Project:  PNG Driver
 * Purpose:  Implement GDAL PNG Support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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
 ******************************************************************************/

#include "pngdrivercore.h"

/************************************************************************/
/*                     PNGDriverIdentify()                              */
/************************************************************************/

int PNGDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    constexpr GByte png_signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (poOpenInfo->fpL == nullptr ||
        poOpenInfo->nHeaderBytes < static_cast<int>(sizeof(png_signature)))
        return FALSE;

    if (memcmp(poOpenInfo->pabyHeader, png_signature, sizeof(png_signature)) !=
        0)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                      PNGDriverSetCommonMetadata()                    */
/************************************************************************/

void PNGDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Portable Network Graphics");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/png.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "png");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/png");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte UInt16");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='WORLDFILE' type='boolean' description='Create world "
        "file' default='FALSE'/>\n"
        "   <Option name='ZLEVEL' type='int' description='DEFLATE compression "
        "level 1-9' default='6'/>\n"
        "   <Option name='SOURCE_ICC_PROFILE' type='string' description='ICC "
        "Profile'/>\n"
        "   <Option name='SOURCE_ICC_PROFILE_NAME' type='string' "
        "description='ICC Profile name'/>\n"
        "   <Option name='SOURCE_PRIMARIES_RED' type='string' "
        "description='x,y,1.0 (xyY) red chromaticity'/>\n"
        "   <Option name='SOURCE_PRIMARIES_GREEN' type='string' "
        "description='x,y,1.0 (xyY) green chromaticity'/>\n"
        "   <Option name='SOURCE_PRIMARIES_BLUE' type='string' "
        "description='x,y,1.0 (xyY) blue chromaticity'/>\n"
        "   <Option name='SOURCE_WHITEPOINT' type='string' "
        "description='x,y,1.0 (xyY) whitepoint'/>\n"
        "   <Option name='PNG_GAMMA' type='string' description='Gamma'/>\n"
        "   <Option name='TITLE' type='string' description='Title'/>\n"
        "   <Option name='DESCRIPTION' type='string' "
        "description='Description'/>\n"
        "   <Option name='COPYRIGHT' type='string' description='Copyright'/>\n"
        "   <Option name='COMMENT' type='string' description='Comment'/>\n"
        "   <Option name='WRITE_METADATA_AS_TEXT' type='boolean' "
        "description='Whether to write source dataset metadata in TEXT chunks' "
        "default='FALSE'/>\n"
        "   <Option name='NBITS' type='int' description='Force output bit "
        "depth: 1, 2 or 4'/>\n"
        "</CreationOptionList>\n");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = PNGDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredPNGPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredPNGPlugin()
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
    PNGDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
