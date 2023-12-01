/******************************************************************************
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Definition of classes for OGR MSSQL Spatial driver.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
