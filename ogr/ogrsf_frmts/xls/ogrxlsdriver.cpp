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

#include "ogr_xls.h"
#include "ogrxlsdrivercore.h"
#include "cpl_conv.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRXLSDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if ((poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0)
    {
        return nullptr;
    }

    if (!poOpenInfo->IsExtensionEqualToCI("XLS"))
    {
        return nullptr;
    }

    OGRXLSDataSource *poDS = new OGRXLSDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, false))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRXLS()                           */
/************************************************************************/

void RegisterOGRXLS()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRXLSDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRXLSDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
