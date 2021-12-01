/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 *
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * Portions of this module properly supporting SpatiaLite DB creation
 * Developed for Faunalia ( http://www.faunalia.it) with funding from
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_sqlite.h"

#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "sqlite3.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                     OGRSQLiteDriverIdentify()                        */
/************************************************************************/

static int OGRSQLiteDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "SQLITE:") )
    {
        return TRUE;
    }

    CPLString osExt(CPLGetExtension(poOpenInfo->pszFilename));
    if( EQUAL(osExt, "gpkg") && GDALGetDriverByName("GPKG") != nullptr )
    {
        return FALSE;
    }
    if( EQUAL(osExt, "mbtiles") && GDALGetDriverByName("MBTILES") != nullptr )
    {
        return FALSE;
    }

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "VirtualShape:") &&
        EQUAL(osExt, "shp"))
    {
        return TRUE;
    }

#ifdef HAVE_RASTERLITE2
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "RASTERLITE2:") )
        return poOpenInfo->nOpenFlags & GDAL_OF_RASTER;
#endif

    if( EQUAL(poOpenInfo->pszFilename, ":memory:") )
        return TRUE;

#ifdef SQLITE_OPEN_URI
    // This code enables support for named memory databases in SQLite.
    // Named memory databases use file name format
    //   file:name?mode=memory&cache=shared
    // SQLITE_USE_URI is checked only to enable backward compatibility, in case
    // we accidentally hijacked some other format.
    if( STARTS_WITH(poOpenInfo->pszFilename, "file:") &&
        CPLTestBool(CPLGetConfigOption("SQLITE_USE_URI", "YES")) )
    {
        char * queryparams = strchr(poOpenInfo->pszFilename, '?');
        if( queryparams )
        {
            if( strstr(queryparams, "mode=memory") != nullptr )
                return TRUE;
        }
    }
#endif

/* -------------------------------------------------------------------- */
/*      Verify that the target is a real file, and has an               */
/*      appropriate magic string at the beginning.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 100 )
        return FALSE;

#ifdef ENABLE_SQL_SQLITE_FORMAT
    if( STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL SQLITE") )
    {
        return TRUE;
    }
    if( STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL RASTERLITE") )
    {
        return -1;
    }
    if( STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL MBTILES") )
    {
        if( GDALGetDriverByName("MBTILES") != nullptr )
            return FALSE;
        if( poOpenInfo->eAccess == GA_Update )
            return FALSE;
        return -1;
    }
#endif

    if( !STARTS_WITH((const char*)poOpenInfo->pabyHeader, "SQLite format 3") )
        return FALSE;

    // In case we are opening /vsizip/foo.zip with a .gpkg inside
    if( (memcmp(poOpenInfo->pabyHeader + 68, "GP10", 4) == 0 ||
         memcmp(poOpenInfo->pabyHeader + 68, "GP11", 4) == 0 ||
         memcmp(poOpenInfo->pabyHeader + 68, "GPKG", 4) == 0) &&
        GDALGetDriverByName("GPKG") != nullptr )
    {
        return FALSE;
    }

    // Could be a Rasterlite or MBTiles file as well
    return -1;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRSQLiteDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( OGRSQLiteDriverIdentify(poOpenInfo) == FALSE )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Check VirtualShape:xxx.shp syntax                               */
/* -------------------------------------------------------------------- */
    int nLen = (int) strlen(poOpenInfo->pszFilename);
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "VirtualShape:") &&
        nLen > 4 && EQUAL(poOpenInfo->pszFilename + nLen - 4, ".SHP"))
    {
        OGRSQLiteDataSource *poDS = new OGRSQLiteDataSource();

        char** papszOptions = CSLAddString(nullptr, "SPATIALITE=YES");
        int nRet = poDS->Create( ":memory:", papszOptions );
        poDS->SetDescription(poOpenInfo->pszFilename);
        CSLDestroy(papszOptions);
        if (!nRet)
        {
            delete poDS;
            return nullptr;
        }

        char* pszSQLiteFilename = CPLStrdup(poOpenInfo->pszFilename + strlen( "VirtualShape:" ));
        GDALDataset* poSQLiteDS = (GDALDataset*) GDALOpenEx(pszSQLiteFilename,
                                            GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
        if (poSQLiteDS == nullptr)
        {
            CPLFree(pszSQLiteFilename);
            delete poDS;
            return nullptr;
        }
        delete poSQLiteDS;

        char* pszLastDot = strrchr(pszSQLiteFilename, '.');
        if (pszLastDot)
            *pszLastDot = '\0';

        const char* pszTableName = CPLGetBasename(pszSQLiteFilename);

        char* pszSQL = CPLStrdup(CPLSPrintf("CREATE VIRTUAL TABLE %s USING VirtualShape(%s, CP1252, -1)",
                                            pszTableName, pszSQLiteFilename));
        poDS->ExecuteSQL(pszSQL, nullptr, nullptr);
        CPLFree(pszSQL);
        CPLFree(pszSQLiteFilename);
        poDS->DisableUpdate();
        return poDS;
    }

/* -------------------------------------------------------------------- */
/*      We think this is really an SQLite database, go ahead and try    */
/*      and open it.                                                    */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource *poDS = new OGRSQLiteDataSource();

    if( !poDS->Open( poOpenInfo ) )
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRSQLiteDriverCreate( const char * pszName,
                                           int nBands,
                                           CPL_UNUSED int nXSize,
                                           CPL_UNUSED int nYSize,
                                           CPL_UNUSED GDALDataType eDT,
                                           char **papszOptions )
{
    if( nBands != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Raster creation through Create() interface is not supported. "
                 "Only CreateCopy() is supported");
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to create datasource.                                       */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource *poDS = new OGRSQLiteDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                             Delete()                                 */
/************************************************************************/

static CPLErr OGRSQLiteDriverDelete( const char *pszName )
{
    if (VSIUnlink( pszName ) == 0)
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                         RegisterOGRSQLite()                          */
/************************************************************************/

void RegisterOGRSQLite()

{
    if( !GDAL_CHECK_VERSION("SQLite driver") )
        return;

    if( GDALGetDriverByName( "SQLite" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SQLite" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
#ifdef HAVE_RASTERLITE2
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "SQLite / Spatialite / RasterLite2" );
#else
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "SQLite / Spatialite" );
#endif
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/sqlite.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "sqlite db" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='LIST_ALL_TABLES' type='boolean' description='Whether all tables, including non-spatial ones, should be listed' default='NO'/>"
"  <Option name='LIST_VIRTUAL_OGR' type='boolean' description='Whether VirtualOGR virtual tables should be listed. Should only be enabled on trusted datasources to avoid potential safety issues' default='NO'/>"
"  <Option name='PRELUDE_STATEMENTS' type='string' description='SQL statement(s) to send on the SQLite connection before any other ones'/>"
#ifdef HAVE_RASTERLITE2
"  <Option name='1BIT_AS_8BIT' type='boolean' scope='raster' description='Whether to promote 1-bit monochrome raster as 8-bit, so as to have higher quality overviews' default='YES'/>"
#endif
"</OpenOptionList>");

    CPLString osCreationOptions(
"<CreationOptionList>"
#ifdef HAVE_SPATIALITE
"  <Option name='SPATIALITE' type='boolean' description='Whether to create a Spatialite database' default='NO'/>"
#endif
"  <Option name='METADATA' type='boolean' description='Whether to create the geometry_columns and spatial_ref_sys tables' default='YES'/>"
"  <Option name='INIT_WITH_EPSG' type='boolean' description='Whether to insert the content of the EPSG CSV files into the spatial_ref_sys table ' default='NO'/>"
#ifdef HAVE_RASTERLITE2
"  <Option name='APPEND_SUBDATASET' scope='raster' type='boolean' description='Whether to add the raster to the existing file' default='NO'/>"
"  <Option name='COVERAGE' scope='raster' type='string' description='Coverage name'/>"
"  <Option name='SECTION' scope='raster' type='string' description='Section name'/>"
"  <Option name='COMPRESS' scope='raster' type='string-select' description='Raster compression' default='NONE'>"
"    <Value>NONE</Value>"
#endif
    );
#ifdef HAVE_RASTERLITE2
    if( rl2_is_supported_codec( RL2_COMPRESSION_DEFLATE ) )
        osCreationOptions += "    <Value>DEFLATE</Value>";
    if( rl2_is_supported_codec( RL2_COMPRESSION_LZMA ) )
        osCreationOptions += "    <Value>LZMA</Value>";
    if( rl2_is_supported_codec( RL2_COMPRESSION_PNG ) )
        osCreationOptions += "    <Value>PNG</Value>";
    if( rl2_is_supported_codec( RL2_COMPRESSION_CCITTFAX4 ) )
        osCreationOptions += "    <Value>CCITTFAX4</Value>";
    if( rl2_is_supported_codec( RL2_COMPRESSION_JPEG ) )
        osCreationOptions += "    <Value>JPEG</Value>";
    if( rl2_is_supported_codec( RL2_COMPRESSION_LOSSY_WEBP ) )
        osCreationOptions += "    <Value>WEBP</Value>";
    if( rl2_is_supported_codec( RL2_COMPRESSION_LOSSY_JP2 ) )
        osCreationOptions += "    <Value>JPEG2000</Value>";
#endif
    osCreationOptions +=
#ifdef HAVE_RASTERLITE2
"  </Option>"
"  <Option name='QUALITY' scope='raster' type='int' description='Image quality for JPEG, WEBP and JPEG2000 compressions'/>"
"  <Option name='PIXEL_TYPE' scope='raster' type='string-select' description='Raster pixel type. Determines photometric interpretation'>"
"    <Value>MONOCHROME</Value>"
"    <Value>PALETTE</Value>"
"    <Value>GRAYSCALE</Value>"
"    <Value>RGB</Value>"
"    <Value>MULTIBAND</Value>"
"    <Value>DATAGRID</Value>"
"  </Option>"
"  <Option name='BLOCKXSIZE' scope='raster' type='int' description='Block width' default='512'/>"
"  <Option name='BLOCKYSIZE' scope='raster' type='int' description='Block height' default='512'/>"
"  <Option name='NBITS' scope='raster' type='int' description='Force bit width. 1, 2 or 4 are supported'/>"
"  <Option name='PYRAMIDIZE' scope='raster' type='boolean' description='Whether to automatically build relevant pyramids/overviews' default='NO'/>"
#endif
"</CreationOptionList>";

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, osCreationOptions);

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='FORMAT' type='string-select' description='Format of geometry columns'>"
"    <Value>WKB</Value>"
"    <Value>WKT</Value>"
#ifdef HAVE_SPATIALITE
"    <Value>SPATIALITE</Value>"
#endif
"  </Option>"
"  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column. Defaults to WKT_GEOMETRY for FORMAT=WKT or GEOMETRY otherwise'/>"
"  <Option name='LAUNDER' type='boolean' description='Whether layer and field names will be laundered' default='YES'/>"
#ifdef HAVE_SPATIALITE
"  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index for Spatialite databases' default='YES'/>"
"  <Option name='COMPRESS_GEOM' type='boolean' description='Whether to use compressed format of Spatialite geometries' default='NO'/>"
#endif
"  <Option name='SRID' type='int' description='Forced SRID of the layer'/>"
"  <Option name='COMPRESS_COLUMNS' type='string' description='=column_name1[,column_name2, ...].  list of (String) columns that must be compressed with ZLib DEFLATE algorithm'/>"
"  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
"  <Option name='FID' type='string' description='Name of the FID column to create' default='OGC_FID'/>"
#if SQLITE_VERSION_NUMBER >= 3037000
"  <Option name='STRICT' type='boolean' description='Whether to create the table in STRICT mode (only compatible of readers with sqlite >= 3.37)' default='NO'/>"
#endif
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date DateTime "
                               "Time Binary IntegerList Integer64List "
                               "RealList StringList" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean Int16 Float32" );

#ifdef HAVE_RASTERLITE2
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 "
                               "Float64" );
#endif
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_UNIQUE_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

#ifdef ENABLE_SQL_SQLITE_FORMAT
    poDriver->SetMetadataItem("ENABLE_SQL_SQLITE_FORMAT", "YES");
#endif
#ifdef SQLITE_HAS_COLUMN_METADATA
    poDriver->SetMetadataItem("SQLITE_HAS_COLUMN_METADATA", "YES");
#endif

    poDriver->pfnOpen = OGRSQLiteDriverOpen;
    poDriver->pfnIdentify = OGRSQLiteDriverIdentify;
    poDriver->pfnCreate = OGRSQLiteDriverCreate;
#ifdef HAVE_RASTERLITE2
    poDriver->pfnCreateCopy = OGRSQLiteDriverCreateCopy;
#endif
    poDriver->pfnDelete = OGRSQLiteDriverDelete;
    poDriver->pfnUnloadDriver = OGRSQLiteDriverUnload;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
