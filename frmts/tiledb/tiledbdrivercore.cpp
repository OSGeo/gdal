/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB Support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2023, TileDB, Inc
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

#define INCLUDE_ONLY_TILEDB_VERSION
#include "include_tiledb.h"

#include "tiledbdrivercore.h"

/************************************************************************/
/*                 TileDBDriverIdentifySimplified()                     */
/************************************************************************/

static int TileDBDriverIdentifySimplified(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB:"))
    {
        return TRUE;
    }

    const char *pszConfig =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_CONFIG");

    if (pszConfig != nullptr)
    {
        return TRUE;
    }

    const bool bIsS3OrGS = STARTS_WITH_CI(poOpenInfo->pszFilename, "/VSIS3/") ||
                           STARTS_WITH_CI(poOpenInfo->pszFilename, "/VSIGS/");
    // If this is a /vsi virtual file systems, bail out, except if it is S3 or GS.
    if (!bIsS3OrGS && STARTS_WITH(poOpenInfo->pszFilename, "/vsi"))
    {
        return false;
    }

    if (poOpenInfo->bIsDirectory)
    {
        return GDAL_IDENTIFY_UNKNOWN;
    }

    if (bIsS3OrGS && !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "tif"))
    {
        return GDAL_IDENTIFY_UNKNOWN;
    }

    return FALSE;
}

/************************************************************************/
/*                      TileDBDriverSetCommonMetadata()                 */
/************************************************************************/

#define XSTRINGIFY(X) #X
#define STRINGIFY(X) XSTRINGIFY(X)

void TileDBDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_SUBCREATECOPY, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "TileDB");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/tiledb.html");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 UInt32 Int32 Float32 "
                              "Float64 CInt16 CInt32 CFloat32 CFloat64");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date Time DateTime "
        "IntegerList Integer64List RealList Binary");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Boolean Int16 Float32");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='COMPRESSION' scope='raster' type='string-select' "
        "description='image "
        "compression to use' default='NONE'>\n"
        "       <Value>NONE</Value>\n"
        "       <Value>GZIP</Value>\n"
        "       <Value>ZSTD</Value>\n"
        "       <Value>LZ4</Value>\n"
        "       <Value>RLE</Value>\n"
        "       <Value>BZIP2</Value>\n"
        "       <Value>DOUBLE-DELTA</Value>\n"
        "       <Value>POSITIVE-DELTA</Value>\n"
        "   </Option>\n"
        "   <Option name='COMPRESSION_LEVEL' scope='raster' type='int' "
        "description='Compression level'/>\n"
        "   <Option name='BLOCKXSIZE' scope='raster' type='int' "
        "description='Tile Width'/>"
        "   <Option name='BLOCKYSIZE' scope='raster' type='int' "
        "description='Tile Height'/>"
        "   <Option name='STATS' scope='raster' type='boolean' default='false' "
        "description='Dump TileDB stats'/>"
        "   <Option name='TILEDB_CONFIG' type='string' "
        "description='location "
        "of configuration file for TileDB'/>"
        "   <Option name='TILEDB_ATTRIBUTE' scope='raster' type='string' "
        "description='co-registered file to add as TileDB attributes, only "
        "applicable for interleave types of band or pixel'/>"
        "   <Option name='INTERLEAVE' scope='raster' type='string-select' "
        "description='Indexing order' default='BAND'>\n"
        "        <Value>BAND</Value>\n"
        "        <Value>PIXEL</Value>\n"
        "        <Value>ATTRIBUTES</Value>\n"
        "   </Option>\n"
        "   <Option name='TILEDB_TIMESTAMP' scope='raster' type='int' "
        "description='Create "
        "array at this timestamp, the timestamp should be > 0'/>\n"
        "   <Option name='BOUNDS' scope='raster' type='string' "
        "description='Specify "
        "bounds for sparse array, minx, miny, maxx, maxy'/>\n"
        "   <Option name='CREATE_GROUP' scope='vector' type='boolean' "
        "description='Whether to create a group for multiple layer support' "
        "default='NO'/>"
        "</CreationOptionList>\n");

    // clang-format off
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='STATS' scope='raster' type='boolean' default='false' "
        "description='Dump TileDB stats'/>"
        "   <Option name='TILEDB_ATTRIBUTE' scope='raster' type='string' "
        "description='Attribute to read from each band'/>"
        "   <Option name='TILEDB_CONFIG' type='string' description='location "
        "of configuration file for TileDB'/>"
        "   <Option name='TILEDB_TIMESTAMP' type='int' description='Open array "
        "at this timestamp, the timestamp should be > 0'/>"
        "   <Option name='BATCH_SIZE' scope='vector' type='int' default='"
        STRINGIFY(DEFAULT_BATCH_SIZE) "' "
        "description='Number of features to fetch/write at once'/>"
        "   <Option name='DIM_X' type='string' scope='vector' default='_X' "
        "description='Name of the X dimension.'/>"
        "   <Option name='DIM_Y' type='string' scope='vector' default='_Y' "
        "description='Name of the Y dimension.'/>"
        "   <Option name='DIM_Z' type='string' scope='vector' default='_Z' "
        "description='Name of the Z dimension.'/>"
        "</OpenOptionList>");
    // clang-format on

    // clang-format off
    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "   <Option name='COMPRESSION' type='string-select' description='"
        "Compression to use' default='NONE'>\n"
        "       <Value>NONE</Value>\n"
        "       <Value>GZIP</Value>\n"
        "       <Value>ZSTD</Value>\n"
        "       <Value>LZ4</Value>\n"
        "       <Value>RLE</Value>\n"
        "       <Value>BZIP2</Value>\n"
        "       <Value>DOUBLE-DELTA</Value>\n"
        "       <Value>POSITIVE-DELTA</Value>\n"
        "   </Option>\n"
        "   <Option name='COMPRESSION_LEVEL' type='int' "
        "description='Compression level'/>\n"
        "   <Option name='BATCH_SIZE' type='int' default='"
        STRINGIFY(DEFAULT_BATCH_SIZE) "' "
        "description='Number of features to write at once'/>"
        "   <Option name='TILE_CAPACITY' type='int' default='"
        STRINGIFY(DEFAULT_TILE_CAPACITY) "' "
        "description='Number of non-empty cells stored in a data tile'/>"
        "   <Option name='BOUNDS' type='string' description='Specify "
        "bounds for sparse array, minx, miny, [minz,] maxx, maxy [, maxz]'/>\n"
        "   <Option name='TILE_EXTENT' type='float' description='Specify "
        "square X/Y tile extents for a sparse array'/>\n"
        "   <Option name='TILE_Z_EXTENT' type='float' description='Specify "
        "Z tile extents for a sparse array'/>\n"
        "   <Option name='ADD_Z_DIM' type='string-select' description='"
        "Whether to add a Z dimension' default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>YES</Value>"
        "       <Value>NO</Value>"
        "   </Option>"
        "   <Option name='FID' type='string' description='Feature id column "
        "name. Set to empty to disable its creation.' default='FID'/>"
        "   <Option name='GEOMETRY_NAME' type='string' description='Name "
        "of the geometry column that will receive WKB encoded geometries. "
        "Set to empty to disable its creation (only for point).' "
        "default='wkb_geometry'/>"
        "   <Option name='TILEDB_TIMESTAMP' type='int' description='Create "
        "array at this timestamp, the timestamp should be > 0'/>"
        "   <Option name='TILEDB_STRING_TYPE' type='string-select' "
        "description='Which TileDB type to create string attributes' "
       "default='UTF8'"
        ">"
        "       <Value>UTF8</Value>"
        "       <Value>ASCII</Value>"
        "   </Option>"
        "   <Option name='STATS' type='boolean' default='false' "
        "description='Dump TileDB stats'/>"
        "</LayerCreationOptionList>");
    // clang-format on

    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_MULTIDIMENSIONAL, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST,
        "<MultiDimDatasetCreationOptionList>"
        "   <Option name='TILEDB_CONFIG' type='string' description='location "
        "of configuration file for TileDB'/>"
        "   <Option name='TILEDB_TIMESTAMP' type='int' description='Create "
        "arrays at this timestamp, the timestamp should be > 0'/>"
        "   <Option name='STATS' type='boolean' default='false' "
        "description='Dump TileDB stats'/>"
        "</MultiDimDatasetCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_MULTIDIM_ARRAY_OPENOPTIONLIST,
        "<MultiDimArrayOpenOptionList>"
        "   <Option name='TILEDB_TIMESTAMP' type='int' description='Open "
        "array at this timestamp, the timestamp should be > 0'/>"
        "</MultiDimArrayOpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST,
        "<MultiDimArrayCreationOptionList>"
        "   <Option name='TILEDB_TIMESTAMP' type='int' description='Create "
        "array at this timestamp, the timestamp should be > 0'/>"
        "   <Option name='BLOCKSIZE' type='int' description='Block size in "
        "pixels'/>"
        "   <Option name='COMPRESSION' type='string-select' description='"
        "Compression to use' default='NONE'>\n"
        "       <Value>NONE</Value>\n"
        "       <Value>GZIP</Value>\n"
        "       <Value>ZSTD</Value>\n"
        "       <Value>LZ4</Value>\n"
        "       <Value>RLE</Value>\n"
        "       <Value>BZIP2</Value>\n"
        "       <Value>DOUBLE-DELTA</Value>\n"
        "       <Value>POSITIVE-DELTA</Value>\n"
        "   </Option>\n"
        "   <Option name='COMPRESSION_LEVEL' type='int' "
        "description='Compression level'/>\n"
        "   <Option name='STATS' type='boolean' default='false' "
        "description='Dump TileDB stats'/>"
        "   <Option name='IN_MEMORY'  type='boolean' default='false' "
        "description='Whether the array should be only in-memory. Useful to "
        "create an indexing variable that is serialized as a dimension label'/>"
        "</MultiDimArrayCreationOptionList>");

    poDriver->pfnIdentify = TileDBDriverIdentifySimplified;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                   DeclareDeferredTileDBPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredTileDBPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    TileDBDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
