/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBDriver class.
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_idb.h"
#include "cpl_conv.h"

#include "ogridbdrivercore.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRIDBDriverOpen(GDALOpenInfo *poOpenInfo)

{
    OGRIDBDataSource *poDS;

    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "IDB:"))
        return nullptr;

    poDS = new OGRIDBDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename,
                    (poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0, TRUE))
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

static GDALDataset *OGRIDBDriverCreate(const char *pszName, int /* nBands */,
                                       int /* nXSize */, int /* nYSize */,
                                       GDALDataType /* eDT */,
                                       char ** /*papszOptions*/)

{
    OGRIDBDataSource *poDS;

    if (!STARTS_WITH_CI(pszName, "IDB:"))
        return nullptr;

    poDS = new OGRIDBDataSource();

    if (!poDS->Open(pszName, TRUE, TRUE))
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "IDB driver doesn't currently support database creation.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRIDB()                            */
/************************************************************************/

void RegisterOGRIDB()

{
    if (!GDAL_CHECK_VERSION("IDB driver"))
        return;
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRIDBDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRIDBDriverOpen;
    poDriver->pfnCreate = OGRIDBDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
