/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implements OGRSDTSDriver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_sdts.h"
#include "cpl_conv.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRSDTSDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "DDF"))
        return nullptr;
    if (poOpenInfo->nHeaderBytes < 10)
        return nullptr;
    const char *pachLeader = (const char *)poOpenInfo->pabyHeader;
    if ((pachLeader[5] != '1' && pachLeader[5] != '2' &&
         pachLeader[5] != '3') ||
        pachLeader[6] != 'L' || (pachLeader[8] != '1' && pachLeader[8] != ' '))
    {
        return nullptr;
    }

    OGRSDTSDataSource *poDS = new OGRSDTSDataSource();
    if (!poDS->Open(poOpenInfo->pszFilename, TRUE))
    {
        delete poDS;
        poDS = nullptr;
    }

    if (poDS != nullptr && poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "SDTS Driver doesn't support update.");
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRSDTS()                          */
/************************************************************************/

void RegisterOGRSDTS()

{
    if (GDALGetDriverByName("OGR_SDTS") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("OGR_SDTS");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "SDTS");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/sdts.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGRSDTSDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
