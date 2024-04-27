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

#ifndef OGARROWWRITERLAYER_HPP_INCLUDED
#define OGARROWWRITERLAYER_HPP_INCLUDED

#include "ogr_arrow.h"

#include "cpl_json.h"
#include "cpl_time.h"

#include "ogrlayerarrow.h"
#include "ogr_wkb.h"

#include <array>
#include <cinttypes>
#include <limits>

static constexpr int TZFLAG_UNINITIALIZED = -1;

#define OGR_ARROW_RETURN_NOT_OK(status, ret_value)                             \
    do                                                                         \
    {                                                                          \
        if (!(status).ok())                                                    \
        {                                                                      \
            CPLError(CE_Failure, CPLE_AppDefined, "%s failed",                 \
                     ARROW_STRINGIFY(status));                                 \
            return (ret_value);                                                \
        }                                                                      \
    } while (false)

#define OGR_ARROW_RETURN_FALSE_NOT_OK(status)                                  \
    OGR_ARROW_RETURN_NOT_OK(status, false)

#define OGR_ARROW_RETURN_OGRERR_NOT_OK(status)                                 \
    OGR_ARROW_RETURN_NOT_OK(status, OGRERR_FAILURE)

#define OGR_ARROW_PROPAGATE_OGRERR(ret_value)                                  \
    do                                                                         \
    {                                                                          \
        if ((ret_value) != OGRERR_NONE)                                        \
            return OGRERR_FAILURE;                                             \
    } while (0)

/************************************************************************/
/*                      OGRArrowWriterLayer()                           */
/************************************************************************/

inline OGRArrowWriterLayer::OGRArrowWriterLayer(
    arrow::MemoryPool *poMemoryPool,
    const std::shared_ptr<arrow::io::OutputStream> &poOutputStream,
    const char *pszLayerName)
    : m_poMemoryPool(poMemoryPool), m_poOutputStream(poOutputStream)
{
    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    SetDescription(pszLayerName);
}

/************************************************************************/
/*                     ~OGRArrowWriterLayer()                           */
/************************************************************************/

inline OGRArrowWriterLayer::~OGRArrowWriterLayer()
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

inline bool OGRArrowWriterLayer::FinalizeWriting()
{
    bool ret = true;

    if (!IsFileWriterCreated())
    {
        CreateWriter();
    }
    if (IsFileWriterCreated())
    {
        PerformStepsBeforeFinalFlushGroup();

        if (!m_apoBuilders.empty() && m_apoFieldsFromArrowSchema.empty())
            ret = FlushGroup();

        if (!CloseFileWriter())
            ret = false;
    }

    return ret;
}

/************************************************************************/
/*                       CreateSchemaCommon()                           */
/************************************************************************/

inline void OGRArrowWriterLayer::CreateSchemaCommon()
{
    CPLAssert(static_cast<int>(m_aeGeomEncoding.size()) ==
              m_poFeatureDefn->GetGeomFieldCount());

    std::vector<std::shared_ptr<arrow::Field>> fields;
    bool bNeedGDALSchema = false;

    m_anTZFlag.resize(m_poFeatureDefn->GetFieldCount(), TZFLAG_UNINITIALIZED);

    if (!m_osFIDColumn.empty())
    {
        bNeedGDALSchema = true;
        fields.emplace_back(arrow::field(m_osFIDColumn, arrow::int64(), false));
    }

    if (!m_apoFieldsFromArrowSchema.empty())
    {
        fields.insert(fields.end(), m_apoFieldsFromArrowSchema.begin(),
                      m_apoFieldsFromArrowSchema.end());
    }

    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        std::shared_ptr<arrow::DataType> dt;
        const auto eSubDT = poFieldDefn->GetSubType();
        const auto &osDomainName = poFieldDefn->GetDomainName();
        const OGRFieldDomain *poFieldDomain = nullptr;
        const int nWidth = poFieldDefn->GetWidth();
        if (!osDomainName.empty())
        {
            const auto oIter = m_oMapFieldDomains.find(osDomainName);
            if (oIter == m_oMapFieldDomains.end())
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Field %s references domain %s, but the later one "
                         "has not been created",
                         poFieldDefn->GetNameRef(), osDomainName.c_str());
            }
            else
            {
                poFieldDomain = oIter->second.get();
            }
        }
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
                if (eSubDT == OFSTBoolean)
                    dt = arrow::boolean();
                else if (eSubDT == OFSTInt16)
                    dt = arrow::int16();
                else
                    dt = arrow::int32();
                if (poFieldDomain != nullptr)
                {
                    dt = arrow::dictionary(dt, arrow::utf8());
                }
                break;

            case OFTInteger64:
                dt = arrow::int64();
                if (poFieldDomain != nullptr)
                {
                    dt = arrow::dictionary(dt, arrow::utf8());
                }
                break;

            case OFTReal:
            {
                const int nPrecision = poFieldDefn->GetPrecision();
                if (nWidth != 0 && nPrecision != 0)
                    dt = arrow::decimal(nWidth, nPrecision);
                else if (eSubDT == OFSTFloat32)
                    dt = arrow::float32();
                else
                    dt = arrow::float64();
                break;
            }

            case OFTString:
            case OFTWideString:
                if (eSubDT != OFSTNone || nWidth > 0)
                    bNeedGDALSchema = true;
                dt = arrow::utf8();
                break;

            case OFTBinary:
                if (nWidth != 0)
                    dt = arrow::fixed_size_binary(nWidth);
                else
                    dt = arrow::binary();
                break;

            case OFTIntegerList:
                if (eSubDT == OFSTBoolean)
                    dt = arrow::list(arrow::boolean());
                else if (eSubDT == OFSTInt16)
                    dt = arrow::list(arrow::int16());
                else
                    dt = arrow::list(arrow::int32());
                break;

            case OFTInteger64List:
                dt = arrow::list(arrow::int64());
                break;

            case OFTRealList:
                if (eSubDT == OFSTFloat32)
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
            {
                const int nTZFlag = poFieldDefn->GetTZFlag();
                if (nTZFlag >= OGR_TZFLAG_MIXED_TZ)
                {
                    m_anTZFlag[i] = nTZFlag;
                }
                dt = arrow::timestamp(arrow::TimeUnit::MILLI);
                break;
            }
        }
        fields.emplace_back(arrow::field(poFieldDefn->GetNameRef(),
                                         std::move(dt),
                                         poFieldDefn->IsNullable()));
        if (poFieldDefn->GetAlternativeNameRef()[0])
            bNeedGDALSchema = true;
        if (!poFieldDefn->GetComment().empty())
            bNeedGDALSchema = true;
    }

    for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
    {
        const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
        const auto eGType = poGeomFieldDefn->GetType();
        const int nDim =
            2 + (OGR_GT_HasZ(eGType) ? 1 : 0) + (OGR_GT_HasM(eGType) ? 1 : 0);

        const bool pointFieldNullable = GetDriverUCName() == "PARQUET";

        // Fixed Size List GeoArrow encoding
        std::shared_ptr<arrow::Field> pointField;
        if (nDim == 2)
            pointField =
                arrow::field("xy", arrow::float64(), pointFieldNullable);
        else if (nDim == 3 && OGR_GT_HasZ(eGType))
            pointField =
                arrow::field("xyz", arrow::float64(), pointFieldNullable);
        else if (nDim == 3 && OGR_GT_HasM(eGType))
            pointField =
                arrow::field("xym", arrow::float64(), pointFieldNullable);
        else
            pointField =
                arrow::field("xyzm", arrow::float64(), pointFieldNullable);

        // Struct GeoArrow encoding
        auto xField(arrow::field("x", arrow::float64(), false));
        auto yField(arrow::field("y", arrow::float64(), false));
        std::vector<std::shared_ptr<arrow::Field>> pointFields{
            arrow::field("x", arrow::float64(), false),
            arrow::field("y", arrow::float64(), false)};
        if (OGR_GT_HasZ(eGType))
            pointFields.emplace_back(
                arrow::field("z", arrow::float64(), false));
        if (OGR_GT_HasM(eGType))
            pointFields.emplace_back(
                arrow::field("m", arrow::float64(), false));
        auto pointStructType(arrow::struct_(std::move(pointFields)));

        std::shared_ptr<arrow::DataType> dt;
        switch (m_aeGeomEncoding[i])
        {
            case OGRArrowGeomEncoding::WKB:
                dt = arrow::binary();
                break;

            case OGRArrowGeomEncoding::WKT:
                dt = arrow::utf8();
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC:
            case OGRArrowGeomEncoding::GEOARROW_STRUCT_GENERIC:
                CPLAssert(false);
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_POINT:
                dt = arrow::fixed_size_list(pointField, nDim);
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_LINESTRING:
                dt = arrow::list(arrow::fixed_size_list(pointField, nDim));
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_POLYGON:
                dt = arrow::list(
                    arrow::list(arrow::fixed_size_list(pointField, nDim)));
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOINT:
                dt = arrow::list(arrow::fixed_size_list(pointField, nDim));
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_MULTILINESTRING:
                dt = arrow::list(
                    arrow::list(arrow::fixed_size_list(pointField, nDim)));
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOLYGON:
                dt = arrow::list(arrow::list(
                    arrow::list(arrow::fixed_size_list(pointField, nDim))));
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_POINT:
                dt = pointStructType;
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_LINESTRING:
                dt = arrow::list(pointStructType);
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_POLYGON:
                dt = arrow::list(arrow::list(pointStructType));
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOINT:
                dt = arrow::list(pointStructType);
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTILINESTRING:
                dt = arrow::list(arrow::list(pointStructType));
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOLYGON:
                dt = arrow::list(arrow::list(arrow::list(pointStructType)));
                break;
        }

        std::shared_ptr<arrow::Field> field(
            arrow::field(poGeomFieldDefn->GetNameRef(), std::move(dt),
                         poGeomFieldDefn->IsNullable()));
        if (m_bWriteFieldArrowExtensionName)
        {
            auto kvMetadata = field->metadata()
                                  ? field->metadata()->Copy()
                                  : std::make_shared<arrow::KeyValueMetadata>();
            kvMetadata->Append(
                "ARROW:extension:name",
                GetGeomEncodingAsString(m_aeGeomEncoding[i], false));
            field = field->WithMetadata(kvMetadata);
        }

        m_apoBaseStructGeomType.emplace_back(std::move(pointStructType));

        fields.emplace_back(std::move(field));
    }

    if (m_bWriteBBoxStruct)
    {
        for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
        {
            const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
            auto bbox_field_xmin(arrow::field("xmin", arrow::float32(), false));
            auto bbox_field_ymin(arrow::field("ymin", arrow::float32(), false));
            auto bbox_field_xmax(arrow::field("xmax", arrow::float32(), false));
            auto bbox_field_ymax(arrow::field("ymax", arrow::float32(), false));
            auto bbox_field(arrow::field(
                CPLGetConfigOption("OGR_PARQUET_COVERING_BBOX_NAME",
                                   std::string(poGeomFieldDefn->GetNameRef())
                                       .append("_bbox")
                                       .c_str()),
                arrow::struct_(
                    {std::move(bbox_field_xmin), std::move(bbox_field_ymin),
                     std::move(bbox_field_xmax), std::move(bbox_field_ymax)}),
                poGeomFieldDefn->IsNullable()));
            fields.emplace_back(bbox_field);
            m_apoFieldsBBOX.emplace_back(bbox_field);
        }
    }

    m_aoEnvelopes.resize(m_poFeatureDefn->GetGeomFieldCount());
    m_oSetWrittenGeometryTypes.resize(m_poFeatureDefn->GetGeomFieldCount());

    m_poSchema = arrow::schema(std::move(fields));
    CPLAssert(m_poSchema);
    if (bNeedGDALSchema &&
        CPLTestBool(CPLGetConfigOption(
            ("OGR_" + GetDriverUCName() + "_WRITE_GDAL_SCHEMA").c_str(),
            "YES")))
    {
        CPLJSONObject oRoot;
        CPLJSONObject oColumns;

        if (!m_osFIDColumn.empty())
            oRoot.Add("fid", m_osFIDColumn);

        oRoot.Add("columns", oColumns);
        for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
        {
            const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            CPLJSONObject oColumn;
            oColumns.Add(poFieldDefn->GetNameRef(), oColumn);
            oColumn.Add("type", OGR_GetFieldTypeName(poFieldDefn->GetType()));
            const auto eSubDT = poFieldDefn->GetSubType();
            if (eSubDT != OFSTNone)
                oColumn.Add("subtype", OGR_GetFieldSubTypeName(eSubDT));
            const int nWidth = poFieldDefn->GetWidth();
            if (nWidth > 0)
                oColumn.Add("width", nWidth);
            const int nPrecision = poFieldDefn->GetPrecision();
            if (nPrecision > 0)
                oColumn.Add("precision", nPrecision);
            if (poFieldDefn->GetAlternativeNameRef()[0])
                oColumn.Add("alternative_name",
                            poFieldDefn->GetAlternativeNameRef());
            if (!poFieldDefn->GetComment().empty())
                oColumn.Add("comment", poFieldDefn->GetComment());
        }

        auto kvMetadata = m_poSchema->metadata()
                              ? m_poSchema->metadata()->Copy()
                              : std::make_shared<arrow::KeyValueMetadata>();
        kvMetadata->Append("gdal:schema",
                           oRoot.Format(CPLJSONObject::PrettyFormat::Plain));
        m_poSchema = m_poSchema->WithMetadata(kvMetadata);
        CPLAssert(m_poSchema);
    }
}

/************************************************************************/
/*                         FinalizeSchema()                             */
/************************************************************************/

inline void OGRArrowWriterLayer::FinalizeSchema()
{
    // Final tuning of schema taking into actual timezone values
    // from features
    int nArrowIdxFirstField = !m_osFIDColumn.empty() ? 1 : 0;
    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
    {
        if (m_anTZFlag[i] >= OGR_TZFLAG_MIXED_TZ)
        {
            const int nOffset = m_anTZFlag[i] == OGR_TZFLAG_UTC
                                    ? 0
                                    : (m_anTZFlag[i] - OGR_TZFLAG_UTC) * 15;
            int nHours = static_cast<int>(nOffset / 60);  // Round towards zero.
            const int nMinutes = std::abs(nOffset - nHours * 60);

            const std::string osTZ =
                CPLSPrintf("%c%02d:%02d", nOffset >= 0 ? '+' : '-',
                           std::abs(nHours), nMinutes);
            auto dt = arrow::timestamp(arrow::TimeUnit::MILLI, osTZ);
            const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            auto field = arrow::field(poFieldDefn->GetNameRef(), std::move(dt),
                                      poFieldDefn->IsNullable());
            auto result = m_poSchema->SetField(nArrowIdxFirstField + i, field);
            if (!result.ok())
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

inline bool
OGRArrowWriterLayer::AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                                    std::string &failureReason)
{
    if (domain->GetDomainType() != OFDT_CODED)
    {
        failureReason = "Only coded field domains are supported by Arrow";
        return false;
    }

    const OGRCodedFieldDomain *poDomain =
        static_cast<const OGRCodedFieldDomain *>(domain.get());
    const OGRCodedValue *psIter = poDomain->GetEnumeration();

    auto poStringBuilder =
        std::make_shared<arrow::StringBuilder>(m_poMemoryPool);

    int nLastCode = -1;
    for (; psIter->pszCode; ++psIter)
    {
        if (CPLGetValueType(psIter->pszCode) != CPL_VALUE_INTEGER)
        {
            failureReason = "Non integer code in domain ";
            failureReason += domain->GetName();
            return false;
        }
        int nCode = atoi(psIter->pszCode);
        if (nCode <= nLastCode || nCode - nLastCode > 100)
        {
            failureReason = "Too sparse codes in domain ";
            failureReason += domain->GetName();
            return false;
        }
        for (int i = nLastCode + 1; i < nCode; ++i)
        {
            OGR_ARROW_RETURN_FALSE_NOT_OK(poStringBuilder->AppendNull());
        }
        if (psIter->pszValue)
            OGR_ARROW_RETURN_FALSE_NOT_OK(
                poStringBuilder->Append(psIter->pszValue));
        else
            OGR_ARROW_RETURN_FALSE_NOT_OK(poStringBuilder->AppendNull());
        nLastCode = nCode;
    }

    std::shared_ptr<arrow::Array> stringArray;
    auto status = poStringBuilder->Finish(&stringArray);
    if (!status.ok())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "StringArray::Finish() failed with %s",
                 status.message().c_str());
        return false;
    }

    m_oMapFieldDomainToStringArray[domain->GetName()] = std::move(stringArray);
    m_oMapFieldDomains[domain->GetName()] = std::move(domain);
    return true;
}

/************************************************************************/
/*                          GetFieldDomainNames()                       */
/************************************************************************/

inline std::vector<std::string> OGRArrowWriterLayer::GetFieldDomainNames() const
{
    std::vector<std::string> names;
    names.reserve(m_oMapFieldDomains.size());
    for (const auto &it : m_oMapFieldDomains)
    {
        names.emplace_back(it.first);
    }
    return names;
}

/************************************************************************/
/*                          GetFieldDomain()                            */
/************************************************************************/

inline const OGRFieldDomain *
OGRArrowWriterLayer::GetFieldDomain(const std::string &name) const
{
    const auto iter = m_oMapFieldDomains.find(name);
    if (iter == m_oMapFieldDomains.end())
        return nullptr;
    return iter->second.get();
}

/************************************************************************/
/*                          CreateField()                               */
/************************************************************************/

inline OGRErr OGRArrowWriterLayer::CreateField(const OGRFieldDefn *poField,
                                               int /* bApproxOK */)
{
    if (m_poSchema)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot add field after a first feature has been written");
        return OGRERR_FAILURE;
    }
    if (!m_apoFieldsFromArrowSchema.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot mix calls to CreateField() and "
                 "CreateFieldFromArrowSchema()");
        return OGRERR_FAILURE;
    }
    m_poFeatureDefn->AddFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                OGRLayer::CreateFieldFromArrowSchema()                */
/************************************************************************/

inline bool OGRArrowWriterLayer::CreateFieldFromArrowSchema(
    const struct ArrowSchema *schema, CSLConstList /*papszOptions*/)
{
    if (m_poSchema)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot add field after a first feature has been written");
        return false;
    }

    if (m_poFeatureDefn->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot mix calls to CreateField() and "
                 "CreateFieldFromArrowSchema()");
        return false;
    }

    if (m_osFIDColumn == schema->name)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FID column has the same name as this field: %s",
                 schema->name);
        return false;
    }

    for (auto &apoField : m_apoFieldsFromArrowSchema)
    {
        if (apoField->name() == schema->name)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Field of name %s already exists", schema->name);
            return false;
        }
    }

    if (m_poFeatureDefn->GetGeomFieldIndex(schema->name) >= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Geometry field of name %s already exists", schema->name);
        return false;
    }

    // ImportField() would release the schema, but we don't want that
    // So copy the structure content into a local variable, and override its
    // release callback to a no-op. This may be a bit fragile, but it doesn't
    // look like ImportField implementation tries to access the C ArrowSchema
    // after it has been called.
    struct ArrowSchema lSchema = *schema;
    const auto DummyFreeSchema = [](struct ArrowSchema *ptrSchema)
    { ptrSchema->release = nullptr; };
    lSchema.release = DummyFreeSchema;
    auto result = arrow::ImportField(&lSchema);
    CPLAssert(lSchema.release == nullptr);
    if (!result.ok())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateFieldFromArrowSchema() failed");
        return false;
    }
    m_apoFieldsFromArrowSchema.emplace_back(std::move(*result));
    return true;
}

/************************************************************************/
/*                   GetPreciseArrowGeomEncoding()                      */
/************************************************************************/

inline OGRArrowGeomEncoding OGRArrowWriterLayer::GetPreciseArrowGeomEncoding(
    OGRArrowGeomEncoding eEncodingType, OGRwkbGeometryType eGType)
{
    CPLAssert(eEncodingType == OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC ||
              eEncodingType == OGRArrowGeomEncoding::GEOARROW_STRUCT_GENERIC);
    const auto eFlatType = wkbFlatten(eGType);
    if (eFlatType == wkbPoint)
    {
        return eEncodingType == OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC
                   ? OGRArrowGeomEncoding::GEOARROW_FSL_POINT
                   : OGRArrowGeomEncoding::GEOARROW_STRUCT_POINT;
    }
    else if (eFlatType == wkbLineString)
    {
        return eEncodingType == OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC
                   ? OGRArrowGeomEncoding::GEOARROW_FSL_LINESTRING
                   : OGRArrowGeomEncoding::GEOARROW_STRUCT_LINESTRING;
    }
    else if (eFlatType == wkbPolygon)
    {
        return eEncodingType == OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC
                   ? OGRArrowGeomEncoding::GEOARROW_FSL_POLYGON
                   : OGRArrowGeomEncoding::GEOARROW_STRUCT_POLYGON;
    }
    else if (eFlatType == wkbMultiPoint)
    {
        return eEncodingType == OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC
                   ? OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOINT
                   : OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOINT;
    }
    else if (eFlatType == wkbMultiLineString)
    {
        return eEncodingType == OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC
                   ? OGRArrowGeomEncoding::GEOARROW_FSL_MULTILINESTRING
                   : OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTILINESTRING;
    }
    else if (eFlatType == wkbMultiPolygon)
    {
        return eEncodingType == OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC
                   ? OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOLYGON
                   : OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOLYGON;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoArrow encoding is currently not supported for %s",
                 OGRGeometryTypeToName(eGType));
        return eEncodingType;
    }
}

/************************************************************************/
/*                        GetGeomEncodingAsString()                     */
/************************************************************************/

inline const char *
OGRArrowWriterLayer::GetGeomEncodingAsString(OGRArrowGeomEncoding eGeomEncoding,
                                             bool bForParquetGeo)
{
    switch (eGeomEncoding)
    {
        case OGRArrowGeomEncoding::WKB:
            return bForParquetGeo ? "WKB" : "geoarrow.wkb";
        case OGRArrowGeomEncoding::WKT:
            return bForParquetGeo ? "WKT" : "geoarrow.wkt";
        case OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC:
        case OGRArrowGeomEncoding::GEOARROW_STRUCT_GENERIC:
            CPLAssert(false);
            break;
        case OGRArrowGeomEncoding::GEOARROW_FSL_POINT:
            return "geoarrow.point";
        case OGRArrowGeomEncoding::GEOARROW_FSL_LINESTRING:
            return "geoarrow.linestring";
        case OGRArrowGeomEncoding::GEOARROW_FSL_POLYGON:
            return "geoarrow.polygon";
        case OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOINT:
            return "geoarrow.multipoint";
        case OGRArrowGeomEncoding::GEOARROW_FSL_MULTILINESTRING:
            return "geoarrow.multilinestring";
        case OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOLYGON:
            return "geoarrow.multipolygon";
        case OGRArrowGeomEncoding::GEOARROW_STRUCT_POINT:
            return bForParquetGeo ? "point" : "geoarrow.point";
        case OGRArrowGeomEncoding::GEOARROW_STRUCT_LINESTRING:
            return bForParquetGeo ? "linestring" : "geoarrow.linestring";
        case OGRArrowGeomEncoding::GEOARROW_STRUCT_POLYGON:
            return bForParquetGeo ? "polygon" : "geoarrow.polygon";
        case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOINT:
            return bForParquetGeo ? "multipoint" : "geoarrow.multipoint";
        case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTILINESTRING:
            return bForParquetGeo ? "multilinestring"
                                  : "geoarrow.multilinestring";
        case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOLYGON:
            return bForParquetGeo ? "multipolygon" : "geoarrow.multipolygon";
    }
    return nullptr;
}

/************************************************************************/
/*                          CreateGeomField()                           */
/************************************************************************/

inline OGRErr
OGRArrowWriterLayer::CreateGeomField(const OGRGeomFieldDefn *poField,
                                     int /* bApproxOK */)
{
    if (m_poSchema)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot add field after a first feature has been written");
        return OGRERR_FAILURE;
    }
    const auto eGType = poField->GetType();
    if (!IsSupportedGeometryType(eGType))
    {
        return OGRERR_FAILURE;
    }

    if (IsSRSRequired() && poField->GetSpatialRef() == nullptr)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Geometry column should have an associated CRS");
    }
    auto eGeomEncoding = m_eGeomEncoding;
    if (eGeomEncoding == OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC ||
        eGeomEncoding == OGRArrowGeomEncoding::GEOARROW_STRUCT_GENERIC)
    {
        const auto eEncodingType = eGeomEncoding;
        eGeomEncoding = GetPreciseArrowGeomEncoding(eEncodingType, eGType);
        if (eGeomEncoding == eEncodingType)
            return OGRERR_FAILURE;
    }
    m_aeGeomEncoding.push_back(eGeomEncoding);
    m_poFeatureDefn->AddGeomFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                        MakeGeoArrowBuilder()                         */
/************************************************************************/

static std::shared_ptr<arrow::ArrayBuilder>
MakeGeoArrowBuilder(arrow::MemoryPool *poMemoryPool, int nDim, int nDepth)
{
    if (nDepth == 0)
        return std::make_shared<arrow::FixedSizeListBuilder>(
            poMemoryPool, std::make_shared<arrow::DoubleBuilder>(poMemoryPool),
            nDim);
    else
        return std::make_shared<arrow::ListBuilder>(
            poMemoryPool, MakeGeoArrowBuilder(poMemoryPool, nDim, nDepth - 1));
}

/************************************************************************/
/*                      MakeGeoArrowStructBuilder()                     */
/************************************************************************/

static std::shared_ptr<arrow::ArrayBuilder>
MakeGeoArrowStructBuilder(arrow::MemoryPool *poMemoryPool, int nDim, int nDepth,
                          const std::shared_ptr<arrow::DataType> &eBaseType)
{
    if (nDepth == 0)
    {
        std::vector<std::shared_ptr<arrow::ArrayBuilder>> builders;
        for (int i = 0; i < nDim; ++i)
            builders.emplace_back(
                std::make_shared<arrow::DoubleBuilder>(poMemoryPool));
        return std::make_shared<arrow::StructBuilder>(eBaseType, poMemoryPool,
                                                      std::move(builders));
    }
    else
        return std::make_shared<arrow::ListBuilder>(
            poMemoryPool, MakeGeoArrowStructBuilder(poMemoryPool, nDim,
                                                    nDepth - 1, eBaseType));
}

/************************************************************************/
/*                         ClearArrayBuilers()                          */
/************************************************************************/

inline void OGRArrowWriterLayer::ClearArrayBuilers()
{
    m_apoBuilders.clear();
    m_apoBuildersBBOXStruct.clear();
    m_apoBuildersBBOXXMin.clear();
    m_apoBuildersBBOXYMin.clear();
    m_apoBuildersBBOXXMax.clear();
    m_apoBuildersBBOXYMax.clear();
}

/************************************************************************/
/*                        CreateArrayBuilders()                         */
/************************************************************************/

inline void OGRArrowWriterLayer::CreateArrayBuilders()
{
    m_apoBuilders.reserve(1 + m_poFeatureDefn->GetFieldCount() +
                          m_poFeatureDefn->GetGeomFieldCount());

    int nArrowIdx = 0;
    if (!m_osFIDColumn.empty())
    {
        m_apoBuilders.emplace_back(std::make_shared<arrow::Int64Builder>());
        nArrowIdx++;
    }

    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i, ++nArrowIdx)
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        std::shared_ptr<arrow::ArrayBuilder> builder;
        const auto eSubDT = poFieldDefn->GetSubType();
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
                if (eSubDT == OFSTBoolean)
                    builder =
                        std::make_shared<arrow::BooleanBuilder>(m_poMemoryPool);
                else if (eSubDT == OFSTInt16)
                    builder =
                        std::make_shared<arrow::Int16Builder>(m_poMemoryPool);
                else
                    builder =
                        std::make_shared<arrow::Int32Builder>(m_poMemoryPool);
                break;

            case OFTInteger64:
                builder = std::make_shared<arrow::Int64Builder>(m_poMemoryPool);
                break;

            case OFTReal:
            {
                const auto arrowType = m_poSchema->fields()[nArrowIdx]->type();
                if (arrowType->id() == arrow::Type::DECIMAL128)
                    builder = std::make_shared<arrow::Decimal128Builder>(
                        arrowType, m_poMemoryPool);
                else if (arrowType->id() == arrow::Type::DECIMAL256)
                    builder = std::make_shared<arrow::Decimal256Builder>(
                        arrowType, m_poMemoryPool);
                else if (eSubDT == OFSTFloat32)
                    builder =
                        std::make_shared<arrow::FloatBuilder>(m_poMemoryPool);
                else
                    builder =
                        std::make_shared<arrow::DoubleBuilder>(m_poMemoryPool);
                break;
            }

            case OFTString:
            case OFTWideString:
                builder =
                    std::make_shared<arrow::StringBuilder>(m_poMemoryPool);
                break;

            case OFTBinary:
                if (poFieldDefn->GetWidth() != 0)
                    builder = std::make_shared<arrow::FixedSizeBinaryBuilder>(
                        arrow::fixed_size_binary(poFieldDefn->GetWidth()),
                        m_poMemoryPool);
                else
                    builder =
                        std::make_shared<arrow::BinaryBuilder>(m_poMemoryPool);
                break;

            case OFTIntegerList:
            {
                std::shared_ptr<arrow::ArrayBuilder> poBaseBuilder;
                if (eSubDT == OFSTBoolean)
                    poBaseBuilder =
                        std::make_shared<arrow::BooleanBuilder>(m_poMemoryPool);
                else if (eSubDT == OFSTInt16)
                    poBaseBuilder =
                        std::make_shared<arrow::Int16Builder>(m_poMemoryPool);
                else
                    poBaseBuilder =
                        std::make_shared<arrow::Int32Builder>(m_poMemoryPool);
                builder = std::make_shared<arrow::ListBuilder>(m_poMemoryPool,
                                                               poBaseBuilder);
                break;
            }

            case OFTInteger64List:
                builder = std::make_shared<arrow::ListBuilder>(
                    m_poMemoryPool,
                    std::make_shared<arrow::Int64Builder>(m_poMemoryPool));

                break;

            case OFTRealList:
                if (eSubDT == OFSTFloat32)
                    builder = std::make_shared<arrow::ListBuilder>(
                        m_poMemoryPool,
                        std::make_shared<arrow::FloatBuilder>(m_poMemoryPool));
                else
                    builder = std::make_shared<arrow::ListBuilder>(
                        m_poMemoryPool,
                        std::make_shared<arrow::DoubleBuilder>(m_poMemoryPool));
                break;

            case OFTStringList:
            case OFTWideStringList:
                builder = std::make_shared<arrow::ListBuilder>(
                    m_poMemoryPool,
                    std::make_shared<arrow::StringBuilder>(m_poMemoryPool));

                break;

            case OFTDate:
                builder =
                    std::make_shared<arrow::Date32Builder>(m_poMemoryPool);
                break;

            case OFTTime:
                builder = std::make_shared<arrow::Time32Builder>(
                    arrow::time32(arrow::TimeUnit::MILLI), m_poMemoryPool);
                break;

            case OFTDateTime:
                builder = std::make_shared<arrow::TimestampBuilder>(
                    arrow::timestamp(arrow::TimeUnit::MILLI), m_poMemoryPool);
                break;
        }
        m_apoBuilders.emplace_back(builder);
    }

    for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i, ++nArrowIdx)
    {
        std::shared_ptr<arrow::ArrayBuilder> builder;
        const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
        const auto eGType = poGeomFieldDefn->GetType();
        const int nDim =
            2 + (OGR_GT_HasZ(eGType) ? 1 : 0) + (OGR_GT_HasM(eGType) ? 1 : 0);

        switch (m_aeGeomEncoding[i])
        {
            case OGRArrowGeomEncoding::WKB:
                builder =
                    std::make_shared<arrow::BinaryBuilder>(m_poMemoryPool);
                break;

            case OGRArrowGeomEncoding::WKT:
                builder =
                    std::make_shared<arrow::StringBuilder>(m_poMemoryPool);
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_POINT:
                builder = MakeGeoArrowBuilder(m_poMemoryPool, nDim, 0);
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_LINESTRING:
                builder = MakeGeoArrowBuilder(m_poMemoryPool, nDim, 1);
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_POLYGON:
                builder = MakeGeoArrowBuilder(m_poMemoryPool, nDim, 2);
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOINT:
                builder = MakeGeoArrowBuilder(m_poMemoryPool, nDim, 1);
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_MULTILINESTRING:
                builder = MakeGeoArrowBuilder(m_poMemoryPool, nDim, 2);
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOLYGON:
                builder = MakeGeoArrowBuilder(m_poMemoryPool, nDim, 3);
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_POINT:
                builder = MakeGeoArrowStructBuilder(m_poMemoryPool, nDim, 0,
                                                    m_apoBaseStructGeomType[i]);
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_LINESTRING:
                builder = MakeGeoArrowStructBuilder(m_poMemoryPool, nDim, 1,
                                                    m_apoBaseStructGeomType[i]);
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_POLYGON:
                builder = MakeGeoArrowStructBuilder(m_poMemoryPool, nDim, 2,
                                                    m_apoBaseStructGeomType[i]);
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOINT:
                builder = MakeGeoArrowStructBuilder(m_poMemoryPool, nDim, 1,
                                                    m_apoBaseStructGeomType[i]);
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTILINESTRING:
                builder = MakeGeoArrowStructBuilder(m_poMemoryPool, nDim, 2,
                                                    m_apoBaseStructGeomType[i]);
                break;

            case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOLYGON:
                builder = MakeGeoArrowStructBuilder(m_poMemoryPool, nDim, 3,
                                                    m_apoBaseStructGeomType[i]);
                break;

            case OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC:
            case OGRArrowGeomEncoding::GEOARROW_STRUCT_GENERIC:
                CPLAssert(false);
                break;
        }

        m_apoBuilders.emplace_back(builder);

        if (m_bWriteBBoxStruct)
        {
            m_apoBuildersBBOXXMin.emplace_back(
                std::make_shared<arrow::FloatBuilder>(m_poMemoryPool));
            m_apoBuildersBBOXYMin.emplace_back(
                std::make_shared<arrow::FloatBuilder>(m_poMemoryPool));
            m_apoBuildersBBOXXMax.emplace_back(
                std::make_shared<arrow::FloatBuilder>(m_poMemoryPool));
            m_apoBuildersBBOXYMax.emplace_back(
                std::make_shared<arrow::FloatBuilder>(m_poMemoryPool));
            m_apoBuildersBBOXStruct.emplace_back(
                std::make_shared<arrow::StructBuilder>(
                    m_apoFieldsBBOX[i]->type(), m_poMemoryPool,
                    std::vector<std::shared_ptr<arrow::ArrayBuilder>>{
                        m_apoBuildersBBOXXMin.back(),
                        m_apoBuildersBBOXYMin.back(),
                        m_apoBuildersBBOXXMax.back(),
                        m_apoBuildersBBOXYMax.back()}));
        }
    }
}

/************************************************************************/
/*                          castToFloatDown()                            */
/************************************************************************/

// Cf https://github.com/sqlite/sqlite/blob/90e4a3b7fcdf63035d6f35eb44d11ff58ff4b068/ext/rtree/rtree.c#L2993C1-L2995C3
/*
** Rounding constants for float->double conversion.
*/
#define RNDTOWARDS (1.0 - 1.0 / 8388608.0) /* Round towards zero */
#define RNDAWAY (1.0 + 1.0 / 8388608.0)    /* Round away from zero */

/*
** Convert an sqlite3_value into an RtreeValue (presumably a float)
** while taking care to round toward negative or positive, respectively.
*/
static float castToFloatDown(double d)
{
    float f = static_cast<float>(d);
    if (f > d)
    {
        f = static_cast<float>(d * (d < 0 ? RNDAWAY : RNDTOWARDS));
    }
    return f;
}

static float castToFloatUp(double d)
{
    float f = static_cast<float>(d);
    if (f < d)
    {
        f = static_cast<float>(d * (d < 0 ? RNDTOWARDS : RNDAWAY));
    }
    return f;
}

/************************************************************************/
/*                         GeoArrowLineBuilder()                        */
/************************************************************************/

template <class PointBuilderType>
static OGRErr GeoArrowLineBuilder(const OGRLineString *poLS,
                                  PointBuilderType *poPointBuilder,
                                  arrow::DoubleBuilder *poXBuilder,
                                  arrow::DoubleBuilder *poYBuilder,
                                  arrow::DoubleBuilder *poZBuilder,
                                  arrow::DoubleBuilder *poMBuilder)
{
    for (int j = 0; j < poLS->getNumPoints(); ++j)
    {
        OGR_ARROW_RETURN_OGRERR_NOT_OK(poPointBuilder->Append());
        OGR_ARROW_RETURN_OGRERR_NOT_OK(poXBuilder->Append(poLS->getX(j)));
        OGR_ARROW_RETURN_OGRERR_NOT_OK(poYBuilder->Append(poLS->getY(j)));
        if (poZBuilder)
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poZBuilder->Append(poLS->getZ(j)));
        if (poMBuilder)
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poMBuilder->Append(poLS->getM(j)));
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                          BuildGeometry()                             */
/************************************************************************/

inline OGRErr OGRArrowWriterLayer::BuildGeometry(OGRGeometry *poGeom,
                                                 int iGeomField,
                                                 arrow::ArrayBuilder *poBuilder)
{
    const auto eGType = poGeom ? poGeom->getGeometryType() : wkbNone;
    const auto eColumnGType =
        m_poFeatureDefn->GetGeomFieldDefn(iGeomField)->GetType();
    const bool bHasZ = CPL_TO_BOOL(OGR_GT_HasZ(eColumnGType));
    const bool bHasM = CPL_TO_BOOL(OGR_GT_HasM(eColumnGType));
    const bool bIsEmpty = poGeom != nullptr && poGeom->IsEmpty();
    OGREnvelope3D oEnvelope;
    if (poGeom != nullptr && !bIsEmpty)
    {
        if (poGeom->Is3D())
        {
            poGeom->getEnvelope(&oEnvelope);
            m_aoEnvelopes[iGeomField].Merge(oEnvelope);
        }
        else
        {
            poGeom->getEnvelope(static_cast<OGREnvelope *>(&oEnvelope));
            m_aoEnvelopes[iGeomField].Merge(oEnvelope);
        }
        m_oSetWrittenGeometryTypes[iGeomField].insert(eGType);
    }

    if (m_bWriteBBoxStruct)
    {
        if (poGeom && !bIsEmpty)
        {
            OGR_ARROW_RETURN_OGRERR_NOT_OK(
                m_apoBuildersBBOXXMin[iGeomField]->Append(
                    castToFloatDown(oEnvelope.MinX)));
            OGR_ARROW_RETURN_OGRERR_NOT_OK(
                m_apoBuildersBBOXYMin[iGeomField]->Append(
                    castToFloatDown(oEnvelope.MinY)));
            OGR_ARROW_RETURN_OGRERR_NOT_OK(
                m_apoBuildersBBOXXMax[iGeomField]->Append(
                    castToFloatUp(oEnvelope.MaxX)));
            OGR_ARROW_RETURN_OGRERR_NOT_OK(
                m_apoBuildersBBOXYMax[iGeomField]->Append(
                    castToFloatUp(oEnvelope.MaxY)));
            OGR_ARROW_RETURN_OGRERR_NOT_OK(
                m_apoBuildersBBOXStruct[iGeomField]->Append());
        }
        else
        {
            OGR_ARROW_RETURN_OGRERR_NOT_OK(
                m_apoBuildersBBOXStruct[iGeomField]->AppendNull());
        }
    }

    if (poGeom == nullptr)
    {
        if (m_aeGeomEncoding[iGeomField] ==
                OGRArrowGeomEncoding::GEOARROW_FSL_POINT &&
            GetDriverUCName() == "PARQUET")
        {
            // For some reason, Parquet doesn't support a NULL FixedSizeList
            // on reading
            auto poPointBuilder =
                static_cast<arrow::FixedSizeListBuilder *>(poBuilder);
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poPointBuilder->Append());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder *>(
                poPointBuilder->value_builder());
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                std::numeric_limits<double>::quiet_NaN()));
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                std::numeric_limits<double>::quiet_NaN()));
            if (bHasZ)
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                    std::numeric_limits<double>::quiet_NaN()));
            if (bHasM)
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                    std::numeric_limits<double>::quiet_NaN()));
        }
        else
        {
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poBuilder->AppendNull());
        }

        return OGRERR_NONE;
    }

    // The following checks are only valid for GeoArrow encoding
    if (m_aeGeomEncoding[iGeomField] != OGRArrowGeomEncoding::WKB &&
        m_aeGeomEncoding[iGeomField] != OGRArrowGeomEncoding::WKT)
    {
        if ((!bIsEmpty && eGType != eColumnGType) ||
            (bIsEmpty && wkbFlatten(eGType) != wkbFlatten(eColumnGType)))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry of type %s found, whereas %s is expected. "
                     "Writing null geometry",
                     OGRGeometryTypeToName(eGType),
                     OGRGeometryTypeToName(eColumnGType));
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poBuilder->AppendNull());

            return OGRERR_NONE;
        }
    }

    switch (m_aeGeomEncoding[iGeomField])
    {
        case OGRArrowGeomEncoding::WKB:
        {
            std::unique_ptr<OGRGeometry> poGeomModified;
            if (OGR_GT_HasM(eGType) && !OGR_GT_HasM(eColumnGType))
            {
                static bool bHasWarned = false;
                if (!bHasWarned)
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
            if (nSize < INT_MAX)
            {
                m_abyBuffer.resize(nSize);
                poGeom->exportToWkb(wkbNDR, &m_abyBuffer[0], wkbVariantIso);
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    static_cast<arrow::BinaryBuilder *>(poBuilder)->Append(
                        m_abyBuffer.data(),
                        static_cast<int>(m_abyBuffer.size())));
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Too big geometry. "
                         "Writing null geometry");
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poBuilder->AppendNull());
            }
            break;
        }

        case OGRArrowGeomEncoding::WKT:
        {
            OGRWktOptions options;
            options.variant = wkbVariantIso;
            if (m_nWKTCoordinatePrecision >= 0)
            {
                options.format = OGRWktFormat::F;
                options.xyPrecision = m_nWKTCoordinatePrecision;
                options.zPrecision = m_nWKTCoordinatePrecision;
                options.mPrecision = m_nWKTCoordinatePrecision;
            }
            OGR_ARROW_RETURN_OGRERR_NOT_OK(
                static_cast<arrow::StringBuilder *>(poBuilder)->Append(
                    poGeom->exportToWkt(options)));
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_FSL_POINT:
        {
            const auto poPoint = poGeom->toPoint();
            auto poPointBuilder =
                static_cast<arrow::FixedSizeListBuilder *>(poBuilder);
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poPointBuilder->Append());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder *>(
                poPointBuilder->value_builder());
            if (bIsEmpty)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                    std::numeric_limits<double>::quiet_NaN()));
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                    std::numeric_limits<double>::quiet_NaN()));
                if (bHasZ)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                        std::numeric_limits<double>::quiet_NaN()));
                if (bHasM)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                        std::numeric_limits<double>::quiet_NaN()));
            }
            else
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    poValueBuilder->Append(poPoint->getX()));
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    poValueBuilder->Append(poPoint->getY()));
                if (bHasZ)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        poValueBuilder->Append(poPoint->getZ()));
                if (bHasM)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        poValueBuilder->Append(poPoint->getM()));
            }
            break;
        }

#define GET_XYZM_STRUCT_FIELD_BUILDERS_FROM(poPointBuilder)                    \
    auto poXBuilder =                                                          \
        static_cast<arrow::DoubleBuilder *>(poPointBuilder->field_builder(0)); \
    auto poYBuilder =                                                          \
        static_cast<arrow::DoubleBuilder *>(poPointBuilder->field_builder(1)); \
    int iSubField = 2;                                                         \
    arrow::DoubleBuilder *poZBuilder = nullptr;                                \
    if (bHasZ)                                                                 \
    {                                                                          \
        poZBuilder = static_cast<arrow::DoubleBuilder *>(                      \
            poPointBuilder->field_builder(iSubField));                         \
        ++iSubField;                                                           \
    }                                                                          \
    arrow::DoubleBuilder *poMBuilder = nullptr;                                \
    if (bHasM)                                                                 \
    {                                                                          \
        poMBuilder = static_cast<arrow::DoubleBuilder *>(                      \
            poPointBuilder->field_builder(iSubField));                         \
    }                                                                          \
    do                                                                         \
    {                                                                          \
    } while (0)

        case OGRArrowGeomEncoding::GEOARROW_STRUCT_POINT:
        {
            const auto poPoint = poGeom->toPoint();
            auto poPointBuilder =
                static_cast<arrow::StructBuilder *>(poBuilder);
            GET_XYZM_STRUCT_FIELD_BUILDERS_FROM(poPointBuilder);
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poPointBuilder->Append());

            if (bIsEmpty)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poXBuilder->Append(
                    std::numeric_limits<double>::quiet_NaN()));
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poYBuilder->Append(
                    std::numeric_limits<double>::quiet_NaN()));
            }
            else
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    poXBuilder->Append(poPoint->getX()));
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    poYBuilder->Append(poPoint->getY()));
            }
            if (poZBuilder)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poZBuilder->Append(
                    bIsEmpty ? std::numeric_limits<double>::quiet_NaN()
                             : poPoint->getZ()));
            }
            if (poMBuilder)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poMBuilder->Append(
                    bIsEmpty ? std::numeric_limits<double>::quiet_NaN()
                             : poPoint->getM()));
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_FSL_LINESTRING:
        {
            const auto poLS = poGeom->toLineString();
            auto poListBuilder = static_cast<arrow::ListBuilder *>(poBuilder);
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder *>(
                poListBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder *>(
                poPointBuilder->value_builder());

            OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
            OGR_ARROW_PROPAGATE_OGRERR(GeoArrowLineBuilder(
                poLS, poPointBuilder, poValueBuilder, poValueBuilder,
                bHasZ ? poValueBuilder : nullptr,
                bHasM ? poValueBuilder : nullptr));
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_STRUCT_LINESTRING:
        {
            const auto poLS = poGeom->toLineString();
            auto poListBuilder = static_cast<arrow::ListBuilder *>(poBuilder);
            auto poPointBuilder = static_cast<arrow::StructBuilder *>(
                poListBuilder->value_builder());
            GET_XYZM_STRUCT_FIELD_BUILDERS_FROM(poPointBuilder);

            OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
            OGR_ARROW_PROPAGATE_OGRERR(
                GeoArrowLineBuilder(poLS, poPointBuilder, poXBuilder,
                                    poYBuilder, poZBuilder, poMBuilder));
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_FSL_POLYGON:
        {
            const auto poPolygon = poGeom->toPolygon();
            auto poPolygonBuilder =
                static_cast<arrow::ListBuilder *>(poBuilder);
            auto poRingBuilder = static_cast<arrow::ListBuilder *>(
                poPolygonBuilder->value_builder());
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder *>(
                poRingBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder *>(
                poPointBuilder->value_builder());
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poPolygonBuilder->Append());
            for (const auto *poRing : *poPolygon)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poRingBuilder->Append());
                OGR_ARROW_PROPAGATE_OGRERR(GeoArrowLineBuilder(
                    poRing, poPointBuilder, poValueBuilder, poValueBuilder,
                    bHasZ ? poValueBuilder : nullptr,
                    bHasM ? poValueBuilder : nullptr));
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_STRUCT_POLYGON:
        {
            const auto poPolygon = poGeom->toPolygon();
            auto poPolygonBuilder =
                static_cast<arrow::ListBuilder *>(poBuilder);
            auto poRingBuilder = static_cast<arrow::ListBuilder *>(
                poPolygonBuilder->value_builder());
            auto poPointBuilder = static_cast<arrow::StructBuilder *>(
                poRingBuilder->value_builder());
            GET_XYZM_STRUCT_FIELD_BUILDERS_FROM(poPointBuilder);

            OGR_ARROW_RETURN_OGRERR_NOT_OK(poPolygonBuilder->Append());
            for (const auto *poRing : *poPolygon)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poRingBuilder->Append());
                OGR_ARROW_PROPAGATE_OGRERR(
                    GeoArrowLineBuilder(poRing, poPointBuilder, poXBuilder,
                                        poYBuilder, poZBuilder, poMBuilder));
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOINT:
        {
            const auto poMultiPoint = poGeom->toMultiPoint();
            auto poListBuilder = static_cast<arrow::ListBuilder *>(poBuilder);
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder *>(
                poListBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder *>(
                poPointBuilder->value_builder());
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
            for (const auto *poPoint : *poMultiPoint)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poPointBuilder->Append());
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    poValueBuilder->Append(poPoint->getX()));
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    poValueBuilder->Append(poPoint->getY()));
                if (bHasZ)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        poValueBuilder->Append(poPoint->getZ()));
                if (bHasM)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        poValueBuilder->Append(poPoint->getM()));
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOINT:
        {
            const auto poMultiPoint = poGeom->toMultiPoint();
            auto poListBuilder = static_cast<arrow::ListBuilder *>(poBuilder);
            auto poPointBuilder = static_cast<arrow::StructBuilder *>(
                poListBuilder->value_builder());
            GET_XYZM_STRUCT_FIELD_BUILDERS_FROM(poPointBuilder);

            OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
            for (const auto *poPoint : *poMultiPoint)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poPointBuilder->Append());
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    poXBuilder->Append(poPoint->getX()));
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    poYBuilder->Append(poPoint->getY()));
                if (poZBuilder)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        poZBuilder->Append(poPoint->getZ()));
                if (poMBuilder)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        poMBuilder->Append(poPoint->getM()));
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_FSL_MULTILINESTRING:
        {
            const auto poMLS = poGeom->toMultiLineString();
            auto poMLSBuilder = static_cast<arrow::ListBuilder *>(poBuilder);
            auto poLSBuilder = static_cast<arrow::ListBuilder *>(
                poMLSBuilder->value_builder());
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder *>(
                poLSBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder *>(
                poPointBuilder->value_builder());
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poMLSBuilder->Append());
            for (const auto *poLS : *poMLS)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poLSBuilder->Append());
                OGR_ARROW_PROPAGATE_OGRERR(GeoArrowLineBuilder(
                    poLS, poPointBuilder, poValueBuilder, poValueBuilder,
                    bHasZ ? poValueBuilder : nullptr,
                    bHasM ? poValueBuilder : nullptr));
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTILINESTRING:
        {
            const auto poMLS = poGeom->toMultiLineString();
            auto poMLSBuilder = static_cast<arrow::ListBuilder *>(poBuilder);
            auto poLSBuilder = static_cast<arrow::ListBuilder *>(
                poMLSBuilder->value_builder());
            auto poPointBuilder = static_cast<arrow::StructBuilder *>(
                poLSBuilder->value_builder());
            GET_XYZM_STRUCT_FIELD_BUILDERS_FROM(poPointBuilder);

            OGR_ARROW_RETURN_OGRERR_NOT_OK(poMLSBuilder->Append());
            for (const auto *poLS : *poMLS)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poLSBuilder->Append());
                OGR_ARROW_PROPAGATE_OGRERR(
                    GeoArrowLineBuilder(poLS, poPointBuilder, poXBuilder,
                                        poYBuilder, poZBuilder, poMBuilder));
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_FSL_MULTIPOLYGON:
        {
            const auto poMPoly = poGeom->toMultiPolygon();
            auto poMPolyBuilder = static_cast<arrow::ListBuilder *>(poBuilder);
            auto poPolyBuilder = static_cast<arrow::ListBuilder *>(
                poMPolyBuilder->value_builder());
            auto poRingBuilder = static_cast<arrow::ListBuilder *>(
                poPolyBuilder->value_builder());
            auto poPointBuilder = static_cast<arrow::FixedSizeListBuilder *>(
                poRingBuilder->value_builder());
            auto poValueBuilder = static_cast<arrow::DoubleBuilder *>(
                poPointBuilder->value_builder());
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poMPolyBuilder->Append());
            for (const auto *poPolygon : *poMPoly)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poPolyBuilder->Append());
                for (const auto *poRing : *poPolygon)
                {
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poRingBuilder->Append());
                    OGR_ARROW_PROPAGATE_OGRERR(GeoArrowLineBuilder(
                        poRing, poPointBuilder, poValueBuilder, poValueBuilder,
                        bHasZ ? poValueBuilder : nullptr,
                        bHasM ? poValueBuilder : nullptr));
                }
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOLYGON:
        {
            const auto poMPoly = poGeom->toMultiPolygon();
            auto poMPolyBuilder = static_cast<arrow::ListBuilder *>(poBuilder);
            auto poPolyBuilder = static_cast<arrow::ListBuilder *>(
                poMPolyBuilder->value_builder());
            auto poRingBuilder = static_cast<arrow::ListBuilder *>(
                poPolyBuilder->value_builder());
            auto poPointBuilder = static_cast<arrow::StructBuilder *>(
                poRingBuilder->value_builder());
            GET_XYZM_STRUCT_FIELD_BUILDERS_FROM(poPointBuilder);

            OGR_ARROW_RETURN_OGRERR_NOT_OK(poMPolyBuilder->Append());
            for (const auto *poPolygon : *poMPoly)
            {
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poPolyBuilder->Append());
                for (const auto *poRing : *poPolygon)
                {
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poRingBuilder->Append());
                    OGR_ARROW_PROPAGATE_OGRERR(GeoArrowLineBuilder(
                        poRing, poPointBuilder, poXBuilder, poYBuilder,
                        poZBuilder, poMBuilder));
                }
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC:
        case OGRArrowGeomEncoding::GEOARROW_STRUCT_GENERIC:
        {
            CPLAssert(false);
            break;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

inline OGRErr OGRArrowWriterLayer::ICreateFeature(OGRFeature *poFeature)
{
    if (m_poSchema == nullptr)
    {
        CreateSchema();
    }

    if (m_apoBuilders.empty())
    {
        if (!m_apoFieldsFromArrowSchema.empty())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "ICreateFeature() cannot be used after "
                     "CreateFieldFromArrowSchema()");
            return OGRERR_FAILURE;
        }
        CreateArrayBuilders();
    }

    // First pass to check not-null constraints as Arrow doesn't seem
    // to do that on the writing side. But such files can't be read.
    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for (int i = 0; i < nFieldCount; ++i)
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        if (!poFieldDefn->IsNullable() &&
            !poFeature->IsFieldSetAndNotNullUnsafe(i))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Null value found in non-nullable field %s",
                     poFieldDefn->GetNameRef());
            return OGRERR_FAILURE;
        }
    }

    const int nGeomFieldCount = m_poFeatureDefn->GetGeomFieldCount();
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
        if (!poGeomFieldDefn->IsNullable() &&
            poFeature->GetGeomFieldRef(i) == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Null value found in non-nullable geometry field %s",
                     poGeomFieldDefn->GetNameRef());
            return OGRERR_FAILURE;
        }
    }

    // Write FID, if FID column present
    int nArrowIdx = 0;
    if (!m_osFIDColumn.empty())
    {
        int64_t nFID = poFeature->GetFID();
        if (nFID == OGRNullFID)
        {
            nFID = m_nFeatureCount;
            poFeature->SetFID(nFID);
        }
        auto poBuilder =
            static_cast<arrow::Int64Builder *>(m_apoBuilders[0].get());
        OGR_ARROW_RETURN_OGRERR_NOT_OK(poBuilder->Append(nFID));
        nArrowIdx++;
    }

    // Write attributes
    for (int i = 0; i < nFieldCount; ++i, ++nArrowIdx)
    {
        auto poBuilder = m_apoBuilders[nArrowIdx].get();
        if (!poFeature->IsFieldSetAndNotNullUnsafe(i))
        {
            OGR_ARROW_RETURN_OGRERR_NOT_OK(poBuilder->AppendNull());
            continue;
        }

        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        const auto eSubDT = poFieldDefn->GetSubType();
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
                if (eSubDT == OFSTBoolean)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        static_cast<arrow::BooleanBuilder *>(poBuilder)->Append(
                            poFeature->GetFieldAsIntegerUnsafe(i) != 0));
                else if (eSubDT == OFSTInt16)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        static_cast<arrow::Int16Builder *>(poBuilder)->Append(
                            static_cast<int16_t>(
                                poFeature->GetFieldAsIntegerUnsafe(i))));
                else
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        static_cast<arrow::Int32Builder *>(poBuilder)->Append(
                            poFeature->GetFieldAsIntegerUnsafe(i)));
                break;

            case OFTInteger64:
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    static_cast<arrow::Int64Builder *>(poBuilder)->Append(
                        static_cast<int64_t>(
                            poFeature->GetFieldAsInteger64Unsafe(i))));
                break;

            case OFTReal:
            {
                const auto arrowType = m_poSchema->fields()[nArrowIdx]->type();
                const double dfVal = poFeature->GetFieldAsDoubleUnsafe(i);
                if (arrowType->id() == arrow::Type::DECIMAL128)
                {
                    auto res = arrow::Decimal128::FromReal(
                        dfVal, poFieldDefn->GetWidth(),
                        poFieldDefn->GetPrecision());
                    if (res.ok())
                    {
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(
                            static_cast<arrow::Decimal128Builder *>(poBuilder)
                                ->Append(*res));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot parse %.18g as a %d.%d decimal", dfVal,
                                 poFieldDefn->GetWidth(),
                                 poFieldDefn->GetPrecision());
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(poBuilder->AppendNull());
                    }
                }
                else if (arrowType->id() == arrow::Type::DECIMAL256)
                {
                    auto res = arrow::Decimal256::FromReal(
                        dfVal, poFieldDefn->GetWidth(),
                        poFieldDefn->GetPrecision());
                    if (res.ok())
                    {
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(
                            static_cast<arrow::Decimal256Builder *>(poBuilder)
                                ->Append(*res));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot parse %.18g as a %d.%d decimal", dfVal,
                                 poFieldDefn->GetWidth(),
                                 poFieldDefn->GetPrecision());
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(poBuilder->AppendNull());
                    }
                }
                else if (eSubDT == OFSTFloat32)
                {
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        static_cast<arrow::FloatBuilder *>(poBuilder)->Append(
                            static_cast<float>(dfVal)));
                }
                else
                {
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        static_cast<arrow::DoubleBuilder *>(poBuilder)->Append(
                            dfVal));
                }
                break;
            }

            case OFTString:
            case OFTWideString:
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    static_cast<arrow::StringBuilder *>(poBuilder)->Append(
                        poFeature->GetFieldAsStringUnsafe(i)));
                break;

            case OFTBinary:
            {
                int nSize = 0;
                const auto pData = poFeature->GetFieldAsBinary(i, &nSize);
                if (poFieldDefn->GetWidth() != 0)
                {
                    if (poFieldDefn->GetWidth() != nSize)
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Cannot write field %s. Got %d bytes, expected %d",
                            poFieldDefn->GetNameRef(), nSize,
                            poFieldDefn->GetWidth());
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(poBuilder->AppendNull());
                    }
                    else
                    {
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(
                            static_cast<arrow::FixedSizeBinaryBuilder *>(
                                poBuilder)
                                ->Append(pData));
                    }
                }
                else
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        static_cast<arrow::BinaryBuilder *>(poBuilder)->Append(
                            pData, nSize));
                break;
            }

            case OFTIntegerList:
            {
                auto poListBuilder =
                    static_cast<arrow::ListBuilder *>(poBuilder);
                if (eSubDT == OFSTBoolean)
                {
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
                    auto poValueBuilder = static_cast<arrow::BooleanBuilder *>(
                        poListBuilder->value_builder());
                    int nValues = 0;
                    const auto panValues =
                        poFeature->GetFieldAsIntegerList(i, &nValues);
                    for (int j = 0; j < nValues; ++j)
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(
                            poValueBuilder->Append(panValues[j] != 0));
                }
                else if (eSubDT == OFSTInt16)
                {
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
                    auto poValueBuilder = static_cast<arrow::Int16Builder *>(
                        poListBuilder->value_builder());
                    int nValues = 0;
                    const auto panValues =
                        poFeature->GetFieldAsIntegerList(i, &nValues);
                    for (int j = 0; j < nValues; ++j)
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                            static_cast<int16_t>(panValues[j])));
                }
                else
                {
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
                    auto poValueBuilder = static_cast<arrow::Int32Builder *>(
                        poListBuilder->value_builder());
                    int nValues = 0;
                    const auto panValues =
                        poFeature->GetFieldAsIntegerList(i, &nValues);
                    for (int j = 0; j < nValues; ++j)
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(
                            poValueBuilder->Append(panValues[j]));
                }
                break;
            }

            case OFTInteger64List:
            {
                auto poListBuilder =
                    static_cast<arrow::ListBuilder *>(poBuilder);
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
                auto poValueBuilder = static_cast<arrow::Int64Builder *>(
                    poListBuilder->value_builder());
                int nValues = 0;
                const auto panValues =
                    poFeature->GetFieldAsInteger64List(i, &nValues);
                for (int j = 0; j < nValues; ++j)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                        static_cast<int64_t>(panValues[j])));
                break;
            }

            case OFTRealList:
            {
                auto poListBuilder =
                    static_cast<arrow::ListBuilder *>(poBuilder);
                if (eSubDT == OFSTFloat32)
                {
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
                    auto poValueBuilder = static_cast<arrow::FloatBuilder *>(
                        poListBuilder->value_builder());
                    int nValues = 0;
                    const auto padfValues =
                        poFeature->GetFieldAsDoubleList(i, &nValues);
                    for (int j = 0; j < nValues; ++j)
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(poValueBuilder->Append(
                            static_cast<float>(padfValues[j])));
                }
                else
                {
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
                    auto poValueBuilder = static_cast<arrow::DoubleBuilder *>(
                        poListBuilder->value_builder());
                    int nValues = 0;
                    const auto padfValues =
                        poFeature->GetFieldAsDoubleList(i, &nValues);
                    for (int j = 0; j < nValues; ++j)
                        OGR_ARROW_RETURN_OGRERR_NOT_OK(
                            poValueBuilder->Append(padfValues[j]));
                }
                break;
            }

            case OFTStringList:
            case OFTWideStringList:
            {
                auto poListBuilder =
                    static_cast<arrow::ListBuilder *>(poBuilder);
                OGR_ARROW_RETURN_OGRERR_NOT_OK(poListBuilder->Append());
                auto poValueBuilder = static_cast<arrow::StringBuilder *>(
                    poListBuilder->value_builder());
                const auto papszValues = poFeature->GetFieldAsStringList(i);
                for (int j = 0; papszValues && papszValues[j]; ++j)
                    OGR_ARROW_RETURN_OGRERR_NOT_OK(
                        poValueBuilder->Append(papszValues[j]));
                break;
            }

            case OFTDate:
            {
                int nYear, nMonth, nDay, nHour, nMinute;
                float fSec;
                int nTZFlag;
                poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay, &nHour,
                                              &nMinute, &fSec, &nTZFlag);
                struct tm brokenDown;
                memset(&brokenDown, 0, sizeof(brokenDown));
                brokenDown.tm_year = nYear - 1900;
                brokenDown.tm_mon = nMonth - 1;
                brokenDown.tm_mday = nDay;
                GIntBig nVal = CPLYMDHMSToUnixTime(&brokenDown);
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    static_cast<arrow::Date32Builder *>(poBuilder)->Append(
                        static_cast<int>(nVal / 86400)));
                break;
            }

            case OFTTime:
            {
                int nYear, nMonth, nDay, nHour, nMinute;
                float fSec;
                int nTZFlag;
                poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay, &nHour,
                                              &nMinute, &fSec, &nTZFlag);
                int nVal = nHour * 3600 + nMinute * 60;
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    static_cast<arrow::Time32Builder *>(poBuilder)->Append(
                        static_cast<int>(
                            (static_cast<double>(nVal) + fSec) * 1000 + 0.5)));
                break;
            }

            case OFTDateTime:
            {
                int nYear, nMonth, nDay, nHour, nMinute;
                float fSec;
                int nTZFlag;
                poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay, &nHour,
                                              &nMinute, &fSec, &nTZFlag);
                struct tm brokenDown;
                memset(&brokenDown, 0, sizeof(brokenDown));
                brokenDown.tm_year = nYear - 1900;
                brokenDown.tm_mon = nMonth - 1;
                brokenDown.tm_mday = nDay;
                brokenDown.tm_hour = nHour;
                brokenDown.tm_min = nMinute;
                brokenDown.tm_sec = 0;
                GIntBig nVal = CPLYMDHMSToUnixTime(&brokenDown);
                if (!IsFileWriterCreated() &&
                    m_anTZFlag[i] != OGR_TZFLAG_UNKNOWN)
                {
                    if (m_anTZFlag[i] == TZFLAG_UNINITIALIZED)
                        m_anTZFlag[i] = nTZFlag;
                    else if (m_anTZFlag[i] != nTZFlag)
                    {
                        if (m_anTZFlag[i] >= OGR_TZFLAG_MIXED_TZ &&
                            nTZFlag >= OGR_TZFLAG_MIXED_TZ)
                        {
                            m_anTZFlag[i] =
                                OGR_TZFLAG_MIXED_TZ;  // harmonize on UTC ultimately
                        }
                        else
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Field %s contains a mix of "
                                     "timezone-aware and local/without "
                                     "timezone values.",
                                     poFieldDefn->GetNameRef());
                            m_anTZFlag[i] = OGR_TZFLAG_UNKNOWN;
                        }
                    }
                }
                if (nTZFlag > OGR_TZFLAG_MIXED_TZ)
                {
                    const int nOffsetSec = (nTZFlag - OGR_TZFLAG_UTC) * 15 * 60;
                    nVal -= nOffsetSec;
                }
                OGR_ARROW_RETURN_OGRERR_NOT_OK(
                    static_cast<arrow::TimestampBuilder *>(poBuilder)->Append(
                        static_cast<int64_t>(
                            (static_cast<double>(nVal) + fSec) * 1000 + 0.5)));
                break;
            }
        }
    }

    // Write geometries
    for (int i = 0; i < nGeomFieldCount; ++i, ++nArrowIdx)
    {
        auto poBuilder = m_apoBuilders[nArrowIdx].get();
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(i);
        if (BuildGeometry(poGeom, i, poBuilder) != OGRERR_NONE)
            return OGRERR_FAILURE;
    }

    m_nFeatureCount++;

    // Flush the current row group if reaching the limit of rows per group.
    if (!m_apoBuilders.empty() && m_apoBuilders[0]->length() == m_nRowGroupSize)
    {
        if (!FlushFeatures())
            return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         FlushFeatures()                              */
/************************************************************************/

inline bool OGRArrowWriterLayer::FlushFeatures()
{
    if (m_apoBuilders.empty() || m_apoBuilders[0]->length() == 0)
        return true;

    if (!IsFileWriterCreated())
    {
        CreateWriter();
        if (!IsFileWriterCreated())
            return false;
    }

    return FlushGroup();
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

inline GIntBig OGRArrowWriterLayer::GetFeatureCount(int bForce)
{
    if (m_poAttrQuery == nullptr && m_poFilterGeom == nullptr)
    {
        return m_nFeatureCount;
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

inline int OGRArrowWriterLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCCreateField) || EQUAL(pszCap, OLCCreateGeomField))
        return m_poSchema == nullptr;

    if (EQUAL(pszCap, OLCSequentialWrite))
        return true;

    if (EQUAL(pszCap, OLCFastWriteArrowBatch))
        return true;

    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return true;

    if (EQUAL(pszCap, OLCMeasuredGeometries))
        return true;

    return false;
}

/************************************************************************/
/*                         WriteArrays()                                */
/************************************************************************/

inline bool OGRArrowWriterLayer::WriteArrays(
    std::function<bool(const std::shared_ptr<arrow::Field> &,
                       const std::shared_ptr<arrow::Array> &)>
        postProcessArray)
{
    int nArrowIdx = 0;
    int nArrowIdxFirstField = !m_osFIDColumn.empty() ? 1 : 0;
    for (const auto &poBuilder : m_apoBuilders)
    {
        const auto &field = m_poSchema->fields()[nArrowIdx];

        std::shared_ptr<arrow::Array> array;
        auto status = poBuilder->Finish(&array);
        if (!status.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "builder::Finish() for field %s failed with %s",
                     field->name().c_str(), status.message().c_str());
            return false;
        }

        // CPLDebug("ARROW", "%s", array->ToString().c_str());

        const int iCol = nArrowIdx - nArrowIdxFirstField;
        if (iCol >= 0 && iCol < m_poFeatureDefn->GetFieldCount())
        {
            const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(iCol);
            const auto eFieldType = poFieldDefn->GetType();
            if (eFieldType == OFTInteger || eFieldType == OFTInteger64)
            {
                const auto &osDomainName = poFieldDefn->GetDomainName();
                const auto oIter =
                    m_oMapFieldDomainToStringArray.find(osDomainName);
                if (oIter != m_oMapFieldDomainToStringArray.end())
                {
                    auto result = arrow::DictionaryArray::FromArrays(
                        field->type(), array, oIter->second);
                    if (!result.ok())
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "DictionaryArray::FromArrays() for field %s "
                                 "failed with %s",
                                 field->name().c_str(),
                                 result.status().message().c_str());
                        return false;
                    }
                    array = *result;
                }
            }
        }

        if (!postProcessArray(field, array))
        {
            return false;
        }

        nArrowIdx++;
    }

    if (m_bWriteBBoxStruct)
    {
        const int nGeomFieldCount = m_poFeatureDefn->GetGeomFieldCount();
        for (int i = 0; i < nGeomFieldCount; ++i)
        {
            const auto &field = m_apoFieldsBBOX[i];
            std::shared_ptr<arrow::Array> array;
            auto status = m_apoBuildersBBOXStruct[i]->Finish(&array);
            if (!status.ok())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "builder::Finish() for field %s failed with %s",
                         field->name().c_str(), status.message().c_str());
                return false;
            }

            if (!postProcessArray(field, array))
            {
                return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                            TestBit()                                 */
/************************************************************************/

static inline bool TestBit(const uint8_t *pabyData, size_t nIdx)
{
    return (pabyData[nIdx / 8] & (1 << (nIdx % 8))) != 0;
}

/************************************************************************/
/*                       WriteArrowBatchInternal()                      */
/************************************************************************/

inline bool OGRArrowWriterLayer::WriteArrowBatchInternal(
    const struct ArrowSchema *schema, struct ArrowArray *array,
    CSLConstList papszOptions,
    std::function<bool(const std::shared_ptr<arrow::RecordBatch> &)> writeBatch)
{
    if (m_poSchema == nullptr)
    {
        CreateSchema();
    }

    if (!IsFileWriterCreated())
    {
        CreateWriter();
        if (!IsFileWriterCreated())
            return false;
    }

    if (m_apoBuilders.empty())
    {
        CreateArrayBuilders();
    }

    const int nGeomFieldCount = m_poFeatureDefn->GetGeomFieldCount();
    const int nGeomFieldCountBBoxFields =
        m_bWriteBBoxStruct ? nGeomFieldCount : 0;

    const char *pszFIDName = CSLFetchNameValueDef(
        papszOptions, "FID", OGRLayer::DEFAULT_ARROW_FID_NAME);
    const char *pszSingleGeomFieldName =
        CSLFetchNameValue(papszOptions, "GEOMETRY_NAME");

    // Sort schema and array children in the same order as m_poSchema.
    // This is needed for non-WKB geometry encoding
    std::map<std::string, int> oMapSchemaChildrenNameToIdx;
    for (int i = 0; i < static_cast<int>(schema->n_children); ++i)
    {
        if (oMapSchemaChildrenNameToIdx.find(schema->children[i]->name) !=
            oMapSchemaChildrenNameToIdx.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Several fields with same name '%s' found",
                     schema->children[i]->name);
            return false;
        }
        oMapSchemaChildrenNameToIdx[schema->children[i]->name] = i;

        if (!pszSingleGeomFieldName && schema->children[i]->metadata)
        {
            const auto oMetadata =
                OGRParseArrowMetadata(schema->children[i]->metadata);
            auto oIter = oMetadata.find(ARROW_EXTENSION_NAME_KEY);
            if (oIter != oMetadata.end() &&
                (oIter->second == EXTENSION_NAME_OGC_WKB ||
                 oIter->second == EXTENSION_NAME_GEOARROW_WKB))
            {
                pszSingleGeomFieldName = schema->children[i]->name;
            }
        }
    }
    if (!pszSingleGeomFieldName)
        pszSingleGeomFieldName = OGRLayer::DEFAULT_ARROW_GEOMETRY_NAME;

    std::vector<int> anMapLayerSchemaToArraySchema(m_poSchema->num_fields(),
                                                   -1);
    struct ArrowArray fidArray;
    struct ArrowSchema fidSchema;
    memset(&fidArray, 0, sizeof(fidArray));
    memset(&fidSchema, 0, sizeof(fidSchema));
    std::vector<void *> apBuffersFid;
    std::vector<int64_t> fids;

    std::set<int> oSetReferencedFieldsInArraySchema;
    const auto DummyFreeArray = [](struct ArrowArray *ptrArray)
    { ptrArray->release = nullptr; };
    const auto DummyFreeSchema = [](struct ArrowSchema *ptrSchema)
    { ptrSchema->release = nullptr; };
    bool bRebuildBatch = false;
    for (int i = 0; i < m_poSchema->num_fields() - nGeomFieldCountBBoxFields;
         ++i)
    {
        auto oIter =
            oMapSchemaChildrenNameToIdx.find(m_poSchema->field(i)->name());
        if (oIter == oMapSchemaChildrenNameToIdx.end())
        {
            if (m_poSchema->field(i)->name() == m_osFIDColumn)
            {
                oIter = oMapSchemaChildrenNameToIdx.find(pszFIDName);
                if (oIter == oMapSchemaChildrenNameToIdx.end())
                {
                    // If the input data does not contain a FID column, but
                    // the output file requires it, creates a default FID column
                    fidArray.release = DummyFreeArray;
                    fidArray.n_buffers = 2;
                    apBuffersFid.resize(2);
                    fidArray.buffers =
                        const_cast<const void **>(apBuffersFid.data());
                    fids.reserve(static_cast<size_t>(array->length));
                    for (size_t iRow = 0;
                         iRow < static_cast<size_t>(array->length); ++iRow)
                        fids.push_back(m_nFeatureCount + iRow);
                    fidArray.buffers[1] = fids.data();
                    fidArray.length = array->length;
                    fidSchema.release = DummyFreeSchema;
                    fidSchema.name = m_osFIDColumn.c_str();
                    fidSchema.format = "l";  // int64
                    continue;
                }
            }
            else if (nGeomFieldCount == 1 &&
                     m_poFeatureDefn->GetGeomFieldIndex(
                         m_poSchema->field(i)->name().c_str()) == 0)
            {
                oIter =
                    oMapSchemaChildrenNameToIdx.find(pszSingleGeomFieldName);
                if (oIter != oMapSchemaChildrenNameToIdx.end())
                    bRebuildBatch = true;
            }

            if (oIter == oMapSchemaChildrenNameToIdx.end())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find field '%s' in schema",
                         m_poSchema->field(i)->name().c_str());
                return false;
            }
        }
        anMapLayerSchemaToArraySchema[i] = oIter->second;
        oSetReferencedFieldsInArraySchema.insert(oIter->second);
    }

    // Note: we cheat a bit by declaring a single instance of the minx/miny/
    // maxx/maxy sub-field ArrowSchema*, and make all struct ArrowSchema point
    // to them. That's OK because we use DummyFreeSchema to release, which does
    // nothing.
    struct ArrowSchema bboxStructSchemaXMin;
    struct ArrowSchema bboxStructSchemaYMin;
    struct ArrowSchema bboxStructSchemaXMax;
    struct ArrowSchema bboxStructSchemaYMax;
    constexpr int BBOX_SUBFIELD_COUNT = 4;
    std::array<struct ArrowSchema *, BBOX_SUBFIELD_COUNT>
        bboxStructSchemaChildren;
    constexpr int BBOX_STRUCT_BUFFER_COUNT = 1;  // validity bitmap array
    // cppcheck-suppress constStatement
    std::vector<std::array<const void *, BBOX_STRUCT_BUFFER_COUNT>>
        bboxStructBuffersPtr;
    std::vector<std::vector<GByte>> aabyBboxStructValidity;
    std::vector<std::vector<float>> aadfMinX, aadfMinY, aadfMaxX, aadfMaxY;
    // cppcheck-suppress constStatement
    std::vector<std::array<struct ArrowArray, BBOX_SUBFIELD_COUNT>> bboxArrays;
    // cppcheck-suppress constStatement
    std::vector<std::array<struct ArrowArray *, BBOX_SUBFIELD_COUNT>>
        bboxArraysPtr;
    constexpr int BBOX_SUBFIELD_BUFFER_COUNT =
        2;  // validity bitmap array and float array
    std::vector<std::array<std::array<const void *, BBOX_SUBFIELD_BUFFER_COUNT>,
                           BBOX_SUBFIELD_COUNT>>
        bboxBuffersPtr;

    // Temporary arrays to hold the geometry bounding boxes.
    std::vector<struct ArrowArray> bboxStructArray;
    std::vector<struct ArrowSchema> bboxStructSchema;

    std::vector<struct ArrowSchema *> newSchemaChildren;
    std::vector<struct ArrowArray *> newArrayChildren;
    newSchemaChildren.reserve(m_poSchema->num_fields());
    newArrayChildren.reserve(m_poSchema->num_fields());
    for (int i = 0; i < m_poSchema->num_fields() - nGeomFieldCountBBoxFields;
         ++i)
    {
        if (anMapLayerSchemaToArraySchema[i] < 0)
        {
            CPLAssert(m_poSchema->field(i)->name() == m_osFIDColumn);
            newSchemaChildren.emplace_back(&fidSchema);
            newArrayChildren.emplace_back(&fidArray);
        }
        else
        {
            newSchemaChildren.emplace_back(
                schema->children[anMapLayerSchemaToArraySchema[i]]);
            newArrayChildren.emplace_back(
                array->children[anMapLayerSchemaToArraySchema[i]]);
        }
    }

    if (m_bWriteBBoxStruct)
    {
        memset(&bboxStructSchemaXMin, 0, sizeof(bboxStructSchemaXMin));
        memset(&bboxStructSchemaYMin, 0, sizeof(bboxStructSchemaYMin));
        memset(&bboxStructSchemaXMax, 0, sizeof(bboxStructSchemaXMax));
        memset(&bboxStructSchemaYMax, 0, sizeof(bboxStructSchemaYMax));

        bboxStructSchemaXMin.release = DummyFreeSchema;
        bboxStructSchemaXMin.name = "xmin";
        bboxStructSchemaXMin.format = "f";  // float32

        bboxStructSchemaYMin.release = DummyFreeSchema;
        bboxStructSchemaYMin.name = "ymin";
        bboxStructSchemaYMin.format = "f";  // float32

        bboxStructSchemaXMax.release = DummyFreeSchema;
        bboxStructSchemaXMax.name = "xmax";
        bboxStructSchemaXMax.format = "f";  // float32

        bboxStructSchemaYMax.release = DummyFreeSchema;
        bboxStructSchemaYMax.name = "ymax";
        bboxStructSchemaYMax.format = "f";  // float32

        try
        {
            constexpr int XMIN_IDX = 0;
            constexpr int YMIN_IDX = 1;
            constexpr int XMAX_IDX = 2;
            constexpr int YMAX_IDX = 3;
            bboxStructSchemaChildren[XMIN_IDX] = &bboxStructSchemaXMin;
            // cppcheck-suppress objectIndex
            bboxStructSchemaChildren[YMIN_IDX] = &bboxStructSchemaYMin;
            // cppcheck-suppress objectIndex
            bboxStructSchemaChildren[XMAX_IDX] = &bboxStructSchemaXMax;
            // cppcheck-suppress objectIndex
            bboxStructSchemaChildren[YMAX_IDX] = &bboxStructSchemaYMax;

            bboxStructArray.resize(nGeomFieldCount);
            bboxStructSchema.resize(nGeomFieldCount);
            bboxArrays.resize(nGeomFieldCount);
            bboxArraysPtr.resize(nGeomFieldCount);
            bboxBuffersPtr.resize(nGeomFieldCount);
            bboxStructBuffersPtr.resize(nGeomFieldCount);
            aabyBboxStructValidity.resize(nGeomFieldCount);
            memset(bboxStructArray.data(), 0,
                   nGeomFieldCount * sizeof(bboxStructArray[0]));
            memset(bboxStructSchema.data(), 0,
                   nGeomFieldCount * sizeof(bboxStructSchema[0]));
            memset(bboxArrays.data(), 0,
                   nGeomFieldCount * sizeof(bboxArrays[0]));
            aadfMinX.resize(nGeomFieldCount);
            aadfMinY.resize(nGeomFieldCount);
            aadfMaxX.resize(nGeomFieldCount);
            aadfMaxY.resize(nGeomFieldCount);
            for (int i = 0; i < nGeomFieldCount; ++i)
            {
                const bool bIsNullable = CPL_TO_BOOL(
                    m_poFeatureDefn->GetGeomFieldDefn(i)->IsNullable());
                aadfMinX[i].reserve(static_cast<size_t>(array->length));
                aadfMinY[i].reserve(static_cast<size_t>(array->length));
                aadfMaxX[i].reserve(static_cast<size_t>(array->length));
                aadfMaxY[i].reserve(static_cast<size_t>(array->length));
                aabyBboxStructValidity[i].resize(
                    static_cast<size_t>(array->length + 7) / 8, 0xFF);

                bboxStructSchema[i].release = DummyFreeSchema;
                bboxStructSchema[i].name = m_apoFieldsBBOX[i]->name().c_str();
                bboxStructSchema[i].format = "+s";  // structure
                bboxStructSchema[i].flags =
                    bIsNullable ? ARROW_FLAG_NULLABLE : 0;
                bboxStructSchema[i].n_children = BBOX_SUBFIELD_COUNT;
                bboxStructSchema[i].children = bboxStructSchemaChildren.data();

                constexpr int VALIDITY_ARRAY_IDX = 0;
                constexpr int BBOX_SUBFIELD_FLOAT_VALUE_IDX = 1;
                bboxBuffersPtr[i][XMIN_IDX][BBOX_SUBFIELD_FLOAT_VALUE_IDX] =
                    aadfMinX[i].data();
                bboxBuffersPtr[i][YMIN_IDX][BBOX_SUBFIELD_FLOAT_VALUE_IDX] =
                    aadfMinY[i].data();
                bboxBuffersPtr[i][XMAX_IDX][BBOX_SUBFIELD_FLOAT_VALUE_IDX] =
                    aadfMaxX[i].data();
                bboxBuffersPtr[i][YMAX_IDX][BBOX_SUBFIELD_FLOAT_VALUE_IDX] =
                    aadfMaxY[i].data();

                for (int j = 0; j < BBOX_SUBFIELD_COUNT; ++j)
                {
                    bboxBuffersPtr[i][j][VALIDITY_ARRAY_IDX] = nullptr;

                    bboxArrays[i][j].release = DummyFreeArray;
                    bboxArrays[i][j].length = array->length;
                    bboxArrays[i][j].n_buffers = BBOX_SUBFIELD_BUFFER_COUNT;
                    bboxArrays[i][j].buffers = bboxBuffersPtr[i][j].data();

                    bboxArraysPtr[i][j] = &bboxArrays[i][j];
                }

                bboxStructArray[i].release = DummyFreeArray;
                bboxStructArray[i].n_children = BBOX_SUBFIELD_COUNT;
                // coverity[escape]
                bboxStructArray[i].children = bboxArraysPtr[i].data();
                bboxStructArray[i].length = array->length;
                bboxStructArray[i].n_buffers = BBOX_STRUCT_BUFFER_COUNT;
                bboxStructBuffersPtr[i][VALIDITY_ARRAY_IDX] =
                    bIsNullable ? aabyBboxStructValidity[i].data() : nullptr;
                // coverity[escape]
                bboxStructArray[i].buffers = bboxStructBuffersPtr[i].data();

                newSchemaChildren.emplace_back(&bboxStructSchema[i]);
                newArrayChildren.emplace_back(&bboxStructArray[i]);
            }
        }
        catch (const std::bad_alloc &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory in "
                     "OGRArrowWriterLayer::WriteArrowBatchInternal()");
            return false;
        }
    }

    for (int i = 0; i < static_cast<int>(schema->n_children); ++i)
    {
        if (oSetReferencedFieldsInArraySchema.find(i) ==
            oSetReferencedFieldsInArraySchema.end())
        {
            if (m_osFIDColumn.empty() &&
                strcmp(schema->children[i]->name, pszFIDName) == 0)
            {
                // If the input data contains a FID column, but the output data
                // does not, then ignore it.
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Found field '%s' in array schema that does not exist "
                         "in layer schema",
                         schema->children[i]->name);
                return false;
            }
        }
    }

    // ImportSchema() would release the schema, but we don't want that
    // So copy the structure content into a local variable, and override its
    // release callback to a no-op. This may be a bit fragile, but it doesn't
    // look like ImportSchema implementation tries to access the C ArrowSchema
    // after it has been called.
    struct ArrowSchema lSchema = *schema;
    schema = &lSchema;
    CPL_IGNORE_RET_VAL(schema);

    lSchema.n_children = newSchemaChildren.size();
    lSchema.children = newSchemaChildren.data();

    lSchema.release = DummyFreeSchema;
    auto poSchemaResult = arrow::ImportSchema(&lSchema);
    CPLAssert(lSchema.release == nullptr);
    if (!poSchemaResult.ok())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "ImportSchema() failed with %s",
                 poSchemaResult.status().message().c_str());
        return false;
    }
    auto poSchema = *poSchemaResult;

    // Hack the array to use the new children we've computed above
    // but make sure the original release() callback sees the original children
    struct ArrayReleaser
    {
        struct ArrowArray ori_array
        {
        };

        explicit ArrayReleaser(struct ArrowArray *array)
        {
            memcpy(&ori_array, array, sizeof(*array));
            array->release = ArrayReleaser::release;
            array->private_data = this;
        }

        static void release(struct ArrowArray *array)
        {
            struct ArrayReleaser *releaser =
                static_cast<struct ArrayReleaser *>(array->private_data);
            memcpy(array, &(releaser->ori_array), sizeof(*array));
            CPLAssert(array->release != nullptr);
            array->release(array);
            CPLAssert(array->release == nullptr);
            delete releaser;
        }
    };

    // Must be allocated on the heap, since ArrayReleaser::release() will be
    // called after this method has ended.
    ArrayReleaser *releaser = new ArrayReleaser(array);
    array->private_data = releaser;
    array->n_children = newArrayChildren.size();
    // cppcheck-suppress autoVariables
    array->children = newArrayChildren.data();

    // Process geometry columns:
    // - if the output encoding is WKB, then just note the geometry type and
    //   envelope.
    // - otherwise convert to the output encoding.
    int nBuilderIdx = 0;
    if (!m_osFIDColumn.empty())
    {
        nBuilderIdx++;
    }
    std::map<std::string, std::shared_ptr<arrow::Array>>
        oMapGeomFieldNameToArray;
    for (int i = 0; i < nGeomFieldCount; ++i, ++nBuilderIdx)
    {
        const char *pszThisGeomFieldName =
            m_poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef();
        int nIdx = poSchema->GetFieldIndex(pszThisGeomFieldName);
        if (nIdx < 0)
        {
            if (nGeomFieldCount == 1)
                nIdx = poSchema->GetFieldIndex(pszSingleGeomFieldName);
            if (nIdx < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find geometry field '%s' in schema",
                         pszThisGeomFieldName);
                return false;
            }
        }

        if (strcmp(lSchema.children[nIdx]->format, "z") != 0 &&
            strcmp(lSchema.children[nIdx]->format, "Z") != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Type of geometry field '%s' is not binary, but '%s'",
                     pszThisGeomFieldName, lSchema.children[nIdx]->format);
            return false;
        }

        const auto psGeomArray = array->children[nIdx];
        const uint8_t *pabyValidity =
            psGeomArray->null_count != 0
                ? static_cast<const uint8_t *>(psGeomArray->buffers[0])
                : nullptr;
        const bool bUseOffsets32 =
            (strcmp(lSchema.children[nIdx]->format, "z") == 0);
        const uint32_t *panOffsets32 =
            static_cast<const uint32_t *>(psGeomArray->buffers[1]) +
            psGeomArray->offset;
        const uint64_t *panOffsets64 =
            static_cast<const uint64_t *>(psGeomArray->buffers[1]) +
            psGeomArray->offset;
        GByte *pabyData =
            static_cast<GByte *>(const_cast<void *>(psGeomArray->buffers[2]));
        OGREnvelope sEnvelope;
        auto poBuilder = m_apoBuilders[nBuilderIdx].get();

        for (size_t iRow = 0; iRow < static_cast<size_t>(psGeomArray->length);
             ++iRow)
        {
            bool bValidGeom = false;

            if (!pabyValidity ||
                TestBit(pabyValidity, iRow + psGeomArray->offset))
            {
                const auto nLen =
                    bUseOffsets32 ? static_cast<size_t>(panOffsets32[iRow + 1] -
                                                        panOffsets32[iRow])
                                  : static_cast<size_t>(panOffsets64[iRow + 1] -
                                                        panOffsets64[iRow]);
                GByte *pabyWkb =
                    pabyData + (bUseOffsets32
                                    ? panOffsets32[iRow]
                                    : static_cast<size_t>(panOffsets64[iRow]));
                if (m_aeGeomEncoding[i] == OGRArrowGeomEncoding::WKB)
                {
                    FixupWKBGeometryBeforeWriting(pabyWkb, nLen);

                    uint32_t nType = 0;
                    bool bNeedSwap = false;
                    if (OGRWKBGetGeomType(pabyWkb, nLen, bNeedSwap, nType))
                    {
                        m_oSetWrittenGeometryTypes[i].insert(
                            static_cast<OGRwkbGeometryType>(nType));
                        if (OGRWKBGetBoundingBox(pabyWkb, nLen, sEnvelope))
                        {
                            bValidGeom = true;
                            m_aoEnvelopes[i].Merge(sEnvelope);

                            if (m_bWriteBBoxStruct)
                            {
                                aadfMinX[i].push_back(
                                    castToFloatDown(sEnvelope.MinX));
                                aadfMinY[i].push_back(
                                    castToFloatDown(sEnvelope.MinY));
                                aadfMaxX[i].push_back(
                                    castToFloatUp(sEnvelope.MaxX));
                                aadfMaxY[i].push_back(
                                    castToFloatUp(sEnvelope.MaxY));
                            }
                        }
                    }
                }
                else
                {
                    size_t nBytesConsumedOut = 0;
                    OGRGeometry *poGeometry = nullptr;
                    OGRGeometryFactory::createFromWkb(
                        pabyWkb, nullptr, &poGeometry, nLen, wkbVariantIso,
                        nBytesConsumedOut);
                    if (BuildGeometry(poGeometry, i, poBuilder) != OGRERR_NONE)
                    {
                        delete poGeometry;
                        return false;
                    }
                    bValidGeom = true;
                    if (m_bWriteBBoxStruct)
                    {
                        poGeometry->getEnvelope(&sEnvelope);
                        aadfMinX[i].push_back(castToFloatDown(sEnvelope.MinX));
                        aadfMinY[i].push_back(castToFloatDown(sEnvelope.MinY));
                        aadfMaxX[i].push_back(castToFloatUp(sEnvelope.MaxX));
                        aadfMaxY[i].push_back(castToFloatUp(sEnvelope.MaxY));
                    }
                    delete poGeometry;
                }
            }
            else
            {
                if (m_aeGeomEncoding[i] != OGRArrowGeomEncoding::WKB)
                {
                    if (BuildGeometry(nullptr, i, poBuilder) != OGRERR_NONE)
                        return false;
                }
            }

            if (!bValidGeom && m_bWriteBBoxStruct)
            {
                if ((bboxStructSchema[i].flags & ARROW_FLAG_NULLABLE))
                {
                    bboxStructArray[i].null_count++;
                    aabyBboxStructValidity[i][iRow / 8] &=
                        ~(1 << static_cast<int>(iRow % 8));
                }
                aadfMinX[i].push_back(0.0f);
                aadfMinY[i].push_back(0.0f);
                aadfMaxX[i].push_back(0.0f);
                aadfMaxY[i].push_back(0.0f);
            }
        }

        if (m_aeGeomEncoding[i] != OGRArrowGeomEncoding::WKB)
        {
            std::shared_ptr<arrow::Array> geomArray;
            auto status = poBuilder->Finish(&geomArray);
            if (!status.ok())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "builder::Finish() for field %s failed with %s",
                         pszThisGeomFieldName, status.message().c_str());
                return false;
            }
            oMapGeomFieldNameToArray[pszThisGeomFieldName] =
                std::move(geomArray);
        }
    }

    auto poRecordBatchResult =
        arrow::ImportRecordBatch(array, std::move(poSchema));
    if (!poRecordBatchResult.ok())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ImportRecordBatch() failed with %s",
                 poRecordBatchResult.status().message().c_str());
        return false;
    }
    auto poRecordBatch = *poRecordBatchResult;

    // below assertion commented out since it is not strictly necessary, but
    // reflects what ImportRecordBatch() does.
    // CPLAssert(array->release == nullptr);

    // We may need to reconstruct a final record batch that perfectly matches
    // the expected schema.
    if (bRebuildBatch || !oMapGeomFieldNameToArray.empty())
    {
        std::vector<std::shared_ptr<arrow::Array>> apoArrays;
        for (int i = 0; i < m_poSchema->num_fields(); ++i)
        {
            auto oIter =
                oMapGeomFieldNameToArray.find(m_poSchema->field(i)->name());
            if (oIter != oMapGeomFieldNameToArray.end())
                apoArrays.emplace_back(oIter->second);
            else
                apoArrays.emplace_back(poRecordBatch->column(i));
            if (apoArrays.back()->type()->id() !=
                m_poSchema->field(i)->type()->id())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field '%s' of unexpected type",
                         m_poSchema->field(i)->name().c_str());
                return false;
            }
        }
        poRecordBatchResult = arrow::RecordBatch::Make(
            m_poSchema, poRecordBatch->num_rows(), std::move(apoArrays));
        if (!poRecordBatchResult.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RecordBatch::Make() failed with %s",
                     poRecordBatchResult.status().message().c_str());
            return false;
        }
        poRecordBatch = *poRecordBatchResult;
    }

    if (writeBatch(poRecordBatch))
    {
        m_nFeatureCount += poRecordBatch->num_rows();
        return true;
    }
    return false;
}

#endif /* OGARROWWRITERLAYER_HPP_INCLUDED */
