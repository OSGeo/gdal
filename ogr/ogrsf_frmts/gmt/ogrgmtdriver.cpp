/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGmtDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_gmt.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                    OGRGMTDriverIdentify()                            */
/************************************************************************/

static int OGRGMTDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    const char *pszHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    if (poOpenInfo->nHeaderBytes && strstr(pszHeader, "@VGMT") != nullptr)
        return true;

    if (poOpenInfo->IsExtensionEqualToCI("GMT"))
        return true;

    return false;
}

/************************************************************************/
/*                           OGRGMTDriverOpen()                         */
/************************************************************************/

static GDALDataset *OGRGMTDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRGMTDriverIdentify(poOpenInfo))
        return nullptr;

    OGRGmtDataSource *poDS = new OGRGmtDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, nullptr, nullptr,
                    poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                         OGRGMTDriverCreate()                         */
/************************************************************************/

static GDALDataset *OGRGMTDriverCreate(const char *, CPL_UNUSED int nBands,
                                       CPL_UNUSED int nXSize,
                                       CPL_UNUSED int nYSize,
                                       CPL_UNUSED GDALDataType eDT, char **)
{
    return new OGRGmtDataSource();
}

/************************************************************************/
/*                           RegisterOGRGMT()                           */
/************************************************************************/

void RegisterOGRGMT()

{
    if (GDALGetDriverByName("OGR_GMT") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("OGR_GMT");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "GMT ASCII Vectors (.gmt)");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "gmt");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/gmt.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGRGMTDriverOpen;
    poDriver->pfnIdentify = OGRGMTDriverIdentify;
    poDriver->pfnCreate = OGRGMTDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
