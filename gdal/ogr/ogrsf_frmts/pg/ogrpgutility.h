/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private utilities for OGR/PostgreSQL driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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

inline void OGRPGClearResult( PGresult*& hResult )
{
    if( NULL != hResult )
    {
        PQclear( hResult );
        hResult = NULL;
    }
}

#endif /* ndef OGRPGUTILITY_H_INCLUDED */

