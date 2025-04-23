/******************************************************************************
 *
 * Project:  MSG Driver
 * Purpose:  GDALDataset driver for MSG translator for read support.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "msgdrivercore.h"

/************************************************************************/
/*                     MSGDriverIdentify()                              */
/************************************************************************/

static int MSGDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return STARTS_WITH(poOpenInfo->pszFilename, "MSG(") ||
           STARTS_WITH(poOpenInfo->pszFilename, "H-000-MSG");
}

/************************************************************************/
/*                      MSGDriverSetCommonMetadata()                    */
/************************************************************************/

void MSGDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MSG HRIT Data");

    poDriver->pfnIdentify = MSGDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                     DeclareDeferredMSGPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredMSGPlugin()
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
    MSGDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
