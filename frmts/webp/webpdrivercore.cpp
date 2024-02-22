/******************************************************************************
 *
 * Project:  GDAL WEBP Driver
 * Purpose:  Implement GDAL WEBP Support based on libwebp
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "webp_headers.h"
#include "webpdrivercore.h"

/************************************************************************/
/*                     WEBPDriverIdentify()                             */
/************************************************************************/

int WEBPDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    const int nHeaderBytes = poOpenInfo->nHeaderBytes;
    const GByte *pabyHeader = poOpenInfo->pabyHeader;

    if (nHeaderBytes < 20)
        return FALSE;

    return memcmp(pabyHeader, "RIFF", 4) == 0 &&
           memcmp(pabyHeader + 8, "WEBP", 4) == 0 &&
           (memcmp(pabyHeader + 12, "VP8 ", 4) == 0 ||
            memcmp(pabyHeader + 12, "VP8L", 4) == 0 ||
            memcmp(pabyHeader + 12, "VP8X", 4) == 0);
}

/************************************************************************/
/*                      WEBPDriverSetCommonMetadata()                   */
/************************************************************************/

void WEBPDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "WEBP");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/webp.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "webp");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/webp");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='QUALITY' type='float' description='good=100, bad=0' "
        "default='75'/>\n"
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
        "   <Option name='LOSSLESS' type='boolean' description='Whether "
        "lossless compression should be used' default='FALSE'/>\n"
#endif
        "   <Option name='LOSSLESS_COPY' type='string-select' "
        "description='Whether conversion should be lossless' default='AUTO'>"
        "     <Value>AUTO</Value>"
        "     <Value>YES</Value>"
        "     <Value>NO</Value>"
        "   </Option>"
        "   <Option name='PRESET' type='string-select' description='kind of "
        "image' default='DEFAULT'>\n"
        "       <Value>DEFAULT</Value>\n"
        "       <Value>PICTURE</Value>\n"
        "       <Value>PHOTO</Value>\n"
        "       <Value>DRAWING</Value>\n"
        "       <Value>ICON</Value>\n"
        "       <Value>TEXT</Value>\n"
        "   </Option>\n"
        "   <Option name='TARGETSIZE' type='int' description='if non-zero, "
        "desired target size in bytes. Has precedence over QUALITY'/>\n"
        "   <Option name='PSNR' type='float' description='if non-zero, minimal "
        "distortion to to achieve. Has precedence over TARGETSIZE'/>\n"
        "   <Option name='METHOD' type='int' description='quality/speed "
        "trade-off. fast=0, slower-better=6' default='4'/>\n"
        "   <Option name='SEGMENTS' type='int' description='maximum number of "
        "segments [1-4]' default='4'/>\n"
        "   <Option name='SNS_STRENGTH' type='int' description='Spatial Noise "
        "Shaping. off=0, maximum=100' default='50'/>\n"
        "   <Option name='FILTER_STRENGTH' type='int' description='Filter "
        "strength. off=0, strongest=100' default='20'/>\n"
        "   <Option name='FILTER_SHARPNESS' type='int' description='Filter "
        "sharpness. off=0, least sharp=7' default='0'/>\n"
        "   <Option name='FILTER_TYPE' type='int' description='Filtering type. "
        "simple=0, strong=1' default='0'/>\n"
        "   <Option name='AUTOFILTER' type='int' description=\"Auto adjust "
        "filter's strength. off=0, on=1\" default='0'/>\n"
        "   <Option name='PASS' type='int' description='Number of entropy "
        "analysis passes [1-10]' default='1'/>\n"
        "   <Option name='PREPROCESSING' type='int' description='Preprocessing "
        "filter. none=0, segment-smooth=1' default='0'/>\n"
        "   <Option name='PARTITIONS' type='int' description='log2(number of "
        "token partitions) in [0..3]' default='0'/>\n"
#if WEBP_ENCODER_ABI_VERSION >= 0x0002
        "   <Option name='PARTITION_LIMIT' type='int' description='quality "
        "degradation allowed to fit the 512k limit on prediction modes coding "
        "(0=no degradation, 100=full)' default='0'/>\n"
#endif
#if WEBP_ENCODER_ABI_VERSION >= 0x0209
        "   <Option name='EXACT' type='int' description='preserve the exact "
        "RGB values under transparent area. off=0, on=1' default='0'/>\n"
#endif
        "</CreationOptionList>\n");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = WEBPDriverIdentify;

    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredWEBPPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredWEBPPlugin()
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
    WEBPDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
