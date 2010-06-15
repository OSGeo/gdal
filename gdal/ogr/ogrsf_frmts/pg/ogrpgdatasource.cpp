/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#include <string.h>
#include "ogr_pg.h"
#include "ogrpgutility.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_hash_set.h"

CPL_CVSID("$Id$");

static void OGRPGNoticeProcessor( void *arg, const char * pszMessage );

/************************************************************************/
/*                          OGRPGDataSource()                           */
/************************************************************************/

OGRPGDataSource::OGRPGDataSource()

{
    pszName = NULL;
    pszDBName = NULL;
    papoLayers = NULL;
    nLayers = 0;
    hPGConn = NULL;
    bHavePostGIS = FALSE;
    bHaveGeography = FALSE;
    bUseBinaryCursor = FALSE;
    nSoftTransactionLevel = 0;
    bBinaryTimeFormatIsInt8 = FALSE;
    
    nGeometryOID = (Oid) 0;
    nGeographyOID = (Oid) 0;

    nKnownSRID = 0;
    panSRID = NULL;
    papoSRS = NULL;

    poLayerInCopyMode = NULL;
}

/************************************************************************/
/*                          ~OGRPGDataSource()                          */
/************************************************************************/

OGRPGDataSource::~OGRPGDataSource()

{
    int         i;

    FlushSoftTransaction();

    CPLFree( pszName );
    CPLFree( pszDBName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    if( hPGConn != NULL )
    {
        /* XXX - mloskot: After the connection is closed, valgrind still
         * reports 36 bytes definitely lost, somewhere in the libpq.
         */
        PQfinish( hPGConn );
        hPGConn = NULL;
    }

    for( i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] != NULL )
            papoSRS[i]->Release();
    }
    CPLFree( panSRID );
    CPLFree( papoSRS );
}

/************************************************************************/
/*                         GetCurrentSchema()                           */
/************************************************************************/

CPLString OGRPGDataSource::GetCurrentSchema()
{
    CPLString osCurrentSchema;
    /* -------------------------------------------- */
    /*          Get the current schema              */
    /* -------------------------------------------- */
    PGresult    *hResult = PQexec(hPGConn,"SELECT current_schema()");
    if ( hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult,0,0) )
    {
        osCurrentSchema = PQgetvalue(hResult,0,0);
    }
    OGRPGClearResult( hResult );

    return osCurrentSchema;
}

/************************************************************************/
/*                      OGRPGDecodeVersionString()                      */
/************************************************************************/

void OGRPGDataSource::OGRPGDecodeVersionString(PGver* psVersion, char* pszVer)
{
    GUInt32 iLen;
    char* ptr;
    char szNum[25];
    char szVer[10];

    ptr = pszVer;
    // get Version string
    if ( *ptr == ' ' ) *ptr++;
    while (*ptr && *ptr != ' ') ptr++;
    iLen = ptr-pszVer;
    if ( iLen > sizeof(szVer) - 1 ) iLen = sizeof(szVer) - 1;
    strncpy(szVer,pszVer,iLen);
    szVer[iLen] = '\0';

    ptr = pszVer = szVer;

    // get Major number
    while (*ptr && *ptr != '.') ptr++;
    iLen = ptr-pszVer;
    if ( iLen > sizeof(szNum) - 1) iLen = sizeof(szNum) - 1;
    strncpy(szNum,pszVer,iLen);
    szNum[iLen] = '\0';
    psVersion->nMajor = atoi(szNum);

    pszVer = ++ptr;

    // get Minor number
    while (*ptr && *ptr != '.') ptr++;
    iLen = ptr-pszVer;
    if ( iLen > sizeof(szNum) - 1) iLen = sizeof(szNum) - 1;
    strncpy(szNum,pszVer,iLen);
    szNum[iLen] = '\0';
    psVersion->nMinor = atoi(szNum);


    if ( *ptr )
    {
        pszVer = ++ptr;

        // get Release number
        while (*ptr && *ptr != '.') ptr++;
        iLen = ptr-pszVer;
        if ( iLen > sizeof(szNum) - 1) iLen = sizeof(szNum) - 1;
        strncpy(szNum,pszVer,iLen);
        szNum[iLen] = '\0';
        psVersion->nRelease = atoi(szNum);
    }

}


/************************************************************************/
/*                     One entry for each PG table                      */
/************************************************************************/

typedef struct
{
    char* pszTableName;
    char* pszSchemaName;
    char** papszGeomColumnNames; /* list of geometry columns */
    int   bDerivedInfoAdded;            /* set to TRUE if it derives from another table */
} PGTableEntry;

static unsigned long OGRPGHashTableEntry(const void * _psTableEntry)
{
    const PGTableEntry* psTableEntry = (PGTableEntry*)_psTableEntry;
    return CPLHashSetHashStr(CPLString().Printf("%s.%s",
                             psTableEntry->pszSchemaName, psTableEntry->pszTableName));
}

static int OGRPGEqualTableEntry(const void* _psTableEntry1, const void* _psTableEntry2)
{
    const PGTableEntry* psTableEntry1 = (PGTableEntry*)_psTableEntry1;
    const PGTableEntry* psTableEntry2 = (PGTableEntry*)_psTableEntry2;
    return strcmp(psTableEntry1->pszTableName, psTableEntry2->pszTableName) == 0 &&
           strcmp(psTableEntry1->pszSchemaName, psTableEntry2->pszSchemaName) == 0;
}

static void OGRPGFreeTableEntry(void * _psTableEntry)
{
    PGTableEntry* psTableEntry = (PGTableEntry*)_psTableEntry;
    CPLFree(psTableEntry->pszTableName);
    CPLFree(psTableEntry->pszSchemaName);
    CSLDestroy(psTableEntry->papszGeomColumnNames);
    CPLFree(psTableEntry);
}

static PGTableEntry* OGRPGFindTableEntry(CPLHashSet* hSetTables,
                                         const char* pszTableName,
                                         const char* pszSchemaName)
{
    PGTableEntry sEntry;
    sEntry.pszTableName = (char*) pszTableName;
    sEntry.pszSchemaName = (char*) pszSchemaName;
    return (PGTableEntry*) CPLHashSetLookup(hSetTables, &sEntry);
}

static PGTableEntry* OGRPGAddTableEntry(CPLHashSet* hSetTables,
                                        const char* pszTableName,
                                        const char* pszSchemaName)
{
    PGTableEntry* psEntry = (PGTableEntry*) CPLCalloc(1, sizeof(PGTableEntry));
    psEntry->pszTableName = CPLStrdup(pszTableName);
    psEntry->pszSchemaName = CPLStrdup(pszSchemaName);

    CPLHashSetInsert(hSetTables, psEntry);

    return psEntry;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRPGDataSource::Open( const char * pszNewName, int bUpdate,
                              int bTestOpen )

{
    CPLAssert( nLayers == 0 );

/* -------------------------------------------------------------------- */
/*      Verify postgresql prefix.                                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszNewName,"PGB:",4) )
    {
        bUseBinaryCursor = TRUE;
        CPLDebug("PG","BINARY cursor is used for geometry fetching");
    }
    else
    if( !EQUALN(pszNewName,"PG:",3) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s does not conform to PostgreSQL naming convention,"
                      " PG:*\n", pszNewName );
        return FALSE;
    }

    pszName = CPLStrdup( pszNewName );
    char* pszConnectionName = CPLStrdup(pszName);

/* -------------------------------------------------------------------- */
/*      Determine if the connection string contains an optional         */
/*      ACTIVE_SCHEMA portion. If so, parse it out.                     */
/* -------------------------------------------------------------------- */
    char             *pszActiveSchemaStart;
    CPLString         osActiveSchema;
    pszActiveSchemaStart = strstr(pszConnectionName, "active_schema=");
    if (pszActiveSchemaStart == NULL)
        pszActiveSchemaStart = strstr(pszConnectionName, "ACTIVE_SCHEMA=");
    if (pszActiveSchemaStart != NULL)
    {
        char           *pszActiveSchema;
        const char     *pszEnd = NULL;

        pszActiveSchema = CPLStrdup( pszActiveSchemaStart + strlen("active_schema=") );

        pszEnd = strchr(pszActiveSchemaStart, ' ');
        if( pszEnd == NULL )
            pszEnd = pszConnectionName + strlen(pszConnectionName);

        // Remove ACTIVE_SCHEMA=xxxxx from pszConnectionName string
        memmove( pszActiveSchemaStart, pszEnd, strlen(pszEnd) + 1 );

        pszActiveSchema[pszEnd - pszActiveSchemaStart - strlen("active_schema=")] = '\0';

        osActiveSchema = pszActiveSchema;
        CPLFree(pszActiveSchema);
    }
    else
    {
        osActiveSchema = "public";
    }

/* -------------------------------------------------------------------- */
/*      Determine if the connection string contains an optional         */
/*      SCHEMAS portion. If so, parse it out.                           */
/* -------------------------------------------------------------------- */
    char             *pszSchemasStart;
    char            **papszSchemaList = NULL;
    pszSchemasStart = strstr(pszConnectionName, "schemas=");
    if (pszSchemasStart == NULL)
        pszSchemasStart = strstr(pszConnectionName, "SCHEMAS=");
    if (pszSchemasStart != NULL)
    {
        char           *pszSchemas;
        const char     *pszEnd = NULL;

        pszSchemas = CPLStrdup( pszSchemasStart + strlen("schemas=") );

        pszEnd = strchr(pszSchemasStart, ' ');
        if( pszEnd == NULL )
            pszEnd = pszConnectionName + strlen(pszConnectionName);

        // Remove SCHEMAS=xxxxx from pszConnectionName string
        memmove( pszSchemasStart, pszEnd, strlen(pszEnd) + 1 );

        pszSchemas[pszEnd - pszSchemasStart - strlen("schemas=")] = '\0';

        papszSchemaList = CSLTokenizeString2( pszSchemas, ",", 0 );

        CPLFree(pszSchemas);

        /* If there is only one schema specified, make it the active schema */
        if (CSLCount(papszSchemaList) == 1)
        {
            osActiveSchema = papszSchemaList[0];
        }
    }

/* -------------------------------------------------------------------- */
/*      Determine if the connection string contains an optional         */
/*      TABLES portion. If so, parse it out. The expected               */
/*      connection string in this case will be, e.g.:                   */
/*                                                                      */
/*        'PG:dbname=warmerda user=warmerda tables=s1.t1,[s2.t2,...]    */
/*              - where sN is schema and tN is table name               */
/*      We must also strip this information from the connection         */
/*      string; PQconnectdb() does not like unknown directives          */
/* -------------------------------------------------------------------- */
    char              **papszTableNames=NULL;
    char              **papszSchemaNames=NULL;
    char              **papszGeomColumnNames=NULL;

    char             *pszTableStart;
    pszTableStart = strstr(pszConnectionName, "tables=");
    if (pszTableStart == NULL)
        pszTableStart = strstr(pszConnectionName, "TABLES=");

    if( pszTableStart != NULL )
    {
        char          **papszTableList;
        char           *pszTableSpec;
        const char     *pszEnd = NULL;
        int             i;

        pszTableSpec = CPLStrdup( pszTableStart + 7 );

        pszEnd = strchr(pszTableStart, ' ');
        if( pszEnd == NULL )
            pszEnd = pszConnectionName + strlen(pszConnectionName);

        // Remove TABLES=xxxxx from pszConnectionName string
        memmove( pszTableStart, pszEnd, strlen(pszEnd) + 1 );

        pszTableSpec[pszEnd - pszTableStart - 7] = '\0';
        papszTableList = CSLTokenizeString2( pszTableSpec, ",", 0 );

        for( i = 0; i < CSLCount(papszTableList); i++ )
        {
            char      **papszQualifiedParts;

            // Get schema and table name
            papszQualifiedParts = CSLTokenizeString2( papszTableList[i],
                                                      ".", 0 );

            /* Find the geometry column name if specified */
            if( CSLCount( papszQualifiedParts ) >= 1 )
            {
                char* pszGeomColumnName = NULL;
                char* pos = strchr(papszQualifiedParts[CSLCount( papszQualifiedParts ) - 1], '(');
                if (pos != NULL)
                {
                    *pos = '\0';
                    pszGeomColumnName = pos+1;
                    int len = strlen(pszGeomColumnName);
                    if (len > 0)
                        pszGeomColumnName[len - 1] = '\0';
                }
                papszGeomColumnNames = CSLAddString( papszGeomColumnNames,
                        pszGeomColumnName ? pszGeomColumnName : "");
            }

            if( CSLCount( papszQualifiedParts ) == 2 )
            {
                papszSchemaNames = CSLAddString( papszSchemaNames, 
                                                papszQualifiedParts[0] );
                papszTableNames = CSLAddString( papszTableNames,
                                                papszQualifiedParts[1] );
            }
            else if( CSLCount( papszQualifiedParts ) == 1 )
            {
                papszSchemaNames = CSLAddString( papszSchemaNames, osActiveSchema.c_str());
                papszTableNames = CSLAddString( papszTableNames,
                                                papszQualifiedParts[0] );
            }

            CSLDestroy(papszQualifiedParts);
        }

        CSLDestroy(papszTableList);
        CPLFree(pszTableSpec);
    }

/* -------------------------------------------------------------------- */
/*      Try to establish connection.                                    */
/* -------------------------------------------------------------------- */
    hPGConn = PQconnectdb( pszConnectionName + (bUseBinaryCursor ? 4 : 3) );
    CPLFree(pszConnectionName);
    pszConnectionName = NULL;

    if( hPGConn == NULL || PQstatus(hPGConn) == CONNECTION_BAD )
    {
        CSLDestroy( papszSchemaList );
        CSLDestroy( papszSchemaNames );
        CSLDestroy( papszTableNames );
        CSLDestroy( papszGeomColumnNames );

        CPLFree(pszName);
        pszName = NULL;

        CPLError( CE_Failure, CPLE_AppDefined,
                  "PQconnectdb failed.\n%s",
                  PQerrorMessage(hPGConn) );
        PQfinish(hPGConn);
        hPGConn = NULL;
        return FALSE;
    }

    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      Set the encoding to UTF8 as the driver advertizes UTF8          */
/*      unless PGCLIENTENCODING is defined                              */
/* -------------------------------------------------------------------- */
    if (CPLGetConfigOption("PGCLIENTENCODING", NULL) == NULL)
    {
        const char* encoding = "UNICODE";
        if (PQsetClientEncoding(hPGConn, encoding) == -1)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                    "PQsetClientEncoding(%s) failed.\n%s", 
                    encoding, PQerrorMessage( hPGConn ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Install a notice processor.                                     */
/* -------------------------------------------------------------------- */
    PQsetNoticeProcessor( hPGConn, OGRPGNoticeProcessor, this );

/* -------------------------------------------------------------------- */
/*      Try to establish the database name from the connection          */
/*      string passed.                                                  */
/* -------------------------------------------------------------------- */
    if( strstr(pszNewName, "dbname=") != NULL )
    {
        pszDBName = CPLStrdup( strstr(pszNewName, "dbname=") + 7 );

        for( int i = 0; pszDBName[i] != '\0'; i++ )
        {
            if( pszDBName[i] == ' ' )
            {
                pszDBName[i] = '\0';
                break;
            }
        }
    }
    else if( getenv( "USER" ) != NULL )
        pszDBName = CPLStrdup( getenv("USER") );
    else
        pszDBName = CPLStrdup( "unknown_dbname" );

    CPLDebug( "PG", "DBName=\"%s\"", pszDBName );

/* -------------------------------------------------------------------- */
/*      Set active schema if different from 'public'                    */
/* -------------------------------------------------------------------- */
    if (strcmp(osActiveSchema, "public") != 0)
    {
        CPLString osCommand;
        osCommand.Printf("SET search_path='%s',public", osActiveSchema.c_str());
        PGresult    *hResult = PQexec(hPGConn, osCommand );

        if( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            OGRPGClearResult( hResult );

            CSLDestroy( papszSchemaList );
            CSLDestroy( papszSchemaNames );
            CSLDestroy( papszTableNames );
            CSLDestroy( papszGeomColumnNames );

            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s", PQerrorMessage(hPGConn) );
            return FALSE;
        }

        OGRPGClearResult(hResult);
    }

/* -------------------------------------------------------------------- */
/*      Find out PostgreSQL version                                     */
/* -------------------------------------------------------------------- */
    PGresult    *hResult = NULL;

    sPostgreSQLVersion.nMajor = -1;
    sPostgreSQLVersion.nMinor = -1;
    sPostgreSQLVersion.nRelease = -1;

    hResult = PQexec(hPGConn, "SELECT version()" );
    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
        && PQntuples(hResult) > 0 )
    {
        char * pszVer = PQgetvalue(hResult,0,0);

        CPLDebug("PG","PostgreSQL version string : '%s'", pszVer);

        if (EQUALN(pszVer, "PostgreSQL ", 11))
        {
            OGRPGDecodeVersionString(&sPostgreSQLVersion, pszVer + 11);
            if (sPostgreSQLVersion.nMajor == 7 && sPostgreSQLVersion.nMinor < 4)
            {
                /* We don't support BINARY CURSOR for PostgreSQL < 7.4. */
                /* The binary protocol for arrays seems to be different from later versions */
                CPLDebug("PG","BINARY cursor will finally NOT be used because version < 7.4");
                bUseBinaryCursor = FALSE;
            }
        }
    }
    OGRPGClearResult(hResult);
    CPLAssert(NULL == hResult); /* Test if safe PQclear has not been broken */

/* -------------------------------------------------------------------- */
/*      Test if time binary format is int8 or float8                    */
/* -------------------------------------------------------------------- */
#if !defined(PG_PRE74)
    if (bUseBinaryCursor)
    {
        SoftStartTransaction();

        hResult = PQexec(hPGConn, "DECLARE gettimebinaryformat BINARY CURSOR FOR SELECT CAST ('00:00:01' AS time)");

        if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
        {
            OGRPGClearResult( hResult );

            hResult = PQexec(hPGConn, "FETCH ALL IN gettimebinaryformat" );

            if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK  && PQntuples(hResult) == 1 )
            {
                if ( PQfformat( hResult, 0 ) == 1 ) // Binary data representation
                {
                    CPLAssert(PQgetlength(hResult, 0, 0) == 8);
                    double dVal;
                    unsigned int nVal[2];
                    memcpy( nVal, PQgetvalue( hResult, 0, 0 ), 8 );
                    CPL_MSBPTR32(&nVal[0]);
                    CPL_MSBPTR32(&nVal[1]);
                    memcpy( &dVal, PQgetvalue( hResult, 0, 0 ), 8 );
                    CPL_MSBPTR64(&dVal);
                    if (nVal[0] == 0 && nVal[1] == 1000000)
                    {
                        bBinaryTimeFormatIsInt8 = TRUE;
                        CPLDebug( "PG", "Time binary format is int8");
                    }
                    else if (dVal == 1.)
                    {
                        bBinaryTimeFormatIsInt8 = FALSE;
                        CPLDebug( "PG", "Time binary format is float8");
                    }
                    else
                    {
                        bBinaryTimeFormatIsInt8 = FALSE;
                        CPLDebug( "PG", "Time binary format is unknown");
                    }
                }
            }
        }

        OGRPGClearResult( hResult );

        hResult = PQexec(hPGConn, "CLOSE gettimebinaryformat");
        OGRPGClearResult( hResult );

        SoftCommit();
    }
#endif

/* -------------------------------------------------------------------- */
/*      Test to see if this database instance has support for the       */
/*      PostGIS Geometry type.  If so, disable sequential scanning      */
/*      so we will get the value of the gist indexes.                   */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "BEGIN");

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        OGRPGClearResult( hResult );
        CPLAssert(NULL == hResult);

        hResult = PQexec(hPGConn,
                         "SELECT oid FROM pg_type WHERE typname = 'geometry'" );
    }

    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
        && PQntuples(hResult) > 0  && CSLTestBoolean(CPLGetConfigOption("PG_USE_POSTGIS", "YES")))
    {
        bHavePostGIS = TRUE;
        nGeometryOID = atoi(PQgetvalue(hResult,0,0));
    }
    else
    {
        nGeometryOID = (Oid) 0;
    }

    int bListAllTables = CSLTestBoolean(CPLGetConfigOption("PG_LIST_ALL_TABLES", "NO"));

    OGRPGClearResult( hResult );

/* -------------------------------------------------------------------- */
/*      Find out PostGIS version                                        */
/* -------------------------------------------------------------------- */

    sPostGISVersion.nMajor = -1;
    sPostGISVersion.nMinor = -1;
    sPostGISVersion.nRelease = -1;

    if( bHavePostGIS )
    {
        hResult = PQexec(hPGConn, "SELECT postgis_version()" );
        if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
            && PQntuples(hResult) > 0 )
        {
            char * pszVer = PQgetvalue(hResult,0,0);

            CPLDebug("PG","PostGIS version string : '%s'", pszVer);

            OGRPGDecodeVersionString(&sPostGISVersion, pszVer);

        }
        OGRPGClearResult(hResult);


        if (sPostGISVersion.nMajor == 0 && sPostGISVersion.nMinor < 8)
        {
            // Turning off sequential scans for PostGIS < 0.8
            hResult = PQexec(hPGConn, "SET ENABLE_SEQSCAN = OFF");
            
            CPLDebug( "PG", "SET ENABLE_SEQSCAN=OFF" );
        }
        else
        {
            // PostGIS >=0.8 is correctly integrated with query planner,
            // thus PostgreSQL will use indexes whenever appropriate.
            hResult = PQexec(hPGConn, "SET ENABLE_SEQSCAN = ON");
        }
        OGRPGClearResult( hResult );
    }

    hResult = PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

/* -------------------------------------------------------------------- */
/*      Get a list of available tables if they have not been            */
/*      specified through the TABLES connection string param           */
/* -------------------------------------------------------------------- */

    CPLHashSet* hSetTables = CPLHashSetNew(OGRPGHashTableEntry, OGRPGEqualTableEntry, OGRPGFreeTableEntry);

    if (papszTableNames == NULL)
    {
        CPLString osCommand;
        const char* pszAllowedRelations;
        if( CSLTestBoolean(CPLGetConfigOption("PG_SKIP_VIEWS", "NO")) )
            pszAllowedRelations = "'r'";
        else
            pszAllowedRelations = "'r','v'";
        
        hResult = PQexec(hPGConn, "BEGIN");

        if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
        {
            OGRPGClearResult( hResult );

            /* Caution : in PostGIS case, the result has 3 columns, whereas in the */
            /* non-PostGIS case it has only 2 columns */
            if ( bHavePostGIS && !bListAllTables )
            {
                /* PostGIS 1.5 brings support for 'geography' type. */
                /* Checks that the type exists */
                if (sPostGISVersion.nMajor > 1 ||
                    (sPostGISVersion.nMajor == 1 && sPostGISVersion.nMinor >= 5))
                {
                    hResult = PQexec(hPGConn,
                                    "SELECT oid FROM pg_type WHERE typname = 'geography'" );

                    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
                        && PQntuples(hResult) > 0)
                    {
                        bHaveGeography = TRUE;
                        nGeographyOID = atoi(PQgetvalue(hResult,0,0));
                    }
                    else
                    {
                        CPLDebug("PG", "PostGIS >= 1.5 detected but cannot find 'geography' type");
                    }
                    
                    OGRPGClearResult( hResult );
                }
                
                osCommand.Printf("DECLARE mycursor CURSOR for "
                                 "SELECT c.relname, n.nspname, g.f_geometry_column FROM pg_class c, pg_namespace n, geometry_columns g "
                                 "WHERE (c.relkind in (%s) AND c.relname !~ '^pg_' AND c.relnamespace=n.oid "
                                 "AND c.relname::TEXT = g.f_table_name::TEXT AND n.nspname = g.f_table_schema)",
                                 pszAllowedRelations);

                if (bHaveGeography)
                    osCommand += CPLString().Printf(
                                     "UNION SELECT c.relname, n.nspname, g.f_geography_column FROM pg_class c, pg_namespace n, geography_columns g "
                                     "WHERE (c.relkind in (%s) AND c.relname !~ '^pg_' AND c.relnamespace=n.oid "
                                     "AND c.relname::TEXT = g.f_table_name::TEXT AND n.nspname = g.f_table_schema)",
                                     pszAllowedRelations);
            }
            else
                osCommand.Printf("DECLARE mycursor CURSOR for "
                                 "SELECT c.relname, n.nspname FROM pg_class c, pg_namespace n "
                                 "WHERE (c.relkind in (%s) AND c.relname !~ '^pg_' AND c.relnamespace=n.oid)",
                                 pszAllowedRelations);
                                
            hResult = PQexec(hPGConn, osCommand.c_str());
        }

        if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
        {
            OGRPGClearResult( hResult );
            hResult = PQexec(hPGConn, "FETCH ALL in mycursor" );
        }

        if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
        {
            OGRPGClearResult( hResult );
            CPLHashSetDestroy(hSetTables);

            CSLDestroy( papszSchemaList );
            CSLDestroy( papszSchemaNames );
            CSLDestroy( papszTableNames );
            CSLDestroy( papszGeomColumnNames );

            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s", PQerrorMessage(hPGConn) );
            return FALSE;
        }

    /* -------------------------------------------------------------------- */
    /*      Parse the returned table list                                   */
    /* -------------------------------------------------------------------- */
        for( int iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
        {
            const char *pszTable = PQgetvalue(hResult, iRecord, 0);
            const char *pszSchemaName = PQgetvalue(hResult, iRecord, 1);
            const char *pszGeomColumnName =
                    (bHavePostGIS && !bListAllTables) ? PQgetvalue(hResult, iRecord, 2) : NULL;

            if( EQUAL(pszTable,"spatial_ref_sys")
                || EQUAL(pszTable,"geometry_columns") )
                continue;

            if( EQUAL(pszSchemaName,"information_schema") )
                continue;

            papszTableNames = CSLAddString(papszTableNames, pszTable);
            papszSchemaNames = CSLAddString(papszSchemaNames, pszSchemaName);
            if (pszGeomColumnName)
                papszGeomColumnNames = CSLAddString(papszGeomColumnNames, pszGeomColumnName);

            PGTableEntry* psEntry = OGRPGFindTableEntry(hSetTables, pszTable, pszSchemaName);
            if (psEntry == NULL)
                psEntry = OGRPGAddTableEntry(hSetTables, pszTable, pszSchemaName);
            if (pszGeomColumnName)
                psEntry->papszGeomColumnNames =
                        CSLAddString(psEntry->papszGeomColumnNames, pszGeomColumnName);
        }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
        OGRPGClearResult( hResult );

        hResult = PQexec(hPGConn, "CLOSE mycursor");
        OGRPGClearResult( hResult );

        hResult = PQexec(hPGConn, "COMMIT");
        OGRPGClearResult( hResult );

        if ( bHavePostGIS && !bListAllTables )
        {
            hResult = PQexec(hPGConn, "BEGIN");

            OGRPGClearResult( hResult );

        /* -------------------------------------------------------------------- */
        /*      Fetch inherited tables                                          */
        /* -------------------------------------------------------------------- */
            hResult = PQexec(hPGConn,
                                "DECLARE mycursor CURSOR for "
                                "SELECT c1.relname AS derived, c2.relname AS parent, n.nspname "
                                "FROM pg_class c1, pg_class c2, pg_namespace n, pg_inherits i "
                                "WHERE i.inhparent = c2.oid AND i.inhrelid = c1.oid AND c1.relnamespace=n.oid "
                                "AND c1.relkind in ('r', 'v') AND c1.relnamespace=n.oid AND c2.relkind in ('r','v') "
                                "AND c2.relname !~ '^pg_' AND c2.relnamespace=n.oid");

            if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
            {
                OGRPGClearResult( hResult );
                hResult = PQexec(hPGConn, "FETCH ALL in mycursor" );
            }

            if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
            {
                OGRPGClearResult( hResult );

                CPLHashSetDestroy(hSetTables);

                CSLDestroy( papszSchemaList );
                CSLDestroy( papszSchemaNames );
                CSLDestroy( papszTableNames );
                CSLDestroy( papszGeomColumnNames );

                CPLError( CE_Failure, CPLE_AppDefined,
                        "%s", PQerrorMessage(hPGConn) );
                return FALSE;
            }

        /* -------------------------------------------------------------------- */
        /*      Parse the returned table list                                   */
        /* -------------------------------------------------------------------- */
            int bHasDoneSomething;
            do
            {
                /* Iterate over the tuples while we have managed to resolved at least one */
                /* table to its table parent with a geometry */
                /* For example if we have C inherits B and B inherits A, where A is a base table with a geometry */
                /* The first pass will add B to the set of tables */
                /* The second pass will add C to the set of tables */

                bHasDoneSomething = FALSE;

                for( int iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
                {
                    const char *pszTable = PQgetvalue(hResult, iRecord, 0);
                    const char *pszParentTable = PQgetvalue(hResult, iRecord, 1);
                    const char *pszSchemaName = PQgetvalue(hResult, iRecord, 2);

                    PGTableEntry* psEntry = OGRPGFindTableEntry(hSetTables, pszTable, pszSchemaName);
                    /* We must be careful that a derived table can have its own geometry column(s) */
                    /* and some inherited from another table */
                    if (psEntry == NULL || psEntry->bDerivedInfoAdded == FALSE)
                    {
                        PGTableEntry* psParentEntry =
                                OGRPGFindTableEntry(hSetTables, pszParentTable, pszSchemaName);
                        if (psParentEntry != NULL)
                        {
                            /* The parent table of this table is already in the set, so we */
                            /* can now add the table in the set if it was not in it already */

                            bHasDoneSomething = TRUE;

                            if (psEntry == NULL)
                                psEntry = OGRPGAddTableEntry(hSetTables, pszTable, pszSchemaName);

                            char** iterGeomColumnNames = psParentEntry->papszGeomColumnNames;
                            while(*iterGeomColumnNames)
                            {
                                papszTableNames = CSLAddString(papszTableNames, pszTable);
                                papszSchemaNames = CSLAddString(papszSchemaNames, pszSchemaName);
                                papszGeomColumnNames = CSLAddString(papszGeomColumnNames, *iterGeomColumnNames);

                                psEntry->papszGeomColumnNames =
                                        CSLAddString(psEntry->papszGeomColumnNames, *iterGeomColumnNames);

                                iterGeomColumnNames ++;
                            }

                            psEntry->bDerivedInfoAdded = TRUE;
                        }
                    }
                }
            } while(bHasDoneSomething);

        /* -------------------------------------------------------------------- */
        /*      Cleanup                                                         */
        /* -------------------------------------------------------------------- */
            OGRPGClearResult( hResult );

            hResult = PQexec(hPGConn, "CLOSE mycursor");
            OGRPGClearResult( hResult );

            hResult = PQexec(hPGConn, "COMMIT");
            OGRPGClearResult( hResult );

        }

    }

    CPLString osCurrentSchema = GetCurrentSchema();

/* -------------------------------------------------------------------- */
/*      Register the available tables.                                  */
/* -------------------------------------------------------------------- */
    for( int iRecord = 0;
         papszTableNames != NULL && papszTableNames[iRecord] != NULL;
         iRecord++ )
    {
        PGTableEntry sEntry;
        PGTableEntry* psEntry;
        sEntry.pszTableName = (char*) papszTableNames[iRecord];
        sEntry.pszSchemaName = (char*) papszSchemaNames[iRecord];
        psEntry = (PGTableEntry* )CPLHashSetLookup(hSetTables, &sEntry);

        /* If SCHEMAS= is specified, only take into account tables inside */
        /* one of the specified schemas */
        if (papszSchemaList != NULL &&
            CSLFindString(papszSchemaList, papszSchemaNames[iRecord]) == -1)
        {
            continue;
        }

        /* Some heuristics to preserve backward compatibility with the way that */
        /* layers were reported in GDAL <= 1.5.0 */
        /* That is to say : */
        /* - if we get only one geometry column from the request to geometry_columns */
        /*   then use it but don't report it into layer definition */
        /* - if we get several geometry columns, use their names and report them */
        /*   except for the wkb_geometry column */
        /* - if we get no geometry column, let ReadTableDefinition() parses the columns */
        /*   and find the likely geometry column */

        if (psEntry != NULL && CSLCount(psEntry->papszGeomColumnNames) <= 1)
        {
            if (CSLCount(psEntry->papszGeomColumnNames) == 1)
                OpenTable( osCurrentSchema, papszTableNames[iRecord],
                           papszSchemaNames[iRecord],
                           psEntry->papszGeomColumnNames[0], bUpdate, FALSE, FALSE );
            else
                OpenTable( osCurrentSchema, papszTableNames[iRecord],
                           papszSchemaNames[iRecord], NULL, bUpdate, FALSE, FALSE );
        }
        else
        {
            CPLAssert( papszGeomColumnNames && papszGeomColumnNames[iRecord]);
            if (EQUAL(papszGeomColumnNames[iRecord], ""))
                OpenTable( osCurrentSchema, papszTableNames[iRecord],
                           papszSchemaNames[iRecord], NULL, bUpdate, FALSE, FALSE );
            else
                OpenTable( osCurrentSchema, papszTableNames[iRecord],
                           papszSchemaNames[iRecord], papszGeomColumnNames[iRecord],
                           bUpdate, FALSE, !EQUAL(papszGeomColumnNames[iRecord], "wkb_geometry") );
        }
    }

    CPLHashSetDestroy(hSetTables);

    CSLDestroy( papszSchemaList );
    CSLDestroy( papszSchemaNames );
    CSLDestroy( papszTableNames );
    CSLDestroy( papszGeomColumnNames );

/* -------------------------------------------------------------------- */
    return TRUE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRPGDataSource::OpenTable( CPLString& osCurrentSchema,
                                const char *pszNewName,
                                const char *pszSchemaName,
                                const char * pszGeomColumnIn,
                                int bUpdate,
                                int bTestOpen,
                                int bAdvertizeGeomColumn )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRPGTableLayer  *poLayer;

    poLayer = new OGRPGTableLayer( this, osCurrentSchema,
                                   pszNewName, pszSchemaName,
                                   pszGeomColumnIn, bUpdate,
                                   bAdvertizeGeomColumn );
    if( poLayer->GetLayerDefn() == NULL )
    {
        delete poLayer;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRPGTableLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRPGTableLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

int OGRPGDataSource::DeleteLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osLayerName = papoLayers[iLayer]->GetLayerDefn()->GetName();
    CPLString osTableName = papoLayers[iLayer]->GetTableName();
    CPLString osSchemaName = papoLayers[iLayer]->GetSchemaName();

    CPLDebug( "PG", "DeleteLayer(%s)", osLayerName.c_str() );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */
    PGresult            *hResult;
    CPLString            osCommand;

    hResult = PQexec(hPGConn, "BEGIN");
    OGRPGClearResult( hResult );

    if( bHavePostGIS )
    {
        /* This is unnecessary if the layer is not a geometry table, or an inherited geometry table */
        /* but it shouldn't hurt */
        osCommand.Printf(
                 "SELECT DropGeometryColumn('%s','%s',(SELECT f_geometry_column from geometry_columns where f_table_name='%s' and f_table_schema='%s' order by f_geometry_column limit 1))",
                 osSchemaName.c_str(), osTableName.c_str(), osTableName.c_str(), osSchemaName.c_str() );

        hResult = PQexec( hPGConn, osCommand.c_str() );
        OGRPGClearResult( hResult );
    }

    osCommand.Printf("DROP TABLE \"%s\".\"%s\" CASCADE", osSchemaName.c_str(), osTableName.c_str() );
    hResult = PQexec( hPGConn, osCommand.c_str() );
    OGRPGClearResult( hResult );

    hResult = PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRPGDataSource::CreateLayer( const char * pszLayerName,
                              OGRSpatialReference *poSRS,
                              OGRwkbGeometryType eType,
                              char ** papszOptions )

{
    PGresult            *hResult = NULL;
    CPLString            osCommand;
    const char          *pszGeomType = NULL;
    char                *pszTableName = NULL;
    char                *pszSchemaName = NULL;
    int                 nDimension = 3;

    if (pszLayerName == NULL)
        return NULL;

    if (strncmp(pszLayerName, "pg", 2) == 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The layer name should not begin by 'pg' as it is a reserved prefix");
    }

    if( wkbFlatten(eType) == eType )
        nDimension = 2;

    if( CSLFetchNameValue( papszOptions, "DIM") != NULL )
        nDimension = atoi(CSLFetchNameValue( papszOptions, "DIM"));
        
    /* Postgres Schema handling:
       Extract schema name from input layer name or passed with -lco SCHEMA.
       Set layer name to "schema.table" or to "table" if schema == current_schema()
       Usage without schema name is backwards compatible
    */
    const char* pszDotPos = strstr(pszLayerName,".");
    if ( pszDotPos != NULL )
    {
      int length = pszDotPos - pszLayerName;
      pszSchemaName = (char*)CPLMalloc(length+1);
      strncpy(pszSchemaName, pszLayerName, length);
      pszSchemaName[length] = '\0';
      
      if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
          pszTableName = LaunderName( pszDotPos + 1 ); //skip "."
      else
          pszTableName = CPLStrdup( pszDotPos + 1 ); //skip "."
    }
    else
    {
      pszSchemaName = NULL;
      if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
          pszTableName = LaunderName( pszLayerName ); //skip "."
      else
          pszTableName = CPLStrdup( pszLayerName ); //skip "."
    }

    
/* -------------------------------------------------------------------- */
/*      Set the default schema for the layers.                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszOptions, "SCHEMA" ) != NULL )
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup(CSLFetchNameValue( papszOptions, "SCHEMA" ));
    }

    CPLString osCurrentSchema = GetCurrentSchema();

    if ( pszSchemaName == NULL && strlen(osCurrentSchema) > 0)
    {
      pszSchemaName = CPLStrdup(osCurrentSchema);
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        CPLString osSQLLayerName;
        if (pszSchemaName == NULL || (strlen(osCurrentSchema) > 0 && EQUAL(pszSchemaName, osCurrentSchema.c_str())))
            osSQLLayerName = pszTableName;
        else
        {
            osSQLLayerName = pszSchemaName;
            osSQLLayerName += ".";
            osSQLLayerName += pszTableName;
        }

        if( EQUAL(osSQLLayerName.c_str(),papoLayers[iLayer]->GetLayerDefn()->GetName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                DeleteLayer( iLayer );
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          osSQLLayerName.c_str() );
                CPLFree( pszTableName );
                CPLFree( pszSchemaName );
                return NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle the GEOM_TYPE option.                                    */
/* -------------------------------------------------------------------- */
    pszGeomType = CSLFetchNameValue( papszOptions, "GEOM_TYPE" );
    if( pszGeomType == NULL )
    {
        if( bHavePostGIS )
            pszGeomType = "geometry";
        else
            pszGeomType = "bytea";
    }
    
    if( EQUAL(pszGeomType, "geography") && !bHaveGeography )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GEOM_TYPE=geography is only supported in PostGIS >= 1.5.\n"
                  "Creation of layer %s has failed.",
                  pszLayerName );
        CPLFree( pszTableName );
        CPLFree( pszSchemaName );
        return NULL;
    }

    if( bHavePostGIS && !EQUAL(pszGeomType,"geometry") &&
        !EQUAL(pszGeomType, "geography") )
    {
        if( bHaveGeography )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "GEOM_TYPE in PostGIS enabled databases must be 'geometry' or 'geography'.\n"
                      "Creation of layer %s with GEOM_TYPE %s has failed.",
                      pszLayerName, pszGeomType );
        else
            CPLError( CE_Failure, CPLE_AppDefined,
                      "GEOM_TYPE in PostGIS enabled databases must be 'geometry'.\n"
                      "Creation of layer %s with GEOM_TYPE %s has failed.",
                      pszLayerName, pszGeomType );

        CPLFree( pszTableName );
        CPLFree( pszSchemaName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding tot the srs table if needed.                             */
/* -------------------------------------------------------------------- */
    int nSRSId = -1;

    if( poSRS != NULL )
        nSRSId = FetchSRSId( poSRS );
        
    const char *pszGeometryType;
    switch( wkbFlatten(eType) )
    {
        case wkbPoint:
            pszGeometryType = "POINT";
            break;

        case wkbLineString:
            pszGeometryType = "LINESTRING";
            break;

        case wkbPolygon:
            pszGeometryType = "POLYGON";
            break;

        case wkbMultiPoint:
            pszGeometryType = "MULTIPOINT";
            break;

        case wkbMultiLineString:
            pszGeometryType = "MULTILINESTRING";
            break;

        case wkbMultiPolygon:
            pszGeometryType = "MULTIPOLYGON";
            break;

        case wkbGeometryCollection:
            pszGeometryType = "GEOMETRYCOLLECTION";
            break;

        default:
            pszGeometryType = "GEOMETRY";
            break;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a basic table with the FID.  Also include the            */
/*      geometry if this is not a PostGIS enabled table.                */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "BEGIN");
    OGRPGClearResult( hResult );
    
    const char *pszGFldName = NULL;
    
    CPLString osCreateTable;
    int bTemporary = CSLFetchNameValue( papszOptions, "TEMPORARY" ) != NULL &&
                     CSLTestBoolean(CSLFetchNameValue( papszOptions, "TEMPORARY" ));
    if (bTemporary)
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup("pg_temp_1");
        osCreateTable.Printf("CREATE TEMPORARY TABLE \"%s\"", pszTableName);
    }
    else
        osCreateTable.Printf("CREATE TABLE \"%s\".\"%s\"", pszSchemaName, pszTableName);
    
    if( !bHavePostGIS )
    {
        osCommand.Printf(
                 "%s ( "
                 "   OGC_FID SERIAL, "
                 "   WKB_GEOMETRY %s, "
                 "   CONSTRAINT \"%s_pk\" PRIMARY KEY (OGC_FID) )",
                 osCreateTable.c_str(), pszGeomType, pszTableName );
    }
    else if ( EQUAL(pszGeomType, "geography") )
    {
        if( CSLFetchNameValue( papszOptions, "GEOMETRY_NAME") != NULL )
            pszGFldName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME");
        else
            pszGFldName = "the_geog";
        
        if (nSRSId)
            osCommand.Printf(
                     "%s ( OGC_FID SERIAL, \"%s\" geography(%s%s,%d), CONSTRAINT \"%s_pk\" PRIMARY KEY (OGC_FID) )",
                     osCreateTable.c_str(), pszGFldName, pszGeometryType, nDimension == 2 ? "" : "Z", nSRSId, pszTableName );
        else
            osCommand.Printf(
                     "%s ( OGC_FID SERIAL, \"%s\" geography(%s%s), CONSTRAINT \"%s_pk\" PRIMARY KEY (OGC_FID) )",
                     osCreateTable.c_str(), pszGFldName, pszGeometryType, nDimension == 2 ? "" : "Z", pszTableName );
    }
    else
    {
        osCommand.Printf(
                 "%s ( OGC_FID SERIAL, CONSTRAINT \"%s_pk\" PRIMARY KEY (OGC_FID) )",
                 osCreateTable.c_str(), pszTableName );
    }

    hResult = PQexec(hPGConn, osCommand.c_str());
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s\n%s", osCommand.c_str(), PQerrorMessage(hPGConn) );
        CPLFree( pszTableName );
        CPLFree( pszSchemaName );

        OGRPGClearResult( hResult );
        hResult = PQexec( hPGConn, "ROLLBACK" );
        OGRPGClearResult( hResult );
        return NULL;
    }

    OGRPGClearResult( hResult );

/* -------------------------------------------------------------------- */
/*      Eventually we should be adding this table to a table of         */
/*      "geometric layers", capturing the WKT projection, and           */
/*      perhaps some other housekeeping.                                */
/* -------------------------------------------------------------------- */
    if( bHavePostGIS && !EQUAL(pszGeomType, "geography"))
    {
        if( CSLFetchNameValue( papszOptions, "GEOMETRY_NAME") != NULL )
            pszGFldName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME");
        else
            pszGFldName = "wkb_geometry";

        /* Sometimes there is an old cruft entry in the geometry_columns
         * table if things were not properly cleaned up before.  We make
         * an effort to clean out such cruft.
         */
        osCommand.Printf(
                 "DELETE FROM geometry_columns WHERE f_table_name = '%s' AND f_table_schema = '%s'",
                 pszTableName, pszSchemaName );

        hResult = PQexec(hPGConn, osCommand.c_str());
        OGRPGClearResult( hResult );

        osCommand.Printf(
                 "SELECT AddGeometryColumn('%s','%s','%s',%d,'%s',%d)",
                 pszSchemaName, pszTableName, pszGFldName,
                 nSRSId, pszGeometryType, nDimension );

        hResult = PQexec(hPGConn, osCommand.c_str());

        if( !hResult
            || PQresultStatus(hResult) != PGRES_TUPLES_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "AddGeometryColumn failed for layer %s, layer creation has failed.",
                      pszLayerName );

            CPLFree( pszTableName );
            CPLFree( pszSchemaName );

            OGRPGClearResult( hResult );

            hResult = PQexec(hPGConn, "ROLLBACK");
            OGRPGClearResult( hResult );

            return NULL;
        }

        OGRPGClearResult( hResult );
    }
    
    if( bHavePostGIS )
    {
/* -------------------------------------------------------------------- */
/*      Create the spatial index.                                       */
/*                                                                      */
/*      We're doing this before we add geometry and record to the table */
/*      so this may not be exactly the best way to do it.               */
/* -------------------------------------------------------------------- */
        const char *pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
        if( pszSI == NULL || CSLTestBoolean(pszSI) )
        {
            osCommand.Printf("CREATE INDEX \"%s_geom_idx\" "
                             "ON \"%s\".\"%s\" "
                             "USING GIST (\"%s\")",
                    pszTableName, pszSchemaName, pszTableName, pszGFldName);

            hResult = PQexec(hPGConn, osCommand.c_str());

            if( !hResult
                || PQresultStatus(hResult) != PGRES_COMMAND_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "'%s' failed for layer %s, index creation has failed.",
                        osCommand.c_str(), pszLayerName );

                CPLFree( pszTableName );
                CPLFree( pszSchemaName );

                OGRPGClearResult( hResult );

                hResult = PQexec(hPGConn, "ROLLBACK");
                OGRPGClearResult( hResult );

                return NULL;
            }
            OGRPGClearResult( hResult );
        }
    }

/* -------------------------------------------------------------------- */
/*      Complete, and commit the transaction.                           */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRPGTableLayer     *poLayer;

    poLayer = new OGRPGTableLayer( this, osCurrentSchema, pszTableName, pszSchemaName, NULL, TRUE, FALSE, nSRSId);
    if( poLayer->GetLayerDefn() == NULL )
    {
        CPLFree( pszTableName );
        CPLFree( pszSchemaName );
        delete poLayer;
        return NULL;
    }

    poLayer->SetLaunderFlag( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) );
    poLayer->SetPrecisionFlag( CSLFetchBoolean(papszOptions,"PRECISION",TRUE));

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRPGTableLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRPGTableLayer *) * (nLayers+1) );

    papoLayers[nLayers++] = poLayer;

    CPLFree( pszTableName );
    CPLFree( pszSchemaName );

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) 
        || EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPGDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGRPGDataSource::GetLayerByName( const char *pszName )

{
    char* pszTableName = NULL;
    char *pszGeomColumnName = NULL;
    char *pszSchemaName = NULL;

    if ( ! pszName )
        return NULL;

    int  i;
    
    int count = GetLayerCount();
    /* first a case sensitive check */
    for( i = 0; i < count; i++ )
    {
        OGRPGTableLayer *poLayer = papoLayers[i];

        if( strcmp( pszName, poLayer->GetLayerDefn()->GetName() ) == 0 )
        {
            return poLayer;
        }
    }
        
    /* then case insensitive */
    for( i = 0; i < count; i++ )
    {
        OGRPGTableLayer *poLayer = papoLayers[i];

        if( EQUAL( pszName, poLayer->GetLayerDefn()->GetName() ) )
        {
            return poLayer;
        }
    }

    char* pszNameWithoutBracket = CPLStrdup(pszName);
    char *pos = strchr(pszNameWithoutBracket, '(');
    if (pos != NULL)
    {
        *pos = '\0';
        pszGeomColumnName = CPLStrdup(pos+1);
        int len = strlen(pszGeomColumnName);
        if (len > 0)
            pszGeomColumnName[len - 1] = '\0';
    }

    pos = strchr(pszNameWithoutBracket, '.');
    if (pos != NULL)
    {
        *pos = '\0';
        pszSchemaName = CPLStrdup(pszNameWithoutBracket);
        pszTableName = CPLStrdup(pos + 1);
    }
    else
    {
        pszTableName = CPLStrdup(pszNameWithoutBracket);
    }
    CPLFree(pszNameWithoutBracket);
    pszNameWithoutBracket = NULL;

    CPLString osCurrentSchema = GetCurrentSchema();
    int bRet = OpenTable( osCurrentSchema, pszTableName, pszSchemaName,
                          pszGeomColumnName, TRUE, FALSE, TRUE );
    CPLFree(pszTableName);
    CPLFree(pszSchemaName);
    CPLFree(pszGeomColumnName);

    if (bRet)
    {
        return GetLayer(count);
    }
    else
    {
        return NULL;
    }
}


/************************************************************************/
/*                        OGRPGNoticeProcessor()                        */
/************************************************************************/

static void OGRPGNoticeProcessor( void *arg, const char * pszMessage )

{
    CPLDebug( "OGR_PG_NOTICE", "%s", pszMessage );
}

/************************************************************************/
/*                      InitializeMetadataTables()                      */
/*                                                                      */
/*      Create the metadata tables (SPATIAL_REF_SYS and                 */
/*      GEOMETRY_COLUMNS).                                              */
/************************************************************************/

OGRErr OGRPGDataSource::InitializeMetadataTables()

{
    // implement later.
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference *OGRPGDataSource::FetchSRS( int nId )

{
    if( nId < 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      First, we look through our SRID cache, is it there?             */
/* -------------------------------------------------------------------- */
    int  i;

    for( i = 0; i < nKnownSRID; i++ )
    {
        if( panSRID[i] == nId )
            return papoSRS[i];
    }

/* -------------------------------------------------------------------- */
/*      Try looking up in spatial_ref_sys table.                        */
/* -------------------------------------------------------------------- */
    PGresult        *hResult = NULL;
    CPLString        osCommand;
    OGRSpatialReference *poSRS = NULL;

    SoftStartTransaction();

    osCommand.Printf(
             "SELECT srtext FROM spatial_ref_sys "
             "WHERE srid = %d",
             nId );
    hResult = PQexec(hPGConn, osCommand.c_str() );

    if( hResult
        && PQresultStatus(hResult) == PGRES_TUPLES_OK
        && PQntuples(hResult) == 1 )
    {
        char *pszWKT;

        pszWKT = PQgetvalue(hResult,0,0);
        poSRS = new OGRSpatialReference();
        if( poSRS->importFromWkt( &pszWKT ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }

    OGRPGClearResult( hResult );
    SoftCommit();

/* -------------------------------------------------------------------- */
/*      Add to the cache.                                               */
/* -------------------------------------------------------------------- */
    panSRID = (int *) CPLRealloc(panSRID,sizeof(int) * (nKnownSRID+1) );
    papoSRS = (OGRSpatialReference **)
        CPLRealloc(papoSRS, sizeof(void*) * (nKnownSRID + 1) );
    panSRID[nKnownSRID] = nId;
    papoSRS[nKnownSRID] = poSRS;
    nKnownSRID++;

    return poSRS;
}

/************************************************************************/
/*                             FetchSRSId()                             */
/*                                                                      */
/*      Fetch the id corresponding to an SRS, and if not found, add     */
/*      it to the table.                                                */
/************************************************************************/

int OGRPGDataSource::FetchSRSId( OGRSpatialReference * poSRS )

{
    PGresult            *hResult = NULL;
    CPLString           osCommand;
    char                *pszWKT = NULL;
    int                 nSRSId = -1;
    const char*         pszAuthorityName;

    if( poSRS == NULL )
        return -1;

    OGRSpatialReference oSRS(*poSRS);
    poSRS = NULL;

    pszAuthorityName = oSRS.GetAuthorityName(NULL);

    if( pszAuthorityName == NULL || strlen(pszAuthorityName) == 0 )
    {
/* -------------------------------------------------------------------- */
/*      Try to identify an EPSG code                                    */
/* -------------------------------------------------------------------- */
        oSRS.AutoIdentifyEPSG();

        pszAuthorityName = oSRS.GetAuthorityName(NULL);
        if (pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG"))
        {
            const char* pszAuthorityCode = oSRS.GetAuthorityCode(NULL);
            if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                oSRS.importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = oSRS.GetAuthorityName(NULL);
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      Check whether the EPSG authority code is already mapped to a    */
/*      SRS ID.                                                         */
/* -------------------------------------------------------------------- */
    if( pszAuthorityName != NULL && EQUAL( pszAuthorityName, "EPSG" ) )
    {
        int             nAuthorityCode;

        /* For the root authority name 'EPSG', the authority code
         * should always be integral
         */
        nAuthorityCode = atoi( oSRS.GetAuthorityCode(NULL) );

        osCommand.Printf("SELECT srid FROM spatial_ref_sys WHERE "
                         "auth_name = '%s' AND auth_srid = %d",
                         pszAuthorityName,
                         nAuthorityCode );
        hResult = PQexec(hPGConn, osCommand.c_str());

        if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
            && PQntuples(hResult) > 0 )
        {
            nSRSId = atoi(PQgetvalue( hResult, 0, 0 ));

            OGRPGClearResult( hResult );

            return nSRSId;
        }

        OGRPGClearResult( hResult );
    }

/* -------------------------------------------------------------------- */
/*      Translate SRS to WKT.                                           */
/* -------------------------------------------------------------------- */
    if( oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        return -1;
    }

/* -------------------------------------------------------------------- */
/*      Try to find in the existing table.                              */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "BEGIN");
    OGRPGClearResult( hResult );

    CPLString osWKT = OGRPGEscapeString(hPGConn, pszWKT, -1, "srtext");
    osCommand.Printf(
             "SELECT srid FROM spatial_ref_sys WHERE srtext = %s",
             osWKT.c_str() );
    hResult = PQexec(hPGConn, osCommand.c_str() );
    CPLFree( pszWKT );  // CM:  Added to prevent mem leaks
    pszWKT = NULL;      // CM:  Added

/* -------------------------------------------------------------------- */
/*      We got it!  Return it.                                          */
/* -------------------------------------------------------------------- */
    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
        && PQntuples(hResult) > 0 )
    {
        nSRSId = atoi(PQgetvalue( hResult, 0, 0 ));

        OGRPGClearResult( hResult );

        hResult = PQexec(hPGConn, "COMMIT");
        OGRPGClearResult( hResult );

        return nSRSId;
    }

/* -------------------------------------------------------------------- */
/*      If the command actually failed, then the metadata table is      */
/*      likely missing. Try defining it.                                */
/* -------------------------------------------------------------------- */
    int         bTableMissing;

    bTableMissing =
        hResult == NULL || PQresultStatus(hResult) == PGRES_NONFATAL_ERROR;

    OGRPGClearResult( hResult );

    hResult = PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

    if( bTableMissing )
    {
        if( InitializeMetadataTables() != OGRERR_NONE )
            return -1;
    }

/* -------------------------------------------------------------------- */
/*      Get the current maximum srid in the srs table.                  */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "BEGIN");
    OGRPGClearResult( hResult );

    hResult = PQexec(hPGConn, "SELECT MAX(srid) FROM spatial_ref_sys" );

    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK )
    {
        nSRSId = atoi(PQgetvalue(hResult,0,0)) + 1;
        OGRPGClearResult( hResult );
    }
    else
    {
        nSRSId = 1;
    }

/* -------------------------------------------------------------------- */
/*      Try adding the SRS to the SRS table.                            */
/* -------------------------------------------------------------------- */
    char    *pszProj4 = NULL;
    if( oSRS.exportToProj4( &pszProj4 ) != OGRERR_NONE )
    {
        CPLFree( pszProj4 );
        return -1;
    }

    CPLString osProj4 = OGRPGEscapeString(hPGConn, pszProj4, -1, "proj4text");

    if( pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG") )
    {
        int             nAuthorityCode;

        nAuthorityCode = atoi( oSRS.GetAuthorityCode(NULL) );

        osCommand.Printf(
                 "INSERT INTO spatial_ref_sys (srid,srtext,proj4text,auth_name,auth_srid) "
                 "VALUES (%d, %s, %s, '%s', %d)",
                 nSRSId, osWKT.c_str(), osProj4.c_str(),
                 pszAuthorityName, nAuthorityCode );
    }
    else
    {
        osCommand.Printf(
                 "INSERT INTO spatial_ref_sys (srid,srtext,proj4text) VALUES (%d,%s,%s)",
                 nSRSId, osWKT.c_str(), osProj4.c_str() );
    }

    // Free everything that was allocated.
    CPLFree( pszProj4 );
    CPLFree( pszWKT);

    hResult = PQexec(hPGConn, osCommand.c_str() );
    OGRPGClearResult( hResult );

    hResult = PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

    return nSRSId;
}

/************************************************************************/
/*                        SoftStartTransaction()                        */
/*                                                                      */
/*      Create a transaction scope.  If we already have a               */
/*      transaction active this isn't a real transaction, but just      */
/*      an increment to the scope count.                                */
/************************************************************************/

OGRErr OGRPGDataSource::SoftStartTransaction()

{
    nSoftTransactionLevel++;

    if( nSoftTransactionLevel == 1 )
    {
        PGresult    *hResult = NULL;
        PGconn      *hPGConn = GetPGConn();

        //CPLDebug( "PG", "BEGIN Transaction" );
        hResult = PQexec(hPGConn, "BEGIN");

        if( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            OGRPGClearResult( hResult );

            CPLDebug( "PG", "BEGIN Transaction failed:\n%s",
                      PQerrorMessage( hPGConn ) );
            return OGRERR_FAILURE;
        }

        OGRPGClearResult( hResult );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             SoftCommit()                             */
/*                                                                      */
/*      Commit the current transaction if we are at the outer           */
/*      scope.                                                          */
/************************************************************************/

OGRErr OGRPGDataSource::SoftCommit()

{
    EndCopy();

    if( nSoftTransactionLevel <= 0 )
    {
        CPLDebug( "PG", "SoftCommit() with no transaction active." );
        return OGRERR_FAILURE;
    }

    nSoftTransactionLevel--;

    if( nSoftTransactionLevel == 0 )
    {
        PGresult    *hResult = NULL;
        PGconn      *hPGConn = GetPGConn();

        //CPLDebug( "PG", "COMMIT Transaction" );
        hResult = PQexec(hPGConn, "COMMIT");

        if( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            OGRPGClearResult( hResult );

            CPLDebug( "PG", "COMMIT Transaction failed:\n%s",
                      PQerrorMessage( hPGConn ) );
            return OGRERR_FAILURE;
        }
        
        OGRPGClearResult( hResult );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            SoftRollback()                            */
/*                                                                      */
/*      Force a rollback of the current transaction if there is one,    */
/*      even if we are nested several levels deep.                      */
/************************************************************************/

OGRErr OGRPGDataSource::SoftRollback()

{
    if( nSoftTransactionLevel <= 0 )
    {
        CPLDebug( "PG", "SoftRollback() with no transaction active." );
        return OGRERR_FAILURE;
    }

    nSoftTransactionLevel = 0;

    PGresult    *hResult = NULL;
    PGconn      *hPGConn = GetPGConn();

    hResult = PQexec(hPGConn, "ROLLBACK");

    if( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        OGRPGClearResult( hResult );

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    return OGRERR_NONE;
}

/************************************************************************/
/*                        FlushSoftTransaction()                        */
/*                                                                      */
/*      Force the unwinding of any active transaction, and it's         */
/*      commit.                                                         */
/************************************************************************/

OGRErr OGRPGDataSource::FlushSoftTransaction()

{
    /* This must come first because of ogr2ogr.  If you want
       to use ogr2ogr with COPY support, then you must specify
       that ogr2ogr does not use transactions.  Thus, 
       nSoftTransactionLevel will always be zero, so this has
       to come first. */
    EndCopy(); 

    if( nSoftTransactionLevel <= 0 )
        return OGRERR_NONE;

    nSoftTransactionLevel = 1;

    return SoftCommit();
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRPGDataSource::ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect )

{
/* -------------------------------------------------------------------- */
/*      Use generic implementation for OGRSQL dialect.                  */
/* -------------------------------------------------------------------- */
    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
        return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"DELLAYER:",9) )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;
        
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( EQUAL(papoLayers[iLayer]->GetLayerDefn()->GetName(), 
                      pszLayerName ))
            {
                DeleteLayer( iLayer );
                break;
            }
        }
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Execute the statement.                                          */
/* -------------------------------------------------------------------- */
    PGresult    *hResult = NULL;

    FlushSoftTransaction();

    if( EQUALN(pszSQLCommand,"VACUUM",6) 
        || SoftStartTransaction() == OGRERR_NONE  )
    {
        if (EQUALN(pszSQLCommand, "SELECT", 6) == FALSE)
        {
            hResult = PQexec(hPGConn, pszSQLCommand );
            CPLDebug( "PG", "Command Results Tuples = %d", PQntuples(hResult) );
        }
        else
        {
            CPLString osCommand;
            osCommand.Printf( "DECLARE %s CURSOR for %s",
                                "executeSQLCursor", pszSQLCommand );

            hResult = PQexec(hPGConn, osCommand );
            OGRPGClearResult( hResult );

            osCommand.Printf( "FETCH 0 in %s", "executeSQLCursor" );
            hResult = PQexec(hPGConn, osCommand );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a tuple result? If so, instantiate a results         */
/*      layer for it.                                                   */
/* -------------------------------------------------------------------- */

    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
        && (EQUALN(pszSQLCommand, "SELECT", 6) || PQntuples(hResult) > 0) )
    {
        OGRPGResultLayer *poLayer = NULL;

        poLayer = new OGRPGResultLayer( this, pszSQLCommand, hResult );

        OGRPGClearResult( hResult );

        if( poSpatialFilter != NULL )
            poLayer->SetSpatialFilter( poSpatialFilter );

        return poLayer;
    }

/* -------------------------------------------------------------------- */
/*      Generate an error report if an error occured.                   */
/* -------------------------------------------------------------------- */
    if( hResult &&
        (PQresultStatus(hResult) == PGRES_NONFATAL_ERROR
         || PQresultStatus(hResult) == PGRES_FATAL_ERROR ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQresultErrorMessage( hResult ) );
    }

    OGRPGClearResult( hResult );

    FlushSoftTransaction();

    return NULL;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRPGDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                            LaunderName()                             */
/************************************************************************/

char *OGRPGDataSource::LaunderName( const char *pszSrcName )

{
    char    *pszSafeName = CPLStrdup( pszSrcName );

    for( int i = 0; pszSafeName[i] != '\0'; i++ )
    {
        pszSafeName[i] = (char) tolower( pszSafeName[i] );
        if( pszSafeName[i] == '-' || pszSafeName[i] == '#' )
            pszSafeName[i] = '_';
    }

    if( strcmp(pszSrcName,pszSafeName) != 0 )
        CPLDebug("PG","LaunderName('%s') -> '%s'", 
                 pszSrcName, pszSafeName);

    return pszSafeName;
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/
void OGRPGDataSource::StartCopy( OGRPGTableLayer *poPGLayer )
{
    EndCopy();
    poLayerInCopyMode = poPGLayer;
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/
OGRErr OGRPGDataSource::EndCopy( )
{
    if( poLayerInCopyMode != NULL )
    {
        OGRErr result = poLayerInCopyMode->EndCopy();
        poLayerInCopyMode = NULL;

        return result;
    }
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                           CopyInProgress()                           */
/************************************************************************/
int OGRPGDataSource::CopyInProgress( )
{
    return ( poLayerInCopyMode != NULL );
}
