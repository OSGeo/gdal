/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCIDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_oci.h"

CPL_CVSID("$Id$");


/************************************************************************/
/*                        OGROCIDriverIdentify()                        */
/************************************************************************/

static int OGROCIDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    return EQUALN(poOpenInfo->pszFilename,"OCI:",4);
}

/************************************************************************/
/*                          OGROCIDriverOpen()                          */
/************************************************************************/

static GDALDataset *OGROCIDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGROCIDriverIdentify(poOpenInfo) )
        return NULL;

    OGROCIDataSource    *poDS;

    poDS = new OGROCIDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                     poOpenInfo->eAccess == GA_Update, TRUE ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                         OGROCIDriverCreate()                         */
/************************************************************************/

static GDALDataset *OGROCIDriverCreate( const char * pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        CPL_UNUSED char **papszOptions )

{
    OGROCIDataSource    *poDS;

    poDS = new OGROCIDataSource();


    if( !poDS->Open( pszName, NULL, TRUE, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
         "Oracle driver doesn't currently support database creation.\n"
                  "Please create database with Oracle tools before loading tables." );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGROCI()                            */
/************************************************************************/

void RegisterOGROCI()

{
    if (! GDAL_CHECK_VERSION("OCI driver"))
        return;
    
    if( GDALGetDriverByName( "OCI" ) == NULL )
    {
        GDALDriver* poDriver = new GDALDriver();

        poDriver->SetDescription( "OCI" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                        "Oracle Spatial" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                        "drv_oci.html" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );

        poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "OCI:" );

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='DBNAME' type='string' description='Database name'/>"
"  <Option name='USER' type='string' description='User name'/>"
"  <Option name='PASSWORD' type='string' description='Password'/>"
"  <Option name='TABLES' type='string' description='Restricted set of tables to list (comma separated)'/>"
"</OpenOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='LAUNDER' type='boolean' description='Whether layer and field names will be laundered' default='NO'/>"
        "  <Option name='PRECISION' type='boolean' description='Whether fields created should keep the width and precision' default='YES'/>"
        "  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
        "  <Option name='TRUNCATE' type='boolean' description='Whether to truncate an existing table' default='NO'/>"
        "  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index' default='YES' deprecated_alias='INDEX'/>"
        "  <Option name='INDEX_PARAMETERS' type='string' description='Creation parameters when the spatial index is created'/>"
        "  <Option name='ADD_LAYER_GTYPE' type='boolean' description='May be set to NO to disable the constraints on the geometry type in the spatial index' default='YES'/>"
        "  <Option name='MULTI_LOAD' type='boolean' description='If enabled new features will be created in groups of 100 per SQL INSERT command' default='YES'/>"
        "  <Option name='LOADER_FILE' type='string' description='If this option is set, all feature information will be written to a file suitable for use with SQL*Loader'/>"
        "  <Option name='DIM' type='integer' description='Set to 2 to force the geometries to be 2D, or 3 to be 2.5D' default='3'/>"
        "  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column.' default='ORA_GEOMETRY'/>"
        "  <Option name='GEOMETRY_NULLABLE' type='boolean' description='Whether the values of the geometry column can be NULL' default='YES'/>"
        "  <Option name='DIMINFO_X' type='string' description='xmin,xmax,xres values to control the X dimension info written into the USER_SDO_GEOM_METADATA table'/>"
        "  <Option name='DIMINFO_Y' type='string' description='ymin,ymax,yres values to control the Y dimension info written into the USER_SDO_GEOM_METADATA table'/>"
        "  <Option name='DIMINFO_Z' type='string' description='zmin,zmax,zres values to control the Z dimension info written into the USER_SDO_GEOM_METADATA table'/>"
        "  <Option name='SRID' type='int' description='Forced SRID of the layer'/>"
        "</LayerCreationOptionList>");
            
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime" );
        poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );

        poDriver->pfnOpen = OGROCIDriverOpen;
        poDriver->pfnIdentify = OGROCIDriverIdentify;
        poDriver->pfnCreate = OGROCIDriverCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

