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

#include "cpl_json.h"
#include "cpl_time.h"
#include "cpl_multiproc.h"
#include "gdal_pam.h"
#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include <algorithm>
#include <cinttypes>
#include <limits>
#include <map>
#include <set>
#include <utility>

#include "ogr_parquet.h"

#include "../arrow_common/ograrrowlayer.hpp"
#include "../arrow_common/ograrrowdataset.hpp"

/************************************************************************/
/*                    OGRParquetLayerBase()                             */
/************************************************************************/

OGRParquetLayerBase::OGRParquetLayerBase(OGRParquetDataset *poDS,
                                         const char *pszLayerName)
    : OGRArrowLayer(poDS, pszLayerName), m_poDS(poDS)
{
}

/************************************************************************/
/*                           GetDataset()                               */
/************************************************************************/

GDALDataset *OGRParquetLayer::GetDataset()
{
    return m_poDS;
}

/************************************************************************/
/*                          LoadGeoMetadata()                           */
/************************************************************************/

void OGRParquetLayerBase::LoadGeoMetadata(
    const std::shared_ptr<const arrow::KeyValueMetadata> &kv_metadata)
{
    if (kv_metadata && kv_metadata->Contains("geo"))
    {
        auto geo = kv_metadata->Get("geo");
        if (geo.ok())
        {
            CPLDebug("PARQUET", "geo = %s", geo->c_str());
            CPLJSONDocument oDoc;
            if (oDoc.LoadMemory(*geo))
            {
                auto oRoot = oDoc.GetRoot();
                const auto osVersion = oRoot.GetString("version");
                if (osVersion != "0.1.0" && osVersion != "0.2.0" &&
                    osVersion != "0.3.0" && osVersion != "0.4.0" &&
                    osVersion != "1.0.0-beta.1")
                {
                    CPLDebug(
                        "PARQUET",
                        "version = %s not explicitly handled by the driver",
                        osVersion.c_str());
                }

                auto oColumns = oRoot.GetObj("columns");
                if (oColumns.IsValid())
                {
                    for (const auto &oColumn : oColumns.GetChildren())
                    {
                        m_oMapGeometryColumns[oColumn.GetName()] = oColumn;
                    }
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot parse 'geo' metadata");
            }
        }
    }
}

/************************************************************************/
/*                      DealWithGeometryColumn()                        */
/************************************************************************/

bool OGRParquetLayerBase::DealWithGeometryColumn(
    int iFieldIdx, const std::shared_ptr<arrow::Field> &field,
    std::function<OGRwkbGeometryType(void)> computeGeometryTypeFun)
{
    const auto &field_kv_metadata = field->metadata();
    std::string osExtensionName;
    if (field_kv_metadata)
    {
        auto extension_name = field_kv_metadata->Get("ARROW:extension:name");
        if (extension_name.ok())
        {
            osExtensionName = *extension_name;
        }
#ifdef DEBUG
        CPLDebug("PARQUET", "Metadata field %s:", field->name().c_str());
        for (const auto &keyValue : field_kv_metadata->sorted_pairs())
        {
            CPLDebug("PARQUET", "  %s = %s", keyValue.first.c_str(),
                     keyValue.second.c_str());
        }
#endif
    }

    bool bRegularField = true;
    // odd indetation to make backports to release/3.5 easier
    {
        auto oIter = m_oMapGeometryColumns.find(field->name());
        if (oIter != m_oMapGeometryColumns.end() ||
            STARTS_WITH(osExtensionName.c_str(), "geoarrow."))
        {
            CPLJSONObject oJSONDef;
            if (oIter != m_oMapGeometryColumns.end())
                oJSONDef = oIter->second;
            auto osEncoding = oJSONDef.GetString("encoding");
            if (osEncoding.empty() && !osExtensionName.empty())
                osEncoding = osExtensionName;

            OGRwkbGeometryType eGeomType = wkbUnknown;
            auto eGeomEncoding = OGRArrowGeomEncoding::WKB;
            if (IsValidGeometryEncoding(field, osEncoding, eGeomType,
                                        eGeomEncoding))
            {
                bRegularField = false;
                OGRGeomFieldDefn oField(field->name().c_str(), wkbUnknown);

                auto oCRS = oJSONDef["crs"];
                OGRSpatialReference *poSRS = nullptr;
                if (!oCRS.IsValid())
                {
                    // WGS 84 is implied if no crs member is found.
                    poSRS = new OGRSpatialReference();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    poSRS->importFromEPSG(4326);
                }
                else if (oCRS.GetType() == CPLJSONObject::Type::String)
                {
                    const auto osWKT = oCRS.ToString();
                    poSRS = new OGRSpatialReference();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

                    if (poSRS->importFromWkt(osWKT.c_str()) != OGRERR_NONE)
                    {
                        poSRS->Release();
                        poSRS = nullptr;
                    }
                }
                else if (oCRS.GetType() == CPLJSONObject::Type::Object)
                {
                    // CRS encoded as PROJJSON (extension)
                    const auto oType = oCRS["type"];
                    if (oType.IsValid() &&
                        oType.GetType() == CPLJSONObject::Type::String)
                    {
                        const auto osType = oType.ToString();
                        if (osType.find("CRS") != std::string::npos)
                        {
                            poSRS = new OGRSpatialReference();
                            poSRS->SetAxisMappingStrategy(
                                OAMS_TRADITIONAL_GIS_ORDER);

                            if (poSRS->SetFromUserInput(
                                    oCRS.ToString().c_str()) != OGRERR_NONE)
                            {
                                poSRS->Release();
                                poSRS = nullptr;
                            }
                        }
                    }
                }

                if (poSRS)
                {
                    const double dfCoordEpoch = oJSONDef.GetDouble("epoch");
                    if (dfCoordEpoch > 0)
                        poSRS->SetCoordinateEpoch(dfCoordEpoch);

                    oField.SetSpatialRef(poSRS);

                    poSRS->Release();
                }

                if (oJSONDef.GetString("edges") == "spherical")
                {
                    SetMetadataItem("EDGES", "SPHERICAL");
                }

                // m_aeGeomEncoding be filled before calling
                // ComputeGeometryColumnType()
                m_aeGeomEncoding.push_back(eGeomEncoding);
                if (eGeomType == wkbUnknown)
                {
                    // geometry_types since 1.0.0-beta1. Was geometry_type
                    // before
                    auto oType = oJSONDef.GetObj("geometry_types");
                    if (!oType.IsValid())
                        oType = oJSONDef.GetObj("geometry_type");
                    if (oType.GetType() == CPLJSONObject::Type::String)
                    {
                        // string is no longer valid since 1.0.0-beta1
                        const auto osType = oType.ToString();
                        if (osType != "Unknown")
                            eGeomType = GetGeometryTypeFromString(osType);
                    }
                    else if (oType.GetType() == CPLJSONObject::Type::Array)
                    {
                        const auto oTypeArray = oType.ToArray();
                        if (oTypeArray.Size() == 1)
                        {
                            eGeomType = GetGeometryTypeFromString(
                                oTypeArray[0].ToString());
                        }
                        else if (oTypeArray.Size() > 1)
                        {
                            const auto PromoteToCollection =
                                [](OGRwkbGeometryType eType)
                            {
                                if (eType == wkbPoint)
                                    return wkbMultiPoint;
                                if (eType == wkbLineString)
                                    return wkbMultiLineString;
                                if (eType == wkbPolygon)
                                    return wkbMultiPolygon;
                                return eType;
                            };
                            bool bMixed = false;
                            bool bHasMulti = false;
                            bool bHasZ = false;
                            bool bHasM = false;
                            const auto eFirstType =
                                OGR_GT_Flatten(GetGeometryTypeFromString(
                                    oTypeArray[0].ToString()));
                            const auto eFirstTypeCollection =
                                PromoteToCollection(eFirstType);
                            for (int i = 0; i < oTypeArray.Size(); ++i)
                            {
                                const auto eThisGeom =
                                    GetGeometryTypeFromString(
                                        oTypeArray[i].ToString());
                                if (PromoteToCollection(OGR_GT_Flatten(
                                        eThisGeom)) != eFirstTypeCollection)
                                {
                                    bMixed = true;
                                    break;
                                }
                                bHasZ |= OGR_GT_HasZ(eThisGeom) != FALSE;
                                bHasM |= OGR_GT_HasM(eThisGeom) != FALSE;
                                bHasMulti |= (PromoteToCollection(
                                                  OGR_GT_Flatten(eThisGeom)) ==
                                              OGR_GT_Flatten(eThisGeom));
                            }
                            if (!bMixed)
                            {
                                if (eFirstTypeCollection == wkbMultiPolygon ||
                                    eFirstTypeCollection == wkbMultiLineString)
                                {
                                    if (bHasMulti)
                                        eGeomType = OGR_GT_SetModifier(
                                            eFirstTypeCollection, bHasZ, bHasM);
                                    else
                                        eGeomType = OGR_GT_SetModifier(
                                            eFirstType, bHasZ, bHasM);
                                }
                            }
                        }
                    }
                    else if (CPLTestBool(CPLGetConfigOption(
                                 "OGR_PARQUET_COMPUTE_GEOMETRY_TYPE", "YES")))
                    {
                        eGeomType = computeGeometryTypeFun();
                    }
                }

                oField.SetType(eGeomType);
                oField.SetNullable(field->nullable());
                m_poFeatureDefn->AddGeomFieldDefn(&oField);
                m_anMapGeomFieldIndexToArrowColumn.push_back(iFieldIdx);
            }
        }
    }
    return !bRegularField;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRParquetLayerBase::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poAttrQuery == nullptr && m_poFilterGeom == nullptr;

    if (EQUAL(pszCap, OLCFastGetExtent))
    {
        for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); i++)
        {
            auto oIter = m_oMapGeometryColumns.find(
                m_poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
            if (oIter == m_oMapGeometryColumns.end())
            {
                return false;
            }
            const auto &oJSONDef = oIter->second;
            const auto oBBox = oJSONDef.GetArray("bbox");
            if (!(oBBox.IsValid() && (oBBox.Size() == 4 || oBBox.Size() == 6)))
            {
                return false;
            }
        }
        return true;
    }

    if (EQUAL(pszCap, OLCMeasuredGeometries))
        return true;

    if (EQUAL(pszCap, OLCFastSetNextByIndex))
        return true;

    return OGRArrowLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                        OGRParquetLayer()                             */
/************************************************************************/

OGRParquetLayer::OGRParquetLayer(
    OGRParquetDataset *poDS, const char *pszLayerName,
    std::unique_ptr<parquet::arrow::FileReader> &&arrow_reader)
    : OGRParquetLayerBase(poDS, pszLayerName),
      m_poArrowReader(std::move(arrow_reader))
{
    const char *pszParquetBatchSize =
        CPLGetConfigOption("OGR_PARQUET_BATCH_SIZE", nullptr);
    if (pszParquetBatchSize)
        m_poArrowReader->set_batch_size(CPLAtoGIntBig(pszParquetBatchSize));

    const char *pszNumThreads = CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
    int nNumThreads = 0;
    if (pszNumThreads == nullptr)
        nNumThreads = std::min(4, CPLGetNumCPUs());
    else
        nNumThreads = EQUAL(pszNumThreads, "ALL_CPUS") ? CPLGetNumCPUs()
                                                       : atoi(pszNumThreads);
    if (nNumThreads > 1)
    {
        CPL_IGNORE_RET_VAL(arrow::SetCpuThreadPoolCapacity(nNumThreads));
        m_poArrowReader->set_use_threads(true);
    }

    EstablishFeatureDefn();
    CPLAssert(static_cast<int>(m_aeGeomEncoding.size()) ==
              m_poFeatureDefn->GetGeomFieldCount());
}

/************************************************************************/
/*                        EstablishFeatureDefn()                        */
/************************************************************************/

void OGRParquetLayer::EstablishFeatureDefn()
{
    const auto metadata = m_poArrowReader->parquet_reader()->metadata();
    const auto &kv_metadata = metadata->key_value_metadata();

    LoadGeoMetadata(kv_metadata);
    const auto oMapFieldNameToGDALSchemaFieldDefn =
        LoadGDALMetadata(kv_metadata.get());

    if (!m_poArrowReader->GetSchema(&m_poSchema).ok())
    {
        return;
    }

    const auto fields = m_poSchema->fields();
    const auto poParquetSchema = metadata->schema();
    int iParquetCol = 0;
    for (int i = 0; i < m_poSchema->num_fields(); ++i)
    {
        const auto &field = fields[i];

        bool bParquetColValid =
            CheckMatchArrowParquetColumnNames(iParquetCol, field);
        if (!bParquetColValid)
            m_bHasMissingMappingToParquet = true;

        if (!m_osFIDColumn.empty() && field->name() == m_osFIDColumn)
        {
            m_iFIDArrowColumn = i;
            if (bParquetColValid)
            {
                m_iFIDParquetColumn = iParquetCol;
                iParquetCol++;
            }
            continue;
        }

        const auto ComputeGeometryColumnTypeLambda =
            [this, bParquetColValid, iParquetCol, &poParquetSchema]()
        {
            // only with GeoParquet < 0.2.0
            if (bParquetColValid &&
                poParquetSchema->Column(iParquetCol)->physical_type() ==
                    parquet::Type::BYTE_ARRAY)
            {
                return ComputeGeometryColumnType(
                    m_poFeatureDefn->GetGeomFieldCount(), iParquetCol);
            }
            return wkbUnknown;
        };

        const bool bGeometryField =
            DealWithGeometryColumn(i, field, ComputeGeometryColumnTypeLambda);
        if (bGeometryField)
        {
            m_anMapGeomFieldIndexToParquetColumn.push_back(
                bParquetColValid ? iParquetCol : -1);
            if (bParquetColValid)
                iParquetCol++;
        }
        else
        {
            CreateFieldFromSchema(field, bParquetColValid, iParquetCol, {i},
                                  oMapFieldNameToGDALSchemaFieldDefn);
        }
    }

    CPLAssert(static_cast<int>(m_anMapFieldIndexToArrowColumn.size()) ==
              m_poFeatureDefn->GetFieldCount());
    CPLAssert(static_cast<int>(m_anMapFieldIndexToParquetColumn.size()) ==
              m_poFeatureDefn->GetFieldCount());
    CPLAssert(static_cast<int>(m_anMapGeomFieldIndexToArrowColumn.size()) ==
              m_poFeatureDefn->GetGeomFieldCount());
    CPLAssert(static_cast<int>(m_anMapGeomFieldIndexToParquetColumn.size()) ==
              m_poFeatureDefn->GetGeomFieldCount());

    if (!fields.empty())
    {
        try
        {
            auto poRowGroup = m_poArrowReader->parquet_reader()->RowGroup(0);
            if (poRowGroup)
            {
                auto poColumn = poRowGroup->metadata()->ColumnChunk(0);
                CPLDebug("PARQUET", "Compression (of first column): %s",
                         arrow::util::Codec::GetCodecAsString(
                             poColumn->compression())
                             .c_str());
            }
        }
        catch (const std::exception &)
        {
        }
    }
}

/************************************************************************/
/*                CheckMatchArrowParquetColumnNames()                   */
/************************************************************************/

bool OGRParquetLayer::CheckMatchArrowParquetColumnNames(
    int &iParquetCol, const std::shared_ptr<arrow::Field> &field) const
{
    const auto metadata = m_poArrowReader->parquet_reader()->metadata();
    const auto poParquetSchema = metadata->schema();
    const int nParquetColumns = poParquetSchema->num_columns();
    const auto fieldName = field->name();
    const int iParquetColBefore = iParquetCol;

    while (iParquetCol < nParquetColumns)
    {
        const auto parquetColumn = poParquetSchema->Column(iParquetCol);
        const auto parquetColumnName = parquetColumn->path()->ToDotString();
        if (fieldName == parquetColumnName ||
            (parquetColumnName.size() > fieldName.size() &&
             STARTS_WITH(parquetColumnName.c_str(), fieldName.c_str()) &&
             parquetColumnName[fieldName.size()] == '.'))
        {
            return true;
        }
        else
        {
            iParquetCol++;
        }
    }

    CPLError(CE_Warning, CPLE_AppDefined,
             "Cannot match Arrow column name %s with a Parquet one",
             fieldName.c_str());
    iParquetCol = iParquetColBefore;
    return false;
}

/************************************************************************/
/*                         CreateFieldFromSchema()                      */
/************************************************************************/

void OGRParquetLayer::CreateFieldFromSchema(
    const std::shared_ptr<arrow::Field> &field, bool bParquetColValid,
    int &iParquetCol, const std::vector<int> &path,
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
            if (bParquetColValid)
            {
                std::string osDomainName(field->name() + "Domain");
                m_poDS->RegisterDomainName(osDomainName,
                                           m_poFeatureDefn->GetFieldCount());
                oField.SetDomainName(osDomainName);
            }
            type = indexType;
        }
        else
        {
            bTypeOK = false;
        }
    }

    int nParquetColIncrement = 1;
    switch (type->id())
    {
        case arrow::Type::STRUCT:
        {
            const auto subfields = field->Flatten();
            auto newpath = path;
            newpath.push_back(0);
            for (int j = 0; j < static_cast<int>(subfields.size()); j++)
            {
                const auto &subfield = subfields[j];
                bParquetColValid =
                    CheckMatchArrowParquetColumnNames(iParquetCol, subfield);
                if (!bParquetColValid)
                    m_bHasMissingMappingToParquet = true;
                newpath.back() = j;
                CreateFieldFromSchema(subfield, bParquetColValid, iParquetCol,
                                      newpath,
                                      oMapFieldNameToGDALSchemaFieldDefn);
            }
            return;  // return intended, not break
        }

        case arrow::Type::MAP:
        {
            // A arrow map maps to 2 Parquet columns
            nParquetColIncrement = 2;
            break;
        }

        default:
            break;
    }

    if (bTypeOK)
    {
        bTypeOK = MapArrowTypeToOGR(type, field, oField, eType, eSubType, path,
                                    oMapFieldNameToGDALSchemaFieldDefn);
        if (bTypeOK)
        {
            m_apoArrowDataTypes.push_back(type);
            m_anMapFieldIndexToParquetColumn.push_back(
                bParquetColValid ? iParquetCol : -1);
        }
    }

    if (bParquetColValid)
        iParquetCol += nParquetColIncrement;
}

/************************************************************************/
/*                          BuildDomain()                               */
/************************************************************************/

std::unique_ptr<OGRFieldDomain>
OGRParquetLayer::BuildDomain(const std::string &osDomainName,
                             int iFieldIndex) const
{
#ifdef DEBUG
    const int iArrowCol = m_anMapFieldIndexToArrowColumn[iFieldIndex][0];
    (void)iArrowCol;
    CPLAssert(m_poSchema->fields()[iArrowCol]->type()->id() ==
              arrow::Type::DICTIONARY);
#endif
    const int iParquetCol = m_anMapFieldIndexToParquetColumn[iFieldIndex];
    CPLAssert(iParquetCol >= 0);
    std::shared_ptr<arrow::RecordBatchReader> poRecordBatchReader;
    const auto oldBatchSize = m_poArrowReader->properties().batch_size();
    m_poArrowReader->set_batch_size(1);
    CPL_IGNORE_RET_VAL(m_poArrowReader->GetRecordBatchReader(
        {0}, {iParquetCol}, &poRecordBatchReader));
    if (poRecordBatchReader != nullptr)
    {
        std::shared_ptr<arrow::RecordBatch> poBatch;
        auto status = poRecordBatchReader->ReadNext(&poBatch);
        if (!status.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "ReadNext() failed: %s",
                     status.message().c_str());
        }
        else if (poBatch)
        {
            m_poArrowReader->set_batch_size(oldBatchSize);
            return BuildDomainFromBatch(osDomainName, poBatch, 0);
        }
    }
    m_poArrowReader->set_batch_size(oldBatchSize);
    return nullptr;
}

/************************************************************************/
/*                     ComputeGeometryColumnType()                      */
/************************************************************************/

OGRwkbGeometryType
OGRParquetLayer::ComputeGeometryColumnType(int iGeomCol, int iParquetCol) const
{
    // Compute type of geometry column by iterating over each geometry, and
    // looking at the WKB geometry type in the first 5 bytes of each geometry.

    OGRwkbGeometryType eGeomType = wkbNone;
    std::shared_ptr<arrow::RecordBatchReader> poRecordBatchReader;

    std::vector<int> anRowGroups;
    const int nNumGroups = m_poArrowReader->num_row_groups();
    anRowGroups.reserve(nNumGroups);
    for (int i = 0; i < nNumGroups; ++i)
        anRowGroups.push_back(i);
    CPL_IGNORE_RET_VAL(m_poArrowReader->GetRecordBatchReader(
        anRowGroups, {iParquetCol}, &poRecordBatchReader));
    if (poRecordBatchReader != nullptr)
    {
        std::shared_ptr<arrow::RecordBatch> poBatch;
        while (true)
        {
            auto status = poRecordBatchReader->ReadNext(&poBatch);
            if (!status.ok())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "ReadNext() failed: %s",
                         status.message().c_str());
                break;
            }
            else if (!poBatch)
                break;

            eGeomType = ComputeGeometryColumnTypeProcessBatch(poBatch, iGeomCol,
                                                              0, eGeomType);
            if (eGeomType == wkbUnknown)
                break;
        }
    }

    return eGeomType == wkbNone ? wkbUnknown : eGeomType;
}

/************************************************************************/
/*                       GetFeatureExplicitFID()                        */
/************************************************************************/

OGRFeature *OGRParquetLayer::GetFeatureExplicitFID(GIntBig nFID)
{
    std::shared_ptr<arrow::RecordBatchReader> poRecordBatchReader;

    std::vector<int> anRowGroups;
    const int nNumGroups = m_poArrowReader->num_row_groups();
    anRowGroups.reserve(nNumGroups);
    for (int i = 0; i < nNumGroups; ++i)
        anRowGroups.push_back(i);
    if (m_bIgnoredFields)
    {
        CPL_IGNORE_RET_VAL(m_poArrowReader->GetRecordBatchReader(
            anRowGroups, m_anRequestedParquetColumns, &poRecordBatchReader));
    }
    else
    {
        CPL_IGNORE_RET_VAL(m_poArrowReader->GetRecordBatchReader(
            anRowGroups, &poRecordBatchReader));
    }
    if (poRecordBatchReader != nullptr)
    {
        std::shared_ptr<arrow::RecordBatch> poBatch;
        while (true)
        {
            auto status = poRecordBatchReader->ReadNext(&poBatch);
            if (!status.ok())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "ReadNext() failed: %s",
                         status.message().c_str());
                break;
            }
            else if (!poBatch)
                break;

            const auto array = poBatch->column(
                m_bIgnoredFields ? m_nRequestedFIDColumn : m_iFIDArrowColumn);
            const auto arrayPtr = array.get();
            const auto arrayTypeId = array->type_id();
            for (int64_t nIdxInBatch = 0; nIdxInBatch < poBatch->num_rows();
                 nIdxInBatch++)
            {
                if (!array->IsNull(nIdxInBatch))
                {
                    if (arrayTypeId == arrow::Type::INT64)
                    {
                        const auto castArray =
                            static_cast<const arrow::Int64Array *>(arrayPtr);
                        if (castArray->Value(nIdxInBatch) == nFID)
                        {
                            return ReadFeature(nIdxInBatch, poBatch->columns());
                        }
                    }
                    else if (arrayTypeId == arrow::Type::INT32)
                    {
                        const auto castArray =
                            static_cast<const arrow::Int32Array *>(arrayPtr);
                        if (castArray->Value(nIdxInBatch) == nFID)
                        {
                            return ReadFeature(nIdxInBatch, poBatch->columns());
                        }
                    }
                }
            }
        }
    }
    return nullptr;
}

/************************************************************************/
/*                         GetFeatureByIndex()                          */
/************************************************************************/

OGRFeature *OGRParquetLayer::GetFeatureByIndex(GIntBig nFID)
{

    if (nFID < 0)
        return nullptr;

    const auto metadata = m_poArrowReader->parquet_reader()->metadata();
    const int nNumGroups = m_poArrowReader->num_row_groups();
    int64_t nAccRows = 0;
    for (int iGroup = 0; iGroup < nNumGroups; ++iGroup)
    {
        const int64_t nNextAccRows =
            nAccRows + metadata->RowGroup(iGroup)->num_rows();
        if (nFID < nNextAccRows)
        {
            std::shared_ptr<arrow::RecordBatchReader> poRecordBatchReader;
            arrow::Status status;
            if (m_bIgnoredFields)
            {
                status = m_poArrowReader->GetRecordBatchReader(
                    {iGroup}, m_anRequestedParquetColumns,
                    &poRecordBatchReader);
            }
            else
            {
                status = m_poArrowReader->GetRecordBatchReader(
                    {iGroup}, &poRecordBatchReader);
            }
            if (poRecordBatchReader == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "GetRecordBatchReader() failed: %s",
                         status.message().c_str());
                return nullptr;
            }

            const int64_t nExpectedIdxInGroup = nFID - nAccRows;
            int64_t nIdxInGroup = 0;
            while (true)
            {
                std::shared_ptr<arrow::RecordBatch> poBatch;
                status = poRecordBatchReader->ReadNext(&poBatch);
                if (!status.ok())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "ReadNext() failed: %s", status.message().c_str());
                    return nullptr;
                }
                if (poBatch == nullptr)
                {
                    return nullptr;
                }
                if (nExpectedIdxInGroup < nIdxInGroup + poBatch->num_rows())
                {
                    const auto nIdxInBatch = nExpectedIdxInGroup - nIdxInGroup;
                    auto poFeature =
                        ReadFeature(nIdxInBatch, poBatch->columns());
                    poFeature->SetFID(nFID);
                    return poFeature;
                }
                nIdxInGroup += poBatch->num_rows();
            }
        }
        nAccRows = nNextAccRows;
    }
    return nullptr;
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature *OGRParquetLayer::GetFeature(GIntBig nFID)
{
    if (!m_osFIDColumn.empty())
    {
        return GetFeatureExplicitFID(nFID);
    }
    else
    {
        return GetFeatureByIndex(nFID);
    }
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

void OGRParquetLayer::ResetReading()
{
    if (m_iRecordBatch != 0)
    {
        m_poRecordBatchReader.reset();
    }
    OGRParquetLayerBase::ResetReading();
}

/************************************************************************/
/*                      CreateRecordBatchReader()                       */
/************************************************************************/

bool OGRParquetLayer::CreateRecordBatchReader(int iStartingRowGroup)
{
    std::vector<int> anRowGroups;
    const int nNumGroups = m_poArrowReader->num_row_groups();
    anRowGroups.reserve(nNumGroups - iStartingRowGroup);
    for (int i = iStartingRowGroup; i < nNumGroups; ++i)
        anRowGroups.push_back(i);
    arrow::Status status;
    if (m_bIgnoredFields)
    {
        status = m_poArrowReader->GetRecordBatchReader(
            anRowGroups, m_anRequestedParquetColumns, &m_poRecordBatchReader);
    }
    else
    {
        status = m_poArrowReader->GetRecordBatchReader(anRowGroups,
                                                       &m_poRecordBatchReader);
    }
    if (m_poRecordBatchReader == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetRecordBatchReader() failed: %s", status.message().c_str());
        return false;
    }
    return true;
}

/************************************************************************/
/*                           ReadNextBatch()                            */
/************************************************************************/

bool OGRParquetLayer::ReadNextBatch()
{
    m_nIdxInBatch = 0;

    if (m_bSingleBatch)
    {
        CPLAssert(m_iRecordBatch == 0);
        CPLAssert(m_poBatch != nullptr);
        return false;
    }

    CPLAssert((m_iRecordBatch == -1 && m_poRecordBatchReader == nullptr) ||
              (m_iRecordBatch >= 0 && m_poRecordBatchReader != nullptr));

    if (m_poRecordBatchReader == nullptr)
    {
        if (!CreateRecordBatchReader(0))
            return false;
    }

    ++m_iRecordBatch;

    std::shared_ptr<arrow::RecordBatch> poNextBatch;
    auto status = m_poRecordBatchReader->ReadNext(&poNextBatch);
    if (!status.ok())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "ReadNext() failed: %s",
                 status.message().c_str());
        poNextBatch.reset();
    }
    if (poNextBatch == nullptr)
    {
        if (m_iRecordBatch == 1)
        {
            m_iRecordBatch = 0;
            m_bSingleBatch = true;
        }
        else
            m_poBatch.reset();
        return false;
    }
    SetBatch(poNextBatch);

#ifdef DEBUG
    const auto &poColumns = m_poBatch->columns();

    // Sanity checks
    CPLAssert(m_poBatch->num_columns() == (m_bIgnoredFields
                                               ? m_nExpectedBatchColumns
                                               : m_poSchema->num_fields()));

    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
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
        CPL_IGNORE_RET_VAL(iCol);  // to make cppcheck happy

        CPLAssert(iCol < static_cast<int>(poColumns.size()));
        CPLAssert(m_poSchema->fields()[m_anMapFieldIndexToArrowColumn[i][0]]
                      ->type()
                      ->id() == poColumns[iCol]->type_id());
    }

    for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
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
        CPL_IGNORE_RET_VAL(iCol);  // to make cppcheck happy

        CPLAssert(iCol < static_cast<int>(poColumns.size()));
        CPLAssert(m_poSchema->fields()[m_anMapGeomFieldIndexToArrowColumn[i]]
                      ->type()
                      ->id() == poColumns[iCol]->type_id());
    }
#endif

    return true;
}

/************************************************************************/
/*                        SetIgnoredFields()                            */
/************************************************************************/

OGRErr OGRParquetLayer::SetIgnoredFields(const char **papszFields)
{
    m_bIgnoredFields = false;
    m_anRequestedParquetColumns.clear();
    m_anMapFieldIndexToArrayIndex.clear();
    m_anMapGeomFieldIndexToArrayIndex.clear();
    m_nRequestedFIDColumn = -1;
    OGRErr eErr = OGRLayer::SetIgnoredFields(papszFields);
    if (!m_bHasMissingMappingToParquet && eErr == OGRERR_NONE)
    {
        m_bIgnoredFields = papszFields != nullptr && papszFields[0] != nullptr;
        if (m_bIgnoredFields)
        {
            int nBatchColumns = 0;
            if (m_iFIDParquetColumn >= 0)
            {
                m_nRequestedFIDColumn = nBatchColumns;
                nBatchColumns++;
                m_anRequestedParquetColumns.push_back(m_iFIDParquetColumn);
            }

            for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
            {
                const auto eArrowType =
                    m_poSchema->fields()[m_anMapFieldIndexToArrowColumn[i][0]]
                        ->type()
                        ->id();
                if (eArrowType == arrow::Type::STRUCT)
                {
                    // For a struct, for the sake of simplicity in
                    // GetNextRawFeature(), as soon as one of the member if
                    // requested, request all Parquet columns, so that the Arrow
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
                            const int iParquetCol =
                                m_anMapFieldIndexToParquetColumn[j];
                            CPLAssert(iParquetCol >= 0);
                            if (!m_poFeatureDefn->GetFieldDefn(j)->IsIgnored())
                            {
                                m_anMapFieldIndexToArrayIndex.push_back(
                                    nBatchColumns);
                            }
                            else
                            {
                                m_anMapFieldIndexToArrayIndex.push_back(-1);
                            }
                            m_anRequestedParquetColumns.push_back(iParquetCol);
                        }
                        i = j - 1;
                        nBatchColumns++;
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
                    const int iParquetCol = m_anMapFieldIndexToParquetColumn[i];
                    CPLAssert(iParquetCol >= 0);
                    m_anMapFieldIndexToArrayIndex.push_back(nBatchColumns);
                    nBatchColumns++;
                    m_anRequestedParquetColumns.push_back(iParquetCol);
                    if (eArrowType == arrow::Type::MAP)
                    {
                        // For a map, request both keys and items Parquet
                        // columns
                        m_anRequestedParquetColumns.push_back(iParquetCol + 1);
                    }
                }
                else
                {
                    m_anMapFieldIndexToArrayIndex.push_back(-1);
                }
            }

            CPLAssert(static_cast<int>(m_anMapFieldIndexToArrayIndex.size()) ==
                      m_poFeatureDefn->GetFieldCount());

            for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
            {
                if (!m_poFeatureDefn->GetGeomFieldDefn(i)->IsIgnored())
                {
                    const int iParquetCol =
                        m_anMapGeomFieldIndexToParquetColumn[i];
                    CPLAssert(iParquetCol >= 0);
                    m_anMapGeomFieldIndexToArrayIndex.push_back(nBatchColumns);
                    nBatchColumns++;
                    m_anRequestedParquetColumns.push_back(iParquetCol);
                }
                else
                {
                    m_anMapGeomFieldIndexToArrayIndex.push_back(-1);
                }
            }

            CPLAssert(
                static_cast<int>(m_anMapGeomFieldIndexToArrayIndex.size()) ==
                m_poFeatureDefn->GetGeomFieldCount());
#ifdef DEBUG
            m_nExpectedBatchColumns = nBatchColumns;
#endif
        }
    }

    // Full invalidation
    m_iRecordBatch = -1;
    m_bSingleBatch = false;
    ResetReading();

    return eErr;
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRParquetLayer::GetFeatureCount(int bForce)
{
    if (m_poAttrQuery == nullptr && m_poFilterGeom == nullptr)
    {
        auto metadata = m_poArrowReader->parquet_reader()->metadata();
        if (metadata)
            return metadata->num_rows();
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRParquetLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCIgnoreFields))
        return !m_bHasMissingMappingToParquet;

    return OGRParquetLayerBase::TestCapability(pszCap);
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char *OGRParquetLayer::GetMetadataItem(const char *pszName,
                                             const char *pszDomain)
{
    // Mostly for unit test purposes
    if (pszDomain != nullptr && EQUAL(pszDomain, "_PARQUET_"))
    {
        int nRowGroupIdx = -1;
        int nColumn = -1;
        if (EQUAL(pszName, "NUM_ROW_GROUPS"))
        {
            return CPLSPrintf("%d", m_poArrowReader->num_row_groups());
        }
        if (EQUAL(pszName, "CREATOR"))
        {
            return CPLSPrintf("%s", m_poArrowReader->parquet_reader()
                                        ->metadata()
                                        ->created_by()
                                        .c_str());
        }
        else if (sscanf(pszName, "ROW_GROUPS[%d]", &nRowGroupIdx) == 1 &&
                 strstr(pszName, ".NUM_ROWS"))
        {
            try
            {
                auto poRowGroup =
                    m_poArrowReader->parquet_reader()->RowGroup(nRowGroupIdx);
                if (poRowGroup == nullptr)
                    return nullptr;
                return CPLSPrintf("%" PRId64,
                                  poRowGroup->metadata()->num_rows());
            }
            catch (const std::exception &)
            {
            }
        }
        else if (sscanf(pszName, "ROW_GROUPS[%d].COLUMNS[%d]", &nRowGroupIdx,
                        &nColumn) == 2 &&
                 strstr(pszName, ".COMPRESSION"))
        {
            try
            {
                auto poRowGroup =
                    m_poArrowReader->parquet_reader()->RowGroup(nRowGroupIdx);
                if (poRowGroup == nullptr)
                    return nullptr;
                auto poColumn = poRowGroup->metadata()->ColumnChunk(nColumn);
                return CPLSPrintf("%s", arrow::util::Codec::GetCodecAsString(
                                            poColumn->compression())
                                            .c_str());
            }
            catch (const std::exception &)
            {
            }
        }
        return nullptr;
    }
    if (pszDomain != nullptr && EQUAL(pszDomain, "_PARQUET_METADATA_"))
    {
        const auto metadata = m_poArrowReader->parquet_reader()->metadata();
        const auto &kv_metadata = metadata->key_value_metadata();
        if (kv_metadata && kv_metadata->Contains(pszName))
        {
            auto metadataItem = kv_metadata->Get(pszName);
            if (metadataItem.ok())
            {
                return CPLSPrintf("%s", metadataItem->c_str());
            }
        }
        return nullptr;
    }
    return OGRLayer::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char **OGRParquetLayer::GetMetadata(const char *pszDomain)
{
    // Mostly for unit test purposes
    if (pszDomain != nullptr && EQUAL(pszDomain, "_PARQUET_METADATA_"))
    {
        m_aosFeatherMetadata.Clear();
        const auto metadata = m_poArrowReader->parquet_reader()->metadata();
        const auto &kv_metadata = metadata->key_value_metadata();
        if (kv_metadata)
        {
            for (const auto &kv : kv_metadata->sorted_pairs())
            {
                m_aosFeatherMetadata.SetNameValue(kv.first.c_str(),
                                                  kv.second.c_str());
            }
        }
        return m_aosFeatherMetadata.List();
    }
    return OGRLayer::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetArrowStream()                            */
/************************************************************************/

bool OGRParquetLayer::GetArrowStream(struct ArrowArrayStream *out_stream,
                                     CSLConstList papszOptions)
{
    const char *pszMaxFeaturesInBatch =
        CSLFetchNameValue(papszOptions, "MAX_FEATURES_IN_BATCH");
    if (pszMaxFeaturesInBatch)
    {
        int nMaxBatchSize = atoi(pszMaxFeaturesInBatch);
        if (nMaxBatchSize <= 0)
            nMaxBatchSize = 1;
        if (nMaxBatchSize > INT_MAX - 1)
            nMaxBatchSize = INT_MAX - 1;
        m_poArrowReader->set_batch_size(nMaxBatchSize);
    }
    return OGRArrowLayer::GetArrowStream(out_stream, papszOptions);
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRParquetLayer::SetNextByIndex(GIntBig nIndex)
{
    if (nIndex < 0)
        return OGRERR_FAILURE;

    const auto metadata = m_poArrowReader->parquet_reader()->metadata();
    if (nIndex >= metadata->num_rows())
        return OGRERR_FAILURE;

    if (m_bSingleBatch)
    {
        ResetReading();
        m_nIdxInBatch = nIndex;
        m_nFeatureIdx = nIndex;
        return OGRERR_NONE;
    }

    const int nNumGroups = m_poArrowReader->num_row_groups();
    int64_t nAccRows = 0;
    const auto nBatchSize = m_poArrowReader->properties().batch_size();
    m_iRecordBatch = -1;
    ResetReading();
    m_iRecordBatch = 0;
    for (int iGroup = 0; iGroup < nNumGroups; ++iGroup)
    {
        const int64_t nNextAccRows =
            nAccRows + metadata->RowGroup(iGroup)->num_rows();
        if (nIndex < nNextAccRows)
        {
            if (!CreateRecordBatchReader(iGroup))
                return OGRERR_FAILURE;

            std::shared_ptr<arrow::RecordBatch> poBatch;
            while (true)
            {
                auto status = m_poRecordBatchReader->ReadNext(&poBatch);
                if (!status.ok())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "ReadNext() failed: %s", status.message().c_str());
                    m_iRecordBatch = -1;
                    ResetReading();
                    return OGRERR_FAILURE;
                }
                if (poBatch == nullptr)
                {
                    m_iRecordBatch = -1;
                    ResetReading();
                    return OGRERR_FAILURE;
                }
                if (nIndex < nAccRows + poBatch->num_rows())
                {
                    break;
                }
                nAccRows += poBatch->num_rows();
                m_iRecordBatch++;
            }
            m_nIdxInBatch = nIndex - nAccRows;
            m_nFeatureIdx = nIndex;
            SetBatch(poBatch);
            return OGRERR_NONE;
        }
        nAccRows = nNextAccRows;
        m_iRecordBatch +=
            (metadata->RowGroup(iGroup)->num_rows() + nBatchSize - 1) /
            nBatchSize;
    }

    m_iRecordBatch = -1;
    ResetReading();
    return OGRERR_FAILURE;
}
