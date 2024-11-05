/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Arrow Database Connectivity driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2024, Dewey Dunnington <dewey@voltrondata.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_adbc.h"
#include "ogr_p.h"

#define ADBC_CALL(func, ...) m_poDS->m_driver.func(__VA_ARGS__)

/************************************************************************/
/*                            OGRADBCLayer()                            */
/************************************************************************/

OGRADBCLayer::OGRADBCLayer(OGRADBCDataset *poDS, const char *pszName,
                           std::unique_ptr<AdbcStatement> poStatement,
                           std::unique_ptr<OGRArrowArrayStream> poStream,
                           ArrowSchema *schema)
    : m_poDS(poDS), m_statement(std::move(poStatement)),
      m_stream(std::move(poStream))
{
    SetDescription(pszName);

    memcpy(&m_schema, schema, sizeof(m_schema));
    schema->release = nullptr;

    m_poAdapterLayer =
        std::make_unique<OGRArrowArrayToOGRFeatureAdapterLayer>(pszName);
    for (int i = 0; i < m_schema.n_children; ++i)
    {
        m_poAdapterLayer->CreateFieldFromArrowSchema(m_schema.children[i]);
    }
}

/************************************************************************/
/*                           ~OGRADBCLayer()                            */
/************************************************************************/

OGRADBCLayer::~OGRADBCLayer()
{
    OGRADBCError error;
    if (m_statement)
        ADBC_CALL(StatementRelease, m_statement.get(), error);
    if (m_schema.release)
        m_schema.release(&m_schema);
}

/************************************************************************/
/*                          GetNextRawFeature()                         */
/************************************************************************/

OGRFeature *OGRADBCLayer::GetNextRawFeature()
{
    if (m_bEOF)
        return nullptr;

    if (m_nIdx == m_poAdapterLayer->m_apoFeatures.size())
    {
        m_nIdx = 0;
        m_poAdapterLayer->m_apoFeatures.clear();

        if (!m_stream)
        {
            auto stream = std::make_unique<OGRArrowArrayStream>();
            if (!GetArrowStreamInternal(stream->get()))
            {
                m_bEOF = true;
                return nullptr;
            }
            m_stream = std::move(stream);
        }

        struct ArrowArray array;
        memset(&array, 0, sizeof(array));
        if (m_stream->get_next(&array) != 0)
        {
            m_bEOF = true;
            return nullptr;
        }
        const bool bOK =
            array.length
                ? m_poAdapterLayer->WriteArrowBatch(&m_schema, &array, nullptr)
                : false;
        if (array.release)
            array.release(&array);
        if (!bOK)
        {
            m_bEOF = true;
            return nullptr;
        }
    }

    auto poFeature = m_poAdapterLayer->m_apoFeatures[m_nIdx++].release();
    poFeature->SetFID(m_nFeatureID++);
    return poFeature;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRADBCLayer::ResetReading()
{
    if (m_nIdx > 0 || m_bEOF)
    {
        m_poAdapterLayer->m_apoFeatures.clear();
        m_stream.reset();
        m_bEOF = false;
        m_nIdx = 0;
        m_nFeatureID = 0;
    }
}

/************************************************************************/
/*                          TestCapability()                            */
/************************************************************************/

int OGRADBCLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCFastGetArrowStream))
    {
        return !m_poFilterGeom && !m_poAttrQuery;
    }
    else if (EQUAL(pszCap, OLCFastFeatureCount))
    {
        return !m_poFilterGeom && !m_poAttrQuery && m_bIsParquetLayer;
    }
    else
    {
        return false;
    }
}

/************************************************************************/
/*                            GetDataset()                              */
/************************************************************************/

GDALDataset *OGRADBCLayer::GetDataset()
{
    return m_poDS;
}

/************************************************************************/
/*                          GetArrowStream()                            */
/************************************************************************/

bool OGRADBCLayer::GetArrowStream(struct ArrowArrayStream *out_stream,
                                  CSLConstList papszOptions)
{
    if (m_poFilterGeom || m_poAttrQuery ||
        CPLFetchBool(papszOptions, GAS_OPT_DATETIME_AS_STRING, false))
    {
        return OGRLayer::GetArrowStream(out_stream, papszOptions);
    }

    if (m_stream)
    {
        memcpy(out_stream, m_stream->get(), sizeof(*out_stream));
        memset(m_stream->get(), 0, sizeof(*out_stream));
        m_stream.reset();
    }

    return GetArrowStreamInternal(out_stream);
}

/************************************************************************/
/*                       GetArrowStreamInternal()                       */
/************************************************************************/

bool OGRADBCLayer::GetArrowStreamInternal(struct ArrowArrayStream *out_stream)
{
    OGRADBCError error;
    int64_t rows_affected = -1;
    if (ADBC_CALL(StatementExecuteQuery, m_statement.get(), out_stream,
                  &rows_affected, error) != ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "AdbcStatementExecuteQuery() failed: %s", error.message());
        return false;
    }

    return true;
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

GIntBig OGRADBCLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom || m_poAttrQuery)
    {
        return OGRLayer::GetFeatureCount(bForce);
    }

    if (m_bIsParquetLayer)
    {
        return GetFeatureCountParquet();
    }

    if (m_nIdx > 0 || m_bEOF)
        m_stream.reset();

    if (!m_stream)
    {
        auto stream = std::make_unique<OGRArrowArrayStream>();
        if (!GetArrowStreamInternal(stream->get()))
        {
            return -1;
        }
        m_stream = std::move(stream);
    }

    GIntBig nTotal = 0;
    while (true)
    {
        struct ArrowArray array;
        memset(&array, 0, sizeof(array));
        if (m_stream->get_next(&array) != 0)
        {
            m_stream.reset();
            return -1;
        }
        const bool bStop = array.length == 0;
        nTotal += array.length;
        if (array.release)
            array.release(&array);
        if (bStop)
            break;
    }
    m_stream.reset();
    return nTotal;
}

/************************************************************************/
/*                        GetFeatureCountParquet()                      */
/************************************************************************/

GIntBig OGRADBCLayer::GetFeatureCountParquet()
{
    const std::string osSQL(CPLSPrintf(
        "SELECT CAST(SUM(num_rows) AS BIGINT) FROM parquet_file_metadata('%s')",
        OGRDuplicateCharacter(m_poDS->m_osParquetFilename, '\'').c_str()));
    auto poCountLayer = m_poDS->CreateLayer(osSQL.c_str(), "numrows");
    if (poCountLayer && poCountLayer->GetLayerDefn()->GetFieldCount() == 1)
    {
        auto poFeature =
            std::unique_ptr<OGRFeature>(poCountLayer->GetNextFeature());
        if (poFeature)
            return poFeature->GetFieldAsInteger64(0);
    }

    return -1;
}

#undef ADBC_CALL
