/******************************************************************************
 * $Id$
 *
 * Project:  OGR ODBC Driver
 * Purpose:  Declarations for ODBC Access Cover API.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2003/09/24 15:39:14  warmerda
 * New
 *
 */

#ifndef CPL_ODBC_H_INCLUDED
#define CPL_ODBC_H_INCLUDED

#include "cpl_port.h"

#include <sql.h>
#include <sqlext.h>

class CPL_DLL CPLODBCSession {
    char      m_szLastError[SQL_MAX_MESSAGE_LENGTH + 1];
    HENV      m_hEnv;
    HDBC      m_hDBC;

  public:
    CPLODBCSession();
    ~CPLODBCSession();

    int		EstablishSession( const char *pszDSN, 
                                  const char *pszUserid, 
                                  const char *pszPassword );
    int         CloseSession();

    int         Failed( int, HSTMT = NULL );

    HDBC        GetConnection() { return m_hDBC; }
    HENV        GetEnvironment()  { return m_hEnv; }

    const char  *GetLastError() { return m_szLastError; }
};

class CPL_DLL CPLODBCStatement {

    CPLODBCSession     *m_poSession;
    HSTMT		m_hStmt;

    short          m_nColCount;
    char         **m_papszColNames;
    short         *m_panColType;
    short         *m_panColSize;
    short         *m_panColPrecision;
    short         *m_panColNullable;

    char         **m_papszColValues;
    
    int            Failed( int );

    char          *m_pszStatement;
    int            m_nStatementMax;
    int            m_nStatementLen;

    int            CollectResultsInfo();

  public:
    CPLODBCStatement( CPLODBCSession * );
    ~CPLODBCStatement();

    HSTMT          GetStatement() { return m_hStmt; }

    // Command buffer related.
    void           Clear();
    void           Append( const char * );
    void           Append( int );
    void           Append( double );
    int            Appendf( const char *, ... );
    const char    *GetCommand() { return m_pszStatement; }

    int            ExecuteSQL( const char * = NULL );

    // Results fetching
    int            Fetch();
    void           ClearColumnData();

    int            GetColCount();
    const char    *GetColName(int iCol);
    short          GetColType(int iCol);
    short          GetColSize(int iCol);
    short          GetColPrecision(int iCol);
    short          GetColNullable(int iCol);

    int            GetColId( const char * );
    const char    *GetColData( int );
};



#endif


