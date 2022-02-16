/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaDriver functions implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
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

#include "cpl_conv.h"
#include "ogr_hana.h"

#include <memory>

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRHanaDriverIdentify()                      */
/************************************************************************/

static int OGRHanaDriverIdentify(GDALOpenInfo* openInfo)
{
    return STARTS_WITH_CI(
        openInfo->pszFilename, OGRHanaDataSource::GetPrefix());
}

/************************************************************************/
/*                         OGRHanaDriverOpen()                          */
/************************************************************************/

static GDALDataset* OGRHanaDriverOpen(GDALOpenInfo* openInfo)
{
    if (!OGRHanaDriverIdentify(openInfo))
        return nullptr;

    auto ds = cpl::make_unique<OGRHanaDataSource>();
    if (!ds->Open(
            openInfo->pszFilename, openInfo->papszOpenOptions,
            openInfo->eAccess == GA_Update))
        return nullptr;
    return ds.release();
}

/************************************************************************/
/*                        OGRHanaDriverCreate()                         */
/************************************************************************/

static GDALDataset* OGRHanaDriverCreate(
    const char* name,
    CPL_UNUSED int nBands,
    CPL_UNUSED int nXSize,
    CPL_UNUSED int nYSize,
    CPL_UNUSED GDALDataType eDT,
    CPL_UNUSED char** options)
{
    auto ds = cpl::make_unique<OGRHanaDataSource>();
    if (!ds->Open(name, options, TRUE))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
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

    if (GDALGetDriverByName("HANA") != nullptr)
        return;

    auto driver = cpl::make_unique<GDALDriver>();
    driver->SetDescription("HANA");
    driver->SetMetadataItem(GDAL_DMD_LONGNAME, "SAP HANA");
    driver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    driver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/hana.html");
    driver->SetMetadataItem(
        GDAL_DMD_CONNECTION_PREFIX, OGRHanaDataSource::GetPrefix());
    driver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST, OGRHanaDataSource::GetOpenOptions());
    driver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList/>");
    driver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        OGRHanaDataSource::GetLayerCreationOptions());
    driver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        OGRHanaDataSource::GetSupportedDataTypes());
    driver->SetMetadataItem(GDAL_DCAP_NOTNULL_FIELDS, "YES");
    driver->SetMetadataItem(GDAL_DCAP_DEFAULT_FIELDS, "YES");

    driver->pfnOpen = OGRHanaDriverOpen;
    driver->pfnIdentify = OGRHanaDriverIdentify;
    driver->pfnCreate = OGRHanaDriverCreate;

    GetGDALDriverManager()->RegisterDriver(driver.release());
}
