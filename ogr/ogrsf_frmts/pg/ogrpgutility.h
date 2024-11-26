/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private utilities for OGR/PostgreSQL driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRPGUTILITY_H_INCLUDED
#define OGRPGUTILITY_H_INCLUDED

#include "libpq-fe.h"

PGresult *OGRPG_PQexec(PGconn *conn, const char *query,
                       int bMultipleCommandAllowed = FALSE,
                       int bErrorAsDebug = FALSE);

/************************************************************************/
/*                            OGRPGClearResult                          */
/*                                                                      */
/*      Safe wrapper for PQclear() function.                            */
/*      Releases given result and resets handle to NULL.                */
/*      Parameter hResult is input/output - a reference to pointer      */
/************************************************************************/

inline void OGRPGClearResult(PGresult *&hResult)
{
    if (nullptr != hResult)
    {
        PQclear(hResult);
        hResult = nullptr;
    }
}

bool OGRPG_Check_Table_Exists(PGconn *hPGConn, const char *pszTableName);

#endif /* ndef OGRPGUTILITY_H_INCLUDED */
