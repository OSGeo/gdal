/******************************************************************************
 *
 * Project:  SOSI Translator
 * Purpose:  Implements OGRSOSIDriver.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

#include "ogrsosidrivercore.h"

/************************************************************************/
/*                      OGRSOSIDriverIdentify()                         */
/************************************************************************/

int OGRSOSIDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->fpL == nullptr ||
        strstr((const char *)poOpenInfo->pabyHeader, ".HODE") == nullptr)
        return FALSE;

    // TODO: add better identification
    return -1;
}

/************************************************************************/
/*                  OGRSOSIDriverSetCommonMetadata()                    */
/************************************************************************/

void OGRSOSIDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    // GDAL_DMD_CREATIONFIELDDATATYPES should also be defined if CreateField is
    // supported poDriver->SetMetadataItem( GDAL_DCAP_CREATE_FIELD, "YES" );
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Norwegian SOSI Standard");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/sosi.html");
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "<Option name='appendFieldsMap' type='string' description='Default is "
        "that all rows for equal field names will be appended in a feature, "
        "but with this parameter you select what field this should be valid "
        "for. With appendFieldsMap=f1&amp;f2, Append will be done for field f1 "
        "and f2 using a comma as delimiter.'/>"
        "</OpenOptionList>");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnIdentify = OGRSOSIDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGRSOSIPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRSOSIPlugin()
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
    OGRSOSIDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
