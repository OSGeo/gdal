/******************************************************************************
 *
 * Project:  OGR ODBC Driver
 * Purpose:  Declarations for ODBC Access Cover API.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
 * Copyright (c) 2008, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_odbc.h"
#include "cpl_vsi.h"
#include "cpl_string.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

#ifndef SQLColumns_TABLE_CAT
#define SQLColumns_TABLE_CAT 1
#define SQLColumns_TABLE_SCHEM 2
#define SQLColumns_TABLE_NAME 3
#define SQLColumns_COLUMN_NAME 4
#define SQLColumns_DATA_TYPE 5
#define SQLColumns_TYPE_NAME 6
#define SQLColumns_COLUMN_SIZE 7
#define SQLColumns_BUFFER_LENGTH 8
#define SQLColumns_DECIMAL_DIGITS 9
#define SQLColumns_NUM_PREC_RADIX 10
#define SQLColumns_NULLABLE 11
#define SQLColumns_REMARKS 12
#define SQLColumns_COLUMN_DEF 13
#define SQLColumns_SQL_DATA_TYPE 14
#define SQLColumns_SQL_DATETIME_SUB 15
#define SQLColumns_CHAR_OCTET_LENGTH 16
#define SQLColumns_ORDINAL_POSITION 17
#define SQLColumns_IS_NULLABLE 18
#endif  // ndef SQLColumns_TABLE_CAT

/************************************************************************/
/*                           CPLODBCDriverInstaller()                   */
/************************************************************************/

CPLODBCDriverInstaller::CPLODBCDriverInstaller() :
    m_nErrorCode(0),
    m_nUsageCount(0)
{
    memset( m_szPathOut, '\0', ODBC_FILENAME_MAX );
    memset( m_szError, '\0', SQL_MAX_MESSAGE_LENGTH );
}

/************************************************************************/
/*                           InstallDriver()                            */
/************************************************************************/

int CPLODBCDriverInstaller::InstallDriver( const char* pszDriver,
                                           CPL_UNUSED const char* pszPathIn,
                                           WORD fRequest )
{
    CPLAssert( NULL != pszDriver );

    // Try to install driver to system-wide location.
    if( FALSE == SQLInstallDriverEx( pszDriver, NULL, m_szPathOut,
                                     ODBC_FILENAME_MAX, NULL, fRequest,
                                     &m_nUsageCount ) )
    {
        const WORD nErrorNum = 1;  // TODO - a function param?
        CPL_UNUSED RETCODE cRet = SQL_ERROR;

        // Failure is likely related to no write permissions to
        // system-wide default location, so try to install to HOME.

        static char* pszEnvIni = NULL;
        if( pszEnvIni == NULL )
        {
            // Read HOME location.
            char* pszEnvHome = NULL;
            pszEnvHome = getenv("HOME");

            CPLAssert( NULL != pszEnvHome );
            CPLDebug( "ODBC", "HOME=%s", pszEnvHome );

            // Set ODBCSYSINI variable pointing to HOME location.
            const size_t nLen = strlen(pszEnvHome) + 12;
            pszEnvIni = static_cast<char *>(CPLMalloc(nLen));

            snprintf( pszEnvIni, nLen, "ODBCSYSINI=%s", pszEnvHome );
            // A 'man putenv' shows that we cannot free pszEnvIni
            // because the pointer is used directly by putenv in old glibc.
            // coverity[tainted_string]
            putenv( pszEnvIni );

            CPLDebug( "ODBC", "%s", pszEnvIni );
        }

        // Try to install ODBC driver in new location.
        if( FALSE == SQLInstallDriverEx(pszDriver, NULL, m_szPathOut,
                                        ODBC_FILENAME_MAX, NULL, fRequest,
                                        &m_nUsageCount) )
        {
            cRet = SQLInstallerError( nErrorNum, &m_nErrorCode,
                            m_szError, SQL_MAX_MESSAGE_LENGTH, NULL );
            CPLAssert( SQL_SUCCESS == cRet || SQL_SUCCESS_WITH_INFO == cRet );

            // FAIL
            return FALSE;
        }
    }

    // SUCCESS
    return TRUE;
}

/************************************************************************/
/*                           RemoveDriver()                             */
/************************************************************************/

int CPLODBCDriverInstaller::RemoveDriver( const char* pszDriverName,
                                          int fRemoveDSN )
{
    CPLAssert( NULL != pszDriverName );

    if( FALSE == SQLRemoveDriver( pszDriverName, fRemoveDSN, &m_nUsageCount ) )
    {
        const WORD nErrorNum = 1; // TODO - a function param?

        // Retrieve error code and message.
        SQLInstallerError( nErrorNum, &m_nErrorCode,
                           m_szError, SQL_MAX_MESSAGE_LENGTH, NULL );

        return FALSE;
    }

    // SUCCESS
    return TRUE;
}

/************************************************************************/
/*                           CPLODBCSession()                           */
/************************************************************************/

/** Constructor */
CPLODBCSession::CPLODBCSession() :
    m_hEnv(NULL),
    m_hDBC(NULL),
    m_bInTransaction(FALSE),
    m_bAutoCommit(TRUE)
{
    m_szLastError[0] = '\0';
}

/************************************************************************/
/*                          ~CPLODBCSession()                           */
/************************************************************************/

/** Destructor */
CPLODBCSession::~CPLODBCSession()

{
    CloseSession();
}

/************************************************************************/
/*                            CloseSession()                            */
/************************************************************************/

/** Close session */
int CPLODBCSession::CloseSession()

{
    if( m_hDBC!=NULL )
    {
        if( IsInTransaction() )
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Closing session with active transactions." );
        CPLDebug( "ODBC", "SQLDisconnect()" );
        SQLDisconnect( m_hDBC );
        SQLFreeConnect( m_hDBC );
        m_hDBC = NULL;
    }

    if( m_hEnv!=NULL )
    {
        SQLFreeEnv( m_hEnv );
        m_hEnv = NULL;
    }

    return TRUE;
}

/************************************************************************/
/*                       ClearTransaction()                             */
/************************************************************************/

/** Clear transaction */
int CPLODBCSession::ClearTransaction()

{
#if (ODBCVER >= 0x0300)

    if( m_bAutoCommit )
        return TRUE;

    SQLUINTEGER bAutoCommit;
    // See if we already in manual commit mode.
    if( Failed( SQLGetConnectAttr( m_hDBC, SQL_ATTR_AUTOCOMMIT, &bAutoCommit,
                                   sizeof(SQLUINTEGER), NULL) ) )
        return FALSE;

    if( bAutoCommit == SQL_AUTOCOMMIT_OFF )
    {
        // Switch the connection to auto commit mode (default).
        if( Failed( SQLSetConnectAttr( m_hDBC, SQL_ATTR_AUTOCOMMIT,
                                       (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0 ) ) )
            return FALSE;
    }

    m_bInTransaction = FALSE;
    m_bAutoCommit = TRUE;

#endif
    return TRUE;
}

/************************************************************************/
/*                       CommitTransaction()                            */
/************************************************************************/

/** Begin transaction */
int CPLODBCSession::BeginTransaction()

{
#if (ODBCVER >= 0x0300)

    SQLUINTEGER bAutoCommit;
    // See if we already in manual commit mode.
    if( Failed( SQLGetConnectAttr( m_hDBC, SQL_ATTR_AUTOCOMMIT, &bAutoCommit,
                                   sizeof(SQLUINTEGER), NULL) ) )
        return FALSE;

    if( bAutoCommit == SQL_AUTOCOMMIT_ON )
    {
        // Switch the connection to manual commit mode.
        if( Failed( SQLSetConnectAttr( m_hDBC, SQL_ATTR_AUTOCOMMIT,
                                       (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0 ) ) )
            return FALSE;
    }

    m_bInTransaction = TRUE;
    m_bAutoCommit = FALSE;

#endif
    return TRUE;
}

/************************************************************************/
/*                       CommitTransaction()                            */
/************************************************************************/

/** Commit transaction */
int CPLODBCSession::CommitTransaction()

{
#if (ODBCVER >= 0x0300)

    if( m_bInTransaction )
    {
        if( Failed( SQLEndTran( SQL_HANDLE_DBC, m_hDBC, SQL_COMMIT ) ) )
        {
            return FALSE;
        }
        m_bInTransaction = FALSE;
    }

#endif
    return TRUE;
}

/************************************************************************/
/*                       RollbackTransaction()                          */
/************************************************************************/

/** Rollback transaction */
int CPLODBCSession::RollbackTransaction()

{
#if (ODBCVER >= 0x0300)

    if( m_bInTransaction )
    {
        // Rollback should not hide the previous error so Failed() is not
        // called.
        int nRetCode = SQLEndTran( SQL_HANDLE_DBC, m_hDBC, SQL_ROLLBACK );
        m_bInTransaction = FALSE;

        return (nRetCode == SQL_SUCCESS || nRetCode == SQL_SUCCESS_WITH_INFO );
    }

#endif
    return TRUE;
}

/************************************************************************/
/*                               Failed()                               */
/************************************************************************/

/** Test if a return code indicates failure, return TRUE if that
 * is the case. Also update error text.
 */
int CPLODBCSession::Failed( int nRetCode, HSTMT hStmt )

{
    m_szLastError[0] = '\0';

    if( nRetCode == SQL_SUCCESS || nRetCode == SQL_SUCCESS_WITH_INFO )
        return FALSE;

    SQLCHAR achSQLState[SQL_MAX_MESSAGE_LENGTH] = {};
    SQLINTEGER nNativeError = 0;
    SQLSMALLINT nTextLength = 0;
    SQLError( m_hEnv, m_hDBC, hStmt, achSQLState, &nNativeError,
              (SQLCHAR *) m_szLastError, sizeof(m_szLastError)-1,
              &nTextLength );
    m_szLastError[nTextLength] = '\0';

    if( nRetCode == SQL_ERROR && m_bInTransaction )
        RollbackTransaction();

    return TRUE;
}

/************************************************************************/
/*                          EstablishSession()                          */
/************************************************************************/

/**
 * Connect to database and logon.
 *
 * @param pszDSN The name of the DSN being used to connect.  This is not
 * optional.
 *
 * @param pszUserid the userid to logon as, may be NULL if not not required,
 * or provided by the DSN.
 *
 * @param pszPassword the password to logon with.   May be NULL if not required
 * or provided by the DSN.
 *
 * @return TRUE on success or FALSE on failure.  Call GetLastError() to get
 * details on failure.
 */

int CPLODBCSession::EstablishSession( const char *pszDSN,
                                      const char *pszUserid,
                                      const char *pszPassword )

{
    CloseSession();

    if( Failed( SQLAllocEnv( &m_hEnv ) ) )
        return FALSE;

    if( Failed( SQLAllocConnect( m_hEnv, &m_hDBC ) ) )
    {
        CloseSession();
        return FALSE;
    }

#ifdef _MSC_VER
#pragma warning( push )
// warning C4996: 'SQLSetConnectOption': ODBC API: SQLSetConnectOption is
// deprecated. Please use SQLSetConnectAttr instead
#pragma warning( disable : 4996 )
#endif
    SQLSetConnectOption( m_hDBC, SQL_LOGIN_TIMEOUT, 30 );
#ifdef _MSC_VER
#pragma warning( pop )
#endif

    if( pszUserid == NULL )
        pszUserid = "";
    if( pszPassword == NULL )
        pszPassword = "";

    bool bFailed = false;
    if( strstr(pszDSN, "=") != NULL )
    {
        CPLDebug( "ODBC", "SQLDriverConnect(%s)", pszDSN );
        SQLCHAR szOutConnString[1024] = {};
        SQLSMALLINT nOutConnStringLen = 0;

        bFailed = CPL_TO_BOOL(Failed(
            SQLDriverConnect( m_hDBC, NULL,
                              (SQLCHAR *) pszDSN, (SQLSMALLINT)strlen(pszDSN),
                              szOutConnString, sizeof(szOutConnString),
                              &nOutConnStringLen, SQL_DRIVER_NOPROMPT ) ));
    }
    else
    {
        CPLDebug( "ODBC", "SQLConnect(%s)", pszDSN );
        bFailed = CPL_TO_BOOL(Failed(
            SQLConnect( m_hDBC, (SQLCHAR *) pszDSN, SQL_NTS,
                        (SQLCHAR *) pszUserid, SQL_NTS,
                        (SQLCHAR *) pszPassword, SQL_NTS ) ));
    }

    if( bFailed )
    {
        CPLDebug( "ODBC", "... failed: %s", GetLastError() );
        CloseSession();
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                            GetLastError()                            */
/************************************************************************/

/**
 * Returns the last ODBC error message.
 *
 * @return pointer to an internal buffer with the error message in it.
 * Do not free or alter.  Will be an empty (but not NULL) string if there is
 * no pending error info.
 */

const char *CPLODBCSession::GetLastError()

{
    return m_szLastError;
}

/************************************************************************/
/* ==================================================================== */
/*                           CPLODBCStatement                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          CPLODBCStatement()                          */
/************************************************************************/

/** Constructor */
CPLODBCStatement::CPLODBCStatement( CPLODBCSession *poSession ) :
    m_poSession(poSession),
    m_hStmt(NULL),
    m_nColCount(0),
    m_papszColNames(NULL),
    m_panColType(NULL),
    m_papszColTypeNames(NULL),
    m_panColSize(NULL),
    m_panColPrecision(NULL),
    m_panColNullable(NULL),
    m_papszColColumnDef(NULL),
    m_papszColValues(NULL),
    m_panColValueLengths(NULL),
    m_pszStatement(NULL),
    m_nStatementMax(0),
    m_nStatementLen(0)
{

    if( Failed(SQLAllocStmt(poSession->GetConnection(), &m_hStmt)) )
    {
        m_hStmt = NULL;
    }
}

/************************************************************************/
/*                         ~CPLODBCStatement()                          */
/************************************************************************/

/** Destructor */
CPLODBCStatement::~CPLODBCStatement()

{
    Clear();

    if( m_hStmt != NULL )
        SQLFreeStmt( m_hStmt, SQL_DROP );
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

/**
 * Execute an SQL statement.
 *
 * This method will execute the passed (or stored) SQL statement,
 * and initialize information about the resultset if there is one.
 * If a NULL statement is passed, the internal stored statement that
 * has been previously set via Append() or Appendf() calls will be used.
 *
 * @param pszStatement the SQL statement to execute, or NULL if the
 * internally saved one should be used.
 *
 * @return TRUE on success or FALSE if there is an error.  Error details
 * can be fetched with OGRODBCSession::GetLastError().
 */

int CPLODBCStatement::ExecuteSQL( const char *pszStatement )

{
    if( m_poSession == NULL || m_hStmt == NULL )
    {
        // We should post an error.
        return FALSE;
    }

    if( pszStatement != NULL )
    {
        Clear();
        Append( pszStatement );
    }

#if( ODBCVER >= 0x0300 )

    if( !m_poSession->IsInTransaction() )
    {
        // Commit pending transactions and set to autocommit mode.
        m_poSession->ClearTransaction();
    }

#endif

    // SQL_NTS=-3 is a valid value for SQLExecDirect.
    // coverity[negative_returns]
    if( Failed(
            SQLExecDirect( m_hStmt, (SQLCHAR *) m_pszStatement, SQL_NTS ) ) )
        return FALSE;

    return CollectResultsInfo();
}

/************************************************************************/
/*                         CollectResultsInfo()                         */
/************************************************************************/

/** CollectResultsInfo */
int CPLODBCStatement::CollectResultsInfo()

{
    if( m_poSession == NULL || m_hStmt == NULL )
    {
        // We should post an error.
        return FALSE;
    }

    if( Failed( SQLNumResultCols(m_hStmt, &m_nColCount) ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Allocate per column information.                                */
/* -------------------------------------------------------------------- */
    m_papszColNames =
        static_cast<char **>(CPLCalloc(sizeof(char *), m_nColCount + 1));
    m_papszColValues =
        static_cast<char **>(CPLCalloc(sizeof(char *), m_nColCount + 1));
    m_panColValueLengths = static_cast<CPL_SQLLEN *>(
        CPLCalloc(sizeof(CPL_SQLLEN), m_nColCount + 1));

    m_panColType =
        static_cast<SQLSMALLINT *>(CPLCalloc(sizeof(SQLSMALLINT), m_nColCount));
    m_papszColTypeNames =
        static_cast<char **>(CPLCalloc(sizeof(char *), m_nColCount + 1));
    m_panColSize =
        static_cast<CPL_SQLULEN *>(CPLCalloc(sizeof(CPL_SQLULEN), m_nColCount));
    m_panColPrecision =
        static_cast<SQLSMALLINT *>(CPLCalloc(sizeof(SQLSMALLINT), m_nColCount));
    m_panColNullable =
        static_cast<SQLSMALLINT *>(CPLCalloc(sizeof(SQLSMALLINT), m_nColCount));
    m_papszColColumnDef =
        static_cast<char **>(CPLCalloc(sizeof(char *), m_nColCount + 1));

/* -------------------------------------------------------------------- */
/*      Fetch column descriptions.                                      */
/* -------------------------------------------------------------------- */
    for( SQLUSMALLINT iCol = 0; iCol < m_nColCount; iCol++ )
    {
        SQLCHAR szName[256] = {};
        SQLSMALLINT nNameLength = 0;

        if( Failed( SQLDescribeCol(m_hStmt, iCol+1,
                                   szName, sizeof(szName), &nNameLength,
                                   m_panColType + iCol,
                                   m_panColSize + iCol,
                                   m_panColPrecision + iCol,
                                   m_panColNullable + iCol) ) )
            return FALSE;

        szName[nNameLength] = '\0';  // Paranoid; the string should be
                                     // null-terminated by the driver.
        m_papszColNames[iCol] = CPLStrdup(reinterpret_cast<char *>(szName));

        // SQLDescribeCol() fetches just a subset of column attributes.
        // In addition to above data we need data type name.
        if( Failed( SQLColAttribute(m_hStmt, iCol + 1, SQL_DESC_TYPE_NAME,
                                    szName, sizeof(szName),
                                    &nNameLength, NULL) ) )
            return FALSE;

        szName[nNameLength] = '\0';  // Paranoid.
        m_papszColTypeNames[iCol] = CPLStrdup(reinterpret_cast<char *>(szName));

#if DEBUG_VERBOSE
        CPLDebug( "ODBC", "%s %s %d", m_papszColNames[iCol],
                  szName, m_panColType[iCol] );
#endif
    }

    return TRUE;
}

/************************************************************************/
/*                            GetRowCountAffected()                     */
/************************************************************************/

/** GetRowCountAffected */
int CPLODBCStatement::GetRowCountAffected()
{
    SQLLEN nResultCount = 0;
    SQLRowCount( m_hStmt, &nResultCount );

    return static_cast<int>(nResultCount);
}

/************************************************************************/
/*                            GetColCount()                             */
/************************************************************************/

/**
 * Fetch the resultset column count.
 *
 * @return the column count, or zero if there is no resultset.
 */

int CPLODBCStatement::GetColCount()

{
    return m_nColCount;
}

/************************************************************************/
/*                             GetColName()                             */
/************************************************************************/

/**
 * Fetch a column name.
 *
 * @param iCol the zero based column index.
 *
 * @return NULL on failure (out of bounds column), or a pointer to an
 * internal copy of the column name.
 */

const char *CPLODBCStatement::GetColName( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return NULL;

    return m_papszColNames[iCol];
}

/************************************************************************/
/*                             GetColType()                             */
/************************************************************************/

/**
 * Fetch a column data type.
 *
 * The return type code is a an ODBC SQL_ code, one of SQL_UNKNOWN_TYPE,
 * SQL_CHAR, SQL_NUMERIC, SQL_DECIMAL, SQL_INTEGER, SQL_SMALLINT, SQL_FLOAT,
 * SQL_REAL, SQL_DOUBLE, SQL_DATETIME, SQL_VARCHAR, SQL_TYPE_DATE,
 * SQL_TYPE_TIME, SQL_TYPE_TIMESTAMPT.
 *
 * @param iCol the zero based column index.
 *
 * @return type code or -1 if the column is illegal.
 */

short CPLODBCStatement::GetColType( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return -1;

    return m_panColType[iCol];
}

/************************************************************************/
/*                             GetColTypeName()                         */
/************************************************************************/

/**
 * Fetch a column data type name.
 *
 * Returns data source-dependent data type name; for example, "CHAR",
 * "VARCHAR", "MONEY", "LONG VARBINAR", or "CHAR ( ) FOR BIT DATA".
 *
 * @param iCol the zero based column index.
 *
 * @return NULL on failure (out of bounds column), or a pointer to an
 * internal copy of the column dat type name.
 */

const char *CPLODBCStatement::GetColTypeName( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return NULL;

    return m_papszColTypeNames[iCol];
}

/************************************************************************/
/*                             GetColSize()                             */
/************************************************************************/

/**
 * Fetch the column width.
 *
 * @param iCol the zero based column index.
 *
 * @return column width, zero for unknown width columns.
 */

short CPLODBCStatement::GetColSize( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return -1;

    return static_cast<short>(m_panColSize[iCol]);
}

/************************************************************************/
/*                          GetColPrecision()                           */
/************************************************************************/

/**
 * Fetch the column precision.
 *
 * @param iCol the zero based column index.
 *
 * @return column precision, may be zero or the same as column size for
 * columns to which it does not apply.
 */

short CPLODBCStatement::GetColPrecision( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return -1;

    return m_panColPrecision[iCol];
}

/************************************************************************/
/*                           GetColNullable()                           */
/************************************************************************/

/**
 * Fetch the column nullability.
 *
 * @param iCol the zero based column index.
 *
 * @return TRUE if the column may contains or FALSE otherwise.
 */

short CPLODBCStatement::GetColNullable( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return -1;

    return m_panColNullable[iCol];
}

/************************************************************************/
/*                             GetColColumnDef()                        */
/************************************************************************/

/**
 * Fetch a column default value.
 *
 * Returns the default value of a column.
 *
 * @param iCol the zero based column index.
 *
 * @return NULL if the default value is not dpecified
 * or the internal copy of the default value.
 */

const char *CPLODBCStatement::GetColColumnDef( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return NULL;

    return m_papszColColumnDef[iCol];
}

/************************************************************************/
/*                               Fetch()                                */
/************************************************************************/

/**
 * Fetch a new record.
 *
 * Requests the next row in the current resultset using the SQLFetchScroll()
 * call.  Note that many ODBC drivers only support the default forward
 * fetching one record at a time.  Only SQL_FETCH_NEXT (the default) should
 * be considered reliable on all drivers.
 *
 * Currently it isn't clear how to determine whether an error or a normal
 * out of data condition has occurred if Fetch() fails.
 *
 * @param nOrientation One of SQL_FETCH_NEXT, SQL_FETCH_LAST, SQL_FETCH_PRIOR,
 * SQL_FETCH_ABSOLUTE, or SQL_FETCH_RELATIVE (default is SQL_FETCH_NEXT).
 *
 * @param nOffset the offset (number of records), ignored for some
 * orientations.
 *
 * @return TRUE if a new row is successfully fetched, or FALSE if not.
 */

int CPLODBCStatement::Fetch( int nOrientation, int nOffset )

{
    ClearColumnData();

    if( m_hStmt == NULL || m_nColCount < 1 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Fetch a new row.  Note that some brain dead drives (such as     */
/*      the unixodbc text file driver) don't implement                  */
/*      SQLScrollFetch(), so we try to stick to SQLFetch() if we        */
/*      can).                                                           */
/* -------------------------------------------------------------------- */
    SQLRETURN nRetCode;

    if( nOrientation == SQL_FETCH_NEXT && nOffset == 0 )
    {
        nRetCode = SQLFetch( m_hStmt );
        if( Failed(nRetCode) )
        {
            if( nRetCode != SQL_NO_DATA )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "%s",
                          m_poSession->GetLastError() );
            }
            return FALSE;
        }
    }
    else
    {
        nRetCode = SQLFetchScroll(m_hStmt,
                                  static_cast<SQLSMALLINT>(nOrientation),
                                  nOffset);
        if( Failed(nRetCode) )
        {
            if( nRetCode == SQL_NO_DATA )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "%s",
                          m_poSession->GetLastError() );
            }
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Pull out all the column values.                                 */
/* -------------------------------------------------------------------- */
    for( SQLSMALLINT iCol = 0; iCol < m_nColCount; iCol++ )
    {
        CPL_SQLLEN cbDataLen = 0;
        SQLSMALLINT nFetchType = GetTypeMapping( m_panColType[iCol] );

        // Handle values other than WCHAR and BINARY as CHAR.
        if( nFetchType != SQL_C_BINARY && nFetchType != SQL_C_WCHAR )
            nFetchType = SQL_C_CHAR;

        char szWrkData[513] = {};

        nRetCode = SQLGetData( m_hStmt, iCol + 1, nFetchType,
                               szWrkData, sizeof(szWrkData) - 1,
                               &cbDataLen );

        // SQLGetData() is giving garbage values in the first 4 bytes of
        // cbDataLen in some architectures. Converting it to int discards the
        // unnecessary bytes. This should not be a problem unless the buffer
        // size reaches 2GB. (#3385)
        cbDataLen = static_cast<int>(cbDataLen);

        if( Failed( nRetCode ) )
        {
            if( nRetCode == SQL_NO_DATA )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "%s",
                          m_poSession->GetLastError() );
            }
            return FALSE;
        }

        if( cbDataLen == SQL_NULL_DATA )
        {
            m_papszColValues[iCol] = NULL;
            m_panColValueLengths[iCol] = 0;
        }

        // Assume big result: should check for state=SQLSATE 01004.
        else if( nRetCode == SQL_SUCCESS_WITH_INFO )
        {
            if( cbDataLen >= (CPL_SQLLEN)(sizeof(szWrkData)-1) )
            {
                cbDataLen = (CPL_SQLLEN)(sizeof(szWrkData)-1);
                if( nFetchType == SQL_C_CHAR )
                    while( (cbDataLen > 1) && (szWrkData[cbDataLen - 1] == 0) )
                        --cbDataLen;  // Trimming the extra terminators: bug 990
                else if( nFetchType == SQL_C_WCHAR )
                    while( (cbDataLen > 1) && (szWrkData[cbDataLen - 1] == 0 )
                        && (szWrkData[cbDataLen - 2] == 0))
                        cbDataLen -= 2;  // Trimming the extra terminators.
            }

            m_papszColValues[iCol] =
                static_cast<char *>(CPLMalloc(cbDataLen + 2));
            memcpy( m_papszColValues[iCol], szWrkData, cbDataLen );
            m_papszColValues[iCol][cbDataLen] = '\0';
            m_papszColValues[iCol][cbDataLen+1] = '\0';
            m_panColValueLengths[iCol] = cbDataLen;

            while( true )
            {
                nRetCode = SQLGetData( m_hStmt,
                                       static_cast<SQLUSMALLINT>(iCol) + 1,
                                       nFetchType,
                                       szWrkData, sizeof(szWrkData)-1,
                                       &cbDataLen );
                if( nRetCode == SQL_NO_DATA )
                    break;

                if( Failed( nRetCode ) )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "%s",
                              m_poSession->GetLastError() );
                    return FALSE;
                }

                CPL_SQLLEN nChunkLen;
                if( cbDataLen >= static_cast<int>(sizeof(szWrkData) - 1)
                    || cbDataLen == SQL_NO_TOTAL )
                {
                    nChunkLen = sizeof(szWrkData)-1;
                    if( nFetchType == SQL_C_CHAR )
                        while( (nChunkLen > 1)
                               && (szWrkData[nChunkLen - 1] == 0) )
                            --nChunkLen;  // Trimming the extra terminators.
                    else if( nFetchType == SQL_C_WCHAR )
                        while( (nChunkLen > 1)
                               && (szWrkData[nChunkLen - 1] == 0)
                               && (szWrkData[nChunkLen - 2] == 0) )
                            nChunkLen -= 2;  // Trimming the extra terminators.
                }
                else
                {
                    nChunkLen = cbDataLen;
                }
                szWrkData[nChunkLen] = '\0';

                m_papszColValues[iCol] = static_cast<char *>(
                    CPLRealloc( m_papszColValues[iCol],
                                m_panColValueLengths[iCol] + nChunkLen + 2 ));
                memcpy( m_papszColValues[iCol] + m_panColValueLengths[iCol],
                        szWrkData, nChunkLen );
                m_panColValueLengths[iCol] += nChunkLen;
                m_papszColValues[iCol][m_panColValueLengths[iCol]] = '\0';
                m_papszColValues[iCol][m_panColValueLengths[iCol]+1] = '\0';
            }
        }
        else
        {
            m_panColValueLengths[iCol] = cbDataLen;
            m_papszColValues[iCol] =
                static_cast<char *>(CPLMalloc(cbDataLen + 2));
            memcpy( m_papszColValues[iCol], szWrkData, cbDataLen );
            m_papszColValues[iCol][cbDataLen] = '\0';
            m_papszColValues[iCol][cbDataLen+1] = '\0';
        }

        // Trim white space off end, if there is any.
        if( nFetchType == SQL_C_CHAR && m_papszColValues[iCol] != NULL )
        {
            char *pszTarget = m_papszColValues[iCol];
            size_t iEnd = strlen(pszTarget);

            while( iEnd > 0 && pszTarget[iEnd - 1] == ' ' )
                pszTarget[--iEnd] = '\0';
        }

        // Convert WCHAR to UTF-8, assuming the WCHAR is UCS-2.
        if( nFetchType == SQL_C_WCHAR && m_papszColValues[iCol] != NULL
            && m_panColValueLengths[iCol] > 0 )
        {
            wchar_t *pwszSrc = (wchar_t *) m_papszColValues[iCol];

            m_papszColValues[iCol] =
                CPLRecodeFromWChar( pwszSrc, CPL_ENC_UCS2, CPL_ENC_UTF8 );
            m_panColValueLengths[iCol] = strlen(m_papszColValues[iCol]);

            CPLFree( pwszSrc );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                             GetColData()                             */
/************************************************************************/

/**
 * Fetch column data.
 *
 * Fetches the data contents of the requested column for the currently loaded
 * row.  The result is returned as a string regardless of the column type.
 * NULL is returned if an illegal column is given, or if the actual column
 * is "NULL".
 *
 * @param iCol the zero based column to fetch.
 *
 * @param pszDefault the value to return if the column does not exist, or is
 * NULL.  Defaults to NULL.
 *
 * @return pointer to internal column data or NULL on failure.
 */

const char *CPLODBCStatement::GetColData( int iCol, const char *pszDefault )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return pszDefault;
    else if( m_papszColValues[iCol] != NULL )
        return m_papszColValues[iCol];
    else
        return pszDefault;
}

/************************************************************************/
/*                             GetColData()                             */
/************************************************************************/

/**
 * Fetch column data.
 *
 * Fetches the data contents of the requested column for the currently loaded
 * row.  The result is returned as a string regardless of the column type.
 * NULL is returned if an illegal column is given, or if the actual column
 * is "NULL".
 *
 * @param pszColName the name of the column requested.
 *
 * @param pszDefault the value to return if the column does not exist, or is
 * NULL.  Defaults to NULL.
 *
 * @return pointer to internal column data or NULL on failure.
 */

const char *CPLODBCStatement::GetColData( const char *pszColName,
                                          const char *pszDefault )

{
    const int iCol = GetColId( pszColName );

    if( iCol == -1 )
        return pszDefault;
    else
        return GetColData( iCol, pszDefault );
}

/************************************************************************/
/*                          GetColDataLength()                          */
/************************************************************************/

/** GetColDataLength */
int CPLODBCStatement::GetColDataLength( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return 0;
    else if( m_papszColValues[iCol] != NULL )
        return static_cast<int>(m_panColValueLengths[iCol]);
    else
        return 0;
}

/************************************************************************/
/*                              GetColId()                              */
/************************************************************************/

/**
 * Fetch column index.
 *
 * Gets the column index corresponding with the passed name.  The
 * name comparisons are case insensitive.
 *
 * @param pszColName the name to search for.
 *
 * @return the column index, or -1 if not found.
 */

int CPLODBCStatement::GetColId( const char *pszColName )

{
    for( SQLSMALLINT iCol = 0; iCol < m_nColCount; iCol++ )
        if( EQUAL(pszColName, m_papszColNames[iCol]) )
            return iCol;

    return -1;
}

/************************************************************************/
/*                          ClearColumnData()                           */
/************************************************************************/

/** ClearColumnData */
void CPLODBCStatement::ClearColumnData()

{
    if( m_nColCount > 0 )
    {
        for( int iCol = 0; iCol < m_nColCount; iCol++ )
        {
            if( m_papszColValues[iCol] != NULL )
            {
                CPLFree( m_papszColValues[iCol] );
                m_papszColValues[iCol] = NULL;
            }
        }
    }
}

/************************************************************************/
/*                               Failed()                               */
/************************************************************************/

/** Failed */
int CPLODBCStatement::Failed( int nResultCode )

{
    if( m_poSession != NULL )
        return m_poSession->Failed( nResultCode, m_hStmt );

    return TRUE;
}

/************************************************************************/
/*                         Append(const char *)                         */
/************************************************************************/

/**
 * Append text to internal command.
 *
 * The passed text is appended to the internal SQL command text.
 *
 * @param pszText text to append.
 */

void CPLODBCStatement::Append( const char *pszText )

{
    const size_t nTextLen = strlen(pszText);

    if( m_nStatementMax < m_nStatementLen + nTextLen + 1 )
    {
        m_nStatementMax = (m_nStatementLen + nTextLen) * 2 + 100;
        if( m_pszStatement == NULL )
        {
            m_pszStatement = static_cast<char *>(VSIMalloc(m_nStatementMax));
            m_pszStatement[0] = '\0';
        }
        else
        {
            m_pszStatement = static_cast<char *>(
                CPLRealloc(m_pszStatement, m_nStatementMax));
        }
    }

    strcpy( m_pszStatement + m_nStatementLen, pszText );
    m_nStatementLen += nTextLen;
}

/************************************************************************/
/*                     AppendEscaped(const char *)                      */
/************************************************************************/

/**
 * Append text to internal command.
 *
 * The passed text is appended to the internal SQL command text after
 * escaping any special characters so it can be used as a character string
 * in an SQL statement.
 *
 * @param pszText text to append.
 */

void CPLODBCStatement::AppendEscaped( const char *pszText )

{
    const size_t nTextLen = strlen(pszText);
    char *pszEscapedText = static_cast<char *>(VSIMalloc(nTextLen*2 + 1));

    size_t iOut = 0;  // Used after for.
    for( size_t iIn = 0; iIn < nTextLen; iIn++ )
    {
        switch( pszText[iIn] )
        {
            case '\'':
            case '\\':
                pszEscapedText[iOut++] = '\\';
                pszEscapedText[iOut++] = pszText[iIn];
                break;

            default:
                pszEscapedText[iOut++] = pszText[iIn];
                break;
        }
    }

    pszEscapedText[iOut] = '\0';

    Append( pszEscapedText );
    CPLFree( pszEscapedText );
}

/************************************************************************/
/*                             Append(int)                              */
/************************************************************************/

/**
 * Append to internal command.
 *
 * The passed value is formatted and appended to the internal SQL command text.
 *
 * @param nValue value to append to the command.
 */

void CPLODBCStatement::Append( int nValue )

{
    char szFormattedValue[32] = {};

    snprintf( szFormattedValue, sizeof(szFormattedValue), "%d", nValue );
    Append( szFormattedValue );
}

/************************************************************************/
/*                            Append(double)                            */
/************************************************************************/

/**
 * Append to internal command.
 *
 * The passed value is formatted and appended to the internal SQL command text.
 *
 * @param dfValue value to append to the command.
 */

void CPLODBCStatement::Append( double dfValue )

{
    char szFormattedValue[100] = {};

    snprintf( szFormattedValue, sizeof(szFormattedValue), "%24g", dfValue );
    Append( szFormattedValue );
}

/************************************************************************/
/*                              Appendf()                               */
/************************************************************************/

/**
 * Append to internal command.
 *
 * The passed format is used to format other arguments and the result is
 * appended to the internal command text.  Long results may not be formatted
 * properly, and should be appended with the direct Append() methods.
 *
 * @param pszFormat printf() style format string.
 *
 * @return FALSE if formatting fails due to result being too large.
 */

int CPLODBCStatement::Appendf( CPL_FORMAT_STRING(const char *pszFormat), ... )

{
    va_list args;

    va_start( args, pszFormat );

    char szFormattedText[8000] = {};  // TODO: Move this off the stack.
    szFormattedText[0] = '\0';

#if defined(HAVE_VSNPRINTF)
    const bool bSuccess =
        vsnprintf( szFormattedText, sizeof(szFormattedText)-1,
                   pszFormat, args )
        < static_cast<int>( sizeof(szFormattedText) - 1 );
#else
    vsprintf( szFormattedText, pszFormat, args );
    const bool bSuccess = true;
#endif
    va_end( args );

    if( bSuccess )
        Append( szFormattedText );

    return bSuccess;
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

/**
 * Clear internal command text and result set definitions.
 */

void CPLODBCStatement::Clear()

{
    /* Closing the cursor if opened */
    if( m_hStmt != NULL )
        SQLFreeStmt( m_hStmt, SQL_CLOSE );

    ClearColumnData();

    if( m_pszStatement != NULL )
    {
        CPLFree( m_pszStatement );
        m_pszStatement = NULL;
    }

    m_nStatementLen = 0;
    m_nStatementMax = 0;

    m_nColCount = 0;

    if( m_papszColNames )
    {
        CPLFree( m_panColType );
        m_panColType = NULL;

        CSLDestroy( m_papszColTypeNames );
        m_papszColTypeNames = NULL;

        CPLFree( m_panColSize );
        m_panColSize = NULL;

        CPLFree( m_panColPrecision );
        m_panColPrecision = NULL;

        CPLFree( m_panColNullable );
        m_panColNullable = NULL;

        CSLDestroy( m_papszColColumnDef );
        m_papszColColumnDef = NULL;

        CSLDestroy( m_papszColNames );
        m_papszColNames = NULL;

        CPLFree( m_papszColValues );
        m_papszColValues = NULL;

        CPLFree( m_panColValueLengths );
        m_panColValueLengths = NULL;
    }
}

/************************************************************************/
/*                             GetColumns()                             */
/************************************************************************/

/**
 * Fetch column definitions for a table.
 *
 * The SQLColumn() method is used to fetch the definitions for the columns
 * of a table (or other queryable object such as a view).  The column
 * definitions are digested and used to populate the CPLODBCStatement
 * column definitions essentially as if a "SELECT * FROM tablename" had
 * been done; however, no resultset will be available.
 *
 * @param pszTable the name of the table to query information on.  This
 * should not be empty.
 *
 * @param pszCatalog the catalog to find the table in, use NULL (the
 * default) if no catalog is available.
 *
 * @param pszSchema the schema to find the table in, use NULL (the
 * default) if no schema is available.
 *
 * @return TRUE on success or FALSE on failure.
 */

int CPLODBCStatement::GetColumns( const char *pszTable,
                                  const char *pszCatalog,
                                  const char *pszSchema )

{
#ifdef notdef
    if( pszCatalog == NULL )
        pszCatalog = "";
    if( pszSchema == NULL )
        pszSchema = "";
#endif

#if (ODBCVER >= 0x0300)

    if( !m_poSession->IsInTransaction() )
    {
        /* commit pending transactions and set to autocommit mode*/
        m_poSession->ClearTransaction();
    }

#endif
/* -------------------------------------------------------------------- */
/*      Fetch columns resultset for this table.                         */
/* -------------------------------------------------------------------- */
    if( Failed( SQLColumns( m_hStmt,
                            (SQLCHAR *) pszCatalog, SQL_NTS,
                            (SQLCHAR *) pszSchema, SQL_NTS,
                            (SQLCHAR *) pszTable, SQL_NTS,
                            (SQLCHAR *) NULL /* "" */, SQL_NTS ) ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Allocate per column information.                                */
/* -------------------------------------------------------------------- */
#ifdef notdef
    // SQLRowCount() is too unreliable (with unixodbc on AIX for instance)
    // so we now avoid it.
    SQLINTEGER nResultCount = 0;

    if( Failed(SQLRowCount( m_hStmt, &nResultCount ) ) )
        nResultCount = 0;

    if( nResultCount < 1 )
        m_nColCount = 500; // Hopefully lots.
    else
        m_nColCount = nResultCount;
#endif

    m_nColCount = 500;

    m_papszColNames =
        static_cast<char **>(CPLCalloc(sizeof(char *), m_nColCount + 1));
    m_papszColValues =
        static_cast<char **>(CPLCalloc(sizeof(char *), m_nColCount + 1));

    m_panColType =
        static_cast<SQLSMALLINT *>(CPLCalloc(sizeof(SQLSMALLINT), m_nColCount));
    m_papszColTypeNames =
        static_cast<char **>(CPLCalloc(sizeof(char *), m_nColCount + 1));
    m_panColSize =
        static_cast<CPL_SQLULEN *>(CPLCalloc(sizeof(CPL_SQLULEN), m_nColCount));
    m_panColPrecision =
        static_cast<SQLSMALLINT *>(CPLCalloc(sizeof(SQLSMALLINT), m_nColCount));
    m_panColNullable =
        static_cast<SQLSMALLINT *>(CPLCalloc(sizeof(SQLSMALLINT), m_nColCount));
    m_papszColColumnDef =
        static_cast<char **>(CPLCalloc(sizeof(char *), m_nColCount + 1));

/* -------------------------------------------------------------------- */
/*      Establish columns to use for key information.                   */
/* -------------------------------------------------------------------- */
    for( SQLUSMALLINT iCol = 0; iCol < m_nColCount; iCol++ )
    {
        if( Failed( SQLFetch( m_hStmt ) ) )
        {
            m_nColCount = iCol;
            break;
        }

        char szWrkData[8193] = {};
        CPL_SQLLEN cbDataLen = 0;

        SQLGetData( m_hStmt, SQLColumns_COLUMN_NAME, SQL_C_CHAR,
                    szWrkData, sizeof(szWrkData)-1, &cbDataLen );
        m_papszColNames[iCol] = CPLStrdup(szWrkData);

        SQLGetData( m_hStmt, SQLColumns_DATA_TYPE, SQL_C_CHAR,
                    szWrkData, sizeof(szWrkData)-1, &cbDataLen );
        m_panColType[iCol] = static_cast<short>(atoi(szWrkData));

        SQLGetData( m_hStmt, SQLColumns_TYPE_NAME, SQL_C_CHAR,
                    szWrkData, sizeof(szWrkData)-1, &cbDataLen );
        m_papszColTypeNames[iCol] = CPLStrdup(szWrkData);

        SQLGetData( m_hStmt, SQLColumns_COLUMN_SIZE, SQL_C_CHAR,
                    szWrkData, sizeof(szWrkData)-1, &cbDataLen );
        m_panColSize[iCol] = atoi(szWrkData);

        SQLGetData( m_hStmt, SQLColumns_DECIMAL_DIGITS, SQL_C_CHAR,
                    szWrkData, sizeof(szWrkData)-1, &cbDataLen );
        m_panColPrecision[iCol] = static_cast<short>(atoi(szWrkData));

        SQLGetData( m_hStmt, SQLColumns_NULLABLE, SQL_C_CHAR,
                    szWrkData, sizeof(szWrkData)-1, &cbDataLen );
        m_panColNullable[iCol] = atoi(szWrkData) == SQL_NULLABLE;
#if (ODBCVER >= 0x0300)
        SQLGetData( m_hStmt, SQLColumns_COLUMN_DEF, SQL_C_CHAR,
                    szWrkData, sizeof(szWrkData)-1, &cbDataLen );
        if( cbDataLen > 0 )
            m_papszColColumnDef[iCol] = CPLStrdup(szWrkData);
#endif
    }

    return TRUE;
}

/************************************************************************/
/*                           GetPrimaryKeys()                           */
/************************************************************************/

/**
 * Fetch primary keys for a table.
 *
 * The SQLPrimaryKeys() function is used to fetch a list of fields
 * forming the primary key.  The result is returned as a result set matching
 * the SQLPrimaryKeys() function result set.  The 4th column in the result
 * set is the column name of the key, and if the result set contains only
 * one record then that single field will be the complete primary key.
 *
 * @param pszTable the name of the table to query information on.  This
 * should not be empty.
 *
 * @param pszCatalog the catalog to find the table in, use NULL (the
 * default) if no catalog is available.
 *
 * @param pszSchema the schema to find the table in, use NULL (the
 * default) if no schema is available.
 *
 * @return TRUE on success or FALSE on failure.
 */

int CPLODBCStatement::GetPrimaryKeys( const char *pszTable,
                                      const char *pszCatalog,
                                      const char *pszSchema )

{
    if( pszCatalog == NULL )
        pszCatalog = "";
    if( pszSchema == NULL )
        pszSchema = "";

#if (ODBCVER >= 0x0300)

    if( !m_poSession->IsInTransaction() )
    {
        /* commit pending transactions and set to autocommit mode*/
        m_poSession->ClearTransaction();
    }

#endif

/* -------------------------------------------------------------------- */
/*      Fetch columns resultset for this table.                         */
/* -------------------------------------------------------------------- */
    if( Failed( SQLPrimaryKeys( m_hStmt,
                                (SQLCHAR *) pszCatalog, SQL_NTS,
                                (SQLCHAR *) pszSchema, SQL_NTS,
                                (SQLCHAR *) pszTable, SQL_NTS ) ) )
        return FALSE;

    return CollectResultsInfo();
}

/************************************************************************/
/*                             GetTables()                              */
/************************************************************************/

/**
 * Fetch tables in database.
 *
 * The SQLTables() function is used to fetch a list tables in the
 * database.    The result is returned as a result set matching
 * the SQLTables() function result set.  The 3rd column in the result
 * set is the table name.  Only tables of type "TABLE" are returned.
 *
 * @param pszCatalog the catalog to find the table in, use NULL (the
 * default) if no catalog is available.
 *
 * @param pszSchema the schema to find the table in, use NULL (the
 * default) if no schema is available.
 *
 * @return TRUE on success or FALSE on failure.
 */

int CPLODBCStatement::GetTables( const char *pszCatalog,
                                 const char *pszSchema )

{
    CPLDebug( "ODBC", "CatalogNameL: %s\nSchema name: %s",
                pszCatalog, pszSchema );

#if (ODBCVER >= 0x0300)

    if( !m_poSession->IsInTransaction() )
    {
        // Commit pending transactions and set to autocommit mode.
        m_poSession->ClearTransaction();
    }

#endif

/* -------------------------------------------------------------------- */
/*      Fetch columns resultset for this table.                         */
/* -------------------------------------------------------------------- */
    if( Failed( SQLTables( m_hStmt,
                           (SQLCHAR *) pszCatalog, SQL_NTS,
                           (SQLCHAR *) pszSchema, SQL_NTS,
                           (SQLCHAR *) NULL, SQL_NTS,
                           (SQLCHAR *) "'TABLE','VIEW'", SQL_NTS ) ) )
        return FALSE;

    return CollectResultsInfo();
}

/************************************************************************/
/*                             DumpResult()                             */
/************************************************************************/

/**
 * Dump resultset to file.
 *
 * The contents of the current resultset are dumped in a simply formatted
 * form to the provided file.  If requested, the schema definition will
 * be written first.
 *
 * @param fp the file to write to.  stdout or stderr are acceptable.
 *
 * @param bShowSchema TRUE to force writing schema information for the rowset
 * before the rowset data itself.  Default is FALSE.
 */

void CPLODBCStatement::DumpResult( FILE *fp, int bShowSchema )

{
/* -------------------------------------------------------------------- */
/*      Display schema                                                  */
/* -------------------------------------------------------------------- */
    if( bShowSchema )
    {
        fprintf( fp, "Column Definitions:\n" );
        for( int iCol = 0; iCol < GetColCount(); iCol++ )
        {
            fprintf( fp, " %2d: %-24s ", iCol, GetColName(iCol) );
            if( GetColPrecision(iCol) > 0
                && GetColPrecision(iCol) != GetColSize(iCol) )
                fprintf( fp, " Size:%3d.%d",
                         GetColSize(iCol), GetColPrecision(iCol) );
            else
                fprintf( fp, " Size:%5d", GetColSize(iCol) );

            CPLString osType = GetTypeName( GetColType(iCol) );
            fprintf( fp, " Type:%s", osType.c_str() );
            if( GetColNullable(iCol) )
                fprintf( fp, " NULLABLE" );
            fprintf( fp, "\n" );
        }
        fprintf( fp, "\n" );
    }

/* -------------------------------------------------------------------- */
/*      Display results                                                 */
/* -------------------------------------------------------------------- */
    int iRecord = 0;
    while( Fetch() )
    {
        fprintf( fp, "Record %d\n", iRecord++ );

        for( int iCol = 0; iCol < GetColCount(); iCol++ )
        {
            fprintf( fp, "  %s: %s\n", GetColName(iCol), GetColData(iCol) );
        }
    }
}

/************************************************************************/
/*                            GetTypeName()                             */
/************************************************************************/

/**
 * Get name for SQL column type.
 *
 * Returns a string name for the indicated type code (as returned
 * from CPLODBCStatement::GetColType()).
 *
 * @param nTypeCode the SQL_ code, such as SQL_CHAR.
 *
 * @return internal string, "UNKNOWN" if code not recognised.
 */

CPLString CPLODBCStatement::GetTypeName( int nTypeCode )

{
    switch( nTypeCode )
    {
      case SQL_CHAR:
        return "CHAR";

      case SQL_NUMERIC:
        return "NUMERIC";

      case SQL_DECIMAL:
        return "DECIMAL";

      case SQL_INTEGER:
        return "INTEGER";

      case SQL_SMALLINT:
        return "SMALLINT";

      case SQL_FLOAT:
        return "FLOAT";

      case SQL_REAL:
        return "REAL";

      case SQL_DOUBLE:
        return "DOUBLE";

      case SQL_DATETIME:
        return "DATETIME";

      case SQL_VARCHAR:
        return "VARCHAR";

      case SQL_TYPE_DATE:
        return "DATE";

      case SQL_TYPE_TIME:
        return "TIME";

      case SQL_TYPE_TIMESTAMP:
        return "TIMESTAMP";

      default:
        CPLString osResult;
        osResult.Printf( "UNKNOWN:%d", nTypeCode );
        return osResult;
    }
}

/************************************************************************/
/*                            GetTypeMapping()                          */
/************************************************************************/

/**
 * Get appropriate C data type for SQL column type.
 *
 * Returns a C data type code, corresponding to the indicated SQL data
 * type code (as returned from CPLODBCStatement::GetColType()).
 *
 * @param nTypeCode the SQL_ code, such as SQL_CHAR.
 *
 * @return data type code. The valid code is always returned. If SQL
 * code is not recognised, SQL_C_BINARY will be returned.
 */

SQLSMALLINT CPLODBCStatement::GetTypeMapping( SQLSMALLINT nTypeCode )

{
    switch( nTypeCode )
    {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
            return SQL_C_CHAR;

        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
            return SQL_C_WCHAR;

        case SQL_DECIMAL:
        case SQL_NUMERIC:
            return SQL_C_NUMERIC;

        case SQL_SMALLINT:
            return SQL_C_SSHORT;

        case SQL_INTEGER:
            return SQL_C_SLONG;

        case SQL_REAL:
            return SQL_C_FLOAT;

        case SQL_FLOAT:
        case SQL_DOUBLE:
            return SQL_C_DOUBLE;

        case SQL_BIGINT:
            return SQL_C_SBIGINT;

        case SQL_BIT:
        case SQL_TINYINT:
        // case SQL_TYPE_UTCDATETIME:
        // case SQL_TYPE_UTCTIME:
        case SQL_INTERVAL_MONTH:
        case SQL_INTERVAL_YEAR:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY:
        case SQL_INTERVAL_HOUR:
        case SQL_INTERVAL_MINUTE:
        case SQL_INTERVAL_SECOND:
        case SQL_INTERVAL_DAY_TO_HOUR:
        case SQL_INTERVAL_DAY_TO_MINUTE:
        case SQL_INTERVAL_DAY_TO_SECOND:
        case SQL_INTERVAL_HOUR_TO_MINUTE:
        case SQL_INTERVAL_HOUR_TO_SECOND:
        case SQL_INTERVAL_MINUTE_TO_SECOND:
        case SQL_GUID:
            return SQL_C_CHAR;

        case SQL_DATE:
        case SQL_TYPE_DATE:
            return SQL_C_DATE;

        case SQL_TIME:
        case SQL_TYPE_TIME:
            return SQL_C_TIME;

        case SQL_TIMESTAMP:
        case SQL_TYPE_TIMESTAMP:
            return SQL_C_TIMESTAMP;

        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
        case -151:  // SQL_SS_UDT
            return SQL_C_BINARY;

        default:
            return SQL_C_CHAR;
    }
}
