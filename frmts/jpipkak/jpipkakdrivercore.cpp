/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  JPIPKAK driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
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

#include "jpipkakdrivercore.h"

/************************************************************************/
/*                       JPIPKAKDatasetIdentify()                       */
/************************************************************************/

static int JPIPKAKDatasetIdentify(GDALOpenInfo *poOpenInfo)
{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "jpip://") ||
           STARTS_WITH_CI(poOpenInfo->pszFilename, "jpips://");
}

/************************************************************************/
/*                   JPIPKAKDriverSetCommonMetadata()                   */
/************************************************************************/

void JPIPKAKDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "JPIP (based on Kakadu)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/jpipkak.html");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jpp-stream");

    poDriver->pfnIdentify = JPIPKAKDatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                   DeclareDeferredJPIPKAKPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredJPIPKAKPlugin()
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
    JPIPKAKDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
