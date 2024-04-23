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

        const bool bIsS3OrGS =
            STARTS_WITH_CI(poOpenInfo->pszFilename, "/VSIS3/") ||
            STARTS_WITH_CI(poOpenInfo->pszFilename, "/VSIGS/");
        // If this is a /vsi virtual file systems, bail out, except if it is S3 or GS.
        if (!bIsS3OrGS && STARTS_WITH(poOpenInfo->pszFilename, "/vsi"))
        {
            return false;
        }

        if (poOpenInfo->bIsDirectory ||
            (bIsS3OrGS &&
             !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "tif")))
        {
            tiledb::Context ctx;
            CPLString osArrayPath =
                TileDBDataset::VSI_to_tiledb_uri(poOpenInfo->pszFilename);
            const auto eType = tiledb::Object::object(ctx, osArrayPath).type();
            if ((poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0)
            {
                if (eType == tiledb::Object::Type::Array ||
                    eType == tiledb::Object::Type::Group)
                    return true;
            }

            if ((poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) != 0)
            {
                if (eType == tiledb::Object::Type::Array ||
                    eType == tiledb::Object::Type::Group)
                    return true;
            }
            if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0)
            {
                if (eType == tiledb::Object::Type::Group)
                    return GDAL_IDENTIFY_UNKNOWN;
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
        const auto eIdentify = TileDBDataset::Identify(poOpenInfo);
        if (eIdentify == GDAL_IDENTIFY_FALSE)
            return nullptr;

        if (STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB:") &&
            !STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB://"))
        {
            // subdataset URI so this is a raster
            return TileDBRasterDataset::Open(poOpenInfo);
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
            if ((poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
                eType == tiledb::Object::Type::Group)
            {
                return OGRTileDBDataset::Open(poOpenInfo, eType);
            }
            else if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
                     eType == tiledb::Object::Type::Group)
            {
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
