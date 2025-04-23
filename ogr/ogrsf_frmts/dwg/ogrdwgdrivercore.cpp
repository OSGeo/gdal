/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Implements OGRDWGDriver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

#include "ogrdwgdrivercore.h"

/************************************************************************/
/*                         OGRDWGDriverIdentify()                       */
/************************************************************************/

int OGRDWGDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return poOpenInfo->IsExtensionEqualToCI("dwg");
}

/************************************************************************/
/*                  OGRDWGDriverSetCommonMetadata()                   */
/************************************************************************/

void OGRDWGDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DWG_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "AutoCAD DWG");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dwg");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/dwg.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_READ, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnIdentify = OGRDWGDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGRDWGPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRDWGPlugin()
{
    if (GDALGetDriverByName(DWG_DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    OGRDWGDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
