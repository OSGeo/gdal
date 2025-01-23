/******************************************************************************
 *
 * Project:  Interlis 2 Translator
 * Purpose:  Implements OGRILI2Layer class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "xercesc_headers.h"

#include "cpl_conv.h"
#include "ogr_ili2.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRILI2DriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update ||
        (!poOpenInfo->bStatOK &&
         strchr(poOpenInfo->pszFilename, ',') == nullptr))
        return nullptr;

    if (poOpenInfo->pabyHeader != nullptr)
    {
        if (poOpenInfo->pabyHeader[0] != '<' ||
            strstr((const char *)poOpenInfo->pabyHeader,
                   "interlis.ch/INTERLIS2") == nullptr)
        {
            return nullptr;
        }
    }
    else if (poOpenInfo->bIsDirectory)
        return nullptr;

    OGRILI2DataSource *poDS = new OGRILI2DataSource();

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
/*                           RegisterOGRILI2()                           */
/************************************************************************/

void RegisterOGRILI2()
{
    if (GDALGetDriverByName("Interlis 2") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("Interlis 2");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Interlis 2");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/ili.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "xtf xml ili");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='MODEL' type='string' description='Filename of the "
        "model in IlisMeta format (.imd)'/>"
        "</OpenOptionList>");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = OGRILI2DriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
