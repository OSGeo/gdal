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
 ****************************************************************************/

#ifndef CPL_ODBC_H_INCLUDED
#define CPL_ODBC_H_INCLUDED

#include "cpl_port.h"

#ifdef WIN32
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <odbcinst.h>
#include "cpl_string.h"

/*! @cond Doxygen_Suppress */
#ifdef PATH_MAX
#define ODBC_FILENAME_MAX PATH_MAX
#else
#define ODBC_FILENAME_MAX (255 + 1) /* Max path length */
#endif
/*! @endcond */

/**
 * \file cpl_odbc.h
 *
 * ODBC Abstraction Layer (C++).
 */

/**
 * A class providing functions to install or remove ODBC driver.
 */
class CPL_DLL CPLODBCDriverInstaller
{
    char m_szPathOut[ODBC_FILENAME_MAX];
    char m_szError[SQL_MAX_MESSAGE_LENGTH];
    DWORD m_nErrorCode;
    DWORD m_nUsageCount;

    static bool FindMdbToolsDriverLib(CPLString &osDriverFile);
    static bool LibraryExists(const char *pszLibPath);

  public:
    // Default constructor.
    CPLODBCDriverInstaller();

    /**
     * Installs ODBC driver or updates definition of already installed driver.
     * Interanally, it calls ODBC's SQLInstallDriverEx function.
     *
     * @param pszDriver - The driver definition as a list of keyword-value
     * pairs describing the driver (See ODBC API Reference).
     *
     * @param pszPathIn - Full path of the target directory of the installation,
     * or a null pointer (for unixODBC, NULL is passed).
     *
     * @param fRequest - The fRequest argument must contain one of
     * the following values:
     * ODBC_INSTALL_COMPLETE - (default) complete the installation request
     * ODBC_INSTALL_INQUIRY - inquire about where a driver can be installed
     *
     * @return TRUE indicates success, FALSE if it fails.
     */
    int InstallDriver(const char *pszDriver, const char *pszPathIn,
                      WORD fRequest = ODBC_INSTALL_COMPLETE);

    /**
     * Attempts to install the MDB Tools driver for Microsoft Access databases.
     *
     * This is only supported on non-Windows platforms.
     *
     * @since GDAL 3.4
     */
    static void InstallMdbToolsDriver();

    /**
     * Removes or changes information about the driver from
     * the Odbcinst.ini entry in the system information.
     *
     * @param pszDriverName - The name of the driver as registered in
     * the Odbcinst.ini key of the system information.
     *
     * @param fRemoveDSN - TRUE: Remove DSNs associated with the driver
     * specified in lpszDriver. FALSE: Do not remove DSNs associated
     * with the driver specified in lpszDriver.
     *
     * @return The function returns TRUE if it is successful,
     * FALSE if it fails. If no entry exists in the system information
     * when this function is called, the function returns FALSE.
     * In order to obtain usage count value, call GetUsageCount().
     */
    int RemoveDriver(const char *pszDriverName, int fRemoveDSN = FALSE);

    /** The usage count of the driver after this function has been called */
    int GetUsageCount() const
    {
        return m_nUsageCount;
    }

    /** Path of the target directory where the driver should be installed.
     * For details, see ODBC API Reference and lpszPathOut
     * parameter of SQLInstallDriverEx
     */
    const char *GetPathOut() const
    {
        return m_szPathOut;
    }

    /** If InstallDriver returns FALSE, then GetLastError then
     * error message can be obtained by calling this function.
     * Internally, it calls ODBC's SQLInstallerError function.
     */
    const char *GetLastError() const
    {
        return m_szError;
    }

    /** If InstallDriver returns FALSE, then GetLastErrorCode then
     * error code can be obtained by calling this function.
     * Internally, it calls ODBC's SQLInstallerError function.
     * See ODBC API Reference for possible error flags.
     */
    DWORD GetLastErrorCode() const
    {
        return m_nErrorCode;
    }
};

class CPLODBCStatement;

/* On MSVC SQLULEN is missing in some cases (i.e. VC6)
** but it is always a #define so test this way.   On Unix
** it is a typedef so we can't always do this.
*/
#if defined(_MSC_VER) && !defined(SQLULEN) && !defined(_WIN64)
#define MISSING_SQLULEN
#endif

/*! @cond Doxygen_Suppress */
#if !defined(MISSING_SQLULEN)
/* ODBC types to support 64 bit compilation */
#define CPL_SQLULEN SQLULEN
#define CPL_SQLLEN SQLLEN
#else
#define CPL_SQLULEN SQLUINTEGER
#define CPL_SQLLEN SQLINTEGER
#endif /* ifdef SQLULEN */
/*! @endcond */

/**
 * A class representing an ODBC database session.
 *
 * Includes error collection services.
 */

class CPL_DLL CPLODBCSession
{

    CPL_DISALLOW_COPY_ASSIGN(CPLODBCSession)

    /*! @cond Doxygen_Suppress */
  protected:
    CPLString m_osLastError{};
    HENV m_hEnv = nullptr;
    HDBC m_hDBC = nullptr;
    int m_bInTransaction = false;
    int m_bAutoCommit = true;
    /*! @endcond */

  public:
    CPLODBCSession();
    ~CPLODBCSession();

    int EstablishSession(const char *pszDSN, const char *pszUserid,
                         const char *pszPassword);
    const char *GetLastError();

    // Transaction handling

    int ClearTransaction();
    int BeginTransaction();
    int CommitTransaction();
    int RollbackTransaction();
    /** Returns whether a transaction is active */
    int IsInTransaction()
    {
        return m_bInTransaction;
    }

    // Essentially internal.

    int CloseSession();

    int Failed(int, HSTMT = nullptr);
    /** Return connection handle */
    HDBC GetConnection()
    {
        return m_hDBC;
    }
    /** Return GetEnvironment handle */
    HENV GetEnvironment()
    {
        return m_hEnv;
    }

    bool ConnectToMsAccess(const char *pszName,
                           const char *pszDSNStringTemplate);
};

/**
 * Abstraction for statement, and resultset.
 *
 * Includes methods for executing an SQL statement, and for accessing the
 * resultset from that statement.  Also provides for executing other ODBC
 * requests that produce results sets such as SQLColumns() and SQLTables()
 * requests.
 */

class CPL_DLL CPLODBCStatement
{

    CPL_DISALLOW_COPY_ASSIGN(CPLODBCStatement)

    /*! @cond Doxygen_Suppress */
  protected:
    int m_nFlags = 0;

    CPLODBCSession *m_poSession = nullptr;
    HSTMT m_hStmt = nullptr;

    SQLSMALLINT m_nColCount = 0;
    char **m_papszColNames = nullptr;
    SQLSMALLINT *m_panColType = nullptr;
    char **m_papszColTypeNames = nullptr;
    CPL_SQLULEN *m_panColSize = nullptr;
    SQLSMALLINT *m_panColPrecision = nullptr;
    SQLSMALLINT *m_panColNullable = nullptr;
    char **m_papszColColumnDef = nullptr;

    char **m_papszColValues = nullptr;
    CPL_SQLLEN *m_panColValueLengths = nullptr;
    double *m_padColValuesAsDouble = nullptr;

    int Failed(int);

    char *m_pszStatement = nullptr;
    size_t m_nStatementMax = 0;
    size_t m_nStatementLen = 0;
    /*! @endcond */

  public:
    /**
     * Flags which control ODBC statement behavior.
     */
    enum Flag
    {
        /**
         * Numeric column values should be retrieved as doubles, using either
         * the SQL_C_DOUBLE or SQL_C_FLOAT types.
         *
         * By default numeric column values are retrieved as characters.
         * Retrieving as character is the safest behavior, but can risk loss of
         * precision.
         *
         * If set, GetColDataAsDouble should be used for numeric columns instead
         * of GetColData.
         *
         * Warning: this flag can expose issues in particular ODBC drivers on
         * different platforms. Use with caution.
         */
        RetrieveNumericColumnsAsDouble = 1 << 0,
    };

    explicit CPLODBCStatement(CPLODBCSession *, int flags = 0);
    ~CPLODBCStatement();

    /** Return statement handle */
    HSTMT GetStatement()
    {
        return m_hStmt;
    }

    /**
     * Returns statement flags.
     */
    int Flags() const
    {
        return m_nFlags;
    }

    // Command buffer related.
    void Clear();
    void AppendEscaped(const char *);
    void Append(const char *);
    // cppcheck-suppress functionStatic
    void Append(int);
    void Append(double);
    int Appendf(CPL_FORMAT_STRING(const char *), ...)
        CPL_PRINT_FUNC_FORMAT(2, 3);
    /** Return statement string */
    const char *GetCommand()
    {
        return m_pszStatement;
    }

    int ExecuteSQL(const char * = nullptr);

    // Results fetching
    int Fetch(int nOrientation = SQL_FETCH_NEXT, int nOffset = 0);
    void ClearColumnData();

    int GetColCount();
    const char *GetColName(int);
    short GetColType(int);
    const char *GetColTypeName(int);
    short GetColSize(int);
    short GetColPrecision(int);
    short GetColNullable(int);
    const char *GetColColumnDef(int);

    int GetColId(const char *) const;
    const char *GetColData(int, const char * = nullptr);
    const char *GetColData(const char *, const char * = nullptr);
    int GetColDataLength(int);

    double GetColDataAsDouble(int) const;
    double GetColDataAsDouble(const char *) const;

    int GetRowCountAffected();

    // Fetch special metadata.
    int GetColumns(const char *pszTable, const char *pszCatalog = nullptr,
                   const char *pszSchema = nullptr);
    int GetPrimaryKeys(const char *pszTable, const char *pszCatalog = nullptr,
                       const char *pszSchema = nullptr);

    int GetTables(const char *pszCatalog = nullptr,
                  const char *pszSchema = nullptr);

    void DumpResult(FILE *fp, int bShowSchema = FALSE);

    static CPLString GetTypeName(int);
    static SQLSMALLINT GetTypeMapping(SQLSMALLINT);

    int CollectResultsInfo();
};

#endif
