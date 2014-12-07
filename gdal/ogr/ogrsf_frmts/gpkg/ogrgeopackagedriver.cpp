/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements GeoPackageDriver.
 * Author:   Paul Ramsey <pramsey@boundlessgeo.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
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

#include "ogr_geopackage.h"

// g++ -g -Wall -fPIC -shared -o ogr_geopackage.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gpkg ogr/ogrsf_frmts/gpkg/*.c* -L. -lgdal 


/* "GP10" in ASCII bytes */
static const char aGpkgId[4] = {0x47, 0x50, 0x31, 0x30};
static const size_t szGpkgIdPos = 68;

/************************************************************************/
/*                       OGRGeoPackageDriverIdentify()                  */
/************************************************************************/

static int OGRGeoPackageDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    if( EQUALN(poOpenInfo->pszFilename, "GPKG:", 5) )
        return TRUE;

    /* Requirement 3: File name has to end in "gpkg" */
    /* http://opengis.github.io/geopackage/#_file_extension_name */
    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "GPKG") )
        return FALSE;

    /* Check that the filename exists and is a file */
    if( poOpenInfo->fpL == NULL)
        return FALSE;

    /* Requirement 2: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) */
    /* in the application id */
    /* http://opengis.github.io/geopackage/#_file_format */
    if( poOpenInfo->nHeaderBytes < 68 + 4 ||
        memcmp(poOpenInfo->pabyHeader + 68, aGpkgId, 4) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "bad application_id on '%s'",
                  poOpenInfo->pszFilename);
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGeoPackageDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( !OGRGeoPackageDriverIdentify(poOpenInfo) )
        return NULL;

    GDALGeoPackageDataset   *poDS = new GDALGeoPackageDataset();

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

static GDALDataset *OGRGeoPackageDriverCreate( const char * pszFilename,
                                               int nXSize,
                                               int nYSize,
                                               int nBands,
                                               GDALDataType eDT,
                                               char **papszOptions )
{
	/* First, ensure there isn't any such file yet. */
    VSIStatBufL sStatBuf;

    if( nBands != 0 )
    {
        if( eDT != GDT_Byte )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Only Byte supported");
            return NULL;
        }
        if( nBands != 1 && nBands != 3 && nBands != 4 )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Only 1, 3 or 4 band dataset supported");
            return NULL;
        }
    }

    int bFileExists = FALSE;
    if( VSIStatL( pszFilename, &sStatBuf ) == 0 )
    {
        bFileExists = TRUE;
        if( nBands != 0 )
        {
            if( CSLFetchNameValue(papszOptions, "RASTER_TABLE") == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                    "A file system object called '%s' already exists. "
                    "TABLE creation option must be explicitely provided",
                    pszFilename );
                return NULL;
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "A file system object called '%s' already exists.",
                    pszFilename );

            return NULL;
        }
    }
	
    GDALGeoPackageDataset   *poDS = new GDALGeoPackageDataset();

    if( !poDS->Create( pszFilename, bFileExists, nXSize, nYSize, nBands, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static CPLErr OGRGeoPackageDriverDelete( const char *pszFilename )

{
    return CE_None;
}

/************************************************************************/
/*                         RegisterOGRGeoPackage()                       */
/************************************************************************/

void RegisterOGRGeoPackage()
{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "GPKG" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "GPKG" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "GeoPackage" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gpkg" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_geopackage.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );

#define COMPRESSION_OPTIONS \
"  <Option name='DRIVER' type='string-select' description='Format to use to create tiles' default='PNG_JPEG'>" \
"    <Value>PNG_JPEG</Value>" \
"    <Value>PNG</Value>" \
"    <Value>PNG8</Value>" \
"    <Value>JPEG</Value>" \
"    <Value>WEBP</Value>" \
"  </Option>" \
"  <Option name='QUALITY' type='int' min='1' max='100' description='Quality for JPEG and WEBP tiles' default='75'/>" \
"  <Option name='ZLEVEL' type='int' min='1' max='9' description='DEFLATE compression level for PNG tiles' default='6'/>" \
"  <Option name='DITHER' type='boolean' description='Whether to apply Floyd-Steinberg dithering (for DRIVER=PNG8)' default='NO'/>"

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
"  <Option name='RASTER_IDENTIFIER' type='string' description='Human-readable identifier (e.g. short name)'/>"
"  <Option name='RASTER_DESCRIPTION' type='string' description='Human-readable description'/>"
"  <Option name='BLOCKSIZE' type='int' description='Block size in pixels' default='256' max='4096'/>"
"  <Option name='BLOCKXSIZE' type='int' description='Block width in pixels' default='256' max='4096'/>"
"  <Option name='BLOCKYSIZE' type='int' description='Block height in pixels' default='256' max='4096'/>"
COMPRESSION_OPTIONS
"</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='GEOMETRY_COLUMN' type='string' description='Name of geometry column.' default='geom'/>"
"  <Option name='FID' type='string' description='Name of the FID column to create' default='fid'/>"
"  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
"  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index' default='YES'/>"
"</LayerCreationOptionList>");

        poDriver->pfnOpen = OGRGeoPackageDriverOpen;
        poDriver->pfnIdentify = OGRGeoPackageDriverIdentify;
        poDriver->pfnCreate = OGRGeoPackageDriverCreate;
        poDriver->pfnDelete = OGRGeoPackageDriverDelete;

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
