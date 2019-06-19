/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility methods
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_pg.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRPG_PQexec()                               */
/************************************************************************/

PGresult *OGRPG_PQexec(PGconn *conn, const char *query, int bMultipleCommandAllowed,
                       int bErrorAsDebug)
{
    PGresult* hResult = bMultipleCommandAllowed
        ? PQexec(conn, query)
        : PQexecParams(conn, query, 0, nullptr, nullptr, nullptr, nullptr, 0);

#ifdef DEBUG
    const char* pszRetCode = "UNKNOWN";
    char szNTuples[32] = {};
    if (hResult)
    {
        switch(PQresultStatus(hResult))
        {
            case PGRES_TUPLES_OK:
                pszRetCode = "PGRES_TUPLES_OK";
                snprintf(szNTuples, sizeof(szNTuples), ", ntuples = %d", PQntuples(hResult));
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
            default: break;
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
    if ( !hResult || (PQresultStatus(hResult) == PGRES_NONFATAL_ERROR ||
                      PQresultStatus(hResult) == PGRES_FATAL_ERROR ) )
    {
        if( bErrorAsDebug )
            CPLDebug("PG", "%s", PQerrorMessage( conn ) );
        else
            CPLError( CE_Failure, CPLE_AppDefined, "%s", PQerrorMessage( conn ) );
    }

    return hResult;
}

/************************************************************************/
/*                       OGRPG_Check_Table_Exists()                     */
/************************************************************************/

bool OGRPG_Check_Table_Exists(PGconn *hPGConn, const char * pszTableName)
{
    CPLString osSQL;
    osSQL.Printf("SELECT 1 FROM information_schema.tables WHERE table_name = %s LIMIT 1",
                 OGRPGEscapeString(hPGConn, pszTableName).c_str());
    PGresult* hResult = OGRPG_PQexec(hPGConn, osSQL);
    bool bRet = ( hResult && PQntuples(hResult) == 1 );
    if( !bRet )
        CPLDebug("PG", "Does not have %s table", pszTableName);
    OGRPGClearResult( hResult );
    return bRet;
}
