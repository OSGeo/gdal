/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility methods
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRPG_PQexec()                               */
/************************************************************************/

PGresult *OGRPG_PQexec(PGconn *conn, const char *query, int bMultipleCommandAllowed)
{
    PGresult* hResult;
#if defined(PG_PRE74)
    /* PQexecParams introduced in PG >= 7.4 */
    hResult = PQexec(conn, query);
#else
    if (bMultipleCommandAllowed)
        hResult = PQexec(conn, query);
    else
        hResult = PQexecParams(conn, query, 0, NULL, NULL, NULL, NULL, 0);
#endif

#ifdef DEBUG
    const char* pszRetCode = "UNKNOWN";
    char szNTuples[32];
    szNTuples[0] = '\0';
    if (hResult)
    {
        switch(PQresultStatus(hResult))
        {
            case PGRES_TUPLES_OK:
                pszRetCode = "PGRES_TUPLES_OK";
                sprintf(szNTuples, ", ntuples = %d", PQntuples(hResult));
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

/* -------------------------------------------------------------------- */
/*      Generate an error report if an error occured.                   */
/* -------------------------------------------------------------------- */
    if ( !hResult || (PQresultStatus(hResult) == PGRES_NONFATAL_ERROR ||
                      PQresultStatus(hResult) == PGRES_FATAL_ERROR ) )
    {
        CPLDebug( "PG", "%s", PQerrorMessage( conn ) );
    }
#endif

    return hResult;
}
