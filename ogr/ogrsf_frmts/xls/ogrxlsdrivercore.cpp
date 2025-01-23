/******************************************************************************
 *
 * Project:  XLS Translator
 * Purpose:  Implements OGRXLSDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

#include "ogrxlsdrivercore.h"

/************************************************************************/
/*                    OGRXLSDriverIdentify()                            */
/************************************************************************/

static int OGRXLSDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return poOpenInfo->IsExtensionEqualToCI("XLS");
}

/************************************************************************/
/*                  OGRXLSDriverSetCommonMetadata()                  */
/************************************************************************/

void OGRXLSDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MS Excel format");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "xls");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/xls.html");
    poDriver->SetMetadataItem(GDAL_DCAP_NONSPATIAL, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnIdentify = OGRXLSDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGRXLSPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRXLSPlugin()
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
    OGRXLSDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
