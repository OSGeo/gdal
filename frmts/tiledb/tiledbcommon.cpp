/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB Support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2023, TileDB, Inc
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "tiledbheaders.h"
#include "tiledbdrivercore.h"

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
                osUri = CPLFormFilenameSafe(pszCurDir, pszUri, nullptr);
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
    int nRet = TileDBDriverIdentifySimplified(poOpenInfo);
    if (nRet == GDAL_IDENTIFY_UNKNOWN)
    {
        try
        {
            tiledb::Context ctx;
            CPLString osArrayPath =
                TileDBDataset::VSI_to_tiledb_uri(poOpenInfo->pszFilename);
            const auto eType = tiledb::Object::object(ctx, osArrayPath).type();
            nRet = (eType == tiledb::Object::Type::Array ||
                    eType == tiledb::Object::Type::Group);
        }
        catch (...)
        {
            nRet = FALSE;
        }
    }
    return nRet;
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
        const auto eIdentify = TileDBDataset::Identify(poOpenInfo);
        if (eIdentify == GDAL_IDENTIFY_FALSE)
            return nullptr;

        if (STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB:") &&
            !STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB://"))
        {
            // subdataset URI so this is a raster
            return TileDBRasterDataset::Open(poOpenInfo,
                                             tiledb::Object::Type::Invalid);
        }
        else
        {
            if ((poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) != 0)
            {
                return TileDBDataset::OpenMultiDimensional(poOpenInfo);
            }

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
            std::string osDatasetType;
            if (eType == tiledb::Object::Type::Group)
            {
                tiledb::Group group(oCtx, osPath, TILEDB_READ);
                tiledb_datatype_t v_type = TILEDB_UINT8;
                const void *v_r = nullptr;
                uint32_t v_num = 0;
                group.get_metadata(DATASET_TYPE_ATTRIBUTE_NAME, &v_type, &v_num,
                                   &v_r);
                if (v_r && (v_type == TILEDB_UINT8 || v_type == TILEDB_CHAR ||
                            v_type == TILEDB_STRING_ASCII ||
                            v_type == TILEDB_STRING_UTF8))
                {
                    osDatasetType =
                        std::string(static_cast<const char *>(v_r), v_num);
                }
            }

            if ((poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
                eType == tiledb::Object::Type::Group &&
                (osDatasetType.empty() ||
                 osDatasetType == GEOMETRY_DATASET_TYPE))
            {
                return OGRTileDBDataset::Open(poOpenInfo, eType);
            }
            else if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
                     (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 &&
                     eType == tiledb::Object::Type::Group &&
                     osDatasetType == GEOMETRY_DATASET_TYPE)
            {
                return nullptr;
            }
            else if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
                     eType == tiledb::Object::Type::Group &&
                     osDatasetType == RASTER_DATASET_TYPE)
            {
                return TileDBRasterDataset::Open(poOpenInfo, eType);
            }
            else if ((poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
                     (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
                     eType == tiledb::Object::Type::Group &&
                     osDatasetType == RASTER_DATASET_TYPE)
            {
                return nullptr;
            }
            else if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
                     eType == tiledb::Object::Type::Group &&
                     osDatasetType.empty())
            {
                // Compatibility with generic arrays
                // If this is a group which has only a single 2D array and
                // no 3D+ arrays, then return this 2D array.
                auto poDSUnique = std::unique_ptr<GDALDataset>(
                    TileDBDataset::OpenMultiDimensional(poOpenInfo));
                if (poDSUnique)
                {
                    auto poRootGroup = poDSUnique->GetRootGroup();
                    if (poRootGroup && poRootGroup->GetGroupNames().empty())
                    {
                        std::shared_ptr<GDALMDArray> poCandidateArray;
                        for (const auto &osName :
                             poRootGroup->GetMDArrayNames())
                        {
                            auto poArray = poRootGroup->OpenMDArray(osName);
                            if (poArray && poArray->GetDimensionCount() >= 3)
                            {
                                poCandidateArray.reset();
                                break;
                            }
                            else if (poArray &&
                                     poArray->GetDimensionCount() == 2 &&
                                     poArray->GetDimensions()[0]->GetType() ==
                                         GDAL_DIM_TYPE_HORIZONTAL_Y &&
                                     poArray->GetDimensions()[1]->GetType() ==
                                         GDAL_DIM_TYPE_HORIZONTAL_X)
                            {
                                if (!poCandidateArray)
                                {
                                    poCandidateArray = std::move(poArray);
                                }
                                else
                                {
                                    poCandidateArray.reset();
                                    break;
                                }
                            }
                        }
                        if (poCandidateArray)
                        {
                            return poCandidateArray->AsClassicDataset(1, 0);
                        }
                    }
                }
                return nullptr;
            }

            tiledb::ArraySchema schema(oCtx, osPath);

            if (schema.array_type() == TILEDB_SPARSE)
                return OGRTileDBDataset::Open(poOpenInfo, eType);
            else
                return TileDBRasterDataset::Open(poOpenInfo, eType);
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

void GDALRegister_TileDB()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    TileDBDriverSetCommonMetadata(poDriver);

    poDriver->pfnIdentify = TileDBDataset::Identify;
    poDriver->pfnOpen = TileDBDataset::Open;
    poDriver->pfnCreate = TileDBDataset::Create;
    poDriver->pfnCreateCopy = TileDBDataset::CreateCopy;
    poDriver->pfnDelete = TileDBDataset::Delete;
    poDriver->pfnCreateMultiDimensional = TileDBDataset::CreateMultiDimensional;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
