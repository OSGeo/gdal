/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  OGRAVCE00Driver implementation (Arc/Info E00ary Coverages)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_avc.h"

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

static int OGRAVCE00DriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (!poOpenInfo->IsExtensionEqualToCI("E00"))
        return FALSE;

    if (poOpenInfo->nHeaderBytes == 0)
        return FALSE;

    if (!(STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "EXP  0") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "EXP  1")))
        return FALSE;

    if (strstr((const char *)poOpenInfo->pabyHeader, "GRD  2") != nullptr ||
        strstr((const char *)poOpenInfo->pabyHeader, "GRD  3") != nullptr)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRAVCE00DriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRAVCE00DriverIdentify(poOpenInfo))
        return nullptr;
    if (poOpenInfo->eAccess == GA_Update)
        return nullptr;

    OGRAVCE00DataSource *poDSE00 = new OGRAVCE00DataSource();

    if (poDSE00->Open(poOpenInfo->pszFilename, TRUE) &&
        poDSE00->GetLayerCount() > 0)
    {
        return poDSE00;
    }
    delete poDSE00;

    return nullptr;
}

/************************************************************************/
/*                           RegisterOGRAVC()                           */
/************************************************************************/

void RegisterOGRAVCE00()

{
    if (GDALGetDriverByName("AVCE00") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("AVCE00");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Arc/Info E00 (ASCII) Coverage");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "e00");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/avce00.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnIdentify = OGRAVCE00DriverIdentify;
    poDriver->pfnOpen = OGRAVCE00DriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
