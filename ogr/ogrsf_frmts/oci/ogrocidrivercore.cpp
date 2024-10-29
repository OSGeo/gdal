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

#include "ogrsf_frmts.h"

#include "ogrocidrivercore.h"

/************************************************************************/
/*                         OGROCIDriverIdentify()                       */
/************************************************************************/

int OGROCIDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "OCI:");
}

/************************************************************************/
/*                  OGROCIDriverSetCommonMetadata()                     */
/************************************************************************/

void OGROCIDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Oracle Spatial");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/oci.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "OCI:");
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='DBNAME' type='string' description='Database name'/>"
        "  <Option name='USER' type='string' description='User name'/>"
        "  <Option name='PASSWORD' type='string' description='Password'/>"
        "  <Option name='TABLES' type='string' description='Restricted set of "
        "tables to list (comma separated)'/>"
        "  <Option name='WORKSPACE' type='string' description='Workspace'/>"
        "  <Option name='MULTI_LOAD' type='boolean' description='If enabled "
        "new features will be created in groups of 100 per SQL INSERT command' "
        "default='YES'/>"
        "  <Option name='MULTI_LOAD_COUNT' type='int' description='Number of "
        "items for a group INSERT' default='100'/>"
        "  <Option name='FIRST_ID' type='int' description='First id value to "
        "be used on append'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='LAUNDER' type='boolean' description='Whether layer "
        "and field names will be laundered' default='NO'/>"
        "  <Option name='PRECISION' type='boolean' description='Whether fields "
        "created should keep the width and precision' default='YES'/>"
        "  <Option name='OVERWRITE' type='boolean' description='Whether to "
        "overwrite an existing table with the layer name to be created' "
        "default='NO'/>"
        "  <Option name='TRUNCATE' type='boolean' description='Whether to "
        "truncate an existing table' default='NO'/>"
        "  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to "
        "create a spatial index' default='YES' deprecated_alias='INDEX'/>"
        "  <Option name='INDEX_PARAMETERS' type='string' description='Creation "
        "parameters when the spatial index is created'/>"
        "  <Option name='ADD_LAYER_GTYPE' type='boolean' description='May be "
        "set to NO to disable the constraints on the geometry type in the "
        "spatial index' default='YES'/>"
        "  <Option name='MULTI_LOAD' type='boolean' description='If enabled "
        "new features will be created in groups of 100 per SQL INSERT command' "
        "default='YES'/>"
        "  <Option name='MULTI_LOAD_COUNT' type='int' description='Number of "
        "items for a group INSERT' default='100'/>"
        "  <Option name='DEFAULT_STRING_SIZE' type='int' description='Default "
        "string column size' default='4000'/>"
        "  <Option name='LOADER_FILE' type='string' description='If this "
        "option is set, all feature information will be written to a file "
        "suitable for use with SQL*Loader'/>"
        "  <Option name='DIM' type='integer' description='Set to 2 to force "
        "the geometries to be 2D, or 3 to be 2.5D' default='3'/>"
        "  <Option name='GEOMETRY_NAME' type='string' description='Name of "
        "geometry column.' default='ORA_GEOMETRY'/>"
        "  <Option name='GEOMETRY_NULLABLE' type='boolean' "
        "description='Whether the values of the geometry column can be NULL' "
        "default='YES'/>"
        "  <Option name='DIMINFO_X' type='string' description='xmin,xmax,xres "
        "values to control the X dimension info written into the "
        "USER_SDO_GEOM_METADATA table'/>"
        "  <Option name='DIMINFO_Y' type='string' description='ymin,ymax,yres "
        "values to control the Y dimension info written into the "
        "USER_SDO_GEOM_METADATA table'/>"
        "  <Option name='DIMINFO_Z' type='string' description='zmin,zmax,zres "
        "values to control the Z dimension info written into the "
        "USER_SDO_GEOM_METADATA table'/>"
        "  <Option name='SRID' type='int' description='Forced SRID of the "
        "layer'/>"
        "  <Option name='FIRST_ID' type='int' description='First id value'/>"
        "  <Option name='NO_LOGGING' type='boolean' description='Create table "
        "with no_logging parameters' default='NO'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DEFAULT_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS,
                              "NATIVE OGRSQL SQLITE");

    poDriver->pfnIdentify = OGROCIDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGROCIPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGROCIPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    OGROCIDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
