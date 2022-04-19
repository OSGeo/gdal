/******************************************************************************
 *
 * Project:  Arrow generic code
 * Purpose:  Arrow generic code
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


#include "ogr_arrow.h"

#include "cpl_json.h"
#include "cpl_time.h"

#include <cinttypes>

static constexpr int                        TZFLAG_UNINITIALIZED = -1;
static constexpr int                        TZFLAG_MIXED = -2;

/************************************************************************/
/*                      OGRArrowWriterLayer()                           */
/************************************************************************/

inline
OGRArrowWriterLayer::OGRArrowWriterLayer(
            arrow::MemoryPool* poMemoryPool,
            const std::shared_ptr<arrow::io::OutputStream>& poOutputStream,
            const char *pszLayerName):
    m_poMemoryPool(poMemoryPool),
    m_poOutputStream(poOutputStream)
{
    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    SetDescription(pszLayerName);
}

/************************************************************************/
/*                     ~OGRArrowWriterLayer()                           */
/************************************************************************/

inline
OGRArrowWriterLayer::~OGRArrowWriterLayer()
{
    CPLDebug("ARROW", "Memory pool (writer layer): bytes_allocated = %" PRId64,
             m_poMemoryPool->bytes_allocated());
    CPLDebug("ARROW", "Memory pool (writer layer): max_memory = %" PRId64,
             m_poMemoryPool->max_memory());

    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                         FinalizeWriting()                            */
/************************************************************************/

inline
void OGRArrowWriterLayer::FinalizeWriting()
{
    if( !IsFileWriterCreated() )
    {
        CreateWriter();
    }
    if( IsFileWriterCreated() )
    {
        PerformStepsBeforeFinalFlushGroup();

        if( !m_apoBuilders.empty() )
            FlushGroup();

        CloseFileWriter();
    }
}

/************************************************************************/
/*                       CreateSchemaCommon()                           */
/************************************************************************/

inline
void OGRArrowWriterLayer::CreateSchemaCommon()
{
    CPLAssert(static_cast<int>(m_aeGeomEncoding.size()) == m_poFeatureDefn->GetGeomFieldCount());

    std::vector<std::shared_ptr<arrow::Field>> fields;
    bool bNeedGDALSchema = false;

    m_anTZFlag.resize(m_poFeatureDefn->GetFieldCount(), TZFLAG_UNINITIALIZED);

    if( !m_osFIDColumn.empty() )
    {
        bNeedGDALSchema = true;
        fields.emplace_back(arrow::field(
            m_osFIDColumn, arrow::int64(), false));
    }

    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i )
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        std::shared_ptr<arrow::DataType> dt;
        const auto eSubDT = poFieldDefn->GetSubType();
        const auto& osDomainName = poFieldDefn->GetDomainName();
        const OGRFieldDomain* poFieldDomain = nullptr;
        const int nWidth = poFieldDefn->GetWidth();
        if( !osDomainName.empty() )
        {
            const auto oIter = m_oMapFieldDomains.find(osDomainName);
            if( oIter == m_oMapFieldDomains.end() )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Field %s references domain %s, but the later one "
                         "has not been created",
                         poFieldDefn->GetNameRef(),
                         osDomainName.c_str());
            }
            else
            {
                poFieldDomain = oIter->second.get();
            }
        }
        switch( poFieldDefn->GetType() )
        {
            case OFTInteger:
                if( eSubDT == OFSTBoolean )
                    dt = arrow::boolean();
                else if( eSubDT == OFSTInt16 )
                    dt = arrow::int16();
                else
                    dt = arrow::int32();
                if( poFieldDomain != nullptr )
                {
                    dt = arrow::dictionary(dt, arrow::utf8());
                }
                break;

            case OFTInteger64:
                dt = arrow::int64();
                if( poFieldDomain != nullptr )
                {
                    dt = arrow::dictionary(dt, arrow::utf8());
                }
                break;

            case OFTReal:
            {
                const int nPrecision = poFieldDefn->GetPrecision();
                if( nWidth != 0 && nPrecision != 0 )
                    dt = arrow::decimal(nWidth, nPrecision);
                else if( eSubDT == OFSTFloat32 )
                    dt = arrow::float32();
                else
                    dt = arrow::float64();
                break;
            }

            case OFTString:
            case OFTWideString:
                if( eSubDT != OFSTNone || nWidth > 0 )
                    bNeedGDALSchema = true;
                dt = arrow::utf8();
                break;

            case OFTBinary:
                if( nWidth != 0 )
                    dt = arrow::fixed_size_binary(nWidth);
                else
                    dt = arrow::binary();
                break;

            case OFTIntegerList:
                if( eSubDT == OFSTBoolean )
                    dt = arrow::list(arrow::boolean());
                else if( eSubDT == OFSTInt16 )
                    dt = arrow::list(arrow::int16());
                else
                    dt = arrow::list(arrow::int32());
                break;

            case OFTInteger64List:
                dt = arrow::list(arrow::int64());
                break;

            case OFTRealList:
                if( eSubDT == OFSTFloat32 )
                    dt = arrow::list(arrow::float32());
                else
                    dt = arrow::list(arrow::float64());
                break;

            case OFTStringList:
            case OFTWideStringList:
                dt = arrow::list(arrow::utf8());
                break;

            case OFTDate:
                dt = arrow::date32();
                break;

            case OFTTime:
                dt = arrow::time32(arrow::TimeUnit::MILLI);
                break;

            case OFTDateTime:
                dt = arrow::timestamp(arrow::TimeUnit::MILLI);
                break;
        }
        fields.emplace_back(arrow::field(
            poFieldDefn->GetNameRef(), dt, poFieldDefn->IsNullable()));
    }

    for( int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i )
    {
        const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
        const auto eGType = poGeomFieldDefn->GetType();
        const int nDim = 2 + (OGR_GT_HasZ(eGType) ? 1 : 0) + (OGR_GT_HasM(eGType) ? 1 : 0);

        const bool pointFieldNullable = GetDriverUCName() == "PARQUET";
        std::shared_ptr<arrow::Field> pointField;
        if( nDim == 2 )
            pointField = arrow::field("xy", arrow::float64(), pointFieldNullable);
        else if( nDim == 3 && OGR_GT_HasZ(eGType) )
            pointField = arrow::field("xyz", arrow::float64(), pointFieldNullable);
        else if( nDim == 3 && OGR_GT_HasM(eGType) )
            pointField = arrow::field("xym", arrow::float64(), pointFieldNullable);
        else
            pointField = arrow::field("xyzm", arrow::float64(), pointFieldNullable);

        std::shared_ptr<arrow::DataType> dt;
        switch( m_aeGeomEncoding[i] )
        {
            case OGRArrowGeomEncoding::WKB:
                dt = arrow::binary();
                break;

            case OGRArrowGeomEncoding::WKT:
                dt = arrow::utf8();
                break;

            case OGRArrowGeomEncoding::GEOARROW_GENERIC:
                CPLAssert(false);
                break;

            case OGRArrowGeomEncoding::GEOARROW_POINT:
                dt = arrow::fixed_size_list(pointField, nDim);
                break;

            case OGRArrowGeomEncoding::GEOARROW_LINESTRING:
                dt = arrow::list(arrow::fixed_size_list(pointField, nDim));
                break;

            case OGRArrowGeomEncoding::GEOARROW_POLYGON:
                dt = arrow::list(arrow::list(arrow::fixed_size_list(pointField, nDim)));
                break;

            case OGRArrowGeomEncoding::GEOARROW_MULTIPOINT:
                dt = arrow::list(arrow::fixed_size_list(pointField, nDim));
                break;

            case OGRArrowGeomEncoding::GEOARROW_MULTILINESTRING:
                dt = arrow::list(arrow::list(arrow::fixed_size_list(pointField, nDim)));
                break;

            case OGRArrowGeomEncoding::GEOARROW_MULTIPOLYGON:
                dt = arrow::list(arrow::list(arrow::list(arrow::fixed_size_list(pointField, nDim))));
                break;
        }

        auto field = arrow::field(
            poGeomFieldDefn->GetNameRef(), dt, poGeomFieldDefn->IsNullable());
        if( m_bWriteFieldArrowExtensionName )
        {
            auto kvMetadata = field->metadata() ? field->metadata()->Copy() :
                              std::make_shared<arrow::KeyValueMetadata>();
            kvMetadata->Append("ARROW::extension:name",
                               GetGeomEncodingAsString(m_aeGeomEncoding[i]));
            field = field->WithMetadata(kvMetadata);
        }

        fields.emplace_back(field);
    }

    m_aoEnvelopes.resize(m_poFeatureDefn->GetGeomFieldCount());
    m_oSetWrittenGeometryTypes.resize(m_poFeatureDefn->GetGeomFieldCount());

    m_poSchema = arrow::schema(fields);
    CPLAssert(m_poSchema);
    if( bNeedGDALSchema &&
        CPLTestBool(CPLGetConfigOption(("OGR_" + GetDriverUCName() + "_WRITE_GDAL_SCHEMA").c_str(), "YES")) )
    {
        CPLJSONObject oRoot;
        CPLJSONObject oColumns;

        if( !m_osFIDColumn.empty() )
            oRoot.Add("fid", m_osFIDColumn);

        oRoot.Add("columns", oColumns);
        for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i )
        {
            const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            CPLJSONObject oColumn;
            oColumns.Add(poFieldDefn->GetNameRef(), oColumn);
            oColumn.Add("type", OGR_GetFieldTypeName(poFieldDefn->GetType()));
            const auto eSubDT = poFieldDefn->GetSubType();
            if( eSubDT != OFSTNone )
                oColumn.Add("subtype", OGR_GetFieldSubTypeName(eSubDT));
            const int nWidth = poFieldDefn->GetWidth();
            if( nWidth > 0 )
                oColumn.Add("width", nWidth);
            const int nPrecision = poFieldDefn->GetPrecision();
            if( nPrecision > 0 )
                oColumn.Add("precision", nPrecision);
        }

        auto kvMetadata = m_poSchema->metadata() ? m_poSchema->metadata()->Copy() :
                              std::make_shared<arrow::KeyValueMetadata>();
        kvMetadata->Append("gdal:schema", oRoot.Format(CPLJSONObject::PrettyFormat::Plain));
        m_poSchema = m_poSchema->WithMetadata(kvMetadata);
        CPLAssert(m_poSchema);
    }
}
/************************************************************************/
/*                         FinalizeSchema()                             */
/************************************************************************/

inline
void OGRArrowWriterLayer::FinalizeSchema()
{
    // Final tuning of schema taking into actual timezone values
    // from features
    int nArrowIdxFirstField = !m_osFIDColumn.empty() ? 1 : 0;
    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i )
    {
        if( m_anTZFlag[i] > 1 )
        {
            const int nOffset = (m_anTZFlag[i] - 100) * 15;
            int nHours = static_cast<int>(nOffset / 60);  // Round towards zero.
            const int nMinutes = std::abs(nOffset - nHours * 60);

            const std::string osTZ = CPLSPrintf("%c%02d:%02d",
                nOffset >= 0 ? '+' : '-',
                std::abs(nHours), nMinutes);
            auto dt = arrow::timestamp(arrow::TimeUnit::MILLI, osTZ);
            const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            auto field = arrow::field(
                poFieldDefn->GetNameRef(), dt, poFieldDefn->IsNullable());
            auto result = m_poSchema->SetField(nArrowIdxFirstField + i, field);
            if( !result.ok() )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Schema::SetField() failed with %s",
                         result.status().message().c_str());
            }
            else
            {
                m_poSchema = *result;
            }
        }
    }
}

/************************************************************************/
/*                         AddFieldDomain()                             */
/************************************************************************/

inline
bool OGRArrowWriterLayer::AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                                           std::string& failureReason)
{
    if( domain->GetDomainType() != OFDT_CODED )
    {
        failureReason = "Only coded field domains are supported by Arrow";
        return false;
    }

    const OGRCodedFieldDomain* poDomain = static_cast<
        const OGRCodedFieldDomain*>(domain.get());
    const OGRCodedValue* psIter = poDomain->GetEnumeration();

    auto poStringBuilder = std::make_shared<arrow::StringBuilder>(m_poMemoryPool);

    int nLastCode = -1;
    for(; psIter->pszCode; ++psIter )
    {
        if( CPLGetValueType(psIter->pszCode) != CPL_VALUE_INTEGER )
        {
            failureReason = "Non integer code in domain ";
            failureReason += domain->GetName();
            return false;
        }
        int nCode = atoi(psIter->pszCode);
        if( nCode <= nLastCode || nCode - nLastCode > 100 )
        {
            failureReason = "Too sparse codes in domain ";
            failureReason += domain->GetName();
            return false;
        }
        for( int i = nLastCode + 1; i < nCode; ++i )
        {
            poStringBuilder->AppendNull();
        }
        if( psIter->pszValue )
            poStringBuilder->Append(psIter->pszValue);
        else
            poStringBuilder->AppendNull();
        nLastCode = nCode;
    }

    std::shared_ptr<arrow::Array> stringArray;
    auto status = poStringBuilder->Finish(&stringArray);
    if( !status.ok() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
             "StringArray::Finish() failed with %s", status.message().c_str());
        return false;
    }

    m_oMapFieldDomainToStringArray[domain->GetName()] = stringArray;
    m_oMapFieldDomains[domain->GetName()] = std::move(domain);
    return true;
}

/************************************************************************/
/*                          GetFieldDomainNames()                       */
/************************************************************************/

inline
std::vector<std::string> OGRArrowWriterLayer::GetFieldDomainNames() const
{
    std::vector<std::string> names;
    names.reserve( m_oMapFieldDomains.size() );
    for(const auto& it : m_oMapFieldDomains ) {
        names.emplace_back( it.first );
    }
    return names;
}

/************************************************************************/
/*                          GetFieldDomain()                            */
/************************************************************************/

inline
const OGRFieldDomain* OGRArrowWriterLayer::GetFieldDomain(const std::string& name) const
{
    const auto iter = m_oMapFieldDomains.find(name);
    if( iter == m_oMapFieldDomains.end() )
        return nullptr;
    return iter->second.get();
}

/************************************************************************/
/*                          CreateField()                               */
/************************************************************************/

inline
OGRErr OGRArrowWriterLayer::CreateField( OGRFieldDefn *poField, int /* bApproxOK */)
{
    if( m_poSchema )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot add field after a first feature has been written");
        return OGRERR_FAILURE;
    }
    m_poFeatureDefn->AddFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                   GetPreciseArrowGeomEncoding()                    */
/************************************************************************/

inline
OGRArrowGeomEncoding OGRArrowWriterLayer::GetPreciseArrowGeomEncoding(
                                                    OGRwkbGeometryType eGType)
{
    const auto eFlatType = wkbFlatten(eGType);
    if( eFlatType == wkbPoint )
    {
        return OGRArrowGeomEncoding::GEOARROW_POINT;
    }
    else if( eFlatType == wkbLineString )
    {
        return OGRArrowGeomEncoding::GEOARROW_LINESTRING;
    }
    else if( eFlatType == wkbPolygon )
    {
        return OGRArrowGeomEncoding::GEOARROW_POLYGON;
    }
    else if( eFlatType == wkbMultiPoint )
    {
        return OGRArrowGeomEncoding::GEOARROW_MULTIPOINT;
    }
    else if( eFlatType == wkbMultiLineString )
    {
        return OGRArrowGeomEncoding::GEOARROW_MULTILINESTRING;
    }
    else if( eFlatType == wkbMultiPolygon )
    {
        return OGRArrowGeomEncoding::GEOARROW_MULTIPOLYGON;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GEOMETRY_FORMAT=GEOARROW is currently not supported for %s",
                 OGRGeometryTypeToName(eGType));
        return OGRArrowGeomEncoding::GEOARROW_GENERIC;
    }
}

/************************************************************************/
/*                        GetGeomEncodingAsString()                   */
/************************************************************************/

inline
const char* OGRArrowWriterLayer::GetGeomEncodingAsString(OGRArrowGeomEncoding eGeomEncoding)
{
    switch( eGeomEncoding )
    {
        case OGRArrowGeomEncoding::WKB:
            return "WKB";
        case OGRArrowGeomEncoding::WKT:
            return "WKT";
        case OGRArrowGeomEncoding::GEOARROW_GENERIC:
            CPLAssert(false);
            break;
        case OGRArrowGeomEncoding::GEOARROW_POINT:
            return "geoarrow.point";
        case OGRArrowGeomEncoding::GEOARROW_LINESTRING:
            return "geoarrow.linestring";
        case OGRArrowGeomEncoding::GEOARROW_POLYGON:
            return "geoarrow.polygon";
        case OGRArrowGeomEncoding::GEOARROW_MULTIPOINT:
            return "geoarrow.multipoint";
        case OGRArrowGeomEncoding::GEOARROW_MULTILINESTRING:
            return "geoarrow.multilinestring";
        case OGRArrowGeomEncoding::GEOARROW_MULTIPOLYGON:
            return "geoarrow.multipolygon";
    }
    return nullptr;
}

/************************************************************************/
/*                          CreateGeomField()                           */
/************************************************************************/

inline
OGRErr OGRArrowWriterLayer::CreateGeomField( OGRGeomFieldDefn *poField, int /* bApproxOK */)
{
    if( m_poSchema )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot add field after a first feature has been written");
        return OGRERR_FAILURE;
    }
    const auto eGType = poField->GetType();
    if( !IsSupportedGeometryType(eGType) )
    {
        return OGRERR_FAILURE;
    }

    if( poField->GetSpatialRef() == nullptr )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Geometry column should have an associated CRS");
    }
    auto eGeomEncoding = m_eGeomEncoding;
    if( eGeomEncoding == OGRArrowGeomEncoding::GEOARROW_GENERIC )
    {
        eGeomEncoding = GetPreciseArrowGeomEncoding(eGType);
        if( eGeomEncoding == OGRArrowGeomEncoding::GEOARROW_GENERIC )
            return OGRERR_FAILURE;
    }
    m_aeGeomEncoding.push_back(eGeomEncoding);
    m_poFeatureDefn->AddGeomFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                        MakeGeoArrowBuilder()                         */
/************************************************************************/

static std::shared_ptr<arrow::ArrayBuilder> MakeGeoArrowBuilder(
                                arrow::MemoryPool* poMemoryPool,
                                int nDim,
                                int nDepth)
{
    if( nDepth == 0 )
        return std::make_shared<arrow::FixedSizeListBuilder>(poMemoryPool,
                std::make_shared<arrow::DoubleBuilder>(poMemoryPool), nDim);
    else
        return std::make_shared<arrow::ListBuilder>(poMemoryPool,
                MakeGeoArrowBuilder(poMemoryPool, nDim, nDepth - 1));
}

/************************************************************************/
/*                        CreateArrayBuilders()                         */
/************************************************************************/

inline
void OGRArrowWriterLayer::CreateArrayBuilders()
{
    m_apoBuilders.reserve(1 + m_poFeatureDefn->GetFieldCount() + m_poFeatureDefn->GetGeomFieldCount() );

    int nArrowIdx = 0;
    if( !m_osFIDColumn.empty() )
    {
        m_apoBuilders.emplace_back(std::make_shared<arrow::Int64Builder>());
        nArrowIdx ++;
    }

    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i, ++nArrowIdx )
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        std::shared_ptr<arrow::ArrayBuilder> builder;
        const auto eSubDT = poFieldDefn->GetSubType();
        switch( poFieldDefn->GetType() )
        {
            case OFTInteger:
                if( eSubDT == OFSTBoolean )
                    builder = std::make_shared<arrow::BooleanBuilder>(m_poMemoryPool);
                else if( eSubDT == OFSTInt16 )
                    builder = std::make_shared<arrow::Int16Builder>(m_poMemoryPool);
                else
                    builder = std::make_shared<arrow::Int32Builder>(m_poMemoryPool);
                break;

            case OFTInteger64:
                builder = std::make_shared<arrow::Int64Builder>(m_poMemoryPool);
                break;

            case OFTReal:
            {
                const auto arrowType = m_poSchema->fields()[nArrowIdx]->type();
                if( arrowType->id() == arrow::Type::DECIMAL128 )
                    builder = std::make_shared<arrow::Decimal128Builder>(arrowType, m_poMemoryPool);
                else if( arrowType->id() == arrow::Type::DECIMAL256 )
                    builder = std::make_shared<arrow::Decimal256Builder>(arrowType, m_poMemoryPool);
                else if( eSubDT == OFSTFloat32 )
                    builder = std::make_shared<arrow::FloatBuilder>(m_poMemoryPool);
                else
                    builder = std::make_shared<arrow::DoubleBuilder>(m_poMemoryPool);
                break;
            }

            case OFTString:
            case OFTWideString:
                builder = std::make_shared<arrow::StringBuilder>(m_poMemoryPool);
                break;

            case OFTBinary:
                if( poFieldDefn->GetWidth() != 0 )
                    builder = std::make_shared<arrow::FixedSizeBinaryBuilder>(
                        arrow::fixed_size_binary(poFieldDefn->GetWidth()),
                        m_poMemoryPool);
                else
                    builder = std::make_shared<arrow::BinaryBuilder>(m_poMemoryPool);
                break;

            case OFTIntegerList:
            {
                std::shared_ptr<arrow::ArrayBuilder> poBaseBuilder;
                if( eSubDT == OFSTBoolean )
                    poBaseBuilder = std::make_shared<arrow::BooleanBuilder>(m_poMemoryPool);
                else if( eSubDT == OFSTInt16 )
                    poBaseBuilder = std::make_shared<arrow::Int16Builder>(m_poMemoryPool);
                else
                    poBaseBuilder = std::make_shared<arrow::Int32Builder>(m_poMemoryPool);
                builder = std::make_shared<arrow::ListBuilder>(m_poMemoryPool,
                       poBaseBuilder);
                break;
            }

            case OFTInteger64List:
                builder = std::make_shared<arrow::ListBuilder>(m_poMemoryPool,
                       std::make_shared<arrow::Int64Builder>(m_poMemoryPool));

                break;

            case OFTRealList:
                if( eSubDT == OFSTFloat32 )
                    builder = std::make_shared<arrow::ListBuilder>(m_poMemoryPool,
                       std::make_shared<arrow::FloatBuilder>(m_poMemoryPool));
                else
                    builder = std::make_shared<arrow::ListBuilder>(m_poMemoryPool,
                       std::make_shared<arrow::DoubleBuilder>(m_poMemoryPool));
                break;

            case OFTStringList:
            case OFTWideStringList:
                builder = std::make_shared<arrow::ListBuilder>(m_poMemoryPool,
                   std::make_shared<arrow::StringBuilder>(m_poMemoryPool));

                break;

            case OFTDate:
                builder = std::make_shared<arrow::Date32Builder>(m_poMemoryPool);
                break;

            case OFTTime:
                builder = std::make_shared<arrow::Time32Builder>(
                    arrow::time32(arrow::TimeUnit::MILLI),
                    m_poMemoryPool);
                break;

            case OFTDateTime:
                builder = std::make_shared<arrow::TimestampBuilder>(
                    arrow::timestamp(arrow::TimeUnit::MILLI),
                    m_poMemoryPool);
                break;
        }
        m_apoBuilders.emplace_back(builder);
    }

    for( int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i, ++nArrowIdx )
    {
        std::shared_ptr<arrow::ArrayBuilder> builder;
        const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
        const auto eGType = poGeomFieldDefn->GetType();
        const int nDim = 2 + (OGR_GT_HasZ(eGType) ? 1 : 0) + (OGR_GT_HasM(eGType) ? 1 : 0);

        if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::WKB )
            builder = std::make_shared<arrow::BinaryBuilder>(m_poMemoryPool);
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::WKT )
            builder = std::make_shared<arrow::StringBuilder>(m_poMemoryPool);
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_POINT )
        {
            builder = MakeGeoArrowBuilder(
                m_poMemoryPool, nDim, 0);
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_LINESTRING )
        {
            builder = MakeGeoArrowBuilder(
                m_poMemoryPool, nDim, 1);
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_POLYGON )
        {
            builder = MakeGeoArrowBuilder(
                m_poMemoryPool, nDim, 2);

        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_MULTIPOINT )
        {
            builder = MakeGeoArrowBuilder(
                m_poMemoryPool, nDim, 1);

        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_MULTILINESTRING )
        {
            builder = MakeGeoArrowBuilder(
                m_poMemoryPool, nDim, 2);

        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_MULTIPOLYGON )
        {
            builder = MakeGeoArrowBuilder(
                m_poMemoryPool, nDim, 3);

        }
        else
        {
            CPLAssert(false);
        }
        m_apoBuilders.emplace_back(builder);
    }
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

inline
OGRErr OGRArrowWriterLayer::ICreateFeature( OGRFeature* poFeature )
{
    if( m_poSchema == nullptr )
    {
        CreateSchema();
    }

    if( m_apoBuilders.empty() )
    {
        CreateArrayBuilders();
    }

    // First pass to check not-null constraints as Arrow doesn't seem
    // to do that on the writing side. But such files can't be read.
    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for( int i = 0; i < nFieldCount; ++i )
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        if( !poFieldDefn->IsNullable() && !poFeature->IsFieldSetAndNotNull(i) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Null value found in non-nullable field %s",
                     poFieldDefn->GetNameRef());
            return OGRERR_FAILURE;
        }
    }

    const int nGeomFieldCount = m_poFeatureDefn->GetGeomFieldCount();
    for( int i = 0; i < nGeomFieldCount; ++i )
    {
        const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
        if( !poGeomFieldDefn->IsNullable() && poFeature->GetGeomFieldRef(i) == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Null value found in non-nullable geometry field %s",
                     poGeomFieldDefn->GetNameRef());
            return OGRERR_FAILURE;
        }
    }

    // Write FID, if FID column present
    int nArrowIdx = 0;
    if( !m_osFIDColumn.empty() )
    {
        int64_t nFID = poFeature->GetFID();
        if( nFID == OGRNullFID )
        {
            nFID = m_nFeatureCount;
            poFeature->SetFID(nFID);
        }
        auto poBuilder = static_cast<arrow::Int64Builder*>(m_apoBuilders[0].get());
        poBuilder->Append(nFID);
        nArrowIdx ++;
    }

    // Write attributes
    for( int i = 0; i < nFieldCount; ++i, ++nArrowIdx )
    {
        auto poBuilder = m_apoBuilders[nArrowIdx].get();
        if( !poFeature->IsFieldSetAndNotNull(i) )
        {
            poBuilder->AppendNull();
            continue;
        }

        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        std::shared_ptr<arrow::ArrayBuilder> builder;
        const auto eSubDT = poFieldDefn->GetSubType();
        switch( poFieldDefn->GetType() )
        {
            case OFTInteger:
                if( eSubDT == OFSTBoolean )
                    static_cast<arrow::BooleanBuilder*>(poBuilder)->Append(
                        poFeature->GetFieldAsInteger(i) != 0);
                else if( eSubDT == OFSTInt16 )
                    static_cast<arrow::Int16Builder*>(poBuilder)->Append(
                        static_cast<int16_t>(poFeature->GetFieldAsInteger(i)));
                else
                    static_cast<arrow::Int32Builder*>(poBuilder)->Append(
                        poFeature->GetFieldAsInteger(i));
                break;

            case OFTInteger64:
                static_cast<arrow::Int64Builder*>(poBuilder)->Append(
                    static_cast<int64_t>(poFeature->GetFieldAsInteger64(i)));
                break;

            case OFTReal:
            {
                const auto arrowType = m_poSchema->fields()[nArrowIdx]->type();
                const double dfVal = poFeature->GetFieldAsDouble(i);
                if( arrowType->id() == arrow::Type::DECIMAL128 )
                {
                    auto res = arrow::Decimal128::FromReal(
                        dfVal, poFieldDefn->GetWidth(), poFieldDefn->GetPrecision());
                    if( res.ok() )
                    {
                        static_cast<arrow::Decimal128Builder*>(poBuilder)->Append(*res);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot parse %.18g as a %d.%d decimal",
                                 dfVal,
                                 poFieldDefn->GetWidth(),
                                 poFieldDefn->GetPrecision());
                        poBuilder->AppendNull();
                    }
                }
                else if( arrowType->id() == arrow::Type::DECIMAL256 )
                {
                    auto res = arrow::Decimal256::FromReal(
                        dfVal, poFieldDefn->GetWidth(), poFieldDefn->GetPrecision());
                    if( res.ok() )
                    {
                        static_cast<arrow::Decimal256Builder*>(poBuilder)->Append(*res);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot parse %.18g as a %d.%d decimal",
                                 dfVal,
                                 poFieldDefn->GetWidth(),
                                 poFieldDefn->GetPrecision());
                        poBuilder->AppendNull();
                    }
                }
                else if( eSubDT == OFSTFloat32 )
                {
                    static_cast<arrow::FloatBuilder*>(poBuilder)->Append(
                        static_cast<float>(dfVal));
                }
                else
                {
                    static_cast<arrow::DoubleBuilder*>(poBuilder)->Append(dfVal);
                }
                break;
            }

            case OFTString:
            case OFTWideString:
                static_cast<arrow::StringBuilder*>(poBuilder)->Append(
                    poFeature->GetFieldAsString(i));
                break;

            case OFTBinary:
            {
                int nSize = 0;
                const auto pData = poFeature->GetFieldAsBinary(i, &nSize);
                if( poFieldDefn->GetWidth() != 0 )
                {
                    if( poFieldDefn->GetWidth() != nSize )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot write field %s. Got %d bytes, expected %d",
                                 poFieldDefn->GetNameRef(),
                                 nSize,
                                 poFieldDefn->GetWidth());
                        poBuilder->AppendNull();
                    }
                    else
                    {
                        static_cast<arrow::FixedSizeBinaryBuilder*>(poBuilder)->Append(pData);
                    }
                }
                else
                    static_cast<arrow::BinaryBuilder*>(poBuilder)->Append(pData, nSize);
                break;
            }

            case OFTIntegerList:
            {
                auto poListBuilder = static_cast<arrow::ListBuilder*>(poBuilder);
                if( eSubDT == OFSTBoolean )
                {
                    poListBuilder->Append();
                    auto poValueBuilder = static_cast<arrow::BooleanBuilder*>(poListBuilder->value_builder());
                    int nValues = 0;
                    const auto panValues = poFeature->GetFieldAsIntegerList(i, &nValues);
                    for(int j = 0; j < nValues; ++j )
                        poValueBuilder->Append(panValues[j] != 0);
                }
                else if( eSubDT == OFSTInt16 )
                {
                    poListBuilder->Append();
                    auto poValueBuilder = static_cast<arrow::Int16Builder*>(poListBuilder->value_builder());
                    int nValues = 0;
                    const auto panValues = poFeature->GetFieldAsIntegerList(i, &nValues);
                    for(int j = 0; j < nValues; ++j )
                        poValueBuilder->Append(static_cast<int16_t>(panValues[j]));
                }
                else
                {
                    poListBuilder->Append();
                    auto poValueBuilder = static_cast<arrow::Int32Builder*>(poListBuilder->value_builder());
                    int nValues = 0;
                    const auto panValues = poFeature->GetFieldAsIntegerList(i, &nValues);
                    for(int j = 0; j < nValues; ++j )
                        poValueBuilder->Append(panValues[j]);
                }
                break;
            }

            case OFTInteger64List:
            {
                auto poListBuilder = static_cast<arrow::ListBuilder*>(poBuilder);
                poListBuilder->Append();
                auto poValueBuilder = static_cast<arrow::Int64Builder*>(poListBuilder->value_builder());
                int nValues = 0;
                const auto panValues = poFeature->GetFieldAsInteger64List(i, &nValues);
                for(int j = 0; j < nValues; ++j )
                    poValueBuilder->Append(static_cast<int64_t>(panValues[j]));
                break;
            }

            case OFTRealList:
            {
                auto poListBuilder = static_cast<arrow::ListBuilder*>(poBuilder);
                if( eSubDT == OFSTFloat32 )
                {
                    poListBuilder->Append();
                    auto poValueBuilder = static_cast<arrow::FloatBuilder*>(poListBuilder->value_builder());
                    int nValues = 0;
                    const auto padfValues = poFeature->GetFieldAsDoubleList(i, &nValues);
                    for(int j = 0; j < nValues; ++j )
                        poValueBuilder->Append(static_cast<float>(padfValues[j]));
                }
                else
                {
                    poListBuilder->Append();
                    auto poValueBuilder = static_cast<arrow::DoubleBuilder*>(poListBuilder->value_builder());
                    int nValues = 0;
                    const auto padfValues = poFeature->GetFieldAsDoubleList(i, &nValues);
                    for(int j = 0; j < nValues; ++j )
                        poValueBuilder->Append(padfValues[j]);
                }
                break;
            }

            case OFTStringList:
            case OFTWideStringList:
            {
                auto poListBuilder = static_cast<arrow::ListBuilder*>(poBuilder);
                poListBuilder->Append();
                auto poValueBuilder = static_cast<arrow::StringBuilder*>(poListBuilder->value_builder());
                const auto papszValues = poFeature->GetFieldAsStringList(i);
                for(int j = 0; papszValues && papszValues[j]; ++j )
                    poValueBuilder->Append(papszValues[j]);
                break;
            }

            case OFTDate:
            {
                int nYear, nMonth, nDay, nHour, nMinute;
                float fSec;
                int nTZFlag;
                poFeature->GetFieldAsDateTime(i,
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &fSec, &nTZFlag);
                struct tm brokenDown;
                memset(&brokenDown, 0, sizeof(brokenDown));
                brokenDown.tm_year = nYear - 1900;
                brokenDown.tm_mon = nMonth - 1;
                brokenDown.tm_mday = nDay;
                GIntBig nVal = CPLYMDHMSToUnixTime(&brokenDown);
                static_cast<arrow::Date32Builder*>(poBuilder)->Append(static_cast<int>(nVal / 86400));
                break;
            }

            case OFTTime:
            {
                int nYear, nMonth, nDay, nHour, nMinute;
                float fSec;
                int nTZFlag;
                poFeature->GetFieldAsDateTime(i,
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &fSec, &nTZFlag);
                int nVal = nHour * 3600 + nMinute * 60;
                static_cast<arrow::Time32Builder*>(poBuilder)->Append(
                    static_cast<int>((static_cast<double>(nVal) + fSec) * 1000 + 0.5));
                break;
            }

            case OFTDateTime:
            {
                int nYear, nMonth, nDay, nHour, nMinute;
                float fSec;
                int nTZFlag;
                poFeature->GetFieldAsDateTime(i,
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &fSec, &nTZFlag);
                struct tm brokenDown;
                memset(&brokenDown, 0, sizeof(brokenDown));
                brokenDown.tm_year = nYear - 1900;
                brokenDown.tm_mon = nMonth - 1;
                brokenDown.tm_mday = nDay;
                brokenDown.tm_hour = nHour;
                brokenDown.tm_min = nMinute;
                brokenDown.tm_sec = 0;
                GIntBig nVal = CPLYMDHMSToUnixTime(&brokenDown);
                if( !IsFileWriterCreated() && m_anTZFlag[i] != TZFLAG_MIXED )
                {
                    if( m_anTZFlag[i] == TZFLAG_UNINITIALIZED )
                        m_anTZFlag[i] = nTZFlag;
                    else if( m_anTZFlag[i] != nTZFlag )
                    {
                        if( m_anTZFlag[i] > 1 && nTZFlag > 1 )
                        {
                            m_anTZFlag[i] = 100; // harmonize on UTC
                        }
                        else
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Field %s contains a mix of "
                                     "timezone-aware and local/without "
                                     "timezone values.",
                                     poFieldDefn->GetNameRef());
                            m_anTZFlag[i] = TZFLAG_MIXED;
                        }
                    }
                }
                if( nTZFlag != 0 && nTZFlag != 1 )
                {
                    nVal -= (nTZFlag - 100) * 15 * 60;
                }
                static_cast<arrow::TimestampBuilder*>(poBuilder)->Append(
                    static_cast<int64_t>((static_cast<double>(nVal) + fSec) * 1000 + 0.5));
                break;
            }

        }
    }

    // Write geometries
    for( int i = 0; i < nGeomFieldCount; ++i, ++nArrowIdx )
    {
        auto poBuilder = m_apoBuilders[nArrowIdx].get();
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        const auto eGType = poGeom ? poGeom->getGeometryType() : wkbNone;
        const auto eColumnGType = m_poFeatureDefn->GetGeomFieldDefn(i)->GetType();
        const bool bIsEmpty = poGeom != nullptr && poGeom->IsEmpty();
        if( poGeom != nullptr && !bIsEmpty )
        {
            OGREnvelope oEnvelope;
            poGeom->getEnvelope(&oEnvelope);
            m_aoEnvelopes[i].Merge(oEnvelope);
            m_oSetWrittenGeometryTypes[i].insert(eGType);
        }

        if( poGeom == nullptr )
        {
            if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_POINT &&
                GetDriverUCName() == "PARQUET" )
            {
                // For some reason, Parquet doesn't support a NULL FixedSizeList on reading
                auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder*>(poBuilder);
                poPointBuilder->Append();
                auto poValueBuilder = static_cast<arrow::DoubleBuilder*>(poPointBuilder->value_builder());
                poValueBuilder->Append(std::numeric_limits<double>::quiet_NaN());
                poValueBuilder->Append(std::numeric_limits<double>::quiet_NaN());
                if( OGR_GT_HasZ(eGType) )
                    poValueBuilder->Append(std::numeric_limits<double>::quiet_NaN());
                if( OGR_GT_HasM(eGType) )
                    poValueBuilder->Append(std::numeric_limits<double>::quiet_NaN());
            }
            else
            {
                poBuilder->AppendNull();
            }
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::WKB )
        {
            std::unique_ptr<OGRGeometry> poGeomModified;
            if( OGR_GT_HasM(eGType) && !OGR_GT_HasM(eColumnGType) )
            {
                static bool bHasWarned = false;
                if( !bHasWarned )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Removing M component from geometry");
                    bHasWarned = true;
                }
                poGeomModified.reset(poGeom->clone());
                poGeomModified->setMeasured(false);
                poGeom = poGeomModified.get();
            }
            FixupGeometryBeforeWriting(poGeom);
            const auto nSize = poGeom->WkbSize();
            if( nSize < INT_MAX )
            {
                m_abyBuffer.resize(nSize);
                poGeom->exportToWkb(wkbNDR, &m_abyBuffer[0], wkbVariantIso);
                static_cast<arrow::BinaryBuilder*>(poBuilder)->Append(
                    m_abyBuffer.data(), static_cast<int>(m_abyBuffer.size()));
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                     "Too big geometry. "
                     "Writing null geometry");
                poBuilder->AppendNull();
            }
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::WKT )
        {
            OGRWktOptions options;
            options.variant = wkbVariantIso;
            static_cast<arrow::StringBuilder*>(poBuilder)->Append(
                poGeom->exportToWkt(options));
        }
        // The following checks are only valid for GeoArrow encoding
        else if( (!bIsEmpty && eGType != eColumnGType) ||
                 (bIsEmpty && wkbFlatten(eGType) != wkbFlatten(eColumnGType)) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry of type %s found, whereas %s is expected. "
                     "Writing null geometry",
                     OGRGeometryTypeToName(eGType),
                     OGRGeometryTypeToName(eColumnGType));
            poBuilder->AppendNull();
        }
        else if( !bIsEmpty &&
                 poGeom->Is3D() != OGR_GT_HasZ(eColumnGType) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry Z flag (%d) != column geometry type Z flag (%d)d. "
                     "Writing null geometry",
                     poGeom->Is3D(),
                     OGR_GT_HasZ(eColumnGType));
            poBuilder->AppendNull();
        }
        else if( !bIsEmpty &&
                 poGeom->IsMeasured() != OGR_GT_HasM(eColumnGType) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry M flag (%d) != column geometry type M flag (%d)d. "
                     "Writing null geometry",
                     poGeom->IsMeasured(),
                     OGR_GT_HasM(eColumnGType));
            poBuilder->AppendNull();
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_POINT )
        {
            const auto poPoint = poGeom->toPoint();
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder*>(poBuilder);
            poPointBuilder->Append();
            auto poValueBuilder = static_cast<arrow::DoubleBuilder*>(poPointBuilder->value_builder());
            poValueBuilder->Append(poPoint->getX());
            poValueBuilder->Append(poPoint->getY());
            if( OGR_GT_HasZ(eColumnGType) )
                poValueBuilder->Append(poPoint->getZ());
            if( OGR_GT_HasM(eColumnGType) )
                poValueBuilder->Append(poPoint->getM());
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_LINESTRING )
        {
            const auto poLS = poGeom->toLineString();
            auto poListBuilder = static_cast<arrow::ListBuilder*>(poBuilder);
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder*>(poListBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder*>(poPointBuilder->value_builder());
            poListBuilder->Append();
            for( int j = 0; j < poLS->getNumPoints(); ++j )
            {
                poPointBuilder->Append();
                poValueBuilder->Append(poLS->getX(j));
                poValueBuilder->Append(poLS->getY(j));
                if( poGeom->Is3D() )
                    poValueBuilder->Append(poLS->getZ(j));
                if( poGeom->IsMeasured() )
                    poValueBuilder->Append(poLS->getM(j));
            }
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_POLYGON )
        {
            const auto poPolygon = poGeom->toPolygon();
            auto poPolygonBuilder = static_cast<arrow::ListBuilder*>(poBuilder);
            auto poRingBuilder = static_cast<arrow::ListBuilder*>(poPolygonBuilder->value_builder());
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder*>(poRingBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder*>(poPointBuilder->value_builder());
            poPolygonBuilder->Append();
            for( const auto* poRing: *poPolygon )
            {
                poRingBuilder->Append();
                for( int j = 0; j < poRing->getNumPoints(); ++j )
                {
                    poPointBuilder->Append();
                    poValueBuilder->Append(poRing->getX(j));
                    poValueBuilder->Append(poRing->getY(j));
                    if( poGeom->Is3D() )
                        poValueBuilder->Append(poRing->getZ(j));
                    if( poGeom->IsMeasured() )
                        poValueBuilder->Append(poRing->getM(j));
                }
            }
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_MULTIPOINT )
        {
            const auto poMultiPoint = poGeom->toMultiPoint();
            auto poListBuilder = static_cast<arrow::ListBuilder*>(poBuilder);
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder*>(poListBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder*>(poPointBuilder->value_builder());
            poListBuilder->Append();
            for( const auto* poPoint: *poMultiPoint )
            {
                poPointBuilder->Append();
                poValueBuilder->Append(poPoint->getX());
                poValueBuilder->Append(poPoint->getY());
                if( poGeom->Is3D() )
                    poValueBuilder->Append(poPoint->getZ());
                if( poGeom->IsMeasured() )
                    poValueBuilder->Append(poPoint->getM());
            }
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_MULTILINESTRING )
        {
            const auto poMLS = poGeom->toMultiLineString();
            auto poMLSBuilder = static_cast<arrow::ListBuilder*>(poBuilder);
            auto poLSBuilder = static_cast<arrow::ListBuilder*>(poMLSBuilder->value_builder());
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder*>(poLSBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder*>(poPointBuilder->value_builder());
            poMLSBuilder->Append();
            for( const auto* poLS: *poMLS )
            {
                poLSBuilder->Append();
                for( int j = 0; j < poLS->getNumPoints(); ++j )
                {
                    poPointBuilder->Append();
                    poValueBuilder->Append(poLS->getX(j));
                    poValueBuilder->Append(poLS->getY(j));
                    if( poGeom->Is3D() )
                        poValueBuilder->Append(poLS->getZ(j));
                    if( poGeom->IsMeasured() )
                        poValueBuilder->Append(poLS->getM(j));
                }
            }
        }
        else if( m_aeGeomEncoding[i] == OGRArrowGeomEncoding::GEOARROW_MULTIPOLYGON )
        {
            const auto poMPoly = poGeom->toMultiPolygon();
            auto poMPolyBuilder = static_cast<arrow::ListBuilder*>(poBuilder);
            auto poPolyBuilder = static_cast<arrow::ListBuilder*>(poMPolyBuilder->value_builder());
            auto poRingBuilder = static_cast<arrow::ListBuilder*>(poPolyBuilder->value_builder());
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder*>(poRingBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder*>(poPointBuilder->value_builder());
            poMPolyBuilder->Append();
            for( const auto* poPolygon: *poMPoly )
            {
                poPolyBuilder->Append();
                for( const auto* poRing: *poPolygon )
                {
                    poRingBuilder->Append();
                    for( int j = 0; j < poRing->getNumPoints(); ++j )
                    {
                        poPointBuilder->Append();
                        poValueBuilder->Append(poRing->getX(j));
                        poValueBuilder->Append(poRing->getY(j));
                        if( poGeom->Is3D() )
                            poValueBuilder->Append(poRing->getZ(j));
                        if( poGeom->IsMeasured() )
                            poValueBuilder->Append(poRing->getM(j));
                    }
                }
            }
        }
        else
        {
            CPLAssert(false);
        }
    }

    m_nFeatureCount ++;

    // Flush the current row group if reaching the limit of rows per group.
    if( !m_apoBuilders.empty() && m_apoBuilders[0]->length() == m_nRowGroupSize )
    {
        if( !IsFileWriterCreated() )
        {
            CreateWriter();
            if( !IsFileWriterCreated() )
                return OGRERR_FAILURE;
        }

        if( !FlushGroup() )
            return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

inline
GIntBig OGRArrowWriterLayer::GetFeatureCount(int bForce)
{
    if( m_poAttrQuery == nullptr && m_poFilterGeom == nullptr )
    {
        return m_nFeatureCount;
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

inline
int OGRArrowWriterLayer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCCreateField) || EQUAL(pszCap, OLCCreateGeomField) )
        return m_poSchema == nullptr;

    if( EQUAL(pszCap, OLCSequentialWrite) )
        return true;

    if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return true;

    if( EQUAL(pszCap, OLCMeasuredGeometries) )
        return true;

    return false;
}

/************************************************************************/
/*                         WriteArrays()                                */
/************************************************************************/

inline
bool OGRArrowWriterLayer::WriteArrays(
    std::function<bool(const std::shared_ptr<arrow::Field>&,
                       const std::shared_ptr<arrow::Array>&)> postProcessArray)
{
    int nArrowIdx = 0;
    int nArrowIdxFirstField = !m_osFIDColumn.empty() ? 1 : 0;
    for( const auto& poBuilder: m_apoBuilders )
    {
        const auto& field = m_poSchema->fields()[nArrowIdx];

        std::shared_ptr<arrow::Array> array;
        auto status = poBuilder->Finish(&array);
        if( !status.ok() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "builder::Finish() for field %s failed with %s",
                field->name().c_str(),
                status.message().c_str());
            return false;
        }

        // CPLDebug("ARROW", "%s", array->ToString().c_str());

        const int iCol = nArrowIdx - nArrowIdxFirstField;
        if( iCol >= 0 && iCol < m_poFeatureDefn->GetFieldCount() )
        {
            const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(iCol);
            const auto eFieldType = poFieldDefn->GetType();
            if( eFieldType == OFTInteger || eFieldType == OFTInteger64 )
            {
                const auto& osDomainName = poFieldDefn->GetDomainName();
                const auto oIter = m_oMapFieldDomainToStringArray.find(osDomainName);
                if( oIter != m_oMapFieldDomainToStringArray.end() )
                {
                    auto result = arrow::DictionaryArray::FromArrays(
                        field->type(),
                        array,
                        oIter->second);
                    if( !result.ok() )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                            "DictionaryArray::FromArrays() for field %s failed with %s",
                            field->name().c_str(),
                            result.status().message().c_str());
                        return false;
                    }
                    array = *result;
                }
            }
        }

        if( !postProcessArray(field, array) )
        {
            return false;
        }

        nArrowIdx ++;
    }
    return true;
}
