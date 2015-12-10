/*****************************************************************************
 *
 * Project:  DB2 Spatial driver
 * Purpose:  Definition of classes for OGR DB2 Spatial driver.
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 *****************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2015, David Adler
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
#include "ogr_db2.h"
#include "cpl_conv.h"

/************************************************************************/
/*                       OGRDB2DriverIdentify()                  */
/************************************************************************/

static int OGRDB2DriverIdentify( GDALOpenInfo* poOpenInfo )
{
#ifdef DEBUG_DB2
    CPLDebug( "OGRDB2DriverIdentify", "pszFilename: '%s'",
              poOpenInfo->pszFilename);
#endif
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, DB2ODBC_PREFIX) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDB2DriverOpen( GDALOpenInfo* poOpenInfo )
{
#ifdef DEBUG_DB2
    CPLDebug( "OGRDB2DriverOpen", "pszFilename: '%s'",
              poOpenInfo->pszFilename);
#endif
    if( !OGRDB2DriverIdentify(poOpenInfo) )
        return NULL;

    OGRDB2DataSource   *poDS = new OGRDB2DataSource();

    if( !poDS->Open( poOpenInfo ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset* OGRDB2DriverCreate( const char * pszFilename,
                                        int nXSize,
                                        int nYSize,
                                        int nBands,
                                        GDALDataType eDT,
                                        char **papszOptions )
{
    OGRDB2DataSource   *poDS = new OGRDB2DataSource();
#ifdef DEBUG_DB2
    CPLDebug( "OGRDB2DriverCreate", "pszFilename: '%s'", pszFilename);
#endif
    if( !poDS->Create( pszFilename, nXSize, nYSize,
                       nBands, eDT, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static CPLErr OGRDB2DriverDelete( const char *pszFilename )

{
#ifdef DEBUG_DB2
    CPLDebug( "OGRDB2DriverDelete", "pszFilename: '%s'", pszFilename);
#endif
    if( VSIUnlink(pszFilename) == 0 )
        return CE_None;
    else
        return CE_Failure;
}


/************************************************************************/
/*                           RegisterOGRDB2()                  */
/************************************************************************/

void RegisterOGRDB2()
{
    if( GDALGetDriverByName("DB2ODBC") != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription( "DB2ODBC" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "IBM DB2 Spatial Database" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_db2.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
                               "<CreationOptionList/>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='GEOM_TYPE' type='string-select' "
        "          description='Format of geometry columns' "
        "          default='geometry'>"
        "    <Value>geometry</Value>"
        "  </Option>"
        "  <Option name='OVERWRITE' type='boolean' "
        "          description='Whether to overwrite an existing table "
        "                       with the layer name to be created' "
        "          default='NO'/>"
        "  <Option name='LAUNDER' type='boolean' "
        "          description='Whether layer and field names will be "
        "                       laundered' default='YES'/>"
        "  <Option name='PRECISION' type='boolean' "
        "          description='Whether fields created should keep the "
        "                       width and precision' default='YES'/>"
        "  <Option name='DIM' type='integer' "
        "          description='Set to 2 to force the geometries to be "
        "                       2D, or 3 to be 2.5D'/>"
        "  <Option name='GEOMETRY_NAME' type='string' "
        "          description='Name of geometry column.' "
        "          default='ogr_geometry' deprecated_alias='GEOM_NAME'/>"
        "  <Option name='SCHEMA' type='string' "
        "          description='Name of schema into which to create the "
        "                       new table' "
        "          default='dbo'/>"
        "  <Option name='SRID' type='int' "
        "          description='Forced SRID of the layer'/>"
        "  <Option name='SPATIAL_INDEX' type='boolean' "
        "          description='Whether to create a spatial index' "
        "          default='YES'/>"
        "  <Option name='UPLOAD_GEOM_FORMAT' type='string-select' "
        "          description='Geometry format when creating or "
        "                       modifying features' "
        "          default='wkb'>"
        "    <Value>wkb</Value>"
        "    <Value>wkt</Value>"
        "  </Option>"
        "  <Option name='FID' type='string' "
        "          description='Name of the FID column to create' "
        "          default='OBJECTID'/>"
        "  <Option name='FID64' type='boolean' "
        "          description='Whether to create the FID column with "
        "                       bigint type to handle 64bit wide ids' "
        "          default='YES'/>"
        "  <Option name='GEOMETRY_NULLABLE' type='boolean' "
        "          description='Whether the values of the geometry "
        "                       column can be NULL' "
        "          default='YES'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date Time "
                               "DateTime Binary" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );

    poDriver->pfnOpen = OGRDB2DriverOpen;
    poDriver->pfnIdentify = OGRDB2DriverIdentify;
    poDriver->pfnCreate = OGRDB2DriverCreate;
    poDriver->pfnDelete = OGRDB2DriverDelete;

    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
