/******************************************************************************
 * $Id$
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

#include "ogr_pg.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ~OGRPGDriver()                            */
/************************************************************************/

OGRPGDriver::~OGRPGDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRPGDriver::GetName()

{
    return "PostgreSQL";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRPGDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGRPGDataSource     *poDS;

    if( !EQUALN(pszFilename,"PGB:",4) &&
        !EQUALN(pszFilename,"PG:",3) )
        return NULL;

    poDS = new OGRPGDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRPGDriver::CreateDataSource( const char * pszName,
                                              char ** /* papszOptions */ )

{
    OGRPGDataSource     *poDS;

    poDS = new OGRPGDataSource();


    if( !poDS->Open( pszName, TRUE, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
         "PostgreSQL driver doesn't currently support database creation.\n"
                  "Please create database with the `createdb' command." );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRPG()                            */
/************************************************************************/

void RegisterOGRPG()

{
    if (! GDAL_CHECK_VERSION("PG driver"))
        return;
    OGRSFDriver* poDriver = new OGRPGDriver;
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                "PostgreSQL/PostGIS" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                "drv_pg.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList/>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='GEOM_TYPE' type='string-select' description='Format of geometry columns' default='geometry'>"
"    <Value>geometry</Value>"
"    <Value>geography</Value>"
"    <Value>BYTEA</Value>"
"    <Value>OID</Value>"
"  </Option>"
"  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
"  <Option name='LAUNDER' type='boolean' description='Whether layer and field names will be laundered' default='YES'/>"
"  <Option name='PRECISION' type='boolean' description='Whether fields created should keep the width and precision' default='YES'/>"
"  <Option name='DIM' type='integer' description='Set to 2 to force the geometries to be 2D, or 3 to be 2.5D'/>"
"  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column. Defaults to wkb_geometry for GEOM_TYPE=geometry or the_geog for GEOM_TYPE=geography'/>"
"  <Option name='SCHEMA' type='string' description='Name of schema into which to create the new table'/>"
"  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index' default='YES'/>"
"  <Option name='TEMPORARY' type='boolean' description='Whether to a temporary table instead of a permanent one' default='NO'/>"
"  <Option name='NONE_AS_UNKNOWN' type='boolean' description='Whether to force non-spatial layers to be created as spatial tables' default='NO'/>"
"  <Option name='FID' type='string' description='Name of the FID column to create' default='ogc_fid'/>"
"  <Option name='FID64' type='boolean' description='Whether to create the FID column with BIGSERIAL type to handle 64bit wide ids' default='NO'/>"
"  <Option name='EXTRACT_SCHEMA_FROM_LAYER_NAME' type='boolean' description='Whether a dot in a layer name should be considered as the separator for the schema and table name' default='YES'/>"
"  <Option name='COLUMN_TYPES' type='string' description='A list of strings of format field_name=pg_field_type (separated by comma) to force the PG column type of fields to be created'/>"
"</LayerCreationOptionList>");
    
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime Time IntegerList Integer64List RealList StringList Binary" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(poDriver);
}

