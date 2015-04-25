/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_hash_set.h"
#include <set>

#define PQexec this_is_an_error

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
    bUserTransactionActive = FALSE;
    bSavePointActive = FALSE;
    nSoftTransactionLevel = 0;
    bBinaryTimeFormatIsInt8 = FALSE;
    bUseEscapeStringSyntax = FALSE;
    
    nGeometryOID = (Oid) 0;
    nGeographyOID = (Oid) 0;

    nKnownSRID = 0;
    panSRID = NULL;
    papoSRS = NULL;

    poLayerInCopyMode = NULL;
    nUndefinedSRID = -1; /* actual value will be autotected if PostGIS >= 2.0 detected */

    pszForcedTables = NULL;
    papszSchemaList = NULL;
    bListAllTables = FALSE;
    bHasLoadTables = FALSE;
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
    CPLFree( pszForcedTables );
    CSLDestroy( papszSchemaList );

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
/*                              FlushCache()                            */
/************************************************************************/

void OGRPGDataSource::FlushCache(void)
{
    EndCopy();
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        papoLayers[iLayer]->RunDifferedCreationIfNecessary();
    }
}

/************************************************************************/
/*                         GetCurrentSchema()                           */
/************************************************************************/

CPLString OGRPGDataSource::GetCurrentSchema()
{
    /* -------------------------------------------- */
    /*          Get the current schema              */
    /* -------------------------------------------- */
    PGresult    *hResult = OGRPG_PQexec(hPGConn,"SELECT current_schema()");
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

void OGRPGDataSource::OGRPGDecodeVersionString(PGver* psVersion, const char* pszVer)
{
    GUInt32 iLen;
    const char* ptr;
    char szNum[25];
    char szVer[10];

    while ( *pszVer == ' ' ) pszVer++;

    ptr = pszVer;
    // get Version string
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

    if (*ptr == 0)
        return;
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
    int   nGeomColumnCount;
    PGGeomColumnDesc* pasGeomColumns;   /* list of geometry columns */
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

static void OGRPGTableEntryAddGeomColumn(PGTableEntry* psTableEntry,
                                         const char* pszName,
                                         const char* pszGeomType = NULL,
                                         int nCoordDimension = 0,
                                         int nSRID = UNDETERMINED_SRID,
                                         PostgisType ePostgisType = GEOM_TYPE_UNKNOWN,
                                         int bNullable = TRUE)
{
    psTableEntry->pasGeomColumns = (PGGeomColumnDesc*)
        CPLRealloc(psTableEntry->pasGeomColumns,
               sizeof(PGGeomColumnDesc) * (psTableEntry->nGeomColumnCount + 1));
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].pszName = CPLStrdup(pszName);
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].pszGeomType = (pszGeomType) ? CPLStrdup(pszGeomType) : NULL;
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].nCoordDimension = nCoordDimension;
    /* With PostGIS 2.0, querying geometry_columns can return 0, not only when */
    /* the SRID is truly set to 0, but also when there's no constraint */
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].nSRID = nSRID > 0 ? nSRID : UNDETERMINED_SRID;
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].ePostgisType = ePostgisType;
    psTableEntry->pasGeomColumns[psTableEntry->nGeomColumnCount].bNullable = bNullable;
    psTableEntry->nGeomColumnCount ++;
}

static void OGRPGTableEntryAddGeomColumn(PGTableEntry* psTableEntry,
                                         const PGGeomColumnDesc* psGeomColumnDesc)
{
    OGRPGTableEntryAddGeomColumn(psTableEntry,
                                 psGeomColumnDesc->pszName,
                                 psGeomColumnDesc->pszGeomType,
                                 psGeomColumnDesc->nCoordDimension,
                                 psGeomColumnDesc->nSRID,
                                 psGeomColumnDesc->ePostgisType,
                                 psGeomColumnDesc->bNullable);
}

static void OGRPGFreeTableEntry(void * _psTableEntry)
{
    PGTableEntry* psTableEntry = (PGTableEntry*)_psTableEntry;
    CPLFree(psTableEntry->pszTableName);
    CPLFree(psTableEntry->pszSchemaName);
    int i;
    for(i=0;i<psTableEntry->nGeomColumnCount;i++)
    {
        CPLFree(psTableEntry->pasGeomColumns[i].pszName);
        CPLFree(psTableEntry->pasGeomColumns[i].pszGeomType);
    }
    CPLFree(psTableEntry->pasGeomColumns);
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
                           int bTestOpen, char** papszOpenOptions )

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
    
    CPLString osConnectionName(pszName);
    const char* apszOpenOptions[] = { "dbname", "port", "user", "password",
                                      "host", "active_schema", "schemas", "tables" };
    for(int i=0; i <(int)(sizeof(apszOpenOptions)/sizeof(char*));i++)
    {
        const char* pszVal = CSLFetchNameValue(papszOpenOptions, apszOpenOptions[i]);
        if( pszVal )
        {
            if( osConnectionName[osConnectionName.size()-1] != ':' )
                osConnectionName += " ";
            osConnectionName += apszOpenOptions[i];
            osConnectionName += "=";
            osConnectionName += pszVal;
        }
    }
    
    char* pszConnectionName = CPLStrdup(osConnectionName);
    

/* -------------------------------------------------------------------- */
/*      Determine if the connection string contains an optional         */
/*      ACTIVE_SCHEMA portion. If so, parse it out.                     */
/* -------------------------------------------------------------------- */
    char             *pszActiveSchemaStart;
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

    char             *pszTableStart;
    pszTableStart = strstr(pszConnectionName, "tables=");
    if (pszTableStart == NULL)
        pszTableStart = strstr(pszConnectionName, "TABLES=");

    if( pszTableStart != NULL )
    {
        const char     *pszEnd = NULL;

        pszForcedTables = CPLStrdup( pszTableStart + 7 );

        pszEnd = strchr(pszTableStart, ' ');
        if( pszEnd == NULL )
            pszEnd = pszConnectionName + strlen(pszConnectionName);

        // Remove TABLES=xxxxx from pszConnectionName string
        memmove( pszTableStart, pszEnd, strlen(pszEnd) + 1 );

        pszForcedTables[pszEnd - pszTableStart - 7] = '\0';
    }


    CPLString      osCurrentSchema;
    PGresult      *hResult = NULL;

/* -------------------------------------------------------------------- */
/*      Try to establish connection.                                    */
/* -------------------------------------------------------------------- */
    hPGConn = PQconnectdb( pszConnectionName + (bUseBinaryCursor ? 4 : 3) );
    CPLFree(pszConnectionName);
    pszConnectionName = NULL;

    if( hPGConn == NULL || PQstatus(hPGConn) == CONNECTION_BAD )
    {
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
        PGresult    *hResult = OGRPG_PQexec(hPGConn, osCommand );

        if( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            OGRPGClearResult( hResult );
            CPLDebug("PG","Command \"%s\" failed. Trying without 'public'.",osCommand.c_str());
            osCommand.Printf("SET search_path='%s'", osActiveSchema.c_str());
            PGresult    *hResult2 = OGRPG_PQexec(hPGConn, osCommand );

            if( !hResult2 || PQresultStatus(hResult2) != PGRES_COMMAND_OK )
            {
                OGRPGClearResult( hResult2 );

                CPLError( CE_Failure, CPLE_AppDefined,
                        "%s", PQerrorMessage(hPGConn) );

                return FALSE;
            }
        }

        OGRPGClearResult(hResult);
    }

/* -------------------------------------------------------------------- */
/*      Find out PostgreSQL version                                     */
/* -------------------------------------------------------------------- */
    sPostgreSQLVersion.nMajor = -1;
    sPostgreSQLVersion.nMinor = -1;
    sPostgreSQLVersion.nRelease = -1;

    hResult = OGRPG_PQexec(hPGConn, "SELECT version()" );
    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
        && PQntuples(hResult) > 0 )
    {
        const char* pszSpace;
        char * pszVer = PQgetvalue(hResult,0,0);

        CPLDebug("PG","PostgreSQL version string : '%s'", pszVer);

        /* Should work with "PostgreSQL X.Y.Z ..." or "EnterpriseDB X.Y.Z ..." */
        pszSpace = strchr(pszVer, ' ');
        if( pszSpace != NULL && isdigit(pszSpace[1]) )
        {
            OGRPGDecodeVersionString(&sPostgreSQLVersion, pszSpace + 1);
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
/*      Test if standard_conforming_strings is recognized               */
/* -------------------------------------------------------------------- */

    hResult = OGRPG_PQexec(hPGConn, "SHOW standard_conforming_strings" );
    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
        && PQntuples(hResult) == 1 )
    {
        /* Whatever the value is, it means that we can use the E'' */
        /* syntax */
        bUseEscapeStringSyntax = TRUE;
    }
    OGRPGClearResult(hResult);

/* -------------------------------------------------------------------- */
/*      Test if time binary format is int8 or float8                    */
/* -------------------------------------------------------------------- */
#if !defined(PG_PRE74)
    if (bUseBinaryCursor)
    {
        SoftStartTransaction();

        hResult = OGRPG_PQexec(hPGConn, "DECLARE gettimebinaryformat BINARY CURSOR FOR SELECT CAST ('00:00:01' AS time)");

        if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
        {
            OGRPGClearResult( hResult );

            hResult = OGRPG_PQexec(hPGConn, "FETCH ALL IN gettimebinaryformat" );

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

        hResult = OGRPG_PQexec(hPGConn, "CLOSE gettimebinaryformat");
        OGRPGClearResult( hResult );

        SoftCommitTransaction();
    }
#endif

#ifdef notdef
    /* This would be the quickest fix... instead, ogrpglayer has been updated to support */
    /* bytea hex format */
    if (sPostgreSQLVersion.nMajor >= 9)
    {
        /* Starting with PostgreSQL 9.0, the default output format for values of type bytea */
        /* is hex, whereas we traditionnaly expect escape */
        hResult = OGRPG_PQexec(hPGConn, "SET bytea_output TO escape");
        OGRPGClearResult( hResult );
    }
#endif

/* -------------------------------------------------------------------- */
/*      Test to see if this database instance has support for the       */
/*      PostGIS Geometry type.  If so, disable sequential scanning      */
/*      so we will get the value of the gist indexes.                   */
/* -------------------------------------------------------------------- */
    hResult = OGRPG_PQexec(hPGConn,
                        "SELECT oid, typname FROM pg_type WHERE typname IN ('geometry', 'geography')" );

    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
        && PQntuples(hResult) > 0  && CSLTestBoolean(CPLGetConfigOption("PG_USE_POSTGIS", "YES")))
    {
        for( int iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
        {
            const char *pszOid = PQgetvalue(hResult, iRecord, 0);
            const char *pszTypname = PQgetvalue(hResult, iRecord, 1);
            if( EQUAL(pszTypname, "geometry") )
            {
                bHavePostGIS = TRUE;
                nGeometryOID = atoi(pszOid);
            }
            else if( CSLTestBoolean(CPLGetConfigOption("PG_USE_GEOGRAPHY", "YES")) )
            {
                bHaveGeography = TRUE;
                nGeographyOID = atoi(pszOid);
            }
        }
    }

    OGRPGClearResult( hResult );

/* -------------------------------------------------------------------- */
/*      Find out PostGIS version                                        */
/* -------------------------------------------------------------------- */

    sPostGISVersion.nMajor = -1;
    sPostGISVersion.nMinor = -1;
    sPostGISVersion.nRelease = -1;

    if( bHavePostGIS )
    {
        hResult = OGRPG_PQexec(hPGConn, "SELECT postgis_version()" );
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
            hResult = OGRPG_PQexec(hPGConn, "SET ENABLE_SEQSCAN = OFF");
            
            CPLDebug( "PG", "SET ENABLE_SEQSCAN=OFF" );
        }
        else
        {
            // PostGIS >=0.8 is correctly integrated with query planner,
            // thus PostgreSQL will use indexes whenever appropriate.
            hResult = OGRPG_PQexec(hPGConn, "SET ENABLE_SEQSCAN = ON");
        }
        OGRPGClearResult( hResult );
    }

/* -------------------------------------------------------------------- */
/*      Find out "unknown SRID" value                                   */
/* -------------------------------------------------------------------- */

    if (sPostGISVersion.nMajor >= 2)
    {
        hResult = OGRPG_PQexec(hPGConn,
                        "SELECT ST_Srid('POINT EMPTY'::GEOMETRY)" );

        if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
            && PQntuples(hResult) > 0)
        {
            nUndefinedSRID = atoi(PQgetvalue(hResult,0,0));
        }

        OGRPGClearResult( hResult );
    }
    else
        nUndefinedSRID = -1;

    osCurrentSchema = GetCurrentSchema();

    bListAllTables = CSLTestBoolean(CSLFetchNameValueDef(
        papszOpenOptions, "LIST_ALL_TABLES",
        CPLGetConfigOption("PG_LIST_ALL_TABLES", "NO")));

    return TRUE;
}

/************************************************************************/
/*                            LoadTables()                              */
/************************************************************************/

void OGRPGDataSource::LoadTables()
{
    if( bHasLoadTables )
        return;
    bHasLoadTables = TRUE;

    PGresult            *hResult;
    PGTableEntry **papsTables = NULL;
    int            nTableCount = 0;
    CPLHashSet    *hSetTables = NULL;
    std::set<CPLString> osRegisteredLayers;

    int i;
    for( i = 0; i < nLayers; i++) 
    {
        osRegisteredLayers.insert(papoLayers[i]->GetName());
    }

    if( pszForcedTables )
    {
        char          **papszTableList;

        papszTableList = CSLTokenizeString2( pszForcedTables, ",", 0 );

        for( i = 0; i < CSLCount(papszTableList); i++ )
        {
            char      **papszQualifiedParts;

            // Get schema and table name
            papszQualifiedParts = CSLTokenizeString2( papszTableList[i],
                                                      ".", 0 );
            int nParts = CSLCount( papszQualifiedParts );

            if( nParts == 1 || nParts == 2 )
            {
                /* Find the geometry column name if specified */
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

                papsTables = (PGTableEntry**)CPLRealloc(papsTables, sizeof(PGTableEntry*) * (nTableCount + 1));
                papsTables[nTableCount] = (PGTableEntry*) CPLCalloc(1, sizeof(PGTableEntry));
                if (pszGeomColumnName)
                    OGRPGTableEntryAddGeomColumn(papsTables[nTableCount], pszGeomColumnName);

                if( nParts == 2 )
                {
                    papsTables[nTableCount]->pszSchemaName = CPLStrdup( papszQualifiedParts[0] );
                    papsTables[nTableCount]->pszTableName = CPLStrdup( papszQualifiedParts[1] );
                }
                else
                {
                    papsTables[nTableCount]->pszSchemaName = CPLStrdup( osActiveSchema.c_str());
                    papsTables[nTableCount]->pszTableName = CPLStrdup( papszQualifiedParts[0] );
                }
                nTableCount ++;
            }

            CSLDestroy(papszQualifiedParts);
        }

        CSLDestroy(papszTableList);
    }

/* -------------------------------------------------------------------- */
/*      Get a list of available tables if they have not been            */
/*      specified through the TABLES connection string param           */
/* -------------------------------------------------------------------- */
    const char* pszAllowedRelations;
    if( CSLTestBoolean(CPLGetConfigOption("PG_SKIP_VIEWS", "NO")) )
        pszAllowedRelations = "'r'";
    else
        pszAllowedRelations = "'r','v','m','f'";

    hSetTables = CPLHashSetNew(OGRPGHashTableEntry, OGRPGEqualTableEntry, OGRPGFreeTableEntry);

    if( nTableCount == 0 && bHavePostGIS && sPostGISVersion.nMajor >= 2 &&
        !bListAllTables &&
        /* Config option mostly for comparison/debugging/etc... */
        CSLTestBoolean(CPLGetConfigOption("PG_USE_POSTGIS2_OPTIM", "YES")) )
    {
/* -------------------------------------------------------------------- */
/*      With PostGIS 2.0, the geometry_columns and geography_columns    */
/*      are views, based on the catalog system, that can be slow to     */
/*      query, so query directly the catalog system.                    */
/*      See http://trac.osgeo.org/postgis/ticket/3092                   */
/* -------------------------------------------------------------------- */
        CPLString osCommand;
        osCommand.Printf(
              "SELECT c.relname, n.nspname, c.relkind, a.attname, t.typname, "
              "postgis_typmod_dims(a.atttypmod) dim, "
              "postgis_typmod_srid(a.atttypmod) srid, "
              "postgis_typmod_type(a.atttypmod)::text geomtyp, "
              "array_agg(s.consrc)::text att_constraints, a.attnotnull "
              "FROM pg_class c JOIN pg_attribute a ON a.attrelid=c.oid "
              "JOIN pg_namespace n ON c.relnamespace = n.oid "
              "AND c.relkind in (%s) AND NOT ( n.nspname = 'public' AND c.relname = 'raster_columns' ) "
              "JOIN pg_type t ON a.atttypid = t.oid AND (t.typname = 'geometry'::name OR t.typname = 'geography'::name) "
              "LEFT JOIN pg_constraint s ON s.connamespace = n.oid AND s.conrelid = c.oid "
              "AND a.attnum = ANY (s.conkey) "
              "AND (s.consrc LIKE '%%geometrytype(%% = %%' OR s.consrc LIKE '%%ndims(%% = %%' OR s.consrc LIKE '%%srid(%% = %%') "
              "GROUP BY c.relname, n.nspname, c.relkind, a.attname, t.typname, dim, srid, geomtyp, a.attnotnull, c.oid, a.attnum "
              "ORDER BY c.oid, a.attnum",
              pszAllowedRelations);
        hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

        if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
        {
            OGRPGClearResult( hResult );

            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s", PQerrorMessage(hPGConn) );
            goto end;
        }
    /* -------------------------------------------------------------------- */
    /*      Parse the returned table list                                   */
    /* -------------------------------------------------------------------- */
        for( int iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
        {
            const char *pszTable = PQgetvalue(hResult, iRecord, 0);
            const char *pszSchemaName = PQgetvalue(hResult, iRecord, 1);
            const char *pszGeomColumnName = PQgetvalue(hResult, iRecord, 3);
            const char *pszGeomOrGeography = PQgetvalue(hResult, iRecord, 4);
            const char *pszDim = PQgetvalue(hResult, iRecord, 5);
            const char *pszSRID = PQgetvalue(hResult, iRecord, 6);
            const char *pszGeomType = PQgetvalue(hResult, iRecord, 7);
            const char *pszConstraint = PQgetvalue(hResult, iRecord, 8);
            const char *pszNotNull = PQgetvalue(hResult, iRecord, 9);
            /*const char *pszRelkind = PQgetvalue(hResult, iRecord, 2);
            CPLDebug("PG", "%s %s %s %s %s %s %s %s %s %s",
                     pszTable, pszSchemaName, pszRelkind,
                     pszGeomColumnName, pszGeomOrGeography, pszDim,
                     pszSRID, pszGeomType, pszConstraint, pszNotNull);*/

            int bNullable = EQUAL(pszNotNull, "f");

            PostgisType ePostgisType = GEOM_TYPE_UNKNOWN;
            if( EQUAL(pszGeomOrGeography, "geometry") )
                ePostgisType = GEOM_TYPE_GEOMETRY;
            else if( EQUAL(pszGeomOrGeography, "geography") )
                ePostgisType = GEOM_TYPE_GEOGRAPHY;

            int nGeomCoordDimension = atoi(pszDim);
            int nSRID = atoi(pszSRID);

            /* Analyze constraints that might override geometrytype, */
            /* coordinate dimension and SRID */
            CPLString osConstraint(pszConstraint);
            osConstraint = osConstraint.tolower();
            pszConstraint = osConstraint.c_str();
            const char* pszNeedle = strstr(pszConstraint, "geometrytype(");
            CPLString osGeometryType;
            if( pszNeedle )
            {
                pszNeedle = strchr(pszNeedle, '\'');
                if( pszNeedle )
                {
                    pszNeedle ++;
                    const char* pszEnd = strchr(pszNeedle, '\'');
                    if( pszEnd )
                    {
                        osGeometryType = pszNeedle;
                        osGeometryType.resize(pszEnd - pszNeedle);
                        pszGeomType = osGeometryType.c_str();
                    }
                }
            }

            pszNeedle = strstr(pszConstraint, "srid(");
            if( pszNeedle )
            {
                pszNeedle = strchr(pszNeedle, '=');
                if( pszNeedle )
                {
                    pszNeedle ++;
                    nSRID = atoi(pszNeedle);
                }
            }

            pszNeedle = strstr(pszConstraint, "ndims(");
            if( pszNeedle )
            {
                pszNeedle = strchr(pszNeedle, '=');
                if( pszNeedle )
                {
                    pszNeedle ++;
                    nGeomCoordDimension = atoi(pszNeedle);
                }
            }

            papsTables = (PGTableEntry**)CPLRealloc(papsTables, sizeof(PGTableEntry*) * (nTableCount + 1));
            papsTables[nTableCount] = (PGTableEntry*) CPLCalloc(1, sizeof(PGTableEntry));
            papsTables[nTableCount]->pszTableName = CPLStrdup( pszTable );
            papsTables[nTableCount]->pszSchemaName = CPLStrdup( pszSchemaName );

            OGRPGTableEntryAddGeomColumn(papsTables[nTableCount],
                                            pszGeomColumnName,
                                            pszGeomType, nGeomCoordDimension,
                                            nSRID, ePostgisType, bNullable);
            nTableCount ++;

            PGTableEntry* psEntry = OGRPGFindTableEntry(hSetTables, pszTable, pszSchemaName);
            if (psEntry == NULL)
                psEntry = OGRPGAddTableEntry(hSetTables, pszTable, pszSchemaName);
            OGRPGTableEntryAddGeomColumn(psEntry,
                                            pszGeomColumnName,
                                            pszGeomType,
                                            nGeomCoordDimension,
                                            nSRID, ePostgisType, bNullable);
        }

        OGRPGClearResult( hResult );
    }
    else if (nTableCount == 0)
    {
        CPLString osCommand;

        /* Caution : in PostGIS case, the result has 11 columns, whereas in the */
        /* non-PostGIS case it has only 3 columns */
        if ( bHavePostGIS && !bListAllTables )
        {
            osCommand.Printf(   "SELECT c.relname, n.nspname, c.relkind, g.f_geometry_column, g.type, g.coord_dimension, g.srid, %d, a.attnotnull, c.oid as oid, a.attnum as attnum FROM pg_class c, pg_namespace n, geometry_columns g, pg_attribute a "
                                "WHERE (c.relkind in (%s) AND c.relname !~ '^pg_' AND c.relnamespace=n.oid "
                                "AND c.relname::TEXT = g.f_table_name::TEXT AND n.nspname = g.f_table_schema AND a.attname = g.f_geometry_column AND a.attrelid = c.oid) ",
                                GEOM_TYPE_GEOMETRY, pszAllowedRelations);

            if (bHaveGeography)
                osCommand += CPLString().Printf(
                                    "UNION SELECT c.relname, n.nspname, c.relkind, g.f_geography_column, g.type, g.coord_dimension, g.srid, %d, a.attnotnull, c.oid as oid, a.attnum as attnum FROM pg_class c, pg_namespace n, geography_columns g, pg_attribute a "
                                    "WHERE (c.relkind in (%s) AND c.relname !~ '^pg_' AND c.relnamespace=n.oid "
                                    "AND c.relname::TEXT = g.f_table_name::TEXT AND n.nspname = g.f_table_schema AND a.attname = g.f_geography_column AND a.attrelid = c.oid)",
                                    GEOM_TYPE_GEOGRAPHY, pszAllowedRelations);
            osCommand += " ORDER BY oid, attnum";
        }
        else
            osCommand.Printf(
                                "SELECT c.relname, n.nspname, c.relkind FROM pg_class c, pg_namespace n "
                                "WHERE (c.relkind in (%s) AND c.relname !~ '^pg_' AND c.relnamespace=n.oid)",
                                pszAllowedRelations);
                            
        hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

        if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
        {
            OGRPGClearResult( hResult );

            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s", PQerrorMessage(hPGConn) );
            goto end;
        }

    /* -------------------------------------------------------------------- */
    /*      Parse the returned table list                                   */
    /* -------------------------------------------------------------------- */
        for( int iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
        {
            const char *pszTable = PQgetvalue(hResult, iRecord, 0);
            const char *pszSchemaName = PQgetvalue(hResult, iRecord, 1);
            const char *pszRelkind = PQgetvalue(hResult, iRecord, 2);
            const char *pszGeomColumnName = NULL;
            const char *pszGeomType = NULL;
            int nGeomCoordDimension = 0;
            int nSRID = 0;
            int bNullable = TRUE;
            PostgisType ePostgisType = GEOM_TYPE_UNKNOWN;
            if (bHavePostGIS && !bListAllTables)
            {
                pszGeomColumnName = PQgetvalue(hResult, iRecord, 3);
                pszGeomType = PQgetvalue(hResult, iRecord, 4);
                nGeomCoordDimension = atoi(PQgetvalue(hResult, iRecord, 5));
                nSRID = atoi(PQgetvalue(hResult, iRecord, 6));
                ePostgisType = (PostgisType) atoi(PQgetvalue(hResult, iRecord, 7));
                bNullable = EQUAL(PQgetvalue(hResult, iRecord, 8), "f");

                /* We cannot reliably find geometry columns of a view that is */
                /* based on a table that inherits from another one, wit that */
                /* method, so give up, and let OGRPGTableLayer::ReadTableDefinition() */
                /* do the job */
                if( pszRelkind[0] == 'v' && sPostGISVersion.nMajor < 2 )
                    pszGeomColumnName = NULL;
            }

            if( EQUAL(pszTable,"spatial_ref_sys")
                || EQUAL(pszTable,"geometry_columns")
                || EQUAL(pszTable,"geography_columns") )
                continue;

            if( EQUAL(pszSchemaName,"information_schema") )
                continue;

            papsTables = (PGTableEntry**)CPLRealloc(papsTables, sizeof(PGTableEntry*) * (nTableCount + 1));
            papsTables[nTableCount] = (PGTableEntry*) CPLCalloc(1, sizeof(PGTableEntry));
            papsTables[nTableCount]->pszTableName = CPLStrdup( pszTable );
            papsTables[nTableCount]->pszSchemaName = CPLStrdup( pszSchemaName );
            if (pszGeomColumnName)
                OGRPGTableEntryAddGeomColumn(papsTables[nTableCount],
                                             pszGeomColumnName,
                                             pszGeomType, nGeomCoordDimension,
                                             nSRID, ePostgisType, bNullable);
            nTableCount ++;

            PGTableEntry* psEntry = OGRPGFindTableEntry(hSetTables, pszTable, pszSchemaName);
            if (psEntry == NULL)
                psEntry = OGRPGAddTableEntry(hSetTables, pszTable, pszSchemaName);
            if (pszGeomColumnName)
                OGRPGTableEntryAddGeomColumn(psEntry,
                                             pszGeomColumnName,
                                             pszGeomType,
                                             nGeomCoordDimension,
                                             nSRID, ePostgisType, bNullable);
        }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
        OGRPGClearResult( hResult );

        /* With PostGIS 2.0, we don't need to query base tables of inherited */
        /* tables */
        if ( bHavePostGIS && !bListAllTables && sPostGISVersion.nMajor < 2 )
        {
        /* -------------------------------------------------------------------- */
        /*      Fetch inherited tables                                          */
        /* -------------------------------------------------------------------- */
            hResult = OGRPG_PQexec(hPGConn,
                                "SELECT c1.relname AS derived, c2.relname AS parent, n.nspname "
                                "FROM pg_class c1, pg_class c2, pg_namespace n, pg_inherits i "
                                "WHERE i.inhparent = c2.oid AND i.inhrelid = c1.oid AND c1.relnamespace=n.oid "
                                "AND c1.relkind in ('r', 'v') AND c1.relnamespace=n.oid AND c2.relkind in ('r','v') "
                                "AND c2.relname !~ '^pg_' AND c2.relnamespace=n.oid");

            if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
            {
                OGRPGClearResult( hResult );

                CPLError( CE_Failure, CPLE_AppDefined,
                        "%s", PQerrorMessage(hPGConn) );
                goto end;
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

                            int iGeomColumn;
                            for(iGeomColumn = 0; iGeomColumn < psParentEntry->nGeomColumnCount; iGeomColumn++)
                            {
                                papsTables = (PGTableEntry**)CPLRealloc(papsTables, sizeof(PGTableEntry*) * (nTableCount + 1));
                                papsTables[nTableCount] = (PGTableEntry*) CPLCalloc(1, sizeof(PGTableEntry));
                                papsTables[nTableCount]->pszTableName = CPLStrdup( pszTable );
                                papsTables[nTableCount]->pszSchemaName = CPLStrdup( pszSchemaName );
                                OGRPGTableEntryAddGeomColumn(papsTables[nTableCount],
                                                             &psParentEntry->pasGeomColumns[iGeomColumn]);
                                nTableCount ++;

                                OGRPGTableEntryAddGeomColumn(psEntry,
                                                             &psParentEntry->pasGeomColumns[iGeomColumn]);
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
        }
    }

/* -------------------------------------------------------------------- */
/*      Register the available tables.                                  */
/* -------------------------------------------------------------------- */
    for( int iRecord = 0; iRecord < nTableCount; iRecord++ )
    {
        PGTableEntry* psEntry;
        CPLString osDefnName;
        psEntry = (PGTableEntry* )CPLHashSetLookup(hSetTables, papsTables[iRecord]);

        /* If SCHEMAS= is specified, only take into account tables inside */
        /* one of the specified schemas */
        if (papszSchemaList != NULL &&
            CSLFindString(papszSchemaList, papsTables[iRecord]->pszSchemaName) == -1)
        {
            continue;
        }

        if ( papsTables[iRecord]->pszSchemaName &&
            osCurrentSchema != papsTables[iRecord]->pszSchemaName )
        {
            osDefnName.Printf("%s.%s", papsTables[iRecord]->pszSchemaName,
                              papsTables[iRecord]->pszTableName );
        }
        else
        {
            //no prefix for current_schema in layer name, for backwards compatibility
            osDefnName = papsTables[iRecord]->pszTableName;
        }
        if( osRegisteredLayers.find( osDefnName ) != osRegisteredLayers.end() )
            continue;
        osRegisteredLayers.insert( osDefnName );

        OGRPGTableLayer* poLayer;
        poLayer = OpenTable( osCurrentSchema, papsTables[iRecord]->pszTableName,
            papsTables[iRecord]->pszSchemaName,
            NULL, bDSUpdate, FALSE );
        if( psEntry != NULL )
        {
            if( psEntry->nGeomColumnCount > 0 )
            {
                poLayer->SetGeometryInformation(psEntry->pasGeomColumns,
                                                psEntry->nGeomColumnCount);
            }
        }
        else
        {
            if( papsTables[iRecord]->nGeomColumnCount > 0 )
            {
                poLayer->SetGeometryInformation(papsTables[iRecord]->pasGeomColumns,
                                                papsTables[iRecord]->nGeomColumnCount);
            }
        }
    }

end:
    if (hSetTables)
        CPLHashSetDestroy(hSetTables);

    for(int i=0;i<nTableCount;i++)
        OGRPGFreeTableEntry(papsTables[i]);
    CPLFree(papsTables);
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

OGRPGTableLayer* OGRPGDataSource::OpenTable( CPLString& osCurrentSchema,
                                const char *pszNewName,
                                const char *pszSchemaName,
                                const char * pszGeomColumnForced,
                                int bUpdate,
                                int bTestOpen)

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRPGTableLayer  *poLayer;

    poLayer = new OGRPGTableLayer( this, osCurrentSchema,
                                   pszNewName, pszSchemaName,
                                   pszGeomColumnForced, bUpdate );
    if( bTestOpen && !(poLayer->ReadTableDefinition()) )
    {
        delete poLayer;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRPGTableLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRPGTableLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

int OGRPGDataSource::DeleteLayer( int iLayer )

{
    /* Force loading of all registered tables */
    GetLayerCount();
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

    if (osLayerName.size() == 0)
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */
    PGresult            *hResult;
    CPLString            osCommand;

    SoftStartTransaction();

    if( bHavePostGIS  && sPostGISVersion.nMajor < 2)
    {
        /* This is unnecessary if the layer is not a geometry table, or an inherited geometry table */
        /* but it shouldn't hurt */
        osCommand.Printf(
                 "DELETE FROM geometry_columns WHERE f_table_name='%s' and f_table_schema='%s'",
                 osTableName.c_str(), osSchemaName.c_str() );

        hResult = OGRPG_PQexec( hPGConn, osCommand.c_str() );
        OGRPGClearResult( hResult );
    }

    osCommand.Printf("DROP TABLE %s.%s CASCADE",
                     OGRPGEscapeColumnName(osSchemaName).c_str(),
                     OGRPGEscapeColumnName(osTableName).c_str() );
    hResult = OGRPG_PQexec( hPGConn, osCommand.c_str() );
    OGRPGClearResult( hResult );

    SoftCommitTransaction();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRPGDataSource::ICreateLayer( const char * pszLayerName,
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

    EndCopy();

    const char* pszFIDColumnName = CSLFetchNameValue(papszOptions, "FID");
    CPLString osFIDColumnName;
    if (pszFIDColumnName == NULL)
        osFIDColumnName = "ogc_fid";
    else
    {
        if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
        {
            char* pszLaunderedFid = OGRPGCommonLaunderName(pszFIDColumnName, "PG");
            osFIDColumnName += OGRPGEscapeColumnName(pszLaunderedFid);
            CPLFree(pszLaunderedFid);
        }
        else
            osFIDColumnName += OGRPGEscapeColumnName(pszFIDColumnName);
    }
    pszFIDColumnName = osFIDColumnName.c_str();

    if (strncmp(pszLayerName, "pg", 2) == 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The layer name should not begin by 'pg' as it is a reserved prefix");
    }

    if( wkbFlatten(eType) == eType )
        nDimension = 2;

    int nForcedDimension = -1;
    if( CSLFetchNameValue( papszOptions, "DIM") != NULL )
    {
        nDimension = atoi(CSLFetchNameValue( papszOptions, "DIM"));
        nForcedDimension = nDimension;
    }

    /* Should we turn layers with None geometry type as Unknown/GEOMETRY */
    /* so they are still recorded in geometry_columns table ? (#4012) */
    int bNoneAsUnknown = CSLTestBoolean(CSLFetchNameValueDef(
                                    papszOptions, "NONE_AS_UNKNOWN", "NO"));
    if (bNoneAsUnknown && eType == wkbNone)
        eType = wkbUnknown;


    int bExtractSchemaFromLayerName = CSLTestBoolean(CSLFetchNameValueDef(
                                    papszOptions, "EXTRACT_SCHEMA_FROM_LAYER_NAME", "YES"));

    /* Postgres Schema handling:
       Extract schema name from input layer name or passed with -lco SCHEMA.
       Set layer name to "schema.table" or to "table" if schema == current_schema()
       Usage without schema name is backwards compatible
    */
    const char* pszDotPos = strstr(pszLayerName,".");
    if ( pszDotPos != NULL && bExtractSchemaFromLayerName )
    {
      int length = pszDotPos - pszLayerName;
      pszSchemaName = (char*)CPLMalloc(length+1);
      strncpy(pszSchemaName, pszLayerName, length);
      pszSchemaName[length] = '\0';
      
      if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
          pszTableName = OGRPGCommonLaunderName( pszDotPos + 1, "PG" ); //skip "."
      else
          pszTableName = CPLStrdup( pszDotPos + 1 ); //skip "."
    }
    else
    {
      pszSchemaName = NULL;
      if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
          pszTableName = OGRPGCommonLaunderName( pszLayerName, "PG" ); //skip "."
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

    if ( pszSchemaName == NULL )
    {
        pszSchemaName = CPLStrdup(osCurrentSchema);
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    int iLayer;

    CPLString osSQLLayerName;
    if (pszSchemaName == NULL || (strlen(osCurrentSchema) > 0 && EQUAL(pszSchemaName, osCurrentSchema.c_str())))
        osSQLLayerName = pszTableName;
    else
    {
        osSQLLayerName = pszSchemaName;
        osSQLLayerName += ".";
        osSQLLayerName += pszTableName;
    }

    /* GetLayerByName() can instanciate layers that would have been */
    /* 'hidden' otherwise, for example, non-spatial tables in a */
    /* Postgis-enabled database, so this apparently useless command is */
    /* not useless... (#4012) */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GetLayerByName(osSQLLayerName);
    CPLPopErrorHandler();
    CPLErrorReset();

    /* Force loading of all registered tables */
    GetLayerCount();
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(osSQLLayerName.c_str(),papoLayers[iLayer]->GetName()) )
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

    const char *pszGFldName = NULL;
    if( eType != wkbNone && EQUAL(pszGeomType, "geography") )
    {
        if( !bHaveGeography )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "GEOM_TYPE=geography is only supported in PostGIS >= 1.5.\n"
                    "Creation of layer %s has failed.",
                    pszLayerName );
            CPLFree( pszTableName );
            CPLFree( pszSchemaName );
            return NULL;
        }

        if( CSLFetchNameValue( papszOptions, "GEOMETRY_NAME") != NULL )
            pszGFldName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME");
        else
            pszGFldName = "the_geog";
    }
    else if ( eType != wkbNone && bHavePostGIS && !EQUAL(pszGeomType, "geography") )
    {
        if( CSLFetchNameValue( papszOptions, "GEOMETRY_NAME") != NULL )
            pszGFldName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME");
        else
            pszGFldName = "wkb_geometry";
    }

    if( eType != wkbNone && bHavePostGIS && !EQUAL(pszGeomType,"geometry") &&
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
    int nSRSId = nUndefinedSRID;

    if( poSRS != NULL )
        nSRSId = FetchSRSId( poSRS );
        
    const char *pszGeometryType = OGRToOGCGeomType(eType);

    int bDifferedCreation = CSLTestBoolean(CPLGetConfigOption( "OGR_PG_DIFFERED_CREATION", "YES" ));
    if( !bHavePostGIS )
        bDifferedCreation = FALSE;  /* to avoid unnecessary implementation and testing burden */
    
/* -------------------------------------------------------------------- */
/*      Create a basic table with the FID.  Also include the            */
/*      geometry if this is not a PostGIS enabled table.                */
/* -------------------------------------------------------------------- */
    int bFID64 = CSLFetchBoolean(papszOptions, "FID64", FALSE);
    const char* pszSerialType = bFID64 ? "BIGSERIAL": "SERIAL";

    CPLString osCreateTable;
    int bTemporary = CSLFetchBoolean( papszOptions, "TEMPORARY", FALSE );
    if (bTemporary)
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup("pg_temp_1");
        osCreateTable.Printf("CREATE TEMPORARY TABLE %s",
                             OGRPGEscapeColumnName(pszTableName).c_str());
    }
    else
        osCreateTable.Printf("CREATE%s TABLE %s.%s",
                             CSLFetchBoolean( papszOptions, "UNLOGGED", FALSE ) ? " UNLOGGED": "",
                             OGRPGEscapeColumnName(pszSchemaName).c_str(), 
                             OGRPGEscapeColumnName(pszTableName).c_str());
    
    if( eType != wkbNone && !bHavePostGIS )
    {
        pszGFldName = "wkb_geometry";
        osCommand.Printf(
                 "%s ( "
                 "    %s %s, "
                 "   %s %s, "
                 "   PRIMARY KEY (%s)",
                 osCreateTable.c_str(),
                 pszFIDColumnName,
                 pszSerialType,
                 pszGFldName,
                 pszGeomType,
                 pszFIDColumnName);
    }
    else if ( !bDifferedCreation && eType != wkbNone && EQUAL(pszGeomType, "geography") )
    {
        osCommand.Printf(
                    "%s ( %s %s, %s geography(%s%s%s), PRIMARY KEY (%s)",
                    osCreateTable.c_str(),
                    pszFIDColumnName,
                    pszSerialType,
                    OGRPGEscapeColumnName(pszGFldName).c_str(), pszGeometryType,
                    nDimension == 2 ? "" : "Z",
                    nSRSId ? CPLSPrintf(",%d", nSRSId) : "", 
                    pszFIDColumnName);
    }
    else if ( !bDifferedCreation && eType != wkbNone && !EQUAL(pszGeomType, "geography") &&
              sPostGISVersion.nMajor >= 2 )
    {
        osCommand.Printf(
                    "%s ( %s %s, %s geometry(%s%s%s), PRIMARY KEY (%s)",
                    osCreateTable.c_str(),
                    pszFIDColumnName,
                    pszSerialType,
                    OGRPGEscapeColumnName(pszGFldName).c_str(), pszGeometryType,
                    nDimension == 2 ? "" : "Z",
                    nSRSId ? CPLSPrintf(",%d", nSRSId) : "", 
                    pszFIDColumnName);
    }
    else
    {
        osCommand.Printf(
                 "%s ( %s %s, PRIMARY KEY (%s)",
                 osCreateTable.c_str(),
                 pszFIDColumnName,
                 pszSerialType,
                 pszFIDColumnName );
    }
    osCreateTable = osCommand;

    const char *pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
    int bCreateSpatialIndex = ( pszSI == NULL || CSLTestBoolean(pszSI) );
    if( eType != wkbNone &&
        pszSI == NULL &&
        CSLFetchBoolean( papszOptions, "UNLOGGED", FALSE ) &&
        !(sPostgreSQLVersion.nMajor > 9 ||
         (sPostgreSQLVersion.nMajor == 9 && sPostgreSQLVersion.nMinor >= 3)) )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "GiST index only supported since Postgres 9.3 on unlogged table");
        bCreateSpatialIndex = FALSE;
    }

    CPLString osEscapedTableNameSingleQuote = OGRPGEscapeString(hPGConn, pszTableName);
    const char* pszEscapedTableNameSingleQuote = osEscapedTableNameSingleQuote.c_str();
    CPLString osEscapedSchemaNameSingleQuote = OGRPGEscapeString(hPGConn, pszSchemaName);
    const char* pszEscapedSchemaNameSingleQuote = osEscapedSchemaNameSingleQuote.c_str();

    if( eType != wkbNone && bHavePostGIS && sPostGISVersion.nMajor <= 1 )
    {
        /* Sometimes there is an old cruft entry in the geometry_columns
        * table if things were not properly cleaned up before.  We make
        * an effort to clean out such cruft.
        * Note: PostGIS 2.0 defines geometry_columns as a view (no clean up is needed)
        */
        osCommand.Printf(
                "DELETE FROM geometry_columns WHERE f_table_name = %s AND f_table_schema = %s",
                pszEscapedTableNameSingleQuote, pszEscapedSchemaNameSingleQuote );

        hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
        OGRPGClearResult( hResult );
    }
    
    if( !bDifferedCreation )
    {
        SoftStartTransaction();

        osCommand = osCreateTable;
        osCommand += " )";

        hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
        if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s\n%s", osCommand.c_str(), PQerrorMessage(hPGConn) );
            CPLFree( pszTableName );
            CPLFree( pszSchemaName );

            OGRPGClearResult( hResult );

            SoftRollbackTransaction();
            return NULL;
        }

        OGRPGClearResult( hResult );

    /* -------------------------------------------------------------------- */
    /*      Eventually we should be adding this table to a table of         */
    /*      "geometric layers", capturing the WKT projection, and           */
    /*      perhaps some other housekeeping.                                */
    /* -------------------------------------------------------------------- */
        if( eType != wkbNone && bHavePostGIS && !EQUAL(pszGeomType, "geography") &&
            sPostGISVersion.nMajor <= 1 )
        {
            osCommand.Printf(
                    "SELECT AddGeometryColumn(%s,%s,%s,%d,'%s',%d)",
                    pszEscapedSchemaNameSingleQuote, pszEscapedTableNameSingleQuote,
                    OGRPGEscapeString(hPGConn, pszGFldName).c_str(),
                    nSRSId, pszGeometryType, nDimension );

            hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

            if( !hResult
                || PQresultStatus(hResult) != PGRES_TUPLES_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "AddGeometryColumn failed for layer %s, layer creation has failed.",
                        pszLayerName );

                CPLFree( pszTableName );
                CPLFree( pszSchemaName );

                OGRPGClearResult( hResult );

                SoftRollbackTransaction();

                return NULL;
            }

            OGRPGClearResult( hResult );
        }
        
        if( eType != wkbNone && bHavePostGIS && bCreateSpatialIndex )
        {
    /* -------------------------------------------------------------------- */
    /*      Create the spatial index.                                       */
    /*                                                                      */
    /*      We're doing this before we add geometry and record to the table */
    /*      so this may not be exactly the best way to do it.               */
    /* -------------------------------------------------------------------- */

            osCommand.Printf("CREATE INDEX %s ON %s.%s USING GIST (%s)",
                            OGRPGEscapeColumnName(
                                CPLSPrintf("%s_%s_geom_idx", pszTableName, pszGFldName)).c_str(),
                            OGRPGEscapeColumnName(pszSchemaName).c_str(),
                            OGRPGEscapeColumnName(pszTableName).c_str(),
                            OGRPGEscapeColumnName(pszGFldName).c_str());

            hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

            if( !hResult
                || PQresultStatus(hResult) != PGRES_COMMAND_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "'%s' failed for layer %s, index creation has failed.",
                        osCommand.c_str(), pszLayerName );

                CPLFree( pszTableName );
                CPLFree( pszSchemaName );

                OGRPGClearResult( hResult );

                SoftRollbackTransaction();

                return NULL;
            }
            OGRPGClearResult( hResult );
        }

    /* -------------------------------------------------------------------- */
    /*      Complete, and commit the transaction.                           */
    /* -------------------------------------------------------------------- */
        SoftCommitTransaction();
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRPGTableLayer     *poLayer;

    poLayer = new OGRPGTableLayer( this, osCurrentSchema, pszTableName,
                                   pszSchemaName, NULL, TRUE );
    poLayer->SetTableDefinition(pszFIDColumnName, pszGFldName, eType,
                                pszGeomType, nSRSId, nDimension);
    poLayer->SetLaunderFlag( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) );
    poLayer->SetPrecisionFlag( CSLFetchBoolean(papszOptions,"PRECISION",TRUE));
    //poLayer->SetForcedSRSId(nForcedSRSId);
    poLayer->SetForcedDimension(nForcedDimension);
    poLayer->SetCreateSpatialIndexFlag(bCreateSpatialIndex);
    poLayer->SetDifferedCreation(bDifferedCreation, osCreateTable);
    

    /* HSTORE_COLUMNS existed at a time during GDAL 1.10dev */
    const char* pszHSTOREColumns = CSLFetchNameValue( papszOptions, "HSTORE_COLUMNS" );
    if( pszHSTOREColumns != NULL )
        CPLError(CE_Warning, CPLE_AppDefined, "HSTORE_COLUMNS not recognized. Use COLUMN_TYPES instead.");

    const char* pszOverrideColumnTypes = CSLFetchNameValue( papszOptions, "COLUMN_TYPES" );
    poLayer->SetOverrideColumnTypes(pszOverrideColumnTypes);

    poLayer->AllowAutoFIDOnCreateViaCopy();
    if( CSLTestBoolean(CPLGetConfigOption("PG_USE_COPY", "YES")) )
        poLayer->SetUseCopy();

    if( bFID64 )
        poLayer->SetMetadataItem(OLMD_FID64, "YES");

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
        || EQUAL(pszCap,ODsCDeleteLayer)
        || EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCTransactions) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRPGDataSource::GetLayerCount()
{
    LoadTables();
    return nLayers;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPGDataSource::GetLayer( int iLayer )

{
    /* Force loading of all registered tables */
    if( iLayer < 0 || iLayer >= GetLayerCount() )
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
    
    /* first a case sensitive check */
    /* do NOT force loading of all registered tables */
    for( i = 0; i < nLayers; i++ )
    {
        OGRPGTableLayer *poLayer = papoLayers[i];

        if( strcmp( pszName, poLayer->GetName() ) == 0 )
        {
            return poLayer;
        }
    }
        
    /* then case insensitive */
    for( i = 0; i < nLayers; i++ )
    {
        OGRPGTableLayer *poLayer = papoLayers[i];

        if( EQUAL( pszName, poLayer->GetName() ) )
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

    OGRPGTableLayer* poLayer = NULL;

    if (pszSchemaName != NULL && osCurrentSchema == pszSchemaName &&
        pszGeomColumnName == NULL )
    {
        poLayer = (OGRPGTableLayer*) GetLayerByName(pszTableName);
    }
    else
    {
        EndCopy();

        CPLString osTableName(pszTableName);
        CPLString osTableNameLower(pszTableName);
        osTableNameLower.tolower();
        if( osTableName != osTableNameLower )
            CPLPushErrorHandler(CPLQuietErrorHandler);
        poLayer = OpenTable( osCurrentSchema, pszTableName,
                             pszSchemaName,
                             pszGeomColumnName,
                             bDSUpdate, TRUE );
        if( osTableName != osTableNameLower )
            CPLPopErrorHandler();
        if( poLayer == NULL && osTableName != osTableNameLower )
        {
            poLayer = OpenTable( osCurrentSchema, osTableNameLower,
                                pszSchemaName,
                                pszGeomColumnName,
                                bDSUpdate, TRUE );
        }
    }

    CPLFree(pszTableName);
    CPLFree(pszSchemaName);
    CPLFree(pszGeomColumnName);

    return poLayer;
}


/************************************************************************/
/*                        OGRPGNoticeProcessor()                        */
/************************************************************************/

static void OGRPGNoticeProcessor( CPL_UNUSED void *arg, const char * pszMessage )
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

    EndCopy();

/* -------------------------------------------------------------------- */
/*      Try looking up in spatial_ref_sys table.                        */
/* -------------------------------------------------------------------- */
    PGresult        *hResult = NULL;
    CPLString        osCommand;
    OGRSpatialReference *poSRS = NULL;

    osCommand.Printf(
             "SELECT srtext FROM spatial_ref_sys "
             "WHERE srid = %d",
             nId );
    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

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
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Could not fetch SRS: %s", PQerrorMessage( hPGConn ) );
    }

    OGRPGClearResult( hResult );

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
    int                 nSRSId = nUndefinedSRID;
    const char*         pszAuthorityName;

    if( poSRS == NULL )
        return nUndefinedSRID;

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
/*      Check whether the authority name/code is already mapped to a    */
/*      SRS ID.                                                         */
/* -------------------------------------------------------------------- */
    int nAuthorityCode = 0;
    if( pszAuthorityName != NULL )
    {
        /* Check that the authority code is integral */
        nAuthorityCode = atoi( oSRS.GetAuthorityCode(NULL) );
        if( nAuthorityCode > 0 )
        {
            osCommand.Printf("SELECT srid FROM spatial_ref_sys WHERE "
                            "auth_name = '%s' AND auth_srid = %d",
                            pszAuthorityName,
                            nAuthorityCode );
            hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

            if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
                && PQntuples(hResult) > 0 )
            {
                nSRSId = atoi(PQgetvalue( hResult, 0, 0 ));

                OGRPGClearResult( hResult );

                return nSRSId;
            }

            OGRPGClearResult( hResult );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate SRS to WKT.                                           */
/* -------------------------------------------------------------------- */
    if( oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        return nUndefinedSRID;
    }

/* -------------------------------------------------------------------- */
/*      Try to find in the existing table.                              */
/* -------------------------------------------------------------------- */
    CPLString osWKT = OGRPGEscapeString(hPGConn, pszWKT, -1, "spatial_ref_sys", "srtext");
    osCommand.Printf(
             "SELECT srid FROM spatial_ref_sys WHERE srtext = %s",
             osWKT.c_str() );
    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );
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

    if( bTableMissing )
    {
        if( InitializeMetadataTables() != OGRERR_NONE )
            return nUndefinedSRID;
    }

/* -------------------------------------------------------------------- */
/*      Get the current maximum srid in the srs table.                  */
/* -------------------------------------------------------------------- */
    hResult = OGRPG_PQexec(hPGConn, "SELECT MAX(srid) FROM spatial_ref_sys" );

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
        return nUndefinedSRID;
    }

    CPLString osProj4 = OGRPGEscapeString(hPGConn, pszProj4, -1, "spatial_ref_sys", "proj4text");

    if( pszAuthorityName != NULL && nAuthorityCode > 0)
    {
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

    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );
    OGRPGClearResult( hResult );

    return nSRSId;
}

/************************************************************************/
/*                         StartTransaction()                           */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRPGDataSource::StartTransaction(CPL_UNUSED int bForce)
{
    if( bUserTransactionActive )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Transaction already established");
        return OGRERR_FAILURE;
    }

    CPLAssert(!bSavePointActive);
    EndCopy();

    if( nSoftTransactionLevel == 0 )
    {
        OGRErr eErr = DoTransactionCommand("BEGIN");
        if( eErr != OGRERR_NONE )
            return eErr;
    }
    else
    {
        OGRErr eErr = DoTransactionCommand("SAVEPOINT ogr_savepoint");
        if( eErr != OGRERR_NONE )
            return eErr;

        bSavePointActive = TRUE;
    }

    nSoftTransactionLevel++;
    bUserTransactionActive = TRUE;

    /*CPLDebug("PG", "poDS=%p StartTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRPGDataSource::CommitTransaction()
{
    if( !bUserTransactionActive )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Transaction not established");
        return OGRERR_FAILURE;
    }

    /*CPLDebug("PG", "poDS=%p CommitTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    FlushCache();

    nSoftTransactionLevel--;
    bUserTransactionActive = FALSE;

    OGRErr eErr;
    if( bSavePointActive )
    {
        CPLAssert(nSoftTransactionLevel > 0);
        bSavePointActive = FALSE;
        
        eErr = DoTransactionCommand("RELEASE SAVEPOINT ogr_savepoint");
    }
    else
    {
        if( nSoftTransactionLevel > 0 )
        {
            // This means we have cursors still in progress
            for(int i=0;i<nLayers;i++)
                papoLayers[i]->InvalidateCursor();
            CPLAssert( nSoftTransactionLevel == 0 );
        }

        eErr = DoTransactionCommand("COMMIT");
    }

    return eErr;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRPGDataSource::RollbackTransaction()
{
    if( !bUserTransactionActive )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Transaction not established");
        return OGRERR_FAILURE;
    }

    /*CPLDebug("PG", "poDS=%p RollbackTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    FlushCache();

    nSoftTransactionLevel--;
    bUserTransactionActive = FALSE;

    OGRErr eErr;
    if( bSavePointActive )
    {
        CPLAssert(nSoftTransactionLevel > 0);
        bSavePointActive = FALSE;

        eErr = DoTransactionCommand("ROLLBACK TO SAVEPOINT ogr_savepoint");
    }
    else
    {
        if( nSoftTransactionLevel > 0 )
        {
            // This means we have cursors still in progress
            for(int i=0;i<nLayers;i++)
                papoLayers[i]->InvalidateCursor();
            CPLAssert( nSoftTransactionLevel == 0 );
        }

        eErr = DoTransactionCommand("ROLLBACK");
    }

    return eErr;
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
    /*CPLDebug("PG", "poDS=%p SoftStartTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    OGRErr eErr = OGRERR_NONE;
    if( nSoftTransactionLevel == 1 )
    {
        eErr = DoTransactionCommand("BEGIN");
    }

    return eErr;
}

/************************************************************************/
/*                     SoftCommitTransaction()                          */
/*                                                                      */
/*      Commit the current transaction if we are at the outer           */
/*      scope.                                                          */
/************************************************************************/

OGRErr OGRPGDataSource::SoftCommitTransaction()

{
    EndCopy();
    
    /*CPLDebug("PG", "poDS=%p SoftCommitTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    if( nSoftTransactionLevel <= 0 )
    {
        CPLAssert(FALSE);
        return OGRERR_FAILURE;
    }

    OGRErr eErr = OGRERR_NONE;
    nSoftTransactionLevel--;
    if( nSoftTransactionLevel == 0 )
    {
        CPLAssert( !bSavePointActive );

        eErr = DoTransactionCommand("COMMIT");
    }

    return eErr;
}

/************************************************************************/
/*                  SoftRollbackTransaction()                           */
/*                                                                      */
/*      Do a rollback of the current transaction if we are at the 1st   */
/*      level                                                           */
/************************************************************************/

OGRErr OGRPGDataSource::SoftRollbackTransaction()

{
    EndCopy();
    
    /*CPLDebug("PG", "poDS=%p SoftRollbackTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    if( nSoftTransactionLevel <= 0 )
    {
        CPLAssert(FALSE);
        return OGRERR_FAILURE;
    }

    OGRErr eErr = OGRERR_NONE;
    nSoftTransactionLevel--;
    if( nSoftTransactionLevel == 0 )
    {
        CPLAssert( !bSavePointActive );

        eErr = DoTransactionCommand("ROLLBACK");
    }

    return eErr;
}

/************************************************************************/
/*                        FlushSoftTransaction()                        */
/*                                                                      */
/*      Force the unwinding of any active transaction, and its          */
/*      commit. Should only be used by datasource destructor            */
/************************************************************************/

OGRErr OGRPGDataSource::FlushSoftTransaction()

{
    FlushCache(); 
    
    /*CPLDebug("PG", "poDS=%p FlushSoftTransaction() nSoftTransactionLevel=%d",
             this, nSoftTransactionLevel);*/

    if( nSoftTransactionLevel <= 0 )
        return OGRERR_NONE;

    for(int i=0;i<nLayers;i++)
        papoLayers[i]->InvalidateCursor();
    bSavePointActive = FALSE;

    OGRErr eErr = OGRERR_NONE;
    if( nSoftTransactionLevel > 0 )
    {
        CPLAssert(nSoftTransactionLevel == 1 );
        nSoftTransactionLevel = 0;
        eErr = DoTransactionCommand("COMMIT");
    }
    return eErr;
}

/************************************************************************/
/*                          DoTransactionCommand()                      */
/************************************************************************/

OGRErr OGRPGDataSource::DoTransactionCommand(const char* pszCommand)

{
    OGRErr      eErr = OGRERR_NONE;
    PGresult    *hResult = NULL;
    PGconn      *hPGConn = GetPGConn();

    hResult = OGRPG_PQexec(hPGConn, pszCommand);
    osDebugLastTransactionCommand = pszCommand;

    if( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        eErr = OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    return eErr;
}

/************************************************************************/
/*                     OGRPGNoResetResultLayer                          */
/************************************************************************/

class OGRPGNoResetResultLayer : public OGRPGLayer
{
  public:
                        OGRPGNoResetResultLayer(OGRPGDataSource *poDSIn,
                                                PGresult *hResultIn);

    virtual             ~OGRPGNoResetResultLayer();

    virtual void        ResetReading();

    virtual int         TestCapability( const char * ) { return FALSE; }

    virtual OGRFeature *GetNextFeature();
    
    virtual CPLString   GetFromClauseForGetExtent() { CPLAssert(FALSE); return ""; }
    virtual void        ResolveSRID(OGRPGGeomFieldDefn* poGFldDefn) { poGFldDefn->nSRSId = -1; }
};


/************************************************************************/
/*                     OGRPGNoResetResultLayer()                        */
/************************************************************************/

OGRPGNoResetResultLayer::OGRPGNoResetResultLayer( OGRPGDataSource *poDSIn,
                                                  PGresult *hResultIn )
{
    poDS = poDSIn;
    ReadResultDefinition(hResultIn);
    hCursorResult = hResultIn;
    CreateMapFromFieldNameToIndex(hCursorResult,
                                  poFeatureDefn,
                                  m_panMapFieldNameToIndex,
                                  m_panMapFieldNameToGeomIndex);
}

/************************************************************************/
/*                   ~OGRPGNoResetResultLayer()                         */
/************************************************************************/

OGRPGNoResetResultLayer::~OGRPGNoResetResultLayer()

{
    OGRPGClearResult( hCursorResult );
    hCursorResult = NULL;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGNoResetResultLayer::ResetReading()
{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGNoResetResultLayer::GetNextFeature()

{
    if (iNextShapeId == PQntuples(hCursorResult))
    {
        return NULL;
    }
    return RecordToFeature(hCursorResult,
                           m_panMapFieldNameToIndex,
                           m_panMapFieldNameToGeomIndex,
                           iNextShapeId ++);
}

/************************************************************************/
/*                      OGRPGMemLayerWrapper                            */
/************************************************************************/

class OGRPGMemLayerWrapper : public OGRLayer
{
  private:
      GDALDataset  *poMemDS;
      OGRLayer       *poMemLayer;

  public:
                        OGRPGMemLayerWrapper( GDALDataset  *poMemDSIn )
                        {
                            poMemDS = poMemDSIn;
                            poMemLayer = poMemDS->GetLayer(0);
                        }

                        ~OGRPGMemLayerWrapper() { delete poMemDS; }

    virtual void        ResetReading() { poMemLayer->ResetReading(); }
    virtual OGRFeature *GetNextFeature() { return poMemLayer->GetNextFeature(); }
    virtual OGRFeatureDefn *GetLayerDefn() { return poMemLayer->GetLayerDefn(); }
    virtual int         TestCapability( const char * ) { return FALSE; }
};

/************************************************************************/
/*                           GetMetadataItem()                          */
/************************************************************************/

const char* OGRPGDataSource::GetMetadataItem(const char* pszKey,
                                             const char* pszDomain)
{
    /* Only used by ogr_pg.py to check inner working */
    if( pszDomain != NULL && EQUAL(pszDomain, "_debug_") &&
        pszKey != NULL )
    {
        if( EQUAL(pszKey, "bHasLoadTables") )
            return CPLSPrintf("%d", bHasLoadTables);
        if( EQUAL(pszKey, "nSoftTransactionLevel") )
            return CPLSPrintf("%d", nSoftTransactionLevel);
        if( EQUAL(pszKey, "bSavePointActive") )
            return CPLSPrintf("%d", bSavePointActive);
        if( EQUAL(pszKey, "bUserTransactionActive") )
            return CPLSPrintf("%d", bUserTransactionActive);
        if( EQUAL(pszKey, "osDebugLastTransactionCommand") )
        {
            const char* pszRet = CPLSPrintf("%s", osDebugLastTransactionCommand.c_str());
            osDebugLastTransactionCommand = "";
            return pszRet;
        }
            
    }
    return OGRDataSource::GetMetadataItem(pszKey, pszDomain);
}


/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRPGDataSource::ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect )

{
    /* Skip leading spaces */
    while(*pszSQLCommand == ' ')
        pszSQLCommand ++;

    FlushCache();

/* -------------------------------------------------------------------- */
/*      Use generic implementation for recognized dialects              */
/* -------------------------------------------------------------------- */
    if( IsGenericSQLDialect(pszDialect) )
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
        
        GetLayerCount();
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( EQUAL(papoLayers[iLayer]->GetName(), 
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

    if (EQUALN(pszSQLCommand, "SELECT", 6) == FALSE ||
        (strstr(pszSQLCommand, "from") == NULL && strstr(pszSQLCommand, "FROM") == NULL))
    {
        /* For something that is not a select or a select without table, do not */
        /* run under transaction (CREATE DATABASE, VACCUUM don't like transactions) */

        hResult = OGRPG_PQexec(hPGConn, pszSQLCommand, TRUE /* multiple allowed */ );
        if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK)
        {
            CPLDebug( "PG", "Command Results Tuples = %d", PQntuples(hResult) );

            GDALDriver* poMemDriver = OGRSFDriverRegistrar::GetRegistrar()->
                            GetDriverByName("Memory");
            if (poMemDriver)
            {
                OGRPGLayer* poResultLayer = new OGRPGNoResetResultLayer( this, hResult );
                GDALDataset* poMemDS = poMemDriver->Create("", 0, 0, 0, GDT_Unknown, NULL);
                poMemDS->CopyLayer(poResultLayer, "sql_statement");
                OGRPGMemLayerWrapper* poResLayer = new OGRPGMemLayerWrapper(poMemDS);
                delete poResultLayer;
                return poResLayer;
            }
            else
                return NULL;
        }
    }
    else
    {
        SoftStartTransaction();

        CPLString osCommand;
        osCommand.Printf( "DECLARE %s CURSOR for %s",
                            "executeSQLCursor", pszSQLCommand );

        hResult = OGRPG_PQexec(hPGConn, osCommand );

/* -------------------------------------------------------------------- */
/*      Do we have a tuple result? If so, instantiate a results         */
/*      layer for it.                                                   */
/* -------------------------------------------------------------------- */
        if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
        {
            OGRPGResultLayer *poLayer = NULL;

            OGRPGClearResult( hResult );

            osCommand.Printf( "FETCH 0 in %s", "executeSQLCursor" );
            hResult = OGRPG_PQexec(hPGConn, osCommand );

            poLayer = new OGRPGResultLayer( this, pszSQLCommand, hResult );

            OGRPGClearResult( hResult );
            
            osCommand.Printf( "CLOSE %s", "executeSQLCursor" );
            hResult = OGRPG_PQexec(hPGConn, osCommand );
            
            SoftCommitTransaction();

            if( poSpatialFilter != NULL )
                poLayer->SetSpatialFilter( poSpatialFilter );

            return poLayer;
        }
        else
        {
            SoftRollbackTransaction();
        }
    }

/* -------------------------------------------------------------------- */
/*      Generate an error report if an error occured.                   */
/* -------------------------------------------------------------------- */
    if( !hResult ||
        (PQresultStatus(hResult) == PGRES_NONFATAL_ERROR
         || PQresultStatus(hResult) == PGRES_FATAL_ERROR ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQerrorMessage( hPGConn ) );
    }

    OGRPGClearResult( hResult );

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
/*                             StartCopy()                              */
/************************************************************************/

void OGRPGDataSource::StartCopy( OGRPGTableLayer *poPGLayer )
{
    if( poLayerInCopyMode == poPGLayer )
        return;
    EndCopy();
    poLayerInCopyMode = poPGLayer;
    poLayerInCopyMode->StartCopy();
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

