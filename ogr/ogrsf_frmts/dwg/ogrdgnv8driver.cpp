/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGNv8
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dgnv8.h"
#include "cpl_conv.h"
#include "ogrteigha.h"
#include "ogrdgnv8drivercore.h"

/************************************************************************/
/*                         OGRDGNV8DriverUnload()                       */
/************************************************************************/

static void OGRDGNV8DriverUnload(GDALDriver *)
{
    CPLDebug("DGNv8", "Driver cleanup");
    OGRTEIGHADeinitialize();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDGNV8DriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRDGNV8DriverIdentify(poOpenInfo))
        return nullptr;

    if (!OGRTEIGHAInitialize())
        return nullptr;

    OGRDGNV8DataSource *poDS = new OGRDGNV8DataSource(OGRDGNV8GetServices());
    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRDGNV8DriverCreate(const char *pszName, int /* nBands */,
                                         int /* nXSize */, int /* nYSize */,
                                         GDALDataType /* eDT */,
                                         char **papszOptions)
{
    if (!OGRTEIGHAInitialize())
        return nullptr;

    OGRDGNV8DataSource *poDS = new OGRDGNV8DataSource(OGRDGNV8GetServices());
    if (!poDS->PreCreate(pszName, papszOptions))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                         RegisterOGRDGNV8()                           */
/************************************************************************/

void RegisterOGRDGNV8()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRDGNV8DriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRDGNV8DriverOpen;
    poDriver->pfnCreate = OGRDGNV8DriverCreate;
    poDriver->pfnUnloadDriver = OGRDGNV8DriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
