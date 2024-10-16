/******************************************************************************
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCIDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_oci.h"
#include "ogrocidrivercore.h"

/************************************************************************/
/*                          OGROCIDriverOpen()                          */
/************************************************************************/

static GDALDataset *OGROCIDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGROCIDriverIdentify(poOpenInfo))
        return nullptr;

    OGROCIDataSource *poDS;

    poDS = new OGROCIDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                    poOpenInfo->eAccess == GA_Update, TRUE))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                         OGROCIDriverCreate()                         */
/************************************************************************/

static GDALDataset *
OGROCIDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                   CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                   CPL_UNUSED GDALDataType eDT, CPL_UNUSED char **papszOptions)

{
    OGROCIDataSource *poDS;

    poDS = new OGROCIDataSource();

    if (!poDS->Open(pszName, nullptr, TRUE, TRUE))
    {
        delete poDS;
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Oracle driver doesn't currently support database creation.\n"
            "Please create database with Oracle tools before loading tables.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGROCI()                            */
/************************************************************************/

void RegisterOGROCI()

{
    if (!GDAL_CHECK_VERSION("OCI driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGROCIDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGROCIDriverOpen;
    poDriver->pfnCreate = OGROCIDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
