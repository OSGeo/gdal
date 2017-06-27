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

CPL_CVSID("$Id$")

/************************************************************************/
/*                       OGRDB2DriverIdentify()                  */
/************************************************************************/

static int OGRDB2DriverIdentify( GDALOpenInfo* poOpenInfo )
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, DB2ODBC_PREFIX) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDB2DriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( !OGRDB2DriverIdentify(poOpenInfo) )
        return NULL;

    CPLDebug( "OGRDB2DriverOpen", "pszFilename: '%s'",
              poOpenInfo->pszFilename);

    OGRDB2DataSource   *poDS = new OGRDB2DataSource();

    if( !poDS->Open( poOpenInfo ) )
    {
        CPLDebug( "OGRDB2DriverOpen", "open error");
        delete poDS;
        poDS = NULL;
    }
    CPLDebug( "OGRDB2DriverOpen", "Exit");
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
    CPLDebug( "OGRDB2DriverCreate", "pszFilename: '%s'", pszFilename);
    CPLDebug( "OGRDB2DriverCreate", "eDT: %d", eDT);
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
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );
#define COMPRESSION_OPTIONS \
"  <Option name='TILE_FORMAT' type='string-select' description='Format to use to create tiles' default='PNG_JPEG'>" \
"    <Value>PNG_JPEG</Value>" \
"    <Value>PNG</Value>" \
"    <Value>PNG8</Value>" \
"    <Value>JPEG</Value>" \
"    <Value>WEBP</Value>" \
"  </Option>" \
"  <Option name='QUALITY' type='int' min='1' max='100' description='Quality for JPEG and WEBP tiles' default='75'/>" \
"  <Option name='ZLEVEL' type='int' min='1' max='9' description='DEFLATE compression level for PNG tiles' default='6'/>" \
"  <Option name='DITHER' type='boolean' description='Whether to apply Floyd-Steinberg dithering (for TILE_FORMAT=PNG8)' default='NO'/>"

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList>"
                               "  <Option name='TABLE' type='string' description='Name of tile user-table'/>"
                               "  <Option name='ZOOM_LEVEL' type='integer' description='Zoom level of full resolution. If not specified, maximum non-empty zoom level'/>"
                               "  <Option name='BAND_COUNT' type='int' min='1' max='4' description='Number of raster bands' default='4'/>"
                               "  <Option name='MINX' type='float' description='Minimum X of area of interest'/>"
                               "  <Option name='MINY' type='float' description='Minimum Y of area of interest'/>"
                               "  <Option name='MAXX' type='float' description='Maximum X of area of interest'/>"
                               "  <Option name='MAXY' type='float' description='Maximum Y of area of interest'/>"
                               "  <Option name='USE_TILE_EXTENT' type='boolean' description='Use tile extent of content to determine area of interest' default='NO'/>"
                               "  <Option name='WHERE' type='string' description='SQL WHERE clause to be appended to tile requests'/>"
                               COMPRESSION_OPTIONS
                               "</OpenOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList>"
                               "  <Option name='RASTER_TABLE' type='string' description='Name of tile user table'/>"
                               "  <Option name='APPEND_SUBDATASET' type='boolean' description='Set to YES to add a new tile user table to an existing GeoPackage instead of replacing it' default='NO'/>"
                               "  <Option name='RASTER_IDENTIFIER' type='string' description='Human-readable identifier (e.g. short name)'/>"
                               "  <Option name='RASTER_DESCRIPTION' type='string' description='Human-readable description'/>"
                               "  <Option name='BLOCKSIZE' type='int' description='Block size in pixels' default='256' max='4096'/>"
                               "  <Option name='BLOCKXSIZE' type='int' description='Block width in pixels' default='256' max='4096'/>"
                               "  <Option name='BLOCKYSIZE' type='int' description='Block height in pixels' default='256' max='4096'/>"
                               COMPRESSION_OPTIONS
                               "  <Option name='TILING_SCHEME' type='string-select' description='Which tiling scheme to use' default='CUSTOM'>"
                               "    <Value>CUSTOM</Value>"
                               "    <Value>GoogleCRS84Quad</Value>"
                               "    <Value>GoogleMapsCompatible</Value>"
                               "    <Value>InspireCRS84Quad</Value>"
                               "    <Value>PseudoTMS_GlobalGeodetic</Value>"
                               "    <Value>PseudoTMS_GlobalMercator</Value>"
                               "  </Option>"
                               "  <Option name='ZOOM_LEVEL_STRATEGY' type='string-select' description='Strategy to determine zoom level. Only used for TILING_SCHEME != CUSTOM' default='AUTO'>"
                               "    <Value>AUTO</Value>"
                               "    <Value>LOWER</Value>"
                               "    <Value>UPPER</Value>"
                               "  </Option>"
                               "  <Option name='RESAMPLING' type='string-select' description='Resampling algorithm. Only used for TILING_SCHEME != CUSTOM' default='BILINEAR'>"
                               "    <Value>NEAREST</Value>"
                               "    <Value>BILINEAR</Value>"
                               "    <Value>CUBIC</Value>"
                               "    <Value>CUBICSPLINE</Value>"
                               "    <Value>LANCZOS</Value>"
                               "    <Value>MODE</Value>"
                               "    <Value>AVERAGE</Value>"
                               "  </Option>"
                               "</CreationOptionList>");

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
    poDriver->pfnCreateCopy = OGRDB2DataSource::CreateCopy;

    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
