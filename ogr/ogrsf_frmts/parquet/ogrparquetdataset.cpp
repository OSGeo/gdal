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

OGRParquetDataset::OGRParquetDataset(
    const std::shared_ptr<arrow::MemoryPool> &poMemoryPool)
    : OGRArrowDataset(poMemoryPool)
{
}

/************************************************************************/
/*                        ~OGRParquetDataset()                          */
/************************************************************************/

OGRParquetDataset::~OGRParquetDataset()
{
    // libarrow might continue to do I/O in auxiliary threads on the underlying
    // files when using the arrow::dataset API even after we closed the dataset.
    // This is annoying as it can cause crashes when closing GDAL, in particular
    // the virtual file manager, as this could result in VSI files being
    // accessed after their VSIVirtualFileSystem has been destroyed, resulting
    // in crashes. The workaround is to make sure that VSIArrowFileSystem
    // waits for all file handles it is aware of to have been destroyed.
    close();
    auto poFS = std::dynamic_pointer_cast<VSIArrowFileSystem>(m_poFS);
    if (poFS)
        poFS->AskToClose();
}

/***********************************************************************/
/*                            ExecuteSQL()                             */
/***********************************************************************/

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
                    const int iCol =
                        iOGRField == OGRParquetLayer::OGR_FID_INDEX
                            ? poLayer->GetFIDParquetColumn()
                            : poLayer->GetMapFieldIndexToParquetColumn()
                                  [iOGRField];
                    if (iCol < 0)
                        break;
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
                        OGRFeature *poFeature =
                            new OGRFeature(poMemLayer->GetLayerDefn());
                        CPL_IGNORE_RET_VAL(
                            poMemLayer->CreateFeature(poFeature));
                        delete poFeature;
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

                    OGRFeature *poFeature = poMemLayer->GetFeature(0);
                    poFeature->SetField(oFieldDefn.GetNameRef(), &sField);
                    CPL_IGNORE_RET_VAL(poMemLayer->SetFeature(poFeature));
                    delete poFeature;
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

    return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
}

/***********************************************************************/
/*                           ReleaseResultSet()                        */
/***********************************************************************/

void OGRParquetDataset::ReleaseResultSet(OGRLayer *poResultsSet)
{
    delete poResultsSet;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRParquetDataset::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCZGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return true;

    return false;
}
