/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Arrow Database Connectivity driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
  *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_adbc.h"
#include "ogr_p.h"

#include <algorithm>

/************************************************************************/
/*                        OGRADBCBigQueryLayer()                        */
/************************************************************************/

OGRADBCBigQueryLayer::OGRADBCBigQueryLayer(OGRADBCDataset *poDS,
                                           const char *pszName,
                                           const std::string &osStatement,
                                           bool bInternalUse)
    : OGRADBCLayer(poDS, pszName, osStatement, bInternalUse)
{
}

/************************************************************************/
/*                    GetBigQueryDatasetAndTableId()                    */
/************************************************************************/

bool OGRADBCBigQueryLayer::GetBigQueryDatasetAndTableId(
    std::string &osDatasetId, std::string &osTableId) const
{
    auto nPos = CPLString(m_osBaseStatement).ifind(" FROM ");
    if (nPos != std::string::npos)
    {
        nPos += strlen(" FROM ");
        const auto nPos2 = m_osBaseStatement.find(' ', nPos);
        const std::string osTableName =
            (nPos2 != std::string::npos)
                ? m_osBaseStatement.substr(nPos, nPos2 - nPos)
                : m_osBaseStatement.substr(nPos);

        const auto nPosDot = osTableName.find('.');
        if (nPosDot != std::string::npos)
        {
            osDatasetId = osTableName.substr(0, nPosDot);
            osTableId = osTableName.substr(nPosDot + 1);
            if (osDatasetId.size() > 2 && osDatasetId[0] == '`' &&
                osDatasetId.back() == '`')
                osDatasetId = osDatasetId.substr(1, osDatasetId.size() - 2);
            if (osTableId.size() > 2 && osTableId[0] == '`' &&
                osTableId.back() == '`')
                osTableId = osTableId.substr(1, osTableId.size() - 2);
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                           BuildLayerDefn()                           */
/************************************************************************/

void OGRADBCBigQueryLayer::BuildLayerDefn()
{
    if (!BuildLayerDefnInit())
        return;

    std::map<std::string, std::unique_ptr<OGRSpatialReference>> oMapGeomColumns;
    std::map<std::string, bool> oMapIsNullable;

    const bool bIsLikelyTableExtract =
        !m_bInternalUse &&
        STARTS_WITH_CI(m_osBaseStatement.c_str(), "SELECT ") &&
        !STARTS_WITH_CI(m_osBaseStatement.c_str(), "SELECT COUNT(");
    if (bIsLikelyTableExtract)
    {
        std::string osDatasetId;
        std::string osTableId;
        if (GetBigQueryDatasetAndTableId(osDatasetId, osTableId))
        {
            auto poColumnList = m_poDS->CreateInternalLayer(CPLSPrintf(
                "SELECT c.column_name, c.data_type, c.is_nullable, "
                "keys.ordinal_position AS key_ordinal_position, "
                "keys.position_in_unique_constraint FROM "
                "`%s`.INFORMATION_SCHEMA.COLUMNS c "
                "LEFT JOIN `%s`.INFORMATION_SCHEMA.KEY_COLUMN_USAGE keys ON "
                "c.table_schema = keys.table_schema AND "
                "c.table_name = keys.table_name AND "
                "c.column_name = keys.column_name "
                "WHERE c.table_name='%s' AND c.is_hidden = 'NO' "
                "ORDER BY c.ordinal_position",
                OGRDuplicateCharacter(osDatasetId.c_str(), '`').c_str(),
                OGRDuplicateCharacter(osDatasetId.c_str(), '`').c_str(),
                OGRDuplicateCharacter(osTableId.c_str(), '\'').c_str()));
            if (poColumnList->GetLayerDefn()->GetFieldCount() == 5)
            {
                for (auto &&f : *poColumnList)
                {
                    constexpr int IDX_COL_NAME = 0;
                    constexpr int IDX_DATA_TYPE = 1;
                    constexpr int IDX_IS_NULLABLE = 2;
                    constexpr int IDX_KEY_ORDINAL_POSITION = 3;
                    constexpr int IDX_POSITION_IN_UNIQUE_CONSTRAINT = 4;
                    const char *pszColName = f->GetFieldAsString(IDX_COL_NAME);
                    const char *pszColType = f->GetFieldAsString(IDX_DATA_TYPE);
                    if (EQUAL(pszColType, "GEOGRAPHY"))
                    {
                        auto poSRS = std::make_unique<OGRSpatialReference>();
                        poSRS->SetAxisMappingStrategy(
                            OAMS_TRADITIONAL_GIS_ORDER);
                        poSRS->importFromEPSG(4326);
                        oMapGeomColumns[pszColName] = std::move(poSRS);
                    }
                    oMapIsNullable[pszColName] =
                        EQUAL(f->GetFieldAsString(IDX_IS_NULLABLE), "YES");
                    if (f->IsFieldNull(IDX_POSITION_IN_UNIQUE_CONSTRAINT) &&
                        !f->IsFieldNull(IDX_KEY_ORDINAL_POSITION))
                    {
                        if (EQUAL(pszColType, "INT64") &&
                            f->GetFieldAsInteger64(IDX_KEY_ORDINAL_POSITION) ==
                                1 &&
                            m_osFIDColName.empty())
                        {
                            m_osFIDColName = pszColName;
                        }
                        else
                        {
                            m_osFIDColName.clear();
                        }
                    }
                }

                if (!oMapGeomColumns.empty())
                {
                    std::string osNewStatement = "SELECT ";
                    for (int i = 0; i < m_schema.n_children; ++i)
                    {
                        if (i > 0)
                            osNewStatement += ", ";
                        const char *pszColName = m_schema.children[i]->name;
                        auto oIter = oMapGeomColumns.find(pszColName);
                        if (oIter != oMapGeomColumns.end())
                        {
                            osNewStatement += "ST_AsBinary(`";
                            osNewStatement +=
                                OGRDuplicateCharacter(pszColName, '`');
                            osNewStatement += "`) AS ";
                        }
                        osNewStatement += '`';
                        osNewStatement +=
                            OGRDuplicateCharacter(pszColName, '`');
                        osNewStatement += '`';
                    }
                    m_osModifiedSelect = osNewStatement;
                    osNewStatement += " FROM (";
                    osNewStatement += m_osBaseStatement;
                    osNewStatement += " )";

#ifdef DEBUG_VEBOSE
                    CPLDebug("ADBC", "%s -> %s", m_osBaseStatement.c_str(),
                             osNewStatement.c_str());
#endif

                    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
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
        }
    }

    auto poLayerDefn = m_poAdapterLayer->GetLayerDefn();

    for (int i = 0; i < m_schema.n_children; ++i)
    {
        const char *pszColName = m_schema.children[i]->name;
        auto oIter = oMapGeomColumns.find(pszColName);
        if (oIter != oMapGeomColumns.end())
        {
            OGRGeomFieldDefn oGeomFieldDefn(pszColName, wkbUnknown);
            auto poSRS = std::move(oIter->second).release();
            if (poSRS)
            {
                oGeomFieldDefn.SetSpatialRef(poSRS);
                poSRS->Release();
            }
            poLayerDefn->AddGeomFieldDefn(&oGeomFieldDefn);
        }
        else
        {
            m_poAdapterLayer->CreateFieldFromArrowSchema(m_schema.children[i]);
        }
    }

    if (bIsLikelyTableExtract)
    {
        for (int i = 0; i < poLayerDefn->GetFieldCount(); ++i)
        {
            auto poFldDefn = poLayerDefn->GetFieldDefn(i);
            auto oIter = oMapIsNullable.find(poFldDefn->GetNameRef());
            if (oIter != oMapIsNullable.end())
                poFldDefn->SetNullable(oIter->second);
        }
        for (int i = 0; i < poLayerDefn->GetGeomFieldCount(); ++i)
        {
            auto poGFldDefn = poLayerDefn->GetGeomFieldDefn(i);
            std::string osSQL = "SELECT DISTINCT ST_GeometryType(`";
            osSQL += OGRDuplicateCharacter(poGFldDefn->GetNameRef(), '`');
            osSQL += "`) FROM (";
            osSQL += m_osBaseStatement;
            osSQL += ')';
            auto poGeomTypeList = m_poDS->CreateInternalLayer(osSQL.c_str());
            if (poGeomTypeList->GetLayerDefn()->GetFieldCount() == 1)
            {
                std::string osType;
                for (auto &&f : *poGeomTypeList)
                {
                    if (osType.empty())
                    {
                        osType = f->GetFieldAsString(0);
                    }
                    else
                    {
                        osType.clear();
                        break;
                    }
                }
                if (STARTS_WITH_CI(osType.c_str(), "ST_"))
                {
                    poGFldDefn->SetType(
                        OGRFromOGCGeomType(osType.c_str() + strlen("ST_")));
                }
            }
            auto oIter = oMapIsNullable.find(poGFldDefn->GetNameRef());
            if (oIter != oMapIsNullable.end())
                poGFldDefn->SetNullable(oIter->second);
        }
    }
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRADBCBigQueryLayer::SetAttributeFilter(const char *pszFilter)
{
    if (!m_osModifiedSelect.empty())
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
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRADBCBigQueryLayer::GetFeatureCount(int /*bForce*/)
{
    if (!m_poAdapterLayer)
        BuildLayerDefn();
    if (m_bLayerDefinitionError)
        return 0;

    auto nCount = GetFeatureCountSelectCountStar();
    if (nCount >= 0)
        return nCount;

    return GetFeatureCountArrow();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRADBCBigQueryLayer::TestCapability(const char *pszCap) const
{
    if (!m_poAdapterLayer)
        const_cast<OGRADBCBigQueryLayer *>(this)->BuildLayerDefn();

    if (EQUAL(pszCap, OLCSequentialWrite) || EQUAL(pszCap, OLCCreateField))
        return m_poDS->GetAccess() == GA_Update;

    if (EQUAL(pszCap, OLCRandomWrite) || EQUAL(pszCap, OLCDeleteFeature))
        return m_poDS->GetAccess() == GA_Update && !m_osFIDColName.empty();

    return OGRADBCLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                             IGetExtent()                             */
/************************************************************************/

OGRErr OGRADBCBigQueryLayer::IGetExtent(int iGeomField, OGREnvelope *psExtent,
                                        bool bForce)
{
    if (!m_poAdapterLayer)
        BuildLayerDefn();

    const char *pszGeomColName =
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetNameRef();
    std::string osSQL = "SELECT ST_Extent(`";
    osSQL += OGRDuplicateCharacter(pszGeomColName, '`');
    osSQL += "`) FROM (";
    osSQL += m_osBaseStatement;
    osSQL += ")";
    auto poExtentLayer = m_poDS->CreateInternalLayer(osSQL.c_str());
    if (poExtentLayer->GetLayerDefn()->GetFieldCount() == 4)
    {
        auto f = std::unique_ptr<OGRFeature>(poExtentLayer->GetNextFeature());
        if (f && f->IsFieldSetAndNotNull(0))
        {
            psExtent->MinX = f->GetFieldAsDouble(0);
            psExtent->MinY = f->GetFieldAsDouble(1);
            psExtent->MaxX = f->GetFieldAsDouble(2);
            psExtent->MaxY = f->GetFieldAsDouble(3);
            return OGRERR_NONE;
        }
        else
            return OGRERR_FAILURE;
    }

    return OGRLayer::IGetExtent(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                        GetCurrentStatement()                         */
/************************************************************************/

std::string OGRADBCBigQueryLayer::GetCurrentStatement() const
{
    if (!m_osAttributeFilter.empty() || m_poFilterGeom)
    {
        std::string osStatement(m_osModifiedSelect);
        osStatement.append(" FROM (")
            .append(m_osBaseStatement)
            .append(") WHERE ");
        if (m_poFilterGeom)
        {
            if (m_sFilterEnvelope.MinX > 180 || m_sFilterEnvelope.MinY > 90 ||
                m_sFilterEnvelope.MaxX < -180 || m_sFilterEnvelope.MaxY < -90)
            {
                osStatement.append(" FALSE");
                return osStatement;
            }
            constexpr double EPSILON = 1e-8;
            const double dfMinX =
                std::max(-180.0, m_sFilterEnvelope.MinX - EPSILON);
            const double dfMinY =
                std::max(-90.0, m_sFilterEnvelope.MinY - EPSILON);
            const double dfMaxX =
                std::min(180.0, m_sFilterEnvelope.MaxX + EPSILON);
            const double dfMaxY =
                std::min(90.0, m_sFilterEnvelope.MaxY + EPSILON);
            const char *pszGeomColName =
                m_poAdapterLayer->GetLayerDefn()
                    ->GetGeomFieldDefn(m_iGeomFieldFilter)
                    ->GetNameRef();
            osStatement +=
                CPLSPrintf("ST_IntersectsBox(`%s`,%.17g,%.17g,%.17g,%.17g)",
                           OGRDuplicateCharacter(pszGeomColName, '`').c_str(),
                           dfMinX, dfMinY, dfMaxX, dfMaxY);
        }
        if (!m_osAttributeFilter.empty())
        {
            if (m_poFilterGeom)
                osStatement.append(" AND ");
            osStatement.append("(");
            osStatement.append(m_osAttributeFilter);
            osStatement.append(")");
        }

#ifdef DEBUG_VEBOSE
        CPLDebug("ADBC", "%s", osStatement.c_str());
#endif

        return osStatement;
    }
    else
    {
        return m_osModifiedBaseStatement;
    }
}

/************************************************************************/
/*                             GetSQLType()                             */
/************************************************************************/

static std::string GetSQLType(const OGRFieldDefn *poField)
{
    switch (poField->GetType())
    {
        case OFTInteger:
            return poField->GetSubType() == OFSTBoolean ? "BOOLEAN" : "INTEGER";
        case OFTInteger64:
            return "INT64";
        case OFTReal:
            return "FLOAT64";
        case OFTDate:
            return "DATE";
        case OFTTime:
            return "TIME";
        case OFTDateTime:
            return "TIMESTAMP";
        case OFTString:
            return poField->GetSubType() == OFSTJSON ? "JSON" : "STRING";
        case OFTBinary:
            return "BYTES";
        case OFTStringList:
            return "ARRAY<STRING>";
        case OFTRealList:
            return "ARRAY<FLOAT64>";
        case OFTIntegerList:
            return "ARRAY<INTEGER>";
        case OFTInteger64List:
            return "ARRAY<INT64>";
        case OFTWideString:
        case OFTWideStringList:
            CPLError(CE_Failure, CPLE_NotSupported, "Unsupported type");
            break;
    }
    return std::string();
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRADBCBigQueryLayer::CreateField(const OGRFieldDefn *poField,
                                         int /*bApproxOK*/)
{
    if (m_poDS->GetAccess() != GA_Update)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "CreateField() only supported on datasets opened in update mode");
        return OGRERR_FAILURE;
    }
    if (!m_poAdapterLayer)
        BuildLayerDefn();
    if (m_bLayerDefinitionError)
        return OGRERR_FAILURE;

    if (GetLayerDefn()->GetFieldIndex(poField->GetNameRef()) >= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Field '%s' already exists.",
                 poField->GetNameRef());
        return OGRERR_FAILURE;
    }

    const auto osSQLType = GetSQLType(poField);
    if (osSQLType.empty())
        return OGRERR_FAILURE;

    if (!m_bDeferredCreation)
    {
        std::string osDatasetId, osTableId;
        if (!GetBigQueryDatasetAndTableId(osDatasetId, osTableId))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "CreateField(): cannot get dataset and table ID");
            return OGRERR_FAILURE;
        }

        std::string osSQL = "ALTER TABLE `";
        osSQL += OGRDuplicateCharacter(osDatasetId.c_str(), '`');
        osSQL += "`.`";
        osSQL += OGRDuplicateCharacter(osTableId.c_str(), '`');
        osSQL += "` ADD COLUMN `";
        osSQL += OGRDuplicateCharacter(poField->GetNameRef(), '`');
        osSQL += "` ";
        osSQL += osSQLType;
        if (m_poDS->CreateInternalLayer(osSQL.c_str())->GotError())
            return OGRERR_FAILURE;
    }

    return m_poAdapterLayer->CreateField(poField, false);
}

/************************************************************************/
/*                           GetFieldValue()                            */
/************************************************************************/

static std::string GetFieldValue(const OGRFieldDefn *poFldDefn,
                                 OGRFeature *poFeature, int iField)
{
    std::string osVal;

    if (poFeature->IsFieldNull(iField))
        osVal = "NULL";

    else if (poFldDefn->GetType() == OFTInteger ||
             poFldDefn->GetType() == OFTInteger64)
    {
        const auto nVal = poFeature->GetFieldAsInteger64(iField);
        if (poFldDefn->GetSubType() == OFSTBoolean)
            osVal = nVal ? "TRUE" : "FALSE";
        else
            osVal = std::to_string(nVal);
    }
    else if (poFldDefn->GetType() == OFTReal)
    {
        osVal = CPLSPrintf("%.17g", poFeature->GetFieldAsDouble(iField));
    }
    else if (poFldDefn->GetType() == OFTDate)
    {
        char szTmpFieldValue[OGR_SIZEOF_ISO8601_DATETIME_BUFFER];
        constexpr bool bAlwaysMillisecond = false;
        OGRGetISO8601DateTime(poFeature->GetRawFieldRef(iField),
                              bAlwaysMillisecond, szTmpFieldValue);
        szTmpFieldValue[strlen("YYYY-MM-DD")] = 0;
        osVal += "DATE \'";
        osVal += szTmpFieldValue;
        osVal += '\'';
    }
    else if (poFldDefn->GetType() == OFTDateTime)
    {
        osVal += '\'';
        osVal += poFeature->GetFieldAsISO8601DateTime(iField, nullptr);
        osVal += '\'';
    }
    else if (poFldDefn->GetType() == OFTBinary)
    {
        osVal += "b'";
        int nCount = 0;
        GByte *pabyVal = poFeature->GetFieldAsBinary(iField, &nCount);
        osVal.reserve(nCount * 4 + 4);
        for (int i = 0; i < nCount; ++i)
        {
            osVal += CPLSPrintf("\\x%02X", pabyVal[i]);
        }
        osVal += '\'';
    }
    else if (poFldDefn->GetType() == OFTStringList)
    {
        CSLConstList papszStr = poFeature->GetFieldAsStringList(iField);
        osVal += '[';
        for (int i = 0; papszStr && papszStr[i]; ++i)
        {
            if (i > 0)
                osVal += ',';
            osVal += '\'';
            osVal += OGRDuplicateCharacter(papszStr[i], '\'');
            osVal += '\'';
        }
        osVal += ']';
    }
    else if (poFldDefn->GetType() == OFTIntegerList)
    {
        int nCount = 0;
        const int *panVals = poFeature->GetFieldAsIntegerList(iField, &nCount);
        osVal += '[';
        for (int i = 0; i < nCount; ++i)
        {
            if (i > 0)
                osVal += ',';
            osVal += std::to_string(panVals[i]);
        }
        osVal += ']';
    }
    else if (poFldDefn->GetType() == OFTInteger64List)
    {
        int nCount = 0;
        const auto *panVals =
            poFeature->GetFieldAsInteger64List(iField, &nCount);
        osVal += '[';
        for (int i = 0; i < nCount; ++i)
        {
            if (i > 0)
                osVal += ',';
            osVal += std::to_string(panVals[i]);
        }
        osVal += ']';
    }
    else if (poFldDefn->GetType() == OFTRealList)
    {
        int nCount = 0;
        const double *padfVals =
            poFeature->GetFieldAsDoubleList(iField, &nCount);
        osVal += '[';
        for (int i = 0; i < nCount; ++i)
        {
            if (i > 0)
                osVal += ',';
            osVal += CPLSPrintf("%.17g", padfVals[i]);
        }
        osVal += ']';
    }
    else
    {
        // Cf https://cloud.google.com/bigquery/docs/json-data?hl=en#create_a_json_value
        if (poFldDefn->GetSubType() == OFSTJSON)
            osVal += "JSON ";
        osVal += '\'';
        osVal +=
            OGRDuplicateCharacter(poFeature->GetFieldAsString(iField), '\'');
        osVal += '\'';
    }
    return osVal;
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGRADBCBigQueryLayer::ICreateFeature(OGRFeature *poFeature)
{
    if (m_poDS->GetAccess() != GA_Update)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "CreateFeature() only supported on datasets opened in update mode");
        return OGRERR_FAILURE;
    }
    if (!m_poAdapterLayer)
        BuildLayerDefn();
    if (m_bDeferredCreation)
        RunDeferredCreation();
    if (m_bLayerDefinitionError)
        return OGRERR_FAILURE;

    std::string osDatasetId;
    std::string osTableId;
    if (!STARTS_WITH_CI(m_osBaseStatement.c_str(), "SELECT * FROM ") ||
        CPLString(m_osBaseStatement).ifind(" WHERE ") != std::string::npos ||
        !GetBigQueryDatasetAndTableId(osDatasetId, osTableId))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature(): cannot get dataset and table ID");
        return OGRERR_FAILURE;
    }

    std::string osFieldNames;
    std::string osFieldValues;

    if (!m_osFIDColName.empty())
    {
        if (poFeature->GetFID() < 0)
        {
            if (m_nMaxFeatureID < 0)
            {
                std::string osSQL = "SELECT MAX(`";
                osSQL += OGRDuplicateCharacter(m_osFIDColName.c_str(), '`');
                osSQL += "`) FROM (";
                osSQL += m_osBaseStatement;
                osSQL += ')';

                auto poMaxFIDLayer = m_poDS->CreateInternalLayer(osSQL.c_str());
                if (poMaxFIDLayer->GetLayerDefn()->GetFieldCount() != 1)
                    return OGRERR_FAILURE;
                auto f = std::unique_ptr<OGRFeature>(
                    poMaxFIDLayer->GetNextFeature());
                if (f)
                    m_nMaxFeatureID = f->GetFieldAsInteger64(0);
                else
                    m_nMaxFeatureID = 0;
            }
            poFeature->SetFID(++m_nMaxFeatureID);
        }
        osFieldNames = m_osFIDColName;
        osFieldValues = std::to_string(poFeature->GetFID());
    }

    auto poFeatureDefn = GetLayerDefn();
    for (int i = 0; i < poFeatureDefn->GetGeomFieldCount(); ++i)
    {
        const char *pszName = poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef();
        if (!osFieldNames.empty())
            osFieldNames += ", ";
        osFieldNames += '`';
        osFieldNames += OGRDuplicateCharacter(pszName, '`');
        osFieldNames += '`';

        if (!osFieldValues.empty())
            osFieldValues += ", ";

        const auto poGeom = poFeature->GetGeomFieldRef(i);
        if (poGeom)
        {
            osFieldValues += "ST_GeogFromText('";
            char *pszWKT = nullptr;
            poGeom->exportToWkt(&pszWKT, wkbVariantIso);
            osFieldValues += pszWKT;
            CPLFree(pszWKT);
            osFieldValues += "')";
        }
        else
        {
            osFieldValues += "NULL";
        }
    }
    for (int i = 0; i < poFeatureDefn->GetFieldCount(); ++i)
    {
        const auto poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        const char *pszName = poFieldDefn->GetNameRef();
        if (!EQUAL(pszName, m_osFIDColName.c_str()) && poFeature->IsFieldSet(i))
        {
            if (!osFieldNames.empty())
                osFieldNames += ", ";
            osFieldNames += '`';
            osFieldNames += OGRDuplicateCharacter(pszName, '`');
            osFieldNames += '`';

            if (!osFieldValues.empty())
                osFieldValues += ", ";

            osFieldValues += GetFieldValue(poFieldDefn, poFeature, i);
        }
    }

    std::string osSQL = "INSERT INTO `";
    osSQL += OGRDuplicateCharacter(osDatasetId, '`');
    osSQL += "`.`";
    osSQL += OGRDuplicateCharacter(osTableId, '`');
    osSQL += "` ";
    if (osFieldNames.empty())
    {
        osSQL += "DEFAULT VALUES";
    }
    else
    {
        osSQL += '(';
        osSQL += osFieldNames;
        osSQL += ") VALUES (";
        osSQL += osFieldValues;
        osSQL += ')';
    }
    return m_poDS->CreateInternalLayer(osSQL.c_str())->GotError()
               ? OGRERR_FAILURE
               : OGRERR_NONE;
}

/************************************************************************/
/*                            ISetFeature()                             */
/************************************************************************/

OGRErr OGRADBCBigQueryLayer::ISetFeature(OGRFeature *poFeature)
{
    if (m_poDS->GetAccess() != GA_Update)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "SetFeature() only supported on datasets opened in update mode");
        return OGRERR_FAILURE;
    }
    if (m_osFIDColName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() only supported on tables with a INT64 single "
                 "column primary key");
        return OGRERR_FAILURE;
    }
    if (poFeature->GetFID() < 0)
    {
        return OGRERR_NON_EXISTING_FEATURE;
    }

    if (!m_poAdapterLayer)
        BuildLayerDefn();
    if (m_bDeferredCreation)
        RunDeferredCreation();
    if (m_bLayerDefinitionError)
        return OGRERR_FAILURE;

    std::string osDatasetId;
    std::string osTableId;
    if (!STARTS_WITH_CI(m_osBaseStatement.c_str(), "SELECT * FROM ") ||
        CPLString(m_osBaseStatement).ifind(" WHERE ") != std::string::npos ||
        !GetBigQueryDatasetAndTableId(osDatasetId, osTableId))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature(): cannot get dataset and table ID");
        return OGRERR_FAILURE;
    }

    std::string osSQL = "UPDATE `";
    osSQL += OGRDuplicateCharacter(osDatasetId, '`');
    osSQL += "`.`";
    osSQL += OGRDuplicateCharacter(osTableId, '`');
    osSQL += "` SET ";

    bool bAddComma = false;
    const auto poFeatureDefn = GetLayerDefn();

    for (int i = 0; i < poFeatureDefn->GetGeomFieldCount(); ++i)
    {
        const char *pszName = poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef();
        if (bAddComma)
            osSQL += ", ";
        bAddComma = true;
        osSQL += '`';
        osSQL += OGRDuplicateCharacter(pszName, '`');
        osSQL += "` = ";
        const auto poGeom = poFeature->GetGeomFieldRef(i);
        if (poGeom)
        {
            osSQL += "ST_GeogFromText('";
            char *pszWKT = nullptr;
            poGeom->exportToWkt(&pszWKT, wkbVariantIso);
            osSQL += pszWKT;
            CPLFree(pszWKT);
            osSQL += "')";
        }
        else
        {
            osSQL += "NULL";
        }
    }

    for (int i = 0; i < poFeatureDefn->GetFieldCount(); ++i)
    {
        const auto poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        const char *pszName = poFieldDefn->GetNameRef();
        if (!EQUAL(pszName, m_osFIDColName.c_str()) && poFeature->IsFieldSet(i))
        {
            if (bAddComma)
                osSQL += ", ";
            bAddComma = true;
            osSQL += '`';
            osSQL += OGRDuplicateCharacter(pszName, '`');
            osSQL += "` = ";
            osSQL += GetFieldValue(poFieldDefn, poFeature, i);
        }
    }

    osSQL += " WHERE `";
    osSQL += OGRDuplicateCharacter(m_osFIDColName, '`');
    osSQL += "` = ";
    osSQL += std::to_string(poFeature->GetFID());

    return bAddComma && m_poDS->CreateInternalLayer(osSQL.c_str())->GotError()
               ? OGRERR_FAILURE
               : OGRERR_NONE;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRADBCBigQueryLayer::DeleteFeature(GIntBig nFID)
{
    if (m_poDS->GetAccess() != GA_Update)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "DeleteFeature() only supported on datasets opened in update mode");
        return OGRERR_FAILURE;
    }
    if (m_osFIDColName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DeleteFeature() only supported on tables with a INT64 single "
                 "column primary key");
        return OGRERR_FAILURE;
    }
    if (!m_poAdapterLayer)
        BuildLayerDefn();
    if (m_bDeferredCreation)
        RunDeferredCreation();
    if (m_bLayerDefinitionError)
        return OGRERR_FAILURE;
    if (nFID < 0)
        return OGRERR_NON_EXISTING_FEATURE;

    std::string osDatasetId;
    std::string osTableId;
    if (!GetBigQueryDatasetAndTableId(osDatasetId, osTableId))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DeleteFeature(): cannot get dataset and table ID");
        return OGRERR_FAILURE;
    }

    std::string osSQL = "DELETE FROM `";
    osSQL += OGRDuplicateCharacter(osDatasetId, '`');
    osSQL += "`.`";
    osSQL += OGRDuplicateCharacter(osTableId, '`');
    osSQL += "` WHERE `";
    osSQL += OGRDuplicateCharacter(m_osFIDColName, '`');
    osSQL += "` = ";
    osSQL += std::to_string(nFID);

    return m_poDS->CreateInternalLayer(osSQL.c_str())->GotError()
               ? OGRERR_FAILURE
               : OGRERR_NONE;
}

/************************************************************************/
/*                        SetDeferredCreation()                         */
/************************************************************************/

void OGRADBCBigQueryLayer::SetDeferredCreation(
    const char *pszFIDColName, const OGRGeomFieldDefn *poGeomFieldDefn)
{
    m_bDeferredCreation = true;
    m_osFIDColName = pszFIDColName;
    m_poAdapterLayer = std::make_unique<OGRArrowArrayToOGRFeatureAdapterLayer>(
        GetDescription());
    if (poGeomFieldDefn && poGeomFieldDefn->GetType() != wkbNone)
    {
        OGRGeomFieldDefn oFieldDefn(poGeomFieldDefn);
        if (oFieldDefn.GetNameRef()[0] == '\0')
            oFieldDefn.SetName("geog");
        m_poAdapterLayer->CreateGeomField(&oFieldDefn, false);
    }
}

/************************************************************************/
/*                        RunDeferredCreation()                         */
/************************************************************************/

bool OGRADBCBigQueryLayer::RunDeferredCreation()
{
    if (m_bDeferredCreation)
    {
        m_bDeferredCreation = false;

        auto poFeatureDefn = m_poAdapterLayer->GetLayerDefn();
        std::string osDatasetId;
        std::string osTableId;
        CPL_IGNORE_RET_VAL(
            GetBigQueryDatasetAndTableId(osDatasetId, osTableId));

        std::string osSQL = "CREATE TABLE `";
        osSQL += OGRDuplicateCharacter(osDatasetId.c_str(), '`');
        osSQL += "`.`";
        osSQL += OGRDuplicateCharacter(osTableId.c_str(), '`');
        osSQL += "` (";
        bool bAddComma = false;
        if (!m_osFIDColName.empty())
        {
            osSQL += '`';
            osSQL += OGRDuplicateCharacter(m_osFIDColName.c_str(), '`');
            osSQL += "` INT64 PRIMARY KEY NOT ENFORCED";
            bAddComma = true;
        }
        for (int i = 0; i < poFeatureDefn->GetGeomFieldCount(); ++i)
        {
            const auto poFieldDefn = poFeatureDefn->GetGeomFieldDefn(i);
            if (bAddComma)
                osSQL += ", ";
            bAddComma = true;
            osSQL += '`';
            osSQL += OGRDuplicateCharacter(poFieldDefn->GetNameRef(), '`');
            osSQL += "` GEOGRAPHY";
            if (!poFieldDefn->IsNullable())
                osSQL += " NOT NULL";
        }
        for (int i = 0; i < poFeatureDefn->GetFieldCount(); ++i)
        {
            const auto poFieldDefn = poFeatureDefn->GetFieldDefn(i);
            if (bAddComma)
                osSQL += ", ";
            bAddComma = true;
            osSQL += '`';
            osSQL += OGRDuplicateCharacter(poFieldDefn->GetNameRef(), '`');
            osSQL += "` ";
            osSQL += GetSQLType(poFieldDefn);
            if (!poFieldDefn->IsNullable())
                osSQL += " NOT NULL";
        }
        osSQL += ')';

        m_bLayerDefinitionError =
            m_poDS->CreateInternalLayer(osSQL.c_str())->GotError();
    }
    return !m_bLayerDefinitionError;
}
