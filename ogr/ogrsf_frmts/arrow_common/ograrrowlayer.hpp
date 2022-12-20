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
#include "ogr_p.h"
#include "ogr_swq.h"

#include <algorithm>
#include <cinttypes>
#include <limits>

#define SWQ_ISNOTNULL (-SWQ_ISNULL)

/************************************************************************/
/*                         OGRArrowLayer()                              */
/************************************************************************/

inline OGRArrowLayer::OGRArrowLayer(OGRArrowDataset *poDS,
                                    const char *pszLayerName)
    : m_poArrowDS(poDS), m_poMemoryPool(poDS->GetMemoryPool())
{
    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    SetDescription(pszLayerName);
}

/************************************************************************/
/*                        ~OGRFeatherLayer()                            */
/************************************************************************/

inline OGRArrowLayer::~OGRArrowLayer()
{
    CPLDebug("ARROW", "Memory pool: bytes_allocated = %" PRId64,
             m_poMemoryPool->bytes_allocated());
    CPLDebug("ARROW", "Memory pool: max_memory = %" PRId64,
             m_poMemoryPool->max_memory());
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                         LoadGDALMetadata()                           */
/************************************************************************/

inline std::map<std::string, std::unique_ptr<OGRFieldDefn>>
OGRArrowLayer::LoadGDALMetadata(const arrow::KeyValueMetadata *kv_metadata)
{
    std::map<std::string, std::unique_ptr<OGRFieldDefn>>
        oMapFieldNameToGDALSchemaFieldDefn;
    if (kv_metadata && kv_metadata->Contains("gdal:schema") &&
        CPLTestBool(CPLGetConfigOption(
            ("OGR_" + GetDriverUCName() + "_READ_GDAL_SCHEMA").c_str(), "YES")))
    {
        auto gdalSchema = kv_metadata->Get("gdal:schema");
        if (gdalSchema.ok())
        {
            CPLDebug(GetDriverUCName().c_str(), "gdal:schema = %s",
                     gdalSchema->c_str());
            CPLJSONDocument oDoc;
            if (oDoc.LoadMemory(*gdalSchema))
            {
                auto oRoot = oDoc.GetRoot();

                m_osFIDColumn = oRoot.GetString("fid");

                auto oColumns = oRoot.GetObj("columns");
                if (oColumns.IsValid())
                {
                    for (const auto &oColumn : oColumns.GetChildren())
                    {
                        const auto osName = oColumn.GetName();
                        const auto osType = oColumn.GetString("type");
                        const auto osSubType = oColumn.GetString("subtype");
                        auto poFieldDefn = cpl::make_unique<OGRFieldDefn>(
                            osName.c_str(), OFTString);
                        for (int iType = 0;
                             iType <= static_cast<int>(OFTMaxType); iType++)
                        {
                            if (EQUAL(osType.c_str(),
                                      OGRFieldDefn::GetFieldTypeName(
                                          static_cast<OGRFieldType>(iType))))
                            {
                                poFieldDefn->SetType(
                                    static_cast<OGRFieldType>(iType));
                                break;
                            }
                        }
                        if (!osSubType.empty())
                        {
                            for (int iSubType = 0;
                                 iSubType <= static_cast<int>(OFSTMaxSubType);
                                 iSubType++)
                            {
                                if (EQUAL(osSubType.c_str(),
                                          OGRFieldDefn::GetFieldSubTypeName(
                                              static_cast<OGRFieldSubType>(
                                                  iSubType))))
                                {
                                    poFieldDefn->SetSubType(
                                        static_cast<OGRFieldSubType>(iSubType));
                                    break;
                                }
                            }
                        }
                        poFieldDefn->SetWidth(oColumn.GetInteger("width"));
                        poFieldDefn->SetPrecision(
                            oColumn.GetInteger("precision"));
                        oMapFieldNameToGDALSchemaFieldDefn[osName] =
                            std::move(poFieldDefn);
                    }
                }
            }
        }
    }
    return oMapFieldNameToGDALSchemaFieldDefn;
}

/************************************************************************/
/*                        IsIntegerArrowType()                          */
/************************************************************************/

inline bool OGRArrowLayer::IsIntegerArrowType(arrow::Type::type typeId)
{
    return typeId == arrow::Type::INT8 || typeId == arrow::Type::UINT8 ||
           typeId == arrow::Type::INT16 || typeId == arrow::Type::UINT16 ||
           typeId == arrow::Type::INT32 || typeId == arrow::Type::UINT32 ||
           typeId == arrow::Type::INT64 || typeId == arrow::Type::UINT64;
}

/************************************************************************/
/*                        MapArrowTypeToOGR()                           */
/************************************************************************/

inline bool OGRArrowLayer::MapArrowTypeToOGR(
    const std::shared_ptr<arrow::DataType> &type,
    const std::shared_ptr<arrow::Field> &field, OGRFieldDefn &oField,
    OGRFieldType &eType, OGRFieldSubType &eSubType,
    const std::vector<int> &path,
    const std::map<std::string, std::unique_ptr<OGRFieldDefn>>
        &oMapFieldNameToGDALSchemaFieldDefn)
{
    bool bTypeOK = true;
    switch (type->id())
    {
        case arrow::Type::NA:
            break;

        case arrow::Type::BOOL:
            eType = OFTInteger;
            eSubType = OFSTBoolean;
            break;
        case arrow::Type::UINT8:
        case arrow::Type::INT8:
        case arrow::Type::UINT16:
            eType = OFTInteger;
            break;
        case arrow::Type::INT16:
            eType = OFTInteger;
            eSubType = OFSTInt16;
            break;
        case arrow::Type::UINT32:
            eType = OFTInteger64;
            break;
        case arrow::Type::INT32:
            eType = OFTInteger;
            break;
        case arrow::Type::UINT64:
            eType = OFTReal;  // potential loss
            break;
        case arrow::Type::INT64:
            eType = OFTInteger64;
            break;
        case arrow::Type::HALF_FLOAT:  // should use OFSTFloat16 if we had it
        case arrow::Type::FLOAT:
            eType = OFTReal;
            eSubType = OFSTFloat32;
            break;
        case arrow::Type::DOUBLE:
            eType = OFTReal;
            break;
        case arrow::Type::STRING:
        case arrow::Type::LARGE_STRING:
            eType = OFTString;
            break;
        case arrow::Type::BINARY:
        case arrow::Type::LARGE_BINARY:
            eType = OFTBinary;
            break;
        case arrow::Type::FIXED_SIZE_BINARY:
            eType = OFTBinary;
            oField.SetWidth(
                std::static_pointer_cast<arrow::FixedSizeBinaryType>(type)
                    ->byte_width());
            break;

        case arrow::Type::DATE32:
        case arrow::Type::DATE64:
            eType = OFTDate;
            break;

        case arrow::Type::TIMESTAMP:
            eType = OFTDateTime;
            break;

        case arrow::Type::TIME32:
            eType = OFTTime;
            break;

        case arrow::Type::TIME64:
            eType = OFTInteger64;  // our OFTTime doesn't have micro or
                                   // nanosecond accuracy
            break;

        case arrow::Type::DECIMAL128:
        case arrow::Type::DECIMAL256:
        {
            const auto decimalType =
                std::static_pointer_cast<arrow::DecimalType>(type);
            eType = OFTReal;
            oField.SetWidth(decimalType->precision());
            oField.SetPrecision(decimalType->scale());
            break;
        }

        case arrow::Type::LIST:
        case arrow::Type::FIXED_SIZE_LIST:
        {
            auto listType = std::static_pointer_cast<arrow::BaseListType>(type);
            switch (listType->value_type()->id())
            {
                case arrow::Type::BOOL:
                    eType = OFTIntegerList;
                    eSubType = OFSTBoolean;
                    break;
                case arrow::Type::UINT8:
                case arrow::Type::INT8:
                case arrow::Type::UINT16:
                case arrow::Type::INT16:
                case arrow::Type::INT32:
                    eType = OFTIntegerList;
                    break;
                case arrow::Type::UINT32:
                    eType = OFTInteger64List;
                    break;
                case arrow::Type::UINT64:
                    eType = OFTRealList;  // potential loss
                    break;
                case arrow::Type::INT64:
                    eType = OFTInteger64List;
                    break;
                case arrow::Type::HALF_FLOAT:  // should use OFSTFloat16 if we
                                               // had it
                case arrow::Type::FLOAT:
                    eType = OFTRealList;
                    eSubType = OFSTFloat32;
                    break;
                case arrow::Type::DOUBLE:
                    eType = OFTRealList;
                    break;
                case arrow::Type::STRING:
                    eType = OFTStringList;
                    break;
                default:
                    bTypeOK = false;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Field %s of unhandled type %s ignored",
                             field->name().c_str(), type->ToString().c_str());
                    break;
            }
            break;
        }

        case arrow::Type::MAP:
        {
            auto mapType = std::static_pointer_cast<arrow::MapType>(type);
            const auto itemTypeId = mapType->item_type()->id();
            if (mapType->key_type()->id() == arrow::Type::STRING &&
                (itemTypeId == arrow::Type::BOOL ||
                 IsIntegerArrowType(itemTypeId) ||
                 itemTypeId == arrow::Type::HALF_FLOAT ||
                 itemTypeId == arrow::Type::FLOAT ||
                 itemTypeId == arrow::Type::DOUBLE ||
                 itemTypeId == arrow::Type::STRING))
            {
                eType = OFTString;
                eSubType = OFSTJSON;
            }
            else
            {
                bTypeOK = false;
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Field %s of unhandled type %s ignored",
                         field->name().c_str(), type->ToString().c_str());
            }
            break;
        }

        case arrow::Type::STRUCT:
            // should be handled by specialized code
            CPLAssert(false);
            break;

            // unhandled types

        case arrow::Type::INTERVAL_MONTHS:
        case arrow::Type::INTERVAL_DAY_TIME:
        case arrow::Type::SPARSE_UNION:
        case arrow::Type::DENSE_UNION:
        case arrow::Type::DICTIONARY:
        case arrow::Type::EXTENSION:
        case arrow::Type::DURATION:
        case arrow::Type::LARGE_LIST:
        case arrow::Type::INTERVAL_MONTH_DAY_NANO:
        case arrow::Type::MAX_ID:
        {
            bTypeOK = false;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Field %s of unhandled type %s ignored",
                     field->name().c_str(), type->ToString().c_str());
            break;
        }
    }

    if (bTypeOK)
    {
        const auto oIter =
            oMapFieldNameToGDALSchemaFieldDefn.find(field->name());
        oField.SetType(eType);
        if (oIter != oMapFieldNameToGDALSchemaFieldDefn.end())
        {
            const auto &poGDALFieldDefn = oIter->second;
            if (poGDALFieldDefn->GetType() == eType)
            {
                if (eSubType == OFSTNone)
                {
                    eSubType = poGDALFieldDefn->GetSubType();
                }
                else if (eSubType != poGDALFieldDefn->GetSubType())
                {
                    CPLDebug(
                        GetDriverUCName().c_str(),
                        "Field subtype inferred from Parquet/Arrow schema is "
                        "%s, "
                        "whereas the one in gdal:schema is %s. "
                        "Using the former one.",
                        OGR_GetFieldSubTypeName(eSubType),
                        OGR_GetFieldSubTypeName(poGDALFieldDefn->GetSubType()));
                }
            }
            else
            {
                CPLDebug(GetDriverUCName().c_str(),
                         "Field type inferred from Parquet/Arrow schema is %s, "
                         "whereas the one in gdal:schema is %s. "
                         "Using the former one.",
                         OGR_GetFieldTypeName(eType),
                         OGR_GetFieldTypeName(poGDALFieldDefn->GetType()));
            }
            if (poGDALFieldDefn->GetWidth() > 0)
                oField.SetWidth(poGDALFieldDefn->GetWidth());
            if (poGDALFieldDefn->GetPrecision() > 0)
                oField.SetPrecision(poGDALFieldDefn->GetPrecision());
        }
        oField.SetSubType(eSubType);
        oField.SetNullable(field->nullable());
        m_poFeatureDefn->AddFieldDefn(&oField);
        m_anMapFieldIndexToArrowColumn.push_back(path);
    }

    return bTypeOK;
}

/************************************************************************/
/*                         CreateFieldFromSchema()                      */
/************************************************************************/

inline void OGRArrowLayer::CreateFieldFromSchema(
    const std::shared_ptr<arrow::Field> &field, const std::vector<int> &path,
    const std::map<std::string, std::unique_ptr<OGRFieldDefn>>
        &oMapFieldNameToGDALSchemaFieldDefn)
{
    OGRFieldDefn oField(field->name().c_str(), OFTString);
    OGRFieldType eType = OFTString;
    OGRFieldSubType eSubType = OFSTNone;
    bool bTypeOK = true;

    auto type = field->type();
    if (type->id() == arrow::Type::DICTIONARY && path.size() == 1)
    {
        const auto dictionaryType =
            std::static_pointer_cast<arrow::DictionaryType>(field->type());
        const auto indexType = dictionaryType->index_type();
        if (dictionaryType->value_type()->id() == arrow::Type::STRING &&
            IsIntegerArrowType(indexType->id()))
        {
            std::string osDomainName(field->name() + "Domain");
            m_poArrowDS->RegisterDomainName(osDomainName,
                                            m_poFeatureDefn->GetFieldCount());
            oField.SetDomainName(osDomainName);
            type = indexType;
        }
        else
        {
            bTypeOK = false;
        }
    }

    if (type->id() == arrow::Type::STRUCT)
    {
        const auto subfields = field->Flatten();
        auto newpath = path;
        newpath.push_back(0);
        for (int j = 0; j < static_cast<int>(subfields.size()); j++)
        {
            const auto &subfield = subfields[j];
            newpath.back() = j;
            CreateFieldFromSchema(subfield, newpath,
                                  oMapFieldNameToGDALSchemaFieldDefn);
        }
    }
    else if (bTypeOK)
    {
        MapArrowTypeToOGR(type, field, oField, eType, eSubType, path,
                          oMapFieldNameToGDALSchemaFieldDefn);
    }
}

/************************************************************************/
/*                       BuildDomainFromBatch()                         */
/************************************************************************/

inline std::unique_ptr<OGRFieldDomain> OGRArrowLayer::BuildDomainFromBatch(
    const std::string &osDomainName,
    const std::shared_ptr<arrow::RecordBatch> &poBatch, int iCol) const
{
    const auto array = poBatch->column(iCol);
    auto castArray = std::static_pointer_cast<arrow::DictionaryArray>(array);
    auto dict = castArray->dictionary();
    CPLAssert(dict->type_id() == arrow::Type::STRING);
    OGRFieldType eType = OFTInteger;
    const auto indexTypeId = castArray->dict_type()->index_type()->id();
    if (indexTypeId == arrow::Type::UINT32 ||
        indexTypeId == arrow::Type::UINT64 || indexTypeId == arrow::Type::INT64)
        eType = OFTInteger64;
    auto values = std::static_pointer_cast<arrow::StringArray>(dict);
    std::vector<OGRCodedValue> asValues;
    asValues.reserve(values->length());
    for (int i = 0; i < values->length(); ++i)
    {
        if (!values->IsNull(i))
        {
            OGRCodedValue val;
            val.pszCode = CPLStrdup(CPLSPrintf("%d", i));
            val.pszValue = CPLStrdup(values->GetString(i).c_str());
            asValues.emplace_back(val);
        }
    }
    return cpl::make_unique<OGRCodedFieldDomain>(
        osDomainName, std::string(), eType, OFSTNone, std::move(asValues));
}

/************************************************************************/
/*               ComputeGeometryColumnTypeProcessBatch()                */
/************************************************************************/

inline OGRwkbGeometryType OGRArrowLayer::ComputeGeometryColumnTypeProcessBatch(
    const std::shared_ptr<arrow::RecordBatch> &poBatch, int iGeomCol,
    int iBatchCol, OGRwkbGeometryType eGeomType) const
{
    const auto array = poBatch->column(iBatchCol);
    const auto castBinaryArray =
        (m_aeGeomEncoding[iGeomCol] == OGRArrowGeomEncoding::WKB)
            ? std::static_pointer_cast<arrow::BinaryArray>(array)
            : nullptr;
    const auto castStringArray =
        (m_aeGeomEncoding[iGeomCol] == OGRArrowGeomEncoding::WKT)
            ? std::static_pointer_cast<arrow::StringArray>(array)
            : nullptr;
    for (int64_t i = 0; i < poBatch->num_rows(); i++)
    {
        if (!array->IsNull(i))
        {
            OGRwkbGeometryType eThisGeomType = wkbNone;
            if (m_aeGeomEncoding[iGeomCol] == OGRArrowGeomEncoding::WKB &&
                castBinaryArray)
            {
                arrow::BinaryArray::offset_type out_length = 0;
                const uint8_t *data = castBinaryArray->GetValue(i, &out_length);
                if (out_length >= 5)
                {
                    OGRReadWKBGeometryType(data, wkbVariantIso, &eThisGeomType);
                }
            }
            else if (m_aeGeomEncoding[iGeomCol] == OGRArrowGeomEncoding::WKT &&
                     castStringArray)
            {
                const auto osWKT = castStringArray->GetString(i);
                if (!osWKT.empty())
                {
                    OGRReadWKTGeometryType(osWKT.c_str(), &eThisGeomType);
                }
            }

            if (eThisGeomType != wkbNone)
            {
                if (eGeomType == wkbNone)
                    eGeomType = eThisGeomType;
                else if (wkbFlatten(eThisGeomType) == wkbFlatten(eGeomType))
                    ;
                else if (wkbFlatten(eThisGeomType) == wkbMultiLineString &&
                         wkbFlatten(eGeomType) == wkbLineString)
                {
                    eGeomType = OGR_GT_SetModifier(
                        wkbMultiLineString,
                        OGR_GT_HasZ(eThisGeomType) || OGR_GT_HasZ(eGeomType),
                        OGR_GT_HasM(eThisGeomType) || OGR_GT_HasM(eGeomType));
                }
                else if (wkbFlatten(eThisGeomType) == wkbLineString &&
                         wkbFlatten(eGeomType) == wkbMultiLineString)
                    ;
                else if (wkbFlatten(eThisGeomType) == wkbMultiPolygon &&
                         wkbFlatten(eGeomType) == wkbPolygon)
                {
                    eGeomType = OGR_GT_SetModifier(
                        wkbMultiPolygon,
                        OGR_GT_HasZ(eThisGeomType) || OGR_GT_HasZ(eGeomType),
                        OGR_GT_HasM(eThisGeomType) || OGR_GT_HasM(eGeomType));
                }
                else if (wkbFlatten(eThisGeomType) == wkbPolygon &&
                         wkbFlatten(eGeomType) == wkbMultiPolygon)
                    ;
                else
                    return wkbUnknown;

                eGeomType = OGR_GT_SetModifier(
                    eGeomType,
                    OGR_GT_HasZ(eThisGeomType) || OGR_GT_HasZ(eGeomType),
                    OGR_GT_HasM(eThisGeomType) || OGR_GT_HasM(eGeomType));
            }
        }
    }
    return eGeomType;
}

/************************************************************************/
/*                           IsPointType()                              */
/************************************************************************/

static bool IsPointType(const std::shared_ptr<arrow::DataType> &type,
                        bool &bHasZOut, bool &bHasMOut)
{
    if (type->id() != arrow::Type::FIXED_SIZE_LIST)
        return false;
    auto poListType = std::static_pointer_cast<arrow::FixedSizeListType>(type);
    const int nOutDimensionality = poListType->list_size();
    const auto osValueFieldName = poListType->value_field()->name();
    if (nOutDimensionality == 2)
    {
        bHasZOut = false;
        bHasMOut = false;
    }
    else if (nOutDimensionality == 3)
    {
        if (osValueFieldName == "xym")
        {
            bHasZOut = false;
            bHasMOut = true;
        }
        else if (osValueFieldName == "xyz")
        {
            bHasMOut = false;
            bHasZOut = true;
        }
    }
    else if (nOutDimensionality == 4)
    {
        bHasMOut = true;
        bHasZOut = true;
    }
    else
    {
        return false;
    }
    return poListType->value_type()->id() == arrow::Type::DOUBLE;
}

/************************************************************************/
/*                         IsListOfPointType()                          */
/************************************************************************/

static bool IsListOfPointType(const std::shared_ptr<arrow::DataType> &type,
                              int nDepth, bool &bHasZOut, bool &bHasMOut)
{
    if (type->id() != arrow::Type::LIST)
        return false;
    auto poListType = std::static_pointer_cast<arrow::ListType>(type);
    return nDepth == 1
               ? IsPointType(poListType->value_type(), bHasZOut, bHasMOut)
               : IsListOfPointType(poListType->value_type(), nDepth - 1,
                                   bHasZOut, bHasMOut);
}

/************************************************************************/
/*                        IsValidGeometryEncoding()                     */
/************************************************************************/

inline bool OGRArrowLayer::IsValidGeometryEncoding(
    const std::shared_ptr<arrow::Field> &field, const std::string &osEncoding,
    OGRwkbGeometryType &eGeomTypeOut,
    OGRArrowGeomEncoding &eOGRArrowGeomEncodingOut)
{
    const auto &fieldName = field->name();
    std::shared_ptr<arrow::DataType> fieldType = field->type();
    auto fieldTypeId = fieldType->id();

    if (fieldTypeId == arrow::Type::EXTENSION)
    {
        auto extensionType =
            cpl::down_cast<arrow::ExtensionType *>(fieldType.get());
        fieldType = extensionType->storage_type();
        fieldTypeId = fieldType->id();
    }

    eGeomTypeOut = wkbUnknown;

    if (osEncoding == "WKT")
    {
        if (fieldTypeId != arrow::Type::STRING)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry column %s has a non String type: %s. "
                     "Handling it as a regular field",
                     fieldName.c_str(), fieldType->name().c_str());
            return false;
        }
        eOGRArrowGeomEncodingOut = OGRArrowGeomEncoding::WKT;
        return true;
    }

    if (osEncoding == "WKB")
    {
        if (fieldTypeId != arrow::Type::BINARY)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry column %s has a non Binary type: %s. "
                     "Handling it as a regular field",
                     fieldName.c_str(), fieldType->name().c_str());
            return false;
        }
        eOGRArrowGeomEncodingOut = OGRArrowGeomEncoding::WKB;
        return true;
    }

    bool bHasZ = false;
    bool bHasM = false;
    if (osEncoding == "geoarrow.point")
    {
        if (!IsPointType(fieldType, bHasZ, bHasM))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry column %s has a type != fixed_size_list<xy: "
                     "double>[2]>: %s. "
                     "Handling it as a regular field",
                     fieldName.c_str(), fieldType->name().c_str());
            return false;
        }
        eGeomTypeOut = OGR_GT_SetModifier(wkbPoint, static_cast<int>(bHasZ),
                                          static_cast<int>(bHasM));
        eOGRArrowGeomEncodingOut = OGRArrowGeomEncoding::GEOARROW_POINT;
        return true;
    }

    if (osEncoding == "geoarrow.linestring")
    {
        if (!IsListOfPointType(fieldType, 1, bHasZ, bHasM))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry column %s has a type != fixed_size_list<xy: "
                     "double>[2]>: %s. "
                     "Handling it as a regular field",
                     fieldName.c_str(), fieldType->name().c_str());
            return false;
        }
        eGeomTypeOut = OGR_GT_SetModifier(
            wkbLineString, static_cast<int>(bHasZ), static_cast<int>(bHasM));
        eOGRArrowGeomEncodingOut = OGRArrowGeomEncoding::GEOARROW_LINESTRING;
        return true;
    }

    if (osEncoding == "geoarrow.polygon")
    {
        if (!IsListOfPointType(fieldType, 2, bHasZ, bHasM))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry column %s has a type != list<vertices: "
                     "fixed_size_list<xy: double>[2]>>: %s. "
                     "Handling it as a regular field",
                     fieldName.c_str(), fieldType->name().c_str());
            return false;
        }
        eGeomTypeOut = OGR_GT_SetModifier(wkbPolygon, static_cast<int>(bHasZ),
                                          static_cast<int>(bHasM));
        eOGRArrowGeomEncodingOut = OGRArrowGeomEncoding::GEOARROW_POLYGON;
        return true;
    }

    if (osEncoding == "geoarrow.multipoint")
    {
        if (!IsListOfPointType(fieldType, 1, bHasZ, bHasM))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry column %s has a type != fixed_size_list<xy: "
                     "double>[2]>: %s. "
                     "Handling it as a regular field",
                     fieldName.c_str(), fieldType->name().c_str());
            return false;
        }
        eGeomTypeOut = OGR_GT_SetModifier(
            wkbMultiPoint, static_cast<int>(bHasZ), static_cast<int>(bHasM));
        eOGRArrowGeomEncodingOut = OGRArrowGeomEncoding::GEOARROW_MULTIPOINT;
        return true;
    }

    if (osEncoding == "geoarrow.multilinestring")
    {
        if (!IsListOfPointType(fieldType, 2, bHasZ, bHasM))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry column %s has a type != list<vertices: "
                     "fixed_size_list<xy: double>[2]>>: %s. "
                     "Handling it as a regular field",
                     fieldName.c_str(), fieldType->name().c_str());
            return false;
        }
        eGeomTypeOut =
            OGR_GT_SetModifier(wkbMultiLineString, static_cast<int>(bHasZ),
                               static_cast<int>(bHasM));
        eOGRArrowGeomEncodingOut =
            OGRArrowGeomEncoding::GEOARROW_MULTILINESTRING;
        return true;
    }

    if (osEncoding == "geoarrow.multipolygon")
    {
        if (!IsListOfPointType(fieldType, 3, bHasZ, bHasM))
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Geometry column %s has a type != list<polygons: list<rings: "
                "list<vertices: fixed_size_list<xy: double>[2]>>>: %s. "
                "Handling it as a regular field",
                fieldName.c_str(), fieldType->name().c_str());
            return false;
        }
        eGeomTypeOut = OGR_GT_SetModifier(
            wkbMultiPolygon, static_cast<int>(bHasZ), static_cast<int>(bHasM));
        eOGRArrowGeomEncodingOut = OGRArrowGeomEncoding::GEOARROW_MULTIPOLYGON;
        return true;
    }

    CPLError(CE_Warning, CPLE_AppDefined,
             "Geometry column %s uses a unhandled encoding: %s. "
             "Handling it as a regular field",
             fieldName.c_str(), osEncoding.c_str());
    return false;
}

/************************************************************************/
/*                    GetGeometryTypeFromString()                       */
/************************************************************************/

inline OGRwkbGeometryType
OGRArrowLayer::GetGeometryTypeFromString(const std::string &osType)
{
    OGRwkbGeometryType eGeomType = wkbUnknown;
    OGRReadWKTGeometryType(osType.c_str(), &eGeomType);
    if (eGeomType == wkbUnknown && !osType.empty())
    {
        CPLDebug("ARROW", "Unknown geometry type: %s", osType.c_str());
    }
    return eGeomType;
}

/************************************************************************/
/*                             ReadWKBUInt32()                          */
/************************************************************************/

inline uint32_t ReadWKBUInt32(const uint8_t *data, OGRwkbByteOrder eByteOrder,
                              size_t &iOffset)
{
    uint32_t v;
    memcpy(&v, data + iOffset, sizeof(v));
    iOffset += sizeof(v);
    if (OGR_SWAP(eByteOrder))
    {
        CPL_SWAP32PTR(&v);
    }
    return v;
}

/************************************************************************/
/*                         ReadWKBPointSequence()                       */
/************************************************************************/

inline bool ReadWKBPointSequence(const uint8_t *data, size_t size,
                                 OGRwkbByteOrder eByteOrder, int nDim,
                                 size_t &iOffset, OGREnvelope &sEnvelope)
{
    const uint32_t nPoints = ReadWKBUInt32(data, eByteOrder, iOffset);
    if (nPoints > (size - iOffset) / (nDim * sizeof(double)))
        return false;
    double dfX = 0;
    double dfY = 0;
    for (uint32_t j = 0; j < nPoints; j++)
    {
        memcpy(&dfX, data + iOffset, sizeof(double));
        memcpy(&dfY, data + iOffset + sizeof(double), sizeof(double));
        iOffset += nDim * sizeof(double);
        if (OGR_SWAP(eByteOrder))
        {
            CPL_SWAP64PTR(&dfX);
            CPL_SWAP64PTR(&dfY);
        }
        sEnvelope.MinX = std::min(sEnvelope.MinX, dfX);
        sEnvelope.MinY = std::min(sEnvelope.MinY, dfY);
        sEnvelope.MaxX = std::max(sEnvelope.MaxX, dfX);
        sEnvelope.MaxY = std::max(sEnvelope.MaxY, dfY);
    }
    return true;
}

/************************************************************************/
/*                         ReadWKBRingSequence()                        */
/************************************************************************/

inline bool ReadWKBRingSequence(const uint8_t *data, size_t size,
                                OGRwkbByteOrder eByteOrder, int nDim,
                                size_t &iOffset, OGREnvelope &sEnvelope)
{
    const uint32_t nRings = ReadWKBUInt32(data, eByteOrder, iOffset);
    if (nRings > (size - iOffset) / sizeof(uint32_t))
        return false;
    for (uint32_t i = 0; i < nRings; i++)
    {
        if (iOffset + sizeof(uint32_t) > size)
            return false;
        if (!ReadWKBPointSequence(data, size, eByteOrder, nDim, iOffset,
                                  sEnvelope))
            return false;
    }
    return true;
}

/************************************************************************/
/*                          ReadWKBBoundingBox()                        */
/************************************************************************/

constexpr uint32_t WKB_PREFIX_SIZE = 1 + sizeof(uint32_t);
constexpr uint32_t MIN_WKB_SIZE = WKB_PREFIX_SIZE + sizeof(uint32_t);

static bool ReadWKBBoundingBoxInternal(const uint8_t *data, size_t size,
                                       size_t &iOffset, OGREnvelope &sEnvelope,
                                       int nRec)
{
    if (size - iOffset < MIN_WKB_SIZE)
        return false;
    const int nByteOrder = DB2_V72_FIX_BYTE_ORDER(data[iOffset]);
    if (!(nByteOrder == wkbXDR || nByteOrder == wkbNDR))
        return false;
    const OGRwkbByteOrder eByteOrder = static_cast<OGRwkbByteOrder>(nByteOrder);

    OGRwkbGeometryType eGeometryType = wkbUnknown;
    OGRReadWKBGeometryType(data + iOffset, wkbVariantIso, &eGeometryType);
    iOffset += 5;
    const auto eFlatType = wkbFlatten(eGeometryType);
    const int nDim = 2 + (OGR_GT_HasZ(eGeometryType) ? 1 : 0) +
                     (OGR_GT_HasM(eGeometryType) ? 1 : 0);

    if (eFlatType == wkbPoint)
    {
        if (size - iOffset < nDim * sizeof(double))
            return false;
        double dfX = 0;
        double dfY = 0;
        memcpy(&dfX, data + iOffset, sizeof(double));
        memcpy(&dfY, data + iOffset + sizeof(double), sizeof(double));
        iOffset += nDim * sizeof(double);
        if (OGR_SWAP(eByteOrder))
        {
            CPL_SWAP64PTR(&dfX);
            CPL_SWAP64PTR(&dfY);
        }
        sEnvelope.MinX = dfX;
        sEnvelope.MinY = dfY;
        sEnvelope.MaxX = dfX;
        sEnvelope.MaxY = dfY;
        return true;
    }

    if (eFlatType == wkbLineString || eFlatType == wkbCircularString)
    {
        sEnvelope.MinX = std::numeric_limits<double>::max();
        sEnvelope.MinY = std::numeric_limits<double>::max();
        sEnvelope.MaxX = -std::numeric_limits<double>::max();
        sEnvelope.MaxY = -std::numeric_limits<double>::max();

        return ReadWKBPointSequence(data, size, eByteOrder, nDim, iOffset,
                                    sEnvelope);
    }

    if (eFlatType == wkbPolygon)
    {
        sEnvelope.MinX = std::numeric_limits<double>::max();
        sEnvelope.MinY = std::numeric_limits<double>::max();
        sEnvelope.MaxX = -std::numeric_limits<double>::max();
        sEnvelope.MaxY = -std::numeric_limits<double>::max();

        return ReadWKBRingSequence(data, size, eByteOrder, nDim, iOffset,
                                   sEnvelope);
    }

    if (eFlatType == wkbMultiPoint)
    {
        sEnvelope.MinX = std::numeric_limits<double>::max();
        sEnvelope.MinY = std::numeric_limits<double>::max();
        sEnvelope.MaxX = -std::numeric_limits<double>::max();
        sEnvelope.MaxY = -std::numeric_limits<double>::max();

        uint32_t nParts = ReadWKBUInt32(data, eByteOrder, iOffset);
        if (nParts >
            (size - iOffset) / (WKB_PREFIX_SIZE + nDim * sizeof(double)))
            return false;
        double dfX = 0;
        double dfY = 0;
        for (uint32_t k = 0; k < nParts; k++)
        {
            iOffset += WKB_PREFIX_SIZE;
            memcpy(&dfX, data + iOffset, sizeof(double));
            memcpy(&dfY, data + iOffset + sizeof(double), sizeof(double));
            iOffset += nDim * sizeof(double);
            if (OGR_SWAP(eByteOrder))
            {
                CPL_SWAP64PTR(&dfX);
                CPL_SWAP64PTR(&dfY);
            }
            sEnvelope.MinX = std::min(sEnvelope.MinX, dfX);
            sEnvelope.MinY = std::min(sEnvelope.MinY, dfY);
            sEnvelope.MaxX = std::max(sEnvelope.MaxX, dfX);
            sEnvelope.MaxY = std::max(sEnvelope.MaxY, dfY);
        }
        return true;
    }

    if (eFlatType == wkbMultiLineString)
    {
        sEnvelope.MinX = std::numeric_limits<double>::max();
        sEnvelope.MinY = std::numeric_limits<double>::max();
        sEnvelope.MaxX = -std::numeric_limits<double>::max();
        sEnvelope.MaxY = -std::numeric_limits<double>::max();

        const uint32_t nParts = ReadWKBUInt32(data, eByteOrder, iOffset);
        if (nParts > (size - iOffset) / MIN_WKB_SIZE)
            return false;
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (iOffset + MIN_WKB_SIZE > size)
                return false;
            iOffset += WKB_PREFIX_SIZE;
            if (!ReadWKBPointSequence(data, size, eByteOrder, nDim, iOffset,
                                      sEnvelope))
                return false;
        }
        return true;
    }

    if (eFlatType == wkbMultiPolygon)
    {
        sEnvelope.MinX = std::numeric_limits<double>::max();
        sEnvelope.MinY = std::numeric_limits<double>::max();
        sEnvelope.MaxX = -std::numeric_limits<double>::max();
        sEnvelope.MaxY = -std::numeric_limits<double>::max();

        const uint32_t nParts = ReadWKBUInt32(data, eByteOrder, iOffset);
        if (nParts > (size - iOffset) / MIN_WKB_SIZE)
            return false;
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (iOffset + MIN_WKB_SIZE > size)
                return false;
            CPLAssert(data[iOffset] == eByteOrder);
            iOffset += WKB_PREFIX_SIZE;
            if (!ReadWKBRingSequence(data, size, eByteOrder, nDim, iOffset,
                                     sEnvelope))
                return false;
        }
        return true;
    }

    if (eFlatType == wkbGeometryCollection || eFlatType == wkbCompoundCurve ||
        eFlatType == wkbCurvePolygon || eFlatType == wkbMultiCurve ||
        eFlatType == wkbMultiSurface)
    {
        if (nRec == 128)
            return false;
        sEnvelope.MinX = std::numeric_limits<double>::max();
        sEnvelope.MinY = std::numeric_limits<double>::max();
        sEnvelope.MaxX = -std::numeric_limits<double>::max();
        sEnvelope.MaxY = -std::numeric_limits<double>::max();

        const uint32_t nParts = ReadWKBUInt32(data, eByteOrder, iOffset);
        if (nParts > (size - iOffset) / MIN_WKB_SIZE)
            return false;
        OGREnvelope sEnvelopeSubGeom;
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (!ReadWKBBoundingBoxInternal(data, size, iOffset,
                                            sEnvelopeSubGeom, nRec + 1))
                return false;
            sEnvelope.Merge(sEnvelopeSubGeom);
        }
        return true;
    }

    return false;
}

inline bool OGRArrowLayer::ReadWKBBoundingBox(const uint8_t *data, size_t size,
                                              OGREnvelope &sEnvelope)
{
    size_t iOffset = 0;
    return ReadWKBBoundingBoxInternal(data, size, iOffset, sEnvelope, 0);
}

/************************************************************************/
/*                            ReadList()                                */
/************************************************************************/

template <class OGRType, class ArrowType, class ArrayType>
static void ReadList(OGRFeature *poFeature, int i, int64_t nIdxInBatch,
                     const ArrayType *array)
{
    const auto values = std::static_pointer_cast<ArrowType>(array->values());
    const auto nIdxStart = array->value_offset(nIdxInBatch);
    const int nCount = array->value_length(nIdxInBatch);
    std::vector<OGRType> aValues;
    aValues.reserve(nCount);
    for (int k = 0; k < nCount; k++)
    {
        aValues.push_back(static_cast<OGRType>(values->Value(nIdxStart + k)));
    }
    poFeature->SetField(i, nCount, aValues.data());
}

template <class ArrowType, class ArrayType>
static void ReadListDouble(OGRFeature *poFeature, int i, int64_t nIdxInBatch,
                           const ArrayType *array)
{
    const auto values = std::static_pointer_cast<ArrowType>(array->values());
    const auto rawValues = values->raw_values();
    const auto nIdxStart = array->value_offset(nIdxInBatch);
    const int nCount = array->value_length(nIdxInBatch);
    std::vector<double> aValues;
    aValues.reserve(nCount);
    for (int k = 0; k < nCount; k++)
    {
        if (values->IsNull(nIdxStart + k))
            aValues.push_back(std::numeric_limits<double>::quiet_NaN());
        else
            aValues.push_back(rawValues[nIdxStart + k]);
    }
    poFeature->SetField(i, nCount, aValues.data());
}

template <class ArrayType>
static void ReadList(OGRFeature *poFeature, int i, int64_t nIdxInBatch,
                     const ArrayType *array, arrow::Type::type valueTypeId)
{
    switch (valueTypeId)
    {
        case arrow::Type::BOOL:
        {
            ReadList<int, arrow::BooleanArray>(poFeature, i, nIdxInBatch,
                                               array);
            break;
        }
        case arrow::Type::UINT8:
        {
            ReadList<int, arrow::UInt8Array>(poFeature, i, nIdxInBatch, array);
            break;
        }
        case arrow::Type::INT8:
        {
            ReadList<int, arrow::Int8Array>(poFeature, i, nIdxInBatch, array);
            break;
        }
        case arrow::Type::UINT16:
        {
            ReadList<int, arrow::UInt16Array>(poFeature, i, nIdxInBatch, array);
            break;
        }
        case arrow::Type::INT16:
        {
            ReadList<int, arrow::Int16Array>(poFeature, i, nIdxInBatch, array);
            break;
        }
        case arrow::Type::INT32:
        {
            ReadList<int, arrow::Int32Array>(poFeature, i, nIdxInBatch, array);
            break;
        }
        case arrow::Type::UINT32:
        {
            ReadList<GIntBig, arrow::UInt32Array>(poFeature, i, nIdxInBatch,
                                                  array);
            break;
        }
        case arrow::Type::INT64:
        {
            ReadList<GIntBig, arrow::Int64Array>(poFeature, i, nIdxInBatch,
                                                 array);
            break;
        }
        case arrow::Type::UINT64:
        {
            ReadList<double, arrow::UInt64Array>(poFeature, i, nIdxInBatch,
                                                 array);
            break;
        }
        case arrow::Type::HALF_FLOAT:
        {
            ReadListDouble<arrow::HalfFloatArray>(poFeature, i, nIdxInBatch,
                                                  array);
            break;
        }
        case arrow::Type::FLOAT:
        {
            ReadListDouble<arrow::FloatArray>(poFeature, i, nIdxInBatch, array);
            break;
        }
        case arrow::Type::DOUBLE:
        {
            ReadListDouble<arrow::DoubleArray>(poFeature, i, nIdxInBatch,
                                               array);
            break;
        }
        case arrow::Type::STRING:
        {
            const auto values =
                std::static_pointer_cast<arrow::StringArray>(array->values());
            const auto nIdxStart = array->value_offset(nIdxInBatch);
            const int nCount = array->value_length(nIdxInBatch);
            CPLStringList aosList;
            for (int k = 0; k < nCount; k++)
            {
                if (values->IsNull(nIdxStart + k))
                    aosList.AddString(
                        "");  // we cannot have null strings in a list
                else
                    aosList.AddString(values->GetString(nIdxStart + k).c_str());
            }
            poFeature->SetField(i, aosList.List());
            break;
        }

        default:
            break;
    }
}

/************************************************************************/
/*                            ReadMap()                                 */
/************************************************************************/

template <class OGRType, class ArrowType>
static void ReadMap(OGRFeature *poFeature, int i, int64_t nIdxInBatch,
                    const arrow::MapArray *array)
{
    const auto keys =
        std::static_pointer_cast<arrow::StringArray>(array->keys());
    const auto values = std::static_pointer_cast<ArrowType>(array->items());
    const auto nIdxStart = array->value_offset(nIdxInBatch);
    const int nCount = array->value_length(nIdxInBatch);
    CPLJSONObject oRoot;
    for (int k = 0; k < nCount; k++)
    {
        if (!keys->IsNull(nIdxStart + k))
        {
            const auto osKey = keys->GetString(nIdxStart + k);
            if (!values->IsNull(nIdxStart + k))
                oRoot.Add(osKey,
                          static_cast<OGRType>(values->Value(nIdxStart + k)));
            else
                oRoot.AddNull(osKey);
        }
    }
    poFeature->SetField(
        i, oRoot.Format(CPLJSONObject::PrettyFormat::Plain).c_str());
}

/************************************************************************/
/*                         SetPointsOfLine()                            */
/************************************************************************/

template <bool bHasZ, bool bHasM, int nDim>
void SetPointsOfLine(OGRLineString *poLS,
                     const std::shared_ptr<arrow::DoubleArray> &pointValues,
                     int pointOffset, int numPoints)
{
    if (!bHasZ && !bHasM)
    {
        static_assert(sizeof(OGRRawPoint) == 2 * sizeof(double),
                      "sizeof(OGRRawPoint) == 2 * sizeof(double)");
        poLS->setPoints(numPoints,
                        reinterpret_cast<const OGRRawPoint *>(
                            pointValues->raw_values() + pointOffset));
        return;
    }

    poLS->setNumPoints(numPoints, FALSE);
    for (int k = 0; k < numPoints; k++)
    {
        if (bHasZ)
        {
            if (bHasM)
            {
                poLS->setPoint(k, pointValues->Value(pointOffset + nDim * k),
                               pointValues->Value(pointOffset + nDim * k + 1),
                               pointValues->Value(pointOffset + nDim * k + 2),
                               pointValues->Value(pointOffset + nDim * k + 3));
            }
            else
            {
                poLS->setPoint(k, pointValues->Value(pointOffset + nDim * k),
                               pointValues->Value(pointOffset + nDim * k + 1),
                               pointValues->Value(pointOffset + nDim * k + 2));
            }
        }
        else /* if( bHasM ) */
        {
            poLS->setPointM(k, pointValues->Value(pointOffset + nDim * k),
                            pointValues->Value(pointOffset + nDim * k + 1),
                            pointValues->Value(pointOffset + nDim * k + 2));
        }
    }
}

typedef void (*SetPointsOfLineType)(OGRLineString *,
                                    const std::shared_ptr<arrow::DoubleArray> &,
                                    int, int);

static SetPointsOfLineType GetSetPointsOfLine(bool bHasZ, bool bHasM)
{
    if (bHasZ && bHasM)
        return SetPointsOfLine<true, true, 4>;
    if (bHasZ)
        return SetPointsOfLine<true, false, 3>;
    if (bHasM)
        return SetPointsOfLine<false, true, 3>;
    return SetPointsOfLine<false, false, 2>;
}

/************************************************************************/
/*                            TimestampToOGR()                          */
/************************************************************************/

inline void
OGRArrowLayer::TimestampToOGR(int64_t timestamp,
                              const arrow::TimestampType *timestampType,
                              OGRField *psField)
{
    const auto unit = timestampType->unit();
    double floatingPart = 0;
    if (unit == arrow::TimeUnit::MILLI)
    {
        floatingPart = (timestamp % 1000) / 1e3;
        timestamp /= 1000;
    }
    else if (unit == arrow::TimeUnit::MICRO)
    {
        floatingPart = (timestamp % (1000 * 1000)) / 1e6;
        timestamp /= 1000 * 1000;
    }
    else if (unit == arrow::TimeUnit::NANO)
    {
        floatingPart = (timestamp % (1000 * 1000 * 1000)) / 1e9;
        timestamp /= 1000 * 1000 * 1000;
    }
    int nTZFlag = 0;
    const auto osTZ = timestampType->timezone();
    if (osTZ == "UTC" || osTZ == "Etc/UTC")
    {
        nTZFlag = 100;
    }
    else if (osTZ.size() == 6 && (osTZ[0] == '+' || osTZ[0] == '-') &&
             osTZ[3] == ':')
    {
        int nTZHour = atoi(osTZ.c_str() + 1);
        int nTZMin = atoi(osTZ.c_str() + 4);
        if (nTZHour >= 0 && nTZHour <= 14 && nTZMin >= 0 && nTZMin < 60 &&
            (nTZMin % 15) == 0)
        {
            nTZFlag = (nTZHour * 4) + (nTZMin / 15);
            if (osTZ[0] == '+')
            {
                nTZFlag = 100 + nTZFlag;
                timestamp += nTZHour * 3600 + nTZMin * 60;
            }
            else
            {
                nTZFlag = 100 - nTZFlag;
                timestamp -= nTZHour * 3600 + nTZMin * 60;
            }
        }
    }
    struct tm dt;
    CPLUnixTimeToYMDHMS(timestamp, &dt);
    psField->Date.Year = static_cast<GInt16>(dt.tm_year + 1900);
    psField->Date.Month = static_cast<GByte>(dt.tm_mon + 1);
    psField->Date.Day = static_cast<GByte>(dt.tm_mday);
    psField->Date.Hour = static_cast<GByte>(dt.tm_hour);
    psField->Date.Minute = static_cast<GByte>(dt.tm_min);
    psField->Date.TZFlag = static_cast<GByte>(nTZFlag);
    psField->Date.Second = static_cast<float>(dt.tm_sec + floatingPart);
}

/************************************************************************/
/*                            ReadFeature()                             */
/************************************************************************/

inline OGRFeature *OGRArrowLayer::ReadFeature(
    int64_t nIdxInBatch,
    const std::vector<std::shared_ptr<arrow::Array>> &poColumnArrays) const
{
    OGRFeature *poFeature = new OGRFeature(m_poFeatureDefn);

    if (m_iFIDArrowColumn >= 0)
    {
        const int iCol =
            m_bIgnoredFields ? m_nRequestedFIDColumn : m_iFIDArrowColumn;
        const arrow::Array *array = poColumnArrays[iCol].get();
        if (!array->IsNull(nIdxInBatch))
        {
            if (array->type_id() == arrow::Type::INT64)
            {
                const auto castArray =
                    static_cast<const arrow::Int64Array *>(array);
                poFeature->SetFID(
                    static_cast<GIntBig>(castArray->Value(nIdxInBatch)));
            }
            else if (array->type_id() == arrow::Type::INT32)
            {
                const auto castArray =
                    static_cast<const arrow::Int32Array *>(array);
                poFeature->SetFID(castArray->Value(nIdxInBatch));
            }
        }
    }

    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for (int i = 0; i < nFieldCount; ++i)
    {
        int iCol;
        if (m_bIgnoredFields)
        {
            iCol = m_anMapFieldIndexToArrayIndex[i];
            if (iCol < 0)
                continue;
        }
        else
        {
            iCol = m_anMapFieldIndexToArrowColumn[i][0];
        }

        const arrow::Array *array = poColumnArrays[iCol].get();
        if (array->IsNull(nIdxInBatch))
        {
            poFeature->SetFieldNull(i);
            continue;
        }

        int j = 1;
        bool bSkipToNextField = false;
        while (array->type_id() == arrow::Type::STRUCT)
        {
            const auto castArray =
                static_cast<const arrow::StructArray *>(array);
            const auto &subArrays = castArray->fields();
            CPLAssert(
                j < static_cast<int>(m_anMapFieldIndexToArrowColumn[i].size()));
            const int iArrowSubcol = m_anMapFieldIndexToArrowColumn[i][j];
            j++;
            CPLAssert(iArrowSubcol < static_cast<int>(subArrays.size()));
            array = subArrays[iArrowSubcol].get();
            if (array->IsNull(nIdxInBatch))
            {
                poFeature->SetFieldNull(i);
                bSkipToNextField = true;
                break;
            }
        }
        if (bSkipToNextField)
            continue;

        if (array->type_id() == arrow::Type::DICTIONARY)
        {
            const auto castArray =
                static_cast<const arrow::DictionaryArray *>(array);
            m_poReadFeatureTmpArray =
                castArray->indices();  // does not return a const reference
            array = m_poReadFeatureTmpArray.get();
            if (array->IsNull(nIdxInBatch))
            {
                poFeature->SetFieldNull(i);
                continue;
            }
        }

        switch (array->type_id())
        {
            case arrow::Type::NA:
                break;

            case arrow::Type::BOOL:
            {
                const auto castArray =
                    static_cast<const arrow::BooleanArray *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, castArray->Value(nIdxInBatch));
                break;
            }
            case arrow::Type::UINT8:
            {
                const auto castArray =
                    static_cast<const arrow::UInt8Array *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, castArray->Value(nIdxInBatch));
                break;
            }
            case arrow::Type::INT8:
            {
                const auto castArray =
                    static_cast<const arrow::Int8Array *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, castArray->Value(nIdxInBatch));
                break;
            }
            case arrow::Type::UINT16:
            {
                const auto castArray =
                    static_cast<const arrow::UInt16Array *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, castArray->Value(nIdxInBatch));
                break;
            }
            case arrow::Type::INT16:
            {
                const auto castArray =
                    static_cast<const arrow::Int16Array *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, castArray->Value(nIdxInBatch));
                break;
            }
            case arrow::Type::UINT32:
            {
                const auto castArray =
                    static_cast<const arrow::UInt32Array *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, static_cast<GIntBig>(castArray->Value(nIdxInBatch)));
                break;
            }
            case arrow::Type::INT32:
            {
                const auto castArray =
                    static_cast<const arrow::Int32Array *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, castArray->Value(nIdxInBatch));
                break;
            }
            case arrow::Type::UINT64:
            {
                const auto castArray =
                    static_cast<const arrow::UInt64Array *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, static_cast<double>(castArray->Value(nIdxInBatch)));
                break;
            }
            case arrow::Type::INT64:
            {
                const auto castArray =
                    static_cast<const arrow::Int64Array *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, static_cast<GIntBig>(castArray->Value(nIdxInBatch)));
                break;
            }
            case arrow::Type::HALF_FLOAT:
            {
                const auto castArray =
                    static_cast<const arrow::HalfFloatArray *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, castArray->Value(nIdxInBatch));
                break;
            }
            case arrow::Type::FLOAT:
            {
                const auto castArray =
                    static_cast<const arrow::FloatArray *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, castArray->Value(nIdxInBatch));
                break;
            }
            case arrow::Type::DOUBLE:
            {
                const auto castArray =
                    static_cast<const arrow::DoubleArray *>(array);
                poFeature->SetFieldSameTypeUnsafe(
                    i, castArray->Value(nIdxInBatch));
                break;
            }
            case arrow::Type::STRING:
            {
                const auto castArray =
                    static_cast<const arrow::StringArray *>(array);
                int out_length = 0;
                const uint8_t *data =
                    castArray->GetValue(nIdxInBatch, &out_length);
                char *pszString =
                    static_cast<char *>(CPLMalloc(out_length + 1));
                memcpy(pszString, data, out_length);
                pszString[out_length] = 0;
                poFeature->SetFieldSameTypeUnsafe(i, pszString);
                break;
            }
            case arrow::Type::BINARY:
            {
                const auto castArray =
                    static_cast<const arrow::BinaryArray *>(array);
                int out_length = 0;
                const uint8_t *data =
                    castArray->GetValue(nIdxInBatch, &out_length);
                poFeature->SetField(i, out_length, data);
                break;
            }
            case arrow::Type::FIXED_SIZE_BINARY:
            {
                const auto castArray =
                    static_cast<const arrow::FixedSizeBinaryArray *>(array);
                const uint8_t *data = castArray->GetValue(nIdxInBatch);
                poFeature->SetField(i, castArray->byte_width(), data);
                break;
            }
            case arrow::Type::DATE32:
            {
                // number of days since Epoch
                const auto castArray =
                    static_cast<const arrow::Date32Array *>(array);
                int64_t timestamp =
                    static_cast<int64_t>(castArray->Value(nIdxInBatch)) * 3600 *
                    24;
                struct tm dt;
                CPLUnixTimeToYMDHMS(timestamp, &dt);
                poFeature->SetField(i, dt.tm_year + 1900, dt.tm_mon + 1,
                                    dt.tm_mday, 0, 0, 0);
                break;
            }
            case arrow::Type::DATE64:
            {
                // number of milliseconds since Epoch
                const auto castArray =
                    static_cast<const arrow::Date64Array *>(array);
                int64_t timestamp =
                    static_cast<int64_t>(castArray->Value(nIdxInBatch)) / 1000;
                struct tm dt;
                CPLUnixTimeToYMDHMS(timestamp, &dt);
                poFeature->SetField(i, dt.tm_year + 1900, dt.tm_mon + 1,
                                    dt.tm_mday, 0, 0, 0);
                break;
            }
            case arrow::Type::TIMESTAMP:
            {
                const auto timestampType = static_cast<arrow::TimestampType *>(
                    array->data()->type.get());
                const auto castArray =
                    static_cast<const arrow::Int64Array *>(array);
                const int64_t timestamp = castArray->Value(nIdxInBatch);
                OGRField sField;
                sField.Set.nMarker1 = OGRUnsetMarker;
                sField.Set.nMarker2 = OGRUnsetMarker;
                sField.Set.nMarker3 = OGRUnsetMarker;
                TimestampToOGR(timestamp, timestampType, &sField);
                poFeature->SetField(i, &sField);
                break;
            }
            case arrow::Type::TIME32:
            {
                const auto timestampType =
                    static_cast<arrow::Time32Type *>(array->data()->type.get());
                const auto castArray =
                    static_cast<const arrow::Int32Array *>(array);
                const auto unit = timestampType->unit();
                int value = castArray->Value(nIdxInBatch);
                double floatingPart = 0;
                if (unit == arrow::TimeUnit::MILLI)
                {
                    floatingPart = (value % 1000) / 1e3;
                    value /= 1000;
                }
                const int nHour = value / 3600;
                const int nMinute = (value / 60) % 60;
                const int nSecond = value % 60;
                poFeature->SetField(i, 0, 0, 0, nHour, nMinute,
                                    static_cast<float>(nSecond + floatingPart));
                break;
            }
            case arrow::Type::TIME64:
            {
                const auto castArray =
                    static_cast<const arrow::Time64Array *>(array);
                poFeature->SetField(
                    i, static_cast<GIntBig>(castArray->Value(nIdxInBatch)));
                break;
            }

            case arrow::Type::DECIMAL128:
            {
                const auto castArray =
                    static_cast<const arrow::Decimal128Array *>(array);
                poFeature->SetField(
                    i, CPLAtof(castArray->FormatValue(nIdxInBatch).c_str()));
                break;
            }

            case arrow::Type::DECIMAL256:
            {
                const auto castArray =
                    static_cast<const arrow::Decimal256Array *>(array);
                poFeature->SetField(
                    i, CPLAtof(castArray->FormatValue(nIdxInBatch).c_str()));
                break;
            }

            case arrow::Type::LIST:
            {
                const auto castArray =
                    static_cast<const arrow::ListArray *>(array);
                const auto listType = static_cast<const arrow::ListType *>(
                    array->data()->type.get());
                ReadList(poFeature, i, nIdxInBatch, castArray,
                         listType->value_field()->type()->id());
                break;
            }

            case arrow::Type::FIXED_SIZE_LIST:
            {
                const auto castArray =
                    static_cast<const arrow::FixedSizeListArray *>(array);
                const auto listType =
                    static_cast<const arrow::FixedSizeListType *>(
                        array->data()->type.get());
                ReadList(poFeature, i, nIdxInBatch, castArray,
                         listType->value_field()->type()->id());
                break;
            }

            case arrow::Type::LARGE_STRING:
            {
                const auto castArray =
                    static_cast<const arrow::LargeStringArray *>(array);
                poFeature->SetField(i,
                                    castArray->GetString(nIdxInBatch).c_str());
                break;
            }
            case arrow::Type::LARGE_BINARY:
            {
                const auto castArray =
                    static_cast<const arrow::LargeBinaryArray *>(array);
                arrow::LargeBinaryArray::offset_type out_length = 0;
                const uint8_t *data =
                    castArray->GetValue(nIdxInBatch, &out_length);
                if (out_length <= INT_MAX)
                {
                    poFeature->SetField(i, static_cast<int>(out_length), data);
                }
                else
                {
                    // this is probably the most likely code path if people use
                    // LargeBinary...
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Too large binary: " CPL_FRMT_GUIB " bytes",
                             static_cast<GUIntBig>(out_length));
                }
                break;
            }

            case arrow::Type::MAP:
            {
                const auto castArray =
                    static_cast<const arrow::MapArray *>(array);
                const auto mapType = static_cast<const arrow::MapType *>(
                    array->data()->type.get());
                const auto itemTypeId = mapType->item_type()->id();
                if (mapType->key_type()->id() == arrow::Type::STRING)
                {
                    if (itemTypeId == arrow::Type::BOOL)
                    {
                        ReadMap<bool, arrow::BooleanArray>(
                            poFeature, i, nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::UINT8)
                    {
                        ReadMap<int, arrow::UInt8Array>(poFeature, i,
                                                        nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::INT8)
                    {
                        ReadMap<int, arrow::Int8Array>(poFeature, i,
                                                       nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::UINT16)
                    {
                        ReadMap<int, arrow::UInt16Array>(
                            poFeature, i, nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::INT16)
                    {
                        ReadMap<int, arrow::Int16Array>(poFeature, i,
                                                        nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::UINT32)
                    {
                        ReadMap<GIntBig, arrow::UInt32Array>(
                            poFeature, i, nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::INT32)
                    {
                        ReadMap<int, arrow::Int32Array>(poFeature, i,
                                                        nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::UINT64)
                    {
                        ReadMap<double, arrow::UInt64Array>(
                            poFeature, i, nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::INT64)
                    {
                        ReadMap<GIntBig, arrow::Int64Array>(
                            poFeature, i, nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::FLOAT)
                    {
                        ReadMap<double, arrow::FloatArray>(
                            poFeature, i, nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::DOUBLE)
                    {
                        ReadMap<double, arrow::DoubleArray>(
                            poFeature, i, nIdxInBatch, castArray);
                    }
                    else if (itemTypeId == arrow::Type::STRING)
                    {
                        const auto keys =
                            std::static_pointer_cast<arrow::StringArray>(
                                castArray->keys());
                        const auto values =
                            std::static_pointer_cast<arrow::StringArray>(
                                castArray->items());
                        const auto nIdxStart =
                            castArray->value_offset(nIdxInBatch);
                        const int nCount = castArray->value_length(nIdxInBatch);
                        CPLJSONDocument oDoc;
                        auto oRoot = oDoc.GetRoot();
                        for (int k = 0; k < nCount; k++)
                        {
                            if (!keys->IsNull(nIdxStart + k))
                            {
                                const auto osKey =
                                    keys->GetString(nIdxStart + k);
                                if (!values->IsNull(nIdxStart + k))
                                    oRoot.Add(osKey,
                                              values->GetString(nIdxStart + k));
                                else
                                    oRoot.AddNull(osKey);
                            }
                        }
                        poFeature->SetField(
                            i, oRoot.Format(CPLJSONObject::PrettyFormat::Plain)
                                   .c_str());
                    }
                }
                break;
            }

            // unhandled types
            case arrow::Type::STRUCT:  // should not happen
            case arrow::Type::INTERVAL_MONTHS:
            case arrow::Type::INTERVAL_DAY_TIME:
            case arrow::Type::SPARSE_UNION:
            case arrow::Type::DENSE_UNION:
            case arrow::Type::DICTIONARY:
            case arrow::Type::EXTENSION:
            case arrow::Type::DURATION:
            case arrow::Type::LARGE_LIST:
            case arrow::Type::INTERVAL_MONTH_DAY_NANO:
            case arrow::Type::MAX_ID:
            {
                // Shouldn't happen normally as we should have discarded those
                // fields when creating OGR field definitions
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot read content for field %s",
                         m_poFeatureDefn->GetFieldDefn(i)->GetNameRef());
                break;
            }
        }
    }

    const int nGeomFieldCount = m_poFeatureDefn->GetGeomFieldCount();
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        int iCol;
        if (m_bIgnoredFields)
        {
            iCol = m_anMapGeomFieldIndexToArrayIndex[i];
            if (iCol < 0)
                continue;
        }
        else
        {
            iCol = m_anMapGeomFieldIndexToArrowColumn[i];
        }

        const auto array = poColumnArrays[iCol].get();
        auto poGeometry = ReadGeometry(i, array, nIdxInBatch);
        if (poGeometry)
        {
            const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
            if (wkbFlatten(poGeometry->getGeometryType()) == wkbLineString &&
                wkbFlatten(poGeomFieldDefn->GetType()) == wkbMultiLineString)
            {
                poGeometry =
                    OGRGeometryFactory::forceToMultiLineString(poGeometry);
            }
            else if (wkbFlatten(poGeometry->getGeometryType()) == wkbPolygon &&
                     wkbFlatten(poGeomFieldDefn->GetType()) == wkbMultiPolygon)
            {
                poGeometry =
                    OGRGeometryFactory::forceToMultiPolygon(poGeometry);
            }
            if (OGR_GT_HasZ(poGeomFieldDefn->GetType()) && !poGeometry->Is3D())
            {
                poGeometry->set3D(true);
            }
            poFeature->SetGeomFieldDirectly(i, poGeometry);
        }
    }

    return poFeature;
}

/************************************************************************/
/*                           ReadGeometry()                             */
/************************************************************************/

inline OGRGeometry *OGRArrowLayer::ReadGeometry(int iGeomField,
                                                const arrow::Array *array,
                                                int64_t nIdxInBatch) const
{
    if (array->IsNull(nIdxInBatch))
    {
        return nullptr;
    }
    OGRGeometry *poGeometry = nullptr;
    const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(iGeomField);
    const auto eGeomType = poGeomFieldDefn->GetType();
    const bool bHasZ = CPL_TO_BOOL(OGR_GT_HasZ(eGeomType));
    const bool bHasM = CPL_TO_BOOL(OGR_GT_HasM(eGeomType));
    const int nDim = 2 + (bHasZ ? 1 : 0) + (bHasM ? 1 : 0);

    const auto CreatePoint =
        [bHasZ, bHasM](const std::shared_ptr<arrow::DoubleArray> &pointValues,
                       int pointOffset)
    {
        if (bHasZ)
        {
            if (bHasM)
            {
                return new OGRPoint(pointValues->Value(pointOffset),
                                    pointValues->Value(pointOffset + 1),
                                    pointValues->Value(pointOffset + 2),
                                    pointValues->Value(pointOffset + 3));
            }
            else
            {
                return new OGRPoint(pointValues->Value(pointOffset),
                                    pointValues->Value(pointOffset + 1),
                                    pointValues->Value(pointOffset + 2));
            }
        }
        else if (bHasM)
        {
            return OGRPoint::createXYM(pointValues->Value(pointOffset),
                                       pointValues->Value(pointOffset + 1),
                                       pointValues->Value(pointOffset + 2));
        }
        else
        {
            return new OGRPoint(pointValues->Value(pointOffset),
                                pointValues->Value(pointOffset + 1));
        }
    };

    switch (m_aeGeomEncoding[iGeomField])
    {
        case OGRArrowGeomEncoding::WKB:
        {
            CPLAssert(array->type_id() == arrow::Type::BINARY);
            const auto castArray =
                static_cast<const arrow::BinaryArray *>(array);
            int out_length = 0;
            const uint8_t *data = castArray->GetValue(nIdxInBatch, &out_length);
            if (OGRGeometryFactory::createFromWkb(
                    data, poGeomFieldDefn->GetSpatialRef(), &poGeometry,
                    out_length) == OGRERR_NONE)
            {
#ifdef DEBUG_ReadWKBBoundingBox
                OGREnvelope sEnvelopeFromWKB;
                bool bRet =
                    ReadWKBBoundingBox(data, out_length, sEnvelopeFromWKB);
                CPLAssert(bRet);
                OGREnvelope sEnvelopeFromGeom;
                poGeometry->getEnvelope(&sEnvelopeFromGeom);
                CPLAssert(sEnvelopeFromWKB == sEnvelopeFromGeom);
#endif
            }
            break;
        }

        case OGRArrowGeomEncoding::WKT:
        {
            CPLAssert(array->type_id() == arrow::Type::STRING);
            const auto castArray =
                static_cast<const arrow::StringArray *>(array);
            const auto osWKT = castArray->GetString(nIdxInBatch);
            OGRGeometryFactory::createFromWkt(
                osWKT.c_str(), poGeomFieldDefn->GetSpatialRef(), &poGeometry);
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_GENERIC:
        {
            CPLAssert(false);
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_POINT:
        {
            CPLAssert(array->type_id() == arrow::Type::FIXED_SIZE_LIST);
            const auto listArray =
                static_cast<const arrow::FixedSizeListArray *>(array);
            CPLAssert(listArray->values()->type_id() == arrow::Type::DOUBLE);
            const auto pointValues =
                std::static_pointer_cast<arrow::DoubleArray>(
                    listArray->values());
            if (!pointValues->IsNull(nDim * nIdxInBatch))
            {
                poGeometry = CreatePoint(pointValues,
                                         static_cast<int>(nDim * nIdxInBatch));
                poGeometry->assignSpatialReference(
                    poGeomFieldDefn->GetSpatialRef());
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_LINESTRING:
        {
            CPLAssert(array->type_id() == arrow::Type::LIST);
            const auto listArray = static_cast<const arrow::ListArray *>(array);
            CPLAssert(listArray->values()->type_id() ==
                      arrow::Type::FIXED_SIZE_LIST);
            const auto listOfPointsValues =
                std::static_pointer_cast<arrow::FixedSizeListArray>(
                    listArray->values());
            CPLAssert(listOfPointsValues->values()->type_id() ==
                      arrow::Type::DOUBLE);
            const auto pointValues =
                std::static_pointer_cast<arrow::DoubleArray>(
                    listOfPointsValues->values());
            const auto nPoints = listArray->value_length(nIdxInBatch);
            const auto nPointOffset =
                listArray->value_offset(nIdxInBatch) * nDim;
            auto poLineString = new OGRLineString();
            poGeometry = poLineString;
            poGeometry->assignSpatialReference(
                poGeomFieldDefn->GetSpatialRef());
            if (nPoints)
            {
                GetSetPointsOfLine(bHasZ, bHasM)(poLineString, pointValues,
                                                 nPointOffset, nPoints);
            }
            else
            {
                poGeometry->set3D(bHasZ);
                poGeometry->setMeasured(bHasM);
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_POLYGON:
        {
            CPLAssert(array->type_id() == arrow::Type::LIST);
            const auto listOfRingsArray =
                static_cast<const arrow::ListArray *>(array);
            CPLAssert(listOfRingsArray->values()->type_id() ==
                      arrow::Type::LIST);
            const auto listOfRingsValues =
                std::static_pointer_cast<arrow::ListArray>(
                    listOfRingsArray->values());
            CPLAssert(listOfRingsValues->values()->type_id() ==
                      arrow::Type::FIXED_SIZE_LIST);
            const auto listOfPointsValues =
                std::static_pointer_cast<arrow::FixedSizeListArray>(
                    listOfRingsValues->values());
            CPLAssert(listOfPointsValues->values()->type_id() ==
                      arrow::Type::DOUBLE);
            const auto pointValues =
                std::static_pointer_cast<arrow::DoubleArray>(
                    listOfPointsValues->values());
            const auto setPointsFun = GetSetPointsOfLine(bHasZ, bHasM);
            const auto nRings = listOfRingsArray->value_length(nIdxInBatch);
            const auto nRingOffset =
                listOfRingsArray->value_offset(nIdxInBatch);
            auto poPoly = new OGRPolygon();
            poGeometry = poPoly;
            poGeometry->assignSpatialReference(
                poGeomFieldDefn->GetSpatialRef());
            for (auto k = decltype(nRings){0}; k < nRings; k++)
            {
                const auto nPoints =
                    listOfRingsValues->value_length(nRingOffset + k);
                const auto nPointOffset =
                    listOfRingsValues->value_offset(nRingOffset + k) * nDim;
                auto poRing = new OGRLinearRing();
                if (nPoints)
                {
                    setPointsFun(poRing, pointValues, nPointOffset, nPoints);
                }
                poPoly->addRingDirectly(poRing);
            }
            if (poGeometry->IsEmpty())
            {
                poGeometry->set3D(bHasZ);
                poGeometry->setMeasured(bHasM);
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_MULTIPOINT:
        {
            CPLAssert(array->type_id() == arrow::Type::LIST);
            const auto listArray = static_cast<const arrow::ListArray *>(array);
            CPLAssert(listArray->values()->type_id() ==
                      arrow::Type::FIXED_SIZE_LIST);
            const auto listOfPointsValues =
                std::static_pointer_cast<arrow::FixedSizeListArray>(
                    listArray->values());
            CPLAssert(listOfPointsValues->values()->type_id() ==
                      arrow::Type::DOUBLE);
            const auto pointValues =
                std::static_pointer_cast<arrow::DoubleArray>(
                    listOfPointsValues->values());
            const auto nPoints = listArray->value_length(nIdxInBatch);
            const auto nPointOffset =
                listArray->value_offset(nIdxInBatch) * nDim;
            auto poMultiPoint = new OGRMultiPoint();
            poGeometry = poMultiPoint;
            poGeometry->assignSpatialReference(
                poGeomFieldDefn->GetSpatialRef());
            for (auto k = decltype(nPoints){0}; k < nPoints; k++)
            {
                poMultiPoint->addGeometryDirectly(
                    CreatePoint(pointValues, nPointOffset + k * nDim));
            }
            if (poGeometry->IsEmpty())
            {
                poGeometry->set3D(bHasZ);
                poGeometry->setMeasured(bHasM);
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_MULTILINESTRING:
        {
            CPLAssert(array->type_id() == arrow::Type::LIST);
            const auto listOfStringsArray =
                static_cast<const arrow::ListArray *>(array);
            CPLAssert(listOfStringsArray->values()->type_id() ==
                      arrow::Type::LIST);
            const auto listOfStringsValues =
                std::static_pointer_cast<arrow::ListArray>(
                    listOfStringsArray->values());
            CPLAssert(listOfStringsValues->values()->type_id() ==
                      arrow::Type::FIXED_SIZE_LIST);
            const auto listOfPointsValues =
                std::static_pointer_cast<arrow::FixedSizeListArray>(
                    listOfStringsValues->values());
            CPLAssert(listOfPointsValues->values()->type_id() ==
                      arrow::Type::DOUBLE);
            const auto pointValues =
                std::static_pointer_cast<arrow::DoubleArray>(
                    listOfPointsValues->values());
            const auto setPointsFun = GetSetPointsOfLine(bHasZ, bHasM);
            const auto nStrings = listOfStringsArray->value_length(nIdxInBatch);
            const auto nRingOffset =
                listOfStringsArray->value_offset(nIdxInBatch);
            auto poMLS = new OGRMultiLineString();
            poGeometry = poMLS;
            poGeometry->assignSpatialReference(
                poGeomFieldDefn->GetSpatialRef());
            for (auto k = decltype(nStrings){0}; k < nStrings; k++)
            {
                const auto nPoints =
                    listOfStringsValues->value_length(nRingOffset + k);
                const auto nPointOffset =
                    listOfStringsValues->value_offset(nRingOffset + k) * nDim;
                auto poLS = new OGRLineString();
                if (nPoints)
                {
                    setPointsFun(poLS, pointValues, nPointOffset, nPoints);
                }
                poMLS->addGeometryDirectly(poLS);
            }
            if (poGeometry->IsEmpty())
            {
                poGeometry->set3D(bHasZ);
                poGeometry->setMeasured(bHasM);
            }
            break;
        }

        case OGRArrowGeomEncoding::GEOARROW_MULTIPOLYGON:
        {
            CPLAssert(array->type_id() == arrow::Type::LIST);
            const auto listOfPartsArray =
                static_cast<const arrow::ListArray *>(array);
            CPLAssert(listOfPartsArray->values()->type_id() ==
                      arrow::Type::LIST);
            const auto listOfPartsValues =
                std::static_pointer_cast<arrow::ListArray>(
                    listOfPartsArray->values());
            CPLAssert(listOfPartsValues->values()->type_id() ==
                      arrow::Type::LIST);
            const auto listOfRingsValues =
                std::static_pointer_cast<arrow::ListArray>(
                    listOfPartsValues->values());
            CPLAssert(listOfRingsValues->values()->type_id() ==
                      arrow::Type::FIXED_SIZE_LIST);
            const auto listOfPointsValues =
                std::static_pointer_cast<arrow::FixedSizeListArray>(
                    listOfRingsValues->values());
            CPLAssert(listOfPointsValues->values()->type_id() ==
                      arrow::Type::DOUBLE);
            const auto pointValues =
                std::static_pointer_cast<arrow::DoubleArray>(
                    listOfPointsValues->values());
            auto poMP = new OGRMultiPolygon();
            poGeometry = poMP;
            poGeometry->assignSpatialReference(
                poGeomFieldDefn->GetSpatialRef());
            const auto setPointsFun = GetSetPointsOfLine(bHasZ, bHasM);
            const auto nParts = listOfPartsArray->value_length(nIdxInBatch);
            const auto nPartOffset =
                listOfPartsArray->value_offset(nIdxInBatch);
            for (auto j = decltype(nParts){0}; j < nParts; j++)
            {
                const auto nRings =
                    listOfPartsValues->value_length(nPartOffset + j);
                const auto nRingOffset =
                    listOfPartsValues->value_offset(nPartOffset + j);
                auto poPoly = new OGRPolygon();
                for (auto k = decltype(nRings){0}; k < nRings; k++)
                {
                    const auto nPoints =
                        listOfRingsValues->value_length(nRingOffset + k);
                    const auto nPointOffset =
                        listOfRingsValues->value_offset(nRingOffset + k) * nDim;
                    auto poRing = new OGRLinearRing();
                    if (nPoints)
                    {
                        setPointsFun(poRing, pointValues, nPointOffset,
                                     nPoints);
                    }
                    poPoly->addRingDirectly(poRing);
                }
                poMP->addGeometryDirectly(poPoly);
            }
            if (poGeometry->IsEmpty())
            {
                poGeometry->set3D(bHasZ);
                poGeometry->setMeasured(bHasM);
            }
            break;
        }
    }
    return poGeometry;
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

inline void OGRArrowLayer::ResetReading()
{
    m_bEOF = false;
    m_nFeatureIdx = 0;
    m_nIdxInBatch = 0;
    m_poReadFeatureTmpArray.reset();
    if (m_iRecordBatch != 0)
    {
        m_iRecordBatch = -1;
        m_poBatch.reset();
        m_poBatchColumns.clear();
    }
}

/***********************************************************************/
/*                        GetColumnSubNode()                           */
/***********************************************************************/

static const swq_expr_node *GetColumnSubNode(const swq_expr_node *poNode)
{
    if (poNode->eNodeType == SNT_OPERATION && poNode->nSubExprCount == 2)
    {
        if (poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN)
            return poNode->papoSubExpr[0];
        if (poNode->papoSubExpr[1]->eNodeType == SNT_COLUMN)
            return poNode->papoSubExpr[1];
    }
    return nullptr;
}

/***********************************************************************/
/*                        GetConstantSubNode()                         */
/***********************************************************************/

static const swq_expr_node *GetConstantSubNode(const swq_expr_node *poNode)
{
    if (poNode->eNodeType == SNT_OPERATION && poNode->nSubExprCount == 2)
    {
        if (poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT)
            return poNode->papoSubExpr[1];
        if (poNode->papoSubExpr[0]->eNodeType == SNT_CONSTANT)
            return poNode->papoSubExpr[0];
    }
    return nullptr;
}

/***********************************************************************/
/*                           IsComparisonOp()                          */
/***********************************************************************/

static bool IsComparisonOp(int op)
{
    return (op == SWQ_EQ || op == SWQ_NE || op == SWQ_LT || op == SWQ_LE ||
            op == SWQ_GT || op == SWQ_GE);
}

/***********************************************************************/
/*                     FillTargetValueFromSrcExpr()                    */
/***********************************************************************/

static bool FillTargetValueFromSrcExpr(const OGRFieldDefn *poFieldDefn,
                                       OGRArrowLayer::Constraint *psConstraint,
                                       const swq_expr_node *poSrcValue)
{
    switch (poFieldDefn->GetType())
    {
        case OFTInteger:
            psConstraint->eType = OGRArrowLayer::Constraint::Type::Integer;
            if (poSrcValue->field_type == SWQ_FLOAT)
                psConstraint->sValue.Integer =
                    static_cast<int>(poSrcValue->float_value);
            else
                psConstraint->sValue.Integer =
                    static_cast<int>(poSrcValue->int_value);
            psConstraint->osValue =
                std::to_string(psConstraint->sValue.Integer);
            break;

        case OFTInteger64:
            psConstraint->eType = OGRArrowLayer::Constraint::Type::Integer64;
            if (poSrcValue->field_type == SWQ_FLOAT)
                psConstraint->sValue.Integer64 =
                    static_cast<GIntBig>(poSrcValue->float_value);
            else
                psConstraint->sValue.Integer64 = poSrcValue->int_value;
            psConstraint->osValue =
                std::to_string(psConstraint->sValue.Integer64);
            break;

        case OFTReal:
            psConstraint->eType = OGRArrowLayer::Constraint::Type::Real;
            psConstraint->sValue.Real = poSrcValue->float_value;
            psConstraint->osValue = std::to_string(psConstraint->sValue.Real);
            break;

        case OFTString:
            psConstraint->eType = OGRArrowLayer::Constraint::Type::String;
            psConstraint->sValue.String = poSrcValue->string_value;
            psConstraint->osValue = psConstraint->sValue.String;
            break;
#ifdef not_yet_handled
        case OFTDate:
        case OFTTime:
        case OFTDateTime:
            if (poSrcValue->field_type == SWQ_TIMESTAMP ||
                poSrcValue->field_type == SWQ_DATE ||
                poSrcValue->field_type == SWQ_TIME)
            {
                int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMin = 0,
                    nSec = 0;
                if (sscanf(poSrcValue->string_value,
                           "%04d/%02d/%02d %02d:%02d:%02d", &nYear, &nMonth,
                           &nDay, &nHour, &nMin, &nSec) == 6 ||
                    sscanf(poSrcValue->string_value, "%04d/%02d/%02d", &nYear,
                           &nMonth, &nDay) == 3 ||
                    sscanf(poSrcValue->string_value, "%02d:%02d:%02d", &nHour,
                           &nMin, &nSec) == 3)
                {
                    psConstraint->eType =
                        OGRArrowLayer::Constraint::Type::DateTime;
                    psConstraint->sValue.Date.Year = (GInt16)nYear;
                    psConstraint->sValue.Date.Month = (GByte)nMonth;
                    psConstraint->sValue.Date.Day = (GByte)nDay;
                    psConstraint->sValue.Date.Hour = (GByte)nHour;
                    psConstraint->sValue.Date.Minute = (GByte)nMin;
                    psConstraint->sValue.Date.Second = (GByte)nSec;
                    psConstraint->sValue.Date.TZFlag = 0;
                    psConstraint->sValue.Date.Reserved = 0;
                }
                else
                    return false;
            }
            else
                return false;
            break;
#endif
        default:
            return false;
    }
    return true;
}

/***********************************************************************/
/*                     ExploreExprNode()                               */
/***********************************************************************/

inline void OGRArrowLayer::ExploreExprNode(const swq_expr_node *poNode)
{
    const auto AddConstraint = [this](Constraint &constraint)
    {
        if (m_bIgnoredFields)
        {
            constraint.iArrayIdx =
                m_anMapFieldIndexToArrayIndex[constraint.iField];
            if (constraint.iArrayIdx < 0)
                return;
        }
        else
        {
            constraint.iArrayIdx =
                m_anMapFieldIndexToArrowColumn[constraint.iField][0];
        }

        m_asAttributeFilterConstraints.emplace_back(constraint);
    };

    if (poNode->eNodeType == SNT_OPERATION && poNode->nOperation == SWQ_AND &&
        poNode->nSubExprCount == 2)
    {
        ExploreExprNode(poNode->papoSubExpr[0]);
        ExploreExprNode(poNode->papoSubExpr[1]);
    }

    else if (poNode->eNodeType == SNT_OPERATION &&
             IsComparisonOp(poNode->nOperation) && poNode->nSubExprCount == 2)
    {
        const swq_expr_node *poColumn = GetColumnSubNode(poNode);
        const swq_expr_node *poValue = GetConstantSubNode(poNode);
        if (poColumn != nullptr && poValue != nullptr &&
            poColumn->field_index < m_poFeatureDefn->GetFieldCount())
        {
            const OGRFieldDefn *poFieldDefn =
                m_poFeatureDefn->GetFieldDefn(poColumn->field_index);

            Constraint constraint;
            constraint.iField = poColumn->field_index;
            constraint.nOperation = poNode->nOperation;

            if (FillTargetValueFromSrcExpr(poFieldDefn, &constraint, poValue))
            {
                if (poColumn == poNode->papoSubExpr[0])
                {
                    // nothing to do
                }
                else
                {
                    /* If "constant op column", then we must reverse */
                    /* the operator for LE, LT, GE, GT */
                    switch (poNode->nOperation)
                    {
                        case SWQ_LE:
                            constraint.nOperation = SWQ_GE;
                            break;
                        case SWQ_LT:
                            constraint.nOperation = SWQ_GT;
                            break;
                        case SWQ_NE: /* do nothing */;
                            break;
                        case SWQ_EQ: /* do nothing */;
                            break;
                        case SWQ_GE:
                            constraint.nOperation = SWQ_LE;
                            break;
                        case SWQ_GT:
                            constraint.nOperation = SWQ_LT;
                            break;
                        default:
                            CPLAssert(false);
                            break;
                    }
                }

                AddConstraint(constraint);
            }
        }
    }

    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_ISNULL && poNode->nSubExprCount == 1)
    {
        const swq_expr_node *poColumn = poNode->papoSubExpr[0];
        if (poColumn->eNodeType == SNT_COLUMN &&
            poColumn->field_index < m_poFeatureDefn->GetFieldCount())
        {
            Constraint constraint;
            constraint.iField = poColumn->field_index;
            constraint.nOperation = poNode->nOperation;
            AddConstraint(constraint);
        }
    }

    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_OPERATION &&
             poNode->papoSubExpr[0]->nOperation == SWQ_ISNULL &&
             poNode->papoSubExpr[0]->nSubExprCount == 1)
    {
        const swq_expr_node *poColumn = poNode->papoSubExpr[0]->papoSubExpr[0];
        if (poColumn->eNodeType == SNT_COLUMN &&
            poColumn->field_index < m_poFeatureDefn->GetFieldCount())
        {
            Constraint constraint;
            constraint.iField = poColumn->field_index;
            constraint.nOperation = SWQ_ISNOTNULL;
            AddConstraint(constraint);
        }
    }
}

/***********************************************************************/
/*                         SetAttributeFilter()                        */
/***********************************************************************/

inline OGRErr OGRArrowLayer::SetAttributeFilter(const char *pszFilter)
{
    m_asAttributeFilterConstraints.clear();

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszFilter);
    if (eErr != OGRERR_NONE)
        return eErr;

    if (m_poAttrQuery != nullptr)
    {
        if (m_nUseOptimizedAttributeFilter < 0)
        {
            m_nUseOptimizedAttributeFilter = CPLTestBool(CPLGetConfigOption(
                ("OGR_" + GetDriverUCName() + "_OPTIMIZED_ATTRIBUTE_FILTER")
                    .c_str(),
                "YES"));
        }
        if (m_nUseOptimizedAttributeFilter)
        {
            swq_expr_node *poNode =
                static_cast<swq_expr_node *>(m_poAttrQuery->GetSWQExpr());
            poNode->ReplaceBetweenByGEAndLERecurse();
            ExploreExprNode(poNode);
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        ConstraintEvaluator()                         */
/************************************************************************/

namespace
{
template <class T, class U> struct CompareGeneric
{
    static inline bool get(int op, const T &val1, const U &val2)
    {
        switch (op)
        {
            case SWQ_LE:
                return val1 <= val2;
            case SWQ_LT:
                return val1 < val2;
            case SWQ_NE:
                return val1 != val2;
            case SWQ_EQ:
                return val1 == val2;
            case SWQ_GE:
                return val1 >= val2;
            case SWQ_GT:
                return val1 > val2;
                break;
            default:
                CPLAssert(false);
        }
        return true;
    }
};

template <class T, class U> struct Compare
{
};

template <class T> struct Compare<T, T> : public CompareGeneric<T, T>
{
};
template <> struct Compare<int, GIntBig> : public CompareGeneric<int, GIntBig>
{
};
template <> struct Compare<double, GIntBig>
{
    static inline bool get(int op, double val1, GIntBig val2)
    {
        return CompareGeneric<double, double>::get(op, val1,
                                                   static_cast<double>(val2));
    }
};
template <> struct Compare<GIntBig, int> : public CompareGeneric<GIntBig, int>
{
};
template <> struct Compare<double, int> : public CompareGeneric<double, int>
{
};

template <class T>
static bool ConstraintEvaluator(const OGRArrowLayer::Constraint &constraint,
                                const T value)
{
    bool b = false;
    switch (constraint.eType)
    {
        case OGRArrowLayer::Constraint::Type::Integer:
            b = Compare<T, int>::get(constraint.nOperation, value,
                                     constraint.sValue.Integer);
            break;
        case OGRArrowLayer::Constraint::Type::Integer64:
            b = Compare<T, GIntBig>::get(constraint.nOperation, value,
                                         constraint.sValue.Integer64);
            break;
        case OGRArrowLayer::Constraint::Type::Real:
            b = Compare<double, double>::get(constraint.nOperation,
                                             static_cast<double>(value),
                                             constraint.sValue.Real);
            break;
        case OGRArrowLayer::Constraint::Type::String:
            b = Compare<std::string, std::string>::get(constraint.nOperation,
                                                       std::to_string(value),
                                                       constraint.osValue);
            break;
    }
    return b;
}

struct StringView
{
    const char *m_ptr;
    size_t m_len;

    StringView(const char *ptr, size_t len) : m_ptr(ptr), m_len(len)
    {
    }
};

inline bool CompareStr(int op, const StringView &val1, const std::string &val2)
{
    if (op == SWQ_EQ)
    {
        return val1.m_len == val2.size() &&
               memcmp(val1.m_ptr, val2.data(), val1.m_len) == 0;
    }
    const int cmpRes = val2.compare(0, val2.size(), val1.m_ptr, val1.m_len);
    switch (op)
    {
        case SWQ_LE:
            return cmpRes >= 0;
        case SWQ_LT:
            return cmpRes > 0;
        case SWQ_NE:
            return cmpRes != 0;
        // case SWQ_EQ: return cmpRes == 0;
        case SWQ_GE:
            return cmpRes <= 0;
        case SWQ_GT:
            return cmpRes < 0;
            break;
        default:
            CPLAssert(false);
    }
    return true;
}

inline bool ConstraintEvaluator(const OGRArrowLayer::Constraint &constraint,
                                const StringView &value)
{
    return CompareStr(constraint.nOperation, value, constraint.osValue);
}

}  // namespace

/************************************************************************/
/*                 SkipToNextFeatureDueToAttributeFilter()              */
/************************************************************************/

inline bool OGRArrowLayer::SkipToNextFeatureDueToAttributeFilter() const
{
    for (const auto &constraint : m_asAttributeFilterConstraints)
    {
        const arrow::Array *array =
            m_poBatchColumns[constraint.iArrayIdx].get();

        const bool bIsNull = array->IsNull(m_nIdxInBatch);
        if (constraint.nOperation == SWQ_ISNULL)
        {
            if (bIsNull)
            {
                continue;
            }
            return true;
        }
        else if (constraint.nOperation == SWQ_ISNOTNULL)
        {
            if (!bIsNull)
            {
                continue;
            }
            return true;
        }
        else if (bIsNull)
        {
            return true;
        }

        switch (array->type_id())
        {
            case arrow::Type::NA:
                break;

            case arrow::Type::BOOL:
            {
                const auto castArray =
                    static_cast<const arrow::BooleanArray *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<int>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::UINT8:
            {
                const auto castArray =
                    static_cast<const arrow::UInt8Array *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<int>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::INT8:
            {
                const auto castArray =
                    static_cast<const arrow::Int8Array *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<int>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::UINT16:
            {
                const auto castArray =
                    static_cast<const arrow::UInt16Array *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<int>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::INT16:
            {
                const auto castArray =
                    static_cast<const arrow::Int16Array *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<int>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::UINT32:
            {
                const auto castArray =
                    static_cast<const arrow::UInt32Array *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<GIntBig>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::INT32:
            {
                const auto castArray =
                    static_cast<const arrow::Int32Array *>(array);
                if (!ConstraintEvaluator(constraint,
                                         castArray->Value(m_nIdxInBatch)))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::UINT64:
            {
                const auto castArray =
                    static_cast<const arrow::UInt64Array *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<double>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::INT64:
            {
                const auto castArray =
                    static_cast<const arrow::Int64Array *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<GIntBig>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::HALF_FLOAT:
            {
                const auto castArray =
                    static_cast<const arrow::HalfFloatArray *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<double>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::FLOAT:
            {
                const auto castArray =
                    static_cast<const arrow::FloatArray *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        static_cast<double>(castArray->Value(m_nIdxInBatch))))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::DOUBLE:
            {
                const auto castArray =
                    static_cast<const arrow::DoubleArray *>(array);
                if (!ConstraintEvaluator(constraint,
                                         castArray->Value(m_nIdxInBatch)))
                {
                    return true;
                }
                break;
            }
            case arrow::Type::STRING:
            {
                const auto castArray =
                    static_cast<const arrow::StringArray *>(array);
                int out_length = 0;
                const uint8_t *data =
                    castArray->GetValue(m_nIdxInBatch, &out_length);
                if (!ConstraintEvaluator(
                        constraint,
                        StringView(reinterpret_cast<const char *>(data),
                                   out_length)))
                {
                    return true;
                }
                break;
            }

            case arrow::Type::DECIMAL128:
            {
                const auto castArray =
                    static_cast<const arrow::Decimal128Array *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        CPLAtof(castArray->FormatValue(m_nIdxInBatch).c_str())))
                {
                    return true;
                }
                break;
            }

            case arrow::Type::DECIMAL256:
            {
                const auto castArray =
                    static_cast<const arrow::Decimal256Array *>(array);
                if (!ConstraintEvaluator(
                        constraint,
                        CPLAtof(castArray->FormatValue(m_nIdxInBatch).c_str())))
                {
                    return true;
                }
                break;
            }

            default:
                break;
        }
    }
    return false;
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

inline OGRFeature *OGRArrowLayer::GetNextRawFeature()
{
    if (m_bEOF || !m_bSpatialFilterIntersectsLayerExtent)
        return nullptr;

    if (m_poBatch == nullptr || m_nIdxInBatch == m_poBatch->num_rows())
    {
        m_bEOF = !ReadNextBatch();
        if (m_bEOF)
            return nullptr;
    }

    // Evaluate spatial filter by computing the bounding box of each geometry
    // but without creating a OGRGeometry
    if (m_poFilterGeom)
    {
        int iCol;
        if (m_bIgnoredFields)
        {
            iCol = m_anMapGeomFieldIndexToArrayIndex[m_iGeomFieldFilter];
        }
        else
        {
            iCol = m_anMapGeomFieldIndexToArrowColumn[m_iGeomFieldFilter];
        }
        if (iCol >= 0 &&
            m_aeGeomEncoding[m_iGeomFieldFilter] == OGRArrowGeomEncoding::WKB)
        {
            auto array = m_poBatchColumns[iCol];
            CPLAssert(array->type_id() == arrow::Type::BINARY);
            auto castArray =
                std::static_pointer_cast<arrow::BinaryArray>(array);
            OGREnvelope sEnvelope;
            while (true)
            {
                bool bSkipToNextFeature = false;
                if (array->IsNull(m_nIdxInBatch))
                {
                    bSkipToNextFeature = true;
                }
                else
                {
                    int out_length = 0;
                    const uint8_t *data =
                        castArray->GetValue(m_nIdxInBatch, &out_length);
                    if (ReadWKBBoundingBox(data, out_length, sEnvelope) &&
                        !m_sFilterEnvelope.Intersects(sEnvelope))
                    {
                        bSkipToNextFeature = true;
                    }
                }
                if (!bSkipToNextFeature)
                {
                    break;
                }
                if (!m_asAttributeFilterConstraints.empty() &&
                    !SkipToNextFeatureDueToAttributeFilter())
                {
                    break;
                }

                m_nFeatureIdx++;
                m_nIdxInBatch++;
                if (m_nIdxInBatch == m_poBatch->num_rows())
                {
                    m_bEOF = !ReadNextBatch();
                    if (m_bEOF)
                        return nullptr;
                    array = m_poBatchColumns[iCol];
                    CPLAssert(array->type_id() == arrow::Type::BINARY);
                    castArray =
                        std::static_pointer_cast<arrow::BinaryArray>(array);
                }
            }
        }
        else if (iCol >= 0 && m_aeGeomEncoding[m_iGeomFieldFilter] ==
                                  OGRArrowGeomEncoding::GEOARROW_MULTIPOLYGON)
        {
            const auto poGeomFieldDefn =
                m_poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter);
            const auto eGeomType = poGeomFieldDefn->GetType();
            const bool bHasZ = CPL_TO_BOOL(OGR_GT_HasZ(eGeomType));
            const bool bHasM = CPL_TO_BOOL(OGR_GT_HasM(eGeomType));
            const int nDim = 2 + (bHasZ ? 1 : 0) + (bHasM ? 1 : 0);

        begin_multipolygon:
            auto array = m_poBatchColumns[iCol].get();
            CPLAssert(array->type_id() == arrow::Type::LIST);
            auto listOfPartsArray =
                static_cast<const arrow::ListArray *>(array);
            CPLAssert(listOfPartsArray->values()->type_id() ==
                      arrow::Type::LIST);
            auto listOfPartsValues = std::static_pointer_cast<arrow::ListArray>(
                listOfPartsArray->values());
            CPLAssert(listOfPartsValues->values()->type_id() ==
                      arrow::Type::LIST);
            auto listOfRingsValues = std::static_pointer_cast<arrow::ListArray>(
                listOfPartsValues->values());
            CPLAssert(listOfRingsValues->values()->type_id() ==
                      arrow::Type::FIXED_SIZE_LIST);
            auto listOfPointsValues =
                std::static_pointer_cast<arrow::FixedSizeListArray>(
                    listOfRingsValues->values());
            CPLAssert(listOfPointsValues->values()->type_id() ==
                      arrow::Type::DOUBLE);
            auto pointValues = std::static_pointer_cast<arrow::DoubleArray>(
                listOfPointsValues->values());

            while (true)
            {
                if (!listOfPartsArray->IsNull(m_nIdxInBatch))
                {
                    OGREnvelope sEnvelope;
                    const auto nParts =
                        listOfPartsArray->value_length(m_nIdxInBatch);
                    const auto nPartOffset =
                        listOfPartsArray->value_offset(m_nIdxInBatch);
                    for (auto j = decltype(nParts){0}; j < nParts; j++)
                    {
                        const auto nRings =
                            listOfPartsValues->value_length(nPartOffset + j);
                        const auto nRingOffset =
                            listOfPartsValues->value_offset(nPartOffset + j);
                        if (nRings >= 1)
                        {
                            const auto nPoints =
                                listOfRingsValues->value_length(nRingOffset);
                            const auto nPointOffset =
                                listOfRingsValues->value_offset(nRingOffset) *
                                nDim;
                            const double *padfRawValue =
                                pointValues->raw_values() + nPointOffset;
                            for (auto l = decltype(nPoints){0}; l < nPoints;
                                 ++l)
                            {
                                sEnvelope.Merge(padfRawValue[nDim * l],
                                                padfRawValue[nDim * l + 1]);
                            }
                            // for bounding box, only the first ring matters
                        }
                    }

                    if (nParts != 0 && m_sFilterEnvelope.Intersects(sEnvelope))
                    {
                        break;
                    }
                }
                if (!m_asAttributeFilterConstraints.empty() &&
                    !SkipToNextFeatureDueToAttributeFilter())
                {
                    break;
                }

                m_nFeatureIdx++;
                m_nIdxInBatch++;
                if (m_nIdxInBatch == m_poBatch->num_rows())
                {
                    m_bEOF = !ReadNextBatch();
                    if (m_bEOF)
                        return nullptr;
                    goto begin_multipolygon;
                }
            }
        }
        else if (iCol >= 0)
        {
            auto array = m_poBatchColumns[iCol].get();
            OGREnvelope sEnvelope;
            while (true)
            {
                bool bSkipToNextFeature = false;
                auto poGeometry = std::unique_ptr<OGRGeometry>(
                    ReadGeometry(m_iGeomFieldFilter, array, m_nIdxInBatch));
                if (poGeometry == nullptr || poGeometry->IsEmpty())
                {
                    bSkipToNextFeature = true;
                }
                else
                {
                    poGeometry->getEnvelope(&sEnvelope);
                    if (!m_sFilterEnvelope.Intersects(sEnvelope))
                    {
                        bSkipToNextFeature = true;
                    }
                }
                if (!bSkipToNextFeature)
                {
                    break;
                }
                if (!m_asAttributeFilterConstraints.empty() &&
                    !SkipToNextFeatureDueToAttributeFilter())
                {
                    break;
                }

                m_nFeatureIdx++;
                m_nIdxInBatch++;
                if (m_nIdxInBatch == m_poBatch->num_rows())
                {
                    m_bEOF = !ReadNextBatch();
                    if (m_bEOF)
                        return nullptr;
                    array = m_poBatchColumns[iCol].get();
                }
            }
        }
    }

    else if (!m_asAttributeFilterConstraints.empty())
    {
        while (true)
        {
            if (!SkipToNextFeatureDueToAttributeFilter())
            {
                break;
            }

            m_nFeatureIdx++;
            m_nIdxInBatch++;
            if (m_nIdxInBatch == m_poBatch->num_rows())
            {
                m_bEOF = !ReadNextBatch();
                if (m_bEOF)
                    return nullptr;
            }
        }
    }

    auto poFeature = ReadFeature(m_nIdxInBatch, m_poBatchColumns);

    if (m_iFIDArrowColumn < 0)
        poFeature->SetFID(m_nFeatureIdx);

    m_nFeatureIdx++;
    m_nIdxInBatch++;

    return poFeature;
}

/************************************************************************/
/*                            GetExtent()                               */
/************************************************************************/

inline OGRErr OGRArrowLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    return GetExtent(0, psExtent, bForce);
}

/************************************************************************/
/*                       GetExtentFromMetadata()                        */
/************************************************************************/

inline OGRErr
OGRArrowLayer::GetExtentFromMetadata(const CPLJSONObject &oJSONDef,
                                     OGREnvelope *psExtent)
{
    const auto oBBox = oJSONDef.GetArray("bbox");
    if (oBBox.IsValid() && oBBox.Size() == 4)
    {
        psExtent->MinX = oBBox[0].ToDouble();
        psExtent->MinY = oBBox[1].ToDouble();
        psExtent->MaxX = oBBox[2].ToDouble();
        psExtent->MaxY = oBBox[3].ToDouble();
        if (psExtent->MinX <= psExtent->MaxX)
            return OGRERR_NONE;
    }
    else if (oBBox.IsValid() && oBBox.Size() == 6)
    {
        psExtent->MinX = oBBox[0].ToDouble();
        psExtent->MinY = oBBox[1].ToDouble();
        // MinZ skipped
        psExtent->MaxX = oBBox[3].ToDouble();
        psExtent->MaxY = oBBox[4].ToDouble();
        // MaxZ skipped
        if (psExtent->MinX <= psExtent->MaxX)
            return OGRERR_NONE;
    }
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                        SetSpatialFilter()                            */
/************************************************************************/

inline void OGRArrowLayer::SetSpatialFilter(int iGeomField,
                                            OGRGeometry *poGeomIn)

{
    m_bSpatialFilterIntersectsLayerExtent = true;
    if (iGeomField >= 0 && iGeomField < GetLayerDefn()->GetGeomFieldCount())
    {
        m_iGeomFieldFilter = iGeomField;
        if (InstallFilter(poGeomIn))
            ResetReading();
        if (m_poFilterGeom != nullptr)
        {
            OGREnvelope sLayerExtent;
            if (GetFastExtent(iGeomField, &sLayerExtent))
            {
                m_bSpatialFilterIntersectsLayerExtent =
                    m_sFilterEnvelope.Intersects(sLayerExtent);
            }
        }
    }
}

/************************************************************************/
/*                         GetFastExtent()                              */
/************************************************************************/

inline bool OGRArrowLayer::GetFastExtent(int iGeomField,
                                         OGREnvelope *psExtent) const
{
    {
        const auto oIter = m_oMapExtents.find(iGeomField);
        if (oIter != m_oMapExtents.end())
        {
            *psExtent = oIter->second;
            return true;
        }
    }

    const char *pszGeomFieldName =
        m_poFeatureDefn->GetGeomFieldDefn(iGeomField)->GetNameRef();
    const auto oIter = m_oMapGeometryColumns.find(pszGeomFieldName);
    if (oIter != m_oMapGeometryColumns.end() &&
        CPLTestBool(CPLGetConfigOption(
            ("OGR_" + GetDriverUCName() + "_USE_BBOX").c_str(), "YES")))
    {
        const auto &oJSONDef = oIter->second;
        if (GetExtentFromMetadata(oJSONDef, psExtent) == OGRERR_NONE)
        {
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                            GetExtent()                               */
/************************************************************************/

inline OGRErr OGRArrowLayer::GetExtent(int iGeomField, OGREnvelope *psExtent,
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

    if (GetFastExtent(iGeomField, psExtent))
    {
        return OGRERR_NONE;
    }

    if (!bForce && !CanRunNonForcedGetExtent())
    {
        return OGRERR_FAILURE;
    }

    int iCol;
    if (m_bIgnoredFields)
    {
        iCol = m_anMapGeomFieldIndexToArrayIndex[iGeomField];
    }
    else
    {
        iCol = m_anMapGeomFieldIndexToArrowColumn[iGeomField];
    }
    if (iCol < 0)
    {
        return OGRERR_FAILURE;
    }

    if (m_aeGeomEncoding[iGeomField] == OGRArrowGeomEncoding::WKB)
    {
        ResetReading();
        if (m_poBatch == nullptr)
        {
            m_bEOF = !ReadNextBatch();
            if (m_bEOF)
                return OGRERR_FAILURE;
        }
        *psExtent = OGREnvelope();

        auto array = m_poBatchColumns[iCol];
        CPLAssert(array->type_id() == arrow::Type::BINARY);
        auto castArray = std::static_pointer_cast<arrow::BinaryArray>(array);
        OGREnvelope sEnvelope;
        while (true)
        {
            if (!array->IsNull(m_nIdxInBatch))
            {
                int out_length = 0;
                const uint8_t *data =
                    castArray->GetValue(m_nIdxInBatch, &out_length);
                if (ReadWKBBoundingBox(data, out_length, sEnvelope))
                {
                    psExtent->Merge(sEnvelope);
                }
            }

            m_nFeatureIdx++;
            m_nIdxInBatch++;
            if (m_nIdxInBatch == m_poBatch->num_rows())
            {
                m_bEOF = !ReadNextBatch();
                if (m_bEOF)
                {
                    ResetReading();
                    if (psExtent->IsInit())
                    {
                        m_oMapExtents[iGeomField] = *psExtent;
                        return OGRERR_NONE;
                    }
                    return OGRERR_FAILURE;
                }
                array = m_poBatchColumns[iCol];
                CPLAssert(array->type_id() == arrow::Type::BINARY);
                castArray = std::static_pointer_cast<arrow::BinaryArray>(array);
            }
        }
    }
    else if (m_aeGeomEncoding[iGeomField] ==
             OGRArrowGeomEncoding::GEOARROW_MULTIPOLYGON)
    {
        ResetReading();
        if (m_poBatch == nullptr)
        {
            m_bEOF = !ReadNextBatch();
            if (m_bEOF)
                return OGRERR_FAILURE;
        }
        *psExtent = OGREnvelope();

        const auto poGeomFieldDefn =
            m_poFeatureDefn->GetGeomFieldDefn(iGeomField);
        const auto eGeomType = poGeomFieldDefn->GetType();
        const bool bHasZ = CPL_TO_BOOL(OGR_GT_HasZ(eGeomType));
        const bool bHasM = CPL_TO_BOOL(OGR_GT_HasM(eGeomType));
        const int nDim = 2 + (bHasZ ? 1 : 0) + (bHasM ? 1 : 0);

    begin_multipolygon:
        auto array = m_poBatchColumns[iCol].get();
        CPLAssert(array->type_id() == arrow::Type::LIST);
        auto listOfPartsArray = static_cast<const arrow::ListArray *>(array);
        CPLAssert(listOfPartsArray->values()->type_id() == arrow::Type::LIST);
        auto listOfPartsValues = std::static_pointer_cast<arrow::ListArray>(
            listOfPartsArray->values());
        CPLAssert(listOfPartsValues->values()->type_id() == arrow::Type::LIST);
        auto listOfRingsValues = std::static_pointer_cast<arrow::ListArray>(
            listOfPartsValues->values());
        CPLAssert(listOfRingsValues->values()->type_id() ==
                  arrow::Type::FIXED_SIZE_LIST);
        auto listOfPointsValues =
            std::static_pointer_cast<arrow::FixedSizeListArray>(
                listOfRingsValues->values());
        CPLAssert(listOfPointsValues->values()->type_id() ==
                  arrow::Type::DOUBLE);
        auto pointValues = std::static_pointer_cast<arrow::DoubleArray>(
            listOfPointsValues->values());

        while (true)
        {
            if (!listOfPartsArray->IsNull(m_nIdxInBatch))
            {
                const auto nParts =
                    listOfPartsArray->value_length(m_nIdxInBatch);
                const auto nPartOffset =
                    listOfPartsArray->value_offset(m_nIdxInBatch);
                for (auto j = decltype(nParts){0}; j < nParts; j++)
                {
                    const auto nRings =
                        listOfPartsValues->value_length(nPartOffset + j);
                    const auto nRingOffset =
                        listOfPartsValues->value_offset(nPartOffset + j);
                    if (nRings >= 1)
                    {
                        const auto nPoints =
                            listOfRingsValues->value_length(nRingOffset);
                        const auto nPointOffset =
                            listOfRingsValues->value_offset(nRingOffset) * nDim;
                        const double *padfRawValue =
                            pointValues->raw_values() + nPointOffset;
                        for (auto l = decltype(nPoints){0}; l < nPoints; ++l)
                        {
                            psExtent->Merge(padfRawValue[nDim * l],
                                            padfRawValue[nDim * l + 1]);
                        }
                        // for bounding box, only the first ring matters
                    }
                }
            }

            m_nFeatureIdx++;
            m_nIdxInBatch++;
            if (m_nIdxInBatch == m_poBatch->num_rows())
            {
                m_bEOF = !ReadNextBatch();
                if (m_bEOF)
                {
                    ResetReading();
                    if (psExtent->IsInit())
                    {
                        m_oMapExtents[iGeomField] = *psExtent;
                        return OGRERR_NONE;
                    }
                    return OGRERR_FAILURE;
                }
                goto begin_multipolygon;
            }
        }
    }

    return GetExtentInternal(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                  OverrideArrowSchemaRelease()                        */
/************************************************************************/

template <class T>
static void OverrideArrowRelease(OGRArrowDataset *poDS, T *obj)
{
    // We override the release callback, since it can use the memory pool,
    // and we need to make sure it is still alive when the object (ArrowArray
    // or ArrowSchema) is deleted
    struct OverriddenPrivate
    {
        OverriddenPrivate() = default;
        OverriddenPrivate(const OverriddenPrivate &) = delete;
        OverriddenPrivate &operator=(const OverriddenPrivate &) = delete;

        std::shared_ptr<arrow::MemoryPool> poMemoryPool{};
        void (*pfnPreviousRelease)(T *) = nullptr;
        void *pPreviousPrivateData = nullptr;
    };

    auto overriddenPrivate = new OverriddenPrivate();
    overriddenPrivate->poMemoryPool = poDS->GetSharedMemoryPool();
    overriddenPrivate->pPreviousPrivateData = obj->private_data;
    overriddenPrivate->pfnPreviousRelease = obj->release;

    obj->release = [](T *l_obj)
    {
        OverriddenPrivate *myPrivate =
            static_cast<OverriddenPrivate *>(l_obj->private_data);
        l_obj->private_data = myPrivate->pPreviousPrivateData;
        l_obj->release = myPrivate->pfnPreviousRelease;
        l_obj->release(l_obj);
        delete myPrivate;
    };
    obj->private_data = overriddenPrivate;
}

/************************************************************************/
/*                   UseRecordBatchBaseImplementation()                 */
/************************************************************************/

inline bool OGRArrowLayer::UseRecordBatchBaseImplementation() const
{
    if (m_poAttrQuery != nullptr || m_poFilterGeom != nullptr ||
        CPLTestBool(CPLGetConfigOption("OGR_ARROW_STREAM_BASE_IMPL", "NO")))
    {
        return true;
    }

    if (EQUAL(m_aosArrowArrayStreamOptions.FetchNameValueDef(
                  "GEOMETRY_ENCODING", ""),
              "WKB"))
    {
        const int nGeomFieldCount = m_poFeatureDefn->GetGeomFieldCount();
        for (int i = 0; i < nGeomFieldCount; i++)
        {
            if (m_aeGeomEncoding[i] != OGRArrowGeomEncoding::WKB)
                return true;
        }
    }

    if (m_bIgnoredFields)
    {
        std::vector<int> ignoredState(m_anMapFieldIndexToArrowColumn.size(),
                                      -1);
        for (size_t i = 0; i < m_anMapFieldIndexToArrowColumn.size(); i++)
        {
            const int nArrowCol = m_anMapFieldIndexToArrowColumn[i][0];
            if (nArrowCol >= static_cast<int>(ignoredState.size()))
                ignoredState.resize(nArrowCol + 1, -1);
            const auto bIsIgnored =
                m_poFeatureDefn->GetFieldDefn(static_cast<int>(i))->IsIgnored();
            if (ignoredState[nArrowCol] < 0)
            {
                ignoredState[nArrowCol] = static_cast<int>(bIsIgnored);
            }
            else
            {
                // struct fields will point to the same arrow column
                if (ignoredState[nArrowCol] != static_cast<int>(bIsIgnored))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

/************************************************************************/
/*                         GetArrowSchema()                             */
/************************************************************************/

inline int OGRArrowLayer::GetArrowSchema(struct ArrowArrayStream *stream,
                                         struct ArrowSchema *out_schema)
{
    if (UseRecordBatchBaseImplementation())
        return OGRLayer::GetArrowSchema(stream, out_schema);

    auto status = arrow::ExportSchema(*m_poSchema, out_schema);
    if (!status.ok())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "ExportSchema() failed with %s",
                 status.message().c_str());
        return EIO;
    }

    CPLAssert(out_schema->n_children == m_poSchema->num_fields());

    if (m_bIgnoredFields)
    {
        // Remove ignored fields from the ArrowSchema.

        struct FieldDesc
        {
            bool bIsRegularField =
                false;  // true = attribute field, false = geometry field
            int nIdx = -1;
        };
        // cppcheck-suppress unreadVariable
        std::vector<FieldDesc> fieldDesc(out_schema->n_children);
        for (size_t i = 0; i < m_anMapFieldIndexToArrowColumn.size(); i++)
        {
            const int nArrowCol = m_anMapFieldIndexToArrowColumn[i][0];
            if (fieldDesc[nArrowCol].nIdx < 0)
            {
                fieldDesc[nArrowCol].bIsRegularField = true;
                fieldDesc[nArrowCol].nIdx = static_cast<int>(i);
            }
        }
        for (size_t i = 0; i < m_anMapGeomFieldIndexToArrowColumn.size(); i++)
        {
            const int nArrowCol = m_anMapGeomFieldIndexToArrowColumn[i];
            CPLAssert(fieldDesc[nArrowCol].nIdx < 0);
            fieldDesc[nArrowCol].bIsRegularField = false;
            fieldDesc[nArrowCol].nIdx = static_cast<int>(i);
        }

        int j = 0;
        for (int i = 0; i < out_schema->n_children; ++i)
        {
            const auto bIsIgnored =
                fieldDesc[i].bIsRegularField
                    ? m_poFeatureDefn->GetFieldDefn(fieldDesc[i].nIdx)
                          ->IsIgnored()
                    : m_poFeatureDefn->GetGeomFieldDefn(fieldDesc[i].nIdx)
                          ->IsIgnored();
            if (bIsIgnored)
            {
                out_schema->children[i]->release(out_schema->children[i]);
            }
            else
            {
                out_schema->children[j] = out_schema->children[i];
                ++j;
            }
        }
        out_schema->n_children = j;
    }

    OverrideArrowRelease(m_poArrowDS, out_schema);
    return 0;
}

/************************************************************************/
/*                       GetNextArrowArray()                            */
/************************************************************************/

inline int OGRArrowLayer::GetNextArrowArray(struct ArrowArrayStream *stream,
                                            struct ArrowArray *out_array)
{
    if (UseRecordBatchBaseImplementation())
        return OGRLayer::GetNextArrowArray(stream, out_array);

    if (m_bEOF)
    {
        memset(out_array, 0, sizeof(*out_array));
        return 0;
    }

    if (m_poBatch == nullptr || m_nIdxInBatch == m_poBatch->num_rows())
    {
        m_bEOF = !ReadNextBatch();
        if (m_bEOF)
        {
            memset(out_array, 0, sizeof(*out_array));
            return 0;
        }
    }

    auto status = arrow::ExportRecordBatch(*m_poBatch, out_array, nullptr);
    m_nIdxInBatch = m_poBatch->num_rows();
    if (!status.ok())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ExportRecordBatch() failed with %s",
                 status.message().c_str());
        return EIO;
    }

    OverrideArrowRelease(m_poArrowDS, out_array);

    return 0;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

inline int OGRArrowLayer::TestCapability(const char *pszCap)
{

    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return true;

    else if (EQUAL(pszCap, OLCFastGetArrowStream) &&
             !UseRecordBatchBaseImplementation())
    {
        return true;
    }

    return false;
}
