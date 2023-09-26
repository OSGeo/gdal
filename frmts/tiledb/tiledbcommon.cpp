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

#include "tiledbheaders.h"

#include "tiledb/tiledb"

/************************************************************************/
/*                      VSI_to_tiledb_uri()                             */
/************************************************************************/

CPLString TileDBDataset::VSI_to_tiledb_uri(const char *pszUri)
{
    CPLString osUri;

    if (STARTS_WITH_CI(pszUri, "/VSIS3/"))
        osUri.Printf("s3://%s", pszUri + 7);
    else if (STARTS_WITH_CI(pszUri, "/VSIGS/"))
        osUri.Printf("gcs://%s", pszUri + 7);
    else
    {
        osUri = pszUri;
        // tiledb (at least at 2.4.2 on Conda) wrongly interprets relative
        // directories on Windows as absolute ones.
        if (CPLIsFilenameRelative(pszUri))
        {
            char *pszCurDir = CPLGetCurrentDir();
            if (pszCurDir)
                osUri = CPLFormFilename(pszCurDir, pszUri, nullptr);
            CPLFree(pszCurDir);
        }
    }

    return osUri;
}

/************************************************************************/
/*                           AddFilter()                                */
/************************************************************************/

CPLErr TileDBDataset::AddFilter(tiledb::Context &ctx,
                                tiledb::FilterList &filterList,
                                const char *pszFilterName, const int level)

{
    try
    {
        if (pszFilterName == nullptr)
            filterList.add_filter(
                tiledb::Filter(ctx, TILEDB_FILTER_NONE)
                    .set_option(TILEDB_COMPRESSION_LEVEL, level));
        else if EQUAL (pszFilterName, "GZIP")
            filterList.add_filter(
                tiledb::Filter(ctx, TILEDB_FILTER_GZIP)
                    .set_option(TILEDB_COMPRESSION_LEVEL, level));
        else if EQUAL (pszFilterName, "ZSTD")
            filterList.add_filter(
                tiledb::Filter(ctx, TILEDB_FILTER_ZSTD)
                    .set_option(TILEDB_COMPRESSION_LEVEL, level));
        else if EQUAL (pszFilterName, "LZ4")
            filterList.add_filter(
                tiledb::Filter(ctx, TILEDB_FILTER_LZ4)
                    .set_option(TILEDB_COMPRESSION_LEVEL, level));
        else if EQUAL (pszFilterName, "RLE")
            filterList.add_filter(
                tiledb::Filter(ctx, TILEDB_FILTER_RLE)
                    .set_option(TILEDB_COMPRESSION_LEVEL, level));
        else if EQUAL (pszFilterName, "BZIP2")
            filterList.add_filter(
                tiledb::Filter(ctx, TILEDB_FILTER_BZIP2)
                    .set_option(TILEDB_COMPRESSION_LEVEL, level));
        else if EQUAL (pszFilterName, "DOUBLE-DELTA")
            filterList.add_filter(
                tiledb::Filter(ctx, TILEDB_FILTER_DOUBLE_DELTA));
        else if EQUAL (pszFilterName, "POSITIVE-DELTA")
            filterList.add_filter(
                tiledb::Filter(ctx, TILEDB_FILTER_POSITIVE_DELTA));
        else
            return CE_Failure;

        return CE_None;
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return CE_Failure;
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int TileDBDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB:"))
    {
        return TRUE;
    }

    try
    {
        const char *pszConfig =
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_CONFIG");

        if (pszConfig != nullptr)
        {
            return TRUE;
        }

        if (poOpenInfo->bIsDirectory ||
            ((STARTS_WITH_CI(poOpenInfo->pszFilename, "/VSIS3/") ||
              STARTS_WITH_CI(poOpenInfo->pszFilename, "/VSIGS/")) &&
             !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "tif")))
        {
            tiledb::Context ctx;
            CPLString osArrayPath =
                TileDBDataset::VSI_to_tiledb_uri(poOpenInfo->pszFilename);
            const auto eType = tiledb::Object::object(ctx, osArrayPath).type();
#ifdef HAS_TILEDB_GROUP
            if ((poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0)
            {
                if (eType == tiledb::Object::Type::Array ||
                    eType == tiledb::Object::Type::Group)
                    return true;
            }
#endif
#ifdef HAS_TILEDB_MULTIDIM
            if ((poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) != 0)
            {
                if (eType == tiledb::Object::Type::Array ||
                    eType == tiledb::Object::Type::Group)
                    return true;
            }
#endif
            if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0)
            {
                return eType == tiledb::Object::Type::Array;
            }
        }

        return FALSE;
    }
    catch (...)
    {
        return FALSE;
    }
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

CPLErr TileDBDataset::Delete(const char *pszFilename)

{
    try
    {
        tiledb::Context ctx;
        tiledb::VFS vfs(ctx);
        CPLString osArrayPath = TileDBDataset::VSI_to_tiledb_uri(pszFilename);

        if (vfs.is_dir(osArrayPath))
        {
            vfs.remove_dir(osArrayPath);
            return CE_None;
        }
        else
            return CE_Failure;
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return CE_Failure;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *TileDBDataset::Open(GDALOpenInfo *poOpenInfo)

{
    try
    {
        if (!TileDBDataset::Identify(poOpenInfo))
            return nullptr;

        if (STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB:") &&
            !STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB://"))
        {
            // subdataset URI so this is a raster
            return TileDBRasterDataset::Open(poOpenInfo);
        }
        else
        {
#ifdef HAS_TILEDB_MULTIDIM
            if ((poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) != 0)
            {
                return TileDBDataset::OpenMultiDimensional(poOpenInfo);
            }
#endif

            const char *pszConfig = CSLFetchNameValue(
                poOpenInfo->papszOpenOptions, "TILEDB_CONFIG");
            tiledb::Context oCtx;

            if (pszConfig != nullptr)
            {
                tiledb::Config cfg(pszConfig);
                oCtx = tiledb::Context(cfg);
            }
            else
            {
                tiledb::Config cfg;
                cfg["sm.enable_signal_handlers"] = "false";
                oCtx = tiledb::Context(cfg);
            }
            const std::string osPath =
                TileDBDataset::VSI_to_tiledb_uri(poOpenInfo->pszFilename);

            const auto eType = tiledb::Object::object(oCtx, osPath).type();
#ifdef HAS_TILEDB_GROUP
            if ((poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
                eType == tiledb::Object::Type::Group)
            {
                return OGRTileDBDataset::Open(poOpenInfo, eType);
            }
#endif

            tiledb::ArraySchema schema(oCtx, osPath);

            if (schema.array_type() == TILEDB_SPARSE)
                return OGRTileDBDataset::Open(poOpenInfo, eType);
            else
                return TileDBRasterDataset::Open(poOpenInfo);
        }
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

GDALDataset *TileDBDataset::Create(const char *pszFilename, int nXSize,
                                   int nYSize, int nBandsIn, GDALDataType eType,
                                   char **papszOptions)

{
    try
    {
        if (nBandsIn > 0)
            return TileDBRasterDataset::Create(pszFilename, nXSize, nYSize,
                                               nBandsIn, eType, papszOptions);
        else
            return OGRTileDBDataset::Create(pszFilename, papszOptions);
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
    }

    return nullptr;
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *TileDBDataset::CreateCopy(const char *pszFilename,
                                       GDALDataset *poSrcDS, int bStrict,
                                       char **papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void *pProgressData)

{
#ifdef HAS_TILEDB_MULTIDIM
    if (poSrcDS->GetRootGroup())
    {
        auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("TileDB"));
        if (poDrv)
        {
            return poDrv->DefaultCreateCopy(pszFilename, poSrcDS, bStrict,
                                            papszOptions, pfnProgress,
                                            pProgressData);
        }
    }
#endif

    try
    {
        if (poSrcDS->GetRasterCount() > 0 ||
            poSrcDS->GetMetadata("SUBDATASETS"))
        {
            return TileDBRasterDataset::CreateCopy(pszFilename, poSrcDS,
                                                   bStrict, papszOptions,
                                                   pfnProgress, pProgressData);
        }
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
    }

    return nullptr;
}

/************************************************************************/
/*                         GDALRegister_TILEDB()                        */
/************************************************************************/

#define XSTRINGIFY(X) #X
#define STRINGIFY(X) XSTRINGIFY(X)

void GDALRegister_TileDB()

{
    if (GDALGetDriverByName("TileDB") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("TileDB");
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
#ifdef HAS_TILEDB_BOOL
                              "Boolean Int16 Float32"
#else
                              "Int16 Float32"
#endif
    );
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
#ifdef HAS_TILEDB_GROUP
        "   <Option name='CREATE_GROUP' scope='vector' type='boolean' "
        "description='Whether to create a group for multiple layer support' "
        "default='NO'/>"
#endif
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
#ifdef HAS_TILEDB_WORKING_UTF8_STRING_FILTER
       "default='UTF8'"
#else
       "default='ASCII'"
#endif
        ">"
        "       <Value>UTF8</Value>"
        "       <Value>ASCII</Value>"
        "   </Option>"
        "   <Option name='STATS' type='boolean' default='false' "
        "description='Dump TileDB stats'/>"
        "</LayerCreationOptionList>");
    // clang-format on

    poDriver->pfnIdentify = TileDBDataset::Identify;
    poDriver->pfnOpen = TileDBDataset::Open;
    poDriver->pfnCreate = TileDBDataset::Create;
    poDriver->pfnCreateCopy = TileDBDataset::CreateCopy;
    poDriver->pfnDelete = TileDBDataset::Delete;
#ifdef HAS_TILEDB_MULTIDIM
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");
    poDriver->pfnCreateMultiDimensional = TileDBDataset::CreateMultiDimensional;

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
#endif

#if !defined(HAS_TILEDB_WORKING_UTF8_STRING_FILTER)
    poDriver->SetMetadataItem("HAS_TILEDB_WORKING_UTF8_STRING_FILTER", "NO");
#endif

#if !defined(HAS_TILEDB_WORKING_OR_FILTER)
    poDriver->SetMetadataItem("HAS_TILEDB_WORKING_OR_FILTER", "NO");
#endif

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
