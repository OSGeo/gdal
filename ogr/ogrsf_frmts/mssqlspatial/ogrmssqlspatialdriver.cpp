/******************************************************************************
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Definition of classes for OGR MSSQL Spatial driver.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_mssqlspatial.h"
#include "cpl_conv.h"
#include "ogrmssqlspatialdrivercore.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRMSSQLSpatialDriverOpen(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;
    const bool bUpdate = (poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0;
    OGRMSSQLSpatialDataSource *poDS;

    if (!OGRMSSQLSPATIALDriverIdentify(poOpenInfo))
        return nullptr;

    poDS = new OGRMSSQLSpatialDataSource();

    if (!poDS->Open(pszFilename, bUpdate, TRUE))
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

static GDALDataset *OGRMSSQLSpatialDriverCreateDataSource(
    const char *pszName, CPL_UNUSED int nBands, CPL_UNUSED int nXSize,
    CPL_UNUSED int nYSize, CPL_UNUSED GDALDataType eDT,
    CPL_UNUSED char **papszOptions)
{
    if (!STARTS_WITH_CI(pszName, "MSSQL:"))
        return nullptr;

    OGRMSSQLSpatialDataSource *poDS = new OGRMSSQLSpatialDataSource();
    if (!poDS->Open(pszName, TRUE, TRUE))
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MSSQL Spatial driver doesn't currently support database "
                 "creation.\n"
                 "Please create database with the Microsoft SQL Server Client "
                 "Tools.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRMSSQLSpatial()                  */
/************************************************************************/

void RegisterOGRMSSQLSpatial()

{
    if (!GDAL_CHECK_VERSION("OGR/MSSQLSpatial driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRMSSQLSPATIALDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRMSSQLSpatialDriverOpen;
    poDriver->pfnCreate = OGRMSSQLSpatialDriverCreateDataSource;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
