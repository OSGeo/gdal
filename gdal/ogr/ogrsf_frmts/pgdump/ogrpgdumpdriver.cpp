/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDumpDriver class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_pgdump.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRPGDumpDriverCreate()                      */
/************************************************************************/

static GDALDataset* OGRPGDumpDriverCreate( const char * pszName,
                                           CPL_UNUSED int nXSize,
                                           CPL_UNUSED int nYSize,
                                           CPL_UNUSED int nBands,
                                           CPL_UNUSED GDALDataType eDT,
                                           char ** papszOptions )
{
    OGRPGDumpDataSource     *poDS;

    if (strcmp(pszName, "/dev/stdout") == 0)
        pszName = "/vsistdout/";

    poDS = new OGRPGDumpDataSource(pszName, papszOptions);
    if( !poDS->Log("SET standard_conforming_strings = OFF") )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                        RegisterOGRPGDump()                           */
/************************************************************************/

void RegisterOGRPGDump()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "PGDUMP" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "PGDUMP" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                    "PostgreSQL SQL dump" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                    "drv_pgdump.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "sql" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
    "<CreationOptionList>"
    #ifdef WIN32
    "  <Option name='LINEFORMAT' type='string-select' description='end-of-line sequence' default='CRLF'>"
    #else
    "  <Option name='LINEFORMAT' type='string-select' description='end-of-line sequence' default='LF'>"
    #endif
    "    <Value>CRLF</Value>"
    "    <Value>LF</Value>"
    "  </Option>"
    "</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
    "<LayerCreationOptionList>"
    "  <Option name='GEOM_TYPE' type='string-select' description='Format of geometry columns' default='geometry'>"
    "    <Value>geometry</Value>"
    "    <Value>geography</Value>"
    "  </Option>"
    "  <Option name='LAUNDER' type='boolean' description='Whether layer and field names will be laundered' default='YES'/>"
    "  <Option name='PRECISION' type='boolean' description='Whether fields created should keep the width and precision' default='YES'/>"
    "  <Option name='DIM' type='integer' description='Set to 2 to force the geometries to be 2D, or 3 to be 2.5D'/>"
    "  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column. Defaults to wkb_geometry for GEOM_TYPE=geometry or the_geog for GEOM_TYPE=geography'/>"
    "  <Option name='SCHEMA' type='string' description='Name of schema into which to create the new table'/>"
    "  <Option name='CREATE_SCHEMA' type='boolean' description='Whether to explictely emit the CREATE SCHEMA statement to create the specified schema' default='YES'/>"
    "  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index' default='YES'/>"
    "  <Option name='TEMPORARY' type='boolean' description='Whether to a temporary table instead of a permanent one' default='NO'/>"
    "  <Option name='WRITE_EWKT_GEOM' type='boolean' description='Whether to write EWKT geometries instead of HEX geometrie' default='NO'/>"
    "  <Option name='CREATE_TABLE' type='boolean' description='Whether to explictely recreate the table if necessary' default='YES'/>"
    "  <Option name='DROP_TABLE' type='string-select' description='Whether to explictely destroy tables before recreating them' default='YES'>"
    "    <Value>YES</Value>"
    "    <Value>ON</Value>"
    "    <Value>TRUE</Value>"
    "    <Value>NO</Value>"
    "    <Value>OFF</Value>"
    "    <Value>FALSE</Value>"
    "    <Value>IF_EXISTS</Value>"
    "  </Option>"
    "  <Option name='SRID' type='int' description='Forced SRID of the layer'/>"
    "  <Option name='NONE_AS_UNKNOWN' type='boolean' description='Whether to force non-spatial layers to be created as spatial tables' default='NO'/>"
    "  <Option name='FID' type='string' description='Name of the FID column to create' default='ogc_fid'/>"
    "  <Option name='EXTRACT_SCHEMA_FROM_LAYER_NAME' type='boolean' description='Whether a dot in a layer name should be considered as the separator for the schema and table name' default='YES'/>"
    "  <Option name='COLUMN_TYPES' type='string' description='A list of strings of format field_name=pg_field_type (separated by comma) to force the PG column type of fields to be created'/>"
    "  <Option name='POSTGIS_VERSION' type='string' description='Can be set to 2.0 for PostGIS 2.0 compatibility. For now, it is not critical to set it. Its effect is just to avoid a few warnings'/>"
    "</LayerCreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnCreate = OGRPGDumpDriverCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
