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
                                         const char *pszLayerName,
                                         CSLConstList papszOpenOptions)
    : OGRArrowLayer(poDS, pszLayerName), m_poDS(poDS),
      m_aosGeomPossibleNames(CSLTokenizeString2(
          CSLFetchNameValueDef(papszOpenOptions, "GEOM_POSSIBLE_NAMES",
                               "geometry,wkb_geometry,wkt_geometry"),
          ",", 0)),
      m_osCRS(CSLFetchNameValueDef(papszOpenOptions, "CRS", ""))
{
}

/************************************************************************/
/*                           GetDataset()                               */
/************************************************************************/

GDALDataset *OGRParquetLayerBase::GetDataset()
{
    return m_poDS;
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

void OGRParquetLayerBase::ResetReading()
{
    if (m_iRecordBatch != 0)
    {
        m_poRecordBatchReader.reset();
    }
    OGRArrowLayer::ResetReading();
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
                    osVersion != "1.0.0-beta.1" && osVersion != "1.0.0-rc.1" &&
                    osVersion != "1.0.0" && osVersion != "1.1.0")
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
/*                   ParseGeometryColumnCovering()                      */
/************************************************************************/

//! Parse bounding box column definition
static bool ParseGeometryColumnCovering(const CPLJSONObject &oJSONDef,
                                        std::string &osBBOXColumn,
                                        std::string &osXMin,
                                        std::string &osYMin,
                                        std::string &osXMax,
                                        std::string &osYMax)
{
    const auto oCovering = oJSONDef["covering"];
    if (oCovering.IsValid() &&
        oCovering.GetType() == CPLJSONObject::Type::Object)
    {
        const auto oBBOX = oCovering["bbox"];
        if (oBBOX.IsValid() && oBBOX.GetType() == CPLJSONObject::Type::Object)
        {
            const auto oXMin = oBBOX["xmin"];
            const auto oYMin = oBBOX["ymin"];
            const auto oXMax = oBBOX["xmax"];
            const auto oYMax = oBBOX["ymax"];
            if (oXMin.IsValid() && oYMin.IsValid() && oXMax.IsValid() &&
                oYMax.IsValid() &&
                oXMin.GetType() == CPLJSONObject::Type::Array &&
                oYMin.GetType() == CPLJSONObject::Type::Array &&
                oXMax.GetType() == CPLJSONObject::Type::Array &&
                oYMax.GetType() == CPLJSONObject::Type::Array)
            {
                const auto osXMinArray = oXMin.ToArray();
                const auto osYMinArray = oYMin.ToArray();
                const auto osXMaxArray = oXMax.ToArray();
                const auto osYMaxArray = oYMax.ToArray();
                if (osXMinArray.Size() == 2 && osYMinArray.Size() == 2 &&
                    osXMaxArray.Size() == 2 && osYMaxArray.Size() == 2 &&
                    osXMinArray[0].GetType() == CPLJSONObject::Type::String &&
                    osXMinArray[1].GetType() == CPLJSONObject::Type::String &&
                    osYMinArray[0].GetType() == CPLJSONObject::Type::String &&
                    osYMinArray[1].GetType() == CPLJSONObject::Type::String &&
                    osXMaxArray[0].GetType() == CPLJSONObject::Type::String &&
                    osXMaxArray[1].GetType() == CPLJSONObject::Type::String &&
                    osYMaxArray[0].GetType() == CPLJSONObject::Type::String &&
                    osYMaxArray[1].GetType() == CPLJSONObject::Type::String &&
                    osXMinArray[0].ToString() == osYMinArray[0].ToString() &&
                    osXMinArray[0].ToString() == osXMaxArray[0].ToString() &&
                    osXMinArray[0].ToString() == osYMaxArray[0].ToString())
                {
                    osBBOXColumn = osXMinArray[0].ToString();
                    osXMin = osXMinArray[1].ToString();
                    osYMin = osYMinArray[1].ToString();
                    osXMax = osXMaxArray[1].ToString();
                    osYMax = osYMaxArray[1].ToString();
                    return true;
                }
            }
        }
    }
    return false;
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
    auto oIter = m_oMapGeometryColumns.find(field->name());
    if (oIter != m_oMapGeometryColumns.end() ||
        STARTS_WITH(osExtensionName.c_str(), "ogc.") ||
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
        if (IsValidGeometryEncoding(field, osEncoding,
                                    oIter != m_oMapGeometryColumns.end(),
                                    eGeomType, eGeomEncoding))
        {
            bRegularField = false;
            OGRGeomFieldDefn oField(field->name().c_str(), wkbUnknown);

            auto oCRS = oJSONDef["crs"];
            OGRSpatialReference *poSRS = nullptr;
            if (!oCRS.IsValid())
            {
                if (!m_oMapGeometryColumns.empty())
                {
                    // WGS 84 is implied if no crs member is found.
                    poSRS = new OGRSpatialReference();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    poSRS->importFromEPSG(4326);
                }
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

                        if (poSRS->SetFromUserInput(oCRS.ToString().c_str()) !=
                            OGRERR_NONE)
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

            if (!m_osCRS.empty())
            {
                poSRS = new OGRSpatialReference();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (poSRS->SetFromUserInput(m_osCRS.c_str()) == OGRERR_NONE)
                {
                    oField.SetSpatialRef(poSRS);
                }
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
                        eGeomType =
                            GetGeometryTypeFromString(oTypeArray[0].ToString());
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
                            const auto eThisGeom = GetGeometryTypeFromString(
                                oTypeArray[i].ToString());
                            if (PromoteToCollection(OGR_GT_Flatten(
                                    eThisGeom)) != eFirstTypeCollection)
                            {
                                bMixed = true;
                                break;
                            }
                            bHasZ |= OGR_GT_HasZ(eThisGeom) != FALSE;
                            bHasM |= OGR_GT_HasM(eThisGeom) != FALSE;
                            bHasMulti |=
                                (PromoteToCollection(OGR_GT_Flatten(
                                     eThisGeom)) == OGR_GT_Flatten(eThisGeom));
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

    // Try to autodetect a (WKB) geometry column from the GEOM_POSSIBLE_NAMES
    // open option
    if (osExtensionName.empty() && m_oMapGeometryColumns.empty() &&
        m_aosGeomPossibleNames.FindString(field->name().c_str()) >= 0)
    {
        std::shared_ptr<arrow::DataType> fieldType = field->type();
        auto fieldTypeId = fieldType->id();
        if (fieldTypeId == arrow::Type::BINARY ||
            fieldTypeId == arrow::Type::LARGE_BINARY)
        {
            CPLDebug("PARQUET",
                     "Field %s detected as likely WKB geometry field",
                     field->name().c_str());
            bRegularField = false;
            m_aeGeomEncoding.push_back(OGRArrowGeomEncoding::WKB);
        }
        else if ((fieldTypeId == arrow::Type::STRING ||
                  fieldTypeId == arrow::Type::LARGE_STRING) &&
                 (field->name().find("wkt") != std::string::npos ||
                  field->name().find("WKT") != std::string::npos))
        {
            CPLDebug("PARQUET",
                     "Field %s detected as likely WKT geometry field",
                     field->name().c_str());
            bRegularField = false;
            m_aeGeomEncoding.push_back(OGRArrowGeomEncoding::WKT);
        }
        if (!bRegularField)
        {
            OGRGeomFieldDefn oField(field->name().c_str(), wkbUnknown);
            oField.SetNullable(field->nullable());

            if (!m_osCRS.empty())
            {
                auto poSRS = new OGRSpatialReference();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (poSRS->SetFromUserInput(m_osCRS.c_str()) == OGRERR_NONE)
                {
                    oField.SetSpatialRef(poSRS);
                }
                poSRS->Release();
            }

            m_poFeatureDefn->AddGeomFieldDefn(&oField);
            m_anMapGeomFieldIndexToArrowColumn.push_back(iFieldIdx);
        }
    }

    return !bRegularField;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRParquetLayerBase::TestCapability(const char *pszCap)
{
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
    std::unique_ptr<parquet::arrow::FileReader> &&arrow_reader,
    CSLConstList papszOpenOptions)
    : OGRParquetLayerBase(poDS, pszLayerName, papszOpenOptions),
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

    m_oFeatureIdxRemappingIter = m_asFeatureIdxRemapping.begin();
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
        LoadGDALSchema(kv_metadata.get());

    LoadGDALMetadata(kv_metadata.get());

    if (!m_poArrowReader->GetSchema(&m_poSchema).ok())
    {
        return;
    }

    const bool bUseBBOX = CPLTestBool(CPLGetConfigOption(
        ("OGR_" + GetDriverUCName() + "_USE_BBOX").c_str(), "YES"));

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
    const auto poParquetSchema = metadata->schema();

    // Map from Parquet column name (with dot separator) to Parquet index
    std::map<std::string, int> oMapParquetColumnNameToIdx;
    const int nParquetColumns = poParquetSchema->num_columns();
    for (int iParquetCol = 0; iParquetCol < nParquetColumns; ++iParquetCol)
    {
        const auto parquetColumn = poParquetSchema->Column(iParquetCol);
        const auto parquetColumnName = parquetColumn->path()->ToDotString();
        oMapParquetColumnNameToIdx[parquetColumnName] = iParquetCol;
    }

    // Synthetize a GeoParquet bounding box column definition when detecting
    // a Overture Map dataset < 2024-04-16-beta.0
    if ((m_oMapGeometryColumns.empty() ||
         // Below is for release 2024-01-17-alpha.0
         (m_oMapGeometryColumns.find("geometry") !=
              m_oMapGeometryColumns.end() &&
          !m_oMapGeometryColumns["geometry"].GetObj("covering").IsValid() &&
          m_oMapGeometryColumns["geometry"].GetString("encoding") == "WKB")) &&
        bUseBBOX &&
        oMapParquetColumnNameToIdx.find("geometry") !=
            oMapParquetColumnNameToIdx.end() &&
        oMapParquetColumnNameToIdx.find("bbox.minx") !=
            oMapParquetColumnNameToIdx.end() &&
        oMapParquetColumnNameToIdx.find("bbox.miny") !=
            oMapParquetColumnNameToIdx.end() &&
        oMapParquetColumnNameToIdx.find("bbox.maxx") !=
            oMapParquetColumnNameToIdx.end() &&
        oMapParquetColumnNameToIdx.find("bbox.maxy") !=
            oMapParquetColumnNameToIdx.end())
    {
        CPLJSONObject oDef;
        if (m_oMapGeometryColumns.find("geometry") !=
            m_oMapGeometryColumns.end())
        {
            oDef = m_oMapGeometryColumns["geometry"];
        }
        CPLJSONObject oCovering;
        oDef.Add("covering", oCovering);
        CPLJSONObject oBBOX;
        oCovering.Add("bbox", oBBOX);
        {
            CPLJSONArray oArray;
            oArray.Add("bbox");
            oArray.Add("minx");
            oBBOX.Add("xmin", oArray);
        }
        {
            CPLJSONArray oArray;
            oArray.Add("bbox");
            oArray.Add("miny");
            oBBOX.Add("ymin", oArray);
        }
        {
            CPLJSONArray oArray;
            oArray.Add("bbox");
            oArray.Add("maxx");
            oBBOX.Add("xmax", oArray);
        }
        {
            CPLJSONArray oArray;
            oArray.Add("bbox");
            oArray.Add("maxy");
            oBBOX.Add("ymax", oArray);
        }
        oSetBBOXColumns.insert("bbox");
        oDef.Add("encoding", "WKB");
        m_oMapGeometryColumns["geometry"] = std::move(oDef);
    }
    // Overture Maps 2024-04-16-beta.0 almost follows GeoParquet 1.1, except
    // they don't declare the "covering" element in the GeoParquet JSON metadata
    else if (m_oMapGeometryColumns.find("geometry") !=
                 m_oMapGeometryColumns.end() &&
             bUseBBOX &&
             !m_oMapGeometryColumns["geometry"].GetObj("covering").IsValid() &&
             m_oMapGeometryColumns["geometry"].GetString("encoding") == "WKB" &&
             oMapParquetColumnNameToIdx.find("geometry") !=
                 oMapParquetColumnNameToIdx.end() &&
             oMapParquetColumnNameToIdx.find("bbox.xmin") !=
                 oMapParquetColumnNameToIdx.end() &&
             oMapParquetColumnNameToIdx.find("bbox.ymin") !=
                 oMapParquetColumnNameToIdx.end() &&
             oMapParquetColumnNameToIdx.find("bbox.xmax") !=
                 oMapParquetColumnNameToIdx.end() &&
             oMapParquetColumnNameToIdx.find("bbox.ymax") !=
                 oMapParquetColumnNameToIdx.end())
    {
        CPLJSONObject oDef = m_oMapGeometryColumns["geometry"];
        CPLJSONObject oCovering;
        oDef.Add("covering", oCovering);
        CPLJSONObject oBBOX;
        oCovering.Add("bbox", oBBOX);
        {
            CPLJSONArray oArray;
            oArray.Add("bbox");
            oArray.Add("xmin");
            oBBOX.Add("xmin", oArray);
        }
        {
            CPLJSONArray oArray;
            oArray.Add("bbox");
            oArray.Add("ymin");
            oBBOX.Add("ymin", oArray);
        }
        {
            CPLJSONArray oArray;
            oArray.Add("bbox");
            oArray.Add("xmax");
            oBBOX.Add("xmax", oArray);
        }
        {
            CPLJSONArray oArray;
            oArray.Add("bbox");
            oArray.Add("ymax");
            oBBOX.Add("ymax", oArray);
        }
        oSetBBOXColumns.insert("bbox");
        m_oMapGeometryColumns["geometry"] = std::move(oDef);
    }

    int iParquetCol = 0;
    for (int i = 0; i < m_poSchema->num_fields(); ++i)
    {
        const auto &field = fields[i];

        bool bParquetColValid =
            CheckMatchArrowParquetColumnNames(iParquetCol, field);
        if (!bParquetColValid)
            m_bHasMissingMappingToParquet = true;

        if (!m_osFIDColumn.empty() && field->name() == m_osFIDColumn &&
            (field->type()->id() == arrow::Type::INT32 ||
             field->type()->id() == arrow::Type::INT64))
        {
            m_poFIDType = field->type();
            m_iFIDArrowColumn = i;
            if (bParquetColValid)
            {
                m_iFIDParquetColumn = iParquetCol;
                iParquetCol++;
            }
            continue;
        }

        if (oSetBBOXColumns.find(field->name()) != oSetBBOXColumns.end())
        {
            m_oSetBBoxArrowColumns.insert(i);
            if (bParquetColValid)
                iParquetCol++;
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
            const auto oIter = m_oMapGeometryColumns.find(field->name());
            if (bUseBBOX && oIter != m_oMapGeometryColumns.end())
            {
                ProcessGeometryColumnCovering(field, oIter->second,
                                              oMapParquetColumnNameToIdx);
            }

            if (bParquetColValid &&
                (field->type()->id() == arrow::Type::STRUCT ||
                 field->type()->id() == arrow::Type::LIST))
            {
                // GeoArrow types
                std::vector<int> anParquetCols;
                for (const auto &iterParquetCols : oMapParquetColumnNameToIdx)
                {
                    if (STARTS_WITH(
                            iterParquetCols.first.c_str(),
                            std::string(field->name()).append(".").c_str()))
                    {
                        iParquetCol =
                            std::max(iParquetCol, iterParquetCols.second);
                        anParquetCols.push_back(iterParquetCols.second);
                    }
                }
                m_anMapGeomFieldIndexToParquetColumns.push_back(anParquetCols);
                ++iParquetCol;
            }
            else
            {
                m_anMapGeomFieldIndexToParquetColumns.push_back(
                    {bParquetColValid ? iParquetCol : -1});
                if (bParquetColValid)
                    iParquetCol++;
            }
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
    CPLAssert(static_cast<int>(m_anMapGeomFieldIndexToParquetColumns.size()) ==
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
/*                  ProcessGeometryColumnCovering()                     */
/************************************************************************/

/** Process GeoParquet JSON geometry field object to extract information about
 * its bounding box column, and appropriately fill m_oMapGeomFieldIndexToGeomColBBOX
 * and m_oMapGeomFieldIndexToGeomColBBOXParquet members with information on that
 * bounding box column.
 */
void OGRParquetLayer::ProcessGeometryColumnCovering(
    const std::shared_ptr<arrow::Field> &field,
    const CPLJSONObject &oJSONGeometryColumn,
    const std::map<std::string, int> &oMapParquetColumnNameToIdx)
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
            const auto oIterParquetIdxXMin = oMapParquetColumnNameToIdx.find(
                std::string(osBBOXColumn).append(".").append(osXMin));
            const auto oIterParquetIdxYMin = oMapParquetColumnNameToIdx.find(
                std::string(osBBOXColumn).append(".").append(osYMin));
            const auto oIterParquetIdxXMax = oMapParquetColumnNameToIdx.find(
                std::string(osBBOXColumn).append(".").append(osXMax));
            const auto oIterParquetIdxYMax = oMapParquetColumnNameToIdx.find(
                std::string(osBBOXColumn).append(".").append(osYMax));
            if (nXMinIdx >= 0 && nYMinIdx >= 0 && nXMaxIdx >= 0 &&
                nYMaxIdx >= 0 && fieldXMin && fieldYMin && fieldXMax &&
                fieldYMax &&
                oIterParquetIdxXMin != oMapParquetColumnNameToIdx.end() &&
                oIterParquetIdxYMin != oMapParquetColumnNameToIdx.end() &&
                oIterParquetIdxXMax != oMapParquetColumnNameToIdx.end() &&
                oIterParquetIdxYMax != oMapParquetColumnNameToIdx.end() &&
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

                GeomColBBOXParquet sDescParquet;
                sDescParquet.iParquetXMin = oIterParquetIdxXMin->second;
                sDescParquet.iParquetYMin = oIterParquetIdxYMin->second;
                sDescParquet.iParquetXMax = oIterParquetIdxXMax->second;
                sDescParquet.iParquetYMax = oIterParquetIdxYMax->second;
                for (const auto &iterParquetCols : oMapParquetColumnNameToIdx)
                {
                    if (STARTS_WITH(
                            iterParquetCols.first.c_str(),
                            std::string(osBBOXColumn).append(".").c_str()))
                    {
                        sDescParquet.anParquetCols.push_back(
                            iterParquetCols.second);
                    }
                }
                m_oMapGeomFieldIndexToGeomColBBOXParquet
                    [m_poFeatureDefn->GetGeomFieldCount() - 1] =
                        std::move(sDescParquet);
            }
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
    const auto &fieldName = field->name();
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
    OGRParquetLayerBase::ResetReading();
    m_oFeatureIdxRemappingIter = m_asFeatureIdxRemapping.begin();
    m_nFeatureIdxSelected = 0;
    if (!m_asFeatureIdxRemapping.empty())
    {
        m_nFeatureIdx = m_oFeatureIdxRemappingIter->second;
        ++m_oFeatureIdxRemappingIter;
    }
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
    return CreateRecordBatchReader(anRowGroups);
}

bool OGRParquetLayer::CreateRecordBatchReader(
    const std::vector<int> &anRowGroups)
{
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
/*                       IsConstraintPossible()                         */
/************************************************************************/

enum class IsConstraintPossibleRes
{
    YES,
    NO,
    UNKNOWN
};

template <class T>
static IsConstraintPossibleRes IsConstraintPossible(int nOperation, T v, T min,
                                                    T max)
{
    if (nOperation == SWQ_EQ)
    {
        if (v < min || v > max)
        {
            return IsConstraintPossibleRes::NO;
        }
    }
    else if (nOperation == SWQ_NE)
    {
        if (v == min && v == max)
        {
            return IsConstraintPossibleRes::NO;
        }
    }
    else if (nOperation == SWQ_LE)
    {
        if (v < min)
        {
            return IsConstraintPossibleRes::NO;
        }
    }
    else if (nOperation == SWQ_LT)
    {
        if (v <= min)
        {
            return IsConstraintPossibleRes::NO;
        }
    }
    else if (nOperation == SWQ_GE)
    {
        if (v > max)
        {
            return IsConstraintPossibleRes::NO;
        }
    }
    else if (nOperation == SWQ_GT)
    {
        if (v >= max)
        {
            return IsConstraintPossibleRes::NO;
        }
    }
    else
    {
        CPLDebug("PARQUET",
                 "IsConstraintPossible: Unhandled operation type: %d",
                 nOperation);
        return IsConstraintPossibleRes::UNKNOWN;
    }
    return IsConstraintPossibleRes::YES;
}

/************************************************************************/
/*                           IncrFeatureIdx()                           */
/************************************************************************/

void OGRParquetLayer::IncrFeatureIdx()
{
    ++m_nFeatureIdxSelected;
    ++m_nFeatureIdx;
    if (m_iFIDArrowColumn < 0 && !m_asFeatureIdxRemapping.empty() &&
        m_oFeatureIdxRemappingIter != m_asFeatureIdxRemapping.end())
    {
        if (m_nFeatureIdxSelected == m_oFeatureIdxRemappingIter->first)
        {
            m_nFeatureIdx = m_oFeatureIdxRemappingIter->second;
            ++m_oFeatureIdxRemappingIter;
        }
    }
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
        m_asFeatureIdxRemapping.clear();

        bool bIterateEverything = false;
        std::vector<int> anSelectedGroups;
        const auto oIterToGeomColBBOX =
            m_oMapGeomFieldIndexToGeomColBBOXParquet.find(m_iGeomFieldFilter);
        const bool bUSEBBOXFields =
            (m_poFilterGeom &&
             oIterToGeomColBBOX !=
                 m_oMapGeomFieldIndexToGeomColBBOXParquet.end() &&
             CPLTestBool(CPLGetConfigOption(
                 ("OGR_" + GetDriverUCName() + "_USE_BBOX").c_str(), "YES")));
        const bool bIsGeoArrowStruct =
            (m_iGeomFieldFilter >= 0 &&
             m_iGeomFieldFilter < static_cast<int>(m_aeGeomEncoding.size()) &&
             m_iGeomFieldFilter <
                 static_cast<int>(
                     m_anMapGeomFieldIndexToParquetColumns.size()) &&
             m_anMapGeomFieldIndexToParquetColumns[m_iGeomFieldFilter].size() >=
                 2 &&
             (m_aeGeomEncoding[m_iGeomFieldFilter] ==
                  OGRArrowGeomEncoding::GEOARROW_STRUCT_POINT ||
              m_aeGeomEncoding[m_iGeomFieldFilter] ==
                  OGRArrowGeomEncoding::GEOARROW_STRUCT_LINESTRING ||
              m_aeGeomEncoding[m_iGeomFieldFilter] ==
                  OGRArrowGeomEncoding::GEOARROW_STRUCT_POLYGON ||
              m_aeGeomEncoding[m_iGeomFieldFilter] ==
                  OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOINT ||
              m_aeGeomEncoding[m_iGeomFieldFilter] ==
                  OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTILINESTRING ||
              m_aeGeomEncoding[m_iGeomFieldFilter] ==
                  OGRArrowGeomEncoding::GEOARROW_STRUCT_MULTIPOLYGON));

        if (m_asAttributeFilterConstraints.empty() && !bUSEBBOXFields &&
            !(bIsGeoArrowStruct && m_poFilterGeom))
        {
            bIterateEverything = true;
        }
        else
        {
            const int nNumGroups = m_poArrowReader->num_row_groups();
            OGRField sMin;
            OGRField sMax;
            OGR_RawField_SetNull(&sMin);
            OGR_RawField_SetNull(&sMax);
            bool bFoundMin = false;
            bool bFoundMax = false;
            OGRFieldType eType = OFTMaxType;
            OGRFieldSubType eSubType = OFSTNone;
            std::string osMinTmp, osMaxTmp;
            int64_t nFeatureIdxSelected = 0;
            int64_t nFeatureIdxTotal = 0;

            int iXMinField = -1;
            int iYMinField = -1;
            int iXMaxField = -1;
            int iYMaxField = -1;

            if (bIsGeoArrowStruct)
            {
                const auto metadata =
                    m_poArrowReader->parquet_reader()->metadata();
                const auto poParquetSchema = metadata->schema();
                for (int iParquetCol :
                     m_anMapGeomFieldIndexToParquetColumns[m_iGeomFieldFilter])
                {
                    const auto parquetColumn =
                        poParquetSchema->Column(iParquetCol);
                    const auto parquetColumnName =
                        parquetColumn->path()->ToDotString();
                    if (parquetColumnName.size() > 2 &&
                        parquetColumnName.find(".x") ==
                            parquetColumnName.size() - 2)
                    {
                        iXMinField = iParquetCol;
                        iXMaxField = iParquetCol;
                    }
                    else if (parquetColumnName.size() > 2 &&
                             parquetColumnName.find(".y") ==
                                 parquetColumnName.size() - 2)
                    {
                        iYMinField = iParquetCol;
                        iYMaxField = iParquetCol;
                    }
                }
            }
            else if (bUSEBBOXFields)
            {
                iXMinField = oIterToGeomColBBOX->second.iParquetXMin;
                iYMinField = oIterToGeomColBBOX->second.iParquetYMin;
                iXMaxField = oIterToGeomColBBOX->second.iParquetXMax;
                iYMaxField = oIterToGeomColBBOX->second.iParquetYMax;
            }

            for (int iRowGroup = 0;
                 iRowGroup < nNumGroups && !bIterateEverything; ++iRowGroup)
            {
                bool bSelectGroup = true;
                auto poRowGroup =
                    GetReader()->parquet_reader()->RowGroup(iRowGroup);

                if (iXMinField >= 0 && iYMinField >= 0 && iXMaxField >= 0 &&
                    iYMaxField >= 0)
                {
                    if (GetMinMaxForParquetCol(iRowGroup, iXMinField, nullptr,
                                               true, sMin, bFoundMin, false,
                                               sMax, bFoundMax, eType, eSubType,
                                               osMinTmp, osMaxTmp) &&
                        bFoundMin && eType == OFTReal)
                    {
                        const double dfGroupMinX = sMin.Real;
                        if (dfGroupMinX > m_sFilterEnvelope.MaxX)
                        {
                            bSelectGroup = false;
                        }
                        else if (GetMinMaxForParquetCol(
                                     iRowGroup, iYMinField, nullptr, true, sMin,
                                     bFoundMin, false, sMax, bFoundMax, eType,
                                     eSubType, osMinTmp, osMaxTmp) &&
                                 bFoundMin && eType == OFTReal)
                        {
                            const double dfGroupMinY = sMin.Real;
                            if (dfGroupMinY > m_sFilterEnvelope.MaxY)
                            {
                                bSelectGroup = false;
                            }
                            else if (GetMinMaxForParquetCol(
                                         iRowGroup, iXMaxField, nullptr, false,
                                         sMin, bFoundMin, true, sMax, bFoundMax,
                                         eType, eSubType, osMinTmp, osMaxTmp) &&
                                     bFoundMax && eType == OFTReal)
                            {
                                const double dfGroupMaxX = sMax.Real;
                                if (dfGroupMaxX < m_sFilterEnvelope.MinX)
                                {
                                    bSelectGroup = false;
                                }
                                else if (GetMinMaxForParquetCol(
                                             iRowGroup, iYMaxField, nullptr,
                                             false, sMin, bFoundMin, true, sMax,
                                             bFoundMax, eType, eSubType,
                                             osMinTmp, osMaxTmp) &&
                                         bFoundMax && eType == OFTReal)
                                {
                                    const double dfGroupMaxY = sMax.Real;
                                    if (dfGroupMaxY < m_sFilterEnvelope.MinY)
                                    {
                                        bSelectGroup = false;
                                    }
                                }
                            }
                        }
                    }
                }

                if (bSelectGroup)
                {
                    for (auto &constraint : m_asAttributeFilterConstraints)
                    {
                        int iOGRField = constraint.iField;
                        if (constraint.iField ==
                            m_poFeatureDefn->GetFieldCount() + SPF_FID)
                        {
                            iOGRField = OGR_FID_INDEX;
                        }
                        if (constraint.nOperation != SWQ_ISNULL &&
                            constraint.nOperation != SWQ_ISNOTNULL)
                        {
                            if (iOGRField == OGR_FID_INDEX &&
                                m_iFIDParquetColumn < 0)
                            {
                                sMin.Integer64 = nFeatureIdxTotal;
                                sMax.Integer64 =
                                    nFeatureIdxTotal +
                                    poRowGroup->metadata()->num_rows() - 1;
                                eType = OFTInteger64;
                            }
                            else if (!GetMinMaxForOGRField(
                                         iRowGroup, iOGRField, true, sMin,
                                         bFoundMin, true, sMax, bFoundMax,
                                         eType, eSubType, osMinTmp, osMaxTmp) ||
                                     !bFoundMin || !bFoundMax)
                            {
                                bIterateEverything = true;
                                break;
                            }
                        }

                        IsConstraintPossibleRes res =
                            IsConstraintPossibleRes::UNKNOWN;
                        if (constraint.eType ==
                                OGRArrowLayer::Constraint::Type::Integer &&
                            eType == OFTInteger)
                        {
#if 0
                            CPLDebug("PARQUET",
                                     "Group %d, field %s, min = %d, max = %d",
                                     iRowGroup,
                                     iOGRField == OGR_FID_INDEX
                                         ? m_osFIDColumn.c_str()
                                         : m_poFeatureDefn->GetFieldDefn(iOGRField)
                                               ->GetNameRef(),
                                     sMin.Integer, sMax.Integer);
#endif
                            res = IsConstraintPossible(
                                constraint.nOperation,
                                constraint.sValue.Integer, sMin.Integer,
                                sMax.Integer);
                        }
                        else if (constraint.eType == OGRArrowLayer::Constraint::
                                                         Type::Integer64 &&
                                 eType == OFTInteger64)
                        {
#if 0
                            CPLDebug("PARQUET",
                                     "Group %d, field %s, min = " CPL_FRMT_GIB
                                     ", max = " CPL_FRMT_GIB,
                                     iRowGroup,
                                     iOGRField == OGR_FID_INDEX
                                         ? m_osFIDColumn.c_str()
                                         : m_poFeatureDefn->GetFieldDefn(iOGRField)
                                               ->GetNameRef(),
                                     static_cast<GIntBig>(sMin.Integer64),
                                     static_cast<GIntBig>(sMax.Integer64));
#endif
                            res = IsConstraintPossible(
                                constraint.nOperation,
                                constraint.sValue.Integer64, sMin.Integer64,
                                sMax.Integer64);
                        }
                        else if (constraint.eType ==
                                     OGRArrowLayer::Constraint::Type::Real &&
                                 eType == OFTReal)
                        {
#if 0
                            CPLDebug("PARQUET",
                                     "Group %d, field %s, min = %g, max = %g",
                                     iRowGroup,
                                     iOGRField == OGR_FID_INDEX
                                         ? m_osFIDColumn.c_str()
                                         : m_poFeatureDefn->GetFieldDefn(iOGRField)
                                               ->GetNameRef(),
                                     sMin.Real, sMax.Real);
#endif
                            res = IsConstraintPossible(constraint.nOperation,
                                                       constraint.sValue.Real,
                                                       sMin.Real, sMax.Real);
                        }
                        else if (constraint.eType ==
                                     OGRArrowLayer::Constraint::Type::String &&
                                 eType == OFTString)
                        {
#if 0
                            CPLDebug("PARQUET",
                                     "Group %d, field %s, min = %s, max = %s",
                                     iRowGroup,
                                     iOGRField == OGR_FID_INDEX
                                         ? m_osFIDColumn.c_str()
                                         : m_poFeatureDefn->GetFieldDefn(iOGRField)
                                               ->GetNameRef(),
                                     sMin.String, sMax.String);
#endif
                            res = IsConstraintPossible(
                                constraint.nOperation,
                                std::string(constraint.sValue.String),
                                std::string(sMin.String),
                                std::string(sMax.String));
                        }
                        else if (constraint.nOperation == SWQ_ISNULL ||
                                 constraint.nOperation == SWQ_ISNOTNULL)
                        {
                            const int iCol =
                                iOGRField == OGR_FID_INDEX
                                    ? m_iFIDParquetColumn
                                    : GetMapFieldIndexToParquetColumn()
                                          [iOGRField];
                            if (iCol >= 0)
                            {
                                const auto metadata =
                                    m_poArrowReader->parquet_reader()
                                        ->metadata();
                                const auto rowGroupColumnChunk =
                                    metadata->RowGroup(iRowGroup)->ColumnChunk(
                                        iCol);
                                const auto rowGroupStats =
                                    rowGroupColumnChunk->statistics();
                                if (rowGroupColumnChunk->is_stats_set() &&
                                    rowGroupStats)
                                {
                                    res = IsConstraintPossibleRes::YES;
                                    if (constraint.nOperation == SWQ_ISNULL &&
                                        rowGroupStats->num_values() ==
                                            poRowGroup->metadata()->num_rows())
                                    {
                                        res = IsConstraintPossibleRes::NO;
                                    }
                                    else if (constraint.nOperation ==
                                                 SWQ_ISNOTNULL &&
                                             rowGroupStats->num_values() == 0)
                                    {
                                        res = IsConstraintPossibleRes::NO;
                                    }
                                }
                            }
                        }
                        else
                        {
                            CPLDebug(
                                "PARQUET",
                                "Unhandled combination of constraint.eType "
                                "(%d) and eType (%d)",
                                static_cast<int>(constraint.eType), eType);
                        }

                        if (res == IsConstraintPossibleRes::NO)
                        {
                            bSelectGroup = false;
                            break;
                        }
                        else if (res == IsConstraintPossibleRes::UNKNOWN)
                        {
                            bIterateEverything = true;
                            break;
                        }
                    }
                }

                if (bSelectGroup)
                {
                    // CPLDebug("PARQUET", "Selecting row group %d", iRowGroup);
                    m_asFeatureIdxRemapping.emplace_back(
                        std::make_pair(nFeatureIdxSelected, nFeatureIdxTotal));
                    anSelectedGroups.push_back(iRowGroup);
                    nFeatureIdxSelected += poRowGroup->metadata()->num_rows();
                }

                nFeatureIdxTotal += poRowGroup->metadata()->num_rows();
            }
        }

        if (bIterateEverything)
        {
            m_asFeatureIdxRemapping.clear();
            m_oFeatureIdxRemappingIter = m_asFeatureIdxRemapping.begin();
            if (!CreateRecordBatchReader(0))
                return false;
        }
        else
        {
            m_oFeatureIdxRemappingIter = m_asFeatureIdxRemapping.begin();
            if (anSelectedGroups.empty())
            {
                return false;
            }
            CPLDebug("PARQUET", "%d/%d row groups selected",
                     int(anSelectedGroups.size()),
                     m_poArrowReader->num_row_groups());
            m_nFeatureIdx = m_oFeatureIdxRemappingIter->second;
            ++m_oFeatureIdxRemappingIter;
            if (!CreateRecordBatchReader(anSelectedGroups))
            {
                return false;
            }
        }
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
            if (m_iRecordBatch == 1 && m_poBatch && m_poAttrQuery == nullptr &&
                m_poFilterGeom == nullptr)
            {
                m_iRecordBatch = 0;
                m_bSingleBatch = true;
            }
            else
                m_poBatch.reset();
            return false;
        }
    } while (poNextBatch->num_rows() == 0);

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
/*                     InvalidateCachedBatches()                        */
/************************************************************************/

void OGRParquetLayer::InvalidateCachedBatches()
{
    m_iRecordBatch = -1;
    m_bSingleBatch = false;
    ResetReading();
}

/************************************************************************/
/*                        SetIgnoredFields()                            */
/************************************************************************/

OGRErr OGRParquetLayer::SetIgnoredFields(CSLConstList papszFields)
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
                    const auto &anVals =
                        m_anMapGeomFieldIndexToParquetColumns[i];
                    CPLAssert(!anVals.empty() && anVals[0] >= 0);
                    m_anRequestedParquetColumns.insert(
                        m_anRequestedParquetColumns.end(), anVals.begin(),
                        anVals.end());
                    m_anMapGeomFieldIndexToArrayIndex.push_back(nBatchColumns);
                    nBatchColumns++;

                    auto oIter = m_oMapGeomFieldIndexToGeomColBBOX.find(i);
                    const auto oIterParquet =
                        m_oMapGeomFieldIndexToGeomColBBOXParquet.find(i);
                    if (oIter != m_oMapGeomFieldIndexToGeomColBBOX.end() &&
                        oIterParquet !=
                            m_oMapGeomFieldIndexToGeomColBBOXParquet.end())
                    {
                        const bool bIsGeoArrowStruct =
                            (m_aeGeomEncoding[i] ==
                                 OGRArrowGeomEncoding::GEOARROW_STRUCT_POINT ||
                             m_aeGeomEncoding[i] ==
                                 OGRArrowGeomEncoding::
                                     GEOARROW_STRUCT_LINESTRING ||
                             m_aeGeomEncoding[i] ==
                                 OGRArrowGeomEncoding::
                                     GEOARROW_STRUCT_POLYGON ||
                             m_aeGeomEncoding[i] ==
                                 OGRArrowGeomEncoding::
                                     GEOARROW_STRUCT_MULTIPOINT ||
                             m_aeGeomEncoding[i] ==
                                 OGRArrowGeomEncoding::
                                     GEOARROW_STRUCT_MULTILINESTRING ||
                             m_aeGeomEncoding[i] ==
                                 OGRArrowGeomEncoding::
                                     GEOARROW_STRUCT_MULTIPOLYGON);
                        if (!bIsGeoArrowStruct)
                        {
                            oIter->second.iArrayIdx = nBatchColumns++;
                            m_anRequestedParquetColumns.insert(
                                m_anRequestedParquetColumns.end(),
                                oIterParquet->second.anParquetCols.begin(),
                                oIterParquet->second.anParquetCols.end());
                        }
                    }
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

    ComputeConstraintsArrayIdx();

    // Full invalidation
    InvalidateCachedBatches();

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
/*                         FastGetExtent()                              */
/************************************************************************/

bool OGRParquetLayer::FastGetExtent(int iGeomField, OGREnvelope *psExtent) const
{
    if (OGRParquetLayerBase::FastGetExtent(iGeomField, psExtent))
        return true;

    const auto oIterToGeomColBBOX =
        m_oMapGeomFieldIndexToGeomColBBOXParquet.find(iGeomField);
    if (oIterToGeomColBBOX != m_oMapGeomFieldIndexToGeomColBBOXParquet.end() &&
        CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_BBOX", "YES")))
    {
        OGREnvelope sExtent;
        OGRField sMin, sMax;
        OGR_RawField_SetNull(&sMin);
        OGR_RawField_SetNull(&sMax);
        bool bFoundMin, bFoundMax;
        OGRFieldType eType = OFTMaxType;
        OGRFieldSubType eSubType = OFSTNone;
        std::string osMinTmp, osMaxTmp;
        if (GetMinMaxForParquetCol(-1, oIterToGeomColBBOX->second.iParquetXMin,
                                   nullptr, true, sMin, bFoundMin, false, sMax,
                                   bFoundMax, eType, eSubType, osMinTmp,
                                   osMaxTmp) &&
            eType == OFTReal)
        {
            sExtent.MinX = sMin.Real;

            if (GetMinMaxForParquetCol(
                    -1, oIterToGeomColBBOX->second.iParquetYMin, nullptr, true,
                    sMin, bFoundMin, false, sMax, bFoundMax, eType, eSubType,
                    osMinTmp, osMaxTmp) &&
                eType == OFTReal)
            {
                sExtent.MinY = sMin.Real;

                if (GetMinMaxForParquetCol(
                        -1, oIterToGeomColBBOX->second.iParquetXMax, nullptr,
                        false, sMin, bFoundMin, true, sMax, bFoundMax, eType,
                        eSubType, osMinTmp, osMaxTmp) &&
                    eType == OFTReal)
                {
                    sExtent.MaxX = sMax.Real;

                    if (GetMinMaxForParquetCol(
                            -1, oIterToGeomColBBOX->second.iParquetYMax,
                            nullptr, false, sMin, bFoundMin, true, sMax,
                            bFoundMax, eType, eSubType, osMinTmp, osMaxTmp) &&
                        eType == OFTReal)
                    {
                        sExtent.MaxY = sMax.Real;

                        CPLDebug("PARQUET",
                                 "Using statistics of bbox.minx, bbox.miny, "
                                 "bbox.maxx, bbox.maxy columns to get extent");
                        m_oMapExtents[iGeomField] = sExtent;
                        *psExtent = sExtent;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRParquetLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poAttrQuery == nullptr && m_poFilterGeom == nullptr;

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

/***********************************************************************/
/*                            GetStats()                               */
/***********************************************************************/

template <class STAT_TYPE> struct GetStats
{
    using T = typename STAT_TYPE::T;

    static T min(const std::shared_ptr<parquet::FileMetaData> &metadata,
                 const int iRowGroup, const int numRowGroups, const int iCol,
                 bool &bFound)
    {
        T v{};
        bFound = false;
        for (int i = 0; i < (iRowGroup < 0 ? numRowGroups : 1); i++)
        {
            const auto columnChunk =
                metadata->RowGroup(iRowGroup < 0 ? i : iRowGroup)
                    ->ColumnChunk(iCol);
            const auto colStats = columnChunk->statistics();
            if (columnChunk->is_stats_set() && colStats &&
                colStats->HasMinMax())
            {
                auto castStats = static_cast<STAT_TYPE *>(colStats.get());
                const auto rowGroupVal = castStats->min();
                if (i == 0 || rowGroupVal < v)
                {
                    bFound = true;
                    v = rowGroupVal;
                }
            }
            else if (columnChunk->num_values() > 0)
            {
                bFound = false;
                break;
            }
        }
        return v;
    }

    static T max(const std::shared_ptr<parquet::FileMetaData> &metadata,
                 const int iRowGroup, const int numRowGroups, const int iCol,
                 bool &bFound)
    {
        T v{};
        bFound = false;
        for (int i = 0; i < (iRowGroup < 0 ? numRowGroups : 1); i++)
        {
            const auto columnChunk =
                metadata->RowGroup(iRowGroup < 0 ? i : iRowGroup)
                    ->ColumnChunk(iCol);
            const auto colStats = columnChunk->statistics();
            if (columnChunk->is_stats_set() && colStats &&
                colStats->HasMinMax())
            {
                auto castStats = static_cast<STAT_TYPE *>(colStats.get());
                const auto rowGroupVal = castStats->max();
                if (i == 0 || rowGroupVal > v)
                {
                    bFound = true;
                    v = rowGroupVal;
                }
            }
            else if (columnChunk->num_values() > 0)
            {
                bFound = false;
                break;
            }
        }
        return v;
    }
};

template <> struct GetStats<parquet::ByteArrayStatistics>
{
    static std::string
    min(const std::shared_ptr<parquet::FileMetaData> &metadata,
        const int iRowGroup, const int numRowGroups, const int iCol,
        bool &bFound)
    {
        std::string v{};
        bFound = false;
        for (int i = 0; i < (iRowGroup < 0 ? numRowGroups : 1); i++)
        {
            const auto columnChunk =
                metadata->RowGroup(iRowGroup < 0 ? i : iRowGroup)
                    ->ColumnChunk(iCol);
            const auto colStats = columnChunk->statistics();
            if (columnChunk->is_stats_set() && colStats &&
                colStats->HasMinMax())
            {
                auto castStats =
                    static_cast<parquet::ByteArrayStatistics *>(colStats.get());
                const auto rowGroupValRaw = castStats->min();
                const std::string rowGroupVal(
                    reinterpret_cast<const char *>(rowGroupValRaw.ptr),
                    rowGroupValRaw.len);
                if (i == 0 || rowGroupVal < v)
                {
                    bFound = true;
                    v = rowGroupVal;
                }
            }
        }
        return v;
    }

    static std::string
    max(const std::shared_ptr<parquet::FileMetaData> &metadata,
        const int iRowGroup, const int numRowGroups, const int iCol,
        bool &bFound)
    {
        std::string v{};
        bFound = false;
        for (int i = 0; i < (iRowGroup < 0 ? numRowGroups : 1); i++)
        {
            const auto columnChunk =
                metadata->RowGroup(iRowGroup < 0 ? i : iRowGroup)
                    ->ColumnChunk(iCol);
            const auto colStats = columnChunk->statistics();
            if (columnChunk->is_stats_set() && colStats &&
                colStats->HasMinMax())
            {
                auto castStats =
                    static_cast<parquet::ByteArrayStatistics *>(colStats.get());
                const auto rowGroupValRaw = castStats->max();
                const std::string rowGroupVal(
                    reinterpret_cast<const char *>(rowGroupValRaw.ptr),
                    rowGroupValRaw.len);
                if (i == 0 || rowGroupVal > v)
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

/************************************************************************/
/*                        GetMinMaxForOGRField()                        */
/************************************************************************/

bool OGRParquetLayer::GetMinMaxForOGRField(int iRowGroup,  // -1 for all
                                           int iOGRField, bool bComputeMin,
                                           OGRField &sMin, bool &bFoundMin,
                                           bool bComputeMax, OGRField &sMax,
                                           bool &bFoundMax, OGRFieldType &eType,
                                           OGRFieldSubType &eSubType,
                                           std::string &osMinTmp,
                                           std::string &osMaxTmp) const
{
    OGR_RawField_SetNull(&sMin);
    OGR_RawField_SetNull(&sMax);
    eType = OFTReal;
    eSubType = OFSTNone;
    bFoundMin = false;
    bFoundMax = false;

    const int iCol = iOGRField == OGR_FID_INDEX
                         ? m_iFIDParquetColumn
                         : GetMapFieldIndexToParquetColumn()[iOGRField];
    if (iCol < 0)
        return false;
    const auto &arrowType = iOGRField == OGR_FID_INDEX
                                ? m_poFIDType
                                : GetArrowFieldTypes()[iOGRField];

    const bool bRet = GetMinMaxForParquetCol(
        iRowGroup, iCol, arrowType, bComputeMin, sMin, bFoundMin, bComputeMax,
        sMax, bFoundMax, eType, eSubType, osMinTmp, osMaxTmp);

    if (eType == OFTInteger64 && arrowType->id() == arrow::Type::TIMESTAMP)
    {
        const OGRFieldDefn oDummyFIDFieldDefn(m_osFIDColumn.c_str(),
                                              OFTInteger64);
        const OGRFieldDefn *poFieldDefn =
            iOGRField == OGR_FID_INDEX ? &oDummyFIDFieldDefn
                                       : const_cast<OGRParquetLayer *>(this)
                                             ->GetLayerDefn()
                                             ->GetFieldDefn(iOGRField);
        if (poFieldDefn->GetType() == OFTDateTime)
        {
            const auto timestampType =
                static_cast<arrow::TimestampType *>(arrowType.get());
            if (bFoundMin)
            {
                const int64_t timestamp = sMin.Integer64;
                OGRArrowLayer::TimestampToOGR(timestamp, timestampType,
                                              poFieldDefn->GetTZFlag(), &sMin);
            }
            if (bFoundMax)
            {
                const int64_t timestamp = sMax.Integer64;
                OGRArrowLayer::TimestampToOGR(timestamp, timestampType,
                                              poFieldDefn->GetTZFlag(), &sMax);
            }
            eType = OFTDateTime;
        }
    }

    return bRet;
}

/************************************************************************/
/*                        GetMinMaxForParquetCol()                      */
/************************************************************************/

bool OGRParquetLayer::GetMinMaxForParquetCol(
    int iRowGroup,  // -1 for all
    int iCol,
    const std::shared_ptr<arrow::DataType> &arrowType,  // potentially nullptr
    bool bComputeMin, OGRField &sMin, bool &bFoundMin, bool bComputeMax,
    OGRField &sMax, bool &bFoundMax, OGRFieldType &eType,
    OGRFieldSubType &eSubType, std::string &osMinTmp,
    std::string &osMaxTmp) const
{
    OGR_RawField_SetNull(&sMin);
    OGR_RawField_SetNull(&sMax);
    eType = OFTReal;
    eSubType = OFSTNone;
    bFoundMin = false;
    bFoundMax = false;

    const auto metadata = GetReader()->parquet_reader()->metadata();
    const auto numRowGroups = metadata->num_row_groups();

    if (numRowGroups == 0)
        return false;

    const auto rowGroup0 = metadata->RowGroup(0);
    if (iCol < 0 || iCol >= rowGroup0->num_columns())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetMinMaxForParquetCol(): invalid iCol=%d", iCol);
        return false;
    }
    const auto rowGroup0columnChunk = rowGroup0->ColumnChunk(iCol);
    const auto rowGroup0Stats = rowGroup0columnChunk->statistics();
    if (!(rowGroup0columnChunk->is_stats_set() && rowGroup0Stats))
    {
        CPLDebug("PARQUET", "Statistics not available for field %s",
                 rowGroup0columnChunk->path_in_schema()->ToDotString().c_str());
        return false;
    }

    const auto physicalType = rowGroup0Stats->physical_type();

    if (bComputeMin)
    {
        if (physicalType == parquet::Type::BOOLEAN)
        {
            eType = OFTInteger;
            eSubType = OFSTBoolean;
            sMin.Integer = GetStats<parquet::BoolStatistics>::min(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMin);
        }
        else if (physicalType == parquet::Type::INT32)
        {
            if (arrowType && arrowType->id() == arrow::Type::UINT32)
            {
                // With parquet file version 2.0,
                // statistics of uint32 fields are
                // stored as signed int32 values...
                eType = OFTInteger64;
                int nVal = GetStats<parquet::Int32Statistics>::min(
                    metadata, iRowGroup, numRowGroups, iCol, bFoundMin);
                if (bFoundMin)
                {
                    sMin.Integer64 = static_cast<uint32_t>(nVal);
                }
            }
            else
            {
                eType = OFTInteger;
                if (arrowType && arrowType->id() == arrow::Type::INT16)
                    eSubType = OFSTInt16;
                sMin.Integer = GetStats<parquet::Int32Statistics>::min(
                    metadata, iRowGroup, numRowGroups, iCol, bFoundMin);
            }
        }
        else if (physicalType == parquet::Type::INT64)
        {
            eType = OFTInteger64;
            sMin.Integer64 = GetStats<parquet::Int64Statistics>::min(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMin);
        }
        else if (physicalType == parquet::Type::FLOAT)
        {
            eType = OFTReal;
            eSubType = OFSTFloat32;
            sMin.Real = GetStats<parquet::FloatStatistics>::min(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMin);
        }
        else if (physicalType == parquet::Type::DOUBLE)
        {
            eType = OFTReal;
            sMin.Real = GetStats<parquet::DoubleStatistics>::min(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMin);
        }
        else if (arrowType &&
                 (arrowType->id() == arrow::Type::STRING ||
                  arrowType->id() == arrow::Type::LARGE_STRING) &&
                 physicalType == parquet::Type::BYTE_ARRAY)
        {
            osMinTmp = GetStats<parquet::ByteArrayStatistics>::min(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMin);
            if (bFoundMin)
            {
                eType = OFTString;
                sMin.String = &osMinTmp[0];
            }
        }
    }

    if (bComputeMax)
    {
        if (physicalType == parquet::Type::BOOLEAN)
        {
            eType = OFTInteger;
            eSubType = OFSTBoolean;
            sMax.Integer = GetStats<parquet::BoolStatistics>::max(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMax);
        }
        else if (physicalType == parquet::Type::INT32)
        {
            if (arrowType && arrowType->id() == arrow::Type::UINT32)
            {
                // With parquet file version 2.0,
                // statistics of uint32 fields are
                // stored as signed int32 values...
                eType = OFTInteger64;
                int nVal = GetStats<parquet::Int32Statistics>::max(
                    metadata, iRowGroup, numRowGroups, iCol, bFoundMax);
                if (bFoundMax)
                {
                    sMax.Integer64 = static_cast<uint32_t>(nVal);
                }
            }
            else
            {
                eType = OFTInteger;
                if (arrowType && arrowType->id() == arrow::Type::INT16)
                    eSubType = OFSTInt16;
                sMax.Integer = GetStats<parquet::Int32Statistics>::max(
                    metadata, iRowGroup, numRowGroups, iCol, bFoundMax);
            }
        }
        else if (physicalType == parquet::Type::INT64)
        {
            eType = OFTInteger64;
            sMax.Integer64 = GetStats<parquet::Int64Statistics>::max(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMax);
        }
        else if (physicalType == parquet::Type::FLOAT)
        {
            eType = OFTReal;
            eSubType = OFSTFloat32;
            sMax.Real = GetStats<parquet::FloatStatistics>::max(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMax);
        }
        else if (physicalType == parquet::Type::DOUBLE)
        {
            eType = OFTReal;
            sMax.Real = GetStats<parquet::DoubleStatistics>::max(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMax);
        }
        else if (arrowType &&
                 (arrowType->id() == arrow::Type::STRING ||
                  arrowType->id() == arrow::Type::LARGE_STRING) &&
                 physicalType == parquet::Type::BYTE_ARRAY)
        {
            osMaxTmp = GetStats<parquet::ByteArrayStatistics>::max(
                metadata, iRowGroup, numRowGroups, iCol, bFoundMax);
            if (bFoundMax)
            {
                eType = OFTString;
                sMax.String = &osMaxTmp[0];
            }
        }
    }

    return bFoundMin || bFoundMax;
}

/************************************************************************/
/*                        GeomColsBBOXParquet()                         */
/************************************************************************/

/** Return for a given geometry column (iGeom: in [0, GetGeomFieldCount()-1] range),
 * the Parquet column number of the corresponding xmin,ymin,xmax,ymax bounding
 * box columns, if existing.
 */
bool OGRParquetLayer::GeomColsBBOXParquet(int iGeom, int &iParquetXMin,
                                          int &iParquetYMin, int &iParquetXMax,
                                          int &iParquetYMax) const
{
    const auto oIter = m_oMapGeomFieldIndexToGeomColBBOXParquet.find(iGeom);
    const bool bFound =
        (oIter != m_oMapGeomFieldIndexToGeomColBBOXParquet.end());
    if (bFound)
    {
        iParquetXMin = oIter->second.iParquetXMin;
        iParquetYMin = oIter->second.iParquetYMin;
        iParquetXMax = oIter->second.iParquetXMax;
        iParquetYMax = oIter->second.iParquetYMax;
    }
    return bFound;
}
