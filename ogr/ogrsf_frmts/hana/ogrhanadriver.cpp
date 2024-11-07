/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaDriver functions implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_hana.h"
#include "ogrhanadrivercore.h"

#include <memory>

/************************************************************************/
/*                         OGRHanaDriverOpen()                          */
/************************************************************************/

static GDALDataset *OGRHanaDriverOpen(GDALOpenInfo *openInfo)
{
    if (!OGRHanaDriverIdentify(openInfo))
        return nullptr;

    auto ds = std::make_unique<OGRHanaDataSource>();
    if (!ds->Open(openInfo->pszFilename, openInfo->papszOpenOptions,
                  openInfo->eAccess == GA_Update))
        return nullptr;
    return ds.release();
}

/************************************************************************/
/*                        OGRHanaDriverCreate()                         */
/************************************************************************/

static GDALDataset *OGRHanaDriverCreate(const char *name, CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        CPL_UNUSED char **options)
{
    auto ds = std::make_unique<OGRHanaDataSource>();
    if (!ds->Open(name, options, TRUE))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HANA driver doesn't currently support database creation.\n"
                 "Please create a database with SAP HANA tools before using.");

        return nullptr;
    }
    return ds.release();
}

/************************************************************************/
/*                          RegisterOGRHANA()                           */
/************************************************************************/

void RegisterOGRHANA()
{
    if (!GDAL_CHECK_VERSION("SAP HANA driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    auto driver = std::make_unique<GDALDriver>();
    OGRHANADriverSetCommonMetadata(driver.get());
    driver->pfnOpen = OGRHanaDriverOpen;
    driver->pfnCreate = OGRHanaDriverCreate;

    GetGDALDriverManager()->RegisterDriver(driver.release());
}
