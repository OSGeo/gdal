/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WCS.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "wcsdrivercore.h"

/************************************************************************/
/*                     WCSDriverIdentify()                              */
/************************************************************************/

int WCSDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      Filename is WCS:URL                                             */
    /*                                                                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes == 0 &&
        STARTS_WITH_CI((const char *)poOpenInfo->pszFilename, "WCS:"))
        return TRUE;

    /* -------------------------------------------------------------------- */
    /*      Is this a WCS_GDAL service description file or "in url"         */
    /*      equivalent?                                                     */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes == 0 &&
        STARTS_WITH_CI((const char *)poOpenInfo->pszFilename, "<WCS_GDAL>"))
        return TRUE;

    else if (poOpenInfo->nHeaderBytes >= 10 &&
             STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "<WCS_GDAL>"))
        return TRUE;

    /* -------------------------------------------------------------------- */
    /*      Is this apparently a WCS subdataset reference?                  */
    /* -------------------------------------------------------------------- */
    else if (STARTS_WITH_CI((const char *)poOpenInfo->pszFilename,
                            "WCS_SDS:") &&
             poOpenInfo->nHeaderBytes == 0)
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                      WCSDriverSetCommonMetadata()                    */
/************************************************************************/

void WCSDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "OGC Web Coverage Service");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/wcs.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnIdentify = WCSDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                     DeclareDeferredWCSPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredWCSPlugin()
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
    WCSDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
