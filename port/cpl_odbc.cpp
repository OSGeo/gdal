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
 * Revision 1.2  2003/09/25 17:09:49  warmerda
 * added some more methods
 *
 * Revision 1.1  2003/09/24 15:38:27  warmerda
 * New
 *
 */

#include "cpl_odbc.h"

CPL_CVSID("$Id$");

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
    SQLSMALLINT nTextLength;


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

    if( Failed( SQLConnect( m_hDBC, (SQLCHAR *) pszDSN, SQL_NTS, 
                            (SQLCHAR *) pszUserid, SQL_NTS, 
                            (SQLCHAR *) pszPassword, SQL_NTS ) ) )
    {
        CloseSession();
        return FALSE;
    }

    return TRUE;
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

    m_pszStatement = NULL;
    m_nStatementMax = 0;
    m_nStatementLen = 0;
}

/************************************************************************/
/*                         ~CPLODBCStatement()                          */
/************************************************************************/

CPLODBCStatement::~CPLODBCStatement()

{
    ClearColumnData();
    Clear();

    if( m_hStmt != NULL )
        SQLFreeStmt( m_hStmt, SQL_DROP );
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

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
    m_papszColNames = (char **) calloc(sizeof(char *),(m_nColCount+1));
    m_papszColValues = (char **) calloc(sizeof(char *),(m_nColCount+1));

    m_panColType = (short *) calloc(sizeof(short),m_nColCount);
    m_panColSize = (short *) calloc(sizeof(short),m_nColCount);
    m_panColPrecision = (short *) calloc(sizeof(short),m_nColCount);
    m_panColNullable = (short *) calloc(sizeof(short),m_nColCount);

/* -------------------------------------------------------------------- */
/*      Fetch column descriptions.                                      */
/* -------------------------------------------------------------------- */
    for( int iCol = 0; iCol < m_nColCount; iCol++ )
    {
        char szColName[256];
        SQLSMALLINT nNameLength;

        if( Failed( 
                SQLDescribeCol( m_hStmt, iCol+1, 
                                (SQLCHAR *) szColName, sizeof(szColName),
                                &nNameLength,
                                m_panColType + iCol,
                                (SQLUINTEGER*) m_panColSize + iCol,
                                m_panColPrecision + iCol,
                                m_panColNullable + iCol )) )
            return FALSE;

        szColName[nNameLength] = '\0';
        m_papszColNames[iCol] = strdup(szColName);
    }

    return TRUE;
}

/************************************************************************/
/*                            GetColCount()                             */
/************************************************************************/

int CPLODBCStatement::GetColCount()

{
    return m_nColCount;
}

/************************************************************************/
/*                             GetColName()                             */
/************************************************************************/

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

short CPLODBCStatement::GetColSize( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return -1;
    else
        return m_panColSize[iCol];
}

/************************************************************************/
/*                          GetColPrecision()                           */
/************************************************************************/

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

short CPLODBCStatement::GetColNullable( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return -1;
    else
        return m_panColNullable[iCol];
}

/************************************************************************/
/*                               Fetch()                                */
/*                                                                      */
/*      Fetch one row from a results set.                               */
/************************************************************************/

int CPLODBCStatement::Fetch( int nOrientation, int nOffset )

{
    ClearColumnData();

    if( m_hStmt == NULL || m_nColCount < 1 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Fetch a new row.                                                */
/* -------------------------------------------------------------------- */
    if( Failed( SQLFetchScroll( m_hStmt, (SQLSMALLINT) nOrientation, 
                                nOffset ) ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Pull out all the column values.                                 */
/* -------------------------------------------------------------------- */
    int iCol;
    
    for( iCol = 0; iCol < m_nColCount; iCol++ )
    {
        char szWrkData[8193];
        SQLINTEGER cbDataLen;

        szWrkData[0] = '\0';
        if( Failed( SQLGetData( m_hStmt, iCol+1, SQL_C_CHAR,
                                szWrkData, sizeof(szWrkData)-1, 
                                &cbDataLen ) ) )
            return FALSE;

        if( cbDataLen == SQL_NULL_DATA )
            m_papszColValues[iCol] = NULL;
        else
        {
            szWrkData[cbDataLen] = '\0';
            while( cbDataLen > 0 && szWrkData[cbDataLen-1] == ' ' )
                szWrkData[--cbDataLen] = '\0';
            
            m_papszColValues[iCol] = strdup( szWrkData );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                             GetColData()                             */
/************************************************************************/

const char *CPLODBCStatement::GetColData( int iCol )

{
    if( iCol < 0 || iCol >= m_nColCount )
        return NULL;
    else 
        return m_papszColValues[iCol];
}

/************************************************************************/
/*                              GetColId()                              */
/************************************************************************/

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
                free( m_papszColValues[iCol] );
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

void CPLODBCStatement::Append( const char *pszText )

{
    int  nTextLen = strlen(pszText);

    if( m_nStatementMax < m_nStatementLen + nTextLen + 1 )
    {
        m_nStatementMax = (m_nStatementLen + nTextLen) * 2 + 100;
        if( m_pszStatement == NULL )
        {
            m_pszStatement = (char *) malloc(m_nStatementMax);
            m_pszStatement[0] = '\0';
        }
        else
        {
            m_pszStatement = (char *) realloc(m_pszStatement, m_nStatementMax);
        }
    }

    strcpy( m_pszStatement + m_nStatementLen, pszText );
    m_nStatementLen += nTextLen;
}

/************************************************************************/
/*                             Append(int)                              */
/************************************************************************/

void CPLODBCStatement::Append( int nValue )

{
    char szFormattedValue[100];

    sprintf( szFormattedValue, "%d", nValue );
    Append( szFormattedValue );
}

/************************************************************************/
/*                            Append(double)                            */
/************************************************************************/

void CPLODBCStatement::Append( double dfValue )

{
    char szFormattedValue[100];

    sprintf( szFormattedValue, "%24g", dfValue );
    Append( szFormattedValue );
}

/************************************************************************/
/*                              Appendf()                               */
/************************************************************************/

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
    vsnprintf( szFormattedText, pszFormat, args );
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

void CPLODBCStatement::Clear()

{
    if( m_pszStatement != NULL )
        free( m_pszStatement );

    m_nStatementLen = 0;
    m_nStatementMax = 0;
}

/************************************************************************/
/*                             GetColumns()                             */
/************************************************************************/

int CPLODBCStatement::GetColumns( const char *pszTable, 
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
    if( Failed( SQLColumns( m_hStmt, 
                            (SQLCHAR *) pszCatalog, SQL_NTS,
                            (SQLCHAR *) pszSchema, SQL_NTS,
                            (SQLCHAR *) pszTable, SQL_NTS,
                            (SQLCHAR *) "", SQL_NTS ) ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Allocate per column information.                                */
/* -------------------------------------------------------------------- */
    SQLINTEGER nResultCount;

    SQLRowCount( m_hStmt, &nResultCount );
    if( nResultCount < 1 )
        m_nColCount = 500; // Hopefully lots.
    else
        m_nColCount = nResultCount;
    
    m_papszColNames = (char **) calloc(sizeof(char *),(m_nColCount+1));
    m_papszColValues = (char **) calloc(sizeof(char *),(m_nColCount+1));

    m_panColType = (short *) calloc(sizeof(short),m_nColCount);
    m_panColSize = (short *) calloc(sizeof(short),m_nColCount);
    m_panColPrecision = (short *) calloc(sizeof(short),m_nColCount);
    m_panColNullable = (short *) calloc(sizeof(short),m_nColCount);

/* -------------------------------------------------------------------- */
/*      Establish columns to use for key information.                   */
/* -------------------------------------------------------------------- */
    int iCol;

    for( iCol = 0; iCol < m_nColCount; iCol++ )
    {
        char szWrkData[8193];
        SQLINTEGER cbDataLen;

        if( Failed( SQLFetch( m_hStmt ) ) )
        {
            m_nColCount = iCol;
            break;
        }

        szWrkData[0] = '\0';

        SQLGetData( m_hStmt, 4, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_papszColNames[iCol] = strdup(szWrkData);

        SQLGetData( m_hStmt, 5, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_panColType[iCol] = atoi(szWrkData);

        SQLGetData( m_hStmt, 7, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_panColSize[iCol] = atoi(szWrkData);

        SQLGetData( m_hStmt, 9, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_panColPrecision[iCol] = atoi(szWrkData);

        SQLGetData( m_hStmt, 11, SQL_C_CHAR, szWrkData, sizeof(szWrkData)-1, 
                    &cbDataLen );
        m_panColNullable[iCol] = atoi(szWrkData) == SQL_NULLABLE;
    }

    return TRUE;
}

/************************************************************************/
/*                             DumpResult()                             */
/************************************************************************/

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

            fprintf( fp, " Type:%s", GetTypeName( GetColType(iCol) ) );
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

const char *CPLODBCStatement::GetTypeName( int nTypeCode )

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
        return "TIMESTAMP";
    }
}
