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

#include "cpl_port.h"
#include "ogrsqliteutility.h"

#include <cstdlib>
#include <string>
#include <regex>
#include <iostream>
#include <sstream>

#include "cpl_error.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

SQLResult::SQLResult(char** result, int nRow, int nCol)
    : papszResult(result), nRowCount(nRow), nColCount(nCol) {
}

SQLResult::~SQLResult () {
    if (papszResult) {
        sqlite3_free_table(papszResult);
    }
}

void SQLResult::LimitRowCount(int nLimit) {
    nRowCount = nLimit;
}

/* Runs a SQL command and ignores the result (good for INSERT/UPDATE/CREATE) */
OGRErr SQLCommand(sqlite3 * poDb, const char * pszSQL)
{
    CPLAssert( poDb != nullptr );
    CPLAssert( pszSQL != nullptr );

    char *pszErrMsg = nullptr;
#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "exec(%s)", pszSQL);
#endif
    int rc = sqlite3_exec(poDb, pszSQL, nullptr, nullptr, &pszErrMsg);

    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_exec(%s) failed: %s",
                  pszSQL, pszErrMsg ? pszErrMsg : "" );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

std::unique_ptr<SQLResult> SQLQuery(sqlite3 * poDb, const char * pszSQL)
{
    CPLAssert( poDb != nullptr );
    CPLAssert( pszSQL != nullptr );

#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "get_table(%s)", pszSQL);
#endif

    char** papszResult = nullptr;
    char* pszErrMsg = nullptr;
    int nRowCount, nColCount;
    int rc = sqlite3_get_table(
        poDb, pszSQL,
        &(papszResult),
        &(nRowCount),
        &(nColCount),
        &(pszErrMsg) );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_get_table(%s) failed: %s", pszSQL, pszErrMsg );
        sqlite3_free(pszErrMsg);
        return nullptr;
    }

    return cpl::make_unique<SQLResult>(papszResult, nRowCount, nColCount);
}

const char* SQLResult::GetValue(int iColNum, int iRowNum) const
{
    const int nCols = nColCount;
#ifdef DEBUG
    const int nRows = nRowCount;
    CPL_IGNORE_RET_VAL(nRows);

    CPLAssert( iColNum >= 0 && iColNum < nCols );
    CPLAssert( iRowNum >= 0 && iRowNum < nRows );
#endif
    return papszResult[ nCols + iRowNum * nCols + iColNum ];
}

int SQLResult::GetValueAsInteger(int iColNum, int iRowNum) const
{
    const char *pszValue = GetValue(iColNum, iRowNum);
    if ( ! pszValue )
        return 0;

    return atoi(pszValue);
}

/* Returns the first row of first column of SQL as integer */
GIntBig SQLGetInteger64(sqlite3 * poDb, const char * pszSQL, OGRErr *err)
{
    CPLAssert( poDb != nullptr );

    sqlite3_stmt *poStmt = nullptr;

    /* Prepare the SQL */
#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "get(%s)", pszSQL);
#endif
    int rc = sqlite3_prepare_v2(poDb, pszSQL, -1, &poStmt, nullptr);
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_prepare_v2(%s) failed: %s",
                  pszSQL, sqlite3_errmsg( poDb ) );
        if ( err ) *err = OGRERR_FAILURE;
        return 0;
    }

    /* Execute and fetch first row */
    rc = sqlite3_step(poStmt);
    if ( rc != SQLITE_ROW )
    {
        if ( err ) *err = OGRERR_FAILURE;
        sqlite3_finalize(poStmt);
        return 0;
    }

    /* Read the integer from the row */
    GIntBig i = sqlite3_column_int64(poStmt, 0);
    sqlite3_finalize(poStmt);

    if ( err ) *err = OGRERR_NONE;
    return i;
}

int SQLGetInteger(sqlite3 * poDb, const char * pszSQL, OGRErr *err)
{
    return static_cast<int>(SQLGetInteger64(poDb, pszSQL, err));
}

int SQLiteFieldFromOGR(OGRFieldType eType)
{
    switch(eType)
    {
        case OFTInteger:
            return SQLITE_INTEGER;
        case OFTReal:
            return SQLITE_FLOAT;
        case OFTString:
            return SQLITE_TEXT;
        case OFTBinary:
            return SQLITE_BLOB;
        case OFTDate:
            return SQLITE_TEXT;
        case OFTDateTime:
            return SQLITE_TEXT;
        default:
            return 0;
    }
}

/************************************************************************/
/*                             SQLUnescape()                            */
/************************************************************************/

CPLString SQLUnescape(const char* pszVal)
{
    char chQuoteChar = pszVal[0];
    if( chQuoteChar != '\'' && chQuoteChar != '"' )
        return pszVal;

    CPLString osRet;
    pszVal ++;
    while( *pszVal != '\0' )
    {
        if( *pszVal == chQuoteChar )
        {
            if( pszVal[1] == chQuoteChar )
                pszVal ++;
            else
                break;
        }
        osRet += *pszVal;
        pszVal ++;
    }
    return osRet;
}

/************************************************************************/
/*                          SQLEscapeLiteral()                          */
/************************************************************************/

CPLString SQLEscapeLiteral( const char *pszLiteral )
{
    CPLString osVal;
    for( int i = 0; pszLiteral[i] != '\0'; i++ )
    {
        if ( pszLiteral[i] == '\'' )
            osVal += '\'';
        osVal += pszLiteral[i];
    }
    return osVal;
}

/************************************************************************/
/*                           SQLEscapeName()                            */
/************************************************************************/

CPLString SQLEscapeName(const char* pszName)
{
    CPLString osRet;
    while( *pszName != '\0' )
    {
        if( *pszName == '"' )
            osRet += "\"\"";
        else
            osRet += *pszName;
        pszName ++;
    }
    return osRet;
}

/************************************************************************/
/*                             SQLTokenize()                            */
/************************************************************************/

char** SQLTokenize( const char* pszStr )
{
    char** papszTokens = nullptr;
    bool bInQuote = false;
    char chQuoteChar = '\0';
    bool bInSpace = true;
    CPLString osCurrentToken;
    while( *pszStr != '\0' )
    {
        if( *pszStr == ' ' && !bInQuote )
        {
            if( !bInSpace )
            {
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
            }
            bInSpace = true;
        }
        else if( (*pszStr == '(' || *pszStr == ')' || *pszStr == ',')  && !bInQuote )
        {
            if( !bInSpace )
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
        else if( *pszStr == '"' || *pszStr == '\'' )
        {
            if( bInQuote && *pszStr == chQuoteChar && pszStr[1] == chQuoteChar )
            {
                osCurrentToken += *pszStr;
                osCurrentToken += *pszStr;
                pszStr += 2;
                continue;
            }
            else if( bInQuote && *pszStr == chQuoteChar )
            {
                osCurrentToken += *pszStr;
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
                bInSpace = true;
                bInQuote = false;
                chQuoteChar = '\0';
            }
            else if( bInQuote )
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
        pszStr ++;
    }

    if( !osCurrentToken.empty() )
        papszTokens = CSLAddString(papszTokens, osCurrentToken);

    return papszTokens;
}

/************************************************************************/
/*                    SQLGetUniqueFieldUCConstraints()                  */
/************************************************************************/

/* Return set of field names (in upper case) that have a UNIQUE constraint,
 * only on that single column.
 */
std::set<std::string> SQLGetUniqueFieldUCConstraints(sqlite3* poDb,
                                                   const char* pszTableName)
{
    // set names (in upper case) of fields with unique constraint
    std::set<std::string> uniqueFieldsUC;

    // std::regex in gcc < 4.9 is broken
#if !defined(__GNUC__) || defined(__clang__) || __GNUC__ >= 5

    try
    {
        static int hasWorkingRegex = std::regex_match("c", std::regex("a|b|c"));
        if( !hasWorkingRegex )
        {
            // Can happen if we build with clang, but run against libstdc++ of gcc < 4.9.
            return uniqueFieldsUC;
        }

        // Unique fields detection
        const std::string upperTableName { CPLString( pszTableName ).toupper() };
        char* pszTableDefinitionSQL = sqlite3_mprintf(
            "SELECT sql, type FROM sqlite_master "
            "WHERE type IN ('table', 'view') AND UPPER(name)='%q'", upperTableName.c_str() );
        auto oResultTable = SQLQuery(poDb, pszTableDefinitionSQL);
        sqlite3_free(pszTableDefinitionSQL);

        if ( !oResultTable || oResultTable->RowCount() == 0 )
        {
            if( oResultTable )
                CPLError( CE_Failure, CPLE_AppDefined, "Cannot find table %s", pszTableName );

            return uniqueFieldsUC;
        }
        if( std::string(oResultTable->GetValue(1, 0)) == "view" )
        {
            return uniqueFieldsUC;
        }

        // Match identifiers with ", ', or ` or no delimiter (and no spaces).
        std::string tableDefinition { oResultTable->GetValue(0, 0) };
        tableDefinition = tableDefinition.substr(tableDefinition.find('('), tableDefinition.rfind(')') );
        std::stringstream tableDefinitionStream { tableDefinition };
        std::smatch uniqueFieldMatch;
        while (tableDefinitionStream.good()) {
            std::string fieldStr;
            std::getline( tableDefinitionStream, fieldStr, ',' );
            if( CPLString( fieldStr ).toupper().find( "UNIQUE" ) != std::string::npos )
            {
                static const std::regex sFieldIdentifierRe {
                    R"raw(^\s*((["'`]([^"'`]+)["'`])|(([^"'`\s]+)\s)).*UNIQUE.*)raw",
                    std::regex_constants::icase};
                if( std::regex_search(fieldStr, uniqueFieldMatch, sFieldIdentifierRe) )
                {
                    const std::string quoted { uniqueFieldMatch.str( 3 ) };
                    uniqueFieldsUC.insert( CPLString(
                        !quoted.empty() ? quoted: uniqueFieldMatch.str( 5 )).toupper() );
                }
            }
        }

        // Search indexes:
        pszTableDefinitionSQL = sqlite3_mprintf(
            "SELECT sql FROM sqlite_master WHERE type='index' AND"
            " UPPER(tbl_name)=UPPER('%q') AND UPPER(sql) "
            "LIKE 'CREATE UNIQUE INDEX%%'", upperTableName.c_str() );
        oResultTable = SQLQuery(poDb, pszTableDefinitionSQL);
        sqlite3_free(pszTableDefinitionSQL);

        if ( !oResultTable  )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Error searching indexes for table %s", pszTableName );
        }
        else if (oResultTable->RowCount() >= 0 )
        {
            for( int rowCnt = 0; rowCnt < oResultTable->RowCount(); ++rowCnt )
            {
                std::string indexDefinition { oResultTable->GetValue(0, rowCnt) };
                if ( CPLString (indexDefinition ).toupper().find( "UNIQUE" ) != std::string::npos )
                {
                    indexDefinition = indexDefinition.substr(
                        indexDefinition.find('('), indexDefinition.rfind(')') );
                    static const std::regex sFieldIndexIdentifierRe {
                        R"raw(\(\s*[`"]?([^",`\)]+)["`]?\s*\))raw" };
                    if( std::regex_search(indexDefinition, uniqueFieldMatch,
                        sFieldIndexIdentifierRe) )
                    {
                        uniqueFieldsUC.insert( CPLString(uniqueFieldMatch.str( 1 )).toupper() );
                    }
                }
            }
        }
    }
    catch( const std::regex_error& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "regex_error: %s", e.what());
    }

#endif  // <-- Unique detection
    return uniqueFieldsUC;
}
