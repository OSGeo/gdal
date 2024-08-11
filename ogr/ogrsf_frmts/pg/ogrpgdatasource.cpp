/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include <string.h>
#include "ogr_pg.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_hash_set.h"
#include <cctype>
#include <set>

#define PQexec this_is_an_error

static void OGRPGNoticeProcessor(void *arg, const char *pszMessage);

/************************************************************************/
/*                          OGRPGDataSource()                           */
/************************************************************************/

OGRPGDataSource::OGRPGDataSource() = default;

/************************************************************************/
/*                          ~OGRPGDataSource()                          */
/************************************************************************/

OGRPGDataSource::~OGRPGDataSource()

{
    OGRPGDataSource::FlushCache(true);

    CPLFree(pszName);
    CPLFree(pszForcedTables);
    CSLDestroy(papszSchemaList);

    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];

    CPLFree(papoLayers);

    if (hPGConn != nullptr)
    {
        // If there are prelude statements, don't mess with transactions.
        if (CSLFetchNameValue(papszOpenOptions, "PRELUDE_STATEMENTS") ==
            nullptr)
            FlushSoftTransaction();

        /* --------------------------------------------------------------------
         */
        /*      Send closing statements */
        /* --------------------------------------------------------------------
         */
        const char *pszClosingStatements =
            CSLFetchNameValue(papszOpenOptions, "CLOSING_STATEMENTS");
        if (pszClosingStatements != nullptr)
        {
            PGresult *hResult =
                OGRPG_PQexec(hPGConn, pszClosingStatements, TRUE);
            OGRPGClearResult(hResult);
        }

        /* XXX - mloskot: After the connection is closed, valgrind still
         * reports 36 bytes definitely lost, somewhere in the libpq.
         */
        PQfinish(hPGConn);
        hPGConn = nullptr;
    }
}

/************************************************************************/
/*                              FlushCache()                            */
/************************************************************************/

OGRErr OGRPGDataSource::FlushCacheWithRet(bool /* bAtClosing */)
{
    OGRErr eErr = EndCopy();
    if (eErr == OGRERR_NONE)
    {
        for (int iLayer = 0; iLayer < nLayers; iLayer++)
        {
            papoLayers[iLayer]->RunDeferredCreationIfNecessary();
        }
    }
    return eErr;
}

CPLErr OGRPGDataSource::FlushCache(bool bAtClosing)
{
    return FlushCacheWithRet(bAtClosing) == OGRERR_NONE ? CE_None : CE_Failure;
}

/************************************************************************/
/*                         GetCurrentSchema()                           */
/************************************************************************/

CPLString OGRPGDataSource::GetCurrentSchema()
{
    /* -------------------------------------------- */
    /*          Get the current schema              */
    /* -------------------------------------------- */
    PGresult *hResult = OGRPG_PQexec(hPGConn, "SELECT current_schema()");
    if (hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult, 0, 0))
    {
        osCurrentSchema = PQgetvalue(hResult, 0, 0);
    }
    OGRPGClearResult(hResult);

    return osCurrentSchema;
}

/************************************************************************/
/*                      OGRPGDecodeVersionString()                      */
/************************************************************************/

void OGRPGDataSource::OGRPGDecodeVersionString(PGver *psVersion,
                                               const char *pszVer)
{
    while (*pszVer == ' ')
        pszVer++;

    const char *ptr = pszVer;
    // get Version string
    while (*ptr && *ptr != ' ')
        ptr++;
    GUInt32 iLen = static_cast<int>(ptr - pszVer);
    char szVer[10] = {};
    if (iLen > sizeof(szVer) - 1)
        iLen = sizeof(szVer) - 1;
    strncpy(szVer, pszVer, iLen);
    szVer[iLen] = '\0';

    ptr = pszVer = szVer;

    // get Major number
    while (*ptr && *ptr != '.')
        ptr++;
    iLen = static_cast<int>(ptr - pszVer);
    char szNum[25] = {};
    if (iLen > sizeof(szNum) - 1)
        iLen = sizeof(szNum) - 1;
    strncpy(szNum, pszVer, iLen);
    szNum[iLen] = '\0';
    psVersion->nMajor = atoi(szNum);

    if (*ptr == 0)
        return;
    pszVer = ++ptr;

    // get Minor number
    while (*ptr && *ptr != '.')
        ptr++;
    iLen = static_cast<int>(ptr - pszVer);
    if (iLen > sizeof(szNum) - 1)
        iLen = sizeof(szNum) - 1;
    strncpy(szNum, pszVer, iLen);
    szNum[iLen] = '\0';
    psVersion->nMinor = atoi(szNum);

    if (*ptr)
    {
        pszVer = ++ptr;

        // get Release number
        while (*ptr && *ptr != '.')
            ptr++;
        iLen = static_cast<int>(ptr - pszVer);
        if (iLen > sizeof(szNum) - 1)
            iLen = sizeof(szNum) - 1;
        strncpy(szNum, pszVer, iLen);
        szNum[iLen] = '\0';
        psVersion->nRelease = atoi(szNum);
    }
}

/************************************************************************/
/*                     One entry for each PG table                      */
/************************************************************************/

struct PGTableEntry
{
    char *pszTableName = nullptr;
    char *pszSchemaName = nullptr;
    char *pszDescription = nullptr;
    int nGeomColumnCount = 0;
    PGGeomColumnDesc *pasGeomColumns = nullptr; /* list of geometry columns */
    int bDerivedInfoAdded =
        false; /* set to TRUE if it derives from another table */
};

static unsigned long OGRPGHashTableEntry(const void *_psTableEntry)
{
    const PGTableEntry *psTableEntry =
        static_cast<const PGTableEntry *>(_psTableEntry);
    return CPLHashSetHashStr(CPLString().Printf(
        "%s.%s", psTableEntry->pszSchemaName, psTableEntry->pszTableName));
}

static int OGRPGEqualTableEntry(const void *_psTableEntry1,
                                const void *_psTableEntry2)
{
    const PGTableEntry *psTableEntry1 =
        static_cast<const PGTableEntry *>(_psTableEntry1);
    const PGTableEntry *psTableEntry2 =
        static_cast<const PGTableEntry *>(_psTableEntry2);
    return strcmp(psTableEntry1->pszTableName, psTableEntry2->pszTableName) ==
               0 &&
           strcmp(psTableEntry1->pszSchemaName, psTableEntry2->pszSchemaName) ==
               0;
}

static void OGRPGTableEntryAddGeomColumn(
    PGTableEntry *psTableEntry, const char *pszName,
    const char *pszGeomType = nullptr, int GeometryTypeFlags = 0,
    int nSRID = UNDETERMINED_SRID, PostgisType ePostgisType = GEOM_TYPE_UNKNOWN,
    int bNullable = TRUE)
{
    psTableEntry->pasGeomColumns = static_cast<PGGeomColumnDesc *>(CPLRealloc(
        psTableEntry->pasGeomColumns,
        sizeof(PGGeomColumnDesc) * (psTableEntry->nGeomColumnCount + 1)));
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].pszName =
        CPLStrdup(pszName);
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].pszGeomType =
        (pszGeomType) ? CPLStrdup(pszGeomType) : nullptr;
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount]
        .GeometryTypeFlags = GeometryTypeFlags;
    /* With PostGIS 2.0, querying geometry_columns can return 0, not only when
     */
    /* the SRID is truly set to 0, but also when there's no constraint */
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].nSRID =
        nSRID > 0 ? nSRID : UNDETERMINED_SRID;
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].ePostgisType =
        ePostgisType;
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].bNullable =
        bNullable;
    psTableEntry->nGeomColumnCount++;
}

static void OGRPGFreeTableEntry(void *_psTableEntry)
{
    PGTableEntry *psTableEntry = static_cast<PGTableEntry *>(_psTableEntry);
    CPLFree(psTableEntry->pszTableName);
    CPLFree(psTableEntry->pszSchemaName);
    CPLFree(psTableEntry->pszDescription);
    for (int i = 0; i < psTableEntry->nGeomColumnCount; i++)
    {
        CPLFree(psTableEntry->pasGeomColumns[i].pszName);
        CPLFree(psTableEntry->pasGeomColumns[i].pszGeomType);
    }
    CPLFree(psTableEntry->pasGeomColumns);
    CPLFree(psTableEntry);
}

static PGTableEntry *OGRPGFindTableEntry(CPLHashSet *hSetTables,
                                         const char *pszTableName,
                                         const char *pszSchemaName)
{
    PGTableEntry sEntry;
    sEntry.pszTableName = const_cast<char *>(pszTableName);
    sEntry.pszSchemaName = const_cast<char *>(pszSchemaName);
    return static_cast<PGTableEntry *>(CPLHashSetLookup(hSetTables, &sEntry));
}

static PGTableEntry *OGRPGAddTableEntry(CPLHashSet *hSetTables,
                                        const char *pszTableName,
                                        const char *pszSchemaName,
                                        const char *pszDescription)
{
    PGTableEntry *psEntry =
        static_cast<PGTableEntry *>(CPLCalloc(1, sizeof(PGTableEntry)));
    psEntry->pszTableName = CPLStrdup(pszTableName);
    psEntry->pszSchemaName = CPLStrdup(pszSchemaName);
    psEntry->pszDescription = CPLStrdup(pszDescription ? pszDescription : "");

    CPLHashSetInsert(hSetTables, psEntry);

    return psEntry;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRPGDataSource::Open(const char *pszNewName, int bUpdate, int bTestOpen,
                          char **papszOpenOptionsIn)

{
    CPLAssert(nLayers == 0);
    papszOpenOptions = CSLDuplicate(papszOpenOptionsIn);

    const char *pszPreludeStatements =
        CSLFetchNameValue(papszOpenOptions, "PRELUDE_STATEMENTS");
    if (pszPreludeStatements)
    {
        // If the prelude statements starts with BEGIN, then don't emit one
        // in our code.
        if (STARTS_WITH_CI(pszPreludeStatements, "BEGIN"))
            nSoftTransactionLevel = 1;
    }

    /* -------------------------------------------------------------------- */
    /*      Verify postgresql prefix.                                       */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszNewName, "PGB:"))
    {
#if defined(BINARY_CURSOR_ENABLED)
        bUseBinaryCursor = TRUE;
        CPLDebug("PG", "BINARY cursor is used for geometry fetching");
#endif
    }
    else if (!STARTS_WITH_CI(pszNewName, "PG:") &&
             !STARTS_WITH(pszNewName, "postgresql://"))
    {
        if (!bTestOpen)
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s does not conform to PostgreSQL naming convention,"
                     " PG:* or postgresql://\n",
                     pszNewName);
        return FALSE;
    }

    pszName = CPLStrdup(pszNewName);

    const auto QuoteAndEscapeConnectionParam = [](const char *pszParam)
    {
        CPLString osRet("\'");
        for (int i = 0; pszParam[i]; ++i)
        {
            if (pszParam[i] == '\'')
                osRet += "\\'";
            else if (pszParam[i] == '\\')
                osRet += "\\\\";
            else
                osRet += pszParam[i];
        }
        osRet += '\'';
        return osRet;
    };

    CPLString osConnectionName(pszName);
    if (osConnectionName.find("PG:postgresql://") == 0)
        osConnectionName = osConnectionName.substr(3);
    const bool bIsURI = osConnectionName.find("postgresql://") == 0;

    const char *const apszOpenOptions[] = {
        "service", "dbname", "port", "user", "password", "host",
        // Non-postgreSQL options
        "active_schema", "schemas", "tables"};
    std::string osSchemas;
    std::string osForcedTables;
    for (const char *pszOpenOption : apszOpenOptions)
    {
        const char *pszVal = CSLFetchNameValue(papszOpenOptions, pszOpenOption);
        if (pszVal && strcmp(pszOpenOption, "active_schema") == 0)
        {
            osActiveSchema = pszVal;
        }
        else if (pszVal && strcmp(pszOpenOption, "schemas") == 0)
        {
            osSchemas = pszVal;
        }
        else if (pszVal && strcmp(pszOpenOption, "tables") == 0)
        {
            osForcedTables = pszVal;
        }
        else if (pszVal)
        {
            if (bIsURI)
            {
                osConnectionName +=
                    osConnectionName.find('?') == std::string::npos ? '?' : '&';
            }
            else
            {
                if (osConnectionName.back() != ':')
                    osConnectionName += ' ';
            }
            osConnectionName += pszOpenOption;
            osConnectionName += "=";
            if (bIsURI)
            {
                char *pszTmp = CPLEscapeString(pszVal, -1, CPLES_URL);
                osConnectionName += pszTmp;
                CPLFree(pszTmp);
            }
            else
            {
                osConnectionName += QuoteAndEscapeConnectionParam(pszVal);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Set application name if not found in connection string          */
    /* -------------------------------------------------------------------- */

    if (strstr(pszName, "application_name") == nullptr &&
        getenv("PGAPPNAME") == nullptr)
    {
        if (bIsURI)
        {
            osConnectionName +=
                osConnectionName.find('?') == std::string::npos ? '?' : '&';
        }
        else
        {
            if (osConnectionName.back() != ':')
                osConnectionName += ' ';
        }
        osConnectionName += "application_name=";
        std::string osVal("GDAL ");
        osVal += GDALVersionInfo("RELEASE_NAME");
        if (bIsURI)
        {
            char *pszTmp = CPLEscapeString(osVal.c_str(), -1, CPLES_URL);
            osConnectionName += pszTmp;
            CPLFree(pszTmp);
        }
        else
        {
            osConnectionName += QuoteAndEscapeConnectionParam(osVal.c_str());
        }
    }

    const auto ParseAndRemoveParam =
        [](char *pszStr, const char *pszParamName, std::string &osValue)
    {
        const int nParamNameLen = static_cast<int>(strlen(pszParamName));
        bool bInSingleQuotedString = false;
        for (int i = 0; pszStr[i]; i++)
        {
            if (bInSingleQuotedString)
            {
                if (pszStr[i] == '\\')
                {
                    if (pszStr[i + 1] == '\\' || pszStr[i + 1] == '\'')
                    {
                        ++i;
                    }
                }
                else if (pszStr[i] == '\'')
                {
                    bInSingleQuotedString = false;
                }
            }
            else if (pszStr[i] == '\'')
            {
                bInSingleQuotedString = true;
            }
            else if (EQUALN(pszStr + i, pszParamName, nParamNameLen) &&
                     (pszStr[i + nParamNameLen] == '=' ||
                      pszStr[i + nParamNameLen] == ' '))
            {
                const int iStart = i;
                i += nParamNameLen;
                while (pszStr[i] == ' ')
                    ++i;
                if (pszStr[i] == '=')
                {
                    ++i;
                    while (pszStr[i] == ' ')
                        ++i;
                    if (pszStr[i] == '\'')
                    {
                        ++i;
                        for (; pszStr[i]; i++)
                        {
                            if (pszStr[i] == '\\')
                            {
                                if (pszStr[i + 1] == '\\' ||
                                    pszStr[i + 1] == '\'')
                                {
                                    osValue += pszStr[i + 1];
                                    ++i;
                                }
                            }
                            else if (pszStr[i] == '\'')
                            {
                                ++i;
                                break;
                            }
                            else
                            {
                                osValue += pszStr[i];
                            }
                        }
                    }
                    else
                    {
                        for (; pszStr[i] && pszStr[i] != ' '; i++)
                        {
                            osValue += pszStr[i];
                        }
                    }

                    // Edit pszStr to remove the parameter and its value
                    if (pszStr[i] == ' ')
                    {
                        memmove(pszStr + iStart, pszStr + i,
                                strlen(pszStr + i) + 1);
                    }
                    else
                    {
                        pszStr[iStart] = 0;
                    }
                }
                return true;
            }
        }
        return false;
    };

    char *pszConnectionName = CPLStrdup(osConnectionName);
    char *pszConnectionNameNoPrefix =
        pszConnectionName + (STARTS_WITH_CI(pszConnectionName, "PGB:")  ? 4
                             : STARTS_WITH_CI(pszConnectionName, "PG:") ? 3
                                                                        : 0);

    /* -------------------------------------------------------------------- */
    /*      Determine if the connection string contains an optional         */
    /*      ACTIVE_SCHEMA portion. If so, parse it out.                     */
    /* -------------------------------------------------------------------- */
    if (osActiveSchema.empty() && !bIsURI &&
        !ParseAndRemoveParam(pszConnectionNameNoPrefix, "active_schema",
                             osActiveSchema))
    {
        osActiveSchema = "public";
    }

    /* -------------------------------------------------------------------- */
    /*      Determine if the connection string contains an optional         */
    /*      SCHEMAS portion. If so, parse it out.                           */
    /* -------------------------------------------------------------------- */
    if (!osSchemas.empty() ||
        (!bIsURI &&
         ParseAndRemoveParam(pszConnectionNameNoPrefix, "schemas", osSchemas)))
    {
        papszSchemaList = CSLTokenizeString2(osSchemas.c_str(), ",", 0);

        /* If there is only one schema specified, make it the active schema */
        if (CSLCount(papszSchemaList) == 1)
        {
            osActiveSchema = papszSchemaList[0];
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Determine if the connection string contains an optional         */
    /*      TABLES portion. If so, parse it out. The expected               */
    /*      connection string in this case will be, e.g.:                   */
    /*                                                                      */
    /*        'PG:dbname=warmerda user=warmerda tables=s1.t1,[s2.t2,...]    */
    /*              - where sN is schema and tN is table name               */
    /*      We must also strip this information from the connection         */
    /*      string; PQconnectdb() does not like unknown directives          */
    /* -------------------------------------------------------------------- */
    if (!osForcedTables.empty() ||
        (!bIsURI && ParseAndRemoveParam(pszConnectionNameNoPrefix, "tables",
                                        osForcedTables)))
    {
        pszForcedTables = CPLStrdup(osForcedTables.c_str());
    }

    /* -------------------------------------------------------------------- */
    /*      Try to establish connection.                                    */
    /* -------------------------------------------------------------------- */
    hPGConn = PQconnectdb(pszConnectionNameNoPrefix);
    CPLFree(pszConnectionName);
    pszConnectionName = nullptr;

    if (hPGConn == nullptr || PQstatus(hPGConn) == CONNECTION_BAD)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "PQconnectdb failed.\n%s",
                 PQerrorMessage(hPGConn));

        PQfinish(hPGConn);
        hPGConn = nullptr;

        return FALSE;
    }

    bDSUpdate = bUpdate;

    /* -------------------------------------------------------------------- */
    /*      Send prelude statements                                         */
    /* -------------------------------------------------------------------- */
    if (pszPreludeStatements != nullptr)
    {
        PGresult *hResult = OGRPG_PQexec(hPGConn, pszPreludeStatements, TRUE);
        if (!hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK)
        {
            OGRPGClearResult(hResult);
            return FALSE;
        }

        OGRPGClearResult(hResult);
    }

    /* -------------------------------------------------------------------- */
    /*      Set the encoding to UTF8 as the driver advertises UTF8          */
    /*      unless PGCLIENTENCODING is defined                              */
    /* -------------------------------------------------------------------- */
    if (CPLGetConfigOption("PGCLIENTENCODING", nullptr) == nullptr)
    {
        const char *encoding = "UTF8";
        if (PQsetClientEncoding(hPGConn, encoding) == -1)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "PQsetClientEncoding(%s) failed.\n%s", encoding,
                     PQerrorMessage(hPGConn));
        }
    }

    {
        PGresult *hResult = OGRPG_PQexec(hPGConn, "SHOW client_encoding");
        if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
            PQntuples(hResult) == 1)
        {
            const char *pszClientEncoding = PQgetvalue(hResult, 0, 0);
            if (pszClientEncoding)
            {
                CPLDebug("PG", "Client encoding: '%s'", pszClientEncoding);
                if (EQUAL(pszClientEncoding, "UTF8"))
                {
                    m_bUTF8ClientEncoding = true;
                }
            }
        }
        OGRPGClearResult(hResult);
    }

    /* -------------------------------------------------------------------- */
    /*      Install a notice processor.                                     */
    /* -------------------------------------------------------------------- */
    PQsetNoticeProcessor(hPGConn, OGRPGNoticeProcessor, this);

    /* -------------------------------------------------------------------- */
    /*      Detect PostGIS schema                                           */
    /* -------------------------------------------------------------------- */
    CPLString osPostgisSchema;
    {
        PGresult *hResult = OGRPG_PQexec(
            hPGConn,
            "SELECT n.nspname FROM pg_proc p JOIN pg_namespace n "
            "ON n.oid = p.pronamespace WHERE proname = 'postgis_version'");
        if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
            PQntuples(hResult) > 0)
        {
            const char *pszPostgisSchema = PQgetvalue(hResult, 0, 0);

            CPLDebug("PG", "PostGIS schema: '%s'", pszPostgisSchema);

            osPostgisSchema = pszPostgisSchema;
        }
        OGRPGClearResult(hResult);
    }

    /* -------------------------------------------------------------------- */
    /*      Get search_path                                                 */
    /* -------------------------------------------------------------------- */
    std::string osSearchPath = "public";
    {
        PGresult *hResult = OGRPG_PQexec(hPGConn, "SHOW search_path");
        if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
            PQntuples(hResult) > 0)
        {
            const char *pszVal = PQgetvalue(hResult, 0, 0);
            if (pszVal)
            {
                osSearchPath = pszVal;
            }
        }
        OGRPGClearResult(hResult);
    }

    /* -------------------------------------------------------------------- */
    /*      Set active schema if different from 'public'. Also add          */
    /*      postgis schema if needed.                                       */
    /* -------------------------------------------------------------------- */
    if (osActiveSchema != "public" ||
        (!osPostgisSchema.empty() &&
         osSearchPath.find(osPostgisSchema) == std::string::npos))
    {
        std::string osNewSearchPath;
        if (osActiveSchema != "public")
        {
            osNewSearchPath +=
                OGRPGEscapeString(hPGConn, osActiveSchema.c_str());
            osNewSearchPath += ',';
        }
        osNewSearchPath += osSearchPath;
        if (!osPostgisSchema.empty() &&
            osSearchPath.find(osPostgisSchema) == std::string::npos)
        {
            osNewSearchPath += ',';
            osNewSearchPath +=
                OGRPGEscapeString(hPGConn, osPostgisSchema.c_str());
        }
        CPLDebug("PG", "Modifying search_path from %s to %s",
                 osSearchPath.c_str(), osNewSearchPath.c_str());

        std::string osCommand = "SET search_path=" + osNewSearchPath;
        PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

        if (!hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK)
        {
            OGRPGClearResult(hResult);
            CPLDebug("PG", "Command \"%s\" failed. Trying without 'public'.",
                     osCommand.c_str());
            osCommand =
                CPLSPrintf("SET search_path='%s'", osActiveSchema.c_str());
            PGresult *hResult2 = OGRPG_PQexec(hPGConn, osCommand.c_str());

            if (!hResult2 || PQresultStatus(hResult2) != PGRES_COMMAND_OK)
            {
                OGRPGClearResult(hResult2);

                CPLError(CE_Failure, CPLE_AppDefined, "%s",
                         PQerrorMessage(hPGConn));

                return FALSE;
            }
        }

        OGRPGClearResult(hResult);
    }

    /* -------------------------------------------------------------------- */
    /*      Find out PostgreSQL version                                     */
    /* -------------------------------------------------------------------- */
    sPostgreSQLVersion.nMajor = -1;
    sPostgreSQLVersion.nMinor = -1;
    sPostgreSQLVersion.nRelease = -1;

    PGresult *hResult = OGRPG_PQexec(hPGConn, "SELECT version()");
    if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
        PQntuples(hResult) > 0)
    {
        char *pszVer = PQgetvalue(hResult, 0, 0);

        CPLDebug("PG", "PostgreSQL version string : '%s'", pszVer);

        /* Should work with "PostgreSQL X.Y.Z ..." or "EnterpriseDB X.Y.Z ..."
         */
        const char *pszSpace = strchr(pszVer, ' ');
        if (pszSpace != nullptr &&
            isdigit(static_cast<unsigned char>(pszSpace[1])))
        {
            OGRPGDecodeVersionString(&sPostgreSQLVersion, pszSpace + 1);
        }
    }
    OGRPGClearResult(hResult);
    CPLAssert(nullptr ==
              hResult); /* Test if safe PQclear has not been broken */

    /* -------------------------------------------------------------------- */
    /*      Set standard_conforming_strings=ON                              */
    /* -------------------------------------------------------------------- */

    hResult = OGRPG_PQexec(hPGConn, "SET standard_conforming_strings = ON");
    if (!(hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", PQerrorMessage(hPGConn));
        OGRPGClearResult(hResult);
        return FALSE;
    }
    OGRPGClearResult(hResult);

/* -------------------------------------------------------------------- */
/*      Test if time binary format is int8 or float8                    */
/* -------------------------------------------------------------------- */
#if defined(BINARY_CURSOR_ENABLED)
    if (bUseBinaryCursor)
    {
        SoftStartTransaction();

        hResult =
            OGRPG_PQexec(hPGConn, "DECLARE gettimebinaryformat BINARY CURSOR "
                                  "FOR SELECT CAST ('00:00:01' AS time)");

        if (hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK)
        {
            OGRPGClearResult(hResult);

            hResult = OGRPG_PQexec(hPGConn, "FETCH ALL IN gettimebinaryformat");

            if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
                PQntuples(hResult) == 1)
            {
                if (PQfformat(hResult, 0) == 1)  // Binary data representation
                {
                    CPLAssert(PQgetlength(hResult, 0, 0) == 8);
                    unsigned int nVal[2] = {0, 0};
                    memcpy(nVal, PQgetvalue(hResult, 0, 0), 8);
                    CPL_MSBPTR32(&nVal[0]);
                    CPL_MSBPTR32(&nVal[1]);
                    double dVal = 0.0;
                    memcpy(&dVal, PQgetvalue(hResult, 0, 0), 8);
                    CPL_MSBPTR64(&dVal);
                    if (nVal[0] == 0 && nVal[1] == 1000000)
                    {
                        bBinaryTimeFormatIsInt8 = TRUE;
                        CPLDebug("PG", "Time binary format is int8");
                    }
                    else if (dVal == 1.)
                    {
                        bBinaryTimeFormatIsInt8 = FALSE;
                        CPLDebug("PG", "Time binary format is float8");
                    }
                    else
                    {
                        bBinaryTimeFormatIsInt8 = FALSE;
                        CPLDebug("PG", "Time binary format is unknown");
                    }
                }
            }
        }

        OGRPGClearResult(hResult);

        hResult = OGRPG_PQexec(hPGConn, "CLOSE gettimebinaryformat");
        OGRPGClearResult(hResult);

        SoftCommitTransaction();
    }
#endif

    /* -------------------------------------------------------------------- */
    /*      Test to see if this database instance has support for the       */
    /*      PostGIS Geometry type.  If so, disable sequential scanning      */
    /*      so we will get the value of the gist indexes.                   */
    /* -------------------------------------------------------------------- */
    hResult =
        OGRPG_PQexec(hPGConn, "SELECT oid, typname FROM pg_type WHERE typname "
                              "IN ('geometry', 'geography') AND typtype='b'");

    if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
        PQntuples(hResult) > 0 &&
        CPLTestBool(CPLGetConfigOption("PG_USE_POSTGIS", "YES")))
    {
        for (int iRecord = 0; iRecord < PQntuples(hResult); iRecord++)
        {
            const char *pszOid = PQgetvalue(hResult, iRecord, 0);
            const char *pszTypname = PQgetvalue(hResult, iRecord, 1);
            if (EQUAL(pszTypname, "geometry"))
            {
                bHavePostGIS = TRUE;
                nGeometryOID = static_cast<Oid>(strtoul(pszOid, nullptr, 10));
            }
            else if (CPLTestBool(CPLGetConfigOption("PG_USE_GEOGRAPHY", "YES")))
            {
                bHaveGeography = TRUE;
                nGeographyOID = static_cast<Oid>(strtoul(pszOid, nullptr, 10));
            }
        }
    }

    OGRPGClearResult(hResult);

    /* -------------------------------------------------------------------- */
    /*      Find out PostGIS version                                        */
    /* -------------------------------------------------------------------- */

    sPostGISVersion.nMajor = -1;
    sPostGISVersion.nMinor = -1;
    sPostGISVersion.nRelease = -1;

    if (bHavePostGIS)
    {
        hResult = OGRPG_PQexec(hPGConn, "SELECT postgis_version()");
        if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
            PQntuples(hResult) > 0)
        {
            char *pszVer = PQgetvalue(hResult, 0, 0);

            CPLDebug("PG", "PostGIS version string : '%s'", pszVer);

            OGRPGDecodeVersionString(&sPostGISVersion, pszVer);
        }
        OGRPGClearResult(hResult);
    }

    m_bHasGeometryColumns =
        OGRPG_Check_Table_Exists(hPGConn, "geometry_columns");
    m_bHasSpatialRefSys = OGRPG_Check_Table_Exists(hPGConn, "spatial_ref_sys");

    /* -------------------------------------------------------------------- */
    /*      Find out "unknown SRID" value                                   */
    /* -------------------------------------------------------------------- */

    if (sPostGISVersion.nMajor >= 2)
    {
        hResult =
            OGRPG_PQexec(hPGConn, "SELECT ST_Srid('POINT EMPTY'::GEOMETRY)");

        if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
            PQntuples(hResult) > 0)
        {
            nUndefinedSRID = atoi(PQgetvalue(hResult, 0, 0));
        }

        OGRPGClearResult(hResult);
    }
    else
        nUndefinedSRID = -1;

    GetCurrentSchema();

    bListAllTables = CPLTestBool(
        CSLFetchNameValueDef(papszOpenOptions, "LIST_ALL_TABLES",
                             CPLGetConfigOption("PG_LIST_ALL_TABLES", "NO")));

    m_bSkipViews = CPLTestBool(
        CSLFetchNameValueDef(papszOpenOptions, "SKIP_VIEWS",
                             CPLGetConfigOption("PG_SKIP_VIEWS", "NO")));

    return TRUE;
}

/************************************************************************/
/*                            LoadTables()                              */
/************************************************************************/

void OGRPGDataSource::LoadTables()
{
    if (bHasLoadTables)
        return;
    bHasLoadTables = TRUE;

    PGTableEntry **papsTables = nullptr;
    int nTableCount = 0;
    CPLHashSet *hSetTables = nullptr;
    std::set<CPLString> osRegisteredLayers;

    for (int i = 0; i < nLayers; i++)
    {
        osRegisteredLayers.insert(papoLayers[i]->GetName());
    }

    if (pszForcedTables)
    {
        char **papszTableList = CSLTokenizeString2(pszForcedTables, ",", 0);

        for (int i = 0; i < CSLCount(papszTableList); i++)
        {
            // Get schema and table name
            char **papszQualifiedParts =
                CSLTokenizeString2(papszTableList[i], ".", 0);
            int nParts = CSLCount(papszQualifiedParts);

            if (nParts == 1 || nParts == 2)
            {
                /* Find the geometry column name if specified */
                char *pszGeomColumnName = nullptr;
                char *pos = strchr(
                    papszQualifiedParts[CSLCount(papszQualifiedParts) - 1],
                    '(');
                if (pos != nullptr)
                {
                    *pos = '\0';
                    pszGeomColumnName = pos + 1;
                    int len = static_cast<int>(strlen(pszGeomColumnName));
                    if (len > 0)
                        pszGeomColumnName[len - 1] = '\0';
                }

                papsTables = static_cast<PGTableEntry **>(CPLRealloc(
                    papsTables, sizeof(PGTableEntry *) * (nTableCount + 1)));
                papsTables[nTableCount] = static_cast<PGTableEntry *>(
                    CPLCalloc(1, sizeof(PGTableEntry)));
                if (pszGeomColumnName)
                    OGRPGTableEntryAddGeomColumn(papsTables[nTableCount],
                                                 pszGeomColumnName);

                if (nParts == 2)
                {
                    papsTables[nTableCount]->pszSchemaName =
                        CPLStrdup(papszQualifiedParts[0]);
                    papsTables[nTableCount]->pszTableName =
                        CPLStrdup(papszQualifiedParts[1]);
                }
                else
                {
                    papsTables[nTableCount]->pszSchemaName =
                        CPLStrdup(osActiveSchema.c_str());
                    papsTables[nTableCount]->pszTableName =
                        CPLStrdup(papszQualifiedParts[0]);
                }
                nTableCount++;
            }

            CSLDestroy(papszQualifiedParts);
        }

        CSLDestroy(papszTableList);
    }

    /* -------------------------------------------------------------------- */
    /*      Get a list of available tables if they have not been            */
    /*      specified through the TABLES connection string param           */
    /* -------------------------------------------------------------------- */
    const char *pszAllowedRelations = m_bSkipViews ? "'r'" : "'r','v','m','f'";

    hSetTables = CPLHashSetNew(OGRPGHashTableEntry, OGRPGEqualTableEntry,
                               OGRPGFreeTableEntry);

    if (nTableCount == 0 && bHavePostGIS && sPostGISVersion.nMajor >= 2 &&
        !bListAllTables &&
        /* Config option mostly for comparison/debugging/etc... */
        CPLTestBool(CPLGetConfigOption("PG_USE_POSTGIS2_OPTIM", "YES")))
    {
        /* --------------------------------------------------------------------
         */
        /*      With PostGIS 2.0, the geometry_columns and geography_columns */
        /*      are views, based on the catalog system, that can be slow to */
        /*      query, so query directly the catalog system. */
        /*      See http://trac.osgeo.org/postgis/ticket/3092 */
        /* --------------------------------------------------------------------
         */
        CPLString osCommand;
        const char *pszConstraintDef = sPostgreSQLVersion.nMajor >= 12
                                           ? "pg_get_constraintdef(s.oid)"
                                           : "s.consrc";
        osCommand.Printf(
            "SELECT c.relname, n.nspname, c.relkind, a.attname, t.typname, "
            "postgis_typmod_dims(a.atttypmod) dim, "
            "postgis_typmod_srid(a.atttypmod) srid, "
            "postgis_typmod_type(a.atttypmod)::text geomtyp, "
            "array_agg(%s)::text att_constraints, a.attnotnull, "
            "d.description "
            "FROM pg_class c JOIN pg_attribute a ON a.attrelid=c.oid "
            "JOIN pg_namespace n ON c.relnamespace = n.oid "
            "AND c.relkind in (%s) AND NOT ( n.nspname = 'public' AND "
            "c.relname = 'raster_columns' ) "
            "JOIN pg_type t ON a.atttypid = t.oid AND (t.typname = "
            "'geometry'::name OR t.typname = 'geography'::name) "
            "LEFT JOIN pg_constraint s ON s.connamespace = n.oid AND "
            "s.conrelid = c.oid "
            "AND a.attnum = ANY (s.conkey) "
            "AND (%s LIKE '%%geometrytype(%% = %%' OR %s LIKE '%%ndims(%% = "
            "%%' OR %s LIKE '%%srid(%% = %%') "
            "LEFT JOIN pg_description d ON d.objoid = c.oid AND d.classoid = "
            "'pg_class'::regclass::oid AND d.objsubid = 0 "
            "GROUP BY c.relname, n.nspname, c.relkind, a.attname, t.typname, "
            "dim, srid, geomtyp, a.attnotnull, c.oid, a.attnum, d.description "
            "ORDER BY c.oid, a.attnum",
            pszConstraintDef, pszAllowedRelations, pszConstraintDef,
            pszConstraintDef, pszConstraintDef);
        PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

        if (!hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK)
        {
            OGRPGClearResult(hResult);

            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     PQerrorMessage(hPGConn));
            goto end;
        }
        /* --------------------------------------------------------------------
         */
        /*      Parse the returned table list */
        /* --------------------------------------------------------------------
         */
        for (int iRecord = 0; iRecord < PQntuples(hResult); iRecord++)
        {
            const char *pszTable = PQgetvalue(hResult, iRecord, 0);
            const char *pszSchemaName = PQgetvalue(hResult, iRecord, 1);
            const char *pszGeomColumnName = PQgetvalue(hResult, iRecord, 3);
            const char *pszGeomOrGeography = PQgetvalue(hResult, iRecord, 4);
            const char *pszDim = PQgetvalue(hResult, iRecord, 5);
            const char *pszSRID = PQgetvalue(hResult, iRecord, 6);
            const char *pszGeomType = PQgetvalue(hResult, iRecord, 7);
            const char *pszConstraint = PQgetvalue(hResult, iRecord, 8);
            const char *pszNotNull = PQgetvalue(hResult, iRecord, 9);
            const char *pszDescription = PQgetvalue(hResult, iRecord, 10);
            /*const char *pszRelkind = PQgetvalue(hResult, iRecord, 2);
            CPLDebug("PG", "%s %s %s %s %s %s %s %s %s %s",
                     pszTable, pszSchemaName, pszRelkind,
                     pszGeomColumnName, pszGeomOrGeography, pszDim,
                     pszSRID, pszGeomType, pszConstraint, pszNotNull);*/

            int bNullable = EQUAL(pszNotNull, "f");

            PostgisType ePostgisType = GEOM_TYPE_UNKNOWN;
            if (EQUAL(pszGeomOrGeography, "geometry"))
                ePostgisType = GEOM_TYPE_GEOMETRY;
            else if (EQUAL(pszGeomOrGeography, "geography"))
                ePostgisType = GEOM_TYPE_GEOGRAPHY;

            int nGeomCoordDimension = atoi(pszDim);
            bool bHasM = pszGeomType[strlen(pszGeomType) - 1] == 'M';
            int nSRID = atoi(pszSRID);

            /* Analyze constraints that might override geometrytype, */
            /* coordinate dimension and SRID */
            CPLString osConstraint(pszConstraint);
            osConstraint = osConstraint.tolower();
            pszConstraint = osConstraint.c_str();
            const char *pszNeedle = strstr(pszConstraint, "geometrytype(");
            CPLString osGeometryType;
            if (pszNeedle)
            {
                pszNeedle = strchr(pszNeedle, '\'');
                if (pszNeedle)
                {
                    pszNeedle++;
                    const char *pszEnd = strchr(pszNeedle, '\'');
                    if (pszEnd)
                    {
                        osGeometryType = pszNeedle;
                        osGeometryType.resize(pszEnd - pszNeedle);
                        pszGeomType = osGeometryType.c_str();
                        bHasM = pszGeomType[strlen(pszGeomType) - 1] == 'M';
                    }
                }
            }

            pszNeedle = strstr(pszConstraint, "srid(");
            if (pszNeedle)
            {
                pszNeedle = strchr(pszNeedle, '=');
                if (pszNeedle)
                {
                    pszNeedle++;
                    nSRID = atoi(pszNeedle);
                }
            }

            pszNeedle = strstr(pszConstraint, "ndims(");
            if (pszNeedle)
            {
                pszNeedle = strchr(pszNeedle, '=');
                if (pszNeedle)
                {
                    pszNeedle++;
                    nGeomCoordDimension = atoi(pszNeedle);
                }
            }

            int GeomTypeFlags = 0;
            if (nGeomCoordDimension == 3)
            {
                if (bHasM)
                    GeomTypeFlags |= OGRGeometry::OGR_G_MEASURED;
                else
                    GeomTypeFlags |= OGRGeometry::OGR_G_3D;
            }
            else if (nGeomCoordDimension == 4)
            {
                GeomTypeFlags |=
                    OGRGeometry::OGR_G_3D | OGRGeometry::OGR_G_MEASURED;
            }

            papsTables = static_cast<PGTableEntry **>(CPLRealloc(
                papsTables, sizeof(PGTableEntry *) * (nTableCount + 1)));
            papsTables[nTableCount] =
                static_cast<PGTableEntry *>(CPLCalloc(1, sizeof(PGTableEntry)));
            papsTables[nTableCount]->pszTableName = CPLStrdup(pszTable);
            papsTables[nTableCount]->pszSchemaName = CPLStrdup(pszSchemaName);
            papsTables[nTableCount]->pszDescription =
                CPLStrdup(pszDescription ? pszDescription : "");

            OGRPGTableEntryAddGeomColumn(
                papsTables[nTableCount], pszGeomColumnName, pszGeomType,
                GeomTypeFlags, nSRID, ePostgisType, bNullable);
            nTableCount++;

            PGTableEntry *psEntry =
                OGRPGFindTableEntry(hSetTables, pszTable, pszSchemaName);
            if (psEntry == nullptr)
                psEntry = OGRPGAddTableEntry(hSetTables, pszTable,
                                             pszSchemaName, pszDescription);
            OGRPGTableEntryAddGeomColumn(psEntry, pszGeomColumnName,
                                         pszGeomType, GeomTypeFlags, nSRID,
                                         ePostgisType, bNullable);
        }

        OGRPGClearResult(hResult);
    }
    else if (nTableCount == 0)
    {
        CPLString osCommand;

        /* Caution : in PostGIS case, the result has 11 columns, whereas in the
         */
        /* non-PostGIS case it has only 3 columns */
        if (bHavePostGIS && !bListAllTables)
        {
            osCommand.Printf(
                "SELECT c.relname, n.nspname, g.f_geometry_column, "
                "g.type, g.coord_dimension, g.srid, %d, a.attnotnull, "
                "d.description, c.oid as oid, a.attnum as attnum "
                "FROM pg_class c "
                "JOIN pg_namespace n ON c.relnamespace=n.oid "
                "JOIN geometry_columns g "
                "ON c.relname::TEXT = g.f_table_name::TEXT AND n.nspname = "
                "g.f_table_schema "
                "JOIN pg_attribute a "
                "ON a.attname = g.f_geometry_column AND a.attrelid = c.oid "
                "LEFT JOIN pg_description d "
                "ON d.objoid = c.oid AND d.classoid = "
                "'pg_class'::regclass::oid AND d.objsubid = 0 "
                "WHERE c.relkind in (%s) ",
                GEOM_TYPE_GEOMETRY, pszAllowedRelations);

            if (bHaveGeography)
                osCommand += CPLString().Printf(
                    "UNION SELECT c.relname, n.nspname, "
                    "g.f_geography_column, "
                    "g.type, g.coord_dimension, g.srid, %d, a.attnotnull, "
                    "d.description, c.oid as oid, a.attnum as attnum "
                    "FROM pg_class c "
                    "JOIN pg_namespace n ON c.relnamespace=n.oid "
                    "JOIN geography_columns g "
                    "ON c.relname::TEXT = g.f_table_name::TEXT AND n.nspname = "
                    "g.f_table_schema "
                    "JOIN pg_attribute a "
                    "ON a.attname = g.f_geography_column AND a.attrelid = "
                    "c.oid "
                    "LEFT JOIN pg_description d "
                    "ON d.objoid = c.oid AND d.classoid = "
                    "'pg_class'::regclass::oid AND d.objsubid = 0 "
                    "WHERE c.relkind in (%s)",
                    GEOM_TYPE_GEOGRAPHY, pszAllowedRelations);
            osCommand += " ORDER BY oid, attnum";
        }
        else
            osCommand.Printf("SELECT c.relname, n.nspname, d.description FROM "
                             "pg_class c "
                             "JOIN pg_namespace n ON c.relnamespace=n.oid "
                             "LEFT JOIN pg_description d "
                             "ON d.objoid = c.oid AND d.classoid = "
                             "'pg_class'::regclass::oid AND d.objsubid = 0 "
                             "WHERE (c.relkind in (%s) AND "
                             "c.relname !~ '^pg_')",
                             pszAllowedRelations);

        PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

        if (!hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK)
        {
            OGRPGClearResult(hResult);

            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     PQerrorMessage(hPGConn));
            goto end;
        }

        /* --------------------------------------------------------------------
         */
        /*      Parse the returned table list */
        /* --------------------------------------------------------------------
         */
        for (int iRecord = 0; iRecord < PQntuples(hResult); iRecord++)
        {
            const char *pszTable = PQgetvalue(hResult, iRecord, 0);
            const char *pszSchemaName = PQgetvalue(hResult, iRecord, 1);
            const char *pszGeomColumnName = nullptr;
            const char *pszGeomType = nullptr;
            const char *pszDescription = nullptr;
            int nGeomCoordDimension = 0;
            bool bHasM = false;
            int nSRID = 0;
            int bNullable = TRUE;
            PostgisType ePostgisType = GEOM_TYPE_UNKNOWN;
            if (bHavePostGIS && !bListAllTables)
            {
                pszGeomColumnName = PQgetvalue(hResult, iRecord, 2);
                pszGeomType = PQgetvalue(hResult, iRecord, 3);
                bHasM = pszGeomType[strlen(pszGeomType) - 1] == 'M';
                nGeomCoordDimension = atoi(PQgetvalue(hResult, iRecord, 4));
                nSRID = atoi(PQgetvalue(hResult, iRecord, 5));
                ePostgisType = static_cast<PostgisType>(
                    atoi(PQgetvalue(hResult, iRecord, 6)));
                bNullable = EQUAL(PQgetvalue(hResult, iRecord, 7), "f");
                pszDescription = PQgetvalue(hResult, iRecord, 8);
            }
            else
            {
                pszDescription = PQgetvalue(hResult, iRecord, 2);
            }

            if (EQUAL(pszTable, "spatial_ref_sys") ||
                EQUAL(pszTable, "geometry_columns") ||
                EQUAL(pszTable, "geography_columns"))
                continue;

            if (EQUAL(pszSchemaName, "information_schema") ||
                EQUAL(pszSchemaName, "ogr_system_tables"))
                continue;

            int GeomTypeFlags = 0;
            if (nGeomCoordDimension == 3)
            {
                if (bHasM)
                    GeomTypeFlags |= OGRGeometry::OGR_G_MEASURED;
                else
                    GeomTypeFlags |= OGRGeometry::OGR_G_3D;
            }
            else if (nGeomCoordDimension == 4)
            {
                GeomTypeFlags |=
                    OGRGeometry::OGR_G_3D | OGRGeometry::OGR_G_MEASURED;
            }

            papsTables = static_cast<PGTableEntry **>(CPLRealloc(
                papsTables, sizeof(PGTableEntry *) * (nTableCount + 1)));
            papsTables[nTableCount] =
                static_cast<PGTableEntry *>(CPLCalloc(1, sizeof(PGTableEntry)));
            papsTables[nTableCount]->pszTableName = CPLStrdup(pszTable);
            papsTables[nTableCount]->pszSchemaName = CPLStrdup(pszSchemaName);
            papsTables[nTableCount]->pszDescription =
                CPLStrdup(pszDescription ? pszDescription : "");
            if (pszGeomColumnName)
                OGRPGTableEntryAddGeomColumn(
                    papsTables[nTableCount], pszGeomColumnName, pszGeomType,
                    GeomTypeFlags, nSRID, ePostgisType, bNullable);
            nTableCount++;

            PGTableEntry *psEntry =
                OGRPGFindTableEntry(hSetTables, pszTable, pszSchemaName);
            if (psEntry == nullptr)
                psEntry = OGRPGAddTableEntry(hSetTables, pszTable,
                                             pszSchemaName, pszDescription);
            if (pszGeomColumnName)
                OGRPGTableEntryAddGeomColumn(psEntry, pszGeomColumnName,
                                             pszGeomType, GeomTypeFlags, nSRID,
                                             ePostgisType, bNullable);
        }

        /* --------------------------------------------------------------------
         */
        /*      Cleanup */
        /* --------------------------------------------------------------------
         */
        OGRPGClearResult(hResult);
    }

    /* -------------------------------------------------------------------- */
    /*      Register the available tables.                                  */
    /* -------------------------------------------------------------------- */
    for (int iRecord = 0; iRecord < nTableCount; iRecord++)
    {
        const PGTableEntry *psEntry = static_cast<PGTableEntry *>(
            CPLHashSetLookup(hSetTables, papsTables[iRecord]));

        /* If SCHEMAS= is specified, only take into account tables inside */
        /* one of the specified schemas */
        if (papszSchemaList != nullptr &&
            CSLFindString(papszSchemaList,
                          papsTables[iRecord]->pszSchemaName) == -1)
        {
            continue;
        }

        CPLString osDefnName;

        if (papsTables[iRecord]->pszSchemaName &&
            osCurrentSchema != papsTables[iRecord]->pszSchemaName)
        {
            osDefnName.Printf("%s.%s", papsTables[iRecord]->pszSchemaName,
                              papsTables[iRecord]->pszTableName);
        }
        else
        {
            // no prefix for current_schema in layer name, for backwards
            // compatibility
            osDefnName = papsTables[iRecord]->pszTableName;
        }
        if (osRegisteredLayers.find(osDefnName) != osRegisteredLayers.end())
            continue;
        osRegisteredLayers.insert(osDefnName);

        OGRPGTableLayer *poLayer = OpenTable(
            osCurrentSchema, papsTables[iRecord]->pszTableName,
            papsTables[iRecord]->pszSchemaName,
            papsTables[iRecord]->pszDescription, nullptr, bDSUpdate, FALSE);
        if (psEntry != nullptr)
        {
            if (psEntry->nGeomColumnCount > 0)
            {
                poLayer->SetGeometryInformation(psEntry->pasGeomColumns,
                                                psEntry->nGeomColumnCount);
            }
        }
        else
        {
            if (papsTables[iRecord]->nGeomColumnCount > 0)
            {
                poLayer->SetGeometryInformation(
                    papsTables[iRecord]->pasGeomColumns,
                    papsTables[iRecord]->nGeomColumnCount);
            }
        }
    }

end:
    if (hSetTables)
        CPLHashSetDestroy(hSetTables);

    for (int i = 0; i < nTableCount; i++)
        OGRPGFreeTableEntry(papsTables[i]);
    CPLFree(papsTables);
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

OGRPGTableLayer *OGRPGDataSource::OpenTable(CPLString &osCurrentSchemaIn,
                                            const char *pszNewName,
                                            const char *pszSchemaName,
                                            const char *pszDescription,
                                            const char *pszGeomColumnForced,
                                            int bUpdate, int bTestOpen)

{
    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    OGRPGTableLayer *poLayer =
        new OGRPGTableLayer(this, osCurrentSchemaIn, pszNewName, pszSchemaName,
                            pszDescription, pszGeomColumnForced, bUpdate);
    if (bTestOpen && !(poLayer->ReadTableDefinition()))
    {
        delete poLayer;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    papoLayers = static_cast<OGRPGTableLayer **>(
        CPLRealloc(papoLayers, sizeof(OGRPGTableLayer *) * (nLayers + 1)));
    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRPGDataSource::DeleteLayer(int iLayer)

{
    /* Force loading of all registered tables */
    GetLayerCount();
    if (iLayer < 0 || iLayer >= nLayers)
        return OGRERR_FAILURE;

    EndCopy();

    /* -------------------------------------------------------------------- */
    /*      Blow away our OGR structures related to the layer.  This is     */
    /*      pretty dangerous if anything has a reference to this layer!     */
    /* -------------------------------------------------------------------- */
    CPLString osLayerName = papoLayers[iLayer]->GetLayerDefn()->GetName();
    CPLString osTableName = papoLayers[iLayer]->GetTableName();
    CPLString osSchemaName = papoLayers[iLayer]->GetSchemaName();

    CPLDebug("PG", "DeleteLayer(%s)", osLayerName.c_str());

    delete papoLayers[iLayer];
    memmove(papoLayers + iLayer, papoLayers + iLayer + 1,
            sizeof(void *) * (nLayers - iLayer - 1));
    nLayers--;

    if (osLayerName.empty())
        return OGRERR_NONE;

    /* -------------------------------------------------------------------- */
    /*      Remove from the database.                                       */
    /* -------------------------------------------------------------------- */
    CPLString osCommand;

    SoftStartTransaction();

    osCommand.Printf("DROP TABLE %s.%s CASCADE",
                     OGRPGEscapeColumnName(osSchemaName).c_str(),
                     OGRPGEscapeColumnName(osTableName).c_str());
    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
    OGRPGClearResult(hResult);

    SoftCommitTransaction();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRPGDataSource::ICreateLayer(const char *pszLayerName,
                                        const OGRGeomFieldDefn *poGeomFieldDefn,
                                        CSLConstList papszOptions)

{
    const char *pszGeomType = nullptr;
    char *pszTableName = nullptr;
    char *pszSchemaName = nullptr;
    int GeometryTypeFlags = 0;

    auto eType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSRS =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    if (pszLayerName == nullptr)
        return nullptr;

    EndCopy();

    const bool bUTF8ToASCII =
        CPLFetchBool(papszOptions, "LAUNDER_ASCII", false);
    const bool bLaunder =
        bUTF8ToASCII || CPLFetchBool(papszOptions, "LAUNDER", true);

    const char *pszFIDColumnNameIn = CSLFetchNameValue(papszOptions, "FID");
    CPLString osFIDColumnName;
    if (pszFIDColumnNameIn == nullptr)
        osFIDColumnName = "ogc_fid";
    else
    {
        if (bLaunder)
        {
            char *pszLaunderedFid =
                OGRPGCommonLaunderName(pszFIDColumnNameIn, "PG", bUTF8ToASCII);
            osFIDColumnName += pszLaunderedFid;
            CPLFree(pszLaunderedFid);
        }
        else
            osFIDColumnName += pszFIDColumnNameIn;
    }
    CPLString osFIDColumnNameEscaped = OGRPGEscapeColumnName(osFIDColumnName);

    if (STARTS_WITH(pszLayerName, "pg"))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The layer name should not begin by 'pg' as it is a reserved "
                 "prefix");
    }

    if (OGR_GT_HasZ(eType))
        GeometryTypeFlags |= OGRGeometry::OGR_G_3D;
    if (OGR_GT_HasM(eType))
        GeometryTypeFlags |= OGRGeometry::OGR_G_MEASURED;

    int ForcedGeometryTypeFlags = -1;
    const char *pszDim = CSLFetchNameValue(papszOptions, "DIM");
    if (pszDim != nullptr)
    {
        if (EQUAL(pszDim, "XY") || EQUAL(pszDim, "2"))
        {
            GeometryTypeFlags = 0;
            ForcedGeometryTypeFlags = GeometryTypeFlags;
        }
        else if (EQUAL(pszDim, "XYZ") || EQUAL(pszDim, "3"))
        {
            GeometryTypeFlags = OGRGeometry::OGR_G_3D;
            ForcedGeometryTypeFlags = GeometryTypeFlags;
        }
        else if (EQUAL(pszDim, "XYM"))
        {
            GeometryTypeFlags = OGRGeometry::OGR_G_MEASURED;
            ForcedGeometryTypeFlags = GeometryTypeFlags;
        }
        else if (EQUAL(pszDim, "XYZM") || EQUAL(pszDim, "4"))
        {
            GeometryTypeFlags =
                OGRGeometry::OGR_G_3D | OGRGeometry::OGR_G_MEASURED;
            ForcedGeometryTypeFlags = GeometryTypeFlags;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for DIM");
        }
    }

    /* Should we turn layers with None geometry type as Unknown/GEOMETRY */
    /* so they are still recorded in geometry_columns table ? (#4012) */
    int bNoneAsUnknown = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "NONE_AS_UNKNOWN", "NO"));
    if (bNoneAsUnknown && eType == wkbNone)
        eType = wkbUnknown;

    int bExtractSchemaFromLayerName = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "EXTRACT_SCHEMA_FROM_LAYER_NAME", "YES"));

    /* Postgres Schema handling:
       Extract schema name from input layer name or passed with -lco SCHEMA.
       Set layer name to "schema.table" or to "table" if schema ==
       current_schema() Usage without schema name is backwards compatible
    */
    const char *pszDotPos = strstr(pszLayerName, ".");
    if (pszDotPos != nullptr && bExtractSchemaFromLayerName)
    {
        int length = static_cast<int>(pszDotPos - pszLayerName);
        pszSchemaName = static_cast<char *>(CPLMalloc(length + 1));
        strncpy(pszSchemaName, pszLayerName, length);
        pszSchemaName[length] = '\0';

        if (bLaunder)
            pszTableName = OGRPGCommonLaunderName(pszDotPos + 1, "PG",
                                                  bUTF8ToASCII);  // skip "."
        else
            pszTableName = CPLStrdup(
                OGRPGCommonGenerateShortEnoughIdentifier(pszDotPos + 1)
                    .c_str());  // skip "."
    }
    else
    {
        pszSchemaName = nullptr;
        if (bLaunder)
            pszTableName = OGRPGCommonLaunderName(pszLayerName, "PG",
                                                  bUTF8ToASCII);  // skip "."
        else
            pszTableName =
                CPLStrdup(OGRPGCommonGenerateShortEnoughIdentifier(pszLayerName)
                              .c_str());  // skip "."
    }

    /* -------------------------------------------------------------------- */
    /*      Set the default schema for the layers.                          */
    /* -------------------------------------------------------------------- */
    if (CSLFetchNameValue(papszOptions, "SCHEMA") != nullptr)
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup(CSLFetchNameValue(papszOptions, "SCHEMA"));
    }

    const bool bTemporary = CPLFetchBool(papszOptions, "TEMPORARY", false);
    if (bTemporary)
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup("pg_temp");
    }
    if (pszSchemaName == nullptr)
    {
        pszSchemaName = CPLStrdup(osCurrentSchema);
    }

    // Check that the schema exist. If there is a single match in a case
    // insensitive way, use it. Otherwise error out if the match is not exact.
    {
        const auto osNewSchemaName = FindSchema(pszSchemaName);
        if (!osNewSchemaName.has_value())
        {
            CPLFree(pszTableName);
            CPLFree(pszSchemaName);
            return nullptr;
        }
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup(osNewSchemaName->c_str());
    }

    /* -------------------------------------------------------------------- */
    /*      Do we already have this layer?  If so, should we blow it        */
    /*      away?                                                           */
    /* -------------------------------------------------------------------- */
    CPLString osSQLLayerName;
    if (pszSchemaName == nullptr ||
        (!osCurrentSchema.empty() &&
         EQUAL(pszSchemaName, osCurrentSchema.c_str())))
        osSQLLayerName = pszTableName;
    else
    {
        osSQLLayerName = pszSchemaName;
        osSQLLayerName += ".";
        osSQLLayerName += pszTableName;
    }

    /* GetLayerByName() can instantiate layers that would have been */
    /* 'hidden' otherwise, for example, non-spatial tables in a */
    /* PostGIS-enabled database, so this apparently useless command is */
    /* not useless. (#4012) */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GetLayerByName(osSQLLayerName);
    CPLPopErrorHandler();
    CPLErrorReset();

    /* Force loading of all registered tables */
    GetLayerCount();

    for (int iLayer = 0; iLayer < nLayers; iLayer++)
    {
        if (EQUAL(osSQLLayerName.c_str(), papoLayers[iLayer]->GetName()))
        {
            if (CSLFetchNameValue(papszOptions, "OVERWRITE") != nullptr &&
                !EQUAL(CSLFetchNameValue(papszOptions, "OVERWRITE"), "NO"))
            {
                DeleteLayer(iLayer);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Layer %s already exists, CreateLayer failed.\n"
                         "Use the layer creation option OVERWRITE=YES to "
                         "replace it.",
                         osSQLLayerName.c_str());
                CPLFree(pszTableName);
                CPLFree(pszSchemaName);
                return nullptr;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Handle the GEOM_TYPE option.                                    */
    /* -------------------------------------------------------------------- */
    pszGeomType = CSLFetchNameValue(papszOptions, "GEOM_TYPE");
    if (pszGeomType == nullptr)
    {
        if (bHavePostGIS)
            pszGeomType = "geometry";
        else
            pszGeomType = "bytea";
    }

    const char *pszGFldName = CSLFetchNameValue(papszOptions, "GEOMETRY_NAME");
    if (eType != wkbNone && EQUAL(pszGeomType, "geography"))
    {
        if (!bHaveGeography)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "GEOM_TYPE=geography is only supported in PostGIS >= 1.5.\n"
                "Creation of layer %s has failed.",
                pszLayerName);
            CPLFree(pszTableName);
            CPLFree(pszSchemaName);
            return nullptr;
        }

        if (pszGFldName == nullptr)
            pszGFldName = "the_geog";
    }
    else if (eType != wkbNone && bHavePostGIS &&
             !EQUAL(pszGeomType, "geography"))
    {
        if (pszGFldName == nullptr)
            pszGFldName = "wkb_geometry";
    }

    if (eType != wkbNone && bHavePostGIS && !EQUAL(pszGeomType, "geometry") &&
        !EQUAL(pszGeomType, "geography"))
    {
        if (bHaveGeography)
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GEOM_TYPE in PostGIS enabled databases must be "
                     "'geometry' or 'geography'.\n"
                     "Creation of layer %s with GEOM_TYPE %s has failed.",
                     pszLayerName, pszGeomType);
        else
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "GEOM_TYPE in PostGIS enabled databases must be 'geometry'.\n"
                "Creation of layer %s with GEOM_TYPE %s has failed.",
                pszLayerName, pszGeomType);

        CPLFree(pszTableName);
        CPLFree(pszSchemaName);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to get the SRS Id of this spatial reference system,         */
    /*      adding tot the srs table if needed.                             */
    /* -------------------------------------------------------------------- */
    int nSRSId = nUndefinedSRID;

    if (poSRS != nullptr)
        nSRSId = FetchSRSId(poSRS);

    if (eType != wkbNone && EQUAL(pszGeomType, "geography"))
    {
        if (poSRS != nullptr && !poSRS->IsGeographic())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "geography type only supports geographic SRS");

            CPLFree(pszTableName);
            CPLFree(pszSchemaName);
            return nullptr;
        }

        if (!(sPostGISVersion.nMajor >= 3 ||
              (sPostGISVersion.nMajor == 2 && sPostGISVersion.nMinor >= 2)) &&
            nSRSId != nUndefinedSRID && nSRSId != 4326)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "geography type in PostGIS < 2.2 only supports SRS = "
                     "EPSG:4326");

            CPLFree(pszTableName);
            CPLFree(pszSchemaName);
            return nullptr;
        }

        if (nSRSId == nUndefinedSRID)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Assuming EPSG:4326 for geographic type (but no implicit "
                     "reprojection to it will be done)");
            nSRSId = 4326;
        }
    }

    const char *pszGeometryType = OGRToOGCGeomType(eType);

    int bDeferredCreation =
        CPLTestBool(CPLGetConfigOption("OGR_PG_DEFERRED_CREATION", "YES"));
    if (!bHavePostGIS)
        bDeferredCreation =
            FALSE; /* to avoid unnecessary implementation and testing burden */

    /* -------------------------------------------------------------------- */
    /*      Create a basic table with the FID.  Also include the            */
    /*      geometry if this is not a PostGIS enabled table.                */
    /* -------------------------------------------------------------------- */
    const bool bFID64 = CPLFetchBool(papszOptions, "FID64", false);
    const char *pszSerialType = bFID64 ? "BIGSERIAL" : "SERIAL";

    CPLString osCreateTable;
    if (bTemporary)
    {
        osCreateTable.Printf("CREATE TEMPORARY TABLE %s",
                             OGRPGEscapeColumnName(pszTableName).c_str());
    }
    else
    {
        osCreateTable.Printf(
            "CREATE%s TABLE %s.%s",
            CPLFetchBool(papszOptions, "UNLOGGED", false) ? " UNLOGGED" : "",
            OGRPGEscapeColumnName(pszSchemaName).c_str(),
            OGRPGEscapeColumnName(pszTableName).c_str());
    }

    const char *suffix = nullptr;
    if ((GeometryTypeFlags & OGRGeometry::OGR_G_3D) &&
        (GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED))
        suffix = "ZM";
    else if ((GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) &&
             (EQUAL(pszGeomType, "geography") ||
              wkbFlatten(eType) != wkbUnknown))
        suffix = "M";
    else if (GeometryTypeFlags & OGRGeometry::OGR_G_3D)
        suffix = "Z";
    else
        suffix = "";

    {
        CPLString osCommand;
        if (eType != wkbNone && !bHavePostGIS)
        {
            if (pszGFldName && !EQUAL(pszGFldName, "wkb_geometry"))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "GEOMETRY_NAME=%s ignored, and set instead to "
                         "'wkb_geometry' as it is the only geometry column "
                         "name recognized for non-PostGIS enabled databases.",
                         pszGFldName);
            }
            pszGFldName = "wkb_geometry";
            osCommand.Printf("%s ( "
                             "    %s %s, "
                             "   %s %s, "
                             "   PRIMARY KEY (%s)",
                             osCreateTable.c_str(),
                             osFIDColumnNameEscaped.c_str(), pszSerialType,
                             pszGFldName, pszGeomType,
                             osFIDColumnNameEscaped.c_str());
        }
        else if (!bDeferredCreation && eType != wkbNone &&
                 EQUAL(pszGeomType, "geography"))
        {
            osCommand.Printf(
                "%s ( %s %s, %s geography(%s%s%s), PRIMARY KEY (%s)",
                osCreateTable.c_str(), osFIDColumnNameEscaped.c_str(),
                pszSerialType, OGRPGEscapeColumnName(pszGFldName).c_str(),
                pszGeometryType, suffix,
                nSRSId ? CPLSPrintf(",%d", nSRSId) : "",
                osFIDColumnNameEscaped.c_str());
        }
        else if (!bDeferredCreation && eType != wkbNone &&
                 !EQUAL(pszGeomType, "geography") &&
                 sPostGISVersion.nMajor >= 2)
        {
            osCommand.Printf(
                "%s ( %s %s, %s geometry(%s%s%s), PRIMARY KEY (%s)",
                osCreateTable.c_str(), osFIDColumnNameEscaped.c_str(),
                pszSerialType, OGRPGEscapeColumnName(pszGFldName).c_str(),
                pszGeometryType, suffix,
                nSRSId ? CPLSPrintf(",%d", nSRSId) : "",
                osFIDColumnNameEscaped.c_str());
        }
        else
        {
            osCommand.Printf("%s ( %s %s, PRIMARY KEY (%s)",
                             osCreateTable.c_str(),
                             osFIDColumnNameEscaped.c_str(), pszSerialType,
                             osFIDColumnNameEscaped.c_str());
        }
        osCreateTable = std::move(osCommand);
    }

    const char *pszSI =
        CSLFetchNameValueDef(papszOptions, "SPATIAL_INDEX", "GIST");
    bool bCreateSpatialIndex =
        (EQUAL(pszSI, "GIST") || EQUAL(pszSI, "SPGIST") ||
         EQUAL(pszSI, "BRIN") || EQUAL(pszSI, "YES") || EQUAL(pszSI, "ON") ||
         EQUAL(pszSI, "TRUE"));
    if (!bCreateSpatialIndex && !EQUAL(pszSI, "NO") && !EQUAL(pszSI, "OFF") &&
        !EQUAL(pszSI, "FALSE") && !EQUAL(pszSI, "NONE"))
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "SPATIAL_INDEX=%s not supported", pszSI);
    }
    const char *pszSpatialIndexType = EQUAL(pszSI, "SPGIST") ? "SPGIST"
                                      : EQUAL(pszSI, "BRIN") ? "BRIN"
                                                             : "GIST";
    if (eType != wkbNone && bCreateSpatialIndex &&
        CPLFetchBool(papszOptions, "UNLOGGED", false) &&
        !(sPostgreSQLVersion.nMajor > 9 ||
          (sPostgreSQLVersion.nMajor == 9 && sPostgreSQLVersion.nMinor >= 3)))
    {
        CPLError(
            CE_Warning, CPLE_NotSupported,
            "GiST index only supported since Postgres 9.3 on unlogged table");
        bCreateSpatialIndex = false;
    }

    if (!bDeferredCreation)
    {
        SoftStartTransaction();

        CPLString osCommand = osCreateTable;
        osCommand += " )";

        PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
        if (PQresultStatus(hResult) != PGRES_COMMAND_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s\n%s", osCommand.c_str(),
                     PQerrorMessage(hPGConn));
            CPLFree(pszTableName);
            CPLFree(pszSchemaName);

            OGRPGClearResult(hResult);

            SoftRollbackTransaction();
            return nullptr;
        }

        OGRPGClearResult(hResult);

        if (eType != wkbNone && bHavePostGIS && bCreateSpatialIndex)
        {
            // Create the spatial index.
            // We're doing this before we add geometry and record to the
            // table, so this may not be exactly the best way to do it.
            const std::string osIndexName(OGRPGCommonGenerateSpatialIndexName(
                pszTableName, pszGFldName, 0));

            osCommand.Printf("CREATE INDEX %s ON %s.%s USING %s (%s)",
                             OGRPGEscapeColumnName(osIndexName.c_str()).c_str(),
                             OGRPGEscapeColumnName(pszSchemaName).c_str(),
                             OGRPGEscapeColumnName(pszTableName).c_str(),
                             pszSpatialIndexType,
                             OGRPGEscapeColumnName(pszGFldName).c_str());

            hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

            if (!hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "'%s' failed for layer %s, index creation has failed.",
                         osCommand.c_str(), pszLayerName);

                CPLFree(pszTableName);
                CPLFree(pszSchemaName);

                OGRPGClearResult(hResult);

                SoftRollbackTransaction();

                return nullptr;
            }
            OGRPGClearResult(hResult);
        }

        /* --------------------------------------------------------------------
         */
        /*      Complete, and commit the transaction. */
        /* --------------------------------------------------------------------
         */
        SoftCommitTransaction();
    }

    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    OGRPGTableLayer *poLayer = new OGRPGTableLayer(
        this, osCurrentSchema, pszTableName, pszSchemaName, "", nullptr, TRUE);
    poLayer->SetTableDefinition(osFIDColumnName, pszGFldName, eType,
                                pszGeomType, nSRSId, GeometryTypeFlags);
    poLayer->SetLaunderFlag(bLaunder);
    poLayer->SetUTF8ToASCIIFlag(bUTF8ToASCII);
    poLayer->SetPrecisionFlag(CPLFetchBool(papszOptions, "PRECISION", true));
    // poLayer->SetForcedSRSId(nForcedSRSId);
    poLayer->SetForcedGeometryTypeFlags(ForcedGeometryTypeFlags);
    poLayer->SetCreateSpatialIndex(bCreateSpatialIndex, pszSpatialIndexType);
    poLayer->SetDeferredCreation(bDeferredCreation, osCreateTable);

    const char *pszDescription = CSLFetchNameValue(papszOptions, "DESCRIPTION");
    if (pszDescription != nullptr)
        poLayer->SetForcedDescription(pszDescription);

    /* HSTORE_COLUMNS existed at a time during GDAL 1.10dev */
    const char *pszHSTOREColumns =
        CSLFetchNameValue(papszOptions, "HSTORE_COLUMNS");
    if (pszHSTOREColumns != nullptr)
        CPLError(CE_Warning, CPLE_AppDefined,
                 "HSTORE_COLUMNS not recognized. Use COLUMN_TYPES instead.");

    const char *pszOverrideColumnTypes =
        CSLFetchNameValue(papszOptions, "COLUMN_TYPES");
    poLayer->SetOverrideColumnTypes(pszOverrideColumnTypes);

    poLayer->AllowAutoFIDOnCreateViaCopy();
    if (CPLTestBool(CPLGetConfigOption("PG_USE_COPY", "YES")))
        poLayer->SetUseCopy();

    if (bFID64)
        poLayer->SetMetadataItem(OLMD_FID64, "YES");

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    papoLayers = static_cast<OGRPGTableLayer **>(
        CPLRealloc(papoLayers, sizeof(OGRPGTableLayer *) * (nLayers + 1)));

    papoLayers[nLayers++] = poLayer;

    CPLFree(pszTableName);
    CPLFree(pszSchemaName);

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer) || EQUAL(pszCap, ODsCDeleteLayer) ||
        EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCCurveGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCTransactions))
        return TRUE;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCRandomLayerWrite))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRPGDataSource::GetLayerCount()
{
    LoadTables();
    return nLayers;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPGDataSource::GetLayer(int iLayer)

{
    /* Force loading of all registered tables */
    if (iLayer < 0 || iLayer >= GetLayerCount())
        return nullptr;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                             FindSchema()                             */
/************************************************************************/

// Check that the schema exists. If there is a single match in a case
// insensitive way, use it. Otherwise error out if the match is not exact.
// Return the schema name with its exact case from pg_catalog, or an empty
// string if an error occurs.
std::optional<std::string>
OGRPGDataSource::FindSchema(const char *pszSchemaNameIn)
{
    if (strcmp(pszSchemaNameIn, "public") == 0 ||
        strcmp(pszSchemaNameIn, "pg_temp") == 0)
    {
        return pszSchemaNameIn;
    }

    EndCopy();

    std::string osSchemaName;
    std::string osCommand(
        "SELECT nspname FROM pg_catalog.pg_namespace WHERE nspname ILIKE ");
    osCommand += OGRPGEscapeString(hPGConn, pszSchemaNameIn);
    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
    if (hResult && PQntuples(hResult) == 1)
    {
        osSchemaName = PQgetvalue(hResult, 0, 0);
    }
    else if (hResult)
    {
        const int nTuples = PQntuples(hResult);
        if (nTuples == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Schema \"%s\" does not exist.", pszSchemaNameIn);
            return {};
        }
        for (int i = 0; i < nTuples; ++i)
        {
            if (strcmp(PQgetvalue(hResult, i, 0), pszSchemaNameIn) == 0)
            {
                osSchemaName = pszSchemaNameIn;
                break;
            }
        }
        if (osSchemaName.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Several schemas exist whose name matches \"%s\", but "
                     "not with that case. "
                     "Please specify the schema name with the exact case.",
                     pszSchemaNameIn);
            return {};
        }
    }
    OGRPGClearResult(hResult);

    return osSchemaName;
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGRPGDataSource::GetLayerByName(const char *pszNameIn)

{
    char *pszTableName = nullptr;
    char *pszGeomColumnName = nullptr;
    char *pszSchemaName = nullptr;

    if (!pszNameIn)
        return nullptr;

    /* first a case sensitive check */
    /* do NOT force loading of all registered tables */
    for (int i = 0; i < nLayers; i++)
    {
        OGRPGTableLayer *poLayer = papoLayers[i];

        if (strcmp(pszNameIn, poLayer->GetName()) == 0)
        {
            return poLayer;
        }
    }

    /* then case insensitive */
    for (int i = 0; i < nLayers; i++)
    {
        OGRPGTableLayer *poLayer = papoLayers[i];

        if (EQUAL(pszNameIn, poLayer->GetName()))
        {
            return poLayer;
        }
    }

    char *pszNameWithoutBracket = CPLStrdup(pszNameIn);
    char *pos = strchr(pszNameWithoutBracket, '(');
    if (pos != nullptr)
    {
        *pos = '\0';
        pszGeomColumnName = CPLStrdup(pos + 1);
        int len = static_cast<int>(strlen(pszGeomColumnName));
        if (len > 0)
            pszGeomColumnName[len - 1] = '\0';
    }

    pos = strchr(pszNameWithoutBracket, '.');
    if (pos != nullptr)
    {
        *pos = '\0';
        const auto osSchemaName = FindSchema(pszNameWithoutBracket);
        if (!osSchemaName.has_value())
        {
            CPLFree(pszNameWithoutBracket);
            CPLFree(pszGeomColumnName);
            return nullptr;
        }
        pszSchemaName = CPLStrdup(osSchemaName->c_str());
        pszTableName = CPLStrdup(pos + 1);
    }
    else
    {
        pszTableName = CPLStrdup(pszNameWithoutBracket);
    }

    if (strlen(pszTableName) > OGR_PG_NAMEDATALEN - 1)
        pszTableName[OGR_PG_NAMEDATALEN - 1] = 0;

    CPLFree(pszNameWithoutBracket);
    pszNameWithoutBracket = nullptr;

    OGRPGTableLayer *poLayer = nullptr;

    if (pszSchemaName != nullptr && osCurrentSchema == pszSchemaName &&
        pszGeomColumnName == nullptr)
    {
        poLayer =
            cpl::down_cast<OGRPGTableLayer *>(GetLayerByName(pszTableName));
    }
    else
    {
        EndCopy();

        CPLString osTableName(pszTableName);
        CPLString osTableNameLower(pszTableName);
        osTableNameLower.tolower();
        if (osTableName != osTableNameLower)
            CPLPushErrorHandler(CPLQuietErrorHandler);
        poLayer = OpenTable(osCurrentSchema, pszTableName, pszSchemaName,
                            nullptr, pszGeomColumnName, bDSUpdate, TRUE);
        if (osTableName != osTableNameLower)
            CPLPopErrorHandler();
        if (poLayer == nullptr && osTableName != osTableNameLower)
        {
            poLayer =
                OpenTable(osCurrentSchema, osTableNameLower, pszSchemaName,
                          nullptr, pszGeomColumnName, bDSUpdate, TRUE);
        }
    }

    CPLFree(pszTableName);
    CPLFree(pszSchemaName);
    CPLFree(pszGeomColumnName);

    return poLayer;
}

/************************************************************************/
/*                        OGRPGNoticeProcessor()                        */
/************************************************************************/

static void OGRPGNoticeProcessor(CPL_UNUSED void *arg, const char *pszMessage)
{
    CPLDebug("OGR_PG_NOTICE", "%s", pszMessage);
}

/************************************************************************/
/*                      InitializeMetadataTables()                      */
/*                                                                      */
/*      Create the metadata tables (SPATIAL_REF_SYS and                 */
/*      GEOMETRY_COLUMNS).                                              */
/************************************************************************/

OGRErr OGRPGDataSource::InitializeMetadataTables()

{
    // implement later.
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

const OGRSpatialReference *OGRPGDataSource::FetchSRS(int nId)

{
    if (nId < 0 || !m_bHasSpatialRefSys)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      First, we look through our SRID cache, is it there?             */
    /* -------------------------------------------------------------------- */
    auto oIter = m_oSRSCache.find(nId);
    if (oIter != m_oSRSCache.end())
    {
        return oIter->second.get();
    }

    EndCopy();

    /* -------------------------------------------------------------------- */
    /*      Try looking up in spatial_ref_sys table.                        */
    /* -------------------------------------------------------------------- */
    CPLString osCommand;
    std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> poSRS;

    osCommand.Printf("SELECT srtext, auth_name, auth_srid FROM spatial_ref_sys "
                     "WHERE srid = %d",
                     nId);
    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

    if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
        PQntuples(hResult) == 1)
    {
        const char *pszWKT = PQgetvalue(hResult, 0, 0);
        const char *pszAuthName = PQgetvalue(hResult, 0, 1);
        const char *pszAuthSRID = PQgetvalue(hResult, 0, 2);
        poSRS.reset(new OGRSpatialReference());
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        // Try to import first from EPSG code, and then from WKT
        if (pszAuthName && pszAuthSRID && EQUAL(pszAuthName, "EPSG") &&
            atoi(pszAuthSRID) == nId &&
            poSRS->importFromEPSG(nId) == OGRERR_NONE)
        {
            // do nothing
        }
        else if (poSRS->importFromWkt(pszWKT) != OGRERR_NONE)
        {
            poSRS.reset();
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not fetch SRS: %s",
                 PQerrorMessage(hPGConn));
    }

    OGRPGClearResult(hResult);

    if (poSRS)
        poSRS->StripTOWGS84IfKnownDatumAndAllowed();

    /* -------------------------------------------------------------------- */
    /*      Add to the cache.                                               */
    /* -------------------------------------------------------------------- */
    oIter = m_oSRSCache.emplace(nId, std::move(poSRS)).first;
    return oIter->second.get();
}

/************************************************************************/
/*                             FetchSRSId()                             */
/*                                                                      */
/*      Fetch the id corresponding to an SRS, and if not found, add     */
/*      it to the table.                                                */
/************************************************************************/

int OGRPGDataSource::FetchSRSId(const OGRSpatialReference *poSRS)

{
    if (poSRS == nullptr || !m_bHasSpatialRefSys)
        return nUndefinedSRID;

    OGRSpatialReference oSRS(*poSRS);
    // cppcheck-suppress uselessAssignmentPtrArg
    poSRS = nullptr;

    const char *pszAuthorityName = oSRS.GetAuthorityName(nullptr);

    if (pszAuthorityName == nullptr || strlen(pszAuthorityName) == 0)
    {
        /* --------------------------------------------------------------------
         */
        /*      Try to identify an EPSG code */
        /* --------------------------------------------------------------------
         */
        oSRS.AutoIdentifyEPSG();

        pszAuthorityName = oSRS.GetAuthorityName(nullptr);
        if (pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG"))
        {
            const char *pszAuthorityCode = oSRS.GetAuthorityCode(nullptr);
            if (pszAuthorityCode != nullptr && strlen(pszAuthorityCode) > 0)
            {
                /* Import 'clean' SRS */
                oSRS.importFromEPSG(atoi(pszAuthorityCode));

                pszAuthorityName = oSRS.GetAuthorityName(nullptr);
            }
        }
    }
    /* -------------------------------------------------------------------- */
    /*      Check whether the authority name/code is already mapped to a    */
    /*      SRS ID.                                                         */
    /* -------------------------------------------------------------------- */
    CPLString osCommand;
    int nAuthorityCode = 0;
    if (pszAuthorityName != nullptr)
    {
        /* Check that the authority code is integral */
        nAuthorityCode = atoi(oSRS.GetAuthorityCode(nullptr));
        if (nAuthorityCode > 0)
        {
            osCommand.Printf("SELECT srid FROM spatial_ref_sys WHERE "
                             "auth_name = '%s' AND auth_srid = %d",
                             pszAuthorityName, nAuthorityCode);
            PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

            if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
                PQntuples(hResult) > 0)
            {
                int nSRSId = atoi(PQgetvalue(hResult, 0, 0));

                OGRPGClearResult(hResult);

                return nSRSId;
            }

            OGRPGClearResult(hResult);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Translate SRS to WKT.                                           */
    /* -------------------------------------------------------------------- */
    char *pszWKT = nullptr;
    if (oSRS.exportToWkt(&pszWKT) != OGRERR_NONE)
    {
        CPLFree(pszWKT);
        return nUndefinedSRID;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to find in the existing table.                              */
    /* -------------------------------------------------------------------- */
    CPLString osWKT =
        OGRPGEscapeString(hPGConn, pszWKT, -1, "spatial_ref_sys", "srtext");
    osCommand.Printf("SELECT srid FROM spatial_ref_sys WHERE srtext = %s",
                     osWKT.c_str());
    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
    CPLFree(pszWKT);   // CM:  Added to prevent mem leaks
    pszWKT = nullptr;  // CM:  Added

    /* -------------------------------------------------------------------- */
    /*      We got it!  Return it.                                          */
    /* -------------------------------------------------------------------- */
    if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
        PQntuples(hResult) > 0)
    {
        int nSRSId = atoi(PQgetvalue(hResult, 0, 0));

        OGRPGClearResult(hResult);

        return nSRSId;
    }

    /* -------------------------------------------------------------------- */
    /*      If the command actually failed, then the metadata table is      */
    /*      likely missing. Try defining it.                                */
    /* -------------------------------------------------------------------- */
    const bool bTableMissing =
        hResult == nullptr || PQresultStatus(hResult) == PGRES_NONFATAL_ERROR;

    OGRPGClearResult(hResult);

    if (bTableMissing)
    {
        if (InitializeMetadataTables() != OGRERR_NONE)
            return nUndefinedSRID;
    }

    /* -------------------------------------------------------------------- */
    /*      Get the current maximum srid in the srs table.                  */
    /* -------------------------------------------------------------------- */
    hResult = OGRPG_PQexec(hPGConn, "SELECT MAX(srid) FROM spatial_ref_sys");

    int nSRSId = 1;
    if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK)
    {
        nSRSId = atoi(PQgetvalue(hResult, 0, 0)) + 1;
        OGRPGClearResult(hResult);
    }

    /* -------------------------------------------------------------------- */
    /*      Try adding the SRS to the SRS table.                            */
    /* -------------------------------------------------------------------- */
    char *pszProj4 = nullptr;
    if (oSRS.exportToProj4(&pszProj4) != OGRERR_NONE)
    {
        CPLFree(pszProj4);
        return nUndefinedSRID;
    }

    CPLString osProj4 = OGRPGEscapeString(hPGConn, pszProj4, -1,
                                          "spatial_ref_sys", "proj4text");

    if (pszAuthorityName != nullptr && nAuthorityCode > 0)
    {
        nAuthorityCode = atoi(oSRS.GetAuthorityCode(nullptr));

        osCommand.Printf("INSERT INTO spatial_ref_sys "
                         "(srid,srtext,proj4text,auth_name,auth_srid) "
                         "VALUES (%d, %s, %s, '%s', %d)",
                         nSRSId, osWKT.c_str(), osProj4.c_str(),
                         pszAuthorityName, nAuthorityCode);
    }
    else
    {
        osCommand.Printf("INSERT INTO spatial_ref_sys (srid,srtext,proj4text) "
                         "VALUES (%d,%s,%s)",
                         nSRSId, osWKT.c_str(), osProj4.c_str());
    }

    // Free everything that was allocated.
    CPLFree(pszProj4);
    CPLFree(pszWKT);

    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
    OGRPGClearResult(hResult);

    return nSRSId;
}

/************************************************************************/
/*                         StartTransaction()                           */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRPGDataSource::StartTransaction(CPL_UNUSED int bForce)
{
    if (bUserTransactionActive)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Transaction already established");
        return OGRERR_FAILURE;
    }

    CPLAssert(!bSavePointActive);
    EndCopy();

    if (nSoftTransactionLevel == 0)
    {
        OGRErr eErr = DoTransactionCommand("BEGIN");
        if (eErr != OGRERR_NONE)
            return eErr;
    }
    else
    {
        OGRErr eErr = DoTransactionCommand("SAVEPOINT ogr_savepoint");
        if (eErr != OGRERR_NONE)
            return eErr;

        bSavePointActive = TRUE;
    }

    nSoftTransactionLevel++;
    bUserTransactionActive = true;

    /*CPLDebug("PG", "poDS=%p StartTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRPGDataSource::CommitTransaction()
{
    if (!bUserTransactionActive)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Transaction not established");
        return OGRERR_FAILURE;
    }

    /*CPLDebug("PG", "poDS=%p CommitTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    OGRErr eErr = FlushCacheWithRet(false);
    if (eErr != OGRERR_NONE)
    {
        RollbackTransaction();
        return eErr;
    }

    nSoftTransactionLevel--;
    bUserTransactionActive = false;

    if (bSavePointActive)
    {
        CPLAssert(nSoftTransactionLevel > 0);
        bSavePointActive = FALSE;

        eErr = DoTransactionCommand("RELEASE SAVEPOINT ogr_savepoint");
    }
    else
    {
        if (nSoftTransactionLevel > 0)
        {
            // This means we have cursors still in progress
            for (int i = 0; i < nLayers; i++)
                papoLayers[i]->InvalidateCursor();
            CPLAssert(nSoftTransactionLevel == 0);
        }

        eErr = DoTransactionCommand("COMMIT");
    }

    return eErr;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRPGDataSource::RollbackTransaction()
{
    if (!bUserTransactionActive)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Transaction not established");
        return OGRERR_FAILURE;
    }

    /*CPLDebug("PG", "poDS=%p RollbackTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    FlushCache(false);

    nSoftTransactionLevel--;
    bUserTransactionActive = false;

    OGRErr eErr;
    if (bSavePointActive)
    {
        CPLAssert(nSoftTransactionLevel > 0);
        bSavePointActive = FALSE;

        eErr = DoTransactionCommand("ROLLBACK TO SAVEPOINT ogr_savepoint");
    }
    else
    {
        if (nSoftTransactionLevel > 0)
        {
            // This means we have cursors still in progress
            for (int i = 0; i < nLayers; i++)
                papoLayers[i]->InvalidateCursor();
            CPLAssert(nSoftTransactionLevel == 0);
        }

        eErr = DoTransactionCommand("ROLLBACK");
    }

    return eErr;
}

/************************************************************************/
/*                        SoftStartTransaction()                        */
/*                                                                      */
/*      Create a transaction scope.  If we already have a               */
/*      transaction active this isn't a real transaction, but just      */
/*      an increment to the scope count.                                */
/************************************************************************/

OGRErr OGRPGDataSource::SoftStartTransaction()

{
    nSoftTransactionLevel++;
    /*CPLDebug("PG", "poDS=%p SoftStartTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    OGRErr eErr = OGRERR_NONE;
    if (nSoftTransactionLevel == 1)
    {
        eErr = DoTransactionCommand("BEGIN");
    }

    return eErr;
}

/************************************************************************/
/*                     SoftCommitTransaction()                          */
/*                                                                      */
/*      Commit the current transaction if we are at the outer           */
/*      scope.                                                          */
/************************************************************************/

OGRErr OGRPGDataSource::SoftCommitTransaction()

{
    EndCopy();

    /*CPLDebug("PG", "poDS=%p SoftCommitTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    if (nSoftTransactionLevel <= 0)
    {
        CPLAssert(false);
        return OGRERR_FAILURE;
    }

    OGRErr eErr = OGRERR_NONE;
    nSoftTransactionLevel--;
    if (nSoftTransactionLevel == 0)
    {
        CPLAssert(!bSavePointActive);

        eErr = DoTransactionCommand("COMMIT");
    }

    return eErr;
}

/************************************************************************/
/*                  SoftRollbackTransaction()                           */
/*                                                                      */
/*      Do a rollback of the current transaction if we are at the 1st   */
/*      level                                                           */
/************************************************************************/

OGRErr OGRPGDataSource::SoftRollbackTransaction()

{
    EndCopy();

    /*CPLDebug("PG", "poDS=%p SoftRollbackTransaction()
       nSoftTransactionLevel=%d", this, nSoftTransactionLevel);*/

    if (nSoftTransactionLevel <= 0)
    {
        CPLAssert(false);
        return OGRERR_FAILURE;
    }

    OGRErr eErr = OGRERR_NONE;
    nSoftTransactionLevel--;
    if (nSoftTransactionLevel == 0)
    {
        CPLAssert(!bSavePointActive);

        eErr = DoTransactionCommand("ROLLBACK");
    }

    return eErr;
}

/************************************************************************/
/*                        FlushSoftTransaction()                        */
/*                                                                      */
/*      Force the unwinding of any active transaction, and its          */
/*      commit. Should only be used by datasource destructor            */
/************************************************************************/

OGRErr OGRPGDataSource::FlushSoftTransaction()

{
    /*CPLDebug("PG", "poDS=%p FlushSoftTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    if (nSoftTransactionLevel <= 0)
        return OGRERR_NONE;

    bSavePointActive = FALSE;

    CPLAssert(nSoftTransactionLevel == 1);
    nSoftTransactionLevel = 0;
    return DoTransactionCommand("COMMIT");
}

/************************************************************************/
/*                          DoTransactionCommand()                      */
/************************************************************************/

OGRErr OGRPGDataSource::DoTransactionCommand(const char *pszCommand)

{
    OGRErr eErr = OGRERR_NONE;
    PGconn *l_hPGConn = GetPGConn();

    PGresult *hResult = OGRPG_PQexec(l_hPGConn, pszCommand);
    osDebugLastTransactionCommand = pszCommand;

    if (!hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK)
    {
        eErr = OGRERR_FAILURE;
    }

    OGRPGClearResult(hResult);

    return eErr;
}

/************************************************************************/
/*                     OGRPGNoResetResultLayer                          */
/************************************************************************/

class OGRPGNoResetResultLayer final : public OGRPGLayer
{
  public:
    OGRPGNoResetResultLayer(OGRPGDataSource *poDSIn, PGresult *hResultIn);

    virtual ~OGRPGNoResetResultLayer();

    virtual void ResetReading() override;

    virtual int TestCapability(const char *) override
    {
        return FALSE;
    }

    virtual OGRFeature *GetNextFeature() override;

    virtual CPLString GetFromClauseForGetExtent() override
    {
        CPLAssert(false);
        return "";
    }

    virtual void ResolveSRID(const OGRPGGeomFieldDefn *poGFldDefn) override
    {
        poGFldDefn->nSRSId = -1;
    }
};

/************************************************************************/
/*                     OGRPGNoResetResultLayer()                        */
/************************************************************************/

OGRPGNoResetResultLayer::OGRPGNoResetResultLayer(OGRPGDataSource *poDSIn,
                                                 PGresult *hResultIn)
{
    poDS = poDSIn;
    ReadResultDefinition(hResultIn);
    hCursorResult = hResultIn;
    CreateMapFromFieldNameToIndex(hCursorResult, poFeatureDefn,
                                  m_panMapFieldNameToIndex,
                                  m_panMapFieldNameToGeomIndex);
}

/************************************************************************/
/*                   ~OGRPGNoResetResultLayer()                         */
/************************************************************************/

OGRPGNoResetResultLayer::~OGRPGNoResetResultLayer()

{
    OGRPGClearResult(hCursorResult);
    hCursorResult = nullptr;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGNoResetResultLayer::ResetReading()
{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGNoResetResultLayer::GetNextFeature()

{
    if (iNextShapeId == PQntuples(hCursorResult))
    {
        return nullptr;
    }
    return RecordToFeature(hCursorResult, m_panMapFieldNameToIndex,
                           m_panMapFieldNameToGeomIndex,
                           static_cast<int>(iNextShapeId++));
}

/************************************************************************/
/*                      OGRPGMemLayerWrapper                            */
/************************************************************************/

class OGRPGMemLayerWrapper final : public OGRLayer
{
  private:
    OGRPGMemLayerWrapper(const OGRPGMemLayerWrapper &) = delete;
    OGRPGMemLayerWrapper &operator=(const OGRPGMemLayerWrapper &) = delete;

    GDALDataset *poMemDS = nullptr;
    OGRLayer *poMemLayer = nullptr;

  public:
    explicit OGRPGMemLayerWrapper(GDALDataset *poMemDSIn)
    {
        poMemDS = poMemDSIn;
        poMemLayer = poMemDS->GetLayer(0);
    }

    virtual ~OGRPGMemLayerWrapper()
    {
        delete poMemDS;
    }

    virtual void ResetReading() override
    {
        poMemLayer->ResetReading();
    }

    virtual OGRFeature *GetNextFeature() override
    {
        return poMemLayer->GetNextFeature();
    }

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return poMemLayer->GetLayerDefn();
    }

    virtual int TestCapability(const char *) override
    {
        return FALSE;
    }
};

/************************************************************************/
/*                           GetMetadataItem()                          */
/************************************************************************/

const char *OGRPGDataSource::GetMetadataItem(const char *pszKey,
                                             const char *pszDomain)
{
    /* Only used by ogr_pg.py to check inner working */
    if (pszDomain != nullptr && EQUAL(pszDomain, "_debug_") &&
        pszKey != nullptr)
    {
        if (EQUAL(pszKey, "bHasLoadTables"))
            return CPLSPrintf("%d", bHasLoadTables);
        if (EQUAL(pszKey, "nSoftTransactionLevel"))
            return CPLSPrintf("%d", nSoftTransactionLevel);
        if (EQUAL(pszKey, "bSavePointActive"))
            return CPLSPrintf("%d", bSavePointActive);
        if (EQUAL(pszKey, "bUserTransactionActive"))
            return CPLSPrintf("%d", bUserTransactionActive);
        if (EQUAL(pszKey, "osDebugLastTransactionCommand"))
        {
            const char *pszRet =
                CPLSPrintf("%s", osDebugLastTransactionCommand.c_str());
            osDebugLastTransactionCommand = "";
            return pszRet;
        }
    }
    return OGRDataSource::GetMetadataItem(pszKey, pszDomain);
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer *OGRPGDataSource::ExecuteSQL(const char *pszSQLCommand,
                                      OGRGeometry *poSpatialFilter,
                                      const char *pszDialect)

{
    /* Skip leading whitespace characters */
    while (std::isspace(static_cast<unsigned char>(*pszSQLCommand)))
        pszSQLCommand++;

    FlushCache(false);

    /* -------------------------------------------------------------------- */
    /*      Use generic implementation for recognized dialects              */
    /* -------------------------------------------------------------------- */
    if (IsGenericSQLDialect(pszDialect))
        return OGRDataSource::ExecuteSQL(pszSQLCommand, poSpatialFilter,
                                         pszDialect);

    /* -------------------------------------------------------------------- */
    /*      Special case DELLAYER: command.                                 */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "DELLAYER:"))
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while (*pszLayerName == ' ')
            pszLayerName++;

        GetLayerCount();
        for (int iLayer = 0; iLayer < nLayers; iLayer++)
        {
            if (EQUAL(papoLayers[iLayer]->GetName(), pszLayerName))
            {
                DeleteLayer(iLayer);
                break;
            }
        }
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Execute the statement.                                          */
    /* -------------------------------------------------------------------- */
    PGresult *hResult = nullptr;

    if (STARTS_WITH_CI(pszSQLCommand, "SELECT") == FALSE ||
        (strstr(pszSQLCommand, "from") == nullptr &&
         strstr(pszSQLCommand, "FROM") == nullptr))
    {
        /* For something that is not a select or a select without table, do not
         */
        /* run under transaction (CREATE DATABASE, VACUUM don't like
         * transactions) */

        hResult =
            OGRPG_PQexec(hPGConn, pszSQLCommand, TRUE /* multiple allowed */);
        if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK)
        {
            CPLDebug("PG", "Command Results Tuples = %d", PQntuples(hResult));

            GDALDriver *poMemDriver =
                OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("Memory");
            if (poMemDriver)
            {
                OGRPGLayer *poResultLayer =
                    new OGRPGNoResetResultLayer(this, hResult);
                GDALDataset *poMemDS =
                    poMemDriver->Create("", 0, 0, 0, GDT_Unknown, nullptr);
                poMemDS->CopyLayer(poResultLayer, "sql_statement");
                OGRPGMemLayerWrapper *poResLayer =
                    new OGRPGMemLayerWrapper(poMemDS);
                delete poResultLayer;
                return poResLayer;
            }
            else
                return nullptr;
        }
    }
    else
    {
        SoftStartTransaction();

        CPLString osCommand;
        osCommand.Printf("DECLARE %s CURSOR for %s", "executeSQLCursor",
                         pszSQLCommand);

        hResult = OGRPG_PQexec(hPGConn, osCommand);

        /* --------------------------------------------------------------------
         */
        /*      Do we have a tuple result? If so, instantiate a results */
        /*      layer for it. */
        /* --------------------------------------------------------------------
         */
        if (hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK)
        {
            OGRPGClearResult(hResult);

            osCommand.Printf("FETCH 0 in %s", "executeSQLCursor");
            hResult = OGRPG_PQexec(hPGConn, osCommand);

            OGRPGResultLayer *poLayer =
                new OGRPGResultLayer(this, pszSQLCommand, hResult);

            OGRPGClearResult(hResult);

            osCommand.Printf("CLOSE %s", "executeSQLCursor");
            hResult = OGRPG_PQexec(hPGConn, osCommand);
            OGRPGClearResult(hResult);

            SoftCommitTransaction();

            if (poSpatialFilter != nullptr)
                poLayer->SetSpatialFilter(poSpatialFilter);

            return poLayer;
        }
        else
        {
            SoftRollbackTransaction();
        }
    }

    OGRPGClearResult(hResult);

    return nullptr;
}

/************************************************************************/
/*                          AbortSQL()                                  */
/************************************************************************/

OGRErr OGRPGDataSource::AbortSQL()
{
    auto cancel = PQgetCancel(hPGConn);
    int result;
    if (cancel)
    {
        char errbuf[255];
        result = PQcancel(cancel, errbuf, 255);
        if (!result)
            CPLDebug("PG", "Error canceling the query: %s", errbuf);
        PQfreeCancel(cancel);
        return result ? OGRERR_NONE : OGRERR_FAILURE;
    }
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRPGDataSource::ReleaseResultSet(OGRLayer *poLayer)

{
    delete poLayer;
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/

void OGRPGDataSource::StartCopy(OGRPGTableLayer *poPGLayer)
{
    if (poLayerInCopyMode == poPGLayer)
        return;
    EndCopy();
    poLayerInCopyMode = poPGLayer;
    poLayerInCopyMode->StartCopy();
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/

OGRErr OGRPGDataSource::EndCopy()
{
    if (poLayerInCopyMode != nullptr)
    {
        OGRErr result = poLayerInCopyMode->EndCopy();
        poLayerInCopyMode = nullptr;

        return result;
    }
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                     CreateMetadataTableIfNeeded()                    */
/************************************************************************/

bool OGRPGDataSource::CreateMetadataTableIfNeeded()
{
    if (m_bCreateMetadataTableIfNeededRun)
        return m_bCreateMetadataTableIfNeededSuccess;

    m_bCreateMetadataTableIfNeededRun = true;

    PGresult *hResult;

    hResult = OGRPG_PQexec(
        hPGConn,
        "SELECT c.oid FROM pg_class c "
        "JOIN pg_namespace n ON c.relnamespace=n.oid "
        "WHERE c.relname = 'metadata' AND n.nspname = 'ogr_system_tables'");
    const bool bFound =
        (hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult, 0, 0));
    OGRPGClearResult(hResult);

    hResult = OGRPG_PQexec(
        hPGConn,
        "SELECT has_database_privilege((select current_database()), 'CREATE')");
    const bool bCanCreateSchema =
        (hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult, 0, 0) &&
         strcmp(PQgetvalue(hResult, 0, 0), "t") == 0);
    OGRPGClearResult(hResult);

    if (!bFound)
    {
        if (!bCanCreateSchema)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "User lacks CREATE SCHEMA privilege to be able to create "
                     "ogr_system_tables.metadata table");
            return false;
        }
    }
    else
    {
        if (!HasWritePermissionsOnMetadataTable())
        {
            return false;
        }
        if (!bCanCreateSchema)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "User lacks CREATE SCHEMA privilege. Assuming "
                     "ogr_system_tables.metadata table has correct structure");
            m_bCreateMetadataTableIfNeededSuccess = true;
            return true;
        }
    }

    hResult =
        OGRPG_PQexec(hPGConn, "CREATE SCHEMA IF NOT EXISTS ogr_system_tables");
    if (!hResult || (PQresultStatus(hResult) != PGRES_COMMAND_OK &&
                     PQresultStatus(hResult) != PGRES_TUPLES_OK))
    {
        OGRPGClearResult(hResult);
        return false;
    }
    OGRPGClearResult(hResult);

    hResult = OGRPG_PQexec(
        hPGConn, "CREATE TABLE IF NOT EXISTS ogr_system_tables.metadata("
                 "id SERIAL, "
                 "schema_name TEXT NOT NULL, "
                 "table_name TEXT NOT NULL, "
                 "metadata TEXT,"
                 "UNIQUE(schema_name, table_name))");
    if (!hResult || (PQresultStatus(hResult) != PGRES_COMMAND_OK &&
                     PQresultStatus(hResult) != PGRES_TUPLES_OK))
    {
        OGRPGClearResult(hResult);
        return false;
    }
    OGRPGClearResult(hResult);

    hResult = OGRPG_PQexec(
        hPGConn,
        "DROP FUNCTION IF EXISTS "
        "ogr_system_tables.event_trigger_function_for_metadata() CASCADE");
    if (!hResult || (PQresultStatus(hResult) != PGRES_COMMAND_OK &&
                     PQresultStatus(hResult) != PGRES_TUPLES_OK))
    {
        OGRPGClearResult(hResult);
        return false;
    }
    OGRPGClearResult(hResult);

    hResult = OGRPG_PQexec(
        hPGConn,
        "CREATE FUNCTION "
        "ogr_system_tables.event_trigger_function_for_metadata()\n"
        "RETURNS event_trigger LANGUAGE plpgsql AS $$\n"
        "DECLARE\n"
        "    obj record;\n"
        "BEGIN\n"
        "  IF has_schema_privilege('ogr_system_tables', 'USAGE') THEN\n"
        "   IF has_table_privilege('ogr_system_tables.metadata', 'DELETE') "
        "THEN\n"
        "    FOR obj IN SELECT * FROM pg_event_trigger_dropped_objects()\n"
        "    LOOP\n"
        "        IF obj.object_type = 'table' THEN\n"
        "            DELETE FROM ogr_system_tables.metadata m WHERE "
        "m.schema_name = obj.schema_name AND m.table_name = "
        "obj.object_name;\n"
        "        END IF;\n"
        "    END LOOP;\n"
        "   END IF;\n"
        "  END IF;\n"
        "END;\n"
        "$$;");
    if (!hResult || (PQresultStatus(hResult) != PGRES_COMMAND_OK &&
                     PQresultStatus(hResult) != PGRES_TUPLES_OK))
    {
        OGRPGClearResult(hResult);
        return false;
    }
    OGRPGClearResult(hResult);

    hResult =
        OGRPG_PQexec(hPGConn, "DROP EVENT TRIGGER IF EXISTS "
                              "ogr_system_tables_event_trigger_for_metadata");
    if (!hResult || (PQresultStatus(hResult) != PGRES_COMMAND_OK &&
                     PQresultStatus(hResult) != PGRES_TUPLES_OK))
    {
        OGRPGClearResult(hResult);
        return false;
    }
    OGRPGClearResult(hResult);

    hResult = OGRPG_PQexec(
        hPGConn,
        "CREATE EVENT TRIGGER ogr_system_tables_event_trigger_for_metadata "
        "ON sql_drop "
        "EXECUTE FUNCTION "
        "ogr_system_tables.event_trigger_function_for_metadata()");
    if (!hResult || (PQresultStatus(hResult) != PGRES_COMMAND_OK &&
                     PQresultStatus(hResult) != PGRES_TUPLES_OK))
    {
        OGRPGClearResult(hResult);
        return false;
    }
    OGRPGClearResult(hResult);

    m_bCreateMetadataTableIfNeededSuccess = true;
    m_bOgrSystemTablesMetadataTableExistenceTested = true;
    m_bOgrSystemTablesMetadataTableFound = true;
    return true;
}

/************************************************************************/
/*                    HasOgrSystemTablesMetadataTable()                 */
/************************************************************************/

bool OGRPGDataSource::HasOgrSystemTablesMetadataTable()
{
    if (!m_bOgrSystemTablesMetadataTableExistenceTested &&
        CPLTestBool(CPLGetConfigOption("OGR_PG_ENABLE_METADATA", "YES")))
    {
        m_bOgrSystemTablesMetadataTableExistenceTested = true;
        // Check that the ogr_system_tables.metadata table exists (without
        // causing errors that might abort transactions)
        PGresult *hResult = OGRPG_PQexec(
            hPGConn,
            "SELECT c.oid FROM pg_class c "
            "JOIN pg_namespace n ON c.relnamespace=n.oid "
            "WHERE c.relname = 'metadata' AND n.nspname = 'ogr_system_tables'");
        const bool bFound =
            (hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult, 0, 0));
        OGRPGClearResult(hResult);
        if (!bFound)
            return false;

        hResult = OGRPG_PQexec(
            hPGConn,
            "SELECT has_schema_privilege('ogr_system_tables', 'USAGE')");
        const bool bHasSchemaPrivilege =
            (hResult && PQntuples(hResult) == 1 &&
             !PQgetisnull(hResult, 0, 0) &&
             strcmp(PQgetvalue(hResult, 0, 0), "t") == 0);
        OGRPGClearResult(hResult);
        if (!bHasSchemaPrivilege)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Table ogr_system_tables.metadata exists but user lacks "
                     "USAGE privilege on ogr_system_tables schema");
            return false;
        }

        hResult = OGRPG_PQexec(
            hPGConn, "SELECT has_table_privilege('ogr_system_tables.metadata', "
                     "'SELECT')");
        m_bOgrSystemTablesMetadataTableFound =
            (hResult && PQntuples(hResult) == 1 &&
             !PQgetisnull(hResult, 0, 0) &&
             strcmp(PQgetvalue(hResult, 0, 0), "t") == 0);
        OGRPGClearResult(hResult);
        if (!m_bOgrSystemTablesMetadataTableFound)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Table ogr_system_tables.metadata exists but user lacks "
                     "SELECT privilege on it");
        }
    }
    return m_bOgrSystemTablesMetadataTableFound;
}

/************************************************************************/
/*                   HasWritePermissionsOnMetadataTable()               */
/************************************************************************/

bool OGRPGDataSource::HasWritePermissionsOnMetadataTable()
{
    if (!m_bHasWritePermissionsOnMetadataTableRun)
    {
        m_bHasWritePermissionsOnMetadataTableRun = true;

        if (HasOgrSystemTablesMetadataTable())
        {
            PGresult *hResult = OGRPG_PQexec(
                hPGConn,
                "SELECT has_table_privilege('ogr_system_tables.metadata', "
                "'INSERT') "
                "AND    has_table_privilege('ogr_system_tables.metadata', "
                "'DELETE')");
            m_bHasWritePermissionsOnMetadataTableSuccess =
                (hResult && PQntuples(hResult) == 1 &&
                 !PQgetisnull(hResult, 0, 0) &&
                 strcmp(PQgetvalue(hResult, 0, 0), "t") == 0);
            OGRPGClearResult(hResult);
            if (!m_bHasWritePermissionsOnMetadataTableSuccess)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "User lacks INSERT and/OR DELETE privilege on "
                         "ogr_system_tables.metadata table");
            }
        }
    }
    return m_bHasWritePermissionsOnMetadataTableSuccess;
}
