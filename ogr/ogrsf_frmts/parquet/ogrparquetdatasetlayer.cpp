/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022-2024, Planet Labs
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

#include "ogrsf_frmts.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <utility>

#include "ogr_parquet.h"

#include "../arrow_common/ograrrowlayer.hpp"
#include "../arrow_common/ograrrowdataset.hpp"

/************************************************************************/
/*                        OGRParquetLayer()                             */
/************************************************************************/

OGRParquetDatasetLayer::OGRParquetDatasetLayer(
    OGRParquetDataset *poDS, const char *pszLayerName, bool bIsVSI,
    const std::shared_ptr<arrow::dataset::Dataset> &dataset,
    CSLConstList papszOpenOptions)
    : OGRParquetLayerBase(poDS, pszLayerName, papszOpenOptions),
      m_bIsVSI(bIsVSI), m_poDataset(dataset)
{
    m_poSchema = m_poDataset->schema();
    EstablishFeatureDefn();
    CPLAssert(static_cast<int>(m_aeGeomEncoding.size()) ==
              m_poFeatureDefn->GetGeomFieldCount());
}

/************************************************************************/
/*                  ProcessGeometryColumnCovering()                     */
/************************************************************************/

/** Process GeoParquet JSON geometry field object to extract information about
 * its bounding box column, and appropriately fill m_oMapGeomFieldIndexToGeomColBBOX
 * member with information on that bounding box column.
 */
void OGRParquetDatasetLayer::ProcessGeometryColumnCovering(
    const std::shared_ptr<arrow::Field> &field,
    const CPLJSONObject &oJSONGeometryColumn)
{
    std::string osBBOXColumn;
    std::string osXMin, osYMin, osXMax, osYMax;
    if (ParseGeometryColumnCovering(oJSONGeometryColumn, osBBOXColumn, osXMin,
                                    osYMin, osXMax, osYMax))
    {
        OGRArrowLayer::GeomColBBOX sDesc;
        sDesc.iArrowCol = m_poSchema->GetFieldIndex(osBBOXColumn);
        const auto fieldBBOX = m_poSchema->GetFieldByName(osBBOXColumn);
        if (sDesc.iArrowCol >= 0 && fieldBBOX &&
            fieldBBOX->type()->id() == arrow::Type::STRUCT)
        {
            const auto fieldBBOXStruct =
                std::static_pointer_cast<arrow::StructType>(fieldBBOX->type());
            const auto fieldXMin = fieldBBOXStruct->GetFieldByName(osXMin);
            const auto fieldYMin = fieldBBOXStruct->GetFieldByName(osYMin);
            const auto fieldXMax = fieldBBOXStruct->GetFieldByName(osXMax);
            const auto fieldYMax = fieldBBOXStruct->GetFieldByName(osYMax);
            const int nXMinIdx = fieldBBOXStruct->GetFieldIndex(osXMin);
            const int nYMinIdx = fieldBBOXStruct->GetFieldIndex(osYMin);
            const int nXMaxIdx = fieldBBOXStruct->GetFieldIndex(osXMax);
            const int nYMaxIdx = fieldBBOXStruct->GetFieldIndex(osYMax);
            if (nXMinIdx >= 0 && nYMinIdx >= 0 && nXMaxIdx >= 0 &&
                nYMaxIdx >= 0 && fieldXMin && fieldYMin && fieldXMax &&
                fieldYMax &&
                (fieldXMin->type()->id() == arrow::Type::FLOAT ||
                 fieldXMin->type()->id() == arrow::Type::DOUBLE) &&
                fieldXMin->type()->id() == fieldYMin->type()->id() &&
                fieldXMin->type()->id() == fieldXMax->type()->id() &&
                fieldXMin->type()->id() == fieldYMax->type()->id())
            {
                CPLDebug("PARQUET",
                         "Bounding box column '%s' detected for "
                         "geometry column '%s'",
                         osBBOXColumn.c_str(), field->name().c_str());
                sDesc.iArrowSubfieldXMin = nXMinIdx;
                sDesc.iArrowSubfieldYMin = nYMinIdx;
                sDesc.iArrowSubfieldXMax = nXMaxIdx;
                sDesc.iArrowSubfieldYMax = nYMaxIdx;
                sDesc.bIsFloat =
                    (fieldXMin->type()->id() == arrow::Type::FLOAT);

                m_oMapGeomFieldIndexToGeomColBBOX
                    [m_poFeatureDefn->GetGeomFieldCount() - 1] =
                        std::move(sDesc);
            }
        }
    }
}

/************************************************************************/
/*                        EstablishFeatureDefn()                        */
/************************************************************************/

void OGRParquetDatasetLayer::EstablishFeatureDefn()
{
    const auto &kv_metadata = m_poSchema->metadata();

    LoadGeoMetadata(kv_metadata);
    const auto oMapFieldNameToGDALSchemaFieldDefn =
        LoadGDALSchema(kv_metadata.get());

    LoadGDALMetadata(kv_metadata.get());

    const bool bUseBBOX =
        CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_BBOX", "YES"));

    // Keep track of declared bounding box columns in GeoParquet JSON metadata,
    // in order not to expose them as regular fields.
    std::set<std::string> oSetBBOXColumns;
    if (bUseBBOX)
    {
        for (const auto &iter : m_oMapGeometryColumns)
        {
            std::string osBBOXColumn;
            std::string osXMin, osYMin, osXMax, osYMax;
            if (ParseGeometryColumnCovering(iter.second, osBBOXColumn, osXMin,
                                            osYMin, osXMax, osYMax))
            {
                oSetBBOXColumns.insert(osBBOXColumn);
            }
        }
    }

    const auto &fields = m_poSchema->fields();
    for (int i = 0; i < m_poSchema->num_fields(); ++i)
    {
        const auto &field = fields[i];

        if (!m_osFIDColumn.empty() && field->name() == m_osFIDColumn)
        {
            m_iFIDArrowColumn = i;
            continue;
        }

        if (oSetBBOXColumns.find(field->name()) != oSetBBOXColumns.end())
        {
            m_oSetBBoxArrowColumns.insert(i);
            continue;
        }

        const bool bGeometryField =
            DealWithGeometryColumn(i, field, []() { return wkbUnknown; });
        if (bGeometryField)
        {
            const auto oIter = m_oMapGeometryColumns.find(field->name());
            if (bUseBBOX && oIter != m_oMapGeometryColumns.end())
            {
                ProcessGeometryColumnCovering(field, oIter->second);
            }
        }
        else
        {
            CreateFieldFromSchema(field, {i},
                                  oMapFieldNameToGDALSchemaFieldDefn);
        }
    }

    CPLAssert(static_cast<int>(m_anMapFieldIndexToArrowColumn.size()) ==
              m_poFeatureDefn->GetFieldCount());
    CPLAssert(static_cast<int>(m_anMapGeomFieldIndexToArrowColumn.size()) ==
              m_poFeatureDefn->GetGeomFieldCount());
}

/************************************************************************/
/*                              BuildScanner()                          */
/************************************************************************/

void OGRParquetDatasetLayer::BuildScanner()
{
    m_bRebuildScanner = false;

    try
    {
        std::shared_ptr<arrow::dataset::ScannerBuilder> scannerBuilder;
        PARQUET_ASSIGN_OR_THROW(scannerBuilder, m_poDataset->NewScan());
        assert(scannerBuilder);

        // We cannot use the shared memory pool. Otherwise we get random
        // crashes in multi-threaded arrow code (apparently some cleanup code),
        // that may used the memory pool after it has been destroyed.
        // At least this was true with some older libarrow version
        // PARQUET_THROW_NOT_OK(scannerBuilder->Pool(m_poMemoryPool));

        if (m_bIsVSI)
        {
            const int nFragmentReadAhead = atoi(
                CPLGetConfigOption("OGR_PARQUET_FRAGMENT_READ_AHEAD", "2"));
            PARQUET_THROW_NOT_OK(
                scannerBuilder->FragmentReadahead(nFragmentReadAhead));
        }

        const char *pszBatchSize =
            CPLGetConfigOption("OGR_PARQUET_BATCH_SIZE", nullptr);
        if (pszBatchSize)
        {
            PARQUET_THROW_NOT_OK(
                scannerBuilder->BatchSize(CPLAtoGIntBig(pszBatchSize)));
        }

        const int nNumCPUs = GetNumCPUs();
        const char *pszUseThreads =
            CPLGetConfigOption("OGR_PARQUET_USE_THREADS", nullptr);
        if (!pszUseThreads && nNumCPUs > 1)
        {
            pszUseThreads = "YES";
        }
        if (CPLTestBool(pszUseThreads))
        {
            PARQUET_THROW_NOT_OK(scannerBuilder->UseThreads(true));
        }

#if PARQUET_VERSION_MAJOR >= 10
        const char *pszBatchReadAhead =
            CPLGetConfigOption("OGR_PARQUET_BATCH_READ_AHEAD", nullptr);
        if (pszBatchReadAhead)
        {
            PARQUET_THROW_NOT_OK(
                scannerBuilder->BatchReadahead(atoi(pszBatchReadAhead)));
        }
#endif

        namespace cp = ::arrow::compute;
        cp::Expression expression;
        if (m_poFilterGeom &&
            CPLTestBool(CPLGetConfigOption(
                "OGR_PARQUET_OPTIMIZED_SPATIAL_FILTER", "YES")))
        {
            const auto oIter =
                m_oMapGeomFieldIndexToGeomColBBOX.find(m_iGeomFieldFilter);
            if (oIter != m_oMapGeomFieldIndexToGeomColBBOX.end())
            {
                // This actually requires Arrow >= 15 (https://github.com/apache/arrow/issues/39064)
                // to be more efficient.
                const auto &oBBOXDef = oIter->second;
                expression = cp::and_(
                    {cp::less_equal(
                         cp::field_ref(arrow::FieldRef(
                             oBBOXDef.iArrowCol, oBBOXDef.iArrowSubfieldXMin)),
                         cp::literal(m_sFilterEnvelope.MaxX)),
                     cp::less_equal(
                         cp::field_ref(arrow::FieldRef(
                             oBBOXDef.iArrowCol, oBBOXDef.iArrowSubfieldYMin)),
                         cp::literal(m_sFilterEnvelope.MaxY)),
                     cp::greater_equal(
                         cp::field_ref(arrow::FieldRef(
                             oBBOXDef.iArrowCol, oBBOXDef.iArrowSubfieldXMax)),
                         cp::literal(m_sFilterEnvelope.MinX)),
                     cp::greater_equal(
                         cp::field_ref(arrow::FieldRef(
                             oBBOXDef.iArrowCol, oBBOXDef.iArrowSubfieldYMax)),
                         cp::literal(m_sFilterEnvelope.MinY))});
            }
        }
        if (expression.is_valid())
        {
            PARQUET_THROW_NOT_OK(scannerBuilder->Filter(expression));
        }

        if (m_bIgnoredFields)
        {
#ifdef DEBUG
            std::string osFields;
            for (const std::string &osField : m_aosProjectedFields)
            {
                if (!osFields.empty())
                    osFields += ',';
                osFields += osField;
            }
            CPLDebug("PARQUET", "Projected fields: %s", osFields.c_str());
#endif
            PARQUET_THROW_NOT_OK(scannerBuilder->Project(m_aosProjectedFields));
        }

        PARQUET_ASSIGN_OR_THROW(m_poScanner, scannerBuilder->Finish());
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Arrow/Parquet exception: %s",
                 e.what());
    }
}

/************************************************************************/
/*                           ReadNextBatch()                            */
/************************************************************************/

bool OGRParquetDatasetLayer::ReadNextBatch()
{
    if (m_bRebuildScanner)
        BuildScanner();

    m_nIdxInBatch = 0;

    if (m_poRecordBatchReader == nullptr)
    {
        if (!m_poScanner)
            return false;
        auto result = m_poScanner->ToRecordBatchReader();
        if (!result.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ToRecordBatchReader() failed: %s",
                     result.status().message().c_str());
            return false;
        }
        m_poRecordBatchReader = *result;
        if (m_poRecordBatchReader == nullptr)
            return false;
    }

    std::shared_ptr<arrow::RecordBatch> poNextBatch;
    do
    {
        ++m_iRecordBatch;

        poNextBatch.reset();
        auto status = m_poRecordBatchReader->ReadNext(&poNextBatch);
        if (!status.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "ReadNext() failed: %s",
                     status.message().c_str());
            poNextBatch.reset();
        }
        if (poNextBatch == nullptr)
        {
            m_poBatch.reset();
            return false;
        }
    } while (poNextBatch->num_rows() == 0);

    // CPLDebug("PARQUET", "Current batch has %d rows", int(poNextBatch->num_rows()));

    SetBatch(poNextBatch);

    return true;
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRParquetDatasetLayer::GetFeatureCount(int bForce)
{
    if (m_poAttrQuery == nullptr && m_poFilterGeom == nullptr)
    {
        if (m_bRebuildScanner)
            BuildScanner();
        if (!m_poScanner)
            return -1;
        auto status = m_poScanner->CountRows();
        if (status.ok())
            return *status;
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                            GetExtent()                               */
/************************************************************************/

OGRErr OGRParquetDatasetLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    return GetExtent(0, psExtent, bForce);
}

/************************************************************************/
/*                         FastGetExtent()                              */
/************************************************************************/

bool OGRParquetDatasetLayer::FastGetExtent(int iGeomField,
                                           OGREnvelope *psExtent) const
{
    const auto oIter = m_oMapExtents.find(iGeomField);
    if (oIter != m_oMapExtents.end())
    {
        *psExtent = oIter->second;
        return true;
    }

    return false;
}

/************************************************************************/
/*                            GetExtent()                               */
/************************************************************************/

OGRErr OGRParquetDatasetLayer::GetExtent(int iGeomField, OGREnvelope *psExtent,
                                         int bForce)
{
    if (iGeomField < 0 || iGeomField >= m_poFeatureDefn->GetGeomFieldCount())
    {
        if (iGeomField != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    if (FastGetExtent(iGeomField, psExtent))
    {
        return OGRERR_NONE;
    }

    // bbox in general m_oMapGeometryColumns can not be trusted (at least at
    // time of writing), so we have to iterate over each fragment.
    const char *pszGeomFieldName =
        m_poFeatureDefn->GetGeomFieldDefn(iGeomField)->GetNameRef();
    auto oIter = m_oMapGeometryColumns.find(pszGeomFieldName);
    if (oIter != m_oMapGeometryColumns.end())
    {
        auto statusFragments = m_poDataset->GetFragments();
        if (statusFragments.ok())
        {
            *psExtent = OGREnvelope();
            int nFragmentCount = 0;
            int nBBoxFragmentCount = 0;
            for (const auto &oFragmentStatus : *statusFragments)
            {
                if (oFragmentStatus.ok())
                {
                    auto statusSchema =
                        (*oFragmentStatus)->ReadPhysicalSchema();
                    if (statusSchema.ok())
                    {
                        nFragmentCount++;
                        const auto &kv_metadata = (*statusSchema)->metadata();
                        if (kv_metadata && kv_metadata->Contains("geo"))
                        {
                            auto geo = kv_metadata->Get("geo");
                            CPLJSONDocument oDoc;
                            if (geo.ok() && oDoc.LoadMemory(*geo))
                            {
                                auto oRoot = oDoc.GetRoot();
                                auto oColumns = oRoot.GetObj("columns");
                                auto oCol = oColumns.GetObj(pszGeomFieldName);
                                OGREnvelope3D sFragmentExtent;
                                if (oCol.IsValid() &&
                                    GetExtentFromMetadata(
                                        oCol, &sFragmentExtent) == OGRERR_NONE)
                                {
                                    nBBoxFragmentCount++;
                                    psExtent->Merge(sFragmentExtent);
                                }
                            }
                        }
                        if (nFragmentCount != nBBoxFragmentCount)
                            break;
                    }
                }
            }
            if (nFragmentCount == nBBoxFragmentCount)
            {
                m_oMapExtents[iGeomField] = *psExtent;
                return OGRERR_NONE;
            }
        }
    }

    return OGRParquetLayerBase::GetExtent(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                        SetSpatialFilter()                            */
/************************************************************************/

void OGRParquetDatasetLayer::SetSpatialFilter(int iGeomField,
                                              OGRGeometry *poGeomIn)

{
    OGRParquetLayerBase::SetSpatialFilter(iGeomField, poGeomIn);
    m_bRebuildScanner = true;

    // Full invalidation
    InvalidateCachedBatches();
}

/************************************************************************/
/*                        SetIgnoredFields()                            */
/************************************************************************/

OGRErr OGRParquetDatasetLayer::SetIgnoredFields(CSLConstList papszFields)
{
    m_bRebuildScanner = true;
    m_aosProjectedFields.clear();
    m_bIgnoredFields = false;
    m_anMapFieldIndexToArrayIndex.clear();
    m_anMapGeomFieldIndexToArrayIndex.clear();
    m_nRequestedFIDColumn = -1;
    OGRErr eErr = OGRParquetLayerBase::SetIgnoredFields(papszFields);
    if (eErr == OGRERR_NONE)
    {
        m_bIgnoredFields = papszFields != nullptr && papszFields[0] != nullptr;
        if (m_bIgnoredFields)
        {
            if (m_iFIDArrowColumn >= 0)
            {
                m_nRequestedFIDColumn =
                    static_cast<int>(m_aosProjectedFields.size());
                m_aosProjectedFields.emplace_back(GetFIDColumn());
            }

            const auto &fields = m_poSchema->fields();
            for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
            {
                const auto &field =
                    fields[m_anMapFieldIndexToArrowColumn[i][0]];
                const auto eArrowType = field->type()->id();
                if (eArrowType == arrow::Type::STRUCT)
                {
                    // For a struct, for the sake of simplicity in
                    // GetNextRawFeature(), as soon as one of the member if
                    // requested, request the struct field, so that the Arrow
                    // type doesn't change
                    bool bFoundNotIgnored = false;
                    for (int j = i; j < m_poFeatureDefn->GetFieldCount() &&
                                    m_anMapFieldIndexToArrowColumn[i][0] ==
                                        m_anMapFieldIndexToArrowColumn[j][0];
                         ++j)
                    {
                        if (!m_poFeatureDefn->GetFieldDefn(j)->IsIgnored())
                        {
                            bFoundNotIgnored = true;
                            break;
                        }
                    }
                    if (bFoundNotIgnored)
                    {
                        int j;
                        for (j = i; j < m_poFeatureDefn->GetFieldCount() &&
                                    m_anMapFieldIndexToArrowColumn[i][0] ==
                                        m_anMapFieldIndexToArrowColumn[j][0];
                             ++j)
                        {
                            if (!m_poFeatureDefn->GetFieldDefn(j)->IsIgnored())
                            {
                                m_anMapFieldIndexToArrayIndex.push_back(
                                    static_cast<int>(
                                        m_aosProjectedFields.size()));
                            }
                            else
                            {
                                m_anMapFieldIndexToArrayIndex.push_back(-1);
                            }
                        }
                        i = j - 1;

                        m_aosProjectedFields.emplace_back(field->name());
                    }
                    else
                    {
                        int j;
                        for (j = i; j < m_poFeatureDefn->GetFieldCount() &&
                                    m_anMapFieldIndexToArrowColumn[i][0] ==
                                        m_anMapFieldIndexToArrowColumn[j][0];
                             ++j)
                        {
                            m_anMapFieldIndexToArrayIndex.push_back(-1);
                        }
                        i = j - 1;
                    }
                }
                else if (!m_poFeatureDefn->GetFieldDefn(i)->IsIgnored())
                {
                    m_anMapFieldIndexToArrayIndex.push_back(
                        static_cast<int>(m_aosProjectedFields.size()));
                    m_aosProjectedFields.emplace_back(field->name());
                }
                else
                {
                    m_anMapFieldIndexToArrayIndex.push_back(-1);
                }
            }

            for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
            {
                const auto &field =
                    fields[m_anMapGeomFieldIndexToArrowColumn[i]];
                if (!m_poFeatureDefn->GetGeomFieldDefn(i)->IsIgnored())
                {
                    m_anMapGeomFieldIndexToArrayIndex.push_back(
                        static_cast<int>(m_aosProjectedFields.size()));
                    m_aosProjectedFields.emplace_back(field->name());

                    auto oIter = m_oMapGeomFieldIndexToGeomColBBOX.find(i);
                    if (oIter != m_oMapGeomFieldIndexToGeomColBBOX.end() &&
                        !OGRArrowIsGeoArrowStruct(m_aeGeomEncoding[i]))
                    {
                        oIter->second.iArrayIdx =
                            static_cast<int>(m_aosProjectedFields.size());
                        m_aosProjectedFields.emplace_back(
                            fields[oIter->second.iArrowCol]->name());
                    }
                }
                else
                {
                    m_anMapGeomFieldIndexToArrayIndex.push_back(-1);
                }
            }
        }
    }

    m_nExpectedBatchColumns =
        m_bIgnoredFields ? static_cast<int>(m_aosProjectedFields.size()) : -1;

    // Full invalidation
    InvalidateCachedBatches();

    return eErr;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRParquetDatasetLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCIgnoreFields))
        return true;

    return OGRParquetLayerBase::TestCapability(pszCap);
}
