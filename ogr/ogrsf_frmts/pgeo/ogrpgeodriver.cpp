/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Personal Geodatabase driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_pgeo.h"
#include "cpl_conv.h"

/************************************************************************/
/*                     OGRPGeoDriverIdentify()                          */
/************************************************************************/

static int OGRPGeoDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "PGEO:"))
        return TRUE;

    if (!poOpenInfo->IsExtensionEqualToCI("mdb"))
        return FALSE;

    // Could potentially be a PGeo or generic ODBC database
    return -1;
}

/************************************************************************/
/*                                OGRPGeoDriverOpen()                   */
/************************************************************************/

static GDALDataset *OGRPGeoDriverOpen(GDALOpenInfo *poOpenInfo)

{
    // The method might return -1 when it is undecided
    if (OGRPGeoDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

#ifndef _WIN32
    // Try to register MDB Tools driver
    CPLODBCDriverInstaller::InstallMdbToolsDriver();
#endif /* ndef WIN32 */

    // Open data source
    OGRPGeoDataSource *poDS = new OGRPGeoDataSource();

    if (!poDS->Open(poOpenInfo))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRPGeo()                          */
/************************************************************************/

void RegisterOGRPGeo()

{
    if (GDALGetDriverByName("PGeo") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver;

    poDriver->SetDescription("PGeo");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ESRI Personal GeoDatabase");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "mdb");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/pgeo.html");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FIELD_DOMAINS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_RELATIONSHIPS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_GEOMETRY_FLAGS,
                              "EquatesMultiAndSingleLineStringDuringWrite "
                              "EquatesMultiAndSinglePolygonDuringWrite");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS,
                              "NATIVE OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='LIST_ALL_TABLES' type='string-select' scope='vector' "
        "description='Whether all tables, including system and internal tables "
        "(such as GDB_* tables) should be listed' default='NO'>"
        "    <Value>YES</Value>"
        "    <Value>NO</Value>"
        "  </Option>"
        "</OpenOptionList>");

    poDriver->pfnOpen = OGRPGeoDriverOpen;
    poDriver->pfnIdentify = OGRPGeoDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
