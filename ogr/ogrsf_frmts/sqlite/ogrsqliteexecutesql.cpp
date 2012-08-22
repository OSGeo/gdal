/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Run SQL requests with SQLite SQL engine
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_sqlite.h"
#include "ogr_api.h"
#include "ogrsqlitevirtualogr.h"
#include "ogrsqliteexecutesql.h"
#include "cpl_multiproc.h"

#ifdef HAVE_SQLITE_VFS

/************************************************************************/
/*                       OGRSQLiteExecuteSQLLayer                       */
/************************************************************************/

class OGRSQLiteExecuteSQLLayer: public OGRSQLiteSelectLayer
{
        OGR2SQLITEModule *poModule;
        char             *pszTmpDBName;

    public:
        OGRSQLiteExecuteSQLLayer(OGR2SQLITEModule* poModule,
                                 char* pszTmpDBName,
                                 OGRSQLiteDataSource* poDS,
                                 CPLString osSQL,
                                 sqlite3_stmt * hStmt,
                                 int bUseStatementForGetNextFeature,
                                 int bEmptyLayer );
        virtual ~OGRSQLiteExecuteSQLLayer();
};

/************************************************************************/
/*                         OGRSQLiteExecuteSQLLayer()                   */
/************************************************************************/

OGRSQLiteExecuteSQLLayer::OGRSQLiteExecuteSQLLayer(OGR2SQLITEModule* poModule,
                                                   char* pszTmpDBName,
                                                   OGRSQLiteDataSource* poDS,
                                                   CPLString osSQL,
                                                   sqlite3_stmt * hStmt,
                                                   int bUseStatementForGetNextFeature,
                                                   int bEmptyLayer ) :

                               OGRSQLiteSelectLayer(poDS, osSQL, hStmt,
                                                    bUseStatementForGetNextFeature,
                                                    bEmptyLayer)
{
    this->poModule = poModule;
    this->pszTmpDBName = pszTmpDBName;
}

/************************************************************************/
/*                        ~OGRSQLiteExecuteSQLLayer()                   */
/************************************************************************/

OGRSQLiteExecuteSQLLayer::~OGRSQLiteExecuteSQLLayer()
{
    /* This is a bit peculiar: we must "finalize" the OGRLayer, since */
    /* it has objects that depend on the datasource, that we are just */
    /* going to destroy afterwards. The issue here is that we destroy */
    /* our own datasource ! */
    Finalize();

    delete poDS;
    delete poModule;
    VSIUnlink(pszTmpDBName);
    CPLFree(pszTmpDBName);
}

/************************************************************************/
/*                            ExtractLayerName()                        */
/************************************************************************/

CPLString ExtractLayerName(const char **ppszSQLCommand)
{
    CPLString osRet;
    const char *pszSQLCommand = *ppszSQLCommand;
    int bInQuotes = FALSE;

    while( *pszSQLCommand == ' ' )
        pszSQLCommand ++;
    if( *pszSQLCommand == '"')
    {
        bInQuotes = TRUE;
        pszSQLCommand ++;
    }
    while( *pszSQLCommand != '\0' )
    {
        if( bInQuotes && *pszSQLCommand == '"' && pszSQLCommand[1] == '"' )
        {
            pszSQLCommand ++;
            osRet += "\"";
        }
        else if( bInQuotes && *pszSQLCommand == '"' )
        {
            pszSQLCommand ++;
            break;
        }
        else if( !bInQuotes && (*pszSQLCommand == ' ' || *pszSQLCommand == ',') )
            break;
        else
            osRet += *pszSQLCommand;

        pszSQLCommand ++;
    }

    *ppszSQLCommand = pszSQLCommand;

    return osRet;
}

/************************************************************************/
/*                     OGR2SQLITEGetPotentialLayerNames()               */
/************************************************************************/

static std::set<CPLString> OGR2SQLITEGetPotentialLayerNames(const char *pszSQLCommand)
{
    std::set<CPLString> oSet;

    while( *pszSQLCommand != '\0' )
    {
        /* Skip literals */
        if( *pszSQLCommand == '\'' )
        {
            pszSQLCommand ++;
            while( *pszSQLCommand != '\0' )
            {
                if( *pszSQLCommand == '\'' && pszSQLCommand[1] == '\'' )
                    pszSQLCommand ++;
                else if( *pszSQLCommand == '\'' )
                {
                    pszSQLCommand ++;
                    break;
                }
                pszSQLCommand ++;
            }
        }
        /* Skip strings */
        else if ( *pszSQLCommand == '"' )
        {
            pszSQLCommand ++;
            while( *pszSQLCommand != '\0' )
            {
                if( *pszSQLCommand == '"' && pszSQLCommand[1] == '"' )
                    pszSQLCommand ++;
                else if( *pszSQLCommand == '"' )
                {
                    pszSQLCommand ++;
                    break;
                }
                pszSQLCommand ++;
            }
        }
        else if( EQUALN(pszSQLCommand, "FROM ", strlen("FROM ")) )
        {
            pszSQLCommand += strlen("FROM ");
            oSet.insert(ExtractLayerName(&pszSQLCommand));
            while( *pszSQLCommand != '\0' )
            {
                if( *pszSQLCommand == ' ' )
                {
                    pszSQLCommand ++;
                    while( *pszSQLCommand == ' ' )
                        pszSQLCommand ++;
                    if( *pszSQLCommand != '\0' &&
                        *pszSQLCommand != ',' &&
                        !EQUALN(pszSQLCommand, "JOIN", 4) )
                        ExtractLayerName(&pszSQLCommand);
                }
                else if (*pszSQLCommand == ',' )
                {
                    pszSQLCommand ++;
                    while( *pszSQLCommand == ' ' )
                        pszSQLCommand ++;
                    oSet.insert(ExtractLayerName(&pszSQLCommand));
                }
                else
                    break;
            }
        }
        else if ( EQUALN(pszSQLCommand, "JOIN ", strlen("JOIN ")) )
        {
            pszSQLCommand += strlen("JOIN ");
            oSet.insert(ExtractLayerName(&pszSQLCommand));
        }
        else if( EQUALN(pszSQLCommand, "INSERT INTO ", strlen("INSERT INTO ")) )
        {
            pszSQLCommand += strlen("INSERT INTO ");
            oSet.insert(ExtractLayerName(&pszSQLCommand));
        }
        else if( EQUALN(pszSQLCommand, "UPDATE ", strlen("UPDATE ")) )
        {
            pszSQLCommand += strlen("UPDATE ");
            oSet.insert(ExtractLayerName(&pszSQLCommand));
        }
        else if ( EQUALN(pszSQLCommand, "DROP TABLE ", strlen("DROP TABLE ")) )
        {
            pszSQLCommand += strlen("DROP TABLE ");
            oSet.insert(ExtractLayerName(&pszSQLCommand));
        }
        else
            pszSQLCommand ++;
    }

    return oSet;
}

/************************************************************************/
/*                          OGRSQLiteExecuteSQL()                       */
/************************************************************************/

OGRLayer * OGRSQLiteExecuteSQL( OGRDataSource* poDS,
                                const char *pszStatement,
                                OGRGeometry *poSpatialFilter,
                                const char *pszDialect )
{
    char* pszTmpDBName = (char*) CPLMalloc(256);
    sprintf(pszTmpDBName, "/vsimem/ogr2sqlite/temp_%p.db", pszTmpDBName);

    OGRSQLiteDataSource* poSQLiteDS = NULL;
    int nRet;
    int bSpatialiteDB = FALSE;
    
    OGR2SQLITE_Register();

/* -------------------------------------------------------------------- */
/*      Create in-memory sqlite/spatialite DB                           */
/* -------------------------------------------------------------------- */

#ifdef HAVE_SPATIALITE

/* -------------------------------------------------------------------- */
/*      Creating an empty spatialite DB (with spatial_ref_sys populated */
/*      has a non-neglectable cost. So at the first attempt, let's make */
/*      one and cache it for later use.                                 */
/* -------------------------------------------------------------------- */
#if 1
    static vsi_l_offset nEmptyDBSize = 0;
    static GByte* pabyEmptyDB = NULL;
    {
        static void* hMutex = NULL;
        CPLMutexHolder oMutexHolder(&hMutex);
        static int bTried = FALSE;
        if( !bTried )
        {
            bTried = TRUE;
            char* pszCachedFilename = (char*) CPLMalloc(256);
            sprintf(pszCachedFilename, "/vsimem/ogr2sqlite/reference_%p.db",pszCachedFilename);
            char** papszOptions = CSLAddString(NULL, "SPATIALITE=YES");
            OGRSQLiteDataSource* poCachedDS = new OGRSQLiteDataSource();
            nRet = poCachedDS->Create( pszCachedFilename, papszOptions );
            CSLDestroy(papszOptions);
            papszOptions = NULL;
            delete poCachedDS;
            if( nRet )
                /* Note: the reference file keeps the ownership of the data, so that */
                /* it gets released with VSICleanupFileManager() */
                pabyEmptyDB = VSIGetMemFileBuffer( pszCachedFilename, &nEmptyDBSize, FALSE );
            CPLFree( pszCachedFilename );
        }
    }

    /* The following configuration option is usefull mostly for debugging/testing */
    if( pabyEmptyDB != NULL && CSLTestBoolean(CPLGetConfigOption("OGR_SQLITE_DIALECT_USE_SPATIALITE", "YES")) )
    {
        GByte* pabyEmptyDBClone = (GByte*)VSIMalloc(nEmptyDBSize);
        if( pabyEmptyDBClone == NULL )
        {
            CPLFree(pszTmpDBName);
            return NULL;
        }
        memcpy(pabyEmptyDBClone, pabyEmptyDB, nEmptyDBSize);
        VSIFCloseL(VSIFileFromMemBuffer( pszTmpDBName, pabyEmptyDBClone, nEmptyDBSize, TRUE ));

        poSQLiteDS = new OGRSQLiteDataSource();
        if( !poSQLiteDS->Open( pszTmpDBName, TRUE ) )
        {
            /* should not happen really ! */
            delete poSQLiteDS;
            VSIUnlink(pszTmpDBName);
            CPLFree(pszTmpDBName);
            return NULL;
        }
        bSpatialiteDB = TRUE;
    }
#else
    /* No caching version */
    poSQLiteDS = new OGRSQLiteDataSource();
    char** papszOptions = CSLAddString(NULL, "SPATIALITE=YES");
    nRet = poSQLiteDS->Create( pszTmpDBName, papszOptions );
    CSLDestroy(papszOptions);
    papszOptions = NULL;
    if( nRet )
    {
        bSpatialiteDB = TRUE;
    }
#endif

    else
    {
        delete poSQLiteDS;
        poSQLiteDS = NULL;
#else
    if( TRUE )
    {
#endif
        poSQLiteDS = new OGRSQLiteDataSource();
        nRet = poSQLiteDS->Create( pszTmpDBName, NULL );
        if( !nRet )
        {
            delete poSQLiteDS;
            VSIUnlink(pszTmpDBName);
            CPLFree(pszTmpDBName);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Attach the Virtual Table OGR2SQLITE module to it.               */
/* -------------------------------------------------------------------- */
    OGR2SQLITEModule* poModule = new OGR2SQLITEModule(poDS);
    poModule->SetToDB(poSQLiteDS->GetDB());

/* -------------------------------------------------------------------- */
/*      Analysze the statement to determine which tables will be used.  */
/* -------------------------------------------------------------------- */
    std::set<CPLString> oSetNames =
                        OGR2SQLITEGetPotentialLayerNames(pszStatement);
    std::set<CPLString>::iterator oIter = oSetNames.begin();
    
    CPLString osStatement(pszStatement);
    int bFoundOGRStyle = ( osStatement.ifind("OGR_STYLE") != std::string::npos );

/* -------------------------------------------------------------------- */
/*      For each of those tables, create a Virtual Table.               */
/* -------------------------------------------------------------------- */
    for(; oIter != oSetNames.end(); ++oIter)
    {
        OGRLayer* poLayer = poDS->GetLayerByName(*oIter);
        if( poLayer == NULL )
            continue;

        CPLString osSQL;

        osSQL.Printf("CREATE VIRTUAL TABLE \"%s\" USING VirtualOGR('%s',%d)",
                     OGRSQLiteEscapeName(poLayer->GetName()).c_str(),
                     OGRSQLiteEscape(poLayer->GetName()).c_str(),
                     bFoundOGRStyle);

        char* pszErrMsg = NULL;
        int rc = sqlite3_exec( poSQLiteDS->GetDB(), osSQL.c_str(),
                               NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create virtual table for layer' %s' : %s",
                     poLayer->GetName(), pszErrMsg);
            sqlite3_free(pszErrMsg);
            continue;
        }

        if( poLayer->GetGeomType() == wkbNone )
            continue;

        CPLString osGeomColRaw(OGR2SQLITE_GetNameForGeometryColumn(poLayer));
        const char* pszGeomColRaw = osGeomColRaw.c_str();

        CPLString osGeomColEscaped(OGRSQLiteEscape(pszGeomColRaw));
        const char* pszGeomColEscaped = osGeomColEscaped.c_str();

        CPLString osLayerNameEscaped(OGRSQLiteEscape(poLayer->GetName()));
        const char* pszLayerNameEscaped = osLayerNameEscaped.c_str();

        int nSRSId = -1;
        OGRSpatialReference* poSRS = poLayer->GetSpatialRef();
        if( poSRS != NULL )
        {
            const char* pszAuthorityName = poSRS->GetAuthorityName(NULL);
            if (pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG"))
            {
                const char* pszAuthorityCode = poSRS->GetAuthorityCode(NULL);
                if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
                {
                    nSRSId = atoi(pszAuthorityCode);
                }
            }
        }

        int bCreateSpatialIndex = FALSE;
        if( !bSpatialiteDB )
        {
            /* Make sure that the SRS is injected in spatial_ref_sys */
            if( poSRS != NULL )
                poSQLiteDS->FetchSRSId(poSRS);

            osSQL.Printf("INSERT INTO geometry_columns (f_table_name, "
                        "f_geometry_column, geometry_format, geometry_type, "
                        "coord_dimension, srid) "
                        "VALUES ('%s','%s','SpatiaLite',%d,%d,%d)",
                        pszLayerNameEscaped,
                        pszGeomColEscaped,
                         (int) wkbFlatten(poLayer->GetGeomType()),
                        ( poLayer->GetGeomType() & wkb25DBit ) ? 3 : 2,
                        nSRSId);
        }
        else
        {
            bCreateSpatialIndex = poSpatialFilter != NULL;
            
            if( poSQLiteDS->HasSpatialite4Layout() )
            {
                int nGeomType = poLayer->GetGeomType();
                int nCoordDimension = 2;
                if( nGeomType & wkb25DBit )
                {
                    nGeomType += 1000;
                    nCoordDimension = 3;
                }

                osSQL.Printf("INSERT INTO geometry_columns (f_table_name, "
                            "f_geometry_column, geometry_type, coord_dimension, "
                            "srid, spatial_index_enabled) "
                            "VALUES ('%s','%s',%d ,%d ,%d, %d)",
                            pszLayerNameEscaped,
                            pszGeomColEscaped, nGeomType,
                            nCoordDimension,
                            nSRSId, bCreateSpatialIndex );
            }
            else
            {
                const char *pszGeometryType = OGRToOGCGeomType(poLayer->GetGeomType());
                if (pszGeometryType[0] == '\0')
                    pszGeometryType = "GEOMETRY";

                osSQL.Printf("INSERT INTO geometry_columns (f_table_name, "
                            "f_geometry_column, type, coord_dimension, "
                            "srid, spatial_index_enabled) "
                            "VALUES ('%s','%s','%s','%s',%d, %d)",
                            pszLayerNameEscaped,
                            pszGeomColEscaped, pszGeometryType,
                            ( poLayer->GetGeomType() & wkb25DBit ) ? "XYZ" : "XY",
                            nSRSId, bCreateSpatialIndex );
            }
        }

        sqlite3_exec( poSQLiteDS->GetDB(), osSQL.c_str(), NULL, NULL, NULL );

/* -------------------------------------------------------------------- */
/*      Should we create a spatial index ?.                             */
/* -------------------------------------------------------------------- */
        if( !bSpatialiteDB || !bCreateSpatialIndex )
            continue;

        CPLString osIdxName(OGRSQLiteEscapeName(
                CPLSPrintf("idx_%s_%s",poLayer->GetName(), pszGeomColRaw)));

        osSQL.Printf("CREATE VIRTUAL TABLE \"%s\" USING rtree(pkid, xmin, xmax, ymin, ymax)",
                     osIdxName.c_str());

        sqlite3_exec( poSQLiteDS->GetDB(), osSQL.c_str(), NULL, NULL, NULL );

        CPLString osGeomColNameEscaped(OGRSQLiteEscape(pszGeomColRaw));
        const char* pszGeomColNameEscaped = osGeomColNameEscaped.c_str();

        osSQL.Printf("INSERT INTO \"%s\" (pkid, xmin, xmax, ymin, ymax) "
                     "SELECT ROWID, MbrMinX(\"%s\"), MbrMaxX(\"%s\"), MbrMinY(\"%s\"), "
                     "MbrMaxY(\"%s\") FROM \"%s\"",
                     osIdxName.c_str(), pszGeomColNameEscaped, pszGeomColNameEscaped,
                     pszGeomColNameEscaped, pszGeomColNameEscaped, pszLayerNameEscaped);

        sqlite3_exec( poSQLiteDS->GetDB(), osSQL.c_str(), NULL, NULL, NULL );
    }

    delete poSQLiteDS;

/* -------------------------------------------------------------------- */
/*      Re-open so that virtual tables are recognized                   */
/* -------------------------------------------------------------------- */
    poSQLiteDS = new OGRSQLiteDataSource();
    if( !poSQLiteDS->Open(pszTmpDBName, TRUE) )
    {
        delete poSQLiteDS;
        VSIUnlink(pszTmpDBName);
        CPLFree(pszTmpDBName);
        return NULL;
    }
    sqlite3* hDB = poSQLiteDS->GetDB();
    poModule->SetToDB(hDB);

/* -------------------------------------------------------------------- */
/*      Prepare the statement.                                          */
/* -------------------------------------------------------------------- */
    /* This will speed-up layer creation */
    /* ORDER BY are costly to evaluate and are not necessary to establish */
    /* the layer definition. */
    int bUseStatementForGetNextFeature = TRUE;
    int bEmptyLayer = FALSE;

    sqlite3_stmt *hSQLStmt = NULL;
    int rc = sqlite3_prepare( hDB,
                              pszStatement, strlen(pszStatement),
                              &hSQLStmt, NULL );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "In ExecuteSQL(): sqlite3_prepare(%s):\n  %s",
                pszStatement, sqlite3_errmsg(hDB) );

        if( hSQLStmt != NULL )
        {
            sqlite3_finalize( hSQLStmt );
        }

        delete poSQLiteDS;
        VSIUnlink(pszTmpDBName);
        CPLFree(pszTmpDBName);

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we get a resultset?                                          */
/* -------------------------------------------------------------------- */
    rc = sqlite3_step( hSQLStmt );
    if( rc != SQLITE_ROW )
    {
        if ( rc != SQLITE_DONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                  "In ExecuteSQL(): sqlite3_step(%s):\n  %s",
                  pszStatement, sqlite3_errmsg(hDB) );

            sqlite3_finalize( hSQLStmt );

            delete poSQLiteDS;
            VSIUnlink(pszTmpDBName);
            CPLFree(pszTmpDBName);

            return NULL;
        }

        if( !EQUALN(pszStatement, "SELECT ", 7) )
        {

            sqlite3_finalize( hSQLStmt );

            delete poSQLiteDS;
            VSIUnlink(pszTmpDBName);
            CPLFree(pszTmpDBName);

            return NULL;
        }

        bUseStatementForGetNextFeature = FALSE;
        bEmptyLayer = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Create layer.                                                   */
/* -------------------------------------------------------------------- */
    OGRSQLiteSelectLayer *poLayer = NULL;

    poLayer = new OGRSQLiteExecuteSQLLayer( poModule, pszTmpDBName,
                                            poSQLiteDS, pszStatement, hSQLStmt,
                                            bUseStatementForGetNextFeature, bEmptyLayer );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );

    return poLayer;
}

#else // HAVE_SQLITE_VFS

/************************************************************************/
/*                          OGRSQLiteExecuteSQL()                       */
/************************************************************************/

OGRLayer * OGRSQLiteExecuteSQL( OGRDataSource* poDS,
                                const char *pszStatement,
                                OGRGeometry *poSpatialFilter,
                                const char *pszDialect )
{
    CPLError(CE_Failure, CPLE_NotSupported,
                "The SQLite version is to old to support the SQLite SQL dialect");
    return NULL;
}

#endif // HAVE_SQLITE_VFS
