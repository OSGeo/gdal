/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#include "ogr_parquet.h"
#include "ogr_mem.h"
#include "ogr_swq.h"

#include "../arrow_common/ograrrowdataset.hpp"
#include "../arrow_common/ograrrowlayer.hpp"

/************************************************************************/
/*                         OGRParquetDataset()                          */
/************************************************************************/

OGRParquetDataset::OGRParquetDataset(const std::shared_ptr<arrow::MemoryPool>& poMemoryPool):
    OGRArrowDataset(poMemoryPool)
{
}

/***********************************************************************/
/*                            GetStats()                               */
/***********************************************************************/

template<class STAT_TYPE> struct GetStats
{
    using T = typename STAT_TYPE::T;

    static T min(const std::shared_ptr<parquet::FileMetaData>& metadata,
                 const int numRowGroups, const int iCol, bool& bFound)
    {
        T v{};
        bFound = false;
        for(int iGroup = 0; iGroup < numRowGroups; iGroup++ )
        {
            const auto columnChunk = metadata->RowGroup(iGroup)->ColumnChunk(iCol);
            const auto colStats = columnChunk->statistics();
            if( columnChunk->is_stats_set() && colStats && colStats->HasMinMax() )
            {
                auto castStats = dynamic_cast<STAT_TYPE*>(colStats.get());
                const auto rowGroupVal = castStats->min();
                if( iGroup == 0 || rowGroupVal < v )
                {
                    bFound = true;
                    v = rowGroupVal;
                }
            }
        }
        return v;
    }

    static T max(const std::shared_ptr<parquet::FileMetaData>& metadata,
                 const int numRowGroups, const int iCol, bool& bFound)
    {
        T v{};
        bFound = false;
        for(int iGroup = 0; iGroup < numRowGroups; iGroup++ )
        {
            const auto columnChunk = metadata->RowGroup(iGroup)->ColumnChunk(iCol);
            const auto colStats = columnChunk->statistics();
            if( columnChunk->is_stats_set() && colStats && colStats->HasMinMax() )
            {
                auto castStats = dynamic_cast<STAT_TYPE*>(colStats.get());
                const auto rowGroupVal = castStats->max();
                if( iGroup == 0 || rowGroupVal > v )
                {
                    bFound = true;
                    v = rowGroupVal;
                }
            }
            else
            {
                bFound = false;
                break;
            }
        }
        return v;
    }
};

template<> struct GetStats<parquet::ByteArrayStatistics>
{
    static std::string min(const std::shared_ptr<parquet::FileMetaData>& metadata,
                 const int numRowGroups, const int iCol, bool& bFound)
    {
        std::string v{};
        bFound = false;
        for(int iGroup = 0; iGroup < numRowGroups; iGroup++ )
        {
            const auto columnChunk = metadata->RowGroup(iGroup)->ColumnChunk(iCol);
            const auto colStats = columnChunk->statistics();
            if( columnChunk->is_stats_set() && colStats && colStats->HasMinMax() )
            {
                auto castStats = dynamic_cast<parquet::ByteArrayStatistics*>(colStats.get());
                const auto rowGroupValRaw = castStats->min();
                const std::string rowGroupVal(reinterpret_cast<const char*>(rowGroupValRaw.ptr), rowGroupValRaw.len);
                if( iGroup == 0 || rowGroupVal < v )
                {
                    bFound = true;
                    v = rowGroupVal;
                }
            }
        }
        return v;
    }

    static std::string max(const std::shared_ptr<parquet::FileMetaData>& metadata,
                 const int numRowGroups, const int iCol, bool& bFound)
    {
        std::string v{};
        bFound = false;
        for(int iGroup = 0; iGroup < numRowGroups; iGroup++ )
        {
            const auto columnChunk = metadata->RowGroup(iGroup)->ColumnChunk(iCol);
            const auto colStats = columnChunk->statistics();
            if( columnChunk->is_stats_set() && colStats && colStats->HasMinMax() )
            {
                auto castStats = dynamic_cast<parquet::ByteArrayStatistics*>(colStats.get());
                const auto rowGroupValRaw = castStats->max();
                const std::string rowGroupVal(reinterpret_cast<const char*>(rowGroupValRaw.ptr), rowGroupValRaw.len);
                if( iGroup == 0 || rowGroupVal > v )
                {
                    bFound = true;
                    v = rowGroupVal;
                }
            }
            else
            {
                bFound = false;
                break;
            }
        }
        return v;
    }
};

/***********************************************************************/
/*                            ExecuteSQL()                             */
/***********************************************************************/

OGRLayer* OGRParquetDataset::ExecuteSQL( const char *pszSQLCommand,
                                         OGRGeometry *poSpatialFilter,
                                         const char *pszDialect )
{
/* -------------------------------------------------------------------- */
/*      Special cases for SQL optimizations                             */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "SELECT ") &&
        (pszDialect == nullptr || EQUAL(pszDialect, "") ||
         EQUAL(pszDialect, "OGRSQL")) )
    {
        swq_select oSelect;
        if( oSelect.preparse(pszSQLCommand) != CE_None )
            return nullptr;

/* -------------------------------------------------------------------- */
/*      MIN/MAX/COUNT optimization                                      */
/* -------------------------------------------------------------------- */
        if( oSelect.join_count == 0 && oSelect.poOtherSelect == nullptr &&
            oSelect.table_count == 1 && oSelect.order_specs == 0 &&
            oSelect.query_mode != SWQM_DISTINCT_LIST &&
            oSelect.where_expr == nullptr &&
            CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_STATISTICS", "YES")) )
        {
            auto poLayer = dynamic_cast<OGRParquetLayer *>(
                    GetLayerByName( oSelect.table_defs[0].table_name));
            if( poLayer )
            {
                OGRMemLayer* poMemLayer = nullptr;
                const auto poLayerDefn = poLayer->GetLayerDefn();

                int i = 0;  // Used after for.
                for( ; i < oSelect.result_columns; i ++ )
                {
                    swq_col_func col_func = oSelect.column_defs[i].col_func;
                    if( !(col_func == SWQCF_MIN || col_func == SWQCF_MAX ||
                          col_func == SWQCF_COUNT) )
                        break;

                    if( oSelect.column_defs[i].field_name == nullptr )
                        break;
                    if( oSelect.column_defs[i].target_type != SWQ_OTHER )
                        break;

                    const int idx = poLayerDefn->GetFieldIndex(
                                            oSelect.column_defs[i].field_name);
                    if( idx < 0 )
                        break;

                    const OGRFieldDefn* poFieldDefn = poLayerDefn->GetFieldDefn(idx);

                    OGRField sField;
                    OGR_RawField_SetNull(&sField);
                    OGRFieldType eType = OFTReal;
                    OGRFieldSubType eSubType = OFSTNone;
                    const int iCol = poLayer->GetMapFieldIndexToParquetColumn()[idx];
                    const auto metadata = poLayer->GetReader()->parquet_reader()->metadata();
                    const auto numRowGroups = metadata->num_row_groups();
                    bool bFound = false;
                    std::string sVal;
                    if( numRowGroups > 0 )
                    {
                        const auto rowGroup0columnChunk = metadata->RowGroup(0)->ColumnChunk(iCol);
                        const auto rowGroup0Stats = rowGroup0columnChunk->statistics();
                        if( rowGroup0columnChunk->is_stats_set() && rowGroup0Stats )
                        {
                            const auto physicalType = rowGroup0Stats->physical_type();
                            if( col_func == SWQCF_MIN )
                            {
                                if( physicalType == parquet::Type::BOOLEAN )
                                {
                                    eType = OFTInteger;
                                    eSubType = OFSTBoolean;
                                    sField.Integer = GetStats<parquet::BoolStatistics>::min(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( physicalType == parquet::Type::INT32 )
                                {
                                    eType = OFTInteger;
                                    if( poFieldDefn->GetSubType() == OFSTInt16 )
                                        eSubType = OFSTInt16;
                                    sField.Integer = GetStats<parquet::Int32Statistics>::min(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( physicalType == parquet::Type::INT64 )
                                {
                                    eType = OFTInteger64;
                                    sField.Integer64 = GetStats<parquet::Int64Statistics>::min(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( physicalType == parquet::Type::FLOAT )
                                {
                                    eType = OFTReal;
                                    eSubType = OFSTFloat32;
                                    sField.Real = GetStats<parquet::FloatStatistics>::min(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( physicalType == parquet::Type::DOUBLE )
                                {
                                    eType = OFTReal;
                                    sField.Real = GetStats<parquet::DoubleStatistics>::min(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( poFieldDefn->GetType() == OFTString &&
                                         physicalType == parquet::Type::BYTE_ARRAY )
                                {
                                    sVal = GetStats<parquet::ByteArrayStatistics>::min(
                                        metadata, numRowGroups, iCol, bFound);
                                    if( bFound )
                                    {
                                        eType = OFTString;
                                        sField.String = &sVal[0];
                                    }
                                }
                            }
                            else if( col_func == SWQCF_MAX )
                            {
                                if( physicalType == parquet::Type::BOOLEAN )
                                {
                                    eType = OFTInteger;
                                    eSubType = OFSTBoolean;
                                    sField.Integer = GetStats<parquet::BoolStatistics>::max(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( physicalType == parquet::Type::INT32 )
                                {
                                    eType = OFTInteger;
                                    if( poFieldDefn->GetSubType() == OFSTInt16 )
                                        eSubType = OFSTInt16;
                                    sField.Integer = GetStats<parquet::Int32Statistics>::max(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( physicalType == parquet::Type::INT64 )
                                {
                                    eType = OFTInteger64;
                                    sField.Integer64 = GetStats<parquet::Int64Statistics>::max(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( physicalType == parquet::Type::FLOAT )
                                {
                                    eType = OFTReal;
                                    eSubType = OFSTFloat32;
                                    sField.Real = GetStats<parquet::FloatStatistics>::max(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( physicalType == parquet::Type::DOUBLE )
                                {
                                    eType = OFTReal;
                                    sField.Real = GetStats<parquet::DoubleStatistics>::max(
                                        metadata, numRowGroups, iCol, bFound);
                                }
                                else if( poFieldDefn->GetType() == OFTString &&
                                         physicalType == parquet::Type::BYTE_ARRAY )
                                {
                                    sVal = GetStats<parquet::ByteArrayStatistics>::max(
                                        metadata, numRowGroups, iCol, bFound);
                                    if( bFound )
                                    {
                                        eType = OFTString;
                                        sField.String = &sVal[0];
                                    }
                                }
                            }
                            else if( col_func == SWQCF_COUNT )
                            {
                                if( oSelect.column_defs[i].distinct_flag )
                                {
                                    eType = OFTInteger64;
                                    sField.Integer64 = 0;
                                    for(int iGroup = 0; iGroup < numRowGroups; iGroup++ )
                                    {
                                        const auto columnChunk = metadata->RowGroup(iGroup)->ColumnChunk(iCol);
                                        const auto colStats = columnChunk->statistics();
                                        if( columnChunk->is_stats_set() && colStats && colStats->HasDistinctCount() )
                                        {
                                            // Statistics generated by arrow-cpp Parquet writer seem
                                            // to be buggy, as distinct_count() is always zero.
                                            // We can detect this: if there are non-null values, then
                                            // distinct_count() should be > 0.
                                            if( colStats->distinct_count() == 0 && colStats->num_values() > 0 )
                                            {
                                                bFound = false;
                                                break;
                                            }
                                            sField.Integer64 += colStats->distinct_count();
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
                                    for(int iGroup = 0; iGroup < numRowGroups; iGroup++ )
                                    {
                                        const auto columnChunk = metadata->RowGroup(iGroup)->ColumnChunk(iCol);
                                        const auto colStats = columnChunk->statistics();
                                        if( columnChunk->is_stats_set() && colStats )
                                        {
                                            sField.Integer64 += colStats->num_values();
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
                                     poFieldDefn->GetNameRef());
                        }
                    }
                    if( !bFound )
                    {
                        break;
                    }

                    const auto& arrowType = poLayer->GetArrowFieldTypes()[idx];
                    if( eType == OFTInteger64 &&
                        poFieldDefn->GetType() == OFTDateTime &&
                        arrowType->id() == arrow::Type::TIMESTAMP )
                    {
                        const auto timestampType = static_cast<arrow::TimestampType*>(arrowType.get());
                        const int64_t timestamp = sField.Integer64;
                        OGRArrowLayer::TimestampToOGR(timestamp, timestampType, &sField);
                        eType = OFTDateTime;
                    }

                    if( poMemLayer == nullptr )
                    {
                        poMemLayer = new OGRMemLayer("SELECT", nullptr, wkbNone);
                        OGRFeature* poFeature = new OGRFeature(poMemLayer->GetLayerDefn());
                        CPL_IGNORE_RET_VAL(poMemLayer->CreateFeature(poFeature));
                        delete poFeature;
                    }

                    const char* pszMinMaxFieldName =
                        CPLSPrintf( "%s_%s", (col_func == SWQCF_MIN) ? "MIN" :
                                             (col_func == SWQCF_MAX) ? "MAX" :
                                                                       "COUNT",
                                            oSelect.column_defs[i].field_name);
                    OGRFieldDefn oFieldDefn(pszMinMaxFieldName, eType);
                    oFieldDefn.SetSubType(eSubType);
                    poMemLayer->CreateField(&oFieldDefn);

                    OGRFeature* poFeature = poMemLayer->GetFeature(0);
                    poFeature->SetField(oFieldDefn.GetNameRef(), &sField);
                    CPL_IGNORE_RET_VAL(poMemLayer->SetFeature(poFeature));
                    delete poFeature;
                }
                if( i != oSelect.result_columns )
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

void OGRParquetDataset::ReleaseResultSet( OGRLayer * poResultsSet )
{
    delete poResultsSet;
}
