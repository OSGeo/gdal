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
 * Revision 1.32  2006/06/06 16:25:22  mloskot
 * Fixed memory 4-5 leaks in CPL ODBC and OGR drivers.
 *
 * Revision 1.31  2006/06/05 20:15:52  mloskot
 * Fixed problem with /odbcinst.ini requirements.
 *
 * Revision 1.30  2006/06/05 18:53:28  mloskot
 * Added usage of ODBCSYSINI env to CPLODBCDriverInstaller.
 *
 * Revision 1.29  2006/06/05 15:52:29  mloskot
 * Added option to the ODBC wrapper to list views together with tables.
 *
 * Revision 1.28  2006/06/01 12:15:39  mloskot
 * Added CPLODBCDriverInstaller utility class to CPL.
 *
 * Revision 1.27  2006/02/19 21:54:34  mloskot
 * [WINCE] Changes related to Windows CE port of CPL. Most changes are #ifdef wrappers.
 *
 * Revision 1.26  2006/01/31 03:04:27  fwarmerdam
 * Fixed null trimming to work for long unicode text strings per:
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=990
 *
 * Revision 1.25  2005/09/24 04:58:16  fwarmerdam
 * Support SQLDriverConnect (pass opts) in EstablishSession
 *
 * Revision 1.24  2005/09/05 20:18:43  fwarmerdam
 * added binary column support
 *
 * Revision 1.23  2005/08/31 03:32:41  fwarmerdam
 * GetTypeName now returns CPLString
 *
 * Revision 1.22  2005/08/07 14:05:44  fwarmerdam
 * added connect/disconnect debug statements
 *
 * Revision 1.21  2005/06/29 01:01:01  ssoule
 * Changed return type of CPLODBCStatement::GetTypeName from const char * to
 * std::string.
 *
 * Revision 1.20  2005/05/23 03:56:44  fwarmerdam
 * make make static buffers threadlocal
 *
 * Revision 1.19  2005/01/12 07:51:29  fwarmerdam
 * Fix some argument types needed for ODBC 3.52. In particular SQLDescribeCol
 * and SQLGetData require SQLLEN and SQLULEN instead of SQLINTEGER and
 * SQLUINTEGER.  Patches provided by David Herring.
 *
 * Revision 1.18  2004/08/18 18:47:12  warmerda
 * Increase size of work buffer a bit so that providers with broken support
 * for multiple SQLGetData() calls will still mostly work.
 *
 * Revision 1.17  2004/08/17 21:51:16  warmerda
 * Changed Fetch() to handle values longer than the working buffer
 * properly ... do extra SQLGetData() calls till complete.
 *
 * Revision 1.16  2004/08/17 20:15:56  warmerda
 * Allow column values up to 64K instead of limited to 8K.
 *
 * Revision 1.15  2004/06/17 17:11:51  warmerda
 * fixed case where vsnprintf does not exist
 *
 * Revision 1.14  2004/06/01 20:40:02  warmerda
 * expanded tabs
 *
 * Revision 1.13  2004/03/30 22:26:58  warmerda
 * Avoid use of SQLRowCount() in GetColumns().  It does not work reliably
 * with Oracle unixodbc driver on AIX.  Argg.
 *
 * Revision 1.12  2004/03/04 16:58:05  warmerda
 * Fixed up memory leak of results set column definition info.
 *
 * Revision 1.11  2004/03/04 05:41:21  warmerda
 * Modified Fetch() method to use SQLFetch() for cases it would be
 * sufficient for.  The SQLScrollFetch() isn't implemented on some brain
 * dead drivers, such as the unixODBC text file driver.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=463
 *
 * Revision 1.10  2003/11/24 20:48:08  warmerda
 * pass NULLs t SQLColumns, not empty strings
 *
 * Revision 1.9  2003/11/24 20:32:07  warmerda
 * Use TABLE instead of TABLES in SQLTables() call
 *
 * Revision 1.8  2003/11/10 20:08:12  warmerda
 * added GetTables() implementation
 *
 * Revision 1.7  2003/10/29 17:56:57  warmerda
 * Added PrimaryKeys() support
 *
 * Revision 1.6  2003/10/06 20:04:08  warmerda
 * added escaping support
 *
 * Revision 1.5  2003/10/06 17:17:02  warmerda
 * fixed some type issues
 *
 * Revision 1.4  2003/09/26 20:02:41  warmerda
 * update GetColData()
 *
 * Revision 1.3  2003/09/26 13:51:02  warmerda
 * Add documentation
 *
 * Revision 1.2  2003/09/25 17:09:49  warmerda
 * added some more methods
 *
 * Revision 1.1  2003/09/24 15:38:27  warmerda
 * New
 *
 */

#include "cpl_odbc.h"
#include "cpl_vsi.h"
#include "cpl_string.h"
#include "cpl_error.h"


#ifndef WIN32CE /* ODBC is not supported on Windows CE. */

CPL_CVSID("$Id$");

/************************************************************************/
/*                           CPLODBCDriverInstaller()                   */
/************************************************************************/

CPLODBCDriverInstaller::CPLODBCDriverInstaller()
    : m_nUsageCount(0)
{
    memset( m_szPathOut, '\0', ODBC_FILENAME_MAX );
    memset( m_szError, '\0', SQL_MAX_MESSAGE_LENGTH );
}

/************************************************************************/
/*                           InstallDriver()                            */
/************************************************************************/

int CPLODBCDriverInstaller::InstallDriver( const char* pszDriver,
        const char* pszPathIn, WORD fRequest )
{
    CPLAssert( NULL != pszDriver ); 

    // Try to install driver to system-wide location
    if ( FALSE == SQLInstallDriverEx( pszDriver, NULL, m_szPathOut,
                    ODBC_FILENAME_MAX, NULL, fRequest,
                    &m_nUsageCount ) )
    {
        const WORD nErrorNum = 1; // TODO - a function param?
        RETCODE cRet = SQL_ERROR;
        
        // Failure is likely related to no write permissions to
        // system-wide default location, so try to install to HOME
       
        // Read HOME location
        char* pszEnvHome = NULL;
        pszEnvHome = getenv("HOME");

        CPLAssert( NULL != pszEnvHome );
        CPLDebug( "ODBC", "HOME=%s", pszEnvHome );

        // Set ODBCSYSINI variable pointing to HOME location
        char* pszEnvIni = (char *)CPLMalloc( strlen(pszEnvHome) + 12 );

        sprintf( pszEnvIni, "ODBCSYSINI=%s", pszEnvHome );
        putenv( pszEnvIni );

        CPLDebug( "ODBC", pszEnvIni );
        //CPLFree( pszEnvIni );
        
        // Try to install ODBC driver in new location
        if ( FALSE == SQLInstallDriverEx( pszDriver, NULL, m_szPathOut,
                ODBC_FILENAME_MAX, NULL, fRequest,
                &m_nUsageCount ) )
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

int CPLODBCDriverInstaller::RemoveDriver( const char* pszDriverName, int fRemoveDSN )
{
    CPLAssert( NULL != pszDriverName ); 

    if ( FALSE == SQLRemoveDriver( pszDriverName, fRemoveDSN, &m_nUsageCount ) )
    {
         const WORD nErrorNum = 1; // TODO - a function param?

        // Retrieve error code and message
        RETCODE cRet = SQLInstallerError( nErrorNum, &m_nErrorCode,
                        m_szError, SQL_MAX_MESSAGE_LENGTH, NULL );

        CPLAssert( SQL_SUCCESS == cRet || SQL_SUCCESS_WITH_INFO == cRet );

        return FALSE;
   }

    // SUCCESS
    return TRUE;
}

/************************************************************************/
/*                           CPLODBCSession()                           */
/************************************************************************/

CPLODBCSession::CPLODBCSession()

{
    m_szLastError[0] = '\0';
    m_hEnv = NULL;
    m_hDBC = NULL;
}

/************************************************************************/
/*                          ~CPLODBCSession()                           */
/************************************************************************/

CPLODBCSession::~CPLODBCSession()

{
    CloseSession();
}

/************************************************************************/
/*                            CloseSession()                            */
/************************************************************************/

int CPLODBCSession::CloseSession()

{
    if( m_hDBC!=NULL ) 
    {
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
/*                               Failed()                               */
/*                                                                      */
/*      Test if a return code indicates failure, return TRUE if that    */
/*      is the case. Also update error text.                            */
/************************************************************************/

int CPLODBCSession::Failed( int nRetCode, HSTMT hStmt )

{
    SQLCHAR achSQLState[SQL_MAX_MESSAGE_LENGTH];
    SQLINTEGER nNativeError;
    SQLSMALLINT nTextLength=0;

    m_szLastError[0] = '\0';

    if( nRetCode == SQL_SUCCESS || nRetCode == SQL_SUCCESS_WITH_INFO )
        return FALSE;

    SQLError( m_hEnv, m_hDBC, hStmt, achSQLState, &nNativeError,
              (SQLCHAR *) m_szLastError, sizeof(m_szLastError)-1, 
              &nTextLength );
    m_szLastError[nTextLength] = '\0';

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

    SQLSetConnectOption( m_hDBC,SQL_LOGIN_TIMEOUT,5 );

    if( pszUserid == NULL )
        pszUserid = "";
    if( pszPassword == NULL )
        pszPassword = "";

    int bFailed;
    if( strstr(pszDSN,"=") != NULL )
    {
        char szOutConnString[1024];
        SQLSMALLINT nOutConnStringLen;

        CPLDebug( "ODBC", "SQLDriverConnect(%s)", pszDSN );
        bFailed = Failed(
            SQLDriverConnect( m_hDBC, NULL, 
                              (SQLCHAR *) pszDSN, strlen(pszDSN), 
                              (SQLCHAR *) szOutConnString, 
                              sizeof(szOutConnString), 
                              &nOutConnStringLen, SQL_DRIVER_NOPROMPT ) );
    }
    else
    {
        CPLDebug( "ODBC", "SQLConnect(%s)", pszDSN );
        bFailed = Failed(
            SQLConnect( m_hDBC, (SQLCHAR *) pszDSN, SQL_NTS, 
                        (SQLCHAR *) pszUserid, SQL_NTS, 
                        (SQLCHAR *) pszPassword, SQL_NTS ) );
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

CPLODBCStatement::CPLODBCStatement( CPLODBCSession *poSession )

{
    m_poSession = poSession;

    if( Failed( 
            SQLAllocStmt( poSession->GetConnection(), &m_hStmt ) ) )
    {
        m_hStmt = NULL;
        return;
    }

    m_nColCount = 0;
    m_papszColNames = NULL;
    m_panColType = NULL;
    m_panColSize = NULL;
    m_panColPrecision = NULL;
    m_panColNullable = NULL;

    m_papszColValues = NULL;
    m_panColValueLengths = NULL;

    m_pszStatement = NULL;
    m_nStatementMax = 0;
    m_nStatementLen = 0;
}

/************************************************************************/
/*                         ~CPLODBCStatement()                          */
/************************************************************************/

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
        // we should post an error.
        return FALSE;
    }

    if( pszStatement != NULL )
    {
        Clear();
        Append( pszStatement );
    }

    if( Failed( 
            SQLExecDirect( m_hStmt, (SQLCHAR *) m_pszStatement, SQL_NTS ) ) )
        return FALSE;

    return CollectResultsInfo();
}

/************************************************************************/
/*                         CollectResultsInfo()                         */
/************************************************************************/

int CPLODBCStatement::CollectResultsInfo()

{
    if( m_poSession == NULL || m_hStmt == NULL )
    {
        // we should post an error.
        return FALSE;
    }

    if( Failed( SQLNumResultCols(m_hStmt,&m_nColCount) ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Allocate per column information.                                */
/* -------------------------------------------------------------------- */
    m_papszColNames = (char **) VSICalloc(sizeof(char *),(m_nColCount+1));
    m_papszColValues = (char **) VSICalloc(sizeof(char *),(m_nColCount+1));
    m_panColValueLengths = (int *) VSICalloc(sizeof(int),(m_nColCount+1));

    m_panColType = (short *) VSICalloc(sizeof(short),m_nColCount);
    m_panColSize = (_SQLULEN *) VSICalloc(sizeof(_SQLULEN),m_nColCount);
    m_panColPrecision = (short *) VSICalloc(sizeof(short),m_nColCount);
    m_panColNullable = (short *) VSICalloc(sizeof(short),m_nColCount);

/* -------------------------------------------------------------------- */
/*      Fetch column descriptions.                                      */
/* -------------------------------------------------------------------- */
    for( int iCol = 0; iCol < m_nColCount; iCol++ )
    {
        char szColName[256];
        SQLSMALLINT nNameLength;

        if( Failed( 
                SQLDescribeCol( m_hStmt, (SQLUSMALLINT) iCol+1, 
                                (SQLCHAR *) szColName, sizeof(szColName),
                                &nNameLength,
                                m_panColType + iCol,
                                m_panColSize + iCol,
                                m_panColPrecision + iCol,
                                m_panColNullable + iCol )) )
            return FALSE;

        szColName[nNameLength] = '\0';
        m_papszColNames[iCol] = CPLStrdup(szColName);
    }

    return TRUE;
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
    else
        return m_papszColNames[iCol];
}

/************************************************************************/
/*                             GetColType()                             */
/************************************************************************/

/**
 * Fetch a column type.
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
    else
        return m_panColType[iCol];
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
    else
        return (short) m_panColSize[iCol];
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
    else
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
    else
        return m_panColNullable[iCol];
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
 * out of data condition has occured if Fetch() fails. 
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
    if( nOrientation == SQL_FETCH_NEXT && nOffset == 0 )
    {
        if( Failed( SQLFetch( m_hStmt ) ) )
            return FALSE;
    }
    else
    {
        if( Failed( SQLFetchScroll( m_hStmt, (SQLSMALLINT) nOrientation, 
                                    nOffset ) ) )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Pull out all the column values.                                 */
/* -------------------------------------------------------------------- */
    int iCol;
    
    for( iCol = 0; iCol < m_nColCount; iCol++ )
    {
        char szWrkData[512];
        _SQLLEN cbDataLen;
        int nRetCode;
        int nFetchType;

        if( m_panColType[iCol] == SQL_BINARY 
            || m_panColType[iCol] == SQL_VARBINARY 
            || m_panColType[iCol] == SQL_LONGVARBINARY )
        {
            nFetchType = SQL_C_BINARY;
        }
        else
            nFetchType = SQL_C_CHAR;

        szWrkData[0] = '\0';
        szWrkData[sizeof(szWrkData)-1] = '\0';

        nRetCode = SQLGetData( m_hStmt, (SQLUSMALLINT) iCol+1, nFetchType,
                               szWrkData, sizeof(szWrkData)-1, 
                               &cbDataLen );
        if( Failed( nRetCode ) )
            return FALSE;

        if( cbDataLen == SQL_NULL_DATA )
        {
            m_papszColValues[iCol] = NULL;
            m_panColValueLengths[iCol] = 0;
        }

        // assume big result: should check for state=SQLSATE 01004.
        else if( nRetCode == SQL_SUCCESS_WITH_INFO  ) 
        {
            if( cbDataLen > (int) (sizeof(szWrkData)-1) )
            {
                cbDataLen = (int) (sizeof(szWrkData)-1);
                if (nFetchType == SQL_C_CHAR) 
                    while ((cbDataLen > 1) && (szWrkData[cbDataLen - 1] == 0)) 
                        --cbDataLen;  // trimming the extra terminators: bug 990
            }
			
            m_papszColValues[iCol] = (char *) CPLMalloc(cbDataLen+1);
            memcpy( m_papszColValues[iCol], szWrkData, cbDataLen );
            m_papszColValues[iCol][cbDataLen] = '\0';
            m_panColValueLengths[iCol] = cbDataLen;

            while( TRUE )
            {
                int nChunkLen;

                nRetCode = SQLGetData( m_hStmt, (SQLUSMALLINT) iCol+1, 
                                       nFetchType,
                                       szWrkData, sizeof(szWrkData)-1, 
                                       &cbDataLen );
                if( nRetCode == SQL_NO_DATA )
                    break;

                if( Failed( nRetCode ) )
                    return FALSE;

                if( cbDataLen > (int) (sizeof(szWrkData) - 1)
                    || cbDataLen == SQL_NO_TOTAL )
                {
                    nChunkLen = sizeof(szWrkData)-1;
                    if (nFetchType == SQL_C_CHAR) 
                        while ((nChunkLen > 1) && (szWrkData[nChunkLen - 1] == 0)) --nChunkLen;  // trimming the extra terminators
                }
                else
                    nChunkLen = cbDataLen;
                szWrkData[nChunkLen] = '\0';

                m_papszColValues[iCol] = (char *) 
                    CPLRealloc( m_papszColValues[iCol], 
                                m_panColValueLengths[iCol] + nChunkLen + 1 );
                memcpy( m_papszColValues[iCol] + m_panColValueLengths[iCol], 
                        szWrkData, nChunkLen );
                m_panColValueLengths[iCol] += nChunkLen;
                m_papszColValues[iCol][m_panColValueLengths[iCol]] = '\0';
            }
        }
        else
        {
            m_panColValueLengths[iCol] = cbDataLen;
            m_papszColValues[iCol] = (char *) CPLMalloc(cbDataLen+1);
            memcpy( m_papszColValues[iCol], szWrkData, cbDataLen );
            m_papszColValues[iCol][cbDataLen] = '\0';
        }

        // Trim white space off end, if there is any.
        if( nFetchType == SQL_C_CHAR && m_papszColValues[iCol] != NULL )
        {
            char *pszTarget = m_papszColValues[iCol];
            int iEnd = strlen(pszTarget) - 1;

            while( iEnd >= 0 && pszTarget[iEnd] == ' ' )
                pszTarget[iEnd--] = '\0';
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
    int iCol = GetColId( pszColName );

    if( iCol == -1 )
        return pszDefault;
    else
        return GetColData( iCol, pszDefault );
}

/************************************************************************/
/*                          GetColDataLength()                          */
/************************************************************************/

int CPLODBCStatement::GetColDataLength( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return 0;
    else if( m_papszColValues[iCol] != NULL )
        return m_panColValueLengths[iCol];
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
    for( int iCol = 0; iCol < m_nColCount; iCol++ )
        if( EQUAL(pszColName,m_papszColNames[iCol]) )
            return iCol;
    
    return -1;
}

/************************************************************************/
/*                          ClearColumnData()                           */
/************************************************************************/

void CPLODBCStatement::ClearColumnData()

{
    if( m_nColCount > 0 )
    {
        for( int iCol = 0; iCol < m_nColCount; iCol++ )
        {
            if( m_papszColValues[iCol] != NULL )
            {
                VSIFree( m_papszColValues[iCol] );
                m_papszColValues[iCol] = NULL;
            }
        }
    }
}

/************************************************************************/
/*                               Failed()                               */
/************************************************************************/

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
    int  nTextLen = strlen(pszText);

    if( m_nStatementMax < m_nStatementLen + nTextLen + 1 )
    {
        m_nStatementMax = (m_nStatementLen + nTextLen) * 2 + 100;
        if( m_pszStatement == NULL )
        {
            m_pszStatement = (char *) VSIMalloc(m_nStatementMax);
            m_pszStatement[0] = '\0';
        }
        else
        {
            m_pszStatement = (char *) VSIRealloc(m_pszStatement, m_nStatementMax);
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
    int  iIn, iOut ,nTextLen = strlen(pszText);
    char *pszEscapedText = (char *) VSIMalloc(nTextLen*2 + 1);

    for( iIn = 0, iOut = 0; iIn < nTextLen; iIn++ )
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
    VSIFree( pszEscapedText );
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
    char szFormattedValue[100];

    sprintf( szFormattedValue, "%d", nValue );
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
    char szFormattedValue[100];

    sprintf( szFormattedValue, "%24g", dfValue );
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
 * @return FALSE if formatting fails dueto result being too large.
 */

int CPLODBCStatement::Appendf( const char *pszFormat, ... )

{
    va_list args;
    char    szFormattedText[8000];
    int     bSuccess;

    va_start( args, pszFormat );
#if defined(HAVE_VSNPRINTF)
    bSuccess = vsnprintf( szFormattedText, sizeof(szFormattedText)-1, 
                          pszFormat, args ) < (int) sizeof(szFormattedText)-1;
#else
    vsprintf( szFormattedText, pszFormat, args );
    bSuccess = TRUE;
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
    ClearColumnData();

    if( m_pszStatement != NULL )
    {
        VSIFree( m_pszStatement );
        m_pszStatement = NULL;
    }

    m_nStatementLen = 0;
    m_nStatementMax = 0;

    if( m_papszColNames )
    {
        CPLFree( m_panColType );
        m_panColType = NULL;

        CPLFree( m_panColSize );
        m_panColSize = NULL;

        CPLFree( m_panColPrecision );
        m_panColPrecision = NULL;

        CPLFree( m_panColNullable );
        m_panColNullable = NULL;

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
 * of a table (or other queriable object such as a view).  The column
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
    SQLINTEGER nResultCount=0;

    if( Failed(SQLRowCount( m_hStmt, &nResultCount ) ) )
        nResultCount = 0;

    if( nResultCount < 1 )
        m_nColCount = 500; // Hopefully lots.
    else
        m_nColCount = (short) nResultCount;
#endif

    m_nColCount = 500;
    
    m_papszColNames = (char **) calloc(sizeof(char *),(m_nColCount+1));
    m_papszColValues = (char **) calloc(sizeof(char *),(m_nColCount+1));

    m_panColType = (short *) calloc(sizeof(short),m_nColCount);
    m_panColSize = (_SQLULEN *) calloc(sizeof(_SQLULEN),m_nColCount);
    m_panColPrecision = (short *) calloc(sizeof(short),m_nColCount);
    m_panColNullable = (short *) calloc(sizeof(short),m_nColCount);

/* -------------------------------------------------------------------- */
/*      Establish columns to use for key information.                   */
/* -------------------------------------------------------------------- */
    int iCol;

    for( iCol = 0; iCol < m_nColCount; iCol++ )
    {
        char szWrkData[8193];
        _SQLLEN cbDataLen;

        if( Failed( SQLFetch( m_hStmt ) ) )
        {
            m_nColCount = (SQLUSMALLINT) iCol;
            break;
        }

        szWrkData[0] = '\0';

        SQLGetData( m_hStmt, 4, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_papszColNames[iCol] = CPLStrdup(szWrkData);

        SQLGetData( m_hStmt, 5, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_panColType[iCol] = (short) atoi(szWrkData);

        SQLGetData( m_hStmt, 7, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_panColSize[iCol] = atoi(szWrkData);

        SQLGetData( m_hStmt, 9, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_panColPrecision[iCol] = (short) atoi(szWrkData);

        SQLGetData( m_hStmt, 11, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_panColNullable[iCol] = atoi(szWrkData) == SQL_NULLABLE;
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

/* -------------------------------------------------------------------- */
/*      Fetch columns resultset for this table.                         */
/* -------------------------------------------------------------------- */
    if( Failed( SQLPrimaryKeys( m_hStmt, 
                                (SQLCHAR *) pszCatalog, SQL_NTS,
                                (SQLCHAR *) pszSchema, SQL_NTS,
                                (SQLCHAR *) pszTable, SQL_NTS ) ) )
        return FALSE;
    else
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
    CPLDebug( "ODBC", "CatalogNameL: %s\nSchema name: %s\n",
                pszCatalog, pszSchema );

/* -------------------------------------------------------------------- */
/*      Fetch columns resultset for this table.                         */
/* -------------------------------------------------------------------- */
    if( Failed( SQLTables( m_hStmt, 
                           (SQLCHAR *) pszCatalog, SQL_NTS,
                           (SQLCHAR *) pszSchema, SQL_NTS,
                           (SQLCHAR *) NULL, SQL_NTS,
                           (SQLCHAR *) "'TABLE','VIEW'", SQL_NTS ) ) )
        return FALSE;
    else
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
    int iCol;

/* -------------------------------------------------------------------- */
/*      Display schema                                                  */
/* -------------------------------------------------------------------- */
    if( bShowSchema )
    {
        fprintf( fp, "Column Definitions:\n" );
        for( iCol = 0; iCol < GetColCount(); iCol++ )
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
        
        for( iCol = 0; iCol < GetColCount(); iCol++ )
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

#endif /* #ifndef WIN32CE */
