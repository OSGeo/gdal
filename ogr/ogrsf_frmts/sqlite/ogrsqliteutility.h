/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Utility header for OGR GeoPackage driver.
 * Author:   Paul Ramsey, pramsey@boundlessgeo.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SQLITEUTILITY_H_INCLUDED
#define OGR_SQLITEUTILITY_H_INCLUDED

#include "ogr_core.h"
#include "cpl_string.h"
#include "sqlite3.h"

#include <set>
#include <string>
#include <memory>
#include <vector>

class SQLResult
{
  public:
    SQLResult(char **result, int nRow, int nCol);
    ~SQLResult();

    int RowCount() const
    {
        return nRowCount;
    }

    int ColCount() const
    {
        return nColCount;
    }

    void LimitRowCount(int nLimit);

    const char *GetValue(int iColumnNum, int iRowNum) const;
    int GetValueAsInteger(int iColNum, int iRowNum) const;
    double GetValueAsDouble(int iColNum, int iRowNum) const;

  private:
    char **papszResult = nullptr;
    int nRowCount = 0;
    int nColCount = 0;

    CPL_DISALLOW_COPY_ASSIGN(SQLResult)
};

OGRErr SQLCommand(sqlite3 *poDb, const char *pszSQL);
int SQLGetInteger(sqlite3 *poDb, const char *pszSQL, OGRErr *err);
GIntBig SQLGetInteger64(sqlite3 *poDb, const char *pszSQL, OGRErr *err);

std::unique_ptr<SQLResult> SQLQuery(sqlite3 *poDb, const char *pszSQL);

/* To escape literals. The returned string doesn't contain the surrounding
 * single quotes */
CPLString SQLEscapeLiteral(const char *pszLiteral);

/* To escape table or field names. The returned string doesn't contain the
 * surrounding double quotes */
CPLString SQLEscapeName(const char *pszName);

/* Remove leading ' or " and unescape in that case. Or return string unmodified
 */
CPLString SQLUnescape(const char *pszVal);

char **SQLTokenize(const char *pszSQL);

struct SQLSqliteMasterContent
{
    std::string osSQL{};
    std::string osType{};
    std::string osTableName{};
};

std::set<std::string> SQLGetUniqueFieldUCConstraints(
    sqlite3 *poDb, const char *pszTableName,
    const std::vector<SQLSqliteMasterContent> &sqliteMasterContent =
        std::vector<SQLSqliteMasterContent>());

bool OGRSQLiteRTreeRequiresTrustedSchemaOn();

bool OGRSQLiteIsSpatialFunctionReturningGeometry(const char *pszName);

/* Wrapper of sqlite3_prepare_v2() that emits a CPLError() if failure */
int SQLPrepareWithError(sqlite3 *db, const char *sql, int nByte,
                        sqlite3_stmt **ppStmt, const char **pzTail);

class GDALDataset;

void OGRSQLite_gdal_get_pixel_value_common(const char *pszFunctionName,
                                           sqlite3_context *pContext, int argc,
                                           sqlite3_value **argv,
                                           GDALDataset *poDS);

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) ||     \
    defined(ALLOW_FORMAT_DUMPS)
bool SQLCheckLineIsSafe(const char *pszLine);
#endif

#endif  // OGR_SQLITEUTILITY_H_INCLUDED
