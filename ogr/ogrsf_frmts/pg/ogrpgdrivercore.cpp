/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#include "ogrsf_frmts.h"

#include "ogrpgdrivercore.h"

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int OGRPGDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "PGB:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "PG:") &&
        !STARTS_WITH(poOpenInfo->pszFilename, "postgresql://"))
        return FALSE;
    return TRUE;
}

/************************************************************************/
/*                OGRPGDriverSetCommonMetadata()                        */
/************************************************************************/

void OGRPGDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "PostgreSQL/PostGIS");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/pg.html");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "PG:");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS,
                              "NATIVE OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='DBNAME' type='string' description='Database name'/>"
        "  <Option name='PORT' type='int' description='Port'/>"
        "  <Option name='USER' type='string' description='User name'/>"
        "  <Option name='PASSWORD' type='string' description='Password'/>"
        "  <Option name='HOST' type='string' description='Server hostname'/>"
        "  <Option name='SERVICE' type='string' description='Service name'/>"
        "  <Option name='ACTIVE_SCHEMA' type='string' description='Active "
        "schema'/>"
        "  <Option name='SCHEMAS' type='string' description='Restricted sets "
        "of schemas to explore (comma separated)'/>"
        "  <Option name='TABLES' type='string' description='Restricted set of "
        "tables to list (comma separated)'/>"
        "  <Option name='LIST_ALL_TABLES' type='boolean' description='Whether "
        "all tables, including non-spatial ones, should be listed' "
        "default='NO'/>"
        "  <Option name='PRELUDE_STATEMENTS' type='string' description='SQL "
        "statement(s) to send on the PostgreSQL client connection before any "
        "other ones'/>"
        "  <Option name='CLOSING_STATEMENTS' type='string' description='SQL "
        "statements() to send on the PostgreSQL client connection after any "
        "other ones'/>"
        "  <Option name='SKIP_VIEWS' type='boolean' description='Whether "
        "views should be omitted from the list' "
        "default='NO'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList/>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='GEOM_TYPE' type='string-select' description='Format "
        "of geometry columns' default='geometry'>"
        "    <Value>geometry</Value>"
        "    <Value>geography</Value>"
        "    <Value>BYTEA</Value>"
        "    <Value>OID</Value>"
        "  </Option>"
        "  <Option name='OVERWRITE' type='boolean' description='Whether to "
        "overwrite an existing table with the layer name to be created' "
        "default='NO'/>"
        "  <Option name='LAUNDER' type='boolean' description='Whether layer "
        "and field names will be laundered' default='YES'/>"
        "  <Option name='LAUNDER_ASCII' type='boolean' description='Same as "
        "LAUNDER, but force generation of ASCII identifiers' default='NO'/>"
        "  <Option name='PRECISION' type='boolean' description='Whether fields "
        "created should keep the width and precision' default='YES'/>"
        "  <Option name='DIM' type='string' description='Set to 2 to force the "
        "geometries to be 2D, 3 to be 2.5D, XYM or XYZM'/>"
        "  <Option name='GEOMETRY_NAME' type='string' description='Name of "
        "geometry column. Defaults to wkb_geometry for GEOM_TYPE=geometry or "
        "the_geog for GEOM_TYPE=geography'/>"
        "  <Option name='SCHEMA' type='string' description='Name of schema "
        "into which to create the new table'/>"
        "  <Option name='SPATIAL_INDEX' type='string-select' description='Type "
        "of spatial index to create' default='GIST'>"
        "    <Value>NONE</Value>"
        "    <Value>GIST</Value>"
        "    <Value>SPGIST</Value>"
        "    <Value>BRIN</Value>"
        "  </Option>"
        "  <Option name='TEMPORARY' type='boolean' description='Whether to a "
        "temporary table instead of a permanent one' default='NO'/>"
        "  <Option name='UNLOGGED' type='boolean' description='Whether to "
        "create the table as a unlogged one' default='NO'/>"
        "  <Option name='NONE_AS_UNKNOWN' type='boolean' description='Whether "
        "to force non-spatial layers to be created as spatial tables' "
        "default='NO'/>"
        "  <Option name='FID' type='string' description='Name of the FID "
        "column to create' default='ogc_fid'/>"
        "  <Option name='FID64' type='boolean' description='Whether to create "
        "the FID column with BIGSERIAL type to handle 64bit wide ids' "
        "default='NO'/>"
        "  <Option name='EXTRACT_SCHEMA_FROM_LAYER_NAME' type='boolean' "
        "description='Whether a dot in a layer name should be considered as "
        "the separator for the schema and table name' default='YES'/>"
        "  <Option name='COLUMN_TYPES' type='string' description='A list of "
        "strings of format field_name=pg_field_type (separated by comma) to "
        "force the PG column type of fields to be created'/>"
        "  <Option name='DESCRIPTION' type='string' description='Description "
        "string to put in the pg_description system table'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime "
                              "Time IntegerList Integer64List RealList "
                              "StringList Binary");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Boolean Int16 Float32");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "WidthPrecision Nullable Unique Default Comment");

    poDriver->SetMetadataItem(
        GDAL_DMD_ALTER_FIELD_DEFN_FLAGS,
        "Name Type WidthPrecision Nullable Default Unique Comment");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DEFAULT_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_UNIQUE_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_RENAME_LAYERS, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS,
                              "Name Type Nullable SRS");
    // see https://www.postgresql.org/docs/current/ddl-system-columns.html
    poDriver->SetMetadataItem(GDAL_DMD_ILLEGAL_FIELD_NAMES,
                              "tableoid xmin cmin xmax cmax ctid");

    poDriver->pfnIdentify = OGRPGDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGRPGPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRPGPlugin()
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
    OGRPGDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
