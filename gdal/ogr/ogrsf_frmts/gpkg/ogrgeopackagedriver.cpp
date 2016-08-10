/******************************************************************************
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

CPL_CVSID("$Id$");

// g++ -g -Wall -fPIC -shared -o ogr_geopackage.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gpkg ogr/ogrsf_frmts/gpkg/*.c* -L. -lgdal


/* "GP10" in ASCII bytes */
static const char aGpkgId[4] = {0x47, 0x50, 0x31, 0x30};
// static const size_t szGpkgIdPos = 68;

/************************************************************************/
/*                       OGRGeoPackageDriverIdentify()                  */
/************************************************************************/


static int OGRGeoPackageDriverIdentify( GDALOpenInfo* poOpenInfo, bool bEmitWarning )
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GPKG:") )
        return TRUE;

    /* Check that the filename exists and is a file */
    if( poOpenInfo->fpL == NULL)
        return FALSE;

    if ( poOpenInfo->nHeaderBytes < 16 ||
        !STARTS_WITH((const char*)poOpenInfo->pabyHeader, "SQLite format 3") )
    {
        return FALSE;
    }

    /* Requirement 3: File name has to end in "gpkg" */
    /* http://opengis.github.io/geopackage/#_file_extension_name */
    /* But be tolerant, if the GPKG application id is found, because some */
    /* producers don't necessarily honour that requirement (#6396) */
    const char* pszExt = CPLGetExtension(poOpenInfo->pszFilename);
    const bool bIsRecognizedExtension = EQUAL(pszExt, "GPKG") || EQUAL(pszExt, "GPKX");

    /* Requirement 2: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) */
    /* in the application id */
    /* http://opengis.github.io/geopackage/#_file_format */
    /* Be tolerant since some datasets don't actually follow that requirement */
    if( poOpenInfo->nHeaderBytes < 68 + 4 ||
        memcmp(poOpenInfo->pabyHeader + 68, aGpkgId, 4) != 0 )
    {
#ifdef DEBUG
        if( EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input")  )
        {
            return FALSE;
        }
#endif
        if( !bIsRecognizedExtension )
            return FALSE;

        if( bEmitWarning )
        {
            char szSignature[4+1];
            memcpy(szSignature, poOpenInfo->pabyHeader + 68, 4);
            szSignature[4] = '\0';

            /* Is this a GPxx version ? */
            const bool bWarn = CPLTestBool(CPLGetConfigOption("GPKG_WARN_UNRECOGNIZED_APPLICATION_ID", "YES"));
            if( szSignature[0] == 'G' && szSignature[1] == 'P' &&
                szSignature[2] >= '0' && szSignature[2] <= '9' &&
                szSignature[3] >= '0' && szSignature[3] <= '9' )
            {
                if( bWarn )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "GPKG: '%s' has version '%s' with may be partially supported by this driver",
                              poOpenInfo->pszFilename, szSignature );
                }
                else
                {
                    CPLDebug( "GPKG",
                              "'%s' has version '%s' with may be partially supported by this driver",
                              poOpenInfo->pszFilename, szSignature );
                }
            }
            else
            {
                if( bWarn )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "GPKG: bad application_id 0x%02X%02X%02X%02X on '%s'",
                              szSignature[0], szSignature[1], szSignature[2], szSignature[3],
                              poOpenInfo->pszFilename );
                }
                else
                {
                    CPLDebug( "GPKG",
                              "bad application_id 0x%02X%02X%02X%02X on '%s'",
                              szSignature[0], szSignature[1], szSignature[2], szSignature[3],
                              poOpenInfo->pszFilename );
                }
            }
        }
    }
    else if( !bIsRecognizedExtension
#ifdef DEBUG
              && !EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input")
#endif
           )
    {
        if( bEmitWarning )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "File %s has GPKG application_id, but non conformant file extension",
                      poOpenInfo->pszFilename);
        }
    }

    return TRUE;
}

static int OGRGeoPackageDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    return OGRGeoPackageDriverIdentify(poOpenInfo, false);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGeoPackageDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( !OGRGeoPackageDriverIdentify(poOpenInfo, true) )
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

static GDALDataset* OGRGeoPackageDriverCreate( const char * pszFilename,
                                            int nXSize,
                                            int nYSize,
                                            int nBands,
                                            GDALDataType eDT,
                                            char **papszOptions )
{
    const char* pszExt = CPLGetExtension(pszFilename);
    const bool bIsRecognizedExtension = EQUAL(pszExt, "GPKG") || EQUAL(pszExt, "GPKX");
    if( !bIsRecognizedExtension )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The '%s' extension is not allowed by the GPKG specification, "
                 "which may cause compatibility problems",
                 pszExt);
    }

    GDALGeoPackageDataset   *poDS = new GDALGeoPackageDataset();

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

static CPLErr OGRGeoPackageDriverDelete( const char *pszFilename )

{
    if( VSIUnlink(pszFilename) == 0 )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                         RegisterOGRGeoPackage()                       */
/************************************************************************/

void RegisterOGRGeoPackage()
{
    if( GDALGetDriverByName( "GPKG" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "GPKG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GeoPackage" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gpkg" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_geopackage.html" );
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
"  <Option name='LIST_ALL_TABLES' type='boolean' description='Whether all tables, including those non listed in gpkg_contents, should be listed' default='YES' />"
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

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column.' default='geom' deprecated_alias='GEOMETRY_COLUMN'/>"
"  <Option name='GEOMETRY_NULLABLE' type='boolean' description='Whether the values of the geometry column can be NULL' default='YES'/>"
"  <Option name='FID' type='string' description='Name of the FID column to create' default='fid'/>"
"  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
"  <Option name='PRECISION' type='boolean' description='Whether text fields created should keep the width' default='YES'/>"
"  <Option name='TRUNCATE_FIELDS' type='boolean' description='Whether to truncate text content that exceeds maximum width' default='NO'/>"
"  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index' default='YES'/>"
"  <Option name='IDENTIFIER' type='string' description='Identifier of the layer, as put in the contents table'/>"
"  <Option name='DESCRIPTION' type='string' description='Description of the layer, as put in the contents table'/>"
"  <Option name='REGISTER_AS_ASPATIAL' type='boolean' description='Whether non spatial tables should be registered as aspatial in gpkg_contents' default='YES'/>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date DateTime "
                               "Binary" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );

    poDriver->pfnOpen = OGRGeoPackageDriverOpen;
    poDriver->pfnIdentify = OGRGeoPackageDriverIdentify;
    poDriver->pfnCreate = OGRGeoPackageDriverCreate;
    poDriver->pfnCreateCopy = GDALGeoPackageDataset::CreateCopy;
    poDriver->pfnDelete = OGRGeoPackageDriverDelete;

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
