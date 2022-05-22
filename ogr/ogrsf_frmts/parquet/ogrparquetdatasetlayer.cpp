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

#include "ogrsf_frmts.h"

#include <map>
#include <set>
#include <utility>

#include "ogr_parquet.h"

#include "../arrow_common/ograrrowlayer.hpp"
#include "../arrow_common/ograrrowdataset.hpp"

/************************************************************************/
/*                        OGRParquetLayer()                             */
/************************************************************************/

OGRParquetDatasetLayer::OGRParquetDatasetLayer(OGRParquetDataset* poDS,
                               const char* pszLayerName,
                               const std::shared_ptr<arrow::dataset::Scanner>& scanner,
                               const std::shared_ptr<arrow::Schema>& schema):
    OGRParquetLayerBase(poDS, pszLayerName),
    m_poScanner(scanner)
{
    EstablishFeatureDefn(schema);
    CPLAssert( static_cast<int>(m_aeGeomEncoding.size()) == m_poFeatureDefn->GetGeomFieldCount() );
}

/************************************************************************/
/*                        EstablishFeatureDefn()                        */
/************************************************************************/

void OGRParquetDatasetLayer::EstablishFeatureDefn(const std::shared_ptr<arrow::Schema>& schema)
{
    const auto& kv_metadata = schema->metadata();

    LoadGeoMetadata(kv_metadata);
    const auto oMapFieldNameToGDALSchemaFieldDefn = LoadGDALMetadata(kv_metadata.get());

    const auto fields = schema->fields();
    for( int i = 0; i < schema->num_fields(); ++i )
    {
        const auto& field = fields[i];

        if( !m_osFIDColumn.empty() &&
            field->name() == m_osFIDColumn )
        {
            m_iFIDArrowColumn = i;
            continue;
        }

        const bool bGeometryField = DealWithGeometryColumn(
                                i, field, []() { return wkbUnknown; });
        if( !bGeometryField )
        {
            CreateFieldFromSchema(field, {i},
                                  oMapFieldNameToGDALSchemaFieldDefn);
        }
    }

    CPLAssert( static_cast<int>(m_anMapFieldIndexToArrowColumn.size()) == m_poFeatureDefn->GetFieldCount() );
    CPLAssert( static_cast<int>(m_anMapGeomFieldIndexToArrowColumn.size()) == m_poFeatureDefn->GetGeomFieldCount() );
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

void OGRParquetDatasetLayer::ResetReading()
{
    m_poRecordBatchReader.reset();
    OGRParquetLayerBase::ResetReading();
}

/************************************************************************/
/*                           ReadNextBatch()                            */
/************************************************************************/

bool OGRParquetDatasetLayer::ReadNextBatch()
{
    m_nIdxInBatch = 0;

    if( m_poRecordBatchReader == nullptr )
    {
        auto result = m_poScanner->ToRecordBatchReader();
        if( !result.ok() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ToRecordBatchReader() failed: %s",
                     result.status().message().c_str());
            return false;
        }
        m_poRecordBatchReader = *result;
        if( m_poRecordBatchReader == nullptr )
            return false;
    }

    ++m_iRecordBatch;

    std::shared_ptr<arrow::RecordBatch> poNextBatch;
    auto status = m_poRecordBatchReader->ReadNext(&poNextBatch);
    if( !status.ok() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ReadNext() failed: %s",
                 status.message().c_str());
        poNextBatch.reset();
    }
    if( poNextBatch == nullptr )
    {
        m_poBatch.reset();
        return false;
    }
    SetBatch(poNextBatch);

    return true;
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRParquetDatasetLayer::GetFeatureCount(int bForce)
{
    if( m_poAttrQuery == nullptr && m_poFilterGeom == nullptr )
    {
        auto status = m_poScanner->CountRows();
        if( status.ok() )
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
/*                            GetExtent()                               */
/************************************************************************/

OGRErr OGRParquetDatasetLayer::GetExtent(int iGeomField, OGREnvelope *psExtent,
                                         int bForce)
{
    if( iGeomField < 0 || iGeomField >= m_poFeatureDefn->GetGeomFieldCount() )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    // bbox in general m_oMapGeometryColumns can not be trusted (at least at
    // time of writing), so we have to iterate over each fragment.
    const char* pszGeomFieldName =
        m_poFeatureDefn->GetGeomFieldDefn(iGeomField)->GetNameRef();
    auto oIter = m_oMapGeometryColumns.find(pszGeomFieldName);
    if( oIter != m_oMapGeometryColumns.end() )
    {
        auto statusFragments = m_poScanner->dataset()->GetFragments();
        if( statusFragments.ok() )
        {
            *psExtent = OGREnvelope();
            int nFragmentCount = 0;
            int nBBoxFragmentCount = 0;
            for( auto oFragmentStatus: *statusFragments )
            {
                if( oFragmentStatus.ok() )
                {
                    auto statusSchema = (*oFragmentStatus)->ReadPhysicalSchema();
                    if( statusSchema.ok() )
                    {
                        nFragmentCount ++;
                        const auto& kv_metadata = (*statusSchema)->metadata();
                        if( kv_metadata && kv_metadata->Contains("geo") )
                        {
                            auto geo = kv_metadata->Get("geo");
                            CPLJSONDocument oDoc;
                            if( geo.ok() && oDoc.LoadMemory(*geo) )
                            {
                                auto oRoot = oDoc.GetRoot();
                                auto oColumns = oRoot.GetObj("columns");
                                auto oCol = oColumns.GetObj(pszGeomFieldName);
                                OGREnvelope sFragmentExtent;
                                if( oCol.IsValid() &&
                                    GetExtentFromMetadata(oCol, &sFragmentExtent) == OGRERR_NONE )
                                {
                                    nBBoxFragmentCount++;
                                    psExtent->MinX = std::min(psExtent->MinX, sFragmentExtent.MinX);
                                    psExtent->MinY = std::min(psExtent->MinY, sFragmentExtent.MinY);
                                    psExtent->MaxX = std::max(psExtent->MaxX, sFragmentExtent.MaxX);
                                    psExtent->MaxY = std::max(psExtent->MaxY, sFragmentExtent.MaxY);
                                }
                            }
                        }
                        if( nFragmentCount != nBBoxFragmentCount )
                            break;
                    }
                }
            }
            if( nFragmentCount == nBBoxFragmentCount )
                return OGRERR_NONE;
        }
    }

    return OGRParquetLayerBase::GetExtent(iGeomField, psExtent, bForce);
}
