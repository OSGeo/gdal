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

#include "tilematrixset.hpp"

CPL_CVSID("$Id$")

// g++ -g -Wall -fPIC -shared -o ogr_geopackage.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gpkg ogr/ogrsf_frmts/gpkg/*.c* -L. -lgdal

/************************************************************************/
/*                       OGRGeoPackageDriverIdentify()                  */
/************************************************************************/

static int OGRGeoPackageDriverIdentify( GDALOpenInfo* poOpenInfo, bool bEmitWarning )
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GPKG:") )
        return TRUE;

#ifdef ENABLE_SQL_GPKG_FORMAT
    if( poOpenInfo->pabyHeader &&
        STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL GPKG") )
    {
        return TRUE;
    }
#endif

    if ( poOpenInfo->nHeaderBytes < 100 ||
         poOpenInfo->pabyHeader == nullptr ||
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

    /* Requirement 2: application id */
    /* http://opengis.github.io/geopackage/#_file_format */
    /* Be tolerant since some datasets don't actually follow that requirement */
    GUInt32 nApplicationId;
    memcpy(&nApplicationId, poOpenInfo->pabyHeader + knApplicationIdPos, 4);
    nApplicationId = CPL_MSBWORD32(nApplicationId);
    GUInt32 nUserVersion;
    memcpy(&nUserVersion, poOpenInfo->pabyHeader + knUserVersionPos, 4);
    nUserVersion = CPL_MSBWORD32(nUserVersion);
    if( nApplicationId != GP10_APPLICATION_ID &&
        nApplicationId != GP11_APPLICATION_ID &&
        nApplicationId != GPKG_APPLICATION_ID )
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
            GByte abySignature[4+1];
            memcpy(abySignature, poOpenInfo->pabyHeader + knApplicationIdPos, 4);
            abySignature[4] = '\0';

            /* Is this a GPxx version ? */
            const bool bWarn = CPLTestBool(CPLGetConfigOption(
                "GPKG_WARN_UNRECOGNIZED_APPLICATION_ID", "YES"));
            if( bWarn )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "GPKG: bad application_id=0x%02X%02X%02X%02X on '%s'",
                          abySignature[0], abySignature[1],
                          abySignature[2], abySignature[3],
                          poOpenInfo->pszFilename );
            }
            else
            {
                CPLDebug( "GPKG",
                          "bad application_id=0x%02X%02X%02X%02X on '%s'",
                          abySignature[0], abySignature[1],
                          abySignature[2], abySignature[3],
                          poOpenInfo->pszFilename );
            }
        }
    }
    else if(nApplicationId == GPKG_APPLICATION_ID &&
            // Accept any 102XX version
            !((nUserVersion >= GPKG_1_2_VERSION &&
               nUserVersion < GPKG_1_2_VERSION + 99) ||
            // Accept any 103XX version
              (nUserVersion >= GPKG_1_3_VERSION &&
               nUserVersion < GPKG_1_3_VERSION + 99)
              ))
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
            GByte abySignature[4+1];
            memcpy(abySignature, poOpenInfo->pabyHeader + knUserVersionPos, 4);
            abySignature[4] = '\0';

            const bool bWarn = CPLTestBool(CPLGetConfigOption(
                            "GPKG_WARN_UNRECOGNIZED_APPLICATION_ID", "YES"));
            if( bWarn )
            {
                if( nUserVersion > GPKG_1_3_VERSION )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "This version of GeoPackage "
                              "user_version=0x%02X%02X%02X%02X "
                              "(%u, v%d.%d.%d) on '%s' may only be "
                              "partially supported",
                              abySignature[0], abySignature[1],
                              abySignature[2], abySignature[3],
                              nUserVersion,
                              nUserVersion / 10000,
                              (nUserVersion % 10000 ) / 100,
                              nUserVersion % 100,
                              poOpenInfo->pszFilename );
                }
                else
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "GPKG: unrecognized user_version="
                              "0x%02X%02X%02X%02X (%u) on '%s'",
                              abySignature[0], abySignature[1],
                              abySignature[2], abySignature[3],
                              nUserVersion,
                              poOpenInfo->pszFilename );
                }
            }
            else
            {
                if( nUserVersion > GPKG_1_3_VERSION )
                {
                    CPLDebug( "GPKG",
                              "This version of GeoPackage "
                              "user_version=0x%02X%02X%02X%02X "
                              "(%u, v%d.%d.%d) on '%s' may only be "
                              "partially supported",
                              abySignature[0], abySignature[1],
                              abySignature[2], abySignature[3],
                              nUserVersion,
                              nUserVersion / 10000,
                              (nUserVersion % 10000 ) / 100,
                              nUserVersion % 100,
                              poOpenInfo->pszFilename );
                }
                else
                {
                    CPLDebug( "GPKG",
                              "unrecognized user_version=0x%02X%02X%02X%02X"
                              "(%u) on '%s'",
                              abySignature[0], abySignature[1],
                              abySignature[2], abySignature[3],
                              nUserVersion,
                              poOpenInfo->pszFilename );
                }
            }
        }
    }
    else if( !bIsRecognizedExtension
#ifdef DEBUG
              && !EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input")
#endif
              && !(STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") &&
                   EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "zip") )
              && !STARTS_WITH(poOpenInfo->pszFilename, "/vsigzip/") )
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
        return nullptr;

    GDALGeoPackageDataset   *poDS = new GDALGeoPackageDataset();

    if( !poDS->Open( poOpenInfo ) )
    {
        delete poDS;
        poDS = nullptr;
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
                 "The filename extension should be 'gpkg' instead of '%s' "
                 "to conform to the GPKG specification.",
                 pszExt);
    }

    GDALGeoPackageDataset   *poDS = new GDALGeoPackageDataset();

    if( !poDS->Create( pszFilename, nXSize, nYSize,
                       nBands, eDT, papszOptions ) )
    {
        delete poDS;
        poDS = nullptr;
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

class GDALGPKGDriver final: public GDALDriver
{
        bool m_bInitialized = false;

        void InitializeCreationOptionList();

    public:
        GDALGPKGDriver() = default;

        const char* GetMetadataItem(const char* pszName, const char* pszDomain) override
        {
            if( EQUAL(pszName, GDAL_DMD_CREATIONOPTIONLIST) )
            {
                InitializeCreationOptionList();
            }
            return GDALDriver::GetMetadataItem(pszName, pszDomain);
        }

        char** GetMetadata(const char* pszDomain) override
        {
            InitializeCreationOptionList();
            return GDALDriver::GetMetadata(pszDomain);
        }
};

#define COMPRESSION_OPTIONS \
"  <Option name='TILE_FORMAT' type='string-select' scope='raster' description='Format to use to create tiles' default='AUTO'>" \
"    <Value>AUTO</Value>" \
"    <Value>PNG_JPEG</Value>" \
"    <Value>PNG</Value>" \
"    <Value>PNG8</Value>" \
"    <Value>JPEG</Value>" \
"    <Value>WEBP</Value>" \
"    <Value>TIFF</Value>" \
"  </Option>" \
"  <Option name='QUALITY' type='int' min='1' max='100' scope='raster' description='Quality for JPEG and WEBP tiles' default='75'/>" \
"  <Option name='ZLEVEL' type='int' min='1' max='9' scope='raster' description='DEFLATE compression level for PNG tiles' default='6'/>" \
"  <Option name='DITHER' type='boolean' scope='raster' description='Whether to apply Floyd-Steinberg dithering (for TILE_FORMAT=PNG8)' default='NO'/>"

void GDALGPKGDriver::InitializeCreationOptionList()
{
    if( m_bInitialized )
        return;
    m_bInitialized = true;

    const char* pszCOBegin =
"<CreationOptionList>"
"  <Option name='RASTER_TABLE' type='string' scope='raster' description='Name of tile user table'/>"
"  <Option name='APPEND_SUBDATASET' type='boolean' scope='raster' description='Set to YES to add a new tile user table to an existing GeoPackage instead of replacing it' default='NO'/>"
"  <Option name='RASTER_IDENTIFIER' type='string' scope='raster' description='Human-readable identifier (e.g. short name)'/>"
"  <Option name='RASTER_DESCRIPTION' type='string' scope='raster' description='Human-readable description'/>"
"  <Option name='BLOCKSIZE' type='int' scope='raster' description='Block size in pixels' default='256' max='4096'/>"
"  <Option name='BLOCKXSIZE' type='int' scope='raster' description='Block width in pixels' default='256' max='4096'/>"
"  <Option name='BLOCKYSIZE' type='int' scope='raster' description='Block height in pixels' default='256' max='4096'/>"
COMPRESSION_OPTIONS
"  <Option name='TILING_SCHEME' type='string' scope='raster' description='Which tiling scheme to use: pre-defined value or custom inline/outline JSON definition' default='CUSTOM'>"
"    <Value>CUSTOM</Value>"
"    <Value>GoogleCRS84Quad</Value>"
"    <Value>PseudoTMS_GlobalGeodetic</Value>"
"    <Value>PseudoTMS_GlobalMercator</Value>";

            const char* pszCOEnd =
"  </Option>"
"  <Option name='ZOOM_LEVEL_STRATEGY' type='string-select' scope='raster' description='Strategy to determine zoom level. Only used for TILING_SCHEME != CUSTOM' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>LOWER</Value>"
"    <Value>UPPER</Value>"
"  </Option>"
"  <Option name='RESAMPLING' type='string-select' scope='raster' description='Resampling algorithm. Only used for TILING_SCHEME != CUSTOM' default='BILINEAR'>"
"    <Value>NEAREST</Value>"
"    <Value>BILINEAR</Value>"
"    <Value>CUBIC</Value>"
"    <Value>CUBICSPLINE</Value>"
"    <Value>LANCZOS</Value>"
"    <Value>MODE</Value>"
"    <Value>AVERAGE</Value>"
"  </Option>"
"  <Option name='PRECISION' type='float' scope='raster' description='Smallest significant value. Only used for tiled gridded coverage datasets' default='1'/>"
"  <Option name='UOM' type='string' scope='raster' description='Unit of Measurement. Only used for tiled gridded coverage datasets' />"
"  <Option name='FIELD_NAME' type='string' scope='raster' description='Field name. Only used for tiled gridded coverage datasets' default='Height'/>"
"  <Option name='QUANTITY_DEFINITION' type='string' scope='raster' description='Description of the field. Only used for tiled gridded coverage datasets' default='Height'/>"
"  <Option name='GRID_CELL_ENCODING' type='string-select' scope='raster' description='Grid cell encoding. Only used for tiled gridded coverage datasets' default='grid-value-is-center'>"
"     <Value>grid-value-is-center</Value>"
"     <Value>grid-value-is-area</Value>"
"     <Value>grid-value-is-corner</Value>"
"  </Option>"
"  <Option name='VERSION' type='string-select' description='Set GeoPackage version (for application_id and user_version fields)' default='AUTO'>"
"     <Value>AUTO</Value>"
"     <Value>1.0</Value>"
"     <Value>1.1</Value>"
"     <Value>1.2</Value>"
"     <Value>1.3</Value>"
"  </Option>"
"  <Option name='DATETIME_FORMAT' type='string-select' description='How to encode DateTime not in UTC' default='WITH_TZ'>"
"     <Value>WITH_TZ</Value>"
"     <Value>UTC</Value>"
"  </Option>"
#ifdef ENABLE_GPKG_OGR_CONTENTS
"  <Option name='ADD_GPKG_OGR_CONTENTS' type='boolean' description='Whether to add a gpkg_ogr_contents table to keep feature count' default='YES'/>"
#endif
"</CreationOptionList>";

    std::string osOptions(pszCOBegin);
    const auto tmsList = gdal::TileMatrixSet::listPredefinedTileMatrixSets();
    for( const auto& tmsName: tmsList )
    {
        const auto poTM = gdal::TileMatrixSet::parse(tmsName.c_str());
        if( poTM &&
            poTM->haveAllLevelsSameTopLeft() &&
            poTM->haveAllLevelsSameTileSize() &&
            poTM->hasOnlyPowerOfTwoVaryingScales() &&
            !poTM->hasVariableMatrixWidth() )
        {
            osOptions += "    <Value>";
            osOptions += tmsName;
            osOptions += "</Value>";
        }
    }
    osOptions += pszCOEnd;

    SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, osOptions.c_str());
}


void RegisterOGRGeoPackage()
{
    if( GDALGetDriverByName( "GPKG" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALGPKGDriver();

    poDriver->SetDescription( "GPKG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GeoPackage" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gpkg" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/geopackage.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte Int16 UInt16 Float32" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList>"
"  <Option name='LIST_ALL_TABLES' type='string-select' scope='vector' description='Whether all tables, including those non listed in gpkg_contents, should be listed' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"  <Option name='TABLE' type='string' scope='raster' description='Name of tile user-table'/>"
"  <Option name='ZOOM_LEVEL' type='integer' scope='raster' description='Zoom level of full resolution. If not specified, maximum non-empty zoom level'/>"
"  <Option name='BAND_COUNT' type='int' min='1' max='4' scope='raster' description='Number of raster bands' default='4'/>"
"  <Option name='MINX' type='float' scope='raster' description='Minimum X of area of interest'/>"
"  <Option name='MINY' type='float' scope='raster' description='Minimum Y of area of interest'/>"
"  <Option name='MAXX' type='float' scope='raster' description='Maximum X of area of interest'/>"
"  <Option name='MAXY' type='float' scope='raster' description='Maximum Y of area of interest'/>"
"  <Option name='USE_TILE_EXTENT' type='boolean' scope='raster' description='Use tile extent of content to determine area of interest' default='NO'/>"
"  <Option name='WHERE' type='string' scope='raster' description='SQL WHERE clause to be appended to tile requests'/>"
COMPRESSION_OPTIONS
"  <Option name='PRELUDE_STATEMENTS' type='string' scope='raster,vector' description='SQL statement(s) to send on the SQLite connection before any other ones'/>"
"</OpenOptionList>");

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
"  <Option name='ASPATIAL_VARIANT' type='string-select' description='How to register non spatial tables' default='GPKG_ATTRIBUTES'>"
"     <Value>GPKG_ATTRIBUTES</Value>"
"     <Value>OGR_ASPATIAL</Value>"
"     <Value>NOT_REGISTERED</Value>"
"  </Option>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date DateTime "
                               "Binary" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean Int16 Float32" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_UNIQUE_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );

#ifdef ENABLE_SQL_GPKG_FORMAT
    poDriver->SetMetadataItem("ENABLE_SQL_GPKG_FORMAT", "YES");
#endif
#ifdef SQLITE_HAS_COLUMN_METADATA
    poDriver->SetMetadataItem("SQLITE_HAS_COLUMN_METADATA", "YES");
#endif

    poDriver->pfnOpen = OGRGeoPackageDriverOpen;
    poDriver->pfnIdentify = OGRGeoPackageDriverIdentify;
    poDriver->pfnCreate = OGRGeoPackageDriverCreate;
    poDriver->pfnCreateCopy = GDALGeoPackageDataset::CreateCopy;
    poDriver->pfnDelete = OGRGeoPackageDriverDelete;

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
