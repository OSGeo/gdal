/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility methods
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_pg.h"
#include "cpl_conv.h"

/************************************************************************/
/*                         OGRPG_PQexec()                               */
/************************************************************************/

PGresult *OGRPG_PQexec(PGconn *conn, const char *query,
                       int bMultipleCommandAllowed, int bErrorAsDebug)
{
    PGresult *hResult = bMultipleCommandAllowed
                            ? PQexec(conn, query)
                            : PQexecParams(conn, query, 0, nullptr, nullptr,
                                           nullptr, nullptr, 0);

#ifdef DEBUG
    const char *pszRetCode = "UNKNOWN";
    char szNTuples[32] = {};
    if (hResult)
    {
        switch (PQresultStatus(hResult))
        {
            case PGRES_TUPLES_OK:
                pszRetCode = "PGRES_TUPLES_OK";
                snprintf(szNTuples, sizeof(szNTuples), ", ntuples = %d",
                         PQntuples(hResult));
                break;
            case PGRES_COMMAND_OK:
                pszRetCode = "PGRES_COMMAND_OK";
                break;
            case PGRES_NONFATAL_ERROR:
                pszRetCode = "PGRES_NONFATAL_ERROR";
                break;
            case PGRES_FATAL_ERROR:
                pszRetCode = "PGRES_FATAL_ERROR";
                break;
            default:
                break;
        }
    }
    if (bMultipleCommandAllowed)
        CPLDebug("PG", "PQexec(%s) = %s%s", query, pszRetCode, szNTuples);
    else
        CPLDebug("PG", "PQexecParams(%s) = %s%s", query, pszRetCode, szNTuples);
#endif

    /* -------------------------------------------------------------------- */
    /*      Generate an error report if an error occurred.                  */
    /* -------------------------------------------------------------------- */
    if (!hResult || (PQresultStatus(hResult) == PGRES_NONFATAL_ERROR ||
                     PQresultStatus(hResult) == PGRES_FATAL_ERROR))
    {
        if (bErrorAsDebug)
            CPLDebug("PG", "%s", PQerrorMessage(conn));
        else
            CPLError(CE_Failure, CPLE_AppDefined, "%s", PQerrorMessage(conn));
    }

    return hResult;
}

/************************************************************************/
/*                       OGRPG_Check_Table_Exists()                     */
/************************************************************************/

bool OGRPG_Check_Table_Exists(PGconn *hPGConn, const char *pszTableName)
{
    CPLString osSQL;
    osSQL.Printf(
        "SELECT 1 FROM information_schema.tables WHERE table_name = %s LIMIT 1",
        OGRPGEscapeString(hPGConn, pszTableName).c_str());
    PGresult *hResult = OGRPG_PQexec(hPGConn, osSQL);
    bool bRet = (hResult && PQntuples(hResult) == 1);
    if (!bRet)
        CPLDebug("PG", "Does not have %s table", pszTableName);
    OGRPGClearResult(hResult);
    return bRet;
}
