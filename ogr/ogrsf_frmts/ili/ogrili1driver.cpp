/******************************************************************************
 *
 * Project:  Interlis 1 Translator
 * Purpose:  Implements OGRILI1Layer class.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_ili1.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRILI1DriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update ||
        (!poOpenInfo->bStatOK &&
         strchr(poOpenInfo->pszFilename, ',') == nullptr))
        return nullptr;

    if (poOpenInfo->pabyHeader != nullptr)
    {
        if (strstr((const char *)poOpenInfo->pabyHeader, "SCNT") == nullptr)
        {
            return nullptr;
        }
    }
    else if (poOpenInfo->bIsDirectory)
        return nullptr;

    OGRILI1DataSource *poDS = new OGRILI1DataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                    TRUE) ||
        poDS->GetLayerCount() == 0)
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRILI1()                           */
/************************************************************************/

void RegisterOGRILI1()
{
    if (GDALGetDriverByName("Interlis 1") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("Interlis 1");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Interlis 1");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/ili.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "itf ili");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='MODEL' type='string' description='Filename of the "
        "model in IlisMeta format (.imd)'/>"
        "</OpenOptionList>");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = OGRILI1DriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
