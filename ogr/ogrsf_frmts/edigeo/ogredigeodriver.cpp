/******************************************************************************
 *
 * Project:  EDIGEO Translator
 * Purpose:  Implements OGREDIGEODriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_edigeo.h"
#include "cpl_conv.h"

extern "C" void RegisterOGREDIGEO();

// g++ -fPIC -g -Wall ogr/ogrsf_frmts/edigeo/*.cpp -shared -o ogr_EDIGEO.so
// -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/generic
// -Iogr/ogrsf_frmts/edigeo -L. -lgdal

/************************************************************************/
/*                        OGREDIGEODriverIdentify()                     */
/************************************************************************/

static int OGREDIGEODriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return poOpenInfo->fpL != nullptr &&
           poOpenInfo->IsExtensionEqualToCI("thf");
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGREDIGEODriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update ||
        !OGREDIGEODriverIdentify(poOpenInfo))
        return nullptr;

    OGREDIGEODataSource *poDS = new OGREDIGEODataSource();

    if (!poDS->Open(poOpenInfo->pszFilename))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGREDIGEO()                        */
/************************************************************************/

void RegisterOGREDIGEO()

{
    if (GDALGetDriverByName("EDIGEO") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("EDIGEO");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "French EDIGEO exchange format");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "thf");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/edigeo.html");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_READ, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGREDIGEODriverOpen;
    poDriver->pfnIdentify = OGREDIGEODriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
