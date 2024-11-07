/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_pg.h"
#include "cpl_conv.h"

#include "ogrpgdrivercore.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRPGDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRPGDriverIdentify(poOpenInfo))
        return nullptr;

    OGRPGDataSource *poDS = new OGRPGDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update,
                    TRUE, poOpenInfo->papszOpenOptions))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

static GDALDataset *
OGRPGDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                  CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                  CPL_UNUSED GDALDataType eDT, char **papszOptions)

{
    OGRPGDataSource *poDS = new OGRPGDataSource();

    if (!poDS->Open(pszName, TRUE, TRUE, papszOptions))
    {
        delete poDS;
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "PostgreSQL driver doesn't currently support database creation.\n"
            "Please create database with the `createdb' command.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRPG()                            */
/************************************************************************/

void RegisterOGRPG()

{
    if (!GDAL_CHECK_VERSION("PG driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRPGDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGRPGDriverOpen;
    poDriver->pfnCreate = OGRPGDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
