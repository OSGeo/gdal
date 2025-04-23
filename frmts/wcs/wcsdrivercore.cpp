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
 * SPDX-License-Identifier: MIT
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
