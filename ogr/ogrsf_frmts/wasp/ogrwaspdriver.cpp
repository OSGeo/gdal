/******************************************************************************
 *
 * Project:  WAsP Translator
 * Purpose:  Implements OGRWAsPDriver.
 * Author:   Vincent Mora, vincent dot mora at oslandia dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Oslandia <info at oslandia dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrwasp.h"
#include "cpl_conv.h"
#include <cassert>

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRWAsPDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update)
    {
        return nullptr;
    }

    if (!poOpenInfo->IsExtensionEqualToCI("map"))
    {
        return nullptr;
    }

    VSILFILE *fh = VSIFOpenL(poOpenInfo->pszFilename, "r");
    if (!fh)
    {
        /*CPLError( CE_Failure, CPLE_FileIO, "cannot open file %s", pszFilename
         * );*/
        return nullptr;
    }
    auto pDataSource =
        std::make_unique<OGRWAsPDataSource>(poOpenInfo->pszFilename, fh);

    if (pDataSource->Load(true) != OGRERR_NONE)
    {
        return nullptr;
    }
    return pDataSource.release();
}

/************************************************************************/
/*                         OGRWAsPDriverCreate()                        */
/************************************************************************/

static GDALDataset *OGRWAsPDriverCreate(const char *pszName, int, int, int,
                                        GDALDataType, char **)

{
    VSILFILE *fh = VSIFOpenL(pszName, "w");
    if (!fh)
    {
        CPLError(CE_Failure, CPLE_FileIO, "cannot open file %s", pszName);
        return nullptr;
    }
    return new OGRWAsPDataSource(pszName, fh);
}

/************************************************************************/
/*                         OGRWAsPDriverDelete()                        */
/************************************************************************/

static CPLErr OGRWAsPDriverDelete(const char *pszName)

{
    return VSIUnlink(pszName) == 0 ? CE_None : CE_Failure;
}

/************************************************************************/
/*                           RegisterOGRWAsP()                          */
/************************************************************************/

void RegisterOGRWAsP()

{
    if (GDALGetDriverByName("WAsP"))
        return;

    auto poDriver = new GDALDriver();
    poDriver->SetDescription("WAsP");

    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "WAsP .map format");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "map");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/wasp.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = OGRWAsPDriverOpen;
    poDriver->pfnCreate = OGRWAsPDriverCreate;
    poDriver->pfnDelete = OGRWAsPDriverDelete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
