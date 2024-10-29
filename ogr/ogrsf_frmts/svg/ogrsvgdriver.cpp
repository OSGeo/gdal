/******************************************************************************
 *
 * Project:  SVG Translator
 * Purpose:  Implements OGRSVGDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_svg.h"
#include "cpl_conv.h"

CPL_C_START
void RegisterOGRSVG();
CPL_C_END

// g++ -g -Wall -fPIC ogr/ogrsf_frmts/svg/*.c* -shared -o ogr_SVG.so -Iport
// -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/svg -L. -lgdal -DHAVE_EXPAT

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRSVGDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update || poOpenInfo->fpL == nullptr)
        return nullptr;

    if (strstr((const char *)poOpenInfo->pabyHeader, "<svg") == nullptr)
        return nullptr;

    OGRSVGDataSource *poDS = new OGRSVGDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRSVG()                           */
/************************************************************************/

void RegisterOGRSVG()

{
    if (!GDAL_CHECK_VERSION("OGR/SVG driver"))
        return;

    if (GDALGetDriverByName("SVG") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("SVG");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Scalable Vector Graphics");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "svg");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/svg.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = OGRSVGDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
