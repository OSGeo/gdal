/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_parquet.h"
#include "memdataset.h"
#include "ogr_swq.h"

#include "../arrow_common/ograrrowdataset.hpp"
#include "../arrow_common/ograrrowlayer.hpp"
#include "../arrow_common/vsiarrowfilesystem.hpp"

/************************************************************************/
/*                         OGRParquetDataset()                          */
/************************************************************************/

OGRParquetDataset::OGRParquetDataset()
    : OGRArrowDataset(arrow::MemoryPool::CreateDefault())
{
}

/************************************************************************/
/*                         ~OGRParquetDataset()                         */
/************************************************************************/

OGRParquetDataset::~OGRParquetDataset()
{
    OGRParquetDataset::Close();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

CPLErr OGRParquetDataset::Close(GDALProgressFunc, void *)
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        // libarrow might continue to do I/O in auxiliary threads on the underlying
        // files when using the arrow::dataset API even after we closed the dataset.
        // This is annoying as it can cause crashes when closing GDAL, in particular
        // the virtual file manager, as this could result in VSI files being
        // accessed after their VSIVirtualFileSystem has been destroyed, resulting
        // in crashes. The workaround is to make sure that VSIArrowFileSystem
        // waits for all file handles it is aware of to have been destroyed.
        eErr = OGRArrowDataset::Close();

        auto poFS = std::dynamic_pointer_cast<VSIArrowFileSystem>(m_poFS);
        if (poFS)
            poFS->AskToClose();
    }

    return eErr;
}

/************************************************************************/
/*                         CreateReaderLayer()                          */
/************************************************************************/

std::unique_ptr<OGRParquetLayer>
OGRParquetDataset::CreateReaderLayer(const std::string &osFilename,
                                     VSILFILE *&fpIn,
                                     CSLConstList papszOpenOptionsIn)
{
    try
    {
        std::shared_ptr<arrow::io::RandomAccessFile> infile;
        if (STARTS_WITH(osFilename.c_str(), "/vsi") ||
            CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_VSI", "NO")))
        {
            VSIVirtualHandleUniquePtr fp(fpIn);
            fpIn = nullptr;
            if (fp == nullptr)
            {
                fp.reset(VSIFOpenL(osFilename.c_str(), "rb"));
                if (fp == nullptr)
                    return nullptr;
            }
            infile = std::make_shared<OGRArrowRandomAccessFile>(osFilename,
                                                                std::move(fp));
        }
        else
        {
            PARQUET_ASSIGN_OR_THROW(infile,
                                    arrow::io::ReadableFile::Open(osFilename));
        }

        // Open Parquet file reader
        std::unique_ptr<parquet::arrow::FileReader> arrow_reader;

        const int nNumCPUs = OGRParquetLayerBase::GetNumCPUs();
        const char *pszUseThreads =
            CPLGetConfigOption("OGR_PARQUET_USE_THREADS", nullptr);
        if (!pszUseThreads && nNumCPUs > 1)
        {
            pszUseThreads = "YES";
        }
        const bool bUseThreads = pszUseThreads && CPLTestBool(pszUseThreads);

        const char *pszParquetBatchSize =
            CPLGetConfigOption("OGR_PARQUET_BATCH_SIZE", nullptr);

        auto poMemoryPool = GetMemoryPool();
#if ARROW_VERSION_MAJOR >= 21
        parquet::arrow::FileReaderBuilder fileReaderBuilder;
        {
            auto st = fileReaderBuilder.Open(std::move(infile));
            if (!st.ok())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "parquet::arrow::FileReaderBuilder::Open() failed: %s",
                         st.message().c_str());
                return nullptr;
            }
        }
        fileReaderBuilder.memory_pool(poMemoryPool);
        parquet::ArrowReaderProperties fileReaderProperties;
        fileReaderProperties.set_arrow_extensions_enabled(CPLTestBool(
            CPLGetConfigOption("OGR_PARQUET_ENABLE_ARROW_EXTENSIONS", "YES")));
        if (pszParquetBatchSize)
        {
            fileReaderProperties.set_batch_size(
                CPLAtoGIntBig(pszParquetBatchSize));
        }
        if (bUseThreads)
        {
            fileReaderProperties.set_use_threads(true);
        }
        fileReaderBuilder.properties(fileReaderProperties);
        {
            auto res = fileReaderBuilder.Build();
            if (!res.ok())
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "parquet::arrow::FileReaderBuilder::Build() failed: %s",
                    res.status().message().c_str());
                return nullptr;
            }
            arrow_reader = std::move(*res);
        }
#elif ARROW_VERSION_MAJOR >= 19
        PARQUET_ASSIGN_OR_THROW(
            arrow_reader,
            parquet::arrow::OpenFile(std::move(infile), poMemoryPool));
#else
        auto st = parquet::arrow::OpenFile(std::move(infile), poMemoryPool,
                                           &arrow_reader);
        if (!st.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "parquet::arrow::OpenFile() failed: %s",
                     st.message().c_str());
            return nullptr;
        }
#endif

#if ARROW_VERSION_MAJOR < 21
        if (pszParquetBatchSize)
        {
            arrow_reader->set_batch_size(CPLAtoGIntBig(pszParquetBatchSize));
        }

        if (bUseThreads)
        {
            arrow_reader->set_use_threads(true);
        }
#endif

        return std::make_unique<OGRParquetLayer>(
            this, CPLGetBasenameSafe(osFilename.c_str()).c_str(),
            std::move(arrow_reader), papszOpenOptionsIn);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Parquet exception: %s",
                 e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer *OGRParquetDataset::ExecuteSQL(const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect)
{
    /* -------------------------------------------------------------------- */
    /*      Special cases for SQL optimizations                             */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "SELECT ") &&
        (pszDialect == nullptr || EQUAL(pszDialect, "") ||
         EQUAL(pszDialect, "OGRSQL")))
    {
        swq_select oSelect;
        if (oSelect.preparse(pszSQLCommand) != CE_None)
            return nullptr;

        /* --------------------------------------------------------------------
         */
        /*      MIN/MAX/COUNT optimization */
        /* --------------------------------------------------------------------
         */
        if (oSelect.join_count == 0 && oSelect.poOtherSelect == nullptr &&
            oSelect.table_count == 1 && oSelect.order_specs == 0 &&
            oSelect.query_mode != SWQM_DISTINCT_LIST &&
            oSelect.where_expr == nullptr &&
            CPLTestBool(
                CPLGetConfigOption("OGR_PARQUET_USE_STATISTICS", "YES")))
        {
            auto poLayer = dynamic_cast<OGRParquetLayer *>(
                GetLayerByName(oSelect.table_defs[0].table_name));
            if (poLayer)
            {
                OGRMemLayer *poMemLayer = nullptr;
                const auto poLayerDefn = poLayer->GetLayerDefn();

                int i = 0;  // Used after for.
                for (; i < oSelect.result_columns(); i++)
                {
                    swq_col_func col_func = oSelect.column_defs[i].col_func;
                    if (!(col_func == SWQCF_MIN || col_func == SWQCF_MAX ||
                          col_func == SWQCF_COUNT))
                        break;

                    const char *pszFieldName =
                        oSelect.column_defs[i].field_name;
                    if (pszFieldName == nullptr)
                        break;
                    if (oSelect.column_defs[i].target_type != SWQ_OTHER)
                        break;

                    const int iOGRField =
                        (EQUAL(pszFieldName, poLayer->GetFIDColumn()) &&
                         pszFieldName[0])
                            ? OGRParquetLayer::OGR_FID_INDEX
                            : poLayerDefn->GetFieldIndex(pszFieldName);
                    if (iOGRField < 0 &&
                        iOGRField != OGRParquetLayer::OGR_FID_INDEX)
                        break;

                    OGRField sField;
                    OGR_RawField_SetNull(&sField);
                    OGRFieldType eType = OFTReal;
                    OGRFieldSubType eSubType = OFSTNone;
                    const std::vector<int> anCols =
                        iOGRField == OGRParquetLayer::OGR_FID_INDEX
                            ? std::vector<int>{poLayer->GetFIDParquetColumn()}
                            : poLayer->GetParquetColumnIndicesForArrowField(
                                  pszFieldName);
                    if (anCols.size() != 1 || anCols[0] < 0)
                        break;
                    const int iCol = anCols[0];
                    const auto metadata =
                        poLayer->GetReader()->parquet_reader()->metadata();
                    const auto numRowGroups = metadata->num_row_groups();
                    bool bFound = false;
                    std::string sVal;

                    if (numRowGroups > 0)
                    {
                        const auto rowGroup0columnChunk =
                            metadata->RowGroup(0)->ColumnChunk(iCol);
                        const auto rowGroup0Stats =
                            rowGroup0columnChunk->statistics();
                        if (rowGroup0columnChunk->is_stats_set() &&
                            rowGroup0Stats)
                        {
                            OGRField sFieldDummy;
                            bool bFoundDummy;
                            std::string sValDummy;

                            if (col_func == SWQCF_MIN)
                            {
                                CPL_IGNORE_RET_VAL(
                                    poLayer->GetMinMaxForOGRField(
                                        /* iRowGroup=*/-1,  // -1 for all
                                        iOGRField, true, sField, bFound, false,
                                        sFieldDummy, bFoundDummy, eType,
                                        eSubType, sVal, sValDummy));
                            }
                            else if (col_func == SWQCF_MAX)
                            {
                                CPL_IGNORE_RET_VAL(
                                    poLayer->GetMinMaxForOGRField(
                                        /* iRowGroup=*/-1,  // -1 for all
                                        iOGRField, false, sFieldDummy,
                                        bFoundDummy, true, sField, bFound,
                                        eType, eSubType, sValDummy, sVal));
                            }
                            else if (col_func == SWQCF_COUNT)
                            {
                                if (oSelect.column_defs[i].distinct_flag)
                                {
                                    eType = OFTInteger64;
                                    sField.Integer64 = 0;
                                    for (int iGroup = 0; iGroup < numRowGroups;
                                         iGroup++)
                                    {
                                        const auto columnChunk =
                                            metadata->RowGroup(iGroup)
                                                ->ColumnChunk(iCol);
                                        const auto colStats =
                                            columnChunk->statistics();
                                        if (columnChunk->is_stats_set() &&
                                            colStats &&
                                            colStats->HasDistinctCount())
                                        {
                                            // Statistics generated by arrow-cpp
                                            // Parquet writer seem to be buggy,
                                            // as distinct_count() is always
                                            // zero. We can detect this: if
                                            // there are non-null values, then
                                            // distinct_count() should be > 0.
                                            if (colStats->distinct_count() ==
                                                    0 &&
                                                colStats->num_values() > 0)
                                            {
                                                bFound = false;
                                                break;
                                            }
                                            sField.Integer64 +=
                                                colStats->distinct_count();
                                            bFound = true;
                                        }
                                        else
                                        {
                                            bFound = false;
                                            break;
                                        }
                                    }
                                }
                                else
                                {
                                    eType = OFTInteger64;
                                    sField.Integer64 = 0;
                                    bFound = true;
                                    for (int iGroup = 0; iGroup < numRowGroups;
                                         iGroup++)
                                    {
                                        const auto columnChunk =
                                            metadata->RowGroup(iGroup)
                                                ->ColumnChunk(iCol);
                                        const auto colStats =
                                            columnChunk->statistics();
                                        if (columnChunk->is_stats_set() &&
                                            colStats)
                                        {
                                            sField.Integer64 +=
                                                colStats->num_values();
                                        }
                                        else
                                        {
                                            bFound = false;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            CPLDebug("PARQUET",
                                     "Statistics not available for field %s",
                                     pszFieldName);
                        }
                    }
                    if (!bFound)
                    {
                        break;
                    }

                    if (poMemLayer == nullptr)
                    {
                        poMemLayer =
                            new OGRMemLayer("SELECT", nullptr, wkbNone);
                        auto poFeature = std::make_unique<OGRFeature>(
                            poMemLayer->GetLayerDefn());
                        CPL_IGNORE_RET_VAL(
                            poMemLayer->CreateFeature(std::move(poFeature)));
                    }

                    const char *pszMinMaxFieldName =
                        oSelect.column_defs[i].field_alias
                            ? oSelect.column_defs[i].field_alias
                            : CPLSPrintf("%s_%s",
                                         (col_func == SWQCF_MIN)   ? "MIN"
                                         : (col_func == SWQCF_MAX) ? "MAX"
                                                                   : "COUNT",
                                         oSelect.column_defs[i].field_name);
                    OGRFieldDefn oFieldDefn(pszMinMaxFieldName, eType);
                    oFieldDefn.SetSubType(eSubType);
                    poMemLayer->CreateField(&oFieldDefn);

                    auto poFeature =
                        std::unique_ptr<OGRFeature>(poMemLayer->GetFeature(0));
                    poFeature->SetField(oFieldDefn.GetNameRef(), &sField);
                    CPL_IGNORE_RET_VAL(
                        poMemLayer->SetFeature(std::move(poFeature)));
                }
                if (i != oSelect.result_columns())
                {
                    delete poMemLayer;
                }
                else
                {
                    CPLDebug("PARQUET",
                             "Using optimized MIN/MAX/COUNT implementation");
                    return poMemLayer;
                }
            }
        }
    }

    else if (EQUAL(pszSQLCommand, "GET_SET_FILES_ASKED_TO_BE_OPEN") &&
             pszDialect && EQUAL(pszDialect, "_DEBUG_"))
    {
        if (auto poFS = dynamic_cast<VSIArrowFileSystem *>(m_poFS.get()))
        {
            auto poMemLayer = std::make_unique<OGRMemLayer>(
                "SET_FILES_ASKED_TO_BE_OPEN", nullptr, wkbNone);
            OGRFieldDefn oFieldDefn("path", OFTString);
            CPL_IGNORE_RET_VAL(poMemLayer->CreateField(&oFieldDefn));
            for (const std::string &path : poFS->GetSetFilesAskedToOpen())
            {
                auto poFeature =
                    std::make_unique<OGRFeature>(poMemLayer->GetLayerDefn());
                poFeature->SetField(0, path.c_str());
                CPL_IGNORE_RET_VAL(
                    poMemLayer->CreateFeature(std::move(poFeature)));
            }
            poFS->ResetSetFilesAskedToOpen();
            return poMemLayer.release();
        }
        return nullptr;
    }

    return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRParquetDataset::ReleaseResultSet(OGRLayer *poResultsSet)
{
    delete poResultsSet;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRParquetDataset::TestCapability(const char *pszCap) const

{
    if (EQUAL(pszCap, ODsCZGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return true;

    return false;
}
