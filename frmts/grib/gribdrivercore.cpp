/******************************************************************************
 *
 * Project:  GRIB Driver
 * Purpose:  GDALDataset driver for GRIB translator for read support
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2007, ITC
 * Copyright (c) 2008-2017, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gribdrivercore.h"

/************************************************************************/
/*                     GRIBDriverIdentify()                             */
/************************************************************************/

int GRIBDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes < 8)
        return FALSE;

    const char *pasHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    // Does a part of what ReadSECT0(), but in a thread-safe way.
    for (int i = 0; i < poOpenInfo->nHeaderBytes - 3; i++)
    {
        if (STARTS_WITH_CI(pasHeader + i, "GRIB")
#ifdef ENABLE_TDLP
            || STARTS_WITH_CI(pasHeader + i, "TDLP")
#endif
        )
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                     GRIBDriverSetCommonMetadata()                    */
/************************************************************************/

void GRIBDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "GRIdded Binary (.grb, .grb2)");
    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST,
                              "<OpenOptionList>"
                              "    <Option name='USE_IDX' type='boolean' "
                              "description='Load metadata from "
                              "wgrib2 index file if available' default='YES'/>"
                              "</OpenOptionList>");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/grib.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "grb grb2 grib2");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 UInt32 Int32 Float32 "
                              "Float64");

    poDriver->pfnIdentify = GRIBDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                    DeclareDeferredGRIBPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredGRIBPlugin()
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
    GRIBDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
