/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           ~OGRMSSQLSpatialDriver()                   */
/************************************************************************/

OGRMSSQLSpatialDriver::~OGRMSSQLSpatialDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRMSSQLSpatialDriver::GetName()

{
    return "MSSQLSpatial";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRMSSQLSpatialDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRMSSQLSpatialDataSource     *poDS;

    if( !EQUALN(pszFilename,"MSSQL:",6) )
        return NULL;

    poDS = new OGRMSSQLSpatialDataSource();

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

OGRDataSource *OGRMSSQLSpatialDriver::CreateDataSource( const char * pszName,
                                                        CPL_UNUSED char **papszOptions )
{
    OGRMSSQLSpatialDataSource   *poDS = new OGRMSSQLSpatialDataSource();

    if( !EQUALN(pszName,"MSSQL:",6) )
        return NULL;

    if( !poDS->Open( pszName, TRUE, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
         "MSSQL Spatial driver doesn't currently support database creation.\n"
                  "Please create database with the Microsoft SQL Server Client Tools." );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMSSQLSpatialDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}


/************************************************************************/
/*                           RegisterOGRMSSQLSpatial()                  */
/************************************************************************/

void RegisterOGRMSSQLSpatial()

{
    if (! GDAL_CHECK_VERSION("OGR/MSSQLSpatial driver"))
        return;
    OGRSFDriver* poDriver = new OGRMSSQLSpatialDriver;
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Microsoft SQL Server Spatial Database" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                "drv_mssqlspatial.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList/>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='GEOM_TYPE' type='string-select' description='Format of geometry columns' default='geometry'>"
"    <Value>geometry</Value>"
"    <Value>geography</Value>"
"  </Option>"
"  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
"  <Option name='LAUNDER' type='boolean' description='Whether layer and field names will be laundered' default='YES'/>"
"  <Option name='PRECISION' type='boolean' description='Whether fields created should keep the width and precision' default='YES'/>"
"  <Option name='DIM' type='integer' description='Set to 2 to force the geometries to be 2D, or 3 to be 2.5D'/>"
"  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column.' default='ogr_geometry' deprecated_alias='GEOM_NAME'/>"
"  <Option name='SCHEMA' type='string' description='Name of schema into which to create the new table' default='dbo'/>"
"  <Option name='SRID' type='int' description='Forced SRID of the layer'/>"
"  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index' default='YES'/>"
"  <Option name='UPLOAD_GEOM_FORMAT' type='string-select' description='Geometry format when creating or modifying features' default='wkb'>"
"    <Value>wkb</Value>"
"    <Value>wkt</Value>"
"  </Option>"
"  <Option name='FID' type='string' description='Name of the FID column to create' default='ogr_fid'/>"
"  <Option name='FID64' type='boolean' description='Whether to create the FID column with bigint type to handle 64bit wide ids' default='NO'/>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date Time DateTime Binary" );
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(poDriver);
}
