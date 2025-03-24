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
#include "ogr_spatialref.h"
#include "ogr_p.h"
#include "cpl_json.h"

#include <cmath>
#include <limits>
#include <map>
#include <set>

#define ADBC_CALL(func, ...) m_poDS->m_driver.func(__VA_ARGS__)

/************************************************************************/
/*                        GetGeometryTypeFromString()                   */
/************************************************************************/

static OGRwkbGeometryType GetGeometryTypeFromString(const std::string &osType)
{
    OGRwkbGeometryType eGeomType = wkbUnknown;
    OGRReadWKTGeometryType(osType.c_str(), &eGeomType);
    if (eGeomType == wkbUnknown && !osType.empty())
    {
        CPLDebug("ADBC", "Unknown geometry type: %s", osType.c_str());
    }
    return eGeomType;
}

/************************************************************************/
/*                            OGRADBCLayer()                            */
/************************************************************************/

OGRADBCLayer::OGRADBCLayer(OGRADBCDataset *poDS, const char *pszName,
                           const char *pszStatement,
                           std::unique_ptr<AdbcStatement> poStatement,
                           std::unique_ptr<OGRArrowArrayStream> poStream,
                           ArrowSchema *schema, bool bInternalUse)
    : m_poDS(poDS), m_osBaseStatement(pszStatement),
      m_osModifiedBaseStatement(m_osBaseStatement),
      m_statement(std::move(poStatement)), m_stream(std::move(poStream))
{
    SetDescription(pszName);

    memcpy(&m_schema, schema, sizeof(m_schema));
    schema->release = nullptr;

    BuildLayerDefn(bInternalUse);
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
/*                      ParseGeoParquetColumn()                         */
/************************************************************************/

static void ParseGeoParquetColumn(
    const CPLJSONObject &oColumn,
    std::map<std::string, OGRwkbGeometryType> &oMapType,
    std::map<std::string, OGREnvelope3D> &oMapExtent,
    std::map<std::string, OGRADBCLayer::GeomColBBOX>
        &oMapGeomColumnToCoveringBBOXColumn,
    std::map<std::string, std::unique_ptr<OGRSpatialReference>>
        &oMapGeomColumnsFromGeoParquet,
    std::set<std::string> &oSetCoveringBBoxColumn)
{
    auto oCrs = oColumn.GetObj("crs");
    if (!oCrs.IsValid())
    {
        // WGS 84 is implied if no crs member is found.
        auto poSRS = std::make_unique<OGRSpatialReference>();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poSRS->importFromEPSG(4326);
        oMapGeomColumnsFromGeoParquet[oColumn.GetName()] = std::move(poSRS);
    }
    else if (oCrs.GetType() == CPLJSONObject::Type::Object)
    {
        // CRS encoded as PROJJSON (extension)
        const auto oType = oCrs["type"];
        if (oType.IsValid() && oType.GetType() == CPLJSONObject::Type::String)
        {
            const auto osType = oType.ToString();
            if (osType.find("CRS") != std::string::npos)
            {
                auto poSRS = std::make_unique<OGRSpatialReference>();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

                if (poSRS->SetFromUserInput(oCrs.ToString().c_str()) ==
                    OGRERR_NONE)
                {
                    oMapGeomColumnsFromGeoParquet[oColumn.GetName()] =
                        std::move(poSRS);
                }
            }
        }
    }
    else
    {
        oMapGeomColumnsFromGeoParquet[oColumn.GetName()] = nullptr;
    }

    OGRwkbGeometryType eGeomType = wkbUnknown;
    auto oType = oColumn.GetObj("geometry_types");
    if (oType.GetType() == CPLJSONObject::Type::Array)
    {
        const auto oTypeArray = oType.ToArray();
        if (oTypeArray.Size() == 1)
        {
            eGeomType = GetGeometryTypeFromString(oTypeArray[0].ToString());
        }
        else if (oTypeArray.Size() > 1)
        {
            const auto PromoteToCollection = [](OGRwkbGeometryType eType)
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
            const auto eFirstType = OGR_GT_Flatten(
                GetGeometryTypeFromString(oTypeArray[0].ToString()));
            const auto eFirstTypeCollection = PromoteToCollection(eFirstType);
            for (int i = 0; i < oTypeArray.Size(); ++i)
            {
                const auto eThisGeom =
                    GetGeometryTypeFromString(oTypeArray[i].ToString());
                if (PromoteToCollection(OGR_GT_Flatten(eThisGeom)) !=
                    eFirstTypeCollection)
                {
                    bMixed = true;
                    break;
                }
                bHasZ |= OGR_GT_HasZ(eThisGeom) != FALSE;
                bHasM |= OGR_GT_HasM(eThisGeom) != FALSE;
                bHasMulti |= (PromoteToCollection(OGR_GT_Flatten(eThisGeom)) ==
                              OGR_GT_Flatten(eThisGeom));
            }
            if (!bMixed)
            {
                if (eFirstTypeCollection == wkbMultiPolygon ||
                    eFirstTypeCollection == wkbMultiLineString)
                {
                    if (bHasMulti)
                        eGeomType = OGR_GT_SetModifier(eFirstTypeCollection,
                                                       bHasZ, bHasM);
                    else
                        eGeomType =
                            OGR_GT_SetModifier(eFirstType, bHasZ, bHasM);
                }
            }
        }
    }

    oMapType[oColumn.GetName()] = eGeomType;

    OGREnvelope3D sExtent;
    const auto oBBox = oColumn.GetArray("bbox");
    if (oBBox.IsValid() && oBBox.Size() == 4)
    {
        sExtent.MinX = oBBox[0].ToDouble();
        sExtent.MinY = oBBox[1].ToDouble();
        sExtent.MinZ = std::numeric_limits<double>::infinity();
        sExtent.MaxX = oBBox[2].ToDouble();
        sExtent.MaxY = oBBox[3].ToDouble();
        sExtent.MaxZ = -std::numeric_limits<double>::infinity();
        if (sExtent.MinX <= sExtent.MaxX)
        {
            oMapExtent[oColumn.GetName()] = sExtent;
        }
    }
    else if (oBBox.IsValid() && oBBox.Size() == 6)
    {
        sExtent.MinX = oBBox[0].ToDouble();
        sExtent.MinY = oBBox[1].ToDouble();
        sExtent.MinZ = oBBox[2].ToDouble();
        sExtent.MaxX = oBBox[3].ToDouble();
        sExtent.MaxY = oBBox[4].ToDouble();
        sExtent.MaxZ = oBBox[5].ToDouble();
        if (sExtent.MinX <= sExtent.MaxX)
        {
            oMapExtent[oColumn.GetName()] = sExtent;
        }
    }

    std::string osBBOXColumn;
    std::string osXMin, osYMin, osXMax, osYMax;
    if (ParseGeometryColumnCovering(oColumn, osBBOXColumn, osXMin, osYMin,
                                    osXMax, osYMax))
    {
        OGRADBCLayer::GeomColBBOX geomColBBOX;
        const std::string osPrefix =
            std::string("\"")
                .append(OGRDuplicateCharacter(osBBOXColumn, '"'))
                .append("\".\"");
        geomColBBOX.osXMin = std::string(osPrefix)
                                 .append(OGRDuplicateCharacter(osXMin, '"'))
                                 .append("\"");
        geomColBBOX.osYMin = std::string(osPrefix)
                                 .append(OGRDuplicateCharacter(osYMin, '"'))
                                 .append("\"");
        geomColBBOX.osXMax = std::string(osPrefix)
                                 .append(OGRDuplicateCharacter(osXMax, '"'))
                                 .append("\"");
        geomColBBOX.osYMax = std::string(osPrefix)
                                 .append(OGRDuplicateCharacter(osYMax, '"'))
                                 .append("\"");
        oMapGeomColumnToCoveringBBOXColumn[oColumn.GetName()] =
            std::move(geomColBBOX);
        oSetCoveringBBoxColumn.insert(osBBOXColumn);
    }
}

/************************************************************************/
/*                           BuildLayerDefn()                           */
/************************************************************************/

void OGRADBCLayer::BuildLayerDefn(bool bInternalUse)
{
    // Identify geometry columns for Parquet files, and query them with
    // ST_AsWKB() to avoid getting duckdb_spatial own geometry encoding
    // (https://github.com/duckdb/duckdb_spatial/blob/a60aa3733741a99c49baaf33390c0f7c8a9598a3/spatial/src/spatial/core/geometry/geometry_serialization.cpp#L11)
    std::map<std::string, std::unique_ptr<OGRSpatialReference>> oMapGeomColumns;
    std::map<std::string, OGRwkbGeometryType> oMapType;
    std::map<std::string, OGREnvelope3D> oMapExtent;
    std::map<std::string, GeomColBBOX> oMapGeomColumnToCoveringBBOXColumn;
    if (!bInternalUse && STARTS_WITH_CI(m_osBaseStatement.c_str(), "SELECT ") &&
        (m_poDS->m_bIsDuckDBDriver ||
         (!m_poDS->m_osParquetFilename.empty() &&
          CPLString(m_osBaseStatement)
                  .ifind(std::string(" FROM '").append(OGRDuplicateCharacter(
                      m_poDS->m_osParquetFilename, '\''))) !=
              std::string::npos)))
    {
        // Try to read GeoParquet 'geo' metadata
        std::map<std::string, std::unique_ptr<OGRSpatialReference>>
            oMapGeomColumnsFromGeoParquet;
        std::set<std::string> oSetCoveringBBoxColumn;

        std::string osGeoParquetMD;
        if (!m_poDS->m_osParquetFilename.empty())
        {
            auto poMetadataLayer = m_poDS->CreateInternalLayer(
                std::string("SELECT value FROM parquet_kv_metadata('")
                    .append(OGRDuplicateCharacter(m_poDS->m_osParquetFilename,
                                                  '\''))
                    .append("') WHERE key = 'geo'")
                    .c_str());
            if (poMetadataLayer)
            {
                auto f = std::unique_ptr<OGRFeature>(
                    poMetadataLayer->GetNextFeature());
                if (f)
                {
                    int nBytes = 0;
                    const GByte *pabyData = f->GetFieldAsBinary(0, &nBytes);
                    osGeoParquetMD.assign(
                        reinterpret_cast<const char *>(pabyData), nBytes);
                    // CPLDebug("ADBC", "%s", osGeoParquetMD.c_str());
                }
            }
        }
        CPLJSONDocument oDoc;
        if (!osGeoParquetMD.empty() && oDoc.LoadMemory(osGeoParquetMD))
        {
            const auto oColumns = oDoc.GetRoot().GetObj("columns");
            for (const auto &oColumn : oColumns.GetChildren())
            {
                if (oColumn.GetString("encoding") == "WKB")
                {
                    ParseGeoParquetColumn(oColumn, oMapType, oMapExtent,
                                          oMapGeomColumnToCoveringBBOXColumn,
                                          oMapGeomColumnsFromGeoParquet,
                                          oSetCoveringBBoxColumn);
                }
            }
        }

        auto poDescribeLayer = m_poDS->CreateInternalLayer(
            std::string("DESCRIBE ").append(m_osBaseStatement).c_str());
        std::string osNewStatement;
        bool bNewStatement = false;
        if (poDescribeLayer && (m_poDS->m_bIsDuckDBDriver ||
                                // cppcheck-suppress knownConditionTrueFalse
                                !oMapGeomColumnsFromGeoParquet.empty()))
        {
            for (auto &&f : *poDescribeLayer)
            {
                const char *pszColName = f->GetFieldAsString("column_name");
                if (cpl::contains(oSetCoveringBBoxColumn, pszColName))
                {
                    bNewStatement = true;
                    continue;
                }

                // f->DumpReadable(stdout);
                if (osNewStatement.empty())
                    osNewStatement = "SELECT ";
                else
                    osNewStatement += ", ";

                auto oIter = oMapGeomColumnsFromGeoParquet.find(pszColName);
                if (oIter != oMapGeomColumnsFromGeoParquet.end())
                {
                    oMapGeomColumns[pszColName] = std::move(oIter->second);
                }
                if (EQUAL(f->GetFieldAsString("column_type"), "GEOMETRY") &&
                    m_poDS->m_bSpatialLoaded)
                {
                    bNewStatement = true;
                    osNewStatement += "ST_AsWKB(\"";
                    osNewStatement += OGRDuplicateCharacter(pszColName, '"');
                    osNewStatement += "\") AS ";
                    if (oIter == oMapGeomColumnsFromGeoParquet.end())
                        oMapGeomColumns[pszColName] = nullptr;
                }
                osNewStatement += '"';
                osNewStatement += OGRDuplicateCharacter(pszColName, '"');
                osNewStatement += '"';
            }
            m_osModifiedSelect = osNewStatement;
            osNewStatement += " FROM (";
            osNewStatement += m_osBaseStatement;
            osNewStatement += " )";
        }

        if (bNewStatement)
        {
            // CPLDebug("ADBC", "%s -> %s", m_osBaseStatement.c_str(), osNewStatement.c_str());
            if (ReplaceStatement(osNewStatement.c_str()))
            {
                m_osModifiedBaseStatement = std::move(osNewStatement);
            }
            else
            {
                m_osModifiedSelect.clear();
                oMapGeomColumns.clear();
            }
        }
    }

    m_poAdapterLayer = std::make_unique<OGRArrowArrayToOGRFeatureAdapterLayer>(
        GetDescription());

    for (int i = 0; i < m_schema.n_children; ++i)
    {
        const char *pszColName = m_schema.children[i]->name;
        auto oIter = oMapGeomColumns.find(pszColName);
        if (oIter != oMapGeomColumns.end())
        {
            OGRGeomFieldDefn oGeomFieldDefn(pszColName, oMapType[pszColName]);
            auto poSRS = std::move(oIter->second).release();
            if (poSRS)
            {
                oGeomFieldDefn.SetSpatialRef(poSRS);
                poSRS->Release();
            }
            m_poAdapterLayer->m_poLayerDefn->AddGeomFieldDefn(&oGeomFieldDefn);

            m_extents.push_back(oMapExtent[pszColName]);
            m_geomColBBOX.push_back(
                oMapGeomColumnToCoveringBBOXColumn[pszColName]);
        }
        else
        {
            m_poAdapterLayer->CreateFieldFromArrowSchema(m_schema.children[i]);
        }
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
/*                           ReplaceStatement()                         */
/************************************************************************/

bool OGRADBCLayer::ReplaceStatement(const char *pszNewStatement)
{
    // CPLDebug("ADBC", "%s", pszNewStatement);
    OGRADBCError error;
    auto statement = std::make_unique<AdbcStatement>();
    if (ADBC_CALL(StatementNew, m_poDS->m_connection.get(), statement.get(),
                  error) != ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AdbcStatementNew() failed: %s",
                 error.message());
        ADBC_CALL(StatementRelease, statement.get(), error);
    }
    else if (ADBC_CALL(StatementSetSqlQuery, statement.get(), pszNewStatement,
                       error) != ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "AdbcStatementSetSqlQuery() failed: %s", error.message());
        error.clear();
        ADBC_CALL(StatementRelease, statement.get(), error);
    }
    else
    {
        auto stream = std::make_unique<OGRArrowArrayStream>();
        int64_t rows_affected = -1;
        ArrowSchema newSchema;
        memset(&newSchema, 0, sizeof(newSchema));
        if (ADBC_CALL(StatementExecuteQuery, statement.get(), stream->get(),
                      &rows_affected, error) != ADBC_STATUS_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "AdbcStatementExecuteQuery() failed: %s", error.message());
            error.clear();
            ADBC_CALL(StatementRelease, statement.get(), error);
        }
        else if (stream->get_schema(&newSchema) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "get_schema() failed");
            ADBC_CALL(StatementRelease, statement.get(), error);
        }
        else
        {
            if (m_schema.release)
                m_schema.release(&m_schema);
            memcpy(&m_schema, &newSchema, sizeof(newSchema));

            if (m_statement)
                ADBC_CALL(StatementRelease, m_statement.get(), error);
            m_statement = std::move(statement);

            m_stream = std::move(stream);

            return true;
        }
    }
    return false;
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
    const int nGeomFieldCount =
        m_poAdapterLayer->m_poLayerDefn->GetFieldCount();
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        auto poGeom = poFeature->GetGeomFieldRef(i);
        if (poGeom)
            poGeom->assignSpatialReference(
                m_poAdapterLayer->m_poLayerDefn->GetGeomFieldDefn(i)
                    ->GetSpatialRef());
    }
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
/*                           IGetExtent()                               */
/************************************************************************/

OGRErr OGRADBCLayer::IGetExtent(int iGeomField, OGREnvelope *psExtent,
                                bool bForce)
{
    *psExtent = m_extents[iGeomField];
    if (psExtent->IsInit())
        return OGRERR_NONE;

    return OGRLayer::IGetExtent(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                           IGetExtent3D()                             */
/************************************************************************/

OGRErr OGRADBCLayer::IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent,
                                  bool bForce)
{
    *psExtent = m_extents[iGeomField];
    if (psExtent->IsInit())
        return OGRERR_NONE;

    return OGRLayer::IGetExtent3D(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                      GetCurrentStatement()                           */
/************************************************************************/

std::string OGRADBCLayer::GetCurrentStatement() const
{
    if (!m_osModifiedSelect.empty() &&
        STARTS_WITH_CI(m_osBaseStatement.c_str(), "SELECT * FROM ") &&
        (!m_osAttributeFilter.empty() ||
         (m_poFilterGeom &&
          (!m_geomColBBOX[m_iGeomFieldFilter].osXMin.empty() ||
           m_poDS->m_bSpatialLoaded))))
    {
        std::string osStatement(m_osModifiedSelect);
        osStatement.append(" FROM (")
            .append(m_osBaseStatement)
            .append(") WHERE ");

        bool bAddAnd = false;
        if (m_poFilterGeom)
        {
            const double dfMinX = std::isinf(m_sFilterEnvelope.MinX)
                                      ? -std::numeric_limits<double>::max()
                                      : m_sFilterEnvelope.MinX;
            const double dfMinY = std::isinf(m_sFilterEnvelope.MinY)
                                      ? -std::numeric_limits<double>::max()
                                      : m_sFilterEnvelope.MinY;
            const double dfMaxX = std::isinf(m_sFilterEnvelope.MaxX)
                                      ? std::numeric_limits<double>::max()
                                      : m_sFilterEnvelope.MaxX;
            const double dfMaxY = std::isinf(m_sFilterEnvelope.MaxY)
                                      ? std::numeric_limits<double>::max()
                                      : m_sFilterEnvelope.MaxY;
            if (!m_geomColBBOX[m_iGeomFieldFilter].osXMin.empty())
            {
                bAddAnd = true;
                osStatement.append(m_geomColBBOX[m_iGeomFieldFilter].osXMin)
                    .append(" <= ")
                    .append(CPLSPrintf("%.17g", dfMaxX))
                    .append(" AND ")
                    .append(m_geomColBBOX[m_iGeomFieldFilter].osXMax)
                    .append(" >= ")
                    .append(CPLSPrintf("%.17g", dfMinX))
                    .append(" AND ")
                    .append(m_geomColBBOX[m_iGeomFieldFilter].osYMin)
                    .append(" <= ")
                    .append(CPLSPrintf("%.17g", dfMaxY))
                    .append(" AND ")
                    .append(m_geomColBBOX[m_iGeomFieldFilter].osYMax)
                    .append(" >= ")
                    .append(CPLSPrintf("%.17g", dfMinY));
            }
            if (m_poDS->m_bSpatialLoaded)
            {
                if (bAddAnd)
                    osStatement.append(" AND ");
                bAddAnd = true;
                osStatement.append("ST_Intersects(\"")
                    .append(OGRDuplicateCharacter(
                        m_poAdapterLayer->m_poLayerDefn
                            ->GetGeomFieldDefn(m_iGeomFieldFilter)
                            ->GetNameRef(),
                        '"'))
                    .append(CPLSPrintf(
                        "\", ST_MakeEnvelope(%.17g,%.17g,%.17g,%.17g))", dfMinX,
                        dfMinY, dfMaxX, dfMaxY));
            }
        }
        if (!m_osAttributeFilter.empty())
        {
            if (bAddAnd)
                osStatement.append(" AND ");
            osStatement.append("(");
            osStatement.append(m_osAttributeFilter);
            osStatement.append(")");
        }

        return osStatement;
    }
    else
    {
        return m_osModifiedBaseStatement;
    }
}

/************************************************************************/
/*                         UpdateStatement()                            */
/************************************************************************/

bool OGRADBCLayer::UpdateStatement()
{
    return ReplaceStatement(GetCurrentStatement().c_str());
}

/***********************************************************************/
/*                         SetAttributeFilter()                        */
/***********************************************************************/

OGRErr OGRADBCLayer::SetAttributeFilter(const char *pszFilter)
{
    if (!m_osModifiedSelect.empty() &&
        STARTS_WITH_CI(m_osBaseStatement.c_str(), "SELECT * FROM "))
    {
        m_osAttributeFilter = pszFilter ? pszFilter : "";
        return UpdateStatement() ? OGRERR_NONE : OGRERR_FAILURE;
    }
    else
    {
        return OGRLayer::SetAttributeFilter(pszFilter);
    }
}

/************************************************************************/
/*                        ISetSpatialFilter()                           */
/************************************************************************/

OGRErr OGRADBCLayer::ISetSpatialFilter(int iGeomField,
                                       const OGRGeometry *poGeomIn)

{
    if (iGeomField < GetLayerDefn()->GetGeomFieldCount())
    {
        m_iGeomFieldFilter = iGeomField;
        if (InstallFilter(poGeomIn))
            ResetReading();
        UpdateStatement();
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                          TestCapability()                            */
/************************************************************************/

int OGRADBCLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCFastGetArrowStream))
    {
        return !m_poFilterGeom && !m_poAttrQuery && m_osAttributeFilter.empty();
    }
    else if (EQUAL(pszCap, OLCFastFeatureCount))
    {
        return !m_poFilterGeom && !m_poAttrQuery &&
               m_osAttributeFilter.empty() && m_bIsParquetLayer;
    }
    else if (EQUAL(pszCap, OLCFastGetExtent))
    {
        return !m_extents.empty() && m_extents[0].IsInit();
    }
    else if (EQUAL(pszCap, OLCFastSpatialFilter) && m_iGeomFieldFilter >= 0 &&
             m_iGeomFieldFilter < GetLayerDefn()->GetGeomFieldCount())
    {
        if (m_poDS->m_bSpatialLoaded && m_poDS->m_bIsDuckDBDataset)
        {
            const char *pszGeomColName =
                m_poAdapterLayer->m_poLayerDefn
                    ->GetGeomFieldDefn(m_iGeomFieldFilter)
                    ->GetNameRef();
            auto poTmpLayer = m_poDS->CreateInternalLayer(CPLSPrintf(
                "SELECT 1 FROM sqlite_master WHERE tbl_name = '%s' AND type = "
                "'index' AND (sql LIKE '%%USING RTREE (%s)%%' OR sql LIKE "
                "'%%USING RTREE (\"%s\")%%')",
                OGRDuplicateCharacter(GetDescription(), '\'').c_str(),
                pszGeomColName,
                OGRDuplicateCharacter(pszGeomColName, '"').c_str()));
            return poTmpLayer &&
                   std::unique_ptr<OGRFeature>(poTmpLayer->GetNextFeature());
        }
        else if (!m_geomColBBOX[m_iGeomFieldFilter].osXMin.empty())
        {
            // Let's assume that the presence of a geometry bounding box
            // column is sufficient enough to pretend to have fast spatial
            // filter capabilities
            return true;
        }
    }

    return false;
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
    if (m_poFilterGeom || m_poAttrQuery || !m_osAttributeFilter.empty())
    {
        if (!m_osModifiedSelect.empty() &&
            STARTS_WITH_CI(m_osBaseStatement.c_str(), "SELECT * FROM ") &&
            (!m_poFilterGeom ||
             !m_geomColBBOX[m_iGeomFieldFilter].osXMin.empty() ||
             m_poDS->m_bSpatialLoaded))
        {
            const std::string osCurStatement = GetCurrentStatement();
            auto poCountLayer = m_poDS->CreateInternalLayer(
                std::string("SELECT COUNT(*) FROM (")
                    .append(osCurStatement)
                    .append(")")
                    .c_str());
            if (poCountLayer &&
                poCountLayer->GetLayerDefn()->GetFieldCount() == 1)
            {
                auto poFeature =
                    std::unique_ptr<OGRFeature>(poCountLayer->GetNextFeature());
                if (poFeature)
                    return poFeature->GetFieldAsInteger64(0);
            }
        }

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
    auto poCountLayer = m_poDS->CreateInternalLayer(osSQL.c_str());
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
