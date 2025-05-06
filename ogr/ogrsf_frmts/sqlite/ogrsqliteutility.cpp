/******************************************************************************
 *
 * Project:  SQLite/GeoPackage Translator
 * Purpose:  Utility functions for OGR SQLite/GeoPackage driver.
 * Author:   Paul Ramsey, pramsey@boundlessgeo.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 * Copyright (c) 2020, Alessandro Pasotti <elpaso@itopen.it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogrsqliteutility.h"

#include <cstdlib>
#include <string>
#include <iostream>
#include <sstream>

#include "cpl_error.h"
#include "ogr_p.h"
#include "gdal_priv.h"

SQLResult::SQLResult(char **result, int nRow, int nCol)
    : papszResult(result), nRowCount(nRow), nColCount(nCol)
{
}

SQLResult::~SQLResult()
{
    if (papszResult)
    {
        sqlite3_free_table(papszResult);
    }
}

void SQLResult::LimitRowCount(int nLimit)
{
    nRowCount = nLimit;
}

/* Runs a SQL command and ignores the result (good for INSERT/UPDATE/CREATE) */
OGRErr SQLCommand(sqlite3 *poDb, const char *pszSQL)
{
    CPLAssert(poDb != nullptr);
    CPLAssert(pszSQL != nullptr);

    char *pszErrMsg = nullptr;
#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "exec(%s)", pszSQL);
#endif
    int rc = sqlite3_exec(poDb, pszSQL, nullptr, nullptr, &pszErrMsg);

    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "sqlite3_exec(%s) failed: %s",
                 pszSQL, pszErrMsg ? pszErrMsg : "");
        sqlite3_free(pszErrMsg);
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

std::unique_ptr<SQLResult> SQLQuery(sqlite3 *poDb, const char *pszSQL)
{
    CPLAssert(poDb != nullptr);
    CPLAssert(pszSQL != nullptr);

#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "get_table(%s)", pszSQL);
#endif

    char **papszResult = nullptr;
    char *pszErrMsg = nullptr;
    int nRowCount, nColCount;
    int rc = sqlite3_get_table(poDb, pszSQL, &(papszResult), &(nRowCount),
                               &(nColCount), &(pszErrMsg));

    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sqlite3_get_table(%s) failed: %s", pszSQL, pszErrMsg);
        sqlite3_free(pszErrMsg);
        return nullptr;
    }

    return std::make_unique<SQLResult>(papszResult, nRowCount, nColCount);
}

const char *SQLResult::GetValue(int iColNum, int iRowNum) const
{
    const int nCols = nColCount;
#ifdef DEBUG
    const int nRows = nRowCount;
    CPL_IGNORE_RET_VAL(nRows);

    CPLAssert(iColNum >= 0 && iColNum < nCols);
    CPLAssert(iRowNum >= 0 && iRowNum < nRows);
#endif
    return papszResult[nCols + iRowNum * nCols + iColNum];
}

int SQLResult::GetValueAsInteger(int iColNum, int iRowNum) const
{
    const char *pszValue = GetValue(iColNum, iRowNum);
    if (!pszValue)
        return 0;

    return atoi(pszValue);
}

/* Returns the first row of first column of SQL as integer */
GIntBig SQLGetInteger64(sqlite3 *poDb, const char *pszSQL, OGRErr *err)
{
    CPLAssert(poDb != nullptr);

    sqlite3_stmt *poStmt = nullptr;

    /* Prepare the SQL */
#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "get(%s)", pszSQL);
#endif
    int rc = sqlite3_prepare_v2(poDb, pszSQL, -1, &poStmt, nullptr);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sqlite3_prepare_v2(%s) failed: %s", pszSQL,
                 sqlite3_errmsg(poDb));
        if (err)
            *err = OGRERR_FAILURE;
        return 0;
    }

    /* Execute and fetch first row */
    rc = sqlite3_step(poStmt);
    if (rc != SQLITE_ROW)
    {
        if (err)
            *err = OGRERR_FAILURE;
        sqlite3_finalize(poStmt);
        return 0;
    }

    /* Read the integer from the row */
    GIntBig i = sqlite3_column_int64(poStmt, 0);
    sqlite3_finalize(poStmt);

    if (err)
        *err = OGRERR_NONE;
    return i;
}

int SQLGetInteger(sqlite3 *poDb, const char *pszSQL, OGRErr *err)
{
    return static_cast<int>(SQLGetInteger64(poDb, pszSQL, err));
}

/************************************************************************/
/*                             SQLUnescape()                            */
/************************************************************************/

CPLString SQLUnescape(const char *pszVal)
{
    char chQuoteChar = pszVal[0];
    if (chQuoteChar != '\'' && chQuoteChar != '"')
        return pszVal;

    CPLString osRet;
    pszVal++;
    while (*pszVal != '\0')
    {
        if (*pszVal == chQuoteChar)
        {
            if (pszVal[1] == chQuoteChar)
                pszVal++;
            else
                break;
        }
        osRet += *pszVal;
        pszVal++;
    }
    return osRet;
}

/************************************************************************/
/*                          SQLEscapeLiteral()                          */
/************************************************************************/

CPLString SQLEscapeLiteral(const char *pszLiteral)
{
    CPLString osVal;
    for (int i = 0; pszLiteral[i] != '\0'; i++)
    {
        if (pszLiteral[i] == '\'')
            osVal += '\'';
        osVal += pszLiteral[i];
    }
    return osVal;
}

/************************************************************************/
/*                           SQLEscapeName()                            */
/************************************************************************/

CPLString SQLEscapeName(const char *pszName)
{
    CPLString osRet;
    while (*pszName != '\0')
    {
        if (*pszName == '"')
            osRet += "\"\"";
        else
            osRet += *pszName;
        pszName++;
    }
    return osRet;
}

/************************************************************************/
/*                             SQLTokenize()                            */
/************************************************************************/

char **SQLTokenize(const char *pszStr)
{
    char **papszTokens = nullptr;
    bool bInQuote = false;
    char chQuoteChar = '\0';
    bool bInSpace = true;
    CPLString osCurrentToken;
    while (*pszStr != '\0')
    {
        if (*pszStr == ' ' && !bInQuote)
        {
            if (!bInSpace)
            {
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
            }
            bInSpace = true;
        }
        else if ((*pszStr == '(' || *pszStr == ')' || *pszStr == ',') &&
                 !bInQuote)
        {
            if (!bInSpace)
            {
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
            }
            osCurrentToken.clear();
            osCurrentToken += *pszStr;
            papszTokens = CSLAddString(papszTokens, osCurrentToken);
            osCurrentToken.clear();
            bInSpace = true;
        }
        else if (*pszStr == '"' || *pszStr == '\'')
        {
            if (bInQuote && *pszStr == chQuoteChar && pszStr[1] == chQuoteChar)
            {
                osCurrentToken += *pszStr;
                osCurrentToken += *pszStr;
                pszStr += 2;
                continue;
            }
            else if (bInQuote && *pszStr == chQuoteChar)
            {
                osCurrentToken += *pszStr;
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
                bInSpace = true;
                bInQuote = false;
                chQuoteChar = '\0';
            }
            else if (bInQuote)
            {
                osCurrentToken += *pszStr;
            }
            else
            {
                chQuoteChar = *pszStr;
                osCurrentToken.clear();
                osCurrentToken += chQuoteChar;
                bInQuote = true;
                bInSpace = false;
            }
        }
        else
        {
            osCurrentToken += *pszStr;
            bInSpace = false;
        }
        pszStr++;
    }

    if (!osCurrentToken.empty())
        papszTokens = CSLAddString(papszTokens, osCurrentToken);

    return papszTokens;
}

/************************************************************************/
/*                    SQLGetUniqueFieldUCConstraints()                  */
/************************************************************************/

/* Return set of field names (in upper case) that have a UNIQUE constraint,
 * only on that single column.
 */

std::set<std::string> SQLGetUniqueFieldUCConstraints(
    sqlite3 *poDb, const char *pszTableName,
    const std::vector<SQLSqliteMasterContent> &sqliteMasterContent)
{
    // set names (in upper case) of fields with unique constraint
    std::set<std::string> uniqueFieldsUC;

    // Unique fields detection
    const std::string upperTableName{CPLString(pszTableName).toupper()};
    std::string tableDefinition;

    if (sqliteMasterContent.empty())
    {
        char *pszTableDefinitionSQL = sqlite3_mprintf(
            "SELECT sql, type FROM sqlite_master "
            "WHERE type IN ('table', 'view') AND UPPER(name)='%q'",
            upperTableName.c_str());
        auto oResultTable = SQLQuery(poDb, pszTableDefinitionSQL);
        sqlite3_free(pszTableDefinitionSQL);

        if (!oResultTable || oResultTable->RowCount() == 0)
        {
            if (oResultTable)
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find table %s",
                         pszTableName);

            return uniqueFieldsUC;
        }
        if (std::string(oResultTable->GetValue(1, 0)) == "view")
        {
            return uniqueFieldsUC;
        }
        tableDefinition = oResultTable->GetValue(0, 0);
    }
    else
    {
        for (const auto &row : sqliteMasterContent)
        {
            if (row.osType == "table" &&
                CPLString(row.osTableName).toupper() == upperTableName)
            {
                tableDefinition = row.osSQL;
                break;
            }
            else if (row.osType == "view" &&
                     CPLString(row.osTableName).toupper() == upperTableName)
            {
                return uniqueFieldsUC;
            }
        }
        if (tableDefinition.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find table %s",
                     pszTableName);

            return uniqueFieldsUC;
        }
    }

    // Parses strings like "colum_name1" KEYWORD1 KEYWORD2 'some string',
    // `column_name2`,"column_name3"
    const auto GetNextToken =
        [](const std::string &osStr, size_t &pos, bool keepQuotes)
    {
        if (pos >= osStr.size())
            return std::string();
        pos = osStr.find_first_not_of(" \t\n\r", pos);
        if (pos == std::string::npos)
            return std::string();

        std::string osToken;
        if (osStr[pos] == '"' || osStr[pos] == '\'' || osStr[pos] == '`')
        {
            const char chQuoteChar = osStr[pos];
            if (keepQuotes)
                osToken += chQuoteChar;
            ++pos;
            while (pos < osStr.size())
            {
                if (osStr[pos] == chQuoteChar)
                {
                    if (pos + 1 < osStr.size() && osStr[pos + 1] == chQuoteChar)
                    {
                        osToken += chQuoteChar;
                        pos += 2;
                    }
                    else
                    {
                        if (keepQuotes)
                            osToken += chQuoteChar;
                        pos++;
                        break;
                    }
                }
                else
                {
                    osToken += osStr[pos];
                    pos++;
                }
            }
        }
        else if (osStr[pos] == ',' || osStr[pos] == '(' || osStr[pos] == ')')
        {
            osToken = osStr[pos];
            pos++;
        }
        else
        {
            size_t pos2 = osStr.find_first_of(") \t\n\r,", pos);
            if (pos2 == std::string::npos)
                osToken = osStr.substr(pos);
            else
                osToken = osStr.substr(pos, pos2 - pos);
            pos = pos2;
        }
        return osToken;
    };

    // Parses CREATE TABLE definition for column UNIQUE keywords
    {
        const auto nPosStart = tableDefinition.find('(');
        const auto nPosEnd = tableDefinition.rfind(')');
        if (nPosStart != std::string::npos && nPosEnd != std::string::npos &&
            nPosEnd > nPosStart &&
            CPLString(tableDefinition).toupper().find("UNIQUE") !=
                std::string::npos)
        {
            tableDefinition =
                tableDefinition.substr(nPosStart + 1, nPosEnd - nPosStart - 1);
            size_t pos = 0;
            bool bHasConstraint = false;
            while (true)
            {
                size_t posBackup = pos;
                if (EQUAL(
                        GetNextToken(tableDefinition, posBackup, true).c_str(),
                        "CONSTRAINT"))
                {
                    bHasConstraint = true;
                    break;
                }

                const std::string osColName =
                    GetNextToken(tableDefinition, pos, false);
                if (osColName.empty())
                {
                    break;
                }
                while (true)
                {
                    const std::string osToken =
                        GetNextToken(tableDefinition, pos, true);
                    if (osToken.empty() || osToken == ",")
                        break;
                    if (EQUAL(osToken.c_str(), "UNIQUE"))
                    {
                        uniqueFieldsUC.insert(CPLString(osColName).toupper());
                    }
                }
            }

            // Process https://www.sqlite.org/syntax/table-constraint.html
            if (bHasConstraint)
            {
                while (true)
                {
                    if (!EQUAL(GetNextToken(tableDefinition, pos, true).c_str(),
                               "CONSTRAINT"))
                    {
                        break;
                    }

                    const std::string osConstraintName =
                        GetNextToken(tableDefinition, pos, false);
                    if (osConstraintName.empty())
                    {
                        break;
                    }

                    const std::string osConstraintType =
                        GetNextToken(tableDefinition, pos, true);
                    if (osConstraintType.empty())
                    {
                        break;
                    }

                    if (EQUAL(osConstraintType.c_str(), "UNIQUE"))
                    {
                        std::string osToken =
                            GetNextToken(tableDefinition, pos, true);
                        if (osToken != "(")
                            break;

                        const std::string osColName =
                            GetNextToken(tableDefinition, pos, false);
                        osToken = GetNextToken(tableDefinition, pos, true);
                        if (osToken == ")")
                        {
                            // Only takes into account single column unique constraint
                            uniqueFieldsUC.insert(
                                CPLString(osColName).toupper());
                        }
                        else
                        {
                            // Skip tokens until ')'
                            if (!osToken.empty())
                            {
                                do
                                {
                                    osToken = GetNextToken(tableDefinition, pos,
                                                           true);
                                } while (!osToken.empty() && osToken != ")");
                            }
                            if (osToken.empty())
                                break;
                        }
                        osToken = GetNextToken(tableDefinition, pos, true);
                        if (osToken != ",")
                            break;
                    }
                    else
                    {
                        // Skip ignored constraint types by looking for the
                        // next "," token, that is not inside parenthesis.
                        int nCountParenthesis = 0;
                        std::string osToken;
                        while (true)
                        {
                            osToken = GetNextToken(tableDefinition, pos, true);
                            if (osToken.empty())
                                break;
                            if (nCountParenthesis == 0 && osToken == ",")
                                break;
                            else if (osToken == "(")
                                nCountParenthesis++;
                            else if (osToken == ")")
                            {
                                nCountParenthesis--;
                                if (nCountParenthesis < 0)
                                {
                                    break;
                                }
                            }
                        }
                        if (!(nCountParenthesis == 0 && osToken == ","))
                            break;
                    }
                }
            }
        }
    }

    // Search indexes:

    const auto ProcessIndexDefinition =
        [&uniqueFieldsUC, &GetNextToken](const std::string &indexDefinitionIn)
    {
        const auto nPosStart = indexDefinitionIn.find('(');
        const auto nPosEnd = indexDefinitionIn.rfind(')');
        if (nPosStart != std::string::npos && nPosEnd != std::string::npos &&
            nPosEnd > nPosStart)
        {
            std::string indexDefinitionMod = indexDefinitionIn.substr(
                nPosStart + 1, nPosEnd - nPosStart - 1);
            size_t pos = 0;
            const std::string osColName =
                GetNextToken(indexDefinitionMod, pos, false);
            // Only matches index on single columns
            if (GetNextToken(indexDefinitionMod, pos, false).empty())
            {
                uniqueFieldsUC.insert(CPLString(osColName).toupper());
            }
        }
    };

    if (sqliteMasterContent.empty())
    {
        char *pszTableDefinitionSQL = sqlite3_mprintf(
            "SELECT sql FROM sqlite_master WHERE type='index' AND"
            " UPPER(tbl_name)='%q' AND UPPER(sql) "
            "LIKE 'CREATE UNIQUE INDEX%%'",
            upperTableName.c_str());
        auto oResultTable = SQLQuery(poDb, pszTableDefinitionSQL);
        sqlite3_free(pszTableDefinitionSQL);

        if (!oResultTable)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error searching indexes for table %s", pszTableName);
        }
        else if (oResultTable->RowCount() >= 0)
        {
            for (int rowCnt = 0; rowCnt < oResultTable->RowCount(); ++rowCnt)
            {
                std::string indexDefinition{oResultTable->GetValue(0, rowCnt)};
                ProcessIndexDefinition(indexDefinition);
            }
        }
    }
    else
    {
        for (const auto &row : sqliteMasterContent)
        {
            if (row.osType == "index" &&
                CPLString(row.osTableName).toupper() == upperTableName &&
                STARTS_WITH_CI(row.osSQL.c_str(), "CREATE UNIQUE INDEX"))
            {
                std::string indexDefinition = row.osSQL;
                ProcessIndexDefinition(indexDefinition);
            }
        }
    }

    return uniqueFieldsUC;
}

/************************************************************************/
/*               OGRSQLiteRTreeRequiresTrustedSchemaOn()                */
/************************************************************************/

/** Whether the use of a RTree in triggers or views requires trusted_schema
 * PRAGMA to be set to ON */
bool OGRSQLiteRTreeRequiresTrustedSchemaOn()
{
    static bool b = []()
    {
        sqlite3 *hDB = nullptr;
        int rc =
            sqlite3_open_v2(":memory:", &hDB, SQLITE_OPEN_READWRITE, nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "sqlite3_open_v2(:memory:) failed");
            sqlite3_close(hDB);
            return false;
        }
        rc = sqlite3_exec(hDB,
                          "CREATE VIRTUAL TABLE foo_rtree USING rtree(id, "
                          "minx, miny, maxx, maxy);",
                          nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CREATE VIRTUAL TABLE foo_rtree failed");
            sqlite3_close(hDB);
            return false;
        }
        rc = sqlite3_exec(hDB, "CREATE VIEW v AS SELECT * FROM foo_rtree;",
                          nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CREATE VIEW v AS SELECT * FROM foo_rtree failed");
            sqlite3_close(hDB);
            return false;
        }
        // Try to read the virtual table from a view. As of today (sqlite 3.43.1)
        // this require trusted_schema = ON
        rc = sqlite3_exec(hDB, "SELECT * FROM v", nullptr, nullptr, nullptr);
        bool bRequiresTrustedSchemaOn = false;
        if (rc != SQLITE_OK)
        {
            CPL_IGNORE_RET_VAL(sqlite3_exec(hDB, "PRAGMA trusted_schema = ON",
                                            nullptr, nullptr, nullptr));
            rc =
                sqlite3_exec(hDB, "SELECT * FROM v", nullptr, nullptr, nullptr);
            if (rc == SQLITE_OK)
                bRequiresTrustedSchemaOn = true;
        }
        sqlite3_close(hDB);
        return bRequiresTrustedSchemaOn;
    }();
    return b;
}

/************************************************************************/
/*               OGRSQLiteIsSpatialFunctionReturningGeometry()          */
/************************************************************************/

bool OGRSQLiteIsSpatialFunctionReturningGeometry(const char *pszName)
{
    const char *const apszFunctions[] = {
        "SetSRID(",
        "IsValidDetail(",
        "Boundary(",
        "Envelope(",
        "ST_Expand(",
        "ST_Reverse(",
        "ST_ForceLHR(",
        "ST_ForcePolygonCW(",
        "ST_ForcePolygonCCW(",
        "SanitizeGeometry(",
        "EnsureClosedRings(",
        "RemoveRepeatedPoints(",
        "CastToPoint(",
        "CastToLinestring(",
        "CastToPolygon(",
        "CastToMultiPoint(",
        "CastToMultiLinestring(",
        "CastToMultiPolygon(",
        "CastToGeometryCollection(",
        "CastToMulti(",
        "ST_Multi(",
        "CastToSingle(",
        "CastToXY(",
        "CastToXYZ(",
        "CastToXYM(",
        "CastToXYZM(",
        "StartPoint(",
        "ST_EndPoint(",
        "PointOnSurface(",
        "Simplify(",
        "ST_Generalize(",
        "SimplifyPreserveTopology(",
        "PointN(",
        "AddPoint(",
        "SetPoint(",
        "SetStartPoint(",
        "SetEndPoint(",
        "RemovePoint(",
        "Centroid(",
        "ExteriorRing(",
        "InteriorRingN(",
        "GeometryN(",
        "ST_AddMeasure(",
        "ST_Locate_Along_Measure(",
        "ST_LocateAlong(",
        "ST_Locate_Between_Measures(",
        "ST_LocateBetween(",
        "ST_TrajectoryInterpolarePoint(",
        "Intersection(",
        "Difference(",
        "GUnion(",
        "ST_Union(",  // UNION is not a valid function name
        "SymDifference(",
        "Buffer(",
        "ConvexHull(",
        "OffsetCurve(",
        "SingleSidedBuffer(",
        "SharedPaths(",
        "Line_Interpolate_Point(",
        "Line_Interpolate_Equidistant_Points(",
        "Line_Substring(",
        "ClosestPoint(",
        "ShortestLine(",
        "Snap(",
        "Collect(",
        "LineMerge(",
        "BuildArea(",
        "Polygonize(",
        "MakePolygon(",
        "UnaryUnion(",
        "UnaryUnion(",
        "DrapeLine(",
        "DrapeLineExceptions(",
        "DissolveSegments(",
        "DissolvePoints(",
        "LinesFromRings(",
        "LinesCutAtNodes(",
        "RingsCutAtNodes(",
        "CollectionExtract(",
        "ExtractMultiPoint(",
        "ExtractMultiLinestring(",
        "ExtractMultiPolygon(",
        "DelaunayTriangulation(",
        "VoronojDiagram(",
        "ConcaveHull(",
        "MakeValid(",
        "MakeValidDiscarded(",
        "Segmentize(",
        "Split(",
        "SplitLeft(",
        "SplitRight(",
        "SnapAndSplit(",
        "Project(",
        "SnapToGrid(",
        "ST_Node(",
        "SelfIntersections(",
        "ST_Subdivide(",
        "Transform(",
        "TransformXY(",
        "TransformXYZ(",
        "ShiftCoords(",
        "ShiftCoordinates(",
        "ST_Translate(",
        "ST_Shift_Longitude(",
        "NormalizeLonLat(",
        "ScaleCoords(",
        "ScaleCoordinates(",
        "RotateCoords(",
        "RotateCoordinates(",
        "ReflectCoords(",
        "ReflectCoordinates(",
        "SwapCoords(",
        "SwapCoordinates(",
        "ATM_Transform(",
        "gpkgMakePoint(",
        "gpkgMakePointZ(",
        "gpkgMakePointZM(",
        "gpkgMakePointM(",
        "AsGPB(",
        "GeomFromGPB(",
        "CastAutomagic(",
    };
    for (const char *pszFunction : apszFunctions)
    {
        if (STARTS_WITH_CI(pszName, pszFunction) ||
            (!STARTS_WITH_CI(pszFunction, "ST_") &&
             STARTS_WITH_CI(pszName, "ST_") &&
             STARTS_WITH_CI(pszName + strlen("ST_"), pszFunction)))
        {
            return true;
        }
    }
    return false;
}

double SQLResult::GetValueAsDouble(int iColNum, int iRowNum) const
{
    const char *pszValue = GetValue(iColNum, iRowNum);
    if (!pszValue)
        return 0;

    return CPLStrtod(pszValue, nullptr);
}

/************************************************************************/
/*                  OGRSQLite_gdal_get_pixel_value_common()             */
/************************************************************************/

void OGRSQLite_gdal_get_pixel_value_common(const char *pszFunctionName,
                                           sqlite3_context *pContext, int argc,
                                           sqlite3_value **argv,
                                           GDALDataset *poDS)
{
    if (sqlite3_value_type(argv[1]) != SQLITE_INTEGER ||
        sqlite3_value_type(argv[2]) != SQLITE_TEXT ||
        (sqlite3_value_type(argv[3]) != SQLITE_INTEGER &&
         sqlite3_value_type(argv[3]) != SQLITE_FLOAT) ||
        (sqlite3_value_type(argv[4]) != SQLITE_INTEGER &&
         sqlite3_value_type(argv[4]) != SQLITE_FLOAT) ||
        (argc == 6 && sqlite3_value_type(argv[5]) != SQLITE_TEXT))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid arguments to %s()",
                 pszFunctionName);
        sqlite3_result_null(pContext);
        return;
    }

    const int nBand = sqlite3_value_int(argv[1]);
    auto poBand = poDS->GetRasterBand(nBand);
    if (!poBand)
    {
        sqlite3_result_null(pContext);
        return;
    }

    const char *pszCoordType =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[2]));
    double x, y;
    if (EQUAL(pszCoordType, "georef"))
    {
        const double X = sqlite3_value_double(argv[3]);
        const double Y = sqlite3_value_double(argv[4]);
        double adfGeoTransform[6];
        if (poDS->GetGeoTransform(adfGeoTransform) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        double adfInvGT[6];
        if (!GDALInvGeoTransform(adfGeoTransform, adfInvGT))
        {
            sqlite3_result_null(pContext);
            return;
        }
        x = adfInvGT[0] + X * adfInvGT[1] + Y * adfInvGT[2];
        y = adfInvGT[3] + X * adfInvGT[4] + Y * adfInvGT[5];
    }
    else if (EQUAL(pszCoordType, "pixel"))
    {
        x = sqlite3_value_int(argv[3]);
        y = sqlite3_value_int(argv[4]);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for 3rd argument of gdal_get_pixel_value(): "
                 "only 'georef' or 'pixel' are supported");
        sqlite3_result_null(pContext);
        return;
    }
    if (x < 0 || x >= poDS->GetRasterXSize() || y < 0 ||
        y >= poDS->GetRasterYSize())
    {
        sqlite3_result_null(pContext);
        return;
    }

    const auto eInterpolation =
        argc == 6 ? GDALRasterIOGetResampleAlg(reinterpret_cast<const char *>(
                        sqlite3_value_text(argv[5])))
                  : GRIORA_NearestNeighbour;

    const auto eDT = poBand->GetRasterDataType();
    if (eDT == GDT_Int64 && eInterpolation == GRIORA_NearestNeighbour)
    {
        int64_t nValue = 0;
        if (poBand->RasterIO(GF_Read, static_cast<int>(x), static_cast<int>(y),
                             1, 1, &nValue, 1, 1, GDT_Int64, 0, 0,
                             nullptr) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        return sqlite3_result_int64(pContext, nValue);
    }
    else if (eDT == GDT_UInt64 && eInterpolation == GRIORA_NearestNeighbour)
    {
        uint64_t nValue = 0;
        if (poBand->RasterIO(GF_Read, static_cast<int>(x), static_cast<int>(y),
                             1, 1, &nValue, 1, 1, GDT_UInt64, 0, 0,
                             nullptr) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        if (nValue > static_cast<uint64_t>(INT64_MAX))
        {
            // Not ideal
            return sqlite3_result_double(pContext, static_cast<double>(nValue));
        }
        else
        {
            return sqlite3_result_int64(pContext, static_cast<int64_t>(nValue));
        }
    }
    else
    {
        double dfValue = 0;
        if (poBand->InterpolateAtPoint(x, y, eInterpolation, &dfValue,
                                       nullptr) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        return sqlite3_result_double(pContext, dfValue);
    }
}

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) ||     \
    defined(ALLOW_FORMAT_DUMPS)

/************************************************************************/
/*                          SQLCheckLineIsSafe()                        */
/************************************************************************/

bool SQLCheckLineIsSafe(const char *pszLine)
{
    CPLString osLine;
    // Strip identifiers and string literals from line
    char chStringEnd = 0;
    for (size_t i = 0; pszLine[i]; ++i)
    {
        if (chStringEnd)
        {
            if (pszLine[i] == chStringEnd && pszLine[i + 1] == chStringEnd)
                ++i;
            else if (pszLine[i] == chStringEnd)
            {
                osLine += chStringEnd;
                chStringEnd = 0;
            }
        }
        else if (pszLine[i] == '\'' || pszLine[i] == '"')
            chStringEnd = pszLine[i];
        else
            osLine += chStringEnd;
    }
    osLine.replaceAll("replace(", 'x');

    // Reject a few words tat might have security implications
    // Basically we just want to allow CREATE TABLE and INSERT INTO
    if (osLine.ifind("ATTACH") != std::string::npos ||
        osLine.ifind("DETACH") != std::string::npos ||
        osLine.ifind("PRAGMA") != std::string::npos ||
        osLine.ifind("SELECT") != std::string::npos ||
        osLine.ifind("UPDATE") != std::string::npos ||
        osLine.ifind("REPLACE") != std::string::npos ||
        osLine.ifind("DELETE") != std::string::npos ||
        osLine.ifind("DROP") != std::string::npos ||
        osLine.ifind("ALTER") != std::string::npos ||
        osLine.ifind("VIRTUAL") != std::string::npos)
    {
        bool bOK = false;
        // Accept creation of spatial index
        if (STARTS_WITH_CI(pszLine, "CREATE VIRTUAL TABLE "))
        {
            const char *pszStr = pszLine + strlen("CREATE VIRTUAL TABLE ");
            if (*pszStr == '"')
                pszStr++;
            while ((*pszStr >= 'a' && *pszStr <= 'z') ||
                   (*pszStr >= 'A' && *pszStr <= 'Z') || *pszStr == '_')
            {
                pszStr++;
            }
            if (*pszStr == '"')
                pszStr++;
            if (EQUAL(pszStr, " USING rtree(id, minx, maxx, miny, maxy);"))
            {
                bOK = true;
            }
        }
        // Accept INSERT INTO rtree_poly_geom SELECT fid, ST_MinX(geom),
        // ST_MaxX(geom), ST_MinY(geom), ST_MaxY(geom) FROM poly;
        else if (STARTS_WITH_CI(pszLine, "INSERT INTO rtree_") &&
                 CPLString(pszLine).ifind("SELECT") != std::string::npos)
        {
            const CPLStringList aosTokens(
                CSLTokenizeString2(pszLine, " (),,", 0));
            if (aosTokens.size() == 15 && EQUAL(aosTokens[3], "SELECT") &&
                EQUAL(aosTokens[5], "ST_MinX") &&
                EQUAL(aosTokens[7], "ST_MaxX") &&
                EQUAL(aosTokens[9], "ST_MinY") &&
                EQUAL(aosTokens[11], "ST_MaxY") && EQUAL(aosTokens[13], "FROM"))
            {
                bOK = true;
            }
        }

        if (!bOK)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Rejected statement: %s",
                     pszLine);
            return false;
        }
    }
    return true;
}

#endif

int SQLPrepareWithError(sqlite3 *db, const char *sql, int nByte,
                        sqlite3_stmt **ppStmt, const char **pzTail)
{
    int ret = sqlite3_prepare_v2(db, sql, nByte, ppStmt, pzTail);
    if (ret != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to prepare SQL %s: %s",
                 sql, sqlite3_errmsg(db));
    }
    return ret;
}
