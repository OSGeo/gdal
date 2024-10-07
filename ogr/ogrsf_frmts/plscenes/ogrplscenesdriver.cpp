/******************************************************************************
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  PlanetLabs scene driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015-2016, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_plscenes.h"
#include "ogrplscenesdrivercore.h"

/************************************************************************/
/*                            OGRPLScenesOpen()                         */
/************************************************************************/

static GDALDataset *OGRPLScenesOpen(GDALOpenInfo *poOpenInfo)
{
    if (!OGRPLSCENESDriverIdentify(poOpenInfo) ||
        poOpenInfo->eAccess == GA_Update)
        return nullptr;

    char **papszOptions = CSLTokenizeStringComplex(
        poOpenInfo->pszFilename + strlen("PLScenes:"), ",", TRUE, FALSE);
    CPLString osVersion = CSLFetchNameValueDef(
        papszOptions, "version",
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "VERSION", ""));
    CSLDestroy(papszOptions);

    if (EQUAL(osVersion, "v0") || EQUAL(osVersion, "v1"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This API version has been removed or deprecated. "
                 "Please use DATA_V1 API instead");
        return nullptr;
    }
    else if (EQUAL(osVersion, "data_v1") || EQUAL(osVersion, ""))
    {
        return OGRPLScenesDataV1Dataset::Open(poOpenInfo);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unhandled API version: %s",
                 osVersion.c_str());
        return nullptr;
    }
}

/************************************************************************/
/*                        RegisterOGRPLSCENES()                         */
/************************************************************************/

void RegisterOGRPLSCENES()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRPLSCENESDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRPLScenesOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
